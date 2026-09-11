#include "manalink.h"

// GLOBAL FUNCTIONS

static int battle_cry(int player, int card, event_t event)
{
  // Whenever this creature attacks, each other attacking creature gets +1/+0 until end of turn.
  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.state = STATE_ATTACKING;
	  test.not_me = 1;
	  pump_creatures_until_eot(player, card, current_turn, 0, 1, 0, 0, 0, &test);
	}

  return 0;
}


// CARDS

int card_thopter_assembly(int player, int card, event_t event){
	/* Thopter Assembly	|6
	 * Artifact Creature - Thopter 5/5
	 * Flying
	 * At the beginning of your upkeep, if you control no Thopters other than ~, return ~ to its owner's hand and put five 1/1 colorless Thopter artifact creature tokens with flying onto the battlefield. */

	if( current_turn == player && upkeep_trigger(player, card, event) ){
		if( in_play(player, card) && count_subtype(player, TYPE_CREATURE, SUBTYPE_THOPTER) < 2 ){
			bounce_permanent(player, card);
			generate_tokens_by_id(player, card, CARD_ID_THOPTER, 5);
		}
	}
	return 0;
}

static int glissa_the_traitor_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_GLISSA_THE_TRAITOR ){
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
				count_for_gfp_ability(player, card, event, 1-player, TYPE_CREATURE, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			int count = count_graveyard_by_type(player, TYPE_ARTIFACT);
			if( ! graveyard_has_shroud(player) ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ARTIFACT, "Select target artifact card.");
				while( amount && count ){
						if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &test) > -1 ){
							count--;
							amount--;
						}
						else{
							break;
						}
				}
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

int card_glissa_the_traitor(int player, int card, event_t event){
/*
Glissa, the Traitor |B|G|G
Legendary Creature — Zombie Elf 3/3
First strike, deathtouch
Whenever a creature an opponent controls dies, you may return target artifact card from your graveyard to your hand.
*/
	deathtouch(player, card, event);

	check_legend_rule(player, card, event);

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)glissa_the_traitor_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &glissa_the_traitor_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, 1-player, TYPE_CREATURE, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			int count = count_graveyard_by_type(player, TYPE_ARTIFACT);
			if( ! graveyard_has_shroud(player) ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ARTIFACT, "Select target artifact card.");
				while( amount && count ){
						if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &test) > -1 ){
							count--;
							amount--;
						}
						else{
							break;
						}
				}
			}
		}
	}

	return 0;
}

int card_hero_of_bladehold( int player, int card, event_t event)
{
  // Battle cry (Whenever this creature attacks, each other attacking creature gets +1/+0 until end of turn.)
  // Whenever ~ attacks, put two 1/1 |Swhite Soldier creature tokens onto the battlefield tapped and attacking.
  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  token_generation_t token;
	  default_token_definition(player, card, CARD_ID_SOLDIER, &token);
	  token.qty = 2;
	  token.action = TOKEN_ACTION_ATTACKING;
	  generate_token(&token);

	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.state = STATE_ATTACKING;
	  test.not_me = 1;
	  pump_creatures_until_eot(player, card, current_turn, 0, 1,0, 0,0, &test);
	}

  return 0;
}

int card_tezzeret_agent_of_bolas(int player, int card, event_t event){

	/* Tezzeret, Agent of Bolas	|2|U|B
	 * Planeswalker - Tezzeret (3)
	 * +1: Look at the top five cards of your library. You may reveal an artifact card from among them and put it into your hand. Put the rest on the bottom of
	 * your library in any order.
	 * -1: Target artifact becomes a 5/5 artifact creature.
	 * -4: Target player loses X life and you gain X life, where X is twice the number of artifacts you control. */

	if (IS_ACTIVATING(event)){

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.preferred_controller = player;

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		int priority_steal_life = 0;
		if( event == EVENT_ACTIVATE ){
			int life_to_steal = count_subtype(player, TYPE_ARTIFACT, -1)*2;
			priority_steal_life = (life[player] < 6 && life[player]+life_to_steal > 6) ? 20 : 0;
			priority_steal_life += (life[1-player] - life_to_steal < 1) ? 20 : 0;
			priority_steal_life += ((count_counters(player, card, COUNTER_LOYALTY)*2)-8);
		}

		int priority_aa = 0;
		if( event == EVENT_ACTIVATE ){
			priority_aa += ((count_counters(player, card, COUNTER_LOYALTY)*15)-15);
		}

		enum{
			CHOICE_LOOK5 = 1,
			CHOICE_ANIMATE_ARTIFACT,
			CHOICE_STEAL_LIFE
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Look at the top 5", 1, 10, 1,
						"Animate an artifact", can_target(&td), priority_aa, -1,
						"My artifacts, your life", can_target(&td1), priority_steal_life, -4);

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
				case CHOICE_ANIMATE_ARTIFACT:
					pick_target(&td, "TARGET_ARTIFACT");
					break;

				case CHOICE_LOOK5:
					break;

				case CHOICE_STEAL_LIFE:
					pick_target(&td1, "TARGET_PLAYER");
					break;
			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		switch (choice)
		{
			case CHOICE_LOOK5:
				reveal_x_and_choose_a_card_type( instance->parent_controller, instance->parent_card, 5, TYPE_ARTIFACT);
				break;

			case CHOICE_ANIMATE_ARTIFACT:
				{
					if( valid_target(&td) ){
						artifact_animation(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
											0, 5, 5, 0, 0);
					}
				}
				break;

			case CHOICE_STEAL_LIFE:
				{
					if( valid_target(&td1) ){
						int count = count_subtype(player, TYPE_ARTIFACT, -1);
						gain_life(player, count*2);
						lose_life(instance->targets[0].player, count*2);
					}
				}
				break;
		}
	}

	return planeswalker(player, card, event, 3);
}

int card_sword_of_feast_and_famine(int player, int card, event_t event)
{
  /* Sword of Feast and Famine	|3
   * Artifact - Equipment
   * Equipped creature gets +2/+2 and has protection from |Sblack and from |Sgreen.
   * Whenever equipped creature deals combat damage to a player, that player discards a card and you untap all lands you control.
   * Equip |2 */

  if (event == EVENT_ABILITIES || event == EVENT_POWER || event == EVENT_TOUGHNESS)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (affect_me(instance->damage_target_player, instance->damage_target_card))
		{
		  if (event == EVENT_ABILITIES)
			event_result |= get_sleighted_protection(player, card, KEYWORD_PROT_BLACK | KEYWORD_PROT_GREEN);
		  else
			event_result += 2;
		}
	}

  if (equipped_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_TRACE_DAMAGED_PLAYERS))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
	  instance->targets[1].player = 0;
	  int p;
	  for (p = 0; p <= 1; ++p)
		for (; times_damaged[p] > 0; --times_damaged[p])
		  {
			discard(p, 0, player);
			manipulate_type(player, card, player, TYPE_LAND, ACT_UNTAP);
		  }
	}

  return basic_equipment(player, card, event, 2);
}

int card_viridian_emissary(int player, int card, event_t event ){
/* CARD_ID_VIRIDIAN_EMISSARY	11553
Viridian Emissary	|1|G
Creature - Elf Scout 2/1
When ~ dies, you may search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		tutor_basic_land(player, 1, 1);
	}
	return 0;
}

int card_blightsteel_colossus(int player, int card, event_t event)
{
  /* Blightsteel Colossus	|12
   * Artifact Creature - Golem 11/11
   * Trample, infect
   * ~ is indestructible.
   * If ~ would be put into a graveyard from anywhere, reveal ~ and shuffle it into its owner's library instead. */

  // Last ability handled in special_mill() and raw_put_iid_on_top_of_graveyard_with_triggers().

  indestructible(player, card, event);
  return 0;
}

