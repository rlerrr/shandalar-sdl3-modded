#include "manalink.h"

// Functions
int landcycling(int player, int card, event_t event, int colorless, int land_subtype1, int land_subtype2){

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_X(2));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "");
		this_test.subtype = land_subtype1;
		if (land_subtype2 <= 0){
			strcpy(this_test.message, get_hacked_land_text(-1, -1, "Select %a card.", land_subtype1));
		}
		else{
			strcpy(this_test.message, get_hacked_land_text(-1, -1, "Select %a or %s card.", land_subtype1, land_subtype2));
			this_test.sub2 = land_subtype2;
			this_test.subtype_flag = F2_MULTISUBTYPE;
		}
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}
	return 0;
}


// Cards
int card_ageless_sentinels(int player, int card, event_t event)
{
  // Also code for Elder Land Wurm (previously 0x4c8dd0).
  // Changed creature type handled in has_subtype().

  /* Ruling for Elder Land Wurm: 10/4/2004: Another effect can give Elder Land Wurm defender after it has lost it. Blocking again will cause it to lose that
   * instance of defender too. */
  if (current_turn == 1-player && blocking(player, card, event) && !is_humiliated(player, card) && check_for_ability(player, card, KEYWORD_DEFENDER))
	{
	  // Probably should look for other instances of the this-loses-defender effect and get rid of them first to reduce clutter.
	  int leg = pump_ability_until_eot(player, card, player, card, 0, 0, KEYWORD_DEFENDER, SP_KEYWORD_DOES_NOT_END_AT_EOT);
	  if (leg != -1)
		get_card_instance(player, leg)->targets[4].player = 0;	// remove keyword

	  get_card_instance(player, card)->targets[5].player = 66;	// subtype hack, for Ageless Sentinels only
	  reset_subtypes(player, card, 3);
	}

  return 0;
}

int card_alpha_status(int player, int card, event_t event){

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			event_result += (2 * count_all_other_creatures_that_shares_a_creature_type_with_me(p, c, ANYBODY));
		}
	}

	return vanilla_aura(player, card, event, player);
}

int card_ambush_commander(int player, int card, event_t event){
/*
Ambush Commander |3|G|G
Creature — Elf 2/2
Forests you control are 1/1 green Elf creatures that are still lands.
{1}{G}, Sacrifice an Elf: Target creature gets +3/+3 until end of turn.
*/

	if( ! IS_GAA_EVENT(event) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);

		global_type_change(player, card, event, player, TYPE_CREATURE, &this_test, 1, 1, 0, SUBTYPE_ELF, COLOR_TEST_GREEN);

		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select an Elf to sacrifice.");
	test.subtype = SUBTYPE_ELF;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 3, 3);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST_XG(1, 1), 0, &td, "TARGET_CREATURE",
															0, &test, 0);
}

int card_ancient_ooze(int player, int card, event_t event){

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && player != -1 && ! is_humiliated(player, card) ){
		int result = 0;
		int count = 0;
		while( count < active_cards_count[player] ){
				if( count != card && in_play(player, count) && is_what(player, count, TYPE_CREATURE) ){
					result+=get_cmc(player, count);
				}
				count++;
		}
		event_result+=result;
	}
	return 0;
}

// aphetto runecaster --> vanilla

int card_ark_of_blight(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_X(3), 0,
									&td, "TARGET_LAND");
}

