#include "manalink.h"

// global functions

static int eot_flipper(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	double_faced_card(player, card, event);

	if( arcane_spirit_spell_trigger(player, card, event, 1+duh_mode(player)) ){
		add_counter(player, card, COUNTER_KI);
	}

	if( count_counters(player, card, COUNTER_KI) > 1 && trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player ){
		if(event == EVENT_TRIGGER){
			event_result |= 1+player;
		}
		else if( event == EVENT_RESOLVE_TRIGGER){
				true_transform(player, card);
				verify_legend_rule(player, card, instance->targets[13].card);
		}
	}

	return 0;
}

/*
static int get_card_with_specific_cmc_from_hand(int player, int card, int req_cmc, int mode){
	// mode = 1 --> card with cmc = req_cmc
	// mode = 2 --> card with cmc < req_cmc
	// mode = 4 --> card with cmc > req_cmc
	// mode = 8 --> selected card cannot be "card"
	int result = -1;
	int count = 0;
	int par_value = 1000;
	while( count < active_cards_count[player] ){
			if( in_hand(player, count) && ( !(mode & 4) || count != card) ){
				if( ((mode & 1)&& get_cmc(player, count) == req_cmc) ||
					((mode & 2) && get_cmc(player, count) < req_cmc) ||
					((mode & 4) && get_cmc(player, count) > req_cmc)
				  ){
					if( get_base_value(player, count) < par_value ){
						par_value = get_base_value(player, count);
						result = count;
					}
				}
			}
			count++;
	}
	return result;
}
*/

static int shoal(int player, int card, event_t event, test_definition_t *test, target_definition_t *td, const char *buffer,
				target_definition_t *td2, const char *buffer2)
{
// You may exile a [required color] card with converted mana cost X from your hand rather than pay [this] mana cost.

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_PITCH = 1,
		CHOICE_PLAY_NORMALLY
	};

	if( event == EVENT_MODIFY_COST ){
		test->zone = TARGET_ZONE_HAND;
		int nullify_cc = 0;
		if( player == AI ){
			test->cmc = get_cmc(player, card);
			test->cmc_flag = F5_CMC_GREATER_THAN_VALUE;
			if( check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, test) > 1 ){
				nullify_cc = 1;
			}
		}
		if( nullify_cc ){
			set_special_flags3(player, card, SF3_MANACOST_NULLIFIED);
			null_casting_cost(player, card);
		}
		else{
			remove_special_flags3(player, card, SF3_MANACOST_NULLIFIED);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			int choice = CHOICE_PLAY_NORMALLY;
			if( !played_for_free(player, card) && !is_token(player, card) && check_special_flags3(player, card, SF3_MANACOST_NULLIFIED) ){
				choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
								"Exile another card from hand", 1, 10,
								"Play normally", 1, 1);

				if( ! choice ){
					spell_fizzled = 1;
					return 0;
				}
			}

			if( choice == CHOICE_PITCH ){
				if( player == AI ){
					test->cmc = get_cmc(player, card);
					test->cmc_flag = F5_CMC_GREATER_THAN_VALUE;
					if( test->color == COLOR_TEST_BLUE ){
						int spell_cmc = get_cmc(card_on_stack_controller, card_on_stack)+2;
						test->cmc = spell_cmc-1;
						test->cmc_flag = F5_CMC_GREATER_THAN_VALUE;
					}
				}
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, test);
				if( selected == -1 ){
					spell_fizzled = 1;
					return 0;
				}
				int cmc = get_cmc(player, selected);
				rfg_card_in_hand(player, selected);
				instance->info_slot = cmc - get_cmc(player, card);
				td->allow_cancel = 0;
			}

			if( choice == CHOICE_PLAY_NORMALLY ){
				if( check_special_flags3(player, card, SF3_MANACOST_NULLIFIED) ){
					charge_mana_from_id(player, -1, event, get_id(player, card));
				}
				set_x_for_x_spells(player, card, event, x_value);
			}

			if( td != NULL ){
				pick_target(td, buffer);
				if( td2 != NULL ){
					new_pick_target(td2, buffer2, 1, 1);
				}
			}
		}
	}

	return 0;
}

static int genju_return_legacy(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		int owner = instance->targets[0].player;
		if( do_dialog(player, player, card, -1, -1, " Return Genju to your hand\n Pass", player == owner ? 0 : 1) == 0 ){
			if( player == owner ){ //If the Genju is stolen, the effect won't return it to hand as it won't find it in the controller's graveyard
				seek_grave_for_id_to_reanimate(player, card, owner, cards_data[instance->targets[0].card].id, REANIMATE_RETURN_TO_HAND);
			}
		}
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

static int genju(int player, int card, event_t event, int subtype, int pow, int tou, int key, int s_key, int clr){

	card_instance_t *instance = get_card_instance(player, card);

	if( in_play(player, card) && instance->damage_target_player != - 1 ){
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		if( event == EVENT_GRAVEYARD_FROM_PLAY ){
			if( affect_me(t_player, t_card) && in_play(t_player, t_card) ){
				card_instance_t *land = get_card_instance(t_player, t_card);
				if( land->kill_code > 0 && land->kill_code < KILL_EXILE ){
					set_special_flags3(player, card, SF3_ENCHANTED_PERMANENT_DYING);
				}
			}
			if( affect_me(player, card) && in_play(player, card) ){
				if( instance->kill_code > 0 && instance->kill_code < KILL_EXILE ){
					if( check_special_flags3(player, card, SF3_ENCHANTED_PERMANENT_DYING) ){
						int legacy = create_legacy_effect(player, card, &genju_return_legacy);
						get_card_instance(player, legacy)->targets[0].player = get_owner(player, card);
						get_card_instance(player, legacy)->targets[0].card = get_original_internal_card_id(player, card);
						get_card_instance(player, legacy)->targets[11].player = 66;
					}
				}
			}
		}

		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
		}

		if(event == EVENT_RESOLVE_ACTIVATION ){
			add_a_subtype(t_player, t_card, SUBTYPE_SPIRIT);
			land_animation2(instance->parent_controller, instance->parent_card, t_player, t_card, 1, pow, tou, key, s_key, clr, 0);
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;
	td.required_subtype = subtype;

	return targeted_aura_custom_prompt(player, card, event, &td, subtype > 0 ? get_hacked_land_text(player, card, "Select target %s.", subtype) : "Select target land.");
}

static int patron(int player, int card, event_t event, int subtype){

	if( event == EVENT_MODIFY_COST ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = subtype;

		card_instance_t *instance = get_card_instance(player, card);

		instance->info_slot = 0;

		if( new_can_sacrifice_as_cost(player, card, &test) ){
			int costs[6];
			card_ptr_t* c = cards_ptr[ get_id(player, card) ];
			costs[0] = get_updated_casting_cost(player, card, -1, event, c->req_colorless);
			costs[1] = c->req_black;
			costs[2] = c->req_blue;
			costs[3] = c->req_green;
			costs[4] = c->req_red;
			costs[5] = c->req_white;
			int count = 0;
			while( count < active_cards_count[player]  ){
					if( in_play(player, count) && is_what(player, count, TYPE_PERMANENT) && has_subtype(player, count, subtype) ){
						card_ptr_t* c1 = cards_ptr[ get_id(player, count) ];
						if( has_mana_multi(player, costs[0]-c1->req_colorless, costs[1]-c1->req_black, costs[2]-c1->req_blue, costs[3]-c1->req_green,
											costs[4]-c1->req_red, costs[5]-c1->req_white)
						  ){
							instance->info_slot = 1;
							null_casting_cost(player, card);
							break;
						}
					}
					count++;
			}
			if( instance->info_slot != 1 && ! can_sorcery_be_played(player, event) ){
				infinite_casting_cost();
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			enum{
				CHOICE_SAC = 1,
				CHOICE_PLAY_NORMALLY
			};

			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			test.subtype = subtype;

			int choice = CHOICE_SAC;
			if( has_mana_to_cast_iid(player, get_card_instance(player, card)->internal_card_id, event) ){
				choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
								get_subtype_text("Pay a %s offering", subtype), 1, 5,
								"Play normally", 1, 1);
				if( ! choice ){
					spell_fizzled = 1;
					return 0;
				}
			}

			if( choice == CHOICE_SAC ){
				int sac = new_sacrifice(player, card, player, SAC_AS_COST | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
				if( sac ){
					card_ptr_t* c1 = cards_ptr[ get_id(instance->targets[0].player, instance->targets[0].card) ];
					int costs[6];
					card_ptr_t* c = cards_ptr[ get_id(player, card) ];
					costs[0] = get_updated_casting_cost(player, card, -1, event, c->req_colorless)-c1->req_colorless;
					costs[1] = c->req_black-c1->req_black;
					costs[2] = c->req_blue-c1->req_blue;
					costs[3] = c->req_green-c1->req_green;
					costs[4] = c->req_red-c1->req_red;
					costs[5] = c->req_white-c1->req_white;
					int i;
					for(i=0; i<6; i++){
						if( costs[i] < 0 ){
							costs[i] = 0;
						}
					}
					charge_mana_multi(player, costs[0], costs[1], costs[2], costs[3], costs[4], costs[5]);
					if( spell_fizzled != 1 ){
						kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
					}
				}
				else{
					spell_fizzled = 1;
					return 0;
				}
			}

			if( choice == CHOICE_PLAY_NORMALLY ){
				charge_mana_from_id(player, card, event, get_id(player, card));
			}
		}
	}

	return flash(player, card, event);
}

int ninjutsu(int player, int card, event_t event, int cless, int black, int blue, int green, int red, int white, int csvid)
{
	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		base_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.required_state = TARGET_STATE_ATTACKING;
		td.illegal_state = TARGET_STATE_ISBLOCKED;

		if( current_turn == player && current_phase == PHASE_AFTER_BLOCKING && can_target(&td) &&
			has_mana_multi(player, cless, black, blue, green, red, white) )
		{
			set_special_flags3(player, card, SF3_MANACOST_NULLIFIED);
			null_casting_cost(player, card);
		}
		else{
			if( ! can_sorcery_be_played(player, event) ){
				infinite_casting_cost();
			}
			remove_special_flags3(player, card, SF3_MANACOST_NULLIFIED);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->number_of_targets = 0;
		if( check_special_flags3(player, card, SF3_MANACOST_NULLIFIED) ){
			target_definition_t td;
			base_target_definition(player, card, &td, TYPE_CREATURE);
			td.allowed_controller = player;
			td.preferred_controller = player;
			td.required_state = TARGET_STATE_ATTACKING;
			td.illegal_state = TARGET_STATE_ISBLOCKED;

			card_instance_t* instance = get_card_instance(player, card);
			if (charge_mana_multi(player, cless, black, blue, green, red, white) && pick_target(&td, "NINJUTSU"))
			{
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
			instance->number_of_targets = 0;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( check_special_flags3(player, card, SF3_MANACOST_NULLIFIED) ){
			add_state(player, card, STATE_ATTACKING | STATE_TAPPED);
		}
	}
/*


	if (!IS_ACTIVATING_FROM_HAND(event))
		return 0;

	target_definition_t td;
	base_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_ATTACKING;
	td.illegal_state = TARGET_STATE_ISBLOCKED;

	if (event == EVENT_CAN_ACTIVATE_FROM_HAND){
		int result = current_phase == PHASE_AFTER_BLOCKING && has_mana_multi(player, cless, black, blue, green, red, white) &&
					can_target(&td) ? 1 : 0;
		return result;
	}

	if (event == EVENT_ACTIVATE_FROM_HAND)
	{
		card_instance_t* instance = get_card_instance(player, card);
		instance->number_of_targets = 0;
		if (charge_mana_multi(player, cless, black, blue, green, red, white) && pick_target(&td, "NINJUTSU"))
		{
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			return 1;
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION_FROM_HAND)
	{
	  card_instance_t* inst;
	  int c;
	  for (c = 0; c < active_cards_count[player]; ++c)
		if ((inst = in_hand(player, c)) && get_id(player, c) == csvid)
		  {
			inst->state |= STATE_ATTACKING | STATE_TAPPED;
			put_into_play(player, c);
			break;
		  }
	}
*/
	return 0;
}

static void make_colorless_spirit(int player, int card, int amount)
{
	token_generation_t token;
	default_token_definition(player, card, CARD_ID_SPIRIT, &token);
	token.pow = token.tou = 1;
	token.color_forced = 0;
	token.qty = amount;
	generate_token(&token);
}

// cards
int card_azamuki_treachery_incarnate(int player, int card, event_t event){
/* Azamuki, Treachery Incarnate
Legendary Creature - Spirit
5/2
Remove a ki counter from Azamuki, Treachery Incarnate: Gain control of target creature until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			gain_control_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_KI), &td, "TARGET_CREATURE");
}

int card_cunning_bandit(int player, int card, event_t event){	// flip => Azamuki, Treachery Incarnate
/* CARD_ID_CUNNING_BANDIT	7455
Cunning Bandit	|1|R|R
Creature - Human Warrior 2/2
Whenever you cast a Spirit or Arcane spell, you may put a ki counter on ~.
At the beginning of the end step, if there are two or more ki counters on ~, you may flip it.
--FLIP-- */

	return eot_flipper(player, card, event);
}

int card_baku_altar(int player, int card, event_t event){
	/* Baku Altar	|2
	 * Artifact
	 * Whenever you cast a Spirit or Arcane spell, you may put a ki counter on ~.
	 * |2, |T, Remove a ki counter from ~: Put a 1/1 colorless Spirit creature token onto the battlefield. */

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		add_counter(player, card, COUNTER_KI);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		make_colorless_spirit(instance->parent_controller, instance->parent_card, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), GVC_COUNTER(COUNTER_KI), NULL, NULL);
}

int card_bile_urchin(int player, int card, event_t event){
/* CARD_ID_BILE_URCHIN	7439
Bile Urchin	|B
Creature - Spirit 1/1
Sacrifice ~: Target player loses 1 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_blademane_baku(int player, int card, event_t event){
/* CARD_ID_BLADEMANE_BAKU	7440
Blademane Baku	|1|R
Creature - Spirit 1/1
Whenever you cast a Spirit or Arcane spell, you may put a ki counter on ~.
|1, Remove X ki counters from ~: For each counter removed, ~ gets +2/+0 until end of turn. */

	if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
	{
		if( has_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			pumpable_power[player] += 2*count_counters(player, card, COUNTER_KI);
		}
	}

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		add_counter(player, card, COUNTER_KI);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		get_card_instance(player, card)->info_slot = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			int amount = count_counters(player, card, COUNTER_KI);
			if( player != AI ){
				amount = choose_a_number(player, "Remove how many counters?", count_counters(player, card, COUNTER_KI));
				if( amount <= 0 || amount > count_counters(player, card, COUNTER_KI) ){
					spell_fizzled = 1;
				}
			}
			remove_counters(player, card, COUNTER_KI, amount);
			get_card_instance(player, card)->info_slot = amount;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
						2*instance->info_slot, 0);
	}

	return 0;
}

ARCANE(card_blazing_shoal){
/*CARD_ID_BLAZING_SHOAL	7441
Blazing Shoal	|X|R|R
Instant - Arcane
You may exile a |Sred card with converted mana cost X from your hand rather than pay ~'s mana cost.
Target creature gets +X/+0 until end of turn. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, instance->info_slot, 0);
		}
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select a Red card to exile");
	test.color = COLOR_TEST_RED;
	return shoal(player, card, event, &test, &td, "TARGET_CREATURE", NULL, NULL);
}

// budoka pupil --> Cunning bandit

int card_ichiga_who_topples_oaks(int player, int card, event_t event){

	/* Ichiga, Who Topples Oaks
	 * Legendary Creature - Spirit
	 * 4/3
	 * Trample
	 * Remove a ki counter from Ichiga, Who Topples Oaks: Target creature gets +2/+2 until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_KI), &td, "TARGET_CREATURE");
}

// callow jushi --> Cunning bandit

int card_jaraku_the_interloper(int player, int card, event_t event){

	/* Jaraku the Interloper
	 * Legendary Creature - Spirit
	 * 3/4
	 * Remove a ki counter from Jaraku the Interloper: Counter target spell unless its controller pays |2. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		counterspell_resolve_unless_pay_x(player, card, &td, 0, 2);
	}

	return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST0, GVC_COUNTER(COUNTER_KI), &td, NULL);
}

int card_child_of_thorns(int player, int card, event_t event){
/* CARD_ID_CHILD_OF_THORNS	7450
Child of Thorns	|G
Creature - Spirit 1/1
Sacrifice ~: Target creature gets +1/+1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_CREATURE");
}

static const char* target_must_have_counters(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  return count_counters(player, card, -1) ? NULL : "must have counters";
}
int card_chisei_heart_of_oceans(int player, int card, event_t event)
{
  /* Chisei, Heart of Oceans	|2|U|U
   * Legendary Creature - Spirit 4/4
   * Flying
   * At the beginning of your upkeep, sacrifice ~ unless you remove a counter from a permanent you control. */

  check_legend_rule(player, card, event);
  upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  target_definition_t td;
	  base_target_definition(player, card, &td, TYPE_PERMANENT);
	  td.preferred_controller = player;
	  td.allowed_controller = player;
	  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	  td.extra = (int)target_must_have_counters;

	  card_instance_t* instance = get_card_instance(player, card);
	  instance->number_of_targets = 0;
	  if (can_target(&td) && pick_target(&td, "TARGET_PERMANENT"))
		{
		  instance->number_of_targets = 0;
		  counter_t typ = choose_existing_counter_type(player, player, card, instance->targets[0].player, instance->targets[0].card,
													   CECT_HUMAN_CAN_CANCEL | CECT_REMOVE | CECT_CONSIDER_ALL, -1, -1);
		  if (typ != COUNTER_invalid)
			{
			  remove_counter(instance->targets[0].player, instance->targets[0].card, typ);
			  return 0;
			}
		}

	  kill_card(player, card, KILL_SACRIFICE);
	}

  return 0;
}

ARCANE(card_crack_the_earth)
{
/* CARD_ID_CRACK_THE_EARTH	7453
Crack the Earth	|R
Sorcery - Arcane
Each player sacrifices a permanent. */

  if (event == EVENT_CAN_CAST)
	return 1;

  if (event == EVENT_RESOLVE_SPELL)
	APNAP(p,
		  player_sacrifices_a_permanent(player, card, p, TYPE_PERMANENT, SAC_NO_CANCEL));

  return 0;
}

int card_day_of_destiny(int player, int card, event_t event){
/* CARD_ID_DAY_OF_DESTINY	7457
Day of Destiny	|3|W
Legendary Enchantment
Legendary creatures you control get +2/+2. */

	check_legend_rule(player, card, event);

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && in_play(player, card) && ! is_humiliated(player, card) ){
		if( affected_card_controller == player && is_what(affected_card_controller,affected_card, TYPE_CREATURE) &&
			is_legendary(affected_card_controller,affected_card) )
		{
			event_result += 2;
		}
	}

	return global_enchantment(player, card, event);
}

