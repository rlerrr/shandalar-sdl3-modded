#include "manalink.h"

static int grave_pact_effect(int player, int card, event_t event);

// --- GLOBAL FUNCTIONS ---

int generic_spike(int player, int card, event_t event, int counters){

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, counters);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_1_1_COUNTER, MANACOST_X(2), 0, &td, "TARGET_CREATURE");
}

// --- CARDS

int acidic_sliver_shared_ability(int player, int card, event_t event)
{
  int card_00;
  card_instance_t *instance;
  card_instance_t *sliver_source;
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
  }
  else
  {
    sliver_source = in_play(result, card_00);
    if (sliver_source == NULL)
    {
      if (event == EVENT_CAN_ACTIVATE)
        return 0;
      if (event == EVENT_CLEANUP)
        kill_card(player, card, KILL_EXILE);
    }
  }
  result = is_humiliated(result, card_00, AL_NM_NL_ACT);
  if (result != 0)
    return 0;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td2, 0x102);
  td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  td2.allow_cancel = 0;
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = would_validate_arbitrary_target(&td2, instance->targets[2].player,
                                                instance->targets[2].card),
       result != 0))
  {
    damage_creature(instance->targets[2].player, instance->targets[2].card, 2, player, card);
  }
  return shared_sliver_activated_ability(player, card, event, 0xc, 2, 0, 0, 0, 0, 0, 0, &td2,
                                         "TARGET_ANY");
}

int card_acidic_sliver(int player, int card, event_t event){
	/* Acidic Sliver	|B|R	0x200372a
	 * Creature - Sliver 2/2
	 * All Slivers have "|2, Sacrifice this permanent: This permanent deals 2 damage to any target." */
	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &acidic_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &acidic_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int card_amok(int player, int card, event_t event)
{
  /* Amok	|1|R
   * Enchantment
   * |1, Discard a card at random: Put a +1/+1 counter on target creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x24, 1, 0, 0, 0, 0, 0,
                                         0, &td, NULL);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 4, 1, 0, 0, 0, 0, 0, 0, &td,
                                "TARGET_CREATURE");
      if (spell_fizzled == 1)
      {
        return 0;
      }
      discard(player, 1, player);
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      result = global_enchantment(player, card, event);
      return result;
    }
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                             COUNTER_P1_P1, 1);
    return 0;
  }
  return 0;
}

int card_awakening2(int player, int card, event_t event){
  /* Awakening	|2|G|G
   * Enchantment
   * At the beginning of each upkeep, untap all creatures and lands. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE | TYPE_LAND);
		new_manipulate_all(player, card, ANYBODY, &this_test, ACT_UNTAP);
	}

	return global_enchantment(player, card, event);
}

int card_bottomless_pit(int player, int card, event_t event){
  /* Bottomless Pit	|1|B|B
   * Enchantment
   * At the beginning of each player's upkeep, that player discards a card at random. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		discard(current_turn, DISC_RANDOM, player);
	}

	return global_enchantment(player, card, event);
}

int card_brush_with_death(int player, int card, event_t event){
  /* Brush with Death	|2|B
   * Sorcery
   * Buyback |2|B|B (You may pay an additional |2|B|B as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Target opponent loses 2 life. You gain 2 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
		if( spell_fizzled != 1 ){
			instance->info_slot = buyback(player, card, MANACOST_XB(2, 2));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 2);
			gain_life(player, 2);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_bullwhip(int player, int card, event_t event){
  /* Bullwhip	|4
   * Artifact
   * |2, |T: This artifact deals 1 damage to target creature. That creature attacks this turn if able. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 1, instance->parent_controller, instance->parent_card);
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
									0, 0, 0, SP_KEYWORD_MUST_ATTACK);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_CREATURE");
}

int card_burgeoning(int player, int card, event_t event){
  /* Burgeoning	|G
   * Enchantment
   * Whenever an opponent plays a land, you may put a land card from your hand onto the battlefield. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( whenever_a_player_plays_a_land(player, card, event, 1-player, RESOLVE_TRIGGER_AI(player)) ){
			if( hand_count[player] ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_LAND, "Select a land card to put into play.");
				new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_cannibalize(int player, int card, event_t event){
  /* Cannibalize	|1|B
   * Sorcery
   * Choose two target creatures controlled by the same player. Exile one of those creatures and put two +1/+1 counters on the other. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		int i;
		for(i=0; i<2; i++){
			if( player == HUMAN || (player == AI && i == 1-player) ){
				td.allowed_controller = i;
				td.preferred_controller = i;
				if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 2, NULL) ){
					return 1;
				}
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "Select target creature to exile.", 0, 1 | GS_LITERAL_PROMPT) ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			td.allowed_controller = instance->targets[0].player;
			td.preferred_controller = instance->targets[0].player;
			if( can_target(&td) ){
				new_pick_target(&td, "Select target creature for the +1/+1 counters.", 1, 1 | GS_LITERAL_PROMPT);
			}
			else{
				spell_fizzled = 1;
			}
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		td.allowed_controller = instance->targets[0].player;
		td.preferred_controller = instance->targets[0].player;
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		if( validate_target(player, card, &td, 1) ){
			add_counters(player, card, instance->targets[1].player, instance->targets[1].card, COUNTER_P1_P1, 2);
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return 0;
}

int card_change_of_heart(int player, int card, event_t event){
  /* Change of Heart	|W
   * Instant
   * Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Target creature can't attack this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( current_turn == player || current_phase > PHASE_DECLARE_ATTACKERS ){
			ai_modifier-=50;
		}
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			instance->info_slot = buyback(player, card, MANACOST_X(3));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if (valid_target(&td)){
			create_targetted_legacy_effect(player, card, effect_cannot_attack_until_eot,
											instance->targets[0].player, instance->targets[0].card);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_constant_mists(int player, int card, event_t event){
  /* Constant Mists	|1|G
   * Instant
   * Buyback-Sacrifice a land. (You may sacrifice a land in addition to any other costs as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Prevent all combat damage that would be dealt this turn. */

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) ){
			if( can_sacrifice_type_as_cost(player, 1, TYPE_LAND) ){
				int choice = do_dialog(player, player, card, -1, -1, " Pay Buyback\n Pass", count_subtype(player, TYPE_LAND, -1) < 4);
				if( choice == 0 ){
					test_definition_t this_test;
					new_default_test_definition(&this_test, TYPE_LAND, "Select a land to sacrifice");

					if( new_sacrifice(player, card, player, SAC_AS_COST, &this_test) ){
						get_card_instance(player, card)->info_slot = 66;
					}
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		fog_effect(player, card);
		if( get_card_instance(player, card)->info_slot == 66 ){
			bounce_permanent(player, card);
			return 0;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_contemplation(int player, int card, event_t event)
{
  /* Contemplation	|1|W|W
   * Enchantment
   * Whenever you cast a spell, you gain 1 life. */

  sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
  if (event == EVENT_SPA)
    gain_life(player, 1);
  return global_enchantment(player, card, event);
}

int card_crovax_the_cursed(int player, int card, event_t event)
{
  /* Crovax the Cursed	|2|B|B
   * Legendary Creature - Vampire Noble 0/0
   * Crovax enters with four +1/+1 counters on it.
   * At the beginning of your upkeep, you may sacrifice a creature. If you do, put a +1/+1 counter on Crovax. If you don't, remove a +1/+1 counter from Crovax.
   * |B: Crovax gains flying until end of turn. */

  check_legend_rule(player, card, event);

  // ~ enters the battlefield with four +1/+1 counters on it.
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 4);

  // At the beginning of your upkeep, you may sacrifice a creature. If you do, put a +1/+1 counter on ~. If you don't, remove a +1/+1 counter from ~.
  upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  int ai_priority_sac = 0;
	  if (player == AI || ai_is_speculating == 1)
		{
		  target_definition_t td;
		  default_target_definition(player, card, &td, TYPE_CREATURE);
		  td.allowed_controller = player;
		  td.preferred_controller = player;
		  td.illegal_abilities = 0;
		  td.special = TARGET_SPECIAL_NOT_ME;

		  ai_priority_sac = can_target(&td) ? 3 : -1;
		}

	  if (DIALOG(player, card, EVENT_ACTIVATE,
				 DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_RANDOM,
				 "Sacrifice for Crovax", 1, ai_priority_sac,
				 "Pass", 1, 1) == 1
		  && controller_sacrifices_a_permanent(player, card, TYPE_CREATURE, 0))
		{
		  if (in_play(player, card))	// might've sacced to himself, for some reason
			add_counters(player, card, player, card, COUNTER_P1_P1, 1);
		}
	  else
		remove_counters(player, card, player, card, COUNTER_P1_P1, 1);

	  get_card_instance(player, card)->number_of_targets = 0;
	}

  // |B: Crovax gains flying until end of turn.
  return generic_shade(player, card, event, 0, MANACOST_B(1), 0,0, KEYWORD_FLYING,0);
}

int card_crystalline_sliver(int player, int card, event_t event){
  /* Crystalline Sliver	|W|U
   * Creature - Sliver 2/2
   * All Slivers have shroud. (They can't be the targets of spells or abilities.) */

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 0, 0, KEYWORD_SHROUD, BCT_INCLUDE_SELF);
	return slivercycling(player, card, event);
}

static int dream_halls_effect(int player, int card, event_t event){

	if( event == EVENT_CAN_ACTIVATE && hand_count[player] > 1 ){
		int result = 0;
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) && get_color(player, i) != COLOR_TEST_COLORLESS ){
				int can_play_this_spell = can_legally_play_iid(player, get_card_instance(player, i)->internal_card_id);
				if( can_play_this_spell ){
					int clr = get_color(player, i);
					int k;
					for(k=0; k<active_cards_count[player]; k++){
						if( k != i && in_hand(player, k) ){
							if( get_color(player, k) & clr ){
								if( can_play_this_spell == 99 ){
									return 99;
								}
								else{
									if( is_what(player, i, TYPE_INSTANT) || can_sorcery_be_played(player, event) ){
										result = 1;
									}
									break;
								}
							}
						}
					}
				}
			}
		}
		return result;
	}

	if(event == EVENT_ACTIVATE ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a card to play with Dream Halls.");

		int pwdh[2][hand_count[player]];
		int pwdhc = 0;
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) && get_color(player, i) != COLOR_TEST_COLORLESS ){
				int can_play_this_spell = can_legally_play_iid(player, get_card_instance(player, i)->internal_card_id);
				if( can_play_this_spell && (is_what(player, i, TYPE_INSTANT | TYPE_INTERRUPT) || can_sorcery_be_played(player, event))){
					int clr = get_color(player, i);
					int k;
					for(k=0; k<active_cards_count[player]; k++){
						if( k != i && in_hand(player, k) ){
							if( get_color(player, k) & clr ){
								pwdh[0][pwdhc] = get_card_instance(player, i)->internal_card_id;
								pwdh[1][pwdhc] = i;
								pwdhc++;
								break;
							}
						}
					}
				}
			}
		}
		int selected = select_card_from_zone(player, player, pwdh[0], pwdhc, 0, AI_MAX_VALUE, -1, &test);
		if( selected != -1 ){
			int clr = get_color(player, pwdh[1][selected]);

			int ctsc[2][hand_count[player]];
			int ctscc = 0;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_hand(player, i) && i != pwdh[1][selected]){
					if( get_color(player, i) & clr ){
						ctsc[0][ctscc] = get_card_instance(player, i)->internal_card_id;
						ctsc[1][ctscc] = i;
						ctscc++;
					}
				}
			}
			if( ctscc == 0 ){
				spell_fizzled = 1;
				return 0;
			}
			strcpy(test.message, "Select a card which shares a color with the previous.");
			int sel2 = select_card_from_zone(player, player, ctsc[0], ctscc, 0, AI_MIN_VALUE, -1, &test);
			if( sel2 != -1 ){
				discard_card(player, ctsc[1][sel2]);
				play_card_in_hand_for_free(player, pwdh[1][selected]);
				cant_be_responded_to = 1;	// The spell will be respondable to, but this (fake) activation won't
			}
			else {
				spell_fizzled = 1;
			}
		}
	}

	return 0;
}

