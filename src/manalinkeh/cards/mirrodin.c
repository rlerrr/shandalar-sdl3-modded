#include "manalink.h"

// GLOBAL FUNCTIONS

void affinity(int player, int card, event_t event, int type, int subtype){

	if( event == EVENT_MODIFY_COST ){
		COST_COLORLESS-=count_subtype(player, type, subtype);
	}
}

static void clockwork(int player, int card, event_t event, int counters){

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, counters);

	if( event == EVENT_DECLARE_ATTACKERS && is_attacking(player, card) && ! is_humiliated(player, card) ){
		get_card_instance( player, card )->info_slot = 66;
	}

	if( current_turn != player && event == EVENT_DECLARE_BLOCKERS && get_card_instance( player, card )->blocking < 255 &&
		! is_humiliated(player, card) )
	{
		get_card_instance( player, card )->info_slot = 66;
	}

	if( get_card_instance( player, card )->info_slot == 66 && end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		remove_1_1_counter(player, card);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->info_slot = 0;
	}
}

int get_average_cmc(int player){

	int *deck = deck_ptr[player];
	int costs[16];
	int i;
	for(i=0; i<16; i++){
		costs[i] = 0;
	}
	int cd = count_deck(player);
	while( cd > -1 ){
			if( ! is_what(-1, deck[cd], TYPE_LAND) ){
				costs[get_cmc_by_id(cards_data[deck[cd]].id)]++;
			}
			cd--;
	}
	int par = 0;
	int result = -1;
	i=0;
	for(i=0; i<16; i++){
		if( costs[i] > par ){
			result = i;
			par = costs[i];
		}
	}
	return result;
}

int nims(int player, int card, event_t event){

	if( event == EVENT_POWER && in_play(player, card) && affect_me(player, card) && ! is_humiliated(player, card) ){
		event_result += count_subtype(player, TYPE_ARTIFACT, -1);
	}
	return 0;
}

static int spellbomb(int player, int card, event_t event){
	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
}


// CARDS

int card_aether_vial(int player, int card, event_t event){
/* At the beginning of your upkeep, you may put a charge counter on ~.
 * |T: You may put a creature card with converted mana cost equal to the number of charge counters on ~ from your hand onto the battlefield. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int AI_choice = 0;
		int most_common_cmc = get_most_common_cmc_in_hand(player, TYPE_CREATURE);
		if( count_counters(player, card, COUNTER_CHARGE) >= most_common_cmc ){
			AI_choice = 1;
		}

		char buffer2[50];
		snprintf(buffer2, 50, " Add a counter (currently at %d)\n Do not add",  count_counters(player, card, COUNTER_CHARGE));
		int choice = do_dialog(player, player, card, -1, -1, buffer2, AI_choice);
		if( choice == 0 ){
			add_counter(player, card, COUNTER_CHARGE);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	default_test_definition(&this_test, TYPE_CREATURE);
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		int amount = count_counters(player, card, COUNTER_CHARGE);
		this_test.cmc = amount;
		if( check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &this_test) ){
			ai_modifier += (current_turn == player ? -48 : 48);
		}
		else{
			ai_modifier -= 128;
		}
		instance->info_slot = amount;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int amount = instance->info_slot;
		this_test.cmc = amount;
		scnprintf(this_test.message, 100, "Select a creature with cmc %d.", amount);
		new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_altar_of_shadows(int player, int card, event_t event){

	/* Altar of Shadows	|7
	 * Artifact
	 * At the beginning of your precombat main phase, add |B to your mana pool for each charge counter on ~.
	 * |7, |T: Destroy target creature. Then put a charge counter on ~. */

	if( beginning_of_phase(player, card, event, player, card, PHASE_MAIN1) ){
		int counters = count_counters(player, card, COUNTER_CHARGE);
		if (counters > 0){
			produce_mana(player, COLOR_BLACK, counters);
		}
	}

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			add_counter(player, card, COUNTER_CHARGE);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(7), 0, &td, "TARGET_CREATURE");
}

int card_arc_slogger(int player, int card, event_t event){
	/* Arc-Slogger	|3|R|R	0x20051bf
	 * Creature - Beast 4/5
	 * |R, Exile the top ten cards of your library: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_ACTIVATE && count_deck(player) >= 10 ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		ai_modifier -= 32;
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_ANY");
		if( spell_fizzled != 1 ){
			int i;
			for(i=0; i<10;i++){
				rfg_top_card_of_deck(player);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return 0;
}

int card_assert_autority(int player, int card, event_t event){
/* CARD_ID_ASSERT_AUTHORITY	6613
Assert Authority	|5|U|U
Instant
Affinity for artifacts (This spell costs |1 less to cast for each artifact you control.)
Counter target spell. If that spell is countered this way, exile it instead of putting it into its owner's graveyard. */

	affinity(player, card, event, TYPE_ARTIFACT, -1);
	return card_dissipate(player, card, event);
}

int card_aether_spellbomb(int player, int card, event_t event){
/* CARD_ID_AETHER_SPELLBOMB	6614
Aether Spellbomb	|1
Artifact
|U, Sacrifice ~: Return target creature to its owner's hand.
|1, Sacrifice ~: Draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	enum{
		CHOICE_DRAW_CARD = 1,
		CHOICE_BOUNCE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return spellbomb(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0,
						1,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_U(1), 0,
													&td, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Sac & draw", 	abils[1], 14 - (hand_count[player]*2),
							"Sac & bounce", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_DRAW_CARD ){
			return spellbomb(player, card, event);
		}
		if( choice == CHOICE_BOUNCE ){
			return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_U(1), 0,
											&td, "TARGET_CREATURE");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DRAW_CARD ){
			draw_cards(player, 1);
		}
		if( instance->info_slot == CHOICE_BOUNCE ){
			if( valid_target(&td) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_auriok_bladewarden(int player, int card, event_t event){
/*
Auriok Bladewarden |1|W	0x200f525
Creature � Human Soldier 1/1
{T}: Target creature gets +X/+X until end of turn, where X is Auriok Bladewarden's power.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = get_power(instance->parent_controller, instance->parent_card);
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							amount, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_auriok_steelshaper(int player, int card, event_t event){
/* CARD_ID_AURIOK_STEELSHAPER	6616
Auriok Steelshaper	|1|W
Creature - Human Soldier 1/1
Equip costs you pay cost |1 less.
As long as ~ is equipped, each creature you control that's a Soldier or a Knight gets +1/+1. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && in_play(player, card) &&
		! is_humiliated(player, card) )
	{
		if( equipments_attached_to_me(player, card, EATM_CHECK) ){
			if( affected_card_controller == player && ( has_creature_type( affected_card_controller, affected_card, SUBTYPE_SOLDIER) ||
				has_creature_type( affected_card_controller, affected_card, SUBTYPE_KNIGHT) ) )
			{
				event_result++;
			}
		}
	}

	return 0;
}

int card_auriok_transfixer(int player, int card, event_t event){
/* CARD_ID_AURIOK_TRANSFIXER	6617
Auriok Transfixer	|W
Creature - Human Scout 1/1
|W, |T: Tap target artifact. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(1), 0, &td, "TARGET_ARTIFACT");
}

static int awe_strike_legacy(int player, int card, event_t event ){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *damage= get_card_instance(affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_source_card == c && damage->damage_source_player == p && damage->info_slot > 0 ){
					gain_life(player, damage->info_slot);
					prevent_damage(affected_card_controller, affected_card, -1);
					kill_card(player, card, KILL_EXILE);
				}
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_awe_strike(int player, int card, event_t event){
/*
Awe Strike |W
Instant
The next time target creature would deal damage this turn, prevent that damage. You gain life equal to the damage prevented this way.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier += (check_state(instance->targets[0].player, instance->targets[0].card, STATE_ATTACKING | STATE_BLOCKING) ? 48 : -48);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &awe_strike_legacy, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_barter_in_blood(int player, int card, event_t event){
/* CARD_ID_BARTER_IN_BLOOD	6620
Barter in Blood	|2|B|B
Sorcery
Each player sacrifices two creatures. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		test.qty = 2;
		APNAP(p, { new_sacrifice(player, card, p, SAC_CAUSED, &test); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_banshees_blade(int player, int card, event_t event)
{
  /* Banshee's Blade	|2
   * Artifact - Equipment
   * Equipped creature gets +1/+1 for each charge counter on ~.
   * Whenever equipped creature deals combat damage, put a charge counter on ~.
   * Equip |2 */

  int packets;
  if ((packets = equipped_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE)))
	add_counters(player, card, COUNTER_CHARGE, packets);

  int counters = count_counters(player, card, COUNTER_CHARGE);
  return vanilla_equipment(player, card, event, 2, counters,counters, 0,0);
}

int card_betrayal_of_flesh(int player, int card, event_t event){
/*
Betrayal of Flesh |5|B
Instant
Choose one �
� Destroy target creature.
� Return target creature card from your graveyard to the battlefield.
Entwine�Sacrifice three lands. (Choose both if you pay the entwine cost.)
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_KILL = 1,
		CHOICE_REANIMATE,
		CHOICE_ENTWINE
	};

	if( event == EVENT_CAN_CAST){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return 1;
		}
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int sacced[3] = {-1};
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			test_definition_t test2;
			default_test_definition(&test2, TYPE_LAND);
			test2.qty = 3;
			int abils[4] = {
							0,
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td, NULL, 1, NULL),
							generic_spell(player, card, EVENT_CAN_CAST, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test),
							new_can_sacrifice_as_cost(player, card, &test)
			};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Kill a creature", abils[1], 10,
								"Reanimate a creature", abils[2], 5,
								"Entwine", abils[1] && abils[2] && abils[3], count_subtype(player, TYPE_LAND, -1) > 6 ? 15 : 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( instance->info_slot & CHOICE_ENTWINE ){
				int i;
				for(i=0; i<3; i++){
					int sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_AS_COST | SAC_RETURN_CHOICE, &test);
					if( sac ){
						sacced[i] = sac;
					}
					else{
						spell_fizzled = 1;
						break;
					}
				}
				for(i=0; i<3; i++){
					if( sacced[i] != -1 ){
						state_untargettable(BYTE2(sacced[i]), BYTE3(sacced[i]), 0);
					}
				}
				if( spell_fizzled == 1 ){
					return 0;
				}
			}
		}
		instance->number_of_targets = 0;
		int base_target = 0;
		if( instance->info_slot & CHOICE_KILL ){
			if( pick_target(&td, "TARGET_CREATURE") ){
				base_target++;
			}
			else{
				return 0;
			}
		}
		if( instance->info_slot & CHOICE_REANIMATE ){
			if( ! select_target_from_grave_source(player, card, player, 0, AI_MAX_CMC, &test, base_target) ){
				return 0;
			}
		}
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int i;
			for(i=0; i<3; i++){
				kill_card(BYTE2(sacced[i]), BYTE3(sacced[i]), KILL_SACRIFICE);
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int base_target = 0;
		if( instance->info_slot & CHOICE_KILL ){
			if( valid_target(&td) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_REANIMATE ){
			int selected = validate_target_from_grave_source(player, card, player, base_target);
			if( selected != -1 ){
				reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int blinding_beam_effect(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		if( affected_card_controller == p && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			does_not_untap(affected_card_controller, affected_card, event);
		}
	}

	if( event == EVENT_END_OF_UNTAP_STEP && current_turn == get_card_instance(player, card)->damage_target_player ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_blinding_beam(int player, int card, event_t event){
/* CARD_ID_BLINDING_BEAM	6623
Blinding Beam	|2|W
Instant
Choose one -
* Tap two target creatures.
* Creatures don't untap during target player's next untap step.
Entwine |1 (Choose both if you pay the entwine cost.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 2, NULL) ){
			return 1;
		}
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td1, NULL, 1, NULL) ){
			return 1;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abilities[2] = {target_available(player, card, &td) > 1, can_target(&td1)};
			int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
								"Tap creatures", abilities[0], 10,
								"Exhaustion for creatures", abilities[1], 5,
								"Entwine", abilities[0] && abilities[1] && has_mana(player, COLOR_COLORLESS, 1), 15);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( choice == 3 ){
				charge_mana(player, COLOR_COLORLESS, 1);
			}
			if( spell_fizzled == 1 ){
				return 0;
			}
		}
		int base_target = 0;
		if( instance->info_slot & 1 ){
			if( pick_target(&td, "TARGET_CREATURE") ){
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
				new_pick_target(&td, "TARGET_CREATURE", 1, 1);
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
				base_target += 2;
			}
			if( spell_fizzled == 1 ){
				return 0;
			}
		}
		if( instance->info_slot & 2 ){
			new_pick_target(&td1, "TARGET_PLAYER", base_target, 1);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int base_target = 0;
		if( instance->info_slot & 1 ){
			int i;
			for(i=0; i<2; i++){
				if( validate_target(player, card, &td, i) ){
					tap_card(instance->targets[i].player, instance->targets[i].card);
				}
			}
			base_target += 2;
		}
		if( instance->info_slot & 2 ){
			if( validate_target(player, card, &td1, base_target) ){
				int legacy = create_legacy_effect(player, card, &blinding_beam_effect);
				card_instance_t *leg = get_card_instance(player, legacy);
				leg->targets[0].player = instance->targets[base_target].player;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_blinkmoth_urn(int player, int card, event_t event){
/* CARD_ID_BLINKMOTH_URN	6624
Blinkmoth Urn	|5
Artifact
At the beginning of each player's precombat main phase, if ~ is untapped, that player adds |C to his or her mana pool for each artifact he or she controls. */

	if( in_play(player, card) && ! is_humiliated(player, card) && ! is_tapped(player, card) ){
		if( beginning_of_phase(player, card, event, ANYBODY, -1, PHASE_MAIN1) ){
			produce_mana(current_turn, COLOR_COLORLESS, count_subtype(current_turn, TYPE_ARTIFACT, -1));
		}
	}

	return 0;
}

int card_blinkmoth_well(int player, int card, event_t event){
/* CARD_ID_BLINKMOTH_WELL	6625
Blinkmoth Well	""
Land
|T: Add |C to your mana pool.
|2, |T: Tap target noncreature artifact. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.illegal_type = TYPE_CREATURE;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_TAP_ARTIFACT
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, NULL) )
		{
			choice = CHOICE_TAP_ARTIFACT;
		}
		instance->info_slot = choice;

		if( choice == CHOICE_TAP_ARTIFACT && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Tap noncreature artifact", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( instance->info_slot == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( instance->info_slot == CHOICE_TAP_ARTIFACT ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(2), 0,
										&td, "Select target noncreature artifact");
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( instance->info_slot == CHOICE_TAP_ARTIFACT ){
		if( instance->info_slot > 0 ){
			if( valid_target(&td) ){
				tap_card(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_bloodscent(int player, int card, event_t event)
{
  /* Bloodscent	|3|G
   * Instant
   * All creatures able to block target creature this turn do so. */

  if (!IS_CASTING(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;

  return vanilla_pump(player, card, event, &td, 0,0, 0,SP_KEYWORD_LURE);
}

int card_bonesplitter(int player, int card, event_t event){
/* CARD_ID_BONESPLITTER	6627
Bonesplitter	|1
Artifact - Equipment
Equipped creature gets +2/+0.
Equip |1 */

	return vanilla_equipment(player, card, event, 1, 2, 0, 0, 0);
}


int card_bosh_iron_golem(int player, int card, event_t event){
	/* Bosh, Iron Golem	|8	0x20051fb
	 * Legendary Artifact Creature - Golem 6/7
	 * Trample
	 * |3|R, Sacrifice an artifact: ~ deals damage equal to the sacrificed artifact's converted mana cost to any target. */
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, get_card_instance(player, card)->info_slot);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST_XR(3, 1), 0,
															&td, "TARGET_ANY", 0, &test, SARV_REPORT_CMC);
}

int card_broodstar(int player, int card, event_t event)
{
  /* Broodstar	|8|U|U
   * Creature - Beast 100/100
   * Affinity for artifacts
   * Flying
   * ~'s power and toughness are each equal to the number of artifacts you control. */

	affinity(player, card, event, TYPE_ARTIFACT, -1);

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && player != -1 && affect_me(player, card) && ! is_humiliated(player, card) )
		event_result += count_subtype(player, TYPE_ARTIFACT, -1);

	return 0;
}

