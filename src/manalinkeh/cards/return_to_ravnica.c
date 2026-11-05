#include "manalink.h"
#ifdef MANALINKEH_STATIC
#include "magic/src/duel_host.h"
#endif
#include <stddef.h>

/* Canonical recovered helper declarations. */
int overload(int player, int card, event_t event, int colorless, int black, int blue,
                  int green, int red, int white);
int pay_overload(int player, int card, int colorless, int black, int blue, int green, int red,
                      int white);

// Functions
static int detain_legacy(int player, int card, event_t event)
{

  if (get_card_instance(player, card)->damage_target_player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);

    int p = instance->damage_target_player;
    int c = instance->damage_target_card;

    cannot_attack(p, c, event);
    cannot_block(p, c, event);
    disable_all_activated_abilities(p, c, 1);

    if (event == EVENT_BEGIN_TURN && current_turn == player)
    {
      disable_all_activated_abilities(p, c, 0);
      kill_card(player, card, KILL_EXILE);
    }
  }

  return 0;
}

int detain(int player, int card, int t_player, int t_card)
{
  return create_targetted_legacy_effect(player, card, &detain_legacy, t_player, t_card);
}

static void ai_modifier_for_detain(int player, int card, int target_pos){
	card_instance_t *instance = get_card_instance(player, card);
	int p = instance->targets[target_pos].player;
	int c = instance->targets[target_pos].card;
	int iid = get_card_instance(p, c)->internal_card_id;
	if( cards_data[iid].extra_ability & (EA_ACT_ABILITY | EA_ACT_INTERRUPT) ){
		ai_modifier += 12;
	}
	if( is_what(p, c, TYPE_CREATURE) &&  ! check_for_ability(p, c, KEYWORD_DEFENDER) &&
		! check_for_special_ability(p, c, SP_KEYWORD_CANNOT_BLOCK) )
	{
		ai_modifier += 24;
	}
	if( is_planeswalker(p, c) ){
		ai_modifier += 48;
	}
}

void populate(int player, int card)
{
  if (player == AI || ai_is_speculating == 1)
	{
	  test_definition_t this_test;
	  default_test_definition(&this_test, TYPE_CREATURE);
	  this_test.type_flag = F1_IS_TOKEN;

	  int result = check_battlefield_for_special_card(player, card, player, CBFSC_AI_MAX_VALUE, &this_test);

	  if (result > -1)
		copy_token(player, card, player, result);
	}
  else
	{
	  target_definition_t td;
	  base_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = player;
	  td.preferred_controller = player;
	  td.allow_cancel = 0;
	  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	  td.extra = target_predicate_extra(target_is_token_too);

	  target_t tgt;

	  if (can_target(&td) && select_target(player, card-1000, &td, "Select a creature token you control.", &tgt))
		copy_token(player, card, tgt.player, tgt.card);
	}
}

int overload(int player, int card, event_t event, int colorless, int black, int blue,
                  int green, int red, int white)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    result = get_updated_casting_cost(player, card, -1, EVENT_MODIFY_COST, colorless);
    result = has_mana_multi(player, result, black, blue, green, red, white);
    if (result == 0)
    {
      instance->info_slot = 0;
      return 0;
    }
    null_casting_cost(player, card);
    instance->info_slot = 1;
  }
  return 0;
}

int pay_overload(int player, int card, int colorless, int black, int blue, int green, int red,
                 int white)
{
  int choice;
  int old_info_slot;
  card_instance_t *instance;
  int updated_cost_or_result;
  int normal_cast_available;
  bool double_x_or_fizzled;

  instance = get_card_instance(player, card);
  updated_cost_or_result = played_for_free(player, card);
  if (((updated_cost_or_result != 0) ||
       (updated_cost_or_result = is_token(player, card), updated_cost_or_result != 0)) ||
      (instance->info_slot == 0))
  {
    return 1;
  }
  double_x_or_fizzled = colorless == -2;
  if (double_x_or_fizzled)
  {
    colorless = 0;
  }
  updated_cost_or_result = get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, colorless);
  if (has_mana_multi(player, updated_cost_or_result, black, blue, green, red, white) == 0)
  {
    return 0;
  }
  old_info_slot = instance->info_slot;
  instance->info_slot = 0;
  normal_cast_available = call_card_function(player, card, EVENT_CAN_CAST);
  instance->info_slot = old_info_slot;
  choice = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1,
                       "Pay Overload cost", 1, 10, "Play the spell normally", normal_cast_available,
                       3, 0, 0, 0, 0);
  if (choice == 1)
  {
    charge_mana_multi(player, updated_cost_or_result, black, blue, green, red, white);
    if (spell_fizzled == 1)
    {
      return 0;
    }
    if (double_x_or_fizzled)
    {
      updated_cost_or_result = charge_mana_for_double_x(player, 0);
      double_x_or_fizzled = spell_fizzled == 1;
      instance->targets[1].player = updated_cost_or_result;
      if (double_x_or_fizzled)
      {
        return 0;
      }
    }
    return 2;
  }
  if (choice != 2)
  {
    spell_fizzled = 1;
    return 0;
  }
  return charge_mana_from_id(player, card, EVENT_CAST_SPELL, get_id(player, card));
}

int unleash(int player, int card, event_t event)
{
  /*
  Unleash (You may have this creature enter the battlefield with a +1/+1 counter on it.
                  It can't block as long as it has a +1/+1 counter on it.)
  */

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
  {
    int choice = do_dialog(player, player, card, -1, -1, " Give a +1/+1 counter\n Decline", 0);
    if (choice == 0)
    {
      enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 1);
    }
  }

  if (event == EVENT_ABILITIES && affect_me(player, card) &&
      !is_humiliated(player, card, AL_STATIC) && count_1_1_counters(player, card))
  {
    cannot_block(player, card, event);
  }

  return 0;
}

int scavenge(int player, int card, event_t event, int colorless, int black, int blue, int green,
             int red, int white)
{
  int result;
  card_instance_t *instance;
  target_definition_t td;
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = has_mana_multi(player, colorless, black, blue, green, red, white);
    if (((result != 0) &&
         (result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY), result != 0)) &&
        (result = can_target(&td), result != 0))
    {
      return 0x60;
    }
  }
  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    charge_mana_multi(player, colorless, black, blue, green, red, white);
    if (spell_fizzled != 1)
    {
      return 2;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATED_GRAVEYARD_ABILITY)
  {
    td.allow_cancel = 0;
    result = pick_target(&td, "TARGET_CREATURE");
    if ((result != 0) && (result = valid_target(&td), result != 0))
    {
      instance = get_card_instance(player, card);
      result = get_base_power_iid(player, instance->internal_card_id);
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                               COUNTER_P1_P1, result);
      return 0;
    }
  }
  return 0;
}

int keyrune(int player, int card, event_t event, int cless, int black, int blue, int green, int red,
            int white, int pow, int tou, int key, int s_key, int clr, int subtype)
{

  if (event == EVENT_CLEANUP)
  {
    get_card_instance(player, card)->targets[1].player = 0;
  }

  if (!IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE)
  {
    if (mana_producer(player, card, event))
      return 1;
    return generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, cless, black, blue,
                                     green, red, white, 0, NULL, NULL) != 0;
  }

  card_instance_t *instance = get_card_instance(player, card);

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ANIMATE
  };

  if (event == EVENT_ACTIVATE)
  {
    int choice = CHOICE_MANA;
    if (!paying_mana() &&
        generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_TYPE_CHANGE, cless, black,
                                  blue, green, red, white, 0, 0, 0))
    {
      choice = CHOICE_ANIMATE;
    }
    instance->info_slot = choice;
    if (!IS_AI(player) && choice == CHOICE_ANIMATE)
    {
      choice = DIALOG(player, card, event, DLG_RANDOM, "Get mana",
                      mana_producer(player, card, EVENT_CAN_ACTIVATE), 1, "Animate", 1, 1);
      if (!choice)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
    if (instance->info_slot == CHOICE_MANA)
    {
      return mana_producer(player, card, event);
    }
    if (instance->info_slot == CHOICE_ANIMATE)
    {
      generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, cless, black, blue, green,
                                red, white, 0, NULL, NULL);
      if (is_tapped(player, card))
      {
        ai_modifier -= 96;
      }
    }
  }

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->info_slot == 2)
    {
      card_instance_t *parent =
          get_card_instance(instance->parent_controller, instance->parent_card);
      int legacy = artifact_animation(instance->parent_controller, instance->parent_card,
                                      instance->parent_controller, instance->parent_card, 1, pow,
                                      tou, key, s_key);
      card_instance_t *leg = get_card_instance(instance->parent_controller, legacy);
      leg->targets[6].player = clr;
      leg->targets[9].card = subtype;
      parent->targets[1].player = 66;
    }
  }

  return 0;
}

// Cards

// White
int card_angel_of_serenity(int player, int card, event_t event)
{
  /* Angel of Serenity	|4|W|W|W
   * Creature - Angel 5/6
   * Flying
   * When this creature enters, you may exile up to three other target creatures from the battlefield and/or creature cards from graveyards.
   * When this creature leaves the battlefield, return the exiled cards to their owners' hands. */

  unsigned int exiled_card;
  int opponent_grave_index;
  unsigned int *exiled_record;
  card_instance_t *instance;
  int selection_or_opponent;
  unsigned int player_00;
  int exiles_remaining;
  int leg;
  int idx;
  int cg[2];
  target_definition_t td;
  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 2);
    td.special = TARGET_SPECIAL_NOT_ME;
    instance = get_card_instance(player, card);
    exiles_remaining = 3;
    while ((selection_or_opponent = can_target(&td),
            selection_or_opponent != 0 &&
                (selection_or_opponent =
                     select_target(player, card, &td, "Select target creature to exile.", NULL),
                 selection_or_opponent != 0)))
    {
      selection_or_opponent = instance->targets[0].card;
      instance->number_of_targets = '\0';
      exile_card_and_remember_it_on_exiledby(player, card, instance->targets[0].player,
                                             selection_or_opponent);
      exiles_remaining = exiles_remaining + -1;
      if (exiles_remaining == 0)
      {
        return 0;
      }
    }
    if (exiles_remaining != 0)
    {
      if (graveyard_has_shroud(0) == 0)
      {
        cg[0] = count_graveyard_by_type(0, 2);
      }
      selection_or_opponent = graveyard_has_shroud(1);
      if (selection_or_opponent == 0)
      {
        cg[1] = count_graveyard_by_type(1, 2);
      }
      new_default_test_definition(&test, 2, "Select target creature card to exile.");
      leg = 0;
      idx = 0;
      if (cg[player] != 0)
      {
        while (selection_or_opponent = new_select_a_card(player, player, 2, 0, 2, -1, &test),
               selection_or_opponent != -1)
        {
          exiledby_remember(player, card, player,
                            graveyard_ptr_mutable[player][selection_or_opponent], &leg, &idx);
          rfg_card_from_grave(player, selection_or_opponent);
        }
      }
      selection_or_opponent = 1 - player;
      if (cg[selection_or_opponent] != 0)
      {
        while (opponent_grave_index =
                   new_select_a_card(player, selection_or_opponent, 2, 0, 2, -1, &test),
               opponent_grave_index != -1)
        {
          exiledby_remember(player, card, selection_or_opponent,
                            graveyard_ptr_mutable[selection_or_opponent][opponent_grave_index],
                            &leg, &idx);
          rfg_card_from_grave(selection_or_opponent, opponent_grave_index);
        }
      }
      leaves_play(player, card, EVENT_ETB, 1, 0);
      return 0;
    }
  }
  else
  {
    leaves_play(player, card, event, 1, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      td.who_chooses = 0;
      test.message[0] = '\0';
      test.message[1] = '\0';
      test.message[2] = '\0';
      test.message[3] = '\0';
      while (exiled_record =
                 (unsigned int *)exiledby_find_any(player, card, &td.who_chooses, (int *)&test),
             exiled_record != NULL)
      {
        exiled_card = *exiled_record;
        *exiled_record = 0xffffffff;
        player_00 = exiled_card >> 0x1f;
        selection_or_opponent = (exiled_card & 0x7fffffff) + exiled_card * 8;
        if (check_rfg(player_00,
                      (unsigned int)*(unsigned short *)(cards_data[0].cc +
                                                        selection_or_opponent * 8 + -7)) != 0)
        {
          add_card_to_hand(player_00, exiled_card & 0x7fffffff);
          remove_card_from_rfg(
              player_00,
              (unsigned int)*(unsigned short *)(cards_data[0].cc + selection_or_opponent * 8 + -7));
        }
      }
    }
  }
  return 0;
}

int card_armory_guard(int player, int card, event_t event){
/* CARD_ID_ARMORY_GUARD	12653
Armory Guard	|3|W
Creature - Giant Soldier 2/5
~ has vigilance as long as you control a Gate. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) &&
		check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_GATE) )
	{
		vigilance(player, card, event);
	}

	return 0;
}

int card_ash_zealot(int player, int card, event_t event){
  /* Ash Zealot |R|R
   * 	  Creature - Human Warrior 2/2
   * 	  First strike, haste
   * 	  Whenever a player casts a spell from a graveyard, Ash Zealot deals 3 damage to that player. */

	haste(player, card, event);
	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && check_special_flags(trigger_cause_controller, trigger_cause, SF_PLAYED_FROM_GRAVE)){
		sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
	}
	if (event == EVENT_SPA){
		damage_player(trigger_cause_controller, 3, player, card);
	}
	return 0;
}

int card_azorius_arrester(int player, int card, event_t event){
/* CARD_ID_AZORIUS_ARRESTER	12660
Azorius Arrester	|1|W
Creature - Human Soldier 2/1
When ~ enters the battlefield, detain target creature an opponent controls.
(Until your next turn, that creature can't attack or block and its activated abilities can't be activated.) */

	// Used externally
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target creature your opponent controls.", 0, GS_LITERAL_PROMPT) ){
			ai_modifier_for_detain(player, card, 0);
			detain(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_azorius_justiciar(int player, int card, event_t event){
/* CARD_ID_AZORIUS_JUSTICIAR	12663
Azorius Justiciar	|2|W|W
Creature - Human Wizard 2/2
When ~ enters the battlefield, detain up to two target creatures your opponents control.
(Until your next turn, those creatures can't attack or block and their activated abilities can't be activated.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		int i;
		for(i=0; i<2; i++){
			if( can_target(&td) && new_pick_target(&td, "Select target creature your opponent controls.", i, GS_LITERAL_PROMPT) ){
				ai_modifier_for_detain(player, card, i);
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
			}
			else{
				break;
			}
		}
		for(i=0; i<instance->number_of_targets; i++){
			detain(player, card, instance->targets[i].player, instance->targets[i].card);
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
		}

		instance->number_of_targets = 0;
	}

	return 0;
}

int card_bazaar_krovod(int player, int card, event_t event)
{
/* CARD_ID_BAZAAR_KROVOD	12667
Bazaar Krovod	|4|W
Creature - Beast 2/5
Whenever ~ attacks, another target attacking creature gets +0/+2 until end of turn. Untap that creature. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.preferred_controller = player;
	  td.allow_cancel = 0;
	  td.required_state = TARGET_STATE_ATTACKING;
	  td.special = TARGET_SPECIAL_NOT_ME;

	  card_instance_t* instance = get_card_instance(player, card);

	  if (can_target(&td) && pick_target(&td, "TARGET_ANOTHER_CREATURE"))
		{
		  pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0,2);
		  untap_card(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

  return 0;
}

// concordia pegasus --> vanilla

int card_ethereal_armor(int player, int card, event_t event)
{
  /* Ethereal Armor	|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+1 for each enchantment you control and has first strike. */

  card_instance_t* instance;
  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES)
	  && (instance = in_play(player, card))
	  && affect_me(instance->damage_target_player, instance->damage_target_card)
	  && ! is_humiliated(player, card, AL_STATIC) )
	{
	  if (event == EVENT_ABILITIES)
		event_result |= KEYWORD_FIRST_STRIKE;
	  else
		event_result += count_subtype(player, TYPE_ENCHANTMENT, -1);
	}

  if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
    return 0;
  }

  return vanilla_aura(player, card, event, player);
}

