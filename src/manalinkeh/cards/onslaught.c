#include "manalink.h"

// Functions
int fetchland(int player, int card, event_t event, subtype_t subtype1, subtype_t subtype2, int lifecost){

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, lifecost, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		if( (!lifecost || life[player] > 6) ){
			ai_modifier+=1000;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, lifecost, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, get_hacked_land_text(player, card, "Select %a or %s card.", subtype1, subtype2));
		this_test.subtype = get_hacked_subtype(player, card, subtype1);
		this_test.sub2 = get_hacked_subtype(player, card, subtype2);
		this_test.subtype_flag = F2_MULTISUBTYPE;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
	}

	return 0;
}

static int crown(int player, int card, event_t event, int pow, int tou, int key, int s_key)
{
  card_instance_t* instance;

  if (event == EVENT_CAN_ACTIVATE)
	return (instance = in_play(player, card)) && instance->damage_target_card >= 0 && CAN_ACTIVATE0(player, card) && can_sacrifice_this_as_cost(player, card);

  if (event == EVENT_ACTIVATE && charge_mana_for_activated_ability(player, card, MANACOST0))
	{
	  instance = get_card_instance(player, card);
	  instance->targets[0].player = instance->damage_target_player;
	  instance->targets[0].card = instance->damage_target_card;
	  kill_card(player, card, KILL_SACRIFICE);
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  instance = get_card_instance(player, card);
	  int p, c;
	  int marked[2][151] = {{0}};
	  if (in_play(instance->targets[0].player, instance->targets[0].card))
		{
		  for (p = 0; p <= 1; ++p)
			for (c = 0; c < active_cards_count[p]; ++c)
			  if (in_play(p, c) && is_what(p, c, TYPE_CREATURE)
				  && ((c == instance->targets[0].card && p == instance->targets[0].player)
					  || shares_creature_subtype(instance->targets[0].player, instance->targets[0].card, p, c)))
				marked[p][c] = 1;

		  for (p = 0; p <= 1; ++p)
			for (c = 0; c < active_cards_count[p]; ++c)
			  if (marked[p][c] && in_play(p, c))
				pump_ability_until_eot(player, card, p, c, pow, tou, key, s_key);
		}
	}

  return generic_aura(player, card, event, player, pow, tou, key, s_key, 0, 0, 0);
}

static void turn_this_face_down(int unused1, int unused2, int player, int card){
	turn_face_down(player, card);
}

void check_for_turned_face_up_card_interactions(int player, int card, int new_identity){
		// What would have been so terrible with making this a trigger instead of trying to simulate one, badly?
		card_instance_t *instance = get_card_instance(player, card);
		int i, count;
		for(i=0;i<2;i++){
			for (count = 0; count < active_cards_count[i]; ++count){
					if( in_play(i, count) && is_what(i, count, TYPE_PERMANENT) && ! is_humiliated(i, count) ){
						int csvid = get_id(i, count);
						switch( csvid ){
								case CARD_ID_APHETTO_RUNECASTER:
									draw_some_cards_if_you_want(i, count, i, 1);
									break;
								case CARD_ID_PINE_WALKER:
								{
									if( player == i ){
										untap_card(player, card);
									}
									break;
								}
								case CARD_ID_UNBLINKING_BLEB:
								      scry(player, 2);
								      break;
								case CARD_ID_TRAIL_OF_MYSTERY:
								{
									if( player == i && is_what(-1, new_identity, TYPE_CREATURE) ){
										pump_until_eot(i, count, player, card, 2, 2);
									}
									break;
								}
								case CARD_ID_SALT_ROAD_AMBUSHERS:
									if (player == i && is_what(-1, new_identity, TYPE_CREATURE)){
										add_1_1_counters(player, card, 2);
									}
									break;
								case CARD_ID_SECRET_PLANS:
								{
									if( player == i ){
										draw_cards(player, 1);
									}
									break;
								}
								case CARD_ID_MASTERY_OF_THE_UNSEEN:
								{
									if( player == i ){
										gain_life(player, count_subtype(player, TYPE_CREATURE, -1));
									}
									break;
								}
								case CARD_ID_TEMUR_WAR_SHAMAN:
								{
									if( player == i && is_what(-1, new_identity, TYPE_CREATURE) ){
										target_definition_t td;
										default_target_definition(player, card, &td, TYPE_CREATURE);
										td.allowed_controller = 1-player;
										td.preferred_controller = 1-player;

										instance->number_of_targets = 0;

										if( can_target(&td) ){
											if( do_dialog(player, player, card, -1, -1, " Make this creature Fight\n Pass", 1) == 0 ){
												if( new_pick_target(&td, "Select target creature you don't control.", 0, GS_LITERAL_PROMPT) ){
													fight(player, card, instance->targets[0].player, instance->targets[0].card);
												}
											}
										}
									}
									break;
								}
								default:
									break;
						}
					}
			}
		}

		int iid_deathmist_raptor = get_internal_card_id_from_csv_id(CARD_ID_DEATHMIST_RAPTOR);
		const int* gy = get_grave(player), *gysrc = graveyard_source[player];
		int grafdigger_checked = 0, max_gysrc_id = next_graveyard_source_id;
		for (i = 0; i < 500 && gy[0] != -1; ++i){
			if (gy[i] == iid_deathmist_raptor
				&& ((gysrc[i] & 0x00FFFE00) >> 9) < max_gysrc_id){	// Don't consider any cards put into the graveyard since we started checking
				if (!grafdigger_checked){
					grafdigger_checked = 1;
					if (check_battlefield_for_id(2, CARD_ID_GRAFDIGGERS_CAGE)){
						break;
					}
				}
				switch (DIALOG(player, card, EVENT_ACTIVATE,
							   DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_FULLCARD_ID(iid_deathmist_raptor),
							   "Return face-up",		1, 4,
							   "Return face_down",	1, has_mana_multi(player, MANACOST_XG(4,1)) ? 8 : 2,
							   "Decline",				1, 1)){
					case 1:
						if (reanimate_permanent(player, -1, player, i, REANIMATE_DEFAULT) != -1){
							--i;
						}
						break;
					case 2:
						if (reanimate_permanent_with_function(player, -1, player, i, REANIMATE_DEFAULT, turn_this_face_down) != -1){
							--i;
						}
						break;
				}
			}
		}
}

void flip_card(int player, int card){

	card_instance_t *instance = get_card_instance(player, card);

	int new_identity = get_internal_card_id_from_csv_id( instance->targets[12].player );
	if( instance->targets[12].player != -1 ){
		check_for_turned_face_up_card_interactions(player, card, new_identity);

		instance->targets[12].card = new_identity;

		dispatch_event_to_single_card_overriding_function(player, card, EVENT_TURNED_FACE_UP, new_identity);

		// See also cloning_and_verify_legend(), which should parallel this.
		instance->regen_status |= KEYWORD_RECALC_CHANGE_TYPE;
		get_abilities(player, card, EVENT_CHANGE_TYPE, -1);

		if( ! is_what(player, card, TYPE_CREATURE) ){
			remove_from_combat(player, card) ;
		}
		if (in_play(player, card)){
			verify_legend_rule(player, card, get_id(player, card));
		}
	}
}

int card_face_down_creature(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	int flip = 0;

	// illusionary mask code
	if( instance->targets[12].player > available_slots-1 ){
		if( is_tapped(player, card) || (instance->state & STATE_ATTACKING) ){
			flip = 1;
		}
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = 2;
		td.extra = damage_card;
		td.required_type = 0;
		if( event == EVENT_CAN_ACTIVATE && can_target(&td) ){
			// see if any damage is coming my way
			int p;
			for(p=0;p<2;p++){
				int count = 0;
				while(count < active_cards_count[p]){
					instance->targets[0].player = p;
					instance->targets[0].card = count;
					if( would_valid_target(&td) ){
						card_instance_t *damage = get_card_instance(p, count);
						if( damage->damage_target_card == card && damage->damage_target_player == player && damage->damage_source_player == player && damage->info_slot > 0 ){
							flip = 1;
							return 0x63;
						}
					}
					count++;
				}
			}
		}
		else if( event == EVENT_ACTIVATE ){
			flip = 1;
		}
	}
	else{
		if( event == EVENT_CAN_ACTIVATE && instance->targets[12].player > -1 ){
			return dispatch_event_to_single_card_overriding_function(player, card, EVENT_CAN_UNMORPH,
																	get_internal_card_id_from_csv_id(instance->targets[12].player));
		}
		else if( event == EVENT_ACTIVATE ){
				dispatch_event_to_single_card_overriding_function(player, card, EVENT_UNMORPH,
																  get_internal_card_id_from_csv_id(instance->targets[12].player));
				if( spell_fizzled != 1 ){
					flip = 1;
					instance->state |= STATE_DONT_RECOPY_ONTO_STACK;
				}
		}
	}

	// turn the card face up
	if( flip == 1 ){
		flip_card(player, card);
	}

	return 0;
}

// Beware, this only works for cards that originally had morph, cards with an ixidron_legacy attached, and cards that call can_exist_while_face_down().
void turn_face_down(int player, int card){
	if (player >= 0 && card >= 0){
		card_instance_t* instance = get_card_instance(player, card);
		if (instance->internal_card_id == -1){
		  return;	// card not in play, not on stack, not in a hand
		}
		instance->targets[12].player = get_original_id(player, card);
		instance->targets[12].card = get_internal_card_id_from_csv_id(CARD_ID_FACE_DOWN_CREATURE);
		/* Force immediate change of type and color calculation.  The letting it fall through to morph(), as it would on cards natively with morph, would only
		 * do the former.  Set internal_card_id directly instead of going through EVENT_CHANGE_TYPE, since (especially if this is a Vesuvan Shapeshifter) this
		 * may temporarily be a creature type that doesn't have morph. */
		instance->regen_status |= KEYWORD_RECALC_SET_COLOR;
		instance->backup_internal_card_id = instance->internal_card_id = instance->targets[12].card;
		get_abilities(player, card, EVENT_SET_COLOR, -1);
	}
}

int casting_permanent_with_morph(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	int cless = played_for_free(player, card) ? 0 : get_updated_casting_cost(player, -1, get_internal_card_id_from_csv_id(CARD_ID_FACE_DOWN_CREATURE), event, 3);
	cless = MAX(0, cless);
	int choice = 0;
	if( has_mana(player, COLOR_COLORLESS, cless) ){
		if( played_for_free(player, card) || has_mana_to_cast_iid(player, event, instance->internal_card_id) ){
			if( ! played_for_free(player, card) ){
				choice = do_dialog(player, player, card, -1, -1, " Play face-down\n Play normally\n Cancel", played_for_free(player, card));
			}
			else{
				choice = do_dialog(player, player, card, -1, -1, " Play face-down\n Play normally", played_for_free(player, card));
			}

		}
	}
	else{
		choice = 1;
	}

	if( choice == 0 ){
		charge_mana(player, COLOR_COLORLESS, cless);
		if( spell_fizzled != 1 ){
			instance->targets[12].player = get_id(player, card);
			instance->targets[12].card = get_internal_card_id_from_csv_id( CARD_ID_FACE_DOWN_CREATURE );
			instance->targets[16].card = 0;

			// Normally these aren't done until the card is in play, but we need it to display the updated values on the stack as well.
			instance->regen_status |= KEYWORD_RECALC_CHANGE_TYPE|KEYWORD_RECALC_ABILITIES|KEYWORD_RECALC_SET_COLOR|KEYWORD_RECALC_POWER|KEYWORD_RECALC_TOUGHNESS;
			get_abilities(player, card, EVENT_CHANGE_TYPE, -1);
			get_abilities(player, card, EVENT_SET_COLOR, -1);	// usually this happens before change type
			get_abilities(player, card, EVENT_ABILITIES, -1);
			get_abilities(player, card, EVENT_TOUGHNESS, -1);
			get_abilities(player, card, EVENT_POWER, -1);
			return 2;
		}
	}

	if( choice == 1 ){
		if( ! played_for_free(player, card) ){
			charge_mana_from_id(player, card, event, get_id(player, card));
		}
		if( spell_fizzled != 1 &&
			( get_id(player, card) == CARD_ID_BLISTERING_FIRECAT ||
			 get_id(player, card) == CARD_ID_FLEDGLING_MAWCOR
			)
		  ){
			instance->targets[1].player = 66;
		}
		return 1;
	}
	return 0;
}

// A card without morph that has a turn-self-face-down ability.
int can_exist_while_face_down(int player, int card, event_t event)
{
	if (event == EVENT_CHANGE_TYPE && affect_me(player, card)){
		card_instance_t* inst = get_card_instance(player, card);
		if (inst->targets[12].card != -1){
			event_result = inst->targets[12].card;
		}
	}
	return 0;
}

int morph(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white){

	if( event == EVENT_HAS_MORPH ){
		return 1;
	}

	// show the face down creature when we are morphed
	if (event == EVENT_CHANGE_TYPE && affect_me(player, card)){
		card_instance_t* instance = get_card_instance(player, card);
		if (instance->targets[12].card != -1){
			event_result = instance->targets[12].card;
		}
	}

	if( event == EVENT_MODIFY_COST ){
		int cless = get_updated_casting_cost(player, -1, get_internal_card_id_from_csv_id(CARD_ID_FACE_DOWN_CREATURE), event, 3);
		cless = MAX(0, cless);
		card_instance_t* instance = get_card_instance(player, card);
		if( has_mana(player, COLOR_COLORLESS, cless) ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) && get_card_instance(player, card)->info_slot == 1){
			if( ! casting_permanent_with_morph(player, card, event) ){
				spell_fizzled = 1;
			}
		}
	}

	else if ( event == EVENT_CAN_UNMORPH && has_mana_multi(player, colorless, black, blue, green, red, white) ){
			  return 1;
	}
	else if( event == EVENT_UNMORPH ){
			 charge_mana_multi(player, colorless, black, blue, green, red, white);
	}
	return 0;
}

void double_faced_card(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[12].card > 0 && event == EVENT_CHANGE_TYPE && affect_me(player, card) ){
		event_result = instance->targets[12].card;
		instance->regen_status |= KEYWORD_RECALC_CHANGE_TYPE|KEYWORD_RECALC_ABILITIES;
	}
	if (event == EVENT_CAN_SKIP_TURN){
	   instance->regen_status |= KEYWORD_RECALC_CHANGE_TYPE|KEYWORD_RECALC_ABILITIES;
	   get_abilities(player, card, EVENT_CHANGE_TYPE, -1);
	   get_abilities(player, card, EVENT_ABILITIES, -1);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		instance->targets[13].player = get_id(player, card);
		instance->targets[13].card = get_id(player, card);
	}
}

// Cards
int card_accursed_centaur(int player, int card, event_t event){
/* CARD_ID_ACCURSED_CENTAUR	6037
Accursed Centaur	|B
Creature - Zombie Centaur 2/2
When ~ enters the battlefield, sacrifice a creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	return 0;
}

int card_aether_charge(int player, int card, event_t event){
	/* Aether Charge	|4|R	0x2009125
	 * Enchantment
	 * Whenever a Beast enters the battlefield under your control, you may have it deal 4 damage to target opponent or planeswalker. */
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_BEAST;

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td_opw;
		opponent_or_pw_target_definition(player, card, &td_opw);
		if (can_target(&td_opw) && pick_target(&td_opw, "TARGET_OPPONENT_OR_PLANESWALKER")){
			damage_target0(player, card, 4);
			get_card_instance(player, card)->number_of_targets = 0;
		}
	}

	return global_enchantment(player, card, event);
}