ARCANE(card_disrupting_shoal){
/* CARD_ID_DISRUPTING_SHOAL	7458
Disrupting Shoal	|X|U|U
Instant - Arcane
You may exile a |Sblue card with converted mana cost X from your hand rather than pay ~'s mana cost.
Counter target spell if its converted mana cost is X. */

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select a Blue card to exile");
	test.color = COLOR_TEST_BLUE;

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_MODIFY_COST ){
		return shoal(player, card, event, &test, &td, NULL, NULL, NULL);
	}

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player == AI ){
			int spell_cmc = get_cmc(card_on_stack_controller, card_on_stack)+2;
			test.cmc = spell_cmc-1;
			test.cmc_flag = F5_CMC_GREATER_THAN_VALUE;
			test.zone = TARGET_ZONE_HAND;
			if( ! check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test) ){
				ai_modifier -= 100;
			}
		}
		shoal(player, card, event, &test, NULL, NULL, NULL, NULL);
		if( spell_fizzled != 1 ){
			return counterspell(player, card, event, &td, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( get_cmc(instance->targets[0].player, instance->targets[0].card) == instance->info_slot ){
			return counterspell(player, card, event, &td, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_empty_shrine_kannushi(int player, int card, event_t event){
/* CARD_ID_EMPTY_SHRINE_KANNUSHI	7459
Empty-Shrine Kannushi	|W
Creature - Human Cleric 1/1
~ has protection from the colors of permanents you control. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card) ){
		int keyword = 0;
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && is_what(p, c, TYPE_PERMANENT) && ! is_what(p, c, TYPE_LAND) ){
										int clr = get_color(p, c);
										int i;
										for(i=1; i<6; i++){
											if( clr & (1<<i) ){
												keyword |= (1<<(10+i));
											}
										}
									}
								};
		);
		event_result |= keyword;
	}
	return 0;
}

int card_enshrined_memories(int player, int card, event_t event){
/*
Enshrined Memories |X|G
Sorcery
Reveal the top X cards of your library. Put all creature cards revealed this way into your hand and the rest on the bottom of your library
in any order.
*/
	if(event == EVENT_RESOLVE_SPELL){
		card_instance_t *instance = get_card_instance( player, card );
		int max = MIN(count_deck(player), instance->info_slot);

		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		SET_BYTE0(test.qty) = max;
		SET_BYTE1(test.qty) = max;

		reveal_card_until_you_reveal_test(player, player, &test, RCUYRT_REVEAL_UP_TO_X | RCUYRT_ADD_TEST_TO_HAND | RCUYRT_PUT_REST_ON_BOTTOM);

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

// faithful squire --> Cunning bandit

int card_kaiso_memory_of_loyalty(int player, int card, event_t event){

	/* Kaiso, Memory of Loyalty
	 * Legendary Creature - Spirit
	 * 3/4
	 * Flying
	 * Remove a ki counter from Kaiso, Memory of Loyalty: Prevent all damage that would be dealt to target creature this turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_all_damage_to_target(instance->parent_controller, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_KI), &td, "TARGET_CREATURE");
}

int card_final_judgment(int player, int card, event_t event){
	/*
	  Final Judgment |4|W|W
	  Sorcery
	  Exile all creatures.
	*/
	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		APNAP(p, {new_manipulate_all(player, card, p, &this_test, KILL_EXILE);};);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

ARCANE(card_first_volley){
/* CARD_ID_FIRST_VOLLEY	7463
First Volley	|1|R
Instant - Arcane
~ deals 1 damage to target creature and 1 damage to that creature's controller. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		pick_target(&td, "TARGET_CREATURE");
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);
			damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
			damage_player(instance->targets[0].player, 1, player, card);
		}
	}

	return 0;
}

static int flames_of_the_blood_hand_legacy(int player, int card, event_t event){

	my_damage_cannot_be_prevented(player, card, event);

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) )
	{
		card_instance_t *leg = get_card_instance(player, card);
		if( leg->targets[3].player > -1 ){
			player_bits[leg->targets[3].player] |= PB_CANT_GAIN_LIFE;
		}
	}

	if( current_phase == PHASE_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_flames_of_the_blood_hand(int player, int card, event_t event){
	/* Flames of the Blood Hand	|2|R	0x2005084
	 * Instant
	 * ~ deals 4 damage to target player or planeswalker. The damage can't be prevented. If that player would gain life this turn, that player gains no life instead. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			int legacy = create_legacy_effect(player, card, &flames_of_the_blood_hand_legacy);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[0].player = player;
			leg->targets[0].card = card;
			if( instance->targets[0].card == -1 ){
				leg->targets[3].player = instance->targets[0].player;
			}

			damage_target0(player, card, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
}

int card_forked_branch_garami(int player, int card, event_t event){
/* CARD_ID_FORKED_BRANCH_GARAMI	7466
Forked-Branch Garami	|3|G|G
Creature - Spirit 4/4
Soulshift 4, soulshift 4 (When this creature dies, you may return up to two target Spirit cards with converted mana cost 4 or less from your graveyard to your hand.)*/

	return soulshift(player, card, event, 4, 2);
}

int card_frostling(int player, int card, event_t event){
/* CARD_ID_FROSTLING	7468
Frostling	|R
Creature - Spirit 1/1
Sacrifice ~: ~ deals 1 damage to target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_fumiko_the_lowblood(int player, int card, event_t event){
/* CARD_ID_FUMIKO_THE_LOWBLOOD	7469
Fumiko the Lowblood	|2|R|R
Legendary Creature - Human Samurai 3/2
~ has bushido X, where X is the number of attacking creatures. (Whenever this creature blocks or becomes blocked, it gets +X/+X until end of turn.)
Creatures your opponents control attack each turn if able. */

	check_legend_rule(player, card, event);

	all_must_attack_if_able(1-player, event, -1);

	if (event == EVENT_DECLARE_BLOCKERS){
		get_card_instance(player, card)->targets[8].player = count_attackers(current_turn);
	}

	return bushido(player, card, event, 0);
}

int card_genju_of_the_cedars(int player, int card, event_t event){
/* CARD_ID_GENJU_OF_THE_CEDARS	7470
Genju of the Cedars	|G
Enchantment - Aura
Enchant |H2Forest
|2: Enchanted |H2Forest becomes a 4/4 |Sgreen Spirit creature until end of turn. It's still a land.
When enchanted |H2Forest is put into a graveyard, you may return ~ from your graveyard to your hand. */

	return genju(player, card, event, SUBTYPE_FOREST, 4, 4, 0, 0, COLOR_TEST_GREEN);
}

int card_genju_of_the_falls(int player, int card, event_t event){
/* CARD_ID_GENJU_OF_THE_FALLS	7471
Genju of the Falls	|U
Enchantment - Aura
Enchant |H2Island
|2: Enchanted |H2Island becomes a 3/2 |Sblue Spirit creature with flying until end of turn. It's still a land.
When enchanted |H2Island is put into a graveyard, you may return ~ from your graveyard to your hand. */

	return genju(player, card, event, SUBTYPE_ISLAND, 3, 2, KEYWORD_FLYING, 0, COLOR_TEST_BLUE);
}

int card_genju_of_the_fields(int player, int card, event_t event){
/* CARD_ID_GENJU_OF_THE_FIELDS	7473
Genju of the Fields	|W
Enchantment - Aura
Enchant |H2Plains
|2: Until end of turn, enchanted |H2Plains becomes a 2/5 |Swhite Spirit creature with "Whenever this creature deals damage,
its controller gains that much life." It's still a land.
When enchanted |H2Plains is put into a graveyard, you may return ~ from your graveyard to your hand. */

	// Special-cased in the legacy effect
	return genju(player, card, event, SUBTYPE_PLAINS, 2, 5, 0, 0, COLOR_TEST_WHITE);
}

int card_genju_of_the_realm(int player, int card, event_t event){
/* CARD_ID_GENJU_OF_THE_REALM	7474
Genju of the Realm	|W|U|B|R|G
Legendary Enchantment - Aura
Enchant land
|2: Enchanted land becomes a legendary 8/12 Spirit creature with trample until end of turn. It's still a land.
When enchanted land is put into a graveyard, you may return ~ from your graveyard to your hand. */

	check_legend_rule(player, card, event);
	return genju(player, card, event, -1, 8, 12, KEYWORD_TRAMPLE, 0, 0);
}

int card_genju_of_the_spires(int player, int card, event_t event){
/* CARD_ID_GENJU_OF_THE_SPIRES	7475
Genju of the Spires	|R
Enchantment - Aura
Enchant |H2Mountain
|2: Enchanted |H2Mountain becomes a 6/1 |Sred Spirit creature until end of turn. It's still a land.
When enchanted |H2Mountain is put into a graveyard, you may return ~ from your graveyard to your hand. */

	return genju(player, card, event, SUBTYPE_MOUNTAIN, 6, 1, 0, 0, COLOR_TEST_RED);
}

// gnarled mass --> vanilla

// goblin cohorth -->Mogg Conscripts

int card_gods_eye_gate_to_reikai(int player, int card, event_t event){
	/* Gods' Eye, Gate to the Reikai	""
	 * Legendary Land
	 * |T: Add |1 to your mana pool.
	 * When ~ is put into a graveyard from the battlefield, put a 1/1 colorless Spirit creature token onto the battlefield. */

	check_legend_rule(player, card, event);

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		generate_token_by_id(player, card, CARD_ID_SPIRIT);
	}

	return mana_producer(player, card, event);
}

ARCANE_WITH_SPLICE(card_goryos_vengeance, MANACOST_XB(2,1)){
	// Return target legendary creature card from your graveyard to the battlefield. That creature gains haste. Exile it at the beginning of the next end step.
	// Splice onto Arcane |2|B

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target legendary creature card.");
	test.subtype = SUBTYPE_LEGEND;

	if(event == EVENT_RESOLVE_SPELL){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_HASTE_AND_EXILE_AT_EOT);
		}
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
}

int card_heartless_hidetsugu(int player, int card, event_t event){
/* CARD_ID_HEARTLESS_HIDETSUGU	7482
Heartless Hidetsugu	|3|R|R
Legendary Creature - Ogre Shaman 4/3
|T: ~ deals damage to each player equal to half that player's life total, rounded down. */

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		APNAP(p,{
					damage_player(p, ((life[p]+1)/2), instance->parent_controller, instance->parent_card);
				}
		);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_higure_the_still_wind(int player, int card, event_t event){

	/* Higure, the Still Wind	|3|U|U
	 * Legendary Creature - Human Ninja 3/4
	 * Ninjutsu |2|U|U
	 * Whenever ~ deals combat damage to a player, you may search your library for a Ninja card, reveal it, and put it into your hand. If you do, shuffle your
	 * library.
	 * |2: Target Ninja creature is unblockable this turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.required_subtype = SUBTYPE_NINJA;
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(2), 0,
											&td, "Select target Ninja creature.");
		}
		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				pump_ability_until_eot(instance->parent_controller, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
			}
		}
	}

	int packets;
	if( (packets = damage_dealt_by_me(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_TRIGGER_OPTIONAL)) ){
		test_definition_t test;
		new_default_test_definition(&test, 0, "Select a Ninja card.");
		test.subtype = SUBTYPE_NINJA;
		test.subtype_flag = MATCH;
		test.qty = packets;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_CMC, &test);
	}

	return ninjutsu(player, card, event, MANACOST_XU(2, 2), CARD_ID_HIGURE_THE_STILL_WIND);
}

// Hired muscle --> Cunning bandit

int card_scarmaker(int player, int card, event_t event){

	/* Scarmaker
	 * Legendary Creature - Spirit
	 * 4/4
	 * Remove a ki counter from Scarmaker: Target creature gains fear until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_FEAR);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_KI), &td, "TARGET_CREATURE");
}

int card_hokori_dust_drinker(int player, int card, event_t event){
/* CARD_ID_HOKORI_DUST_DRINKER	7488
Hokori, Dust Drinker	|2|W|W
Legendary Creature - Spirit 2/2
Lands don't untap during their controllers' untap steps.
At the beginning of each player's upkeep, that player untaps a land he or she controls. */

	check_legend_rule(player, card, event);

	untap_only_1_permanent_per_upkeep(player, card, event, 2, TYPE_LAND);

	return 0;
}

ARCANE_WITH_SPLICE(card_horobis_whisper, MANACOST_X(-10)){
	// If you control |Ha Swamp, destroy target non|Sblack creature.
	// Splice onto Arcane - Exile four cards from your graveyard.

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
	td.allow_cancel = 0;

	if (event == EVENT_CAN_SPLICE){
		return count_graveyard(player) >= 4;
	}

	if( event == EVENT_SPLICE){
		int i;
		for(i=0; i<4; i++){
			int selected = select_a_card(player, player, TUTOR_FROM_GRAVE, 1, AI_MIN_VALUE, -1, 0,0, 0,0, 0,0, 0,0, -1,0);
			rfg_card_from_grave(player, selected);
		}
		return 1;
	}

	if( event == EVENT_CAN_CAST  ){
		return can_target(&td);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		pick_target(&td, "TARGET_CREATURE");
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) && check_battlefield_for_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_SWAMP)) ){
			card_instance_t *instance = get_card_instance(player, card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}
	return 0;
}

int card_in_the_web_of_war(int player, int card, event_t event){
/* CARD_ID_IN_THE_WEB_OF_WAR	7491
In the Web of War	|3|R|R
Enchantment
Whenever a creature enters the battlefield under your control, it gets +2/+0 and gains haste until end of turn. */

	etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		pump_ability_until_eot(player, card, trigger_cause_controller, trigger_cause, 2, 0, 0, SP_KEYWORD_HASTE);
	}

	return global_enchantment(player, card, event);
}

int card_indebted_samurai(int player, int card, event_t event){
/* CARD_ID_INDEBTED_SAMURAI	7492
Indebted Samurai	|3|W
Creature - Human Samurai 2/3
Bushido 1 (Whenever this creature blocks or becomes blocked, it gets +1/+1 until end of turn.)
Whenever a Samurai you control dies, you may put a +1/+1 counter on ~. */

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_SAMURAI;
		count_for_gfp_ability(player, card, event, player, 0, &this_test);
	}

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		card_instance_t *instance = get_card_instance(player, card);
		add_1_1_counters(player, card, instance->targets[11].card);
		instance->targets[11].card = 0;
	}

	return bushido(player, card, event, 1);
}

int card_ink_eyes_servant_of_oni(int player, int card, event_t event){
/*
Ink-Eyes, Servant of Oni |4|B|B
Legendary Creature — Rat Ninja 5/4
Ninjutsu {3}{B}{B} ({3}{B}{B}, Return an unblocked attacker you control to hand:
		Put this card onto the battlefield from your hand tapped and attacking.)
Whenever Ink-Eyes, Servant of Oni deals combat damage to a player, you may put target creature card from that player's
	graveyard onto the battlefield under your control.
{1}{B}: Regenerate Ink-Eyes.
*/
	check_legend_rule(player, card, event);

	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE))
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		int i;
		for(i=0; i<2; i++){
			if( times_damaged[i] ){
				if( count_graveyard_by_type(i, TYPE_CREATURE) > 0 && ! graveyard_has_shroud(i) ){
					test_definition_t this_test;
					new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
					new_global_tutor(player, i, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
				}
			}
		}
	}

	if( land_can_be_played & LCBP_REGENERATION ){
		return regeneration(player, card, event, MANACOST_XB(1, 1));
	}

	return ninjutsu(player, card, event, MANACOST_XB(3, 2), CARD_ID_INK_EYES_SERVANT_OF_ONI);
}