int card_chalice_of_the_void(int player, int card, event_t event){

	/* Chalice of the Void	|X|X
	 * Artifact
	 * ~ enters the battlefield with X charge counters on it.
	 * Whenever a player casts a spell with converted mana cost equal to the number of charge counters on ~, counter that spell. */

	if( event == EVENT_MODIFY_COST ){
		if( check_battlefield_for_id(ANYBODY, CARD_ID_GADDOCK_TEEG) ){
			infinite_casting_cost();
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		instance->info_slot = 0;
		if( ! is_token(player, card) && ! played_for_free(player, card) ){
			if( player == AI ){
				if( instance->targets[1].player < 0 ){
					instance->targets[1].player = get_average_cmc(1-player);
				}
				if( ! has_mana(player, COLOR_ARTIFACT, instance->targets[1].player*2) ){
					ai_modifier-=1000;
				}
				else{
					charge_mana(player, COLOR_ARTIFACT, instance->targets[1].player*2);
					if( spell_fizzled != 1 ){
						instance->info_slot = instance->targets[1].player;
					}
				}
			}
			else{
				instance->info_slot = (charge_mana_for_double_x(player, COLOR_ARTIFACT) / 2);
			}
		}
		set_special_flags2(player, card, SF2_X_SPELL);
		enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, instance->info_slot);
	}

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.cmc = count_counters(player, card, COUNTER_CHARGE);
		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &test) ){
			real_counter_a_spell(player, card, trigger_cause_controller, trigger_cause);
		}
	}

	return 0;
}

int card_chrome_mox(int player, int card, event_t event){
/* CARD_ID_CHROME_MOX	6632
Chrome Mox	|0
Artifact
Imprint - When ~ enters the battlefield, you may exile a nonartifact, nonland card from your hand.
|T: Add one mana of any of the exiled card's colors to your mana pool. */

	if( IS_AI(player) && event == EVENT_MODIFY_COST ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND | TYPE_ARTIFACT);
		this_test.type_flag = DOESNT_MATCH;
		this_test.zone = TARGET_ZONE_HAND;
		if( ! check_battlefield_for_special_card(player, card, player, 0, &this_test)  ){
			infinite_casting_cost();
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND | TYPE_ARTIFACT, "Select a nonartifact, nonland card to exile.");
		this_test.type_flag = DOESNT_MATCH;
		this_test.zone = TARGET_ZONE_HAND;

		int selected = -1;
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			if( player == HUMAN ){
				selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_VALUE, -1, &this_test);
			}
			else{
				int count = 0;
				int max_color = -1;
				while( count < active_cards_count[player] ){
						if( in_hand(player, count) && new_make_test_in_play(player, count, -1, &this_test) ){
							int clr = num_bits_set(get_color(player, count) & COLOR_TEST_ANY_COLORED);
							if( clr > max_color ){
								max_color = clr;
								selected = count;
								if( max_color == 5 ){
									break;
								}
							}
						}
						count++;
				}
			}
		}
		if( selected != -1 ){
			get_card_instance( player, card)->info_slot |= (get_color(player, selected) & COLOR_TEST_ANY_COLORED);
			rfg_card_in_hand(player, selected);
		}
	}
	return mana_producer(player, card, event);
}

int card_clockwork_beetle(int player, int card, event_t event){
/* CARD_ID_CLOCKWORK_BEETLE	6633
Clockwork Beetle	|1
Artifact Creature - Insect 0/0
~ enters the battlefield with two +1/+1 counters on it.
Whenever ~ attacks or blocks, remove a +1/+1 counter from it at end of combat. */

	clockwork(player, card, event, 2);
	return 0;
}

int card_clockwork_dragon(int player, int card, event_t event){
/* CARD_ID_CLOCKWORK_DRAGON	6635
Clockwork Dragon	|7
Artifact Creature - Dragon 0/0
Flying
~ enters the battlefield with six +1/+1 counters on it.
Whenever ~ attacks or blocks, remove a +1/+1 counter from it at end of combat.
|3: Put a +1/+1 counter on ~. */

	clockwork(player, card, event, 6);
	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		add_1_1_counter(instance->parent_controller, instance->parent_card);
		return 0;
	}
	return generic_shade(player, card, event, 0, MANACOST_X(3), 1, 1, 0, 0);
}

int card_cloudpost(int player, int card, event_t event){
/* CARD_ID_CLOUDPOST	6637
Cloudpost	""
Land - Locus
~ enters the battlefield tapped.
|T: Add |C to your mana pool for each Locus on the battlefield. */

	comes_into_play_tapped(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
		declare_mana_available(player, COLOR_COLORLESS, count_subtype(2, TYPE_LAND, SUBTYPE_LOCUS) );
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, EVENT_CAN_ACTIVATE);
	}

	if( event == EVENT_ACTIVATE){
		produce_mana_tapped(player, card, COLOR_COLORLESS, count_subtype(2, TYPE_LAND, SUBTYPE_LOCUS));
	}

	return 0;
}

int card_confusion_in_the_ranks(int player, int card, event_t event){
/*
Confusion in the Ranks |3|R|R
Enchantment
Whenever an artifact, creature, or enchantment enters the battlefield, its controller chooses target permanent another player controls
that shares a card type with it. Exchange control of those permanents.
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(affected_card_controller, affected_card) &&
		reason_for_trigger_controller == player )
	{
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT | TYPE_CREATURE | TYPE_ENCHANTMENT);
		if( new_specific_cip(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &this_test) ){
			card_instance_t *instance = get_card_instance( player, card );
			if( instance->targets[2].player == 66 ){  //If you exchange this at his ETB with another enchantment, strange things happens.
				instance->targets[2].player = 0;
				return 0;
			}

			int tp = trigger_cause_controller;
			int tc = trigger_cause;

			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_PERMANENT );
			td.required_type = get_type(tp, tc);
			td.preferred_controller = 1-tp;
			td.allowed_controller = 1-tp;
			td.allow_cancel = 0;
			td.who_chooses = tp;

			instance->number_of_targets = 0;

			char msg[100];
			char typetext[100];
			scnprintf(msg, 100, "Select target %s your opponent controls.", type_text(typetext, 100, td.required_type, 0));

			if( can_target(&td) && new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
				if( tp == player && tc == card  ){
					instance->targets[2].player = 66;
				}
				if( instance->targets[1].player == player ){
					exchange_control_of_target_permanents(player, card, tp, tc, instance->targets[0].player, instance->targets[0].card);
				}
				else{
					exchange_control_of_target_permanents(player, card, instance->targets[0].player, instance->targets[0].card, tp, tc);
				}
				instance->number_of_targets = 0;
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[2].player = 0;
	}

	return global_enchantment(player, card, event);
}

int card_consume_spirit(int player, int card, event_t event){
	/* Consume Spirit	|X|1|B	0x2005642
	 * Sorcery
	 * Spend only |Sblack mana on X.
	 * ~ deals X damage to any target and you gain X life. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST || event == EVENT_CAN_CHANGE_TARGET || event == EVENT_CHANGE_TARGET ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_ANY", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			charge_mana(player, COLOR_BLACK, -1);
			if( spell_fizzled != 1 ){
				set_x_for_x_spells(player, card, event, x_value);
			}
		}
		if( spell_fizzled != 1 ){
			pick_target(&td, "TARGET_ANY");
		}
	}


	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) && instance->info_slot > 0){
			damage_target0(player, card, instance->info_slot);
			gain_life(player, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_crystal_shard(int player, int card, event_t event){
/* CARD_ID_CRYSTAL_SHARD	6643
Crystal Shard	|3
Artifact
|3, |T or |U, |T: Return target creature to its owner's hand unless its controller pays |1. */	

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(1), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int choice = 0;
		if( has_mana_for_activated_ability(player, card, MANACOST_U(1)) ){
			if( has_mana_for_activated_ability(player, card, MANACOST_X(3)) ){
				choice = do_dialog(player, player, card, -1, -1, " Pay U\n Pay 3\n Cancel", 0);
			}
		}
		else{
			choice = 1;
		}

		if( choice == 2 ){
			spell_fizzled = 1;
		}
		else{
			int u = choice == 0 ? 1 : 0;
			int x = choice == 1 ? 3 : 0;
			if( charge_mana_for_activated_ability(player, card, MANACOST_XU(x,u)) && pick_target(&td, "TARGET_CREATURE") ){
				add_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int bounce = 1;
			if( has_mana(instance->targets[0].player, COLOR_COLORLESS, 1) ){
				int ai_choice = instance->targets[0].player != player ? 0 : 1;
				int choice = do_dialog(instance->targets[0].player, player, card, -1, -1, " Pay 1\n Pass", ai_choice);
				if( ! choice ){
					charge_mana(instance->targets[0].player, COLOR_COLORLESS, 1);
					if( spell_fizzled != 1 ){
						bounce = 0;
					}
				}
			}
			if( bounce == 1 ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_culling_scales(int player, int card, event_t event){
/*
Culling Scales |3
Artifact
At the beginning of your upkeep, destroy target nonland permanent with the lowest converted mana cost.
(If two or more permanents are tied for lowest cost, target any one of them.)
*/
	upkeep_trigger_ability(player, card, event, player);
	
    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_LAND;
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
					   if( in_play(i, count) && is_what(i, count, TYPE_PERMANENT) && ! is_what(i, count, TYPE_LAND) ){
						   if( get_cmc(i, count) < min_t ){
							   t_player = i;
							   t_card = count;
							   min_t = get_cmc(i, count);
							   doubles_count = 1;
							   instance->targets[doubles_count].player = t_player;
							   instance->targets[doubles_count].card = t_card;
						   }
						   else if( get_cmc(i, count) == min_t ){
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
							if( in_play(i, count) && is_what(i, count, TYPE_PERMANENT) && ! is_what(i, count, TYPE_LAND) ){

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
			    char buffer[100];
			    scnprintf(buffer, 100, "Select target permanent with CMC %d", min_t);
				
				if(new_pick_target(&td, buffer, 0, GS_LITERAL_PROMPT)){
					instance->number_of_targets = 1;
					t_player = instance->targets[0].player;
					t_card = instance->targets[0].card;
					for(i=0; i<2; i++){
						count = 0;
						while( count < active_cards_count[i] ){
								if( in_play(i, count) && is_what(i, count, TYPE_PERMANENT) && ! is_what(i, count, TYPE_LAND) ){

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
			    kill_card(t_player, t_card, KILL_DESTROY);
		    }
	     }
	 }
	return 0;
}

int card_damping_matrix(int player, int card, event_t event){
/*
Damping Matrix |3
Artifact
Activated abilities of artifacts and creatures can't be activated unless they're mana abilities.
*/
	if( in_play(player, card) && ! is_humiliated(player, card) ){
		player_bits[0] |= (PB_NONMANA_ACTIVATE_ABILITIES_OF_CREATURES_DISABLED | PB_NONMANA_ACTIVATE_ABILITIES_OF_ARTIFACTS_DISABLED);
		player_bits[1] |= (PB_NONMANA_ACTIVATE_ABILITIES_OF_CREATURES_DISABLED | PB_NONMANA_ACTIVATE_ABILITIES_OF_ARTIFACTS_DISABLED);
	}

	return 0;
}

int card_deconstruct(int player, int card, event_t event){
/* CARD_ID_DECONSTRUCT	6647
Deconstruct	|2|G
Sorcery
Destroy target artifact. Add |G|G|G to your mana pool. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			produce_mana(player, COLOR_GREEN, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
}

static int disciple_of_the_vault_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_DISCIPLE_OF_THE_VAULT ){
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
					}
				}
				count_for_gfp_ability(player, card, event, ANYBODY, TYPE_ARTIFACT, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			target_definition_t td;
			default_target_definition(player, card, &td, 0);
			td.zone = TARGET_ZONE_PLAYERS;
			if( would_validate_arbitrary_target(&td, 1-player, -1) ){
				int amount = BYTE0(result);
				lose_life(1-player, amount);
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

int card_disciple_of_the_vault(int player, int card, event_t event)
{
/*
Disciple of the Vault |B
Creature � Human Cleric 1/1
Whenever an artifact is put into a graveyard from the battlefield, you may have target opponent lose 1 life.
*/
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)disciple_of_the_vault_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &disciple_of_the_vault_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_ARTIFACT, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			target_definition_t td;
			default_target_definition(player, card, &td, 0);
			td.zone = TARGET_ZONE_PLAYERS;
			if( would_validate_arbitrary_target(&td, 1-player, -1) ){
				int amount = BYTE0(result);
				lose_life(1-player, amount);
			}
		}
	}

  return 0;
}

int card_dreams_grip(int player, int card, event_t event){
/*
Dream's Grip |U
Instant
Choose one �
� Tap target permanent.
� Untap target permanent.
Entwine {1} (Choose both if you pay the entwine cost.)
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);
	td1.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_TAP = 1,
		CHOICE_UNTAP,
		CHOICE_ENTWINE
	};

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int can_entwine = target_available(player, card, &td) > 1 && has_mana(player, COLOR_COLORLESS, 1) ? 1 : 0;
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Tap a creature", 		1, 5,
								"Untap a creature", 	1, 5,
								"Entwine", 				can_entwine, 10);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( instance->info_slot & CHOICE_ENTWINE ){
				charge_mana(player, COLOR_COLORLESS, 1);
				if( spell_fizzled == 1 ){
					return 0;
				}
			}
		}
		int base_target = 0;
		if( instance->info_slot & CHOICE_TAP ){
			if( new_pick_target(&td, "Select target creature to tap.", 0, 1 | GS_LITERAL_PROMPT) ){
				base_target++;
			}
			else{
				return 0;
			}
		}
		if( instance->info_slot & CHOICE_UNTAP ){
			new_pick_target(&td1, "Select target creature to untap.", base_target, 1 | GS_LITERAL_PROMPT);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int base_target = 0;
		if( instance->info_slot & CHOICE_TAP ){
			if( validate_target(player, card, &td, 0) ){
				tap_card(instance->targets[0].player, instance->targets[0].card);
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_UNTAP ){
			if( validate_target(player, card, &td1, base_target) ){
				untap_card(instance->targets[base_target].player, instance->targets[base_target].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int dross_harvester_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_DROSS_HARVESTER ){
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
					}
				}
				count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			gain_life(player, amount);
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

int card_dross_harvester(int player, int card, event_t event){
/* CARD_ID_DROSS_HARVESTER	6652
Dross Harvester	|1|B|B
Creature - Horror 4/4
Protection from |Swhite
At the beginning of your end step, you lose 4 life.
Whenever a creature dies, you gain 2 life. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && life[player] < 9 ){
		ai_modifier-=100;
	}

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)dross_harvester_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &dross_harvester_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			gain_life(player, amount);
		}
	}

	if( current_turn == player && eot_trigger(player, card, event) ){
		lose_life(player, 4);
	}

	return 0;
}

static int dross_scorpion_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_DROSS_SCORPION ){
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
						count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
						if( instance->targets[11].player < 0 ){
							instance->targets[11].player = 0;
						}
						SET_BYTE1(instance->targets[11].player) = 1;
						return 0;
					}
				}
				test_definition_t test;
				default_test_definition(&test, TYPE_CREATURE | TYPE_ARTIFACT);
				test.type_flag = F1_MATCH_ALL;
				count_for_gfp_ability(player, card, event, ANYBODY, 0, &test);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_ARTIFACT);
			td.preferred_controller = player;
			card_instance_t *instance = get_card_instance(player, card);
			int amount = BYTE0(result);
			int i;
			for(i=0; i<amount; i++){
				if( can_target(&td) && new_pick_target(&td, "Select target artifact to untap.", 1, GS_LITERAL_PROMPT) ){
					untap_card(instance->targets[1].player, instance->targets[1].card);
				}
				instance->number_of_targets = 1;
			}
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

int card_dross_scorpion(int player, int card, event_t event){
/*
Dross Scorpion |4
Artifact Creature - Scorpion 3/1
Whenever Dross Scorpion or another artifact creature dies, you may untap target artifact.
*/
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)dross_scorpion_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &dross_scorpion_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( affect_me(player, card) ){
			count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
			return 0;
		}
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE | TYPE_ARTIFACT);
		test.type_flag = F1_MATCH_ALL;
		count_for_gfp_ability(player, card, event, ANYBODY, 0, &test);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_ARTIFACT);
			td.preferred_controller = player;
			card_instance_t *instance = get_card_instance(player, card);
			int amount = BYTE0(result);
			int i;
			for(i=0; i<amount; i++){
				if( can_target(&td) && new_pick_target(&td, "Select target artifact to untap.", 0, GS_LITERAL_PROMPT) ){
					untap_card(instance->targets[0].player, instance->targets[0].card);
				}
				instance->number_of_targets = 0;
			}
		}
	}

	return 0;
}