int card_astral_steel(int player, int card, event_t event){

	/* Astral Steel	|2|W
	 * Instant
	 * Target creature gets +1/+2 until end of turn.
	 * Storm */

	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 && ! check_special_flags(player, card, SF_NOT_CAST) ){
			int i;
			for(i=0;i<get_storm_count();i++){
				if( new_pick_target(&td, "TARGET_CREATURE", 1, 0) ){
					pump_until_eot(player, card, instance->targets[1].player, instance->targets[1].card, 1, 2);
				}
			}
			instance->number_of_targets = 1;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_bladewing_the_risen(int player, int card, event_t event){
	/* Bladewing the Risen	|3|B|B|R|R
	 * Legendary Creature - Zombie Dragon 4/4
	 * Flying
	 * When ~ enters the battlefield, you may return target Dragon permanent card from your graveyard to the battlefield.
	 * |B|R: Dragon creatures get +1/+1 until end of turn. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "Select target Dragon permanent card.");
		this_test.subtype = SUBTYPE_DRAGON;

		if( new_special_count_grave(player, &this_test) && ! graveyard_has_shroud(player) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card);
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, ANYBODY, SUBTYPE_DRAGON, 1, 1, 0, 0);
		return 0;
	}

	return generic_shade(player, card, event, 0, MANACOST_BR(1,1), 1, 1, 0, 0);
}

int card_bladewings_thrall(int player, int card, event_t event){

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card) ){
		if( check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_DRAGON) ){
			event_result |= KEYWORD_FLYING;
		}
	}
	return 0;
}

int card_brain_freeze(int player, int card, event_t event){
	/* Brain Freeze	|1|U
	 * Instant
	 * Target player puts the top three cards of his or her library into his or her graveyard.
	 * Storm */

	if( ! is_unlocked(player, card, event, 11) ){ return 0; }

	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t* instance = get_card_instance(player, card);

	if(event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 && ! check_special_flags(player, card, SF_NOT_CAST) ){
			int i;
			for(i=0;i<get_storm_count();i++){
				if( new_pick_target(&td, "TARGET_PLAYER", 1, 0) ){
					mill(instance->targets[1].player, 3);
				}
			}
			instance->number_of_targets = 1;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if (valid_target(&td)){
			mill(instance->targets[0].player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_break_asunder(int player, int card, event_t event){

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return cycling(player, card, event, MANACOST_X(2));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
}

int card_cabal_conditioning(int player, int card, event_t event)
{
  /* Cabal Conditioning	|6|B
   * Sorcery
   * Any number of target players each discard a number of cards equal to the highest converted mana cost among permanents you control. */

	if( event == EVENT_CAN_CAST )
		return basic_spell(player, card, event);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.allow_cancel = 3;	// Both Done and Cancel buttons

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
		instance->number_of_targets = 0;
		while (can_target(&td))
		{
			if (!new_pick_target(&td, "TARGET_PLAYER", -1, 0))
			{
				if (instance->targets[instance->number_of_targets].card == -1)	// picked cancel, not done
					cancel = 1;
				return 0;
			}

			if (td.allowed_controller == ANYBODY)	// was first choice
			{
				if (IS_AI(player) && instance->targets[0].player == 1-player)
					break;	// AI never targets self
				else
					td.allowed_controller = td.preferred_controller = 1 - instance->targets[0].player;
			}
			else
				break;	// was second choice
		}
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
		int highest_cmc = get_highest_cmc(player, TYPE_PERMANENT);
		int i;
		for (i = 0; i < instance->number_of_targets; ++i)
			if (validate_target(player, card, &td, i))
				new_multidiscard(instance->targets[i].player, highest_cmc, 0, player);

		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_cabal_interrogator(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && instance->info_slot > 0 ){
			ec_definition_t ec;
			default_ec_definition(instance->targets[0].player, player, &ec);
			ec.cards_to_reveal = instance->info_slot;

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			new_effect_coercion(&ec, &this_test);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XB(-1, 1), 0, &td, "TARGET_PLAYER");
}

int card_call_to_the_grave(int player, int card, event_t event){

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a non-Zombie creature to sacrifice.");
		this_test.subtype = SUBTYPE_ZOMBIE;
		this_test.subtype_flag = 1;

		new_sacrifice(player, card, current_turn, SAC_CAUSED | SAC_NO_CANCEL, &this_test);
	}

	if(trigger_condition == TRIGGER_EOT &&  affect_me(player, card ) && reason_for_trigger_controller == player){
		if( count_subtype(ANYBODY, TYPE_CREATURE, -1) < 1 && eot_trigger(player, card, event) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return global_enchantment(player, card, event);
}

int card_carbonize(int player, int card, event_t event){
	/* Carbonize	|2|R	0x20094ae
	 * Instant
	 * ~ deals 3 damage to any target. If it's a creature, it can't be regenerated this turn, and if it would die this turn, exile it instead. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance( player, card );
			if( instance->targets[0].card != -1 && is_what(instance->targets[0].player, instance->targets[0].card, TYPE_CREATURE) ){
				cannot_regenerate_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
				exile_if_would_be_put_into_graveyard(player, card, instance->targets[0].player, instance->targets[0].card, 1);
			}
			damage_creature_or_player(player, card, event, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_carrion_feeder(int player, int card, event_t event)
{
  /* Carrion Feeder	|B
   * Creature - Zombie 1/1
   * ~ can't block.
   * Sacrifice a creature: Put a +1/+1 counter on ~. */

	cannot_block(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		add_1_1_counter(instance->parent_controller, instance->parent_card);
		return 0;
	}

	return generic_husk(player, card, event, TYPE_CREATURE, 1, 1, 0, 0);
}

int card_chartooth_cougar(int player, int card, event_t event)
{
  /* Chartooth Cougar	|5|R
   * Creature - Cat Beast 4/4
   * |R: ~ gets +1/+0 until end of turn.
   * |H2Mountaincycling |2 */

  if (IS_ACTIVATING_FROM_HAND(event))
	return landcycling(player, card, event, 2, SUBTYPE_MOUNTAIN, 0);

  return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1,0);
}

int card_consumptive_goo(int player, int card, event_t event){

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		if( can_target(&td) ){
			return generic_shade(player, card, event, 0, MANACOST_XB(2, 2), 1, 1, 0, 0);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card,
							instance->targets[0].player, instance->targets[0].card, -1, -1);
			add_1_1_counter(instance->parent_controller, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XB(2, 2), 0, &td, "TARGET_CREATURE");
}

int card_daru_spiritualist(int player, int card, event_t event)
{
	const target_t* targets = any_becomes_target(player, card, event, player, 0, TYPE_CREATURE, SUBTYPE_CLERIC, TYPE_EFFECT|TYPE_NONEFFECT,
												ANYBODY, RESOLVE_TRIGGER_MANDATORY);
	if (targets)
		for (; targets->player != -1; ++targets)
			pump_until_eot(player, card, targets->player, targets->card, 0, 2);

	return 0;
}

int card_daru_warchief(int player, int card, event_t event){

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card) ){
		if( has_subtype(affected_card_controller, affected_card, SUBTYPE_SOLDIER) ){
			COST_COLORLESS--;
		}
	}

	boost_creature_type(player, card, event, SUBTYPE_SOLDIER, 1, 2, 0, BCT_INCLUDE_SELF+BCT_CONTROLLER_ONLY);

	return 0;
}

int card_dawn_elemental(int player, int card, event_t event){

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == card && damage->damage_target_player == player &&
				damage->info_slot > 0 )
			{
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
	}

	return 0;
}

int card_day_of_the_dragons(int player, int card, event_t event){
/* Day of the Dragons	|4|U|U|U
 * Enchantment
 * When ~ enters the battlefield, exile all creatures you control. Then put that many 5/5 |Sred Dragon creature tokens with flying onto
 * 		the battlefield.
* When ~ leaves the battlefield, sacrifice all Dragons you control. Then return the exiled cards to the battlefield under your control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int count, actually_a_count = 0;
		for (count = active_cards_count[player]-1; count >= 0; --count){
				if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) ){
					if( ! is_token(player, count) ){
						exile_card_and_remember_it_on_exiledby(player, card, player, count);
					}
					else{
						kill_card(player, count, KILL_EXILE);
					}
					++actually_a_count;
				}
		}
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_DRAGON_SPIRIT, &token);
		token.pow = token.tou = 5;
		token.key_plus = KEYWORD_FLYING;
		token.color_forced = COLOR_TEST_RED;
		token.qty = actually_a_count;
		generate_token(&token);
	}

	if( leaves_play(player, card, event) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_DRAGON;
		new_manipulate_all(player, card, player, &this_test, KILL_BURY);

		int leg = 0;
		int idx = 0;
		int* loc;

		while ((loc = exiledby_find_any(player, card, &leg, &idx)) != NULL){
				int owner = (*loc & 0x80000000) ? 1 : 0;
				int iid = *loc & ~0x80000000;
				*loc = -1;
				int card_added = add_card_to_hand(player, iid);
				if( owner != player ){
					if( player == AI ){
						remove_state(player, card_added, STATE_OWNED_BY_OPPONENT);
					}
					else{
						add_state(player, card_added, STATE_OWNED_BY_OPPONENT);
					}
				}
				put_into_play(player, card_added);
				if( owner != player ){
					create_targetted_legacy_effect(player, card, &empty, player, card_added);
				}
		}
	}

	return global_enchantment(player, card, event);
}

int card_decree_of_annihilation(int player, int card, event_t event){
/*
Decree of Annihilation |8|R|R
Sorcery
Exile all artifacts, creatures, and lands from the battlefield, all cards from all graveyards, and all cards from all hands.
Cycling {5}{R}{R} ({5}{R}{R}, Discard this card: Draw a card.)
When you cycle Decree of Annihilation, destroy all lands.
*/
	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XR(5, 2));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND ){
		draw_cards(player, 1);
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		APNAP(p,{ new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int mod = 0;
		CYCLE_ALL_CARDS(p, c,	{
									if( in_play(p, c) ){
										if( (is_what(p, c, TYPE_ENCHANTMENT) && ! has_subtype_by_id(get_id(p, c), SUBTYPE_AURA)) ||
											is_planeswalker(p, c) )
										{
											mod += (p == player ? 12 : -12);
										}
									}
								}
		);
		ai_modifier += mod;
	}

	if( event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND | TYPE_CREATURE | TYPE_ARTIFACT);
		APNAP(p,{
					int c;
					for( c = active_cards_count[p]-1; c > -1; c-- ){
						if( in_play(p, c) && is_what(p, c, TYPE_LAND | TYPE_CREATURE | TYPE_ARTIFACT) ){
							kill_card(p, c, KILL_EXILE);
						}
						if( in_hand(p, c) ){
							rfg_card_in_hand(p, c);
						}
					}
					rfg_whole_graveyard(p);
				};
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_decree_of_justice(int player, int card, event_t event){
	/* Decree of Justice	|X|X|2|W|W
	 * Sorcery
	 * Put X 4/4 |Swhite Angel creature tokens with flying onto the battlefield.
	 * Cycling |2|W
	 * When you cycle ~, you may pay |X. If you do, put X 1/1 |Swhite Soldier creature tokens onto the battlefield. */


	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XW(2, 1));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		draw_a_card(player);
		int tokens = 0;
		charge_mana(player, COLOR_COLORLESS, -1);
		if( spell_fizzled != 1 ){
			tokens = x_value;
		}
		if( tokens > 0 ){
			generate_tokens_by_id(player, card, CARD_ID_SOLDIER, tokens);
		}
	}

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int amount = charge_mana_for_double_x(player, COLOR_COLORLESS);
		if( amount > 0 ){
			int diff = true_get_updated_casting_cost(player, card, -1, event, -1);
			if( diff < 0 ){
				amount -= diff;
			}
			get_card_instance(player, card)->info_slot = amount/2;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int tokens = get_card_instance(player, card)->info_slot;
		if( tokens > 0 ){
			generate_tokens_by_id(player, card, CARD_ID_ANGEL, tokens);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_decree_of_pain(int player, int card, event_t event){

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XB(3, 2));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND ){
		draw_cards(player, 1);
		pump_subtype_until_eot(player, card, ANYBODY, -1, -2, -2, 0, 0);
	}

	if( event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		int result = 0;
		APNAP(p,{ result += new_manipulate_all(player, card, p, &this_test, KILL_BURY); };);
		draw_cards(player, result);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_decree_of_savagery(int player, int card, event_t event){

	/* Decree of Savagery	|7|G|G
	 * Instant
	 * Put four +1/+1 counters on each creature you control.
	 * Cycling |4|G|G
	 * When you cycle ~, you may put four +1/+1 counters on target creature. */

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XG(4, 2));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND ){
		draw_cards(player, 1);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			add_1_1_counters(instance->targets[0].player, instance->targets[0].card, 4);
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		manipulate_type(player, card, player, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_P1_P1, 4));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_decree_of_silence(int player, int card, event_t event){
/* Decree of Silence	|6|U|U
 * Enchantment
 * Whenever an opponent casts a spell, counter that spell and put a depletion counter on ~.
 *		If there are three or more depletion counters on ~, sacrifice it.
 * Cycling |4|U|U
 * When you cycle ~, you may counter target spell. */

// 14/01/2017: Actually, the "counter-cycling" ability can't be coded.

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XU(4, 2));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND ){
		draw_cards(player, 1);
	}
	if( new_specific_spell_played(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		real_counter_a_spell(player, card, trigger_cause_controller, trigger_cause);
		add_counter(player, card, COUNTER_DEPLETION);
		if( count_counters(player, card, COUNTER_DEPLETION) >= 3 ){	// Part of the triggered ability, not separate
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return global_enchantment(player, card, event);
}

static int dimensional_breach_storing_legacy(int player, int card, event_t event){
// Just for comparison purposes
	return 0;
}

static int dimensional_breach_effect_legacy(int player, int card, event_t event){

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int stored[100];
		int sc = 0;

		int c;
		for(c = 0; c < active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)dimensional_breach_storing_legacy ){
					int any_iid_still_exiled_on_this_legacy = 0;
					int i;
					for(i=0; i<19; i++){
						int owner = inst->targets[i].player;
						int iid = inst->targets[i].card;
						if( owner > -1 ){
							if( check_rfg(owner, cards_data[iid].id) ){
								any_iid_still_exiled_on_this_legacy++;
								if( owner == current_turn ){
									stored[sc] = iid;
									sc++;
								}
							}
						}
						else{
							break;
						}
					}
					if( ! any_iid_still_exiled_on_this_legacy ){
						kill_card(player, c, KILL_EXILE);
					}
				}
			}
		}

		if( sc == 0 ){
			kill_card(player, card, KILL_EXILE);
			return 0;
		}

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a permanent to put into play.");
		int selected = select_card_from_zone(current_turn, current_turn, stored, sc, 1, AI_MAX_CMC, -1, &this_test);
		if( selected > -1 ){
			int card_added = add_card_to_hand(current_turn, stored[selected]);
			remove_card_from_rfg(current_turn, cards_data[stored[selected]].id);
			put_into_play(current_turn, card_added);
		}
	}

	return 0;
}

int card_dimensional_breach(int player, int card, event_t event){

	if( event == EVENT_RESOLVE_SPELL){
		int legacy = -1;
		int pos = 0;
		int found = 0;
		APNAP(p,{
					int c;
					for( c = active_cards_count[p]-1; c > -1; c--){
						if( in_play(p, c) && is_what(p, c, TYPE_PERMANENT) && ! is_token(p, c) ){
							int owner = get_owner(p, c);
							int iid = get_original_internal_card_id(p, c);
							if( legacy == -1 ){
								legacy = create_legacy_effect(player, card, &dimensional_breach_storing_legacy);
								add_status(player, legacy, STATUS_INVISIBLE_FX);
							}
							card_instance_t *instance = get_card_instance(player, legacy);
							instance->targets[pos].player = owner;
							instance->targets[pos].card = iid;
							pos++;
							if( pos == 19 ){
								legacy = -1;
								pos = 0;
							}
							found++;
							kill_card(p, c, KILL_EXILE);
						}
					}
				}
		);
		if( found ){
			create_legacy_effect(player, card, &dimensional_breach_effect_legacy);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_dispersal_shield(int player, int card, event_t event){
/* CARD_ID_DISPERSAL_SHIELD	6508
Dispersal Shield	|1|U
Instant
Counter target spell if its converted mana cost is less than or equal to the highest converted mana cost among permanents you control. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		if( counterspell_validate(player, card, NULL, 0) &&
			get_cmc(instance->targets[0].player, instance->targets[0].card) <= get_highest_cmc_nonland(player) )
		{
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return card_counterspell(player, card, event);
}

int card_dragon_breath(int player, int card, event_t event){
/* CARD_ID_DRAGON_BREATH	6510
Dragon Breath	|1|R
Enchantment - Aura
Enchant creature
Enchanted creature has haste.
|R: Enchanted creature gets +1/+0 until end of turn.
When a creature with converted mana cost 6 or greater enters the battlefield, you may return ~ from your graveyard to the battlefield attached to that creature. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player >= 0 ){
		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return generic_activated_ability(player, card, event, 0, MANACOST_R(1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card, instance->damage_target_player, instance->damage_target_card, 1, 0);
	}

	return generic_aura(player, card, event, player, 0, 0, 0, SP_KEYWORD_HASTE, 0, 0, 0);
}

int card_dragons_fangs(int player, int card, event_t event){
/* CARD_ID_DRAGON_FANGS	6511
Dragon Fangs	|1|G
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+1 and has trample.
When a creature with converted mana cost 6 or greater enters the battlefield, you may return ~ from your graveyard to the battlefield attached to that creature. */

	return generic_aura(player, card, event, player, 1, 1, KEYWORD_TRAMPLE, 0, 0, 0, 0);
}

int card_dragon_mage(int player, int card, event_t event){
/* CARD_ID_DRAGON_MAGE	6512
Dragon Mage	|5|R|R
Creature - Dragon Wizard 5/5
Flying
Whenever ~ deals combat damage to a player, each player discards his or her hand, then draws seven cards. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		while( amount ){
				APNAP(p,{
						discard_all(p);
						draw_cards(p, 7);
						};
				);
				amount--;
		}
	}

	return 0;
}

int card_dragons_scales(int player, int card, event_t event){
/* CARD_ID_DRAGON_SCALES	6513
Dragon Scales	|1|W
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+2 and has vigilance.
When a creature with converted mana cost 6 or greater enters the battlefield, you may return ~ from your graveyard to the battlefield attached to that creature. */

	return generic_aura(player, card, event, player, 1, 2, 0, SP_KEYWORD_VIGILANCE, 0, 0, 0);
}

int card_dragons_shadow(int player, int card, event_t event){
/* CARD_ID_DRAGON_SHADOW	6514
Dragon Shadow	|1|B
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+0 and has fear. (It can't be blocked except by artifact creatures and/or black creatures.)
When a creature with converted mana cost 6 or greater enters the battlefield, you may return ~ from your graveyard to the battlefield attached to that creature. */

	return generic_aura(player, card, event, player, 1, 0, 0, SP_KEYWORD_FEAR, 0, 0, 0);
}

int card_dragon_tyrant(int player, int card, event_t event){
/* CARD_ID_DRAGON_TYRANT	6515
Dragon Tyrant	|8|R|R
Creature - Dragon 6/6
Flying, trample
Double strike (This creature deals both first-strike and regular combat damage.)
At the beginning of your upkeep, sacrifice ~ unless you pay |R|R|R|R.
|R: ~ gets +1/+0 until end of turn. */

	basic_upkeep(player, card, event, MANACOST_R(4));

	return generic_shade(player, card, event, 0, MANACOST_R(1), 1, 0, 0, 0);
}

int card_dragons_wings(int player, int card, event_t event){
	/* Dragon Wings	|1|U
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature has flying.
	 * Cycling |1|U
	 * When a creature with converted mana cost 6 or greater enters the battlefield, you may return ~ from your graveyard to the battlefield attached to that creature. */

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XU(1, 1));
	}
	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		draw_a_card(player);
	}
	return generic_aura(player, card, event, player, 0, 0, KEYWORD_FLYING, 0, 0, 0, 0);
}

int card_dragonspeaker_shaman(int player, int card, event_t event){
/* CARD_ID_DRAGONSPEAKER_SHAMAN	6516
Dragonspeaker Shaman	|1|R|R
Creature - Human Barbarian Shaman 2/2
Dragon spells you cast cost |2 less to cast. */

	if(event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card) ){
	   if( has_subtype(affected_card_controller, affected_card, SUBTYPE_DRAGON) ){
		  COST_COLORLESS-=2;
	   }
	}

	return 1;
}