ARCANE(card_ire_of_kaminari){
	/* Ire of Kaminari	|3|R	0x2006222
	 * Instant - Arcane
	 * ~ deals damage to any target equal to the number of Arcane cards in your graveyard. */
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		pick_target(&td, "TARGET_ANY");
	}

	if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				damage_creature_or_player(player, card, event, count_graveyard_by_subtype(player, SUBTYPE_ARCANE));
			}
	}
	return 0;
}

int card_isao_enlightned_bushi(int player, int card, event_t event){
/* CARD_ID_ISAO_ENLIGHTENED_BUSHI	7495
Isao, Enlightened Bushi	|2|G
Legendary Creature - Human Samurai 2/1
~ can't be countered.
Bushido 2 (Whenever this creature blocks or becomes blocked, it gets +2/+2 until end of turn.)
|2: Regenerate target Samurai. */

	check_legend_rule(player, card, event);

	cannot_be_countered(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_SAMURAI;
	td.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( land_can_be_played & LCBP_REGENERATION ){
		if( event == EVENT_CAN_ACTIVATE ){
			if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(2), 0, &td, NULL) ){
				return 0x63;
			}
		}
		if( event == EVENT_ACTIVATE ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(2), 0,
											&td, "Select target Samurai to regenerate.");
		}
		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) && can_regenerate(instance->targets[0].player, instance->targets[0].card) ){
				regenerate_target(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return bushido(player, card, event, 2);
}

int card_iwamori_the_open_fist(int player, int card, event_t event){
/* CARD_ID_IWAMORI_OF_THE_OPEN_FIST	7497
Iwamori of the Open Fist	|2|G|G
Legendary Creature - Human Monk 5/5
Trample
When ~ enters the battlefield, each opponent may put a legendary creature card from his or her hand onto the battlefield. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a Legendary creature card.");
		this_test.subtype = SUBTYPE_LEGEND;
		new_global_tutor(1-player, 1-player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
	}

	return 0;
}

int card_kami_of_false_hope(int player, int card, event_t event){
/* CARD_ID_KAMI_OF_FALSE_HOPE	7500
Kami of False Hope	|W
Creature - Spirit 1/1
Sacrifice ~: Prevent all combat damage that would be dealt this turn. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		fog_effect(player, card);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_kentaro_the_smiling_cat(int player, int card, event_t event){
/* Kentaro, the Smiling Cat	|1|W
 * Legendary Creature - Human Samurai 2/1
 * Bushido 1
 * You may pay |X rather than pay the mana cost for Samurai spells you cast, where X is that spell's converted mana cost. */

	check_legend_rule(player, card, event);
    bushido(player, card, event, 1);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a Samurai card to play.");
		this_test.subtype = SUBTYPE_SAMURAI;

	return you_may_spend_mana_as_though_it_were_mana_of_any_color_to_play_spell(player, card, event, 0, &this_test);
}

/* This is just an approximation */
int card_kira_great_glass_spinner(int player, int card, event_t event){
/* CARD_ID_KIRA_GREAT_GLASS_SPINNER	7504
Kira, Great Glass-Spinner	|1|U|U
Legendary Creature - Spirit 2/2
Flying
Creatures you control have "Whenever this creature becomes the target of a spell or ability for the first time each turn, counter that spell or ability."*/

	check_legend_rule(player, card, event);

	if( ! is_humiliated(player, card) ){

		if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller ){
			int trig = 0;
			if( ! is_what(trigger_cause_controller, trigger_cause, TYPE_LAND) ){
				card_instance_t *instance = get_card_instance(trigger_cause_controller, trigger_cause);
				int i;
				for(i=0; i<instance->number_of_targets; i++){
					if( instance->targets[i].card != -1 && instance->targets[i].player == player &&
						is_what(instance->targets[i].player, instance->targets[i].card, TYPE_CREATURE) &&
						! check_special_flags2(instance->targets[i].player, instance->targets[i].card, SF2_KIRA_GREAT_GLASS_SPINNER)
					  ){
						trig = 1;
						break;
					}
				}
			}

			if( trig > 0 ){
				if(event == EVENT_TRIGGER){
					event_result |= 2;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						card_instance_t *instance = get_card_instance(trigger_cause_controller, trigger_cause);
						int i;
						for(i=0; i<instance->number_of_targets; i++){
							if( instance->targets[i].card != -1 && instance->targets[i].player == player &&
								is_what(instance->targets[i].player, instance->targets[i].card, TYPE_CREATURE)
							  ){
								set_special_flags2(instance->targets[i].player, instance->targets[i].card, SF2_KIRA_GREAT_GLASS_SPINNER);
							}
						}
						kill_card(trigger_cause_controller, trigger_cause, KILL_SACRIFICE);
				}
			}
		}

		if ( stack_size >= 1){
			if( stack_data[stack_size - 1].generating_event != EVENT_RESOLVE_TRIGGER &&
				cards_data[stack_data[stack_size - 1].internal_card_id].id != 904
			  ){
				int trig = 0;
				card_instance_t *instance = get_card_instance(stack_cards[stack_size - 1].player, stack_cards[stack_size - 1].card);
				int i;
				for(i=0; i<instance->number_of_targets; i++){
					if( instance->targets[i].card != -1 && instance->targets[i].player == player &&
						is_what(instance->targets[i].player, instance->targets[i].card, TYPE_CREATURE) &&
						! check_special_flags2(instance->targets[i].player, instance->targets[i].card, SF2_KIRA_GREAT_GLASS_SPINNER)
					 ){
						trig = 1;
						break;
					}
				}
				if( trig == 1 ){
					for(i=0; i<instance->number_of_targets; i++){
						if( instance->targets[i].card != -1 && instance->targets[i].player == player &&
							is_what(instance->targets[i].player, instance->targets[i].card, TYPE_CREATURE) &&
							! check_special_flags2(instance->targets[i].player, instance->targets[i].card, SF2_KIRA_GREAT_GLASS_SPINNER)
						 ){
							set_special_flags2(instance->targets[i].player, instance->targets[i].card, SF2_KIRA_GREAT_GLASS_SPINNER);
						}
					}
					instance->internal_card_id = -1;
				}
			}
		}

		if( event == EVENT_CLEANUP || leaves_play(player, card, event) ){
			remove_special_flags2(player, -1, SF2_KIRA_GREAT_GLASS_SPINNER);
		}
	}
	return 0;
}

