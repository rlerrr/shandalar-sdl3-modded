#include "manalink.h"

// ---- General functions

static int smiths(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);

		return new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test);
	}

	return 0;
}

static int som_spellbombs(int player, int card, event_t event, int manacolor){

	if( graveyard_from_play(player, card, event) && has_mana(player, manacolor, 1) ){
		charge_mana(player, manacolor, 1);
		if( spell_fizzled != 1 ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

static int trigon(int player, int card, event_t event, color_t color, const char* dialog_prompt, target_definition_t* td, const char* target_prompt)
{
  /* Trigon of [Something]	|[cost]
   * Artifact
   * ~ enters the battlefield with three charge counters on it.
   * |C|C, |T: Put a charge counter on ~.
   * |2, |T, Remove a charge counter from ~: [Do something]. */

  enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, 3);

  if (IS_ACTIVATING(event))
	{
	  if (event == EVENT_CAN_ACTIVATE && !(CAN_TAP(player, card) && can_use_activated_abilities(player, card)))
		return 0;

	  enum
	  {
		CHOICE_ACTIVATE = 1,
		CHOICE_CHARGE,
	  } choice= DIALOG(player, card, event,
					   dialog_prompt,
							count_counters(player, card, COUNTER_CHARGE) && (!td || can_target(td)),
							2,
							DLG_MANA(MANACOST_X(2)),
					   "Add a charge counter",
							1,
							current_turn == 1-player && current_phase == PHASE_DISCARD ? 3 : 1,
							DLG_MANA(MANACOST_CLR(color, 2)));

	  if (event == EVENT_CAN_ACTIVATE)
		return choice ? 1 : 0;
	  else if (event == EVENT_ACTIVATE)
		switch (choice)
		  {
			case CHOICE_ACTIVATE:
			  if (td)
				{
				  get_card_instance(player, card)->number_of_targets = 0;
				  if (!pick_target(td, target_prompt))
					break;
				}
			  remove_counter(player, card, COUNTER_CHARGE);
			  tap_card(player, card);
			  break;

			case CHOICE_CHARGE:
			  tap_card(player, card);
			  break;
		  }
	  else	// EVENT_RESOLVE_ACTIVATION
		switch (choice)
		  {
			case CHOICE_ACTIVATE:
			  return 2;

			case CHOICE_CHARGE:
			  add_counter(player, card, COUNTER_CHARGE);
			  break;
		  }
	}

  if (event == EVENT_SHOULD_AI_PLAY)
	ai_modifier += (player == AI ? 8 : -8) * count_counters(player, card, COUNTER_CHARGE);

  return 0;
}

int metalcraft(int player, int card){
	if( !is_humiliated(player, card) && count_permanents_by_type(player, TYPE_ARTIFACT) > 2 ){
		return 1;
	}
	return 0;
}

void proliferate_auto_single_permanent_or_player(int player, int card, int p, int c ){
	if( c != -1 && count_counters(p, c, -1) > 0 ){
		counter_t counter_type = choose_existing_counter_type(player, player, card, p, c, CECT_AI_CAN_CANCEL | CECT_AI_NO_DIALOG, -1, -1);
		if( counter_type != COUNTER_invalid )
		{
			add_counter(p, c, counter_type);
		}
	}
	else{
		if ( p == 1-player && POISON_COUNTERS(p) > 0 ){
			poison(p, 1);
		}
		if( p == player ){
			int exp = EXPERIENCE_COUNTERS(p);
			int en = ENERGY_COUNTERS(p);
			if( exp && en ){
				if( EXPERIENCE_COUNTERS(p) > ENERGY_COUNTERS(p) ){
					add_energy(p, 1);
				}
				else{
					add_experience(p, 1);
				}
			}
			else{
				if( exp ){
					add_experience(p, 1);
				}
				if( en ){
					add_energy(p, 1);
				}
			}
		}
	}
}

void proliferate_manual_single_permanent_or_player(int player, int card, int p, int c, int add_untargettable){
	if( c != -1)
	{
		counter_t counter_type = choose_existing_counter_type(player, player, card, p, c, CECT_HUMAN_CAN_CANCEL, -1, -1);
		if (counter_type == COUNTER_invalid)
			cancel = 0;
		else
		{
			if( add_untargettable ){
				state_untargettable(p, c, 1);
			}
			add_counter(p, c, counter_type);
		}
	}
	else{
		int mode = 0;
		int i;
		for(i=0; i<=PCOUNTER_MAX_PCOUNTERS; i++){
			mode |= (count_player_counters(player, i) ? 1<<i : 0);
		}
		if( mode ){
			int counters_to_add = DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_OMIT_ILLEGAL,
						"Poison counter", (mode & (1<<0)), 1,
						"Experience counter", (mode & (1<<1)), 1,
						"Energy counter", (mode & (1<<2)), 1);
			add_player_counters(player, counters_to_add-1, 1); //Since the first PLAYER_COUNTER is defined as "0"
		}
	}
}

void proliferate(int player, int card)
{
// You choose any number of permanents and/or players with counters on them, then give each another counter of a kind already there.

	enum{
		CHOICE_AUTO = 1,
		CHOICE_MANUAL
	};

	int choice = CHOICE_AUTO;
	if( player != AI ){
		choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_NO_CANCEL,
						"Automatic", 1, 1,
						"Manual", 1, 1);

	}
	if( choice == CHOICE_AUTO )
	{
		APNAP(p,{
					proliferate_auto_single_permanent_or_player(player, card, p, -1);
					int c;
					for(c=active_cards_count[p]-1; c>-1; c--){
						if (in_play(p, c) && is_what(p, c, TYPE_PERMANENT) ){
							proliferate_auto_single_permanent_or_player(player, card, p, c);
						}
					}
				};
		);
	}
	if( choice == CHOICE_MANUAL )
	{
		target_definition_t td;
		base_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 2;	// Done button, no cancel
		td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
		td.extra = 0;
		SET_BYTE0(td.extra) = COUNTER_invalid;
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		if( ! count_player_counters(0, PCOUNTER_invalid) || ! count_player_counters(1, PCOUNTER_invalid) ){
			td.zone = TARGET_ZONE_IN_PLAY;	// no targettable players
		}

		int chosen_players[2] = {0, 0};

		target_t tgt;

		while (can_target(&td))
		{
			load_text(0, "PROLIFERATE");
			if( !select_target(player, card-1000, &td, text_lines[0], &tgt) ){
				break;
			}

			if (tgt.card != -1)
			{
				proliferate_manual_single_permanent_or_player(player, card, tgt.player, tgt.card, 1);
			}
			else{
				if( chosen_players[tgt.player] != 1 ){
					proliferate_manual_single_permanent_or_player(player, card, tgt.player, tgt.card, 0);
					chosen_players[tgt.player] = 1;
					if( chosen_players[1 - tgt.player] || ! count_player_counters(1 - tgt.player, PCOUNTER_invalid) ){
						td.zone = TARGET_ZONE_IN_PLAY;	// no more targettable chosen_players
					}
				}
			}
		}
		remove_untargettable_from_all();
	}
}

static void ai_mod_for_proliferate(int player, int mana_spent){
	int mod = 0;
	int mul = 8-mana_spent;
	CYCLE_ALL_CARDS(p, c,	{
								if( in_play(p, c) ){
									if( p == player ){
										if( count_counters(p, c, COUNTER_CHARGE) ){
											mod += (mul * 2);
										}
										if( count_counters(p, c, COUNTER_P1_P1) ){
											mod += (mul * 3);
										}
										if( count_counters(p, c, COUNTER_LOYALTY) ){
											mod += (mul * 4);
										}
									}
									else{
										if( count_counters(p, c, COUNTER_M1_M1) ){
											mod += (mul * 3);
										}
									}
								}
							}
	);
	mod += (POISON_COUNTERS(1-player) ? 5 * mod : 0);
	mod += (EXPERIENCE_COUNTERS(player) ? 4 * mod : 0);
	mod += (ENERGY_COUNTERS(player) ? 3 * mod : 0);
	ai_modifier += mod;
}

static int generic_artifact_with_proliferate(int player, int card, event_t event, int manacost, int times_proliferate){

	if( event == EVENT_ACTIVATE ){
		int i;
		for(i=0; i<times_proliferate; i++){
			if( i == 0 ){
				ai_mod_for_proliferate(player, manacost);
			}
			else{
				ai_mod_for_proliferate(player, 0);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int i;
		for (i = 0; i < times_proliferate; ++i){
			proliferate(player, card);
		}
	}


	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(manacost), 0, NULL, NULL);
}

// ------- Cards

int card_wurmcoil_engine(int player, int card, event_t event){
	/* Wurmcoil Engine	|6
	 * Artifact Creature - Wurm 6/6
	 * Deathtouch, lifelink
	 * When ~ dies, put a 3/3 colorless Wurm artifact creature token with deathtouch and a 3/3 colorless Wurm artifact creature token with lifelink onto the battlefield. */

	deathtouch(player, card, event);

	lifelink(player, card, event);

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_WURM, &token);
		token.pow = token.tou = 3;
		token.color_forced = COLOR_TEST_COLORLESS;
		token.action = TOKEN_ACTION_CONVERT_INTO_ARTIFACT;
		token.s_key_plus = SP_KEYWORD_DEATHTOUCH;
		generate_token(&token);

		token.s_key_plus = SP_KEYWORD_LIFELINK;
		generate_token(&token);
	}

	return 0;
}

int card_myr_battlesphere(int player, int card, event_t event)
{
	/* Myr Battlesphere	|7	0x2002884
	 * Artifact Creature - Myr Construct 4/7
	 * When ~ enters the battlefield, create four 1/1 colorless Myr artifact creature tokens.
	 * Whenever ~ attacks, you may tap X untapped Myr you control. If you do, ~ gets +X/+0 until end of turn and deals X damage to the player or planeswalker it's attacking. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
		generate_tokens_by_id(player, card, CARD_ID_MYR, 4);

  if (declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), player, card))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_PERMANENT);
	  td.allowed_controller = player;
	  td.preferred_controller = player;
	  td.required_subtype = SUBTYPE_MYR;
	  td.illegal_state = TARGET_STATE_TAPPED;
	  td.illegal_abilities = 0;
	  td.allow_cancel = 3;

	  char marked[2][151] = {{0}};
	  int num = mark_up_to_n_targets_noload(&td, "Select an untapped Myr you control.", -1, marked);
	  if (num > 0)
		{
		  int attacking = planeswalker_its_attacking(player, card);

		  int c;
		  for (c = 0; c < active_cards_count[player]; ++c)
			if (marked[player][c] && in_play(player, c))	// Tapping one of the previous Myr might've destroyed this one.
			  tap_card(player, c);

		  if (attacking != -2)
			damage_creature(1-current_turn, attacking, num, player, card);

		  if (in_play(player, card))	// Tapping one of the Myr might've destroyed the Battlesphere.  Stupid instantaneous Manalink triggers.
			pump_until_eot(player, card, player, card, num,0);
		}
	}

  return 0;
}

int card_ezuris_brigade(int player, int card, event_t event){
	/* Ezuri's Brigade	|2|G|G
	 * Creature - Elf Warrior 4/4
	 * Metalcraft - As long as you control three or more artifacts, ~ gets +4/+4 and has trample. */

	if( in_play(player, card) && affect_me(player, card) && metalcraft(player, card) ){
		if( event == EVENT_POWER || event == EVENT_TOUGHNESS ){
			event_result+=4;
		}
		else if(event == EVENT_ABILITIES ){
			event_result |= KEYWORD_TRAMPLE;
		}
	}
	return 0;
}

int card_koth_of_the_hammer(int player, int card, event_t event){
	/* Koth of the Hammer	|2|R|R	0x20028c5
	 * Legendary Planeswalker - Koth (3)
	 * +1: Untap target |H2Mountain. It becomes a 4/4 |Sred Elemental creature until end of turn. It's still a land.
	 * -2: Add |R for each |H2Mountain you control.
	 * -5: You get an emblem with "|H1Mountains you control have '|T: This land deals 1 damage to any target.'" */
	if (IS_ACTIVATING(event)){

		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.required_subtype = get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN);
		td.preferred_controller = player;

		int priority_mana = 0;
		int priority_emblem = 0;

		if( event == EVENT_ACTIVATE ){
			int mountains = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN));
			priority_mana = (4*mountains)+((count_counters(player, card, COUNTER_LOYALTY)*4)-8);
			priority_emblem = (5*mountains)+((count_counters(player, card, COUNTER_LOYALTY)*5)-25);
		}

		enum{
			CHOICE_ANIMATE = 1,
			CHOICE_MANA,
			CHOICE_EMBLEM
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Animate a land", can_target(&td), 10, 1,
						"Produce mana", 1, priority_mana, -2,
						"Emblem", 1, priority_emblem, -5);

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
				case CHOICE_ANIMATE:
					if( select_target(player, card, &td, get_hacked_land_text(player, card, "Select target %s.", SUBTYPE_MOUNTAIN), NULL) ){
						instance->number_of_targets = 1;
					}
					else{
						spell_fizzled = 1;
					}
					break;

				case CHOICE_MANA:
				case CHOICE_EMBLEM:
					break;
			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		switch (choice)
		{
			case CHOICE_ANIMATE:
				if( valid_target(&td) ){
					untap_card(instance->targets[0].player, instance->targets[0].card);
					land_animation2(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 4, 4, 0, 0, COLOR_TEST_RED, 0);
				}
				break;

			case CHOICE_MANA:
				produce_mana(player, COLOR_RED, count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN)));
				break;

			case CHOICE_EMBLEM:
				generate_token_by_id(player, card, CARD_ID_KOTHS_EMBLEM);
				break;
		}
	}

	return planeswalker(player, card, event, 3);
}

int card_koth_emblem(int player, int card, event_t event){
	/* Koth's Emblem	""	0x20028ca
	 * Emblem - Koth
	 * |H1Mountains you control have "|T: This land deals 1 damage to any target." */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = SUBTYPE_MOUNTAIN;
	td.preferred_controller = player;
	td.allowed_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1)  ){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0,
															&td1, "TARGET_ANY", &td, 1);
}

int card_venser_emblem(int player, int card, event_t event){

	if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_PERMANENT", 0, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		instance->number_of_targets = 0;
	}

  return 0;
}

int card_venser_the_sojourner(int player, int card, event_t event){

	/* Venser, the Sojourner	|3|W|U
	 * Planeswalker - Venser (3)
	 * +2: Exile target permanent you own. Return it to the battlefield under your control at the beginning of the next end step.
	 * -1: Creatures are unblockable this turn.
	 * -8: You get an emblem with "Whenever you cast a spell, exile target permanent." */

	if (IS_ACTIVATING(event)){

		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allowed_controller = player | TARGET_PLAYER_OWNER;
		td.preferred_controller = player | TARGET_PLAYER_OWNER;

		int priority_unb = 0;
		int priority_emblem = 0;

		if( event == EVENT_ACTIVATE ){
			instance->number_of_targets = 0;
			priority_unb = (3*count_subtype(player, TYPE_CREATURE, -1))-((count_counters(player, card, COUNTER_LOYALTY)*4)-4);
			priority_emblem = 55+((count_counters(player, card, COUNTER_LOYALTY)*5)-40);
		}

		enum{
			CHOICE_BLINK = 1,
			CHOICE_UNBLOCKABLE,
			CHOICE_EMBLEM
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Blink a permanent", can_target(&td), 10, 2,
						"Give unblockable to all", 1, priority_unb, -1,
						"Emblem", 1, priority_emblem, -8);

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
				case CHOICE_BLINK:
					new_pick_target(&td, "Select a permanent you own.", 0, 1 | GS_LITERAL_PROMPT);
					break;

				case CHOICE_UNBLOCKABLE:
				case CHOICE_EMBLEM:
					break;
			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		switch (choice)
		{
			case CHOICE_BLINK:
				if( valid_target(&td) ){
					set_legacy_image(player, cards_data[get_original_internal_card_id(instance->targets[0].player, instance->targets[0].card)].id,
									remove_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card));
				}
				break;

			case CHOICE_UNBLOCKABLE:
				creatures_cannot_block(instance->parent_controller, instance->parent_card, NULL, 1);
				break;

			case CHOICE_EMBLEM:
				generate_token_by_id(player, card, CARD_ID_VENSERS_EMBLEM );
				break;
		}
	}

	return planeswalker(player, card, event, 3);
}

static int is_land_or_token(int player, int card){
	if( is_what(player, card, TYPE_LAND) || is_token(player, card) ){
		return 1;
	}
	return 0;
}

static int count_land_or_token(int p){
	int result = 0;
	int count = active_cards_count[p]-1;
	while( count > -1 ){
			if( in_play(p, count) && is_what(p, count, TYPE_PERMANENT) && is_land_or_token(p, count) ){
				result++;
			}
			count--;
	}
	return result;
}

int card_elspeth_tirel(int player, int card, event_t event){

	/* Elspeth Tirel	|3|W|W
	 * Planeswalker - Elspeth (4)
	 * +2: You gain 1 life for each creature you control.
	 * -2: Put three 1/1 |Swhite Soldier creature tokens onto the battlefield.
	 * -5: Destroy all other permanents except for lands and tokens. */

	if (IS_ACTIVATING(event)){
		card_instance_t *instance = get_card_instance(player, card);


		int priorities[3] = {0, 0, 0};

		if( event == EVENT_ACTIVATE ){
			instance->number_of_targets = 0;
			priorities[0] = count_subtype(player, TYPE_CREATURE, -1)*3;
			if( priorities[0] ){
				priorities[0]+=(60-(life[player]*3));
			}
			priorities[1] = ((count_counters(player, card, COUNTER_LOYALTY)*4)-8)-(count_subtype(player, TYPE_CREATURE, -1)*5);
			priorities[2] = ((count_counters(player, card, COUNTER_LOYALTY)*4)-20);
			priorities[2] += (5*count_land_or_token(player));
			priorities[2] -= (5*count_land_or_token(1-player));
		}

		enum{
			CHOICE_LIFE = 1,
			CHOICE_SOLDIERS,
			CHOICE_KILL_NONTOKENS
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Gain life", 1, priorities[0], 2,
						"3 Soldiers", 1, priorities[1], -2,
						"Nontokens must die !", 1, priorities[2], -5);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_RESOLVE_ACTIVATION)
		{
			switch (choice)
			{
				case CHOICE_LIFE:
					gain_life(player, count_permanents_by_type(player, TYPE_CREATURE));
					break;

				case CHOICE_SOLDIERS:
					generate_tokens_by_id(player, card, CARD_ID_SOLDIER, 3);
					break;

				case CHOICE_KILL_NONTOKENS:
				{
					APNAP(p, {
								int count = active_cards_count[p]-1;
								while( count > -1 ){
										if( in_play(p, count) && is_what(p, count, TYPE_PERMANENT) &&
											!(p == instance->parent_controller && count == instance->parent_card) &&
											! is_land_or_token(p, count)
										  ){
											kill_card(p, count, KILL_DESTROY);
										}
										count--;
								}
							};
						);
				}
				break;
			}
		}
	}

	return planeswalker(player, card, event, 4);
}


