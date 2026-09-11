#include "manalink.h"

// Functions
static int subtype_searcher(int player, int card, event_t event, int subtype){
	return generic_subtype_searcher(player, card, event, subtype, 3, 2);
}

static int mercenary_searcher(int player, int card, event_t event, int cless){
	return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, cless, 1);
}

int seal(int player, int card, event_t event, target_definition_t *td, const char *prompt ){
	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, td, prompt);
}

static int parallax(int player, int card, event_t event){

	fading(player, card, event, 5);

	return global_enchantment(player, card, event);
}

int nemesis_freespell(int player, int card, int event){
	if( ! played_for_free(player, card) && ! is_token(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->info_slot == 1 && player != AI && has_mana_to_cast_id(player, event, get_id(player, card)) ){
			int choice = do_dialog(player, player, card, -1, -1, " Play for free\n Play normally\n Cancel", 0);
			if( choice == 1 ){
				if( ! charge_mana_from_id(player, card, event, get_id(player, card)) ){
					return 0;
				}
			}
			else if( choice == 2 ){
					return 0;
			}
		}
	}
	return 1;
}


// Cards

int card_accumulated_knowledge(int player, int card, event_t event){
/* CARD_ID_ACCUMULATED_KNOWLEDGE	4660
Accumulated Knowledge	|1|U
Instant
Draw a card, then draw cards equal to the number of cards named ~ in all graveyards. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 24;
		ai_modifier += ((count_graveyard_by_id(player, get_id(player, card)) + count_graveyard_by_id(1-player, get_id(player, card))) * 8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = 1 + count_graveyard_by_id(player, get_id(player, card)) + count_graveyard_by_id(1-player, get_id(player, card));
		draw_cards(player, amount);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_aether_barrier(int player, int card, event_t event){
/* CARD_ID_AETHER_BARRIER	4667
Aether Barrier	|2|U
Enchantment
Whenever a player casts a creature spell, that player sacrifices a permanent unless he or she pays |1. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &test) ){
			int sac = 1;
			if( has_mana(trigger_cause_controller, COLOR_COLORLESS, 1) &&
				charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, trigger_cause_controller, COLOR_COLORLESS, 1) )
			{
				sac = 0;
			}
			if( sac == 1 ){
				new_sacrifice(player, card, trigger_cause_controller, SAC_CAUSED | SAC_NO_CANCEL, &test);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_ancient_hydra(int player, int card, event_t event){
	/* Ancient Hydra	|4|R	0x2008694
	 * Creature - Hydra 5/1
	 * Fading 5
	 * |1, Remove a fade counter from ~: ~ deals 1 damage to any target. */
	fading(player, card, event, 5);

	if (!IS_GAA_EVENT(event)){
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

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), GVC_COUNTER(COUNTER_FADE),
									&td, "TARGET_ANY");
}

int card_arc_mage(int player, int card, event_t event){
	/* Arc Mage	|2|R	0x2008699
	 * Creature - Human Spellshaper 2/2
	 * |2|R, |T, Discard a card: ~ deals 2 damage divided as you choose among one or two targets. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DISCARD, MANACOST_XR(2, 1), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_XR(2, 1)) ){
			if( pick_target(&td, "TARGET_ANY") ){
				if( new_pick_target(&td, "TARGET_ANY", 1, 1) ){
					add_state(player, card, STATE_TAPPED);
					discard(player, 0, player);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		divide_damage(player, card, &td);
	}

	return 0;
}

int card_ascendant_evincar(int player, int card, event_t event){
/* CARD_ID_ASCENDANT_EVINCAR	4666
Ascendant Evincar	|4|B|B
Legendary Creature - Vampire 3/3
Flying (This creature can't be blocked except by creatures with flying or reach.)
Other |Sblack creatures get +1/+1.
Non|Sblack creatures get -1/-1. */

	check_legend_rule(player, card, event);

	evincars(player, card, event, COLOR_TEST_BLACK);

	return 0;
}

int card_avenger_en_dal(int player, int card, event_t event){
/* CARD_ID_AVENGER_EN_DAL	4668
Avenger en-Dal	|1|W
Creature - Human Spellshaper 1/1
|2|W, |T, Discard a card: Exile target attacking creature. Its controller gains life equal to its toughness. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = get_toughness(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			gain_life(instance->targets[0].player, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD | GAA_CAN_TARGET | GAA_LITERAL_PROMPT,
									MANACOST_XW(2, 1), 0, &td, "Select target attacking creature.");
}

int card_belbes_armor(int player, int card, event_t event){
/* CARD_ID_BELBES_ARMOR	4670
Belbe's Armor	|3
Artifact
|X, |T: Target creature gets -X/+X until end of turn. */
	
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
    td.preferred_controller = ANYBODY;

	 if (event == EVENT_CHECK_PUMP)
    {
      int avail = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, 
	                                                                MANACOST_X(-1), 0, &td, "TARGET_CREATURE");
      pumpable_power[1-player] -= avail;
      pumpable_toughness[player] += avail;
    }
	
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = instance->info_slot;
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							-amount, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(-1), 0, &td, "TARGET_CREATURE");
}

int card_belbes_portal(int player, int card, event_t event){

	/* Belbe's Portal	|5
	 * Artifact
	 * As ~ enters the battlefield, choose a creature type.
	 * |3, |T: You may put a creature card of the chosen type from your hand onto the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = select_a_subtype(player, card);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int subt = get_card_instance(player, card)->info_slot;
		if( subt <= 0 ){	// no creature type was chosen
			return 0;
		}

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, get_subtype_text("Select %a creature card.", subt));
		this_test.subtype = subt;
		new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
}

int card_blastoderm(int player, int card, event_t event){
	/* Blastoderm	|2|G|G
	 * Creature - Beast 5/5
	 * Shroud
	 * Fading 3 */
	fading(player, card, event, 3);
	return 0;
}

int card_blinding_angel(int player, int card, event_t event)
{
/*
Blinding Angel |3|W|W
Creature — Angel 2/4
Flying (This creature can't be blocked except by creatures with flying or reach.)
Whenever Blinding Angel deals combat damage to a player, that player skips his or her next combat phase.
*/
	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					if( times_damaged[p] ){
						int legacy = target_player_skips_his_next_attack_step(player, card, p, 0);
						get_card_instance(player, legacy)->targets[2].player = times_damaged[p];
					}
				}
		);

	}

	return 0;
}

int card_bola_warrior(int player, int card, event_t event){
/* CARD_ID_BOLA_WARRIOR	4674
Bola Warrior	|1|R
Creature - Human Spellshaper Warrior 1/1
|R, |T, Discard a card: Target creature can't block this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
									0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD | GAA_CAN_TARGET, MANACOST_W(1), 0,
									&td, "TARGET_CREATURE");
}

int card_chieftain_en_dal(int player, int card, event_t event)
{
/* CARD_ID_CHIEFTAIN_EN_DAL	4675
Chieftain en-Dal	|1|W|W
Creature - Human Knight 2/2
Whenever ~ attacks, attacking creatures gain first strike until end of turn. */

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.state = STATE_ATTACKING;
	  pump_creatures_until_eot(player, card, current_turn, 0, 0,0, KEYWORD_FIRST_STRIKE,0, &test);
	}

	return 0;
}

