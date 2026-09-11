#include "manalink.h"

// utility functions

static int count_allies(int player, int card){
	return count_subtype(player, TYPE_CREATURE, SUBTYPE_ALLY);
}

void landfall_mode(int player, int card, event_t event, resolve_trigger_t trigger_mode){
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);

		etb_ability_global(player, event, trigger_mode, 0, &test, player, card);
	}
}

void landfall(int player, int card, event_t event){
	landfall_mode(player, card, event, RESOLVE_TRIGGER_MANDATORY);
}

void ally_trigger(int player, int card, event_t event, int trigger_mode){
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_ALLY;

		etb_ability_global(player, event, trigger_mode, 0, &test, player, card);
	}
}

static int quest(int player, int card, event_t event, int req_counters, target_definition_t *td, const char *prompt){
	if( event == EVENT_CAN_ACTIVATE && count_counters(player, card, COUNTER_QUEST) >= req_counters ){
		int flags = GAA_SACRIFICE_ME;
		if( td != NULL ){
			flags |= GAA_CAN_TARGET;
		}
		return generic_activated_ability(player, card, event, flags, MANACOST0, 0, td, prompt);
	}
	if( event == EVENT_ACTIVATE ){
		int flags = GAA_SACRIFICE_ME;
		if( td != NULL ){
			flags |= GAA_CAN_TARGET;
		}
		return generic_activated_ability(player, card, event, flags, MANACOST0, 0, td, prompt);
	}
   return global_enchantment(player, card, event);
}

static int expedition(int player, int card, event_t event){
	landfall_mode(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		add_counter(player, card, COUNTER_QUEST);
	}
	return quest(player, card, event, 3, NULL, 0);
}

// artifacts
int card_adventuring_gear(int player, int card, event_t event){
/* CARD_ID_ADVENTURING_GEAR	10492
Adventuring Gear	|1
Artifact - Equipment
Landfall - Whenever a land enters the battlefield under your control, equipped creature gets +2/+2 until end of turn.
Equip |1 (|1: Attach to target creature you control. Equip only as a sorcery.) */

	if( is_equipping(player, card) ){
		landfall(player, card, event);
		if( event == EVENT_ETB_ABILITY ){
			card_instance_t *instance = get_card_instance(player, card);
			pump_until_eot_merge_previous(player, card, instance->damage_target_player, instance->damage_target_card, 2, 2);
		}
	}
	return basic_equipment(player, card, event, 1);
}

int card_blade_of_the_bloodchief(int player, int card, event_t event){
/* CARD_ID_BLADE_OF_THE_BLOODCHIEF	10505
Blade of the Bloodchief	|1
Artifact - Equipment
Whenever a creature dies, put a +1/+1 counter on equipped creature. If equipped creature is a Vampire, put two +1/+1 counters on it instead.
Equip |1 */

	if( is_equipping(player, card) ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( result ){
		if( is_equipping(player, card) ){
			card_instance_t *instance = get_card_instance(player, card);
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			int amount = BYTE0(result);
			add_1_1_counters(p, c, amount);
			if( has_subtype(p, c, SUBTYPE_VAMPIRE) ){
			   add_1_1_counters(p, c, amount);
			}
		}
	}

	return basic_equipment(player, card, event, 1);
}

int card_blazing_torch(int player, int card, event_t event){
	/* Blazing Torch	|1	0x200192a
	 * Artifact - Equipment
	 * Equipped creature can't be blocked by Vampires or Zombies.
	 * Equipped creature has "|T, Sacrifice ~: ~ deals 2 damage to any target."
	 * Equip |1 */
	if( event == EVENT_BLOCK_LEGALITY && is_equipping(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( attacking_card_controller == p && attacking_card == c ){
			if( has_creature_type(affected_card_controller, affected_card, SUBTYPE_VAMPIRE) ||
				has_creature_type(affected_card_controller, affected_card, SUBTYPE_ZOMBIE) )
			{
				event_result = 1;
			}
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_EQUIP = 1,
		CHOICE_SAC_DAMAGE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( is_equipping(player, card) ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			td.illegal_abilities = get_protections_from(p, c);
			if( granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED | GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0,
													&td, NULL) )
			{
				return 1;
			}
		}
		return basic_equipment(player, card, event, 1);
	}

    if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, can_activate_basic_equipment(player, card, event, 1), 0};
		if( is_equipping(player, card) ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			td.illegal_abilities = get_protections_from(p, c);
			if( granted_generic_activated_ability(player, card, p, c, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0,
													&td, NULL) )
			{
				abils[2] = 1;
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Equip", abils[1], 5,
							"Sac & damage", abils[2], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 1);
		}
		if( choice == CHOICE_SAC_DAMAGE ){
			if( is_equipping(player, card) ){
				int p = instance->damage_target_player;
				int c = instance->damage_target_card;
				td.illegal_abilities = get_protections_from(p, c);
				instance->targets[1].player = get_protections_from(p, c);
				return granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED | GAA_SACRIFICE_ME | GAA_CAN_TARGET,
															MANACOST0, 0, &td, "TARGET_ANY");
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 1);
		}
		if( instance->info_slot == CHOICE_SAC_DAMAGE ){
			td.illegal_abilities = instance->targets[1].player;
			if( valid_target(&td) ){
				damage_target0(player, card, 2);
			}
		}
	}

	return 0;
}

int card_carnage_altar(int player, int card, event_t event){
/* CARD_ID_CARNAGE_ALTAR	10518
Carnage Altar	|2
Artifact
|3, Sacrifice a creature: Draw a card. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_a_card(player);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST_X(3), 0, NULL, NULL);
}

int card_eldrazi_monument(int player, int card, event_t event){
/* CARD_ID_ELDRAZI_MONUMENT	10531
Eldrazi Monument	|5
Artifact
Creatures you control get +1/+1 and have flying and indestructible.
At the beginning of your upkeep, sacrifice a creature. If you can't, sacrifice ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int kill_me = ! new_sacrifice(player, card, player, SAC_AS_COST | SAC_NO_CANCEL, &test);
		if( kill_me ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( in_play(player, card) && ! is_humiliated(player, card) && affected_card_controller == player &&
		is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
	{
		switch( event ){
				case EVENT_ABILITIES:
				{
					indestructible(affected_card_controller, affected_card, event);
					event_result |= KEYWORD_FLYING;
				}
				break;

				case EVENT_POWER:
				case EVENT_TOUGHNESS:
					event_result++;
					break;

				default:
					break;
		}
	}

	return 0;
}


int card_eternity_vessel(int player, int card, event_t event)
{
/* CARD_ID_ETERNITY_VESSEL	10536
Eternity Vessel	|6
Artifact
~ enters the battlefield with X charge counters on it, where X is your life total.
Landfall - Whenever a land enters the battlefield under your control, you may have your life total become the number of charge counters on ~. */

  enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, life[player]);

  // Landfall - Whenever a land enters the battlefield under your control, you may have your life total become the number of charge counters on ~.
  landfall_mode(player, card, event,
					!duh_mode(player) ? RESOLVE_TRIGGER_OPTIONAL
					: life[player] < count_counters(player, card, COUNTER_CHARGE) ? RESOLVE_TRIGGER_MANDATORY : 0);
	if( event == EVENT_ETB_ABILITY )
		set_life_total(player, count_counters(player, card, COUNTER_CHARGE));

  return 0;
}

int card_expedition_map(int player, int card, event_t event){
/* CARD_ID_EXPEDITION_MAP	10537
Expedition Map	|1
Artifact
|2, |T, Sacrifice ~: Search your library for a land card, reveal it, and put it into your hand. Then shuffle your library. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a land card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL);
}

int card_explorers_scope(int player, int card, event_t event){
/* CARD_ID_EXPLORERS_SCOPE	10538
Explorer's Scope	|1
Artifact - Equipment
Whenever equipped creature attacks, look at the top card of your library. If it's a land card, you may put it onto the battlefield tapped.
Equip |1 (|1: Attach to target creature you control. Equip only as a sorcery.) */

	if( is_equipping(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_ATTACK_RATING && affect_me(p, c) ){
			ai_defensive_modifier -= 25;
		}
		// Whenever equipped creature attacks, look at the top card of your library. If it's a land card, you may put it onto the
		//		battlefield tapped.
		if (declare_attackers_trigger(player, card, event, 0, p, c))
		{
			int* deck = deck_ptr[player];
			if (deck[0] == -1)
				return 0;

			int play = 0;
			if (player == AI || ai_is_speculating == 1)
				play = is_what(-1, deck[0], TYPE_LAND);
			else
			{
				if (is_what(-1, deck[0], TYPE_LAND))
					play = reveal_card_optional_iid(player, card, deck[0], "Put on the battlefield tapped");
				else
					reveal_card_iid(player, card, deck[0]);
			}

			if (play)
			{
				int iid = deck[0];
				remove_card_from_deck(player, 0);
				int card_added = add_card_to_hand(player, iid);
				get_card_instance(player, card_added)->state |= STATE_TAPPED;
				put_into_play(player, card_added);
			}
		}
	}

	return basic_equipment(player, card, event, 1);
}

int card_hedron_scrabbler(int player, int card, event_t event){
/* CARD_ID_HEDRON_SCRABBLER	10564
Hedron Scrabbler	|2
Artifact Creature - Construct 1/1
Landfall - Whenever a land enters the battlefield under your control, ~ gets +1/+1 until end of turn. */

	landfall(player, card, event);
	if( event == EVENT_ETB_ABILITY ){
		pump_until_eot_merge_previous(player, card, player, card, 1, 1);
	}
	return 0;
}

int card_khalni_gem(int player, int card, event_t event){
/* CARD_ID_KHALNI_GEM	10582
Khalni Gem	|4
Artifact
When ~ enters the battlefield, return two lands you control to their owner's hand.
|T: Add two mana of any one color to your mana pool. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.allowed_controller = td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;

		int i;
		for(i=0; i<2; i++){
			if( can_target(&td) && new_pick_target(&td, "Select a land you control.", 0, GS_LITERAL_PROMPT) ){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
			}
		}

		for(i=0; i<instance->number_of_targets; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			bounce_permanent(instance->targets[i].player, instance->targets[i].card);
		}
		instance->number_of_targets = 0;
	}

	return artifact_mana_all_one_color(player, card, event, 2, 0);
}

int card_spidersilk_net(int player, int card, event_t event){
/* CARD_ID_SPIDERSILK_NET	10673
Spidersilk Net	|0
Artifact - Equipment
Equipped creature gets +0/+2 and has reach. (It can block creatures with flying.)
Equip |2 (|2: Attach to target creature you control. Equip only as a sorcery.) */

	return vanilla_equipment(player, card, event, 2, 0, 2, KEYWORD_REACH, 0);
}

int card_trailblazers_boots(int player, int card, event_t event){
/* CARD_ID_TRAILBLAZERS_BOOTS	10690
Trailblazer's Boots	|2
Artifact - Equipment
Equipped creature has nonbasic landwalk. (It can't be blocked as long as defending player controls a nonbasic land.)
Equip |2 */

	if( is_equipping(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_BLOCK_LEGALITY && attacking_card_controller == p && attacking_card == c ){
			test_definition_t test;
			default_test_definition(&test, TYPE_LAND);
			test.subtype = SUBTYPE_BASIC;
			test.subtype_flag = DOESNT_MATCH;
			if( check_battlefield_for_special_card(player, card, 1-p, CBFSC_CHECK_ONLY, &test) ){
				event_result = 1;
			}
		}
	}

	return basic_equipment(player, card, event, 2);
}

int card_trusty_machete(int player, int card, event_t event){
/* CARD_ID_TRUSTY_MACHETE	10693
Trusty Machete	|1
Artifact - Equipment
Equipped creature gets +2/+1.
Equip |2 */

	return vanilla_equipment(player, card, event, 2, 2, 1, 0, 0);
}

// lands
int card_akoum_refuge(int player, int card, event_t event){
	/* Akoum Refuge	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, you gain 1 life.
	 * |T: Add |B or |R to your mana pool. */

	comes_into_play_tapped(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 1);
	}

	return mana_producer(player, card, event);
}

int card_arid_mesa(int player, int card, event_t event){
	/* Arid Mesa	""
	 * Land
	 * |T, Pay 1 life, Sacrifice ~: Search your library for |Ha Mountain or |H2Plains card and put it onto the battlefield. Then shuffle your library. */
	return fetchland(player, card, event, SUBTYPE_MOUNTAIN, SUBTYPE_PLAINS, 1);
}

int card_marsh_flats(int player, int card, event_t event){
	/* Marsh Flats	""
	 * Land
	 * |T, Pay 1 life, Sacrifice ~: Search your library for |Ha Plains or |H2Swamp card and put it onto the battlefield. Then shuffle your library. */
	return fetchland(player, card, event, SUBTYPE_PLAINS, SUBTYPE_SWAMP, 1);
}

int card_misty_rainforest(int player, int card, event_t event){
	/* Misty Rainforest	""
	 * Land
	 * |T, Pay 1 life, Sacrifice ~: Search your library for |Ha Forest or |H2Island card and put it onto the battlefield. Then shuffle your library. */
	return fetchland(player, card, event, SUBTYPE_FOREST, SUBTYPE_ISLAND, 1);
}

int card_scalding_tarn(int player, int card, event_t event){
	/* Scalding Tarn	""
	 * Land
	 * |T, Pay 1 life, Sacrifice ~: Search your library for |Han Island or |H2Mountain card and put it onto the battlefield. Then shuffle your library. */
	return fetchland(player, card, event, SUBTYPE_ISLAND, SUBTYPE_MOUNTAIN, 1);
}

int card_verdant_catacombs(int player, int card, event_t event){
	/* Verdant Catacombs	""
	 * Land
	 * |T, Pay 1 life, Sacrifice ~: Search your library for |Ha Swamp or |H2Forest card and put it onto the battlefield. Then shuffle your library. */
	return fetchland(player, card, event, SUBTYPE_SWAMP, SUBTYPE_FOREST, 1);
}

int card_crypt_of_agadeem(int player, int card, event_t event){
/* CARD_ID_CRYPT_OF_AGADEEM	10526
Crypt of Agadeem	""
Land
~ enters the battlefield tapped.
|T: Add |B to your mana pool.
|2, |T: Add |B to your mana pool for each |Sblack creature card in your graveyard. */

	comes_into_play_tapped(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		if( has_mana(player, COLOR_ANY, 3) && ! paying_mana() ) {
			int ai_choice = 0;
			if( count_graveyard_by_type(player, TYPE_CREATURE) > 3 ){
				ai_choice = 1;
			}
			choice = do_dialog(player, player, card, -1, -1, " Generate B\n Add B for each dead guy\n Cancel", ai_choice);
		}
		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		if( choice == 1 ){
			add_state(player, card, STATE_TAPPED);
			charge_mana(player, COLOR_COLORLESS, 2);
			if( spell_fizzled != 1  ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
				int count = new_special_count_grave(player, &this_test);
				produce_mana_tapped(player, card, COLOR_BLACK, count);
			}
			else{
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	return 0;
}

int card_emeria_the_sky_ruin(int player, int card, event_t event ){
/* CARD_ID_EMERIA_THE_SKY_RUIN	10535
Emeria, the Sky Ruin	""
Land
~ enters the battlefield tapped.
At the beginning of your upkeep, if you control seven or more |H1Plains, you may return target creature card from your graveyard to the battlefield.
|T: Add |W to your mana pool. */

	comes_into_play_tapped(player, card, event);

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		if( count_subtype(player, TYPE_LAND, SUBTYPE_PLAINS) >= 7 ){
			upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));
		}
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) && ! graveyard_has_shroud(player) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}

	return mana_producer(player, card, event);
}

int card_kabira_crossroads(int player, int card, event_t event){
	/* Kabira Crossroads	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, you gain 2 life.
	 * |T: Add |W to your mana pool. */

	comes_into_play_tapped(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 2);
	}

	return mana_producer(player, card, event);
}

int card_magosi_the_waterveil(int player, int card, event_t event){
/* Magosi, the Waterveil	""
 * Land
 * ~ enters the battlefield tapped.
 * |T: Add |U to your mana pool.
 * |U, |T: Put an eon counter on ~. Skip your next turn.
 * |T, Remove an eon counter from ~ and return it to its owner's hand: Take an extra turn after this one. */

	comes_into_play_tapped(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance= get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_SKIP_TURN,
		CHOICE_TIME_WALK
	};

	if( event == EVENT_ACTIVATE ){
		int abils[4] = {
						0,
						1,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_U(2), 0, NULL, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, GVC_COUNTER(COUNTER_EON), NULL, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Get mana", abils[1], 1,
							"Skip turn & get Eon counter", abils[2] && ! paying_mana() ? 1 : 0, 5,
							"Gain an extra turn", abils[3] && ! paying_mana() ? 1 : 0, 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_SKIP_TURN ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, 0, MANACOST_U(1), 0, NULL, NULL);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
		if( choice == CHOICE_TIME_WALK ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTER(COUNTER_EON), NULL, NULL);
			bounce_permanent(player, card);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SKIP_TURN ){
			add_counter(instance->parent_controller, instance->parent_card, COUNTER_EON);
		    alternate_legacy_text(1, player, skip_next_turn(player, card, player));
		}
		if( instance->info_slot == CHOICE_TIME_WALK ){
			alternate_legacy_text(2, player, time_walk_effect(instance->parent_controller, instance->parent_card));
		}
	}

	return 0;
}