int card_kyoki_sanitys_eclipse(int player, int card, event_t event){
/* CARD_ID_KYOKI_SANITYS_ECLIPSE	7508
Kyoki, Sanity's Eclipse	|4|B|B
Legendary Creature - Demon Spirit 6/4
Whenever you cast a Spirit or Arcane spell, target opponent exiles a card from his or her hand. */

	check_legend_rule(player, card, event);

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.zone = TARGET_ZONE_PLAYERS;

		if( would_validate_arbitrary_target(&td, 1-player, -1) && hand_count[1-player] > 0 ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);

			ec_definition_t this_definition;
			default_ec_definition(1-player, 1-player, &this_definition);
			this_definition.effect = EC_RFG;
			this_definition.ai_selection_mode = AI_MIN_VALUE;

			new_effect_coercion(&this_definition, &this_test);
		}
	}

	return 0;
}

int card_lifegift(int player, int card, event_t event){
/* CARD_ID_LIFEGIFT	7509
Lifegift	|2|G
Enchantment
Whenever a land enters the battlefield, you may gain 1 life. */

    if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && ! is_humiliated(player, card) ){
			etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_AI(player), TYPE_LAND, 0, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		    gain_life(player, 1);
	}

	return global_enchantment(player, card, event);
}

int card_lifespinner(int player, int card, event_t event){
/*
Lifespinner |3|G
Creature — Spirit 3/3
{T}, Sacrifice three Spirits: Search your library for a legendary Spirit permanent card and put it onto the battlefield.
	Then shuffle your library.
*/

	if( event == EVENT_CAN_ACTIVATE ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a Spirit to sacrifice.");
		this_test.subtype = SUBTYPE_SPIRIT;
		this_test.qty = 3;
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			return new_can_sacrifice_as_cost(player, card, &this_test);
		}
	}

	if(event == EVENT_ACTIVATE ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a Spirit to sacrifice.");
		this_test.subtype = SUBTYPE_SPIRIT;
		this_test.qty = 1;

		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int sacrifices[3] = {-1};
			int sc = 0;

			int i;
			for(i=0; i<3; i++){
				sacrifices[i] = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_RETURN_CHOICE | SAC_AS_COST, &this_test);
				if( sacrifices[i] == -1 ){
					break;
				}
				sc++;
			}
			for(i=0; i<3; i++){
				if( sacrifices[i] != -1 ){
					state_untargettable(BYTE2(sacrifices[i]), BYTE3(sacrifices[i]), 0);
					if( sc == 3 ){
						kill_card(BYTE2(sacrifices[i]), BYTE3(sacrifices[i]), KILL_SACRIFICE);
					}
				}
			}
			if( sc != 3 ){
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a legendary Spirit permanent card.");
		this_test.subtype = SUBTYPE_SPIRIT;
		this_test.sub2 = SUBTYPE_LEGEND;
		this_test.subtype_flag = F2_MULTISUBTYPE_ALL;

		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
	}

	return 0;
}

int card_loam_dweller(int player, int card, event_t event){
/* CARD_ID_LOAM_DWELLER	7511
Loam Dweller	|1|G
Creature - Spirit 2/2
Whenever you cast a Spirit or Arcane spell, you may put a land card from your hand onto the battlefield tapped. */

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a basic land card.");
		this_test.subtype = SUBTYPE_BASIC;

		new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY_TAPPED, 0, AI_FIRST_FOUND, &this_test);
	}

	return 0;
}

int card_mannichi_the_fevered_dream(int player, int card, event_t event)
{
/* CARD_ID_MANNICHI_THE_FEVERED_DREAM	7512
Mannichi, the Fevered Dream	|2|R
Legendary Creature - Spirit 1/2
|1|R: Switch each creature's power and toughness until end of turn. */

  check_legend_rule(player, card, event);

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  int p, c;
	  char marked[2][151] = {{0}};
	  for (p = 0; p <= 1; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if (in_play(p, c) && is_what(p, c, TYPE_CREATURE))
			marked[p][c] = 1;

	  for (p = 0; p <= 1; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if (marked[p][c])
			switch_power_and_toughness_until_eot(instance->parent_controller, instance->parent_card, p, c);
	}

  return generic_activated_ability(player, card, event, 0, MANACOST_XR(1,1), 0, NULL, NULL);
}

int card_mark_of_the_oni(int player, int card, event_t event){

	/* Mark of the Oni	|2|B
	 * Enchantment - Aura
	 * Enchant creature
	 * You control enchanted creature.
	 * At the beginning of the end step, if you control no Demons, sacrifice ~. */

	if( eot_trigger(player, card, event) && ! check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_DEMON) ){
		kill_card(player, card, KILL_SACRIFICE);
	}

	return card_control_magic(player, card, event);
}

int card_mark_of_sakiko(int player, int card, event_t event)
{
  /* Mark of Sakiko	|1|G	0x200dbd0
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has "Whenever this creature deals combat damage to a player, add that much |G to your mana pool. Until end of turn, this mana doesn't empty from your mana pool as steps and phases end." */

  if (attached_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_REPORT_DAMAGE_DEALT))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int amt = instance->targets[16].player;
	  instance->targets[16].player = 0;
	  produce_mana(player, COLOR_GREEN, amt);
	  mana_doesnt_drain_until_eot(player, COLOR_GREEN, amt);
	}

  return vanilla_aura(player, card, event, player);
}

int card_matsu_tribe_sniper(int player, int card, event_t event){
/* CARD_ID_MATSU_TRIBE_SNIPER	7515
Matsu-Tribe Sniper	|1|G
Creature - Snake Warrior Archer 1/1
|T: ~ deals 1 damage to target creature with flying.
Whenever ~ deals damage to a creature, tap that creature and it doesn't untap during its controller's next untap step. */

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_abilities = KEYWORD_FLYING;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, instance->parent_card);
			}
		}

		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED | GAA_LITERAL_PROMPT, MANACOST0, 0,
										&td, "Select target creature with flying.");
	}

	return freeze_when_damage(player, card, event);
}