int card_dragonstorm(int player, int card, event_t event){
	/*
	  Dragonstorm |8|R
	  Sorcery
	  Search your library for a Dragon permanent card and put it onto the battlefield. Then shuffle your library.
	  Storm (When you cast this spell, copy it for each spell cast before it this turn.)
	*/
	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags(player, card, SF_NOT_CAST) ){
			int amount = get_storm_count();

			test_definition_t test;
			new_default_test_definition(&test, TYPE_PERMANENT, "Select a Dragon permanent card.");
			test.subtype = SUBTYPE_DRAGON;

			int i;
			for(i=0;i<amount;i++){
				new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a Dragon permanent card.");
		test.subtype = SUBTYPE_DRAGON;

		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &test);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_edgewalker(int player, int card, event_t event){
/* CARD_ID_EDGEWALKER	6520
Edgewalker	|1|W|B
Creature - Human Cleric 2/2
Cleric spells you cast cost |W|B less to cast. This effect reduces only the amount of colored mana you pay.
(For example, if you cast a Cleric spell with mana cost |1|W, it costs |1 to cast.) */

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card) ){
		if( has_subtype(affected_card_controller, affected_card, SUBTYPE_CLERIC) ){
			COST_BLACK--;
			COST_WHITE--;
		}
	}

	return 1;
}

