#include "manalink.h"

static int autumns_veil_effect(int player, int card, event_t event);

// global functions
static int empire_set(int player){

	int score = 0;
	int count = 0;
	while( count < active_cards_count[player] ){
			if( in_play(player, count) ){
				if( get_card_name(player, count) == CARD_ID_CROWN_OF_EMPIRES ){
					score |=1;
				}
				else if( get_card_name(player, count) == CARD_ID_SCEPTER_OF_EMPIRES ){
						 score |=2;
				}
				else if( get_card_name(player, count) == CARD_ID_THRONE_OF_EMPIRES ){
						 score |=4;
				}
			}
			if( score == 7 ){
				return 1;
			}
			count++;
	}
	return 0;
}

// cards
int card_adaptive_automaton(int player, int card, event_t event){
/* CARD_ID_ADAPTIVE_AUTOMATON	11771
Adaptive Automaton	|3
Artifact Creature - Construct 2/2
As ~ enters the battlefield, choose a creature type.
~ is the chosen type in addition to its other types.
Other creatures you control of the chosen type get +1/+1. */

	if (event == EVENT_RESOLVE_SPELL){
		if (player == AI){ state_untargettable(player, card, 1); }
		card_instance_t* instance = get_card_instance(player, card);
		instance->info_slot = select_a_subtype(player, card);
		if (player == AI){ state_untargettable(player, card, 0); }
	}else if (event == EVENT_CHANGE_TYPE && affect_me(player, card)){
		add_a_subtype(player, card, get_card_instance(player, card)->info_slot);
	}else if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && !affect_me(player, card) && !is_humiliated(player, card, AL_STATIC)){
		if (is_creature_type(get_card_instance(player, card)->info_slot)){
			int subtype = get_card_instance(player, card)->info_slot;
			if (subtype > 0 && in_play(affected_card_controller, affected_card) && affected_card_controller == player
				&& has_subtype(affected_card_controller, affected_card, subtype)){
				++event_result;
			}
		}
	}
	return 0;
}

int card_angelic_destiny(int player, int card, event_t event){
/* CARD_ID_ANGELIC_DESTINY	11775
Angelic Destiny	|2|W|W
Enchantment - Aura
Enchant creature
Enchanted creature gets +4/+4, has flying and first strike, and is an Angel in addition to its other types.
When enchanted creature dies, return ~ to its owner's hand. */

	when_enchanted_permanent_dies_return_aura_to_hand(player, card, event);

	return generic_aura(player, card, event, player, 4, 4, KEYWORD_FLYING | KEYWORD_FIRST_STRIKE, 0, SUBTYPE_ANGEL, 0, GA_ADD_SUBTYPE);
}

static int aegis_angel_effect(int player, int card, event_t event)
{

  if (get_card_instance(player, card)->damage_target_player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int p = instance->damage_target_player;
    int c = instance->damage_target_card;
    indestructible(p, c, event);
  }

  if (get_card_instance(player, card)->targets[1].player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);
    other_leaves_play(player, card, instance->targets[1].player, instance->targets[1].card, event,
                      0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      kill_card(player, card, KILL_EXILE);
    }
    if (event == EVENT_CARDCONTROLLED &&
        affect_me(instance->targets[1].player, instance->targets[1].card))
    {
      kill_card(player, card, KILL_EXILE);
    }
  }

  return 0;
}

int card_aegis_angel(int player, int card, event_t event){
/* CARD_ID_AEGIS_ANGEL	11772
Aegis Angel	|4|W|W
Creature - Angel 5/5
Flying (This creature can't be blocked except by creatures with flying or reach.)
When ~ enters the battlefield, another target permanent gains indestructible for as long as you control ~. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;
		td.preferred_controller = player;
		td.special = TARGET_SPECIAL_NOT_ME;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select another target permanent.", 0, GS_LITERAL_PROMPT) ){
			int legacy = create_targetted_legacy_effect(player, card, &aegis_angel_effect,
														instance->targets[0].player, instance->targets[0].card);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[1].player = player;
			leg->targets[1].card = card;
			leg->number_of_targets = 2;
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_alabaster_mage(int player, int card, event_t event)
{
  /* Alabaster Mage	|1|W
	 * Creature - Human Wizard 2/1
	 * |1|W: Target creature you control gains lifelink until end of turn. */

if (IS_GAA_EVENT(event))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.allowed_controller = td.preferred_controller = player;
    if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
    {
      card_instance_t* instance = get_card_instance(player, card);
      pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
          0, 0, 0, SP_KEYWORD_LIFELINK);
    }
    return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XW(1, 1), 0, &td, "TARGET_CREATURE");
  }
  return 0;
}

int card_angels_mercy(int player, int card, event_t event){
/* CARD_ID_ANGELS_MERCY	10383
Angel's Mercy	|2|W|W
Instant
You gain 7 life. */

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, 7);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_arbalest_elite(int player, int card, event_t event){
/* CARD_ID_ARBALEST_ELITE	11778
Arbalest Elite	|2|W|W
Creature - Human Archer 2/3
|2|W, |T: ~ deals 3 damage to target attacking or blocking creature. ~ doesn't untap during your next untap step. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_IN_COMBAT;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, instance->parent_card);
			if (in_play(instance->parent_controller, instance->parent_card) ){
			  does_not_untap_effect(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									EDNT_CHECK_ORIGINAL_CONTROLLER, 1);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XW(2, 1), 0,
									&td, "Select target attacking or blocking creature.");
}

/*
static int autumns_veil_effect(int player, int card, event_t event){
	if( event == EVENT_CAST_SPELL && ! affect_me(player, card) && affected_card_controller == player ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			set_special_flags2(affected_card_controller, affected_card, SF2_AUTUMN_VEIL);
		}
	}
	if( eot_trigger(player, card, event) ){
		remove_special_flags2(player, -1, SF2_AUTUMN_VEIL);
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}
*/
int card_autumns_veil(int player, int card, event_t event)
{
  /* Autumn's Veil	|G
	 * Instant
	 * Spells you control can't be countered by |Sblue or |Sblack spells this turn, and creatures you control can't be the targets of |Sblue or |Sblack spells
	 * this turn. */

  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_CAST_SPELL && affect_me(player, card) && IS_AI(player) &&
      (get_deck_color(-1, 1-player) & (COLOR_TEST_BLACK | COLOR_TEST_BLUE)))
    ai_modifier += 25;
  if (event == EVENT_RESOLVE_SPELL)
  {
    your_spells_cant_be_countered_by_color[player] |=
        (uint8_t)get_sleighted_color_test(player, card, COLOR_TEST_BLACK | COLOR_TEST_BLUE);
    create_legacy_effect(player, card, autumns_veil_effect);
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_azure_mage(int player, int card, event_t event)
{
  /* Azure Mage	|1|U
	 * Creature - Human Wizard 2/1
	 * |3|U: Draw a card. */

if (event == EVENT_CAN_CAST)
    return 1;
  if (event == EVENT_RESOLVE_ACTIVATION)
    draw_cards(player, 1);
  return generic_activated_ability(player, card, event, 0, MANACOST_XU(3, 1), 0, NULL, NULL);
}

int card_benalish_veteran(int player, int card, event_t event)
{
	/* Benalish Veteran	|2|W
	 * Creature - Human Soldier 2/2
	 * Whenever ~ attacks, it gets +1/+1 until end of turn. */
	/* Charging Griffin	|3|W
	 * Creature - Griffin 2/2
	 * Flying (This creature can't be blocked except by creatures with flying or reach.)
	 * Whenever ~ attacks, it gets +1/+1 until end of turn. */
	/* Graceful Cat	|2|W
	 * Creature - Cat 2/2
	 * Whenever ~ attacks, it gets +1/+1 until end of turn. */

	return when_attacks_pump_self(player, card, event, 1,1);
}

int card_bloodlord_of_vaasgoth(int player, int card, event_t event){
/* CARD_ID_BLOODLORD_OF_VAASGOTH	11784
Bloodlord of Vaasgoth	|3|B|B
Creature - Vampire Warrior 3/3
Bloodthirst 3 (If an opponent was dealt damage this turn, this creature enters the battlefield with three +1/+1 counters on it.)
Flying
Whenever you cast a Vampire creature spell, it gains bloodthirst 3. */

	bloodthirst(player, card, event, 3);

	if( (event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE) && ! affect_me(player, card) &&
		affected_card_controller == player && ! is_humiliated(player, card, AL_TRIG) )
	{
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			if( get_trap_condition(1-player, TRAP_DAMAGE_TAKEN) ){
				if( has_subtype(affected_card_controller, affected_card, SUBTYPE_VAMPIRE) ){
					enters_the_battlefield_with_counters(affected_card_controller, affected_card, event, COUNTER_P1_P1, 3);
				}
			}
		}
	}

	return 0;
}

int chandra_firebrand_legacy(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_SPELL);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
			kill_card(player, card, KILL_EXILE);
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_chandra_firebrand(int player, int card, event_t event){
	/* Chandra, the Firebrand	|3|R	0x2003cb1
	 * Legendary Planeswalker - Chandra (3)
	 * +1: ~ deals 1 damage to any target.
	 * -2: When you cast your next instant or sorcery spell this turn, copy that spell. You may choose new targets for the copy.
	 * -6: ~ deals 6 damage to each of up to six targets. */

	check_legend_rule(player, card, event);
	if (IS_ACTIVATING(event)){

		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);

		enum{
			CHOICE_PING = 1,
			CHOICE_COPY_SPELL,
			CHOICE_6TARGETS
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Ping creature or player", can_target(&td), 15, 1,
						"Copy spell", 1, count_counters(player, card, COUNTER_LOYALTY) > 2 ? 20 : 5, -2,
						"6 damage to 6 targets", 1, 3*target_available(player, card, &td), -6);

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
			  case CHOICE_PING:
					instance->number_of_targets = 0;
					pick_target(&td, "TARGET_ANY");
					break;

			  case CHOICE_COPY_SPELL:
					break;

			  case CHOICE_6TARGETS:
				{
					int trgs = 0;
					int targetted_players[2] = {! would_validate_arbitrary_target(&td, 0, -1),
												player == AI ? 1 : ! would_validate_arbitrary_target(&td, 1, -1)};
					if( targetted_players[1-player] != 1 && player == AI ){
						instance->targets[0].player = 1-player;
						instance->targets[0].card = -1;
						instance->number_of_targets = 1;
						trgs++;
					}
					while( trgs < 6 ){
							char buffer[100];
							scnprintf(buffer, 100, "Select target creature, player, or planeswalker (%d of 6)", trgs+1);
							if( new_pick_target(&td, buffer, trgs, GS_LITERAL_PROMPT) ){
								if( instance->targets[trgs].card != -1 ){
									state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
									trgs++;
								}
								else{
									if( targetted_players[instance->targets[trgs].player] != 1 ){
										targetted_players[instance->targets[trgs].player] = 1;
										trgs++;
									}
									else{
										instance->number_of_targets--;
									}
								}
							}
							else{
								break;
							}
							if( targetted_players[0]+targetted_players[1] == 2 && ! can_target(&td2) ){
								break;
							}

					}
					int i;
					for(i=0; i<instance->number_of_targets; i++){
						if( instance->targets[i].card != -1 ){
							state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
						}
					}
				}
				break;
			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		switch (choice)
		  {
			case CHOICE_PING:
				if( valid_target(&td) ){
					damage_target0(player, card, 1);
				}
				break;

			case CHOICE_COPY_SPELL:
				create_legacy_effect(player, card, &chandra_firebrand_legacy);
				break;

			case CHOICE_6TARGETS:
				{
					int i;
					for(i=0; i<instance->number_of_targets; i++){
						if( validate_target(player, card, &td, i) ){
							damage_creature(instance->targets[i].player, instance->targets[i].card, 6, player, card);
						}
					}
				}
			  break;
		  }
	}

	return planeswalker(player, card, event, 3);
}