int card_tempered_steel(int player, int card, event_t event){
  /* Tempered Steel	|1|W|W
   * Enchantment
   * Artifact creatures you control get +2/+2. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == player){
			if( is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) &&
				is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
			{
				event_result += 2;
			}
		}
	}

	return global_enchantment(player, card, event);
}


int card_myr_galvanizer(int player, int card, event_t event){
	/* Myr Galvanizer	|3
	 * Artifact Creature - Myr 2/2
	 * Other Myr creatures you control get +1/+1.
	 * |1, |T: Untap each other Myr you control. */

	boost_creature_type(player, card, event, SUBTYPE_MYR, 1, 1, 0, BCT_CONTROLLER_ONLY);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_MYR;
		this_test.not_me = 1;
		new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL);
}

int card_kuldotha_rebirth(int player, int card, event_t event){
	/* Kuldotha Rebirth	|R
	 * Sorcery
	 * As an additional cost to cast ~, sacrifice an artifact.
	 * Put three 1/1 |Sred Goblin creature tokens onto the battlefield. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");

	if( event == EVENT_CAN_CAST && new_can_sacrifice_as_cost(player, card, &test) ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int sac = new_sacrifice(player, card, player, SAC_AS_COST | SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_GOBLIN, 3);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_spikeshot_elder(int player, int card, event_t event){
	/* Spikeshot Elder	|R	0x20028ed
	 * Creature - Goblin Shaman 1/1
	 * |1|R|R: ~ deals damage equal to its power to any target. */
	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);
			damage_target0(player, card, get_power(instance->parent_controller, instance->parent_card));
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 2), 0, &td, "TARGET_ANY");
}

int card_argentum_armor(int player, int card, event_t event)
{
  /* Argentum Armor	|6
   * Artifact - Equipment
   * Equipped creature gets +6/+6.
   * Whenever equipped creature attacks, destroy target permanent.
   * Equip |6 */

   if( is_equipping(player, card) && ! is_humiliated(player, card) ){
 		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if ( declare_attackers_trigger(player, card, event, 0, p, c))
		{
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_PERMANENT);
			td.allow_cancel = 0;

			if (can_target(&td) && new_pick_target(&td, "TARGET_PERMANENT", 0, 0) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
			instance->number_of_targets = 0;
		}
	}

	return vanilla_equipment(player, card, event, 6, 6, 6, 0, 0);
}

int card_sword_of_body_and_mind(int player, int card, event_t event)\
{
  /* Sword of Body and Mind	|3
   * Artifact - Equipment
   * Equipped creature gets +2/+2 and has protection from |Sgreen and from |Sblue.
   * Whenever equipped creature deals combat damage to a player, you put a 2/2 |Sgreen Wolf creature token onto the battlefield and that player puts the top ten
   * cards of his or her library into his or her graveyard.
   * Equip |2 */

	if (equipped_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_TRACE_DAMAGED_PLAYERS))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
	  instance->targets[1].player = 0;
	  int p;
	  for (p = 0; p <= 1; ++p)
		for (; times_damaged[p] > 0; --times_damaged[p])
		  {
			generate_token_by_id(player, card, CARD_ID_WOLF);
			mill(p, 10);
		  }
	}

	return vanilla_equipment(player, card, event, 2, 2, 2, get_sleighted_protection(player, card, KEYWORD_PROT_GREEN | KEYWORD_PROT_BLUE), 0);
}

int card_glint_hawk(int player, int card, event_t event){
  /* Glint Hawk	|W
   * Creature - Bird 2/2
   * Flying
   * When ~ enters the battlefield, sacrifice it unless you return an artifact you control to its owner's hand. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! count_subtype(player, TYPE_ARTIFACT, -1) ){
			ai_modifier -= 256;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance( player, card );

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;

		if( can_target(&td)  && new_pick_target(&td, "Select an artifact you control.", 0, GS_LITERAL_PROMPT) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			instance->number_of_targets = 0;
		}
		else{
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}


int card_genesis_wave(int player, int card, event_t event){
/* Genesis Wave	|X|G|G|G
 * Sorcery
 * Reveal the top X cards of your library. You may put any number of permanent cards with converted mana cost X or less from among
 *		them onto the battlefield. Then put all cards revealed this way that weren't put onto the battlefield into your graveyard. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
		if( spell_fizzled != 1 ){
			card_instance_t *instance = get_card_instance( player, card );
			ai_modifier += (instance->info_slot < 6 ? -48 : (instance->info_slot * 12));
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL){
		card_instance_t *instance = get_card_instance( player, card );
		int max = MIN(count_deck(player), instance->info_slot);
		if( max ){
			int gc_flag = check_battlefield_for_id(ANYBODY, CARD_ID_GRAFDIGGERS_CAGE);
			show_deck( 0, deck_ptr[player], max, "Cards revealed by Genesis Wave", 0, 0x7375B0 );
			show_deck( 1, deck_ptr[player], max, "Cards revealed by Genesis Wave", 0, 0x7375B0 );
			if( ! gc_flag ){
				int deck_copy[max];
				int i;
				for(i=0; i<max; i++){
					deck_copy[i] = deck_ptr[player][0];
					remove_card_from_deck(player, 0);
				}
				int amount = max;
				char msg[100];
				scnprintf(msg, 100, "Select a permanent card with CMC %d.", max);
				test_definition_t test;
				new_default_test_definition(&test, TYPE_PERMANENT, msg);
				test.cmc = max + 1;
				test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
				while( amount ){
						int selected = select_card_from_zone(player, player, deck_copy, amount, 0, AI_MAX_VALUE, -1, &test);
						if( selected == -1 ){
							break;
						}
						int card_added = add_card_to_hand(player, deck_copy[selected]);
						put_into_play(player, card_added);
						for(i=selected; i<(amount-1); i++){
							deck_copy[i] = deck_copy[i+1];
						}
						amount--;
				}
				if( amount ){
					for(i=0; i<amount; i++){
						raw_put_iid_on_top_of_deck(player, deck_copy[i]);
					}
					mill(player, amount);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_ratchet_bomb(int player, int card, event_t event)
{
  /* Ratchet Bomb	|2
   * Artifact
   * |T: Put a charge counter on ~.
   * |T, Sacrifice ~: Destroy each nonland permanent with converted mana cost equal to the number of charge counters on ~. */

  if (IS_ACTIVATING(event))
	{
	  if (event == EVENT_CAN_ACTIVATE && !(CAN_TAP(player, card) && CAN_ACTIVATE0(player, card)))
		return 0;

	  enum
	  {
		CHOICE_CHARGE = 1,
		CHOICE_DETONATE
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM,
						"Add charge counter", 1, 1,
						"Detonate", can_sacrifice_this_as_cost(player, card), 1);

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (event == EVENT_ACTIVATE)
		switch (choice)
		  {
			case CHOICE_CHARGE:
			  if (charge_mana_for_activated_ability(player, card, MANACOST_X(0)))
				tap_card(player, card);
			  break;

			case CHOICE_DETONATE:
			  if (charge_mana_for_activated_ability(player, card, MANACOST_X(0)))
				{
				  tap_card(player, card);
				  kill_card(player, card, KILL_SACRIFICE);
				}
			  break;
		  }
	  else	// EVENT_RESOLVE_ACTIVATION
		switch (choice)
		  {
			case CHOICE_CHARGE:
			  add_counter(player, card, COUNTER_CHARGE);
			  break;

			case CHOICE_DETONATE:
			  ;test_definition_t test;
			  default_test_definition(&test, TYPE_LAND);
			  test.type_flag = DOESNT_MATCH;
			  test.cmc = count_counters(player, card, COUNTER_CHARGE);
			  new_manipulate_all(player, card, ANYBODY, &test, KILL_DESTROY);
			  break;
		  }
	}

  if (event == EVENT_SHOULD_AI_PLAY)
	{
	  int most_common = get_most_common_cmc_nonland(1-player);
	  int counters = count_counters(player, card, COUNTER_CHARGE);
	  /* Moderately high modifier to increase towards most common.  If at most common or higher, and the AI doesn't want to sacrifice (perhaps it has better
	   * permanents at that cmc), then tiny modifier for more counters so it increases until it finds a cmc it's willing to sacrifice at. */
	  ai_modifier += (player == AI ? 1 : -1) * (counters <= most_common ? 32 : 4) * counters;
	}

	return 0;
}

int card_ezuri_renegade_leader(int player, int card, event_t event){
	/* Ezuri, Renegade Leader	|1|G|G
	 * Legendary Creature - Elf Warrior 2/2
	 * |G: Regenerate another target Elf.
	 * |2|G|G|G: Elf creatures you control get +3/+3 and gain trample until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_ELF;
	td.required_state = TARGET_STATE_DESTROYED;
	if( IS_AI(player) ){
		td.special = TARGET_SPECIAL_REGENERATION | TARGET_SPECIAL_NOT_ME;
	}
	else{
		td.special = TARGET_SPECIAL_NOT_ME;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_REGENERATION = 1,
		CHOICE_OVERRUN
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( land_can_be_played & LCBP_REGENERATION ){
			if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, NULL) ){
				return 99;
			}
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_XG(2, 3), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE  ){
		instance->number_of_targets = 0;
		if( land_can_be_played & LCBP_REGENERATION ){
			instance->info_slot = CHOICE_REGENERATION;
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_G(1), 0,
											&td, "Select another target Elf to regenerate.");
		}
		instance->info_slot = CHOICE_OVERRUN;
		return generic_activated_ability(player, card, event, 0, MANACOST_XG(2, 3), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_REGENERATION && valid_target(&td) &&
			can_be_regenerated(instance->targets[0].player, instance->targets[0].card) )
		{
			regenerate_target(instance->targets[0].player, instance->targets[0].card);
		}
		if( instance->info_slot == CHOICE_OVERRUN ){
			pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, SUBTYPE_ELF, 3, 3, KEYWORD_TRAMPLE, 0);
		}
	}

	return 0;
}

int card_skinrender(int player, int card, event_t event){
	/* Skinrender	|2|B|B
	 * Creature - Zombie 3/3
	 * When ~ enters the battlefield, put three -1/-1 counters on target creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 3);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_moltentail_masticore(int player, int card, event_t event){
	/* Molten-Tail Masticore	|4	0x200294c
	 * Artifact Creature - Masticore 4/4
	 * At the beginning of your upkeep, sacrifice ~ unless you discard a card.
	 * |4, Exile a creature card from your graveyard: ~ deals 4 damage to any target.
	 * |2: Regenerate ~. */
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int kill = 1;
		if( hand_count[player] > 0 ){
			int choice = do_dialog(player, player, card, player, card, " Pay upkeep\n Pass", 0);
			if( choice == 0 ){
				discard(player, 0, player);
				kill--;
			}
		}
		if( kill > 0 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;


	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_REGENERATION = 1,
		CHOICE_DAMAGE
	};

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		if( regeneration(player, card, event, MANACOST_X(2)) ){
			return 0x63;
		}
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(4), 0, &td, NULL) ){
			return count_graveyard_by_type(player, TYPE_CREATURE) ? 1 : 0;
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( regeneration(player, card, EVENT_CAN_ACTIVATE, MANACOST_X(2)) ){
			instance->info_slot = CHOICE_REGENERATION;
			return regeneration(player, card, event, MANACOST_X(2));
		}
		instance->info_slot = CHOICE_DAMAGE;
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(4), 0, &td, "TARGET_ANY");
		if( spell_fizzled != 1 ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card to exile.");
			int selected = new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test);
			if( selected == -1 ){
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_REGENERATION ){
			return regeneration(player, card, event, MANACOST_X(2));
		}
		if( instance->info_slot == CHOICE_DAMAGE && valid_target(&td) ){
			damage_target0(player, card, 4);
		}
	}

	return 0;
}