int card_aggravated_assault(int player, int card, event_t event){
/* CARD_ID_AGGRAVATED_ASSAULT	6038
Aggravated Assault	|2|R
Enchantment
|3|R|R: Untap all creatures you control. After this main phase, there is an additional combat phase followed by an additional main phase.
Activate this ability only any time you could cast a sorcery. */

	if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XR(3, 2), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(instance->parent_controller, instance->parent_card, player, &this_test, ACT_UNTAP);

		get_an_additional_combat_phase();
	}

	return global_enchantment(player, card, event);
}

int card_airborne_aid(int player, int card, event_t event){
/* CARD_ID_AIRBORNE_AID	6039
Airborne Aid	|3|U
Sorcery
Draw a card for each Bird on the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, count_subtype(2, TYPE_PERMANENT, SUBTYPE_BIRD));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_akromas_blessing(int player, int card, event_t event){
/* CARD_ID_AKROMAS_BLESSING	6041
Akroma's Blessing	|2|W
Instant
Choose a color. Creatures you control gain protection from the chosen color until end of turn.
Cycling |W (|W, Discard this card: Draw a card.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 0, 0, select_a_protection(player) | KEYWORD_RECALC_SET_COLOR, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return cycling(player, card, event, MANACOST_W(1));
}

int card_akromas_vengeance(int player, int card, event_t event){
/* CARD_ID_AKROMAS_VENGEANCE	6042
Akroma's Vengeance	|4|W|W
Sorcery
Destroy all artifacts, creatures, and enchantments.
Cycling |3 (|3, Discard this card: Draw a card.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT | TYPE_CREATURE | TYPE_ENCHANTMENT);
		APNAP(p, { new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return cycling(player, card, event, MANACOST_X(3));
}

int card_ancestors_prophet(int player, int card, event_t event){
/*
Ancestor's Prophet |4|W
Creature — Human Cleric 1/5
Tap five untapped Clerics you control: You gain 10 life.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);
	td1.illegal_state = TARGET_STATE_TAPPED;
	td1.required_subtype = SUBTYPE_CLERIC;
	td1.preferred_controller = td1.allowed_controller = player;
	td1.illegal_abilities = 0;

	if(event == EVENT_ACTIVATE ){
		if( life[player] < 6 && current_turn == 1-player && current_phase >= PHASE_AFTER_BLOCKING ){
			ai_modifier += 96;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 10);
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, &td1, 5);
}

int card_aphetto_dredging(int player, int card, event_t event){
/* CARD_ID_APHETTO_DREDGING	6047
Aphetto Dredging	|3|B
Sorcery
Return up to three target creature cards of the creature type of your choice from your graveyard to your hand. */

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		int subt = select_a_subtype(player, card);

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, get_subtype_text("Select up to three %s creature cards.", subt));
		this_test.subtype = subt;
		this_test.subtype_flag = MATCH;

		if (new_special_count_grave(player, &this_test) == 0 || graveyard_has_shroud(player)){
			ai_modifier -= 48;
		}

		card_instance_t* instance = get_card_instance(player, card);
		instance->info_slot = select_multiple_cards_from_graveyard(player, player, 0, AI_MAX_VALUE, &this_test, 3, &instance->targets[0]);
	}

	if (event == EVENT_RESOLVE_SPELL){
		card_instance_t* instance = get_card_instance(player, card);
		int i;
		for (i = 0; i < instance->info_slot; ++i){
			int selected = validate_target_from_grave(player, card, player, i);
			if (selected != -1){
				from_grave_to_hand(player, selected, TUTOR_HAND);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_aphetto_grifter(int player, int card, event_t event){
/*
Aphetto Grifter |2|U
Creature — Human Wizard 1/1
Tap two untapped Wizards you control: Tap target permanent.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);
	td1.illegal_state = TARGET_STATE_TAPPED;
	td1.required_subtype = SUBTYPE_WIZARD;
	td1.preferred_controller = player;
	td1.allowed_controller = player;
	td1.illegal_abilities = 0;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td2) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td2, "TARGET_PERMANENT",
															&td1, 2);
}

int card_arcanis_the_omnipotent(int player, int card, event_t event){
/* CARD_ID_ARCANIS_THE_OMNIPOTENT	6050
Arcanis the Omnipotent	|3|U|U|U
Legendary Creature - Wizard 3/4
|T: Draw three cards.
|2|U|U: Return ~ to its owner's hand. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_DRAW = 1,
		CHOICE_BOUNCE
	};

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_XU(2, 2), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XU(2, 2), 0, NULL, NULL)
		};
		int priority_draw = 1;
		if( (current_turn == 1-player && current_phase >= PHASE_AFTER_BLOCKING) || count_subtype(player, TYPE_LAND, -1) >= 6){
			priority_draw = 10;
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Tap & draw 3", abils[1], priority_draw,
							"Return to Hand", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_DRAW ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
		if( choice == CHOICE_BOUNCE ){
			return generic_activated_ability(player, card, event, 0, MANACOST_XU(2, 2), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DRAW ){
			draw_cards(player, 3);
		}
		if( instance->info_slot == CHOICE_BOUNCE ){
			if( in_play(instance->parent_controller, instance->parent_card) ){
				bounce_permanent( instance->parent_controller, instance->parent_card );
			}
		}
	}

	return 0;
}

int card_astral_slide(int player, int card, event_t event){
/* CARD_ID_ASTRAL_SLIDE	6053
Astral Slide	|2|W
Enchantment
Whenever a player cycles a card, you may exile target creature.
If you do, return that card to the battlefield under its owner's control at the beginning of the next end step. */

	return global_enchantment(player, card, event);
}

// activate Astral Slide
void activate_astral_slide(){

	int p,c;
	for(p=0;p<2;p++){
		for(c=0;c<active_cards_count[p];c++){
			if( in_play(p, c) && CARD_ID_ASTRAL_SLIDE == get_id(p, c) ){
				target_definition_t td;
				default_target_definition(p, c, &td, TYPE_CREATURE );
				td.allow_cancel = 0;
				td.preferred_controller = 1-p;

				int ai_choice = 0;

				if( p == AI ){
					td.allowed_controller = 1-p;
					if( ! can_target(&td) ){
						ai_choice = 1;
					}
					td.allowed_controller = 2;
				}

				if( can_target(&td) ){
					int choice = do_dialog(p, p, c, -1, -1, " Activate Astral Slide\n Pass", ai_choice);
					if( choice == 0 ){
						if( pick_target(&td, "TARGET_CREATURE") ){
							card_instance_t *instance = get_card_instance(p, c);
							instance->number_of_targets = 1;
							remove_until_eot(p, c, instance->targets[0].player, instance->targets[0].card);
						}
					}
				}
			}
			if( in_play(p, c) && CARD_ID_INVIGORATING_BOON == get_id(p, c) ){
				target_definition_t td;
				default_target_definition(p, c, &td, TYPE_CREATURE);
				td.preferred_controller = p;

				card_instance_t *instance = get_card_instance(p, c);

				if( pick_target(&td, "TARGET_CREATURE") ){
					instance->number_of_targets = 1;
					add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
				}
			}
			if( in_play(p, c) && CARD_ID_LIGHTNING_RIFT == get_id(p, c) ){
				/* Lightning Rift	|1|R	0x2009238
				 * Enchantment
				 * Whenever a player cycles a card, you may pay |1. If you do, ~ deals 2 damage to any target. */
				target_definition_t td;
				default_target_definition(p, c, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
				td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
				td.preferred_controller = 1-p;

				card_instance_t *instance = get_card_instance(p, c);

				int ai_choice = 0;

				if( p == AI ){
					td.allowed_controller = 1-p;
					if( ! can_target(&td) ){
						ai_choice = 1;
					}
					td.allowed_controller = 2;
				}

			   if( can_target(&td) ){
					if( has_mana(p, COLOR_ANY, 1) ){
					    int choice = do_dialog(p, p, c, -1, -1, " Activate Lightning Rift\n Pass", ai_choice);
					    if( choice == 0 ){
						    charge_mana(p, COLOR_COLORLESS, 1);
						    if( pick_target(&td, "TARGET_ANY") ){
							    instance->number_of_targets = 1;
							    damage_creature(instance->targets[0].player, instance->targets[0].card, 2, p, c);
						    }
					    }
					}
				}
			}
		}
	}
}

int card_aura_extraction(int player, int card, event_t event){
/* CARD_ID_AURA_EXTRACTION	6055
Aura Extraction	|1|W
Instant
Put target enchantment on top of its owner's library.
Cycling |2 (|2, Discard this card: Draw a card.) */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return cycling(player, card, event, MANACOST_X(2));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT", 1, NULL);
}

int card_aurification(int player, int card, event_t event)
{
  /* Aurification	|2|W|W
   * Enchantment
   * Whenever a creature deals damage to you, put a gold counter on it.
   * Each creature with a gold counter on it is a Wall in addition to its other creature types and has defender.
   * When ~ leaves the battlefield, remove all gold counters from all creatures. */

   if( in_play(player, card) && is_humiliated(player, card) )
	   return 0;

  card_instance_t* damage = damage_being_dealt(event);
  if (damage
	  && damage->damage_target_card == -1 && damage->damage_target_player == player && !damage_is_to_planeswalker(damage)
	  && (damage->targets[3].player & TYPE_CREATURE)
	  && in_play(damage->damage_source_player, damage->damage_source_card))
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  if (instance->number_of_targets < 10)
		{
		  instance->targets[instance->number_of_targets].player = damage->damage_source_player;
		  instance->targets[instance->number_of_targets].card = damage->damage_source_card;
		  ++instance->number_of_targets;
		}
	  else	// no more room in targets; just add the counter now
		add_counter(damage->damage_source_player, damage->damage_source_card, COUNTER_GOLD);
	}

  if (trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (event == EVENT_TRIGGER && instance->number_of_targets > 0)
		event_result |= RESOLVE_TRIGGER_MANDATORY;
	  if (event == EVENT_RESOLVE_TRIGGER)
		{
		  int i;
		  for (i = 0; i < instance->number_of_targets; ++i)
			add_counter(instance->targets[i].player, instance->targets[i].card, COUNTER_GOLD);
		  instance->number_of_targets = 0;
		}
	}

  // Wall subtype added by a hack in has_subtype(), rather than add_a_subtype(), so that it follows moved gold counters.

  if (event == EVENT_ABILITIES && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
	  && count_counters(affected_card_controller, affected_card, COUNTER_GOLD))
	event_result |= KEYWORD_DEFENDER;

  if (leaves_play(player, card, event))
	{
	  int p, c;
	  for (p = 0; p <= 1; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if (in_play(p, c) && is_what(p, c, TYPE_CREATURE))
			remove_all_counters(p, c, COUNTER_GOLD);
	}

  return global_enchantment(player, card, event);
}

int card_avarax(int player, int card, event_t event){
/*CARD_ID_AVARAX	6057
Avarax	|3|R|R
Creature - Beast 3/3
Haste
When ~ enters the battlefield, you may search your library for a card named ~, reveal it, and put it into your hand. If you do, shuffle your library.
|1|R: ~ gets +1/+0 until end of turn.*/

	haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.id = get_card_name(player, card);
		scnprintf(this_test.message, 100, "Select a card named %s.", cards_ptr[get_card_name(player, card)]->name);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);
	}

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_XR(1, 1), 1, 0);
}

int card_aven_brigadier(int player, int card, event_t event){
/* CARD_ID_AVEN_BRIGADIER	6058
Aven Brigadier	|3|W|W|W
Creature - Bird Soldier 3/5
Flying
Other Bird creatures get +1/+1.
Other Soldier creatures get +1/+1. */

	boost_creature_type(player, card, event, SUBTYPE_BIRD, 1, 1, 0, 0);
	boost_creature_type(player, card, event, SUBTYPE_SOLDIER, 1, 1, 0, 0);
	return 0;
}

int card_barren_moor(int player, int card, event_t event){
/* CARD_ID_BARREN_MOOR	6063
Barren Moor	""
Land
~ enters the battlefield tapped.
|T: Add |B to your mana pool.
Cycling |B (|B, Discard this card: Draw a card.) */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return cycling(player, card, event, MANACOST_B(1));
	}

	comes_into_play_tapped(player, card, event);

	return mana_producer(player, card, event);
}

int card_battlefield_medic(int player, int card, event_t event){
/* CARD_ID_BATTLEFIELD_MEDIC	6065
Battlefield Medic	|1|W
Creature - Human Cleric 1/1
|T: Prevent the next X damage that would be dealt to target creature this turn, where X is the number of Clerics on the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, count_subtype(2, TYPE_PERMANENT, SUBTYPE_CLERIC));
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST0, 0,
									&td, "TARGET_DAMAGE");
}

int card_biorhythm(int player, int card, event_t event){
/* CARD_ID_BIORHYTHM	1510
Biorhythm	|6|G|G
Sorcery
Each player's life total becomes the number of creatures he or she controls. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int amount = count_subtype(player, TYPE_CREATURE, -1);
		if( amount > 1 && ! count_subtype(1-player, TYPE_CREATURE, -1) ){
			ai_modifier += 256;
		}
		if( life[player] < amount ){
			ai_modifier += ((amount-life[player]) * 4);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		set_life_total(player, count_permanents_by_type(player, TYPE_CREATURE));
		set_life_total(1-player, count_permanents_by_type(1-player, TYPE_CREATURE));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_birchlore_rangers(int player, int card, event_t event){
/* CARD_ID_BIRCHLORE_RANGERS	6066
Birchlore Rangers	|G
Creature - Elf Druid 1/1
Tap two untapped Elves you control: Add one mana of any color to your mana pool.
Morph |G (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	// See also card_heritage_druid() in morningtide.c.
	if( IS_GAA_EVENT(event) || (event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.required_subtype = SUBTYPE_ELF;
		td.illegal_state = TARGET_STATE_TAPPED;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( event == EVENT_CAN_ACTIVATE && can_produce_mana(player, card) && target_available(player, card, &td) > 1 ){
			return 1;
		}

		if( event == EVENT_ACTIVATE){
			if( tapsubtype_ability(player, card, 2, &td) ){
				instance->number_of_targets = 2;
				// See comments in tap_a_permanent_you_control_for_mana(), which ideally should be generalized to handle this card.
				produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1);
				tapped_for_mana_color = -2;
				/* EVENT_TAP_CARD already sent for both elves.  It'll get sent twice for this if this happens to be one of the ones chosen; but it's not worth the
				 * effort to fix. */
			}
			else{
				spell_fizzled = 1;
			}
		}

		if( event == EVENT_COUNT_MANA && affect_me(player, card) && can_produce_mana(player, card) ){
			int count = target_available(player, card, &td);
			if (count > 2){
				/* Same issues and solution as card_axebane_guardian() in return_to_ravnica.c and permanents_you_control_can_tap_for_mana_all_one_color() in
				 * produce_mana.c. */
				count /= 2;
				int i;
				for (i = 0; i < 5; ++i){
					if ((count + i) >= 5){
						declare_mana_available_hex(player, COLOR_TEST_ANY_COLORED, (count + i) / 5);
					}
				}
			}
		}
	}

	if (event == EVENT_ATTACK_RATING && affect_me(player, card)){
		ai_defensive_modifier += 24;
	}
	if (event == EVENT_BLOCK_RATING && affect_me(player, card)){
		ai_defensive_modifier -= 24;
	}

	return morph(player, card, event, MANACOST_G(1));
}