void activate_oran_rief_pump(int player, int card, int color){
	int p, c;
	for(p=0; p<2; p++){
		for(c=active_cards_count[p]-1; c>-1; c--){
			if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) && check_special_flags(p, c, SF_JUST_CAME_INTO_PLAY) ){
				if(get_sleighted_color_test(player, card, color) & get_color(p, c)){
					add_1_1_counter(p, c);
				}
			}
		}
	}
}

int card_oran_rief_the_vastwood(int player, int card, event_t event){
/* CARD_ID_ORAN_RIEF_THE_VASTWOOD	10629
Oran-Rief, the Vastwood	""
Land
~ enters the battlefield tapped.
|T: Add |G to your mana pool.
|T: Put a +1/+1 counter on each |Sgreen creature that entered the battlefield this turn. */

	if( event == EVENT_CLEANUP ){
		remove_special_flags(2, -1, SF_JUST_CAME_INTO_PLAY);
	}

	comes_into_play_tapped(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance= get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_P1_P1_COUNTER
	};

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						1,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Get mana", abils[1], 1,
							"Pump creatures", abils[2] && ! paying_mana() ? 1 : 0, 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_P1_P1_COUNTER ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 2 ){
			activate_oran_rief_pump(player, instance->parent_card,
									get_sleighted_color_test(player, card, COLOR_TEST_GREEN));
		}
	}

	return 0;
}

int card_piranha_marsh(int player, int card, event_t event){
	/* Piranha Marsh	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, target player loses 1 life.
	 * |T: Add |B to your mana pool. */

	comes_into_play_tapped(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.allow_cancel = 0;
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER" ) ){
			lose_life(instance->targets[0].player, 1);
		}
		instance->number_of_targets = 0;
	}

	return mana_producer(player, card, event);
}

int card_soaring_seacliff(int player, int card, event_t event){
	/* Soaring Seacliff	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, target creature gains flying until end of turn.
	 * |T: Add |U to your mana pool. */

	comes_into_play_tapped(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t*instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE" ) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FLYING, 0);
		}
		instance->number_of_targets = 0;
	}

	return mana_producer(player, card, event);
}

int card_teetering_peaks(int player, int card, event_t event){
	/* Teetering Peaks	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, target creature gets +2/+0 until end of turn.
	 * |T: Add |R to your mana pool. */

	comes_into_play_tapped(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE" ) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 0);
		}
		instance->number_of_targets = 0;
	}

	return mana_producer(player, card, event);
}

int card_turntimber_grove(int player, int card, event_t event){
	/* Turntimber Grove	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, target creature gets +1/+1 until end of turn.
	 * |T: Add |G to your mana pool. */

	comes_into_play_tapped(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE" ) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
		instance->number_of_targets = 0;
	}

	return mana_producer(player, card, event);
}

int card_valakut_the_molten_pinnacle(int player, int card, event_t event)
{
	/* Valakut, the Molten Pinnacle	""	0x20019bb
	 * Land
	 * ~ enters the battlefield tapped.
	 * Whenever |Ha Mountain enters the battlefield under your control, if you control at least five other |H1Mountains, you may have ~ deal 3 damage to any target.
	 * |T: Add |R. */
	comes_into_play_tapped(player, card, event);

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		if( count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN)) >= 6 ){
			test_definition_t test;
			default_test_definition(&test, TYPE_LAND);
			test.subtype = get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN);

			etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
		}
	}

	if( event == EVENT_ETB_ABILITY ){
		if( count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN)) >= 6 ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

			card_instance_t* instance = get_card_instance(player, card);

			if( can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0) ){
				damage_target0(player, card, 3);
			}
			instance->number_of_targets = 0;
		}
	}

	return mana_producer(player, card, event);
}

// green cards
int card_baloth_cage_trap(int player, int card, event_t event){
/* CARD_ID_BALOTH_CAGE_TRAP	10501
Baloth Cage Trap	|3|G|G
Instant - Trap
If an opponent had an artifact enter the battlefield under his or her control this turn, you may pay |1|G rather than pay ~'s mana cost.
Create a 4/4 |Sgreen Beast creature token. */

	if( event == EVENT_MODIFY_COST ){
		if( get_trap_condition(1-player, TRAP_ARTIFACTS_PLAYED) == 1 ){
			COST_COLORLESS-=2;
			COST_GREEN--;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_BEAST, &token);
		token.pow = token.tou = 4;
		generate_token(&token);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_baloth_woodcrasher(int player, int card, event_t event){
/* CARD_ID_BALOTH_WOODCRASHER	10502
Baloth Woodcrasher	|4|G|G
Creature - Beast 4/4
Landfall - Whenever a land enters the battlefield under your control, ~ gets +4/+4 and gains trample until end of turn. */

	landfall(player, card, event);
	if( event == EVENT_ETB_ABILITY ){
		pump_ability_until_eot(player, card, player, card, 4, 4, KEYWORD_TRAMPLE, 0 );
	}
	return 0;
}

int card_beast_hunt(int player, int card, event_t event){
/* CARD_ID_BEAST_HUNT	10503
Beast Hunt	|3|G
Sorcery
Reveal the top three cards of your library. Put all creature cards revealed this way into your hand and the rest into your graveyard. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 24;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = MIN(3, count_deck(player));
		while( amount ){
				int iid = deck_ptr[player][0];
				if( is_what(-1, iid, TYPE_CREATURE) ){
					add_card_to_hand(player, iid);
					remove_card_from_deck(player, 0);
				}
				else{
					mill(player, 1);
				}
				amount--;
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_beastmaster_ascension(int player, int card, event_t event)
{
/* CARD_ID_BEASTMASTER_ASCENSION	10504
Beastmaster Ascension	|2|G
Enchantment
Whenever a creature you control attacks, you may put a quest counter on ~.
As long as ~ has seven or more quest counters on it, creatures you control get +5/+5. */

	if (declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_DUH | DAT_SEPARATE_TRIGGERS, player, -1))
		add_counter(player, card, COUNTER_QUEST);

	// As long as ~ has seven or more quest counters on it, creatures you control get +5/+5.
	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == player &&
		count_counters(player, card, COUNTER_QUEST) >= 7 && ! is_humiliated(player, card) )
		event_result += 5;

	return global_enchantment(player, card, event);
}

int card_cobra_trap(int player, int card, event_t event){
	/* Cobra Trap	|4|G|G
	 * Instant - Trap
	 * If a noncreature permanent under your control was destroyed this turn by a spell or ability an opponent controlled, you may pay |G rather than pay ~'s mana cost.
	 * Put four 1/1 |Sgreen Snake creature tokens onto the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_SNAKE, 4 );
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

int card_frontier_guide(int player, int card, event_t event){
/* CARD_ID_FRONTIER_GUIDE	10541
Frontier Guide	|1|G
Creature - Elf Scout 1/1
|3|G, |T: Search your library for a basic land card and put it onto the battlefield tapped. Then shuffle your library. */

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (current_turn == 1-player ? 24 : 12);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		tutor_basic_land(player, 1, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XG(3, 1), 0, NULL, NULL);
}

int card_gigantiform(int player, int card, event_t event){
/* CARD_ID_GIGANTIFORM	10545
Gigantiform	|3|G|G
Enchantment - Aura
Kicker |4
Enchant creature
Enchanted creature has base power and toughness 8/8 and has trample.
When ~ enters the battlefield, if it was kicked, you may search your library for a card named ~, put it onto the battlefield, then shuffle your library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags(player, card, SF_NOT_CAST) && ! is_token(player, card) ){
			do_kicker(player, card, MANACOST_X(4));
		}
	}

	if( comes_into_play(player, card, event) && kicked(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		this_test.id = get_card_name(player, card);
		scnprintf(this_test.message, 100, "Select a card named %s.", cards_ptr[get_card_name(player, card)]->name);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);
	}

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( event == EVENT_POWER && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			event_result += 8 - get_base_power(instance->damage_target_player, instance->damage_target_card);
		}
		if( event == EVENT_TOUGHNESS && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			event_result += 8 - get_base_toughness(instance->damage_target_player, instance->damage_target_card);
		}
		if( event == EVENT_ABILITIES && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			event_result |= KEYWORD_TRAMPLE;
		}
	}

	return vanilla_aura(player, card, event, player);
}

int card_grazing_gladehart(int player, int card, event_t event){
/* CARD_ID_GRAZING_GLADEHART	10554
Grazing Gladehart	|2|G
Creature - Antelope 2/2
Landfall - Whenever a land enters the battlefield under your control, you may gain 2 life. */

	landfall_mode(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 2);
	}
	return 0;
}

int card_greenweaver_druid(int player, int card, event_t event){
/* CARD_ID_GREENWEAVER_DRUID	10555
Greenweaver Druid	|2|G
Creature - Elf Druid 1/1
|T: Add |G|G to your mana pool. */

	//0x40b410 (Fyndhorn Elder)
	return mana_producing_creature(player, card, event, 36, COLOR_GREEN, 2);
}

int card_harrow(int player, int card, event_t event){
/* CARD_ID_HARROW	3164
Harrow	|2|G
Instant
As an additional cost to cast ~, sacrifice a land.
Search your library for up to two basic land cards and put them onto the battlefield. Then shuffle your library. */

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return can_sacrifice_type_as_cost(player, 1, TYPE_LAND);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a land to sacrifice.");
		if( ! new_sacrifice(player, card, player, 0, &this_test) ){
			spell_fizzled = 1;
			return 0;
		}
		ai_modifier += 48;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a basic land card.");
		this_test.subtype = SUBTYPE_BASIC;
		this_test.qty = 2;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);
		kill_card(player, card, KILL_DESTROY );
	}
	return 0;
}

int card_joraga_bard(int player, int card, event_t event){
	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		pump_subtype_until_eot(player, card, player, SUBTYPE_ALLY, 0, 0, 0, SP_KEYWORD_VIGILANCE );
	}
	return 0;
}

int card_khalni_heart_expedition(int player, int card, event_t event){
	/* Khalni Heart Expedition	|1|G
	 * Enchantment
	 * Landfall - Whenever a land enters the battlefield under your control, you may put a quest counter on ~.
	 * Remove three quest counters from ~ and sacrifice it: Search your library for up to two basic land cards, put them onto the battlefield tapped, then
	 * shuffle your library. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 2);
	}
	return expedition(player, card, event);
}

int card_lotus_cobra(int player, int card, event_t event){
/* CARD_ID_LOTUS_COBRA	10597
Lotus Cobra	|1|G
Creature - Snake 2/1
Landfall - Whenever a land enters the battlefield under your control, you may add one mana of any color to your mana pool. */

	landfall_mode(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		int color = choose_a_color(player, get_deck_color(player, player));
		produce_mana(player, color, 1);
	}
	return 0;
}

int card_mold_shambler(int player, int card, event_t event){
/* CARD_ID_MOLD_SHAMBLER	10613
Mold Shambler	|3|G
Creature - Fungus Beast 3/3
Kicker |1|G (You may pay an additional |1|G as you cast this spell.)
When ~ enters the battlefield, if it was kicked, destroy target noncreature permanent. */

	kicker(player, card, event, MANACOST_XG(1, 1));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_CREATURE;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "Select target non-creature permanent.", 0, GS_LITERAL_PROMPT) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_nissa_revane(int player, int card, event_t event){

	/* Nissa Revane	|2|G|G
	 * Planeswalker - Nissa (2)
	 * +1: Search your library for a card named Nissa's Chosen and put it onto the battlefield. Then shuffle your library.
	 * +1: You gain 2 life for each Elf you control.
	 * -7: Search your library for any number of Elf creature cards and put them onto the battlefield. Then shuffle your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	if (IS_ACTIVATING(event)){
		card_instance_t *instance = get_card_instance(player, card);
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.subtype = SUBTYPE_ELF;

		int priority_life = 0;
		int priority_ef = 0;
		if( event == EVENT_ACTIVATE ){
			priority_life = (count_subtype(player, TYPE_PERMANENT, SUBTYPE_ELF)*2)+(15-life[player]);
			int c = 0;
			while( deck_ptr[player][c] != -1 ){
					if( is_what(-1, deck_ptr[player][c], TYPE_PERMANENT) && has_subtype_by_id(cards_data[deck_ptr[player][c]].id, SUBTYPE_ELF) ){
						priority_ef+=3;
					}
					c++;
			}
		}

		enum{
			CHOICE_NC = 1,
			CHOICE_LIFE,
			CHOICE_ELF_FEST
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Search for Nissa's Chosen", player == AI ? (instance->targets[1].player != -1 ? 1 : 0) : 1, 10, 1,
						"Gain 2 life for each Elf", 1, priority_life, 1,
						"Search for Elf creatures", 1, priority_ef, -7);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_RESOLVE_ACTIVATION)
		{
		  switch (choice)
			{
				case CHOICE_NC:
					{
						this_test.id = CARD_ID_NISSAS_CHOSEN;
						int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);
						get_card_instance(instance->parent_controller, instance->parent_card)->targets[1].player = result;
					}
					break;

				case CHOICE_LIFE:
					gain_life(player, count_subtype(player, TYPE_PERMANENT, SUBTYPE_ELF)*2);
					break;

				case CHOICE_ELF_FEST:
					{
						this_test.no_shuffle = 1;
						while( 1 ){
								if( new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test) == -1 ){
									break;
								}
						}
						shuffle(player);
					}
					break;
			}
		}
	}

	return planeswalker(player, card, event, 2);
}

int card_nissas_chosen(int player, int card, event_t event)
{
/* CARD_ID_NISSAS_CHOSEN	10621
Nissa's Chosen	|G|G
Creature - Elf Warrior 2/3
If ~ would die, put it on the bottom of its owner's library instead. */

	int owner, position;
	if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) && find_in_owners_graveyard(player, card, &owner, &position))
	{
		int iid = get_grave(owner)[position];
		obliterate_card_in_grave(owner, position);
		deck_ptr[player][count_deck(player)] = iid;
	}

	return 0;
}

