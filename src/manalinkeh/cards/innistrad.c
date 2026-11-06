#include "manalink.h"

static int turn_card_in_grave_face_up_src(int player, int source, int position_hint, int iid);

// GLOBAL FUNCTIONS

int card_revenant(int player, int card, event_t event);	// in stronghold.c

void human_moon_phases(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[9].card == 66 && current_phase > PHASE_UPKEEP && current_phase < PHASE_DISCARD){
		instance->targets[9].card = 0;
	}

	if( instance->targets[8].player == 0 && instance->targets[8].card == 0 &&
		upkeep_trigger(player, card, event)
	  ){
		transform(player, card);
	}
}

void werewolf_moon_phases(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[9].card == 66 && current_phase > PHASE_UPKEEP && current_phase < PHASE_DISCARD){
		instance->targets[9].card = 0;
	}

	if( (instance->targets[8].player > 1 || instance->targets[8].card > 1)  &&
		upkeep_trigger(player, card, event)
	  ){
		transform(player, card);
	}

}

/* ManalinkEh 2022: 0x2072420; original cards/innistrad.c:37. */
static int turn_card_in_grave_face_up_src(int player, int source, int position_hint, int iid)
{
  int result;
  int *grave_card;

  if ((iid == -1) || (iid_draw_a_card == iid))
  {
    backtrace_and_abort("cards/innistrad.c:39\niid != -1 && iid != iid_draw_a_card");
  }
  result = find_in_graveyard_by_source(player, source, position_hint);
  grave_card = graveyard_ptr_mutable[player] + result;
  if (*grave_card != iid_draw_a_card)
  {
    backtrace_and_abort(
        "cards/innistrad.c:41\ngraveyard_ptr_mutable[player][position] == iid_draw_a_card");
    grave_card = graveyard_ptr_mutable[player] + result;
  }
  types_of_cards_in_graveyard[player] =
      types_of_cards_in_graveyard[player] | (unsigned int)cards_data[iid].type;
  *grave_card = iid;
  return result;
}

int gather_body_parts(int player, int card, int body_parts, int special)
{
  // Hide the card being cast so it cannot be selected as an additional cost.
  int result = 1;
  test_definition_t this_test;
  new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card to remove.");
  this_test.id = 904;
  this_test.id_flag = DOESNT_MATCH;
  int backup[3][body_parts];
  int bc = 0;
  int backup_special[3] = {-1, -1, -1};
  if (special)
  {
    for (int count = count_graveyard(player) - 1; count >= 0; --count)
    {
      if (cards_data[get_grave(player)[count]].id == get_id(player, card))
      {
        int position = find_in_graveyard_by_source(
            player, get_card_instance(player, card)->targets[3].card, count);
        backup_special[0] = turn_card_in_grave_face_down(player, position);
        backup_special[1] = position;
        backup_special[2] = graveyard_source[player][position];
        break;
      }
    }
  }
  for (int i = 0; i < body_parts; ++i)
  {
    int selected =
        new_select_a_card(player, player, TUTOR_FROM_GRAVE, 0, AI_MIN_VALUE, -1, &this_test);
    if (selected == -1)
    {
      result = 0;
      break;
    }
    backup[0][bc] = turn_card_in_grave_face_down(player, selected);
    backup[1][bc] = selected;
    backup[2][bc] = graveyard_source[player][selected];
    ++bc;
  }
  if (backup_special[0] >= 0)
    turn_card_in_grave_face_up_src(player, backup_special[2], backup_special[1], backup_special[0]);
  for (int i = bc - 1; i >= 0; --i)
  {
    int position = turn_card_in_grave_face_up_src(player, backup[2][i], backup[1][i], backup[0][i]);
    if (bc == body_parts)
      rfg_card_from_grave(player, position);
  }
  return result;
}

void skaabs(int player, int card, event_t event, int body_parts){

	if( event == EVENT_MODIFY_COST ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) < body_parts ){
			infinite_casting_cost();
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			if( ! gather_body_parts(player, card, body_parts, 0) ){
				spell_fizzled = 1;
			}
		}
	}
}

int curse(int player, int card, event_t event)
{
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.zone = TARGET_ZONE_PLAYERS;
  td.preferred_controller = 1 - player;
  if (event == EVENT_SHOULD_AI_PLAY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player == player || instance->targets[0].player == player)
      ai_modifier -= 48;
    else
      ai_modifier += 48;
  }
  return enchant_player(player, card, event, &td);
}

int tutor_random_permanent_from_grave(int player, int card, int t_player, int tutor_type, int type, int amount, reanimate_mode_t reanimation_mode){

	int card_added = -1;
	if( count_graveyard_by_type(t_player, type) > 0 ){
		const int *grave = get_grave(t_player);
		int creature_array[99];
		int count = 0;
		int ca_count = 0;
		while( count < count_graveyard(t_player) ){
				if( is_what(-1, grave[count], type) ){
					creature_array[ca_count] = count;
					ca_count++;
				}
				count++;
		}
		if( ca_count <= amount ){
			count = ca_count-1;
			while( count > -1 ){
					if( tutor_type == TUTOR_PLAY ){
						card_added = reanimate_permanent(player, card, t_player, creature_array[count], reanimation_mode);
					}
					if( tutor_type == TUTOR_HAND ){
						card_added = add_card_to_hand(t_player, grave[creature_array[count]]);
						remove_card_from_grave(t_player, creature_array[count]);
					}
					count--;
			}
		}
		else{
			count = 0;
			while( count < amount && ca_count > 0 ){
					int rnd = internal_rand(ca_count);
					if( tutor_type == TUTOR_PLAY ){
						card_added = reanimate_permanent(player, card, t_player, creature_array[rnd], reanimation_mode);
					}
					if( tutor_type == TUTOR_HAND ){
						card_added = add_card_to_hand(t_player, grave[creature_array[rnd]]);
						remove_card_from_grave(t_player, creature_array[rnd]);
					}
					int i;
					for( i = rnd; i < ca_count; i++){
						 creature_array[i] = creature_array[i+1];
					}
					count++;
					ca_count--;
			}
		}
	}
	return card_added;
}

static int can_transform(int player, int card){

	APNAP(p,{
				int c;
				for(c=0; c<active_cards_count[p]; c++){
					if( in_play(p, c) && ! is_humiliated(p, c, AL_STATIC) ){
						int csvid = get_id(p, c);
						switch( csvid ){
								case CARD_ID_IMMERWOLF:
								{
									if( has_subtype(player, card, SUBTYPE_WEREWOLF) && ! has_subtype(player, card, SUBTYPE_HUMAN) ){
										if( p == player ){
											return 0;
										}
									}
									break;

								}
								case CARD_ID_BOUND_BY_MOONSILVER:
								{
									card_instance_t *instance = get_card_instance( p, c );
									if( instance->damage_target_player == player && instance->damage_target_card == card ){
										return 0;
									}
									break;

								}
								default:
									break;

						}
					}
				}
			};
	);

	return 1;
}

void transform(int player, int card)
{

	if( ! can_transform(player, card) ){
		return;
	}

	true_transform(player, card);

	if( ! check_special_flags3(player, card, SF3_CARD_IS_FLIPPED) ){
		set_special_flags3(player, card, SF3_CARD_IS_FLIPPED);
	}
	else{
		remove_special_flags3(player, card, SF3_CARD_IS_FLIPPED);
	}

	card_instance_t* instance = get_card_instance(player, card);
	instance->regen_status |= KEYWORD_RECALC_ALL;
	get_abilities(player, card, EVENT_CHANGE_TYPE, -1);
	get_abilities(player, card, EVENT_SET_COLOR, -1);

	if (is_what(player, card, TYPE_CREATURE))
	{
	  get_abilities(player, card, EVENT_POWER, -1);
	  get_abilities(player, card, EVENT_TOUGHNESS, -1);
	}
	call_card_function_i(instance, player, card, EVENT_TRANSFORMED);
	dispatch_event_with_initial_event_result(player, card, EVENT_ANOTHER_PERMANENT_HAS_TRANSFORMED, 0);
	verify_legend_rule(player, card, cards_data[instance->targets[12].card].id);
}

static void create_spirit(int player, int card, int amount)
{
  token_generation_t token;
  default_token_definition(player, card, CARD_ID_SPIRIT, &token);
  token.key_plus = KEYWORD_FLYING;
  token.color_forced = COLOR_TEST_WHITE;
  token.qty = amount;
  generate_token(&token);
}

// CARDS

int card_army_of_the_damned(int player, int card, event_t event){
  /* Army of the Damned	|5|B|B|B
   * Sorcery
   * Create thirteen tapped 2/2 black Zombie creature tokens.
   * Flashback |7|B|B|B (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
		token.qty = 13;
		token.action = TOKEN_ACTION_TAPPED;
		generate_token(&token);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XB(7, 3));
}

int card_back_from_the_brink(int player, int card, event_t event ){
	/* Back from the Brink	|4|U|U
	 * Enchantment
	 * Exile a creature card from your graveyard and pay its mana cost: Put a token onto the battlefield that's a copy of that card. Activate this ability only any time you could cast a sorcery. */

	 if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST0, 0, NULL, NULL) ){
			const int *grave = get_grave(player);
			int count = 0;
			while( grave[count] != -1 ){
					if( is_what(-1, grave[count], TYPE_CREATURE) ){
						card_ptr_t* c = cards_ptr[ cards_data[grave[count]].id];
						int costs[6] = {c->req.req_colorless > 15 ? 0 : c->req.req_colorless, c->req.req_black, c->req.req_blue, c->req.req_green, c->req.req_red, c->req.req_white};
						costs[0] = get_cost_mod_for_activated_abilities(player, card, costs[0], costs[1], costs[2], costs[3], costs[4], costs[5]);
						if( has_mana_multi(player, costs[0], costs[1], costs[2], costs[3], costs[4], costs[5]) ){
							return 1;
						}
					}
					count++;
			}
		}
	}
	if( event == EVENT_ACTIVATE ){
		card_instance_t *instance = get_card_instance(player, card);
		int playable[2][count_graveyard(player)];
		int pc = 0;
		const int *grave = get_grave(player);
		int count = 0;
		while( grave[count] != -1 ){
				if( is_what(-1, grave[count], TYPE_CREATURE) ){
					card_ptr_t* c = cards_ptr[ cards_data[grave[count]].id];
					int costs[6] = {c->req.req_colorless > 15 ? 0 : c->req.req_colorless, c->req.req_black, c->req.req_blue, c->req.req_green, c->req.req_red, c->req.req_white};
					costs[0] = get_cost_mod_for_activated_abilities(player, card, costs[0], costs[1], costs[2], costs[3], costs[4], costs[5]);
					if( has_mana_multi(player, costs[0], costs[1], costs[2], costs[3], costs[4], costs[5]) ){
						playable[0][pc] = grave[count];
						playable[1][pc] = count;
						pc++;
					}
				}
				count++;
		}
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
		int selected = select_card_from_zone(player, player, playable[0], pc, 0, AI_MAX_VALUE, -1, &this_test);
		if( selected != -1 ){
			card_ptr_t* c = cards_ptr[ cards_data[playable[0][selected]].id];
			int costs[6] = {c->req.req_colorless > 15 ? 0 : c->req.req_colorless, c->req.req_black, c->req.req_blue, c->req.req_green, c->req.req_red, c->req.req_white};
			costs[0] = get_cost_mod_for_activated_abilities(player, card, costs[0], costs[1], costs[2], costs[3], costs[4], costs[5]);
			charge_mana_multi(player, costs[0], costs[1], costs[2], costs[3], costs[4], costs[5]);
			if( spell_fizzled != 1 ){
				instance->info_slot = cards_data[playable[0][selected]].id;
				rfg_card_from_grave(player, playable[1][selected]);
			}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, get_card_instance(player, card)->info_slot, &token);
		token.no_sleight = 1;
		generate_token(&token);
	}
	return global_enchantment(player, card, event);
}

int card_bloodgift_demon(int player, int card, event_t event ){
  /* Bloodgift Demon	|3|B|B
   * Creature - Demon 5/4
   * Flying
   * At the beginning of your upkeep, target player draws a card and loses 1 life. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		if( can_target(&td) ){
			if( player == AI ){
				if( life[1-player] < 2 || count_deck(1-player) < 2 || life[player] < 6 ){
					instance->targets[0].player = 1-player;
					instance->targets[0].card = -1;
					instance->number_of_targets = 1;
					if( ! valid_target(&td) ){
						instance->targets[0].player = player;
						instance->targets[0].card = -1;
						instance->number_of_targets = 1;
					}
				}
				else{
					instance->targets[0].player = player;
					instance->targets[0].card = -1;
					instance->number_of_targets = 1;
					if( ! valid_target(&td) ){
						instance->targets[0].player = 1-player;
						instance->targets[0].card = -1;
						instance->number_of_targets = 1;
					}
				}
			}
			else{
				pick_target(&td, "TARGET_PLAYER");
				instance->number_of_targets = 1;
			}
			lose_life(instance->targets[0].player, 1);
			draw_cards(instance->targets[0].player, 1);
		}
	}

	return 0;
}

int card_bloodline_keeper(int player, int card, event_t event){
  /* Bloodline Keeper	|2|B|B
   * Creature - Vampire 3/3
   * Flying
   * |T: Create a 2/2 black Vampire creature token with flying.
   * |B: Transform this creature. Activate only if you control five or more Vampires. */

	double_faced_card(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		if( count_subtype(player, TYPE_PERMANENT, SUBTYPE_VAMPIRE) > 4 &&
			generic_activated_ability(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL) )
		{
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		card_instance_t *instance = get_card_instance(player, card);
		int choice = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			if( count_subtype(player, TYPE_PERMANENT, SUBTYPE_VAMPIRE) > 4 &&
				generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_B(1), 0, NULL, NULL)
			  ){
				choice = do_dialog(player, player, card, -1, -1, " Generate vampire\n Transform\n Do nothing", 1);
			}
		}
		else{
			 choice = 1;
		}
		if( choice == 0 ){
			generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				instance->info_slot = 66+choice;
			}
		}
		else if( choice == 1 ){
				generic_activated_ability(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL);
				if( spell_fizzled != 1 ){
					instance->info_slot = 66+choice;
				}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->info_slot == 66){
			token_generation_t token;
			default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_VAMPIRE, &token);
			token.pow = 2;
			token.tou = 2;
			token.key_plus = KEYWORD_FLYING;
			generate_token(&token);
		}
		if( instance->info_slot == 67){
			transform(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_cackling_counterpart(int player, int card, event_t event){
	/* Cackling Counterpart	|1|U|U
	 * Instant
	 * Put a token onto the battlefield that's a copy of target creature you control.
	 * Flashback |5|U|U */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XU(5, 2));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			copy_token(player, card, player, instance->targets[0].card);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature you control.", 1, NULL);
}

int card_champion_of_the_parish(int player, int card, event_t event)
{
  /* CARD_ID_CHAMPION_OF_THE_PARISH	11900
   * Champion of the Parish	|W
   * Creature - Human Soldier 1/1
   * Whenever another Human enters the battlefield under your control, put a +1/+1 counter on ~. */

  test_definition_t this_test;
  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    default_test_definition(&this_test, 2);
    this_test.subtype = 0x5b;
    this_test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &this_test, player, card);
  }
  if (event != EVENT_ETB)
  {
    return 0;
  }
  add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  return 0;
}

int card_clifftop_retreat(int player, int card, event_t event){
  /* Clifftop Retreat	""
   * Land
   * This land enters tapped unless you control a Mountain or a Plains.
   * |T: Add |R or |W. */

	return m10_lands(player, card, event, SUBTYPE_PLAINS, SUBTYPE_MOUNTAIN);
}

int card_diregraf_ghoul(int player, int card, event_t event){
  /* Diregraf Ghoul	|B
   * Creature - Zombie 2/2
   * This creature enters tapped. */

	comes_into_play_tapped(player, card, event);

	return 0;
}

int card_divine_reckoning(int player, int card, event_t event){
  /* Divine Reckoning |2|W|W
   * Sorcery
   * Each player chooses a creature he or she controls. Destroy the rest.
   * Flashback {5}{W}{W} (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		int selected[2] = {-1};

		APNAP(p,{
					target_definition_t td;
					default_target_definition(player, card, &td, TYPE_CREATURE);
					td.allowed_controller = p;
					td.preferred_controller = p;
					td.who_chooses = p;
					td.allow_cancel = 0;
					td.illegal_abilities = 0;

					if( can_target(&td) && new_pick_target(&td, "Select a creature you control to keep.", 0, GS_LITERAL_PROMPT) ){
						state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
						selected[p] = instance->targets[0].card;
						instance->number_of_targets = 0;
					}
				};
		);

		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		new_manipulate_all(player, card, ANYBODY, &test, KILL_DESTROY);

		APNAP(p,{
					if( selected[p] != -1 ){
						state_untargettable(p, selected[p], 0);
					}
				};
		);

		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XW(5, 2));
}

int card_devils_play(int player, int card, event_t event){
	/* Devil's Play	|X|R	0x2003dd8
	 * Sorcery
	 * ~ deals X damage to any target.
	 * Flashback |X|R|R|R */
	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_R(3));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			set_special_flags2(player, card, SF2_X_SPELL);
			charge_mana(player, COLOR_COLORLESS, -1);
			if( spell_fizzled != 1 ){
				set_x_for_x_spells(player, card, event, x_value);
			}
			else{
				return 0;
			}
		}
		pick_target(&td, "TARGET_ANY");
	}

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) && instance->info_slot > 0 ){
			damage_target0(player, card, instance->info_slot);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_endless_ranks_of_the_dead(int player, int card, event_t event){
  /* Endless Ranks of the Dead	|2|B|B
   * Enchantment
   * At the beginning of your upkeep, create X 2/2 black Zombie creature tokens, where X is half the number of Zombies you control, rounded down. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int zombies = count_subtype(player, TYPE_CREATURE, SUBTYPE_ZOMBIE) / 2;
		if( zombies > 0 ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
			token.pow = 2;
			token.tou = 2;
			token.qty = zombies;
			generate_token(&token);
		}
	}

	return global_enchantment(player, card, event);
}

static int effect_essence_of_the_wild(int player, int card, event_t event)
{
  if (event == EVENT_CHANGE_TYPE && affected_card_controller == player
	  && !(land_can_be_played & LCBP_DURING_EVENT_CHANGE_TYPE_SECOND_PASS)
	  && check_special_flags2(affected_card_controller, affected_card, SF2_INFINITE_REFLECTION))
	{
	  card_instance_t* instance = get_card_instance(affected_card_controller, affected_card);
	  if (instance->targets[12].card != -1)
		event_result = instance->targets[12].card;
	}

  return 0;
}

int card_essence_of_the_wild(int player, int card, event_t event)
{
  /* Essence of the Wild	|3|G|G|G
   * Creature - Avatar 6/6
   * Creatures you control enter the battlefield as a copy of ~. */

  card_instance_t* instance;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  // Prevent a redundant effect card from being added each time a creature is cast (even though it'll be invisible)
	  int c;
	  for (c = 0; c < active_cards_count[player]; ++c)
		if ((instance = in_play(player, c)) && instance->internal_card_id == LEGACY_EFFECT_CUSTOM && instance->info_slot == (intptr_t)effect_essence_of_the_wild)
		  goto found;

	  //otherwise
	  {
		int leg = create_legacy_effect(player, card, effect_essence_of_the_wild);
		if (leg != -1)
		  get_card_instance(player, leg)->token_status |= STATUS_INVISIBLE_FX;
	  }

	found:;
	}

	if( event == EVENT_CAST_SPELL && ! affect_me(player, card) && affected_card_controller == player && ! is_humiliated(player, card, AL_STATIC) &&
		is_what(affected_card_controller, affected_card, TYPE_CREATURE)
	  ){
		int p = affected_card_controller, c = affected_card;
		set_special_flags2(p, c, SF2_INFINITE_REFLECTION);
		cloning(p, c, player, card);
		get_card_instance(p, c)->regen_status |= KEYWORD_RECALC_ALL;
		get_abilities(p, c, EVENT_CHANGE_TYPE, -1);
		get_abilities(p, c, EVENT_SET_COLOR, -1);
		get_abilities(p, c, EVENT_POWER, -1);
		get_abilities(p, c, EVENT_TOUGHNESS, -1);
	}

  return 0;
}

int card_geist_honored_monk(int player, int card, event_t event)
{
  /* Geist-Honored Monk	|3|W|W
   * Creature - Human Monk 100/100
   * Vigilance
   * ~'s power and toughness are each equal to the number of creatures you control.
   * When ~ enters the battlefield, put two 1/1 |Swhite Spirit creature tokens with flying onto the battlefield. */

  vigilance(player, card, event);
  if ((uint32_t)(event - EVENT_POWER) < 2 && card == affected_card &&
      affected_card_controller == player && player != -1)
    set_characteristic_pt(count_subtype(player, TYPE_CREATURE, -1));
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
    create_spirit(player, card, 2);
  return 0;
}

