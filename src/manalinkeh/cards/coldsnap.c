#include "manalink.h"

static char *can_have_counters_placed_on_it_shim(int who_chooses, int player, int card, int targeting_player, int targeting_card);

//-------- GLOBAL FUNCTIONS

static int coldsnap_crusaders(int player, int card, event_t event, int mana_color)
{

  if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
  {
    int c[6] = {0};
    c[mana_color] = 2;
    return generic_shade(player, card, event, 0, c[0], c[1], c[2], c[3], c[4], c[5], 1, 0, 0, 0);
  }

  if (event == EVENT_CLEANUP)
  {
    get_card_instance(player, card)->targets[1].player = 0;
  }

  if (!IS_GAA_EVENT(event))
  {
    return 0;
  }

  enum
  {
    CHOICE_FLYING = 1,
    CHOICE_PUMP,
  };

  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_CAN_ACTIVATE)
  {
    int c[6] = {0};
    c[mana_color] = 1;
    return generic_activated_ability(player, card, event, 0, c[0], c[1], c[2], c[3], c[4], c[5], 0,
                                     NULL, NULL);
  }

  if (event == EVENT_ACTIVATE)
  {
    int abils[3] = {0, 1, 0};
    int c[6] = {0};
    c[mana_color] = 2;
    if (has_mana_for_activated_ability(player, card, c[0], c[1], c[2], c[3], c[4], c[5]))
    {
      abils[2] = 1;
    }
    int priorities[3] = {
        0,
        (instance->targets[1].player > -1 && !(instance->targets[1].player & KEYWORD_FLYING)) &&
                current_phase < PHASE_DECLARE_BLOCKERS
            ? 10
            : 1,
        current_phase == PHASE_AFTER_BLOCKING ? 5 : 1,
    };
    int choice = DIALOG(player, card, event, DLG_RANDOM, "Gains Flying", abils[1], priorities[1],
                        "+1/+0 until EOT", abils[2], priorities[2]);
    if (!choice)
    {
      spell_fizzled = 1;
      return 0;
    }
    c[mana_color] = choice;
    if (charge_mana_for_activated_ability(player, card, c[0], c[1], c[2], c[3], c[4], c[5]))
    {
      if (instance->targets[1].player < 0)
      {
        instance->targets[1].player = 0;
      }
      instance->targets[1].player |= (choice == CHOICE_FLYING ? KEYWORD_FLYING : 0);
    }
  }

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->info_slot == CHOICE_FLYING)
    {
      alternate_legacy_text(1, player,
                            pump_ability_until_eot(instance->parent_controller,
                                                   instance->parent_card,
                                                   instance->parent_controller,
                                                   instance->parent_card, 0, 0, KEYWORD_FLYING, 0));
    }

    if (instance->info_slot == CHOICE_PUMP)
    {
      pump_until_eot_merge_previous_alt(player, card, instance->parent_controller,
                                        instance->parent_card, 1, 0, 2);
    }
  }

  return 0;
}

static void ripple(int player, int card){
	if( ! check_special_flags(player, card, SF_NOT_CAST) ){
		int max = 4;
		int *deck = deck_ptr[player];
		if( max > count_deck(player) ){
			max = count_deck(player);
		}
		if( max > 0){
			char buffer[100];
			card_ptr_t* c = cards_ptr[ get_id(player, card)  ];
			scnprintf(buffer, 100, "Cards revealed by %s", c->name);
			show_deck(HUMAN, deck, max, buffer, 0, gs_done_008b40e0 );
			int i;
			for(i=max-1; i > -1; i--){
				if( cards_data[deck[i]].id == get_id(player, card) ){
					char buffer2[100];
					card_ptr_t* c2 = cards_ptr[ cards_data[deck[i]].id  ];
					scnprintf(buffer2, 100, " Play %s\n Pass", c2->name);
					int choice = do_dialog(player, player, card, -1, -1, buffer2, 0);
					if( choice == 0 ){
						play_card_in_deck_for_free(player, player, i);
						max--;
					}
				}
			}
			if( max > 0 ){
				put_top_x_on_bottom(player, player, max);
			}
		}
	}
}

static int casting_coldsnap_pitchspell(int player, int card, event_t event, test_definition_t *this_test){

	card_instance_t *instance = get_card_instance(player, card);

	if( played_for_free(player, card) || is_token(player, card) || instance->info_slot != 1){
		return 1;
	}

	int choice = 0;
	if( has_mana_to_cast_id(player, event, get_id(player, card)) ){
		choice = do_dialog(player, player, card, -1, -1, " RFG two cards in hand\n Cast normally\n Cancel", 0);
	}
	if( choice == 0 ){
		int selected[2] = {-1, -1};
		int h_array[2][100];
		int ha_count = 0;
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) && i != card ){
				h_array[0][ha_count] = get_original_internal_card_id(player, i);
				h_array[1][ha_count] = i;
				ha_count++;
			}
		}
		for(i=0; i<2; i++){
			selected[i] = select_card_from_zone(player, player, h_array[0], ha_count, 0, AI_MIN_VALUE, -1, this_test);
			if( selected[i] != -1 ){
				h_array[0][selected[i]] = iid_draw_a_card;
			}
			else{
				break;
			}
		}
		if( selected[0] == -1 || selected[1] == -1 ){
			return 0;
		}
		else{
			for(i=0; i<2; i++){
				rfg_card_in_hand(player, h_array[1][selected[i]]);
			}
			return 1;
		}
	}
	if( choice == 1 ){
		charge_mana_from_id(player, -1, event, get_id(player, card));
		if( spell_fizzled != 1 ){
			return 1;
		}
	}
	return 0;
}

static void cost_mod_for_coldsnap_pitchspell(int player, int card, event_t event, test_definition_t *this_test){
	if( event == EVENT_MODIFY_COST ){
		if( check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, this_test) > 2 ){
			null_casting_cost(player, card);
			get_card_instance(player, card)->info_slot = 1;
		}
		else{
			get_card_instance(player, card)->info_slot = 0;
		}
	}
}

static int coldsnap_pitchspell(int player, int card, event_t event, test_definition_t *this_test){

	cost_mod_for_coldsnap_pitchspell(player, card, event, this_test);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! casting_coldsnap_pitchspell(player, card, event, this_test) ){
			spell_fizzled = 1;
		}
	}
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

//-------- CARDS

static int effect_valkyrie(int player, int card, event_t event)
{

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    card_instance_t *instance = get_card_instance(player, card);

    int p = instance->targets[0].player;
    int c = instance->targets[0].card;

    if (affect_me(p, c) && !check_for_death_trigger_removal(p, c))
    {
      card_instance_t *affected = get_card_instance(p, c);
      if (affected->kill_code > 0 && affected->kill_code < KILL_EXILE)
      {
        instance->targets[11].player = 66;
        // Unattach
        instance->targets[0].player = instance->damage_target_player;
        instance->targets[0].card = instance->damage_target_card;
        instance->damage_target_player = -1;
        instance->damage_target_card = -1;
      }
    }
  }

  if (resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int owner, position;
    if (find_in_owners_graveyard(instance->targets[0].player, instance->targets[0].card, &owner,
                                 &position))
    {
      reanimate_permanent(player, card, owner, position, REANIMATE_NO_CONTROL_LEGACY);
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

int card_adarkar_valkyrie(int player, int card, event_t event){
/* CARD_ID_ADARKAR_VALKYRIE	8328
Adarkar Valkyrie	|4|W|W
Snow Creature - Angel 4/5
Flying, vigilance
|T: When target creature other than ~ dies this turn, return that card to the battlefield under your control. */

	vigilance(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &effect_valkyrie,
											instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_NOT_ME_AS_TARGET, MANACOST_X(0), 0,
									&td, "TARGET_CREATURE");
}

int card_allosaurus_rider(int player, int card, event_t event)
{
  /* CARD_ID_ALLOSAURUS_RIDER	8330
   * Allosaurus Rider	|5|G|G
   * Creature - Elf Warrior 101/101
   * You may exile two |Sgreen cards from your hand rather than pay ~'s mana cost.
   * ~'s power and toughness are each equal to 1 plus the number of lands you control. */

  test_definition_t this_test;
  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if (((card == affected_card) && (affected_card_controller == player)) && (player != -1))
    {
      set_characteristic_pt(landsofcolor_controlled[player][7] + 1);
    }
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      return 0;
  }
  else if (event != EVENT_CAN_CAST && event != EVENT_MODIFY_COST && event != EVENT_RESOLVE_SPELL &&
           event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  new_default_test_definition(&this_test, 0xff, "Select a green card to exile");
  this_test.color = 8;
  this_test.zone = 0x100;
  this_test.id = CARD_ID_RULES_ENGINE;
  this_test.id_flag = 1;
  return coldsnap_pitchspell(player, card, event, &this_test);
}

int card_arctic_nishoba(int player, int card, event_t event)
{
  /* Arctic Nishoba	|5|G
   * Creature - Cat Warrior 6/6
   * Trample
   * Cumulative upkeep |G or |W
   * When ~ dies, you gain 2 life for each age counter on it. */

  cumulative_upkeep_hybrid(player, card, event, 1, COLOR_GREEN, COLOR_WHITE, 0);

  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	gain_life(player, 2 * count_counters(player, card, COUNTER_AGE));

  return 0;
}

int card_arcum_dagsson(int player, int card, event_t event){
/* CARD_ID_ARCUM_DAGSSON	8333
Arcum Dagsson	|3|U
Legendary Creature - Human Artificer 2/2
|T: Target artifact creature's controller sacrifices it.
That player may search his or her library for a noncreature artifact card, put it onto the battlefield, then shuffle his or her library. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.special = TARGET_SPECIAL_ARTIFACT_CREATURE;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) && can_sacrifice(player, instance->targets[0].player, 1, TYPE_CREATURE, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);

			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select a non-creature artifact.");
			this_test.type_flag = F1_NO_CREATURE;
			new_global_tutor(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC,
							&this_test);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target artifact creature.");
}

int card_bull_aurochs(int player, int card, event_t event)
{
/* CARD_ID_BULL_AUROCHS	8346
Bull Aurochs	|1|G
Creature - Aurochs 2/1
Trample
Whenever ~ attacks, it gets +1/+0 until end of turn for each other attacking Aurochs. */
  // Used externally
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
	{
	  test_definition_t this_test;
	  default_test_definition(&this_test, TYPE_PERMANENT);
	  this_test.subtype = SUBTYPE_AUROCHS;
	  this_test.state = STATE_ATTACKING;
	  this_test.not_me = 1;

	  int amount = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test);
	  pump_until_eot(player, card, player, card, amount, 0);
	}

  return 0;
}

int card_aurochs_herd(int player, int card, event_t event){
/* CARD_ID_AUROCHS_HERD	8334
Aurochs Herd	|5|G
Creature - Aurochs 4/4
Trample
When ~ enters the battlefield, you may search your library for an Aurochs card, reveal it, and put it into your hand. If you do, shuffle your library.
Whenever ~ attacks, it gets +1/+0 until end of turn for each other attacking Aurochs. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "Select an Aurochs card.");
		this_test.subtype = SUBTYPE_AUROCHS;

		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return card_bull_aurochs(player, card, event);
}

int card_balduvian_fallen(int player, int card, event_t event)
{
  /* Balduvian Fallen	|3|B
   * Creature - Zombie 3/5
   * Cumulative upkeep |1
   * Whenever ~'s cumulative upkeep is paid, it gets +1/+0 until end of turn for each |B or |R spent this way. */

  cumulative_upkeep(player, card, event, MANACOST_X(1));
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY && in_play(player, card)	// Cumulative upkeep was just successfully paid
	  && (mana_paid[COLOR_BLACK] || mana_paid[COLOR_RED]))
	pump_until_eot(player, card, player, card, mana_paid[COLOR_BLACK] + mana_paid[COLOR_RED], 0);

  return 0;
}