int card_blatant_thievery(int player, int card, event_t event){
	/*
	  Blatant Thievery |4|U|U|U
	  Sorcery
	  For each opponent, gain control of target permanent that player controls.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = 1-player;

	if (event == EVENT_RESOLVE_SPELL)
	{
		if( valid_target(&td) )
		{
			card_instance_t* instance = get_card_instance(player, card);
			gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}


	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
}

int card_blistering_firecat(int player, int card, event_t event){
/*
Blistering Firecat |1|R|R|R
Creature — Elemental Cat 7/1
Trample, haste
At the beginning of the end step, sacrifice Blistering Firecat.
Morph {R}{R} (You may cast this card face down as a 2/2 creature for {3}. Turn it face up any time for its morph cost.)
*/

	if( event == EVENT_TURNED_FACE_UP ){
		get_card_instance(player, card)->targets[1].player = 66;
	}

	if( get_card_instance(player, card)->targets[1].player == 66 && ! is_humiliated(player, card) ){
		haste(player, card, event);
		if( eot_trigger(player, card, event) && ! check_state(player, card, STATE_OUBLIETTED) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return morph(player, card, event, MANACOST_R(2));
}

int card_bloodline_shaman(int player, int card, event_t event){

	/* Bloodline Shaman	|1|G
	 * Creature - Elf Wizard Shaman 1/1
	 * |T: Choose a creature type. Reveal the top card of your library. If that card is a creature card of the chosen type, put it into your hand. Otherwise,
	 * put it into your graveyard. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int subt = select_a_subtype_full_choice(instance->parent_controller, instance->parent_card, instance->parent_controller, NULL, 0);
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			reveal_card_iid(player, card, deck[0]);
			if( is_what(-1, deck[0], TYPE_CREATURE) && has_subtype(-1, deck[0], subt) ){
				add_card_to_hand(player, deck[0]);
				remove_card_from_deck(player, 0);
			}
			else{
				mill(player, 1);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_NONSICK, MANACOST0, 0, NULL, NULL);
}

int card_bloodstained_mire(int player, int card, event_t event){
	/* Bloodstained Mire	""
	 * Land
	 * |T, Pay 1 life, Sacrifice ~: Search your library for |Ha Swamp or |H2Mountain card and put it onto the battlefield. Then shuffle your library. */
	return fetchland(player, card, event, SUBTYPE_SWAMP, SUBTYPE_MOUNTAIN, 1);
}

int card_boneknitter(int player, int card, event_t event){
/* CARD_ID_BONEKNITTER	6071
Boneknitter	|1|B
Creature - Zombie Cleric 1/1
|1|B: Regenerate target Zombie.
Morph |2|B (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.preferred_controller = player;
		td.required_subtype = SUBTYPE_ZOMBIE;
		td.required_state = TARGET_STATE_DESTROYED;
		if( player == AI ){
			td.special = TARGET_SPECIAL_REGENERATION;
		}

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_ACTIVATE && ( land_can_be_played & LCBP_REGENERATION) ){
			if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XB(1, 1), 0, &td, NULL) ){
				return 0x63;
			}
		}

		if( event == EVENT_ACTIVATE ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XB(1, 1), 0,
											&td, "Select target Zombie to regenerate.");
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
				regenerate_target(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return morph(player, card, event, MANACOST_XB(2, 1));
}

int card_brightstone_ritual(int player, int card, event_t event){
/* CARD_ID_BRIGHTSTONE_RITUAL	6073
Brightstone Ritual	|R
Instant
Add |R to your mana pool for each Goblin on the battlefield. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_GOBLIN) * 8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		produce_mana(player, COLOR_RED, count_subtype(2, TYPE_PERMANENT, SUBTYPE_GOBLIN));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}


int card_broodhatch_nantuko(int player, int card, event_t event){
/* CARD_ID_BROODHATCH_NANTUKO	6074
Broodhatch Nantuko	|1|G
Creature - Insect Druid 1/1
Whenever ~ is dealt damage, you may create that many 1/1 |Sgreen Insect creature tokens.
Morph |2|G (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	int m = morph(player, card, event, MANACOST_XG(2, 1));
	if (event == EVENT_CAN_UNMORPH){
		return m;
	}

	return card_saber_ants(player, card, event);
}

int card_cabal_archon(int player, int card, event_t event){
/* CARD_ID_CABAL_ARCHON	6076
Cabal Archon	|2|B
Creature - Human Cleric 2/2
|B, Sacrifice a Cleric: Target player loses 2 life and you gain 2 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Cleric to sacrifice.");
	test.subtype = SUBTYPE_CLERIC;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_ACTIVATE ){
		if( life[1-player] < 6 || life[player] < 6 ){
			ai_modifier += 48;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 2);
			gain_life(player, 2);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST_B(1), 0, &td, "TARGET_PLAYER",
															0, &test, 0);
}

int card_catapult_master(int player, int card, event_t event){
/* CARD_ID_CATAPULT_MASTER	6080
Catapult Master	|3|W|W
Creature - Human Soldier 3/3
Tap five untapped Soldiers you control: Exile target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);
	td1.illegal_state = TARGET_STATE_TAPPED;
	td1.required_subtype = SUBTYPE_SOLDIER;
	td1.preferred_controller = player;
	td1.allowed_controller = player;
	td1.illegal_abilities = 0;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td2) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td2, "TARGET_CREATURE",
															&td1, 5);
}

int card_catapult_squad(int player, int card, event_t event){
/* CARD_ID_CATAPULT_SQUAD	6081
Catapult Squad	|1|W
Creature - Human Soldier 2/1
Tap two untapped Soldiers you control: ~ deals 2 damage to target attacking or blocking creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);
	td1.illegal_state = TARGET_STATE_TAPPED;
	td1.required_subtype = SUBTYPE_SOLDIER;
	td1.preferred_controller = player;
	td1.allowed_controller = player;
	td1.illegal_abilities = 0;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.required_state = TARGET_STATE_IN_COMBAT;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td2) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
															&td2, "Select target attacking or blocking creature.", &td1, 2);
}

int card_centaur_glade(int player, int card, event_t event){
/* Centaur Glade	|3|G|G
 * Enchantment
 * |2|G|G: Put a 3/3 |Sgreen Centaur creature token onto the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_CENTAUR);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XG(2, 2), 0, NULL, NULL);
}

int card_choking_tethers(int player, int card, event_t event){
	/* Choking Tethers	|3|U
	 * Instant
	 * Tap up to four target creatures.
	 * Cycling |1|U
	 * When you cycle ~, you may tap target creature. */

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND  ){
		return cycling(player, card, event, MANACOST_XU(1, 1));
	}

	if( event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XU(1, 1));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		draw_a_card(player);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				tap_card(instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}


	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 4, NULL);
}

int card_contested_cliffs(int player, int card, event_t event){
	/* Contested Cliffs	""
	 * Land
	 * |T: Add |1 to your mana pool.
	 * |R|G, |T: Target Beast creature you control fights target creature an opponent controls. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.allowed_controller = 1-player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE );
	td2.preferred_controller = player;
	td2.allowed_controller = player;
	td2.required_subtype = SUBTYPE_BEAST;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_FIGHT
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() && can_target(&td) &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_RG(1, 1), 0, &td2, NULL) )
		{
			choice = CHOICE_FIGHT;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_FIGHT && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Beast fight", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_FIGHT ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT,
										MANACOST_RG(1, 1), 0, &td2, "Select target Beast creature you control.");
			if( spell_fizzled != 1 ){
				new_pick_target(&td, "Select a creature your opponent controls.", 1, 1 | GS_LITERAL_PROMPT);
			}
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_FIGHT ){
			if( validate_target(player, card, &td2, 0) && validate_target(player, card, &td, 1) ){
				fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player, instance->targets[1].card);
			}
		}
	}

	return 0;
}

int card_convalescent_care(int player, int card, event_t event){
/* CARD_ID_CONVALESCENT_CARE	6093
Convalescent Care	|1|W|W
Enchantment
At the beginning of your upkeep, if you have 5 or less life, you gain 3 life and draw a card. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( life[player] < 6 ){
			gain_life(player, 3);
			draw_cards(player, 1);
		}
	}

	return global_enchantment(player, card, event);
}

int card_cover_of_darkness(int player, int card, event_t event){
/* CARD_ID_COVER_OF_DARKNESS	6094
Cover of Darkness	|1|B
Enchantment
As ~ enters the battlefield, choose a creature type.
Creatures of the chosen type have fear. (They can't be blocked except by artifact creatures and/or black creatures.) */

	card_instance_t *instance = get_card_instance( player, card );
	if( event == EVENT_RESOLVE_SPELL ){
		instance->targets[1].card = select_a_subtype(player, card);
	}

	return card_intimidation(player, card, event);
}

int card_crown_of_ascension(int player, int card, event_t event){
/* CARD_ID_CROWN_OF_ASCENSION	6097
Crown of Ascension	|1|U
Enchantment - Aura
Enchant creature
Enchanted creature has flying.
Sacrifice ~: Enchanted creature and other creatures that share a creature type with it gain flying until end of turn. */

	return crown(player, card, event, 0, 0, KEYWORD_FLYING, 0);
}

int card_crown_of_awe(int player, int card, event_t event){
/* CARD_ID_CROWN_OF_AWE	6098
Crown of Awe	|1|W
Enchantment - Aura
Enchant creature
Enchanted creature has protection from |Sblack and from |Sred.
Sacrifice ~: Enchanted creature and other creatures that share a creature type with it gain protection from |Sblack and from |Sred until end of turn. */

	return crown(player, card, event, 0, 0, KEYWORD_PROT_BLACK+KEYWORD_PROT_RED, 0);
}

int card_crown_of_fury(int player, int card, event_t event){
/* CARD_ID_CROWN_OF_FURY	6099
Crown of Fury	|1|R
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+0 and has first strike.
Sacrifice ~: Enchanted creature and other creatures that share a creature type with it get +1/+0 and gain first strike until end of turn. */

	return crown(player, card, event, 1, 0, KEYWORD_FIRST_STRIKE, 0);
}

int card_crown_of_suspicion(int player, int card, event_t event){
/* CARD_ID_CROWN_OF_SUSPICION	6100
Crown of Suspicion	|1|B
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/-1.
Sacrifice ~: Enchanted creature and other creatures that share a creature type with it get +2/-1 until end of turn. */

	return crown(player, card, event, 2, -1, 0, 0);
}

int card_crown_of_vigor(int player, int card, event_t event){
/* CARD_ID_CROWN_OF_VIGOR	6101
Crown of Vigor	|1|G
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+1.
Sacrifice ~: Enchanted creature and other creatures that share a creature type with it get +1/+1 until end of turn. */

	return crown(player, card, event, 1, 1, 0, 0);
}

int card_cruel_revival(int player, int card, event_t event){

	/* Cruel Revival	|4|B
	 * Instant
	 * Destroy target non-Zombie creature. It can't be regenerated. Return up to one target Zombie card from your graveyard to your hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_ZOMBIE;
	td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target non-Zombie creature.", 1, NULL);
		if( spell_fizzled != 1 ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_PERMANENT, "Select target Zombie card.");
			this_test.subtype = SUBTYPE_ZOMBIE;
			if( new_special_count_grave(player, &this_test) && ! graveyard_has_shroud(2) ){
				select_target_from_grave_source(player, card, player, SFG_NO_SPELL_FIZZLED, AI_MAX_VALUE, &this_test, 1);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		int selected = validate_target_from_grave_source(player, card, player, 1);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_cryptic_gateway(int player, int card, event_t event){
/* Cryptic Gateway	|5
 * Artifact
 * Tap two untapped creatures you control: You may put a creature card from your hand that shares a creature type with each creature
 * tapped this way onto the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}


	target_definition_t td;
	base_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_state = TARGET_STATE_TAPPED;
	td.preferred_controller = td.allowed_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && target_available(player, card, &td) > 1 ){
		return generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		ai_modifier += (current_turn == 1-player && current_phase >= PHASE_AFTER_BLOCKING ? 48 : -48);
		generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			int total = 0;
			while( total < 2 && can_target(&td) ){
					if( select_target(player, card, &td, "Select a creature.", &(instance->targets[total])) ){
						instance->number_of_targets = 1;
						state_untargettable(instance->targets[total].player, instance->targets[total].card, 1);
						total++;
					}
					else{
						break;
					}
			}
			int i;
			for(i=0; i<total; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
				if( total == 2 ){
					tap_card(instance->targets[i].player, instance->targets[i].card);
				}
			}
			instance->number_of_targets = 0;
			if( total != 2 ){
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = -1;
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
		if( ! IS_AI(player) ){
			selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
		}
		else{
			int sel_array[0][hand_count[player]];
			int sac = 0;
			CYCLE_ALL_CARDS(p, c,	{
										if( p == player && in_hand(p, c) &&
											(shares_creature_subtype(p, c, instance->targets[0].player, instance->targets[0].card) ||
											shares_creature_subtype(p, c, instance->targets[1].player, instance->targets[1].card)) )
										{
											sel_array[0][sac] = get_original_internal_card_id(p, c);
											sel_array[1][sac] = c;
											sac++;
										}
									}
			);
			if( sac ){
				int pos = select_card_from_zone(player, player, sel_array[0], sac, 0, AI_MAX_CMC, -1, &this_test);
				if( pos > -1 ){
					selected = sel_array[1][pos];
				}
			}
		}
		if( selected != -1 ){
			if( shares_creature_subtype(player, selected, instance->targets[0].player, instance->targets[0].card) &&
				shares_creature_subtype(player, selected, instance->targets[1].player, instance->targets[1].card)
			  ){
				put_into_play(player, selected);
			}
		}
	}

	return 0;
}

int card_death_match(int player, int card, event_t event){
/* CARD_ID_DEATH_MATCH	6112
Death Match	|3|B
Enchantment
Whenever a creature enters the battlefield, that creature's controller may have target creature of his or her choice get -3/-3 until end of turn. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);

		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.who_chooses = trigger_cause_controller;

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			pump_until_eot_merge_previous(player, card, instance->targets[0].player, instance->targets[0].card, -3, -3);
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_death_pulse(int player, int card, event_t event){
	/* Death Pulse	|2|B|B
	 * Instant
	 * Target creature gets -4/-4 until end of turn.
	 * Cycling |1|B|B
	 * When you cycle ~, you may have target creature get -1/-1 until end of turn. */

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XB(1, 2));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		draw_a_card(player);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -1, -1);
		}
	}

	return vanilla_instant_pump(player, card, event, ANYBODY, player, -4, -4, 0, 0);
}

int card_demystify(int player, int card, event_t event ){
	/*
	  Demystify |W
	  Instant
	  Destroy target enchantment.
	*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT", 1, NULL);
}

int card_dirge_of_the_dread(int player, int card, event_t event){
	/* Dirge of Dread	|2|B
	 * Sorcery
	 * All creatures gain fear until end of turn.
	 * Cycling |1|B
	 * When you cycle ~, you may have target creature gain fear until end of turn. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, 2, -1, 0, 0, 0, SP_KEYWORD_FEAR);
		kill_card(player, card, KILL_DESTROY);
	}

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XB(1, 1));
	}
	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		draw_a_card(player);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			card_instance_t* instance = get_card_instance(player, card);
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_FEAR);
		}
	}

	return 0;
}

int card_discombobulate(int player, int card, event_t event){
/* CARD_ID_DISCOMBOBULATE	6117
Discombobulate	|2|U|U
Instant
Counter target spell. Look at the top four cards of your library, then put them back in any order. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			rearrange_top_x(player, player, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

int card_dispersing_orb(int player, int card, event_t event){
/*
Dispersing Orb |3|U|U
Enchantment
{3}{U}, Sacrifice a permanent: Return target permanent to its owner's hand.
*/
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_ACTIVATE ){
		if( count_subtype(player, TYPE_PERMANENT, -1) >= 10 ){
			ai_modifier += 24;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST_XU(1, 1), 0,
															&td, "TARGET_PERMANENT", 0, &test, 0);
}

int card_disruptive_pitmage(int player, int card, event_t event){
	/* Disruptive Pitmage	|2|U
	 * Creature - Human Wizard 1/1
	 * |T: Counter target spell unless its controller pays |1.
	 * Morph |U */

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		counterspell_target_definition(player, card, &td, 0);
		if( event == EVENT_RESOLVE_ACTIVATION ){
			counterspell_resolve_unless_pay_x(player, card, &td, 0, 1);
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SPELL_ON_STACK, MANACOST0, 0, &td, NULL);
	}

	return morph(player, card, event, MANACOST_U(1));
}

