#include "manalink.h"

//----- GLOBAL FUNCTION ----

static int generic_type_recycler_creature(int player, int card, event_t event, test_definition_t *this_test){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( ! graveyard_has_shroud(player) ){
			strcpy(this_test->message, "Select target ");
			if (ai_is_speculating != 1)
			{
				int l = strlen(this_test->message);
				type_text(this_test->message + l, 100 - l, this_test->type, 0);
				l = strlen(this_test->message);
				scnprintf(this_test->message + l, 100 - l, " card.");
			}
			if( new_special_count_grave(player, this_test) ){
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, this_test);
			}
		}
	}

	return 0;
}

static void pump_when_blocked(int player, int card, event_t event, int pow_plus, int tou_plus, int ability){

	if( event == EVENT_DECLARE_BLOCKERS && current_turn == player && is_attacking(player, card) ){
		int count = 0;
		while(count < active_cards_count[1-player]){
				if(in_play(1-player, count) && is_what(1-player, count, TYPE_CREATURE) ){
					card_instance_t *instance = get_card_instance( 1-player, count);
					if( instance->blocking == card ){
						pump_ability_until_eot(player, card, player, card, pow_plus, tou_plus, ability, 0);
					}
				}
				count++;
		}
	}
}

static int exodus_keepers(int player, int card, event_t event, int color){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	int c[6] = {0, 0, 0, 0, 0, 0};
	c[color] = 1;

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_ONLY_TARGET_OPPONENT | GAA_UNTAPPED, c[0], c[1], c[2], c[3], c[4], c[5], 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, c[0], c[1], c[2], c[3], c[4], c[5]) ){
			instance->targets[0].player = 1-player;
			instance->targets[0].card = -1;
			instance->number_of_targets = 1;
		}
	}

	return 0;
}

//---- CARDS ----

int card_allay(int player, int card, event_t event)
{
  // Buyback |3
  // Destroy target enchantment.

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_CAN_CAST || event == EVENT_CAN_CHANGE_TARGET || event == EVENT_CHANGE_TARGET ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT", 1, NULL);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( pick_target(&td, "TARGET_ENCHANTMENT") ){
			instance->info_slot = buyback(player, card, MANACOST_X(3));
		}
	}

	if (event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td)){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if (instance->info_slot){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_anarchist(int player, int card, event_t event){
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && reason_for_trigger_controller == player ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_SORCERY);
		this_test.type_flag = F1_NO_CREATURE;
		return generic_type_recycler_creature(player, card, event, &this_test);
	}
	return 0;
}

int card_carnophage(int player, int card, event_t event){

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( ! is_tapped(player, card) ){
			int to_tap = 1;
			if( can_pay_life(player, 1) ){
				int ai_choice = 0;
				if(life[player]-1 < 6){
					ai_choice = 1;
				}

				int choice = do_dialog(player, player, card, -1, -1, " Pay 1 life\n Tap this", ai_choice);
				if( choice == 0){
					lose_life(player, 1);
					to_tap = 0;
				}
			}

			if( to_tap == 1 ){
				tap_card(player, card);
			}
		}
	}

	return 0;
}

int card_cartographer(int player, int card, event_t event){
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && reason_for_trigger_controller == player ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		return generic_type_recycler_creature(player, card, event, &this_test);
	}
	return 0;
}

int not_marked(int iid, int markedptr, int player, int card)
{
  char (*marked)[2][151] = (char(*)[2][151])markedptr;
  return !(*marked)[player][card];
}

int card_cataclysm(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
			char marked[2][151] = {{0}};
			APNAP(p, {
				target_definition_t td;
				default_target_definition(player, card, &td, 0);
				td.allowed_controller = p;
				td.preferred_controller = p;
				td.who_chooses = p;
				td.allow_cancel = 0;
				td.illegal_abilities = 0;

				type_t types[] = { TYPE_ARTIFACT, TYPE_CREATURE, TYPE_ENCHANTMENT, TYPE_LAND };
				const char* prompts[] = { "TARGET_ARTIFACT", "TARGET_CREATURE", "TARGET_ENCHANTMENT", "TARGET_LAND" };

				int i;
				for (i = 0; i < 4; ++i){
					td.required_type = types[i];
					if (can_target(&td) && pick_target(&td, prompts[i])){
						marked[instance->targets[0].player][instance->targets[0].card] = 1;
						card_instance_t* aff = get_card_instance(player, card);

						// It's legal to choose the same permanent for more than one type, but don't let the AI
						aff->state |= STATE_TARGETTED;
						if (p == AI || ai_is_speculating == 1){
							aff->state |= STATE_CANNOT_TARGET;
						}
					}
				}

				int c;
				for (c = 0; c < active_cards_count[p]; ++c){
					if (marked[p][c]){
						// In particular, don't leave the cannot-target markers in place while sacrificing, or they interfere with leave-play triggers
						get_card_instance(p, c)->state &= ~(STATE_TARGETTED | STATE_CANNOT_TARGET);
					}
				}
			});
			APNAP(p, {
				if( can_sacrifice(player, p, 1, TYPE_PERMANENT, 0) ){
					test_definition_t this_test;
					default_test_definition(&this_test, TYPE_PERMANENT);
					this_test.special_selection_function = &not_marked;
					this_test.value_for_special_selection_function = (int)(marked);
					new_manipulate_all(player, card, p, &this_test, KILL_SACRIFICE);
				}
			});
			kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_city_of_traitors(int player, int card, event_t event){
	if( ! is_unlocked(player, card, event, 2) ){ return 0; }

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( whenever_a_player_plays_a_land(player, card, event, player, RESOLVE_TRIGGER_MANDATORY) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return two_mana_land(player, card, event, COLOR_COLORLESS, COLOR_COLORLESS);
}

int card_coat_of_arms(int player, int card, event_t event){
	// original code : 004057C0

	/* Coat of Arms	|5
	 * Artifact
	 * Each creature gets +1/+1 for each other creature on the battlefield that shares at least one creature type with it. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
		in_play(player, card) && in_play(affected_card_controller, affected_card) && !is_humiliated(player, card) )
	{
		event_result += count_all_other_creatures_that_shares_a_creature_type_with_me(affected_card_controller, affected_card, ANYBODY);
	}

	return 0;
}

int card_convalescence(int player, int card, event_t event){

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( life[player] < 10 ){
			gain_life(player, 1);
		}
	}

	return global_enchantment(player, card, event);
}

int card_culling_the_weak(int player, int card, event_t event){

	if( event == EVENT_RESOLVE_SPELL ){
		produce_mana(player, COLOR_BLACK, 4);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 0, NULL);
}

int card_curiosity(int player, int card, event_t event)
{
  // original code : 0x405D70

  /* Curiosity	|U
   * Enchantment - Aura
   * Enchant creature
   * Whenever enchanted creature deals damage to an opponent, you may draw a card. */

  int packets;
  if ((packets = attached_creature_deals_damage(player, card, event, DDBM_MUST_DAMAGE_OPPONENT|DDBM_TRIGGER_OPTIONAL)))
	draw_cards(player, packets);

  return vanilla_aura(player, card, event, player);
}

int card_dauthi_jackal(int player, int card, event_t event){

	shadow(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.required_state = TARGET_STATE_BLOCKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME+GAA_CAN_TARGET, MANACOST_B(2), 0, &td1, "TARGET_CREATURE");
}

int card_dauthi_warlord(int player, int card, event_t event){
	shadow(player, card, event);
	if(event == EVENT_POWER && affect_me(player, card) && ! is_humiliated(player, card) ){
		int p;
		for( p = 0; p < 2; p++){
			int count = 0;
			while(count < active_cards_count[p]){
				if( in_play(p, count) && check_for_special_ability(p, count, SP_KEYWORD_SHADOW) ){
					event_result++;
				}
				count++;
		   }
		}
	}
	return 0;
}

int card_deaths_duet(int player, int card, event_t event)
{
  /* Death's Duet	|2|B
   * Sorcery
   * Return two target creature cards from your graveyard to your hand. */

  if (event == EVENT_CAN_CAST)
	return count_graveyard_by_type(player, TYPE_CREATURE) >= 2 && !graveyard_has_shroud(player);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "Select two target creature cards.");

	  card_instance_t* instance = get_card_instance(player, card);
	  select_multiple_cards_from_graveyard(player, player, -1, AI_MAX_VALUE, &test, 2, &instance->targets[0]);
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  int i, num_validated = 0, selected;
	  for (i = 0; i < 2; ++i)
		if ((selected = validate_target_from_grave(player, card, player, i)) != -1)
		  {
			from_grave_to_hand(player, selected, TUTOR_HAND);
			++num_validated;
		  }

	  if (num_validated == 0)
		spell_fizzled = 1;

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_dominating_licid(int player, int card, event_t event){
	/*
	  Dominating Licid |1|U|U
	  Creature - Licid 1/1
	  {1}{U}{U}, {T}: Dominating Licid loses this ability and becomes an Aura enchantment with enchant creature.
	  Attach it to target creature. You may pay {U} to end this effect.

	  You control enchanted creature.
	*/
	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CHANGE_TYPE && affect_me(player, card) && instance->targets[2].card > 0 ){
		event_result = instance->targets[2].card;
	}

	if( ! is_what(player, card, TYPE_CREATURE) ){
		if( leaves_play(player, card, event) ){
			if( instance->targets[3].card > -1 ){
				kill_card(instance->targets[3].player, instance->targets[3].card, KILL_EXILE);
			}
		}

		if( ! IS_AURA_EVENT(player, card, event) && ! IS_GAA_EVENT(event) ){
			return 0;
		}
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		if( instance->damage_target_player > -1 ){
			if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
				return generic_activated_ability(player, card, event, 0, MANACOST_U(1), 0, NULL, NULL);
			}

			if( event == EVENT_RESOLVE_ACTIVATION ){
				card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
				reset_subtypes(instance->parent_controller, instance->parent_card, 2);
				parent->targets[2].card = parent->damage_target_player = parent->damage_target_card = parent->targets[3].card = -1;
				if( instance->targets[3].card > -1 ){
					kill_card(instance->targets[3].player, instance->targets[3].card, KILL_EXILE);
				}
			}
			return generic_stealing_aura(player, card, event, &td, "TARGET_CREATURE");
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK+GAA_CAN_TARGET, MANACOST_XU(1, 2), 0, &td, "TARGET_CREATURE");
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
			int newtype = create_a_card_type(parent->internal_card_id);
			cards_at_7c7000[newtype]->type = TYPE_ENCHANTMENT;
			cards_at_7c7000[newtype]->power = 0;
			cards_at_7c7000[newtype]->toughness = 0;
			parent->targets[2].card = newtype;
			add_a_subtype(instance->parent_controller, instance->parent_card, SUBTYPE_AURA_CREATURE);
			gain_control_and_attach_as_aura(instance->parent_controller, instance->parent_card, event, instance->targets[0].player, instance->targets[0].card);
		}
	}
	return 0;
}

