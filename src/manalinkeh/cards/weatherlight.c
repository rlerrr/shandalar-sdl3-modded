#include "manalink.h"

static int angelic_renewal_effect(int player, int card, event_t event);

// ---- GLOBAL FUNCTION

int spatial_binding_legacy(int player, int card, event_t event);

static char *is_blocking_master_of_arms(int who_chooses, int player, int card, int targeting_player,
                                        int targeting_card);
static int sawtooth_ogre_legacy(int player, int card, event_t event);

#ifndef SHANDALAR

static char *is_already_damaged(int who_chooses, int player, int card, int unused_source_player, int unused_source_card);

#endif

#ifndef SHANDALAR

int card_mischievous_poltergeist(int player, int card, event_t event);

#endif

static int remove_top_creature_card_from_grave( int player ){
	const int *grave = get_grave(player);
	int count = count_graveyard(player)-1;
	while(count > -1){
			if( cards_data[grave[count]].type & TYPE_CREATURE ){
				rfg_card_from_grave(player, count);
				return 1;
			}
			count--;
	}
	return 0;
}

static int generic_creature_with_discard_type_on_cip(int player, int card, event_t event, int selected_type){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int kill_me = 1;
		if( hand_count[player] ){
			test_definition_t test;
			default_test_definition(&test, selected_type);
			char ttext[100];
			scnprintf(test.message, 100, "Select a %s card to discard.", type_text(ttext, 100, selected_type, 0));
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &test);
			if( selected > -1 ){
				discard_card(player, selected);
				kill_me = 0;
			}
		}
		if( kill_me ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, selected_type);
		test.zone = TARGET_ZONE_HAND;
		if( ! check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test) ){
			ai_modifier -= 256;
		}
	}

	return 0;
}

/* ManalinkEh 2022: 0x22e9b50; original cards/weatherlight.c:54. */
static int abduction_effect(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (!resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    return 0;
  }
  int owner = instance->targets[0].player;
  seek_grave_for_id_to_reanimate(owner, card, owner,
                                 (unsigned int)cards_data[instance->targets[0].card].id,
                                 REANIMATE_DEFAULT);
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x22eb060; original cards/weatherlight.c:66. */
int card_abduction(int player, int card, event_t event)
{
  /* Abduction	|2|U|U
   * Enchantment - Aura
   * Enchant creature
   * When this Aura enters, untap enchanted creature.
   * You control enchanted creature.
   * When enchanted creature dies, return that card to the battlefield under its owner's control. */

  char attached_player;
  card_instance_t *instance;
  card_instance_t *source_or_legacy;
  int result;

  instance = get_card_instance(player, card);
  source_or_legacy = in_play(player, card);
  if (source_or_legacy != NULL)
  {
    attached_player = instance->damage_target_player;
    if ((event == EVENT_GRAVEYARD_FROM_PLAY) && (attached_player != -1))
    {
      if ((instance->damage_target_card == affected_card) &&
          (attached_player == affected_card_controller))
      {
        result = is_this_dying((int)attached_player, instance->damage_target_card);
        if (result != 0)
        {
          result = check_for_death_trigger_removal(affected_card_controller, affected_card);
          if (result == 0)
          {

            result = create_legacy_effect(player, card, abduction_effect);
            source_or_legacy = get_card_instance(player, result);
            result = get_owner((int)instance->damage_target_player, instance->damage_target_card);
            source_or_legacy->targets[0].player = result;
            result = get_original_internal_card_id((int)instance->damage_target_player,
                                                   instance->damage_target_card);
            source_or_legacy->targets[0xb].player = 1;
            source_or_legacy->targets[0].card = result;
          }
        }
      }
      etb_ability(player, card, EVENT_GRAVEYARD_FROM_PLAY, RESOLVE_TRIGGER_MANDATORY);
      return card_control_magic(player, card, event);
    }
  }
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    untap_card((int)instance->damage_target_player, instance->damage_target_card);
  }
  return card_control_magic(player, card, event);
}

// ---- CARDS

int card_abeyance(int player, int card, event_t event)
{
  /*CARD_ID_ABEYANCE	2922
  Abeyance	|1|W
  Instant
  Until end of turn, target player can't cast instant or sorcery spells, and that player can't
  activate abilities that aren't mana abilities. Draw a card.*/

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;

  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
  {
    if (current_phase <= PHASE_MAIN1)
    {
      ai_modifier += 24;
    }
    if (current_turn == 1 - player)
    {
      ai_modifier += 24;
    }
  }

  if (event == EVENT_RESOLVE_SPELL)
  {
    if (validate_target(player, card, &td, 0))
    {
      target_player_cant_cast_type(player, card, instance->targets[0].player,
                                   TYPE_SORCERY | TYPE_INSTANT);
      draw_cards(player, 1);
    }
    kill_card(player, card, KILL_DESTROY);
  }

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_abjure(int player, int card, event_t event){
/* CARD_ID_ABJURE	2923
Abjure	|U
Instant
As an additional cost to cast ~, sacrifice a |Sblue permanent.
Counter target spell. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a blue permanent to sacrifice.");
	test.color = COLOR_TEST_BLUE;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		if( counterspell(player, card, event, NULL, 0) ){
			if( new_can_sacrifice_as_cost(player, card, &test) ){
				return 99;
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		counterspell(player, card, event, NULL, 0);
		if( spell_fizzled != 1 ){
			instance->info_slot = get_cmc(BYTE2(sac), BYTE3(sac));
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
		else{
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		return counterspell(player, card, event, NULL, 0);
	}

   return 0;
}

static int can_have_m1_m1_counters(int player, int card, int number_of_age_counters)
{
  return !check_battlefield_for_id(player, CARD_ID_MELIRA_SYLVOK_OUTCAST);
}

static int add_m1_m1_counters_thunk(int player, int card, int number_of_age_counters)
{
  add_counters(player, card, player, card, COUNTER_M1_M1, number_of_age_counters);
  return 1;
}

int card_aboroth(int player, int card, event_t event){

  /* Aboroth	|4|G|G
   * Creature - Elemental 9/9
   * Cumulative upkeep-Put a -1/-1 counter on ~. */

  cumulative_upkeep_general(player, card, event, can_have_m1_m1_counters, add_m1_m1_counters_thunk);
  return 0;
}

int card_abyssal_gatekeeper(int player, int card, event_t event){
/* CARD_ID_ABYSSAL_GATEKEEPER	2925
Abyssal Gatekeeper	|1|B
Creature - Horror 1/1
When ~ dies, each player sacrifices a creature. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		APNAP(p, { new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test); };);
	}

	return 0;
}

int card_agonizing_memories(int player, int card, event_t event){

	/* Agonizing Memories	|2|B|B
	 * Sorcery
	 * Look at target player's hand and choose two cards from it. Put them on top of that player's library in any order. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			ec_definition_t ec;
			default_ec_definition(instance->targets[0].player, player, &ec);
			ec.effect = EC_PUT_ON_TOP;
			ec.qty = 2;

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			new_effect_coercion(&ec, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_alabaster_dragon(int player, int card, event_t event)
{
/* CARD_ID_ALABASTER_DRAGON	2927
Alabaster Dragon	|4|W|W
Creature - Dragon 4/4
Flying
When ~ dies, shuffle it into its owner's library. */

	  int owner, position;
  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY)
	  && find_in_owners_graveyard(player, card, &owner, &position))
	from_graveyard_to_deck(owner, position, 3);

  return 0;
}