int card_mirror_gallery(int player, int card, event_t event){
/* CARD_ID_MIRROR_GALLERY	7519
Mirror Gallery	|5
Artifact
The "legend rule" doesn't apply. */

	if( leaves_play(player, card, event) ){
		int i;
		for(i=0; i<2; i++){
				int count = active_cards_count[i]-1;
				while( count > -1 ){
						if( in_play(i, count) && is_legendary(i, count) ){
							verify_legend_rule(i, count, get_id(i, count));
						}
						count--;
				}
		}
	}

	return 0;
}

int card_mistblade_shinobi(int player, int card, event_t event){
/* CARD_ID_MISTBLADE_SHINOBI	7520
Mistblade Shinobi	|2|U
Creature - Human Ninja 1/1
Ninjutsu |U (|U, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand tapped and attacking.)
Whenever ~ deals combat damage to a player, you may return target creature that player controls to its owner's hand. */

	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE))
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		int i;
		for(i=0; i<2; i++){
			if( times_damaged[i] ){
				target_definition_t td;
				default_target_definition(player, card, &td, TYPE_CREATURE );
				td.allowed_controller = td.preferred_controller = i;
				td.allow_cancel = 2; // "Done" button
				if( can_target(&td) ){
					int done = 0;
					if( i == player ){
						done = new_pick_target(&td, "Select target creature you control.", 0, GS_LITERAL_PROMPT);
					}
					else{
						done = new_pick_target(&td, "Select target creature you opponent controls.", 0, GS_LITERAL_PROMPT);
					}
					if( done ){
						action_on_target(player, card, 0, ACT_BOUNCE);
					}
				}
			}
			instance->number_of_targets = 0;
		}
	}

	return ninjutsu(player, card, event, MANACOST_U(1), CARD_ID_MISTBLADE_SHINOBI);
}

int card_moonlit_strider(int player, int card, event_t event){
/* CARD_ID_MOONLIT_STRIDER	7521
Moonlit Strider	|3|W
Creature - Spirit 1/4
Sacrifice ~: Target creature you control gains protection from the color of your choice until end of turn.
Soulshift 3 (When this creature dies, you may return target Spirit card with converted mana cost 3 or less from your graveyard to your hand.)*/

	if( ! IS_GAA_EVENT(event) ){
		return soulshift(player, card, event, 3, 1);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = td.allowed_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME | GAA_LITERAL_PROMPT, MANACOST0, 0,
										&td, "Select target creature you control.");
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
								0, 0, select_a_protection(player) | KEYWORD_RECALC_SET_COLOR, 0);
	}

	return 0;
}

int neko_te_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);

	if( instance->damage_target_player > -1 ){
		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);
		if( leaves_play(instance->targets[1].player, instance->targets[1].card, event) ){
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_neko_te(int player, int card, event_t event)
{
  /* Neko-Te	|3
   * Artifact - Equipment
   * Whenever equipped creature deals damage to a creature, tap that creature. That creature doesn't untap during its controller's untap step for as long as ~ remains on the battlefield.
   * Whenever equipped creature deals damage to a player, that player loses 1 life.
   * Equip |2 */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_DEAL_DAMAGE && is_equipping(player, card)){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_source_card == instance->targets[8].card && damage->damage_source_player == instance->targets[8].player ){
				int good = 0;
				if( damage->info_slot > 0 ){
					good = 1;
				}
				else{
					card_instance_t *trg = get_card_instance(damage->damage_source_player, damage->damage_source_card);
					if( trg->targets[16].player > 0 ){
						good = 1;
					}
				}

				if( good == 1 && instance->info_slot < 8){
					if( instance->info_slot < 0 ){
						instance->info_slot = 0;
					}
					instance->targets[instance->info_slot].player = damage->damage_target_player;
					instance->targets[instance->info_slot].card = damage->damage_target_card;
					instance->info_slot++;
				}
			}
		}
	}

	if( instance->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
		reason_for_trigger_controller == player
	  ){
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				int i;
				for(i=0;i<instance->info_slot;i++){
					if( instance->targets[i].card == -1 ){
						lose_life(instance->targets[i].player, 1);
					}
					else{
						if( in_play(instance->targets[i].player, instance->targets[i].card) ){
							tap_card(instance->targets[i].player, instance->targets[i].card);
							int legacy = create_targetted_legacy_effect(player, card, &neko_te_legacy, instance->targets[i].player, instance->targets[i].card);
							card_instance_t *leg = get_card_instance(player, legacy);
							leg->targets[1].player = player;
							leg->targets[1].card = card;
						}
					}
				}
				instance->info_slot = 0;
		}
	}

	return basic_equipment(player, card, event, 2);
}

int card_ninja_of_the_deep_hours(int player, int card, event_t event){
/* CARD_ID_NINJA_OF_THE_DEEP_HOURS	7524
Ninja of the Deep Hours	|3|U
Creature - Human Ninja 2/2
Ninjutsu |1|U (|1|U, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand tapped and attacking.)
Whenever ~ deals combat damage to a player, you may draw a card. */

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_TRIGGER_OPTIONAL | DDBM_MUST_DAMAGE_PLAYER) ){
		draw_cards(player, 1);
	}

	return ninjutsu(player, card, event, MANACOST_XU(1, 1), CARD_ID_NINJA_OF_THE_DEEP_HOURS);
}

ARCANE(card_nourishing_shoal){
/* CARD_ID_NOURISHING_SHOAL	7525
Nourishing Shoal	|X|G|G
Instant - Arcane
You may exile a |Sgreen card with converted mana cost X from your hand rather than pay ~'s mana cost.
You gain X life. */

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, instance->info_slot);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select a Green card to exile");
	test.color = COLOR_TEST_GREEN;
	return shoal(player, card, event, &test, NULL, NULL, NULL, NULL);
}

int card_ogre_marauder(int player, int card, event_t event){
	// Whenever ~ attacks, it gains "~ is unblockable" until end of turn unless defending player sacrifices a creature.

	if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
		int choice = 1;
		if( can_sacrifice(1-player, player, 1, TYPE_CREATURE, 0) ){
			choice = DIALOG(player, card, EVENT_ACTIVATE,
							DLG_WHO_CHOOSES(1-player), DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_RANDOM,
							"Ogre Marauder is unblockable", 1, life[1-player] < get_attack_power(player, card) ? 1 : 6,
							"Sacrifice a creature", 1, 3);
		}

		if (choice == 1){
			pump_ability_until_eot(player, card, player, card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
		}
		else{
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature to sacrifice.");
			new_sacrifice(player, card, 1-player, SAC_NO_CANCEL | SAC_CAUSED, &this_test);
		}
	}

	return 0;
}

int card_okiba_gang_shinobi(int player, int card, event_t event){
/* CARD_ID_OKIBA_GANG_SHINOBI	7528
Okiba-Gang Shinobi	|3|B|B
Creature - Rat Ninja 3/2
Ninjutsu |3|B (|3|B, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand tapped and attacking.)
Whenever ~ deals combat damage to a player, that player discards two cards. */

	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE))
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		int i;
		for(i=0; i<2; i++){
			if( times_damaged[i] ){
				new_multidiscard(i, 2, 0, player);
			}
		}
	}

	return ninjutsu(player, card, event, MANACOST_XB(3, 1), CARD_ID_OKIBA_GANG_SHINOBI);
}

static int opal_eye_card = -1;
static const char* is_damaging_opal_eye(int who_chooses, int player, int card){
	card_instance_t *instance = get_card_instance(player, card);
	if( is_what(player, card, TYPE_EFFECT) && instance->internal_card_id == damage_card ){
		if( instance->damage_target_player == who_chooses && instance->damage_target_card == opal_eye_card ){
			return NULL;
		}
	}
	return "must be damage to Opal Eye, Konda's Jojimbo";
}

int card_opal_eye_kondas_yojimbo(int player, int card, event_t event){
/* CARD_ID_OPAL_EYE_KONDAS_YOJIMBO	7529
Opal-Eye, Konda's Yojimbo	|1|W|W
Legendary Creature - Fox Samurai 1/4
Defender (This creature can't attack.)
Bushido 1 (Whenever this creature blocks or becomes blocked, it gets +1/+1 until end of turn.)
|T: The next time a source of your choice would deal damage this turn, that damage is dealt to ~ instead.
|1|W: Prevent the next 1 damage that would be dealt to ~ this turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ANY);
	td.extra = damage_card;

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.extra = (int32_t)is_damaging_opal_eye;
	td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	opal_eye_card = card;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_DAMAGE");
		if( ! result ){
			result = generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST_XW(1, 1), 0, &td1, "TARGET_DAMAGE");
		}
		return result;
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int choice = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_DAMAGE") ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST_XW(1, 1), 0, &td1, "TARGET_DAMAGE") ){
				choice = do_dialog(player, player, card, -1, -1, " Redirect damage to Opal Eye\n Prevent damage to Opal-Eye\n Cancel", 1);
			}
		}
		else{
			choice = 1;
		}

		if( choice == 0 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST0) && pick_target(&td, "TARGET_DAMAGE") ){
				instance->info_slot = 66;
				tap_card(player, card);
			}
		}

		else if( choice == 1 ){
				if( charge_mana_for_activated_ability(player, card, MANACOST_XW(1, 1)) && pick_target(&td1, "TARGET_DAMAGE") ){
					instance->info_slot = 67;
				}
		}
		else{
			spell_fizzled = 1;
		}

	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 ){
			if( valid_target(&td) ){
				card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
				target->damage_target_player = instance->parent_controller;
				target->damage_target_card = instance->parent_card;
			}
		}
		if( instance->info_slot == 67 ){
			if( valid_target(&td) ){
				card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
				prevent_damage(target->targets[0].player, target->targets[0].card, 1);
			}
		}
	}

	return bushido(player, card, event, 1);
}

int card_orb_of_dreams(int player, int card, event_t event){

	/* Orb of Dreams	|3
	 * Artifact
	 * Permanents enter the battlefield tapped. */

	permanents_enters_battlefield_tapped(player, card, event, ANYBODY, TYPE_PERMANENT, NULL);

	return 0;
}

int card_oyobi_who_splits_the_heavens(int player, int card, event_t event){
	/* Oyobi, Who Split the Heavens	|6|W
	 * Legendary Creature - Spirit 3/6
	 * Flying
	 * Whenever you cast a Spirit or Arcane spell, put a 3/3 |Swhite Spirit creature token with flying onto the battlefield. */

	check_legend_rule(player, card, event);

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SPIRIT, &token);
		token.pow = 3;
		token.tou = 3;
		token.key_plus = KEYWORD_FLYING;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		generate_token(&token);
	}

	return 0;
}

int card_patron_of_the_akki(int player, int card, event_t event){

	check_legend_rule(player, card, event);

	// Whenever ~ attacks, creatures you control get +2/+0 until end of turn.
	if (declare_attackers_trigger(player, card, event, 0, player, card)){
		pump_creatures_until_eot(player, card, player, 0, 2,0, 0,0, NULL);
	}

	// Goblin offering
	return patron(player, card, event, SUBTYPE_GOBLIN);
}

int card_patron_of_the_kitsune(int player, int card, event_t event){

	check_legend_rule(player, card, event);

	// Whenever a creature attacks, you may gain 1 life.
	if (declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player) | DAT_SEPARATE_TRIGGERS, 2, -1)){
		gain_life(player, 1);
	}

	// Fox offering
	return patron(player, card, event, SUBTYPE_FOX);
}

int card_patron_of_the_moon(int player, int card, event_t event){
/* CARD_ID_PATRON_OF_THE_MOON	7536
Patron of the Moon	|5|U|U
Legendary Creature - Spirit 5/4
Moonfolk offering (You may cast this card any time you could cast an instant by sacrificing a Moonfolk and paying the difference in mana costs between this and the sacrificed Moonfolk. Mana cost includes color.)
Flying
|1: Put up to two land cards from your hand onto the battlefield tapped. */

	if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a land card.");
		this_test.qty = 2;

		tutor_multiple_card_from_hand(player, player, player, TUTOR_PLAY_TAPPED, 0, AI_MAX_VALUE, &this_test);
	}

	return patron(player, card, event, SUBTYPE_MOONFOLK);
}