int card_eyes_in_the_sky(int player, int card, event_t event){
	/* Eyes in the Skies	|3|W
	 * Instant
	 * Put a 1/1 |Swhite Bird creature token with flying onto the battlefield, then populate. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 24;
	}

	if(event == EVENT_RESOLVE_SPELL ){
		generate_token_by_id(player, card, CARD_ID_BIRD);
		populate(player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// fencing ace --> vanilla

// keening apparition --> kami of the ancient law

int card_knightly_valor(int player, int card, event_t event){
/* CARD_ID_KNIGHTLY_VALOR	12762
Knightly Valor	|4|W
Enchantment - Aura
Enchant creature
When ~ enters the battlefield, create a 2/2 |Swhite Knight creature token with vigilance. (Attacking doesn't cause it to tap.)
Enchanted creature gets +2/+2 and has vigilance. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_KNIGHT, &token);
		token.pow = token.tou = 2;
		token.s_key_plus = SP_KEYWORD_VIGILANCE;
		generate_token(&token);
	}

	return generic_aura(player, card, event, player, 2, 2, 0, SP_KEYWORD_VIGILANCE, 0, 0, 0);
}

int card_martial_law(int player, int card, event_t event){
/* CARD_ID_MARTIAL_LAW	12771
Martial Law	|2|W|W
Enchantment
At the beginning of your upkeep, detain target creature an opponent controls.
(Until your next turn, that creature can't attack or block and its activated abilities can't be activated.) */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target creature your opponent controls.", 0, GS_LITERAL_PROMPT) ){
			ai_modifier_for_detain(player, card, 0);
			detain(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_palisade_giant(int player, int card, event_t event){
  /* Palisade Giant	|4|W|W
   * Creature - Giant Soldier 2/7
   * All damage that would be dealt to you or another permanent you control is dealt to ~ instead. */

	if( is_humiliated(player, card, AL_STATIC) )
	   return 0;

	card_instance_t* damage = damage_being_prevented(event);
	if (damage && damage->damage_target_player == player)
		damage->damage_target_card = card;

	return 0;
}

int card_phantom_general(int player, int card, event_t event){
/* CARD_ID_PHANTOM_GENERAL	12786
Phantom General	|3|W
Creature - Spirit Soldier 2/3
Creature tokens you control get +1/+1. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == player && in_play(player, card) &&
		! is_humiliated(player, card, AL_STATIC) && is_token(affected_card_controller, affected_card) )
	{
		event_result++;
	}

	return 0;
}

int card_precint_captain(int player, int card, event_t event){
	/* Precinct Captain	|W|W
	 * Creature - Human Soldier 2/2
	 * First strike
	 * Whenever ~ deals combat damage to a player, put a 1/1 |Swhite Soldier creature token onto the battlefield. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		generate_tokens_by_id(player, card, CARD_ID_SOLDIER, amount);
	}

	return 0;
}

int card_rest_in_peace(int player, int card, event_t event){

  /* Rest in Peace	|1|W
   * Enchantment
   * When ~ enters the battlefield, exile all cards from all graveyards.
   * If a card or token would be put into a graveyard from anywhere, exile it instead. */

  test_definition_t test, *test_ptr;
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
	{
	  new_default_test_definition(&test, 0, "");	// Give it an empty test, not null (which would make it not affect tokens)
	  test_ptr = &test;
	}
  else
	test_ptr = NULL;

  if_a_card_would_be_put_into_graveyard_from_anywhere_exile_it_instead(player, card, event, ANYBODY, test_ptr);

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB_ABILITY)
	{
	  rfg_whole_graveyard(current_turn);
	  rfg_whole_graveyard(1-current_turn);
	}

  return global_enchantment(player, card, event);
}

int card_rootborn_defenses(int player, int card, event_t event){
	/* Rootborn Defenses	|2|W
	 * Instant
	 * Populate. Creatures you control gain indestructible until end of turn. */

	if( event == EVENT_CHECK_PUMP )
	{
		if (!has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id))
			return 0;
		pumpable_toughness[player] += 99;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		populate(player, card);
		pump_creatures_until_eot(player, card, player, 0, 0,0, 0,SP_KEYWORD_INDESTRUCTIBLE, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_security_blockade(int player, int card, event_t event)
{
  /* CARD_ID_SECURITY_BLOCKADE	12813
   * Security Blockade	|2|W
   * Enchantment - Aura
   * Enchant land
   * When ~ enters the battlefield, create a 2/2 |Swhite Knight creature token with vigilance.
   * Enchanted land has "|T: Prevent the next 1 damage that would be dealt to you this turn." */

  int enchanted_card_or_result;
  int activation_flags;
  card_instance_t *instance;
  int target_player;
  event_t event_00;
  target_definition_t *ptd;
  const char *prompt;

  target_definition_t td1;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_KNIGHT, &token);
    token.s_key_plus = 8;
    token.tou = 2;
    token.pow = 2;
    generate_token(&token);
    if (is_humiliated(player, card, AL_STATIC) == 0)
    {
      instance = get_card_instance(player, card);
      if (instance->damage_target_player >= 0)
      {
        instance = get_card_instance(player, card);
        default_target_definition((int)instance->damage_target_player, instance->damage_target_card,
                                  &td1, 0);
      }
    }
  }
  else
  {
    enchanted_card_or_result = is_humiliated(player, card, AL_STATIC);
    if (enchanted_card_or_result == 0 &&
        (instance = get_card_instance(player, card), instance->damage_target_player >= 0))
    {
      instance = get_card_instance(player, card);
      target_player = (int)instance->damage_target_player;
      enchanted_card_or_result = instance->damage_target_card;
      default_target_definition(target_player, enchanted_card_or_result, &td1, 0);
      td1.extra = damage_card;
      td1.special = TARGET_SPECIAL_DAMAGE_PLAYER;
      ptd = &td1;
      if (event == EVENT_CAN_ACTIVATE)
      {
        prompt = "TARGET_DAMAGE";
        activation_flags = 0x1000005;
        event_00 = EVENT_CAN_ACTIVATE;
        return granted_generic_activated_ability(player, card, target_player, enchanted_card_or_result,
                                                 event_00, activation_flags, 0, 0, 0, 0, 0, 0, 0,
                                                 ptd, prompt);
      }
      if (event == EVENT_ACTIVATE)
      {
        instance->number_of_targets = 0;
        if (pick_target(ptd, "TARGET_DAMAGE") != 0)
        {
          prompt = NULL;
          ptd = NULL;
          activation_flags = 1;
          event_00 = EVENT_ACTIVATE;
          return granted_generic_activated_ability(
              player, card, target_player, enchanted_card_or_result, event_00, activation_flags, 0, 0,
              0, 0, 0, 0, 0, ptd, prompt);
        }
      }
      else if (event == EVENT_RESOLVE_ACTIVATION)
      {
        if (validate_target(target_player, enchanted_card_or_result, ptd, 0) != 0)
        {
          instance = get_card_instance(target_player, enchanted_card_or_result);
          prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
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
  target_definition_t td;
  default_target_definition(player, card, &td, 1);
  td.preferred_controller = player;
  td.allowed_controller = player;
  return targeted_aura_custom_prompt(player, card, event, &td, "Select target land you control.");
}

int card_selesnya_sentry(int player, int card, event_t event){
/* CARD_ID_SELESNYA_SENTRY	12817
Selesnya Sentry	|2|W
Creature - Elephant Soldier 3/2
|5|G: Regenerate ~. */

	return regeneration(player, card, event, MANACOST_XG(5, 1));
}

int card_seller_of_songbirds(int player, int card, event_t event){
	/* Seller of Songbirds	|2|W
	 * Creature - Human 1/2
	 * When ~ enters the battlefield, put a 1/1 |Swhite Bird creature token with flying onto the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_token_by_id(player, card, CARD_ID_BIRD);
	}

	return 0;
}

int card_soul_tithe(int player, int card, event_t event){
/* CARD_ID_SOUL_TITHE	12829
Soul Tithe	|1|W
Enchantment - Aura
Enchant nonland permanent
At the beginning of the upkeep of enchanted permanent's controller, that player sacrifices it unless he or she pays |X, where X is its converted mana cost.*/

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		upkeep_trigger_ability(player, card, event, t_player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			int sac = 1;
			int cmc = get_cmc(t_player, t_card);
			if( has_mana(t_player, COLOR_COLORLESS, cmc) ){
				charge_mana(t_player, COLOR_COLORLESS, cmc);
				if( spell_fizzled != 1 ){
					sac = 0;
				}
			}
			if( sac == 1 ){
				kill_card(t_player, t_card, KILL_SACRIFICE);
			}
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_type = TYPE_LAND;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target non-land permanent.");
}

// sphere of safety --> propaganda.

// sunspire griffin --> vanilla

int card_swift_justice(int player, int card, event_t event){
	/* Swift Justice	|W
	 * Instant
	 * Until end of turn, target creature gets +1/+0 and gains first strike and lifelink. */
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 1, 0, KEYWORD_FIRST_STRIKE, SP_KEYWORD_LIFELINK);
}

// trained caracal --> child of night

int card_trostanis_judgement(int player, int card, event_t event){
/* CARD_ID_TROSTANIS_JUDGMENT	12859
Trostani's Judgment	|5|W
Instant
Exile target creature, then populate. (Create a token that's a copy of a creature token you control.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			populate(player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

// Blue
int card_aquus_steed(int player, int card, event_t event){
/* CARD_ID_AQUUS_STEED	12649
Aquus Steed	|3|U
Creature - Beast 1/3
|2|U, |T: Target creature gets -2/-0 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, -2, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XU(2, 1), 0, &td, "TARGET_CREATURE");
}

int card_blustersquall(int player, int card, event_t event){
/* CARD_ID_BLUSTERSQUALL	12671
Blustersquall	|U
Instant
Tap target creature you don't control.
Overload |3|U (You may cast this spell for its overload cost. If you do, change its text by replacing all instances of "target" with "each.")*/

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST ){
			if( generic_spell(player, card, event, 0, NULL, NULL, 0, NULL) ){
				if( instance->info_slot == 1 ){
					return 1;
				}
				return can_target(&td);
			}
		}

		if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			int result = 1;
			instance->number_of_targets = 0;
			if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
				result = pay_overload(player, card, MANACOST_XU(2, 1));
			}
			if( result == 1 ){
				new_pick_target(&td, "Select target creature an opponent controls.", 0, 1 | GS_LITERAL_PROMPT);
				if(	spell_fizzled == 1 ){
					return 0;
				}
			}
			instance->info_slot |= result;
		}

		if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot & 2 ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				new_manipulate_all(player, card, 1-player, &this_test, ACT_TAP);
			}
			else{
				if( valid_target(&td) ){
					tap_card(instance->targets[0].player, instance->targets[0].card);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return overload(player, card, event, MANACOST_XU(3, 1));
}

int card_chronic_flooding(int player, int card, event_t event){
/* CARD_ID_CHRONIC_FLOODING	12682
Chronic Flooding	|1|U
Enchantment - Aura
Enchant land
Whenever enchanted land becomes tapped, its controller puts the top three cards of his or her library into his or her graveyard. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_TAP_CARD && affect_me(p, c) ){
			mill(p, 3);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	return targeted_aura(player, card, event, &td, "TARGET_LAND");
}

int card_crosstown_courier(int player, int card, event_t event){
/* CARD_ID_CROSSTOWN_COURIER	12693
Crosstown Courier	|1|U
Creature - Vedalken 2/1
Whenever ~ deals combat damage to a player, that player puts that many cards from the top of his or her library into his or her graveyard.*/

	if (damage_dealt_by_me(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_REPORT_DAMAGE_DEALT | DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS)){
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = {BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player)};
		instance->targets[1].player = 0;
		int damages[2] = {BYTE0(instance->targets[16].player), BYTE1(instance->targets[16].player)};
		instance->targets[16].player = 0;
		APNAP(p, { if (times_damaged[p]){ mill(p, damages[p]); } });
	}
	return 0;
}

int card_cyclonic_rift(int player, int card, event_t event){
/* CARD_ID_CYCLONIC_RIFT	12695
Cyclonic Rift	|1|U
Instant
Return target nonland permanent you don't control to its owner's hand.
Overload |6|U (You may cast this spell for its overload cost. If you do, change its text by replacing all instances of "target" with "each.") */

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_LAND;
		td.allowed_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST ){
			if( generic_spell(player, card, event, 0, NULL, NULL, 0, NULL) ){
				if( instance->info_slot == 1 ){
					return 1;
				}
				return can_target(&td);
			}
		}

		if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			int result = 1;
			instance->number_of_targets = 0;
			if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
				result = pay_overload(player, card, MANACOST_XU(6, 1));
			}
			if( result == 1 ){
				new_pick_target(&td, "Select target nonland permanent your opponent controls.", 0, 1 | GS_LITERAL_PROMPT);
				if(	spell_fizzled == 1 ){
					return 0;
				}
			}
			instance->info_slot |= result;
		}

		if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot & 2 ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_LAND);
				this_test.type_flag = DOESNT_MATCH;
				new_manipulate_all(player, card, 1-player, &this_test, ACT_BOUNCE);
			}
			else{
				if( valid_target(&td) ){
					bounce_permanent(instance->targets[0].player, instance->targets[0].card);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return overload(player, card, event, MANACOST_XU(6, 1));
}

// dispel --> flash counter

int card_doorkeeper(int player, int card, event_t event){
/* CARD_ID_DOORKEEPER	12706
Doorkeeper	|1|U
Creature - Homunculus 0/4
Defender
|2|U, |T: Target player puts the top X cards of his or her library into his or her graveyard, where X is the number of creatures with defender you control.*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			mill(instance->targets[0].player, count_defenders(player));
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_XU(2, 1), 0, &td, "TARGET_PLAYER");
}

int card_downsize(int player, int card, event_t event){
/* CARD_ID_DOWNSIZE	12707
Downsize	|U
Instant
Target creature you don't control gets -4/-0 until end of turn.
Overload |2|U (You may cast this spell for its overload cost. If you do, change its text by replacing all instances of "target" with "each.") */

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST ){
			if( generic_spell(player, card, event, 0, NULL, NULL, 0, NULL) ){
				if( instance->info_slot == 1 ){
					return 1;
				}
				return can_target(&td);
			}
		}

		if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			int result = 1;
			instance->number_of_targets = 0;
			if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
				result = pay_overload(player, card, MANACOST_XU(2, 1));
			}
			if( result == 1 ){
				new_pick_target(&td, "Select target creature an opponent controls.", 0, 1 | GS_LITERAL_PROMPT);
				if(	spell_fizzled == 1 ){
					return 0;
				}
			}
			instance->info_slot |= result;
		}

		if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot & 2 ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				pump_creatures_until_eot(player, card, 1-player, 0, -4, 0, 0, 0, &this_test);
			}
			else{
				if( valid_target(&td) ){
					pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -4, 0);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return overload(player, card, event, MANACOST_XU(2, 1));
}

int card_faerie_impostor(int player, int card, event_t event){
  /* Faerie Impostor	|U
   * Creature - Faerie Rogue 2/1
   * Flying
   * When this creature enters, sacrifice it unless you return another creature you control to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		int sac = 1;
		state_untargettable(player, card, 1);
		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			sac = 0;
			instance->number_of_targets = 0;
		}
		if( sac == 1 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
		state_untargettable(player, card, 0);
	}
	return 0;
}

// hover barrier --> vanilla

int card_inaction_injunction(int player, int card, event_t event){
  /* Inaction Injunction	|1|U
   * Sorcery
   * Detain target creature an opponent controls. (Until your next turn, that creature can't attack or block and its activated abilities can't be activated.)
   * Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int mod = 0;
		if( (current_turn == player && current_phase < PHASE_DECLARE_BLOCKERS) ||
			(current_turn == 1-player && current_phase < PHASE_DECLARE_ATTACKERS) )
		{
			mod = 24;
		}
		ai_modifier += mod;
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature your opponent controls.", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier_for_detain(player, card, 0);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			detain(player, card, instance->targets[0].player, instance->targets[0].card);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature your opponent controls.", 1, NULL);
}

// isperia skywatch --> azorius arrester
static int jace_weaken_attackers(int player, int card, event_t event)
{
  if (current_turn != player)
  {
    store_attackers(player, card, event, DAT_TRACK, 1 - player, -1, NULL);
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      card_instance_t *instance = get_card_instance(player, card);
      unsigned char *attackers = (unsigned char *)&instance->targets[2].player;
      for (int amt = instance->targets[1].player - 1; amt >= 0; --amt)
        if (in_play(current_turn, attackers[amt]))
          pump_until_eot(player, card, current_turn, attackers[amt], -1, 0);
    }
  }
  if (event == EVENT_BEGIN_TURN && current_turn == player)
    kill_card(player, card, KILL_EXILE);
  return 0;
}

int card_jace_architect_of_thought(int player, int card, event_t event)
{

  /* Jace, Architect of Thought	|2|U|U
   * Planeswalker - Jace (4)
   * +1: Until your next turn, whenever a creature an opponent controls attacks, it gets -1/-0 until
   * end of turn. -2: Reveal the top three cards of your library. An opponent separates those cards
   * into two piles. Put one pile into your hand and the other on the bottom of your library in any
   * order. -8: For each player, search that player's library for a nonland card and exile it, then
   * that player shuffles his or her library. You may cast those cards without paying their mana
   * costs. */

  check_legend_rule(player, card, event);

  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION)
  {
    card_instance_t *instance = get_card_instance(player, card);

    enum
    {
      CHOICE_WEAKEN = 1,
      CHOICE_LFOF,
      CHOICE_SSPELL
    }

    choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, "Weaken attackers", 1,
                    count_subtype(1 - player, TYPE_CREATURE, -1) * 5, 1, "Little FoF", 1, 15, -2,
                    "Steal a Spell", 1, 25, -8);

    if (event == EVENT_CAN_ACTIVATE)
    {
      if (!choice)
        return 0; // else fall through to planeswalker()
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    { // event == EVENT_RESOLVE_ACTIVATION

      if (choice == CHOICE_WEAKEN)
        create_legacy_effect(instance->parent_controller, instance->parent_card,
                             &jace_weaken_attackers);

      else if (choice == CHOICE_LFOF)
        effect_fof(player, player, 3, TUTOR_BOTTOM_OF_DECK);

      else
      {
        test_definition_t this_test;
        new_default_test_definition(&this_test, TYPE_INTERRUPT | TYPE_LAND,
                                    "Select a nonland (non-interrupt) card to exile.");
        this_test.type_flag = DOESNT_MATCH;

        int i, selected[2];
        for (i = 0; i <= 1; i++)
        {
          selected[i] =
              new_global_tutor(player, i, TUTOR_FROM_DECK, TUTOR_RFG, AI_MAX_CMC, -1, &this_test);
        }
        while (selected[0] >= 0 || selected[1] >= 0)
        {
          int can_play_exiled_spell[2] = {
              (selected[0] >= 0 && can_legally_play_csvid(player, selected[0])),
              (selected[1] >= 0 && can_legally_play_csvid(player, selected[1]))};
          int spell_to_play = DIALOG(
              player, card, event, DLG_RANDOM, DLG_OMIT_ILLEGAL, DLG_NO_CANCEL, DLG_NO_STORAGE,
              selected[0] < 0 ? "" : get_card_name_by_id(selected[0]), can_play_exiled_spell[0], 3,
              selected[1] < 0 ? "" : get_card_name_by_id(selected[1]), can_play_exiled_spell[1], 3,
              "Done", (event != EVENT_CAN_CAST), 1);
          if (spell_to_play == 3)
          {
            break;
          }
          else
          {
            int csvid = selected[spell_to_play - 1];
            selected[spell_to_play - 1] = -1;
            play_card_in_exile_for_free(player, spell_to_play - 1, csvid);
          }
        }
      }
    }
  }

  if (event == EVENT_BEGIN_TURN && current_turn == player)
  {
    get_card_instance(player, card)->targets[1].player = 0;
  }

  return planeswalker(player, card, event, 4);
}

int card_mizzium_skin(int player, int card, event_t event){
  /* Mizzium Skin	|U
   * Instant
   * Target creature you control gets +0/+1 and gains hexproof until end of turn.
   * Overload |1|U (You may cast this spell for its overload cost. If you do, change "target" in its text to "each.") */

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST ){
			if( generic_spell(player, card, event, 0, NULL, NULL, 0, NULL) ){
				if( instance->info_slot == 1 ){
					return 1;
				}
				return can_target(&td);
			}
		}

		if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			int result = 1;
			instance->number_of_targets = 0;
			if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
				result = pay_overload(player, card, MANACOST_XU(1, 1));
			}
			if( result == 1 ){
				new_pick_target(&td, "Select target creature you control.", 0, 1 | GS_LITERAL_PROMPT);
				if(	spell_fizzled == 1 ){
					return 0;
				}
			}
			instance->info_slot |= result;
		}

		if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot & 2 ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				pump_creatures_until_eot(player, card, player, 0, 0, 1, 0, SP_KEYWORD_HEXPROOF, &this_test);
			}
			else{
				if( valid_target(&td) ){
					pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 1, 0, SP_KEYWORD_HEXPROOF);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return overload(player, card, event, MANACOST_XU(1, 1));
}

int card_psychic_spiral(int player, int card, event_t event){
  /* Psychic Spiral	|4|U
   * Instant
   * Shuffle all cards from your graveyard into your library. Target player mills that many cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 48;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int result = reshuffle_grave_into_deck(player, 0);
			mill(instance->targets[0].player, result);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_runewing(int player, int card, event_t event){
  /* Runewing	|3|U
   * Creature - Bird 2/2
   * Flying
   * When this creature dies, draw a card. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		draw_cards(player, 1);
	}
	return 0;
}

static int get_stored_cards_on_search_the_city(int player, int card, int exiled[]){
	card_instance_t *instance = get_card_instance(player, card);
	int i;
	for(i=0; i<20; i++){
		exiled[i] = -1;
	}
	int ec = 0;
	for(i=0; i<10; i++){
		int iid = instance->targets[i].player;
		if( iid == -1 ){
			break;
		}
		if( check_rfg(get_owner(player, card), cards_data[iid].id) ){
			exiled[ec] = iid;
			ec++;
		}
		instance->targets[i].player = -1;
		iid = instance->targets[i].card;
		if( iid == -1 ){
			break;
		}
		if( check_rfg(get_owner(player, card), cards_data[iid].id) ){
			exiled[ec] = iid;
			ec++;
		}
		instance->targets[i].card = -1;
	}
	return ec;
}

static void imprint_an_iid_on_search_the_city(int player, int card, int iid){
	card_instance_t *instance = get_card_instance(player, card);
	int pos = instance->info_slot;
	while( pos < 10 ){
			if( instance->targets[pos].player == -1 ){
				instance->targets[pos].player = iid;
				break;
			}
			if( instance->targets[pos].card == -1 ){
				instance->targets[pos].card = iid;
				break;
			}
			pos++;
	}
	instance->info_slot = pos;
}

int card_search_the_city(int player, int card, event_t event){
  /* Search the City |4|U
   * Enchantment
   * When Search the City enters the battlefield, exile the top five cards of your library.
   * Whenever you play a card with the same name as one of the exiled cards, you may put one of those cards with that name into its owner's hand.
   * 	Then if there are no cards exiled with Search the City, sacrifice it. If you do, take an extra turn after this one. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int amount = MIN(5, count_deck(player));
		if( amount ){
			show_deck(player, deck_ptr[player], amount, "Cards exiled by Search the City.", 0, gs_done_008b40e0 );
			while( amount ){
					int iid = deck_ptr[player][0];
					rfg_top_card_of_deck(player);
					imprint_an_iid_on_search_the_city(player, card, iid);
					amount--;

			}
		}
	}

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller ){
		if( trigger_cause_controller == player ){
			int exiled[20];
			int this_iid = get_original_internal_card_id(trigger_cause_controller, trigger_cause);
			int trig = 0;
			get_stored_cards_on_search_the_city(player, card, exiled);
			int i;
			for(i=0; i<20; i++){
				if( exiled[i] == -1 ){
					break;
				}
				if( this_iid == exiled[i] ){
					trig = 1;
					break;
				}
			}
			if( ! trig ){
				for(i=0; i<20; i++){
					if( exiled[i] == -1 ){
						break;
					}
					imprint_an_iid_on_search_the_city(player, card, exiled[i]);
				}
			}
			if( trig > 0 ){
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if( event == EVENT_RESOLVE_TRIGGER ){
						char msg[100];
						scnprintf(msg, 100, "Retrieve %s", cards_ptr[cards_data[this_iid].id]->name);
						int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
											msg, 1, 10,
											"Decline", 1, 1);
						if( choice == 1 ){
							for(i=0; i<20; i++){
								if( exiled[i] == this_iid ){
									add_card_to_hand(get_owner(player, card), this_iid);
									int k;
									for(k=0; k<19; k++){
										exiled[k] = exiled[k+1];
									}
									exiled[19] = -1;
									break;
								}
							}
						}
						for(i=0; i<20; i++){
							if( exiled[i] == -1 ){
								break;
							}
							imprint_an_iid_on_search_the_city(player, card, exiled[i]);
						}
						if( exiled[0] == -1 && ! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) ){
							kill_card(player, card, KILL_SACRIFICE);
							time_walk_effect(player, card);
						}
				}
			}
		}
	}

	if( ! IS_AI(player) ){
		if( event == EVENT_CAN_ACTIVATE ){
			card_instance_t *instance = get_card_instance(player, card);
			if( instance->targets[0].player > -1 ){
				return 1;
			}
		}
		if( event == EVENT_ACTIVATE ){
			int exiled[20];
			int amount = get_stored_cards_on_search_the_city(player, card, exiled);
			show_deck(player, exiled, amount, "Cards exiled by Search the City", 0, gs_done_008b40e0 );
			int i;
			for(i=0; i<amount; i++){
				imprint_an_iid_on_search_the_city(player, card, exiled[i]);
			}
		}
	}

	return global_enchantment(player, card, event);
}

// skyline predator --> a creature with flash

int card_soulsworn_spirit(int player, int card, event_t event){
  /* Soulsworn Spirit	|3|U
   * Creature - Spirit 2/1
   * This creature can't be blocked.
   * When this creature enters, detain target creature an opponent controls. (Until your next turn, that creature can't attack or block and its activated abilities can't be activated.) */

	unblockable(player, card, event);
	return card_azorius_arrester(player, card, event);
}

int card_sphinx_of_the_chimes(int player, int card, event_t event){
  /* Sphinx of the Chimes |4|U|U
   * Creature — Sphinx 5/6
   * Flying
   * Discard two nonland cards with the same name: Draw four cards. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			int i;
			int id_map[100];
			int id_map_count = 0;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_hand(player, i) && ! is_what(player, i, TYPE_LAND) ){
					int id = get_id(player, i);
					if( id_map_count > 0 ){
						int k;
						for(k=0; k<id_map_count; k++){
							if( id_map[k] == id ){
								return 1;
							}
						}
					}
					id_map[id_map_count] = id;
					id_map_count++;
				}
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			int cards_with_doubles[151];
			int cwdc = 0;
			int id_map[151];
			int id_map_count = 0;
			int i;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_hand(player, i) && ! is_what(player, i, TYPE_LAND) ){
					int iid = get_original_internal_card_id(player, i);
					if( id_map_count > 0 ){
						int k;
						for(k=0; k<id_map_count; k++){
							if( id_map[k] == iid ){
								int found = 0;
								int q;
								for(q=0; q<cwdc; q++){
									if( cards_with_doubles[q] == iid ){
										found = 1;
										break;
									}
								}
								if( ! found ){
									cards_with_doubles[cwdc] = iid;
									cwdc++;
								}
								break;
							}
						}
					}
					id_map[id_map_count] = iid;
					id_map_count++;
				}
			}
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card");
			int selected = select_card_from_zone(player, player, cards_with_doubles, cwdc, 0, AI_MIN_VALUE, -1, &this_test);
			if( selected == -1 ){
				spell_fizzled = 1;
				return 0;
			}
			int discarded = 0;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_hand(player, i) && ! is_what(player, i, TYPE_LAND) ){
					if( get_original_internal_card_id(player, i) == cards_with_doubles[selected] ){
						discard_card(player, selected);
						discarded++;
						if( discarded == 2 ){
							break;
						}
					}
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 4);
	}

	return 0;
}

int card_stealer_of_secrets(int player, int card, event_t event){
	/* Stealer of Secrets	|2|U
	 * Creature - Human Rogue 2/2
	 * Whenever ~ deals combat damage to a player, draw a card. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		draw_cards(player, amount);
	}

	return 0;
}

int card_tower_drake(int player, int card, event_t event)
{
  /* Tower Drake	|2|U
   * Creature - Drake 2/1
   * Flying
   * |W: This creature gets +0/+1 until end of turn. */

  return generic_shade_merge_pt(player, card, event, 0, MANACOST_W(1), 0, 1);
}

int card_voidwielder(int player, int card, event_t event){
  /* Voidwielder	|4|U
   * Creature - Human Wizard 1/4
   * When this creature enters, you may return target creature to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

// Black
int card_assassins_strike(int player, int card, event_t event){
  /* Assassin's Strike	|4|B|B
   * Sorcery
   * Destroy target creature. Its controller discards a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			discard(instance->targets[0].player, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_CREATURE", 1, NULL);
}

// catacomb slug --> vanilla

// daggerdrome imp --> child of night

int card_dark_revenant(int player, int card, event_t event){
  /* Dark Revenant	|3|B
   * Creature - Spirit 2/2
   * Flying
   * When this creature dies, put it on top of its owner's library. */

	int owner, position;
	if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) && find_in_owners_graveyard(player, card, &owner, &position))
	{
		int iid = get_grave(owner)[position];
		obliterate_card_in_grave(owner, position);
		raw_put_iid_on_top_of_deck(owner, iid);
	}

	return 0;
}

int card_dark_reveler(int player, int card, event_t event){
  /* Dead Reveler	|2|B
   * Creature - Zombie 2/3
   * Unleash (You may have this creature enter with a +1/+1 counter on it. It can't block as long as it has a +1/+1 counter on it.) */

	// dead reveler
	return unleash(player, card, event);
}