int card_alms2(int player, int card, event_t event){
/* CARD_ID_ALMS	2928
Alms	|W
Enchantment
|1, Exile the top card of your graveyard: Prevent the next 1 damage that would be dealt to target creature this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( count_graveyard(player) > 0 ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_X(1), 0, &td, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_X(1), 0, &td, "TARGET_DAMAGE");
		if(	spell_fizzled != 1 ){
			rfg_card_from_grave(player, count_graveyard(player)-1);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
		}
	}

	return 0;
}

int card_ancestral_knowledge(int player, int card, event_t event)
{
  /* Ancestral Knowledge	|1|U
   * Enchantment
   * Cumulative upkeep |1
   * When ~ enters the battlefield, look at the top ten cards of your library, then exile any number of them and put the rest back on top of your library in
   * any order.
   * When ~ leaves the battlefield, shuffle your library. */

  cumulative_upkeep(player, card, event, 1, 0, 0, 0, 0, 0);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    int *deck = deck_ptr[player];
    int amount = count_deck(player);
    if (amount > 10)
      amount = 10;
    if (ai_is_speculating != 1 && (player != AI || ((unsigned char)trace_mode & 2)))
    {
      while (amount != 0)
      {
        int selected = show_deck(player, deck, amount, "Choose a card to remove", 0,
                                 gs_done_008b40e0);
        if (selected == -1)
          break;
        rfg_card_in_deck(player, selected);
        --amount;
      }
    }
    rearrange_top_x(player, player, amount);
  }
  leaves_play(player, card, event, 1, 1);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
    shuffle(player);
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x22e9d40; original cards/weatherlight.c:352. */
static int angelic_renewal_effect(int player, int card, event_t event)
{
  int *grave;
  card_instance_t *instance;
  int selection_count_or_result;
  int grave_index_or_iid;
  int matching_grave_index;
  card_instance_t *dying;
  target_t *stored_death_cursor;
  int stored_or_grave_iid;
  int added[2][18];
  test_definition_t this_test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (selection_count_or_result =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           selection_count_or_result != 0xb72))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (selection_count_or_result = effect_follows_control_of_attachment(player, card, 0x4700),
         selection_count_or_result != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (selection_count_or_result = effect_follows_control_of_attachment(player, card, event),
         selection_count_or_result != 0))
    {
      return 0;
    }
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {

      instance = get_card_instance(player, card);
      dying = in_play(affected_card_controller, affected_card);
      if ((dying != NULL) &&
          (selection_count_or_result = is_this_dying(affected_card_controller, affected_card),
           selection_count_or_result != 0))
      {

        if ((instance->damage_target_card == affected_card) &&
            (instance->damage_target_player == affected_card_controller))
        {
          instance->damage_target_card = -1;
          instance->damage_target_player = -1;
          remove_status(player, card, 0x10000);
          if (instance->targets[0xb].player < 0)
          {
            instance->targets[0xb].player = 0;
          }
          SET_BYTE2(instance->targets[0xb].player) = 1;
        }
        count_for_gfp_ability_and_store_values(player, card, 0x77, player, 2, NULL, 0x70, 1);
      }
    }
  }
  selection_count_or_result = duh_mode(player);
  selection_count_or_result = resolve_gfp_ability(
      player, card, event, (selection_count_or_result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if ((selection_count_or_result != 0) &&
      (selection_count_or_result = check_status(player, card, 0x10000),
       selection_count_or_result == 0))
  {

    instance = get_card_instance(player, card);
    state_untargettable(player, card, 1);
    stored_death_cursor = instance->targets + 1;
    selection_count_or_result = 0;
    do
    {
      if (stored_death_cursor->player == -1)
      {
        stored_or_grave_iid = stored_death_cursor->card;
      }
      else
      {

        grave = graveyard_ptr_mutable[player];
        stored_or_grave_iid = *grave;
        if (stored_or_grave_iid != -1)
        {
          grave_index_or_iid = 0;
          do
          {
            if (stored_death_cursor->player == stored_or_grave_iid)
            {
              if (grave_index_or_iid != -1)
              {
                stored_or_grave_iid = grave[grave_index_or_iid];
                added[1][selection_count_or_result] = grave_index_or_iid;
                added[0][selection_count_or_result] = stored_or_grave_iid;
                selection_count_or_result = selection_count_or_result + 1;
              }
              break;
            }
            grave_index_or_iid = grave_index_or_iid + 1;
            stored_or_grave_iid = grave[grave_index_or_iid];
          } while (stored_or_grave_iid != -1);
        }
        stored_or_grave_iid = stored_death_cursor->card;
        stored_death_cursor->player = -1;
      }
      if (stored_or_grave_iid != -1)
      {

        grave = graveyard_ptr_mutable[player];
        grave_index_or_iid = *grave;
        if (grave_index_or_iid != -1)
        {
          matching_grave_index = 0;
          do
          {
            if (stored_or_grave_iid == grave_index_or_iid)
            {
              if (matching_grave_index != -1)
              {
                stored_or_grave_iid = grave[matching_grave_index];
                added[1][selection_count_or_result] = matching_grave_index;
                added[0][selection_count_or_result] = stored_or_grave_iid;
                selection_count_or_result = selection_count_or_result + 1;
              }
              break;
            }
            matching_grave_index = matching_grave_index + 1;
            grave_index_or_iid = grave[matching_grave_index];
          } while (grave_index_or_iid != -1);
        }
      }
      stored_death_cursor->card = -1;
      stored_death_cursor = stored_death_cursor + 1;
    } while (stored_death_cursor != instance->targets + 10);
    if (selection_count_or_result != 0)
    {

      new_default_test_definition(&this_test, 2, "Select a creature to reanimate.");
      selection_count_or_result = select_card_from_zone(
          player, player, added[0], selection_count_or_result, 0, 2, -1, &this_test);
      if (selection_count_or_result != -1)
      {
        reanimate_permanent(player, -1, player, added[1][selection_count_or_result],
                            REANIMATE_DEFAULT);
        kill_card(player, card, KILL_SACRIFICE);
      }
    }
    state_untargettable(player, card, 0);
    kill_card(player, card, KILL_EXILE);
  }
  if ((event == EVENT_CLEANUP) && (selection_count_or_result = check_status(player, card, 0x10000),
                                   selection_count_or_result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

int card_angelic_renewal(int player, int card, event_t event){
/* CARD_ID_ANGELIC_RENEWAL	2930
Angelic Renewal	|1|W
Enchantment
Whenever a creature is put into your graveyard from the battlefield, you may sacrifice ~. If you do, return that card to the battlefield. */
	if (event == EVENT_STATIC_EFFECTS && affect_me(player, card)){
        int found = 0;
        for (int c = 0; c < active_cards_count[player]; ++c){
            if (in_play(player, c) && is_what(player, c, TYPE_EFFECT)){
                card_instance_t* legacy = get_card_instance(player, c);
                if (legacy->info_slot == (int)angelic_renewal_effect
                    && ((legacy->damage_target_player == player && legacy->damage_target_card == card)
                        || (legacy->targets[0].player == player && legacy->targets[0].card == card))){
                    found = 1;
                    break;
                }
            }
        }
        if (!found){
            int leg = create_targetted_legacy_effect(player, card, angelic_renewal_effect, player, card);
            card_instance_t* legacy = get_card_instance(player, leg);
            legacy->targets[0].player = player;
            legacy->targets[0].card = card;
            legacy->number_of_targets = 1;
            add_status(player, leg, STATUS_INVISIBLE_FX);
        }
    }

	count_for_gfp_ability_and_store_values(player, card, event, player, TYPE_CREATURE, NULL, GFPC_TRACK_DEAD_CREATURES | GFPC_INTO_WHOSE_GRAVE | GFPC_EXTRA_SKIP_TOKENS, 0);

	if( resolve_gfp_ability(player, card, event, duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL) ){
		card_instance_t *instance = get_card_instance(player, card);
		state_untargettable(player, card, 1); //Prevent unwanted interactions
		int added[2][20];
		int ac = 0;
		int k;
		for(k=0; k<10; k++){
			if( instance->targets[k].player != -1 ){
				int count = 0;
				int result = -1;
				while( get_grave(player)[count] != -1 ){
						if( get_grave(player)[count] == instance->targets[k].player ){
							result = count;
							break;
						}
						count++;
				}
				if( result != -1 ){
					added[0][ac] = get_grave(player)[result];
					added[1][ac] = result;
					ac++;
				}
				instance->targets[k].player = -1;
			}
			if( instance->targets[k].card != -1 ){
				int count = 0;
				int result = -1;
				while( get_grave(player)[count] != -1 ){
						if( get_grave(player)[count] == instance->targets[k].card ){
							result = count;
							break;
						}
						count++;
				}
				if( result != -1 ){
					added[0][ac] = get_grave(player)[result];
					added[1][ac] = result;
					ac++;
				}
			}
			instance->targets[k].card = -1;
		}
		if( ac ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature to reanimate.");
			int selected = select_card_from_zone(player, player, added[0], ac, 0, AI_MAX_CMC, -1, &this_test);
			if( selected != -1 ){
				reanimate_permanent(player, -1, player, added[1][selected], REANIMATE_DEFAULT);
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
		instance->targets[11].player = 0;
		state_untargettable(player, card, 0);
	}

	return global_enchantment(player, card, event);
}

int card_argivian_find(int player, int card, event_t event){
/* CARD_ID_ARGIVIAN_FIND	2933
Argivian Find	|W
Instant
Return target artifact or enchantment card from your graveyard to your hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ENCHANTMENT | TYPE_ARTIFACT, "Select an Artifact or Enchantment card.");

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
		if( spell_fizzled != 1 ){
			card_instance_t *instance = get_card_instance(player, card);
			int iid = get_grave(player)[instance->targets[0].player];
			ai_modifier += cards_ptr[iid]->ai_base_value;
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			const int *grave = get_grave(player);
			add_card_to_hand(player, grave[selected]);
			remove_card_from_grave(player, selected);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
}

int card_argivian_restoration(int player, int card, event_t event){
  /* Argivian Restoration |2|U|U
   * Sorcery
   * Return target artifact card from your graveyard to the battlefield. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select target artifact card.");
	test.ai_selection_mode = AI_MAX_CMC;

	if( event == EVENT_RESOLVE_SPELL){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
}

int card_aether_flash(int player, int card, event_t event){
/* CARD_ID_AETHER_FLASH	2935
Aether Flash	|2|R|R
Enchantment
Whenever a creature enters the battlefield, ~ deals 2 damage to it. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);

		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		damage_creature(trigger_cause_controller, trigger_cause, 2, player, card);
	}

	return global_enchantment(player, card, event);
}

int card_aura_of_silence(int player, int card, event_t event){
/* CARD_ID_AURA_OF_SILENCE	2936
Aura of Silence	|1|W|W
Enchantment
Artifact and enchantment spells your opponents cast cost |2 more to cast.
Sacrifice ~: Destroy target artifact or enchantment. */

	if( event == EVENT_MODIFY_COST_GLOBAL && in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( affected_card_controller == 1-player && is_what(affected_card_controller, affected_card, TYPE_ARTIFACT | TYPE_ENCHANTMENT) &&
			! is_what(affected_card_controller, affected_card, TYPE_LAND) )
		{
			COST_COLORLESS += 2;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td, "DISENCHANT");
}

int card_barishi(int player, int card, event_t event){
/* CARD_ID_BARISHI	2938
Barishi	|2|G|G
Creature - Elemental 4/3
When ~ dies, exile ~, then shuffle all creature cards from your graveyard into your library. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		exile_from_owners_graveyard(player, card);
		int count = count_graveyard(player)-1;
		const int *grave = get_grave(player);
		while(count > -1){
			if( cards_data[grave[count]].type & TYPE_CREATURE ){
				from_graveyard_to_deck(player, count, 2);
			}
			count--;
		}
		shuffle(player);
	}

	return 0;
}

int card_barrow_ghoul(int player, int card, event_t event){

	/* Barrow Ghoul	|1|B
	 * Creature - Zombie 4/4
	 * At the beginning of your upkeep, sacrifice ~ unless you exile the top creature card of your graveyard. */
	// Used externally
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int kill = 1;
		if( any_in_graveyard_by_type(player, TYPE_CREATURE) ){
			int choice = do_dialog(player, player, card, -1, -1, " Pay upkeep\n Decline", 0);
			if( choice == 0 && remove_top_creature_card_from_grave(player) ){
				kill = 0;
			}
		}

		if( kill != 0 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_bethrothed_of_fire(int player, int card, event_t event)
{
  /* Betrothed of Fire	|1|R
   * Enchantment - Aura
   * Enchant creature
   * Sacrifice an untapped creature: Enchanted creature gets +2/+0 until end of turn.
   * Sacrifice enchanted creature: Creatures you control get +2/+0 until end of turn. */

  int choice;
  char attached_player;
  int attached_card_or_result;
  card_instance_t *instance;
  int controller_or_available;
  int untapped_creature_count;
  int pump_limit_or_result;
  int sacrificable_count_or_result;
  uint32_t other_sacrifice_priority;
  int sac;
  test_definition_t test;
  if (event == EVENT_CHECK_ABILITIES)
  {
    instance = get_card_instance(player, card);
    attached_player = instance->damage_target_player;
    attached_card_or_result = instance->damage_target_card;
    instance = in_play(player, card);
    controller_or_available = affected_card_controller;
    if (instance != NULL)
    {
      if (player == attached_player && attached_card_or_result == affected_card &&
          player == affected_card_controller)
      {
        default_test_definition(&test, 2);
        test.state = 0x10;
        test.state_flag = 1;
        sacrificable_count_or_result = max_can_sacrifice_as_cost(player, card, &test);
        untapped_creature_count = count_untapped_subtype(player, 2, -1);
        pump_limit_or_result = untapped_creature_count + -1;
        if (sacrificable_count_or_result < untapped_creature_count + -1)
        {
          pump_limit_or_result = sacrificable_count_or_result;
        }
        if (pump_limit_or_result <= 0)
          return 0;
        controller_or_available =
            generic_shade_amt_can_pump(controller_or_available, attached_card_or_result, 2, 0, 0, 0,
                                       0, 0, 0, 0, pump_limit_or_result);
        check_abilities_power = check_abilities_power + controller_or_available;
      }
      if (((affected_card_controller == player) &&
           (attached_card_or_result =
                can_sacrifice_this_as_cost((int)attached_player, attached_card_or_result),
            attached_card_or_result != 0)) &&
          ((attached_card_or_result = is_what(affected_card_controller, affected_card, 2),
            attached_card_or_result != 0 &&
                ((attached_card_or_result = is_humiliated(player, card, AL_STATIC),
                  attached_card_or_result == 0 &&
                      (attached_card_or_result = generic_activated_ability(
                           player, card, EVENT_CHECK_ABILITIES, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL),
                       attached_card_or_result != 0))))))
      {
        check_abilities_power = check_abilities_power + 2;
      }
    }
    if (is_humiliated(player, card, AL_NM_NL_ACT) != 0)
      return 0;
    instance = get_card_instance(player, card);
    if (-1 < instance->damage_target_player)
    {
      get_card_instance(player, card);
      new_default_test_definition(&test, 2, "Select an untapped creature to sacrifice.");
      test.state = 0x10;
      test.state_flag = 1;
    }
    return 0;
  }
  attached_card_or_result = is_humiliated(player, card, AL_NM_NL_ACT);
  if ((attached_card_or_result == 0) &&
      (instance = get_card_instance(player, card), -1 < instance->damage_target_player))
  {
    instance = get_card_instance(player, card);
    controller_or_available = (int)instance->damage_target_player;
    attached_card_or_result = instance->damage_target_card;
    new_default_test_definition(&test, 2, "Select an untapped creature to sacrifice.");
    test.state = 0x10;
    test.state_flag = 1;
    if (event == EVENT_CAN_ACTIVATE)
    {
      pump_limit_or_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0,
                                                       0, 0, 0, 0, NULL, NULL);
      if ((pump_limit_or_result != 0) &&
          ((pump_limit_or_result = new_can_sacrifice_as_cost(player, card, &test),
            pump_limit_or_result != 0 || (attached_card_or_result = can_sacrifice_this_as_cost(
                                              controller_or_available, attached_card_or_result),
                                          attached_card_or_result != 0))))
      {
        return 1;
      }
    }
    else if (event == EVENT_ACTIVATE)
    {
      pump_limit_or_result = new_can_sacrifice_as_cost(player, card, &test);
      controller_or_available =
          can_sacrifice_this_as_cost(controller_or_available, attached_card_or_result);
      sacrificable_count_or_result = is_attacking(player, card);
      if ((sacrificable_count_or_result == 0) ||
          (sacrificable_count_or_result = is_unblocked(player, card),
           sacrificable_count_or_result == 0))
      {
        other_sacrifice_priority = 1;
      }
      else
      {
        other_sacrifice_priority = 10;
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Sac another creature",
                           pump_limit_or_result, other_sacrifice_priority, "Sac enchanted creature",
                           controller_or_available, 5, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == 1)
      {
        sac = new_sacrifice(player, card, player, SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK,
                            &test);
      }
      else
      {
        if (choice != 2)
        {
          spell_fizzled = 1;
          return 0;
        }
        sac = attached_card_or_result << 0x18;
      }
      if (sac == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        state_untargettable(player, (unsigned int)sac >> 0x18, 0);
        return 0;
      }
      kill_card(player, (unsigned int)sac >> 0x18, KILL_SACRIFICE);
    }
    else
    {
      if (event == EVENT_RESOLVE_ACTIVATION)
      {
        pump_limit_or_result = instance->info_slot;
        if ((-1 < controller_or_available) && (pump_limit_or_result == 1))
        {
          pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                        controller_or_available, attached_card_or_result, 2, 0);
          pump_limit_or_result = instance->info_slot;
        }
        if (pump_limit_or_result == 2)
        {
          pump_subtype_until_eot(player, card, player, -1, 2, 0, 0, 0);
        }
      }
    }
  }
  if (event == EVENT_CAN_CAST ||
      (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) ||
      event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET ||
      event == EVENT_CHANGE_TARGET ||
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CAN_MOVE_AURA || (uint32_t)(event - EVENT_POWER) < 3 ||
      event == EVENT_RESOLVE_MOVING_AURA || event == EVENT_CHANGE_TYPE ||
      event == EVENT_SET_COLOR || event == EVENT_GET_SELECTED_CARD || event == EVENT_STATIC_EFFECTS)
    return vanilla_aura(player, card, event, player);
  return 0;
}

int card_blossoming_wreath(int player, int card, event_t event){
/* CARD_ID_BLOSSOMING_WREATH	2945
Blossoming Wreath	|G
Instant
You gain life equal to the number of creature cards in your graveyard. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( life[player] < 6 ){
			ai_modifier += (count_graveyard_by_type(player, TYPE_CREATURE) * 12);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, count_graveyard_by_type(player, TYPE_CREATURE));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_bone_dancer(int player, int card, event_t event){
/* CARD_ID_BONE_DANCER	2948
Bone Dancer	|1|B|B
Creature - Zombie 2/2
Whenever ~ attacks and isn't blocked, you may put the top creature card of defending player's graveyard onto the battlefield under your control.
If you do, ~ assigns no combat damage this turn. */

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->info_slot = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && is_attacking(player, card) && is_unblocked(player, card) &&
		current_phase == PHASE_AFTER_BLOCKING && instance->info_slot != 66 && ! is_humiliated(player, card, AL_TRIG) &&
		count_graveyard_by_type(1-player, TYPE_CREATURE) )
	{
		return 1;
	}

	if( event == EVENT_ACTIVATE){
		instance->info_slot = 66;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		negate_combat_damage_this_turn(player, instance->parent_card, player, instance->parent_card, 0);

		const int *grave = get_grave(1-player);
		int count = count_graveyard(1-player)-1;
		while(count > -1){
				if( cards_data[grave[count]].type & TYPE_CREATURE ){
					reanimate_permanent(player, card, 1-player, count, REANIMATE_DEFAULT);
					break;
				}
				count--;
		}
	}


	if( instance->info_slot == 66 && current_phase == PHASE_UPKEEP ){
		instance->info_slot = 0;
	}

	return 0;
}

static int bosium_strip_legacy(int player, int card, event_t event)
{
  if (event == EVENT_CLEANUP )
	kill_card(player, card, KILL_EXILE);

  if (!IS_ACTIVATING(event))
	return 0;

  if (event == EVENT_CAN_ACTIVATE)
	{
		const int *grave = get_grave(player);
		int top_card = count_graveyard(player) - 1;
		if( grave[0] != -1 && is_what(-1, grave[top_card], TYPE_SPELL) )
			if( has_mana_to_cast_iid(player, event, grave[top_card]) )
				return can_legally_play_iid(player, grave[top_card]);
	}

  if (event == EVENT_ACTIVATE )
	{
		const int *grave = get_grave(player);
		int top_card = count_graveyard(player) - 1;
		int id = cards_data[grave[top_card]].id;
		if( charge_mana_from_id(player, -1, event, id) )
		  {
			play_card_in_grave_for_free_and_exile_it(player, player, top_card);
			cant_be_responded_to = 1;	// already had opportunity to respond to the spell; no response to this fake activation
		  }
	}

  return 0;
}

int card_bosium_strip(int player, int card, event_t event){
/* CARD_ID_BOSIUM_STRIP	2943
Bosium Strip	|3
Artifact
|3, |T: Until end of turn, if the top card of your graveyard is an instant or sorcery card, you may cast that card.
If a card cast this way would be put into a graveyard this turn, exile it instead. */

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (count_subtype(player, TYPE_LAND, -1) * 8);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		create_legacy_activate(instance->parent_controller, instance->parent_card, &bosium_strip_legacy);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
}

int card_briar_shield(int player, int card, event_t event){
/* CARD_ID_BRIAR_SHIELD	2949
Briar Shield	|G
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+1.
Sacrifice ~: Enchanted creature gets +3/+3 until end of turn. */

	if (event == EVENT_CHECK_ABILITIES){
		card_instance_t* instance = get_card_instance(player, card);
		if (affect_me(instance->damage_target_player, instance->damage_target_card) && instance->damage_target_player == player &&
			in_play(player, card) && can_sacrifice_this_as_cost(player, card)){
			check_abilities_power += 2;
			check_abilities_toughness += 2;
		}
	}
	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE  ){
			return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_until_eot(player, card, instance->damage_target_player, instance->damage_target_card, 3, 3);
	}
	return generic_aura(player, card, event, player, 1, 1, 0, 0, 0, 0, 0);
}

int card_bubble_matrix(int player, int card, event_t event ){
/* CARD_ID_BUBBLE_MATRIX	2950
Bubble Matrix	|4
Artifact
Prevent all damage that would be dealt to creatures. */

	if( event == EVENT_PREVENT_DAMAGE && in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *source = get_card_instance(affected_card_controller, affected_card);
		if( damage_card == source->internal_card_id && source->damage_target_card > -1 && source->info_slot > 0 ){
			if( is_what(source->damage_target_player, source->damage_target_card, TYPE_CREATURE) ){
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
	}

	return 0;
}

int card_buried_alive(int player, int card, event_t event){
/* CARD_ID_BURIED_ALIVE	1528
Buried Alive	|2|B
Sorcery
Search your library for up to three creature cards and put them into your graveyard. Then shuffle your library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 48;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		char msg[100] = "Select a creature card to put into grave.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, msg);
		this_test.qty = 3;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_GRAVE, 0, AI_GOOD_TO_PUT_IN_GRAVE, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_call_of_the_wild(int player, int card, event_t event)
{
  /* Call of the Wild	|2|G|G
   * Enchantment
   * |2|G|G: Reveal the top card of your library. If it's a creature card, put it onto the battlefield. Otherwise, put it into your graveyard. */

  // 0x404fc0



	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if (event == EVENT_RESOLVE_ACTIVATION && deck_ptr[player][0] != -1)
	{
	  reveal_card_iid(player, card, deck_ptr[player][0]);

	  if (is_what(-1, deck_ptr[player][0], TYPE_CREATURE))
		put_into_play_a_card_from_deck(player, player, 0);
	  else
		mill(player, 1);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XG(2,2), 0, NULL, NULL);
}

int card_chimeric_sphere(int player, int card, event_t event){
/* CARD_ID_CHIMERIC_SPHERE	2951
Chimeric Sphere	|3
Artifact
|2: Until end of turn, ~ becomes a 2/1 Construct artifact creature with flying.
|2: Until end of turn, ~ becomes a 3/2 Construct artifact creature and loses flying. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_2_1_FLYING = 1,
		CHOICE_3_2
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_X(2), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.required_abilities = KEYWORD_FLYING;
		td.illegal_abilities = 0;
		int priority_2_1_flying = can_target(&td) ? 1 : 10;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"2/1 with Flying", 1, priority_2_1_flying,
							"3/2 without Flying", 1, 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_X(2), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int pow = instance->info_slot == CHOICE_2_1_FLYING ? 2 : 3;
		int tou = instance->info_slot == CHOICE_2_1_FLYING ? 1 : 2;
		int key = instance->info_slot == CHOICE_2_1_FLYING ? KEYWORD_FLYING : 0;
		int legacy = artifact_animation(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
							1, pow, tou, key, 0);
		get_card_instance(player, legacy)->targets[9].card = SUBTYPE_CONSTRUCT;
	}

	return 0;
}

int card_circling_vultures(int player, int card, event_t event){

	/* Circling Vultures	|B
	 * Creature - Bird 3/2
	 * Flying
	 * You may discard ~ any time you could cast an instant.
	 * At the beginning of your upkeep, sacrifice ~ unless you exile the top creature card of your graveyard. */

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND ){
		return 1;
	}
	if( event == EVENT_ACTIVATE_FROM_HAND ){
		discard_card(player, card);
		return 1;
	}

	return card_barrow_ghoul(player, card, event);
}

int card_cinder_wall(int player, int card, event_t event){
/* CARD_ID_CINDER_WALL	993
Cinder Wall	|R
Creature - Wall 3/3
Defender
When ~ blocks, destroy it at end of combat. */

	if( ! is_humiliated(player, card, AL_TRIG) ){
		if( blocking(player, card, event) ){
			get_card_instance(player, card)->info_slot = 66;
		}
		if( get_card_instance(player, card)->info_slot == 66 && end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			kill_card(player, card, KILL_DESTROY);
		}
	}
	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->info_slot = 0;
	}
	return 0;
}

int card_debt_of_loyalty(int player, int card, event_t event){
/* CARD_ID_DEBT_OF_LOYALTY	2958
Debt of Loyalty	|1|W|W
Instant
Regenerate target creature. You gain control of that creature if it regenerates this way. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target(instance->targets[0].player, instance->targets[0].card);
			if( player != instance->targets[0].player ){
				gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_REGENERATION, &td, "TARGET_CREATURE", 1, NULL);
}

int card_dense_foliage(int player, int card, event_t event)
{
  /* Dense Foliage	|2|G
   * Enchantment
   * Creatures can't be the targets of spells. */

  /*
	if( event == EVENT_RESOLVE_SPELL ){
		set_special_flags2(player, -1, SF2_DENSE_FOLIAGE);
	}

	if( leaves_play(player, card, event, RESOLVE_TRIGGER_MANDATORY, 1) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT);
		this_test.id = get_id(player, card);
		this_test.not_me = 1;
		if( ! check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			remove_special_flags2(player, -1, SF2_DENSE_FOLIAGE);
		}
	}

	if( event == EVENT_CAST_SPELL && affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
		set_special_flags2(affected_card_controller, affected_card, SF2_DENSE_FOLIAGE);
	}
	*/

if (event == EVENT_ABILITIES && is_what(affected_card_controller, affected_card, TYPE_CREATURE))
    special_abilities2(affected_card_controller, affected_card, event, SP_KEYWORD2_GRANTED_PROT_MISC);
  else if (event == EVENT_PROT_MISC && get_card_instance(affected_card_controller, affected_card) &&
           is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
           ((unsigned int)event_result >> 24) == 5 && is_being_cast(attacking_card_controller, attacking_card))
    event_result |= 1;
  return global_enchantment(player, card, event);
}

static int dingus_staff_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_DINGUS_STAFF)
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
    if (in_play(affected_card_controller, affected_card) &&
        is_this_dying(affected_card_controller, affected_card))
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
      count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, TYPE_CREATURE, NULL,
                                             GFPC_TRACK_CONTROLLER, 0);
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    APNAP(p, {
      int amount = p == 0 ? BYTE0(result) : BYTE1(result);
      for (; amount > 0; --amount)
        damage_player(p, 2, player, card);
    });
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_dingus_staff(int player, int card, event_t event)
{
  /* CARD_ID_DINGUS_STAFF	2961
  Dingus Staff	|4
  Artifact
  Whenever a creature dies, ~ deals 2 damage to that creature's controller. */

  if (is_humiliated(player, card, AL_TRIG))
  {
    return 0;
  }

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    int found = 0;
    int c;
    for (c = 0; c < active_cards_count[player]; c++)
    {
      if (in_play(player, c) && is_what(player, c, TYPE_EFFECT))
      {
        card_instance_t *inst = get_card_instance(player, c);
        if (inst->info_slot == (int)dingus_staff_effect)
        {
          if ((inst->damage_target_player == player && inst->damage_target_card == card) ||
              (inst->targets[0].player == player && inst->targets[0].card == card))
          {
            found = 1;
            break;
          }
        }
      }
    }
    if (!found)
    {
      int legacy = create_targetted_legacy_effect(player, card, &dingus_staff_effect, player, card);
      card_instance_t *inst = get_card_instance(player, legacy);
      inst->targets[0].player = player;
      inst->targets[0].card = card;
      inst->number_of_targets = 1;
      add_status(player, legacy, STATUS_INVISIBLE_FX);
    }
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
    count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, TYPE_CREATURE, NULL,
                                           GFPC_TRACK_CONTROLLER, 0);

  if (get_card_instance(player, card)->kill_code <= 0)
  {
    int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (result)
    {
      APNAP(p, {
        int amount = p == 0 ? BYTE0(result) : BYTE1(result);
        for (; amount > 0; --amount)
          damage_player(p, 2, player, card);
      });
    }
  }

  return 0;
}

int card_disrupt(int player, int card, event_t event)
{
  /* Disrupt	|U
   * Instant
   * Counter target instant or sorcery spell unless its controller pays |1.
   * Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_INSTANT | TYPE_INTERRUPT | TYPE_SORCERY);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (counterspell_resolve_unless_pay_x(player, card, &td, 0, 1))
		draw_a_card(player);

	  kill_card(player, card, KILL_DESTROY);
	  return 0;
	}
	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}

int card_doomsday(int player, int card, event_t event)
{
  /* Doomsday	|B|B|B
   * Sorcery
   * Search your library and graveyard for five cards and exile the rest. Put the chosen cards on top of your library in any order. You lose half your life,
   * rounded up. */

	if (event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		int cd = count_deck(player);
		int cg = count_graveyard(player);
		int cards = MIN(5, cd+cg);

		int i = 0, chosen[5] = { -1, -1, -1, -1, -1 };
		while (i < cards){
			int selected = -1;
			if( cd > 0 ){
				selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MAX_VALUE, -1, &this_test);
				if( selected != -1 ){
					chosen[i] = deck_ptr[player][selected];
					remove_card_from_deck(player, selected);
					i++;
					cd--;
				}
			}
			if( selected == -1 && cg > 0){
				selected = new_select_a_card(player, player, TUTOR_FROM_GRAVE, 0, AI_MAX_VALUE, -1, &this_test);
				if( selected != -1 ){
					chosen[i] = get_grave(player)[selected];
					remove_card_from_grave(player, selected);
					i++;
					cg--;
				}
			}
		}

		reshuffle_grave_into_deck(player, 1);

		rfg_whole_library(player);

		for (i = 0; i < cards; ++i){
			deck_ptr[player][i] = chosen[i];
		}
		if( cards > 1 ){
			rearrange_top_x(player, player, cards);
		}
		lose_life(player, (life[player] + 1) / 2);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_downdraft(int player, int card, event_t event){
	/* Downdraft	|2|G
	 * Enchantment
	 * |G: Target creature loses flying until end of turn.
	 * Sacrifice ~: ~ deals 2 damage to each creature with flying. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_LOSES_FLYING = 1,
		CHOICE_SAC_DAMAGE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME, MANACOST0, 0, &td, NULL)
		};
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.keyword = KEYWORD_FLYING;
		test.toughness = 3;
		test.toughness_flag = F5_TOUGHNESS_LESSER_THAN_VALUE;
		int priority_sac = (check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &test) -
							check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test)) * 2;

		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Target loses fling", abils[1], 5,
							"Sac & damage flyers", abils[2], priority_sac);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_LOSES_FLYING ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, "TARGET_CREATURE");
			if( spell_fizzled != 1 ){
				ai_modifier += (! check_for_ability(instance->targets[0].player, instance->targets[0].card, KEYWORD_FLYING) ? -48 : 0);
			}
		}

		if( choice == CHOICE_SAC_DAMAGE ){
			return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_LOSES_FLYING && valid_target(&td) ){
			negate_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, KEYWORD_FLYING);
		}
		if( instance->info_slot == CHOICE_SAC_DAMAGE ){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			test.keyword = KEYWORD_FLYING;
			new_damage_all(player, card, ANYBODY, 2, 0, &test);
		}
	}

	return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22eeb00; original cards/weatherlight.c:1424. */
int card_dwarven_berserker(int player, int card, event_t event)
{
  /* CARD_ID_DWARVEN_BERSERKER	2965
   * Dwarven Berserker	|1|R
   * Creature - Dwarf Berserker 1/1
   * Whenever ~ becomes blocked, it gets +3/+0 and gains trample until end of turn. */

  int result;
  if (event == EVENT_CHECK_ABILITIES)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (current_turn == player)) &&
        (current_phase < 0x18))
    {
      check_abilities_keywords = check_abilities_keywords | 0x80;
      check_abilities_power = check_abilities_power + 3;
    }
  }
  else if (event == EVENT_DECLARE_BLOCKERS)
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
          pump_ability_until_eot(player, card, player, card, 3, 0, 0x80, 0);
        }
      }
    }
  }
  return 0;
}
#endif