// cloudskate --> blastoderm

int card_complex_automaton(int player, int card, event_t event){
/* CARD_ID_COMPLEX_AUTOMATON	4678
Complex Automaton	|4
Artifact Creature - Golem 4/4
At the beginning of your upkeep, if you control seven or more permanents, return ~ to its owner's hand. */

	if( current_turn == player && upkeep_trigger(player, card, event) ){
		if( count_subtype(player, TYPE_PERMANENT, -1) > 6 ){
			bounce_permanent(player, card);
		}
	}
	return 0;
}

int card_dark_triumph(int player, int card, event_t event){
	/*
	  Dark Triumph |4|B
	  Instant
	  If you control a Swamp, you may sacrifice a creature rather than pay Dark Triumph's mana cost.
	  Creatures you control get +2/+0 until end of turn.
	*/
	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_MODIFY_COST ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_SWAMP) && new_can_sacrifice_as_cost(player, card, &test) ){
			instance->info_slot = 1;
			null_casting_cost(player, card);
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_id(player, event, get_id(player, card)) ){
				choice = do_dialog(player, player, card, -1, -1, " Sac a creature\n Play normally\n Cancel", 0);
			}

			if( choice == 0 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
				if( ! new_sacrifice(player, card, player, 0, &test) ){
					spell_fizzled = 1;
				}
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 2, 0, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_daze(int player, int card, event_t event){
	/*
	  Daze |1|U
	  Instant
	  You may return an Island you control to its owner's hand rather than pay Daze's mana cost.
	  Counter target spell unless its controller pays {1}.
	*/
	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.required_subtype = SUBTYPE_ISLAND;
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		if( target_available(player, card, &td) > 0 ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_id(player, event, get_id(player, card)) ){
				choice = do_dialog(player, player, card, -1, -1, " Bounce an Island\n Play normally\n Cancel", 0);
			}
			if( choice == 0 ){
				target_definition_t td;
				default_target_definition(player, card, &td, TYPE_LAND);
				td.allowed_controller = player;
				td.preferred_controller = player;
				td.required_subtype = SUBTYPE_ISLAND;
				td.illegal_abilities = 0;

				if( new_pick_target(&td, "Select an Island to return to your hand.", 0, 1 | GS_LITERAL_PROMPT) ){
					bounce_permanent(instance->targets[0].player, instance->targets[0].card);;
				}
				instance->number_of_targets = 0;
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
			}
		}
		if( spell_fizzled != 1 ){
			return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		return card_force_spike(player, card, event);
	}

	return 0;
}

int card_death_pit_offering(int player, int card, event_t event){
	/*
	  Death Pit Offering |2|B|B
	  Enchantment
	  When Death Pit Offering enters the battlefield, sacrifice all creatures you control.
	  Creatures you control get +2/+2.
	*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		new_manipulate_all(player, card, player, &test, KILL_SACRIFICE);
	}

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		boost_creature_type(player, card, event, -1, 2, 2, 0, BCT_CONTROLLER_ONLY);
	}

	return global_enchantment(player, card, event);
}

int card_defender_en_vec(int player, int card, event_t event){
	/* Defender en-Vec	|3|W	0x20086c1
	 * Creature - Human Cleric 2/4
	 * Fading 4
	 * Remove a fade counter from ~: Prevent the next 2 damage that would be dealt to any target this turn. */
	fading(player, card, event, 4);

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.extra = damage_card;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 2);
		}
	}
	return generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_FADE),
									&td1, "TARGET_DAMAGE");
}

int card_defiant_falcon(int player, int card, event_t event)
{
  /* Amrou Scout	|1|W
   * Creature - Kithkin Rebel Scout 2/1
   * |4, |T: Search your library for a Rebel permanent card with converted mana cost 3 or less and put it onto the battlefield. Then shuffle your library. */

  /* Defiant Falcon	|1|W
   * Creature - Rebel Bird 1/1
   * Flying
   * |4, |T: Search your library for a Rebel permanent card with converted mana cost 3 or less and put it onto the battlefield. Then shuffle your library. */

  return generic_subtype_searcher(player, card, event, SUBTYPE_REBEL, 4, 0);
}