int card_desecration_demon(int player, int card, event_t event){
  /* Desecration Demon	|2|B|B
   * Creature - Demon 6/6
   * Flying
   * At the beginning of each combat, any opponent may sacrifice a creature of their choice. If a player does, tap this creature and put a +1/+1 counter on it. */

	if ( beginning_of_combat(player, card, event, ANYBODY) ){
		if (can_sacrifice(player, 1-player, 1, TYPE_CREATURE, 0)){
			int ai_choice = 1;
			if (1-player == AI
				&& !is_tapped(player, card)
				&& current_turn == player
				&& !is_sick(player, card)
				&& life[1-player]-get_attack_power(player, card) < 6){
				ai_choice = 0;
			}

			int choice = do_dialog(1-player, player, card, -1, -1, " Sac to tap Desecration Demon\n Pass", ai_choice);
			if (choice == 0){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
				int sac = new_sacrifice(player, card, 1-current_turn, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
				if (!sac){
					return 0;
				}
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				tap_card(player, card);
				add_counters(player, card, player, card, COUNTER_P1_P1, 1);
			}
		}
	}

	return 0;
}

int card_destroy_the_evidence(int player, int card, event_t event){
  /* Destroy the Evidence	|4|B
   * Sorcery
   * Destroy target land. Its controller reveals cards from the top of their library until they reveal a land card, then puts those cards into their graveyard. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			int *deck = deck_ptr[instance->targets[0].player];
			int count = 0;
			while( deck[count] != -1 && ! is_what(-1, deck[count], TYPE_LAND) ){
				count++;
			}
			show_deck(0, deck, count+1, "Cards revealed by Destroy the Evidence", 0, gs_done_008b40e0 );
			show_deck(1, deck, count+1, "Cards revealed by Destroy the Evidence", 0, gs_done_008b40e0 );
			mill(instance->targets[0].player, count+1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
}

int card_deviant_glee(int player, int card, event_t event){
  /* Deviant Glee	|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+1 and has "|R: This creature gains trample until end of turn." */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_CAN_ACTIVATE ){
			return generic_activated_ability(p, c, event, 0, MANACOST_R(1), 0, NULL, NULL);
		}
		if( event == EVENT_ACTIVATE ){
			return generic_activated_ability(p, c, event, 0, MANACOST_R(1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, p, c, 0, 0, KEYWORD_TRAMPLE, 0);
	}

	return generic_aura(player, card, event, player, 2, 1, 0, 0, 0, 0, 0);
}

int card_drainpipe_vermin(int player, int card, event_t event)
{
  /* Drainpipe Vermin |B
   * Creature — Rat 1/1
   * When Drainpipe Vermin dies, you may pay {B}. If you do, target player discards a card. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		if( can_target(&td) && has_mana(player, COLOR_BLACK, 1) &&
			charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_BLACK, 1) )
		{
			if( pick_target(&td, "TARGET_PLAYER") ){
				discard(get_card_instance(player, card)->targets[0].player, 0, player);
			}
		}
	}

	return 0;
}

static int grave_betrayal_reanimation_legacy(int player, int card, event_t event){

	if( eot_trigger(player, card, event) ){
		card_instance_t *instance = get_card_instance(player, card);
		int owner = instance->targets[0].player;
		int iid = instance->targets[0].card;
 		int pos = -1;
		int c;
		for( c = count_graveyard(owner)-1; c > -1; c-- ){
			if( get_grave(owner)[c] == iid ){
				pos = c;
				break;
			}
		}
		if( pos > -1 ){
			reanimate_permanent_with_counter_and_effect(player, card, owner, pos, REANIMATE_ADD_BLACK_ZOMBIE, COUNTER_P1_P1, NULL);
		}
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

static int grave_betrayal_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_GRAVE_BETRAYAL)
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
        count_for_gfp_ability_and_store_values(player, card, event, 1 - player, TYPE_CREATURE, NULL,
                                               GFPC_TRACK_DEAD_CREATURES | GFPC_STORE_OWNER, 0);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int amount = player == 0 ? BYTE1(result) : BYTE0(result);
    int i;
    for (i = 0; i < amount; i++)
    {
      int owner = instance->targets[i].player;
      int iid = instance->targets[i].card;
      instance->targets[i].player = instance->targets[i].card = -1;
      int legacy = create_legacy_effect(player, card, &grave_betrayal_reanimation_legacy);
      card_instance_t *leg = get_card_instance(player, legacy);
      leg->targets[0].player = owner;
      leg->targets[0].card = iid;
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_grave_betrayal(int player, int card, event_t event)
{
  /* Grave Betrayal |5|B|B
   *   Enchantment
   *   Whenever a creature you don't control dies, return it to the battlefield under your control with
   *   an additional +1/+1 counter on it at the beginning of the next end step. That creature is a black
   *   Zombie in addition to its other colors and types. */

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    int found = 0;
    int c;
    for (c = 0; c < active_cards_count[player]; c++)
    {
      if (in_play(player, c) && is_what(player, c, TYPE_EFFECT))
      {
        card_instance_t *inst = get_card_instance(player, c);
        if (inst->info_slot == (int)grave_betrayal_effect)
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
      int legacy =
          create_targetted_legacy_effect(player, card, &grave_betrayal_effect, player, card);
      card_instance_t *inst = get_card_instance(player, legacy);
      inst->targets[0].player = player;
      inst->targets[0].card = card;
      inst->number_of_targets = 1;
      add_status(player, legacy, STATUS_INVISIBLE_FX);
    }
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    count_for_gfp_ability_and_store_values(player, card, event, 1 - player, TYPE_CREATURE, NULL,
                                           GFPC_TRACK_DEAD_CREATURES | GFPC_STORE_OWNER, 0);
  }

  if (get_card_instance(player, card)->kill_code <= 0)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (result)
    {
      int amount = player == 0 ? BYTE1(result) : BYTE0(result);
      int i;
      for (i = 0; i < amount; i++)
      {
        int owner = instance->targets[i].player;
        int iid = instance->targets[i].card;
        instance->targets[i].player = instance->targets[i].card = -1;
        int legacy = create_legacy_effect(player, card, &grave_betrayal_reanimation_legacy);
        card_instance_t *leg = get_card_instance(player, legacy);
        leg->targets[0].player = owner;
        leg->targets[0].card = iid;
      }
    }
  }

  return global_enchantment(player, card, event);
}

int card_launch_party(int player, int card, event_t event){
  /* Launch Party	|3|B
   * Instant
   * As an additional cost to cast this spell, sacrifice a creature.
   * Destroy target creature. Its controller loses 2 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			lose_life(instance->targets[0].player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_SAC_CREATURE_AS_COST, &td, "TARGET_CREATURE", 1, NULL);
}

int card_grim_roustabout(int player, int card, event_t event){
  /* Grim Roustabout	|1|B
   * Creature - Skeleton Warrior 1/1
   * Unleash (You may have this creature enter with a +1/+1 counter on it. It can't block as long as it has a +1/+1 counter on it.)
   * |1|B: Regenerate this creature. */

	unleash(player, card, event);
	return regeneration(player, card, event, MANACOST_XB(1, 1));
}

int card_necropolis_regent(int player, int card, event_t event)
{
  /* Necropolis Regent	|3|B|B|B
   * Creature - Vampire 6/5
   * Flying
   * Whenever a creature you control deals combat damage to a player, put that many +1/+1 counters on it. */

  card_instance_t *instance = get_card_instance(player, card);
  card_instance_t *damage = combat_damage_being_dealt(event);
  uint8_t *records = (uint8_t *)instance + offsetof(card_instance_t, targets);
  if (damage && damage->damage_source_player == player && (damage->targets[3].player & 2) &&
      damage->damage_target_card == -1 &&
      !check_special_flags(player, damage->damage_source_card, 0x40) && instance->info_slot < 26)
  {
    // Three bytes per hit: source player, source card, saturated damage amount.
    // Recompute the offset for each write, including the release's negative-count behavior.
    records[instance->info_slot * 3] = damage->damage_source_player;
    records[instance->info_slot * 3 + 1] = (uint8_t)damage->damage_source_card;
    int amount = damage->info_slot;
    if (amount > 255)
      amount = 255;
    records[instance->info_slot * 3 + 2] = (uint8_t)amount;
    ++instance->info_slot;
  }
  if (trigger_condition == TRIGGER_DEAL_DAMAGE && card == affected_card &&
      player == affected_card_controller && reason_for_trigger_controller == player &&
      instance->info_slot > 0)
  {
    if (event == EVENT_TRIGGER)
      event_result |= RESOLVE_TRIGGER_MANDATORY;
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      uint8_t *record = records;
      int resolved = 0;
      do
      {
        ++resolved;
        add_counters(player, card, record[0], record[1], COUNTER_P1_P1, record[2]);
        record += 3;
      } while (resolved < instance->info_slot);
      instance->info_slot = 0;
    }
  }
  return 0;
}

int card_ogre_jailbreaker(int player, int card, event_t event)
{
  /* Ogre Jailbreaker	|3|B
   * Creature - Ogre Rogue 4/4
   * Defender
   * This creature can attack as though it didn't have defender as long as you control a Gate. */

	if (event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) &&
		check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_GATE) )
		get_card_instance(player, card)->token_status |= STATUS_WALL_CAN_ATTACK;

	return 0;
}

int card_pack_rat(int player, int card, event_t event)
{
  /* Pack Rat	|1|B
   * Creature - Rat 100/100
   * ~'s power and toughness are each equal to the number of Rats you control.
   * |2|B, Discard a card: Put a token onto the battlefield that's a copy of ~. */

  int result;
  card_instance_t *instance;
  card_instance_t *parent;
  token_generation_t token;
  if (((uint32_t)(event - EVENT_POWER) < 2) && (-1 < player))
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = count_subtype(player, 0x47, 0x9a);
      set_characteristic_pt(result);
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    parent = get_card_instance(instance->parent_controller, instance->parent_card);
    if (parent->internal_card_id < 0)
    {
      default_token_definition(
          player, card, (unsigned int)cards_data[instance->backup_internal_card_id].id, &token);
      token.no_sleight = '\x01';
    }
    else
    {
      copy_token_definition(player, card, &token, instance->parent_controller, instance->parent_card);
    }
    generate_token(&token);
  }
  return generic_activated_ability(player, card, event, 0x20, 2, 1, 0, 0, 0, 0, 0, NULL, NULL);
}

int card_perilous_shadow(int player, int card, event_t event){
  /* Perilous Shadow	|2|B|B
   * Creature - Insect Shade 0/4
   * |1|B: This creature gets +2/+2 until end of turn. */

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_XB(1, 1), 2, 2);
}

int card_sewer_shambler(int player, int card, event_t event){
  /* Sewer Shambler	|2|B
   * Creature - Zombie 2/1
   * Swampwalk (This creature can't be blocked as long as defending player controls a Swamp.)
   * Scavenge |2|B (|2|B, Exile this card from your graveyard: Put a number of +1/+1 counters equal to this card's power on target creature. Scavenge only as a sorcery.) */

	return scavenge(player, card, event, MANACOST_XB(2, 1));
}

int card_shrieking_affliction(int player, int card, event_t event){
  /* Shrieking Affliction	|B
   * Enchantment
   * At the beginning of each opponent's upkeep, if that player has one or fewer cards in hand, they lose 3 life. */

	upkeep_trigger_ability(player, card, event, 1-player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( hand_count[1-player] <= 1 ){
			lose_life(1-player, 3);
		}
	}

	return global_enchantment(player, card, event);
}

int card_stab_wound(int player, int card, event_t event){
  /* Stab Wound	|2|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets -2/-2.
   * At the beginning of the upkeep of enchanted creature's controller, that player loses 2 life. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		upkeep_trigger_ability(player, card, event, instance->damage_target_player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			lose_life(instance->damage_target_player, 2);
		}
	}

	return generic_aura(player, card, event, player, -2, -2, 0, 0, 0, 0, 0);
}

int card_tavern_swindler(int player, int card, event_t event){
  /* Tavern Swindler	|1|B
   * Creature - Human Rogue 2/2
   * |T, Pay 3 life: Flip a coin. If you win the flip, you gain 6 life. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int result = flip_a_coin(player, card);
		if( result ){
			gain_life(player, 6);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 3, NULL, NULL);
}

int card_terrus_wurm(int player, int card, event_t event){
  /* Terrus Wurm	|6|B
   * Creature - Zombie Wurm 5/5
   * Scavenge |6|B (|6|B, Exile this card from your graveyard: Put a number of +1/+1 counters equal to this card's power on target creature. Scavenge only as a sorcery.) */

	return scavenge(player, card, event, MANACOST_XB(6, 1));
}

int card_thrill_kill_assassin(int player, int card, event_t event){
  /* Thrill-Kill Assassin	|1|B
   * Creature - Human Assassin 1/2
   * Deathtouch
   * Unleash (You may have this creature enter with a +1/+1 counter on it. It can't block as long as it has a +1/+1 counter on it.) */

	deathtouch(player, card, event);
	return unleash(player, card, event);
}

const char* target_is_monocolored(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	if (num_bits_set(get_color(player, card) & COLOR_TEST_ANY_COLORED) == 1){
		return NULL;
	} else {
		return "color";
	}
}

int card_ultimate_price(int player, int card, event_t event){
  /* Ultimate Price	|1|B	=>return_to_ravnica.c:card_ultimate_price
   * Instant
   * Destroy target monocolored creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.extra = target_predicate_extra(target_is_monocolored);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target monocolored creature.", 1, NULL);
}

int card_underworld_connections(int player, int card, event_t event){
  /* Underworld Connections	|1|B|B
   * Enchantment - Aura
   * Enchant land
   * Enchanted land has "|T, Pay 1 life: Draw a card." */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return granted_generic_activated_ability(player, card, t_player, t_card, event, GAA_UNTAPPED, MANACOST0, 1, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;
	td.allowed_controller = player;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target land you control.");
}

int card_zanikev_locust(int player, int card, event_t event){
/* CARD_ID_ZANIKEV_LOCUST	12874
Zanikev Locust	|5|B
Creature - Insect 3/3
Flying
Scavenge |2|B|B (|2|B|B, Exile this card from your graveyard: Put a number of +1/+1 counters equal to this card's power on target creature. Scavenge only as a sorcery.) */

	return scavenge(player, card, event, MANACOST_XB(2, 2));
}

// Red

// Annihilating Fire => Yamabushi's Flame

// ash zealot --> raging goblin

int card_batterhorn(int player, int card, event_t event){
/* CARD_ID_BATTERHORN	12666
Batterhorn	|4|R
Creature - Beast 4/3
When ~ enters the battlefield, you may destroy target artifact. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_ARTIFACT", 0, 0)){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_bellows_lizard(int player, int card, event_t event){
/* CARD_ID_BELLOWS_LIZARD	12668 => card_wildfire_emissary 200bb0a
Bellows Lizard	|R
Creature - Lizard 1/1
|1|R: ~ gets +1/+0 until end of turn. */

	return generic_shade(player, card, event, 0, MANACOST_XR(1, 1), 1, 0, 0, 0);
}

// bloodfray giant --> dark reveler

int card_chaos_imps(int player, int card, event_t event){
/* Chaos Imps	|4|R|R
Creature - Imp 6/5
Flying
Unleash (You may have this creature enter the battlefield with a +1/+1 counter on it. It can't block as long as it has a +1/+1 counter on it.)
~ has trample as long as it has a +1/+1 counter on it. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( count_1_1_counters(player, card) ){
			event_result |= KEYWORD_TRAMPLE;
		}
	}

	return unleash(player, card, event);
}

// cobblebrute --> vanilla

int card_dynacharge(int player, int card, event_t event){
/* CARD_ID_DYNACHARGE	12715
Dynacharge	|R
Instant
Target creature you control gets +2/+0 until end of turn.
Overload |2|R (You may cast this spell for its overload cost. If you do, change its text by replacing all instances of "target" with "each.") */

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST ){
			if( generic_spell(player, card, event, 0, NULL, NULL, 0, NULL) ){
				if( instance->info_slot == 1 ){
					return 1;
				}
				return can_target(&td);
			}
		}

		if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			int result = 1;
			instance->number_of_targets = 0;
			if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
				result = pay_overload(player, card, MANACOST_XR(2, 1));
			}
			if( result == 1 ){
				new_pick_target(&td, "Select target creature you control.", 0, 1 | GS_LITERAL_PROMPT);
				if(	spell_fizzled == 1 ){
					return 0;
				}
			}
			instance->info_slot |= result;
		}

		if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot & 2 ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				pump_creatures_until_eot(player, card, player, 0, 2, 0, 0, 0, &this_test);
			}
			else{
				if( valid_target(&td) ){
					pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 0, 0, 0);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return overload(player, card, event, MANACOST_XR(2, 1));
}

int card_electrickery(int player, int card, event_t event){
  /* Electrickery	|R
   * Instant
   * ~ deals 1 damage to target creature you don't control.
   * Overload |1|R (You may cast this spell for its overload cost. If you do, change "target" in its text to "each.") */

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST ){
			if( generic_spell(player, card, event, 0, NULL, NULL, 0, NULL) ){
				if( instance->info_slot == 1 ){
					return 1;
				}
				return can_target(&td);
			}
		}

		if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			int result = 1;
			instance->number_of_targets = 0;
			if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
				result = pay_overload(player, card, MANACOST_XR(1, 1));
			}
			if( result == 1 ){
				new_pick_target(&td, "Select target creature your opponent controls.", 0, 1 | GS_LITERAL_PROMPT);
				if(	spell_fizzled == 1 ){
					return 0;
				}
			}
			instance->info_slot |= result;
		}

		if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot & 2 ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				new_damage_all(player, card, 1-player, 1, NDA_ALL_CREATURES, &this_test);
			}
			else{
				if( valid_target(&td) ){
					damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return overload(player, card, event, MANACOST_XR(1, 1));
}

// explosive impact --> thunderous wrath

int card_goblin_rally(int player, int card, event_t event){
	/* Goblin Rally	|3|R|R
	 * Sorcery
	 * Put four 1/1 |Sred Goblin creature tokens onto the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_GOBLIN, 4);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// gore-house chainwalker --> dark reveler

int card_guild_feud(int player, int card, event_t event){
/* Guild Feud	|5|R
 * Enchantment
 * At the beginning of your upkeep, target opponent reveals the top three cards of his or her library, may put a creature card from
 * 		among them onto the battlefield, then puts the rest into his or her graveyard.
 * You do the same with the top three cards of your library. If two creatures are put onto the battlefield this way, those creatures
 *		fight each other. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;

		if( would_validate_arbitrary_target(&td, 1-player, -1) ){
			int monster[2] = {-1, -1};
			int i;
			for(i=0; i<2; i++){
				int p = i == 0 ? 1-player : player;
				int amount = MIN(3, count_deck(p));
				if( amount ){
					test_definition_t this_test;
					new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
					this_test.create_minideck = amount;
					this_test.no_shuffle = 1;
					monster[p] = new_global_tutor(p, p, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
					amount--;
					if( amount ){
						mill(p, amount);
					}
				}
			}
			if( monster[0] >= 0 && monster[1] >= 0 ){
				fight(player, monster[player], 1-player, monster[1-player]);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_guttersnipe(int player, int card, event_t event){
  /* Guttersnipe	|2|R
   * Creature - Goblin Shaman 2/2
   * Whenever you cast an instant or sorcery spell, this creature deals 2 damage to each opponent. */

	sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_SPELL, NULL, player, card);
	if (event == EVENT_SPA){
		damage_player(1-player, 2, player, card);
	}
	return 0;
}

int card_lobber_crew(int player, int card, event_t event){
  /* Lobber Crew	|2|R
   * Creature - Goblin Warrior 0/4
   * Defender
   * |T: This creature deals 1 damage to each opponent.
   * Whenever you cast a multicolored spell, untap this creature. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color_flag = F3_MULTICOLORED;
		sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
	}

	if (event == EVENT_SPA){ untap_card(player, card); }
	else if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		damage_player(1-player, 1, instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

// minotaur aggressor --> raging goblin

int card_mizzium_mortars(int player, int card, event_t event){
  /* Mizzium Mortars	|1|R
   * Sorcery
   * ~ deals 4 damage to target creature you don't control.
   * Overload |3|R|R|R (You may cast this spell for its overload cost. If you do, change "target" in its text to "each.") */

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST ){
			if( generic_spell(player, card, event, 0, NULL, NULL, 0, NULL) ){
				if( instance->info_slot == 1 ){
					return 1;
				}
				return can_target(&td);
			}
		}

		if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			int result = 1;
			instance->number_of_targets = 0;
			if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
				result = pay_overload(player, card, MANACOST_XR(3, 3));
			}
			if( result == 1 ){
				new_pick_target(&td, "Select target creature your opponent controls.", 0, 1 | GS_LITERAL_PROMPT);
				if(	spell_fizzled == 1 ){
					return 0;
				}
			}
			instance->info_slot |= result;
		}

		if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot & 2 ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				new_damage_all(player, card, 1-player, 4, NDA_ALL_CREATURES, &this_test);
			}
			else{
				if( valid_target(&td) ){
					damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return overload(player, card, event, MANACOST_XR(3, 3));
}

int card_pursuit_of_flight(int player, int card, event_t event){
  /* Pursuit of Flight	|1|R
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+2 and has "|U: This creature gains flying until end of turn." */

	if (!IS_GAA_EVENT(event)){
		return generic_aura(player, card, event, player, 2, 2, 0, 0, 0, 0, 0);
	}
	if (in_play(player, card)){
		card_instance_t* instance = get_card_instance(player, card);
		if (instance->damage_target_player != -1){
			instance = get_card_instance(player, card);
			if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE){
				return granted_generic_activated_ability(player, card, instance->damage_target_player, instance->damage_target_card, event, 0, MANACOST_U(1), 0, NULL, NULL);
			}
		}
	}
	if (event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);
		int p = instance->parent_controller, c = instance->parent_card;
		card_instance_t* parent = get_card_instance(p, c);
		pump_ability_until_eot_no_repeat(p, c, parent->damage_target_player, parent->damage_target_card, KEYWORD_FLYING, 0);
	}
	return 0;
}

int card_pyroconvergence(int player, int card, event_t event){
	/* Pyroconvergence	|4|R	0x2007640
	 * Enchantment
	 * Whenever you cast a multicolored spell, ~ deals 2 damage to any target. */

	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color_flag = F3_MULTICOLORED;
		sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;
		card_instance_t* instance = get_card_instance(player, card);
		if (can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0)){ damage_target0(player, card, 2); }
		instance->number_of_targets = 0;
	}
	return global_enchantment(player, card, event);
}

int card_racecourse_fury(int player, int card, event_t event){
  /* Racecourse Fury |R
   * 	Enchantment — Aura
   * 	Enchant land
   * 	Enchanted land has "{T}: Target creature gains haste until end of turn." */

	if( ! is_humiliated(player, card, AL_STATIC) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance( player, card );
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		target_definition_t td1;
		default_target_definition(p, c, &td1, TYPE_CREATURE);
		td1.preferred_controller = player;

		if( event == EVENT_CAN_ACTIVATE ){
			return granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0,
													&td1, "TARGET_CREATURE");
		}

		if( event == EVENT_ACTIVATE ){
			instance->number_of_targets = 0;
			if( pick_target(&td1, "TARGET_CREATURE") ){
				ai_modifier_for_haste_granting_effects(player, card);
				return granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( validate_target(p, c, &td1, 0) ){
				card_instance_t *land = get_card_instance( p, c );
				pump_ability_until_eot(instance->parent_controller, instance->parent_card, land->targets[0].player, land->targets[0].card,
											0, 0, 0, SP_KEYWORD_HASTE);
			}
		}
	}

	if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.preferred_controller = player;
		td.allowed_controller = player;

		return targeted_aura_custom_prompt(player, card, event, &td, "Select target land you control.");
	}

	return 0;
}