int card_dwarven_thaumaturgist(int player, int card, event_t event)
{
/* CARD_ID_DWARVEN_THAUMATURGIST	2966
Dwarven Thaumaturgist	|2|R
Creature - Dwarf Shaman 1/2
|T: Switch target creature's power and toughness until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
		switch_power_and_toughness_until_eot(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card);

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(0), 0, &td, "TARGET_CREATURE");
}

int card_empyrial_armor(int player, int card, event_t event){
/* CARD_ID_EMPYRIAL_ARMOR	2967
Empyrial Armor	|1|W|W
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+1 for each card in your hand. */

	return generic_aura(player, card, event, player, hand_count[player], hand_count[player], 0, 0, 0, 0, 0);
}

int card_ertais_familiar(int player, int card, event_t event){
/* CARD_ID_ERTAIS_FAMILIAR	2968
Ertai's Familiar	|1|U
Creature - Illusion 2/2
Phasing (This phases in or out before you untap during each of your untap steps. While it's phased out, it's treated as though it doesn't exist.)
When ~ phases out or leaves the battlefield, put the top three cards of your library into your graveyard.
|U: Until your next upkeep, ~ can't phase out. */

	card_instance_t *instance = get_card_instance(player, card);

	int ph = phasing(player, card, event);
	if (ph){
		return ph;
	}

	leaves_play(player, card, event, 1, 1);
	if (event == EVENT_LEAVES_PLAY_ABILITY){
		mill(player, 3);	// also in phase_out_impl()
	}

	else if( event == EVENT_RESOLVE_ACTIVATION ){
		set_special_flags(player, instance->parent_card, SF_CANNOT_PHASE_OUT);
		create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &spatial_binding_legacy,
										instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_U(1), 0, NULL, NULL);
}

int card_fallow_wurm(int player, int card, event_t event){
/* CARD_ID_FALLOW_WURM	2969
Fallow Wurm	|2|G
Creature - Wurm 4/4
When ~ enters the battlefield, sacrifice it unless you discard a land card. */

	return generic_creature_with_discard_type_on_cip(player, card, event, TYPE_LAND);
}

int card_fervor(int player, int card, event_t event)
{
/* CARD_ID_FERVOR	1604
Fervor	|2|R
Enchantment
Creatures you control have haste. (They can attack and |T as soon as they come under your control.) */

  boost_subtype(player, card, event, -1, 0, 0, 0, SP_KEYWORD_HASTE, BCT_CONTROLLER_ONLY|BCT_INCLUDE_SELF);

  return global_enchantment(player, card, event);
}

int card_festering_evil(int player, int card, event_t event){
/* CARD_ID_FESTERING_EVIL	2972
Festering Evil	|3|B|B
Enchantment
At the beginning of your upkeep, ~ deals 1 damage to each creature and each player.
|B|B, Sacrifice ~: ~ deals 3 damage to each creature and each player. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		new_damage_all(player, card, ANYBODY, 1, NDA_PLAYER_TOO | NDA_ALL_CREATURES, NULL);
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		new_damage_all(player, card, ANYBODY, 3, NDA_PLAYER_TOO | NDA_ALL_CREATURES, NULL);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_B(2), 0, NULL, NULL);
}

int card_fire_whip(int player, int card, event_t event){
	/* Fire Whip	|1|R	0x20033ab
	 * Enchantment - Aura
	 * Enchant creature you control
	 * Enchanted creature has "|T: This creature deals 1 damage to any target."
	 * Sacrifice ~: ~ deals 1 damage to any target. */
	if( ! IS_GAA_EVENT(event) && (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		return targeted_aura_custom_prompt(player, card, event, &td, "Select target creature you control.");
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	enum{
		CHOICE_TAP_CREATURE = 1,
		CHOICE_SAC
	};

	int p = instance->damage_target_player;
	int c = instance->damage_target_card;

	if( event == EVENT_CAN_ACTIVATE ){
		if( p > -1 ){
			target_definition_t td3;
			default_target_definition(p, c, &td3, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td3.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			if( granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td3, NULL) ){
				return 1;
			}
		}
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0, &td2, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0};
		if( p > -1 ){
			target_definition_t td3;
			default_target_definition(p, c, &td3, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td3.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			if( granted_generic_activated_ability(player, card, p, c, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0,
												&td3, NULL) )
			{
				abils[1] = 1;
			}
		}
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0, &td2, NULL) ){
			abils[2] = 1;
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Tap enchanted creature", abils[1], 5,
							"Sac Fire Whip", abils[2], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_TAP_CREATURE ){
			target_definition_t td3;
			default_target_definition(p, c, &td3, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td3.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			return granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0,
														&td3, "TARGET_ANY");
		}

		if( choice == CHOICE_SAC ){
			return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0,
												&td2, "TARGET_ANY");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_TAP_CREATURE ){
			target_definition_t td3;
			default_target_definition(p, c, &td3, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td3.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			if( validate_target(player, card, &td3, 0) ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, 1, p, c);
			}
		}
		if( instance->info_slot == CHOICE_SAC && valid_target(&td2) ){
			damage_target0(player, card, 1);
		}
	}

	return 0;
}

int card_firestorm(int player, int card, event_t event)
{
	/* Firestorm	|R	0x20032d9
	 * Instant
	 * As an additional cost to cast this spell, discard X cards.
	 * ~ deals X damage to each of X targets. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t* instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td.allow_cancel = 3;

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
		if( !is_token(player, card) )
		{
			int max_targets = target_available(player, card, &td);
			max_targets = MIN(max_targets, hand_count[player]);
			int chosen = pick_up_to_n_targets(&td, "TARGET_ANY", max_targets);
			if (cancel != 1 && chosen > 0)
			{
				instance->info_slot = chosen;
				multidiscard(player, chosen, 0);
			}
		}
	  else
		{
		  td.allow_cancel = 1;
		  pick_up_to_n_targets(&td, "TARGET_ANY", instance->info_slot);
		}
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (instance->info_slot > 0)
		{
		  int num_validated = 0;

		  int i;
		  for (i = 0; i < instance->info_slot; ++i)
			if (validate_target(player, card, &td, i))
			  {
				if (player == AI && instance->targets[i].player == AI)
				  ai_modifier -= 32;

				++num_validated;
				damage_creature(instance->targets[i].player, instance->targets[i].card, instance->info_slot, player, card);
			  }

		  if (num_validated == 0)
			spell_fizzled = 1;
		}
	  else if (player == AI)
		ai_modifier -= 64;

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_flux(int player, int card, event_t event){

	/* Flux	|2|U
	 * Sorcery
	 * Each player discards any number of cards, then draws that many cards.
	 * Draw a card. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 24;
	}

	if( event == EVENT_RESOLVE_SPELL){
		APNAP(p,{
					test_definition_t test;
					new_default_test_definition(&test, TYPE_ANY, "Select a card to discard.");
					int disc = 0;
					while( hand_count[player] ){
							int selected = new_select_a_card(p, p, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &test);
							if( selected > -1 ){
								disc++;
								discard_card(p, selected);
								if( disc == 3 && IS_AI(p) ){
									break;
								}
							}
							else{
								break;
							}
					}
					draw_cards(p, disc);
				}
		);
		draw_cards(player, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_fog_elemental(int player, int card, event_t event)
{
  /* CARD_ID_FOG_ELEMENTAL	2977
Fog Elemental	|2|U
Creature - Elemental 4/4
Flying (This creature can't be blocked except by creatures with flying or reach.)
When ~ attacks or blocks, sacrifice it at end of combat. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY || (!is_humiliated(player, card, AL_TRIG) && blocking(player, card, event)))
    create_targetted_legacy_effect(player, card, sacrifice_at_end_of_combat, player, card);
  return 0;
}

int card_gaeas_blessing(int player, int card, event_t event){
/* CARD_ID_GAEAS_BLESSING	2979
Gaea's Blessing	|1|G
Sorcery
Target player shuffles up to three target cards from his or her graveyard into his or her library.
Draw a card.
When ~ is put into your graveyard from your library, shuffle your graveyard into your library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target player.", 1, NULL);
		if( spell_fizzled != 1 ){
			select_multiple_cards_from_graveyard(player, instance->targets[0].player, 0, player == instance->targets[0].player ? AI_MAX_VALUE : AI_MIN_VALUE, NULL, 3, &instance->targets[1]);
		}
	}

    else if(event == EVENT_RESOLVE_SPELL ){
		int i, any_shuffled_in = 0, num_targets = 0;
		for (i = 1; i <= 3; ++i){
			if( instance->targets[i].player != -1 ){
				++num_targets;
				int selected = validate_target_from_grave(player, card, instance->targets[0].player, i);
				if (selected != -1){
					from_graveyard_to_deck(instance->targets[0].player, selected, 2);
					any_shuffled_in = 1;
				}
			}
		}
		if (any_shuffled_in){
			shuffle(instance->targets[0].player);
		}
		if (any_shuffled_in	// a card was targeted and it was still legal
				|| valid_target(&td)){	// the targeted player was still legal
				draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int can_pay_1_life_thunk(int player, int card, int number_of_age_counters)
{
  return can_pay_life(player, number_of_age_counters);
}

int lose_1_life_thunk(int player, int card, int number_of_age_counters)
{
  lose_life(player, number_of_age_counters);
  return 1;
}

int card_gallowbraid(int player, int card, event_t event)
{
  /* Gallowbraid	|3|B|B
   * Legendary Creature - Horror 5/5
   * Trample
   * Cumulative upkeep-Pay 1 life. */
  /* Morinfen	|3|B|B
   * Legendary Creature - Horror 5/4
   * Flying
   * Cumulative upkeep-Pay 1 life. */

  check_legend_rule(player, card, event);
  cumulative_upkeep_general(player, card, event, can_pay_1_life_thunk, lose_1_life_thunk);
  return 0;
}

int card_gemstone_mine(int player, int card, event_t event)
{
  /* Gemstone Mine	""
   * Land
   * ~ enters the battlefield with three mining counters on it.
   * |T, Remove a mining counter from ~: Add one mana of any color to your mana pool. If there are no mining counters on ~, sacrifice it. */

  if (!is_unlocked(player, card, event, 16))
	return 0;



  if (!is_humiliated(player, card, AL_TRIG)){
	enters_the_battlefield_with_counters(player, card, event, COUNTER_MINING, 3);
  }

  if (event == EVENT_CAN_ACTIVATE && count_counters(player, card, COUNTER_MINING) <= 0)
	return 0;	// else fall through to mana_producer()

  if (event == EVENT_ACTIVATE)
	{
	  mana_producer(player, card, event);
	  if (cancel != 1)
		{
		  remove_counters(player, card, player, card, COUNTER_MINING, 1);
		  if (count_counters(player, card, COUNTER_MINING) <= 0)
			kill_card(player, card, KILL_SACRIFICE);
		}
	  return 0;
	}

  return mana_producer(player, card, event);
}

int card_gerrards_wisdom(int player, int card, event_t event){
/* CARD_ID_GERRARDS_WISDOM	2982
Gerrard's Wisdom	|2|W|W
Sorcery
You gain 2 life for each card in your hand. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( life[player] < 6 ){
			ai_modifier += (2 * hand_count[player] * 8);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, 2 * hand_count[player]);
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

int card_goblin_bomb(int player, int card, event_t event)
{
  /* Goblin Bomb	|1|R	0x20033b0
   * Enchantment
   * At the beginning of your upkeep, you may flip a coin. If you win the flip, put a fuse counter on ~. If you lose the flip, remove a fuse counter from ~.
   * Remove five fuse counters from ~, Sacrifice ~: ~ deals 20 damage to target player or planeswalker. */

  resolve_trigger_t trigger_mode = (duh_mode(player) != 0) + RESOLVE_TRIGGER_OPTIONAL;
  upkeep_trigger_ability_mode(player, card, event, player, trigger_mode);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    if (flip_a_coin(player, card))
      add_counters(player, card, player, card, COUNTER_FUSE, 1);
    else
      remove_counters(player, card, player, card, COUNTER_FUSE, 1);
  }
  else if (IS_GAA_EVENT(event))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x100);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
      damage_target0(player, card, 20);
    return generic_activated_ability(player, card, event, 0xc, 0, 0, 0, 0, 0, 0, 0x1310500, &td,
                                       "TARGET_PLAYER_OR_PLANESWALKER");
  }
  return global_enchantment(player, card, event);
}

