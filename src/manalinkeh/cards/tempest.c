#include "manalink.h"

static int sadistic_glee_effect(int player, int card, event_t event);

// ------------ General functions

int card_magus_of_the_scroll(int player, int card, event_t event);

char *with_shadow(int who_chooses, int player, int card);
static int effect_maddening_imp(int player, int card, event_t event);

void bellowing_fiend_effect(int player, int card, int t_player, int t_card);
int damage_controller_and_you(int player, int card, event_t event);

#ifndef SHANDALAR

static char *is_already_damaged(int who_chooses, int player, int card, int unused_source_player, int unused_source_card);

#endif

int tempest_painland(int player, int card, event_t event)
{

  if (!is_humiliated(player, card, AL_TRIG))
    comes_into_play_tapped(player, card, event);

  return generic_painland(player, card, event);
}

/* ManalinkEh 2022: 0x2148db0; original cards/tempest.c:13. */
int tempest_does_not_untap_land(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;
  unsigned int original_color_bit;
  unsigned int replacement_color_bit;
  color_test_t available_colors;
  unsigned int color_mask_or_index;
  unsigned int color_index;
  int mana_quantity;
  color_test_t overridden_colors;

  if (event == EVENT_CAN_ACTIVATE)
  {
    if ((((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
         (life[player] < 2)) &&
        (instance = get_card_instance(player, card), (instance->mana_color & 1) == 0))
    {
      return 0;
    }
    return mana_producer(player, card, event);
  }
  if (event == EVENT_ACTIVATE)
  {

    instance = get_card_instance(player, card);
    available_colors = (unsigned int)instance->mana_color | mana_colors_added[player];
    if ((((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
         (life[player] < 2)) &&
        ((available_colors & COLOR_TEST_COLORLESS) != COLOR_TEST_0))
    {
      produce_mana_tapped_all_one_color(player, card,
                                        mana_colors_added[player] | COLOR_TEST_COLORLESS, 1);
    }
    else
    {
      produce_mana_tapped_all_one_color(player, card, available_colors, 1);
      result = check_special_flags2(player, card, 0x40);
      if (result == 0)
      {
        if ((chosen_colors &
             ((mana_colors_added[player] | COLOR_TEST_COLORLESS) ^ ~COLOR_TEST_0)) != COLOR_TEST_0)
        {
          does_not_untap_effect(player, card, player, card, 8, 1);
        }
      }
      else
      {

        overridden_colors = COLOR_TEST_0;
        color_mask_or_index = 0;
        while (color_index = color_mask_or_index, color_mask_or_index = color_index + 1,
               color_mask_or_index != 7)
        {
          if (((mana_colors_added[player] | 1U) & color_mask_or_index) != 0)
          {
            available_colors = color_to_color_test(mana_color_override[player][color_index + 1]);
            overridden_colors = overridden_colors | available_colors;
            if ((chosen_colors & (overridden_colors ^ ~COLOR_TEST_0)) != COLOR_TEST_0)
            {
              does_not_untap_effect(player, card, player, card, 8, 1);
            }
          }
        }
      }
    }
  }
  else
  {
    if (((event != EVENT_COUNT_MANA) || (card != affected_card)) ||
        (player != affected_card_controller))
      return mana_producer(player, card, event);
    result = is_tapped(player, card);
    if (((result == 0) && (result = is_animated_and_sick(player, card), result == 0)) &&
        (result = can_produce_mana(player, card), result != 0))
    {
      result = check_special_flags2(player, card, 0x40);

      if (result == 0)
      {

        instance = get_card_instance(player, card);
        color_mask_or_index = mana_colors_added[player] | (unsigned int)instance->mana_color;
        if (((ai_is_speculating == 1) ||
             ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
            (life[player] < 2))
        {
          if ((color_mask_or_index & 1) != 0)
          {
            declare_mana_available(player, 0, 1);
          }
        }
        else
        {
          declare_mana_available_hex(player, color_mask_or_index, 1);
        }
      }
      else
      {
        instance = get_card_instance(player, card);
        color_mask_or_index = (unsigned int)instance->mana_color | mana_colors_added[player];
        if (((ai_is_speculating == 1) ||
             ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
            (life[player] < 2))
        {
          if ((color_mask_or_index & 1) != 0)
          {
            result = mana_quantity_override[player][0];
            if (result < 0)
            {
              result = 1;
            }
            declare_mana_available(player, mana_color_override[player][0], result);
          }
        }
        else
        {
          color_index = 0;
          mana_quantity = 1;
          do
          {
            original_color_bit = color_to_color_test(color_index);
            if (((original_color_bit & color_mask_or_index) != 0) &&
                (mana_color_override[player][color_index] != 0))
            {
              original_color_bit = color_to_color_test(color_index);
              replacement_color_bit = color_to_color_test(mana_color_override[player][color_index]);
              color_mask_or_index =
                  color_mask_or_index & (original_color_bit ^ 0xffffffff) | replacement_color_bit;
              if (-1 < mana_quantity_override[player][color_index])
              {
                mana_quantity = mana_quantity_override[player][color_index];
              }
            }

            color_index = color_index + 1;
          } while (color_index < 7);
          declare_mana_available_hex(player, color_mask_or_index, mana_quantity);
        }
      }
    }
  }
  return 0;
}

static int tempest_medallion(int player, int card, event_t event, int clr){
	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card, AL_STATIC) ){
		if( affected_card_controller == player && (get_color(affected_card_controller, affected_card) & clr) ){
			COST_COLORLESS--;
		}
	}

   return 0;
}

int buyback(int player, int card, int colorless, int black, int blue, int green, int red, int white){
	if( ! is_token(player, card) ){
		int diff = 0;
		if( ! played_for_free(player, card) ){
			diff = MIN(0, cards_ptr[get_id(player, card)]->req.req_colorless+true_get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, -1));
		}
		diff-=(2*count_cards_by_id(player, CARD_ID_MEMORY_CRYSTAL));
		int cless = MAX(0, colorless+diff);
		if( has_mana_multi(player, cless, black, blue, green, red, white) ){
			int choice = do_dialog(player, player, card, -1, -1, " Buyback\n Play normally", 0);
			if( choice == 0 ){
				charge_mana_multi(player, cless, black, blue, green, red, white);
				if( spell_fizzled != 1 ){
					return 1;
				}
			}
		}
	}
	return 0;
}

int generic_licid(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white, int pow, int tou, int key, int s_key,
					int pref_controller
  ){
	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CHANGE_TYPE && affect_me(player, card) && instance->dummy3 > 0 ){
		event_result = instance->dummy3;
	}

	if( ! is_what(player, card, TYPE_CREATURE) && instance->damage_target_player > -1 ){
		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return generic_activated_ability(player, card, event, 0, 0, black, blue, green, red, white, 0, 0, 0);
		}
		if( event == EVENT_RESOLVE_ACTIVATION ){
			card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
			parent->dummy3 = parent->damage_target_player = parent->damage_target_card = -1;
			parent->regen_status |= KEYWORD_RECALC_ALL ;
			get_abilities(instance->parent_controller, instance->parent_card, EVENT_CHANGE_TYPE, -1);
		}
		return generic_aura(player, card, event, player, pow, tou, key, s_key, 0, 0, 0);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = pref_controller;

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK+GAA_CAN_TARGET, colorless, black, blue, green, red, white, 0,
										&td, "TARGET_CREATURE");
	}

	if( event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK+GAA_CAN_TARGET, colorless, black, blue, green, red, white, 0,
										&td, "TARGET_CREATURE");
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
			int newtype = create_a_card_type(parent->internal_card_id);
			cards_at_7c7000[newtype]->type = TYPE_ENCHANTMENT;
			cards_at_7c7000[newtype]->power = 0;
			cards_at_7c7000[newtype]->toughness = 0;
			parent->dummy3 = newtype;
			attach_aura_to_target(instance->parent_controller, instance->parent_card, event, instance->targets[0].player, instance->targets[0].card);
			parent->regen_status |= KEYWORD_RECALC_ALL ;
			get_abilities(instance->parent_controller, instance->parent_card, EVENT_CHANGE_TYPE, -1);
		}
	}

	return 0;
}

const char* activating_sliver(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	if(	can_use_activated_abilities(player, card) ){
		return NULL;
	}
	return "this Sliver cannot use activated abilities.";
}

int sliver_shared_shade_ability(int player, int card, event_t event)
{
  int p;
  card_instance_t *effect;
  card_instance_t *instance;
  unsigned int keywords;
  int source_card_or_result;
  int scan_card_or_result;
  unsigned int special_keywords;
  unsigned int power_bonus;
  unsigned int toughness_bonus;
  target_definition_t *sliver_target;
  target_definition_t td;
  char buffer[100];

  effect = get_card_instance(player, card);
  p = effect->targets[1].player;
  if (p < 0)
  {
    return 0;
  }
  source_card_or_result = effect->targets[1].card;
  leaves_play(p, source_card_or_result, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    add_status(player, card, 0x10000);
    in_play(p, source_card_or_result);
    is_humiliated(p, source_card_or_result, AL_NM_NL_ACT);
    return 0;
  }
  instance = in_play(p, source_card_or_result);
  if (!instance && event == EVENT_CAN_ACTIVATE)
    return 0;
  if (!instance && event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);
  if (is_humiliated(p, source_card_or_result, AL_NM_NL_ACT))
    return 0;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.required_subtype = 0xb1;
  td.extra = target_predicate_extra(activating_sliver);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.illegal_abilities = 0;
  if (event == EVENT_CAN_ACTIVATE)
  {
    scan_card_or_result = 0;
    if (active_cards_count[player] < 1)
    {
      return 0;
    }
    while (
        (((effect = in_play(player, scan_card_or_result),
           effect == NULL || (source_card_or_result = is_what(player, scan_card_or_result, 0x47),
                              source_card_or_result == 0)) ||
          (source_card_or_result = has_subtype(player, scan_card_or_result, SUBTYPE_SLIVER),
           source_card_or_result == 0)) ||
         (source_card_or_result = generic_activated_ability(
              player, scan_card_or_result, EVENT_CAN_ACTIVATE, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL),
          source_card_or_result == 0)))
    {
      scan_card_or_result = scan_card_or_result + 1;
      if (active_cards_count[player] <= scan_card_or_result)
      {
        return 0;
      }
    }
    return 1;
  }
  sliver_target = &td;
  if (event == EVENT_ACTIVATE)
  {
    scan_card_or_result = get_id(p, source_card_or_result);
    scnprintf(buffer, 100, "%s: select a Sliver to activate.",
              cards_ptr[scan_card_or_result]->name);
    if (new_pick_target(sliver_target, buffer, 0, 0x81) != 0)
    {
      scan_card_or_result = effect->targets[0].card;
      effect->number_of_targets = '\x01';
      if (can_use_activated_abilities(effect->targets[0].player, scan_card_or_result) == 0)
      {
        spell_fizzled = 1;
      }
      else
      {
        charge_mana_for_activated_ability(effect->targets[0].player, effect->targets[0].card, 2, 0,
                                          0, 0, 0, 0);
      }
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    power_bonus = effect->targets[2].player;
    toughness_bonus = effect->targets[2].card;
    power_bonus = power_bonus & (int)(power_bonus ^ 0xffffffff) >> 0x1f;
    toughness_bonus = toughness_bonus & (int)(toughness_bonus ^ 0xffffffff) >> 0x1f;
    keywords = effect->targets[3].player;
    keywords = keywords & (int)(keywords ^ 0xffffffff) >> 0x1f;
    special_keywords = effect->targets[3].card;
    special_keywords = special_keywords & (int)(special_keywords ^ 0xffffffff) >> 0x1f;
    scan_card_or_result = effect->targets[0].card;
    source_card_or_result = effect->targets[0].player;
    if (keywords == 0 && special_keywords == 0)
    {
      pump_until_eot(source_card_or_result, scan_card_or_result, source_card_or_result,
                     scan_card_or_result, power_bonus, toughness_bonus);
    }
    else
    {
      pump_ability_until_eot(source_card_or_result, scan_card_or_result, source_card_or_result,
                             scan_card_or_result, power_bonus, toughness_bonus, keywords,
                             special_keywords);
    }
  }
  return 0;
}

int sliver_with_shared_shade_ability(int player, int card, event_t event, int pow, int tou, int key, int s_key){

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &sliver_shared_shade_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->targets[2].player = pow;
		instance->targets[2].card = tou;
		instance->targets[3].player = key;
		instance->targets[3].card = s_key;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &sliver_shared_shade_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->targets[2].player = pow;
		instance->targets[2].card = tou;
		instance->targets[3].player = key;
		instance->targets[3].card = s_key;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int shared_sliver_activated_ability(int player, int card, event_t event, int mode, int cless, int black, int blue, int green, int red, int white,
									uint32_t variable_costs, target_definition_t *td2, const char *prompt){

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_SLIVER;
	td.extra = target_predicate_extra(activating_sliver);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.illegal_state = (mode & GAA_UNTAPPED) ? TARGET_STATE_TAPPED | TARGET_STATE_SUMMONSICK_NOTAP : 0;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && in_play(player, card) ){
		int cc;
		for(cc=0; cc < active_cards_count[player]; cc++){
			if( in_play(player, cc) && is_what(player, cc, TYPE_PERMANENT) && has_subtype(player, cc, SUBTYPE_SLIVER) ){
				int result = generic_activated_ability(player, cc, EVENT_CAN_ACTIVATE, mode, cless, black, blue, green, red, white, variable_costs, td2, prompt);
				if( result ){
					return result;
				}
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( new_pick_target(&td, "Select a Sliver to activate", 0, 1 | GS_LITERAL_PROMPT) ){
			instance->number_of_targets = 1;
			if( can_use_activated_abilities(instance->targets[0].player, instance->targets[0].card) ){
				generic_activated_ability(instance->targets[0].player, instance->targets[0].card, event,
											mode, cless, black, blue, green, red, white, variable_costs, td2, prompt);
				if( spell_fizzled != 1 ){
					instance->targets[2] = get_card_instance(instance->targets[0].player, instance->targets[0].card)->targets[0];
				}
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	return 0;
}

//--------------- Cards

int card_abandon_hope(int player, int card, event_t event){

	/* Abandon Hope	|X|1|B
	 * Sorcery
	 * As an additional cost to cast ~, discard X cards.
	 * Look at target opponent's hand and choose X cards from it. That player discards those cards. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		set_special_flags2(player, card, SF2_X_SPELL);
		if( (played_for_free(player, card) || is_token(player, card)) && ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			return 0;
		}
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int original_hand[2][hand_count[player]];
			int ohc = 0;
			int i;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_hand(player, i) ){
					original_hand[0][ohc] = get_original_internal_card_id(player, i);
					original_hand[1][ohc] = i;
					ohc++;
				}
			}
			int to_discard[hand_count[player]];
			int tdc = 0;
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to discard.");
			while( ohc && has_mana(player, COLOR_COLORLESS, tdc+1) ){
					int selected = select_card_from_zone(player, player, original_hand[0], ohc, 0, AI_MIN_VALUE, -1, &this_test);
					if( selected != -1 ){
						to_discard[tdc] = original_hand[1][selected];
						tdc++;
						for(i=selected; i<ohc; i++){
							original_hand[0][i] = original_hand[0][i+1];
							original_hand[1][i] = original_hand[1][i+1];
						}
						ohc--;
					}
					else{
						break;
					}
			}
			if( tdc ){
				charge_mana(player, COLOR_COLORLESS, tdc);
				if( spell_fizzled != 1 ){
					for(i=0; i<tdc; i++){
						discard_card(player, to_discard[i]);
					}
					instance->info_slot = tdc;
				}
			}
		}
		instance->targets[0].player = 1-player;
		instance->targets[0].card = -1;
		instance->number_of_targets = 1;
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			this_definition.qty = instance->info_slot;

			new_effect_coercion(&this_definition, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_advance_scout(int player, int card, event_t event){
/* CARD_ID_ADVANCE_SCOUT	3073
Advance Scout	|1|W
Creature - Human Soldier Scout 1/1
First strike
|W: Target creature gains first strike until end of turn. */

	if (event == EVENT_CHECK_ABILITIES){
		if (affected_card_controller == player && card_advance_scout(player, card, EVENT_CAN_ACTIVATE)){
			check_abilities_keywords |= KEYWORD_FIRST_STRIKE;
		}
		return 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
									0, 0, KEYWORD_FIRST_STRIKE, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_W(1), 0, &td, "TARGET_CREATURE");
}

int card_aftershock(int player, int card, event_t event){
/* CARD_ID_AFTERSHOCK	3074
Aftershock	|2|R|R
Sorcery
Destroy target artifact, creature, or land. ~ deals 3 damage to you. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			damage_player(player, 3, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target artifact, creature or land.", 1, NULL);
}

int card_altar_of_dementia(int player, int card, event_t event){

	/* Altar of Dementia	|2
	 * Artifact
	 * Sacrifice a creature: Target player puts a number of cards equal to the sacrificed creature's power from the top of his or her library into his or her
	 * graveyard. */

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE, MANACOST0, 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_PLAYER") ){
			instance->info_slot = get_power(BYTE2(sac), BYTE3(sac));
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
		else{
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			mill(instance->targets[0].player, instance->info_slot);
		}
	}

	return 0;
}

static int aluren_effect(int player, int card, event_t event)
{

  if (is_what(player, card, TYPE_EFFECT) && get_card_instance(player, card)->targets[0].player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (is_humiliated(instance->targets[0].player, instance->targets[0].card, AL_STATIC))
    {
      return 0;
    }
    other_leaves_play(player, card, instance->targets[0].player, instance->targets[0].card, event,
                      0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      kill_card(player, card, KILL_EXILE);
      return 0;
    }
  }

  if (event == EVENT_CAN_ACTIVATE)
  {
    int i;
    for (i = 0; i < active_cards_count[player]; i++)
    {
      if (in_hand(player, i) && is_what(player, i, TYPE_CREATURE) && get_cmc(player, i) <= 3)
      {
        return 1;
      }
    }
  }

  if (event == EVENT_ACTIVATE)
  {
    test_definition_t this_test;
    new_default_test_definition(&this_test, TYPE_CREATURE,
                                "Select a creature card with CMC <= 3 to play.");
    this_test.cmc = 4;
    this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
    int selected =
        new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_VALUE, -1, &this_test);
    if (selected != -1)
    {
      play_card_in_hand_for_free(player, selected);
      cant_be_responded_to =
          1; // The spell will be respondable to, but this (fake) activation won't
    }
    else
    {
      spell_fizzled = 1;
    }
  }
  return 0;
}

int card_aluren(int player, int card, event_t event){
  /* Aluren |2|G|G
   * 	  Enchantment
   * 	  Any player may play creature cards with converted mana cost 3 or less without paying their mana cost and as though they had flash. */

	if( event == EVENT_RESOLVE_SPELL ){
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		int legacy = create_legacy_activate(1-player, fake, &aluren_effect);
		card_instance_t *instance = get_card_instance(1-player, legacy);
		instance->targets[0].player = player;
		instance->targets[0].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	if( (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE) && ! is_humiliated(player, card, AL_STATIC) ){
		return aluren_effect(player, card, event);
	}

	return global_enchantment(player, card, event);
}

int card_ancient_runes(int player, int card, event_t event){
/* CARD_ID_ANCIENT_RUNES	3077
Ancient Runes	|2|R
Enchantment
At the beginning of each player's upkeep, ~ deals damage to that player equal to the number of artifacts he or she controls. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		damage_player(current_turn, count_subtype(current_turn, TYPE_ARTIFACT, -1), player, card);
	}

	return global_enchantment(player, card, event);
}

int card_ancient_tomb(int player, int card, event_t event)
{
  /* CARD_ID_ANCIENT_TOMB	3078
Ancient Tomb	""
Land
|T: Add |C|C to your mana pool. ~ deals 2 damage to you. */

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    card_instance_t* instance = get_card_instance(player, card);
    if (!(mana_colors_added[player] & color_to_color_test(tapped_for_mana_color)))
      damage_player(player, 2, instance->parent_controller, instance->parent_card);
    return 0;
  }
  return two_mana_land(player, card, event, COLOR_COLORLESS, COLOR_COLORLESS);
}

int card_anoint(int player, int card, event_t event){
/* CARD_ID_ANOINT	3080
Anoint	|W
Instant
Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
Prevent the next 3 damage that would be dealt to target creature this turn. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_CREATURE;
	td.required_type = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, "TARGET_DAMAGE", 1, NULL);
		if( spell_fizzled != 1 ){
			instance->info_slot = buyback(player, card, MANACOST_X(3));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 3);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_apocalypse(int player, int card, event_t event){
/* CARD_ID_APOCALYPSE	3082
Apocalypse	|2|R|R|R
Sorcery
Exile all permanents. You discard your hand. */

	if( event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		new_manipulate_all(player, card, ANYBODY, &this_test, KILL_EXILE);
		discard_all(player);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_armord_sliver(int player, int card, event_t event){
  /* Armor Sliver |2|W
   * 	  Creature - Sliver 2/2
   * 	  All Sliver creatures have "{2}: This creature gets +0/+1 until end of turn." */

	return sliver_with_shared_shade_ability(player, card, event, 0, 1, 0, 0);
}

int card_auratog(int player, int card, event_t event){
  /* Auratog English |1|W
   * 	  Creature - Atog 1/2
   * 	  Sacrifice an enchantment: Auratog gets +2/+2 until end of turn. */

	return generic_husk(player, card, event, TYPE_ENCHANTMENT, 2, 2, 0, 0);
}

int card_barbed_sliver(int player, int card, event_t event){
  /* Barbed Sliver |2|R
   * 	  Creature - Sliver 2/2
   * 	  All Sliver creatures have "{2}: This creature gets +1/+0 until end of turn." */

	return sliver_with_shared_shade_ability(player, card, event, 1, 0, 0, 0);
}

int card_blood_frenzy(int player, int card, event_t event){
/* CARD_ID_BLOOD_FRENZY	3090
Blood Frenzy	|1|R
Instant
Cast ~ only before the combat damage step.
Target attacking or blocking creature gets +4/+0 until end of turn. Destroy that creature at the beginning of the next end step. */

	if (event == EVENT_CHECK_PUMP ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.required_state = TARGET_STATE_IN_COMBAT;
		if( current_phase == PHASE_AFTER_BLOCKING && generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return vanilla_instant_pump(player, card, event, ANYBODY, player, 4, 0, 0, 0);
		}
	}

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_IN_COMBAT;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && current_phase == PHASE_AFTER_BLOCKING ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target attacking or blocking creature", 1, NULL);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_ability_t pump;
			default_pump_ability_definition(player, card, &pump, 4, 0, 0, 0);
			pump.paue_flags = PAUE_END_AT_EOT | PAUE_REMOVE_TARGET_AT_EOT;
			pump.eot_removal_method = KILL_DESTROY;
			pump_ability(player, card, instance->targets[0].player, instance->targets[0].card, &pump);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

/* ManalinkEh 2022: 0x214b120; original cards/tempest.c:762. */
int card_booby_trap(int player, int card, event_t event)
{
  /* Booby Trap	|6
   * Artifact
   * As this artifact enters, choose an opponent and a card name other than a basic land card name.
   * The chosen player reveals each card they draw.
   * When the chosen player draws a card with the chosen name, sacrifice this artifact. If you do, this artifact deals 10 damage to that player. */

  if (event == EVENT_RESOLVE_SPELL)
  {

    card_instance_t *instance = get_card_instance(player, card);
    test_definition_t test;
    int csvid;
    new_default_test_definition(&test, 0xff, "Select a card other than a basic land.");
    test.subtype = 0x2000;
    test.subtype_flag = 1;
    if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
    {
      csvid = ai_name_a_random_csvid(player, card, 1 - player, TYPE_NONE, &test);
    }
    else
    {
      csvid = new_card_from_list(player, 3, &test);
    }
    if (csvid >= 0)
    {
      instance->info_slot = csvid;
      create_card_name_legacy(player, card, csvid);
    }
  }
  if ((((trigger_condition == TRIGGER_CARD_DRAWN) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (((reason_for_trigger_controller != player && (trigger_cause_controller != player)) &&
        (suppress_draw == 0))))
  {
    if (is_humiliated(player, card, AL_TRIG) == 0)
    {

      card_instance_t *instance = get_card_instance(player, card);
      if (event == EVENT_TRIGGER)
      {
        event_result = event_result | 2;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {

        int iid = get_original_internal_card_id(trigger_cause_controller, trigger_cause);
        int trap = get_internal_card_id_from_csv_id(instance->info_slot);
        reveal_card_iid(player, card, iid);
        if (iid == trap)
        {
          damage_player(1 - player, 10, player, card);
          kill_card(player, card, KILL_SACRIFICE);
        }
      }
    }
  }
  return 0;
}

int card_bottle_gnomes(int player, int card, event_t event){
/* CARD_ID_BOTTLE_GNOMES	1705
Bottle Gnomes	|3
Artifact Creature - Gnome 1/3
Sacrifice ~: You gain 3 life. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 3);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_broken_fall(int player, int card, event_t event){
/* CARD_ID_BROKEN_FALL	3094
Broken Fall	|2|G
Enchantment
Return ~ to its owner's hand: Regenerate target creature. */

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

	if( event == EVENT_CAN_ACTIVATE && ( land_can_be_played & LCBP_REGENERATION ) ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_BOUNCE_ME, MANACOST0, 0, &td, NULL) ){
			return 99;
		}
	}

	if( event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_BOUNCE_ME | GAA_LITERAL_PROMPT, MANACOST0, 0,
										&td, "Select target creature to regenerate.");
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target( instance->targets[0].player, instance->targets[0].card );
		}
	}

	return 0;
}

int card_capsize(int player, int card, event_t event){
  /* Capsize	|1|U|U
   * Instant
   * Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Return target permanent to its owner's hand. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
		if( spell_fizzled != 1 ){
			instance->info_slot = buyback(player, card, MANACOST_X(3));
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_carrionette(int player, int card, event_t event){
	/* Carrionette	|1|B
	 * Creature - Skeleton 1/1
	 * |2|B|B: Exile ~ and target creature unless that creature's controller pays |2. Activate this ability only if ~ is in your graveyard. */

	if( event == EVENT_GRAVEYARD_ABILITY && has_mana_multi(player, MANACOST_XB(2, 2)) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );

		if( can_target(&td) ){
			return GA_BASIC;
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana_multi(player, MANACOST_XB(2, 2));
		if( spell_fizzled != 1){
			return GAPAID_EXILE;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATED_GRAVEYARD_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.allow_cancel = 0;

		card_instance_t *instance= get_card_instance(player, card);
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			charge_mana(instance->targets[0].player, COLOR_COLORLESS, 2);
			if( spell_fizzled == 1 ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			}
		}
	}

	return 0;
}

int card_choke(int player, int card, event_t event){
  /* Choke |2|G
   * 	  Enchantment
   * 	  Islands don't untap during their controllers' untap steps. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier+=5*(count_subtype(1-player, TYPE_LAND, SUBTYPE_ISLAND)-count_subtype(player, TYPE_LAND, SUBTYPE_ISLAND));
	}

	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP ){
		if( has_subtype(affected_card_controller, affected_card, get_hacked_subtype(player, card, SUBTYPE_ISLAND)) ){
			does_not_untap(affected_card_controller, affected_card, event);
		}
	}

	return 0;
}

const char* sliver_to_regenerate(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	if(	can_use_activated_abilities(player, card) ){
		if( who_chooses == HUMAN || (who_chooses == AI && can_regenerate(player, card)) ){
			return NULL;
		}
	}
	return "this Sliver cannot regenerate.";
}

int clot_sliver_shared_ability(int player, int card, event_t event)
{
  int p;
  card_instance_t *instance;
  int result_or_source_card;
  int scan_player;
  int c;
  target_definition_t td;

  instance = get_card_instance(player, card);
  p = instance->targets[1].player;
  if (p < 0)
  {
    return 0;
  }
  c = instance->targets[1].card;
  leaves_play(p, c, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    kill_card(player, card, KILL_EXILE);
    is_humiliated(p, c, AL_NM_NL_ACT);
  }
  else
  {
    result_or_source_card = is_humiliated(p, c, AL_NM_NL_ACT);
    if (result_or_source_card == 0)
    {
      if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
      {
        default_target_definition(player, card, &td, 2);
        td.preferred_controller = player;
        td.required_state = TARGET_STATE_DESTROYED;
        td.required_subtype = 0xb1;
        td.extra = target_predicate_extra(sliver_to_regenerate);
        td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
        td.illegal_abilities = 0;
        if (event == EVENT_CAN_ACTIVATE)
        {
          instance = in_play(p, c);
          if (instance == NULL)
          {
            return 0;
          }
          if ((((unsigned int)land_can_be_played >> 8 & 0xff) & 2) == 0)
          {
            return 0;
          }
          scan_player = 0;
          do
          {
            c = 0;
            if (0 < active_cards_count[scan_player])
            {
              do
              {
                instance = in_play(scan_player, c);
                if ((((instance != NULL) && (result_or_source_card = is_what(scan_player, c, 0x47),
                                             result_or_source_card != 0)) &&
                     (result_or_source_card = has_subtype(scan_player, c, SUBTYPE_SLIVER),
                      result_or_source_card != 0)) &&
                    (result_or_source_card =
                         regeneration(scan_player, c, EVENT_CAN_ACTIVATE, 2, 0, 0, 0, 0, 0),
                     result_or_source_card != 0))
                {
                  return result_or_source_card;
                }
                c = c + 1;
              } while (c < active_cards_count[scan_player]);
            }
            scan_player = scan_player + 1;
            if (scan_player == 2)
            {
              return 0;
            }
          } while (true);
        }
      }
      else
      {
        if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        {
          return 0;
        }
        default_target_definition(player, card, &td, 2);
      }
      td.preferred_controller = player;
      td.required_state = TARGET_STATE_DESTROYED;
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      td.required_subtype = 0xb1;
      td.extra = (intptr_t)sliver_to_regenerate;
      td.illegal_abilities = 0;
      if (event != EVENT_ACTIVATE)
      {
        if (event != EVENT_RESOLVE_ACTIVATION)
        {
          return 0;
        }
        if (valid_target(&td) == 0)
        {
          return 0;
        }
        if (can_regenerate(instance->targets[0].player, instance->targets[0].card) == 0)
        {
          return 0;
        }
        regenerate_target(instance->targets[0].player, instance->targets[0].card);
        return 0;
      }
      if (new_pick_target(&td, "Select a Sliver to regenerate.", 0, 0x81) == 0)
      {
        return 0;
      }
      scan_player = instance->targets[0].card;
      instance->number_of_targets = '\x01';
      if (can_use_activated_abilities(instance->targets[0].player, scan_player) == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      charge_mana_for_activated_ability(instance->targets[0].player, instance->targets[0].card, 2,
                                        0, 0, 0, 0, 0);
      return 0;
    }
  }
  return 0;
}

int card_clot_sliver(int player, int card, event_t event){
  /* Clot Sliver |1|B
   * 	  Creature - Sliver 1/1
   * 	  All Slivers have "{2}: Regenerate this permanent." */

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &clot_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &clot_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}
	return slivercycling(player, card, event);
}

int card_cloudchaser_eagle(int player, int card, event_t event){
  /* Cloudchaser Eagle |3|W
   * 	  Creature - Bird 2/2
   * 	  Flying
   * 	  When Cloudchaser Eagle enters the battlefield, destroy target enchantment. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ENCHANTMENT);
		td.allow_cancel = 0;

		if( can_target(&td) && new_pick_target(&td, "TARGET_ENCHANTMENT", 0, 0) ){
			action_on_target(player, card, 0, KILL_DESTROY);
		}
		get_card_instance(player, card)->number_of_targets = 0;
	}

	return 0;
}

int cq_effect(int player, int card, event_t event)
{

  if (get_card_instance(player, card)->targets[0].player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int cq_player = instance->targets[0].player;
    int cq_card = instance->targets[0].card;
    other_leaves_play(player, card, cq_player, cq_card, event, 0, 0);
    if ((event == EVENT_STATIC_EFFECTS && !is_tapped(cq_player, cq_card)) ||
        event == EVENT_LEAVES_PLAY_ABILITY)
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
    }
  }

  return 0;
}

int card_coffin_queen(int player, int card, event_t event)
{
  /* Coffin Queen	|2|B
   * Creature - Zombie Wizard 1/1
   * You may choose not to untap this creature during your untap step.
   * |2|B, |T: Put target creature card from a graveyard onto the battlefield under your control. When this creature becomes untapped or you lose control of this creature, exile that creature. */

  choose_to_untap(player, card, event);

  if (!IS_GAA_EVENT(event))
  {
    return 0;
  }

  test_definition_t test;
  new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
    if (selected != -1)
    {
      int zombie = reanimate_permanent(player, card, instance->targets[2].player, selected,
                                       REANIMATE_DEFAULT | REANIMATE_NO_CONTROL_LEGACY);

      if (zombie > -1)
      {
        int legacy = create_targetted_legacy_effect(
            instance->parent_controller, instance->parent_card, &cq_effect, player, zombie);
        card_instance_t *leg = get_card_instance(player, legacy);
        leg->targets[0].player = instance->parent_controller;
        leg->targets[0].card = instance->parent_card;
        leg->number_of_targets = 1;
        card_instance_t *parent =
            get_card_instance(instance->parent_controller, instance->parent_card);
        parent->targets[1] = instance->targets[0];
      }
    }
  }

  return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED,
                                                        MANACOST_XB(2, 1), 0, NULL, NULL, 0, &test,
                                                        GS_GRAVE_RECYCLER_BOTH_GRAVES);
}

int card_cold_storage(int player, int card, event_t event){
  /* Cold Storage |4
   * 	  Artifact
   * 	  {3}: Exile target creature you control.
   * 	  Sacrifice Cold Storage: Return each creature card exiled with Cold Storage to the battlefield under your control. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.allowed_controller = player;
	td.preferred_controller = player;
	if( player == AI ){
		td.required_state = TARGET_STATE_DESTROYED;
	}

	enum{
		CHOICE_EXILE = 1,
		CHOICE_RETURN,
		CHOICE_SHOW_EXILED
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( player == AI ){
			if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_REGENERATION, MANACOST_X(3), 0, &td, "TARGET_CREATURE") ){
				return 0x63;
			}
		}
		if( player == HUMAN ){
			if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_CREATURE") ){
				return 1;
			}
			if( exiledby_count(player, card, ANYBODY) ){
				return 1;
			}
		}
		if( generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE){
		instance->info_slot = instance->number_of_targets = 0;
		int abils[3] = {
						(player == AI ? generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_REGENERATION, MANACOST_X(3), 0, &td, NULL) :
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_X(3), 0, &td, NULL)),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL),
						player == HUMAN && exiledby_count(player, card, ANYBODY)
						};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Exile creature", abils[0], 10,
							"Sac & return creature", abils[1], (instance->targets[1].player > -1 ? instance->targets[1].player*5 : 0),
							"Show exiled creatures", abils[2], 1);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_EXILE ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_X(3)) ){
				pick_target(&td, "TARGET_CREATURE");
			}
		}
		if( choice == CHOICE_RETURN ){
			if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
				instance->targets[1].card = exiledby_detach(player, card);
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
		if( choice == CHOICE_SHOW_EXILED ){
			exiledby_choose(player, card, CARD_ID_COLD_STORAGE, EXBY_CHOOSE, 0, "Creature", 0);
			spell_fizzled = 1;
		}
		if( spell_fizzled != 1 ){
			instance->info_slot = choice;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( instance->info_slot == CHOICE_EXILE ){
			if( valid_target(&td) ){
				if( player == AI ){
					regenerate_target(instance->targets[0].player, instance->targets[0].card);
				}
				exile_card_and_remember_it_on_exiledby(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
				int prev = get_card_instance(instance->parent_controller, instance->parent_card)->targets[1].player;
				get_card_instance(instance->parent_controller, instance->parent_card)->targets[1].player = (prev != -1 ? prev+1 : 1);
			}
		}

		if( instance->info_slot == CHOICE_RETURN ){
			int leg = 0;
			int idx = 0;
			int* loc;

			while ((loc = exiledby_find_any(player-2, instance->targets[1].card, &leg, &idx)) != NULL){
					int owner = (*loc & 0x80000000) ? 1 : 0;
					int iid = *loc & ~0x80000000;
					*loc = -1;
					if( check_rfg(owner, cards_data[iid].id) ){
						int card_added = add_card_to_hand(player, iid);
						remove_card_from_rfg(owner, cards_data[iid].id);
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
		}
	}

	return 0;
}

int card_commander_greven_il_vec(int player, int card, event_t event){
  /* Commander Greven il-Vec	|3|B|B|B
   * Legendary Creature - Phyrexian Human Warrior 7/5
   * Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
   * When ~ enters, sacrifice a creature. */

	check_legend_rule(player, card, event);

	fear(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( count_permanents_by_type(player, TYPE_CREATURE) < 1 ){
			ai_modifier-=128;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}
	return 0;
}

int card_corpse_dance(int player, int card, event_t event){
  /* Corpse Dance	|2|B
   * Instant
   * Buyback |2 (You may pay an additional |2 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Return the top creature card of your graveyard to the battlefield. That creature gains haste until end of turn. Exile it at the beginning of the next end step. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		basic_spell(player, card, event);
		if( spell_fizzled != 1 ){
			get_card_instance(player, card)->info_slot = buyback(player, card, MANACOST_X(2));
		}
		if( ! count_graveyard_by_type(player, TYPE_CREATURE) ){
			ai_modifier -= 96;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		const int *grave = get_grave(player);
		int count = count_graveyard(player)-1;
		while( count > -1 ){
				if( is_what(-1, grave[count], TYPE_CREATURE) ){
					reanimate_permanent(player, card, player, count, REANIMATE_HASTE_AND_EXILE_AT_EOT);
					break;
				}
				count--;
		}
		if( get_card_instance(player, card)->info_slot == 1 ){
			bounce_permanent(player, card);
			return 0;
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_crazed_armodon(int player, int card, event_t event){
  /* Crazed Armodon	|2|G|G
   * Creature - Elephant 3/3
   * |G: This creature gets +3/+0 and gains trample until end of turn. Destroy this creature at the beginning of the next end step. Activate only once each turn. */

	if (event == EVENT_CHECK_ABILITIES){
		if (affect_me(player, card) && card_crazed_armodon(player, card, EVENT_CAN_ACTIVATE)){
			check_abilities_keywords |= KEYWORD_TRAMPLE;
			check_abilities_power += 3;
		}
	}
	else if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_ability_t pump;
		default_pump_ability_definition(instance->parent_controller, instance->parent_card, &pump, 3, 0, KEYWORD_TRAMPLE, 0);
		pump.paue_flags = PAUE_END_AT_EOT | PAUE_REMOVE_TARGET_AT_EOT;
		pump.eot_removal_method = KILL_DESTROY;
		pump_ability(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, &pump);
	}

	return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST_G(1), 0, NULL, NULL);
}

int card_cursed_scroll(int player, int card, event_t event){
	/* Cursed Scroll	|1	0x2001812
	 * Artifact
	 * |3, |T: Choose a card name, then reveal a card at random from your hand. If that card has the chosen name, ~ deals 2 damage to any target. */
	if( ! is_unlocked(player, card, event, 6) ){ return 0; }

	return card_magus_of_the_scroll(player, card, event);
}

int card_darkling_stalker(int player, int card, event_t event){
  /* Darkling Stalker	|3|B
   * Creature - Shade Spirit 1/1
   * |B: Regenerate this creature.
   * |B: This creature gets +1/+1 until end of turn. */

	if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST){
		return generic_shade(player, card, event, 0, MANACOST_B(1), 1, 1, 0, 0);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_PUMP = 1,
		CHOICE_REGENERATE
	};


	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL) ){
			if( ( land_can_be_played & LCBP_REGENERATION) && (! IS_AI(player) || can_regenerate(player, card)) ){
				return 0x63;
			}
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->info_slot = CHOICE_PUMP;
		if( ( land_can_be_played & LCBP_REGENERATION) && (! IS_AI(player) || can_regenerate(player, card)) ){
			instance->info_slot = CHOICE_REGENERATE;
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PUMP ){
			return generic_shade_merge_pt(player, card, event, 0, MANACOST_B(1), 1, 1);
		}
		if( instance->info_slot == CHOICE_REGENERATE && can_regenerate(instance->parent_controller, instance->parent_card) ){
			regenerate_target(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_dauthi_embrace(int player, int card, event_t event){
  /* Dauthi Embrace	|2|B
   * Enchantment
   * |B|B: Target creature gains shadow until end of turn. (It can block or be blocked by only creatures with shadow.) */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (current_phase < PHASE_DECLARE_BLOCKERS ? 48 : -48);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_SHADOW);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_B(2), 0, &td, "TARGET_CREATURE");
}

int card_dauthi_ghoul(int player, int card, event_t event){
  /* Dauthi Ghoul	|1|B
   * Creature - Dauthi Zombie 1/1
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * Whenever a creature with shadow dies, put a +1/+1 counter on this creature. */

	shadow(player, card, event);

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( in_play(affected_card_controller, affected_card) &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
			check_for_special_ability(affected_card_controller, affected_card, SP_KEYWORD_SHADOW) )
		{
			count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
		}
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( result ){
		for (int i = BYTE0(result); i > 0; --i){
			add_counters(player, card, player, card, COUNTER_P1_P1, 1);
		}
	}

	return 0;
}

int card_dauthi_horror(int player, int card, event_t event){
  /* Dauthi Horror	|1|B
   * Creature - Dauthi Horror 2/1
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * This creature can't be blocked by white creatures. */

	shadow(player, card, event);
	if(event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card, AL_STATIC) ){
		if(player == attacking_card_controller && card == attacking_card ){
			if (get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_WHITE) ){
				event_result = 1;
			}
		}
	}
	return 0;
}