int card_oracle_of_mul_daya(int player, int card, event_t event){
/* CARD_ID_ORACLE_OF_MUL_DAYA	10626
Oracle of Mul Daya	|3|G
Creature - Elf Shaman 2/2
You may play an additional land on each of your turns.
Play with the top card of your library revealed.
You may play the top card of your library if it's a land card. */

	reveal_top_card(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		int* deck = deck_ptr[player];
		card_data_t* card_d = deck[0] == -1 ? NULL : &cards_data[ deck[0] ];

		if( can_sorcery_be_played(player, event) && deck[0] != -1 && ( card_d->type & TYPE_LAND ) &&
			! ( land_can_be_played & LCBP_LAND_HAS_BEEN_PLAYED ) )
		{
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		play_card_in_deck_for_free(player, player, 0);
		cant_be_responded_to = 1;
	}

	return card_exploration(player, card, event);
}

int card_oran_rief_recluse(int player, int card, event_t event){
/* CARD_ID_ORAN_RIEF_RECLUSE	10627
Oran-Rief Recluse	|2|G
Creature - Spider 1/3
Kicker |2|G (You may pay an additional |2|G as you cast this spell.)
Reach (This creature can block creatures with flying.)
When ~ enters the battlefield, if it was kicked, destroy target creature with flying. */

	kicker(player, card, event, MANACOST_XG(2, 1));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.required_abilities = KEYWORD_FLYING;
		td.allow_cancel = 0;
		if( can_target(&td) && pick_target(&td, "TARGET_FLYING") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_oran_rief_survivalist (int player, int card, event_t event){
/* CARD_ID_ORAN_RIEF_SURVIVALIST	10628
Oran-Rief Survivalist	|1|G
Creature - Human Warrior Ally 1/1
Whenever ~ or another Ally enters the battlefield under your control, you may put a +1/+1 counter on ~. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		add_1_1_counter(player, card);
	}
	return 0;
}

int card_predatory_urge(int player, int card, event_t event){
	/* Predatory Urge	|3|G
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature has "|T: This creature deals damage equal to its power to target creature. 
	   That creature deals damage equal to its power to this creature." */

	if( ! IS_GAA_EVENT(event) && ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

    int rval = aura_granting_activated_ability(player, card, event, player, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td1, "TARGET_CREATURE");
 
	if( ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_ACTIVATE ){
			instance->number_of_targets = 0;
		    return rval;
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( validate_target(player, card, &td1, 0) ){
				card_instance_t* i1 = in_play(p, c);
				if (!i1 || !is_what(p, c, TYPE_CREATURE)){
					return 0;
				}
				card_instance_t* i2 = in_play(instance->targets[0].player, instance->targets[0].card);
				if (!i2 || !is_what(instance->targets[0].player, instance->targets[0].card, TYPE_CREATURE)){
					return 0;
				}

				i1->regen_status |= KEYWORD_RECALC_POWER;
				int pow1 = get_abilities(p, c, EVENT_POWER, -1);

				i2->regen_status |= KEYWORD_RECALC_POWER;
				int pow2 = get_abilities(instance->targets[0].player, instance->targets[0].card, EVENT_POWER, -1);

				damage_creature(instance->targets[0].player, instance->targets[0].card, pow1, p, c);
				damage_creature(p, c, pow2, instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return rval;
}

int card_primal_bellow(int player, int card, event_t event){
	/* Primal Bellow	|G
	 * Instant
	 * Target creature gets +1/+1 until end of turn for each |H2Forest you control. */

	if( event == EVENT_CHECK_PUMP ){
		int amount = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_FOREST));
		return vanilla_instant_pump(player, card, event, ANYBODY, player, amount, amount, 0, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_FOREST));
		return vanilla_instant_pump(player, card, event, ANYBODY, player, amount, amount, 0, 0);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_quest_for_the_gemblades(int player, int card, event_t event){
/* Quest for the Gemblades	|1|G
 * Enchantment
 * Whenever a creature you control deals combat damage to a creature, you may put a quest counter on ~.
 * Remove a quest counter from ~ and sacrifice it: Put four +1/+1 counters on target creature. */

	card_instance_t* damage = combat_damage_being_dealt(event);
	if( damage && (damage->targets[3].player & TYPE_CREATURE) ){
		check_damage(player, -1, event, DDBM_MUST_BE_COMBAT_DAMAGE, player, card);
	}

	int result = resolve_damage_trigger(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE, player, card);
	if( result )
		add_counters(player, card, COUNTER_QUEST, result);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);
			add_1_1_counters( instance->targets[0].player, instance->targets[0].card, 4 );
		}
	}

	return quest(player, card, event, 1, &td, "TARGET_CREATURE");
}

int card_rampaging_baloths(int player, int card, event_t event)
{
/* CARD_ID_RAMPAGING_BALOTHS	10644
Rampaging Baloths	|4|G|G
Creature - Beast 6/6
Trample
Landfall - Whenever a land enters the battlefield under your control, you may create a 4/4 |Sgreen Beast creature token. */

	landfall_mode(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY )
	{
	  token_generation_t token;
	  default_token_definition(player, card, CARD_ID_BEAST, &token);
	  token.pow = token.tou = 4;
	  generate_token(&token);
	}

  return 0;
}

int card_relic_crush(int player, int card, event_t event){
/* CARD_ID_RELIC_CRUSH	10647
Relic Crush	|4|G
Instant
Destroy target artifact or enchantment and up to one other target artifact or enchantment. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT_ENCHANTMENT", 1, NULL);
		if( spell_fizzled != 1 ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			if( can_target(&td) ){
				new_pick_target(&td, "TARGET_ARTIFACT_ENCHANTMENT", 1, 0);
			}
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_savage_silhouette(int player, int card, event_t event){
/* CARD_ID_SAVAGE_SILHOUETTE	10653
Savage Silhouette	|2|G
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2 and has "|1|G: Regenerate this creature." */

	if( ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			event_result += 2;
		}

		if( event == EVENT_RESOLVE_ACTIVATION )
			if( can_be_regenerated(instance->damage_target_player, instance->damage_target_card))
				regenerate_target(instance->damage_target_player, instance->damage_target_card);
	}

	return aura_granting_activated_ability(player, card, event, player, GAA_REGENERATION, MANACOST_XG(1, 1), 0, NULL, NULL);
}


int card_scute_mob(int player, int card, event_t event){
/* CARD_ID_SCUTE_MOB	10655
Scute Mob	|G
Creature - Insect 1/1
At the beginning of your upkeep, if you control five or more lands, put four +1/+1 counters on ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( count_subtype(player, TYPE_LAND, -1) >= 5 ){
			add_1_1_counters(player, card, 4);
		}
	}

	return 0;
}

int card_scythe_tiger(int player, int card, event_t event){
/* CARD_ID_SCYTHE_TIGER	10656
Scythe Tiger	|G
Creature - Cat 3/2
Shroud (This creature can't be the target of spells or abilities.)
When ~ enters the battlefield, sacrifice it unless you sacrifice a land. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		int kill_me = ! new_sacrifice(player, card, player, SAC_AS_COST, &test);
		if( kill_me ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_summoning_trap(int player, int card, event_t event){
/* CARD_ID_SUMMONING_TRAP	10679
Summoning Trap	|4|G|G
Instant - Trap
If a creature spell you cast this turn was countered by a spell or ability an opponent controlled, you may pay |0 rather than pay ~'s mana cost.
Look at the top seven cards of your library. You may put a creature card from among them onto the battlefield. Put the rest on the bottom of your library in any order. */

	if( event == EVENT_MODIFY_COST ){
		if( get_trap_condition(player, TRAP_SUMMONING_TRAP) ){
			null_casting_cost(player, card);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int max = 7;
		if( max > count_deck(player) ){
			max = count_deck(player);
		}
		if( max > 0 ){
			char msg[100] = "Select a creature card.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, msg);
			this_test.create_minideck = max;
			this_test.no_shuffle = 1;

			if( new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test) != 1 ){
				max--;
			}
			if( max > 0 ){
				put_top_x_on_bottom(player, player, max);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_tajuru_archer(int player, int card, event_t event){
/* CARD_ID_TAJURU_ARCHER	10682
Tajuru Archer	|2|G
Creature - Elf Archer Ally 1/2
Whenever ~ or another Ally enters the battlefield under your control,
you may have ~ deal damage to target creature with flying equal to the number of Allies you control. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.required_abilities = KEYWORD_FLYING;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, count_allies(player, card), player, card);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_terra_stomper(int player, int card, event_t event){
/* CARD_ID_TERRA_STOMPER	10686
Terra Stomper	|3|G|G|G
Creature - Beast 8/8
~ can't be countered.
Trample (This creature can deal excess combat damage to defending player or planeswalker while attacking.) */

	cannot_be_countered(player, card, event);
	return 0;
}

int card_timbermaw_larva( int player, int card, event_t event)
{
/* CARD_ID_TIMBERMAW_LARVA	10688
Timbermaw Larva	|3|G
Creature - Beast 2/2
Whenever ~ attacks, it gets +1/+1 until end of turn for each |H2Forest you control. */

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  int amt = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_FOREST));
	  if (amt > 0)
		pump_until_eot_merge_previous(player, card, player, card, amt, amt);
	}

  return 0;
}

int card_turntimber_basilisk(int player, int card, event_t event){
/* CARD_ID_TURNTIMBER_BASILISK	10695
Turntimber Basilisk	|1|G|G
Creature - Basilisk 2/1
Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.)
Landfall - Whenever a land enters the battlefield under your control, you may have target creature block ~ this turn if able. */

	deathtouch(player, card, event);

	landfall_mode(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.allowed_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			target_must_block_me(player, card, instance->targets[0].player, instance->targets[0].card, 1);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_turntimber_ranger(int player, int card, event_t event){
	/* Turntimber Ranger	|3|G|G
	 * Creature - Elf Scout Ally 2/2
	 * Whenever ~ or another Ally enters the battlefield under your control, you may put a 2/2 |Sgreen Wolf creature token onto the battlefield. If you do, put a +1/+1 counter on ~. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		add_1_1_counter(player, card);
		generate_token_by_id(player, card, CARD_ID_WOLF);
	}
	return 0;
}

int card_vines_of_vastwood(int player, int card, event_t event){
/* CARD_ID_VINES_OF_VASTWOOD	10707
Vines of Vastwood	|G
Instant
Kicker |G (You may pay an additional |G as you cast this spell.)
Target creature can't be the target of spells or abilities your opponents control this turn. If ~ was kicked, that creature gets +4/+4 until end of turn. */

	if( event == EVENT_CHECK_PUMP && has_mana(player, COLOR_GREEN, 2) ){
		pumpable_power[player] += 4;
		pumpable_toughness[player] += 4;
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			do_kicker(player, card, MANACOST_G(1));
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int boost = 0;
			if( kicked(player, card) ){
				boost = 4;
			}
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
									boost, boost, KEYWORD_SHROUD, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

// black
int card_bala_ged_thief(int player, int card, event_t event){
/* CARD_ID_BALA_GED_THIEF	10500
Bala Ged Thief	|3|B
Creature - Human Rogue Ally 2/2
Whenever ~ or another Ally enters the battlefield under your control,
target player reveals a number of cards from his or her hand equal to the number of Allies you control. You choose one of them. That player discards that card. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( event == EVENT_ETB_ABILITY ){
		if (pick_player_duh(player, card, 1-player, 0)){
			int tgt = get_card_instance(player, card)->targets[0].player;
			int amount = count_subtype(player, TYPE_PERMANENT, SUBTYPE_ALLY);

			ec_definition_t ec;
			default_ec_definition(tgt, player, &ec);
			if( amount < hand_count[tgt] ){
				ec.cards_to_reveal = amount;
			}

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			new_effect_coercion(&ec, &this_test);
		}
	}
	return 0;
}

int card_blood_seeker(int player, int card, event_t event){
/* CARD_ID_BLOOD_SEEKER	10508
Blood Seeker	|1|B
Creature - Vampire Shaman 1/1
Whenever a creature enters the battlefield under an opponent's control, you may have that player lose 1 life. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		etb_ability_global(1-player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		lose_life(1-player, 1);
	}

	return 0;
}

int card_blood_tribute(int player, int card, event_t event)
{
/* CARD_ID_BLOOD_TRIBUTE	10509
Blood Tribute	|4|B|B
Sorcery
Kicker-Tap an untapped Vampire you control. (You may tap a Vampire you control in addition to any other costs as you cast this spell.)
Target opponent loses half his or her life, rounded up. If ~ was kicked, you gain life equal to the life lost this way. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if (event == EVENT_CAN_CAST)
		return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
		generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
		if( spell_fizzled != 1 ){
			target_definition_t td2;
			default_target_definition(player, card, &td2, TYPE_PERMANENT);
			td2.required_subtype = SUBTYPE_VAMPIRE;
			td2.illegal_state = TARGET_STATE_TAPPED;
			td2.allowed_controller = player;
			td2.preferred_controller = player;
			td2.allow_cancel = 0;

			if( can_target(&td2) ){
				if( do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass", 0) == 0 ){
					if( new_pick_target(&td2, "Select a Vampire you control.", 0, GS_LITERAL_PROMPT) ){
						card_instance_t *instance = get_card_instance(player, card);
						tap_card(instance->targets[0].player, instance->targets[0].card);
						set_special_flags(player, card, SF_KICKED);
						instance->number_of_targets = 0;
					}
				}
			}
		}
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
		if( valid_target(&td) ){
			int life_lost = lose_life(1-player, life[1-player] / 2);
			if (kicked(player, card))
				gain_life(player, life_lost);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, "TARGET_PLAYER", 1, NULL);
}

int card_bloodchief_ascension(int player, int card, event_t event)
{
  /* Bloodchief Ascension	|B
   * Enchantment
   * At the beginning of each end step, if an opponent lost 2 or more life this turn, you may put a quest counter on ~.
   * Whenever a card is put into an opponent's graveyard from anywhere, if ~ has three or more quest counters on it, you may have that player lose 2 life. If
   * you do, you gain 2 life. */

  if (get_trap_condition(1-player, TRAP_LIFE_LOST) >= 2 && eot_trigger_mode(player, card, event, current_turn, RESOLVE_TRIGGER_AI(player)))
	add_counter(player, card, COUNTER_QUEST);

  if (count_counters(player, card, COUNTER_QUEST) >= 3)
	{
	  // from library
	  enable_xtrigger_flags |= ENABLE_XTRIGGER_MILLED;
	  if (xtrigger_condition() == XTRIGGER_MILLED && affect_me(player, card) && reason_for_trigger_controller == player
		  && trigger_cause_controller == 1-player && !is_humiliated(player, card))
		{
		  if (event == EVENT_TRIGGER)
			event_result |= duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;

		  if (event == EVENT_RESOLVE_TRIGGER)
			{
			  lose_life(1-player, num_cards_milled * 2);
			  gain_life(player, num_cards_milled * 2);
			}
		}

	  if (when_a_card_is_put_into_graveyard_from_anywhere_but_library_do_something(player, card, event, 1-player, RESOLVE_TRIGGER_DUH, NULL))
		{
		  lose_life(1-player, 2);
		  gain_life(player, 2);
		}
	}

  return global_enchantment(player, card, event);
}

int card_bloodghast(int player, int card, event_t event){
/* CARD_ID_BLOODGHAST	10511
Bloodghast	|B|B
Creature - Vampire Spirit 2/1
~ can't block.
~ has haste as long as an opponent has 10 or less life.
Landfall - Whenever a land enters the battlefield under your control, you may return ~ from your graveyard to the battlefield. */

	cannot_block(player, card, event);
	if( life[1-player]<=10 && ! is_humiliated(player, card) ){
		haste(player, card, event);
	}
	return 0;
}

int card_crypt_ripper(int player, int card, event_t event){
/* CARD_ID_CRYPT_RIPPER	10525
Crypt Ripper	|2|B|B
Creature - Shade 2/2
Haste
|B: ~ gets +1/+1 until end of turn. */

	haste(player, card, event);
	return card_nantuko_shade(player, card, event);
}

int card_desecrated_earth(int player, int card, event_t event){
/* CARD_ID_DESECRATED_EARTH	10528
Desecrated Earth	|4|B
Sorcery
Destroy target land. Its controller discards a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			discard(instance->targets[0].player, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
}

int card_disfigure(int player, int card, event_t event){
/* CARD_ID_DISFIGURE	10530
Disfigure	|B
Instant
Target creature gets -2/-2 until end of turn. */

	return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -2, -2, 0, 0);
}

int card_feast_of_blood(int player, int card, event_t event){
/* CARD_ID_FEAST_OF_BLOOD	10539
Feast of Blood	|1|B
Sorcery
Cast ~ only if you control two or more Vampires.
Destroy target creature. You gain 4 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( count_subtype(player, TYPE_PERMANENT, SUBTYPE_VAMPIRE) < 2 ){
			return 0;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			gain_life(player, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}


int card_gatekeeper_of_malakir(int player, int card, event_t event){
/* CARD_ID_GATEKEEPER_OF_MALAKIR	10542
Gatekeeper of Malakir	|B|B
Creature - Vampire Warrior 2/2
Kicker |B (You may pay an additional |B as you cast this spell.)
When ~ enters the battlefield, if it was kicked, target player sacrifices a creature. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags(player, card, SF_NOT_CAST) && ! is_token(player, card) ){
			if( player == AI ){
				target_definition_t td;
				default_target_definition(player, card, &td, TYPE_CREATURE);
				td.zone = TARGET_ZONE_PLAYERS;
				if( would_validate_arbitrary_target(&td, 1-player, -1) ){
					do_kicker(player, card, MANACOST_B(1));
				}
			}
			else{
				do_kicker(player, card, MANACOST_B(1));
			}
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		// choose a player
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( pick_target(&td, "TARGET_PLAYER") ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			new_sacrifice(player, card, instance->targets[0].player,  SAC_CAUSED | SAC_NO_CANCEL, &test);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_grim_discovery(int player, int card, event_t event)
{
  /* Grim Discovery	|1|B
   * Sorcery
   * Choose one or both - Return target creature card from your graveyard to your hand; and/or return target land card from your graveyard to your hand. */

  return spell_return_one_or_two_cards_from_gy_to_hand(player, card, event, TYPE_CREATURE, TYPE_LAND);
}

int card_guul_draz_specter(int player, int card, event_t event)
{
  /* Guul Draz Specter	|2|B|B
   * Creature - Specter 2/2
   * Flying
   * ~ gets +3/+3 as long as an opponent has no cards in hand.
   * Whenever ~ deals combat damage to a player, that player discards a card. */

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && hand_count[1-player] == 0 &&
		! is_humiliated(player, card) )
		event_result += 3;

  whenever_i_deal_damage_to_a_player_he_discards_a_card(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE, 0);

  return 0;
}

int card_guul_draz_vampire(int player, int card, event_t event){
/* CARD_ID_GUUL_DRAZ_VAMPIRE	10558
Guul Draz Vampire	|B
Creature - Vampire Rogue 1/1
As long as an opponent has 10 or less life, ~ gets +2/+1 and has intimidate.
(It can't be blocked except by artifact creatures and/or creatures that share a color with it.) */

	if( life[1-player] <= 10 && ! is_humiliated(player, card)){
		if( affect_me(player, card) ){
			if( event == EVENT_POWER ){
				event_result += 2;
			}
			else if( event == EVENT_TOUGHNESS ){
				event_result++;
			}
		}
		intimidate(player, card, event);
	}

	return 0;
}

int card_hagra_crocodile(int player, int card, event_t event){
/* CARD_ID_HAGRA_CROCODILE	10559
Hagra Crocodile	|3|B
Creature - Crocodile 3/1
~ can't block.
Landfall - Whenever a land enters the battlefield under your control, ~ gets +2/+2 until end of turn. */

	cannot_block(player, card, event);
	landfall(player, card, event);
	if( event == EVENT_ETB_ABILITY ){
		pump_until_eot_merge_previous(player, card, player, card, 2, 2);
	}
	return 0;
}

int card_hagra_diabolist(int player, int card, event_t event){
/* CARD_ID_HAGRA_DIABOLIST	10560
Hagra Diabolist	|4|B
Creature - Ogre Shaman Ally 3/2
Whenever ~ or another Ally enters the battlefield under your control, you may have target player lose life equal to the number of Allies you control. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		if (pick_player_duh(player, card, 1-player, 1)){
			card_instance_t* instance = get_card_instance(player, card);
			lose_life(instance->targets[0].player, count_allies(player, card));
		}
	}
	return 0;
}

int card_halo_hunter(int player, int card, event_t event){
/* CARD_ID_HALO_HUNTER	10561
Halo Hunter	|2|B|B|B
Creature - Demon 6/3
Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)
When ~ enters the battlefield, destroy target Angel. */

	intimidate(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT );
		td.required_subtype = SUBTYPE_ANGEL;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );
		if (can_target(&td) && select_target(player, card, &td, "Select target Angel.", NULL)){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_heartstabber_mosquito(int player, int card, event_t event){
/* CARD_ID_HEARTSTABBER_MOSQUITO	10562
Heartstabber Mosquito	|3|B
Creature - Insect 2/2
Kicker |2|B (You may pay an additional |2|B as you cast this spell.)
Flying
When ~ enters the battlefield, if it was kicked, destroy target creature. */

	kicker(player, card, event, MANACOST_XB(2, 1));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY  && kicked(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;
		card_instance_t *instance = get_card_instance(player, card);
		if( pick_target(&td, "TARGET_CREATURE") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY );
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_hideous_end(int player, int card, event_t event){
/* CARD_ID_HIDEOUS_END	10567
Hideous End	|1|B|B
Instant
Destroy target non|Sblack creature. Its controller loses 2 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
							get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, NULL);
	}
	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY );
			lose_life(instance->targets[0].player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_kalitas_bloodchief_of_ghet(int player, int card, event_t event){
/* CARD_ID_KALITAS_BLOODCHIEF_OF_GHET	10578
Kalitas, Bloodchief of Ghet	|5|B|B
Legendary Creature - Vampire Warrior 5/5
|B|B|B, |T: Destroy target creature. If that creature dies this way, create a |Sblack Vampire creature token.
Its power is equal to that creature's power and its toughness is equal to that creature's toughness. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int power = get_power(instance->targets[0].player, instance->targets[0].card);
			int toughness = get_toughness(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY );

			token_generation_t token;
			default_token_definition(player, card, CARD_ID_VAMPIRE, &token);
			token.pow = power;
			token.tou = toughness;
			generate_token(&token);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(3), 0, &td, "TARGET_CREATURE");
}

int card_malakir_bloodwitch(int player, int card, event_t event){
/* CARD_ID_MALAKIR_BLOODWITCH	10603
Malakir Bloodwitch	|3|B|B
Creature - Vampire Shaman 4/4
Flying, protection from |Swhite
When ~ enters the battlefield, each opponent loses life equal to the number of Vampires you control. You gain life equal to the life lost this way. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int count = count_subtype(player, TYPE_CREATURE, SUBTYPE_VAMPIRE);
		gain_life(player, count);
		lose_life(1-player, count);
	}
	return 0;
}

int card_marsh_casualties(int player, int card, event_t event){

	/* Marsh Casualties	|B|B
	 * Sorcery
	 * Kicker |3
	 * Creatures target player controls get -1/-1 until end of turn. If ~ was kicked, those creatures get -2/-2 until end of turn instead. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			do_kicker(player, card, MANACOST_X(3));
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		if (valid_target(&td)){
			int num = -1;
			if( kicked(player, card) ){
				num = -2;
			}
			card_instance_t* instance = get_card_instance(player, card);
			pump_creatures_until_eot(player, card, instance->targets[0].player, 0, num,num, 0,0, NULL);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_mindless_null(int player, int card, event_t event){
/* CARD_ID_MINDLESS_NULL	10610
Mindless Null	|2|B
Creature - Zombie 2/2
~ can't block unless you control a Vampire. */

	if( event == EVENT_BLOCK_LEGALITY && affect_me(player, card) && ! is_humiliated(player, card) ){
		if( ! check_battlefield_for_subtype(player, TYPE_CREATURE, SUBTYPE_VAMPIRE) ){
			cannot_block(player, card, event);
		}
	}
	return 0;
}

int card_needlebite_trap(int player, int card, event_t event){

	/* Needlebite Trap	|5|B|B
	 * Instant - Trap
	 * If an opponent gained life this turn, you may pay |B rather than pay ~'s mana cost.
	 * Target player loses 5 life and you gain 5 life. */

	if( event == EVENT_MODIFY_COST ){
		if( get_trap_condition(1-player, TRAP_LIFE_GAINED) > 0 ){
			COST_COLORLESS-=5;
			COST_BLACK--;
		}
	}

	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL ){
		if (valid_target(&td)){
			card_instance_t* instance = get_card_instance(player, card);
			lose_life(instance->targets[0].player, 5);
			gain_life(player, 5 );
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_ob_nixilis_the_fallen(int player, int card, event_t event){
/* Ob Nixilis, the Fallen	|3|B|B
 * Legendary Creature - Demon 3/3
 * Landfall - Whenever a land enters the battlefield under your control, you may have target player lose 3 life.
 * If you do, put three +1/+1 counters on * ~. */

	landfall(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			lose_life(instance->targets[0].player, 3);
			add_1_1_counters(player, card, 3);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_quest_for_the_gravelord(int player, int card, event_t event){

	/* Quest for the Gravelord	|B
	 * Enchantment
	 * Whenever a creature dies, you may put a quest counter on ~.
	 * Remove three quest counters from ~ and sacrifice it: Put a 5/5 |Sblack Zombie Giant creature token onto the battlefield. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ZOMBIE_GIANT, &token);
		token.pow = 5;
		token.tou = 5;
		generate_token(&token);
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
	}

	if( in_play(player, card) && resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		card_instance_t* instance = get_card_instance(player, card);
		add_counters(player, card, COUNTER_QUEST, instance->targets[11].card);
		instance->targets[11].card = 0;
	}

	return quest(player, card, event, 3, NULL, 0);
}

int card_ravenous_trap(int player, int card, event_t event){

	/* Ravenous Trap	|2|B|B
	 * Instant - Trap
	 * If an opponent had three or more cards put into his or her graveyard from anywhere this turn, you may pay |0 rather than pay ~'s mana cost.
	 * Exile all cards from target player's graveyard. */

	if( event == EVENT_MODIFY_COST ){
		if( get_trap_condition(1-player, TRAP_CARDS_TO_GY_FROM_ANYWHERE) >= 3 ){
			null_casting_cost(player, card);
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL ){
		if (valid_target(&td)){
			rfg_whole_graveyard(get_card_instance(player, card)->targets[0].player);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_sadistic_sacrament(int player, int card, event_t event){
/* CARD_ID_SADISTIC_SACRAMENT	10652
Sadistic Sacrament	|B|B|B
Sorcery
Kicker |7 (You may pay an additional |7 as you cast this spell.)
Search target player's library for up to three cards, exile them, then that player shuffles his or her library.
If ~ was kicked, instead search that player's library for up to fifteen cards, exile them, then that player shuffles his or her library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			do_kicker(player, card, MANACOST_X(7));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = kicked(player, card) ? 15 : 3;
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card to exile.");
			test.qty = amount;
			new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_MAX_VALUE, &test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int effect_sorin_markov2(int player, int card, event_t event){
	if( eot_trigger(player, card, event) ){
		draw_a_card(1-player);
		kill_card(player, card, KILL_SACRIFICE );
	}
	return 0;
}

static int effect_sorin_markov(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	if( current_phase > PHASE_MAIN2 && instance->targets[0].player != 1 ){
		instance->targets[0].player = 1;
		int i;
		int p = 1-player;
		for(i=0;i<active_cards_count[p];i++){
			card_data_t* card_d = get_card_data(p, i);
			if((card_d->type & TYPE_LAND) && in_play(p, i)){
				tap_card(p, i);
			}
		}
		create_legacy_effect(player, card, &effect_sorin_markov2 );
		card_time_walk(player, card, EVENT_RESOLVE_SPELL);
	}
	return 0;
}

int card_sorin_markov(int player, int card, event_t event){
	/* Sorin Markov	|3|B|B|B	0x2001ccc
	 * Legendary Planeswalker - Sorin (4)
	 * +2: ~ deals 2 damage to any target and you gain 2 life.
	 * -3: Target opponent's life total becomes 10.
	 * -7: You control target player during that player's next turn. */
	if (IS_ACTIVATING(event)){

		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		target_definition_t td2;
		default_target_definition(player, card, &td2, 0);
		td2.zone = TARGET_ZONE_PLAYERS;

		enum{
			CHOICE_DAMAGE = 1,
			CHOICE_SET_LIFE,
			CHOICE_MINDSLAVER
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Damage & gain life", can_target(&td), 10, 2,
						"Set life to 10", would_validate_arbitrary_target(&td2, 1-player, -1), (life[1-player]-10)*3, -3,
						"Mindslaver (approx.)", would_validate_arbitrary_target(&td2, 1-player, -1), 20, -7);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_ACTIVATE)
		{
		  instance->number_of_targets = 0;
		  switch (choice)
			{
				case CHOICE_DAMAGE:
					pick_target(&td, "TARGET_ANY");
					break;

				case CHOICE_SET_LIFE:
				case CHOICE_MINDSLAVER:
				{
					instance->targets[0].player = 1-player;
					instance->targets[0].card = -1;
					instance->number_of_targets =  1;
				}
				break;

			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		switch (choice)
		{
			case CHOICE_DAMAGE:
				if ( valid_target(&td)){
					damage_target0(player, card, 2);
					gain_life(player, 2);
				}
				break;

			case CHOICE_SET_LIFE:
				if ( valid_target(&td2)){
					set_life_total(instance->targets[0].player , 10);
				}
				break;

			case CHOICE_MINDSLAVER:
				if ( valid_target(&td2) ){
					create_legacy_effect(player, card, &effect_sorin_markov );
				}
				break;
		}
	}

	return planeswalker(player, card, event, 4);
}

int card_soul_stair_expedition(int player, int card, event_t event)
{
  /* Soul Stair Expedition	|B
   * Enchantment
   * Landfall - Whenever a land enters the battlefield under your control, you may put a quest counter on ~.
   * Remove three quest counters from ~ and sacrifice it: Return up to two target creature cards from your graveyard to your hand. */

	landfall_mode(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY )
		add_counter(player, card, COUNTER_QUEST);

  if (event == EVENT_CAN_ACTIVATE)
	return count_counters(player, card, COUNTER_QUEST) >= 3 && CAN_ACTIVATE0(player, card) && can_sacrifice_this_as_cost(player, card);

  if (event == EVENT_ACTIVATE)
	{
	  if (!charge_mana_for_activated_ability(player, card, MANACOST0))
		return 0;

	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "Select up to two target creature cards.");

	  // Translate from EVENT_ACTIVATE to EVENT_CAST_SPELL && affect_me(player, card)
	  int old_affected_card_controller = affected_card_controller;
	  affected_card_controller = player;
	  int old_affected_card = affected_card;
	  affected_card = card;

	  spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(player, card, EVENT_CAST_SPELL, 2, &test, 1);

	  affected_card_controller = old_affected_card_controller;
	  affected_card = old_affected_card;

	  kill_card(player, card, KILL_SACRIFICE);
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(player, card, EVENT_RESOLVE_SPELL, 2, NULL, 1);

  return 0;
}

int card_surrakar_marauder(int player, int card, event_t event){
/* CARD_ID_SURRAKAR_MARAUDER	10681
Surrakar Marauder	|1|B
Creature - Surrakar 2/1
Landfall - Whenever a land enters the battlefield under your control, ~ gains intimidate until end of turn.
(It can't be blocked except by artifact creatures and/or creatures that share a color with it.) */

	landfall(player, card, event);
	if( event == EVENT_ETB_ABILITY ){
		pump_ability_until_eot(player, card, player, card, 0, 0, 0, SP_KEYWORD_INTIMIDATE);
	}
	return 0;
}

static const char* target_must_have_counters(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  return count_counters(player, card, -1) ? NULL : "must have counters";
}
int card_vampire_hexmage(int player, int card, event_t event)
{
  /* Vampire Hexmage	|B|B
   * Creature - Vampire Shaman 2/1
   * First strike
   * Sacrifice ~: Remove all counters from target permanent. */

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_PERMANENT);
  td.preferred_controller = ANYBODY;
  if (IS_AI(player) && event != EVENT_RESOLVE_ACTIVATION)
	{
	  /* Prevents the AI from considering removal of counters from permanents without them (thus speculating more on which type of counter to remove from
	   * permanents that *do* have them).  However, it also prevents it from using this ability to kill a Skulking Ghost or activate another whenever-this-
	   * becomes-targeted trigger.  I think this is an acceptable tradeoff. */
	  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	  td.extra = (int)target_must_have_counters;
	}

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  remove_all_counters(instance->targets[0].player, instance->targets[0].card, -1);
	}

  return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_PERMANENT");
}

int card_vampire_lacerator(int player, int card, event_t event){
/* CARD_ID_VAMPIRE_LACERATOR	10702
Vampire Lacerator	|B
Creature - Vampire Warrior 2/2
At the beginning of your upkeep, you lose 1 life unless an opponent has 10 or less life. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( life[1-player] > 10 ){
			lose_life(player, 1);
		}
	}
	return 0;
}

int card_vampire_nighthawk(int player, int card, event_t event){
/* CARD_ID_VAMPIRE_NIGHTHAWK	10703
Vampire Nighthawk	|1|B|B
Creature - Vampire Shaman 2/3
Flying, deathtouch, lifelink */

	lifelink(player, card, event);
	deathtouch(player, card, event);
	return 0;
}

int card_vampires_bite(int player, int card, event_t event){
/* CARD_ID_VAMPIRES_BITE	10704
Vampire's Bite	|B
Instant
Kicker |2|B (You may pay an additional |2|B as you cast this spell.)
Target creature gets +3/+0 until end of turn. If ~ was kicked, that creature gains lifelink until end of turn.
(Damage dealt by the creature also causes its controller to gain that much life.) */

	if( event == EVENT_CHECK_PUMP ){
		return vanilla_instant_pump(player, card, event, ANYBODY, player, 3, 0, 0, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			do_kicker(player, card, MANACOST_XB(2, 1));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int ability = 0;
			if( kicked(player, card) ){
				ability = SP_KEYWORD_LIFELINK;
			}
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3, 0, 0, ability);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}


// red
int card_bladetusk_boar(int player, int card, event_t event){
/* CARD_ID_BLADETUSK_BOAR	10506
Bladetusk Boar	|3|R
Creature - Boar 3/2
Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.) */

	intimidate(player, card, event);
	return 0;
}

int card_burst_lightning(int player, int card, event_t event){
	/* Burst Lightning	|R	0x2001b0a
	 * Instant
	 * Kicker |4
	 * ~ deals 2 damage to any target. If this spell was kicked, it deals 4 damage to that permanent or player instead. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 ){
			do_kicker(player, card, MANACOST_X(4));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int dmg = kicked(player, card) ? 4 : 2;
			damage_target0(player, card, dmg);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_chandra_ablaze(int player, int card, event_t event){
	/* Chandra Ablaze	|4|R|R	0x2001b0f
	 * Legendary Planeswalker - Chandra (5)
	 * +1: Discard a card. If a |Sred card is discarded this way, ~ deals 4 damage to any target.
	 * -2: Each player discards their hand, then draws three cards.
	 * -7: Cast any number of |Sred instant and/or sorcery cards from your graveyard without paying their mana costs. */
	if (IS_ACTIVATING(event)){

		card_instance_t* instance = get_card_instance(player, card);

		int priorities[3] = {0, 0, 0};

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		this_test.zone = TARGET_ZONE_HAND;

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		if( event == EVENT_ACTIVATE ){
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
			priorities[0] = check_battlefield_for_special_card(player, card, player, 0, &this_test) ? 10 : 0;
			this_test.color = 0;

			priorities[1] = hand_count[player] < 3 && hand_count[1-player] > hand_count[player] ? 15 : 0;

			int c = 0;
			const int *grave = get_grave(player);
			while( grave[c] != -1 ){
					if( is_what(-1, grave[c], TYPE_SPELL) &&
						(get_color_by_internal_id(player, grave[c]) & get_sleighted_color_test(player, card, COLOR_TEST_RED)) &&
						can_legally_play_iid(player, grave[c])
					  ){
						priorities[2]+=5;
					}
					c++;
			}
		}

		enum{
			CHOICE_DISCARD = 1,
			CHOICE_MINI_WOF,
			CHOICE_CAST_FROM_GRAVE
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Discard & damage", can_target(&td), priorities[0], 1,
						"All discard & draw 3", 1, priorities[1], -2,
						"Play spells from grave", 1, priorities[2], -7);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_ACTIVATE)
		{
			instance->number_of_targets = 0;
			switch (choice)
			{
				case CHOICE_DISCARD:
					pick_target(&td, "TARGET_ANY");
					break;

				default:
					break;
			}
		}
	  else if (event == EVENT_RESOLVE_ACTIVATION)
		{
			switch (choice)
			{
				case CHOICE_DISCARD:
				if( hand_count[player] > 0 && valid_target(&td) ){
					/*
					10/1/2009: If you activate Chandra Ablaze's first ability, you don't discard a card until the ability resolves. You may activate the ability even if your hand is empty. You choose a target as you activate the ability even if you have no red cards in hand at that time.
					10/1/2009: As the first ability resolves, nothing happens if your hand is empty. But if you have any cards in hand, you must discard one. If you discard a nonred card, Chandra doesn't deal any damage.
					10/1/2009: If the creature targeted by the first ability is an illegal target by the time it resolves, the entire ability is countered. You won't discard a card.
					*/
					if( player == AI ){
						this_test.color = get_sleighted_color_test(instance->parent_controller, instance->parent_card, COLOR_TEST_RED);
					}
					int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &this_test);

					// was a red card chosen?
					int red_card = get_color(player, selected) & get_sleighted_color_test(instance->parent_controller, instance->parent_card, COLOR_TEST_RED);

					discard_card(player, selected);

					if( red_card ){
						damage_target0(player, card, 4);
					}
				}
				break;

				case CHOICE_MINI_WOF:
				{
					APNAP(p, {
								new_discard_all(p, player);
								draw_cards(p, 3);
							};);
				}
				break;

				case CHOICE_CAST_FROM_GRAVE:
				{
					/* This is a bit complex, since we don't want to allow repeated castings (the cards go back into the graveyard; they don't get exiled or anything
					 * else nearly so convenient), but the list of valid cards would change after each spell if we just cast them one after another, both because the
					 * available targets change after each spell resolves, and because one spell could destroy a Painter's Servant making the other cards red.
					 *
					 * The way it's supposed to work under modern rules is to put all cards on the stack, then pop each one off.  We could, in theory, do that, but it's
					 * difficult to do it cleanly due to the way Manalink handles interrupt timing.  Plus, cancelling during target selection - already problematic when
					 * casting from anywhere but hand - would be particularly disastrous here.  Maybe once put_card_on_stack1() is moved into C.
					 *
					 * So the current approximation is to first choose each card, remove them all from the graveyard, and then, for each, put it back into the graveyard
					 * and cast it from there. */

					const int *grave = get_grave(player);
					int i, sources[500], positions[500], iids[500], num_valid = 0;

					int red = get_sleighted_color_test(player, card, COLOR_TEST_RED);
					int servant = get_global_color_hack(player);

					for (i = 0; i < 500 && grave[i] != -1; ++i){
						if (((get_color_by_internal_id(player, grave[i]) | servant) & red) &&
							is_what(-1, grave[i], TYPE_SPELL) &&
							can_legally_play_iid(player, grave[i])
						   ){
							sources[num_valid] = graveyard_source[player][i];
							positions[num_valid] = i;
							iids[num_valid] = grave[i];
							++num_valid;
						}
					}

					int chosen_iids[500], num_chosen = 0;

					while (num_valid > 0){
						int selected = show_deck(player, iids, num_valid, "Select a card to play.", 0, 0x7375B0);
						if (selected == -1){
							break;
						}

						// Add to list of chosen cards
						chosen_iids[num_chosen] = iids[selected];
						++num_chosen;

						// Remove from graveyard
						int pos = find_in_graveyard_by_source(player, sources[selected], positions[selected]);
						ASSERT(pos != -1);	// nothing has had a chance to remove it from the graveyard except this
						remove_card_from_grave(player, pos);

						// Remove from choosable cards
						--num_valid;
						int j;
						for (j = selected; j < num_valid; ++j){
							sources[j] = sources[j + 1];
							positions[j] = positions[j + 1];
							iids[j] = iids[j + 1];
						}
					}

					// Put each card back in grave and cast it from there
					for (i = 0; i < num_chosen; ++i){
						int pos = raw_put_iid_on_top_of_graveyard(player, chosen_iids[i]);
						increase_trap_condition(player, TRAP_CARDS_TO_GY_FROM_ANYWHERE, -1);    // Since it's not being put into the graveyard now; we're just reversing our previous hack

						play_card_in_grave_for_free(player, player, pos);
						/* play_card_in_grave_for_free() checks can_legally_play_iid().  If that fails, this is a bit inaccurate: the spell was legal to play initially,
						 * but isn't now.  That might be because e.g. it requires two targets, but only one is still valid; in that case, it shouldn't fizzle, but just
						 * resolve on the other target.  Oh well. */
					}
				}
				break;

			}
		}
	}

	return planeswalker(player, card, event, 5);
}

int card_electropotence(int player, int card, event_t event)
{
	/* Electropotence	|2|R	0x2001b14
	 * Enchantment
	 * Whenever a creature enters the battlefield under your control, you may pay |2|R. If you do, that creature deals damage equal to its power to any target. */
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(trigger_cause_controller, trigger_cause, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		card_instance_t* instance = get_card_instance(trigger_cause_controller, trigger_cause);
		if( can_target(&td) && has_mana_multi(player, MANACOST_XR(2, 1)) &&
			charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, MANACOST_XR(2, 1)) )
		{
			if( pick_target(&td, "TARGET_ANY") )
			{
				int pow = get_power(trigger_cause_controller, trigger_cause);
				damage_target0(trigger_cause_controller, trigger_cause, pow);
			}
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

static int elemental_appeal_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	int p = instance->damage_target_player;
	int c = instance->damage_target_card;

	if( p > -1 ){
		haste(p, c, event);

		if( event == EVENT_POWER && affect_me(p, c) && instance->targets[2].player > 0 ){
			event_result += instance->targets[2].player;
		}

		if( trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player ){
			if( instance->targets[1].player > 0 ){
				instance->targets[1].player--;
			}
			else{
				if( eot_trigger(player, card, event) ){
					if( ! check_state(p, c, STATE_OUBLIETTED) ){
						kill_card(p, c, KILL_EXILE);
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

static void create_elemental_appeal_legacy(token_generation_t* token, int beast, int number)
{
	if( number != 0 ){
		return;
	}
	int legacy = create_targetted_legacy_effect(token->s_player, token->s_card, &elemental_appeal_legacy, token->t_player, beast);
	if( current_phase == PHASE_DISCARD ){
		get_card_instance(token->s_player, legacy)->targets[1].player = 1;
	}
	if( kicked(token->s_player, token->s_card) ){
		get_card_instance(token->s_player, legacy)->targets[2].player = 7;
	}
}

int card_elemental_appeal(int player, int card, event_t event){
/* CARD_ID_ELEMENTAL_APPEAL	10533
Elemental Appeal	|R|R|R|R
Sorcery
Kicker |5 (You may pay an additional |5 as you cast this spell.)
Create a 7/1 |Sred Elemental creature token with trample and haste. Exile it at the beginning of the next end step.
If ~ was kicked, that creature gets +7/+0 until end of turn. */

	kicker(player, card, event, MANACOST_X(5));

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
		token.pow = 7;
		token.tou = 1;
		token.key_plus = KEYWORD_TRAMPLE;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		token.special_code_on_generation = &create_elemental_appeal_legacy;
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_geyser_glider(int player, int card, event_t event){
/* CARD_ID_GEYSER_GLIDER	10543
Geyser Glider	|3|R|R
Creature - Elemental Beast 4/4
Landfall - Whenever a land enters the battlefield under your control, ~ gains flying until end of turn. */

	landfall(player, card, event);
	if( event == EVENT_ETB_ABILITY ){
		pump_ability_until_eot(player, card, player, card, 0, 0, KEYWORD_FLYING, 0);
	}
	return 0;
}

int card_goblin_bushwacker(int player, int card, event_t event){
/* CARD_ID_GOBLIN_BUSHWHACKER	10546
Goblin Bushwhacker	|R
Creature - Goblin Warrior 1/1
Kicker |R (You may pay an additional |R as you cast this spell.)
When ~ enters the battlefield, if it was kicked, creatures you control get +1/+0 and gain haste until end of turn. */

	kicker(player, card, event, MANACOST_R(1));
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		pump_subtype_until_eot(player, card, player, -1, 1, 0, 0, SP_KEYWORD_HASTE);
	}
	return 0;
}

int card_goblin_ruinblaster(int player, int card, event_t event){
/* CARD_ID_GOBLIN_RUINBLASTER	10548
Goblin Ruinblaster	|2|R
Creature - Goblin Shaman 2/1
Kicker |R (You may pay an additional |R as you cast this spell.)
Haste
When ~ enters the battlefield, if it was kicked, destroy target nonbasic land. */

	kicker(player, card, event, MANACOST_R(1));
	haste(player, card, event);
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( pick_target_nonbasic_land(player, card, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

static int effect_goblin_shortcutter(int player, int card, event_t event ){
	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		cannot_block(p, c, event);
	}
	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_goblin_shortcutter(int player, int card, event_t event){
/* CARD_ID_GOBLIN_SHORTCUTTER	10549
Goblin Shortcutter	|1|R
Creature - Goblin Scout 2/1
When ~ enters the battlefield, target creature can't block this turn. */

	// Used externally
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.allow_cancel = 0;
		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			create_targetted_legacy_effect(player, card, &effect_goblin_shortcutter, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_goblin_war_paint(int player, int card, event_t event){
/* CARD_ID_GOBLIN_WAR_PAINT	10550
Goblin War Paint	|1|R
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2 and has haste. */

	return generic_aura(player, card, event, player, 2, 2, 0, SP_KEYWORD_HASTE, 0, 0, 0);
}

int card_goblin_guide(int player, int card, event_t event)
{
/* CARD_ID_GOBLIN_GUIDE	10547
Goblin Guide	|R
Creature - Goblin Scout 2/2
Haste
Whenever ~ attacks, defending player reveals the top card of his or her library. If it's a land card, that player puts it into his or her hand. */

  haste(player, card, event);

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  int iid = deck_ptr[1-player][0];
	  reveal_card_iid(player, card, iid);
	  if (iid != -1 && is_what(-1, iid, TYPE_LAND))
		{
		  add_card_to_hand(1-player, iid);
		  obliterate_top_card_of_deck(1-player);
		}
	}

  return 0;
}

int card_hellfire_mongrel(int player, int card, event_t event){
/* CARD_ID_HELLFIRE_MONGREL	10565
Hellfire Mongrel	|2|R
Creature - Elemental Hound 2/2
At the beginning of each opponent's upkeep, if that player has two or fewer cards in hand, ~ deals 2 damage to him or her. */

	upkeep_trigger_ability(player, card, event, ANYBODY);
	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( hand_count[current_turn] < 3 ){
			damage_player(current_turn, 2, player, card);
		}
	}
	return 0;
}

int card_highland_berserker(int player, int card, event_t event){
/* CARD_ID_HIGHLAND_BERSERKER	10568
Highland Berserker	|1|R
Creature - Human Berserker Ally 2/1
Whenever ~ or another Ally enters the battlefield under your control, you may have Ally creatures you control gain first strike until end of turn.*/

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		pump_subtype_until_eot(player, card, player, SUBTYPE_ALLY, 0, 0, KEYWORD_FIRST_STRIKE, 0 );
	}
	return 0;
}

int card_inferno_trap(int player, int card, event_t event){
/* CARD_ID_INFERNO_TRAP	10569
Inferno Trap	|3|R
Instant - Trap
If you've been dealt damage by two or more creatures this turn, you may pay |R rather than pay ~'s mana cost.
~ deals 4 damage to target creature. */

	if( event == EVENT_MODIFY_COST && get_trap_condition(1-player, TRAP_UNBLOCKED_CREATURES) >= 2 && current_phase >= PHASE_MAIN2 ){
		COST_COLORLESS -= 3;
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_kazuul_warlord(int player, int card, event_t event){
/* CARD_ID_KAZUUL_WARLORD	10581
Kazuul Warlord	|4|R
Creature - Minotaur Warrior Ally 3/3
Whenever ~ or another Ally enters the battlefield under your control, you may put a +1/+1 counter on each Ally creature you control. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		int count = 0;
		while(count < active_cards_count[player]){
			card_data_t* card_d = get_card_data(player, count);
			if((card_d->type & TYPE_CREATURE) && in_play(player, count) && has_subtype(player, count, SUBTYPE_ALLY) ){
				add_1_1_counter(player, count );
			}
			count++;
		}
	}
	return 0;
}

int card_lavaball_trap(int player, int card, event_t event){
/*
Lavaball Trap |6|R|R
Instant — Trap
If an opponent had two or more lands enter the battlefield under his or her control this turn, you may pay {3}{R}{R} rather than
	pay Lavaball Trap's mana cost.
Destroy two target lands. Lavaball Trap deals 4 damage to each creature.
*/
	if( event == EVENT_MODIFY_COST && get_trap_condition(1-player, TRAP_LANDS_PLAYED) >= 2 ){
		COST_COLORLESS -= 3;
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int valid = 0;
		if( validate_target(player, card, &td, 0 )){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			valid = 1;
		}
		if( validate_target(player, card, &td, 1 )){
			kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
			valid = 1;
		}
		if( valid == 1 ){
			new_damage_all(player, card, ANYBODY, 4, NDA_ALL_CREATURES, NULL);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 2, NULL);
}

int card_magma_rift(int player, int card, event_t event){
/* CARD_ID_MAGMA_RIFT	10600
Magma Rift	|2|R
Sorcery
As an additional cost to cast ~, sacrifice a land.
~ deals 5 damage to target creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

	if( event == EVENT_CAN_CAST ){
		if( new_can_sacrifice_as_cost(player, card, &test) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled == 1 ){
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
			cancel = 1;
			return 0;
		}
		kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 5);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_mark_of_mutiny(int player, int card, event_t event){
/* CARD_ID_MARK_OF_MUTINY	10604
Mark of Mutiny	|2|R
Sorcery
Gain control of target creature until end of turn. Put a +1/+1 counter on it and untap it.
That creature gains haste until end of turn. (It can attack and |T this turn.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
			effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return aot_like_spell(player, card, event, &td, "TARGET_CREATURE", 0);
}

int card_mire_blight(int player, int card, event_t event){
/* CARD_ID_MIRE_BLIGHT	10611
Mire Blight	|B
Enchantment - Aura
Enchant creature
When enchanted creature is dealt damage, destroy it. */

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( damage_dealt_to_me_arbitrary(instance->damage_target_player, instance->damage_target_card, event, 0, player, card) ){
			kill_card(instance->damage_target_player, instance->damage_target_card, KILL_DESTROY) ;
		}
	}
	return disabling_aura(player, card, event);
}

int card_murasa_pyromancer(int player, int card, event_t event){
/* CARD_ID_MURASA_PYROMANCER	10615
Murasa Pyromancer	|4|R|R
Creature - Human Shaman Ally 3/2
Whenever ~ or another Ally enters the battlefield under your control, you may have ~ deal damage to target creature equal to the number of Allies you control. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, count_allies(player, card), player, card);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

static int effect_obsidian_fireheart(int player, int card, event_t event)
{
  card_instance_t* instance = get_card_instance(player, card);
  int p = instance->damage_target_player;
  int c = instance->damage_target_card;
  if (p > -1)
	{
	  upkeep_trigger_ability(player, card, event, p);
	  if (event == EVENT_UPKEEP_TRIGGER_ABILITY || event == EVENT_SHOULD_AI_PLAY)
		damage_player(p, 1, p, c);

	  if (event == EVENT_STATIC_EFFECTS && !count_counters(p, c, COUNTER_BLAZE))
		kill_card(player, card, KILL_EXILE);
	}
  else if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

static const char* doesnt_have_a_blaze_counter(int who_chooses, int player, int card){
	return count_counters(player, card, COUNTER_BLAZE) ? "has a blaze counter" : NULL;
}

int card_obsidian_fireheart(int player, int card, event_t event){

	/* Obsidian Fireheart	|1|R|R|R
	 * Creature - Elemental 4/4
	 * |1|R|R: Put a blaze counter on target land without a blaze counter on it. For as long as that land has a blaze counter on it, it has "At the beginning of
	 * your upkeep, this land deals 1 damage to you." */

	if (!IS_GAA_EVENT(event)){
	  return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );
	td.extra = (int32_t)doesnt_have_a_blaze_counter;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		if( current_turn == 1-player ){
			ai_modifier += 24;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_BLAZE);
			create_targetted_legacy_effect(player, card, &effect_obsidian_fireheart, instance->targets[0].player, instance->targets[0].card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 2), 0, &td, "TARGET_LAND");
}

int card_punishing_fire(int player, int card, event_t event){
	/* Punishing Fire	|1|R	0x2001b64
	 * Instant
	 * ~ deals 2 damage to any target.
	 * Whenever an opponent gains life, you may pay |R. If you do, return ~ from your graveyard to your hand. */
	return card_shock2(player, card, event);
}

int card_pyromancer_ascension(int player, int card, event_t event){

	/* Pyromancer Ascension	|1|R
	 * Enchantment
	 * Whenever you cast an instant or sorcery spell that has the same name as a card in your graveyard, you may put a quest counter on ~.
	 * Whenever you cast an instant or sorcery spell while ~ has two or more quest counters on it, you may copy that spell. You may choose new targets for the
	 * copy. */

	 // was an instant or sorcery cast?
	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && ! is_humiliated(player, card)
		 && player == reason_for_trigger_controller && player == trigger_cause_controller
		 && ! check_special_flags(trigger_cause_controller, trigger_cause, SF_NOT_CAST) )
	{
		int p = trigger_cause_controller;
		int c = trigger_cause;

		if (is_what(p, c, TYPE_SPELL)){
			card_instance_t *instance = get_card_instance(p, c);
			card_instance_t *me = get_card_instance(player, card);

			if(event == EVENT_TRIGGER){
				if( count_counters(player, card, COUNTER_QUEST) < 2 ){
					// is this spell the same as one in the graveyard?
					const int *grave = get_grave(player);
					int i;
					for(i=0;i<count_graveyard(player);i++){
						if( grave[i] == instance->internal_card_id ){
							event_result |= RESOLVE_TRIGGER_AI(player);
							me->info_slot = -2;
							break;
						}
					}
				}
				else{
					me->info_slot = instance->internal_card_id;
					event_result |= RESOLVE_TRIGGER_AI(player);
				}
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
				if( me->info_slot == -2 ){
					add_counter(player, card, COUNTER_QUEST);
				}
				else{
					copy_spell_from_stack(player, p, c);
				}
			}
		}
	}
	return global_enchantment(player, card, event);
}

int card_ruinous_minotaur(int player, int card, event_t event){
/* CARD_ID_RUINOUS_MINOTAUR	10650
Ruinous Minotaur	|1|R|R
Creature - Minotaur Warrior 5/2
Whenever ~ deals damage to an opponent, sacrifice a land. */

	check_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_MUST_DAMAGE_OPPONENT, player, card);
	int result = resolve_damage_trigger(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_MUST_DAMAGE_OPPONENT, player, card);
	if( result ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		test.qty = result;
		new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}
	return 0;
}

int card_runeflare_trap(int player, int card, event_t event){
	/* Runeflare Trap	|4|R|R
	 * Instant - Trap
	 * If an opponent drew three or more cards this turn, you may pay |R rather than pay ~'s mana cost.
	 * ~ deals damage to target player equal to the number of cards in that player's hand. */

	if( event == EVENT_MODIFY_COST && cards_drawn_this_turn[1-player] >= 3 ){
		COST_COLORLESS -= 4;
		COST_RED--;
	}
	return card_storm_seeker(player, card, event);
}

int card_slaughter_cry(int player, int card, event_t event){
	/* Slaughter Cry	|2|R
	 * Instant
	 * Target creature gets +3/+0 and gains first strike until end of turn. */
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 3,0, KEYWORD_FIRST_STRIKE,0);
}

int card_spire_barrage(int player, int card, event_t event){
	/* Spire Barrage	|4|R	0x2001b7d
	 * Sorcery
	 * ~ deals damage to any target equal to the number of |H1Mountains you control. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, count_subtype(player, TYPE_LAND, SUBTYPE_MOUNTAIN));
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_torch_slinger(int player, int card, event_t event){
/* CARD_ID_TORCH_SLINGER	10689
Torch Slinger	|2|R
Creature - Goblin Shaman 2/2
Kicker |1|R (You may pay an additional |1|R as you cast this spell.)
When ~ enters the battlefield, if it was kicked, it deals 2 damage to target creature. */

	kicker(player, card, event, MANACOST_XR(1, 1));
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.allow_cancel = 0;
		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 2, player, card);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_tuktuk_grunts(int player, int card, event_t event){
/* CARD_ID_TUKTUK_GRUNTS	10694
Tuktuk Grunts	|4|R
Creature - Goblin Warrior Ally 2/2
Haste
Whenever ~ or another Ally enters the battlefield under your control, you may put a +1/+1 counter on ~. */

	haste(player, card, event);
	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		add_1_1_counter(player, card);
	}
	return 0;
}

int card_unstable_footing(int player, int card, event_t event){
	/* Unstable Footing	|R	0x2001b8c
	 * Instant
	 * Kicker |3|R
	 * Damage can't be prevented this turn. If this spell was kicked, it deals 5 damage to target player or planeswalker. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->info_slot = instance->number_of_targets = 0;
		if( can_target(&td) && do_kicker(player, card, MANACOST_XR(3, 1)) ){
			generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		damage_cannot_be_prevented_until_eot(player, card);
		if( kicked(player, card) && valid_target(&td) ){
			damage_target0(player, card, 5);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_zektar_shrine_expedition(int player, int card, event_t event){
	/* Zektar Shrine Expedition	|1|R
	 * Enchantment
	 * Landfall - Whenever a land enters the battlefield under your control, you may put a quest counter on ~.
	 * Remove three quest counters from ~ and sacrifice it: Put a 7/1 |Sred Elemental creature token with trample and haste onto the battlefield. Exile it at
	 * the beginning of the next end step. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
		token.pow = 7;
		token.tou = 1;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		token.key_plus = KEYWORD_TRAMPLE;
		token.special_code_on_generation = &haste_and_remove_token_at_end_of_next_turn;
		generate_token(&token);
	}
	return expedition(player, card, event);
}

// white
int card_armament_master(int player, int card, event_t event){
/* CARD_ID_ARMAMENT_MASTER	10497
Armament Master	|W|W
Creature - Kor Soldier 2/2
Other Kor creatures you control get +2/+2 for each Equipment attached to ~. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && ! is_humiliated(player, card) ){
		if( ! affect_me(player, card) && affected_card_controller == player && has_creature_type( affected_card_controller, affected_card, SUBTYPE_KOR) ){
			event_result += (equipments_attached_to_me(player, card, 0) * EATM_REPORT_TOTAL);
		}
	}

	return 0;
}

int card_arrow_volley_trap(int player, int card, event_t event){

	/* Arrow Volley Trap	|3|W|W
	 * Instant - Trap
	 * If four or more creatures are attacking, you may pay |1|W rather than pay ~'s mana cost.
	 * ~ deals 5 damage divided as you choose among any number of target attacking creatures. */

	if( event == EVENT_MODIFY_COST && current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2 && count_attackers(current_turn) >= 4 ){
		COST_COLORLESS -= 2;
		COST_WHITE--;
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_state = TARGET_STATE_ATTACKING;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		char buffer[100];
		int i;
		for (i = 0; i < 5; ++i){
			scnprintf(buffer, 100, "Select target for damage %d", i+1 );
			if (!select_target(player, card, &td, buffer, &instance->targets[i])){
				cancel = 1;
				break;
			}
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		divide_damage(player, card, &td);
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_bold_defense(int player, int card, event_t event){
/* CARD_ID_BOLD_DEFENSE	10513
Bold Defense	|2|W
Instant
Kicker |3|W (You may pay an additional |3|W as you cast this spell.)
Creatures you control get +1/+1 until end of turn. If ~ was kicked, instead creatures you control get +2/+2 and gain first strike until end of turn.*/

	kicker(player, card, event, MANACOST_XW(3, 1));

	if( event == EVENT_RESOLVE_SPELL ){
		int p = player;
		int count = 0;
		while(count < active_cards_count[p]){
			card_data_t* card_d = get_card_data(p, count);
			if((card_d->type & TYPE_CREATURE) && in_play(p, count)){
				if( kicked(player, card) ){
					pump_ability_until_eot(player, card, p, count, 2, 2, KEYWORD_FIRST_STRIKE, 0 );
				}
				else{
					pump_until_eot(player, card, p, count, 1, 1 );
				}
			}
			count++;
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

int card_brave_the_elements(int player, int card, event_t event){

	/* Brave the Elements	|W
	 * Instant
	 * Choose a color. |SWhite creatures you control gain protection from the chosen color until end of turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		int keyword = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
		int white = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		int count = active_cards_count[player] - 1;
		while (count > -1){
				if (in_play(player, count) && is_what(player, count, TYPE_CREATURE) && (get_color(player, count) & white)){
					pump_ability_until_eot(player, card, player, count, 0, 0, keyword, 0);
				}
				count--;
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

int card_celestial_mantle(int player, int card, event_t event)
{
  /* Celestial Mantle	|3|W|W|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +3/+3.
   * Whenever enchanted creature deals combat damage to a player, double its controller's life total. */

  int packets;
  if ((packets = attached_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER)))
	{
	  int p = get_card_instance(player, card)->damage_target_player;
	  for (; packets > 0; --packets)
		gain_life(p, life[p]);
	}

  return generic_aura(player, card, event, player, 3, 3, 0, 0, 0, 0, 0);
}

int card_conquerors_pledge(int player, int card, event_t event){
	/* Conqueror's Pledge	|2|W|W|W
	 * Sorcery
	 * Kicker |6
	 * Put six 1/1 |Swhite Kor Soldier creature tokens onto the battlefield. If ~ was kicked, put twelve of those tokens onto the battlefield instead. */

	kicker(player, card, event, MANACOST_X(6));

	if(event == EVENT_RESOLVE_SPELL){
		int total = 6;
		if( kicked(player, card) ){
			total = 12;
		}
		generate_tokens_by_id(player, card, CARD_ID_KOR_SOLDIER, total);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_day_of_judgment(int player, int card, event_t event){
	// Used externally
/* CARD_ID_DAY_OF_JUDGMENT	10527
Day of Judgment	|2|W|W
Sorcery
Destroy all creatures. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		APNAP(p, { new_manipulate_all(player, card, p, &test, KILL_DESTROY); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_devout_lightcaster(int player, int card, event_t event){
/* CARD_ID_DEVOUT_LIGHTCASTER	10529
Devout Lightcaster	|W|W|W
Creature - Kor Cleric 2/2
Protection from |Sblack
When ~ enters the battlefield, exile target |Sblack permanent. */

	pro_black(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT );
		td.allow_cancel = 0;
		td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) &&
			new_pick_target(&td, get_sleighted_color_text(player, card, "Select target %s permanent.", COLOR_BLACK), 0, GS_LITERAL_PROMPT) )
		{
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE );
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_emeria_angel (int player, int card, event_t event)
{
  /* Emeria Angel	|2|W|W
   * Creature - Angel 3/3
   * Flying
   * Landfall - Whenever a land enters the battlefield under your control, you may put a 1/1 |Swhite Bird creature token with flying onto the battlefield. */
	landfall_mode(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY )
		generate_token_by_id(player, card, CARD_ID_BIRD);

  return 0;
}

int card_felidar_sovereign (int player, int card, event_t event){

	/* Felidar Sovereign	|4|W|W
	 * Creature - Cat Beast 4/6
	 * Vigilance, lifelink
	 * At the beginning of your upkeep, if you have 40 or more life, you win the game. */

	lifelink(player, card, event);

	vigilance(player, card, event);

	if( current_turn==player && life[player] >= 40 && upkeep_trigger(player, card, event) ){
		lose_the_game(1-player);
	}

	if (event == EVENT_SHOULD_AI_PLAY){
		int l = life[player] - 20;
		if (l > 0){
			ai_modifier += (player == AI ? 1 : -1) * life[player] * life[player] * life[player];
		}
		if (life[player] >= 40){
			lose_the_game(1-player);
		}
	}

	return 0;
}

int card_iona_shield_of_emeria(int player, int card, event_t event){
/* CARD_ID_IONA_SHIELD_OF_EMERIA	10571
Iona, Shield of Emeria	|6|W|W|W
Legendary Creature - Angel 7/7
Flying
As ~ enters the battlefield, choose a color.
Your opponents can't cast spells of the chosen color. */

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->targets[1].card = 1<<choose_a_color_and_show_legacy(player, card, 1-player, -1);
	}

	if(event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == 1-player && ! is_humiliated(player, card) &&
		get_card_instance(player, card)->targets[1].card > 0 )
	{
		int clr = get_color(affected_card_controller, affected_card);
		card_instance_t *instance2 = get_card_instance( affected_card_controller, affected_card);
		if ((clr & get_card_instance(player, card)->targets[1].card) && !(cards_data[instance2->internal_card_id].type & TYPE_LAND)){
			infinite_casting_cost();
		}
	}
	return 0;
}

int card_journey_to_nowhere(int player, int card, event_t event){

	/* Journey to Nowhere	|1|W
	 * Enchantment
	 * When ~ enters the battlefield, exile target creature.
	 * When ~ leaves the battlefield, return the exiled card to the battlefield under its owner's control. */
	// Used externally
	return_from_oblivion(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;
		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			if (player == AI && instance->targets[0].player == AI){
				ai_modifier -= 64;
			}
			obliviation(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		else if (player == AI) {
			ai_modifier -= 48;
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_kabira_evangel(int player, int card, event_t event){
/* CARD_ID_KABIRA_EVANGEL	10577
Kabira Evangel	|2|W
Creature - Human Cleric Ally 2/3
Whenever ~ or another Ally enters the battlefield under your control, you may choose a color.
If you do, Allies you control gain protection from the chosen color until end of turn. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		int keyword = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
		pump_subtype_until_eot(player, card, player, SUBTYPE_ALLY, 0, 0, keyword, 0);
	}
	return 0;
}

int card_kazandu_blademaster(int player, int card, event_t event){
/* CARD_ID_KAZANDU_BLADEMASTER	10579
Kazandu Blademaster	|W|W
Creature - Human Soldier Ally 1/1
First strike, vigilance
Whenever ~ or another Ally enters the battlefield under your control, you may put a +1/+1 counter on ~. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		add_1_1_counter(player, card);
	}
	vigilance(player, card, event);
	return 0;
}

int card_kor_aeronaut(int player, int card, event_t event){
/* CARD_ID_KOR_AERONAUT	10584
Kor Aeronaut	|W|W
Creature - Kor Soldier 2/2
Kicker |1|W (You may pay an additional |1|W as you cast this spell.)
Flying
When ~ enters the battlefield, if it was kicked, target creature gains flying until end of turn. */

	kicker(player, card, event, MANACOST_XW(1, 1));
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;
		card_instance_t *instance = get_card_instance( player, card );
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
									0, 0, KEYWORD_FLYING, 0);
		}
	}
	return 0;
}

int card_kor_cartographer(int player, int card, event_t event){
/* CARD_ID_KOR_CARTOGRAPHER	10585
Kor Cartographer	|3|W
Creature - Kor Scout 2/2
When ~ enters the battlefield, you may search your library for |Ha Plains card, put it onto the battlefield tapped, then shuffle your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.subtype = get_hacked_subtype(player, card, SUBTYPE_PLAINS);
		scnprintf(test.message, 100, "%s", get_hacked_land_text(player, card, "Select a %s card.", SUBTYPE_PLAINS));
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY_TAPPED, 0, AI_MAX_VALUE, &test);
	}
	return 0;
}

int card_kor_hookmaster(int player, int card, event_t event){
/* CARD_ID_KOR_HOOKMASTER	10587
Kor Hookmaster	|2|W
Creature - Kor Soldier 2/2
When ~ enters the battlefield, tap target creature an opponent controls. That creature doesn't untap during its controller's next untap step. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.allowed_controller = 1-player;
		td1.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td1) ){
			if( pick_target(&td1, "TARGET_CREATURE") ){
				effect_frost_titan(player, card, instance->targets[0].player, instance->targets[0].card);
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

static void kor_outfitter_effect(int player, int card ){
	 target_definition_t td;
	 default_target_definition(player, card, &td, TYPE_ARTIFACT);
	 td.allowed_controller = player;
	 td.preferred_controller = player;
	 td.required_subtype = SUBTYPE_EQUIPMENT;
	 //td.allow_cancel = 0;

	 target_definition_t td1;
	 default_target_definition(player, card, &td1, TYPE_CREATURE);
	 td1.allowed_controller = player;
	 td1.preferred_controller = player;

	 card_instance_t *instance = get_card_instance( player, card );

	 if( can_target(&td) && can_target(&td1) ){
		int eq_controller = -1;
		int eq_card = -1;
		int eq_creature_controller = -1;
		int eq_creature = -1;

		if( select_target(player, card, &td, "Select target Equipment you control.", NULL) ){
			eq_controller = instance->targets[0].player;
			eq_card = instance->targets[0].card;
			instance->number_of_targets = 1;

			if( select_target(player, card, &td1, "Select target creature you control.", NULL) ){
				eq_creature_controller = instance->targets[0].player;
				eq_creature = instance->targets[0].card;
			}
		}

		if( eq_controller != -1 && eq_card != -1 && eq_creature_controller != -1 && eq_creature != -1 ){
			equip_target_creature( eq_controller, eq_card, eq_creature_controller, eq_creature);
		}
	}
	instance->number_of_targets = 0;
}

int card_kor_outfitter(int player, int card, event_t event){
/* CARD_ID_KOR_OUTFITTER	10588
Kor Outfitter	|W|W
Creature - Kor Soldier 2/2
When ~ enters the battlefield, you may attach target Equipment you control to target creature you control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		kor_outfitter_effect(player, card);
	}

	return 0;
}

int card_kor_sanctifiers(int player, int card, event_t event){
/* CARD_ID_KOR_SANCTIFIERS	10589
Kor Sanctifiers	|2|W
Creature - Kor Cleric 2/3
Kicker |W (You may pay an additional |W as you cast this spell.)
When ~ enters the battlefield, if it was kicked, destroy target artifact or enchantment. */

	kicker(player, card, event, MANACOST_W(1));
	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);
		td.allow_cancel = 0;
		card_instance_t *instance = get_card_instance( player, card );
		if( can_target(&td) && pick_target(&td, "DISENCHANT") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_kor_skyfisher(int player, int card, event_t event ){
/* CARD_ID_KOR_SKYFISHER	10590
Kor Skyfisher	|1|W
Creature - Kor Soldier 2/3
Flying
When ~ enters the battlefield, return a permanent you control to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allowed_controller = td.preferred_controller = player;
		td.allow_cancel = 0;
		td.illegal_abilities = 0;

		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && new_pick_target(&td, "Select a permanent you control to bounce.", 0, GS_LITERAL_PROMPT) ){
			bounce_permanent( instance->targets[0].player, instance->targets[0].card );
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_landbind_ritual(int player, int card, event_t event){
/* CARD_ID_LANDBIND_RITUAL	10592
Landbind Ritual	|3|W|W
Sorcery
You gain 2 life for each |H2Plains you control. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( life[player] < 6  ){
			ai_modifier += (2*count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_PLAINS))*8);
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, 2*count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_PLAINS)) );
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

int card_luminarch_ascension(int player, int card, event_t event){
/* Luminarch Ascension
 * |1|W
 * Enchantment
 * At the beginning of each opponent's end step, if you didn't lose life this turn, you may put a quest counter on ~.
 * |1|W: Put a 4/4 |Swhite Angel creature token with flying onto the battlefield. Activate this ability only if ~ has four or more quest counters on it. */

	if( trigger_condition == TRIGGER_EOT && affect_me(player, card) && current_turn != player ){
		if( get_trap_condition(current_turn, TRAP_LIFE_LOST) && eot_trigger(player, card, event) ){
			add_counter(player, card, COUNTER_QUEST);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( count_counters(player, card, COUNTER_QUEST) < 4 ){
			return 0;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_ANGEL, &token);
		token.pow = token.tou = 4;
		token.key_plus = KEYWORD_FLYING;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XW(1, 1), 0, NULL, NULL);
}

int card_makindi_shieldmate(int player, int card, event_t event){
/* CARD_ID_MAKINDI_SHIELDMATE	10602
Makindi Shieldmate	|2|W
Creature - Kor Soldier Ally 0/3
Defender
Whenever ~ or another Ally enters the battlefield under your control, you may put a +1/+1 counter on ~. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		add_1_1_counter(player, card);
	}
	return 0;
}

int card_narrow_escape(int player, int card, event_t event){

	/* Narrow Escape	|2|W
	 * Instant
	 * Return target permanent you control to its owner's hand. You gain 4 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = td.preferred_controller = player;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			gain_life(player, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT_YOU_CONTROL", 1, NULL);
}

int card_nimbus_wings(int player, int card, event_t event){
/* CARD_ID_NIMBUS_WINGS	10619
Nimbus Wings	|1|W
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+2 and has flying. */

	return generic_aura(player, card, event, player, 1, 2, KEYWORD_FLYING, 0, 0, 0, 0);
}

int card_ondu_cleric(int player, int card, event_t event){
/* CARD_ID_ONDU_CLERIC	10625
Ondu Cleric	|1|W
Creature - Kor Cleric Ally 1/1
Whenever ~ or another Ally enters the battlefield under your control, you may gain life equal to the number of Allies you control. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, count_allies(player, card) );
	}
	return 0;
}

int card_pitfall_trap(int player, int card, event_t event){

	/* Pitfall Trap	|2|W
	 * Instant - Trap
	 * If exactly one creature is attacking, you may pay |W rather than pay ~'s mana cost.
	 * Destroy target attacking creature without flying. */

	if( event == EVENT_MODIFY_COST && current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2 && count_attackers(current_turn) == 1 ){
		COST_COLORLESS -=2 ;
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t* instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_state = TARGET_STATE_ATTACKING;
	td.illegal_abilities |= KEYWORD_FLYING;

	if( event == EVENT_RESOLVE_SPELL ){
		if (valid_target(&td)){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY );
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target attacking creature without flying.", 1, NULL);
}

int card_quest_for_the_holy_relic(int player, int card, event_t event){

	/* Quest for the Holy Relic	|W
	 * Enchantment
	 * Whenever you cast a creature spell, you may put a quest counter on ~.
	 * Remove five quest counters from ~ and sacrifice it: Search your library for an Equipment card, put it onto the battlefield, and attach it to a creature
	 * you control. Then shuffle your library. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		add_counter(player, card, COUNTER_QUEST);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, 0, "Select an Equipment card.");
		this_test.subtype = SUBTYPE_EQUIPMENT;

		int equipment = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		if( equipment != -1 ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.allowed_controller = player;
			td.preferred_controller = player;
			td.illegal_abilities = 0;

			if( pick_target(&td, "TARGET_CREATURE") ){
				card_instance_t* instance = get_card_instance(player, card);
				instance->number_of_targets = 0;
				equip_target_creature(player, equipment, instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return quest(player, card, event, 4, NULL, 0);
}

int card_shieldmates_blessing(int player, int card, event_t event){
	/* Shieldmate's Blessing	|W	0x2001c13
	 * Instant
	 * Prevent the next 3 damage that would be dealt to any target this turn. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, "TARGET_DAMAGE", 1, NULL);
}

int card_steppe_lynx(int player, int card, event_t event){
/* CARD_ID_STEPPE_LYNX	10676
Steppe Lynx	|W
Creature - Cat 0/1
Landfall - Whenever a land enters the battlefield under your control, ~ gets +2/+2 until end of turn. */

	landfall(player, card, event );
	if( event == EVENT_ETB_ABILITY ){
		pump_until_eot_merge_previous(player, card, player, card, 2, 2);
	}
	return 0;
}

int card_sunspring_expedition(int player, int card, event_t event){
	/* Sunspring Expedition	|W
	 * Enchantment
	 * Landfall - Whenever a land enters the battlefield under your control, you may put a quest counter on ~.
	 * Remove three quest counters from ~ and sacrifice it: You gain 8 life. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 8);
	}
	return expedition(player, card, event);
}

int card_windborne_charge(int player, int card, event_t event){
/* CARD_ID_WINDBORNE_CHARGE	10711
Windborne Charge	|2|W|W
Sorcery
Two target creatures you control each get +2/+2 and gain flying until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0;i<2;i++){
			if( validate_target(player, card, &td, i) ){
				pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 2, 2, KEYWORD_FLYING, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

int card_world_queller(int player, int card, event_t event){
/* CARD_ID_WORLD_QUELLER	10713
World Queller	|3|W|W
Creature - Avatar 4/4
At the beginning of your upkeep, you may choose a card type. If you do, each player sacrifices a permanent of that type. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if (event == EVENT_UPKEEP_TRIGGER_ABILITY){
		if (!in_play(player, card)){	// sacrificed self on a previous upkeep?
			return -1;
		}
		int available_types[6] = { TYPE_CREATURE, TYPE_ARTIFACT, TYPE_LAND, TYPE_ENCHANTMENT, TARGET_TYPE_PLANESWALKER, -1 };

		int ai_choice = 5;
		int k;
		for (k = 0; k <= 4; ++k){
			if (count_permanents_by_type(player, available_types[k]) == 0
				&& count_permanents_by_type(1-player, available_types[k]) > 0 ){
				ai_choice = k;
				break;
			}
		}

		int choice = do_dialog(player, player, card, -1, -1, " Creature\n Artifact\n Land\n Enchantment\n Planeswalker\n Cancel", ai_choice);
		if( choice == 5 ){ return -1; }

		int type = available_types[choice];
		test_definition_t test;
		default_test_definition(&test, type);
		char msg[100];
		scnprintf(test.message, 100, "Select a %s to sacrifice.", type_text(msg, 100, type, 0));
		APNAP(p, { new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test); };);
	}
	return 0;
}

// blue
int card_aether_figment(int player, int card, event_t event){
/* CARD_ID_AETHER_FIGMENT	10499
Aether Figment	|1|U
Creature - Illusion 1/1
Kicker |3 (You may pay an additional |3 as you cast this spell.)
~ can't be blocked.
If ~ was kicked, it enters the battlefield with two +1/+1 counters on it. */

	kicker(player, card, event, MANACOST_X(3));
	unblockable(player, card, event);
	if( kicked(player, card) ){
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);
	}
	return 0;
}

int card_archive_trap(int player, int card, event_t event){
/* CARD_ID_ARCHIVE_TRAP	10494
Archive Trap	|3|U|U
Instant - Trap
If an opponent searched his or her library this turn, you may pay |0 rather than pay ~'s mana cost.
Target opponent puts the top thirteen cards of his or her library into his or her graveyard. */

	if( event == EVENT_MODIFY_COST ){
		if( get_trap_condition(1-player, TRAP_DECK_WAS_SEARCHED) ==1 ){
			null_casting_cost(player, card);
			get_card_instance(player, card)->info_slot = 1;
		}
		else{
			get_card_instance(player, card)->info_slot = 0;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1){
			int choice = 0;
			if( player != AI ){
				choice = do_dialog(player, player, card, -1, -1, " Play for free\n Pay full cost\n Cancel", 0);
			}
			if( choice == 1 ){
				charge_mana_from_id(player, card, event, get_id(player, card));
			}
			if( choice == 2 ){
				spell_fizzled = 1;
			}
		}
		return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			mill(instance->targets[0].player, 13);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_archmage_ascension(int player, int card, event_t event)
{
  /* Archmage Ascension	|2|U
   * Enchantment
   * At the beginning of each end step, if you drew two or more cards this turn, you may put a quest counter on ~.
   * As long as ~ has six or more quest counters on it, if you would draw a card, you may instead search your library for a card, put that card into your hand,
   * then shuffle your library. */

  if (trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && reason_for_trigger_controller == player && !suppress_draw
	  && count_counters(player, card, COUNTER_QUEST) >= 6)
	{
	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_AI(player);

	  if (event == EVENT_RESOLVE_TRIGGER)
		{
		  test_definition_t test;
		  default_test_definition(&test, 0);
		  new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 1, AI_MAX_VALUE, &test);
		  suppress_draw = 1;
		}
	}

  if (trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player
	  && cards_drawn_this_turn[player] >= 2 && count_counters(player, card, COUNTER_QUEST) < 150
	  && !is_humiliated(player, card)
	 ){
	  if (event == EVENT_TRIGGER){
		  event_result |= RESOLVE_TRIGGER_AI(player);
	  }
	  if (event == EVENT_RESOLVE_TRIGGER){
		  add_counter(player, card, COUNTER_QUEST);
	  }
  }

  return global_enchantment(player, card, event);
}

int card_caller_of_gales(int player, int card, event_t event){
/* CARD_ID_CALLER_OF_GALES	10516
Caller of Gales	|U
Creature - Merfolk Wizard 1/1
|1|U, |T: Target creature gains flying until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
								0, 0, KEYWORD_FLYING, 0);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XU(1, 1), 0, &td, "TARGET_CREATURE");
}

int card_cosis_trickster(int player, int card, event_t event){
/* CARD_ID_COSIS_TRICKSTER	10524
Cosi's Trickster	|U
Creature - Merfolk Wizard 1/1
Whenever an opponent shuffles his or her library, you may put a +1/+1 counter on ~. */

	return 0;
}

static void put_on_top_of_owners_deck(int player, int card, int t_player, int t_card)
{
	if( in_play(t_player, t_card) )
		put_on_top_of_deck(t_player, t_card);
}

int card_gomazoa(int player, int card, event_t event){
/* CARD_ID_GOMAZOA	10551
Gomazoa	|2|U
Creature - Jellyfish 0/3
Defender, flying
|T: Put ~ and each creature it's blocking on top of their owners' libraries, then those players shuffle their libraries. */

	if( event == EVENT_ACTIVATE ){
		get_card_instance(player, card)->info_slot = check_state(player, card, STATE_BLOCKING);
		if( current_phase != PHASE_AFTER_BLOCKING || check_state(player, card, STATE_BLOCKING) ){
			ai_modifier -= 96;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->info_slot == 1 ){
			for_each_creature_blocked_by_me(instance->parent_controller, instance->parent_card, put_on_top_of_owners_deck,
											instance->parent_controller, instance->parent_card);
			shuffle(1-player);
		}
		if( in_play(instance->parent_controller, instance->parent_card) ){
			shuffle_into_library(instance->parent_controller, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_hedron_crab(int player, int card, event_t event){
/* CARD_ID_HEDRON_CRAB	10563
Hedron Crab	|U
Creature - Crab 0/2
Landfall - Whenever a land enters the battlefield under your control, target player puts the top three cards of his or her library into his or her graveyard.*/

	landfall(player, card, event );
	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;
		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			mill( instance->targets[0].player, 3);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_into_the_roil(int player, int card, event_t event){
/* CARD_ID_INTO_THE_ROIL	10570
Into the Roil	|1|U
Instant
Kicker |1|U (You may pay an additional |1|U as you cast this spell.)
Return target nonland permanent to its owner's hand. If ~ was kicked, draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );
	td.illegal_type = TYPE_LAND;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nonland permanent", 1, NULL);
		if( spell_fizzled != 1 ){
			do_kicker(player, card, MANACOST_XU(1, 1));
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			bounce_permanent( instance->targets[0].player, instance->targets[0].card);
			if( kicked(player, card) ){
				draw_a_card(player);
			}
		}
		kill_card(player, card, KILL_DESTROY );
	}
	return 0;
}

int card_ior_ruin_expedition(int player, int card, event_t event){
	/* Ior Ruin Expedition	|1|U
	 * Enchantment
	 * Landfall - Whenever a land enters the battlefield under your control, you may put a quest counter on ~.
	 * Remove three quest counters from ~ and sacrifice it: Draw two cards. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 2);
	}
	return expedition(player, card, event);
}

int card_lethargy_trap(int player, int card, event_t event){
/* CARD_ID_LETHARGY_TRAP	10594
Lethargy Trap	|3|U
Instant - Trap
If three or more creatures are attacking, you may pay |U rather than pay ~'s mana cost.
Attacking creatures get -3/-0 until end of turn. */

	 if( event == EVENT_MODIFY_COST && current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2  && current_turn == 1-player){
		int i=0;
		int count = 0;
		int p = 1-player;
		for(i=0;i<active_cards_count[p]; i++){
			card_instance_t *instance2 = get_card_instance(p, i);
			if( instance2->state & STATE_ATTACKING ){
				count++;;
			}
		}
		if( count >= 3 ){
			COST_COLORLESS -=3 ;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.state = STATE_ATTACKING;
		pump_creatures_until_eot(player, card, current_turn, 0, -3, 0, 0, 0, &test);
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_living_tsunami(int player, int card, event_t event){
/* CARD_ID_LIVING_TSUNAMI	10595
Living Tsunami	|2|U|U
Creature - Elemental 4/4
Flying
At the beginning of your upkeep, sacrifice ~ unless you return a land you control to its owner's hand. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.allowed_controller = td.preferred_controller = player;
		td.illegal_abilities = 0;
		card_instance_t *instance= get_card_instance(player, card);
		int kill_me = 1;
		if( can_target(&td) && new_pick_target(&td, "Select a land you control,", 0, GS_LITERAL_PROMPT) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
				kill_me = 0;
		}
		instance->number_of_targets = 0;
		if( kill_me ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}
	return 0;
}

int card_lorthos_the_tidemaker(int player, int card, event_t event)
{
/* CARD_ID_LORTHOS_THE_TIDEMAKER	10596
Lorthos, the Tidemaker	|5|U|U|U
Legendary Creature - Octopus 8/8
Whenever ~ attacks, you may pay |8. If you do, tap up to eight target permanents. Those permanents don't untap during their controllers' next untap steps. */

  check_legend_rule(player, card, event);

  // Whenever ~ attacks, you may pay |8. If you do, tap up to eight target permanents. Those permanents don't untap during their controllers' next untap steps.
  if (xtrigger_condition() == XTRIGGER_ATTACKING || event == EVENT_DECLARE_ATTACKERS)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_PERMANENT);
	  td.allow_cancel = 3;
	  if (player == AI || ai_is_speculating == 1)
		td.allowed_controller = td.preferred_controller = 1-player;

	  int i, trigger_mode;
	  if ((player == AI || ai_is_speculating == 1) && has_mana(player, COLOR_ANY, 8) && can_target(&td))
		trigger_mode = RESOLVE_TRIGGER_MANDATORY;
	  else
		trigger_mode = RESOLVE_TRIGGER_OPTIONAL;

	  card_instance_t* instance = get_card_instance(player, card);

	  if (declare_attackers_trigger(player, card, event, trigger_mode | DAT_STORE_IN_INFO_SLOT, player, card)
		  && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_COLORLESS, 8)
		  && pick_up_to_n_targets(&td, "TARGET_PERMANENT", 8))
		for (i = 0; i < instance->number_of_targets; ++i)
		  does_not_untap_effect(player, card, instance->targets[i].player, instance->targets[i].card, EDNT_TAP_TARGET, 1);
	}

  return 0;
}

int card_merfolk_seastalkers(int player, int card, event_t event){
/* CARD_ID_MERFOLK_SEASTALKERS	10607
Merfolk Seastalkers	|3|U
Creature - Merfolk Scout 2/3
|H2Islandwalk (This creature can't be blocked as long as defending player controls |Han Island.)
|2|U: Tap target creature without flying. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.illegal_abilities |= KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XU(2, 1), 0, &td, "FLOOD");
		if( spell_fizzled != 1 ){
			ai_mod_for_tap_effects(player, card, 0);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XU(2, 1), 0, &td, "FLOOD");
}

int card_merfolk_wayfinder(int player, int card, event_t event){
/* CARD_ID_MERFOLK_WAYFINDER	10608
Merfolk Wayfinder	|2|U
Creature - Merfolk Scout 1/2
Flying
When ~ enters the battlefield, reveal the top three cards of your library.
Put all |H2Island cards revealed this way into your hand and the rest on the bottom of your library in any order. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( event == EVENT_ETB_ABILITY ){
		recruiting(player, card, SUBTYPE_ISLAND, 3);
	}
	return 0;
}

int card_mindbreak_trap(int player, int card, event_t event){
/* CARD_ID_MINDBREAK_TRAP	10609
Mindbreak Trap	|2|U|U
Instant - Trap
If an opponent cast three or more spells this turn, you may pay |0 rather than pay ~'s mana cost.
Exile any number of target spells. */

	if( event == EVENT_MODIFY_COST && get_specific_storm_count(1-player) >= 3 ){
		COST_BLUE -= 2;
		COST_COLORLESS -= 2;
	}
	return card_dissipate(player, card, event);
}

int card_paralyzing_grasp(int player, int card, event_t event){
/* CARD_ID_PARALYZING_GRASP	10630
Paralyzing Grasp	|2|U
Enchantment - Aura
Enchant creature
Enchanted creature doesn't untap during its controller's untap step. */

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);
	}
	return disabling_aura(player, card, event);
}

int card_quest_for_ancient_secrets(int player, int card, event_t event)
{
  /* Quest for Ancient Secrets	|U
   * Enchantment
   * Whenever a card is put into your graveyard from anywhere, you may put a quest counter on ~.
   * Remove five quest counters from ~ and sacrifice it: Target player shuffles his or her graveyard into his or her library. */

  // from library
  enable_xtrigger_flags |= ENABLE_XTRIGGER_MILLED;
  if (xtrigger_condition() == XTRIGGER_MILLED && affect_me(player, card) && reason_for_trigger_controller == player
	  && trigger_cause_controller == player && !is_humiliated(player, card))
	{
	  if (event == EVENT_TRIGGER)
		event_result |= duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;

	  if (event == EVENT_RESOLVE_TRIGGER)
		add_counters(player, card, COUNTER_QUEST, num_cards_milled);
	}

  if (when_a_card_is_put_into_graveyard_from_anywhere_but_library_do_something(player, card, event, player, RESOLVE_TRIGGER_DUH, NULL))
	add_counter(player, card, COUNTER_QUEST);

  if( ! IS_GAA_EVENT(event) ){
	  return global_enchantment(player, card, event);
  }

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	reshuffle_grave_into_deck(get_card_instance(player, card)->targets[0].player, 0);

  return quest(player, card, event, 5, &td, "TARGET_PLAYER");
}

int card_reckless_scholar(int player, int card, event_t event){
/* CARD_ID_RECKLESS_SCHOLAR	10646
Reckless Scholar	|2|U
Creature - Human Wizard 2/1
|T: Target player draws a card, then discards a card. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int p = instance->targets[0].player;
			draw_a_card(p);
			discard(p, 0, player);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_rite_of_replication(int player, int card, event_t event){
	/* Rite of Replication	|2|U|U
	 * Sorcery
	 * Kicker |5
	 * Put a token onto the battlefield that's a copy of target creature. If ~ was kicked, put five of those tokens onto the battlefield instead. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			do_kicker(player, card, MANACOST_X(5));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			token_generation_t token;
			copy_token_definition(player, card, &token, instance->targets[0].player, instance->targets[0].card);
			token.qty = kicked(player, card) ? 5 : 1;
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_roil_elemental(int player, int card, event_t event){
/* CARD_ID_ROIL_ELEMENTAL	10649
Roil Elemental	|3|U|U|U
Creature - Elemental 3/2
Flying
Landfall - Whenever a land enters the battlefield under your control, you may gain control of target creature for as long as you control ~. */

	landfall_mode(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			gain_control_until_source_is_in_play_and_tapped(player, card, instance->targets[0].player, instance->targets[0].card,
															GCUS_CONTROLLED);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_sea_gate_loremaster(int player, int card, event_t event){
/* CARD_ID_SEA_GATE_LOREMASTER	10657
Sea Gate Loremaster	|4|U
Creature - Merfolk Wizard Ally 1/3
|T: Draw a card for each Ally you control. */

	if( event == EVENT_ACTIVATE ){
		int amount = count_subtype(player, TYPE_PERMANENT, SUBTYPE_ALLY);
		int mod = current_turn == 1-player && current_phase >= PHASE_MAIN2 ? 8 : 4;
		ai_modifier += (amount * mod);
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, count_subtype(player, TYPE_PERMANENT, SUBTYPE_ALLY) );
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_seascape_aerialist(int player, int card, event_t event){
/* CARD_ID_SEASCAPE_AERIALIST	10658
Seascape Aerialist	|4|U
Creature - Merfolk Wizard Ally 2/3
Whenever ~ or another Ally enters the battlefield under your control, you may have Ally creatures you control gain flying until end of turn. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		pump_subtype_until_eot(player, card, player, SUBTYPE_ALLY, 0, 0, KEYWORD_FLYING, 0 );
	}
	return 0;
}

int card_shoal_serpent(int player, int card, event_t event){
/* CARD_ID_SHOAL_SERPENT	10664
Shoal Serpent	|5|U
Creature - Serpent 5/5
Defender
Landfall - Whenever a land enters the battlefield under your control, ~ loses defender until end of turn. */

	landfall(player, card, event);
	if( event == EVENT_ETB_ABILITY ){
		int leg = pump_ability_until_eot(player, card, player, card, 0, 0, KEYWORD_DEFENDER, 0);
		if (leg != -1){
			get_card_instance(player, leg)->targets[4].player = 0;	// remove keyword
		}
	}
	return 0;
}

int card_spell_pierce(int player, int card, event_t event)
{
/* CARD_ID_SPELL_PIERCE	10670
Spell Pierce	|U
Instant
Counter target noncreature spell unless its controller pays |2. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);
	td.illegal_type = TYPE_CREATURE;

	if (event == EVENT_RESOLVE_SPELL)
	{
	  counterspell_resolve_unless_pay_x(player, card, NULL, 0, 2);
	  kill_card(player, card, KILL_DESTROY);
	  return 0;
	}

	return counterspell(player, card, event, &td, 0);
}

int card_sphinx_of_jwar_isle(int player, int card, event_t event){
/* CARD_ID_SPHINX_OF_JWAR_ISLE	10671
Sphinx of Jwar Isle	|4|U|U
Creature - Sphinx 5/5
Flying
Shroud (This creature can't be the target of spells or abilities.)
You may look at the top card of your library. (You may do this at any time.) */

	if( player == HUMAN && ! is_humiliated(player, card) ){
		reveal_top_card(player, card, event);
	}
	return 0;
}

int card_sphinx_of_lost_truths (int player, int card, event_t event){
/* CARD_ID_SPHINX_OF_LOST_TRUTHS	10672
Sphinx of Lost Truths	|3|U|U
Creature - Sphinx 3/5
Kicker |1|U (You may pay an additional |1|U as you cast this spell.)
Flying
When ~ enters the battlefield, draw three cards. Then if it wasn't kicked, discard three cards. */

	kicker(player, card, event, MANACOST_XU(1, 1));
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 3);
		if( ! kicked(player, card) ){
			new_multidiscard(player, 3, 0, player);
		}
	}
	return 0;
}

int card_spreading_seas(int player, int card, event_t event){
/* CARD_ID_SPREADING_SEAS	10675
Spreading Seas	|1|U
Enchantment - Aura
Enchant land
When ~ enters the battlefield, draw a card.
Enchanted land is |Han Island. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_CHANGE_TYPE && affect_me(p, c) ){
			if( instance->dummy3 < 1 ){
				instance->dummy3 = land_into_new_basic_land_type(event_result, SUBTYPE_ISLAND);
			}
			event_result = instance->dummy3;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
		if( spell_fizzled != 1 ){
			if( has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_ISLAND) ||
				instance->targets[0].player == player )
			{
				ai_modifier -= 24;
			}
			else{
				ai_modifier += 24;
			}
		}
		return 0;
	}

	return targeted_aura(player, card, event, &td, "TARGET_LAND");
}

int card_summoners_bane(int player, int card, event_t event){
/* CARD_ID_SUMMONERS_BANE	10678
Summoner's Bane	|2|U|U
Instant
Counter target creature spell. Create a 2/2 |Sblue Illusion creature token. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, &td, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ILLUSION_TOKEN, &token);
			token.special_infos = 66;
			token.pow = 2;
			token.tou = 2;
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}

int card_tempest_owl(int player, int card, event_t event){
/* CARD_ID_TEMPEST_OWL	10685
Tempest Owl	|1|U
Creature - Bird 1/2
Kicker |4|U (You may pay an additional |4|U as you cast this spell.)
Flying
When ~ enters the battlefield, if it was kicked, tap up to three target permanents. */

	kicker(player, card, event, MANACOST_XU(4, 1));
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT );
		int i;
		for(i=0;i<3;i++){
			if( can_target(&td) && new_pick_target(&td, "TARGET_PERMANENT", i, 0) ){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
			}
			else{
				break;
			}
		}
		for(i=0;i<instance->number_of_targets;i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			tap_card(instance->targets[i].player, instance->targets[i].card);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_trapfinders_trick(int player, int card, event_t event){
/* CARD_ID_TRAPFINDERS_TRICK	10691
Trapfinder's Trick	|1|U
Sorcery
Target player reveals his or her hand and discards all Trap cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if(event == EVENT_RESOLVE_SPELL ){
		if (valid_target(&td)){
			ec_definition_t ec;
			default_ec_definition(get_card_instance(player, card)->targets[0].player, player, &ec);
			ec.effect = EC_DISCARD | EC_ALL_WHICH_MATCH_CRITERIA;

			test_definition_t test;
			new_default_test_definition(&test, 0, "");
			test.subtype = SUBTYPE_TRAP;

			new_effect_coercion(&ec, &test);
		}

		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_trapmakers_snare(int player, int card, event_t event){
/* CARD_ID_TRAPMAKERS_SNARE	10692
Trapmaker's Snare	|1|U
Instant
Search your library for a Trap card, reveal it, and put it into your hand. Then shuffle your library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 24;
	}
	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, 0, "Select a Trap card.");
		test.subtype = SUBTYPE_TRAP;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 1, AI_MAX_VALUE, &test);
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

int card_whiplash_trap(int player, int card, event_t event){
/* CARD_ID_WHIPLASH_TRAP	10710
Whiplash Trap	|3|U|U
Instant - Trap
If an opponent had two or more creatures enter the battlefield under his or her control this turn, you may pay |U rather than pay ~'s mana cost.
Return two target creatures to their owners' hands. */

	if( event == EVENT_MODIFY_COST ){
		if( get_trap_condition(1-player, TRAP_CREATURES_PLAYED) >= 2 ){
			COST_BLUE --;
			COST_COLORLESS -= 3;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				bounce_permanent(instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

int card_hellkite_charger(int player, int card, event_t event)
{
/* CARD_ID_HELLKITE_CHARGER	10566
Hellkite Charger	|4|R|R
Creature - Dragon 5/5
Flying, haste
Whenever ~ attacks, you may pay |5|R|R. If you do, untap all attacking creatures and after this phase, there is an additional combat phase. */

	haste(player, card, event);

	store_attackers(player, card, event, 0, player, card, NULL);

	if (xtrigger_condition() == XTRIGGER_ATTACKING && affect_me(player, card) && reason_for_trigger_controller == player){
		int good = player == HUMAN ? 1 : 0;
		if( player == AI ){
			if( has_mana_multi(player, MANACOST_XR(5,2)) ){
				good = 1;
			}
		}
		if( declare_attackers_trigger(player, card, event, (good ? RESOLVE_TRIGGER_AI(player) : 0), player, card) &&
			charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, MANACOST_XR(5,2))
		  ){
			relentless_assault_effect(player, card);
		}
	}

	return 0;
}

static const char* is_damage_to_player_or_planeswalker(int who_chooses, int player, int card, int targeting_player, int targeting_card){
	card_instance_t* dmg = get_card_instance(player, card);
	if( dmg->internal_card_id != damage_card ){
		return ",damage";
	}
	if( dmg->damage_target_card >= 0 && !damage_is_to_planeswalker(dmg) ){
		return ",damage to a player or planeswalker";
	}
	return NULL;
}

int card_noble_vestige(int player, int card, event_t event){
	/* Noble Vestige	|2|W	0x200bb3c
	 * Creature - Spirit 1/2
	 * Flying
	 * |T: Prevent the next 1 damage that would be dealt to target player or planeswalker this turn. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_ANY);
	td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td1.extra = (int32_t)is_damage_to_player_or_planeswalker;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0,
									&td1, "TARGET_DAMAGE");
}

int card_tanglesap(int player, int card, event_t event){
/* CARD_ID_TANGLESAP	10683
Tanglesap	|1|G
Instant
Prevent all combat damage that would be dealt this turn by creatures without trample. */

	if( event == EVENT_RESOLVE_SPELL ){
		fog_special(player, card, ANYBODY, FOG_COMBAT_DAMAGE_ONLY | FOG_CREATURES_WITHOUT_TRAMPLE);
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return card_fog(player, card, event);
}
