#include "manalink.h"
static int split_card_impl(int player,int card,event_t event,int id_first_half,int can_play_first_half,int priority_first_half,int id_second_half,int can_play_second_half,int priority_second_half,int can_be_fused);

/* Canonical recovered helper declarations. */
int generic_split_card(int player, int card, event_t event, int id_first_half,
                            int can_play_first_half, int priority_first_half, int id_second_half,
                            int can_play_second_half, int priority_second_half);

// Functions

int generic_protection_from(int player, int card, event_t event, color_test_t cols);

static int split_card_impl(int player, int card, event_t event, int id_first_half,
                           int can_play_first_half, int priority_first_half, int id_second_half,
                           int can_play_second_half, int priority_second_half, int can_be_fused);

static void minus_2_minus_2_creature_as_long_as_color_is_most_common_color(int player, int card,
                                                                           event_t event,
                                                                           color_test_t col);

#ifndef SHANDALAR

static int effect_sacrifice_when_lose_control_then_gain_life_equal_toughness(int player, int card,
                                                                      event_t event);

#endif

#ifndef SHANDALAR

static int effect_defiling_tears(int player, int card, event_t event);

#endif

int do_kicker(int player, int card, int colorless, int black, int blue, int green, int red, int white){
	int diff = 0;
	if( ! played_for_free(player, card) ){
		diff = MIN(0, cards_ptr[get_id(player, card)]->req.req_colorless+true_get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, -1));
	}
	if( colorless != -1 ){
		int cless = MAX(0, colorless+diff);
		if( has_mana_multi(player, cless, black, blue, green, red, white) ){
			int choice = do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass", 0);
			if( choice == 0 ){
				charge_mana_multi(player, cless, black, blue, green, red, white);
				if( spell_fizzled != 1 ){
					set_special_flags(player, card, SF_KICKED);
					return 1;
				}
			}
		}
	}
	else{
		int choice = do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass", 0);
		if( choice == 0 ){
			charge_mana(player, COLOR_COLORLESS, -1);
			if( spell_fizzled != 1 ){
				get_card_instance(player, card)->info_slot = x_value;
				if( diff < 0 ){
					get_card_instance(player, card)->info_slot-=diff;
				}
				set_special_flags(player, card, SF_KICKED);
				return 1;
			}
		}
	}

	return 0;
}

int kicker(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white){
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) && ! check_special_flags(player, card, SF_NOT_CAST) ){
			do_kicker(player, card, colorless, black, blue, green, red, white);
		}
	}
	return 0;
}

int kicked(int player, int card){
	if( check_special_flags(player, card, SF_KICKED) ){
		return 1;
	}
	return 0;
}

/* ManalinkEh 2022: 0x2485f50; original cards/invasion.c:56. */
static int split_card_impl(int player, int card, event_t event, int id_first_half,
                           int can_play_first_half, int priority_first_half, int id_second_half,
                           int can_play_second_half, int priority_second_half, int can_be_fused)
{
  unsigned char first_generic_cost;
  unsigned char second_generic_cost;
  card_ptr_t *first_card_data;
  card_ptr_t *second_card_data;
  card_instance_t *instance;
  unsigned int free_or_first_flag;
  unsigned int selected_modes;
  int first_cost_or_available;
  int second_cost;
  unsigned int stored_mode_flags;

  if (event == EVENT_CAN_CAST)
  {
    instance = get_card_instance(player, card);
    if (((instance->info_slot & 0x100U) == 0) &&
        (first_cost_or_available = played_for_free(player, card), first_cost_or_available == 0))
    {
      return can_play_first_half;
    }
    if (can_play_first_half == 99)
    {
      return 99;
    }
    return can_play_second_half;
  }
  if (event != EVENT_CAST_SPELL)
  {
    if (event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
        event != EVENT_CHANGE_TARGET && event != EVENT_MODIFY_COST)
      return 0;
    instance = get_card_instance(player, card);
    if (event == EVENT_MODIFY_COST)
    {
      if (check_special_flags2(player, card, -0x80000000) != 0)
      {
        return 0;
      }
      first_cost_or_available = has_mana_to_cast_id(player, EVENT_MODIFY_COST, id_second_half);
      if ((can_play_second_half != 0) && (first_cost_or_available != 0))
      {
        null_casting_cost(player, card);
        instance->info_slot = instance->info_slot | 0x100;
        return 0;
      }
      first_cost_or_available = has_mana_to_cast_id(player, EVENT_MODIFY_COST, id_first_half);
      if ((can_play_first_half != 0) && (first_cost_or_available != 0))
      {
        null_casting_cost(player, card);
        return 0;
      }
      instance->info_slot = instance->info_slot & 0xfffffeff;
      return 0;
    }
    return 0;
  }
  if (card != affected_card || player != affected_card_controller)
    return 0;
  instance = get_card_instance(player, card);
  if (card != affected_card || player != affected_card_controller)
    return 0;
  free_or_first_flag = played_for_free(player, card);
  selected_modes = is_token(player, card);
  free_or_first_flag |= selected_modes;
  instance->number_of_targets = 0;
  first_cost_or_available = check_special_flags2(player, card, -0x80000000);
  selected_modes = instance->info_slot;
  if (!first_cost_or_available)
  {
    bool choose_available = false;
    bool choose_dialog = false;
    if (!(selected_modes & 0x100))
    {
      instance->info_slot = 0;
      stored_mode_flags = 0;
      if (!free_or_first_flag)
        selected_modes = 1;
      else
        choose_available = true;
    }
    else
    {
      instance->info_slot = 0x100;
      if (free_or_first_flag)
      {
        stored_mode_flags = 0x100;
        choose_available = true;
      }
      else
      {
        first_cost_or_available = has_mana_to_cast_id(player, EVENT_CAST_SPELL, id_first_half);
        if (!first_cost_or_available || !can_play_first_half)
        {
          stored_mode_flags = instance->info_slot;
          selected_modes = 2;
        }
        else if (has_mana_to_cast_id(player, EVENT_CAST_SPELL, id_second_half))
          choose_dialog = true;
        else
        {
          stored_mode_flags = instance->info_slot;
          selected_modes = 1;
        }
      }
    }
    if (choose_available)
    {
      if (!can_play_first_half)
        selected_modes = 2;
      else
        choose_dialog = true;
    }
    if (choose_dialog)
    {
      if (!can_play_second_half)
      {
        stored_mode_flags = instance->info_slot;
        selected_modes = 1;
      }
      else
      {
        if (can_be_fused == 0)
        {
          selected_modes = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                                       cards_ptr[id_first_half]->name, can_play_first_half,
                                       priority_first_half, cards_ptr[id_second_half]->name,
                                       can_play_second_half, priority_second_half, 0, 0, 0, 0);
        }
        else
        {

          first_cost_or_available = not_played_from_hand(player, card);
          selected_modes = 0;
          if (first_cost_or_available == 0)
          {

            first_card_data = cards_ptr[id_first_half];
            first_generic_cost = first_card_data->req.req_colorless;
            second_card_data = cards_ptr[id_second_half];
            first_cost_or_available = get_updated_casting_cost(
                player, -1, get_internal_card_id_from_csv_id(id_first_half), EVENT_CAST_SPELL,
                (unsigned int)first_generic_cost);
            first_generic_cost = second_card_data->req.req_colorless;
            second_cost = get_updated_casting_cost(
                player, -1, get_internal_card_id_from_csv_id(id_second_half), EVENT_CAST_SPELL,
                (unsigned int)first_generic_cost);
            first_cost_or_available = has_mana_multi(
                player, first_cost_or_available + second_cost,
                (unsigned int)second_card_data->req.req_black +
                    (unsigned int)first_card_data->req.req_black,
                (unsigned int)first_card_data->req.req_blue + (unsigned int)second_card_data->req.req_blue,
                (unsigned int)first_card_data->req.req_green +
                    (unsigned int)second_card_data->req.req_green,
                (unsigned int)first_card_data->req.req_red + (unsigned int)second_card_data->req.req_red,
                (unsigned int)first_card_data->req.req_white +
                    (unsigned int)second_card_data->req.req_white);
            selected_modes = (unsigned int)(first_cost_or_available != 0);
          }
          selected_modes =
              dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                          cards_ptr[id_first_half]->name, can_play_first_half, priority_first_half,
                          cards_ptr[id_second_half]->name, can_play_second_half,
                          priority_second_half, "Fuse", selected_modes, 200, 0, 0, 0, 0);
        }
        if (selected_modes == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        stored_mode_flags = instance->info_slot;
      }
    }
    selected_modes |= stored_mode_flags;
    instance->info_slot = selected_modes;
  }
  if ((selected_modes & 3) == 3)
  {
    first_card_data = cards_ptr[id_first_half];
    first_generic_cost = first_card_data->req.req_colorless;
    second_card_data = cards_ptr[id_second_half];
    second_generic_cost = second_card_data->req.req_colorless;
    first_cost_or_available = get_updated_casting_cost(
        player, -1, get_id(player, card), EVENT_CAST_SPELL,
        (unsigned int)first_generic_cost + (unsigned int)second_generic_cost);
    charge_mana_multi(
        player, first_cost_or_available,
        (unsigned int)first_card_data->req.req_black + (unsigned int)second_card_data->req.req_black,
        (unsigned int)first_card_data->req.req_blue + (unsigned int)second_card_data->req.req_blue,
        (unsigned int)first_card_data->req.req_green + (unsigned int)second_card_data->req.req_green,
        (unsigned int)first_card_data->req.req_red + (unsigned int)second_card_data->req.req_red,
        (unsigned int)first_card_data->req.req_white + (unsigned int)second_card_data->req.req_white);
    if (spell_fizzled == 1)
    {
      instance->info_slot &= 0xfffffffc;
      return 0;
    }
    // Native fused payment reuses this word as the first-half execution flag.
    free_or_first_flag = instance->info_slot & 1;
    if (!free_or_first_flag)
    {
      if (!(instance->info_slot & 2U))
        return 0;
      call_card_function(player, card, EVENT_PLAY_SECOND_HALF);
      if (spell_fizzled == 1)
      {
        instance->info_slot &= 0xfffffffc;
        return 0;
      }
      return 0;
    }
    call_card_function(player, card, EVENT_PLAY_FIRST_HALF);
    if (spell_fizzled == 1)
    {
      instance->info_slot &= 0xfffffffc;
      return 0;
    }
    selected_modes = instance->info_slot;
  }
  else if (selected_modes & 1)
  {
    if (!free_or_first_flag)
    {
      charge_mana_from_id(player, -1, EVENT_CAST_SPELL, id_first_half);
      if (is_x_spell(player, card))
        instance->targets[1].card = x_value;
    }
    if (spell_fizzled == 1)
    {
      instance->info_slot &= 0xfffffffc;
      return 0;
    }
    call_card_function(player, card, EVENT_PLAY_FIRST_HALF);
    if (spell_fizzled == 1)
    {
      instance->info_slot &= 0xfffffffc;
      return 0;
    }
    selected_modes = instance->info_slot;
  }
  if (!(selected_modes & 2))
    return 0;
  if (!free_or_first_flag)
  {
    charge_mana_from_id(player, -1, EVENT_CAST_SPELL, id_second_half);
    if (is_x_spell(player, card))
      instance->targets[1].card = x_value;
  }
  if (spell_fizzled == 1)
  {
    instance->info_slot &= 0xfffffffc;
    return 0;
  }
  call_card_function(player, card, EVENT_PLAY_SECOND_HALF);
  if (spell_fizzled == 1)
    instance->info_slot &= 0xfffffffc;
  return 0;
}

/* ManalinkEh 2022: 0x2488e80; original cards/invasion.c:189. */
int generic_split_card(int player, int card, event_t event, int id_first_half,
                            int can_play_first_half, int priority_first_half, int id_second_half,
                            int can_play_second_half, int priority_second_half)
{
  int result;
  result =
      split_card_impl(player, card, event, id_first_half, can_play_first_half, priority_first_half,
                      id_second_half, can_play_second_half, priority_second_half, 0);
  return result;
}

/* ManalinkEh 2022: 0x2488ee0; original cards/invasion.c:196. */
int split_card_with_fuse(int player, int card, event_t event, int id_first_half,
                         int can_play_first_half, int priority_first_half, int id_second_half,
                         int can_play_second_half, int priority_second_half)
{
  int result;

  result =
      split_card_impl(player, card, event, id_first_half, can_play_first_half, priority_first_half,
                      id_second_half, can_play_second_half, priority_second_half, 1);
  return result;
}

static int invasion_apprentices(int player, int card, event_t event, int clr1, int clr2)
{

  if (!IS_GAA_EVENT(event))
  {
    return 0;
  }

  target_definition_t td1;
  default_target_definition(player, card, &td1, 0);
  td1.zone = TARGET_ZONE_PLAYERS;

  target_definition_t td2;
  default_target_definition(player, card, &td2, TYPE_CREATURE);
  td2.preferred_controller = player;
  td2.allowed_controller = player;

  target_definition_t td3;
  default_target_definition(player, card, &td3, TYPE_CREATURE);
  td3.preferred_controller = player;

  target_definition_t td4;
  default_target_definition(player, card, &td4, TYPE_CREATURE);

  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_CAN_ACTIVATE)
  {
    if ((clr1 == COLOR_BLACK || clr2 == COLOR_BLACK) &&
        generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1),
                                  0, &td1, NULL))
    {
      return 1;
    }
    if ((clr1 == COLOR_BLUE || clr2 == COLOR_BLUE) &&
        generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(1),
                                  0, &td2, NULL))
    {
      return 1;
    }
    if ((clr1 == COLOR_GREEN || clr2 == COLOR_GREEN) &&
        generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_G(1),
                                  0, &td3, NULL))
    {
      return 1;
    }
    if ((clr1 == COLOR_RED || clr2 == COLOR_RED) &&
        generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1),
                                  0, &td3, NULL))
    {
      return 1;
    }
    if ((clr1 == COLOR_WHITE || clr2 == COLOR_WHITE) &&
        generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(1),
                                  0, &td4, NULL))
    {
      return 1;
    }
  }

  if (event == EVENT_ACTIVATE)
  {
    instance->number_of_targets = 0;
    instance->info_slot = 0;
    int abilities[5] = {
        generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET,
                                  MANACOST_B(1), 0, &td1, NULL),
        generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET,
                                  MANACOST_U(1), 0, &td2, NULL),
        generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET,
                                  MANACOST_G(1), 0, &td3, NULL),
        generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET,
                                  MANACOST_R(1), 0, &td3, NULL),
        generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET,
                                  MANACOST_W(1), 0, &td4, NULL)};
    int priorities[5] = {
        life[1 - player] < 6 && would_validate_arbitrary_target(&td1, 1 - player, -1) ? 5 : 15, 8,
        10, 5 + (10 * current_phase == PHASE_AFTER_BLOCKING), 10};

    int choice = DIALOG(
        player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL,
        "Target player loses 1 life", abilities[0] && (clr1 == COLOR_BLACK || clr2 == COLOR_BLACK),
        priorities[0], "Put target creature on top of your deck",
        abilities[1] && (clr1 == COLOR_BLUE || clr2 == COLOR_BLUE), priorities[1],
        "Pump a creature", abilities[2] && (clr1 == COLOR_GREEN || clr2 == COLOR_GREEN),
        priorities[2], "Give first strike",
        abilities[3] && (clr1 == COLOR_RED || clr2 == COLOR_RED), priorities[3],
        "Tap target creature", abilities[4] && (clr1 == COLOR_WHITE || clr2 == COLOR_WHITE),
        priorities[4]);
    if (!choice)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (charge_mana_for_activated_ability(player, card, 0, (choice == 1), (choice == 2),
                                          (choice == 3), (choice == 4), (choice == 5)))
    {
      instance->number_of_targets = 0;
      if (choice == 1)
      {
        pick_target(&td1, "TARGET_PLAYER");
      }
      if (choice == 2)
      {
        new_pick_target(&td2, "Select target creature you control.", 0,
                        GS_CAN_TARGET | GS_LITERAL_PROMPT);
      }
      if (choice == 3)
      {
        pick_target(&td3, "TARGET_CREATURE");
      }
      if (choice == 4)
      {
        pick_target(&td3, "TARGET_CREATURE");
      }
      if (choice == 5)
      {
        pick_target(&td4, "TARGET_CREATURE");
      }
      if (spell_fizzled != 1)
      {
        instance->number_of_targets = 1;
        instance->info_slot = choice;
        tap_card(player, card);
      }
    }
  }

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->info_slot == 1 && valid_target(&td1))
    {
      lose_life(instance->targets[0].player, 1);
    }
    if (instance->info_slot == 2 && valid_target(&td2))
    {
      put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
    }
    if (instance->info_slot == 3 && valid_target(&td3))
    {
      pump_until_eot(instance->parent_controller, instance->parent_card,
                     instance->targets[0].player, instance->targets[0].card, 1, 1);
    }
    if (instance->info_slot == 4 && valid_target(&td3))
    {
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                             instance->targets[0].player, instance->targets[0].card, 0, 0,
                             KEYWORD_FIRST_STRIKE, 0);
    }
    if (instance->info_slot == 5 && valid_target(&td4))
    {
      tap_card(instance->targets[0].player, instance->targets[0].card);
    }
  }

  return 0;
}

static int mutation_spell(int player, int card, event_t event, int type, const char *buffer){

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, type);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int cmc = get_cmc(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			generate_tokens_by_id(player, card, CARD_ID_SAPROLING, cmc);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, buffer, 1, NULL);
}

static int invasion_masters(int player, int card, event_t event, int clr1, int clr2){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.zone = TARGET_ZONE_PLAYERS;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.preferred_controller = player;

	target_definition_t td3;
	default_target_definition(player, card, &td3, TYPE_CREATURE);


	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( (clr1 == COLOR_BLACK || clr2 == COLOR_BLACK) &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(2), 0, &td1, NULL)
		  ){
			return 1;
		}
		if( (clr1 == COLOR_BLUE || clr2 == COLOR_BLUE) &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(2), 0, &td3, NULL)
		  ){
			return 1;
		}
		if( (clr1 == COLOR_GREEN || clr2 == COLOR_GREEN ) && generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_G(2), 0, NULL, NULL) ){
			return 1;
		}
		if( (clr1 == COLOR_RED || clr2 == COLOR_RED ) &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(2), 0, &td3, NULL)
		  ){
			return 1;
		}
		if( (clr1 == COLOR_WHITE || clr2 == COLOR_WHITE) &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(2), 0, &td2, NULL)
		  ){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		instance->info_slot = 0;
		int abilities[5] = {generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(2), 0, &td1, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(2), 0, &td3, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_G(2), 0,  NULL, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(2), 0, &td3, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(2), 0, &td2, NULL)
							};
		int priorities[5] = {	life[1-player] < 6 && would_validate_arbitrary_target(&td1, 1-player, -1) ? 5 : 15,
								8,
								current_phase <= PHASE_AFTER_BLOCKING ? 10 : 5,
								8,
								8
							};

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL,
							"Target player loses 2 life", abilities[0] && (clr1 == COLOR_BLACK || clr2 == COLOR_BLACK), priorities[0],
							"Bounce a creature", abilities[1] && (clr1 == COLOR_BLUE || clr2 == COLOR_BLUE), priorities[1],
							"Pump all your creatures", abilities[2] && (clr1 == COLOR_GREEN || clr2 == COLOR_GREEN), priorities[2],
							"Deal 2 damages", abilities[3] && (clr1 == COLOR_RED || clr2 == COLOR_RED), priorities[3],
							"Give protection", abilities[4] && (clr1 == COLOR_WHITE || clr2 == COLOR_WHITE), priorities[4]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, 0,
															(choice == 1 ? 2 : 0),
															(choice == 2 ? 2 : 0),
															(choice == 3 ? 2 : 0),
															(choice == 4 ? 2 : 0),
															(choice == 5 ? 2 : 0)
															)
		  ){
			if( choice == 1 ){
				pick_target(&td1, "TARGET_PLAYER");
			}
			if( choice == 2 ){
				pick_target(&td3, "TARGET_CREATURE");
			}
			if( choice == 4 ){
				pick_target(&td3, "TARGET_CREATURE");
			}
			if( choice == 5 ){
				pick_target(&td2, "TARGET_CREATURE");
			}
			if( spell_fizzled != 1 ){
				instance->number_of_targets = 1;
				instance->info_slot = choice;
				tap_card(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 && valid_target(&td1) ){
			lose_life(instance->targets[0].player, 2);
			gain_life(player, 2);
		}
		if( instance->info_slot == 2 && valid_target(&td3) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		if( instance->info_slot == 3 ){
			pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1, 2, 2, 0, 0);
		}
		if( instance->info_slot == 4 && valid_target(&td3) ){
			damage_target0(player, card, 2);
		}
		if( instance->info_slot == 5 && valid_target(&td2) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
									0, 0, select_a_protection(player), 0);
		}
	}

	return 0;
}

void spell_can_be_cast_as_instant_by_paying_2(int player, int card, event_t event){
	if( event == EVENT_MODIFY_COST ){
		if( ! can_sorcery_be_played(player, event) ){
			COST_COLORLESS+=2;
		}
		else{
			get_card_instance(player, card)->info_slot = 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( get_card_instance(player, card)->info_slot == 0 && player != AI && has_mana(player, COLOR_ANY, 2) ){
			int choice = do_dialog(player, player, card, -1, -1, " Play as Sorcery\n Play as Instant\n Cancel", 0);
			if( choice == 1 ){
				charge_mana(player, COLOR_COLORLESS, 2);
			}
		}
	}
}

static void invasion_dragon(int player, int card, event_t event, int cless, int black, int blue,
                            int green, int red, int white)
{

  if (damage_dealt_by_me(player, card, event,
                         DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS |
                             DDBM_MUST_BE_COMBAT_DAMAGE))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int times_damaged[2] = {BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player)};
    instance->targets[1].player = 0;
    APNAP(p, {
      if (times_damaged[p])
      {
        char buffer[100];
        if (p == player)
        {
          scnprintf(buffer, 100, " Activate (you were damaged)\n Cancel");
        }
        else
        {
          scnprintf(buffer, 100, " Activate (opponent was damaged)\n Cancel");
        }
        while (times_damaged[p] && has_mana_multi(player, cless, black, blue, green, red, white))
        {
          int choice = do_dialog(player, player, card, -1, -1, buffer, p == player);
          if (choice == 0)
          {
            charge_mana_multi(player, cless, black, blue, green, red, white);
            if (spell_fizzled != 1)
            {
              SET_BYTE1(instance->info_slot) = p;
              call_card_function(player, card, EVENT_DAMAGE_TRIGGER_ABILITY);
              instance->info_slot = 0;
            }
          }
          times_damaged[p]--;
        }
      }
    };);
  }
}

// Cards