int card_skithiryx_blight_dragon(int player, int card, event_t event){
	/* Skithiryx, the Blight Dragon	|3|B|B
	 * Legendary Creature - Dragon Skeleton 4/4
	 * Flying
	 * Infect
	 * |B: ~ gains haste until end of turn.
	 * |B|B: Regenerate Skithiryx. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_REGENERATION = 1,
		CHOICE_HASTE
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		if( regeneration(player, card, event, MANACOST_B(2)) ){
			return 0x63;
		}
		return generic_shade(player, card, event, 0, MANACOST_B(1), 0, 0, 0, SP_KEYWORD_HASTE);
	}

	if( event == EVENT_ACTIVATE ){
		if( regeneration(player, card, EVENT_CAN_ACTIVATE, MANACOST_B(2)) ){
			instance->info_slot = CHOICE_REGENERATION;
			return regeneration(player, card, event, MANACOST_B(2));
		}
		instance->info_slot = CHOICE_HASTE;
		ai_modifier += (current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS && is_sick(player, card) ? 48 : -48);
		return generic_shade(player, card, event, 0, MANACOST_B(1), 0, 0, 0, SP_KEYWORD_HASTE);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_REGENERATION ){
			return regeneration(player, card, event, MANACOST_B(2));
		}
		if( instance->info_slot == CHOICE_HASTE ){
			return generic_shade(player, card, event, 0, MANACOST_B(1), 0, 0, 0, SP_KEYWORD_HASTE);
		}
	}

	return 0;
}


int card_mimic_vat(int player, int card, event_t event){
/* Mimic Vat	|3
 * Artifact
 * Imprint - Whenever a nontoken creature dies, you may exile that card. If you do, return each other card exiled with ~ to its owner's
 * 		graveyard.
 * |3, |T: Put a token onto the battlefield that's a copy of the exiled card. It gains haste. Exile it at the beginning of the next end step. */

	if( event == EVENT_GRAVEYARD_FROM_PLAY && in_play(affected_card_controller, affected_card) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) && ! is_token(affected_card_controller, affected_card) ){
			if( is_this_dying(affected_card_controller, affected_card) ){
				card_instance_t *instance = get_card_instance(player, card);
				if( instance->targets[11].player < 0 ){
					instance->targets[11].player = 0;
				}
				int position = BYTE0(instance->targets[11].player);
				if( position < 9 ){
					instance->targets[position].player = get_owner(affected_card_controller, affected_card);
					instance->targets[position].card = get_original_internal_card_id(affected_card_controller, affected_card);
					SET_BYTE0(instance->targets[11].player)++;
				}
		   }
		}
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( result ){
		card_instance_t *instance = get_card_instance(player, card);
		int amount = BYTE0(result);
		int dead[2][9];
		int i;
		for (i = 0; i < amount; i++){
			dead[0][i] = instance->targets[i].player;
			dead[1][i] = instance->targets[i].card;
			instance->targets[i].player = instance->targets[i].card = -1;
		}
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card to imprint.");
		int selected = select_card_from_zone(player, player, dead[1], amount, 0, AI_MAX_VALUE, -1, &test);
		if( selected != -1 ){
			int owner = dead[0][selected];
			int iid = dead[1][selected];
			int previous_iid = instance->targets[9].card;
			int found = 0;
			for(i = count_graveyard(owner)-1; i > -1; i--){
				if( get_grave(owner)[i] == iid ){
					instance->targets[9].player = owner;
					instance->targets[9].card = iid;
					rfg_card_from_grave(owner, i);
					found = 1;
					break;
				}
			}
			if( found && previous_iid > -1 ){
				int position = find_iid_in_rfg(instance->targets[9].player, instance->targets[9].card);
				if( position != -1 ){
					from_exile_to_graveyard(instance->targets[9].player, position);
				}
			}
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( IS_AI(player) || instance->targets[9].card == -1 ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
		}
		return 1;
	}

	if( event == EVENT_ACTIVATE ){
		int choice = 1;
		if( ! IS_AI(player) && instance->targets[9].card > -1 ){
			int can_reveal = check_rfg(instance->targets[9].player, cards_data[instance->targets[9].card].id);
			int can_activate = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
			choice = DIALOG(player, card, event, DLG_NO_STORAGE,
							"Activate Mimic Vat", can_activate, 1,
							"Show imprinted card", can_reveal, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == 1 ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
		}
		if( choice == 2 ){
			int exiled[1] = {instance->targets[9].card};
			show_deck(player, exiled, 1, "Here's the card imprinted on Mimic Vat", 0, 0x7375B0);
			spell_fizzled = 1;
		}
	}


	if( event == EVENT_RESOLVE_ACTIVATION){
		if( instance->targets[9].player > -1 ){
			int csvid = cards_data[instance->targets[9].card].id;
			if( check_rfg(instance->targets[9].player, csvid) ){
				token_generation_t token;
				default_token_definition(instance->parent_controller, instance->parent_card, csvid, &token);
				token.special_code_on_generation = &haste_and_remove_token_at_end_of_next_turn;
				generate_token(&token);
			}
		}
	}

	return 0;
}

int card_contagion_clasp(int player, int card, event_t event){
	/* Contagion Clasp	|2
	 * Artifact
	 * When ~ enters the battlefield, put a -1/-1 counter on target creature.
	 * |4, |T: Proliferate. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 1);
		}
		instance->number_of_targets = 0;
	}

	return generic_artifact_with_proliferate(player, card, event, 4, 1);
}

int card_contagion_engine(int player, int card, event_t event){
/* Contagion Engine	|6
 * Artifact
 * When ~ enters the battlefield, put a -1/-1 counter on each creature target player controls.
 * |4, |T: Proliferate, then proliferate again. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0 );
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			manipulate_type(player, card, instance->targets[0].player, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_M1_M1, 1));
		}
		instance->number_of_targets = 0;
	}

	return generic_artifact_with_proliferate(player, card, event, 4, 2);
}


int card_thrummingbird(int player, int card, event_t event){
	/* Thrummingbird	|1|U
	 * Creature - Bird Horror 1/1
	 * Flying
	 * Whenever ~ deals combat damage to a player, proliferate. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		int i;
		for (i = 0; i < amount; ++i){
			proliferate(player, card);
		}
	}

	return 0;
}

int card_lux_cannon(int player, int card, event_t event){
/* Lux Cannon	|4
 * Artifact
 * |T: Put a charge counter on ~.
 * |T, Remove three charge counters from ~: Destroy target permanent. */

	if (event == EVENT_SHOULD_AI_PLAY){
		int count = count_counters(player, card, COUNTER_CHARGE);
		ai_modifier += (player == AI ? 1 : -1) * (count + (12 * (count / 3)));
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_COUNTER = 1,
		CHOICE_KILL_PERMANENT
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int can_kill_permanent = can_target(&td) && count_counters(player, card, COUNTER_CHARGE) >= 3 ? 1 : 0;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Add a charge counter", 1, 5,
							"Kill a permanent", can_kill_permanent, 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_COUNTER ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
		if( choice == CHOICE_KILL_PERMANENT ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0,
											GVC_COUNTERS(COUNTER_CHARGE, 3), &td, "TARGET_PERMANENT");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_COUNTER ){
			add_counter(instance->parent_controller, instance->parent_card, COUNTER_CHARGE);
		}

		if( instance->info_slot == CHOICE_KILL_PERMANENT && valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_etched_champion(int player, int card, event_t event){
  /* Etched Champion	|3
   * Artifact Creature - Soldier 2/2
   * Metalcraft - ~ has protection from all colors as long as you control three or more artifacts. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && in_play(player, card) ){
		if( metalcraft(player, card) ){
			event_result |= KEYWORD_PROT_BLACK;
			event_result |= KEYWORD_PROT_BLUE;
			event_result |= KEYWORD_PROT_GREEN;
			event_result |= KEYWORD_PROT_RED;
			event_result |= KEYWORD_PROT_WHITE;
		}
	}

	return 0;
}

int card_shape_anew(int player, int card, event_t event){
  /* Shape Anew	|3|U
   * Sorcery
   * The controller of target artifact sacrifices it, then reveals cards from the top of his or her library until he or she reveals an artifact card. That player puts that card onto the battlefield, then shuffles all other cards revealed this way into his or her library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			metamorphosis(instance->targets[0].player, instance->targets[0].card, TYPE_ARTIFACT, KILL_SACRIFICE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
}

int card_hand_of_the_praetors(int player, int card, event_t event){
  /* Hand of the Praetors	|3|B
   * Creature - Zombie 3/2
   * Infect
   * Other creatures you control with infect get +1/+1.
   * Whenever you cast a creature spell with infect, target player gets a poison counter. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && ! affect_me(player, card) ){
			if( affected_card_controller == player && in_play(affected_card_controller, affected_card) &&
				is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
				(get_abilities(affected_card_controller, affected_card, EVENT_ABILITIES, -1) & KEYWORD_INFECT) )
			{
				event_result++;
			}
		}
	}

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.keyword = KEYWORD_INFECT;

		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			target_definition_t td;
			default_target_definition(player, card, &td, 0);
			td.zone = TARGET_ZONE_PLAYERS;

			card_instance_t *instance = get_card_instance(player, card);

			if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
				poison(instance->targets[0].player, 1);
			}
			instance->number_of_targets = 0;
		}
	}

 return 0;
}

static void pump_ichorclaw_myr(int player, int card, int t_player, int t_card)
{
	pump_until_eot_merge_previous(player, card, player, card, 2, 2);
}

int card_ichorclaw_myr(int player, int card, event_t event){ //Sharing code with: Razorclaw Bear, Slashing Tiger, Snorting Gahr.
  /* Ichorclaw Myr	|2
   * Artifact Creature - Myr 1/1
   * Infect
   * Whenever ~ becomes blocked, it gets +2/+2 until end of turn. */

	if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card))
	{
		if (player == current_turn && check_state(player, card, STATE_ATTACKING) )
			for_each_creature_blocking_me(player, card, pump_ichorclaw_myr, player, card);
	}

	return 0;
}

int card_vector_asp(int player, int card, event_t event){
	/* Vector Asp	|1
	 * Artifact Creature - Snake 1/1
	 * |B: ~ gains infect until end of turn. */

	return generic_shade(player, card, event, 0, MANACOST_B(1), 0, 0, KEYWORD_INFECT, 0);
}

static int get_potential_poison(int player, int exc_player, int exc_card){
	int result = 0;
	CYCLE_ALL_CARDS(p, c,	{
								if( p == 1-player && in_play(p, c) && is_what(p, c, TYPE_CREATURE) && is_attacking(p, c) &&
									is_unblocked(p, c) && !( p == exc_player && c == exc_card ) )
								{
									if( get_abilities(p, c, -1, EVENT_ABILITIES) & KEYWORD_INFECT ){
										result += get_attack_power(p, c);
									}
								}
							}
	);
	return result;
}

int card_tainted_strike(int player, int card, event_t event){
  /* Tainted Strike	|B
   * Instant
   * Target creature gets +1/+0 and gains infect until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			int mod = 0;
			if( instance->targets[0].player == 1-player ){
				mod = -256;
				if( is_attacking(instance->targets[0].player, instance->targets[0].card) &&
					is_unblocked(instance->targets[0].player, instance->targets[0].card) )
				{
					int pow = get_attack_power(instance->targets[0].player, instance->targets[0].card);
					if( life[player] - pow <= 0 ){
						if( get_potential_poison(player, instance->targets[0].player, instance->targets[0].card) +
							POISON_COUNTERS(player) + (pow +1) < 10 )
						{
							mod = 96;
						}
					}
				}
			}
			else{
				mod += (get_attack_power(instance->targets[0].player, instance->targets[0].card) * 12);
			}
			ai_modifier += mod;
		}
		return 0;
	}

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1, 0, KEYWORD_INFECT, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_grafted_exoskeleton(int player, int card, event_t event){
	/* Grafted Exoskeleton	|4
	 * Artifact - Equipment
	 * Equipped creature gets +2/+2 and has infect.
	 * Whenever ~ becomes unattached from a permanent, sacrifice that permanent.
	 * Equip |2 */

	if( event == EVENT_UNATTACH || leaves_play(player, card, event) ){
		if( is_equipping(player, card) ){
			card_instance_t *instance = get_card_instance(player, card);
			if( player == instance->damage_target_player ){
				kill_card(instance->damage_target_player, instance->damage_target_card, KILL_SACRIFICE);
			}
		}
	}

	return vanilla_equipment(player, card, event, 2, 2, 2, KEYWORD_INFECT, 0);
}

int card_semblance_anvil(int player, int card, event_t event){
	/* Semblance Anvil	|3
	 * Artifact
	 * Imprint - When ~ enters the battlefield, you may exile a nonland card from your hand.
	 * Spells you cast that share a card type with the exiled card cost |2 less to cast. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a nonland card to Imprint");
		test.type_flag = DOESNT_MATCH;

		int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &test);
		if( selected > -1 ){
			int pos = MIN(1, instance->info_slot);
			if( pos < 10 ){
				instance->targets[pos].card = get_type(player, selected);
				instance->info_slot = pos + 1;
			}
			rfg_card_in_hand(player, selected);
		}
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card) &&
		(affected_card_controller == -1 || affected_card_controller == player) )
	{
		card_instance_t *instance = get_card_instance(player, card);
		int i;
		for(i=1; i<instance->info_slot; i++){
			if( is_what(affected_card_controller, affected_card, instance->targets[i].card) ){
				COST_COLORLESS-=2;
			}
		}
		return 0;
	}

	return 0;
}

int card_throne_of_geth(int player, int card, event_t event){
	/* Throne of Geth	|2
	 * Artifact
	 * |T, Sacrifice an artifact: Proliferate. */

	if( event == EVENT_ACTIVATE){
		ai_mod_for_proliferate(player, 6); //Not really a manacost in this case, just weighting the sacrifice of one of your artifacts
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		proliferate(player, card);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL,
															TYPE_ARTIFACT, NULL, 0);
}

int card_kuldotha_forgemaster(int player, int card, event_t event){
	/* Kuldotha Forgemaster	|5
	 * Artifact Creature - Construct 3/5
	 * |T, Sacrifice three artifacts: Search your library for an artifact card and put it onto the battlefield. Then shuffle your library. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");

    if( event == EVENT_CAN_ACTIVATE ){
		test.qty = 3;
		return( generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL,
															0, &test, 0) );
	}

	if( event == EVENT_ACTIVATE){
		int sac[3];
		int sc = 0;
		while( sc < 3 ){
				sac[sc] = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
				if( ! sac[sc] ){
					spell_fizzled = 1;
					break;
				}
				sc++;
		}
		int i;
		for(i=0; i<sc; i++){
			state_untargettable(BYTE2(sac[i]), BYTE3(sac[i]), 0);
		}
		if( spell_fizzled == 1 ){
			return 0;
		}
		generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, 0, 0);
		if( spell_fizzled == 1 ){
			return 0;
		}
		for(i=0; i<3; i++){
			kill_card(BYTE2(sac[i]), BYTE3(sac[i]), KILL_SACRIFICE);
		}

		int mod = -24;
		   if( current_turn == 1-player && current_phase >= PHASE_MAIN2 ){
			    mod += 24;
		   }
		mod += ((count_subtype(player, TYPE_ARTIFACT, -1)-3) * 8);
		ai_modifier += mod;
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an artifact card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
	}

	return 0;
}

int card_tumble_magnet(int player, int card, event_t event){

	/* Tumble Magnet	|3
	 * Artifact
	 * ~ enters the battlefield with three charge counters on it.
	 * |T, Remove a charge counter from ~: Tap target artifact or creature. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, 3);

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_CHARGE),
									&td, "PUTREFY");	// "Select target artifact or creature."
		if( spell_fizzled != 1 ){
			ai_mod_for_tap_effects(player, card, 0);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_CHARGE), &td, NULL);
}

int card_necrotic_ooze(int player, int card, event_t event ){
	/* Necrotic Ooze	|2|B|B
	 * Creature - Ooze 4/3
	 * As long as ~ is on the battlefield, it has all activated abilities of all creature cards in all graveyards. */

	if( event == EVENT_CAN_ACTIVATE ){
		int result = 0;
		int p;
		for (p = 0; p <= 1; ++p){
			int i = 0;
			while( get_grave(p)[i] != -1 ){
					int iid = get_grave(p)[i];
					if( is_what(-1, iid, TYPE_CREATURE) && cards_data[iid].id != CARD_ID_NECROTIC_OOZE ){
						if( cards_data[iid].extra_ability & EA_ACT_ABILITY ){
							CardFunction ptFunction = cards_data[iid].code_pointer;
							int act = ptFunction(player, card, EVENT_CAN_ACTIVATE);
							if( act == 99 ){
								return 99;
							}
							if( act == 1 ){
								result = 1;
							}
						}
					}
					i++;
			}
		}
		return result;
	}

	if( event == EVENT_ACTIVATE ){
		get_card_instance(player, card)->number_of_targets = 0;
		int activable[10] = {0}; //It could be higher, but oh well...
		int ac = 0;
		int p;
		int i;
		for (p = 0; p <= 1; ++p){
			i = 0;
			while( get_grave(p)[i] != -1 ){
					int iid = get_grave(p)[i];
					if( is_what(-1, iid, TYPE_CREATURE) && cards_data[iid].id != CARD_ID_NECROTIC_OOZE ){
						if( cards_data[iid].extra_ability & EA_ACT_ABILITY ){
							CardFunction ptFunction = cards_data[iid].code_pointer;
							if( ptFunction(player, card, EVENT_CAN_ACTIVATE) ){
								activable[ac] = iid;
								ac++;
							}
						}
					}
					i++;
			}
		}
		if( ! ac ){
			spell_fizzled = 1;
			return 0;
		}
		char msg[2][100];
		for(i=0; i<ac; i++){
			scnprintf(msg[i], 100, "%s", cards_ptr[cards_data[activable[ac]].id]->name);
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL,
							msg[0], activable[0], 1,
							msg[1], activable[1], 1,
							msg[2], activable[2], 1,
							msg[3], activable[3], 1,
							msg[4], activable[4], 1,
							msg[5], activable[5], 1,
							msg[6], activable[6], 1,
							msg[7], activable[7], 1,
							msg[8], activable[8], 1,
							msg[9], activable[9], 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		// pay the costs for the selected card
		CardFunction ptFunction = cards_data[ activable[choice-1] ].code_pointer;
		ptFunction(player, card, EVENT_ACTIVATE);
		if( spell_fizzled != 1 ){
			get_card_instance(player, card)->info_slot = activable[choice-1];
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int int_id = get_card_instance(player, card)->info_slot;
		CardFunction ptFunction = cards_data[int_id].code_pointer;
		ptFunction(player, card, EVENT_RESOLVE_ACTIVATION);
	}

	return 0;
}

int card_generic_som_tapland(int player, int card, event_t event)
{
  /* Blackcleave Cliffs	""
   * Land
   * ~ enters the battlefield tapped unless you control two or fewer other lands.
   * |T: Add |B or |R to your mana pool. */

  /* Copperline Gorge	""
   * Land
   * ~ enters the battlefield tapped unless you control two or fewer other lands.
   * |T: Add |R or |G to your mana pool. */

  /* Darkslick Shores	""
   * Land
   * ~ enters the battlefield tapped unless you control two or fewer other lands.
   * |T: Add |U or |B to your mana pool. */

  /* Razorverge Thicket	""
   * Land
   * ~ enters the battlefield tapped unless you control two or fewer other lands.
   * |T: Add |G or |W to your mana pool. */

  /* Seachrome Coast	""
   * Land
   * ~ enters the battlefield tapped unless you control two or fewer other lands.
   * |T: Add |W or |U to your mana pool. */

  // inlines comes_into_play_tapped()
  if ((event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE) && affect_me(player, card)
	  && !check_special_flags2(player, card, SF2_FACE_DOWN_DUE_TO_MANIFEST))
	{
	  int num_needed = 2;
	  if (in_play(player, card))	// will be false for EVENT_CAST_SPELL, true for EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE
		num_needed = 3;

	  if (basiclandtypes_controlled[player][COLOR_ANY] > num_needed)
		get_card_instance(player, card)->state |= STATE_TAPPED;	// avoid sending event
	}

  return mana_producer(player, card, event);
}

int card_nim_deathmantle(int player, int card, event_t event){
/* Nim Deathmantle	|2
 * Artifact - Equipment
 * Equipped creature gets +2/+2, has intimidate, and is a |Sblack Zombie.
 * Whenever a nontoken creature is put into your graveyard from the battlefield, you may pay |4. If you do, return that card to the
 * 		battlefield and attach ~ to it.
 * Equip |4 */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( is_equipping(player, card) ){
			card_instance_t *instance = get_card_instance(player, card);
			if( event == EVENT_SET_COLOR && affect_me(instance->damage_target_player, instance->damage_target_card) ){
				event_result = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
			}
		}

		if( event == EVENT_GRAVEYARD_FROM_PLAY && in_play(affected_card_controller, affected_card) &&
			! is_token(affected_card_controller, affected_card) && get_owner(affected_card_controller, affected_card) == player )
		{
			count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, TYPE_CREATURE, NULL, GFPC_TRACK_DEAD_CREATURES, 0);
		}
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( result ){
		card_instance_t *instance = get_card_instance(player, card);
		int amount = BYTE0(result);
		if( amount ){
			int dead[2][amount];
			int i;
			for(i=0; i<amount; i++){
				dead[0][i] = instance->targets[i].player;
				dead[1][i] = instance->targets[i].card;
			}
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a permanent to bring back.");
			int selected = select_card_from_zone(player, player, dead[1], amount, 0, AI_MAX_CMC, -1, &test);
			if( selected > -1 && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 4) ){
				int owner = dead[0][selected];
				int iid = dead[1][selected];
				int zombo = seek_grave_for_id_to_reanimate(player, card, owner, cards_data[iid].id, REANIMATE_DEFAULT);
				if( zombo > -1 ){
					equip_target_creature(player, card, player, zombo);
				}
				else{
					spell_fizzled = 1;
				}
			}
		}
	}

	if( leaves_play(player, card, event) || event == EVENT_UNATTACH ){
		if( is_equipping(player, card) ){
			card_instance_t *instance = get_card_instance(player, card);
			reset_subtypes(instance->damage_target_player, instance->damage_target_card, RS_RESET_ADDED);
		}
	}

	if( event == EVENT_EQUIPMENT_ATTACHED ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( p > -1 && in_play(p, c) ){
			add_a_subtype(p, c, SUBTYPE_ZOMBIE);
		}
	}

	return vanilla_equipment(player, card, event, 4, 2, 2, 0, SP_KEYWORD_INTIMIDATE);
}

int card_perilous_myr(int player, int card, event_t event){
	/* Perilous Myr	|2	0x2003f72
	 * Artifact Creature - Myr 1/1
	 * When ~ dies, it deals 2 damage to any target. */
	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
			damage_target_creature_or_player(instance->targets[0].player, instance->targets[0].card, 2, player, card);
		}
	}

	return 0;
}

int card_abuna_acolyte(int player, int card, event_t event){
	/* Abuna Acolyte	|1|W	0x2004166
	 * Creature - Cat Cleric 1/1
	 * |T: Prevent the next 1 damage that would be dealt to any target this turn.
	 * |T: Prevent the next 2 damage that would be dealt to target artifact creature this turn. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			int minus = 1;
			if( target->damage_target_card != -1 ){
				if( in_play(target->damage_target_player, target->damage_target_card) &&
					is_what(target->damage_target_player, target->damage_target_card, TYPE_CREATURE) &&
					is_what(target->damage_target_player, target->damage_target_card, TYPE_ARTIFACT) )
				{
					minus = 2;
				}
			}
			prevent_damage(instance->targets[0].player, instance->targets[0].card, minus);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST0, 0,
									&td, "TARGET_DAMAGE");
}

int card_accorder_shield(int player, int card, event_t event){
	/* Accorder's Shield	|0
	 * Artifact - Equipment
	 * Equipped creature gets +0/+3 and has vigilance.
	 * Equip |3 */

	return vanilla_equipment(player, card, event, 3, 0, 3, 0, SP_KEYWORD_VIGILANCE);
}

int card_acid_web_spider(int player, int card, event_t event){
	/* Acid Web Spider	|3|G|G
	 * Creature - Spider 3/5
	 * Reach
	 * When ~ enters the battlefield, you may destroy target Equipment. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.required_subtype = SUBTYPE_EQUIPMENT;
		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && new_pick_target(&td, "TARGET_EQUIPMENT", 0, 0)  ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_arc_trail(int player, int card, event_t event){
	/* Arc Trail	|1|R	0x200417a
	 * Sorcery
	 * ~ deals 2 damage to any target and 1 damage to another target. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			target_definition_t td2;
			default_target_definition(player, card, &td2, TYPE_CREATURE );

			target_definition_t td3;
			default_target_definition(player, card, &td3, 0 );
			td3.zone = TARGET_ZONE_PLAYERS;

			return target_available(player, card, &td2)+target_available(player, card, &td3) >= 2 ? 1 : 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int players[2] = {0, 0};
		if( new_pick_target(&td, "TARGET_ANY", 0, 1) ){
			if( instance->targets[0].card != -1 ){
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			}
			else{
				players[instance->targets[0].player] = 1;
			}
			if( new_pick_target(&td, "Select another target creature, player, or planeswalker.", 1, 1 | GS_LITERAL_PROMPT) ){
				if( instance->targets[1].card == -1 && players[instance->targets[1].player] == 1 ){
					spell_fizzled = 1;
				}
			}
			if( instance->targets[0].card != -1 ){
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
			}
		}
		if( spell_fizzled != 1 ){
			return basic_spell(player, card, event);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_argent_sphynx(int player, int card, event_t event){
	/* Argent Sphinx	|2|U|U
	 * Creature - Sphinx 4/3
	 * Flying
	 * Metalcraft - |U: Exile ~. Return it to the battlefield under your control at the beginning of the next end step.
	 Activate this ability only if you control three or more artifacts. */

	if( event == EVENT_CAN_ACTIVATE && ! metalcraft(player, card) ){
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t *instance = get_card_instance( player, card );
		blink_effect_controller(instance->parent_controller, instance->parent_card, 0);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_U(1), 0, NULL, NULL);
}