int card_leonin_relic_warder(int player, int card, event_t event){
/* CARD_ID_LEONIN_RELIC_WARDER	11472
Leonin Relic-Warder	|W|W
Creature - Cat Cleric 2/2
When ~ enters the battlefield, you may exile target artifact or enchantment.
When ~ leaves the battlefield, return the exiled card to the battlefield under its owner's control. */

	return_from_oblivion(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);
		td.allowed_controller = 2;
		td.preferred_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "DISENCHANT", 0, 0) ){
			obliviation(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_red_suns_zenith(int player, int card, event_t event){
	/* Red Sun's Zenith	|X|R	0x2002c17
	 * Sorcery
	 * ~ deals X damage to any target. If a creature dealt damage this way would die this turn, exile it instead. Shuffle ~ into its owner's library. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int dc = damage_target0(player, card, instance->info_slot);
			if( instance->targets[0].card > -1 && is_what(instance->targets[0].player, instance->targets[0].card, TYPE_CREATURE) ){
				get_card_instance(player, dc)->targets[3].card |= DMG_EXILE_IF_LETHALLY_DAMAGED_THIS_WAY;
			}
			if( !is_token(player, card) ){
				shuffle_into_library(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_blue_suns_zenith(int player, int card, event_t event){
/* CARD_ID_BLUE_SUNS_ZENITH	11422
Blue Sun's Zenith	|X|U|U|U
Instant
Target player draws X cards. Shuffle ~ into its owner's library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			if( instance->targets[0].player == player ){
				int mod = current_turn == player ? 4 : 8;
				ai_modifier += ((7*mod) - (hand_count[player]*mod));
			}
			else{
				if( count_deck(1-player)-instance->info_slot <= 1 ){
					ai_modifier += (instance->info_slot * 16);
				}
				else{
					ai_modifier -= 96;
				}
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(instance->targets[0].player, instance->info_slot);
			if( ! is_token(player, card) ){
				shuffle_into_library(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_black_suns_zenith(int player, int card, event_t event){
    /* CARD_ID_BLACK_SUNS_ZENITH
	 * Black Sun's Zenith	|X|B|B
	 * Sorcery
	 * Put X -1/-1 counters on each creature. Shuffle ~ into its owner's library. */

	 if( event == EVENT_RESOLVE_SPELL ){
		manipulate_type(player, card, ANYBODY, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_M1_M1, get_card_instance(player, card)->info_slot));
		if( ! is_token(player, card) ){
			shuffle_into_library(player, card);
			return 0;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_green_suns_zenith(int player, int card, event_t event){
/* CARD_ID_GREEN_SUNS_ZENITH	11454
Green Sun's Zenith	|X|G
Sorcery
Search your library for a |Sgreen creature card with converted mana cost X or less, put it onto the battlefield, then shuffle your library.
Shuffle ~ into its owner's library. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		x_value = 0; //For avoiding problems with creatures with X in the CMC cost.
		char msg[100];
		scnprintf(msg, 100, "%s CMC %d or less", get_sleighted_color_text(player, card, "Select a %s creature card", COLOR_GREEN),
				instance->info_slot);
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, msg);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		this_test.cmc = instance->info_slot+1;
		this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
		shuffle_into_library(player, card);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_white_suns_zenith(int player, int card, event_t event){
	/* White Sun's Zenith	|X|W|W|W
	 * Instant
	 * Put X 2/2 |Swhite Cat creature tokens onto the battlefield. Shuffle ~ into its owner's library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == player ? -24 : 24);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_tokens_by_id(player, card, CARD_ID_CAT, instance->info_slot);
		if( ! is_token(player, card) ){
			shuffle_into_library(player, card);
			return 0;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_phyrexian_vatmother(int player, int card, event_t event ){
/* CARD_ID_PHYREXIAN_VATMOTHER	11502
Phyrexian Vatmother	|2|B|B
Creature - Horror 4/5
Infect (This creature deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.)
At the beginning of your upkeep, you get a poison counter. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		poison(player, 1);
	}

	return 0;
}

int card_victorys_herald(int player, int card, event_t event)
{
/* CARD_ID_VICTORYS_HERALD	11550
Victory's Herald	|3|W|W|W
Creature - Angel 4/4
Flying
Whenever ~ attacks, attacking creatures gain flying and lifelink until end of turn. */

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.state = STATE_ATTACKING;
	  pump_creatures_until_eot(player, card, current_turn, 0, 0,0, KEYWORD_FLYING,SP_KEYWORD_LIFELINK, &test);
	}

  return 0;
}

int card_steel_sabotage(int player, int card, event_t event ){
/*
Steel Sabotage |U
Instant
Choose one —
• Counter target artifact spell.
• Return target artifact to its owner's hand.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ARTIFACT);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance( player, card);

	enum{
		CHOICE_COUNTER = 1,
		CHOICE_BOUNCE
	};

	if( event == EVENT_CAN_CAST ){
		int result = generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
		if( result ){
			return result;
		}
		return generic_spell(player, card, event, GS_CAN_TARGET, &td2, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = CHOICE_BOUNCE;
			if( generic_spell(player, card, EVENT_CAN_CAST, GS_COUNTERSPELL, &td, NULL, 1, NULL) ){
				instance->info_slot = CHOICE_COUNTER;
			}
			if( ! IS_AI(player) ){
				int choice = DIALOG(player, card, event, DLG_RANDOM,
									"Counter an artifact spell", instance->info_slot == CHOICE_COUNTER, 1,
									"Bounce an artifact", can_target(&td2), 1);
				if( ! choice ){
					spell_fizzled = 1;
					return 0;
				}
			}
		}
		if( instance->info_slot == CHOICE_COUNTER ){
			return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
		}
		if( instance->info_slot  == CHOICE_BOUNCE ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td2, "TARGET_ARTIFACT", 1, NULL);
		}
	}

	if(event == EVENT_RESOLVE_SPELL){
		if( instance->info_slot == CHOICE_COUNTER ){
			return counterspell(player, card, event, &td, 0);
		}
		if( instance->info_slot == CHOICE_BOUNCE ){
			if( valid_target(&td2) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_treasure_mage(int player, int card, event_t event ){
/* CARD_ID_TREASURE_MAGE	11545
Treasure Mage	|2|U
Creature - Human Wizard 2/2
When ~ enters the battlefield, you may search your library for an artifact card with converted mana cost 6 or greater, reveal that card, and put it into your hand.
If you do, shuffle your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact card with CMC 6 or greater.");
		test.cmc = 5;
		test.cmc_flag = F5_CMC_GREATER_THAN_VALUE;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
	}

	return 0;
}

static int sangromancer_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_SANGROMANCER ){
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
				count_for_gfp_ability(player, card, event, 1-player, TYPE_CREATURE, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			gain_life(player, 3*amount);
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

int card_sangromancer(int player, int card, event_t event ){
/*
Sangromancer |2|B|B
Creature — Vampire Shaman 3/3
Flying
Whenever a creature an opponent controls dies, you may gain 3 life.
Whenever an opponent discards a card, you may gain 3 life.
*/

	if( discard_trigger(player, card, event, 1-player, RESOLVE_TRIGGER_DUH, 0) ){
		gain_life(player, 3);
	}

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)sangromancer_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &sangromancer_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, 1-player, TYPE_CREATURE, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			int amount = BYTE0(result);
			gain_life(player, 3*amount);
		}
	}

	return 0;
}


int card_phyrexian_hydra(int player, int card, event_t event ){
/*
Phyrexian Hydra |3|G|G
Creature — Hydra 7/7
Infect (This creature deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.)
If damage would be dealt to Phyrexian Hydra, prevent that damage. Put a -1/-1 counter on Phyrexian Hydra for each 1 damage prevented this way.
*/
	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *damage = damage_being_prevented(event);
		if( damage ){
			if( damage->damage_target_player == player && damage->damage_target_card == card ){
				int result = prevent_damage(affected_card_controller, affected_card, -1);
				add_counters(player, card, COUNTER_M1_M1, result);
			}
		}
	}

	return 0;
}

int card_rot_wolf(int player, int card, event_t event)
{
/* CARD_ID_ROT_WOLF	11516
Rot Wolf	|2|G
Creature - Wolf 2/2
Infect (This creature deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.)
Whenever a creature dealt damage by ~ this turn dies, you may draw a card. */

	if_a_creature_damaged_by_me_dies_do_something(player, card, event, 2);
	return 0;
}

int card_ichor_wellspring(int player, int card, event_t event ){
/* CARD_ID_ICHOR_WELLSPRING	11462
Ichor Wellspring	|2
Artifact
When ~ enters the battlefield or is put into a graveyard from the battlefield, draw a card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY)){
		draw_cards(player, 1);
	}

	return 0;
}

int card_signal_pest(int player, int card, event_t event ){
/* CARD_ID_SIGNAL_PEST	11524
Signal Pest	|1
Artifact Creature - Pest 0/1
Battle cry (Whenever this creature attacks, each other attacking creature gets +1/+0 until end of turn.)
~ can't be blocked except by creatures with flying or reach. */

	battle_cry(player, card, event);

	if (event == EVENT_ATTACK_RATING && affect_me(player, card)){
		ai_defensive_modifier -= 12;
	}

	return card_orchard_spirit(player, card, event);
}

static int effect_hero_of_oxid_ridge(int player, int card, event_t event)
{

  if (event == EVENT_BLOCK_LEGALITY
	  && get_power(affected_card_controller, affected_card) <= 1)
	event_result = 1;

  if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

int card_hero_of_oxid_ridge(int player, int card, event_t event)
{
/* CARD_ID_HERO_OF_OXID_RIDGE	11459
Hero of Oxid Ridge	|2|R|R
Creature - Human Knight 4/2
Haste
Battle cry (Whenever this creature attacks, each other attacking creature gets +1/+0 until end of turn.)
Whenever ~ attacks, creatures with power 1 or less can't block this turn. */

  haste(player, card, event);

  battle_cry(player, card, event);

  // Whenever ~ attacks, creatures with power 1 or less can't block this turn.
  if (event == EVENT_DECLARE_ATTACKERS && is_attacking(player, card))
	create_legacy_effect(player, card, &effect_hero_of_oxid_ridge);

  return 0;
}

int card_creeping_corrosion(int player, int card, event_t event ){
	/* Creeping Corrosion	|2|G|G
	 * Sorcery
	 * Destroy all artifacts. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		APNAP(p, { new_manipulate_all(player, card, p, &test, KILL_DESTROY); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_go_for_the_throat(int player, int card, event_t event ){
/* CARD_ID_GO_FOR_THE_THROAT	11451
Go for the Throat	|1|B
Instant
Destroy target nonartifact creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_type = TYPE_ARTIFACT;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nonartifact creature.", 1, NULL);
}

int card_vivisection(int player, int card, event_t event ){
	/* Vivisection	|3|U
	 * Sorcery
	 * As an additional cost to cast ~, sacrifice a creature.
	 * Draw three cards. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (28-(hand_count[player]*4) + (count_subtype(player, TYPE_CREATURE, -1)*2));
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 3);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 1, NULL);
}

int card_thrun_the_last_troll(int player, int card, event_t event){
/* CARD_ID_THRUN_THE_LAST_TROLL	11541
Thrun, the Last Troll	|2|G|G
Legendary Creature - Troll Shaman 4/4
~ can't be countered.
Hexproof (This creature can't be the target of spells or abilities your opponents control.)
|1|G: Regenerate ~. */

	check_legend_rule(player, card, event);

	cannot_be_countered(player, card, event);

	hexproof(player, card, event);

	return regeneration(player, card, event, MANACOST_XG(1, 1));
}


int card_spine_of_ish_sah(int player, int card, event_t event ){
/*
Spine of Ish Sah |7
Artifact
When Spine of Ish Sah enters the battlefield, destroy target permanent.
When Spine of Ish Sah is put into a graveyard from the battlefield, return Spine of Ish Sah to its owner's hand.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_PERMANENT", 0, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	int owner, position;
	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) && find_in_owners_graveyard(player, card, &owner, &position) ){
		from_grave_to_hand(owner, position, TUTOR_HAND);
	}

	return 0;
}

int card_turn_the_tide(int player, int card, event_t event){
/* CARD_ID_TURN_THE_TIDE	11546
Turn the Tide	|1|U
Instant
Creatures your opponents control get -2/-0 until end of turn. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == 1-player ? count_attackers(current_turn)*8 : -48);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, 1-player, -1, -2, 0, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static int massacre_wurm_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_MASSACRE_WURM ){
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
				count_for_gfp_ability(player, card, event, 1-player, TYPE_CREATURE, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			lose_life(1-player, 2*amount);
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

int card_massacre_wurm(int player, int card, event_t event){
/* CARD_ID_MASSACRE_WURM	11476
Massacre Wurm	|3|B|B|B
Creature - Wurm 6/5
When ~ enters the battlefield, creatures your opponents control get -2/-2 until end of turn.
Whenever a creature an opponent controls dies, that player loses 2 life. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		pump_subtype_until_eot(player, card, 1-player, -1, -2, -2, 0, 0);
	}

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)massacre_wurm_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &massacre_wurm_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, 1-player, TYPE_CREATURE, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			lose_life(1-player, 2*amount);
		}
	}

	return 0;
}

int card_ardent_recruit(int player, int card, event_t event){
/* CARD_ID_ARDENT_RECRUIT	11415
Ardent Recruit	|W
Creature - Human Soldier 1/1
Metalcraft - ~ gets +2/+2 as long as you control three or more artifacts. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card) &&
		metalcraft(player, card) )
	{
	   event_result+=2;
	}

	return 0;
}

int card_galvanoth(int player, int card, event_t event){
/* Galvanoth	|3|R|R
 * Creature - Beast 3/3
 * At the beginning of your upkeep, you may look at the top card of your library. If it's an instant or sorcery card, you may cast
 * 		it without paying its mana cost. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int* deck = deck_ptr[player];
		if( deck[0] != -1 ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_SPELL, "You could play this card if it is an instant or sorcery.");
			this_test.create_minideck = 1;

			int selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_FIRST_FOUND, -1, &this_test);
			if( selected != -1 ){
				play_card_in_deck_for_free(player, player, selected);
			}
		}
	}

	return 0;
}

int card_flayer_husk(int player, int card, event_t event ){
/* CARD_ID_FLAYER_HUSK	11442
Flayer Husk	|1
Artifact - Equipment
Living weapon (When this Equipment enters the battlefield, create a 0/0 |Sblack Germ creature token, then attach this to it.)
Equipped creature gets +1/+1.
Equip |2 */

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			event_result++;
		}
	}

	return living_weapon(player, card, event, 2);
}