// splatter thug --> dark reveler

int card_street_spasm(int player, int card, event_t event){
  /* Street Spasm |X|R
   * 	  Instant
   * 	  Street Spasm deals X damage to target creature without flying you don't control.
   * 	  Overload {X}{X}{R}{R} (You may cast this spell for its overload cost. If you do, change its text by replacing all instances of "target" with "each.") */

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.illegal_abilities |= KEYWORD_FLYING;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST ){
			if( generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL) ){
				if( instance->info_slot == 1 ){
					return 1;
				}
				return can_target(&td);
			}
		}

		if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			int result = 1;
			instance->number_of_targets = instance->targets[1].player = 0;
			if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
				result = pay_overload(player, card, MANACOST_XR(-2, 2));
			}
			if( result == 1 ){
				charge_mana(player, COLOR_COLORLESS, -1);
				if( spell_fizzled != 1 && new_pick_target(&td, "Select target creature without flying your opponent controls.", 0, 1 | GS_LITERAL_PROMPT) ){
					instance->targets[1].player = x_value;
				}
				else{
					return 0;
				}
			}
			instance->info_slot |= result;
		}

		if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot & 2 ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				this_test.keyword = KEYWORD_FLYING;
				this_test.keyword_flag = 1;
				new_damage_all(player, card, 1-player, instance->targets[1].player/2, 0, &this_test);
			}
			else{
				if( valid_target(&td) ){
					damage_target0(player, card, instance->targets[1].player);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return overload(player, card, event, MANACOST_XR(2, 2));
}

int card_survey_the_wreckage(int player, int card, event_t event){
	/* Survey the Wreckage	|4|R
	 * Sorcery
	 * Destroy target land. Put a 1/1 |Sred Goblin creature token onto the battlefield. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			generate_token_by_id(player, card, CARD_ID_GOBLIN);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
}

// tenement crasher --> raging goblin

int card_utvara_hellkite(int player, int card, event_t event)
{
  /* Utvara Hellkite	|6|R|R
   * Creature - Dragon 6/6
   * Flying
   * Whenever a Dragon you control attacks, put a 6/6 |Sred Dragon creature token with flying onto the battlefield. */

	if (event == EVENT_DECLARE_ATTACKERS){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_DRAGON;
		store_attackers(player, card, event, DAT_TRACK, player, -1, &test);
	}
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_DRAGON, &token);
		token.pow = token.tou = 6;
		generate_token(&token);
	}
	return 0;
}

int card_vandalblast(int player, int card, event_t event){
  /* Vandalblast	|R
   * Sorcery
   * Destroy target artifact you don't control.
   * Overload |4|R (You may cast this spell for its overload cost. If you do, change "target" in its text to "each.") */

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.allowed_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST ){
			if( generic_spell(player, card, event, 0, NULL, NULL, 0, NULL) ){
				if( instance->info_slot == 1 ){
					return 1;
				}
				return can_target(&td);
			}
		}

		if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			int result = 1;
			instance->number_of_targets = 0;
			if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
				result = pay_overload(player, card, MANACOST_XR(4, 1));
			}
			if( result == 1 ){
				new_pick_target(&td, "Select target artifact your opponent controls.", 0, 1 | GS_LITERAL_PROMPT);
				if(	spell_fizzled == 1 ){
					return 0;
				}
			}
			instance->info_slot |= result;
		}

		if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot & 2 ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_ARTIFACT);
				new_manipulate_all(player, card, 1-player, &this_test, KILL_DESTROY);
			}
			else{
				if( valid_target(&td) ){
					kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return overload(player, card, event, MANACOST_XR(4, 1));
}

int card_viashino_racketeer(int player, int card, event_t event){
  /* Viashino Racketeer	|2|R
   * Creature - Lizard Rogue 2/1
   * When this creature enters, you may discard a card. If you do, draw a card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		if( hand_count[player] > 0 ){
			char msg[100] = "Select a card to discard.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, msg);
			int result = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
			if( result != -1 ){
				discard_card(player, result);
				draw_cards(player, 1);
			}
		}
	}
	return 0;
}

// Green

int card_aerial_predation(int player, int card, event_t event){
  /* Aerial Predation	|2|G
   * Instant
   * Destroy target creature with flying. You gain 2 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			gain_life(player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature with flying.", 1, NULL);
}

// archweaver --> vanilla

int card_axebane_guardian(int player, int card, event_t event){
  /* Axebane Guardian	|2|G
   * Creature - Human Druid 0/3
   * Defender
   * |T: Add X mana in any combination of colors, where X is the number of creatures you control with defender. */

	if( event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	if(event == EVENT_ACTIVATE ){
		int amount = count_defenders(player);
		produce_mana_tapped_any_combination_of_colors(player, card, COLOR_TEST_ANY_COLORED, amount, NULL);
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( ! is_tapped(player, card) && ! is_sick(player, card) && can_produce_mana(player, card) ){
			declare_mana_available_any_combination_of_colors(player, COLOR_TEST_ANY_COLORED, count_defenders(player));
		}
	}

	return 0;
}

// axebane stag --> vanilla

// brushstrider --> serra angel

int card_centaurs_herald(int player, int card, event_t event){
	/* Centaur's Herald	|G
	 * Creature - Elf Scout 0/1
	 * |2|G, Sacrifice ~: Put a 3/3 |Sgreen Centaur creature token onto the battlefield. */

	if( (event == EVENT_ATTACK_RATING || event == EVENT_BLOCK_RATING) &&
		generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME, MANACOST_XG(2, 1), 0, NULL, NULL) )
	{
		if (event == EVENT_ATTACK_RATING){
			ai_defensive_modifier -= 24;
		}
		else {	// EVENT_BLOCK_RATING
			ai_defensive_modifier += 24;
		}
	}

	if( event == EVENT_ACTIVATE ){
		card_instance_t *instance = get_card_instance(player, card);
		if( check_state(player, card, STATE_BLOCKING) && get_attack_power(1-player, instance->blocking) >= get_toughness(player, card) ){
			ai_modifier += 96;
		}
		if( check_state(player, card, STATE_ATTACKING) && current_phase == PHASE_AFTER_BLOCKING ){
			int total_pow = 0;
			int c;
			for(c=0; c<active_cards_count[1-player]; c++){
				if( in_play(1-player, c) && is_what(1-player, c, TYPE_CREATURE) && get_card_instance(1-player, c)->blocking == card ){
					total_pow += get_attack_power(1-player, c);
				}
			}
			if( total_pow >= get_toughness(player, card) ){
				ai_modifier += 96;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_CENTAUR);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XG(2, 1), 0, NULL, NULL);
}

int card_chorus_of_might(int player, int card, event_t event)
{
  /* Chorus of Might	|3|G
   * Instant
   * Until end of turn, target creature gets +1/+1 for each creature you control and gains trample. */
  int amt = (event == EVENT_CHECK_PUMP || event == EVENT_RESOLVE_SPELL) ? count_subtype(player, TYPE_CREATURE, -1) : 0;
  return vanilla_instant_pump(player, card, event, ANYBODY, player, amt, amt, KEYWORD_TRAMPLE, 0);
}

int card_deadbridge_goliath(int player, int card, event_t event){
  /* Deadbridge Goliath	|2|G|G
   * Creature - Insect 5/5
   * Scavenge |4|G|G (|4|G|G, Exile this card from your graveyard: Put a number of +1/+1 counters equal to this card's power on target creature. Scavenge only as a sorcery.) */

	return scavenge(player, card, event, MANACOST_XG(4, 2));
}

static int deaths_presence_effect(int player, int card, event_t event)
{
  int pow;
  card_instance_t *instance;
  int stored_index_or_result;
  card_instance_t *dying_permanent;
  int stored_power;
  int stored_count_or_slot;
  target_definition_t *creature_target_definition;
  int i;
  int stored[18];
  target_definition_t td;
  char msg[100];

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (stored_index_or_result =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           stored_index_or_result != 0x319d))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  instance = get_card_instance(player, card);
  if ((-1 < instance->damage_target_player) &&
      (stored_index_or_result = effect_follows_control_of_attachment(player, card, event),
       stored_index_or_result != 0))
  {
    return 0;
  }
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = get_card_instance(player, card);
    dying_permanent = in_play(affected_card_controller, affected_card);
    if ((dying_permanent != NULL) &&
        (dying_permanent = get_card_instance(affected_card_controller, affected_card),
         dying_permanent->kill_code != '\0'))
    {
      if ((-1 < instance->damage_target_player) &&
          ((instance->damage_target_card == affected_card &&
            (instance->damage_target_player == affected_card_controller))))
      {
        instance->damage_target_card = -1;
        instance->damage_target_player = -1;
        remove_status(player, card, 0x10000);
        if (instance->targets[0xb].player < 0)
        {
          instance->targets[0xb].player = 0;
        }
        SET_BYTE1(instance->targets[0xb].player) = 1;
      }
      if ((((player == affected_card_controller) &&
            (stored_index_or_result = is_what(player, affected_card, 2),
             stored_index_or_result != 0)) &&
           (stored_index_or_result = is_this_dying(affected_card_controller, affected_card),
            stored_index_or_result != 0)) &&
          (stored_index_or_result =
               check_for_death_trigger_removal(affected_card_controller, affected_card),
           stored_index_or_result == 0))
      {
        i = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
        do
        {
          pow = get_power(affected_card_controller, affected_card);
          if (pow != 0)
          {
            if (instance->targets[0xb].player < 0)
            {
              instance->targets[0xb].player = 0;
            }
            *(uint8_t *)&instance->targets[0xb].player = 1;
            stored_count_or_slot = 1;
            do
            {
              if (instance->targets[stored_count_or_slot].player == -1)
              {
                instance->targets[stored_count_or_slot].player = pow;
                break;
              }
              if (instance->targets[stored_count_or_slot].card == -1)
              {
                instance->targets[stored_count_or_slot].card = pow;
                break;
              }
              stored_count_or_slot = stored_count_or_slot + 1;
            } while (stored_count_or_slot != 10);
          }
          i = i + -1;
        } while (i != 0);
      }
    }
  }
  stored_index_or_result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (stored_index_or_result == 0 && event == EVENT_GRAVEYARD_FROM_PLAY)
    return 0;
  if (stored_index_or_result != 0)
  {
    stored_index_or_result = check_status(player, card, 0x10000);
    if (stored_index_or_result == 0)
    {
      instance = get_card_instance(player, card);
      stored_index_or_result = 0;
      do
      {
        stored_power = (&instance->targets[1].player)[stored_index_or_result];
        stored_count_or_slot = stored_index_or_result;
        if (stored_power == -1)
          break;
        stored[stored_index_or_result] = stored_power;
        stored_power = (&instance->targets[1].card)[stored_index_or_result];
        stored_count_or_slot = stored_index_or_result + 1;
        (&instance->targets[1].player)[stored_index_or_result] = -1;
        if (stored_power == -1)
          break;
        stored[stored_index_or_result + 1] = stored_power;
        stored_count_or_slot = stored_index_or_result + 2;
        (&instance->targets[1].card)[stored_index_or_result] = -1;
        stored_index_or_result = stored_count_or_slot;
      } while (stored_count_or_slot != 0x12);
      default_target_definition(player, card, &td, 2);
      td.allow_cancel = 0;
      td.preferred_controller = player;
      if (stored_count_or_slot > 0)
      {
        creature_target_definition = &td;
        td.preferred_controller = player;
        td.allow_cancel = 0;
        stored_index_or_result = 0;
        do
        {
          if (can_target(creature_target_definition) != 0)
          {
            stored_power = stored[stored_index_or_result];
            scnprintf(msg, 100, "Select target creature (%d +1/+1 counters).", stored_power);
            if (new_pick_target(creature_target_definition, msg, 0, 0x80) != 0)
            {
              add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                           COUNTER_P1_P1, stored_power);
              instance->number_of_targets = '\0';
            }
          }
          stored_index_or_result = stored_index_or_result + 1;
        } while (stored_index_or_result != stored_count_or_slot);
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) &&
      (stored_index_or_result = check_status(player, card, 0x10000), stored_index_or_result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

int card_deaths_presence(int player, int card, event_t event)
{
  /* Death's Presence	|5|G
   * Enchantment
   * Whenever a creature you control dies, put X +1/+1 counters on target creature you control, where X is the power of the creature that died. */

  int i;
  int pow;
  card_instance_t *instance;
  int stored_index_or_result;
  int stored_power;
  int stored_count_or_result;
  target_definition_t *creature_target_definition;
  int stored[18];
  target_definition_t td;
  char msg[100];
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      bool legacy_found = false;
      if (0 < active_cards_count[player])
      {
        stored_index_or_result = 0;
        do
        {
          instance = in_play(player, stored_index_or_result);
          if ((((instance != NULL) &&
                (stored_count_or_result = is_what(player, stored_index_or_result, 0x80),
                 stored_count_or_result != 0)) &&
               (instance = get_card_instance(player, stored_index_or_result),
                instance->info_slot_fn == deaths_presence_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            legacy_found = true;
            break;
          }
          stored_index_or_result = stored_index_or_result + 1;
        } while (stored_index_or_result < active_cards_count[player]);
      }
      if (!legacy_found)
      {
        stored_index_or_result =
            create_targetted_legacy_effect(player, card, deaths_presence_effect, player, card);
        instance = get_card_instance(player, stored_index_or_result);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, stored_index_or_result, 0x10000);
      }
    }
  }
  else if (((event == EVENT_GRAVEYARD_FROM_PLAY) && (affected_card_controller == player)) &&
           ((stored_index_or_result = is_what(player, affected_card, 2),
             stored_index_or_result != 0 &&
                 ((stored_index_or_result = is_this_dying(affected_card_controller, affected_card),
                   stored_index_or_result != 0 &&
                       (stored_index_or_result = check_for_death_trigger_removal(
                            affected_card_controller, affected_card),
                        stored_index_or_result == 0))))))
  {
    stored_index_or_result = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
    do
    {
      pow = get_power(affected_card_controller, affected_card);
      if (pow != 0)
      {
        instance = get_card_instance(player, card);
        if (instance->targets[0xb].player < 1)
        {
          instance->targets[0xb].player = 1;
        }
        i = 1;
        do
        {
          if (instance->targets[i].player == -1)
          {
            instance->targets[i].player = pow;
            break;
          }
          if (instance->targets[i].card == -1)
          {
            instance->targets[i].card = pow;
            break;
          }
          i = i + 1;
        } while (i != 10);
      }
      stored_index_or_result = stored_index_or_result + -1;
    } while (stored_index_or_result != 0);
  }
  instance = get_card_instance(player, card);
  if ((instance->kill_code == '\0') &&
      (stored_index_or_result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY),
       stored_index_or_result != 0))
  {
    instance = get_card_instance(player, card);
    stored_index_or_result = 0;
    do
    {
      stored_power = (&instance->targets[1].player)[stored_index_or_result];
      stored_count_or_result = stored_index_or_result;
      if (stored_power == -1)
        break;
      stored[stored_index_or_result] = stored_power;
      stored_power = (&instance->targets[1].card)[stored_index_or_result];
      stored_count_or_result = stored_index_or_result + 1;
      (&instance->targets[1].player)[stored_index_or_result] = -1;
      if (stored_power == -1)
        break;
      stored[stored_index_or_result + 1] = stored_power;
      stored_count_or_result = stored_index_or_result + 2;
      (&instance->targets[1].card)[stored_index_or_result] = -1;
      stored_index_or_result = stored_count_or_result;
    } while (stored_count_or_result != 0x12);
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    td.allowed_controller = player;
    td.preferred_controller = player;
    if (stored_count_or_result < 1)
      return global_enchantment(player, card, event);
    creature_target_definition = &td;
    td.allow_cancel = 0;
    stored_index_or_result = 0;
    td.allowed_controller = player;
    td.preferred_controller = player;
    do
    {
      if (can_target(creature_target_definition) != 0)
      {
        stored_power = stored[stored_index_or_result];
        scnprintf(msg, 100, "Select target creature (%d +1/+1 counters).", stored_power);
        if (new_pick_target(creature_target_definition, msg, 0, 0x80) != 0)
        {
          add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                       COUNTER_P1_P1, stored_power);
          instance->number_of_targets = '\0';
        }
      }
      stored_index_or_result = stored_index_or_result + 1;
    } while (stored_index_or_result != stored_count_or_result);
  }
  return global_enchantment(player, card, event);
}

int card_drudge_beetle(int player, int card, event_t event){
  /* Drudge Beetle	|1|G
   * Creature - Insect 2/2
   * Scavenge |5|G (|5|G, Exile this card from your graveyard: Put a number of +1/+1 counters equal to this card's power on target creature. Scavenge only as a sorcery.) */

	return scavenge(player, card, event, MANACOST_XG(5, 1));
}

int card_druids_deliverance(int player, int card, event_t event){
  /* Druid's Deliverance |1|G
   * Instant, 1G (2)
   * Prevent all combat damage that would be dealt to you this turn.
   * Populate. (Put a token onto the battlefield that's a copy of a creature token you control.) */

	if( event == EVENT_RESOLVE_SPELL ){
		fog_special(player, card, player, FOG_COMBAT_DAMAGE_ONLY | FOG_ONLY_DAMAGE_TO_T_PLAYER);
		populate(player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_gatecreeper_vine(int player, int card, event_t event){
  /* Gatecreeper Vine	|1|G
   * Creature - Plant 0/2
   * Defender
   * When this creature enters, you may search your library for a basic land card or a Gate card, reveal it, put it into your hand, then shuffle. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		int choice = 0;
		unsigned int subt = SUBTYPE_GATE;
		int type = TYPE_PERMANENT;
		const char* msg = "Select a Gate card.";
		if( !IS_AI(player)
#ifdef MANALINKEH_STATIC
            || duel_host_scripted_input()
#endif
          ){
			choice = do_dialog(player, player, card, -1, -1, " Tutor a Gate card\n Tutor a basic land", 0);
		}
		if( choice == 1 ){
			subt = SUBTYPE_BASIC;
			type = TYPE_LAND;
			msg = "Select a basic land card.";
		}

		test_definition_t this_test;
		new_default_test_definition(&this_test, type, msg);
		this_test.subtype = subt;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}
	return 0;
}

int card_gobbling_ooze(int player, int card, event_t event){
  /* Gobbling Ooze	|4|G
   * Creature - Ooze 3/3
   * |G, Sacrifice another creature: Put a +1/+1 counter on this creature. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE | GAA_NOT_ME_AS_TARGET, MANACOST_G(1), 0, NULL, NULL);
}

int card_golgari_decoy(int player, int card, event_t event){
  /* Golgari Decoy	|3|G
   * Creature - Elf Rogue 2/2
   * All creatures able to block this creature do so.
   * Scavenge |3|G|G (|3|G|G, Exile this card from your graveyard: Put a number of +1/+1 counters equal to this card's power on target creature. Scavenge only as a sorcery.) */

	everybody_must_block_me(player, card, event);

	return scavenge(player, card, event, MANACOST_XG(3, 2));
}

int card_horncallers_chant(int player, int card, event_t event){
  /* Horncaller's Chant	|7|G
   * Sorcery
   * Create a 4/4 green Rhino creature token with trample, then populate. (Create a token that's a copy of a creature token you control.) */

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_RHINO, &token);
		token.key_plus = KEYWORD_TRAMPLE;
		generate_token(&token);

		populate(player, card);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_korozda_monitor(int player, int card, event_t event){
  /* Korozda Monitor	|2|G|G
   * Creature - Lizard 3/3
   * Trample
   * Scavenge |5|G|G (|5|G|G, Exile this card from your graveyard: Put a number of +1/+1 counters equal to this card's power on target creature. Scavenge only as a sorcery.) */

	return scavenge(player, card, event, MANACOST_XG(5, 2));
}

int card_mana_bloom(int player, int card, event_t event)
{
  /* Mana Bloom	|X|G
   * Enchantment
   * ~ enters the battlefield with X charge counters on it.
   * Remove a charge counter from ~: Add one mana of any color to your mana pool. Activate this ability only once each turn.
   * At the beginning of your upkeep, if ~ has no charge counters on it, return it to its owner's hand. */
  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  if (player == AI && x_value <= 0)
		ai_modifier -= 36;

	  instance->info_slot = x_value;
	}

  enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, instance->info_slot);

  if (event == EVENT_CAN_ACTIVATE)
	return count_counters(player, card, COUNTER_CHARGE) && can_produce_mana(player, card) && instance->targets[2].player <= 0;

  if (event == EVENT_ACTIVATE && produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1))
	{
	  instance->targets[2].player = 1;
	  remove_counters(player, card, player, card, COUNTER_CHARGE, 1);
	  tapped_for_mana_color = -2;
	}

  if (event == EVENT_COUNT_MANA && instance->targets[2].player <= 0 && count_counters(player, card, COUNTER_CHARGE))
	declare_mana_available_hex(player, COLOR_TEST_ANY_COLORED, 1);

  if (event == EVENT_CLEANUP && affect_me(player, card))
	instance->targets[2].player = -1;

  if (count_counters(player, card, COUNTER_CHARGE) <= 0 && current_turn == player && upkeep_trigger(player, card, event))	// deliberately not upkeep_trigger_ability
	bounce_permanent(player, card);

  return global_enchantment(player, card, event);
}

int card_oak_street_innkeeper(int player, int card, event_t event)
{
  /* Oak Street Innkeeper	|2|G
   * Creature - Elf 1/2
   * During turns other than yours, tapped creatures you control have hexproof. */

  if (event == EVENT_ABILITIES
	  && current_turn != player
	  && affected_card_controller == player
	  && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
	  && is_tapped(affected_card_controller, affected_card)
	  && !is_humiliated(player, card, AL_STATIC))
	hexproof(affected_card_controller, affected_card, event);

  // Force recomputation when a creature taps or untaps; this normally isn't done until a card or effect resolves
  if ((event == EVENT_UNTAP_CARD || event == EVENT_TAP_CARD)
	  && current_turn != player
	  && affected_card_controller == player
	  && is_what(affected_card_controller, affected_card, TYPE_CREATURE))
	{
	  get_card_instance(affected_card_controller, affected_card)->regen_status |= KEYWORD_RECALC_ABILITIES;
	  get_abilities(affected_card_controller, affected_card, EVENT_ABILITIES, -1);
	}

  return 0;
}

// rubbleback rhino --> Gladecover Scout