static int patron_of_the_nezumi_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_PATRON_OF_THE_NEZUMI){
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
					}
					if( instance->targets[11].player < 0 ){
						instance->targets[11].player = 0;
					}
					SET_BYTE1(instance->targets[11].player) = 1;
				}
				count_for_gfp_ability(player, card, event, 1-player, TYPE_PERMANENT, 0);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			lose_life(1-player, amount);
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

int card_patron_of_the_nezumi(int player, int card, event_t event){
/* CARD_ID_PATRON_OF_THE_NEZUMI	7537
Patron of the Nezumi	|5|B|B
Legendary Creature - Spirit 6/6
Rat offering (You may cast this card any time you could cast an instant by sacrificing a Rat and paying the difference in mana costs between this and the sacrificed Rat. Mana cost includes color.)
Whenever a permanent is put into an opponent's graveyard, that player loses 1 life. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)patron_of_the_nezumi_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &patron_of_the_nezumi_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}


	check_legend_rule(player, card, event);

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, 1-player, TYPE_PERMANENT, 0);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			lose_life(1-player, amount);
		}
	}

	return patron(player, card, event, SUBTYPE_RAT);
}

int card_patron_of_the_orochi(int player, int card, event_t event){
/* CARD_ID_PATRON_OF_THE_OROCHI	7538
Patron of the Orochi	|6|G|G
Legendary Creature - Spirit 7/7
Snake offering (You may cast this card any time you could cast an instant by sacrificing a Snake and paying the difference in mana costs between this and the sacrificed Snake. Mana cost includes color.)
|T: Untap all |H1Forests and all |Sgreen creatures. Activate this ability only once each turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		if( event == EVENT_RESOLVE_ACTIVATION ){
			int i, end = active_cards_count[player], clr = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
			int marked[151] = {0};
			for (i = 0; i < end; ++i){
				if (in_play(player, i) && is_tapped(player, i) &&
					(has_subtype(player, i, SUBTYPE_FOREST) || (is_what(player, i, TYPE_CREATURE) && (get_color(player, i) & clr))) )
				{
					marked[i] = 1;
				}
			}
			for (i = 0; i < end; ++i){
				if (marked[i]){
					untap_card(player, i);
				}
			}
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_ONCE_PER_TURN, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	return patron(player, card, event, SUBTYPE_SNAKE);
}

int card_petalmane_baku(int player, int card, event_t event){

	/* Whenever you cast a Spirit or Arcane spell, you may put a ki counter on ~.
	 * |1, Remove X ki counters from ~: Add X mana of any one color to your mana pool. */

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_DUH) ){
		add_counter(player, card, COUNTER_KI);
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( can_produce_mana(player, card) && count_counters(player, card, COUNTER_KI) > 1 &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(1), 0, NULL, NULL) )
		{
			declare_mana_available(player, COLOR_ANY, count_counters(player, card, COUNTER_KI)-1);
		}
	}

	if( event == EVENT_CAN_ACTIVATE && can_produce_mana(player, card) && count_counters(player, card, COUNTER_KI) > 0){
		return generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		charge_mana_for_activated_ability(player, card, MANACOST_X(1));
		if( spell_fizzled != 1 ){
			int amount = count_counters(player, card, COUNTER_KI);
			if( !IS_AI(player) ){
				amount = choose_a_number(player, "Remove how many counters?", count_counters(player, card, COUNTER_KI));
				if( amount == 0 || amount > count_counters(player, card, COUNTER_KI) ){
					spell_fizzled = 1;
				}
			}
			remove_counters(player, card, COUNTER_KI, amount);
			produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, amount);
			tapped_for_mana_color = -2;	// mana ability, so can't respond (but not "tapping for mana", or even tapping)
		}
	}

	return 0;
}

int card_reduce_to_dreams(int player, int card, event_t event){

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;

		default_test_definition(&this_test, TYPE_ENCHANTMENT);
		APNAP(p, {new_manipulate_all(player, card, p, &this_test, ACT_BOUNCE);};);

		default_test_definition(&this_test, TYPE_LAND);
		this_test.type_flag = DOESNT_MATCH;
		APNAP(p, {new_manipulate_all(player, card, p, &this_test, ACT_BOUNCE);};);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_ronin_warclub(int player, int card, event_t event){

	etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		equip_target_creature(player, card, trigger_cause_controller, trigger_cause);
	}

	return vanilla_equipment(player, card, event, 5, 2, 1, 0, 0);
}

int card_sakiko_mother_of_summer(int player, int card, event_t event){

	check_legend_rule(player, card, event);

	card_instance_t* damage = combat_damage_being_dealt(event);
	if (damage &&
		damage->damage_source_player == player &&
		(damage->targets[3].player & TYPE_CREATURE) &&	// probably redundant to combat damage check
		damage->damage_target_card == -1 && !check_special_flags(damage->damage_source_player, damage->damage_source_card, SF_ATTACKING_PWALKER)
	   ){
		get_card_instance(player, card)->info_slot += damage->targets[16].player;
	}

	if( trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player ){
		card_instance_t* instance = get_card_instance(player, card);
		if (instance->info_slot <= 0){
			return 0;
		}
		if(event == EVENT_TRIGGER){
			event_result |= 2;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				produce_mana(player, COLOR_GREEN, instance->info_slot);
				mana_doesnt_drain_until_eot(player, COLOR_GREEN, instance->info_slot);
				instance->info_slot = 0;
		}
	}
	return 0;
}

int card_sakura_tribe_springcaller(int player, int card, event_t event)
{
  /* Sakura-Tribe Springcaller	|3|G	0x200dbd5
   * Creature - Snake Shaman 2/4
   * At the beginning of your upkeep, add |G to your mana pool. Until end of turn, this mana doesn't empty from your mana pool as steps and phases end. */

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  produce_mana(player, COLOR_GREEN, 1);
	  mana_doesnt_drain_until_eot(player, COLOR_GREEN, 1);
	}

  return 0;
}

int card_scourge_of_numai(int player, int card, event_t event){
/* CARD_ID_SCOURGE_OF_NUMAI	7551
Scourge of Numai	|3|B
Creature - Demon Spirit 4/4
At the beginning of your upkeep, you lose 2 life if you don't control an Ogre. */

	if( !is_humiliated(player, card) && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && current_turn == player
	      && reason_for_trigger_controller == player ){
		int count = count_upkeeps(player);
		if(event == EVENT_TRIGGER && count > 0 && ! check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_OGRE) ){
			event_result |= 2;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				lose_life(player, 2*count);
		}
	}

	return 0;
}

static int redirect_damage_legacy(int player, int card, event_t event){

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
		if( damage->info_slot > 0 && damage->internal_card_id == damage_card ){
			if( damage->damage_source_player == instance->targets[0].player && damage->damage_source_card == instance->targets[0].card ){
				if( ! check_state(affected_card_controller, affected_card, STATE_CANNOT_TARGET) ){
					damage->damage_target_player = instance->targets[1].player;
					damage->damage_target_card = instance->targets[1].card;
					instance->counter_toughness -= damage->info_slot;
					if( ! instance->counter_toughness ){
						kill_card(player, card, KILL_EXILE);
					}
				}
			}
		}
	}

	if( event == EVENT_CLEANUP && get_card_instance(player, card)->targets[1].player > -1 ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

ARCANE(card_shining_shoal){
	/* Shining Shoal	|X|W|W	0x20062b3
	 * Instant - Arcane
	 * You may exile a |Swhite card with converted mana cost X from your hand rather than pay this spell's mana cost.
	 * The next X damage that a source of your choice would deal to you and/or creatures you control this turn is dealt to any target instead. */
	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td2) ){
		return generic_spell(player, card, event, GS_DAMAGE_PREVENTION | GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) && validate_target(player, card, &td2, 1) ){
			int max = instance->info_slot;
			card_instance_t *damage = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			damage->damage_target_player = instance->targets[1].player;
			damage->damage_target_card = instance->targets[1].card;
			max -= instance->info_slot;
			if( max ){
				int legacy = create_legacy_effect(player, card, &redirect_damage_legacy);
				card_instance_t *leg = get_card_instance(player, legacy);
				leg->targets[0].player = damage->damage_source_player;
				leg->targets[0].card = damage->damage_source_card;
				leg->targets[1] = instance->targets[1];
				leg->targets[2].player = 1;
				leg->number_of_targets = 2;
				leg->counter_toughness = max;
			}
		}
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select a White card to exile");
	test.color = COLOR_TEST_WHITE;
	return shoal(player, card, event, &test, &td, "TARGET_DAMAGE", &td2, "TARGET_ANY");
}

int card_shirei_shizos_caretaker(int player, int card, event_t event){
/*
Shirei, Shizo's Caretaker |4|B	0x200f516
Legendary Creature — Spirit 2/2
Whenever a creature with power 1 or less is put into your graveyard from the battlefield, you may return that card to the battlefield
	at the beginning of the next end step if Shirei, Shizo's Caretaker is still on the battlefield.
*/
	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.power = 2;
		this_test.power_flag = F5_POWER_LESSER_THAN_VALUE;

		if( get_owner(affected_card_controller, affected_card) == player ){
			count_for_gfp_ability_and_store_values(player, card, event, player, 0, &this_test, GFPC_TRACK_DEAD_CREATURES, 0);
		}
	}

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		card_instance_t *instance = get_card_instance(player, card);
		int dead[20];
		int dc = 0;
		int i;
		for(i=0; i<10; i++){
			if( instance->targets[i].player > -1 ){
				dead[dc] = instance->targets[i].player;
				dc++;
			}
			else{
				break;
			}
			if( instance->targets[i].card > -1 ){
				dead[dc] = instance->targets[i].card;
				dc++;
			}
			else{
				break;
			}
		}
		int to_reanimate[2][16];
		int trc = 0;
		for(i=0; i<dc; i++){
			char buffer[100];
			scnprintf(buffer, 100, "Return %s to play at eot", cards_ptr[cards_data[dead[i]].id]->name);
			int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_SMALLCARD_ID(dead[i]),
								buffer, 1, 5,
								"Decline", 1, 1);
			if( choice == 1 ){
				to_reanimate[0][trc] = player;
				to_reanimate[1][trc] = cards_data[dead[i]].id;
				trc++;
				if( trc == 16 ){
					break;
				}
			}
		}
		if( trc ){
			int legacy = create_targetted_legacy_effect(player, card, &reanimate_csvids_at_eot_legacy, player, card);
			card_instance_t* leg = get_card_instance(player, legacy);
			leg->targets[1].player = REANIMATE_DEFAULT;
			for(i=0; i<trc; i++){
				instance->targets[i+2].player = to_reanimate[0][i];
				instance->targets[i+2].card = to_reanimate[1][i];
			}
		}
	}

	return 0;
}

int card_shizuko_caller_of_autumn(int player, int card, event_t event)
{
  /* Shizuko, Caller of Autumn	|1|G|G	0x200dbda
   * Legendary Creature - Snake Shaman 2/3
   * At the beginning of each player's upkeep, that player adds |G|G|G to his or her mana pool. Until end of turn, this mana doesn't empty from that player's mana pool as steps and phases end. */

  check_legend_rule(player, card, event);

  upkeep_trigger_ability(player, card, event, ANYBODY);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  produce_mana(current_turn, COLOR_GREEN, 3);
	  mana_doesnt_drain_until_eot(current_turn, COLOR_GREEN, 3);
	}

  return 0;
}