int card_duplicant(int player, int card, event_t event){
/*
Duplicant |6
Artifact Creature � Shapeshifter 2/4
Imprint � When Duplicant enters the battlefield, you may exile target nontoken creature.
As long as a card exiled with Duplicant is a creature card, Duplicant has the power, toughness,
and creature types of the last creature card exiled with Duplicant. It's still a Shapeshifter.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.illegal_type = TARGET_TYPE_TOKEN;
		td.special = player == AI ? TARGET_SPECIAL_NOT_ME : 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) ){
			if( new_pick_target(&td, "Select target nontoken creature.", 0, GS_LITERAL_PROMPT) ){
				real_set_pt(player, card, player, card, get_base_power(instance->targets[0].player, instance->targets[0].card),
							get_base_toughness(instance->targets[0].player, instance->targets[0].card), 0);

				reported_subtypes_t stolen_subtypes;
				get_subtypes_by_csvid( cards_data[get_original_internal_card_id(instance->targets[0].player, instance->targets[0].card)].id,
										&stolen_subtypes);
				int i;
				for(i=0; i<4; i++){
					instance->targets[i+2].player = stolen_subtypes.reported_subtypes[i+2];
				}
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			}
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_electrostatic_bolt(int player, int card, event_t event){
/* CARD_ID_ELECTROSTATIC_BOLT	6657
Electrostatic Bolt	|R
Instant
~ deals 2 damage to target creature. If it's an artifact creature, ~ deals 4 damage to it instead. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int damage = 2;
			if( is_what(instance->targets[0].player, instance->targets[0].card, TYPE_ARTIFACT) ){
				damage = 4;
			}
			damage_target0(player, card, damage);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_elf_replica(int player, int card, event_t event){
/* CARD_ID_ELF_REPLICA	6658
Elf Replica	|3
Artifact Creature - Elf 2/2
|1|G, Sacrifice ~: Destroy target enchantment. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_XG(1, 1), 0, &td, "TARGET_ENCHANTENT");
}

int card_empyrial_plate(int player, int card, event_t event){
/* CARD_ID_EMPYRIAL_PLATE	6659
Empyrial Plate	|2
Artifact - Equipment
Equipped creature gets +1/+1 for each card in your hand.
Equip |2 */

	return vanilla_equipment(player, card, event, 2, hand_count[player], hand_count[player], 0, 0);
}

int card_extraplanar_lens(int player, int card, event_t event){
/*
Extraplanar Lens |3	0x200f52a
Artifact
Imprint � When Extraplanar Lens enters the battlefield, you may exile target land you control.
Whenever a land with the same name as the exiled card is tapped for mana, its controller adds one mana to his or her mana pool of
	any type that land produced.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND );
		td.preferred_controller = player;
		td.allowed_controller = player;
		td.illegal_abilities = 0;
		if( IS_AI(player) ){
			td.required_subtype = SUBTYPE_BASIC;
		}

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) ){
			if( new_pick_target(&td, "Select target land you control.", 0, GS_LITERAL_PROMPT) ){
				int pos = MAX(1, instance->info_slot);
				if( pos < 10 ){
					instance->targets[pos].card = get_card_name(instance->targets[0].player, instance->targets[0].card);
					create_card_name_legacy(player, card, instance->targets[pos].card);
					kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
					instance->info_slot = pos+1;
				}
			}
		}
	}

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		switch (event){
			case EVENT_TAP_CARD:
			{
				card_instance_t *instance = get_card_instance( player, card );

				if (!is_what(affected_card_controller, affected_card, TYPE_LAND)){
					return 0;
				}

				if (!in_play(player, card)){	// Seems unnecessary, but it's explicitly checked in the exe version, and is harmless
					return 0;
				}

				int found = 0;
				int i;
				for(i=1; i<instance->info_slot; i++){
					if( get_card_name(affected_card_controller, affected_card) == instance->targets[i].card ){
						found = 1;
						break;
					}
				}
				if( ! found ){
					return 0;
				}

				produce_mana_of_any_type_tapped_for(player, card, 1);

				return 0;
			}

			case EVENT_COUNT_MANA:
			{
				card_instance_t *instance = get_card_instance( player, card );

				if (!is_what(affected_card_controller, affected_card, TYPE_LAND)){
					return 0;
				}

				if (!in_play(player, card)){	// Seems unnecessary, but it's explicitly checked in the exe version, and is harmless
					return 0;
				}

				int found = 0;
				int i;
				for(i=1; i<instance->info_slot; i++){
					if( get_card_name(affected_card_controller, affected_card) == instance->targets[i].card ){
						found = 1;
						break;
					}
				}
				if( ! found ){
					return 0;
				}

				// A check for EA_MANA_SOURCE is conspicuous by its absence in the exe version.
				if (is_tapped(affected_card_controller, affected_card) || is_animated_and_sick(affected_card_controller, affected_card)
					|| !can_produce_mana(affected_card_controller, affected_card) ){
					return 0;
				}

				int num_colors = 0;
				color_t col;
				card_instance_t* aff_instance = get_card_instance(affected_card_controller, affected_card);
				for (col = COLOR_COLORLESS; col <= COLOR_ARTIFACT; ++col){
					if (aff_instance->card_color & (1 << col)){
						++num_colors;
					}
				}

				if (num_colors > 0){
					declare_mana_available_hex(affected_card_controller, aff_instance->card_color, 1);
				}
				else {
					declare_mana_available(affected_card_controller, single_color_test_bit_to_color(aff_instance->card_color), 1);
				}

				return 0;
			}

			default:
				return 0;
		}
	}

	return 0;
}

int card_farsight_mask(int player, int card, event_t event){
/*
Farsight Mask |5
Artifact
Whenever a source an opponent controls deals damage to you, if Farsight Mask is untapped, you may draw a card.
*/
	check_damage_test(1-player, -1, event, DDBM_MUST_DAMAGE_TRIGGERING_PLAYER, player, card, TYPE_ANY, NULL);

	if( ! is_tapped(player, card) ){
		int result = resolve_damage_trigger(player, card, event, DDBM_MUST_DAMAGE_TRIGGERING_PLAYER, player, card);
		if( result ){
			draw_some_cards_if_you_want(player, card, player, result);
		}
	}

	return 0;
}

int card_fabricate(int player, int card, event_t event){
/* CARD_ID_FABRICATE	1514
Fabricate	|2|U
Sorcery
Search your library for an artifact card, reveal it, and put it into your hand. Then shuffle your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an artifact card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, 1, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_fiery_gambit(int player, int card, event_t event){
/* CARD_ID_FIERY_GAMBIT	6664
Fiery Gambit	|2|R
Sorcery
Flip a coin until you lose a flip or choose to stop flipping. If you lose a flip, ~ has no effect. If you win one or more flips, ~ deals 3 damage to target creature.
If you win two or more flips, ~ deals 6 damage to each opponent. If you win three or more flips, draw nine cards and untap all lands you control. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int flips = 0;
			while(1){
					int result = flip_a_coin(player, card);
					if( result == 0 ){
						flips = 0;
						break;
					}
					else{
						flips++;
						int choice = do_dialog(player, player, card, -1, -1, " Continue\n Stop", flips >= 3 ? 1 : 0);
						if( choice == 1 ){
							break;
						}
					}
					if( flips == 3 ){
						break;
					}
			}
			if( flips ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, card);
			}
			if( flips >= 2 ){
				damage_player(1-player, 6, player, card);
			}
			if( flips >= 3 ){
				draw_cards(player, 9);

				test_definition_t test;
				default_test_definition(&test, TYPE_LAND);
				new_manipulate_all(player, card, player, &test, ACT_UNTAP);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);

}

int card_fireshrieker(int player, int card, event_t event){
/* CARD_ID_FIRESHRIEKER	6665
Fireshrieker	|3
Artifact - Equipment
Equipped creature has double strike. (It deals both first-strike and regular combat damage.)
Equip |2 (|2: Attach to target creature you control. Equip only as a sorcery.) */

	return vanilla_equipment(player, card, event, 2, 0, 0, KEYWORD_DOUBLE_STRIKE, 0);
}

int card_frogmite(int player, int card, event_t event){
	/*
	  Frogmite |4
	  Artifact Creature - Frog 2/2
	  Affinity for artifacts (This spell costs {1} less to cast for each artifact you control.)
	*/
	affinity(player, card, event, TYPE_ARTIFACT, -1);
	return 0;
}

int card_fists_of_the_anvil(int player, int card, event_t event)
{
  /* Fists of the Anvil	|1|R
   * Instant
   * Target creature gets +4/+0 until end of turn. */
  return vanilla_instant_pump(player, card, event, ANYBODY, player, 4,0, 0,0);
}

int card_gate_to_aether(int player, int card, event_t event){
/* CARD_ID_GATE_TO_THE_AETHER	6670
Gate to the Aether	|6
Artifact
At the beginning of each player's upkeep, that player reveals the top card of his or her library.
If it's an artifact, creature, enchantment, or land card, the player may put it onto the battlefield. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int *deck = deck_ptr[current_turn];
		show_deck( HUMAN, deck, 1, "Here's the first card of deck", 0, 0x7375B0 );
		if( is_what(-1, deck[0], TYPE_PERMANENT) ){
			int choice = do_dialog(current_turn, player, card, -1, -1, " Put into play\n Pass", 0);
			if( choice == 0 ){
				int card_added = add_card_to_hand(current_turn, deck[0]);
				remove_card_from_deck(current_turn, 0);
				put_into_play(current_turn, card_added);
			}
		}
	}

	return 0;
}

int card_gilded_lotus(int player, int card, event_t event)
{
  // 0x4dbf30

  /* Gilded Lotus	|5
   * Artifact
   * |T: Add three mana of any one color to your mana pool. */
  // Used externally
  return artifact_mana_all_one_color(player, card, event, 3, 0);
}

int card_glimmervoid(int player, int card, event_t event)
{
  /* Glimmervoid	""
   * Land
   * At the beginning of the end step, if you control no artifacts, sacrifice ~.
   * |T: Add one mana of any color to your mana pool. */

  if (trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player
	  && count_permanents_by_type(player, TYPE_ARTIFACT) <= 0 && eot_trigger(player, card, event))
	kill_card(player, card, KILL_SACRIFICE);

  return mana_producer(player, card, event);
}

int card_goblin_charbelcher(int player, int card, event_t event){
	/* Goblin Charbelcher	|4	0x20018b2
	 * Artifact
	 * |3, |T: Reveal cards from the top of your library until you reveal a land card. ~ deals damage equal to the number of nonland cards revealed this way to any target.
	  If the revealed land card was |Ha Mountain, ~ deals double that damage instead. Put the revealed cards on the bottom of your library in any order. */
	if( ! is_unlocked(player, card, event, 12) ){ return 0; }

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int i = 0;
			int dmg = 0;
			if( deck_ptr[player][0] != -1 ){
				while( deck_ptr[player][i] != -1 ){
						int iid = deck_ptr[player][i];
						if( ! is_what(-1, iid, TYPE_LAND) ){
							dmg++;
						}
						else{
							break;
						}
						i++;
				}
				if( deck_ptr[player][i] != -1 ){
					show_deck( HUMAN, deck_ptr[player], i+1, "Cards revealed by Goblin Charbelcher.", 0, 0x7375B0 );
					if( has_subtype_by_id(cards_data[ deck_ptr[player][i] ].id, SUBTYPE_MOUNTAIN) ){
						dmg *= 2;
					}
				}

				if( dmg > 0 ){
					damage_target0(player, card, dmg);
				}

				if( i > 0 ){
					put_top_x_on_bottom(player, player, i+1);
				}
			}
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_ANY");
}

int card_goblin_replica(int player, int card, event_t event){
/* CARD_ID_GOBLIN_REPLICA	6675
Goblin Replica	|3
Artifact Creature - Goblin 2/2
|3|R, Sacrifice ~: Destroy target artifact. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_XR(3, 1), 0, &td, "TARGET_ARTIFACT");
}

int card_golem_skin_gauntlets(int player, int card, event_t event){
/* CARD_ID_GOLEM_SKIN_GAUNTLETS	6678
Golem-Skin Gauntlets	|1
Artifact - Equipment
Equipped creature gets +1/+0 for each Equipment attached to it.
Equip |2 (|2: Attach to target creature you control. Equip only as a sorcery.
This card enters the battlefield unattached and stays on the battlefield if the creature leaves.) */

	if( is_equipping(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_POWER && affect_me(p, c) ){
			event_result += equipments_attached_to_me(p, c, EATM_REPORT_TOTAL);
		}
	}

	return basic_equipment(player, card, event, 2);
}