int card_balduvian_rage(int player, int card, event_t event){
/* CARD_ID_BALDUVIAN_RAGE	8337
Balduvian Rage	|X|R
Instant
Target attacking creature gets +X/+0 until end of turn.
Draw a card at the beginning of the next turn's upkeep. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, instance->info_slot, 0);
			cantrip(player, card, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT | GS_X_SPELL, &td,
						"Select target attacking creature.", 1, NULL);
}

int card_blizzard_specter(int player, int card, event_t event){
  /* Blizzard Specter |2|U|B
   * Snow Creature — Specter 2/3
   * Flying
   * Whenever Blizzard Specter deals combat damage to a player, choose one —
   * • That player returns a permanent he or she controls to its owner's hand.
   * • That player discards a card. */

	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE))
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;

		int i;
		for(i=0; i<2; i++){
			if( times_damaged[i] ){
				target_definition_t td;
				default_target_definition(player, card, &td, TYPE_PERMANENT);
				td.allowed_controller = i;
				td.preferred_controller = i;
				td.illegal_abilities = 0;
				td.who_chooses = i;
				td.allow_cancel = 0;
				char buffer1[100];
				scnprintf(buffer1, 100, "%s discard a card", i == player ? "You" : "Opponent");
				char buffer2[100];
				scnprintf(buffer2, 100, "%s bounce a permanent", i == player ? "You" : "Opponent");
				int priorities[3] = {0, 0, 0};
				if(player == IS_AI(player)){
				   if( player == i ){
					   if( hand_count[player] == 0 || target_available(player, card, &td) <= 6 ){
						   priorities[1] = 10;
						   priorities[2] = 1;
					   }
					   else{
						    priorities[1] = 1;
						    priorities[2] = 10;
					   }
				   }
				    else{
					     if( target_available(player, card, &td) <= 3 || hand_count[i] == 0 ){
						     priorities[1] = 1;
						     priorities[2] = 10;
					     }
					     else{
						      priorities[1] = 10;
						      priorities[2] = 1;
					     }
				    }
				}
				int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_RANDOM,
									buffer1, 1, priorities[1],
									buffer2, 1, priorities[2]);
				if( choice == 1 ){
					discard(i, 0, player);
				}
				if( choice == 2 ){
					if( new_pick_target(&td, "Select a permenant you control.", 0, 0 | GS_LITERAL_PROMPT) ){
						bounce_permanent(instance->targets[0].player, instance->targets[0].card);
					}
				}
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_boreal_druid(int player, int card, event_t event){
/* CARD_ID_BOREAL_DRUID	8341
Boreal Druid	|G
Snow Creature - Elf Druid 1/1
|T: Add |C to your mana pool. */

	return mana_producing_creature(player, card, event, 24, COLOR_COLORLESS, 1);
}

static int rtrue(int player, int card, int number_of_age_counters)
{
  return 1;
}

static int produce_mana_red_thunk(int player, int card, int number_of_age_counters)
{
  produce_mana(player, COLOR_RED, number_of_age_counters);
  return 1;
}

int card_braid_of_fire(int player, int card, event_t event)
{
  /* Braid of Fire	|1|R
   * Enchantment
   * Cumulative upkeep-Add |R to your mana pool. */

  cumulative_upkeep_general(player, card, event, rtrue, produce_mana_red_thunk);

  return global_enchantment(player, card, event);
}

// controvert --> counterspell

int card_controvert(int player, int card, event_t event){
/* CARD_ID_CONTROVERT	8351
Controvert	|2|U|U
Instant
Counter target spell.
Recover |2|U|U (When a creature is put into your graveyard from the battlefield, you may pay |2|U|U.
If you do, return this card from your graveyard to your hand. Otherwise, exile this card.) */

	return card_counterspell(player, card, event);
}

int card_counterbalance(int player, int card, event_t event)
{
/* CARD_ID_COUNTERBALANCE	8352
Counterbalance	|U|U
Enchantment
Whenever an opponent casts a spell, you may reveal the top card of your library.
If you do, counter that spell if it has the same converted mana cost as the revealed card. */

  if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller
	  && trigger_cause_controller == 1-player && !is_what(trigger_cause_controller, trigger_cause, TYPE_LAND)
	  && deck_ptr[player][0] != -1)
	{
	  if (event == EVENT_TRIGGER)
		{
		  if ((player == AI || ai_is_speculating == 1) && !check_state(trigger_cause_controller, trigger_cause, STATE_CANNOT_TARGET))
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		  else
			event_result |= RESOLVE_TRIGGER_OPTIONAL;
		}

	  if (event == EVENT_RESOLVE_TRIGGER)
		{
		  reveal_card_iid(player, card, deck_ptr[player][0]);
		  if (get_cmc_by_internal_id(deck_ptr[player][0]) == get_cmc(trigger_cause_controller, trigger_cause)
			  && !check_state(trigger_cause_controller, trigger_cause, STATE_CANNOT_TARGET))
			real_counter_a_spell(player, card, trigger_cause_controller, trigger_cause);
		}
	}

  return global_enchantment(player, card, event);
}

int card_coldsteel_heart(int player, int card, event_t event){

	/* Coldsteel Heart	|2
	 * Snow Artifact
	 * ~ enters the battlefield tapped.
	 * As ~ enters the battlefield, choose a color.
	 * |T: Add one mana of the chosen color to your mana pool. */

	comes_into_play_tapped(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t* instance = get_card_instance(player, card);
		instance->info_slot = 1 << choose_a_color(player, get_deck_color(player, player));
	}

	return plain_mana_producer(player, card, event);
}

/* ManalinkEh 2022: 0x2162120; original cards/coldsnap.c:586. */
int card_commandeer(int player, int card, event_t event)
{
  /* Commandeer	|5|U|U
   * Instant
   * You may exile two blue cards from your hand rather than pay this spell's mana cost.
   * Gain control of target noncreature spell. You may choose new targets for it. (If that spell is an artifact, enchantment, or planeswalker, the permanent enters under your control.) */

  int result;
  card_instance_t *target_spell;
  int card_00;
  target_definition_t *spell_target_definition;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t this_test;

  if ((event == EVENT_CAN_CAST) || (event == EVENT_MODIFY_COST))
  {
    counterspell_target_definition(player, card, &td, 0xff);
    td.illegal_type = TYPE_CREATURE;
    new_default_test_definition(&this_test, 0xff, "Select a blue card to exile");
    this_test.color = 4;
    this_test.zone = 0x100;
    this_test.id = CARD_ID_RULES_ENGINE;
    this_test.id_flag = 1;
    instance = get_card_instance(player, card);
    cost_mod_for_coldsnap_pitchspell(player, card, event, &this_test);
    if (event == EVENT_CAN_CAST)
    {
      return counterspell(player, card, EVENT_CAN_CAST, &td, -1);
    }
  }
  else
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
      counterspell_target_definition(player, card, &td, 0xff);
      td.illegal_type = TYPE_CREATURE;
      new_default_test_definition(&this_test, 0xff, "Select a blue card to exile");
      this_test.color = 4;
      this_test.zone = 0x100;
      this_test.id = CARD_ID_RULES_ENGINE;
      this_test.id_flag = 1;
      instance = get_card_instance(player, card);
      cost_mod_for_coldsnap_pitchspell(player, card, EVENT_CAST_SPELL, &this_test);
      spell_target_definition = &td;
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
      result = casting_coldsnap_pitchspell(player, card, EVENT_CAST_SPELL, &this_test);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      target_spell = get_card_instance(card_on_stack_controller, card_on_stack);
      if (target_spell->number_of_targets == '\x01')
      {
        instance->info_slot = 1;
        return deflect_one_target(player, card, EVENT_CAST_SPELL, NULL);
      }
      target_spell = get_card_instance(card_on_stack_controller, card_on_stack);
      if (target_spell->number_of_targets < 2)
      {
        return counterspell(player, card, EVENT_CAST_SPELL, spell_target_definition, 0);
      }
      instance->info_slot = 2;
      return deflect_multi_targets(player, card, EVENT_CAST_SPELL, NULL);
    }
    if ((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET))
    {
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
      counterspell_target_definition(player, card, &td, 0xff);
      td.illegal_type = TYPE_CREATURE;
      new_default_test_definition(&this_test, 0xff, "Select a blue card to exile");
      this_test.color = 4;
      this_test.zone = 0x100;
      this_test.id = CARD_ID_RULES_ENGINE;
      this_test.id_flag = 1;
      get_card_instance(player, card);
      cost_mod_for_coldsnap_pitchspell(player, card, EVENT_CHANGE_TARGET, &this_test);
      return 0;
    }
    counterspell_target_definition(player, card, &td, 0xff);
    td.illegal_type = TYPE_CREATURE;
    new_default_test_definition(&this_test, 0xff, "Select a blue card to exile");
    this_test.color = 4;
    this_test.zone = 0x100;
    this_test.id = CARD_ID_RULES_ENGINE;
    this_test.id_flag = 1;
    instance = get_card_instance(player, card);
    cost_mod_for_coldsnap_pitchspell(player, card, event, &this_test);
  }
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }
  result = counterspell_validate(player, card, NULL, 0);
  if (result != 0)
  {
    result = instance->info_slot;
    if (result == 1)
    {
      deflect_one_target(player, card, EVENT_RESOLVE_SPELL, NULL);
      result = instance->info_slot;
    }
    if (result == 2)
    {
      deflect_multi_targets(player, card, EVENT_RESOLVE_SPELL, NULL);
    }
    result = is_what(instance->targets[0].player, instance->targets[0].card, 0x144);
    if (result != 0)
    {
      target_spell = get_card_instance(instance->targets[0].player, instance->targets[0].card);
      result = target_spell->internal_card_id;
      if (result != -1)
      {
        obliterate_card(instance->targets[0].player, instance->targets[0].card);
        card_00 = add_card_to_hand(player, result);
        put_into_play(player, card_00);
        result = instance->targets[0].player;
        if (result != player)
        {
          if (result == 1)
          {
            add_state(player, card_00, 0x1000);
          }
          else
          {
            remove_state(player, card_00, 0x1000);
          }
        }
      }
    }
  }
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

static int has_snow_mana_thunk(int player, int card, int number_of_age_counters)
{
  return has_snow_mana(player, number_of_age_counters, 0);
}

static int charge_snow_mana_while_processing(int player, int card, int number_of_age_counters)
{
  put_card_or_activation_onto_stack(player, card, EVENT_RESOLVE_TRIGGER, 0, 0);
  ldoubleclicked = spell_fizzled = 0;
  int rval = charge_snow_mana(player, card, number_of_age_counters, 0);
  obliterate_top_card_of_stack();
  return rval;
}

int card_cover_of_winter(int player, int card, event_t event){
  /* Cumulative upkeep |I
	 * If a creature would deal combat damage to you and/or one or more creatures you control, prevent X of that damage, where X is the number of age counters
	 * on ~.
	 * |I: Put an age counter on ~. */

	cumulative_upkeep_general(player, card, event, has_snow_mana_thunk, charge_snow_mana_while_processing);

	card_instance_t* damage = combat_damage_being_prevented(event);
	if( damage &&
		damage->damage_target_player == player &&
		(damage->targets[3].player & TYPE_CREATURE) &&	// probably redundant to status check
		!check_special_flags(damage->damage_source_player, damage->damage_source_card, SF_ATTACKING_PWALKER)
	  ){
		if( damage->info_slot < count_counters(player, card, COUNTER_AGE) ){
			damage->info_slot = 0;
		}
		else{
			damage->info_slot-=count_counters(player, card, COUNTER_AGE);
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, 0, 0, 0, 0, 0, 0);
		return can_use_activated_abilities(player, card) && has_snow_mana(player, 1, c1);
	}

	if( event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, 0, 0, 0, 0, 0, 0);
		charge_snow_mana(player, card, 1, c1);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t* instance = get_card_instance(player, card);
		add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_AGE, 1);
	}

	return global_enchantment(player, card, event);
}

int card_darien_king_of_kjeldor(int player, int card, event_t event){
	/* Darien, King of Kjeldor	|4|W|W
	 * Legendary Creature - Human Soldier 3/3
	 * Whenever you're dealt damage, you may put that many 1/1 |Swhite Soldier creature tokens onto the battlefield. */

	check_legend_rule(player, card, event);

	if(event == EVENT_DEAL_DAMAGE){
		check_damage(ANYBODY, -1, event, DDBM_REPORT_DAMAGE_DEALT | DDBM_MUST_DAMAGE_TRIGGERING_PLAYER, player, card);
	}

	if( resolve_damage_trigger(player, card, event, DDBM_REPORT_DAMAGE_DEALT | DDBM_MUST_DAMAGE_TRIGGERING_PLAYER, player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		int amount = instance->targets[16].player;
		instance->targets[16].player = 0;

		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SOLDIER, &token);
		token.pow = token.tou = 1;
		token.color_forced = COLOR_TEST_WHITE;
		token.qty = amount;
		generate_token(&token);
	}

	return 0;
}

int card_dark_depths(int player, int card, event_t event){
  /* Dark Depths	""
   * Legendary Snow Land
   * ~ enters the battlefield with ten ice counters on it.
   * |3: Remove an ice counter from ~.
   * When ~ has no ice counters on it, sacrifice it. If you do, put a legendary 20/20 |Sblack Avatar creature token with flying and indestructible named Marit Lage onto the battlefield. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ABILITY = 2
  };




	card_instance_t *instance = get_card_instance(player, card);
	check_legend_rule(player, card, event);
	if (event == EVENT_RESOLVE_SPELL){ play_land_sound_effect(player, card); }
	if (!is_humiliated(player, card, AL_TRIG)){
	    enters_the_battlefield_with_counters(player, card, event, COUNTER_ICE, 10);
	}
	if (event == EVENT_STATIC_EFFECTS && count_counters(player, card, COUNTER_ICE) == 0 && !is_humiliated(player, card, AL_TRIG)){
	    kill_card(player, card, KILL_SACRIFICE);
	    generate_token_by_id(player, card, CARD_ID_MARIT_LAGE);
	}
	if (event == EVENT_CAN_ACTIVATE && mana_colors_added[player]){ return mana_producer(player, card, event); }
	if (event == EVENT_ACTIVATE){
	    if (mana_colors_added[player]){
	        if (paying_mana() || !generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(4), 0, NULL, NULL)){
	            return mana_producer(player, card, event);
	        }
	        if (!IS_AI(player)){
	            int choice = DIALOG(player, card, event, DLG_RANDOM, "Get mana", 1, 1, "Use Ability", 1, 1);
	            if (!choice){ spell_fizzled = 1; return 0; }
	            if (choice == CHOICE_MANA){ return mana_producer(player, card, event); }
	            if (choice != CHOICE_ABILITY){ return generic_activated_ability(player, card, event, 0, MANACOST_X(3), 0, NULL, NULL); }
	        }
	    }
	    instance->info_slot = CHOICE_ABILITY;
	    return generic_activated_ability(player, card, event, 0, MANACOST_X(3), 0, NULL, NULL);
	}
	if (event == EVENT_RESOLVE_ACTIVATION && instance->info_slot == CHOICE_ABILITY){
	    remove_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_ICE, 1);
	}
	if (IS_GAA_EVENT(event)){ return generic_activated_ability(player, card, event, 0, MANACOST_X(3), 0, NULL, NULL); }
	return mana_colors_added[player] ? mana_producer(player, card, event) : 0;
}

int card_marit_lage(int player, int card, event_t event){
/* CARD_ID_MARIT_LAGE	17446
Marit Lage	""(token)
Legendary Creature - Avatar 20/20
Flying
Indestructible */

	check_legend_rule(player, card, event);
	indestructible(player, card, event);
	return 0;
}