int card_elvish_berserk(int player, int card, event_t event){

	pump_when_blocked(player, card, event, 1, 1,0);

	return 0;
}

int card_ephemeron(int player, int card, event_t event){
	// Used externally
	card_instance_t *instance = get_card_instance(player, card);
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( in_play(instance->parent_controller, instance->parent_card) ){
			bounce_permanent(instance->parent_controller, instance->parent_card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
}


int card_equilibrium(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) ){
			if( has_mana(player, COLOR_ANY, 1) && can_target(&td) ){
				if( charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1) ){
					if( pick_target(&td, "TARGET_CREATURE") ){
						bounce_permanent(instance->targets[0].player, instance->targets[0].card);
					}
				}
			}
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_erratic_portal(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

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

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_CREATURE");
}

int card_ertai_wizard_adept(int player, int card, event_t event){

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( counterspell_validate(player, card, &td, 0) ){
			real_counter_a_spell(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_NONSICK | GAA_SPELL_ON_STACK, MANACOST_XU(2, 2), 0, &td, NULL);
}

int card_exalted_dragon(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_PAY_TO_ATTACK && current_phase == PHASE_DECLARE_ATTACKERS && ! is_humiliated(player, card) &&
		affect_me( player, card ) && reason_for_trigger_controller == player && forbid_attack == 0 &&
		trigger_cause_controller == player && trigger_cause == card )
	{
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
				if( ! new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
					forbid_attack = 1;
				}
		}
	}

	return 0;
}