int card_goblin_grenadiers(int player, int card, event_t event){
/* CARD_ID_GOBLIN_GRENADIERS	2984
Goblin Grenadiers	|3|R
Creature - Goblin 2/2
Whenever ~ attacks and isn't blocked, you may sacrifice it. If you do, destroy target creature and target land. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && is_attacking(player, card) && is_unblocked(player, card) &&
		current_phase == PHASE_AFTER_BLOCKING && can_target(&td) && can_target(&td2) && ! is_humiliated(player, card, AL_TRIG) )
	{
		return 1;
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "TARGET_CREATURE", 0, 1) ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			new_pick_target(&td2, "TARGET_LAND", 1, 1);
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
			if( spell_fizzled != 1 ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( validate_target(player, card, &td2, 1) ){
			kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_goblin_vandal(int player, int card, event_t event){
/* CARD_ID_GOBLIN_VANDAL	2986
Goblin Vandal	|R
Creature - Goblin Rogue 1/1
Whenever ~ attacks and isn't blocked, you may pay |R. If you do, destroy target artifact defending player controls and ~ assigns no combat damage this turn. */

	if( event == EVENT_CLEANUP )
		get_card_instance(player, card)->info_slot = 0;

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.allowed_controller = td.preferred_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && is_attacking(player, card) && is_unblocked(player, card) && ! is_humiliated(player, card, AL_TRIG) &&
		current_phase == PHASE_AFTER_BLOCKING && can_target(&td) && has_mana(player, COLOR_RED, 1) && instance->info_slot != 66 )
	{
		return 1;
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		charge_mana(player, COLOR_RED, 1);
		if( spell_fizzled != 1 && pick_target(&td, "TARGET_ARTIFACT") ){
			instance->info_slot = 66;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			negate_combat_damage_this_turn(instance->parent_controller, instance->parent_card,
											instance->parent_controller, instance->parent_card, 0);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_haunting_misery(int player, int card, event_t event)
{
	/* Haunting Misery	|1|B|B	0x200bb5f
	 * Sorcery
	 * As an additional cost to cast this spell, exile X creature cards from your graveyard.
	 * ~ deals X damage to target player or planeswalker. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if (event == EVENT_CAN_CAST)
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);


  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
	  if (!is_token(player, card) && cancel != 1)
		{
		  int num_exiled = 0;

		  test_definition_t test;
		  new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card to exile.");

		  while (new_global_tutor(player, player, TUTOR_FROM_GRAVE_NOTARGET, TUTOR_RFG, 0, AI_MIN_VALUE, &test) >= 0)
			++num_exiled;

		  get_card_instance(player, card)->info_slot = num_exiled;

		  ai_modifier += (player == AI ? 12 : -12) * (num_exiled - 2);
		}
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  if (valid_target(&td))
		damage_target0(player, card, instance->info_slot);

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_harvest_wurm(int player, int card, event_t event){
/* CARD_ID_HARVEST_WURM	2987
Harvest Wurm	|1|G
Creature - Wurm 3/2
When ~ enters the battlefield, sacrifice it unless you return a basic land card from your graveyard to your hand. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( !any_in_graveyard_by_type(player, TYPE_LAND) ){
			ai_modifier -= 1000;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int kill_me = 1;
		if( any_in_graveyard_by_type(player, TYPE_LAND) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Select a land card.");
			int result = new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
			kill_me = result >= 0 ? 0 : 1;
		}
		if( kill_me ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_heart_of_bogardan(int player, int card, event_t event)
{
	/* Heart of Bogardan	|2|R|R	0x2003301
	 * Enchantment
	 * Cumulative upkeep |2
	 * When a player doesn't pay ~'s cumulative upkeep, ~ deals X damage to target player or planeswalker and each creature that player or that planeswalker's controller controls, where X is twice the number of age counters on ~ minus 2. */
  if (cumulative_upkeep(player, card, event, MANACOST_X(2)))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  // Some shenanigans here - resurrect the card long enough to target and deal damage, then zorch it again.
	  instance->internal_card_id = get_internal_card_id_from_csv_id(CARD_ID_HEART_OF_BOGARDAN);
	  instance->state |= STATE_INVISIBLE | STATE_CANNOT_TARGET | STATE_IN_PLAY;

	  target_definition_t td;
	  default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	  td.allow_cancel = 0;

	  if (count_counters(player, card, COUNTER_AGE) >= 2 && can_target(&td) && pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER"))
		{
		  int dmg = count_counters(player, card, COUNTER_AGE)*2 - 2;
		  damage_target0(player, card, dmg);
		  new_damage_all(player, card, instance->targets[0].player, dmg, NDA_ALL_CREATURES, NULL);
		}

	  instance->internal_card_id = -1;
	  instance->state &= ~(STATE_INVISIBLE | STATE_CANNOT_TARGET);
	}

  return global_enchantment(player, card, event);
}

int card_heavy_ballista(int player, int card, event_t event){
  /* Heavy Ballista |3|W
   * 	  Creature - Human Soldier 2/3
   * 	  {T}: Heavy Ballista deals 2 damage to target attacking or blocking creature. */

	// Used externally
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_IN_COMBAT;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target attacking or blocking creature.");
}

int card_hidden_horror(int player, int card, event_t event){
/* CARD_ID_HIDDEN_HORROR	2991
Hidden Horror	|1|B|B
Creature - Horror 4/4
When ~ enters the battlefield, sacrifice it unless you discard a creature card. */

	return generic_creature_with_discard_type_on_cip(player, card, event, TYPE_CREATURE);
}

static int can_pay_2_life_thunk(int player, int card, int number_of_age_counters)
{
  return can_pay_life(player, 2 * number_of_age_counters);
}

static int lose_2_life_thunk(int player, int card, int number_of_age_counters)
{
  lose_life(player, 2 * number_of_age_counters);
  return 1;
}

int card_inner_sanctum(int player, int card, event_t event){

	/* Inner Sanctum	|1|W|W
	 * Enchantment
	 * Cumulative upkeep-Pay 2 life.
	 * Prevent all damage that would be dealt to creatures you control. */

	cumulative_upkeep_general(player, card, event, can_pay_2_life_thunk, lose_2_life_thunk);

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *source = get_card_instance(affected_card_controller, affected_card);

		if( damage_card != source->internal_card_id || source->damage_target_card == -1 || source->info_slot < 1 ||
			source->damage_target_player != player ){
			return 0;
		}

		else{
			 source->info_slot = 0;
		}
	}

	return global_enchantment(player, card, event);
}

int card_infernal_tribute(int player, int card, event_t event){
/* CARD_ID_INFERNAL_TRIBUTE	2993
Infernal Tribute	|B|B|B
Enchantment
|2, Sacrifice a nontoken permanent: Draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a nontoken permanent to sacrifice.");
	test.type_flag = F1_NO_TOKEN;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL, 0, &test, 0);
}

int card_jabaris_banner(int player, int card, event_t event){
/* CARD_ID_JABARIS_BANNER	2995
Jabari's Banner	|2
Artifact
|1, |T: Target creature gains flanking until end of turn.
(Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			ai_modifier += (! check_for_special_ability(instance->targets[0].player, instance->targets[0].card, SP_KEYWORD_FLANKING) &&
							is_attacking(instance->targets[0].player, instance->targets[0].card) &&
							current_turn == player && current_phase < PHASE_DECLARE_BLOCKERS ? 48 : -48);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			alternate_legacy_text(2, player, pump_ability_until_eot(instance->parent_controller, instance->parent_card,
																	instance->targets[0].player, instance->targets[0].card,
																	0, 0, 0, SP_KEYWORD_FLANKING));
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_CREATURE");
}

int card_lava_hounds(int player, int card, event_t event){
  /* Lava Hounds |2|R|R
   * 	  Creature - Hound 4/4
   * 	  Haste
   * 	  When Lava Hounds enters the battlefield, it deals 4 damage to you. */

	haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		damage_player(player, 4, player, card);
	}

	return 0;
}

int card_liege_of_hollows(int player, int card, event_t event){
	/* Liege of the Hollows	|2|G|G
	 * Creature - Spirit 3/4
	 * When ~ dies, each player may pay any amount of mana. Then each player who paid mana this way puts that many 1/1 |Sgreen Squirrel creature tokens onto the battlefield. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SQUIRREL, &token);
		APNAP(p,{
					if( has_mana(p, COLOR_ANY, 1) && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, p, COLOR_ANY, -1) ){
						if( x_value > 0 ){
							token.t_player = p;
							token.qty = x_value;
							generate_token(&token);
						}
					}
				}
		);
	}

	return 0;
}

/* CARD_ID_LLANOWAR_BEHEMOTH	1073 =>   CARD_ID_BRAMBLESNAP
Llanowar Behemoth	|3|G|G
Creature - Elemental 4/4
Tap an untapped creature you control: ~ gets +1/+1 until end of turn. */

int card_llanowar_sentinel(int player, int card, event_t event){
/* CARD_ID_LLANOWAR_SENTINEL	3001
Llanowar Sentinel	|2|G
Creature - Elf 2/3
When ~ enters the battlefield, you may pay |1|G. If you do, search your library for a card named ~ and put that card onto the battlefield. Then shuffle your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		if( has_mana_multi(player, MANACOST_XG(1, 1)) &&
			charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, MANACOST_XG(1, 1)) )
		{
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			this_test.id = get_card_name(player, card);
			scnprintf(this_test.message, 100, "Select a card named %s", cards_ptr[get_card_name(player, card)]->name);
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);
		}
	}

	return 0;
}

static int if_would_enter_bf_sac_two_untapped_lands_instead(int player, int card, event_t event)
{
  /* Land
   * If ~ would enter the battlefield, sacrifice two untapped lands instead. If you do, put ~ onto the battlefield. If you don't, put it into its owner's
   * graveyard. */

  if (event == EVENT_CAST_SPELL && affect_me(player, card) && IS_AI(player))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_LAND, "");
	  test.state = STATE_TAPPED;
	  test.state_flag = DOESNT_MATCH;

	  if (check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test) < 2)
		ai_modifier -= 512;
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  play_land_sound_effect(player, card);

	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_LAND, "Select two untapped lands to sacrifice.");
	  test.state = STATE_TAPPED;
	  test.state_flag = DOESNT_MATCH;
	  test.qty = 2;
	  test.not_me = 1;

	  if (!new_sacrifice(player, card, player, SAC_NO_CANCEL|SAC_ALL_OR_NONE, &test))
		kill_card(player, card, KILL_STATE_BASED_ACTION);

	  return 1;	// so the sound effect doesn't play again in mana_producer() after prompting for cip effects
	}

  return 0;
}

int card_lotus_vale(int player, int card, event_t event)
{
  /* Lotus Vale	""
   * Land
   * If ~ would enter the battlefield, sacrifice two untapped lands instead. If you do, put ~ onto the battlefield. If you don't, put it into its owner's
   * graveyard.
   * |T: Add three mana of any one color to your mana pool. */

  if (if_would_enter_bf_sac_two_untapped_lands_instead(player, card, event) &&
      !is_humiliated(player, card, AL_TRIG))
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CHANGE_TYPE)
  {
    if (affect_me(player, card) && instance->targets[12].card != -1 && instance->parent_card < 0)
      event_result = instance->targets[12].card;
  }
  else if (event == EVENT_CAST_SPELL && affect_me(player, card))
  {
    ai_modifier += 100;
  }
  else if (event == EVENT_ACTIVATE)
  {
    if (!can_produce_mana(player, card))
      spell_fizzled = 1;
    else
      produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 3);
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    play_land_sound_effect(player, card);
    instance->targets[13].player = get_id(player, card);
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    return CAN_TAP_FOR_MANA(player, card);
  }
  else if (event == EVENT_COUNT_MANA && affect_me(player, card) && CAN_TAP_FOR_MANA(player, card))
  {
    if (check_special_flags2(player, card, SF2_CONTAMINATION) && is_what(player, card, TYPE_LAND))
      declare_mana_available(player, COLOR_BLACK, 1);
    else
      declare_mana_available_hex(player, COLOR_TEST_ANY_COLORED, 3);
  }
  return 0;
}

/* ManalinkEh 2022: 0x22eabc0; original cards/weatherlight.c:2436. */
static int has_same_mana_type(int unused, int color, int player, int card)
{
  unsigned int available_colors;
  available_colors = get_colors_of_mana_land_could_produce_ignoring_costs(player, card);
  return available_colors & color;
}

/* ManalinkEh 2022: 0x22f1960; original cards/weatherlight.c:2442. */
int card_mana_web(int player, int card, event_t event)
{
  /* Mana Web	|3
   * Artifact
   * Whenever a land an opponent controls is tapped for mana, tap all lands that player controls that could produce any type of mana that land could produce. */

  card_instance_t *instance = get_card_instance(player, card);
  int p = 1 - player;
  if (trigger_condition == TRIGGER_TAP_CARD && card == affected_card &&
      player == affected_card_controller && instance->info_slot != 0x42 &&
      p == trigger_cause_controller && is_what(p, trigger_cause, TYPE_LAND) &&
      !is_humiliated(player, card, AL_TRIG))
  {
    if (event == EVENT_TRIGGER)
    {
      if (tapped_for_mana_color >= 0)
        event_result |= 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      int color = get_colors_of_mana_land_could_produce_ignoring_costs(
          trigger_cause_controller, trigger_cause);
      test_definition_t test;
      default_test_definition(&test, TYPE_LAND);
      test.special_selection_function = has_same_mana_type;
      test.value_for_special_selection_function = color;
      if (player == HUMAN || !get_setting(0x16))
      {
        instance->info_slot = 0x42;
        for (int c = 0; c < active_cards_count[p]; ++c)
        {
          if (new_make_test_in_play(p, c, -1, &test))
          {
            int land_iid = get_internal_card_id_from_csv_id(get_id(p, c));
            if (cards_data[land_iid].extra_ability & 0x1000)
              autotap_mana_source(p, c);
          }
        }
        instance->info_slot = 0;
      }
      new_manipulate_all(player, card, p, &test, ACT_TAP);
    }
  }
  return 0;
}