int card_dauthi_marauder(int player, int card, event_t event){
  /* Dauthi Marauder	|2|B
   * Creature - Dauthi Minion 3/1
   * Shadow (This creature can block or be blocked by only creatures with shadow.) */

	shadow(player, card, event);
	return 0;
}

int card_dauthi_mercenary(int player, int card, event_t event)
{
  /* Dauthi Mercenary	|2|B
   * Creature - Dauthi Knight Mercenary 2/1
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * |1|B: This creature gets +1/+0 until end of turn. */

  if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
    return generic_shade(player, card, event, 0, MANACOST_XB(1, 1), 1, 0, 0, 0);
  shadow(player, card, event);
  return generic_shade_merge_pt(player, card, event, 0, MANACOST_XB(1, 1), 1, 0);
}

int card_dauthi_mindripper(int player, int card, event_t event){
  /* Dauthi Mindripper	|3|B
   * Creature - Dauthi Minion 2/1
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * Whenever this creature attacks and isn't blocked, you may sacrifice it. If you do, defending player discards three cards. */

   shadow(player, card, event);

   if( ! is_humiliated(player, card, AL_TRIG) ){
		if( event == EVENT_CAN_ACTIVATE && is_attacking(player, card) && is_unblocked(player, card) &&
			current_phase == PHASE_AFTER_BLOCKING )
		{
			return 1;
		}

		if( event == EVENT_ACTIVATE){
			kill_card(player, card, KILL_SACRIFICE);
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			new_multidiscard(1-player, 3, 0, player);
		}
	}

	return 0;
}

int card_dauthi_slayer(int player, int card, event_t event){
  /* Dauthi Slayer	|B|B
   * Creature - Dauthi Soldier 2/2
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * This creature attacks each combat if able. */

	shadow(player, card, event);
	return attack_if_able(player, card, event);
}