int card_flowstone_flood(int player, int card, event_t event){

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( pick_target(&td, "TARGET_LAND") ){
			if( ! is_token(player, card) && can_pay_life(player, 3) && hand_count[player] > 0 ){
				int choice = do_dialog(player, player, card, -1, -1, " Play normally\n Buyback", 1);
				if( choice == 1 ){
					lose_life(player, 3);
					discard(player, DISC_RANDOM, player);
					instance->info_slot = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_forbid(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return counterspell(player, card, event, NULL, 0);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		counterspell(player, card, event, NULL, 0);
		if( spell_fizzled != 1 && ! is_token(player, card) && hand_count[player] >= 2 ){
			int choice = do_dialog(player, player, card, -1, -1, " Play normally\n Buyback", 0);
			instance->info_slot = choice;
			if( choice == 1){
				multidiscard(player, 2, 0);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_hatred(int player, int card, event_t event){

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_pay_life_as_cost_for_spells_or_activated_abilities(player, 1) &&
				generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( pick_target(&td, "TARGET_CREATURE") ){
			int to_pay = 0;
			if( player == AI ){
				to_pay = life[1-player]-get_attack_power(instance->targets[0].player, instance->targets[0].card);
			}
			else{
				 to_pay = choose_a_number(player, "Pay how much life?", life[player]);
			}

			if( to_pay > life[player] ){
				to_pay = life[player]-1;
			}

			if( to_pay < 1 ){
				spell_fizzled = 1;
			}
			else{
				 lose_life(player, to_pay);
				 instance->info_slot = to_pay;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, instance->info_slot, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_high_ground(int player, int card, event_t event)
{
  if (affected_card_controller == player)
	arbitrary_can_block_additional(event, 1);

  return global_enchantment(player, card, event);
}

int card_jackalope_herd(int player, int card, event_t event){

	if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		bounce_permanent(player, card);
	}

	return 0;
}

int card_keeper_of_the_beasts(int player, int card, event_t event){
	/*
	  Keeper of the Beasts English |G|G
	  Creature - Human Wizard 1/2
	  {G}, {T}: Choose target opponent who controlled more creatures than you did as you activated this ability.
	  Put a 2/2 green Beast creature token onto the battlefield.
	*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_ACTIVATE ){
		if( exodus_keepers(player, card, event, COLOR_GREEN) && count_subtype(1-player, TYPE_CREATURE, -1) > count_subtype(player, TYPE_CREATURE, -1) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		exodus_keepers(player, card, event, COLOR_GREEN);
		if( spell_fizzled != 1 ){
			tap_card(player, card);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_BEAST, &token);
			token.pow = 2;
			token.tou = 2;
			generate_token(&token);
		}
	}

	return 0;
}

int card_keeper_of_the_dead(int player, int card, event_t event){
	/*
	  Keeper of the Dead |B|B
	  Creature - Human Wizard 1/2
	  {B}, {T}: Choose target opponent who had at least two fewer creature cards in his or her graveyard than you did as you activated this ability.
	  Destroy target nonblack creature he or she controls.
	*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
	td1.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( exodus_keepers(player, card, event, COLOR_BLACK) ){
			return count_graveyard_by_type(player, TYPE_CREATURE)-count_graveyard_by_type(1-player, TYPE_CREATURE) > 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		exodus_keepers(player, card, event, COLOR_BLACK);
		if( spell_fizzled != 1 ){
			if( new_pick_target(&td1, get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, 1 | GS_LITERAL_PROMPT) ){
				tap_card(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && validate_target(player, card, &td1, 1) ){
			kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_keeper_of_the_flame(int player, int card, event_t event){
	/*
	  Keeper of the Flame |R|R
	  Creature - Human Wizard 1/2
	  {R}, {T}: Choose target opponent who had more life than you did as you activated this ability. Keeper of the Flame deals 2 damage to him or her.
	*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_ACTIVATE ){
		if( exodus_keepers(player, card, event, COLOR_RED) ){
			return life[1-player] > life[player];
		}
	}

	if( event == EVENT_ACTIVATE ){
		exodus_keepers(player, card, event, COLOR_RED);
		if( spell_fizzled != 1 ){
			tap_card(player, card);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return 0;
}

int card_keeper_of_the_light(int player, int card, event_t event){
	/*
	  Keeper of the Light |W|W
	  Creature - Human Wizard 1/2
	  {W}, {T}: Choose target opponent who had more life than you did as you activated this ability. You gain 3 life.
	*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_ACTIVATE ){
		if( exodus_keepers(player, card, event, COLOR_WHITE) ){
			return life[player] < life[1-player];
		}
	}

	if( event == EVENT_ACTIVATE ){
		exodus_keepers(player, card, event, COLOR_WHITE);
		if( spell_fizzled != 1 ){
			tap_card(player, card);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			gain_life(player, 3);
		}
	}

	return 0;
}

int card_keeper_of_the_mind(int player, int card, event_t event){
	/*
	  Keeper of the Mind |U|U
	  Creature - Human Wizard 1/2
	  {U}, {T}: Choose target opponent who had at least two more cards in hand than you did as you activated this ability. Draw a card.
	*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_ACTIVATE ){
		if( exodus_keepers(player, card, event, COLOR_BLUE) && (hand_count[1-player]-hand_count[player]) > 1){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		exodus_keepers(player, card, event, COLOR_BLUE);
		if( spell_fizzled != 1 ){
			tap_card(player, card);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_mana_breach(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && reason_for_trigger_controller == player ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.illegal_abilities = 0;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, NULL) ){
			td.allowed_controller = trigger_cause_controller;
			td.preferred_controller = trigger_cause_controller;
			td.who_chooses = trigger_cause_controller;
			if( can_target(&td) && new_pick_target(&td, "Select a land you control to return to your hand.", 0, GS_LITERAL_PROMPT) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_manabond(int player, int card, event_t event){
	if( ! is_unlocked(player, card, event, 13) ){ return 0; }

	if( eot_trigger_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player)) ){
		reveal_target_player_hand(player);
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_hand(p, c) && is_what(p, c, TYPE_LAND) ){
										put_into_play(p, c);
									}
								};
		);
	}

	return global_enchantment(player, card, event);
}

int card_memory_crystal(int player, int card, event_t event){

	return 0;
}

int card_merfolk_looter(int player, int card, event_t event)
{
  // 0x1202CB8

  /* Merfolk Looter	|1|U
   * Creature - Merfolk Rogue 1/1
   * |T: Draw a card, then discard a card. */
  // Used externally
  if (!IS_GAA_EVENT(event))
	return 0;

  if (event == EVENT_ACTIVATE && IS_AI(player) && player == AI)
	{
	  // Same logic as Jalum Tome.
	  int c, total_lands = 0;
	  for (c = 0; c < active_cards_count[player]; ++c)
		if ((in_play(player, c) || in_hand(player, c)) && is_what(player, c, TYPE_LAND))
		  ++total_lands;

	  if (total_lands <= 2 || total_lands >= 8)
		ai_modifier += 48;
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  draw_a_card(player);
	  discard(player, 0, player);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_mind_maggots(int player, int card, event_t event){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( hand_count[player] ){
			char msg[100] = "Select a creature card.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, msg);
			this_test.zone = TARGET_ZONE_HAND;

			if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
				int hand_copy[2][hand_count[player]];
				int hcc = 0;
				int discarded[hand_count[player]];
				int dc = 0;
				CYCLE_ALL_CARDS(p, c, 	{
											if( p == player && in_hand(p, c) ){
												hand_copy[0][hcc] = get_original_internal_card_id(p, c);
												hand_copy[1][hcc] = c;
												hcc++;
											}
										}
				);
				while( hcc ){
						int selected = select_card_from_zone(player, player, hand_copy[0], hcc, 0, AI_MIN_VALUE, -1, &this_test);
						if( selected != -1 ){
							discarded[dc] = hand_copy[1][selected];
							dc++;
						}
						else{
							break;
						}
						if( player == AI && dc == 3 ){
							break;
						}
				}
				int i;
				for(i=0; i<dc; i++){
					new_discard_card(player, discarded[i], 0, player);
					add_1_1_counters(player, card, 2);
				}
			}
		}
	}

	return 0;
}

int card_mind_over_matter(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	char msg[100] = "Select a card to discard.";
	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, msg);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_LAND | TYPE_ARTIFACT);

	if(event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DISCARD, MANACOST0, 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DISCARD, MANACOST0, 0, &td, "TWIDDLE");
		ai_modifier_twiddle(player, card, 0);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td, 0) ){
			twiddle(player, card, 0);
		}
	}

	return 0;
}

int card_mindless_automaton(int player, int card, event_t event){
/*
Mindless Automaton |4
Artifact Creature — Construct 0/0
Mindless Automaton enters the battlefield with two +1/+1 counters on it.
{1}, Discard a card: Put a +1/+1 counter on Mindless Automaton.
Remove two +1/+1 counters from Mindless Automaton: Draw a card.
*/
	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		int max_pump = hand_count[player];
		while( ! has_mana_for_activated_ability(player, card, MANACOST_X(max_pump)) ){
				max_pump--;
		}
		return max_pump;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_DISCARD = 1,
		CHOICE_DRAW
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST_X(1), 0, NULL, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_P1_P1, 2), NULL, NULL) ){
			return 1;
		}

	}
	if( event == EVENT_ACTIVATE ){
		int abilities[3] = {
							0,
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_DISCARD, MANACOST_X(1), 0, NULL, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, GVC_COUNTERS(COUNTER_P1_P1, 2), NULL, NULL)
		};

		int choice = DIALOG(player, card, EVENT_CAST_SPELL, DLG_RANDOM,
							"Discard & pump", 	abilities[1], (current_phase == PHASE_AFTER_BLOCKING && hand_count[player]) ? 10 : 1,
							"Draw a card", 		abilities[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_DISCARD ){
			return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST_X(1), 0, NULL, NULL);
		}
		if( choice == CHOICE_DRAW ){
			return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_P1_P1, 2), NULL, NULL);
	    }
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DISCARD ){
			add_1_1_counter(instance->parent_controller, instance->parent_card);
		}
		if( instance->info_slot == CHOICE_DRAW ){
			draw_cards(player, 1);
		}
	}


	return 0;
}

int card_mirri_cat_warrior(int player, int card, event_t event)
{
  /* Mirri, Cat Warrior	|1|G|G
   * Legendary Creature - Cat Warrior 2/3
   * First strike, |H2forestwalk, vigilance */

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  return 0;
}

int card_nausea(int player, int card, event_t event){
  /* Mirri, Cat Warrior	|1|G|G
   * Legendary Creature - Cat Warrior 2/3
   * First strike, |H2forestwalk, vigilance */

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, ANYBODY, -1, -1, -1, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}