static int dream_halls_legacy(int player, int card, event_t event)
{

  if (get_card_instance(player, card)->targets[0].player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (is_humiliated(instance->targets[0].player, instance->targets[0].card, AL_STATIC))
    {
      return 0;
    }
    leaves_play(instance->targets[0].player, instance->targets[0].card, event, 0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      kill_card(player, card, KILL_EXILE);
    }
    return dream_halls_effect(player, card, event);
  }

  return 0;
}

int card_dream_halls(int player, int card, event_t event){
	/* Dream Halls	|3|U|U
	 * Enchantment
	 * Rather than pay the mana cost for a spell, its controller may discard a card that shares a color with that spell. */

	if( event == EVENT_RESOLVE_SPELL ){
		int fake = add_card_to_hand(1-player, get_original_internal_card_id(player, card));
		int legacy = create_legacy_activate(1-player, fake, &dream_halls_legacy);
		card_instance_t *instance = get_card_instance(1-player, legacy);
		instance->targets[0].player = player;
		instance->targets[0].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	if( IS_GAA_EVENT(event) && ! is_humiliated(player, card, AL_STATIC) ){
		return dream_halls_effect(player, card, event);
	}

	return global_enchantment(player, card, event);
}

int card_elven_rite(int player, int card, event_t event)
{
  /* Elven Rite	|1|G
   * Sorcery
   * Distribute two +1/+1 counters among one or two target creatures. */

  target_t *target_player_entry;
  target_t *target_card_entry;
  card_instance_t *instance;
  int i;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      instance = get_card_instance(player, card);
      if ((card == affected_card) &&
          ((player == affected_card_controller &&
            (generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL),
             spell_fizzled != 1))))
      {
        add_state(instance->targets[0].player, instance->targets[0].card, 0x200000);
        new_pick_target(&td, "TARGET_CREATURE", 1, 0);
        remove_state(instance->targets[0].player, instance->targets[0].card, 0x200000);
        if ((instance->number_of_targets == '\x02') &&
            ((instance->targets[0].card == instance->targets[1].card &&
              (instance->targets[0].player == instance->targets[1].player))))
        {
          instance->number_of_targets = '\x01';
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
      i = 0;
      if (instance->number_of_targets != '\0')
      {
        do
        {
          if (validate_target(player, card, &td, i) != 0)
          {
            target_card_entry = instance->targets + i;
            target_player_entry = instance->targets + i;
            i = i + 1;
            add_counters(player, card, target_player_entry->player, target_card_entry->card,
                         COUNTER_P1_P1,
                         (int)(2 / (unsigned long long)(long long)(int)(unsigned int)
                                       instance->number_of_targets));
          }
          else
            i = i + 1;
        } while (i < (int)(unsigned int)instance->number_of_targets);
      }
      kill_card(player, card, KILL_SACRIFICE);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
  }
  return 0;
}

int card_ensnaring_bridge(int player, int card, event_t event){
  /* Ensnaring Bridge |3
   * 	  Artifact
   * 	  Creatures with power greater than the number of cards in your hand can't attack. */

	if( event == EVENT_ATTACK_LEGALITY && ! is_humiliated(player, card, AL_STATIC) &&
		get_power(affected_card_controller, affected_card) > hand_count[player] )
	{
		event_result = 1;
	}

	return 0;
}

int card_evacuation(int player, int card, event_t event){
  /* Evacuation |3|U|U
   * 	  Instant
   * 	  Return all creatures to their owners' hands. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(player, card, ANYBODY, &this_test, ACT_BOUNCE);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_fanning_the_flames(int player, int card, event_t event){
	/* Fanning the Flames	|X|R|R	0x200a0ca
	 * Sorcery
	 * Buyback |3
	 * ~ deals X damage to any target. */
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
		generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 ){
			instance->targets[1].player = buyback(player, card, MANACOST_X(3));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
			if( instance->targets[1].player == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_flame_wave(int player, int card, event_t event){
	/* Flame Wave	|3|R|R|R|R	0x200a0cf
	 * Sorcery
	 * ~ deals 4 damage to target player or planeswalker and each creature that player or that planeswalker's controller controls. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
			new_damage_all(player, card, instance->targets[0].player, 4, NDA_ALL_CREATURES, NULL);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
}

int card_flowstone_mauler(int player, int card, event_t event){
  /* Flowstone Mauler	|4|R|R
   * Creature - Beast 4/5
   * Trample
   * |R: This creature gets +1/-1 until end of turn. */

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1, -1);
}

int card_flowstone_hellion(int player, int card, event_t event){
  /* Flowstone Hellion	|4|R
   * Creature - Hellion Beast 3/3
   * Haste
   * |0: This creature gets +1/-1 until end of turn. */

	haste(player, card, event);
	return generic_shade_merge_pt(player, card, event, 0, MANACOST0, 1, -1);
}

int card_foul_imp(int player, int card, event_t event){
  /* Foul Imp	|B|B
   * Creature - Imp 2/2
   * Flying
   * When this creature enters, you lose 2 life. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		lose_life(player, 2);
	}

	return 0;
}

int card_grave_pact(int player, int card, event_t event){
  /* Grave Pact	|1|B|B|B
   * Enchantment
   * Whenever a creature you control dies, each other player sacrifices a creature of their choice. */

	// Used externally
  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    int found = 0;
    for (int c = 0; c < active_cards_count[player]; ++c)
      if (in_play(player, c) && is_what(player, c, TYPE_EFFECT))
      {
        card_instance_t* legacy = get_card_instance(player, c);
        if (legacy->info_slot == (int32_t)grave_pact_effect &&
            ((legacy->damage_target_player == player && legacy->damage_target_card == card) ||
             (legacy->targets[0].player == player && legacy->targets[0].card == card)))
        {
          found = 1;
          break;
        }
      }
    if (!found)
    {
      int effect = create_targetted_legacy_effect(player, card, grave_pact_effect, player, card);
      card_instance_t* legacy = get_card_instance(player, effect);
      legacy->targets[0].player = player;
      legacy->targets[0].card = card;
      legacy->targets[1].card = get_id(player, card);
      legacy->number_of_targets = 1;
      add_status(player, effect, STATUS_INVISIBLE_FX);
    }
  }
	count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( result ){
		int amount = BYTE0(result);

		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		test.qty = amount;
		new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	return global_enchantment(player, card, event);
}

int card_hammerhead_shark(int player, int card, event_t event)
{
  /* Hammerhead Shark	|1|U
   * Creature - Shark 2/3
   * This creature can't attack unless defending player controls an Island. */

	if( event == EVENT_ATTACK_LEGALITY && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) &&
		!basiclandtypes_controlled[1 - player][get_hacked_color(player, card, COLOR_BLUE)] )
		event_result = 1;

	return 0;
}

int card_heartstone(int player, int card, event_t event)
{
  /* Heartstone	|3
   * Artifact
   * Activated abilities of creatures cost |1 less to activate. This effect can't reduce the mana in that cost to less than one mana. */

  int result;
  event_t event_00;
  test_definition_t this_test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&this_test, 2);
    set_cost_mod_for_activated_abilities(2, -1, 1, &this_test);
    event_00 = EVENT_RESOLVE_SPELL;
  }
  else
  {
    if (event != EVENT_CAST_SPELL)
    {
      leaves_play(player, card, event, 0, 0);
      if (event != EVENT_LEAVES_PLAY_ABILITY)
      {
        return 0;
      }
      default_test_definition(&this_test, 2);
      remove_cost_mod_for_activated_abilities(2, -1, 1, &this_test);
      return 0;
    }
    result = is_what(affected_card_controller, affected_card, 2);
    if (result != 0)
    {
      set_cost_mod_for_activated_abilities(affected_card_controller, affected_card, 1, NULL);
    }
    event_00 = EVENT_CAST_SPELL;
  }
  leaves_play(player, card, event_00, 0, 0);
  return 0;
}

int card_hermit_druid(int player, int card, event_t event){
  /* Hermit Druid	|1|G
   * Creature - Human Druid 1/1
   * |G, |T: Reveal cards from the top of your library until you reveal a basic land card. Put that card into your hand and all other cards revealed this way into your graveyard. */

	if( event == EVENT_RESOLVE_ACTIVATION  ){
		int *deck = deck_ptr[player];
		int count = 0;
		int good = 0;
		while( deck[count] != -1 ){
				if( is_basic_land_by_id(cards_data[deck[count]].id) ){
					good = 1;
					break;
				}
				else{
					count++;
				}
		}
		if( count > 0 ){
			show_deck( HUMAN, deck, count+1, "Cards revealed by Hermit Druid", 0, gs_done_008b40e0 );
			mill(player, count);
		}
		if( good == 1 ){
			add_card_to_hand(player, deck[0]);
			remove_card_from_deck(player, 0);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_G(1), 0, NULL, NULL);
}

int card_hesitation(int player, int card, event_t event)
{
  /* Hesitation	|1|U
   * Enchantment
   * When a player casts a spell, sacrifice this enchantment and counter that spell. */

  sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
  if (event == EVENT_SPA)
  {
    real_counter_a_spell(player, card, trigger_cause_controller, trigger_cause);
    kill_card(player, card, KILL_SACRIFICE);
  }
  return global_enchantment(player, card, event);
}

int hybernating_sliver_shared_ability(int player, int card, event_t event)
{
  int card_00;
  card_instance_t *instance;
  card_instance_t *sliver_or_bounce_target;
  int result;

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
  }
  else
  {
    sliver_or_bounce_target = in_play(result, card_00);
    if (sliver_or_bounce_target == NULL)
    {
      if (event == EVENT_CAN_ACTIVATE)
        return 0;
      if (event == EVENT_CLEANUP)
        kill_card(player, card, KILL_EXILE);
    }
  }
  result = is_humiliated(result, card_00, AL_NM_NL_ACT);
  if (result != 0)
    return 0;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    sliver_or_bounce_target = in_play(instance->targets[0].player, instance->targets[0].card);
    if (sliver_or_bounce_target != NULL)
    {
      bounce_permanent(instance->targets[0].player, instance->targets[0].card);
    }
  }
  return shared_sliver_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 0, 2, NULL, NULL);
}