int card_asceticism(int player, int card, event_t event){
	/* Asceticism	|3|G|G
	 * Enchantment
	 * Creatures you control have hexproof.
	 * |1|G: Regenerate target creature. */

	boost_subtype(player, card, event, -1, 0, 0, 0, SP_KEYWORD_HEXPROOF, BCT_CONTROLLER_ONLY|BCT_INCLUDE_SELF);

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

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( land_can_be_played & LCBP_REGENERATION ){
			if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XG(1, 1), 0, &td, NULL) ){
				return 99;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XG(1, 1), 0, &td, "TARGET_CREATURE");
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target( instance->targets[0].player, instance->targets[0].card );
		}
	}

	return 0;
}

int card_assault_strobe(int player, int card, event_t event){
	/* Assault Strobe	|R
	 * Sorcery
	 * Target creature gains double strike until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			if( current_phase > PHASE_AFTER_BLOCKING ){
				ai_modifier -=96;
				return 0;
			}
			if( current_turn == player && check_for_ability(instance->targets[0].player, instance->targets[0].card, KEYWORD_DEFENDER) ){
				ai_modifier -=96;
				return 0;
			}
			if( current_turn == 1-player &&
				check_for_special_ability(instance->targets[0].player, instance->targets[0].card, SP_KEYWORD_CANNOT_BLOCK) )
			{
				ai_modifier -=96;
				return 0;
			}
			int pow = get_power(instance->targets[0].player, instance->targets[0].card);
			int mul = 1;
			if( check_for_ability(instance->targets[0].player, instance->targets[0].card, KEYWORD_FLYING) ){
				mul++;
			}
			if( check_for_special_ability(instance->targets[0].player, instance->targets[0].card,
											SP_KEYWORD_SHADOW | SP_KEYWORD_UNBLOCKABLE) )
			{
				mul++;
			}
			ai_modifier += (pow * 8 * mul);
			return 0;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_DOUBLE_STRIKE, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_auriok_edgewright(int player, int card, event_t event){
	/* Auriok Edgewright	|W|W
	 * Creature - Human Soldier 2/2
	 * Metalcraft - ~ has double strike as long as you control three or more artifacts. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && in_play(player, card) && metalcraft(player, card) ){
		event_result |= KEYWORD_DOUBLE_STRIKE;
	}

	return 0;
}

int card_auriok_replica(int player, int card, event_t event){
	/* Auriok Replica	|3
	 * Artifact Creature - Cleric 2/2
	 * |W, Sacrifice ~: Prevent all damage a source of your choice would deal to you this turn. */

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
			prevent_damage(instance->targets[0].player, instance->targets[0].card, -1);
			card_instance_t *source = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			if( in_play(source->damage_source_player, source->damage_source_card) &&
				is_what(source->damage_source_player, source->damage_source_card, TYPE_PERMANENT) )
			{
				int legacy = prevent_all_damage_to_target(player, card, player, -1, 1);
				card_instance_t *leg = get_card_instance(player, legacy);
				leg->targets[1] = instance->targets[0];
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_W(1), 0,
									&td, "TARGET_DAMAGE");
}

int card_auriok_sunchaser(int player, int card, event_t event){
	/* Auriok Sunchaser	|1|W
	 * Creature - Human Soldier 1/1
	 * Metalcraft - As long as you control three or more artifacts, ~ gets +2/+2 and has flying. */

	if( affect_me(player, card) && in_play(player, card) && metalcraft(player, card) ){
		switch( event ){
				case EVENT_ABILITIES:
					event_result |= KEYWORD_FLYING;
					break;
				case EVENT_POWER:
				case EVENT_TOUGHNESS:
					event_result += 2;
				default:
					break;
		}
	}

	return 0;
}

int card_barbed_battlegear(int player, int card, event_t event){
	/* Barbed Battlegear	|3
	 * Artifact - Equipment
	 * Equipped creature gets +4/-1.
	 * Equip |2 */

	return vanilla_equipment(player, card, event, 2, 4, -1, 0, 0);
}

int card_barrage_ogre(int player, int card, event_t event){
	/* Barrage Ogre	|3|R|R	0x200419d
	 * Creature - Ogre Warrior 3/3
	 * |T, Sacrifice an artifact: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td1) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0,
															&td1, "TARGET_ANY", TYPE_ARTIFACT, NULL, 0);
}

int card_bellowing_tanglewurm(int player, int card, event_t event)
{
	/* Bellowing Tanglewurm	|3|G|G
	 * Creature - Wurm 4/4
	 * Intimidate
	 * Other |Sgreen creatures you control have intimidate. */
    intimidate(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_ABILITIES && affected_card_controller == player && ! affect_me(player, card) &&
			in_play(affected_card_controller, affected_card) &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
			(get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_GREEN)) )
		{
			intimidate(affected_card_controller, affected_card, event);
		}
	}

	return 0;
}

int card_blade_tribe_berserkers(int player, int card, event_t event){
	/* Blade-Tribe Berserkers	|3|R
	 * Creature - Human Berserker 3/3
	 * Metalcraft - When ~ enters the battlefield, if you control three or more artifacts, ~ gets +3/+3 and gains haste until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( metalcraft(player, card) ){
			pump_ability_until_eot(player, card, player, card, 3, 3, 0, SP_KEYWORD_HASTE);
		}
	}

	return 0;
}

int card_bladed_pinons(int player, int card, event_t event){
	/* Bladed Pinions	|2
	 * Artifact - Equipment
	 * Equipped creature has flying and first strike.
	 * Equip |2 */

	return vanilla_equipment(player, card, event, 2, 0, 0, KEYWORD_FLYING | KEYWORD_FIRST_STRIKE, 0);
}

int card_bleak_coven_vampires(int player, int card, event_t event){
	/* Bleak Coven Vampires	|3|B|B
	 * Creature - Vampire Warrior 4/3
	 * Metalcraft - When ~ enters the battlefield, if you control three or more artifacts, target player loses 4 life and you gain 4 life. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( metalcraft(player, card) && can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			lose_life(instance->targets[0].player, 4);
			gain_life(player, 4);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_blistergrub(int player, int card, event_t event){
	/* Blistergrub	|2|B
	 * Creature - Horror 2/2
	 * |H2Swampwalk
	 * When ~ dies, each opponent loses 2 life. */
	/* Tattered Mummy	|1|B
	 * Creature - Zombie Jackal 1/2
	 * When ~ dies, each opponent loses 2 life. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		lose_life(1-player, 2);
	}

	return 0;
}

int card_bloodshoot_trainee(int player, int card, event_t event){
	/* Bloodshot Trainee	|3|R
	 * Creature - Goblin Warrior 2/3
	 * |T: ~ deals 4 damage to target creature. Activate this ability only if ~'s power is 4 or greater. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_CAN_ACTIVATE && get_power(player, card) < 4 ){
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_blunt_the_assault(int player, int card, event_t event){
	/* Blunt the Assault	|3|G
	 * Instant
	 * You gain 1 life for each creature on the battlefield. Prevent all combat damage that would be dealt this turn. */

	if(event == EVENT_RESOLVE_SPELL){
		int amount = count_permanents_by_type(player, TYPE_CREATURE) + count_permanents_by_type(1-player, TYPE_CREATURE);
		gain_life(player, amount);
		fog_effect(player, card);
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return card_fog(player, card, event);
}

int card_bonds_of_quicksilver(int player, int card, event_t event){
	/* Bonds of Quicksilver	|3|U
	 * Enchantment - Aura
	 * Flash
	 * Enchant creature
	 * Enchanted creature doesn't untap during its controller's untap step. */

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance( player, card )->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance( player, card );
		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier -= 16;
	}

	return disabling_aura(player, card, event);
}

int card_carnifex_demon(int player, int card, event_t event){
	/* Carnifex Demon	|4|B|B
	 * Creature - Demon 6/6
	 * Flying
	 * ~ enters the battlefield with two -1/-1 counters on it.
	 * |B, Remove a -1/-1 counter from ~: Put a -1/-1 counter on each other creature. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_M1_M1, 2);

	if( event == EVENT_RESOLVE_ACTIVATION){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		card_instance_t* instance = get_card_instance(player, card);
		new_manipulate_all(instance->parent_controller, instance->parent_card, ANYBODY, &this_test, ACT_ADD_COUNTERS(COUNTER_M1_M1, 1));
	}

	return generic_activated_ability(player, card, event, GAA_MINUS1_MINUS1_COUNTER, MANACOST_B(1), 0, NULL, NULL);
}

int card_carrion_call(int player, int card, event_t event){
	/* Carrion Call	|3|G
	 * Instant
	 * Put two 1/1 |Sgreen Insect creature tokens with infect onto the battlefield. */

	if(event == EVENT_RESOLVE_SPELL){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_INSECT, &token);
		token.qty = 2;
		token.legacy = 1;
		token.special_code_for_legacy = &empty;
		token.special_infos = 67;
		generate_token(&token);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_cerebral_eruption(int player, int card, event_t event){
/* Cerebral Eruption	|2|R|R
 * Sorcery
 * Target opponent reveals the top card of his or her library. ~ deals damage equal to the revealed card's converted mana cost to that
 * 		player and each creature he or she controls. If a land card is revealed this way, return ~ to its owner's hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			int *deck = deck_ptr[1-player];
			if( deck[0] != -1 ){
				show_deck(0, deck, 1, "Cerebral Eruption revealed this card.", 0, 0x7375B0 );
				show_deck(1, deck, 1, "Cerebral Eruption revealed this card.", 0, 0x7375B0 );
				int cmc = get_cmc_by_internal_id(deck[0]);
				if( cmc > 0 ){
					new_damage_all(player, card, 1-player, cmc, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
				}
				if( is_what(-1, deck[0], TYPE_LAND) ){
					bounce_permanent(player, card);
					return 0;
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_chimeric_mass(int player, int card, event_t event){
	/* Chimeric Mass	|X
	 * Artifact
	 * ~ enters the battlefield with X charge counters on it.
	 * |1: Until end of turn, ~ becomes a Construct artifact creature with "This creature's power and toughness are each equal to the number of charge counters on it." */

	/* Chimeric Mass	|X
	 * Artifact
	 * ~ enters the battlefield with X charge counters on it.
	 * |1: Until end of turn, ~ becomes a Construct artifact creature with "This creature's power and toughness are each equal to the number of charge counters
	 * on it." */

	if( event == EVENT_MODIFY_COST ){
		if( check_battlefield_for_id(2, CARD_ID_GADDOCK_TEEG) ){
			infinite_casting_cost();
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		instance->info_slot = 0;
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			charge_mana(player, COLOR_COLORLESS, -1);
			if( spell_fizzled != 1 ){
				generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
				ai_modifier += (instance->info_slot < 2 ? -256 : instance->info_slot * 4);
			}
		}
	}

	enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, get_card_instance( player, card )->info_slot);

	if( event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t *instance = get_card_instance( player, card );
		int pp = instance->parent_controller, pc = instance->parent_card;
		force_a_subtype(pp, pc, SUBTYPE_CONSTRUCT);
		artifact_animation(pp, pc, pp, pc, 1, count_counters(player, card, COUNTER_CHARGE), count_counters(player, card, COUNTER_CHARGE), 0, 0);
	}

	return generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_X(1), 0, NULL, NULL);
}

int card_clone_shell(int player, int card, event_t event){
	/*
	  Clone Shell |5
	  Artifact Creature - Shapeshifter 2/2
	  Imprint -	When Clone Shell enters the battlefield, look at the top four cards of your library, exile one face down,
	  then put the rest on the bottom of your library in any order.
	  When Clone Shell dies, turn the exiled card face up. If it's a creature card, put it onto the battlefield under your control.
	*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance( player, card );
		int number = MIN(4, count_deck(player));
		if( number > 0 ){
			char msg[100] = "Select a card.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, msg);
			this_test.create_minideck = number;
			this_test.no_shuffle = 1;

			int *deck = deck_ptr[player];

			int selected = -1;
			if( player != AI ){
				selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 1, AI_MAX_CMC, -1, &this_test);
			}
			else{
				new_default_test_definition(&this_test, TYPE_CREATURE, msg);
				selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 1, AI_MAX_CMC, -1, &this_test);
				if( selected == -1 ){
					new_default_test_definition(&this_test, TYPE_ANY, msg);
					selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 1, AI_MAX_CMC, -1, &this_test);
				}
			}
			int pos = MAX(1, instance->info_slot);
			if( pos < 10 ){
				instance->targets[pos].card = deck[selected];
				instance->info_slot = pos+1;
			}
			rfg_card_in_deck(player, selected);
			number--;
			if( number > 0 ){
				put_top_x_on_bottom(player, player, number);
			}
		}
	}

	if( get_card_instance( player, card )->info_slot > 1 && this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance( player, card );
		int stored[9];
		int sc = 0;
		int i;
		for(i=1; i<instance->info_slot; i++){
			stored[sc] = instance->targets[i].card;
			sc++;
		}
		show_deck(0, stored, sc, "Cards exiled by Clone Shell", 0, 0x7375B0 );
		show_deck(1, stored, sc, "Cards exiled by Clone Shell", 0, 0x7375B0 );
		for(i=0; i<sc; i++){
			int iid = stored[i];
			if( check_rfg(player, cards_data[iid].id) && is_what(-1, iid, TYPE_CREATURE) ){
				int card_added = add_card_to_hand(player, iid);
				remove_card_from_rfg(player, cards_data[iid].id);
				put_into_play(player, card_added);
			}
		}
	}

	if( event == EVENT_CAN_ACTIVATE && ! IS_AI(player) && get_card_instance( player, card )->info_slot > 1 ){
		return 1;
	}

	if( event == EVENT_ACTIVATE ){
		card_instance_t *instance = get_card_instance( player, card );
		int stored[9];
		int sc = 0;
		int i;
		for(i=1; i<instance->info_slot; i++){
			stored[sc] = instance->targets[i].card;
			sc++;
		}
		show_deck(player, stored, sc, "Cards exiled by Clone Shell", 0, 0x7375B0 );
		spell_fizzled = 1;
	}

	return 0;
}

int card_copperhorn_scout(int player, int card, event_t event)
{
  /* Copperhorn Scout	|G
   * Creature - Elf Scout 1/1
   * Whenever ~ attacks, untap each other creature you control. */

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.not_me = 1;
	  new_manipulate_all(player, card, player, &test, ACT_UNTAP);
	}

  return 0;
}

int card_corpse_cur(int player, int card, event_t event){
	/* Corpse Cur	|4
	 * Artifact Creature - Hound 2/2
	 * Infect
	 * When ~ enters the battlefield, you may return target creature card with infect from your graveyard to your hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card with Infect.");
		this_test.keyword = KEYWORD_INFECT;
		if( new_special_count_grave(player, &this_test) && ! graveyard_has_shroud(player) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

int card_corrupted_harvester(int player, int card, event_t event)
{
  /* Corrupted Harvester	|4|B|B
   * Creature - Horror 6/3
   * |B, Sacrifice a creature: Regenerate ~. */

  if (!(land_can_be_played & LCBP_REGENERATION))
	return 0;

  if (!IS_ACTIVATING(event))
	return 0;

  card_instance_t* instance = get_card_instance(player, card);

  test_definition_t test;
  new_default_test_definition(&test, TYPE_CREATURE, "");
  if (IS_AI(player))
	test.not_me = 1;

  if (event == EVENT_CAN_ACTIVATE && CAN_ACTIVATE(player, card, MANACOST_B(1)) && new_can_sacrifice_as_cost(player, card, &test))
	return can_regenerate(player, card);

  if (event == EVENT_ACTIVATE && charge_mana_for_activated_ability(player, card, MANACOST_B(1)))
	new_sacrifice(player, card, player, SAC_AS_COST, &test);

  if (event == EVENT_RESOLVE_ACTIVATION && can_regenerate(instance->parent_controller, instance->parent_card))
	regenerate_target(instance->parent_controller, instance->parent_card);

  return 0;
}

int card_culling_dais(int player, int card, event_t event)
{
  /* Culling Dais	|2
   * Artifact
   * |T, Sacrifice a creature: Put a charge counter on ~.
   * |1, Sacrifice ~: Draw a card for each charge counter on ~. */

  if (IS_ACTIVATING(event))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");

	  enum
	  {
		CHOICE_CHARGE = 1,
		CHOICE_DRAW
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM,
						"Charge counter", can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE), 2, DLG_TAP, DLG_MANA(MANACOST0),
						"Draw cards", can_sacrifice_this_as_cost(player, card), count_counters(player, card, COUNTER_CHARGE), DLG_MANA(MANACOST_X(1)));

	  if (event == EVENT_CAN_ACTIVATE)
		return can_use_activated_abilities(player, card) && choice;
	  else if (event == EVENT_ACTIVATE)
		switch (choice)
		  {
			case CHOICE_CHARGE:
			  if (controller_sacrifices_a_permanent(player, card, TYPE_CREATURE, 0))
				tap_card(player, card);
			  break;

			case CHOICE_DRAW:
			  kill_card(player, card, KILL_SACRIFICE);
			  break;
		  }
	  else	// EVENT_RESOLVE_ACTIVATION
		switch (choice)
		  {
			case CHOICE_CHARGE:
			  add_counter(player, card, COUNTER_CHARGE);
			  break;

			case CHOICE_DRAW:
			  draw_cards(player, count_counters(player, card, COUNTER_CHARGE));
			  break;
		  }
	}

  if (event == EVENT_SHOULD_AI_PLAY)
	ai_modifier += (player == AI ? 12 : -12) * count_counters(player, card, COUNTER_CHARGE);

  return 0;
}