int card_grab_the_reins(int player, int card, event_t event){
	/* Grab the Reins	|3|R	0x2005269
	 * Instant
	 * Choose one -
	 * * Until end of turn, you gain control of target creature and it gains haste.
	 * * Sacrifice a creature. ~ deals damage equal to that creature's power to any target.
	 * Entwine |2|R */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_AOT = 1,
		CHOICE_SAC_DAMAGE,
		CHOICE_ENTWINE
	};

	if( event == EVENT_CAN_CAST && basic_spell(player, card, event)){
		return can_target(&td) || can_target(&td2) ? 1 : 0;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int can_entwine = can_target(&td) && can_target(&td2) && has_mana_multi(player, MANACOST_XR(2, 1)) ? 1 : 0;
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Act of Treson", can_target(&td), 10,
								"Sac & damage", can_target(&td2), (can_target(&td) || new_can_sacrifice(player, card, player, &test) ? 5 : 1),
								"Entwine", can_entwine,  15);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( instance->info_slot & CHOICE_ENTWINE ){
				charge_mana_multi(player, MANACOST_XR(2, 1));
				if( spell_fizzled == 1){
					return 0;
				}
			}
		}
		int base_target = 0;
		if( instance->info_slot & CHOICE_AOT ){
			if( pick_target(&td, "TARGET_CREATURE") ){
				if( instance->targets[0].player == player ){
					ai_modifier += (is_sick(instance->targets[0].player, instance->targets[0].card) ||
									is_tapped(instance->targets[0].player, instance->targets[0].card) ? 48 : -48);
				}
				if( instance->targets[0].player == 1-player ){
					ai_modifier += (! check_for_ability(instance->targets[0].player, instance->targets[0].card, KEYWORD_DEFENDER) ? 48 : -48);
				}
				base_target++;
			}
		}
		if( instance->info_slot & CHOICE_SAC_DAMAGE ){
			new_pick_target(&td, "TARGET_ANY", base_target, 1);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int base_target = 0;
		if( instance->info_slot & CHOICE_AOT ){
			if( validate_target(player, card, &td, 0) ){
				effect_act_of_treason(player, card, instance->targets[1].player, instance->targets[1].card);
			}
			base_target++;
		}
		if( instance->info_slot & CHOICE_SAC_DAMAGE ){
			if( validate_target(player, card, &td2, base_target) ){
				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_RETURN_CHOICE | SAC_NO_CANCEL, &test);
				if( sac ){
					int pow = get_power(BYTE2(sac), BYTE3(sac));
					kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
					damage_creature(instance->targets[base_target].player, instance->targets[base_target].card, pow, player, card);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_granite_shard(int player, int card, event_t event){
	/* Granite Shard	|3	0x2005269	0x200526e
	 * Artifact
	 * |3, |T or |R, |T: ~ deals 1 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = instance->number_of_targets = 0;
		if( has_mana_for_activated_ability(player, card, MANACOST_R(1)) ){
			if( has_mana_for_activated_ability(player, card, MANACOST_X(3)) ){
				choice = do_dialog(player, player, card, -1, -1, " Pay R\n Pay 3\n Cancel", 0);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 2 ){
			spell_fizzled = 1;
			return 0;
		}
		int red = choice == 0 ? 1 : 0;
		int cless = choice == 1 ? 3 : 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_XR(cless, red)) ){
			if( pick_target(&td, "TARGET_ANY") ){
				add_state(player, card, STATE_TAPPED);
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

int card_grid_monitor(int player, int card, event_t event){
/* CARD_ID_GRID_MONITOR	6681
Grid Monitor	|4
Artifact Creature - Construct 4/6
You can't cast creature spells. */

	if( event == EVENT_MODIFY_COST_GLOBAL && in_play(player, card) && ! is_humiliated(player, card) ){
		if( affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE)){
			infinite_casting_cost();
		}
	}

	return 0;
}

int card_heartwood_shard(int player, int card, event_t event){
/* CARD_ID_HEARTWOOD_SHARD	6684
Heartwood Shard	|3
Artifact
|3, |T or |G, |T: Target creature gains trample until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_G(1), 0, &td, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = instance->number_of_targets = 0;
		if( has_mana_for_activated_ability(player, card, MANACOST_G(1)) ){
			if( has_mana_for_activated_ability(player, card, MANACOST_X(3)) ){
				choice = do_dialog(player, player, card, -1, -1, " Pay G\n Pay 3\n Cancel", 0);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 2 ){
			spell_fizzled = 1;
			return 0;
		}
		int green = choice == 0 ? 1 : 0;
		int cless = choice == 1 ? 3 : 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_XG(cless, green)) ){
			if( pick_target(&td, "TARGET_CREATURE") ){
				add_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_TRAMPLE, 0);
		}
	}

	return 0;
}

static int is_not_x_spell(int unused1, int unused2, int player, int card){
	int iid = get_original_internal_card_id(player, card);
	return cards_data[iid].cc[1] == -1 ? 0 : 1;
}

int card_isochron_scepter(int player, int card, event_t event){
/* Isochron Scepter	|2
 * Artifact
 * Imprint - When ~ enters the battlefield, you may exile an instant card with converted mana cost 2 or less from your hand.
 * |2, |T: You may copy the exiled card. If you do, you may cast the copy without paying its mana cost. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_INSTANT, "Choose an instant card with CMC 2 or less.");
		this_test.cmc = 3;
		this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		this_test.zone = TARGET_ZONE_HAND;
		this_test.special_selection_function = &is_not_x_spell;

		ai_modifier += (check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &this_test) ? 256 : -256);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_INSTANT, "Choose an instant card with CMC 2 or less.");
		this_test.cmc = 3;
		this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		this_test.zone = TARGET_ZONE_HAND;
		if( IS_AI(player) ){
			this_test.special_selection_function = &is_not_x_spell;
		}

		int result = -1;
		if( hand_count[player] > 0 ){
			result = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_VALUE, -1, &this_test);
		}
		if( result > -1 ){
			int pos = instance->info_slot;
			if( pos < 10 ){
				int iid = get_original_internal_card_id(player, result);
				instance->targets[pos].card = iid;
				rfg_card_in_hand(player, result);
				instance->info_slot++;
				create_card_name_legacy(player, card, cards_data[iid].id);
			}
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_NONSICK, MANACOST_X(2), 0, NULL, NULL) ){
			int i;
			for(i=0; i<instance->info_slot; i++){
				int iid = instance->targets[i].card;
				if( check_rfg(player, cards_data[iid].id) ){
					int result = can_legally_play_iid(player, iid);
					if( result == 99 ){
						return 99;
					}
				}
			}
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int found = 0;
		int i;
		for(i=0; i<instance->info_slot; i++){
			int iid = instance->targets[i].card;
			if( check_rfg(player, cards_data[iid].id) && can_legally_play_iid(player, iid) ){
				found = 1;
				break;
			}
		}
		ai_modifier += (found ? 96 : -96);
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_NONSICK, MANACOST_X(2), 0, NULL, NULL);
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		int removed[10];
		int rc = 0;
		int i;
		for(i=0; i<instance->info_slot; i++){
			int iid = instance->targets[i].card;
			if( check_rfg(player, cards_data[iid].id) && can_legally_play_iid(player, iid) ){
				removed[rc] = iid;
				rc++;
			}
		}
		int selected = 0;
		if( rc > 1 ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_INSTANT | TYPE_INTERRUPT, "Select an instant to copy.");
			selected = select_card_from_zone(player, player, removed, rc, 0, AI_MAX_VALUE, -1, &test);
		}
		copy_spell(player, cards_data[removed[selected]].id);
	}

	return 0;
}

int card_journey_of_discovery(int player, int card, event_t event){
/* CARD_ID_JOURNEY_OF_DISCOVERY	6692
Journey of Discovery	|2|G
Sorcery
Choose one -
* Search your library for up to two basic land cards, reveal them, put them into your hand, then shuffle your library.
* You may play up to two additional lands this turn.
Entwine |2|G (Choose both if you pay the entwine cost.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum{
		CHOICE_TUTOR_LANDS = 1,
		CHOICE_ADDITIONAL_LANDS,
		CHOICE_ENTWINE
	};

	if( event == EVENT_CAN_CAST  ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			test_definition_t test;
			default_test_definition(&test, TYPE_LAND);
			test.zone = TARGET_ZONE_HAND;
			int lands_found = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test);
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Tutor lands", 1, 5,
								"Play additional lands", 1, lands_found * 5,
								"Entwine", has_mana_multi(player, MANACOST_XG(2, 1)), 100);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( choice == CHOICE_ENTWINE ){
				charge_mana_multi(player, MANACOST_XG(2, 1));
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->info_slot & CHOICE_TUTOR_LANDS ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Select a basic land card.");
			test.subtype = SUBTYPE_BASIC;
			test.qty = 2;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &test);
		}
		if( instance->info_slot & CHOICE_ADDITIONAL_LANDS ){
			int legacy = create_legacy_effect(player, card, &check_playable_lands_legacy);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[2].card = get_id(player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

// Krark's Thumb effect is in "flip_a_coin" function.

int card_krark_clan_shaman(int player, int card, event_t event){
/* Krark-Clan Shaman	|R
 * Creature - Goblin Shaman 1/1
 * Sacrifice an artifact: ~ deals 1 damage to each creature without flying. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		test_definition_t test2;
		default_test_definition(&test2, TYPE_CREATURE);
		test2.keyword = KEYWORD_FLYING;
		test2.keyword_flag = DOESNT_MATCH;
		new_damage_all(instance->parent_controller, instance->parent_card, ANYBODY, 1, 0, &test2);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
}

int card_leonin_abunas(int player, int card, event_t event){
/* CARD_ID_LEONIN_ABUNAS	6696
Leonin Abunas	|3|W
Creature - Cat Cleric 2/5
Artifacts you control have hexproof. (They can't be the targets of spells or abilities your opponents control.) */

	if( event == EVENT_ABILITIES && in_play(player, card) && ! is_humiliated(player, card) ){
		if( affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) ){
			hexproof(affected_card_controller, affected_card, event);
		}
	}

	return 0;
}

int card_leonin_bladetrap(int player, int card, event_t event){
/* CARD_ID_LEONIN_BLADETRAP	6697
Leonin Bladetrap	|3
Artifact
Flash (You may cast this spell any time you could cast an instant.)
|2, Sacrifice ~: ~ deals 2 damage to each attacking creature without flying. */

	if( IS_GAA_EVENT(event) ){
		if( event == EVENT_RESOLVE_ACTIVATION ){
			test_definition_t test2;
			default_test_definition(&test2, TYPE_CREATURE);
			test2.state = STATE_ATTACKING;
			test2.keyword = KEYWORD_FLYING;
			test2.keyword_flag = DOESNT_MATCH;
			new_damage_all(player, card, ANYBODY, 2, 0, &test2);
		}
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL);
	}

	return flash(player, card, event);
}

int card_leonin_den_guard(int player, int card, event_t event)
{
/* CARD_ID_LEONIN_DEN_GUARD	6698
Leonin Den-Guard	|1|W
Creature - Cat Soldier 1/3
As long as ~ is equipped, it gets +1/+1 and has vigilance. */

	if( (event == EVENT_ABILITIES || event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) &&
	  in_play(player, card) && ! is_humiliated(player, card) && equipments_attached_to_me(player, card, EATM_CHECK) )
	{
		if (event == EVENT_ABILITIES)
			vigilance(player, card, event);
		else
			++event_result;
	}

  return 0;
}

int card_leonin_elder(int player, int card, event_t event){
/* CARD_ID_LEONIN_ELDER	6699
Leonin Elder	|W
Creature - Cat Cleric 1/1
Whenever an artifact enters the battlefield, you may gain 1 life. */

	etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), TYPE_ARTIFACT, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 1);
	}

	return 0;
}

int card_leonin_scimitar(int player, int card, event_t event){
/* CARD_ID_LEONIN_SCIMITAR	6700
Leonin Scimitar	|1
Artifact - Equipment
Equipped creature gets +1/+1.
Equip |1 (|1: Attach to target creature you control. Equip only as a sorcery.) */

	return vanilla_equipment(player, card, event, 1, 1, 1, 0, 0);
}

int card_leonin_sun_standard(int player, int card, event_t event){
/* CARD_ID_LEONIN_SUN_STANDARD	6701
Leonin Sun Standard	|2
Artifact
|1|W: Creatures you control get +1/+1 until end of turn. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1, 1, 1, 0, 0);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XW(1, 1), 0, NULL, NULL);
}

int card_lightning_coils(int player, int card, event_t event){

	/* Lightning Coils	|3
	 * Artifact
	 * Whenever a nontoken creature you control dies, put a charge counter on ~.
	 * At the beginning of your upkeep, if ~ has five or more charge counters on it, remove all of them from it and put that many 3/1 |Sred Elemental creature
	 * tokens with haste onto the battlefield. Exile them at the beginning of the next end step. */

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.type_flag = F1_NO_TOKEN;
		count_for_gfp_ability(player, card, event, player, 0, &test);
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( result ){
		add_counters(player, card, COUNTER_CHARGE, BYTE0(result));
	}

	if( count_counters(player, card, COUNTER_CHARGE) >= 5 ){
		upkeep_trigger_ability(player, card, event, player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			int amount = count_counters(player, card, COUNTER_CHARGE);
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
			token.qty = amount;
			token.pow = 3;
			token.tou = 1;
			token.s_key_plus = SP_KEYWORD_HASTE;
			token.legacy = 1;
			token.special_code_for_legacy = remove_at_eot;
			generate_token(&token);
			remove_all_counters(player, card, COUNTER_CHARGE);
		}
	}

	return 0;
}

int card_lightning_greaves(int player, int card, event_t event){
/* CARD_ID_LIGHTNING_GREAVES	6706
Lightning Greaves	|2
Artifact - Equipment
Equipped creature has haste and shroud. (It can't be the target of spells or abilities.)
Equip |0 */

	return vanilla_equipment(player, card, event, 0, 0, 0, KEYWORD_SHROUD, SP_KEYWORD_HASTE);
}

int card_living_hive(int player, int card, event_t event){
	/* Living Hive	|6|G|G
	 * Creature - Elemental Insect 6/6
	 * Trample
	 * Whenever ~ deals combat damage to a player, put that many 1/1 |Sgreen Insect creature tokens onto the battlefield. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_REPORT_DAMAGE_DEALT);
	if( result ){
		card_instance_t *instance = get_card_instance(player, card);
		int amount = BYTE0(instance->targets[16].player) + BYTE1(instance->targets[16].player);
		instance->targets[16].player = 0;
		generate_tokens_by_id(player, card, CARD_ID_INSECT, amount);
	}

	return 0;
}

int card_lodestone_myr(int player, int card, event_t event){
/* CARD_ID_LODESTONE_MYR	6708
Lodestone Myr	|4
Artifact Creature - Myr 2/2
Trample
Tap an untapped artifact you control: ~ gets +1/+1 until end of turn. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST )
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.illegal_state = TARGET_STATE_TAPPED;

		int num_can_tap = target_available(player, card, &td);
		if (num_can_tap <= 0)
			return 0;

		return generic_shade_amt_can_pump(player, card, 1, 0, MANACOST0, num_can_tap);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;
	td.illegal_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			if( new_pick_target(&td, "Select an untapped artifact you control.", 0, 1 | GS_LITERAL_PROMPT) ){
				tap_card(instance->targets[0].player, instance->targets[0].card);
				instance->number_of_targets = 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade(player, card, event, 0, MANACOST0, 1, 1, 0, 0);
	}

	return 0;
}

int card_loxodon_punisher(int player, int card, event_t event){
/* CARD_ID_LOXODON_PUNISHER	6712
Loxodon Punisher	|3|W
Creature - Elephant Soldier 2/2
~ gets +2/+2 for each Equipment attached to it. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card) ){
		event_result += (equipments_attached_to_me(player, card, EATM_REPORT_TOTAL)*2);
	}

	return 0;
}

int card_loxodon_warhammer(int player, int card, event_t event){
/* CARD_ID_LOXODON_WARHAMMER	6713
Loxodon Warhammer	|3
Artifact - Equipment
Equipped creature gets +3/+0 and has trample and lifelink.
Equip |3 */

	return vanilla_equipment(player, card, event, 3, 3, 0, KEYWORD_TRAMPLE, SP_KEYWORD_LIFELINK);
}

int card_lumengrid_augur(int player, int card, event_t event){
/* CARD_ID_LUMENGRID_AUGUR	6714
Lumengrid Augur	|3|U
Creature - Vedalken Wizard 2/2
|1, |T: Target player draws a card, then discards a card. If that player discards an artifact card this way, untap ~. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		int lands = get_total_available_lands(player);
		ai_modifier += (lands <= 3 || lands > 6 ? 48 : -48);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int p = instance->targets[0].player;

			draw_cards(p, 1);

			if( hand_count[p] > 0 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ANY, "Select a card to discard.");
				if( IS_AI(player) ){
					int lands = get_total_available_lands(player);
					if( lands <= 3 ){
						test.type = TYPE_LAND;
						test.type_flag = DOESNT_MATCH;
					}
					if( lands <= 3 ){
						test.type = TYPE_LAND;
					}
				}
				int selected = new_select_a_card(p, p, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &test);
				if( IS_AI(player) && selected == -1 ){
					test.type = TYPE_ANY;
					selected = new_select_a_card(p, p, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &test);
				}
				int untap = is_what(p, selected, TYPE_ARTIFACT);
				new_discard_card(p, selected, player, 0);
				if( untap ){
					untap_card(instance->parent_controller, instance->parent_card);
				}
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_PLAYER");
}

int card_lumengrid_sentinel(int player, int card, event_t event){
/* CARD_ID_LUMENGRID_SENTINEL	6715
Lumengrid Sentinel	|2|U
Creature - Human Wizard 1/2
Flying
Whenever an artifact enters the battlefield under your control, you may tap target permanent. */

	etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), TYPE_ARTIFACT, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_PERMANENT", 0, 0) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_luminous_angel(int player, int card, event_t event){
	/* Luminous Angel	|4|W|W|W
	 * Creature - Angel 4/4
	 * Flying
	 * At the beginning of your upkeep, you may put a 1/1 |Swhite Spirit creature token with flying onto the battlefield. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SPIRIT, &token);
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		token.key_plus = KEYWORD_FLYING;
		generate_token(&token);
	}

	return 0;
}

int card_march_of_the_machines(int player, int card, event_t event){
/* CARD_ID_MARCH_OF_THE_MACHINES	6719
March of the Machines	|3|U
Enchantment
Each noncreature artifact is an artifact creature with power and toughness each equal to its converted mana cost.
(Equipment that's a creature can't equip a creature.) */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		global_type_change(player, card, event, ANYBODY, TYPE_CREATURE, &test, 16384, 16384, 0, 0, 0);
	}

	return global_enchantment(player, card, event);
}

int card_mask_of_memory(int player, int card, event_t event)
{
  /* Mask of Memory	|2
   * Artifact - Equipment
   * Whenever equipped creature deals combat damage to a player, you may draw two cards. If you do, discard a card.
   * Equip |1 */

  int packets;
  if ((packets = equipped_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_TRIGGER_OPTIONAL)))
	for (; packets > 0; --packets)
	  {
		draw_cards(player, 2);
		discard(player, 0, player);
	  }

  return basic_equipment(player, card, event, 1);
}

int card_mass_hysteria(int player, int card, event_t event)
{
  // All creatures have haste.
  boost_subtype(player, card, event, -1, 0, 0, 0, SP_KEYWORD_HASTE, BCT_INCLUDE_SELF);

  return global_enchantment(player, card, event);
}

int card_megatog(int player, int card, event_t event){
	/* Megatog	|4|R|R
	 * Creature - Atog 3/4
	 * Sacrifice an artifact: ~ gets +3/+3 and gains trample until end of turn. */
	return generic_husk(player, card, event, TYPE_ARTIFACT, 3, 3, KEYWORD_TRAMPLE, 0);
}

int card_mesmeric_orb(int player, int card, event_t event)
{
/* CARD_ID_MESMERIC_ORB	6723
Mesmeric Orb	|2
Artifact
Whenever a permanent becomes untapped, that permanent's controller puts the top card of his or her library into his or her graveyard. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		player_bits[0] |= PB_SEND_EVENT_UNTAP_CARD_TO_ALL;
		player_bits[1] |= PB_SEND_EVENT_UNTAP_CARD_TO_ALL;
		if (event == EVENT_UNTAP_CARD  ){
			mill(affected_card_controller, 1);
		}
	}

	return 0;
}

int card_minds_eye(int player, int card, event_t event)
{
  /* Mind's Eye	|5
   * Artifact
   * Whenever an opponent draws a card, you may pay |1. If you do, draw a card. */

  if (card_drawn_trigger(player, card, event, 1-player, RESOLVE_TRIGGER_AI(player))
	  && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_COLORLESS, 1))
	draw_a_card(player);

  return 0;
}