int card_crimson_mage(int player, int card, event_t event)
{
  /* Crimson Mage	|1|R
	 * Creature - Human Shaman 2/1
	 * |R: Target creature you control gains haste until end of turn. */

if (IS_GAA_EVENT(event))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.allowed_controller = td.preferred_controller = player;
    if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
    {
      card_instance_t* instance = get_card_instance(player, card);
      pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
          0, 0, 0, SP_KEYWORD_HASTE);
    }
    return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_CREATURE");
  }
  return 0;
}

int card_divine_favor(int player, int card, event_t event){
/* CARD_ID_DIVINE_FAVOR	11798
Divine Favor	|1|W
Enchantment - Aura
Enchant creature
When ~ enters the battlefield, you gain 3 life.
Enchanted creature gets +1/+3. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 3);
	}

	return generic_aura(player, card, event, player, 1, 3, 0, 0, 0, 0, 0);
}

int card_doubling_chant(int player, int card, event_t event)
{
  /* CARD_ID_DOUBLING_CHANT	11799
   * Doubling Chant	|5|G
   * Sorcery
   * For each creature you control, you may search your library for a creature card with the same name as that creature.
   * Put those cards onto the battlefield, then shuffle your library. */

  enum
  {
    AUTO_MODE = 1,
    MANUAL_MODE = 2
  };



if (event == EVENT_RESOLVE_SPELL)
  {
    int choice = DIALOG(player, card, event, DLG_NO_DISPLAY_FOR_AI, DLG_NO_STORAGE,
        "Auto search", 1, 10, "Manual search", 1, 0);
    if (choice == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    for (int c = active_cards_count[player]-1; c >= 0; --c)
      if (in_play(player, c) && is_what(player, c, TYPE_CREATURE))
      {
        int id = get_card_name(player, c);
        if ((choice == AUTO_MODE && has_subtype(player, c, SUBTYPE_LEGEND) && player == AI) || id == -1)
          continue;
        char msg[100];
        scnprintf(msg, 100, "Select a card named %s", cards_ptr[id]->name);
        test_definition_t test;
        new_default_test_definition(&test, TYPE_CREATURE, msg);
        test.qty = 1;
        test.no_shuffle = 1;
        test.id_flag = 0;
        test.id = id;
        new_global_tutor(player, player, choice == 1 ? TUTOR_FROM_DECK_AUTO : TUTOR_FROM_DECK,
            TUTOR_PLAY, 0, AI_FIRST_FOUND, &test);
      }
    shuffle(player);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

int card_druidic_satchel(int player, int card, event_t event){
/* Druidic Satchel	|3
 * Artifact
 * |2, |T: Reveal the top card of your library.
		If it's a creature card, put a 1/1 |Sgreen Saproling creature token onto the battlefield.
		If it's a land card, put that card onto the battlefield under your control.
		If it's a noncreature, nonland card, you gain 2 life. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int *deck = deck_ptr[player];
		show_deck( HUMAN, deck, 1, "Here's the first card of the deck", 0, gs_done_008b40e0 );
		int mode = 0;
		if( cards_data[deck[0]].type & TYPE_CREATURE ){
			mode |= 1;
		}
		if( cards_data[deck[0]].type & TYPE_LAND ){
			mode |= 2;
		}

		if( mode & 1 ){
			generate_token_by_id(player, card, CARD_ID_SAPROLING);
		}
		if( mode & 2 ){
			int card_added = add_card_to_hand(player, deck[0]);
			remove_card_from_deck(player, 0);
			put_into_play(player, card_added);
		}
		if( mode == 0 ){
			gain_life(player, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL);
}

int card_garruk_primal_hunter(int player, int card, event_t event){

	/* Garruk, Primal Hunter	|2|G|G|G
	 * Planeswalker - Garruk (3)
	 * +1: Put a 3/3 |Sgreen Beast creature token onto the battlefield.
	 * -3: Draw cards equal to the greatest power among creatures you control.
	 * -6: Put a 6/6 |Sgreen Wurm creature token onto the battlefield for each land you control. */
	check_legend_rule(player, card, event);

	if (IS_ACTIVATING(event)){

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);

		int priority_draw = 0;
		if( event == EVENT_ACTIVATE ){
			priority_draw = (2*check_battlefield_for_special_card(player, card, player, CBFSC_GET_MAX_POW, &this_test))-
							(3*((count_counters(player, card, COUNTER_LOYALTY)*10)-30));
		}

		int priority_wurm = 0;
		if( event == EVENT_ACTIVATE ){
			priority_draw = (4*count_subtype(player, TYPE_LAND, -1))-
							(3*((count_counters(player, card, COUNTER_LOYALTY)*10)-60));
		}

		enum{
			CHOICE_BEAST = 1,
			CHOICE_DRAW,
			CHOICE_WURMFEST
		}
		choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
						"Generate a Beast", 1, 10, 1,
						"Draw cards", 1, priority_draw, -3,
						"Wurm fest", 1, priority_wurm, -6);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_RESOLVE_ACTIVATION)
		{
			switch (choice)
			{
				case CHOICE_BEAST:
					generate_token_by_id(player, card, CARD_ID_BEAST);
					break;

				case CHOICE_DRAW:
					draw_cards(player, check_battlefield_for_special_card(player, card, player, CBFSC_GET_MAX_POW, &this_test));
					break;

				case CHOICE_WURMFEST:
					generate_tokens_by_id(player, card, CARD_ID_WURM, count_subtype(player, TYPE_LAND, -1));
					break;
			}
		}
	}

	return planeswalker(player, card, event, 3);
}