static int target_for_deepfire_elemental(int player, int card, int mode){
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_CREATURE);

	card_instance_t *instance= get_card_instance(player, card);

	int i;
	int trg = -1;
	int par = -1;
	for(i=0; i<2; i++){
		if( i == 1-player || player == HUMAN ){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && is_what(i, count, TYPE_ARTIFACT | TYPE_CREATURE) ){
						int cost = (get_cmc(i, count)*2)+1;
						if( has_mana_for_activated_ability(player, card, MANACOST_X(cost)) ){
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

int card_deepfire_elemental(int player, int card, event_t event){
  /* Deepfire Elemental |4|B|R
   * Creature � Elemental 4/4
   * {X}{X}{1}: Destroy target artifact or creature with converted mana cost X. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_CREATURE);

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
		return target_for_deepfire_elemental(player, card, 0);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int op = -1;
		int oc = -1;
		if( player == HUMAN ){
			if( select_target(player, card, &td, "Select target creature or artifact", NULL) ){
				op = instance->targets[0].player;
				oc = instance->targets[0].card;
				int cost = (2*get_cmc( instance->targets[0].player, instance->targets[0].card ))+1;
				if( ! has_mana_for_activated_ability(player, card, MANACOST_X(cost)) ){
					spell_fizzled = 1;
					return 0;
				}
				if( charge_mana_for_activated_ability(player, card, MANACOST_X(cost)) ){
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
		else if( player == AI ){
				int result = target_for_deepfire_elemental(player, card, 1);
				if( result != -1 ){
					int cost = (2*get_cmc( instance->targets[0].player, instance->targets[0].card ))+1;
			        charge_mana_for_activated_ability(player, card, MANACOST_X(cost));
					instance->targets[0].player = 1-player;
					instance->targets[0].card = result;
					instance->number_of_targets = 1;
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

int card_diamond_faerie(int player, int card, event_t event){

	/* Diamond Faerie	|2|G|W|U
	 * Snow Creature - Faerie 3/3
	 * Flying
	 * |1|I: Snow creatures you control get +1/+1 until end of turn. */

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, 1, 0, 0, 0, 0, 0);
		return can_use_activated_abilities(player, card) && has_snow_mana(player, 1, c1);
	}

	if( event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, 1, 0, 0, 0, 0, 0);
		charge_snow_mana(player, card, 1, c1);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, SUBTYPE_SNOW, 1, 1, 0, 0);
	}

	return 0;
}

int card_disciple_of_tevesh_szat(int player, int card, event_t event){
  /* Disciple of Tevesh Szat |2|B|B
   * Creature � Human Cleric 3/1
   * {T}: Target creature gets -1/-1 until end of turn.
   * {4}{B}{B}, {T}, Sacrifice Disciple of Tevesh Szat: Target creature gets -6/-6 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_M1_M1 = 1,
		CHOICE_M6_M6
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			return 1;
	    }
		 return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_XB(4, 2), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						   0,
		                   generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL),
						   generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XB(4, 2), 0, &td, NULL)
		};

			int choice = DIALOG(player, card, event, DLG_RANDOM,
								  "Give -1/-1", 		abils[1],	5,
								  "Sac and give -6/-6",	abils[2], 	10);
			if( ! choice ){
				  spell_fizzled = 1;
				  return 0;
			}

			if( choice == CHOICE_M1_M1 ){
				return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
			}
			if( choice == CHOICE_M6_M6 ){
				return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_XB(4, 2), 0, &td, "TARGET_CREATURE");
			}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == CHOICE_M1_M1 && valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, -1, -1);
		}
		if( choice == CHOICE_M6_M6 && valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, -6, -6);
		}
	}
	return 0;
}

/* ManalinkEh 2022: 0x2163780; original cards/coldsnap.c:1035. */
int card_drelnoch(int player, int card, event_t event)
{
  /* Drelnoch	|4|U
   * Creature - Yeti Mutant 3/3
   * Whenever this creature becomes blocked, you may draw two cards. */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((event == EVENT_DECLARE_BLOCKERS) && (result == 0))
    {
      result = is_attacking(player, card);
      if (result != 0)
      {
        result = is_unblocked(player, card);
        if (result == 0)
        {
          result = count_deck(player);
          result = do_dialog(player, player, card, -1, -1, " Draw two cards\n Pass",
                             (unsigned int)(0x10 < result));
          if (result == 0)
          {
            draw_cards(player, 2);
          }
        }
      }
    }
  }
  return 0;
}

int card_earthen_goo(int player, int card, event_t event)
{
  /* Earthen Goo	|2|R
   * Creature - Ooze 2/2
   * Trample
   * Cumulative upkeep |R or |G
   * ~ gets +1/+1 for each age counter on it. */

	cumulative_upkeep_hybrid(player, card, event, 1, COLOR_RED, COLOR_GREEN, 0);

	if ( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && in_play(player, card) &&
		! is_humiliated(player, card, AL_STATIC) )
		event_result += count_counters(player, card, COUNTER_AGE);

	return 0;
}

int card_feast_of_flesh(int player, int card, event_t event){
/* CARD_ID_FEAST_OF_FLESH	8363
Feast of Flesh	|B
Sorcery
~ deals X damage to target creature and you gain X life, where X is 1 plus the number of cards named ~ in all graveyards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = count_graveyard_by_id(player, get_id(player, card))+1;
			damage_creature(instance->targets[0].player, instance->targets[0].card, amount, player, card);
			gain_life(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_field_marshal( int player, int card, event_t event){
/* CARD_ID_FIELD_MARSHAL	1327
Field Marshal	|1|W|W
Creature - Human Soldier 2/2
Other Soldier creatures get +1/+1 and have first strike. (They deal combat damage before creatures without first strike.) */

	return boost_creature_type(player, card, event, SUBTYPE_SOLDIER, 1, 1, KEYWORD_FIRST_STRIKE, 0);
}

int card_frostweb_spider(int player, int card, event_t event){
/* CARD_ID_FROSTWEB_SPIDER	8368
Frostweb Spider	|2|G
Snow Creature - Spider 1/3
Reach (This creature can block creatures with flying.)
Whenever ~ blocks a creature with flying, put a +1/+1 counter on ~ at end of combat. */

	if( current_turn != player && event == EVENT_DECLARE_BLOCKERS && blocking(player, card, event) && ! is_humiliated(player, card, AL_TRIG) ){
		if( check_for_ability(1-player, get_card_instance( player, card )->blocking, KEYWORD_FLYING) ){
			get_card_instance( player, card )->info_slot = 66;
		}
	}

	if( get_card_instance( player, card )->info_slot == 66 && ! is_humiliated(player, card, AL_TRIG)  &&
		end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) )
	{
	 add_counters(player, card, player, card, COUNTER_P1_P1, 1);
	 get_card_instance( player, card )->info_slot = 0;
	}

	return 0;
}

int card_frozen_solid(int player, int card, event_t event){
/* CARD_ID_FROZEN_SOLID	8369
Frozen Solid	|1|U|U
Enchantment - Aura
Enchant creature
Enchanted creature doesn't untap during its controller's untap step.
When enchanted creature is dealt damage, destroy it. */

	if( in_play(player, card) && get_card_instance( player, card )->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance( player, card );
		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);
		if( damage_dealt_to_me_arbitrary(instance->damage_target_player, instance->damage_target_card, event, 0, player, card) ){
			kill_card(instance->damage_target_player, instance->damage_target_card, KILL_DESTROY);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return disabling_aura(player, card, event);
}

/* ManalinkEh 2022: 0x2163d80; original cards/coldsnap.c:1151. */
int card_fury_of_the_horde(int player, int card, event_t event)
{
  /* Fury of the Horde	|5|R|R
   * Sorcery
   * You may exile two red cards from your hand rather than pay this spell's mana cost.
   * Untap all creatures that attacked this turn. After this main phase, there is an additional combat phase followed by an additional main phase. */

  int result;
  test_definition_t this_test;
  test_definition_t this_test1;

  if ((event == EVENT_CAN_CAST) || (event == EVENT_MODIFY_COST))
  {
    new_default_test_definition(&this_test, 0xff, "Select a red card to exile");
    this_test.id = (int)cards_data[iid_draw_a_card].id;
    this_test.color = 0x10;
    this_test.zone = 0x100;
    this_test.id_flag = 1;
    cost_mod_for_coldsnap_pitchspell(player, card, event, &this_test);
    if (event == EVENT_CAN_CAST)
    {
      return basic_spell(player, card, EVENT_CAN_CAST);
    }
  }
  else
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
      new_default_test_definition(&this_test, 0xff, "Select a red card to exile");
      this_test.id = (int)cards_data[iid_draw_a_card].id;
      this_test.color = 0x10;
      this_test.zone = 0x100;
      this_test.id_flag = 1;
      cost_mod_for_coldsnap_pitchspell(player, card, EVENT_CAST_SPELL, &this_test);
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
      result = -0x30;
      if (current_turn == player)
      {
        result = (unsigned int)((current_phase == 0x1e) ? 48 : -48);
      }
      ai_modifier = ai_modifier + result;
      result = casting_coldsnap_pitchspell(player, card, EVENT_CAST_SPELL, &this_test);
      if (result != 0)
      {
        return 0;
      }
      spell_fizzled = 1;
      return 0;
    }
    if ((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET))
    {
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
      new_default_test_definition(&this_test, 0xff, "Select a red card to exile");
      this_test.id = (int)cards_data[iid_draw_a_card].id;
      this_test.color = 0x10;
      this_test.zone = 0x100;
      this_test.id_flag = 1;
      cost_mod_for_coldsnap_pitchspell(player, card, EVENT_CHANGE_TARGET, &this_test);
      return 0;
    }
    new_default_test_definition(&this_test, 0xff, "Select a red card to exile");
    this_test.id = (int)cards_data[iid_draw_a_card].id;
    this_test.color = 0x10;
    this_test.zone = 0x100;
    this_test.id_flag = 1;
    cost_mod_for_coldsnap_pitchspell(player, card, event, &this_test);
  }
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }
  default_test_definition(&this_test1, 2);
  this_test1.state = 0x44;
  extra_combat_effect(player, card, EVENT_RESOLVE_SPELL, 1, &this_test1, 1);
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

int card_garza_zol_plague_queen(int player, int card, event_t event){
/* CARD_ID_GARZA_ZOL_PLAGUE_QUEEN	8371
Garza Zol, Plague Queen	|4|U|B|R
Legendary Creature - Vampire 5/5
Flying, haste
Whenever a creature dealt damage by ~ this turn dies, put a +1/+1 counter on ~ Zol.
Whenever ~ Zol deals combat damage to a player, you may draw a card. */

	check_legend_rule(player, card, event);

	haste(player, card, event);

	if( has_combat_damage_been_inflicted_to_a_player(player, card, event) ){
		draw_cards(player, 1);
	}
	if( sengir_vampire_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		add_counters(player, card, player, card, COUNTER_P1_P1, get_card_instance(player, card)->targets[11].card);
	}

	return 0;
}

int card_garzas_assassin(int player, int card, event_t event){
/* CARD_ID_GARZAS_ASSASSIN	8372
Garza's Assassin	|B|B|B
Creature - Human Assassin 2/2
Sacrifice ~: Destroy target non|Sblack creature.
Recover-Pay half your life, rounded up. (When another creature is put into your graveyard from the battlefield,
you may pay half your life, rounded up. If you do, return this card from your graveyard to your hand. Otherwise, exile this card.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, get_sleighted_color_text(player, card, "Select target non%s creature", COLOR_BLACK));
}

int card_gelid_shackles(int player, int card, event_t event){

	/* Gelid Shackles	|W
	 * Snow Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature can't block, and its activated abilities can't be activated.
	 * |I: Enchanted creature gains defender until end of turn. */

	 if( in_play(player, card) && get_card_instance( player, card )->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance( player, card );
		cannot_block(instance->damage_target_player, instance->damage_target_card, event);
		if( event == EVENT_CAN_ACTIVATE ){
			int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST0);
			return can_use_activated_abilities(player, card) && has_snow_mana(player, 1, c1);
		}

		if( event == EVENT_ACTIVATE ){
			int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST0);
			charge_snow_mana(player, card, 1, c1);
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->damage_target_player, instance->damage_target_card, 0, 0, KEYWORD_DEFENDER, 0);
		}
	}

	return generic_aura(player, card, event, 1-player, 0, 0, 0, 0, 0, 0, GA_FORBID_ALL_ACTIVATED_ABILITIES);
}

int card_glacial_plating(int player, int card, event_t event)
{
 /* Glacial Plating	|2|W|W
  * Snow Enchantment - Aura
  * Enchant creature
  * Cumulative upkeep |I
  * Enchanted creature gets +3/+3 for each age counter on ~. */

	cumulative_upkeep_general(player, card, event, has_snow_mana_thunk, charge_snow_mana_while_processing);

	card_instance_t* instance;
	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && (instance = in_play(player, card)) &&
		affect_me(instance->damage_target_player, instance->damage_target_card) && ! is_humiliated(player, card, AL_STATIC) )
		event_result += 3 * count_counters(player, card, COUNTER_AGE);

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	return vanilla_aura(player, card, event, player);
}

int card_grim_harvest(int player, int card, event_t event){
/* CARD_ID_GRIM_HARVEST	8378
Grim Harvest	|1|B
Instant
Return target creature card from your graveyard to your hand.
Recover |2|B (When a creature is put into your graveyard from the battlefield, you may pay |2|B.
If you do, return this card from your graveyard to your hand. Otherwise, exile this card.) */

	return card_raise_dead(player, card, event);
}