int card_garruk_relentless(int player, int card, event_t event)
{
  /* Garruk Relentless	|3|G
   * Planeswalker - Garruk (3)
   * When ~ has two or fewer loyalty counters on him, transform him.
   * 0: ~ deals 3 damage to target creature. That creature deals damage equal to its power to him.
   * 0: Put a 2/2 |Sgreen Wolf creature token onto the battlefield. */

  enum { CHOICE_FIGHT = 1, CHOICE_WOLF };
  check_legend_rule(player, card, event);
  double_faced_card(player, card, event);
  if (event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION)
  {
    card_instance_t *instance = get_card_instance(player, card);
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    int choice = dialog_impl(player, card, event, "Fight a creature", can_target(&td), 2,
                              "Make a Wolf", 1, 1, 0, 0, 0, 0);
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = 0;
      if (choice == CHOICE_FIGHT)
      {
        pick_target(&td, "TARGET_CREATURE");
        if (ai_is_speculating == 1 || (player == AI && !(trace_mode & 2)))
        {
          int loyalty = count_counters(player, card, COUNTER_LOYALTY);
          int target_power = get_power(instance->targets[0].player, instance->targets[0].card);
          ai_modifier += target_power < loyalty ? 64 : -12;
        }
      }
    }
    else if (choice == CHOICE_FIGHT && valid_target(&td))
    {
      int damage = get_power(instance->targets[0].player, instance->targets[0].card);
      damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, card);
      int source_card = instance->parent_card;
      int source_player = instance->parent_controller;
      instance->targets[1].card = source_card;
      instance->targets[1].player = source_player;
      damage_creature(source_player, source_card, damage,
                       instance->targets[0].player, instance->targets[0].card);
    }
    else if (choice == CHOICE_WOLF)
      generate_token_by_id(player, card, CARD_ID_WOLF);
  }
  if (event == EVENT_STATIC_EFFECTS && count_counters(player, card, COUNTER_LOYALTY) < 3 &&
      count_counters(player, card, COUNTER_LOYALTY) > 0)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->targets[7].card != 0x42)
    {
      instance->targets[7].card = 0x42;
      transform(player, card);
    }
  }
  return planeswalker(player, card, event, 3);
}

int card_garruk_the_veil_cursed(int player, int card, event_t event){

	/* Garruk, the Veil-Cursed	""
	 * Planeswalker - Garruk (3)
	 * +1: Put a 1/1 |Sblack Wolf creature token with deathtouch onto the battlefield.
	 * -1: Sacrifice a creature. If you do, search your library for a creature card, reveal it, put it into your hand, then shuffle your library.
	 * -3: Creatures you control gain trample and get +X/+X until end of turn, where X is the number of creature cards in your graveyard. */

	check_legend_rule(player, card, event);

	if (event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION){	// planeswalker() for EVENT_CAN_ACTIVATE; always at least one choice legal

		int can_tutor = can_sacrifice(player, player, 1, TYPE_CREATURE, 0);
		int ai_priority_tutor = count_counters(player, card, COUNTER_LOYALTY) >= 2 ? 3 - hand_count[player] : -1;
		int ai_priority_overrun = count_counters(player, card, COUNTER_LOYALTY) - 3 + count_graveyard_by_type(player, TYPE_CREATURE);

		enum {
			CHOICE_WOLF = 1,
			CHOICE_TUTOR = 2,
			CHOICE_OVERRUN = 3
		} choice = DIALOG(player, card, event,
						  DLG_RANDOM, DLG_PLANESWALKER,
						  "Make a Wolf", 1, 2, 1,
						  "Sacrifice and tutor", can_tutor, ai_priority_tutor, -1,
						  "Necro-overrun", 1, ai_priority_overrun, -3);

		if (event == EVENT_ACTIVATE){
			switch (choice){
				case CHOICE_WOLF:
					break;

				case CHOICE_TUTOR:
					break;

				case CHOICE_OVERRUN:
					break;
			}
		} else {	// event == EVENT_RESOLVE_ACTIVATION
			switch (choice){
				case CHOICE_WOLF:{
					token_generation_t token;
					default_token_definition(player, card, CARD_ID_WOLF, &token);
					token.s_key_plus = SP_KEYWORD_DEATHTOUCH;
					token.color_forced = COLOR_TEST_BLACK;
					token.pow = 1;
					token.tou = 1;
					generate_token(&token);
					break;
				}

				case CHOICE_TUTOR:
					if (sacrifice(player, card, player, 0, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0)){
						test_definition_t this_test;
						new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
						new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
					} else {
						ai_modifier += (player == AI ? -128 : 128);
					}
					break;

				case CHOICE_OVERRUN:{
					int plus = count_graveyard_by_type(player, TYPE_CREATURE);
					card_instance_t* instance = get_card_instance(player, card);
					pump_subtype_until_eot(player, instance->parent_card, player, -1, plus, plus, KEYWORD_TRAMPLE, 0);
					break;
				}
			}
		}
	}

	return planeswalker(player, card, event, 3);
}

int card_ghost_quarter(int player, int card, event_t event){
  /* Ghost Quarter	""
   * Land
   * |T: Add |C.
   * |T, Sacrifice this land: Destroy target land. Its controller may search their library for a basic land card, put it onto the battlefield, then shuffle. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	if( player == AI ){
		td.required_subtype = SUBTYPE_BASIC;
		td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		instance->number_of_targets = 0;
		instance->info_slot = 0;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0,
									&td, NULL) )
		{
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Kill a land\n Cancel", 1);
		}
		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		else if( choice == 1 ){
				if( charge_mana_for_activated_ability(player, card, MANACOST0) &&
					new_pick_target(&td, "TARGET_LAND", 0, 1)
				  ){
					instance->info_slot = 1;
					tap_card(player, card);
					kill_card(player, card, KILL_SACRIFICE);
				}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			if( valid_target(&td) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				int will_tutor = 1;
				if( instance->targets[0].player == HUMAN )
					will_tutor = 1-do_dialog(instance->targets[0].player, player, card, -1, -1, " Tutor a basic land\n Pass", 0);
				if( will_tutor ){
					tutor_basic_land(instance->targets[0].player, 1, 0);
				}
			}
		}
	}

	return 0;
}

int card_grimgrin_corpse_born(int player, int card, event_t event){
  /* Grimgrin, Corpse-Born	|3|U|B
   * Legendary Creature - Zombie Warrior 5/5
   * Grimgrin enters tapped and doesn't untap during your untap step.
   * Sacrifice another creature: Untap Grimgrin and put a +1/+1 counter on it.
   * Whenever Grimgrin attacks, destroy target creature defending player controls, then put a +1/+1 counter on Grimgrin. */

	check_legend_rule(player, card, event);

	// ~ enters the battlefield tapped and doesn't untap during your untap step.
	comes_into_play_tapped(player, card, event);
	does_not_untap(player, card, event);

	// Whenever Grimgrin attacks, destroy target creature defending player controls, then put a +1/+1 counter on ~.
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;
		td.allowed_controller = 1-player;

		card_instance_t* instance = get_card_instance(player, card);
		instance->number_of_targets = 0;

		if (can_target(&td) && pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS")){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			add_counters(player, card, player, card, COUNTER_P1_P1, 1);
		}
	}

	// Sacrifice another creature: Untap ~ and put a +1/+1 counter on it.
	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t* instance = get_card_instance(player, card);
		untap_card(instance->parent_controller, instance->parent_card);
		add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE|GAA_NOT_ME_AS_TARGET, MANACOST_X(0), 0, NULL, NULL);
}

int card_grimoire_of_the_dead(int player, int card, event_t event){

	/* Grimoire of the Dead	|4
	 * Legendary Artifact
	 * |1, |T, Discard a card: Put a study counter on ~.
	 * |T, Remove three study counters from ~ and sacrifice it: Put all creature cards from all graveyards onto the battlefield under
	 *		your control. They're |Sblack Zombies in addition to their other colors and types. */

	check_legend_rule(player, card, event);

	if (event == EVENT_SHOULD_AI_PLAY){
		int counters = count_counters(player, card, COUNTER_STUDY);
		counters = MIN(counters, 3);
		ai_modifier += (player == AI ? 32 : -32 ) * counters * counters;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_COUNTERS = 1,
		CHOICE_REANIMATE
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, GVC_COUNTERS(COUNTER_STUDY, 3),
										NULL, NULL) )
		{
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD, MANACOST_X(1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
            0,
            generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_DISCARD, MANACOST0, 0, NULL, NULL),
            generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, GVC_COUNTERS(COUNTER_STUDY, 3), NULL, NULL)
        };

		int priority_reanimate = (count_graveyard_by_type(player, TYPE_CREATURE)+count_graveyard_by_type(1-player, TYPE_CREATURE))*5;

		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Add a counter", 			abils[1], 10,
							"Reanimate all creatures", 	abils[2], priority_reanimate);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if (choice == CHOICE_COUNTERS){
            generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD, MANACOST_X(1), 0, NULL, NULL);
            if (player == AI && !(current_turn == 1-player && current_phase == PHASE_DISCARD)){
                ai_modifier -= 160;
            }
        }else if (choice == CHOICE_REANIMATE){
            generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, GVC_COUNTERS(COUNTER_STUDY, 3), NULL, NULL);
        }
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_COUNTERS ){
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_STUDY, 1);
		}

		if( instance->info_slot == CHOICE_REANIMATE ){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			new_reanimate_all(player, card, ANYBODY, &test, REANIMATE_ADD_BLACK_ZOMBIE | REANIMATE_ALL_UNDER_CASTERS_CONTROL);
		}
	}

	return 0;
}

int card_heartless_summoning(int player, int card, event_t event ){
  /* Heartless Summoning	|1|B
   * Enchantment
   * Creature spells you cast cost |2 less to cast.
   * Creatures you control get -1/-1. */

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card, AL_STATIC) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE)  ){
			COST_COLORLESS-=2;
		}
	}

	boost_creature_type(player, card, event, -1, -1, -1, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);

	return global_enchantment(player, card, event);
}

int card_hinterland_harbor(int player, int card, event_t event){
  /* Hinterland Harbor	""
   * Land
   * This land enters tapped unless you control a Forest or an Island.
   * |T: Add |G or |U. */

	return m10_lands(player, card, event, SUBTYPE_ISLAND, SUBTYPE_FOREST);
}

int card_howlpack_alpha(int player, int card, event_t event){
	/* Howlpack Alpha	""
	 * Creature - Werewolf 3/3
	 * Each other creature you control that's a Werewolf or a Wolf gets +1/+1.
	 * At the beginning of your end step, put a 2/2 |Sgreen Wolf creature token onto the battlefield.
	 * At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */

	werewolf_moon_phases(player, card, event);

	if( affected_card_controller == player && (event == EVENT_POWER || event == EVENT_TOUGHNESS) && ! affect_me(player, card) &&
		! is_humiliated(player, card, AL_STATIC)
	  ){
		if( has_subtype(affected_card_controller, affected_card, SUBTYPE_WEREWOLF) ||
			has_subtype(affected_card_controller, affected_card, SUBTYPE_WOLF) )
		{
			event_result++;
		}
	}

	if( current_turn == player && eot_trigger(player, card, event) ){
		generate_token_by_id(player, card, CARD_ID_WOLF);
	}

	return 0;
}

int card_instigator_gang(int player, int card, event_t event){
  /* Instigator Gang	|3|R
   * Creature - Human Werewolf 2/3
   * Attacking creatures you control get +1/+0.
   * At the beginning of each upkeep, if no spells were cast last turn, transform this creature. */

	double_faced_card(player, card, event);

	human_moon_phases(player, card, event);

	if( event == EVENT_POWER && affected_card_controller == player && ! is_humiliated(player, card, AL_STATIC) ){
		if( is_attacking(affected_card_controller, affected_card) ){
			event_result++;
		}
	}

	return 0;
}

int card_isolated_chapel(int player, int card, event_t event){
  /* Isolated Chapel	""
   * Land
   * This land enters tapped unless you control a Plains or a Swamp.
   * |T: Add |W or |B. */

	return m10_lands(player, card, event, SUBTYPE_SWAMP, SUBTYPE_PLAINS);
}

int card_lord_of_lineage(int player, int card, event_t event){
  /* Lord of Lineage	""
   * Creature - Vampire 5/5
   * Flying
   * Other Vampire creatures you control get +2/+2.
   * |T: Create a 2/2 black Vampire creature token with flying. */

	double_faced_card(player, card, event);

	boost_creature_type(player, card, event, SUBTYPE_VAMPIRE, 2, 2, 0, BCT_CONTROLLER_ONLY);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_VAMPIRE, &token);
		token.pow = 2;
		token.tou = 2;
		token.key_plus = KEYWORD_FLYING;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_mayor_of_avabruck(int player, int card, event_t event){
  /* Mayor of Avabruck	|1|G
   * Creature - Human Advisor Werewolf 1/1
   * Other Human creatures you control get +1/+1.
   * At the beginning of each upkeep, if no spells were cast last turn, transform this creature. */

	double_faced_card(player, card, event);

	human_moon_phases(player, card, event);

	boost_creature_type(player, card, event, SUBTYPE_HUMAN, 1, 1, 0, BCT_CONTROLLER_ONLY);

	return 0;
}

int card_mentor_of_the_meek(int player, int card, event_t event){

  /* Mentor of the Meek	|2|W
   * Creature - Human Soldier 2/2
   * Whenever another creature with power 2 or less enters the battlefield under your control, you may pay |1. If you do, draw a card. */

	if ( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) )
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.power = 3;
		test.power_flag = F5_POWER_LESSER_THAN_VALUE;
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		if( has_mana(player, COLOR_ANY, 1) && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1) ){
			draw_a_card(player);
		}
	}

	return 0;
}

int card_mikaeus_the_lunarch(int player, int card, event_t event){

	/* Mikaeus, the Lunarch	|X|W
	 * Legendary Creature - Human Cleric 0/0
	 * ~ enters the battlefield with X +1/+1 counters on it.
	 * |T: Put a +1/+1 counter on Mikaeus.
	 * |T, Remove a +1/+1 counter from Mikaeus: Put a +1/+1 counter on each other creature you control. */

	check_legend_rule(player, card, event);

	if (event == EVENT_CAST_SPELL && affect_me(player, card) && !is_token(player, card)){
		generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, get_card_instance(player, card)->info_slot);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_PUMP_MIKAEUS = 1,
		CHOICE_PUMP_OTHERS
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, GVC_COUNTERS(COUNTER_P1_P1, 1), NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, GVC_COUNTERS(COUNTER_P1_P1, 1),
													NULL, NULL)
		};
		int priorities[3] = {
							0,
							30-(count_counters(player, card, COUNTER_P1_P1)*10),
							count_counters(player, card, COUNTER_P1_P1)*10
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Pump Mikaeus",			abils[1], priorities[1],
							"Pump everybody else", 	abils[2], priorities[2]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_PUMP_MIKAEUS ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
		if( choice == CHOICE_PUMP_OTHERS ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, GVC_COUNTERS(COUNTER_P1_P1, 1), NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PUMP_MIKAEUS ){
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 1);
		}
		if( instance->info_slot == CHOICE_PUMP_OTHERS ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "");
			this_test.not_me = 1;
			new_manipulate_all(instance->parent_controller, instance->parent_card, player, &this_test, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));
		}
	}

	return 0;
}

int card_mindshrieker(int player, int card, event_t event){
  /* Mindshrieker	|1|U
   * Creature - Spirit Bird 1/1
   * Flying
   * |2: Target player mills a card. This creature gets +X/+X until end of turn, where X is the milled card's mana value. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int *deck = deck_ptr[instance->targets[0].player];
			int pump = get_cmc_by_id( cards_data[deck[0]].id );
			mill(instance->targets[0].player, 1);
			pump_until_eot(player, card, player, instance->parent_card, pump, pump);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_PLAYER");
}

int card_moans_of_the_unhallowed(int player, int card, event_t event){
	/* Moan of the Unhallowed	|2|B|B
	 * Sorcery
	 * Put two 2/2 |Sblack Zombie creature tokens onto the battlefield.
	 * Flashback |5|B|B */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}
	if(event == EVENT_RESOLVE_SPELL){
		generate_tokens_by_id(player, card, CARD_ID_ZOMBIE, 2);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return do_flashback(player, card, event, MANACOST_XB(5, 2));
}

int card_moonmist(int player, int card, event_t event)
{
  /* Moonmist	|1|G
   * Instant
   * Transform all Humans. Prevent all combat damage that would be dealt this turn by creatures other than Werewolves and Wolves. (Only double-faced cards can be transformed.) */

  card_instance_t *instance;
  int human_or_type_match;
  int i;
  int p;
  int *marked_cursor;
  int marked[151];
  p = current_turn;
  if (event == EVENT_CAN_CAST)
  {
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }
  memset(marked, 0, sizeof(marked));
  i = active_cards_count[current_turn] + -1;
  if (-1 < i)
  {
    do
    {
      if ((((p == player) && (instance = in_play(p, i), instance != NULL)) &&
           (human_or_type_match = is_what(p, i, 2), human_or_type_match != 0)) &&
          ((human_or_type_match = has_subtype(p, i, SUBTYPE_HUMAN),
            human_or_type_match != 0 &&
                (instance = get_card_instance(p, i), instance->targets[0xd].player != -1))))
      {
        marked[i] = 1;
      }
      i = i + -1;
    } while (i != -1);
  }
  p = 1 - current_turn;
  i = active_cards_count[p] + -1;
  if (-1 < i)
  {
    do
    {
      if (((p == player) && (instance = in_play(p, i), instance != NULL)) &&
          ((human_or_type_match = is_what(p, i, 2),
            human_or_type_match != 0 &&
                ((human_or_type_match = has_subtype(p, i, SUBTYPE_HUMAN),
                  human_or_type_match != 0 &&
                      (instance = get_card_instance(p, i), instance->targets[0xd].player != -1))))))
      {
        marked[i] = 1;
      }
      i = i + -1;
    } while (i != -1);
  }
  marked_cursor = marked + 0x96;
  while (true)
  {
    if (*marked_cursor != 0)
    {
      transform(player, *marked_cursor);
    }
    if (marked == marked_cursor)
      break;
    marked_cursor = marked_cursor + -1;
  }
  fog_effect(player, card);
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

int card_olivia_voldaren(int player, int card, event_t event){

	/* Olivia Voldaren	|2|B|R
	 * Legendary Creature - Vampire 3/3
	 * Flying
	 * |1|R: ~ deals 1 damage to another target creature. That creature becomes a Vampire in addition to its other types.
	 *		Put a +1/+1 counter on ~.
	 * |3|B|B: Gain control of target Vampire for as long as you control ~. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.special = TARGET_SPECIAL_NOT_ME;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);
	td1.required_subtype = SUBTYPE_VAMPIRE;
	if( player == AI ){
		td1.allowed_controller = 1-player;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XB(3, 2), 0, &td1, NULL) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE  ){
		instance->info_slot = instance->number_of_targets = 0;
		int choice = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, NULL)  ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_XB(3, 2), 0, &td1, NULL) ){
				choice = do_dialog(player, player, card, -1, -1, " Ping creature\n Steal vampire\n Cancel", 1);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 0 ){
			generic_activated_ability(player, card, event, GAA_LITERAL_PROMPT | GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, "Select another target creature");
			if( spell_fizzled != 1 ){
				instance->info_slot = 66+choice;
			}
		}
		else if( choice == 1 ){
				generic_activated_ability(player, card, event, GAA_LITERAL_PROMPT | GAA_CAN_TARGET, MANACOST_XB(3, 2), 0, &td1, "Select target Vampire permanent.");
				if( spell_fizzled != 1 ){
					instance->info_slot = 66+choice;
				}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 && valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 1, instance->parent_controller, instance->parent_card);
			add_a_subtype_permanently(player, card, instance->targets[0].player, instance->targets[0].card, SUBTYPE_VAMPIRE);
			create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &empty, instance->targets[0].player, instance->targets[0].card);
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 1);
		}
		if( instance->info_slot == 67 && valid_target(&td1) ){
			if( instance->parent_controller != instance->targets[0].player ){
				gain_control_until_source_is_in_play_and_tapped(instance->parent_controller, instance->parent_card,
																instance->targets[0].player, instance->targets[0].card, GCUS_SOURCE_IN_PLAY | GCUS_CONTROLLED);
			}
		}
	}

	return 0;
}

int card_past_in_flames(int player, int card, event_t event ){
  /* Past in Flames	|3|R
   * Sorcery
   * Each instant and sorcery card in your graveyard gains flashback until end of turn. The flashback cost is equal to its mana cost.
   * Flashback |4|R (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int count = 0;
		while( get_grave(player)[count] != -1 ){
				if( is_what(-1, get_grave(player)[count], TYPE_SPELL) && ! is_suspend_only_card(-1, get_grave(player)[count]) ){
					create_spell_has_flashback_legacy(player, card, count, 0);
				}
				count++;
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XR(4, 1));
}

int card_rakish_heir(int player, int card, event_t event)
{
  /* Rakish Heir	|2|R
   * Creature - Vampire 2/2
   * Whenever a Vampire you control deals combat damage to a player, put a +1/+1 counter on it. */

  uint8_t *damage_sources;
  int source_offset;
  card_instance_t *instance;
  card_instance_t *combat_damage;
  int i;

  instance = get_card_instance(player, card);
  damage_sources = (uint8_t *)instance + offsetof(card_instance_t, targets);
  combat_damage = combat_damage_being_dealt(event);
  if ((((combat_damage != NULL) && (combat_damage->damage_source_player == player)) &&
       (combat_damage->damage_source_card != -1)) &&
      (combat_damage->damage_target_card == -1))
  {
    if (check_special_flags(player, combat_damage->damage_source_card, 0x40) == 0)
    {
      if (has_subtype((int)combat_damage->damage_source_player, combat_damage->damage_source_card,
                      SUBTYPE_VAMPIRE) != 0)
      {
        if (instance->info_slot < 0x28)
        {
          damage_sources[instance->info_slot * 2] = (uint8_t)combat_damage->damage_source_player;
          damage_sources[instance->info_slot * 2 + 1] = (uint8_t)combat_damage->damage_source_card;
          instance->info_slot = instance->info_slot + 1;
        }
      }
    }
  }
  if (((trigger_condition == TRIGGER_DEAL_DAMAGE) && (card == affected_card)) &&
      ((player == affected_card_controller &&
        ((reason_for_trigger_controller == player && (0 < instance->info_slot))))))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      i = 0;
      do
      {
        source_offset = i * 2;
        i = i + 1;
        add_counters(player, card,
                     (unsigned int)damage_sources[source_offset],
                     (unsigned int)damage_sources[source_offset + 1],
                     COUNTER_P1_P1, 1);
      } while (i < instance->info_slot);
      instance->info_slot = 0;
    }
  }
  return 0;
}