int card_hybernating_sliver(int player, int card, event_t event){
	/*
	  Hibernation Sliver |U|B
	  Creature - Sliver 2/2
	  All Slivers have "Pay 2 life: Return this permanent to its owner's hand."
	*/

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &hybernating_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &hybernating_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int card_horn_of_greed(int player, int card, event_t event){
  /* Horn of Greed	|3
   * Artifact
   * Whenever a player plays a land, that player draws a card. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( whenever_a_player_plays_a_land(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY) ){
			draw_a_card(trigger_cause_controller);
		}
	}

	return 0;
}

int card_intruder_alarm(int player, int card, event_t event){
  /* Intruder Alarm	|2|U
   * Enchantment
   * Creatures don't untap during their controllers' untap steps.
   * Whenever a creature enters, untap all creatures. */

	if( ! is_humiliated(player, card, AL_TRIG) ){
	  // Creatures don't untap during their controllers' untap steps.
	  if (current_phase == PHASE_UNTAP && event == EVENT_UNTAP && is_what(affected_card_controller, affected_card, TYPE_CREATURE))
		get_card_instance(affected_card_controller, affected_card)->untap_status &= ~3;

	  // Whenever a creature enters the battlefield, untap all creatures.
	  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && new_specific_cip_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card))
		{
		  test_definition_t test;
		  default_test_definition(&test, TYPE_CREATURE);
		  new_manipulate_all(player, card, ANYBODY, &test, ACT_UNTAP);
		}

	  // AI
	  if (event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI && check_battlefield_for_id(ANYBODY, CARD_ID_INTRUDER_ALARM))
		ai_modifier -= 64;

	  if (event == EVENT_SHOULD_AI_PLAY)
		{
		  int p, c, mod;
		  for (p = 0; p <= 1; ++p)
			{
			  for (c = mod = 0; c < active_cards_count[p]; ++c)
				if (in_play(p, c) && is_what(p, c, TYPE_CREATURE))
				  {
					if (!check_for_special_ability(player, card, SP_KEYWORD_VIGILANCE))
					  mod += get_power(p, c);

					if (get_card_data(p, c)->extra_ability & EA_MANA_SOURCE)
					  mod -= 4;
				  }

			  if (p == HUMAN)
				ai_modifier += 12 * mod;
			  else
				ai_modifier -= 12 * mod;
			}
		}
	}

	return global_enchantment(player, card, event);
}

static int jinxed_ring_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_JINXED_RING)
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
          count_for_gfp_ability(player, card, event, ANYBODY, TYPE_PERMANENT, NULL);
        }
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);
    for (; amount > 0; --amount)
      damage_player(player, 1, player, card);
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_jinxed_ring(int player, int card, event_t event){
  /* Jinxed Ring	|2
   * Artifact
   * Whenever a nontoken permanent is put into your graveyard from the battlefield, this artifact deals 1 damage to you.
   * Sacrifice a creature: Target opponent gains control of this artifact. (This effect lasts indefinitely.) */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)jinxed_ring_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &jinxed_ring_effect, player, card);
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
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_PERMANENT, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			for (int i = 0; i < amount; ++i){ damage_player(player, 1, player, card); }
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			give_control_of_self(instance->parent_controller, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_ONLY_TARGET_OPPONENT | GAA_SACRIFICE_CREATURE, MANACOST0, 0, &td, NULL);
}

int card_lab_rats(int player, int card, event_t event){
  /* Lab Rats	|B
   * Sorcery
   * Buyback |4 (You may pay an additional |4 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Create a 1/1 black Rat creature token. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		basic_spell(player, card, event);
		if( spell_fizzled != 1 ){
			get_card_instance(player, card)->info_slot = buyback(player, card, MANACOST_X(4));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_RAT, &token);
		token.pow = token.tou = 1;
		generate_token(&token);
		if( get_card_instance(player, card)->info_slot == 1 ){
			bounce_permanent(player, card);
			return 0;
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

// lancer en-kor --> nomads en-kor

void lowland_basilisk_effect(int player, int card, int t_player, int t_card){
	create_targetted_legacy_effect(player, card, &die_at_end_of_combat, t_player, t_card);
}

int card_lowland_basilisk(int player, int card, event_t event){
  /* Lowland Basilisk	|2|G
   * Creature - Basilisk 1/3
   * Whenever this creature deals damage to a creature, destroy that creature at end of combat. */

	if( ! is_humiliated(player, card, AL_TRIG) ){
		if (event == EVENT_CHANGE_TYPE && affect_me(player, card) ){
			card_instance_t* instance = get_card_instance(player, card);
			instance->destroys_if_blocked |= DIFB_DESTROYS_UNPROTECTED;
		}

		for_each_creature_damaged_by_me(player, card, event, 0, &lowland_basilisk_effect, player, card);
	}

	return 0;
}

int card_mana_leak(int player, int card, event_t event){
  /* Mana Leak |1|U
   * 	  Instant
   * 	  Counter target spell unless its controller pays {3}. */

	if( event == EVENT_RESOLVE_SPELL ){
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, 3);
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return card_counterspell(player, card, event);
}

int card_mask_of_the_mimic(int player, int card, event_t event){
  /* Mask of the Mimic	|U
   * Instant
   * As an additional cost to cast this spell, sacrifice a creature.
   * Search your library for a card with the same name as target nontoken creature, put that card onto the battlefield, then shuffle. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_type = TARGET_TYPE_TOKEN;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			tutor_card_with_the_same_name(instance->targets[0].player, instance->targets[0].card, player, player,
											TUTOR_FROM_DECK, TUTOR_PLAY, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_SAC_CREATURE_AS_COST | GS_LITERAL_PROMPT,
						&td, "Select target nontoken creature.", 1, NULL);
}

int card_megrim(int player, int card, event_t event){
  /* Megrim |2|B
   * 	  Enchantment
   * 	  Whenever an opponent discards a card, Megrim deals 2 damage to that player. */

	if( ! is_humiliated(player, card, AL_TRIG) && discard_trigger(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, 0) ){
		damage_player(1-player, 2, player, card);
	}

	return global_enchantment(player, card, event);
}

int card_mind_games(int player, int card, event_t event){
  /* Mind Games	|U
   * Instant
   * Buyback |2|U (You may pay an additional |2|U as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Tap target artifact, creature, or land. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TWIDDLE", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_mod_for_tap_effects(player, card, 0);
			instance->info_slot = buyback(player, card, MANACOST_XU(2, 1));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_mind_peel(int player, int card, event_t event){
  /* Mind Peel	|B
   * Sorcery
   * Buyback |2|B|B (You may pay an additional |2|B|B as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Target player discards a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			instance->info_slot = buyback(player, card, MANACOST_XB(2, 2));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			discard(instance->targets[0].player, 0, player);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_mindwarper(int player, int card, event_t event){
  /* Mindwarper	|2|B|B
   * Creature - Spirit 0/0
   * This creature enters with three +1/+1 counters on it.
   * |2|B, Remove a +1/+1 counter from this creature: Target player discards a card. Activate only as a sorcery. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			discard(instance->targets[0].player, 0, player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED | GAA_1_1_COUNTER | GAA_CAN_TARGET, MANACOST_XB(2, 1),
									0, &td, "TARGET_PLAYER");
}

int card_mob_justice(int player, int card, event_t event){
	/* Mob Justice	|1|R	0x200a106
	 * Sorcery
	 * ~ deals damage to target player or planeswalker equal to the number of creatures you control. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, count_subtype(player, TYPE_CREATURE, -1));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
}

int card_mogg_bombers(int player, int card, event_t event){
	/* Mogg Bombers	|3|R	0x20035d6
	 * Creature - Goblin 3/4
	 * When another creature enters the battlefield, sacrifice ~ and it deals 3 damage to target player or planeswalker. */
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) &&
		!( trigger_cause_controller == player && trigger_cause == card) )
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		if( new_specific_cip_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			td.allow_cancel = 0;

			if( can_target(&td) && pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER") ){
				damage_target0(player, card, 3);
			}

			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_mogg_flunkies(int player, int card, event_t event)
{
  /* Mogg Flunkies	|1|R
   * Creature - Goblin 3/3
   * This creature can't attack or block alone. */

	cannot_attack_alone(player, card, event);
	cannot_block_alone(player, card, event);
	return 0;
}

int card_mogg_infestation(int player, int card, event_t event){
  /* Mogg Infestation	|3|R|R
   * Sorcery
   * Destroy all creatures target player controls. For each creature that died this way, that player creates two 1/1 red Goblin creature tokens. */

	//APPROXIMATION
	/*
	  Mogg Infestation |3|R|R
	  Sorcery
	  Destroy all creatures target player controls. For each creature that died this way, put two 1/1 red Goblin creature tokens
	  onto the battlefield under that player's control.
	*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			int amount = new_manipulate_all(player, card, instance->targets[0].player, &test, KILL_DESTROY);

			token_generation_t token;
			default_token_definition(player, card, CARD_ID_GOBLIN, &token);
			token.t_player = instance->targets[0].player;
			token.qty = amount * 2;
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_mogg_maniac(int player, int card, event_t event){
	/* Mogg Maniac	|1|R	0x20035e5
	 * Creature - Goblin 1/1
	 * Whenever ~ is dealt damage, it deals that much damage to target opponent or planeswalker. */
	if( ! is_humiliated(player, card, AL_TRIG) ){
	  card_instance_t* damage = damage_being_dealt(event);
	  if (damage
		  && damage->damage_target_card == card && damage->damage_target_player == player)
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  if (instance->info_slot < 10)
			{
			  if (instance->info_slot < 1)
				instance->info_slot = 1;

			  instance->targets[instance->info_slot++].player = damage->info_slot;
			}
		}

	  if (trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player)
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  if (instance->info_slot > 1)
			{
			  if (event == EVENT_TRIGGER)
				event_result |= RESOLVE_TRIGGER_MANDATORY;

			  if (event == EVENT_RESOLVE_TRIGGER)
				{
				  target_definition_t td_opw;
				  opponent_or_pw_target_definition(player, card, &td_opw);

				  // Free up info_slot and targets, just in case something triggers instantly (and improperly) which eventually results in dealing damage back.
				  int i, dam[10];
				  for (i = 1; i < instance->info_slot; ++i)
					dam[i] = instance->targets[i].player;

				  int numdam = instance->info_slot;
				  instance->info_slot = 1;

				  for (i = 1; i < numdam; ++i)
					if (can_target(&td_opw) && pick_target(&td_opw, "TARGET_OPPONENT_OR_PLANESWALKER"))
					  {
						damage_target0(player, card, dam[i]);
						instance->number_of_targets = 0;
					  }
				}
			}
		}
	}

	return 0;
}