/* ManalinkEh 2022: 0x21646a0; original cards/coldsnap.c:1306. */
int card_gristle_grinner(int player, int card, event_t event)
{
  /* Gristle Grinner	|4|B
   * Creature - Zombie 3/3
   * Whenever a creature dies, this creature gets +2/+2 until end of turn. */

  card_instance_t *instance;

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    count_for_gfp_ability(player, card, 0x77, 2, 2, NULL);
  }
  if (resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      pump_until_eot_merge_previous(player, card, player, card, 2, 2);
      return 0;
    }
  }
  return 0;
}

int card_gutless_ghoul(int player, int card, event_t event){
/* CARD_ID_GUTLESS_GHOUL	8380
Gutless Ghoul	|2|B
Snow Creature - Zombie 2/2
|1, Sacrifice a creature: You gain 2 life. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 2);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST_X(1), 0, NULL, NULL);
}

int card_haakon_stromgald_scourge(int player, int card, event_t event)
{
  /* Haakon, Stromgald Scourge	|1|B|B
   * Legendary Creature - Zombie Knight 3/3
   * You may cast ~ from your graveyard, but not from anywhere else.
   * As long as Haakon is on the battlefield, you may play Knight cards from your graveyard.
   * When Haakon dies, you lose 2 life. */

  int grave_index_or_result;
  card_instance_t *instance;
  int knight_iid_or_timing;
  int *grave_cursor;
  test_definition_t this_test;
  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST)
  {
    instance = in_hand(player, card);
    if (instance != NULL)
      infinite_casting_cost();
  }
  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) != 0)
    lose_life(player, 2);
  if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    instance = get_card_instance(player, card);
    charge_mana_multi(player,
                      get_updated_casting_cost(player, -1, instance->internal_card_id,
                                               EVENT_PAY_FLASHBACK_COSTS, 1),
                      2, 0, 0, 0, 0);
    if (spell_fizzled != 1)
    {
      return 1;
    }
    return 0;
  }
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    instance = get_card_instance(player, card);
    grave_index_or_result = get_updated_casting_cost(player, -1, instance->internal_card_id,
                                                     EVENT_GRAVEYARD_ABILITY, 1);
    if (has_mana_multi(player, grave_index_or_result, 2, 0, 0, 0, 0) != 0)
    {
      return 2;
    }
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    knight_iid_or_timing = can_sorcery_be_played(player, EVENT_CAN_ACTIVATE);
    grave_cursor = graveyard_ptr_mutable[player];
    grave_index_or_result = *grave_cursor;
    while (true)
    {
      if (grave_index_or_result == -1)
      {
        return 0;
      }
      grave_index_or_result = has_subtype_by_id(
          player, (unsigned int)cards_data[grave_index_or_result].id, SUBTYPE_KNIGHT);
      if ((grave_index_or_result != 0) &&
          (((knight_iid_or_timing != 0 || ((cards_data[*grave_cursor].type & 0x30) != 0)) &&
            (grave_index_or_result = has_mana_to_cast_id(
                 player, EVENT_CAN_ACTIVATE, (unsigned int)cards_data[*grave_cursor].id),
             grave_index_or_result != 0))))
        break;
      grave_index_or_result = grave_cursor[1];
      grave_cursor = grave_cursor + 1;
    }
    return 1;
  }
  if (event != EVENT_ACTIVATE)
    return 0;
  new_default_test_definition(&this_test, 0, "Select a Knight card.");
  this_test.subtype = 0x67;
  grave_index_or_result = new_select_a_card(player, player, 2, 0, 1, -1, &this_test);
  if (grave_index_or_result != -1)
  {
    grave_cursor = graveyard_ptr_mutable[player] + grave_index_or_result;
    knight_iid_or_timing = *grave_cursor;
    if ((cards_data[knight_iid_or_timing].type & 0x30) == 0)
    {
      if (can_sorcery_be_played(player, EVENT_ACTIVATE) == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      knight_iid_or_timing = *grave_cursor;
    }
    if (has_mana_to_cast_id(player, EVENT_ACTIVATE,
                            (unsigned int)cards_data[knight_iid_or_timing].id) != 0)
    {
      charge_mana_from_id(player, -1, EVENT_ACTIVATE, (unsigned int)cards_data[*grave_cursor].id);
      if (spell_fizzled != 1)
      {
        play_card_in_grave_for_free(player, player, grave_index_or_result);
        cant_be_responded_to = 1;
        return 0;
      }
      return 0;
    }
  }
  spell_fizzled = 1;
  return 0;
}

int card_heidar_rimewind_master(int player, int card, event_t event){
/* CARD_ID_HEIDAR_RIMEWIND_MASTER	8382
Heidar, Rimewind Master	|4|U
Legendary Creature - Human Wizard 3/3
|2, |T: Return target permanent to its owner's hand. Activate this ability only if you control four or more snow permanents. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_CAN_ACTIVATE && count_subtype(player, TYPE_PERMANENT, SUBTYPE_SNOW) < 4){
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0,
									&td, "TARGET_PERMANENT");
}

static int opponent_controls_lands(int player, int card, int number_of_age_counters)
{
  int c;
  for (c = 0; c < active_cards_count[1-player]; ++c)
	if (in_play(1-player, c) && is_what(1-player, c, TYPE_LAND)
		&& --number_of_age_counters == 0)
	  return 1;
  return 0;
}

static int gain_control_of_lands(int player, int card, int number_of_age_counters)
{
  target_definition_t td;
  base_target_definition(player, card, &td, TYPE_LAND);
  td.allowed_controller = 1-player;
  td.preferred_controller = 1-player;

  char marked[2][151] = {{0}};
  if (mark_up_to_n_targets_noload(&td, "Select a land you don't control.", number_of_age_counters, marked) == number_of_age_counters)
	{
	  int c;
	  for (c = 0; c < active_cards_count[1-player]; ++c)
		if (marked[1-player][c] && in_play(1-player, c))
		  gain_control_permanently(player, 1-player, c);
	  return 1;
	}
  else
	return 0;
}

int card_herald_of_leshrac(int player, int card, event_t event)
{
  /* Herald of Leshrac	|6|B
   * Creature - Avatar 2/4
   * Flying
   * Cumulative upkeep-Gain control of a land you don't control.
   * ~ gets +1/+1 for each land you control but don't own.
   * When ~ leaves the battlefield, each player gains control of each land he or she owns that you control. */

  int c;
  card_instance_t *instance;
  int temporary_card;
  int opponent;
  cumulative_upkeep_general(player, card, event, opponent_controls_lands, gain_control_of_lands);
  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (0 < active_cards_count[player]))
    {
      c = 0;
      do
      {
        instance = in_play(player, c);
        if (instance != NULL)
        {
          if (is_what(player, c, 1) != 0)
          {
            if (is_stolen(player, c) != 0)
            {
              event_result = event_result + 1;
            }
          }
        }
        c = c + 1;
      } while (c < active_cards_count[player]);
    }
    leaves_play(player, card, event, 1, 1);
    return 0;
  }
  leaves_play(player, card, event, 1, 1);
  if (event != EVENT_LEAVES_PLAY_ABILITY)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  opponent = 1 - player;
  temporary_card = add_card_to_hand(opponent, instance->internal_card_id);
  get_back_your_permanents(opponent, temporary_card, TYPE_LAND);
  obliterate_card(opponent, temporary_card);
  return 0;
}

int card_hibernations_end(int player, int card, event_t event)
{
  /* Hibernation's End	|4|G
   * Enchantment
   * Cumulative upkeep |1
   * Whenever you pay ~'s cumulative upkeep, you may search your library for a creature card with converted mana cost equal to the number of age counters on ~
   * and put it onto the battlefield. If you do, shuffle your library. */

	cumulative_upkeep(player, card, event, MANACOST_X(1));

	if(event == EVENT_UPKEEP_TRIGGER_ABILITY && in_play(player, card)	// Cumulative upkeep was just successfully paid
	  && (IS_AI(player) || do_dialog(player, player, card, -1, -1, " Search for a creature\n Decline", 0) == 0))
	{
	  int counters = count_counters(player, card, COUNTER_AGE);
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  scnprintf(test.message, 100, "Select a creature card with CMC %d.", counters);
	  test.cmc = counters;
	  new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
	}

  return global_enchantment(player, card, event);
}

int card_icefall(int player, int card, event_t event){
/* CARD_ID_ICEFALL	8386
Icefall	|2|R|R
Sorcery
Destroy target artifact or land.
Recover |R|R (When a creature is put into your graveyard from the battlefield, you may pay |R|R.
If you do, return this card from your graveyard to your hand. Otherwise, exile this card.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND | TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target artifact or land.", 1, NULL);
}

int card_into_the_north(int player, int card, event_t event){
/* CARD_ID_INTO_THE_NORTH	8387
Into the North	|1|G
Sorcery
Search your library for a snow land card and put it onto the battlefield tapped. Then shuffle your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		char msg[100] = "Select a Snow land.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, msg);
		this_test.subtype = SUBTYPE_SNOW;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY_TAPPED, 0, AI_MAX_VALUE, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_jesters_scepter(int player, int card, event_t event){

	/* Jester's Scepter	|3
	 * Artifact
	 * When ~ enters the battlefield, exile the top five cards of target player's library face down. You may look at those cards for as long as they remain
	 * exiled.
	 * |2, |T, Put a card exiled with ~ into its owner's graveyard: Counter target spell if it has the same name as that card. */

#pragma message "They're currently exiled face-up, so it can't possibly work in mutiplayer."

	card_instance_t *instance = get_card_instance(player, card);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( event == EVENT_ETB ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			int leg = 0, idx = 0;
			int max = MIN(5, count_deck(instance->targets[0].player));
			while( max ){
				exiledby_remember(player, card, instance->targets[0].player, deck_ptr[instance->targets[0].player][0], &leg, &idx);
				rfg_card_in_deck(instance->targets[0].player, 0);
				max--;
			}
			instance->targets[1] = instance->targets[0];
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		int leg = 0;
		int idx = 0;
		if( exiledby_find_any(player, card, &leg, &idx) != NULL ){
			int result = generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SPELL_ON_STACK, MANACOST_X(2), 0, NULL, NULL);
			if( result ){
				if( player != AI ){
					return result;
				}
				else{
					int needle = get_original_internal_card_id(card_on_stack_controller, card_on_stack);
					needle |= (instance->targets[1].player == 1 ? 0x80000000 : 0);
					if( exiledby_find(player, card, needle, &leg, &idx) != NULL ){
						return result;
					}
				}
			}
			else{
				if( player == HUMAN ){
					return 1;
				}
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		if( player == HUMAN ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SPELL_ON_STACK, MANACOST_X(2), 0, NULL, NULL) ){
				choice = do_dialog(player, player, card, -1, -1, " Jester's Scepter ability\n Show exiled cards\n Cancel", 0);
			}
			else{
				choice = 1;
			}
		}
		if( choice == 1 ){
			exiledby_choose(player, card, CARD_ID_JESTERS_SCEPTER, EXBY_CHOOSE, 0, "", 1);
			spell_fizzled = 1;
			return 0;
		}
		if( choice == 2 ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == 0 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
				int iid = -1;
				if( player == HUMAN ){
					int rval = exiledby_choose(player, card, CARD_ID_JESTERS_SCEPTER, EXBY_CHOOSE, 0, "", 1);
					int* loc = (int*)rval;
					if( loc ){
						int *loc2 = exiledby_find(player, card, *loc, NULL, NULL);
						iid = *loc2 & ~0x80000000;
						*loc2 = -1;
					}
				}
				else{
					int leg = 0;
					int idx = 0;
					int* loc;
					while ((loc = exiledby_find_any(player, card, &leg, &idx)) != NULL){
							int iid2 = *loc & ~0x80000000;
							if( iid2 == get_original_internal_card_id(card_on_stack_controller, card_on_stack) ){
								*loc = -1;
								iid = iid2;
								break;
							}
							idx++;
					}
				}
				if( iid != -1 ){
					int pos = find_iid_in_rfg(instance->targets[1].player, iid);
					if( pos > -1 ){
						from_exile_to_graveyard(instance->targets[1].player, pos);
					}
					instance->targets[0].player = card_on_stack_controller;
					instance->targets[0].card = card_on_stack;
					instance->number_of_targets = 1;
					instance->info_slot = iid;
					tap_card(player, card);
				}
				else{
					spell_fizzled = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( get_id(card_on_stack_controller, card_on_stack) == cards_data[instance->info_slot].id ){
			real_counter_a_spell(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_jokulmorder(int player, int card, event_t event){
  /* Jokulmorder |4|U|U|U
   * Creature — Leviathan 12/12
   * Trample
   * Jokulmorder enters the battlefield tapped.
   * When Jokulmorder enters the battlefield, sacrifice it unless you sacrifice five lands.
   * Jokulmorder doesn't untap during your untap step.
   * Whenever you play an Island, you may untap Jokulmorder. */

	comes_into_play_tapped(player, card, event);

	does_not_untap(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int sac_me = 1;
		if( can_sacrifice(player, player, 5, TYPE_LAND, 0) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, "Select a land to sacrifice.");
			int sacced[5];
			int sc = 0;
			int i;
			for(i=0; i<5; i++){
				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_RETURN_CHOICE, &this_test);
				if( sac ){
					sacced[i] = BYTE3(sac);
					sc++;
				}
				else{
					break;
				}
			}
			for(i=0; i<sc; i++){
				state_untargettable(player, sacced[i], 1);
				if( sc == 5 ){
					kill_card(player, sacced[i], KILL_SACRIFICE);
				}
			}
			if( sc == 5 ){
				sac_me = 0;
			}
		}
		if( sac_me ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( whenever_a_player_plays_a_land(player, card, event, player, RESOLVE_TRIGGER_AI(player) ) &&
	    has_subtype(trigger_cause_controller, trigger_cause, get_hacked_subtype(player, card, SUBTYPE_ISLAND)) && reason_for_trigger_controller == player ){
		untap_card(player, card);
	}

	return 0;
}

static int can_pay_jotun_grunt_upkeep(int player, int card, int number_of_age_counters)
{
  int gy[2] = { count_graveyard(0) / 2, count_graveyard(1) / 2 };
  return gy[0] + gy[1] >= number_of_age_counters;
}

static int pay_jotun_grunt_upkeep(int player, int card, int number_of_age_counters)
{
  int gy[2] = { count_graveyard(0) / 2, count_graveyard(1) / 2 };

  card_instance_t* instance = get_card_instance(player, card);

  test_definition_t test;
  default_test_definition(&test, 0);

  while (number_of_age_counters > 0 && gy[0] > 0 && gy[1] > 0)
	if (select_target_from_either_grave(player, card, SFG_NOTARGET, AI_MIN_VALUE, AI_MAX_VALUE, &test, 0, 1) != -1)
	  {
		from_graveyard_to_deck(instance->targets[0].player, instance->targets[1].player, 2);
		new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_GRAVE_NOTARGET, TUTOR_BOTTOM_OF_DECK, 1,
						 instance->targets[0].player == player ? AI_MIN_VALUE : AI_MAX_VALUE, &test);
		--number_of_age_counters;
		--gy[instance->targets[0].player];
	  }

  if (number_of_age_counters > 0)
	{
	  int who = gy[0] > 0 ? 0 : 1;
	  for (number_of_age_counters *= 2; number_of_age_counters > 0; --number_of_age_counters)
		new_global_tutor(player, who, TUTOR_FROM_GRAVE_NOTARGET, TUTOR_BOTTOM_OF_DECK, 1, who == player ? AI_MIN_VALUE : AI_MAX_VALUE, &test);
	}

  return 1;
}

int card_jotun_grunt(int player, int card, event_t event)
{
  /* Jotun Grunt	|1|W
   * Creature - Giant Soldier 4/4
   * Cumulative upkeep-Put two cards from a single graveyard on the bottom of their owner's library. */
  cumulative_upkeep_general(player, card, event, can_pay_jotun_grunt_upkeep, pay_jotun_grunt_upkeep);
  return 0;
}

int card_jotun_owl_keeper(int player, int card, event_t event)
{
  /* Jotun Owl Keeper	|2|W
   * Creature - Giant 3/3
   * Cumulative upkeep |W or |U
   * When ~ dies, put a 1/1 |Swhite Bird creature token with flying onto the battlefield for each age counter on it. */

	cumulative_upkeep_hybrid(player, card, event, 1, COLOR_WHITE, COLOR_BLUE, 0);

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_BIRD, &token);
		token.pow = token.tou = 1;
		token.key_plus = KEYWORD_FLYING;
		token.color_forced = COLOR_TEST_WHITE;
		token.qty = count_counters(player, card, COUNTER_AGE);
		generate_token(&token);

	}

	return 0;
}

int card_juniper_order_ranger(int player, int card, event_t event)
{
  /* CARD_ID_JUNIPER_ORDER_RANGER	8392
   * Juniper Order Ranger	|3|G|W
   * Creature - Human Knight 2/4
   * Whenever another creature enters the battlefield under your control, put a +1/+1 counter on that creature and a +1/+1 counter on ~. */

  test_definition_t test;
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY)
  {
    new_default_test_definition(&test, 2, "");
    test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    add_counters(player, card, trigger_cause_controller, trigger_cause, COUNTER_P1_P1,
                             1);
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    return 0;
  }
  return 0;
}

static int flip_coins(int player, int card, int number_of_age_counters)
{
  for (; number_of_age_counters > 0; --number_of_age_counters)
	flip_a_coin(player, card);
  return 1;
}

int card_karplusan_minotaur(int player, int card, event_t event)
{
  /* Karplusan Minotaur	|2|R|R	0x200a4ee
	 * Creature - Minotaur Warrior 3/3
	 * Cumulative upkeep-Flip a coin.
	 * Whenever you win a coin flip, ~ deals 1 damage to any target.
	 * Whenever you lose a coin flip, ~ deals 1 damage to any target of an opponent's choice. */

  if (event == EVENT_COIN_FLIP_WON_OR_LOST && affected_card_controller == player)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if (affected_card == 0)
    {
      td.preferred_controller = player;
      td.who_chooses = 1-player;
    }
    td.allow_cancel = 0;
    card_instance_t* instance = get_card_instance(player, card);
    if (can_target(&td) && pick_target(&td, "TARGET_ANY"))
    {
      instance->number_of_targets = 1;
      damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
    }
  }
  cumulative_upkeep_general(player, card, event, rtrue, flip_coins);
  return 0;
}