int card_reaper_from_the_abyss(int player, int card, event_t event)
{
  /* Reaper from the Abyss	|3|B|B|B
   * Creature - Demon 6/6
   * Flying
   * Morbid - At the beginning of each end step, if a creature died this turn, destroy target non-Demon creature. */

	if (morbid() && eot_trigger(player, card, event))
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_subtype = SUBTYPE_DEMON;
		td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_next_target_noload(&td, "Select target non-Demon creature.")){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

  return 0;
}

int card_rooftop_storm(int player, int card, event_t event){
  /* Rooftop Storm	|5|U
   * Enchantment
   * You may pay |0 rather than pay the mana cost for Zombie creature spells you cast. */

	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card, AL_STATIC) ){
		if( affected_card_controller == player && is_what(affected_card_controller,affected_card, TYPE_CREATURE) &&
			has_subtype(affected_card_controller,affected_card, SUBTYPE_ZOMBIE)
		  ){
			card_ptr_t* c = cards_ptr[ get_id(affected_card_controller,affected_card) ];
			COST_COLORLESS-=c->req.req_colorless;
			COST_BLACK-=c->req.req_black;
			COST_BLUE-=c->req.req_blue;
			COST_GREEN-=c->req.req_green;
			COST_RED-=c->req.req_red;
			COST_WHITE-=c->req.req_white;
		}
	}

	return global_enchantment(player, card, event);
}

int card_skirsdag_high_priest(int player, int card, event_t event){

	/* Skirsdag High Priest	|1|B
	 * Creature - Human Cleric 1/2
	 * Morbid - |T, Tap two untapped creatures you control: Put a 5/5 |Sblack Demon creature token with flying onto the battlefield. Activate this ability only
	 * if a creature died this turn. */

	if (!IS_ACTIVATING(event)){
		return 0;
	}

	target_definition_t td;
	base_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) && morbid() ){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			test.state = STATE_TAPPED;
			test.state_flag = DOESNT_MATCH;
			test.not_me = 1;
			if( check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test) > 1 ){
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			state_untargettable(player, card, 1);
			if( tapsubtype_ability(player, card, 2, &td) ){
				tap_card(player, card);
			}
			else{
				spell_fizzled = 1;
			}
			state_untargettable(player, card, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_DEMON);
	}

	return 0;
}

int card_sulfur_falls(int player, int card, event_t event){
  /* Sulfur Falls	""
   * Land
   * This land enters tapped unless you control an Island or a Mountain.
   * |T: Add |U or |R. */

	return m10_lands(player, card, event, SUBTYPE_ISLAND, SUBTYPE_MOUNTAIN);
}

int card_snapcaster_mage(int player, int card, event_t event ){

	/* Snapcaster Mage	|1|U
	 * Creature - Human Wizard 2/1
	 * Flash
	 * When ~ enters the battlefield, target instant or sorcery card in your graveyard gains flashback until end of turn. The flashback cost is equal to its
	 * mana cost. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_SPELL, "Select target instant or sorcery card.");

		if( new_special_count_grave(player, &this_test) && ! graveyard_has_shroud(2) ){
			int selected = new_select_a_card(player, player, TUTOR_FROM_GRAVE, 0, AI_MAX_VALUE, -1, &this_test);
			create_spell_has_flashback_legacy(player, card, selected, 0);
		}
	}

	return flash(player, card, event);
}

int card_skaab_ruinator(int player, int card, event_t event)
{
  /* Skaab Ruinator	|1|U|U
   * Creature - Zombie Horror 5/6
   * As an additional cost to cast ~, exile three creature cards from your graveyard.
   * Flying
   * You may cast ~ from your graveyard. */

  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    int cless = get_updated_casting_cost(player, card, -1, event, 1);
    if (has_mana_multi(player, MANACOST_XU(cless, 2)) &&
        count_graveyard_by_type(player, TYPE_CREATURE) > 3 && can_sorcery_be_played(player, event))
    {
      return GA_PLAYABLE_FROM_GRAVE;
    }
  }

  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    if (charge_mana_from_id(player, -1, event, get_id(player, card)) &&
        gather_body_parts(player, card, 3, 1))
      return GAPAID_REMOVE;
    spell_fizzled = 1;
    return 0;
  }
  else
    skaabs(player, card, event, 3);
  return 0;
}

int card_stitchers_apprentice(int player, int card, event_t event ){
  /* Stitcher's Apprentice	|1|U
   * Creature - Homunculus 1/2
   * |1|U, |T: Create a 2/2 blue Homunculus creature token, then sacrifice a creature. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_HOMUNCULUS, &token);
		token.pow = 2;
		token.tou = 2;
		generate_token(&token);

		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		new_sacrifice(instance->parent_controller, instance->parent_card, instance->parent_controller, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XU(1, 1), 0, NULL, NULL);
}

int card_undead_alchemist(int player, int card, event_t event)
{
  /* Undead Alchemist	|3|U
   * Creature - Zombie 4/2
   * If a Zombie you control would deal combat damage to a player, instead that player puts that many cards from the top of his or her library into his or her
   * graveyard.
   * Whenever a creature card is put into an opponent's graveyard from his or her library, exile that card and put a 2/2 |Sblack Zombie creature token onto the
   * battlefield. */

  card_instance_t* instance = get_card_instance(player, card);
  card_instance_t* damage = combat_damage_being_dealt(event);

  if (damage
	  && damage->damage_target_card == -1 && !damage_is_to_planeswalker(damage)
	  && (damage->targets[3].player & TYPE_CREATURE)	// probably redundant to combat check
	  && has_subtype(damage->damage_source_player, damage->damage_source_card, SUBTYPE_ZOMBIE))
	{
	  instance->info_slot += damage->info_slot;
	  damage->info_slot = 0;
	}

  if (trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player
	  && instance->info_slot > 0)
	{
	  if (event == EVENT_TRIGGER)
		event_result |= 2;

	  if (event == EVENT_RESOLVE_TRIGGER)
		{
		  mill(1-player, instance->info_slot);
		  instance->info_slot = 0;
		}
	}

  enable_xtrigger_flags |= ENABLE_XTRIGGER_MILLED;
  if (xtrigger_condition() == XTRIGGER_MILLED && affect_me(player, card) && reason_for_trigger_controller == player
	  && trigger_cause_controller == 1-player && !is_humiliated(player, card, AL_TRIG))
	{
	  if (event != EVENT_TRIGGER && event != EVENT_RESOLVE_TRIGGER)
		return 0;

	  int i;
	  for (i = 0; i < num_cards_milled; ++i)
		if (is_what(-1, cards_milled[i].internal_card_id, TYPE_CREATURE))
		  {
			if (event == EVENT_TRIGGER)
			  {
				event_result |= RESOLVE_TRIGGER_MANDATORY;
				return 0;
			  }

			if (event == EVENT_RESOLVE_TRIGGER)
			  {
				if (cards_milled[i].position != -1)
				  {
					int pos = find_in_graveyard_by_source(1-player, cards_milled[i].source, cards_milled[i].position);
					if (pos != -1)
					  rfg_card_from_grave(1-player, pos);
					cards_milled[i].position = -1;	// No longer in graveyard, so keep any other triggers from looking
				  }

				generate_token_by_id(player, card, CARD_ID_ZOMBIE);	// Whether or not we successfully exiled it
			  }
		  }
	}

  return 0;
}

int card_woodland_cemetery(int player, int card, event_t event){
/* CARD_ID_WOODLAND_CEMETERY	12127
Woodland Cemetery	""
Land
~ enters the battlefield tapped unless you control |Ha Swamp or |Ha Forest.
|T: Add |B or |G to your mana pool. */

	return m10_lands(player, card, event, SUBTYPE_SWAMP, SUBTYPE_FOREST);
}

int card_wildblood_pack(int player, int card, event_t event){
/* CARD_ID_WILDBLOOD_PACK	11991
Wildblood Pack	""
Creature - Werewolf 5/5
Trample
Attacking creatures you control get +3/+0.
At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */

	double_faced_card(player, card, event);

	werewolf_moon_phases(player, card, event);

	if( event == EVENT_POWER && affected_card_controller == player && ! is_humiliated(player, card, AL_STATIC) ){
		if( is_attacking(affected_card_controller, affected_card) ){
			event_result+=3;
		}
	}

	return 0;
}

int card_intangible_virtue(int player, int card, event_t event){
/* CARD_ID_INTANGIBLE_VIRTUE	11992
Intangible Virtue	|1|W
Enchantment
Creature tokens you control get +1/+1 and have vigilance. */

	if( ! is_humiliated(player, card, AL_STATIC) && in_play(player, card) ){
		if( event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES ){
			if( affected_card_controller == player && is_token(affected_card_controller, affected_card) ){
				switch( event ){
						case EVENT_POWER:
						case EVENT_TOUGHNESS:
							event_result++;
							break;
						case EVENT_ABILITIES:
							vigilance(affected_card_controller, affected_card, event);
							break;
						default:
							break;
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_forbidden_alchemy(int player, int card, event_t event){
/* CARD_ID_FORBIDDEN_ALCHEMY	11949
Forbidden Alchemy	|2|U
Instant
Look at the top four cards of your library. Put one of them into your hand and the rest into your graveyard.
Flashback |6|B (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		select_one_and_mill_the_rest(player, player, 4, TYPE_ANY);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XB(6, 1));
}

int card_unburial_rites(int player, int card, event_t event){
/* CARD_ID_UNBURIAL_RITES	12110
Unburial Rites	|4|B
Sorcery
Return target creature card from your graveyard to the battlefield.
Flashback |3|W (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
		this_test.ai_selection_mode = AI_MAX_CMC;
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 0, &this_test);
	}

	if(event == EVENT_RESOLVE_SPELL){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XW(3, 1));
}

int card_liliana_of_the_veil(int player, int card, event_t event){
  /* Liliana of the Veil	|1|B|B
   * Planeswalker - Liliana (3)
   * +1: Each player discards a card.
   * -2: Target player sacrifices a creature.
   * -6: Separate all permanents target player controls into two piles. That player sacrifices all permanents in the pile of his or her choice. */

	check_legend_rule(player, card, event);




	if (IS_ACTIVATING(event)){

		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;

		enum{
			CHOICE_ALL_DISCARD = 1,
			CHOICE_DIABOLIC_EDICT,
			CHOICE_ULTIMATUM
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Each player discards", 1, 5, 1,
						"Sacrifice creature", can_target(&td), count_subtype(1-player, TYPE_CREATURE, -1) ? 10 : 0, -2,
						"Sacrifice pile", can_target(&td), 3*count_subtype(1-player, TYPE_PERMANENT, -1), -6);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_ACTIVATE)
		{
			instance->number_of_targets = 0;
			if( choice == CHOICE_DIABOLIC_EDICT || choice == CHOICE_ULTIMATUM ){
				pick_target(&td, "TARGET_PLAYER");
			}
		}
		else{	// event == EVENT_RESOLVE_ACTIVATION
			switch (choice){
				case( CHOICE_ALL_DISCARD ):
						APNAP(p, {discard(p, 0, player);};);
						break;

				case( CHOICE_DIABOLIC_EDICT ):
						impose_sacrifice(player, instance->parent_card, instance->targets[0].player, 1, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0);
						break;
				case( CHOICE_ULTIMATUM ):
				{
						int t_player = instance->targets[0].player;
						if( player != AI ){
							target_definition_t td1;
							default_target_definition(player, card, &td1, TYPE_PERMANENT);
							td1.allowed_controller = t_player;
							td1.preferred_controller = t_player;
							td1.illegal_abilities = 0;
							while( can_target(&td1) ){
									if( pick_target(&td1, "TARGET_PERMANENT") ){
										state_untargettable(t_player, instance->targets[0].card, 1);
									}
									else{
										 break;
									}
							}
						}
						else{
							 int counts[5][2];
							 counts[0][0] = TYPE_CREATURE;
							 counts[0][1] = 0;
							 counts[1][0] = TYPE_LAND;
							 counts[1][1] = 0;
							 counts[2][0] = TYPE_ARTIFACT;
							 counts[2][1] = 0;
							 counts[3][0] = TYPE_ENCHANTMENT;
							 counts[3][1] = 0;
							 counts[4][0] = 999;
							 counts[4][1] = 0;
							 int count = 0;
							 while( count < active_cards_count[t_player] ){
									if( in_play(t_player, count) && is_what(t_player, count, TYPE_PERMANENT)){
										int k;
										for(k=0; k<4; k++){
											if( is_planeswalker(t_player, count)){
												counts[4][1]++;
											}
											else{
												if( is_what(t_player, count, counts[k][0]) ){
													counts[k][1]++;
												}
											}
										}
									}
									count++;
							}
							int i;
							int selected_type = -1;
							int par = 0;
							for(i=0; i<5; i++){
								if( counts[i][1] > par ){
									par = counts[i][1];
									selected_type = counts[i][0];
								}
							}
							count = 0;
							while( count < active_cards_count[t_player] ){
									if( in_play(t_player, count) && is_what(t_player, count, TYPE_PERMANENT) ){
										if( selected_type != 999 ){
											if( is_what(t_player, count, selected_type) ){
												state_untargettable(t_player, count, 1);
											}
										}
										else{
											if( is_planeswalker(t_player, count) ){
												state_untargettable(t_player, count, 1);
											}
										}
									}
									count++;
							}
						}
						int ai_choice = 0;
						if( t_player == AI ){
							int counts[2] = {0,0};
							int count = 0;
							while( count < active_cards_count[t_player] ){
									if( in_play(t_player, count) && is_what(t_player, count, TYPE_PERMANENT) ){
										card_instance_t *this= get_card_instance(t_player, count);
										card_ptr_t* c = cards_ptr[ get_id(player, count) ];
										if( this->state & STATE_CANNOT_TARGET ){
											counts[0]+=c->ai_base_value;
										}
										else{
											counts[1]+=c->ai_base_value;
										}
									}
									count++;
							}
							if( counts[0] > counts[1] ){
								ai_choice = 1;
							}
						}
						int choice2 = do_dialog(t_player, player, card, -1, -1, " Sac signed pile\n Sac unsigned pile", ai_choice);
						int count = active_cards_count[t_player]-1;
						while( count > -1 ){
								if( in_play(t_player, count) && is_what(t_player, count, TYPE_PERMANENT) ){
									card_instance_t *this= get_card_instance(t_player, count);
									if( this->state & STATE_CANNOT_TARGET ){
										if( choice2 == 0 ){
											kill_card(t_player, count, KILL_SACRIFICE);
										}
										else{
											state_untargettable(t_player, count, 0);
										}
									}
									else{
										if( choice2 == 1 ){
											kill_card(t_player, count, KILL_SACRIFICE);
										}
									}
								}
								count--;

						}
				}
				break;
			}
		}
	}
	return planeswalker(player, card, event, 3);
}

int card_doomed_traveller(int player, int card, event_t event){
	/* Doomed Traveler	|W
	 * Creature - Human Soldier 1/1
	 * When ~ dies, put a 1/1 |Swhite Spirit creature token with flying onto the battlefield. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		create_spirit(player, card, 1);
	}

	return 0;
}

int card_deranged_assistant(int player, int card, event_t event){
/* CARD_ID_DERANGED_ASSISTANT	11928
Deranged Assistant	|1|U
Creature - Human Wizard 1/1
|T, Put the top card of your library into your graveyard: Add |C to your mana pool. */

	if( event == EVENT_CAN_ACTIVATE || (event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		if( deck_ptr[player][0] != -1 ){
			return mana_producer(player, card, event);
		}
	}

	if(event == EVENT_ACTIVATE ){
		if( count_deck(player) <= 20 ){
			ai_modifier-=50;
		}
		mill(player, 1);
		return mana_producer(player, card, event);
	}

	return 0;
}

int card_blasphemous_act(int player, int card, event_t event){
/* CARD_ID_BLASPHEMOUS_ACT	11884
Blasphemous Act	|8|R
Sorcery
~ costs |1 less to cast for each creature on the battlefield.
~ deals 13 damage to each creature. */

	if( event == EVENT_MODIFY_COST ){
		COST_COLORLESS -= count_subtype(ANYBODY, TYPE_CREATURE, -1);
	}

	if(event == EVENT_RESOLVE_SPELL){
		new_damage_all(player, card, ANYBODY, 13, NDA_ALL_CREATURES, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_cloistered_youth(int player, int card, event_t event){
/* CARD_ID_CLOISTERED_YOUTH	11907
Cloistered Youth	|1|W
Creature - Human 1/1
At the beginning of your upkeep, you may transform ~.
(Unholy Fiend - 3/3) At the beginning of your end step, you lose 1 life. */

	double_faced_card(player, card, event);

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		transform(player, card);
	}

	return 0;
}

int card_unholy_fiend(int player, int card, event_t event){
/* CARD_ID_UNHOLY_FIEND	11908
Unholy Fiend	""
Creature - Horror 3/3
At the beginning of your end step, you lose 1 life. */

	if( current_turn == player && eot_trigger(player, card, event) ){
		lose_life(player, 1);
	}

	return 0;
}

int card_mausoleum_guard(int player, int card, event_t event){
	/* Mausoleum Guard	|3|W
	 * Creature - Human Scout 2/2
	 * When ~ dies, put two 1/1 |Swhite Spirit creature tokens with flying onto the battlefield. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		create_spirit(player, card, 2);
	}

	return 0;
}

int card_midnight_haunting(int player, int card, event_t event){
	/* Midnight Haunting	|2|W
	 * Instant
	 * Put two 1/1 |Swhite Spirit creature tokens with flying onto the battlefield. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		create_spirit(player, card, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_selfless_cathar(int player, int card, event_t event ){
/* CARD_ID_SELFLESS_CATHAR	12064
Selfless Cathar	|W
Creature - Human Cleric 1/1
|1|W, Sacrifice ~: Creatures you control get +1/+1 until end of turn. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_subtype_until_eot(player, card, player, -1, 1, 1, 0, 0);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XW(1, 1), 0, NULL, NULL);
}

int card_silverchase_fox(int player, int card, event_t event ){
/* CARD_ID_SILVERCHASE_FOX	12071
Silverchase Fox	|1|W
Creature - Fox 2/2
|1|W, Sacrifice ~: Exile target enchantment. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_XW(1, 1), 0, &td, "TARGET_ENCHANTMENT");
}

int card_lost_in_the_mist(int player, int card, event_t event){
/* CARD_ID_LOST_IN_THE_MIST	12005
Lost in the Mist	|3|U|U
Instant
Counter target spell. Return target permanent to its owner's hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return counterspell(player, card, event, NULL, 0);
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		counterspell(player, card, event, NULL, 0);
		new_pick_target(&td, "TARGET_PERMANENT", 1, 1);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		if( validate_target(player, card, &td, 1) ){
			bounce_permanent(instance->targets[1].player, instance->targets[1].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_skaab_goliath(int player, int card, event_t event ){
/* CARD_ID_SKAAB_GOLIATH	12072
Skaab Goliath	|5|U
Creature - Zombie Giant 6/9
As an additional cost to cast ~, exile two creature cards from your graveyard.
Trample (This creature can deal excess combat damage to defending player or planeswalker while attacking.) */

	skaabs(player, card, event, 2);

	return 0;
}

int card_stitched_drake(int player, int card, event_t event ){
/* CARD_ID_STITCHED_DRAKE	12088
Stitched Drake	|1|U|U
Creature - Zombie Drake 3/4
As an additional cost to cast ~, exile a creature card from your graveyard.
Flying */

	skaabs(player, card, event, 1);

	return 0;
}

int card_altars_reap(int player, int card, event_t event){
/* CARD_ID_ALTARS_REAP	11871
Altar's Reap	|1|B
Instant
As an additional cost to cast ~, sacrifice a creature.
Draw two cards. */

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 0, NULL);
}

int card_disciple_of_griselbrand(int player, int card, event_t event ){
/* CARD_ID_DISCIPLE_OF_GRISELBRAND	11932
Disciple of Griselbrand	|1|B
Creature - Human Cleric 1/1
|1, Sacrifice a creature: You gain life equal to the sacrificed creature's toughness. */

    test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, get_card_instance( player, card )->info_slot);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL,
												             0, &test, SARV_REPORT_TOUGHNESS);
}

int card_unbreathing_horde(int player, int card, event_t event ){
	/* Unbreathing Horde	|2|B
	 * Creature - Zombie 0/0
	 * ~ enters the battlefield with a +1/+1 counter on it for each other Zombie you control and each Zombie card in your graveyard.
	 * If ~ would be dealt damage, prevent that damage and remove a +1/+1 counter from it. */

	if ((event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE) && affect_me(player, card)){
		int total = count_subtype(player, TYPE_PERMANENT, SUBTYPE_ZOMBIE) + count_graveyard_by_subtype(player, SUBTYPE_ZOMBIE);
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, total);
	}

	phantom_effect(player, card, event, 0);

	return 0;
}

int card_balefire_dragon(int player, int card, event_t event ){
  /* Balefire Dragon |5|R|R
   * Creature — Dragon 6/6, 5RR (7)
   * Flying
   * Whenever Balefire Dragon deals combat damage to a player, it deals that much damage to each creature that player controls. */

	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_REPORT_DAMAGE_DEALT)){
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = {BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player)};
		instance->targets[1].player = 0;
		int damages[2] = {BYTE0(instance->targets[16].player), BYTE1(instance->targets[16].player)};
		instance->targets[16].player = 0;
		APNAP(p, { if (times_damaged[p]){ new_damage_all(player, card, p, damages[p], NDA_ALL_CREATURES, NULL); --times_damaged[p]; } });
	}
	return 0;
}