int card_gideons_avenger(int player, int card, event_t event)
{
  /* CARD_ID_GIDEONS_AVENGER	11808
   * Gideon's Avenger	|1|W|W
   * Creature - Human Soldier 2/2
   * Whenever a creature an opponent controls becomes tapped, put a +1/+1 counter on ~. */

  card_instance_t *instance;
  if ((event == EVENT_TAP_CARD) && (1 - player == affected_card_controller))
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      if (is_what(affected_card_controller, affected_card, 2))
      {
        instance = get_card_instance(player, card);
        instance->targets[1].card = 0x42;
        add_counters(player, card, player, card, COUNTER_P1_P1, 1);
        instance->targets[1].card = 0;
      }
    }
  }
  return 0;
}

int card_grand_abolisher(int player, int card, event_t event)
{
  /* CARD_ID_GRAND_ABOLISHER	11814
   * Grand Abolisher	|W|W
   * Creature - Human Cleric 2/2
   * During your turn, your opponents can't cast spells or activate abilities of artifacts, creatures, or enchantments. */

  card_instance_t *instance;
  int turn_player;
  test_definition_t test;
  turn_player = current_turn;
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if (current_turn == player)
    {
      instance = in_play(current_turn, card);
      if ((instance != NULL) && (1 - turn_player == affected_card_controller))
      {
        if (is_what(1 - turn_player, affected_card, 1) == 0)
        {
          if (is_humiliated(turn_player, card, AL_STATIC) == 0)
          {
            infinite_casting_cost();
          }
        }
      }
    }
  }
  else if (current_turn == player)
  {
    default_test_definition(&test, 0x46);
    remove_abilities_by_test(1 - turn_player, event, &test, AL_ALL_ACT);
    return 0;
  }
  return 0;
}

int card_guardians_pledge(int player, int card, event_t event)
{
  /* Guardians' Pledge	|1|W|W
   * Instant
   * White creatures you control get +2/+2 until end of turn. */

  if (event == EVENT_RESOLVE_SPELL)
  {
    pump_color_until_eot(player, card, player, get_sleighted_color_test(player, card, COLOR_TEST_WHITE), 2, 2, 0, 0);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

int card_griffin_rider(int player, int card, event_t event){
  /* Griffin Rider	|1|W
   * Creature - Human Knight 1/1
   * As long as you control a Griffin creature, this creature gets +3/+3 and has flying. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			if( check_battlefield_for_subtype(player, TYPE_CREATURE, SUBTYPE_GRIFFIN) ){
				event_result +=3;
			}
		}

		if( event == EVENT_ABILITIES && affect_me(player, card) ){
			if( check_battlefield_for_subtype(player, TYPE_CREATURE, SUBTYPE_GRIFFIN) ){
				event_result |= KEYWORD_FLYING;
			}
		}
	}

	return 0;
}

int card_hideous_visage(int player, int card, event_t event){
  /* Hideous Visage	|2|B
   * Sorcery
   * Creatures you control gain intimidate until end of turn. (Each of those creatures can't be blocked except by artifact creatures and/or creatures that share a color with it.) */

	if( event == EVENT_RESOLVE_SPELL ){
		int count = active_cards_count[player]-1;
		while( count > -1 ){
				if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) ){
					pump_ability_until_eot(player, card, player, count, 0, 0, 0, SP_KEYWORD_INTIMIDATE);
				}
				count--;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static int hunters_insight_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( damage_dealt_by_me_arbitrary(p, c, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MAY_DAMAGE_PLANESWALKER | DDBM_MUST_BE_COMBAT_DAMAGE |
										 DDBM_REPORT_DAMAGE_DEALT, player, card) )
		{
			draw_cards(player, instance->targets[16].player);
			instance->targets[16].player = 0;
		}
	}

	if( event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_hunters_insight(int player, int card, event_t event){
  /* Hunter's Insight	|2|G
   * Instant
   * Choose target creature you control. Whenever that creature deals combat damage to a player or planeswalker this turn, draw that many cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature you control.", 1, NULL);
		if( spell_fizzled != 1 ){
			int mod = -48;
			if( check_for_ability(instance->targets[0].player, instance->targets[0].card, KEYWORD_TRAMPLE) ||
				is_unblocked(instance->targets[0].player, instance->targets[0].card) )
			{
				mod = 48;
			}
			ai_modifier += mod;
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &hunters_insight_legacy, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, NULL, 1, NULL);
}

int card_jace_memory_adept(int player, int card, event_t event){

	/* Jace, Memory Adept	|3|U|U
	 * Planeswalker - Jace (4)
	 * +1: Draw a card. Target player puts the top card of his or her library into his or her graveyard.
	 * 0: Target player puts the top ten cards of his or her library into his or her graveyard.
	 * -7: Any number of target players each draw twenty cards. */
	check_legend_rule(player, card, event);

	if (IS_ACTIVATING(event)){

		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;

		enum{
			CHOICE_DRAW_MILL = 1,
			CHOICE_MILL10,
			CHOICE_DRAW7
		};
		int mill10_priority = would_validate_arbitrary_target(&td, 1-player, -1) ? 40-count_deck(1-player) : 0;
		int draw7_priority = would_validate_arbitrary_target(&td, player, -1) && count_deck(player) > 30 ? 42-(hand_count[player]*6) : 0;
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
						"Draw & mill", can_target(&td), 35-(hand_count[player]*5), 1,
						"Mill 10", can_target(&td), mill10_priority, 0,
						"Draw 20", can_target(&td), draw7_priority, -7);

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
			  case CHOICE_DRAW_MILL:
			  case CHOICE_MILL10:
				pick_target(&td, "TARGET_PLAYER");
				break;

			  case CHOICE_DRAW7:
				{
					if( player == HUMAN ){
						if( pick_target(&td, "TARGET_PLAYER") ){
							new_pick_target(&td, "TARGET_PLAYER", 1, 0);
							if( instance->targets[1].player == instance->targets[0].player ){
								instance->number_of_targets--;
							}
						}
					}
				}
				break;
			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		switch (choice)
		  {
			case CHOICE_DRAW_MILL:
				draw_cards(player, 1);
				if ( valid_target(&td)){
					mill(instance->targets[0].player, 1);
				}
				break;

			case CHOICE_MILL10:
				if ( valid_target(&td)){
					mill(instance->targets[0].player, 10);
				}
				break;

			case CHOICE_DRAW7:
				{
					int i;
					for(i=0; i<instance->number_of_targets; i++){
						if( validate_target(player, card, &td, i) ){
							draw_cards(instance->targets[i].player, 20);
						}
					}
				}
				break;
		  }
	}

	return planeswalker(player, card, event, 4);
}

int card_jaces_archivist(int player, int card, event_t event){
  /* Jace's Archivist	|1|U|U
   * Creature - Vedalken Wizard 2/2
   * |U, |T: Each player discards their hand, then draws cards equal to the greatest number of cards a player discarded this way. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int to_draw = MAX(hand_count[player], hand_count[1-player]);
		APNAP(p,{
					new_discard_all(p, player);
					draw_cards(p, to_draw);
				};
		);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_U(1), 0, NULL, NULL);
}

int card_jade_mage(int player, int card, event_t event)
{
  /* Jade Mage	|1|G
	 * Creature - Human Shaman 2/1
	 * |2|G: Put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

if (event == EVENT_RESOLVE_ACTIVATION)
    generate_token_by_id(player, card, CARD_ID_SAPROLING);
  // The October 2022 binary charges 3U here.
  return generic_activated_ability(player, card, event, 0, MANACOST_XU(3, 1), 0, NULL, NULL);
}

int card_lord_of_the_unreal(int player, int card, event_t event)
{
  /* Lord of the Unreal	|U|U
   * Creature - Human Wizard 2/2
   * Illusion creatures you control get +1/+1 and have hexproof. (They can't be the targets of spells or abilities your opponents control.) */

  // Illusion creatures you control get +1/+1 and have hexproof.
  boost_subtype(player, card, event, SUBTYPE_ILLUSION, 1,1, 0,SP_KEYWORD_HEXPROOF, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
  return 0;
}

int card_onyx_mage(int player, int card, event_t event)
{
  /* Onyx Mage	|1|B
	 * Creature - Human Wizard 2/1
	 * |1|B: Target creature you control gains deathtouch until end of turn. */

if (IS_GAA_EVENT(event))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.allowed_controller = td.preferred_controller = player;
    if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
    {
      card_instance_t* instance = get_card_instance(player, card);
      pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
          0, 0, 0, SP_KEYWORD_DEATHTOUCH);
    }
    return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XB(1, 1), 0, &td, "TARGET_CREATURE");
  }
  return 0;
}