int card_doomed_necromancer(int player, int card, event_t event){
/* CARD_ID_DOOMED_NECROMANCER	6122
Doomed Necromancer	|2|B
Creature - Human Cleric Mercenary 2/2
|B, |T, Sacrifice ~: Return target creature card from your graveyard to the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");

	if(event == EVENT_ACTIVATE ){
		if( check_state(player, card, STATE_BLOCKING) ){
			card_instance_t *instance = get_card_instance(player, card);
			if( get_power(1-player, instance->blocking) >= get_toughness(player, card) &&
				get_toughness(1-player, instance->blocking) < get_power(player, card) )
			{
				ai_modifier += 96;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_B(1), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER);
}

int card_doubtless_one(int player, int card, event_t event){
/* CARD_ID_DOUBTLESS_ONE	6123
Doubtless One	|3|W
Creature - Cleric Avatar 100/100
~'s power and toughness are each equal to the number of Clerics on the battlefield.
Whenever ~ deals damage, you gain that much life. */

	spirit_link_effect(player, card, event, player);

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && player > -1 && ! is_humiliated(player, card) ){
		event_result+=count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_CLERIC);
	}

	return 0;
}

int card_dragon_roost(int player, int card, event_t event){
	/* Dragon Roost	|4|R|R
	 * Enchantment
	 * |5|R|R: Put a 5/5 |Sred Dragon creature token with flying onto the battlefield. */
	// original code : 004E1EEE

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_DRAGON, &token);
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		token.pow = token.tou = 5;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XR(5, 2), 0, NULL, NULL);
}

// dream chisel --> vanilla

int card_dwarven_blastminer(int player, int card, event_t event){
/* CARD_ID_DWARVEN_BLASTMINER	6125
Dwarven Blastminer	|1|R
Creature - Dwarf 1/1
|2|R, |T: Destroy target nonbasic land.
Morph |R (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	if( IS_GAA_EVENT(event) ){
		return card_dwarven_miner(player, card, event);
	}

	return morph(player, card, event, MANACOST_R(1));
}

int card_ebonblade_reaper(int player, int card, event_t event)
{
  /* Ebonblade Reaper	|2|B
   * Creature - Human Cleric 1/1
   * Whenever ~ attacks, you lose half your life, rounded up.
   * Whenever ~ deals combat damage to a player, that player loses half his or her life, rounded up.
   * Morph |3|B|B */

  if (declare_attackers_trigger(player, card, event, DAT_STORE_IN_TARGETS_3, player, card))
	lose_life(player, (life[player] + 1) / 2);

  if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER|DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_TRACE_DAMAGED_PLAYERS))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
	  instance->targets[1].player = 0;
	  int p;
	  for (p = 0; p <= 1; ++p)
		for (; times_damaged[p] > 0; --times_damaged[p])
		  lose_life(p, (life[p] + 1) / 2);
	}

  return morph(player, card, event, MANACOST_XB(3,2));
}

int card_elvish_vanguard(int player, int card, event_t event){
/* CARD_ID_ELVISH_VANGUARD	6129
Elvish Vanguard	|1|G
Creature - Elf Warrior 1/1
Whenever another Elf enters the battlefield, put a +1/+1 counter on ~. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_ELF;
		this_test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		add_1_1_counter(player, card);
	}

	return 0;
}

int card_elvish_pioneer(int player, int card, event_t event){
	/*
	  Elvish Pioneer |G
	  Creature - Elf Druid 1/1
	  When Elvish Pioneer enters the battlefield, you may put a basic land card from your hand onto the battlefield tapped.
	*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a basic land card.");
		test.zone = TARGET_ZONE_HAND;
		if( check_battlefield_for_special_card(player, card, player, 0, &test) ){
			new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY_TAPPED, 0, AI_FIRST_FOUND, &test);
		}
	}

	return 0;
}

int card_enchantresss_presence(int player, int card, event_t event){
/* CARD_ID_ENCHANTRESSS_PRESENCE	6131
Enchantress's Presence	|2|G
Enchantment
Whenever you cast an enchantment spell, draw a card. */

	if(event == EVENT_CAN_CAST || event == EVENT_SHOULD_AI_PLAY){
		return global_enchantment(player, card, event);
	}
	return card_argothian_enchantress(player, card,  event);
}

int card_entrails_feaster(int player, int card, event_t event){
/* CARD_ID_ENTRAILS_FEASTER	6133
Entrails Feaster	|B
Creature - Zombie Cat 1/1
At the beginning of your upkeep, you may exile a creature card from a graveyard. If you do, put a +1/+1 counter on ~. If you don't, tap ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int tp = 1;
		char msg[100] = "Select a creature card to exile.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, msg);
		if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test) != -1){
			add_1_1_counter(player, card);
			tp = 0;
		}
		if( tp ){
			tap_card(player, card);
		}
	}

	return 0;
}

int card_erratic_explosion(int player, int card, event_t event){
	/* Erratic Explosion	|2|R	0x20091f2
	 * Sorcery
	 * Choose any target. Reveal cards from the top of your library until you reveal a nonland card. ~ deals damage equal to that card's converted mana cost to that permanent or player. Put the revealed cards on the bottom of your library in any order. */
	if( !IS_CASTING(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int *deck = deck_ptr[player];
			if( deck[0] != -1 ){
				int dmg = 0;
				int count = 0;
				while( deck[count] != -1 ){
						if( ! is_what(-1, deck[count], TYPE_LAND) ){
							break;
						}
						count++;
				}
				show_deck(player, deck, count+1, "Erratic Explosion revealed these cards.", 0, 0x7375B0);
				show_deck(1-player, deck, count+1, "Erratic Explosion revealed these cards.", 0, 0x7375B0);
				dmg = get_cmc_by_internal_id(deck[count]);
				if( dmg > 0 ){
					damage_target0(player, card, dmg);
				}
				put_top_x_on_bottom(player, player, count+1);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_exalted_angel(int player, int card, event_t event){
/* CARD_ID_EXALTED_ANGEL	6137
Exalted Angel	|4|W|W
Creature - Angel 4/5
Flying
Whenever ~ deals damage, you gain that much life.
Morph |2|W|W (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	spirit_link_effect(player, card, event, player);
	return morph(player, card, event, MANACOST_XW(2, 2));
}

int card_explosive_vegetation(int player, int card, event_t event){
	/*
	  Explosive Vegetation |3|G
	  Sorcery
	  Search your library for up to two basic land cards and put them onto the battlefield tapped. Then shuffle your library.
	*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (get_total_available_lands(player) < 6 ? 48 : -24);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_false_cure(int player, int card, event_t event){
/* CARD_ID_FALSE_CURE	6140
False Cure	|B|B
Instant
Until end of turn, whenever a player gains life, that player loses 2 life for each 1 life he or she gained. */

	if(event == EVENT_RESOLVE_SPELL){
		int legacy = create_legacy_effect(player, card, &until_eot_legacy);
		card_instance_t *leg = get_card_instance(player, legacy);
		leg->targets[3].card = CARD_ID_FALSE_CURE;
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_festering_goblin(int player, int card, event_t event){
/* CARD_ID_FESTERING_GOBLIN	6142
Festering Goblin	|B
Creature - Zombie Goblin 1/1
When ~ dies, target creature gets -1/-1 until end of turn. */

	if( this_dies_trigger(player, card, event, 2) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -1, -1);
		}
	}

	return 0;
}

int card_flooded_strand(int player, int card, event_t event){
	/* Flooded Strand	""
	 * Land
	 * |T, Pay 1 life, Sacrifice ~: Search your library for |Ha Plains or |H2Island card and put it onto the battlefield. Then shuffle your library. */
	return fetchland(player, card, event, SUBTYPE_PLAINS, SUBTYPE_ISLAND, 1);
}

int card_forgotten_cave(int player, int card, event_t event){
/* CARD_ID_FORGOTTEN_CAVE	6147
Forgotten Cave	""
Land
~ enters the battlefield tapped.
|T: Add |R to your mana pool.
Cycling |R (|R, Discard this card: Draw a card.) */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return cycling(player, card, event, MANACOST_R(1));
	}

	comes_into_play_tapped(player, card, event);

	return mana_producer(player, card, event);
}

int card_future_sight(int player, int card, event_t event){
/* CARD_ID_FUTURE_SIGHT	6149
Future Sight	|2|U|U|U
Enchantment
Play with the top card of your library revealed.
You may play the top card of your library. */

	reveal_top_card(player, card, event);

	if( event == EVENT_CAN_ACTIVATE && ! is_humiliated(player, card) ){
		int iid = deck_ptr[player][0];
		if( iid > -1 && get_card_instance(player, card)->info_slot != 66 ){
			int can_cast;
			if( (can_cast = can_legally_play_iid_now(player, iid, event)) &&
				has_mana_to_cast_id(player, event, cards_data[iid].id) )
			{
				return can_cast;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int iid = deck_ptr[player][0];
		get_card_instance(player, card)->info_slot = 66;
		charge_mana_from_id(player, -1, event, cards_data[iid].id);
		get_card_instance(player, card)->info_slot = 0;
		if (cancel != 1){
			play_card_in_deck_for_free(player, player, 0);
			cant_be_responded_to = 1;
		}
	}

	return global_enchantment(player, card, event);
}

int card_gangrenous_goliath(int player, int card, event_t event)
{
	/* Gangrenous Goliath	|3|B|B
	 * Creature - Zombie Giant 4/4
	 * Tap three untapped Clerics you control: Return ~ from your graveyard to your hand. */

	if (event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS)
	{
	  target_definition_t td;
	  base_target_definition(player, card, &td, TYPE_PERMANENT);
	  td.preferred_controller = player;
	  td.allowed_controller = player;
	  td.illegal_state = TARGET_STATE_TAPPED;
	  td.required_subtype = SUBTYPE_CLERIC;

	  if (event == EVENT_GRAVEYARD_ABILITY && target_available(player, card, &td) >= 3)
		return GA_RETURN_TO_HAND;

	  if (event == EVENT_PAY_FLASHBACK_COSTS && pick_up_to_n_targets_noload(&td, "Select an untapped Cleric.", 3) == 3)
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  int i;
		  for (i = 0; i < 3; ++i)
			tap_card(instance->targets[i].player, instance->targets[i].card);

		  instance->number_of_targets = 0;
		  return GAPAID_REMOVE;
		}
	}

  return 0;
}

int card_gigapede(int player, int card, event_t event){
/* CARD_ID_GIGAPEDE	6152
Gigapede	|3|G|G
Creature - Insect 6/1
Shroud (This creature can't be the target of spells or abilities.)
At the beginning of your upkeep, if ~ is in your graveyard, you may discard a card. If you do, return ~ to your hand. */

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		if( hand_count[player] > 0 ){
			int choice = do_dialog(player, player, card, -1, -1," Return Gigapede to hand\n Pass", 0);
			if( choice == 0 ){
				discard(player, 0, player);
				instance->state &= ~STATE_INVISIBLE;
				hand_count[player]++;
				return -1;
			}
		}
		return -2;
	}

	return 0;
}

int card_goblin_burrows(int player, int card, event_t event){
/* CARD_ID_GOBLIN_BURROWS	6154
Goblin Burrows	""
Land
|T: Add |C to your mana pool.
|1|R, |T: Target Goblin creature gets +2/+0 until end of turn. */

	card_instance_t *instance = get_card_instance( player, card );

	if( (event == EVENT_COUNT_MANA && affect_me(player, card)) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_GOBLIN;

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = instance->info_slot = 0;
		int choice = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XR(2, 1), 0, &td, "TARGET_CREATURE") ){
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Pump a Goblin\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		if( choice == 1 ){
			instance->state |= STATE_TAPPED;
			if (charge_mana_for_activated_ability(player, card, MANACOST_XR(1, 1)) && pick_target(&td, "TARGET_CREATURE") ){
				tap_card(player, card);
				instance->info_slot = 1;
			}
			else{
				untap_card_no_event(player, card);
			}
		}

		if (choice == 2){
			cancel = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			if( valid_target(&td) ){
				pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
								2, 0);
			}
		}
	}

	return 0;
}

int card_goblin_piledriver( int player, int card, event_t event)
{
/* CARD_ID_GOBLIN_PILEDRIVER	6156
Goblin Piledriver	|1|R
Creature - Goblin Warrior 1/2
Protection from |Sblue (This creature can't be blocked, targeted, dealt damage, or enchanted by anything |Sblue.)
Whenever ~ attacks, it gets +2/+0 until end of turn for each other attacking Goblin. */

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.state = STATE_ATTACKING;
	  test.subtype = SUBTYPE_GOBLIN;
	  test.not_me = 1;
	  int num = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test);

	  pump_until_eot(player, card, player, card, 2 * num, 0);
	}

  return 0;
}

int card_goblin_pyromancer(int player, int card, event_t event){
/* CARD_ID_GOBLIN_PYROMANCER	6157
Goblin Pyromancer	|3|R
Creature - Goblin Wizard 2/2
When ~ enters the battlefield, Goblin creatures get +3/+0 until end of turn.
At the beginning of the end step, destroy all Goblins. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		pump_subtype_until_eot(player, card, ANYBODY, SUBTYPE_GOBLIN, 3, 0, 0, 0);
	}

	if( eot_trigger(player, card, event) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.subtype = SUBTYPE_GOBLIN;
		APNAP(p, { new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);
	}

	return 0;
}

int card_goblin_sharpshooter( int player, int card, event_t event){
	/* Goblin Sharpshooter	|2|R	0x2001542
	 * Creature - Goblin 1/1
	 * ~ doesn't untap during your untap step.
	 * Whenever a creature dies, untap ~.
	 * |T: ~ deals 1 damage to any target. */
	does_not_untap(player, card, event);

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
	}

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		untap_card(player, card);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_creature_or_player(player, card, event, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ANY");
}

int card_goblin_sledder(int player, int card, event_t event){
/* CARD_ID_GOBLIN_SLEDDER	6160
Goblin Sledder	|R
Creature - Goblin 1/1
Sacrifice a Goblin: Target creature gets +1/+1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Goblin to sacrifice.");
	test.subtype = SUBTYPE_GOBLIN;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td1, "TARGET_CREATURE",
															0, &test, 0);
}

int card_goblin_taskmaster(int player, int card, event_t event){
/* CARD_ID_GOBLIN_TASKMASTER	6161
Goblin Taskmaster	|R
Creature - Goblin 1/1
|1|R: Target Goblin creature gets +1/+0 until end of turn.
Morph |R (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	if( IS_GAA_EVENT(event) ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.preferred_controller = player;
		td1.required_subtype = SUBTYPE_GOBLIN;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td1) ){
				pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
												instance->targets[0].player, instance->targets[0].card, 1, 0);
			}
		}
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XR(1, 1), 0,
										&td1, "Select target Goblin creature.");
	}

	return morph(player, card, event, MANACOST_R(1));
}

int card_gratuitous_violence(int player, int card, event_t event)
{
	/* Gratuitous Violence	|2|R|R|R	0x2009215
	 * Enchantment
	 * If a creature you control would deal damage to a permanent or player, it deals double that damage to that permanent or player instead. */
  card_instance_t* damage = damage_being_dealt(event);
  if (damage
	  && damage->damage_source_player == player
	  && is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE))
	damage->info_slot *= 2;

  return global_enchantment(player, card, event);
}