int card_death_pits_of_rath(int player, int card, event_t event){
  /* Death Pits of Rath |3|B|B
   * 	  Enchantment
   * 	  Whenever a creature is dealt damage, destroy it. It can't be regenerated. */

	if( ! is_humiliated(player, card, AL_TRIG) ){
		if( event == EVENT_DEAL_DAMAGE ){
			card_instance_t *instance = get_card_instance(player, card);
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_target_card != -1 ){
					if( is_what(damage->damage_target_player, damage->damage_target_card, TYPE_CREATURE) ){
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

						if( good == 1 ){
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
		}

		if( get_card_instance(player, card)->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
			reason_for_trigger_controller == player ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					card_instance_t *instance = get_card_instance(player, card);
					int i;
					for(i=0;i<instance->info_slot;i++){
						if( in_play(instance->targets[i].player, instance->targets[i].card) ){
							kill_card(instance->targets[i].player, instance->targets[i].card, KILL_BURY);
						}
					}
					instance->info_slot = 0;
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_diabolic_edict(int player, int card, event_t event){
  /* Diabolic Edict	|1|B
   * Instant
   * Target player sacrifices a creature of their choice. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);

			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED | SAC_NO_CANCEL, &test);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);

}

int card_dirtcowl_wurm(int player, int card, event_t event){
  /* Dirtcowl Wurm	|4|G
   * Creature - Wurm 3/4
   * Whenever an opponent plays a land, put a +1/+1 counter on this creature. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( whenever_a_player_plays_a_land(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY) ){
			add_counters(player, card, player, card, COUNTER_P1_P1, 1);
		}
	}

	return 0;
}

int card_dismiss2(int player, int card, event_t event){
  /* Dismiss |2|U|U
   * 	  Instant
   * 	  Counter target spell.
   * 	  Draw a card. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t* instance = get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return counterspell(player, card, event, NULL, 0);
}

int card_disturbed_burial(int player, int card, event_t event)
{
  /* Disturbed Burial	|1|B
   * Sorcery
   * Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Return target creature card from your graveyard to your hand. */

  // Buyback |3
  // Return target creature card from your graveyard to your hand.

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
		if( spell_fizzled != 1 ){
			get_card_instance(player, card)->info_slot = buyback(player, card, MANACOST_X(3));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
			if( get_card_instance(player, card)->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_dracoplasm(int player, int card, event_t event){
  /* Dracoplasm	|U|R
   * Creature - Shapeshifter 0/0
   * Flying
   * As this creature enters, sacrifice any number of creatures. This creature's power becomes the total power of those creatures and its toughness becomes their total toughness.
   * |R: This creature gets +1/+0 until end of turn. */

   if( event == EVENT_RESOLVE_SPELL ){
	   card_instance_t *instance = get_card_instance(player, card);

	   target_definition_t td;
	   default_target_definition(player, card, &td, TYPE_CREATURE);
	   td.preferred_controller = player;
	   td.allowed_controller = player;
	   td.illegal_abilities = 0;

	   add_state(player, card, STATE_OUBLIETTED);
	   instance->targets[1].player = 0;
	   instance->targets[1].card = 0;

	   test_definition_t test;
	   new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
	   while( can_target(&td) ){
		   int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		   if (!sac){
			   break;
		   }
		   instance->targets[1].player += get_power(BYTE2(sac), BYTE3(sac));
		   instance->targets[1].card += get_toughness(BYTE2(sac), BYTE3(sac));
		   kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
	   }
	   remove_state(player, card, STATE_OUBLIETTED);
   }

   if( event == EVENT_POWER && affect_me(player, card) ){
	   card_instance_t *instance = get_card_instance(player, card);
	   event_result += (instance->targets[1].player > -1 ? instance->targets[1].player : 0);
   }

   if( event == EVENT_TOUGHNESS && affect_me(player, card) ){
	   card_instance_t *instance = get_card_instance(player, card);
	   event_result += (instance->targets[1].card > -1 ? instance->targets[1].card : 0);
   }

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1, 0);
}

int card_dregs_of_sorrow(int player, int card, event_t event){
  /* Dregs of Sorrow	|X|4|B
   * Sorcery
   * Destroy X target nonblack creatures. Draw X cards. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}
	else if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			int trgs = 0;
			instance->number_of_targets = 0;
			while( can_target(&td) && has_mana(player, COLOR_COLORLESS, trgs+1) ){
					if( new_pick_target(&td, get_sleighted_color_text(player, card, "Select target non%s creature", COLOR_BLACK), trgs, GS_LITERAL_PROMPT) ){
						state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
						trgs++;
					}
					else{
						break;
					}
			}

			if( trgs > 0 ){
				int k;
				for(k=0; k<trgs; k++){
					state_untargettable(instance->targets[k].player, instance->targets[k].card, 0);
				}
				charge_mana(player, COLOR_COLORLESS, trgs);
				if( spell_fizzled != 1 ){
					instance->info_slot = trgs;
				}
			}
			else{
				spell_fizzled = 1;
			}
	}
	else if( event == EVENT_RESOLVE_SPELL ){
			int k;
			int good = 0;
			for(k=0; k<instance->info_slot; k++){
				if( validate_target(player, card, &td, k) ){
					kill_card(instance->targets[k].player, instance->targets[k].card, KILL_DESTROY);
					good++;
				}
			}
			if( good > 0 ){
				draw_cards(player, instance->info_slot);
			}
			kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_earthcraft(int player, int card, event_t event){
  /* Earthcraft	|1|G
   * Enchantment
   * Tap an untapped creature you control: Untap target basic land. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_LAND);
	td1.preferred_controller = player;
	td1.required_subtype = SUBTYPE_BASIC;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
		return can_target(&td1);
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int creature_to_tap = -1;
			if( new_pick_target(&td, "Select a creature to tap.", 0, 1 | GS_LITERAL_PROMPT) ){
				add_state(instance->targets[0].player, instance->targets[0].card, STATE_TARGETTED);
				creature_to_tap = instance->targets[0].card;
				instance->number_of_targets = 0;
				if( new_pick_target(&td1, "Select a basic land to untap.", 0, 1 | GS_LITERAL_PROMPT) ){
					tap_card(player, creature_to_tap);
				}
				remove_state(player, creature_to_tap, STATE_TARGETTED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			untap_card( instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_echo_chamber(int player, int card, event_t event){
  /* Echo Chamber	|4
   * Artifact
   * |4, |T: An opponent chooses target creature they control. Create a token that's a copy of that creature. That token gains haste until end of turn. Exile the token at the beginning of the next end step. Activate only as a sorcery. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = 1-player;
	td.allowed_controller = 1-player;
	td.who_chooses = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			token_generation_t token;
			copy_token_definition(player, card, &token, instance->targets[0].player, instance->targets[0].card);
			token.legacy = 1;
			token.special_code_for_legacy = &haste_and_remove_eot;
			generate_token(&token);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_CAN_SORCERY_BE_PLAYED, MANACOST_X(4), 0,
									&td, "TARGET_CREATURE");
}

int vineyard_effect(int player, int card, event_t event){

	if (event == EVENT_PHASE_CHANGED && current_phase == PHASE_MAIN1 ){
		if( get_card_instance(player, card)->targets[1].player != 1 ){
			produce_mana(current_turn, COLOR_GREEN, 2);
			get_card_instance(player, card)->targets[1].player = 1;
		}
	}

	if(event == EVENT_PHASE_CHANGED && current_phase != PHASE_MAIN1){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	return 0;
}

int card_eladamris_vineyard(int player, int card, event_t event){
  /* Eladamri's Vineyard	|G
   * Enchantment
   * At the beginning of each player's first main phase, that player adds |G|G. */

	vineyard_effect(player, card, event);

	return global_enchantment(player, card, event);
}

int card_eladamri_lord_of_leaves(int player, int card, event_t event){
  /* Eladamri, Lord of Leaves	|G|G
   * Legendary Creature - Elf Warrior 2/2
   * Other Elf creatures have forestwalk. (They can't be blocked as long as defending player controls a Forest.)
   * Other Elves have shroud. (They can't be the targets of spells or abilities.) */

	check_legend_rule(player, card, event);

	boost_creature_type(player, card, event, SUBTYPE_ELF, 0, 0, KEYWORD_SHROUD | get_hacked_walk(player, card, KEYWORD_FORESTWALK), 0);

	return 0;
}

int card_elite_javelineers(int player, int card, event_t event){
  /* Elite Javelineer |2|W
   * 	  Creature - Human Soldier 2/2
   * 	  Whenever Elite Javelineer blocks, it deals 1 damage to target attacking creature. */

	if( blocking(player, card, event) && ! is_humiliated(player, card, AL_TRIG) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_state = TARGET_STATE_ATTACKING;
		td.allow_cancel = 0;
		if( can_target(&td) && new_pick_target(&td, "Select target attacking creature.", 0, GS_LITERAL_PROMPT) ){
			damage_target0(player, card, 1);
		}
		get_card_instance(player, card)->number_of_targets = 0;
	}

	return 0;
}

int card_elven_warhounds(int player, int card, event_t event)
{
  /* Elven Warhounds	|3|G
   * Creature - Dog 2/2
   * Whenever this creature becomes blocked by a creature, put that creature on top of its owner's library. */

  // Whenever ~ becomes blocked by a creature, put that creature on top of its owner's library.
  if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && current_turn == player && !is_humiliated(player, card, AL_TRIG))
	get_card_instance(player, card)->destroys_if_blocked |= DIFB_DESTROYS_ALL;

  if (event == EVENT_DECLARE_BLOCKERS && is_attacking(player, card) && !is_humiliated(player, card, AL_TRIG))
	{
	  char marked[2][151] = {{0}};
	  mark_each_creature_blocking_me(player, card, marked);
	  int c;
	  for (c = 0; c < active_cards_count[1-current_turn]; ++c)
		if (marked[1-current_turn][c] && in_play(1-current_turn, c))
		  put_on_top_of_deck(1-current_turn, c);
	}

  return 0;
}

int card_elvish_fury(int player, int card, event_t event){
  /* Elvish Fury	|G
   * Instant
   * Buyback |4 (You may pay an additional |4 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Target creature gets +2/+2 until end of turn. */

	if( !IS_GS_EVENT(player, card, event)){
		return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 2, 0, 0);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			instance->info_slot = buyback(player, card, MANACOST_X(4));
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_emerald_medallion(int player, int card, event_t event){
/* CARD_ID_EMERALD_MEDALLION	3132
Emerald Medallion	|2
Artifact
|SGreen spells you cast cost |1 less to cast. */

	return tempest_medallion(player, card, event, get_sleighted_color_test(player, card, COLOR_TEST_GREEN));
}

int card_emessi_tome(int player, int card, event_t event){
  /* Emmessi Tome	|4
   * Artifact - Book
   * |5, |T: Draw two cards, then discard a card. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 2);
		discard(player, 0, player);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(5), 0, NULL, NULL);
}

int card_essence_bottle(int player, int card, event_t event)
{
  /* Essence Bottle	|2
   * Artifact
   * |3, |T: Put an elixir counter on ~.
   * |T, Remove all elixir counters from ~: You gain 2 life for each elixir counter removed this way. */

  enum
  {
    CHOICE_COUNTER = 1,
    CHOICE_LIFE = 2
  };

  int result;
  card_instance_t *instance;
  uint32_t elixir_counters;
  bool opponents_turn;
  if ((event == EVENT_RESOLVE_ACTIVATION) ||
      (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE))
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = is_tapped(player, card);
      if (((result == 0) && (result = is_animated_and_sick(player, card), result == 0)) &&
          (result = can_use_activated_abilities(player, card), result != 0))
      {
        result = count_counters(player, card, COUNTER_ELIXIR);
        return dialog_impl(player, card, EVENT_CAN_ACTIVATE, DLG_RANDOM, "Add a counter", 1, 3,
                           DLG_MANA6(3, 0, 0, 0, 0, 0), "Gain life", 1, result, DLG_MANA6(0, 0,
                           0, 0, 0, 0), 0, 0, 0, 0);
      }
    }
    else
    {
      result = count_counters(player, card, COUNTER_ELIXIR);
      result =
          dialog_impl(player, card, event, DLG_RANDOM, "Add a counter", 1, 3, DLG_MANA6(3, 0, 0,
                      0, 0, 0), "Gain life", 1, result, DLG_MANA6(0, 0, 0, 0, 0, 0), 0, 0, 0, 0);
      if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
      {
        instance = get_card_instance(player, card);
        if (result == CHOICE_COUNTER)
        {
          opponents_turn = current_turn != player;
          instance->state = instance->state | 0x10;
          if ((opponents_turn) && (current_phase == 0x1f))
          {
            ai_modifier = ai_modifier + 0x40;
          }
        }
        else if (result == CHOICE_LIFE)
        {
          instance->state = instance->state | 0x10;
          elixir_counters = count_counters(player, card, COUNTER_ELIXIR);
          instance->eot_toughness = elixir_counters;
          remove_all_counters(player, card, player, card, COUNTER_ELIXIR);
          ai_modifier = ai_modifier - (life[player] * 2 + 0x20);
        }
        tapped_for_mana_color = -1;
        return 0;
      }
      if (result == CHOICE_COUNTER)
      {
        add_counters(player, card, player, card, COUNTER_ELIXIR, 1);
      }
      else if (result == CHOICE_LIFE)
      {
        instance = get_card_instance(player, card);
        gain_life(player, instance->eot_toughness * 2);
        return 0;
      }
    }
  }
  return 0;
}

int card_energizer(int player, int card, event_t event){
  /* Energizer	|4
   * Artifact Creature - Juggernaut 2/2
   * |2, |T: Put a +1/+1 counter on this creature. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL) ){
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL);
}

int card_escaped_shapeshifter(int player, int card, event_t event){
  /* Escaped Shapeshifter	|3|U|U
   * Creature - Shapeshifter 3/4
   * As long as an opponent controls a creature with flying not named ~, this creature has flying. The same is true for first strike, trample, and protection from any color. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		int keyword_mask = 	KEYWORD_FLYING | KEYWORD_FIRST_STRIKE | KEYWORD_TRAMPLE | KEYWORD_PROT_BLACK | KEYWORD_PROT_BLUE |
							KEYWORD_PROT_GREEN | KEYWORD_PROT_RED | KEYWORD_PROT_RED;
		int i;
		int keyword = 0;
		for(i=0; i<active_cards_count[1-player]; i++){
			if( in_play(1-player, i) && is_what(1-player, i, TYPE_CREATURE) && get_id(1-player, i) != get_id(player, card) ){
				int opp_key = get_abilities(1-player, i, EVENT_ABILITIES, -1);
				keyword |= (opp_key & keyword_mask);
			}
			if( keyword == keyword_mask ){
				break;
			}
		}
		event_result |= keyword;
	}

	return 0;
}

int card_evincars_justice(int player, int card, event_t event){
  /* Evincar's Justice	|2|B|B
   * Sorcery
   * Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * ~ deals 2 damage to each creature and each player. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		basic_spell(player, card, event);
		if( spell_fizzled != 1 ){
			get_card_instance(player, card)->info_slot = buyback(player, card, MANACOST_X(3));
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		new_damage_all(player, card, ANYBODY, 2, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
		if( get_card_instance(player, card)->info_slot == 1 ){
			bounce_permanent(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_extinction(int player, int card, event_t event){
  /* Extinction	|4|B
   * Sorcery
   * Destroy all creatures of the creature type of your choice. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.subtype = select_a_subtype(player, card);

		new_manipulate_all(player, card, 2, &this_test, KILL_DESTROY);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_feverd_convulsion(int player, int card, event_t event){
  /* Fevered Convulsions	|B|B
   * Enchantment
   * |2|B|B: Put a -1/-1 counter on target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_M1_M1, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XB(2, 2), 0, &td, "TARGET_CREATURE");
}

static int field_of_souls_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_FIELD_OF_SOULS)
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
            SET_BYTE1(instance->targets[11].player) = 1;
          }
        }
        if (!is_token(affected_card_controller, affected_card) &&
            get_owner(affected_card_controller, affected_card) == player)
        {
          count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
        }
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_SPIRIT, &token);
    token.color_forced = COLOR_TEST_WHITE;
    token.key_plus = KEYWORD_FLYING;
    token.qty = BYTE0(result);
    generate_token(&token);

    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_field_of_souls(int player, int card, event_t event){
	/* Field of Souls	|2|W|W
	 * Enchantment
	 * Whenever a nontoken creature is put into your graveyard from the battlefield, put a 1/1 |Swhite Spirit creature token with flying onto the battlefield. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (intptr_t)field_of_souls_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &field_of_souls_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY && ! is_token(affected_card_controller, affected_card) &&
		get_owner(affected_card_controller, affected_card) == player )
	{
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SPIRIT, &token);
			token.color_forced = COLOR_TEST_WHITE;
			token.key_plus = KEYWORD_FLYING;
			token.qty = BYTE0(result);
			generate_token(&token);
		}
	}

	return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x214fa10; original cards/tempest.c:2359. */
int card_fireslinger(int player, int card, event_t event)
{
  /* Fireslinger	|1|R
   * Creature - Human Wizard 1/1
   * |T: This creature deals 1 damage to any target and 1 damage to you. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      damage_target0(player, card, 1);
      damage_player(instance->parent_controller, 1, instance->parent_controller,
                    instance->parent_card);
    }
  }
  result =
      generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td, "TARGET_ANY");
  return result;
}

int card_flickering_ward(int player, int card, event_t event){
  /* Flickering Ward	|W
   * Enchantment - Aura
   * Enchant creature
   * As this Aura enters, choose a color.
   * Enchanted creature has protection from the chosen color. This effect doesn't remove this Aura.
   * |W: Return this Aura to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if (event == EVENT_ETB_ABILITY){ get_card_instance(player, card)->info_slot = select_a_protection(player); }
	if (this_effect_doesnt_remove_this(player, card, event)){ return 0; }
	if (event == EVENT_ABILITIES){
	    card_instance_t* instance = in_play(player, card);
	    if (instance && instance->damage_target_card >= 0 && !is_humiliated(player, card, AL_STATIC) && affect_me(instance->damage_target_player, instance->damage_target_card)){
	        event_result |= instance->info_slot;
	    }
	}
	if (IS_GAA_EVENT(event)){
	    if (event == EVENT_RESOLVE_ACTIVATION){
	        card_instance_t* instance = get_card_instance(player, card);
	        bounce_permanent(instance->parent_controller, instance->parent_card);
	    }
	    return generic_activated_ability(player, card, event, 0, MANACOST_W(1), 0, NULL, NULL);
	}
	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }
	return aura(player, card, event, NULL, NULL);
}

int card_flowstone_sculpture(int player, int card, event_t event)
{
  /* Flowstone Sculpture	|6
   * Artifact Creature - Shapeshifter 4/4
   * |2, Discard a card: Put a +1/+1 counter on this creature or this creature gains flying, first strike, or trample. (This effect lasts indefinitely.) */

  int choice;
  card_instance_t *instance;
  int power_or_mode;
  int toughness_or_ability;
  int opponent_or_attacking;
  card_instance_t *blocker_or_parent;
  unsigned int argument7;
  int card_00;
  int blocker_toughness;
  int blocker_power;

  if (event == EVENT_CHECK_ABILITIES)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (power_or_mode = card_flowstone_sculpture(player, card, EVENT_CAN_ACTIVATE),
         power_or_mode != 0))
    {
      check_abilities_keywords = check_abilities_keywords | 0x180;
      power_or_mode = has_mana(player, 7, 2);
      check_abilities_power = hand_count[player];
      if (power_or_mode < hand_count[player])
      {
        check_abilities_power = power_or_mode;
      }
      check_abilities_toughness = hand_count[player];
      if (power_or_mode < hand_count[player])
      {
        check_abilities_toughness = power_or_mode;
      }
    }
  }
  else
  {
    if (event == EVENT_ABILITIES)
    {
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
      instance = get_card_instance(player, card);
      if (instance->targets[1].card < 1)
      {
        return 0;
      }
      if (is_humiliated(player, card, AL_STATIC) != 0)
      {
        return 0;
      }
      instance = get_card_instance(player, card);
      event_result = event_result | instance->targets[1].card;
      return 0;
    }
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x20, 2, 0, 0, 0, 0, 0,
                                         0, NULL, NULL);
      }

      argument7 = (unsigned int)(current_phase < 0x17);
      if (!(current_phase != 0x18))
      {
        power_or_mode = get_power(player, card);
        toughness_or_ability = get_toughness(player, card);
        opponent_or_attacking = is_attacking(player, card);
        if ((opponent_or_attacking != 0) &&
            (opponent_or_attacking = is_unblocked(player, card), opponent_or_attacking == 0))
        {
          opponent_or_attacking = 1 - player;
          blocker_toughness = 0;
          blocker_power = 0;
          if (0 < active_cards_count[opponent_or_attacking])
          {
            card_00 = 0;
            do
            {
              blocker_or_parent = in_play(opponent_or_attacking, card_00);
              if (((blocker_or_parent != NULL) &&
                   (is_what(opponent_or_attacking, card_00, 2) != 0)) &&
                  (blocker_or_parent = get_card_instance(opponent_or_attacking, card_00),
                   (unsigned int)blocker_or_parent->blocking == card))
              {
                blocker_power = get_power(opponent_or_attacking, card_00);
                blocker_toughness = get_toughness(opponent_or_attacking, card_00);
              }
              card_00 = card_00 + 1;
            } while (card_00 < active_cards_count[opponent_or_attacking]);
          }
          if (blocker_power < toughness_or_ability)
          {
            if (blocker_toughness < power_or_mode)
            {
              toughness_or_ability = check_for_ability(player, card, 0x80);
              if (toughness_or_ability == 0)
              {
                argument7 = 3;
              }
            }
          }
          else
          {
            if ((blocker_toughness <= power_or_mode) &&
                (toughness_or_ability = check_for_ability(player, card, 0x100),
                 toughness_or_ability == 0))
            {
              argument7 = 2;
            }
          }
        }
        if (((current_turn != player) && (instance->blocking != 0xff)) &&
            ((toughness_or_ability = get_toughness(1 - player, (unsigned int)instance->blocking),
              toughness_or_ability <= power_or_mode &&
                  (power_or_mode = check_for_ability(player, card, 0x100), power_or_mode == 0))))
        {
          argument7 = 2;
        }
      }
      choice = do_dialog(
          player, player, card, -1, -1,
          " Add +1/+1 counter\n Gains flying\n Gains first strike\n Gains trample\n Cancel",
          argument7);
      if (choice == 4)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (charge_mana_for_activated_ability(player, card, 2, 0, 0, 0, 0, 0) == 0)
      {
        return 0;
      }
      discard(player, 0, player);
      instance->info_slot = choice + 0x42;
      return 0;
    }
  }
  if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    blocker_or_parent = get_card_instance(instance->parent_controller, instance->parent_card);
    if (blocker_or_parent->targets[1].card < 0)
    {
      blocker_or_parent->targets[1].card = 0;
    }
    power_or_mode = instance->info_slot;
    if (power_or_mode == 0x42)
    {
      add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1,
                   1);
      power_or_mode = instance->info_slot;
    }
    if (power_or_mode == 0x43)
    {
      blocker_or_parent->targets[1].card = blocker_or_parent->targets[1].card | 0x20;
    }
    else
    {
      if (power_or_mode == 0x44)
      {
        blocker_or_parent->targets[1].card = blocker_or_parent->targets[1].card | 0x100;
        return 0;
      }
      if (power_or_mode == 0x45)
      {
        blocker_or_parent->targets[1].card = blocker_or_parent->targets[1].card | 0x80;
        return 0;
      }
    }
  }
  return 0;
}

int card_fools_tome(int player, int card, event_t event){
  /* Fool's Tome	|4
   * Artifact - Book
   * |2, |T: Draw a card. Activate only if you have no cards in hand. */

	if( event == EVENT_CAN_ACTIVATE && hand_count[player] ){
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL);
}

int card_furnace_of_rath(int player, int card, event_t event)
{
	/* Bitter Feud	|4|R	0x2001da8
	 * Enchantment
	 * As ~ enters the battlefield, choose two players.
	 * If a source controlled by one of the chosen players would deal damage to the other chosen player or a permanent that player controls, that source deals double that damage to that player or permanent instead. */
	/* Furnace of Rath	|1|R|R|R	0x2001DA8
	 * Enchantment
	 * If a source would deal damage to a permanent or player, it deals double that damage to that permanent or player instead. */
	card_instance_t* damage = damage_being_dealt(event);
	if( damage && in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		damage->info_slot *= 2;
	}

	return global_enchantment(player, card, event);
}

int card_gerrards_battle_cry(int player, int card, event_t event){
  /* Gerrard's Battle Cry	|W
   * Enchantment
   * |2|W: Creatures you control get +1/+1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1, 1, 1, 0, 0);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XW(2, 1), 0, NULL, NULL);
}

int card_goblin_bombardment(int player, int card, event_t event){
	/* Goblin Bombardment	|1|R	0x2003478
	 * Enchantment
	 * Sacrifice a creature: ~ deals 1 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE, MANACOST0, 0,
									&td1, "TARGET_ANY");
}

int card_grindstone(int player, int card, event_t event){
  /* Grindstone	|1
   * Artifact
   * |3, |T: Target player mills two cards. If two cards that share a color were milled this way, repeat this process. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
            unsigned int timestamp = 0;
            int changer = -1;
            int colors = 0;
            for (int p = 0; p < 2; ++p){
                for (int c = 0; c < active_cards_count[p]; ++c){
                    if (in_play(p, c)){
                        if (get_id(p, c) == CARD_ID_PAINTERS_SERVANT){
                            card_instance_t* inst = get_card_instance(p, c);
                            if (inst->targets[1].card >= 0 && inst->timestamp > timestamp){
                                timestamp = inst->timestamp;
                                changer = CARD_ID_PAINTERS_SERVANT;
                                colors |= inst->targets[1].card;
                            }
                        }
                        if (p == player && get_id(player, c) == CARD_ID_CELESTIAL_DAWN){
                            card_instance_t* inst = get_card_instance(player, c);
                            if (inst->timestamp > timestamp){
                                timestamp = inst->timestamp;
                                changer = CARD_ID_CELESTIAL_DAWN;
                                colors = get_sleighted_color_test(player, c, COLOR_TEST_WHITE);
                            }
                        }
                        if (get_id(p, c) == CARD_ID_MYCOSYNTH_LATTICE){
                            card_instance_t* inst = get_card_instance(p, c);
                            if (inst->timestamp > timestamp){
                                timestamp = inst->timestamp;
                                changer = CARD_ID_MYCOSYNTH_LATTICE;
                                colors = COLOR_TEST_COLORLESS;
                            }
                        }
                    }
                }
            }
            int* deck = deck_ptr[instance->targets[0].player];
            int to_mill = 2;
            if (changer != CARD_ID_MYCOSYNTH_LATTICE){
                if (changer == CARD_ID_PAINTERS_SERVANT){
                    to_mill = count_deck(instance->targets[0].player);
                } else {
                    int count = 0;
                    while (deck[count] != -1){
                        int c1;
                        int c2;
                        if (changer == CARD_ID_CELESTIAL_DAWN){
                            c1 = cards_data[deck[count]].type & TYPE_LAND ? COLOR_TEST_COLORLESS : colors;
                            c2 = cards_data[deck[count+1]].type & TYPE_LAND ? COLOR_TEST_COLORLESS : colors;
                        } else {
                            c1 = get_color_by_internal_id_no_hack(deck[count]);
                            c2 = get_color_by_internal_id_no_hack(deck[count+1]);
                        }
                        if (!(c1 & c2) || c1 == COLOR_TEST_COLORLESS || c2 == COLOR_TEST_COLORLESS){ break; }
                        to_mill += 2;
                        count += 2;
                    }
                    if (to_mill > count_deck(instance->targets[0].player)){ to_mill = count_deck(instance->targets[0].player); }
                }
            }
            mill(instance->targets[0].player, to_mill);
        }
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_PLAYER");
}

int card_hannas_custody(int player, int card, event_t event){
  /* Hanna's Custody	|2|W
   * Enchantment
   * All artifacts have shroud. (They can't be the targets of spells or abilities.) */

	if( event == EVENT_ABILITIES && ! is_humiliated(player, card, AL_STATIC) && in_play(player, card) ){
		if( is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) ){
			event_result |= KEYWORD_SHROUD;
		}
	}

	return global_enchantment(player, card, event);
}

int card_heart_sliver(int player, int card, event_t event)
{
  /* Heart Sliver	|1|R
   * Creature - Sliver 1/1
   * All Sliver creatures have haste. */

  // All Sliver creatures have haste.
  boost_subtype(player, card, event, SUBTYPE_SLIVER, 0,0, 0,SP_KEYWORD_HASTE, BCT_INCLUDE_SELF);

  return slivercycling(player, card, event);
}

int card_heartwood_giant(int player, int card, event_t event){
	/* Heartwood Giant	|3|G|G	0x200a048
	 * Creature - Giant 4/4
	 * |T, Sacrifice |Ha Forest: ~ deals 2 damage to target player or planeswalker. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	default_test_definition(&test, TYPE_LAND);
	test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_ACTIVATE ){
		scnprintf(test.message, 100, "%s", get_hacked_land_text(player, card, "Select a %s to sacrifice", SUBTYPE_FOREST));
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0,
															&td, "TARGET_PLAYER_OR_PLANESWALKER", 0, &test, 0);
}

int card_helm_of_possession(int player, int card, event_t event){

	/* Helm of Possession	|4
	 * Artifact
	 * You may choose not to untap ~ during your untap step.
	 * |2, |T, Sacrifice a creature: Gain control of target creature for as long as you control ~ and ~ remains tapped. */

	choose_to_untap(player, card, event);

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		card_instance_t* instance = get_card_instance(player, card);
		gain_control_until_source_is_in_play_and_tapped(player, card, instance->targets[0].player, instance->targets[0].card,
														GCUS_SOURCE_IN_PLAY | GCUS_CONTROLLED | GCUS_TAPPED);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_SACRIFICE_CREATURE, MANACOST_X(2), 0,
									&td, "TARGET_CREATURE");
}

int card_horned_sliver(int player, int card, event_t event){
  /* Horned Sliver	|2|G
   * Creature - Sliver 2/2
   * All Sliver creatures have trample. */

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 0, 0, KEYWORD_TRAMPLE, BCT_INCLUDE_SELF);
	return slivercycling(player, card, event);
}

int card_humility(int player, int card, event_t event)
{
  /* Humility	|2|W|W
   * Enchantment
   * All creatures lose all abilities and have base power and toughness 1/1. */

  card_instance_t *instance;
  if ((2 < (uint32_t)(event - EVENT_POWER)) && (event != EVENT_CAST_SPELL))
  {
    return global_enchantment(player, card, event);
  }
  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = in_play(affected_card_controller, affected_card);
    if (instance != NULL)
    {
      if (is_what(affected_card_controller, affected_card, 2))
      {
        if (!is_humiliated(player, card, AL_STATIC))
        {
          if ((uint32_t)(event - EVENT_POWER) < 2)
          {
            set_base_pt(1);
          }
          else if ((event == EVENT_ABILITIES) || (event == EVENT_CAST_SPELL))
          {
            instance = get_card_instance(affected_card_controller, affected_card);
            event_result = event_result & 0x9f080000;
            instance->state = instance->state & 0xffffdfff;
            instance->ext->sp_keywords = instance->ext->sp_keywords & 0x993c00c0;
            humiliate(player, card, affected_card_controller, affected_card, 1);
          }
          return global_enchantment(player, card, event);
        }
      }
    }
  }
  return 0;
}

int card_intuition(int player, int card, event_t event){
  /* Intuition	|2|U
   * Instant
   * Search your library for three cards and reveal them. Target opponent chooses one. Put that card into your hand and the rest into your graveyard. Then shuffle. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 96;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int cards = MIN(count_deck(player), 3);
		if( cards > 0 ){
			int i;
			int chosen[3] = {-1, -1, -1};
			int *deck = deck_ptr[player];
			for(i=0; i<cards; i++){
				int selected = pick_card_from_deck(player);
				chosen[i] = deck[selected];
				remove_card_from_deck(player, selected);
			}

			int card_to_add = -1;
			if( !IS_AI(1-player) ){
				while( card_to_add == -1 ){
						card_to_add = show_deck(1-player, chosen, 3, "Choose a card", 0, gs_done_008b40e0 );
				}
			}
			else{
				i=0;
				int max_value = 1000;
				for(i=0; i<cards; i++){
					card_ptr_t* c = cards_ptr[ cards_data[chosen[i]].id ];
					if( c->ai_base_value < max_value ){
						max_value = c->ai_base_value;
						card_to_add = i;
					}
				}
			}

			add_card_to_hand(player, chosen[card_to_add]);
			chosen[card_to_add] = -1;
			int num_unchosen = 0;
			for (i = 0; i < cards; ++i){
				if( chosen[i] != -1 ){
					raw_put_iid_on_top_of_deck(player, chosen[i]);
					++num_unchosen;
				}
			}
			mill(player, num_unchosen);
		}
		shuffle(player);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static const char* not_effect_from_monotype_planeswalker(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
  /* This is the first card I've seen that can target all of artifact, creature, enchantment, and land, but not planeswalker.  Putting TARGET_TYPE_PLANESWALKER
   * into td.illegal_type is tempting, but won't work - it'll fail for an animated Gideon Jura, a planeswalker-artifact (by Memnarch), or planeswalker-
   * enchantment (by Enchanted Evening). */
  card_instance_t* eff = get_card_instance(player, card);
  if (!is_what(-1, eff->original_internal_card_id, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_LAND)
	  && is_planeswalker(-1, eff->original_internal_card_id)
	  && !check_special_flags2(eff->parent_controller, eff->parent_card, SF2_ENCHANTED_EVENING))	// works properly even if parent card has left play by now
	return "type";

  return NULL;
}