int card_personal_sanctuary(int player, int card, event_t event){
  /* Personal Sanctuary |2|W
   * Enchantment
   * During your turn, prevent all damage that would be dealt to you. */

	if( event == EVENT_PREVENT_DAMAGE && in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *damage = damage_being_prevented(event);
		if( current_turn == player && damage->damage_target_card == -1 && damage->damage_target_player == player &&
			! damage_is_to_planeswalker(damage) )
		{
			prevent_damage(affected_card_controller, affected_card, -1);
		}
	}

	return global_enchantment(player, card, event);
}

int card_phantasmal_image(int player, int card, event_t event)
{
	/* Phantasmal Image	|1|U	0x2003ce8
	 * Creature - Illusion 0/0
	 * You may have ~ enter the battlefield as a copy of any creature on the battlefield, except it's an Illusion in addition to its other types and it has "When this creature becomes the target of a spell or ability, sacrifice it." */
	if (enters_the_battlefield_as_copy_of_any_creature(player, card, event)){
		add_a_subtype_permanently(player, card, player, card, SUBTYPE_ILLUSION);
		set_legacy_image(player, CARD_ID_PHANTASMAL_IMAGE, create_targetted_legacy_effect(player, card, attached_creature_gains_sacrifice_when_becomes_target, player, card));
	}

	return 0;
}

int card_pride_guardian(int player, int card, event_t event){
  /* Pride Guardian	|W
   * Creature - Cat Monk 0/3
   * Defender
   * Whenever this creature blocks, you gain 3 life. */

	if( current_turn == 1-player && event == EVENT_DECLARE_BLOCKERS && blocking(player, card, event) && ! is_humiliated(player, card, AL_TRIG) ){
		gain_life(player, 3);
	}

	return 0;
}

int card_primordial_hydra(int player, int card, event_t event){
  /* Primordial Hydra	|X|G|G
   * Creature - Hydra 0/0
   * This creature enters with X +1/+1 counters on it.
   * At the beginning of your upkeep, double the number of +1/+1 counters on this creature.
   * This creature has trample as long as it has ten or more +1/+1 counters on it. */

	if (event == EVENT_CAST_SPELL && affect_me(player, card) && !is_token(player, card)){
		generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, get_card_instance(player, card)->info_slot);
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int number = count_1_1_counters(player, card);
		add_counters(player, card, player, card, COUNTER_P1_P1, number);
	}

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( count_1_1_counters(player, card) > 9 ){
			event_result |= KEYWORD_TRAMPLE;
		}
	}

	return 0;
}

int card_rites_of_flourishing(int player, int card, event_t event)
{
  /* Rites of Flourishing	|2|G
   * Enchantment
   * At the beginning of each player's draw step, that player draws an additional card.
   * Each player may play an additional land on each of their turns. */

  card_instance_t *instance;
  int result;
  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      check_playable_lands(current_turn);
    }
  }
  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if ((event == EVENT_DRAW_PHASE) && (result == 0))
    {
      event_result = event_result + 1;
    }
  }
  return global_enchantment(player, card, event);
}

int card_rune_scarred_demon(int player, int card, event_t event){
  /* Rune-Scarred Demon	|5|B|B
   * Creature - Demon 6/6
   * Flying
   * When this creature enters, search your library for a card, put it into your hand, then shuffle. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
	}

	return 0;
}

int card_skinshifter(int player, int card, event_t event)
{
  /* Skinshifter	|1|G
   * Creature - Human Shaman 1/1
   * |G: Choose one. Activate only once each turn.
   * * Until end of turn, this creature becomes a Rhino with base power and toughness 4/4 and gains trample.
   * * Until end of turn, this creature becomes a Bird with base power and toughness 2/2 and gains flying.
   * * Until end of turn, this creature becomes a Plant with base power and toughness 0/8. */

  enum
  {
    CHOICE_RHINO = 1,
    CHOICE_BIRD = 2,
    CHOICE_PLANT = 3
  };

  int choice;
  card_instance_t *instance;
  int attacking;
  unsigned int rhino_priority;
  int32_t resolved_mode;
  unsigned int bird_priority;
  uint32_t plant_priority;
  bool your_turn;

  if ((uint32_t)(event - EVENT_POW_BOOST) < 2)
  {
    if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x2000, 0, 0, 0, 1, 0, 0, 0,
                                  NULL, NULL) == 0)
    {
      return 0;
    }
    return (unsigned int)(event != EVENT_POW_BOOST) * 4 + 3;
  }
  if (!IS_GAA_EVENT(event))
    return 0;
  instance = get_card_instance(player, card);
  if (event == EVENT_ACTIVATE)
  {
    your_turn = current_turn == player;
    instance->info_slot = 0;
    if (your_turn)
    {
      bird_priority = ((0x16 < current_phase) ? 0 : 10);
      attacking = is_attacking(player, card);
      plant_priority = 0;
      rhino_priority = 0;
      if (attacking != 0)
      {
        rhino_priority = ((current_phase != 0x18) ? 0 : 10);
      }
    }
    else
    {
      bird_priority = 0;
      plant_priority = 10;
      rhino_priority = 0;
    }
    choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, "4/4 Rhino", 1,
                         rhino_priority, "2/2 Bird", 1, bird_priority, "0/8 Plant", 1,
                         plant_priority, 0, 0, 0, 0);
    if (choice == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    instance->info_slot = choice;
    return generic_activated_ability(player, card, event, 0x2000, 0, 0, 0, 1, 0, 0, 0, NULL,
                                     NULL);
  }

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    resolved_mode = instance->info_slot;
    if (resolved_mode == CHOICE_RHINO)
    {
      set_pt_and_abilities_until_eot(instance->parent_controller, instance->parent_card,
                                     instance->parent_controller, instance->parent_card, 4, 4, 0x80,
                                     0, 1);
      force_a_subtype_until_eot(player, card, instance->parent_controller, instance->parent_card,
                                SUBTYPE_RHINO);
      resolved_mode = instance->info_slot;
    }
    if (resolved_mode == CHOICE_BIRD)
    {
      set_pt_and_abilities_until_eot(instance->parent_controller, instance->parent_card,
                                     instance->parent_controller, instance->parent_card, 2, 2, 0x20,
                                     0, 1);
      force_a_subtype_until_eot(player, card, instance->parent_controller, instance->parent_card,
                                SUBTYPE_BIRD);
      resolved_mode = instance->info_slot;
    }
    if (resolved_mode == CHOICE_PLANT)
    {
      set_pt_and_abilities_until_eot(instance->parent_controller, instance->parent_card,
                                     instance->parent_controller, instance->parent_card, 0, 8, 0, 0,
                                     1);
      force_a_subtype_until_eot(player, card, instance->parent_controller, instance->parent_card,
                                SUBTYPE_PLANT);
    }
  }
  return generic_activated_ability(player, card, event, 0x2000, 0, 0, 0, 1, 0, 0, 0, NULL, NULL);
}

int card_spirit_mantle(int player, int card, event_t event){
  /* Spirit Mantle	|1|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+1 and has protection from creatures. */

	attached_gets_pt(player, card, event, 1, 1);
	attached_gets_kw(player, card, event, KEYWORD_PROT_CREATURES, SP_KEYWORD_0);
	return aura(player, card, event, NULL, NULL);
}