int card_karplusan_wolverine(int player, int card, event_t event){
	/* Karplusan Wolverine	|R	0x200a4f3
	 * Snow Creature - Wolverine Beast 1/1
	 * Whenever ~ becomes blocked, you may have it deal 1 damage to any target. */
	if( event == EVENT_DECLARE_BLOCKERS && ! is_humiliated(player, card, AL_TRIG) && is_attacking(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		card_instance_t *instance = get_card_instance(player, card);

		instance->number_of_targets = 0;

		int good = 0;
		if( current_turn == player ){
			if( is_attacking(player, card) ){
				int count = active_cards_count[1-player]-1;
				while( count > -1 ){
						if( in_play(1-player, count) && is_what(1-player, count, TYPE_CREATURE) ){
							card_instance_t *this = get_card_instance(1-player, count);
							if( this->blocking == card ){
								good = 1;
								break;
							}
						}
						count--;
				}
			}
		}
		if( good == 1 ){
			if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
				damage_target0(player, card, 1);
			}
		}
	}

	return 0;
}

int card_kjeldoran_javelineer(int player, int card, event_t event){
/* Kjeldoran Javelineer	|W
 * Creature - Human Soldier 1/2
 * Cumulative upkeep |1
 * |T: ~ deals damage equal to the number of age counters on it to target attacking or blocking creature. */

	cumulative_upkeep(player, card, event, MANACOST_X(1));

	if (!IS_GAA_EVENT(event)){
	  return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_IN_COMBAT;

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			damage_target0(player, card, count_counters(instance->parent_controller, instance->parent_card, COUNTER_AGE));
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target attacking or blocking creature.");
}

int card_kjeldoran_war_cry(int player, int card, event_t event){
/* CARD_ID_KJELDORAN_WAR_CRY	8399
Kjeldoran War Cry	|1|W
Instant
Creatures you control get +X/+X until end of turn, where X is 1 plus the number of cards named ~ in all graveyards. */

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = 1+ count_graveyard_by_id(ANYBODY, get_id(player, card));
		pump_subtype_until_eot(player, card, player, -1, amount, amount, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_krovikan_rot(int player, int card, event_t event){
/* CARD_ID_KROVIKAN_ROT	8401
Krovikan Rot	|2|B
Instant
Destroy target creature with power 2 or less.
Recover |1|B|B (When a creature is put into your graveyard from the battlefield, you may pay |1|B|B.
If you do, return this card from your graveyard to your hand. Otherwise, exile this card.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = 2 | TARGET_PT_LESSER_OR_EQUAL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature with power 2 or less.",
						1, NULL);
}

int card_krovikan_whispers(int player, int card, event_t event)
{
  /* Krovikan Whispers	|3|U
   * Enchantment - Aura
   * Enchant creature
   * Cumulative upkeep |U or |B
   * You control enchanted creature.
   * When ~ is put into a graveyard from the battlefield, you lose 2 life for each age counter on it. */

  cumulative_upkeep_hybrid(player, card, event, 1, COLOR_BLUE, COLOR_BLACK, 0);

  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	lose_life(player, 2 * count_counters(player, card, COUNTER_AGE));

  return card_control_magic(player, card, event);
}

int card_lightning_serpent(int player, int card, event_t event){

	/* Lightning Serpent	|X|R
	 * Creature - Elemental Serpent 2/1
	 * Trample, haste
	 * ~ enters the battlefield with X +1/+0 counters on it.
	 * At the beginning of the end step, sacrifice ~. */

	haste(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		set_x_for_x_spells(player, card, event, x_value);
	}

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P0, get_card_instance(player, card)->info_slot);

	if( eot_trigger(player, card, event) ){
		kill_card(player, card, KILL_SACRIFICE);
	}

	return 0;
}

int card_lovisa_coldeyes(int player, int card, event_t event)
{
  /* Lovisa Coldeyes |3|R|R
   * Legendary Creature — Human 3/3
   * Each creature that's a Barbarian, a Warrior, or a Berserker gets +2/+2 and has haste. */

	check_legend_rule(player, card, event);

	if( (event == EVENT_ABILITIES || event == EVENT_POWER || event == EVENT_TOUGHNESS) && in_play(player, card) &&
		! is_humiliated(player, card, AL_STATIC) )
	{
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			if( has_subtype(affected_card_controller, affected_card, SUBTYPE_BARBARIAN) ||
				has_subtype(affected_card_controller, affected_card, SUBTYPE_BERSERKER) ||
				has_subtype(affected_card_controller, affected_card, SUBTYPE_WARRIOR))
			{
				switch( event ){
						case EVENT_POWER:
						case EVENT_TOUGHNESS:
							event_result += 2;
							break;
						case EVENT_ABILITIES:
							haste(affected_card_controller, affected_card, event);
							break;
						default:
							break;

				}
			}
		}
	}

	return 0;
}

int card_magmatic_core(int player, int card, event_t event){
/* Magmatic Core	|2|R|R
 * Enchantment
 * Cumulative upkeep |1
 * At the beginning of your end step, ~ deals X damage divided as you choose among any number of target creatures,
 * where X is the number of age counters on it. */

	cumulative_upkeep(player, card, event, MANACOST_X(1));

	if( count_counters(player, card, COUNTER_AGE) > 0 && current_turn == player && eot_trigger(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		get_card_instance(player, card)->number_of_targets = 0;

		target_and_divide_damage(player, card, &td, "TARGET_CREATURE", count_counters(player, card, COUNTER_AGE));
	}

	return global_enchantment(player, card, event);
}

int card_martyr_of_the_ashes(int player, int card, event_t event){
/*
Martyr of Ashes |R
Creature — Human Shaman 1/1
{2}, Reveal X red cards from your hand, Sacrifice Martyr of Ashes: Martyr of Ashes deals X damage to each creature without flying.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_RED));
	this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if(	charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			instance->info_slot = reveal_cards_from_your_hand(player, card, &this_test);
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		test_definition_t this_test2;
		default_test_definition(&this_test2, TYPE_CREATURE);
		this_test2.keyword = KEYWORD_FLYING;
		this_test2.keyword_flag = 1;
		new_damage_all(player, card, 2, instance->info_slot, 0, &this_test2);
	}

	return 0;
}

int card_martyr_of_bones(int player, int card, event_t event){
  /* Martyr of Bones |B
   * Creature — Human Wizard 1/1
   * {1}, Reveal X black cards from your hand, Sacrifice Martyr of Bones: Exile up to X target cards from a single graveyard. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_BLACK));
	this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability_with_target_in_grave(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL,
																TYPE_ANY, &this_test, GS_GRAVE_RECYCLER_BOTH_GRAVES);
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		if(	charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			int amount = reveal_cards_from_your_hand(player, card, &this_test);
			if( amount ){
				test_definition_t this_test2;
				new_default_test_definition(&this_test2, TYPE_ANY, "Select target card to exile.");

				int targettable_players[2] = {0};
				int i;
				for(i=0; i<ANYBODY; i++){
					if( get_grave(i)[0] != -1 && ! graveyard_has_shroud(i) ){
						targettable_players[i] = 1;
					}
				}
				instance->targets[0].player = 1-player;
				if( ! targettable_players[1-player] ){
					instance->targets[0].player = player;
				}
				int max = MIN(amount, count_graveyard(instance->targets[0].player));
				int ai_mode = instance->targets[0].player == 1-player ? AI_MAX_VALUE : AI_MIN_VALUE;
				select_multiple_cards_from_graveyard(player, instance->targets[0].player, -1, ai_mode, &this_test2, max,
													&instance->targets[0]);
				if( spell_fizzled == 1 ){
					return 0;
				}
				SET_BYTE0(instance->info_slot) = max;
				SET_BYTE1(instance->info_slot) = instance->targets[0].player;
			}
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		int max_targets = BYTE0(instance->info_slot);
		int t_player = BYTE1(instance->info_slot);
		int i;
		for(i=max_targets-1; i>1; i--){
			int selected = validate_target_from_grave_source(player, card, t_player, i);
			if( selected != -1 ){
				rfg_card_from_grave(instance->targets[0].player, selected);
			}
		}
	}

	return 0;
}

int card_martyr_of_frost(int player, int card, event_t event){
  /* Martyr of Frost |U
   * Creature � Human Wizard 1/1
   * {2}, Reveal X blue cards from your hand, Sacrifice Martyr of Frost: Counter target spell unless its controller pays {X}. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_BLUE));
	this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_SPELL_ON_STACK, MANACOST_X(2), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if(	charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			instance->info_slot = reveal_cards_from_your_hand(player, card, &this_test);
			generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_SPELL_ON_STACK, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, instance->info_slot);
	}

	return 0;
}

int card_martyr_of_sands(int player, int card, event_t event){
  /* Martyr of Sands |W
   * Creature — Human Cleric 1/1
   * {1}, Reveal X white cards from your hand, Sacrifice Martyr of Sands: You gain three times X life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_WHITE));
	this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if(	charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			instance->info_slot = reveal_cards_from_your_hand(player, card, &this_test);
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		gain_life(player, 3*instance->info_slot);
	}

	return 0;
}

int card_martyr_of_spores(int player, int card, event_t event){
  /* Martyr of Spores |G
   * Creature — Human Shaman 1/1
   * {1}, Reveal X green cards from your hand, Sacrifice Martyr of Spores: Target creature gets +X/+X until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_GREEN));
	this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
	this_test.zone = TARGET_ZONE_HAND;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_CREATURE") ){
			if( spell_fizzled != 1 ){
				instance->info_slot = reveal_cards_from_your_hand(player, card, &this_test);
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, instance->info_slot, instance->info_slot);
		}
	}

	return 0;
}

/* ManalinkEh 2022: 0x21674a0; original cards/coldsnap.c:2325. */
int card_mishras_bauble(int player, int card, event_t event)
{
  /* Mishra's Bauble	|0
   * Artifact
   * |T, Sacrifice this artifact: Look at the top card of target player's library. Draw a card at the beginning of the next turn's upkeep. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {

    instance = get_card_instance(player, card);
    if (*deck_ptr[instance->targets[0].player] != -1)
    {
      show_deck(player, deck_ptr[instance->targets[0].player], 1,
                "Here\'s the first card of deck.", 0, gs_done_008b40e0);
    }
    cantrip(player, card, 1);
  }
  result = generic_activated_ability(player, card, event, 0xc, 0, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_PLAYER");
  return result;
}

int card_mouth_of_ronom(int player, int card, event_t event){
/* Mouth of Ronom	""
 * Snow Land
 * |T: Add |1 to your mana pool.
 * |4|I, |T, Sacrifice ~: ~ deals 4 damage to target creature. */

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_ACTIVATE ){
		add_state(player, card, STATE_TAPPED);
		int choice = 0;
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_X(4));
		if( !paying_mana() && can_use_activated_abilities(player, card) && can_target(&td) && has_snow_mana(player, 1, c1) ){
			choice = do_dialog(player, player, card, -1, -1, " Generate 1\n Sac, Deal 4\n Cancel", 1);
		}
		instance->info_slot = choice;
		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		if( choice == 1 ){
			if( ! charge_snow_mana(player, card, 1, c1) ){
				spell_fizzled = 1;
				remove_state(player, card, STATE_TAPPED);
				return 0;
			}
			else{
				if( pick_target(&td, "TARGET_CREATURE") ){
					tap_card(player, card);
					kill_card(player, card, KILL_SACRIFICE);
					tapped_for_mana_color = -1;
				}
				else{
					spell_fizzled = 1;
					remove_state(player, card, STATE_TAPPED);
					return 0;
				}
			}
		}
		if( choice == 2 ){
			spell_fizzled = 1;
			remove_state(player, card, STATE_TAPPED);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			if( valid_target(&td) ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
			}
		}
	}

	return 0;
}

int card_ohran_viper(int player, int card, event_t event){
  /* Ohran Viper	|1|G|G
   * Snow Creature - Snake 1/3
   * Whenever ~ deals combat damage to a creature, destroy that creature at end of combat.
   * Whenever ~ deals combat damage to a player, you may draw a card. */

	// original code : 012053FC



	if( damage_dealt_by_me(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_TRIGGER_OPTIONAL) ){
		draw_a_card(player);
	}

	if( ! is_humiliated(player, card, AL_TRIG) ){
		if (event == EVENT_CHANGE_TYPE && affect_me(player, card) ){
			card_instance_t* instance = get_card_instance(player, card);
			instance->destroys_if_blocked |= DIFB_DESTROYS_UNPROTECTED;
		}

		for_each_creature_damaged_by_me(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE, &lowland_basilisk_effect, player, card);
	}

	return 0;
}

int card_perilous_researches(int player, int card, event_t event){
/* CARD_ID_PERILOUS_RESEARCH	8420
Perilous Research	|1|U
Instant
Draw two cards, then sacrifice a permanent. */

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);

		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		new_sacrifice(player, card, player, SAC_NO_CANCEL, &test);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_phobian_phantasm(int player, int card, event_t event){
/* CARD_ID_PHOBIAN_PHANTASM	8421
Phobian Phantasm	|1|B|B
Creature - Illusion 3/3
Flying; fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
Cumulative upkeep |B (At the beginning of your upkeep, put an age counter on this permanent, then sacrifice it unless you pay its upkeep cost for each age counter on it.)*/

	cumulative_upkeep(player, card, event, MANACOST_B(1));

	fear(player, card, event);

	return 0;
}

int card_phyrexian_etchings(int player, int card, event_t event){
/* Phyrexian Etchings	|B|B|B
 * Enchantment
 * Cumulative upkeep |B
 * At the beginning of your end step, draw a card for each age counter on ~.
 * When ~ is put into a graveyard from the battlefield, you lose 2 life for each age counter on it. */

	cumulative_upkeep(player, card, event, MANACOST_B(1));

	if (current_turn == player && eot_trigger(player, card, event))
		draw_cards(player, count_counters(player, card, COUNTER_AGE));

	if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
		lose_life(player, 2 * count_counters(player, card, COUNTER_AGE));

	return global_enchantment(player, card, event);
}

int card_phyrexian_ironfoot(int player, int card, event_t event){

	/* Phyrexian Ironfoot	|3
	 * Snow Artifact Creature - Construct 3/4
	 * ~ doesn't untap during your untap step.
	 * |1|I: Untap ~. */

	does_not_untap(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_X(1));
		return can_use_activated_abilities(player, card) && has_snow_mana(player, 1, c1);
	}

	if( event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_X(1));
		charge_snow_mana(player, card, 1, c1);
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t *instance = get_card_instance(player, card);
		untap_card(instance->parent_controller, instance->parent_card);
	}

	return 0;
}