int card_brimstone_volley(int player, int card, event_t event){
	/* Brimstone Volley	|2|R	0x2003ee1
	 * Instant
	 * ~ deals 3 damage to any target.
	 * Morbid - ~ deals 5 damage to that permanent or player instead if a creature died this turn. */
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		damage_target0(player, card, morbid() ? 5 : 3);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_charmbreaker_devils(int player, int card, event_t event){
  /* Charmbreaker Devils	|5|R
   * Creature - Devil 4/4
   * At the beginning of your upkeep, return an instant or sorcery card at random from your graveyard to your hand.
   * Whenever you cast an instant or sorcery spell, this creature gets +4/+0 until end of turn. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		const int *grave = get_grave(player);
		int array[count_graveyard(player)];
		int array_count = 0;
		int count = 0;
		int count_g = count_graveyard(player);
		while( count < count_g ){
				if( is_what(-1, grave[count], TYPE_SORCERY | TYPE_INSTANT | TYPE_INTERRUPT) &&
					! is_what(-1, grave[count], TYPE_PERMANENT)
				  ){
					array[array_count] = count;
					array_count++;
				}
				count++;
		}
		if( array_count > 0 ){
			int random_card = internal_rand(array_count);
			add_card_to_hand(player, grave[array[random_card]]);
			remove_card_from_grave(player, array[random_card]);
		}
	}

	sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_SPELL, NULL, player, card);
	if (event == EVENT_SPA){ pump_until_eot_merge_previous(player, card, player, card, 4, 0); }
	return 0;
}

int card_falkenrath_marauders(int player, int card, event_t event){
  /* Falkenrath Marauders	|3|R|R
   * Creature - Vampire Warrior 2/2
   * Flying
   * Haste (This creature can attack and |T as soon as it comes under your control.)
   * Whenever this creature deals combat damage to a player, put two +1/+1 counters on it. */

	haste(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = (BYTE0(result) + BYTE1(result))*2;
		add_counters(player, card, player, card, COUNTER_P1_P1, amount);
	}

	return 0;
}

int card_vampiric_fury(int player, int card, event_t event){
  /* Vampiric Fury	|1|R
   * Instant
   * Vampire creatures you control get +2/+0 and gain first strike until end of turn. (They deal combat damage before creatures without first strike.) */

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, SUBTYPE_VAMPIRE, 2, 0, KEYWORD_FIRST_STRIKE, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_bramblecrush(int player, int card, event_t event){
  /* Bramblecrush	|2|G|G
   * Sorcery
   * Destroy target noncreature permanent. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );
	td.illegal_type = TYPE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target noncreature permanent.", 1, NULL);
}

int card_caravan_vigil(int player, int card, event_t event)
{
  /* Caravan Vigil	|G
   * Sorcery
   * Search your library for a basic land card, reveal it, put it into your hand, then shuffle your library.
   * Morbid - You may put that card onto the battlefield instead of putting it into your hand if a creature died this turn. */

  if (event == EVENT_CAN_CAST)
		return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  int destination = TUTOR_HAND;
	  if (morbid()
		  && (duh_mode(player)
			  || do_dialog(player, player, card, -1, -1, " Put the land into play\n Leave in hand", 0) == 0))
		destination = TUTOR_PLAY;

	  tutor_basic_lands(player, destination, 1);

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_creeping_renaissance(int player, int card, event_t event)
{
  /* Creeping Renaissance	|3|G|G
   * Sorcery
   * Choose a permanent type. Return all cards of the chosen type from your graveyard to your hand.
   * Flashback |5|G|G */

  if (event == EVENT_CAN_CAST)
		return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  const int types[] =
		{
		  -1,
		  TYPE_ARTIFACT,
		  TYPE_CREATURE,
		  TYPE_ENCHANTMENT,
		  TYPE_LAND,
		  TARGET_TYPE_PLANESWALKER,
		};
	  int type = types[DIALOG(player, card, event,
							  DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
							  raw_get_subtype_text(SUBTYPE_ARTIFACT), 1, 1,
							  raw_get_subtype_text(SUBTYPE_CREATURE), 1, 1,
							  raw_get_subtype_text(SUBTYPE_ENCHANTMENT), 1, 1,
							  raw_get_subtype_text(SUBTYPE_LAND), 1, 1,
							  raw_get_subtype_text(SUBTYPE_PLANESWALKER), 1, 1)];

	  test_definition_t test;
	  new_default_test_definition(&test, type, "");

	  int num_returned = from_grave_to_hand_multiple(player, &test);
	  if (player == AI)
		{
		  if (num_returned == 0)
			ai_modifier -= 96;
		  else
			ai_modifier += 12 * num_returned - 24;
		}

	  kill_card(player, card, get_flashback(player, card) ? KILL_EXILE : KILL_DESTROY);
	}

  return do_flashback(player, card, event, MANACOST_XG(5, 2));
}

int card_elder_of_laurels(int player, int card, event_t event ){
/* CARD_ID_ELDER_OF_LAURELS	11937
Elder of Laurels	|2|G
Creature - Human Advisor 2/3
|3|G: Target creature gets +X/+X until end of turn, where X is the number of creatures you control. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE );
	td1.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			int amount = count_subtype(player, TYPE_CREATURE, -1);
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, amount, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XG(3, 1), 0, &td1, "TARGET_CREATURE");
}

int card_festerhide_boar(int player, int card, event_t event)
{
  /* Festerhide Boar	|3|G
   * Creature - Boar 3/3
   * Trample
   * Morbid - ~ enters the battlefield with two +1/+1 counters on it if a creature died this turn. */

  if (event == EVENT_CAST_SPELL)
  {
    if ((((card == affected_card) && (affected_card_controller == player)) && (player == 1)) &&
        (creatures_dead_this_turn < 1))
    {
      ai_modifier = ai_modifier + -0xf;
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) && (0 < creatures_dead_this_turn))
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 2);
    return 0;
  }
  return 0;
}

int card_full_moons_rise(int player, int card, event_t event ){
  /* Full Moon's Rise |1|G
   * Enchantment
   * Werewolf creatures you control get +1/+0 and have trample.
   * Sacrifice Full Moon's Rise: Regenerate all Werewolf creatures you control. */

	boost_creature_type(player, card, event, SUBTYPE_WEREWOLF, 1, 0, KEYWORD_TRAMPLE, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_DESTROYED;
	td.allowed_controller = player;
	td.preferred_controller = player;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}
	td.illegal_abilities = 0;

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
		if( result && (land_can_be_played & LCBP_REGENERATION) && can_target(&td) ){
			return 99;
		}
		return result;
	}

	if( event == EVENT_ACTIVATE ){
		if( !((land_can_be_played & LCBP_REGENERATION) && can_target(&td)) ){
			ai_modifier -= 100;
		}
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int c;
		for(c = active_cards_count[player]-1; c > -1; c--){
			if( in_play(player, c) && has_subtype(player, c, SUBTYPE_WEREWOLF) ){
				if( can_be_regenerated(player, c) ){
					regenerate_target( player, c );
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

static int gutter_grime_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_GUTTER_GRIME)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }

  if (get_card_instance(player, card)->damage_target_player > -1 &&
      effect_follows_control_of_attachment(player, card, event))
  {
    return 0;
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (in_play(affected_card_controller, affected_card))
    {
      int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
      if (kill_code)
      {
        if (instance->damage_target_player > -1)
        {
          int p = instance->damage_target_player;
          int c = instance->damage_target_card;
          if (affect_me(p, c))
          {
            instance->damage_target_player = instance->damage_target_card = -1;
            remove_status(player, card, STATUS_INVISIBLE_FX);
            if (instance->targets[11].player < 0)
            {
              instance->targets[11].player = 0;
            }
            SET_BYTE2(instance->targets[11].player) = 1;
          }
        }
        test_definition_t this_test;
        default_test_definition(&this_test, TYPE_CREATURE);
        this_test.type_flag = F1_NO_TOKEN;
        count_for_gfp_ability(player, card, event, player, 0, &this_test);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    /* In this particular case, the Slime tokens will be created normally but they will die
       immediately as Gutter Grime has already left the BF, turning them into 0/0, unless something
       else will raise their toughness. However, they could still trigger something else. */
    int amount = BYTE0(result);

    token_generation_t token;
    default_token_definition(player, card, CARD_ID_OOZE, &token);
    token.pow = token.tou = 0;
    token.qty = amount;
    generate_token(&token);
    kill_card(player, card, KILL_EXILE);
  }

  if (!check_status(player, card, STATUS_INVISIBLE_FX) && event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_gutter_grime(int player, int card, event_t event)
{
  /* Gutter Grime	|4|G
   * Enchantment
   * Whenever a nontoken creature you control dies, put a slime counter on ~, then put a |Sgreen Ooze creature token onto the battlefield
   * with "This creature's power and toughness are each equal to the number of slime counters on ~." */

  int c;
  card_instance_t *instance;
  int legacy_card;
  unsigned int death_count;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      bool legacy_found = false;
      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          instance = in_play(player, c);
          if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
               (instance = get_card_instance(player, c),
                instance->info_slot_fn == gutter_grime_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            legacy_found = true;
            break;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }
      if (!legacy_found)
      {
        legacy_card =
            create_targetted_legacy_effect(player, card, gutter_grime_effect, player, card);
        instance = get_card_instance(player, legacy_card);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, legacy_card, 0x10000);
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 2);
    this_test.type_flag = 3;
    count_for_gfp_ability(player, card, 0x77, player, 0, &this_test);
  }
  instance = get_card_instance(player, card);
  if ((instance->kill_code == '\0') &&
      (death_count = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY),
       death_count != 0))
  {
    token_generation_t token;
    add_counters(player, card, player, card, COUNTER_SLIME, death_count & 0xff);
    default_token_definition(player, card, CARD_ID_OOZE, &token);
    token.qty = (char)death_count;
    token.tou = 16384;
    token.pow = 16384;
    token.special_infos = 66;
    generate_token(&token);
  }
  return global_enchantment(player, card, event);
}

int card_hamlet_captain(int player, int card, event_t event)
{
/* CARD_ID_HAMLET_CAPTAIN	11979
Hamlet Captain	|1|G
Creature - Human Warrior 2/2
Whenever ~ attacks or blocks, other Humans you control get +1/+1 until end of turn. */

	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY || (blocking(player, card, event) && !is_humiliated(player, card, AL_TRIG))){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_HUMAN;
		test.not_me = 1;
		pump_creatures_until_eot(player, card, player, 0, 1, 1, 0, 0, &test);
	}

  return 0;
}

int card_kessig_cagebreakers(int player, int card, event_t event){
/* CARD_ID_KESSIG_CAGEBREAKERS	11996
Kessig Cagebreakers	|4|G
Creature - Human Rogue 3/4
Whenever ~ attacks, create a 2/2 |Sgreen Wolf creature token that's tapped and attacking for each creature card in your graveyard. */

	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY){
		int amount = count_graveyard_by_type(player, TYPE_CREATURE);
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_WOLF, &token);
		token.action = TOKEN_ACTION_ATTACKING;
		token.qty = amount;
		generate_token(&token);
	}

	return 0;
}

int card_make_a_wish(int player, int card, event_t event){
/* CARD_ID_MAKE_A_WISH	12009
Make a Wish	|3|G
Sorcery
Return two cards at random from your graveyard to your hand. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		if( count_graveyard(player) < 2 ){
			ai_modifier-=25;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int count_g = count_graveyard(player);
		int cards = 0;
		const int *grave = get_grave(player);
		while( cards < 2 && count_g > 0){
				int rnd = count_g-1;
				if( rnd > cards ){
					rnd = internal_rand(count_g);
				}
				add_card_to_hand(player, grave[rnd]);
				remove_card_from_grave(player, rnd);
				cards++;
				count_g--;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_moldgraf_monstrosity(int player, int card, event_t event)
{
/* CARD_ID_MOLDGRAF_MONSTROSITY	12026
Moldgraf Monstrosity	|4|G|G|G
Creature - Insect 8/8
Trample
When ~ dies, exile it, then return two creature cards at random from your graveyard to the battlefield. */

  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	{
	  exile_from_owners_graveyard(player, card);
	  tutor_random_permanent_from_grave(player, card, player, TUTOR_PLAY, TYPE_CREATURE, 2, REANIMATE_DEFAULT);
	}

  return 0;
}

int card_parallel_lives(int player, int card, event_t event){
/* CARD_ID_PARALLEL_LIVES	12041
Parallel Lives	|3|G
Enchantment
If an effect would create one or more tokens under your control, it creates twice that many of those tokens instead. */

	return global_enchantment(player, card, event);
}

int card_splinterfright(int player, int card, event_t event){
	/* Splinterfright	|2|G
	 * Creature - Elemental 100/100
	 * Trample
	 * ~'s power and toughness are each equal to the number of creature cards in your graveyard.
	 * At the beginning of your upkeep, put the top two cards of your library into your graveyard. */
	// Used externally
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		mill(player, 2);
	}
	return card_revenant(player, card, event);
}

int innistrad_exile_at_end_of_combat_legacy(int player, int card, event_t event)
{
  if (end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	{
		card_instance_t* instance = get_card_instance(player, card);
		if( ! check_state(instance->damage_target_player, instance->damage_target_card, STATE_OUBLIETTED) ||
			is_token(instance->damage_target_player, instance->damage_target_card) )
		{
			kill_card(instance->damage_target_player, instance->damage_target_card, KILL_EXILE);
		}
		kill_card(player, card, KILL_EXILE);
	}

  return 0;
}

void saint_traft_ability(int player, int card, event_t event, int attacker_player,
                         int attacker_card)
{
  /* Whenever (attacker_player, attacker_card) attacks, put a 4/4 |Swhite Angel creature token with
   * flying onto the battlefield tapped and attacking. Exile that token at end of combat.*/

  store_attackers(player, card, event, 0, attacker_player, attacker_card, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_ANGEL, &token);
    token.pow = token.tou = 4;
    token.key_plus = KEYWORD_FLYING;
    token.color_forced = COLOR_TEST_WHITE;
    token.legacy = 1;
    token.special_code_for_legacy = &innistrad_exile_at_end_of_combat_legacy;
    token.action = TOKEN_ACTION_ATTACKING;
    generate_token(&token);
  }
}

int card_geist_of_saint_traft(int player, int card, event_t event){
/* CARD_ID_GEIST_OF_SAINT_TRAFT	11961
Geist of Saint Traft	|1|W|U
Legendary Creature - Spirit Cleric 2/2
Hexproof (This creature can't be the target of spells or abilities your opponents control.)
Whenever ~ attacks, create a 4/4 |Swhite Angel creature token with flying that's tapped and attacking. Exile that token at end of combat. */

	check_legend_rule(player, card, event);

	hexproof(player, card, event);

	saint_traft_ability(player, card, event, player, card);

	return 0;
}

int card_inquisitors_flail(int player, int card, event_t event)
{
/* CARD_ID_INQUISITORS_FLAIL	11989
Inquisitor's Flail	|2
Artifact - Equipment
If equipped creature would deal combat damage, it deals double that damage instead.
If another creature would deal combat damage to equipped creature, it deals double that damage to equipped creature instead.
Equip |2 */

  card_instance_t* damage = combat_damage_being_dealt(event), *instance;
  if (damage
	  && is_equipping(player, card)
	  && (instance = get_card_instance(player, card))
	  && ((damage->damage_source_card == instance->damage_target_card && damage->damage_source_player == instance->damage_target_player)
		  || (damage->damage_target_card == instance->damage_target_card && damage->damage_target_player == instance->damage_target_player)))
	damage->info_slot *= 2;

  return basic_equipment(player, card, event, 2);
}