int card_defiant_vanguard(int player, int card, event_t event){

	/* Defiant Vanguard	|2|W
	 * Creature - Human Rebel 2/2
	 * When ~ blocks, at end of combat, destroy it and all creatures it blocked this turn.
	 * |5, |T: Search your library for a Rebel permanent card with converted mana cost 4 or less and put it onto the battlefield. Then shuffle your library. */

	if( current_turn != player && event == EVENT_DECLARE_BLOCKERS && ! is_humiliated(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		if( instance->blocking < 255 ){
			create_targetted_legacy_effect(player, card, &die_at_end_of_combat, 1-player, instance->blocking);
			create_targetted_legacy_effect(player, card, &die_at_end_of_combat, player, card);
		}
	}

	return generic_subtype_searcher(player, card, event, SUBTYPE_REBEL, 5, 0);
}

int card_dominate(int player, int card, event_t event){
/* CARD_ID_DOMINATE	4685
Dominate	|X|1|U|U
Instant
Gain control of target creature with converted mana cost X or less. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			int amount = get_cmc(instance->targets[0].player, instance->targets[0].card);
			charge_mana(player, COLOR_COLORLESS, amount);
			if( spell_fizzled != 1 ){
				instance->info_slot = amount;
			}
		}	
		if (!can_target(&td)){
			spell_fizzled = 1;
			ai_modifier -= 128;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_ensnare(int player, int card, event_t event){
/* CARD_ID_ENSNARE	4687
Ensnare	|3|U
Instant
You may return two |H1Islands you control to their owner's hand rather than pay ~'s mana cost.
Tap all creatures. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(player, card, 2, &this_test, ACT_TAP);
		kill_card(player, card, KILL_DESTROY);
	}
	else{
		return card_gush(player, card, event);
	}

	return 0;
}

int card_fanatical_devotion(int player, int card, event_t event){
/* CARD_ID_FANATICAL_DEVOTION	4689
Fanatical Devotion	|2|W
Enchantment
Sacrifice a creature: Regenerate target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_REGENERATION | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
		if( result && can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE) && count_subtype(player, TYPE_CREATURE, -1) > 1){
			return result;
		}
	}

	if( event == EVENT_ACTIVATE ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		if( new_pick_target(&td, "Select target creature to regenerate.", 0, 1 | GS_LITERAL_PROMPT) ){
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
		else{
			state_untargettable(BYTE2(sac), BYTE3(sac), 1);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target( instance->targets[0].player, instance->targets[0].card );
		}
	}

	return 0;
}

int card_flame_rift(int player, int card, event_t event){
	/*
	  Flame Rift |1|R
	  Sorcery
	  Flame Rift deals 4 damage to each player.
	*/
	if( event == EVENT_RESOLVE_SPELL ){
		damage_player(player, 4, player, card);
		damage_player(1-player, 4, player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_flowstone_overseer(int player, int card, event_t event){
/* CARD_ID_FLOWSTONE_OVERSEER	4693
Flowstone Overseer	|2|R|R|R
Creature - Beast 4/4
|R|R: Target creature gets +1/-1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, -1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_R(2), 0, &td, "TARGET_CREATURE");
}

int card_flowstone_slide(int player, int card, event_t event){
/* CARD_ID_FLOWSTONE_SLIDE	4694
Flowstone Slide	|X|2|R|R
Sorcery
All creatures get +X/-X until end of turn. */

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		charge_mana(player, COLOR_COLORLESS, -1);
		if( spell_fizzled != 1 ){
			set_x_for_x_spells(player, card, event, x_value);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = get_card_instance(player, card)->info_slot;
		pump_subtype_until_eot(player, card, 2, -1, amount, -amount, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_flowstone_surge(int player, int card, event_t event){
/* CARD_ID_FLOWSTONE_SURGE	4696
Flowstone Surge	|1|R
Enchantment
Creatures you control get +1/-1. */

	boost_creature_type(player, card, event, -1, 1, -1, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
	return global_enchantment(player, card, event);
}

int card_jolting_merfolk(int player, int card, event_t event){

	/* Jolting Merfolk	|2|U|U
	 * Creature - Merfolk 2/2
	 * Fading 4
	 * Remove a fade counter from ~: Tap target creature. */

	fading(player, card, event, 4);

	if (!IS_GAA_EVENT(event)){
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

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_FADE), &td, "TARGET_CREATURE");
}

int card_kill_switch(int player, int card, event_t event){

	/* Kill Switch	|3
	 * Artifact
	 * |2, |T: Tap all other artifacts. They don't untap during their controllers' untap steps for as long as ~ remains tapped. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int pp = instance->parent_controller, pc = instance->parent_card;
		APNAP(p,{
					int c;
					for(c = active_cards_count[p]-1; c > -1; c--){
						if( in_play(p, c) && is_what(p, c, TYPE_ARTIFACT) && !(p == pp && c == pc) ){
							tap_card(p, c);
							does_not_untap_until_im_tapped(pp, pc, p, c);
						}
					}
				}
		);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL);
}

int card_kor_haven(int player, int card, event_t event){
	/*
	  Kor Haven English
	  Legendary Land
	  {T}: Add {1} to your mana pool.
	  {1}{W}, {T}: Prevent all combat damage that would be dealt by target attacking creature this turn.
	*/
	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int choice = 0;
		if( !paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XW(2, 1), 0, &td, NULL) ){
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Prevent Damage\n Cancel", 1);
		}

		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		else if( choice == 1 ){
				add_state(player, card, STATE_TAPPED);
				if( charge_mana_for_activated_ability(player, card, MANACOST_XW(1, 1)) &&
					new_pick_target(&td, "DESERT", 0, 1) )
				{
					instance->info_slot = 1;
				}
				else{
					remove_state(player, card, STATE_TAPPED);
				}
		}
		else if(choice == 2){
				spell_fizzled = 1;
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			if( valid_target(&td) ){
				negate_combat_damage_this_turn(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 0);
			}
		}
	}

	return 0;
}

int card_laccolith_grunt(int player, int card, event_t event){
/* CARD_ID_LACCOLITH_GRUNT	4705
Laccolith Grunt	|2|R
Creature - Beast 2/2
Whenever ~ becomes blocked, you may have it deal damage equal to its power to target creature. If you do, ~ assigns no combat damage this turn. */

	// also code for Laccolith Titan, Laccolith Warrior and Laccolith Whelp

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( current_phase == PHASE_AFTER_BLOCKING && is_attacking(player, card) && ! is_unblocked(player, card) &&
			instance->targets[1].player != 66 )
		{
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
		}
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			instance->targets[1].player = 66;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, get_power(player, instance->parent_card),
							instance->parent_controller, instance->parent_card);
		}
		negate_combat_damage_this_turn(instance->parent_controller, instance->parent_card,
										instance->parent_controller, instance->parent_card, 0);
	}

	return 0;
}

int card_lin_sivvi_defiant_hero(int player, int card, event_t event){
/* Lin Sivvi, Defiant Hero	|1|W|W
 * Legendary Creature - Human Rebel 1/3
 * |X, |T: Search your library for a Rebel permanent card with converted mana cost X or less and put it onto the battlefield.
 *		Then shuffle your library.
 * |3: Put target Rebel card from your graveyard on the bottom of your library. */

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance( player, card )->targets[1].card = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select target Rebel card.");
	test.subtype = SUBTYPE_REBEL;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_REBEL_SEARCH = 1,
		CHOICE_REBEL_RECYCLE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER) )
		{
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(-1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(-1), 0, NULL, NULL),
						generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(3), 0,
																		NULL, NULL, 0, &test, GS_GRAVE_RECYCLER)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Rebel search", 	abils[1], 5,
							"Rebel recycle", 	abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		instance->targets[1].card = choice;
		if( choice == CHOICE_REBEL_SEARCH ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(-1), 0, NULL, NULL);
		}
		if( choice == CHOICE_REBEL_RECYCLE ){
			return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL,
																	0, &test, GS_GRAVE_RECYCLER);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->targets[1].card == CHOICE_REBEL_SEARCH ){
			int val = instance->info_slot;
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_PERMANENT, "");
			snprintf(this_test.message, 100, "Select a Rebel permanent card with CMC %d or less.", val);
			this_test.subtype = SUBTYPE_REBEL;
			this_test.cmc = val+1;
			this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
		}
		if( instance->targets[1].card == CHOICE_REBEL_RECYCLE ){
			int selected = validate_target_from_grave_source(player, card, player, 0);
			if( selected != -1 ){
				from_graveyard_to_deck(player, selected, 2);
			}
		}
	}

	return 0;
}