int card_savage_surge(int player, int card, event_t event){
  /* Savage Surge	|1|G
   * Instant
   * Target creature gets +2/+2 until end of turn. Untap that creature. */

	if( event == EVENT_CHECK_PUMP ){
		return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 2, 0, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier += (is_tapped(instance->targets[0].player, instance->targets[0].card) ? 24 : 0);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
			untap_card(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_slime_molding(int player, int card, event_t event){
	/* Slime Molding	|X|G
	 * Sorcery
	 * Put an X/X |Sgreen Ooze creature token onto the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_OOZE, &token);
		token.pow = token.tou = get_card_instance(player, card)->info_slot;
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_stonefare_crocodile(int player, int card, event_t event){
/* CARD_ID_STONEFARE_CROCODILE	12838
Stonefare Crocodile	|2|G
Creature - Crocodile 3/2
|2|B: ~ gains lifelink until end of turn. (Damage dealt by this creature also causes you to gain that much life.)*/

	return generic_shade(player, card, event, 0, MANACOST_XB(2, 1), 0, 0, 0, SP_KEYWORD_LIFELINK);
}

// towering indrik --> vanilla

int card_urban_burgeoning(int player, int card, event_t event){
/* CARD_ID_URBAN_BURGEONING	12862
Urban Burgeoning	|G
Enchantment - Aura
Enchant land
Enchanted land has "Untap this land during each other player's untap step." */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( current_turn != player && current_phase == PHASE_UNTAP && is_tapped(p, c) ){
		untap_card(p, c);

		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;

	return targeted_aura(player, card, event, &td, "TARGET_LAND");
}

int card_wild_beastmaster(int player, int card, event_t event)
{
/* CARD_ID_WILD_BEASTMASTER	12872
Wild Beastmaster	|2|G
Creature - Human Shaman 1/1
Whenever ~ attacks, each other creature you control gets +X/+X until end of turn, where X is ~'s power. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
	{
	  test_definition_t test;
	  default_test_definition(&test, TYPE_CREATURE);
	  test.not_me = 1;

	  int amt = get_power(player, card);

	  pump_creatures_until_eot(player, card, player, 0, amt,amt, 0,0, &test);
	}

  return 0;
}

int card_worldspine_wurm(int player, int card, event_t event){

	/* Worldspine Wurm	|8|G|G|G
	 * Creature - Wurm 15/15
	 * Trample
	 * When ~ dies, put three 5/5 |Sgreen Wurm creature tokens with trample onto the battlefield.
	 * When ~ is put into a graveyard from anywhere, shuffle it into its owner's library. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_WURM, &token);
		token.qty = 3;
		token.pow = token.tou = 5;
		token.key_plus = KEYWORD_TRAMPLE;
		generate_token(&token);
	}

	return 0;
}

// Gold
int card_abrupt_decay(int player, int card, event_t event){
/* CARD_ID_ABRUPT_DECAY	12645
Abrupt Decay	|B|G
Instant
~ can't be countered by spells or abilities.
Destroy target nonland permanent with converted mana cost 3 or less. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
	td.extra = 3;
	td.illegal_type = TYPE_LAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		state_untargettable(player, card, 1);
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
					"Select target non-land permanent with CMC 3 or less.", 1, NULL);
		if( spell_fizzled == 1 ){
			state_untargettable(player, card, 0);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		state_untargettable(player, card, 0);
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, NULL, 1, NULL);
}

int card_archon_of_the_triumvirate(int player, int card, event_t event)
{
/* CARD_ID_ARCHON_OF_THE_TRIUMVIRATE	12650
Archon of the Triumvirate	|5|W|U
Creature - Archon 4/5
Flying
Whenever ~ attacks, detain up to two target nonland permanents your opponents control.
(Until your next turn, those permanents can't attack or block and their activated abilities can't be activated.) */

	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY)
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_LAND;
		td.allowed_controller = 1-player;
		td.allow_cancel = 2;

		card_instance_t *instance = get_card_instance(player, card);

		int i;
		for(i=0; i<2; i++){
			if( new_pick_target(&td, "Select target nonland permanent an opponent controls.", i, GS_LITERAL_PROMPT) ){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
				ai_modifier_for_detain(player, card, i);
			}
			else{
				break;
			}
		}

		for(i=0; i<instance->number_of_targets; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			detain(player, card, instance->targets[i].player, instance->targets[i].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_armada_wurm(int player, int card, event_t event){
/* CARD_ID_ARMADA_WURM	12652
Armada Wurm	|2|G|G|W|W
Creature - Wurm 5/5
Trample
When ~ enters the battlefield, create a 5/5 |Sgreen Wurm creature token with trample. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_WURM, &token);
		token.pow = token.tou = 5;
		token.key_plus = KEYWORD_TRAMPLE;
		generate_token(&token);
	}

	return 0;
}

int card_auger_spree(int player, int card, event_t event){
/* CARD_ID_AUGER_SPREE	12656
Auger Spree	|1|B|R
Instant
Target creature gets +4/-4 until end of turn. */

	return vanilla_instant_pump(player, card, event, ANYBODY, ANYBODY, 4, -4, 0, 0);
}

int card_azorius_charm(int player, int card, event_t event){
  /* Azorius Charm |W|U
   * Instant
   * Choose one —
   * • Creatures you control gain lifelink until end of turn.
   * • Draw a card.
   * • Put target attacking or blocking creature on top of its owner's library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_IN_COMBAT;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_LIFELINK = 1,
		CHOICE_DRAW,
		CHOICE_REMOVE_CRIT
	};

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int priority_lifelink = 1;
			if( life[player] < 6 ){
				test_definition_t test;
				default_test_definition(&test, TYPE_CREATURE);
				if( current_turn == player ){
					test.state = STATE_ATTACKING;
					priority_lifelink = 1 + (check_battlefield_for_special_card(player, card, player, CBFSC_GET_TOTAL_POW, &test)*2);
				}
				else{
					test.state = STATE_BLOCKING;
					priority_lifelink = 1 + (check_battlefield_for_special_card(player, card, player, CBFSC_GET_TOTAL_POW, &test)*2);
				}
			}
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Lifelink to your crits", 1, priority_lifelink,
								"Draw a card", 1, 5,
								"Put creature on deck", can_target(&td), 10);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_LIFELINK || instance->info_slot == CHOICE_DRAW ){
			return basic_spell(player, card, event);
		}
		if( instance->info_slot == CHOICE_REMOVE_CRIT ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ATTACKING_BLOCKING_CREATURE", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_LIFELINK ){
			pump_subtype_until_eot(player, card, player, -1, 0, 0, 0, SP_KEYWORD_LIFELINK);
		}
		if( instance->info_slot == CHOICE_DRAW ){
			draw_cards(player, 1);
		}
		if( instance->info_slot == CHOICE_REMOVE_CRIT && valid_target(&td) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_call_of_the_conclave(int player, int card, event_t event){
	/* Call of the Conclave	|G|W
	 * Sorcery
	 * Put a 3/3 |Sgreen Centaur creature token onto the battlefield. */

	if(event == EVENT_RESOLVE_SPELL ){
		generate_token_by_id(player, card, CARD_ID_CENTAUR);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_carnival_hellsteed(int player, int card, event_t event){
/* CARD_ID_CARNIVAL_HELLSTEED	12674
Carnival Hellsteed	|4|B|R
Creature - Nightmare Horse 5/4
First strike, haste
Unleash (You may have this creature enter the battlefield with a +1/+1 counter on it. It can't block as long as it has a +1/+1 counter on it.)*/

	haste(player, card, event);
	return unleash(player, card, event);
}

int card_centaur_healer(int player, int card, event_t event){
/* CARD_ID_CENTAUR_HEALER	12676
Centaur Healer	|1|G|W
Creature - Centaur Cleric 3/3
When ~ enters the battlefield, you gain 3 life. */

	return cip_lifegain(player, card, event, 3);
}

int card_chemisters_trick(int player, int card, event_t event){
/* CARD_ID_CHEMISTERS_TRICK	12679
Chemister's Trick	|U|R
Instant
Target creature you don't control gets -2/-0 until end of turn and attacks this turn if able.
Overload |3|U|R (You may cast this spell for its overload cost. If you do, change its text by replacing all instances of "target" with "each.")*/

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST ){
			if( generic_spell(player, card, event, 0, NULL, NULL, 0, NULL) ){
				if( instance->info_slot == 1 ){
					return 1;
				}
				return can_target(&td);
			}
		}

		if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			if( current_turn == player || current_phase > PHASE_DECLARE_ATTACKERS ){
				ai_modifier-=25;
			}
			int result = 1;
			instance->number_of_targets = 0;
			if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
				result = pay_overload(player, card, MANACOST_XUR(3, 1, 1));
			}
			if( result == 1 ){
				new_pick_target(&td, "Select target creature your opponent controls.", 0, 1 | GS_LITERAL_PROMPT);
				if(	spell_fizzled == 1 ){
					return 0;
				}
			}
			instance->info_slot |= result;
		}

		if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot & 2 ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				pump_creatures_until_eot(player, card, 1-player, 0, -2, 0, 0, SP_KEYWORD_MUST_ATTACK, &this_test);
			}
			else{
				if( valid_target(&td) ){
					pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -2, 0, 0, SP_KEYWORD_MUST_ATTACK);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return overload(player, card, event, MANACOST_XUR(3, 1, 1));
}

int card_collective_blessing(int player, int card, event_t event){
/* CARD_ID_COLLECTIVE_BLESSING	12686
Collective Blessing	|3|G|G|W
Enchantment
Creatures you control get +3/+3. */

	boost_creature_type(player, card, event, -1, 3, 3, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
	return global_enchantment(player, card, event);
}

int card_common_bond(int player, int card, event_t event){
/* CARD_ID_COMMON_BOND	12687
Common Bond	|1|G|W
Instant
Put a +1/+1 counter on target creature.
Put a +1/+1 counter on target creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			add_state(instance->targets[0].player, instance->targets[0].card, STATE_TARGETTED);
			new_pick_target(&td, "TARGET_CREATURE", 1, 1);
			remove_state(instance->targets[0].player, instance->targets[0].card, STATE_TARGETTED);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				add_counters(player, card, instance->targets[i].player, instance->targets[i].card, COUNTER_P1_P1, 1);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

// corpsejack menace --> vanilla

int card_coursers_accord(int player, int card, event_t event){
	/* Coursers' Accord	|4|G|W
	 * Sorcery
	 * Put a 3/3 |Sgreen Centaur creature token onto the battlefield, then populate. */

	if(event == EVENT_RESOLVE_SPELL ){
		generate_token_by_id(player, card, CARD_ID_CENTAUR);
		populate(player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_detention_sphere(int player, int card, event_t event)
{
  /* Detention Sphere	|1|W|U
   * Enchantment
   * When this enchantment enters, you may exile target nonland permanent not named ~ and all other permanents with the same name as that permanent.
   * When this enchantment leaves the battlefield, return the exiled cards to the battlefield under their owner's control. */

  int p;
  int c;
  int iid;
  int scan_card_or_data_offset;
  int chosen_name_or_returned_card;
  unsigned int *exiled_record;
  card_instance_t *instance;
  card_instance_t *candidate;
  int original_iid;
  int candidate_owner_or_result;
  unsigned int player_00;
  unsigned int exiled_card_or_opponent;
  int (*marked_cursor)[151];
  unsigned int exiled_csv_id;
  int leg;
  int idx;
  target_definition_t td;
  int marked[2][151];
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    leaves_play(player, card, event, 1, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      td.who_chooses = 0;
      marked[0][0] = 0;
      while (exiled_record =
                 (unsigned int *)exiledby_find_any(player, card, &td.who_chooses, marked[0]),
             exiled_record != NULL)
      {
        exiled_card_or_opponent = *exiled_record;
        *exiled_record = 0xffffffff;
        player_00 = exiled_card_or_opponent >> 0x1f;
        scan_card_or_data_offset =
            (exiled_card_or_opponent & 0x7fffffff) + exiled_card_or_opponent * 8;
        if (check_rfg(player_00,
                      (unsigned int)*(unsigned short *)(cards_data[0].cc +
                                                        scan_card_or_data_offset * 8 + -7)) != 0)
        {
          chosen_name_or_returned_card =
              add_card_to_hand(player_00, exiled_card_or_opponent & 0x7fffffff);
          remove_card_from_rfg(
              player_00, (unsigned int)*(unsigned short *)(cards_data[0].cc +
                                                           scan_card_or_data_offset * 8 + -7));
          put_into_play(player_00, chosen_name_or_returned_card);
        }
      }
    }
    return global_enchantment(player, card, event);
  }
  default_target_definition(player, card, &td, 0x47);
  td.illegal_type = TYPE_LAND;
  td.special = TARGET_SPECIAL_NOT_ME;
  td.allow_cancel = 0;
  instance = get_card_instance(player, card);
  scan_card_or_data_offset = can_target(&td);
  if ((scan_card_or_data_offset != 0) && (scan_card_or_data_offset = new_pick_target(
                                              &td, "Select another nonland permanent.", 0, 0x80),
                                          scan_card_or_data_offset != 0))
  {
    chosen_name_or_returned_card =
        get_card_name(instance->targets[0].player, instance->targets[0].card);
    p = current_turn;
    marked_cursor = marked;
    for (candidate_owner_or_result = 0x12e; candidate_owner_or_result != 0;
         candidate_owner_or_result = candidate_owner_or_result + -1)
    {
      (*marked_cursor)[0] = 0;
      marked_cursor = (int (*)[151])((int)marked_cursor + 4u);
    }
    if (chosen_name_or_returned_card == -1)
    {
      iid = get_original_internal_card_id(instance->targets[0].player, instance->targets[0].card);
      exiled_csv_id = (unsigned int)cards_data[iid].id;
      exile_card_and_remember_it_on_exiledby(player, card, instance->targets[0].player,
                                             instance->targets[0].card);
      create_card_name_legacy(player, card, exiled_csv_id);
    }
    else
    {
      leg = 0;
      idx = 0;
      c = active_cards_count[current_turn] + -1;
      if (c < 0)
      {
        exiled_csv_id = 0xffffffff;
      }
      else
      {
        exiled_csv_id = 0xffffffff;
        do
        {
          candidate = in_play(p, c);
          if (((candidate != NULL) &&
               (candidate_owner_or_result = is_what(p, c, 0x47), candidate_owner_or_result != 0)) &&
              (candidate_owner_or_result =
                   has_same_name(instance->targets[0].player, instance->targets[0].card, p, c),
               candidate_owner_or_result != 0))
          {
            candidate_owner_or_result = is_token(p, c);
            if (candidate_owner_or_result == 0)
            {
              candidate_owner_or_result = get_owner(p, c);
              original_iid = get_original_internal_card_id(p, c);
              exiled_csv_id = (unsigned int)cards_data[original_iid].id;
              exiledby_remember(player, card, candidate_owner_or_result, original_iid, &leg, &idx);
            }
            marked[p][c] = 1;
          }
          c = c + -1;
        } while (c != -1);
      }
      p = 1 - current_turn;
      c = active_cards_count[p] + -1;
      if (-1 < c)
      {
        do
        {
          candidate = in_play(p, c);
          if (((candidate != NULL) &&
               (candidate_owner_or_result = is_what(p, c, 0x47), candidate_owner_or_result != 0)) &&
              (candidate_owner_or_result =
                   has_same_name(instance->targets[0].player, instance->targets[0].card, p, c),
               candidate_owner_or_result != 0))
          {
            candidate_owner_or_result = is_token(p, c);
            if (candidate_owner_or_result == 0)
            {
              candidate_owner_or_result = get_owner(p, c);
              original_iid = get_original_internal_card_id(p, c);
              exiled_csv_id = (unsigned int)cards_data[original_iid].id;
              exiledby_remember(player, card, candidate_owner_or_result, original_iid, &leg, &idx);
            }
            marked[p][c] = 1;
          }
          c = c + -1;
        } while (c != -1);
      }
      p = current_turn;
      c = 0x96;
      do
      {
        if (marked[p][c] != 0)
        {
          kill_card(p, c, KILL_EXILE);
        }
        c = c + -1;
      } while (c != -1);
      exiled_card_or_opponent = current_turn ^ 1;
      scan_card_or_data_offset = 0x96;
      do
      {
        if (marked[exiled_card_or_opponent][scan_card_or_data_offset] != 0)
        {
          kill_card(exiled_card_or_opponent, scan_card_or_data_offset, KILL_EXILE);
        }
        scan_card_or_data_offset = scan_card_or_data_offset + -1;
      } while (scan_card_or_data_offset != -1);
    }
    if (chosen_name_or_returned_card != -1 && (int)exiled_csv_id >= 0)
      create_card_name_legacy(player, card, exiled_csv_id);
  }
  leaves_play(player, card, EVENT_ETB, 1, 0);
  return global_enchantment(player, card, event);
}

int card_dramatic_rescue(int player, int card, event_t event){
  /* Dramatic Rescue	|W|U
   * Instant
   * Return target creature to its owner's hand. You gain 2 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			gain_life(player, 2);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_dreadbore(int player, int card, event_t event){
  /* Dreadbore	|B|R
   * Sorcery
   * Destroy target creature or planeswalker. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature or planeswalker.", 1, NULL);
}

int card_dreg_mangler(int player, int card, event_t event){
  /* Dreg Mangler	|1|B|G
   * Creature - Plant Zombie 3/3
   * Haste
   * Scavenge |3|B|G (|3|B|G, Exile this card from your graveyard: Put a number of +1/+1 counters equal to this card's power on target creature. Scavenge only as a sorcery.) */

	haste(player, card, event);
	return scavenge(player, card, event, MANACOST_XBG(3, 1, 1));
}

int card_epic_experiment(int player, int card, event_t event){

	/* Epic Experiment	|X|U|R
	 * Sorcery
	 * Exile the top X cards of your library. For each instant and sorcery card with converted mana cost X or less among them, you may cast that card without
	 * paying its mana cost. Then put all cards exiled this way that weren't cast into your graveyard. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		int max = MIN(count_deck(player), instance->info_slot);
		if( max ){
			int *deck = deck_ptr[player];
			show_deck(player, deck, max, "Cards revealed with Epic Experiment.", 0, gs_done_008b40e0 );
			int i;
			int cards_exiled[max];
			for(i=0; i<max; i++){
				cards_exiled[i] = deck[0];
				rfg_card_in_deck(player, 0);
			}
			int playable[max];
			int pc = 0;
			for(i=0; i<max; i++){
				if( is_what(-1, cards_exiled[i], TYPE_SPELL) && ! is_what(-1, cards_exiled[i], TYPE_CREATURE) &&
					get_cmc_by_internal_id(cards_exiled[i]) <= instance->info_slot && can_legally_play_iid(player, cards_exiled[i])
				  ){
					playable[pc] = cards_exiled[i];
					pc++;
				}
			}
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_SPELL, "Select a spell to play for free.");
			while( pc ){
					int selected = select_card_from_zone(player, player, playable, pc, 0, AI_MAX_VALUE, -1, &this_test);
					if( selected != -1 ){
						int csvid = cards_data[playable[selected]].id;
						int k;
						for(k=0; k<max; k++){
							if( cards_exiled[k] == playable[selected] ){
								cards_exiled[k] = -1;
								break;
							}
						}
						for(k=selected; k<pc; k++){
							playable[k] = playable[k+1];
						}
						pc--;
						play_card_in_exile_for_free(player, player, csvid);
					}
					else{
						break;
					}
			}
			for(i=0; i<max; i++){
				if( cards_exiled[i] != -1 ){
					int pos = find_iid_in_rfg(player, cards_exiled[i]);
					if( pos > -1 ){
						from_exile_to_graveyard(player, pos);
					}
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_essence_backlash(int player, int card, event_t event){
  /* Essence Backlash	|2|U|R
   * Instant
   * Counter target creature spell. ~ deals damage equal to that spell's power to its controller. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, &td, 0) ){
			int pow = get_base_power(instance->targets[0].player, instance->targets[0].card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			damage_player(instance->targets[0].player, pow, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}

int card_fall_of_the_gavel(int player, int card, event_t event){
  /* Fall of the Gavel	|3|W|U
   * Instant
   * Counter target spell. You gain 5 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, &td, 0) ){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			gain_life(player, 5);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}

int card_fireminds_foresight(int player, int card, event_t event){
  /* Firemind's Foresight	|5|U|R
   * Instant
   * Search your library for an instant card with mana value 3, reveal it, and put it into your hand. Then repeat this process for instant cards with mana values 2 and 1. Then shuffle. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_INSTANT | TYPE_INTERRUPT);
		this_test.type_flag = F1_NO_CREATURE;
		this_test.no_shuffle = 1;
		int i;
		for(i=3; i>0;i--){
			scnprintf(this_test.message, 100, "Select an Instant card with CMC = %d", i);
			this_test.cmc = i;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		}
		shuffle(player);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_goblin_electromancer(int player, int card, event_t event){
  /* Goblin Electromancer	|U|R
   * Creature - Goblin Wizard 2/2
   * Instant and sorcery spells you cast cost |1 less to cast. */

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card, AL_STATIC) ){
		if( is_what(affected_card_controller, affected_card, TYPE_SPELL) &&
			! is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
		{
			COST_COLORLESS--;
		}
	}

	return 0;
}

int card_golgari_charm(int player, int card, event_t event){
  /* Golgari Charm |B|G
   * Instant
   * Choose one —
   * • All creatures get -1/-1 until end of turn.
   * • Destroy target enchantment.
   * • Regenerate each creature you control. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.illegal_abilities = 0;
	td2.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td2.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_WEAKEN = 1,
		CHOICE_KILL_ENCHANTMENT,
		CHOICE_REGENERATION
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET | GS_REGENERATION, &td2, NULL, 1, NULL) ){
			return 99;
		}
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {
							0,
							1,
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td, NULL, 1, NULL),
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET | GS_REGENERATION, &td2, NULL, 1, NULL)
			};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Weaken all crits", abils[1], current_phase == PHASE_AFTER_BLOCKING ? 10 : 1,
								"Kill an enchantment", abils[2], 5,
								"Regenerate all your crits", abils[3], 25);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_WEAKEN || instance->info_slot == CHOICE_REGENERATION ){
			return basic_spell(player, card, event);
		}
		if( instance->info_slot == CHOICE_KILL_ENCHANTMENT ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_WEAKEN ){
			pump_subtype_until_eot(player, card, ANYBODY, -1, -1, -1, 0, 0);
		}
		if( instance->info_slot == CHOICE_KILL_ENCHANTMENT && valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( instance->info_slot == CHOICE_REGENERATION ){
			int c;
			for(c = active_cards_count[player]-1; c > -1; c--){
				if( in_play(player, c) && is_what(player, c, TYPE_CREATURE) ){
					if( can_be_regenerated(player, c) ){
						regenerate_target(player, c);
					}
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_grisly_salvage(int player, int card, event_t event){
/* CARD_ID_GRISLY_SALVAGE	12739 -> also code for Scout the Borders
Grisly Salvage	|B|G
Instant
Reveal the top five cards of your library. You may put a creature or land card from among them into your hand. Put the rest into your graveyard.*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int total = get_total_available_lands(player);
		if( total <= 3 || total >= 6 ){
			ai_modifier += 48;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int amount = MIN(5, count_deck(player));
		if( amount > 0 ){
			show_deck(0, deck_ptr[player], amount, "Cards revealed.", 0, gs_done_008b40e0 );
			show_deck(1, deck_ptr[player], amount, "Cards revealed.", 0, gs_done_008b40e0 );


			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE | TYPE_LAND, "Select an Creature or Land card.");
			this_test.create_minideck = amount;
			this_test.no_shuffle = 1;
			if( IS_AI(player) ){
				int total = get_total_available_lands(player);
				if( total <= 3 ){
					this_test.type = TYPE_LAND;
				}
				if( total >= 6 ){
					this_test.type = TYPE_CREATURE;
				}
			}
			int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
			if( result < 0 && IS_AI(player) ){
				this_test.type = TYPE_CREATURE | TYPE_LAND;
				result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
			}
			if( result > -1 ){
				amount--;
			}
			if( amount > 0 ){
				mill(player, amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_havoc_festival(int player, int card, event_t event){
  /* Havoc Festival	|4|B|R
   * Enchantment
   * Players can't gain life.
   * At the beginning of each player's upkeep, that player loses half their life, rounded up. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int amount = (life[current_turn]+1)/2;
		lose_life(current_turn, amount);
	}

	if( in_play(player, card) && event == EVENT_STATIC_EFFECTS && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) )
	{
		player_bits[0] |= PB_CANT_GAIN_LIFE;
		player_bits[1] |= PB_CANT_GAIN_LIFE;
	}

	return global_enchantment(player, card, event);
}

int card_hellhole_flailer(int player, int card, event_t event){
	/* Hellhole Flailer	|1|B|R	0x200773f
	 * Creature - Ogre Warrior 3/2
	 * Unleash
	 * |2|B|R, Sacrifice ~: ~ deals damage equal to its power to target player or planeswalker. */
	unleash(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
	}

	if (event == EVENT_ACTIVATE){
		instance->info_slot = get_power(player, card);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XBR(2, 1, 1), 0, &td, "TARGET_PLAYER_OR_PLANESWALKER");
}

int card_hussar_patrol(int player, int card, event_t event){
  /* Hussar Patrol	|2|W|U
   * Creature - Human Knight 2/4
   * Flash (You may cast this spell any time you could cast an instant.)
   * Vigilance */

	vigilance(player, card, event);
	return flash(player, card, event);
}

int card_hypersonic_dragon(int player, int card, event_t event){
  /* Hypersonic Dragon	|3|U|R
   * Creature - Dragon 4/4
   * Flying, haste
   * You may cast sorcery spells as though they had flash. (You may cast them any time you could cast an instant.) */

	haste(player, card, event);

	if( event == EVENT_CAN_ACTIVATE && hand_count[player] > 0 && ! is_humiliated(player, card, AL_STATIC) ){
		int count = 0;
		while( count < active_cards_count[player] ){
				if( in_hand(player, count) && is_what(player, count, TYPE_SORCERY) ){
					if( has_mana_to_cast_iid(player, event, get_original_internal_card_id(player, count)) &&
						can_legally_play_iid(player, get_original_internal_card_id(player, count))
					  ){
						return 1;
					}
				}
				count++;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int playable[2][hand_count[player]];
		int pc = 0;
		int count = 0;
		while( count < active_cards_count[player] ){
				if( in_hand(player, count) && is_what(player, count, TYPE_SORCERY) ){
					if( has_mana_to_cast_iid(player, event, get_original_internal_card_id(player, count)) &&
						can_legally_play_iid(player, get_original_internal_card_id(player, count))
					  ){
						playable[0][pc] = get_original_internal_card_id(player, count);
						playable[1][pc] = count;
						pc++;
					}
				}
				count++;
		}
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_SORCERY, "Select a Sorcery to play");
		int selected = select_card_from_zone(player, player, playable[0], pc, 0, AI_MAX_VALUE, -1, &this_test);
		if( selected != -1 ){
			charge_mana_from_id(player, -1, event, cards_data[playable[0][selected]].id);
			if( spell_fizzled != 1 ){
				play_card_in_hand_for_free(player, playable[1][selected]);
				cant_be_responded_to = 1;	// The spell will be respondable to, but this (fake) activation won't
			}
		}
		else{
			spell_fizzled = 1;
			return 0;
		}
	}

	return flash(player, card, event);
}

int card_isperia_supreme_judge(int player, int card, event_t event)
{
  /* Isperia, Supreme Judge	|2|W|W|U|U
   * Legendary Creature - Sphinx 6/4
   * Flying
   * Whenever a creature attacks you or a planeswalker you control, you may draw a card. */

  check_legend_rule(player, card, event);

  store_attackers(player, card, event, DAT_SEPARATE_TRIGGERS, 1-player, -1, NULL);
  dat_ability(player, card, event, DAT_SEPARATE_TRIGGERS | (duh_mode(player) ? DAT_RESOLVE_TRIGGER_MANDATORY : DAT_RESOLVE_TRIGGER_OPTIONAL), 1);
  if (event == EVENT_DAT_ABILITY)
    draw_a_card(player);
  return 0;
}

int card_izzet_charm(int player, int card, event_t event){
	/* Izzet Charm	|U|R
	 * Instant
	 * Choose one -
	 * * Counter target noncreature spell unless its controller pays |2.
	 * * ~ deals 2 damage to target creature.
	 * * Draw two cards, then discard two cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td2;
	counterspell_target_definition(player, card, &td2, TYPE_ANY);
	td2.illegal_type = TYPE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_COUNTER = 1,
		CHOICE_DAMAGE,
		CHOICE_DRAW_DISCARD
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_COUNTERSPELL, &td2, NULL, 1, NULL) ){
			return 99;
		}
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {
							0,
							generic_spell(player, card, EVENT_CAN_CAST, GS_COUNTERSPELL, &td2, NULL, 1, NULL),
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td, NULL, 1, NULL),
							1
			};
			int total = get_total_available_lands(player);
			int priority_draw_discard = total <= 3 || total >= 6 ? 10 : 1;
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Counter a noncreature spell", abils[1], 25,
								"2 damage to a creature", abils[2], 5,
								"Draw 2 & discard 2", abils[3], priority_draw_discard);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_COUNTER ){
			return generic_spell(player, card, event, GS_COUNTERSPELL, &td2, NULL, 1, NULL);
		}
		if( instance->info_slot == CHOICE_DAMAGE ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CRETURE", 1, NULL);
		}
		if( instance->info_slot == CHOICE_DRAW_DISCARD ){
			ai_modifier += 24;
			return basic_spell(player, card, event);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_COUNTER ){
			return counterspell(player, card, event, &td2, 0);
		}
		if( instance->info_slot == CHOICE_DAMAGE && valid_target(&td) ){
			damage_target0(player, card, 2);
		}
		if( instance->info_slot == CHOICE_DRAW_DISCARD ){
			draw_cards(player, 2);
			new_multidiscard(player, 2, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_izzet_staticaster(int player, int card, event_t event){
  /* Izzet Staticaster	|1|U|R
   * Creature - Human Wizard 0/3
   * Flash (You may cast this spell any time you could cast an instant.)
   * Haste
   * |T: This creature deals 1 damage to target creature and each other creature with the same name as that creature. */

	haste(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return flash(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_creature_and_all_the_others_with_the_same_name(instance->parent_controller, instance->parent_card,
																	instance->targets[0].player, instance->targets[0].card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST0, 0, &td, "TARGET_CREATURE");
}

static const char* is_forest_or_swamp(int who_chooses, int player, int card, int targeting_player, int targeting_card){
	return has_subtype(player, card, SUBTYPE_SWAMP) || has_subtype(player, card, SUBTYPE_FOREST) ? NULL : gs_target_error_subtype;	// ",subtype"
}

int card_jarad_golgari_lich_lord(int player, int card, event_t event){
	/* Jarad, Golgari Lich Lord	|B|B|G|G
	 * Legendary Creature - Zombie Elf 2/2
	 * ~ gets +1/+1 for each creature card in your graveyard.
	 * |1|B|G, Sacrifice another creature: Each opponent loses life equal to the sacrificed creature's power.
	 * Sacrifice |Ha Swamp and |Ha Forest: Return Jarad from your graveyard to your hand. */

	check_legend_rule(player, card, event);

	if( event == EVENT_GRAVEYARD_ABILITY ){
		if( can_sacrifice_type_as_cost(player, 2, TYPE_LAND) ){
			int f_found = 0;
			int s_found = 0;
			int good = 0;
			int i;
			for(i=0; i<active_cards_count[player]; i++ ){
				if( in_play(player, i) && is_what(player, i, TYPE_LAND) &&
					! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) )
				{
					if( has_subtype(player, i, SUBTYPE_FOREST) || has_subtype(player, i, SUBTYPE_SWAMP) ){
						good++;
						if( has_subtype(player, i, SUBTYPE_FOREST) ){
							f_found++;
						}
						if( has_subtype(player, i, SUBTYPE_SWAMP) ){
							s_found++;
						}
					}
				}
				if( f_found > 0 && s_found > 0 && good > 1 ){
					return GA_RETURN_TO_HAND;
				}
			}
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_LAND);
		td1.allowed_controller = player;
		td1.preferred_controller = player;
		td1.illegal_abilities = 0;
		td1.extra = target_predicate_extra(is_forest_or_swamp);
		td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;

		card_instance_t *instance = get_card_instance(player, card);

		if( new_pick_target(&td1, "Select a Forest or Swamp to sacrifice", 0, 1 | GS_LITERAL_PROMPT) ){
			instance->number_of_targets = 0;
			int mode = 0;
			int lnds[2] = {-1, -1};
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			lnds[0] = instance->targets[0].card;
			mode |= has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_SWAMP) ? 1 : 0;
			mode |= has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_FOREST) ? 2 : 0;
			char msg[100];
			if( mode == 3 ){
				scnprintf(msg, 100, "Select a Forest or Swamp to sacrifice");
			}
			else{
				if( mode & 1 ){
					scnprintf(msg, 100, "Select a Forest to sacrifice");
					default_target_definition(player, card, &td1, TYPE_LAND);
					td1.allowed_controller = player;
					td1.preferred_controller = player;
					td1.illegal_abilities = 0;
					td1.required_subtype = SUBTYPE_FOREST;
				}
				if( mode & 2 ){
					scnprintf(msg, 100, "Select a Swamp to sacrifice");
					default_target_definition(player, card, &td1, TYPE_LAND);
					td1.allowed_controller = player;
					td1.preferred_controller = player;
					td1.illegal_abilities = 0;
					td1.required_subtype = SUBTYPE_SWAMP;
				}
			}
			if( new_pick_target(&td1, msg, 0, 1 | GS_LITERAL_PROMPT) ){
				lnds[1] = instance->targets[0].card;
				instance->number_of_targets = 0;
			}
			state_untargettable(player, lnds[0], 0);
			if( spell_fizzled == 1 ){
				return 0;
			}
			kill_card(player, lnds[0], KILL_SACRIFICE);
			kill_card(player, lnds[1], KILL_SACRIFICE);
			return GAPAID_REMOVE;
		}
	}

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		event_result+=count_graveyard_by_type(player, TYPE_CREATURE);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_NOT_ME_AS_TARGET | GAA_SACRIFICE_CREATURE, MANACOST_XBG(1, 1, 1), 0,
										NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_XBG(1, 1, 1)) ){
			state_untargettable(player, card, 1);
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			state_untargettable(player, card, 0);
			if (!sac){
				cancel = 1;
				return 0;
			}
			get_card_instance(player, card)->info_slot = get_power(BYTE2(sac), BYTE3(sac));
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		lose_life(1-player, get_card_instance(player, card)->info_slot);
	}

	return 0;
}

int card_jarads_orders(int player, int card, event_t event){
  /* Jarad's Orders	|2|B|G
   * Sorcery
   * Search your library for up to two creature cards and reveal them. Put one into your hand and the other into your graveyard. Then shuffle. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_LAND, 1)*8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select an Creature to add to your hand.");
		this_test.no_shuffle = 1;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);

		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a Creature to put into the graveyard.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_GRAVE, 0, AI_MAX_CMC, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_korozda_guildmage(int player, int card, event_t event){
/* Korozda Guildmage	|B|G
 * Creature - Elf Shaman 2/2
 * |1|B|G: Target creature gets +1/+1 and gains intimidate until end of turn.
 * |2|B|G, Sacrifice a nontoken creature: Put X 1/1 |Sgreen Saproling creature tokens onto the battlefield, where X is the sacrificed
 *		creature's toughness. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a nontoken creature to sacrifice.");
	test.type = F1_NO_TOKEN;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_PUMP = 1,
		CHOICE_SAPROLINGS
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XBG(1, 1, 1), 0, &td, NULL) ){
			return 1;
		}
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_XBG(2, 1, 1), 0, NULL, NULL,
																0, &test, 0);
	}

	if( event == EVENT_ACTIVATE ){
		instance->targets[1].player = 0;
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_XBG(1, 1, 1), 0, &td, NULL),
						generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XBG(2, 1, 1), 0,
																		NULL, NULL, 0, &test, 0)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"+1/+1 and intimidate", 	abils[1], current_phase < PHASE_DECLARE_BLOCKERS ? 10 : 1,
							"Sac & Saprolings", 		abils[2], current_phase >= PHASE_AFTER_BLOCKING ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_PUMP ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XBG(1, 1, 1), 0, &td, "TARGET_CREATURE");
		}
		if( choice == CHOICE_SAPROLINGS ){
			generic_activated_ability(player, card, event, 0, MANACOST_XBG(2, 1, 1), 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
				if( !sac ){
					cancel = 1;
					return 0;
				}
				instance->targets[1].player = get_toughness(BYTE2(sac), BYTE3(sac));
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PUMP && valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 1, 1, 0, SP_KEYWORD_INTIMIDATE);
		}
		if( instance->info_slot == CHOICE_SAPROLINGS ){
			generate_tokens_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING, instance->targets[1].player);
		}
	}

	return 0;
}

int card_lotleth_troll(int player, int card, event_t event){
  /* Lotleth Troll |B|G
   * Creature — Zombie Troll 2/1
   * Trample
   * Discard a creature card: Put a +1/+1 counter on Lotleth Troll.
   * {B}: Regenerate Lotleth Troll. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.zone = TARGET_ZONE_HAND;

		int num_can_discard = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test);
		if( num_can_discard <= 0 )
			return 0;

		return generic_shade_amt_can_pump(player, card, 1, 0, MANACOST0, num_can_discard);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a Creature card to discard.");
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_PUMP = 1,
		CHOICE_REGENERATE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( regeneration(player, card, event, MANACOST_B(1)) ){
			return 99;
		}
		if( generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL) ){
			return check_battlefield_for_special_card(player, card, player, 0, &this_test);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, 0, 0};
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_DISCARD, MANACOST0, 0, NULL, NULL) &&
            check_battlefield_for_special_card(player, card, player, 0, &this_test)){
            abils[1] = !(land_can_be_played & LCBP_REGENERATION);
        }
        abils[2] = regeneration(player, card, EVENT_CAN_ACTIVATE, MANACOST_B(1));
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Discard & pump", 	abils[1], 5,
							"Regenerate", 		abils[2], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_PUMP ){
			generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
				if( selected > -1 ){
					discard_card(player, selected);
				}
				else{
					spell_fizzled = 1;
				}
			}
		}
		if( choice == CHOICE_REGENERATE ){
			return regeneration(player, card, event, MANACOST_B(1));
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PUMP ){
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 1);
		}
		if( instance->info_slot == CHOICE_REGENERATE ){
			return regeneration(player, card, event, MANACOST_B(1));
		}
	}

	return 0;
}

int card_lyev_skyknight(int player, int card, event_t event){
  /* Lyev Skyknight	|1|W|U
   * Creature - Human Knight 3/1
   * Flying
   * When this creature enters, detain target nonland permanent an opponent controls. (Until your next turn, that permanent can't attack or block and its activated abilities can't be activated.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_LAND;
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target non-land permanent your opponent controls", 0, GS_LITERAL_PROMPT) ){
			ai_modifier_for_detain(player, card, 0);
			detain(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_loxodon_smither(int player, int card, event_t event){
  /* Loxodon Smiter	|1|G|W
   * Creature - Elephant Soldier 4/4
   * This spell can't be countered.
   * If a spell or ability an opponent controls causes you to discard this card, put it onto the battlefield instead of putting it into your graveyard. */

	cannot_be_countered(player, card, event);
	return 0;
}

int card_mercurial_chemister(int player, int card, event_t event){
  /* Mercurial Chemister |3|U|R
   * Creature — Human Wizard 2/3
   * {U}, {T}: Draw two cards.
   * {R}, {T}, Discard a card: Mercurial Chemister deals damage to target creature equal to the discarded card's converted mana cost. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_DRAW = 1,
		CHOICE_DISCARD_DAMAGE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_U(1), 0, NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DISCARD, MANACOST_R(1), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
                        0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_U(1), 0, NULL, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DISCARD,
													MANACOST_R(1), 0, &td, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Draw 2 cards", 					abils[1], hand_count[player] <= 3 ? 10 : 1,
							"Discard & damage creature", 		abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_DRAW ){
			ai_modifier += (current_turn == 1-player && current_phase >= PHASE_AFTER_BLOCKING ? 48 : 24);
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_U(1), 0, NULL, NULL);
		}
		if( choice == CHOICE_DISCARD_DAMAGE ){
			ai_modifier += (current_turn == 1-player && current_phase >= PHASE_AFTER_BLOCKING ? 24 : 12);
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_CREATURE");
			if( spell_fizzled != 1 ){
				int selected = -1;
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ANY, "Select a card to discard.");
				if( ! IS_AI(player) ){
					selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &test);
				}
				else{
					int hand_copy[2][hand_count[player]];
					int hc = 0;
					int tou = get_toughness(instance->targets[0].player, instance->targets[0].card);
					int c;
					for(c=0; c<active_cards_count[player]; c++){
						if( in_hand(player, c) && get_cmc(player, c) >= tou ){
							hand_copy[0][hc] = get_original_internal_card_id(player, c);
							hand_copy[1][hc] = c;
							hc++;
						}
					}
					if( ! hc ){
						ai_modifier -= 96;
					}
					else{
						int s1 = select_card_from_zone(player, player, hand_copy[0], hc, 0, AI_MIN_VALUE, -1, &test);
						if( s1 > -1 ){
							selected = hand_copy[1][s1];
						}
					}
				}
				if( selected == -1 ){
					spell_fizzled = 1;
					remove_state(player, card, STATE_TAPPED);
				}
				instance->targets[1].player = get_cmc(player, selected);
				discard_card(player, selected);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DRAW ){
			draw_cards(player, 2);
		}
		if( instance->info_slot == CHOICE_DISCARD_DAMAGE ){
			if( valid_target(&td) ){
				damage_target0(player, card, instance->targets[1].player);
			}
		}
	}

	return 0;
}

int card_new_prahv_guildmage(int player, int card, event_t event){
  /* New Prahv Guildmage |W|U
   * Creature — Human Wizard 2/2
   * {W}{U}: Target creature gains flying until end of turn.
   * {3}{W}{U}: Detain target nonland permanent an opponent controls.
   * 	(Until your next turn, that permanent can't attack or block and its activated abilities can't be activated.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_PERMANENT);
	td2.allowed_controller = 1-player;
	td2.illegal_type = TYPE_LAND;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_FLYING = 1,
		CHOICE_DETAIN
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_UW(1, 1), 0, &td, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XUW(3, 1, 1), 0, &td2, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_UW(1, 1), 0, &td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_XUW(3, 1, 1), 0, &td2, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Give flying", 	abils[1], current_phase < PHASE_DECLARE_BLOCKERS ? 10 : 1,
							"Detain", 		abils[2], current_phase >= PHASE_MAIN2 ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_FLYING ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_UW(1, 1), 0, &td, "TARGET_CREATURE");
		}
		if( choice == CHOICE_DETAIN ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XUW(3, 1, 1), 0,
									&td2, "Select target non-land permanent your opponent controls.");
			if( spell_fizzled != 1 ){
				ai_modifier_for_detain(player, card, 0);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_FLYING && valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FLYING, 0);
		}
		if( instance->info_slot == CHOICE_DETAIN ){
			detain(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_nivix_guildmage(int player, int card, event_t event){
  /* Nivix Guildmage |U|R
   * 	  Creature - Human Wizard 2/2, UR (2)
   * 	  {1}{U}{R}: Draw a card, then discard a card.
   * 	  {2}{U}{R}: Copy target instant or sorcery spell you control. You may choose new targets for the copy. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_INSTANT|TYPE_SORCERY);
	td.allowed_controller = td.preferred_controller = player;

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST_XUR(2, 1, 1), 0, &td, NULL) ){
			return 99;
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_XUR(1, 1, 1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = instance->info_slot = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SPELL_ON_STACK, MANACOST_XUR(2, 1, 1), 0, &td, NULL) ){
			instance->info_slot = 2;
			return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST_XUR(2, 1, 1), 0, &td, NULL);
		}
		else{
			instance->info_slot = 1;
			int total = get_total_available_lands(player);
			int mod = (total <= 3 || total >= 6 ? 24 : 12);
			ai_modifier += (current_turn == 1-player ? mod * 2 : mod);
			return generic_activated_ability(player, card, event, 0, MANACOST_XUR(1, 1, 1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			draw_cards(player, 1);
			discard(player, 0, player);
		}
		if( instance->info_slot == 2 && valid_target(&td) ){
			copy_spell_from_stack(player, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_niv_mizzet_dracogenius(int player, int card, event_t event){
	/* Niv-Mizzet, Dracogenius	|2|U|U|R|R	0x2007785
	 * Legendary Creature - Dragon Wizard 5/5
	 * Flying
	 * Whenever ~ deals damage to a player, you may draw a card.
	 * |U|R: ~ deals 1 damage to any target. */
	check_legend_rule(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_TRIGGER_OPTIONAL);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		draw_cards(player, amount);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_UR(1, 1), 0, &td, "TARGET_ANY");
}

int card_rakdos_charm(int player, int card, event_t event){
  /* Rakdos Charm |B|R
   * Instant
   * Choose one —
   * • Exile all cards from target player's graveyard.
   * • Destroy target artifact.
   * • Each creature deals 1 damage to its controller. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_EXILE_GRAVEYARD = 1,
		CHOICE_KILL_ARTIFACT,
		CHOICE_AUTO_DAMAGE
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return 1;
		}
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td2, NULL, 1, NULL) ){
			return 1;
		}
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {
							0,
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td, NULL, 1, NULL),
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td2, NULL, 1, NULL),
							1
			};
			int priorities[4] = {
								0,
								count_graveyard(1-player),
								5,
								(count_subtype(1-player, TYPE_CREATURE, -1) - count_subtype(player, TYPE_CREATURE, -1)) * 2
			};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Exile a graveyard", 					abils[1], priorities[1],
								"Destroy an artifact", 					abils[2], priorities[2],
								"Each creature damage its controller",	abils[3], priorities[3]);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_EXILE_GRAVEYARD ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		}
		if( instance->info_slot == CHOICE_KILL_ARTIFACT ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td2, "TARGET_ARTIFACT", 1, NULL);
		}
		if( instance->info_slot == CHOICE_AUTO_DAMAGE ){
			return basic_spell(player, card, event);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_EXILE_GRAVEYARD ){
			if( valid_target(&td) ){
				rfg_whole_graveyard(instance->targets[0].player);
			}
		}
		if( instance->info_slot == CHOICE_KILL_ARTIFACT && valid_target(&td2) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( instance->info_slot == CHOICE_AUTO_DAMAGE ){
			APNAP(p,{
						int c;
						for(c=active_cards_count[p]-1; c > -1; c--){
							if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
								damage_player(p, 1, p, c);
							}
						}
					}
			);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_rakdos_ragemutt(int player, int card, event_t event){
/* CARD_ID_RAKDOS_RAGEMUTT	12796
Rakdos Ragemutt	|3|B|R
Creature - Elemental Hound 3/3
Lifelink, haste */

	haste(player, card, event);
	lifelink(player, card, event);
	return 0;
}

int card_rakdos_ringleader(int player, int card, event_t event){
/* CARD_ID_RAKDOS_RINGLEADER	12797
Rakdos Ringleader	|4|B|R
Creature - Skeleton Warrior 3/1
First strike
Whenever ~ deals combat damage to a player, that player discards a card at random.
|B: Regenerate ~. */

	whenever_i_deal_damage_to_a_player_he_discards_a_card(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE, 0);
	return regeneration(player, card, event, MANACOST_B(1));
}

int card_rakdos_lord_of_riots(int player, int card, event_t event){
/* CARD_ID_RAKDOS_LORD_OF_RIOTS	12799
Rakdos, Lord of Riots	|B|B|R|R
Legendary Creature - Demon 6/6
You can't cast ~ unless an opponent lost life this turn.
Flying, trample
Creature spells you cast cost |1 less to cast for each 1 life your opponents have lost this turn. */

	check_legend_rule(player, card, event);

	if( event == EVENT_MODIFY_COST ){
		if( get_trap_condition(1-player, TRAP_LIFE_LOST) < 1 ){
			infinite_casting_cost();
		}
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player &&
		is_what(affected_card_controller, affected_card, TYPE_CREATURE) && ! is_humiliated(player, card, AL_STATIC) )
	{
		COST_COLORLESS-=get_trap_condition(1-player, TRAP_LIFE_LOST);
	}

	return 0;
}

int card_rakdoss_return(int player, int card, event_t event){
	/* Rakdos's Return	|X|B|R	0x200779e
	 * Sorcery
	 * ~ deals X damage to target opponent or planeswalker. That player or that planeswalker's controller discards X cards. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	opponent_or_pw_target_definition(player, card, &td);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_OPPONENT_OR_PLANESWALKER", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier += (instance->info_slot * 12);
		}
		return 0;
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
			new_multidiscard(instance->targets[0].player, instance->info_slot, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_OPPONENT_OR_PLANESWALKER", 1, NULL);
}

int card_righteous_autority(int player, int card, event_t event){
/* CARD_ID_RIGHTEOUS_AUTHORITY	12802
Righteous Authority	|3|W|U
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+1 for each card in its controller's hand.
At the beginning of the draw step of enchanted creature's controller, that player draws an additional card. */

	if( ! is_humiliated(player, card, AL_STATIC) && get_card_instance(player, card)->damage_target_player != -1 ){
		if( current_turn == get_card_instance(player, card)->damage_target_player && event == EVENT_DRAW_PHASE ){
			event_result++;
		}
	}

	return generic_aura(player, card, event, player, hand_count[player], hand_count[player], 0, 0, 0, 0, 0);
}

// Risen sanctuary --> Serra Angel

int card_rites_of_reaping(int player, int card, event_t event){
/* CARD_ID_RITES_OF_REAPING	12804
Rites of Reaping	|4|B|G
Sorcery
Target creature gets +3/+3 until end of turn. Another target creature gets -3/-3 until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 2, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_phase < PHASE_DECLARE_ATTACKERS ? 48 : -48);
		td.preferred_controller = player;
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature (+3/+3).", 1, NULL);
		if( spell_fizzled != 1 ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			td.preferred_controller = 1-player;
			new_pick_target(&td, "Select target creature (-3/-3).", 1, 1 | GS_LITERAL_PROMPT);
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3, 3);
		}
		if( validate_target(player, card, &td, 1) ){
			pump_until_eot(player, card, instance->targets[1].player, instance->targets[1].card, -3, -3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_rix_maadi_guildmage(int player, int card, event_t event){
  /* Rix Maadi Guildmage |B|R
   * Creature — Human Shaman 2/2
   * {B}{R}: Target blocking creature gets -1/-1 until end of turn.
   * {B}{R}: Target player who lost life this turn loses 1 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_BLOCKING;

	target_definition_t td2;
	default_target_definition(player, card, &td2, 0);
	td2.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_M1_M1 = 1,
		CHOICE_LOSE_LIFE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_BR(1, 1), 0, &td, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, 0, MANACOST_BR(1, 1), 0, NULL, NULL) ){
			APNAP(p,{
						if( get_trap_condition(p, TRAP_LIFE_LOST) && would_validate_arbitrary_target(&td2, p, -1) ){
							return 1;
						}
					}
			);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_BR(1, 1), 0, &td, NULL),
						0
		};
		int can_target_player[2] = {0};
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_BR(1, 1), 0, NULL, NULL) ){
			APNAP(p,{
						if( get_trap_condition(p, TRAP_LIFE_LOST) && would_validate_arbitrary_target(&td2, p, -1) ){
							abils[2] = 1;
							can_target_player[p] = 1;
						}
					}
			);
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Give -1/-1 to blocking creature", 	abils[1], 10,
							"Make player lose 1 life", 			abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_M1_M1 ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_BR(1, 1), 0,
											&td, "Select target blocking creature.");
		}
		if( choice == CHOICE_LOSE_LIFE ){
			if( can_target_player[0] && can_target_player[1] ){
				return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_BR(1, 1), 0, &td2, "TARGET_PLAYER");
			}
			else{
				generic_activated_ability(player, card, event, 0, MANACOST_BR(1, 1), 0, NULL, NULL);
				if( spell_fizzled != 1 ){
					if( can_target_player[0] ){
						instance->targets[0].player = 0;
					}
					else{
						instance->targets[0].player = 1;
					}
					instance->targets[0].card = -1;
					instance->number_of_targets = 1;
				}
				if( instance->targets[0].player == player ){
					ai_modifier -= 96;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_M1_M1 && valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, -1, -1);
		}
		if( instance->info_slot == CHOICE_LOSE_LIFE && valid_target(&td2) ){
			lose_life(instance->targets[0].player, 1);
		}
	}

	return 0;
}

int card_search_warrant(int player, int card, event_t event){
/* CARD_ID_SEARCH_WARRANT	12812
Search Warrant	|W|U
Sorcery
Target player reveals his or her hand. You gain life equal to the number of cards in that player's hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			int mod = life[1-player] < 6 ? 12 : 6;
			if( instance->targets[0].player == 1-player ){
				ai_modifier += (hand_count[1-player]*(mod*2));
			}
			else{
				ai_modifier += (hand_count[1-player]*mod);
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			reveal_target_player_hand(instance->targets[0].player);
			gain_life(player, hand_count[instance->targets[0].player]);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_selesnya_charm(int player, int card, event_t event){
	/* Selesnya Charm	|G|W
	 * Instant
	 * Choose one -
	 * * Target creature gets +2/+2 and gains trample until end of turn.
	 * * Exile target creature with power 5 or greater.
	 * * Put a 2/2 |Swhite Knight creature token with vigilance onto the battlefield. */
	if( event == EVENT_CHECK_PUMP && current_phase < PHASE_MAIN2 ){
		return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 2, KEYWORD_TRAMPLE, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.power_requirement = 5 | TARGET_PT_GREATER_OR_EQUAL;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_PUMP = 1,
		CHOICE_EXILE_CRIT,
		CHOICE_KNIGHT
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return 1;
		}
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td2, NULL, 1, NULL) ){
			return 1;
		}
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {
							0,
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td, NULL, 1, NULL),
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td2, NULL, 1, NULL),
							1
			};
			int priorities[4] = {
								0,
								current_phase < PHASE_MAIN2 ? 10 : 1,
								current_phase >= PHASE_MAIN2 ? 10 : 1,
								current_turn == 1-player && current_phase < PHASE_DECLARE_BLOCKERS ? 5 : 1
			};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Give +2/+2 and trample", 				abils[1], priorities[1],
								"Exile a crit with POW >= 5", 			abils[2], priorities[2],
								"Create a Knight",						abils[3], priorities[3]);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_PUMP ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
		if( instance->info_slot == CHOICE_EXILE_CRIT ){
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT,
								&td2, "Select target creature with power 5 or higher.", 1, NULL);
		}
		if( instance->info_slot == CHOICE_KNIGHT ){
			return basic_spell(player, card, event);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_PUMP && valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2, KEYWORD_TRAMPLE, 0);
		}
		if( instance->info_slot == CHOICE_EXILE_CRIT && valid_target(&td2) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		if( instance->info_slot == CHOICE_KNIGHT ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_KNIGHT, &token);
			token.s_key_plus = SP_KEYWORD_VIGILANCE;
			token.pow = token.tou = 2;
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_skull_rend(int player, int card, event_t event){
/* CARD_ID_SKULL_REND	12821
Skull Rend	|3|B|R
Sorcery
~ deals 2 damage to each opponent. Those players each discard two cards at random. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 24;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		damage_player(1-player, 2, player, card);
		new_multidiscard(1-player, 2, DISC_RANDOM, player);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_skymark_roc(int player, int card, event_t event)
{
/* CARD_ID_SKYMARK_ROC	12823
Skymark Roc	|2|W|U
Creature - Bird 3/3
Flying
Whenever ~ attacks, you may return target creature defending player controls with toughness 2 or less to its owner's hand. */

  dat_ability(player, card, event, duh_mode(player) ? DAT_RESOLVE_TRIGGER_MANDATORY : DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
  if (event == EVENT_DAT_ABILITY)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.toughness_requirement = 2 | TARGET_PT_LESSER_OR_EQUAL;
	  td.allowed_controller = 1-player;

	  card_instance_t* instance = get_card_instance(player, card);

	  if (can_target(&td) && pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS"))
		{
		  bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		  instance->number_of_targets = 0;
		}
	}

  return 0;
}

int card_slaughter_games(int player, int card, event_t event){
/* Slaughter Games	|2|B|R
 * Sorcery
 * ~ can't be countered by spells or abilities.
 * Name a nonland card. Search target opponent's graveyard, hand, and library for any number of cards with that name and exile them.
 * Then that player shuffles his or her library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 48;
		state_untargettable(player, card, 1);
		generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
		if( spell_fizzled == 1 ){
			state_untargettable(player, card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		state_untargettable(player, card, 0);
		if( valid_target(&td) ){
			int csvid = -1;
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Name a nonland card.");
			test.type_flag = DOESNT_MATCH;
			if( IS_AI(player) ){
				csvid = ai_name_a_random_csvid(player, card, 1-player, 0, &test);
			}
			else{
				csvid = new_card_from_list(player, 3, &test);
			}
			lobotomy_effect(player, 1-player, csvid, -1, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_sluiceway_scorpion(int player, int card, event_t event){
/* CARD_ID_SLUICEWAY_SCORPION	12827
Sluiceway Scorpion	|2|B|G
Creature - Scorpion 2/2
Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.)
Scavenge |1|B|G (|1|B|G, Exile this card from your graveyard: Put a number of +1/+1 counters equal to this card's power on target creature.
Scavenge only as a sorcery.) */

	deathtouch(player, card, event);
	return scavenge(player, card, event, MANACOST_XBG(1, 1, 1));
}

// spawn of rix maadi --> dark reveler

int card_sphynxs_revelation(int player, int card, event_t event){
/* CARD_ID_SPHINXS_REVELATION	12834
Sphinx's Revelation	|X|W|U|U
Instant
You gain X life and draw X cards. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
		if( spell_fizzled != 1 ){
			int mod = life[player] < 6 ? 12 : 8;
			ai_modifier += (get_card_instance(player, card)->info_slot * mod);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		draw_cards(player, instance->info_slot);
		gain_life(player, instance->info_slot);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_supreme_verdict(int player, int card, event_t event){
/* CARD_ID_SUPREME_VERDICT	12843
Supreme Verdict	|1|W|W|U
Sorcery
~ can't be countered.
Destroy all creatures. */

	cannot_be_countered(player, card, event);
	return card_day_of_judgment(player, card, event);
}

int card_teleportal(int player, int card, event_t event){
/* CARD_ID_TELEPORTAL	12848
Teleportal	|U|R
Sorcery
Target creature you control gets +1/+0 until end of turn and can't be blocked this turn.
Overload |3|U|R (You may cast this spell for its overload cost. If you do, change its text by replacing all instances of "target" with "each.") */

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allowed_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST ){
			if( generic_spell(player, card, event, 0, NULL, NULL, 0, NULL) ){
				if( instance->info_slot == 1 ){
					return 1;
				}
				return can_target(&td);
			}
		}

		if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			int result = 1;
			instance->number_of_targets = 0;
			if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
				result = pay_overload(player, card, MANACOST_XUR(3, 1, 1));
			}
			if( result == 1 ){
				new_pick_target(&td, "Select target creature you control.", 0, 1 | GS_LITERAL_PROMPT);
				if(	spell_fizzled == 1 ){
					return 0;
				}
			}
			instance->info_slot |= result;
		}

		if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot & 2 ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				pump_creatures_until_eot(player, card, player, 0, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE, &this_test);
			}
			else{
				if( valid_target(&td) ){
					pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -2, 0, 0, SP_KEYWORD_UNBLOCKABLE);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return overload(player, card, event, MANACOST_XUR(3, 1, 1));
}