int card_sphere_of_the_suns(int player, int card, event_t event)
{
  /* Sphere of the Suns	|2
   * Artifact
   * ~ enters the battlefield tapped and with three charge counters on it.
   * |T, Remove a charge counter from ~: Add one mana of any color to your mana pool. */

  comes_into_play_tapped(player, card, event);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, 3);

  if (event == EVENT_CAN_ACTIVATE)
	return CAN_TAP_FOR_MANA(player, card) && count_counters(player, card, COUNTER_CHARGE) > 0;

  if (event == EVENT_ACTIVATE && produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 1))
	remove_counter(player, card, COUNTER_CHARGE);

  if (event == EVENT_COUNT_MANA && affect_me(player, card)
	  && CAN_TAP_FOR_MANA(player, card) && count_counters(player, card, COUNTER_CHARGE) > 0)
	declare_mana_available_hex(player, COLOR_TEST_ANY_COLORED, 1);

  return 0;
}

int card_inkmoth_nexus(int player, int card, event_t event){
/* CARD_ID_INKMOTH_NEXUS	11463
Inkmoth Nexus	""
Land
|T: Add |C to your mana pool.
|1: ~ becomes a 1/1 Blinkmoth artifact creature with flying and infect until end of turn. It's still a land. */
	return manland_normal(player, card, event, MANACOST_X(1));
}

int card_infective_blinkmoth(int player, int card, event_t event){
	return manland_animated(player, card, event, MANACOST_X(1));
}