int card_elvish_aberration(int player, int card, event_t event)
{
  /* Elvish Aberration	|5|G
   * Creature - Elf Mutant 4/5
   * |T: Add |G|G|G to your mana pool.
   * |H2Forestcycling |2 */

  if (IS_ACTIVATING_FROM_HAND(event))
	return landcycling(player, card, event, 2, SUBTYPE_FOREST, 0);

  return mana_producing_creature(player, card, event, 0, COLOR_GREEN, 3);
}

int card_eternal_dragon(int player, int card, event_t event){

	/* Eternal Dragon	|5|W|W
	 * Creature - Dragon Spirit 5/5
	 * Flying
	 * |3|W|W: Return ~ from your graveyard to your hand. Activate this ability only during your upkeep.
	 * |H2Plainscycling |2 */

	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		if( has_mana_multi(player, MANACOST_XW(3, 2)) ){
			int choice = do_dialog(player, player, card, -1, -1," Return Eternal Dragon to hand\n Pass\n", 0);
			if( choice == 0 ){
				if (charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, MANACOST_XW(3,2))){
					card_instance_t* instance = get_card_instance(player, card);
					instance->state &= ~STATE_INVISIBLE;
					return -1;
				}
			}
		}
		return -2;
	}

	return landcycling(player, card, event, 2, SUBTYPE_PLAINS, 0);
}

int card_fierce_empath(int player, int card, event_t event){
/* CARD_ID_FIERCE_EMPATH	6527
Fierce Empath	|2|G
Creature - Elf 1/1
When ~ enters the battlefield, you may search your library for a creature card with converted mana cost 6 or greater, reveal it,
put it into your hand, then shuffle your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card with CMC 6 or more.");
		this_test.cmc = 5;
		this_test.cmc_flag = F5_CMC_GREATER_THAN_VALUE;

		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_CMC, &this_test);
	}

	return 0;
}

int card_final_punishment(int player, int card, event_t event){
/* CARD_ID_FINAL_PUNISHMENT	6528
Final Punishment	|3|B|B
Sorcery
Target player loses life equal to the damage already dealt to him or her this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			lose_life(instance->targets[0].player, get_trap_condition(instance->targets[0].player, TRAP_DAMAGE_TAKEN));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_PLAYER", 1, NULL);

}

int card_forgotten_ancient(int player, int card, event_t event){

	/* Forgotten Ancient	|3|G
	 * Creature - Elemental 0/3
	 * Whenever a player casts a spell, you may put a +1/+1 counter on ~.
	 * At the beginning of your upkeep, you may move any number of +1/+1 counters from ~ onto other creatures. */

	if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_AI(player), NULL) ){
		add_1_1_counter(player, card);
	}

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td;
		base_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.special = TARGET_SPECIAL_NOT_ME;
		td.allow_cancel = 2; //"Done" button

		while( count_1_1_counters(player, card) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
				move_counters(instance->targets[0].player, instance->targets[0].card, player, card, COUNTER_P1_P1, 1);
				instance->number_of_targets = 0;
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_form_of_the_dragon(int player, int card, event_t event){
	/* Form of the Dragon	|4|R|R|R	0x200952b
	 * Enchantment
	 * At the beginning of your upkeep, ~ deals 5 damage to any target.
	 * At the beginning of each end step, your life total becomes 5.
	 * Creatures without flying can't attack you. */
	if( event == EVENT_ATTACK_LEGALITY && affected_card_controller != player && ! is_humiliated(player, card) &&
 		!(get_card_instance(affected_card_controller, affected_card)->regen_status & KEYWORD_FLYING) )
	{
		event_result = 1;
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
			damage_creature_or_player(player, card, event, 5);
			instance->number_of_targets = 1;
		}
	}

	if( life[player] != 5 && eot_trigger(player, card, event) ){
		set_life_total(player, 5);
	}

	return global_enchantment(player, card, event);
}

static int frontline_strategist_legacy(int player, int card, event_t event){

	card_instance_t* damage = combat_damage_being_prevented(event);
	if( damage &&
		(damage->targets[3].player & TYPE_CREATURE) &&	// probably redundant to status check
		!has_subtype(damage->damage_source_player, damage->damage_source_card, SUBTYPE_SOLDIER) )
	{
		prevent_damage(affected_card_controller, affected_card, -1);
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_frontline_strategist(int player, int card, event_t event){
/* CARD_ID_FRONTLINE_STRATEGIST	6532
Frontline Strategist	|W
Creature - Human Soldier 1/1
Morph |W (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, prevent all combat damage non-Soldier creatures would deal this turn. */

	if( event == EVENT_TURNED_FACE_UP ){
		create_legacy_effect(player, card, &frontline_strategist_legacy);
	}

	return morph(player, card, event, MANACOST_W(1));
}

static int gilded_light_legacy(int player, int card, event_t event){

	give_shroud_to_player(player, card, event);

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_gilded_light(int player, int card, event_t event){
/* CARD_ID_GILDED_LIGHT	6533
Gilded Light	|1|W
Instant
You gain shroud until end of turn. (You can't be the target of spells or abilities.)
Cycling |2 (|2, Discard this card: Draw a card.) */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return cycling(player, card, event, MANACOST_X(2));
	}

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &gilded_light_legacy);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_goblin_war_strike(int player, int card, event_t event){
	/* Goblin War Strike	|R	0x200953a
	 * Sorcery
	 * ~ deals damage to target player or planeswalker equal to the number of Goblins you control. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			damage_target0(player, card, count_subtype(player, TYPE_PERMANENT, SUBTYPE_GOBLIN));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
}

int card_goblin_warchief(int player, int card, event_t event)
{
	// !!!DOM: goblin creatures you control => goblins you control
	/* Goblin Warchief	|1|R|R	0x2001173
	 * Creature - Goblin Warrior 2/2
	 * Goblin spells you cast cost |1 less to cast.
	 * Goblins you control have haste. */
	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card) &&
		affected_card_controller == player && has_creature_type(affected_card_controller, affected_card, SUBTYPE_GOBLIN))
		--COST_COLORLESS;

	boost_subtype(player, card, event, SUBTYPE_GOBLIN, 0,0, 0,SP_KEYWORD_HASTE, BCT_CONTROLLER_ONLY|BCT_INCLUDE_SELF);

	return 0;
}