static int can_sacrifice_creatures(int player, int card, int number_of_age_counters)
{
	test_definition_t this_test;
	default_test_definition(&this_test, TYPE_CREATURE);
	this_test.qty = number_of_age_counters;

	return new_can_sacrifice(player, card, player, &this_test);
}

static int sacrifice_creatures(int player, int card, int number_of_age_counters)
{
  return impose_sacrifice(player, card, player, number_of_age_counters, TYPE_CREATURE,MATCH, 0,0, 0,0, 0,0, -1,0) == number_of_age_counters;
}

int card_phyrexian_soulgorger(int player, int card, event_t event)
{
  /* Phyrexian Soulgorger	|3
   * Snow Artifact Creature - Construct 8/8
   * Cumulative upkeep-Sacrifice a creature. */
  cumulative_upkeep_general(player, card, event, can_sacrifice_creatures, sacrifice_creatures);
  return 0;
}

int card_resize(int player, int card, event_t event){
  /* Resize	|1|G
   * Instant
   * Target creature gets +3/+3 until end of turn.
   * Recover |1|G */

//UNUSEDCARD

	return vanilla_instant_pump(player, card, event, ANYBODY, player, 3, 3, 0, 0);
}

static int effect_rt(int player, int card, event_t event){

	if(event == EVENT_BLOCK_LEGALITY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player == attacking_card_controller && instance->damage_target_card == attacking_card ){
			if( ! has_subtype(affected_card_controller, affected_card, SUBTYPE_SNOW) ){
				event_result = 1;
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_rime_transfusion(int player, int card, event_t event){
/* Rime Transfusion	|1|B
 * Snow Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+1 and has "|I: This creature can't be blocked this turn except by snow creatures." */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		if( is_humiliated(player, card, AL_STATIC) ){
			return 0;
		}

		if( event == EVENT_POWER && affect_me(t_player, t_card) ){
			event_result += 2;
		}

		if( event == EVENT_TOUGHNESS && affect_me(t_player, t_card) ){
			event_result += 1;
		}

		if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(t_player, t_card) ){
			int c1 = get_cost_mod_for_activated_abilities(t_player, t_card, MANACOST0);
			if( has_snow_mana(player, 1, c1) ){
				return 1;
			}
		}

		if( event == EVENT_ACTIVATE ){
			int c1 = get_cost_mod_for_activated_abilities(t_player, t_card, MANACOST0);
			charge_snow_mana(t_player, t_card, 1, c1);
		}

		if( event == EVENT_RESOLVE_ACTIVATION){
			create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &effect_rt, t_player, t_card);
		}

	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, player);
}

int card_rimebound_dead(int player, int card, event_t event){

	/* Rimebound Dead	|B
	 * Snow Creature - Skeleton 1/1
	 * |I: Regenerate ~. */

	if( land_can_be_played & LCBP_REGENERATION ){
		if( event == EVENT_CAN_ACTIVATE ){
			int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST0);
			if (can_use_activated_abilities(player, card) && has_snow_mana(player, 1, c1)){
				return can_regenerate(player, card);
			}
		}
		if( event == EVENT_ACTIVATE ){
			int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST0);
			charge_snow_mana(player, card, 1, c1);
		}
		if( event == EVENT_RESOLVE_ACTIVATION ){
			return regeneration(player, card, event, MANACOST_X(1));
		}
	}

	return 0;
}

int card_rimefeather_owl(int player, int card, event_t event)
{
  /* Rimefeather Owl	|5|U|U
   * Snow Creature - Bird 100/100
   * Flying
   * ~'s power and toughness are each equal to the number of snow permanents on the battlefield.
   * |1|I: Put an ice counter on target permanent.
   * Permanents with ice counters on them are snow. */

  card_instance_t *instance;
  int result;
  target_definition_t td1;

  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (result = is_humiliated(player, card, AL_STATIC), result == 0))
    {
      result = count_subtype(2, 0x47, 0x2003);
      set_characteristic_pt(result);
    }
  }
  else if (event == EVENT_CHANGE_TYPE)
  {
    instance = in_play(player, card);
    if (((instance != NULL) &&
         (result = is_what(affected_card_controller, affected_card, 0x47), result != 0)) &&
        (result = count_counters(affected_card_controller, affected_card, COUNTER_ICE),
         result != 0))
    {
      add_a_subtype(affected_card_controller, affected_card, SUBTYPE_SNOW);
    }
  }
  else if (((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td1, 0x47);
    td1.allow_cancel = 0;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = can_use_activated_abilities(player, card);
      if (result != 0)
      {
        result = get_cost_mod_for_activated_abilities(player, card, 1, 0, 0, 0, 0, 0);
        result = has_snow_mana(player, 1, result);
        if (result != 0)
        {
          result = can_target(&td1);
          return result;
        }
      }
    }
    else if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      result = get_cost_mod_for_activated_abilities(player, card, 1, 0, 0, 0, 0, 0);
      result = charge_snow_mana(player, card, 1, result);
      if ((result != 0) && (result = pick_target(&td1, "TARGET_PERMANENT"), result != 0))
      {
        instance->number_of_targets = '\x01';
      }
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td1), result != 0))
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                               COUNTER_ICE, 1);
      return 0;
    }
  }
  return 0;
}

int card_rimescale_dragon(int player, int card, event_t event)
{
  /* Rimescale Dragon	|5|R|R
   * Snow Creature - Dragon 5/5
   * Flying
   * |2|I: Tap target creature and put an ice counter on it.
   * Creatures with ice counters on them don't untap during their controllers' untap steps. */

  card_instance_t *instance;
  int result;
  target_definition_t td1;

  if ((current_phase == 1) && (event == EVENT_UNTAP))
  {
    result = count_counters(affected_card_controller, affected_card, COUNTER_ICE);
    if ((result != 0) &&
        (result = is_what(affected_card_controller, affected_card, 2), result != 0))
    {
      does_not_untap(affected_card_controller, affected_card, EVENT_UNTAP);
    }
  }
  else if (((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td1, 2);
    td1.allow_cancel = 0;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = can_use_activated_abilities(player, card);
      if (result != 0)
      {
        result = get_cost_mod_for_activated_abilities(player, card, 2, 0, 0, 0, 0, 0);
        result = has_snow_mana(player, 1, result);
        if (result != 0)
        {
          result = can_target(&td1);
          return result;
        }
      }
    }
    else if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      result = get_cost_mod_for_activated_abilities(player, card, 2, 0, 0, 0, 0, 0);
      result = charge_snow_mana(player, card, 1, result);
      if (result != 0)
      {
        pick_target(&td1, "TARGET_CREATURE");
      }
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td1), result != 0))
    {
      tap_card(instance->targets[0].player, instance->targets[0].card);
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                               COUNTER_ICE, 1);
      return 0;
    }
  }
  return 0;
}

int card_rimewind_taskmage(int player, int card, event_t event)
{
/* CARD_ID_RIMEWIND_TASKMAGE	8433
Rimewind Taskmage	|1|U
Creature - Human Wizard 1/2
|1, |T: You may tap or untap target permanent. Activate this ability only if you control four or more snow permanents. */

 	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	if (event == EVENT_CAN_ACTIVATE && count_subtype(player, TYPE_PERMANENT, SUBTYPE_SNOW) < 4)
		return 0;

	int rval = generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_PERMANENT");

	if (event == EVENT_ACTIVATE && player == AI && cancel != 1)
		ai_modifier_twiddle(player, card, 0);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
		twiddle(player, card, 0);

	return rval;
}