int card_titan_forge(int player, int card, event_t event ){
/* Titan Forge	|3
 * Artifact
 * |3, |T: Put a charge counter on ~.
 * |T, Remove three charge counters from ~: Put a 9/9 colorless Golem artifact creature token onto the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_COUNTER = 1,
		CHOICE_GOLEM
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, GVC_COUNTERS(COUNTER_CHARGE, 3), NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( count_counters(player, card, COUNTER_CHARGE) < 3 ){
			ai_modifier += (current_turn == player ? 12 : 48);
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Add Charge counter", has_mana_for_activated_ability(player, card, MANACOST_X(3)), 5,
							"Create a 9/9 Golem", count_counters(player, card, COUNTER_CHARGE) >= 3, 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_COUNTER ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
		}
		if( choice == CHOICE_GOLEM ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, GVC_COUNTERS(COUNTER_CHARGE, 3), NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_COUNTER ){
			add_counter(instance->parent_controller, instance->parent_card, COUNTER_CHARGE);
		}
		if( instance->info_slot == CHOICE_GOLEM ){
			token_generation_t token;
			default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_GOLEM, &token);
			token.pow = token.tou = 9;
			generate_token(&token);
		}
	}

	return 0;
}

int card_mortarpod(int player, int card, event_t event){
	/* Mortarpod	|2	0x200381f
	 * Artifact - Equipment
	 * Living weapon
	 * Equipped creature gets +0/+1 and has "Sacrifice this creature: This creature deals 1 damage to any target."
	 * Equip |2 */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_GERM, &token);
		token.action = TOKEN_ACTION_EQUIP;
		generate_token(&token);
	}

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_ABILITIES && affect_me(player, card) ){
			// Mimic the special abilities of the equipped creature.
			int result = get_special_abilities(p, c);
			if( result > 0 ){
				special_abilities(player, card, event, result & ~SP_KEYWORD_LURE, player, card);
			}
			if( check_for_ability(p, c, KEYWORD_INFECT) ){
				event_result |= KEYWORD_INFECT;
			}
		}

		if (event == EVENT_TOUGHNESS && affect_me(p, c) ){
			event_result++;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_EQUIP = 1,
		CHOICE_SAC_AND_DAMAGE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( is_equipping(player, card) ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			target_definition_t td1;
			default_target_definition(p, c, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			if( granted_generic_activated_ability(player, card, p, c, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td1, NULL) ){
				return 1;
			}
		}
		return basic_equipment(player, card, event, 2);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int abils[3] = {0, can_activate_basic_equipment(player, card, event, 2), 0};
		if( is_equipping(player, card) ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			target_definition_t td1;
			default_target_definition(p, c, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			if( granted_generic_activated_ability(player, card, p, c, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0,
													&td1, NULL) )
			{
				abils[2] = 1;
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Equip", 						abils[1], 5,
							"Sac equipped crit & damage", 	abils[2], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 2);
		}
		if( choice == CHOICE_SAC_AND_DAMAGE ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			target_definition_t td1;
			default_target_definition(p, c, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			if( charge_mana_for_activated_ability(p, c, MANACOST0) ){
				if( select_target(player, card, &td1, "Select target creature, player, or planeswalker.", &instance->targets[0]) ){
					instance->number_of_targets++;
					kill_card(p, c, KILL_SACRIFICE);
				}
				else{
					spell_fizzled = 1;
				}
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 2);
		}
		if( instance->info_slot == CHOICE_SAC_AND_DAMAGE ){
			target_definition_t td1;
			default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			if( valid_target(&td1) ){
				damage_target0(player, card, 1);
			}
		}
	}

	return 0;
}

int lead_the_stampede_effect(int player, int card, int amount){
/* Look at the top [amount] cards of your library. You may reveal any number of creature cards from among them and put the revealed
 * cards into your hand. Put the rest on the bottom of your library in any order. */

	int result = MIN(amount, count_deck(player));
	while( amount ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
			this_test.create_minideck = amount;
			this_test.no_shuffle = 1;

			int revealed =  new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
			if( revealed != -1 ){
				amount--;
				result++;
			}
			else{
				break;
			}
	}
	if( amount ){
		put_top_x_on_bottom(player, player, amount);
	}

	return result;
}


int card_lead_the_stampede(int player, int card, event_t event){
/* CARD_ID_LEAD_THE_STAMPEDE	11471
Lead the Stampede	|2|G
Sorcery
Look at the top five cards of your library. You may reveal any number of creature cards from among them and put the revealed cards into your hand.
Put the rest on the bottom of your library in any order. */

	if(event == EVENT_RESOLVE_SPELL ){
		lead_the_stampede_effect(player, card, 5);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_slagstorm(int player, int card, event_t event){
/* CARD_ID_SLAGSTORM	11527
Slagstorm	|1|R|R
Sorcery
Choose one -
* ~ deals 3 damage to each creature.
* ~ deals 3 damage to each player. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_DAMAGE_CREATURES = 1,
		CHOICE_DAMAGE_PLAYERS
	};

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int priority_damage_players = (life[player]-3 > 6) && (life[1-player]-3 < 6) ? 10 : 1;
			priority_damage_players = life[player] > 3 && life[1-player] <= 3 ? 10 : 1;
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"3 damages to all creatures", 1, 5,
								"3 damages to all players", 1, priority_damage_players);

			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_DAMAGE_CREATURES ){
			new_damage_all(player, card, ANYBODY, 3, NDA_ALL_CREATURES, NULL);
		}
		if( instance->info_slot == CHOICE_DAMAGE_PLAYERS ){
			APNAP(p, { damage_player(p, 3, player, card); };);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_septic_rats(int player, int card, event_t event )
{
/* CARD_ID_SEPTIC_RATS	11520
Septic Rats	|1|B|B
Creature - Rat 2/2
Infect (This creature deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.)
Whenever ~ attacks, if defending player is poisoned, it gets +1/+1 until end of turn. */

  if ((xtrigger_condition() == XTRIGGER_ATTACKING || event == EVENT_DECLARE_ATTACKERS)
	  && POISON_COUNTERS(1-player) > 0
	  && declare_attackers_trigger(player, card, event, 0, player, card))
	pump_until_eot(player, card, player, card, 1, 1);

  return 0;
}

int card_accorder_paladin(int player, int card, event_t event ){
/* CARD_ID_ACCORDER_PALADIN	11414
Accorder Paladin	|1|W
Creature - Human Knight 3/1
Battle cry (Whenever this creature attacks, each other attacking creature gets +1/+0 until end of turn.) */

	battle_cry(player, card, event);

	return 0;
}

int card_banishment_decree(int player, int card, event_t event){
/* CARD_ID_BANISHMENT_DECREE	11416
Banishment Decree	|3|W|W
Instant
Put target artifact, creature, or enchantment on top of its owner's library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_CREATURE | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target artifact, creature or enchantment.", 1, NULL);
}

int card_bladed_sentinel(int player, int card, event_t event){
/* CARD_ID_BLADED_SENTINEL	11418
Bladed Sentinel	|4
Artifact Creature - Construct 2/4
|W: ~ gains vigilance until end of turn. */

	return generic_shade(player, card, event, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, SP_KEYWORD_VIGILANCE);
}

int card_blisterstick_shaman(int player, int card, event_t event){
	/* Blisterstick Shaman	|2|R	0x2004490
	 * Creature - Goblin Shaman 2/1
	 * When ~ enters the battlefield, it deals 1 damage to any target. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		if( can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0) ){
			damage_target0(player, card, 1);
		}
		get_card_instance(player, card)->number_of_targets = 0;
	}

	return 0;
}

int card_bonehoard(int player, int card, event_t event ){
/* CARD_ID_BONEHOARD	11423
Bonehoard	|4
Artifact - Equipment
Living weapon (When this Equipment enters the battlefield, create a 0/0 |Sblack Germ creature token, then attach this to it.)
Equipped creature gets +X/+X, where X is the number of creature cards in all graveyards.
Equip |2 */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		int amount = count_graveyard_by_type(HUMAN, TYPE_CREATURE) + count_graveyard_by_type(AI, TYPE_CREATURE);
		if( amount < 1 ){
			ai_modifier-=1000;
		}
	}

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			int amount = count_graveyard_by_type(HUMAN, TYPE_CREATURE) + count_graveyard_by_type(AI, TYPE_CREATURE);
			event_result += amount;
		}
	}

	return living_weapon(player, card, event, 2);
}

int card_brass_squire(int player, int card, event_t event){
/* CARD_ID_BRASS_SQUIRE	11424
Brass Squire	|3
Artifact Creature - Myr 1/3
|T: Attach target Equipment you control to target creature you control. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.required_subtype = SUBTYPE_EQUIPMENT;
	td.allowed_controller = player;
	td.preferred_controller = player;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller = player;
	td1.preferred_controller = player;
	td1.allow_cancel = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_target(&td1) ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_X(3), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(3), 0,
								&td, "Select target Equipment you control.");
		if( spell_fizzled != 1 ){
			if( new_pick_target(&td1, "Select target creature you control.", 1, 1 | GS_LITERAL_PROMPT) ){
				add_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td1, 1) ){
			equip_target_creature(instance->targets[0].player, instance->targets[0].card,
									instance->targets[1].player, instance->targets[1].card);
		}
	}

	return 0;
}

int card_burn_the_impure(int player, int card, event_t event){
/* CARD_ID_BURN_THE_IMPURE	11425
Burn the Impure	|1|R
Instant
~ deals 3 damage to target creature. If that creature has infect, ~ deals 3 damage to that creature's controller. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if (check_for_ability(instance->targets[0].player, instance->targets[0].card, KEYWORD_INFECT)){
				damage_player(instance->targets[0].player, 3, player, card);
			}
			damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_caustic_hound(int player, int card, event_t event){
/* CARD_ID_CAUSTIC_HOUND	11426
Caustic Hound	|5|B
Creature - Hound 4/4
When ~ dies, each player loses 4 life. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		APNAP(p, { lose_life(p, 4); };);
	}

	return 0;
}

int card_chocking_fumes(int player, int card, event_t event){
/* CARD_ID_CHOKING_FUMES	11427
Choking Fumes	|2|W
Instant
Put a -1/-1 counter on each attacking creature. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn != player ? count_attackers(current_turn)*12 : -96);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int count = active_cards_count[current_turn]-1;
		while( count > -1 ){
				if( in_play(current_turn, count) && is_what(current_turn, count, TYPE_CREATURE) &&
					is_attacking(current_turn, count)
				  ){
					add_minus1_minus1_counters(current_turn, count, 1);
				}
				count--;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_concussive_bolt(int player, int card, event_t event){
	/* Concussive Bolt	|3|R|R	0x20044ae
	 * Sorcery
	 * ~ deals 4 damage to target player or planeswalker.
	 * Metalcraft - If you control three or more artifacts, creatures controlled by that player or by that planeswalker's controller can't block this turn. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_phase == PHASE_MAIN1 ? 48 : -24);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
			if( metalcraft(player, card) ){
				pump_subtype_until_eot(player, card, get_card_instance(player, card)->targets[0].player, -1, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
}

int card_consecrated_sphinx(int player, int card, event_t event)
{
  /* Consecrated Sphinx	|4|U|U
   * Creature - Sphinx 4/6
   * Flying
   * Whenever an opponent draws a card, you may draw two cards. */

	if (card_drawn_trigger(player, card, event, 1-player, RESOLVE_TRIGGER_AI(player)))
		draw_cards(player, 2);

	return 0;
}

int card_contested_war_zone(int player, int card, event_t event){
/* Contested War Zone	""
 * Land
 * Whenever a creature deals combat damage to you, that creature's controller gains control of ~.
 * |T: Add |C to your mana pool.
 * |1, |T: Attacking creatures get +1/+0 until end of turn. */


	card_instance_t* damage = combat_damage_being_dealt(event);

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( damage ){
		if( damage->damage_target_player == player && damage->damage_target_card == -1 && (damage->targets[3].player & TYPE_CREATURE) &&
			! damage_is_to_planeswalker(damage) )
		{
			card_instance_t *instance = get_card_instance( player, card );
			if( instance->targets[8].player < 0 ){
				instance->targets[8].player = 0;
			}
			instance->targets[8].player++;
			if( instance->targets[1].player < 0 ){
				instance->targets[1].player = 0;
			}
			if( damage->damage_source_player == 0 ){
				SET_BYTE0(instance->targets[1].player)++;
			}
			else{
				SET_BYTE1(instance->targets[1].player)++;
			}
			give_control_of_self(player, card);
		}
	}

	if( resolve_damage_trigger(player, card, event, DDBM_TRACE_DAMAGED_PLAYERS, player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		int times_damaged[2] = {BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player)};
		instance->targets[1].player = 0;
		APNAP(p,{
					if( times_damaged[p] ){
						gain_control_permanently(p, player, card);
					}
				}
		);
	}

	if( ! IS_GAA_EVENT(event) || event == EVENT_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_PUMP
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		int can_pump = ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL);
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Get mana", 1, 5,
							"Pump attacking creatures", can_pump, 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_PUMP ){
			add_state(player, card, STATE_TAPPED);
			if( ! charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PUMP ){
			int count = active_cards_count[current_turn]-1;
			while( count > -1 ){
					if( in_play(current_turn, count) && is_what(current_turn, count, TYPE_CREATURE) && is_attacking(current_turn, count) ){
							pump_until_eot(player, card, current_turn, count, 1, 0);
					}
					count--;
			}
		}
	}

	return 0;
}

int card_copper_carapace(int player, int card, event_t event){
/* CARD_ID_COPPER_CARAPACE	11431
Copper Carapace	|1
Artifact - Equipment
Equipped creature gets +2/+2 and can't block.
Equip |3 (|3: Attach to target creature you control. Equip only as a sorcery.) */

	return vanilla_equipment(player, card, event, 3, 2, 2, 0, SP_KEYWORD_CANNOT_BLOCK);
}

int card_core_prowler(int player, int card, event_t event){
/* CARD_ID_CORE_PROWLER	11432
Core Prowler	|4
Artifact Creature - Horror 2/2
Infect (This creature deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.)
When ~ dies, proliferate. (You choose any number of permanents and/or players with counters on them, then give each another counter of a kind already there.) */

	if( this_dies_trigger(player, card, event, 2) ){
		proliferate(player, card);
	}

	return 0;
}

int card_corrupted_conscience(int player, int card, event_t event)
{
  /* Corrupted Conscience	|3|U|U
   * Enchantment - Aura
   * Enchant creature
   * You control enchanted creature.
   * Enchanted creature has infect. */

   if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_ABILITIES && affect_me(p, c)  ){
			event_result |= KEYWORD_INFECT;
		}
	}

	return card_control_magic(player, card, event);
}

int card_crush(int player, int card, event_t event){
/* CARD_ID_CRUSH	11435
Crush	|R
Instant
Destroy target noncreature artifact. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.illegal_type = TYPE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target non-creature artifact.", 1, NULL);
}

static int cryptoplasm_legacy(int player, int card, event_t event){

	if (effect_follows_control_of_attachment(player, card, event)){
		return 0;
	}

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){

		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player, c = instance->damage_target_card;

		target_definition_t td;
		default_target_definition(p, c, &td, TYPE_CREATURE);
		td.preferred_controller = ANYBODY;
		td.special = TARGET_SPECIAL_NOT_ME;

		if( can_target(&td) && new_pick_target(&td, "Select another target creature", 1, GS_LITERAL_PROMPT) ){
			cloning_and_verify_legend(p, c, instance->targets[1].player, instance->targets[1].card);
		}
		instance->number_of_targets = 1;
	}

	return 0;
}
int card_cryptoplasm(int player, int card, event_t event){

	/* Cryptoplasm	|1|U|U
	 * Creature - Shapeshifter 2/2
	 * At the beginning of your upkeep, you may have ~ become a copy of another target creature. If you do, ~ gains this ability. */

	if( enters_the_battlefield_as_copy_of_any_creature(player, card, event) )
	{
		set_legacy_image(player, CARD_ID_CRYPTOPLASM, create_targetted_legacy_activate(player, card, cryptoplasm_legacy, player, card));
	}

	return 0;
}