int card_molder_slug(int player, int card, event_t event){
/* CARD_ID_MOLDER_SLUG	6728
Molder Slug	|3|G|G
Creature - Slug Beast 4/6
At the beginning of each player's upkeep, that player sacrifices an artifact. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");
		new_sacrifice(player, card, current_turn, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	return 0;
}

int card_molten_rain(int player, int card, event_t event){
/* CARD_ID_MOLTEN_RAIN	6729
Molten Rain	|1|R|R
Sorcery
Destroy target land. If that land was nonbasic, ~ deals 2 damage to the land's controller. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
		if( spell_fizzled != 1 ){
			if( count_subtype(instance->targets[0].player, TYPE_LAND, -1) <= 3 ){
				ai_modifier += 24;
			}
			if( ! is_basic_land(instance->targets[0].player, instance->targets[0].card) ){
				ai_modifier += 48;
			}
		}
		return 0;
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int is_not_basic_land = ! is_basic_land(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if( is_not_basic_land ){
				damage_player(instance->targets[0].player, 2, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
}

int card_myr_incubator(int player, int card, event_t event){
/* Myr Incubator	|6
 * Artifact
 * |6, |T, Sacrifice ~: Search your library for any number of artifact cards, exile them, then put that many 1/1 colorless Myr
 *		artifact creature tokens onto the battlefield. Then shuffle your library. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int myr = 0;
		int choice = do_dialog(player, player, card, -1, -1, " Auto mode\n Manual mode", 0);
		int *deck = deck_ptr[player];

		if( choice == 1){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an artifact card.");
			while( deck[0] != -1 ){
					if( new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test) != -1 ){
						myr++;
					}
					else{
						break;
					}
			}
		}
		else{
			int count = count_deck(player)-1;
			while( count > -1 ){
					if( is_what(-1, deck[count], TYPE_ARTIFACT) ){
						rfg_card_in_deck(player, count);
						myr++;
						if( IS_AI(player) && myr == 10 ){
							break;
						}
					}
					count--;
			}
		}
		generate_tokens_by_id(player, card, CARD_ID_MYR, myr);
		shuffle(player);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(6), 0, NULL, NULL);
}

int card_myr_retriver(int player, int card, event_t event){
/* CARD_ID_MYR_RETRIEVER	6736
Myr Retriever	|2
Artifact Creature - Myr 1/1
When ~ dies, return another target artifact card from your graveyard to your hand. */

// Same code as Junk Diver
  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact card.");
	  tutor_from_grave_to_hand_except_for_dying_card(player, card, 1, AI_MAX_CMC, &test);
	}

	return 0;
}

int card_necrogen_mists(int player, int card, event_t event){
/* CARD_ID_NECROGEN_MISTS	6737
Necrogen Mists	|2|B
Enchantment
At the beginning of each player's upkeep, that player discards a card. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		discard(current_turn, 0, player);
	}

	return global_enchantment(player, card, event);
}

int card_necrogen_spellbomb(int player, int card, event_t event){
/* CARD_ID_NECROGEN_SPELLBOMB	6738
Necrogen Spellbomb	|1
Artifact
|B, Sacrifice ~: Target player discards a card.
|1, Sacrifice ~: Draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	enum{
		CHOICE_DRAW_CARD = 1,
		CHOICE_DISCARD
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return spellbomb(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0,
						1,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_B(1), 0,
													&td, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Sac & draw", 	abils[1], 14 - (hand_count[player]*2),
							"Sac & force discard", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_DRAW_CARD ){
			return spellbomb(player, card, event);
		}
		if( choice == CHOICE_DISCARD ){
			return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_B(1), 0,
											&td, "TARGET_PLAYER");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DRAW_CARD ){
			draw_cards(player, 1);
		}
		if( instance->info_slot == CHOICE_DISCARD ){
			if( valid_target(&td) ){
				discard(instance->targets[0].player, 0, player);
			}
		}
	}

	return 0;
}

int card_needlebug(int player, int card, event_t event){
/* CARD_ID_NEEDLEBUG	6739
Needlebug	|4
Artifact Creature - Insect 2/2
Flash
Protection from artifacts */

	return flash(player, card, event);
}

int card_neurok_familiar(int player, int card, event_t event){
/* CARD_ID_NEUROK_FAMILIAR	6740
Neurok Familiar	|1|U
Creature - Bird 1/1
Flying
When ~ enters the battlefield, reveal the top card of your library. If it's an artifact card, put it into your hand. Otherwise, put it into your graveyard.*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int iid = deck_ptr[player][0];
		if( iid > -1 ){
			reveal_card_iid(player, card, iid);
			if( is_what(-1, iid, TYPE_ARTIFACT) ){
				add_card_to_hand(player, iid);
				remove_card_from_deck(player, 0);
			}
			else{
				mill(player, 1);
			}
		}
	}

	return 0;
}

int card_nightmare_lash(int player, int card, event_t event){
/* Nightmare Lash	|4
 * Artifact - Equipment
 * Equipped creature gets +1/+1 for each |H2Swamp you control.
 * Equip-Pay 3 life. */

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( (event== EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			event_result += count_subtype(player, TYPE_PERMANENT, get_hacked_subtype(player, card, SUBTYPE_SWAMP));
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( basic_equipment(player, card, event, 0) ){
			if( can_pay_life(player, 3) ){
				return 1;
			}
			if( metalcraft(player, card) && check_battlefield_for_id(player, CARD_ID_PURESTEEL_PALADIN) ){
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		enum{
			CHOICE_PAY_3_LIFE = 1,
			CHOICE_USE_PA_ABILITY
		};
		int choice = metalcraft(player, card) && check_battlefield_for_id(player, CARD_ID_PURESTEEL_PALADIN) ?
					CHOICE_USE_PA_ABILITY : CHOICE_PAY_3_LIFE;
		if( choice == CHOICE_USE_PA_ABILITY && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Pay 3 life", can_pay_life(player, 3), 1,
							"Pay 0", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_PAY_3_LIFE ){
			ai_modifier += (life[player]-3 < 6 ? -96 : -24);
			lose_life(player, 3);
		}
		if( choice == CHOICE_USE_PA_ABILITY ){
			charge_mana_for_activated_ability(player, card, MANACOST0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return basic_equipment(player, card, event, 0);
	}

	return 0;
}

int card_nim_devourer(int player, int card, event_t event){
/*
Nim Devourer |3|B|B
Creature � Zombie 4/1
Nim Devourer gets +1/+0 for each artifact you control.
{B}{B}: Return Nim Devourer from your graveyard to the battlefield, then sacrifice a creature.
		Activate this ability only during your upkeep.
*/
	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		if( has_mana(player, COLOR_BLACK, 2) ){
			int ai_choice = count_subtype(player, TYPE_CREATURE, -1) > 0 ? 0 : 1;
			int choice = do_dialog(player, player, card, -1, -1," Return Nim Devourer to play\n Pass", ai_choice);
			if( choice == 0 ){
				charge_mana_while_resolving(player, card, event, player, COLOR_BLACK, 2);
				if( spell_fizzled != 1){
					remove_state(player, card, STATE_INVISIBLE);
					++hand_count[player];
					put_into_play(player, card);
					controller_sacrifices_a_creature(player, card);
					return -1;
				}
			}
		}
		return -2;
	}

	return nims(player, card, event);
}

int card_nim_lasher(int player, int card, event_t event){
/* CARD_ID_NIM_LASHER	6745
Nim Lasher	|2|B
Creature - Zombie 1/1
~ gets +1/+0 for each artifact you control. */

	return nims(player, card, event);
}

int card_nim_replica(int player, int card, event_t event){
/* CARD_ID_NIM_REPLICA	6746
Nim Replica	|3
Artifact Creature - Zombie 3/1
|2|B, Sacrifice ~: Target creature gets -1/-1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -1, -1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XB(2, 1), 0, &td, "TARGET_CREATURE");
}

int card_nim_shambler(int player, int card, event_t event){
/* CARD_ID_NIM_SHAMBLER	6747
Nim Shambler	|2|B|B
Creature - Zombie 2/1
~ gets +1/+0 for each artifact you control.
Sacrifice a creature: Regenerate ~. */

	if( IS_GAA_EVENT(event) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

		if( event == EVENT_CAN_ACTIVATE ){
			if(generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0,
															         NULL, 0, 0, &test, 0)){
				if( IS_AI(player) ){
					test.not_me = 1;
				}
			    return can_regenerate(player, card);
		    }
		}
		if( event == EVENT_ACTIVATE ){
			if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
				state_untargettable(player, card, 1);
				if( ! new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
					spell_fizzled = 1;
				}
				state_untargettable(player, card, 0);
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			return regeneration(player, card, event, MANACOST0);
		}
	}

	return nims(player, card, event);
}

int card_nuisance_engine(int player, int card, event_t event){
	/* Nuisance Engine	|3
	 * Artifact
	 * |2, |T: Put a 0/1 colorless Pest artifact creature token onto the battlefield. */

	// original code : 004DAC3A

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_PEST);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL);
}

void oblivion_stone_effect(void)
{
	int p, c;
	for (p = 0; p < 2; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
			if (in_play(p, c) && is_what(p, c, TYPE_PERMANENT) && !is_what(p, c, TYPE_LAND) && count_counters(p, c, COUNTER_FATE) == 0)
				kill_card(p, c, KILL_DESTROY);

	regenerate_or_graveyard_triggers();	// regenerate_or_graveyard_triggers()

	for (p = 0; p < 2; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
			if (in_play(p, c) && is_what(p, c, TYPE_PERMANENT) && !is_what(p, c, TYPE_LAND))
				remove_all_counters(p, c, COUNTER_FATE);
}

int card_oblivion_stone(int player, int card, event_t event){
/* Oblivion Stone	|3
 * Artifact
 * |4, |T: Put a fate counter on target permanent.
 * |5, |T, Sacrifice ~: Destroy each nonland permanent without a fate counter on it, then remove all fate counters from all permanents. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );
	if( IS_AI(player) ){
		td.illegal_type = TYPE_LAND;
	}
	td.preferred_controller = player;

	enum{
		CHOICE_FATE_COUNTER = 1,
		CHOICE_DESTROY_ALL
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(4), 0, &td, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(5), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(4), 0,
													&td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(5), 0,
												NULL, NULL)
		};
		int priority_destroy_all = 0;
		if( abils[2] ){
			CYCLE_ALL_CARDS(p, c, 	{
										if( in_play(p, c) && is_what(p, c, TYPE_PERMANENT) && ! is_what(p, c, TYPE_LAND) ){
											if( p == player ){
												if( !(p == player && c == card) && count_counters(p, c, COUNTER_FATE) &&
													! check_for_special_ability(p, c, SP_KEYWORD_INDESTRUCTIBLE) )
												{
													priority_destroy_all -= (get_cmc(p, c)*2);
												}
											}
											else{
												if( ! count_counters(p, c, COUNTER_FATE) &&
													! check_for_special_ability(p, c, SP_KEYWORD_INDESTRUCTIBLE) )
												{
													priority_destroy_all += (get_cmc(p, c)*2);
												}
											}
										}
									};
			);
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Add a Fate counter", abils[1], 10,
							"Destroy nonland permanents", abils[2], priority_destroy_all);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_FATE_COUNTER ){
			ai_modifier += (current_turn == player ? -24 : 24);
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(4), 0, &td, "TARGET_PERMANENT");
		}
		if( choice == CHOICE_DESTROY_ALL ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(5), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_FATE_COUNTER ){
			if( valid_target(&td) ){
				add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_FATE);
			}
		}
		if( instance->info_slot == CHOICE_DESTROY_ALL ){
			oblivion_stone_effect();
		}
	}

	return 0;
}

int card_one_dozen_eyes(int player, int card, event_t event){
	/* One Dozen Eyes	|5|G
	 * Sorcery
	 * Choose one -
	 * * Put a 5/5 |Sgreen Beast creature token onto the battlefield.
	 * * Put five 1/1 |Sgreen Insect creature tokens onto the battlefield.
	 * Entwine |G|G|G */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_BEAST = 1,
		CHOICE_INSECT,
		CHOICE_ENTWINE
	};

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"One 5/5 Beast", 1, 5,
								"5 1/1 Insects", 1, 5,
								"Entwine", has_mana(player, COLOR_GREEN, 3), 15);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( choice == CHOICE_ENTWINE ){
				charge_mana(player, COLOR_GREEN, 3);
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot & CHOICE_BEAST ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_BEAST, &token);
			token.pow = token.tou = 5;
			generate_token(&token);
		}
		if( instance->info_slot &  CHOICE_INSECT ){
			generate_tokens_by_id(player, card, CARD_ID_INSECT, 5);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_override(int player, int card, event_t event){
	/* Override	|2|U
	 * Instant
	 * Counter target spell unless its controller pays |1 for each artifact you control. */

	if(event == EVENT_RESOLVE_SPELL ){
		int amount = count_subtype(player, TYPE_ARTIFACT, -1);
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, amount);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

int card_pearl_shard(int player, int card, event_t event){
	/* Pearl Shard	|3	0x2005322
	 * Artifact
	 * |3, |T or |W, |T: Prevent the next 2 damage that would be dealt to any target this turn. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ANY);
	td.extra = damage_card;

	if( event == EVENT_CAN_ACTIVATE ){
		if( has_mana_for_activated_ability(player, card, MANACOST_W(1)) ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST0, 0,
										&td, NULL);
		}
			if( has_mana_for_activated_ability(player, card, MANACOST_X(3)) ){
		        return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST0, 0,
										&td, NULL);
			}
	}

	if( event == EVENT_ACTIVATE ){
		int choice = instance->number_of_targets = 0;
		if( has_mana_for_activated_ability(player, card, MANACOST_W(1)) ){
			if( has_mana_for_activated_ability(player, card, MANACOST_X(3)) ){
				choice = do_dialog(player, player, card, -1, -1, " Pay W\n Pay 3\n Cancel", 0);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 2 ){
			spell_fizzled = 1;
			return 0;
		}
		int white = choice == 0 ? 1 : 0;
		int cless = choice == 1 ? 3 : 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_XW(cless, white)) ){
			if( pick_target(&td, "TARGET_DAMAGE") ){
				add_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 2);
		}
	}

	return 0;
}

int card_predators_strike(int player, int card, event_t event){
	/* Awaken the Bear	|2|G
	 * Instant
	 * Target creature gets +3/+3 and gains trample until end of turn. */
	/* Crash the Ramparts	|2|G
	 * Instant
	 * Target creature gets +3/+3 and gains trample until end of turn. */
	/* Predator's Strike	|1|G
	 * Instant
	 * Target creature gets +3/+3 and gains trample until end of turn. */
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 3, 3, KEYWORD_TRAMPLE, 0);
}

int card_promise_of_power(int player, int card, event_t event){
	/* Promise of Power	|2|B|B|B
	 * Sorcery
	 * Choose one -
	 * * You draw five cards and you lose 5 life.
	 * * Put an X/X |Sblack Demon creature token with flying onto the battlefield, where X is the number of cards in your hand.
	 * Entwine |4 */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_DRAW_5 = 1,
		CHOICE_DEMON,
		CHOICE_ENTWINE
	};

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Draw 5 & lose 5 life", 	1, 14-(hand_count[player]*2),
								"Create an X/X Demon", 		1, hand_count[player]*2,
								"Entwine", 					has_mana(player, COLOR_ANY, 4), 100);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( choice == CHOICE_ENTWINE ){
				charge_mana(player, COLOR_ANY, 4);
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot & CHOICE_DRAW_5 ){
			draw_cards(player, 5);
			lose_life(player, 5);
		}
		if( instance->info_slot & CHOICE_DEMON ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_DEMON, &token);
			token.pow = hand_count[player];
			token.tou = hand_count[player];
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_proteus_staff(int player, int card, event_t event){
/*
Proteus Staff |3
Artifact
{2}{U}, {T}: Put target creature on the bottom of its owner's library. That creature's controller reveals cards from the top of his or
	her library until he or she reveals a creature card. The player puts that card onto the battlefield and the rest on the bottom of
	his or her library in any order. Activate this ability only any time you could cast a sorcery.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = ANYBODY;

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XU(2, 1), 0,
									&td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			if( instance->targets[0].player == player ){
				ai_modifier += (is_token(instance->targets[0].player, instance->targets[0].card) ? 48 : 24);
			}
			if( instance->targets[0].player == 1-player ){
				ai_modifier += (get_cmc(instance->targets[0].player, instance->targets[0].card) >= 4 ? 24 : -24);
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			put_on_bottom_of_deck(instance->targets[0].player, instance->targets[0].card);

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			reveal_card_until_you_reveal_test(player, instance->targets[0].player, &this_test,
												RCUYRT_PUT_TEST_INTO_PLAY | RCUYRT_PUT_REST_ON_BOTTOM);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XU(2, 1), 0,
									&td, "TARGET_CREATURE");
}

int card_psychic_membrane(int player, int card, event_t event){
/*
Psychic Membrane |2|U
Creature � Wall 0/3, 2U (3)
Defender (This creature can't attack.)
Whenever Psychic Membrane blocks, you may draw a card.
*/

	if( event == EVENT_CAN_ACTIVATE && get_card_instance( player, card )->targets[1].player != 66 ){
		if( current_turn == 1-player && current_phase == PHASE_AFTER_BLOCKING && get_card_instance( player, card )->blocking < 255 )
		{
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		get_card_instance( player, card )->targets[1].player = 66;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return 0;
}

// Psychogenic Probe code is in "shuffle()" in "functions.c"

int card_pyrite_spellbomb(int player, int card, event_t event){
	/* Pyrite Spellbomb	|1	0x2005345
	 * Artifact
	 * |R, Sacrifice ~: It deals 2 damage to any target.
	 * |1, Sacrifice ~: Draw a card. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	enum{
		CHOICE_DRAW_CARD = 1,
		CHOICE_DAMAGE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return spellbomb(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0,
						1,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_R(1), 0,
													&td, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Sac & draw", 	abils[1], 14 - (hand_count[player]*2),
							"Sac & damage", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_DRAW_CARD ){
			return spellbomb(player, card, event);
		}
		if( choice == CHOICE_DAMAGE ){
			return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_R(1), 0,
											&td, "TARGET_ANY");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DRAW_CARD ){
			draw_cards(player, 1);
		}
		if( instance->info_slot == CHOICE_DAMAGE ){
			if( valid_target(&td) ){
				damage_target0(player, card, 2);
			}
		}
	}

	return 0;
}

static int quicksilver_fountain_effect(int player, int card, event_t event)
{
	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		int qf_p = instance->targets[0].player;
		int qf_c = instance->targets[0].card;
		if( ! count_counters(p, c, COUNTER_FLOOD) ){
			kill_card(player, card, KILL_EXILE);
		}
		if( in_play(qf_p, qf_c) && ! is_humiliated(qf_p, qf_c) ){
			if( event == EVENT_STATIC_EFFECTS ){
				if( instance->targets[1].card != get_hacked_subtype(qf_p, qf_c, SUBTYPE_ISLAND) ){
					instance->targets[1].card = get_hacked_subtype(qf_p, qf_c, SUBTYPE_ISLAND);
					int orig_id = get_original_internal_card_id(p, c);
					instance->dummy3 = land_into_new_basic_land_type(orig_id, instance->targets[1].card);
				}
			}
			if( event == EVENT_CHANGE_TYPE && affect_me(p, c) ){
				if( instance->dummy3 >= 0 ){
					event_result = instance->dummy3;
				}
			}
		}
	}

	return 0;
}

int card_quicksilver_fountain(int player, int card, event_t event)
{
/* Quicksilver Fountain	|3
 * Artifact
 * At the beginning of each player's upkeep, that player puts a flood counter on target non-|H2Island land he or she controls of his or
 * 		her choice. That land is |Han Island for as long as it has a flood counter on it.
 * At the beginning of each end step, if all lands on the battlefield are |H1Islands, remove all flood counters from them. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.allowed_controller = current_turn;
		td.preferred_controller = current_turn;
		td.who_chooses = current_turn;
		td.allow_cancel = 0;
		td.required_subtype = get_hacked_subtype(player, card, SUBTYPE_ISLAND);
		td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) &&
			new_pick_target(&td, get_hacked_land_text(player, card, "Select target non-%s land.", SUBTYPE_ISLAND), 0, GS_LITERAL_PROMPT) )
		{
			add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_FLOOD);
			int legacy = create_targetted_legacy_effect(player, card, &quicksilver_fountain_effect,
														instance->targets[0].player, instance->targets[0].card);
			card_instance_t* leg = get_card_instance(player, legacy);
			leg->targets[0].player = player;
			leg->targets[0].card = card;
			leg->number_of_targets = 1;
			leg->targets[1].card = get_hacked_subtype(player, card, SUBTYPE_ISLAND);
			int orig_id = get_original_internal_card_id(instance->targets[0].player, instance->targets[0].card);
			leg->dummy3 = land_into_new_basic_land_type(orig_id, get_hacked_subtype(player, card, SUBTYPE_ISLAND));
		}
		instance->number_of_targets = 0;
	}

	if (trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player)
	{
		int island_subtype = get_hacked_subtype(player, card, SUBTYPE_ISLAND);
		int p, c;
		for (p = 0; p <= 1; ++p)
			for (c = 0; c < active_cards_count[p]; ++c)
				if (in_play(p, c) && is_what(p, c, TYPE_LAND) && !has_subtype(p, c, island_subtype))
					return 0;

		if (eot_trigger(player, card, event))
		{
			// Remove all Quicksilver Fountain effect cards, so they don't all call recalculate_all_cards_in_play() during static effects
			card_instance_t* inst;
			for (p = 0; p <= 1; ++p){
				for (c = 0; c < active_cards_count[p]; ++c){
					if ((inst = in_play(p, c)) && is_what(p, c, TYPE_EFFECT) && inst->info_slot == (int)quicksilver_fountain_effect ){
						kill_card(p, c, KILL_EXILE);
					}
				}
			}

			// Remove all flood counters
			for (p = 0; p <= 1; ++p){
				for (c = 0; c < active_cards_count[p]; ++c){
					if (in_play(p, c) && is_what(p, c, TYPE_LAND)){
						remove_all_counters(p, c, COUNTER_FLOOD);
					}
				}
			}

			recalculate_all_cards_in_play();	// Likely redundant
		}
	}

	// Update all existing legacies with this Quicksilver Fountain
	if (event == EVENT_RESOLVE_SPELL){
		int p, c;
		card_instance_t* inst;
		for (p = 0; p <= 1; ++p){
			for (c = 0; c < active_cards_count[p]; ++c){
				if ((inst = in_play(p, c)) && is_what(p, c, TYPE_EFFECT) && inst->info_slot == (int)quicksilver_fountain_effect ){
					inst->targets[0].player = player;
					inst->targets[0].card = card;
				}
			}
		}
	}

	return 0;
}

int card_raise_the_alarm(int player, int card, event_t event){
/* CARD_ID_RAISE_THE_ALARM	1450
Raise the Alarm	|1|W
Instant
Create two 1/1 |Swhite Soldier creature tokens. */

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_SOLDIER, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_razor_barrier(int player, int card, event_t event){

	/* Razor Barrier	|1|W
	 * Instant
	 * Target permanent you control gains protection from artifacts or from the color of your choice until end of turn. */

	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int choice = do_dialog(player, player, card, -1, -1,
									" Prot. from Black\n Prot. from Blue\n Prot. from Green\n Prot from Red\n Prot. from White\n Prot from Artifacts",
									get_deck_color(player, 1-player) - 1);
			int key = (1 << (11+choice)) | KEYWORD_RECALC_SET_COLOR;
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, key, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_reiver_demon(int player, int card, event_t event){
/* CARD_ID_REIVER_DEMON	6768
Reiver Demon	|4|B|B|B|B
Creature - Demon 6/6
Flying
When ~ enters the battlefield, if you cast it from your hand, destroy all nonartifact, non|Sblack creatures. They can't be regenerated. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->info_slot = 1 * not_played_from_hand(player, card);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( get_card_instance(player, card)->info_slot != 1 ){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			test.type_flag = F1_NONARTIFACT_CREATURE;
			test.color = get_sleighted_color_test(player, card,  COLOR_TEST_BLACK);
			test.color = DOESNT_MATCH;
			APNAP(p, { new_manipulate_all(player, card, p, &test, KILL_BURY); };);
		}
	}

	return 0;
}

int card_roar_of_the_kha(int player, int card, event_t event){
/* CARD_ID_ROAR_OF_THE_KHA	6770
Roar of the Kha	|1|W
Instant
Choose one -
* Creatures you control get +1/+1 until end of turn.
* Untap all creatures you control.
Entwine |1|W (Choose both if you pay the entwine cost.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_PUMP = 1,
		CHOICE_UNTAP,
		CHOICE_ENTWINE
	};

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Pump your dudes", 1, 5,
								"Untap your dudes", 1, 5,
								"Entwine", has_mana_multi(player, MANACOST_XW(1, 1)), 15);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( choice == CHOICE_ENTWINE ){
				charge_mana_multi(player, MANACOST_XW(1, 1));
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot & CHOICE_PUMP ){
			pump_subtype_until_eot(player, card, player, -1, 1, 1, 0, 0);
		}
		if( instance->info_slot & CHOICE_UNTAP ){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			new_manipulate_all(player, card, player, &test, ACT_UNTAP);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_rust_elemental(int player, int card, event_t event)
{
  /* Rust Elemental	|4
   * Artifact Creature - Elemental 4/4
   * Flying
   * At the beginning of your upkeep, sacrifice an artifact other than ~. If you can't, tap ~ and you lose 4 life. */

  upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_ARTIFACT, "");
	  test.not_me = 1;

	  if (!new_sacrifice(player, card, player, SAC_NO_CANCEL, &test))
		{
		  tap_card(player, card);
		  lose_life(player, 4);
		}
	}

  return 0;
}

int card_rustmouth_ogre(int player, int card, event_t event){
/*
Rustmouth Ogre |4|R|R
Creature � Ogre 5/4
Whenever Rustmouth Ogre deals combat damage to a player, you may destroy target artifact that player controls.
*/

	int result = whenever_i_deal_damage_to_a_player(player, card, event, RESOLVE_TRIGGER_AI(player) | DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					while( times_damaged[p] ){
							target_definition_t td;
							default_target_definition(player, card, &td, TYPE_ARTIFACT );
							td.allowed_controller = p;
							td.preferred_controller = p;

							card_instance_t *instance = get_card_instance(player, card);

							if( can_target(&td) ){
								char msg[100];
								if( p == player ){
									scnprintf(msg, 100, "Select target artifact you control.");
								}
								else{
									scnprintf(msg, 100, "Select target artifact your opponent controls.");
								}
								if( new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
									kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
								}
							}
							instance->number_of_targets = 0;
							times_damaged[p]--;
					}
				};
		);
	}

	return 0;
}

int card_sculpting_steel(int player, int card, event_t event)
{
  /* Sculpting Steel	|3
   * Artifact
   * You may have ~ enter the battlefield as a copy of any artifact on the battlefield. */

  target_definition_t td;
  if (event == EVENT_RESOLVE_SPELL)
	base_target_definition(player, card, &td, TYPE_ARTIFACT);

  enters_the_battlefield_as_copy_of_any(player, card, event, &td, "SELECT_AN_ARTIFACT");

  return 0;
}

int card_scythe_of_the_wretched(int player, int card, event_t event){
/*
Scythe of the Wretched |2
Artifact � Equipment
Equipped creature gets +2/+2.
Whenever a creature dealt damage by equipped creature this turn dies, return that card to the battlefield under your control.
	Attach Scythe of the Wretched to that creature.
Equip {4}
*/

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int e_player = instance->damage_target_player;
		int e_card = instance->damage_target_card;

		if( event == EVENT_DEAL_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_source_card == e_card && damage->damage_source_player == e_player &&
					damage->damage_target_card != -1 && damage->info_slot > 0 &&
					is_what(damage->damage_target_player, damage->damage_target_card, TYPE_CREATURE) )
				{
					if( instance->info_slot < 1 ){
						instance->info_slot = 1;
					}
					int pos = instance->info_slot;
					if( pos < 8 ){
						instance->targets[pos].player = damage->damage_target_player;
						instance->targets[pos].card = damage->damage_target_card;
						instance->info_slot++;
					}
				}
			}
		}

		if( event == EVENT_GRAVEYARD_FROM_PLAY ){
			if( in_play(affected_card_controller, affected_card) &&
				is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
				! is_token(affected_card_controller, affected_card) && instance->info_slot > 1 )
			{
				if( is_this_dying(affected_card_controller, affected_card) ){
					int i;
					for(i=1; i<instance->info_slot; i++){
						if( instance->targets[i].player == affected_card_controller &&
							instance->targets[i].card == affected_card
						  ){
							if( instance->targets[11].player < 0 ){
								instance->targets[11].player = 0;
							}
							instance->targets[11].player++;
							instance->targets[i].player |= (1<<20);
							instance->targets[i].card = get_original_internal_card_id(affected_card_controller, affected_card);
							break;
						}
					}
				}
			}
		}

		if( instance->targets[11].player > 0 && trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY &&
			player == reason_for_trigger_controller && affect_me(player, card) )
		{
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						int k;
						for(k=1; k<8; k++){
							if( instance->targets[k].player > -1 && (instance->targets[k].player & (1<<20)) ){
								int owner = (instance->targets[k].player &= ~(1<<20));
								int csvid = cards_data[instance->targets[k].card].id;
								int result = seek_grave_for_id_to_reanimate(player, -1, owner, csvid, REANIMATE_DEFAULT);
								if( result > -1 ){
									equip_target_creature(player, card, player, result);
								}
							}
						}
						instance->targets[11].player = 0;
				}
		}

		if( event == EVENT_CLEANUP ){
			instance->info_slot = 1;
		}
	}

	return vanilla_equipment(player, card, event, 4, 2, 2, 0, 0);
}