int card_thoughtflare(int player, int card, event_t event){
/* CARD_ID_THOUGHTFLARE	12851
Thoughtflare	|3|U|R
Instant
Draw four cards, then discard two cards. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_LAND, -1) * 8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 4);
		multidiscard(player, 2, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_treasured_find(int player, int card, event_t event){
/* CARD_ID_TREASURED_FIND	12856
Treasured Find	|B|G
Sorcery
Return target card from your graveyard to your hand. Exile ~. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select target card.");

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
		if( spell_fizzled != 1 ){
			card_instance_t *instance = get_card_instance(player, card);
			int iid = get_grave(player)[instance->targets[0].player];
			ai_modifier += cards_ptr[cards_data[iid].id]->ai_base_value;
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected > -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
		kill_card(player, card, KILL_EXILE);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
}

int card_trestle_troll(int player, int card, event_t event){
  /* Trestle Troll	|1|B|G
   * Creature - Troll 1/4
   * Defender
   * Reach (This creature can block creatures with flying.)
   * |1|B|G: Regenerate this creature. */

	return regeneration(player, card, event, MANACOST_XGB(1, 1, 1));
}

int card_trostani_selesnyas_voice(int player, int card, event_t event){

	/* Trostani, Selesnya's Voice	|G|G|W|W
	 * Legendary Creature - Dryad 2/5
	 * Whenever another creature enters the battlefield under your control, you gain life equal to that creature's toughness.
	 * |1|G|W, |T: Populate. */

	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, get_toughness(trigger_cause_controller, trigger_cause));
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	if( event == EVENT_ACTIVATE ){
		target_definition_t td;
		base_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		td.extra = target_predicate_extra(target_is_token_too);
		if( can_target(&td) ){
			ai_modifier += (current_turn == 1-player && current_phase < PHASE_DECLARE_BLOCKERS ? 48 : 24);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		populate(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XGW(1, 1, 1), 0, NULL, NULL);
}

int card_vitu_ghazi_guildmage(int player, int card, event_t event){
	/* Vitu-Ghazi Guildmage	|G|W
	 * Creature - Dryad Shaman 2/2
	 * |4|G|W: Put a 3/3 |Sgreen Centaur creature token onto the battlefield.
	 * |2|G|W: Populate. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_CENTAUR = 1,
		CHOICE_POPULATE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_XGW(2, 1, 1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = { 0, has_mana_for_activated_ability(player, card, MANACOST_XGW(4, 1, 1)), 1 };
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Create a Centaur", 			abils[2], 5,
							"Pupulate", 					abils[1], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_CENTAUR ){
			ai_modifier += (current_turn == 1-player && current_phase < PHASE_DECLARE_BLOCKERS ? 48 : -48);
			return generic_activated_ability(player, card, event, 0, MANACOST_XGW(2, 1, 1), 0, NULL, NULL);
		}
		if( choice == CHOICE_POPULATE ){
			target_definition_t td;
			base_target_definition(player, card, &td, TYPE_CREATURE);
			td.allowed_controller = td.preferred_controller = player;
			td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
			td.extra = target_predicate_extra(target_is_token_too);
			if( can_target(&td) ){
				ai_modifier += (current_turn == 1-player && current_phase < PHASE_DECLARE_BLOCKERS ? 48 : 24);
			}
			return generic_activated_ability(player, card, event, 0, MANACOST_XGW(4, 1, 1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_CENTAUR ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_CENTAUR);
		}
		if( instance->info_slot == CHOICE_POPULATE ){
			populate(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

static int is_damaging_this_planeswalker(card_instance_t* damage, int pw_player, int pw_card)
{
  // Must be damaging either the planeswalker's player or the planeswalker directly.
  if (damage->damage_target_player != pw_player)
	return 0;

  // Damage card already targeting the planeswalker directly?
  if (damage->damage_target_card == pw_card)
	return 1;

  // Then must be damaging the player.
  if (damage->damage_target_card != -1)
	return 0;

  int cr_player = damage->damage_source_player;
  int cr_card = damage->damage_source_card;

  // With the Attacking-A-Planeswalker bit.
  if (!check_special_flags(cr_player, cr_card, SF_ATTACKING_PWALKER))
	return 0;

  // And must have exactly one attacking-a-planeswalker legacy attached, with targets[1].card == pw_card.
  card_instance_t* effect;
  int c;
  for (c = 0; c < active_cards_count[cr_player]; ++c)
	if ((effect = in_play(cr_player, c))
		&& effect->damage_target_player == cr_player
		&& effect->damage_target_card == cr_card
		&& effect->internal_card_id == LEGACY_EFFECT_CUSTOM
		&& check_special_flags(cr_player, c, SF_ATTACKING_PWALKER))
	  return effect->targets[1].card == pw_card;

  return 0;
}

static int vraska_legacy(int player, int card, event_t event)
{
  card_instance_t* damage = combat_damage_being_dealt(event), *instance;
  if (damage
	  && (damage->targets[3].player & TYPE_CREATURE)
	  && (instance = get_card_instance(player, card))->number_of_targets < 19
	  && is_damaging_this_planeswalker(damage, instance->damage_source_player, instance->damage_source_card))
	{
	  instance->targets[instance->number_of_targets].player = damage->damage_source_player;
	  instance->targets[instance->number_of_targets].card = damage->damage_source_card;
	  ++instance->number_of_targets;
	}

  if (trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player
	  && (instance = get_card_instance(player, card))->number_of_targets > 0)
	{
	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_MANDATORY;

	  if (event == EVENT_RESOLVE_TRIGGER)
		{
		  int i;
		  for (i = 0; i < instance->number_of_targets; ++i)
			if (in_play(instance->targets[i].player, instance->targets[i].card))
			  kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);

		  instance->number_of_targets = 0;
		}
	}

  if (event == EVENT_BEGIN_TURN && current_turn == player)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

int card_vraska_the_unseen(int player, int card, event_t event){
/* Vraska the Unseen	|3|B|G
 * Planeswalker - Vraska (5)
 * +1: Until your next turn, whenever a creature deals combat damage to ~, destroy that creature.
 * -3: Destroy target nonland permanent.
 * -7: Put three 1/1 |Sblack Assassin creature tokens onto the battlefield with "Whenever this creature deals combat damage to a player,
	that player loses the game." */
	check_legend_rule(player, card, event);

	if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION)
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_LAND;

		card_instance_t *instance = get_card_instance(player, card);

		int ai_first_vraska_ability = 15;
		if(player == AI || ai_is_speculating == 1){
			ai_first_vraska_ability -= count_counters(player, card, COUNTER_LOYALTY);
			ai_first_vraska_ability -= count_subtype(player, TYPE_CREATURE, -1)*2;
			ai_first_vraska_ability += count_subtype(1-player, TYPE_CREATURE, -1)*3;
			if( ai_first_vraska_ability < 5 )
				ai_first_vraska_ability = 5;
		}

		enum {
			CHOICE_DIE_IF_TOUCH_ME = 1,
			CHOICE_KILL_PERMANENT,
			CHOICE_ASSASSINS
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Who damages Vraska dies", 1, ai_first_vraska_ability, 1,
						"Destroy a nonland", can_target(&td), 15, -3,
						"Unleash 3 Assassins", 1, 10+(15 * !check_battlefield_for_subtype(1-player, TYPE_CREATURE, -1)), -7);
		if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;	// else fall through to planeswalker()
		}

		else if (event == EVENT_ACTIVATE)
		{
			if( choice == CHOICE_KILL_PERMANENT ){
				instance->number_of_targets = 0;
				pick_target(&td, "TARGET_NONLAND_PERMANENT");
			}
		}

		else{	// event == EVENT_RESOLVE_ACTIVATION
			if( choice == CHOICE_DIE_IF_TOUCH_ME ){
				int legacy = create_legacy_effect(player, instance->parent_card, &vraska_legacy);
				card_instance_t* leg = get_card_instance(player, legacy);
				leg->number_of_targets = 0;
			}
			else if( choice == CHOICE_KILL_PERMANENT ){
					if (valid_target(&td)){
					  kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
					}
			}
			else if( choice == CHOICE_ASSASSINS ){
					token_generation_t token;
					default_token_definition(player, card, CARD_ID_ASSASSIN, &token);
					token.pow = token.tou = 1;
					token.qty = 3;
					token.special_infos = 66;
					generate_token(&token);
			}
		}
	}

	return planeswalker(player, card, event, 5);
}