int card_gavony_township(int player, int card, event_t event){
	/* Gavony Township	""
	 * Land
	 * |T: Add |1 to your mana pool.
	 * |2|G|W, |T: Put a +1/+1 counter on each creature you control. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		int choice = instance->info_slot = 0;
		if( !paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_XGW(3, 1, 1), 0, NULL, NULL) ){
			choice = do_dialog(player, player, card, -1, -1, " Generate mana\n Add +1/+1 counters\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		if( choice == 1 ){
			add_state(player, card, STATE_TAPPED);
			if( charge_mana_for_activated_ability(player, card, MANACOST_XGW(2, 1, 1)) ){
				tap_card(player, card);
				instance->info_slot = 1;
			}
			else{
				remove_state(player, card, STATE_TAPPED);
			}
			tapped_for_mana_color = -1;
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
			manipulate_type(instance->parent_controller, instance->parent_card, instance->parent_controller, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));
		}
	}

	return 0;
}

int card_kessig_wolf_run(int player, int card, event_t event){
/* CARD_ID_KESSIG_WOLF_RUN	11998
Kessig Wolf Run	""
Land
|T: Add |C to your mana pool.
|X|R|G, |T: Target creature gets +X/+0 and gains trample until end of turn. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE );
	td1.preferred_controller = player;

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int choice = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_GR(1, 1), 0, &td1, NULL) ){
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Pump a creature\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		if( choice == 1 ){
        instance->number_of_targets = instance->info_slot = 0;
        add_state(player, card, STATE_TAPPED);
        tapped_for_mana_color = -1;
        if (charge_mana_for_activated_ability(player, card, MANACOST_XGR(-1, 1, 1)) && spell_fizzled != 1 && pick_target(&td1, "TARGET_CREATURE")){
            instance->info_slot = 1+x_value;
        } else {
            remove_state(player, card, STATE_TAPPED);
        }
        tapped_for_mana_color = -1;
	}
		if (choice == 2){
			cancel = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
			int amount = instance->info_slot-1;
			if( valid_target(&td1) ){
				pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
										amount, 0, KEYWORD_TRAMPLE, 0);
			}
		}
	}

	return 0;
}

int card_tree_of_redemption(int player, int card, event_t event ){
	/* Tree of Redemption	|3|G
	 * Creature - Plant 0/13
	 * Defender
	 * |T: Exchange your life total with ~'s toughness. */

	if (event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);
		if (in_play(instance->parent_controller, instance->parent_card)){
			int toughness = get_toughness(instance->parent_controller, instance->parent_card);
			int legacy = real_set_pt(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, -1, life[player], 8);
			add_status(player, legacy, STATUS_INVISIBLE_FX);
			set_life_total(player, toughness);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_abattoir_ghoul(int player, int card, event_t event ){
/* CARD_ID_ABATTOIR_GHOUL	11869
Abattoir Ghoul	|3|B
Creature - Zombie 3/2
First strike
Whenever a creature dealt damage by ~ this turn dies, you gain life equal to that creature's toughness. */

	if_a_creature_damaged_by_me_dies_do_something(player, card, event, 1);
	return 0;
}

int card_ancient_grudge(int player, int card, event_t event){
	/* Ancient Grudge	|1|R
	 * Instant
	 * Destroy target artifact.
	 * Flashback |G */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_G(1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
}

int card_angel_of_flight_alabaster(int player, int card, event_t event ){
	/* Angel of Flight Alabaster	|4|W
	 * Creature - Angel 4/4
	 * Flying
	 * At the beginning of your upkeep, return target Spirit card from your graveyard to your hand. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( any_in_graveyard_by_subtype(player, SUBTYPE_SPIRIT) && ! graveyard_has_shroud(player) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select target Spirt card.");
			this_test.subtype = SUBTYPE_SPIRIT;
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 1, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

int card_angelic_overseer(int player, int card, event_t event ){
/* CARD_ID_ANGELIC_OVERSEER	11874
Angelic Overseer	|3|W|W
Creature - Angel 5/3
Flying
As long as you control a Human, ~ has hexproof and indestructible. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_HUMAN) ){
			hexproof(player, card, event);
			indestructible(player, card, event);
		}
	}

	return 0;
}

int card_armored_skaab(int player, int card, event_t event ){
/* CARD_ID_ARMORED_SKAAB	11875
Armored Skaab	|2|U
Creature - Zombie Warrior 1/4
When ~ enters the battlefield, put the top four cards of your library into your graveyard. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		mill(player, 4);
	}

	return 0;
}

int card_ashmouth_hound(int player, int card, event_t event ){
/* CARD_ID_ASHMOUTH_HOUND	11877
Ashmouth Hound	|1|R
Creature - Elemental Hound 2/1
Whenever ~ blocks or becomes blocked by a creature, ~ deals 1 damage to that creature. */

	if( ! is_humiliated(player, card, AL_TRIG) ){
		if( current_turn == player && is_attacking(player, card) && event == EVENT_DECLARE_BLOCKERS ){
			int count = active_cards_count[1-player]-1;
			while( count > -1 ){
					if( in_play(1-player, count) && is_what(1-player, count, TYPE_CREATURE) ){
						card_instance_t *this = get_card_instance(1-player, count);
						if( this->blocking == card ){
							damage_creature(1-player, count, 1, player, card);
						}
					}
					count--;
			}
		}

		if( current_turn != player && event == EVENT_DECLARE_BLOCKERS ){
			card_instance_t *instance = get_card_instance(player, card);
			if( instance->blocking < 255 ){
				damage_creature(1-player, instance->blocking, 1, player, card);
			}
		}
	}

	return 0;
}

int card_avacynian_priest(int player, int card, event_t event ){
/* CARD_ID_AVACYNIAN_PRIEST	11879
Avacynian Priest	|1|W
Creature - Human Cleric 1/2
|1, |T: Tap target non-Human creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_HUMAN;
	td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED | GAA_LITERAL_PROMPT, MANACOST_X(1), 0,
									&td, "Select target non-Human creature.");
}

int card_bane_of_hanweir(int player, int card, event_t event){
/* CARD_ID_BANE_OF_HANWEIR	11981
Bane of Hanweir	""
Creature - Werewolf 5/5
~ attacks each turn if able.
At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */

	werewolf_moon_phases(player, card, event);
	attack_if_able(player, card, event);
	return 0;
}

int card_battleground_geist(int player, int card, event_t event){
/* CARD_ID_BATTLEGROUND_GEIST	11882
Battleground Geist	|4|U
Creature - Spirit 3/3
Flying
Other Spirit creatures you control get +1/+0. */

	return boost_creature_type(player, card, event, SUBTYPE_SPIRIT, 1, 0, 0, BCT_CONTROLLER_ONLY);
}

int card_bitterheart_witch(int player, int card, event_t event){
/* CARD_ID_BITTERHEART_WITCH	11883
Bitterheart Witch	|4|B
Creature - Human Shaman 1/2
Deathtouch
When ~ dies, you may search your library for a Curse card, put it onto the battlefield attached to target player, then shuffle your library. */

	deathtouch(player, card, event);

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		char msg[100] = "Select a Curse card.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ENCHANTMENT, msg);
		this_test.subtype = SUBTYPE_CURSE;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
	}

	return 0;
}

int card_bloodcrazed_neonate(int player, int card, event_t event){
/* CARD_ID_BLOODCRAZED_NEONATE	11885
Bloodcrazed Neonate	|1|R
Creature - Vampire 2/1
~ attacks each turn if able.
Whenever ~ deals combat damage to a player, put a +1/+1 counter on it. */

	attack_if_able(player, card, event);

	return card_slith_predator(player, card, event);
}

int card_bonds_of_faith(int player, int card, event_t event){
	/* Bonds of Faith	|1|W
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature gets +2/+2 as long as it's a Human. Otherwise, it can't attack or block. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t* instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( has_subtype(p, c, SUBTYPE_HUMAN) ){
			if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
				event_result+=2;
			}
		}
		else{
			cannot_attack(p, c, event);
			cannot_block(p, c, event);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_CREATURE") ){
			if( has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_HUMAN) ){
				ai_modifier += (instance->targets[0].player == player ? 24 : -24);
			}
			else{
				ai_modifier += (instance->targets[0].player != player ? 24 : -24);
			}
		}
		return 0;
	}

	return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

int card_brain_weevil(int player, int card, event_t event ){
/* CARD_ID_BRAIN_WEEVIL	11891
Brain Weevil	|3|B
Creature - Insect 1/1
Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)
Sacrifice ~: Target player discards two cards. Activate this ability only any time you could cast a sorcery. */

    intimidate(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			new_multidiscard(instance->targets[0].player, 2, 0, player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_SACRIFICE_ME|GAA_CAN_SORCERY_BE_PLAYED, MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_bump_in_the_night(int player, int card, event_t event ){
	/* Bump in the Night	|B
	 * Sorcery
	 * Target opponent loses 3 life.
	 * Flashback |5|R */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XR(3, 1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 3);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_burning_vengeance(int player, int card, event_t event){
	/* Burning Vengeance	|2|R	0x200484b
	 * Enchantment
	 * Whenever you cast a spell from your graveyard, ~ deals 2 damage to any target. */

	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && reason_for_trigger_controller == player
		&& check_special_flags(trigger_cause_controller, trigger_cause, SF_PLAYED_FROM_GRAVE)
		&& get_owner(trigger_cause_controller, trigger_cause) == player){
		sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
	}
	if (event == EVENT_SPA){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		if (can_target(&td) && pick_target(&td, "TARGET_ANY")){ damage_target0(player, card, 2); }
		get_card_instance(player, card)->number_of_targets = 0;
	}
	return global_enchantment(player, card, event);
}

int card_butchers_cleaver(int player, int card, event_t event){
/* CARD_ID_BUTCHERS_CLEAVER	11896
Butcher's Cleaver	|3
Artifact - Equipment
Equipped creature gets +3/+0.
As long as equipped creature is a Human, it has lifelink.
Equip |3 */

	if( event == EVENT_ABILITIES && is_equipping(player, card) ){
		card_instance_t* instance = in_play(player, card);
		if( instance && has_subtype(instance->damage_target_player, instance->damage_target_card, SUBTYPE_HUMAN) ){
			lifelink(instance->damage_target_player, instance->damage_target_card, event);
		}
	}
	return vanilla_equipment(player, card, event, 3, 3, 0, 0, 0);
}

int card_cellar_door(int player, int card, event_t event)
{
  /* Cellar Door	|2
   * Artifact
   * |3, |T: Target player puts the bottom card of his or her library into his or her graveyard.
     If it's a creature card, you put a 2/2 |Sblack Zombie creature token onto the battlefield. */

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  special_mill(instance->parent_controller, instance->parent_card, CARD_ID_CELLAR_DOOR, instance->targets[0].player, 1);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_PLAYER");
}

int card_civilized_scholar(int player, int card, event_t event ){
/* CARD_ID_CIVILIZED_SCHOLAR	11903
Civilized Scholar	|2|U
Creature - Human Advisor 0/1
|T: Draw a card, then discard a card. If a creature card is discarded this way, untap ~, then transform it.
(Homicidal Brute - 5/1) At the beginning of your end step, if Homicidal Brute didn't attack this turn, tap Homicidal Brute, then transform it. */

	double_faced_card(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		draw_cards(player, 1);
		if( hand_count[player] > 0 ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to discard.");
			int selected = -1;
			if( player == AI ){
				this_test.type = TYPE_CREATURE;
				selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &this_test);
				if( selected == -1 ){
					this_test.type = TYPE_ANY;
					selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &this_test);
				}
			}
			else{
				selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &this_test);
			}
			int flip = 0;
			if( is_what(player, selected, TYPE_CREATURE) ){
				flip = 1;
			}
			discard_card(player, selected);
			if( flip ){
				untap_card(instance->parent_controller, instance->parent_card);
				transform(instance->parent_controller, instance->parent_card);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_claustrophobia(int player, int card, event_t event){
/* CARD_ID_CLAUSTROPHOBIA	11905
Claustrophobia	|1|U|U
Enchantment - Aura
Enchant creature
When ~ enters the battlefield, tap enchanted creature.
Enchanted creature doesn't untap during its controller's untap step. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance( player, card );
		tap_card(instance->damage_target_player, instance->damage_target_card);
	}

	if( in_play(player, card) && get_card_instance( player, card )->damage_target_card != -1 ){
		card_instance_t *instance = get_card_instance( player, card );
		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return disabling_aura(player, card, event);
}

int card_cobbled_wings(int player, int card, event_t event){
/* CARD_ID_COBBLED_WINGS	11909
Cobbled Wings	|2
Artifact - Equipment
Equipped creature has flying.
Equip |1 (|1: Attach to target creature you control. Equip only as a sorcery.) */

	return vanilla_equipment(player, card, event, 1, 0, 0, KEYWORD_FLYING, 0);
}

int card_corpse_lunge(int player, int card, event_t event ){
	/* Corpse Lunge	|2|B
	 * Instant
	 * As an additional cost to cast ~, exile a creature card from your graveyard.
	 * ~ deals damage equal to the exiled card's power to target creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
		return count_graveyard_by_type(player, TYPE_CREATURE) > 0;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		const int *grave = get_grave(player);
		int selected = select_a_card(player, player, 2, 1, 2, -1, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0);
		instance->info_slot = get_base_power_iid(player, grave[selected]);
		if( pick_target(&td, "TARGET_CREATURE") ){
			rfg_card_from_grave(player, selected);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, instance->info_slot, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_creepy_doll(int player, int card, event_t event ){
  /* Creepy Doll |5
   * Artifact Creature — Construct 1/1
   * Indestructible
   * Whenever Creepy Doll deals combat damage to a creature, flip a coin. If you win the flip, destroy that creature. */

	indestructible(player, card, event);

	if( is_humiliated(player, card, AL_TRIG) )
		return 0;

	if( event == EVENT_DEAL_DAMAGE ){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card != -1 && damage->damage_source_player == player &&
				damage->damage_source_card == card && damage->info_slot > 0
			  ){
				instance->targets[instance->info_slot].player = damage->damage_target_player;
				instance->targets[instance->info_slot].card = damage->damage_target_card;
				instance->info_slot++;
			}
		}
	}

	if( get_card_instance(player, card)->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
		reason_for_trigger_controller == player )
	{
		card_instance_t *instance = get_card_instance(player, card);
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				int i;
				for(i=0;i<instance->info_slot;i++){
					if( instance->targets[i].player != -1 && instance->targets[i].card != -1 &&
						in_play(instance->targets[i].player, instance->targets[i].card) )
					{
						if (flip_a_coin(player, card)){
							kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
						}
					}
				}
				instance->info_slot = 0;
		}
	}
	return 0;
}

int card_curse_of_deaths_hold(int player, int card, event_t event ){
/* CARD_ID_CURSE_OF_DEATHS_HOLD	11914
Curse of Death's Hold	|3|B|B
Enchantment - Aura Curse
Enchant player
Creatures enchanted player controls get -1/-1. */

	if( in_play(player, card) && get_card_instance(player, card)->targets[0].player != -1 && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == instance->targets[0].player ){
			event_result--;
		}
	}

	return curse(player, card, event);
}

int card_curse_of_oblivion(int player, int card, event_t event ){
/* CARD_ID_CURSE_OF_OBLIVION	11915
Curse of Oblivion	|3|B
Enchantment - Aura Curse
Enchant player
At the beginning of enchanted player's upkeep, that player exiles two cards from his or her graveyard. */

	if( in_play(player, card) && get_card_instance(player, card)->targets[0].player != -1 && ! is_humiliated(player, card, AL_TRIG) ){
		card_instance_t *instance = get_card_instance(player, card);

		upkeep_trigger_ability(player, card, event, instance->targets[0].player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			int p = instance->targets[0].player;
			int i = 0;
			while( get_grave(p)[0] != -1 && i < 2 ){
					char msg[100] = "Select a card to exile.";
					test_definition_t this_test;
					new_default_test_definition(&this_test, TYPE_ANY, msg);
					new_global_tutor(p, p, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test);
					i++;
			}
		}
	}

	return curse(player, card, event);
}

int card_curse_of_stalked_prey(int player, int card, event_t event ){
/* CARD_ID_CURSE_OF_STALKED_PREY	11916
Curse of Stalked Prey	|1|R
Enchantment - Aura Curse
Enchant player
Whenever a creature deals combat damage to enchanted player, put a +1/+1 counter on that creature. */

	card_instance_t* instance;

	if( (instance = in_play(player, card)) && instance->targets[0].player != -1 && ! is_humiliated(player, card, AL_TRIG) ){
		int p = instance->targets[0].player;

		card_instance_t* damage = combat_damage_being_dealt(event);
		if (damage &&
			damage->damage_target_player == p &&
			damage->damage_target_card == -1 && !check_special_flags(damage->damage_source_player, damage->damage_source_card, SF_ATTACKING_PWALKER) &&
			instance->info_slot < 40	// targets[1] through targets[10], 8 bytes each; 2 bytes per creature
		   ){
			unsigned char* creatures = (unsigned char*)(&instance->targets[1].player);
			creatures[2 * instance->info_slot] = damage->damage_source_player;
			creatures[2 * instance->info_slot + 1] = damage->damage_source_card;
			instance->info_slot++;
		}

		if( trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
			reason_for_trigger_controller == player && instance->info_slot > 0 ){
			if(event == EVENT_TRIGGER){
				event_result |= 2;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					unsigned char* creatures = (unsigned char*)(&instance->targets[1].player);
					int i;
					for (i = 0; i < instance->info_slot; i++){
						add_counters(player, card, creatures[2 * i], creatures[2 * i + 1], COUNTER_P1_P1, 1);
					}
					instance->info_slot = 0;
			}
		}
	}
	return curse(player, card, event);
}

int card_curse_of_the_bloody_tome(int player, int card, event_t event ){
/* CARD_ID_CURSE_OF_THE_BLOODY_TOME	11917
Curse of the Bloody Tome	|2|U
Enchantment - Aura Curse
Enchant player
At the beginning of enchanted player's upkeep, that player puts the top two cards of his or her library into his or her graveyard. */


	if( in_play(player, card) && get_card_instance(player, card)->targets[0].player != -1 && ! is_humiliated(player, card, AL_TRIG) ){
		card_instance_t *instance = get_card_instance(player, card);

		upkeep_trigger_ability(player, card, event, instance->targets[0].player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			mill(instance->targets[0].player, 2);
		}
	}
	return curse(player, card, event);
}

int card_curse_of_the_nightly_hunt(int player, int card, event_t event ){
/* CARD_ID_CURSE_OF_THE_NIGHTLY_HUNT	11918
Curse of the Nightly Hunt	|2|R
Enchantment - Aura Curse
Enchant player
Creatures enchanted player controls attack each turn if able. */

	if( in_play(player, card) && get_card_instance(player, card)->targets[0].player != -1 && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance(player, card);
		all_must_attack_if_able(instance->targets[0].player, event, -1);
	}

	return curse(player, card, event);
}

int card_curse_of_the_pierced_heart(int player, int card, event_t event ){
	/* Curse of the Pierced Heart	|1|R	0x200488c
	 * Enchantment - Aura Curse
	 * Enchant player
	 * At the beginning of enchanted player's upkeep, ~ deals 1 damage to that player or a planeswalker that player controls. */
	if( in_play(player, card) && get_card_instance(player, card)->targets[0].player != -1 && ! is_humiliated(player, card, AL_TRIG) ){
		card_instance_t *instance = get_card_instance(player, card);

		upkeep_trigger_ability(player, card, event, instance->targets[0].player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			target_definition_t td;
			default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
			td.allowed_controller = instance->targets[0].player;
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			if (can_target(&td) && pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER")){
				damage_target0(player, card, 1);
			}
		}

	}

	return curse(player, card, event);
}

int card_darkthicket_wolf(int player, int card, event_t event ){
/* CARD_ID_DARKTHICKET_WOLF	11920
Darkthicket Wolf	|1|G
Creature - Wolf 2/2
|2|G: ~ gets +2/+2 until end of turn. Activate this ability only once each turn. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_ONCE_PER_TURN, MANACOST_XG(2, 1), 0, NULL, NULL) ){
			return 2;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 2, 2);
	}

	return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST_XG(2, 1), 0, NULL, NULL);
}

int card_daybreak_ranger(int player, int card, event_t event){
/* CARD_ID_DAYBREAK_RANGER	11921
Daybreak Ranger	|2|G
Creature - Human Archer Werewolf 2/2
|T: ~ deals 2 damage to target creature with flying.
At the beginning of each upkeep, if no spells were cast last turn, transform ~.
(Nightfall Predator - 4/4) |R, |T: Nightfall Predator fights target creature. (Each deals damage equal to its power to the other.)
At the beginning of each upkeep, if a player cast two or more spells last turn, transform Nightfall Predator. */

	double_faced_card(player, card, event);

	human_moon_phases(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_abilities = KEYWORD_FLYING;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target creature with flying.");
}

int card_dead_weight(int player, int card, event_t event){
/* CARD_ID_DEAD_WEIGHT	11923
Dead Weight	|B
Enchantment - Aura
Enchant creature
Enchanted creature gets -2/-2. */

	return generic_aura(player, card, event, 1-player, -2, -2, 0, 0, 0, 0, 0);
}

int card_delver_of_secrets(int player, int card, event_t event){
/* CARD_ID_DELVER_OF_SECRETS	11925
Delver of Secrets	|U
Creature - Human Wizard 1/1
At the beginning of your upkeep, look at the top card of your library. You may reveal that card. If an instant or sorcery card is revealed this way, transform ~.
(Insectile Aberration - 3/2) Flying */

	double_faced_card(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int *deck = deck_ptr[player];
		int ai_choice = 1;
		if( ! is_what(-1, deck[0], TYPE_PERMANENT) ){
			ai_choice = 0;
		}
		if( player != AI ){
			show_deck( player, deck, 1, "Here's the first card of your deck", 0, gs_done_008b40e0 );
			if( ai_choice == 0 ){
				int choice = do_dialog(player, player, card, -1, -1, " Reveal this card\n Pass", 0);
				if( choice == 0 ){
					transform(player, card);
				}
			}
		}
		else{
			 if( ai_choice == 0 ){
				show_deck( 1-player, deck, 1, "Here's the first card of opponent's deck", 0, gs_done_008b40e0 );
				transform(player, card);
			}
		}
	}

	return 0;
}

int card_demonmail_hauberk(int player, int card, event_t event){
/* CARD_ID_DEMONMAIL_HAUBERK	11927
Demonmail Hauberk	|4
Artifact - Equipment
Equipped creature gets +4/+2.
Equip-Sacrifice a creature. */

	return altar_equipment(player, card, event, TYPE_CREATURE, 4, 2, 0);
}

int card_desperate_ravings(int player, int card, event_t event){
/* CARD_ID_DESPERATE_RAVINGS	11929
Desperate Ravings	|1|R
Instant
Draw two cards, then discard a card at random.
Flashback |2|U (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL){
		draw_cards(player, 2);
		discard(player, DISC_RANDOM, player);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XU(2, 1));
}

int card_dream_twist(int player, int card, event_t event){
	/* Dream Twist	|U
	 * Instant
	 * Target player puts the top three cards of his or her library into his or her graveyard.
	 * Flashback |1|U */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XU(1, 1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);


	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			mill(instance->targets[0].player, 3);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_elder_cathar(int player, int card, event_t event ){
/* CARD_ID_ELDER_CATHAR	11936
Elder Cathar	|2|W
Creature - Human Soldier 2/2
When ~ dies, put a +1/+1 counter on target creature you control. If that creature is a Human, put two +1/+1 counters on it instead. */

	if( this_dies_trigger(player, card, event, 2) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
			if( has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_HUMAN) ){
				add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
			}
		}
	}
	return 0;
}

int card_elite_inquisitor(int player, int card, event_t event ){
/* CARD_ID_ELITE_INQUISITOR	11938
Elite Inquisitor	|W|W
Creature - Human Soldier 2/2
First strike, vigilance
Protection from Vampires, from Werewolves, and from Zombies */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		vigilance(player, card, event);
		protection_from_subtype(player, card, event, SUBTYPE_WEREWOLF);
		protection_from_subtype(player, card, event, SUBTYPE_VAMPIRE);
		protection_from_subtype(player, card, event, SUBTYPE_ZOMBIE);
	}

	return 0;
}

int effect_evil_twin(int player, int card, event_t event ){
	/*
	  "|U|B, |T: Destroy target creature with the same name as this creature."
	*/
	if (effect_follows_control_of_attachment(player, card, event)){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->damage_target_player != -1 ){
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if (!in_play(p, c)){
			return 0;
		}

		if( get_card_name(p, c) == -1 ){ //No point of continuing if Evil Twin is cloning a face-down card, which has no name.
			return 0;
		}

		if( ! IS_GAA_EVENT(event) ){
			return 0;
		}

		target_definition_t td1;
		default_target_definition(p, c, &td1, TYPE_ANY);
		td1.extra = get_internal_card_id_from_csv_id(get_card_name(p, c));

		if( event == EVENT_CAN_ACTIVATE ){
			return granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_BU(1, 1), 0, &td1, NULL);
		}

		if( event == EVENT_ACTIVATE ){
			instance->number_of_targets = 0;
			charge_mana_for_activated_ability(p, c, MANACOST_BU(1, 1));
			if( spell_fizzled != 1 ){
				char buffer[100];
				card_ptr_t* c1 = cards_ptr[cards_data[td1.extra].id];
				scnprintf(buffer, 100, "Select a card named %s.", c1->full_name);
				if( select_target(p, c-1000, &td1, buffer, &(instance->targets[0])) ){
					tap_card(p, c);
				}
				else{
					spell_fizzled = 1;
				}
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td1) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
		}
	}

	return 0;
}

int card_evil_twin(int player, int card, event_t event)
{
	/* Evil Twin	|2|U|B	0x20048c3
	 * Creature - Shapeshifter 0/0
	 * You may have ~ enter the battlefield as a copy of any creature on the battlefield,
	 except it has "|U|B, |T: Destroy target creature with the same name as this creature." */
  if (enters_the_battlefield_as_copy_of_any_creature(player, card, event))
	  set_legacy_image(player, CARD_ID_EVIL_TWIN, create_targetted_legacy_activate(player, card, &effect_evil_twin, player, card));

  return 0;
}