int card_maraxus_of_keld(int player, int card, event_t event)
{
  /* Maraxus of Keld	|4|R|R
   * Legendary Creature - Human Warrior 100/100
   * ~'s power and toughness are each equal to the number of untapped artifacts, creatures, and lands you control. */

  card_instance_t *instance;
  int result;
  int card_00;
  int newval;

  check_legend_rule(player, card, event);
  if (((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    newval = 0;
    if (0 < active_cards_count[player])
    {
      card_00 = 0;
      do
      {
        instance = in_play(player, card_00);
        if (instance != NULL)
        {
          result = is_what(player, card_00, 0x43);
          if (result != 0)
          {
            result = is_tapped(player, card_00);
            newval = newval + (unsigned int)(result == 0);
          }
        }
        card_00 = card_00 + 1;
      } while (card_00 < active_cards_count[player]);
    }
    set_characteristic_pt(newval);
  }
  return 0;
}

int card_merfolk_traders(int player, int card, event_t event){
/* CARD_ID_MERFOLK_TRADERS	3008
Merfolk Traders	|1|U
Creature - Merfolk 1/2
When ~ enters the battlefield, draw a card, then discard a card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
		discard(player, 0, player);
	}

	return 0;
}

int card_mind_stone(int player, int card, event_t event){
/* CARD_ID_MIND_STONE	3009
Mind Stone	|2
Artifact
|T: Add |C to your mana pool.
|1, |T, Sacrifice ~: Draw a card. */

	card_instance_t *instance = get_card_instance(player, card);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	if(event == EVENT_ACTIVATE ){
		int choice = instance->info_slot = 0;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL) )
		{
			int ai_choice = count_permanents_by_type(player, TYPE_LAND) > 6 ? 1 : 0;
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Sac and draw 1\n Cancel", ai_choice);
		}
		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		if( choice == 1 ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				instance->info_slot = 1;
			}
			else{
				remove_state(player, card, STATE_TAPPED);
			}
		}
		if( choice == 2 ){
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION && instance->info_slot == 1 ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_mistmoon_griffin(int player, int card, event_t event){
/* CARD_ID_MISTMOON_GRIFFIN	3011
Mistmoon Griffin	|3|W
Creature - Griffin 2/2
Flying
When ~ dies, exile ~, then return the top creature card of your graveyard to the battlefield. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		exile_from_owners_graveyard(player, card);
		const int *grave = get_grave(player);
		int count = count_graveyard(player)-1;
		while (count > -1){
				if( cards_data[grave[count]].type & TYPE_CREATURE ){
					reanimate_permanent(player, card, player, count, REANIMATE_DEFAULT);
					break;
				}
				count--;
		}
	}

	return 0;
}

int card_natures_resurgence(int player, int card, event_t event){
/* CARD_ID_NATURES_RESURGENCE	3015
Nature's Resurgence	|2|G|G
Sorcery
Each player draws a card for each creature card in his or her graveyard. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier +=  ((count_graveyard_by_type(player, TYPE_CREATURE) - count_graveyard_by_type(1-player, TYPE_CREATURE)) * 8);
	}

	if( event == EVENT_RESOLVE_SPELL){
		APNAP(p, { draw_cards(p, count_graveyard_by_type(p, TYPE_CREATURE)); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_necratog(int player, int card, event_t event){
/* CARD_ID_NECRATOG	3016
Necratog	|1|B|B
Creature - Atog 1/2
Exile the top creature card of your graveyard: ~ gets +2/+2 until end of turn. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		int crit_in_grave = count_graveyard_by_type(player, TYPE_CREATURE);
		if (crit_in_grave <= 0)
			return 0;

		return generic_shade_amt_can_pump(player, card, 2, 0, MANACOST0, crit_in_grave);
	}

	if( event == EVENT_CAN_ACTIVATE && count_graveyard_by_type(player, TYPE_CREATURE) ){
		return generic_shade(player, card, event, 0, MANACOST0, 2, 2, 0, 0);
	}

	if( event == EVENT_ACTIVATE){
		generic_shade(player, card, event, 0, MANACOST0, 2, 2, 0, 0);
		if( spell_fizzled != 1 ){
			remove_top_creature_card_from_grave(player);
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade(player, card, event, 0, MANACOST0, 2, 2, 0, 0);
	}

   return 0;
}

/* ManalinkEh 2022: 0x22f22a0; original cards/weatherlight.c:2629. */
int card_noble_benefactor(int player, int card, event_t event)
{
  /* Noble Benefactor	|2|U
   * Creature - Human Cleric 2/2
   * When this creature dies, each player may search their library for a card and put that card into their hand. Then each player who searched their library this way shuffles. */

  int result;
  unsigned int player_pass;
  unsigned int argument1;
  test_definition_t this_test;

  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {

    player_pass = 0;

    do
    {
      while (
          (argument1 = current_turn ^ player_pass,
           ai_is_speculating == 1 || (((argument1 == 1 && (((unsigned char)trace_mode & 2) == 0)) ||
                                       (result = do_dialog(argument1, player, card, player, card,
                                                           " Search your library\n Pass", 0),
                                        result == 0)))))
      {
        new_default_test_definition(&this_test, 0xff, "Select any card.");
        this_test.no_shuffle = 1;
        result = new_global_tutor(argument1, argument1, 1, 1, 0, 1, &this_test);
        if (result < 0)
          break;
        player_pass = player_pass + 1;
        shuffle(argument1);
        if (player_pass == 2)
        {
          return 0;
        }
      }
      player_pass = player_pass + 1;
    } while (player_pass != 2);
  }
  return 0;
}

int card_null_rod(int player, int card, event_t event)
{
  /* CARD_ID_NULL_ROD	3018
   * Null Rod	|2
   * Artifact
   * Activated abilities of artifacts can't be activated. */

  if (in_play(player, card) && !is_humiliated(player, card, AL_STATIC))
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_ARTIFACT);
    remove_abilities_by_test(ANYBODY, event, &test, AL_ALL_ACT);
  }
  return 0;
}

int card_ophidian(int player, int card, event_t event){
/* CARD_ID_OPHIDIAN	3020
Ophidian	|2|U
Creature - Snake 1/3
Whenever ~ attacks and isn't blocked, you may draw a card. If you do, ~ assigns no combat damage this turn. */

	if( ! IS_GAA_EVENT(event) && event != EVENT_CLEANUP ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && ! is_humiliated(player, card, AL_TRIG) && is_attacking(player, card) && is_unblocked(player, card) &&
		current_phase == PHASE_AFTER_BLOCKING && instance->info_slot != 66 ){
		return 1;
	}

	if( event == EVENT_ACTIVATE){
		ai_modifier += (56-(hand_count[player] * 8));
		instance->info_slot = 66;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		negate_combat_damage_this_turn(instance->parent_controller, instance->parent_card,
										instance->parent_controller, instance->parent_card, 0);
		draw_cards(player, 1);
	}

	if( event == EVENT_CLEANUP ){
		instance->info_slot = 0;
	}

	return 0;
}

int card_orcish_settlers(int player, int card, event_t event)
{
  /* Orcish Settlers	|1|R
   * Creature - Orc 1/1
   * |X|X|R, |T, Sacrifice ~: Destroy X target lands. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

  card_instance_t* instance = get_card_instance(player, card);

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_LAND);
  td.allow_cancel = 3;

  if (event == EVENT_CAN_ACTIVATE)
	return (CAN_TAP(player, card) && can_sacrifice_this_as_cost(player, card)
			&& (IS_AI(player) ? CAN_ACTIVATE(player, card, MANACOST_XR(2,1)) && can_target(&td) : CAN_ACTIVATE(player, card, MANACOST_R(1))));

  if (event == EVENT_ACTIVATE)
	{
	  int max_targets;
	  if (IS_AI(player))
		{
		  int mana_avail = has_mana(player, COLOR_ANY, 1);
		  max_targets = (mana_avail - 1) / 2;
		  max_targets = MAX(max_targets, 0);
		  max_targets = MIN(max_targets, 10);
		}
	  else
		max_targets = 10;

	  pick_up_to_n_targets(&td, "TARGET_LAND", max_targets);
	  if (cancel != 1 && charge_mana_for_activated_ability(player, card, MANACOST_XR(2 * instance->number_of_targets, 1)))
		{
		  tap_card(player, card);
		  kill_card(player, card, KILL_SACRIFICE);
		}
	  else
		instance->number_of_targets = 0;	// Since it was set before attempting to charge mana
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  int i;
	  for (i = 0; i < instance->number_of_targets; ++i)
		if (validate_target(player, card, &td, i))
		  kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
	}

  return 0;
}

int card_paradigm_shift(int player, int card, event_t event){
/* CARD_ID_PARADIGM_SHIFT	3022
Paradigm Shift	|1|U
Sorcery
Exile all cards from your library. Then shuffle your graveyard into your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = count_deck(player);
		int i;
		for( i=0; i<amount; i++){
			 rfg_top_card_of_deck(player);
		}
		amount = count_graveyard(player)-1;
		i=0;
		for( i=0; i<amount; i++){
			 from_graveyard_to_deck(player, 0, 1);
		}
		from_graveyard_to_deck(player, 0, 3);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_peacekeeper(int player, int card, event_t event){
/* CARD_ID_PEACEKEEPER	3023
Peacekeeper	|2|W
Creature - Human 1/1
At the beginning of your upkeep, sacrifice ~ unless you pay |1|W.
Creatures can't attack. */

	nobody_can_attack(player, card, event, ANYBODY);
	basic_upkeep(player, card, event, MANACOST_XW(1, 1));
	return 0;
}

void pendrell_effect(int player, int card, event_t event, kill_t kill_mode)
{
  upkeep_trigger_ability(player, card, event, ANYBODY);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  int c, p = current_turn;
	  char marked[151] = {0};
	  for (c = 0; c < active_cards_count[p]; ++c)
		if (in_play(p, c) && is_what(p, c, TYPE_CREATURE))
		  marked[c] = 1;

	  card_instance_t* inst;
	  for (c = 0; c < active_cards_count[p]; ++c)
		if (marked[c]	// was in play and a creature at start of resolution (iterating in reverse isn't sufficient)
			&& (inst = in_play(p, c))				// hasn't left play due to a trigger from a previously-unpaid upkeep
			&& !(inst->token_status & STATUS_DYING)	// hasn't been marked dying by a trigger from a previously-unpaid upkeep
			&& is_what(p, c, TYPE_CREATURE)			// hasn't stopped being a creature due to a trigger from a previously-unpaid upkeep
			&& (!has_mana(p, COLOR_COLORLESS, 1)
				|| do_dialog(p, p, c, player, card, " Pay upkeep\n Pass", 0) == 1
				|| !charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, p, COLOR_COLORLESS, 1)))
		  kill_card(p, c, kill_mode);
	}
}

int card_pendrell_mists(int player, int card, event_t event){
/* CARD_ID_PENDRELL_MISTS	3024
Pendrell Mists	|3|U
Enchantment
All creatures have "At the beginning of your upkeep, sacrifice this creature unless you pay |1." */

	pendrell_effect(player, card, event, KILL_SACRIFICE);

	return global_enchantment(player, card, event);
}

int card_phyrexian_furnace(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_FURNACE	3026
Phyrexian Furnace	|1
Artifact
|T: Exile the bottom card of target player's graveyard.
|1, Sacrifice ~: Exile target card from a graveyard. Draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			return 1;
		}
		int can_target_player[2] = {count_graveyard(0) > 0 && ! graveyard_has_shroud(0), count_graveyard(1) > 0 && ! graveyard_has_shroud(1)};
		if( can_target_player[0] + can_target_player[1] ){
			if( generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(1), 0, &td, NULL) ){
				return 1;
			}
		}
	}
	if( event == EVENT_ACTIVATE){
		instance->info_slot = instance->number_of_targets = 0;
		int abilities[2] = { generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL), 0};
		int can_target_player[2] = {count_graveyard(0) > 0 && ! graveyard_has_shroud(0), count_graveyard(1) > 0 && ! graveyard_has_shroud(1)};
		if( can_target_player[0] + can_target_player[1] ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME, MANACOST_X(1), 0, &td, NULL) ){
				abilities[1] = 1;
			}
		}
		int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
							"Exile bottom card", abilities[0], 6,
							"Exile target card & draw", abilities[1], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(choice-1)) ){
			if( choice == 1 ){
				if( pick_target(&td, "TARGET_PLAYER") ){
					instance->info_slot = choice;
					tap_card(player, card);
				}
			}
			if( choice == 2 ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");
				select_target_from_either_grave(player, card, 0, AI_MAX_CMC, AI_MAX_CMC, &this_test, 0, 1);
				if( spell_fizzled != 1 ){
					instance->info_slot = choice;
					kill_card(player, card, KILL_SACRIFICE);
				}
			}
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION){
		if( instance->info_slot == 1 && valid_target(&td) ){
			rfg_card_from_grave(instance->targets[0].player, 0);
		}
		if( instance->info_slot == 2 ){
			int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
			if( selected != -1 ){
				rfg_card_from_grave(instance->targets[0].player, selected);
				draw_cards(player, 1);
			}
		}
	}

	return 0;
}

static int can_draw_cards_as_cost_thunk(int player, int card, int number_of_age_counters)
{
  return can_draw_cards_as_cost(player, number_of_age_counters);
}

static int draw_cards_thunk(int player, int card, int number_of_age_counters)
{
  draw_cards(player, number_of_age_counters);
  return 1;
}

int card_psychic_vortex(int player, int card, event_t event)
{
  /* Psychic Vortex	|2|U|U
   * Enchantment
   * Cumulative upkeep-Draw a card.
   * At the beginning of your end step, sacrifice a land and discard your hand. */

  cumulative_upkeep_general(player, card, event, can_draw_cards_as_cost_thunk, draw_cards_thunk);

  if (current_turn == player && eot_trigger(player, card, event))
	{
	  controller_sacrifices_a_permanent(player, card, TYPE_LAND, SAC_NO_CANCEL);
	  discard_all(player);
	}

  return global_enchantment(player, card, event);
}

int card_revered_unicorn(int player, int card, event_t event)
{
  /* Revered Unicorn	|1|W
   * Creature - Unicorn 2/3
   * Cumulative upkeep |1
   * When ~ leaves the battlefield, you gain life equal to the number of age counters on it. */

  cumulative_upkeep(player, card, event, MANACOST_X(1));
  leaves_play(player, card, event, 1, 1);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
    gain_life(player, count_counters(player, card, COUNTER_AGE));
  return 0;
}

int card_rogue_elephant(int player, int card, event_t event){
/* CARD_ID_ROGUE_ELEPHANT	3033
Rogue Elephant	|G
Creature - Elephant 3/3
When ~ enters the battlefield, sacrifice it unless you sacrifice |Ha Forest. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
		if( ! check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test) ){
			ai_modifier -= 256;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int kill_me = 1;
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
		if( check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test) ){
			scnprintf(test.message, 100, "%s", get_hacked_land_text(player, card, "Select a %s to sacrifice", SUBTYPE_FOREST));
			if( new_sacrifice(player, card, player, 0, &test) ){
				kill_me = 0;
			}
		}
		if( kill_me ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_scorched_ruins(int player, int card, event_t event)
{
  /* Scorched Ruins	""
   * Land
   * If ~ would enter the battlefield, sacrifice two untapped lands instead. If you do, put ~ onto
   * the battlefield. If you don't, put it into its owner's graveyard. |T: Add |4 to your mana pool.
   */

  if (if_would_enter_bf_sac_two_untapped_lands_instead(player, card, event) &&
      !is_humiliated(player, card, AL_TRIG))
    return 0;

  if (event == EVENT_CAN_ACTIVATE)
    return CAN_TAP_FOR_MANA(player, card);

  if (event == EVENT_ACTIVATE)
    produce_mana_tapped(player, card, COLOR_COLORLESS, 4);

  if (event == EVENT_COUNT_MANA && affect_me(player, card) && CAN_TAP_FOR_MANA(player, card))
    declare_mana_available(player, COLOR_COLORLESS, 4);

  return 0;
}

int card_serenity(int player, int card, event_t event){
/* CARD_ID_SERENITY	3036
Serenity	|1|W
Enchantment
At the beginning of your upkeep, destroy all artifacts and enchantments. They can't be regenerated. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT | TYPE_ARTIFACT);
		int diff = 	check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &this_test) -
					check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test);
		ai_modifier += (diff * 8);
	}

	if( current_turn == player && upkeep_trigger(player, card, event) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT | TYPE_ARTIFACT);
		new_manipulate_all(player, card, ANYBODY, &this_test, KILL_BURY);
	}

	return 0;
}

int card_serras_blessing(int player, int card, event_t event){
/* CARD_ID_SERRAS_BLESSING	1605
Serra's Blessing	|1|W
Enchantment
Creatures you control have vigilance. (Attacking doesn't cause them to tap.) */

 if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
	if( event == EVENT_ABILITIES && affected_card_controller == player &&
		is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
	{
		vigilance(affected_card_controller, affected_card, event);
	}
 }
	return global_enchantment(player, card, event);
}

int card_serrated_biskelion(int player, int card, event_t event){
/* CARD_ID_SERRATED_BISKELION	3037
Serrated Biskelion	|3
Artifact Creature - Construct 2/2
|T: Put a -1/-1 counter on ~ and a -1/-1 counter on target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_M1_M1, 1);
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_M1_M1, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_shattered_crypt(int player, int card, event_t event){
/*CARD_ID_SHATTERED_CRYPT	3039
Shattered Crypt	|X|B|B
Sorcery
Return X target creature cards from your graveyard to your hand. You lose X life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_CAN_CAST){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		int max_cards = count_graveyard_by_type(player, TYPE_CREATURE);
		int generic_mana_available = has_mana(player, COLOR_ANY, 1);	// A useful trick from card_fireball().
		max_cards = MIN(max_cards, generic_mana_available);
		max_cards = MIN(max_cards, 10);

		if (max_cards <= 0 || graveyard_has_shroud(player)){
			ai_modifier -= 48;
			instance->info_slot = 0;
			return 0;
		}

		char buf[100];
		if (ai_is_speculating == 1){
			*buf = 0;
		} else if (max_cards == 1){
			strcpy(buf, "Select a creature card.");
		} else {
			sprintf(buf, "Select up to %d creature cards.", max_cards);
		}
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, buf);
		instance->info_slot = select_multiple_cards_from_graveyard(player, player, 0, AI_MAX_VALUE, &this_test, max_cards, &instance->targets[0]);
		int amount = MAX(0, (instance->info_slot + true_get_updated_casting_cost(player, card, -1, event, -1)));
		if (instance->info_slot > 0){
			charge_mana(player, COLOR_COLORLESS, amount);
		}
	}
	if (event == EVENT_RESOLVE_SPELL){
		int i, num_validated = 0;
		for (i = 0; i < instance->info_slot; ++i){
			int selected = validate_target_from_grave(player, card, player, i);
			if (selected != -1){
				from_grave_to_hand(player, selected, TUTOR_HAND);
				++num_validated;
			}
		}
		if (num_validated > 0){
			lose_life(player, instance->info_slot);
		} else if (instance->info_slot > 0) {
			spell_fizzled = 1;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_southern_paladin(int player, int card, event_t event)
{
  /* Southern Paladin	|2|W|W
   * Creature - Human Knight 3/3
   * |W|W, |T: Destroy target |Sred permanent. */

  return northern_southern_paladin(player, card, event, COLOR_RED);
}

int card_spinning_darkness(int player, int card, event_t event){
/* CARD_ID_SPINNING_DARKNESS	3042
Spinning Darkness	|4|B|B
Instant
You may exile the top three |Sblack cards of your graveyard rather than pay ~'s mana cost.
~ deals 3 damage to target non|Sblack creature. You gain 3 life. */

	if( event == EVENT_MODIFY_COST ){
		if( count_graveyard_by_color(player, COLOR_TEST_BLACK) >= 3 ){
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

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1  ){
			if( has_mana_to_cast_id(player, event, get_id(player, card)) ){
				int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Pitch Spinning Darkness", 1, 5,
								"Play normally", 1, 1);
				if( ! choice ){
					spell_fizzled = 1;
					return 0;
				}

				if( choice == 1 ){
					const int *grave = get_grave(player);
					int count = count_graveyard(player)-1;
					int removed = 0;
					while( count > -1 && removed < 3 ){
							if( cards_data[grave[count]].color & COLOR_TEST_BLACK){
								rfg_card_from_grave(player, count);
								removed++;
							}
							count--;
					}
					td.allow_cancel = 0;
				}

				if( choice == 2 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
				}
			}
		}
		if( spell_fizzled == 1 ){
			return 0;
		}
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
							get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 3);
			gain_life(player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_steel_golem(int player, int card, event_t event){
/* CARD_ID_STEEL_GOLEM	3043
Steel Golem	|3
Artifact Creature - Golem 3/4
You can't cast creature spells. */

	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card, AL_STATIC) ){
		if( affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			infinite_casting_cost();
		}
	}

   return 0;
}

int card_strands_of_night(int player, int card, event_t event){
/* CARD_ID_STRANDS_OF_NIGHT	3044
Strands of Night	|2|B|B
Enchantment
|B|B, Pay 2 life, Sacrifice |Ha Swamp: Return target creature card from your graveyard to the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");

	test_definition_t test2;
	default_test_definition(&test2, TYPE_LAND);
	test2.subtype = get_hacked_subtype(player, card, SUBTYPE_SWAMP);

	if( event == EVENT_CAN_ACTIVATE ){
		if( new_can_sacrifice_as_cost(player, card, &test2) ){
			return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_B(2), 2, NULL, NULL, 0, &test,
																	GS_GRAVE_RECYCLER);
		}
	}

	if( event == EVENT_ACTIVATE){
		scnprintf(test2.message, 100, "%s", get_hacked_land_text(player, card, "Select a %s to sacrifice.", SUBTYPE_SWAMP));
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test2);
		if (!sac){
			cancel = 1;
			return 0;
		}
		generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_B(2), 2, NULL, NULL, 0, &test, GS_GRAVE_RECYCLER);
		if( spell_fizzled != 1 ){
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
		else{
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
		}
	}

	return 0;
}

int card_straw_golem(int player, int card, event_t event){
/* CARD_ID_STRAW_GOLEM	3045
Straw Golem	|1
Artifact Creature - Golem 2/3
When an opponent casts a creature spell, sacrifice ~. */

	sp_ability_trigger(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);
	if (event == EVENT_SPA){
		kill_card(player, card, KILL_SACRIFICE);
	}
	return 0;
}

int card_sylvan_hierophant(int player, int card, event_t event)
{
/* CARD_ID_SYLVAN_HIEROPHANT	3046
Sylvan Hierophant	|1|G
Creature - Human Cleric 1/2
When ~ dies, exile ~, then return another target creature card from your graveyard to your hand. */

  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	{
	  exile_from_owners_graveyard(player, card);
	  if (count_graveyard_by_type(player, TYPE_CREATURE) > 0 && !graveyard_has_shroud(player))
		{
		  test_definition_t this_test;
		  new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
		  new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 1, AI_MAX_VALUE, &this_test);
		}
	}

  return 0;
}