int card_darksteel_plate(int player, int card, event_t event){
/* CARD_ID_DARKSTEEL_PLATE	11437
Darksteel Plate	|3
Artifact - Equipment
Indestructible
Equipped creature has indestructible.
Equip |2 */

	indestructible(player, card, event);

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		indestructible(p, c, event);
	}

	return basic_equipment(player, card, event, 2);
}

int card_decimator_web(int player, int card, event_t event){
/* CARD_ID_DECIMATOR_WEB	11438
Decimator Web	|4
Artifact
|4, |T: Target opponent loses 2 life, gets a poison counter, then puts the top six cards of his or her library into his or her graveyard. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 2);
			poison(instance->targets[0].player, 1);
			mill(instance->targets[0].player, 6);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST_X(4), 0, &td, NULL);
}

int card_distant_memories(int player, int card, event_t event){
/*
Distant Memories |2|U|U
Sorcery
Search your library for a card, exile it, then shuffle your library. Any opponent may have you put that card into your hand.
	If no player does, you draw three cards.
*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_LAND, -1)*8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( deck_ptr[player][0] != 1 ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card.");
			int selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 1, AI_MAX_VALUE, -1, &test);
			int iid = deck_ptr[player][selected];
			reveal_card_iid(player, card, iid);
			rfg_card_in_deck(player, selected);
			int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL
								"Opponent gets that card", 1, 1,
								"Opponent draws 3 cards", 1, 1);
			if( choice == 1 ){
				add_card_to_hand(player, iid);
				remove_card_from_rfg(player, cards_data[iid].id);
			}
			if( choice == 2 ){
				draw_cards(player, 3);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_dross_ripper(int player, int card, event_t event){
/* CARD_ID_DROSS_RIPPER	11440
Dross Ripper	|4
Artifact Creature - Hound 3/3
|2|B: ~ gets +1/+1 until end of turn. */

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_XB(2, 1), 1, 1);
}

int card_fangren_marauder(int player, int card, event_t event){
/* CARD_ID_FANGREN_MARAUDER	11441
Fangren Marauder	|5|G
Creature - Beast 5/5
Whenever an artifact is put into a graveyard from the battlefield, you may gain 5 life. */

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_ARTIFACT, NULL);
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( result ){
		int amount = BYTE0(result);
		gain_life(player, amount*5);
	}

	return 0;
}

int card_frantic_salvage(int player, int card, event_t event){
/*
Frantic Salvage |3|W
Instant
Put any number of target artifact cards from your graveyard on top of your library.
Draw a card.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select any target artifact cards.");

	return spell_puts_up_to_x_target_cards_from_graveyard_on_top_of_deck(player, card, event, player, 10, &this_test, 1);
}

int card_fuel_for_the_cause(int player, int card, event_t event){
/* CARD_ID_FUEL_FOR_THE_CAUSE	11446
Fuel for the Cause	|2|U|U
Instant
Counter target spell, then proliferate.
(You choose any number of permanents and/or players with counters on them, then give each another counter of a kind already there.) */

	if(event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			proliferate(player, card);
			return card_counterspell(player, card, event);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

int card_gnathosaur(int player, int card, event_t event){
	/* Gnathosaur	|4|R|R
	 * Creature - Lizard 5/4
	 * Sacrifice an artifact: ~ gains trample until end of turn. */
	return generic_husk(player, card, event, TYPE_ARTIFACT, 0, 0, KEYWORD_TRAMPLE, 0);
}

int card_gore_vassal(int player, int card, event_t event){
/*
Gore Vassal |2|W
Creature — Hound 2/1
Sacrifice Gore Vassal: Put a -1/-1 counter on target creature. Then if that creature's toughness is 1 or greater, regenerate it.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME | GAA_REGENERATION, MANACOST0, 0, &td, NULL) ){
			if( land_can_be_played & LCBP_REGENERATION ){
				td.required_state = TARGET_STATE_DESTROYED;
				return 99;
			}
		}
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td, NULL) ){
			   return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( land_can_be_played & LCBP_REGENERATION ){
			td.required_state = TARGET_STATE_DESTROYED;
		}
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			if( land_can_be_played & LCBP_REGENERATION ){
				if( instance->targets[0].player == player ){
					ai_modifier += (get_toughness(instance->targets[0].player, instance->targets[0].card) > 1 ? 48 : -48);
				}
				else{
					ai_modifier += (get_toughness(instance->targets[0].player, instance->targets[0].card) <= 1 ? 48 : -48);
				}
			}
			else{
				ai_modifier += (instance->targets[0].player == player ? -48 : 48);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 1);
			if( in_play(instance->targets[0].player, instance->targets[0].card) &&
				get_toughness(instance->targets[0].player, instance->targets[0].card) > 0 )
			{
				regenerate_or_shield(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_gruesome_encore(int player, int card, event_t event){
/* CARD_ID_GRUESOME_ENCORE	11455
Gruesome Encore	|2|B
Sorcery
Put target creature card from an opponent's graveyard onto the battlefield under your control. It gains haste. Exile it at the beginning of the next end step.
If that creature would leave the battlefield, exile it instead of putting it anywhere else. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	if(event == EVENT_RESOLVE_SPELL){
		int selected = validate_target_from_grave_source(player, card, 1-player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, 1-player, selected, REANIMATE_UNEARTH);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_OPP_GRAVE, NULL, NULL, 1, &this_test);
}

int card_gust_skimmer(int player, int card, event_t event){
/* CARD_ID_GUST_SKIMMER	11456
Gust-Skimmer	|2
Artifact Creature - Insect 2/1
|U: ~ gains flying until end of turn. */

	return generic_shade(player, card, event, 0, MANACOST_U(1), 0, 0, KEYWORD_FLYING, 0);
}

int card_hellkite_igniter(int player, int card, event_t event){
/* CARD_ID_HELLKITE_IGNITER	11457
Hellkite Igniter	|5|R|R
Creature - Dragon 5/5
Flying, haste
|1|R: ~ gets +X/+0 until end of turn, where X is the number of artifacts you control. */

	haste(player, card, event);

	int amount = (event == EVENT_RESOLVE_ACTIVATION || event == EVENT_POW_BOOST) ? count_permanents_by_type(player, TYPE_ARTIFACT) : 0;

	return generic_shade(player, card, event, 0, MANACOST_XR(1, 1), amount, 0, 0, 0);
}

int card_horryfing_revelation(int player, int card, event_t event){
/* CARD_ID_HORRIFYING_REVELATION	11461
Horrifying Revelation	|B
Sorcery
Target player discards a card, then puts the top card of his or her library into his or her graveyard. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			discard(instance->targets[0].player, 0, player);
			mill(instance->targets[0].player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_into_the_core(int player, int card, event_t event){
/* CARD_ID_INTO_THE_CORE	11465
Into the Core	|2|R|R
Instant
Exile two target artifacts */

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

int card_kembas_legion(int player, int card, event_t event){
/* CARD_ID_KEMBAS_LEGION	11466
Kemba's Legion	|5|W|W
Creature - Cat Soldier 4/6
Vigilance
~ can block an additional creature each combat for each Equipment attached to ~. */

  vigilance(player, card, event);

  if (trigger_condition == TRIGGER_BLOCKER_CHOSEN)	// Avoid counting if we can
	creature_can_block_additional(player, card, event, equipments_attached_to_me(player, card, EATM_REPORT_TOTAL));

  return 0;
}

int card_kuldotha_flamefiend(int player, int card, event_t event){
	/* Kuldotha Flamefiend	|4|R|R	0x2004526
	 * Creature - Elemental 4/4
	 * When ~ enters the battlefield, you may sacrifice an artifact. If you do, ~ deals 4 damage divided as you choose among any number of targets. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		if( controller_sacrifices_a_permanent(player, card, TYPE_ARTIFACT, 0) ){
			target_and_divide_damage(player, card, NULL, NULL, 4);
		}
		get_card_instance(player, card)->number_of_targets = 0;
	}

	return 0;
}

int card_kuldotha_ringleader(int player, int card, event_t event){
/* CARD_ID_KULDOTHA_RINGLEADER	11470
Kuldotha Ringleader	|4|R
Creature - Giant Berserker 4/4
Battle cry (Whenever this creature attacks, each other attacking creature gets +1/+0 until end of turn.)
~ attacks each turn if able. */

	attack_if_able(player, card, event);

	battle_cry(player, card, event);

	return 0;
}

static int magnetic_mine_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_MAGNETIC_MINE ){
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
						return 0;
					}
				}
				count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, TYPE_ARTIFACT, NULL, GFPC_TRACK_CONTROLLER, 0);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int dead[2] = {BYTE0(result), BYTE1(result)};
			APNAP(p,{ damage_player(p, dead[p]*2, player, card); };);
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

int card_magnetic_mine(int player, int card, event_t event){
/*
Magnetic Mine |4
Artifact
Whenever another artifact is put into a graveyard from the battlefield, Magnetic Mine deals 2 damage to that artifact's controller.
*/
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)magnetic_mine_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &magnetic_mine_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY && ! affect_me(player, card) ){
		count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, TYPE_ARTIFACT, NULL, GFPC_TRACK_CONTROLLER, 0);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int dead[2] = {BYTE0(result), BYTE1(result)};
			APNAP(p,{ damage_player(p, dead[p]*2, player, card); };);
		}
	}

	return 0;
}