int card_morgue_thrull(int player, int card, event_t event){
  /* Morgue Thrull	|2|B
   * Creature - Thrull 2/2
   * Sacrifice this creature: Mill three cards. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		mill(player, 3);
	}
	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

static int mortuary_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_MORTUARY)
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
        count_for_gfp_ability_and_store_values(
            player, card, event, player, TYPE_CREATURE, NULL,
            GFPC_TRACK_DEAD_CREATURES | GFPC_INTO_WHOSE_GRAVE | GFPC_EXTRA_SKIP_TOKENS, 1);
      }
    }
  }

  int resolve = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (resolve && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int k;
    int total = 0;
    for (k = 1; k < 10; k++)
    {
      if (instance->targets[k].player != -1)
      {
        int iid = instance->targets[k].player;
        int result = seek_grave_for_id_to_reanimate(player, -1, player, cards_data[iid].id,
                                                    REANIMATEXTRA_LEAVE_IN_GRAVEYARD);
        if (result != -1)
        {
          int card_added = add_card_to_hand(player, instance->targets[k].player);
          remove_card_from_grave(player, result);
          put_on_top_of_deck(player, card_added);
          total++;
        }
        instance->targets[k].player = -1;
      }
      if (instance->targets[k].card != -1)
      {
        int iid = instance->targets[k].player;
        int result = seek_grave_for_id_to_reanimate(player, -1, player, cards_data[iid].id,
                                                    REANIMATEXTRA_LEAVE_IN_GRAVEYARD);
        if (result != -1)
        {
          int card_added = add_card_to_hand(player, instance->targets[k].card);
          remove_card_from_grave(player, result);
          put_on_top_of_deck(player, card_added);
          total++;
        }
        instance->targets[k].card = -1;
      }
    }
    if (total > 1)
    {
      rearrange_top_x(player, player, total);
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_mortuary(int player, int card, event_t event){
  /* Mortuary	|3|B
   * Enchantment
   * Whenever a creature is put into your graveyard from the battlefield, put that card on top of your library. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)mortuary_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &mortuary_effect, player, card);
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
		count_for_gfp_ability_and_store_values(player, card, event, player, TYPE_CREATURE, NULL, GFPC_TRACK_DEAD_CREATURES, 0);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			card_instance_t *instance = get_card_instance(player, card);
			int k;
			int total = 0;
			for(k=0; k<10; k++){
				if( instance->targets[k].player != -1 ){
					int iid = instance->targets[k].player;
					int result = seek_grave_for_id_to_reanimate(player, -1, player, cards_data[iid].id, REANIMATEXTRA_LEAVE_IN_GRAVEYARD);
					if( result != -1 ){
						int card_added = add_card_to_hand(player, instance->targets[k].player);
						remove_card_from_grave(player, result);
						put_on_top_of_deck(player, card_added);
						total++;
					}
					instance->targets[k].player = -1;
				}
				if( instance->targets[k].card != -1 ){
					int iid = instance->targets[k].player;
					int result = seek_grave_for_id_to_reanimate(player, -1, player, cards_data[iid].id, REANIMATEXTRA_LEAVE_IN_GRAVEYARD);
					if( result != -1 ){
						int card_added = add_card_to_hand(player, instance->targets[k].card);
						remove_card_from_grave(player, result);
						put_on_top_of_deck(player, card_added);
						total++;
					}
					instance->targets[k].card = -1;
				}
			}
			if( total > 1 ){
				rearrange_top_x(player, player, total);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_mox_diamond(int player, int card, event_t event)
{
	/* Mox Diamond	|0
	 * Artifact
	 * If ~ would enter the battlefield, you may discard a land card instead. If you do, put ~ onto the battlefield. If you don't, put it into its owner's
	 * graveyard.
	 * |T: Add one mana of any color to your mana pool. */

	if (event == EVENT_RESOLVE_SPELL){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.zone = TARGET_ZONE_HAND;

		if(!select_target(player, card, &td, "Discard a land card.", NULL)){
			kill_card(player, card, KILL_STATE_BASED_ACTION);
		}
		else{
			card_instance_t* instance = get_card_instance(player, card);
			instance->number_of_targets = 0;
			discard_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return artifact_mana_all_one_color(player, card, event, 1, 0);
}

static int en_kor_card = -1;
/* ManalinkEh 2022: 0x212d5f0; original cards/stronghold.c:416. */
int card_conviction(int player, int card, event_t event)
{
  /* Conviction	|1|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+3.
   * |W: Return this Aura to its owner's hand. */

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
          return generic_aura(player, card, event, player, 1, 3, 0, 0, 0, 0, 0);
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          bounce_permanent(instance->parent_controller, instance->parent_card);
        }
      }
      return generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 1, 0, NULL, NULL);
    }
  }
  return generic_aura(player, card, event, player, 1, 3, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x212d9e0; original cards/stronghold.c:499. */
int card_dauthi_trapper(int player, int card, event_t event)
{
  /* Dauthi Trapper	|2|B
   * Creature - Dauthi Minion 1/1
   * |T: Target creature gains shadow until end of turn. (It can block or be blocked by only creatures with shadow.) */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                             0, 0, 0, 0x100);
    }
  }
  result = generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
  return result;
}

/* ManalinkEh 2022: 0x212ad80; original cards/stronghold.c:833. */
static int grave_pact_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  int death_count_or_result;
  card_instance_t *dying_permanent;
  test_definition_t test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (death_count_or_result =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           death_count_or_result != instance->targets[1].card))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  instance = get_card_instance(player, card);
  if ((-1 < instance->damage_target_player) &&
      (death_count_or_result = effect_follows_control_of_attachment(player, card, event),
       death_count_or_result != 0))
  {
    return 0;
  }
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = get_card_instance(player, card);
    dying_permanent = in_play(affected_card_controller, affected_card);
    if ((dying_permanent != NULL) &&
        (death_count_or_result = is_this_dying(affected_card_controller, affected_card),
         death_count_or_result != 0))
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
      count_for_gfp_ability(player, card, 0x77, player, 2, NULL);
    }
  }
  death_count_or_result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (death_count_or_result == 0 && event == EVENT_GRAVEYARD_FROM_PLAY)
    return 0;
  if (death_count_or_result != 0)
  {
    if (check_status(player, card, 0x10000) == 0)
    {

      new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
      test.qty = (unsigned short)death_count_or_result & 0xff;
      new_sacrifice(player, card, 1 - player, SAC_CAUSED | SAC_NO_CANCEL, &test);
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) &&
      (death_count_or_result = check_status(player, card, 0x10000), death_count_or_result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

static const char* is_damaging_me(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	card_instance_t* dmg = get_card_instance(player, card);
	if ( dmg->internal_card_id == damage_card && dmg->info_slot > 0 ){
		if( dmg->damage_target_player == who_chooses && dmg->damage_target_card == en_kor_card ){
			return NULL;
		}
	}

	return "must be damage to this card.";
}

int en_kor_damage_prevention_ability(int player, int card, event_t event){
	// Also code for: Lancers en-Kor, Spirit en-Kor, Warrior en-Kor

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_EFFECT);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(is_damaging_me);


	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.allowed_controller = player;
	td2.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		en_kor_card = card;
		int result = generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
		if( result && can_target(&td2) ){
			return result;
		}
	}

	if( event == EVENT_ACTIVATE ){
		en_kor_card = card;
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "Select a damage dealt to this card.", 0, 1 | GS_LITERAL_PROMPT) ){
			new_pick_target(&td2, "Select target creature you control.", 1, 1 | GS_LITERAL_PROMPT);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		en_kor_card = instance->parent_card;
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td2, 1) ){
			card_instance_t *dmg = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			dmg->info_slot--;
			damage_creature(instance->targets[1].player, instance->targets[1].card, 1, dmg->damage_source_player, dmg->damage_source_card);
		}
	}

	return 0;
}

int card_nomads_en_kor(int player, int card, event_t event){
  /* Nomads en-Kor	|W
   * Creature - Kor Nomad Soldier 1/1
   * |0: The next 1 damage that would be dealt to this creature this turn is dealt to target creature you control instead. */

	return en_kor_damage_prevention_ability(player, card, event);
}

int card_overgrowth(int player, int card, event_t event){
  /* Overgrowth	|2|G
   * Enchantment - Aura
   * Enchant land
   * Whenever enchanted land is tapped for mana, its controller adds an additional |G|G. */

	return wild_growth_aura(player, card, event, -1, COLOR_GREEN, 2);
}

int card_portcullis(int player, int card, event_t event){
/*CARD_ID_PORTCULLIS	3405
Portcullis	|4
Artifact
Whenever a creature enters the battlefield, if there are two or more other creatures on the battlefield, exile that creature.
Return that card to the battlefield under its owner's control when ~ leaves the battlefield. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me( player, card ) )
	{
		if( is_what(trigger_cause_controller, trigger_cause, TYPE_CREATURE) && count_subtype(ANYBODY, TYPE_CREATURE, -1) >= 3 ){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			if( new_specific_cip_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card) ){
				create_card_name_legacy(player, card, get_id(trigger_cause_controller, trigger_cause));
				exile_card_and_remember_it_on_exiledby(player, card, trigger_cause_controller, trigger_cause);
			}
		}
	}

	leaves_play(player, card, event, 1, 0);
	if (event == EVENT_LEAVES_PLAY_ABILITY){
		int leg = 0;
		int idx = 0;
		int* loc;

		while ((loc = exiledby_find_any(player, card, &leg, &idx)) != NULL){
				int owner = (*loc & 0x80000000) ? 1 : 0;
				int iid = *loc & ~0x80000000;
				*loc = -1;
				if( check_rfg(owner, cards_data[iid].id) ){
					int card_added = add_card_to_hand(owner, iid);
					remove_card_from_rfg(owner, cards_data[iid].id);
					put_into_play(owner, card_added);
				}
		}
	}

	return 0;
}

int card_primal_rage(int player, int card, event_t event){
  /* Primal Rage	|1|G
   * Enchantment
   * Creatures you control have trample. (A creature with trample can deal excess combat damage to the player or planeswalker it's attacking.) */

	if( event == EVENT_ABILITIES && ! is_humiliated(player, card, AL_STATIC) ){
		if( affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			event_result |= KEYWORD_TRAMPLE;
		}
	}

	return global_enchantment(player, card, event);
}

int card_pursuit_of_knowledge(int player, int card, event_t event){

	/* Pursuit of Knowledge	|3|W
	 * Enchantment
	 * If you would draw a card, you may put a study counter on ~ instead.
	 * Remove three study counters from ~, Sacrifice ~: Draw seven cards. */

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && reason_for_trigger_controller == player &&
		!suppress_draw && ! is_humiliated(player, card, AL_STATIC) )
	{
		if(event == EVENT_TRIGGER){
			event_result |= (!IS_AI(player) ? RESOLVE_TRIGGER_OPTIONAL
							 : count_counters(player, card, COUNTER_STUDY) < 3 ? RESOLVE_TRIGGER_MANDATORY
							 : 0);
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				add_counters(player, card, player, card, COUNTER_STUDY, 1);
				suppress_draw = 1;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 7);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, GVC_COUNTERS(COUNTER_STUDY, 3), NULL, NULL);
}