int card_second_sunrise(int player, int card, event_t event){
/* CARD_ID_SECOND_SUNRISE	6778
Second Sunrise	|1|W|W
Instant
Each player returns to the battlefield all artifact, creature, enchantment, and land cards in his or her graveyard that were put there from the battlefield this turn.*/

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p, { reanimate_all_dead_this_turn(p, TYPE_PERMANENT | GDC_NONTOKEN | GDC_NONPLANESWALKER); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_seething_song(int player, int card, event_t event){
/*
Seething Song |2|R
Instant
Add {R}{R}{R}{R}{R} to your mana pool.
*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (hand_count[player] * 12);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		produce_mana(player, COLOR_RED, 5);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_serum_tank(int player, int card, event_t event){
/* Serum Tank	|3
 * Artifact
 * Whenever ~ or another artifact enters the battlefield, put a charge counter on ~.
 * |3, |T, Remove a charge counter from ~: Draw a card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if (trigger_condition == TRIGGER_COMES_INTO_PLAY){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ARTIFACT, "");
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ARTIFACT, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		add_counter(player, card, COUNTER_CHARGE);
	}

	if( event == EVENT_ACTIVATE ){
		if( count_subtype(player, -1, TYPE_LAND) >= 6 ){
			ai_modifier += (current_turn == player ? 24 : 48);
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION){
		draw_a_card(player);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), GVC_COUNTER(COUNTER_CHARGE), NULL, NULL);
}

int card_shrapnel_blast(int player, int card, event_t event){
	/* Shrapnel Blast	|1|R	0x200cdde
	 * Instant
	 * As an additional cost to cast this spell, sacrifice an artifact.
	 * ~ deals 5 damage to any target. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL) ){
			return can_sacrifice_type_as_cost(player, 1, TYPE_ARTIFACT);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->number_of_targets = 0;
		int sac = -1;
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			sac = new_sacrifice(player, card, player, SAC_AS_COST | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
			if( ! sac ){
				spell_fizzled = 1;
				return 0;
			}
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
		}
		if( pick_target(&td, "TARGET_ANY") && sac ){
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 5);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_skeleton_shard(int player, int card, event_t event){
/* CARD_ID_SKELETON_SHARD	6781
Skeleton Shard	|3
Artifact
|3, |T or |B, |T: Return target artifact creature card from your graveyard to your hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select an Artifact Creature.");
	test.special_selection_function = &is_artifact_creature_by_internal_id;

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST_B(1), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER) )
		{
			return 1;
		}
		return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL,
																0, &test, GS_GRAVE_RECYCLER);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = instance->number_of_targets = 0;
		if( has_mana_for_activated_ability(player, card, MANACOST_B(1)) ){
			if( has_mana_for_activated_ability(player, card, MANACOST_X(3)) ){
				choice = do_dialog(player, player, card, -1, -1, " Pay B\n Pay 3\n Cancel", 0);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 2 ){
			spell_fizzled = 1;
			return 0;
		}
		int black = choice == 0 ? 1 : 0;
		int cless = choice == 1 ? 3 : 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_XB(cless, black)) ){
			if( select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &test, 0) ){
				add_state(player, card, STATE_TAPPED);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}

	return 0;
}

int card_slith_bloodletter(int player, int card, event_t event){
/* CARD_ID_SLITH_BLOODLETTER	1477
Slith Bloodletter	|B|B
Creature - Slith 1/1
Whenever ~ deals combat damage to a player, put a +1/+1 counter on it.
|1|B: Regenerate ~. */

	if( IS_GAA_EVENT(event) ){
		return regeneration(player, card, event, MANACOST_XB(1, 1));
	}

	return card_slith_predator(player, card, event);
}

int card_slith_firewalker(int player, int card, event_t event){
/* CARD_ID_SLITH_FIREWALKER	1476
Slith Firewalker	|R|R
Creature - Slith 1/1
Haste
Whenever ~ deals combat damage to a player, put a +1/+1 counter on it. */

	haste(player, card, event);

	return card_slith_predator(player, card, event);
}

int card_slith_predator(int player, int card, event_t event){
/* CARD_ID_SLITH_PREDATOR	1475
Slith Predator	|G|G
Creature - Slith 1/1
Trample
Whenever ~ deals combat damage to a player, put a +1/+1 counter on it. */

	// Used externally
	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		add_1_1_counters(player, card, amount);
	}

	return 0;
}

int card_slith_strider(int player, int card, event_t event){
/* CARD_ID_SLITH_STRIDER	6785
Slith Strider	|1|U|U
Creature - Slith 1/1
Whenever ~ becomes blocked, draw a card.
Whenever ~ deals combat damage to a player, put a +1/+1 counter on it. */

	if( current_turn == player && current_phase == PHASE_DECLARE_BLOCKERS && event == EVENT_DECLARE_BLOCKERS &&
		is_attacking(player, card) && ! is_unblocked(player, card) )
	{
		draw_cards(player, 1);
	}

	if( event == EVENT_ATTACK_RATING && affect_me(player, card)){
		ai_defensive_modifier -= 24;
	}

	return card_slith_predator(player, card, event);
}

int card_solar_tide(int player, int card, event_t event){
/*
Solar Tide |4|W|W
Sorcery
Choose one �
� Destroy all creatures with power 2 or less.
� Destroy all creatures with power 3 or greater.
Entwine�Sacrifice two lands. (Choose both if you pay the entwine cost.)
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_POW_2_OR_LESS = 1,
		CHOICE_POW_3_OR_GREATER,
		CHOICE_ENTWINE
	};

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int priorities[4] = {0};
			CYCLE_ALL_CARDS(p, c, 	{
										if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
											if( p == player ){
												int cmc = get_cmc(p, c);
												priorities[1] -= (get_power(p, c) <= 2 ? cmc * 2 : 0);
												priorities[2] -= (get_power(p, c) >= 3 ? cmc * 2 : 0);
												priorities[3] -= cmc;
											}
											else{
												int cmc = get_cmc(p, c);
												priorities[1] += (get_power(p, c) <= 2 ? cmc * 2 : 0);
												priorities[2] += (get_power(p, c) >= 3 ? cmc * 2 : 0);
												priorities[3] += cmc;
											}
										}
									}
			);
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Wipe power <= 2", 1, 1,
								"Wipe power >= 3", 1, 1,
								"Entwine", new_can_sacrifice_as_cost(player, card, &test), 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( choice == CHOICE_ENTWINE ){
				int sacs[2];
				int i;
				for(i=0; i<2; i++){
					sacs[i] = new_sacrifice(player, card, player, SAC_AS_COST | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
					if( ! sacs[i] ){
						spell_fizzled = 1;
						return 0;
					}
				}
				for(i=0; i<2; i++){
					state_untargettable(BYTE2(sacs[i]), BYTE3(sacs[i]), 0);
					kill_card(BYTE2(sacs[i]), BYTE3(sacs[i]), KILL_SACRIFICE);
				}
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		if( instance->info_slot != CHOICE_ENTWINE ){
			if( instance->info_slot & CHOICE_POW_2_OR_LESS ){
				test.power = 3;
				test.power_flag = F5_POWER_LESSER_THAN_VALUE;
			}
			if( instance->info_slot & CHOICE_POW_3_OR_GREATER ){
				test.power = 2;
				test.power_flag = F5_POWER_GREATER_THAN_VALUE;
			}
		}
		APNAP(p, { new_manipulate_all(player, card, p, &test, KILL_DESTROY); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_soldier_replica(int player, int card, event_t event){
/* CARD_ID_SOLDIER_REPLICA	6787
Soldier Replica	|3
Artifact Creature - Soldier 1/3
|1|W, Sacrifice ~: ~ deals 3 damage to target attacking or blocking creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_state = TARGET_STATE_IN_COMBAT;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XW(1,1), 0, &td, "TARGET_ATTACKING_BLOCKING_CREATURE");
}

int card_solemn_simulacrum(int player, int card, event_t event)
{
/* CARD_ID_SOLEMN_SIMULACRUM	1207
Solemn Simulacrum	|4
Artifact Creature - Golem 2/2
When ~ enters the battlefield, you may search your library for a basic land card, put that card onto the battlefield tapped, then shuffle your library.
When ~ dies, you may draw a card. */

 	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY )
		tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 1);

	if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_DUH))
		draw_cards(player, 1);

	return 0;
}