static int falkenrath_noble_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_FALKENRATH_NOBLE)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }

  if (get_card_instance(player, card)->damage_target_player > -1 &&
      effect_follows_control_of_attachment(player, card, event))
  {
    return 0;
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (in_play(affected_card_controller, affected_card))
    {
      int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
      if (kill_code)
      {
        if (instance->damage_target_player > -1)
        {
          int p = instance->damage_target_player;
          int c = instance->damage_target_card;
          if (affect_me(p, c))
          {
            // Also count the source if it is no longer a creature.
            if (!is_what(p, c, TYPE_CREATURE))
              count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
            instance->damage_target_player = instance->damage_target_card = -1;
            remove_status(player, card, STATUS_INVISIBLE_FX);
            if (instance->targets[11].player < 0)
              instance->targets[11].player = 0;
            SET_BYTE2(instance->targets[11].player) = 1;
          }
        }
        count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);
    while (amount > 0)
    {
      life_sucking(player, card, 1);
      --amount;
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (!check_status(player, card, STATUS_INVISIBLE_FX) && event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_falkenrath_noble(int player, int card, event_t event){
/* CARD_ID_FALKENRATH_NOBLE	11944
Falkenrath Noble	|3|B
Creature - Vampire 2/2
Flying
Whenever ~ or another creature dies, target player loses 1 life and you gain 1 life. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (intptr_t)falkenrath_noble_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &falkenrath_noble_effect, player, card);
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
			while (amount > 0){ --amount; life_sucking(player, card, 1); }
		}
	}

	return 0;
}

int card_feeling_of_dread(int player, int card, event_t event){
	/* Feeling of Dread	|1|W
	 * Instant
	 * Tap up to two target creatures.
	 * Flashback |1|U */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XU(1, 1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				tap_card(instance->targets[i].player, instance->targets[i].card);
			}
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

int card_feral_ridgewolf(int player, int card, event_t event){
/* CARD_ID_FERAL_RIDGEWOLF	11946
Feral Ridgewolf	|2|R
Creature - Wolf 1/2
Trample
|1|R: ~ gets +2/+0 until end of turn. */

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_XR(1, 1), 2, 0);
}

int card_fiend_hunter(int player, int card, event_t event){
  /* Fiend Hunter English |1|W|W
   * 	  Creature - Human Cleric 1/3
   * 	  When Fiend Hunter enters the battlefield, you may exile another target creature.
   * 	  When Fiend Hunter leaves the battlefield, return the exiled card to the battlefield under its owner's control. */

	return_from_oblivion(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.special = TARGET_SPECIAL_NOT_ME;

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			card_instance_t *instance = get_card_instance(player, card);
			obliviation(player, card, instance->targets[0].player, instance->targets[0].card);
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_frightful_delusion(int player, int card, event_t event){
	/* Frightful Delusion	|2|U
	 * Instant
	 * Counter target spell unless its controller pays |1. That player discards a card. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card );
		if (counterspell_resolve_unless_pay_x(player, card, NULL, 0, 1)){
			discard(instance->targets[0].player, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	else{
		return counterspell(player, card, event, NULL, 0);
	}

	return 0;
}

int card_furor_of_the_bitten(int player, int card, event_t event){
/* CARD_ID_FUROR_OF_THE_BITTEN	11953
Furor of the Bitten	|R
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2 and attacks each turn if able. */

	if( in_play(player, card) && get_card_instance( player, card )->damage_target_card != -1 ){
		card_instance_t *instance = get_card_instance( player, card );
		attack_if_able(instance->damage_target_player, instance->damage_target_card, event);
	}

	return generic_aura(player, card, event, player, 2, 2, 0, 0, 0, 0, 0);
}

int card_gallows_warden(int player, int card, event_t event){
/* CARD_ID_GALLOWS_WARDEN	11954
Gallows Warden	|4|W
Creature - Spirit 3/3
Flying
Other Spirit creatures you control get +0/+1. */

	return boost_creature_type(player, card, event, SUBTYPE_SPIRIT, 0, 1, 0, BCT_CONTROLLER_ONLY);
}

int card_galvanic_juggernaut(int player, int card, event_t event){
/* CARD_ID_GALVANIC_JUGGERNAUT	11955
Galvanic Juggernaut	|4
Artifact Creature - Juggernaut 5/5
~ attacks each turn if able.
~ doesn't untap during your untap step.
Whenever another creature dies, untap ~. */

	does_not_untap(player, card, event);

	attack_if_able(player, card, event);

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, 0);
	}

	if(	resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		untap_card(player, card);
	}

	return 0;
}

int card_gastaf_howler(int player, int card, event_t event){
/* CARD_ID_GATSTAF_HOWLER	11959
Gatstaf Howler	""
Creature - Werewolf 3/3
Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)
At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */

	werewolf_moon_phases(player, card, event);
	intimidate(player, card, event);
	return 0;
}

int card_gastaf_shepherd(int player, int card, event_t event){
/* CARD_ID_GATSTAF_SHEPHERD	11958
Gatstaf Shepherd	|1|G
Creature - Human Werewolf 2/2
At the beginning of each upkeep, if no spells were cast last turn, transform ~.
(Gatstaf Howler - 3/3) Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)
At the beginning of each upkeep, if a player cast two or more spells last turn, transform Gatstaf Howler. */

	if (get_id(player, card) == CARD_ID_VILLAGE_MESSENGER){
		haste(player, card, event);
	}
	double_faced_card(player, card, event);
	human_moon_phases(player, card, event);
	return 0;
}

int card_geistcatchers_rig(int player, int card, event_t event){
/* CARD_ID_GEISTCATCHERS_RIG	11963
Geistcatcher's Rig	|6
Artifact Creature - Construct 4/5
When ~ enters the battlefield, you may have it deal 4 damage to target creature with flying. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_abilities = KEYWORD_FLYING;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_CREATURE")){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_geistflame(int player, int card, event_t event){
	/* Geistflame	|R	0x20048fa
	 * Instant
	 * ~ deals 1 damage to any target.
	 * Flashback |3|R */
	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XR(3, 1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_ghostly_possession(int player, int card, event_t event){
/* CARD_ID_GHOSTLY_POSSESSION	11965
Ghostly Possession	|2|W
Enchantment - Aura
Enchant creature
Enchanted creature has flying.
Prevent all combat damage that would be dealt to and dealt by enchanted creature. */

	if (event == EVENT_CHECK_ABILITIES){
		card_instance_t* attached = get_card_instance(player, card);
		if (affect_me(attached->damage_target_player, attached->damage_target_card) && in_play(player, card)){
			check_abilities_power -= get_power(attached->damage_target_player, attached->damage_target_card);
			check_abilities_toughness += 0x3fff;
		}
	}
	card_instance_t* instance;

	if( (instance = in_play(player, card)) && instance->damage_target_player != -1 && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t* damage = combat_damage_being_prevented(event);
		if( damage && ((damage->token_status & STATUS_COMBAT_DAMAGE) || (damage->token_status & STATUS_FIRST_STRIKE_DAMAGE)) ){
			if( (damage->damage_source_player == instance->damage_target_player && damage->damage_source_card == instance->damage_target_card) ||
				(damage->damage_target_player == instance->damage_target_player && damage->damage_target_card == instance->damage_target_card)
		      ){
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
	}

	return generic_aura(player, card, event, player, 0, 0, KEYWORD_FLYING, 0, 0, 0, 0);
}

int card_ghoulcallers_bell(int player, int card, event_t event ){
/* CARD_ID_GHOULCALLERS_BELL	11966
Ghoulcaller's Bell	|1
Artifact
|T: Each player puts the top card of his or her library into his or her graveyard. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		mill(player, 1);
		mill(1-player, 1);
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_ghoulcallers_chant(int player, int card, event_t event)
{
  /* Ghoulcaller's Chant	|B
   * Sorcery
   * Choose one - Return target creature card from your graveyard to your hand; or return two target Zombie cards from your graveyard to your hand. */

	if (!IS_GS_EVENT(player, card, event))
		return 0;

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	enum{
		CHOICE_CREATURE = 1,
		CHOICE_ZOMBIES = 2,
	};

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->info_slot = 0;
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Raise a creature", 1, 2,
							"Raise two Zombies", count_graveyard_by_subtype(player, SUBTYPE_ZOMBIE) >= 2, 3);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		switch (choice){
			case CHOICE_CREATURE:
				if (new_select_target_from_grave(player, card, player, 0, AI_MAX_VALUE, &this_test, 0) == -1)
					cancel = 1;
				else
					instance->targets[1].player = instance->targets[1].card = -1;
				break;

			case CHOICE_ZOMBIES:
			{
				test_definition_t this_test2;
				new_default_test_definition(&this_test2, TYPE_CREATURE, "Select a Zombie card.");
				this_test2.subtype = SUBTYPE_ZOMBIE;
				select_multiple_cards_from_graveyard(player, player, -1, AI_MAX_VALUE, &this_test2, 2, &instance->targets[0]);
			}
			break;
		}
		instance->info_slot = choice;
	}

	if (event == EVENT_RESOLVE_SPELL){
		int i, num_validated = 0, selected;
		for (i = 0; i < instance->info_slot; ++i){	// choice conveniently also matches the number of cards to raise
			if ((selected = validate_target_from_grave(player, card, player, i)) != -1){
				from_grave_to_hand(player, selected, TUTOR_HAND);
				++num_validated;
			}
		}

		if (num_validated == 0)
			spell_fizzled = 1;

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_ghoulraiser(int player, int card, event_t event)
{
  /* CARD_ID_GHOULRAISER	11968
   * Ghoulraiser	|1|B|B
   * Creature - Zombie 2/2
   * When ~ enters the battlefield, return a Zombie card at random from your graveyard to your hand. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    int *grave = graveyard_ptr_mutable[player];
    if (*grave != -1)
    {
      int zombies[60];
      int zombie_count = 0;
      for (int i = 0; i < count_graveyard(player); ++i)
        if (has_subtype_by_id(player, cards_data[grave[i]].id, SUBTYPE_ZOMBIE))
          zombies[zombie_count++] = i;
      if (zombie_count > 0)
      {
        int selected = zombies[internal_rand(zombie_count)];
        add_card_to_hand(player, grave[selected]);
        remove_card_from_grave(player, selected);
      }
    }
  }
  return 0;
}

int card_gnaw_to_the_bone(int player, int card, event_t event){
/* CARD_ID_GNAW_TO_THE_BONE	11969
Gnaw to the Bone	|2|G
Instant
You gain 2 life for each creature card in your graveyard.
Flashback |2|G (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL){
		gain_life(player, 2*count_graveyard_by_type(player, TYPE_CREATURE));
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XG(2, 1));
}

int card_grasp_of_phantoms(int player, int card, event_t event){
	/* Grasp of Phantoms	|3|U
	 * Sorcery
	 * Put target creature on top of its owner's library.
	 * Flashback |7|U */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XU(7, 1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_grave_bramble(int player, int card, event_t event ){
/* CARD_ID_GRAVE_BRAMBLE	11971
Grave Bramble	|1|G|G
Creature - Plant 3/4
Defender, protection from Zombies */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		protection_from_subtype(player, card, event, SUBTYPE_ZOMBIE);
	}

	return 0;
}

int card_graveyard_shovel(int player, int card, event_t event){
/* CARD_ID_GRAVEYARD_SHOVEL	11972
Graveyard Shovel	|2
Artifact
|2, |T: Target player exiles a card from his or her graveyard. If it's a creature card, you gain 2 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			const int *grave = get_grave(instance->targets[0].player);
			if( grave[0] != -1 ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile");
				int selected = new_select_a_card(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_GRAVE, 1, AI_MIN_VALUE, -1, &this_test);
				if( is_what(-1, grave[selected], TYPE_CREATURE) ){
					gain_life(player, 2);
				}
				rfg_card_from_grave(instance->targets[0].player, selected);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_PLAYER");
}

int card_gruesome_deformity(int player, int card, event_t event){
/* CARD_ID_GRUESOME_DEFORMITY	11977
Gruesome Deformity	|B
Enchantment - Aura
Enchant creature
Enchanted creature has intimidate. (It can't be blocked except by artifact creatures and/or creatures that share a color with it.) */

	return generic_aura(player, card, event, player, 0, 0, 0, SP_KEYWORD_INTIMIDATE, 0, 0, 0);
}

int card_hanweir_watchkeep(int player, int card, event_t event){
/* CARD_ID_HANWEIR_WATCHKEEP	11980
Hanweir Watchkeep	|2|R
Creature - Human Warrior Werewolf 1/5
Defender
At the beginning of each upkeep, if no spells were cast last turn, transform ~.
(Bane of Hanweir - 5/5) Bane of Hanweir attacks each turn if able.
At the beginning of each upkeep, if a player cast two or more spells last turn, transform Bane of Hanweir. */

	double_faced_card(player, card, event);
	human_moon_phases(player, card, event);
	return 0;
}

int card_harvest_pyre(int player, int card, event_t event){
/* CARD_ID_HARVEST_PYRE	11982
Harvest Pyre	|1|R
Instant
As an additional cost to cast ~, exile X cards from your graveyard.
~ deals X damage to target creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int cg = count_graveyard(player);
		if( ! cg ){
			ai_modifier-=25;
		}
		instance->number_of_targets = instance->info_slot = 0;
		if( pick_target(&td, "TARGET_CREATURE") ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");
			while( cg && (player == HUMAN || (player == AI && instance->info_slot < get_toughness(instance->targets[0].player, instance->targets[0].card))) ){
					if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test) >= 0 ){
						instance->info_slot++;
						cg--;
					}
					else{
						break;
					}
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, instance->info_slot, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_heretics_punishment(int player, int card, event_t event){
	/* Heretic's Punishment	|4|R	0x2004936
	 * Enchantment
	 * |3|R: Choose any target, then put the top three cards of your library into your graveyard. ~ deals damage to that permanent or player equal to the highest converted mana cost among those cards. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			special_mill(instance->parent_controller, instance->parent_card, CARD_ID_HERETICS_PUNISHMENT, instance->parent_controller, 3);
			card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
			if( parent->info_slot ){
				damage_target0(player, card, parent->info_slot);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(3, 1), 0, &td, "TARGET_ANY");
}

int card_hollowhenge_scavenger(int player, int card, event_t event){

	/* Hollowhenge Scavenger	|3|G|G
	 * Creature - Elemental 4/5
	 * Morbid - When ~ enters the battlefield, if a creature died this turn, you gain 5 life. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && morbid() ){
		gain_life(player, 5);
	}

	return 0;
}

int card_homicidal_brute(int player, int card, event_t event){
/* CARD_ID_HOMICIDAL_BRUTE	11904
Homicidal Brute	""
Creature - Human Mutant 5/1
At the beginning of your end step, if ~ didn't attack this turn, tap ~, then transform it. */

	double_faced_card(player, card, event);

	if( ! is_humiliated(player, card, AL_TRIG) && current_turn == player && !check_state(player, card, STATE_ATTACKED) &&
		eot_trigger(player, card, event) )
	{
		tap_card(player, card);
		transform(player, card);
	}

	return 0;
}

int card_howlpack_of_estwald(int player, int card, event_t event){
/* CARD_ID_HOWLPACK_OF_ESTWALD	12122
Howlpack of Estwald	""
Creature - Werewolf 4/6
At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */

	werewolf_moon_phases(player, card, event);
	return 0;
}

int card_hysterical_blindness(int player, int card, event_t event){
/* CARD_ID_HYSTERICAL_BLINDNESS	11987
Hysterical Blindness	|2|U
Instant
Creatures your opponents control get -4/-0 until end of turn. */

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, 1-player, -1, -4, 0, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_infernal_plunge(int player, int card, event_t event){
/* CARD_ID_INFERNAL_PLUNGE	11988
Infernal Plunge	|R
Sorcery
As an additional cost to cast ~, sacrifice a creature.
Add |R|R|R to your mana pool. */

	if( event == EVENT_RESOLVE_SPELL ){
		produce_mana(player, COLOR_RED, 3);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 0, NULL);
}

int card_into_the_maw_of_hell(int player, int card, event_t event){
/* CARD_ID_INTO_THE_MAW_OF_HELL	11993
Into the Maw of Hell	|4|R|R
Sorcery
Destroy target land. ~ deals 13 damage to target creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
		return can_target(&td1);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_LAND") ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			new_pick_target(&td1, "TARGET_CREATURE", 1, 1);
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( validate_target(player, card, &td1, 1) ){
			damage_creature(instance->targets[1].player, instance->targets[1].card, 13, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_invisible_stalker(int player, int card, event_t event){
/* CARD_ID_INVISIBLE_STALKER	11994
Invisible Stalker	|1|U
Creature - Human Rogue 1/1
Hexproof (This creature can't be the target of spells or abilities your opponents control.)
~ can't be blocked. */

	unblockable(player, card, event);
	hexproof(player, card, event);

	return 0;
}

int card_kessig_wolf2(int player, int card, event_t event)
{
  /* CARD_ID_KESSIG_WOLF	11997
Kessig Wolf	|2|R
Creature - Wolf 3/1
|1|R: ~ gains first strike until end of turn. */

  if (event == EVENT_CHECK_ABILITIES && affect_me(player, card) &&
      generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XR(1, 1), 0, NULL, NULL))
    check_abilities_keywords |= KEYWORD_FIRST_STRIKE;
  return generic_shade(player, card, event, 0, MANACOST_XR(1, 1), 0, 0, KEYWORD_FIRST_STRIKE, 0);
}

int card_laboratory_maniac(int player, int card, event_t event){

	/* Laboratory Maniac	|2|U
	 * Creature - Human Wizard 2/2
	 * If you would draw a card while your library has no cards in it, you win the game instead. */

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) &&
		reason_for_trigger_controller == player && !suppress_draw && ! is_humiliated(player, card, AL_STATIC) )
	{
		int trig = 0;
		int * deck = deck_ptr[player];
		if( deck[0] == -1 ){
			trig = 1;
		}
		if( trig == 1 ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					lose_the_game(1-player);
					suppress_draw = 1;
			}
		}
	}
	return 0;
}

int card_ludevics_test_subject(int player, int card, event_t event)
{
  /* Ludevic's Test Subject	|1|U
   * Creature - Lizard 0/3
   * Defender
   * |1|U: Put a hatchling counter on ~. Then if there are five or more hatchling counters on it, remove all of them and transform it. */

  double_faced_card(player, card, event);
  if (event == EVENT_SHOULD_AI_PLAY)
  {
    int counters = count_counters(player, card, COUNTER_HATCHLING);
    if (counters > 4)
      counters = 4;
    ai_modifier += ((unsigned int)(player == 1 ? 8 : -8)) * counters * counters;
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    add_counters(player, card, player, card, COUNTER_HATCHLING, 1);
    int counters = count_counters(player, card, COUNTER_HATCHLING);
    if (counters < 5)
    {
      if (current_phase != 0x1f || 1 - player != current_turn)
        ai_modifier += (unsigned int)(player != 1 ? 0 : -128) + 0x40;
    }
    else
    {
      remove_all_counters(player, card, player, card, COUNTER_HATCHLING);
      card_instance_t *instance = get_card_instance(player, card);
      transform(instance->parent_controller, instance->parent_card);
    }
  }
  return generic_activated_ability(player, card, event, 0, 1, 0, 1, 0, 0, 0, 0, NULL, NULL);
}

int card_lumberknot(int player, int card, event_t event ){
/* CARD_ID_LUMBERKNOT	12008
Lumberknot	|2|G|G
Creature - Treefolk 1/1
Hexproof (This creature can't be the target of spells or abilities your opponents control.)
Whenever a creature dies, put a +1/+1 counter on ~. */

	hexproof(player, card, event);

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, 0);
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if(	result ){
		int amount = BYTE0(result);
		add_counters(player, card, player, card, COUNTER_P1_P1, amount);
	}

	return 0;
}

int card_makeshift_mauler(int player, int card, event_t event ){
/* CARD_ID_MAKESHIFT_MAULER	12010
Makeshift Mauler	|3|U
Creature - Zombie Horror 4/5
As an additional cost to cast ~, exile a creature card from your graveyard. */

	skaabs(player, card, event, 1);

	return 0;
}

int card_manor_gargoyle(int player, int card, event_t event)
{
/* CARD_ID_MANOR_GARGOYLE	12011
Manor Gargoyle	|5
Artifact Creature - Gargoyle 4/4
Defender
~ has indestructible as long as it has defender.
|1: Until end of turn, ~ loses defender and gains flying. */

	if (event == EVENT_RESOLVE_ACTIVATION)
	{
		card_instance_t *instance = get_card_instance(player, card);
		create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, effect_lose_defender_gain_flying_until_eot,
										instance->parent_controller, instance->parent_card);
	}

	if( get_card_instance(player, card)->regen_status & KEYWORD_DEFENDER )
		indestructible(player, card, event);

	return generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
}

int card_manor_skeleton(int player, int card, event_t event ){
/* CARD_ID_MANOR_SKELETON	12012
Manor Skeleton	|1|B
Creature - Skeleton 1/1
Haste
|1|B: Regenerate ~. */

	haste(player, card, event);

	return regeneration(player, card, event, MANACOST_XB(1, 1));
}

int card_mask_of_avacyn(int player, int card, event_t event){
/* CARD_ID_MASK_OF_AVACYN	12014
Mask of Avacyn	|2
Artifact - Equipment
Equipped creature gets +1/+2 and has hexproof. (It can't be the target of spells or abilities your opponents control.)
Equip |3 */

	return vanilla_equipment(player, card, event, 3, 1, 2, 0,SP_KEYWORD_HEXPROOF);
}

int card_maw_of_the_mire(int player, int card, event_t event){
/* CARD_ID_MAW_OF_THE_MIRE	12016
Maw of the Mire	|4|B
Sorcery
Destroy target land. You gain 4 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			gain_life(player, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
}

int card_memorys_journey(int player, int card, event_t event)
{
  /* Memory's Journey	|1|U
   * Instant
   * Target player shuffles up to three target cards from his or her graveyard into his or her library.
   * Flashback |G */

  if (IS_CASTING(player, card, event))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, 0);
	  td.zone = TARGET_ZONE_PLAYERS;

	  card_instance_t* instance = get_card_instance(player, card);

	  if (event == EVENT_CAN_CAST)
		return can_target(&td);

	  if (event == EVENT_CAST_SPELL && affect_me(player, card)
		  && pick_target(&td, "TARGET_PLAYER"))
		select_multiple_cards_from_graveyard(player, instance->targets[0].player, 0, player == instance->targets[0].player ? AI_MAX_VALUE : AI_MIN_VALUE, NULL, 3, &instance->targets[1]);

	  if (event == EVENT_RESOLVE_SPELL)
		{
		  int i, any_shuffled_in = 0, num_targets = 0;
		  for (i = 1; i <= 3; ++i)
			if (instance->targets[i].player != -1)
			  {
				++num_targets;
				int selected = validate_target_from_grave(player, card, instance->targets[0].player, i);
				if (selected != -1)
				  {
					from_graveyard_to_deck(instance->targets[0].player, selected, 2);
					any_shuffled_in = 1;
				  }
			  }

		  if (any_shuffled_in)
			shuffle(instance->targets[0].player);

		  if (!any_shuffled_in		// at least one card was targeted but none were still legal
			  && !valid_target(&td))	// the targeted player isn't still legal
			spell_fizzled = 1;

		  kill_card(player, card, get_flashback(player, card) ? KILL_EXILE : KILL_DESTROY);
		}
	}

  return do_flashback(player, card, event, MANACOST_G(1));
}

int card_mirror_mad_phantasm(int player, int card, event_t event){
/* CARD_ID_MIRROR_MAD_PHANTASM	12024
Mirror-Mad Phantasm	|3|U|U
Creature - Spirit 5/1
Flying
|1|U: ~'s owner shuffles it into his or her library. If that player does, he or she reveals cards from the top of that library
until a card named ~ is revealed. The player puts that card onto the battlefield and all other cards revealed this way into his or her graveyard.*/

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( player == AI && in_play(instance->parent_controller, instance->parent_card)){
			regenerate_target(instance->parent_controller, instance->parent_card);
		}
		int owner = get_owner(instance->parent_controller, instance->parent_card);
		shuffle_into_library(instance->parent_controller, instance->parent_card);
		int *deck = deck_ptr[owner];
		int count = 0;
		while( deck[count] != -1 ){
				if( cards_data[deck[count]].id == CARD_ID_MIRROR_MAD_PHANTASM ){
					break;
				}
				count++;
		}
		if( deck[count] != -1 ){
			int card_added = add_card_to_hand(owner, deck[count]);
			remove_card_from_deck(owner, count);
			mill(owner, count);
			put_into_play(owner, card_added);
		}
	}

	return generic_activated_ability(player, card, event, player == AI ? GAA_REGENERATION : 0, MANACOST_XU(1, 1), 0, NULL, NULL);
}

int card_moment_of_heroism(int player, int card, event_t event){
/* CARD_ID_MOMENT_OF_HEROISM	12027
Moment of Heroism	|1|W
Instant
Target creature gets +2/+2 and gains lifelink until end of turn. (Damage dealt by the creature also causes its controller to gain that much life.)*/

	return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 2, 0, SP_KEYWORD_LIFELINK);
}