int card_rite_of_flame(int player, int card, event_t event){
/* CARD_ID_RITE_OF_FLAME	8434
Rite of Flame	|R
Sorcery
Add |R|R to your mana pool, then add |R to your mana pool for each card named ~ in each graveyard. */

	if(event == EVENT_RESOLVE_SPELL ){
		int count = count_graveyard_by_id(HUMAN, CARD_ID_RITE_OF_FLAME);
		count += count_graveyard_by_id(AI, CARD_ID_RITE_OF_FLAME);
		produce_mana(player, COLOR_RED, 2 + count );
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x2168a60; original cards/coldsnap.c:2781. */
int card_ronom_hulk(int player, int card, event_t event)
{
  /* Ronom Hulk	|4|G
   * Creature - Beast 5/6
   * Protection from snow
   * Cumulative upkeep |1 (At the beginning of your upkeep, put an age counter on this permanent, then sacrifice it unless you pay its upkeep cost for each age counter on it.) */

  protection_from_subtype(player, card, event, SUBTYPE_SNOW);
  cumulative_upkeep(player, card, event, 1, 0, 0, 0, 0, 0);
  return 0;
}

int card_ronom_unicorn(int player, int card, event_t event){
/* CARD_ID_RONOM_UNICORN	8437
Ronom Unicorn	|1|W
Creature - Unicorn 2/2
Sacrifice ~: Destroy target enchantment. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td1) ){
			kill_card(instance->targets[0].player,instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td1, "TARGET_ENCHANTMENT");
}

int card_rune_snag(int player, int card, event_t event){
	/* Rune Snag	|1|U
	 * Instant
	 * Counter target spell unless its controller pays |2 plus an additional |2 for each card named ~ in each graveyard. */

	if( event != EVENT_RESOLVE_SPELL ){
		return counterspell(player, card, event, NULL, 0);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = 1 + count_graveyard_by_id(player, CARD_ID_RUNE_SNAG);
		amount += count_graveyard_by_id(1-player, CARD_ID_RUNE_SNAG);
		amount *= 2;

		counterspell_resolve_unless_pay_x(player, card, NULL, 0, amount);

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_scrying_sheets(int player, int card, event_t event)
{
  /* CARD_ID_SCRYING_SHEETS	8439
   * Scrying Sheets	""
   * Snow Land
   * |T: Add |C to your mana pool.
   * |1|I, |T: Look at the top card of your library. If that card is snow, you may reveal it and put it into your hand.
   * (|I can be paid with one mana from a snow permanent.) */

  int *deck;
  int result;
  card_instance_t *instance;
  test_definition_t this_test;

  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
      result = can_use_activated_abilities(player, card);
      if (result != 0)
      {
        result = get_cost_mod_for_activated_abilities(player, card, 1, 0, 0, 0, 0, 0);
        result = has_snow_mana(player, 1, result);
        if ((result != 0) && (result = do_dialog(player, player, card, -1, -1,
                                                 " Generate 1\n Snow Scry\n Cancel", 1),
                              result != 0))
        {
          if (result != 1)
          {
            spell_fizzled = 1;
            return 0;
          }
          tap_card(player, card);
          result = get_cost_mod_for_activated_abilities(player, card, 1, 0, 0, 0, 0, 0);
          result = charge_snow_mana(player, card, 1, result);
          if (result != 0)
          {
            instance->info_slot = 1;
            return 0;
          }
          spell_fizzled = 1;
          untap_card_no_event(player, card);
          return 0;
        }
      }
      result = mana_producer(player, card, EVENT_ACTIVATE);
      return result;
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      instance = get_card_instance(player, card);
      if ((event == EVENT_RESOLVE_ACTIVATION) && (instance->info_slot == 1))
      {
        deck = deck_ptr[player];
        show_deck(player, deck, 1, "This is the first card of your library", 0, gs_done_008b40e0);
        result = has_subtype_by_id(player, (unsigned int)cards_data[*deck].id, SUBTYPE_SNOW);
        if ((result != 0) && (*deck != -1))
        {
          new_default_test_definition(&this_test, 1, "Select a snow land card to reveal.");
          this_test.no_shuffle = 1;
          this_test.create_minideck = 1;
          new_global_tutor(player, player, 1, 1, 0, 5, &this_test);
          return 0;
        }
      }
      return 0;
    }
  }
  result = mana_producer(player, card, event);
  return result;
}

static int sek_kuar_deathkeeper_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_SEKKUAR_DEATHKEEPER)
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
        if (!is_token(affected_card_controller, affected_card))
        {
          count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
        }
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);

    token_generation_t token;
    default_token_definition(player, card, CARD_ID_GRAVEBORN, &token);
    token.pow = 3;
    token.tou = 1;
    token.qty = amount;
    generate_token(&token);

    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_sek_kuar_death_keeper(int player, int card, event_t event){
  /* Sek'Kuar, Deathkeeper |2|B|R|G
   * Legendary Creature — Orc Shaman 4/3, 2BRG (5)
   * Whenever another nontoken creature you control dies, put a 3/1 black and red Graveborn creature token with haste onto the battlefield. */

	check_legend_rule(player, card, event);

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)sek_kuar_deathkeeper_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &sek_kuar_deathkeeper_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY && ! affect_me(player, card) && ! is_token(affected_card_controller, affected_card) ){
		count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);

			token_generation_t token;
			default_token_definition(player, card, CARD_ID_GRAVEBORN, &token);
			token.pow = 3;
			token.tou = 1;
			token.qty = amount;
			generate_token(&token);
		}
	}

	return 0;
}

int card_shape_of_the_wiitigo(int player, int card, event_t event)
{
/* CARD_ID_SHAPE_OF_THE_WIITIGO	8441
Shape of the Wiitigo	|3|G|G|G
Enchantment - Aura
Enchant creature
When ~ enters the battlefield, put six +1/+1 counters on enchanted creature.
At the beginning of your upkeep, put a +1/+1 counter on enchanted creature if it attacked or blocked since your last upkeep. Otherwise, remove a +1/+1 counter from it.*/

	card_instance_t* instance = in_play(player, card);
	if ((event == EVENT_DECLARE_ATTACKERS || event == EVENT_ETB_ABILITY || event == EVENT_UPKEEP_TRIGGER_ABILITY)
	    && instance && instance->damage_target_card >= 0 && !is_humiliated(player, card, AL_TRIG)){
	    if (event == EVENT_ETB_ABILITY){
	        if (in_play(instance->damage_target_player, instance->damage_target_card)){ add_counters(player, card, instance->damage_target_player, instance->damage_target_card, COUNTER_P1_P1, 6); }
	        upkeep_trigger_ability(player, card, event, player);
	    } else {
	        upkeep_trigger_ability(player, card, event, player);
	        if (event == EVENT_UPKEEP_TRIGGER_ABILITY){
	            if (in_play(instance->damage_target_player, instance->damage_target_card)){
	                if (instance->targets[1].player == 66){
	                    add_counters(player, card, instance->damage_target_player, instance->damage_target_card, COUNTER_P1_P1, 1);
	                    instance->targets[1].player = 0;
	                } else {
	                    remove_counters(player, card, instance->damage_target_player, instance->damage_target_card, COUNTER_P1_P1, 1);
	                }
	            }
	        } else {
	            card_instance_t* aff = in_play(instance->damage_target_player, instance->damage_target_card);
	            if (aff && (aff->state & STATE_ATTACKING)){ instance->targets[1].player = 66; }
	        }
	    }
	}
	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }
	return vanilla_aura(player, card, event, player);
}

/* ManalinkEh 2022: 0x2160760; original cards/coldsnap.c:3061. */
static char *can_have_counters_placed_on_it_shim(int who_chooses, int player, int card,
                                                 int targeting_player, int targeting_card)
{
  int result;
  char *rejection;

  result = can_have_counters_placed_on_it(player, card);
  rejection = NULL;
  if (result == 0)
  {
    rejection = "can\'t have counters";
  }
  return rejection;
}

static int can_put_p1_p1_counter_on_opp(int player, int card, int number_of_age_counters)
{
  if (check_battlefield_for_id(player, CARD_ID_BLIGHTBEETLE))
    return 0;
  target_definition_t td;
  base_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = td.preferred_controller = 1 - player;
  td.extra = (int32_t)can_have_counters_placed_on_it_shim;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  return can_target(&td);
}

static int put_p1_p1_counter_on_opp(int player, int card, int number_of_age_counters)
{
  card_instance_t *instance;
  int i;
  target_definition_t td;

  base_target_definition(player, card, &td, 2);
  td.extra = (int32_t)can_have_counters_placed_on_it_shim;
  td.allowed_controller = 1 - player;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.allow_cancel = 0;
  td.preferred_controller = td.allowed_controller;
  instance = get_card_instance(player, card);
  instance->number_of_targets = '\0';
  if (0 < number_of_age_counters)
  {
    i = 0;
    do
    {
      if (can_target(&td) == 0)
      {
        return 0;
      }
      if (pick_next_target_noload(&td, "Select a creature an opponent controls.") == 0)
      {
        return 0;
      }
      i = i + 1;
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                               COUNTER_P1_P1, 1);
      instance->number_of_targets = '\0';
    } while (number_of_age_counters != i);
  }
  return 1;
}

int card_sheltering_ancient(int player, int card, event_t event)
{
  /* Sheltering Ancient	|1|G
   * Creature - Treefolk 5/5
   * Trample
   * Cumulative upkeep-Put a +1/+1 counter on a creature an opponent controls. */
  cumulative_upkeep_general(player, card, event, can_put_p1_p1_counter_on_opp, put_p1_p1_counter_on_opp);
  return 0;
}

int card_skred(int player, int card, event_t event)
{
  /* Skred	|R
   * Instant
   * ~ deals damage to target creature equal to the number of snow permanents you control. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		damage_target0(player, card, count_snow_permanents(player, TYPE_PERMANENT, 0));

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_soul_spike(int player, int card, event_t event){
	/* Soul Spike	|5|B|B	0x200a539
	 * Instant
	 * You may exile two |Sblack cards from your hand rather than pay this spell's mana cost.
	 * ~ deals 4 damage to any target and you gain 4 life. */
	if( event != EVENT_MODIFY_COST && ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select a black card to exile");
	this_test.color = COLOR_TEST_BLACK;
	this_test.zone = TARGET_ZONE_HAND;
	this_test.id = cards_data[iid_draw_a_card].id;
	this_test.id_flag = 1;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td1.allow_cancel = 0;

	cost_mod_for_coldsnap_pitchspell(player, card, event, &this_test);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td1, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! casting_coldsnap_pitchspell(player, card, event, &this_test) ){
			spell_fizzled = 1;
			return 0;
		}
		pick_target(&td1, "TARGET_ANY");
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
			gain_life(player, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_sound_the_call(int player, int card, event_t event){
/* CARD_ID_SOUND_THE_CALL	8446
Sound the Call	|2|G
Sorcery
Create a 1/1 |Sgreen Wolf creature token. It has "This creature gets +1/+1 for each card named ~ in each graveyard." */

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_WOLF, &token);
		token.color_forced = COLOR_TEST_GREEN;
		token.pow = 1;
		token.tou = 1;
		token.special_infos = 66;
		generate_token(&token);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_squall_drifter(int player, int card, event_t event){
/* CARD_ID_SQUALL_DRIFTER	8447
Squall Drifter	|1|W
Snow Creature - Elemental 1/1
Flying
|W, |T: Tap target creature. */

	// Used externally
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(1), 0, &td, "TARGET_CREATURE");
}

int card_stalking_yeti(int player, int card, event_t event){
/* CARD_ID_STALKING_YETI	8448
Stalking Yeti	|2|R|R
Snow Creature - Yeti 3/3
When ~ enters the battlefield, if it's on the battlefield, it deals damage equal to its power to target creature an opponent controls
and that creature deals damage equal to its power to ~.
|2|I: Return ~ to its owner's hand. Activate this ability only any time you could cast a sorcery. (|I can be paid with one mana from a snow permanent.)*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.preferred_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if (can_target(&td) && pick_target(&td, "TARGET_CREATURE")){
			int my_power = get_power(player, card);
			int his_power = get_power(instance->targets[0].player, instance->targets[0].card);
			damage_creature(instance->targets[0].player, instance->targets[0].card, my_power, player, card);
			damage_creature(player, card, his_power, instance->targets[0].player, instance->targets[0].card);
		}
	}

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) && can_sorcery_be_played(player, event) ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_X(2));
		if( has_snow_mana(player, 1, c1) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_X(2));
		charge_snow_mana(player, card, 1, c1);
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t *instance = get_card_instance(player, card);
		bounce_permanent(instance->parent_controller, instance->parent_card);
	}

	return 0;
}

int card_stromgald_crusader(int player, int card, event_t event){
/* CARD_ID_STROMGALD_CRUSADER	1701
Stromgald Crusader	|B|B
Creature - Zombie Knight 2/1
Protection from |Swhite
|B: ~ gains flying until end of turn.
|B|B: ~ gets +1/+0 until end of turn. */

	return coldsnap_crusaders(player, card, event, COLOR_BLACK);
}

int card_suns_bounty(int player, int card, event_t event){
/* CARD_ID_SUNS_BOUNTY	8450
Sun's Bounty	|1|W
Instant
You gain 4 life.
Recover |1|W (When a creature is put into your graveyard from the battlefield, you may pay |1|W.
If you do, return this card from your graveyard to your hand. Otherwise, exile this card.) */

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, 4);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_sunscour(int player, int card, event_t event){
/* CARD_ID_SUNSCOUR	8451
Sunscour	|5|W|W
Sorcery
You may exile two |Swhite cards from your hand rather than pay ~'s mana cost.
Destroy all creatures. */

	if( event != EVENT_MODIFY_COST && ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select a white card to exile");
	this_test.color = COLOR_TEST_WHITE;
	this_test.zone = TARGET_ZONE_HAND;
	this_test.id = CARD_ID_RULES_ENGINE;
	this_test.id_flag = 1;

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test2;
		default_test_definition(&this_test2, TYPE_CREATURE);
		new_manipulate_all(player, card, 2, &this_test2, KILL_DESTROY);
		kill_card(player, card, KILL_DESTROY);
	}

	return coldsnap_pitchspell(player, card, event, &this_test);
}

int card_surging_aether(int player, int card, event_t event){
/* CARD_ID_SURGING_AETHER	8452
Surging Aether	|3|U
Instant
Ripple 4 (When you cast this spell, you may reveal the top four cards of your library.
You may cast any revealed cards with the same name as this spell without paying their mana costs. Put the rest on the bottom of your library.)
Return target permanent to its owner's hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
		if( spell_fizzled != 1 ){
			ripple(player, card);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_surging_dementia(int player, int card, event_t event){
/* CARD_ID_SURGING_DEMENTIA	8453
Surging Dementia	|1|B
Sorcery
Ripple 4 (When you cast this spell, you may reveal the top four cards of your library.
You may cast any revealed cards with the same name as this spell without paying their mana costs. Put the rest on the bottom of your library.)
Target player discards a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			ripple(player, card);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			discard(instance->targets[0].player, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_surging_flame(int player, int card, event_t event){
	/* Surging Flame	|1|R	0x200a55c
	 * Instant
	 * Ripple 4 (When you cast this spell, you may reveal the top four cards of your library. You may cast any revealed cards with the same name as this spell without paying their mana costs. Put the rest on the bottom of your library.)
	 * ~ deals 2 damage to any target. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 ){
			ripple(player, card);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_surging_might(int player, int card, event_t event){
/* CARD_ID_SURGING_MIGHT	8455
Surging Might	|2|G
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2.
Ripple 4 (When you cast this spell, you may reveal the top four cards of your library.
You may cast any revealed cards with the same name as this spell without paying their mana costs. Put the rest on the bottom of your library.)*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_aura(player, card, event, player, 2, 2, 0, 0, 0, 0, 0);
		if( spell_fizzled != 1 ){
			ripple(player, card);
		}
	}

	if( !(event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return generic_aura(player, card, event, player, 2, 2, 0, 0, 0, 0, 0);
	}

	return 0;
}

int card_surging_sentinels(int player, int card, event_t event){
/* CARD_ID_SURGING_SENTINELS	8456
Surging Sentinels	|2|W
Creature - Human Soldier 2/1
First strike
Ripple 4 (When you cast this spell, you may reveal the top four cards of your library.
You may cast any revealed cards with the same name as this spell without paying their mana costs. Put the rest on the bottom of your library.)*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && spell_fizzled != 1){
		ripple(player, card);
	}

	return 0;
}

int card_tamanoa(int player, int card, event_t event){
  /* Tamanoa |W|R|G
   * Creature — Spirit 2/4
   * Whenever a noncreature source you control deals damage, you gain that much life. */

	if( event == EVENT_DEAL_DAMAGE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.type_flag = DOESNT_MATCH;

		check_damage_test(player, -1, event, DDBM_REPORT_DAMAGE_DEALT|DDBM_MAY_DAMAGE_PLANESWALKER, player, card, 0, &test);
	}

	if( resolve_damage_trigger(player, card, event, DDBM_REPORT_DAMAGE_DEALT|DDBM_MAY_DAMAGE_PLANESWALKER, player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		gain_life(player, instance->targets[16].player);
		instance->targets[16].player = 0;
	}

	return 0;

}

int card_thermopod(int player, int card, event_t event){
/* CARD_ID_THERMOPOD	8461
Thermopod	|4|R
Snow Creature - Slug 4/3
|I: ~ gains haste until end of turn. (|I can be paid with one mana from a snow permanent.)
Sacrifice a creature: Add |R to your mana pool. */

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && can_use_activated_abilities(player, card) ){
		if( can_produce_mana(player, card) && can_sacrifice_type_as_cost(player, count_subtype(player, TYPE_CREATURE, -1), TYPE_CREATURE ) ){
			declare_mana_available(player, COLOR_RED, count_subtype(player, TYPE_CREATURE, -1));
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_HASTE
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_use_activated_abilities(player, card) && can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			return has_snow_mana(player, 1, 0);
		}
	}

	if(event == EVENT_ACTIVATE ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

		int choice = CHOICE_MANA;
		if( ! paying_mana() ){
			int ab1 = can_use_activated_abilities(player, card) && can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE) ? 1 : 0;
			int ab2 = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, 0, NULL, NULL) &&
						has_snow_mana(player, 1, 0) ? 1 : 0;
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 	ab1, 5,
							"Get Haste", 	ab2, current_phase < PHASE_DECLARE_BLOCKERS && is_sick(player, card) ? 10 : 1);

			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
				produce_mana(player, COLOR_RED, 1);
			}
			else{
				spell_fizzled = 1;
			}
		}
		if( choice == CHOICE_HASTE ){
			if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
				if( ! charge_snow_mana(player, card, 1, 0) ){
					spell_fizzled = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_HASTE ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									0, 0, 0, SP_KEYWORD_HASTE);
		}
	}

	return 0;
}