int card_gravespawn_sovereign(int player, int card, event_t event){

	/* Gravespawn Sovereign	|4|B|B
	 * Creature - Zombie 3/3
	 * Tap five untapped Zombies you control: Put target creature card from a graveyard onto the battlefield under your control. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	base_target_definition(player, card, &td1, TYPE_PERMANENT);
	td1.illegal_state = TARGET_STATE_TAPPED;
	td1.required_subtype = SUBTYPE_ZOMBIE;
	td1.preferred_controller = player;
	td1.allowed_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( (count_graveyard_by_type(player, TYPE_CREATURE) && ! graveyard_has_shroud(player)) ||
			(count_graveyard_by_type(1-player, TYPE_CREATURE) && ! graveyard_has_shroud(1-player)) )
		{
			return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, &td1, 5);
		}
	}

	if( event == EVENT_ACTIVATE && charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
		if( select_target_from_either_grave(player, card, 0, AI_MAX_CMC, AI_MAX_CMC, &this_test, 1, 2) == -1 ){
			return 0;
		}
		return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, &td1, 5);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, instance->targets[1].player, 2);
		if( selected != -1 ){
			reanimate_permanent(player, card, instance->targets[6].player, selected, REANIMATE_DEFAULT);
		}
	}

	return 0;
}

int card_grinning_demon(int player, int card, event_t event){
/* CARD_ID_GRINNING_DEMON	6169
Grinning Demon	|2|B|B
Creature - Demon 6/6
At the beginning of your upkeep, you lose 2 life.
Morph |2|B|B (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		lose_life(player, 2);
	}

	return morph(player, card, event, MANACOST_XB(2, 2));
}

int card_haunted_cadaver(int player, int card, event_t event){
/*
Haunted Cadaver |3|B
Creature — Zombie 2/2, 3B (4)
Whenever Haunted Cadaver deals combat damage to a player, you may sacrifice it. If you do, that player discards three cards.
Morph {1}{B} (You may cast this card face down as a 2/2 creature for {3}. Turn it face up any time for its morph cost.)
*/
	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					if( times_damaged[p] ){
						int choice = 1;
						if( p == 1-player && hand_count[1-player] > 1 ){
							choice = 0;
						}
						if( ! IS_AI(player) ){
							choice = do_dialog(player, player, card, -1, -1, " Sacrifice Haunted Cadaver\n Pass", 0);
						}
						if( choice == 0 ){
							new_multidiscard(p, 3, 0, player);
							kill_card(player, card, KILL_SACRIFICE);
							return 0;
						}
					}
				}
		);
	}

	return morph(player, card, event, MANACOST_XB(1, 1));
}

int card_head_games(int player, int card, event_t event){
/*
Head Games |3|B|B
Sorcery, 3BB
Target opponent puts the cards from his or her hand on top of his or her library. Search that player's library for that many cards.
The player puts those cards into his or her hand, then shuffles his or her library.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (hand_count[1-player] * 12);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = 0;
			int c;
			for(c = active_cards_count[1-player]-1; c > -1; c--){
				if( in_hand(1-player, c) ){
					amount++;
					put_on_top_of_deck(1-player, c);
				}
			}
			if( ! IS_AI(player) ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, "Select a card.");
				while( amount ){
						int selected = new_select_a_card(player, 1-player, TUTOR_FROM_DECK, 1, AI_MIN_VALUE, -1, &this_test);
						add_card_to_hand(1-player, deck_ptr[1-player][selected]);
						remove_card_from_deck(1-player, selected);
						amount--;
				}
			}
			else{
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_LAND);
				while( amount ){
						int selected = new_select_a_card(player, 1-player, TUTOR_FROM_DECK, 1, AI_MIN_VALUE, -1, &this_test);
						if( selected != -1 ){
							add_card_to_hand(1-player, deck_ptr[1-player][selected]);
							remove_card_from_deck(1-player, selected);
							amount--;
						}
						else{
							break;
						}
				}
				default_test_definition(&this_test, TYPE_ANY);
				while( amount ){
						int selected = new_select_a_card(player, 1-player, TUTOR_FROM_DECK, 1, AI_MIN_VALUE, -1, &this_test);
						add_card_to_hand(1-player, deck_ptr[1-player][selected]);
						remove_card_from_deck(1-player, selected);
						amount--;
				}
			}
			shuffle(1-player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_heedless_one(int player, int card, event_t event){
	/* Heedless One	|3|G
	 * Creature - Elf Avatar 100/100
	 * Trample
	 * ~'s power and toughness are each equal to the number of Elves on the battlefield. */

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && player > -1 && affect_me(player, card) && !is_humiliated(player, card)){
		event_result += count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_ELF);
	}

	return 0;
}

int card_headhunter(int player, int card, event_t event)
{
  /* Headhunter	|1|B
   * Creature - Human Cleric 1/1
   * Whenever ~ deals combat damage to a player, that player discards a card.
   * Morph |B */

  whenever_i_deal_damage_to_a_player_he_discards_a_card(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE, 0);

  return morph(player, card, event, MANACOST_B(1));
}

int card_hystrodon(int player, int card, event_t event){
/* CARD_ID_HYSTRODON	6180
Hystrodon	|4|G
Creature - Beast 3/4
Trample
Whenever ~ deals combat damage to a player, you may draw a card.
Morph |1|G|G (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_TRIGGER_OPTIONAL);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		draw_cards(player, amount);
	}
	return morph(player, card, event, MANACOST_XG(1, 2));
}

int card_information_dealer(int player, int card, event_t event){
/* CARD_ID_INFORMATION_DEALER	6183
Information Dealer	|1|U
Creature - Human Wizard 1/1
|T: Look at the top X cards of your library, where X is the number of Wizards on the battlefield, then put them back in any order. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		rearrange_top_x(player, player, count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_WIZARD));
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_infest(int player, int card, event_t event){
	/*
	  Infest |1|B|B
	  Sorcery
	  All creatures get -2/-2 until end of turn.
	*/

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p,{ pump_subtype_until_eot(player, card, p, -1, -2, -2, 0, 0); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_insurrection(int player, int card, event_t event){
/* CARD_ID_INSURRECTION	6185
Insurrection	|5|R|R|R
Sorcery
Untap all creatures and gain control of them until end of turn. They gain haste until end of turn. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(1-player, TYPE_CREATURE, -1)*12);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		insurrection_effect(player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_invigorating_boon(int player, int card, event_t event){
/* CARD_ID_INVIGORATING_BOON	6186
Invigorating Boon	|1|G
Enchantment
Whenever a player cycles a card, you may put a +1/+1 counter on target creature. */

	return global_enchantment(player, card, event);
}

int card_ironfist_crusher(int player, int card, event_t event)
{
/* CARD_ID_IRONFIST_CRUSHER	6187
Ironfist Crusher	|4|W
Creature - Human Soldier 2/4
~ can block any number of creatures.
Morph |3|W (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

  creature_can_block_additional(player, card, event, 255);
  return morph(player, card, event, MANACOST_XW(3,1));
}

int card_ixidor_reality_sculptor(int player, int card, event_t event)
{
  /* Ixidor, Reality Sculptor	|3|U|U
   * Legendary Creature - Human Wizard 3/4
   * Face-down creatures get +1/+1.
   * |2|U: Turn target face-down creature face up. */

	check_legend_rule(player, card, event);

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && in_play(player, card) && ! is_humiliated(player, card) ){
		if( get_id(affected_card_controller, affected_card) == CARD_ID_FACE_DOWN_CREATURE ){
			event_result++;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.extra = get_internal_card_id_from_csv_id(CARD_ID_FACE_DOWN_CREATURE);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
		card_instance_t* inst = get_card_instance(player, card);
		flip_card(inst->targets[0].player, inst->targets[0].card);
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST_XU(2, 1), 0,
									&td, "Select target face-down creature.");
}

int card_jareth_leonine_titan(int player, int card, event_t event){
/* CARD_ID_JARETH_LEONINE_TITAN	6190
Jareth, Leonine Titan	|3|W|W|W
Legendary Creature - Cat Giant 4/7
Whenever ~ blocks, it gets +7/+7 until end of turn.
|W: ~ gains protection from the color of your choice until end of turn. */

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int keyword = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								0, 0, keyword, 0 );
	}

	if( event == EVENT_DECLARE_BLOCKERS ){
		if( get_card_instance(player, card)->blocking < 255 ){
			pump_until_eot(player, card, player, card, 7, 7 );
		}
	}
	return generic_activated_ability(player, card, event, 0, MANACOST_W(1), 0, NULL, NULL);
}

int card_kamahl_fist_of_krosa(int player, int card, event_t event){
/* CARD_ID_KAMAHL_FIST_OF_KROSA	6193
Kamahl, Fist of Krosa	|4|G|G
Legendary Creature - Human Druid 4/3
|G: Target land becomes a 1/1 creature until end of turn. It's still a land.
|2|G|G|G: Creatures you control get +3/+3 and gain trample until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.illegal_type = TYPE_CREATURE;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_ANIMATE = 1,
		CHOICE_OVERRUN
	};

	if( event == EVENT_CAN_ACTIVATE  ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_XG(2, 3), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int priority_overrun = count_subtype(player, TYPE_CREATURE, -1)*2;
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XG(2, 3), 0, NULL, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Animate a land", abils[1], 5,
							"Overrun", abils[2], priority_overrun);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_ANIMATE ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, "TARGET_LAND");
		}
		if( choice == CHOICE_OVERRUN ){
			return generic_activated_ability(player, card, event, 0, MANACOST_XG(2, 3), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_ANIMATE && valid_target(&td) ){
			land_animation2(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							1, 1, 1, 0, 0, 0, 0);
		}
		if( instance->info_slot == CHOICE_OVERRUN ){
			pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1, 3, 3, KEYWORD_TRAMPLE, 0);
		}
	}
	return 0;
}

int card_krosan_colossus(int player, int card, event_t event){
/* CARD_ID_KROSAN_COLOSSUS	6194
Krosan Colossus	|6|G|G|G
Creature - Beast 9/9
Morph |6|G|G (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	return morph(player, card, event, MANACOST_XG(6, 2));
}

int card_krosan_groundshaker(int player, int card, event_t event){
/* CARD_ID_KROSAN_GROUNDSHAKER	6195
Krosan Groundshaker	|4|G|G|G
Creature - Beast 6/6
|G: Target Beast creature gains trample until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.preferred_controller = player;
	td1.required_subtype = SUBTYPE_BEAST;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_TRAMPLE, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_G(1), 0,
									&td1, "Select target Beast creature.");
}

int card_krosan_tusker(int player, int card, event_t event){
	/* Krosan Tusker	|5|G|G
	 * Creature - Boar Beast 6/5
	 * Cycling |2|G
	 * When you cycle ~, you may search your library for a basic land card, reveal that card, put it into your hand, then shuffle your library. */

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XG(2, 1));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		draw_a_card(player);
		int choice = IS_AI(player) ? 0 : do_dialog(player, player, card, -1, -1, " Search for a basic land\n Pass", 0);
		if( choice == 0 ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			this_test.subtype = SUBTYPE_BASIC;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);
		}
	}

	return 0;
}

// lightning Rift --> Invigorating Boon

int card_lonely_sandbar(int player, int card, event_t event){
/* CARD_ID_LONELY_SANDBAR	6200
Lonely Sandbar	""
Land
~ enters the battlefield tapped.
|T: Add |U to your mana pool.
Cycling |U (|U, Discard this card: Draw a card.) */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return cycling(player, card, event, MANACOST_U(1));
	}

	comes_into_play_tapped(player, card, event);

	return mana_producer(player, card, event);
}

int card_mana_echoes(int player, int card, event_t event){
/*
Mana Echoes |2|R|R
Enchantment
Whenever a creature enters the battlefield, you may add an amount of {C} to your mana pool equal to the number of creatures you control
	that share a creature type with it.
*/

	etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		int quantity = 1;
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_play(player, i) && is_what(player, i, TYPE_CREATURE) && i != card &&
				shares_creature_subtype(trigger_cause_controller, trigger_cause, player, i) )
			{
				quantity++;
			}
		}
		produce_mana(trigger_cause_controller, COLOR_COLORLESS, quantity);
	}

   return global_enchantment(player, card, event);
}

int card_mobilization(int player, int card, event_t event){
	/* Mobilization	|2|W
	 * Enchantment
	 * Soldier creatures have vigilance.
	 * |2|W: Put a 1/1 |Swhite Soldier creature token onto the battlefield. */

	if( event == EVENT_ABILITIES && in_play(player, card) && ! is_humiliated(player, card) ){
		if( has_creature_type(affected_card_controller, affected_card, SUBTYPE_SOLDIER) ){
			vigilance(affected_card_controller, affected_card, event);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SOLDIER);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XW(2, 1), 0, NULL, NULL);
}

int card_mythic_proportions(int player, int card, event_t event){
/* CARD_ID_MYTHIC_PROPORTIONS	6213
Mythic Proportions	|4|G|G|G
Enchantment - Aura
Enchant creature
Enchanted creature gets +8/+8 and has trample. */

	return generic_aura(player, card, event, player, 8, 8, KEYWORD_TRAMPLE, 0, 0, 0, 0);
}

int card_nameless_one(int player, int card, event_t event){
	/* Nameless One	|3|U
	 * Creature - Wizard Avatar 100/100
	 * ~'s power and toughness are each equal to the number of Wizards on the battlefield.
	 * Morph |2|U */

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && player > -1 && affect_me(player, card) && !is_humiliated(player, card)){
		event_result+=count_subtype(2, TYPE_PERMANENT, SUBTYPE_WIZARD);
	}

	return morph(player, card, event, MANACOST_XU(2, 1));
}

int card_nantuko_husk(int player, int card, event_t event){
	// original code : 012039A8
	/* Nantuko Husk	|2|B
	 * Creature - Zombie Insect 2/2
	 * Sacrifice a creature: ~ gets +2/+2 until end of turn. */
	/* Bloodthrone Vampire	|1|B
	 * Creature - Vampire 1/1
	 * Sacrifice a creature: ~ gets +2/+2 until end of turn. */
	/* Phyrexian Ghoul	|2|B
	 * Creature - Zombie 2/2
	 * Sacrifice a creature: ~ gets +2/+2 until end of turn. */
	/* Vampire Aristocrat	|2|B
	 * Creature - Vampire Rogue 2/2
	 * Sacrifice a creature: ~ gets +2/+2 until end of turn. */
	return generic_husk(player, card, event, TYPE_CREATURE, 2, 2, 0, 0);
}

int card_nova_cleric(int player, int card, event_t event){
/* CARD_ID_NOVA_CLERIC	6217
Nova Cleric	|W
Creature - Human Cleric 1/2
|2|W, |T, Sacrifice ~: Destroy all enchantments. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT);
		APNAP(p, { new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_UNTAPPED, MANACOST_XW(2, 1), 0, NULL, NULL);
}

int card_oblation(int player, int card, event_t event){
/* CARD_ID_OBLATION	6218
Oblation	|2|W
Instant
The owner of target nonland permanent shuffles it into his or her library, then draws two cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_type = TYPE_LAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int owner = get_owner(instance->targets[0].player, instance->targets[0].card);
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
			shuffle(owner);
			draw_cards(owner, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nonland permanent.", 1, NULL);
}

int card_oversold_cemetery(int player, int card, event_t event){
/* CARD_ID_OVERSOLD_CEMETERY	6219
Oversold Cemetery	|1|B
Enchantment
At the beginning of your upkeep, if you have four or more creature cards in your graveyard, you may return target creature card from your graveyard to your hand.*/

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if (count_graveyard_by_type(player, TYPE_CREATURE) >= 4){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 1, AI_MAX_VALUE, &this_test);
		}
	}

	return global_enchantment(player, card, event);
}

int card_overwhelming_instinct(int player, int card, event_t event)
{
/* CARD_ID_OVERWHELMING_INSTINCT	6220
Overwhelming Instinct	|2|G
Enchantment
Whenever you attack with three or more creatures, draw a card. */

  if (declare_attackers_trigger(player, card, event, DAT_ATTACKS_WITH_3_OR_MORE, player, -1))
	draw_a_card(player);

  return global_enchantment(player, card, event);
}