int card_null_brooch(int player, int card, event_t event){
/* CARD_ID_NULL_BROOCH	3521
Null Brooch	|4
Artifact
|2, |T, Discard your hand: Counter target noncreature spell. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);
	td.illegal_type = TYPE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SPELL_ON_STACK, MANACOST_X(2), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if(	charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			tap_card(player, card);
			discard_all(player);
			instance->targets[0].player = card_on_stack_controller;
			instance->targets[0].card = card_on_stack;
			instance->number_of_targets = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( counterspell_validate(player, card, &td, 0) ){
			real_counter_a_spell(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_oath_of_druids(int player, int card, event_t event){
/*
Oath of Druids |1|G
Enchantment
At the beginning of each player's upkeep, that player chooses target player who controls more creatures than he or she does and is his
or her opponent. The first player may reveal cards from the top of his or her library until he or she reveals a creature card.
If he or she does, that player puts that card onto the battlefield and all other cards revealed this way into his or her graveyard.
*/

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( count_subtype(current_turn, TYPE_CREATURE, -1) < count_subtype(1-current_turn, TYPE_CREATURE, -1) ){
			int trig = 1;
			if( player_bits[1-current_turn] & (PB_PLAYER_HAS_SHROUD | PB_PLAYER_HAS_HEXPROOF) ){
				trig = 0;
			}
			if( trig ){
				int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_CANCEL, DLG_NO_STORAGE,
									"Activate Oath of Druids", 1, count_subtype(current_turn, TYPE_CREATURE, -1) == 0 ? 10 : 1,
									"Decline", 1, 1);
				if( choice == 1 ){
					test_definition_t test;
					default_test_definition(&test, TYPE_CREATURE);
					reveal_card_until_you_reveal_test(current_turn, current_turn, &test, RCUYRT_MILL_REST | RCUYRT_PUT_TEST_INTO_PLAY);
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_oath_of_ghouls(int player, int card, event_t event){
/* CARD_ID_OATH_OF_GHOULS	3523
Oath of Ghouls	|1|B
Enchantment
At the beginning of each player's upkeep, that player chooses target player whose graveyard has fewer creature cards in it than
his or her graveyard does and is his or her opponent. The first player may return a creature card from his or her graveyard to his or her hand. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( count_graveyard_by_type(current_turn, TYPE_CREATURE) > count_graveyard_by_type(1-current_turn, TYPE_CREATURE) ){
			int trig = 1;
			if( player_bits[1-current_turn] & (PB_PLAYER_HAS_SHROUD | PB_PLAYER_HAS_HEXPROOF) ){
				trig = 0;
			}
			if( trig ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
				new_global_tutor(current_turn, current_turn, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_oath_of_lieges(int player, int card, event_t event){
/* CARD_ID_OATH_OF_LIEGES	3524
Oath of Lieges	|1|W
Enchantment
At the beginning of each player's upkeep, that player chooses target player who controls more lands than he or she does and is his or her opponent.
The first player may search his or her library for a basic land card, put that card onto the battlefield, then shuffle his or her library. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->targets[1].card = 1;
	}

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( landsofcolor_controlled[current_turn][COLOR_ANY] < landsofcolor_controlled[1-current_turn][COLOR_ANY] ){
			int trig = 1;
			if( player_bits[1-current_turn] & (PB_PLAYER_HAS_SHROUD | PB_PLAYER_HAS_HEXPROOF) ){
				trig = 0;
			}
			if( trig ){
				int prev_found = get_card_instance(player, card)->targets[1].card;
				int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_CANCEL, DLG_NO_STORAGE,
									"Activate Oath of Lieges", 1, prev_found ? 10 : 1,
									"Decline", 1, 5);
				if( choice == 1 ){
					test_definition_t this_test;
					new_default_test_definition(&this_test, TYPE_LAND, "Select a basic land card.");
					this_test.subtype = SUBTYPE_BASIC;
					int found = new_global_tutor(current_turn, current_turn, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);
					get_card_instance(player, card)->targets[1].card = found;
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_oath_of_scholars(int player, int card, event_t event){
/* CARD_ID_OATH_OF_SCHOLARS	3526
Oath of Scholars	|3|U
Enchantment
At the beginning of each player's upkeep, that player chooses target player who has more cards in hand than he or she does and is his or her opponent.
The first player may discard his or her hand and draw three cards. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( hand_count[current_turn] < hand_count[1-current_turn] ){
			int trig = 1;
			if( player_bits[1-current_turn] & (PB_PLAYER_HAS_SHROUD | PB_PLAYER_HAS_HEXPROOF) ){
				trig = 0;
			}
			if( trig ){
				int priority_draw = hand_count[player] < 3 && count_deck(player) >= 20 ? 10 : 1;
				int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_CANCEL, DLG_NO_STORAGE,
									"Activate Oath of Scholars", 1, priority_draw,
									"Decline", 1, 5);
				if( choice == 1 ){
					new_discard_all(current_turn, player);
					draw_cards(current_turn, 3);
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_oath_of_mages(int player, int card, event_t event){
/* CARD_ID_OATH_OF_MAGES	3525
Oath of Mages	|1|R
Enchantment
At the beginning of each player's upkeep, that player chooses target player who has more life than he or she does and is his or her opponent.
The first player may have ~ deal 1 damage to the second player. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( life[current_turn] < life[1-current_turn] ){
			int trig = 1;
			if( player_bits[1-current_turn] & (PB_PLAYER_HAS_SHROUD | PB_PLAYER_HAS_HEXPROOF) ){
				trig = 0;
			}
			if( trig ){
				int priority_draw = hand_count[player] < 3 && count_deck(player) >= 20 ? 10 : 1;
				int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_CANCEL, DLG_NO_STORAGE,
									"Activate Oath of Mages", 1, priority_draw,
									"Decline", 1, 5);
				if( choice == 1 ){
					damage_player(1-current_turn, 1, player, card);
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_ogre_shaman(int player, int card, event_t event){
	/* Ogre Shaman	|3|R|R	0x20036bc
	 * Creature - Ogre Shaman 3/3
	 * |2, Discard a card at random: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DISCARD, MANACOST_X(2), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) && pick_target(&td, "TARGET_ANY") ){
			discard(player, DISC_RANDOM, player);
		}
	}

   if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
   }

	return 0;
}

int card_onslaught(int player, int card, event_t event){
/* CARD_ID_ONSLAUGHT	3528
Onslaught	|R
Enchantment
Whenever you cast a creature spell, tap target creature. */

	if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target creature to tap.", 0, GS_LITERAL_PROMPT) ){
			action_on_target(player, card, 0, ACT_TAP);
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_paladin_en_vec(int player, int card, event_t event){
	/*
	  Paladin en-Vec |1|W|W
	  Creature - Human Knight 2/2
	  First strike, protection from black and from red
	*/
	if( ! is_humiliated(player, card) ){
		protection_from_black(player, card, event);
		protection_from_red(player, card, event);
	}

	return 0;
}

void pandemonium_effect(int player, int card){
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td.who_chooses = player;

	card_instance_t* instance = get_card_instance(player, card);
	int pow = get_power(player, card);
	instance->number_of_targets = 0;

	if( pow > 0 && can_target(&td) && pick_target(&td, "TARGET_ANY") ){
		damage_target0(player, card, pow);
	}
	instance->number_of_targets = 0;
}

int card_pandemonium(int player, int card, event_t event){
	/* Pandemonium	|3|R	0x2002ab4
	 * Enchantment
	 * Whenever a creature enters the battlefield, that creature's controller may have it deal damage equal to its power to any target of their choice. */
	etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		pandemonium_effect(trigger_cause_controller, trigger_cause);
	}

	return global_enchantment(player, card, event);
}

int card_peace_of_mind(int player, int card, event_t event){
/* CARD_ID_PEACE_OF_MIND	3531
Peace of Mind	|1|W
Enchantment
|W, Discard a card: You gain 3 life. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 3);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST_W(1), 0, NULL, NULL);
}

int card_pegasus_stampede(int player, int card, event_t event){
	/* Pegasus Stampede	|1|W
	 * Sorcery
	 * Buyback-Sacrifice a land.
	 * Put a 1/1 |Swhite Pegasus creature token with flying onto the battlefield. */

	 if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! is_token(player, card) && can_sacrifice_type_as_cost(player, 1, TYPE_LAND) ){
			int choice = DIALOG(player, card, event, DLG_NO_CANCEL, DLG_NO_STORAGE,
								"Pay Buyback cost", 1, count_subtype(player, TYPE_LAND, -1) >= 6 ? 10 : 1,
								"Decline", 1, 5);
			if( choice == 1 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
				if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
					instance->info_slot = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(player, card, CARD_ID_PEGASUS);
		if( instance->info_slot == 1 ){
			bounce_permanent(player, card);
			return 0;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_pit_spawn(int player, int card, event_t event){
/* CARD_ID_PIT_SPAWN	3534
Pit Spawn	|4|B|B|B
Creature - Demon 6/4
First strike
At the beginning of your upkeep, sacrifice ~ unless you pay |B|B.
Whenever ~ deals damage to a creature, exile that creature. */

	if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card)){
		get_card_instance(player, card)->destroys_if_blocked |= DIFB_DESTROYS_UNPROTECTED;
	}

	basic_upkeep(player, card, event, MANACOST_B(2));

	rfg_when_damage(player, card, event);

	return 0;
}

static int target_for_plaguebearer(int player, int card, int mode){
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance= get_card_instance(player, card);

	int i;
	int trg = -1;
	int par = -1;
	for(i=0; i<2; i++){
		if( i == 1-player || player == HUMAN ){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) ){
						int cost = get_cmc(i, count)*2;
						if( has_mana_for_activated_ability(player, card, MANACOST_XB(cost, 1)) ){
							instance->targets[0].player = i;
							instance->targets[0].card = count;
							instance->number_of_targets = 1;
							if( would_validate_target(player, card, &td, 0) ){
								if( mode == 0 || player == HUMAN ){
									return 1;
								}
								if( mode == 1 && get_base_value(i, count) > par ){
									par = get_base_value(i, count);
									trg = count;
								}
							}
						}
					}
					count++;
			}
		}
	}
	if( mode == 1 ){
		return trg;
	}
	return 0;
}

int card_plaguebearer(int player, int card, event_t event){
	/*
	  Plaguebearer English |1|B
	  Creature - Zombie 1/1
	  {X}{X}{B}: Destroy target nonblack creature with converted mana cost X.
	*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
		return target_for_plaguebearer(player, card, 0);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int op = -1;
		int oc = -1;
		if( player == HUMAN ){
			if( select_target(player, card, &td, "Select target nonblack creature.", NULL) ){
				op = instance->targets[0].player;
				oc = instance->targets[0].card;
				int cost = 2*get_cmc( instance->targets[0].player, instance->targets[0].card );
				if( ! has_mana_for_activated_ability(player, card, MANACOST_XB(cost, 1)) ){
					spell_fizzled = 1;
					return 0;
				}
				if( charge_mana_for_activated_ability(player, card, MANACOST_XB(cost, 1)) ){
					instance->targets[0].player = op;
					instance->targets[0].card = oc;
					instance->number_of_targets = 1;
				}
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		else{
			int result = target_for_plaguebearer(player, card, 1);
			if( result != -1 ){
				int cost = 2*get_cmc( 1-player, result );
				if( charge_mana_for_activated_ability(player, card, MANACOST_XB(cost, 1)) ){
					instance->targets[0].player = 1-player;
					instance->targets[0].card = result;
					instance->number_of_targets = 1;
				}
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card( instance->targets[0].player, instance->targets[0].card, KILL_DESTROY );
		}
	}

	return 0;
}

int card_price_of_progress(int player, int card, event_t event){
/* CARD_ID_PRICE_OF_PROGRESS	3538
Price of Progress	|1|R
Instant
~ deals damage to each player equal to twice the number of nonbasic lands that player controls. */

	if(event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.subtype = SUBTYPE_BASIC;
		this_test.subtype_flag = DOESNT_MATCH;
		APNAP(p,
				{
					int amount = 2*check_battlefield_for_special_card(player, card, p, CBFSC_GET_COUNT, &this_test);
					if( amount ){
						damage_player(p, amount, player, card);
					}
				}
		);
		kill_card(player, card, KILL_BURY);
	}

	return basic_spell(player, card, event);
}

int card_pygmy_troll(int player, int card, event_t event){
/* CARD_ID_PYGMY_TROLL	3539
Pygmy Troll	|1|G
Creature - Troll 1/1
Whenever ~ becomes blocked by a creature, ~ gets +1/+1 until end of turn.
|G: Regenerate ~. */

	pump_when_blocked(player, card, event, 1, 1, 0);

	return card_wall_of_brambles(player, card, event);
}

int card_ravenous_baboons(int player, int card, event_t event){
/* CARD_ID_RAVENOUS_BABOONS	3541
Ravenous Baboons	|3|R
Creature - Monkey 2/2
When ~ enters the battlefield, destroy target nonbasic land. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.required_subtype = SUBTYPE_BASIC;
		td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target non-Basic land.", 0, GS_LITERAL_PROMPT) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_reaping_the_rewards(int player, int card, event_t event){
/* CARD_ID_REAPING_THE_REWARDS	3542
Reaping the Rewards	|W
Instant
Buyback-Sacrifice a land. (You may sacrifice a land in addition to any other costs as you cast this spell.
If you do, put this card into your hand as it resolves.) You gain 2 life. */

	 if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! is_token(player, card) && can_sacrifice_type_as_cost(player, 1, TYPE_LAND) ){
			int choice = DIALOG(player, card, event, DLG_NO_CANCEL, DLG_NO_STORAGE,
								"Pay Buyback cost", 1, count_subtype(player, TYPE_LAND, -1) >= 6 ? 10 : 1,
								"Decline", 1, 5);
			if( choice == 1 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
				if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
					instance->info_slot = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		gain_life(player, 2);
		if( instance->info_slot == 1 ){
			bounce_permanent(player, card);
			return 0;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_reclaim(int player, int card, event_t event){
/* CARD_ID_RECLAIM	3544
Reclaim	|G
Instant
Put target card from your graveyard on top of your library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select a card to put on top.");

	if( event == EVENT_RESOLVE_SPELL){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			const int *grave = get_grave(player);
			int card_added = add_card_to_hand(player, grave[selected]);
			remove_card_from_grave(player, selected);
			put_on_top_of_deck(player, card_added);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
}

int card_recurring_nightmare(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");

	if(event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 1);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event,
															GAA_CAN_SORCERY_BE_PLAYED | GAA_BOUNCE_ME | GAA_SACRIFICE_CREATURE, MANACOST0, 0,
															NULL, NULL, 0, &test, GS_GRAVE_RECYCLER);
}

int card_rootwater_mystic(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int *deck = deck_ptr[instance->targets[0].player];
			if( deck[0] != -1 ){
				show_deck( player, deck, 1, "Here's the first card of deck.", 0, 0x7375B0 );
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XU(1, 1), 0, &td, "TARGET_PLAYER");
}

int card_scalding_salamander(int player, int card, event_t event)
{
/* CARD_ID_SCALDING_SALAMANDER	3552
Scalding Salamander	|2|R
Creature - Salamander 2/1
Whenever ~ attacks, you may have it deal 1 damage to each creature without flying defending player controls. */

  if (declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), player, card))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.keyword = KEYWORD_FLYING;
	  test.keyword_flag = DOESNT_MATCH;
	  new_damage_all(player, card, 1-current_turn, 1, 0, &test);
	}

  return 0;
}

int card_scare_tactics(int player, int card, event_t event){
/* CARD_ID_SCARE_TACTICS	3553
Scare Tactics	|B
Instant
Creatures you control get +1/+0 until end of turn. */

	if( event == EVENT_RESOLVE_SPELL){
		pump_subtype_until_eot(player, card, player, -1, 1, 0, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_school_of_piranha(int player, int card, event_t event){
/* CARD_ID_SCHOOL_OF_PIRANHA	3554
School of Piranha	|1|U
Creature - Fish 3/3
At the beginning of your upkeep, sacrifice ~ unless you pay |1|U. */

	basic_upkeep(player, card, event, MANACOST_XU(1, 1));

	return 0;
}

int card_srivener(int player, int card, event_t event){
/* CARD_ID_SCRIVENER	3555
Scrivener	|4|U
Creature - Human Wizard 2/2
When ~ enters the battlefield, you may return target instant card from your graveyard to your hand. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && reason_for_trigger_controller == player ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_INSTANT | TYPE_INTERRUPT);
		this_test.type_flag = F1_NO_CREATURE;
		return generic_type_recycler_creature(player, card, event, &this_test);
	}
	return 0;
}

int card_seismic_assault(int player, int card, event_t event){
	/* Seismic Assault	|R|R|R	0x200a192
	 * Enchantment
	 * Discard a land card: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_LAND, "Select a land card to discard.");
	this_test.zone = TARGET_ZONE_HAND;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td1, NULL) ){
			return check_battlefield_for_special_card(player, card, player, 0, &this_test);
		}
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			instance->number_of_targets = 0;
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
			if( selected != -1 ){
				if( pick_target(&td1, "TARGET_ANY") ){
					instance->number_of_targets = 1;
					discard_card(player, selected);
				}
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			damage_target0(player, card, 2);
		}
	}

	return 0;
}

int card_shackles(int player, int card, event_t event){
/* CARD_ID_SHACKLES	3556
Shackles	|2|W
Enchantment - Aura
Enchant creature
Enchanted creature doesn't untap during its controller's untap step.
|W: Return ~ to its owner's hand. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);

		if( IS_GAA_EVENT(event) ){
			if( event == EVENT_RESOLVE_ACTIVATION && in_play(instance->parent_controller, instance->parent_card) ){
				bounce_permanent(instance->parent_controller, instance->parent_card);
			}
			return generic_activated_ability(player, card, event, 0, MANACOST_W(1), 0, NULL, NULL);
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	return disabling_aura(player, card, event);
}

int card_shattering_pulse(int player, int card, event_t event){
/* CARD_ID_SHATTERING_PULSE	1434
Shattering Pulse	|1|R
Instant
Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
Destroy target artifact. */

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
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
		if( spell_fizzled != 1 && ! is_token(player, card) ){
			instance->info_slot = buyback(player, card, MANACOST_X(3));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_skyshaper(int player, int card, event_t event){
/* CARD_ID_SKYSHAPER	3558
Skyshaper	|2
Artifact
Sacrifice ~: Creatures you control gain flying until end of turn. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_subtype_until_eot(player, card, player, -1, 0, 0, KEYWORD_FLYING, 0);
	}
	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_skyshroud_elite(int player, int card, event_t event){
/* CARD_ID_SKYSHROUD_ELITE	3559
Skyshroud Elite	|G
Creature - Elf 1/1
~ gets +1/+2 as long as an opponent controls a nonbasic land. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_POWER && affect_me(player, card) ){
			if( control_nonbasic_land(1-player) ){
				event_result++;
			}
		}

		if( event == EVENT_TOUGHNESS && affect_me(player, card) ){
			if( control_nonbasic_land(1-player) ){
				event_result+=2;
			}
		}
	}

	return 0;
}

int card_skyshroud_warbeast(int player, int card, event_t event){
	/* Skyshroud War Beast	|1|G
	 * Creature - Beast 100/100
	 * Trample
	 * As ~ enters the battlefield, choose an opponent.
	 * ~'s power and toughness are each equal to the number of nonbasic lands the chosen player controls. */

	if (card == -1){
		return 0;	// No opponent chosen yet.
	}

	// This doesn't account for change of control - it should always be the player chosen as it entered the bf, not the opponent of whoever currently controls it
	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card) ){
		event_result += count_nonbasic_lands(1-player);
	}

	return 0;
}

int card_slaughter2(int player, int card, event_t event){
/* CARD_ID_SLAUGHTER	3561
Slaughter	|2|B|B
Instant
Buyback-Pay 4 life. (You may pay 4 life in addition to any other costs as you cast this spell. If you do, put this card into your hand as it resolves.)
Destroy target non|Sblack creature. It can't be regenerated. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( pick_target(&td, "TARGET_CREATURE") ){
			if( ! is_token(player, card) && can_pay_life(player, 4) ){
				int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
									"Pay Buyback cost", 1, life[player]-4 > 6 ? 10 : 1,
									"Decline", 			1, 5);
				if( choice == 1 ){
					lose_life(player, 4);
					instance->info_slot = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_soltari_visionary(int player, int card, event_t event){
/*
Soltari Visionary |1|W|W
Creature — Soltari Cleric 2/2
Shadow (This creature can block or be blocked by only creatures with shadow.)
Whenever Soltari Visionary deals damage to a player, destroy target enchantment that player controls.
*/
	shadow(player, card, event);

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		APNAP(p,{
					if( times_damaged[p] ){
						target_definition_t td;
						default_target_definition(player, card, &td, TYPE_ENCHANTMENT );
						td.allowed_controller = td.preferred_controller = p;
						td.allow_cancel = 0;

						if( can_target(&td) ){
							char buffer[100];
							if( p == player ){
								scnprintf(buffer, 100, "Select target enchantment you control.");
							}
							else{
								scnprintf(buffer, 100, "Select target enchantment controlled by your opponent.");
							}
							if( new_pick_target(&td, buffer, 0, GS_LITERAL_PROMPT) ){
								kill_card( instance->targets[0].player, instance->targets[0].card, KILL_DESTROY );
							}
						}
						instance->number_of_targets = 0;
					}
				}
		);
	}

	return 0;
}

int card_sonic_burst(int player, int card, event_t event){
	/* Sonic Burst	|1|R	0x20036f8
	 * Instant
	 * As an additional cost to cast this spell, discard a card at random.
	 * ~ deals 4 damage to any target. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST && hand_count[player] > 1 ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 ){
			discard(player, DISC_RANDOM, player);
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_soul_warden(int player, int card, event_t event){
/* CARD_ID_SOUL_WARDEN	1557
Soul Warden	|W
Creature - Human Cleric 1/1
Whenever another creature enters the battlefield, you gain 1 life. */

    test_definition_t this_test;
	default_test_definition(&this_test, TYPE_CREATURE);
	this_test.not_me = 1;
	etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, &this_test, player, card);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 1);
	}

	return 0;
}