int card_absorb(int player, int card, event_t event){
  /* Absorb	|W|U|U
   * Instant
   * Counter target spell. You gain 3 life. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			gain_life(player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 0, NULL);
}

int card_addle(int player, int card, event_t event){

	/* Addle	|1|B
	 * Sorcery
	 * Choose a color. Target player reveals his or her hand and you choose a card of that color from it. That player discards that card. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int clr = 1<<choose_a_color(player, get_deck_color(player, 1-player));
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			this_test.color = clr;

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_aether_rift(int player, int card, event_t event){
  /* Aether Rift	|1|R|G
   * Enchantment
   * At the beginning of your upkeep, discard a card at random. If you discard a creature card this way, return it from your graveyard to the battlefield unless any player pays 5 life. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int chosen = get_random_card_in_hand(player);
		if( chosen > -1 ){
			int id = -1;
			if( is_what(player, chosen, TYPE_CREATURE) ){
				id = get_id(player, chosen);
			}
			discard_card(player, chosen);
			if( id > -1 ){
				int result = 0;
				APNAP(p, {
							int choice = 0;
							if( can_pay_life(p, 5) ){
								int ai_choice = 1;
								if( life[p]-5 < 6 ){
									ai_choice = 0;
								}
								char buffer[500];
								card_ptr_t* c = cards_ptr[ id ];
								scnprintf(buffer, 500, " Make %s return\n Pay 5 life", c->name);
								choice = do_dialog(p, player, card, -1, -1, buffer, ai_choice);
							}
							if( choice == 0 ){
								result++;
							}
							else{
								lose_life(p, 5);
								result--;
							}
						};
				);
				if( result == 2 ){
					seek_grave_for_id_to_reanimate(player, card, player, id, REANIMATE_DEFAULT);
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2489500; original cards/invasion.c:641. */
int card_aggressive_urge(int player, int card, event_t event)
{
  /* Aggressive Urge	|1|G
   * Instant
   * Target creature gets +1/+1 until end of turn.
   * Draw a card. */

  int result;
  result = vanilla_instant_pump(player, card, event, 2, player, 1, 1, 0x1000000, 0);
  return result;
}

int card_ancient_spring(int player, int card, event_t event){
  /* Ancient Spring	""
   * Land
   * This land enters tapped.
   * |T: Add |U.
   * |T, Sacrifice this land: Add |W|B. */

	return sac_land_tapped(player, card, event, COLOR_BLUE, COLOR_WHITE, COLOR_BLACK);
}

int card_angelic_shield(int player, int card, event_t event){
  /* Angelic Shield	|W|U
   * Enchantment
   * Creatures you control get +0/+1.
   * Sacrifice this enchantment: Return target creature to its owner's hand. */

	boost_creature_type(player, card, event, -1, 0, 1, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);

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

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_armadillo_cloack(int player, int card, event_t event){
  /* Armadillo Cloak	|1|G|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+2 and has trample.
   * Whenever enchanted creature deals damage, you gain that much life. */

	if (in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		card_instance_t *instance = get_card_instance(player, card);
		if (instance->damage_target_player >= 0 && instance->damage_target_card >= 0){
			spirit_link_effect(instance->damage_target_player, instance->damage_target_card, event, player);
		}
	}

	return generic_aura(player, card, event, player, 2, 2, KEYWORD_TRAMPLE, 0, 0, 0, 0);
}

int card_artifact_mutation(int player, int card, event_t event){
	/* Artifact Mutation	|R|G
	 * Instant
	 * Destroy target artifact. It can't be regenerated. Put X 1/1 |Sgreen Saproling creature tokens onto the battlefield, where X is that artifact's converted mana cost. */

	return mutation_spell(player, card, event, TYPE_ARTIFACT, "TARGET_ARTIFACT");
}

int card_assault_battery(int player, int card, event_t event)
{
  /* Assault // Battery	|R|/|3|G	0x200887e
   * Sorcery // Sorcery
   * Assault deals 2 damage to any target.
   * //
   * Create a 3/3 |Sgreen Elephant creature token. */

  card_instance_t *instance;
  unsigned int selected_modes;
  int result;
  target_definition_t td;

  if (event == EVENT_MODIFY_COST)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    result = can_target(&td);
    return split_card_impl(player, card, EVENT_MODIFY_COST, 0x4acc, result, 0, 0x4acf, 1, 0, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    pick_target(&td, "TARGET_ANY");
    return 0;
  }
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    selected_modes = instance->info_slot;
    if ((selected_modes & 1) != 0)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        damage_target0(player, card, 2);
      }
      selected_modes = instance->info_slot;
    }
    if ((selected_modes & 2) != 0)
    {
      generate_token_by_id(player, card, CARD_ID_ELEPHANT);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = can_target(&td);
  return split_card_impl(player, card, event, 0x4acc, result, 10, 0x4acf, 1, 5, 0);
}

int card_atalya_samite_master(int player, int card, event_t event){
  /* Atalya, Samite Master	|3|W|W
   * Legendary Creature - Human Cleric 2/3
   * |X, |T: Choose one -
   * * Prevent the next X damage that would be dealt to target creature this turn. Spend only white mana on X.
   * * You gain X life. Spend only white mana on X. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.extra = damage_card;
	td1.special = TARGET_SPECIAL_DAMAGE_CREATURE;
	td1.required_type = 0;

	card_instance_t *instance = get_card_instance(player, card);

	check_legend_rule(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DAMAGE_PREVENTION_CREATURE | GAA_CAN_TARGET,
												MANACOST_W(1), 0, &td1, NULL);
		if( result ){
			return result;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_W(1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int choice = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_DAMAGE_PREVENTION_CREATURE | GAA_CAN_TARGET,
										MANACOST0, 0, &td1, NULL) )
		{
			int ai_choice = life[player] < 6 ? 1 : 0;
			choice = do_dialog(player, player, card, -1, -1, " Prevent damage to a creature\n Gain life\n Cancel", ai_choice);
		}
		else{
			choice = 1;
		}
		if( choice == 2 ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			charge_mana(player, COLOR_WHITE, -1);
			if( spell_fizzled != 1 ){
				instance->info_slot = x_value;
				if( choice == 0 ){
					if( new_pick_target(&td1, "Select a damage card which targets a creature.", 0, 1 | GS_LITERAL_PROMPT) ){
						instance->targets[1].player = 66+choice;
						tap_card(player, card);
					}
				}
				if( choice == 1 ){
					instance->targets[1].player = 66+choice;
					tap_card(player, card);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->targets[1].player == 66 && valid_target(&td1) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, instance->info_slot);
		}
		if( instance->targets[1].player == 67){
			gain_life(player, instance->info_slot);
		}
	}

	return 0;
}

int card_aura_mutation(int player, int card, event_t event){
	/* Aura Mutation	|G|W
	 * Instant
	 * Destroy target enchantment. Put X 1/1 |Sgreen Saproling creature tokens onto the battlefield, where X is that enchantment's converted mana cost. */

	return mutation_spell(player, card, event, TYPE_ENCHANTMENT, "TARGET_ENCHANTMENT");
}

int card_aura_shards(int player, int card, event_t event){
  /* Aura Shards |1|W|G
   * Enchantment
   * Whenever a creature enters the battlefield under your control, you may destroy target artifact or enchantment. */

	etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), TYPE_CREATURE, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "DISENCHANT") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_backlash(int player, int card, event_t event){
  /* Backlash	|1|B|R
   * Instant
   * Tap target untapped creature. That creature deals damage equal to its power to its controller. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
			int dmg = get_power(instance->targets[0].player, instance->targets[0].card);
			damage_player(instance->targets[0].player, dmg, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target untapped creature.", 1, NULL);
}

int card_barrins_spite(int player, int card, event_t event){
  /* Barrin's Spite	|2|U|B
   * Sorcery
   * Choose two target creatures controlled by the same player. Their controller chooses and sacrifices one of them. Return the other to its owner's hand. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, 0, NULL, NULL, 0, NULL) ){
			td.allowed_controller = 1-player;
			td.preferred_controller = 1-player;
			if( target_available(player, card, &td) > 1 ){
				return 1;
			}
			td.allowed_controller = player;
			td.preferred_controller = player;
			if( target_available(player, card, &td) > 1 ){
				return 1;
			}
			default_target_definition(player, card, &td, TYPE_CREATURE);
		}
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_CREATURE") ){
			td.allowed_controller = instance->targets[0].player;
			td.preferred_controller = instance->targets[0].player;
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			if( can_target(&td) ){
				new_pick_target(&td, "TARGET_CREATURE", 1, 1);
			} else {
				cancel = 1;
			}
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}
	if(event == EVENT_RESOLVE_SPELL ){
		int legal_targets = validate_target(player, card, &td, 0) | (2*validate_target(player, card, &td, 1));
		if( legal_targets ){
			if( legal_targets & 1 ){
				if( legal_targets & 2 ){
					int p, c;
					for(p=0; p<2; p++){
						for(c=0; c<active_cards_count[p]; c++){
							if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
								state_untargettable(p, c, 1);
							}
						}
					}
					state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
					state_untargettable(instance->targets[1].player, instance->targets[1].card, 0);

					target_definition_t td2;
					default_target_definition(player, card, &td2, TYPE_CREATURE);
					td2.allowed_controller = instance->targets[0].player;
					td2.preferred_controller = instance->targets[0].player;
					td2.who_chooses = instance->targets[0].player;
					td2.allow_cancel = 0;

					if( new_pick_target(&td2, "Select the creature to sacrifice.", 2, GS_LITERAL_PROMPT) ){
						if( instance->targets[0].player == instance->targets[2].player && instance->targets[0].card == instance->targets[2].card ){
							state_untargettable(instance->targets[1].player, instance->targets[1].card, 1);
							if( can_sacrifice(player, instance->targets[0].player, 1, TYPE_CREATURE, 0) ){
								kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
							}
							else{
								state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
							}
							bounce_permanent(instance->targets[1].player, instance->targets[1].card);
						}
						else{
							state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
							if( can_sacrifice(player, instance->targets[1].player, 1, TYPE_CREATURE, 0) ){
								kill_card(instance->targets[1].player, instance->targets[1].card, KILL_SACRIFICE);
							}
							else{
								state_untargettable(instance->targets[1].player, instance->targets[1].card, 1);
							}
							bounce_permanent(instance->targets[0].player, instance->targets[0].card);
						}
					}
					for(p=0; p<2; p++){
						for(c=0; c<active_cards_count[p]; c++){
							if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
								state_untargettable(p, c, 0);
							}
						}
					}
				}
			}
			else{
				if( can_sacrifice(player, instance->targets[1].player, 1, TYPE_CREATURE, 0) ){
					kill_card(instance->targets[1].player, instance->targets[1].card, KILL_SACRIFICE);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_benalish_heralds(int player, int card, event_t event){
  /* Benalish Heralds	|3|W
   * Creature - Human Soldier 2/4
   * |3|U, |T: Draw a card. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XU(3, 1), 0, NULL, NULL);
}

int card_bind2(int player, int card, event_t event)
{
  /* Bind	|1|G
   * Instant
   * Counter target activated ability. (Mana abilities can't be targeted.)
   * Draw a card. */

  if (event == EVENT_CAN_CAST)
	return can_counter_activated_ability(player, card, event, NULL);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  get_card_instance(player, card)->number_of_targets = 0;
	  return cast_counter_activated_ability(player, card, 0);
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (resolve_counter_activated_ability(player, card, NULL, 0))
		draw_cards(player, 1);
	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_blazing_specter(int player, int card, event_t event)
{
  /* Blazing Specter	|2|B|R
   * Creature - Specter 2/2
   * Flying, haste
   * Whenever ~ deals combat damage to a player, that player discards a card. */

  //0x4d8fd0



  haste(player, card, event);
  whenever_i_deal_damage_to_a_player_he_discards_a_card(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE, 0);
  return 0;
}

int card_bloodstone_cameo(int player, int card, event_t event){
  /* Bloodstone Cameo	|3
   * Artifact
   * |T: Add |B or |R. */

	return mana_producer(player, card, event);
}

int card_blurred_mangoose(int player, int card, event_t event){
  /* Blurred Mongoose	|1|G
   * Creature - Mongoose 2/1
   * This spell can't be countered.
   * Shroud (This creature can't be the target of spells or abilities.) */

	cannot_be_countered(player, card, event);
	return flash(player, card, event);
}

int card_breaking_wave(int player, int card, event_t event)
{
  /* Breaking Wave	|2|U|U
   * Sorcery
   * You may cast ~ any time you could cast an instant if you pay |2 more to cast it.
   * Simultaneously untap all tapped creatures and tap all untapped creatures. */

	spell_can_be_cast_as_instant_by_paying_2(player, card, event);

  if (event == EVENT_CAN_CAST)
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  char tapped[2][151] = {{0}};
	  int p, c;
	  for (p = 0; p <= 1; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if (in_play(p, c) && is_what(p, c, TYPE_CREATURE))
			tapped[p][c] = is_tapped(p, c) ? 1 : 2;

	  APNAP(pl,
			{
			  for (c = 0; c < active_cards_count[pl]; ++c)
				if (tapped[pl][c] && in_play(pl, c))
				  {
					if (tapped[pl][c] == 1)
					  untap_card(pl, c);
					else
					  tap_card(pl, c);
				  }
			});

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_breath_of_darigaaz(int player, int card, event_t event){
  /* Breath of Darigaaz	|1|R
   * Sorcery
   * Kicker |2 (You may pay an additional |2 as you cast this spell.)
   * ~ deals 1 damage to each creature without flying and each player. If this spell was kicked, it deals 4 damage to each creature without flying and each player instead. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int dmg = kicked(player, card) ? 4 : 1;
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		this_test.keyword_flag = 1;
		new_damage_all(player, card, ANYBODY, dmg, NDA_PLAYER_TOO, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return kicker(player, card, event, MANACOST_X(2));
}

int card_canopy_surge(int player, int card, event_t event){
  /* Canopy Surge	|1|G
   * Sorcery
   * Kicker |2 (You may pay an additional |2 as you cast this spell.)
   * ~ deals 1 damage to each creature with flying and each player. If this spell was kicked, it deals 4 damage to each creature with flying and each player instead. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int dmg = kicked(player, card) ? 4 : 1;
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		new_damage_all(player, card, 2, dmg, NDA_PLAYER_TOO, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return kicker(player, card, event, MANACOST_X(2));
}

int card_captain_sisay(int player, int card, event_t event){

	/* Captain Sisay	|2|G|W
	 * Legendary Creature - Human Soldier 2/2
	 * |T: Search your library for a legendary card, reveal that card, and put it into your hand. Then shuffle your library. */

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION  ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		new_default_test_definition(&this_test, 0, "Select a legendary card.");
		this_test.subtype = SUBTYPE_LEGEND;

		card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
		parent->targets[2].card = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DECK_SEARCHER, MANACOST0, 0, NULL, NULL);
}

static int cauldron_dance_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		haste(p, c, event);

		if( eot_trigger(player, card, event) ){
			if( ! check_state(p, c, STATE_OUBLIETTED) ){
				bounce_permanent(p, c);
			}
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_cauldron_dance(int player, int card, event_t event){
  /* Cauldron Dance	|4|B|R
   * Instant
   * Cast this spell only during combat.
   * Return target creature card from your graveyard to the battlefield. That creature gains haste. Return it to your hand at the beginning of the next end step.
   * You may put a creature card from your hand onto the battlefield. That creature gains haste. Its controller sacrifices it at the beginning of the next end step. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	if( event == EVENT_CAN_CAST && current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2 ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 0, &this_test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 0, &this_test);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected > -1 ){
			int zombo = reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
			if( zombo > -1 ){
				create_targetted_legacy_effect(player, card, &cauldron_dance_legacy, player, zombo);
			}
			this_test.zone = TARGET_ZONE_HAND;
			if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
				int zombo2 = new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
				if( zombo2 > -1  ){
					int legacy = haste_and_remove_at_beginning_of_next_end_step(player, card, player, zombo2, KILL_SACRIFICE);
					get_card_instance(player, legacy)->number_of_targets = 1;
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_charging_troll(int player, int card, event_t event)
{
  /* Charging Troll	|2|G|W
   * Creature - Troll 3/3
   * Vigilance
   * |G: Regenerate ~. */

  //0x1205390



  vigilance(player, card, event);
  return regeneration(player, card, event, MANACOST_G(1));
}

int card_chromatic_sphere(int player, int card, event_t event)
{
  /* Chromatic Sphere	|1
   * Artifact
   * |1, |T, Sacrifice this artifact: Add one mana of any color. Draw a card. (Activate only as an instant.) */

	if (event == EVENT_COUNT_MANA && affect_me(player, card)
	    && !is_tapped(player, card) && !is_animated_and_sick(player, card)
	    && can_produce_mana(player, card) && can_sacrifice_this_as_cost(player, card)
	    && has_mana_for_activated_ability(player, card, MANACOST_X(1))){
	    declare_mana_available_hex(player, COLOR_TEST_ANY_COLORED, 1);
	    declare_mana_available(player, COLOR_COLORLESS, -1);
	}
	if (event == EVENT_CAN_ACTIVATE){
	    return !is_tapped(player, card) && !is_animated_and_sick(player, card)
	        && can_produce_mana(player, card)
	        && has_mana_for_activated_ability(player, card, MANACOST_X(1))
	        && get_card_instance(player, card)->targets[1].player != 1
	        && can_sacrifice_this_as_cost(player, card);
	}
	if (event == EVENT_ACTIVATE){
	    card_instance_t* instance = get_card_instance(player, card);
	    color_t color = choose_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1, "What kind of mana?", COLOR_TEST_COLORLESS);
	    if (color == -1){ spell_fizzled = 1; return 0; }
	    instance->targets[1].player = 1;
	    charge_mana_for_activated_ability(player, card, MANACOST_X(1));
	    instance->targets[1].player = 0;
	    if (spell_fizzled != 1){
	        kill_card(player, card, KILL_SACRIFICE);
	        produce_mana_tapped(player, card, color, 1);
	        draw_a_card(player);
	    }
	}
	return 0;
}

int card_cinder_shade(int player, int card, event_t event){
  /* Cinder Shade	|1|B|R
   * Creature - Shade 1/1
   * |B: This creature gets +1/+1 until end of turn.
   * |R, Sacrifice this creature: It deals damage equal to its power to target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return generic_shade(player, card, event, 0, MANACOST_B(1), 1, 1, 0, 0);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_R(1), 0, &td, NULL) ){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		int choice = instance->info_slot = instance->number_of_targets = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_B(1), 0, NULL, NULL) ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_R(1), 0, &td, NULL) ){
				int ai_choice = current_phase == PHASE_AFTER_BLOCKING ? 0 : 1;
				choice = do_dialog(player, player, card, -1, -1, " Pump\n Sac & damage\n Cancel", ai_choice);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 0 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_B(1)) ){
				instance->info_slot = -1;
			}
		}
		else if( choice == 1 ){
				charge_mana_for_activated_ability(player, card, MANACOST_R(1));
				if( spell_fizzled != 1 && pick_target(&td, "TARGET_CREATURE") ){
					instance->info_slot = get_power(player, card);
					kill_card(player, card, KILL_SACRIFICE);
				}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot < 0 ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 1, 1);
		}
		else{
			if( valid_target(&td) ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, instance->info_slot, player, card);
			}
		}
	}

	return 0;
}

int card_coalition_victory(int player, int card, event_t event){
  /* Coalition Victory	|3|W|U|B|R|G
   * Sorcery
   * You win the game if you control a land of each basic land type and a creature of each color. */

	if( event == EVENT_RESOLVE_SPELL ){
		int you_win = 0;

		if( count_domain(player, card) == 5 ){
			int c;
			int global_colors = 0;
			for(c=0; c<active_cards_count[player]; c++){
				if( in_play(player, c) && is_what(player, c, TYPE_CREATURE) ){
					global_colors |= get_color(player, c);
					global_colors &= ~COLOR_TEST_COLORLESS;
					if( (global_colors & COLOR_TEST_ANY_COLORED) == COLOR_TEST_ANY_COLORED ){
						you_win = 1;
						break;
					}
				}
			}
		}

		if( you_win ){
			lose_the_game(1-player);
		}

		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_collapsing_borders(int player, int card, event_t event){
  /* Collapsing Borders	|3|R
   * Enchantment
   * Domain - At the beginning of each player's upkeep, that player gains 1 life for each basic land type among lands they control. Then this enchantment deals 3 damage to that player. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		gain_life(current_turn, count_domain(current_turn, card));
		damage_player(current_turn, 3, player, card);
	}

	return global_enchantment(player, card, event);
}

int card_collective_restraint(int player, int card, event_t event){
  /* Collective Restraint	|3|U
   * Enchantment
   * Domain - Creatures can't attack you unless their controller pays |X for each creature they control that's attacking you, where X is the number of basic land types among lands you control. */

	tax_attack(player, card, event );
	return global_enchantment(player, card, event);
}

int card_crosis_the_purger(int player, int card, event_t event){
  /* Crosis, the Purger |3|U|B|R
   * Legendary Creature — Dragon 6/6
   * Flying
   * Whenever Crosis, the Purger deals combat damage to a player, you may pay {2}{B}.
   * If you do, choose a color, then that player reveals his or her hand and discards all cards of that color. */

	check_legend_rule(player, card, event);

	invasion_dragon(player, card, event, MANACOST_XB(2, 1));

	if( event == EVENT_DAMAGE_TRIGGER_ABILITY ){
		int t_player = BYTE1(get_card_instance(player, card)->info_slot);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		this_test.color = 1<<choose_a_color(player, get_deck_color(player, t_player));

		ec_definition_t this_definition;
		default_ec_definition(t_player, player, &this_definition);
		this_definition.effect = EC_ALL_WHICH_MATCH_CRITERIA | EC_DISCARD;
		new_effect_coercion(&this_definition, &this_test);
	}

	return 0;
}

int card_crypt_angel(int player, int card, event_t event){
/* CARD_ID_CRYPT_ANGEL	4972
Crypt Angel	|4|B
Creature - Angel 3/3
Flying, protection from |Swhite
When ~ enters the battlefield, return target |Sblue or |Sred creature card from your graveyard to your hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		char msg[100];
		int pos = scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select target %s", COLOR_BLUE));
		scnprintf(msg + pos, 100 - pos, "%s", get_sleighted_color_text(player, card, " or %s creature card.", COLOR_RED));
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, msg);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE | COLOR_TEST_RED);
		if( new_special_count_grave(player, &this_test) && ! graveyard_has_shroud(player) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 1, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

int card_darigaaz_the_igniter(int player, int card, event_t event){
  /* Darigaaz, the Igniter |3|B|R|G
   * Legendary Creature — Dragon 6/6
   * Flying
   * Whenever Darigaaz, the Igniter deals combat damage to a player, you may pay {2}{R}.
   * If you do, choose a color, then that player reveals his or her hand and Darigaaz deals damage to the player equal to the number of cards
   * 	of that color revealed this way. */

	check_legend_rule(player, card, event);

	invasion_dragon(player, card, event, MANACOST_XR(2, 1));

	if( event == EVENT_DAMAGE_TRIGGER_ABILITY ){
		int t_player = BYTE1(get_card_instance(player, card)->info_slot);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		this_test.color = 1<<choose_a_color(player, get_deck_color(player, t_player));
		this_test.zone = TARGET_ZONE_HAND;

		reveal_target_player_hand(t_player);

		int amount = check_battlefield_for_special_card(player, card, t_player, CBFSC_GET_COUNT, &this_test);
		if( amount > 0 ){
			damage_player(t_player, amount, player, card);
		}
	}

	return 0;
}

int card_death_or_glory(int player, int card, event_t event)
{
  /* Death or Glory	|4|W
   * Sorcery
   * Separate all creature cards in your graveyard into two piles. Exile the pile of an opponent's choice and return the other to the battlefield. */

	if (event == EVENT_RESOLVE_SPELL)
	  {
		/* Move creatures into local dead[] array and obliterate from graveyard, to prevent interactions with creatures that manipulate the graveyard as they
		 * enter the battlefield (such as Gravedigger, Angel of Glory's Rise, Sutured Ghoul, etc.).  This causes problems with Grafdigger's Cage - the creatures
		 * will get moved to the top of their graveyard.  On the other hand, it's only one card.  We could also work around it directly here, by adding another
		 * code path if there's a Cage on the battlefield - don't obliterate cards early, and keep track of the graveyard indices when building piles so the
		 * right ones can be exiled.  Let's wait on that until someone complains.  And at that point, probably better to fix reanimate_all() to work right with
		 * Gravedigger-like cards. */
		int dead[500];
		int dead_count = 0;
		const int* grave = get_grave(player);
		int i;
		for (i = 0; i < 500 && grave[i] != -1;)
		  if (is_what(-1, grave[i], TYPE_CREATURE))
			{
			  dead[dead_count] = grave[i];
			  ++dead_count;
			  obliterate_card_in_grave(player, i);
			}
		  else
			++i;

		if (dead_count > 0)
		  {
			int piles[2][dead_count];

			separate_into_two_piles(player, dead, dead_count, piles);

			int chosen = choose_between_two_piles(1-player, dead_count, piles, AI_MAX_VALUE, "Exile these");

			// Exile the chosen pile
			int any_exiled = 0;
			int* rfg = rfg_ptr[player];
			int rfg_pos = count_rfg(player);
			for (i = 0; i < dead_count; ++i)
			  if (piles[chosen][i] != iid_draw_a_card && piles[chosen][i] != -1)
				{
				  rfg[rfg_pos] = piles[chosen][i];
				  ++rfg_pos;
				  any_exiled = 1;
				}
			if (any_exiled)
			  play_sound_effect(WAV_DESTROY);

			// Now add the non-chosen pile to graveyard and reanimate them, one at a time
			for (i = 0; i < dead_count; ++i)
			  if (piles[1 - chosen][i] != iid_draw_a_card && piles[1 - chosen][i] != -1)
				{
				  int pos = raw_put_iid_on_top_of_graveyard(player, piles[1 - chosen][i]);
				  increase_trap_condition(player, TRAP_CARDS_TO_GY_FROM_ANYWHERE, -1);	// Since not really putting in graveyard
				  reanimate_permanent(player, card, player, pos, REANIMATE_DEFAULT);
				}
		  }

		kill_card(player, card, KILL_DESTROY);
	  }

	return basic_spell(player, card, event);
}

int card_desperate_research(int player, int card, event_t event){
  /* Desperate Research |1|B
   * Sorcery
   * Name a card other than a basic land card.
   * Then reveal the top seven cards of your library and put all of them with that name into your hand. Exile the rest. */

	if (event == EVENT_RESOLVE_SPELL){
		int* deck = deck_ptr[player];
		int deck_count = count_deck(player);
		int csvid;
		test_definition_t test;
		if (player == AI){
			default_test_definition(&test, TYPE_ANY);
			test.subtype = SUBTYPE_BASIC;
			test.subtype_flag = DOESNT_MATCH;
			csvid = ai_name_a_random_csvid(player, card, player, TYPE_NONE, &test);
		}else{
			new_default_test_definition(&test, TYPE_ANY, "Choose a card name other than a basic land card name.");
			test.subtype = SUBTYPE_BASIC;
			test.subtype_flag = DOESNT_MATCH;
			csvid = new_card_from_list(player, 3, &test);
		}
		if (csvid != -1 && deck_count != 0){
			int amount = MIN(7, deck_count);
			show_deck(player, deck, amount, "Cards revealed by Desperate Research", 0, gs_done_008b40e0);
			for (int i = 0; i < amount; ++i){
				int iid = deck[0];
				if (cards_data[iid].id == csvid){ add_card_to_hand(player, iid); remove_card_from_deck(player, 0); }
				else{ rfg_top_n_cards_of_deck(player, 1); }
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

int card_dismantling_blow(int player, int card, event_t event){

	/* Dismantling Blow	|2|W
	 * Instant
	 * Kicker |2|U
	 * Destroy target artifact or enchantment.
	 * If ~ was kicked, draw two cards. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
		if( spell_fizzled != 1 ){
			do_kicker(player, card, MANACOST_XU(2, 1));
		}
	}
	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if( kicked(player, card) ){
				draw_cards(player, 2);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_devouring_strossus(int player, int card, event_t event){
/* CARD_ID_DEVOURING_STROSSUS	4979
Devouring Strossus	|5|B|B|B
Creature - Horror 9/9
Flying, trample
At the beginning of your upkeep, sacrifice a creature.
Sacrifice a creature: Regenerate ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return regeneration(player, card, event, MANACOST0);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE | GAA_REGENERATION, MANACOST0, 0, NULL, NULL);
}

int card_distorting_wake(int player, int card, event_t event){
/* CARD_ID_DISTORTING_WAKE	4981
Distorting Wake	|X|U|U|U
Sorcery
Return X target nonland permanents to their owners' hands. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_type = TYPE_LAND;
	td.allow_cancel = 2; //"Done" button

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, NULL, 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int trgs = instance->number_of_targets = 0;
		int max_targets = has_mana(player, COLOR_ANY, -1) + true_get_updated_casting_cost(player, card, -1, event, -1);
		while( can_target(&td) && trgs < max_targets ){
				if( new_pick_target(&td, "Select target nonland permanent.", trgs, 0 | GS_LITERAL_PROMPT) ){
					state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
					trgs++;
				}
				else{
					break;
				}
		}
		int i;
		for(i=0; i<trgs; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
		}
		int mana_to_charge = trgs + true_get_updated_casting_cost(player, card, -1, event, -1);
		charge_mana(player, COLOR_ANY, mana_to_charge);
		if( spell_fizzled != 1 ){
			instance->info_slot = trgs;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				bounce_permanent(instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

/* ManalinkEh 2022: 0x248cb80; original cards/invasion.c:1751. */
int card_dream_thrush(int player, int card, event_t event)
{
  /* Dream Thrush	|1|U
   * Creature - Bird 1/1
   * Flying
   * |T: Target land becomes the basic land type of your choice until end of turn. */

  card_instance_t *instance;
  int result;
  int legacy_card;
  target_definition_t td;

  if (!IS_GAA_EVENT(event))
    return 0;

  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 1);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    result = recorded_rand(player, 5);
    result = choose_a_land(player, result + 1);
    legacy_card = create_targetted_legacy_effect(
        player, card, fx_change_landtype, instance->targets[0].player, instance->targets[0].card);
    instance = get_card_instance(player, legacy_card);
    instance->targets[1].player = result;
  }
  result = generic_activated_ability(
      player, card, event, 7, 0, 0, 0, 0, 0, 0, 0, &td,
      "Select target land it becomes the basic land type of your choice.");
  return result;
}

// drake skull cameo -->  bloodstone cameo

int card_dromar_the_banisher(int player, int card, event_t event)
{
  /* Dromar, the Banisher |3|W|U|B
   *   Legendary Creature — Dragon 6/6
   *   Flying
   *   Whenever Dromar, the Banisher deals combat damage to a player, you may pay {2}{U}.
   *   If you do, choose a color, then return all creatures of that color to their owners' hands. */

  check_legend_rule(player, card, event);

  invasion_dragon(player, card, event, MANACOST_XU(2, 1));

  if (event == EVENT_DAMAGE_TRIGGER_ABILITY)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, TYPE_CREATURE);
    this_test.color = 1 << choose_a_color(player, get_deck_color(player, 1 - player));
    new_manipulate_all(player, card, ANYBODY, &this_test, ACT_BOUNCE);
  }

  return 0;
}

int card_elfhame_sanctuary(int player, int card, event_t event){
  /* Elfhame Sanctuary |1|G
   * Enchantment
   * At the beginning of your upkeep, you may search your library for a basic land card, reveal that card, and put it into your hand.
   * If you do, you skip your draw step this turn and shuffle your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->targets[1].card = 0;
	}

	if( current_turn == player && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG)
	     && reason_for_trigger_controller == player){
		if( event == EVENT_TRIGGER ){
			int trig_mode = player == HUMAN ? RESOLVE_TRIGGER_OPTIONAL : 0;
			if( player == AI && get_card_instance(player, card)->targets[1].card >= 0 ){
				test_definition_t test;
				default_test_definition(&test, TYPE_LAND);
				test.zone = TARGET_ZONE_HAND;
				int lands_in_hand = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test);
				if( ! lands_in_hand ){
					if( count_subtype(player, TYPE_LAND, -1) <= 3 || hand_count[player] >= 5 ){
						trig_mode = RESOLVE_TRIGGER_MANDATORY;
					}
				}
			}
			event_result |= trig_mode;
		}
		else if (event == EVENT_RESOLVE_TRIGGER){
				int count = count_upkeeps(current_turn);
				while( count > 0 ){
						/* Ruling 10/4/2004: The "if you do" means "if you search your library". You get to skip your draw step and
						 * shuffle your deck even if you don't find (or choose not to find) a basic land. */
						test_definition_t this_test;
						new_default_test_definition(&this_test, TYPE_LAND, "Select a basic land card.");
						this_test.subtype = SUBTYPE_BASIC;
						int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);
						get_card_instance(player, card)->targets[1].card = result;
						get_card_instance(player, card)->targets[1].player = 66;
						count--;
				}
		}
	}

	if( get_card_instance(player, card)->targets[1].player == 66 ){
		skip_your_draw_step(player, event);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	return global_enchantment(player, card, event);
}

int card_elvish_champion(int player, int card, event_t event){
/* CARD_ID_ELVISH_CHAMPION	1024
Elvish Champion	|1|G|G
Creature - Elf 2/2
Other Elf creatures get +1/+1 and have |H2forestwalk. (They can't be blocked as long as defending player controls |Ha Forest.) */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( ! affect_me(player, card) && has_creature_type(affected_card_controller, affected_card, SUBTYPE_ELF) ){
			switch( event ){
					case EVENT_POWER:
					case EVENT_TOUGHNESS:
						event_result++;
						break;
					case EVENT_ABILITIES:
						event_result |= get_hacked_walk(player, card, KEYWORD_FORESTWALK);
						break;
					default:
						break;
			}
		}
	}
	return 0;
}

int card_empress_galina(int player, int card, event_t event){
/* CARD_ID_EMPRESS_GALINA	4992
Empress Galina	|3|U|U
Legendary Creature - Merfolk 1/3
|U|U, |T: Gain control of target legendary permanent. (This effect lasts indefinitely.) */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = 1-player;
	td.preferred_controller = 1-player;
	td.required_subtype = SUBTYPE_LEGEND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td)  ){
			gain_control_permanently(instance->parent_controller, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(2), 0, &td, "TARGET_LEGENDARY_PERMANENT");
}