static int interdict_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance( player, card );
	if( instance->targets[0].player > -1 ){
		if( eot_trigger(player, card, event) ){
			disable_all_activated_abilities(instance->targets[0].player, instance->targets[0].card, 0);
			kill_card(player, card, KILL_EXILE);
		}
	}
	return 0;
}

int card_interdict(int player, int card, event_t event)
{
  /* Interdict	|1|U
   * Instant
   * Counter target activated ability from an artifact, creature, enchantment, or land. That permanent's activated abilities can't be activated this turn. (Mana abilities can't be targeted.)
   * Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

  target_definition_t td;
  counter_activated_target_definition(player, card, &td, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_ENCHANTMENT|TYPE_LAND);
  td.extra = target_predicate_extra(not_effect_from_monotype_planeswalker);
  td.special |= TARGET_SPECIAL_EXTRA_FUNCTION;

  card_instance_t *instance = get_card_instance( player, card );

  if (event == EVENT_CAN_CAST)
	return can_counter_activated_ability(player, card, event, &td);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  instance->number_of_targets = 0;
	  return cast_counter_activated_ability(player, card, 0);
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (resolve_counter_activated_ability(player, card, &td, 0))
		{
		  card_instance_t* tgt = get_card_instance(instance->targets[0].player, instance->targets[0].card);
		  disable_all_activated_abilities(tgt->parent_controller, tgt->parent_card, 1);
		  create_targetted_legacy_effect(player, card, &interdict_legacy, tgt->parent_controller, tgt->parent_card);
		  draw_cards(player, 1);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_invulnerability(int player, int card, event_t event){
  /* Invulnerability	|1|W
   * Instant
   * Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * The next time a source of your choice would deal damage to you this turn, prevent that damage. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;
	td.extra = damage_card;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_DAMAGE_PREVENTION | GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_DAMAGE_PREVENTION | GS_CAN_TARGET, &td, "TARGET_DAMAGE", 1, NULL);
		if( spell_fizzled != 1 ){
			instance->info_slot = buyback(player, card, MANACOST_X(3));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, -1);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_jackal_pup(int player, int card, event_t event){
  /* Jackal Pup	|R
   * Creature - Jackal 2/1
   * Whenever this creature is dealt damage, it deals that much damage to you. */

	if ( ! is_humiliated(player, card, AL_TRIG) && damage_dealt_to_me_arbitrary(player, card, event, 0, player, card)){
		card_instance_t* instance = get_card_instance(player, card);
		damage_player(player, instance->targets[7].card, player, card);
	}
	return 0;
}

int card_jet_medallion(int player, int card, event_t event){
/* CARD_ID_JET_MEDALLION	3180
Jet Medallion	|2
Artifact
|SBlack spells you cast cost |1 less to cast. */

	return tempest_medallion(player, card, event, get_sleighted_color_test(player, card, COLOR_TEST_BLACK));
}

int card_kezzerdrix(int player, int card, event_t event){
  /* Kezzerdrix	|2|B|B
   * Creature - Rabbit Beast 4/4
   * First strike
   * At the beginning of your upkeep, if your opponents control no creatures, this creature deals 4 damage to you. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( count_subtype(1-player, TYPE_CREATURE, -1) < 1 ){
			damage_player(player, 4, player, card);
		}
	}

	return 0;
}

int card_kindle(int player, int card, event_t event){
	/* Flame Burst	|1|R	0x200349b
	 * Instant
	 * ~ deals X damage to any target, where X is 2 plus the number of cards named ~ in all graveyards. */
	/* Kindle	|1|R	0x200349b
	 * Instant
	 * ~ deals X damage to any target, where X is 2 plus the number of cards named ~ in all graveyards. */
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = 2;
			amount+= count_graveyard_by_id( player, get_id(player, card) );
			amount+= count_graveyard_by_id( 1-player, get_id(player, card) );
			damage_target0(player, card, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_knight_of_dawn(int player, int card, event_t event){
  /* Knight of Dawn	|1|W|W
   * Creature - Human Knight 2/2
   * First strike
   * |W|W: This creature gains protection from the color of your choice until end of turn. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								0, 0, select_a_protection(player) | KEYWORD_RECALC_SET_COLOR, 0);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_W(2), 0, NULL, NULL);
}

static const char* is_blocking_kod(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  if (is_blocking(card, targeting_card))
	return NULL;
  else
	return "must be blocking Knight of Dusk";
}

int card_knight_of_dusk(int player, int card, event_t event){
  /* Knight of Dusk	|1|B|B
   * Creature - Human Knight 2/2
   * |B|B: Destroy target creature blocking this creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.allowed_controller =  1-player;
	td2.preferred_controller = 1-player;
	td2.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td2.extra = target_predicate_extra(is_blocking_kod);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td2) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_B(2), 0,
									&td2, "Select target creature blocking Knight of Dusk.");
}

int card_krakilin(int player, int card, event_t event){
  /* Krakilin	|X|G|G
   * Creature - Beast 0/0
   * This creature enters with X +1/+1 counters on it.
   * |1|G: Regenerate this creature. */

	if( IS_GAA_EVENT(event) ){
		regeneration(player, card, event, MANACOST_XG(1, 1));
	}

	return card_ivy_elemental(player, card, event);
}

int card_legacys_allure(int player, int card, event_t event){

	/* Legacy's Allure	|U|U
	 * Enchantment
	 * At the beginning of your upkeep, you may put a treasure counter on ~.
	 * Sacrifice ~: Gain control of target creature with power less than or equal to the number of treasure counters on ~. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counters(player, card, player, card, COUNTER_TREASURE, 1);
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = 1-player;
	td.allowed_controller = 1-player;
	td.power_requirement = count_counters(player, card, COUNTER_TREASURE) | TARGET_PT_LESSER_OR_EQUAL;

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_lightning_blast(int player, int card, event_t event){
	/* Flame Lash	|3|R	0x200cce4
	 * Instant
	 * ~ deals 4 damage to any target. */
	/* Lightning Blast	|3|R	0x200cce4
	 * Instant
	 * ~ deals 4 damage to any target. */
	/* Unfriendly Fire	|4|R	0x200cce4
	 * Instant
	 * ~ deals 4 damage to any target. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_living_death(int player, int card, event_t event){
  /* Living Death	|3|B|B
   * Sorcery
   * Each player exiles all creature cards from their graveyard, then sacrifices all creatures they control, then puts all cards they exiled this way onto the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_reanimate_all(player, card, ANYBODY, &this_test, REANIMATE_SPECIAL_LIVING_DEAD);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_lobotomy(int player, int card, event_t event){

	/* Lobotomy	|2|U|B
	 * Sorcery
	 * Target player reveals his or her hand, then you choose a card other than a basic land card from it. Search that player's graveyard, hand, and library for
	 * all cards with the same name as the chosen card and exile them. Then that player shuffles his or her library. */

	if (!IS_GS_EVENT(player, card, event)){ return 0; }
	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	if (event == EVENT_RESOLVE_SPELL){
	    if (valid_target(&td)){
	        int target = get_card_instance(player, card)->targets[0].player;
	        if (hand_count[target] > 0){
	            int cards_array[500];
	            int hand_index = 0;
	            int ai_pick = -1;
	            int ai_id = -1;
	            int min_value = 0;
	            for (int c = 0; c < active_cards_count[target]; ++c){
	                card_instance_t* instance = in_hand(target, c);
	                if (instance && !is_basic_land(target, c)){
	                    card_ptr_t* data = cards_ptr[get_id(target, c)];
	                    if (data->ai_base_value > min_value){
	                        min_value = data->ai_base_value;
	                        ai_pick = hand_index;
	                        ai_id = get_id(target, c);
	                    }
	                    cards_array[hand_index++] = instance->internal_card_id;
	                }
	            }
	            int id = ai_id;
	            if (player == HUMAN){
	                int valid = 0;
	                do {
	                    int selected = show_deck(player, cards_array, hand_index, "Pick a card", ai_pick != -1, gs_done_008b40e0);
	                    if (selected != -1 && !is_basic_land_by_id(cards_data[cards_array[selected]].id)){
	                        id = cards_data[cards_array[selected]].id;
	                        valid = 1;
	                    }
	                } while (!valid && ai_pick != -1);
	                if (!valid){ id = -1; }
	            }
	            if (id >= 0){ lobotomy_effect(player, get_card_instance(player, card)->targets[0].player, id, -1, 1); }
	        }
	    }
	    kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_lotus_petal(int player, int card, event_t event)
{
  /* Lotus Petal	|0
   * Artifact
   * |T, Sacrifice this artifact: Add one mana of any color. */

  // 0x4dd5d1
  return artifact_mana_all_one_color(player, card, event, 1, 1);
}

int card_mana_severance(int player, int card, event_t event){
  /* Mana Severance	|1|U
   * Sorcery
   * Search your library for any number of land cards, exile them, then shuffle. */

	if( event == EVENT_RESOLVE_SPELL ){
		int choice = do_dialog(player, player, card, -1, -1, " Auto mode\n Manual mode", 0);
		if( choice == 0 ){
			int count = count_deck(player)-1;
			int *deck = deck_ptr[player];
			while( count > -1 ){
					if( cards_data[deck[count]].type & TYPE_LAND ){
						rfg_card_in_deck(player, count);
					}
					count--;
			}
		}
		else if( choice == 1 ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_LAND, "Select a land card to exile.");
				while( new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test) >= 0 ){
				}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_marble_titan(int player, int card, event_t event){
  /* Marble Titan	|3|W
   * Creature - Giant 3/3
   * Creatures with power 3 or greater don't untap during their controllers' untap steps. */

	if( event == EVENT_UNTAP && current_phase == PHASE_UNTAP && ! is_humiliated(player, card, AL_STATIC) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
			get_power(affected_card_controller, affected_card) > 2 ){
			card_instance_t *instance= get_card_instance(affected_card_controller, affected_card);
			instance->untap_status &= ~3;
		}
	}

	return 0;
}

int card_meditate(int player, int card, event_t event)
{
  /* Meditate	|2|U
   * Instant
   * Draw four cards. You skip your next turn. */

	if (event == EVENT_RESOLVE_SPELL){
		draw_cards(player, 4);
		skip_next_turn(player, card, player);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int mindwhip_sliver_shared_ability(int player, int card, event_t event)
{
  int card_00;
  card_instance_t *instance;
  card_instance_t *source_in_play;
  int result;
  target_definition_t td2;

  instance = get_card_instance(player, card);
  result = instance->targets[1].player;
  if (result < 0)
  {
    return 0;
  }
  card_00 = instance->targets[1].card;
  leaves_play(result, card_00, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    add_status(player, card, 0x10000);
    in_play(result, card_00);
    is_humiliated(result, card_00, AL_NM_NL_ACT);
    return 0;
  }
  source_in_play = in_play(result, card_00);
  if (!source_in_play && event == EVENT_CAN_ACTIVATE)
    return 0;
  if (!source_in_play && event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);
  result = is_humiliated(result, card_00, AL_NM_NL_ACT);
  if (result != 0)
    return 0;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td2, 0);
  td2.zone = TARGET_ZONE_PLAYERS;
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = would_validate_arbitrary_target(&td2, instance->targets[2].player,
                                                instance->targets[2].card),
       result != 0))
  {
    discard(instance->targets[2].player, 1, player);
  }
  return shared_sliver_activated_ability(player, card, event, 0x1c, 2, 0, 0, 0, 0, 0, 0, &td2,
                                         "TARGET_PLAYER");
}

int card_mindwhip_sliver(int player, int card, event_t event){
  /* Mindwhip Sliver |2|B
   * 	  Creature - Sliver 2/2, 2B (3)
   * 	  All Slivers have "{2}, Sacrifice this permanent: Target player discards a card at random. Activate this ability only any time you could cast a sorcery." */

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &mindwhip_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &mindwhip_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}
	return slivercycling(player, card, event);
}

int card_minion_of_the_wastes(int player, int card, event_t event)
{
  /* Minion of the Wastes	|3|B|B|B
   * Creature - Minion 100/100
   * Trample
   * As ~ enters the battlefield, pay any amount of life.
   * ~'s power and toughness are each equal to the life paid as it entered the battlefield. */

  card_instance_t *instance;
  unsigned int life_payment;
  int argument3;

  if (card != -1)
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      argument3 = life[player];
      if (player == 1)
      {
        argument3 = argument3 + -7;
      }
      life_payment = choose_a_number(player, "Pay how much life?", argument3);
      if (life[player] < (int)life_payment)
      {
        life_payment = life[player] - 1;
      }
      life_payment = (int)(life_payment ^ 0xffffffff) >> 0x1f & life_payment;
      lose_life(player, life_payment);
      instance->targets[1].player = life_payment;
      return 0;
    }
    if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
        (player == affected_card_controller))
    {
      set_characteristic_pt(instance->targets[1].player);
    }
  }
  return 0;
}

int card_mirris_guile(int player, int card, event_t event){
  /* Mirri's Guile	|G
   * Enchantment
   * At the beginning of your upkeep, you may look at the top three cards of your library, then put them back in any order. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		rearrange_top_x(player, player, 3);
	}

	return global_enchantment(player, card, event);
}

int mnemonic_sliver_shared_ability(int player, int card, event_t event)
{
  card_instance_t *instance;
  unsigned int sacrifice_or_player;
  int source_card_or_result;
  int c;
  unsigned int card_00;
  test_definition_t test;

  instance = get_card_instance(player, card);
  c = instance->targets[1].player;
  if (c < 0)
  {
    return 0;
  }
  source_card_or_result = instance->targets[1].card;
  leaves_play(c, source_card_or_result, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    add_status(player, card, 0x10000);
    in_play(c, source_card_or_result);
  }
  else
  {
    instance = in_play(c, source_card_or_result);
    if (instance == NULL)
    {
      if (event == EVENT_CAN_ACTIVATE)
      {
        return 0;
      }
      if (event == EVENT_CLEANUP)
      {
        kill_card(player, card, KILL_EXILE);
        is_humiliated(c, source_card_or_result, AL_NM_NL_ACT);
        return 0;
      }
    }
  }
  if (is_humiliated(c, source_card_or_result, AL_NM_NL_ACT) == 0)
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      instance = in_play(c, source_card_or_result);
      if ((instance != NULL) && (c = 0, 0 < active_cards_count[player]))
      {
        do
        {
          instance = in_play(player, c);
          if ((instance != NULL) &&
              (((source_card_or_result = is_what(player, c, 0x47),
                 source_card_or_result != 0 &&
                     (source_card_or_result = has_subtype(player, c, SUBTYPE_SLIVER),
                      source_card_or_result != 0)) &&
                (source_card_or_result = generic_activated_ability(player, c, EVENT_CAN_ACTIVATE, 8,
                                                                   2, 0, 0, 0, 0, 0, 0, NULL, NULL),
                 source_card_or_result != 0))))
          {
            return 1;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }
    }
    else if (event == EVENT_ACTIVATE)
    {
      new_default_test_definition(&test, 0x47, "Select a Sliver to sacrifice.");
      test.subtype = 0xb1;
      sacrifice_or_player = new_sacrifice(player, card, player,
                                          SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
      if (sacrifice_or_player == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      card_00 = sacrifice_or_player >> 0x18;
      sacrifice_or_player = sacrifice_or_player >> 0x10 & 0xff;
      if (charge_mana_for_activated_ability(sacrifice_or_player, card_00, 2, 0, 0, 0, 0, 0) == 0)
      {
        state_untargettable(sacrifice_or_player, card_00, 0);
      }
      else
      {
        kill_card(sacrifice_or_player, card_00, KILL_SACRIFICE);
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      draw_cards(player, 1);
    }
  }
  return 0;
}

int card_mnemonic_sliver(int player, int card, event_t event){
  /* Mnemonic Sliver |2|U
   * 	  Creature - Sliver 2/2
   * 	  All Slivers have "{2}, Sacrifice this permanent: Draw a card." */

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &mnemonic_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &mnemonic_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}
	return slivercycling(player, card, event);
}

int card_mogg_conscripts(int player, int card, event_t event){
  /* Mogg Conscripts	|R
   * Creature - Goblin 2/2
   * This creature can't attack unless you've cast a creature spell this turn. */

	if( event == EVENT_ATTACK_LEGALITY && affect_me(player, card) && get_stormcreature_count(player) < 1 &&
		! is_humiliated(player, card, AL_STATIC) )
	{
		event_result = 1;
	}

	return 0;
}

int card_mogg_fanatic(int player, int card, event_t event){
	/* Mogg Fanatic	|R	0x200cd11
	 * Creature - Goblin 1/1
	 * Sacrifice ~: It deals 1 damage to any target. */
	/* Triskelavite	""(token)	0x200cd11
	 * Artifact Creature - Triskelavite 1/1
	 * Flying
	 * Sacrifice this creature: This creature deals 1 damage to any target. */
	// Used externally
	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
		damage_target0(player, card, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ANY");
}

int card_mogg_raider(int player, int card, event_t event){
  /* Mogg Raider	|R
   * Creature - Goblin 1/1
   * Sacrifice a Goblin: Target creature gets +1/+1 until end of turn. */

	if (event == EVENT_CHECK_ABILITIES && affected_card_controller == player && card_mogg_raider(player, card, EVENT_CAN_ACTIVATE)){
		int amount = MIN(has_mana(player, COLOR_ANY, 0), count_subtype(player, TYPE_CREATURE, SUBTYPE_GOBLIN));
		check_abilities_power += amount;
		check_abilities_toughness += amount;
	}
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Goblin to sacrifice.");
	test.subtype = SUBTYPE_GOBLIN;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE",
															0, &test, 0);
}

int card_mongrel_pack(int player, int card, event_t event){
	/* Mongrel Pack	|3|G
	 * Creature - Hound 4/1
	 * When ~ dies during combat, put four 1/1 |Sgreen Hound creature tokens onto the battlefield. */

	if (current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2 && this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY)){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_HOUND, &token);
		token.qty = 4;
		token.pow = 1;
		token.tou = 1;
		token.color_forced = COLOR_TEST_GREEN;
		generate_token(&token);
	}

	return 0;
}

int card_mounted_archers(int player, int card, event_t event)
{
  /* Mounted Archers	|3|W
   * Creature - Human Soldier Archer 2/3
   * Reach (This creature can block creatures with flying.)
   * |W: This creature can block an additional creature this turn. */

	if (event == EVENT_RESOLVE_ACTIVATION)
	{
		card_instance_t* instance = get_card_instance(player, card);
		can_block_additional_until_eot(instance->parent_controller, instance->parent_card,
										instance->parent_controller, instance->parent_card, 1);
	}

  return generic_activated_ability(player, card, event, GAA_NONE, MANACOST_W(1), 0, NULL, NULL);
}

int card_muscle_sliver(int player, int card, event_t event){
  /* Muscle Sliver	|1|G
   * Creature - Sliver 1/1
   * All Sliver creatures get +1/+1. */

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 1, 1, 0, BCT_INCLUDE_SELF);
	return slivercycling(player, card, event);
}

int card_natures_revolt(int player, int card, event_t event){
  /* Nature's Revolt	|3|G|G
   * Enchantment
   * All lands are 2/2 creatures that are still lands. */

	if( event == EVENT_CHANGE_TYPE ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);

		global_type_change(player, card, event, ANYBODY, TYPE_CREATURE, &this_test, 2, 2, 0, 0, 0);
	}

	return global_enchantment(player, card, event);
}