int card_darkslick_drake(int player, int card, event_t event){
/* Darkslick Drake	|2|U|U
 * Creature - Drake 2/4
 * Flying
 * When ~ dies, draw a card. */
/* Exultant Cultist	|2|U
 * Creature - Human Wizard 2/2
 * When ~ dies, draw a card. */
/* Feral Prowler	|1|G
 * Creature - Cat 1/3
 * When ~ dies, draw a card. */
/* Kingfisher	|3|U
 * Creature - Bird 2/2
 * Flying
 * When ~ dies, draw a card. */
/* Messenger Drake	|3|U|U
 * Creature - Drake 3/3
 * Flying
 * When ~ dies, draw a card. */
/* Palace Familiar	|1|U
 * Creature - Bird 1/1
 * Flying
 * When ~ dies, draw a card. */

	if( this_dies_trigger(player, card, event, 2) ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_darksteel_axe(int player, int card, event_t event){
	/* Darksteel Axe	|1
	 * Artifact - Equipment
	 * Indestructible
	 * Equipped creature gets +2/+0.
	 * Equip |2 */

	indestructible(player, card, event);

	return vanilla_equipment(player, card, event, 2, 2, 0, 0, 0);
}

int card_darksteel_juggernaut(int player, int card, event_t event){
	/* Darksteel Juggernaut	|5
	 * Artifact Creature - Juggernaut 100/100
	 * Indestructible
	 * ~'s power and toughness are each equal to the number of artifacts you control.
	 * ~ attacks each turn if able. */

	indestructible(player, card, event);

	attack_if_able(player, card, event);

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && player > -1 && affect_me(player, card) && ! is_humiliated(player, card) ){
		event_result += count_permanents_by_type(player, TYPE_ARTIFACT);
	}

	return 0;
}

int card_darksteel_myr(int player, int card, event_t event){
	/* Darksteel Gargoyle	|7
	 * Artifact Creature - Gargoyle 3/3
	 * Flying
	 * Indestructible (Damage and effects that say "destroy" don't destroy this creature.) */
	/* Darksteel Myr	|3
	 * Artifact Creature - Myr 0/1
	 * Indestructible */
	/* Darksteel Relic	|0
	 * Artifact
	 * Indestructible (Effects that say "destroy" don't destroy this artifact.) */
	/* Seraph of the Suns	|5|W|W
	 * Creature - Angel 4/4
	 * Flying
	 * Indestructible (Damage and effects that say "destroy" don't destroy this creature. If its toughness is 0 or less, it's still put into its owner's graveyard.) */

	indestructible(player, card, event);

	return 0;
}

int card_darksteel_sentinel(int player, int card, event_t event){
	/* Darksteel Sentinel	|6
	 * Artifact Creature - Golem 3/3
	 * Flash
	 * Vigilance
	 * Indestructible */

	indestructible(player, card, event);

	vigilance(player, card, event);

	return flash(player, card, event);
}

int card_dispense_justice(int player, int card, event_t event){
	/* Dispense Justice	|2|W
	 * Instant
	 * Target player sacrifices an attacking creature.
	 * Metalcraft - That player sacrifices two attacking creatures instead if you control three or more artifacts. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance( player, card );

			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			test.state = STATE_ATTACKING;
			test.qty = metalcraft(player, card) ? 2 : 1;

			new_sacrifice(player, card, instance->targets[0].player, SAC_NO_CANCEL | SAC_CAUSED, &test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_dissipation_field(int player, int card, event_t event){
/* Dissipation Field	|2|U|U
 * Enchantment
 * Whenever a permanent deals damage to you, return it to its owner's hand. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){

		if( event == EVENT_DEAL_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_target_card == -1 && damage->damage_target_player == player ){
					if( damage->info_slot > 0 ){
						card_instance_t *instance = get_card_instance(player, card);
						if( instance->info_slot < 0 ){
							instance->info_slot = 0;
						}
						instance->targets[instance->info_slot].player = damage->damage_source_player;
						instance->targets[instance->info_slot].card = damage->damage_source_card;
						instance->info_slot++;
					}
				}
			}
		}

		if( get_card_instance(player, card)->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
			reason_for_trigger_controller == player )
		{
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					card_instance_t *instance = get_card_instance(player, card);
					int i;
					for(i=0;i<instance->info_slot;i++){
						if( instance->targets[i].player != -1 && instance->targets[i].card != -1 &&
							in_play(instance->targets[i].player, instance->targets[i].card)
						  ){
							bounce_permanent(instance->targets[i].player, instance->targets[i].card);
						}
					}
					instance->info_slot = 0;
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_dross_hopper(int player, int card, event_t event){
	/* Dross Hopper	|1|B
	 * Creature - Insect Horror 2/1
	 * Sacrifice a creature: ~ gains flying until end of turn. */

	return generic_husk(player, card, event, TYPE_CREATURE, 0, 0, KEYWORD_FLYING, 0);
}

int card_echo_circlet(int player, int card, event_t event){
	/* Echo Circlet	|2
	 * Artifact - Equipment
	 * Equipped creature can block an additional creature each combat.
	 * Equip |1 */

	if( is_equipping(player, card) ){
		attached_creature_can_block_additional(player, card, event, 1);
	}
	return basic_equipment(player, card, event, 1);
}

int card_embersmith(int player, int card, event_t event){
	/* Embersmith	|1|R	0x2004229
	 * Creature - Human Artificer 2/1
	 * Whenever you cast an artifact spell, you may pay |1. If you do, ~ deals 1 damage to any target. */
	if( smiths(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && has_mana(player, COLOR_ANY, 1) && charge_mana_while_resolving(player, card, event, player, COLOR_COLORLESS, 1) ){
			if( new_pick_target(&td, "TARGET_ANY", 0, 1) ){
				damage_target0(player, card, 1);
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

static void destroy_creature_and_gain_life(int player, int card, int t_player, int t_card)
{
  int tgh = get_toughness(t_player, t_card);
  kill_card(t_player, t_card, KILL_DESTROY);
  gain_life(player, tgh);
}

int card_engulfing_slagwurm(int player, int card, event_t event)
{
  /* Engulfing Slagwurm	|5|G|G
   * Creature - Wurm 7/7
   * Whenever ~ blocks or becomes blocked by a creature, destroy that creature. You gain life equal to that creature's toughness. */

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card))
	instance->destroys_if_blocked |= DIFB_DESTROYS_ALL;

  if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card))
	{
	  if (player == current_turn && (instance->state & STATE_ATTACKING))
		for_each_creature_blocking_me(player, card, destroy_creature_and_gain_life, player, card);

	  if (player == 1-current_turn && (instance->state & STATE_BLOCKING))
		for_each_creature_blocked_by_me(player, card, destroy_creature_and_gain_life, player, card);
	}

  return 0;
}

int card_exsanguinate(int player, int card, event_t event){
	/* Exsanguinate	|X|B|B
	 * Sorcery
	 * Each opponent loses X life. You gain life equal to the life lost this way. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
		if( spell_fizzled != 1 ){
			if( life[player] < 6 ){
				ai_modifier += (get_card_instance( player, card )->info_slot * 12);
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL){
		card_instance_t *instance = get_card_instance( player, card );
		gain_life(player, instance->info_slot);
		lose_life(1-player, instance->info_slot);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

static void pump_ezuri_archer(int player, int card, int t_player, int t_card)
{
	pump_until_eot_merge_previous(player, card, player, card, 3, 0);
}

int card_ezuris_archers(int player, int card, event_t event){
	/* Ezuri's Archers	|G
	 * Creature - Elf Archer 1/2
	 * Reach
	 * Whenever ~ blocks a creature with flying, ~ gets +3/+0 until end of turn. */

	if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card))
	{
		if (player == 1-current_turn && check_state(player, card, STATE_BLOCKING) )
			for_each_creature_blocked_by_me(player, card, pump_ezuri_archer, player, card);
	}

	return 0;
}

int card_ferrovore(int player, int card, event_t event){
	/* Ferrovore	|2|R
	 * Creature - Beast 2/2
	 * |R, Sacrifice an artifact: ~ gets +3/+0 until end of turn. */

	test_definition_t test;
    new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");
	test.not_me = 1;

	if (event == EVENT_POW_BOOST )
	{

		int num_can_sac = max_can_sacrifice_as_cost(player, card, &test);
		if (num_can_sac <= 0)
			return 0;

		return generic_shade_amt_can_pump(player, card, 3, 0, MANACOST_R(1), num_can_sac);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 3, 0);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_R(1), 0, NULL, NULL, 0, &test, 0);
}

int card_flameborn_hellion(int player, int card, event_t event){
	/* Flameborn Hellion	|5|R
	 * Creature - Hellion 5/4
	 * Haste
	 * ~ attacks each turn if able. */

	haste(player, card, event);
	attack_if_able(player, card, event);

	return 0;
}

int card_flesh_allergy(int player, int card, event_t event){

	/* Flesh Allergy	|2|B|B
	 * Sorcery
	 * As an additional cost to cast ~, sacrifice a creature.
	 * Destroy target creature. Its controller loses life equal to the number of creatures that died this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td1) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			lose_life(instance->targets[0].player, creatures_dead_this_turn);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_SAC_CREATURE_AS_COST, &td1, "TARGET_CREATURE", 1, NULL);

}

int card_flight_spellbomb(int player, int card, event_t event){
	/* Flight Spellbomb	|1
	 * Artifact
	 * |T, Sacrifice ~: Target creature gains flying until end of turn.
	 * When ~ is put into a graveyard from the battlefield, you may pay |U. If you do, draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return som_spellbombs(player, card, event, COLOR_BLUE);
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FLYING, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_fulgent_distraction(int player, int card, event_t event){
	/* Fulgent Distraction	|2|W
	 * Instant
	 * Choose two target creatures. Tap those creatures, then unattach all Equipment from them. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				tap_card(instance->targets[i].player, instance->targets[i].card);
				equipments_attached_to_me(instance->targets[i].player, instance->targets[i].card, EATM_UNATTACH);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

int card_fume_spitter(int player, int card, event_t event){
	/* Fume Spitter	|B
	 * Creature - Horror 1/1
	 * Sacrifice ~: Put a -1/-1 counter on target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

static int furnace_celebration_effect(int player, int card, event_t event){

	//In case something is copying Pawn of Ulamog and then reverts to its original form.
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_FURNACE_CELEBRATION ){
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
				if( affected_card_controller == player &&
					is_what(affected_card_controller, affected_card, TYPE_PERMANENT) &&
					kill_code == KILL_SACRIFICE &&
					!check_special_flags(affected_card_controller, affected_card, SF_KILL_STATE_BASED_ACTION) )
				{
					if( instance->targets[11].player < 0 ){
						instance->targets[11].player = 0;
					}
					SET_BYTE0(instance->targets[11].player)++;
				}
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE );
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

			card_instance_t *instance= get_card_instance(player, card);

			int amount = BYTE0(result);
			while( amount ){
					if( has_mana(player, COLOR_ANY, 2) && can_target(&td) ){
						if( charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 2) &&
							new_pick_target(&td, "TARGET_CREATURE_OR_PLAYER", 0, 1) )
						{
							damage_target0(player, card, 2);
							instance->number_of_targets = 0;
							amount--;
						}
						else{
							break;
						}
					}
					else{
						break;
					}
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

int card_furnace_celebration(int player, int card, event_t event){
	/* Furnace Celebration	|1|R|R	0x2004265
	 * Enchantment
	 * Whenever you sacrifice another permanent, you may pay |2. If you do, ~ deals 2 damage to any target. */
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)furnace_celebration_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &furnace_celebration_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t* affected = in_play(affected_card_controller, affected_card);
		if( affected && affected_card_controller == player && affected_card != card &&
			is_what(affected_card_controller, affected_card, TYPE_PERMANENT) &&
			affected->kill_code == KILL_SACRIFICE &&
			!check_special_flags(affected_card_controller, affected_card, SF_KILL_STATE_BASED_ACTION) )
		{
			card_instance_t *instance = get_card_instance(player, card);
			if( instance->targets[11].player < 0 ){
				instance->targets[11].player = 0;
			}
			SET_BYTE0(instance->targets[11].player)++;
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

			card_instance_t *instance= get_card_instance(player, card);

			int amount = BYTE0(result);
			while( amount ){
					if( has_mana(player, COLOR_ANY, 2) && can_target(&td) ){
						if( charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 2) &&
							new_pick_target(&td, "TARGET_ANY", 0, 1) )
						{
							damage_target0(player, card, 2);
							instance->number_of_targets = 0;
							amount--;
						}
						else{
							break;
						}
					}
					else{
						break;
					}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_galvanic_blast(int player, int card, event_t event){
	/* Galvanic Blast	|R	0x200426a
	 * Instant
	 * ~ deals 2 damage to any target.
	 * Metalcraft - ~ deals 4 damage to that permanent or player instead if you control three or more artifacts. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			damage_target0(player, card, metalcraft(player, card) ? 4 : 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_geth_lord_of_the_vault(int player, int card, event_t event ){
/* Geth, Lord of the Vault	|4|B|B
 * Legendary Creature - Zombie 5/5
 * Intimidate
 * |X|B: Put target artifact or creature card with converted mana cost X from an opponent's graveyard onto the battlefield under
 *		your control tapped. Then that player puts the top X cards of his or her library into his or her graveyard. */

	check_legend_rule(player, card, event);

	intimidate(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE | TYPE_ARTIFACT, "Select a creature or artifact card.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL, 0,
															&this_test, GS_GRAVE_RECYCLER_OPP_GRAVE);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, 0, MANACOST_XB(-1, 1), 0, NULL, NULL);
		if( spell_fizzled != -1 ){
			this_test.cmc = instance->info_slot;
			if( ! select_target_from_grave_source(player, card, 1-player, 0, AI_MAX_VALUE, &this_test, 0) ){
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, 1-player, 0);
		if( selected != -1 ){
			reanimate_permanent(instance->parent_controller, instance->parent_card, 1-player, selected, REANIMATE_TAP);
			mill(1-player, instance->info_slot);
		}
	}

	return 0;
}

int card_glimmerpoint_stag(int player, int card, event_t event ){
/* Glimmerpoint Stag	|2|W|W
 * Creature - Elk 3/3
 * Vigilance
 * When ~ enters the battlefield, exile another target permanent. Return that card to the battlefield under its owner's control at
 *		the beginning of the next end step. */

	vigilance(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT );
		td.special = TARGET_SPECIAL_NOT_ME;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select another target permanent.", 0, GS_LITERAL_PROMPT) ){
			remove_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_glimmerpost(int player, int card, event_t event)
{
  /* Glimmerpost	""
   * Land - Locus
   * When ~ enters the battlefield, you gain 1 life for each Locus on the battlefield.
   * |T: Add |C to your mana pool. */

  if (comes_into_play(player, card, event))
	gain_life(player, count_subtype(ANYBODY, TYPE_LAND, SUBTYPE_LOCUS));

  return mana_producer(player, card, event);
}

static void animate_glint_hawk_idol(int player, int card){
	add_a_subtype(player, card, SUBTYPE_BIRD);
	int legacy = artifact_animation(player, card, player, card, 1, 2, 2, KEYWORD_FLYING, 0);
	card_instance_t *leg = get_card_instance( player, legacy );
	leg->targets[6].player = COLOR_TEST_WHITE;
}

int card_glint_hawk_idol(int player, int card, event_t event){
	/* Glint Hawk Idol	|2
	 * Artifact
	 * Whenever another artifact enters the battlefield under your control, you may have ~ become a 2/2 Bird artifact creature with flying until end of turn.
	 * |W: ~ becomes a 2/2 Bird artifact creature with flying until end of turn. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		test.not_me = 1;

		if( new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			animate_glint_hawk_idol(player, card);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t *instance = get_card_instance( player, card );
		animate_glint_hawk_idol(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_W(1), 0, NULL, NULL);
}

int card_goblin_gaveleer(int player, int card, event_t event){
	/* Goblin Gaveleer	|R
	 * Creature - Goblin Warrior 1/1
	 * Trample
	 * ~ gets +2/+0 for each Equipment attached to it. */

	if( event == EVENT_POWER && affect_me(player, card) && in_play(player, card) && ! is_humiliated(player, card) ){
		event_result += (equipments_attached_to_me(player, card, EATM_REPORT_TOTAL) *2);
	}

	return 0;
}

int card_golden_urn(int player, int card, event_t event){

	/* Golden Urn	|1
	 * Artifact
	 * At the beginning of your upkeep, you may put a charge counter on ~.
	 * |T, Sacrifice ~: You gain life equal to the number of charge counters on ~. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counter(player, card, COUNTER_CHARGE);
	}

	if( event == EVENT_ACTIVATE ){
		if( life[player] < 6 ){
			ai_modifier += (count_counters(player, card, COUNTER_CHARGE) * 12);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, count_counters(player, card, COUNTER_CHARGE));
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_golem_artisan(int player, int card, event_t event){
	/* Golem Artisan	|5
	 * Artifact Creature - Golem 3/3
	 * |2: Target artifact creature gets +1/+1 until end of turn.
	 * |2: Target artifact creature gains your choice of flying, trample, or haste until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.special = TARGET_SPECIAL_ARTIFACT_CREATURE;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_PUMP = 1,
		CHOICE_ABILITY
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(2), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(2), 0,
								&td, "Select target artifact creature.");
		if( spell_fizzled != 1 ){
			int priority_pump = 5;
			int priority_ability = 1;
			if( current_phase < PHASE_DECLARE_BLOCKERS ||
				(current_phase == PHASE_AFTER_BLOCKING &&
				! check_for_ability(instance->targets[0].player, instance->targets[0].card, KEYWORD_TRAMPLE)) )
			{
				priority_ability = 10;
			}
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Give +1/+1", 1, priority_pump,
								"Give flying, trample, or haste", 1, priority_ability);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			if( instance->info_slot == CHOICE_PUMP ){
				pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
												instance->targets[0].player, instance->targets[0].card, 1, 1);
			}
			if( instance->info_slot == CHOICE_ABILITY ){
				int choice = 1;
				if( current_phase < PHASE_DECLARE_ATTACKERS ){
					choice = 2;
				}
				if( current_phase < PHASE_DECLARE_BLOCKERS ){
					choice = 3;
				}
				if( ! IS_AI(player) ){
					choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
									"Give Trample", 1, 1,
									"Give Haste", 1, 1,
									"Give Flying", 1, 1);
				}
				int key = KEYWORD_TRAMPLE;
				int s_key = 0;
				if( choice == 2 ){
					key = 0;
					s_key = SP_KEYWORD_HASTE;
				}
				if( choice == 3 ){
					key = KEYWORD_FLYING;
					s_key = 0;
				}
				pump_ability_until_eot(instance->parent_controller, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card, 0, 0, key, s_key);
			}
		}
	}

	return 0;
}

int card_golem_foundry(int player, int card, event_t event){
	/* Golem Foundry	|3
	 * Artifact
	 * Whenever you cast an artifact spell, you may put a charge counter on ~.
	 * Remove three charge counters from ~: Put a 3/3 colorless Golem artifact creature token onto the battlefield. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_DUH, &test) ){
			add_counter(player, card, COUNTER_CHARGE);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_GOLEM);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_CHARGE, 3), NULL, NULL);
}

int card_golems_heart(int player, int card, event_t event){
	/* Golem's Heart	|2
	 * Artifact
	 * Whenever a player casts an artifact spell, you may gain 1 life. */

	 if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_AI(player), &test) ){
			gain_life(player, 1);
		}
	}

	return 0;
}