int card_massacre2(int player, int card, event_t event){
/* CARD_ID_MASSACRE	4715
Massacre	|2|B|B
Sorcery
If an opponent controls |Ha Plains and you control |Ha Swamp, you may cast ~ without paying its mana cost.
All creatures get -2/-2 until end of turn. */

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_MODIFY_COST ){
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_SWAMP) && check_battlefield_for_subtype(1-player, TYPE_LAND, SUBTYPE_PLAINS) ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! nemesis_freespell(player, card, event) ){
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p, { pump_subtype_until_eot(player, card, p, -1, -2, -2, 0, 0); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_mind_slash(int player, int card, event_t event){
/* CARD_ID_MIND_SLASH	1088
Mind Slash	|1|B|B
Enchantment
|B, Sacrifice a creature: Target opponent reveals his or her hand. You choose a card from it.
That player discards that card. Activate this ability only any time you could cast a sorcery. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);
		}
	}
	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE | GAA_CAN_TARGET, MANACOST_B(1), 0, &td, "TARGET_PLAYER");
}

int card_mogg_salvage(int player, int card, event_t event){
/* CARD_ID_MOGG_SALVAGE	4718
Mogg Salvage	|2|R
Instant
If an opponent controls |Han Island and you control |Ha Mountain, you may cast ~ without paying its mana cost.
Destroy target artifact. */

	if( event == EVENT_MODIFY_COST ){
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_MOUNTAIN) &&
			check_battlefield_for_subtype(1-player, TYPE_LAND, SUBTYPE_ISLAND) )
		{
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
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! nemesis_freespell(player, card, event) ){
			spell_fizzled = 1;
			return 0;
		}
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_moggcatcher(int player, int card, event_t event){
/* CARD_ID_MOGGCATCHER	4720
Moggcatcher	|2|R|R
Creature - Human Mercenary 2/2
|3, |T: Search your library for a Goblin permanent card and put it onto the battlefield. Then shuffle your library. */

	return subtype_searcher(player, card, event, SUBTYPE_GOBLIN);
}

int card_murderous_betrayal(int player, int card, event_t event){
/*
Murderous Betrayal |B|B|B
Enchantment
{B}{B}, Pay half your life, rounded up: Destroy target nonblack creature. It can't be regenerated.
*/
	if( !IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_B(2),
									(life[player]+1)/2, &td,
									get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK));
}

// nesting wurm --> squadron hawk

int card_netter_en_dal(int player, int card, event_t event){
/* CARD_ID_NETTER_EN_DAL	4723
Netter en-Dal	|W
Creature - Human Spellshaper 1/1
|W, |T, Discard a card: Target creature can't attack this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, effect_cannot_attack_until_eot,
										   instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_NONSICK|GAA_DISCARD|GAA_CAN_TARGET, MANACOST_W(1), 0,
									&td, "TARGET_CREATURE");
}

int card_oracles_attendants(int player, int card, event_t event){
/* CARD_ID_ORACLES_ATTENDANTS	1103
Oracle's Attendants	|3|W
Creature - Human Soldier 1/5
|T: All damage that would be dealt to target creature this turn by a source of your choice is dealt to ~ instead. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_CREATURE;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *source = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			source->damage_target_player = player;
			source->damage_target_card = instance->parent_card;
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST0, 0,
									&td, "TARGET_DAMAGE");
}

int card_overlaid_terrain(int player, int card, event_t event){
/* CARD_ID_OVERLAID_TERRAIN	4727
Overlaid Terrain	|2|G|G
Enchantment
As ~ enters the battlefield, sacrifice all lands you control.
Lands you control have "|T: Add two mana of any one color to your mana pool." */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		new_manipulate_all(player, card, player, &this_test, KILL_SACRIFICE);
	}

	int result = permanents_you_control_can_tap_for_mana_all_one_color(player, card, event, TYPE_LAND, -1, COLOR_TEST_ANY_COLORED, 2);
	if (event == EVENT_CAN_ACTIVATE){
		return result;
	}

	return global_enchantment(player, card, event);
}

int card_pack_hunt(int player, int card, event_t event){
/* CARD_ID_PACK_HUNT	4728
Pack Hunt	|3|G
Sorcery
Search your library for up to three cards with the same name as target creature, reveal them, and put them into your hand. Then shuffle your library.*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			int id = get_card_name(instance->targets[0].player, instance->targets[0].card);
			char msg[100];
			if( id == -1 ){ // A card with no name, probably a face-down card.
				scnprintf(msg, 100, "You won't be able to tutor any card as you're searching for a card with no name.");
			}
			else{
				scnprintf(msg, 100, "Select a card named %s", cards_ptr[ id ]->name);
			}
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, msg);
			this_test.id = id;
			this_test.qty = id != -1 ? 3 : 1; //No need to tutor for 3 cards if you can't get any
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_parallax_inhibitor(int player, int card, event_t event){
/* Parallax Inhibitor	|2
 * Artifact
 * |1, |T, Sacrifice ~: Put a fade counter on each permanent with fading you control. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int count = active_cards_count[player]-1;
		while( count > -1 ){
				if( in_play(player, count) && is_what(player, count, TYPE_PERMANENT) &&
					get_counter_type_by_id(get_id(player, count)) == COUNTER_FADE )
				{
					add_counter(player, count, COUNTER_FADE);
				}
				count--;
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
}

int card_parallax_nexus(int player, int card, event_t event){

	/* Parallax Nexus	|2|B
	 * Enchantment
	 * Fading 5
	 * Remove a fade counter from ~: Target opponent exiles a card from his or her hand. Activate this ability only any time you could cast a sorcery.
	 * When ~ leaves the battlefield, each player returns to his or her hand all cards he or she owns exiled with ~. */

	card_instance_t *instance = get_card_instance( player, card );

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return generic_activated_ability(player, card, event, GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST0, GVC_COUNTER(COUNTER_FADE), &td, NULL);
		}
		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_ANY);

				ec_definition_t this_definition;
				default_ec_definition(instance->targets[0].player, instance->targets[0].player, &this_definition);
				this_definition.ai_selection_mode = AI_MIN_VALUE;
				this_definition.effect = EC_RFG;
				int id = new_effect_coercion(&this_definition, &this_test);
				if( id > -1 && in_play(player, instance->parent_card) ){
					card_instance_t *parent = get_card_instance( player, instance->parent_card);
					if( parent->info_slot < 1 ){
						parent->info_slot = 1;
					}
					int pos = parent->info_slot;
					parent->targets[pos].player = instance->targets[0].player;
					parent->targets[pos].card = id;
					parent->info_slot++;
					create_card_name_legacy(player, instance->parent_card, id);
				}
			}
		}
	}

	if( leaves_play(player, card, event) ){
		int i;
		for(i=1; i<instance->info_slot; i++){
			if( instance->targets[i].player != -1 ){
				if( check_rfg(instance->targets[i].player, instance->targets[i].card) ){
					int to_add = get_internal_card_id_from_csv_id(instance->targets[i].card);
					add_card_to_hand(instance->targets[i].player, to_add);
					remove_card_from_rfg(instance->targets[i].player, instance->targets[i].card);
				}
			}
		}
	}

	return parallax(player, card, event);
}