int card_orims_prayer(int player, int card, event_t event)
{
  /* Orim's Prayer	|1|W|W
   * Enchantment
   * Whenever one or more creatures attack you, you gain 1 life for each attacking creature. */

  store_attackers(player, card, event, DAT_ATTACKS_PLAYER, 1-player, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
    gain_life(player, get_card_instance(player, card)->info_slot);
  return global_enchantment(player, card, event);
}

int card_orim_samite_healer(int player, int card, event_t event){
	/* Orim, Samite Healer	|1|W|W	0x20034dc
	 * Legendary Creature - Human Cleric 1/3
	 * |T: Prevent the next 3 damage that would be dealt to any target this turn. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	card_instance_t *instance = get_card_instance(player, card);

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 3);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST0, 0,
									&td, "TARGET_DAMAGE");
}

int card_overrun(int player, int card, event_t event){
  /* Overrun	|2|G|G|G
   * Sorcery
   * Creatures you control get +3/+3 and gain trample until end of turn. (Each of those creatures can deal excess combat damage to the player or planeswalker it's attacking.) */

	if(event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 3, 3, KEYWORD_TRAMPLE, 0);
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_patchwork_gnomes(int player, int card, event_t event){
  /* Patchwork Gnomes	|3
   * Artifact Creature - Gnome 2/1
   * Discard a card: Regenerate this creature. (The next time this creature would be destroyed this turn, instead tap it, remove it from combat, and heal all damage on it.) */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( can_regenerate(player, instance->parent_card) ){
			regenerate_target(player, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD | GAA_REGENERATION, MANACOST0, 0, NULL, NULL);
}

int card_pearl_medallion(int player, int card, event_t event){
/* CARD_ID_PEARL_MEDALLION	3225
Pearl Medallion	|2
Artifact
|SWhite spells you cast cost |1 less to cast. */

	return tempest_medallion(player, card, event, get_sleighted_color_test(player, card, COLOR_TEST_WHITE));
}

int card_pegasus_refuge(int player, int card, event_t event){
	/* Pegasus Refuge	|3|W
	 * Enchantment
	 * |2, Discard a card: Put a 1/1 |Swhite Pegasus creature token with flying onto the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_PEGASUS, &token);
		token.pow = token.tou = 1;
		token.color_forced = COLOR_TEST_WHITE;
		token.key_plus = KEYWORD_FLYING;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST_X(2), 0, NULL, NULL);
}

int card_perish(int player, int card, event_t event){
  /* Perish	|2|B
   * Sorcery
   * Destroy all green creatures. They can't be regenerated. */

	if(event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		new_manipulate_all(player, card, ANYBODY, &this_test, KILL_BURY);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_precognition(int player, int card, event_t event){
  /* Precognition	|4|U
   * Enchantment
   * At the beginning of your upkeep, you may look at the top card of target opponent's library. If you do, you may put that card on the bottom of that player's library. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;

		if( would_validate_arbitrary_target(&td, 1-player, -1) ){
			int *deck = deck_ptr[1-player];
			if( deck[0] != -1 ){
				int selected = -1;
				if( player == HUMAN ){
					selected = show_deck( player, deck, 1, "Choose a card to put on the bottom", 0, gs_done_008b40e0 );
				}
				else{
					 selected = 0;
				}
				if( selected != -1 ){
					deck[ count_deck(player) ] = deck[0];
					remove_card_from_deck( player, 0 );
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_propaganda(int player, int card, event_t event){
  /* Propaganda	|2|U
   * Enchantment
   * Creatures can't attack you unless their controller pays |2 for each creature they control that's attacking you. */

	tax_attack(player, card, event);
	return global_enchantment(player, card, event);
}

int card_puppet_strings(int player, int card, event_t event){
  /* Puppet Strings	|3
   * Artifact
   * |2, |T: You may tap or untap target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_CREATURE");
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

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_CREATURE");
}

int card_rathi_dragon(int player, int card, event_t event){
  /* Rathi Dragon	|2|R|R
   * Creature - Dragon 5/5
   * Flying (This creature can't be blocked except by creatures with flying or reach.)
   * When this creature enters, sacrifice it unless you sacrifice two Mountains. */

	if( player == AI && event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( count_subtype(player, TYPE_LAND, SUBTYPE_MOUNTAIN) < 2 ){
			ai_modifier-=500;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a Mountain to sacrifice.");
		test.subtype = SUBTYPE_MOUNTAIN;
		int sac[2] = {-1, -1};
		int i;
		for(i=0; i<2; i++){
			sac[i] = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			if (!sac[i]){
				break;
			}
		}
		if( !sac[0] || ! sac[1] ){
			for(i=0; i<2; i++){
				if( sac[i] ){
					state_untargettable(BYTE2(sac[i]), BYTE3(sac[i]), 0);
				}
			}
			kill_card(player, card, KILL_SACRIFICE);
		}
		else{
			for(i=0; i<2; i++){
				kill_card(BYTE2(sac[i]), BYTE3(sac[i]), KILL_SACRIFICE);
			}
		}
	}

	return 0;
}

int card_reanimate(int player, int card, event_t event){
  /* Reanimate	|B
   * Sorcery
   * Put target creature card from a graveyard onto the battlefield under your control. You lose life equal to that card's mana value. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
	this_test.ai_selection_mode = AI_MAX_CMC;
	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
		if (spell_fizzled != 1){
			card_instance_t* instance = get_card_instance(player, card);
			const int* grave = get_grave(instance->targets[0].player);
			int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 0);
			if (selected >= 0)
				ai_modifier += cards_ptr[cards_data[grave[selected]].id]->ai_base_value;
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			int zombo = reanimate_permanent(player, card, instance->targets[0].player, selected, REANIMATE_DEFAULT);
			lose_life(player, get_cmc(player, zombo));
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
}

int card_reckless_spite(int player, int card, event_t event){
  /* Reckless Spite	|1|B|B
   * Instant
   * Destroy two target nonblack creatures. You lose 5 life. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		int resolved = 0;
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
				resolved++;
			}
		}
		if( resolved > 0 ){
			lose_life(player, 5);
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 2, NULL);
}

int card_recycle(int player, int card, event_t event)
{
  /* Recycle	|4|G|G
   * Enchantment
   * Skip your draw step.
   * Whenever you play a card, draw a card.
   * Your maximum hand size is two. */

  int result;
  result = is_humiliated(player, card, AL_STATIC);
  if (result == 0)
  {
    skip_your_draw_step(player, event);
    if ((current_turn == player) && (event == EVENT_MAX_HAND_SIZE))
    {
      event_result = 2;
    }
  }
  result = is_humiliated(player, card, AL_TRIG);
  if ((((result == 0) && (trigger_condition == TRIGGER_SPELL_CAST)) && (card == affected_card)) &&
      (((player == affected_card_controller && (trigger_cause_controller == player)) &&
        (reason_for_trigger_controller == player))))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      draw_a_card(player);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

int card_reflecting_pool(int player, int card, event_t event){
  /* Reflecting Pool	""
   * Land
   * |T: Add one mana of any type that a land you control could produce. */

	if( event == EVENT_RESOLVE_SPELL ){
		int colors = get_color_of_mana_produced_by_id(get_id(player, card), COLOR_TEST_ANY_COLORED, player);
		if (colors < 0){
			colors = 0;
		}
		play_land_sound_effect_force_color(player, card, colors);
		return 0;	// so mana_producer() doesn't play the colorless sound
	}

	return mana_producer(player, card, event);
}

int card_repentance(int player, int card, event_t event){
  /* Repentance	|2|W
   * Sorcery
   * Target creature deals damage to itself equal to its power. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card,
							get_power(instance->targets[0].player, instance->targets[0].card),
							instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_respite(int player, int card, event_t event){
  /* Respite	|1|G
   * Instant
   * Prevent all combat damage that would be dealt this turn. You gain 1 life for each attacking creature. */

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, count_attackers(current_turn));
		fog_effect(player, card);
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return card_fog(player, card, event);
}

int card_rolling_thunder(int player, int card, event_t event){
	/* Rolling Thunder	|X|R|R	0x2003504
	 * Sorcery
	 * ~ deals X damage divided as you choose among any number of targets. */
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td.allow_cancel = 3;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if ((event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CHANGE_TARGET){
		set_special_flags2(player, card, SF2_X_SPELL);
		if( (played_for_free(player, card) || is_token(player, card)) && ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			return 0;
		}
		instance->number_of_targets = 0;
		int trgs = 0;
		while( 1 ){
				if( check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
					if( trgs >= instance->info_slot ){
						break;
					}
				}
				else{
					if( ! has_mana(player, COLOR_COLORLESS, trgs+1) ){
						break;
					}
				}
				if( new_pick_target(&td, "TARGET_ANY", trgs, 0) ){
					if( instance->targets[trgs].card != -1 ){
						add_state(instance->targets[trgs].player, instance->targets[trgs].card, STATE_TARGETTED);
					}
					trgs++;
				}
				else{
					break;
				}
		}
		if( ! trgs ){
			spell_fizzled = 1;
			return 0;
		}
		int i;
		for(i=0; i<trgs; i++){
			if( instance->targets[i].card != -1 ){
				remove_state(instance->targets[i].player, instance->targets[i].card, STATE_TARGETTED);
			}
		}
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			charge_mana(player, COLOR_COLORLESS, trgs);
			if( spell_fizzled != 1 ){
				instance->info_slot = trgs;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		divide_damage(player, card, &td);
		kill_card(player, card, KILL_DESTROY);
	}

   return 0;
}

int card_root_maze(int player, int card, event_t event){

	/* Root Maze	|G
	 * Enchantment
	 * Artifacts and lands enter the battlefield tapped. */

	permanents_enters_battlefield_tapped(player, card, event, ANYBODY, TYPE_ARTIFACT | TYPE_LAND, NULL);

	return global_enchantment(player, card, event);
}

int card_ruby_medallion(int player, int card, event_t event){
/* CARD_ID_RUBY_MEDALLION	3256
Ruby Medallion	|2
Artifact
|SRed spells you cast cost |1 less to cast. */

	return tempest_medallion(player, card, event, get_sleighted_color_test(player, card, COLOR_TEST_RED));
}

/* ManalinkEh 2022: 0x21546c0; original cards/tempest.c:4205. */
int card_sacred_guide(int player, int card, event_t event)
{
  /* Sacred Guide	|W
   * Creature - Human Cleric 1/1
   * |1|W, Sacrifice this creature: Reveal cards from the top of your library until you reveal a white card. Put that card into your hand and exile all other cards revealed this way. */

  int result;
  test_definition_t this_test;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    default_test_definition(&this_test, 0xff);
    this_test.color = get_sleighted_color_test(player, card, 0x20);
    reveal_card_until_you_reveal_test(player, player, &this_test, 0x28);
  }
  result = generic_activated_ability(player, card, event, 8, 1, 0, 0, 0, 0, 1, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x21484a0; original cards/tempest.c:4222. */
static int sadistic_glee_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  card_instance_t *aura_or_attached;
  unsigned int pending_deaths;
  card_instance_t *dying;
  int result;
  int target_player;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (result = get_id((int)instance->damage_target_player, instance->damage_target_card),
           result != 0xcba))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (result = effect_follows_control_of_attachment(player, card, 0x4700), result != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (result = effect_follows_control_of_attachment(player, card, event), result != 0))
    {
      return 0;
    }
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {

      instance = get_card_instance(player, card);
      dying = in_play(affected_card_controller, affected_card);
      if ((dying != NULL) &&
          (result = is_this_dying(affected_card_controller, affected_card), result != 0))
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
        count_for_gfp_ability(player, card, 0x77, 2, 2, NULL);
      }
    }
  }
  pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_deaths != 0)
  {
    result = check_status(player, card, 0x10000);
    if (result == 0)
    {

      instance = get_card_instance(player, card);
      aura_or_attached = get_card_instance(instance->targets[0].player, instance->targets[0].card);
      target_player = (int)aura_or_attached->damage_target_player;
      result = aura_or_attached->damage_target_card;
      if (((-1 < target_player) && (-1 < result)) &&
          (aura_or_attached = in_play(target_player, result), aura_or_attached != NULL))
      {

        for (pending_deaths = pending_deaths & 0xff; pending_deaths != 0;
             pending_deaths = pending_deaths - 1)
        {
          add_counters(player, card, target_player, result, COUNTER_P1_P1, 1);
        }
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) && (result = check_status(player, card, 0x10000), result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

int card_sadistic_glee(int player, int card, event_t event){
  /* Sadistic Glee	|B
   * Enchantment - Aura
   * Enchant creature
   * Whenever a creature dies, put a +1/+1 counter on enchanted creature. */

	if (event == EVENT_STATIC_EFFECTS && affect_me(player, card)){
        int found = 0;
        for (int c = 0; c < active_cards_count[player]; ++c){
            if (in_play(player, c) && is_what(player, c, TYPE_EFFECT)){
                card_instance_t* legacy = get_card_instance(player, c);
                if (legacy->info_slot == (intptr_t)sadistic_glee_effect
                    && ((legacy->damage_target_player == player && legacy->damage_target_card == card)
                        || (legacy->targets[0].player == player && legacy->targets[0].card == card))){
                    found = 1;
                    break;
                }
            }
        }
        if (!found){
            int leg = create_targetted_legacy_effect(player, card, sadistic_glee_effect, player, card);
            card_instance_t* legacy = get_card_instance(player, leg);
            legacy->targets[0].player = player;
            legacy->targets[0].card = card;
            legacy->number_of_targets = 1;
            add_status(player, leg, STATUS_INVISIBLE_FX);
        }
    }

	if( ! is_humiliated(player, card, AL_TRIG) && get_card_instance(player, card)->damage_target_player > -1 ){
		if( event == EVENT_GRAVEYARD_FROM_PLAY ){
			count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, 0);
		}
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( result && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		for (int i = BYTE0(result); i > 0; --i){
			add_counters(player, card, instance->damage_target_player, instance->damage_target_card, COUNTER_P1_P1, 1);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, player);
}

int card_safeguard(int player, int card, event_t event){
  /* Safeguard	|3|W|W
   * Enchantment
   * |2|W: Prevent all combat damage that would be dealt by target creature this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			negate_combat_damage_this_turn(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XW(2, 1), 0, &td, "TARGET_CREATURE");
}

int card_sapphire_medallion(int player, int card, event_t event){
/* CARD_ID_SAPPHIRE_MEDALLION	3261
Sapphire Medallion	|2
Artifact
|SBlue spells you cast cost |1 less to cast. */

	return tempest_medallion(player, card, event, get_sleighted_color_test(player, card, COLOR_TEST_BLUE));
}

int card_scalding_tongs(int player, int card, event_t event){
	/* Scalding Tongs	|2	0x2003531
	 * Artifact
	 * At the beginning of your upkeep, if you have three or fewer cards in hand, ~ deals 1 damage to target opponent or planeswalker. */
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY && hand_count[player] <= 3 ){
		target_definition_t td_opw;
		opponent_or_pw_target_definition(player, card, &td_opw);
		if (can_target(&td_opw) && pick_target(&td_opw, "TARGET_OPPONENT_OR_PLANESWALKER")){
			damage_target0(player, card, 1);
			get_card_instance(player, card)->number_of_targets = 0;
		}
	}

	return 0;
}

int card_sarcomancy (int player, int card, event_t event){
	/* Sarcomancy	|B
	 * Enchantment
	 * When ~ enters the battlefield, put a 2/2 |Sblack Zombie creature token onto the battlefield.
	 * At the beginning of your upkeep, if there are no Zombies on the battlefield, ~ deals 1 damage to you. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_token_by_id(player, card, CARD_ID_ZOMBIE);
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( ! count_subtype(player, TYPE_PERMANENT, SUBTYPE_ZOMBIE) ){
			damage_player(player, 1, player, card);
		}
	}

	return global_enchantment(player, card, event);
}

int card_scorched_earth(int player, int card, event_t event){
  /* Scorched Earth	|X|R
   * Sorcery
   * As an additional cost to cast this spell, discard X land cards.
   * Destroy X target lands. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_LAND, "Select a land card to discard.");
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return check_battlefield_for_special_card(player, card, player, 0, &this_test);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		set_special_flags2(player, card, SF2_X_SPELL);
		instance->number_of_targets = 0;
		if( (played_for_free(player, card) || is_token(player, card)) && ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			return 0;
		}
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test);
		}
		int trgs = 0;
		while( trgs < instance->info_slot && can_target(&td) ){
				if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) && ! has_mana(player, COLOR_COLORLESS, trgs+1) ){
					break;
				}
				if( new_pick_target(&td, "TARGET_LAND", trgs, 0) ){
					state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
					trgs++;
				}
				else{
					break;
				}
		}
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
		}
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int original_hand[2][hand_count[player]];
			int ohc = 0;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_hand(player, i) ){
					original_hand[0][ohc] = get_original_internal_card_id(player, i);
					original_hand[1][ohc] = i;
					ohc++;
				}
			}
			int to_discard[hand_count[player]];
			int tdc = 0;
			while( tdc < instance->number_of_targets ){
					int selected = select_card_from_zone(player, player, original_hand[0], ohc, 0, AI_MIN_VALUE, -1, &this_test);
					if( selected != -1 ){
						to_discard[tdc] = original_hand[1][selected];
						tdc++;
						for(i=selected; i<ohc; i++){
							original_hand[0][i] = original_hand[0][i+1];
							original_hand[1][i] = original_hand[1][i+1];
						}
						ohc--;
					}
					else{
						break;
					}
			}
			if( tdc < instance->number_of_targets ){
				spell_fizzled = 1;
				return 0;
			}
			charge_mana(player, COLOR_COLORLESS, instance->number_of_targets);
			if( spell_fizzled != 1 ){
				for(i=0; i<instance->number_of_targets; i++){
					discard_card(player, to_discard[i]);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_scroll_rack(int player, int card, event_t event){
  /* Scroll Rack	|2
   * Artifact
   * |1, |T: Exile any number of cards from your hand face down. Put that many cards from the top of your library into your hand. Then look at the exiled cards and put them on top of your library in any order. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		char msg[100] = "Select a card to put on top.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, msg);

		int my_hand[100];
		int mhc = 0;
		while( hand_count[player] > 0 ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
				if( selected != -1 ){
					card_instance_t *this = get_card_instance( player, selected );
					my_hand[mhc] = this->internal_card_id;
					mhc++;
					obliterate_card(player, selected);
				}
				else{
					break;
				}
		}
		if( mhc > 0 ){
			draw_cards(player, mhc);
			int i;
			for(i = 0; i < mhc; i++){
				int card_added = add_card_to_hand(player, my_hand[i]);
				put_on_top_of_deck(player, card_added);
			}
			rearrange_top_x(player, player, mhc);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL);
}

int card_searing_touch(int player, int card, event_t event){
	/* Searing Touch	|R	0x200a0c0
	 * Instant
	 * Buyback |4
	 * ~ deals 1 damage to any target. */
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 ){
			instance->info_slot = buyback(player, card, MANACOST_X(4));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_seeker_of_skybreak(int player, int card, event_t event){
  /* Seeker of Skybreak	|1|G
   * Creature - Elf 2/1
   * |T: Untap target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			ai_mod_for_untap_effects(player, card, 0, 0);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_selenia_dark_angel(int player, int card, event_t event){
  /* Selenia, Dark Angel	|3|W|B
   * Legendary Creature - Phyrexian Angel 3/3
   * Flying
   * Pay 2 life: Return Selenia to its owner's hand. */

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(instance->parent_controller, instance->parent_card) ){
			bounce_permanent(instance->parent_controller, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, 2, NULL, NULL);
}

int card_serene_offering(int player, int card, event_t event){
  /* Serene Offering	|1|W
   * Instant
   * Destroy target enchantment. You gain life equal to its mana value. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = get_cmc(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			gain_life(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT", 1, NULL);
}

int card_shadow_rift(int player, int card, event_t event){
	/* Shadow Rift	|U
	 * Instant
	 * Target creature gains shadow until end of turn.
	 * Draw a card. */
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;
	return vanilla_pump(player, card, event, &td, 0, 0, VANILLA_PUMP_DRAW_A_CARD, SP_KEYWORD_SHADOW);
}

int card_shocker(int player, int card, event_t event){
/*CARD_ID_SHOCKER	3277
Shocker	|1|R
Creature - Insect 1/1
Whenever ~ deals damage to a player, that player discards all the cards in his or her hand, then draws that many cards.*/


	int result = whenever_i_deal_damage_to_a_player(player, card, event, 0);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					while( times_damaged[p] ){
							int amount = hand_count[p];
							new_discard_all(p, player);
							draw_cards(p, amount);
							times_damaged[p]--;
					}
				}
		);
	}

	return 0;
}

int card_skyshroud_ranger(int player, int card, event_t event){
  /* Skyshroud Ranger	|G
   * Creature - Elf Ranger 1/1
   * |T: You may put a land card from your hand onto the battlefield. Activate only as a sorcery. */

	// |T: You may put a land card from your hand onto the battlefield. Activate this ability only any time you could cast a sorcery.
	if (event == EVENT_RESOLVE_ACTIVATION){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a land to put into play.");
		new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_SORCERY_BE_PLAYED, MANACOST_X(0), 0, NULL, NULL);
}

int card_skyshroud_elves(int player, int card, event_t event){
  /* Skyshroud Elf	|1|G
   * Creature - Elf Druid 1/1
   * |T: Add |G.
   * |1: Add |R or |W. */

	if (event == EVENT_COUNT_MANA){
	    if (affect_me(player, card) && can_produce_mana(player, card)){
	        if (!is_tapped(player, card) && !is_animated_and_sick(player, card) && can_produce_mana(player, card)){
	            declare_mana_available(player, COLOR_GREEN, 1);
	        }
	        int amount = has_mana(player, COLOR_ANY, 1);
	        if (amount > 0){
	            declare_mana_available(player, COLOR_COLORLESS, -amount);
	            declare_mana_available_hex(player, COLOR_TEST_RED | COLOR_TEST_WHITE, amount);
	        }
	    }
	} else if ((event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE) && can_produce_mana(player, card)){
	    int can_produce_g = !is_tapped(player, card) && !is_sick(player, card);
	    card_instance_t* instance = get_card_instance(player, card);
	    int can_produce_rw = has_mana(player, COLOR_ANY, 1);
	    if (event == EVENT_CAN_ACTIVATE){ return can_produce_g || can_produce_rw; }
	    if (can_produce_rw){
	        int colors = COLOR_TEST_RED | COLOR_TEST_WHITE;
	        if (can_produce_g){ colors |= COLOR_TEST_GREEN; }
	        int choice = choose_mana_all_one_color(player, colors, 1, "What kind of mana?", COLOR_TEST_COLORLESS);
	        if (choice == -1){ spell_fizzled = 1; return 0; }
	        if (choice != COLOR_GREEN){
	            instance->info_slot = 1;
	            charge_mana(player, COLOR_COLORLESS, 1);
	            instance->info_slot = 0;
	            if (spell_fizzled == 1){ return 0; }
	            produce_mana(player, choice, 1);
	            tapped_for_mana_color = -2;
	            return 0;
	        }
	    }
	    produce_mana_tapped(player, card, COLOR_GREEN, 1);
	}
	return 0;
}

int card_skyshroud_vampire(int player, int card, event_t event){
  /* Skyshroud Vampire	|3|B|B
   * Creature - Vampire 3/3
   * Flying
   * Discard a creature card: This creature gets +2/+2 until end of turn. */

	if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.zone = TARGET_ZONE_HAND;
		return generic_shade_amt_can_pump(player, card, 2, 0, MANACOST0,
										check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test));
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			return check_battlefield_for_special_card(player, card, player, 0, &this_test);
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
			if( selected != -1 ){
				discard_card(player, selected);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 2, 2);
	}


	return 0;
}

int card_soltari_crusader(int player, int card, event_t event)
{
  /* Soltari Crusader	|2|W
   * Creature - Soltari Knight 2/1
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * |1|W: This creature gets +1/+0 until end of turn. */

  shadow(player, card, event);
  if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
    return generic_shade(player, card, event, 0, MANACOST_XW(1, 1), 1, 0, 0, 0);
  return generic_shade_merge_pt(player, card, event, 0, MANACOST_XW(1, 1), 1, 0);
}

int card_soltari_emissary(int player, int card, event_t event){
  /* Soltari Emissary	|1|W
   * Creature - Soltari Soldier 2/1
   * |W: This creature gains shadow until end of turn. (It can block or be blocked by only creatures with shadow.) */

	return generic_shade(player, card, event, 0, MANACOST_W(1), 0, 0, 0, SP_KEYWORD_SHADOW);
}

int soltari_guerrillas_effect(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	card_instance_t* damage = combat_damage_being_prevented(event);
	if( damage && instance->targets[1].player > -1 &&
		damage->damage_source_player == instance->damage_target_player && damage->damage_source_card == instance->damage_target_card &&
		damage->damage_target_card == -1 && !check_special_flags(damage->damage_source_player, damage->damage_source_card, SF_ATTACKING_PWALKER) &&
		damage->damage_target_player != player &&
		in_play(instance->targets[0].player, instance->targets[0].card)
	  ){
		int dam = damage->info_slot;
		damage->info_slot = 0;
		damage_creature(instance->targets[1].player, instance->targets[1].card, dam, instance->damage_target_player, instance->damage_target_card);
		kill_card(player, card, KILL_EXILE);
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_soltari_guerrillas(int player, int card, event_t event)
{
  /* Soltari Guerrillas	|2|R|W
   * Creature - Soltari Soldier 3/2
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * |0: The next time this creature would deal combat damage to an opponent this turn, it deals that damage to target creature instead. */

  int32_t target_player;
  card_instance_t *instance;
  card_instance_t *redirect_effect;
  target_definition_t td;

  shadow(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    if (is_humiliated(player, card, AL_NM_NL_ACT) != 0)
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (is_attacking(player, card) == 0)
      {
        return 0;
      }
      if (is_unblocked(player, card) == 0)
      {
        return 0;
      }
      if (current_phase != 0x18)
      {
        return 0;
      }
      if (instance->info_slot == 0x42)
      {
        return 0;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 0, 0, 0, 0, 0, 0, 0,
                                       &td, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 4, 0, 0, 0, 0, 0, 0, 0, &td, NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      instance->info_slot = 0x42;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    if (is_humiliated(player, card, AL_NM_NL_ACT) != 0)
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    redirect_effect = get_card_instance(
        player, create_targetted_legacy_effect(instance->parent_controller, instance->parent_card,
                                               soltari_guerrillas_effect,
                                               instance->parent_controller, instance->parent_card));
    target_player = instance->targets[0].player;
    redirect_effect->targets[1].card = instance->targets[0].card;
    redirect_effect->targets[1].player = target_player;
    redirect_effect->number_of_targets = '\x02';
  }
  else if (event == EVENT_CLEANUP)
  {
    instance->info_slot = 0;
    return 0;
  }
  return 0;
}

int card_soltari_lancer(int player, int card, event_t event){
  /* Soltari Lancer	|2|W
   * Creature - Soltari Knight 2/2
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * This creature has first strike as long as it's attacking. */

	shadow(player, card, event);
	if( event == EVENT_ABILITIES && affect_me(player, card) && is_attacking(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		event_result |= KEYWORD_FIRST_STRIKE;
	}
	return 0;
}

int card_soltari_trooper(int player, int card, event_t event)
{
	/* Soltari Trooper	|1|W
	 * Creature - Soltari Soldier 1/1
	 * Shadow
	 * Whenever ~ attacks, it gets +1/+1 until end of turn. */

	shadow(player, card, event);

	return when_attacks_pump_self(player, card, event, 1,1);
}

int card_spike_drone(int player, int card, event_t event){
  /* Spike Drone	|G
   * Creature - Spike Drone 0/0
   * This creature enters with a +1/+1 counter on it.
   * |2, Remove a +1/+1 counter from this creature: Put a +1/+1 counter on target creature. */

	return generic_spike(player, card, event, 1);
}

int card_spirit_mirror(int player, int card, event_t event){
	/* Spirit Mirror	|2|W|W
	 * Enchantment
	 * At the beginning of your upkeep, if there are no Reflection tokens on the battlefield, put a 2/2 |Swhite Reflection creature token onto the battlefield.
	 * |0: Destroy target Reflection. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.type_flag = F1_IS_TOKEN;
		this_test.subtype = SUBTYPE_REFLECTION;

		if( ! check_battlefield_for_special_card(player, card, ANYBODY, 0, &this_test) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_REFLECTION, &token);
			token.pow = token.tou = 2;
			token.color_forced = COLOR_TEST_WHITE;
			generate_token(&token);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.required_subtype = SUBTYPE_REFLECTION;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target Reflection.");
}

int card_starke_of_rath(int player, int card, event_t event)
{
  /* Starke of Rath	|1|R|R
   * Legendary Creature - Human Rogue 2/2
   * |T: Destroy target artifact or creature. That permanent's controller gains control of ~. */

  check_legend_rule(player, card, event);

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_CREATURE);

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int pp = instance->parent_controller, pc = instance->parent_card;

	  kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);

	  if (pp != instance->targets[0].player)
		give_control_of_self(pp, pc);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "PUTREFY");	// Select target artifact or creature
}

static const char* can_be_untapped(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
	if( ! check_special_flags2(player, card, SF2_COULD_NOT_UNTAP) ){
		return NULL;
	}
	return "this permanent couldn't be untapped";
}

int card_static_orb(int player, int card, event_t event){
  /* Static Orb	|3
   * Artifact
   * As long as this artifact is untapped, players can't untap more than two permanents during their untap steps. */

	// Used externally
	card_instance_t *instance = get_card_instance(player, card);

	if( ! is_tapped(player, card) && current_phase == PHASE_UNTAP && event == EVENT_UNTAP && ! is_humiliated(player, card, AL_STATIC) ){
		// This block avoid that multiple Static Orbs allow to untap more than 2 permanentes per untap
		int i;
		for(i=0; i<2; i++){
			int count = active_cards_count[i]-1;
			while( count > -1 ){
					if( in_play(i, count) && get_id(i, count) == get_id(player, card) && !(i == player && count == card) ){
						get_card_instance(i, count)->info_slot = 2;
					}
					count--;
			}
		}

		if( instance->info_slot == 0 ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_PERMANENT);
			td.illegal_type = TYPE_ENCHANTMENT | TARGET_TYPE_PLANESWALKER;
			td.allowed_controller = current_turn;
			td.preferred_controller = current_turn;
			td.required_state = TARGET_STATE_TAPPED;
			td.who_chooses = current_turn;
			td.illegal_abilities = 0;
			td.allow_cancel = 0;
			td.extra = target_predicate_extra(can_be_untapped);
			td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
			while( instance->info_slot < 2 && can_target(&td) ){
					if( select_target(player, card, &td, "Select a permanent to untap", &(instance->targets[0])) ){
						instance->number_of_targets = 1;
						untap_card(instance->targets[0].player, instance->targets[0].card);
						instance->info_slot++;
					}
			}
		}

		get_card_instance(affected_card_controller, affected_card)->untap_status &= ~3;
	}

	if( event == EVENT_CLEANUP ){
		instance->info_slot = 0;
	}

	return 0;
}

int card_staunch_defenders(int player, int card, event_t event){
  /* Staunch Defenders	|3|W|W
   * Creature - Human Soldier 3/4
   * When this creature enters, you gain 4 life. */

	// original code : 0x413EE0

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 4);
	}

	return 0;
}

int card_storm_front(int player, int card, event_t event){
  /* Storm Front	|G
   * Enchantment
   * |G|G: Tap target creature with flying. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_G(2), 0,
									&td, "Select target creature with flying.");
}

int card_talon_sliver(int player, int card, event_t event){
  /* Talon Sliver	|1|W
   * Creature - Sliver 1/1
   * All Sliver creatures have first strike. */

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 0, 0, KEYWORD_FIRST_STRIKE, BCT_INCLUDE_SELF);
	return slivercycling(player, card, event);
}

int card_thalakos_seer(int player, int card, event_t event)
{
  /* Thalakos Seer	|U|U
   * Creature - Thalakos Wizard 1/1
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * When this creature leaves the battlefield, draw a card. */

  shadow(player, card, event);

  leaves_play(player, card, event, 1, 1);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
    draw_cards(player, 1);
  return 0;
}

int card_thumbscrew(int player, int card, event_t event){
	/* Thumbscrews	|2	0x2003572
	 * Artifact
	 * At the beginning of your upkeep, if you have five or more cards in hand, ~ deals 1 damage to target opponent or planeswalker. */
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY && hand_count[player] >= 5 ){
		target_definition_t td_opw;
		opponent_or_pw_target_definition(player, card, &td_opw);
		if (can_target(&td_opw) && pick_target(&td_opw, "TARGET_OPPONENT_OR_PLANESWALKER")){
			damage_target0(player, card, 1);
			get_card_instance(player, card)->number_of_targets = 0;
		}
	}

	return 0;
}