int card_grand_architect(int player, int card, event_t event){
	/* Grand Architect	|1|U|U
	 * Creature - Vedalken Artificer 1/3
	 * Other |Sblue creatures you control get +1/+1.
	 * |U: Target artifact creature becomes |Sblue until end of turn.
	 * Tap an untapped |Sblue creature you control: Add |C|C to your mana pool. Spend this mana only to cast artifact spells or activate abilities of artifacts. */

	if( ! IS_GAA_EVENT(event) && ! (event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		return boost_creature_by_color(player, card, event, COLOR_TEST_BLUE, 1, 1, 0, BCT_CONTROLLER_ONLY);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
	td.illegal_state = TARGET_STATE_TAPPED;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE );
	td1.preferred_controller = player;
	td1.special = TARGET_SPECIAL_ARTIFACT_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_CHANGE_COLOR
	};

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( can_target(&td) && can_use_activated_abilities(player, card) ){
			declare_mana_available(player, COLOR_ARTIFACT, 2);
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_U(1), 0, &td1, NULL) ){
			return 1;
		}
		if( can_target(&td) && can_use_activated_abilities(player, card) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int choice = CHOICE_MANA;
		if( can_target(&td) && can_use_activated_abilities(player, card) ){
			if( ! paying_mana() &&
				generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_U(1), 0, &td1, NULL) )
			{
				choice = DIALOG(player, card, event, DLG_RANDOM,
								"Tap creature for mana", 1, 1,
								"Change color of artifact creature", 1, 5);
				if( ! choice ){
					spell_fizzled = 1;
					return 0;
				}
			}
		}
		else{
			choice = CHOICE_CHANGE_COLOR;
		}
		instance->info_slot = choice;

		if( choice == CHOICE_MANA ){
			if( new_pick_target(&td, get_sleighted_color_text(player, card, "Select a %s creature to tap", COLOR_BLUE),
								0, 1 | GS_LITERAL_PROMPT) )
			{
				// see comments in tap_a_permanent_you_control_for_mana(), which this should likely be rewritten to use
				produce_mana(player, COLOR_ARTIFACT, 2);
				// This isn't actually "tapping for mana", since it doesn't have the {T} symbol, but still a mana ability, so prevent response
				tapped_for_mana_color = -2;
				if (instance->targets[0].player == player && instance->targets[0].card == card){
					instance->state |= STATE_TAPPED;
				}
				else{
					get_card_instance(instance->targets[0].player, instance->targets[0].card)->state |= STATE_TAPPED;
					dispatch_event(instance->targets[0].player, instance->targets[0].card, EVENT_TAP_CARD);
				}
			}
		}
		if( choice == CHOICE_CHANGE_COLOR ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_U(1), 0,
											&td1, "Select target artifact creature.");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_CHANGE_COLOR && valid_target(&td1) ){
			change_color(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
						get_sleighted_color_test(player, card, COLOR_TEST_BLUE), CHANGE_COLOR_SET|CHANGE_COLOR_END_AT_EOT);
		}
	}

	return 0;
}

int card_grasp_of_darkness(int player, int card, event_t event){
	/* Grasp of Darkness	|B|B
	 * Instant
	 * Target creature gets -4/-4 until end of turn. */
	return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -4, -4, 0, 0);
}

int card_grindclock(int player, int card, event_t event)
{
  /* Grindclock	|2
   * Artifact
   * |T: Put a charge counter on ~.
   * |T: Target player puts the top X cards of his or her library into his or her graveyard, where X is the number of charge counters on ~. */

  if (IS_ACTIVATING(event))
	{
	  if (event == EVENT_CAN_ACTIVATE && !can_use_activated_abilities(player, card))
		return 0;

	  int ai_priority_charge, ai_priority_mill;
	  if (event == EVENT_ACTIVATE && IS_AI(player))
		{
		  int counters = count_counters(player, card, COUNTER_CHARGE);
		  if (counters == 0)
			{
			  ai_priority_charge = 1;
			  ai_priority_mill = -1;
			}
		  else
			{
			  int cards_in_library = count_deck(1-player);
			  // 1000 - (number of turns to exhaust library, including one normal draw per turn, rounded up)
			  ai_priority_mill   = 1000 - (  (cards_in_library + counters  ) / (1 + counters  ));
			  ai_priority_charge = 1000 - (1+(cards_in_library + counters+1) / (1 + counters+1));
			}
		}
	  else
		ai_priority_charge = ai_priority_mill = 1;

	  target_definition_t td;
	  default_target_definition(player, card, &td, 0);
	  td.zone = TARGET_ZONE_PLAYERS;

	  enum
	  {
		CHOICE_CHARGE = 1,
		CHOICE_MILL
	  } choice = DIALOG(player, card, event,
						"Charge counter", 1, ai_priority_charge, DLG_TAP, DLG_MANA(MANACOST0),
						"Mill",           1, ai_priority_mill,   DLG_TAP, DLG_MANA(MANACOST0), DLG_TARGET(&td, "TARGET_PLAYER"));

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (event == EVENT_ACTIVATE)
		ai_modifier += player == AI ? 24 : -24;
	  else if (event == EVENT_RESOLVE_ACTIVATION)
		switch (choice)
		  {
			case CHOICE_CHARGE:
			  add_counter(player, card, COUNTER_CHARGE);
			  break;

			case CHOICE_MILL:
			  mill(get_card_instance(player, card)->targets[0].player, count_counters(player, card, COUNTER_CHARGE));
			  break;
		  }
	}

	return 0;
}

int card_halt_order(int player, int card, event_t event){
	/* Halt Order	|2|U
	 * Instant
	 * Counter target artifact spell.
	 * Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, &td, 0) ){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			draw_cards(player, 1);
		}
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}

int card_heavy_arbalest(int player, int card, event_t event){
	/* Heavy Arbalest	|3	0x20042b5
	 * Artifact - Equipment
	 * Equipped creature doesn't untap during its controller's untap step.
	 * Equipped creature has "|T: This creature deals 2 damage to any target."
	 * Equip |4 */
	 if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		does_not_untap(p, c, event);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_EQUIP = 1,
		CHOICE_DAMAGE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( is_equipping(player, card) ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;

			target_definition_t td1;
			default_target_definition(p, c, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			if( granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td1, NULL) ){
				return 1;
			}
		}
		return basic_equipment(player, card, event, 4);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, can_activate_basic_equipment(player, card, event, 4), 0};
		if( is_equipping(player, card) ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;

			target_definition_t td1;
			default_target_definition(p, c, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			if( granted_generic_activated_ability(player, card, p, c, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0,
				&td1, NULL) )
			{
				abils[2] = 1;
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Equip", abils[1], 1,
							"Tap equipped & damage", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 4);
		}
		if( choice == CHOICE_DAMAGE ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;

			target_definition_t td1;
			default_target_definition(p, c, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0,
												&td1, "TARGET_ANY");
			if( spell_fizzled != 1 ){
				card_instance_t *equip = get_card_instance(p, c);
				instance->targets[0] = equip->targets[0];
				instance->number_of_targets = 1;
				instance->targets[1].player = get_protections_from(p, c);
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 4);
		}
		if( instance->info_slot == CHOICE_DAMAGE ){
			target_definition_t td1;
			base_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			td1.illegal_abilities = instance->targets[1].player;
			if( valid_target(&td1) ){
				if( is_equipping(instance->parent_controller, instance->parent_card) ){
					card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
					damage_creature(instance->targets[0].player, instance->targets[0].card, 2,
									parent->damage_target_player, parent->damage_target_card);
				}
				else{
					damage_target0(player, card, 2);
				}
			}
		}
	}

	return 0;
}

int card_hoard_smelter_dragon(int player, int card, event_t event){
	/* Hoard-Smelter Dragon	|4|R|R
	 * Creature - Dragon 5/5
	 * Flying
	 * |3|R: Destroy target artifact. ~ gets +X/+0 until end of turn, where X is that artifact's converted mana cost. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int pump = get_cmc(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			return generic_shade(player, card, event, 0, MANACOST0, pump, 0, 0, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(3, 1), 0, &td, "TARGET_ARTIFACT");
}

int card_horizon_spellbomb(int player, int card, event_t event){
	/* Horizon Spellbomb	|1
	 * Artifact
	 * |2, |T, Sacrifice ~: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library.
	 * When ~ is put into a graveyard from the battlefield, you may pay |G. If you do, draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return som_spellbombs(player, card, event, COLOR_GREEN);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		tutor_basic_land(player, 0, 0);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL);
}

int card_ichor_rats(int player, int card, event_t event){
	/* Ichor Rats	|1|B|B
	 * Creature - Rat 2/1
	 * Infect
	 * When ~ enters the battlefield, each player gets a poison counter. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		poison(player, 1);
		poison(1-player, 1);
	}

	return 0;
}

int card_indomitable_archangel(int player, int card, event_t event){
	/* Indomitable Archangel	|2|W|W
	 * Creature - Angel 4/4
	 * Flying
	 * Metalcraft - Artifacts you control have shroud as long as you control three or more artifacts. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_ABILITIES && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) && metalcraft(player, card) )
		{
			event_result |= KEYWORD_SHROUD;
		}
	}

	return 0;
}

int card_inexorable_tide(int player, int card, event_t event){
	/* Inexorable Tide	|3|U|U
	 * Enchantment
	 * Whenever you cast a spell, proliferate. */

	if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		proliferate(player, card);
	}

	return global_enchantment(player, card, event);
}

int card_infiltration_lens(int player, int card, event_t event)
{
  /* Infiltration Lens	|1
   * Artifact - Equipment
   * Whenever equipped creature becomes blocked by a creature, you may draw two cards.
   * Equip |1 */

  if (event == EVENT_DECLARE_BLOCKERS && is_equipping(player, card))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int p = instance->targets[8].player;
	  int c = instance->targets[8].card;

	  int num_blockers;
	  if (current_turn == p && is_attacking(p, c))
		for (num_blockers = count_my_blockers(p, c); num_blockers > 0; --num_blockers)
		  if (!draw_some_cards_if_you_want(player, card, player, 2))
			break;
	}

  if (event == EVENT_ATTACK_RATING)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (affect_me(instance->targets[8].player, instance->targets[8].card) && is_equipping(player, card))
		ai_defensive_modifier -= 36;
	}

  return vanilla_equipment(player, card, event, 1, 0, 0, 0, 0);
}

int card_instill_infection(int player, int card, event_t event){
	/* Instill Infection	|3|B
	 * Instant
	 * Put a -1/-1 counter on target creature.
	 * Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 1);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_kembas_skyguard(int player, int card, event_t event){
	/* Kemba's Skyguard	|1|W|W
	 * Creature - Cat Knight 2/2
	 * Flying
	 * When ~ enters the battlefield, you gain 2 life. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 2);
	}

	return 0;
}

int card_kemba_kha_regent(int player, int card, event_t event){
	/* Kemba, Kha Regent	|1|W|W
	 * Legendary Creature - Cat Cleric 2/4
	 * At the beginning of your upkeep, put a 2/2 |Swhite Cat creature token onto the battlefield for each Equipment attached to ~. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int amount = equipments_attached_to_me(player, card, EATM_REPORT_TOTAL);
		generate_tokens_by_id(player, card, CARD_ID_CAT, amount);
	}
	return 0;
}

int card_kuldotha_phoenix(int player, int card, event_t event){
	/* Kuldotha Phoenix	|2|R|R|R
	 * Creature - Phoenix 4/4
	 * Flying, haste
	 * Metalcraft - |4: Return ~ from your graveyard to the battlefield. Activate this ability only during your upkeep and only if you control three or more artifacts. */

	haste(player, card, event);

	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		if( current_turn == player && has_mana(player, COLOR_COLORLESS, 4) && metalcraft(player, card) ){
			int choice = do_dialog(player, player, card, -1, -1," Return Kuldotha Phoenix to play\n Pass", 0);
			if( choice == 0 ){
				charge_mana(player, COLOR_COLORLESS, 4);
				if( spell_fizzled != 1){
					get_card_instance( player, card )->state &= ~STATE_INVISIBLE;
					put_into_play(player, card);
					return -1;
				}
			}
		}
		return -2;
	}

	return 0;
}

int card_leonine_arbiter(int player, int card, event_t event){
	/* Leonin Arbiter	|1|W
	 * Creature - Cat Cleric 2/2
	 * Players can't search libraries. Any player may pay |2 for that player to ignore this effect until end of turn. */

	// The real effect is in "get_tutoring_denial"
	if( event == EVENT_CLEANUP ){
		card_instance_t *instance = get_card_instance( player, card );
		instance->targets[2].player = 0;
		instance->targets[3].player = 0;
	}

	return 0;
}

int card_liege_of_the_tangle(int player, int card, event_t event){

	/* Liege of the Tangle	|6|G|G
	 * Creature - Elemental 8/8
	 * Trample
	 * Whenever ~ deals combat damage to a player, you may choose any number of target lands you control and put an awakening counter on each of them. Each of
	 * those lands is an 8/8 |Sgreen Elemental creature for as long as it has an awakening counter on it. They're still lands. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.illegal_type = TYPE_CREATURE;
		td.allowed_controller = player;
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		color_test_t clr = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		int choice = 0;
		if( !IS_AI(player) ){
			choice = do_dialog(player, player, card, -1, -1," Auto mode\n Manual mode", 0);
		}
		if( choice == 1 ){
			while( can_target(&td) && pick_target(&td, "TARGET_LAND") ){
					instance->number_of_targets = 0;
					add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_AWAKENING);
					add_a_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_ELEMENTAL);
					int leg = land_animation2(player, card, instance->targets[0].player, instance->targets[0].card, 4, 8,8, 0,0, clr, NULL);
					SET_BYTE0(get_card_instance(player, leg)->targets[7].player) = COUNTER_AWAKENING;
			}
		}
		else{
			int count = active_cards_count[player]-1;
			while( count > -1 ){
					if( in_play(player, count) && is_what(player, count, TYPE_LAND) && ! is_what(player, count, TYPE_CREATURE) ){
						add_counter(player, count, COUNTER_AWAKENING);
						add_a_subtype(player, count, SUBTYPE_ELEMENTAL);
						int leg = land_animation2(player, card, player, count, 4, 8, 8, 0,0, clr, NULL);
						SET_BYTE0(get_card_instance(player, leg)->targets[7].player) = COUNTER_AWAKENING;
					}
					count--;
			}
		}

	}
	return 0;
}

int card_lifesmith(int player, int card, event_t event){
	/* Lifesmith	|1|G
	 * Creature - Human Artificer 2/1
	 * Whenever you cast an artifact spell, you may pay |1. If you do, you gain 3 life. */

	if( smiths(player, card, event) ){
		if( has_mana(player, COLOR_ANY, 1) && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1) ){
			gain_life(player, 3);
		}
	}

	return 0;
}


int card_liquidmetal_coating(int player, int card, event_t event){
	/* Liquimetal Coating	|2
	 * Artifact
	 * |T: Target permanent becomes an artifact in addition to its other types until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );
	td.illegal_type = TYPE_ARTIFACT;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			turn_into_artifact(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PERMANENT");
}

int card_livewire_lash(int player, int card, event_t event){
	/* Livewire Lash	|2	0x20042fb
	 * Artifact - Equipment
	 * Equipped creature gets +2/+0 and has "Whenever this creature becomes the target of a spell, this creature deals 2 damage to any target."
	 * Equip |2 */
	if (is_equipping(player, card)){
		card_instance_t *instance = get_card_instance( player, card );

		int p = instance->targets[8].player;
		int c = instance->targets[8].card;

		if (!is_humiliated(player, card)
			&& becomes_target_of_spell(player, card, event, p, c, ANYBODY, RESOLVE_TRIGGER_MANDATORY))
		{
			get_card_instance( p, c )->number_of_targets = 0;

			target_definition_t td;
			default_target_definition(p, c, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			td.allow_cancel = 0;

			if (can_target(&td)){
				if( new_pick_target(&td, "TARGET_ANY", 0, 0) ){
					damage_creature(instance->targets[0].player, instance->targets[0].card, 2, p, c);
				}
			}
		}
	}

	return vanilla_equipment(player, card, event, 2, 2, 0, 0, 0);
}

int card_lumengrid_drake(int player, int card, event_t event){
	/* Lumengrid Drake	|3|U
	 * Creature - Drake 2/2
	 * Flying
	 * Metalcraft - When ~ enters the battlefield, if you control three or more artifacts, return target creature to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( metalcraft(player, card) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.required_state = TARGET_STATE_TAPPED;

			card_instance_t *instance = get_card_instance(player, card);

			if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_memoricide(int player, int card, event_t event){
/* Memoricide	|3|B
 * Sorcery
 * Name a nonland card. Search target player's graveyard, hand, and library for any number of cards with that name and exile them.
 * Then that player shuffles his or her library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 48;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			int csvid = -1;
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Name a nonland card.");
			test.type_flag = DOESNT_MATCH;
			if( IS_AI(player) ){
				csvid = ai_name_a_random_csvid(player, card, instance->targets[0].player, 0, &test);
			}
			else{
				csvid = new_card_from_list(player, 3, &test);
			}
			lobotomy_effect(player, instance->targets[0].player, csvid, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_melt_terrain(int player, int card, event_t event){
	/* Melt Terrain	|2|R|R
	 * Sorcery
	 * Destroy target land. ~ deals 2 damage to that land's controller. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			damage_player(instance->targets[0].player, 2, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
}

int card_molder_beast(int player, int card, event_t event){
	/* Molder Beast	|4|G
	 * Creature - Beast 5/3
	 * Trample
	 * Whenever an artifact is put into a graveyard from the battlefield, ~ gets +2/+0 until end of turn. */

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, player, TYPE_ARTIFACT, NULL);
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( result ){
		pump_until_eot_merge_previous(player, card, player, card, 2*BYTE0(result), 0);
	}

	return 0;
}

int card_molten_psyche(int player, int card, event_t event){
/* Molten Psyche	|1|R|R
 * Sorcery
 * Each player shuffles the cards from his or her hand into his or her library, then draws that many cards.
 * Metalcraft - If you control three or more artifacts, ~ deals damage to each opponent equal to the number of cards that player has
 *		drawn this turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p,{
					int num = reshuffle_hand_into_deck(p, 0);
					draw_cards(p, num);
				}
		);
		if( metalcraft(player, card) ){
			damage_player(1-player, cards_drawn_this_turn[1-player], player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_moriok_replica(int player, int card, event_t event){
/* Moriok Replica	|3
 * Artifact Creature - Warrior 2/2
 * |1|B, Sacrifice ~: You draw two cards and you lose 2 life. */

 if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 2);
		lose_life(player, 2);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XB(1, 1), 0, NULL, NULL);
}

int card_mox_opal(int player, int card, event_t event){
	/* Mox Opal	|0
	 * Legendary Artifact
	 * Metalcraft - |T: Add one mana of any color to your mana pool. Activate this ability only if you control three or more artifacts. */

	check_legend_rule(player, card, event);

	if( (event == EVENT_CAN_ACTIVATE || (event == EVENT_COUNT_MANA && affect_me(player, card))) && ! metalcraft(player, card) ){
		return 0;
	}

	return mana_producer(player, card, event);
}