int card_parallax_wave(int player, int card, event_t event){

	/* Parallax Wave	|2|W|W
	 * Enchantment
	 * Fading 5
	 * Remove a fade counter from ~: Exile target creature.
	 * When ~ leaves the battlefield, each player returns to the battlefield all cards he or she owns exiled with ~. */

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = ANYBODY;

		card_instance_t *instance = get_card_instance( player, card );

		if( event == EVENT_CAN_ACTIVATE ){
			int result = generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_FADE),
													&td, "TARGET_CREATURE");
			if( result ){
				if( ! IS_AI(player) ){
					return 1;
				}
				else{
					if( land_can_be_played & LCBP_REGENERATION ){
						return 99;
					}
					return 1;
				}
			}
		}

		if( event == EVENT_ACTIVATE ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_FADE), &td, "TARGET_CREATURE");
			if( spell_fizzled != 1 ){
				if( instance->targets[0].player == player ){
					if( ! is_token(instance->targets[0].player, instance->targets[0].card) ){
						int kc = get_card_instance(instance->targets[0].player, instance->targets[0].card)->kill_code;
						ai_modifier += ((kc == KILL_DESTROY || kc == KILL_BURY)? 48 : -48);
					}
					else{
						ai_modifier -= 48;
					}
				}
				if( instance->targets[0].player == 1-player ){
					ai_modifier += (is_token(instance->targets[0].player, instance->targets[0].card) ? 48 : 24);
				}
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				exile_card_and_remember_it_on_exiledby(instance->parent_controller, instance->parent_card,
														instance->targets[0].player, instance->targets[0].card);
			}
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

	return parallax(player, card, event);
}

int card_parallax_tide(int player, int card, event_t event){
/* Parallax Tide	|2|U|U
 * Enchantment
 * Fading 5
 * Remove a fade counter from ~: Exile target land.
 * When ~ leaves the battlefield, each player returns to the battlefield all cards he or she owns exiled with ~. */

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.preferred_controller = ANYBODY;

		card_instance_t *instance = get_card_instance( player, card );

		if( event == EVENT_CAN_ACTIVATE ){
			int result = generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_FADE),
													&td, "TARGET_LAND");
			if( result ){
				if( ! IS_AI(player) ){
					return 1;
				}
				else{
					if( land_can_be_played & LCBP_REGENERATION ){
						return 99;
					}
					return 1;
				}
			}
		}

		if( event == EVENT_ACTIVATE ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_FADE), &td, "TARGET_LAND");
			if( spell_fizzled != 1 ){
				if( instance->targets[0].player == player ){
					if( ! is_token(instance->targets[0].player, instance->targets[0].card) ){
						int kc = get_card_instance(instance->targets[0].player, instance->targets[0].card)->kill_code;
						ai_modifier += ((kc == KILL_DESTROY || kc == KILL_BURY)? 48 : -48);
					}
					else{
						ai_modifier -= 48;
					}
				}
				if( instance->targets[0].player == 1-player ){
					ai_modifier += (is_token(instance->targets[0].player, instance->targets[0].card) ? 48 : 24);
				}
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				exile_card_and_remember_it_on_exiledby(instance->parent_controller, instance->parent_card,
														instance->targets[0].player, instance->targets[0].card);
			}
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

	return parallax(player, card, event);
}

int card_plague_witch(int player, int card, event_t event){
/* CARD_ID_PLAGUE_WITCH	4737
Plague Witch	|1|B
Creature - Elf Spellshaper 1/1
|B, |T, Discard a card: Target creature gets -1/-1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							-1, -1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD | GAA_CAN_TARGET, MANACOST_B(1), 0,
									&td, "TARGET_CREATURE");
}

int card_predator_flagship(int player, int card, event_t event){
/*
Predator, Flagship |5
Legendary Artifact
{2}: Target creature gains flying until end of turn.
{5}, {T}: Destroy target creature with flying.
*/
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.required_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_FLYING = 1,
		CHOICE_KILL_FLYIER
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(2), 0, &td, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_X(5), 0, &td1, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_X(2), 0, &td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_X(5), 0, &td1, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Give flying", abils[1], 5,
							"Kill a creature with flying", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_FLYING ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_CREATURE");
		}
		if( choice == CHOICE_KILL_FLYIER ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED | GAA_LITERAL_PROMPT, MANACOST_X(5), 0,
											&td1, "Select target creature with flying.");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_FLYING && valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FLYING, 0);
		}
		if( instance->info_slot == CHOICE_KILL_FLYIER && valid_target(&td1) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

// rackling --> the rack

int card_raths_edge(int player, int card, event_t event){
	/* Rath's Edge	""	0x200872a
	 * Legendary Land
	 * |T: Add |C.
	 * |4, |T, Sacrifice a land: ~ deals 1 damage to any target. */
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAC_DAMAGE
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() && new_can_sacrifice_as_cost(player, card, &test) &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_X(5), 0, &td, NULL) )
		{
			choice = CHOICE_SAC_DAMAGE;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_SAC_DAMAGE && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Sac a land & damage target", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_SAC_DAMAGE ){
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			if( sac ){
				if( BYTE3(sac) == card ){
					ai_modifier -= 96;
				}
				add_state(player, card, STATE_TAPPED);
				state_untargettable(BYTE2(sac), BYTE3(sac), 0);
				generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(4), 0, &td, "TARGET_ANY");
				if( spell_fizzled != 1 ){
					kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				}
				else{
					remove_state(player, card, STATE_TAPPED);
				}
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAC_DAMAGE ){
			damage_target0(player, card, 1);
		}
	}

	return 0;
}

int card_rathi_assassin(int player, int card, event_t event){
/*
Rathi Assassin |2|B|B|
Creature — Zombie Mercenary Assassin 2/2
{1}{B}{B}, {T}: Destroy target tapped nonblack creature.
{3}, {T}: Search your library for a Mercenary permanent card with converted mana cost 3 or less and put it onto the battlefield.
	Then shuffle your library.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
	td.required_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_KILL = 1,
		CHOICE_TUTOR
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_XB(1, 2), 0, &td, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_XB(1, 2), 0, &td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL)
		};
		char msg[100];
		scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Kill tapped non%s crit", COLOR_BLACK));
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							msg, abils[1], 5,
							"Tutor a Mercenary", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_KILL ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_XB(1, 2), 0,
											&td, get_sleighted_color_text(player, card, "Select target tapped non%s creature.", COLOR_BLACK));
		}
		if( choice == CHOICE_TUTOR ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_KILL && valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( instance->info_slot == CHOICE_TUTOR ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a Mercenary card with CMC 3 or less.");
			this_test.subtype = SUBTYPE_MERCENARY;
			this_test.cmc = 4;
			this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

int card_rathi_fiend(int player, int card, event_t event){
/* CARD_ID_RATHI_FIEND	4742
Rathi Fiend	|3|B
Creature - Horror Mercenary 2/2
When ~ enters the battlefield, each player loses 3 life.
|3, |T: Search your library for a Mercenary permanent card with converted mana cost 3 or less and put it onto the battlefield. Then shuffle your library.*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		lose_life(player, 3);
		lose_life(1-player, 3);
	}

	return mercenary_searcher(player, card, event, 3);
}

int card_rathi_intimidator(int player, int card, event_t event){
/* CARD_ID_RATHI_INTIMIDATOR	4743
Rathi Intimidator	|1|B|B
Creature - Horror Mercenary 2/1
Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
|2, |T: Search your library for a Mercenary permanent card with converted mana cost 2 or less and put it onto the battlefield. Then shuffle your library.*/

	fear(player, card, event);
	return mercenary_searcher(player, card, event, 2);
}