int card_patriarchs_bidding(int player, int card, event_t event){
/* CARD_ID_PATRIARCHS_BIDDING	6221
Patriarch's Bidding	|3|B|B
Sorcery
Each player chooses a creature type. Each player returns all creature cards of a type chosen this way from his or her graveyard to the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		int subtype_chosen[2];
		APNAP(p, {subtype_chosen[p] = select_a_subtype_full_choice(player, card, p, get_grave(p), 0);};);
		APNAP(p, {
					test_definition_t test;
					new_default_test_definition(&test, TYPE_PERMANENT, "");
					test.subtype = subtype_chosen[0];
					test.sub2 = subtype_chosen[1];
					test.subtype_flag = F2_MULTISUBTYPE;
					new_reanimate_all(p, -1, p, &test, REANIMATE_DEFAULT);
				};
		);
		kill_card( player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

static const char* target_is_attached_to_creature(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (instance->damage_target_card >= 0 && is_what(instance->damage_target_player, instance->damage_target_card, TYPE_CREATURE))
	return NULL;
  else
	return "attached to a creature";
}
int card_piety_charm(int player, int card, event_t event)
{
  /* Piety Charm	|W
   * Instant
   * Choose one - Destroy target Aura attached to a creature; or target Soldier creature gets +2/+2 until end of turn; or creatures you control gain vigilance
   * until end of turn. */

  if (IS_CASTING(player, card, event))
	{
	  target_definition_t td_aura;
	  default_target_definition(player, card, &td_aura, TYPE_ENCHANTMENT);
	  td_aura.required_subtype = SUBTYPE_AURA;
	  td_aura.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	  td_aura.extra = (int)target_is_attached_to_creature;

	  target_definition_t td_soldier;
	  default_target_definition(player, card, &td_soldier, TYPE_CREATURE);
	  td_soldier.required_subtype = SUBTYPE_SOLDIER;
	  td_soldier.preferred_controller = player;

	  card_instance_t* instance = get_card_instance(player, card);

	  enum
	  {
		CHOICE_AURA = 1,
		CHOICE_SOLDIER,
		CHOICE_VIGILANCE
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM,
						"Destroy an Aura", 1, 10, DLG_LITERAL_TARGET(&td_aura, "Select target Aura attached to a creature."),
						"Soldier gets +2/+2", 1, 10, DLG_LITERAL_TARGET(&td_soldier, "Select target Soldier creature."),
						"Vigilance", 1, creature_count[player] > 0 && current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS ? 10 : 1);

	  if (event == EVENT_CAN_CAST)
		return choice;
	  else if (event == EVENT_CAST_SPELL && affect_me(player, card))
		{
		  if (choice == CHOICE_VIGILANCE && player == AI)
			{
			  if (current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS)
				{
				  int c;
				  card_instance_t* inst;
				  for (c = 0; c < active_cards_count[player]; ++c)
					if ((inst = in_play(player, c)) && is_what(player, c, TYPE_CREATURE) && CAN_TAP(player, c) && !is_humiliated(player, c)
						&& !(inst->state & STATE_VIGILANCE))
					  ai_modifier += 12;
				}
			  else
				ai_modifier -= 48;	// discourage casting on opponent's turn or after combat, but allow e.g. for Black Vise
			}
		}
	  else	// EVENT_RESOLVE_SPELL
		{
		  switch (choice)
			{
			  case CHOICE_AURA:
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				break;

			  case CHOICE_SOLDIER:
				alternate_legacy_text(1, player, pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2,2));
				break;

			  case CHOICE_VIGILANCE:
				pump_creatures_until_eot(player, card, player, 2, 0,0, 0,SP_KEYWORD_VIGILANCE, NULL);
				break;
			}

		  kill_card(player, card, KILL_DESTROY);
		}
	}

  return 0;
}

int card_polluted_delta(int player, int card, event_t event){
	/* Polluted Delta	""
	 * Land
	 * |T, Pay 1 life, Sacrifice ~: Search your library for |Han Island or |H2Swamp card and put it onto the battlefield. Then shuffle your library. */
	return fetchland(player, card, event, SUBTYPE_ISLAND, SUBTYPE_SWAMP, 1);
}

int card_primal_boost(int player, int card, event_t event){
	/* Primal Boost	|2|G
	 * Instant
	 * Target creature gets +4/+4 until end of turn.
	 * Cycling |2|G
	 * When you cycle ~, you may have target creature get +1/+1 until end of turn. */

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XG(2, 1));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		draw_a_card(player);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
	}

	return vanilla_instant_pump(player, card, event, ANYBODY, player, 4, 4, 0, 0);
}

int card_profane_prayers(int player, int card, event_t event){
	/* Profane Prayers	|2|B|B	0x2009279
	 * Sorcery
	 * ~ deals X damage to any target and you gain X life, where X is the number of Clerics on the battlefield. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_CLERIC);
			damage_target0(player, card, amount);
			gain_life(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_ravenous_baloth(int player, int card, event_t event){
/* CARD_ID_RAVENOUS_BALOTH	1222
Ravenous Baloth	|2|G|G
Creature - Beast 4/4
Sacrifice a Beast: You gain 4 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Beast to sacrifice.");
	test.subtype = SUBTYPE_BEAST;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, SARV_REPORT_TOUGHNESS);
		if( spell_fizzled != 1 ){
			if( instance->info_slot == 0 ){
				ai_modifier -= 48;
			}
			else{
				if( life[player] < 6 ){
					ai_modifier += (instance->info_slot * 8);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, instance->info_slot);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0,
															&test, SARV_REPORT_TOUGHNESS);
}


int card_read_the_runes(int player, int card, event_t event){
/* CARD_ID_READ_THE_RUNES	6231
Read the Runes	|X|U
Instant
Draw X cards. For each card drawn this way, discard a card unless you sacrifice a permanent. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		int i;
		for(i=0; i<instance->info_slot; i++){
			draw_cards(player, 1);
			int choice = 0;
			if( count_permanents_by_type(player, TYPE_PERMANENT) > 0 ){
				choice = do_dialog(player, player, card, -1, -1, " Discard\n Sac a permanent", 0);
			}
			if( choice == 0 ){
				discard(player, 0, player);
			}
			else{
				sacrifice(player, card, player, 1, TYPE_PERMANENT, 0, 0, 0, 0, 0, 0, 0, -1, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_reckless_one(int player, int card, event_t event){
	/* Reckless One	|3|R
	 * Creature - Goblin Avatar 100/100
	 * Haste
	 * ~'s power and toughness are each equal to the number of Goblins on the battlefield. */

	haste(player, card, event);

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && player > -1 && affect_me(player, card) && !is_humiliated(player, card)){
		event_result += count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_GOBLIN);
	}

	return 0;
}

int card_reminisce(int player, int card, event_t event){
/* CARD_ID_REMINISCE	6233
Reminisce	|2|U
Sorcery
Target player shuffles his or her graveyard into his or her library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += count_graveyard(player);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			reshuffle_grave_into_deck(instance->targets[0].player, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_renewed_faith(int player, int card, event_t event){
	/* Renewed Faith	|2|W	0x200f534
	 * Instant
	 * You gain 6 life.
	 * Cycling |1|W
	 * When you cycle ~, you may gain 2 life. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (life[player] < 6 ? 48 : 24);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, 6);
		kill_card(player, card, KILL_DESTROY);
	}

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XW(1, 1));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		draw_a_card(player);
		if( do_dialog(player, player, card, -1, -1, " Gain 2 life\n Decline", 0) == 0 ){
			gain_life(player, 2);
		}
	}

	return 0;
}

int card_riptide_laboratory(int player, int card, event_t event){
	/* Riptide Laboratory	""
	 * Land
	 * |T: Add |C to your mana pool.
	 * |1|U, |T: Return target Wizard you control to its owner's hand. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT );
	td1.preferred_controller = player;
	td1.allowed_controller = player;
	td1.required_subtype = SUBTYPE_WIZARD;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_BOUNCE_WIZARD
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(2), 0, &td1, NULL) )
		{
			choice = CHOICE_BOUNCE_WIZARD;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_BOUNCE_WIZARD && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Bounce a wizard you control", 1, 5);

			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_BOUNCE_WIZARD ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_U(2), 0,
										&td1, "Select target Wizard you control.");
			if( spell_fizzled != 1 ){
				if( get_id(instance->targets[0].player, instance->targets[0].card) == CARD_ID_VENSER_SHAPER_SAVANT ){
					ai_modifier += 48;
				}
			}
			else{
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_BOUNCE_WIZARD){
			if( valid_target(&td1) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_riptide_replicator(int player, int card, event_t event){

	/* Riptide Replicator	|X|4
	 * Artifact
	 * As ~ enters the battlefield, choose a color and a creature type.
	 * ~ enters the battlefield with X charge counters on it.
	 * |4, |T: Put an X/X creature token of the chosen color and type onto the battlefield, where X is the number of charge counters on ~. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		instance->info_slot = 0;
		charge_mana(player, COLOR_ARTIFACT, -1);
		if( spell_fizzled != 1 ){
			instance->info_slot = x_value;
		}
	}

	enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, get_card_instance(player, card)->info_slot);

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		instance->targets[1].player = 1<<choose_a_color_and_show_legacy(player, card, player, -1);
		instance->targets[1].card = select_a_subtype(player, card);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int pt = count_counters(instance->parent_controller, instance->parent_card, COUNTER_CHARGE);
		generate_custom_token_from_subtype(instance->parent_controller, instance->parent_card, instance->targets[1].card,
											instance->targets[1].player, pt, pt, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL);
}

int card_riptide_shapeshifter(int player, int card, event_t event){
/* CARD_ID_RIPTIDE_SHAPESHIFTER	6241
Riptide Shapeshifter	|3|U|U
Creature - Shapeshifter 3/3
|2|U|U, Sacrifice ~: Choose a creature type. Reveal cards from the top of your library until you reveal a creature card of that type.
Put that card onto the battlefield and shuffle the rest into your library. */

	if(event == EVENT_ACTIVATE ){
		if( check_state(player, card, STATE_BLOCKING) ){
			card_instance_t *instance = get_card_instance(player, card);
			if( get_power(1-player, instance->blocking) >= get_toughness(player, card) &&
				get_toughness(1-player, instance->blocking) < get_power(player, card) )
			{
				ai_modifier += 96;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.subtype = select_a_subtype(player, card);
		reveal_card_until_you_reveal_test(player, player, &this_test, RCUYRT_PUT_TEST_INTO_PLAY | RCUYRT_SHUFFLE_REST);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XU(2, 2), 0, NULL, NULL);
}

int card_rorix_bladewing(int player, int card, event_t event)
{
  /* Rorix Bladewing	|3|R|R|R
   * Legendary Creature - Dragon 6/5
   * Flying, haste */

  check_legend_rule(player, card, event);
  haste(player, card, event);
  return 0;
}

static int rotlung_reanimator_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_ROTLUNG_REANIMATOR ){
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
						//Will trigger also if this is not a Cleric when it goes into the graveyard;
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
				if( has_subtype(affected_card_controller, affected_card, SUBTYPE_CLERIC) ){
					count_for_gfp_ability(player, card, event, ANYBODY, TYPE_PERMANENT, NULL);
				}
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			generate_tokens_by_id(player, card, CARD_ID_ZOMBIE, amount);
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

int card_rotlung_reanimator(int player, int card, event_t event){
	/* Rotlung Reanimator	|2|B
	 * Creature - Zombie Cleric 2/2
	 * Whenever ~ or another Cleric dies, put a 2/2 |Sblack Zombie creature token onto the battlefield. */

	 if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)rotlung_reanimator_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &rotlung_reanimator_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( affect_me(player, card) || has_subtype(affected_card_controller, affected_card, SUBTYPE_CLERIC) ){
			count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			generate_tokens_by_id(player, card, CARD_ID_ZOMBIE, amount);
		}
	}

	return 0;
}

//Sandskin => Gaseous Form 0x200c442

int card_seaside_haven(int player, int card, event_t event){
	/* Seaside Haven	""
	 * Land
	 * |T: Add |C to your mana pool.
	 * |W|U, |T, Sacrifice a Bird: Draw a card. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Bird to sacrifice.");
	test.subtype = SUBTYPE_BIRD;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAC_AND_DRAW
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_WU(1, 1), 0, NULL, NULL,
															0, &test, 0) )
		{
			choice = CHOICE_SAC_AND_DRAW;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_SAC_AND_DRAW && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Sac a Bird and draw", 1, 5);

			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_SAC_AND_DRAW ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED, MANACOST_WU(1, 1), 0, NULL, NULL,
															0, &test, 0);
			if( spell_fizzled != 1 ){
				if( hand_count[player] < 2 ){
					ai_modifier += (count_subtype(player, TYPE_PERMANENT, SUBTYPE_BIRD) * 8);
				}
			}
			else{
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAC_AND_DRAW ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_secluded_steppe(int player, int card, event_t event){
/* CARD_ID_SECLUDED_STEPPE	6253
Secluded Steppe	""
Land
~ enters the battlefield tapped.
|T: Add |W to your mana pool.
Cycling |W (|W, Discard this card: Draw a card.) */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return cycling(player, card, event, MANACOST_W(1));
	}

	comes_into_play_tapped(player, card, event);

	return mana_producer(player, card, event);
}

int card_shared_triumph(int player, int card, event_t event){
/* CARD_ID_SHARED_TRIUMPH	6257
Shared Triumph	|1|W
Enchantment
As ~ enters the battlefield, choose a creature type.
Creatures of the chosen type get +1/+1. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance( player, card )->info_slot = select_a_subtype(player, card);
	}

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && in_play(player, card) && ! is_humiliated(player, card) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
			has_subtype(affected_card_controller, affected_card, get_card_instance( player, card )->info_slot) )
		{
			event_result++;
		}
	}

	return global_enchantment(player, card, event);
}

int card_shepherd_of_rot(int player, int card, event_t event){
/* CARD_ID_SHEPHERD_OF_ROT	6258
Shepherd of Rot	|1|B
Creature - Zombie Cleric 1/1
|T: Each player loses 1 life for each Zombie on the battlefield. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int amount = count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_ZOMBIE);
		lose_life(player, amount);
		lose_life(1-player, amount);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

static void sigil_of_the_new_dawn_ability(int player, int card, int cless, int black, int blue, int green, int red, int white){
	card_instance_t *instance = get_card_instance(player, card);

	int stored[18];
	int sc = 0;
	int i;
	for(i=0; i<10; i++){
		int iid = instance->targets[i].player;
		if( iid == -1 ){
			break;
		}
		stored[sc] = iid;
		sc++;
		iid = instance->targets[i].card;
		if( iid == -1 ){
			break;
		}
		stored[sc] = iid;
		sc++;
	}
	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to return");
	while( sc && has_mana_multi(player, cless, black, blue, green, red, white) ){
			int selected = select_card_from_zone(player, player, stored, sc, 0, AI_MAX_VALUE, -1, &test);
			if( selected > -1 && charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER,
																	player, cless, black, blue, green, red, white) )
			{
				int iid = stored[selected];
				for(i=selected; i<(sc-1); i++){
					stored[i] = stored[i+1];
				}
				sc--;
				for(i = count_graveyard(player)-1; i > -1; i--){
					if( get_grave(player)[i] == iid ){
						add_card_to_hand(player, iid);
						remove_card_from_grave(player, i);
						break;
					}
				}
			}
			else{
				break;
			}
	}
}

static int sigil_of_the_new_dawn_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_SIGIL_OF_THE_NEW_DAWN ){
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
						if( ! is_what(p, c, TYPE_CREATURE) ){
							count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
						}
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
						if( instance->targets[11].player < 0 ){
							instance->targets[11].player = 0;
						}
						SET_BYTE1(instance->targets[11].player) = 1;
					}
				}
				count_for_gfp_ability_and_store_values(player, card, event, player, TYPE_CREATURE, NULL,
														GFPC_EXTRA_SKIP_TOKENS | GFPC_TRACK_DEAD_CREATURES, 0);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			sigil_of_the_new_dawn_ability(player, card, MANACOST_XW(1, 1));
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

int card_sigil_of_the_new_dawn(int player, int card, event_t event){
/*
Sigil of the New Dawn |3|W
Enchantment
Whenever a creature is put into your graveyard from the battlefield, you may pay {1}{W}. If you do, return that card to your hand.
*/
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)sigil_of_the_new_dawn_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &sigil_of_the_new_dawn_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability_and_store_values(player, card, event, player, TYPE_CREATURE, NULL,
												GFPC_EXTRA_SKIP_TOKENS | GFPC_TRACK_DEAD_CREATURES, 0);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			sigil_of_the_new_dawn_ability(player, card, MANACOST_XW(1, 1));
		}
	}

	return global_enchantment(player, card, event);
}