int soul_foundry_impl(int player, int card, event_t event){
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_CREATE_TOKEN = 1,
		CHOICE_SHOW_EXILED
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			int result = exiledby_choose(player, card, CARD_ID_SOUL_FOUNDRY, EXBY_TEST_HAS_GENERIC_MANA_TO_PAY_CMC, 0, "", 1);
			return result;
		}
		if( ! IS_AI(player) ){
			if( exiledby_count(player, card, ANYBODY) ){
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_CREATE_TOKEN;
		instance->info_slot = choice;
		instance->targets[1].card = -1;
		if( ! IS_AI(player) ){
			int abils[3] = {0, 0, 1};
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
				abils[1] = exiledby_choose(player, card, CARD_ID_SOUL_FOUNDRY, EXBY_TEST_HAS_GENERIC_MANA_TO_PAY_CMC, event, "", 1);
			}
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Create a token", abils[1], 1,
							"Show exiled card", abils[2], 1);
		}
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_SHOW_EXILED ){
			exiledby_choose(player, card, CARD_ID_SOUL_FOUNDRY, EXBY_CHOOSE, 0, "Creature", 0);
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_CREATE_TOKEN ){
			int rval = exiledby_choose(player, card, CARD_ID_SOUL_FOUNDRY, EXBY_CHOOSE | EXBY_TEST_HAS_GENERIC_MANA_TO_PAY_CMC, event,
										"", 1);
			if( rval ){
				int* loc = (int*)rval;
				if( loc ){
					int *loc2 = exiledby_find(player, card, *loc, NULL, NULL);
					int iid = *loc2 & ~0x80000000;
					instance->targets[1].card = iid;
					return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(get_cmc_by_internal_id(iid)), 0,
													NULL, NULL);
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

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(instance->parent_controller, instance->parent_card, cards_data[instance->targets[1].card].id);
	}

	return 0;
}

int card_soul_foundry(int player, int card, event_t event){
/* Soul Foundry	|4
 * Artifact
 * Imprint - When ~ enters the battlefield, you may exile a creature card from your hand.
 * |X, |T: Put a token that's a copy of the exiled card onto the battlefield. X is the converted mana cost of that card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card to imprint.");
		this_test.zone = TARGET_ZONE_HAND;

		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			int result = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_VALUE, -1, &this_test);
			if( result > -1 ){
				exile_card_and_remember_it_on_exiledby(player, card, player, result);
			}
		}
	}

	return soul_foundry_impl(player, card, event);
}

int card_spellweaver_helix(int player, int card, event_t event){
/*
Spellweaver Helix |3
Artifact
Imprint � When Spellweaver Helix enters the battlefield, you may exile two target sorcery cards from a single graveyard.
Whenever a player casts a card, if it has the same name as one of the cards exiled with Spellweaver Helix,
you may copy the other. If you do, you may cast the copy without paying its mana cost.
*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int my_g = count_graveyard_by_type(player, TYPE_SORCERY);
		int his_g = count_graveyard_by_type(1-player, TYPE_SORCERY);
		if( my_g >= 2 || his_g >= 2 ){
			ai_modifier+=150;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.illegal_abilities = 0;

		card_instance_t *instance = get_card_instance( player, card );

		int my_g = count_graveyard_by_type(player, TYPE_SORCERY);
		int his_g = count_graveyard_by_type(1-player, TYPE_SORCERY);
		if( my_g > 1 || his_g > 1 ){
			int trg = 1-player;
			if( his_g > 1 ){
				if( my_g > 1 ){
					if( new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
						instance->number_of_targets = 0;
						trg = instance->targets[0].player;
					}
					else{
						trg = -1;
					}
				}
			}
			else{
				trg = player;
			}

			if( trg > -1 ){
				const int *grave = get_grave(trg);
				test_definition_t test;
				new_default_test_definition(&test, TYPE_SORCERY, "Select a sorcery card to imprint.");
				int count = 0, leg = 0, idx = 0;
				while( count < 2 ){
						int must_select = count < 1 ? 0 : 1;
						int selected = new_select_a_card(player, player, TUTOR_FROM_GRAVE, must_select, AI_MAX_CMC, -1, &test);
						if( selected == -1 ){
							break;
						}
						else{
							exiledby_remember(player, card, trg, grave[selected], &leg, &idx);
							rfg_card_from_grave(trg, selected);
							count++;
						}
				}
			}
		}
	}

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) &&
		player == reason_for_trigger_controller && ! is_humiliated(player, card) &&
		! check_special_flags(trigger_cause_controller, trigger_cause, SF_NOT_CAST) )
	{
		int id = get_id(trigger_cause_controller, trigger_cause);
		if( exiledby_count(player, card, player) >=2 || exiledby_count(player, card, 1-player) >= 2 ){
			int rval = exiledby_choose(player, card, CARD_ID_SPELLWEAVER_HELIX, EXBY_FIRST_FOUND | EXBY_TEST_IID, id, "", 1);
			if( rval ){
				int* loc = (int*)rval;
				if( loc ){
					int *loc2 = exiledby_find(player, card, *loc, NULL, NULL);
					int t_player = *loc2 & 0x80000000 ? 1 : 0;
					if( check_rfg(t_player, id) ){
						if(event == EVENT_TRIGGER){
							event_result |= RESOLVE_TRIGGER_AI(player);
						}
						else if(event == EVENT_RESOLVE_TRIGGER){
								copy_spell(player, id);
						}
					}
				}
			}
		}
	}

	return 0;
}

int card_spikeshot_goblin(int player, int card, event_t event){
	/* Spikeshot Goblin	|2|R	0x2005390
	 * Creature - Goblin Shaman 1/2
	 * |R, |T: ~ deals damage equal to its power to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if (event == EVENT_ACTIVATE){
		instance->info_slot = get_power(player, card);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td)  ){
			int amount = instance->info_slot;
			if( in_play(instance->parent_controller, instance->parent_card) ){
				amount = get_power(instance->parent_controller, instance->parent_card);
			}
			damage_target0(player, card, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_ANY");
}

int card_spoils_of_the_vault(int player, int card, event_t event){
/*
Spoils of the Vault |B
Instant
Name a card. Reveal cards from the top of your library until you reveal the named card, then put that card into your hand.
Exile all other cards revealed this way, and you lose 1 life for each of the exiled cards.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		int csvid = -1;
		if( IS_AI(player) ){
			csvid = ai_name_a_random_csvid(player, card, player, TYPE_ANY, NULL);
		}
		else{
			csvid = card_from_list(player, 3, TYPE_ANY, 0, 0, 0, 0, 0, 0, 0, -1, 0);
		}
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.id = csvid;
		reveal_card_until_you_reveal_test(player, player, &test, RCUYRT_ADD_TEST_TO_HAND | RCUYRT_EXILE_REST |
											RCUYRT_RETURN_REVEALED_CARDS);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_stalking_stones(int player, int card, event_t event){
/* CARD_ID_STALKING_STONES	3299
Stalking Stones	""
Land
|T: Add |C to your mana pool.
|6: ~ becomes a 3/3 Elemental artifact creature that's still a land. (This effect lasts indefinitely.) */

	return manland_normal(player, card, event, 6, 0, 0, 0, 0, 0);
}

int card_stalking_stones_animated(int player, int card, event_t event){
	return manland_animated(player, card, event, 6, 0, 0, 0, 0, 0);
}

int card_sun_droplet(int player, int card, event_t event)
{
  /* Sun Droplet	|2
   * Artifact
   * Whenever you're dealt damage, put that many charge counters on ~.
   * At the beginning of each upkeep, you may remove a charge counter from ~. If you do, you gain 1 life. */

	check_damage_test(ANYBODY, -1, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_DAMAGE_TRIGGERING_PLAYER | DDBM_REPORT_DAMAGE_DEALT,
						player, card, 0, NULL);

	int result = resolve_damage_trigger(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_DAMAGE_TRIGGERING_PLAYER |
										DDBM_REPORT_DAMAGE_DEALT, player, card);
	if( result ){
		int amount = get_card_instance(player, card)->targets[16].player;
		get_card_instance(player, card)->targets[16].player = 0;
		add_counters(player, card, COUNTER_CHARGE, amount);
	}

	if( count_counters(player, card, COUNTER_CHARGE) ){
		upkeep_trigger_ability_mode(player, card, event, ANYBODY, RESOLVE_TRIGGER_AI(player));

		if (event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			remove_counter(player, card, COUNTER_CHARGE);
			gain_life(player, 1);
		}
	}

	return 0;
}

int card_sunbeam_spellbomb(int player, int card, event_t event){
	/* Sunbeam Spellbomb	|1
	 * Artifact
	 * |W, Sacrifice ~: You gain 5 life.
	 * |1, Sacrifice ~: Draw a card. */


	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_DRAW_CARD = 1,
		CHOICE_GAIN_LIFE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return spellbomb(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0,
						1,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME, MANACOST_W(1), 0, NULL, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Sac & draw", 	abils[1], 14 - (hand_count[player]*2),
							"Sac & gain life", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_DRAW_CARD ){
			return spellbomb(player, card, event);
		}
		if( choice == CHOICE_GAIN_LIFE ){
			return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_W(1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DRAW_CARD ){
			draw_cards(player, 1);
		}
		if( instance->info_slot == CHOICE_GAIN_LIFE ){
			gain_life(player, 5);
		}
	}

	return 0;
}

int card_sword_of_kaldra(int player, int card, event_t event){
/* CARD_ID_SWORD_OF_KALDRA	6796
Sword of Kaldra	|4
Legendary Artifact - Equipment
Equipped creature gets +5/+5.
Whenever equipped creature deals damage to a creature, exile that creature. (Exile it only if it's still on the battlefield.)
Equip |4 (|4: Attach to target creature you control. Equip only as a sorcery.
This card enters the battlefield unattached and stays on the battlefield if the creature leaves.) */

	check_legend_rule(player, card, event);

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		rfg_when_damage(p, c, event);

		if( event == EVENT_CHANGE_TYPE && affect_me(p, c) && ! is_humiliated(player, card) ){
			get_card_instance(p, c)->destroys_if_blocked |= DIFB_DESTROYS_ALL;
		}
	}

	return vanilla_equipment(player, card, event, 4, 5, 5, 0, 0);
}

int card_sylvan_scrying(int player, int card, event_t event){
	/*
	  Sylvan Scrying |1|G
	  Sorcery
	  Search your library for a land card, reveal it, and put it into your hand. Then shuffle your library.
	*/
	if( event == EVENT_RESOLVE_SPELL ){
		tutor_lands(player, TUTOR_HAND, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}


int card_taj_nar_swordsmith(int player, int card, event_t event){
/* CARD_ID_TAJ_NAR_SWORDSMITH	6798
Taj-Nar Swordsmith	|3|W
Creature - Cat Soldier 2/3
When ~ enters the battlefield, you may pay |X.
If you do, search your library for an Equipment card with converted mana cost X or less and put that card onto the battlefield. Then shuffle your library.*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		charge_mana(player, COLOR_COLORLESS, -1);
		if( spell_fizzled != 1 ){
			int x = x_value;
			x_value = 0;
			test_definition_t test;
			default_test_definition(&test, TYPE_ARTIFACT);
			scnprintf(test.message, 100, "Select an equipment card with CMC %d or less.", x);
			test.subtype = SUBTYPE_EQUIPMENT;
			test.cmc = x + 1;
			test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
		}
	}

	return 0;
}

int card_tangleroot(int player, int card, event_t event){
/* CARD_ID_TANGLEROOT	6805
Tangleroot	|3
Artifact
Whenever a player casts a creature spell, that player adds |G to his or her mana pool. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &test) ){
			produce_mana(trigger_cause_controller, COLOR_GREEN, 1);
		}
	}

	return 0;
}

// Tel-Jilad Chosen -> vanilla

// Tempest of Ligh -> Tranquillity

int card_temporal_cascade(int player, int card, event_t event){
/*
Temporal Cascade |5|U|U
Sorcery
Choose one �
� Each player shuffles his or her hand and graveyard into his or her library.
� Each player draws seven cards.
Entwine {2} (Choose both if you pay the entwine cost.)
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum{
		CHOICE_RESHUFFLE_HAND_GRAVE = 1,
		CHOICE_DRAW7,
		CHOICE_ENTWINE
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST  ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Timetwister without drawing", 1, 5,
								"Both draws 7 cards", 1, hand_count[1-player]-hand_count[player] >= 5 ? 10 : 1,
								"Entwine", has_mana(player, COLOR_ANY, 2), 20);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( choice == CHOICE_ENTWINE ){
				charge_mana(player, COLOR_COLORLESS, 2);
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot & CHOICE_RESHUFFLE_HAND_GRAVE ){
			APNAP(p,{
						reshuffle_hand_into_deck(p, 1);
						reshuffle_grave_into_deck(p, 0);
					};
			);
		}
		if( instance->info_slot & CHOICE_DRAW7 ){
			APNAP(p, { draw_cards(p, 7); };);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_thirst_for_knowledge(int player, int card, event_t event){
/* CARD_ID_THIRST_FOR_KNOWLEDGE	1545
Thirst for Knowledge	|2|U
Instant
Draw three cards. Then discard two cards unless you discard an artifact card. */

	/* Original Code: 004E76FB */

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 3);

		test_definition_t test;
		new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact card to discard");
		test.zone = TARGET_ZONE_HAND;
		int selected = -1;
		if( check_battlefield_for_special_card(player, card, player, 0, &test) ){
			selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &test);
		}
		if( selected != -1 ){
			discard_card(player, selected);
		}
		else{
			multidiscard(player, 2, 0);
		}

		kill_card(player, card, KILL_SACRIFICE);
	}

	return basic_spell(player, card, event);
}