int card_myr_propagator(int player, int card, event_t event){
	/* Myr Propagator	|3
	 * Artifact Creature - Myr 1/1
	 * |3, |T: Put a token that's a copy of ~ onto the battlefield. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );
		card_instance_t* parent = get_card_instance(instance->parent_controller, instance->parent_card);

		token_generation_t token;
		int iid = parent->internal_card_id;
		if (iid >= 0){
			copy_token_definition(player, card, &token, instance->parent_controller, instance->parent_card);
		} else {
			default_token_definition(player, card, cards_data[instance->backup_internal_card_id].id, &token);
			token.no_sleight = 1;
		}
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
}

int card_myr_reservoir(int player, int card, event_t event){
/* Myr Reservoir	|3
 * Artifact
 * |T: Add |C|C to your mana pool. Spend this mana only to cast Myr spells or activate abilities of Myr.
 * |3, |T: Return target Myr card from your graveyard to your hand. */

// 13/1/2017: Mana restriction usage is impossible right now in Manalink, so the first ability is uncoded


	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select target Myr card.");
	this_test.subtype = SUBTYPE_MYR;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL,
															0, &this_test, GS_GRAVE_RECYCLER);
}

int card_myrsmith(int player, int card, event_t event){
	/* Myrsmith	|1|W
	 * Creature - Human Artificer 2/1
	 * Whenever you cast an artifact spell, you may pay |1. If you do, put a 1/1 colorless Myr artifact creature token onto the battlefield. */

	if( smiths(player, card, event) ){
		if( has_mana(player, COLOR_ANY, 1) && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1) ){
			generate_token_by_id(player, card, CARD_ID_MYR);
		}
	}

	return 0;
}

int card_necrogen_censer(int player, int card, event_t event){

	/* Necrogen Censer	|3
	 * Artifact
	 * ~ enters the battlefield with two charge counters on it.
	 * |T, Remove a charge counter from ~: Target player loses 2 life. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, 2);

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 2);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_CHARGE),
									&td, "TARGET_PLAYER");
}

int card_necrogen_scudder(int player, int card, event_t event){
	/* Necrogen Scudder	|2|B
	 * Creature - Horror 3/3
	 * Flying
	 * When ~ enters the battlefield, you lose 3 life. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player == AI && life[player]-3 < 6 ){
			ai_modifier-=256;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		lose_life(player, 3);
	}

	return 0;
}

int card_necropede(int player, int card, event_t event){
	/* Festering Mummy	|B
	 * Creature - Zombie 1/1
	 * When ~ dies, you may put a -1/-1 counter on target creature. */
	/* Necropede	|2
	 * Artifact Creature - Insect 1/1
	 * Infect
	 * When ~ dies, you may put a -1/-1 counter on target creature. */

	if( this_dies_trigger(player, card, event, 2) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		card_instance_t *instance = get_card_instance( player, card );
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 1);
		}
	}

	return 0;
}

int card_neurok_invisimancer(int player, int card, event_t event){
	/* Neurok Invisimancer	|1|U|U
	 * Creature - Human Wizard 2/1
	 * ~ can't be blocked.
	 * When ~ enters the battlefield, target creature can't be blocked this turn. */

	unblockable(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_neurok_replica(int player, int card, event_t event){
	/* Neurok Replica	|3
	 * Artifact Creature - Wizard 1/4
	 * |1|U, Sacrifice ~: Return target creature to its owner's hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XU(1, 1), 0, &td, "TARGET_CREATURE");
}

int card_nihil_spellbomb(int player, int card, event_t event){
	/* Nihil Spellbomb	|1
	 * Artifact
	 * |T, Sacrifice ~: Exile all cards from target player's graveyard.
	 * When ~ is put into a graveyard from the battlefield, you may pay |B. If you do, draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return som_spellbombs(player, card, event, COLOR_BLACK);
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			rfg_whole_graveyard(instance->targets[0].player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_PLAYER");
}

static int effect_unattach_when_lose_control(int player, int card, event_t event)
{
  card_instance_t* instance;
  if (event == EVENT_CARDCONTROLLED
	  && (instance = get_card_instance(player, card))
	  && affect_me(instance->damage_target_player, instance->damage_target_card))
	{
	  unattach(instance->damage_target_player, instance->damage_target_card);
	  kill_card(player, card, KILL_EXILE);
	}

  return 0;
}

int card_ogre_geargrabber(int player, int card, event_t event)
{
  /* Ogre Geargrabber	|4|R|R
   * Creature - Ogre Warrior 4/4
   * Whenever ~ attacks, gain control of target Equipment an opponent controls until end of turn. Attach it to ~. When you lose control of that Equipment, unattach it. */

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_ARTIFACT);
	  td.allowed_controller = 1-player;
	  td.preferred_controller = 1-player;
	  td.required_subtype = SUBTYPE_EQUIPMENT;

	  card_instance_t* instance = get_card_instance(player, card);
	  instance->number_of_targets = 0;

	  if (can_target(&td) && pick_target(&td, "TARGET_EQUIPMENT"))
		{
		  gain_control_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
		  // targets[0] will have been updated during the control change.
		  equip_target_creature(instance->targets[0].player, instance->targets[0].card, player, card);
		  if (instance->targets[0].player == player)
			create_targetted_legacy_effect(player, card, &effect_unattach_when_lose_control, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_origin_spellbomb(int player, int card, event_t event){
	/* Origin Spellbomb	|1
	 * Artifact
	 * |1, |T, Sacrifice ~: Put a 1/1 colorless Myr artifact creature token onto the battlefield.
	 * When ~ is put into a graveyard from the battlefield, you may pay |W. If you do, draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return som_spellbombs(player, card, event, COLOR_WHITE);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_MYR);
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, NULL, NULL);
}


int card_oxidda_daredevil(int player, int card, event_t event){
	/* Oxidda Daredevil	|1|R
	 * Creature - Goblin Artificer 2/1
	 * Sacrifice an artifact: ~ gains haste until end of turn. */

	return generic_husk(player, card, event, TYPE_ARTIFACT, 0, 0, 0, SP_KEYWORD_HASTE);
}

int card_painful_quandary(int player, int card, event_t event){
	/* Painful Quandary	|3|B|B
	 * Enchantment
	 * Whenever an opponent casts a spell, that player loses 5 life unless he or she discards a card. */

	if( new_specific_spell_played(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		int ai_choice = 0;
		if( life[1-player]-5 < 6 && hand_count[1-player] ){
			ai_choice = 1;
		}
		int choice = do_dialog(1-player, player, card, -1, -1, " Lose 5 life\n Discard", ai_choice);
		if( choice == 0 ){
			lose_life(1-player, 5);
		}
		else{
			discard(1-player, 0, player);
		}
	}

	return global_enchantment(player, card, event);
}

int card_painsmith(int player, int card, event_t event){
	/* Painsmith	|1|B
	 * Creature - Human Artificer 2/1
	 * Whenever you cast an artifact spell, you may have target creature get +2/+0 and gain deathtouch until end of turn. */

	if( smiths(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 0, 0, SP_KEYWORD_DEATHTOUCH);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_panic_spellbomb(int player, int card, event_t event){
	/* Panic Spellbomb	|1
	 * Artifact
	 * |T, Sacrifice ~: Target creature can't block this turn.
	 * When ~ is put into a graveyard from the battlefield, you may pay |R. If you do, draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return som_spellbombs(player, card, event, COLOR_RED);
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0,
									&td, "TARGET_CREATURE");
}

int card_platinum_emperion(int player, int card, event_t event){
	/* Platinum Emperion	|8
	 * Artifact Creature - Golem 8/8
	 * Your life total can't change. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		player_bits[player] |= PB_CANT_GAIN_LIFE;
	}

	return 0;
}

int card_precursor_golem(int player, int card, event_t event){
	/* Precursor Golem	|5
	 * Artifact Creature - Golem 3/3
	 * When ~ enters the battlefield, put two 3/3 colorless Golem artifact creature tokens onto the battlefield.
	 * Whenever a player casts an instant or sorcery spell that targets only a single Golem, that player copies that spell for each other Golem that spell could target. Each copy targets a different one of those Golems. */

	return 0;
}

int card_prototype_portal(int player, int card, event_t event){
	/* Prototype Portal	|4
	 * Artifact
	 * Imprint - When ~ enters the battlefield, you may exile an artifact card from your hand.
	 * |X, |T: Put a token that's a copy of the exiled card onto the battlefield. X is the converted mana cost of that card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an artifact card to imprint.");
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

int card_psychic_miasma(int player, int card, event_t event){
	/* Psychic Miasma	|1|B
	 * Sorcery
	 * Target player discards a card. If a land card is discarded this way, return ~ to its owner's hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			ec_definition_t ec;
			default_ec_definition(instance->targets[0].player, instance->targets[0].player, &ec);

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			int result = new_effect_coercion(&ec, &this_test);
			if (result != -1 && is_what(-1, get_internal_card_id_from_csv_id(result), TYPE_LAND)){
				bounce_permanent(player, card);
			} else {
				kill_card(player, card, KILL_DESTROY);
			}
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_quicksilver_gargantuan(int player, int card, event_t event)
{
	/* Quicksilver Gargantuan	|5|U|U	0x2004382
	 * Creature - Shapeshifter 7/7
	 * You may have ~ enter the battlefield as a copy of any creature on the battlefield, except it's 7/7. */
	if (enters_the_battlefield_as_copy_of_any_creature(player, card, event))
		set_legacy_image(player, CARD_ID_QUICKSILVER_GARGANTUAN,
						 set_pt_and_abilities_until_eot(player, card, player, card, 7, 7, 0, 0, 0));

	return 0;
}

int card_razor_hippogriff(int player, int card, event_t event){
	/* Razor Hippogriff	|3|W|W
	 * Creature - Hippogriff 3/3
	 * Flying
	 * When ~ enters the battlefield, return target artifact card from your graveyard to your hand.
	 * 		You gain life equal to that card's converted mana cost. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( count_graveyard_by_type(player, TYPE_ARTIFACT) && ! graveyard_has_shroud(player) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ARTIFACT, "Select target artifact card.");
			int card_added = new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_CMC, &test);
			if( card_added != -1 ){
				gain_life(player, get_cmc(player, card_added));
			}
		}
	}
	return 0;
}

int card_relic_putrescence(int player, int card, event_t event){
	/* Relic Putrescence	|2|B
	 * Enchantment - Aura
	 * Enchant artifact
	 * Whenever enchanted artifact becomes tapped, its controller gets a poison counter. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_TAP_CARD && affect_me(p, c) ){
			poison(p, 1);
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT );

	return targeted_aura(player, card, event, &td, "TARGET_ARTIFACT");
}

int card_revoke_existence(int player, int card, event_t event){
	/* Revoke Existence	|1|W
	 * Sorcery
	 * Exile target artifact or enchantment. */
     // used externally

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT_ENCHANTMENT", 1, NULL);
}

int card_riddlesmith(int player, int card, event_t event){
	/* Riddlesmith	|1|U
	 * Creature - Human Artificer 2/1
	 * Whenever you cast an artifact spell, you may draw a card. If you do, discard a card. */

	if( smiths(player, card, event) ){
		draw_cards(player, 1);
		discard(player, 0, player);
	}

	return 0;
}

int card_rust_tick(int player, int card, event_t event){
	/* Rust Tick	|3
	 * Artifact Creature - Insect 1/3
	 * You may choose not to untap ~ during your untap step.
	 * |1, |T: Tap target artifact. It doesn't untap during its controller's untap step for as long as ~ remains tapped. */

	choose_to_untap(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_ARTIFACT");
		if( spell_fizzled != 1 ){
			ai_mod_for_tap_effects(player, card, 0);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
			does_not_untap_until_im_tapped(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
			card_instance_t *parent = get_card_instance( player, instance->parent_card );
			parent->targets[1].player = instance->targets[0].player;
			parent->targets[1].card = instance->targets[0].card;
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_ARTIFACT");
}

int card_rusted_relic(int player, int card, event_t event){
	/* Rusted Relic	|4
	 * Artifact
	 * Metalcraft - ~ is a 5/5 Golem artifact creature as long as you control three or more artifacts. */


	if( event == EVENT_CHANGE_TYPE && affect_me(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( metalcraft(player, card) ){
			int newtype = create_a_card_type(instance->internal_card_id);
			cards_at_7c7000[newtype]->type |= (cards_data[instance->internal_card_id].type | TYPE_CREATURE);
			cards_at_7c7000[newtype]->power = 5;
			cards_at_7c7000[newtype]->toughness = 5;
			instance->dummy3 = newtype;
		}
		else{
			instance->dummy3 = 0;
		}
		if( instance->dummy3 > 0 ){
			event_result = instance->dummy3;
		}
	}

	return 0;
}

int card_saberclaw_golem(int player, int card, event_t event){
	/* Saberclaw Golem	|5
	 * Artifact Creature - Golem 4/2
	 * |R: ~ gains first strike until end of turn. */

	return generic_shade(player, card, event, 0, MANACOST_R(1), 0, 0, KEYWORD_FIRST_STRIKE, 0);
}

int card_salvage_scout(int player, int card, event_t event){
	/* Salvage Scout	|W
	 * Creature - Human Scout 1/1
	 * |W, Sacrifice ~: Return target artifact card from your graveyard to your hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an artifact card.");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_SACRIFICE_ME, MANACOST_W(1), 0, NULL, NULL,
															0, &this_test, GS_GRAVE_RECYCLER);
}

int card_scrapdiver_serpent(int player, int card, event_t event)
{
  /* Scrapdiver Serpent	|5|U|U
   * Creature - Serpent 5/5
   * ~ can't be blocked as long as defending player controls an artifact. */

	if (event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card) &&
		count_permanents_by_type(1-player, TYPE_ARTIFACT) > 0 )
		unblockable(player, card, event);

	  return 0;
}

int card_screeching_silcaw(int player, int card, event_t event){
	/* Screeching Silcaw	|1|U
	 * Creature - Bird 1/2
	 * Flying
	 * Metalcraft - Whenever ~ deals combat damage to a player, if you control three or more artifacts, that player puts the top four cards of his or her library into his or her graveyard. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result && metalcraft(player, card) ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					while( times_damaged[p] ){
							mill(p, 4);
							times_damaged[p]--;
					}
				}
		);
	}

	return 0;
}

int card_seize_the_initiative(int player, int card, event_t event){
	/* Seize the Initiative	|W
	 * Instant
	 * Target creature gets +1/+1 and gains first strike until end of turn. */
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 1, 1, KEYWORD_FIRST_STRIKE, 0);
}

int card_slice_in_twain(int player, int card, event_t event){
	/* Slice in Twain	|2|G|G
	 * Instant
	 * Destroy target artifact or enchantment.
	 * Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT_ENCHANTMENT", 1, NULL);
}

int card_snapsail_glider(int player, int card, event_t event){
	/* Snapsail Glider	|3
	 * Artifact Creature - Construct 2/2
	 * Metalcraft - ~ has flying as long as you control three or more artifacts. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card) ){
		if( metalcraft(player, card)  ){
			event_result |= KEYWORD_FLYING;
		}
	}

	return 0;
}

int card_soul_parry(int player, int card, event_t event){
	/* Soul Parry	|1|W
	 * Instant
	 * Prevent all damage one or two target creatures would deal this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			if( can_target(&td) ){
				td.allow_cancel = 2; //"Done" button;
				new_pick_target(&td, "TARGET_CREATURE", 1, 0);
			}
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				negate_damage_this_turn(player, card, instance->targets[i].player, instance->targets[i].card, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_steady_progress(int player, int card, event_t event){
	/* Steady Progress	|2|U
	 * Instant
	 * Proliferate.
	 * Draw a card. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_mod_for_proliferate(player, 3);
	}

	if( event == EVENT_RESOLVE_SPELL){
		proliferate(player, card);
		draw_cards(player, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_stoic_rebuttal(int player, int card, event_t event){
	/* Stoic Rebuttal	|1|U|U
	 * Instant
	 * Metalcraft - ~ costs |1 less to cast if you control three or more artifacts.
	 * Counter target spell. */

	if( event == EVENT_MODIFY_COST ){
		if( metalcraft(player, card) ){
			COST_COLORLESS--;
		}
	}
	return card_counterspell(player, card, event);
}

int card_strata_scythe(int player, int card, event_t event){
	/* Strata Scythe	|3
	 * Artifact - Equipment
	 * Imprint - When ~ enters the battlefield, search your library for a land card, exile it, then shuffle your library.
	 * Equipped creature gets +1/+1 for each land on the battlefield with the same name as the exiled card.
	 * Equip |3 */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);
		int result[2][60];
		int rc = 0;
		int k;
		CYCLE_ALL_CARDS(p, c,	{
									if( in_play(p, c) && is_what(p, c, TYPE_LAND) ){
										int csvid = get_card_name(p, c);
										int found = 0;
										for(k=0; k<rc; k++){
											if( result[0][k] == csvid ){
												result[1][k]++;
												found = 1;
												break;
											}
										}
										if( ! found ){
											result[0][k] = csvid;
											result[1][k] = 1;
										}
									}
								}
		);
		if( rc > 1 ){
			for(k=0; k<(rc-1); k++){
				if( result[1][k] < result[1][k+1] ){
					int transit = result[1][k];
					result[1][k] = result[1][k+1];
					result[1][k+1] = transit;
					transit = result[0][k];
					result[0][k] = result[0][k+1];
					result[0][k+1] = transit;
				}
			}
		}
		int csvid = -1;
		if( ! IS_AI(player) || rc >= 1 ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Select a land card to imprint.");

			if( ! IS_AI(player) ){
				csvid = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_FIRST_FOUND, &test);
			}
			else{
				k = 0;
				while( k < rc && csvid == -1 ){
						test.id = result[0][k];
						int selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_FIRST_FOUND, -1, &test);
						if( selected > -1 ){
							csvid = cards_data[deck_ptr[player][selected]].id;
							rfg_card_in_deck(player, selected);
						}
				}
				shuffle(player);
			}
		}
		if( csvid > -1 ){
			int pos = MAX(1, instance->info_slot);
			if( pos < 10 ){
				instance->targets[pos].card = csvid;
				instance->info_slot = pos+1;
				create_card_name_legacy(player, card, csvid);
			}
		}
	}

	if( in_play(player, card) && is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			int i;
			for(i=1; i<instance->info_slot; i++){
				event_result += count_permanents_by_name(ANYBODY, TYPE_LAND, instance->targets[i].card);
			}
		}
	}

	return basic_equipment(player, card, event, 3);

}

int card_strider_harness(int player, int card, event_t event){
	/* Strider Harness	|3
	 * Artifact - Equipment
	 * Equipped creature gets +1/+1 and has haste.
	 * Equip |1 */

	return vanilla_equipment(player, card, event, 1, 1, 1, 0, SP_KEYWORD_HASTE);
}

int card_sunblast_angel(int player, int card, event_t event){
	/* Sunblast Angel	|4|W|W
	 * Creature - Angel 4/5
	 * Flying
	 * When ~ enters the battlefield, destroy all tapped creatures. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.state = STATE_TAPPED;
		APNAP(p, { new_manipulate_all(player, card, p, &test, KILL_DESTROY); };);
	}

	return 0;
}

int card_sunspear_shikari(int player, int card, event_t event){
	/* Sunspear Shikari	|1|W
	 * Creature - Cat Soldier 2/2
	 * As long as ~ is equipped, it has first strike and lifelink. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && !is_humiliated(player, card) && in_play(player, card) &&
		equipments_attached_to_me(player, card, EATM_CHECK) )
	{
		lifelink(player, card, event);
		event_result |= KEYWORD_FIRST_STRIKE;
	}

	return 0;
}

int card_sylvok_lifestaff(int player, int card, event_t event){
	/* Sylvok Lifestaff	|1
	 * Artifact - Equipment
	 * Equipped creature gets +1/+0.
	 * Whenever equipped creature dies, you gain 3 life.
	 * Equip |1 */

	if( in_play(player, card) && is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(p, c) && is_this_dying(p, c) ){
			get_card_instance( player, card )->targets[11].player = 66;
		}
	}

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		gain_life(player, 3);
	}

	return vanilla_equipment(player, card, event, 1, 1, 0, 0, 0);
}

int card_sylvok_replica(int player, int card, event_t event){
	/* Sylvok Replica	|3
	 * Artifact Creature - Shaman 1/3
	 * |G, Sacrifice ~: Destroy target artifact or enchantment. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}
	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_G(1), 0, &td, "DISENCHANT");
}

int card_tangle_angler(int player, int card, event_t event){
	/* Tangle Angler	|3|G
	 * Creature - Horror 1/5
	 * Infect
	 * |G: Target creature blocks ~ this turn if able. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			target_must_block_me(instance->parent_controller, instance->parent_card,
								instance->targets[0].player, instance->targets[0].card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, "TARGET_CREATURE");
}

static int has_damaged_player(int player, int card){
	card_instance_t *instance = get_card_instance(player, card);
	if( instance->targets[1].player > 0 ){
		return instance->targets[1].player;
	}
	return 0;
}

int card_steel_hellkite(int player, int card, event_t event){
/*
Steel Hellkite |6
Artifact Creature - Dragon 5/5
Flying
{2}: Steel Hellkite gets +1/+0 until end of turn.
{X}: Destroy each nonland permanent with converted mana cost X whose controller was dealt combat damage by Steel Hellkite this turn.
	  Activate this ability only once each turn.
*/

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(2), 1, 0);
	}

	check_damage(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS, player, card);

	if( event == EVENT_CLEANUP ){
		card_instance_t *instance = get_card_instance( player, card );
		instance->targets[1].player = instance->targets[1].card = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_PUMP = 1,
		CHOICE_WIPEOUT
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		int prev_choice = 0;
		if( instance->targets[1].card > -1 ){
			prev_choice = BYTE1(instance->targets[1].card);
		}
		int result = has_damaged_player(player, card);
		if( result && !(prev_choice & CHOICE_WIPEOUT) ){
			if( IS_AI(player) ){
				if( (player == 0 && ! BYTE0(result)) || (player == 1 && ! BYTE1(result)) ){
					if( generic_activated_ability(player, card, event, 0, MANACOST_X(get_most_common_cmc_nonland(1-player)), 0, NULL, NULL) ){
						return 1;
					}
				}
			}
			else{
				if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
					return 1;
				}
			}
		}
		return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(2), 1, 0);
	}

	if( event == EVENT_ACTIVATE ){
		if( instance->targets[1].card < 0 ){
			instance->targets[1].card = 0;
		}
		int abils[3] = {0, generic_shade_merge_pt(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(2), 1, 0), 0};
		int prev_choice = 0;
		if( instance->targets[1].card > -1 ){
			prev_choice = BYTE1(instance->targets[1].card);
		}
		int result = has_damaged_player(player, card);
		if( result && !(prev_choice & CHOICE_WIPEOUT) ){
			if( IS_AI(player) ){
				if( (player == 0 && ! BYTE0(result)) || (player == 1 && ! BYTE1(result)) ){
					if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(get_most_common_cmc_nonland(1-player)), 0,
						NULL, NULL) )
					{
						abils[2] = 1;
					}
				}
			}
			else{
				if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, 0, NULL, NULL) ){
					abils[2] = 1;
				}
			}
		}
		int priority_pump = current_turn == player && is_attacking(player, card) && current_phase == PHASE_AFTER_BLOCKING ? 10 : 0;
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Pump", abils[1], priority_pump,
							"Wipe out permanents", abils[2], 5);
		SET_BYTE0(instance->targets[1].card) = choice;
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_PUMP ){
			return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(2), 1, 0);
		}
		if( choice == CHOICE_WIPEOUT ){
			instance->info_slot = 0;
			if( ! IS_AI(player) ){
				if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
					charge_mana(player, COLOR_COLORLESS, -1);
					if( spell_fizzled != 1 ){
						SET_BYTE1(instance->targets[1].card) = choice;
						instance->info_slot = x_value;
					}
				}
			}
			else{
				int ai_value = get_most_common_cmc_nonland(1-player);
				if( charge_mana_for_activated_ability(player, card, MANACOST_X(ai_value)) ){
					SET_BYTE1(instance->targets[1].card) = choice;
					instance->info_slot = ai_value;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = BYTE0(instance->targets[1].card);
		if( choice == CHOICE_PUMP ){
			return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(2), 1, 0);
		}
		if( choice == CHOICE_WIPEOUT ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			this_test.type_flag = DOESNT_MATCH;
			this_test.cmc = instance->info_slot;
			int result = has_damaged_player(player, card);
			APNAP(p,{
						if( (p == 0 && BYTE0(result)) || (p == 1 && BYTE1(result)) )
						{
							new_manipulate_all(player, card, p, &this_test, KILL_DESTROY);
						};
					};
			);
		}
	}

	return 0;
}