int card_shuko(int player, int card, event_t event){
/* CARD_ID_SHUKO	7557
Shuko	|1
Artifact - Equipment
Equipped creature gets +1/+0.
Equip |0 (|0: Attach to target creature you control. Equip only as a sorcery.) */

	return vanilla_equipment(player, card, event, 0, 1, 0, 0, 0);
}

ARCANE(card_sickening_shoal){
/* CARD_ID_SICKENING_SHOAL	7559
Sickening Shoal	|X|B|B
Instant - Arcane
You may exile a |Sblack card with converted mana cost X from your hand rather than pay ~'s mana cost.
Target creature gets -X/-X until end of turn. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -instance->info_slot, -instance->info_slot);
		}
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select a Black card to exile");
	test.color = COLOR_TEST_BLACK;
	return shoal(player, card, event, &test, &td, "TARGET_CREATURE", NULL, NULL);
}

int card_skullsnatcher(int player, int card, event_t event){
/* CARD_ID_SKULLSNATCHER	7562
Skullsnatcher	|1|B
Creature - Rat Ninja 2/1
Ninjutsu |B (|B, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand tapped and attacking.)
Whenever ~ deals combat damage to a player, exile up to two target cards from that player's graveyard. */

	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE))
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		int i;
		for(i=0; i<2; i++){
			if( times_damaged[i] ){
				if( count_graveyard_by_type(i, TYPE_CREATURE) > 0 && ! graveyard_has_shroud(i) ){
					if( player == HUMAN || (player == AI && i != player) ){
						test_definition_t this_test;
						new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");
						this_test.qty = MIN(2, count_graveyard(i));
						new_global_tutor(player, i, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MAX_VALUE, &this_test);
					}
				}
			}
		}
	}

	return ninjutsu(player, card, event, MANACOST_B(1), CARD_ID_SKULLSNATCHER);
}

int card_slumbering_tora(int player, int card, event_t event){
/* CARD_ID_SLUMBERING_TORA	7563
Slumbering Tora	|3
Artifact
|2, Discard a Spirit or Arcane card: ~ becomes an X/X Cat artifact creature until end of turn, where X is the discarded card's converted mana cost.*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select an Arcane or Spirit card.");
	this_test.subtype = SUBTYPE_ARCANE;
	this_test.sub2 = SUBTYPE_SPIRIT;
	this_test.subtype_flag = F2_MULTISUBTYPE;
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_X(2), 0, NULL, NULL) ){
			return check_battlefield_for_special_card(player, card, player, 0, &this_test);
		}
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
			if( selected != -1 ){
				instance->info_slot = get_cmc(player, selected);
				discard_card(player, selected);
				set_special_flags(player, card, SF_TYPE_ALREADY_CHANGED);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		artifact_animation(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 1,
							instance->info_slot, instance->info_slot, 0, 0 );
	}

	return 0;
}

int card_sosukes_summons(int player, int card, event_t event){
/* Sosuke's Summons	|2|G
 * Sorcery
 * Put two 1/1 |Sgreen Snake creature tokens onto the battlefield.
 * Whenever a nontoken Snake enters the battlefield under your control, you may return ~ from your graveyard to your hand. */

	 if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SNAKE, &token);
		token.pow = token.tou = 1;
		token.qty = 2;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_stir_the_grave(int player, int card, event_t event){
/*
Stir the Grave |X|B
Sorcery
Return target creature card with converted mana cost X or less from your graveyard to the battlefield.
*/

	if( event == EVENT_CAN_CAST ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		if( generic_spell(player, card, event, GS_GRAVE_RECYCLER | GS_X_SPELL, NULL, NULL, 1, &test) ){
			int count = count_graveyard(player);
			if( count > 0 ){
				const int *grave = get_grave(player);
				while( count > -1 ){
						if( is_what(-1, grave[count], TYPE_CREATURE) &&
							has_mana(player, COLOR_COLORLESS, get_cmc_by_id(cards_data[grave[count]].id)) )
						{
							return 1;
						}
						count--;
				}
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			if( ! played_for_free(player, card) && ! is_token(player, card) ){
				if( player != AI ){
					charge_mana(player, COLOR_COLORLESS, -1);
					if( spell_fizzled != 1 ){
						set_x_for_x_spells(player, card, event, x_value);
					}
				}
				else{
					int selected = -1;
					int max_cmc = -1;
					int count = count_graveyard(player);
					if( count > 0 ){
						const int *grave = get_grave(player);
						while( count > -1 ){
								int cmc = get_cmc_by_id(cards_data[grave[count]].id);
								if( is_what(-1, grave[count], TYPE_CREATURE) && has_mana(player, COLOR_COLORLESS, cmc) ){
									if( cmc > max_cmc ){
										max_cmc = cmc;
										selected = count;
									}
								}
								count--;
						}
					}
					if( selected > -1 ){
						charge_mana(player, COLOR_COLORLESS, max_cmc);
						if( spell_fizzled != -1 ){
							set_x_for_x_spells(player, card, event, max_cmc);
						}
					}
					else{
						spell_fizzled = 1;
					}
				}
			}
		}
		test.cmc = instance->info_slot + 1;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		scnprintf(test.message, 100, "Select target creature card with CMC %d or less.", instance->info_slot);
		if( select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &test, 0) == -1 ){
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_sway_of_the_stars(int player, int card, event_t event){
/* CARD_ID_SWAY_OF_THE_STARS	7569
Sway of the Stars	|8|U|U
Sorcery
Each player shuffles his or her hand, graveyard, and permanents he or she owns into his or her library, then draws seven cards. Each player's life total becomes 7.*/

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;

		default_test_definition(&this_test, TYPE_ENCHANTMENT);
		APNAP(p, {new_manipulate_all(player, card, p, &this_test, ACT_BOUNCE);};);

		default_test_definition(&this_test, TYPE_LAND);
		this_test.type_flag = DOESNT_MATCH;
		APNAP(p, {new_manipulate_all(player, card, p, &this_test, ACT_BOUNCE);};);

		APNAP(p,{
					draw_cards(p, 7);
					set_life_total(p, 7);
				};
		);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_takenuma_bleeder(int player, int card, event_t event)
{
/* CARD_ID_TAKENUMA_BLEEDER	7571
Takenuma Bleeder	|2|B
Creature - Ogre Shaman 3/3
Whenever ~ attacks or blocks, you lose 1 life if you don't control a Demon. */

  if ((xtrigger_condition() == XTRIGGER_ATTACKING || event == EVENT_DECLARE_ATTACKERS || event == EVENT_DECLARE_BLOCKERS)
	  && !check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_DEMON)
	  && (declare_attackers_trigger(player, card, event, 0, player, card)
		  || (blocking(player, card, event) && !is_humiliated(player, card))))
	lose_life(player, 1);

  return 0;
}

int card_tallowisp(int player, int card, event_t event){

	/* Tallowisp	|1|W
	 * Creature - Spirit 1/3
	 * Whenever you cast a Spirit or Arcane spell, you may search your library for an Aura card with enchant creature, reveal it, and put it into your hand. If
	 * you do, shuffle your library. */

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ENCHANTMENT, "Select an Aura card with enchant creature.");
		test.subtype = SUBTYPE_AURA_CREATURE;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
	}

	return 0;
}

int card_tendo_ice_bridge(int player, int card, event_t event){

	/* ~ enters the battlefield with a charge counter on it.
	 * |T: Add |1 to your mana pool.
	 * |T, Remove a charge counter from ~: Add one mana of any color to your mana pool. */

	card_instance_t *instance = get_card_instance(player, card);

	enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, 1);

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect_force_color(player, card, COLOR_TEST_ANY_COLORED);

		instance->info_slot = COLOR_TEST_ANY;
		instance->targets[1].player = 66;
		return 0;	// so mana_producer() doesn't play the colorless sound
	}

	if( count_counters(player, card, COUNTER_CHARGE) <= 0 && instance->targets[1].player == 66 ){
		instance->info_slot = COLOR_TEST_COLORLESS;
	}

	int colored_mana_before = 0;
	if( event == EVENT_ACTIVATE ){
		int i;
		for(i=COLOR_BLACK; i<=COLOR_WHITE;i++){
			colored_mana_before+=mana_pool[player][i];
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int colored_mana_after = 0;
		int i;
		for(i=COLOR_BLACK; i<=COLOR_WHITE;i++){
			colored_mana_after+=mana_pool[player][i];
		}
		if( colored_mana_before < colored_mana_after ){
			remove_counter(instance->parent_controller, instance->parent_card, COUNTER_CHARGE);
		}
	}

  return mana_producer(player, card, event);
}

ARCANE(card_terashis_grasp){
/* CARD_ID_TERASHIS_GRASP	7575
Terashi's Grasp	|2|W
Sorcery - Arcane
Destroy target artifact or enchantment. You gain life equal to its converted mana cost. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		pick_target(&td, "DISENCHANT");
	}

	else if(event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				card_instance_t *instance = get_card_instance(player, card);
				int amount = get_cmc(instance->targets[0].player, instance->targets[0].card);
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				gain_life(player, amount);
			}
	}
	return 0;
}

int card_that_which_was_taken(int player, int card, event_t event)
{
  /* That Which Was Taken	|5
   * Legendary Artifact
   * |4, |T: Put a divinity counter on target permanent other than ~.
   * Each permanent with a divinity counter on it is indestructible. */

  check_legend_rule(player, card, event);

  if (event == EVENT_ABILITIES && is_what(affected_card_controller, affected_card, TYPE_PERMANENT)
	  && count_counters(affected_card_controller, affected_card, COUNTER_DIVINITY) > 0
	  && in_play(player, card) && !is_humiliated(player, card))
	indestructible(affected_card_controller, affected_card, event);

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_PERMANENT);
  td.preferred_controller = player;
  td.special = TARGET_SPECIAL_NOT_ME;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_DIVINITY);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(4), 0, &td, "TARGET_PERMANENT");
}

int card_threads_of_disloyalty(int player, int card, event_t event){
/* CARD_ID_THREADS_OF_DISLOYALTY	7578
Threads of Disloyalty	|1|U|U
Enchantment - Aura
Enchant creature with converted mana cost 2 or less
You control enchanted creature. */

	// Checking if the CMC varies becoming > 2 (EX: we've stolen a face-down card, which was later turned face-up)
	if( in_play(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_cmc(instance->damage_target_player, instance->damage_target_card) > 2 ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
	td.extra = 2;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_AURA | GS_LITERAL_PROMPT,
							&td, "Select a creature with CMC 2 or less", 1, NULL);
	}
	else{
		return generic_stealing_aura(player, card, event, &td, "TARGET_CREATURE");
	}

	return 0;
}

ARCANE(card_three_tragedies){
/* CARD_ID_THREE_TRAGEDIES	7579
Three Tragedies	|3|B|B
Sorcery - Arcane
Target player discards three cards. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		pick_target(&td, "TARGET_PLAYER");
	}

	else if(event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				card_instance_t *instance = get_card_instance(player, card);
				new_multidiscard(instance->targets[0].player, 3, 0, player);
			}
	}
	return 0;
}


int card_throat_slitter(int player, int card, event_t event){
/* CARD_ID_THROAT_SLITTER	7580
Throat Slitter	|4|B
Creature - Rat Ninja 2/2
Ninjutsu |2|B (|2|B, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand tapped and attacking.)
Whenever ~ deals combat damage to a player, destroy target non|Sblack creature that player controls. */

	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE))
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		int i;
		for(i=0; i<2; i++){
			if( times_damaged[i] ){
				target_definition_t td;
				default_target_definition(player, card, &td, TYPE_CREATURE );
				td.allowed_controller = td.preferred_controller = i;
				td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
				td.allow_cancel = 0;
				if( can_target(&td) ){
					int done = 0;
					if( i == player ){
						done = new_pick_target(&td, get_sleighted_color_text(player, card, "Select target non%s creature you control.", COLOR_BLACK),
												 0, GS_LITERAL_PROMPT);
					}
					else{
						done = new_pick_target(&td, get_sleighted_color_text(player, card, "Select target non%s creature your opponent controls.", COLOR_BLACK),
												 0, GS_LITERAL_PROMPT);
					}
					if( done ){
						action_on_target(player, card, 0, KILL_DESTROY);
					}
				}
			}
			instance->number_of_targets = 0;
		}
	}

	return ninjutsu(player, card, event, MANACOST_XB(2, 1), CARD_ID_THROAT_SLITTER);
}