int card_masters_call(int player, int card, event_t event){
	/* Master's Call	|2|W
	 * Instant
	 * Put two 1/1 colorless Myr artifact creature tokens onto the battlefield. */

	if(event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_MYR, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_metallic_mastery(int player, int card, event_t event){
/* CARD_ID_METALLIC_MASTERY	11479
Metallic Mastery	|2|R
Sorcery
Gain control of target artifact until end of turn. Untap that artifact. It gains haste until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	return aot_like_spell(player, card, event, &td, "TARGET_ARTIFACT", 0);
}

int card_mirran_mettle(int player, int card, event_t event){
/* CARD_ID_MIRRAN_METTLE	11481
Mirran Mettle	|G
Instant
Target creature gets +2/+2 until end of turn.
Metalcraft - That creature gets +4/+4 until end of turn instead if you control three or more artifacts. */

	return vanilla_instant_pump(player, card, event, ANYBODY, player,
								metalcraft(player, card) ? 4 : 2, metalcraft(player, card) ? 4 : 2, 0, 0);
}

int card_mirran_spy(int player, int card, event_t event){
/* CARD_ID_MIRRAN_SPY	11482
Mirran Spy	|2|U
Creature - Drone 1/3
Flying
Whenever you cast an artifact spell, you may untap target creature. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.preferred_controller = player;

			card_instance_t *instance = get_card_instance(player, card);

			if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
				untap_card(instance->targets[0].player, instance->targets[0].card);
			}
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

static void mirrorworks_set_processing(token_generation_t* token, int card, int number)
{
  /* This prevents the following scenario:
   * 1. Mirrorworks (A) in play.
   * 2. Cast another Mirrorworks (B) (or, as originally formulated in the bug, a Copy Artifact or Sculpting Steel).
   * 3. Mirrorworks A triggers and puts a token Mirrorworks (C) in play.
   * 4. Newly-created Mirrorworks token C triggers on B entering play, since we're still resolving its come-into-play triggers, and creates another token
   *    Mirrorworks (D).
   * 5. Newly-created Mirrorworks token D triggers on B entering play, since we're still resolving its come-into-play triggers, and creates another token
   *    Mirrorworks (E).
   * 6. Repeat until out of mana.
   * See bug #12160. */
  get_card_instance(token->t_player, card)->state |= STATE_PROCESSING;
}

int card_mirrorworks(int player, int card, event_t event)
{
  /* Mirrorworks	|5
   * Artifact
   * Whenever another nontoken artifact enters the battlefield under your control, you may pay |2. If you do, put a token that's a copy of that artifact onto
   * the battlefield. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ARTIFACT, "");
		test.type_flag = F1_NO_TOKEN;
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), TYPE_ARTIFACT, NULL, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		if( has_mana(player, COLOR_ANY, 2) && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 2) ){
			token_generation_t token;
			copy_token_definition(player, card, &token, trigger_cause_controller, trigger_cause);
			token.special_code_on_generation = mirrorworks_set_processing;
			generate_token(&token);
		}
	}

  return 0;
}

int card_mitotic_manipulation(int player, int card, event_t event){
/*
Mitotic Manipulation |1|U|U
Sorcery, 1UU (3)
Look at the top seven cards of your library. You may put one of those cards onto the battlefield if it has the same name as a permanent.
Put the rest on the bottom of your library in any order.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = MIN(7, count_deck(player));
		if( amount ){
			int selected = -1;
			int match = 0;
			if( ! IS_AI(player) ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ANY, "Select a card.");
				test.create_minideck = amount;
				test.no_shuffle = 1;
				selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 1, AI_MAX_CMC, -1, &test);
			}
			else{
				int max_cmc = -1;
				int i;
				for(i=0; i<amount; i++){
					int iid = deck_ptr[player][i];
					//No point of parsing non-permanents or permanents with X costs
					if( is_what(-i, iid, TYPE_PERMANENT) && cards_data[iid].cc[1] != 255 ){
						int csvid = cards_data[iid].id;
						CYCLE_ALL_CARDS(p, c, 	{
													if( in_play(p, c) && is_what(p, c, TYPE_PERMANENT) && get_card_name(p, c) == csvid ){
														goto found;
													}
												};
						);
						found:
						{
							int this_cmc = get_cmc_by_internal_id(iid);
							if( this_cmc > max_cmc ){
								max_cmc = this_cmc;
								selected = i;
							}
						}
					}
				}
			}
			if( selected > -1 ){
				int csvid = cards_data[deck_ptr[player][selected]].id;
				CYCLE_ALL_CARDS(p, c, 	{
											if( in_play(p, c) && is_what(p, c, TYPE_PERMANENT) && get_card_name(p, c) == csvid ){
												match = 1;
											}
										};
				);
			}
			if( match ){
				put_into_play_a_card_from_deck(player, player, selected);
				amount--;
			}
			if( amount ){
				put_top_x_on_bottom(player, player, amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_morbid_plunder(int player, int card, event_t event)
{
  /* Morbid Plunder	|1|B|B
   * Sorcery
   * Return up to two target creature cards from your graveyard to your hand. */
  if (!IS_CASTING(player, card, event))
	return 0;
  test_definition_t test;
  new_default_test_definition(&test, TYPE_CREATURE, "Select up to two target creature cards.");
  return spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(player, card, event, 2, &test, 0);
}

int card_myr_sire(int player, int card, event_t event){
	/* Myr Sire	|2
	 * Artifact Creature - Myr 1/1
	 * When ~ dies, put a 1/1 colorless Myr artifact creature token onto the battlefield. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		generate_token_by_id(player, card, CARD_ID_MYR);
	}

	return 0;
}

int card_myr_turbine(int player, int card, event_t event){
/* Myr Turbine	|5
 * Artifact
 * |T: Put a 1/1 colorless Myr artifact creature token onto the battlefield.
 * |T, Tap five untapped Myr you control: Search your library for a Myr creature card, put it onto the battlefield,
 *		then shuffle your library. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_MYR;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MYR = 1,
		CHOICE_TUTOR
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int priority_tutor = current_turn == player ? 1 : 10;
			int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
								"Create a Myr", 1, 5,
								"Tutor a Myr", target_available(player, card, &td) >= 5, priority_tutor);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( choice == CHOICE_MYR ){
				add_state(player, card, STATE_TAPPED);
			}
			if( choice == CHOICE_TUTOR ){
				if( tapsubtype_ability(player, card, 5, &td) ){
					add_state(player, card, STATE_TAPPED);
				}
				else{
					spell_fizzled = 1;
				}
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_MYR ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_MYR);
		}
		if( instance->info_slot == CHOICE_TUTOR ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a Myr creature card.");
			test.subtype = SUBTYPE_MYR;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
		}
	}

	return 0;
}

int card_myr_welder(int player, int card, event_t event ){
/*
Myr Welder |3
Artifact Creature — Myr 1/4
Imprint — {T}: Exile target artifact card from a graveyard.
Myr Welder has all activated abilities of all cards exiled with it.
*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact card.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER_BOTH_GRAVES) )
		{
			return 1;
		}
		int result = 0;
		int i;
		for(i=2; i<10; i++){
			if( instance->targets[i].player != -1 ){
				card_data_t* card_d = &cards_data[ instance->targets[i].player ];
				CardFunction ptFunction = card_d->code_pointer;
				int this_result = ptFunction(player, card, EVENT_CAN_ACTIVATE );
				if( this_result ){
					result = 1;
					if( this_result == 99 ){
						return 99;
					}
				}
			}
			if( instance->targets[i].card != -1 ){
				card_data_t* card_d = &cards_data[ instance->targets[i].card ];
				CardFunction ptFunction = card_d->code_pointer;
				int this_result = ptFunction(player, card, EVENT_CAN_ACTIVATE );
				if( this_result ){
					result = 1;
					if( this_result == 99 ){
						return 99;
					}
				}
			}
		}
		return result;
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int activable[16];
		int ac = 0;
		int i;
		for(i=2; i<10; i++){
			if( instance->targets[i].player != -1 ){
				card_data_t* card_d = &cards_data[ instance->targets[i].player ];
				CardFunction ptFunction = card_d->code_pointer;
				int this_result = ptFunction(player, card, EVENT_CAN_ACTIVATE );
				if( this_result ){
					activable[ac] = instance->targets[i].player;
					ac++;
				}
			}
			if( instance->targets[i].card != -1 ){
				card_data_t* card_d = &cards_data[ instance->targets[i].card ];
				CardFunction ptFunction = card_d->code_pointer;
				int this_result = ptFunction(player, card, EVENT_CAN_ACTIVATE );
				if( this_result ){
					activable[ac] = instance->targets[i].player;
					ac++;
				}
			}
		}
		char mgs[16][100];
		for(i=0; i<ac; i++){
			scnprintf(mgs[i], 100, "%s", cards_ptr[cards_data[activable[i]].id]->name);
		}
		int can_imprint = generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0,
																		NULL, NULL,0, &test, GS_GRAVE_RECYCLER_BOTH_GRAVES);
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL,
							"Imprint an artifact card", can_imprint, 10,
							mgs[0], ac ? 1 : 0, 1,
							mgs[1], ac >= 2 ? 1 : 0, 1,
							mgs[2], ac >= 3 ? 1 : 0, 1,
							mgs[3], ac >= 4 ? 1 : 0, 1,
							mgs[4], ac >= 5 ? 1 : 0, 1,
							mgs[5], ac >= 6 ? 1 : 0, 1,
							mgs[6], ac >= 7 ? 1 : 0, 1,
							mgs[7], ac >= 8 ? 1 : 0, 1,
							mgs[8], ac >= 9 ? 1 : 0, 1,
							mgs[9], ac >= 10 ? 1 : 0, 1,
							mgs[10], ac >= 11 ? 1 : 0, 1,
							mgs[11], ac >= 12 ? 1 : 0, 1,
							mgs[12], ac >= 13 ? 1 : 0, 1,
							mgs[13], ac >= 14 ? 1 : 0, 1,
							mgs[14], ac >= 15 ? 1 : 0, 1,
							mgs[15], ac == 16 ? 1 : 0, 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == 1 ){
			return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST0, 0,
																	NULL, NULL,0, &test, GS_GRAVE_RECYCLER_BOTH_GRAVES);
		}
		int iid = activable[choice-2];
		card_data_t* card_d = &cards_data[ iid ];
		CardFunction ptFunction = card_d->code_pointer;
		if( ptFunction(player, card, EVENT_ACTIVATE) ){
			instance->info_slot = iid;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 0 ){
			int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
			if( selected != -1 ){
				const int *grave = get_grave(instance->targets[0].player);
				card_instance_t *parent = get_card_instance(player, instance->parent_card);
				int pos = 2;
				while( pos < 10 ){
						if( parent->targets[pos].player == -1 ){
							parent->targets[pos].player = grave[selected];
							create_card_name_legacy(player, instance->parent_card, cards_data[grave[selected]].id);
							break;
						}
						if( parent->targets[pos].card == -1 ){
							parent->targets[pos].card = grave[selected];
							create_card_name_legacy(player, instance->parent_card, cards_data[grave[selected]].id);
							break;
						}
						pos++;
				}
				rfg_card_from_grave(instance->targets[0].player, selected);
			}
		}
		else{
			CardFunction ptFunction = cards_data[instance->info_slot].code_pointer;
			ptFunction(player, card, EVENT_RESOLVE_ACTIVATION);
		}
	}

	return 0;
}

int card_nested_ghoul(int player, int card, event_t event ){
	/* Nested Ghoul	|3|B|B
	 * Creature - Zombie Warrior 4/2
	 * Whenever a source deals damage to ~, put a 2/2 |Sblack Zombie creature token onto the battlefield. */


	card_instance_t *damage = damage_being_dealt(event);
	if( damage ){
		if( damage->damage_target_player == player && damage->damage_target_card == card ){
			card_instance_t *instance = get_card_instance(player, card);
			if( instance->targets[8].player < 0 ){
				instance->targets[8].player = 0;
			}
			instance->targets[8].player++;
		}
	}

	int result = resolve_damage_trigger(player, card, event, 0, player, card);
	if( result ){
		generate_tokens_by_id(player, card, CARD_ID_ZOMBIE, result);
	}

	return 0;
}

int card_neurok_commando(int player, int card, event_t event ){
/* CARD_ID_NEUROK_COMMANDO	11491
Neurok Commando	|1|U|U
Creature - Human Rogue 2/1
Shroud (This creature can't be the target of spells or abilities.)
Whenever ~ deals combat damage to a player, you may draw a card. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, RESOLVE_TRIGGER_AI(player) | DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		draw_cards(player, amount);
	}

	return 0;
}

int card_oculus(int player, int card, event_t event ){
/* CARD_ID_OCULUS	11492
Oculus	|1|U
Creature - Homunculus 1/1
When ~ dies, you may draw a card. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		draw_cards(player, 1);
	}
	return 0;
}

int card_peace_strider(int player, int card, event_t event ){
/* CARD_ID_PEACE_STRIDER	11494
Peace Strider	|4
Artifact Creature - Construct 3/3
When ~ enters the battlefield, you gain 3 life. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 3);
	}

	return 0;
}

int card_phyresis(int player, int card, event_t event ){
/* CARD_ID_PHYRESIS	11495
Phyresis	|1|B
Enchantment - Aura
Enchant creature
Enchanted creature has infect. (It deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.)*/

	return generic_aura(player, card, event, player, 0, 0, KEYWORD_INFECT, 0, 0, 0, 0);
}

int card_phyrexian_rebirth(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_REBIRTH	11500
Phyrexian Rebirth	|4|W|W
Sorcery
Destroy all creatures, then create an X/X colorless Horror artifact creature token, where X is the number of creatures destroyed this way. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);

		int amount = 0;
		APNAP(p, { amount += new_manipulate_all(player, card, p, &test, KILL_DESTROY); };);

		token_generation_t token;
		default_token_definition(player, card, CARD_ID_HORROR, &token);
		token.pow = token.tou = amount;
		token.action = TOKEN_ACTION_CONVERT_INTO_ARTIFACT;
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_pierce_strider(int player, int card, event_t event ){
/* CARD_ID_PIERCE_STRIDER	11503
Pierce Strider	|4
Artifact Creature - Construct 3/3
When ~ enters the battlefield, target opponent loses 3 life. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		if( would_validate_arbitrary_target(&td, 1-player, -1) ){
			lose_life(1-player, 3);
		}
	}

	return 0;
}

int card_piston_sledge(int player, int card, event_t event)
{
/*
Piston Sledge |3
Artifact — Equipment
When Piston Sledge enters the battlefield, attach it to target creature you control.
Equipped creature gets +3/+1.
Equip—Sacrifice an artifact.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.allow_cancel = 0;
		card_instance_t* instance = get_card_instance(player, card);
		if( can_target(&td) && new_pick_target(&td, "Select target creature you control.", 0, GS_LITERAL_PROMPT) ){
			ai_modifier += (! is_sick(instance->targets[0].player, instance->targets[0].card) &&
							! check_for_ability(instance->targets[0].player, instance->targets[0].card, KEYWORD_DEFENDER) ? 42 : -42);
			equip_target_creature(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return altar_equipment(player, card, event, TYPE_ARTIFACT, 3, 1, 0);
}

int card_pistus_strike(int player, int card, event_t event){
/* CARD_ID_PISTUS_STRIKE	11505
Pistus Strike	|2|G
Instant
Destroy target creature with flying. Its controller gets a poison counter. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			poison(instance->targets[0].player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature with flying.", 1, NULL);
}

int card_plaguemaw_beast(int player, int card, event_t event){
/* CARD_ID_PLAGUEMAW_BEAST	11507
Plaguemaw Beast	|3|G|G
Creature - Beast 4/3
|T, Sacrifice a creature: Proliferate. (You choose any number of permanents and/or players with counters on them, then give each another counter of a kind already there.)*/

	if( event == EVENT_RESOLVE_ACTIVATION ){
		proliferate(player, card);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL);
}

int card_praetors_counsel(int player, int card, event_t event){
/* CARD_ID_PRAETORS_COUNSEL	11508
Praetor's Counsel	|5|G|G|G
Sorcery
Return all cards from your graveyard to your hand. Exile ~. You have no maximum hand size for the rest of the game. */

	if(event == EVENT_RESOLVE_SPELL ){
		int cg = count_graveyard(player)-1;
		const int *grave = get_grave(player);
		while( cg > -1 ){
				add_card_to_hand(player, grave[cg]);
				remove_card_from_grave(player, cg);
				cg--;
		}
		create_legacy_effect(player, card, &card_spellbook);
		kill_card(player, card, KILL_EXILE);
	}

	return basic_spell(player, card, event);
}

int card_psychosis_crawler(int player, int card, event_t event){

	/* Psychosis Crawler	|5
	 * Artifact Creature - Horror 100/100
	 * ~'s power and toughness are each equal to the number of cards in your hand.
	 * Whenever you draw a card, each opponent loses 1 life. */

	if (card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY)){
		lose_life(1-player, 1);
	}

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && !is_humiliated(player, card) && player != -1){
		event_result += hand_count[player];
	}

	return 0;
}