int card_silent_specter(int player, int card, event_t event){
/* CARD_ID_SILENT_SPECTER	6261
Silent Specter	|4|B|B
Creature - Specter 4/4
Flying
Whenever ~ deals combat damage to a player, that player discards two cards.
Morph |3|B|B (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		new_multidiscard(0, times_damaged[0]*2, 0, player);
		new_multidiscard(1, times_damaged[1]*2, 0, player);
	}

	return morph(player, card, event, MANACOST_XB(3, 2));
}

int card_silklash_spider(int player, int card, event_t event){
/* CARD_ID_SILKLASH_SPIDER	6262
Silklash Spider	|3|G|G
Creature - Spider 2/7
Reach
|X|G|G: ~ deals X damage to each creature with flying. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		new_damage_all(instance->parent_controller, instance->parent_card, ANYBODY, instance->info_slot, 0, &this_test);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XG(-1, 2), 0, NULL, NULL);
}

int card_silvos_rogue_elemental(int player, int card, event_t event)
{
  /* Silvos, Rogue Elemental	|3|G|G|G
   * Legendary Creature - Elemental 8/5
   * Trample
   * |G: Regenerate ~. */

	check_legend_rule(player, card, event);
	return regeneration(player, card, event, MANACOST_G(1));
}

int card_skirk_fire_marshal(int player, int card, event_t event){
/* CARD_ID_SKIRK_FIRE_MARSHAL	6264
Skirk Fire Marshal	|3|R|R
Creature - Goblin 2/2
Protection from |Sred
Tap five untapped Goblins you control: ~ deals 10 damage to each creature and each player. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);
	td1.illegal_state = TARGET_STATE_TAPPED;
	td1.required_subtype = SUBTYPE_GOBLIN;
	td1.preferred_controller = player;
	td1.allowed_controller = player;
	td1.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		new_damage_all(instance->parent_controller, instance->parent_card, ANYBODY, 10, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, &td1, 5);
}

int card_skirk_prospector(int player, int card, event_t event){
/* CARD_ID_SKIRK_PROSPECTOR	6265
Skirk Prospector	|R
Creature - Goblin 1/1
Sacrifice a Goblin: Add |R to your mana pool. */

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_GOBLIN;
		if( can_use_activated_abilities(player, card) && new_can_sacrifice_as_cost(player, card, &this_test) ){
			declare_mana_available(player, COLOR_RED, 1);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a Goblin to sacrifice.");
	this_test.subtype = SUBTYPE_GOBLIN;

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_use_activated_abilities(player, card) ){
			return new_can_sacrifice_as_cost(player, card, &this_test);
		}
	}

	if(event == EVENT_ACTIVATE ){
		if( new_sacrifice(player, card, player, SAC_AS_COST, &this_test) ){
			produce_mana(player, COLOR_RED, 1);
		}
		else{
			spell_fizzled = 1;
		}
	}

	return 0;
}

int card_slate_of_ancestry(int player, int card, event_t event){
	/* Slate of Ancestry	|4
	 * Artifact
	 * |4, |T, Discard your hand: Draw a card for each creature you control. */

	if( event == EVENT_ACTIVATE ){
		int amount = count_subtype(player, TYPE_CREATURE, -1);
		if( hand_count[player] < amount ){
			ai_modifier += (amount * 8);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, count_subtype(player, TYPE_CREATURE, -1));
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL);
}

int card_slice_and_dice(int player, int card, event_t event){
	/* Slice and Dice	|4|R|R
	 * Sorcery
	 * ~ deals 4 damage to each creature.
	 * Cycling |2|R
	 * When you cycle ~, you may have it deal 1 damage to each creature. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_damage_all(player, card, ANYBODY, 4, NDA_ALL_CREATURES, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XR(2, 1));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		draw_a_card(player);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.toughness = 2;
		this_test.toughness_flag = F5_TOUGHNESS_LESSER_THAN_VALUE;

		int priority_damage = (check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test)-
								check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &this_test))*5;

		int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
							"1 damage to all creatures", 1,	priority_damage,
							"Decline", 1, 1);
		if( choice == 1 ){
			APNAP(p, {new_damage_all(player, card, p, 1, NDA_ALL_CREATURES, NULL);};);
		}
	}

	return 0;
}

int card_smother(int player, int card, event_t event){
/* CARD_ID_SMOTHER	6270
Smother	|1|B
Instant
Destroy target creature with converted mana cost 3 or less. It can't be regenerated. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.extra = 3;
	td.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature with CMC 3 or less.", 1, NULL);

}

int card_snarling_undorak(int player, int card, event_t event){
/* CARD_ID_SNARLING_UNDORAK	6272
Snarling Undorak	|2|G|G
Creature - Beast 3/3
|2|G: Target Beast creature gets +1/+1 until end of turn.
Morph |1|G|G (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.preferred_controller = player;
	td1.required_subtype = SUBTYPE_BEAST;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XG(2, 1), 0,
										&td1, "Select target Beast creature.");
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
	}

	return morph(player, card, event, MANACOST_XG(1, 2));
}

int card_solar_blast(int player, int card, event_t event){
	/* Solar Blast	|3|R	0x20092c4
	 * Instant
	 * ~ deals 3 damage to any target.
	 * Cycling |1|R|R
	 * When you cycle ~, you may have it deal 1 damage to any target. */
	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_XR(1, 2));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		draw_a_card(player);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		if( pick_target(&td, "TARGET_ANY") ){
			damage_target0(player, card, 1);
		}
	}

	return card_lightning_bolt(player, card, event);
}

int card_soulless_one(int player, int card, event_t event){
	/* Soulless One	|3|B
	 * Creature - Zombie Avatar 100/100
	 * ~'s power and toughness are each equal to the number of Zombies on the battlefield plus the number of Zombie cards in all graveyards. */

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && player > -1 && affect_me(player, card) && !is_humiliated(player, card)){
		event_result += count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_ZOMBIE);
		event_result += count_graveyard_by_subtype(ANYBODY, SUBTYPE_ZOMBIE);
	}

	return 0;
}

int card_sparksmith(int player, int card, event_t event){
/* CARD_ID_SPARKSMITH	6275
Sparksmith	|1|R
Creature - Goblin 1/1
|T: ~ deals X damage to target creature and X damage to you, where X is the number of Goblins on the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_GOBLIN);
			damage_target0(player, card, amount);
			damage_player(player, amount, instance->parent_controller, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_starlit_sanctum(int player, int card, event_t event){
	/* Starlit Sanctum	""
	 * Land
	 * |T: Add |C to your mana pool.
	 * |W, |T, Sacrifice a Cleric creature: You gain life equal to the sacrificed creature's toughness.
	 * |B, |T, Sacrifice a Cleric creature: Target player loses life equal to the sacrificed creature's power. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a Cleric creature to sacrifice.");
	test.subtype = SUBTYPE_CLERIC;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAC_GAIN,
		CHOICE_SAC_LOSE
	};

	if( event == EVENT_ACTIVATE ){
		int abils[4] = {
			            0,
						1,
						generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_W(1), 0,
																		 NULL, NULL, 0, &test, 0),
						generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET,
																		 MANACOST_B(1), 0, &td, NULL, 0, &test, 0)
		};
		int choice = instance->info_slot =  CHOICE_MANA;
		if( ! paying_mana() ){
			if( (abils[2] && life[player] < 6) || (abils[3] && life[1-player] < 6) ){
				ai_modifier += 48;
			}
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", abils[1], 5,
							"Sac Cleric & gain life", abils[2], life[player] < 6 ? 10 : 1,
							"Sac Cleric & weaken opp", abils[3], life[1-player] < 6 ? 10 : 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_SAC_GAIN ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED, MANACOST_W(1), 0, NULL, NULL, 0,
															             &test, 0);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
		if( choice == CHOICE_SAC_LOSE ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1), 0,
															             &td, "TARGET_PLAYER", 0, &test, 0);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAC_GAIN ){
		        gain_life(player, instance->info_slot);
		}
		if( instance->info_slot == CHOICE_SAC_LOSE ){
			if( valid_target(&td) ){
				lose_life(instance->targets[0].player, instance->info_slot);
			}
		}
	}

	return 0;
}

int card_starstorm(int player, int card, event_t event){
	/* Starstorm	|X|R|R
	 * Instant
	 * ~ deals X damage to each creature.
	 * Cycling |3 */

	if (IS_CASTING(player, card, event)){
		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot > 0 ){
				new_damage_all(player, card, ANYBODY, instance->info_slot, NDA_ALL_CREATURES, NULL);
			}
			kill_card(player, card, KILL_DESTROY);
		}

		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}
	return cycling(player, card, event, MANACOST_X(3));
}

int card_steely_resolve(int player, int card, event_t event){
/* CARD_ID_STEELY_RESOLVE	6285
Steely Resolve	|1|G
Enchantment
As ~ enters the battlefield, choose a creature type.
Creatures of the chosen type have shroud. (They can't be the targets of spells or abilities.) */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance( player, card )->info_slot = select_a_subtype(player, card);
	}

	if( event == EVENT_ABILITIES && in_play(player, card) && ! is_humiliated(player, card) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
			has_subtype(affected_card_controller, affected_card, get_card_instance( player, card )->info_slot) )
		{
			event_result |= KEYWORD_SHROUD;
		}
	}

	return global_enchantment(player, card, event);
}

int card_supreme_inquisitor(int player, int card, event_t event){
/* CARD_ID_SUPREME_INQUISITOR	6288
Supreme Inquisitor	|3|U|U
Creature - Human Wizard 1/3
Tap five untapped Wizards you control: Search target player's library for up to five cards and exile them. Then that player shuffles his or her library.*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);
	td1.illegal_state = TARGET_STATE_TAPPED;
	td1.required_subtype = SUBTYPE_WIZARD;
	td1.preferred_controller = player;
	td1.allowed_controller = player;
	td1.illegal_abilities = 0;

	target_definition_t td2;
	default_target_definition(player, card, &td2, 0);
	td2.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td2) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");
			this_test.qty = 5;
			new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_MAX_VALUE, &this_test);
		}
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td2, "TARGET_PLAYER",
															&td1, 5);
}

int card_symbiotic_beast(int player, int card, event_t event){
/* CARD_ID_SYMBIOTIC_BEAST	6289
Symbiotic Beast	|4|G|G
Creature - Insect Beast 4/4
When ~ dies, create four 1/1 |Sgreen Insect creature tokens. */

	symbiotic_creature(player, card, event, CARD_ID_INSECT, 4);
	return 0;
}

int card_symbiotic_elf(int player, int card, event_t event){
/* CARD_ID_SYMBIOTIC_ELF	6290
Symbiotic Elf	|3|G
Creature - Elf 2/2
When ~ dies, create two 1/1 |Sgreen Insect creature tokens. */

	symbiotic_creature(player, card, event, CARD_ID_INSECT, 2);
	return 0;
}

int card_symbiotic_wurm(int player, int card, event_t event){
/* CARD_ID_SYMBIOTIC_WURM	1484
Symbiotic Wurm	|5|G|G|G
Creature - Wurm 7/7
When ~ dies, create seven 1/1 |Sgreen Insect creature tokens. */

	symbiotic_creature(player, card, event, CARD_ID_INSECT, 7);
	return 0;
}

int card_tempting_wurm(int player, int card, event_t event){
/* Tempting Wurm	|1|G
 * Creature - Wurm 5/5
 * When ~ enters the battlefield, each opponent may put any number of artifact, creature, enchantment, and/or land cards from his or her
 * 		hand onto the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ARTIFACT | TYPE_CREATURE | TYPE_ENCHANTMENT | TYPE_LAND,
									"Select an artifact, creature, enchantment or land to put into play.");
		this_test.zone = TARGET_ZONE_HAND;
		while( check_battlefield_for_special_card(player, card, 1-player, 0, &this_test) ){
			if( new_global_tutor(1-player, 1-player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test) == -1 ){
				break;
			}
		}
	}

	return 0;
}

int card_tephraderm(int player, int card, event_t event){
/*
Tephraderm |4|R
Creature — Beast 4/5
Whenever a creature deals damage to Tephraderm, Tephraderm deals that much damage to that creature.
Whenever a spell deals damage to Tephraderm, Tephraderm deals that much damage to that spell's controller.
*/
	if( is_humiliated(player, card) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_DEAL_DAMAGE ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->info_slot > 0 ){
			if( damage->damage_target_card == card && damage->damage_target_player == player ){
				int pos = instance->info_slot;
				if( pos < 10 ){
					instance->targets[pos].player = damage->damage_source_player;
					instance->targets[pos].card = 0;
					if( damage->targets[3].player & TYPE_CREATURE ){
						SET_BYTE0(instance->targets[pos].card) = damage->damage_source_card;
						SET_BYTE1(instance->targets[pos].card) = damage->info_slot;
						SET_BYTE2(instance->targets[pos].card) = 1;
					}
					else{
						SET_BYTE1(instance->targets[pos].card) = damage->info_slot;
					}
				}
			}
		}
	}

	if( instance->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
		reason_for_trigger_controller == player )
	{
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				int i;
				for(i=0;i<instance->info_slot;i++){
					int t_player = instance->targets[i].player;
					instance->targets[i].player = -1;
					int dmg = BYTE1(instance->targets[i].card);
					int t_card = BYTE2(instance->targets[i].card) ? BYTE1(instance->targets[i].card) : -1;
					instance->targets[i].card = -1;
					damage_creature(t_player, t_card, dmg, player, card);
				}
				instance->info_slot = 0;
		}
	}
	return 0;
}

int card_tranquil_thicket(int player, int card, event_t event){
/* CARD_ID_TRANQUIL_THICKET	6300
Tranquil Thicket	""
Land
~ enters the battlefield tapped.
|T: Add |G to your mana pool.
Cycling |G (|G, Discard this card: Draw a card.) */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return cycling(player, card, event, MANACOST_G(1));
	}

	comes_into_play_tapped(player, card, event);

	return mana_producer(player, card, event);
}