int card_exclude(int player, int card, event_t event){
  /* Exclude |2|U
   * 	  Instant, 2U
   * 	  Counter target creature spell.
   * 	  Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, &td, 0) ){
			draw_cards(player, 1);
			card_instance_t *instance= get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}

int card_fact_or_fiction(int player, int card, event_t event){
  /* Fact or Fiction	|3|U
   * Instant
   * Reveal the top five cards of your library. An opponent separates those cards into two piles. Put one pile into your hand and the other into your graveyard. */

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		ai_modifier += 48;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		effect_fof(player, player, 5, TUTOR_GRAVE);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_fires_of_yavimaya(int player, int card, event_t event){
  /* Fires of Yavimaya	|1|R|G
   * Enchantment
   * Creatures you control have haste.
   * Sacrifice this enchantment: Target creature gets +2/+2 until end of turn. */

	if( event == EVENT_ABILITIES && in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( affected_card_controller == player && in_play(affected_card_controller, affected_card) &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
		{
			haste(affected_card_controller, affected_card, event);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_geothermal_crevice(int player, int card, event_t event){
  /* Geothermal Crevice	""
   * Land
   * This land enters tapped.
   * |T: Add |R.
   * |T, Sacrifice this land: Add |B|G. */

	return sac_land_tapped(player, card, event, COLOR_RED, COLOR_BLACK, COLOR_GREEN);
}

int card_galinas_knight(int player, int card, event_t event){
  /* Galina's Knight |W|U
   * 	  Creature - Merfolk Knight 2/2
   * 	  Protection from red */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		protection_from_red(player, card, event);
	}
	return 0;
}

int card_ghitu_fire(int player, int card, event_t event){
	/* Ghitu Fire	|X|R	0x20088e2
	 * Sorcery
	 * You may cast ~ as though it had flash if you pay |2 more to cast it.
	 * ~ deals X damage to any target. */
	spell_can_be_cast_as_instant_by_paying_2(player, card, event);

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_ANY", 1, NULL);
}

int card_glimmering_angel(int player, int card, event_t event){
  /* Glimmering Angel |3|W
   * 	  Creature - Angel 2/2
   * 	  Flying
   * 	  {U}: Glimmering Angel gains shroud until end of turn. (It can't be the target of spells or abilities.) */

	return generic_shade(player, card, event, 0, MANACOST_U(1), 0, 0, KEYWORD_SHROUD, 0);
}

int card_global_ruin(int player, int card, event_t event)
{
  /* Global Ruin	|4|W
   * Sorcery
   * Each player chooses from the lands they control a land of each basic land type, then sacrifices the rest. */

  // Each player chooses from the lands he or she controls a land of each basic land type, then sacrifices the rest.

  if (event == EVENT_CAN_CAST)
	return 1;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  int subtypes[5] = {SUBTYPE_SWAMP, SUBTYPE_ISLAND, SUBTYPE_FOREST, SUBTYPE_MOUNTAIN, SUBTYPE_PLAINS};

	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_LAND);
	  td.illegal_abilities = 0;

	  card_instance_t* instance = get_card_instance(player, card);

	  int pl, i;
	  char marked[2][151] = {{0}};
	  // Each player targets a land of each type.  Mark them.
	  APNAP(p,
			{
			  td.allowed_controller = td.preferred_controller = td.who_chooses = p;
			  for (i = 0; i < 5; ++i)
				{
				  td.required_subtype = subtypes[i];
				  if (can_target(&td))
					{
					  load_text(0, "WOOD_ELVES");
					  if (select_target(player, card, &td, text_lines[i], &instance->targets[0]))
						{
						  instance->number_of_targets = 0;
						  marked[instance->targets[0].player][instance->targets[0].card] = 1;
						  state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
						}
					}
				}
			});

	  int can_sac[2];

	  // Unmark the chosen lands; mark all other lands.
	  for (pl = 0; pl <= 1; ++pl)
		{
		  for (i = 0; i < active_cards_count[pl]; ++i)
			if (marked[pl][i])
			  {
				marked[pl][i] = 0;
				state_untargettable(pl, i, 0);
			  }
			else if (in_play(pl, i) && is_what(pl, i, TYPE_LAND))
			  marked[pl][i] = 1;

		  can_sac[pl] = can_sacrifice(player, pl, 1, TYPE_LAND, 0);
		}

	  // Each player who can sacrifice sacrifices all marked lands.
	  APNAP(p,
			{
			  if (can_sac[p])
				for (i = 0; i < active_cards_count[p]; ++i)
				  if (marked[p][i])
					kill_card(p, i, KILL_SACRIFICE);
			});

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_hanna_ships_navigator(int player, int card, event_t event){
  /* Hanna, Ship's Navigator	|1|W|U
   * Legendary Creature - Human Artificer 1/2
   * |1|W|U, |T: Return target artifact or enchantment card from your graveyard to your hand. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ARTIFACT | TYPE_ENCHANTMENT, "Select an artifact or enchantment card.");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int result = validate_target_from_grave_source(player, card, player, 0);
		if( result > -1 ){
			const int *grave = get_grave(player);
			add_card_to_hand(player, grave[result]);
			remove_card_from_grave(player, result);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST_XUW(1, 1, 1), 0, NULL, NULL,
															0, &this_test, GS_GRAVE_RECYCLER);
}

int card_harsh_judgement(int player, int card, event_t event){
  /* Harsh Judgment	|2|W|W
   * Enchantment
   * As this enchantment enters, choose a color.
   * If an instant or sorcery spell of the chosen color would deal damage to you, it deals that damage to its controller instead. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = 1<<choose_a_color_and_show_legacy(player, card, 1-player, -1);
	}

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == -1 && damage->damage_target_player == player &&
				damage->damage_source_player != damage->damage_target_player && damage->info_slot > 0 )
			{
				if( instance->targets[3].player & TYPE_SPELL ){
					if( instance->initial_color & get_card_instance(player, card)->info_slot ){
						damage->damage_target_player = damage->damage_source_player;
					}
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_heroes_reunion(int player, int card, event_t event){
  /* Heroes' Reunion	|G|W
   * Instant
   * Target player gains 7 life. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) )
			gain_life(instance->targets[0].player, 7);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

// hunting kavu --> giant trap door spider

int card_irrigation_ditch(int player, int card, event_t event){
  /* Irrigation Ditch	""
   * Land
   * This land enters tapped.
   * |T: Add |W.
   * |T, Sacrifice this land: Add |G|U. */

	return sac_land_tapped(player, card, event, COLOR_WHITE, COLOR_GREEN, COLOR_BLUE);
}

int card_jade_leech(int player, int card, event_t event){
  /* Jade Leech	|2|G|G
   * Creature - Leech 5/5
   * Green spells you cast cost |G more to cast. */

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card, AL_STATIC) ){
		if( get_color(affected_card_controller, affected_card) & COLOR_TEST_GREEN ){
			COST_GREEN++;
		}
	}

	return 0;
}

int card_juntu_stakes(int player, int card, event_t event){
  /* Juntu Stakes	|2
   * Artifact
   * Creatures with power 1 or less don't untap during their controllers' untap steps. */

	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && ! is_humiliated(player, card, AL_STATIC) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) && get_power(affected_card_controller, affected_card) < 2 ){
			card_instance_t *instance= get_card_instance(affected_card_controller, affected_card);
			instance->untap_status &= ~3;
		}
	}
	return 0;
}

int card_kangee_aerie_keeper(int player, int card, event_t event)
{
  /* Kangee, Aerie Keeper	|2|W|U
   * Legendary Creature - Bird Wizard 2/2
   * Kicker |X|2
   * Flying
   * When ~ enters the battlefield, if it was kicked, put X feather counters on it.
   * Other Bird creatures get +1/+1 for each feather counter on ~. */

  int result;
  card_instance_t *instance;

  check_legend_rule(player, card, event);
  if (event != EVENT_CAST_SPELL)
  {
    etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (event == EVENT_ETB)
    {
      result = check_special_flags(player, card, 0x80);
      if (result != 0)
      {
        instance = get_card_instance(player, card);
        add_counters(player, card, player, card, COUNTER_FEATHER, instance->info_slot);
      }
    }
    else if ((uint32_t)(event - EVENT_POWER) < 2)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = count_counters(player, card, COUNTER_FEATHER);
        if (result != 0)
        {
          boost_subtype(player, card, event, SUBTYPE_BIRD, result, result, 0, 0,
                        BCT_CONTROLLER_ONLY);
        }
      }
    }
    return 0;
  }
  if ((card == affected_card) && (player == affected_card_controller))
  {
    result = has_mana(player, 7, 2);
    if (result != 0)
    {
      result = has_mana(player, 7, 3);
      result = do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass",
                         (unsigned int)(result == 0));
      if (result == 0)
      {
        result = charge_mana_multi(player, 2, 0, 0, 0, 0, 0);
        if (result != 0)
        {
          result = charge_mana_multi(player, -1, 0, 0, 0, 0, 0);
          if (result != 0)
          {
            set_x_for_x_spells(player, card, EVENT_CAST_SPELL, x_value);
            set_special_flags(player, card, 0x80);
          }
        }
      }
    }
  }
  etb_ability(player, card, EVENT_CAST_SPELL, RESOLVE_TRIGGER_MANDATORY);
  return 0;
}

int card_kavu_chameleon(int player, int card, event_t event){
  /* Kavu Chameleon	|3|G|G
   * Creature - Kavu 4/4
   * This spell can't be countered.
   * |G: This creature becomes the color of your choice until end of turn. */

	cannot_be_countered(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int clr = 1<<choose_a_color(player, get_deck_color(player, 1-player));
		change_color(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, clr,
					CHANGE_COLOR_SET|CHANGE_COLOR_END_AT_EOT|CHANGE_COLOR_NO_SLEIGHT);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL);
}

int card_kavu_lair(int player, int card, event_t event){
  /* Kavu Lair |2|G
   * Enchantment
   * Whenever a creature with power 4 or greater enters the battlefield, its controller draws a card. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.power = 3;
		this_test.power_flag = F5_POWER_GREATER_THAN_VALUE;
		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(trigger_cause_controller, 1);
	}

	return global_enchantment(player, card, event);
}

int card_kavu_monarch(int player, int card, event_t event){
  /* Kavu Monarch |2|R|R
   * Creature — Kavu 3/3
   * Kavu creatures have trample.
   * Whenever another Kavu enters the battlefield, put a +1/+1 counter on Kavu Monarch. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.subtype = SUBTYPE_KAVU;
		this_test.not_me = 1;
		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		add_counters(player, card, player, card, COUNTER_P1_P1, 1);
	}

	boost_creature_type(player, card, event, SUBTYPE_KAVU, 0, 0, KEYWORD_TRAMPLE, BCT_INCLUDE_SELF);

	return 0;
}

int card_kavu_titan(int player, int card, event_t event){
  /* Kavu Titan	|1|G
   * Creature - Kavu 2/2
   * Kicker |2|G (You may pay an additional |2|G as you cast this spell.)
   * If this creature was kicked, it enters with three +1/+1 counters on it and with trample. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		do_kicker(player, card, MANACOST_XG(2, 1));
	}

	if( kicked(player, card) ){
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);
	}

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) && kicked(player, card) ){
		event_result |= KEYWORD_TRAMPLE;
	}

	return 0;
}

int card_keldon_necropolis(int player, int card, event_t event){
	/* Keldon Necropolis	""	0x2008914
	 * Legendary Land
	 * |T: Add |C.
	 * |4|R, |T, Sacrifice a creature: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td.allow_cancel = 0;

	card_instance_t *instance = get_card_instance( player, card );

	check_legend_rule(player, card, event);

	if( event == EVENT_ACTIVATE ){
		int choice = instance->number_of_targets = instance->info_slot = 0;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_CREATURE | GAA_CAN_TARGET, MANACOST_XR(5, 1), 0, &td, NULL)
		  ){
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Sac & damage\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		if( choice == 1 ){
			add_state(player, card, STATE_TAPPED);
			tapped_for_mana_color = -1;
			if( charge_mana_for_activated_ability(player, card, 4, 0, 0, 0, 1, 0) &&
				sacrifice(player, card, player, 0, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0) &&
				pick_target(&td, "TARGET_ANY")
			  ){
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
		if( instance->info_slot > 0 ){
			if( valid_target(&td) ){
				damage_target0(player, card, 2);
			}
		}
	}

	return 0;
}

int card_liberate(int player, int card, event_t event){
  /* Liberate	|1|W
   * Instant
   * Exile target creature you control. Return that card to the battlefield under its owner's control at the beginning of the next end step. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	if( player == AI ){
		td.required_state = TARGET_STATE_DESTROYED;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			if( player == AI ){
				regenerate_target(instance->targets[0].player, instance->targets[0].card);
			}
			remove_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	if( player == AI ){
		return generic_spell(player, card, event, GS_REGENERATION | GS_CAN_TARGET, &td, NULL, 1, NULL);
	}
	else{
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature you control.", 1, NULL);
	}
	return 0;
}

/* ManalinkEh 2022: 0x248ee90; original cards/invasion.c:2482. */
int card_loafing_giant(int player, int card, event_t event)
{
  /* Loafing Giant	|4|R
   * Creature - Giant 4/6
   * Whenever this creature attacks or blocks, mill a card. If a land card was milled this way, prevent all combat damage this creature would deal this turn. */

  int result;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);

  if (((event == EVENT_DAT_ABILITY) ||
       ((result = blocking(player, card, event),
         result != 0 &&
             (result = is_humiliated(player, card, AL_TRIG), result == 0)))) &&
      (result = *deck_ptr[player], result != -1))
  {

    mill(player, 1);
    result = is_what(-1, result, 1);
    if (result != 0)
    {
      negate_combat_damage_this_turn(player, card, player, card, 0);
    }
  }
  return 0;
}

int card_malice_spite(int player, int card, event_t event)
{
  /* Malice |3|B
   * 	  Instant
   * 	  Destroy target nonblack creature. It can't be regenerated.
   *
   * 	  Spite |3|U
   * 	  Instant
   * 	  Counter target noncreature spell. */

  int available_or_valid;
  card_instance_t *instance;
  unsigned int selected_modes;
  target_definition_t *counter_target;
  target_definition_t td;
  target_definition_t td1;

  if (event == EVENT_MODIFY_COST)
  {
    default_target_definition(player, card, &td, 2);
    td.illegal_color = get_sleighted_color_test(player, card, 2);
    counterspell_target_definition(player, card, &td1, 0);
    td1.illegal_type = TYPE_CREATURE;
    available_or_valid = counterspell(player, card, EVENT_CAN_CAST, &td1, 0);
    split_card_impl(player, card, EVENT_MODIFY_COST, 0x4b0f, can_target(&td), 10, 0x4b34,
                    available_or_valid, 8, 0);
    return 0;
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    default_target_definition(player, card, &td1, 2);
    td1.illegal_color = get_sleighted_color_test(player, card, 2);
    new_pick_target(&td1, "Select target non-black creature.", 0, 0x81);
    return 0;
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    instance = get_card_instance(player, card);
    instance->targets[0].player = card_on_stack_controller;
    instance->targets[0].card = card_on_stack;
    instance->number_of_targets = '\x01';
    return 0;
  }
  if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 2);
  td.illegal_color = get_sleighted_color_test(player, card, 2);
  counterspell_target_definition(player, card, &td1, 0);
  td1.illegal_type = TYPE_CREATURE;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    selected_modes = instance->info_slot;
    if ((selected_modes & 1) != 0)
    {
      available_or_valid = valid_target(&td);
      if (available_or_valid != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
      selected_modes = instance->info_slot;
    }
    counter_target = &td1;
    if (((selected_modes & 2) != 0) &&
        (available_or_valid = counterspell_validate(player, card, counter_target, 0),
         available_or_valid != 0))
    {
      real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  counter_target = &td1;
  available_or_valid = counterspell(player, card, EVENT_CAN_CAST, counter_target, 0);
  return split_card_impl(player, card, event, 0x4b0f, can_target(&td), 10, 0x4b34,
                         available_or_valid, 8, 0);
}

int card_metathran_aerostat(int player, int card, event_t event){
  /* Metathran Aerostat |2|U|U
   * Creature — Metathran 2/2
   * Flying
   * {X}{U}: You may put a creature card with converted mana cost X from your hand onto the battlefield.
   * 	If you do, return Metathran Aerostat to its owner's hand. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		char buffer[100];
		scnprintf(buffer, 100, "Select a creature card with CMC %d.", instance->info_slot);
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, buffer);
		this_test.cmc = instance->info_slot;
		if( new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test) >= 0 ){
			bounce_permanent(instance->parent_controller, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XU(-1, 1), 0, NULL, NULL);
}

int card_meteor_storm(int player, int card, event_t event){
	/* Meteor Storm	|R|G	0x2008928
	 * Enchantment
	 * |2|R|G, Discard two cards at random: ~ deals 4 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_ACTIVATE && hand_count[player] > 1 ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XGR(2, 1, 1), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XGR(2, 1, 1), 0, &td, "TARGET_ANY");
		if( spell_fizzled != 1 ){
			multidiscard(player, 2, 1);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
	}

	return 0;
}

int card_nightscape_apprentice(int player, int card, event_t event)
{
  /* Nightscape Apprentice	|B
   * Creature - Zombie Wizard 1/1
   * |U, |T: Put target creature you control on top of its owner's library.
   * |R, |T: Target creature gains first strike until end of turn. */

  if (event == EVENT_CHECK_ABILITIES && affected_card_controller == player &&
      generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_R(1), 0,
                                NULL, NULL))
    check_abilities_keywords |= KEYWORD_FIRST_STRIKE;

  /*
    Nightscape Apprentice |B
    Creature - Zombie Wizard 1/1
    {U}, {T}: Put target creature you control on top of its owner's library.
    {R}, {T}: Target creature gains first strike until end of turn.
  */
  return invasion_apprentices(player, card, event, COLOR_BLUE, COLOR_RED);
}

int card_nightscape_master(int player, int card, event_t event){
  /* Nightscape Master	|2|B|B
   * Creature - Zombie Wizard 2/2
   * |U|U, |T: Return target creature to its owner's hand.
   * |R|R, |T: This creature deals 2 damage to target creature. */

	return invasion_masters(player, card, event, COLOR_BLUE, COLOR_RED);
}

int card_noble_panther(int player, int card, event_t event)
{
  /* Noble Panther English |1|W|G
   * 	  Creature - Cat 3/3
   * 	  {1}: Noble Panther gains first strike until end of turn. */

  if (event == EVENT_CHECK_ABILITIES && affect_me(player, card) &&
      generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(1), 0, NULL, NULL))
    check_abilities_keywords |= KEYWORD_FIRST_STRIKE;
  return generic_shade(player, card, event, 0, MANACOST_X(1), 0, 0, KEYWORD_FIRST_STRIKE, 0);
}

int card_obliterate(int player, int card, event_t event)
{
  /* Obliterate	|6|R|R
   * Sorcery
   * ~ can't be countered.
   * Destroy all artifacts, creatures, and lands. They can't be regenerated. */

  cannot_be_countered(player, card, event);
  return card_jokulhaups(player, card, event);
}