int card_thrummingstone(int player, int card, event_t event)
{
  /* CARD_ID_THRUMMING_STONE	8462
Thrumming Stone	|5
Legendary Artifact
Spells you cast have ripple 4. (Whenever you cast a spell, you may reveal the top four cards of your library.
You may cast any revealed cards with the same name as the spell without paying their mana costs. Put the rest on the bottom of your library.)*/

  check_legend_rule(player, card, event);

  sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
  if (event == EVENT_SPA)
    ripple(trigger_cause_controller, trigger_cause);
  return 0;
}

int card_vanish_into_memory(int player, int card, event_t event){
/* CARD_ID_VANISH_INTO_MEMORY	8466
Vanish into Memory	|2|W|U
Instant
Exile target creature. You draw cards equal to that creature's power. At the beginning of your next upkeep,
return that card to the battlefield under its owner's control. If you do, discard cards equal to that creature's toughness. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(player, get_power(instance->targets[0].player, instance->targets[0].card));
			remove_until_upkeep_discard_cards_equal_to_toughness(player, card, instance->targets[0].player, instance->targets[0].card, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

static int can_discard_n(int player, int card, int number_of_age_counters)
{
  return hand_count[player] >= number_of_age_counters;
}

static int multidiscard_thunk(int player, int card, int number_of_age_counters)
{
	if( player == HUMAN || (player == AI && number_of_age_counters < 3) ){
		multidiscard(player, number_of_age_counters, 0);
		return 1;
	}
	return 0;
}

int card_vexing_sphinx(int player, int card, event_t event)
{
  /* Vexing Sphinx	|1|U|U
   * Creature - Sphinx 4/4
   * Flying
   * Cumulative upkeep-Discard a card.
   * When ~ dies, draw a card for each age counter on it. */

	cumulative_upkeep_general(player, card, event, can_discard_n, multidiscard_thunk);

	if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
		draw_cards(player, count_counters(player, card, COUNTER_AGE));

	return 0;
}

static int opp_can_gain_life_as_cost(int player, int card, int number_of_age_counters)
{
  if (player_bits[1 - player] & PB_CANT_GAIN_LIFE)
    return 0;
  return life_total_can_change(1 - player) != 0;
}

static int opp_gains_life(int player, int card, int number_of_age_counters)
{
  gain_life(1-player, number_of_age_counters);
  return 1;
}

int card_wall_of_shards(int player, int card, event_t event)
{
  /* Wall of Shards	|1|W
   * Snow Creature - Wall 1/8
   * Defender, flying
   * Cumulative upkeep-An opponent gains 1 life. */
  cumulative_upkeep_general(player, card, event, opp_can_gain_life_as_cost, opp_gains_life);
  return 0;
}

int card_white_shield_crusader(int player, int card, event_t event){
/* CARD_ID_WHITE_SHIELD_CRUSADER	1700
White Shield Crusader	|W|W
Creature - Human Knight 2/1
Protection from |Sblack
|W: ~ gains flying until end of turn.
|W|W: ~ gets +1/+0 until end of turn. */

	return coldsnap_crusaders(player, card, event, COLOR_WHITE);
}

int card_void_maw(int player, int card, event_t event){

	/* Void Maw	|4|B|B
	 * Creature - Horror 4/5
	 * Trample
	 * If another creature would die, exile it instead.
	 * Put a card exiled with ~ into its owner's graveyard: ~ gets +2/+2 until end of turn. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( !affect_me(player, card) && in_play(affected_card_controller, affected_card) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			card_instance_t *affected = get_card_instance(affected_card_controller, affected_card);
			if( affected->kill_code > 0 && affected->kill_code < KILL_EXILE){
				int leg = 0, idx = 0;
				if (!is_token(affected_card_controller, affected_card)){
					exiledby_remember(player, card, get_owner(affected_card_controller, affected_card),
										get_original_internal_card_id(affected_card_controller, affected_card), &leg, &idx);
				}
				affected->kill_code = KILL_EXILE;
			}
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			return exiledby_choose(player, card, CARD_ID_VOID_MAW, EXBY_FIRST_FOUND, 0, "Creature", 1);
		}
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) ){
			int rval = exiledby_choose(player, card, CARD_ID_VOID_MAW, EXBY_CHOOSE, 0, "Creature", 1);
			int* loc = (int*)rval;
			if( loc ){
				int *loc2 = exiledby_find(player, card, *loc, NULL, NULL);
				int owner = *loc2 & 0x80000000 ? 1 : 0;
				int iid = *loc2 & ~0x80000000;
				int pos = find_iid_in_rfg(owner, iid);
				if( pos > -1 ){
					from_exile_to_graveyard(owner, pos);
				}
				*loc2 = -1;
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

int card_wilderness_elemental(int player, int card, event_t event){
	/* Wilderness Elemental	|1|R|G
	 * Creature - Elemental 100/3
	 * Trample
	 * ~'s power is equal to the number of nonbasic lands your opponents control. */

	if( event == EVENT_POWER && affect_me(player, card) && player != -1){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.subtype = SUBTYPE_BASIC;
		this_test.subtype_flag = DOESNT_MATCH;
		set_characteristic_pt(check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &this_test));
	}

	return 0;
}

int card_woolly_razorback(int player, int card, event_t event)
{
  /* Woolly Razorback	|2|W|W
   * Creature - Boar Beast 7/7
   * ~ enters the battlefield with three ice counters on it.
   * As long as ~ has an ice counter on it, prevent all combat damage it would deal and it has defender.
   * Whenever ~ blocks, remove an ice counter from it. */

  char damage_player;
  card_instance_t *instance;
  int result;

  enters_the_battlefield_with_counters(player, card, event, COUNTER_ICE, 3);
  if (event == EVENT_ABILITIES)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = count_counters(player, card, COUNTER_ICE), 0 < result)) &&
        (result = is_humiliated(player, card, AL_STATIC), result == 0))
    {
      event_result = event_result | 0x800000;
    }
  }
  instance = combat_damage_being_prevented(event);
  if (instance != NULL)
  {
    damage_player = instance->damage_source_player;
    if (((damage_player == player) && (instance->damage_source_card == card)) &&
        ((result = count_counters(player, card, COUNTER_ICE),
          0 < result && (result = is_humiliated(player, card, AL_STATIC), result == 0))))
    {
      prevent_damage(affected_card_controller, affected_card, instance->info_slot);
    }
  }
  if (((event == EVENT_DECLARE_BLOCKERS) &&
       (result = count_counters(player, card, COUNTER_ICE), 0 < result)) &&
      ((result = blocking(player, card, EVENT_DECLARE_BLOCKERS),
        result != 0 && (result = is_humiliated(player, card, AL_TRIG), result == 0))))
  {
    remove_counters(player, card, player, card, COUNTER_ICE, 1);
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x216b6c0; original cards/coldsnap.c:3799. */
int card_zombie_musher(int player, int card, event_t event)
{
  /* Zombie Musher	|3|B
   * Snow Creature - Zombie 2/3
   * Snow landwalk (This creature can't be blocked as long as defending player controls a snow land.)
   * |S: Regenerate this creature. (|S can be paid with one mana from a snow source.) */

  int mana_cost_or_result;
  int abilities_available;
  if (event == EVENT_BLOCK_LEGALITY)
  {
    mana_cost_or_result = is_humiliated(player, card, AL_STATIC);
    if ((((mana_cost_or_result == 0) && (attacking_card_controller == player)) &&
         (attacking_card == card)) &&
        (mana_cost_or_result = control_a_snow_land(1 - current_turn, 0), mana_cost_or_result != 0))
    {
      event_result = 1;
    }
  }
  else if ((((unsigned int)land_can_be_played >> 8 & 0xff) & 2) != 0)
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      mana_cost_or_result = get_cost_mod_for_activated_abilities(player, card, 0, 0, 0, 0, 0, 0);
      abilities_available = can_use_activated_abilities(player, card);
      if ((abilities_available != 0) &&
          (mana_cost_or_result = has_snow_mana(player, 1, mana_cost_or_result),
           mana_cost_or_result != 0))
      {
        return can_regenerate(player, card);
      }
    }
    else if (event == EVENT_ACTIVATE)
    {
      charge_snow_mana(player, card, 1,
                       get_cost_mod_for_activated_abilities(player, card, 0, 0, 0, 0, 0, 0));
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      return regeneration(player, card, EVENT_RESOLVE_ACTIVATION, 1, 0, 0, 0, 0, 0);
    }
  }
  return 0;
}
#endif

int card_zur_the_enchanter(int player, int card, event_t event){
  /* Zur the Enchanter |1|W|U|B
   * Legendary Creature — Human Wizard 1/4
   * Flying
   * Whenever Zur the Enchanter attacks, you may search your library for an enchantment card with converted mana cost 3 or less and
   * 	put it onto the battlefield. If you do, shuffle your library. */

	check_legend_rule(player, card, event);

	dat_ability(player, card, event, duh_mode(player) ? DAT_RESOLVE_TRIGGER_MANDATORY : DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
	if (event == EVENT_DAT_ABILITY){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ENCHANTMENT, "Select an enchantment with CMC 3 or less.");
		this_test.cmc = 4;
		this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
	}

	return 0;
}