int card_guilty_conscience(int player, int card, event_t event){

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		check_damage_test(p, c, event, DDBM_REPORT_DAMAGE_DEALT, player, card, 0, NULL);
	}

	if( resolve_damage_trigger(player, card, event, DDBM_REPORT_DAMAGE_DEALT, player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int amount = instance->targets[16].player;
		instance->targets[16].player = 0;
		damage_creature(instance->damage_target_player, instance->damage_target_card, amount, player, card);
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			card_instance_t *instance = get_card_instance(player, card);
			if( instance->targets[0].player == player ){
				int csvid = get_id(instance->targets[0].player, instance->targets[0].card);
				switch( csvid ){
						case CARD_ID_STUFFY_DOLL:
						{
							if( can_use_activated_abilities(instance->targets[0].player, instance->targets[0].card) ){
								ai_modifier += 48;
								if( ! is_tapped(instance->targets[0].player, instance->targets[0].card) &&
									! is_sick(instance->targets[0].player, instance->targets[0].card) )
								{
									ai_modifier += 48;
								}
							}
							break;
						}
						case CARD_ID_BOROS_RECKONER:
						case CARD_ID_MOGG_MANIAC:
						case CARD_ID_PHYTOHYDRA:
						case CARD_ID_SABER_ANTS:
						case CARD_ID_SPITEMARE:
						{
							if( ! is_humiliated(instance->targets[0].player, instance->targets[0].card) ){
								ai_modifier += 48;
							}
							break;
						}
						default:
							ai_modifier -= 96;
							break;

				}
			}
			else{
				if( get_attack_power(instance->targets[0].player, instance->targets[0].card) >=
					get_toughness(instance->targets[0].player, instance->targets[0].card) )
				{
						ai_modifier += 48;
				}
				else{
					ai_modifier += 24;
				}
			}
		}
		return 0;
	}

	return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

int card_hindering_touch(int player, int card, event_t event ){
	/* Hindering Touch	|3|U
	 * Instant
	 * Counter target spell unless its controller pays |2.
	 * Storm */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->info_slot = 0;
		if( ! check_special_flags(player, card, SF_NOT_CAST) ){
			get_card_instance(player, card)->info_slot = get_storm_count();
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = 2 + (2 * get_card_instance(player, card)->info_slot);
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, amount);
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return card_counterspell(player, card, event);
}

int card_hunting_pack(int player, int card, event_t event){
	/* Hunting Pack	|5|G|G
	 * Instant
	 * Put a 4/4 |Sgreen Beast creature token onto the battlefield.
	 * Storm */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags(player, card, SF_NOT_CAST) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_BEAST, &token);
			token.pow = token.tou = 4;
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
			token.qty = get_storm_count();
			generate_token(&token);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_BEAST, &token);
		token.pow = token.tou = 4;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_karona_false_god(int player, int card, event_t event){

	check_legend_rule(player, card, event);

	haste(player, card, event);

	// At the beginning of each player's upkeep, that player untaps ~ and gains control of it.
	if( current_turn != player && upkeep_trigger(player, card, event) ){
		untap_card(player, card);
		give_control_of_self(player, card);
	}

	// Whenever ~ attacks, creatures of the creature type of your choice get +3/+3 until end of turn.
	if (declare_attackers_trigger(player, card, event, 0, player, card)){
		pump_subtype_until_eot(player, card, ANYBODY, select_a_subtype_full_choice(player, card, player, NULL, 2), 3, 3, 0,0);
	}

	return 0;
}

int card_krosan_drover(int player, int card, event_t event){
	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) && get_cmc(affected_card_controller, affected_card) > 5){
			COST_COLORLESS-=2;
		}
	}
	return 0;
}

int card_krosan_warchief(int player, int card, event_t event){

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card) ){
		if( has_subtype(affected_card_controller, affected_card, SUBTYPE_BEAST) ){
			COST_COLORLESS--;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_DESTROYED;
	td.required_subtype = SUBTYPE_BEAST;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( land_can_be_played & LCBP_REGENERATION ){
			if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XG(1, 1), 0, &td, NULL) ){
				return 0x63;
			}
		}
	}

	if( event == EVENT_ACTIVATE  ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XG(1, 1), 0,
										&td, "Select target Beast to regenerate.");
	}

	if( event == EVENT_RESOLVE_ACTIVATION && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
		regenerate_target(instance->targets[0].player, instance->targets[0].card);
	}

	return 0;
}