int card_opt(int player, int card, event_t event){
  /* Opt	|U
   * Instant
   * Scry 1. (Look at the top card of your library. You may put that card on the bottom.)
   * Draw a card. */

	if( event == EVENT_RESOLVE_SPELL){
		scry(player, 1);
		draw_cards(player, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_ordered_migration(int player, int card, event_t event)
{
  /* Ordered Migration	|3|W|U
   * Sorcery
   * Domain - Put a 1/1 |Sblue Bird creature token with flying onto the battlefield for each basic land type among lands you control. */

	if (event == EVENT_RESOLVE_SPELL)
	{
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_BIRD, &token);
		token.pow = token.tou = 1;
		token.color_forced = COLOR_TEST_BLUE;
		token.qty = count_domain(player, card);
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_overabundance(int player, int card, event_t event){
  /* Overabundance	|1|R|G
   * Enchantment
   * Whenever a player taps a land for mana, that player adds one mana of any type that land produced, and this enchantment deals 1 damage to the player. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( event == EVENT_TAP_CARD && is_what(affected_card_controller, affected_card, TYPE_LAND) && tapped_for_mana_color >= 0 ){
			damage_player(affected_card_controller, 1, player, card);
		}
	}

	return card_mana_flare(player, card, event);
}

int card_overload(int player, int card, event_t event){
/* CARD_ID_OVERLOAD	5055
Overload	|R
Instant
Kicker |2 (You may pay an additional |2 as you cast this spell.)
Destroy target artifact if its converted mana cost is 2 or less. If ~ was kicked, destroy that artifact if its converted mana cost is 5 or less instead. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_ARTIFACT") ){
			if( get_cmc(instance->targets[0].player, instance->targets[0].card) > 2 ){
				do_kicker(player, card, MANACOST_X(2));
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			if( ! kicked(player, card) && get_cmc(instance->targets[0].player, instance->targets[0].card) < 3 ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
			if( kicked(player, card) && get_cmc(instance->targets[0].player, instance->targets[0].card) < 6 ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				}

		  }
	  kill_card(player, card, KILL_DESTROY );
	}
	return 0;
}

int card_phyrexian_altar(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_ALTAR	5057
Phyrexian Altar	|3
Artifact
Sacrifice a creature: Add one mana of any color to your mana pool. */

	if( ! IS_GAA_EVENT(event) && !(event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		return new_can_sacrifice_as_cost(player, card, &test);
	}

	if(event == EVENT_ACTIVATE ){
		ai_modifier -= 36;
		if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
			FORCE(produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1));
			tapped_for_mana_color = -2;
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( can_produce_mana(player, card) && new_can_sacrifice_as_cost(player, card, &test)){
			declare_mana_available_hex(player, 1, COLOR_TEST_ANY_COLORED);
		}
	}

	return 0;
}

int card_phyrexian_delver(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_DELVER	5059
Phyrexian Delver	|3|B|B
Creature - Zombie 3/2
When ~ enters the battlefield, return target creature card from your graveyard to the battlefield. You lose life equal to that card's converted mana cost.*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) && ! graveyard_has_shroud(player) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
			int result = new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_VALUE, &this_test);
			lose_life(player, get_cmc(player, result));
		}
	}

	return 0;
}

int card_phyrexian_infiltrator(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_INFILTRATOR	5060
Phyrexian Infiltrator	|2|B
Creature - Minion 2/2
|2|U|U: Exchange control of ~ and target creature. (This effect lasts indefinitely.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XU(2, 2), 0, &td, "TARGET_CREATURE");
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XU(2, 2), 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			ai_modifier += (instance->targets[0].player != player ? 48 : -48);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && in_play(instance->parent_controller, instance->parent_card) ){
			if( instance->parent_controller != instance->targets[0].player ){
				exchange_control_of_target_permanents(instance->parent_controller, instance->parent_card,
														instance->parent_controller, instance->parent_card,
														instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_phyrexian_lens(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_LENS	5061
Phyrexian Lens	|3
Artifact
|T, Pay 1 life: Add one mana of any color to your mana pool. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( mana_producer(player, card, event) ){
			return can_pay_life(player, 1);
		}
	}

	if(event == EVENT_ACTIVATE ){
		ai_modifier -= 36;

		produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 1);
		if( spell_fizzled != 1){
			lose_life(player, 1);
		}
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( can_pay_life(player, 1) ){
			return mana_producer(player, card, event);
		}
	}

	return 0;
}

int card_plague_spitter(int player, int card, event_t event){
/* CARD_ID_PLAGUE_SPITTER	5065
Plague Spitter	|2|B
Creature - Horror 2/2
At the beginning of your upkeep, ~ deals 1 damage to each creature and each player.
When ~ dies, ~ deals 1 damage to each creature and each player. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		new_damage_all(player, card, ANYBODY, 1, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
	}

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		new_damage_all(player, card, ANYBODY, 1, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
	}

	return 0;
}

int card_plague_spores(int player, int card, event_t event)
{
  /* Plague Spores	|4|B|R
   * Sorcery
   * Destroy target non|Sblack creature and target land. They can't be regenerated. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}


	target_definition_t td_creature;
	default_target_definition(player, card, &td_creature, TYPE_CREATURE);
	td_creature.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	target_definition_t td_land;
	default_target_definition(player, card, &td_land, TYPE_LAND);

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td_land) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td_creature, NULL, 1, NULL);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
		instance->number_of_targets = 0;
		char msg[100];
		scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select target non-%s creature.", COLOR_BLACK));
		if( new_pick_target(&td_creature, msg, 0, 1 | GS_LITERAL_PROMPT) ){
			new_pick_target(&td_land, "TARGET_LAND", 1, 1);
		}
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
		if (validate_target(player, card, &td_creature, 0))
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);

		if (validate_target(player, card, &td_land, 1))
			kill_card(instance->targets[1].player, instance->targets[1].card, KILL_BURY);

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_planar_portal(int player, int card, event_t event){
/* CARD_ID_PLANAR_PORTAL	1116
Planar Portal	|6
Artifact
|6, |T: Search your library for a card and put that card into your hand. Then shuffle your library. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card to tutor.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(6), 0, NULL, NULL);
}

int card_probe(int player, int card, event_t event){
/* CARD_ID_PROBE	5071
Probe	|2|U
Sorcery
Kicker |1|B (You may pay an additional |1|B as you cast this spell.)
Draw three cards, then discard two cards.
If ~ was kicked, target player discards two cards. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		basic_spell(player, card, event);
		do_kicker(player, card, MANACOST_XB(1, 1));
		if( kicked(player, card) ){
			instance->number_of_targets = 0;
			if( can_target(&td) ){
				pick_target(&td, "TARGET_PLAYER");
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		draw_cards(player, 3);
		multidiscard(player, 2, player);
		if( kicked(player, card) && valid_target(&td) ){
			new_multidiscard(instance->targets[0].player, 2, 0, player);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_prohibit(int player, int card, event_t event){
/* CARD_ID_PROHIBIT	5072
Prohibit	|1|U
Instant
Kicker |2 (You may pay an additional |2 as you cast this spell.)
Counter target spell if its converted mana cost is 2 or less. If ~ was kicked, counter that spell if its converted mana cost is 4 or less instead. */

	if( event == EVENT_CAN_CAST ){
		int result = counterspell(player, card, event, NULL, 0);
		if( result ){
			if( player == AI ){
				int cmc = get_cmc(card_on_stack_controller, card_on_stack);
				if( cmc < 5 ){
					if( cmc <=2 || (cmc > 2 && has_mana(player, COLOR_COLORLESS, 2)) ){
						return result;
					}
				}
			}
			else{
				return result;
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		counterspell(player, card, event, NULL, 0);
		if( spell_fizzled != 1 ){
			do_kicker(player, card, MANACOST_X(2));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if (counterspell_validate(player, card, NULL, 0)){
			card_instance_t *instance = get_card_instance(player, card);
			int cmc = get_cmc(instance->targets[0].player, instance->targets[0].card);
			int good = (cmc <=2 || (cmc > 2 && cmc <= 4 && kicked(player, card))) ? 1 : 0;
			if( good ){
				real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_pyre_zombie(int player, int card, event_t event){
	/* Pyre Zombie	|1|B|R	0x2002280
	 * Creature - Zombie 2/1
	 * At the beginning of your upkeep, if ~ is in your graveyard, you may pay |1|B|B. If you do, return ~ to your hand.
	 * |1|R|R, Sacrifice ~: It deals 2 damage to any target. */
	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		if( has_mana_multi(player, MANACOST_XB(1, 2)) ){
			int choice = do_dialog(player, player, card, -1, -1," Return Pyre Zombie to hand\n Pass", 0);
			if( choice == 0 ){
				charge_mana_multi(player, MANACOST_XB(1, 2));
				if( spell_fizzled != 1){
					remove_state(player, card, STATE_INVISIBLE);
					hand_count[player]++;
					return -1;
				}
			}
		}
		return -2;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XR(1, 2), 0,
									&td, "TARGET_ANY");
}

int card_raging_kavu(int player, int card, event_t event){
/* CARD_ID_RAGING_KAVU	5081
Raging Kavu	|1|R|G
Creature - Kavu 3/1
Flash
Haste */

	haste(player, card, event);
	return flash(player, card, event);
}

int card_reckless_assault(int player, int card, event_t event){
	/* Reckless Assault	|2|B|R	0x2008973
	 * Enchantment
	 * |1, Pay 2 life: ~ deals 1 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 2, &td, "TARGET_ANY");
}

int card_recoil(int player, int card, event_t event){
/* CARD_ID_RECOIL	1554
Recoil	|1|U|B
Instant
Return target permanent to its owner's hand. Then that player discards a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			int owner = get_owner(instance->targets[0].player, instance->targets[0].card);bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			if( hand_count[owner] > 0 ){
				discard(owner, 0, player);
			}
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
}

int card_repulse(int player, int card, event_t event){
/* CARD_ID_REPULSE	1352
Repulse	|2|U
Instant
Return target creature to its owner's hand.
Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			draw_a_card(player);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_restock(int player, int card, event_t event){

	/* Restock	|3|G|G
	 * Sorcery
	 * Return two target cards from your graveyard to your hand. Exile ~. */

	if( event == EVENT_CAN_CAST ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		if( generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 0, &this_test)){
			return count_graveyard(player) >= 2;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		select_multiple_cards_from_graveyard(player, player, -1, AI_MAX_VALUE, NULL, 2, &instance->targets[0]);
	}

	if( event == EVENT_RESOLVE_SPELL){
		int i, num_validated = 0;
		for (i = 0; i < 2; ++i){
			int selected = validate_target_from_grave(player, card, player, i);
			if (selected != -1){
				from_grave_to_hand(player, selected, TUTOR_HAND);
				++num_validated;
			}
		}
		if (num_validated == 0){
			spell_fizzled = 1;
			kill_card(player, card, KILL_DESTROY);
		}
		else {
			kill_card(player, card, KILL_EXILE);
		}
  }
  return 0;
}

int card_reviving_vapors(int player, int card, event_t event){
/* CARD_ID_REVIVING_VAPORS	5089
Reviving Vapors	|2|W|U
Instant
Reveal the top three cards of your library and put one of them into your hand. You gain life equal to that card's converted mana cost.
Put all other cards revealed this way into your graveyard. */

	if( event == EVENT_RESOLVE_SPELL){
		int amount = MIN(count_deck(player), 3);
		if( amount ){
			int *deck = deck_ptr[player];
			show_deck(1-player, deck, amount, "Cards revealed by Reviving Vapors", 0, gs_done_008b40e0 );

			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to add to your hand.");
			this_test.create_minideck = amount;
			this_test.no_shuffle = 1;
			int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 1, AI_MAX_CMC, &this_test);
			amount--;
			if( amount ){
				mill(player, amount);
			}
			gain_life(player, get_cmc(player, result));
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_reya_dawnbringer(int player, int card, event_t event){
/* CARD_ID_REYA_DAWNBRINGER	5091
Reya Dawnbringer	|6|W|W|W
Legendary Creature - Angel 4/6
Flying
At the beginning of your upkeep, you may return target creature card from your graveyard to the battlefield. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) && ! graveyard_has_shroud(player) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_CMC, &this_test);
		}
	}

	return 0;
}

int card_rith_the_awakener(int player, int card, event_t event){
/* Rith, the Awakener	|3|R|G|W
 * Legendary Creature - Dragon 6/6
 * Flying
 * Whenever ~ deals combat damage to a player, you may pay |2|G.
 * If you do, choose a color, then put a 1/1 |Sgreen Saproling creature token onto the battlefield for each permanent of that color. */

	check_legend_rule(player, card, event);

	invasion_dragon(player, card, event, MANACOST_XG(2, 1));

	if( event == EVENT_DAMAGE_TRIGGER_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = 1<<choose_a_color(player, get_deck_color(player, 1-player));

		int amount = check_battlefield_for_special_card(player, card, ANYBODY, CBFSC_GET_COUNT, &this_test);
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, amount);
	}

	return 0;
}

int card_rout(int player, int card, event_t event){
/* CARD_ID_ROUT	5097
Rout	|3|W|W
Sorcery
You may cast ~ as though it had flash if you pay |2 more to cast it. (You may cast it any time you could cast an instant.)
Destroy all creatures. They can't be regenerated. */

	if( ! is_unlocked(player, card, event, 10) ){ return 0; }
	spell_can_be_cast_as_instant_by_paying_2(player, card, event);
	return card_wrath_of_god(player, card, event);
}

int card_sabertooth_nishoba(int player, int card, event_t event){
	/* Sabertooth Nishoba	|4|G|W
	 * Creature - Cat Beast Warrior 5/5
	 * Trample, protection from |Sblue and from |Sred */

	return generic_protection_from(player, card, event, COLOR_TEST_BLUE | COLOR_TEST_RED);
}

int card_saproling_infestation(int player, int card, event_t event){
	/* Saproling Infestation	|1|G
	 * Enchantment
	 * Whenever a player kicks a spell, you put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && reason_for_trigger_controller == player){
		if (check_special_flags(trigger_cause_controller, trigger_cause, SF_KICKED) || check_special_flags(trigger_cause_controller, trigger_cause, SF_KICKED2)){
			sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
		}
	}
	if (event == EVENT_SPA){
		generate_token_by_id(player, card, CARD_ID_SAPROLING);
	}
	return global_enchantment(player, card, event);
}

int card_saproling_symbiosis(int player, int card, event_t event){
	/* Saproling Symbiosis	|3|G
	 * Sorcery
	 * You may cast ~ as though it had flash if you pay |2 more to cast it.
	 * Put a 1/1 |Sgreen Saproling creature token onto the battlefield for each creature you control. */

	spell_can_be_cast_as_instant_by_paying_2(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, count_subtype(player, TYPE_CREATURE, -1));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// seashell cameo --> bloodstone cameo

int card_shivan_harvest(int player, int card, event_t event){
/* CARD_ID_SHIVAN_HARVEST	5115
Shivan Harvest	|1|R
Enchantment
|1|R, Sacrifice a creature: Destroy target nonbasic land. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = SUBTYPE_NONBASIC;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE | GAA_LITERAL_PROMPT, MANACOST_XR(1, 1),
									0, &td, "Select target nonbasic land.");
}

int card_skizzik(int player, int card, event_t event){
/* CARD_ID_SKIZZIK	5118
Skizzik	|3|R
Creature - Elemental 5/3
Kicker |R (You may pay an additional |R as you cast this spell.)
Trample, haste
At the beginning of the end step, sacrifice ~ unless it was kicked. */

	haste(player, card, event);

	if( event == EVENT_RESOLVE_SPELL && ! kicked(player, card) ){
		haste_and_remove_at_beginning_of_next_end_step(player, card, player, card, KILL_SACRIFICE);
	}

	return kicker(player, card, event, MANACOST_R(1));
}

int card_slay2(int player, int card, event_t event){
  /* Slay |2|B
   * 	  Instant, 2B (3)
   * 	  Destroy target green creature. It can't be regenerated.
   * 	  Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target %s creature.", COLOR_GREEN), 1, NULL);
}

int card_sleepers_robe(int player, int card, event_t event)
{
  /* Sleeper's Robe	|U|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has fear.
   * Whenever enchanted creature deals combat damage to an opponent, you may draw a card. */

  // original code : 0x4DBC2B



	int packets = equipped_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_OPPONENT|
												DDBM_TRIGGER_OPTIONAL);
	if( packets )
		draw_cards(player, packets);

	return generic_aura(player, card, event, player, 0, 0, 0,SP_KEYWORD_FEAR, 0, 0, 0);
}

int card_smoldering_tar(int player, int card, event_t event){
/* CARD_ID_SMOLDERING_TAR	5122
Smoldering Tar	|2|B|R
Enchantment
At the beginning of your upkeep, target player loses 1 life.
Sacrifice ~: ~ deals 4 damage to target creature. Activate this ability only any time you could cast a sorcery. */

	if( trigger_condition == TRIGGER_UPKEEP && current_turn == player ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		upkeep_trigger_ability_mode(player, card, event, player, can_target(&td) ? RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			lose_life(instance->targets[0].player, 1);
		}

		instance->number_of_targets = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME | GAA_CAN_SORCERY_BE_PLAYED, MANACOST0, 0,
									&td1, "TARGET_CREATURE");
}

int card_spirit_of_resistance(int player, int card, event_t event){
/* CARD_ID_SPIRIT_OF_RESISTANCE	5126
Spirit of Resistance	|2|W
Enchantment
As long as you control a permanent of each color, prevent all damage that would be dealt to you. */

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *damage = get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->damage_target_card == -1 && damage->damage_target_player == player && !damage_is_to_planeswalker(damage) &&
			damage->info_slot > 0 )
		{
			int i;
			int clr = 0;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_play(player, i) && is_what(player, i, TYPE_PERMANENT) ){
					clr |= get_color(player, i);
				}
				if( num_bits_set(clr) > 4 ){
					break;
				}
			}
			if( num_bits_set(clr) > 4 ){
				int amount = damage->info_slot;
				prevent_damage(affected_card_controller, affected_card, amount);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_stalking_assassin(int player, int card, event_t event){
/* CARD_ID_STALKING_ASSASSIN	5129
Stalking Assassin	|1|U|B
Creature - Human Assassin 1/1
|3|U, |T: Tap target creature.
|3|B, |T: Destroy target tapped creature. */

	if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td_creature;
		default_target_definition(player, card, &td_creature, TYPE_CREATURE);

		target_definition_t td_tapped_creature;
		default_target_definition(player, card, &td_tapped_creature, TYPE_CREATURE);
		td_tapped_creature.required_state = TARGET_STATE_TAPPED;

		int can_tap = !is_tapped(player, card) && !is_sick(player, card);
		int can_tap_creature = can_tap && can_target(&td_creature) && has_mana_for_activated_ability(player, card, MANACOST_XU(3, 1));
		int can_destroy_creature = can_tap && can_target(&td_tapped_creature) && has_mana_for_activated_ability(player, card, MANACOST_XB(3, 1));

		int choice = DIALOG(player, card, event,
							"Tap a creature", can_tap_creature, 1,
							"Destroy a tapped creature", can_destroy_creature, 2);

		if (event == EVENT_CAN_ACTIVATE){
			return choice;
		} else if (event == EVENT_ACTIVATE){
			instance->number_of_targets = 0;
			if (choice == 1){
				if (current_turn == player){
					ai_modifier -= 48;	// Very relucant to activate to tap during own turn
				} else if (current_phase == PHASE_DISCARD){
					ai_modifier += 24;	// If nothing available to destroy by opponent's end phase, more eager to tap
				}
				if (charge_mana_for_activated_ability(player, card, MANACOST_XU(3, 1)) && pick_target(&td_creature, "TARGET_CREATURE")){
					tap_card(player, card);
				}
			} else if (choice == 2){
				if (charge_mana_for_activated_ability(player, card, MANACOST_XB(3, 1)) && pick_target(&td_creature, "TARGET_CREATURE")){
					tap_card(player, card);
				}
			}
		} else {	// EVENT_RESOLVE_ACTIVATION
			if (choice == 1 && valid_target(&td_creature)){
				tap_card(instance->targets[0].player, instance->targets[0].card);
			} else if (choice == 2 && valid_target(&td_tapped_creature)){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
		}
	}

	if (event == EVENT_ATTACK_RATING && affect_me(player, card)){
		ai_defensive_modifier += 48;
	} else if (event == EVENT_BLOCK_RATING && affect_me(player, card)){
		ai_defensive_modifier -= 48;
	}
	return 0;
}

int card_sterling_grove(int player, int card, event_t event){
/* CARD_ID_STERLING_GROVE	5132
Sterling Grove	|G|W
Enchantment
Other enchantments you control have shroud. (They can't be the targets of spells or abilities.)
|1, Sacrifice ~: Search your library for an enchantment card and reveal that card. Shuffle your library, then put the card on top of it. */

	if( event == EVENT_ABILITIES && in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( ! affect_me(player, card) && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_ENCHANTMENT) )
		{
			event_result |= KEYWORD_SHROUD;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ENCHANTMENT, "Select an enchantment card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_DECK, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
}

int card_stormscape_apprentice(int player, int card, event_t event){
  /* Stormscape Apprentice |U
   * 	  Creature - Human Wizard 1/1
   * 	  {W}, {T}: Tap target creature.
   * 	  {B}, {T}: Target player loses 1 life. */

	return invasion_apprentices(player, card, event, COLOR_BLACK, COLOR_WHITE);
}

int card_stormscape_master(int player, int card, event_t event){
/* CARD_ID_STORMSCAPE_MASTER	5133
Stormscape Master	|2|U|U
Creature - Human Wizard 2/2
|W|W, |T: Target creature gains protection from the color of your choice until end of turn.
|B|B, |T: Target player loses 2 life and you gain 2 life. */

	return invasion_masters(player, card, event, COLOR_BLACK, COLOR_WHITE);
}

int card_sulfur_vent(int player, int card, event_t event){
/* CARD_ID_SULFUR_VENT	5136
Sulfur Vent	""
Land
~ enters the battlefield tapped.
|T: Add |B to your mana pool.
|T, Sacrifice ~: Add |U|R to your mana pool. */

	return sac_land_tapped(player, card, event, COLOR_BLACK, COLOR_RED, COLOR_BLUE);
}

int card_sunscape_apprentice(int player, int card, event_t event){
  /* Sunscape Apprentice |W
   * 	  Creature - Human Wizard 1/1
   * 	  {G}, {T}: Target creature gets +1/+1 until end of turn.
   * 	  {U}, {T}: Put target creature you control on top of its owner's library. */

	return invasion_apprentices(player, card, event, COLOR_BLUE, COLOR_GREEN);
}

int card_sunscape_master(int player, int card, event_t event){
/* CARD_ID_SUNSCAPE_MASTER	5137
Sunscape Master	|2|W|W
Creature - Human Wizard 2/2
|G|G, |T: Creatures you control get +2/+2 until end of turn.
|U|U, |T: Return target creature to its owner's hand. */

	return invasion_masters(player, card, event, COLOR_BLUE, COLOR_GREEN);
}

int card_tangle2(int player, int card, event_t event){
/* CARD_ID_TANGLE	5140
Tangle	|1|G
Instant
Prevent all combat damage that would be dealt this turn.
Each attacking creature doesn't untap during its controller's next untap step. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == player ? count_attackers(player)*5 : -48);
	}


	if( event == EVENT_RESOLVE_SPELL ){
		fog_effect(player, card);
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.state = STATE_ATTACKING;
		new_manipulate_all(player, card, 2, &this_test, ACT_DOES_NOT_UNTAP);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_tectonic_instability(int player, int card, event_t event){
/* CARD_ID_TECTONIC_INSTABILITY	5141
Tectonic Instability	|2|R
Enchantment
Whenever a land enters the battlefield, tap all lands its controller controls. */

	etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_LAND, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		new_manipulate_all(player, card, trigger_cause_controller, &this_test, ACT_TAP);
	}

	return global_enchantment(player, card, event);
}

int card_teferis_moat(int player, int card, event_t event)
{
/* CARD_ID_TEFERIS_MOAT	5143
Teferi's Moat	|3|W|U
Enchantment
As ~ enters the battlefield, choose a color.
Creatures of the chosen color without flying can't attack you. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = 1<<choose_a_color_and_show_legacy(player, card, 1-player, -1);
	}

	if (event == EVENT_ATTACK_LEGALITY && ! is_humiliated(player, card, AL_STATIC) ){
		int clr = get_card_instance(player, card)->info_slot;
		if( affected_card_controller != player && clr &&
			!(get_card_instance(affected_card_controller, affected_card)->regen_status & KEYWORD_FLYING) &&
			(get_color(affected_card_controller, affected_card) & clr) )
		{
			event_result = 1;
		}
	}

	return global_enchantment(player, card, event);
}

static const char* targets_a_land_you_control(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
  int i;
  card_instance_t* instance = get_card_instance(player, card);

  for (i = 0; i < instance->number_of_targets; ++i)
	if (instance->targets[0].player == who_chooses
		&& is_what(instance->targets[0].player, instance->targets[0].card, TYPE_LAND))
	  return NULL;

  return "targets land you control";
}

int card_teferis_response(int player, int card, event_t event)
{
/* CARD_ID_TEFERIS_RESPONSE	5144
Teferi's Response	|1|U
Instant
Counter target spell or ability an opponent controls that targets a land you control. If a permanent's ability is countered this way, destroy that permanent.
Draw two cards. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

  card_instance_t* instance = get_card_instance(player, card);

  target_definition_t td_effect;
  counter_activated_target_definition(player, card, &td_effect, 0);
  td_effect.allowed_controller = 1-player;
  td_effect.extra = target_predicate_extra(targets_a_land_you_control);
  td_effect.special |= TARGET_SPECIAL_EXTRA_FUNCTION;

  target_definition_t td_spell;
  counterspell_target_definition(player, card, &td_spell, 0);
  td_spell.allowed_controller = 1-player;
  td_spell.extra = target_predicate_extra(targets_a_land_you_control);
  td_spell.special |= TARGET_SPECIAL_EXTRA_FUNCTION;

  if (event == EVENT_CAN_CAST)
	{
	  if (counterspell(player, card, event, &td_spell, 0))
		{
		  instance->info_slot = 1;
		  return 99;
		}
	  if (can_counter_activated_ability(player, card, event, &td_effect))
		{
		  instance->info_slot = 2;
		  return 99;
		}
	  return 0;
	}
  else if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  if (instance->info_slot == 1)
		counterspell(player, card, event, &td_spell, 0);
	  else if (instance->info_slot == 2)
		cast_counter_activated_ability(player, card, 0);
	}
  else if (event == EVENT_RESOLVE_SPELL)
	{
	  if (instance->info_slot == 1)
		{
		  if (counterspell_validate(player, card, &td_spell, 0))
			{
			  if (pre_counterspell(player, card, instance->targets[0].player, instance->targets[0].card)){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			  }
			  draw_cards(player, 2);
			}
		}
	  else if (instance->info_slot == 2)
		{
		  if (resolve_counter_activated_ability(player, card, &td_effect, 0))
			{
			  card_instance_t* ability = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			  if (is_what(-1, ability->original_internal_card_id, TYPE_PERMANENT)
				  && in_play(ability->parent_controller, ability->parent_card))
				kill_card(ability->parent_controller, ability->parent_card, KILL_DESTROY);

			  draw_cards(player, 2);
			}
		}
	  kill_card(player, card, KILL_DESTROY);
	}
  else
	return counterspell(player, card, event, &td_spell, 0);

  return 0;
}

int card_tek(int player, int card, event_t event){
  /* Tek |5
   * Artifact Creature — Dragon 2/2
   * Tek gets +0/+2 as long as you control a Plains, has flying as long as you control an Island, gets +2/+0 as long as you control a Swamp,
   * 	has first strike as long as you control a Mountain, and has trample as long as you control a Forest. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES) && affect_me(player, card) ){
			int pow = 0;
			int tou = 0;
			int key = 0;
			CYCLE_ALL_CARDS(p, c, 	{
										if( p == player && in_play(p, c) ){
											if( has_subtype(p, c, SUBTYPE_PLAINS) ){
												tou = 2;
											}
											if( has_subtype(p, c, SUBTYPE_ISLAND) ){
												key |= KEYWORD_FLYING;
											}
											if( has_subtype(p, c, SUBTYPE_SWAMP) ){
												pow = 2;
											}
											if( has_subtype(p, c, SUBTYPE_MOUNTAIN) ){
												key |= KEYWORD_FIRST_STRIKE;
											}
											if( has_subtype(p, c, SUBTYPE_FOREST) ){
												key |= KEYWORD_TRAMPLE;
											}
										}
									};
			);
			switch( event ){
					case EVENT_POWER:
						event_result += pow;
						break;
					case EVENT_TOUGHNESS:
						event_result += tou;
						break;
					case EVENT_ABILITIES:
						event_result |= key;
						break;
					default:
						break;
			}
		}
	}

	return 0;
}

int card_thicket_elemental(int player, int card, event_t event){
/* Thicket Elemental	|3|G|G
 * Creature - Elemental 4/4
 * Kicker |1|G
 * When ~ enters the battlefield, if it was kicked, you may reveal cards from the top of your library until you reveal a creature card.
 * If you do, put that card onto the battlefield and shuffle all other cards revealed this way into your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		reveal_card_until_you_reveal_test(player, player, &test, RCUYRT_PUT_TEST_INTO_PLAY | RCUYRT_SHUFFLE_REST);
	}

	return kicker(player, card, event, MANACOST_XG(1, 1));
}

int card_thornscape_apprentice(int player, int card, event_t event)
{
  /* Thornscape Apprentice	|G
   * Creature - Human Wizard 1/1
   * |R, |T: Target creature gains first strike until end of turn.
   * |W, |T: Tap target creature. */

  if (event == EVENT_CHECK_ABILITIES && affected_card_controller == player &&
      generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_R(1), 0,
                                NULL, NULL))
    check_abilities_keywords |= KEYWORD_FIRST_STRIKE;

  /*
    Thornscape Apprentice |G
    Creature - Human Wizard 1/1
    {R}, {T}: Target creature gains first strike until end of turn.
    {W}, {T}: Tap target creature.
  */
  return invasion_apprentices(player, card, event, COLOR_RED, COLOR_WHITE);
}

int card_thornscape_master(int player, int card, event_t event){
/* CARD_ID_THORNSCAPE_MASTER	5148
Thornscape Master	|2|G|G
Creature - Human Wizard 2/2
|R|R, |T: ~ deals 2 damage to target creature.
|W|W, |T: Target creature gains protection from the color of your choice until end of turn. */

	return invasion_masters(player, card, event, COLOR_RED, COLOR_WHITE);
}

int card_thunderscape_apprentice(int player, int card, event_t event){
  /* Thunderscape Apprentice |R
   * 	  Creature - Human Wizard 1/1
   * 	  {B}, {T}: Target player loses 1 life.
   * 	  {G}, {T}: Target creature gets +1/+1 until end of turn. */

	return invasion_apprentices(player, card, event, COLOR_BLACK, COLOR_GREEN);
}

int card_thunderscape_master(int player, int card, event_t event){
/* CARD_ID_THUNDERSCAPE_MASTER	5149
Thunderscape Master	|2|R|R
Creature - Human Wizard 2/2
|B|B, |T: Target player loses 2 life and you gain 2 life.
|G|G, |T: Creatures you control get +2/+2 until end of turn. */

	return invasion_masters(player, card, event, COLOR_BLACK, COLOR_GREEN);
}

// tigereye cameo --> bloodstone cameo

int card_tinder_farm(int player, int card, event_t event){
/* CARD_ID_TINDER_FARM	5152
Tinder Farm	""
Land
~ enters the battlefield tapped.
|T: Add |G to your mana pool.
|T, Sacrifice ~: Add |R|W to your mana pool. */

	return sac_land_tapped(player, card, event, COLOR_GREEN, COLOR_RED, COLOR_WHITE);
}

// trench wurm --> Dwarven miner

int card_treva_the_renewer(int player, int card, event_t event){
  /* Treva, the Renewer |3|W|U|G
   * Legendary Creature — Dragon 6/6
   * Flying
   * Whenever Treva, the Renewer deals combat damage to a player, you may pay {2}{W}.
   * If you do, choose a color, then you gain 1 life for each permanent of that color. */

	check_legend_rule(player, card, event);

	invasion_dragon(player, card, event, MANACOST_XW(2, 1));

	if( event == EVENT_DAMAGE_TRIGGER_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = 1<<choose_a_color(player, get_deck_color(player, 1-player));

		int amount = check_battlefield_for_special_card(player, card, ANYBODY, CBFSC_GET_COUNT, &this_test);
		gain_life(player, amount);
	}

	return 0;
}

int card_tribal_flames(int player, int card, event_t event){
	/* Tribal Flames	|1|R	0x20017c7
	 * Sorcery
	 * Domain - ~ deals X damage to any target, where X is the number of basic land types among lands you control. */
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td)){
			damage_target0(player, card, count_domain(player, card));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

// troll horn cameo --> mox sapphire

int card_tsabo_tavoc(int player, int card, event_t event)
{
  /* Tsabo Tavoc	|5|B|R
   * Legendary Creature - Horror 7/4
   * First strike, protection from legendary creatures
   * |B|B, |T: Destroy target legendary creature. It can't be regenerated. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  check_legend_rule(player, card, event);
  misc_protection(player, card, event);
  if (event == EVENT_PROT_MISC)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        ((((unsigned int)event_result >> 24 & 0xff) == '\x06' ||
          ((result = is_what(attacking_card_controller, attacking_card, 2),
            result != 0 &&
                (result = is_legendary(attacking_card_controller, attacking_card), result != 0))))))
    {
      event_result = event_result | 1;
    }
  }
  else if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
            (event == EVENT_RESOLVE_ACTIVATION)) ||
           (event == EVENT_CLEANUP))
  {
    default_target_definition(player, card, &td, 2);
    td.required_subtype = 0x2001;
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      instance = get_card_instance(player, card);
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
    }
    result = generic_activated_ability(player, card, event, 7, 0, 2, 0, 0, 0, 0, 0, &td,
                                       "Select target Legendary creature.");
    return result;
  }
  return 0;
}

int card_tsabos_decree(int player, int card, event_t event){

	/* Tsabo's Decree	|5|B
	 * Instant
	 * Choose a creature type. Target player reveals his or her hand and discards all creature cards of that type. Then destroy all creatures of that type that
	 * player controls. They can't be regenerated. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}


	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.subtype = select_a_subtype(player, card);

			reveal_target_player_hand(instance->targets[0].player);

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			this_definition.effect = EC_ALL_WHICH_MATCH_CRITERIA | EC_DISCARD;

			new_effect_coercion(&this_definition, &this_test);
			new_manipulate_all(player, card, instance->targets[0].player, &this_test, KILL_BURY);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

static int tsabo_web_exceptions(int csvid){
	if( csvid == CARD_ID_BOTTOMLESS_VAULT ||
		csvid == CARD_ID_DWARVEN_HOLD ||
		csvid == CARD_ID_HOLLOW_TREES ||
		csvid == CARD_ID_ICATIAN_STORE ||
		csvid == CARD_ID_SAND_SILOS ||
		csvid == CARD_ID_CRYSTAL_QUARRY ||
		csvid == CARD_ID_DARKWATER_CATACOMBS ||
		csvid == CARD_ID_MOSSFIRE_VALLEY ||
		csvid == CARD_ID_SHADOWBLOOD_RIDGE ||
		csvid == CARD_ID_SKYCLOUD_EXPANSE ||
		csvid == CARD_ID_SUNGRASS_PRAIRIE ||
		csvid == CARD_ID_SHIMMERING_GROTTO ||
		csvid == CARD_ID_UNKNOWN_SHORES
	  ){
		return 1;
	}
	return 0;
}

int card_tsabos_web(int player, int card, event_t event){
/* CARD_ID_TSABOS_WEB	5164
Tsabo's Web	|2
Artifact
When ~ enters the battlefield, draw a card.
Each land with an activated ability that isn't a mana ability doesn't untap during its controller's untap step. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && ! is_humiliated(player, card, AL_STATIC) ){
		if( is_what(affected_card_controller, affected_card, TYPE_LAND) ){
			card_instance_t *instance= get_card_instance(affected_card_controller, affected_card);
			int affected_land = 0;
			if( (cards_data[instance->internal_card_id].extra_ability & EA_ACT_ABILITY) &&
				! tsabo_web_exceptions(get_id(affected_card_controller, affected_card))
			  ){
				affected_land = 1;
			}
			if( ! affected_land ){
				int special_value = cards_data[instance->internal_card_id].cc[2];
				if( special_value == 4 || special_value == 9 ){ //AKA have a activated ability managed via the Rules Engine card.
					affected_land = 1;
				}
			}
			if( affected_land ){
				does_not_untap(affected_card_controller, affected_card, event);
			}
		}
	}

	return 0;
}

int card_twilights_call(int player, int card, event_t event){
/* CARD_ID_TWILIGHTS_CALL	5166
Twilight's Call	|4|B|B
Sorcery
You may cast ~ as though it had flash if you pay |2 more to cast it. (You may cast it any time you could cast an instant.)
Each player returns all creature cards from his or her graveyard to the battlefield. */

	spell_can_be_cast_as_instant_by_paying_2(player, card, event);

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		APNAP(p, {new_reanimate_all(p, -1, p, &this_test, REANIMATE_DEFAULT);});
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_undermine(int player, int card, event_t event){
  /* Undermine |U|U|B
   * 	  Instant
   * 	  Counter target spell. Its controller loses 3 life. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			lose_life(instance->targets[0].player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 0, NULL);
}

int card_urborg_emissary(int player, int card, event_t event){
  /* Urborg Emissary |2|B
   * Creature — Human Wizard 3/1
   * Kicker {1}{U} (You may pay an additional {1}{U} as you cast this spell.)
   * When Urborg Emissary enters the battlefield, if it was kicked, return target permanent to its owner's hand. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allowed_controller = 1-player;
		if( player == HUMAN || (player == AI || can_target(&td)) ){
			return kicker(player, card, event, MANACOST_XU(1, 1));
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_urzas_filter(int player, int card, event_t event){
/* CARD_ID_URZAS_FILTER	5172
Urza's Filter	|4
Artifact
Multicolored spells cost up to |2 less to cast. */

	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card, AL_STATIC) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		this_test.color_flag = F3_MULTICOLORED;
		if( new_make_test_in_play(affected_card_controller, affected_card, -1, &this_test) ){
			COST_COLORLESS-=2;
		}
	}

	return 0;
}

int card_urzas_rage(int player, int card, event_t event){
	/* Urza's Rage	|2|R	0x2008a0e
	 * Instant
	 * Kicker |8|R
	 * This spell can't be countered.
	 * ~ deals 3 damage to any target. If this spell was kicked, instead it deals 10 damage to that permanent or player and the damage can't be prevented. */
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 ){
			state_untargettable(player, card, 1);
			do_kicker(player, card, MANACOST_XR(8,1));
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		state_untargettable(player, card, 0);
		if( valid_target(&td) ){
			int dmg = 3;
			if( kicked(player, card) ){
				dmg = 10;
				int legacy = create_legacy_effect(player, card, &my_damage_cannot_be_prevented);
				get_card_instance(player, legacy)->targets[0].player = player;
				get_card_instance(player, legacy)->targets[0].card = card;
			}
			damage_target0(player, card, dmg);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_utopia_tree(int player, int card, event_t event){
/* CARD_ID_UTOPIA_TREE	1200
Utopia Tree	|1|G
Creature - Plant 0/2
|T: Add one mana of any color to your mana pool. */

	return plain_mana_producer(player, card, event);
}

int card_verdeloth_the_ancient(int player, int card, event_t event){
/* Verdeloth the Ancient	|4|G|G
 * Legendary Creature - Treefolk 4/7
 * Kicker |X
 * Saproling creatures and other Treefolk creatures get +1/+1.
 * When ~ enters the battlefield, if it was kicked, put X 1/1 |Sgreen Saproling creature tokens onto the battlefield. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, get_card_instance(player, card)->info_slot);
	}

	boost_creature_type(player, card, event, SUBTYPE_SAPROLING, 1, 1, 0, BCT_INCLUDE_SELF);

	boost_creature_type(player, card, event, SUBTYPE_TREEFOLK, 1, 1, 0, 0);

	return kicker(player, card, event, MANACOST_X(-1));
}

int card_verduran_emissary(int player, int card, event_t event){
/* CARD_ID_VERDURAN_EMISSARY	5175
Verduran Emissary	|2|G
Creature - Human Wizard 2/3
Kicker |1|R (You may pay an additional |1|R as you cast this spell.)
When ~ enters the battlefield, if it was kicked, destroy target artifact. It can't be regenerated. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.allowed_controller = 1-player;
		if( player == HUMAN || (player == AI || can_target(&td)) ){
			return kicker(player, card, event, MANACOST_XR(1, 1));
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && kicked(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_ARTIFACT") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_vicious_kavu(int player, int card, event_t event)
{
  /* CARD_ID_VICIOUS_KAVU	5177
Vicious Kavu	|1|B|R
Creature - Kavu 2/2
Whenever ~ attacks, it gets +2/+0 until end of turn. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
    pump_until_eot(player, card, player, card, 2, 0);
  return 0;
}

int card_vigorous_charge(int player, int card, event_t event){
/* CARD_ID_VIGOROUS_CHARGE	5178
Vigorous Charge	|G
Instant
Kicker |W (You may pay an additional |W as you cast this spell.)
Target creature gains trample until end of turn. Whenever that creature deals combat damage this turn, if ~ was kicked, you gain life equal to that damage.*/

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST || event == EVENT_CAN_CHANGE_TARGET || event == EVENT_CHANGE_TARGET){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( pick_target(&td, "TARGET_CREATURE") ){
			if( ! is_token(player, card) ){
				do_kicker(player, card, MANACOST_W(1));
			}
		}
	}
	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			int legacy = pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_TRAMPLE, 0);
			if( kicked(player, card) ){
				card_instance_t* leg = get_card_instance(player, legacy);
				if (leg->targets[4].card == -1){
					leg->targets[4].card = 0;
				}
				leg->targets[4].card |= (player == 0 ? 1 : 2);
			}
		}
		kill_card(player, card, KILL_DESTROY );
	}


	return 0;
}

int card_vile_consumption(int player, int card, event_t event){
/* CARD_ID_VILE_CONSUMPTION	5179
Vile Consumption	|1|U|B
Enchantment
All creatures have "At the beginning of your upkeep, sacrifice this creature unless you pay 1 life." */

	if( upkeep_trigger(player, card, event) ){
		int count = active_cards_count[current_turn]-1;
		while( count > -1 ){
				if( in_play(current_turn, count) && is_what(current_turn, count, TYPE_CREATURE) ){
					int ai_choice = 0;
					int choice = 1;
					if( can_pay_life(current_turn, 1) ){
						if( life[player] < 7 ){
							ai_choice = 1;
						}
						choice = do_dialog(current_turn, current_turn, count, -1, -1, " Pay 1 life\n Sac this", ai_choice);
					}
					if( choice == 0 ){
						lose_life(current_turn, 1);
					}
					else{
						kill_card(current_turn, count, KILL_SACRIFICE);
					}
				}
				count--;
		}
	}

	return global_enchantment(player, card, event);
}

int card_void(int player, int card, event_t event){
/* CARD_ID_VOID	5184
Void	|3|B|R
Sorcery
Choose a number. Destroy all artifacts and creatures with converted mana cost equal to that number.
Then target player reveals his or her hand and discards all nonland cards with converted mana cost equal to the number. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int number = 0;
			int i;
			if( player == AI ){
				int cmc[15];

				for(i=0;i<15; i++){
					cmc[i] = 0;
				}

				i=0;
				for(i=0;i<active_cards_count[1-player]; i++){
					if( in_play(1-player, i) && (is_what(1-player, i, TYPE_CREATURE) || is_what(1-player, i, TYPE_ARTIFACT)) ){
						cmc[get_cmc(1-player, i)]++;
					}
				}

				int most_common_cmc = -1;
				int par = 0;
				i=0;
				for(i=0;i<15; i++){
					if( cmc[i] > 0 && cmc[i] > par ){
						par = cmc[i];
						most_common_cmc = i;
					}
				}
				number = most_common_cmc;
			}

			else{
				number = choose_a_number(player, "Choose a number.", 15);
			}
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE | TYPE_ARTIFACT);
			this_test.cmc = number;

			new_manipulate_all(player, card, 2, &this_test, KILL_DESTROY);

			default_test_definition(&this_test, TYPE_LAND);
			this_test.type_flag = DOESNT_MATCH;
			this_test.cmc = number;

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			this_definition.effect = EC_ALL_WHICH_MATCH_CRITERIA | EC_DISCARD;
			new_effect_coercion(&this_definition, &this_test);

		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_wax_wane(int player, int card, event_t event)
{
  /* Wax // Wane	|G // |W
   * Instant // Instant
   * Wax:
   * Target creature gets +2/+2 until end of turn.
   *
   * Wane:
   * Destroy target enchantment. */

  card_instance_t *instance;
  int available_or_valid;
  unsigned int selected_modes;
  target_definition_t *enchantment_target;
  target_definition_t td;
  target_definition_t td2;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    default_target_definition(player, card, &td2, 4);
    available_or_valid = can_target(&td2);
    split_card_impl(player, card, EVENT_MODIFY_COST, 0x4b45, can_target(&td), 0, 0x4b44,
                    available_or_valid, 0, 0);
    return 0;
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = player;
    pick_target(&td2, "TARGET_CREATURE");
    return 0;
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    default_target_definition(player, card, &td2, 4);
    pick_target(&td2, "TARGET_ENCHANTMENT");
    return 0;
  }
  if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  default_target_definition(player, card, &td2, 4);
  if (event == EVENT_RESOLVE_SPELL)
  {
    selected_modes = instance->info_slot;
    if ((selected_modes & 1) != 0)
    {
      available_or_valid = valid_target(&td);
      if (available_or_valid == 0)
      {
        selected_modes = instance->info_slot;
      }
      else
      {
        pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
        selected_modes = instance->info_slot;
      }
    }
    enchantment_target = &td2;
    if (((selected_modes & 2) != 0) &&
        (available_or_valid = valid_target(enchantment_target), available_or_valid != 0))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  enchantment_target = &td2;
  available_or_valid = can_target(enchantment_target);
  return split_card_impl(player, card, event, 0x4b45, can_target(&td), 8, 0x4b44,
                         available_or_valid, 10, 0);
}

int card_wash_out(int player, int card, event_t event){
/* CARD_ID_WASH_OUT	5188
Wash Out	|3|U
Sorcery
Return all permanents of the color of your choice to their owners' hands. */

	if(event == EVENT_RESOLVE_SPELL ){
		int clr = 1<<choose_a_color(player, get_deck_color(player, 1-player));
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = clr;
		new_manipulate_all(player, card, 2, &this_test, ACT_BOUNCE);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x24952e0; original cards/invasion.c:4585. */
int card_whip_silk(int player, int card, event_t event)
{
  /* Whip Silk	|G
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has reach. (It can block creatures with flying.)
   * |G: Return this Aura to its owner's hand. */

  card_instance_t *instance;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player != -1)
    {

      instance = get_card_instance(player, card);
      if (event != EVENT_ACTIVATE)
      {
        if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
          return generic_aura(player, card, event, player, 0, 0, 0x400, 0, 0, 0, 0);
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          bounce_permanent(instance->parent_controller, instance->parent_card);
        }
      }
      return generic_activated_ability(player, card, event, 0, 0, 0, 0, 1, 0, 0, 0, NULL, NULL);
    }
  }
  return generic_aura(player, card, event, player, 0, 0, 0x400, 0, 0, 0, 0);
}

int card_yawgmoths_agenda(int player, int card, event_t event)
{
  /* Yawgmoth's Agenda	|3|B|B
   * Enchantment
   * You can't cast more than one spell each turn.
   * You may play cards from your graveyard.
   * If a card would be put into your graveyard from anywhere, exile it instead. */

	if_a_card_would_be_put_into_graveyard_from_anywhere_exile_it_instead(player, card, event, player, NULL);

	if (event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && get_specific_storm_count(player) > 0
		&& !is_what(affected_card_controller, affected_card, TYPE_LAND) && ! is_humiliated(player, card, AL_STATIC) )
		infinite_casting_cost();

	if ((event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE) && ! is_humiliated(player, card, AL_STATIC)){
		type_t types = TYPE_ANY;
		if (get_specific_storm_count(player) > 0)
			types = TYPE_LAND;

		return can_activate_to_play_cards_from_graveyard(player, card, event, types);
	}

	return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2495510; original cards/invasion.c:4631. */
int card_zap(int player, int card, event_t event)
{
  /* Zap	|2|R
   * Instant
   * ~ deals 1 damage to any target.
   * Draw a card. */

  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      damage_target0(player, card, 1);
      draw_cards(player, 1);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_ANY", 1, NULL);
}

/* ManalinkEh 2022: 0x2495660; original cards/invasion.c:4659. */
int card_agonizing_demise(int player, int card, event_t event)
{
  /* Agonizing Demise	|3|B
   * Instant
   * Kicker |1|R (You may pay an additional |1|R as you cast this spell.)
   * Destroy target nonblack creature. It can't be regenerated. If this spell was kicked, ~ deals damage equal to that creature's power to the creature's controller. */

  const char *prompt;
  card_instance_t *instance;
  int p;
  int target_card;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.illegal_color = get_sleighted_color_test(player, card, 2);
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.illegal_color = get_sleighted_color_test(player, card, 2);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        prompt = get_sleighted_color_text(player, card, "Select target non%s creature.", 1);
        if (pick_next_target_noload(&td, prompt) != 0)
        {
          do_kicker(player, card, 1, 0, 0, 0, 1, 0);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.illegal_color = get_sleighted_color_test(player, card, 2);
    if (event == EVENT_RESOLVE_SPELL)
    {

      instance = get_card_instance(player, card);
      if (valid_target(&td) != 0)
      {

        p = instance->targets[0].player;
        target_card = instance->targets[0].card;
        if (check_special_flags(player, card, 0x80) == 0)
        {
          kill_card(p, target_card, KILL_BURY);
        }
        if (check_special_flags(player, card, 0x80) != 0)
        {
          kill_card(p, target_card, KILL_BURY);
          damage_player(p, get_power(p, target_card), player, card);
        }
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 2);
    get_sleighted_color_test(player, card, 2);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2495940; original cards/invasion.c:4703. */
int card_alabaster_leech(int player, int card, event_t event)
{
  /* Alabaster Leech	|W
   * Creature - Leech 1/3
   * White spells you cast cost |W more to cast. */

  unsigned int affected_color;
  unsigned int spell_color;

  if ((event == EVENT_MODIFY_COST_GLOBAL) && (affected_card_controller == player))
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {

      affected_color = get_sleighted_color_test(player, card, 0x20);
      spell_color = get_color(affected_card_controller, affected_card);
      if ((spell_color & affected_color) != 0)
      {
        COST_WHITE = COST_WHITE + '\x01';
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x24959d0; original cards/invasion.c:4719. */
int card_alloy_golem(int player, int card, event_t event)
{
  /* Alloy Golem	|6
   * Artifact Creature - Golem 4/4
   * As this creature enters, choose a color.
   * This creature is the chosen color. (It's still an artifact.) */

  int result;
  card_instance_t *instance;

  if (event != EVENT_RESOLVE_SPELL)
  {
    if (event == EVENT_SET_COLOR)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {

        instance = get_card_instance(player, card);
        result = instance->targets[1].card;
        if (((0 < result) && (affected_card_controller == player)) && (affected_card == card))
        {
          event_result = result;
        }
      }
    }
    return 0;
  }
  result = choose_a_color_and_show_legacy(player, card, player, -1);
  instance = get_card_instance(player, card);
  instance->targets[1].card = 1 << ((unsigned char)result & 0x1f);
  return 0;
}

/* ManalinkEh 2022: 0x2495ab0; original cards/invasion.c:4739. */
int card_andradite_leech(int player, int card, event_t event)
{
  /* Andradite Leech	|2|B
   * Creature - Leech 2/2
   * Black spells you cast cost |B more to cast.
   * |B: This creature gets +1/+1 until end of turn. */

  unsigned int affected_color;
  unsigned int spell_color;

  if ((event == EVENT_MODIFY_COST_GLOBAL) && (affected_card_controller == player))
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {

      affected_color = get_sleighted_color_test(player, card, 2);
      spell_color = get_color(affected_card_controller, affected_card);
      if ((affected_color & spell_color) != 0)
      {
        COST_BLACK = COST_BLACK + '\x01';
      }
    }
  }
  return card_nantuko_shade(player, card, event);
}

/* ManalinkEh 2022: 0x2495b40; original cards/invasion.c:4756. */
int card_archaeological_dig(int player, int card, event_t event)
{
  /* Archaeological Dig	""
   * Land
   * |T: Add |C.
   * |T, Sacrifice this land: Add one mana of any color. */

  card_instance_t *instance;
  color_t color;
  unsigned int color_mask;
  int result;
  unsigned int ordinary_colors;
  bool fizzled;

  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    instance->info_slot = 1;
    play_land_sound_effect_force_color(player, card, 0x3e);
    count_mana();
    return 0;
  }
  if (event == EVENT_CHANGE_TYPE)
  {

    result = can_sacrifice_this_as_cost(player, card);
    color_mask = 0x3f;
    if (result == 0)
    {
      color_mask = 1;
      result = is_what(player, card, 1);
      if (result != 0)
      {
        color_mask = mana_colors_added[player] | 1;
      }
    }
    instance = get_card_instance(player, card);
    instance->info_slot = color_mask;
    return 0;
  }
  if (event == EVENT_COUNT_MANA)
  {
    if (card != affected_card)
    {
      return 0;
    }
    if (player != affected_card_controller)
    {
      return 0;
    }
    result = is_tapped(player, card);
    if (result != 0)
    {
      return 0;
    }
    result = is_animated_and_sick(player, card);
    if (result != 0)
    {
      return 0;
    }
    result = can_produce_mana(player, card);
    if (result == 0)
    {
      return 0;
    }
    result = can_sacrifice_this_as_cost(player, card);
    if (result != 0)
    {
      declare_mana_available_hex(player, 0x3f, 1);
      return 0;
    }
    declare_mana_available(player, 0, 1);
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    instance = get_card_instance(player, card);
    if (instance->targets[1].player == 0x42)
    {
      return 0;
    }
    result = mana_producer(player, card, EVENT_CAN_ACTIVATE);
    return (unsigned int)(result != 0);
  }
  if (event != EVENT_ACTIVATE)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  if (instance->targets[1].player == 0x42)
  {
    return 0;
  }
  result = can_sacrifice_this_as_cost(player, card);
  if (result == 0)
  {
    result = is_what(player, card, 1);
    color = COLOR_COLORLESS;
    if (result != 0 && mana_colors_added[player] != 0)
      color = choose_a_color_exe(player, "What kind of mana?", 1, 0, mana_colors_added[player] | 1);
  }
  else
  {
    color = choose_mana_all_one_color(player, COLOR_TEST_ANY, 1, "What kind of mana?",
                                      COLOR_TEST_COLORLESS);
  }
  if (color == ~COLOR_COLORLESS)
  {
    spell_fizzled = 1;
    return 0;
  }
  color_mask = color_to_color_test(color);
  result = is_what(player, card, 1);
  ordinary_colors = 1;
  if (result != 0)
  {
    ordinary_colors = mana_colors_added[player] | 1;
  }
  if ((color_mask & ordinary_colors) == 0)
  {
    instance = get_card_instance(player, card);
    instance->targets[1].player = 0x42;
    kill_card(player, card, KILL_SACRIFICE);
    instance = get_card_instance(player, card);
    fizzled = spell_fizzled == 1;
    instance->targets[1].player = 0;
    if (fizzled)
    {
      return 0;
    }
  }
  produce_mana_tapped_impl(player, card, color, 1, 0, 1);
  return 0;
}

/* ManalinkEh 2022: 0x2495e80; original cards/invasion.c:4816. */
int card_ardent_soldier(int player, int card, event_t event)
{
  /* Ardent Soldier	|1|W
   * Creature - Human Soldier 1/2
   * Kicker |2 (You may pay an additional |2 as you cast this spell.)
   * Vigilance
   * If this creature was kicked, it enters with a +1/+1 counter on it. */

  int result;
  kicker(player, card, event, 2, 0, 0, 0, 0, 0);
  result = check_special_flags(player, card, 0x80);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1,
                                       (unsigned int)(result != 0));
  return 0;
}

/* ManalinkEh 2022: 0x2495f20; original cards/invasion.c:4829. */
int card_armored_guardian(int player, int card, event_t event)
{
  /* Armored Guardian	|3|W|U
   * Creature - Cat Soldier 2/5
   * |1|W|W: Target creature you control gains protection from the color of your choice until end of turn.
   * |1|U|U: This creature gains shroud until end of turn. (It can't be the target of spells or abilities.) */

  enum
  {
    CHOICE_PROTECT = 1,
    CHOICE_SHROUD = 2
  };

  int choice;
  card_instance_t *instance;
  int blue_mana_or_source_player;
  int flags_or_mode;
  unsigned int protection;
  int white_mana_or_source_card;
  target_definition_t *td_00;
  const char *prompt;
  target_definition_t td;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    td_00 = &td;
    default_target_definition(player, card, td_00, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 0, 0, 0, 0, 2, 0, td_00,
                                    NULL) == 0)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 2, 0, 0, 0, 0,
                                         NULL, NULL);
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      flags_or_mode = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 0, 0, 0, 0,
                                                2, 0, td_00, NULL);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                           "Target gains protection", flags_or_mode, 0xf, "Gains Shroud",
                           generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 2,
                                                     0, 0, 0, 0, NULL, NULL),
                           10, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_PROTECT)
      {
        prompt = "TARGET_CREATURE_YOU_CONTROL";
        white_mana_or_source_card = 2;
        blue_mana_or_source_player = 0;
        flags_or_mode = 4;
      }
      else
      {
        if (choice != CHOICE_SHROUD)
        {
          return 0;
        }
        prompt = NULL;
        td_00 = NULL;
        white_mana_or_source_card = 0;
        blue_mana_or_source_player = 2;
        flags_or_mode = 0;
      }
      return generic_activated_ability(player, card, EVENT_ACTIVATE, flags_or_mode, 1, 0,
                                       blue_mana_or_source_player, 0, 0, white_mana_or_source_card,
                                       0, td_00, prompt);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    flags_or_mode = instance->info_slot;
    blue_mana_or_source_player = instance->parent_controller;
    white_mana_or_source_card = instance->parent_card;
    if (flags_or_mode == CHOICE_PROTECT)
    {
      if (valid_target(&td) == 0)
      {
        flags_or_mode = instance->info_slot;
      }
      else
      {
        protection = select_a_protection(player);
        pump_ability_until_eot_no_repeat_alt(blue_mana_or_source_player, white_mana_or_source_card,
                                             instance->targets[0].player, instance->targets[0].card,
                                             protection | 0x10000000, 0, 1);
        flags_or_mode = instance->info_slot;
      }
    }
    if (flags_or_mode == CHOICE_SHROUD)
    {
      pump_ability_until_eot_no_repeat_alt(blue_mana_or_source_player, white_mana_or_source_card,
                                           blue_mana_or_source_player, white_mana_or_source_card,
                                           0x20000, 0, 2);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2488800; original cards/invasion.c:4897. */
/* The release specializes player to 2 (both players). */
static int most_common_color_all_permanents(int player)
{
  int counts[6] = {0};
  int p, c, color, most = 0, result = 0;
  for (p = 0; p < 2; ++p)
    for (c = 0; c < active_cards_count[p]; ++c)
      if (in_play(p, c) && is_what(p, c, 0x47))
      {
        unsigned int colors = get_color(p, c);
        for (color = 1; color < 6; ++color)
          if (colors & (1U << color))
            ++counts[color];
      }
  for (color = 1; color < 6; ++color)
    if (counts[color] > most)
      most = counts[color];
  for (color = 1; color < 6; ++color)
    if (counts[color] >= most)
      result |= 1U << color;
  return result;
}

/* ManalinkEh 2022: 0x24963b0; original cards/invasion.c:4925. */
int card_barrins_unmaking(int player, int card, event_t event)
{
  /* Barrin's Unmaking	|1|U
   * Instant
   * Return target permanent to its owner's hand if that permanent shares a color with the most common color among all permanents or a color tied for most common. */

  card_instance_t *instance;
  unsigned int target_colors;
  unsigned int common_colors;
  target_definition_t td;

  default_target_definition(player, card, &td, 0x47);
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_PERMANENT", 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_PERMANENT", 1, NULL);
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      target_colors = get_color(instance->targets[0].player, instance->targets[0].card);
      common_colors = most_common_color_all_permanents(2);
      if ((target_colors & common_colors) != 0)
      {
        bounce_permanent(instance->targets[0].player, instance->targets[0].card);
      }
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2496500; original cards/invasion.c:4955. */
int card_benalish_emissary(int player, int card, event_t event)
{
  /* Benalish Emissary	|2|W
   * Creature - Human Wizard 1/4
   * Kicker |1|G (You may pay an additional |1|G as you cast this spell.)
   * When this creature enters, if it was kicked, destroy target land. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  kicker(player, card, event, 1, 0, 0, 1, 0, 0);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    result = check_special_flags(player, card, 0x80);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      default_target_definition(player, card, &td, 1);
      td.preferred_controller = 1 - player;
      td.allow_cancel = 0;
      result = can_target(&td);
      if (result != 0)
      {
        result = pick_target(&td, "TARGET_LAND");
        if (result != 0)
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
      }
      instance->number_of_targets = '\0';
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2496620; original cards/invasion.c:4980. */
int card_benalish_lancer(int player, int card, event_t event)
{
  /* Benalish Lancer	|2|W
   * Creature - Human Knight 2/2
   * Kicker |2|W (You may pay an additional |2|W as you cast this spell.)
   * If this creature was kicked, it enters with two +1/+1 counters on it and with first strike. */

  int result;

  if (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller)
  {
    do_kicker(player, card, 2, 0, 0, 0, 0, 1);
  }
  result = check_special_flags(player, card, 0x80);
  if (result == 0 && event == EVENT_CAST_SPELL)
    return 0;
  if (result != 0)
  {
    enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);
  }
  if ((((event == EVENT_ABILITIES) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((result = is_humiliated(player, card, AL_STATIC),
        result == 0 && (result = check_special_flags(player, card, 0x80), result != 0))))
  {
    event_result = event_result | 0x100;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2496760; original cards/invasion.c:5002. */
int card_bend_or_break(int player, int card, event_t event) {
  /* Bend or Break	|3|R
   * Sorcery
   * Each player separates all nontoken lands they control into two piles. For each player, one of their piles is chosen by one of their opponents of their choice. Destroy all lands in the chosen piles. Tap all lands in the other piles. */

 return 0; }

/* ManalinkEh 2022: 0x2496770; original cards/invasion.c:5011. */
int card_blind_seer(int player, int card, event_t event)
{
  /* Blind Seer	|2|U|U
   * Legendary Creature - Human Wizard 3/3
   * |1|U: Target spell or permanent becomes the color of your choice until end of turn. */

  enum
  {
    CHOICE_CHANGE_COLOR_OF_PERMANENT = 1,
    CHOICE_CHANGE_COLOR_OF_SPELL = 2
  };

  card_instance_t *instance;
  int result;
  event_t event_00;
  target_definition_t *td_00;
  const char *prompt;
  target_definition_t td;
  target_definition_t td_spell;

  if ((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE))
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x47);
    counterspell_target_definition(player, card, &td_spell, 0xff);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      instance = get_card_instance(player, card);
      result = instance->info_slot;
      if (result - 2U < 2)
      {
        result = counterspell_validate(player, card, &td_spell, 0);
        if (result != 0)
        {
          result = get_deck_color(player, 1 - player);
          result = choose_a_color(player, result);
          change_color(player, card, instance->targets[0].player, instance->targets[0].card,
                       1 << ((unsigned char)result & 0x1f),
                       CHANGE_COLOR_END_AT_EOT | CHANGE_COLOR_SET);
        }
        result = instance->info_slot;
      }
      if ((result == CHOICE_CHANGE_COLOR_OF_PERMANENT) && (result = valid_target(&td), result != 0))
      {
        result = get_deck_color(player, 1 - player);
        result = choose_a_color(player, result);
        change_color(instance->parent_controller, instance->parent_card,
                     instance->targets[0].player, instance->targets[0].card,
                     1 << ((unsigned char)result & 0x1f),
                     CHANGE_COLOR_END_AT_EOT | CHANGE_COLOR_SET);
      }
    }
    return 0;
  }
  td_00 = &td;
  default_target_definition(player, card, td_00, 0x47);
  counterspell_target_definition(player, card, &td_spell, 0xff);
  if (event == EVENT_CAN_ACTIVATE)
  {

    result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x400, 1, 0, 1, 0, 0, 0, 0,
                                       &td_spell, NULL);
    if (result != 0)
    {
      return result;
    }
    prompt = NULL;
    event_00 = EVENT_CAN_ACTIVATE;
  }
  else
  {

    instance = get_card_instance(player, card);
    instance->info_slot = 0;
    instance->number_of_targets = '\0';
    result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x400, 1, 0, 1, 0, 0, 0, 0,
                                       &td_spell, NULL);
    if (result != 0)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0x400, 1, 0, 1, 0, 0, 0, 0, td_00,
                                NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      instance->info_slot = 3;
      return 0;
    }

    result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 0, 1, 0, 0, 0, 0,
                                       td_00, NULL);
    if ((result == 0) || (result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_OMIT_ILLEGAL,
                                               "Target permanent", result, 1, 0, 0, 0, 0),
                          result == 0))
    {
      spell_fizzled = 1;
      return 0;
    }
    if (result == CHOICE_CHANGE_COLOR_OF_PERMANENT)
    {
      prompt = "TARGET_PERMANENT";
    }
    else
    {
      if (result != CHOICE_CHANGE_COLOR_OF_SPELL)
      {
        return 0;
      }
      prompt = "TARGET_ANY";
      td_00 = &td_spell;
    }
    event_00 = EVENT_ACTIVATE;
  }
  return generic_activated_ability(player, card, event_00, 4, 1, 0, 1, 0, 0, 0, 0, td_00, prompt);
}