int card_spellbook(int player, int card, event_t event){
	/*
	  Spellbook |0
	  Artifact
	  You have no maximum hand size.
	*/
	// Used externally
	if( ! is_humiliated(player, card) ){
		if (event == EVENT_MAX_HAND_SIZE && current_turn == player ){
			event_result = 1000;
		}
	}

	return 0;
}


int card_spellshock(int player, int card, event_t event){
/* CARD_ID_SPELLSHOCK	3565
Spellshock	|2|R
Enchantment
Whenever a player casts a spell, ~ deals 2 damage to that player. */

	if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		damage_player(trigger_cause_controller, 2, player, card);
	}

	return global_enchantment(player, card, event);
}

int card_sphere_of_resistance(int player, int card, event_t event){
/* CARD_ID_SPHERE_OF_RESISTANCE	3566
Sphere of Resistance	|2
Artifact
Spells cost |1 more to cast. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_MODIFY_COST_GLOBAL && ! is_what(affected_card_controller, affected_card, TYPE_LAND) ){
			COST_COLORLESS++;
		}
	}

	return 0;
}

int card_spike_cannibal(int player, int card, event_t event)
{
  /* Spike Cannibal	|1|B|B
   * Creature - Spike 0/0
   * ~ enters the battlefield with a +1/+1 counter on it.
   * When ~ enters the battlefield, move all +1/+1 counters from all creatures onto it. */

  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 1);

  if (comes_into_play(player, card, event))
	{
	  // Carefully not moving counters that are added as a result of removing counters from other creatures from some horrid combination of death triggers
	  int num_counters[2][151] = {{0}};
	  int p, c;
	  for (p = 0; p < 2; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if (in_play(p, c) && is_what(p, c, TYPE_CREATURE))
			num_counters[p][c] = count_counters(p, c, COUNTER_P1_P1);

	  num_counters[player][card] = 0;

	  for (p = 0; p < 2; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if (num_counters[p][c])
			move_counters(player, card, p, c, COUNTER_P1_P1, num_counters[p][c]);
	}

  return 0;
}

int card_spike_hatcher(int player, int card, event_t event){
/* CARD_ID_SPIKE_HATCHER	3568
Spike Hatcher	|6|G
Creature - Spike 0/0
~ enters the battlefield with six +1/+1 counters on it.
|2, Remove a +1/+1 counter from ~: Put a +1/+1 counter on target creature.
|1, Remove a +1/+1 counter from ~: Regenerate ~. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 6);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MOVE_COUNTER = 1,
		CHOICE_REGENERATE
	};

	if( event == EVENT_CAN_ACTIVATE  ){
		if( generic_activated_ability(player, card, event, GAA_REGENERATION | GAA_1_1_COUNTER, MANACOST_X(1), 0, NULL, NULL) ){
			return 99;
		}
		return generic_spike(player, card, event, 6);
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = CHOICE_REGENERATE;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_REGENERATION | GAA_1_1_COUNTER, MANACOST_X(1), 0, NULL, NULL) ){
			return generic_activated_ability(player, card, event, GAA_REGENERATION | GAA_1_1_COUNTER, MANACOST_X(1), 0, NULL, NULL);
		}
		instance->info_slot = CHOICE_MOVE_COUNTER;
		return generic_spike(player, card, event, 6);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_MOVE_COUNTER && valid_target(&td) ){
			add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
		}
		if( instance->info_slot == CHOICE_REGENERATE && can_regenerate(instance->parent_controller, instance->parent_card) ){
			regenerate_target(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_spike_rogue(int player, int card, event_t event){
/* Spike Rogue	|1|G|G
 * Creature - Spike 0/0
 * ~ enters the battlefield with two +1/+1 counters on it.
 * |2, Remove a +1/+1 counter from ~: Put a +1/+1 counter on target creature.
 * |2, Remove a +1/+1 counter from a creature you control: Put a +1/+1 counter on ~. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( has_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			int max_counters = 0;
			CYCLE_ALL_CARDS(p, c, 	{
										if( in_play(p, c) && p == player && is_what(p, c, TYPE_CREATURE) ){
											max_counters += count_counters(p, c, COUNTER_P1_P1);
										}
									};
			);
			while( ! has_mana_for_activated_ability(player, card, MANACOST_X(max_counters*2)) ){
					max_counters--;
			}
			return max_counters;
		}
	}

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	target_definition_t td1;
	base_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller = player;
	td1.preferred_controller = player;
	td1.special = TARGET_SPECIAL_REQUIRES_COUNTER | TARGET_SPECIAL_NOT_ME;
	td1.extra = COUNTER_P1_P1;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MOVE_COUNTERS = 1,
		CHOICE_STEAL_COUNTERS
	};

	if( event == EVENT_CAN_ACTIVATE  ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_1_1_COUNTER, MANACOST_X(2), 0, &td1, NULL) ){
			return 1;
		}
		return generic_spike(player, card, event, 2);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int abils[3] = {0,
						generic_spike(player, card, EVENT_CAN_ACTIVATE, 2),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_1_1_COUNTER, MANACOST_X(2), 0,
													&td1, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Move a counter", 	abils[1], 	5,
							"Steal a counter",	abils[2], 	current_phase == PHASE_AFTER_BLOCKING ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MOVE_COUNTERS ){
			return generic_spike(player, card, event, 2);
		}
		if( choice == CHOICE_STEAL_COUNTERS ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_1_1_COUNTER, MANACOST_X(2), 0,
									&td1, "Select target creature you control with a +1/+1 counter.");
			if( spell_fizzled != 1 ){
				remove_1_1_counter(instance->targets[0].player, instance->targets[0].card);
				instance->number_of_targets = 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_MOVE_COUNTERS ){
			add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
		}
		if( instance->info_slot == CHOICE_STEAL_COUNTERS ){
			add_1_1_counter(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_spike_weaver(int player, int card, event_t event){
/* CARD_ID_SPIKE_WEAVER	3570
Spike Weaver	|2|G|G
Creature - Spike 0/0
~ enters the battlefield with three +1/+1 counters on it.
|2, Remove a +1/+1 counter from ~: Put a +1/+1 counter on target creature.
|1, Remove a +1/+1 counter from ~: Prevent all combat damage that would be dealt this turn. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MOVE_COUNTERS = 1,
		CHOICE_FOG
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_X(1), GVC_COUNTER(COUNTER_P1_P1), NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( current_turn != player && count_subtype(player, TYPE_CREATURE, -1) < count_subtype(1-player, TYPE_CREATURE, -1) ){
			ai_modifier += 15;
		}
		instance->number_of_targets = 0;
		int abils[3] = {0,
						generic_spike(player, card, EVENT_CAN_ACTIVATE, 3),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(1), GVC_COUNTER(COUNTER_P1_P1), NULL, NULL)
		};
		int priority_fog = current_turn != player ? 10 : 1;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Move a counter", 	abils[1], 	5,
							"Fog",				abils[2], 	priority_fog);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MOVE_COUNTERS ){
			return generic_spike(player, card, event, 3);
		}
		if( choice == CHOICE_FOG ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(1), GVC_COUNTER(COUNTER_P1_P1), NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_MOVE_COUNTERS && valid_target(&td) ){
			add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
		}
		if( instance->info_slot == CHOICE_FOG ){
			fog_effect(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_survival_of_the_fittest(int player, int card, event_t event){
/* CARD_ID_SURVIVAL_OF_THE_FITTEST	3572
Survival of the Fittest	|1|G
Enchantment
|G, Discard a creature card: Search your library for a creature card, reveal that card, and put it into your hand. Then shuffle your library. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &this_test) ){
			return generic_activated_ability(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL);
		}
	}
	if( event == EVENT_ACTIVATE ){
		if( instance->info_slot == 1 ){
			ai_modifier -= 256;
		}
		else{
			ai_modifier += (current_turn != player ? 48 : -48);
		}
		int found = 0;
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_hand(p, c) ){
										// Check for cards that have activated or continuous abilities when in your graveyard
										int iid = get_original_internal_card_id(p, c);
										if( cards_data[iid].cc[2] == 9 || cards_data[iid].cc[2] == 2 ){
											found = 1;
											goto end;
										}
									}
								};
		);
		end:
		if( ! found ){
			ai_modifier -= 24;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_G(1)) ){
			if( spell_fizzled != 1 ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_GOOD_TO_PUT_IN_GRAVE, -1, &this_test);
				if( selected != -1 ){
					discard_card(player, selected);
					instance->info_slot = 1;
				}
				else{
					spell_fizzled = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( IS_AI(player) && current_turn == player ){
			this_test.cmc = has_mana(player, COLOR_ANY, 1) + 1;
			this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		}
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	if( event == EVENT_CLEANUP ){
		instance->info_slot = 0;
	}

	return 0;
}

int card_thalakos_drifters(int player, int card, event_t event){
/* CARD_ID_THALAKOS_DRIFTERS	3573
Thalakos Drifters	|2|U|U
Creature - Thalakos 3/3
Discard a card: ~ gains shadow until end of turn. (It can block or be blocked by only creatures with shadow.) */

	if( event == EVENT_ACTIVATE ){
		if( current_phase >= PHASE_DECLARE_BLOCKERS ){
			ai_modifier -= 15;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								0, 0, 0, SP_KEYWORD_SHADOW);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
}

int card_thalakos_scout(int player, int card, event_t event){
/* CARD_ID_THALAKOS_SCOUT	3574
Thalakos Scout	|2|U
Creature - Thalakos Soldier Scout 2/1
Shadow (This creature can block or be blocked by only creatures with shadow.)
Discard a card: Return ~ to its owner's hand. */

   shadow(player, card, event);
   return card_ephemeron(player, card, event);
}

int card_theft_of_dreams(int player, int card, event_t event){
/* CARD_ID_THEFT_OF_DREAMS	3575
Theft of Dreams	|2|U
Sorcery
Draw a card for each tapped creature target opponent controls. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance( player, card );

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.state = STATE_TAPPED;

			draw_cards(player, check_battlefield_for_special_card(player, card, instance->targets[0].player, CBFSC_GET_COUNT, &this_test));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_thrull_surgeon(int player, int card, event_t event){
/* CARD_ID_THRULL_SURGEON	3577
Thrull Surgeon	|1|B
Creature - Thrull 1/1
|1|B, Sacrifice ~: Look at target player's hand and choose a card from it. That player discards that card. Activate this ability only any time you could cast a sorcery. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED|GAA_SACRIFICE_ME|GAA_CAN_TARGET, MANACOST_XB(1, 1), 0,
									&td, "TARGET_PLAYER");
}

int card_treasure_hunter(int player, int card, event_t event){
/* CARD_ID_TREASURE_HUNTER	3579
Treasure Hunter	|2|W
Creature - Human 2/2
When ~ enters the battlefield, you may return target artifact card from your graveyard to your hand. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && reason_for_trigger_controller == player ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT);
		return generic_type_recycler_creature(player, card, event, &this_test);
	}
	return 0;
}

int card_treasure_trove(int player, int card, event_t event){
	/*
	  Treasure Trove |2|U|U
	  Enchantment
	  {2}{U}{U}: Draw a card.
	*/

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_XU(2, 2), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int amount = count_subtype(player, TYPE_LAND, -1);
		if( amount <= 2 || amount >= 6 ){
			ai_modifier += 48;
		}
		if( current_turn != player && current_phase >= PHASE_MAIN2 ){
			ai_modifier += 20;
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_XU(2, 2), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_trasmogrifying_licid(int player, int card, event_t event){
/* CARD_ID_TRANSMOGRIFYING_LICID	3578
Transmogrifying Licid	|3
Artifact Creature - Licid 2/2
|1, |T: ~ loses this ability and becomes an Aura enchantment with enchant creature. Attach it to target creature. You may pay |1 to end this effect.
Enchanted creature gets +1/+1 and is an artifact in addition to its other types. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CHANGE_TYPE && affect_me(player, card) && instance->targets[3].player > 0 ){
		event_result = instance->targets[3].player;
	}

	if( instance->targets[3].card == 66 ){
		if( instance->damage_target_player > -1 ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;

			if( p != instance->targets[2].player || c != instance->targets[2].card ){
				instance->dummy3 = -1;
				instance->targets[2].player = p;
				instance->targets[2].card = c;
			}

			if( ! is_what(-1, get_original_internal_card_id(p, c), TYPE_ARTIFACT) ){
				if( event == EVENT_CHANGE_TYPE && affect_me(p, c) ){
					if( instance->dummy3 == -1 ){
						int newtype = create_a_card_type(get_original_internal_card_id(p, c));
						cards_at_7c7000[newtype]->type |= (cards_data[get_original_internal_card_id(p, c)].type | TYPE_ARTIFACT | TYPE_CREATURE);
						cards_at_7c7000[newtype]->power = get_base_power(p, c);
						cards_at_7c7000[newtype]->toughness = get_base_toughness(p, c);
						instance->dummy3 = newtype;
					}
					event_result = instance->dummy3;
				}
			}

			if( leaves_play(p, c, event) ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}

		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
			reset_subtypes(instance->parent_controller, instance->parent_card, 2);
			parent->dummy3 = parent->damage_target_player = parent->damage_target_card = -1;
			parent->targets[3].player = -1;
			parent->regen_status |= KEYWORD_RECALC_ALL ;
			get_abilities(instance->parent_controller, instance->parent_card, EVENT_CHANGE_TYPE, -1);
		}

		if( event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_CAN_MOVE_AURA || event == EVENT_MOVE_AURA || event == EVENT_RESOLVE_MOVING_AURA ){
			return generic_aura(player, card, event, player, 1, 1, 0, 0, 0, 0, 0);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
		if( parent->targets[3].card == 66 ){
			parent->targets[3].card = 0;
		}
		else{
			if( valid_target(&td) ){
				int newtype = create_a_card_type(parent->internal_card_id);
				cards_at_7c7000[newtype]->type = TYPE_ENCHANTMENT;
				cards_at_7c7000[newtype]->power = 0;
				cards_at_7c7000[newtype]->toughness = 0;
				parent->targets[3].player = newtype;
				add_a_subtype(instance->parent_controller, instance->parent_card, SUBTYPE_AURA_CREATURE);
				attach_aura_to_target(instance->parent_controller, instance->parent_card, event, instance->targets[0].player, instance->targets[0].card);
				parent->regen_status |= KEYWORD_RECALC_ALL ;
				get_abilities(instance->parent_controller, instance->parent_card, EVENT_CHANGE_TYPE, -1);
				parent->targets[3].card = 66;
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_CREATURE");
}

int card_vampire_hounds(int player, int card, event_t event){
/* CARD_ID_VAMPIRE_HOUNDS	3580
Vampire Hounds	|2|B
Creature - Vampire Hound 2/2
Discard a creature card: ~ gets +2/+2 until end of turn. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( has_mana_for_activated_ability(player, card, MANACOST0) ){
			test_definition_t this_test2;
			default_test_definition(&this_test2, TYPE_CREATURE);
			this_test2.zone = TARGET_ZONE_HAND;

			int max_pump = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test2);
			return max_pump * 2;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test2;
	new_default_test_definition(&this_test2, TYPE_CREATURE, "Select a creature card to discard.");
	this_test2.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			return check_battlefield_for_special_card(player, card, player, 0, &this_test2);
		}
	}

	if( event == EVENT_ACTIVATE){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test2);
			if( selected != -1 ){
				discard_card(player, selected);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 2, 2);
	}

	return 0;
}

static int volraths_dungeon_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[0].player > -1 ){
		if( leaves_play(instance->targets[0].player, instance->targets[0].card, event) ){
			kill_card(player, card, KILL_EXILE);
		}

		if( event == EVENT_CAN_ACTIVATE ){
			if( can_use_activated_abilities(instance->targets[0].player, instance->targets[0].card) && current_turn == player ){
				int c1 = get_cost_mod_for_activated_abilities(instance->targets[0].player, instance->targets[0].card, MANACOST0);
				if( has_mana(player, COLOR_COLORLESS, c1) ){
					return can_pay_life(player, 5);
				}
			}
		}

		if( event == EVENT_ACTIVATE ){
			int c1 = get_cost_mod_for_activated_abilities(instance->targets[0].player, instance->targets[0].card, MANACOST0);
			charge_mana(player, COLOR_COLORLESS, c1);
			if( spell_fizzled != 1 ){
				lose_life(player, 5);
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}

	}

	return 0;
}

int card_volraths_dungeon(int player, int card, event_t event){
/*
Volrath's Dungeon |2|B|B
Enchantment
Pay 5 life: Destroy Volrath's Dungeon. Any player may activate this ability but only during his or her turn.
Discard a card: Target player puts a card from his or her hand on top of his or her library.
	Activate this ability only any time you could cast a sorcery.
*/
	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found[2] = {-1};
		CYCLE_ALL_CARDS(p, c, 	{
									if( in_play(p, c) && is_what(p, c, TYPE_EFFECT) ){
										card_instance_t *inst = get_card_instance(p, c);
										if( inst->info_slot == (int)volraths_dungeon_legacy ){
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
			int legacy = create_legacy_effect(1-player, fake, &volraths_dungeon_legacy);
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

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.zone = TARGET_ZONE_PLAYERS;

	enum{
		CHOICE_TARGETED_ABILITY = 1,
		CHOICE_KILL_VD
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_DISCARD | GAA_CAN_SORCERY_BE_PLAYED | GAA_CAN_TARGET, MANACOST0, 0,
									&td1, NULL) )
		{
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_IN_YOUR_TURN, MANACOST0, 5, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = instance->info_slot = 0;
		int abilities[2] = {	generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_DISCARD | GAA_CAN_SORCERY_BE_PLAYED | GAA_CAN_TARGET,
								                           MANACOST0, 0, &td1, NULL),
								generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_IN_YOUR_TURN, MANACOST0, 5, NULL, NULL)
		};
		choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
						"Targeted ability", abilities[0], 2,
						"Destroy Volrath's Dungeon", abilities[1], 1);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_TARGETED_ABILITY ){
			generic_activated_ability(player, card, event, GAA_DISCARD | GAA_CAN_SORCERY_BE_PLAYED | GAA_CAN_TARGET, MANACOST0, 0, &td1, "TARGET_PLAYER");
			if( spell_fizzled != 1 ){
				instance->info_slot = choice;
			}
		}
		if( choice == CHOICE_KILL_VD ){
			generic_activated_ability(player, card, event, GAA_IN_YOUR_TURN, MANACOST0, 5, NULL, NULL);
			if( spell_fizzled != 1 ){
				instance->info_slot = choice;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_TARGETED_ABILITY ){
			if( validate_target(player, card, &td1, 0) && hand_count[instance->targets[0].player] > 0 ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, "Select a card to put on top.");
				new_global_tutor(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_HAND, TUTOR_DECK, 1, AI_MIN_VALUE, &this_test);
			}
		}
		if( instance->info_slot == CHOICE_KILL_VD ){
			kill_card(instance->parent_controller, instance->parent_card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_wall_of_nets(int player, int card, event_t event){
/*
Wall of Nets |1|W|W
Creature — Wall 0/7
Defender (This creature can't attack.)
At end of combat, exile all creatures blocked by Wall of Nets.
When Wall of Nets leaves the battlefield, return all cards exiled with Wall of Nets to the battlefield under their owners' control.
*/
	if( trigger_condition == TRIGGER_LEAVE_PLAY && affect_me(player, card) && reason_for_trigger_controller == player ){
		if( trigger_cause_controller == player && trigger_cause == card ){
			card_instance_t *instance = get_card_instance(player, card);
			instance->targets[1].card = exiledby_detach(player, card);
			if( leaves_play(player, card, event) ){
				get_back_cards_stored_in_exiledby(player, player-2, instance->targets[1].card, GBC_PUT_PERMANENTS_INTO_BF);
			}
		}
	}

	if( trigger_condition == TRIGGER_END_COMBAT && affect_me(player, card) && reason_for_trigger_controller == player ){
		int trig = 0;
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->blocking < 255 ){
			trig = 1;
		}
		if( trig == 1 && end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			if( ! is_token(1-player, instance->blocking) ){
				create_card_name_legacy(player, card, cards_data[get_original_internal_card_id(1-player, instance->blocking)].id);
				exile_card_and_remember_it_on_exiledby(player, card, 1-player, instance->blocking);
			}
			else{
				kill_card(1-player, instance->blocking, KILL_EXILE);
			}
		}
	}

	return 0;
}

int card_welkin_hawk(int player, int card, event_t event){
/* CARD_ID_WELKIN_HAWK	3584
Welkin Hawk	|1|W
Creature - Bird 1/1
Flying
When ~ dies, you may search your library for a card named ~, reveal that card, put it into your hand, then shuffle your library. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		char msg[100] = "Select a Wekin Hawk card.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, msg);
		this_test.id = get_id(player, card);

	   new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);
	}

	return 0;
}

int card_workhorse(int player, int card, event_t event){
/* CARD_ID_WORKHORSE	3586
Workhorse	|6
Artifact Creature - Horse 0/0
~ enters the battlefield with four +1/+1 counters on it.
Remove a +1/+1 counter from ~: Add |C to your mana pool. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 4);

	if( event == EVENT_CAN_ACTIVATE && count_1_1_counters(player, card) > 0 ){
	   return can_produce_mana(player, card);
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && count_1_1_counters(player, card) > 0 && can_produce_mana(player, card)){
		declare_mana_available(player, COLOR_COLORLESS, 1);
	}

	if( event == EVENT_ACTIVATE){
		remove_1_1_counter(player, card);
		produce_mana(player, COLOR_COLORLESS, 1);
	}

	return 0;
}

int card_zealots_en_dal(int player, int card, event_t event){
/*
Zealots en-Dal |3|W	0x200f516
Creature — Human Soldier 2/4
At the beginning of your upkeep, if all nonland permanents you control are white, you gain 1 life.
*/
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_play(p, c) && ! is_what(p, c, TYPE_LAND) ){
										if( !(get_color(p, c) && get_sleighted_color_test(player, card, COLOR_TEST_WHITE)) ){
											return 0;
										}
									}
								}
		);
		gain_life(player, 1);
	}

	return 0;
}

// ---- DOES NOT WORK

int card_limited_resources(int player, int card, event_t event){
	/*
	  Limited Resources |W
	  Enchantment
	  When Limited Resources enters the battlefield, each player chooses five lands he or she controls and sacrifices the rest.
	  Players can't play lands as long as ten or more lands are on the battlefield.
	*/
	if( in_play(player, card) ){
		player_bits[player] |= PB_COUNT_TOTAL_PLAYABLE_LANDS;
		player_bits[1-player] |= PB_COUNT_TOTAL_PLAYABLE_LANDS;

		if( !(land_can_be_played & LCBP_LAND_HAS_BEEN_PLAYED) && landsofcolor_controlled[0][COLOR_ANY]+landsofcolor_controlled[1][COLOR_ANY] >= 10){
			land_can_be_played |= LCBP_LAND_HAS_BEEN_PLAYED;
		}
	}

	if( leaves_play(player, card, event) && lands_played < total_playable_lands(current_turn) ){
		land_can_be_played &= ~LCBP_LAND_HAS_BEEN_PLAYED;
		player_bits[player] &= ~PB_COUNT_TOTAL_PLAYABLE_LANDS;
		player_bits[1-player] &= ~PB_COUNT_TOTAL_PLAYABLE_LANDS;
	}

	if( comes_into_play(player, card, event) ){
		APNAP(p, {
					int amount = count_subtype(p, TYPE_LAND, -1);
					if( amount > 5 ){
						impose_sacrifice(player, card, p, amount-5, TYPE_LAND, 0, 0, 0, 0, 0, 0, 0, -1, 0);
					}
				};
			);
	}

	return global_enchantment(player, card, event);
}