int card_tariff(int player, int card, event_t event){
/* CARD_ID_TARIFF	3047
Tariff	|1|W
Sorcery
Each player sacrifices the creature he or she controls with the highest converted mana cost unless he or she pays that creature's mana cost.
If two or more creatures a player controls are tied for highest cost, that player chooses one. */

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p, {
					card_instance_t *instance = get_card_instance(player, card);
					instance->targets[0].player = instance->targets[0].card = -1;
					int max_t = -1;
					int count = active_cards_count[p]-1;
					int doubles_count = 1;
					while( count > -1 ){
							if( in_play(p, count) && is_what(p, count, TYPE_CREATURE) ){
								if( get_cmc(p, count) > max_t ){
									instance->targets[0].player = p;
									instance->targets[0].card = count;
									doubles_count = 1;
									max_t = get_cmc(p, count);
								}
								else if( get_cmc(p, count) == max_t ){
										doubles_count++;
								}
							}
							count--;
					}
					if( doubles_count > 1 ){
						target_definition_t td;
						default_target_definition(player, card, &td, TYPE_CREATURE);
						td.allowed_controller = p;
						td.preferred_controller = p;
						td.who_chooses = p;
						td.illegal_abilities = 0;
						td.allow_cancel = 0;
						td.extra = max_t;
						td.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
						char msg[100];
						scnprintf(msg, 100, "Select a creature you control with CMC %d.", max_t);
						instance->number_of_targets = 0;
						new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT);
					}

					if( instance->targets[0].player != -1 && instance->targets[0].card != -1 ){
						int kill = 1;
						if( has_mana_to_cast_iid(p, event, get_card_instance(instance->targets[0].player, instance->targets[0].card)->internal_card_id) ){
							charge_mana_from_id(p, -1, event, get_id(instance->targets[0].player, instance->targets[0].card));
							if( spell_fizzled != 1 ){
								kill = 0;
							}
						}
						if( kill == 1 ){
							kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
						}
					}
				};
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int teferis_veil_effect(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			phase_out(p, c);
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_teferis_veil(int player, int card, event_t event){
/* CARD_ID_TEFERIS_VEIL	3048
Teferi's Veil	|1|U
Enchantment
Whenever a creature you control attacks, it phases out at end of combat.
(While it's phased out, it's treated as though it doesn't exist. It phases in before you untap during your next untap step.) */

  store_attackers(player, card, event, DAT_TRACK, player, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int amt = instance->info_slot;
	  unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
	  for (--amt; amt >= 0; --amt)
		if (in_play(current_turn, attackers[amt]))
		  create_targetted_legacy_effect(player, card, &teferis_veil_effect, current_turn, attackers[amt]);
	}

  return global_enchantment(player, card, event);
}

int card_thran_tome(int player, int card, event_t event){
/* CARD_ID_THRAN_TOME	3051
Thran Tome	|4
Artifact
|5, |T: Reveal the top three cards of your library. Target opponent chooses one of those cards. Put that card into your graveyard, then draw two cards. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE )
		ai_modifier += (count_subtype(player, TYPE_LAND, -1) * 8);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int to_reveal = MIN(3, count_deck(player));
			if( ! to_reveal ){
				return 0;
			}

			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card.");
			this_test.create_minideck = to_reveal;
			this_test.no_shuffle = 1;
			new_global_tutor(instance->targets[0].player, player, TUTOR_FROM_DECK, TUTOR_GRAVE, 1, AI_MAX_VALUE, &this_test);
			to_reveal--;
			if( to_reveal ){
				draw_cards(player, to_reveal);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST_X(5), 0, &td, NULL);
}

int card_thunderbolt(int player, int card, event_t event){
	/* Thunderbolt	|1|R	0x200336f
	 * Instant
	 * Choose one -
	 * * ~ deals 3 damage to target player or planeswalker.
	 * * ~ deals 4 damage to target creature with flying. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.required_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_DAMAGE_PLAYER = 1,
		CHOICE_DAMAGE_FLYIER
	};

	if( event == EVENT_CAN_CAST && (can_target(&td) || can_target(&td2)) ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"3 damage to player", can_target(&td), life[1-player] <= 3 ? 10 : 1,
								"4 damage to flyer", can_target(&td2), 5);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_DAMAGE_PLAYER ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
		}
		if( instance->info_slot == CHOICE_DAMAGE_FLYIER ){
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td2, "Select target creature with flying.", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( instance->info_slot == CHOICE_DAMAGE_PLAYER && valid_target(&td) ){
			damage_target0(player, card, 3);
		}

		if( instance->info_slot == CHOICE_DAMAGE_FLYIER && valid_target(&td2) ){
			damage_target0(player, card, 4);
		}

		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_thundermare(int player, int card, event_t event){
/* CARD_ID_THUNDERMARE	3053
Thundermare	|5|R
Creature - Elemental Horse 5/5
Haste (This creature can attack the turn it comes under your control.)
When ~ enters the battlefield, tap all other creatures. */

	// Used externally

	haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		new_manipulate_all(player, card, ANYBODY, &this_test, ACT_TAP);
	}

	return 0;
}

// tolarian drake --> sandbar crocodile

static int entrancer_effect(int player, int card, event_t event){

	if( end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance(player, card);
		gain_control_permanently(player, instance->damage_target_player, instance->damage_target_card);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

static void tolarian_entrancer_ability(int player, int card, int t_player, int t_card)
{
	create_targetted_legacy_effect(player, card, &entrancer_effect, t_player, t_card);
}

int card_tolarian_entrancer(int player, int card, event_t event){
/* CARD_ID_TOLARIAN_ENTRANCER	3056
Tolarian Entrancer	|1|U
Creature - Human Wizard 1/1
Whenever ~ becomes blocked by a creature, gain control of that creature at end of combat. */

  if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG))
		get_card_instance(player, card)->destroys_if_blocked |= DIFB_DESTROYS_ALL;

  if (event == EVENT_DECLARE_BLOCKERS && current_turn == player && !is_humiliated(player, card, AL_TRIG))
	    for_each_creature_blocking_me(player, card, tolarian_entrancer_ability, player, card);

	return 0;
}

int card_tolarian_serpent(int player, int card, event_t event){
/* CARD_ID_TOLARIAN_SERPENT	3057
Tolarian Serpent	|5|U|U
Creature - Serpent 7/7
At the beginning of your upkeep, put the top seven cards of your library into your graveyard. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		mill(player, 7);
	}
	return 0;
}

int card_touchstone(int player, int card, event_t event){
/* CARD_ID_TOUCHSTONE	3058
Touchstone	|2
Artifact
|T: Tap target artifact you don't control. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target artifact you don't control.");
		if( spell_fizzled != 1 ){
			ai_mod_for_tap_effects(player, card, 0);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, NULL);
}

int card_tranquil_grove(int player, int card, event_t event){
/* CARD_ID_TRANQUIL_GROVE	3059
Tranquil Grove	|1|G
Enchantment
|1|G|G: Destroy all other enchantments. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT);
		this_test.not_me = 1;
		new_manipulate_all(instance->parent_controller, instance->parent_card, ANYBODY, &this_test, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XG(1, 2), 0, NULL, NULL);
}

int card_uktabi_efreet(int player, int card, event_t event){
/* CARD_ID_UKTABI_EFREET	3060
Uktabi Efreet	|2|G|G
Creature - Efreet 5/4
Cumulative upkeep |G (At the beginning of your upkeep, put an age counter on this permanent,
then sacrifice it unless you pay its upkeep cost for each age counter on it.) */

	cumulative_upkeep(player, card, event, MANACOST_G(1));
	return 0;
}

int card_urborg_justice(int player, int card, event_t event){
/* CARD_ID_URBORG_JUSTICE	3061
Urborg Justice	|B|B
Instant
Target opponent sacrifices a creature for each creature put into your graveyard from the battlefield this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = get_dead_count(player, TYPE_CREATURE);
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			test.qty = amount;
			new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED | SAC_NO_CANCEL, &test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_urborg_stalker(int player, int card, event_t event){
/* CARD_ID_URBORG_STALKER	3062
Urborg Stalker	|3|B
Creature - Horror 2/4
At the beginning of each player's upkeep, if that player controls a non|Sblack, nonland permanent, ~ deals 1 damage to that player. */

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && reason_for_trigger_controller == player ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.type_flag = DOESNT_MATCH;
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		this_test.color_flag = DOESNT_MATCH;

		upkeep_trigger_ability_mode(player, card, event, ANYBODY, check_battlefield_for_special_card(player, card, current_turn, 0, &this_test) ?
								RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.type_flag = DOESNT_MATCH;
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		this_test.color_flag = DOESNT_MATCH;
		if( check_battlefield_for_special_card(player, card, current_turn, 0, &this_test) ){
			damage_player(current_turn, 1, player, card);
		}
	}
	return 0;
}

int card_veteran_explorer(int player, int card, event_t event)
{
/* CARD_ID_VETERAN_EXPLORER	3063
Veteran Explorer	|G
Creature - Human Soldier Scout 1/1
When ~ dies, each player may search his or her library for up to two basic land cards and put them onto the battlefield.
Then each player who searched his or her library this way shuffles it. */

  int p;
  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	for (p = 0; p < 2; ++p)
	  if (do_dialog(p, player, card, -1, -1, " Search library\n Pass", 0) == 0)
		tutor_basic_lands(p, TUTOR_PLAY, 2);

  return 0;
}

int card_vitalize(int player, int card, event_t event){
/* CARD_ID_VITALIZE	3064
Vitalize	|G
Instant
Untap all creatures you control. */

	if( event == EVENT_CAN_CAST){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.state = STATE_TAPPED;
		ai_modifier += (check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test) * 12);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_vodalian_illusionist(int player, int card, event_t event){
/* CARD_ID_VODALIAN_ILLUSIONIST	3065
Vodalian Illusionist	|2|U
Creature - Merfolk Wizard 2/2
|U|U, |T: Target creature phases out. (While it's phased out, it's treated as though it doesn't exist.
It phases in before its controller untaps during his or her next untap step.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			phase_out(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(2), 0, &td, "TARGET_CREATURE");
}

int card_volounteer_reserves(int player, int card, event_t event){
/* CARD_ID_VOLUNTEER_RESERVES	3066
Volunteer Reserves	|1|W
Creature - Human Soldier 2/4
Banding
Cumulative upkeep |1 (At the beginning of your upkeep, put an age counter on this permanent, then sacrifice it unless you pay its upkeep cost for each age counter on it.)*/

	cumulative_upkeep(player, card, event, MANACOST_X(1));
	return 0;
}

int card_wave_of_terror(int player, int card, event_t event){

  /* Wave of Terror	|2|B
   * Enchantment
   * Cumulative upkeep |1
   * At the beginning of your draw step, destroy each creature with converted mana cost equal to the number of age counters on ~. They can't be regenerated. */

  cumulative_upkeep(player, card, event, MANACOST_X(1));

  if (trigger_condition == TRIGGER_DRAW_PHASE && affect_me(player, card)
	  && current_turn == player && reason_for_trigger_controller == player)
	{
		if (event == EVENT_TRIGGER)
			event_result |= RESOLVE_TRIGGER_MANDATORY;

		if( event == EVENT_RESOLVE_TRIGGER){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			test.cmc = count_counters(player, card, COUNTER_AGE);
			new_manipulate_all(player, card, ANYBODY, &test, KILL_BURY);
		}
	}

  return global_enchantment(player, card, event);
}

static int effect_enable_flash_land(int player, int card, event_t event)
{
  /* Local data:
   * targets[2].player: allowable types
   * targets[2].card: illegal types
   * targets[3].player: prompt, a const char* cast to an int.
   * targets[3].card: whether prompt it literal. */

  if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);

  if (!IS_ACTIVATING(event))
	return 0;

  card_instance_t* instance = get_card_instance(player, card);

  target_definition_t td;
  base_target_definition(player, card, &td, instance->targets[2].player);
  td.illegal_type = instance->targets[2].card;
  td.allowed_controller = td.preferred_controller = player;
  td.zone = TARGET_ZONE_HAND;

  return can_play_cards_as_though_they_had_flash(player, card, event, &td, (const char*)(instance->targets[3].player), instance->targets[3].card);
}

int enable_flash_land(int player, int card, event_t event, int x, int b, int u, int g, int r, int w, int required_type, int illegal_type, const char* prompt, int literal)
{
  /* Land
   * |T: Add |1 to your mana pool.
   * [xbugrw], |T: You may cast [required_type], non[illegal_type] cards this turn as though they had flash. */

  if (IS_ACTIVATING(event))
	{
	  target_definition_t td;
	  base_target_definition(player, card, &td, required_type);
	  td.illegal_type = illegal_type;
	  td.allowed_controller = td.preferred_controller = player;
	  td.zone = TARGET_ZONE_HAND;

	  enum
	  {
		CHOICE_MANA = 1,
		CHOICE_FLASH
	  } choice;

	  if (paying_mana())
		choice = event == EVENT_CAN_ACTIVATE ? CAN_TAP_FOR_MANA(player, card) : CHOICE_MANA;
	  else
		choice = DIALOG(player, card, event,
						"Add 1", CAN_TAP_FOR_MANA(player, card), -1,
						"Flash", CAN_TAP(player, card) && CAN_ACTIVATE(player, card, MANACOST6(x+1,b,u,g,r,w)), IS_AI(player) && can_target(&td) ? 1 : -1);

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (event == EVENT_ACTIVATE)
		switch (choice)
		  {
			case CHOICE_MANA:
			  mana_producer(player, card, event);
			  break;

			case CHOICE_FLASH:
			  tap_card(player, card);
			  if (!charge_mana_for_activated_ability(player, card, x,b,u,g,r,w))
				untap_card_no_event(player, card);
			  break;
		  }
	  else	// event == EVENT_RESOLVE_ACTIVATION
		if (choice == CHOICE_FLASH)
		  {
			int leg = create_legacy_activate(player, card, effect_enable_flash_land);
			if (leg == -1)
			  return 0;
			card_instance_t* legacy = get_card_instance(player, leg);
			legacy->targets[2].player = required_type;
			legacy->targets[2].card = illegal_type;
			legacy->targets[3].player = (int)prompt;
			legacy->targets[3].card = literal;

			// If there's cards on the stack below this, allow a response before further unwinding the stack.
			if (stack_size > 0)
			  allow_response_to_activation(player, card);
		  }

	  return 0;
	}
  else
	return mana_producer(player, card, event);
}

int card_winding_canyons(int player, int card, event_t event)
{
  /* Winding Canyons	""
   * Land
   * |T: Add |1 to your mana pool.
   * |2, |T: Until end of turn, you may play creature cards as though they had flash. */
  return enable_flash_land(player, card, event, MANACOST_X(2), TYPE_CREATURE, 0, "SELECT_CREATURE", 0);
}