int card_quicksilve_geyser(int player, int card, event_t event){
	/*
	  Quicksilver Geyser |4|U
	  Instant
	  Return up to two target nonland permanents to their owners' hands.
	*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_type = TYPE_LAND;
	td.allow_cancel = 2; //"Done" button.

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				bounce_permanent(instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET | GS_LITERAL_PROMPT, &td, "Select target nonland permanent.", 2, NULL);
}

int card_rally_the_forces(int player, int card, event_t event){
/* CARD_ID_RALLY_THE_FORCES	11513
Rally the Forces	|2|R
Instant
Attacking creatures get +1/+0 and gain first strike until end of turn. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "");
		test.state = STATE_ATTACKING;
		pump_creatures_until_eot(player, card, current_turn, 0, 1,0, KEYWORD_FIRST_STRIKE,0, &test);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_rusted_slasher(int player, int card, event_t event)
{
  /* Rusted Slasher	|4
   * Artifact Creature - Horror 4/1
   * Sacrifice an artifact: Regenerate ~. */

  if (!(land_can_be_played & LCBP_REGENERATION))
	return 0;

  if (!IS_ACTIVATING(event))
	return 0;

  card_instance_t* instance = get_card_instance(player, card);

  test_definition_t test;
  new_default_test_definition(&test, TYPE_ARTIFACT, "");
  if (IS_AI(player))
	test.not_me = 1;

  if (event == EVENT_CAN_ACTIVATE && CAN_ACTIVATE0(player, card) && new_can_sacrifice_as_cost(player, card, &test))
	return can_regenerate(player, card);

  if (event == EVENT_ACTIVATE && charge_mana_for_activated_ability(player, card, MANACOST0))
	new_sacrifice(player, card, player, SAC_AS_COST, &test);

  if (event == EVENT_RESOLVE_ACTIVATION && can_regenerate(instance->parent_controller, instance->parent_card))
	regenerate_target(instance->parent_controller, instance->parent_card);

  return 0;
}

int card_serum_raker(int player, int card, event_t event){
/* CARD_ID_SERUM_RAKER	11521
Serum Raker	|2|U|U
Creature - Drake 3/2
Flying
When ~ dies, each player discards a card. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		APNAP(p, {discard(p, 0, player);};);
	}

	return 0;
}

int card_shimmer_myr(int player, int card, event_t event){
/* CARD_ID_SHIMMER_MYR	11522
Shimmer Myr	|3
Artifact Creature - Myr 2/2
Flash
You may cast artifact spells as though they had flash. */

	if( ! IS_GAA_EVENT(event) ){
		return flash(player, card, event);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) && is_what(player, i, TYPE_ARTIFACT) &&
				has_mana_to_cast_iid(player, event, get_original_internal_card_id(player, i)) )
			{
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int pc[2][hand_count[player]];
		int pcc = 0;
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) && is_what(player, i, TYPE_ARTIFACT) &&
				has_mana_to_cast_iid(player, event, get_original_internal_card_id(player, i)) )
			{
				pc[0][pcc] = get_original_internal_card_id(player, i);
				pc[1][pcc] = i;
				pcc++;
			}
		}
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact card to play.");
		int selected = select_card_from_zone(player, player, pc[0], pcc, 0, AI_MAX_VALUE, -1, &test);
		if( selected == -1 ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_from_id(player, pc[1][selected], event, get_id(player, pc[1][selected])) ){
			play_card_in_hand_for_free(player, pc[1][selected]);
			cant_be_responded_to = 1;	// The spell will be respondable to, but this (fake) activation won't
		}
		else{
			spell_fizzled = 1;
		}
	}

	return 0;
}