int card_stonehorn_dignitary(int player, int card, event_t event){
  /* Stonehorn Dignitary	|3|W
   * Creature - Rhino Soldier 1/4
   * When this creature enters, target opponent skips their next combat phase. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_player_skips_his_next_attack_step(player, card, 1-player, 0);
	}

	return 0;

}

int card_timely_reinforcements(int player, int card, event_t event){
/* Timely Reinforcements	|2|W
 * Sorcery
 * If you have less life than an opponent, you gain 6 life.
 * If you control fewer creatures than an opponent, put three 1/1 |Swhite Soldier creature tokens onto the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( life[player] < life[1-player] ){
			gain_life(player, 6);
		}

		if( count_permanents_by_type(player, TYPE_CREATURE) < count_permanents_by_type(1-player, TYPE_CREATURE) ){
			generate_tokens_by_id(player, card, CARD_ID_SOLDIER, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_vengeful_pharaoh(int player, int card, event_t event){
  /* Vengeful Pharaoh	|2|B|B|B
   * Creature - Zombie 5/4
   * Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.)
   * Whenever combat damage is dealt to you or a planeswalker you control, if this card is in your graveyard, destroy target attacking creature, then put this card on top of your library. */

	// the graveyard effect is in "rules_engine.c" and "planeswalkers.c"

	deathtouch(player, card, event);

	return 0;
}

int card_vampire_outcasts(int player, int card, event_t event){
  /* Vampire Outcasts	|2|B|B
   * Creature - Vampire 2/2
   * Bloodthirst 2 (If an opponent was dealt damage this turn, this creature enters with two +1/+1 counters on it.)
   * Lifelink (Damage dealt by this creature also causes you to gain that much life.) */

	bloodthirst(player, card, event, 2);

	lifelink(player, card, event);

	return 0;
}

int card_worldslayer(int player, int card, event_t event)
{
  /* Worldslayer	|5
   * Artifact - Equipment
   * Whenever equipped creature deals combat damage to a player, destroy all permanents other than ~.
   * Equip |5 */

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (is_equipped(player, card) ? -48 : 48);
	}

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_ATTACK_RATING && affect_me(p, c) ){
			ai_defensive_modifier -= 128;
		}
	}

	if( equipped_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER) )
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.not_me = 1;

		new_manipulate_all(player, card, ANYBODY, &test, KILL_DESTROY);
	}

	return basic_equipment(player, card, event, 5);
}

int card_crown_of_empires(int player, int card, event_t event){
  /* Crown of Empires	|2
   * Artifact
   * |3, |T: Tap target creature. Gain control of that creature instead if you control artifacts named Scepter of Empires and Throne of Empires. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			if( empire_set(player) ){
				gain_control_permanently(instance->parent_controller, instance->targets[0].player, instance->targets[0].card);
			}
			else{
				tap_card(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_CREATURE");
}

int card_scepter_of_empires(int player, int card, event_t event){
	/* Scepter of Empires	|3	0x2003e78
	 * Artifact
	 * |T: ~ deals 1 damage to target player or planeswalker. It deals 3 damage to that player or planeswalker instead if you control artifacts named Crown of Empires and Throne of Empires. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, empire_set(player) ? 3 : 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER_OR_PLANESWALKER");
}

int card_throne_of_empires(int player, int card, event_t event){
	/* Throne of Empires	|4
	 * Artifact
	 * |1, |T: Put a 1/1 |Swhite Soldier creature token onto the battlefield. Put five of those tokens onto the battlefield instead if you control artifacts named Crown of Empires and Scepter of Empires. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SOLDIER, &token);
		token.pow = token.tou = 1;
		token.qty = empire_set(player) ? 5 : 1;
		token.color_forced = COLOR_TEST_WHITE;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL);
}

int card_stave_off(int player, int card, event_t event){
  /* Stave Off	|W
   * Instant
   * Target creature gains protection from the color of your choice until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int kw = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, kw, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_aven_fleetwing(int player, int card, event_t event){
	/* Ancient of the Equinox	(Transforms from CARD_ID_AUTUMNAL_GLOOM)
	 * Creature - Treefolk 4/4
	 * Trample, hexproof */

	/* Ascended Lawmage	|2|W|U
	 * Creature - Vedalken Wizard 3/2
	 * Flying, hexproof */

	/* Aven Fleetwing	|3|U
	 * Creature - Bird Soldier 2/2
	 * Flying
	 * Hexproof */

	/* Bassara Tower Archer	|G|G
	 * Creature - Human Archer 2/1
	 * Hexproof, reach */

	/* Benthic Giant	|5|U
	 * Creature - Giant 4/5
	 * Hexproof */

	/* Conifer Strider	|3|G
	 * Creature - Elemental 5/1
	 * Hexproof (This creature can't be the target of spells or abilities your opponents control.) */

	/* Gladecover Scout	|G
	 * Creature - Elf Scout 1/1
	 * Hexproof */

	/* Plated Crusher	|4|G|G|G
	 * Creature - Beast 7/6
	 * Trample, hexproof */

	/* Plated Slagwurm	|4|G|G|G
	 * Creature - Wurm 8/8
	 * Hexproof */

	/* Primal Huntbeast	|3|G
	 * Creature - Beast 3/3
	 * Hexproof */

	/* Rubbleback Rhino	|4|G
	 * Creature - Rhino 3/4
	 * Hexproof */

	/* Sacred Wolf	|2|G
	 * Creature - Wolf 3/1
	 * Hexproof */

	/* Scaled Behemoth	|4|G|G
	 * Creature - Crocodile 6/7
	 * Hexproof (This creature can't be the target of spells or abilities your opponents control.) */

	/* Taoist Hermit	|2|G
	 * Creature - Human Mystic 2/2
	 * Hexproof (This creature can't be the target of spells or abilities your opponents control.) */

	hexproof(player, card, event);

	return 0;
}

int card_belltower_sphinx(int player, int card, event_t event){
  /* Belltower Sphinx |4|U
   * Creature — Sphinx 2/5
   * Flying
   * Whenever a source deals damage to Belltower Sphinx, that source's controller puts that many cards from the top of his or her library
   * 	into his or her graveyard. */

	if (event == EVENT_DEAL_DAMAGE){
		card_instance_t* instance = get_card_instance(player, card);
		card_instance_t* damage = get_card_instance(affected_card_controller, affected_card);
		if (damage->internal_card_id == damage_card && instance->info_slot != current_phase
			&& damage->damage_target_card == card && damage->damage_target_player == player && damage->info_slot > 0){
			instance->targets[8].player = instance->targets[8].player < 0 ? 1 : instance->targets[8].player + 1;
			int amount = damage->info_slot;
			int source_player = damage->damage_source_player;
			if (instance->targets[source_player].player >= 0){ amount += instance->targets[source_player].player; }
			instance->targets[source_player].player = amount;
		}
	}
	if (resolve_damage_trigger(player, card, event, DDBM_0, player, card)){
		card_instance_t* instance = get_card_instance(player, card);
		int p = current_turn;
		int amount = instance->targets[p].player;
		if (amount > 0){ instance->targets[p].player = 0; mill(p, amount); }
		p = current_turn ^ 1;
		amount = instance->targets[p].player;
		if (amount > 0){ instance->targets[p].player = 0; mill(p, amount); }
	}
	return 0;
}