int card_wayfaring_temple(int player, int card, event_t event)
{
  /* Wayfaring Temple	|1|G|W
   * Creature - Elemental * / *
   * ~'s power and toughness are each equal to the number of creatures you control.
   * Whenever this creature deals combat damage to a player, populate. (Create a token that's a copy of a creature token you control.) */

  unsigned int damage_triggers;
  int result;

  damage_triggers =
      whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
  if (damage_triggers != 0)
  {
    for (result = (damage_triggers >> 8 & 0xff) + (damage_triggers & 0xff); result != 0;
         result = result + -1)
    {
      populate(player, card);
    }
  }
  if (((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    result = count_subtype(player, 2, -1);
    set_characteristic_pt(result);
  }
  return 0;
}

// Hybrid
int card_azors_elocutors(int player, int card, event_t event)
{
  /* Azor's Elocutors	|3|WU|WU
   * Creature - Human Advisor 3/5
   * At the beginning of your upkeep, put a filibuster counter on ~. Then if ~ has five or more filibuster counters on it, you win the game.
   * Whenever a source deals damage to you, remove a filibuster counter from ~. */

  hybrid(player, card, event);
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY || event == EVENT_SHOULD_AI_PLAY)
  {
    add_counters(player, card, player, card, COUNTER_FILIBUSTER, 1);
    if (count_counters(player, card, COUNTER_FILIBUSTER) > 4)
      lose_the_game(1 - player);
    if (event == EVENT_SHOULD_AI_PLAY)
    {
      int counters = count_counters(player, card, COUNTER_FILIBUSTER);
      int bonus = counters * counters * counters;
      if (player == 1)
        ai_modifier += bonus * 8;
      else
        ai_modifier += bonus * -8;
    }
  }
  card_instance_t *damage = damage_being_dealt(event);
  if (damage && damage->damage_target_card == -1 && damage->damage_target_player == player &&
      !damage_is_to_planeswalker(damage))
    ++get_card_instance(player, card)->info_slot;
  if (trigger_condition == TRIGGER_DEAL_DAMAGE && card == affected_card &&
      player == affected_card_controller && reason_for_trigger_controller == player)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (event == EVENT_TRIGGER)
    {
      if (instance->info_slot > 0)
        event_result |= 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      remove_counters(player, card, player, card, COUNTER_FILIBUSTER, instance->info_slot);
      instance->info_slot = 0;
    }
  }
  return 0;
}