int card_shriekhorn(int player, int card, event_t event){
/* Shriekhorn	|1
 * Artifact
 * ~ enters the battlefield with three charge counters on it.
 * |T, Remove a charge counter from ~: Target player puts the top two cards of his or her library into his or her graveyard. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, 3);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			mill(instance->targets[0].player, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_CHARGE),
									&td, "TARGET_PLAYER");
}

int card_skinwing(int player, int card, event_t event ){
/* CARD_ID_SKINWING	11526
Skinwing	|4
Artifact - Equipment
Living weapon (When this Equipment enters the battlefield, create a 0/0 |Sblack Germ creature token, then attach this to it.)
Equipped creature gets +2/+2 and has flying.
Equip |6 */

	if( is_equipping(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES) && affect_me(p, c) ){
			if( event == EVENT_ABILITIES ){
				event_result |= KEYWORD_FLYING;
			}
			else{
				event_result += 2;
			}
		}
	}

	return living_weapon(player, card, event, 6);
}

int card_spin_engine(int player, int card, event_t event){
/* CARD_ID_SPIN_ENGINE	11529
Spin Engine	|3
Artifact Creature - Construct 3/1
|R: Target creature can't block ~ this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.allowed_controller = td.preferred_controller = 1-player;

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (current_turn == player && current_phase < PHASE_DECLARE_BLOCKERS ? 24 : -24);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);
			int p = instance->parent_controller, c = instance->parent_card;

			creature1_cant_block_creature2_until_eot(p, c, instance->targets[0].player, instance->targets[0].card, p, c);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_CREATURE");
}

int card_spiraling_duelist(int player, int card, event_t event ){
/* CARD_ID_SPIRALING_DUELIST	11531
Spiraling Duelist	|2|R|R
Creature - Human Berserker 3/1
Metalcraft - ~ has double strike as long as you control three or more artifacts. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card) && metalcraft(player, card) ){
		event_result |= KEYWORD_DOUBLE_STRIKE;
	}

	return 0;
}

int card_spire_serpent(int player, int card, event_t event ){
/* CARD_ID_SPIRE_SERPENT	11532
Spire Serpent	|4|U
Creature - Serpent 3/5
Defender
Metalcraft - As long as you control three or more artifacts, ~ gets +2/+2 and can attack as though it didn't have defender. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
		if( ! is_humiliated(player, card) && metalcraft(player, card) ){
			event_result += 2;
		}
	}

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card)){
		if( metalcraft(player, card) ){
			add_status(player, card, STATUS_WALL_CAN_ATTACK);
		}
		else{
			remove_status(player, card, STATUS_WALL_CAN_ATTACK);
		}
	}

	return 0;
}

int card_spread_the_sickness(int player, int card, event_t event){
/* CARD_ID_SPREAD_THE_SICKNESS	11533
Spread the Sickness	|4|B
Sorcery
Destroy target creature, then proliferate.
(You choose any number of permanents and/or players with counters on them, then give each another counter of a kind already there.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			proliferate(player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_strandwalker(int player, int card, event_t event ){
/* CARD_ID_STRANDWALKER	11535
Strandwalker	|5
Artifact - Equipment
Living weapon (When this Equipment enters the battlefield, create a 0/0 |Sblack Germ creature token, then attach this to it.)
Equipped creature gets +2/+4 and has reach.
Equip |4 */

	if( is_equipping(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES) && affect_me(p, c) ){
			return vanilla_equipment(player, card, event, 4, 2, 4, KEYWORD_REACH, 0);
		}
	}

	return living_weapon(player, card, event, 4);
}

int card_tangle_hulk(int player, int card, event_t event){
/* CARD_ID_TANGLE_HULK	11537
Tangle Hulk	|5
Artifact Creature - Beast 5/3
|2|G: Regenerate ~. */

	return regeneration(player, card, event, MANACOST_XG(2, 1));
}

int card_training_drone(int player, int card, event_t event)
{
/* CARD_ID_TRAINING_DRONE	11544
Training Drone	|3
Artifact Creature - Drone 4/4
~ can't attack or block unless it's equipped. */

  if ((event == EVENT_ATTACK_LEGALITY || event == EVENT_BLOCK_LEGALITY) && affect_me(player, card) && ! is_humiliated(player, card) &&
	!equipments_attached_to_me(player, card, EATM_REPORT_TOTAL))
	event_result = 1;

  return 0;
}

int card_unnatural_predation(int player, int card, event_t event){
/* CARD_ID_UNNATURAL_PREDATION	11547
Unnatural Predation	|G
Instant
Target creature gets +1/+1 and gains trample until end of turn. */

	return vanilla_instant_pump(player, card, event, ANYBODY, player, 1, 1, KEYWORD_TRAMPLE, 0);
}

int card_vedalken_anatomist(int player, int card, event_t event){
/* CARD_ID_VEDALKEN_ANATOMIST	11548
Vedalken Anatomist	|2|U
Creature - Vedalken Wizard 1/2
|2|U, |T: Put a -1/-1 counter on target creature. You may tap or untap that creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 1);
			twiddle(player, card, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_XU(2, 1), 0, &td, "TARGET_CREATURE");
}

int card_vedalken_infuser(int player, int card, event_t event){

	/* Vedalken Infuser	|3|U
	 * Creature - Vedalken Wizard 1/4
	 * At the beginning of your upkeep, you may put a charge counter on target artifact. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT );
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_ARTIFACT", 0, 0) ){
			ai_modifier += (get_counter_type_by_id(get_id(instance->targets[0].player, instance->targets[0].card)) == COUNTER_CHARGE ?
							48 : -48);
			add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_CHARGE);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_viridian_claw(int player, int card, event_t event){
/* CARD_ID_VIRIDIAN_CLAW	11551
Viridian Claw	|2
Artifact - Equipment
Equipped creature gets +1/+0 and has first strike.
Equip |1 */

	return vanilla_equipment(player, card, event, 1, 1, 0, KEYWORD_FIRST_STRIKE, 0);
}

static int virulent_wound_legacy(int player, int card, event_t event ){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(p, c) ){
			if( is_this_dying(p, c) ){
				instance->targets[11].player = 66;
				instance->targets[0].player = instance->damage_target_player;
				instance->damage_target_player = instance->damage_target_card = -1;
			}
		}
	}

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance( player, card );
		poison(instance->targets[0].player, 1);
		kill_card(player, card, KILL_EXILE);
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_virulent_wound(int player, int card, event_t event){
/*
Virulent Wound |B
Instant
Put a -1/-1 counter on target creature. When that creature dies this turn, its controller gets a poison counter.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &virulent_wound_legacy,
											instance->targets[0].player, instance->targets[0].card);
			add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

static int is_nonland_permanent_by_iid(int iid, int unused1, int unused2, int unused3){
	if( is_what(-1, iid, TYPE_PERMANENT) && ! is_what(-1, iid, TYPE_LAND) ){
		return 1;
	}
	return 0;
}

int card_phyrexian_revoker(int player, int card, event_t event){
/*
Phyrexian Revoker |2
Artifact Creature — Horror 2/1
As Phyrexian Revoker enters the battlefield, name a nonland card.
Activated abilities of sources with the chosen name can't be activated.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT );
		td.illegal_type = TYPE_LAND;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		int csvid = -1;
		int choice = IS_AI(player) && can_target(&td) ? 1 : 2;
		if( ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
							"Select a card in play", 1, 1,
							"Choose a card from a list", 1, 1);
		}
		if( choice == 1 ){
			if( new_pick_target(&td, "Select a nonland permanent in play.", 0, GS_LITERAL_PROMPT) ){
				csvid = get_card_name(instance->targets[0].player, instance->targets[0].card);
			}
			instance->number_of_targets = 0;
		}
		if( choice == 2 ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Select a nonland card.");
			test.type_flag = DOESNT_MATCH;
			if( IS_AI(player) ){
				test.special_selection_function = &is_nonland_permanent_by_iid;
				csvid = ai_name_a_random_csvid(player, card, 1-player, 0, &test);
			}
			if( ! IS_AI(player) ){
				csvid = new_card_from_list(player, 3, &test);
			}
		}
		instance->info_slot = csvid;
		create_card_name_legacy(player, card, csvid);
		APNAP(p,{
					int c;
					for(c=0; c<active_cards_count[p]; c++){
						if( in_play(p, c) && get_card_name(p, c) == csvid ){
							disable_all_activated_abilities(p, c, ALL_AB_DISABLE);
						}
					}
				}
		);
	}

	if( event == EVENT_CAST_SPELL && ! affect_me(player, card) ){
		if( get_card_name(affected_card_controller, affected_card) == get_card_instance(player, card)->info_slot ){
			disable_all_activated_abilities(affected_card_controller, affected_card, ALL_AB_DISABLE);
		}
	}

	if( leaves_play(player, card, event) ){
		int csvid = get_card_instance(player, card)->info_slot;
		APNAP(p,{
					int c;
					for(c=0; c<active_cards_count[p]; c++){
						if( in_play(p, c) && get_card_name(p, c) == csvid ){
							disable_all_activated_abilities(p, c, ALL_AB_ENABLE);
						}
					}
				}
		);
	}

	return 0;
}