int card_tomorrow_azamis_familiar(int player, int card, event_t event){

/* Tomorrow, Azami's Familiar	|5|U
 * Legendary Creature - Spirit 1/5
 * If you would draw a card, look at the top three cards of your library instead.
 * Put one of those cards into your hand and the rest on the bottom of your library in any order. */

	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && !suppress_draw ){
		if( reason_for_trigger_controller == player ){
			if(event == EVENT_TRIGGER){
				event_result |= 2u;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					int amount = count_deck(player) < 3 ? count_deck(player) : 3;
					if( amount ){
						test_definition_t this_test;
						default_test_definition(&this_test, TYPE_ANY);
						this_test.create_minideck = 3;
						this_test.no_shuffle = 1;
						new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 1, AI_MAX_VALUE, &this_test);
						amount--;
						if( amount > 0 ){
							put_top_x_on_bottom(player, player, amount);
						}
					}
					suppress_draw = 1;
			}
		}
	}
	return 0;
}

ARCANE_WITH_SPLICE(card_torrent_of_stone, MANACOST_X(-10))
{
/* CARD_ID_TORRENT_OF_STONE	7583
Torrent of Stone	|3|R
Instant - Arcane
~ deals 4 damage to target creature.
Splice onto Arcane-Sacrifice two |H1Mountains. (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost.
If you do, add this card's effects to that spell.) */

  // (No consideration for Magical Hack; by the time it can be cast, the splice cost's already been paid.)

#define DECL_TEST(test)																\
  test_definition_t test;															\
  new_default_test_definition(&test, TYPE_LAND, "Select a Mountain to sacrifice.");	\
  test.subtype = SUBTYPE_MOUNTAIN

#define DECL_TD(td)												\
  target_definition_t td;										\
  default_target_definition(player, card, &td, TYPE_CREATURE);	\
  td.allow_cancel = 0

  if (event == EVENT_CAN_SPLICE)
	{
	  DECL_TEST(test);
	  test.qty = 2;
	  return new_can_sacrifice_as_cost(player, card, &test);
	}

  if (event == EVENT_SPLICE)
	{
	  DECL_TEST(test);
	  return (new_sacrifice(player, card, player, 0, &test)
			  && new_sacrifice(player, card, player, SAC_NO_CANCEL, &test));
	}

  if (event == EVENT_CAN_CAST)
	{
	  DECL_TD(td);
	  return can_target(&td);
	}

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  DECL_TD(td);
	  pick_target(&td, "TARGET_CREATURE");
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  DECL_TD(td);
	  if (valid_target(&td))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
		}
	}

  return 0;
#undef DECL_TEST
#undef DECL_TD
}

int card_toshiro_umezawa(int player, int card, event_t event){
/*
Toshiro Umezawa |1|B|B
Legendary Creature — Human Samurai 2/2
Bushido 1 (Whenever this creature blocks or becomes blocked, it gets +1/+1 until end of turn.)
Whenever a creature an opponent controls dies, you may cast target instant card from your graveyard.
	If that card would be put into a graveyard this turn, exile it instead.
*/
	check_legend_rule(player, card, event);

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, 1-player, TYPE_CREATURE, NULL);
	}

	int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( amount && ! graveyard_has_shroud(player) ){
		int cont = 1;
		while( cont ){
				int playable[2][amount];
				int pc = 0;
				const int *grave = get_grave(player);
				int i = 0;
				while( grave[i] != -1 ){
						if( is_what(-1, grave[i], TYPE_INSTANT | TYPE_INTERRUPT) && can_play_iid(player, event, grave[i]) ){
							playable[0][pc] = grave[i];
							playable[1][pc] = i;
							pc++;
							if( pc == amount ){
								break;
							}
						}
						i++;
				}

				if( pc ){
					test_definition_t this_test;
					new_default_test_definition(&this_test, TYPE_ANY, "Select an Instant to play");

					int selected = select_card_from_zone(player, player, playable[0], pc, 0, AI_MAX_VALUE, -1, &this_test);
					if( selected != -1 ){
						if( charge_mana_from_id(player, card, event, cards_data[playable[0][selected]].id) ){
							play_card_in_grave_for_free_and_exile_it(player, player, playable[1][selected]);
							pc--;
							cont = pc > 0 ? 1 : 0;
						}
					}
					else{
						cont = 0;
					}
				}
		}
	}

	return bushido(player, card, event, 1);
}

int card_umezawas_jitte(int player, int card, event_t event)
{
  /* Umezawa's Jitte	|2
   * Legendary Artifact - Equipment
   * Whenever equipped creature deals combat damage, put two charge counters on ~.
   * Remove a charge counter from ~: Choose one - Equipped creature gets +2/+2 until end of turn; or target creature gets -1/-1 until end of turn; or you gain 2
   * life.
   * Equip |2 */

  if (!is_unlocked(player, card, event, 26))
	return 0;

  check_legend_rule(player, card, event);

  // test for if combat damage was dealt
  int packets;
  if ((packets = equipped_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE)))
	add_counters(player, card, COUNTER_CHARGE, 2 * packets);

  if (!IS_ACTIVATING(event))
	return 0;

  if (event == EVENT_CAN_ACTIVATE && !CAN_ACTIVATE0(player, card))
	return 0;

  card_instance_t* instance = get_card_instance(player, card);

  int counters = count_counters(player, card, COUNTER_CHARGE);

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  enum
  {
	CHOICE_EQUIP = 1,
	CHOICE_GROW = 2,
	CHOICE_SHRINK = 3,
	CHOICE_LIFE = 4,
  } choice = DIALOG(player, card, event,
					DLG_RANDOM,
					"Equip", can_activate_basic_equipment(player, card, event, 2), 1,
					"+2/+2", counters > 0 && is_equipping(player, card), 10,
					"-1/-1", counters > 0 && can_target(&td), 10,
					"Gain 2 life", counters > 0, 5);

  if (event == EVENT_CAN_ACTIVATE)
	return choice;
  else if (event == EVENT_ACTIVATE)
	{
	  instance->number_of_targets = 0;
	  switch (choice)
		{
		  case CHOICE_EQUIP:
			activate_basic_equipment(player, card, 2);
			if (player == AI && !is_equipping(player, card))
			  ai_modifier += 128;
			break;

		  case CHOICE_GROW:
		  case CHOICE_LIFE:
			if (charge_mana_for_activated_ability(player, card, MANACOST0))
			  remove_counter(player, card, COUNTER_CHARGE);
			break;

		  case CHOICE_SHRINK:
			if (charge_mana_for_activated_ability(player, card, MANACOST0) && pick_target(&td, "TARGET_CREATURE"))
			  remove_counter(player, card, COUNTER_CHARGE);
			break;
		}
	}
  else	// EVENT_RESOLVE_ACTIVATION
	switch (choice)
	  {
		case CHOICE_EQUIP:
		  resolve_activation_basic_equipment(player, card);
		  break;

		case CHOICE_GROW:
		  if (is_equipping(player, card))
			pump_until_eot(player, card, instance->targets[8].player, instance->targets[8].card, 2, 2);
		  break;

		case CHOICE_SHRINK:
		  if (valid_target(&td))
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -1, -1);
		  else
			spell_fizzled = 1;
		  break;

		case CHOICE_LIFE:
		  gain_life(player, 2);
		  break;
	  }

  return 0;
}

ARCANE(card_unchecked_growth){
/* CARD_ID_UNCHECKED_GROWTH	7588
Unchecked Growth	|2|G
Instant - Arcane
Target creature gets +4/+4 until end of turn. If it's a Spirit, it gains trample until end of turn. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		pick_target(&td, "TARGET_CREATURE");
	}

	else if(event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				card_instance_t *instance = get_card_instance(player, card);
				int key = 0;
				if( has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_SPIRIT) ){
					key = KEYWORD_TRAMPLE;
				}
				pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 4, 4, key, 0);
			}
	}
	return 0;
}

int card_waxmane_baku(int player, int card, event_t event){
/* CARD_ID_WAXMANE_BAKU	7594
Waxmane Baku	|2|W
Creature - Spirit 2/2
Whenever you cast a Spirit or Arcane spell, you may put a ki counter on ~.
|1, Remove X ki counters from ~: Tap X target creatures. */

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		add_counter(player, card, COUNTER_KI);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && count_counters(player, card, COUNTER_KI) > 0 ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			int t = 0;
			while( can_target(&td) && t < count_counters(player, card, COUNTER_KI) ){
					if( new_pick_target(&td, "TARGET_CREATURE", t, 0) ){
						state_untargettable(instance->targets[t].player, instance->targets[t].card, 1);
						t++;
					}
					else{
						break;
					}
			}
			int i;
			for(i=0; i<t; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			}
			remove_counters(player, card, COUNTER_KI, t);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				tap_card(instance->targets[i].player, instance->targets[i].card);
			}
		}
	}

	return 0;
}

static int yomiji_who_bars_the_way_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_YOMIJI_WHO_BARS_THE_WAY ){
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
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_PERMANENT);
				this_test.subtype = SUBTYPE_LEGEND;
				count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, 0, &this_test, GFPC_TRACK_DEAD_CREATURES, 0);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			card_instance_t *instance = get_card_instance(player, card);
			int i;
			for(i=0; i<10; i++){
				if( instance->targets[i].player > -1 ){
					int owner = instance->targets[i].player;
					int iid = instance->targets[i].card;
					instance->targets[i].player = instance->targets[i].card = -1;
					seek_grave_for_id_to_reanimate(player, card, owner, cards_data[iid].id, REANIMATEXTRA_RETURN_TO_HAND2);
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

int card_yomiji_who_bars_the_way(int player, int card, event_t event){
/* CARD_ID_YOMIJI_WHO_BARS_THE_WAY	7595
Yomiji, Who Bars the Way	|5|W|W
Legendary Creature - Spirit 4/4
Whenever a legendary permanent other than ~ is put into a graveyard from the battlefield, return that card to its owner's hand. */

	check_legend_rule(player, card, event);

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)yomiji_who_bars_the_way_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &yomiji_who_bars_the_way_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_LEGEND;
		count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, 0, &this_test, GFPC_TRACK_DEAD_CREATURES, 0);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			card_instance_t *instance = get_card_instance(player, card);
			int i;
			for(i=0; i<10; i++){
				if( instance->targets[i].player > -1 ){
					int owner = instance->targets[i].player;
					int iid = instance->targets[i].card;
					instance->targets[i].player = instance->targets[i].card = -1;
					seek_grave_for_id_to_reanimate(player, card, owner, cards_data[iid].id, REANIMATEXTRA_RETURN_TO_HAND2);
				}
			}
		}
	}

	return 0;
}

int card_yukora_the_prisoner(int player, int card, event_t event){
/* CARD_ID_YUKORA_THE_PRISONER	7596
Yukora, the Prisoner	|2|B|B
Legendary Creature - Demon Spirit 5/5
When ~ leaves the battlefield, sacrifice all non-Ogre creatures you control. */

	check_legend_rule(player, card, event);

	if( leaves_play(player, card, event) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_OGRE;
		test.subtype_flag = DOESNT_MATCH;
		new_manipulate_all(player, card, player, &test, KILL_SACRIFICE);
	}
	return 0;
}
