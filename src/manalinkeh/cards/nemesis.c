#include "manalink.h"

// Functions

static char *greatest_power_among_creatures(int who_chooses, int player, int card);

static int mana_cache_legacy(int player, int card, event_t event);

int pale_moon_legacy(int player, int card, event_t event);

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

	sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);
	if (event == EVENT_SPA){
		if (!has_mana(trigger_cause_controller, COLOR_COLORLESS, 1) || !charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, trigger_cause_controller, COLOR_COLORLESS, 1)){
			player_sacrifices_a_creature(player, card, trigger_cause_controller);
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
					tapped_for_mana_color = -1;
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
  /* Blinding Angel |3|W|W
   * Creature — Angel 2/4
   * Flying (This creature can't be blocked except by creatures with flying or reach.)
   * Whenever Blinding Angel deals combat damage to a player, that player skips his or her next combat phase. */

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

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
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
  /* Dark Triumph |4|B
   * 	  Instant
   * 	  If you control a Swamp, you may sacrifice a creature rather than pay Dark Triumph's mana cost.
   * 	  Creatures you control get +2/+0 until end of turn. */

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
  /* Daze |1|U
   * 	  Instant
   * 	  You may return an Island you control to its owner's hand rather than pay Daze's mana cost.
   * 	  Counter target spell unless its controller pays {1}. */

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
  /* Death Pit Offering |2|B|B
   * 	  Enchantment
   * 	  When Death Pit Offering enters the battlefield, sacrifice all creatures you control.
   * 	  Creatures you control get +2/+2. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		new_manipulate_all(player, card, player, &test, KILL_SACRIFICE);
	}

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
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

	if( current_turn != player && event == EVENT_DECLARE_BLOCKERS && ! is_humiliated(player, card, AL_TRIG) ){
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
  /* Flame Rift |1|R
   * 	  Sorcery
   * 	  Flame Rift deals 4 damage to each player. */

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
  /* Kor Haven	""
   * Legendary Land
   * |T: Add |C.
   * |1|W, |T: Prevent all combat damage that would be dealt by target attacking creature this turn. */

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
				tapped_for_mana_color = -1;
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
	new_default_test_definition(&test, TYPE_ANY, "Select target Rebel card, put it on the bottom of your library.");
	test.subtype = SUBTYPE_REBEL;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_REBEL_SEARCH = 1,
		CHOICE_REBEL_RECYCLE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_X(3), 0, NULL, NULL,
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
						generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(3), 0,
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
			return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_X(3), 0, NULL, NULL,
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
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! nemesis_freespell(player, card, event) ){
			spell_fizzled = 1;
			return 0;
		}
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
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
  /* Murderous Betrayal |B|B|B
   * Enchantment
   * {B}{B}, Pay half your life, rounded up: Destroy target nonblack creature. It can't be regenerated. */

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