int card_time_warp(int player, int card, event_t event)
{
	/* Time Warp	|3|U|U
	 * Sorcery
	 * Target player takes an extra turn after this one. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 96;
	}

	if (event == EVENT_RESOLVE_SPELL){
		card_instance_t* instance = get_card_instance(player, card);
		if (valid_target(&td)){
			if( instance->targets[0].player == player ){
				time_walk_effect(player, card);
			}
			else{
				int fake = add_card_to_hand(instance->targets[0].player, get_original_internal_card_id(player, card));
				time_walk_effect(instance->targets[0].player, fake);
				obliterate_card(instance->targets[0].player, fake);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_tooth_and_claw(int player, int card, event_t event){
	/* Tooth and Claw	|3|R
	 * Enchantment
	 * Sacrifice two creatures: Put a 3/1 |Sred Beast creature token named Carnivore onto the battlefield. */

	if( event == EVENT_CAN_ACTIVATE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.qty = 2;
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
	}

	if( event == EVENT_ACTIVATE ){
		int sacs[2] = {-1};
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int i;
		for(i=0; i<2; i++){
			sacs[i] = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			if (!sacs[i]){
				cancel = 1;
				break;
			}
		}
		for(i=0; i<2; i++){
			if( sacs[i] > -1 ){
				state_untargettable(BYTE2(sacs[i]), BYTE3(sacs[i]), 0);
			}
		}
		generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			kill_card(BYTE2(sacs[0]), BYTE3(sacs[0]), KILL_SACRIFICE);
			kill_card(BYTE2(sacs[1]), BYTE3(sacs[1]), KILL_SACRIFICE);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_CARNIVORE);
	}

	return global_enchantment(player, card, event);
}

int card_torture_chamber(int player, int card, event_t event)
{
  /* Torture Chamber	|3
   * Artifact
   * At the beginning of your upkeep, put a pain counter on ~.
   * At the beginning of your end step, ~ deals damage to you equal to the number of pain counters on it.
   * |1, |T, Remove all pain counters from ~: ~ deals damage to target creature equal to the number of pain counters removed this way. */

  int result;
  card_instance_t *instance;
  target_definition_t *creature_target;
  target_definition_t td;

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    add_counters(player, card, player, card, COUNTER_PAIN, 1);
  result = count_counters(player, card, COUNTER_PAIN);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY && result < 1)
    return global_enchantment(player, card, event);
  if (result > 0 && current_turn == player && eot_trigger(player, card, event))
  {
    result = count_counters(player, card, COUNTER_PAIN);
    damage_player(player, result, player, card);
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 1, 0, 0, 0, 0, 0, 0,
                                       &td, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      result = charge_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0);
      if (result == 0)
      {
        return 0;
      }
      result = pick_target(&td, "TARGET_CREATURE");
      if (result == 0)
      {
        return 0;
      }
      result = count_counters(player, card, COUNTER_PAIN);
      instance->info_slot = result;
      remove_counters(player, card, player, card, COUNTER_PAIN, result);
      tap_card(player, card);
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return global_enchantment(player, card, event);
    }
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
  }
  creature_target = &td;
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(creature_target), result != 0))
  {
    damage_target0(player, card, instance->info_slot);
    return 0;
  }
  return 0;
}

int card_tradewind_rider(int player, int card, event_t event){
  /* Tradewind Rider	|3|U
   * Creature - Spirit 1/4
   * Flying
   * |T, Tap two untapped creatures you control: Return target permanent to its owner's hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_abilities = 0;
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;
	td.special = TARGET_SPECIAL_NOT_ME;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, &td1, NULL)  ){
			return target_available(player, card, &td) > 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int tapped[2] = {-1, -1};
			int tc = 0;
			while( tc < 2 ){
				if( select_target(player, card, &td, "Select a creature to tap.", &(instance->targets[tc])) ){
					state_untargettable(instance->targets[tc].player, instance->targets[tc].card, 1);
					tapped[tc] = instance->targets[tc].card;
					tc++;
				}
				else{
					break;
				}
			}
			int i;
			for(i=0; i<tc; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			}
			if( tc < 2 ){
				spell_fizzled = 1;
				return 0;
			}
			instance->number_of_targets = 0;
			if( pick_target(&td1, "TARGET_PERMANENT") ){
				for(i=0; i<2; i++){
					tap_card(player, tapped[i]);
				}
				tap_card(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if(valid_target(&td1) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

static int effect_unstable_shapeshifter(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {
    result = is_humiliated((int)instance->damage_target_player,
                                       instance->damage_target_card, AL_TRIG);
    if ((((result == 0) && (trigger_condition == TRIGGER_COMES_INTO_PLAY)) && (card == affected_card)) &&
        (player == affected_card_controller))
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = new_specific_cip_global(2, event, 2, TYPE_CREATURE, NULL, player, card);
        if (result != 0)
        {
          cloning_and_verify_legend((int)instance->damage_target_player,
                                    instance->damage_target_card, instance->targets[1].player,
                                    instance->targets[1].card);
        }
      }
    }
  }
  return 0;
}

int card_unstable_shapeshifter(int player, int card, event_t event)
{
	/* Unstable Shapeshifter	|3|U	0x200357c
	 * Creature - Shapeshifter 0/1
	 * Whenever another creature enters the battlefield, ~ becomes a copy of that creature, except it has this ability. */
  cloning_card(player, card, event);

  if (event == EVENT_RESOLVE_SPELL)
	create_targetted_legacy_effect(player, card, &effect_unstable_shapeshifter, player, card);

  return 0;
}

int card_verdant_force(int player, int card, event_t event){
	/* Verdant Force	|5|G|G|G
	 * Creature - Elemental 7/7
	 * At the beginning of each upkeep, put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		generate_token_by_id(player, card, CARD_ID_SAPROLING);
	}

	return 0;
}

int card_vathi_il_dal(int player, int card, event_t event){
  /* Vhati il-Dal	|2|B|G
   * Legendary Creature - Human Warrior 3/3
   * |T: Until end of turn, target creature has base power 1 or base toughness 1. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int ai_choice = 1;
			if( is_attacking(instance->targets[0].player, instance->targets[0].card) &&
				is_unblocked(instance->targets[0].player, instance->targets[0].card) )
			{
				ai_choice = 0;
			}
			int choice = do_dialog(player, player, card, -1, -1, " Turn power to 1\n Turn toughness to 1", ai_choice);
			if( choice == 0 ){
				real_set_pt(instance->parent_controller, instance->parent_card,
												instance->targets[0].player, instance->targets[0].card, 1, -1, SET_PT_END_AT_EOT);
			}
			else{
				real_set_pt(instance->parent_controller, instance->parent_card,
												instance->targets[0].player, instance->targets[0].card, -1, 1, SET_PT_END_AT_EOT);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_wasteland(int player, int card, event_t event){
  /* Wasteland	""
   * Land
   * |T: Add |C.
   * |T, Sacrifice this land: Destroy target nonbasic land. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = SUBTYPE_NONBASIC;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE){
		instance->info_slot = 0;
		int choice = 0;
		if( !paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td, NULL) ){
			choice = do_dialog(player, player, card, -1, -1, " Get mana\n Destroy a nonbasic land\n Cancel", 1);
		}
		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		else if( choice == 1 ){
				add_state(player, card, STATE_TAPPED);
				if( charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) &&
					new_pick_target(&td, "TARGET_NONBASIC_LAND", 0, 1)
				  ){
					instance->info_slot = 1;
					kill_card(player, card, KILL_SACRIFICE);
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

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 && validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_wild_wurm(int player, int card, event_t event){
  /* Wild Wurm	|3|R
   * Creature - Wurm 5/4
   * When this creature enters, flip a coin. If you lose the flip, return this creature to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( ! flip_a_coin(player, card) ){
			bounce_permanent(player, card);
		}
	}

	return 0;
}

int card_winged_sliver(int player, int card, event_t event){
  /* Winged Sliver	|1|U
   * Creature - Sliver 1/1
   * All Sliver creatures have flying. */

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 0, 0, KEYWORD_FLYING, BCT_INCLUDE_SELF);
	return slivercycling(player, card, event);
}