int card_thought_prison(int player, int card, event_t event){
/*
Thought Prison |5
Artifact
Imprint � When Thought Prison enters the battlefield, you may have target player reveal his or her hand.
		If you do, choose a nonland card from it and exile that card.
Whenever a player casts a spell that shares a color or converted mana cost with the exiled card, Thought Prison deals 2 damage
	to that player.
*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player == AI  ){
			if( hand_count[1-player] < 1 ){
				ai_modifier-=1000;
			}
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			this_test.type_flag = DOESNT_MATCH;

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			this_definition.effect = EC_RFG;
			int id = new_effect_coercion(&this_definition, &this_test);
			if( id > -1 ){
				int pos = MAX(1, instance->info_slot);
				if( pos < 10 ){
					instance->targets[pos].card = id;
					instance->info_slot = pos+1;
					create_card_name_legacy(player, card, id);
				}
			}
		}
	}

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller ){
		card_instance_t *instance = get_card_instance(player, card);
		int dmg = 0;
		int i;
		for(i=1; i<instance->info_slot && ! dmg; i++){
			int iid = instance->targets[i].card;
			if( has_my_colors(trigger_cause_controller, trigger_cause, -1, iid) ){
				dmg = 1;
			}
			if( get_cmc(trigger_cause_controller, trigger_cause) == get_cmc_by_id(iid) ){
				dmg = 1;
			}
		}
		if( dmg && new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, NULL) ){
			damage_player(trigger_cause_controller, 2, player, card);
		}
	}

	return 0;
}

int card_thoughtcast(int player, int card, event_t event)
{
  // 0x12050f2

  /* Thoughtcast	|4|U
   * Sorcery
   * Affinity for artifacts
   * Draw two cards. */

	affinity(player, card, event, TYPE_ARTIFACT, -1);

	if (event == EVENT_CAN_CAST)
		return basic_spell(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) )
		ai_modifier += (count_subtype(player, TYPE_ARTIFACT, -1)*8);

	if (event == EVENT_RESOLVE_SPELL)
	{
		draw_cards(player, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_tooth_and_nail(int player, int card, event_t event){
/* CARD_ID_TOOTH_AND_NAIL	1548
Tooth and Nail	|5|G|G
Sorcery
Choose one -
* Search your library for up to two creature cards, reveal them, put them into your hand, then shuffle your library.
* Put up to two creature cards from your hand onto the battlefield.
Entwine |2 (Choose both if you pay the entwine cost.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_TUTOR_CREATURE = 1,
		CHOICE_PUT_CREATURES_INTO_PLAY,
		CHOICE_ENTWINE
	};

	if(event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.cmc = 5;
			this_test.cmc_flag = F5_CMC_GREATER_THAN_VALUE;
			this_test.zone = TARGET_ZONE_HAND;
			int priority_put_creature_into_play = 5*check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test);
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Search for 2 creatures", 1, 5,
								"Put 2 Creatures into play", 1, priority_put_creature_into_play,
								"Entwine", has_mana(player, COLOR_ANY, 2), 100);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( choice == CHOICE_ENTWINE ){
				ai_modifier += 96;
				charge_mana(player, COLOR_COLORLESS, 2);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card");
		this_test.qty = 2;
		if( instance->info_slot & CHOICE_TUTOR_CREATURE ){
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_CMC, &this_test);
		}
		if( instance->info_slot & CHOICE_PUT_CREATURES_INTO_PLAY ){
			new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return 0;
}

int card_tower_of_fortunes(int player, int card, event_t event){

	/* Tower of Fortunes	|4
	 * Artifact
	 * |8, |T: Draw four cards. */

	if(event == EVENT_RESOLVE_ACTIVATION){
		draw_cards(player, 4);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(8), 0, NULL, NULL);
}

int card_troll_ascetic(int player, int card, event_t event){
/* CARD_ID_TROLL_ASCETIC	6820
Troll Ascetic	|1|G|G
Creature - Troll Shaman 3/2
Hexproof (This creature can't be the target of spells or abilities your opponents control.)
|1|G: Regenerate ~. */

	hexproof(player, card, event);

	return regeneration(player, card, event, MANACOST_XG(1, 1));
}

int card_scale_of_chiss_goria(int player, int card, event_t event){
/* CARD_ID_SCALE_OF_CHISS_GORIA	6775
Scale of Chiss-Goria	|3
Artifact
Flash
Affinity for artifacts (This spell costs |1 less to cast for each artifact you control.)
|T: Target creature gets +0/+1 until end of turn. */

	// tooth of chiss-goria

	affinity(player, card, event, TYPE_ARTIFACT, -1);

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				pump_until_eot(instance->parent_controller, instance->parent_card,
								instance->targets[0].player, instance->targets[0].card, 1, 0);
			}
		}

		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
	}

	return flash(player, card, event);
}

int card_tower_of_champions(int player, int card, event_t event){
/* CARD_ID_TOWER_OF_CHAMPIONS	6815
Tower of Champions	|4
Artifact
|8, |T: Target creature gets +6/+6 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	return vanilla_creature_pumper(player, card, event, MANACOST_X(8), GAA_UNTAPPED, 6, 6, 0, 0, &td);
}

int card_tower_of_eons(int player, int card, event_t event){
/* CARD_ID_TOWER_OF_EONS	6816
Tower of Eons	|4
Artifact
|8, |T: You gain 10 life. */

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (life[player] < 6 ? 48 : -48);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 10);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(8), 0, NULL, NULL);
}

int card_tower_of_murmurs(int player, int card, event_t event){
/* CARD_ID_TOWER_OF_MURMURS	6818
Tower of Murmurs	|4
Artifact
|8, |T: Target player puts the top eight cards of his or her library into his or her graveyard. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		ai_modifier += 24;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			mill(instance->targets[0].player, 8);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(8), 0, &td, "TARGET_CREATURE");
}

int card_trash_for_treasures(int player, int card, event_t event){
/* CARD_ID_TRASH_FOR_TREASURE	6819
Trash for Treasure	|2|R
Sorcery
As an additional cost to cast ~, sacrifice an artifact.
Return target artifact card from your graveyard to the battlefield. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) && count_graveyard_by_type(player, TYPE_ARTIFACT) && ! graveyard_has_shroud(player) ){
			return new_can_sacrifice_as_cost(player, card, &test);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int min_cmc = 100;
		int max_cmc = -1;
		CYCLE_ALL_CARDS(p, c, 	{
									if( in_play(p, c) && is_what(p, c, TYPE_ARTIFACT) ){
										int this_cmc = get_cmc(p, c);
										if( this_cmc < min_cmc ){
											min_cmc = this_cmc;
										}
									}
								};
		);

		int i;
		for(i=0; i<count_graveyard(player); i++){
			int iid = get_grave(player)[i];
			if( is_what(-1, iid, TYPE_ARTIFACT) ){
				int this_cmc = get_cmc_by_internal_id(iid);
				if( this_cmc > max_cmc ){
					max_cmc = this_cmc;
				}
			}
		}

		ai_modifier += ((max_cmc - min_cmc)*8);

		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_AS_COST | SAC_RETURN_CHOICE, &test);
		if( ! sac ){
			spell_fizzled = 1;
			return 0;
		}
		if( select_target_from_grave_source(player, card, player, 0, AI_MAX_CMC, &test, 0) > -1 ){
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
		else{
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
			spell_fizzled = 1;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected > -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_turn_to_dust(int player, int card, event_t event){
/* CARD_ID_TURN_TO_DUST	6822
Turn to Dust	|G
Instant
Destroy target Equipment. Add |G to your mana pool. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.required_subtype = SUBTYPE_EQUIPMENT;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			produce_mana(player, COLOR_GREEN, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_EQUIPMENT", 1, NULL);
}

int card_vedalken_archmage(int player, int card, event_t event){
/* CARD_ID_VEDALKEN_ARCHMAGE	6823
Vedalken Archmage	|2|U|U
Creature - Vedalken Wizard 0/2
Whenever you cast an artifact spell, draw a card. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_vermiculos(int player, int card, event_t event){
/* CARD_ID_VERMICULOS	6824
Vermiculos	|4|B
Creature - Horror 1/1
Whenever an artifact enters the battlefield, ~ gets +4/+4 until end of turn. */

	etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ARTIFACT, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		pump_until_eot_merge_previous(player, card, player, card, 4, 4);
	}

	return 0;
}

int card_viridian_joiner(int player, int card, event_t event){
/* CARD_ID_VIRIDIAN_JOINER	6825
Viridian Joiner	|2|G
Creature - Elf Druid 1/2
|T: Add an amount of |G to your mana pool equal to ~'s power. */

	return mana_producing_creature(player, card, event, 12, COLOR_GREEN, get_power(player, card));
}

int card_vorrac_battlehorns(int player, int card, event_t event){
/* CARD_ID_VORRAC_BATTLEHORNS	6827
Vorrac Battlehorns	|2
Artifact - Equipment
Equipped creature has trample and can't be blocked by more than one creature.
Equip |1 (|1: Attach to target creature you control. Equip only as a sorcery.
This card enters the battlefield unattached and stays on the battlefield if the creature leaves.) */

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		if( event == EVENT_DECLARE_BLOCKERS && current_turn == player){
			int block_count = 0;
			int count = 0;
			while(count < active_cards_count[1-player]){
					if(in_play(1-player, count) ){
						if( get_card_instance(1-player, count)->blocking == instance->targets[8].card ){
							block_count++;
							if( block_count > 1 ){
								get_card_instance(1-player, count)->blocking = 255;
								get_card_instance(1-player, count)->state &= ~(STATE_BLOCKED|STATE_BLOCKING);
							}
						}
					}
					count++;
			}
		}
	}

	return vanilla_equipment(player, card, event, 1, 0, 0, KEYWORD_TRAMPLE, 0);
}

int card_vulshok_battlegear(int player, int card, event_t event){
/* CARD_ID_VULSHOK_BATTLEGEAR	6828
Vulshok Battlegear	|3
Artifact - Equipment
Equipped creature gets +3/+3.
Equip |3 (|3: Attach to target creature you control. Equip only as a sorcery.
This card enters the battlefield unattached and stays on the battlefield if the creature leaves.) */

	return vanilla_equipment(player, card, event, 3, 3, 3, 0, 0);
}

int card_vulshok_battlemaster(int player, int card, event_t event){
/* CARD_ID_VULSHOK_BATTLEMASTER	6829
Vulshok Battlemaster	|4|R
Creature - Human Warrior 2/2
Haste
When ~ enters the battlefield, attach all Equipment on the battlefield to it. (Control of the Equipment doesn't change.) */

	haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int i;
		for(i=0; i<2; i++){
			int count = active_cards_count[i]-1;
			while( count > -1 ){
					if( in_play(i, count) && is_what(i, count, TYPE_ARTIFACT) && has_subtype(i, count, SUBTYPE_EQUIPMENT) ){
						equip_target_creature(i, count, player, card);
					}
					count--;
			}
		}
	}

	return 0;
}

int card_vulshock_gauntlets(int player, int card, event_t event){
/* CARD_ID_VULSHOK_GAUNTLETS	6831
Vulshok Gauntlets	|2
Artifact - Equipment
Equipped creature gets +4/+2 and doesn't untap during its controller's untap step.
Equip |3 (|3: Attach to target creature you control. Equip only as a sorcery.
This card enters the battlefield unattached and stays on the battlefield if the creature leaves.) */

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		does_not_untap(p, c, event);
	}

	return vanilla_equipment(player, card, event, 3, 4, 2, 0, 0);
}

int card_wail_of_the_nim(int player, int card, event_t event){
/*
Wail of the Nim English |2|B
Instant
Choose one �
� Regenerate each creature you control.
� Wail of the Nim deals 1 damage to each creature and each player.
Entwine {B} (Choose both if you pay the entwine cost.)
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_REGENERATE_ALL = 1,
		CHOICE_DAMAGE_ALL,
		CHOICE_ENTWINE
	};

	if( event == EVENT_CAN_CAST){
		target_definition_t td;
	    default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_state = TARGET_STATE_DESTROYED;
		if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	    }
		if( generic_spell(player, card, event, GS_REGENERATION|GAA_CAN_TARGET, &td, NULL, 1, NULL) ){
			return 99;
		}
		else{
			return basic_spell(player, card, event);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Regenerate your guys", 1, 10,
								"1 damage to all creatures and players", 1, 5,
								"Entwine", has_mana(player, COLOR_BLACK, 1), 15);

			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( choice == CHOICE_ENTWINE ){
				charge_mana(player, COLOR_BLACK, 1);
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot & CHOICE_REGENERATE_ALL ){
			int count = active_cards_count[player]-1;
			while( count > -1 ){
					if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) ){
							regenerate_or_shield(player, card, player, count);
					}
					count--;
			}
		}
		if( instance->info_slot & CHOICE_DAMAGE_ALL ){
			APNAP(p, { new_damage_all(player, card, p, 1, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL); };);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_war_elemental(int player, int card, event_t event){
/*
War Elemental |R|R|R
Creature � Elemental 1/1
When War Elemental enters the battlefield, sacrifice it unless an opponent was dealt damage this turn.
Whenever an opponent is dealt damage, put that many +1/+1 counters on War Elemental.
*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( get_trap_condition(1-player, TRAP_DAMAGE_TAKEN) < 1 ){
			ai_modifier-=1000;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && in_play(player, card) ){
		if( get_trap_condition(1-player, TRAP_DAMAGE_TAKEN) < 1 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	check_damage_test(ANYBODY, -1, event, DDBM_MUST_DAMAGE_OPPONENT | DDBM_REPORT_DAMAGE_DEALT, player, card, 0, NULL);

	if( resolve_damage_trigger(player, card, event, DDBM_MUST_DAMAGE_OPPONENT | DDBM_REPORT_DAMAGE_DEALT, player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int amount = instance->targets[16].player;
		instance->targets[16].player = 0;
		add_1_1_counters(player, card, amount);
	}

	return 0;
}

int card_welding_jar(int player, int card, event_t event){
/* CARD_ID_WELDING_JAR	6836
Welding Jar	|0
Artifact
Sacrifice ~: Regenerate target artifact. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_type = TYPE_ARTIFACT;
	td.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target( instance->targets[0].player, instance->targets[0].card );
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET | GAA_REGENERATION | GAA_LITERAL_PROMPT,
										MANACOST0, 0, &td, "Select target artifact to regenerate.");
}

int card_wizard_replica(int player, int card, event_t event){
	/* Wizard Replica	|3
	 * Artifact Creature - Wizard 1/3
	 * Flying
	 * |U, Sacrifice ~: Counter target spell unless its controller pays |2. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		counterspell_resolve_unless_pay_x(player, card, &td, 0, 2);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_SPELL_ON_STACK, MANACOST_U(1), 0, &td, NULL);
}

int card_wrench_mind(int player, int card, event_t event){
/* CARD_ID_WRENCH_MIND	6840
Wrench Mind	|B|B
Sorcery
Target player discards two cards unless he or she discards an artifact card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			char msg[100] = "Select a card to discard.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, msg);

			test_definition_t this_test2;
			default_test_definition(&this_test2, TYPE_ARTIFACT);

			int count = 0;
			while( count < 2 && hand_count[instance->targets[0].player] > 0 ){
					int selected = -1;
					if( instance->targets[0].player == AI ){
						selected = new_select_a_card(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_HAND, 1,
													AI_MIN_VALUE, -1, &this_test2);
					}
					if( instance->targets[0].player == HUMAN || selected == -1 ){
						selected = new_select_a_card(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_HAND, 1,
													AI_MIN_VALUE, -1, &this_test);
					}
					if( is_what(instance->targets[0].player, selected, TYPE_ARTIFACT) ){
						new_discard_card(instance->targets[0].player, selected, player, 0);
						break;
					}
					else{
						new_discard_card(instance->targets[0].player, selected, player, 0);
						count++;
					}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}