int card_refreshing_rain(int player, int card, event_t event){
/* CARD_ID_REFRESHING_RAIN	4744
Refreshing Rain	|3|G
Instant
If an opponent controls |Ha Swamp and you control |Ha Forest, you may cast ~ without paying its mana cost.
Target player gains 6 life. */

	if( event == EVENT_MODIFY_COST ){
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_FOREST) &&
			check_battlefield_for_subtype(1-player, TYPE_LAND, SUBTYPE_SWAMP) )
		{
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
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! nemesis_freespell(player, card, event) ){
			spell_fizzled = 1;
		}
		else{
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			gain_life(instance->targets[0].player, 6);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_reverent_silence(int player, int card, event_t event){
/* CARD_ID_REVERENT_SILENCE	4746
Reverent Silence	|3|G
Sorcery
If you control |Ha Forest, rather than pay ~'s mana cost, you may have each other player gain 6 life.
Destroy all enchantments. */

	if( event == EVENT_MODIFY_COST ){
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_FOREST) )
		{
			null_casting_cost(player, card);
			get_card_instance(player, card)->info_slot = 1;
		}
		else{
			get_card_instance(player, card)->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && get_card_instance(player, card)->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_id(player, event, get_id(player, card)) ){
				choice = do_dialog(player, player, card, -1, -1, " Make opponent gain 6 life\n Play normally\n Cancel", 1);
			}
			if( choice == 0 ){
				gain_life(1-player, 6);
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT);
		APNAP(p, { new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_rhox(int player, int card, event_t event){
/* CARD_ID_RHOX	1132
Rhox	|4|G|G
Creature - Rhino Beast 5/5
You may have ~ assign its combat damage as though it weren't blocked.
|2|G: Regenerate ~. (The next time this creature would be destroyed this turn, it isn't. Instead tap it, remove all damage from it, and remove it from combat.)*/

	card_thorn_elemental(player, card, event);
	return regeneration(player, card, event, 2, 0, 0, 1, 0, 0);
}

int card_rising_waters(int player, int card, event_t event){
/* CARD_ID_RISING_WATERS	4747
Rising Waters	|3|U
Enchantment
Lands don't untap during their controllers' untap steps.
At the beginning of each player's upkeep, that player untaps a land he or she controls. */

	untap_only_1_permanent_per_upkeep(player, card, event, 2, TYPE_LAND);
	return global_enchantment(player, card, event);
}

int card_rupture(int player, int card, event_t event){
/*
Rupture |2|R
Sorcery
Sacrifice a creature. Rupture deals damage equal to that creature's power to each creature without flying and each player.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int result = new_sacrifice(player, card, player, SAC_NO_CANCEL | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
		if( result ){
			int pow = get_power(BYTE2(result), BYTE3(result));
			kill_card(BYTE2(result), BYTE3(result), KILL_SACRIFICE);

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.keyword = KEYWORD_FLYING;
			this_test.keyword_flag = DOESNT_MATCH;
			new_damage_all(player, card, ANYBODY, pow, NDA_PLAYER_TOO, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_saproling_burst(int player, int card, event_t event){

	/* Saproling Burst	|4|G
	 * Enchantment
	 * Fading 7
	 * Remove a fade counter from ~: Put a |Sgreen Saproling creature token onto the battlefield. It has "This creature's power and toughness are each equal to
	 * the number of fade counters on ~."
	 * When ~ leaves the battlefield, destroy all tokens put onto the battlefield with ~. They can't be regenerated. */

	fading(player, card, event, 7);

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance= get_card_instance(player, card);

		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING, &token);
		token.pow = token.tou = count_counters(player, card, COUNTER_FADE);
		token.special_infos = 66;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTER(COUNTER_FADE), NULL, NULL);
}

static int saproling_cluster_legacy(int player, int card, event_t event ){

	if( get_card_instance(player, card)->targets[0].player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->targets[0].player;
		int c = instance->targets[0].card;

		if( event == EVENT_CAN_ACTIVATE ){
			if( can_use_activated_abilities(p, c) && hand_count[player] > 0 ){
				int cless = get_cost_mod_for_activated_abilities(p, c, MANACOST_X(1));
				if( has_mana(player, COLOR_COLORLESS, cless) ){
					return 1;
				}
			}
		}

		if( event == EVENT_ACTIVATE ){
			int cless = get_cost_mod_for_activated_abilities(p, c, MANACOST_X(1));
			charge_mana(player, COLOR_COLORLESS, cless);
			if( spell_fizzled != 1 ){
				discard(player, 0, player);
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			generate_token_by_id(p, c, CARD_ID_SAPROLING);
		}

		if( other_leaves_play(player, card, p, c, event) ){
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_saproling_cluster(int player, int card, event_t event ){//UNUSEDCARD
	/* Saproling Cluster	|1|G
	 * Enchantment
	 * |1, Discard a card: Put a 1/1 |Sgreen Saproling creature token onto the battlefield. Any player may activate this ability. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found[2] = {-1};
		CYCLE_ALL_CARDS(p, c, 	{
									if( in_play(p, c) && is_what(p, c, TYPE_EFFECT) ){
										card_instance_t *inst = get_card_instance(p, c);
										if( inst->info_slot == (int)saproling_cluster_legacy ){
											if( inst->targets[0].player == player && inst->targets[0].card == card ){
												found[p] = c;
											}
										}
									}
								};
		);
		if( found[player] > -1 ){
			kill_card(player, found[player], KILL_EXILE);
		}
		if( found[1-player] == -1 ){
			int fake = add_card_to_hand(1-player, get_original_internal_card_id(player, card));
			int legacy = create_legacy_effect(1-player, fake, &saproling_cluster_legacy);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			obliterate_card(1-player, fake);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST_X(1), 0, NULL, NULL);
}

int card_seahunter(int player, int card, event_t event){
	/* Seahunter	|2|U|U
	 * Creature - Human Mercenary 2/2
	 * |3, |T: Search your library for a Merfolk permanent card and put it onto the battlefield. Then shuffle your library. */
	return subtype_searcher(player, card, event, SUBTYPE_MERFOLK);
}

// seal of cleansing --> seal of the primordium

int card_seal_of_doom(int player, int card, event_t event){
/* CARD_ID_SEAL_OF_DOOM	4756
Seal of Doom	|2|B
Enchantment
Sacrifice ~: Destroy target non|Sblack creature. It can't be regenerated. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0,
									&td, get_sleighted_color_text(player, card, "Select target non%s creature", COLOR_BLACK));
}

int card_seal_of_fire(int player, int card, event_t event){
	/* Seal of Fire	|R	0x2007848
	 * Enchantment
	 * Sacrifice ~: It deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_creature_or_player(player, card, event, 2);
		}
	}

	return seal(player, card, event, &td, "TARGET_ANY");
}

int card_seal_of_removal(int player, int card, event_t event){
/* CARD_ID_SEAL_OF_REMOVAL	4758
Seal of Removal	|U
Enchantment
Sacrifice ~: Return target creature to its owner's hand. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return seal(player, card, event, &td, "TARGET_CREATURE");
}

int card_seal_of_strength(int player, int card, event_t event){
/* CARD_ID_SEAL_OF_STRENGTH	4759
Seal of Strength	|G
Enchantment
Sacrifice ~: Target creature gets +3/+3 until end of turn. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

    if (event == EVENT_CHECK_PUMP )
	{
	     if(seal(player, card, EVENT_CAN_ACTIVATE, &td, "TARGET_CREATURE")){
	        pumpable_power[player] += 3;
	        pumpable_toughness[player] += 3;
	     }
    }

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3, 3);
		}
	}
	
	return seal(player, card, event, &td, "TARGET_CREATURE");
}

// shrieking mogg --> thundermare

static int sivvis_rouse_legacy(int player, int card, event_t event){

	if( (event == EVENT_ATTACK_RATING || event == EVENT_BLOCK_RATING) && affected_card_controller == player ){
		if(event == EVENT_ATTACK_RATING){
			ai_defensive_modifier -= 24;
		}
		else{
			ai_defensive_modifier += 24;
		}
	}

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *source = get_card_instance(affected_card_controller, affected_card);
		if( damage_card == source->internal_card_id && source->info_slot > 0 ){
			if( source->damage_target_player == player && source->damage_target_card != -1 ){
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_sivvis_rouse(int player, int card, event_t event){
/*
Sivvi's Ruse |2|W|W
Instant
If an opponent controls a Mountain and you control a Plains, you may cast Sivvi's Ruse without paying its mana cost.
Prevent all damage that would be dealt this turn to creatures you control.
*/
	if( event == EVENT_MODIFY_COST ){
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_PLAINS) &&
			check_battlefield_for_subtype(1-player, TYPE_LAND, SUBTYPE_MOUNTAIN) )
		{
			null_casting_cost(player, card);
			get_card_instance( player, card )->info_slot = 1;
		}
		else{
			get_card_instance( player, card )->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! nemesis_freespell(player, card, event) ){
			spell_fizzled = 1;
		}
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1)*8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &sivvis_rouse_legacy);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int sivvis_valor_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *source = get_card_instance(affected_card_controller, affected_card);
			if( damage_card == source->internal_card_id && source->info_slot > 0 ){
				if( source->damage_target_player == p && source->damage_target_card == c ){
					source->damage_target_player = player;
					source->damage_target_card = -1;
				}
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_sivvis_valor(int player, int card, event_t event){
	/*
	  Sivvi's Valor |2|W
	  Instant
	  If you control a Plains, you may tap an untapped creature you control rather than pay Sivvi's Valor's mana cost.
	  All damage that would be dealt to target creature this turn is dealt to you instead.
	*/
	if( event == EVENT_MODIFY_COST ){
		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.allowed_controller = player;
		td2.preferred_controller = player;
		td2.illegal_abilities = 0;
		td2.illegal_state = TARGET_STATE_TAPPED;

		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_PLAINS) && can_target(&td2) ){
			get_card_instance( player, card )->info_slot = 1;
			null_casting_cost(player, card);
		}
		else{
			get_card_instance( player, card )->info_slot = 0;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){

		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_iid(player, get_card_instance(player, card)->internal_card_id, event) ){
				choice = do_dialog(player, player, card, -1, -1, " Tap a creature you control\n Play normally\n Cancel", 0);
			}
			if( choice == 0 ){
				target_definition_t td2;
				default_target_definition(player, card, &td2, TYPE_CREATURE);
				td2.allowed_controller = player;
				td2.preferred_controller = player;
				td2.illegal_abilities = 0;
				td2.illegal_state = TARGET_STATE_TAPPED;
				if( new_pick_target(&td2, "Select a creature you control to tap.", 0, 1 | GS_LITERAL_PROMPT) ){
					action_on_target(player, card, 0, ACT_TAP);
					td.allow_cancel = 0;
					instance->number_of_targets = 0;
				}
				else{
					spell_fizzled = 1;
					return 0;
				}
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		ai_modifier += 24;
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &sivvis_valor_legacy, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	if( event == EVENT_CAN_CHANGE_TARGET || event == EVENT_CHANGE_TARGET ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	return 0;
}

int card_skyshroud_behemoth(int player, int card, event_t event){
	/* Skyshroud Behemoth	|5|G|G
	 * Creature - Beast 10/10
	 * Fading 2
	 * ~ enters the battlefield tapped. */
	comes_into_play_tapped(player, card, event);
	fading(player, card, event, 2);
	return 0;
}

int card_skyshroud_claim(int player, int card, event_t event){
/* CARD_ID_SKYSHROUD_CLAIM	4766
Skyshroud Claim	|3|G
Sorcery
Search your library for up to two |H2Forest cards and put them onto the battlefield. Then shuffle your library. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, get_hacked_land_text(player, card, "Select a %s card.", SUBTYPE_FOREST));
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
		this_test.qty = 2;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
		shuffle(player);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_skyshroud_poacher(int player, int card, event_t event){
/* CARD_ID_SKYSHROUD_POACHER	4768
Skyshroud Poacher	|2|G|G
Creature - Human Rebel 2/2
|3, |T: Search your library for an Elf permanent card and put it onto the battlefield. Then shuffle your library. */

	return subtype_searcher(player, card, event, SUBTYPE_ELF);
}

int card_skyshroud_ridgeback(int player, int card, event_t event){
/* CARD_ID_SKYSHROUD_RIDGEBACK	4769
Skyshroud Ridgeback	|G
Creature - Beast 2/3
Fading 2 (This creature enters the battlefield with two fade counters on it. At the beginning of your upkeep, remove a fade counter from it.
If you can't, sacrifice it.) */

	fading(player, card, event, 2);
	return 0;
}

// skyshroud sentinel --> squadron hawk

int card_sneaky_homunculus(int player, int card, event_t event){
	/*
	  Sneaky Homunculus |1|U
	  Creature - Homunculus Illusion 1/1
	  Sneaky Homunculus can't block or be blocked by creatures with power 2 or greater.
	*/
	if( ! is_humiliated(player, card) ){
		if( event == EVENT_BLOCK_LEGALITY ){
			if( affect_me(player, card) ){
				if( get_power(attacking_card_controller, attacking_card) > 1 ){
					event_result = 1;
				}
			}
			if( player == attacking_card_controller && card == attacking_card ){
				if( get_power(affected_card_controller, affected_card) > 1 ){
					event_result = 1;
				}
			}
		}
	}
	return 0;
}

int card_spiritual_asylum(int player, int card, event_t event)
{
/* CARD_ID_SPIRITUAL_ASYLUM	4772
Spiritual Asylum	|2|W|W
Enchantment
Creatures and lands you control have shroud. (They can't be the targets of spells or abilities.)
When a creature you control attacks, sacrifice ~. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		// Creatures and lands you control have shroud.
		if (event == EVENT_ABILITIES && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE | TYPE_LAND) )
			event_result |= KEYWORD_SHROUD;

		// When a creature you control attacks, sacrifice ~.
		// Simplification: only check once per attack.
		if (declare_attackers_trigger(player, card, event, 0, player, -1))
			kill_card(player, card, KILL_SACRIFICE);
	}

	return global_enchantment(player, card, event);
}

int card_stampede_driver(int player, int card, event_t event){
/* CARD_ID_STAMPEDE_DRIVER	4774
Stampede Driver	|G
Creature - Human Spellshaper 1/1
|1|G, |T, Discard a card: Creatures you control get +1/+1 and gain trample until end of turn. */

	if( event == EVENT_ACTIVATE ){
		if( current_turn == player && current_phase == PHASE_AFTER_BLOCKING ){
			ai_modifier += (count_attackers(current_turn)*8);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1, 1, 1, KEYWORD_TRAMPLE, 0);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD, MANACOST_G(1), 0, NULL, NULL);
}

int card_stronghold_biologist(int player, int card, event_t event){
/* CARD_ID_STRONGHOLD_BIOLOGIST	4775
Stronghold Biologist	|2|U
Creature - Human Spellshaper 1/1
|U|U, |T, Discard a card: Counter target creature spell. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( counterspell_validate(player, card, &td, 0) ){
			real_counter_a_spell(instance->parent_controller, instance->parent_card,
								instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD | GAA_SPELL_ON_STACK, MANACOST_U(2), 0, &td, NULL);
}

int card_stronghold_machinist(int player, int card, event_t event){
/* CARD_ID_STRONGHOLD_MACHINIST	4778
Stronghold Machinist	|2|U
Creature - Human Spellshaper 1/1
|U|U, |T, Discard a card: Counter target noncreature spell. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);
	td.illegal_type = TYPE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( counterspell_validate(player, card, &td, 0) ){
			real_counter_a_spell(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD | GAA_SPELL_ON_STACK, MANACOST_U(2), 0, &td, NULL);
}

int card_tangle_wire(int player, int card, event_t event){
	if( ! is_unlocked(player, card, event, 15) ){ return 0; }

	/* Tangle Wire	|3
	 * Artifact
	 * Fading 4
	 * At the beginning of each player's upkeep, that player taps an untapped artifact, creature, or land he or she controls for each fade counter on ~. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_FADE, 4);

	card_instance_t *instance = get_card_instance(player, card);

	upkeep_trigger_ability(player, card, event, 2);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( current_turn == player ){
			if( count_counters(player, card, COUNTER_FADE) <= 0 ){
				kill_card(player, card, KILL_SACRIFICE);
			}
			else{
				remove_counter(player, card, COUNTER_FADE);
			}
		}
		int to_tap = count_counters(player, card, COUNTER_FADE);
		if( to_tap > 0 ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_CREATURE | TYPE_LAND );
			td.illegal_state = TARGET_STATE_TAPPED;
			td.allowed_controller = current_turn;
			td.preferred_controller = current_turn;
			td.who_chooses = current_turn;
			td.illegal_abilities = 0;
			td.allow_cancel = 0;

			int i;
			for(i=0;i<to_tap;i++){
				if( can_target(&td) && new_pick_target(&td, "Chosse an artifact, creature or land to tap.", i, 0) ){
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
	}

	return 0;
}

int card_trickster_mage(int player, int card, event_t event){
/* CARD_ID_TRICKSTER_MAGE	4785
Trickster Mage	|U
Creature - Human Spellshaper 1/1
|U, |T, Discard a card: You may tap or untap target artifact, creature, or land. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_LAND);
	td.preferred_controller = ANYBODY;

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD | GAA_CAN_TARGET, MANACOST_U(1), 0, &td, "TWIDDLE");
		if( spell_fizzled != 1 ){
			ai_modifier_twiddle(player, card, 0);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			twiddle(player, card, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD | GAA_CAN_TARGET, MANACOST_U(1), 0, &td, "TWIDDLE");
}

int card_vicious_hunger(int player, int card, event_t event){
	// Also code for Douse in Gloom and Sorin't Thirst
/* CARD_ID_VICIOUS_HUNGER	1185
Vicious Hunger	|B|B
Sorcery
~ deals 2 damage to target creature and you gain 2 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 2, player, card);
			gain_life(player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

// viseling --> black vise
/* CARD_ID_VISELING	4786
Viseling	|4
Artifact Creature - Construct 2/2
At the beginning of each opponent's upkeep, ~ deals X damage to that player, where X is the number of cards in his or her hand minus 4. */

int card_volrath_the_fallen(int player, int card, event_t event){
/* CARD_ID_VOLRATH_THE_FALLEN	4787
Volrath the Fallen	|3|B|B|B
Legendary Creature - Shapeshifter 6/4
|1|B, Discard a creature card: ~ gets +X/+X until end of turn, where X is the discarded card's converted mana cost. */

	check_legend_rule(player, card, event);

	if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
	{
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card to discard.");
		this_test.zone = TARGET_ZONE_HAND;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE , 0, MANACOST_XB(1, 1), 0, NULL, NULL) ){
			if( check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &this_test) ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
				return get_cmc(player, selected);
			}
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card to discard.");
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_XB(1, 1), 0, NULL, NULL) ){
			return check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &this_test);
		}
	}

	if(event == EVENT_ACTIVATE ){
		charge_mana_for_activated_ability(player, card, MANACOST_XB(1, 1));
		if( spell_fizzled != 1 ){
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
			if( selected != -1 ){
				instance->info_slot = get_cmc(player, selected);
				new_discard_card(player, selected, 0, player);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
						instance->info_slot, instance->info_slot);
	}

	return 0;
}

int card_wild_mammoth(int player, int card, event_t event){
/* CARD_ID_WILD_MAMMOTH	4789
Wild Mammoth	|2|G
Creature - Elephant 3/4
At the beginning of your upkeep, if a player controls more creatures than each other player, the player who controls the most creatures gains control of ~.*/

	if( current_turn == player && upkeep_trigger(player, card, event) ){
		if( count_subtype(player, TYPE_CREATURE, -1) < count_subtype(1-player, TYPE_CREATURE, -1) ){
			give_control_of_self(player, card);
		}
	}

	return 0;
}

int card_woodripper(int player, int card, event_t event){

	/* Woodripper	|3|G|G
	 * Creature - Beast 4/6
	 * Fading 3
	 * |1, Remove a fade counter from ~: Destroy target artifact. */

	fading(player, card, event, 3);

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), GVC_COUNTER(COUNTER_FADE), &td, "TARGET_ARTIFACT");
}