/* ManalinkEh 2022: 0x20902e0; original cards/nemesis.c:1250. */
int pale_moon_legacy(int player, int card, event_t event)
{
  int p;
  int card_00;

  if (event == EVENT_CAST_SPELL)
  {
    if (is_what(affected_card_controller, affected_card, 1) != 0)
    {
      set_special_flags2(affected_card_controller, affected_card, 0x40);
    }
  }
  else if (event == EVENT_MODIFY_MANA_PROD)
  {
    for (p = 0; p < 2; ++p)
    {
      for (int color = 0; color < 7; ++color)
        mana_color_override[p][color] = 0;
      for (card_00 = 0; card_00 < active_cards_count[p]; ++card_00)
        if (in_play(p, card_00) && is_what(p, card_00, 1) &&
            !has_subtype(p, card_00, SUBTYPE_BASIC))
          set_special_flags2(p, card_00, 0x40);
    }
  }
  if (eot_trigger(player, card, event) != 0)
  {
    kill_card(player, card, KILL_EXILE);
    cant_be_responded_to = 1;
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2094700; original cards/nemesis.c:1279. */
int card_pale_moon(int player, int card, event_t event)
{
  /* Pale Moon	|1|U
   * Instant
   * Until end of turn, if a player taps a nonbasic land for mana, it produces colorless mana instead of any other type. */

  int p;
  int card_00;

  if (event == EVENT_RESOLVE_SPELL)
  {
    for (p = 0; p < 2; ++p)
    {
      for (int color = 0; color < 7; ++color)
        mana_color_override[p][color] = 0;
      for (card_00 = 0; card_00 < active_cards_count[p]; ++card_00)
        if (in_play(p, card_00) && is_what(p, card_00, 1) &&
            !has_subtype(p, card_00, SUBTYPE_BASIC))
          set_special_flags2(p, card_00, 0x40);
    }
    create_legacy_effect(player, card, pale_moon_legacy);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

int card_parallax_inhibitor(int player, int card, event_t event)
{
  /* Parallax Inhibitor	|2
   * Artifact
   * |1, |T, Sacrifice ~: Put a fade counter on each permanent with fading you control. */

  int permanent_index;
  card_instance_t *instance;
  int counter_type_or_result;

  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (permanent_index = active_cards_count[player] + -1, -1 < permanent_index))
  {
    do
    {
      instance = in_play(player, permanent_index);
      if ((instance != NULL) && (counter_type_or_result = is_what(player, permanent_index, 0x47),
                                 counter_type_or_result != 0))
      {
        counter_type_or_result = get_counter_type_by_id(get_id(player, permanent_index));
        if (counter_type_or_result == 0x29)
        {
          add_counters(player, card, player, permanent_index, COUNTER_FADE, 1);
        }
      }
      permanent_index = permanent_index + -1;
    } while (permanent_index != -1);
  }
  return generic_activated_ability(player, card, event, 9, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
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

	leaves_play(player, card, event, 1, 0);
	if (event == EVENT_LEAVES_PLAY_ABILITY){
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

	leaves_play(player, card, event, 1, 0);
	if (event == EVENT_LEAVES_PLAY_ABILITY){
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

	leaves_play(player, card, event, 1, 0);
	if (event == EVENT_LEAVES_PLAY_ABILITY){
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

	if (event == EVENT_CHECK_ABILITIES){
		if (affected_card_controller == 1-player && card_plague_witch(player, card, EVENT_CAN_ACTIVATE)){
			--check_abilities_power;
			--check_abilities_toughness;
		}
		return 0;
	}

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
  /* Predator, Flagship |5
   * Legendary Artifact
   * {2}: Target creature gains flying until end of turn.
   * {5}, {T}: Destroy target creature with flying. */

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
  /* Rathi Assassin |2|B|B|
   * Creature — Zombie Mercenary Assassin 2/2
   * {1}{B}{B}, {T}: Destroy target tapped nonblack creature.
   * {3}, {T}: Search your library for a Mercenary permanent card with converted mana cost 3 or less and put it onto the battlefield.
   * 	Then shuffle your library. */

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
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_FOREST) && player_can_gain_life_as_cost(1-player) )
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
		new_manipulate_all(player, card, ANYBODY, &this_test, ACT_KILL_DESTROY);

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
  /* Rupture |2|R
   * Sorcery
   * Sacrifice a creature. Rupture deals damage equal to that creature's power to each creature without flying and each player. */

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
		token.pow = token.tou = 0x4000;
		token.special_infos = 66;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTER(COUNTER_FADE), NULL, NULL);
}

static int saproling_cluster_legacy(int player, int card, event_t event)
{
  int p;
  int target_card;
  card_instance_t *instance;
  int source_available;
  int source_player_cost_or_id;
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_player_cost_or_id = get_id(p, instance->targets[0].card);
        if (source_player_cost_or_id != 0x1291)
        {
          kill_card(player, card, KILL_EXILE);
        }
        instance = in_play(instance->targets[0].player, instance->targets[0].card);
        if (instance == NULL)
        {
          kill_card(player, card, KILL_EXILE);
        }
      }
    }
    instance = get_card_instance(player, card);
    if (instance->targets[0].player != -1)
    {
      get_card_instance(player, card);
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if (instance->targets[0].player != -1)
    {
      instance = get_card_instance(player, card);
      source_player_cost_or_id = instance->targets[0].player;
      target_card = instance->targets[0].card;
      if (event == EVENT_CAN_ACTIVATE)
      {
        source_available = can_use_activated_abilities(source_player_cost_or_id, target_card);
        if ((source_available != 0) && (0 < hand_count[player]))
        {
          source_player_cost_or_id = get_cost_mod_for_activated_abilities(
              source_player_cost_or_id, target_card, 1, 0, 0, 0, 0, 0);
          if (has_mana(player, 0, source_player_cost_or_id) != 0)
          {
            return 1;
          }
        }
      }
      else if (event == EVENT_ACTIVATE)
      {
        charge_mana(player, COLOR_COLORLESS,
                    get_cost_mod_for_activated_abilities(source_player_cost_or_id, target_card, 1, 0, 0,
                                                         0, 0, 0));
        if (spell_fizzled != 1)
        {
          discard(player, 0, player);
        }
      }
      else if (event == EVENT_RESOLVE_ACTIVATION)
      {
        generate_token_by_id(player, card, CARD_ID_SAPROLING);
        return 0;
      }
    }
  }
  return 0;
}

int card_saproling_cluster(int player, int card, event_t event)
{
  /* Saproling Cluster	|1|G
   * Enchantment
   * |1, Discard a card: Put a 1/1 |Sgreen Saproling creature token onto the battlefield. Any player
   * may activate this ability. */

 // UNUSEDCARD


  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    int found[2] = {-1, -1};
    CYCLE_ALL_CARDS(p, c, {
      if (in_play(p, c) && is_what(p, c, TYPE_EFFECT))
      {
        card_instance_t *inst = get_card_instance(p, c);
        if (inst->info_slot == (int)saproling_cluster_legacy)
        {
          if (inst->targets[0].player == player && inst->targets[0].card == card)
          {
            found[p] = c;
          }
        }
      }
    };);
    if (found[player] > -1)
    {
      kill_card(player, found[player], KILL_EXILE);
    }
    if (found[1 - player] == -1)
    {
      int fake = add_card_to_hand(1 - player, get_card_instance(player, card)->internal_card_id);
      int legacy = create_legacy_activate(1 - player, fake, &saproling_cluster_legacy);
      card_instance_t *inst = get_card_instance(1 - player, legacy);
      inst->targets[0].player = player;
      inst->targets[0].card = card;
      inst->number_of_targets = 1;
      obliterate_card(1 - player, fake);
    }
  }

  if (!IS_GAA_EVENT(event))
  {
    return global_enchantment(player, card, event);
  }

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
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
			damage_target0(player, card, 2);
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
  /* Sivvi's Ruse |2|W|W
   * Instant
   * If an opponent controls a Mountain and you control a Plains, you may cast Sivvi's Ruse without paying its mana cost.
   * Prevent all damage that would be dealt this turn to creatures you control. */

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
  /* Sivvi's Valor |2|W
   * 	  Instant
   * 	  If you control a Plains, you may tap an untapped creature you control rather than pay Sivvi's Valor's mana cost.
   * 	  All damage that would be dealt to target creature this turn is dealt to you instead. */

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
  /* Sneaky Homunculus |1|U
   * 	  Creature - Homunculus Illusion 1/1
   * 	  Sneaky Homunculus can't block or be blocked by creatures with power 2 or greater. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
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

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		// Creatures and lands you control have shroud.
		if (event == EVENT_ABILITIES && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE | TYPE_LAND) )
			event_result |= KEYWORD_SHROUD;

		// When a creature you control attacks, sacrifice ~.
		// Simplification: only check once per attack.
		store_attackers(player, card, event, 0, player, -1, NULL);
		dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
		if (event == EVENT_DAT_ABILITY)
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

/* ManalinkEh 2022: 0x2098360; original cards/nemesis.c:2556. */
int card_submerge(int player, int card, event_t event)
{
  /* Submerge	|4|U
   * Instant
   * If an opponent controls a Forest and you control an Island, you may cast this spell without paying its mana cost.
   * Put target creature on top of its owner's library. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_MODIFY_COST)
  {
    result = get_hacked_subtype(player, card, 0x5001);
    result = check_battlefield_for_subtype(1 - player, 1, result);
    if (result != 0)
    {
      result = get_hacked_subtype(player, card, 0x5002);
      result = check_battlefield_for_subtype(player, 1, result);
      if (result != 0)
      {
        null_casting_cost(player, card);
        instance = get_card_instance(player, card);
        instance->info_slot = 1;
        return 0;
      }
    }
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = 1 - player;
      get_card_instance(player, card);
      result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
      return result;
    }
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        default_target_definition(player, card, &td, 2);
        td.preferred_controller = 1 - player;
        instance = get_card_instance(player, card);
        if ((card == affected_card) && (player == affected_card_controller))
        {
          instance->number_of_targets = '\0';
          result = nemesis_freespell(player, card, 0x6c);
          if (result != 0)
          {
            result =
                generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
            return result;
          }
          spell_fizzled = 1;
        }
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = 1 - player;
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
        }
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
    }
    else if (event == EVENT_CHANGE_TARGET)
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = 1 - player;
      get_card_instance(player, card);
    }
  }
  return 0;
}

int card_tangle_wire(int player, int card, event_t event)
{
  /* Tangle Wire	|3
   * Artifact
   * Fading 4
   * At the beginning of each player's upkeep, that player taps an untapped artifact, creature, or land he or she controls for each fade counter on ~. */

  int i;
  target_t *target_cursor;
  target_t *other_target_cursor;
  int fade_count;
  card_instance_t *instance;
  int ret_location;
  target_definition_t td;

  if (is_unlocked(player, card, event, 0xf) != 0)
  {
    enters_the_battlefield_with_counters(player, card, event, COUNTER_FADE, 4);
    instance = get_card_instance(player, card);
    upkeep_trigger_ability(player, card, event, 2);
    if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    {
      if (current_turn == player)
      {
        fade_count = count_counters(player, card, COUNTER_FADE);
        if (fade_count < 1)
        {
          kill_card(player, card, KILL_SACRIFICE);
        }
        else
        {
          remove_counters(player, card, player, card, COUNTER_FADE, 1);
        }
      }
      fade_count = count_counters(player, card, COUNTER_FADE);
      if (0 < fade_count)
      {
        default_target_definition(player, card, &td, 0x43);
        td.illegal_state = TARGET_STATE_TAPPED;
        td.allowed_controller = current_turn;
        td.preferred_controller = current_turn;
        td.who_chooses = current_turn;
        td.illegal_abilities = 0;
        td.allow_cancel = 0;
        ret_location = 0;
        do
        {
          if (can_target(&td) == 0)
            break;
          if (new_pick_target(&td, "Select an untapped artifact, creature or land to tap.",
                              ret_location, 0x80) == 0)
            break;
          other_target_cursor = instance->targets + ret_location;
          target_cursor = instance->targets + ret_location;
          ret_location = ret_location + 1;
          state_untargettable(target_cursor->player, other_target_cursor->card, 1);
        } while (fade_count != ret_location);
        i = 0;
        if (instance->number_of_targets != '\0')
        {
          do
          {
            state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
            other_target_cursor = instance->targets + i;
            target_cursor = instance->targets + i;
            i = i + 1;
            tap_card(target_cursor->player, other_target_cursor->card);
          } while (i < (int)(unsigned int)instance->number_of_targets);
        }
        instance->number_of_targets = '\0';
      }
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
  /* CARD_ID_VICIOUS_HUNGER	1185
   * Vicious Hunger	|B|B
   * Sorcery
   * ~ deals 2 damage to target creature and you gain 2 life. */

	// Also code for Douse in Gloom and Sorin't Thirst


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

/* CARD_ID_VISELING	4786
Viseling	|4
Artifact Creature - Construct 2/2
At the beginning of each opponent's upkeep, ~ deals X damage to that player, where X is the number of cards in his or her hand minus 4. */

// viseling --> black vise

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

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2099130; original cards/nemesis.c:2828. */
int card_air_bladder(int player, int card, event_t event)
{
  /* Air Bladder	|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has flying.
   * Enchanted creature can block only creatures with flying. */

  card_instance_t *instance;
  unsigned int attacker_keywords;

  if (event == EVENT_BLOCK_LEGALITY)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (instance->damage_target_card == affected_card)) &&
        (instance->damage_target_player == affected_card_controller))
    {
      attacker_keywords =
          get_abilities(attacking_card_controller, attacking_card, EVENT_ABILITIES, -1);
      if ((attacker_keywords & 0x20) == 0)
      {
        event_result = 1;
      }
    }
  }
  attached_gets_kw(player, card, event, KEYWORD_FLYING, SP_KEYWORD_0);
  return aura(player, card, event, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x2099200; original cards/nemesis.c:2847. */
int card_angelic_favor(int player, int card, event_t event)
{
  /* Angelic Favor	|3|W
   * Instant
   * If you control a Plains, you may tap an untapped creature you control rather than pay this spell's mana cost.
   * Cast this spell only during combat.
   * Create a 4/4 white Angel creature token with flying. Exile it at the beginning of the next end step. */

  card_instance_t *instance;
  card_instance_t *spell_instance;
  int result;

  if (event == EVENT_MODIFY_COST)
  {
    instance = get_card_instance(player, card);
    target_definition_t td2;
    base_target_definition(player, card, &td2, 2);
    td2.allowed_controller = player;
    td2.preferred_controller = player;
    td2.illegal_state = TARGET_STATE_TAPPED;
    result = check_battlefield_for_subtype(player, 1, 0x5007);
    if ((result == 0) || (result = can_target(&td2), result == 0))
    {
      instance->info_slot = 0;
      get_card_instance(player, card);
      return 0;
    }
    instance->info_slot = 1;
    null_casting_cost(player, card);
  }
  else
  {
    if (event != EVENT_CAN_CAST)
    {
      instance = get_card_instance(player, card);
      if (event != EVENT_CAST_SPELL)
      {
        if (event != EVENT_RESOLVE_SPELL)
        {
          return 0;
        }
        token_generation_t token;
        default_token_definition(player, card, CARD_ID_ANGEL, &token);
        token.legacy = 1;
        token.special_code_on_generation = haste_and_remove_token_at_end_of_next_turn;
        generate_token(&token);
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
      instance->number_of_targets = '\0';
      result = played_for_free(player, card);
      if (result != 0)
      {
        return 0;
      }
      result = is_token(player, card);
      if (result != 0)
      {
        return 0;
      }
      if (instance->info_slot != 1)
      {
        return 0;
      }
      spell_instance = get_card_instance(player, card);
      result = has_mana_to_cast_iid(player, spell_instance->internal_card_id, 0x6c);
      if ((result != 0) &&
          (result = do_dialog(player, player, card, -1, -1,
                              " Tap a creature you control\n Play normally\n Cancel", 0),
           result != 0))
      {
        if (result == 1)
        {
          result = get_id(player, card);
          charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
          return 0;
        }
        spell_fizzled = 1;
        return 0;
      }
      target_definition_t td2;
      base_target_definition(player, card, &td2, 2);
      td2.allowed_controller = player;
      td2.preferred_controller = player;
      td2.illegal_state = TARGET_STATE_TAPPED;
      result = new_pick_target(&td2, "Select a creature you control to tap.", 0, 0x81);
      if (result != 0)
      {
        action_on_target(player, card, 0, ACT_TAP);
        instance->number_of_targets = '\0';
        return 0;
      }
      spell_fizzled = 1;
      return 0;
    }
    if (current_phase - 0x15U < 9)
    {
      return basic_spell(player, card, EVENT_CAN_CAST);
    }
  }
  get_card_instance(player, card);
  return 0;
}

/* ManalinkEh 2022: 0x20994e0; original cards/nemesis.c:2923. */
int card_animate_land(int player, int card, event_t event)
{
  /* Animate Land	|G
   * Instant
   * Until end of turn, target land becomes a 3/3 creature that's still a land. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 1);
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      land_animation2(player, card, instance->targets[0].player, instance->targets[0].card, 1,
                      3, 3, 0, 0, 0, NULL);
    }
    kill_card(player, card, KILL_SACRIFICE);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_LAND", 1, NULL);
}

/* ManalinkEh 2022: 0x2099670; original cards/nemesis.c:2949. */
int card_battlefield_percher(int player, int card, event_t event)
{
  /* Battlefield Percher	|3|B|B
   * Creature - Bird 2/2
   * Flying
   * This creature can block only creatures with flying.
   * |1|B: This creature gets +1/+1 until end of turn. */

  int result;

  if (((event == EVENT_BLOCK_LEGALITY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      result = check_for_ability(attacking_card_controller, attacking_card, 0x20);
      if (result == 0)
      {
        event_result = 1;
      }
    }
  }
  result = generic_shade_merge_pt(player, card, event, 0, 1, 1, 0, 0, 0, 0, 1, 1);
  return result;
}

/* ManalinkEh 2022: 0x2099750; original cards/nemesis.c:2965. */
int card_coiling_woodworm(int player, int card, event_t event)
{
  /* Coiling Woodworm	|2|G
   * Creature - Insect Worm * /1
   * ~'s power is equal to the number of Forests on the battlefield. */

  int result;

  if ((((event == EVENT_POWER) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    result = get_hacked_subtype(player, card, 0x5001);
    result = count_subtype(2, 0x47, result);
    set_characteristic_pt(result);
  }
  return 0;
}

/* ManalinkEh 2022: 0x20997c0; original cards/nemesis.c:2983. */
int card_downhill_charge(int player, int card, event_t event)
{
  /* Downhill Charge	|2|R
   * Instant
   * You may sacrifice a Mountain rather than pay this spell's mana cost.
   * Target creature gets +X/+0 until end of turn, where X is the number of Mountains you control. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  test_definition_t test;

  if (event == EVENT_MODIFY_COST)
  {
    default_test_definition(&test, 0x47);
    test.subtype = 0x5006;
    result = new_can_sacrifice_as_cost(player, card, &test);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      null_casting_cost(player, card);
      return 0;
    }
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
    return 0;
  }
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
  }
  if (event != EVENT_CAST_SPELL)
  {
    if ((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET))
    {
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
      default_target_definition(player, card, &td, 2);
      get_card_instance(player, card);
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event != EVENT_RESOLVE_SPELL)
    {
      return 0;
    }
    result = valid_target(&td);
    if (result != 0)
    {

      result = get_hacked_subtype(player, card, 0x5006);
      result = count_subtype(player, 1, result);
      pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, result,
                     0);
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  if (card != affected_card)
  {
    return 0;
  }
  if (player != affected_card_controller)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (card != affected_card)
  {
    return 0;
  }
  if (player != affected_card_controller)
  {
    return 0;
  }
  result = played_for_free(player, card);
  if (((result == 0) && (result = is_token(player, card), result == 0)) &&
      (instance->info_slot == 1))
  {

    result = has_mana_to_cast_iid(player, EVENT_CAST_SPELL, instance->internal_card_id);
    if (result != 0)
    {
      result = count_subtype(player, 1, -1);
      result =
          dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                      "Sacrifice a mountain", 1, result * 2, "Cast normally", 1, 5, 0, 0, 0, 0);
      if (result != 1)
      {
        if (result == 2)
        {
          result = get_id(player, card);
          charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
        }
        else
        {
          spell_fizzled = 1;
        }
        pick_target(&td, "TARGET_CREATURE");
        return 0;
      }
    }
    new_default_test_definition(&test, 0x47, "Select a Mountain to sacrifice.");
    test.subtype = 0x5006;
    new_sacrifice(player, card, player, SAC_AS_COST, &test);
  }
  pick_target(&td, "TARGET_CREATURE");
  return 0;
}

/* ManalinkEh 2022: 0x2099bf0; original cards/nemesis.c:3053. */
int card_eye_of_yawgmoth(int player, int card, event_t event)
{
  /* Eye of Yawgmoth	|3
   * Artifact
   * |3, |T, Sacrifice a creature: Reveal a number of cards from the top of your library equal to the sacrificed creature's power. Put one into your hand and exile the rest. */

  int result;
  card_instance_t *instance;
  int argument2;
  char msg[100];
  test_definition_t test_sac;
  test_definition_t test;

  new_default_test_definition(&test_sac, 2, "Select a creature to sacrifice.");
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);

    result = count_deck(player);
    if (instance->info_slot <= result)
    {
      result = instance->info_slot;
    }
    if (result != 0)
    {

      show_deck(player, deck_ptr[player], result, "Cards revealed by Eye of Yawgmoth", 0,
                gs_done_008b40e0);
      scnprintf(msg, 100, "Select a card to put into your hand.", result);
      new_default_test_definition(&test, 0x47, msg);
      argument2 = select_card_from_zone(player, player, deck_ptr[player], result, 0, 1, -1, &test);
      if (argument2 != -1)
      {
        add_card_to_hand(player, deck_ptr[player][argument2]);
        remove_card_from_deck(player, argument2);
        result = result + -1;
      }
      rfg_top_n_cards_of_deck(player, result);
    }
  }
  return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED, 3, 0,
                                                            0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE,
                                                            &test_sac, SARV_REPORT_POWER);
}

/* ManalinkEh 2022: 0x2099e20; original cards/nemesis.c:3087. */
int card_flint_golem(int player, int card, event_t event)
{
  /* Flint Golem	|4
   * Artifact Creature - Golem 2/3
   * Whenever this creature becomes blocked, defending player mills three cards. */

  int result;

  if (event == EVENT_DECLARE_BLOCKERS)
  {
    result = is_attacking(player, card);
    if (result != 0)
    {
      result = is_unblocked(player, card);
      if (result == 0)
      {
        result = is_humiliated(player, card, AL_TRIG);
        if (result == 0)
        {
          mill(1 - current_turn, 3);
        }
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2099ea0; original cards/nemesis.c:3102. */
int card_flowstone_armor(int player, int card, event_t event)
{
  /* Flowstone Armor	|3
   * Artifact
   * You may choose not to untap this artifact during your untap step.
   * |3, |T: Target creature gets +1/-1 for as long as this artifact remains tapped. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  choose_to_untap(player, card, event);
  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    pump_until_source_is_in_play_and_tapped(
        instance->parent_controller, instance->parent_card, instance->targets[0].player,
        instance->targets[0].card, 1, -1, 0, 0, 1);
  }
  result = generic_activated_ability(player, card, event, 5, 3, 0, 0, 0, 0, 0, 0, &td,
                                          "TARGET_CREATURE");
  return result;
}
#endif

/* ManalinkEh 2022: 0x209a030; original cards/nemesis.c:3128. */
int card_flowstone_strike(int player, int card, event_t event)
{
  /* Flowstone Strike	|1|R
   * Instant
   * Target creature gets +1/-1 and gains haste until end of turn. */

  int result;
  result = vanilla_instant_pump(player, card, event, 2, 1 - player, 1, -1, 0, 4);
  return result;
}

/* ManalinkEh 2022: 0x209a090; original cards/nemesis.c:3137. */
int card_flowstone_thopter(int player, int card, event_t event)
{
  /* Flowstone Thopter	|7
   * Artifact Creature - Thopter 4/4
   * |1: This creature gets +1/-1 and gains flying until end of turn. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
  {
    result = has_mana(player, 7, 1);
    if (result != 0)
      event_result += event == EVENT_POW_BOOST ? 1 : -1;
    return 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                           instance->parent_controller, instance->parent_card, 1, -1, 0x20, 0);
  }
  return generic_activated_ability(player, card, event, 0, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x209a200; original cards/nemesis.c:3179. */
int card_laccolith_rig(int player, int card, event_t event)
{
  /* Laccolith Rig	|R
   * Enchantment - Aura
   * Enchant creature
   * Whenever enchanted creature becomes blocked, you may have it deal damage equal to its power to target creature. If you do, the first creature assigns no combat damage this turn. */

  int choice;
  card_instance_t *instance;
  int attached_card;
  int power_or_result;
  int target_player;
  target_definition_t td;

  instance = in_play(player, card);

  if (((instance != NULL) &&
       (instance = get_card_instance(player, card), -1 < instance->damage_target_player)) &&
      (instance = get_card_instance(player, card), event == EVENT_DECLARE_BLOCKERS))
  {
    target_player = (int)instance->damage_target_player;
    attached_card = instance->damage_target_card;
    power_or_result = is_attacking(target_player, attached_card);
    if (((power_or_result != 0) &&
         (power_or_result = is_unblocked(target_player, attached_card), power_or_result == 0)) &&
        (power_or_result = is_humiliated(player, card, AL_TRIG), power_or_result == 0))
    {

      choice = do_dialog(player, player, card, -1, -1, " Damage creature\n Pass", 0);
      if (choice == 0)
      {

        instance = get_card_instance(target_player, attached_card);
        instance->number_of_targets = '\0';
        default_target_definition(target_player, attached_card, &td, 2);
        td.who_chooses = player;
        power_or_result = can_target(&td);
        if ((power_or_result != 0) &&
            (power_or_result = pick_target(&td, "TARGET_CREATURE"), power_or_result != 0))
        {
          negate_combat_damage_this_turn(player, card, target_player, attached_card, 0);
          power_or_result = get_power(target_player, attached_card);
          damage_creature(instance->targets[0].player, instance->targets[0].card, power_or_result,
                          target_player, attached_card);
        }
        instance->number_of_targets = '\0';
      }
      else if (choice == 1)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
  }
  return aura_pref_controller(player, card, event, player, NULL);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x209a3e0; original cards/nemesis.c:3218. */
int card_lashknife(int player, int card, event_t event)
{
  /* Lashknife	|1|W
   * Enchantment - Aura
   * If you control a Plains, you may tap an untapped creature you control rather than pay this spell's mana cost.
   * Enchant creature
   * Enchanted creature has first strike. */

  card_instance_t *instance;
  int result;
  card_instance_t *spell_instance;
  target_definition_t td2;

  if (event == EVENT_MODIFY_COST)
  {

    instance = get_card_instance(player, card);
    base_target_definition(player, card, &td2, 2);
    td2.allowed_controller = player;
    td2.preferred_controller = player;
    td2.illegal_state = TARGET_STATE_TAPPED;
    result = check_battlefield_for_subtype(player, 1, 0x5007);
    if ((result == 0) || (result = can_target(&td2), result == 0))
    {
      instance->info_slot = 0;
    }
    else
    {
      instance->info_slot = 1;
      null_casting_cost(player, card);
    }
    get_card_instance(player, card);
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      return aura_pref_controller(player, card, EVENT_CAN_CAST, player, NULL);
    }
    instance = get_card_instance(player, card);
    if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
        (player == affected_card_controller))
    {
      instance->number_of_targets = '\0';
      result = played_for_free(player, card);
      if (((result == 0) && (result = is_token(player, card), result == 0)) &&
          (instance->info_slot == 1))
      {

        spell_instance = get_card_instance(player, card);
        result = has_mana_to_cast_iid(player, spell_instance->internal_card_id, 0x6c);
        if ((result == 0) ||
            (result = do_dialog(player, player, card, -1, -1,
                                " Tap a creature you control\n Play normally\n Cancel", 0),
             result == 0))
        {
          base_target_definition(player, card, &td2, 2);
          td2.allowed_controller = player;
          td2.preferred_controller = player;
          td2.illegal_state = TARGET_STATE_TAPPED;
          result = new_pick_target(&td2, "Select a creature you control to tap.", 0, 0x81);
          if (result == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          action_on_target(player, card, 0, ACT_TAP);
          instance->number_of_targets = '\0';
        }
        else
        {
          if (result != 1)
          {
            spell_fizzled = 1;
            return 0;
          }
          result = get_id(player, card);
          charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
        }
      }
    }
  }
  attached_gets_kw(player, card, event, KEYWORD_FIRST_STRIKE, SP_KEYWORD_0);
  return aura_pref_controller(player, card, event, player, NULL);
}
#endif

/* ManalinkEh 2022: 0x209a6c0; original cards/nemesis.c:3285. */
int card_lawbringer(int player, int card, event_t event)
{
  /* Lawbringer	|2|W
   * Creature - Kor Rebel 2/2
   * |T, Sacrifice this creature: Exile target red creature. */

  card_instance_t *instance;
  const char *prompt;
  int result;
  target_definition_t td;

  if (!IS_GAA_EVENT(event))
    return 0;

  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 2);
  td.required_color = get_sleighted_color_test(player, card, 0x10);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
  }
  prompt = get_sleighted_color_text(player, card, "Select target %s creature card.", 4);
  result = generic_activated_ability(player, card, event, 0xf, 0, 0, 0, 0, 0, 0, 0, &td, prompt);
  return result;
}

/* ManalinkEh 2022: 0x209a840; original cards/nemesis.c:3311. */
int card_lightbringer(int player, int card, event_t event)
{
  /* Lightbringer	|2|W
   * Creature - Kor Rebel 2/2
   * |T, Sacrifice this creature: Exile target black creature. */

  card_instance_t *instance;
  const char *prompt;
  int result;
  target_definition_t td;

  if (!IS_GAA_EVENT(event))
    return 0;

  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 2);
  td.required_color = get_sleighted_color_test(player, card, 2);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
  }
  prompt = get_sleighted_color_text(player, card, "Select target %s creature card.", 1);
  result = generic_activated_ability(player, card, event, 0xf, 0, 0, 0, 0, 0, 0, 0, &td, prompt);
  return result;
}

/* ManalinkEh 2022: 0x20906d0; original cards/nemesis.c:3337. */
static int mana_cache_legacy(int player, int card, event_t event)
{
  int p;
  card_instance_t *instance;
  int charge_count;
  int source_player_or_id;
  int card_00;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_player_or_id = get_id(p, instance->targets[0].card);
        if (source_player_or_id != 0x126a)
        {
          kill_card(player, card, KILL_EXILE);
        }
        instance = in_play(instance->targets[0].player, instance->targets[0].card);
        if (instance == NULL)
        {
          kill_card(player, card, KILL_EXILE);
        }
      }
    }
    instance = get_card_instance(player, card);
    if (instance->targets[0].player == -1)
    {
      return 0;
    }
    instance = get_card_instance(player, card);
    source_player_or_id = instance->targets[0].player;
    card_00 = instance->targets[0].card;
  }
  else
  {
    instance = get_card_instance(player, card);
    if (instance->targets[0].player == -1)
    {
      return 0;
    }

    instance = get_card_instance(player, card);
    source_player_or_id = instance->targets[0].player;
    card_00 = instance->targets[0].card;
    if (event == EVENT_COUNT_MANA)
    {
      if (card_00 != affected_card)
      {
        return 0;
      }
      if (source_player_or_id != affected_card_controller)
      {
        return 0;
      }
      if (count_counters(source_player_or_id, card_00, COUNTER_CHARGE) == 0)
      {
        return 0;
      }
      if (can_produce_mana(source_player_or_id, card_00) == 0)
      {
        return 0;
      }
      declare_mana_available(player, 0,
                             count_counters(source_player_or_id, card_00, COUNTER_CHARGE));
      return 0;
    }
    if (event == EVENT_CAN_ACTIVATE)
    {
      charge_count = count_counters(source_player_or_id, card_00, COUNTER_CHARGE);
      if (charge_count < 1)
      {
        return 0;
      }
      if (current_turn != player)
      {
        return 0;
      }
      return can_produce_mana(source_player_or_id, card_00);
    }
  }
  if (event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
  {
    return 0;
  }
  if (event == EVENT_ACTIVATE)
  {
    remove_counters(source_player_or_id, card_00, source_player_or_id, card_00, COUNTER_CHARGE, 1);
    produce_mana(player, 0, 1);
    tapped_for_mana_color = -2;
  }
  return 0;
}

/* ManalinkEh 2022: 0x209a9c0; original cards/nemesis.c:3377. */
int card_mana_cache(int player, int card, event_t event)
{
  /* Mana Cache	|1|R|R
   * Enchantment
   * At the beginning of each player's end step, put a charge counter on this enchantment for each untapped land that player controls.
   * Remove a charge counter from this enchantment: Add |C. Any player may activate this ability but only during their turn before the end step. */

  int p;
  int charge_count_or_result;
  card_instance_t *instance;
  int temporary_card_or_result;
  int legacy_card;
  int c;

  p = current_turn;
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      int found[2];
      found[0] = -1;
      found[1] = -1;
      c = active_cards_count[current_turn] + -1;
      if (-1 < c)
      {
        do
        {
          instance = in_play(p, c);

          if ((((instance != NULL) &&
                (temporary_card_or_result = is_what(p, c, 0x80), temporary_card_or_result != 0)) &&
               (instance = get_card_instance(p, c), instance->info_slot_fn == mana_cache_legacy)) &&
              ((instance->targets[0].player == player && (instance->targets[0].card == card))))
          {
            found[p] = c;
          }
          c = c + -1;
        } while (c != -1);
      }
      p = 1 - current_turn;
      c = active_cards_count[p] + -1;
      if (-1 < c)
      {
        do
        {
          instance = in_play(p, c);

          if (((instance != NULL) &&
               (temporary_card_or_result = is_what(p, c, 0x80), temporary_card_or_result != 0)) &&
              ((instance = get_card_instance(p, c), instance->info_slot_fn == mana_cache_legacy &&
                                                        ((instance->targets[0].player == player &&
                                                          (instance->targets[0].card == card))))))
          {
            found[p] = c;
          }
          c = c + -1;
        } while (c != -1);
      }
      if (-1 < found[player])
      {
        kill_card(player, found[player], KILL_EXILE);
      }
      p = 1 - player;
      if (found[p] == -1)
      {

        instance = get_card_instance(player, card);
        temporary_card_or_result = add_card_to_hand(p, instance->internal_card_id);
        legacy_card = create_legacy_activate(p, temporary_card_or_result, mana_cache_legacy);
        instance = get_card_instance(p, legacy_card);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        obliterate_card(p, temporary_card_or_result);
      }
    }
  }
  charge_count_or_result = eot_trigger(player, card, event);
  if (charge_count_or_result != 0)
  {
    test_definition_t this_test;
    new_default_test_definition(&this_test, 1, "");
    this_test.state = STATE_TAPPED;
    this_test.state_flag = DOESNT_MATCH;
    charge_count_or_result =
        check_battlefield_for_special_card(player, card, current_turn, 4, &this_test);
    add_counters(player, card, player, card, COUNTER_CHARGE, charge_count_or_result);
  }
  if (event == EVENT_COUNT_MANA)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (charge_count_or_result = count_counters(player, card, COUNTER_CHARGE),
          charge_count_or_result != 0)) &&
        (charge_count_or_result = can_produce_mana(player, card), charge_count_or_result != 0))
    {
      declare_mana_available(player, 0, count_counters(player, card, COUNTER_CHARGE));
    }
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    charge_count_or_result = count_counters(player, card, COUNTER_CHARGE);
    if ((0 < charge_count_or_result) && (current_turn == player))
    {
      return can_produce_mana(player, card);
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    remove_counters(player, card, player, card, COUNTER_CHARGE, 1);
    produce_mana(player, 0, 1);
    tapped_for_mana_color = -2;
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x209ae60; original cards/nemesis.c:3438. */
int card_mind_swords(int player, int card, event_t event)
{
  /* Mind Swords	|1|B
   * Sorcery
   * If you control a Swamp, you may sacrifice a creature rather than pay this spell's mana cost.
   * Each player exiles two cards from their hand. */

  card_instance_t *instance;
  int result;
  unsigned int player_00;
  unsigned int player_pass;
  test_definition_t test;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
    result = check_battlefield_for_subtype(player, 1, 0x5009);
    if ((result == 0) || (result = new_can_sacrifice_as_cost(player, card, &test), result == 0))
    {
      instance->info_slot = 0;
    }
    else
    {
      instance->info_slot = 1;
      null_casting_cost(player, card);
    }
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      return basic_spell(player, card, EVENT_CAN_CAST);
    }
    if (event == EVENT_CAST_SPELL)
    {
      if ((((card == affected_card) && (player == affected_card_controller)) &&
           (result = played_for_free(player, card), result == 0)) &&
          ((result = is_token(player, card), result == 0 && (instance->info_slot == 1))))
      {

        result = get_id(player, card);
        result = has_mana_to_cast_id(player, EVENT_CAST_SPELL, result);
        if ((result == 0) || (result = do_dialog(player, player, card, -1, -1,
                                                 " Sac a creature\n Play normally\n Cancel", 0),
                              result == 0))
        {
          new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
          result = new_sacrifice(player, card, player, 0, &test);
          if (result != 0)
          {
            return 0;
          }
        }
        else if (result == 1)
        {
          result = get_id(player, card);
          charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
          return 0;
        }
        spell_fizzled = 1;
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {

      player_pass = 0;
      do
      {

        player_00 = current_turn ^ player_pass;
        player_pass = player_pass + 1;
        new_default_test_definition(&test, 0xff, "Select a card to exile.");
        new_global_tutor(player_00, player_00, 4, 6, 1, 3, &test);
        new_global_tutor(player_00, player_00, 4, 6, 1, 3, &test);
      } while (player_pass != 2);
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x209b190; original cards/nemesis.c:3502. */
int card_mogg_alarm(int player, int card, event_t event)
{
  /* Mogg Alarm	|1|R|R
   * Sorcery
   * You may sacrifice two Mountains rather than pay this spell's mana cost.
   * Create two 1/1 red Goblin creature tokens. */

  card_instance_t *instance;
  int result;
  unsigned int first_sacrifice;
  unsigned int second_sacrifice;
  unsigned int player_00;
  unsigned int player_01;
  test_definition_t test;

  if (event == EVENT_MODIFY_COST)
  {

    new_default_test_definition(&test, 0x47, "Select a Mountain to sacrifice.");
    test.subtype = 0x5006;
    test.qty = 2;
    instance = get_card_instance(player, card);
    result = new_can_sacrifice_as_cost(player, card, &test);
    if (result == 0)
    {
      instance->info_slot = 0;
    }
    else
    {
      instance->info_slot = 1;
      null_casting_cost(player, card);
    }
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      return basic_spell(player, card, EVENT_CAN_CAST);
    }
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {

        instance = get_card_instance(player, card);
        result = played_for_free(player, card);
        if ((result == 0) &&
            ((result = is_token(player, card), result == 0 && (instance->info_slot == 1))))
        {

          instance = get_card_instance(player, card);
          result = has_mana_to_cast_iid(player, instance->internal_card_id, 0x6c);
          if ((result == 0) ||
              (result = do_dialog(player, player, card, -1, -1,
                                  " Sacrifice 2 Mountains\n Play normally\n Cancel", 0),
               result == 0))
          {

            new_default_test_definition(&test, 0x47, "Select a Mountain to sacrifice.");
            test.subtype = 0x5006;
            first_sacrifice = new_sacrifice(player, card, player,
                                            SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
            if (first_sacrifice != 0)
            {
              second_sacrifice = new_sacrifice(
                  player, card, player, SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
              if (second_sacrifice != 0)
              {

                player_01 = (int)first_sacrifice >> 0x10 & 0xff;
                state_untargettable(player_01, first_sacrifice >> 0x18, 0);
                player_00 = (int)second_sacrifice >> 0x10 & 0xff;
                kill_card(player_01, first_sacrifice >> 0x18, KILL_SACRIFICE);
                state_untargettable(player_00, second_sacrifice >> 0x18, 0);
                kill_card(player_00, second_sacrifice >> 0x18, KILL_SACRIFICE);
                return 0;
              }
              state_untargettable((int)first_sacrifice >> 0x10 & 0xff, first_sacrifice >> 0x18, 0);
              spell_fizzled = 1;
              return 0;
            }
          }
          else if (result == 1)
          {
            result = get_id(player, card);
            charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
            return 0;
          }
          spell_fizzled = 1;
        }
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      generate_tokens_by_id(player, card, CARD_ID_GOBLIN, 2);
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x209b540; original cards/nemesis.c:3585. */
int card_mossdog(int player, int card, event_t event)
{
  /* Mossdog	|G
   * Creature - Plant Dog 1/1
   * Whenever this creature becomes the target of a spell or ability an opponent controls, put a +1/+1 counter on this creature. */

  card_instance_t *instance;
  const target_t *targeting_source;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      targeting_source =
          becomes_target_of_spell_or_effect(player, card, event, player, card, 1 - player);
      if (targeting_source != NULL)
      {
        add_counters(player, card, player, card, COUNTER_P1_P1, 1);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x209b5d0; original cards/nemesis.c:3600. */
int card_noble_stand(int player, int card, event_t event)
{
  /* Noble Stand	|4|W
   * Enchantment
   * Whenever a creature you control blocks, you gain 2 life. */

  int blocker_or_result;
  card_instance_t *instance;
  int blocking_creature;

  if (((event == EVENT_DECLARE_BLOCKERS) &&
       (blocker_or_result = is_humiliated(player, card, AL_TRIG), blocker_or_result == 0)) &&
      (blocker_or_result = active_cards_count[1 - current_turn] + -1, -1 < blocker_or_result))
  {
    do
    {
      instance = in_play(1 - current_turn, blocker_or_result);
      if (((instance != NULL) &&
           (blocking_creature = is_what(1 - current_turn, blocker_or_result, 2),
            blocking_creature != 0)) &&
          ((blocking_creature =
                blocking(1 - current_turn, blocker_or_result, EVENT_DECLARE_BLOCKERS),
            blocking_creature != 0 && (1 - current_turn == player))))
      {
        gain_life(player, 2);
      }
      blocker_or_result = blocker_or_result + -1;
    } while (blocker_or_result != -1);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x209b6d0; original cards/nemesis.c:3617. */
int card_off_balance(int player, int card, event_t event)
{
  /* Off Balance	|W
   * Instant
   * Target creature can't attack or block this turn. */

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  target_definition_t td;
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_RESOLVE_SPELL)
  {
    card_instance_t *inst = get_card_instance(player, card);
    if (valid_target(&td))
    {
      pump_ability_t pump;
      default_pump_ability_definition(player, card, &pump, 0, 0, 0, 0x20);
      pump.paue_flags = 0x210;
      pump_ability(player, card, inst->targets[0].player, inst->targets[0].card,
                   &pump);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x209b880; original cards/nemesis.c:3643. */
int card_parallax_dementia(int player, int card, event_t event)
{
  /* Parallax Dementia	|1|B
   * Enchantment - Aura
   * Enchant creature
   * Fading 1 (This enchantment enters with one fade counter on it. At the beginning of your upkeep, remove a fade counter from it. If you can't, sacrifice it.)
   * Enchanted creature gets +3/+2.
   * When this Aura leaves the battlefield, destroy enchanted creature. That creature can't be regenerated. */

  int result;
  card_instance_t *instance;

  leaves_play(player, card, event, 1, 1);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {

    instance = get_card_instance(player, card);
    kill_card((int)instance->damage_target_player, instance->damage_target_card, KILL_BURY);
  }
  fading(player, card, event, 1);
  attached_gets_pt(player, card, event, 3, 2);
  result = aura_pref_controller(player, card, event, player, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x209b950; original cards/nemesis.c:3663. */
int card_phyrexian_driver(int player, int card, event_t event)
{
  /* Phyrexian Driver	|2|B
   * Creature - Phyrexian Zombie Mercenary 1/1
   * When this creature enters, other Mercenary creatures get +1/+1 until end of turn. */

  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  new_default_test_definition(&test, 2, "");
  test.subtype = 0x73;
  test.not_me = 1;
  pump_creatures_until_eot(player, card, player, 0, 1, 1, 0, 0, &test);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x209ba10; original cards/nemesis.c:3682. */
int card_phyrexian_prowler(int player, int card, event_t event)
{
  /* Phyrexian Prowler	|3|B
   * Creature - Phyrexian Zombie Mercenary 3/3
   * Fading 3 (This creature enters with three fade counters on it. At the beginning of your upkeep, remove a fade counter from it. If you can't, sacrifice it.)
   * Remove a fade counter from this creature: This creature gets +1/+1 until end of turn. */

  card_instance_t *instance;
  int result;
  fading(player, card, event, 3);
  if (event == EVENT_CHECK_ABILITIES)
  {
    result = can_use_activated_abilities(player, card);
    if ((((result != 0) && (result = count_counters(player, card, COUNTER_FADE), 0 < result)) &&
         (card == affected_card)) &&
        (player == affected_card_controller))
    {
      result = count_counters(player, card, COUNTER_FADE);
      check_abilities_power = check_abilities_power + result;
      result = count_counters(player, card, COUNTER_FADE);
      check_abilities_toughness = check_abilities_toughness + result;
    }
    return 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                  instance->parent_controller, instance->parent_card, 1, 1);
  }
  return generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 0, 0x1290100, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x209bba0; original cards/nemesis.c:3711. */
int card_rejuvenation_chamber(int player, int card, event_t event)
{
  /* Rejuvenation Chamber	|3
   * Artifact
   * Fading 2 (This artifact enters with two fade counters on it. At the beginning of your upkeep, remove a fade counter from it. If you can't, sacrifice it.)
   * |T: You gain 2 life. */

  int result;
  fading(player, card, event, 2);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    gain_life(player, 2);
  }
  result = generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x209bc50; original cards/nemesis.c:3728. */
int card_rootwater_thief(int player, int card, event_t event)
{
  /* Rootwater Thief	|1|U
   * Creature - Merfolk Rogue 1/2
   * |U: This creature gains flying until end of turn.
   * Whenever this creature deals combat damage to a player, you may pay |2. If you do, search that player's library for a card and exile it, then the player shuffles. */

  int result;
  int pos;
  test_definition_t this_test;

  result = damage_dealt_by_me(player, card, event,
                              DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE |
                                  DDBM_TRIGGER_OPTIONAL);
  if (result != 0)
  {
    result = has_mana(player, 0, 2);
    if (result != 0)
    {
      result = charge_mana_while_resolving(player, card, event, player, COLOR_COLORLESS, 2);

      if ((result != 0) && (result = 1 - player, *deck_ptr[result] != -1))
      {

        new_default_test_definition(&this_test, 0xff, "Select a card to exile.");
        pos = new_select_a_card(player, result, 1, 0, 1, -1, &this_test);
        if (pos != -1)
        {
          rfg_card_in_deck(result, pos);
        }
      }
    }
  }
  result = card_gust_skimmer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x209bd80; original cards/nemesis.c:3753. */
int card_rusting_golem(int player, int card, event_t event)
{
  /* Rusting Golem	|4
   * Artifact Creature - Golem * / *
   * Fading 5 (This creature enters with five fade counters on it. At the beginning of your upkeep, remove a fade counter from it. If you can't, sacrifice it.)
   * ~'s power and toughness are each equal to the number of fade counters on it. */

  int newval;

  fading(player, card, event, 5);
  if (((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    if (card == -1)
    {
      set_characteristic_pt(0);
    }
    else
    {
      newval = count_counters(player, card, COUNTER_FADE);
      set_characteristic_pt(newval);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x209be10; original cards/nemesis.c:3772. */
int card_silkenfist_fighter(int player, int card, event_t event)
{
  /* Silkenfist Fighter	|1|W
   * Creature - Kor Soldier 1/3
   * Whenever this creature becomes blocked, untap it. */

  int result;

  if (event == EVENT_DECLARE_BLOCKERS)
  {
    result = is_attacking(player, card);
    if (result != 0)
    {
      result = is_unblocked(player, card);
      if (result == 0)
      {
        result = is_humiliated(player, card, AL_TRIG);
        if (result == 0)
        {
          untap_card(player, card);
        }
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x209be80; original cards/nemesis.c:3789. */
int card_skyshroud_cutter(int player, int card, event_t event)
{
  /* Skyshroud Cutter	|3|G
   * Creature - Beast 2/2
   * If you control a Forest, rather than pay this spell's mana cost, you may have each other player gain 5 life. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_MODIFY_COST)
  {
    result = check_battlefield_for_subtype(player, 1, 0x5001);
    if ((result != 0) && (result = player_can_gain_life_as_cost(1 - player), result != 0))
    {
      null_casting_cost(player, card);
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      return 0;
    }
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
    return 0;
  }
  if (event == EVENT_CAN_CAST)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    return result;
  }
  if (((((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
        (player == affected_card_controller)) &&
       ((result = played_for_free(player, card),
         result == 0 && (result = is_token(player, card), result == 0)))) &&
      (instance = get_card_instance(player, card), instance->info_slot == 1))
  {

    result = get_id(player, card);
    result = has_mana_to_cast_id(player, EVENT_CAST_SPELL, result);
    if ((result != 0) &&
        (result = do_dialog(player, player, card, -1, -1,
                            " Make opponent gain 5 life\n Play normally\n Cancel", 0),
         result != 0))
    {
      if (result != 1)
      {
        spell_fizzled = 1;
        return 0;
      }
      result = get_id(player, card);
      charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
      return 0;
    }
    gain_life(1 - player, 5);
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x209c040; original cards/nemesis.c:3831. */
int card_sliptide_serpent(int player, int card, event_t event)
{
  /* Sliptide Serpent	|4|U|U
   * Creature - Serpent 4/4
   * |3|U: Return this creature to its owner's hand. */

  int result;
  card_instance_t *instance;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    bounce_permanent(instance->parent_controller, instance->parent_card);
  }
  result = generic_activated_ability(player, card, event, 0, 3, 0, 1, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x209c0f0; original cards/nemesis.c:3845. */
int card_spiteful_bully(int player, int card, event_t event)
{
  /* Spiteful Bully	|1|B
   * Creature - Phyrexian Zombie Mercenary 3/3
   * At the beginning of your upkeep, this creature deals 3 damage to target creature you control. */

  int result;
  target_definition_t td;
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    td.preferred_controller = player;
    td.allowed_controller = player;
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_CREATURE_YOU_CONTROL");
      if (result != 0)
      {
        damage_target0(player, card, 3);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x209c190; original cards/nemesis.c:3872. */
int card_terrain_generator(int player, int card, event_t event)
{
  /* Terrain Generator	""
   * Land
   * |T: Add |C.
   * |2, |T: You may put a basic land card from your hand onto the battlefield tapped. */

  int result;
  card_instance_t *instance;
  test_definition_t test;

  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      new_default_test_definition(&test, 1, "Select a basic land card.");
      test.subtype = 0x2000;
      test.zone = 0x100;
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
      result = paying_mana();
      if (((result == 0) && (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 3,
                                                                2, 0, 0, 0, 0, 0, 0, NULL, NULL),
                             result != 0)) &&
          (result = do_dialog(player, player, card, -1, -1,
                              " Generate mana\n Basic land to battlefield\n Cancel", 1),
           result != 0))
      {
        if (result == 1)
        {
          add_state(player, card, 0x10);
          result = generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 2, 0, 0, 0, 0, 0, 0,
                                             NULL, NULL);
          if (result == 0)
          {
            remove_state(player, card, 0x10);
          }
          else
          {
            result = check_battlefield_for_special_card(player, card, player, 0, &test);
            if (result != 0)
            {
              instance->info_slot = 1;
            }
          }
          tapped_for_mana_color = -1;
        }
        else if (result == 2)
        {
          spell_fizzled = 1;
          return 0;
        }
        return 0;
      }
      event = EVENT_ACTIVATE;
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      new_default_test_definition(&test, 1, "Select a basic land card.");
      test.subtype = 0x2000;
      test.zone = 0x100;
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (0 < instance->info_slot)
      {
        result = check_battlefield_for_special_card(player, card, player, 0, &test);
        if (result != 0)
        {
          new_global_tutor(player, player, 4, 5, 0, 5, &test);
          return 0;
        }
        return 0;
      }
      return 0;
    }
  }

  result = mana_producer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2090920; original cards/nemesis.c:3929. */
static char *greatest_power_among_creatures(int who_chooses, int player, int card)
{
  int target_power;
  int highest_power;
  char *requirement_error;
  target_power = get_power(player, card);
  highest_power = get_highest_power(2);
  requirement_error = NULL;
  if (target_power != highest_power)
  {
    requirement_error = "Target has not the greatest power among creatures on the battlefield.";
  }
  return requirement_error;
}

/* ManalinkEh 2022: 0x209c4e0; original cards/nemesis.c:3936. */
int card_topple(int player, int card, event_t event)
{
  /* Topple	|2|W
   * Sorcery
   * Exile target creature with the greatest power among creatures on the battlefield. (If two or more creatures are tied for greatest power, target any one of them.) */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (int32_t)greatest_power_among_creatures;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td,
                         "Select target creature with the greatest power.", 1, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x209c650; original cards/nemesis.c:3962. */
int card_treetop_bracers(int player, int card, event_t event)
{
  /* Treetop Bracers	|1|G
   * Enchantment - Aura
   * Enchant creature (Target a creature as you cast this. This card enters attached to that creature.)
   * Enchanted creature gets +1/+1 and can't be blocked except by creatures with flying. */

  card_instance_t *instance;
  unsigned int blocker_keywords;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {
      instance = get_card_instance(player, card);
      if (-1 < instance->damage_target_player)
      {

        instance = get_card_instance(player, card);
        if (((event == EVENT_BLOCK_LEGALITY) &&
             (instance->damage_target_player == attacking_card_controller)) &&
            (instance->damage_target_card == attacking_card))
        {

          blocker_keywords =
              get_abilities(affected_card_controller, affected_card, EVENT_ABILITIES, -1);
          if ((blocker_keywords & 0x20) == 0)
          {
            event_result = 1;
          }
        }
      }
    }
  }
  attached_gets_pt(player, card, event, 1, 1);
  return aura_pref_controller(player, card, event, player, NULL);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x209c750; original cards/nemesis.c:3984. */
int card_wandering_eye(int player, int card, event_t event)
{
  /* Wandering Eye	|2|U
   * Creature - Illusion 1/3
   * Flying
   * Players play with their hands revealed. */

  if (event == EVENT_STATIC_EFFECTS)
  {
    player_bits[0] = player_bits[0] | PB_HAND_REVEALED;
    player_bits[1] = player_bits[1] | PB_HAND_REVEALED;
  }
  return 0;
}
#endif