int card_rabid_rats(int player, int card, event_t event){
  /* Rabid Rats	|1|B
   * Creature - Rat 1/1
   * |T: Target blocking creature gets -1/-1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_BLOCKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							-1, -1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target blocking creature.");
}

int card_ransack(int player, int card, event_t event){
  /* Ransack	|3|U
   * Sorcery
   * Look at the top five cards of target player's library. Put any number of them on the bottom of that library in any order and the rest on top of the library in any order. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int total = get_total_available_lands(player);
		if( total <= 3 || total >= 6 ){
			ai_modifier += 48;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = MIN(5, count_deck(instance->targets[0].player));
			if( amount > 0 ){
				scrylike_effect(player, instance->targets[0].player, amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_reins_of_power(int player, int card, event_t event){
  /* Reins of Power |2|U|U
   * Instant
   * Untap all creatures you control and all creatures target opponent controls.
   * You and that opponent each gain control of all creatures the other controls until end of turn.
   * Those creatures gain haste until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
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
			new_manipulate_all(player, card, ANYBODY, &this_test, ACT_UNTAP);

			/* Snapshot both cohorts before either transfer.  Taking the opponent's
			 * creatures can grow our active range or reuse an empty slot; those new
			 * arrivals must not be given straight back by the second loop. */
			int c, marked[2][151] = {{0}};
			int original_count[2] = {active_cards_count[0], active_cards_count[1]};
			APNAP(p,{
						for (c = 0; c < original_count[p]; ++c){
							if (in_play(p, c) && is_what(p, c, TYPE_CREATURE)){
								marked[p][c] = 1;
							}
							else{
								marked[p][c] = 0;
							}
						};
					};
			);

			for (c = original_count[1-player]-1; c >-1 ; c--){
				if (marked[1-player][c]){
					effect_act_of_treason(player, card, 1-player, c);
				}
			}

			int fake = add_card_to_hand(1-player, instance->internal_card_id);
			for (c = original_count[player]-1; c >-1 ; c--){
				if (marked[player][c]){
					effect_act_of_treason(1-player, fake, player, c);
				}
			}
			obliterate_card(1-player, fake);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_revenant(int player, int card, event_t event)
{
  /* Boneyard Wurm	|1|G
   * Creature - Wurm 100/100
   * ~'s power and toughness are each equal to the number of creature cards in your graveyard. */

  int newval;
  if (((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    newval = count_graveyard_by_type(player, 2);
    set_characteristic_pt(newval);
  }
  return 0;
}

int card_rolling_stones(int player, int card, event_t event)
{
  /* Rolling Stones	|1|W
   * Enchantment
   * Wall creatures can attack as though they didn't have defender. */

  // 0x404020
  if (event == EVENT_ABILITIES
	  && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
	  && has_subtype(affected_card_controller, affected_card, SUBTYPE_WALL)
	  && in_play(player, card) && in_play(affected_card_controller, affected_card)
	  && !is_humiliated(player, card, AL_STATIC))
	add_status(affected_card_controller, affected_card, STATUS_WALL_CAN_ATTACK);

  return global_enchantment(player, card, event);
}

int card_ruination(int player, int card, event_t event){
  /* Ruination	|3|R
   * Sorcery
   * Destroy all nonbasic lands. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.subtype = SUBTYPE_BASIC;
		this_test.subtype_flag = DOESNT_MATCH;
		new_manipulate_all(player, card, ANYBODY, &this_test, KILL_DESTROY);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_scapegoat(int player, int card, event_t event){
  /* Scapegoat	|W
   * Instant
   * As an additional cost to cast this spell, sacrifice a creature.
   * Return any number of target creatures you control to their owner's hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.allow_cancel = 2; //"Done" button.

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		int trgs = instance->number_of_targets = 0;
		while( trgs < 10 && can_target(&td) ){
				if( new_pick_target(&td, "Select target creature you control.", trgs, GS_LITERAL_PROMPT) ){
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
		if( trgs == 0 ){
			int choice = 1;
			if( player == HUMAN ){
				choice = do_dialog(player, player, card, -1, -1, " Continue\n Abort casting", 0);
			}
			if( choice == 0 ){
				state_untargettable(BYTE2(sac), BYTE3(sac), 0);
				spell_fizzled = 1;
				return 0;
			}
		}
		kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
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

int card_seething_anger(int player, int card, event_t event){
  /* Seething Anger	|R
   * Sorcery
   * Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Target creature gets +3/+0 until end of turn. */

	if( event == EVENT_CHECK_PUMP ){
		if( can_sorcery_be_played(player, event) ){
			return vanilla_instant_pump(player, card, event, ANYBODY, player, 3, 0, 0, 0);
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 0, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 0, NULL);
		if( spell_fizzled != 1 ){
			instance->info_slot = buyback(player, card, MANACOST_X(3));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3, 0);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_shaman_en_kor(int player, int card, event_t event){
  /* Shaman en-Kor |1|W
   * 	  Creature � Kor Cleric Shaman 1/2
   * 	  {0}: The next 1 damage that would be dealt to Shaman en-Kor this turn is dealt to target creature you control instead.
   * 	  {1}{W}: The next time a source of your choice would deal damage to target creature this turn, that damage is dealt to Shaman en-Kor instead. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_EFFECT);
	td.extra = damage_card;
	td.preferred_controller = 1-player;
	td.special = TARGET_SPECIAL_DAMAGE_CREATURE;

	enum
	{
		CHOICE_PASS_DAMAGE = 1,
		CHOICE_TAKE_DAMAGE,
		CHOICE_CANCEL
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_XW(1, 1), 0, &td, NULL);
		if( result ){
			return result;
		}
		return en_kor_damage_prevention_ability(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int can_take_damage = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_XW(1, 1), 0, &td, NULL);
		int can_pass_damage = en_kor_damage_prevention_ability(player, card, EVENT_CAN_ACTIVATE);
		int choice = CHOICE_PASS_DAMAGE;
		if( can_pass_damage ){
			if( can_take_damage ){
				choice = DIALOG(player, card, event, DLG_RANDOM,
								"Pass damage", can_pass_damage, 10,
								"Take damage", can_take_damage, 5);
			}
		}
		else{
			choice = CHOICE_TAKE_DAMAGE;
		}
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_PASS_DAMAGE ){
			en_kor_damage_prevention_ability(player, card, event);
			if( spell_fizzled != 1 ){
				instance->info_slot = CHOICE_PASS_DAMAGE;
			}
		}
		if( choice == CHOICE_TAKE_DAMAGE ){
			if( new_pick_target(&td, "Select target damage dealt to a creature.", 0, 1 | GS_LITERAL_PROMPT) ){
				charge_mana_for_activated_ability(player, card, MANACOST_XW(1, 1));
				instance->info_slot = CHOICE_TAKE_DAMAGE;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PASS_DAMAGE  ){
			return en_kor_damage_prevention_ability(player, card, event);
		}

		if( instance->info_slot == CHOICE_TAKE_DAMAGE ){
			if( valid_target(&td) ){
				card_instance_t *dmg = get_card_instance(instance->targets[0].player, instance->targets[0].card);
				dmg->damage_target_player = instance->parent_controller;
				dmg->damage_target_card = instance->parent_card;
			}
		}
	}

	return 0;
}

int card_shard_phoenix(int player, int card, event_t event){
	/* Shard Phoenix	|4|R
	 * Creature - Phoenix 2/2
	 * Flying
	 * Sacrifice ~: ~ deals 2 damage to each creature without flying.
	 * |R|R|R: Return ~ from your graveyard to your hand. Activate this ability only during your upkeep. */

	if( event == EVENT_GRAVEYARD_ABILITY  && has_mana(player, COLOR_RED, 3) && current_phase == PHASE_UPKEEP && current_turn == player ){
		return GA_RETURN_TO_HAND;
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana(player, COLOR_RED, 3);
		if( spell_fizzled != 1){
			return GAPAID_REMOVE;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "");
		test.keyword = KEYWORD_FLYING;
		test.keyword_flag = DOESNT_MATCH;

		card_instance_t* instance = get_card_instance(player, card);
		new_damage_all(instance->parent_controller, instance->parent_card, ANYBODY, 2, 0, &test);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_shock2(int player, int card, event_t event){
	/* Bee Sting	|3|G	0x200cb8b
	 * Sorcery
	 * ~ deals 2 damage to any target. */
	/* Guerrilla Tactics	|1|R	0x200cb8b
	 * Instant
	 * ~ deals 2 damage to any target.
	 * When a spell or ability an opponent controls causes you to discard ~, ~ deals 4 damage to any target. */
	/* Shock	|R	0x200cb8b
	 * Instant
	 * ~ deals 2 damage to any target. */
	/* Tarfire	|R	0x200cb8b
	 * Tribal Instant - Goblin
	 * ~ deals 2 damage to any target. */
	/* Unyaro Bee Sting	|3|G	0x200cb8b
	 * Sorcery
	 * ~ deals 2 damage to any target. */
	/* Reality Hemorrhage	|1|R	0x200cb8b
	 * Instant
	 * Devoid
	 * ~ deals 2 damage to any target. */
	// Used externally
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_sift(int player, int card, event_t event){
  /* Sift	|3|U
   * Sorcery
   * Draw three cards, then discard a card. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int total = get_total_available_lands(player);
		if( total <= 3 || total >= 6 ){
			ai_modifier += 48;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 3);
		discard(player, 0, player);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_skeleton_scavengers(int player, int card, event_t event){
  /* Skeleton Scavengers	|2|B
   * Creature - Skeleton 0/0
   * This creature enters with a +1/+1 counter on it.
   * Pay |1 for each +1/+1 counter on this creature: Regenerate this creature. When it regenerates this way, put a +1/+1 counter on it. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 1);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( can_regenerate(instance->parent_controller, instance->parent_card) ){
			regenerate_target(instance->parent_controller, instance->parent_card);
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_REGENERATION, MANACOST_X(count_1_1_counters(player, card)), 0, NULL, NULL);
}

// spike token --> rhino token.

int card_skyshroud_archer(int player, int card, event_t event){
  /* Skyshroud Archer	|G
   * Creature - Elf Archer 1/1
   * |T: Target creature with flying gets -1/-1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							-1, -1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target creature with flying");
}

int card_sliver_queen(int player, int card, event_t event ){
  /* Sliver Queen	|W|U|B|R|G
   * Legendary Creature - Sliver 7/7
   * |2: Put a 1/1 colorless Sliver creature token onto the battlefield. */

	// asm implementation: 0x4DE208



	check_legend_rule(player, card, event);

	if( IS_GAA_EVENT(event) ){
		if( event == EVENT_RESOLVE_ACTIVATION ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SLIVER, &token);
			token.pow = token.tou = 1;
			generate_token(&token);
		}
		return generic_activated_ability(player, card, event, 0,MANACOST_X(2), 0, NULL, NULL);
	}

	return slivercycling(player, card, event);
}

int card_soltari_champion(int player, int card, event_t event)
{
  /* Soltari Champion	|2|W
   * Creature - Soltari Soldier 2/2
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * Whenever this creature attacks, other creatures you control get +1/+1 until end of turn. */

	shadow(player, card, event);

	// Whenever ~ attacks, all other creatures you control get +1/+1 until end of turn.
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY)
		{
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "");
			test.not_me = 1;
			pump_creatures_until_eot(player, card, player, 0, 1,1, 0,0, &test);
		}

	return 0;
}

int card_spike_breeder(int player, int card, event_t event){
	/* Spike Breeder	|3|G
	 * Creature - Spike 0/0
	 * ~ enters the battlefield with three +1/+1 counters on it.
	 * |2, Remove a +1/+1 counter from ~: Put a +1/+1 counter on target creature.
	 * |2, Remove a +1/+1 counter from ~: Put a 1/1 |Sgreen Spike creature token onto the battlefield. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE  ){
		return generic_activated_ability(player, card, event, GAA_1_1_COUNTER, MANACOST_X(2), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		if( generic_spike(player, card, EVENT_CAN_ACTIVATE, 6) ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_1_1_COUNTER, MANACOST_X(2), 0, NULL, NULL) ){
				int ai_choice = 1;
				if( current_phase == PHASE_AFTER_BLOCKING ){
					ai_choice = 0;
				}
				choice = do_dialog(player, player, card, -1, -1, " Add +1/+1 counter\n Spike token\n Cancel", ai_choice);
			}
		}
		else{
			choice = 1;
		}

		if( choice == 0 ){
			generic_spike(player, card, event, 2);
			if( spell_fizzled != 1 ){
				instance->info_slot = 66+choice;
			}
		}
		else if( choice == 1 ){
			generic_activated_ability(player, card, event, GAA_1_1_COUNTER, MANACOST_X(2), 0, NULL, NULL);
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
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
		}
		if( instance->info_slot == 67 ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SPIKE, &token);
			token.pow = 1;
			token.tou = 1;
			generate_token(&token);
		}
	}

	return 0;
}

int card_spike_colony(int player, int card, event_t event){
  /* Spike Colony	|4|G
   * Creature - Spike 0/0
   * This creature enters with four +1/+1 counters on it.
   * |2, Remove a +1/+1 counter from this creature: Put a +1/+1 counter on target creature. */

	return generic_spike(player, card, event, 4);
}

int card_spike_feeder(int player, int card, event_t event){
  /* Spike Feeder	|1|G|G
   * Creature - Spike 0/0
   * This creature enters with two +1/+1 counters on it.
   * |2, Remove a +1/+1 counter from this creature: Put a +1/+1 counter on target creature.
   * Remove a +1/+1 counter from this creature: You gain 2 life. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE  ){
		return generic_activated_ability(player, card, event, GAA_1_1_COUNTER, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		if( generic_spike(player, card, EVENT_CAN_ACTIVATE, 6) ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_1_1_COUNTER, MANACOST0, 0, NULL, NULL) ){
				int ai_choice = 0;
				if( life[player] < 6 ){
					ai_choice = 1;
				}
				choice = do_dialog(player, player, card, -1, -1, " Move a counter\n Gain 2 life\n Cancel", ai_choice);
			}
		}
		else{
			choice = 1;
		}

		if( choice == 0 ){
			generic_spike(player, card, event, 2);
			if( spell_fizzled != 1 ){
				instance->info_slot = 66+choice;
			}
		}
		else if( choice == 1 ){
			generic_activated_ability(player, card, event, GAA_1_1_COUNTER, MANACOST0, 0, NULL, NULL);
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
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
		}
		if( instance->info_slot == 67 ){
			gain_life(player, 2);
		}
	}

	return 0;
}

int card_spike_soldier(int player, int card, event_t event){
  /* Spike Soldier	|2|G|G
   * Creature - Spike Soldier 0/0
   * This creature enters with three +1/+1 counters on it.
   * |2, Remove a +1/+1 counter from this creature: Put a +1/+1 counter on target creature.
   * Remove a +1/+1 counter from this creature: This creature gets +2/+2 until end of turn. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);
    if (event == EVENT_CHECK_ABILITIES){
        if (can_use_activated_abilities(player, card) && count_counters(player, card, COUNTER_P1_P1) > 0){
            if (affect_me(player, card)){
                check_abilities_power += count_counters(player, card, COUNTER_P1_P1);
                check_abilities_toughness += count_counters(player, card, COUNTER_P1_P1);
            }
            if (affected_card_controller == player && affected_card != card &&
                has_mana_for_activated_ability(player, card, MANACOST_X(2))){
                int amount = has_mana_for_activated_ability(player, card, MANACOST_X(2));
                int counters = count_counters(player, card, COUNTER_P1_P1);
                if (counters < amount){ amount = counters; }
                check_abilities_power += amount;
                check_abilities_toughness += amount;
            }
        }
        return 0;
    }

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE  ){
		return generic_activated_ability(player, card, event, GAA_1_1_COUNTER, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		if( generic_spike(player, card, EVENT_CAN_ACTIVATE, 6) ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_1_1_COUNTER, MANACOST0, 0, NULL, NULL) ){
				int ai_choice = 0;
				if (current_phase == PHASE_AFTER_BLOCKING && is_attacking(player, card)){
                    ai_choice = 1;
                } else if (affected_card_controller == 1-player){
                    ai_choice = !!is_blocking_or_blocked_by(player, card, 1-player, affected_card);
                }
				choice = do_dialog(player, player, card, -1, -1, " Move a counter\n Pump Spike Soldier\n Cancel", ai_choice);
			}
		}
		else{
			choice = 1;
		}

		if( choice == 0 ){
			generic_spike(player, card, event, 2);
			if( spell_fizzled != 1 ){
				instance->info_slot = 66+choice;
			}
		}
		else if( choice == 1 ){
			generic_activated_ability(player, card, event, GAA_1_1_COUNTER, MANACOST0, 0, NULL, NULL);;
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
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
		}
		if( instance->info_slot == 67 ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 2, 2);
		}
	}

	return 0;
}

// spirit en-kor --> nomads en-kor

int card_spike_worker(int player, int card, event_t event){
  /* Spike Worker	|2|G
   * Creature - Spike 0/0
   * This creature enters with two +1/+1 counters on it.
   * |2, Remove a +1/+1 counter from this creature: Put a +1/+1 counter on target creature. */

	return generic_spike(player, card, event, 2);
}

int card_spindrift_drake(int player, int card, event_t event){
  /* Spindrift Drake	|U
   * Creature - Drake 2/1
   * Flying
   * At the beginning of your upkeep, sacrifice this creature unless you pay |U. */

	basic_upkeep(player, card, event, MANACOST_U(1));

	return 0;
}

int card_spined_sliver(int player, int card, event_t event){
  /* Spined Sliver	|R|G
   * Creature - Sliver 2/2
   * Whenever a Sliver becomes blocked, that Sliver gets +1/+1 until end of turn for each creature blocking it. */

	if( event == EVENT_DECLARE_BLOCKERS && ! is_humiliated(player, card, AL_TRIG) ){
		int i;
		for( i=0; i<active_cards_count[current_turn]; i++){
			if( in_play(current_turn, i) && is_what(current_turn, i, TYPE_CREATURE) && is_attacking(current_turn, i) &&
				has_subtype(current_turn, i, SUBTYPE_SLIVER)
				){
				int amount = count_my_blockers(current_turn, i);
				if( amount ){
					pump_until_eot(player, card, current_turn, i, amount, amount);
				}
			}
		}
	}

	return slivercycling(player, card, event);
}

int card_spitting_hydra(int player, int card, event_t event){
  /* Spitting Hydra	|3|R|R
   * Creature - Hydra 0/0
   * This creature enters with four +1/+1 counters on it.
   * |1|R, Remove a +1/+1 counter from this creature: It deals 1 damage to target creature. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 4);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), GVC_COUNTER(COUNTER_P1_P1),
									&td, "TARGET_CREATURE");
}

int card_stronghold_assassin(int player, int card, event_t event){
  /* Stronghold Assassin	|1|B|B
   * Creature - Phyrexian Zombie Assassin 2/1
   * |T, Sacrifice a creature: Destroy target nonblack creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE | GAA_LITERAL_PROMPT,
									MANACOST0, 0, &td1,
									get_sleighted_color_text(player, card, "Select target non%s creature", COLOR_BLACK));
}

int card_sword_of_the_chosen(int player, int card, event_t event){
  /* Sword of the Chosen	|2
   * Legendary Artifact
   * |T: Target legendary creature gets +2/+2 until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_subtype = SUBTYPE_LEGEND;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									 &td, "Select target legendary creature.");
}

int card_thalakos_deceiver(int player, int card, event_t event)
{
  /* Thalakos Deceiver	|3|U
   * Creature - Thalakos Wizard 1/1
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * Whenever this creature attacks and isn't blocked, you may sacrifice it. If you do, gain control of target creature. (This effect lasts indefinitely.) */

  int result;
  card_instance_t *instance;
  target_definition_t *creature_target_definition;
  target_definition_t td;

  shadow(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result != 0)
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    td.preferred_controller = td.allowed_controller;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = is_attacking(player, card);
      if (result == 0)
      {
        return 0;
      }
      result = is_unblocked(player, card);
      if (result == 0)
      {
        return 0;
      }
      if (current_phase != 0x18)
      {
        return 0;
      }
      return can_target(&td);
    }
    if (event == EVENT_ACTIVATE)
    {
      result = pick_target(&td, "TARGET_CREATURE");
      if (result == 0)
      {
        return 0;
      }
      kill_card(player, card, KILL_SACRIFICE);
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    result = is_humiliated(player, card, AL_TRIG);
    if (result != 0)
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    td.preferred_controller = td.allowed_controller;
    instance = get_card_instance(player, card);
  }
  creature_target_definition = &td;
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = valid_target(creature_target_definition), result != 0))
  {
    gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
    return 0;
  }
  return 0;
}

int card_tortured_existence(int player, int card, event_t event){

	/* Tortured Existence	|B
	 * Enchantment
	 * |B, Discard a creature card: Return target creature card from your graveyard to your hand. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
	test.zone = TARGET_ZONE_HAND;

	test_definition_t test2;
	new_default_test_definition(&test2, TYPE_CREATURE, "Select target creature card.");

	if( event == EVENT_CAN_ACTIVATE ){
		if( check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test) ){
			return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL,
																	0, &test2, GS_GRAVE_RECYCLER);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &test);
		if( selected == -1 ){
			spell_fizzled = 1;
			return 0;
		}
		generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL, 0, &test2, GS_GRAVE_RECYCLER);
		if( selected != -1 ){
			discard_card(player, selected);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
   }

   return global_enchantment(player, card, event);
}

int card_venerable_monk(int player, int card, event_t event){
  /* Venerable Monk	|2|W
   * Creature - Human Monk Cleric 2/2
   * When this creature enters, you gain 2 life. */

	return cip_lifegain(player, card, event, 2);
}

int victual_sliver_shared_ability(int player, int card, event_t event)
{
  int card_00;
  card_instance_t *instance;
  int result;

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
    result = is_humiliated(result, card_00, AL_NM_NL_ACT);
    if (result == 0)
      return shared_sliver_activated_ability(player, card, event, 8, 2, 0, 0, 0, 0, 0, 0, NULL,
                                             NULL);
  }
  else
  {
    instance = in_play(result, card_00);
    if (instance == NULL)
    {
      if (event == EVENT_CAN_ACTIVATE)
      {
        return 0;
      }
      if (event == EVENT_CLEANUP)
      {
        kill_card(player, card, KILL_EXILE);
        result = is_humiliated(result, card_00, AL_NM_NL_ACT);
        if (result == 0)
          return shared_sliver_activated_ability(player, card, event, 8, 2, 0, 0, 0, 0, 0, 0, NULL,
                                                 NULL);
        return 0;
      }
    }
    result = is_humiliated(result, card_00, AL_NM_NL_ACT);
    if (result == 0)
    {
      if (event == EVENT_RESOLVE_ACTIVATION)
      {
        gain_life(player, 4);
      }
      return shared_sliver_activated_ability(player, card, event, 8, 2, 0, 0, 0, 0, 0, 0, NULL,
                                             NULL);
    }
  }
  return 0;
}

int card_victual_sliver(int player, int card, event_t event){
  /* Victual Sliver |W|G
   * 	  Creature - Sliver 2/2
   * 	  All Slivers have "{2}, Sacrifice this permanent: You gain 4 life." */

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &victual_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &victual_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int card_volraths_gardens(int player, int card, event_t event){
  /* Volrath's Gardens	|1|G
   * Enchantment
   * |2, Tap an untapped creature you control: You gain 2 life. Activate only as a sorcery. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.allowed_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(2), 0,
								&td, "Select target untapped creature you control.");
		if( spell_fizzled != 1 ){
			instance->number_of_targets = 0;
			tap_card(instance->targets[0].player, instance->targets[0].card);
			if( life[player] < 6 ){
				ai_modifier += 48;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 2);
	}

	return 0;
}

int card_volraths_laboratory(int player, int card, event_t event){
	/* Volrath's Laboratory	|5
	 * Artifact
	 * As ~ enters the battlefield, choose a color and a creature type.
	 * |5, |T: Put a 2/2 creature token of the chosen color and type onto the battlefield. */


	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card );
		instance->targets[1].player = 1<<choose_a_color_and_show_legacy(player, card, 1-player, -1);
		instance->targets[1].card = select_a_subtype(player, card);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_custom_token_from_subtype(instance->parent_controller, instance->parent_card, instance->targets[1].card,
											instance->targets[1].player, 2, 2, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(5), 0, NULL, NULL);
}

static int vs_legacy_ability(int player, int card, event_t event)
{
  int p;
  card_instance_t *instance;
  int source_id_or_followed;
  int discard_choice_or_result;
  int source_card;
  test_definition_t test;
  if (((event == EVENT_STATIC_EFFECTS) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (source_id_or_followed =
             get_id((int)instance->damage_target_player, instance->damage_target_card),
         source_id_or_followed != 0xd7b))
    {
      remove_status(player, card, 0x10000);
    }
  }
  instance = get_card_instance(player, card);
  if (((instance->damage_target_player < '\0') ||
       (source_id_or_followed = effect_follows_control_of_attachment(player, card, event),
        source_id_or_followed == 0)) &&
      (instance = get_card_instance(player, card), -1 < instance->damage_target_player))
  {
    instance = get_card_instance(player, card);
    p = (int)instance->damage_target_player;
    if (event == EVENT_ACTIVATE)
    {
      source_card = instance->damage_target_card;
      if (is_humiliated(p, source_card, AL_NM_NL_ACT) == 0)
      {
        new_default_test_definition(&test, 2, "Select a creature card to discard.");
        return granted_generic_activated_ability(player, card, p, source_card, event, 0, 2, 0, 0, 0,
                                                 0, 0, 0, NULL, NULL);
      }
    }
    else if (((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2) || (event == EVENT_CLEANUP))
    {
      source_card = instance->damage_target_card;
      discard_choice_or_result = is_humiliated(p, source_card, AL_NM_NL_ACT);
      if (discard_choice_or_result == 0)
      {
        new_default_test_definition(&test, 2, "Select a creature card to discard.");
        if (((event == EVENT_RESOLVE_ACTIVATION) && (hand_count[p] != 0)) &&
            (discard_choice_or_result = new_select_a_card(p, p, 4, 1, 2, -1, &test),
             discard_choice_or_result != -1))
        {
          discard_card(p, discard_choice_or_result);
        }
        return granted_generic_activated_ability(player, card, p, source_card, event, 0, 2, 0, 0, 0,
                                                 0, 0, 0, NULL, NULL);
      }
    }
  }
  return 0;
}

int card_volraths_shapeshifter(int player, int card, event_t event){

	/* Volrath's Shapeshifter	|1|U|U
	 * Creature - Shapeshifter 0/1
	 * As long as the top card of your graveyard is a creature card, ~ has the full text of that card and has the text "|2: Discard a card."
	 * |2: Discard a card. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->damage_target_player == player && inst->damage_target_card == card &&
					inst->info_slot == (int)vs_legacy_ability )
				{
					found = 1;
					break;
				}
			}
		}
		if( ! found ){
			int legacy = create_targetted_legacy_activate(player, card, &vs_legacy_ability, player, card);
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_CHANGE_TYPE && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		const int *grave = get_grave(player);
		if( grave[0] != -1 && (cards_data[grave[count_graveyard(player)-1]].type & TYPE_CREATURE) ){
			event_result = grave[count_graveyard(player)-1];
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card to discard.");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( hand_count[player] ){
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MAX_CMC, -1, &test);
			if( selected != -1 ){
				discard_card(player, selected);
			}
		}
	}
	return generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
}

int card_volrath_stronghold(int player, int card, event_t event){
  /* Volrath's Stronghold	""
   * Legendary Land
   * |T: Add |C.
   * |1|B, |T: Put target creature card from your graveyard on top of your library. */

	check_legend_rule(player, card, event);

	return recycling_land(player, card, event, TYPE_CREATURE, MANACOST_XB(1, 1));
}
int card_wall_of_essence(int player, int card, event_t event)
{
  /* Wall of Essence	|1|W
   * Creature - Wall 0/4
   * Defender (This creature can't attack.)
   * Whenever this creature is dealt combat damage, you gain that much life. */

  if (!is_humiliated(player, card, AL_TRIG) && damage_dealt_to_me_arbitrary(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE, player, card))
  {
    card_instance_t* instance = get_card_instance(player, card);
    gain_life(player, instance->targets[7].card);
    instance->targets[7].card = 0;
  }
  return 0;
}
int card_wall_of_souls(int player, int card, event_t event){
	/* Wall of Souls	|1|B	0x2003653
	 * Creature - Wall 0/4
	 * Defender
	 * Whenever ~ is dealt combat damage, it deals that much damage to target opponent or planeswalker. */
	if( !is_humiliated(player, card, AL_TRIG) && damage_dealt_to_me_arbitrary(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE, player, card) ){
		target_definition_t td_opw;
		opponent_or_pw_target_definition(player, card, &td_opw);

		card_instance_t *instance = get_card_instance(player, card);

		if (can_target(&td_opw) && pick_target(&td_opw, "TARGET_OPPONENT_OR_PLANESWALKER")){
			damage_target0(player, card, instance->targets[7].card);
			instance->number_of_targets = 0;
		}
		instance->targets[7].card = 0;
	}

	return 0;
}

// warrior angel --> child of night

// warrior en-kor --> nomads en-kor

static int wot_effect(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;
  result = end_of_combat_trigger(player, card, event, 2);
  if (result == 0)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  result = check_state((int)instance->damage_target_player, instance->damage_target_card, 0x800000);
  if (result != 0)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  bounce_permanent((int)instance->damage_target_player, instance->damage_target_card);
  return 0;
}

static void create_stronghold_end_combat_bounce(int player, int card, int t_player, int t_card)
{
  create_targetted_legacy_effect(player, card, wot_effect, t_player, t_card);
  return;
}

int card_wall_of_tears(int player, int card, event_t event)
{
  /* Kaijin of the Vanishing Touch	|1|U
   * Creature - Spirit 0/3
   * Defender (This creature can't attack.)
   * Whenever ~ blocks a creature, return that creature to its owner's hand at end of combat. (Return it only if it's on the battlefield.) */

  card_instance_t *instance;
  int result;
  if (event == EVENT_CHANGE_TYPE)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = is_humiliated(player, card, AL_TRIG);
      if ((result == 0) && (1 - player == current_turn))
      {
        instance = get_card_instance(player, card);
        instance->destroys_if_blocked = instance->destroys_if_blocked | 0xc;
      }
    }
  }
  else if ((event == EVENT_DECLARE_BLOCKERS) && (1 - player == current_turn))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      for_each_creature_blocked_by_me(player, card, create_stronghold_end_combat_bounce, player,
                                      card);
    }
  }
  return 0;
}