static int effect_lethal_vapors(int player, int card, event_t event ){

	if( get_card_instance(player, card)->targets[0].player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->targets[0].player;
		int c = instance->targets[0].card;

		if( event == EVENT_CAN_ACTIVATE ){
			if( can_use_activated_abilities(p, c) ){
				int cless = get_cost_mod_for_activated_abilities(p, c, MANACOST0);
				if( has_mana(player, COLOR_COLORLESS, cless) ){
					return 1;
				}
			}
		}

		if( event == EVENT_ACTIVATE ){
			int amount = count_subtype(1-player, TYPE_CREATURE, -1)-count_subtype(player, TYPE_CREATURE, -1);
			ai_modifier += (amount * 12);
			int cless = get_cost_mod_for_activated_abilities(p, c, MANACOST0);
			charge_mana(player, COLOR_COLORLESS, cless);
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			skip_next_turn(p, c, player);
			kill_card(p, c, KILL_DESTROY);
		}

		if( other_leaves_play(player, card, p, c, event) ){
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_lethal_vapors(int player, int card, event_t event ){

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		if( new_specific_cip(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &test) ){
			kill_card(trigger_cause_controller, trigger_cause, KILL_DESTROY);
		}
	}

	if( event == EVENT_CAN_CAST )
		return basic_spell(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int amount = count_subtype(player, TYPE_CREATURE, -1)-count_subtype(1-player, TYPE_CREATURE, -1);
		ai_modifier += (amount * 12);
	}

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found[2] = {-1};
		CYCLE_ALL_CARDS(p, c,	{
									if( in_play(p, c) && is_what(p, c, TYPE_EFFECT) ){
										card_instance_t *inst = get_card_instance(player, c);
										if( inst->info_slot == (int)effect_lethal_vapors ){
											if( inst->targets[0].player == player && inst->targets[0].card == card )
											{
												found[p] = c;
											}
										}
									}
								}
		);
		if( found[player] > -1 ){
			kill_card(player, found[player], KILL_EXILE);
		}
		if( found[1-player] == -1){
			int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
			int legacy = create_legacy_effect(1-player, fake, &effect_lethal_vapors);
			card_instance_t *inst = get_card_instance(1-player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int amount = count_subtype(1-player, TYPE_CREATURE, -1)-count_subtype(player, TYPE_CREATURE, -1);
		ai_modifier += (amount * 12);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		skip_next_turn(instance->parent_controller, instance->parent_card, player);
		kill_card(player, instance->parent_card, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

int card_long_term_plans(int player, int card, event_t event){

	/* Long-Term Plans	|2|U
	 * Instant
	 * Search your library for a card, shuffle your library, then put that card third from the top. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 48;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, 0);
		int iid = -1;
		int selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MAX_VALUE, -1, &this_test);
		if( selected != -1 ){
			iid = deck_ptr[player][selected];
			remove_card_from_deck(player, selected);
		}
		shuffle(player);
		if( iid != -1 ){
			put_iid_under_the_first_x_cards_of_library(player, iid, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static int minds_desire_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance( player, card);
	if (event == EVENT_SET_LEGACY_EFFECT_NAME && instance->targets[0].player > -1 ){
		scnprintf(set_legacy_effect_name_addr, 51, "%s", cards_ptr[cards_data[instance->targets[0].player].id]->name);
	}
	if( instance->targets[0].card > -1 && event == EVENT_CAN_ACTIVATE ){
		int rval = can_legally_play_iid(player, instance->targets[0].card);
		if( rval == 99 && (cards_data[instance->targets[0].card].type & (TYPE_INSTANT | TYPE_INTERRUPT)) ){
			return 99;
		}
		if( rval && !(cards_data[instance->targets[0].card].type & (TYPE_INSTANT | TYPE_INTERRUPT)) && ! can_sorcery_be_played(player, event) ){
			rval = 0;
		}
		return rval;
	}
	if( event == EVENT_ACTIVATE ){
		char buffer[500];
		card_ptr_t* c = cards_ptr[ cards_data[instance->targets[0].card].id ];
		scnprintf(buffer, 500, " Play %s\n Cancel", c->name);
		int choice = do_dialog(player, player, card, -1, -1, buffer, 1);
		if( choice == 0 ){
			int iid = instance->targets[0].card;
			instance->targets[0].card = -1;
			play_card_in_exile_for_free(player, player, cards_data[iid].id);
			kill_card(player, card, KILL_EXILE);
			cant_be_responded_to = 1;
		}
		if( choice == 1 ){
			spell_fizzled = 1;
		}
	}
	if( eot_trigger(player, card, event ) ){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

static int create_minds_desire_legacy(int player, int card){
	int *deck = deck_ptr[player];
	int iid = deck[0];
	rfg_top_card_of_deck(player);
	int legacy = create_legacy_activate(player, card, &minds_desire_legacy);
	card_instance_t *instance = get_card_instance( player, legacy);
	instance->targets[0].card = iid;
	instance->eot_toughness = EVENT_SET_LEGACY_EFFECT_NAME;
	return legacy;
}


int card_minds_desire(int player, int card, event_t event){
	if( ! is_unlocked(player, card, event, 28) ){ return 0; }

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (get_storm_count() * 12);
		if( ! check_special_flags(player, card, SF_NOT_CAST) ){
			int count = get_storm_count();
			while( count > 0 ){
					create_minds_desire_legacy(player, card);
					count--;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		create_minds_desire_legacy(player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_mischievous_quanar(int player, int card, event_t event)
{
  card_instance_t* instance = get_card_instance(player, card);

  // |3|U|U: Turn ~ face down.
  if (event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) && has_mana_for_activated_ability(player, card, MANACOST_XU(3,2)))
	return 1;

  if (event == EVENT_ACTIVATE)
	charge_mana_for_activated_ability(player, card, MANACOST_XU(3,2));

  if (event == EVENT_RESOLVE_ACTIVATION)
	turn_face_down(instance->parent_controller, instance->parent_card);

  // Morph |1|U|U
  // When ~ is turned face up, copy target instant or sorcery spell. You may choose new targets for that copy.
  if (event == EVENT_CAN_UNMORPH)
	{
	  if (!has_mana_multi(player, MANACOST_XU(1,2)))
		return 0;

	  int tc = instance->targets[9].card = activate_twincast(player, card, EVENT_CAN_ACTIVATE, NULL, NULL);
	  return tc ? tc : 1;
	}

  int mf = morph(player, card, event, MANACOST_XU(1,2));

  if (event == EVENT_UNMORPH)
	{
	  if (cancel != 1 && card_on_stack_controller != -1 && instance->targets[9].card == 99)
		{
		  activate_twincast(player, card, EVENT_ACTIVATE, NULL, NULL);
		  if (cancel == 1)
			{
			  cancel = 0;
			  instance->targets[9].card = 0;
			  ai_modifier -= 96;
			}
		}
	  else
		ai_modifier -= 96;
	}

  if (event == EVENT_TURNED_FACE_UP)
	{
	  if (instance->targets[9].card == 99)
		activate_twincast(player, card, EVENT_RESOLVE_ACTIVATION, NULL, NULL);
	  cancel = 0;
	}

  return mf;
}

int card_misguided_rage(int player, int card, event_t event){

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent to sacrifice.");
			new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED | SAC_NO_CANCEL, &test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_PLAYER", 1, NULL);
}

int card_nefashu(int player, int card, event_t event)
{
	// Whenever ~ attacks, up to five target creatures each get -1/-1 until end of turn.
	if (declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player) | DAT_STORE_IN_INFO_SLOT, player, card)){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t* instance = get_card_instance(player, card);
		instance->number_of_targets = 0;
		int trgs = 0;
		while( trgs < 5 && can_target(&td) ){
				if( new_pick_target(&td, "TARGET_CREATURE", trgs, 0) ){
					state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
					trgs++;
				}
				else{
					break;
				}
		}
		int i;
		for (i = 0; i < instance->number_of_targets; ++i){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			pump_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, -1, -1);
		}
	}

	return 0;
}

int card_noble_templar(int player, int card, event_t event){
	vigilance(player, card, event);
	return landcycling(player, card, event, 2, SUBTYPE_PLAINS, 0);
}

int card_proteus_machine(int player, int card, event_t event){
	if(event == EVENT_TURNED_FACE_UP ){
		add_a_subtype(player, card, select_a_subtype(player, card));
	}
	return morph(player, card, event, MANACOST0);
}

int card_putrid_raptor(int player, int card, event_t event)
{
  // Morph - Discard a Zombie card.
#define DECL_TEST(test)												\
  test_definition_t test;											\
  new_default_test_definition(&test, 0, "Select a Zombie card.");	\
  test.subtype = SUBTYPE_ZOMBIE;									\
  test.zone = TARGET_ZONE_HAND

  if (event == EVENT_CAN_UNMORPH)
	{
	  DECL_TEST(test);
	  return check_battlefield_for_special_card(player, card, player, 0, &test);
	}

  if (event == EVENT_UNMORPH)
	{
	  DECL_TEST(test);
	  int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &test);
	  if (selected == -1)
		spell_fizzled = 1;
	  else
		discard_card(player, selected);

	  return 0;
	}

  return morph(player, card, event, MANACOST_X(0));
#undef DECL_TEST
}

int card_pyrostatic_pillar(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.cmc = 4;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &test) ){
			damage_player(trigger_cause_controller, 2, player, card);
		}
	}

	return global_enchantment(player, card, event);
}

int card_raven_guild_master(int player, int card, event_t event){

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					while( times_damaged[p] ){
							int count = 0;
							while( count < 10 && deck_ptr[p][0] != -1 ){
									rfg_top_card_of_deck(p);
									count++;
							}
							times_damaged[p]--;
					}
				}
		);
	}

	return morph(player, card, event, MANACOST_XU(2, 2));
}

int card_reaping_the_graves(int player, int card, event_t event){

	/* Reaping the Graves	|2|B
	 * Instant
	 * Return target creature card from your graveyard to your hand.
	 * Storm */

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");

		int swap[500][2];
		int max = 1;
		if( ! check_special_flags(player, card, SF_NOT_CAST) ){
			max += get_storm_count();
		}
		max = MIN(19, max);
		int pos = 0;
		while( max > 0 ){
				int selected = new_select_target_from_grave(player, card, player, 0, AI_MAX_VALUE, &this_test, pos);
				if( selected != -1  ){
					swap[pos][0] = selected;
					swap[pos][1] = turn_card_in_grave_face_down(player, selected);
					pos++;
					max--;
				}
				else{
					break;
				}
		}
		if( pos > 0 ){
			int q;
			for (q = 0; q < pos; q++){
				turn_card_in_grave_face_up(player, swap[q][0], swap[q][1]);
			}
			if( pos > 1 ){
				int i, k;
				for(i=0; i<pos; i++){
					for(k=i; k<(pos-1); k++){
						if( instance->targets[k].player > instance->targets[k+1].player ){
							SWAP(instance->targets[k], instance->targets[k+1]);	// struct copy
						}
					}
				}
			}
			instance->info_slot = pos;
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		int i;
		for(i=(instance->info_slot-1); i>-1; i--){
			int result = validate_target_from_grave(player, card, player, i);
			if( result != -1 ){
				const int *grave = get_grave(player);
				add_card_to_hand(player, grave[result]);
				remove_card_from_grave(player, result);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_riptide_survivor(int player, int card, event_t event){

	if(event == EVENT_TURNED_FACE_UP ){
		multidiscard(player, 2, 0);
		draw_cards(player, 3);
	}

	return morph(player, card, event, MANACOST_XU(1, 2));
}

int card_root_elemental(int player, int card, event_t event){

	if(event == EVENT_TURNED_FACE_UP ){
		if( hand_count[player] ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a Creature card.");
			new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}

	return morph(player, card, event, MANACOST_XG(5, 2));
}

int card_rush_of_knowledge(int player, int card, event_t event){

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (get_highest_cmc_nonland(player) * 12);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, get_highest_cmc_nonland(player));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_shoreline_ranger(int player, int card, event_t event){
	return landcycling(player, card, event, 2, SUBTYPE_ISLAND, 0);
}

int card_siege_gang_commander(int player, int card, event_t event){
	/* Siege-Gang Commander	|3|R|R	0x20011fa
	 * Creature - Goblin 2/2
	 * When ~ enters the battlefield, create three 1/1 |Sred Goblin creature tokens.
	 * |1|R, Sacrifice a Goblin: ~ deals 2 damage to any target. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_tokens_by_id(player, card, CARD_ID_GOBLIN, 3);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Goblin to sacrifice.");
	test.subtype = SUBTYPE_GOBLIN;

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0,
															&td, "TARGET_ANY", 0, &test, 0);
}

int card_skulltap(int player, int card, event_t event){

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 0, NULL);
}

int card_sliver_overlord(int player, int card, event_t event)
{
  /* Sliver Overlord	|W|U|B|R|G
   * Legendary Creature - Sliver Mutant 7/7
   * |3: Search your library for a Sliver card, reveal that card, and put it into your hand. Then shuffle your library.
   * |3: Gain control of target Sliver. */

  if( ! is_unlocked(player, card, event, 29) ){ return 0; }

  check_legend_rule(player, card, event);

  if (IS_ACTIVATING(event))
	{
	  if (event == EVENT_CAN_ACTIVATE && !CAN_ACTIVATE(player, card, MANACOST_X(3)))
		return 0;

	  card_instance_t* instance = get_card_instance(player, card);

	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_PERMANENT);
	  td.required_subtype = SUBTYPE_SLIVER;

	  target_definition_t td_opp;
	  default_target_definition(player, card, &td_opp, TYPE_PERMANENT);
	  td_opp.required_subtype = SUBTYPE_SLIVER;
	  td_opp.allowed_controller = 1-player;

	  enum
	  {
		CHOICE_TUTOR = 1,
		CHOICE_CONTROL
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM,
						"Search for a Sliver", 1, 1,
						"Control a Sliver", can_target(&td), IS_AI(player) && can_target(&td_opp) ? 2 : -1);

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (event == EVENT_ACTIVATE)
		{
		  instance->number_of_targets = 0;
		  if (charge_mana_for_activated_ability(player, card, MANACOST_X(3)))
			switch (choice)
			  {
				case CHOICE_TUTOR:
				  break;

				case CHOICE_CONTROL:
				  pick_next_target_noload(&td, "Select target Sliver.");
				  break;
			  }
		}
	  else	// EVENT_RESOLVE_ACTIVATION
		switch (choice)
		  {
			case CHOICE_TUTOR:
			  ;test_definition_t test;
			  new_default_test_definition(&test, TYPE_PERMANENT, "Select a Sliver card.");
			  test.subtype = SUBTYPE_SLIVER;
			  new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
			  break;

			case CHOICE_CONTROL:
			  if (valid_target(&td))
				gain_control(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
			  break;
		  }
	}

  return slivercycling(player, card, event);
}

int card_soul_collector(int player, int card, event_t event){
/*
Soul Collector |3|B|B
Creature — Vampire 3/4
Flying
Whenever a creature dealt damage by Soul Collector this turn dies, return that card to the battlefield under your control.
Morph {B}{B}{B} (You may cast this card face down as a 2/2 creature for {3}. Turn it face up any time for its morph cost.)
*/
	if( event == EVENT_DEAL_DAMAGE ){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *damage = get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->info_slot > 0 ){
			if( damage->damage_source_card == card && damage->damage_source_player == player &&
				damage->damage_target_card != -1 && ! is_token(damage->damage_target_player, damage->damage_target_card) )
			{
				int i;
				for(i=1; i<10; i++){
					if( instance->targets[i].player == -1 ){
						instance->targets[i].player = damage->damage_target_player;
						instance->targets[i].card = damage->damage_target_card;
						break;
					}
				}
			}
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! is_token(affected_card_controller, affected_card) && is_this_dying(affected_card_controller, affected_card) ){
			int i;
			for(i=1; i<10; i++){
				if( instance->targets[i].player > -1 && ! BYTE1(instance->targets[i].player) ){
					if( instance->targets[i].player == affected_card_controller && instance->targets[i].card == affected_card ){
						SET_BYTE0(instance->targets[i].player) = get_owner(affected_card_controller, affected_card);
						SET_BYTE1(instance->targets[i].player) = 1;
						instance->targets[i].card = get_original_internal_card_id(affected_card_controller, affected_card);
						instance->targets[11].player = 66;
						break;
					}
				}
			}
		}
	}


	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance(player, card);
		int i;
		for(i=1; i<10; i++){
			if( instance->targets[i].player > -1 && BYTE1(instance->targets[i].player) == 1 ){
				int owner = BYTE0(instance->targets[i].player);
				int iid = instance->targets[i].card;
				instance->targets[i].player = instance->targets[i].card = -1;
				seek_grave_for_id_to_reanimate(player, card, owner, cards_data[iid].id, REANIMATE_DEFAULT);
			}
		}
		instance->targets[11].card = 0;
	}

	if( event == EVENT_CLEANUP ){
		card_instance_t *instance = get_card_instance(player, card);
		int i;
		for(i=1; i<10; i++){
			instance->targets[i].player = instance->targets[i].card = -1;
		}
	}

	return morph(player, card, event, MANACOST_B(3));
}

int card_sprouting_vines(int player, int card, event_t event){

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a basic land card.");
		this_test.subtype = SUBTYPE_BASIC;
		int i, storm = 0;
		if( ! check_special_flags(player, card, SF_NOT_CAST) ){
			storm = get_storm_count();
		}
		for (i = 0; i < storm; ++i){
			// Deliberately shuffle after each search
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a basic land card.");
		this_test.subtype = SUBTYPE_BASIC;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_stifle(int player, int card, event_t event)
{
  if (event == EVENT_CAN_CAST)
	return can_counter_activated_ability(player, card, event, NULL);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  get_card_instance(player, card)->number_of_targets = 0;
	  return cast_counter_activated_ability(player, card, 0);
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  resolve_counter_activated_ability(player, card, NULL, 0);
	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_sulfuric_vortex(int player, int card, event_t event){
/* CARD_ID_SULFURIC_VORTEX	6586
Sulfuric Vortex	|1|R|R
Enchantment
At the beginning of each player's upkeep, ~ deals 2 damage to that player.
If a player would gain life, that player gains no life instead. */

	upkeep_trigger_ability(player, card, event, 2);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		damage_player(current_turn, 2, player, card);
	}

	if( in_play(player, card) && event == EVENT_STATIC_EFFECTS && affect_me(player, card) && ! is_humiliated(player, card) )
	{
		player_bits[0] |= PB_CANT_GAIN_LIFE;
		player_bits[1] |= PB_CANT_GAIN_LIFE;
	}

	return global_enchantment(player, card, event);
}

int card_temple_of_the_false_god(int player, int card, event_t event){
/* CARD_ID_TEMPLE_OF_THE_FALSE_GOD	6587
Temple of the False God	""
Land
|T: Add |C|C to your mana pool. Activate this ability only if you control five or more lands. */

	if( event == EVENT_CAN_ACTIVATE || (event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		if( count_subtype(player, TYPE_LAND, -1) < 5 ){
			return 0;
		}
	}

	return two_mana_land(player, card, event, COLOR_COLORLESS, COLOR_COLORLESS);
}

int card_tendrils_of_agony(int player, int card, event_t event)
{
	/* Tendrils of Agony	|2|B|B
	 * Sorcery
	 * Target player loses 2 life and you gain 2 life.
	 * Storm */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t* instance = get_card_instance(player, card);

	if(event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 && ! check_special_flags(player, card, SF_NOT_CAST) ){
			int i;
			for(i=0;i<get_storm_count();i++){
				if( new_pick_target(&td, "TARGET_PLAYER", 1, 0) ){
					lose_life(instance->targets[1].player, 2);
					gain_life(player, 2);
				}
			}
			instance->number_of_targets = 1;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if (valid_target(&td)){
			lose_life(instance->targets[0].player, 2);
			gain_life(player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_twisted_abomination(int player, int card, event_t event)
{
  /* Twisted Abomination	|5|B
   * Creature - Zombie Mutant 5/3
   * |B: Regenerate ~.
   * |H2Swampcycling |2 */

  if (IS_ACTIVATING_FROM_HAND(event))
	return landcycling(player, card, event, 2, SUBTYPE_SWAMP, 0);

  return regeneration(player, card, event, MANACOST_B(1));
}

int card_unburden(int player, int card, event_t event){
/* CARD_ID_UNBURDEN	6596
Unburden	|1|B|B
Sorcery
Target player discards two cards.
Cycling |2 (|2, Discard this card: Draw a card.) */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return cycling(player, card, event, MANACOST_X(2));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			new_multidiscard(instance->targets[0].player, 2, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_PLAYER", 1, NULL);
}

int card_undead_warchief(int player, int card, event_t event){
/* CARD_ID_UNDEAD_WARCHIEF	6598
Undead Warchief	|2|B|B
Creature - Zombie 1/1
Zombie spells you cast cost |1 less to cast.
Zombie creatures you control get +2/+1. */

	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card) ){
		if( affected_card_controller == player && has_subtype(affected_card_controller, affected_card, SUBTYPE_ZOMBIE) ){
			COST_COLORLESS--;
		}
	}

	boost_creature_type(player, card, event, SUBTYPE_ZOMBIE, 2, 1, 0, BCT_CONTROLLER_ONLY|BCT_INCLUDE_SELF);

	return 0;
}

int card_unspeakable_symbol(int player, int card, event_t event){
/* CARD_ID_UNSPEAKABLE_SYMBOL	6599
Unspeakable Symbol	|1|B|B
Enchantment
Pay 3 life: Put a +1/+1 counter on target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 3, &td, "TARGET_CREATURE");
}

static int vengeful_dead_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_VENGEFUL_DEAD ){
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
						//Will trigger also if this is not a creature when it goes into the graveyard;
						count_for_gfp_ability(player, card, event, ANYBODY, TYPE_PERMANENT, NULL);
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
						if( instance->targets[11].player < 0 ){
							instance->targets[11].player = 0;
						}
						SET_BYTE1(instance->targets[11].player) = 1;
						return 0;
					}
				}
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_PERMANENT);
				this_test.subtype = SUBTYPE_ZOMBIE;
				count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			lose_life(1-player, amount);
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

int card_vengeful_dead(int player, int card, event_t event){
/* CARD_ID_VENGEFUL_DEAD	6601
Vengeful Dead	|3|B
Creature - Zombie 3/2
Whenever ~ or another Zombie dies, each opponent loses 1 life. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)vengeful_dead_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &vengeful_dead_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( affect_me(player, card) ){
			count_for_gfp_ability(player, card, event, ANYBODY, TYPE_PERMANENT, NULL);
		}
		else{
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_PERMANENT);
			this_test.subtype = SUBTYPE_ZOMBIE;
			count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			lose_life(1-player, amount);
		}
	}

	return 0;
}

int card_wing_shards(int player, int card, event_t event){
/*
Wing Shards |1|W|W
Instant
Target player sacrifices an attacking creature.
Storm (When you cast this spell, copy it for each spell cast before it this turn.)
*/
	// Approximation

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int i;
		int base_target = 0;
		for(i=0; i<get_storm_count()+1; i++){
			if( new_pick_target(&td, "TARGET_PLAYER", base_target, 1) ){
				base_target++;
			}
			else{
				break;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		int i;
		card_instance_t *instance = get_card_instance( player, card );
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){

				test_definition_t test;
				default_test_definition(&test, TYPE_CREATURE);
				test.state = STATE_ATTACKING;

				new_sacrifice(player, card, instance->targets[i].player, SAC_NO_CANCEL | SAC_CAUSED, &test);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_wirewood_guardian(int player, int card, event_t event){
/* CARD_ID_WIREWOOD_GUARDIAN	6604
Wirewood Guardian	|5|G|G
Creature - Elf Mutant 6/6
|H2Forestcycling |2 (|2, Discard this card: Search your library for |Ha Forest card, reveal it, and put it into your hand. Then shuffle your library.) */

	return landcycling(player, card, event, 2, SUBTYPE_FOREST, 0);
}

int card_wirewood_symbiote(int player, int card, event_t event){
/* CARD_ID_WIREWOOD_SYMBIOTE	6605
Wirewood Symbiote	|G
Creature - Insect 1/1
Return an Elf you control to its owner's hand: Untap target creature. Activate this ability only once each turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_ELF;
	td.illegal_abilities = 0;

	return quirion_ranger_ability(player, card, event, &td, "Select an Elf you control.");
}

int card_xantid_swarm(int player, int card, event_t event){
/* CARD_ID_XANTID_SWARM	6607
Xantid Swarm	|G
Creature - Insect 0/1
Flying
Whenever ~ attacks, defending player can't cast spells this turn. */

	if( declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		target_player_cant_cast_type(player, card, 1-player, TYPE_CREATURE|TYPE_ENCHANTMENT|TYPE_ARTIFACT|TARGET_TYPE_PLANESWALKER|TYPE_SPELL|TYPE_EFFECT);
	}

	return 0;
}

int card_zombie_cutthroat(int player, int card, event_t event)
{
/* CARD_ID_ZOMBIE_CUTTHROAT	6609
Zombie Cutthroat	|3|B|B
Creature - Zombie 3/4
Morph-Pay 5 life. (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

  if (event == EVENT_CAN_UNMORPH)
	return can_pay_life(player, 5);

  if (event == EVENT_UNMORPH)
	{
	  lose_life(player, 5);
	  return 0;
	}

  return morph(player, card, event, MANACOST_X(0));
}