int card_chasm_drake(int player, int card, event_t event)
{
  /* Chasm Drake	|4|U
   * Creature - Drake 3/3
   * Flying
   * Whenever this creature attacks, target creature you control gains flying until end of turn. */

	// Whenever ~ attacks, target creature you control gains flying until end of turn.
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY)
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td))
		{
			if (player == AI && !(td.illegal_abilities & KEYWORD_FLYING))
			{
				td.illegal_abilities |= KEYWORD_FLYING;
				if (!can_target(&td))
					td.illegal_abilities &= ~KEYWORD_FLYING;
			}

			if (pick_target(&td, "ASHNODS_BATTLEGEAR"))	// Select target creature you control.
				pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FLYING, 0);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_djinn_of_wishes(int player, int card, event_t event){
/* Djinn of Wishes	|3|U|U
 * Creature - Djinn 4/4
 * Flying
 * ~ enters the battlefield with three wish counters on it.
 * |2|U|U, Remove a wish counter from ~: Reveal the top card of your library. You may play that card without paying its mana cost.
 * 		If you don't, exile it. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_WISH, 3);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int iid = deck_ptr[player][0];
		reveal_card_iid(instance->parent_controller, instance->parent_card, iid);
		int exile_it = 1;
		if( can_legally_play_iid(player, iid) ){
			card_ptr_t* c = cards_ptr[ cards_data[iid].id ];
			char buffer[100];
			snprintf(buffer, 100, " Play %s\n Pass", c->name );
			int choice = do_dialog(player, player, card, -1, -1, buffer, 0);
			if( choice == 0 ){
				play_card_in_deck_for_free(player, player, 0);
				exile_it = 1;
			}
		}
		if( exile_it ){
			rfg_top_card_of_deck(player);
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XU(2,2), GVC_COUNTER(COUNTER_WISH), NULL, NULL);
}

int card_frost_breath(int player, int card, event_t event){
  /* Frost Breath	|2|U
   * Instant
   * Tap up to two target creatures. Those creatures don't untap during their controller's next untap step. */

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
				does_not_untap_effect(player, card, instance->targets[i].player, instance->targets[i].card, EDNT_TAP_TARGET, 1);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

int card_master_thief(int player, int card, event_t event){
  /* Master Thief	|2|U|U
   * Creature - Human Rogue 2/2
   * When this creature enters, gain control of target artifact for as long as you control this creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
      td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_ARTIFACT", 0, 0) ){
			gain_control_until_source_is_in_play_and_tapped(player, card, instance->targets[0].player, instance->targets[0].card,
															GCUS_SOURCE_IN_PLAY | GCUS_CONTROLLED);
		}
	}

	return 0;
}

int card_merfolk_mesmerist(int player, int card, event_t event){
  /* Merfolk Mesmerist	|1|U
   * Creature - Merfolk Wizard 1/2
   * |U, |T: Target player mills two cards. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			mill(instance->targets[0].player, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(1), 0, &td, "TARGET_PLAYER");
}

int card_mind_unbound(int player, int card, event_t event){

	/* Mind Unbound	|4|U|U
	 * Enchantment
	 * At the beginning of your upkeep, put a lore counter on ~, then draw a card for each lore counter on ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counters(player, card, player, card, COUNTER_LORE, 1);
		draw_cards(player, count_counters(player, card, COUNTER_LORE));
	}

	return global_enchantment(player, card, event);
}

int card_skywinder_drake(int player, int card, event_t event ){
  /* Skywinder Drake	|2|U
   * Creature - Drake 3/1
   * Flying
   * This creature can block only creatures with flying. */

	if( event == EVENT_BLOCK_LEGALITY && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( ! check_for_ability(attacking_card_controller, attacking_card, KEYWORD_FLYING) ){
			event_result = 1;
		}
	}

	return 0;
}

int card_sphinx_of_uthuun(int player, int card, event_t event ){
  /* Sphinx of Uthuun	|5|U|U
   * Creature - Sphinx 5/6
   * Flying
   * When this creature enters, reveal the top five cards of your library. An opponent separates those cards into two piles. Put one pile into your hand and the other into your graveyard. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		effect_fof(player, player, 5, TUTOR_GRAVE);
	}

	return 0;
}

int card_turn_into_frog(int player, int card, event_t event){
  /* Turn to Frog |1|U
   * 	  Instant
   * 	  Target creature loses all abilities and becomes a 1/1 blue Frog until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t hc;
			default_test_definition(&hc, 0);
			hc.power = 1;
			hc.toughness = 1;
			hc.subtype = SUBTYPE_FROG;
			hc.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
			humiliate_and_set_pt_abilities(player, card, instance->targets[0].player, instance->targets[0].card, 4, &hc);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_visions_of_beyond(int player, int card, event_t event){
  /* Visions of Beyond	|U
   * Instant
   * Draw a card. If a graveyard has twenty or more cards in it, draw three cards instead. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( count_graveyard(player) >= 20 || count_graveyard(1-player) >= 20 ){
			draw_cards(player, 3);
		}
		else{
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_bloodrage_vampire(int player, int card, event_t event){
  /* Bloodrage Vampire	|2|B
   * Creature - Vampire 3/1
   * Bloodthirst 1 (If an opponent was dealt damage this turn, this creature enters with a +1/+1 counter on it.) */

	bloodthirst(player, card, event, 1);

	return 0;
}

int card_brink_of_disaster(int player, int card, event_t event){
  /* Brink of Disaster	|2|B|B
   * Enchantment - Aura
   * Enchant creature or land
   * When enchanted permanent becomes tapped, destroy it. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_TAP_CARD && affect_me(p, c) ){
			kill_card(p, c, KILL_DESTROY);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_LAND);

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target creature or land.");
}

int card_dark_favor(int player, int card, event_t event){
  /* Dark Favor	|1|B
   * Enchantment - Aura
   * Enchant creature
   * When this Aura enters, you lose 1 life.
   * Enchanted creature gets +3/+1. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( life[player] < 7 ){
			ai_modifier -=1000;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		lose_life(player, 1);
	}

	return generic_aura(player, card, event, player, 3, 1, 0, 0, 0, 0, 0);
}

int card_devouring_swarm(int player, int card, event_t event){
	/* Devouring Swarm	|1|B|B
	 * Creature - Insect 2/1
	 * Flying
	 * Sacrifice a creature: ~ gets +1/+1 until end of turn. */
	/* Flesh-Eater Imp	|3|B
	 * Creature - Imp 2/2
	 * Flying
	 * Infect
	 * Sacrifice a creature: ~ gets +1/+1 until end of turn. */
	return generic_husk(player, card, event, TYPE_CREATURE, 1, 1, 0, 0);
}

int card_distress(int player, int card, event_t event){

	/* Distress	|B|B
	 * Sorcery
	 * Target player reveals his or her hand. You choose a nonland card from it. That player discards that card. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			this_test.type_flag = DOESNT_MATCH;

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_monomania(int player, int card, event_t event){
  /* Monomania	|3|B|B
   * Sorcery
   * Target player chooses a card in their hand and discards the rest. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) && hand_count[instance->targets[0].player] > 1 ){
			char msg[100] = "Select a card to keep.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, msg);
			int selected = new_select_a_card(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_HAND, 1, AI_MAX_VALUE, -1, &this_test);
			state_untargettable(instance->targets[0].player, selected, 1);
			int count = active_cards_count[instance->targets[0].player]-1;
			while( count > -1 ){
					if( in_hand(instance->targets[0].player, count) ){
						if( check_state(instance->targets[0].player, count, STATE_CANNOT_TARGET) ){
							remove_state(instance->targets[0].player, count, STATE_CANNOT_TARGET);
						}
						else{
							new_discard_card(instance->targets[0].player, count, player, 0);
						}
					}
					count--;
				}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

static int pingers(int player, int card, event_t event, int life_to_drain){

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, life_to_drain);
			gain_life(player, life_to_drain);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
}

int card_sorins_vengeance(int player, int card, event_t event){
	/* Sorin's Vengeance	|4|B|B|B	0x200407b
	 * Sorcery
	 * ~ deals 10 damage to target player or planeswalker and you gain 10 life. */
	return pingers(player, card, event, 10);
}

int card_taste_of_blood(int player, int card, event_t event){
	/* Taste of Blood	|B	0x200407b
	 * Sorcery
	 * ~ deals 1 damage to target player or planeswalker and you gain 1 life. */
	return pingers(player, card, event, 1);
}

int card_tormented(int player, int card, event_t event){
/*
Tormented Soul |B
Creature — Spirit 1/1
Tormented Soul can't block and can't be blocked.
*/
	cannot_block(player, card, event);
	unblockable(player, card, event);

	return 0;
}

int card_wring_flesh(int player, int card, event_t event){
  /* Wring Flesh	|B
   * Instant
   * Target creature gets -3/-1 until end of turn. */

	return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -3, -1, 0, 0);
}

int card_circle_of_flame(int player, int card, event_t event)
{
  /* Circle of Flame	|1|R
   * Enchantment
   * Whenever a creature without flying attacks you or a planeswalker you control, this enchantment deals 1 damage to that creature. */

  // Whenever a creature without flying attacks you or a planeswalker you control, ~ deals 1 damage to that creature.

	if (event == EVENT_DECLARE_ATTACKERS){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.keyword = KEYWORD_FLYING;
		test.keyword_flag = DOESNT_MATCH;
		store_attackers(player, card, event, DAT_TRACK, 1-player, -1, &test);
	}
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY){
		card_instance_t* instance = get_card_instance(player, card);
		unsigned char* attackers = (unsigned char*)&instance->targets[2].player;
		for (int i = instance->info_slot - 1; i >= 0; --i){
			if (in_play(current_turn, attackers[i])){ damage_creature(current_turn, attackers[i], 1, player, card); }
		}
	}
	return global_enchantment(player, card, event);
}

int card_furyborn_hellkite(int player, int card, event_t event){
  /* Furyborn Hellkite	|4|R|R|R
   * Creature - Dragon 6/6
   * Bloodthirst 6 (If an opponent was dealt damage this turn, this creature enters with six +1/+1 counters on it.)
   * Flying */

	bloodthirst(player, card, event, 6);

	return 0;
}