int card_calming_licid(int player, int card, event_t event){
  /* Calming Licid	|2|W
   * Creature - Licid 2/2
   * |W, |T: This creature loses this ability and becomes an Aura enchantment with enchant creature. Attach it to target creature. You may pay |W to end this effect.
   * Enchanted creature can't attack. */

	if (!is_what(player, card, TYPE_CREATURE)){
		effect_cannot_attack(player, card, event);
	}

	return generic_licid(player, card, event, MANACOST_W(1), 0, 0, 0, 0, 1-player);
}

int card_convulsing_licid(int player, int card, event_t event){
  /* Convulsing Licid	|2|R
   * Creature - Licid 2/2
   * |R, |T: This creature loses this ability and becomes an Aura enchantment with enchant creature. Attach it to target creature. You may pay |R to end this effect.
   * Enchanted creature can't block. */

	return generic_licid(player, card, event, MANACOST_R(1), 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK, 1-player);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int tidal_warrior_effect(int player, int card, event_t event);

int card_corrupting_licid(int player, int card, event_t event){
  /* Corrupting Licid	|2|B
   * Creature - Licid 2/2
   * |B, |T: This creature loses this ability and becomes an Aura enchantment with enchant creature. Attach it to target creature. You may pay |B to end this effect.
   * Enchanted creature has fear. (It can't be blocked except by artifact creatures and/or black creatures.) */

	return generic_licid(player, card, event, MANACOST_B(1), 0, 0, 0, SP_KEYWORD_FEAR, player);
}

int card_gliding_licid(int player, int card, event_t event){
  /* Gliding Licid	|2|U
   * Creature - Licid 2/2
   * |U, |T: This creature loses this ability and becomes an Aura enchantment with enchant creature. Attach it to target creature. You may pay |U to end this effect.
   * Enchanted creature has flying. */

	return generic_licid(player, card, event, MANACOST_U(1), 0, 0, KEYWORD_FLYING, 0, player);
}

int card_tempting_licid(int player, int card, event_t event){
  /* Tempting Licid	|2|G
   * Creature - Licid 2/2
   * |G, |T: This creature loses this ability and becomes an Aura enchantment with enchant creature. Attach it to target creature. You may pay |G to end this effect.
   * All creatures able to block enchanted creature do so. */

	return generic_licid(player, card, event, MANACOST_G(1), 0, 0, 0, SP_KEYWORD_LURE, player);
}

/* ManalinkEh 2022: 0x2135220; original cards/stronghold.c:3439. */
int card_crossbow_ambush(int player, int card, event_t event)
{
  /* Crossbow Ambush	|G
   * Instant
   * Creatures you control gain reach until end of turn. (They can block creatures with flying.) */

  int result;
  if (event == EVENT_RESOLVE_SPELL)
  {
    pump_subtype_until_eot(player, card, player, -1, 0, 0, 0x400, 0);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x21352a0; original cards/stronghold.c:3452. */
int card_dream_prowler(int player, int card, event_t event)
{
  /* Dream Prowler	|2|U|U
   * Creature - Illusion 1/5
   * This creature can't be blocked as long as it's attacking alone. */

  int result;

  if (event == EVENT_BLOCK_LEGALITY)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((((result == 0) && (attacking_card_controller == player)) && (attacking_card == card)) &&
        (number_of_attackers_declared == 1))
    {
      event_result = 1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2135300; original cards/stronghold.c:3466. */
int card_duct_crawler(int player, int card, event_t event)
{
  /* Duct Crawler	|R
   * Creature - Insect 1/1
   * |1|R: Target creature can't block this creature this turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GAA_EVENT(event))
    return 0;
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = 1 - player;
  if (event == EVENT_ACTIVATE)
  {
    result = -0x18;
    if (current_turn == player)
    {
      result = (unsigned int)((current_phase < 0x17) ? 24 : -24);
    }
    ai_modifier = ai_modifier + result;
  }

  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {

    instance = get_card_instance(player, card);
    creature1_cant_block_creature2_until_eot(instance->parent_controller, instance->parent_card,
                                             instance->targets[0].player, instance->targets[0].card,
                                             instance->parent_controller, instance->parent_card);
  }
  return generic_activated_ability(player, card, event, 6, 1, 0, 0, 0, 1, 0, 0, &td,
                                   "Select target creature can\'t block Duct Crawler this turn");
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21354a0; original cards/stronghold.c:3497. */
int card_endangered_armodon(int player, int card, event_t event)
{
  /* Endangered Armodon	|2|G|G
   * Creature - Elephant 4/5
   * When you control a creature with toughness 2 or less, sacrifice this creature. */

  int result;

  if (((event == EVENT_STATIC_EFFECTS) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      result = count_creatures_by_toughness(player, 3, 2);
      if (result != 0)
      {
        kill_card(player, card, KILL_SACRIFICE);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x212b380; original cards/stronghold.c:3509. */
static int source_type_spell_damage_prevented(int player, int card, event_t event)
{
  card_instance_t *damage;
  card_instance_t *instance;

  damage = damage_being_dealt(event);
  instance = get_card_instance(player, card);
  if ((((damage != NULL) && ((int)damage->damage_source_player == instance->targets[0].player)) &&
       (damage->damage_source_card == instance->targets[0].card)) &&
      (-1 < damage->info_slot))
  {
    damage->info_slot = 0;
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x21358b0; original cards/stronghold.c:3647. */
int card_samite_blessing(int player, int card, event_t event)
{
  /* Samite Blessing	|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has "|T: The next time a source of your choice would deal damage to target creature this turn, prevent that damage." */

  char enchanted_player;
  int target_card;
  card_instance_t *instance;
  int ability_result;
  target_definition_t td1;

  instance = get_card_instance(player, card);
  enchanted_player = instance->damage_target_player;
  target_card = instance->damage_target_card;
  default_target_definition(player, card, &td1, 2);
  ability_result = aura_granting_activated_ability(
      player, card, event, (int)enchanted_player, 7, 0, 0, 0, 0, 0, 0, 0, &td1,
      "Select target creature, prevent all damage the next source would deal to it.");
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (validate_target(player, card, &td1, 0) != 0)
    {
      prevent_the_next_n_damage((int)enchanted_player, target_card, instance->targets[0].player,
                                instance->targets[0].card, 0, PREVENT_INFINITE, 0, 0);
    }
  }
  return ability_result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21359f0; original cards/stronghold.c:3674. */
int card_smite(int player, int card, event_t event)
{
  /* Smite	|W (Stronghold)
   * Instant
   * Destroy target blocked creature. */

  card_instance_t *instance;
  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.required_state = TARGET_STATE_ISBLOCKED;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      instance = get_card_instance(player, card);
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td,
                         "Select target blocked creature to destroy.", 1, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2135b40; original cards/stronghold.c:3700. */
int card_stronghold_taskmaster(int player, int card, event_t event)
{
  /* Stronghold Taskmaster	|2|B|B
   * Creature - Giant Minion 4/3
   * Other black creatures get -1/-1. */

  card_instance_t *instance;
  unsigned int creature_color;
  unsigned int weakened_color;

  if ((((uint32_t)(event - EVENT_POWER) < 2) &&
       (((card != affected_card || (player != affected_card_controller)) &&
         (instance = in_play(player, card), instance != NULL)))) &&
      ((instance = in_play(affected_card_controller, affected_card),
        instance != NULL && (is_humiliated(player, card, AL_STATIC) == 0))))
  {
    creature_color = get_color(affected_card_controller, affected_card);
    weakened_color = get_sleighted_color_test(player, card, 2);
    if ((creature_color & weakened_color) != 0)
    {
      event_result = event_result + -1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2135c20; original cards/stronghold.c:3717. */
int card_temper(int player, int card, event_t event)
{
  /* Temper	|X|1|W
   * Instant
   * Prevent the next X damage that would be dealt to target creature this turn. For each 1 damage prevented this way, put a +1/+1 counter on that creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
    result = generic_spell(player, card, EVENT_CAN_CAST, 5, &td, "TARGET_CREATURE", 1, NULL);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        charge_mana(player, COLOR_COLORLESS, -1);
        if (spell_fizzled != 1)
        {
          generic_spell(player, card, EVENT_CAST_SPELL, 5, &td, "TARGET_CREATURE", 1, NULL);
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
      result = valid_target(&td);
      if (result != 0)
      {
        prevent_the_next_n_damage(player, card, instance->targets[0].player,
                                  instance->targets[0].card, instance->info_slot,
                                  PREVENT_ADD_1_1_COUNTER, 0, 0);
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
  return 0;
}

/* ManalinkEh 2022: 0x212c210; original cards/stronghold.c:3751. */
static int tidal_warrior_effect(int player, int card, event_t event)
{
  int basic_land_type_to_become;
  card_instance_t *instance;
  if (event == EVENT_CHANGE_TYPE)
  {
    instance = get_card_instance(player, card);
    if ((instance->damage_target_card == affected_card) &&
        (instance->damage_target_player == affected_card_controller))
    {
      basic_land_type_to_become = get_hacked_subtype(player, card, 0x5002);
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

/* ManalinkEh 2022: 0x2135e70; original cards/stronghold.c:3769. */
int card_tidal_warrior(int player, int card, event_t event)
{
  /* Tidal Warrior	|U
   * Creature - Merfolk Warrior 1/1
   * |T: Target land becomes an Island until end of turn. */

  int32_t source_card;
  card_instance_t *instance;
  int target_valid;
  card_instance_t *island_effect;
  target_definition_t td;

  if (((event != EVENT_ACTIVATE) && (1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION))) &&
      (event != EVENT_CLEANUP))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 1);
  if (player == 1 && (ai_is_speculating == 1 || ((unsigned char)trace_mode & 2) == 0))
  {
    td.required_subtype = get_hacked_subtype(1, card, 0x5002);
    td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
  }
  instance = get_card_instance(player, card);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (target_valid = valid_target(&td), target_valid != 0))
  {
    island_effect = get_card_instance(
        player, create_targetted_legacy_effect(instance->parent_controller, instance->parent_card,
                                               tidal_warrior_effect, instance->targets[0].player,
                                               instance->targets[0].card));
    source_card = instance->parent_card;
    island_effect->targets[0].player = instance->parent_controller;
    island_effect->targets[0].card = source_card;
    island_effect->number_of_targets = '\x01';
  }
  return generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td, "TARGET_LAND");
}

/* ManalinkEh 2022: 0x2136010; original cards/stronghold.c:3802. */
int card_verdant_touch(int player, int card, event_t event)
{
  /* Verdant Touch	|1|G
   * Sorcery
   * Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Target land becomes a 2/2 creature that's still a land. (This effect lasts indefinitely.) */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 1);
    td.preferred_controller = player;
    td.illegal_type = TYPE_CREATURE;
    get_card_instance(player, card);
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 1);
      td.illegal_type = TYPE_CREATURE;
      td.preferred_controller = player;
      instance = get_card_instance(player, card);
      if ((card == affected_card) &&
          ((player == affected_card_controller &&
            (generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                           "Select target land it becomes a 2/2 creature land.", 1, NULL),
             spell_fizzled != 1))))
      {
        result = buyback(player, card, 3, 0, 0, 0, 0, 0);
        instance->info_slot = result;
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 1);
    td.illegal_type = TYPE_CREATURE;
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = valid_target(&td);
      if ((result == 0) || (land_animation2(player, card, instance->targets[0].player,
                                            instance->targets[0].card, 0, 2, 2, 0, 0, 0, NULL),
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
    default_target_definition(player, card, &td, 1);
    td.illegal_type = TYPE_CREATURE;
    td.preferred_controller = player;
    get_card_instance(player, card);
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x21362e0; original cards/stronghold.c:3843. */
int card_walking_dream(int player, int card, event_t event)
{
  /* Walking Dream	|3|U
   * Creature - Illusion 3/3
   * This creature can't be blocked.
   * This creature doesn't untap during your untap step if an opponent controls two or more creatures. */

  int result;

  unblockable(player, card, event);
  if (((event == EVENT_UNTAP) && (card == affected_card)) && (player == affected_card_controller))
  {
    result = count_subtype(1 - player, 2, -1);
    if (1 < result)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        does_not_untap(player, card, EVENT_UNTAP);
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2135130; original cards/stronghold.c:3419. */
int card_contempt(int player, int card, event_t event)
{
  /* Contempt	|1|U
   * Enchantment - Aura
   * Enchant creature
   * When enchanted creature attacks, return it and this Aura to their owners' hands at end of combat. */

  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {

    instance = get_card_instance(player, card);
    store_attackers(player, card, event, 0, (int)instance->damage_target_player,
                    instance->damage_target_card, NULL);
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      bounce_permanent(player, card);
      bounce_permanent((int)instance->damage_target_player, instance->damage_target_card);
    }
  }
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2135530; original cards/stronghold.c:3524. */
int card_hidden_retreat(int player, int card, event_t event)
{
  /* Hidden Retreat	|2|W
   * Enchantment
   * Put a card from your hand on top of your library: Prevent all damage that would be dealt by target instant or sorcery spell this turn. */

  int32_t target_spell_card;
  card_instance_t *instance;
  int target_player_or_result;
  int legacy_card;
  target_definition_t td_spell;
  test_definition_t this_test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    counterspell_target_definition(player, card, &td_spell, 0x38);
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (hand_count[player] < 1)
      {
        return 0;
      }
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x400, 0, 0, 0, 0, 0, 0, 0,
                                    &td_spell, NULL) == 0)
      {
        return 0;
      }
      return 99;
    }
    if (event == EVENT_ACTIVATE)
    {
      if (hand_count[player] < 1)
      {
        return 0;
      }
      instance->info_slot = 0;
      if (generic_activated_ability(player, card, EVENT_ACTIVATE, 0x400, 0, 0, 0, 0, 0, 0, 0,
                                    &td_spell, NULL) == 0)
      {
        return 0;
      }
      new_default_test_definition(&this_test, 0xff, "Select a card to put on top of your library");
      target_player_or_result = new_global_tutor(player, player, 4, 10, 0, 3, &this_test);
      if (target_player_or_result == -2)
      {
        return 0;
      }
      ((unsigned char *)&instance->info_slot)[0] = 1;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return global_enchantment(player, card, event);
    }
    counterspell_target_definition(player, card, &td_spell, 0x38);
    instance = get_card_instance(player, card);
  }
  if ((((event == EVENT_RESOLVE_ACTIVATION) && (instance->info_slot == 1)) &&
       (target_player_or_result = counterspell_validate(player, card, &td_spell, 0),
        target_player_or_result != 0)) &&
      ((target_player_or_result = instance->targets[0].player,
        target_player_or_result != -1 && (instance->info_slot == 1))))
  {
    target_spell_card = instance->targets[0].card;
    legacy_card = create_legacy_effect(player, card, source_type_spell_damage_prevented);
    instance = get_card_instance(player, legacy_card);
    instance->targets[0].player = target_player_or_result;
    instance->targets[0].card = target_spell_card;
    return 0;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static void destroy_token_at_next_eot(token_generation_t *token, int blue_wall, int number);

/* ManalinkEh 2022: 0x212c130; original cards/stronghold.c:3579. */
static int effect_destroy_at_next_eot(int player, int card, event_t event)
{
  char enchanted_player;
  int turns_remaining_or_card;
  card_instance_t *instance;
  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {
    instance = get_card_instance(player, card);
    if ((((trigger_condition == TRIGGER_EOT) && (card == affected_card)) &&
         (player == affected_card_controller)) &&
        (player == reason_for_trigger_controller))
    {
      turns_remaining_or_card = instance->targets[1].player;
      if (turns_remaining_or_card < 1)
      {
        if (event == EVENT_TRIGGER)
        {
          event_result = event_result | 2;
        }
        else if (event == EVENT_RESOLVE_TRIGGER)
        {
          enchanted_player = instance->damage_target_player;
          turns_remaining_or_card = instance->damage_target_card;
          if (check_state((int)enchanted_player, turns_remaining_or_card, STATE_OUBLIETTED) == 0)
          {
            kill_card((int)enchanted_player, turns_remaining_or_card, KILL_SACRIFICE);
          }
          else
          {
            kill_card(player, card, KILL_EXILE);
          }
        }
      }
      else
      {
        instance->targets[1].player = turns_remaining_or_card + -1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x212ad10; original cards/stronghold.c:3610. */
static void destroy_token_at_next_eot(token_generation_t *token, int blue_wall, int number)
{
  int card;
  card_instance_t *instance;

  card = create_targetted_legacy_effect(token->s_player, token->s_card, effect_destroy_at_next_eot,
                                        token->t_player, blue_wall);
  if ((current_phase == 0x1f) && (-1 < card))
  {
    instance = get_card_instance(token->s_player, card);
    instance->targets[1].player = 1;
    return;
  }
  return;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21357f0; original cards/stronghold.c:3619. */
int card_hornet_cannon(int player, int card, event_t event)
{
  /* Hornet Cannon	|4
   * Artifact
   * |3, |T: Create a 1/1 colorless Insect artifact creature token with flying and haste named Hornet. Destroy it at the beginning of the next end step. */

  int result;
  token_generation_t token;

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    default_token_definition(player, card, CARD_ID_HORNET, &token);
    token.s_key_plus = 4;
    token.special_code_on_generation = destroy_token_at_next_eot;
    generate_token(&token);
  }
  result = generic_activated_ability(player, card, event, 1, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

#endif