int card_tribal_unity(int player, int card, event_t event){
	/* Tribal Unity	|X|2|G
	 * Instant
	 * Creatures of the creature type of your choice get +X/+X until end of turn. */

	if(event == EVENT_CAN_CAST){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		charge_mana(player, COLOR_COLORLESS, -1);
		if( spell_fizzled != 1 ){
			set_x_for_x_spells(player, card, event, x_value);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_subtype_until_eot(player, card, 2, select_a_subtype(player, card), instance->info_slot, instance->info_slot, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_true_believer(int player, int card, event_t event){
/* CARD_ID_TRUE_BELIEVER	6305
True Believer	|W|W
Creature - Human Cleric 2/2
You have shroud. (You can't be the target of spells or abilities.) */

	give_shroud_to_player(player, card, event);
	return 0;
}

int card_undead_gladiator(int player, int card, event_t event){
	/* Undead Gladiator	|1|B|B
	 * Creature - Zombie Barbarian 3/1
	 * |1|B, Discard a card: Return ~ from your graveyard to your hand. Activate this ability only during your upkeep.
	 * Cycling |1|B */

	if( IS_AI(player) ){
		if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP && hand_count[player] > 0 && has_mana_multi(player, 1, 1, 0, 0, 0, 0) ){
			card_instance_t *instance = get_card_instance( player, card );
			int choice = do_dialog(player, player, card, -1, -1," Return Undead Gladiator to hand\n Pass", 0);
			if( choice == 0 ){
				charge_mana_multi(player, 1, 1, 0, 0, 0, 0);
				if( spell_fizzled != 1 ){
					discard(player, 0, player);
					instance->state &= ~STATE_INVISIBLE;
					hand_count[player]++;
					return -1;
				}
			}
			return -2;
		}
	}
	else{
		if( event == EVENT_GRAVEYARD_ABILITY && has_mana_multi(player, MANACOST_XB(1, 1)) && current_phase == PHASE_UPKEEP && hand_count[player] ){
			return GA_RETURN_TO_HAND;
		}
		if( event == EVENT_PAY_FLASHBACK_COSTS ){
			charge_mana_multi(player, MANACOST_XB(1, 1));
			if( spell_fizzled != 1 ){
				discard(player, 0, player);
				return GAPAID_REMOVE;
			}
		}
	}

	return cycling(player, card, event, MANACOST_XB(1, 1));
}

int card_unholy_grotto(int player, int card, event_t event){
	/* Unholy Grotto	""
	 * Land
	 * |T: Add |C to your mana pool.
	 * |B, |T: Put target Zombie card from your graveyard on top of your library. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select target Zombie card.");
	test.subtype = SUBTYPE_ZOMBIE;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_ZOMBIE
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_B(1), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER) )
		{
			choice = CHOICE_ZOMBIE;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_ZOMBIE && ! IS_AI(player) ){
			if( count_subtype(player, TYPE_CREATURE, -1) == 0 ){
				ai_modifier += (current_turn != player ? 48 : 24);
			}
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Return a Zombie", 1, 5);

			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_ZOMBIE ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_ZOMBIE){
			int selected = validate_target_from_grave_source(player, card, player, 0);
			if( selected > -1 ){
				from_graveyard_to_deck(player, selected, 1);
			}
		}
	}

	return 0;
}

int card_visara_the_dreadful(int player, int card, event_t event)
{
  /* Visara the Dreadful	|3|B|B|B
   * Legendary Creature - Gorgon 5/5
   * Flying
   * |T: Destroy target creature. It can't be regenerated. */

  check_legend_rule(player, card, event);

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_voice_of_the_woods(int player, int card, event_t event){
	/* Voice of the Woods	|3|G|G
	 * Creature - Elf 2/2
	 * Tap five untapped Elves you control: Put a 7/7 |Sgreen Elemental creature token with trample onto the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);
	td1.illegal_state = TARGET_STATE_TAPPED;
	td1.required_subtype = SUBTYPE_ELF;
	td1.preferred_controller = player;
	td1.allowed_controller = player;
	td1.illegal_abilities = 0;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
		token.pow = 7;
		token.tou = 7;
		token.color_forced = COLOR_TEST_GREEN;
		token.key_plus = KEYWORD_TRAMPLE;
		generate_token(&token);
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, &td1, 5);
}

int card_voidmage_prodigy(int player, int card, event_t event){
/* CARD_ID_VOIDMAGE_PRODIGY	6312
Voidmage Prodigy	|U|U
Creature - Human Wizard 2/1
|U|U, Sacrifice a Wizard: Counter target spell.
Morph |U (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	if( ! IS_GAA_EVENT(event) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a Wizard to sacrifice.");
		test.subtype = SUBTYPE_WIZARD;

		if( event == EVENT_ACTIVATE ){
			if( get_cmc(card_on_stack_controller, card_on_stack) >= 4 ){
				ai_modifier += 48;
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( counterspell_validate(player, card, NULL, 0) ){
				card_instance_t *instance = get_card_instance(player, card);
				real_counter_a_spell(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card);
			}
		}

		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_SPELL_ON_STACK, MANACOST0, 0, NULL, NULL,
																0, &test, 0);
	}

	return morph(player, card, event, MANACOST_U(1));
}

int card_wall_of_mulch(int player, int card, event_t event){
/* CARD_ID_WALL_OF_MULCH	6314
Wall of Mulch	|1|G
Creature - Wall 0/4
Defender (This creature can't attack.)
|G, Sacrifice a Wall: Draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Wall to sacrifice.");
	test.subtype = SUBTYPE_WALL;

	if( event == EVENT_ACTIVATE ){
		if( check_state(player, card, STATE_BLOCKING) ){
			card_instance_t *instance = get_card_instance(player, card);
			if( get_power(1-player, instance->blocking) >= get_toughness(player, card) &&
				get_toughness(1-player, instance->blocking) < get_power(player, card) )
			{
				ai_modifier += 96;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL, 0, &test, 0);
}

int card_weathered_wayfarer(int player, int card, event_t event){
/* CARD_ID_WEATHERED_WAYFARER	6316
Weathered Wayfarer	|W
Creature - Human Nomad Cleric 1/1
|W, |T: Search your library for a land card, reveal it, and put it into your hand. Then shuffle your library.
Activate this ability only if an opponent controls more lands than you. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( count_permanents_by_type(player, TYPE_LAND) >= count_permanents_by_type(1-player, TYPE_LAND) ){
			return 0;
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( get_total_available_lands(player) < 4 ){
			ai_modifier += 48;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a land card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_W(1), 0, NULL, NULL);
}

int card_weird_harvest(int player, int card, event_t event){
/* Weird Harvest	|X|G|G
 * Sorcery
 * Each player may search his or her library for up to X creature cards, reveal those cards, and put them into his or her hand.
 *		Then each player who searched his or her library this way shuffles it. */

	if(event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		APNAP(p, {
					int choice = 0;
					if( ! duh_mode(p) ){
						choice = do_dialog(p, player, card, player, card, " Search your library\n Pass", 0);
					}
					if( choice == 0 ){
						test_definition_t this_test;
						new_default_test_definition(&this_test, TYPE_CREATURE, "Select a Creature card.");
						this_test.qty = instance->info_slot;
						new_global_tutor(p, p, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
						shuffle(p);
					}
				};
			);
			kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_wellwisher(int player, int card, event_t event){
/* CARD_ID_WELLWISHER	6318
Wellwisher	|1|G
Creature - Elf 1/1
|T: You gain 1 life for each Elf on the battlefield. */

	if( event == EVENT_ACTIVATE ){
		if( life[player] < 6 ){
			ai_modifier += (count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_ELF)*8);
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		int amount = count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_ELF);
		gain_life(player, amount);
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_whipcorder(int player, int card, event_t event){
/* CARD_ID_WHIPCORDER	6320
Whipcorder	|W|W
Creature - Human Soldier Rebel 2/2
|W, |T: Tap target creature.
Morph |W (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	if( IS_GAA_EVENT(event) ){
		return card_squall_drifter(player, card, event);
	}
	return morph(player, card, event, MANACOST_W(1));
}

int card_windswept_heath(int player, int card, event_t event){
	/* Windswept Heath	""
	 * Land
	 * |T, Pay 1 life, Sacrifice ~: Search your library for |Ha Forest or |H2Plains card and put it onto the battlefield. Then shuffle your library. */
	return fetchland(player, card, event, SUBTYPE_FOREST, SUBTYPE_PLAINS, 1);
}

int card_wirewood_herald(int player, int card, event_t event ){
/* CARD_ID_WIREWOOD_HERALD	6322
Wirewood Herald	|1|G
Creature - Elf 1/1
When ~ dies, you may search your library for an Elf card, reveal that card, put it into your hand, then shuffle your library. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		char msg[100] = "Select an Elf card.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, msg);
		this_test.subtype = SUBTYPE_ELF;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return 0;
}

int card_wirewood_lodge(int player, int card, event_t event){
	/* Wirewood Lodge	""
	 * Land
	 * |T: Add |C to your mana pool.
	 * |G, |T: Untap target Elf. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT );
	td1.preferred_controller = player;
	td1.allowed_controller = player;
	td1.required_subtype = SUBTYPE_ELF;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_UNTAP_ELF
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_G(1), 0, &td1, NULL) )
		{
			choice = CHOICE_UNTAP_ELF;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_UNTAP_ELF && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Untap target Elf", 1, 5);

			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_UNTAP_ELF ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_G(1), 0,
										&td1, "Select target Elf.");
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_UNTAP_ELF){
			if( valid_target(&td1) ){
				untap_card(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_wirewood_pride(int player, int card, event_t event){
	/* Wirewood Pride	|G
	 * Instant
	 * Target creature gets +X/+X until end of turn, where X is the number of Elves on the battlefield. */
	int amt = (event == EVENT_CHECK_PUMP || event == EVENT_RESOLVE_SPELL) ? count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_ELF) : 0;
	return vanilla_instant_pump(player, card, event, ANYBODY, player, amt, amt, 0, 0);
}

int card_wirewood_savage(int player, int card, event_t event){
/* CARD_ID_WIREWOOD_SAVAGE	6325
Wirewood Savage	|2|G
Creature - Elf 2/2
Whenever a Beast enters the battlefield, you may draw a card. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_BEAST;

		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_wooded_foothills(int player, int card, event_t event){
	/* Wooded Foothills	""
	 * Land
	 * |T, Pay 1 life, Sacrifice ~: Search your library for |Ha Mountain or |H2Forest card and put it onto the battlefield. Then shuffle your library. */
	return fetchland(player, card, event, SUBTYPE_MOUNTAIN, SUBTYPE_FOREST, 1);
}

int card_words_of_war(int player, int card, event_t event){
	/* Words of War	|2|R	0x200281b
	 * Enchantment
	 * |1: The next time you would draw a card this turn, ~ deals 2 damage to any target instead. */
	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && reason_for_trigger_controller == player &&
		! suppress_draw && CAN_ACTIVATE(player, card, MANACOST_X(1))// Worded as an activated ability that causes a delayed triggered ability
	  ){
		if(event == EVENT_TRIGGER){
			int trig_mode = RESOLVE_TRIGGER_OPTIONAL;
			if( IS_AI(player) ){
				trig_mode = hand_count[player] >= 5 ? RESOLVE_TRIGGER_MANDATORY : 0;
			}
			event_result |= trig_mode;
		}
		else if( event == EVENT_RESOLVE_TRIGGER &&
				charge_mana_for_activated_ability_while_resolving(player, card, EVENT_RESOLVE_ACTIVATION, MANACOST_X(1)) )
		{
				target_definition_t td;
				default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
				td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

				card_instance_t *instance = get_card_instance( player, card );

				if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
					damage_creature_or_player(player, card, event, 2);
					suppress_draw = 1;
				}
				instance->number_of_targets = 0;
		}
	}
	return global_enchantment(player, card, event);
}

int card_words_of_waste(int player, int card, event_t event){
	/* Words of Waste	|2|B
	 * Enchantment
	 * |1: The next time you would draw a card this turn, each opponent discards a card instead. */

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && reason_for_trigger_controller == player &&
		! suppress_draw && CAN_ACTIVATE(player, card, MANACOST_X(1))// Worded as an activated ability that causes a delayed triggered ability
	  ){
		if(event == EVENT_TRIGGER){
			int trig_mode = RESOLVE_TRIGGER_OPTIONAL;
			if( IS_AI(player) ){
				trig_mode = hand_count[player] >= 5 ? RESOLVE_TRIGGER_MANDATORY : 0;
			}
			event_result |= trig_mode;
		}
		else if( event == EVENT_RESOLVE_TRIGGER &&
				charge_mana_for_activated_ability_while_resolving(player, card, EVENT_RESOLVE_ACTIVATION, MANACOST_X(1)) )
		{
				discard(1-player, 0, player);
				suppress_draw = 1;
		}
	}
	return global_enchantment(player, card, event);
}

int card_words_of_wilding(int player, int card, event_t event){
	/* Words of Wilding	|2|G
	 * Enchantment
	 * |1: The next time you would draw a card this turn, put a 2/2 |Sgreen Bear creature token onto the battlefield instead. */

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && reason_for_trigger_controller == player &&
		! suppress_draw && CAN_ACTIVATE(player, card, MANACOST_X(1))// Worded as an activated ability that causes a delayed triggered ability
	  ){
		if(event == EVENT_TRIGGER){
			int trig_mode = RESOLVE_TRIGGER_OPTIONAL;
			if( IS_AI(player) ){
				trig_mode = hand_count[player] >= 5 ? RESOLVE_TRIGGER_MANDATORY : 0;
			}
			event_result |= trig_mode;
		}
		else if( event == EVENT_RESOLVE_TRIGGER &&
				charge_mana_for_activated_ability_while_resolving(player, card, EVENT_RESOLVE_ACTIVATION, MANACOST_X(1)) )
		{
				generate_token_by_id(player, card, CARD_ID_BEAR);
				suppress_draw = 1;
		}
	}
	return global_enchantment(player, card, event);
}

int card_words_of_wind(int player, int card, event_t event){
	/* Words of Wind	|2|U
	 * Enchantment
	 * |1: The next time you would draw a card this turn, each player returns a permanent he or she controls to its owner's hand instead. */

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && reason_for_trigger_controller == player &&
		! suppress_draw && CAN_ACTIVATE(player, card, MANACOST_X(1))// Worded as an activated ability that causes a delayed triggered ability
	  ){
		if(event == EVENT_TRIGGER){
			int trig_mode = RESOLVE_TRIGGER_OPTIONAL;
			if( IS_AI(player) ){
				trig_mode = hand_count[player] >= 5 ? RESOLVE_TRIGGER_MANDATORY : 0;
			}
			event_result |= trig_mode;
		}
		else if( event == EVENT_RESOLVE_TRIGGER &&
				charge_mana_for_activated_ability_while_resolving(player, card, EVENT_RESOLVE_ACTIVATION, MANACOST_X(1)) )
		{
			card_instance_t *instance = get_card_instance(player, card);
			APNAP(p, {
						target_definition_t td;
						default_target_definition(player, card, &td, TYPE_PERMANENT);
						td.allowed_controller = p;
						td.preferred_controller = p;
						td.who_chooses = p;
						td.allow_cancel = 0;

						if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
							bounce_permanent(instance->targets[0].player, instance->targets[0].card);
						}
						instance->number_of_targets = 0;
					}
			);

			suppress_draw = 1;
		}
	}
	return global_enchantment(player, card, event);
}

int card_words_of_worship(int player, int card, event_t event){
	/* Words of Worship	|2|W
	 * Enchantment
	 * |1: The next time you would draw a card this turn, you gain 5 life instead. */

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && reason_for_trigger_controller == player &&
		! suppress_draw && CAN_ACTIVATE(player, card, MANACOST_X(1))// Worded as an activated ability that causes a delayed triggered ability
	  ){
		if(event == EVENT_TRIGGER){
			int trig_mode = RESOLVE_TRIGGER_OPTIONAL;
			if( IS_AI(player) ){
				trig_mode = life[player] < 6 ? RESOLVE_TRIGGER_MANDATORY : 0;
			}
			event_result |= trig_mode;
		}
		else if( event == EVENT_RESOLVE_TRIGGER &&
				charge_mana_for_activated_ability_while_resolving(player, card, EVENT_RESOLVE_ACTIVATION, MANACOST_X(1)) )
		{
				gain_life(player, 5);
				suppress_draw = 1;
		}
	}
	return global_enchantment(player, card, event);
}