int card_goblin_bangchuckers(int player, int card, event_t event){
	/* Goblin Bangchuckers	|2|R|R	0x20040a3
	 * Creature - Goblin Warrior 2/2
	 * |T: Flip a coin. If you win the flip, ~ deals 2 damage to any target. If you lose the flip, ~ deals 2 damage to itself. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE){
		instance->number_of_targets = instance->info_slot = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			instance->info_slot = flip_a_coin(player, card);
			if( instance->info_slot == 1 ){
				pick_target(&td, "TARGET_ANY");
			}
			if( spell_fizzled != 1){
				add_state(player, card, STATE_TAPPED);
				tapped_for_mana_color = -1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			if( valid_target(&td) ){
				damage_target0(player, card, 2);
			}
		}
		else{
			damage_creature(instance->parent_controller, instance->parent_card, 2, instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_goblin_fireslinger(int player, int card, event_t event){
	/* Goblin Fireslinger	|R	0x20040a8
	 * Creature - Goblin Warrior 1/1
	 * |T: ~ deals 1 damage to target player or planeswalker. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER_OR_PLANESWALKER");
}

int card_gorehorn_minotaur(int player, int card, event_t event){
  /* Gorehorn Minotaurs	|2|R|R
   * Creature - Minotaur Warrior 3/3
   * Bloodthirst 2 (If an opponent was dealt damage this turn, this creature enters with two +1/+1 counters on it.) */

	bloodthirst(player, card, event, 2);

	return 0;
}