int card_xanthic_statue(int player, int card, event_t event){
/* CARD_ID_XANTHIC_STATUE	3070
Xanthic Statue	|8
Artifact
|5: Until end of turn, ~ becomes an 8/8 Golem artifact creature with trample. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int legacy = artifact_animation(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
							1, 8, 8, KEYWORD_TRAMPLE, 0);
		get_card_instance(player, legacy)->targets[9].card = SUBTYPE_GOLEM;
	}

	return generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_X(5), 0, NULL, NULL);
}

int card_zombie_scavengers(int player, int card, event_t event){
/* CARD_ID_ZOMBIE_SCAVENGERS	3071
Zombie Scavengers	|2|B
Creature - Zombie 3/1
Exile the top creature card of your graveyard: Regenerate ~. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( regeneration(player, card, event, MANACOST0) && count_graveyard_by_type(player, TYPE_CREATURE) > 0 ){
			return 99;
		}
	}

	if( event == EVENT_ACTIVATE ){
		regeneration(player, card, event, MANACOST0);
		if( spell_fizzled != 1 ){
			remove_top_creature_card_from_grave(player);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return regeneration(player, card, event, MANACOST0);
	}

	return 0;
}

/* ManalinkEh 2022: 0x22f65e0; original cards/weatherlight.c:3979. */
int card_apathy(int player, int card, event_t event)
{
  /* Apathy	|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature doesn't untap during its controller's untap step.
   * At the beginning of the upkeep of enchanted creature's controller, that player may discard a card at random. If the player does, untap that creature. */

  card_instance_t *instance;
  int result;
  unsigned int argument7;

  instance = in_play(player, card);
  if (instance && !is_humiliated(player, card, AL_STATIC) &&
      (instance = get_card_instance(player, card), instance->damage_target_player != -1))
  {
    instance = get_card_instance(player, card);
    does_not_untap((int)instance->damage_target_player, instance->damage_target_card, event);
    upkeep_trigger_ability(player, card, event, (int)instance->damage_target_player);
    if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    {
      result = (int)instance->damage_target_player;
      if ((result == current_turn) && (0 < hand_count[result]))
      {

        result = is_tapped(result, instance->damage_target_card);
        argument7 = 1;
        if (result != 0)
        {
          argument7 = (unsigned int)(hand_count[current_turn] < 3);
        }
        result = do_dialog((int)instance->damage_target_player, player, card, -1, -1,
                           " Discard a card/untap crearture\n Pass", argument7);
        if ((result == 0) &&
            (discard((int)instance->damage_target_player, 1, player), spell_fizzled != 1))
        {
          untap_card((int)instance->damage_target_player, instance->damage_target_card);
        }
      }
    }
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  return disabling_aura(player, card, event);
}

/* ManalinkEh 2022: 0x22f6810; original cards/weatherlight.c:4016. */
int card_arctic_wolves(int player, int card, event_t event)
{
  /* Arctic Wolves	|3|G|G
   * Creature - Wolf 4/5
   * Cumulative upkeep |2 (At the beginning of your upkeep, put an age counter on this permanent, then sacrifice it unless you pay its upkeep cost for each age counter on it.)
   * When this creature enters, draw a card. */

  cumulative_upkeep(player, card, event, 2, 0, 0, 0, 0, 0);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  draw_a_card(player);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22f68b0; original cards/weatherlight.c:4032. */
int card_avizoa(int player, int card, event_t event)
{
  /* Avizoa	|3|U
   * Creature - Jellyfish 2/2
   * Flying
   * |0: This creature gets +2/+2 until end of turn. You skip your next untap step. Activate only once each turn. */

  int result;
  card_instance_t *instance;
  if (event == EVENT_CHECK_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x2000, 0, 0, 0, 0,
                                              0, 0, 0, NULL, NULL);
      if (result != 0)
      {
        check_abilities_power = check_abilities_power + 2;
        check_abilities_toughness = check_abilities_toughness + 2;
      }
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = pump_until_eot(instance->parent_controller, instance->parent_card,
                                 instance->parent_controller, instance->parent_card, 2, 2);
    alternate_legacy_text(1, player, result);
    result = target_player_skips_next_untap(player, card, player);
    alternate_legacy_text(2, player, result);
  }
  return generic_activated_ability(player, card, event, 0x2000, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x22f6a70; original cards/weatherlight.c:4055. */
int card_benalish_missionary(int player, int card, event_t event)
{
  /* Benalish Missionary	|W
   * Creature - Human Cleric 1/1
   * |1|W, |T: Prevent all combat damage that would be dealt by target blocked creature this turn. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_ISBLOCKED;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 1, 0, 0, 0, 0, 1, 0,
                                         &td, NULL);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 5, 1, 0, 0, 0, 0, 1, 0, &td,
                                "TARGET_CREATURE");
      if (spell_fizzled == 1)
      {
        return 0;
      }
      result = is_attacking(instance->targets[0].player, instance->targets[0].card);
      ai_modifier = ai_modifier + (unsigned int)((result == 0) ? -96 : 0) + 0x30;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_ISBLOCKED;
    instance = get_card_instance(player, card);
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  negate_combat_damage_this_turn(player, card, instance->targets[0].player,
                                 instance->targets[0].card, 0);
  return 0;
}

/* ManalinkEh 2022: 0x22f6c60; original cards/weatherlight.c:4088. */
int card_bogardan_Firefiend(int player, int card, event_t event)
{
  /* Bogardan Firefiend	|2|R
   * Creature - Elemental Spirit 2/1
   * When this creature dies, it deals 2 damage to target creature. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_CREATURE");
      if (result != 0)
      {
        damage_creature(instance->targets[0].player, instance->targets[0].card, 2, player, card);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22f6d20; original cards/weatherlight.c:4108. */
int card_boiling_blood(int player, int card, event_t event)
{
  /* Boiling Blood	|2|R
   * Instant
   * Target creature attacks this turn if able.
   * Draw a card. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
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
      if ((card == affected_card) && (player == affected_card_controller))
      {
        if ((current_turn == player) || (result = ai_modifier + 0x30, 0x15 < current_phase))
        {
          result = ai_modifier + -0x60;
        }
        ai_modifier = result;
        result = check_for_special_ability(instance->targets[0].player, instance->targets[0].card,
                                           0x400);
        if (result == 0)
        {
          if ((player == 1) || (result = has_subtype(instance->targets[0].player,
                                                     instance->targets[0].card, SUBTYPE_WALL),
                                result == 0))
          {
            instance->targets[0].player = 0;
            return generic_spell(player, card, event, 0x81, &td,
                                 "Select target creature it attacks this turn if able.", 1, NULL);
          }
          instance->targets[0].player = 1;
        }
        ai_modifier = ai_modifier + -0x30;
      }
      return generic_spell(player, card, event, 0x81, &td,
                           "Select target creature it attacks this turn if able.", 1, NULL);
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 2);
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          pump_ability_until_eot(player, card, instance->targets[0].player,
                                 instance->targets[0].card, 0, 0, 0, 0x400);
          draw_cards(player, 1);
        }
        kill_card(player, card, KILL_DESTROY);
      }
      return generic_spell(player, card, event, 0x81, &td,
                           "Select target creature it attacks this turn if able.", 1, NULL);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  default_target_definition(player, card, &td, 2);
  get_card_instance(player, card);
  return generic_spell(player, card, event, 0x81, &td,
                       "Select target creature it attacks this turn if able.", 1, NULL);
}

/* ManalinkEh 2022: 0x22f6f90; original cards/weatherlight.c:4154. */
int card_cinder_giant(int player, int card, event_t event)
{
  /* Cinder Giant	|3|R
   * Creature - Giant 5/3
   * At the beginning of your upkeep, this creature deals 2 damage to each other creature you control. */

  test_definition_t this_test;
  upkeep_trigger_ability(player, card, event, player);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    return 0;
  }
  default_test_definition(&this_test, 2);
  this_test.not_me = 1;
  new_damage_all(player, card, player, 2, 0, &this_test);
  return 0;
}

/* ManalinkEh 2022: 0x22eabf0; original cards/weatherlight.c:4171. */
static void blocking_enchanted(int player, int card, int t_player, int t_card)
{
  int result;

  result = has_subtype(t_player, t_card, SUBTYPE_WALL);
  if (result != 0)
  {
    return;
  }
  kill_card(t_player, t_card, KILL_DESTROY);
  return;
}

/* ManalinkEh 2022: 0x22f7020; original cards/weatherlight.c:4176. */
int card_coils_of_the_medusa(int player, int card, event_t event)
{
  /* Coils of the Medusa	|1|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/-1.
   * Sacrifice this Aura: Destroy all non-Wall creatures blocking enchanted creature. */

  card_instance_t *instance;
  card_instance_t *aura;
  card_instance_t *attached_creature;
  int attached_card;
  int p;

  aura = in_play(player, card);
  if (!(((aura == NULL) || (is_humiliated(player, card, AL_STATIC) != 0)) ||
        (aura = get_card_instance(player, card), aura->damage_target_player < '\0')))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ||
        ((uint32_t)(event - EVENT_DECLARE_BLOCKERS) & 0xfffffff7) == 0 || event == EVENT_RESOLVE_ACTIVATION)
    {
      p = (int)instance->damage_target_player;
      attached_card = instance->damage_target_card;
      attached_creature = get_card_instance(p, attached_card);
      if (current_turn == player && (attached_creature->state & 4) != 0)
        instance->info_slot = 0x42;
      if (event == EVENT_CAN_ACTIVATE)
        return generic_activated_ability(player, card, event, 8, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
      if (event == EVENT_ACTIVATE)
        generic_activated_ability(player, card, event, 8, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
      if (event == EVENT_RESOLVE_ACTIVATION && instance->info_slot == 0x42)
        for_each_creature_blocking_me(p, attached_card, blocking_enchanted, player, card);
      return 0;
    }
  }
  return generic_aura(player, card, event, player, 1, -1, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22eac40; original cards/weatherlight.c:4211. */
int source_deals_double_damage(int player, int card, event_t event)
{
  int dmg;
  card_instance_t *damage;
  card_instance_t *instance;
  bool is_damage_card;

  damage = damage_being_dealt(event);
  instance = get_card_instance(player, card);
  if (((damage != NULL) && (damage->damage_source_player == player)) &&
      (damage->damage_source_card == instance->targets[0].card))
  {
    dmg = damage->info_slot * 2;
    is_damage_card = damage->internal_card_id == damage_card;
    damage->info_slot = dmg;
    if (is_damage_card)
    {
      damage_target0(player, card, dmg);
    }
    kill_card(player, card, KILL_EXILE);
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x22ead00; original cards/weatherlight.c:4228. */
int source_damage_prevented(int player, int card, event_t event)
{
  card_instance_t *damage;
  card_instance_t *instance;

  damage = damage_being_dealt(event);
  instance = get_card_instance(player, card);
  if (((damage != NULL) && (damage->damage_source_player == player)) &&
      (damage->damage_source_card == instance->targets[0].card))
  {
    if (-1 < damage->info_slot)
    {
      damage->info_slot = 0;
    }
    kill_card(player, card, KILL_EXILE);
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22f72b0; original cards/weatherlight.c:4245. */
int card_desperate_gambit(int player, int card, event_t event)
{
  /* Desperate Gambit	|R
   * Instant
   * Choose a source you control and flip a coin. If you win the flip, the next time that source would deal damage this turn, it deals double that damage instead. If you lose the flip, the next time it would deal damage this turn, prevent that damage. */

  int target_card;
  card_instance_t *instance;
  int target_player_or_result;
  int (*func_ptr)(int, int, event_t);
  target_definition_t td;

  default_target_definition(player, card, &td, 0x47);
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.illegal_abilities = 0;
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    target_player_or_result = counterspell(player, card, EVENT_CAN_CAST, NULL, 0);
    if (target_player_or_result != 0)
    {
      return target_player_or_result;
    }
    if (generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL) != 0)
    {
      return (((land_can_be_played & 4U) == 0) ? 1U : 99U);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      instance->info_slot = 0;
      if (counterspell(player, card, EVENT_CAN_CAST, NULL, 0) == 0)
      {
        return generic_spell(
            player, card, EVENT_CAST_SPELL, 0x81, &td,
            "Choose a source you control if win the flip deals double damage else prevented it.", 1,
            NULL);
      }
      counterspell(player, card, EVENT_CAST_SPELL, NULL, 0);
      ((unsigned char *)&instance->info_slot)[0] = 1;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    if (instance->info_slot == 1)
    {
      target_player_or_result = counterspell_validate(player, card, NULL, 0);
    }
    else
    {
      target_player_or_result = valid_target(&td);
    }
    if (target_player_or_result != 0)
    {
      target_player_or_result = instance->targets[0].player;
      target_card = instance->targets[0].card;
      if (target_player_or_result != -1)
      {
        if (flip_a_coin(player, card) == 0)
        {
          if (instance->info_slot != 1)
          {
            create_targetted_legacy_effect(player, card, source_damage_prevented, player,
                                           target_card);
            kill_card(player, card, KILL_DESTROY);
            return 0;
          }
          func_ptr = source_damage_prevented;
        }
        else
        {
          if (instance->info_slot != 1)
          {
            create_targetted_legacy_effect(player, card, source_deals_double_damage, player,
                                           target_card);
            kill_card(player, card, KILL_DESTROY);
            return 0;
          }
          func_ptr = source_deals_double_damage;
        }
        instance = get_card_instance(player, create_legacy_effect(player, card, func_ptr));
        instance->targets[0].player = target_player_or_result;
        instance->targets[0].card = target_card;
      }
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x22f7590; original cards/weatherlight.c:4324. */
int card_familiar_ground(int player, int card, event_t event)
{
  /* Familiar Ground	|2|G
   * Enchantment
   * Each creature you control can't be blocked by more than one creature. */

  int result;

  if (event == EVENT_ABILITIES)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((result == 0) && (affected_card_controller == player))
    {
      maxblockers_1(player, affected_card, EVENT_ABILITIES);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22e9b20; original cards/weatherlight.c:4338. */
static char *is_already_damaged(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
  card_instance_t *instance;
  char *requirement_error;

  instance = get_card_instance(player, card);
  requirement_error = NULL;
  if (instance->damage_on_card == 0)
  {
    requirement_error = "must be a creature that was dealt damage this turn.";
  }
  return requirement_error;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22f7600; original cards/weatherlight.c:4345. */
int card_fatal_blow(int player, int card, event_t event)
{
  /* Fatal Blow	|B
   * Instant
   * Destroy target creature that was dealt damage this turn. It can't be regenerated. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.extra = (int32_t)is_already_damaged;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td,
                              "Select target creature that was dealt damage this turn, bury it.", 1,
                              NULL);
}
#endif

/* ManalinkEh 2022: 0x22f7770; original cards/weatherlight.c:4371. */
int card_fit_of_rage(int player, int card, event_t event)
{
  /* Fit of Rage	|1|R
   * Sorcery
   * Target creature gets +3/+3 and gains first strike until end of turn. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      pump_ability_until_eot(player, card, instance->targets[0].player,
                             instance->targets[0].card, 3, 3, 0x100, 0);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22f78e0; original cards/weatherlight.c:4397. */
int card_fungus_elemental(int player, int card, event_t event)
{
  /* Fungus Elemental	|3|G
   * Creature - Fungus Elemental 3/3
   * |G, Sacrifice a Forest: Put a +2/+2 counter on this creature. Activate only if this creature entered this turn. */

  const char *sacrifice_prompt;
  card_instance_t *instance;
  int result;
  test_definition_t test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    instance->info_slot = instance->info_slot | 2;
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      sacrifice_prompt = get_hacked_land_text(player, card, "Select a %s to sacrifice", 0x5001);
      new_default_test_definition(&test, 0x47, sacrifice_prompt);
      test.subtype = get_hacked_subtype(player, card, 0x5001);
      if (event == EVENT_CAN_ACTIVATE)
      {
        result = generic_activated_ability_with_sacrifice_as_cost(
            player, card, EVENT_CAN_ACTIVATE, GAA_NONE, 0, 0, 0, 1, 0, 0, 0, NULL, NULL, TYPE_NONE,
            &test, SARV_0);
        if (result == 0)
        {
          return 0;
        }
        instance = get_card_instance(player, card);
        if (((unsigned int)instance->info_slot & 2) == 0)
        {
          return 0;
        }
        return 1;
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      {
        return 0;
      }
      sacrifice_prompt = get_hacked_land_text(player, card, "Select a %s to sacrifice", 0x5001);
      new_default_test_definition(&test, 0x47, sacrifice_prompt);
      test.subtype = get_hacked_subtype(player, card, 0x5001);
    }
    if (event == EVENT_ACTIVATE)
    {
      return generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_ACTIVATE,
                                                              GAA_NONE, 0, 0, 0, 1, 0, 0, 0, NULL,
                                                              NULL, TYPE_NONE, &test, SARV_0);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      add_counters(player, card, player, card, COUNTER_P2_P2, 1);
      return 0;
    }
    if (event == EVENT_CLEANUP)
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x22f7ba0; original cards/weatherlight.c:4436. */
int card_guided_strike(int player, int card, event_t event)
{
  /* Guided Strike	|1|W
   * Instant
   * Target creature gets +1/+0 and gains first strike until end of turn.
   * Draw a card. */

  int result;
  result = vanilla_instant_pump(player, card, event, 2, player, 1, 0, 0x1000100, 0);
  return result;
}

/* ManalinkEh 2022: 0x22f7bf0; original cards/weatherlight.c:4446. */
int card_heat_stroke(int player, int card, event_t event)
{
  /* Heat Stroke	|2|R
   * Enchantment
   * At end of combat, destroy each creature that blocked or was blocked this turn. */

  int result;
  card_instance_t *instance;
  test_definition_t test;

  if (event == EVENT_CHANGE_TYPE)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      instance = get_card_instance(affected_card_controller, affected_card);
      instance->destroys_if_blocked = instance->destroys_if_blocked | 0xc;
    }
  }
  result = end_of_combat_trigger(player, card, event, 2);
  if (result != 0)
  {
    new_default_test_definition(&test, 2, "");
    test.state = 0x404c;
    new_manipulate_all(player, card, 2, &test, ACT_KILL_DESTROY);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22f7ce0; original cards/weatherlight.c:4470. */
int card_hurloon_shaman(int player, int card, event_t event)
{
  /* Hurloon Shaman	|1|R|R
   * Creature - Minotaur Shaman 2/3
   * When this creature dies, each player sacrifices a land of their choice. */

  int result;
  test_definition_t test;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  new_default_test_definition(&test, 1, "Select a land to sacrifice.");
  new_sacrifice(player, card, current_turn, SAC_CAUSED | SAC_NO_CANCEL, &test);
  new_sacrifice(player, card, current_turn ^ 1, SAC_CAUSED | SAC_NO_CANCEL, &test);
  return 0;
}

/* ManalinkEh 2022: 0x22f7d90; original cards/weatherlight.c:4484. */
int card_jangling_automaton(int player, int card, event_t event)
{
  /* Jangling Automaton	|3
   * Artifact Creature - Construct 3/2
   * Whenever this creature attacks, untap all creatures defending player controls. */

  test_definition_t test;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }
  new_default_test_definition(&test, 2, "");
  test.not_me = 1;
  new_manipulate_all(player, card, 1 - current_turn, &test, ACT_UNTAP);
  return 0;
}

/* ManalinkEh 2022: 0x22f7e40; original cards/weatherlight.c:4502. */
int card_kithkin_armor(int player, int card, event_t event)
{
  /* Kithkin Armor	|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature can't be blocked by creatures with power 3 or greater.
   * Sacrifice this Aura: The next time a source of your choice would deal damage to enchanted creature this turn, prevent that damage. */

  card_instance_t *instance;
  int result;
  card_instance_t *aura_in_play;
  card_instance_t *parent;
  target_definition_t td;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        default_target_definition(player, card, &td, 2);
        td.preferred_controller = player;
        return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL) ||
             ((event == EVENT_CAN_CHANGE_TARGET || (event == EVENT_CHANGE_TARGET))))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
    }
    if (((event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA) &&
        (((((2 < (uint32_t)(event - EVENT_POWER) && (event != EVENT_RESOLVE_MOVING_AURA)) &&
            (event != EVENT_CHANGE_TYPE)) &&
           ((event != EVENT_SET_COLOR && (event != EVENT_GET_SELECTED_CARD)))) &&
          (event != EVENT_STATIC_EFFECTS))))
    {
      aura_in_play = in_play(player, card);
      if (((aura_in_play != NULL) &&
           (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
          (aura_in_play = get_card_instance(player, card), -1 < aura_in_play->damage_target_player))
      {

        instance = get_card_instance(player, card);
        if (event != EVENT_BLOCK_LEGALITY)
        {
          if ((event == EVENT_ACTIVATE) || (event == EVENT_CAN_ACTIVATE))
          {
            default_target_definition(player, card, &td, 2);
            td.illegal_abilities = 0;
            result =
                generic_activated_ability(player, card, event, 8, 0, 0, 0, 0, 0, 0, 0, &td, NULL);
            if (event == EVENT_CAN_ACTIVATE)
            {
              if (result != 0)
              {
                return (((land_can_be_played & 4U) == 0) ? 1U : 99U);
              }
              return 0;
            }
          }
          else
          {
            if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
            {
              return 0;
            }

            default_target_definition(player, card, &td, 2);
            td.illegal_abilities = 0;
            generic_activated_ability(player, card, event, 8, 0, 0, 0, 0, 0, 0, 0, &td, NULL);
          }
          if (event != EVENT_RESOLVE_ACTIVATION)
          {
            return 0;
          }
          result = valid_target(&td);
          if (result != 0)
          {

            parent = get_card_instance(instance->parent_controller, instance->parent_card);
            prevent_the_next_n_damage(instance->parent_controller, instance->parent_card,
                                      (int)parent->damage_target_player, parent->damage_target_card,
                                      0, PREVENT_INFINITE, 0, 0);
            return 0;
          }
          return 0;
        }
        if ((instance->damage_target_player == attacking_card_controller) &&
            (instance->damage_target_card == attacking_card))
        {
          result = is_what(affected_card_controller, affected_card, 2);
          if ((result != 0) &&
              (result = get_power(affected_card_controller, affected_card), result < 3))
          {
            return 0;
          }
          event_result = 1;
          return 0;
        }
      }
      return 0;
    }
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

/* ManalinkEh 2022: 0x22f8200; original cards/weatherlight.c:4558. */
int card_lava_storm(int player, int card, event_t event)
{
  /* Lava Storm	|3|R|R
   * Instant
   * ~ deals 2 damage to each attacking creature or ~ deals 2 damage to each blocking creature. */

  int choice;
  test_definition_t this_test;

  if (event != EVENT_RESOLVE_SPELL)
    return basic_spell(player, card, event);

  current_turn = (int)(player == 1);
  choice =
      do_dialog(player, player, card, -1, -1, " Damage attackers\n Damage blockers", current_turn);
  if (choice == 0)
  {
    default_test_definition(&this_test, 2);
    this_test.state = 4;
    new_damage_all(player, card, 2, 2, 0, &this_test);
  }
  else if (choice == 1)
  {
    default_test_definition(&this_test, 2);
    this_test.state = 8;
    new_damage_all(player, card, 2, 2, 0, &this_test);
  }
  kill_card(player, card, KILL_DESTROY);
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x22f8300; original cards/weatherlight.c:4584. */
int card_llanowar_druid(int player, int card, event_t event)
{
  /* Llanowar Druid	|1|G
   * Creature - Elf Druid 1/2
   * |T, Sacrifice this creature: Untap all Forests. */

  test_definition_t this_test;
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    default_test_definition(&this_test, 1);
    this_test.subtype = get_hacked_subtype(player, card, 0x5001);
    new_manipulate_all(player, card, 2, &this_test, ACT_UNTAP);
  }
  return generic_activated_ability(player, card, event, 9, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x22f8400; original cards/weatherlight.c:4603. */
int card_mana_chains(int player, int card, event_t event)
{
  /* Mana Chains	|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has "Cumulative upkeep |1." (At the beginning of its controller's upkeep, that player puts an age counter on it, then sacrifices it unless they pay its upkeep cost for each age counter on it.) */

  card_instance_t *instance;

  instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0 && (instance = in_play(player, card), instance != NULL) &&
      !is_humiliated(player, card, AL_TRIG))
  {
    instance = get_card_instance(player, card);
    upkeep_trigger_ability(player, card, event, (int)instance->damage_target_player);
    if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    {
      instance = get_card_instance(player, card);
      if (instance->damage_target_player == current_turn)
      {

        instance = get_card_instance(player, card);
        cumulative_upkeep((int)instance->damage_target_player, instance->damage_target_card,
                          EVENT_UPKEEP_TRIGGER_ABILITY, 1, 0, 0, 0, 0, 0);
      }
    }
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  return vanilla_aura(player, card, event, 1 - player);
}

/* ManalinkEh 2022: 0x22f85d0; original cards/weatherlight.c:4628. */
int card_manta_ray(int player, int card, event_t event)
{
  /* Manta Ray	|1|U|U
   * Creature - Fish 3/3
   * This creature can't attack unless defending player controls an Island.
   * This creature can't be blocked except by blue creatures.
   * When you control no Islands, sacrifice this creature. */

  unsigned int creature_colors;
  unsigned int required_color;

  if (((event == EVENT_BLOCK_LEGALITY) && (attacking_card_controller == player)) &&
      (attacking_card == card))
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {
      creature_colors = get_color(affected_card_controller, affected_card);
      required_color = get_sleighted_color_test(player, card, 4);
      if ((creature_colors & required_color) == 0)
      {
        event_result = 1;
      }
    }
  }
  landhome(player, card, event, SUBTYPE_ISLAND);
  return 0;
}

/* ManalinkEh 2022: 0x22eada0; original cards/weatherlight.c:4648. */
static char *is_blocking_master_of_arms(int who_chooses, int player, int card, int targeting_player,
                                        int targeting_card)
{
  int result;
  result = is_blocking(card, targeting_card);
  if (result == 0)
  {
    return "must be blocking Master of Arms";
  }
  return NULL;
}

/* ManalinkEh 2022: 0x22f8680; original cards/weatherlight.c:4655. */
int card_master_of_arms(int player, int card, event_t event)
{
  /* Master of Arms	|2|W
   * Creature - Human Soldier 2/2
   * First strike
   * |1|W: Tap target creature blocking this creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td2;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td2, 2);
    td2.allowed_controller = 1 - player;
    td2.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td2.extra = (int32_t)is_blocking_master_of_arms;
    td2.preferred_controller = td2.allowed_controller;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td2, 2);
    td2.allowed_controller = 1 - player;
    td2.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td2.extra = (int32_t)is_blocking_master_of_arms;
    td2.preferred_controller = td2.allowed_controller;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td2), result != 0))
    {
      tap_card(instance->targets[0].player, instance->targets[0].card);
    }
  }
  result = generic_activated_ability(player, card, event, 6, 1, 0, 0, 0, 0, 1, 0, &td2,
                                     "Select target creature blocking Master of Arms.");
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22f87f0; original cards/weatherlight.c:4685. */
int card_mischievous_poltergeist(int player, int card, event_t event)
{
  /* Mischievous Poltergeist	|2|B
   * Creature - Spirit 1/1
   * Flying
   * Pay 1 life: Regenerate this creature. */

  int result;
  card_instance_t *instance;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = can_regenerate(instance->parent_controller, instance->parent_card);
    if (result != 0)
    {
      regenerate_target(instance->parent_controller, instance->parent_card);
    }
  }
  result = generic_activated_ability(player, card, event, GAA_REGENERATION, 0, 0, 0, 0, 0, 0,
                                          1, NULL, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x22f88c0; original cards/weatherlight.c:4701. */
int card_mwonvuli_ooze(int player, int card, event_t event)
{
  /* Mwonvuli Ooze	|G
   * Creature - Ooze 1+* /1+*
   * Cumulative upkeep |2 (At the beginning of your upkeep, put an age counter on this permanent, then sacrifice it unless you pay |2 for each age counter on it.)
   * ~'s power and toughness are each equal to 1 plus twice the number of age counters on it. */

  int result;

  cumulative_upkeep(player, card, event, 2, 0, 0, 0, 0, 0);
  if (((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    if (card == -1)
    {
      set_characteristic_pt(1);
    }
    else
    {
      result = count_counters(player, card, COUNTER_AGE);
      set_characteristic_pt(result * 2 + 1);
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22f8980; original cards/weatherlight.c:4719. */
int card_natures_kiss(int player, int card, event_t event)
{
  /* Nature's Kiss	|1|G
   * Enchantment - Aura
   * Enchant creature
   * |1, Exile the top card of your graveyard: Enchanted creature gets +1/+1 until end of turn. */

  card_instance_t *instance;
  card_instance_t *aura;
  int pump_limit_or_result;
  int grave_size;
  card_instance_t *attached_in_play;

  aura = get_card_instance(player, card);
  if (event == EVENT_CHECK_ABILITIES)
  {
    if ((((aura->damage_target_card == affected_card) &&
          (affected_card_controller == aura->damage_target_player)) &&
         (aura->damage_target_player == player)) &&
        ((aura = in_play(player, card), aura != NULL && (pump_limit_or_result = card_natures_kiss(
                                                             player, card, EVENT_CAN_ACTIVATE),
                                                         pump_limit_or_result != 0))))
    {
      pump_limit_or_result = has_mana(player, 7, 1);
      grave_size = count_graveyard(player);
      if (grave_size < pump_limit_or_result)
      {
        pump_limit_or_result = grave_size;
      }
      check_abilities_power = check_abilities_power + pump_limit_or_result;
      check_abilities_toughness = check_abilities_toughness + pump_limit_or_result;
    }
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    pump_limit_or_result = count_graveyard(player);
    if (0 < pump_limit_or_result)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
    if (spell_fizzled != 1)
    {
      pump_limit_or_result = count_graveyard(player);
      rfg_card_from_grave(player, pump_limit_or_result + -1);
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    attached_in_play = in_play((int)instance->damage_target_player, instance->damage_target_card);
    if (attached_in_play != NULL)
    {
      pump_until_eot_merge_previous(player, card, (int)instance->damage_target_player,
                                    instance->damage_target_card, 1, 1);
    }
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  return vanilla_aura(player, card, event, player);
}
#endif

/* ManalinkEh 2022: 0x22f8c80; original cards/weatherlight.c:4763. */
int card_phantom_wings(int player, int card, event_t event)
{
  /* Phantom Wings	|1|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has flying.
   * Sacrifice this Aura: Return enchanted creature to its owner's hand. */

  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if (((instance->damage_target_player < '\0') ||
       (instance = in_play(player, card), instance == NULL)) ||
      (result = is_humiliated(player, card, AL_STATIC), result != 0))
  {
    result = generic_aura(player, card, event, player, 0, 0, 0x20, 0, 0, 0, 0);
  }
  else
  {
    if (event == EVENT_ACTIVATE)
    {
      get_card_instance(player, card);
    }
    else
    {
      if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
        return generic_aura(player, card, event, player, 0, 0, 0x20, 0, 0, 0, 0);

      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_ACTIVATION)
      {
        bounce_permanent((int)instance->damage_target_player, instance->damage_target_card);
      }
    }
    result = generic_activated_ability(player, card, event, 8, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  }
  return result;
}

/* ManalinkEh 2022: 0x22f8de0; original cards/weatherlight.c:4784. */
int card_roc_hatchling(int player, int card, event_t event)
{
  /* Roc Hatchling	|R
   * Creature - Bird 0/1
   * This creature enters with four shell counters on it.
   * At the beginning of your upkeep, remove a shell counter from this creature.
   * As long as this creature has no shell counters on it, it gets +3/+2 and has flying. */

  int result;

  enters_the_battlefield_with_counters(player, card, event, COUNTER_SHELL, 4);
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    remove_counters(player, card, player, card, COUNTER_SHELL, 1);
    result = count_counters(player, card, COUNTER_SHELL);
    if (0 < result)
    {
      return 0;
    }
  }
  else
  {
    result = count_counters(player, card, COUNTER_SHELL);
    if (0 < result)
    {
      return 0;
    }
    if (event == EVENT_POWER)
    {
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
      event_result = event_result + 3;
      return 0;
    }
    if (event == EVENT_TOUGHNESS)
    {
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
      event_result = event_result + 2;
      return 0;
    }
  }
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    event_result = event_result | 0x20;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22eadd0; original cards/weatherlight.c:4814. */
static int sawtooth_ogre_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (!end_of_combat_trigger(player, card, event, 2))
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  damage_creature((int)instance->damage_target_player, instance->damage_target_card, 1,
                  (int)instance->damage_source_player, instance->damage_source_card);
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x22e9d10; original cards/weatherlight.c:4825. */
static void damage_blocked_blocking_creature(int player, int card, int t_player, int t_card)
{
  create_targetted_legacy_effect(player, card, sawtooth_ogre_legacy, t_player, t_card);
  return;
}

/* ManalinkEh 2022: 0x22f8f20; original cards/weatherlight.c:4829. */
int card_sawtooth_ogre(int player, int card, event_t event)
{
  /* Sawtooth Ogre	|2|R|R
   * Creature - Ogre 3/3
   * Whenever this creature blocks or becomes blocked by a creature, this creature deals 1 damage to that creature at end of combat. */

  if (event == EVENT_DECLARE_BLOCKERS)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      if (current_turn == player)
      {
        for_each_creature_blocking_me(current_turn, card, damage_blocked_blocking_creature,
                                      current_turn, card);
      }
      if (1 - current_turn == player)
      {
        for_each_creature_blocked_by_me(player, card, damage_blocked_blocking_creature, player,
                                        card);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22f8fc0; original cards/weatherlight.c:4846. */
int card_soul_shepherd(int player, int card, event_t event)
{
  /* Soul Shepherd	|1|W
   * Creature - Human Cleric 2/1
   * |W, Exile a creature card from your graveyard: You gain 1 life. */

  int result;
  test_definition_t test;
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = can_use_activated_abilities(player, card);
    if ((result != 0) &&
        (result = has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 1), result != 0))
    {
      result = any_in_graveyard_by_type(player, 2);
      return (unsigned int)(result != 0);
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 1);
    if (spell_fizzled != 1)
    {
      new_default_test_definition(&test, 2, "Select a creature card to exile.");
      result = new_select_a_card(player, player, 2, 0, 3, -1, &test);
      if (result != -1)
      {
        rfg_card_from_grave(player, result);
        return 0;
      }
      spell_fizzled = 1;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    gain_life(player, 1);
  }
  return 0;
}

/* ManalinkEh 2022: 0x22f9160; original cards/weatherlight.c:4879. */
int card_tendrils_of_despair(int player, int card, event_t event)
{
  /* Tendrils of Despair	|B
   * Sorcery
   * As an additional cost to cast this spell, sacrifice a creature.
   * Target opponent discards two cards. */

  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      new_multidiscard(1 - player, 2, 0, player);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x440, &td, NULL, 1, NULL);
}

/* ManalinkEh 2022: 0x22f92b0; original cards/weatherlight.c:4904. */
int card_thran_forge(int player, int card, event_t event)
{
  /* Thran Forge	|3
   * Artifact
   * |2: Until end of turn, target nonartifact creature gets +1/+0 and becomes an artifact in addition to its other types. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CHECK_PUMP)
  {
    result = card_thran_forge(player, card, EVENT_CAN_ACTIVATE);
    if (result != 0)
    {
      pumpable_power[player] = pumpable_power[player] + 1;
    }
  }
  else if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
            (event == EVENT_RESOLVE_ACTIVATION)) ||
           (event == EVENT_CLEANUP))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.illegal_type = TYPE_ARTIFACT;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                      instance->targets[0].player, instance->targets[0].card, 1, 0);
        turn_into_artifact(instance->parent_controller, instance->parent_card,
                           instance->targets[0].player, instance->targets[0].card, 1);
      }
    }
    result = generic_activated_ability(
        player, card, event, 6, 2, 0, 0, 0, 0, 0, 0, &td,
        "Select target nonartifact creature gets +1/+0 & becomes artifact");
    return result;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22f9450; original cards/weatherlight.c:4937. */
int card_timid_drake(int player, int card, event_t event)
{
  /* Timid Drake	|2|U
   * Creature - Drake 3/3
   * Flying
   * When another creature enters, return this creature to its owner's hand. */

  int result;
  test_definition_t test;
  if (trigger_condition != TRIGGER_COMES_INTO_PLAY)
  {
    return 0;
  }
  if (((card == affected_card) && (player == affected_card_controller)) &&
      ((trigger_cause_controller != player || (trigger_cause != card))))
  {
    default_test_definition(&test, 2);
    test.not_me = 1;
    result = new_specific_cip_global(2, event, 2, TYPE_NONE, &test, player, card);
    if (result != 0)
    {
      bounce_permanent(player, card);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22ea7e0; original cards/weatherlight.c:4957. */
static int well_of_knowledge_legacy(int player, int card, event_t event)
{
  int granting_card;
  card_instance_t *instance;
  int result;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      result = instance->targets[0].player;
      if (-1 < result)
      {
        result = get_id(result, instance->targets[0].card);
        if (result != 0xbfc)
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
      result = instance->targets[0].player;
      granting_card = instance->targets[0].card;
      if (event == EVENT_CAN_ACTIVATE)
      {
        if ((current_phase == 10) && (result = granted_generic_activated_ability(
                                          result, granting_card, player, card, EVENT_CAN_ACTIVATE,
                                          0x10000, 2, 0, 0, 0, 0, 0, 0, NULL, NULL),
                                      result != 0))
        {
          return 1;
        }
      }
      else
      {
        if (event == EVENT_ACTIVATE)
        {
          result =
              granted_generic_activated_ability(result, granting_card, player, card, EVENT_ACTIVATE,
                                                0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
          return result;
        }
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          draw_a_card(player);
          return 0;
        }
      }
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x22f9510; original cards/weatherlight.c:4997. */
int card_well_of_knowledge(int player, int card, event_t event)
{
  /* Well of Knowledge	|3
   * Artifact
   * |2: Draw a card. Any player may activate this ability but only during their draw step. */

  int p;
  bool legacy_found;
  card_instance_t *instance;
  card_instance_t *scanned_instance;
  int activation_available;
  int card_00;

  instance = get_card_instance(player, card);
  if (event == EVENT_STATIC_EFFECTS)
  {
    scanned_instance = in_play(player, card);
    if (((scanned_instance != NULL) && (card == affected_card)) &&
        (player == affected_card_controller))
    {
      legacy_found = false;
      p = 0;
      do
      {

        card_00 = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            scanned_instance = in_play(p, card_00);

            if (((scanned_instance != NULL) && (is_what(p, card_00, 0x80) != 0)) &&
                ((scanned_instance = get_card_instance(p, card_00),
                  scanned_instance->info_slot_fn == well_of_knowledge_legacy &&
                      ((scanned_instance->targets[0].player == player &&
                        (scanned_instance->targets[0].card == card))))))
            {
              if (player == p)
              {
                kill_card(player, card_00, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                {
                  return 0;
                }
                legacy_found = true;
              }
            }
            card_00 = card_00 + 1;
          } while (card_00 <= active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
      if ((!legacy_found) && (instance->targets[9].card != 0x42))
      {
        create_legacy_activate_for_opponent(player, card, well_of_knowledge_legacy, player, card);
      }
    }
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    activation_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x10000, 2,
                                                     0, 0, 0, 0, 0, 0, NULL, NULL);
    if ((activation_available != 0) && (current_phase == 10))
    {
      return 1;
    }
  }
  else
  {
    if (event == EVENT_ACTIVATE)
    {
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 2, 0, 0, 0, 0, 0, 0, NULL,
                                       NULL);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      draw_a_card(player);
      return 0;
    }
  }
  return 0;
}