int card_whispers_of_the_muse(int player, int card, event_t event){
  /* Whispers of the Muse	|U
   * Instant
   * Buyback |5 (You may pay an additional |5 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Draw a card. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int total = get_total_available_lands(player);
		if( total <= 3 || total >= 6 ){
			ai_modifier += 24;
		}
		if( has_mana_multi(player, MANACOST_X(5)) ){
			ai_modifier += 24;
		}
		basic_spell(player, card, event);
		if( spell_fizzled != 1 ){
			get_card_instance(player, card)->info_slot = buyback(player, card, MANACOST_X(5));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 1);
		if( get_card_instance(player, card)->info_slot == 1 ){
			bounce_permanent(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_worthy_cause(int player, int card, event_t event){
  /* Worthy Cause	|W
   * Instant
   * Buyback |2 (You may pay an additional |2 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * As an additional cost to cast this spell, sacrifice a creature.
   * You gain life equal to the sacrificed creature's toughness. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		instance->targets[1].player = get_toughness(BYTE2(sac), BYTE3(sac));
		instance->info_slot = buyback(player, card, MANACOST_X(2));
		if( spell_fizzled != 1 ){
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
		else{
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, instance->targets[1].player);

		if( instance->info_slot == 1 ){
			bounce_permanent(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_winds_of_rath(int player, int card, event_t event)
{
  /* Winds of Rath	|3|W|W
   * Sorcery
   * Destroy all creatures that aren't enchanted. They can't be regenerated. */

  // Destroy all creatures that aren't enchanted. They can't be regenerated.

	if (event == EVENT_RESOLVE_SPELL){
		char enchanted[2][151] = {{0}};
		int p, c;
		card_instance_t* aura;
		for (p = 0; p <= 1; ++p){
			for (c = 0; c < active_cards_count[p]; ++c){
				if ((aura = in_play(p, c)) && is_what(p, c, TYPE_ENCHANTMENT) && has_subtype(p, c, SUBTYPE_AURA)
					&& aura->damage_target_player >= 0 && aura->damage_target_player <= 1
					&& aura->damage_target_card >= 0 && aura->damage_target_card <= 150
				  ){
					enchanted[aura->damage_target_player][aura->damage_target_card] = 1;
				}
			}

			for (p = 0; p <= 1; ++p){
				for (c = 0; c < active_cards_count[p]; ++c){
					if (in_play(p, c) && is_what(p, c, TYPE_CREATURE) && !enchanted[p][c]){
						kill_card(p, c, KILL_BURY);
					}
				}
			}
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_rootwater_matriarch(int player, int card, event_t event){
  /* Rootwater Matriarch	|2|U|U
   * Creature - Merfolk 2/3
   * |T: Gain control of target creature for as long as that creature is enchanted. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = 1-player;
	if( duh_mode(player) ){
		td.required_state = TARGET_STATE_ENCHANTED;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		if( is_enchanted(instance->targets[0].player, instance->targets[0].card) ){
			gain_control_until_source_is_in_play_and_tapped(instance->parent_controller, instance->parent_card,
															instance->targets[0].player, instance->targets[0].card,
															GCUS_ENCHANTED);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_enraging_licid(int player, int card, event_t event){
  /* Enraging Licid	|1|R
   * Creature - Licid 1/1
   * |R, |T: This creature loses this ability and becomes an Aura enchantment with enchant creature. Attach it to target creature. You may pay |R to end this effect.
   * Enchanted creature has haste. */

	return generic_licid(player, card, event, MANACOST_R(1), 0, 0, 0, SP_KEYWORD_HASTE, player);
}

int card_leeching_licid(int player, int card, event_t event){
  /* Leeching Licid	|1|B
   * Creature - Licid 1/1
   * |B, |T: This creature loses this ability and becomes an Aura enchantment with enchant creature. Attach it to target creature. You may pay |B to end this effect.
   * At the beginning of the upkeep of enchanted creature's controller, this creature deals 1 damage to that player. */

	card_instance_t *instance = get_card_instance(player, card);

	if( ! is_what(player, card, TYPE_CREATURE) && instance->damage_target_player > -1 ){
		upkeep_trigger_ability(player, card, event, instance->damage_target_player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			damage_player(instance->damage_target_player, 1, player, card);
		}
	}

	return generic_licid(player, card, event, MANACOST_B(1), 0, 0, 0, 0, 1-player);
}

int card_nurturing_licid(int player, int card, event_t event)
{
  /* Nurturing Licid	|1|G
   * Creature - Licid 1/1
   * |G, |T: This creature loses this ability and becomes an Aura enchantment with enchant creature. Attach it to target creature. You may pay |G to end this effect.
   * |G: Regenerate enchanted creature. */

  if (event == EVENT_CHANGE_TYPE && card == affected_card && player == affected_card_controller)
  {
    card_instance_t *licid = get_card_instance(player, card);
    if (licid->targets[2].card != -1)
    {
      licid = get_card_instance(player, card);
      event_result = licid->targets[2].card;
    }
  }
  if (!is_what(player, card, 2))
  {
    card_instance_t *licid = get_card_instance(player, card);
    leaves_play(player, card, event, 0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      int legacy = licid->targets[3].card;
      if (legacy >= 0)
        kill_card(licid->targets[3].player, legacy, KILL_EXILE);
      return 0;
    }
    if (licid->damage_target_player >= 0)
    {
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0, 1, 0, 0, 0,
                                      NULL, NULL))
        {
          if ((((unsigned int)land_can_be_played >> 8) & 2) &&
              get_card_instance((int)licid->damage_target_player, licid->damage_target_card)
                      ->kill_code == 2)
            return 99;
          return 1;
        }
      }
      else if (event == EVENT_ACTIVATE)
      {
        if (charge_mana_for_activated_ability(player, card, 0, 0, 0, 1, 0, 0))
        {
          unsigned char regeneration_window = ((unsigned int)land_can_be_played >> 8) & 2;
          licid->info_slot = 2;
          if (regeneration_window &&
              get_card_instance((int)licid->damage_target_player, licid->damage_target_card)
                      ->kill_code == 2)
          {
            int can_regenerate =
                can_be_regenerated((int)licid->damage_target_player, licid->damage_target_card);
            licid->info_slot = (can_regenerate == 0) + 1;
          }
        }
      }
      else if (event == EVENT_RESOLVE_ACTIVATION)
      {
        int mode = licid->info_slot;
        if (mode == 1)
        {
          if (can_be_regenerated((int)licid->damage_target_player, licid->damage_target_card))
            regenerate_target((int)licid->damage_target_player, licid->damage_target_card);
          mode = licid->info_slot;
        }
        if (mode == 2)
        {
          card_instance_t *parent = get_card_instance(licid->parent_controller, licid->parent_card);
          purge_add_force_a_subtype_permanently_legacy(licid->parent_controller, licid->parent_card,
                                                       2);
          parent->targets[3].card = -1;
          parent->damage_target_player = -1;
          parent->damage_target_card = -1;
          parent->targets[2].card = -1;
          int legacy = licid->targets[3].card;
          if (legacy >= 0)
            kill_card(licid->targets[3].player, legacy, KILL_EXILE);
        }
      }
      else if (event != EVENT_CAN_CAST &&
               !(event == EVENT_CAST_SPELL && card == affected_card &&
                 player == affected_card_controller) &&
               event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
               event != EVENT_CHANGE_TARGET &&
               (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA &&
               (uint32_t)(event - EVENT_POWER) > 2 && event != EVENT_RESOLVE_MOVING_AURA &&
               event != EVENT_CHANGE_TYPE && event != EVENT_SET_COLOR &&
               event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS &&
               event != EVENT_CLEANUP)
        return 0;
      return generic_aura(player, card, event, player, 0, 0, 0x200, 0, 0, 0, 0);
    }
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
    return generic_activated_ability(player, card, event, 5, 0, 0, 0, 1, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
  {
    card_instance_t *instance = get_card_instance(player, card);
    card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
    int new_type = create_a_card_type(parent->internal_card_id);
    card_data_t *type_data = cards_at_7c7000[new_type];
    type_data->type = 4;
    type_data->power = 0;
    type_data->toughness = 0;
    parent->targets[2].card = new_type;
    add_a_subtype_permanently(player, card, instance->parent_controller, instance->parent_card,
                              SUBTYPE_AURA_CREATURE);
    attach_aura_to_target(instance->parent_controller, instance->parent_card,
                          EVENT_RESOLVE_ACTIVATION, instance->targets[0].player,
                          instance->targets[0].card);
  }
  return 0;
}

int card_quickening_licid(int player, int card, event_t event){
  /* Quickening Licid	|1|W
   * Creature - Licid 1/1
   * |1|W, |T: This creature loses this ability and becomes an Aura enchantment with enchant creature. Attach it to target creature. You may pay |W to end this effect.
   * Enchanted creature has first strike. */

	return generic_licid(player, card, event, MANACOST_XW(1, 1), 0, 0, KEYWORD_FIRST_STRIKE, 0, player);
}

int card_stinging_licid(int player, int card, event_t event){
  /* Stinging Licid	|1|U
   * Creature - Licid 1/1
   * |1|U, |T: This creature loses this ability and becomes an Aura enchantment with enchant creature. Attach it to target creature. You may pay |U to end this effect.
   * Whenever enchanted creature becomes tapped, this creature deals 2 damage to that creature's controller. */

	if( ! is_what(player, card, TYPE_CREATURE) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( event == EVENT_TAP_CARD && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			damage_player(instance->damage_target_player, 2, player, card);
		}
	}

	return generic_licid(player, card, event, MANACOST_XU(1, 1), 0, 0, 0, 0, 1-player);
}

/* ManalinkEh 2022: 0x2159660; original cards/tempest.c:5848. */
int card_avenging_angel(int player, int card, event_t event)
{
  /* Avenging Angel	|3|W|W
   * Creature - Angel 3/3
   * Flying
   * When this creature dies, you may put it on top of its owner's library. */

  int result;
  int owner;
  int position;
  result = duh_mode(player);
  result = this_dies_trigger(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (result != 0)
  {
    result = find_in_owners_graveyard(player, card, &owner, &position);
    if (result != 0)
    {
      result = graveyard_ptr_mutable[owner][position];
      remove_card_from_grave(owner, position);
      put_iid_under_the_first_x_cards_of_library(owner, result, 0);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21489c0; original cards/tempest.c:5867. */
int damage_controller_and_you(int player, int card, event_t event)
{
  card_instance_t *instance;

  if (event != EVENT_RESOLVE_TRIGGER)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  damage_player((int)instance->damage_target_player, 3, player, card);
  damage_player(player, 3, player, card);
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x2147be0; original cards/tempest.c:5878. */
void bellowing_fiend_effect(int player, int card, int t_player, int t_card)
{
  create_targetted_legacy_effect(player, card, damage_controller_and_you, t_player, t_card);
  return;
}

/* ManalinkEh 2022: 0x2159710; original cards/tempest.c:5882. */
int card_bellowing_fiend(int player, int card, event_t event)
{
  /* Bellowing Fiend	|4|B
   * Creature - Spirit 3/3
   * Flying
   * Whenever this creature deals damage to a creature, this creature deals 3 damage to that creature's controller and 3 damage to you. */

  if (is_humiliated(player, card, AL_TRIG))
  {
    return 0;
  }
  for_each_creature_damaged_by_me(player, card, event, 0, bellowing_fiend_effect, player, card);
  return 0;
}

/* ManalinkEh 2022: 0x2159780; original cards/tempest.c:5895. */
int card_blood_pet(int player, int card, event_t event)
{
  /* Blood Pet	|B
   * Creature - Thrull 1/1
   * Sacrifice this creature: Add |B. */

  switch (event)
  {
    case EVENT_CAN_ACTIVATE:
      if (can_use_activated_abilities(player, card))
        return can_sacrifice_this_as_cost(player, card);
      break;
    case EVENT_ACTIVATE:
      produce_mana(player, COLOR_BLACK, 1);
      tapped_for_mana_color = -2;
      kill_card(player, card, KILL_SACRIFICE);
      break;
    case EVENT_COUNT_MANA:
      if (affect_me(player, card) && can_sacrifice_this_as_cost(player, card) &&
          can_use_activated_abilities(player, card))
        declare_mana_available(player, COLOR_BLACK, 1);
      break;
    default:
      break;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2159870; original cards/tempest.c:5918. */
int card_bounty_hunter(int player, int card, event_t event)
{
  /* Bounty Hunter	|2|B|B
   * Creature - Human Archer Minion 2/2
   * |T: Put a bounty counter on target nonblack creature.
   * |T: Destroy target creature with a bounty counter on it. */

  enum
  {
    CHOICE_ADD_COUNTER = 1,
    CHOICE_DESTROY = 2
  };

  int choice;
  int mark_available;
  int destroy_available;
  int marked_target_available;
  int opponent_or_result;
  const char *prompt;
  target_definition_t *unmarked_target;
  card_instance_t *instance;
  target_definition_t td;
  target_definition_t td2;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    opponent_or_result = 1 - player;
    td.preferred_controller = opponent_or_result;
    td.illegal_color = get_sleighted_color_test(player, card, 2);
    default_target_definition(player, card, &td2, 2);
    td2.special = TARGET_SPECIAL_REQUIRES_COUNTER;
    td2.extra = 0x14;
    td2.preferred_controller = opponent_or_result;
    if (event == EVENT_CAN_ACTIVATE)
    {

      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL) == 0)
      {
        return 0;
      }
      opponent_or_result = can_target(&td);
      if ((opponent_or_result == 0) &&
          (opponent_or_result = can_target(&td2), opponent_or_result == 0))
      {
        return 0;
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->info_slot = 0;
      instance->number_of_targets = '\0';
      mark_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0, 0,
                                                 0, 0, &td, NULL);
      destroy_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0,
                                                    0, 0, 0, &td2, NULL);
      opponent_or_result = count_counters(opponent_or_result, card, COUNTER_BOUNTY);
      marked_target_available = can_target(&td2);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Add Bounty counter",
                           mark_available, (unsigned int)(opponent_or_result == 0),
                           "Destroy creature with bounty", destroy_available,
                           (unsigned int)(marked_target_available != 0), 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      instance->info_slot = choice;
      if (choice == CHOICE_ADD_COUNTER)
      {
        if (generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL,
                                      NULL) != 0)
        {
          prompt = get_sleighted_color_text(player, card, "Select target non%s creature", 1);
          new_pick_target(&td, prompt, 0, 0x81);
          if (spell_fizzled == 1)
          {
            remove_state(player, card, 0x10);
          }
        }
        opponent_or_result =
            count_counters(instance->targets[0].player, instance->targets[0].card, COUNTER_BOUNTY);
        if ((((opponent_or_result < 1) &&
              (opponent_or_result = can_target(&td), opponent_or_result != 0)) &&
             (current_turn != player)) &&
            (current_phase == 0x1f))
        {
          ai_modifier = ai_modifier + 0x30;
          return 0;
        }
        ai_modifier = ai_modifier + -0x30;
        return 0;
      }
      if (choice != CHOICE_DESTROY)
      {
        return 0;
      }
      opponent_or_result = generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 0,
                                                     0, 0, NULL, NULL);
      if ((opponent_or_result != 0) &&
          (new_pick_target(&td2, "Select target creature with a bounty counter to destroy.", 1,
                           0x81),
           spell_fizzled == 1))
      {
        remove_state(player, card, 0x10);
      }
      opponent_or_result =
          count_counters(instance->targets[1].player, instance->targets[1].card, COUNTER_BOUNTY);
      if (opponent_or_result < 1)
      {
        return 0;
      }
      ai_modifier = ai_modifier + 0x30;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 1 - player;
    td.illegal_color = get_sleighted_color_test(player, card, 2);
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = 1 - player;
  }
  unmarked_target = &td;
  td2.special = TARGET_SPECIAL_REQUIRES_COUNTER;
  td2.extra = 0x14;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    if (instance->info_slot == CHOICE_ADD_COUNTER)
    {
      if (valid_target(unmarked_target) != 0)
      {
        add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                     COUNTER_BOUNTY, 1);
      }
    }
    else if ((instance->info_slot == CHOICE_DESTROY) &&
             (opponent_or_result = validate_target(player, card, &td2, 1), opponent_or_result != 0))
    {
      kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2159f70; original cards/tempest.c:6017. */
int card_canyon_drake(int player, int card, event_t event)
{
  /* Canyon Drake	|2|R|R
   * Creature - Drake 1/2
   * Flying
   * |1, Discard a card at random: This creature gets +2/+0 until end of turn. */

  int result;
  if (event == EVENT_POW_BOOST)
  {
    if (0 < hand_count[player])
    {
      result = generic_shade_amt_can_pump(player, card, 2, 0, 1, 0, 0, 0, 0, 0, hand_count[player]);
      return result;
    }
  }
  else
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x20, 1, 0, 0, 0, 0, 0,
                                         0, NULL, NULL);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_shade_merge_pt(player, card, EVENT_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0, 0);
      if (spell_fizzled != 1)
      {
        discard(player, 1, player);
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result =
          generic_shade_merge_pt(player, card, EVENT_RESOLVE_ACTIVATION, 0, 0, 0, 0, 0, 0, 0, 2, 0);
      return result;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x215a1a0; original cards/tempest.c:6053. */
int card_chaotic_goo(int player, int card, event_t event)
{
  /* Chaotic Goo	|2|R|R
   * Creature - Ooze 0/0
   * This creature enters with three +1/+1 counters on it.
   * At the beginning of your upkeep, you may flip a coin. If you win the flip, put a +1/+1 counter on this creature. If you lose the flip, remove a +1/+1 counter from this creature. */

  int result;

  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);
  result = duh_mode(player);
  upkeep_trigger_ability_mode(player, card, event, player,
                              (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    return 0;
  }
  result = flip_a_coin(player, card);
  if (result != 0)
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    return 0;
  }
  remove_counters(player, card, player, card, COUNTER_P1_P1, 1);
  return 0;
}

/* ManalinkEh 2022: 0x215a260; original cards/tempest.c:6074. */
int card_chill(int player, int card, event_t event)
{
  /* Chill	|1|U
   * Enchantment
   * Red spells cost |2 more to cast. */

  unsigned int spell_colors;
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    spell_colors = get_color(affected_card_controller, affected_card);
    if ((spell_colors & 0x10) != 0)
    {
      COST_COLORLESS = COST_COLORLESS + '\x02';
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2147c10; original cards/tempest.c:6094. */
static char *prevent_damage_from_shadow_creature(int who_chooses, int player, int card)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if ((((instance->internal_card_id == damage_card) &&
        (instance->damage_target_player == player)) &&
       (result = is_what((int)instance->damage_source_player, instance->damage_source_card, 2),
        result != 0)) &&
      (result = check_for_special_ability((int)instance->damage_source_player,
                                          instance->damage_source_card, 0x100),
       result != 0))
  {
    return NULL;
  }
  return "must be combat damage from a creature with shadow";
}

/* ManalinkEh 2022: 0x215a2c0; original cards/tempest.c:6106. */
int card_circle_of_protection_shadow(int player, int card, event_t event)
{
  /* Circle of Protection: Shadow	|1|W
   * Enchantment
   * |1: The next time a creature of your choice with shadow would deal damage to you this turn, prevent that damage. */

  if (!IS_GAA_EVENT(event))
    return global_enchantment(player, card, event);

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_EFFECT);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (intptr_t)prevent_damage_from_shadow_creature;
  td.illegal_abilities = 0;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
  {
    card_instance_t *damage = get_card_instance(instance->targets[0].player, instance->targets[0].card);
    if (damage->info_slot > 0)
      damage->info_slot = 0;
  }
  return generic_activated_ability(
      player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET | GAA_LITERAL_PROMPT,
      MANACOST_X(1), 0, &td, "Select target damage from a creature with shadow.");
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x215a450; original cards/tempest.c:6136. */
int card_crown_of_flames(int player, int card, event_t event)
{
  /* Crown of Flames	|R
   * Enchantment - Aura
   * Enchant creature
   * |R: Enchanted creature gets +1/+0 until end of turn.
   * |R: Return this Aura to its owner's hand. */

  enum
  {
    CHOICE_BOOST = 1,
    CHOICE_RETURN = 2
  };

  int result_or_mode;
  card_instance_t *instance;
  int pump_available;
  if (event == EVENT_SHOULD_AI_PLAY)
  {
    instance = get_card_instance(player, card);
    result_or_mode = 0;
    if (instance->damage_target_player == player)
    {
      result_or_mode = has_mana(player, 4, 1);
      result_or_mode = result_or_mode * ((unsigned int)((player == 1) ? 12 : -12));
    }
    ai_modifier = ai_modifier + result_or_mode;
  }
  else if (event == EVENT_CHECK_ABILITIES)
  {
    instance = get_card_instance(player, card);
    result_or_mode = instance->damage_target_card;
    instance = get_card_instance(player, card);
    if ((((result_or_mode == affected_card) &&
          (instance->damage_target_player == affected_card_controller)) &&
         (instance = get_card_instance(player, card), instance->damage_target_player == player)) &&
        ((instance = in_play(player, card),
          instance != NULL &&
              (result_or_mode = can_use_activated_abilities(player, card), result_or_mode != 0))))
    {
      result_or_mode = has_mana(player, 4, 1);
      check_abilities_power = check_abilities_power + result_or_mode;
    }
  }
  result_or_mode = is_humiliated(player, card, AL_NM_NL_ACT);
  if ((result_or_mode == 0) &&
      (instance = get_card_instance(player, card), instance->damage_target_player != -1))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0, 0, 1, 0, 0,
                                       NULL, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      result_or_mode = has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 1, 0);
      pump_available = has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 1, 0);
      if (dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Boost enchanted +1/0.",
                      pump_available, 0x14, "Bounce Crown of Flames.", result_or_mode, 5, 0, 0, 0,
                      0) == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 0, 0, 0, 0, 1, 0, 0, NULL,
                                       NULL);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result_or_mode = instance->info_slot;
      if (result_or_mode == CHOICE_BOOST)
      {
        pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                      (int)instance->damage_target_player,
                                      instance->damage_target_card, 1, 0);
        result_or_mode = instance->info_slot;
      }
      if (result_or_mode == CHOICE_RETURN)
      {
        bounce_permanent(instance->parent_controller, instance->parent_card);
      }
    }
  }
  return vanilla_aura(player, card, event, player);
}
#endif

/* ManalinkEh 2022: 0x215a850; original cards/tempest.c:6194. */
int card_deadshot(int player, int card, event_t event)
{
  /* Deadshot	|3|R
   * Sorcery
   * Tap target creature. It deals damage equal to its power to another target creature. */

  card_instance_t *instance;
  int source_power;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 2, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        if (pick_target(&td, "TARGET_CREATURE") != 0)
        {
          if (is_token(player, card) == 0)
          {
            state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
            if (can_target(&td) != 0)
            {
              new_pick_target(&td, "TARGET_ANOTHER_CREATURE", 1, 1);
            }
            state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
          }
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (valid_target(&td) != 0)
      {
        tap_card(instance->targets[0].player, instance->targets[0].card);
        source_power = get_power(instance->targets[0].player, instance->targets[0].card);
        if (validate_target(player, card, &td, 1) != 0)
        {
          damage_creature(instance->targets[1].player, instance->targets[1].card, source_power,
                          player, card);
        }
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 2);
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x215ab10; original cards/tempest.c:6238. */
int card_dread_of_night(int player, int card, event_t event)
{
  /* Dread of Night	|B
   * Enchantment
   * White creatures get -1/-1. */

  card_instance_t *instance;
  unsigned int creature_colors;
  unsigned int required_color;

  if ((((uint32_t)(event - EVENT_POWER) < 2) &&
       (((card != affected_card || (player != affected_card_controller)) &&
         (instance = in_play(player, card), instance != NULL)))) &&
      ((instance = in_play(affected_card_controller, affected_card),
        instance != NULL && (is_humiliated(player, card, AL_STATIC) == 0))))
  {
    creature_colors = get_color(affected_card_controller, affected_card);
    required_color = get_sleighted_color_test(player, card, 0x20);
    if ((creature_colors & required_color) != 0)
    {
      event_result = event_result + -1;
    }
  }
  return global_enchantment(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x215abf0; original cards/tempest.c:6264. */
int card_endless_scream(int player, int card, event_t event)
{
  /* Endless Scream	|X|B
   * Enchantment - Aura
   * Enchant creature
   * This Aura enters with X scream counters on it.
   * Enchanted creature gets +1/+0 for each scream counter on this Aura. */

  card_instance_t *instance;
  int result;
  int32_t number;

  instance = get_card_instance(player, card);
  number = x_value;
  if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance->info_slot = x_value;
    if (player == 1)
    {
      ai_modifier = ai_modifier + (number * 3 + -9) * 8;
    }
  }
  else
  {
    number = instance->info_slot;
  }
  enters_the_battlefield_with_counters(player, card, event, COUNTER_SCREAM, number);
  attached_gets_pt(player, card, event, instance->info_slot, 0);
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x2147610; original cards/tempest.c:6300. */
static void blocked_by_me_or_my_blocker_gets_1_1(int player, int card, int t_player, int t_card)
{
  pump_ability_until_eot(player, card, t_player, t_card, 1, 1, 0, 0);
  return;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x215acc0; original cards/tempest.c:6305. */
int card_flailing_drake(int player, int card, event_t event)
{
  /* Flailing Drake	|3|G
   * Creature - Drake 2/3
   * Flying
   * Whenever this creature blocks or becomes blocked by a creature, that creature gets +1/+1 until end of turn. */

  card_instance_t *instance;
  if (event == EVENT_CHECK_ABILITIES)
  {
    if (((card == affected_card) && (player == affected_card_controller)) && (current_phase < 0x18))
    {
      check_abilities_power = check_abilities_power + -1;
      check_abilities_toughness = check_abilities_toughness + -1;
    }
  }
  else if (event == EVENT_DECLARE_BLOCKERS)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      instance = get_card_instance(player, card);
      if ((current_turn == player) && ((instance->state & 4) != 0))
      {
        for_each_creature_blocking_me(current_turn, card, blocked_by_me_or_my_blocker_gets_1_1,
                                      current_turn, card);
      }
      if ((1 - current_turn == player) && ((instance->state & 8) != 0))
      {
        for_each_creature_blocked_by_me(player, card, blocked_by_me_or_my_blocker_gets_1_1, player,
                                        card);
      }
    }
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x215adf0; original cards/tempest.c:6329. */
int card_flowstone_giant(int player, int card, event_t event)
{
  /* Flowstone Giant	|2|R|R
   * Creature - Giant 3/3
   * |R: This creature gets +2/-2 until end of turn. */

  int result;
  unsigned int current_toughness;
  if (event == EVENT_CHECK_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      current_toughness = get_toughness(player, card);
      if ((current_toughness & 1) == 0)
      {
        result = (int)current_toughness / 2 + -1;
      }
      else
      {
        result = (int)(current_toughness - 1) / 2;
      }
      result = generic_shade_amt_can_pump(player, card, 2, 0, 0, 0, 0, 0, 1, 0, result);
      check_abilities_power = check_abilities_power + result;
      check_abilities_toughness = check_abilities_toughness - result;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    return generic_shade_merge_pt(player, card, EVENT_RESOLVE_ACTIVATION, 0, 0, 0, 0, 0, 1, 0, 2,
                                  -2);
  }
  return generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 1, 0, 0, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x2148470; original cards/tempest.c:6357. */
static char *is_blocking_flowstone_salamander(int who_chooses, int player, int card,
                                              int targeting_player, int targeting_card)
{
  int result;

  result = is_blocking(card, targeting_card);
  if (result == 0)
  {
    return "must be blocking this attacking creature";
  }
  return NULL;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x215afa0; original cards/tempest.c:6364. */
int card_flowstone_salamander(int player, int card, event_t event)
{
  /* Flowstone Salamander	|3|R|R
   * Creature - Salamander 3/4
   * |R: This creature deals 1 damage to target creature blocking it. */

  int result;
  target_definition_t td2;
  if (event == EVENT_CHECK_ABILITIES)
  {
    if (((((current_turn == player) && (card == affected_card)) &&
          (player == affected_card_controller)) &&
         ((current_phase < 0x18 &&
           (result = can_use_activated_abilities(player, card), result != 0)))) &&
        (result = has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0), result != 0))
    {
      result = has_mana(player, 4, 1);
      check_abilities_power = check_abilities_power + result;
      check_abilities_toughness = check_abilities_toughness + result;
    }
  }
  else if (((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
           ((event == EVENT_RESOLVE_ACTIVATION || (event == EVENT_CLEANUP))))
  {
    default_target_definition(player, card, &td2, 2);
    td2.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td2.extra = (intptr_t)is_blocking_flowstone_salamander;
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td2), result != 0))
    {
      damage_target0(player, card, 1);
    }
    result =
        generic_activated_ability(player, card, event, 6, 0, 0, 0, 0, 1, 0, 0, &td2,
                                       "Select target creature blocking this deal 1 damage to it.");
    return result;
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x215b160; original cards/tempest.c:6395. */
int card_frog_tongue(int player, int card, event_t event)
{
  /* Frog Tongue	|G
   * Enchantment - Aura
   * Enchant creature
   * When this Aura enters, draw a card.
   * Enchanted creature has reach. (It can block creatures with flying.) */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_a_card(player);
  }
  attached_gets_kw(player, card, event, KEYWORD_REACH, SP_KEYWORD_0);
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x215b1f0; original cards/tempest.c:6414. */
int card_fugitive_druid(int player, int card, event_t event)
{
  /* Fugitive Druid	|3|G
   * Creature - Human Druid 3/2
   * Whenever this creature becomes the target of an Aura spell, you draw a card. */

  card_instance_t *instance;
  target_t *targeting_aura;
  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      targeting_aura = becomes_target_of_subtype_spell(player, card, event, player, card, 2,
                                                       RESOLVE_TRIGGER_MANDATORY, 0x4000);
      if (targeting_aura != NULL)
      {
        draw_cards(player, 1);
      }
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x215b280; original cards/tempest.c:6428. */
int card_fylamarid(int player, int card, event_t event)
{
  /* Fylamarid	|1|U|U
   * Creature - Squid Beast 1/3
   * Flying
   * This creature can't be blocked by blue creatures.
   * |U: Target creature becomes blue until end of turn. */

  card_instance_t *instance;
  const char *prompt;
  color_test_t new_color;
  unsigned int creature_colors;
  unsigned int required_color;
  target_definition_t td;

  if (event == EVENT_BLOCK_LEGALITY)
  {
    if ((attacking_card_controller == player) && (attacking_card == card))
    {
      if (!is_humiliated(player, card, AL_STATIC))
      {
        creature_colors = get_color(affected_card_controller, affected_card);
        required_color = get_sleighted_color_test(player, card, 4);
        if ((creature_colors & required_color) != 0)
        {
          event_result = 1;
        }
      }
    }
    default_target_definition(player, card, &td, 2);
    get_card_instance(player, card);
  }
  else
  {
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (valid_target(&td))
      {
        new_color = get_sleighted_color_test(player, card, 4);
        change_color(instance->parent_controller, instance->parent_card,
                     instance->targets[0].player, instance->targets[0].card, new_color,
                     CHANGE_COLOR_END_AT_EOT | CHANGE_COLOR_SET);
      }
    }
  }
  prompt = get_sleighted_color_text(player, card,
                                    "Select target creature it becomes %s until end of turn.", 2);
  return generic_activated_ability(player, card, event, 6, 0, 0, 1, 0, 0, 0, 0, &td, prompt);
}

/* ManalinkEh 2022: 0x215b450; original cards/tempest.c:6458. */
int card_gallantry(int player, int card, event_t event)
{
  /* Gallantry	|1|W
   * Instant
   * Target blocking creature gets +4/+4 until end of turn.
   * Draw a card. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (event != EVENT_CHECK_PUMP)
  {
    if (event != EVENT_CAN_CAST)
    {
      if (event == EVENT_CAST_SPELL)
      {
        if ((card != affected_card) || (player != affected_card_controller))
        {
          return 0;
        }
      }
      else if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
               (event != EVENT_CHANGE_TARGET))
      {
        return 0;
      }
    }
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.required_state = TARGET_STATE_BLOCKING;
    result = vanilla_pump(player, card, event, &td, 4, 4, 0x1000000, 0);
    return result;
  }
  instance = get_card_instance(player, card);
  result = has_mana_to_cast_iid(player, EVENT_CAN_CAST, instance->internal_card_id);
  if (result == 0)
  {
    return 0;
  }
  if (current_turn != player)
  {
    pumpable_power[player] = pumpable_power[player] + 4;
    pumpable_toughness[player] = pumpable_toughness[player] + 4;
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x215b590; original cards/tempest.c:6485. */
int card_ghost_town(int player, int card, event_t event)
{
  /* Ghost Town	""
   * Land
   * |T: Add |C.
   * |0: Return this land to its owner's hand. Activate only if it's not your turn. */

  enum { CHOICE_MANA = 1, CHOICE_RETURN_TO_HAND = 2, RESOLVE_RETURN_TO_HAND = 3 };

  if (event == EVENT_CAN_ACTIVATE)
  {
    if (generic_activated_ability(player, card, event, GAA_IN_OPPONENT_TURN,
                                  MANACOST0, 0, NULL, NULL))
      return 1;
    if (generic_activated_ability(player, card, event, GAA_UNTAPPED,
                                  MANACOST0, 0, NULL, NULL))
      return mana_producer(player, card, event);
    get_card_instance(player, card);
    return 0;
  }
  if (event != EVENT_ACTIVATE)
  {
    if (event == EVENT_RESOLVE_ACTIVATION || event == EVENT_CLEANUP)
    {
      card_instance_t *instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_ACTIVATION && instance->info_slot == RESOLVE_RETURN_TO_HAND)
        bounce_permanent(instance->parent_controller, instance->parent_card);
      return 0;
    }
    return mana_producer(player, card, event);
  }

  card_instance_t *instance = get_card_instance(player, card);
  instance->info_slot = 0;
  int mana_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE,
                                                GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
  int bounce_available = 0;
  if (!paying_mana())
    bounce_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE,
                                                 GAA_IN_OPPONENT_TURN, MANACOST0, 0, NULL, NULL) != 0;
  // The second query is live: the availability helper above may change payment state.
  if (paying_mana())
  {
    if (!mana_available)
    {
      spell_fizzled = 1;
      return 0;
    }
    return mana_producer(player, card, event);
  }

  int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                      "Get mana", mana_available, 10,
                      "Return this to hand", bounce_available, 5);
  if (!choice)
  {
    spell_fizzled = 1;
    return 0;
  }
  if (choice == CHOICE_MANA)
    return mana_producer(player, card, event);

  instance->info_slot = choice;
  if (choice == CHOICE_RETURN_TO_HAND)
  {
    if (charge_mana_for_activated_ability(player, card, MANACOST0))
      instance->info_slot = RESOLVE_RETURN_TO_HAND;
    tapped_for_mana_color = -1;
  }
  return 0;
}

/* ManalinkEh 2022: 0x215b970; original cards/tempest.c:6558. */
int card_havoc(int player, int card, event_t event)
{
  /* Havoc	|1|R
   * Enchantment
   * Whenever an opponent casts a white spell, they lose 2 life. */

  int result;
  test_definition_t test;

  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 0xff);
    test.color = get_sleighted_color_test(player, card, 0x20);
    sp_ability_trigger(1 - player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA)
  {
    lose_life(1 - player, 2);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x215ba50; original cards/tempest.c:6577. */
int card_heartwood_dryad(int player, int card, event_t event)
{
  /* Heartwood Dryad	|1|G
   * Creature - Dryad 2/1
   * This creature can block creatures with shadow as though it had shadow. */

  if (event != EVENT_ABILITIES)
  {
    return 0;
  }
  special_abilities(player, card, EVENT_ABILITIES, 0x8000000, player, card);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x215baa0; original cards/tempest.c:6594. */
int card_heros_resolve(int player, int card, event_t event)
{
  /* Hero's Resolve	|1|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+5. */

  int result;

  attached_gets_pt(player, card, event, 1, 5);
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x215bb00; original cards/tempest.c:6605. */
int card_imps_taunt(int player, int card, event_t event)
{
  /* Imps' Taunt	|1|B
   * Instant
   * Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Target creature attacks this turn if able. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    get_card_instance(player, card);
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      instance = get_card_instance(player, card);
      if ((card == affected_card) &&
          ((player == affected_card_controller &&
            (generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                           "Select target creature it attacks this turn if able.", 1, NULL),
             spell_fizzled != 1))))
      {
        result = buyback(player, card, 3, 0, 0, 0, 0, 0);
        instance->info_slot = result;
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = valid_target(&td);
      if ((result == 0) || (pump_ability_until_eot(player, card, instance->targets[0].player,
                                                   instance->targets[0].card, 0, 0, 0, 0x400),
                            instance->info_slot != 1))
      {
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
      bounce_permanent(player, card);
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 2);
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x215bd80; original cards/tempest.c:6645. */
int card_insight(int player, int card, event_t event)
{
  /* Insight	|2|U
   * Enchantment
   * Whenever an opponent casts a green spell, you draw a card. */

  int result;
  test_definition_t test;

  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 0xff);
    test.color = get_sleighted_color_test(player, card, 8);
    sp_ability_trigger(1 - player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA)
  {
    draw_cards(player, 1);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x215be60; original cards/tempest.c:6664. */
int card_legerdemain(int player, int card, event_t event)
{
  /* Legerdemain	|2|U|U
   * Sorcery
   * Exchange control of target artifact or creature and another target permanent that shares one of those types with it. (This effect lasts indefinitely.) */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  target_definition_t td2;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x42);
    td.preferred_controller = player;
    td.allowed_controller = player;
    default_target_definition(player, card, &td2, 0x42);
    td.preferred_controller = 1 - player;
    td2.allowed_controller = td.preferred_controller;
    get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
      return (unsigned int)(result != 0);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x42);
      td.preferred_controller = player;
      td.allowed_controller = player;
      default_target_definition(player, card, &td2, 0x42);
      td.preferred_controller = 1 - player;
      td2.allowed_controller = td.preferred_controller;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        result = new_pick_target(&td, "Select target artifact or creature you control.", 0, 0x81);
        if (result != 0)
        {
          new_pick_target(&td2, "Select target artifact or creature controlled by opponent.", 1,
                          0x81);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x42);
    td.preferred_controller = player;
    td.allowed_controller = player;
    default_target_definition(player, card, &td2, 0x42);
    td.preferred_controller = 1 - player;
    td2.allowed_controller = td.preferred_controller;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = validate_target(player, card, &td, 0);
      if ((result != 0) && (result = validate_target(player, card, &td2, 1), result != 0))
      {
        exchange_control_of_target_permanents(
            player, card, instance->targets[0].player, instance->targets[0].card,
            instance->targets[1].player, instance->targets[1].card);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 0x42);
    td.preferred_controller = player;
    td.allowed_controller = player;
    default_target_definition(player, card, &td2, 0x42);
    td.preferred_controller = 1 - player;
    td2.allowed_controller = td.preferred_controller;
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x215c1d0; original cards/tempest.c:6706. */
int card_light_of_day(int player, int card, event_t event)
{
  /* Light of Day	|3|W
   * Enchantment
   * Black creatures can't attack or block. */

  card_instance_t *instance;
  unsigned int creature_colors;
  unsigned int required_color;

  if (event == EVENT_ABILITIES)
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        creature_colors = get_color(affected_card_controller, affected_card);
        required_color = get_sleighted_color_test(player, card, 2);
        if ((creature_colors & required_color) != 0)
        {
          cannot_attack(affected_card_controller, affected_card, EVENT_ABILITIES);
          cannot_block(affected_card_controller, affected_card, EVENT_ABILITIES);
        }
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2148c40; original cards/tempest.c:6721. */
static int effect_maddening_imp(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;
  int card_00;

  all_must_attack_if_able(player, event, -1);
  if (((event == EVENT_CLEANUP) && (card == affected_card)) && (player == affected_card_controller))
  {

    card_00 = active_cards_count[current_turn] + -1;
    if (-1 < card_00)
    {
      do
      {
        instance = in_play(current_turn, card_00);
        if (((instance != NULL) && (result = is_what(current_turn, card_00, 2), result != 0)) &&
            ((result = has_subtype(current_turn, card_00, SUBTYPE_WALL),
              result == 0 && (instance = get_card_instance(current_turn, card_00),
                              (instance->state & 0x20040) == 0))))
        {
          kill_card(current_turn, card_00, KILL_DESTROY);
        }
        card_00 = card_00 + -1;
      } while (card_00 != -1);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return 0;
}

/* ManalinkEh 2022: 0x215c2a0; original cards/tempest.c:6740. */
int card_maddening_imp(int player, int card, event_t event)
{
  /* Maddening Imp	|2|B
   * Creature - Imp 1/1
   * Flying
   * |T: Non-Wall creatures the active player controls attack this turn if able. At the beginning of the next end step, destroy each of those creatures that didn't attack this turn. Activate only during an opponent's turn and only before combat. */

  int result;
  int opponent;

  if (event == EVENT_ACTIVATE)
  {
    result = generic_activated_ability(player, card, EVENT_ACTIVATE, 0x40000001, 0, 0, 0, 0, 0, 0,
                                       0, NULL, NULL);
    if (result == 0)
    {
      return 0;
    }
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    result =
        generic_activated_ability(player, card, event, 0x40000001, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  }
  if (result != 0 && current_phase < 0x15)
  {
    return 1;
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    result = get_original_internal_card_id(player, card);
    opponent = 1 - player;
    result = add_card_to_hand(opponent, result);
    create_legacy_effect(opponent, result, effect_maddening_imp);
    obliterate_card(opponent, result);
  }
  return 0;
}

/* ManalinkEh 2022: 0x215c430; original cards/tempest.c:6765. */
int card_magmasaur(int player, int card, event_t event)
{
  /* Magmasaur	|3|R|R
   * Creature - Elemental Dinosaur 0/0
   * This creature enters with five +1/+1 counters on it.
   * At the beginning of your upkeep, you may remove a +1/+1 counter from this creature. If you don't, sacrifice this creature and it deals damage equal to the number of +1/+1 counters on it to each creature without flying and each player. */

  int dmg;
  int result;
  test_definition_t this_test;

  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 5);
  upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    return 0;
  }

  dmg = count_counters(player, card, COUNTER_P1_P1);
  default_test_definition(&this_test, 2);
  this_test.keyword = 0x20;
  this_test.keyword_flag = 1;
  result = do_dialog(player, player, card, -1, -1, " Remove counter\n sacrifice & damage", 0);
  if (result == 0)
  {
    remove_counters(player, card, player, card, COUNTER_P1_P1, 1);
    return 0;
  }
  kill_card(player, card, KILL_SACRIFICE);
  new_damage_all(player, card, 2, dmg, 2, &this_test);
  return 0;
}

/* ManalinkEh 2022: 0x215c590; original cards/tempest.c:6804. */
int card_marsh_lurker(int player, int card, event_t event)
{
  /* Marsh Lurker	|3|B
   * Creature - Beast 3/2
   * Sacrifice a Swamp: This creature gains fear until end of turn. (It can't be blocked except by artifact creatures and/or black creatures.) */

  const char *swamp_prompt;
  test_definition_t this_test;

  if (event == EVENT_ACTIVATE)
  {
    swamp_prompt = get_hacked_land_text(player, card, "Select a %s to sacrifice.", 0x5009);
    new_default_test_definition(&this_test, 1, swamp_prompt);
    this_test.subtype = get_hacked_subtype(player, card, 0x5009);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    swamp_prompt = get_hacked_land_text(player, card, "Select a %s to sacrifice.", 0x5009);
    new_default_test_definition(&this_test, 1, swamp_prompt);
    this_test.subtype = get_hacked_subtype(player, card, 0x5009);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      pump_ability_until_eot(player, card, player, card, 0, 0, 0, 1);
    }
  }
  return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_NONE, 0, 1, 0, 0,
                                                          0, 0, 0, NULL, NULL, TYPE_NONE,
                                                          &this_test, SARV_0);
}

/* ManalinkEh 2022: 0x2148b60; original cards/tempest.c:6825. */
char *with_shadow(int who_chooses, int player, int card)
{
  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (((instance != NULL) && (result = is_what(player, card, 2), result != 0)) &&
      (result = check_for_special_ability(player, card, 0x100), result != 0))
  {
    return NULL;
  }
  return "illegal target doesn\'t have shadow.";
}

/* ManalinkEh 2022: 0x215c730; original cards/tempest.c:6835. */
int card_maze_of_shadows(int player, int card, event_t event)
{
  /* Maze of Shadows	""
   * Land
   * |T: Add |C.
   * |T: Untap target attacking creature with shadow. Prevent all combat damage that would be dealt to and dealt by that creature this turn. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ABILITY = 2
  };

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      default_target_definition(player, card, &td, 2);
      td.required_state = TARGET_STATE_ATTACKING;
      td.extra = (intptr_t)with_shadow;
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      instance = get_card_instance(player, card);

      result = paying_mana();
      if ((result == 0) && (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5,
                                                               1, 0, 0, 0, 0, 0, 0, &td, NULL),
                            result != 0))
      {
        result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                             "Prevent damage", 1, 1, 0, 0, 0, 0);
        if (result == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        instance->info_slot = result;
        if (result != CHOICE_MANA)
        {
          if (result != CHOICE_ABILITY)
          {
            return 0;
          }
          generic_activated_ability(player, card, EVENT_ACTIVATE, 7, 0, 0, 0, 0, 0, 0, 0, &td,
                                    "Select target attacking creature with shadow");
          return 0;
        }
      }
      else
      {
        instance->info_slot = 1;
      }
      mana_producer(player, card, EVENT_ACTIVATE);
      return 0;
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      default_target_definition(player, card, &td, 2);
      td.required_state = TARGET_STATE_ATTACKING;
      td.extra = (intptr_t)with_shadow;
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot == 2)
      {
        result = validate_target(player, card, &td, 0);
        if (result != 0)
        {
          untap_card(instance->targets[0].player, instance->targets[0].card);
          maze_of_ith_effect(player, instance->parent_card, instance->targets[0].player,
                             instance->targets[0].card);
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

/* ManalinkEh 2022: 0x215ca50; original cards/tempest.c:6893. */
int card_mogg_cannon(int player, int card, event_t event)
{
  /* Mogg Cannon	|2
   * Artifact
   * |T: Target creature you control gets +1/+0 and gains flying until end of turn. Destroy that creature at the beginning of the next end step. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allowed_controller = player;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      result = pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                                      instance->targets[0].player, instance->targets[0].card, 1, 0,
                                      0x20, 0x10000000);
      instance = get_card_instance(player, result);
      instance->targets[3].card = 2;
    }
  }
  result = generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
  return result;
}

/* ManalinkEh 2022: 0x215cbe0; original cards/tempest.c:6928. */
int card_mogg_squad(int player, int card, event_t event)
{
  /* Mogg Squad	|1|R
   * Creature - Goblin 3/3
   * This creature gets -1/-1 for each other creature on the battlefield. */

  int result;
  test_definition_t this_test;

  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      default_test_definition(&this_test, 2);
      this_test.not_me = 1;
      result = check_battlefield_for_special_card(player, card, 2, 4, &this_test);
      event_result = event_result - result;
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2147060; original cards/tempest.c:6950. */
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
/* ManalinkEh 2022: 0x215cc90; original cards/tempest.c:6956. */
int card_opportunist(int player, int card, event_t event)
{
  /* Opportunist	|2|R
   * Creature - Human Soldier 2/2
   * |T: This creature deals 1 damage to target creature that was dealt damage this turn. */

  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.extra = (intptr_t)is_already_damaged;
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.extra = (intptr_t)is_already_damaged;
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      damage_target0(player, card, 1);
    }
  }
  result =
      generic_activated_ability(player, card, event, 7, 0, 0, 0, 0, 0, 0, 0, &td,
                                     "Select target creature that was dealt damage this turn.");
  return result;
}
#endif

/* ManalinkEh 2022: 0x215cdd0; original cards/tempest.c:6988. */
int card_pallimud(int player, int card, event_t event)
{
  /* Pallimud	|2|R
   * Creature - Beast * /3
   * As this creature enters, choose an opponent.
   * ~'s power is equal to the number of tapped lands the chosen player controls. */

  int newval;
  test_definition_t this_test;

  if ((((event == EVENT_POWER) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    default_test_definition(&this_test, 1);
    this_test.state = 0x10;
    newval = check_battlefield_for_special_card(player, card, 1 - player, 4, &this_test);
    set_characteristic_pt(newval);
  }
  return 0;
}

/* ManalinkEh 2022: 0x215ce60; original cards/tempest.c:7014. */
int card_rats_of_rath(int player, int card, event_t event)
{
  /* Rats of Rath	|1|B
   * Creature - Rat 2/1
   * |B: Destroy target artifact, creature, or land you control. */

  if (!IS_GAA_EVENT(event))
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, 0x43);
  td.allowed_controller = player;
  td.preferred_controller = player;
  if (event == EVENT_ACTIVATE)
  {
    generic_activated_ability(player, card, event, 6, 0, 1, 0, 0, 0, 0, 0, &td,
                              "Select target artifact, creature, or land you control.");
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
  {
    card_instance_t *instance = get_card_instance(player, card);
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
  }
  return generic_activated_ability(player, card, event, 4, 0, 1, 0, 0, 0, 0, 0, &td, NULL);
}

/* ManalinkEh 2022: 0x215d010; original cards/tempest.c:7045. */
int card_reality_anchor(int player, int card, event_t event)
{
  /* Reality Anchor	|1|G
   * Instant
   * Target creature loses shadow until end of turn.
   * Draw a card. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        default_target_definition(player, card, &td, 2);
        if ((card != affected_card) || (player != affected_card_controller))
          return generic_spell(player, card, event, 0x81, &td,
                               "Select target creature it loses shadow until end of turn.", 1,
                               NULL);
        instance = get_card_instance(player, card);
        generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                      "Select target creature it loses shadow until end of turn.", 1, NULL);
        if (spell_fizzled != 1)
        {
          result = check_for_special_ability(instance->targets[0].player, instance->targets[0].card,
                                             0x100);
          ai_modifier = (unsigned int)((result == 0) ? -36 : 0) + ai_modifier + 0x48 +
                        (unsigned int)((instance->targets[0].player == 1 - player) ? 0 : -96);
        }
      }
      return 0;
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 2);
      if (event == EVENT_RESOLVE_SPELL)
      {

        instance = get_card_instance(player, card);
        result = valid_target(&td);
        if (result != 0)
        {
          negate_ability_and_special_ability_until_eot(player, card, instance->targets[0].player,
                                                       instance->targets[0].card, KEYWORD_0, 0x100);
          draw_a_card(player);
        }
        kill_card(player, card, KILL_DESTROY);
      }
      return generic_spell(player, card, event, 0x81, &td,
                           "Select target creature it loses shadow until end of turn.", 1, NULL);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  default_target_definition(player, card, &td, 2);
  return generic_spell(player, card, event, 0x81, &td,
                       "Select target creature it loses shadow until end of turn.", 1, NULL);
}

/* ManalinkEh 2022: 0x215d260; original cards/tempest.c:7082. */
int card_reap(int player, int card, event_t event)
{
  /* Reap	|1|G
   * Instant
   * Return up to X target cards from your graveyard to your hand, where X is the number of black permanents target opponent controls as you cast this spell. */

  int limit_or_result;
  target_definition_t td;
  target_definition_t td_opp;
  test_definition_t this_test;

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
    }
    else if ((event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET) &&
             (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  default_target_definition(player, card, &td, 0xff);
  default_target_definition(player, card, &td_opp, 0);
  td_opp.zone = TARGET_ZONE_PLAYERS;
  td_opp.allowed_controller = 1 - player;
  limit_or_result =
      count_permanents_by_color(1 - player, 0x47, get_sleighted_color_test(player, card, 2));
  new_default_test_definition(&this_test, 0xff,
                              "Select a card to return from your graveyard to your hand.");
  limit_or_result = spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(
      player, card, event, limit_or_result, &this_test, 0);
  if (can_target(&td_opp) == 0)
  {
    return 0;
  }
  return limit_or_result;
}

/* ManalinkEh 2022: 0x215d3d0; original cards/tempest.c:7115. */
int card_rootwater_diver(int player, int card, event_t event)
{
  /* Rootwater Diver	|U
   * Creature - Merfolk 1/1
   * |T, Sacrifice this creature: Return target artifact card from your graveyard to your hand. */

  int result;
  test_definition_t this_test;
  if (!IS_GAA_EVENT(event))
    return 0;

  new_default_test_definition(&this_test, 0x40, "Select an artifact card.");
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = validate_target_from_grave_source(player, card, player, 0), result != -1))
  {
    from_grave_to_hand(player, result, 1);
  }
  result = generic_activated_ability_with_target_in_grave(
      player, card, event, GAA_SACRIFICE_ME | GAA_UNTAPPED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL,
      TYPE_NONE, &this_test, 8);
  return result;
}

/* ManalinkEh 2022: 0x215d510; original cards/tempest.c:7139. */
int card_rootwater_shaman(int player, int card, event_t event)
{
  /* Rootwater Shaman	|2|U
   * Creature - Merfolk Shaman 2/2
   * You may cast Aura spells with enchant creature as though they had flash. */

  const char *prompt;
  target_definition_t td;

  if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_ACTIVATE))
  {
    return 0;
  }
  base_target_definition(player, card, &td, 4);
  td.required_subtype = 0x4002;
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.zone = TARGET_ZONE_HAND;
  if (event == EVENT_ACTIVATE)
  {
    prompt = "Select an aura card with enchant creature.";
  }
  else
  {
    prompt = NULL;
  }
  return can_play_cards_as_though_they_had_flash(player, card, event, &td, prompt, 1);
}

/* ManalinkEh 2022: 0x215d5b0; original cards/tempest.c:7158. */
int card_segmented_wurm(int player, int card, event_t event)
{
  /* Segmented Wurm	|3|R|G
   * Creature - Wurm 5/5
   * Whenever this creature becomes the target of a spell or ability, put a -1/-1 counter on it. */

  card_instance_t *instance;
  const target_t *targeting_source;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      targeting_source = becomes_target_of_spell_or_effect(player, card, event, player, card, 2);
      if (targeting_source != NULL)
      {
        add_counters(player, card, player, card, COUNTER_M1_M1, 1);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x215d640; original cards/tempest.c:7172. */
int card_servant_of_volrath(int player, int card, event_t event)
{
  /* Servant of Volrath	|2|B
   * Creature - Minion 3/3
   * When this creature leaves the battlefield, sacrifice a creature. */

  leaves_play(player, card, event, 1, 1);
  if (event != EVENT_LEAVES_PLAY_ABILITY)
  {
    return 0;
  }
  impose_sacrifice(player, card, player, 1, 2, 0, 0, 0, 0, 0, 0, 0, -1, 0);
  return 0;
}

/* ManalinkEh 2022: 0x215d700; original cards/tempest.c:7185. */
int card_shadowstorm(int player, int card, event_t event)
{
  /* Shadowstorm	|R
   * Sorcery
   * ~ deals 2 damage to each creature with shadow. */

  int result;
  test_definition_t test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    new_default_test_definition(&test, 2, "");
    test.keyword = 0x100;
    test.keyword_flag = 2;
    new_damage_all(player, card, 2, 2, 0, &test);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x215d7b0; original cards/tempest.c:7204. */
int card_shimmering_wings(int player, int card, event_t event)
{
  /* Shimmering Wings	|U
   * Enchantment - Aura
   * Enchant creature (Target a creature as you cast this. This card enters attached to that creature.)
   * Enchanted creature has flying. (It can't be blocked except by creatures with flying or reach.)
   * |U: Return this Aura to its owner's hand. */

  card_instance_t *instance;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = get_card_instance(player, card);
    if (-1 < instance->damage_target_player)
    {

      instance = get_card_instance(player, card);
      if (event != EVENT_ACTIVATE)
      {
        if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
        {
          attached_gets_kw(player, card, event, KEYWORD_FLYING, SP_KEYWORD_0);
          return aura(player, card, event, NULL, NULL);
        }
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          bounce_permanent(instance->parent_controller, instance->parent_card);
        }
      }
      return generic_activated_ability(player, card, event, 0, 0, 0, 1, 0, 0, 0, 0, NULL, NULL);
    }
  }
  attached_gets_kw(player, card, event, KEYWORD_FLYING, SP_KEYWORD_0);
  return aura(player, card, event, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x215d8e0; original cards/tempest.c:7227. */
int card_souldrinker(int player, int card, event_t event)
{
  /* Souldrinker	|3|B
   * Creature - Spirit 2/2
   * Pay 3 life: Put a +1/+1 counter on this creature. */

  int result;

  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  }
  result = generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 0, 3, NULL, NULL);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x215d9a0; original cards/tempest.c:7244. */
int card_spinal_graft(int player, int card, event_t event)
{
  /* Spinal Graft	|1|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +3/+3.
   * When enchanted creature becomes the target of a spell or ability, destroy that creature. It can't be regenerated. */

  card_instance_t *instance;
  card_instance_t *aura_or_attached;
  int result;
  const target_t *targeting_source;

  instance = get_card_instance(player, card);
  aura_or_attached = in_play(player, card);
  if ((((aura_or_attached != NULL) && (-1 < instance->damage_target_card)) &&
       (aura_or_attached =
            in_play((int)instance->damage_target_player, instance->damage_target_card),
        aura_or_attached != NULL)) &&
      ((result = is_humiliated(player, card, AL_TRIG),
        result == 0 && (targeting_source = becomes_target_of_spell_or_effect(
                            player, card, event, (int)instance->damage_target_player,
                            instance->damage_target_card, 2),
                        targeting_source != NULL))))
  {
    kill_card((int)instance->damage_target_player, instance->damage_target_card, KILL_BURY);
    return 1;
  }
  attached_gets_pt(player, card, event, 3, 3);
  return aura(player, card, event, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x215dab0; original cards/tempest.c:7266. */
int card_spontaneous_combustion(int player, int card, event_t event)
{
  /* Spontaneous Combustion	|1|B|R
   * Instant
   * As an additional cost to cast this spell, sacrifice a creature.
   * ~ deals 3 damage to each creature. */

  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    new_damage_all(player, card, 2, 3, 1, NULL);
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x400, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x215db90; original cards/tempest.c:7285. */
int card_squees_toy(int player, int card, event_t event)
{
  /* Squee's Toy	|1
   * Artifact
   * |T: Prevent the next 1 damage that would be dealt to target creature this turn. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 0xff);
  td.extra = damage_card;
  td.special = TARGET_SPECIAL_DAMAGE_CREATURE;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    result = validate_target(player, card, &td, 0);
    if (result != 0)
    {
      prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
    }
  }
  result = generic_activated_ability(player, card, event, 0x1000005, 0, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_DAMAGE");
  return result;
}

/* ManalinkEh 2022: 0x215dc90; original cards/tempest.c:7306. */
int card_steal_enchantment(int player, int card, event_t event)
{
  /* Steal Enchantment	|U|U
   * Enchantment - Aura
   * Enchant enchantment
   * You control enchanted enchantment. */

  target_definition_t td;

  leaves_play(player, card, event, 0, 0);
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && (event & ~EVENT_UPKEEP) != EVENT_GET_SELECTED_CARD &&
      event != EVENT_STATIC_EFFECTS && event != EVENT_LEAVES_PLAY_ABILITY)
    return 0;
  default_target_definition(player, card, &td, 4);
  return generic_stealing_aura(player, card, event, &td, "TARGET_ENCHANTMENT");
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x215dda0; original cards/tempest.c:7324. */
int card_tahngarths_rage(int player, int card, event_t event)
{
  /* Tahngarth's Rage	|R
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +3/+0 as long as it's attacking. Otherwise, it gets -2/-1. */

  card_instance_t *instance;
  int result;
  card_instance_t *aura_record;

  instance = get_card_instance(player, card);
  if ((((event == EVENT_CHECK_ABILITIES) && (instance->damage_target_card == affected_card)) &&
       (affected_card_controller == instance->damage_target_player)) &&
      (instance->damage_target_player == player))
  {
    aura_record = in_play(player, card);
    if ((aura_record != NULL) && (current_phase < 0x18))
    {
      check_abilities_power = check_abilities_power + 5;
      check_abilities_toughness = check_abilities_toughness + 1;
    }
  }
  result = is_humiliated(player, card, AL_STATIC);
  if (result == 0)
  {
    aura_record = get_card_instance(player, card);
    if (-1 < aura_record->damage_target_player)
    {
      if (event == EVENT_POWER)
      {
        if (instance->damage_target_card == affected_card)
        {
          aura_record = in_play(player, card);
          if (aura_record != NULL)
          {
            result =
                is_attacking((int)instance->damage_target_player, instance->damage_target_card);
            if (result == 0)
            {
              event_result = event_result + -2;
            }
            else
            {
              event_result = event_result + 3;
            }
          }
        }
      }
      else if ((event == EVENT_TOUGHNESS) && (instance->damage_target_card == affected_card))
      {
        aura_record = in_play(player, card);
        if (aura_record != NULL)
        {
          result = is_attacking((int)instance->damage_target_player, instance->damage_target_card);
          if (result == 0)
          {
            event_result = event_result + -1;
          }
        }
      }
    }
  }
  return aura(player, card, event, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x215df40; original cards/tempest.c:7356. */
int card_thalakos_dreamsower(int player, int card, event_t event)
{
  /* Thalakos Dreamsower	|2|U
   * Creature - Thalakos Wizard 1/1
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * You may choose not to untap this creature during your untap step.
   * Whenever this creature deals damage to an opponent, tap target creature. That creature doesn't untap during its controller's untap step for as long as this creature remains tapped. */

  int remaining_hits;
  int32_t target_player;
  unsigned int packed_damage;
  card_instance_t *instance;
  card_instance_t *dreamsower;
  int times_damaged[2];
  target_definition_t td;

  shadow(player, card, event);
  choose_to_untap(player, card, event);
  packed_damage =
      whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_DAMAGE_OPPONENT);
  if (packed_damage != 0)
  {
    times_damaged[1] = packed_damage >> 8 & 0xff;
    times_damaged[0] = packed_damage & 0xff;
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    remaining_hits = times_damaged[1 - player];
    while (((remaining_hits != 0 && (can_target(&td) != 0)) &&
            (new_pick_target(&td, "Select target creature to tap.", 0, 0x80) != 0)))
    {
      tap_card(instance->targets[0].player, instance->targets[0].card);
      does_not_untap_until_im_tapped(player, card, instance->targets[0].player,
                                     instance->targets[0].card);
      dreamsower = get_card_instance(player, card);
      remaining_hits = remaining_hits + -1;
      target_player = instance->targets[0].player;
      dreamsower->targets[1].card = instance->targets[0].card;
      dreamsower->targets[1].player = target_player;
    }
    instance->number_of_targets = '\0';
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x215e0a0; original cards/tempest.c:7399. */
int card_thalakos_mistfolk(int player, int card, event_t event)
{
  /* Thalakos Mistfolk	|2|U
   * Creature - Thalakos Illusion 2/1
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * |U: Put this creature on top of its owner's library. */

  card_instance_t *instance;
  card_instance_t *parent_in_play;

  shadow(player, card, event);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    parent_in_play = in_play(instance->parent_controller, instance->parent_card);
    if (parent_in_play != NULL)
    {
      put_on_top_of_deck(instance->parent_controller, instance->parent_card);
    }
  }
  return generic_activated_ability(player, card, event, 0, 0, 0, 1, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x215e190; original cards/tempest.c:7417. */
int card_trumpeting_armodon(int player, int card, event_t event)
{
  /* Trumpeting Armodon	|3|G
   * Creature - Elephant 3/3
   * |1|G: Target creature blocks this creature this turn if able. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (!IS_GAA_EVENT(event))
    return 0;

  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 2);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    target_must_block_me(instance->parent_controller, instance->parent_card,
                         instance->targets[0].player, instance->targets[0].card, 1);
  }
  result = generic_activated_ability(player, card, event, 6, 1, 0, 0, 1, 0, 0, 0, &td,
                                     "Select target creature that will be forced to block this.");
  return result;
}

/* ManalinkEh 2022: 0x215e2f0; original cards/tempest.c:7441. */
int card_twitch(int player, int card, event_t event)
{
  /* Twitch	|2|U
   * Instant
   * You may tap or untap target artifact, creature, or land.
   * Draw a card. */

  if (!IS_GS_EVENT(player, card, event))
    return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_CREATURE | TYPE_LAND);
  td.preferred_controller = ANYBODY;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      twiddle(player, card, 0);
      draw_a_card(player);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  int result = generic_spell(player, card, event, GS_CAN_TARGET, &td, "TWIDDLE", 1, NULL);
  if (event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI && spell_fizzled != 1)
    ai_modifier_twiddle(player, card, 0);
  return result;
}

/* ManalinkEh 2022: 0x215e510; original cards/tempest.c:7485. */
int card_warmth(int player, int card, event_t event)
{
  /* Warmth	|1|W
   * Enchantment
   * Whenever an opponent casts a red spell, you gain 2 life. */

  int result;
  test_definition_t test;

  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 0xff);
    test.color = get_sleighted_color_test(player, card, 0x10);
    sp_ability_trigger(1 - player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA)
  {
    gain_life(player, 2);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x215e5f0; original cards/tempest.c:7504. */
int card_watchdog(int player, int card, event_t event)
{
  /* Watchdog	|3
   * Artifact Creature - Dog 1/2
   * This creature blocks each combat if able.
   * As long as this creature is untapped, all creatures attacking you get -1/-0. */

  card_instance_t *instance;
  int result;

  block_if_able(player, card, event);
  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_tapped(player, card);
    if (result == 0)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (((event == EVENT_POWER) && (result == 0)) && (1 - player == affected_card_controller))
      {
        result = is_attacking(1 - player, affected_card);
        if (result != 0)
        {
          event_result = event_result + -1;
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x215e690; original cards/tempest.c:7528. */
int card_bandage(int player, int card, event_t event)
{
  /* Bandage	|W
   * Instant
   * Prevent the next 1 damage that would be dealt to any target this turn.
   * Draw a card. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    td.preferred_controller = player;
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_ANY", 1, NULL);
    if (result != 0)
    {
      return (((land_can_be_played & 4U) == 0) ? 1U : 99U);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x102);
      td.preferred_controller = player;
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance = get_card_instance(player, card);
        instance->number_of_targets = '\0';
        generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                      "Select any target to prevent the next 1 damage to it", 1, NULL);
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x102);
    td.preferred_controller = player;
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        instance = get_card_instance(player, card);
        prevent_the_next_n_damage(player, card, instance->targets[0].player,
                                  instance->targets[0].card, 1, 0, 0, 0);
        draw_a_card(player);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 0x102);
  }
  return 0;
}