extern int suppress_legend_rule;
/* ManalinkEh 2022: 0x249eb60; original cards/m12.c:198. */
static int autumns_veil_effect(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;
  unsigned int spell_color;
  unsigned int protected_colors;

  if (event == EVENT_ABILITIES)
  {
    result = is_what(affected_card_controller, affected_card, 2);
    if ((result != 0) && (affected_card_controller == player))
    {
      special_abilities2(player, affected_card, EVENT_ABILITIES, SP_KEYWORD2_GRANTED_PROT_MISC);
    }
  }
  else if (event == EVENT_PROT_MISC)
  {
    instance = get_card_instance(affected_card_controller, affected_card);
    if (instance != NULL)
    {
      result = is_what(affected_card_controller, affected_card, 2);
      if (((result != 0) && (affected_card_controller == player)) &&
          (((unsigned int)event_result >> 24 & 0xff) == '\x05'))
      {
        result = is_being_cast(attacking_card_controller, attacking_card);
        if (result != 0)
        {
          spell_color = get_color(attacking_card_controller, attacking_card);
          protected_colors = get_sleighted_color_test(player, card, 6);
          if ((spell_color & protected_colors) != 0)
          {
            event_result = event_result | 1;
          }
        }
      }
    }
  }
  result = eot_trigger(player, card, event);
  if (result == 0)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x24a31b0; original cards/m12.c:1646. */
int card_sundial_of_the_infinite(int player, int card, event_t event)
{
  /* Sundial of the Infinite	|2
   * Artifact
   * |1, |T: End the turn. Activate only during your turn. (Exile all spells and abilities from the stack. Discard down to your maximum hand size. Damage wears off, and "this turn" and "until end of turn" effects end.) */

  int result;

  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    end_the_turn(player, card, EVENT_RESOLVE_ACTIVATION);
  }
  result = generic_activated_ability(player, card, event, 0x10001, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

int card_scrambleverse(int player, int card, event_t event){
  /* Scrambleverse	|6|R|R
   * Sorcery
   * For each nonland permanent, choose a player at random. Then each player gains control of each permanent for which they were chosen. Untap those permanents. */

	/*
	  Scrambleverse
	  Sorcery, 6RR (8)
	  For each nonland permanent, choose a player at random. Then each player gains control of each permanent for which he or she was chosen.
	  Untap those permanents.
	*/

	if( event == EVENT_RESOLVE_SPELL ){
		suppress_legend_rule = 1;

		APNAP(p,{
					int count = 0;
					while( count < active_cards_count[p] ){
							if( in_play(p, count) && is_what(p, count, TYPE_PERMANENT) && ! is_what(p, count, TYPE_LAND) ){
								int number = internal_rand(2);
								if( number == 1 ){
									state_untargettable(p, count, 1);
								}
							}
							count++;
					};
				};
		);

		APNAP(p,{
					int count = active_cards_count[p]-1;
					while( count > -1 ){
							if( in_play(p, count) && is_what(p, count, TYPE_PERMANENT) && check_state(p, count, STATE_CANNOT_TARGET) ){
								remove_state(p, count, STATE_CANNOT_TARGET);
								add_state(p, count, STATE_TARGETTED);
								give_control(player, card, p, count);
							}
							count--;
					};
				};
		);

		APNAP(p,{
					int count = 0;
					while( count < active_cards_count[p] ){
							if( in_play(p, count) && is_what(p, count, TYPE_PERMANENT) && check_state(p, count, STATE_TARGETTED) ){
								remove_state(p, count, STATE_TARGETTED);
								untap_card(p, count);
							}
							count++;
					};
				};
		);

		suppress_legend_rule = 0;

		APNAP(p,{
					int count = active_cards_count[p]-1;
					while( count > -1 ){
							if( in_play(p, count) && is_what(p, count, TYPE_PERMANENT) && is_legendary(p, count) ){
								verify_legend_rule(p, count, get_id(p, count));
							}
							count--;
					};
				};
		);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_stormblood_berserker(int player, int card, event_t event){

	/* Stormblood Berserker	|1|R
	 * Creature - Human Berserker 1/1
	 * Bloodthirst 2
	 * Menace */

	bloodthirst(player, card, event, 2);

	menace(player, card, event);

	return 0;
}

int card_tectonic_rift(int player, int card, event_t event){
  /* Tectonic Rift	|3|R
   * Sorcery
   * Destroy target land. Creatures without flying can't block this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.keyword = KEYWORD_FLYING;
			this_test.keyword_flag = 1;
			APNAP(p, {pump_creatures_until_eot(player, card, p, 0, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK, &this_test);};);

		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
}

int card_warstorm_surge(int player, int card, event_t event){
	/* Warstorm Surge	|5|R	0x20040c1
	 * Enchantment
	 * Whenever a creature enters the battlefield under your control, it deals damage equal to its power to any target. */
	etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		if( can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0) ){
			damage_target0(player, card, get_power(trigger_cause_controller, trigger_cause));
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_arachnus_spinner(int player, int card, event_t event){
  /* Arachnus Spinner |5|G
   * Creature - Spider 5/7
   * Reach (This creature can block creatures with flying.)
   * Tap an untapped Spider you control: Search your graveyard and/or library for a card named Arachnus Web and put it onto the battlefield
   * 	attached to target creature. If you search your library this way, shuffle it. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;
	td.required_subtype = SUBTYPE_SPIDER;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td1, NULL) ){
			if( can_target(&td) ){
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int spider = -1;
			if( new_pick_target(&td, "Select an untapped Spider you control.", 0, 1 | GS_LITERAL_PROMPT) ){
				spider = instance->targets[0].card;
				instance->number_of_targets = 0;
				if( pick_target(&td1, "TARGET_CREATURE") ){
					if( get_power(instance->targets[0].player, instance->targets[0].card) >= 4 ){
						ai_modifier -= 12;
					}
					tap_card(player, spider);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			char msg[100] = "Select a card named Arachnus Web.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, msg);
			this_test.id = CARD_ID_ARACHNUS_WEB;

			int result = -1;
			if( new_special_count_grave(player, &this_test) ){
				result = new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);
			}
			if( result < 0 ){
				result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);
			}
			if( result > -1 ){
				put_into_play_aura_attached_to_target(player, result, instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_arachnus_web(int player, int card, event_t event){
  /* Arachnus Web	|2|G
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature can't attack or block, and its activated abilities can't be activated.
   * At the beginning of the end step, if enchanted creature's power is 4 or greater, destroy this Aura. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 && ! is_humiliated(player, card, AL_TRIG) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( trigger_condition == TRIGGER_EOT &&  affect_me(player, card ) && reason_for_trigger_controller == player){
			if( get_power(p, c) >= 4 ){
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						kill_card(player, card, KILL_SACRIFICE);
				}
			}
		}
	}

	return card_arrest(player, card, event);
}

int card_carnage_wurm(int player, int card, event_t event){
  /* Carnage Wurm	|6|G
   * Creature - Wurm 6/6
   * Bloodthirst 3 (If an opponent was dealt damage this turn, this creature enters with three +1/+1 counters on it.)
   * Trample (This creature can deal excess combat damage to the player or planeswalker it's attacking.) */

	bloodthirst(player, card, event, 3);

	return 0;
}

int card_dungrove_elder(int player, int card, event_t event)
{
  /* Dungrove Elder	|2|G
   * Creature - Treefolk * / *
   * Hexproof (This creature can't be the target of spells or abilities your opponents control.)
   * ~'s power and toughness are each equal to the number of Forests you control. */

  int result;
  hexproof(player, card, event);
  if (((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    result = get_hacked_subtype(player, card, 0x5001);
    result = count_subtype(player, 0x47, result);
    set_characteristic_pt(result);
  }
  return 0;
}

int card_garruks_horde(int player, int card, event_t event){

	/* Garruk's Horde	|5|G|G
	 * Creature - Beast 7/7
	 * Trample
	 * Play with the top card of your library revealed.
	 * You may cast the top card of your library if it's a creature card. */

	reveal_top_card(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		return (deck_ptr[player][0] != -1 && is_what(-1, deck_ptr[player][0], TYPE_CREATURE) &&
				can_legally_play_iid_now(player, deck_ptr[player][0], event) && has_mana_to_cast_iid(player, event, deck_ptr[player][0]));
	}

	if( event == EVENT_ACTIVATE ){
		int *deck = deck_ptr[player];
		if( charge_mana_from_id(player, -1, event, cards_data[deck[0]].id) ){
			play_card_in_deck_for_free(player, player, 0);
			cant_be_responded_to = 1;
		}
	}

	return 0;
}

int card_stingerfling_spider(int player, int card, event_t event){
  /* Stingerfling Spider |4|G
   * Creature — Spider 2/5
   * Reach
   * When Stingerfling Spider enters the battlefield, you may destroy target creature with flying. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_abilities = KEYWORD_FLYING;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_trollhide(int player, int card, event_t event)
{
  /* Trollhide	|2|G
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+2 and has "|1|G: Regenerate this creature." (The next time the creature would be destroyed this turn, instead tap it, remove it from combat, and heal all damage on it.) */

  card_instance_t *instance;
  int enchanted_card;
  int player_00;
  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = get_card_instance(player, card);
    if (-1 < instance->damage_target_player)
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_ACTIVATE)
      {
        player_00 = (int)instance->damage_target_player;
        enchanted_card = instance->damage_target_card;
      }
      else
      {
        if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
          return generic_aura(player, card, event, player, 2, 2, 0, 0, 0, 0, 0);
        player_00 = (int)instance->damage_target_player;
        enchanted_card = instance->damage_target_card;
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          if (can_regenerate(player_00, enchanted_card) != 0)
          {
            regenerate_target(player_00, enchanted_card);
          }
        }
      }
      return granted_generic_activated_ability(player, card, player_00, enchanted_card, event,
                                               0x800000, 1, 0, 0, 1, 0, 0, 0, NULL, NULL);
    }
  }
  return generic_aura(player, card, event, player, 2, 2, 0, 0, 0, 0, 0);
}

int card_crumbling_colossus(int player, int card, event_t event)
{
  /* Crumbling Colossus	|5
   * Artifact Creature - Golem 7/4
   * Trample (This creature can deal excess combat damage to the player or planeswalker it's attacking.)
   * When this creature attacks, sacrifice it at end of combat. */

	// When ~ attacks, sacrifice it at end of combat.
	if (declare_attackers_trigger(player, card, event, 0, player, card))
		create_targetted_legacy_effect(player, card, &sacrifice_at_end_of_combat, player, card);

	return 0;
}

int card_greatsword(int player, int card, event_t event){
  /* Greatsword	|3
   * Artifact - Equipment
   * Equipped creature gets +3/+0.
   * Equip |3 (|3: Attach to target creature you control. Equip only as a sorcery.) */

  return vanilla_equipment(player, card, event, 3, 3, 0, 0, 0);
}

int card_kite_shield(int player, int card, event_t event){
  /* Kite Shield	|0
   * Artifact - Equipment
   * Equipped creature gets +0/+3.
   * Equip |3 (|3: Attach to target creature you control. Equip only as a sorcery.) */

  return vanilla_equipment(player, card, event, 3, 0, 3, 0, 0);
}

int card_swiftfoot_boots(int player, int card, event_t event){
  /* Swiftfoot Boots	|2
   * Artifact - Equipment
   * Equipped creature has hexproof and haste. (It can't be the target of spells or abilities your opponents control. It can attack and |T no matter when it came under your control.)
   * Equip |1 (|1: Attach to target creature you control. Equip only as a sorcery.) */

	return vanilla_equipment(player, card, event, 1, 0,0, 0,SP_KEYWORD_HASTE|SP_KEYWORD_HEXPROOF);
}

int card_buried_ruin(int player, int card, event_t event){
  /* Buried Ruin	""
   * Land
   * |T: Add |C.
   * |2, |T, Sacrifice this land: Return target artifact card from your graveyard to your hand. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select target artifact card.");

	enum{
		CHOICE_MANA = 1,
		CHOICE_RECYCLE
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(3),
															0, NULL, NULL, 0, &test, GS_GRAVE_RECYCLER) )
		{
			choice = CHOICE_RECYCLE;
		}
		instance->info_slot = choice;

		if( ! IS_AI(player) && choice ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Recycle an artifact", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_RECYCLE ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability_with_target_in_grave(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_RECYCLE ){
			int selected = validate_target_from_grave_source(player, card, player, 0);
			if( selected != -1 ){
				from_grave_to_hand(player, selected, TUTOR_HAND);
			}
		}
	}

	return 0;
}

int card_lifelink(int player, int card, event_t event){
  /* Lifelink	|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has lifelink. (Damage dealt by the creature also causes its controller to gain that much life.) */

	return generic_aura(player, card, event, player, 0, 0, 0, SP_KEYWORD_LIFELINK, 0, 0, 0);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_levitation(int player, int card, event_t event){
  /* Levitation	|2|U|U
   * Enchantment
   * Creatures you control have flying. */

	if( event == EVENT_ABILITIES && affected_card_controller == player && ! is_humiliated(player, card, AL_STATIC) ){
		if( is_what(affected_card_controller,affected_card, TYPE_CREATURE) ){
			event_result |= KEYWORD_FLYING;
		}
	}
	return global_enchantment(player, card, event);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_pentavus(int player, int card, event_t event)
{
  /* Pentavus	|7
   * Artifact Creature - Construct 0/0
   * ~ enters the battlefield with five +1/+1 counters on it.
   * |1, Remove a +1/+1 counter from ~: Put a 1/1 colorless Pentavite artifact creature token with flying onto the battlefield.
   * |1, Sacrifice a Pentavite: Put a +1/+1 counter on ~. */

	if( (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST) && current_turn == player ){
		test_definition_t test;
		new_default_test_definition(&test, 0, "Select a Pentavite to sacrifice.");
		test.subtype = SUBTYPE_PENTAVITE;
		int num_can_sac = max_can_sacrifice_as_cost(player, card, &test);
		if (num_can_sac <= 0)
			return 0;
		return generic_shade_amt_can_pump(player, card, 1, 0, MANACOST_X(1), num_can_sac);
	}

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 5);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_TOKEN = 1,
		CHOICE_COUNTER
	};

	test_definition_t test;
	new_default_test_definition(&test, 0, "Select a Pentavite to sacrifice.");
	test.subtype = SUBTYPE_PENTAVITE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( count_counters(player, card, COUNTER_P1_P1) || new_can_sacrifice_as_cost(player, card, &test) ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int choice = DIALOG(player, card, event,DLG_RANDOM,
							"Counter to Pentavite", count_counters(player, card, COUNTER_P1_P1), current_turn != player ? 10 : 1,
							"Pentavite to counter", new_can_sacrifice_as_cost(player, card, &test), current_turn == player ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		switch (choice)
		{
			case CHOICE_TOKEN:
			{
				if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
					remove_counters(player, card, player, card, COUNTER_P1_P1, 1);
				}
				break;
			}

			case CHOICE_COUNTER:
			{
				if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
					new_sacrifice(player, card, player, 0, &test);
				}
				break;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		switch (choice)
		{
			case CHOICE_TOKEN:
				generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_PENTAVITE);
				break;

			case CHOICE_COUNTER:
				add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 1);
				break;
		}
	}

	return 0;
}