/* ManalinkEh 2022: 0x2496cb0; original cards/invasion.c:5096. */
int card_bog_initiate(int player, int card, event_t event)
{
  /* Bog Initiate	|1|B
   * Creature - Human Wizard 1/1
   * |1: Add |B. */

  int result;

  if (event == EVENT_COUNT_MANA)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = has_mana(player, 7, 1), result != 0)) &&
        (result = can_produce_mana(player, card), result != 0))
    {
      declare_mana_available(player, 1, 1);
    }
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    result = can_use_activated_abilities(player, card);
    if (result != 0)
    {
      result = has_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0);
      return result;
    }
  }
  else if ((event == EVENT_ACTIVATE) &&
           (charge_mana_multi(player, 1, 0, 0, 0, 0, 0), spell_fizzled != 1))
  {
    produce_mana(player, 1, 1);
    tapped_for_mana_color = -2;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2496e20; original cards/invasion.c:5123. */
int card_callous_giant(int player, int card, event_t event)
{
  /* Callous Giant	|4|R|R
   * Creature - Giant 4/4
   * If a source would deal 3 or less damage to this creature, prevent that damage. */

  card_instance_t *instance;

  if (event == EVENT_PREVENT_DAMAGE)
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {

      instance = get_card_instance(affected_card_controller, affected_card);
      if ((((instance->internal_card_id == damage_card) &&
            (instance->damage_target_card == card)) &&
           (instance->damage_target_player == player)) &&
          (instance->info_slot < 4))
      {

        prevent_damage(affected_card_controller, affected_card, instance->info_slot);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2496eb0; original cards/invasion.c:5141. */
int card_capashen_unicorn(int player, int card, event_t event)
{
  /* Capashen Unicorn	|1|W
   * Creature - Unicorn 1/2
   * |1|W, |T, Sacrifice this creature: Destroy target artifact or enchantment. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 0x44);
  instance = get_card_instance(player, card);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
  }
  result =
      generic_activated_ability(player, card, event, 0xd, 1, 0, 0, 0, 0, 1, 0, &td, "DISENCHANT");
  return result;
}

/* ManalinkEh 2022: 0x2496ff0; original cards/invasion.c:5165. */
int card_chaotic_strike(int player, int card, event_t event)
{
  /* Chaotic Strike	|1|R
   * Instant
   * Cast this spell only during combat after blockers are declared.
   * Flip a coin. If you win the flip, target creature gets +1/+1 until end of turn.
   * Draw a card. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (event != EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        result = flip_a_coin(player, card);
        if (result != 0)
        {
          instance = get_card_instance(player, card);
          pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1,
                         1);
        }
        draw_a_card(player);
      }
      kill_card(player, card, KILL_DESTROY);
    }
    result = generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
    return result;
  }
  return (unsigned int)(current_phase - 0x18U < 6);
}

/* ManalinkEh 2022: 0x2497110; original cards/invasion.c:5197. */
int card_crimson_acolyte(int player, int card, event_t event)
{
  /* Crimson Acolyte	|1|W
   * Creature - Human Cleric 1/1
   * Protection from red
   * |W: Target creature gains protection from red until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = valid_target(&td);
    if (result != 0)
    {
      result =
          get_sleighted_protection(instance->parent_controller, instance->parent_card, 0x4000);
      pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                             0, 0, result, 0);
    }
  }
  result = generic_activated_ability(player, card, event, 4, 0, 0, 0, 0, 0, 1, 0, &td,
                                     "TARGET_CREATURE");
  return result;
}

/* ManalinkEh 2022: 0x2497290; original cards/invasion.c:5222. */
int card_crosiss_attendant(int player, int card, event_t event)
{
  /* Crosis's Attendant	|5
   * Artifact Creature - Golem 3/3
   * |1, Sacrifice this creature: Add |U|B|R. */

  card_instance_t *instance;
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      ai_modifier = ai_modifier + -0x18;
      instance = get_card_instance(player, card);
      instance->state = instance->state | 0x10;
      charge_mana(player, COLOR_COLORLESS, 1);
      if (spell_fizzled == 1)
      {
        instance->state = instance->state & 0xffffffef;
      }
      else
      {
        produce_mana_multi(player, 0, 1, 1, 0, 1, 0);
        kill_card(player, card, KILL_SACRIFICE);
      }
    }
    return 0;
  }
  return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 1, 0, 0, 0, 0, 0, 0, NULL,
                                     NULL);
}

/* ManalinkEh 2022: 0x24973f0; original cards/invasion.c:5250. */
int card_crusading_knight(int player, int card, event_t event)
{
  /* Crusading Knight	|2|W|W
   * Creature - Human Knight 2/2
   * Protection from black
   * This creature gets +1/+1 for each Swamp your opponents control. */

  int result;
  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = get_hacked_subtype(player, card, 0x5009);
    result = count_subtype(1 - player, 0x47, result);
    event_result = event_result + result;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2497470; original cards/invasion.c:5269. */
int card_darigaazs_attendant(int player, int card, event_t event)
{
  /* Darigaaz's Attendant	|5
   * Artifact Creature - Golem 3/3
   * |1, Sacrifice this creature: Add |B|R|G. */

  card_instance_t *instance;
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      ai_modifier = ai_modifier + -0x18;
      instance = get_card_instance(player, card);
      instance->state = instance->state | 0x10;
      charge_mana(player, COLOR_COLORLESS, 1);
      if (spell_fizzled == 1)
      {
        instance->state = instance->state & 0xffffffef;
      }
      else
      {
        produce_mana_multi(player, 0, 1, 0, 1, 1, 0);
        kill_card(player, card, KILL_SACRIFICE);
      }
    }
    return 0;
  }
  return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 1, 0, 0, 0, 0, 0, 0, NULL,
                                     NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2488450; original cards/invasion.c:5297. */
static int effect_defiling_tears(int player, int card, event_t event)
{
  if (effect_follows_control_of_attachment(player, card, event))
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0)
  {
    instance = get_card_instance(player, card);
    int attached_player = instance->damage_target_player;
    int attached_card = instance->damage_target_card;
    if (attached_card == affected_card && attached_player == affected_card_controller)
    {
      if (event == EVENT_SET_COLOR)
        event_result = 2;
      else if (event == EVENT_POWER)
        ++event_result;
      else if (event == EVENT_TOUGHNESS)
        --event_result;
      else if (event == EVENT_ABILITIES)
        event_result |= 0x280;
    }
    if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ||
        event == EVENT_RESOLVE_ACTIVATION || event == EVENT_CLEANUP)
    {
      if (attached_player != -1)
      {
        if (event == EVENT_RESOLVE_ACTIVATION && can_regenerate(attached_player, attached_card))
          regenerate_target(attached_player, attached_card);
        return granted_generic_activated_ability(player, card, attached_player, attached_card,
                                                 event, GAA_REGENERATION, 0, 1, 0, 0, 0, 0, 0, NULL,
                                                 NULL);
      }
      return 0;
    }
  }
  if (current_phase == 0x22)
    kill_card(player, card, KILL_EXILE);
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x24975d0; original cards/invasion.c:5345. */
int card_defiling_tears(int player, int card, event_t event)
{
  /* Defiling Tears	|2|B
   * Instant
   * Until end of turn, target creature becomes black, gets +1/-1, and gains "|B: Regenerate this creature." */

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
      create_targetted_legacy_activate(player, card, effect_defiling_tears,
                                       instance->targets[0].player,
                                       instance->targets[0].card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}
#endif

/* ManalinkEh 2022: 0x2497730; original cards/invasion.c:5371. */
int card_divine_presence(int player, int card, event_t event)
{
  /* Divine Presence	|2|W
   * Enchantment
   * If a source would deal 4 or more damage to a permanent or player, that source deals 3 damage to that permanent or player instead. */

  card_instance_t *damage;
  card_instance_t *source_in_play;

  damage = damage_being_dealt(event);
  if ((damage != NULL) && (3 < damage->info_slot))
  {
    source_in_play = in_play(player, card);
    if (source_in_play != NULL)
    {
      if (!is_humiliated(player, card, AL_TRIG))
      {
        damage->info_slot = 3;
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x24977b0; original cards/invasion.c:5390. */
int card_dredge(int player, int card, event_t event)
{
  /* Dredge	|B
   * Instant
   * Sacrifice a creature or land.
   * Draw a card. */

  int result;
  test_definition_t this_test;
  if (event == EVENT_CAN_CAST)
  {
    default_test_definition(&this_test, 3);
    result = new_can_sacrifice_as_cost(player, card, &this_test);
    if (result != 0)
    {
      result = basic_spell(player, card, EVENT_CAN_CAST);
      return result;
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (result = controller_sacrifices_a_permanent(player, card, 3, 0), result == 0))
    {
      spell_fizzled = 1;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    draw_cards(player, 1);
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x24978c0; original cards/invasion.c:5419. */
int card_dromars_attendant(int player, int card, event_t event)
{
  /* Dromar's Attendant	|5
   * Artifact Creature - Golem 3/3
   * |1, Sacrifice this creature: Add |W|U|B. */

  card_instance_t *instance;
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      ai_modifier = ai_modifier + -0x18;
      instance = get_card_instance(player, card);
      instance->state = instance->state | 0x10;
      charge_mana(player, COLOR_COLORLESS, 1);
      if (spell_fizzled == 1)
      {
        instance->state = instance->state & 0xffffffef;
      }
      else
      {
        produce_mana_multi(player, 0, 1, 1, 0, 0, 1);
        kill_card(player, card, KILL_SACRIFICE);
      }
    }
    return 0;
  }
  return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 1, 0, 0, 0, 0, 0, 0, NULL,
                                     NULL);
}

/* ManalinkEh 2022: 0x2497a20; original cards/invasion.c:5447. */
int card_dueling_grounds(int player, int card, event_t event)
{
  /* Dueling Grounds	|1|G|W
   * Enchantment
   * No more than one creature can attack each combat.
   * No more than one creature can block each combat. */

  int result;

  result = is_humiliated(player, card, AL_STATIC);
  if (result == 0)
  {
    no_more_than_x_creatures_can_attack(player, card, event, 2, 1);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2497a90; original cards/invasion.c:5464. */
int card_duskwalker(int player, int card, event_t event)
{
  /* Duskwalker	|B
   * Creature - Human Minion 1/1
   * Kicker |3|B (You may pay an additional |3|B as you cast this spell.)
   * If this creature was kicked, it enters with two +1/+1 counters on it and with fear. (It can't be blocked except by artifact creatures and/or black creatures.) */

  int result;

  if (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller)
  {
    do_kicker(player, card, 3, 1, 0, 0, 0, 0);
  }
  result = check_special_flags(player, card, 0x80);
  if (result == 0 && event == EVENT_CAST_SPELL)
    return 0;
  if (result != 0)
  {
    enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);
  }
  if ((((event == EVENT_ABILITIES) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((result = is_humiliated(player, card, AL_STATIC),
        result == 0 && (result = check_special_flags(player, card, 0x80), result != 0))))
  {
    special_abilities(player, card, EVENT_ABILITIES, 1, player, card);
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2497bf0; original cards/invasion.c:5486. */
int card_essence_leak(int player, int card, event_t event)
{
  /* Essence Leak	|U
   * Enchantment - Aura
   * Enchant permanent
   * As long as enchanted permanent is red or green, it has "At the beginning of your upkeep, sacrifice this permanent unless you pay its mana cost." */

  card_instance_t *instance;
  int attached_card;
  unsigned int attached_colors;
  unsigned int affected_colors;
  int mana_value;
  int target_player;

  instance = in_play(player, card);
  if ((instance != NULL) &&
      (instance = get_card_instance(player, card), -1 < instance->damage_target_player))
  {

    instance = get_card_instance(player, card);
    target_player = (int)instance->damage_target_player;
    attached_card = instance->damage_target_card;
    attached_colors = get_color(target_player, attached_card);
    affected_colors = get_sleighted_color_test(target_player, attached_card, 0x18);
    if (((attached_colors & affected_colors) != 0) &&
        ((upkeep_trigger_ability(player, card, event, target_player),
          event == EVENT_UPKEEP_TRIGGER_ABILITY &&
              (instance = in_play(target_player, attached_card), instance != NULL))))
    {

      mana_value = get_cmc(target_player, attached_card);
      if (has_mana(target_player, 7, mana_value) == 0)
      {
        kill_card(target_player, attached_card, KILL_SACRIFICE);
      }
      else
      {
        if (do_dialog(target_player, target_player, attached_card, -1, -1, " Pay cmc\n Sac this", 0) == 0)
        {
          charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, target_player,
                                      COLOR_COLORLESS, get_cmc(target_player, attached_card));
          if (spell_fizzled != 1)
            return aura_pref_controller(player, card, event, 1 - player, NULL);
        }
      }
      kill_card(target_player, attached_card, KILL_SACRIFICE);
    }
  }
  return aura_pref_controller(player, card, event, 1 - player, NULL);
}
#endif

/* ManalinkEh 2022: 0x2497db0; original cards/invasion.c:5527. */
int card_exotic_curse(int player, int card, event_t event)
{
  /* Exotic Curse	|2|B
   * Enchantment - Aura
   * Enchant creature
   * Domain - Enchanted creature gets -1/-1 for each basic land type among lands you control. */

  int result;
  card_instance_t *instance;

  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (instance->damage_target_card == affected_card)) &&
        (instance->damage_target_player == affected_card_controller))
    {
      result = count_domain(player, card);
      event_result = event_result - result;
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

/* ManalinkEh 2022: 0x2497ed0; original cards/invasion.c:5547. */
int card_explosive_growth(int player, int card, event_t event)
{
  /* Explosive Growth	|G
   * Instant
   * Kicker |5 (You may pay an additional |5 as you cast this spell.)
   * Target creature gets +2/+2 until end of turn. If this spell was kicked, that creature gets +5/+5 until end of turn instead. */

  card_instance_t *instance;
  int power_or_result;
  int toughness;
  target_definition_t td;

  if (event == EVENT_CHECK_PUMP)
  {
    if (has_mana_multi(player, 5, 0, 0, 1, 0, 0) == 0)
    {
      toughness = 2;
      power_or_result = 2;
    }
    else
    {
      toughness = 5;
      power_or_result = 5;
    }
    return vanilla_instant_pump(player, card, EVENT_CHECK_PUMP, player, player, power_or_result,
                                toughness, 0, 0);
  }
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
    power_or_result =
        generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
  }
  else
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        default_target_definition(player, card, &td, 2);
        td.preferred_controller = player;
        get_card_instance(player, card);
        if ((card == affected_card) && (player == affected_card_controller))
        {
          generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
          if (spell_fizzled != 1)
          {
            do_kicker(player, card, 5, 0, 0, 0, 0, 0);
          }
        }
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {
        if (valid_target(&td) != 0)
        {
          if (check_special_flags(player, card, 0x80) == 0)
          {
            toughness = 2;
            power_or_result = 2;
          }
          else
          {
            toughness = 5;
            power_or_result = 5;
          }
          pump_ability_until_eot(player, card, instance->targets[0].player,
                                 instance->targets[0].card, power_or_result, toughness, 0, 0);
        }
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
    }
    else if (event == EVENT_CHANGE_TARGET)
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      get_card_instance(player, card);
    }
    power_or_result = 0;
  }
  return power_or_result;
}

/* ManalinkEh 2022: 0x2498210; original cards/invasion.c:5599. */
int card_faerie_squadron(int player, int card, event_t event)
{
  /* Faerie Squadron	|U
   * Creature - Faerie 1/1
   * Kicker |3|U (You may pay an additional |3|U as you cast this spell.)
   * If this creature was kicked, it enters with two +1/+1 counters on it and with flying. */

  int result;

  if (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller)
  {
    do_kicker(player, card, 3, 0, 1, 0, 0, 0);
  }
  result = check_special_flags(player, card, 0x80);
  if (result == 0 && event == EVENT_CAST_SPELL)
    return 0;
  if (result != 0)
  {
    enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);
  }
  if ((((event == EVENT_ABILITIES) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((result = is_humiliated(player, card, AL_STATIC),
        result == 0 && (result = check_special_flags(player, card, 0x80), result != 0))))
  {
    event_result = event_result | 0x20;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2498350; original cards/invasion.c:5627. */
int card_firebrand_ranger(int player, int card, event_t event)
{
  /* Firebrand Ranger	|1|R
   * Creature - Human Soldier Ranger 2/1
   * |G, |T: You may put a basic land card from your hand onto the battlefield. */

  int result;
  test_definition_t test;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    new_default_test_definition(&test, 1, "Select a basic land card.");
    test.subtype = 0x2000;
    test.zone = 0x100;
    result = check_battlefield_for_special_card(player, card, player, 0, &test);
    if (result != 0)
    {
      new_global_tutor(player, player, 4, 4, 0, 5, &test);
    }
  }
  result = generic_activated_ability(player, card, event, 1, 0, 0, 0, 1, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2498460; original cards/invasion.c:5647. */
int card_goblin_spy(int player, int card, event_t event)
{
  /* Goblin Spy	|R
   * Creature - Goblin Rogue 1/1
   * Play with the top card of your library revealed. */

  reveal_top_card(player, card, event);
  return 0;
}

/* ManalinkEh 2022: 0x2488930; original cards/invasion.c:5658. */
static void minus_2_minus_2_creature_as_long_as_color_is_most_common_color(int player, int card,
                                                                           event_t event,
                                                                           color_test_t col)
{
  int result;
  unsigned int affected_color;
  unsigned int common_colors;

  if (((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card))
  {
    if (player == affected_card_controller)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {

        affected_color = get_sleighted_color_test(player, card, col);
        common_colors = most_common_color_all_permanents(2);
        if ((common_colors & affected_color) != 0)
        {

          event_result = event_result + -2;
        }
      }
    }
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x2498490; original cards/invasion.c:5668. */
int card_goham_djinn(int player, int card, event_t event)
{
  /* Goham Djinn	|5|B
   * Creature - Djinn 5/5
   * |1|B: Regenerate this creature.
   * This creature gets -2/-2 as long as black is the most common color among all permanents or is tied for most common. */

  minus_2_minus_2_creature_as_long_as_color_is_most_common_color(player, card, event,
                                                                 COLOR_TEST_BLACK);
  return regeneration(player, card, event, 1, 1, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x2498500; original cards/invasion.c:5679. */
int card_halam_djinn(int player, int card, event_t event)
{
  /* Halam Djinn	|5|R
   * Creature - Djinn 6/5
   * Haste
   * This creature gets -2/-2 as long as red is the most common color among all permanents or is tied for most common. */

  minus_2_minus_2_creature_as_long_as_color_is_most_common_color(player, card, event,
                                                                 COLOR_TEST_RED);
  return 0;
}

/* ManalinkEh 2022: 0x2498520; original cards/invasion.c:5690. */
int card_hate_weaver(int player, int card, event_t event)
{
  /* Hate Weaver	|1|B
   * Creature - Zombie Wizard 2/1
   * |2: Target blue or red creature gets +1/+0 until end of turn. */

  const char *__format_1;
  card_instance_t *instance;
  int result;
  size_t __n_1;
  target_definition_t td;
  char msg[100];

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.required_color = get_sleighted_color_test(player, card, 0x14);
    td.preferred_controller = player;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 2, 0, 0, 0, 0, 0, 0,
                                         &td, NULL);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      __n_1 = 4;
      __format_1 =
          get_sleighted_color_text2(player, card, "Select target %s or %s creature.", 2, 4);
      snprintf(msg, 100, "%s", __format_1, __n_1);
      generic_activated_ability(player, card, EVENT_ACTIVATE, 6, 2, 0, 0, 0, 0, 0, 0, &td, msg);
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
    td.required_color = get_sleighted_color_test(player, card, 0x14);
  }
  td.preferred_controller = player;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = valid_target(&td);
    if (result != 0)
    {
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                             instance->targets[0].player, instance->targets[0].card, 1, 0, 0, 0);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2498770; original cards/invasion.c:5725. */
int card_hooded_kavu(int player, int card, event_t event)
{
  /* Hooded Kavu	|2|R
   * Creature - Kavu 2/2
   * |B: This creature gains fear until end of turn. (It can't be blocked except by artifact creatures and/or black creatures.) */

  int result;
  result = generic_shade(player, card, event, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1);
  return result;
}

/* ManalinkEh 2022: 0x24987f0; original cards/invasion.c:5734. */
int card_hypnotic_cloud(int player, int card, event_t event)
{
  /* Hypnotic Cloud	|1|B
   * Sorcery
   * Kicker |4 (You may pay an additional |4 as you cast this spell.)
   * Target player discards a card. If this spell was kicked, that player discards three cards instead. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    get_card_instance(player, card);
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_PLAYER", 1, NULL);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_PLAYER", 1, NULL);
        if (spell_fizzled != 1)
        {
          do_kicker(player, card, 4, 0, 0, 0, 0, 0);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        result = check_special_flags(player, card, 0x80);
        new_multidiscard(instance->targets[0].player,
                         ((result == 0) ? 1U : 3U), 0, player);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2498a80; original cards/invasion.c:5773. */
int card_kavu_aggressor(int player, int card, event_t event)
{
  /* Kavu Aggressor	|2|R
   * Creature - Kavu 3/2
   * Kicker |4 (You may pay an additional |4 as you cast this spell.)
   * This creature can't block.
   * If this creature was kicked, it enters with a +1/+1 counter on it. */

  int result;
  kicker(player, card, event, 4, 0, 0, 0, 0, 0);
  cannot_block(player, card, event);
  result = check_special_flags(player, card, 0x80);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1,
                                       (unsigned int)(result != 0));
  return 0;
}

/* ManalinkEh 2022: 0x2498b30; original cards/invasion.c:5787. */
int card_kavu_runner(int player, int card, event_t event)
{
  /* Kavu Runner	|3|R
   * Creature - Kavu 3/3
   * This creature has haste as long as no opponent controls a white or blue creature. */

  card_instance_t *instance;
  int result;
  test_definition_t this_test;

  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        default_test_definition(&this_test, 2);
        this_test.color = get_sleighted_color_test(player, card, 0x24);
        result = check_battlefield_for_special_card(player, card, 1 - player, 0, &this_test);
        if (result == 0)
        {
          haste(player, card, EVENT_ABILITIES);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2498c20; original cards/invasion.c:5805. */
int card_kavu_scout(int player, int card, event_t event)
{
  /* Kavu Scout	|2|R
   * Creature - Kavu Scout 0/2
   * Domain - This creature gets +1/+0 for each basic land type among lands you control. */

  card_instance_t *instance;
  int result;

  if (((event == EVENT_POWER) && (card == affected_card)) && (player == affected_card_controller))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = count_domain(player, card);
        event_result = event_result + result;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2498ca0; original cards/invasion.c:5817. */
int card_lightning_dart(int player, int card, event_t event)
{
  /* Lightning Dart	|1|R
   * Instant
   * ~ deals 1 damage to target creature. If that creature is white or blue, ~ deals 4 damage to it instead. */

  if (!IS_GS_EVENT(player, card, event))
    return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  // Target setup is an opaque call; the release checks the affected card again.
  if (event == EVENT_CAST_SPELL && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int value = get_base_value(instance->targets[0].player, instance->targets[0].card);
    ai_modifier += value;
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      card_instance_t *instance = get_card_instance(player, card);
      int target_player = instance->targets[0].player;
      int target_card = instance->targets[0].card;
      int amount = 1;
      if (is_what(target_player, target_card, TYPE_CREATURE))
      {
        int colors = get_color(target_player, target_card);
        if (colors & get_sleighted_color_test(target_player, target_card,
                                             COLOR_TEST_BLUE | COLOR_TEST_WHITE))
          amount = 4;
      }
      damage_target0(player, card, amount);
    }
    kill_card(player, card, KILL_DESTROY);
  }

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x2498ec0; original cards/invasion.c:5853. */
int card_llanowar_elite(int player, int card, event_t event)
{
  /* Llanowar Elite	|G
   * Creature - Elf 1/1
   * Kicker |8 (You may pay an additional |8 as you cast this spell.)
   * Trample
   * If this creature was kicked, it enters with five +1/+1 counters on it. */

  int result;
  int number;
  kicker(player, card, event, 8, 0, 0, 0, 0, 0);
  result = check_special_flags(player, card, 0x80);
  number = 0;
  if (result != 0)
  {
    number = 5;
  }
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, number);
  return 0;
}

/* ManalinkEh 2022: 0x2498f60; original cards/invasion.c:5866. */
int card_llanowar_vanguard(int player, int card, event_t event)
{
  /* Llanowar Vanguard	|2|G
   * Creature - Dryad 1/1
   * |T: This creature gets +0/+4 until end of turn. */

  int result;
  result = generic_shade_tap(player, card, event, 0, 0, 0, 0, 0, 0, 0, 4);
  return result;
}

/* ManalinkEh 2022: 0x2498fd0; original cards/invasion.c:5886. */
int card_manipulate_fate(int player, int card, event_t event)
{
  /* Manipulate Fate	|1|U
   * Sorcery
   * Search your library for three cards, exile them, then shuffle.
   * Draw a card. */

  if (event != EVENT_RESOLVE_SPELL)
    return basic_spell(player, card, event);
  test_definition_t test;
  if (ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0))
  {
    bool select_lands = count_subtype(player, 1, -1) > 4 && count_deck_by_type(player, 1) > 0;
    new_default_test_definition(&test, select_lands ? 1 : 0xff, "Select a card to exile.");
    ai_modifier += select_lands ? 0x40 : 0x20;
  }
  else
    new_default_test_definition(&test, 0xff, "Select a card to exile.");
  test.qty = 3;
  new_global_tutor(player, player, 1, 6, 0, 3, &test);
  draw_a_card(player);
  kill_card(player, card, KILL_DESTROY);
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x2499140; original cards/invasion.c:5918. */
int card_marauding_knight(int player, int card, event_t event)
{
  /* Marauding Knight	|2|B|B
   * Creature - Phyrexian Zombie Knight 2/2
   * Protection from white
   * This creature gets +1/+1 for each Plains your opponents control. */

  int result;
  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = get_hacked_subtype(player, card, 0x5007);
    result = count_subtype(1 - player, 0x47, result);
    event_result = event_result + result;
  }
  return 0;
}

/* ManalinkEh 2022: 0x24991c0; original cards/invasion.c:5931. */
int card_might_weaver(int player, int card, event_t event)
{
  /* Might Weaver	|1|G
   * Creature - Human Wizard 2/1
   * |2: Target red or white creature gains trample until end of turn. (It can deal excess combat damage to the player or planeswalker it's attacking.) */

  unsigned int first_color;
  unsigned int second_color;
  const char *prompt;
  card_instance_t *instance;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    first_color = get_sleighted_color_test(player, card, 0x10);
    second_color = get_sleighted_color_test(player, card, 0x20);
    td.required_color = second_color | first_color;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    first_color = get_sleighted_color_test(player, card, 0x10);
    second_color = get_sleighted_color_test(player, card, 0x20);
    td.required_color = second_color | first_color;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      if (valid_target(&td))
      {
        pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                               instance->targets[0].player, instance->targets[0].card, 0, 0, 0x80,
                               0);
      }
    }
  }
  prompt = get_sleighted_color_text2(
      player, card, "Select target %s or %s creature it gains trample until eot.", 4, 5);
  return generic_activated_ability(player, card, event, 6, 2, 0, 0, 0, 0, 0, 0, &td, prompt);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x24993a0; original cards/invasion.c:5956. */
int card_mourning(int player, int card, event_t event)
{
  /* Mourning	|1|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets -2/-0.
   * |B: Return this Aura to its owner's hand. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_CHECK_PUMP)
  {
    result = vanilla_instant_pump(player, card, EVENT_CHECK_PUMP, 2, 1 - player, -2, 0, 0, 0);
    return result;
  }
  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      attached_gets_pt(player, card, event, -2, 0);
      result = aura_pref_controller(player, card, event, 1 - player, NULL);
      return result;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      instance = get_card_instance(player, card);
      bounce_permanent(instance->parent_controller, instance->parent_card);
    }
  }
  return generic_activated_ability(player, card, event, 0, 0, 1, 0, 0, 0, 0, 0, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x2499510; original cards/invasion.c:5979. */
int card_nomadic_elf(int player, int card, event_t event)
{
  /* Nomadic Elf	|1|G
   * Creature - Elf Nomad 2/2
   * |1|G: Add one mana of any color. */

  int result;

  if (event == EVENT_COUNT_MANA)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = can_produce_mana(player, card), result != 0)) &&
        (result = has_mana_for_activated_ability(player, card, 1, 0, 0, 1, 0, 0), result != 0))
    {
      result = has_mana(player, 3, 1);
      declare_mana_available_hex(player, 0x36, result);
      declare_mana_available(player, 0, -1);
    }
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    result = can_produce_mana(player, card);
    if (result != 0)
    {
      result = has_mana_for_activated_ability(player, card, 1, 0, 0, 1, 0, 0);
      return result;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    result = choose_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1, "What kind of mana?",
                                            COLOR_TEST_COLORLESS);
    if (result == -1)
    {
      spell_fizzled = 1;
    }
    else
    {
      charge_mana_for_activated_ability(player, card, 1, 0, 0, 1, 0, 0);
      if (spell_fizzled != 1)
      {
        produce_mana(player, result, 1);
        tapped_for_mana_color = -2;
        return 0;
      }
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x2499730; original cards/invasion.c:6017. */
int card_obsidian_acolyte(int player, int card, event_t event)
{
  /* Obsidian Acolyte	|1|W
   * Creature - Human Cleric 1/1
   * Protection from black
   * |W: Target creature gains protection from black until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = valid_target(&td);
    if (result != 0)
    {
      result =
          get_sleighted_protection(instance->parent_controller, instance->parent_card, 0x800);
      pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                             0, 0, result, 0);
    }
  }
  result = generic_activated_ability(player, card, event, 4, 0, 0, 0, 0, 0, 1, 0, &td,
                                     "TARGET_CREATURE");
  return result;
}

/* ManalinkEh 2022: 0x24998b0; original cards/invasion.c:6042. */
int card_orims_touch(int player, int card, event_t event)
{
  /* Orim's Touch	|W
   * Instant
   * Kicker |1 (You may pay an additional |1 as you cast this spell.)
   * Prevent the next 2 damage that would be dealt to any target this turn. If this spell was kicked, prevent the next 4 damage that would be dealt to that permanent or player this turn instead. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    td.preferred_controller = player;
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    result = can_target(&td);
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
        do_kicker(player, card, 1, 0, 0, 0, 0, 0);
        result = pick_target(&td, "TARGET_ANY");
        if ((result != 0) && (((ai_is_speculating == 1 ||
                                ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
                               (instance->targets[0].player != player))))
        {
          ai_modifier = ai_modifier + -0x60;
        }
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
        result = check_special_flags(player, card, 0x80);
        if (result == 0)
        {
          result = 2;
        }
        else
        {
          result = 4;
        }
        prevent_the_next_n_damage(player, card, instance->targets[0].player,
                                  instance->targets[0].card, result, 0, 0, 0);
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

/* ManalinkEh 2022: 0x2499b30; original cards/invasion.c:6090. */
int card_pain_suffering(int player, int card, event_t event)
{
  /* Pain // Suffering	|B // |3|R
   * Sorcery // Sorcery
   * Pain:
   * Target player discards a card.
   *
   * Suffering:
   * Destroy target land. */

  int available_or_valid;
  card_instance_t *instance;
  unsigned int selected_modes;
  target_definition_t *td_00;
  const char *prompt;
  target_definition_t td_pl;
  target_definition_t td;

  td_00 = &td_pl;
  default_target_definition(player, card, td_00, 0);
  td_pl.zone = TARGET_ZONE_PLAYERS;
  default_target_definition(player, card, &td, 1);
  if (event == EVENT_MODIFY_COST)
  {
    available_or_valid = can_target(&td);
    return split_card_impl(player, card, EVENT_MODIFY_COST, 0x4b1c, can_target(td_00), 0, 0x4b39,
                           available_or_valid, 0, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    prompt = "TARGET_PLAYER";
  }
  else
  {
    if (event != EVENT_PLAY_SECOND_HALF)
    {
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
        else if (event == EVENT_RESOLVE_SPELL)
        {

          instance = get_card_instance(player, card);
          selected_modes = instance->info_slot;
          if ((selected_modes & 1) != 0)
          {
            available_or_valid = valid_target(td_00);
            if (available_or_valid != 0)
            {
              discard(instance->targets[0].player, 0, player);
            }
            selected_modes = instance->info_slot;
          }
          if (((selected_modes & 2) != 0) &&
              (available_or_valid = valid_target(&td), available_or_valid != 0))
          {
            kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
          }
          kill_card(player, card, KILL_DESTROY);
        }
        else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
        {
          return 0;
        }
      }
      available_or_valid = can_target(&td);
      return split_card_impl(player, card, event, 0x4b1c, can_target(td_00), 10, 0x4b39,
                             available_or_valid, 5, 0);
    }
    prompt = "TARGET_LAND";
    td_00 = &td;
  }
  return generic_spell(player, card, EVENT_CAST_SPELL, 1, td_00, prompt, 1, NULL);
}

/* ManalinkEh 2022: 0x2486940; original cards/invasion.c:6139. */
static void destroy_green_creature_when_blocked(intptr_t player, intptr_t card, int t_player, int t_card)
{
  unsigned int creature_color;
  unsigned int destroyed_color;

  creature_color = get_color(t_player, t_card);
  destroyed_color = get_sleighted_color_test(player, card, 8);
  if ((creature_color & destroyed_color) == 0)
  {
    return;
  }
  kill_card(t_player, t_card, KILL_BURY);
  return;
}

/* ManalinkEh 2022: 0x2499e10; original cards/invasion.c:6145. */
int card_phyrexian_reaper(int player, int card, event_t event)
{
  /* Phyrexian Reaper	|4|B
   * Creature - Phyrexian Zombie 3/3
   * Whenever this creature becomes blocked by a green creature, destroy that creature. It can't be regenerated. */

  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if (event == EVENT_CHANGE_TYPE)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = is_humiliated(player, card, AL_TRIG);
      if ((result == 0) && (current_turn == player))
      {
        instance->destroys_if_blocked = instance->destroys_if_blocked | 0xc;
      }
    }
  }
  else if (event == EVENT_DECLARE_BLOCKERS)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (((result == 0) && (current_turn == player)) && ((instance->state & 4) != 0))
    {
      for_each_creature_blocking_me(player, card, destroy_green_creature_when_blocked, player,
                                    card);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x24869b0; original cards/invasion.c:6165. */
static void destroy_white_creature_when_blocked(intptr_t player, intptr_t card, int t_player, int t_card)
{
  unsigned int creature_color;
  unsigned int destroyed_color;

  creature_color = get_color(t_player, t_card);
  destroyed_color = get_sleighted_color_test(player, card, 0x20);
  if ((creature_color & destroyed_color) == 0)
  {
    return;
  }
  kill_card(t_player, t_card, KILL_BURY);
  return;
}

/* ManalinkEh 2022: 0x2499ee0; original cards/invasion.c:6171. */
int card_phyrexian_slayer(int player, int card, event_t event)
{
  /* Phyrexian Slayer	|3|B
   * Creature - Phyrexian Minion 2/2
   * Flying
   * Whenever this creature becomes blocked by a white creature, destroy that creature. It can't be regenerated. */

  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if (event == EVENT_CHANGE_TYPE)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = is_humiliated(player, card, AL_TRIG);
      if ((result == 0) && (current_turn == player))
      {
        instance->destroys_if_blocked = instance->destroys_if_blocked | 0xc;
      }
    }
  }
  else if (event == EVENT_DECLARE_BLOCKERS)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (((result == 0) && (current_turn == player)) && ((instance->state & 4) != 0))
    {
      for_each_creature_blocking_me(player, card, destroy_white_creature_when_blocked, player,
                                    card);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2499fb0; original cards/invasion.c:6192. */
int card_pincer_spider(int player, int card, event_t event)
{
  /* Pincer Spider	|2|G
   * Creature - Spider 2/3
   * Kicker |3 (You may pay an additional |3 as you cast this spell.)
   * Reach (This creature can block creatures with flying.)
   * If this creature was kicked, it enters with a +1/+1 counter on it. */

  int result;
  kicker(player, card, event, 3, 0, 0, 0, 0, 0);
  result = check_special_flags(player, card, 0x80);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1,
                                       (unsigned int)(result != 0));
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x249a050; original cards/invasion.c:6205. */
int card_pledge_of_loyalty(int player, int card, event_t event)
{
  /* Pledge of Loyalty	|1|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has protection from each color among permanents you control. This effect doesn't remove this Aura. */

  if (this_effect_doesnt_remove_this(player, card, event))
    return 0;
  if (event == EVENT_ABILITIES)
  {
    card_instance_t *inst = in_play(player, card);
    if (inst && inst->damage_target_card >= 0)
    {
      int humiliated = is_humiliated(player, card, AL_STATIC);
      int first_player = current_turn;
      if (!humiliated && inst->damage_target_card == affected_card &&
          inst->damage_target_player == affected_card_controller)
      {
        unsigned int keyword = 0;
        for (int pass = 0; pass != 2; ++pass)
        {
          int p = pass == 0 ? first_player : 1 - current_turn;
          for (int c = active_cards_count[p] - 1; c >= 0; --c)
          {
            if (player == p && is_what(player, c, 0x47) &&
                !is_what(player, c, TYPE_LAND) && in_play(player, c))
            {
              unsigned int clr = get_color(player, c);
              for (int i = 1; i != 6; ++i)
                if (clr & (1U << i))
                  keyword |= 1U << (i + 10);
            }
          }
        }
        event_result |= keyword;
      }
    }
  }
  return aura(player, card, event, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x249a2a0; original cards/invasion.c:6243. */
int card_pouncing_kavu(int player, int card, event_t event)
{
  /* Pouncing Kavu	|1|R
   * Creature - Kavu 1/1
   * Kicker |2|R (You may pay an additional |2|R as you cast this spell.)
   * First strike
   * If this creature was kicked, it enters with two +1/+1 counters on it and with haste. */

  int result;

  if (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller)
  {
    do_kicker(player, card, 2, 0, 0, 0, 1, 0);
  }
  result = check_special_flags(player, card, 0x80);
  if (result == 0 && event == EVENT_CAST_SPELL)
    return 0;
  if (result != 0)
  {
    enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);
  }
  if ((((event == EVENT_ABILITIES) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((result = is_humiliated(player, card, AL_STATIC),
        result == 0 && (result = check_special_flags(player, card, 0x80), result != 0))))
  {
    special_abilities(player, card, EVENT_ABILITIES, 4, player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x249a400; original cards/invasion.c:6266. */
int card_power_armor(int player, int card, event_t event)
{
  /* Power Armor	|4
   * Artifact
   * Domain - |3, |T: Target creature gets +1/+1 until end of turn for each basic land type among lands you control. */

  card_instance_t *instance;
  int result;
  int power;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
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
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      result = count_domain(player, card);
      power = count_domain(player, card);
      pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, power,
                     result);
    }
  }
  result = generic_activated_ability(player, card, event, 5, 3, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
  return result;
}

/* ManalinkEh 2022: 0x249a570; original cards/invasion.c:6291. */
int card_prison_barricade(int player, int card, event_t event)
{
  /* Prison Barricade	|1|W
   * Creature - Wall 1/3
   * Defender (This creature can't attack.)
   * Kicker |1|W (You may pay an additional |1|W as you cast this spell.)
   * If this creature was kicked, it enters with a +1/+1 counter on it and with "This creature can attack as though it didn't have defender." */

  int result;

  if (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller)
  {
    do_kicker(player, card, 1, 0, 0, 0, 0, 1);
  }
  result = check_special_flags(player, card, 0x80);
  if (result == 0 && event == EVENT_CAST_SPELL)
    return 0;
  if (result != 0)
  {
    enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 1);
  }
  if ((((event == EVENT_ABILITIES) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((result = is_humiliated(player, card, AL_STATIC),
        result == 0 && (result = check_special_flags(player, card, 0x80), result != 0))))
  {
    add_status(affected_card_controller, affected_card, 0x800);
  }
  return 0;
}

/* ManalinkEh 2022: 0x249a6c0; original cards/invasion.c:6332. */
int card_pure_reflection(int player, int card, event_t event)
{
  /* Pure Reflection	|2|W
   * Enchantment
   * Whenever a player casts a creature spell, destroy all Reflections. Then that player creates an X/X white Reflection creature token, where X is the mana value of that spell. */

  int result;
  token_generation_t token;
  test_definition_t this_test;

  sp_ability_trigger(2, event, 2, TYPE_CREATURE, NULL, player, card);
  if (event == EVENT_SPA)
  {
    default_test_definition(&this_test, 0x47);
    this_test.subtype = 0x9c;
    new_manipulate_all(player, card, 2, &this_test, ACT_KILL_DESTROY);
    default_token_definition(player, card, CARD_ID_REFLECTION, &token);
    token.t_player = trigger_cause_controller;
    token.color_forced = ' ';
    token.pow = get_cmc(trigger_cause_controller, trigger_cause);
    token.tou = token.pow;
    generate_token(&token);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x249a7d0; original cards/invasion.c:6357. */
int card_quirion_sentinel(int player, int card, event_t event)
{
  /* Quirion Sentinel	|1|G
   * Creature - Elf Druid 2/1
   * When this creature enters, add one mana of any color. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if ((event == EVENT_ETB) &&
      (is_humiliated(player, card, AL_TRIG) == 0))
  {
    do
    {
      spell_fizzled = 0;
      produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1);
    } while (spell_fizzled == 1);
  }
  return 0;
}

/* ManalinkEh 2022: 0x249a860; original cards/invasion.c:6375. */
int card_rage_weaver(int player, int card, event_t event)
{
  /* Rage Weaver	|1|R
   * Creature - Human Wizard 2/1
   * |2: Target black or green creature gains haste until end of turn. (It can attack and |T this turn.) */

  unsigned int first_color;
  unsigned int second_color;
  const char *prompt;
  card_instance_t *instance;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    first_color = get_sleighted_color_test(player, card, 2);
    second_color = get_sleighted_color_test(player, card, 8);
    td.required_color = second_color | first_color;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    first_color = get_sleighted_color_test(player, card, 2);
    second_color = get_sleighted_color_test(player, card, 8);
    td.required_color = second_color | first_color;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      if (valid_target(&td))
      {
        pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                               instance->targets[0].player, instance->targets[0].card, 0, 0, 0, 4);
      }
    }
  }
  prompt = get_sleighted_color_text2(
      player, card, "Select target %s or %s creature it gains haste until eot.", 1, 3);
  return generic_activated_ability(player, card, event, 6, 2, 0, 0, 0, 0, 0, 0, &td, prompt);
}

/* ManalinkEh 2022: 0x249aa40; original cards/invasion.c:6400. */
int card_rainbow_crow(int player, int card, event_t event)
{
  /* Rainbow Crow	|3|U
   * Creature - Bird 2/2
   * Flying
   * |1: This creature becomes the color of your choice until end of turn. */

  int result;
  card_instance_t *instance;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = get_deck_color(player, 1 - player);
    result = choose_a_color(player, result);
    change_color(instance->parent_controller, instance->parent_card, instance->parent_controller,
                 instance->parent_card, 1 << ((unsigned char)result & 0x1f),
                 CHANGE_COLOR_NO_SLEIGHT | CHANGE_COLOR_END_AT_EOT | CHANGE_COLOR_SET);
  }
  result = generic_activated_ability(player, card, event, 0, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x249ab30; original cards/invasion.c:6418. */
int card_rampant_elephant(int player, int card, event_t event)
{
  /* Rampant Elephant	|3|W
   * Creature - Elephant 2/2
   * |G: Target creature blocks this creature this turn if able. */

  int result;

  result = card_tangle_angler(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x249ab40; original cards/invasion.c:6427. */
int card_recover(int player, int card, event_t event)
{
  /* Recover	|2|B
   * Sorcery
   * Return target creature card from your graveyard to your hand.
   * Draw a card. */

  int *graveyard;
  int result;
  test_definition_t this_test;
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        new_default_test_definition(&this_test, 2, "Select a creature card.");
        if ((card != affected_card) || (player != affected_card_controller))
          return generic_spell(player, card, event, 8, NULL, NULL, 1, &this_test);
        generic_spell(player, card, EVENT_CAST_SPELL, 8, NULL, NULL, 1, &this_test);
        if (spell_fizzled != 1)
        {
          graveyard = graveyard_ptr_mutable[player];
          result = validate_target_from_grave_source(player, card, player, 0);
          ai_modifier = ai_modifier + cards_ptr[graveyard[result]]->ai_base_value;
        }
      }
      return 0;
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      new_default_test_definition(&this_test, 2, "Select a creature card.");
      if (event == EVENT_RESOLVE_SPELL)
      {
        result = validate_target_from_grave_source(player, card, player, 0);
        if (result != -1)
        {
          from_grave_to_hand(player, result, 1);
          draw_a_card(player);
        }
        kill_card(player, card, KILL_DESTROY);
      }
      return generic_spell(player, card, event, 8, NULL, NULL, 1, &this_test);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  new_default_test_definition(&this_test, 2, "Select a creature card.");
  return generic_spell(player, card, event, 8, NULL, NULL, 1, &this_test);
}

/* ManalinkEh 2022: 0x249ad50; original cards/invasion.c:6462. */
int card_restrain(int player, int card, event_t event)
{
  /* Restrain	|2|W
   * Instant
   * Prevent all combat damage that would be dealt by target attacking creature this turn.
   * Draw a card. */

  card_instance_t *instance;
  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.required_state = TARGET_STATE_ATTACKING;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      instance = get_card_instance(player, card);
      negate_combat_damage_this_turn(player, card, instance->targets[0].player,
                                     instance->targets[0].card, 0);
      draw_a_card(player);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_ATTACKING_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x249aeb0; original cards/invasion.c:6490. */
int card_rewards_of_diversity(int player, int card, event_t event)
{
  /* Rewards of Diversity	|2|W
   * Enchantment
   * Whenever an opponent casts a multicolored spell, you gain 4 life. */

  int result;
  test_definition_t this_test;

  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&this_test, 0xff);
    this_test.color_flag = 2;
    sp_ability_trigger(1 - player, event, 2, TYPE_NONE, &this_test, player, card);
  }
  if (event == EVENT_SPA)
  {
    gain_life(player, 4);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x249af80; original cards/invasion.c:6509. */
int card_riptide_crab(int player, int card, event_t event)
{
  /* Riptide Crab	|1|W|U
   * Creature - Crab 1/3
   * Vigilance
   * When this creature dies, draw a card. */

  int result;
  vigilance(player, card, event);
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  draw_cards(player, 1);
  return 0;
}

/* ManalinkEh 2022: 0x249aff0; original cards/invasion.c:6524. */
int card_riths_attendant(int player, int card, event_t event)
{
  /* Rith's Attendant	|5
   * Artifact Creature - Golem 3/3
   * |1, Sacrifice this creature: Add |R|G|W. */

  card_instance_t *instance;
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      ai_modifier = ai_modifier + -0x18;
      instance = get_card_instance(player, card);
      instance->state = instance->state | 0x10;
      charge_mana(player, COLOR_COLORLESS, 1);
      if (spell_fizzled == 1)
      {
        instance->state = instance->state & 0xffffffef;
      }
      else
      {
        produce_mana_multi(player, 0, 0, 0, 1, 1, 1);
        kill_card(player, card, KILL_SACRIFICE);
      }
    }
    return 0;
  }
  return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 1, 0, 0, 0, 0, 0, 0, NULL,
                                     NULL);
}

/* ManalinkEh 2022: 0x249b150; original cards/invasion.c:6552. */
int card_rogue_kavu(int player, int card, event_t event)
{
  /* Rogue Kavu	|1|R
   * Creature - Kavu 1/1
   * Whenever this creature attacks alone, it gets +2/+0 until end of turn. */

  dat_ability(player, card, event, DAT_ATTACKS_ALONE | DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }
  pump_until_eot_merge_previous(player, card, player, card, 2, 0);
  return 0;
}

/* ManalinkEh 2022: 0x249b1d0; original cards/invasion.c:6565. */
int card_rooting_kavu(int player, int card, event_t event)
{
  /* Rooting Kavu	|2|G|G
   * Creature - Kavu 4/3
   * When this creature dies, you may exile it. If you do, shuffle all creature cards from your graveyard into your library. */

  int mode = (duh_mode(player) != 0) + RESOLVE_TRIGGER_OPTIONAL;
  if (this_dies_trigger(player, card, event, mode) &&
      !is_humiliated(player, card, AL_TRIG) && exile_from_owners_graveyard(player, card))
  {
    int graveyard_count = count_graveyard(player);
    int *graveyard = graveyard_ptr_mutable[player];
    for (int i = graveyard_count - 1; i >= 0; --i)
      if (cards_data[graveyard[i]].type & TYPE_CREATURE)
        from_graveyard_to_deck(player, i, 2);
    shuffle(player);
  }
  return 0;
}

/* ManalinkEh 2022: 0x249b2a0; original cards/invasion.c:6586. */
int card_ruby_leech(int player, int card, event_t event)
{
  /* Ruby Leech	|1|R
   * Creature - Leech 2/2
   * First strike
   * Red spells you cast cost |R more to cast. */

  unsigned int affected_color;
  unsigned int spell_color;

  if ((event == EVENT_MODIFY_COST_GLOBAL) && (affected_card_controller == player))
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {

      affected_color = get_sleighted_color_test(player, card, 0x10);
      spell_color = get_color(affected_card_controller, affected_card);
      if ((spell_color & affected_color) != 0)
      {
        COST_RED = COST_RED + '\x01';
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x249b330; original cards/invasion.c:6603. */
int card_ruham_djinn(int player, int card, event_t event)
{
  /* Ruham Djinn	|5|W
   * Creature - Djinn 5/5
   * First strike
   * This creature gets -2/-2 as long as white is the most common color among all permanents or is tied for most common. */

  minus_2_minus_2_creature_as_long_as_color_is_most_common_color(player, card, event,
                                                                 COLOR_TEST_WHITE);
  return 0;
}

/* ManalinkEh 2022: 0x249b350; original cards/invasion.c:6614. */
int card_samite_archer(int player, int card, event_t event)
{
  /* Samite Archer	|1|W|U
   * Creature - Human Cleric Archer 1/1
   * |T: Prevent the next 1 damage that would be dealt to any target this turn.
   * |T: This creature deals 1 damage to any target. */

  int result;

  if (((event != EVENT_ACTIVATE) && (1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION))) &&
      (event != EVENT_CLEANUP))
  {
    return 0;
  }
  if (((unsigned char)land_can_be_played & 4) != 0)
  {
    result = card_samite_healer(player, card, event);
    return result;
  }
  return card_prodigal_sorcerer(player, card, event);
}

/* ManalinkEh 2022: 0x2488260; original cards/invasion.c:6635. */
static int source_damage_prevented_if_black_or_red_damage_source_controller(int player, int card,
                                                                            event_t event)
{
  int damage_iid;
  card_instance_t *damage;
  card_instance_t *instance;
  int planeswalker_or_amount;
  unsigned int source_colors;
  unsigned int red_color;
  unsigned int black_color;

  damage = damage_being_dealt(event);
  instance = get_card_instance(player, card);
  if ((((damage != NULL) && ((int)damage->damage_source_player == instance->targets[0].player)) &&
       (damage->damage_source_card == instance->targets[0].card)) &&
      ((damage->damage_target_player == player && (damage->damage_target_card == -1))))
  {
    planeswalker_or_amount = damage_is_to_planeswalker(damage);
    damage_iid = damage_card;
    if ((planeswalker_or_amount == 0) &&
        ((planeswalker_or_amount = damage->info_slot,
          0 < planeswalker_or_amount &&
              (damage->info_slot = 0, damage->internal_card_id == damage_iid))))
    {
      source_colors = get_color((int)damage->damage_source_player, damage->damage_source_card);
      red_color = get_sleighted_color_test(player, card, 0x10);
      black_color = get_sleighted_color_test(player, card, 2);
      if ((source_colors & (black_color | red_color)) != 0)
      {
        gain_life(player, planeswalker_or_amount);
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x249b3a0; original cards/invasion.c:6662. */
int card_samite_ministration(int player, int card, event_t event)
{
  /* Samite Ministration	|1|W
   * Instant
   * Prevent all damage that would be dealt to you this turn by a source of your choice. Whenever damage from a black or red source is prevented this way this turn, you gain that much life. */

  int target_card;
  card_instance_t *instance;
  int result;
  int legacy_card;
  target_definition_t td;
  target_definition_t td_ai;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0xff);
    td.preferred_controller = 1 - player;
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.illegal_abilities = 0;
    default_target_definition(player, card, &td_ai, 0);
    td_ai.extra = damage_card;
    get_card_instance(player, card);
    result = counterspell(player, card, EVENT_CAN_CAST, NULL, 0);
    if (result != 0)
    {
      return result;
    }
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    if (result != 0)
    {
      return (((land_can_be_played & 4U) == 0) ? 1U : 99U);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0xff);
      td.preferred_controller = 1 - player;
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td.illegal_abilities = 0;
      default_target_definition(player, card, &td_ai, 0);
      td_ai.extra = damage_card;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->info_slot = 0;
        result = counterspell(player, card, EVENT_CAN_CAST, NULL, 0);
        if (result == 0)
        {
          generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                             "Select a source, prevent all damage it will deal this turn.", 1,
                             NULL);
        }
        else
        {
          counterspell(player, card, EVENT_CAST_SPELL, NULL, 0);
          ((unsigned char *)&instance->info_slot)[0] = 1;
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0xff);
    td.preferred_controller = 1 - player;
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.illegal_abilities = 0;
    default_target_definition(player, card, &td_ai, 0);
    td_ai.extra = damage_card;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (instance->info_slot == 1)
      {
        result = counterspell_validate(player, card, NULL, 0);
      }
      else
      {
        result = valid_target(&td);
      }
      if (result != 0)
      {
        result = instance->targets[0].player;
        target_card = instance->targets[0].card;
        if (result != -1)
        {
          if (instance->info_slot == 1)
          {
            legacy_card = create_legacy_effect(
                player, card, source_damage_prevented_if_black_or_red_damage_source_controller);
            instance = get_card_instance(player, legacy_card);
            instance->targets[0].player = result;
            instance->targets[0].card = target_card;
          }
          else
          {
            create_targetted_legacy_effect(
                player, card, source_damage_prevented_if_black_or_red_damage_source_controller,
                result, target_card);
          }
        }
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 0xff);
    td.preferred_controller = 1 - player;
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.illegal_abilities = 0;
    default_target_definition(player, card, &td_ai, 0);
    td_ai.extra = damage_card;
    get_card_instance(player, card);
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x249b800; original cards/invasion.c:6738. */
int card_sapphire_leech(int player, int card, event_t event)
{
  /* Sapphire Leech	|1|U
   * Creature - Leech 2/2
   * Flying
   * Blue spells you cast cost |U more to cast. */

  unsigned int affected_color;
  unsigned int spell_color;

  if ((event == EVENT_MODIFY_COST_GLOBAL) && (affected_card_controller == player))
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {

      affected_color = get_sleighted_color_test(player, card, 4);
      spell_color = get_color(affected_card_controller, affected_card);
      if ((spell_color & affected_color) != 0)
      {
        COST_BLUE = COST_BLUE + '\x01';
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x249b890; original cards/invasion.c:6755. */
int card_savage_offensive(int player, int card, event_t event)
{
  /* Savage Offensive	|1|R
   * Sorcery
   * Kicker |G (You may pay an additional |G as you cast this spell.)
   * Creatures you control gain first strike until end of turn. If this spell was kicked, they get +1/+1 until end of turn. */

  int result;
  if (event == EVENT_CAN_CAST)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      do_kicker(player, card, 0, 0, 0, 1, 0, 0);
      return 0;
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET) ||
            (event == EVENT_CHANGE_TARGET)) &&
           (event == EVENT_RESOLVE_SPELL))
  {
    result = check_special_flags(player, card, 0x80);
    if (result == 0)
    {
      pump_creatures_until_eot(player, card, player, 1, 0, 0, 0x100, 0, NULL);
    }
    else
    {
      pump_creatures_until_eot(player, card, player, 2, 1, 1, 0x100, 0, NULL);
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x249b9f0; original cards/invasion.c:6787. */
int card_scarred_puma(int player, int card, event_t event)
{
  /* Scarred Puma	|R
   * Creature - Cat 2/1
   * This creature can't attack unless a black or green creature also attacks. */

  int result;
  target_definition_t td;

  if (((event == EVENT_ATTACK_LEGALITY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = player;
      td.preferred_controller = player;
      td.illegal_abilities = 0;
      td.required_state = TARGET_STATE_ATTACKING;
      td.required_color = get_sleighted_color_test(player, card, 10);
      result = can_target(&td);
      if (result == 0)
      {
        event_result = 1;
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x249bab0; original cards/invasion.c:6808. */
int card_scavenged_weaponry(int player, int card, event_t event)
{
  /* Scavenged Weaponry	|2|B
   * Enchantment - Aura
   * Enchant creature
   * When this Aura enters, draw a card.
   * Enchanted creature gets +1/+1. */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_cards(player, 1);
  }
  attached_gets_pt(player, card, event, 1, 1);
  result = aura_pref_controller(player, card, event, player, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x249bb40; original cards/invasion.c:6826. */
int card_scorching_lava(int player, int card, event_t event)
{
  /* Scorching Lava	|1|R
   * Instant
   * Kicker |R (You may pay an additional |R as you cast this spell.)
   * ~ deals 2 damage to any target. If this spell was kicked, that creature can't be regenerated this turn and if it would die this turn, exile it instead. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      if ((card == affected_card) && (player == affected_card_controller))
      {
        generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_ANY", 1, NULL);
        if (spell_fizzled != 1)
        {
          do_kicker(player, card, 0, 0, 0, 0, 1, 0);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        result = check_special_flags(player, card, 0x80);
        if (result != 0)
        {
          instance = get_card_instance(player, card);
          result = instance->targets[0].card;
          if (result != -1)
          {
            result = is_what(instance->targets[0].player, result, 2);
            if (result != 0)
            {
              cannot_regenerate_until_eot(player, card, instance->targets[0].player,
                                          instance->targets[0].card);
              exile_if_would_be_put_into_graveyard(player, card, instance->targets[0].player,
                                                   instance->targets[0].card, 1);
            }
          }
        }
        damage_target0(player, card, 2);
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

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x249bde0; original cards/invasion.c:6869. */
int card_scouting_trek(int player, int card, event_t event)
{
  /* Scouting Trek	|1|G
   * Sorcery
   * Search your library for any number of basic land cards, reveal those cards, then shuffle and put them on top. */

  if (event == EVENT_RESOLVE_SPELL)
  {
    test_definition_t this_test;
    new_default_test_definition(&this_test, TYPE_LAND, "Select a basic land card.");
    this_test.subtype = 0x2000;
    this_test.no_shuffle = 1;
    int deck_count = count_deck(player);
    int tutored[deck_count];
    int count = 0;
    for (;;)
    {
      int selected = new_select_a_card(player, player, 1, 0, 1, -1, &this_test);
      if (selected == -1)
        break;
      tutored[count] = deck_ptr[player][selected];
      ++count;
      remove_card_from_deck(player, selected);
    }
    shuffle(player);
    if (count > 0)
    {
      for (int i = 0; i != count + 1; ++i)
        raw_put_iid_on_top_of_deck(player, tutored[i]);
    }
    if (count > 1)
      rearrange_top_x(player, player, count);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}
#endif

/* ManalinkEh 2022: 0x249bf40; original cards/invasion.c:6911. */
int card_searing_rays(int player, int card, event_t event)
{
  /* Searing Rays	|2|R
   * Sorcery
   * Choose a color. ~ deals damage to each player equal to the number of creatures of that color that player controls. */

  int result;
  unsigned int pass;
  unsigned int t_player;
  test_definition_t this_test;

  if (event == EVENT_RESOLVE_SPELL)
  {

    result = get_deck_color(player, 1 - player);
    result = choose_a_color(player, result);
    default_test_definition(&this_test, 2);
    this_test.color = 1 << ((unsigned char)result & 0x1f);

    pass = 0;
    do
    {

      t_player = current_turn ^ pass;
      result = check_battlefield_for_special_card(player, card, t_player, 4, &this_test);
      if (result != 0)
      {
        damage_player(t_player, result, player, card);
      }
      pass = pass + 1;
    } while (pass != 2);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x249c030; original cards/invasion.c:6937. */
int card_seers_vision(int player, int card, event_t event)
{
  /* Seer's Vision	|2|U|B
   * Enchantment
   * Your opponents play with their hands revealed.
   * Sacrifice this enchantment: Look at target player's hand and choose a card from it. That player discards that card. Activate only as a sorcery. */

  card_instance_t *instance;
  int result;
  ec_definition_t this_definition;
  target_definition_t td;
  test_definition_t this_test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      player_bits[(1 - player)] =
          player_bits[(1 - player)] | 0x200;
    }
  }
  else if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
            (event == EVENT_RESOLVE_ACTIVATION)) ||
           (event == EVENT_CLEANUP))
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        default_test_definition(&this_test, 0xff);
        default_ec_definition(instance->targets[0].player, player, &this_definition);
        new_effect_coercion(&this_definition, &this_test);
      }
    }
    result = generic_activated_ability(player, card, event, 0x1c, 0, 0, 0, 0, 0, 0, 0, &td,
                                       "TARGET_PLAYER");
    return result;
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x249c1e0; original cards/invasion.c:6971. */
int card_serpentine_kavu(int player, int card, event_t event)
{
  /* Serpentine Kavu	|4|G
   * Creature - Kavu 4/4
   * |R: This creature gains haste until end of turn. */

  int result;
  result = generic_shade(player, card, event, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 4);
  return result;
}

/* ManalinkEh 2022: 0x249c260; original cards/invasion.c:6980. */
int card_shivan_emissary(int player, int card, event_t event)
{
  /* Shivan Emissary	|2|R
   * Creature - Human Wizard 1/1
   * Kicker |1|B (You may pay an additional |1|B as you cast this spell.)
   * When this creature enters, if it was kicked, destroy target nonblack creature. It can't be regenerated. */

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 2);
    td.illegal_color = get_sleighted_color_test(player, card, 2);
    td.preferred_controller = 1 - player;
    if ((unsigned int)player < 2 || can_target(&td))
      return kicker(player, card, EVENT_CAST_SPELL, 1, 1, 0, 0, 0, 0);
  }
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB && check_special_flags(player, card, 0x80))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 2);
    td.illegal_color = get_sleighted_color_test(player, card, 2);
    td.preferred_controller = 1 - player;
    td.allow_cancel = 0;
    card_instance_t *instance = get_card_instance(player, card);
    if (can_target(&td))
    {
      const char *target_prompt = get_sleighted_color_text(
          player, card, "Select target non-%s creature.", 1);
      char msg[100];
      scnprintf(msg, sizeof(msg), "%s", target_prompt);
      if (new_pick_target(&td, msg, 0, 0x81))
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
    }
    instance->number_of_targets = 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x249c4a0; original cards/invasion.c:7021. */
int card_shoreline_raider(int player, int card, event_t event)
{
  /* Shoreline Raider	|2|U
   * Creature - Merfolk 2/2
   * Protection from Kavu */

  protection_from_subtype(player, card, event, SUBTYPE_KAVU);
  return 0;
}

/* ManalinkEh 2022: 0x249c4d0; original cards/invasion.c:7031. */
int card_skittish_kavu(int player, int card, event_t event)
{
  /* Skittish Kavu	|1|R
   * Creature - Kavu 1/1
   * This creature gets +1/+1 as long as no opponent controls a white or blue creature. */

  int result;
  test_definition_t this_test;

  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      default_test_definition(&this_test, 2);
      this_test.color = get_sleighted_color_test(player, card, 0x24);
      result = check_battlefield_for_special_card(player, card, 1 - player, 0, &this_test);
      if (result == 0)
      {
        event_result = event_result + 1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x249c5a0; original cards/invasion.c:7048. */
int card_sky_weaver(int player, int card, event_t event)
{
  /* Sky Weaver	|1|U
   * Creature - Metathran Wizard 2/1
   * |2: Target white or black creature gains flying until end of turn. (It can't be blocked except by creatures with flying or reach.) */

  unsigned int first_color;
  unsigned int second_color;
  const char *prompt;
  card_instance_t *instance;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    first_color = get_sleighted_color_test(player, card, 0x20);
    second_color = get_sleighted_color_test(player, card, 2);
    td.required_color = second_color | first_color;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    first_color = get_sleighted_color_test(player, card, 0x20);
    second_color = get_sleighted_color_test(player, card, 2);
    td.required_color = second_color | first_color;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      if (valid_target(&td))
      {
        pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                               instance->targets[0].player, instance->targets[0].card, 0, 0, 0x20,
                               0);
      }
    }
  }
  prompt = get_sleighted_color_text2(
      player, card, "Select target %s or %s creature it gains haste until eot.", 5, 1);
  return generic_activated_ability(player, card, event, 6, 2, 0, 0, 0, 0, 0, 0, &td, prompt);
}

/* ManalinkEh 2022: 0x2488690; original cards/invasion.c:7073. */
static int target_land_becomes_swamp_until_eot_effect(int player, int card, event_t event)
{
  int basic_land_type_to_become;
  card_instance_t *instance;
  if (event == EVENT_CHANGE_TYPE)
  {
    instance = get_card_instance(player, card);
    if ((instance->damage_target_card == affected_card) &&
        (instance->damage_target_player == affected_card_controller))
    {
      basic_land_type_to_become = get_hacked_subtype(player, card, 0x5009);
      event_result = land_into_new_basic_land_type(event_result, basic_land_type_to_become);
    }
  }
  else if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(player, card);
    if (-1 < instance->targets[0].player)
    {
      kill_card(player, card, KILL_EXILE);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x249c780; original cards/invasion.c:7091. */
int card_slimy_kavu(int player, int card, event_t event)
{
  /* Slimy Kavu	|2|R
   * Creature - Kavu 2/2
   * |T: Target land becomes a Swamp until end of turn. */

  int32_t source_card;
  card_instance_t *instance;
  const char *prompt;
  int valid;
  card_instance_t *legacy;
  target_definition_t td;

  if (((event != EVENT_ACTIVATE) && (1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION))) &&
      (event != EVENT_CLEANUP))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 1);
  if (player == 1 && (ai_is_speculating == 1 || ((unsigned char)trace_mode & 2) == 0))
  {
    td.required_subtype = get_hacked_subtype(1, card, 0x5009);
    td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
  }
  instance = get_card_instance(player, card);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (valid = valid_target(&td), valid != 0))
  {
    legacy = get_card_instance(player, create_targetted_legacy_effect(
                                           instance->parent_controller, instance->parent_card,
                                           target_land_becomes_swamp_until_eot_effect,
                                           instance->targets[0].player, instance->targets[0].card));
    source_card = instance->parent_card;
    legacy->targets[0].player = instance->parent_controller;
    legacy->targets[0].card = source_card;
    legacy->number_of_targets = '\x01';
  }
  prompt = get_hacked_land_text(player, card, "Target land becomes %a .", 0x5009);
  return generic_activated_ability(player, card, event, 7, 0, 0, 0, 0, 0, 0, 0, &td, prompt);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2488740; original cards/invasion.c:7125. */
static int effect_sacrifice_when_lose_control_then_gain_life_equal_toughness(int player, int card,
                                                                      event_t event)
{
  card_instance_t *instance;
  int result;

  if (event == EVENT_CARDCONTROLLED)
  {
    instance = get_card_instance(player, card);
    if (((instance != NULL) && (instance->damage_target_card == affected_card)) &&
        (instance->damage_target_player == affected_card_controller))
    {
      result = check_special_flags2((int)instance->damage_target_player,
                                         instance->damage_target_card, SF2_CANNOT_BE_SACRIFICED);
      if (result == 0)
      {
        kill_card((int)instance->damage_target_player, instance->damage_target_card,
                  KILL_SACRIFICE);
        result = get_toughness((int)instance->damage_target_player, instance->damage_target_card);
        gain_life(player, result);
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x249c930; original cards/invasion.c:7141. */
int card_spinal_embrace(int player, int card, event_t event)
{
  /* Spinal Embrace	|3|U|U|B
   * Instant
   * Cast this spell only during combat.
   * Untap target creature you don't control and gain control of it. It gains haste until end of turn. At the beginning of the next end step, sacrifice it. If you do, you gain life equal to its toughness. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if ((result != 0) && (result = instance->targets[0].player, result != player))
    {
      untap_card(result, instance->targets[0].card);
      result = gain_control_until_eot_haste(player, card, instance->targets[0].player,
                                            instance->targets[0].card);
      alternate_legacy_text(1, player, result);
      result = create_targetted_legacy_effect(
          player, card, effect_sacrifice_when_lose_control_then_gain_life_equal_toughness,
          instance->targets[0].player, instance->targets[0].card);
      alternate_legacy_text(2, player, result);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td,
                              "Select target creature you don\'t control to untap & gain control.",
                              1, NULL);
}
#endif

/* ManalinkEh 2022: 0x249cb00; original cards/invasion.c:7173. */
int card_spirit_weaver(int player, int card, event_t event)
{
  /* Spirit Weaver	|1|W
   * Creature - Human Wizard 2/1
   * |2: Target green or blue creature gets +0/+1 until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  char msg[100];

  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 2);
  td.required_color = get_sleighted_color_test(player, card, 0xc);
  td.preferred_controller = player;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = valid_target(&td);
    if (result != 0)
    {
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                             instance->targets[0].player, instance->targets[0].card, 0, 1, 0, 0);
    }
  }
  const char *color_text = get_sleighted_color_text2(
      player, card, "Select target %s or %s creature.", 3, 2);
  snprintf(msg, sizeof(msg), "%s", color_text);
  result = generic_activated_ability(player, card, event, 6, 2, 0, 0, 0, 0, 0, 0, &td, msg);
  return result;
}

/* ManalinkEh 2022: 0x249ccf0; original cards/invasion.c:7200. */
int card_spreading_plague(int player, int card, event_t event)
{
  /* Spreading Plague	|4|B
   * Enchantment
   * Whenever a creature enters, destroy all other creatures that share a color with it. They can't be regenerated. */

  int result;
  test_definition_t test;
  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    result = get_color(trigger_cause_controller, trigger_cause);
    if (result != 0)
    {
      etb_ability_global(2, event, 2, TYPE_CREATURE, NULL, player, card);
    }
  }
  if (event == EVENT_ETB)
  {
    state_untargettable(trigger_cause_controller, trigger_cause, 1);
    default_test_definition(&test, 2);
    test.color = get_color(trigger_cause_controller, trigger_cause);
    new_manipulate_all(player, card, 2, &test, ACT_KILL_BURY);
    state_untargettable(trigger_cause_controller, trigger_cause, 0);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x249ce40; original cards/invasion.c:7224. */
int card_stand_deliver(int player, int card, event_t event)
{
  /* Stand // Deliver	|W // |2|U
   * Instant // Instant
   * Stand:
   * Prevent the next 2 damage that would be dealt to target creature this turn.
   *
   * Deliver:
   * Return target permanent to its owner's hand. */

  int available_or_flags;
  card_instance_t *instance;
  unsigned int selected_modes;
  int first_half_available;
  target_definition_t *td;
  const char *prompt;
  target_definition_t td_cr;
  target_definition_t td_per;

  td = &td_cr;
  default_target_definition(player, card, td, 2);
  default_target_definition(player, card, &td_per, 0x47);
  if (event == EVENT_MODIFY_COST)
  {

    first_half_available = can_target(td);
    available_or_flags = 0;
    if (first_half_available != 0)
    {
      available_or_flags = (((land_can_be_played & 4U) == 0) ? 1U : 99U);
    }
    return split_card_impl(player, card, EVENT_MODIFY_COST, 0x4b36, available_or_flags, 0, 0x4ae8,
                           can_target(&td_per), 0, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    prompt = "Select target creature to prevent next 2 damage to it.";
    available_or_flags = 0x81;
    return generic_spell(player, card, EVENT_CAST_SPELL, available_or_flags, td, prompt, 1, NULL);
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    prompt = "TARGET_PERMANENT";
    available_or_flags = 1;
    td = &td_per;
    return generic_spell(player, card, EVENT_CAST_SPELL, available_or_flags, td, prompt, 1, NULL);
  }
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
    else
    {
      if (event == EVENT_RESOLVE_SPELL)
      {

        instance = get_card_instance(player, card);
        selected_modes = instance->info_slot;
        if ((selected_modes & 1) != 0)
        {
          available_or_flags = valid_target(td);
          if (available_or_flags != 0)
          {
            prevent_the_next_n_damage(player, card, instance->targets[0].player,
                                      instance->targets[0].card, 2, 0, 0, 0);
          }
          selected_modes = instance->info_slot;
        }
        if (((selected_modes & 2) != 0) &&
            (available_or_flags = valid_target(&td_per), available_or_flags != 0))
        {
          bounce_permanent(instance->targets[0].player, instance->targets[0].card);
        }
        kill_card(player, card, KILL_DESTROY);
        available_or_flags = can_target(td);
        first_half_available = 0;
        if (available_or_flags != 0)
        {
          first_half_available =
              (((land_can_be_played & 4U) == 0) ? 1U : 99U);
        }
        return split_card_impl(player, card, event, 0x4b36, first_half_available, 10, 0x4ae8,
                               can_target(&td_per), 5, 0);
      }
      if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
      {
        return 0;
      }
    }
  }
  available_or_flags = can_target(td);
  first_half_available = 0;
  if (available_or_flags != 0)
  {
    first_half_available = (((land_can_be_played & 4U) == 0) ? 1U : 99U);
  }
  return split_card_impl(player, card, event, 0x4b36, first_half_available, 10, 0x4ae8,
                         can_target(&td_per), 5, 0);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x249d190; original cards/invasion.c:7281. */
int card_strength_of_unity(int player, int card, event_t event)
{
  /* Strength of Unity	|3|W
   * Enchantment - Aura
   * Enchant creature
   * Domain - Enchanted creature gets +1/+1 for each basic land type among lands you control. */

  int result;
  card_instance_t *instance;

  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (instance->damage_target_card == affected_card)) &&
        (instance->damage_target_player == affected_card_controller))
    {
      result = count_domain(player, card);
      event_result = event_result + result;
    }
  }
  result = aura_pref_controller(player, card, event, player, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x249d220; original cards/invasion.c:7297. */
int card_sulam_djinn(int player, int card, event_t event)
{
  /* Sulam Djinn	|5|G
   * Creature - Djinn 6/6
   * Trample
   * This creature gets -2/-2 as long as green is the most common color among all permanents or is tied for most common. */

  minus_2_minus_2_creature_as_long_as_color_is_most_common_color(player, card, event,
                                                                 COLOR_TEST_GREEN);
  return 0;
}

/* ManalinkEh 2022: 0x249d240; original cards/invasion.c:7308. */
int card_sway_of_llusion(int player, int card, event_t event)
{
  /* Sway of Illusion	|1|U
   * Instant
   * Any number of target creatures become the color of your choice until end of turn.
   * Draw a card. */

  int i;
  card_instance_t *instance;
  int selected_color;
  target_t *target_cursor;
  target_t *other_target_cursor;
  int available_or_index;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 2;
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 0, NULL, NULL, 0, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.allow_cancel = 2;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        i = 0;
        instance->number_of_targets = '\0';
        do
        {
          available_or_index = can_target(&td);
          if ((available_or_index == 0) ||
              (available_or_index = new_pick_target(&td, "TARGET_CREATURE", i, 0),
               available_or_index == 0))
          {
            if (i < 1)
            {

              if (player != 0 || i != 0)
              {
                return 0;
              }
              if (do_dialog(0, 0, card, -1, -1, " Continue\n Abort casting", 0) != 0)
              {
                return 0;
              }
              spell_fizzled = 1;
              return 0;
            }
            break;
          }
          other_target_cursor = instance->targets + i;
          target_cursor = instance->targets + i;
          i = i + 1;
          state_untargettable(target_cursor->player, other_target_cursor->card, 1);
        } while (i != 10);
        target_cursor = instance->targets;
        do
        {
          other_target_cursor = target_cursor + 1;
          state_untargettable(target_cursor->player, target_cursor->card, 0);
          target_cursor = other_target_cursor;
        } while (instance->targets + i != other_target_cursor);
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 2;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      selected_color = choose_a_color(player, get_deck_color(player, 1 - player));
      if (instance->number_of_targets != '\0')
      {
        available_or_index = 0;
        do
        {
          if (validate_target(player, card, &td, available_or_index) == 0)
          {
            available_or_index = available_or_index + 1;
            continue;
          }
          other_target_cursor = instance->targets + available_or_index;
          target_cursor = instance->targets + available_or_index;
          available_or_index = available_or_index + 1;
          change_color(player, card, target_cursor->player, other_target_cursor->card,
                       1 << ((unsigned char)selected_color & 0x1f),
                       CHANGE_COLOR_END_AT_EOT | CHANGE_COLOR_SET);
        } while (available_or_index < (int)(unsigned int)instance->number_of_targets);
      }
      draw_a_card(player);
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 2;
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x249d5a0; original cards/invasion.c:7371. */
int card_tainted_well(int player, int card, event_t event)
{
  /* Tainted Well	|2|B
   * Enchantment - Aura
   * Enchant land
   * When this Aura enters, draw a card.
   * Enchanted land is a Swamp. */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_cards(player, 1);
  }
  result = card_evil_presence(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x249d600; original cards/invasion.c:7388. */
int card_temporal_distortion(int player, int card, event_t event)
{
  /* Temporal Distortion	|3|U|U
   * Enchantment
   * Whenever a creature or land becomes tapped, put an hourglass counter on it.
   * Each permanent with an hourglass counter on it doesn't untap during its controller's untap step.
   * At the beginning of each player's upkeep, remove all hourglass counters from permanents that player controls. */

  int p;
  card_instance_t *instance;
  int eligible_or_counter_count;
  int permanent_or_counters;
  int card_00;

  instance = in_play(player, card);
  if (((instance == NULL) || (eligible_or_counter_count = is_humiliated(player, card, AL_TRIG),
                              event != EVENT_TAP_CARD)) ||
      (eligible_or_counter_count != 0))
  {
    if ((current_phase == 1) && (event == EVENT_UNTAP))
    {
      eligible_or_counter_count = is_humiliated(player, card, AL_STATIC);
      if (((eligible_or_counter_count == 0) &&
           (eligible_or_counter_count = is_what(affected_card_controller, affected_card, 0x47),
            eligible_or_counter_count != 0)) &&
          ((eligible_or_counter_count =
                count_counters(affected_card_controller, affected_card, COUNTER_HOURGLASS),
            0 < eligible_or_counter_count && (instance = in_play(player, card), instance != NULL))))
      {
        does_not_untap(affected_card_controller, affected_card, EVENT_UNTAP);
      }
      upkeep_trigger_ability(player, card, EVENT_UNTAP, 2);
    }
    else
    {
      upkeep_trigger_ability(player, card, event, 2);
      p = current_turn;

      if ((event == EVENT_UPKEEP_TRIGGER_ABILITY) && (0 < active_cards_count[current_turn]))
      {
        card_00 = 0;
        do
        {
          permanent_or_counters = is_what(p, card_00, 0x47);
          if (((permanent_or_counters != 0) &&
               (instance = in_play(p, card_00), instance != NULL)) &&
              (permanent_or_counters = count_counters(p, card_00, COUNTER_HOURGLASS),
               0 < permanent_or_counters))
          {
            remove_all_counters(player, card, p, card_00, COUNTER_HOURGLASS);
          }
          card_00 = card_00 + 1;
        } while (card_00 < active_cards_count[p]);
      }
    }
  }
  else
  {
    eligible_or_counter_count = is_what(affected_card_controller, affected_card, 1);
    if ((eligible_or_counter_count != 0) ||
        (eligible_or_counter_count = is_what(affected_card_controller, affected_card, 2),
         eligible_or_counter_count != 0))
    {
      add_counters(player, card, affected_card_controller, affected_card, COUNTER_HOURGLASS, 1);
    }
    upkeep_trigger_ability(player, card, EVENT_TAP_CARD, 2);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x249d8a0; original cards/invasion.c:7425. */
int card_tidal_visionary(int player, int card, event_t event)
{
  /* Tidal Visionary	|U
   * Creature - Merfolk Wizard 1/1
   * |T: Target creature becomes the color of your choice until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 2);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    instance = get_card_instance(player, card);
    result = get_deck_color(player, 1 - player);
    result = choose_a_color(player, result);
    change_color(instance->parent_controller, instance->parent_card, instance->targets[0].player,
                 instance->targets[0].card, 1 << ((unsigned char)result & 0x1f),
                 CHANGE_COLOR_END_AT_EOT | CHANGE_COLOR_SET);
  }
  result = generic_activated_ability(
      player, card, event, 7, 0, 0, 0, 0, 0, 0, 0, &td,
      "Select target creature it becomes the color of your choice until end of turn.");
  return result;
}

/* ManalinkEh 2022: 0x249da10; original cards/invasion.c:7448. */
int card_tolarian_emissary(int player, int card, event_t event)
{
  /* Tolarian Emissary	|2|U
   * Creature - Human Wizard 1/2
   * Kicker |1|W (You may pay an additional |1|W as you cast this spell.)
   * Flying
   * When this creature enters, if it was kicked, destroy target enchantment. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  kicker(player, card, event, 1, 0, 0, 0, 0, 1);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    result = check_special_flags(player, card, 0x80);
    if (result != 0)
    {
      default_target_definition(player, card, &td, 4);
      td.allow_cancel = 0;
      instance = get_card_instance(player, card);
      result = can_target(&td);
      if (result != 0)
      {
        result = pick_target(&td, "TARGET_ENCHANTMENT");
        if (result != 0)
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
      }
      instance->number_of_targets = '\0';
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x249db20; original cards/invasion.c:7471. */
int card_treefolk_healer(int player, int card, event_t event)
{
  /* Treefolk Healer	|4|G
   * Creature - Treefolk Cleric 2/3
   * |2|W, |T: Prevent the next 2 damage that would be dealt to any target this turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td_prev;
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td_prev, 0x102);
    td_prev.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0, 0, 2, 0,
                                         &td_prev, NULL);
      if (result == 0)
      {
        return 0;
      }
      return (((land_can_be_played & 4U) == 0) ? 1U : 99U);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
      result = generic_activated_ability(player, card, EVENT_ACTIVATE, 7, 0, 0, 0, 0, 0, 2, 0,
                                         &td_prev, "Select any target to prevent up to 2 damage.");
      if (player != 1)
      {
        return 0;
      }
      if (result == 0)
      {
        return 0;
      }
      if (instance->targets[0].player == 1)
      {
        return 0;
      }
      ai_modifier = ai_modifier + -0x60;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td_prev, 0x102);
  }
  td_prev.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td_prev), result != 0))
  {
    instance = get_card_instance(player, card);
    prevent_the_next_n_damage(player, card, instance->targets[0].player, instance->targets[0].card,
                              2, 0, 0, 0);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x249dd50; original cards/invasion.c:7509. */
int card_trevas_attendant(int player, int card, event_t event)
{
  /* Treva's Attendant	|5
   * Artifact Creature - Golem 3/3
   * |1, Sacrifice this creature: Add |G|W|U. */

  card_instance_t *instance;
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      ai_modifier = ai_modifier + -0x18;
      instance = get_card_instance(player, card);
      instance->state = instance->state | 0x10;
      charge_mana(player, COLOR_COLORLESS, 1);
      if (spell_fizzled == 1)
      {
        instance->state = instance->state & 0xffffffef;
      }
      else
      {
        produce_mana_multi(player, 0, 0, 1, 1, 0, 1);
        kill_card(player, card, KILL_SACRIFICE);
      }
    }
    return 0;
  }
  return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 1, 0, 0, 0, 0, 0, 0, NULL,
                                     NULL);
}

/* ManalinkEh 2022: 0x249deb0; original cards/invasion.c:7537. */
int card_tsabos_assassin(int player, int card, event_t event)
{
  /* Tsabo's Assassin	|2|B|B
   * Creature - Phyrexian Zombie Assassin 1/1
   * |T: Destroy target creature if it shares a color with the most common color among all permanents or a color tied for most common. A creature destroyed this way can't be regenerated. */

  card_instance_t *instance;
  unsigned int target_colors;
  unsigned int common_colors;
  target_definition_t td;

  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 2);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (valid_target(&td) != 0))
  {

    instance = get_card_instance(player, card);
    target_colors = get_color(instance->targets[0].player, instance->targets[0].card);
    common_colors = most_common_color_all_permanents(2);
    if ((target_colors & common_colors) != 0)
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
    }
  }
  return generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                   "TARGET_CREATURE");
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x249e010; original cards/invasion.c:7565. */
int card_urborg_phantom(int player, int card, event_t event)
{
  /* Urborg Phantom	|2|B
   * Creature - Spirit Minion 3/1
   * This creature can't block.
   * |U: Prevent all combat damage that would be dealt to and dealt by this creature this turn. */

  int result;
  card_instance_t *instance;
  cannot_block(player, card, event);
  if (event == EVENT_CHECK_ABILITIES)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (result = card_urborg_phantom(player, card, EVENT_CAN_ACTIVATE), result != 0))
    {
      if (1 - player == current_turn)
      {
        result = get_power(player, card);
        check_abilities_power = check_abilities_power - result;
        check_abilities_toughness = check_abilities_toughness + 0x3fff;
      }
      else
      {
        check_abilities_toughness = check_abilities_toughness + 0x3fff;
      }
    }
  }
  else
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 1, 0, 0, 0,
                                              0, NULL, NULL);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      if (((current_turn == player) && (result = is_unblocked(current_turn, card), result != 0)) ||
          ((1 - player == current_turn &&
            (instance = get_card_instance(player, card), instance->blocking == 0xff))))
      {
        ai_modifier = ai_modifier + -0x19;
      }
      result = generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 0, 0, 1, 0, 0, 0, 0,
                                              NULL, NULL);
      return result;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      maze_of_ith_effect(instance->parent_controller, instance->parent_card,
                         instance->parent_controller, instance->parent_card);
      return 0;
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x249e270; original cards/invasion.c:7605. */
int card_urborg_skeleton(int player, int card, event_t event)
{
  /* Urborg Skeleton	|B
   * Creature - Skeleton 0/1
   * Kicker |3 (You may pay an additional |3 as you cast this spell.)
   * |B: Regenerate this creature.
   * If this creature was kicked, it enters with a +1/+1 counter on it. */

  int result;
  if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    do_kicker(player, card, 3, 0, 0, 0, 0, 0);
  }
  result = check_special_flags(player, card, 0x80);
  if (result != 0)
  {
    enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 1);
  }
  return card_drudge_skeletons(player, card, event);
}

/* ManalinkEh 2022: 0x249e330; original cards/invasion.c:7624. */
int card_viashino_grappler(int player, int card, event_t event)
{
  /* Viashino Grappler	|2|R
   * Creature - Lizard 3/1
   * |G: This creature gains trample until end of turn. */

  int result;
  result = generic_shade(player, card, event, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0x80, 0);
  return result;
}

/* ManalinkEh 2022: 0x249e3b0; original cards/invasion.c:7633. */
int card_vodalian_serpent(int player, int card, event_t event)
{
  /* Vodalian Serpent	|3|U
   * Creature - Serpent 2/2
   * Kicker |2 (You may pay an additional |2 as you cast this spell.)
   * This creature can't attack unless defending player controls an Island.
   * If this creature was kicked, it enters with four +1/+1 counters on it. */

  int kicker_flags;
  int island_type_or_counters;

  if (((event == EVENT_ATTACK_LEGALITY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    if (is_humiliated(player, card, AL_STATIC) == 0)
    {
      island_type_or_counters = get_hacked_color(player, card, 2);
      if (basiclandtypes_controlled[1 - player][island_type_or_counters] == 0)
      {
        event_result = 1;
      }
    }
  }
  kicker(player, card, event, 2, 0, 0, 0, 0, 0);
  kicker_flags = check_special_flags(player, card, 0x80);
  island_type_or_counters = 0;
  if (kicker_flags != 0)
  {
    island_type_or_counters = 4;
  }
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, island_type_or_counters);
  return 0;
}

/* ManalinkEh 2022: 0x249e4d0; original cards/invasion.c:7651. */
int card_wallop(int player, int card, event_t event)
{
  /* Wallop	|1|G
   * Sorcery
   * Destroy target blue or black creature with flying. */

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  target_definition_t td;
  default_target_definition(player, card, &td, 2);
  td.required_color = get_sleighted_color_test(player, card, 6);
  td.required_abilities = 0x20;
  td.preferred_controller = 1 - player;
  if (event == EVENT_RESOLVE_SPELL)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (valid_target(&td))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  const char *prompt =
      get_sleighted_color_text2(player, card, "Select target %s or %s creature with flying.", 2, 1);
  return generic_spell(player, card, event, 0x81, &td, prompt, 1, NULL);
}

/* ManalinkEh 2022: 0x249e690; original cards/invasion.c:7679. */
int card_wandering_stream(int player, int card, event_t event)
{
  /* Wandering Stream	|2|G
   * Sorcery
   * Domain - You gain 2 life for each basic land type among lands you control. */

  int result;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = count_domain(player, card);
    gain_life(player, result * 2);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x249e6f0; original cards/invasion.c:7693. */
int card_wayfaring_giant(int player, int card, event_t event)
{
  /* Wayfaring Giant	|5|W
   * Creature - Giant 1/3
   * Domain - This creature gets +1/+1 for each basic land type among lands you control. */

  card_instance_t *instance;
  int result;

  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = count_domain(player, card);
        event_result = event_result + result;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x249e780; original cards/invasion.c:7705. */
int card_well_laid_plans(int player, int card, event_t event)
{
  /* Well-Laid Plans	|2|U
   * Enchantment
   * Prevent all damage that would be dealt to a creature by another creature if they share a color. */

  card_instance_t *instance;
  unsigned int target_colors;
  unsigned int source_colors;

  instance = damage_being_dealt(event);
  if ((instance != NULL) && (instance->damage_source_card != -1))
  {
    if (is_what((int)instance->damage_source_player, instance->damage_source_card, 2))
    {
      if (is_what((int)instance->damage_target_player, instance->damage_target_card, 2))
      {
        target_colors =
            get_color((int)instance->damage_target_player, instance->damage_target_card);
        source_colors =
            get_color((int)instance->damage_source_player, instance->damage_source_card);
        if ((target_colors & source_colors) != 0)
        {
          prevent_damage(affected_card_controller, affected_card, instance->info_slot);
        }
      }
    }
  }
  return global_enchantment(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x249e870; original cards/invasion.c:7724. */
int card_wings_of_hope(int player, int card, event_t event)
{
  /* Wings of Hope	|W|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+3 and has flying. */

  int result;

  attached_gets_pt(player, card, event, 1, 3);
  attached_gets_kw(player, card, event, KEYWORD_FLYING, SP_KEYWORD_0);
  result = aura_pref_controller(player, card, event, player, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x249e8f0; original cards/invasion.c:7736. */
int card_winnow(int player, int card, event_t event)
{
  /* Winnow	|1|W
   * Instant
   * Destroy target nonland permanent if another permanent with the same name is on the battlefield.
   * Draw a card. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.illegal_type = TYPE_LAND;
  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    result = validate_target(player, card, &td, 0);
    if (result != 0)
    {
      result = get_card_name(instance->targets[0].player, instance->targets[0].card);
      result = count_permanents_by_name(2, TYPE_PERMANENT, result);
      if (1 < result)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
      draw_a_card(player);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target nonland permanent.", 1, NULL);
}

/* ManalinkEh 2022: 0x249eaa0; original cards/invasion.c:7765. */
int card_yavimaya_kavu(int player, int card, event_t event)
{
  /* Yavimaya Kavu	|2|R|G
   * Creature - Kavu * / *
   * ~'s power is equal to the number of red creatures on the battlefield.
   * ~'s toughness is equal to the number of green creatures on the battlefield. */

  if ((event != EVENT_POWER && event != EVENT_TOUGHNESS) || card != affected_card ||
      affected_card_controller != player || player == -1)
    return 0;
  int color = get_sleighted_color_test(player, card, event == EVENT_POWER ? 0x10 : 8);
  int creatures = count_permanents_by_color(2, TYPE_CREATURE, color);
  set_characteristic_pt(creatures);
  return 0;
}

/* ManalinkEh 2022: 0x249eb40; original cards/invasion.c:7782. */
int card_zanam_djinn(int player, int card, event_t event)
{
  /* Zanam Djinn	|5|U
   * Creature - Djinn 5/6
   * Flying
   * This creature gets -2/-2 as long as blue is the most common color among all permanents or is tied for most common. */

  minus_2_minus_2_creature_as_long_as_color_is_most_common_color(player, card, event,
                                                                 COLOR_TEST_BLUE);
  return 0;
}