int card_moorland_haunt(int player, int card, event_t event){
	/* Moorland Haunt	""
	 * Land
	 * |T: Add |1 to your mana pool.
	 * |W|U, |T, Exile a creature card from your graveyard: Put a 1/1 |Swhite Spirit creature token with flying onto the battlefield. */

	card_instance_t *instance = get_card_instance( player, card );

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = instance->info_slot = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_UW(1, 1), 0, NULL, NULL) &&
			count_graveyard_by_type(player, TYPE_CREATURE) > 0
		  ){
			choice = do_dialog(player, player, card, -1, -1, " Add 1\n Generate a Spirit\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		else if( choice == 1 ){
				if( charge_mana_for_activated_ability(player, card, MANACOST_UW(1, 1)) ){
					test_definition_t this_test;
					new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card to exile.");
					if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test) >= 0 ){
						tap_card(player, card);
						instance->info_slot = 1;
					}
				}
				else{
					spell_fizzled = 1;
				}
		}
		else{
			spell_fizzled = 1;
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			create_spirit(instance->parent_controller, instance->parent_card, 1);
		}
	}

	return 0;
}

int card_morkrut_banshee(int player, int card, event_t event){
	/* Morkrut Banshee	|3|B|B
	 * Creature - Spirit 4/4
	 * Morbid - When ~ enters the battlefield, if a creature died this turn, target creature gets -4/-4 until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && morbid() ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td1) && pick_target(&td1, "TARGET_CREATURE") ){
			if (player == AI && instance->targets[0].player == AI){
				ai_modifier -= 500;
			}
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -4, -4);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_mulch(int player, int card, event_t event){
/* CARD_ID_MULCH	3402
Mulch	|1|G
Sorcery
Reveal the top four cards of your library. Put all land cards revealed this way into your hand and the rest into your graveyard. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL){
		int amount = MIN(4, count_deck(player));
		if( amount ){
			int *deck = deck_ptr[player];
			APNAP(p, { show_deck(p, deck, amount, "Mulch revealed these cards.", 0, gs_done_008b40e0); };);
			int i;
			for(i=0; i<amount; i++){
				if( is_what(-1, deck[0], TYPE_LAND) ){
					add_card_to_hand(player, deck[0]);
					remove_card_from_deck(player, 0);
				}
				else{
					mill(player, 1);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int murder_of_crows_legacy(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_MURDER_OF_CROWS)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }

  if (get_card_instance(player, card)->damage_target_player > -1 &&
      effect_follows_control_of_attachment(player, card, event))
  {
    return 0;
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (in_play(affected_card_controller, affected_card))
    {
      int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
      if (kill_code)
      {
        if (instance->damage_target_player > -1)
        {
          int p = instance->damage_target_player;
          int c = instance->damage_target_card;
          if (affect_me(p, c))
          {
            instance->damage_target_player = instance->damage_target_card = -1;
            remove_status(player, card, STATUS_INVISIBLE_FX);
            if (instance->targets[11].player < 0)
            {
              instance->targets[11].player = 0;
            }
            SET_BYTE2(instance->targets[11].player) = 1;
            return 0;
          }
        }
        test_definition_t this_test;
        default_test_definition(&this_test, TYPE_CREATURE);
        count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);
    while (amount)
    {
      int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                          "Draw & discard", 1, count_deck(player) > 20 ? 10 : 1, "Decline", 1, 0);
      if (choice == 1)
      {
        draw_cards(player, 1);
        discard(player, 0, player);
      }
      else
      {
        break;
      }
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_murder_of_crows(int player, int card, event_t event ){
  /* Murder of Crows |3|U|U
   * Creature — Bird 4/4
   * Flying
   * Whenever another creature dies, you may draw a card. If you do, discard a card. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (intptr_t)murder_of_crows_legacy ){
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
			int legacy = create_targetted_legacy_effect(player, card, &murder_of_crows_legacy, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
	}

	if(	get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			while( amount ){
					int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
										"Draw & discard", 1, count_deck(player) > 20 ? 10 : 1,
										"Decline", 1, 0);
					if( choice == 1 ){
						draw_cards(player, 1);
						discard(player, 0, player);
					}
					else{
						break;
					}
			}
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_nephalia_drownyard(int player, int card, event_t event){
/* CARD_ID_NEPHALIA_DROWNYARD	12033
Nephalia Drownyard	""
Land
|T: Add |C to your mana pool.
|1|U|B, |T: Target player puts the top three cards of his or her library into his or her graveyard. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE );
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		int choice = instance->info_slot = instance->number_of_targets = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XBU(2, 1, 1), 0, &td1, NULL) ){
			choice = do_dialog(player, player, card, -1, -1, " Add 1\n Mill a player\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		else if( choice == 1 ){
				add_state(player, card, STATE_TAPPED);
				tapped_for_mana_color = -1;
				if( charge_mana_for_activated_ability(player, card, MANACOST_XBU(1, 1, 1)) && pick_target(&td1, "TARGET_PLAYER") ){
					instance->info_slot = 1;
					tap_card(player, card);
				}
				else{
					remove_state(player, card, STATE_TAPPED);
				}
				tapped_for_mana_color = -1;
		}
		else{
			 spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			if( valid_target(&td1) ){
				mill(instance->targets[0].player, 3);
			}
		}
	}

	return 0;
}

int card_night_revelers(int player, int card, event_t event ){
/* CARD_ID_NIGHT_REVELERS	12035
Night Revelers	|4|R
Creature - Vampire 4/4
~ has haste as long as an opponent controls a Human. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_ABILITIES && affect_me(player, card) ){
			if( check_battlefield_for_subtype(1-player, TYPE_PERMANENT, SUBTYPE_HUMAN) ){
				haste(player, card, event);
			}
		}
	}

	return 0;
}

int card_night_terrors(int player, int card, event_t event){
/* CARD_ID_NIGHT_TERRORS	12036
Night Terrors	|2|B
Sorcery
Target player reveals his or her hand. You choose a nonland card from it. Exile that card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			this_test.type_flag = DOESNT_MATCH;

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			this_definition.effect = EC_RFG;
			new_effect_coercion(&this_definition, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_nightbirds_clutches(int player, int card, event_t event){
	/* Nightbird's Clutches	|1|R
	 * Sorcery
	 * Up to two target creatures can't block this turn.
	 * Flashback |3|R */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XR(3, 1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card,
										0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
			}
		}
		kill_card(player, card, get_flashback(player, card) ? KILL_EXILE : KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

int card_nightfall_predator(int player, int card, event_t event){
	/* Nightfall Predator	""
	 * Creature - Werewolf 4/4
	 * |R, |T: ~ fights target creature.
	 * At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~.
	 * (Daybreak Ranger - 2/2) |T: Daybreak Ranger deals 2 damage to target creature with flying.
	 * At the beginning of each upkeep, if no spells were cast last turn, transform Daybreak Ranger. */

	werewolf_moon_phases(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			fight(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_CREATURE");
}

int card_one_eyed_scarecrow(int player, int card, event_t event){
/* CARD_ID_ONE_EYED_SCARECROW	12039
One-Eyed Scarecrow	|3
Artifact Creature - Scarecrow 2/3
Defender
Creatures with flying your opponents control get -1/-0. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_POWER && affected_card_controller == 1-player &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
			check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING) )
		{
			event_result--;
		}
	}

	return 0;
}

int card_orchard_spirit(int player, int card, event_t event ){
/* CARD_ID_ORCHARD_SPIRIT	12040
Orchard Spirit	|2|G
Creature - Spirit 2/2
~ can't be blocked except by creatures with flying or reach. */

	// Used externally
	if( event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card, AL_STATIC) ){
		if( player == attacking_card_controller && card == attacking_card ){
			int abilities = get_abilities(affected_card_controller, affected_card, EVENT_ABILITIES, -1);
			if( !((abilities & KEYWORD_FLYING) || (abilities & KEYWORD_REACH)) ){
				event_result = 1;
			}
		}
	}

	return 0;
}

int card_paraselne(int player, int card, event_t event){
/* CARD_ID_PARASELENE	12042
Paraselene	|2|W
Sorcery
Destroy all enchantments. You gain 1 life for each enchantment destroyed this way. */

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		if( ! check_battlefield_for_id(2, CARD_ID_ENCHANTED_EVENING) ){
			default_test_definition(&this_test, TYPE_ENCHANTMENT);
		}
		else{
			default_test_definition(&this_test, TYPE_PERMANENT);
		}
		int result = new_manipulate_all(player, card, 2, &this_test, KILL_DESTROY);
		gain_life(player, result);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_pitchburn_devils(int player, int card, event_t event){
	/* Mudbutton Torchrunner	|2|R	0x20049d6
	 * Creature - Goblin Warrior 1/1
	 * When ~ dies, it deals 3 damage to any target. */
	/* Pitchburn Devils	|4|R	0x20049d6
	 * Creature - Devil 3/3
	 * When ~ dies, it deals 3 damage to any target. */
	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0) ){
			damage_target_creature_or_player(instance->targets[0].player, instance->targets[0].card, 3, player, card);
		}
	}

	return 0;
}