int card_tel_jilad_defiance(int player, int card, event_t event){
	/* Tel-Jilad Defiance	|1|G
	 * Instant
	 * Target creature gains protection from artifacts until end of turn.
	 * Draw a card. */

	if( event == EVENT_RESOLVE_SPELL){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		card_instance_t *instance = get_card_instance( player, card );
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
									0, 0, KEYWORD_PROT_ARTIFACTS, 0);
			draw_a_card(player);
		}
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return vanilla_instant_pump(player, card, event, ANYBODY, player, 0, 0, KEYWORD_PROT_ARTIFACTS, 0);
}

int card_tower_of_calamities(int player, int card, event_t event){
	/* Tower of Calamities	|4
	 * Artifact
	 * |8, |T: ~ deals 12 damage to target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td)  ){
			damage_target0(player, card, 12);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(8), 0, &td, "TARGET_CREATURE");
}

int card_trigon_of_corruption(int player, int card, event_t event)
{
  /* Trigon of Corruption	|4
   * Artifact
   * ~ enters the battlefield with three charge counters on it.
   * |B|B, |T: Put a charge counter on ~.
   * |2, |T, Remove a charge counter from ~: Put a -1/-1 counter on target creature. */

  target_definition_t td;
  if (IS_ACTIVATING(event))
	default_target_definition(player, card, &td, TYPE_CREATURE);

  switch (trigon(player, card, event, COLOR_BLACK, "-1/-1 counter", &td, "TARGET_CREATURE"))
	{
	  case 1:	return 1;
	  case 2:
		if (valid_target(&td))
		  {
			card_instance_t* instance = get_card_instance(player, card);
			add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_M1_M1);
		  }
	}
  return 0;
}

int card_trigon_of_infestation(int player, int card, event_t event)
{
  /* Trigon of Infestation	|4
   * Artifact
   * ~ enters the battlefield with three charge counters on it.
   * |G|G, |T: Put a charge counter on ~.
   * |2, |T, Remove a charge counter from ~: Put a 1/1 |Sgreen Insect creature token with infect onto the battlefield. */

  switch (trigon(player, card, event, COLOR_GREEN, "Insect token", NULL, NULL))
	{
	  case 1:	return 1;
	  case 2:
		;token_generation_t token;
		default_token_definition(player, card, CARD_ID_INSECT, &token);
		token.special_infos = 67;
		generate_token(&token);
	}
  return 0;
}

int card_trigon_of_mending(int player, int card, event_t event)
{
  /* Trigon of Mending	|2
   * Artifact
   * ~ enters the battlefield with three charge counters on it.
   * |W|W, |T: Put a charge counter on ~.
   * |2, |T, Remove a charge counter from ~: Target player gains 3 life. */

  target_definition_t td;
  if (IS_ACTIVATING(event))
	{
	  default_target_definition(player, card, &td, 0);
	  td.zone = TARGET_ZONE_PLAYERS;
	  td.preferred_controller = player;
	}

  switch (trigon(player, card, event, COLOR_WHITE, "Gain life", &td, "TARGET_PLAYER"))
	{
	  case 1:	return 1;
	  case 2:
		if (valid_target(&td))
		  gain_life(get_card_instance(player, card)->targets[0].player, 3);
	}
  return 0;
}

int card_trigon_of_rage(int player, int card, event_t event)
{
  /* Trigon of Rage	|2
   * Artifact
   * ~ enters the battlefield with three charge counters on it.
   * |R|R, |T: Put a charge counter on ~.
   * |2, |T, Remove a charge counter from ~: Target creature gets +3/+0 until end of turn. */

  target_definition_t td;
  if (IS_ACTIVATING(event))
	{
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.preferred_controller = player;
	}

  if (event == EVENT_CHECK_PUMP && CAN_ACTIVATE(player, card, MANACOST_X(2)) && CAN_TAP(player, card) && count_counters(player, card, COUNTER_CHARGE))
	pumpable_power[player] += 3;

  switch (trigon(player, card, event, COLOR_RED, "+3/+0 until end of turn", &td, "TARGET_CREATURE"))
	{
	  case 1:	return 1;
	  case 2:
		if (valid_target(&td))
		  {
			card_instance_t* instance = get_card_instance(player, card);
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3,0);
		  }
	}
  return 0;
}

int card_trigon_of_thought(int player, int card, event_t event)
{
  /* Trigon of Thought	|5
   * Artifact
   * ~ enters the battlefield with three charge counters on it.
   * |U|U, |T: Put a charge counter on ~.
   * |2, |T, Remove a charge counter from ~: Draw a card. */

  switch (trigon(player, card, event, COLOR_BLUE, "Draw a card", NULL, NULL))
	{
	  case 1:	return 1;
	  case 2:
		draw_a_card(player);
	}
  return 0;
}

int card_true_conviction(int player, int card, event_t event)
{
  /* True Conviction	|3|W|W|W
   * Enchantment
   * Creatures you control have double strike and lifelink. */

  if (event == EVENT_ABILITIES
	  && affected_card_controller == player
	  && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
	  && in_play(player, card) && !is_humiliated(player, card))
	{
	  event_result |= KEYWORD_DOUBLE_STRIKE;
	  lifelink(affected_card_controller, affected_card, event);
	}

  return global_enchantment(player, card, event);
}

int card_tunnel_ignus(int player, int card, event_t event){
/* Tunnel Ignus	|1|R
 * Creature - Elemental 2/1
 * Whenever a land enters the battlefield under an opponent's control, if that player had another land enter the battlefield under
 * 		his or her control this turn, ~ deals 3 damage to that player. */

	etb_ability_global(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_LAND, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		if( get_trap_condition(1-player, TRAP_LANDS_PLAYED) > 1 ){
			damage_player(1-player, 3, player, card);
		}
	}

	return 0;
}

int card_turn_aside(int player, int card, event_t event){
	/* Turn Aside	|U
	 * Instant
	 * Counter target spell that targets a permanent you control. */

	if( event == EVENT_CAN_CAST ){
		int result = card_counterspell(player, card, event);
		if( result > 0 ){
			card_instance_t *instance = get_card_instance( card_on_stack_controller, card_on_stack );
			int i;
			for(i=0; i<instance->number_of_targets; i++){
				if( instance->targets[i].player == player && instance->targets[i].card != -1 &&
					in_play(instance->targets[i].player, instance->targets[i].card) &&
					is_what(instance->targets[i].player, instance->targets[i].card, TYPE_PERMANENT) )
				{
					return result;
				}
			}
		}
		return 0;
	}

	return card_counterspell(player, card, event);
}

int card_turn_to_slag(int player, int card, event_t event){
	/* Turn to Slag	|3|R|R
	 * Sorcery
	 * ~ deals 5 damage to target creature. Destroy all Equipment attached to that creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			equipments_attached_to_me(instance->targets[0].player, instance->targets[0].card, EATM_DESTROY);
			damage_target0(player, card, 5);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_twisted_image(int player, int card, event_t event)
{
  /* Twisted Image	|U
   * Instant
   * Switch target creature's power and toughness until end of turn.
   * Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_SPELL)
	{
		if (valid_target(&td))
		{
			switch_power_and_toughness_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_untamed_might(int player, int card, event_t event){
	/* Untamed Might	|X|G
	 * Instant
	 * Target creature gets +X/+X until end of turn. */

	if( event == EVENT_CHECK_PUMP ){
		int amount = get_card_instance( player, card )->info_slot;
		return vanilla_instant_pump(player, card, event, ANYBODY, player, amount, amount, 0, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
							instance->info_slot, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_CREATURE", 1, NULL);
}

int card_vault_skyward(int player, int card, event_t event){
	/* Vault Skyward	|U
	 * Instant
	 * Target creature gains flying until end of turn. Untap it. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FLYING, 0);
			untap_card(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_CREATURE", 1, NULL);
}

int card_vedalken_certarch(int player, int card, event_t event){
	/* Vedalken Certarch	|U
	 * Creature - Vedalken Wizard 1/1
	 * Metalcraft - |T: Tap target artifact, creature, or land. Activate this ability only if you control three or more artifacts. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_ARTIFACT | TYPE_LAND );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE && metalcraft(player, card) ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TWIDDLE");
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TWIDDLE");
		if( spell_fizzled != 1 ){
			ai_mod_for_tap_effects(player, card, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td)  ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_vensers_journal(int player, int card, event_t event){
	/* Venser's Journal	|5
	 * Artifact
	 * You have no maximum hand size.
	 * At the beginning of your upkeep, you gain 1 life for each card in your hand. */

	if( current_turn == player && event == EVENT_MAX_HAND_SIZE && ! is_humiliated(player, card) ){
		event_result = 99;
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		gain_life(player, hand_count[player]);
	}

	return 0;
}

static int vigil_for_the_lost_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_VIGIL_FOR_THE_LOST ){
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
				count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			if( BYTE1(result) && has_mana(player, COLOR_ANY, 1) &&
				charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, -1) )
			{
				gain_life(player, x_value);
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

int card_vigil_for_the_lost(int player, int card, event_t event){
	/* Vigil for the Lost	|3|W
	 * Enchantment
	 * Whenever a creature you control dies, you may pay |X. If you do, you gain X life. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)vigil_for_the_lost_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &vigil_for_the_lost_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			if( BYTE1(result) && has_mana(player, COLOR_ANY, 1) &&
				charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, -1) )
			{
				gain_life(player, x_value);
			}
		}
	}

	return global_enchantment(player, card, event);
}

static int viridian_revel_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_VIRIDIAN_REVEL ){
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
				if( get_owner(affected_card_controller, affected_card) == 1-player ){
					count_for_gfp_ability(player, card, event, ANYBODY, TYPE_ARTIFACT, NULL);
				}
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			draw_some_cards_if_you_want(player, card, player, BYTE0(result));
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

int card_viridian_revel(int player, int card, event_t event){
	/* Viridian Revel	|1|G|G
	 * Enchantment
	 * Whenever an artifact is put into an opponent's graveyard from the battlefield, you may draw a card. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)viridian_revel_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &viridian_revel_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( get_owner(affected_card_controller, affected_card) == 1-player ){
			count_for_gfp_ability(player, card, event, ANYBODY, TYPE_ARTIFACT, NULL);
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			draw_cards(player, BYTE0(result));
		}
	}

	return global_enchantment(player, card, event);
}

int card_volition_reins(int player, int card, event_t event)
{
  /* Volition Reins	|3|U|U|U
   * Enchantment - Aura
   * Enchant permanent
   * When ~ enters the battlefield, if enchanted permanent is tapped, untap it.
   * You control enchanted permanent. */

  if (comes_into_play(player, card, event))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->damage_target_player >= 0 && instance->damage_target_card >= 0
		  && in_play(instance->damage_target_player, instance->damage_target_card)
		  && is_tapped(instance->damage_target_player, instance->damage_target_card))
		untap_card(instance->damage_target_player, instance->damage_target_card);
	}

  return card_confiscate(player, card, event);
}

int card_vulshok_heartstoker(int player, int card, event_t event){
	/* Vulshok Heartstoker	|2|R
	 * Creature - Human Shaman 2/2
	 * When ~ enters the battlefield, target creature gets +2/+0 until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 0);
		}
	}

	return 0;
}

int card_vulshok_replica(int player, int card, event_t event){
	/* Vulshok Replica	|3	0x2004463
	 * Artifact Creature - Berserker 3/1
	 * |1|R, Sacrifice ~: ~ deals 3 damage to target player or planeswalker. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 3);
		}
	}
	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, "TARGET_PLAYER_OR_PLANESWALKER");
}

int card_wall_of_tanglecord(int player, int card, event_t event){
	/* Wall of Tanglecord	|2
	 * Artifact Creature - Wall 0/6
	 * Defender
	 * |G: ~ gains reach until end of turn. */

	return generic_shade(player, card, event, 0, MANACOST_G(1), 0, 0, KEYWORD_REACH, 0);
}

int card_whitesuns_passage(int player, int card, event_t event){
	/* Whitesun's Passage	|1|W
	 * Instant
	 * You gain 5 life. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( life[player] < 6 ){
			ai_modifier += 96;
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		gain_life(player, 5);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_wing_puncture(int player, int card, event_t event){
	/* Wing Puncture	|G
	 * Instant
	 * Target creature you control deals damage equal to its power to target creature with flying. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.allowed_controller = player;
	td.preferred_controller = player;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE );
	td1.required_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature your control.", 1, NULL);
		if( spell_fizzled != 1 ){
			new_pick_target(&td1, "Select target creature with flying.", 1, 1 | GS_LITERAL_PROMPT);
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td1, 1) ){
			int amount = get_power(instance->targets[0].player, instance->targets[0].card);
			damage_creature(instance->targets[1].player, instance->targets[1].card, amount,
							instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_withstand_death(int player, int card, event_t event)
{
  /* Withstand Death	|G
   * Instant
   * Target creature gains indestructible until end of turn. */

  return vanilla_instant_pump(player, card, event, ANYBODY, player, 0,0, 0,SP_KEYWORD_INDESTRUCTIBLE);
}

int card_palladium_myr(int player, int card, event_t event){
	/* Kozilek's Channeler	|5
	 * Creature - Eldrazi 4/4
	 * |T: Add |C|C to your mana pool. */
	/* Palladium Myr	|3
	 * Artifact Creature - Myr 2/2
	 * |T: Add |C|C to your mana pool. */
	/* Ulvenwald Abomination	(Transforms from Ulvenwald Captive)
	 * Creature - Eldrazi Werewolf 4/6
	 * |T: Add |C|C to your mana pool. */
	/* Weaver of Currents	|1|G|U
	 * Creature - Naga Druid 2/2
	 * |T: Add |C|C to your mana pool. */

	return mana_producing_creature(player, card, event, 18, COLOR_COLORLESS, 2);
}