int card_blistercoil_weird(int player, int card, event_t event){
/* CARD_ID_BLISTERCOIL_WEIRD	12669
Blistercoil Weird	|UR
Creature - Weird 1/1
Whenever you cast an instant or sorcery spell, ~ gets +1/+1 until end of turn. Untap it. */

	hybrid(player, card, event);
	sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_SPELL, NULL, player, card);
	if (event == EVENT_SPA){
		pump_until_eot_merge_previous(player, card, player, card, 1, 1);
		untap_card(player, card);
	}
	return 0;
}

int card_cryptborn_horror(int player, int card, event_t event){
/* CARD_ID_CRYPTBORN_HORROR	12694
Cryptborn Horror	|1|BR|BR
Creature - Horror 0/0
Trample
~ enters the battlefield with X +1/+1 counters on it, where X is the total life lost by your opponents this turn. */

	hybrid(player, card, event);

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, get_trap_condition(1-player, TRAP_LIFE_LOST));

	return 0;
}

int card_deathrite_shaman(int player, int card, event_t event)
{
  /* Deathrite Shaman	|BG
   * Creature - Elf Shaman 1/2
   * |T: Exile target land card from a graveyard. Add one mana of any color to your mana pool.
   * |B, |T: Exile target instant or sorcery card from a graveyard. Each opponent loses 2 life.
   * |G, |T: Exile target creature card from a graveyard. You gain 2 life. */

	// The first ability isn't a mana ability, since it has a target.  Therefore it's also not "tapping to produce mana" for e.g. Mana Reflection.

	hybrid(player, card, event);

#define CAN_PRODUCE_MANA(player, card) (CAN_TAP(player, card) && CAN_ACTIVATE0(player, card) && any_in_graveyard_by_type(ANYBODY, TYPE_LAND))

	if (event == EVENT_COUNT_MANA && affect_me(player, card) && CAN_PRODUCE_MANA(player, card) )
		declare_mana_available_hex(player, COLOR_TEST_ANY_COLORED, 1);

	if (!IS_ACTIVATING(event))
		return 0;

	enum{
		CHOICE_CANCEL = 0,
		CHOICE_MANA = 1,
		CHOICE_LOSE_LIFE = 2,
		CHOICE_GAIN_LIFE = 3
	} choice = CHOICE_CANCEL;

	if (event == EVENT_CAN_ACTIVATE){
		if (!can_use_activated_abilities(player, card) || !CAN_TAP(player, card) || graveyard_has_shroud(ANYBODY))
			return 0;

		if (paying_mana() && has_mana_for_activated_ability(player, card, MANACOST0) && any_in_graveyard_by_type(ANYBODY, TYPE_LAND))
			choice = CHOICE_MANA;
	}

	if (choice == CHOICE_CANCEL){
		int abils[3] = {any_in_graveyard_by_type(ANYBODY, TYPE_LAND),
						any_in_graveyard_by_type(ANYBODY, TYPE_SPELL) && has_mana_for_activated_ability(player, card, MANACOST_B(CAN_PRODUCE_MANA(player, card) ? 2 : 1)),
						any_in_graveyard_by_type(ANYBODY, TYPE_CREATURE) && has_mana_for_activated_ability(player, card, MANACOST_G(CAN_PRODUCE_MANA(player, card) ? 2 : 1))
		};
		choice = DIALOG(player, card, event, DLG_RANDOM,
						"Produce mana", abils[0], -1,
						"Lose life", abils[1], 20-life[1-player],
						"Gain life", abils[2], 20-life[player]);

	}
	if (event == EVENT_CAN_ACTIVATE)
		return choice;
	else if (event == EVENT_ACTIVATE){
			if( choice == CHOICE_CANCEL ){
				spell_fizzled = 1;
				return 0;
			}
			test_definition_t test;
			add_state(player, card, STATE_TAPPED);
			if( charge_mana_for_activated_ability(player, card, 0, (choice == CHOICE_LOSE_LIFE ? 1 : 0), 0,	(choice == CHOICE_GAIN_LIFE ? 1 : 0), 0, 0) ){
				switch (choice)
				{
					case CHOICE_CANCEL:
						return 0;

					case CHOICE_MANA:
						new_default_test_definition(&test, TYPE_LAND, "Select target land card.");
						break;

					case CHOICE_LOSE_LIFE:
						new_default_test_definition(&test, TYPE_SPELL, "Select target instant or sorcery card.");
						break;

					case CHOICE_GAIN_LIFE:
						new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");
						break;
				}

				if (select_target_from_either_grave(player, card, 0, AI_MIN_VALUE, AI_MAX_VALUE, &test, 0, 1) == -1){
					remove_state(player, card, STATE_TAPPED);
				}
			}
			else{
				remove_state(player, card, STATE_TAPPED);
			}
            tapped_for_mana_color = -1;
	}
  else	// event == EVENT_RESOLVE_ACTIVATION
	{
	  int whose_graveyard = get_card_instance(player, card)->targets[0].player;

	  int selected = validate_target_from_grave_source(player, card, whose_graveyard, 1);
	  if (selected == -1)
		{
		  spell_fizzled = 1;
		  return 0;
		}

	  rfg_card_from_grave(whose_graveyard, selected);
	  switch (choice)
		{
		  case CHOICE_CANCEL:
			return 0;

		  case CHOICE_MANA:
			FORCE(produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1));
			break;

		  case CHOICE_LOSE_LIFE:
			lose_life(1-player, 2);
			break;

		  case CHOICE_GAIN_LIFE:
			gain_life(player, 2);
			break;
		}
	}

  return 0;
}

int card_dryad_militant(int player, int card, event_t event)
{
  /* Dryad Militant	|GW
   * Creature - Dryad Soldier 2/1
   * If an instant or sorcery card would be put into a graveyard from anywhere, exile it instead. */

	hybrid(player, card, event);

	enable_xtrigger_flags |= ENABLE_XTRIGGER_REPLACE_MILL | ENABLE_XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY;
	if( event == EVENT_GRAVEYARD_FROM_PLAY || xtrigger_condition() == XTRIGGER_REPLACE_MILL ||
		xtrigger_condition() == XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY)
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_SPELL, "");

		if_a_card_would_be_put_into_graveyard_from_anywhere_exile_it_instead(player, card, event, ANYBODY, &test);
	}

	return 0;
}

int card_frostburn_weird(int player, int card, event_t event){
/* CARD_ID_FROSTBURN_WEIRD	12726
Frostburn Weird	|UR|UR
Creature - Weird 1/4
|UR: ~ gets +1/-1 until end of turn. */

	hybrid(player, card, event);

	if( event == EVENT_POW_BOOST ){
		int i = 0;
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_U(1));
		while( has_mana_hybrid(player, i, COLOR_BLUE, COLOR_RED, c1) ){
				i++;
		}
		return i;
	}

	if( event == EVENT_TOU_BOOST ){
		int i = 0;
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_U(1));
		while( has_mana_hybrid(player, i, COLOR_BLUE, COLOR_RED, c1) ){
				i++;
		}
		return -i;
	}

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_U(1));
		if( has_mana_hybrid(player, 1, COLOR_BLUE, COLOR_RED, c1) ){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_U(1));
		charge_mana_hybrid(player, card, 1, COLOR_BLUE, COLOR_RED, c1);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade(player, card, event, 0, MANACOST_U(1), 1, -1, 0, 0);
	}

	return 0;
}

int card_golgari_longlegs(int player, int card, event_t event){
/* CARD_ID_GOLGARI_LONGLEGS	12735
Golgari Longlegs	|3|BG|BG
Creature - Insect 5/4 */

	hybrid(player, card, event);

	return 0;
}

int card_growing_ranks(int player, int card, event_t event){
/* CARD_ID_GROWING_RANKS	12741
Growing Ranks	|2|GW|GW
Enchantment
At the beginning of your upkeep, populate. (Create a token that's a copy of a creature token you control.) */

	hybrid(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		populate(player, card);
	}

	return global_enchantment(player, card, event);
}

int card_judges_familiar(int player, int card, event_t event){
	/* Judge's Familiar	|WU
	 * Creature - Bird 1/1
	 * Flying
	 * Sacrifice ~: Counter target instant or sorcery spell unless its controller pays |1. */

	hybrid(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_SPELL);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		counterspell_resolve_unless_pay_x(player, card, &td, 0, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_SPELL_ON_STACK, MANACOST0, 0, &td, NULL);
}

int card_nivmagus_elemental(int player, int card, event_t event){
/* CARD_ID_NIVMAGUS_ELEMENTAL	12779
Nivmagus Elemental	|UR
Creature - Elemental 1/2
Exile an instant or sorcery spell you control: Put two +1/+1 counters on ~. (That spell won't resolve.) */

	hybrid(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_SPELL);
	td.allowed_controller = td.preferred_controller = player;
	td.illegal_abilities = 0;

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST0, 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			put_on_top_of_deck(card_on_stack_controller, card_on_stack);;
			rfg_top_card_of_deck(card_on_stack_controller);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 2);
	}

	return 0;
}

int card_rakdos_cackler(int player, int card, event_t event){
/* CARD_ID_RAKDOS_CACKLER	12792
Rakdos Cackler	|BR
Creature - Devil 1/1
Unleash (You may have this creature enter the battlefield with a +1/+1 counter on it. It can't block as long as it has a +1/+1 counter on it.) */

	hybrid(player, card, event);

	return unleash(player, card, event);
}

int card_rakdos_shred_freak(int player, int card, event_t event){
/* CARD_ID_RAKDOS_SHRED_FREAK	12798
Rakdos Shred-Freak	|BR|BR
Creature - Human Berserker 2/1
Haste */

	haste(player, card, event);

	return hybrid(player, card, event);
}

int card_slitherhead(int player, int card, event_t event){
/* CARD_ID_SLITHERHEAD	12826
Slitherhead	|BG
Creature - Plant Zombie 1/1
Scavenge |0 (|0, Exile this card from your graveyard: Put a number of +1/+1 counters equal to this card's power on target creature. Scavenge only as a sorcery.)*/

	hybrid(player, card, event);

	return scavenge(player, card, event, MANACOST0);
}

int card_sundering_growth(int player, int card, event_t event){
/* CARD_ID_SUNDERING_GROWTH	12841
Sundering Growth	|GW|GW
Instant
Destroy target artifact or enchantment, then populate. (Create a token that's a copy of a creature token you control.) */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( hybrid_casting(player, card, 0) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			populate(player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

// vassal soul --> golgari longlegs

// Artifacts

int card_azorius_keyrune(int player, int card, event_t event){
/* CARD_ID_AZORIUS_KEYRUNE	12664
Azorius Keyrune	|3
Artifact
|T: Add |W or |U to your mana pool.
|W|U: ~ becomes a 2/2 |Swhite and |Sblue Bird artifact creature with flying until end of turn. */

	return keyrune(player, card, event, MANACOST_UW(1, 1), 2, 2, KEYWORD_FLYING, 0, COLOR_TEST_BLUE | COLOR_TEST_WHITE, SUBTYPE_BIRD);
}

int card_chromatic_lantern(int player, int card, event_t event){
/* CARD_ID_CHROMATIC_LANTERN	12681
Chromatic Lantern	|3
Artifact
Lands you control have "|T: Add one mana of any color to your mana pool."
|T: Add one mana of any color to your mana pool. */

	if (event == EVENT_MODIFY_MANA_PROD && !is_humiliated(player, card, AL_STATIC)){
		mana_colors_added[player] |= COLOR_TEST_ANY_COLORED;
	}
	return plain_mana_producer(player, card, event);
}

int card_civic_saber(int player, int card, event_t event){
/* CARD_ID_CIVIC_SABER	12683
Civic Saber	|1
Artifact - Equipment
Equipped creature gets +1/+0 for each of its colors.
Equip |1 */

	if( in_play(player, card) && is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_POWER && affect_me(p, c) ){
			event_result += count_colors(p, c);
		}
	}

	return basic_equipment(player, card, event, 1);
}

int card_codex_shredder(int player, int card, event_t event){
  /* Codex Shredder |1
   * Artifact
   * {T}: Target player puts the top card of his or her library into his or her graveyard.
   * {5}, {T}, Sacrifice Codex Shredder: Return target card from your graveyard to your hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select target card.");

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MILL = 1,
		CHOICE_REGROWTH
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			return 1;
		}
		return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(5), 0,
																NULL, NULL, 0, &test, GS_GRAVE_RECYCLER);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL),
						generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED |GAA_SACRIFICE_ME,
																		MANACOST_X(5), 0, NULL, NULL, 0, &test, GS_GRAVE_RECYCLER)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Mill a player", 						abils[1], 1,
							"Sac and return a card from graveyard", abils[2], 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_MILL ){
			ai_modifier += 24;
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
		}
		if( choice == CHOICE_REGROWTH ){
			generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED |GAA_SACRIFICE_ME, MANACOST_X(5), 0,
															NULL, NULL, 0, &test, GS_GRAVE_RECYCLER);
			if( spell_fizzled != 1 ){
				int iid = get_grave(player)[instance->targets[0].player];
				ai_modifier += cards_ptr[cards_data[iid].id]->ai_base_value;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_MILL && valid_target(&td) ){
			mill(instance->targets[0].player, 1);
		}
		if( instance->info_slot == CHOICE_REGROWTH ){
			int selected = validate_target_from_grave_source(player, card, player, 0);
			if( selected > -1 ){
				from_grave_to_hand(player, selected, TUTOR_HAND);
			}
		}
	}

	return 0;
}

int card_counterflux(int player, int card, event_t event){
  /* Counterflux |U|U|R
   * 	  Instant
   * 	  Counterflux can't be countered by spells or abilities.
   * 	  Counter target spell you don't control.
   * 	  Overload {1}{U}{U}{R} (You may cast this spell for its overload cost.
   * 							  If you do, change its text by replacing all instances of "target" with "each.") */

	//Overload won't be coded as it's worthless under Manalink (no real stack system)

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);
	td.allowed_controller = player;

	if( event == EVENT_CAN_CAST ){
		return counterspell(player, card, event, &td, 0);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		add_state(player, card, STATE_CANNOT_TARGET);
		return counterspell(player, card, event, &td, 0);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		remove_state(player, card, STATE_CANNOT_TARGET);
		return counterspell(player, card, event, &td, 0);
	}

	return 0;
}

int card_golgari_keyrune(int player, int card, event_t event){
/* CARD_ID_GOLGARI_KEYRUNE	12734
Golgari Keyrune	|3
Artifact
|T: Add |B or |G to your mana pool.
|B|G: ~ becomes a 2/2 |Sblack and |Sgreen Insect artifact creature with deathtouch until end of turn. */

	return keyrune(player, card, event, MANACOST_BG(1, 1), 2, 2, 0, SP_KEYWORD_DEATHTOUCH, COLOR_TEST_BLACK | COLOR_TEST_GREEN,
					SUBTYPE_INSECT);
}

int card_izzet_keyrune(int player, int card, event_t event){
/* CARD_ID_IZZET_KEYRUNE	12755
Izzet Keyrune	|3
Artifact
|T: Add |U or |R to your mana pool.
|U|R: Until end of turn, ~ becomes a 2/1 |Sblue and |Sred Elemental artifact creature.
Whenever ~ deals combat damage to a player, you may draw a card. If you do, discard a card. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, RESOLVE_TRIGGER_AI(player) | DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		while( amount ){
				draw_a_card(player);
				discard(player, 0, player);
				amount--;
		}
	}
	return keyrune(player, card, event, MANACOST_UR(1, 1), 2, 1, 0, 0, COLOR_TEST_BLUE | COLOR_TEST_RED, SUBTYPE_ELEMENTAL);
}

int card_rakdos_keyrune(int player, int card, event_t event){
/* CARD_ID_RAKDOS_KEYRUNE	12795
Rakdos Keyrune	|3
Artifact
|T: Add |B or |R to your mana pool.
|B|R: ~ becomes a 3/1 |Sblack and |Sred Devil artifact creature with first strike until end of turn. */

	return keyrune(player, card, event, MANACOST_BR(1, 1), 3, 1, KEYWORD_FIRST_STRIKE, 0, COLOR_TEST_BLACK | COLOR_TEST_RED, SUBTYPE_DEVIL);
}

int card_selesnya_keyrune(int player, int card, event_t event){
/* CARD_ID_SELESNYA_KEYRUNE	12816
Selesnya Keyrune	|3
Artifact
|T: Add |G or |W to your mana pool.
|G|W: ~ becomes a 3/3 |Sgreen and |Swhite Wolf artifact creature until end of turn. */

	return keyrune(player, card, event, MANACOST_GW(1, 1), 3, 3, 0, 0, COLOR_TEST_WHITE | COLOR_TEST_GREEN, SUBTYPE_WOLF);
}

int card_street_sweeper(int player, int card, event_t event)
{
/* CARD_ID_STREET_SWEEPER	12840
Street Sweeper	|6
Artifact Creature - Construct 4/6
Whenever ~ attacks, destroy all Auras attached to target land. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_LAND);
	  td.allow_cancel = 0;

	  card_instance_t* instance = get_card_instance(player, card);

	  if (can_target(&td) && pick_target(&td, "TARGET_LAND"))
		{
		  // Enchantments don't seem to go through a dying/graveyard-order stage like other permanents, so mark/sweep.
		  int marked[2][151] = {{0}};
		  card_instance_t* aura;
		  int p, c;
		  for (p = 0; p <= 1; ++p)
			for (c = 0; c < active_cards_count[p]; ++c)
			  if ((aura = in_play(p, c)) && is_what(p, c, TYPE_ENCHANTMENT) && has_subtype(p, c, SUBTYPE_AURA)
				  && aura->damage_target_player == instance->targets[0].player && aura->damage_target_card == instance->targets[0].card)
				marked[p][c] = 1;

		  for (p = 0; p <= 1; ++p)
			for (c = 0; c < active_cards_count[p]; ++c)
			  if (marked[p][c] && in_play(p, c))
				kill_card(p, c, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_tablet_of_the_guilds(int player, int card, event_t event){

	/* Tablet of the Guilds	|2
	 * Artifact
	 * As ~ enters the battlefield, choose two colors.
	 * Whenever you cast a spell, if it's at least one of the chosen colors, you gain 1 life for each of the chosen colors it is. */

	if( event == EVENT_RESOLVE_SPELL ){
		int clrs = 0;
		if( !IS_AI(player) ){
			int c1 = choose_a_color(player, 0);
			create_card_name_legacy(player, card, CARD_ID_BLACK+(c1-1));
			clrs = 1 << c1;
			c1 = choose_a_color_from(player, 1, COLOR_TEST_ANY_COLORED & ~clrs);
			create_card_name_legacy(player, card, CARD_ID_BLACK+(c1-1));
			clrs |= 1 << c1;
		}
		else{
			int c1 = choose_a_color_and_show_legacy(player, card, player, -1);
			clrs = 1 << c1;
			int c2 = c1+1;
			if( c2 > 5 ){
				c2 = c1-1;
			}
			clrs |= 1 << c2;
			create_card_name_legacy(player, card, CARD_ID_BLACK+(c2-1));
		}
		get_card_instance(player, card)->info_slot = clrs;
	}

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		int stored_colors = get_card_instance(player, card)->info_slot;
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = stored_colors;
		sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
	}

	if (event == EVENT_SPA){
		int stored_colors = get_card_instance(player, card)->info_slot;
		gain_life(player, num_bits_set(get_color(trigger_cause_controller, trigger_cause) & stored_colors));
	}
	return 0;
}

int card_volatile_rig(int player, int card, event_t event){
/* CARD_ID_VOLATILE_RIG	12869
Volatile Rig	|4
Artifact Creature - Construct 4/4
Trample
~ attacks each turn if able.
Whenever ~ is dealt damage, flip a coin. If you lose the flip, sacrifice ~.
When ~ dies, flip a coin. If you lose the flip, it deals 4 damage to each creature and each player. */

	attack_if_able(player, card, event);

	if( !is_humiliated(player, card, AL_TRIG) && damage_dealt_to_me_arbitrary(player, card, event, 0, player, card) ){
		if( ! flip_a_coin(player, card) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		if( ! flip_a_coin(player, card) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			new_damage_all(player, card, ANYBODY, 4, NDA_PLAYER_TOO, &this_test);
		}
	}

	return 0;
}

// Lands
int card_azorius_guildgate(int player, int card, event_t event){
/* CARD_ID_AZORIUS_GUILDGATE	12662
Azorius Guildgate	""
Land - Gate
~ enters the battlefield tapped.
|T: Add |W or |U to your mana pool. */

	// Also code for other "gate" lands.
	comes_into_play_tapped(player, card, event);
	return mana_producer(player, card, event);
}

int card_grove_of_the_guardian(int player, int card, event_t event){
/* Grove of the Guardian	""
 * Land
 * |T: Add |1 to your mana pool.
 * |3|G|W, |T, Tap two untapped creatures you control, Sacrifice ~: Put an 8/8 |Sgreen and |Swhite Elemental creature token with
		vigilance onto the battlefield. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_ELEMENTAL
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability_with_tapsubtype_as_cost(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME,
																MANACOST_XGW(4, 1, 1), 0, NULL, NULL, &td, 2) )
		{
			choice = CHOICE_ELEMENTAL;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_ELEMENTAL && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Create an Elemental", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_ELEMENTAL ){
			ai_modifier += (current_turn == 1-player && current_phase < PHASE_DECLARE_BLOCKERS ? 48 : -48);
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability_with_tapsubtype_as_cost(player, card, event, GAA_SACRIFICE_ME, MANACOST_XGW(3, 1, 1), 0, NULL, NULL,
																&td, 2);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_ELEMENTAL ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
			token.pow = token.tou = 8;
			token.s_key_plus = SP_KEYWORD_VIGILANCE;
			token.color_forced = COLOR_TEST_WHITE | COLOR_TEST_GREEN;
			generate_token(&token);
		}
	}

	return 0;
}

int card_rogues_passage(int player, int card, event_t event){
/* CARD_ID_ROGUES_PASSAGE	12806
Rogue's Passage	""
Land
|T: Add |C to your mana pool.
|4, |T: Target creature can't be blocked this turn. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_GIVE_UNBLOCKABLE
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(5), 0, &td, NULL) )
		{
			choice = CHOICE_GIVE_UNBLOCKABLE;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_GIVE_UNBLOCKABLE && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Give 'Unblockable' to a creature", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_GIVE_UNBLOCKABLE ){
			ai_modifier += (current_turn == player && current_phase < PHASE_DECLARE_BLOCKERS ? 48 : -48);
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(4), 0, &td, "TARGET_CREATURE");
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_GIVE_UNBLOCKABLE && valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
		}
	}

	return 0;
}

int card_transguild_promenade(int player, int card, event_t event){

	/* Transguild Promenade	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, sacrifice it unless you pay |1.
	 * |T: Add one mana of any color to your mana pool. */

	/* Rupture Spire	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, sacrifice it unless you pay |1.
	 * |T: Add one mana of any color to your mana pool. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if (event == EVENT_ETB_ABILITY && !charge_mana_while_resolving(player, card, 0, player, COLOR_COLORLESS, 1)){
		ai_modifier += player == AI ? -128 : 128;
		kill_card(player, card, KILL_SACRIFICE);
	}

	return plain_mana_producer_tapped(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x245c180; original cards/return_to_ravnica.c:94. */

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x245c0c0; original cards/return_to_ravnica.c:79. */

#endif