int card_prey_upon(int player, int card, event_t event){
	/* Pounce	|1|G
	 * Instant
	 * Target creature you control fights target creature you don't control. (Each deals damage equal to its power to the other.) */
	/* Prey Upon	|G
	 * Sorcery
	 * Target creature you control fights target creature you don't control. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller = 1-player;
	td1.preferred_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
		return can_target(&td1);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "Select target creature you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			new_pick_target(&td1, "Select target creature opponent controls.", 1, 1 | GS_LITERAL_PROMPT);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td1, 1) ){
			fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player, instance->targets[1].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_purify_the_grave(int player, int card, event_t event){
	/* Purify the Grave	|W
	 * Instant
	 * Exile target card from a graveyard.
	 * Flashback |W */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_W(1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			rfg_card_from_grave(instance->targets[0].player, selected);
		}
		kill_card(player, card, get_flashback(player, card) ? KILL_EXILE : KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
}

static int rage_thrower_legacy(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_RAGE_THROWER)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }

  if (get_card_instance(player, card)->damage_target_player > -1 &&
      effect_follows_control_of_attachment(player, card, event))
  {
    return 0;
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (in_play(affected_card_controller, affected_card))
    {
      int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
      if (kill_code)
      {
        if (instance->damage_target_player > -1)
        {
          int p = instance->damage_target_player;
          int c = instance->damage_target_card;
          if (affect_me(p, c))
          {
            instance->damage_target_player = instance->damage_target_card = -1;
            remove_status(player, card, STATUS_INVISIBLE_FX);
            if (instance->targets[11].player < 0)
            {
              instance->targets[11].player = 0;
            }
            SET_BYTE2(instance->targets[11].player) = 1;
            return 0;
          }
        }
        test_definition_t this_test;
        default_test_definition(&this_test, TYPE_CREATURE);
        count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.zone = TARGET_ZONE_PLAYERS;
    td.allow_cancel = 0;

    card_instance_t *instance = get_card_instance(player, card);

    while (amount)
    {
      if (can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 1, 0))
      {
        damage_player(instance->targets[1].player, 2, player, card);
        instance->number_of_targets = 0;
      }
      amount--;
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_rage_thrower(int player, int card, event_t event){
	/* Rage Thrower	|5|R	0x20049e5
	 * Creature - Human Shaman 4/2
	 * Whenever another creature dies, ~ deals 2 damage to target player or planeswalker. */
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (intptr_t)rage_thrower_legacy ){
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
			int legacy = create_targetted_legacy_effect(player, card, &rage_thrower_legacy, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			target_definition_t td;
			default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			td.allow_cancel = 0;

			card_instance_t *instance = get_card_instance(player, card);

			while( amount ){
					if( can_target(&td) && pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER") ){
						damage_target0(player, card, 2);
						instance->number_of_targets = 0;
					}
					amount--;
			}
		}
	}

	return 0;
}

int card_rally_the_peasants(int player, int card, event_t event){
/* CARD_ID_RALLY_THE_PEASANTS	12049
Rally the Peasants	|2|W
Instant
Creatures you control get +2/+0 until end of turn.
Flashback |2|R (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 2, 0, 0, 0);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			 kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XR(2, 1));
}

int card_rangers_guile(int player, int card, event_t event){
	/* Ranger's Guile	|G
	 * Instant
	 * Target creature you control gets +1/+1 and gains hexproof until end of turn. */
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 1, 1, 0, SP_KEYWORD_HEXPROOF);
}

int card_rebuke(int player, int card, event_t event){
/* CARD_ID_REBUKE	12052
Rebuke	|2|W
Instant
Destroy target attacking creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target attacking creature", 1, NULL);
}

int card_rolling_temblor(int player, int card, event_t event){
/* CARD_ID_ROLLING_TEMBLOR	12056
Rolling Temblor	|2|R
Sorcery
~ deals 2 damage to each creature without flying.
Flashback |4|R|R (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		this_test.keyword_flag = DOESNT_MATCH;
		new_damage_all(player, card, ANYBODY, 2, 0, &this_test);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XR(4, 2));
}

int card_runechanters_pike(int player, int card, event_t event){
/* CARD_ID_RUNECHANTERS_PIKE	12059
Runechanter's Pike	|2
Artifact - Equipment
Equipped creature has first strike and gets +X/+0, where X is the number of instant and sorcery cards in your graveyard.
Equip |2 */

	if( in_play(player, card) && is_equipping(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_POWER && affect_me(p, c) ){
			int amount = count_graveyard_by_type(player, TYPE_SPELL);
			event_result += amount;
		}
		if( event == EVENT_ABILITIES && affect_me(p, c) ){
			event_result |= KEYWORD_FIRST_STRIKE;
		}
	}

	return basic_equipment(player, card, event, 2);
}

static int iid_has_flashback(int iid, int me, int player, int card){
	if( is_what(-1, iid, TYPE_SPELL) && cards_data[iid].cc[2] == 9 ){
		return 1;
	}
	return 0;
}

int card_runic_repetition(int player, int card, event_t event){
/* CARD_ID_RUNIC_REPETITION	12060
Runic Repetition	|2|U
Sorcery
Return target exiled card with flashback you own to your hand. */

	if( event == EVENT_CAN_CAST ){
		int *rfg = rfg_ptr[player];
		int count = count_rfg(player);
		while( count > -1 ){
				if( is_what(-1, rfg[count], TYPE_SPELL) && cards_data[rfg[count]].cc[2] == 9 ){
					return basic_spell(player, card, event);
				}
				count--;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_SPELL, "Select a card with Flashback.");
		this_test.special_selection_function = iid_has_flashback;
		int selected = new_select_a_card(player, player, TUTOR_FROM_RFG, 0, AI_MAX_VALUE, -1, &this_test);
		if( selected != -1 ){
			instance->targets[0].player = selected;
			instance->targets[0].card = rfg_ptr[player][selected];
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		card_instance_t *instance = get_card_instance( player, card );
		if( rfg_ptr[player][instance->targets[0].player] == instance->targets[0].card ){
			add_card_to_hand(player, instance->targets[0].card);
			remove_card_from_rfg(player, cards_data[instance->targets[0].card].id);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_scourge_of_geier_reach(int player, int card, event_t event){
/* CARD_ID_SCOURGE_OF_GEIER_REACH	12061
Scourge of Geier Reach	|3|R|R
Creature - Elemental 3/3
~ gets +1/+1 for each creature your opponents control. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		event_result += count_subtype(1-player, TYPE_CREATURE, -1);
	}

	return 0;
}

int card_screeching_bat(int player, int card, event_t event){
/* CARD_ID_SCREECHING_BAT	12062
Screeching Bat	|2|B
Creature - Bat 2/2
Flying
At the beginning of your upkeep, you may pay |2|B|B. If you do, transform ~.
(Stalking Vampire - 5/5) At the beginning of your upkeep, you may pay |2|B|B. If you do, transform Stalking Vampire. */

	double_faced_card(player, card, event);

	upkeep_trigger_ability_mode(player, card, event, player, has_mana_multi(player, MANACOST_XB(2, 2)) ? RESOLVE_TRIGGER_AI(player) : 0);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		charge_mana_multi(player, MANACOST_XB(2, 2));
		if( spell_fizzled != 1 ){
			transform(player, card);
		}
	}

	return 0;
}

static int selhoff_occultist_legacy(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_SELHOFF_OCCULTIST)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }

  if (get_card_instance(player, card)->damage_target_player > -1 &&
      effect_follows_control_of_attachment(player, card, event))
  {
    return 0;
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (in_play(affected_card_controller, affected_card))
    {
      int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
      if (kill_code)
      {
        if (instance->damage_target_player > -1)
        {
          int p = instance->damage_target_player;
          int c = instance->damage_target_card;
          if (affect_me(p, c))
          {
            if (!is_what(p, c, TYPE_CREATURE))
              count_for_gfp_ability(player, card, event, ANYBODY, TYPE_PERMANENT, NULL);
            instance->damage_target_player = instance->damage_target_card = -1;
            remove_status(player, card, STATUS_INVISIBLE_FX);
            if (instance->targets[11].player < 0)
            {
              instance->targets[11].player = 0;
            }
            SET_BYTE2(instance->targets[11].player) = 1;
          }
        }
        count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.zone = TARGET_ZONE_PLAYERS;
    td.allow_cancel = 0;

    card_instance_t *instance = get_card_instance(player, card);

    while (amount)
    {
      if (can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 1, 0))
      {
        mill(instance->targets[1].player, 1);
        instance->number_of_targets = 0;
      }
      amount--;
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_selhoff_occultist(int player, int card, event_t event){
  /* Selhoff Occultist |2|U
   * Creature — Human Rogue 2/3
   * Whenever Selhoff Occultist or another creature dies, target player puts the top card of his or her library into his or her graveyard. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (intptr_t)selhoff_occultist_legacy ){
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
			int legacy = create_targetted_legacy_effect(player, card, &selhoff_occultist_legacy, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( ! affect_me(player, card) ){
			count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);

			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.zone = TARGET_ZONE_PLAYERS;
			td.allow_cancel = 0;

			card_instance_t *instance = get_card_instance(player, card);

			while( amount ){
					if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
						mill(instance->targets[0].player, 1);
						instance->number_of_targets = 0;
					}
					amount--;
			}
		}
	}

	return 0;
}

int card_card_sensory_deprivation(int player, int card, event_t event){
/* CARD_ID_SENSORY_DEPRIVATION	12066
Sensory Deprivation	|U
Enchantment - Aura
Enchant creature
Enchanted creature gets -3/-0. */

	return generic_aura(player, card, event, 1-player, -3, 0, 0, 0, 0, 0, 0);
}

int card_sever_the_bloodline(int player, int card, event_t event){
	/* Sever the Bloodline	|3|B
	 * Sorcery
	 * Exile target creature and all other creatures with the same name as that creature.
	 * Flashback |5|B|B */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XB(5, 2));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			echoing_effect(player, card, instance->targets[0].player, instance->targets[0].card, &td, KILL_EXILE);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_sharpened_pitchfork(int player, int card, event_t event){
/* CARD_ID_SHARPENED_PITCHFORK	12068
Sharpened Pitchfork	|2
Artifact - Equipment
Equipped creature has first strike.
As long as equipped creature is a Human, it gets +1/+1.
Equip |1 */

	if( is_equipping(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			if( has_subtype(p, c, SUBTYPE_HUMAN) ){
				event_result++;
			}
		}
	}
	return vanilla_equipment(player, card, event, 1, 0, 0, KEYWORD_FIRST_STRIKE, 0);
}

int card_silent_departure(int player, int card, event_t event){
	/* Silent Departure	|U
	 * Sorcery
	 * Return target creature to its owner's hand.
	 * Flashback |4|U */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XU(4, 1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_silver_inlaid_dagger(int player, int card, event_t event){
/* CARD_ID_SILVER_INLAID_DAGGER	12070
Silver-Inlaid Dagger	|1
Artifact - Equipment
Equipped creature gets +2/+0.
As long as equipped creature is a Human, it gets an additional +1/+0.
Equip |2 */

	if( is_equipping(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event == EVENT_POWER) && affect_me(p, c) ){
			event_result += (has_subtype(p, c, SUBTYPE_HUMAN) ? 3 : 2);
		}
	}

	return basic_equipment(player, card, event, 2);
}

int card_skeletal_grimace(int player, int card, event_t event){
/* CARD_ID_SKELETAL_GRIMACE	12074
Skeletal Grimace	|1|B
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+1 and has "|B: Regenerate this creature." */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance( player, card );

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( IS_GAA_EVENT(event) ){
			if( event == EVENT_RESOLVE_ACTIVATION ){
				if( can_be_regenerated(p, c) ){
					regenerate_target(p,c);
				}
				return 0;
			}
			return regeneration(p, c, event, MANACOST_B(1));
		}
	}

	return generic_aura(player, card, event, player, 1, 1, 0, 0, 0, 0, 0);
}

int card_skirsdag_cultist(int player, int card, event_t event){
	/* Skirsdag Cultist	|2|R|R	0x2004a3a
	 * Creature - Human Shaman 2/2
	 * |R, |T, Sacrifice a creature: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE, MANACOST_R(1), 0,
									&td1, "TARGET_ANY");
}

static const char* target_is_vampire_werewolf_or_zombie(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  return (has_subtype(player, card, SUBTYPE_VAMPIRE) || has_subtype(player, card, SUBTYPE_WEREWOLF) || has_subtype(player, card, SUBTYPE_ZOMBIE)
		  ? NULL : gs_target_error_subtype);	//",subtype"
}

int card_slayer_of_the_wicked(int player, int card, event_t event)
{
  /* Slayer of the Wicked	|3|W
   * Creature - Human Soldier 3/2
   * When ~ enters the battlefield, you may destroy target Vampire, Werewolf, or Zombie. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		td.extra = target_predicate_extra(target_is_vampire_werewolf_or_zombie);

		card_instance_t* instance = get_card_instance(player, card);


		if (can_target(&td) && pick_next_target_noload(&td, "Select target Vampire, Werewolf, or Zombie."))
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_smite_the_monstrous(int player, int card, event_t event){

	/* Legion's Judgment	|2|W
	 * Sorcery
	 * Destroy target creature with power 4 or greater. */
	/* Smite the Monstrous	|3|W
	 * Instant
	 * Destroy target creature with power 4 or greater. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.power_requirement = 4 | TARGET_PT_GREATER_OR_EQUAL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td1, "Select target creature with power 4 or greater.", 1, NULL);
}

static int spare_from_evil_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  int p = instance->targets[0].player;
  int c = instance->targets[0].card;
  attached_gets_sp_kw2(player, card, event, SP_KEYWORD2_GRANTED_PROT_MISC);
  if (event == EVENT_PROT_MISC && affect_me(p, c) &&
      (BYTE3(event_result) == 6 ||
       !has_subtype(attacking_card_controller, attacking_card, SUBTYPE_HUMAN)))
    event_result |= 1;
  if (eot_trigger(player, card, event))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

int card_spare_from_evil(int player, int card, event_t event){
/* CARD_ID_SPARE_FROM_EVIL	12081
Spare from Evil	|1|W
Instant
Creatures you control gain protection from non-Human creatures until end of turn. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL){
		int count = active_cards_count[player]-1;
		while( count > -1 ){
				if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) ){
					create_targetted_legacy_effect(player, card, &spare_from_evil_legacy, player, count);
				}
				count--;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_spectral_flight(int player, int card, event_t event){
/* CARD_ID_SPECTRAL_FLIGHT	12082
Spectral Flight	|1|U
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2 and has flying. */

	return generic_aura(player, card, event, player, 2, 2, KEYWORD_FLYING, 0, 0, 0, 0);
}

int card_spectral_rider(int player, int card, event_t event){
/* CARD_ID_SPECTRAL_RIDER	12083
Spectral Rider	|W|W
Creature - Spirit Knight 2/2
Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.) */

	intimidate(player, card, event);

	return 0;
}

int card_spiders_spawning(int player, int card, event_t event){
/* CARD_ID_SPIDER_SPAWNING	12084
Spider Spawning	|4|G
Sorcery
Create a 1/2 |Sgreen Spider creature token with reach for each creature card in your graveyard.
Flashback |6|B (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		ai_modifier -= (count_graveyard_by_type(player, TYPE_CREATURE) < 2 ? 15 : 0);
	}

	if(event == EVENT_RESOLVE_SPELL){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SPIDER, &token);
		token.pow = 1;
		token.tou = 2;
		token.qty = count_graveyard_by_type(player, TYPE_CREATURE);
		generate_token(&token);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XB(6, 1));
}

int card_spidery_grasp(int player, int card, event_t event){
/* CARD_ID_SPIDERY_GRASP	12085
Spidery Grasp	|2|G
Instant
Untap target creature. It gets +2/+4 and gains reach until end of turn. (It can block creatures with flying.) */

	if(event == EVENT_CHECK_PUMP ){
		return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 4, 0, 0);
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
			untap_card(instance->targets[0].player, instance->targets[0].card);
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
									2, 4, KEYWORD_REACH, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_stalking_vampire(int player, int card, event_t event){
/* CARD_ID_STALKING_VAMPIRE	12063
Stalking Vampire	""
Creature - Vampire 5/5
At the beginning of your upkeep, you may pay |2|B|B. If you do, transform ~. */

	double_faced_card(player, card, event);

	upkeep_trigger_ability_mode(player, card, event, player, has_mana_multi(player, MANACOST_XB(2, 2)) ? RESOLVE_TRIGGER_AI(player) : 0);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		charge_mana_multi(player, MANACOST_XB(2, 2));
		if( spell_fizzled != 1 ){
			transform(player, card);
		}
	}

	return 0;
}

int card_stensia_bloodhall(int player, int card, event_t event){
	/* Stensia Bloodhall	""	0x2004a67
	 * Land
	 * |T: Add |C.
	 * |3|B|R, |T: ~ deals 2 damage to target player or planeswalker. */
	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TARGET_TYPE_PLANESWALKER );
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td1.allow_cancel = 0;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		int choice = instance->number_of_targets = instance->info_slot = 0;
		int ai_choice = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_XBR(3, 1, 1), 0, &td1, NULL) ){
			choice = do_dialog(player, player, card, -1, -1, " Add 1 mana\n Damage player/planeswalker\n Cancel", ai_choice);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		if( choice == 1 ){
			add_state(player, card, STATE_TAPPED);
			tapped_for_mana_color = -1;
			if( charge_mana_for_activated_ability(player, card, MANACOST_XBR(3, 1, 1)) && pick_target(&td1, "TARGET_PLAYER_OR_PLANESWALKER")){
				instance->info_slot = 1;
			}
			else{
				remove_state(player, card, STATE_TAPPED);
				spell_fizzled = 1;
			}
			tapped_for_mana_color = -1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			if( valid_target(&td1) ){
				damage_target0(player, card, 2);
			}

		}
	}

	return 0;
}

int card_stony_silence(int player, int card, event_t event)
{
  /* CARD_ID_STONY_SILENCE	12090
   * Stony Silence	|1|W
   * Enchantment
   * Activated abilities of artifacts can't be activated. */

  card_instance_t *instance;
  int result;
  test_definition_t test;
  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      default_test_definition(&test, 0x40);
      remove_abilities_by_test(2, event, &test, AL_ALL_ACT);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

int card_stromkirk_noble(int player, int card, event_t event ){
/* CARD_ID_STROMKIRK_NOBLE	12091
Stromkirk Noble	|R
Creature - Vampire 1/1
~ can't be blocked by Humans.
Whenever ~ deals combat damage to a player, put a +1/+1 counter on it. */

	if( event == EVENT_BLOCK_LEGALITY ){
		if( player == attacking_card_controller && card == attacking_card && ! is_humiliated(player, card, AL_STATIC) ){
			if( has_subtype(affected_card_controller, affected_card, SUBTYPE_HUMAN) ){
				event_result = 1;
			}
		}
	}

	return card_slith_predator(player, card, event);
}

int card_sturmgeist(int player, int card, event_t event)
{
  /* Sturmgeist	|3|U|U
   * Creature - Spirit 100/100
   * Flying
   * ~'s power and toughness are each equal to the number of cards in your hand.
   * Whenever ~ deals combat damage to a player, draw a card. */

  unsigned int damage_triggers;

  if (((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    set_characteristic_pt(hand_count[player]);
  }
  damage_triggers =
      whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
  if (damage_triggers == 0)
  {
    return 0;
  }
  draw_cards(player, (damage_triggers & 0xff) + (damage_triggers >> 8 & 0xff));
  return 0;
}

int card_terror_of_kruin_pass(int player, int card, event_t event){

	/* Terror of Kruin Pass	""
	 * Creature - Werewolf 3/3
	 * Double strike
	 * Werewolves you control have menace.
	 * At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */

	werewolf_moon_phases(player, card, event);

	if ((event == EVENT_DECLARE_BLOCKERS || event == EVENT_ABILITIES) && ! is_humiliated(player, card, AL_STATIC) ){
		int c;
		for (c = 0; c < active_cards_count[player]; ++c){
			if (in_play(player, c) && has_subtype(player, c, SUBTYPE_WEREWOLF)){
				menace(player, c, event);
			}
		}
	}

	return 0;
}

int card_thraben_sentry(int player, int card, event_t event ){
/* CARD_ID_THRABEN_SENTRY	12096
Thraben Sentry	|3|W
Creature - Human Soldier 2/2
Vigilance
Whenever another creature you control dies, you may transform ~.
(Thraben Militia - 5/4) Trample */

	double_faced_card(player, card, event);

	vigilance(player, card, event);

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
	}

	if(	resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		transform(player, card);
	}

	return 0;
}

int card_traitorous_blood(int player, int card, event_t event){
/* CARD_ID_TRAITOROUS_BLOOD	12100
Traitorous Blood	|1|R|R
Sorcery
Gain control of target creature until end of turn. Untap it. It gains trample and haste until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		if( valid_target(&td) ){
			effect_act_of_treason_and_modify_pt_or_abilities(player, card, instance->targets[0].player, instance->targets[0].card,
															0, 0, KEYWORD_TRAMPLE, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_travel_preparations(int player, int card, event_t event)
{
  /* Travel Preparations	|1|G
   * Sorcery
   * Put a +1/+1 counter on each of up to two target creatures.
   * Flashback |1|W */

  target_t *target_player_entry;
  target_t *target_card_entry;
  card_instance_t *instance;
  int i;
  target_definition_t *creature_target_definition;
  target_definition_t td;

  if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
  {
    return do_flashback(player, card, event, 1, 0, 0, 0, 0, 1);
  }
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (instance->number_of_targets != '\0')
    {
      i = 0;
      do
      {
        if (validate_target(player, card, &td, i) == 0)
        {
          i = i + 1;
          continue;
        }
        target_card_entry = instance->targets + i;
        target_player_entry = instance->targets + i;
        i = i + 1;
        add_counters(player, card, target_player_entry->player, target_card_entry->card,
                     COUNTER_P1_P1, 1);
      } while (i < (int)(unsigned int)instance->number_of_targets);
    }
    if (get_flashback(player, card) == 0)
    {
      kill_card(player, card, KILL_DESTROY);
    }
    else
    {
      kill_card(player, card, KILL_EXILE);
    }
  }
  creature_target_definition = &td;
  return generic_spell(player, card, event, 0x20, creature_target_definition, "TARGET_CREATURE", 2,
                       NULL);
}

int card_travelers_amulet(int player, int card, event_t event){
/* CARD_ID_TRAVELERS_AMULET	12102
Traveler's Amulet	|1
Artifact
|1, Sacrifice ~: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		tutor_basic_land(player, 0, 0);
	}
	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
}

int card_trepanation_blade(int player, int card, event_t event)
{
/* CARD_ID_TREPANATION_BLADE	12104
Trepanation Blade	|3
Artifact - Equipment
Whenever equipped creature attacks, defending player reveals cards from the top of his or her library until he or she reveals a land card.
The creature gets +1/+0 until end of turn for each card revealed this way. That player puts the revealed cards into his or her graveyard.
Equip |2 */

	if (is_equipping(player, card)){
	card_instance_t *instance = get_card_instance(player, card);
	store_attackers(player, card, event, 0, instance->damage_target_player, instance->damage_target_card, NULL);
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY){
		int pump = 0;
		int *deck = deck_ptr[1-player];
		while (deck[pump] != -1 && !is_what(-1, deck[pump], TYPE_LAND)){
			pump++;
		}
		if (deck[pump] != -1){	// still a card, so it must be the revealed land
			pump++;
		}
		if (pump > 0){
			show_deck(player, deck, pump, "Cards revealed with Trepan. Blade", 0, gs_done_008b40e0);
			if (in_play(current_turn, BYTE0(instance->targets[2].player))){
				pump_until_eot(player, card, current_turn, BYTE0(instance->targets[2].player), pump, 0);
			}
			mill(1-player, pump);
		}
	}
	}
	return basic_equipment(player, card, event, 2);
}

int card_tribute_to_hunger(int player, int card, event_t event ){
/* CARD_ID_TRIBUTE_TO_HUNGER	12105
Tribute to Hunger	|2|B
Instant
Target opponent sacrifices a creature. You gain life equal to that creature's toughness. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature to sacrifice");
			int sac = new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED | SAC_NO_CANCEL | SAC_JUST_MARK | SAC_RETURN_CHOICE, &this_test);
			if( sac ){
				int amount = get_toughness(BYTE2(sac), BYTE3(sac));
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				gain_life(player, amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_ulvenwald_primordials(int player, int card, event_t event){
/* CARD_ID_ULVENWALD_PRIMORDIALS	12108
Ulvenwald Primordials	""
Creature - Werewolf 5/5
|G: Regenerate ~.
At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */

	werewolf_moon_phases(player, card, event);
	return regeneration(player, card, event, MANACOST_G(1));
}

int card_unruly_mob(int player, int card, event_t event ){
/* CARD_ID_UNRULY_MOB	12112
Unruly Mob	|1|W
Creature - Human 1/1
Whenever another creature you control dies, put a +1/+1 counter on ~. */

	if( event == EVENT_GRAVEYARD_FROM_PLAY && in_play(player, card)){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		count_for_gfp_ability(player, card, event, player, 0, &this_test);
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if(	result ){
		int amount = BYTE0(result);
		add_counters(player, card, player, card, COUNTER_P1_P1, amount);
	}

	return 0;
}

static const char* is_enchantment_or_spirit_permanent(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
	if( is_what(player, card, TYPE_ENCHANTMENT) || has_subtype(player, card, SUBTYPE_SPIRIT) ){
		return NULL;
	}
	return gs_target_error_subtype;//",subtype"
}

int card_urgent_exorcism(int player, int card, event_t event){
/* CARD_ID_URGENT_EXORCISM	12113
Urgent Exorcism	|1|W
Instant
Destroy target Spirit or enchantment. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(is_enchantment_or_spirit_permanent);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target enchantment or Spirit permanent.", 1,
						NULL);
}

int card_vampire_interloper(int player, int card, event_t event){
/* CARD_ID_VAMPIRE_INTERLOPER	12114
Vampire Interloper	|1|B
Creature - Vampire Scout 2/1
Flying
~ can't block. */

	cannot_block(player, card, event);
	return 0;
}

static const char* target_is_not_vampire_werewolf_or_zombie(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  return (has_subtype(player, card, SUBTYPE_VAMPIRE) || has_subtype(player, card, SUBTYPE_WEREWOLF) || has_subtype(player, card, SUBTYPE_ZOMBIE)
		  ? gs_target_error_subtype : NULL);	//",subtype"
}

int card_victim_of_night(int player, int card, event_t event)
{
  /* Victim of Night	|B|B
   * Instant
   * Destroy target non-Vampire, non-Werewolf, non-Zombie creature. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = target_predicate_extra(target_is_not_vampire_werewolf_or_zombie);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (valid_target(&td))
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET|GS_LITERAL_PROMPT,
						&td, "Select target non-Vampire, non-Werewolf, non-Zombie creature.", 1, NULL);
}

int card_village_bell_ringer(int player, int card, event_t event){
/* CARD_ID_VILLAGE_BELL_RINGER	12117
Village Bell-Ringer	|2|W
Creature - Human Scout 1/4
Flash (You may cast this spell any time you could cast an instant.)
When ~ enters the battlefield, untap all creatures you control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
	}

	return flash(player, card, event);
}

int card_village_cannibals(int player, int card, event_t event ){
/* CARD_ID_VILLAGE_CANNIBALS	12118
Village Cannibals	|2|B
Creature - Human 2/2
Whenever another Human creature dies, put a +1/+1 counter on ~. */

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.subtype = SUBTYPE_HUMAN;
		this_test.not_me = 1;
		count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if(	result ){
		int amount = BYTE0(result);
		while (amount){
			add_counters(player, card, player, card, COUNTER_P1_P1, 1);
			--amount;
		}
	}

	return 0;
}

int card_witchbane_orb(int player, int card, event_t event ){
  /* Witchbane Orb |4
   * Artifact
   * When Witchbane Orb enters the battlefield, destroy all Curses attached to you.
   * You have hexproof. (You can't be the target of spells or abilities your opponents control, including Aura spells.) */

    if( in_play(player, card) && !is_humiliated(player, card, AL_STATIC) ){
	     give_hexproof_to_player(player, card, event);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_CURSE;
		int marked[2][151] = {{0}};
		CYCLE_ALL_CARDS(p, c, 	{
									if( in_play(p, c) && new_make_test_in_play(p, c, -1, &this_test) ){
										if( get_card_instance(p, c)->targets[0].player == player ){
											marked[p][c] = 1;
										}
									}
								};
		);
		APNAP(p,{
					int i;
					for(i=150; i > -1; i--){
						if( marked[p][i] ){
							kill_card(p, marked[p][i], KILL_DESTROY);
						}
					}
				};
		);
		new_manipulate_all(player, card, 1-player, &this_test, KILL_DESTROY);
	}

	return 0;
}

static void bury_vampire(int player, int card, int t_player, int t_card)
{
  kill_card(t_player, t_card, KILL_BURY);
}

int card_wooden_stake(int player, int card, event_t event)
{
  /* CARD_ID_WOODEN_STAKE	12126
   * Wooden Stake	|2
   * Artifact - Equipment
   * Equipped creature gets +1/+0.
   * Whenever equipped creature blocks or becomes blocked by a Vampire, destroy that creature. It can't be regenerated.
   * Equip |1 (|1: Attach to target creature you control. Equip only as a sorcery.) */

  card_instance_t *instance;
  int result;
  int target_player;
  instance = is_equipping(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      if ((event == EVENT_DECLARE_BLOCKERS) ||
          (event == EVENT_CHANGE_TYPE || event == EVENT_CHECK_DESTROY_IF_BLOCKED))
      {
        instance = get_card_instance(player, card);
        target_player = (int)instance->damage_target_player;
        result = instance->damage_target_card;
        if (event == EVENT_CHANGE_TYPE)
        {
          if ((result == affected_card) && (target_player == affected_card_controller))
          {
            instance->destroys_if_blocked = instance->destroys_if_blocked | 1;
          }
        }
        else if (event == EVENT_CHANGE_TYPE || event == EVENT_CHECK_DESTROY_IF_BLOCKED)
        {
          if ((result == affected_card) && (target_player == affected_card_controller))
          {
            result = has_subtype(attacking_card_controller, attacking_card, SUBTYPE_VAMPIRE);
            if (result != 0)
            {
              event_result = event_result | 1;
            }
          }
        }
        else
        {
          instance = get_card_instance(target_player, result);
          if ((current_turn == target_player) && ((instance->state & 4) != 0))
          {
            for_each_creature_blocking_me(current_turn, result, bury_vampire, player, card);
          }
          if ((1 - current_turn == target_player) && ((instance->state & 8) != 0))
          {
            for_each_creature_blocked_by_me(target_player, result, bury_vampire, player, card);
          }
        }
      }
    }
  }
  result = vanilla_equipment(player, card, event, 1, 1, 0, 0, 0);
  return result;
}

int card_woodland_sleuth(int player, int card, event_t event){

	/* Woodland Sleuth	|3|G
	 * Creature - Human Scout 2/3
	 * Morbid - When ~ enters the battlefield, if a creature died this turn, return a creature card at random from your graveyard to your hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && morbid() ){
		tutor_random_permanent_from_grave(player, card, player, TUTOR_HAND, TYPE_CREATURE, 1, REANIMATE_DEFAULT);
	}

	return 0;
}

int card_wreath_of_geists(int player, int card, event_t event){
/* CARD_ID_WREATH_OF_GEISTS	12129
Wreath of Geists	|G
Enchantment - Aura
Enchant creature
Enchanted creature gets +X/+X, where X is the number of creature cards in your graveyard. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t* instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			event_result += count_graveyard_by_type(player, TYPE_CREATURE);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	return vanilla_aura(player, card, event, player);
}

int card_lantern_spirit(int player, int card, event_t event ){

	/*CARD_ID_LANTERN_SPIRIT	12003
      Lantern Spirit	|2|U
      Creature - Spirit 2/1
      Flying
      |U: Return ~ to its owner's hand.*/

	if( event == EVENT_RESOLVE_ACTIVATION ){

		card_instance_t *instance = get_card_instance( player, card );

		if( in_play(instance->parent_controller, instance->parent_card) ){
		bounce_permanent(instance->parent_controller, instance->parent_card);
	    }
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_U(1), 0, NULL, NULL);
}

int card_nevermore(int player, int card, event_t event ){
/* CARD_ID_NEVERMORE	12034
Nevermore	|1|W|W
Enchantment
As ~ enters the battlefield, choose a nonland card name.
Spells with the chosen name can't be cast. */

	meddling_mage_effect(player, card, event);

	return global_enchantment(player, card, event);
}
