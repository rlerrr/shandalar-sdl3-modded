#include "manalink.h"

// Functions

static int was_dealt_damage_this_turn(int unused, intptr_t unused2, int player, int card);
static int mirror_strike_effect(int player, int card, event_t event);

void death_charmer_effect(intptr_t player, intptr_t card, int t_player, int t_card);
void plague_fiend_effect(intptr_t player, intptr_t card, int t_player, int t_card);
void soul_charmer_effect(intptr_t player, intptr_t card, int t_player, int t_card);
int destroy_unless_pay_2(int player, int card, event_t event);
int gain_2_life_unless_they_pay_2(int player, int card, event_t event);
int lose_2_life_unless_pay_2(int player, int card, event_t event);
int get_power_sum_of_untapped_creatures(int player);

static int control_an_untapped_land(int player){
	int i;
	for(i=0; i<2; i++){
		if( player == i || player == 2 ){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && is_what(i, count, TYPE_LAND) && ! is_tapped(i, count) ){
						return 1;
					}
					count++;
			}
		}
	}
	return 0;
}

// Cards

int card_abolish(int player, int card, event_t event){
  /* Abolish |1|W|W
   * 	  Instant
   * 	  You may discard a Plains card rather than pay Abolish's mana cost.
   * 	  Destroy target artifact or enchantment. */

	if( event == EVENT_MODIFY_COST ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a Plains card.");
		this_test.zone = TARGET_ZONE_HAND;
		this_test.subtype = SUBTYPE_PLAINS;	// can't hack in time
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
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

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_LAND, "Select a Plains card.");
	this_test.zone = TARGET_ZONE_HAND;
	this_test.subtype = SUBTYPE_PLAINS;	// can't hack in time

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_id(player, event, get_id(player, card)) ){
				choice = do_dialog(player, player, card, -1, -1, " Discard a Plains card\n Play normally\n Cancel", 1);
			}

			if( choice == 0 ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
				if( selected > -1 ){
					discard_card(player, selected);
					td.allow_cancel = 0;
				}
				else{
					spell_fizzled = 1;
				}
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
			}
			if( spell_fizzled == 1 ){
				return 0;
			}
		}
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_alexi_zephyr_mage(int player, int card, event_t event){
  /* Alexi, Zephyr Mage |3|U|U
   * Legendary Creature — Human Spellshaper 3/3, 3UU (5)
   * {X}{U}, {T}, Discard two cards: Return X target creatures to their owners' hands. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && hand_count[player] > 1 ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(1), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int trgs = 0;
		while( has_mana_for_activated_ability(player, card, MANACOST_XU(trgs, 1)) && can_target(&td) ){
				if( new_pick_target(&td, "TARGET_CREATURE", trgs, 0) ){
					state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
					trgs++;
				}
				else{
					break;
				}
		}
		if( trgs == 0 ){
			spell_fizzled = 1;
		}
		else{
			int i;
			for(i=0; i<trgs; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			}
			instance->number_of_targets = trgs;
			charge_mana_for_activated_ability(player, card, MANACOST_XU(trgs, 1));
			if( spell_fizzled != 1 ){
				tap_card(player, card);
				multidiscard(player, 2, 0);
				instance->info_slot = trgs;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int i;
		for(i=0; i<instance->info_slot; i++){
			if( validate_target(player, card, &td, i) ){
				bounce_permanent(instance->targets[i].player, instance->targets[i].card);
			}
		}
	}

	return 0;
}

int card_aura_fracture(int player, int card, event_t event){
  /* Aura Fracture |2|W
   * Enchantment
   * Sacrifice a land: Destroy target enchantment. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ENCHANTMENT",
															0, &test, 0);
}

int card_avatar_of_hope(int player, int card, event_t event)
{
/* CARD_ID_AVATAR_OF_HOPE	975
Avatar of Hope	|6|W|W
Creature - Avatar 4/9
If you have 3 or less life, ~ costs |6 less to cast.
Flying
~ can block any number of creatures. */

	// 415910
	if (event == EVENT_MODIFY_COST && life[player] <= 3){
		COST_COLORLESS -= 6;
	}

	creature_can_block_additional(player, card, event, 255);

	return 0;
}

int card_avatar_of_fury(int player, int card, event_t event){
/* CARD_ID_AVATAR_OF_FURY	4796
Avatar of Fury	|6|R|R
Creature - Avatar 6/6
If an opponent controls seven or more lands, ~ costs |6 less to cast.
Flying
|R: ~ gets +1/+0 until end of turn. */

	if( event == EVENT_MODIFY_COST ){
		if( count_subtype(1-player, TYPE_LAND, -1) > 6 ){
			COST_COLORLESS-=6;
		}
	}

	return generic_shade(player, card, event, 0, MANACOST_R(1), 1, 0, 0, 0);
}

int card_avatar_of_might(int player, int card, event_t event){
/* CARD_ID_AVATAR_OF_MIGHT	4797
Avatar of Might	|6|G|G
Creature - Avatar 8/8
If an opponent controls at least four more creatures than you, ~ costs |6 less to cast.
Trample (This creature can deal excess combat damage to defending player or planeswalker while attacking.) */

	if( event == EVENT_MODIFY_COST ){
		if( count_subtype(1-player, TYPE_CREATURE, -1)-count_subtype(player, TYPE_CREATURE, -1) > 3){
			COST_COLORLESS-=6;
		}
	}

	return 0;
}

int card_avatar_of_will(int player, int card, event_t event){
/* CARD_ID_AVATAR_OF_WILL	4798
Avatar of Will	|6|U|U
Creature - Avatar 5/6
If an opponent has no cards in hand, ~ costs |6 less to cast.
Flying */

	if( event == EVENT_MODIFY_COST ){
		if( hand_count[1-player] < 1 ){
			COST_COLORLESS-=6;
		}
	}

	return 0;
}

int card_avatar_of_wOE(int player, int card, event_t event){
/* CARD_ID_AVATAR_OF_WOE	4799
Avatar of Woe	|6|B|B
Creature - Avatar 6/5
If there are ten or more creature cards total in all graveyards, ~ costs |6 less to cast.
Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
|T: Destroy target creature. It can't be regenerated. */

	fear(player, card, event);

	if( event == EVENT_MODIFY_COST ){
		if( count_graveyard_by_type(1-player, TYPE_CREATURE)+count_graveyard_by_type(player, TYPE_CREATURE) > 9){
			COST_COLORLESS-=6;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_blessed_wind(int player, int card, event_t event){
/* CARD_ID_BLESSED_WIND	4801
Blessed Wind	|7|W|W
Sorcery
Target player's life total becomes 20. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			if( instance->targets[0].player == player ){
				ai_modifier += ((20-life[player]) * 8);
			}
			if( instance->targets[0].player == 1-player ){
				ai_modifier += ((life[1-player]-20) * 8);
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			set_life_total(instance->targets[0].player, 20);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_branded_brawlers(int player, int card, event_t event){
/* CARD_ID_BRANDED_BRAWLERS	4804
Branded Brawlers	|R
Creature - Human Soldier 2/2
~ can't attack if defending player controls an untapped land.
~ can't block if you control an untapped land. */

	if (event == EVENT_ATTACK_LEGALITY && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) && control_an_untapped_land(1-player))
		event_result = 1;

	if (event == EVENT_BLOCK_LEGALITY && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) && control_an_untapped_land(player))
		event_result = 1;

	return 0;
}

int card_calming_verses(int player, int card, event_t event){
/* CARD_ID_CALMING_VERSE	4806
Calming Verse	|3|G
Sorcery
Destroy all enchantments you don't control. Then, if you control an untapped land, destroy all enchantments you control. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT);
		new_manipulate_all(player, card, 1-player, &this_test, KILL_DESTROY);
		if( control_an_untapped_land(player) ){
			new_manipulate_all(player, card, player, &this_test, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_chimeric_idol(int player, int card, event_t event){
/* CARD_ID_CHIMERIC_IDOL	4809
Chimeric Idol	|3
Artifact
|0: Tap all lands you control. ~ becomes a 3/3 Turtle artifact creature until end of turn. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		new_manipulate_all(player, card, player, &this_test, ACT_TAP);
		int legacy = artifact_animation(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
							1, 3, 3, 0, 0);
		get_card_instance(instance->parent_controller, legacy)->targets[9].card = SUBTYPE_TURTLE;
	}

	return generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST0, 0, NULL, NULL);
}

int card_citadel_of_pain(int player, int card, event_t event){
/* CARD_ID_CITADEL_OF_PAIN	4810
Citadel of Pain	|2|R
Enchantment
At the beginning of each player's end step, ~ deals X damage to that player, where X is the number of untapped lands he or she controls. */

	if( eot_trigger(player, card, event) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.state = STATE_TAPPED;
		this_test.state_flag = 1;
		int amount = check_battlefield_for_special_card(player, card, current_turn, 4, &this_test);
		if( amount > 0 ){
			damage_player(current_turn, amount, player, card);
		}
	}

	return global_enchantment(player, card, event);
}

int card_coastal_hornclaw(int player, int card, event_t event){
  /* Coastal Hornclaw |4|U
   * 	  Creature - Bird 3/3
   * 	  Sacrifice a land: Coastal Hornclaw gains flying until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION ){
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								0, 0, KEYWORD_FLYING, 0);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
}

int card_denying_wind(int player, int card, event_t event){
/* CARD_ID_DENYING_WIND	4815
Denying Wind	|7|U|U
Sorcery
Search target player's library for up to seven cards and exile them. Then that player shuffles his or her library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			this_test.no_shuffle = 1;

			int amount = MIN(7, count_deck(instance->targets[0].player));

			while( amount > 0 ){
					scnprintf(this_test.message, 100, "Select a card to exile (%d left)", amount);
					if( new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_MAX_VALUE, &this_test) >= 0 ){
						amount--;
					}
					else{
						break;
					}
			}
			shuffle(instance->targets[0].player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

static int token_generated_by_me(int player, int card, int tok_player, int tok_card){
	if( ! is_token(tok_player, tok_card) ){
		return 0;
	}
	card_instance_t *instance = get_card_instance(tok_player, tok_card);
	if( instance->damage_source_player > -1 ){
		if( instance->damage_source_player == player && instance->damage_source_card == card ){
			return 1;
		}
	}
	return 0;
}

int card_dual_nature(int player, int card, event_t event)
{
  /* Dual Nature	|4|G|G
   * Enchantment
   * Whenever a nontoken creature enters, its controller creates a token that's a copy of that creature.
   * Whenever a nontoken creature leaves the battlefield, exile all tokens with the same name as that creature.
   * When this enchantment leaves the battlefield, exile all tokens created with this enchantment. */

  if (is_humiliated(player, card, AL_TRIG))
    return global_enchantment(player, card, event);
  test_definition_t creature_test;
  default_test_definition(&creature_test, 2);
  creature_test.type_flag = 3;
  etb_ability_global(2, event, 2, TYPE_NONE, &creature_test, player, card);
  if (event == EVENT_ETB)
  {
    token_generation_t token;
    int csvid = get_id(trigger_cause_controller, trigger_cause);
    default_token_definition(player, card, csvid, &token);
    token.t_player = trigger_cause_controller;
    token.legacy = 1;
    token.special_code_for_legacy = empty;
    generate_token(&token);
  }
  int affected_player_snapshot = affected_card_controller;
  int leaving_csvid;
  bool creature_departure = trigger_condition == TRIGGER_LEAVE_PLAY && card == affected_card &&
                            player == affected_card_controller &&
                            reason_for_trigger_controller == player &&
                            (leaving_csvid = get_card_name(trigger_cause_controller, trigger_cause),
                             leaving_csvid != -1) &&
                            is_what(trigger_cause_controller, trigger_cause, 2) &&
                            !is_token(trigger_cause_controller, trigger_cause);
  if (creature_departure && (event == EVENT_TRIGGER || event == EVENT_RESOLVE_TRIGGER))
  {
    if (event == EVENT_TRIGGER)
      event_result |= 2;
    else
    {
      test_definition_t token_test;
      default_test_definition(&token_test, 2);
      token_test.type_flag = F1_IS_TOKEN;
      token_test.id = leaving_csvid;
      new_manipulate_all(player, card, 2, &token_test, ACT_KILL_REMOVE);
    }
    leaves_play(affected_player_snapshot, card, event, 1, 0);
  }
  else
  {
    leaves_play(player, card, event, 1, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      for (int pass = 0; pass < 2; ++pass)
      {
        int token_player = pass == 0 ? player : 1 - player;
        for (int token_card = active_cards_count[token_player] - 1; token_card >= 0; --token_card)
        {
          card_instance_t *instance = in_play(token_player, token_card);
          if (instance && is_what(token_player, token_card, 2) &&
              is_token(token_player, token_card))
          {
            instance = get_card_instance(token_player, token_card);
            if (instance->damage_source_player >= 0 && instance->damage_source_player == player &&
                instance->damage_source_card == card)
              kill_card(token_player, token_card, KILL_EXILE);
          }
        }
      }
    }
  }
  return global_enchantment(player, card, event);
}

int card_entangler(int player, int card, event_t event)
{
  /* Entangler	|2|W|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature can block any number of creatures. */

  attached_creature_can_block_additional(player, card, event, 255);
  if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS)
    return vanilla_aura(player, card, event, player);
  return 0;
}

int card_foil(int player, int card, event_t event){
  /* Foil	|2|U|U
   * Instant
   * You may discard an Island card and another card rather than pay this spell's mana cost.
   * Counter target spell. */

	if( event == EVENT_MODIFY_COST ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select an Island card.");
		this_test.zone = TARGET_ZONE_HAND;
		this_test.subtype = SUBTYPE_ISLAND;	// can't hack in time
		if( hand_count[player] > 1 && check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			null_casting_cost(player, card);
			get_card_instance( player, card )->info_slot = 1;
		}
		else{
			get_card_instance( player, card )->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return counterspell(player, card, event, NULL, 0);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select an Island card.");
		this_test.zone = TARGET_ZONE_HAND;
		this_test.subtype = SUBTYPE_ISLAND;	// can't hack in time
		if( ! played_for_free(player, card) && ! is_token(player, card) && get_card_instance( player, card )->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_id(player, event, get_id(player, card)) ){
				choice = do_dialog(player, player, card, -1, -1, " Discard an Island + another card\n Play normally\n Cancel", 1);
			}

			if( choice == 0 ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
				if( selected > -1 ){
					discard_card(player, selected);
					discard(player, 0, player);
				}
				else{
					spell_fizzled = 1;
				}
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		if( spell_fizzled != 1 ){
			counterspell(player, card, event, NULL, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		return counterspell(player, card, event, NULL, 0);
	}

	return 0;
}

int card_greel_mind_raker(int player, int card, event_t event){
  /* Greel, Mind Raker	|3|B|B
   * Legendary Creature - Horror Spellshaper 3/3
   * |X|B, |T, Discard two cards: Target player discards X cards at random. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && hand_count[player] > 1 ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_B(1), 0, &td, NULL);;
	}

	if(event == EVENT_ACTIVATE ){
		ai_modifier += (((hand_count[1-player]-hand_count[player])-2)*8);
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_XB(-1, 1), 0, &td, "TARGET_PLAYER");
		if( spell_fizzled != 1 ){
			multidiscard(player, 2, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			new_multidiscard(instance->targets[0].player, instance->info_slot, DISC_RANDOM, player);
		}
	}

	return 0;
}

int card_gulf_squid(int player, int card, event_t event){
  /* Gulf Squid	|3|U
   * Creature - Squid Beast 2/2
   * When this creature enters, tap all lands target player controls. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			new_manipulate_all(player, card, instance->targets[0].player, &this_test, ACT_TAP);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_heightened_awareness(int player, int card, event_t event){
  /* Heightened Awareness	|3|U|U
   * Enchantment
   * As this enchantment enters, discard your hand.
   * At the beginning of your draw step, draw an additional card. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (hand_count[player] < 2 ? 48 : -12);
	}

	if( current_turn == player && event == EVENT_DRAW_PHASE && ! is_humiliated(player, card, AL_TRIG) ){
		event_result++;
	}

	if( event == EVENT_RESOLVE_SPELL && ! is_humiliated(player, card, AL_TRIG) ){
		discard_all(player);
	}

	return basic_spell(player, card, event);
}

int card_lesser_gargadon(int player, int card, event_t event){
  /* Lesser Gargadon |2|R|R
   * 	  Creature - Beast 6/4
   * 	  Whenever Lesser Gargadon attacks or blocks, sacrifice a land. */

	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY || (blocking(player, card, event) && !is_humiliated(player, card, AL_TRIG))){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}
	return 0;
}

int card_infernal_genesis(int player, int card, event_t event){
  /* Infernal Genesis	|4|B|B
   * Enchantment
   * At the beginning of each player's upkeep, that player mills a card. Then they create X 1/1 black Minion creature tokens, where X is the milled card's mana value. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int *deck = deck_ptr[current_turn];
		if( deck[0] != -1 ){
			int cmc = get_cmc_by_id(cards_data[deck[0]].id);
			mill(current_turn, 1);

			token_generation_t token;
			default_token_definition(player, card, CARD_ID_MINION, &token);
			token.t_player = current_turn;
			token.pow = token.tou = 1;
			token.qty = cmc;
			generate_token(&token);
		}
	}

	return global_enchantment(player, card, event);
}

int card_jolrael_empress_of_beasts(int player, int card, event_t event){
  /* Jolrael, Empress of Beasts	|3|G|G
   * Legendary Creature - Human Spellshaper 3/3
   * |2|G, |T, Discard two cards: All lands target player controls become 3/3 creatures until end of turn. They're still lands. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_NONE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && hand_count[player] >= 2 ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XG(2, 1), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XG(2, 1), 0, &td, "TARGET_PLAYER");
		if( spell_fizzled != 1 ){
			multidiscard(player, 2, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			land_animation2(instance->parent_controller, instance->parent_card, instance->targets[0].player, -1, 1, 3, 3, 0, 0, 0, &this_test);
		}
	}

	return 0;
}

int card_keldon_firebombers(int player, int card, event_t event){
  /* Keldon Firebombers |3|R|R
   * Creature — Human Soldier 3/3
   * When Keldon Firebombers enters the battlefield, each player sacrifices all lands he or she controls except for three. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int lands_to_sacrifice[2] = {count_subtype(0, TYPE_LAND, -1)-3, count_subtype(1, TYPE_LAND, -1)-3};
		APNAP(p,{
					if( lands_to_sacrifice[p] ){
						test_definition_t test;
						default_test_definition(&test, TYPE_LAND);
						test.qty = lands_to_sacrifice[p];
						scnprintf(test.message, 100, "Select %d land%s to sacrifice.", lands_to_sacrifice[p],
									lands_to_sacrifice[p] > 1 ? "s" : "");
						new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test);
					}
				}
		);
	}

	return 0;
}

int card_latulla_keldon_overseer(int player, int card, event_t event){
	/* Latulla, Keldon Overseer	|3|R|R	0x20087f7
	 * Legendary Creature - Human Spellshaper 3/3
	 * |X|R, |T, Discard two cards: ~ deals X damage to any target. */
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && hand_count[player] >= 2 ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XR(2, 1), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XR(2, 1), 0, &td, "TARGET_ANY");
		if( spell_fizzled != 1 ){
			multidiscard(player, 2, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
	}

	return 0;
}

int card_mageta_the_lion(int player, int card, event_t event){
  /* Mageta the Lion	|3|W|W
   * Legendary Creature - Human Spellshaper 3/3
   * |2|W|W, |T, Discard two cards: Destroy all creatures except for Mageta. Those creatures can't be regenerated. */

	check_legend_rule(player, card, event);

	if( event == EVENT_CAN_ACTIVATE && hand_count[player] >= 2 ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XW(2, 2), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XW(2, 2), 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			multidiscard(player, 2, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		new_manipulate_all(instance->parent_controller, instance->parent_card, ANYBODY, &this_test, KILL_BURY);
	}

	return 0;
}

int card_mungha_wurm(int player, int card, event_t event){
  /* Mungha Wurm	|2|G|G
   * Creature - Wurm 6/5
   * You can't untap more than one land during your untap step. */

	if (current_turn == player){
		untap_only_one_permanent_type(player, card, event, ANYBODY, TYPE_LAND);
	}
	return 0;
}

int card_overburden(int player, int card, event_t event){
  /* Overburden	|1|U
   * Enchantment
   * Whenever a player puts a nontoken creature onto the battlefield, that player returns a land they control to its owner's hand. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.type_flag = F1_NO_TOKEN;

		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.illegal_abilities = 0;
		td.allow_cancel = 0;
		td.allowed_controller = td.preferred_controller = trigger_cause_controller;
		td.who_chooses = trigger_cause_controller;

		card_instance_t *instance = get_card_instance(player, card);

		if( new_pick_target(&td, "Select a land you control to bounce.", 0, GS_LITERAL_PROMPT)  ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

// pigmy razorback -> vanilla

int card_panic_attack(int player, int card, event_t event){
  /* Panic Attack |2|R
   * 	  Sorcery
   * 	  Up to three target creatures can't block this turn. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		int i;
		for(i = 0; i < instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 0,0, 0,SP_KEYWORD_CANNOT_BLOCK);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 3, NULL);
}

int card_plague_wind(int player, int card, event_t event){
  /* Plague Wind |7|B|B
   * 	  Sorcery
   * 	  Destroy all creatures you don't control. They can't be regenerated. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(player, card, 1-player, &this_test, KILL_BURY);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_rethink(int player, int card, event_t event){
	/* Rethink	|2|U
	 * Instant
	 * Counter target spell unless its controller pays |X, where X is its converted mana cost. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, get_cmc(instance->targets[0].player, instance->targets[0].card));
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

int card_reveille_squad(int player, int card, event_t event){
  /* Reveille Squad |2|W|W
   * Creature — Human Rebel 3/3
   * Whenever one or more creatures attack you, if Reveille Squad is untapped, you may untap all creatures you control. */

	if (!is_tapped(player, card)){
		store_attackers(player, card, event, DAT_ATTACKS_PLAYER, 1-player, -1, NULL);
		dat_ability(player, card, event, duh_mode(player) ? DAT_RESOLVE_TRIGGER_MANDATORY : DAT_RESOLVE_TRIGGER_OPTIONAL, 1);
	}
	if (event == EVENT_DAT_ABILITY){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		new_manipulate_all(player, card, player, &test, ACT_UNTAP);
	}
	return 0;
}

int card_rhystic_circle(int player, int card, event_t event){
/* CARD_ID_RHYSTIC_CIRCLE	4872
Rhystic Circle	|2|W|W
Enchantment
|1: Any player may pay |1. If no one does, the next time a source of your choice would deal damage to you this turn, prevent that damage.*/

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.extra = damage_card;
	td1.required_type = 0;
	td1.special = TARGET_SPECIAL_DAMAGE_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			int prevent = 1;
			if( has_mana(1-player, COLOR_ANY, 1) ){
				charge_mana(1-player, COLOR_ANY, 1);
				if( spell_fizzled != 1 ){
					prevent = 0;
				}
			}
			if( prevent ){
				prevent_damage(instance->targets[0].player, instance->targets[0].card, -1);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION | GAA_LITERAL_PROMPT, MANACOST_X(1), 0,
									&td1, "Select target damage card attached to a player.");
}

int card_rhystic_deluge(int player, int card, event_t event){
/* CARD_ID_RHYSTIC_DELUGE	4873
Rhystic Deluge	|2|U
Enchantment
|U: Tap target creature unless its controller pays |1. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			int tap = 1;
			if( has_mana(instance->targets[0].player, COLOR_COLORLESS, 1) ){
				int ai_choice = instance->targets[0].player != player ? 0 : 1;
				int choice = do_dialog(instance->targets[0].player, player, card, -1, -1, " Pay 1\n Pass", ai_choice);
				if( ! choice ){
					charge_mana(instance->targets[0].player, COLOR_COLORLESS, 1);
					if( spell_fizzled != 1 ){
						tap = 0;
					}
				}
			}
			if( tap == 1 ){
				tap_card(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_U(1), 0, &td1, "TARGET_CREATURE");
}

int card_rhystic_study(int player, int card, event_t event)
{
  /*CARD_ID_RHYSTIC_STUDY	4877
Rhystic Study	|2|U
Enchantment
Whenever an opponent casts a spell, you may draw a card unless that player pays |1. */

  sp_ability_trigger(1-player, event, RESOLVE_TRIGGER_AI(player), TYPE_ANY, NULL, player, card);
  if (event == EVENT_SPA)
  {
    charge_mana(1-player, COLOR_ANY, 1);
    if (spell_fizzled == 1)
      draw_cards(player, 1);
  }
  return global_enchantment(player, card, event);
}

int card_rhystic_tutor(int player, int card, event_t event){
/* CARD_ID_RHYSTIC_TUTOR	4879
Rhystic Tutor	|2|B
Sorcery
Unless any player pays |2, search your library for a card, put that card into your hand, then shuffle your library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier +=(has_mana(1-player, COLOR_COLORLESS, 2) ? -96 : 96);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		charge_mana(1-player, COLOR_COLORLESS, 2);
		if( spell_fizzled == 1 ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card.");
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_searing_wind(int player, int card, event_t event){
	/* Searing Wind	|8|R	0x2008824
	 * Instant
	 * ~ deals 10 damage to any target. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 10);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

static int shield_dancer_effect(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *source = get_card_instance(affected_card_controller, affected_card);
			if( damage_card == source->internal_card_id && source->info_slot > 0 ){
				if( source->damage_target_player == instance->targets[1].player &&
					source->damage_target_card == instance->targets[1].card &&
					source->damage_source_player == p &&
					source->damage_source_card == c )
				{
					source->damage_target_player = p;
					source->damage_target_card = c;
					kill_card(player, card, KILL_EXILE);
				}
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_shield_dancer(int player, int card, event_t event){
  /* Shield Dancer |2|W
   * Creature — Human Rebel 1/3
   * {2}{W}: The next time target attacking creature would deal combat damage to Shield Dancer this turn, that creature deals that damage
   * 	to itself instead. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XW(2, 1), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XW(2, 1), 0, &td, "Select target attacking creature.");
		if( spell_fizzled != 1 ){
			if( instance->targets[0].player == 1-player && instance->targets[0].card == instance->blocking ){
				ai_modifier += 48;
			}
			else{
				ai_modifier -= 48;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int legacy = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &shield_dancer_effect,
														instance->targets[0].player, instance->targets[0].card);
			card_instance_t *trg = get_card_instance(player, legacy);
			trg->targets[1].player = instance->parent_controller;
			trg->targets[1].card = instance->parent_card;
			trg->number_of_targets = 2;
		}
	}

	return 0;
}

int card_snag(int player, int card, event_t event){
  /* Snag	|3|G
   * Instant
   * You may discard a Forest card rather than pay this spell's mana cost.
   * Prevent all combat damage that would be dealt by unblocked creatures this turn. */

	if( event == EVENT_MODIFY_COST ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a Forest card.");
		this_test.zone = TARGET_ZONE_HAND;
		this_test.subtype = SUBTYPE_FOREST;	// can't hack in time

		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			null_casting_cost(player, card);
			get_card_instance( player, card )->info_slot = 1;
		}
		else{
			get_card_instance( player, card )->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && get_card_instance( player, card )->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_id(player, event, get_id(player, card)) ){
				choice = do_dialog(player, player, card, -1, -1, " Discard a Forest\n Play normally\n Cancel", 1);
			}

			if( choice == 0 ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_LAND, "Select a Forest card.");
				this_test.zone = TARGET_ZONE_HAND;
				this_test.subtype = SUBTYPE_FOREST;	// can't hack in time

				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
				if( selected > -1 ){
					discard_card(player, selected);
				}
				else{
					spell_fizzled = 1;
				}
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		fog_special(player, card, ANYBODY, FOG_COMBAT_DAMAGE_ONLY | FOG_UNBLOCKED_CREATURES_ONLY);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_spiketail_drake2(int player, int card, event_t event){
	/* Spiketail Drake	|3|U|U
	 * Creature - Drake 3/3
	 * Flying
	 * Sacrifice ~: Counter target spell unless its controller pays |3. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		counterspell_resolve_unless_pay_x(player, card, &td, 0, 3);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_SPELL_ON_STACK, MANACOST0, 0, &td, NULL);
}

int card_spiketail_hatchling(int player, int card, event_t event){
	/* Spiketail Hatchling	|1|U
	 * Creature - Drake 1/1
	 * Flying
	 * Sacrifice ~: Counter target spell unless its controller pays |1. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		counterspell_resolve_unless_pay_x(player, card, &td, 0, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_SPELL_ON_STACK, MANACOST0, 0, &td, NULL);
}

int card_spitting_spider(int player, int card, event_t event){
  /* Spitting Spider English |3|G|G
   * 	  Creature - Spider 3/5
   * 	  Reach (This creature can block creatures with flying.)
   * 	  Sacrifice a land: Spitting Spider deals 1 damage to each creature with flying. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t test2;
		default_test_definition(&test2, TYPE_CREATURE);
		test2.keyword = KEYWORD_FLYING;
		new_damage_all(instance->parent_controller, instance->parent_card, ANYBODY, 1, 0, &test2);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
}

int card_spore_frog(int player, int card, event_t event){
  /* Spore Frog	|G
   * Creature - Frog 1/1
   * Sacrifice this creature: Prevent all combat damage that would be dealt this turn. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		fog_effect(player, card);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_squirrel_wrangler(int player, int card, event_t event){
	/* Squirrel Wrangler	|2|G|G
	 * Creature - Human Druid 2/2
	 * |1|G, Sacrifice a land: Put two 1/1 |Sgreen Squirrel creature tokens onto the battlefield.
	 * |1|G, Sacrifice a land: Squirrel creatures get +1/+1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_XG(1, 1), 0, NULL, NULL, 0, &test, 0);
	}

	if(event == EVENT_ACTIVATE ){
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Generate 2 Squirrels", 1, 5,
							"Pump your Squirrels", count_subtype(player, TYPE_CREATURE, SUBTYPE_SQUIRREL)*2, 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_XG(1, 1), 0, NULL, NULL, 0, &test, 0);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			generate_tokens_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SQUIRREL, 2);
		}
		if( instance->info_slot == 2 ){
			pump_subtype_until_eot(instance->parent_controller, instance->parent_card, 2, SUBTYPE_SQUIRREL, 1, 1, 0, 0);
		}
	}

	return 0;
}

int card_sword_dancer(int player, int card, event_t event){
  /* Sword Dancer |1|W
   * 	  Creature - Human Rebel 1/2
   * 	  {W}{W}: Target attacking creature gets -1/-0 until end of turn. */

	if (!IS_GAA_EVENT(event)  ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, -1, 0);
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_W(2), 0,
									&td, "Select target attacking creature.");
}

int card_thrive(int player, int card, event_t event)
{
  /* Thrive	|X|G
   * Sorcery
   * Put a +1/+1 counter on each of X target creatures. */

  int i;
  card_instance_t *instance;
  int mana_or_valid;
  int eligible_or_selected;
  target_t *target_cursor;
  target_t *next_target_cursor;
  int cost_or_index;
  int target_limit;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allow_cancel = 2;
    get_card_instance(player, card);
    if (can_target(&td) != 0)
    {
      return generic_spell(player, card, EVENT_CAN_CAST, 4, NULL, NULL, 0, NULL);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      td.allow_cancel = 2;
      instance = get_card_instance(player, card);
      if (((card == affected_card) && (player == affected_card_controller)) &&
          (((cost_or_index = played_for_free(player, card),
             cost_or_index == 0 && (cost_or_index = is_token(player, card), cost_or_index == 0)) ||
            (cost_or_index = check_special_flags2(player, card, -0x80000000), cost_or_index != 0))))
      {
        cost_or_index = true_get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, -1);
        mana_or_valid = has_mana(player, 7, 1);
        eligible_or_selected = check_special_flags2(player, card, -0x80000000);
        if (eligible_or_selected == 0)
        {
          target_limit = mana_or_valid - cost_or_index;
        }
        else
        {
          target_limit = instance->info_slot;
        }
        i = 0;
        while (((eligible_or_selected = can_target(&td),
                 i < target_limit && (eligible_or_selected != 0)) &&
                (eligible_or_selected = new_pick_target(&td, "TARGET_CREATURE", i, 0),
                 eligible_or_selected != 0)))
        {
          next_target_cursor = instance->targets + i;
          target_cursor = instance->targets + i;
          i = i + 1;
          state_untargettable(target_cursor->player, next_target_cursor->card, 1);
        }
        if (0 < i)
        {
          target_cursor = instance->targets;
          do
          {
            next_target_cursor = target_cursor + 1;
            state_untargettable(target_cursor->player, target_cursor->card, 0);
            target_cursor = next_target_cursor;
          } while (next_target_cursor != instance->targets + i);
        }
        eligible_or_selected = played_for_free(player, card);
        if (((eligible_or_selected == 0) &&
             (eligible_or_selected = is_token(player, card), eligible_or_selected == 0)) &&
            (charge_mana(player, COLOR_COLORLESS, cost_or_index + i), spell_fizzled != 1))
        {
          instance->info_slot = i;
        }
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allow_cancel = 2;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (0 < instance->info_slot)
      {
        cost_or_index = 0;
        do
        {
          if (mana_or_valid = validate_target(player, card, &td, cost_or_index), mana_or_valid == 0)
          {
            cost_or_index = cost_or_index + 1;
            continue;
          }
          next_target_cursor = instance->targets + cost_or_index;
          target_cursor = instance->targets + cost_or_index;
          cost_or_index = cost_or_index + 1;
          add_counters(player, card, target_cursor->player, next_target_cursor->card, COUNTER_P1_P1,
                       1);
        } while (cost_or_index < instance->info_slot);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

int card_troublesome_spirit(int player, int card, event_t event){
  /* Troublesome Spirit	|2|U|U
   * Creature - Spirit 3/4
   * Flying
   * At the beginning of your end step, tap all lands you control. */

	if( current_turn == player && eot_trigger(player, card, event) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		new_manipulate_all(player, card, player, &this_test, ACT_TAP);
	}

	return 0;
}

// veteran brawlers --> branded brawlers

int card_vitalizing_wind(int player, int card, event_t event){
  /* Vitalizing Wind	|8|G
   * Instant
   * Creatures you control get +7/+7 until end of turn. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1) * 16);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 7, 7, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_well_of_discovery(int player, int card, event_t event){
  /* Well of Discovery	|6
   * Artifact
   * At the beginning of your end step, if you control no untapped lands, draw a card. */

	if(current_turn == player && trigger_condition == TRIGGER_EOT &&  affect_me(player, card ) && reason_for_trigger_controller == player){
		if( ! control_an_untapped_land(player) && ! is_humiliated(player, card, AL_TRIG) ){
			if(event == EVENT_TRIGGER){
				event_result |= 2;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					draw_cards(player, 1);
			}
		}
	}
	return 0;
}

int card_well_of_life(int player, int card, event_t event){
  /* Well of Life	|4
   * Artifact
   * At the beginning of your end step, if you control no untapped lands, you gain 2 life. */

	if( current_turn == player && trigger_condition == TRIGGER_EOT &&  affect_me(player, card ) && reason_for_trigger_controller == player){
		if( ! control_an_untapped_land(player) && ! is_humiliated(player, card, AL_TRIG) ){
			if(event == EVENT_TRIGGER){
				event_result |= 2;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					gain_life(player, 2);
			}
		}
	}

	return 0;
}

int card_whip_sergeant(int player, int card, event_t event){
  /* Whip Sergeant	|2|R
   * Creature - Human Soldier 2/1
   * |R: Target creature gains haste until end of turn. (It can attack this turn.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			ai_modifier_for_haste_granting_effects(player, card);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player,  instance->targets[0].card, 0, 0, 0, SP_KEYWORD_HASTE);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_CREATURE");
}

int card_wild_might(int player, int card, event_t event){
  /* Wild Might	|1|G
   * Instant
   * Target creature gets +1/+1 until end of turn. That creature gets an additional +4/+4 until end of turn unless any player pays |2. */

	if(event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player,  instance->targets[0].card, 1, 1);
			charge_mana(1-instance->targets[0].player, COLOR_COLORLESS, 2);
			if( spell_fizzled == 1 ){
				pump_until_eot_merge_previous(player, card, instance->targets[0].player,  instance->targets[0].card, 4, 4);
			}
		}
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}
	int pump = ! has_mana(1-player, COLOR_ANY, 1) ? 5 : 1;
	return vanilla_instant_pump(player, card, event, ANYBODY, player, pump, pump, 0, 0);
}

int card_withdraw(int player, int card, event_t event){
  /* Withdraw	|U|U
   * Instant
   * Return target creature to its owner's hand. Then return another target creature to its owner's hand unless its controller pays |1. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			bounce_permanent(instance->targets[0].player,  instance->targets[0].card);
		}
		if( validate_target(player, card, &td, 1) ){
			charge_mana(instance->targets[1].player, COLOR_ANY, 1);
			if( spell_fizzled == 1 ){
				bounce_permanent(instance->targets[1].player,  instance->targets[1].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

/* ManalinkEh 2022: 0x26005c0; original cards/prophecy.c:1692. */
int card_living_terrain(int player, int card, event_t event)
{
  /* Living Terrain	|2|G|G
   * Enchantment - Aura
   * Enchant land
   * Enchanted land is a 5/6 green Treefolk creature that's still a land. */

  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 1);
  td.preferred_controller = player;
  attached_is_additional_subtype(player, card, event, SUBTYPE_TREEFOLK);
  attached_is_additional_color(player, card, event, COLOR_TEST_GREEN);
  result = generic_animating_aura(player, card, event, &td, "TARGET_LAND", 5, 6, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x2600670; original cards/prophecy.c:1709. */
int card_agent_of_shauku(int player, int card, event_t event)
{
  /* Agent of Shauku	|1|B
   * Creature - Human Mercenary 1/1
   * |1|B, Sacrifice a land: Target creature gets +2/+0 until end of turn. */

  card_instance_t *instance;
  unsigned int chosen_sacrifice;
  int result;
  unsigned int player_00;
  target_definition_t td;
  test_definition_t test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = can_sacrifice_type_as_cost(player, 1, TYPE_LAND);
      if (result == 0)
      {
        return 0;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 1, 0, 0, 0, 0, 0,
                                       &td, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      new_default_test_definition(&test, 1, "Select a land to sacrifice.");
      chosen_sacrifice = new_sacrifice(player, card, player,
                                       SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
      if (chosen_sacrifice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      generic_activated_ability(player, card, EVENT_ACTIVATE, 4, 1, 1, 0, 0, 0, 0, 0, &td,
                                "TARGET_CREATURE");
      player_00 = (int)chosen_sacrifice >> 0x10 & 0xff;
      if (spell_fizzled != 1)
      {
        kill_card(player_00, chosen_sacrifice >> 0x18, KILL_SACRIFICE);
        if (current_phase != 0x17)
        {
          return 0;
        }
        ai_modifier = ai_modifier + 0x18;
        return 0;
      }
      state_untargettable(player_00, chosen_sacrifice >> 0x18, 0);
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
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                           instance->targets[0].player, instance->targets[0].card, 2, 0, 0, 0);
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2600950; original cards/prophecy.c:1760. */
int card_barbed_field(int player, int card, event_t event)
{
  /* Barbed Field	|2|R|R
   * Enchantment - Aura
   * Enchant land
   * Enchanted land has "|T: This land deals 1 damage to any target." */

  int attached_card;
  card_instance_t *instance;
  int flags_or_valid;
  int p;
  event_t event_00;
  target_definition_t *td_00;
  target_definition_t td;

  if (is_humiliated(player, card, AL_STATIC) == 0)
  {
    instance = get_card_instance(player, card);
    td_00 = &td;
    if (-1 < instance->damage_target_player)
    {

      instance = get_card_instance(player, card);
      p = (int)instance->damage_target_player;
      attached_card = instance->damage_target_card;
      default_target_definition(p, attached_card, td_00, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td.preferred_controller = 1 - player;
      if (event == EVENT_CAN_ACTIVATE)
      {
        flags_or_valid = 5;
        event_00 = EVENT_CAN_ACTIVATE;
        return granted_generic_activated_ability(player, card, p, attached_card, event_00,
                                                 flags_or_valid, 0, 0, 0, 0, 0, 0, 0, td_00, NULL);
      }
      if (event == EVENT_ACTIVATE)
      {
        instance->number_of_targets = '\0';
        if (pick_target(td_00, "TARGET_ANY") != 0)
        {
          td_00 = NULL;
          flags_or_valid = 1;
          event_00 = EVENT_ACTIVATE;
          return granted_generic_activated_ability(player, card, p, attached_card, event_00,
                                                   flags_or_valid, 0, 0, 0, 0, 0, 0, 0, td_00,
                                                   NULL);
        }
      }
      else if ((event == EVENT_RESOLVE_ACTIVATION) &&
               (flags_or_valid = validate_target(p, attached_card, td_00, 0),
                flags_or_valid != 0))
      {

        instance = get_card_instance(p, attached_card);
        damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
      }
    }
  }
  default_target_definition(player, card, &td, 1);
  td.preferred_controller = player;
  return aura_literal_prompt(player, card, event, &td, "Select target land.");
}
#endif

/* ManalinkEh 2022: 0x2600ba0; original cards/prophecy.c:1803. */
int card_bog_elemental(int player, int card, event_t event)
{
  /* Bog Elemental	|3|B|B
   * Creature - Elemental 5/4
   * Protection from white
   * At the beginning of your upkeep, sacrifice this creature unless you sacrifice a land. */

  int result;
  test_definition_t test2;
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    new_default_test_definition(&test2, 1, "Select a land to sacrifice.");
    result = new_sacrifice(player, card, player, 0, &test2);
    if (result == 0)
    {
      kill_card(player, card, KILL_SACRIFICE);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2600c40; original cards/prophecy.c:1827. */
int card_bog_glider(int player, int card, event_t event)
{
  /* Bog Glider	|2|B
   * Creature - Human Mercenary 1/1
   * Flying
   * |T, Sacrifice a land: Search your library for a Mercenary permanent card with mana value 2 or less, put it onto the battlefield, then shuffle. */

  int result;
  test_definition_t test;

  if (event == EVENT_ACTIVATE)
  {
    new_default_test_definition(&test, 0x47, "Select a Mercenary card");
    test.subtype = 0x73;
    test.cmc = 3;
    test.cmc_flag = 3;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    new_default_test_definition(&test, 0x47, "Select a Mercenary card");
    test.subtype = 0x73;
    test.cmc = 3;
    test.cmc_flag = 3;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      new_global_tutor(player, player, 1, 4, 0, 5, &test);
    }
  }
  result = generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_UNTAPPED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_LAND, NULL, SARV_0);
  return result;
}

/* ManalinkEh 2022: 0x2600da0; original cards/prophecy.c:1854. */
int card_celestial_convergence(int player, int card, event_t event)
{
  /* Celestial Convergence	|2|W|W
   * Enchantment
   * This enchantment enters with seven omen counters on it.
   * At the beginning of your upkeep, remove an omen counter from this enchantment. If there are no omen counters on this enchantment, the player with the highest life total wins the game. If two or more players are tied for highest life total, the game is a draw. */

  int controller_life;
  int value;
  card_instance_t *instance;

  enters_the_battlefield_with_counters(player, card, event, COUNTER_OMEN, 7);
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    remove_counters(player, card, player, card, COUNTER_OMEN, 1);
  }
  else if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {

      instance = get_card_instance(player, card);
      value = count_counters(player, card, COUNTER_OMEN);
      if (value < 1)
      {
        if (instance->info_slot == 0x12c7)
        {
          value = is_humiliated(player, card, AL_STATIC);
          if (value == 0)
          {
            value = life[1 - player];
            controller_life = life[player];
            if (value < controller_life)
            {
              lose_the_game(1 - player);
            }
            else if (controller_life < value)
            {
              lose_the_game(player);
            }
            else if (controller_life == value)
            {
              lose_the_game(2);
            }
          }
          else
          {
            instance->info_slot = 0;
          }
        }
      }
      else
      {
        instance->info_slot = 0x12c7;
      }
    }
  }
  else if ((event == EVENT_SHOULD_AI_PLAY) &&
           (((ai_is_speculating == 1 ||
              ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
             (life[0] < life[1]))))
  {
    ai_modifier = ai_modifier + 0x18;
  }
  value = global_enchantment(player, card, event);
  return value;
}

/* ManalinkEh 2022: 0x2600f60; original cards/prophecy.c:1897. */
int card_chilling_apparition(int player, int card, event_t event)
{
  /* Chilling Apparition	|2|B
   * Creature - Spirit 1/1
   * |B: Regenerate this creature.
   * Whenever this creature deals combat damage to a player, that player discards a card. */

  int result;
  whenever_i_deal_damage_to_a_player_he_discards_a_card(player, card, event,
                                                        DDBM_MUST_BE_COMBAT_DAMAGE, 0);
  result = regeneration(player, card, event, 0, 1, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x2600fe0; original cards/prophecy.c:1908. */
int card_coffin_puppets(int player, int card, event_t event)
{
  /* Coffin Puppets	|3|B|B
   * Creature - Zombie 3/3
   * Sacrifice two lands: Return this card from your graveyard to the battlefield. Activate only during your upkeep and only if you control a Swamp. */

  test_definition_t test_food;
  new_default_test_definition(&test_food, TYPE_LAND, "Select a land to sacrifice");
  test_food.qty = 2;
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    if (check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_SWAMP) &&
        generic_activated_ability_with_sacrifice_as_cost(
            player, card, EVENT_CAN_ACTIVATE, GAA_IN_YOUR_TURN | GAA_ONLY_ON_UPKEEP,
            MANACOST0, 0, NULL, NULL, TYPE_NONE, &test_food, SARV_0))
      return GA_RETURN_TO_PLAY;
  }
  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    generic_activated_ability_with_sacrifice_as_cost(
        player, card, EVENT_ACTIVATE, GAA_IN_YOUR_TURN, MANACOST0, 0, NULL, NULL,
        TYPE_NONE, &test_food, SARV_0);
    return spell_fizzled != 1;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2601180; original cards/prophecy.c:1935. */
int card_copper_leaf_angel(int player, int card, event_t event)
{
  /* Copper-Leaf Angel	|5
   * Artifact Creature - Angel 2/2
   * Flying
   * |T, Sacrifice X lands: Put X +1/+1 counters on this creature. */

  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  test_definition_t test;
  new_default_test_definition(&test, 1, "");
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (!new_can_sacrifice_as_cost(player, card, &test))
      return 0;
    return generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  }
  if (event == EVENT_ACTIVATE)
  {
    bool speculating = ai_is_speculating == 1;
    instance->info_slot = 0;
    int sacrifice_limit = 151;
    if (speculating || (player == 1 && ((unsigned char)trace_mode & 2) == 0))
      sacrifice_limit = internal_rand(10) + 3;
    int devoured[151];
    int selected_count = 0;
    while (new_can_sacrifice_as_cost(player, card, &test))
    {
      unsigned int choice = new_sacrifice(player, card, player,
                                          SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
      if (!choice)
        break;
      devoured[selected_count++] = choice >> 24;
      if (selected_count == sacrifice_limit)
        break;
    }
    generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
    for (int index = selected_count - 1; index >= 0; --index)
    {
      if (spell_fizzled == 1)
        state_untargettable(player, devoured[index], 0);
      else
      {
        kill_card(player, devoured[index], KILL_SACRIFICE);
        instance->info_slot = (int32_t)((uint32_t)instance->info_slot + 1U);
      }
    }
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
    add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1,
                 instance->info_slot);
  return 0;
}

/* ManalinkEh 2022: 0x25fb320; original cards/prophecy.c:1995. */
int lose_2_life_unless_pay_2(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  if (event == EVENT_RESOLVE_TRIGGER)
  {

    instance = get_card_instance(player, card);
    result = has_mana((int)instance->damage_target_player, 7, 2);
    if ((result == 0) ||
        (result = charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER,
                                              (int)instance->damage_target_player, COLOR_ANY, 2),
         result == 0))
    {
      lose_life((int)instance->damage_target_player, 2);
    }
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x25f8da0; original cards/prophecy.c:2017. */
void death_charmer_effect(intptr_t player, intptr_t card, int t_player, int t_card)
{
  create_targetted_legacy_effect(player, card, lose_2_life_unless_pay_2, t_player, t_card);
  return;
}

/* ManalinkEh 2022: 0x2601490; original cards/prophecy.c:2021. */
int card_death_charmer(int player, int card, event_t event)
{
  /* Death Charmer	|2|B
   * Creature - Worm Mercenary 2/2
   * Whenever this creature deals combat damage to a creature, that creature's controller loses 2 life unless they pay |2. */

  if (is_humiliated(player, card, AL_TRIG))
  {
    return 0;
  }
  for_each_creature_damaged_by_me(player, card, event, 4, death_charmer_effect, player, card);
  return 0;
}

/* ManalinkEh 2022: 0x2601500; original cards/prophecy.c:2033. */
int card_despoil(int player, int card, event_t event)
{
  /* Despoil	|3|B
   * Sorcery
   * Destroy target land. Its controller loses 2 life. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 1);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (validate_target(player, card, &td, 0))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      lose_life(instance->targets[0].player, 2);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_LAND", 1, NULL);
}

/* ManalinkEh 2022: 0x2601670; original cards/prophecy.c:2058. */
int card_devastate(int player, int card, event_t event)
{
  /* Devastate	|3|R|R
   * Sorcery
   * Destroy target land. ~ deals 1 damage to each creature and each player. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 1);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (validate_target(player, card, &td, 0))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      new_damage_all(player, card, 2, 1, 3, NULL);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_LAND", 1, NULL);
}

/* ManalinkEh 2022: 0x26017f0; original cards/prophecy.c:2083. */
int card_elephant_token(int player, int card, event_t event)
{
  if (card == -1)
    return 0;
  if ((uint32_t)(event - EVENT_POWER) < 2 && card == affected_card &&
      player == affected_card_controller && get_special_infos(player, card) == 0x42)
    set_characteristic_pt(count_graveyard_by_type(player, TYPE_CREATURE));
  return generic_token(player, card, event);
}

/* ManalinkEh 2022: 0x2601880; original cards/prophecy.c:2102. */
int card_elephant_resurgence(int player, int card, event_t event)
{
  /* Elephant Resurgence	|1|G
   * Sorcery
   * Each player creates a green Elephant creature token. Those creatures have "This token's power and toughness are each equal to the number of creature cards in its controller's graveyard." */

  unsigned int other_player;
  int result;
  token_generation_t token;
  result = current_turn;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_token_definition(player, card, CARD_ID_ELEPHANT, &token);
    token.t_player = result;
    token.special_infos = 0x42;
    token.tou = 0x4000;
    token.pow = 0x4000;
    generate_token(&token);
    other_player = current_turn ^ 1;
    default_token_definition(player, card, CARD_ID_ELEPHANT, &token);
    token.special_infos = 0x42;
    token.tou = 0x4000;
    token.pow = 0x4000;
    token.t_player = other_player;
    generate_token(&token);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x25f9e40; original cards/prophecy.c:2124. */
static int endbringers_revel_legacy(int player, int card, event_t event)
{
  int p;
  int granting_card;
  card_instance_t *instance;
  int source_player_or_grave_index;
  test_definition_t *grave_test;
  test_definition_t test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_player_or_grave_index = get_id(p, instance->targets[0].card);
        if (source_player_or_grave_index != 0x12d4)
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
      source_player_or_grave_index = instance->targets[0].player;
      granting_card = instance->targets[0].card;
      if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
      {
        new_default_test_definition(&test, 2, "Select target creature card.");
        if (event == EVENT_CAN_ACTIVATE)
        {
          if (any_in_graveyard_by_type(2, 2) == 0)
          {
            return 0;
          }
          if (graveyard_has_shroud(2) != 0)
          {
            return 0;
          }
          if (granted_generic_activated_ability(source_player_or_grave_index, granting_card, player,
                                                card, EVENT_CAN_ACTIVATE, 0x10, 4, 0, 0, 0, 0, 0, 0,
                                                NULL, NULL) == 0)
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
        new_default_test_definition(&test, 2, "Select target creature card.");
      }
      grave_test = &test;
      if (event == EVENT_ACTIVATE)
      {
        granted_generic_activated_ability(source_player_or_grave_index, granting_card, player, card,
                                          EVENT_ACTIVATE, 0, 4, 0, 0, 0, 0, 0, 0, NULL, NULL);
        if (spell_fizzled != 1)
        {
          select_target_from_either_grave(player, card, SFG_0, 2, 2, grave_test, 1, 2);
        }
      }
      else
      {

        if ((event == EVENT_RESOLVE_ACTIVATION) &&
            (source_player_or_grave_index =
                 validate_target_from_grave_source(player, card, instance->targets[1].player, 2),
             source_player_or_grave_index != -1))
        {
          from_grave_to_hand(instance->targets[1].player, source_player_or_grave_index, 1);
        }
      }
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x2601960; original cards/prophecy.c:2177. */
int card_endbringers_revel(int player, int card, event_t event)
{
  /* Endbringer's Revel	|2|B
   * Enchantment
   * |4: Return target creature card from a graveyard to its owner's hand. Any player may activate this ability but only as a sorcery. */

  int p;
  bool legacy_found;
  card_instance_t *instance;
  int grave_index;
  card_instance_t *legacy_record;
  int card_00;
  test_definition_t test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {

      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      do
      {
        card_00 = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, card_00);

            if (((legacy_record != NULL) && (is_what(p, card_00, 0x80) != 0)) &&
                ((legacy_record = get_card_instance(p, card_00),
                  legacy_record->info_slot_fn == endbringers_revel_legacy &&
                      ((legacy_record->targets[0].player == player &&
                        (legacy_record->targets[0].card == card))))))
            {
              if (player == p)
              {
                kill_card(player, card_00, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                  return global_enchantment(player, card, event);
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
        create_legacy_activate_for_opponent(player, card, endbringers_revel_legacy, player, card);
      }
    }
  }
  else if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
            (event == EVENT_RESOLVE_ACTIVATION)) ||
           (event == EVENT_CLEANUP))
  {
    new_default_test_definition(&test, 2, "Select target creature card.");
    instance = get_card_instance(player, card);

    if ((event == EVENT_RESOLVE_ACTIVATION) && (grave_index = validate_target_from_grave_source(
                                                    player, card, instance->targets[0].player, 1),
                                                grave_index != -1))
    {
      from_grave_to_hand(instance->targets[0].player, grave_index, 1);
    }
    return generic_activated_ability_with_target_in_grave(
        player, card, event, GAA_CAN_SORCERY_BE_PLAYED, 4, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE,
        &test, 0x1000);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25fa1c0; original cards/prophecy.c:2232. */
static int excavation_legacy(int player, int card, event_t event)
{
  int p;
  int granting_card;
  card_instance_t *instance;
  int can_sacrifice;
  int source_player_or_available;
  unsigned int chosen_sacrifice;
  unsigned int player_00;
  test_definition_t test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_player_or_available = get_id(p, instance->targets[0].card);
        if (source_player_or_available != 0x12d6)
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
      source_player_or_available = instance->targets[0].player;
      granting_card = instance->targets[0].card;
      if (event == EVENT_CAN_ACTIVATE)
      {
        can_sacrifice = can_sacrifice_type_as_cost(player, 1, TYPE_LAND);
        if ((can_sacrifice != 0) && (source_player_or_available = granted_generic_activated_ability(
                                         source_player_or_available, granting_card, player, card,
                                         EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0, NULL, NULL),
                                     source_player_or_available != 0))
        {
          return 1;
        }
      }
      else if (event == EVENT_ACTIVATE)
      {

        new_default_test_definition(&test, 1, "Select a land to sacrifice.");
        chosen_sacrifice = new_sacrifice(source_player_or_available, granting_card, player,
                                         SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
        if (chosen_sacrifice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        granted_generic_activated_ability(source_player_or_available, granting_card, player, card,
                                          EVENT_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
        player_00 = (int)chosen_sacrifice >> 0x10 & 0xff;
        if (spell_fizzled == 1)
        {
          state_untargettable(player_00, chosen_sacrifice >> 0x18, 0);
        }
        else
        {
          kill_card(player_00, chosen_sacrifice >> 0x18, KILL_SACRIFICE);
        }
      }
      else if (event == EVENT_RESOLVE_ACTIVATION)
      {
        draw_a_card(player);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2601c40; original cards/prophecy.c:2287. */
int card_excavation(int player, int card, event_t event)
{
  /* Excavation	|1|U
   * Enchantment
   * |1, Sacrifice a land: Draw a card. Any player may activate this ability. */

  int c;
  int p;
  bool legacy_found;
  card_instance_t *instance;
  int can_sacrifice;
  unsigned int chosen_sacrifice;
  card_instance_t *legacy_record;
  int activation_result;
  unsigned int player_00;
  test_definition_t test;

  if (event != EVENT_STATIC_EFFECTS)
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      can_sacrifice = can_sacrifice_type_as_cost(player, 1, TYPE_LAND);
      activation_result = 0;
      if (can_sacrifice != 0)
      {
        activation_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0,
                                                      0, 0, 0, 0, NULL, NULL);
      }
    }
    else if (event == EVENT_ACTIVATE)
    {

      new_default_test_definition(&test, 1, "Select a land to sacrifice.");
      chosen_sacrifice = new_sacrifice(player, card, player,
                                       SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
      if (chosen_sacrifice == 0)
      {
        spell_fizzled = 1;
        activation_result = 0;
      }
      else
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
        player_00 = (int)chosen_sacrifice >> 0x10 & 0xff;
        if (spell_fizzled == 1)
        {
          activation_result = 0;
          state_untargettable(player_00, chosen_sacrifice >> 0x18, 0);
        }
        else
        {
          activation_result = 0;
          kill_card(player_00, chosen_sacrifice >> 0x18, KILL_SACRIFICE);
        }
      }
    }
    else
    {
      if (event == EVENT_RESOLVE_ACTIVATION)
      {
        draw_a_card(player);
        return 0;
      }
      if (event != EVENT_CLEANUP)
        return global_enchantment(player, card, event);
      activation_result = 0;
    }
    return activation_result;
  }
  instance = in_play(player, card);
  if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
  {

    p = 0;
    instance = get_card_instance(player, card);
    legacy_found = false;
    do
    {
      c = 0;
      if (-1 < active_cards_count[p])
      {
        do
        {
          legacy_record = in_play(p, c);

          if (((legacy_record != NULL) && (is_what(p, c, 0x80) != 0)) &&
              ((legacy_record = get_card_instance(p, c),
                legacy_record->info_slot_fn == excavation_legacy &&
                    ((legacy_record->targets[0].player == player &&
                      (legacy_record->targets[0].card == card))))))
          {
            if (player == p)
            {
              kill_card(player, c, KILL_EXILE);
            }
            else
            {
              if (p == 1)
                return global_enchantment(player, card, event);
              legacy_found = true;
            }
          }
          c = c + 1;
        } while (c <= active_cards_count[p]);
      }
      p = p + 1;
    } while (p != 2);
    if ((!legacy_found) && (instance->targets[9].card != 0x42))
    {
      create_legacy_activate_for_opponent(player, card, excavation_legacy, player, card);
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2601ff0; original cards/prophecy.c:2357. */
int card_excise(int player, int card, event_t event)
{
  /* Excise	|X|W
   * Instant
   * Exile target attacking creature unless its controller pays |X. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.required_state = TARGET_STATE_ATTACKING;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if ((result != 0) &&
        ((result = has_mana(instance->targets[0].player, 7, instance->info_slot),
          result == 0 || (result = charge_mana_while_resolving(
                              player, card, EVENT_RESOLVE_SPELL, instance->targets[0].player,
                              COLOR_ANY, instance->info_slot),
                          result == 0))))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x85, &td, "Select target attacking creature.", 1, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26021a0; original cards/prophecy.c:2394. */
int card_fault_riders(int player, int card, event_t event)
{
  /* Fault Riders	|2|R
   * Creature - Human Soldier 2/2
   * Sacrifice a land: This creature gets +2/+0 and gains first strike until end of turn. Activate only once each turn. */

  card_instance_t *instance;
  int result;
  test_definition_t test;

  if (event == EVENT_CHECK_ABILITIES)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = card_fault_riders(player, card, EVENT_CAN_ACTIVATE), result != 0)) &&
        (result = is_humiliated(player, card, AL_STATIC), result == 0))
    {
      check_abilities_keywords = check_abilities_keywords | 0x100;
      check_abilities_power = check_abilities_power + 2;
    }
  }
  else if (((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
           ((event == EVENT_RESOLVE_ACTIVATION || (event == EVENT_CLEANUP))))
  {
    new_default_test_definition(&test, 1, "Select a land to sacrifice.");
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                                  instance->parent_controller, instance->parent_card, 2, 0, 0x100,
                                  0);
    }
    result = generic_activated_ability_with_sacrifice_as_cost(
        player, card, event, GAA_ONCE_PER_TURN, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test,
        SARV_0);
    return result;
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x2602350; original cards/prophecy.c:2422. */
int card_fen_stalker(int player, int card, event_t event)
{
  /* Fen Stalker	|3|B
   * Creature - Nightstalker 3/2
   * This creature has fear as long as you control no untapped lands. (It can't be blocked except by artifact creatures and/or black creatures.) */

  int result;

  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = control_an_untapped_land(player);
    if (result == 0)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        fear(player, card, EVENT_ABILITIES);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x26023c0; original cards/prophecy.c:2434. */
int card_fickle_efreet(int player, int card, event_t event)
{
  /* Fickle Efreet	|3|R
   * Creature - Efreet 5/2
   * Whenever this creature attacks or blocks, flip a coin at end of combat. If you lose the flip, an opponent gains control of this creature. */

  int result;
  card_instance_t *instance;

  if (((event == EVENT_DECLARE_ATTACKERS) && (result = is_attacking(player, card), result != 0)) ||
      ((result = blocking(player, card, event),
        result != 0 && (result = is_humiliated(player, card, AL_TRIG), result == 0))))
  {
    instance = get_card_instance(player, card);
    instance->targets[1].card = 0x42;
  }
  instance = get_card_instance(player, card);
  if ((instance->targets[1].card == 0x42) &&
      (result = end_of_combat_trigger(player, card, event, 2), result != 0))
  {
    result = flip_a_coin(player, card);
    if (result == 0)
    {
      instance = get_card_instance(player, card);
      instance->targets[1].card = 0;
      give_control_of_self(player, card);
    }
    instance = get_card_instance(player, card);
    instance->targets[1].card = 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x26024d0; original cards/prophecy.c:2455. */
int card_flameshot(int player, int card, event_t event)
{
  /* Flameshot	|3|R
   * Sorcery
   * You may discard a Mountain card rather than pay this spell's mana cost.
   * ~ deals 3 damage divided as you choose among one, two, or three target creatures. */

  card_instance_t *instance;
  int result;
  target_t *ret_location;
  int target_number;
  target_definition_t td;
  test_definition_t this_test;

  if (event == EVENT_MODIFY_COST)
  {
    new_default_test_definition(&this_test, 1, "Select a Mountain card.");
    this_test.zone = 0x100;
    this_test.subtype = 0x5006;
    result = check_battlefield_for_special_card(player, card, player, 0, &this_test);
    if (result == 0)
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
      return 0;
    }
    null_casting_cost(player, card);
    instance = get_card_instance(player, card);
    instance->info_slot = 1;
    return 0;
  }
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    new_default_test_definition(&this_test, 1, "Select a Mountain card.");
    this_test.zone = 0x100;
    this_test.subtype = 0x5006;
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
  }
  if (event != EVENT_CAST_SPELL)
  {
    if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_RESOLVE_SPELL))
    {
      if (event == EVENT_CHANGE_TARGET)
      {
        default_target_definition(player, card, &td, 2);
        new_default_test_definition(&this_test, 1, "Select a Mountain card.");
        this_test.zone = 0x100;
        this_test.subtype = 0x5006;
        get_card_instance(player, card);
        return 0;
      }
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    new_default_test_definition(&this_test, 1, "Select a Mountain card.");
    this_test.zone = 0x100;
    this_test.subtype = 0x5006;
    get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      divide_damage(player, card, &td);
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
    return 0;
  }
  if (card != affected_card)
  {
    return 0;
  }
  if (player != affected_card_controller)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  new_default_test_definition(&this_test, 1, "Select a Mountain card.");
  this_test.zone = 0x100;
  this_test.subtype = 0x5006;
  instance = get_card_instance(player, card);
  if (card != affected_card)
  {
    return 0;
  }
  if (player != affected_card_controller)
  {
    return 0;
  }

  instance->number_of_targets = '\0';
  result = played_for_free(player, card);
  if (((result == 0) && (result = is_token(player, card), result == 0)) &&
      (instance->info_slot == 1))
  {

    result = get_id(player, card);
    result = has_mana_to_cast_id(player, EVENT_CAST_SPELL, result);
    if ((result == 0) ||
        (result = do_dialog(player, player, card, -1, -1,
                            " Discard a Mountain card\n Play normally\n Cancel", 1),
         result == 0))
    {
      result = new_select_a_card(player, player, 4, 0, 3, -1, &this_test);
      if (result < 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      discard_card(player, result);
      td.allow_cancel = 0;
    }
    else
    {
      if (result != 1)
      {
        spell_fizzled = 1;
        return 0;
      }
      result = get_id(player, card);
      charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
    }
    if (spell_fizzled == 1)
    {
      return 0;
    }
  }
  ret_location = instance->targets;
  target_number = 0;
  do
  {
    result = select_target(player, card, &td, "Select target creature.", ret_location);
    if (result == 0)
    {
      if (target_number != 3)
      {
        spell_fizzled = 1;
      }
      break;
    }
    target_number = target_number + 1;
    ret_location = ret_location + 1;
  } while (target_number != 3);
  return basic_spell(player, card, EVENT_CAST_SPELL);
}

/* ManalinkEh 2022: 0x26029e0; original cards/prophecy.c:2545. */
int card_flay(int player, int card, event_t event)
{
  /* Flay	|3|B
   * Sorcery
   * Target player discards a card at random. Then that player discards another card at random unless they pay |1. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {

    return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
      return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      discard(instance->targets[0].player, 1, player);
      result = has_mana(instance->targets[0].player, 7, 1);
      if ((result == 0) ||
          (result = charge_mana(instance->targets[0].player, COLOR_ANY, 1), result == 0))
      {
        discard(instance->targets[0].player, 1, player);
      }
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2602b50; original cards/prophecy.c:2573. */
int card_flowering_field(int player, int card, event_t event)
{
  /* Flowering Field	|1|W
   * Enchantment - Aura
   * Enchant land
   * Enchanted land has "|T: Prevent the next 1 damage that would be dealt to any target this turn." */

  card_instance_t *instance;
  target_definition_t td;
  if (!is_humiliated(player, card, AL_STATIC) &&
      (instance = get_card_instance(player, card), instance->damage_target_player >= 0))
  {
    instance = get_card_instance(player, card);
    int attached_player = instance->damage_target_player;
    int attached_card = instance->damage_target_card;
    default_target_definition(attached_player, attached_card, &td, 0xff);
    td.extra = damage_card;
    td.special = TARGET_SPECIAL_DAMAGE_PLAYER;
    td.illegal_abilities = 0;
    if (event == EVENT_CAN_ACTIVATE)
      return granted_generic_activated_ability(player, card, attached_player, attached_card, event,
                                               0x1000005, 0, 0, 0, 0, 0, 0, 0, &td,
                                               "TARGET_DAMAGE");
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = 0;
      if (pick_target(&td, "TARGET_DAMAGE"))
        return granted_generic_activated_ability(player, card, attached_player, attached_card,
                                                 event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
    }
    else if (event == EVENT_RESOLVE_ACTIVATION &&
             validate_target(attached_player, attached_card, &td, 0))
    {
      instance = get_card_instance(attached_player, attached_card);
      prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
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

  default_target_definition(player, card, &td, 1);
  if (ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0))
    td.allowed_controller = td.preferred_controller = player;
  return aura(player, card, event, &td, "TARGET_LAND");
}
#endif

/* ManalinkEh 2022: 0x2602e70; original cards/prophecy.c:2622. */
int card_forgotten_harvest(int player, int card, event_t event)
{
  /* Forgotten Harvest	|1|G
   * Enchantment
   * At the beginning of your upkeep, you may exile a land card from your graveyard. If you do, put a +1/+1 counter on target creature. */

  int result;
  card_instance_t *instance;
  char *prompt_cursor;
  char *message_cursor;
  target_definition_t td;
  char msg[100];
  test_definition_t this_test;

  result = duh_mode(player);
  upkeep_trigger_ability_mode(player, card, event, player,
                              (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {

    instance = get_card_instance(player, card);
    msg[0x1d] = '\0';
    prompt_cursor = "Select a land card to exile.";
    message_cursor = msg;
    for (result = 7; result != 0; result = result + -1)
    {
      *(uint32_t *)message_cursor = *(uint32_t *)prompt_cursor;
      prompt_cursor = prompt_cursor + 4u;
      message_cursor = message_cursor + 4u;
    }
    *message_cursor = *prompt_cursor;
    prompt_cursor = msg + 0x20;
    for (result = 0x11; result != 0; result = result + -1)
    {
      prompt_cursor[0] = '\0';
      prompt_cursor[1] = '\0';
      prompt_cursor[2] = '\0';
      prompt_cursor[3] = '\0';
      prompt_cursor = prompt_cursor + 4u;
    }
    msg[0x1e] = '\0';
    msg[0x1f] = '\0';
    new_default_test_definition(&this_test, 1, msg);
    result = new_global_tutor(player, player, 2, 6, 0, 3, &this_test);
    if (-1 < result)
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      result = can_target(&td);
      if (result != 0)
      {
        result = pick_target(&td, "TARGET_CREATURE");
        if (result != 0)
        {
          add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                       COUNTER_P1_P1, 1);
        }
      }
    }
    instance->number_of_targets = '\0';
  }
  return global_enchantment(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25fa4f0; original cards/prophecy.c:2650. */
static int glittering_lion_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  int p;
  int source_card;
  int activation_flags;
  if (event == EVENT_STATIC_EFFECTS && card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    p = instance->targets[0].player;
    if (p >= 0)
    {
      int source_id = get_id(p, instance->targets[0].card);
      if (source_id != 0x12e0)
        kill_card(player, card, KILL_EXILE);
      instance = in_play(instance->targets[0].player, instance->targets[0].card);
      if (!instance)
        kill_card(player, card, KILL_EXILE);
    }
  }
  instance = get_card_instance(player, card);
  if (instance->targets[0].player == -1)
    return 0;
  instance = get_card_instance(player, card);
  p = instance->targets[0].player;
  source_card = instance->targets[0].card;
  if (event == EVENT_CHECK_ABILITIES)
  {
    if (source_card == affected_card && p == affected_card_controller && current_phase < 0x18 &&
        !is_humiliated(p, source_card, AL_STATIC) &&
        (instance = get_card_instance(p, source_card), instance->targets[1].player != 0x42))
      check_abilities_toughness += 0x3fff;
  }
  instance = get_card_instance(p, source_card);
  if (instance->targets[1].player != 0x42 && !is_humiliated(p, source_card, AL_KEY))
    prevent_all_damage(p, source_card, event);
  if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(p, source_card);
    instance->targets[1].player = 0;
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE)
    return granted_generic_activated_ability(p, source_card, player, card, event, 0, 3, 0, 0, 0, 0,
                                             0, 0, NULL, NULL) != 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(p, source_card);
    instance->targets[1].player = 0x42;
    return 0;
  }
  if (event != EVENT_ACTIVATE)
    return 0;
  activation_flags = 1;
  if (ai_is_speculating != 1)
  {
    activation_flags = 0x2000;
    if ((trace_mode & 2U) == 0 && player == 0)
      return granted_generic_activated_ability(p, source_card, player, card, EVENT_ACTIVATE,
                                               activation_flags, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
    activation_flags = 0;
  }
  activation_flags = (unsigned int)(1 - player == activation_flags) << 0xd;
  return granted_generic_activated_ability(p, source_card, player, card, EVENT_ACTIVATE,
                                           activation_flags, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2603010; original cards/prophecy.c:2705. */
int card_glittering_lion(int player, int card, event_t event)
{
  /* Glittering Lion	|2|W
   * Creature - Cat 2/2
   * Prevent all damage that would be dealt to this creature.
   * |3: Until end of turn, this creature loses "Prevent all damage that would be dealt to this creature." Any player may activate this ability. */

  card_instance_t *instance;
  int p;
  bool legacy_found;
  card_instance_t *source_record;
  card_instance_t *legacy_record;
  int activation_flags;
  int card_00;

  if (event == EVENT_CHECK_ABILITIES)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (current_phase < 0x18)) &&
        ((activation_flags = is_humiliated(player, card, AL_STATIC),
          activation_flags == 0 && (source_record = get_card_instance(player, card),
                                    source_record->targets[1].player != 0x42))))
    {
      check_abilities_toughness = check_abilities_toughness + 0x3fff;
    }
  }
  else if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      legacy_found = false;
      bool scan_stopped = false;
      p = 0;
      do
      {
        card_00 = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, card_00);
            if ((((legacy_record != NULL) && (is_what(p, card_00, 0x80) != 0)) &&
                 (legacy_record = get_card_instance(p, card_00),
                  legacy_record->info_slot_fn == glittering_lion_legacy)) &&
                ((legacy_record->targets[0].player == player &&
                  (legacy_record->targets[0].card == card))))
            {
              if (player == p)
              {
                kill_card(player, card_00, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                {
                  scan_stopped = true;
                  break;
                }
                legacy_found = true;
              }
            }
            card_00 = card_00 + 1;
          } while (card_00 <= active_cards_count[p]);
        }
        if (scan_stopped)
          break;
        p = p + 1;
      } while (p != 2);
      if ((!scan_stopped && !legacy_found) && (instance->targets[9].card != 0x42))
      {
        create_legacy_activate_for_opponent(player, card, glittering_lion_legacy, player, card);
      }
    }
  }
  instance = get_card_instance(player, card);
  if (instance->targets[1].player != 0x42 && !is_humiliated(player, card, AL_KEY))
    prevent_all_damage(player, card, event);
  if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(player, card);
    instance->targets[1].player = 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (ai_is_speculating == 1)
    activation_flags = (unsigned int)(1 - player == 1) << 13;
  else if ((trace_mode & 2U) == 0 && player == 0)
    activation_flags = 0x2000;
  else
    activation_flags = (unsigned int)(1 - player == 0) << 13;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    instance = get_card_instance(instance->parent_controller, instance->parent_card);
    instance->targets[1].player = 0x42;
  }
  return generic_activated_ability(player, card, event, activation_flags, 3, 0, 0, 0, 0, 0, 0, NULL,
                                   NULL);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25fa8d0; original cards/prophecy.c:2770. */
static int glittering_lynx_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  int p;
  int source_card;
  int activation_flags;
  if (event == EVENT_STATIC_EFFECTS && card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    p = instance->targets[0].player;
    if (p >= 0)
    {
      int source_id = get_id(p, instance->targets[0].card);
      if (source_id != 0x12e1)
        kill_card(player, card, KILL_EXILE);
      instance = in_play(instance->targets[0].player, instance->targets[0].card);
      if (!instance)
        kill_card(player, card, KILL_EXILE);
    }
  }
  instance = get_card_instance(player, card);
  if (instance->targets[0].player == -1)
    return 0;
  instance = get_card_instance(player, card);
  p = instance->targets[0].player;
  source_card = instance->targets[0].card;
  if (event == EVENT_CHECK_ABILITIES)
  {
    if (source_card == affected_card && p == affected_card_controller && current_phase < 0x18 &&
        !is_humiliated(p, source_card, AL_STATIC) &&
        (instance = get_card_instance(p, source_card), instance->targets[1].player != 0x42))
      check_abilities_toughness += 0x3fff;
  }
  instance = get_card_instance(p, source_card);
  if (instance->targets[1].player != 0x42 && !is_humiliated(p, source_card, AL_KEY))
    prevent_all_damage(p, source_card, event);
  if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(p, source_card);
    instance->targets[1].player = 0;
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE)
    return granted_generic_activated_ability(p, source_card, player, card, event, 0, 2, 0, 0, 0, 0,
                                             0, 0, NULL, NULL) != 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(p, source_card);
    instance->targets[1].player = 0x42;
    return 0;
  }
  if (event != EVENT_ACTIVATE)
    return 0;
  activation_flags = 1;
  if (ai_is_speculating != 1)
  {
    activation_flags = 0x2000;
    if ((trace_mode & 2U) == 0 && player == 0)
      return granted_generic_activated_ability(p, source_card, player, card, EVENT_ACTIVATE,
                                               activation_flags, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
    activation_flags = 0;
  }
  activation_flags = (unsigned int)(1 - player == activation_flags) << 0xd;
  return granted_generic_activated_ability(p, source_card, player, card, EVENT_ACTIVATE,
                                           activation_flags, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2603370; original cards/prophecy.c:2825. */
int card_glittering_lynx(int player, int card, event_t event)
{
  /* Glittering Lynx	|W
   * Creature - Cat 1/1
   * Prevent all damage that would be dealt to this creature.
   * |2: Until end of turn, this creature loses "Prevent all damage that would be dealt to this creature." Any player may activate this ability. */

  card_instance_t *instance;
  int p;
  bool legacy_found;
  card_instance_t *source_record;
  card_instance_t *legacy_record;
  int activation_flags;
  int card_00;

  if (event == EVENT_CHECK_ABILITIES)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (current_phase < 0x18)) &&
        ((activation_flags = is_humiliated(player, card, AL_STATIC),
          activation_flags == 0 && (source_record = get_card_instance(player, card),
                                    source_record->targets[1].player != 0x42))))
    {
      check_abilities_toughness = check_abilities_toughness + 0x3fff;
    }
  }
  else if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      legacy_found = false;
      bool scan_stopped = false;
      p = 0;
      do
      {
        card_00 = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, card_00);
            if ((((legacy_record != NULL) && (is_what(p, card_00, 0x80) != 0)) &&
                 (legacy_record = get_card_instance(p, card_00),
                  legacy_record->info_slot_fn == glittering_lynx_legacy)) &&
                ((legacy_record->targets[0].player == player &&
                  (legacy_record->targets[0].card == card))))
            {
              if (player == p)
              {
                kill_card(player, card_00, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                {
                  scan_stopped = true;
                  break;
                }
                legacy_found = true;
              }
            }
            card_00 = card_00 + 1;
          } while (card_00 <= active_cards_count[p]);
        }
        if (scan_stopped)
          break;
        p = p + 1;
      } while (p != 2);
      if ((!scan_stopped && !legacy_found) && (instance->targets[9].card != 0x42))
      {
        create_legacy_activate_for_opponent(player, card, glittering_lynx_legacy, player, card);
      }
    }
  }
  instance = get_card_instance(player, card);
  if (instance->targets[1].player != 0x42 && !is_humiliated(player, card, AL_KEY))
    prevent_all_damage(player, card, event);
  if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(player, card);
    instance->targets[1].player = 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (ai_is_speculating == 1)
    activation_flags = (unsigned int)(1 - player == 1) << 13;
  else if ((trace_mode & 2U) == 0 && player == 0)
    activation_flags = 0x2000;
  else
    activation_flags = (unsigned int)(1 - player == 0) << 13;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    instance = get_card_instance(instance->parent_controller, instance->parent_card);
    instance->targets[1].player = 0x42;
  }
  return generic_activated_ability(player, card, event, activation_flags, 2, 0, 0, 0, 0, 0, 0, NULL,
                                   NULL);
}
#endif

/* ManalinkEh 2022: 0x26036d0; original cards/prophecy.c:2890. */
int card_hazy_homunculus(int player, int card, event_t event)
{
  /* Hazy Homunculus	|1|U
   * Creature - Homunculus Illusion 1/1
   * This creature can't be blocked as long as defending player controls an untapped land. */

  int result;
  test_definition_t test;

  if (event == EVENT_BLOCK_LEGALITY)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (((result == 0) && (attacking_card_controller == player)) && (attacking_card == card))
    {
      default_test_definition(&test, 1);
      test.state = 0x10;
      test.state_flag = 1;
      result = check_battlefield_for_special_card(player, card, 1 - current_turn, 0, &test);
      if (result != 0)
      {
        event_result = 1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2603790; original cards/prophecy.c:2911. */
int card_hollow_warrior(int player, int card, event_t event)
{
  /* Hollow Warrior	|4
   * Artifact Creature - Golem Warrior 4/4
   * This creature can't attack or block unless you tap an untapped creature you control not declared as an attacking or blocking creature this combat. (This cost is paid as attackers or blockers are declared.) */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (trigger_condition == TRIGGER_PAY_TO_ATTACK)
  {
    if (current_phase != 0x15)
    {
      return 0;
    }
    result = is_humiliated(player, card, AL_STATIC);
    if (((((card == affected_card) && (player == affected_card_controller)) &&
          (reason_for_trigger_controller == player)) &&
         ((result == 0 && forbid_attack == 0 && (trigger_cause_controller == player)))) &&
        (trigger_cause == card))
    {
      if (event == EVENT_TRIGGER)
      {
        event_result = event_result | 2;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {
        base_target_definition(player, card, &td, 2);
        td.allowed_controller = player;
        td.preferred_controller = player;
        td.illegal_state = TARGET_STATE_TAPPED | TARGET_STATE_ATTACKING | TARGET_STATE_ATTACKED | TARGET_STATE_ISBLOCKED | TARGET_STATE_BLOCKING | TARGET_STATE_IN_COMBAT;
        td.special = TARGET_SPECIAL_NOT_ME;
        result = can_target(&td);
        if ((result == 0) ||
            (result = new_pick_target(
                 &td, "Select an untapped creature you control not declared as an attacker.", 0,
                 0x80),
             result == 0))
        {
          forbid_attack = 1;
        }
        else
        {

          instance = get_card_instance(player, card);
          tap_card(instance->targets[0].player, instance->targets[0].card);
          instance->number_of_targets = '\0';
        }
        if (forbid_attack != 0)
          remove_state(trigger_cause_controller, trigger_cause, 0x8000);
      }
    }
  }
  if (((((trigger_condition == TRIGGER_PAY_TO_BLOCK) && (current_phase == 0x17)) && (card == affected_card)) &&
       (((player == affected_card_controller &&
          (1 - current_turn == reason_for_trigger_controller)) &&
         ((trigger_cause_controller == player &&
           ((trigger_cause == card && (forbid_attack == 0)))))))) &&
      (result = is_humiliated(player, card, AL_STATIC), result == 0))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      base_target_definition(player, card, &td, 2);
      td.allowed_controller = player;
      td.preferred_controller = player;
      td.illegal_state = TARGET_STATE_TAPPED | TARGET_STATE_ATTACKING | TARGET_STATE_ATTACKED | TARGET_STATE_ISBLOCKED | TARGET_STATE_BLOCKING | TARGET_STATE_IN_COMBAT;
      td.special = TARGET_SPECIAL_NOT_ME;
      result = can_target(&td);
      if ((result == 0) ||
          (result = new_pick_target(
               &td, "Select an untapped creature you control not declared as a blocker.", 0, 0x80),
           result == 0))
      {
        forbid_attack = 1;
      }
      else
      {

        instance = get_card_instance(player, card);
        tap_card(instance->targets[0].player, instance->targets[0].card);
        instance->number_of_targets = '\0';
      }
    }
    if (forbid_attack != 0)
    {
      remove_state(player, card, 0x8008);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25fb150; original cards/prophecy.c:2975. */
static int was_dealt_damage_this_turn(int unused, intptr_t unused2, int player, int card)
{
  int result;
  card_instance_t *instance;
  result = is_what(player, card, 2);
  if (result == 0)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  return (unsigned int)(instance->damage_on_card != 0);
}

/* ManalinkEh 2022: 0x2603a90; original cards/prophecy.c:2982. */
int card_inflame(int player, int card, event_t event)
{
  /* Inflame	|R
   * Instant
   * ~ deals 2 damage to each creature dealt damage this turn. */

  int result;
  test_definition_t test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    new_default_test_definition(&test, 2, "");
    test.special_selection_function = was_dealt_damage_this_turn;
    new_damage_all(player, card, 2, 2, 0, &test);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2603b40; original cards/prophecy.c:2999. */
int card_jeweled_spirit(int player, int card, event_t event)
{
  /* Jeweled Spirit	|3|W|W
   * Creature - Spirit 3/3
   * Flying
   * Sacrifice two lands: This creature gains protection from artifacts or from the color of your choice until end of turn. */

  card_instance_t *instance;
  int result;
  test_definition_t test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    new_default_test_definition(&test, 1, "Select a land card to sacrifice.");
    test.qty = 2;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE,
                                                                GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL,
                                                                NULL, TYPE_NONE, &test, SARV_0);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_ACTIVATE, GAA_NONE, 0, 0,
                                                       0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test,
                                                       SARV_0);
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    new_default_test_definition(&test, 1, "Select a land card to sacrifice.");
  }
  test.qty = 2;
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  result = get_deck_color(player, 1 - player);
  result = do_dialog(player, player, card, -1, -1,
                     " Prot. from Black\n Prot. from Blue\n Prot. from Green\n Prot from Red\n "
                     "Prot. from White\n Prot from Artifacts",
                     result + -1);
  pump_ability_until_eot(player, card, instance->parent_controller, instance->parent_card, 0, 0,
                         1 << ((char)result + 0xbU & 0x1f) | 0x10000000, 0);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2603da0; original cards/prophecy.c:3034. */
int card_jolraels_favor(int player, int card, event_t event)
{
  /* Jolrael's Favor	|1|G
   * Enchantment - Aura
   * Flash
   * Enchant creature
   * |1|G: Regenerate enchanted creature. */

  card_instance_t *instance = get_card_instance(player, card);
  if (instance->damage_target_player < 0 ||
      (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE &&
       event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP))
    return aura_pref_controller(player, card, event, player, NULL);
  instance = get_card_instance(player, card);
  if (event != EVENT_CLEANUP)
  {
    int attached_player = instance->damage_target_player;
    int attached_card = instance->damage_target_card;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (can_be_regenerated(attached_player, attached_card))
        regenerate_target(attached_player, attached_card);
    }
    else if (event == EVENT_CAN_ACTIVATE)
    {
      if (attached_player < 0 || !((((unsigned int)land_can_be_played >> 8) & 0xff) & 2) ||
          !can_be_regenerated(attached_player, attached_card))
        return 0;
    }
  }
  return generic_activated_ability(player, card, event, 0, 1, 0, 0, 1, 0, 0, 0, NULL, NULL) ? 99
                                                                                            : 0;
}
#endif

/* ManalinkEh 2022: 0x2603f30; original cards/prophecy.c:3065. */
int card_keldon_arsonist(int player, int card, event_t event)
{
  /* Keldon Arsonist	|2|R
   * Creature - Human Soldier 1/1
   * |1, Sacrifice two lands: Destroy target land. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  test_definition_t test;

  if (event == EVENT_ACTIVATE)
  {
    new_default_test_definition(&test, 1, "Select a land card to sacrifice.");
    test.qty = 2;
    default_target_definition(player, card, &td, 1);
    td.preferred_controller = player;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    new_default_test_definition(&test, 1, "Select a land card to sacrifice.");
    test.qty = 2;
    default_target_definition(player, card, &td, 1);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
  }
  result = generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, 1, 0, 0, 0, 0, 0, 0, &td,
      "Select target land to destroy.", TYPE_NONE, &test, SARV_0);
  return result;
}

/* ManalinkEh 2022: 0x26040f0; original cards/prophecy.c:3093. */
int get_power_sum_of_untapped_creatures(int player)
{
  card_instance_t *instance;
  int result;
  int player_00;
  int card;
  int total_power;

  total_power = 0;
  player_00 = 0;
  do
  {

    if (((player == player_00) || (player == 2)) && (0 < active_cards_count[player_00]))
    {
      card = 0;
      do
      {
        instance = in_play(player_00, card);
        if (instance != NULL)
        {
          result = is_what(player_00, card, 2);
          if (result != 0)
          {
            result = is_tapped(player_00, card);
            if (result == 0)
            {
              result = get_power(player_00, card);
              total_power = total_power + result;
            }
          }
        }
        card = card + 1;
      } while (card < active_cards_count[player_00]);
    }
    player_00 = player_00 + 1;
  } while (player_00 != 2);
  return total_power;
}

/* ManalinkEh 2022: 0x2604190; original cards/prophecy.c:3112. */
int card_keldon_battlewagon(int player, int card, event_t event)
{
  /* Keldon Battlewagon	|5
   * Artifact Creature - Juggernaut 0/3
   * Trample
   * This creature can't block.
   * When this creature attacks, sacrifice it at end of combat.
   * Tap an untapped creature you control: This creature gets +X/+0 until end of turn, where X is the power of the creature tapped this way. */

  int power_or_result;
  card_instance_t *instance;
  int target_available_count;
  target_definition_t td;

  cannot_block(player, card, event);
  if (declare_attackers_trigger(player, card, event, 0, player, card) != 0)
  {
    create_targetted_legacy_effect(player, card, sacrifice_at_end_of_combat, player, card);
  }
  if ((uint32_t)(event - EVENT_POW_BOOST) < 2)
  {

    base_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.illegal_state = TARGET_STATE_TAPPED;
    target_available_count = target_available(player, card, &td);
    power_or_result = 0;
    if (0 < target_available_count)
    {
      if (can_use_activated_abilities(player, card) == 0)
      {
        return 0;
      }
      if (has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) == 0)
      {
        return 0;
      }
      power_or_result = get_power_sum_of_untapped_creatures(player);
    }
  }
  else
  {
    if ((((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE)) &&
         (event != EVENT_RESOLVE_ACTIVATION)) &&
        (event != EVENT_CLEANUP))
    {
      return 0;
    }

    instance = get_card_instance(player, card);
    base_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.illegal_state = TARGET_STATE_TAPPED;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      return generic_shade_merge_pt(
          player, card, EVENT_RESOLVE_ACTIVATION, 0, 0, 0, 0, 0, 0, 0,
          get_power(instance->targets[9].player, instance->targets[9].card), 0);
    }
    power_or_result = generic_activated_ability_with_tapsubtype_as_cost(
        player, card, event, GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, &td, 1);
  }
  return power_or_result;
}

/* ManalinkEh 2022: 0x2604420; original cards/prophecy.c:3161. */
int card_keldon_berserker(int player, int card, event_t event)
{
  /* Keldon Berserker	|3|R
   * Creature - Human Soldier Berserker 2/3
   * Whenever this creature attacks, if you control no untapped lands, it gets +3/+0 until end of turn. */

  int result;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    result = control_an_untapped_land(player);
    if (result == 0)
    {
      pump_until_eot(player, card, player, card, 3, 0);
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26044a0; original cards/prophecy.c:3174. */
int card_latullas_orders(int player, int card, event_t event)
{
  /* Latulla's Orders	|1|R
   * Enchantment - Aura
   * Flash
   * Enchant creature
   * Whenever enchanted creature deals combat damage to defending player, you may destroy target artifact that player controls. */

  unsigned int packed_damage_counts;
  int result;
  card_instance_t *instance;
  int times_damaged[2];
  target_definition_t td;

  result = attached_creature_deals_damage(player, card, event,
                                          DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_MUST_DAMAGE_OPPONENT |
                                              DDBM_TRIGGER_OPTIONAL);
  if (result != 0)
  {

    instance = get_card_instance(player, card);
    packed_damage_counts = instance->targets[1].player;
    times_damaged[1] = packed_damage_counts >> 8 & 0xff;
    times_damaged[0] = packed_damage_counts & 0xff;
    default_target_definition(player, card, &td, 0x40);
    td.preferred_controller = 1 - current_turn;
    td.allowed_controller = td.preferred_controller;
    result = times_damaged[td.preferred_controller];
    instance->targets[0].player = td.preferred_controller;
    if (result != 0)
    {
      result = can_target(&td);
      if (result != 0)
      {
        result = new_pick_target(
            &td, "Select target artifact defending player controls to destroy.", 0, 0x80);
        if (result != 0)
        {
          kill_card(1 - current_turn, instance->targets[0].card, KILL_DESTROY);
        }
      }
    }
    instance->number_of_targets = '\0';
  }
  return aura_pref_controller(player, card, event, player, NULL);
}
#endif

/* ManalinkEh 2022: 0x26045d0; original cards/prophecy.c:3201. */
int card_mana_vapors(int player, int card, event_t event)
{
  /* Mana Vapors	|1|U
   * Sorcery
   * Lands target player controls don't untap during their next untap step. */

  int p;
  int opponent;
  card_instance_t *instance;
  int c;
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
      default_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      get_card_instance(player, card);

      if (((card == affected_card) && (player == affected_card_controller)) &&
          (opponent = 1 - player, 0 < active_cards_count[opponent]))
      {
        c = 0;
        do
        {
          instance = in_play(opponent, c);
          if (((instance != NULL) && (is_what(opponent, c, 1) != 0)) &&
              (is_tapped(opponent, c) != 0))
          {
            ai_modifier = ai_modifier + c * 4;
          }
          c = c + 1;
        } while (c < active_cards_count[opponent]);
      }
      return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {
        if (valid_target(&td) != 0)
        {

          p = instance->targets[0].player;
          c = active_cards_count[p] + -1;
          if (-1 < c)
          {
            do
            {
              instance = in_play(p, c);
              if ((instance != NULL) && (is_what(p, c, 1) != 0))
              {
                does_not_untap_effect(player, card, p, c, 0, 1);
              }
              c = c + -1;
            } while (c != -1);
          }
        }
        kill_card(player, card, KILL_DESTROY);
      }
      return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  get_card_instance(player, card);

  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}

/* ManalinkEh 2022: 0x2604870; original cards/prophecy.c:3243. */
int card_mercenary_informer(int player, int card, event_t event)
{
  /* Mercenary Informer	|2|W
   * Creature - Human Rebel Mercenary 2/1
   * This creature can't be the target of black spells or abilities from black sources.
   * |2|W: Put target nontoken Mercenary on the bottom of its owner's library. */

  card_instance_t *instance;
  unsigned int attacker_color;
  unsigned int protected_colors;
  target_definition_t td;

  special_abilities2(player, card, event, SP_KEYWORD2_PROT_MISC);
  if (event == EVENT_PROT_MISC)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (((unsigned int)event_result >> 24 & 0xff) == '\x05'))
    {
      attacker_color = get_color(attacking_card_controller, attacking_card);
      protected_colors = get_sleighted_color_test(player, card, 2);
      if ((attacker_color & protected_colors) != 0)
      {
        event_result = event_result | 1;
      }
    }
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TARGET_TYPE_TOKEN;
    td.required_subtype = 0x73;
  }
  else
  {
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TARGET_TYPE_TOKEN;
    td.required_subtype = 0x73;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (valid_target(&td))
      {

        instance = get_card_instance(player, card);
        put_on_bottom_of_deck(instance->targets[0].player, instance->targets[0].card);
      }
    }
  }
  return generic_activated_ability(player, card, event, 6, 2, 0, 0, 0, 0, 1, 0, &td,
                                   "Select target nontoken Mercenary card.");
}

/* ManalinkEh 2022: 0x2604a00; original cards/prophecy.c:3274. */
int card_mine_bearer(int player, int card, event_t event)
{
  /* Mine Bearer	|2|W
   * Creature - Human Soldier 1/1
   * |T, Sacrifice this creature: Destroy target attacking creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_ATTACKING;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_ATTACKING;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
  }
  result = generic_activated_ability(player, card, event, 0xf, 0, 0, 0, 0, 0, 0, 0, &td,
                                     "Select target attacking creature to destroy.");
  return result;
}

/* ManalinkEh 2022: 0x25facb0; original cards/prophecy.c:3300. */
static int mirror_strike_effect(int player, int card, event_t event)
{
  char attacking_player;
  int defending_player;
  card_instance_t *instance;
  int attacking_card_or_planeswalker;
  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {
    instance = get_card_instance(player, card);
    attacking_player = instance->damage_target_player;
    attacking_card_or_planeswalker = instance->damage_target_card;
    defending_player = instance->targets[1].player;
    instance = damage_being_dealt(event);
    if ((((instance != NULL) && (attacking_player == instance->damage_source_player)) &&
         (instance->damage_source_card == attacking_card_or_planeswalker)) &&
        ((instance->damage_target_player == defending_player &&
          (instance->damage_target_card == -1))))
    {
      attacking_card_or_planeswalker = damage_is_to_planeswalker(instance);
      if ((attacking_card_or_planeswalker == 0) && ((instance->token_status & 0x140000) != 0))
      {
        instance->damage_target_player = attacking_player;
        instance->damage_target_card = -1;
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

/* ManalinkEh 2022: 0x2604b50; original cards/prophecy.c:3328. */
int card_mirror_strike(int player, int card, event_t event)
{
  /* Mirror Strike	|3|W
   * Instant
   * All combat damage that would be dealt to you this turn by target unblocked creature is dealt to its controller instead. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_ATTACKING;
    td.illegal_state = TARGET_STATE_ISBLOCKED;
    get_card_instance(player, card);
    if ((current_phase != 0x18) && (result = can_target(&td), result != 0))
    {
      return 0;
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
    }
    else
    {
      if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
      {
        default_target_definition(player, card, &td, 2);
        td.required_state = TARGET_STATE_ATTACKING;
        td.illegal_state = TARGET_STATE_ISBLOCKED;
        instance = get_card_instance(player, card);
        if (event == EVENT_RESOLVE_SPELL)
        {
          result = valid_target(&td);
          if (result != 0)
          {
            result = create_targetted_legacy_effect(player, card, mirror_strike_effect,
                                                    instance->targets[0].player,
                                                    instance->targets[0].card);
            instance = get_card_instance(player, result);
            instance->targets[1].player = player;
          }
          kill_card(player, card, KILL_DESTROY);
        }
        return generic_spell(player, card, event, 0x81, &td, "Select target unblocked creature.", 1,
                             NULL);
      }
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
    }
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_ATTACKING;
    td.illegal_state = TARGET_STATE_ISBLOCKED;
    get_card_instance(player, card);
  }

  return generic_spell(player, card, event, 0x81, &td, "Select target unblocked creature.", 1,
                       NULL);
}

/* ManalinkEh 2022: 0x2604d30; original cards/prophecy.c:3361. */
int card_nakaya_shade(int player, int card, event_t event)
{
  /* Nakaya Shade	|1|B
   * Creature - Shade 1/1
   * |B: This creature gets +1/+1 until end of turn unless any player pays |2. */

  bool unpaid;
  int result;
  unsigned int pass;
  unsigned int argument1;

  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      result = charge_mana_for_activated_ability(player, card, 0, 1, 0, 0, 0, 0);
    }
    else
    {
      if (event == EVENT_RESOLVE_ACTIVATION)
      {

        pass = 0;
        unpaid = true;
        do
        {
          argument1 = current_turn ^ pass;
          result = has_mana(argument1, 7, 2);

          if ((((unpaid) && (result != 0)) &&
               (result = do_dialog(argument1, player, card, -1, -1, " Pay 2 stop boost\n Pass",
                                   (unsigned int)(player == argument1)),
                result == 0)) &&
              (charge_mana_while_resolving(player, card, EVENT_RESOLVE_ACTIVATION, argument1,
                                           COLOR_COLORLESS, 2),
               spell_fizzled != 1))
          {
            unpaid = false;
          }
          pass = pass + 1;
        } while (pass != 2);
        if (unpaid)
        {
          generic_shade_merge_pt(player, card, EVENT_RESOLVE_ACTIVATION, 0, 0, 1, 0, 0, 0, 0, 1, 1);
        }
      }
      else if (event != EVENT_CLEANUP)
        return generic_shade_merge_pt(player, card, event, 0, 0, 1, 0, 0, 0, 0, 1, 1);
      result = 0;
    }
    return result;
  }
  event = EVENT_CAN_ACTIVATE;
  return generic_shade_merge_pt(player, card, event, 0, 0, 1, 0, 0, 0, 0, 1, 1);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2604fb0; original cards/prophecy.c:3401. */
int card_noxious_field(int player, int card, event_t event)
{
  /* Noxious Field	|1|B|B
   * Enchantment - Aura
   * Enchant land
   * Enchanted land has "|T: This land deals 1 damage to each creature and each player." */

  target_definition_t td;
  if (!is_humiliated(player, card, AL_STATIC))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player >= 0)
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_ACTIVATE || (uint32_t)(event - EVENT_RESOLVE_ACTIVATION) <= 1 ||
          event == EVENT_CLEANUP)
      {
        int attached_player = instance->damage_target_player;
        int attached_card = instance->damage_target_card;
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          test_definition_t test;
          default_test_definition(&test, 2);
          new_damage_all(attached_player, attached_card, 2, 1, 2, &test);
        }
        return granted_generic_activated_ability(player, card, attached_player, attached_card,
                                                 event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
      }
    }
  }
  default_target_definition(player, card, &td, 1);
  td.preferred_controller = player;
  return aura_literal_prompt(player, card, event, &td, "Select target land.");
}
#endif

/* ManalinkEh 2022: 0x2605170; original cards/prophecy.c:3430. */
int card_outbreak(int player, int card, event_t event)
{
  /* Outbreak	|3|B
   * Sorcery
   * You may discard a Swamp card rather than pay this spell's mana cost.
   * Choose a creature type. All creatures of that type get -1/-1 until end of turn. */

  int result;
  card_instance_t *instance;
  test_definition_t this_test;

  if (event == EVENT_MODIFY_COST)
  {
    new_default_test_definition(&this_test, 1, "Select a Swamp card.");
    this_test.zone = 0x100;
    this_test.subtype = 0x5009;
    result = check_battlefield_for_special_card(player, card, player, 0, &this_test);
    if (result != 0)
    {
      null_casting_cost(player, card);
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      return 0;
    }
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
    return 0;
  }
  if (event == EVENT_CAN_CAST)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    return result;
  }
  if (event != EVENT_CAST_SPELL)
  {
    if (event != EVENT_RESOLVE_SPELL)
    {
      return 0;
    }
    default_test_definition(&this_test, 2);
    this_test.subtype = select_a_subtype(player, card);
    pump_creatures_until_eot(player, card, 2, 0, -1, -1, 0, 0, &this_test);
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  if (card != affected_card)
  {
    return 0;
  }
  if (player != affected_card_controller)
  {
    return 0;
  }
  result = played_for_free(player, card);
  if (result != 0)
  {
    return 0;
  }
  result = is_token(player, card);
  if (result != 0)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  if (instance->info_slot == 1)
  {

    result = get_id(player, card);
    result = has_mana_to_cast_id(player, EVENT_CAST_SPELL, result);
    if ((result == 0) || (result = do_dialog(player, player, card, -1, -1,
                                             " Discard a Swamp\n Play normally\n Cancel", 1),
                          result == 0))
    {

      new_default_test_definition(&this_test, 1, "Select a Swamp card.");
      this_test.zone = 0x100;
      this_test.subtype = 0x5009;
      result = new_select_a_card(player, player, 4, 0, 3, -1, &this_test);
      if (-1 < result)
      {
        discard_card(player, result);
        return 0;
      }
    }
    else if (result == 1)
    {
      result = get_id(player, card);
      charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
      return 0;
    }
    spell_fizzled = 1;
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2605450; original cards/prophecy.c:3501. */
int card_pit_raptor(int player, int card, event_t event)
{
  /* Pit Raptor	|2|B|B
   * Creature - Bird Mercenary 4/3
   * Flying, first strike
   * At the beginning of your upkeep, sacrifice this creature unless you pay |2|B|B. */

  basic_upkeep(player, card, event, 2, 2, 0, 0, 0, 0);
  return 0;
}

/* ManalinkEh 2022: 0x25fb1a0; original cards/prophecy.c:3512. */
int destroy_unless_pay_2(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  if (event == EVENT_RESOLVE_TRIGGER)
  {

    instance = get_card_instance(player, card);
    result = has_mana((int)instance->damage_target_player, 7, 2);
    if ((result == 0) ||
        (result = charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER,
                                              (int)instance->damage_target_player, COLOR_ANY, 2),
         result == 0))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x25f8dd0; original cards/prophecy.c:3535. */
void plague_fiend_effect(intptr_t player, intptr_t card, int t_player, int t_card)
{
  create_targetted_legacy_effect(player, card, destroy_unless_pay_2, t_player, t_card);
  return;
}

/* ManalinkEh 2022: 0x26054b0; original cards/prophecy.c:3538. */
int card_plague_fiend(int player, int card, event_t event)
{
  /* Plague Fiend	|1|B
   * Creature - Insect 1/1
   * Whenever this creature deals combat damage to a creature, destroy that creature unless its controller pays |2. */

  card_instance_t *instance;

  if (!is_humiliated(player, card, AL_TRIG))
  {
    if (((event == EVENT_CHANGE_TYPE) && (card == affected_card)) &&
        (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      instance->destroys_if_blocked = instance->destroys_if_blocked | 0x10;
    }
    for_each_creature_damaged_by_me(player, card, event, 4, plague_fiend_effect, player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25f8e30; original cards/prophecy.c:3561. */
static int quicksilver_wall_legacy(int player, int card, event_t event)
{
  int p;
  int target_card;
  card_instance_t *instance;
  int flags_or_effect;
  int source_id;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_id = get_id(p, instance->targets[0].card);
        if (source_id != 0x1303)
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
    return 0;
  }
  instance = get_card_instance(player, card);
  if (instance->targets[0].player == -1)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  p = instance->targets[0].player;
  target_card = instance->targets[0].card;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION && in_play(p, target_card))
    bounce_permanent(p, target_card);
  flags_or_effect = 1;
  if (ai_is_speculating != 1)
  {
    flags_or_effect = 0x2000;
    if ((trace_mode & 2U) == 0 && player == 0)
      return granted_generic_activated_ability(p, target_card, player, card, event, flags_or_effect, 4,
                                               0, 0, 0, 0, 0, 0, NULL, NULL);
    flags_or_effect = 0;
  }
  flags_or_effect = (unsigned int)(1 - player == flags_or_effect) << 0xd;
  return granted_generic_activated_ability(p, target_card, player, card, event, flags_or_effect, 4, 0,
                                           0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x2605540; original cards/prophecy.c:3597. */
int card_quicksilver_wall(int player, int card, event_t event)
{
  /* Quicksilver Wall	|2|U
   * Creature - Wall 1/6
   * Defender (This creature can't attack.)
   * |4: Return this creature to its owner's hand. Any player may activate this ability. */

  bool legacy_found;
  card_instance_t *instance;
  card_instance_t *legacy_record;
  int p;
  int card_00;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {

      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      do
      {
        card_00 = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, card_00);

            if (((legacy_record != NULL) && (is_what(p, card_00, 0x80) != 0)) &&
                ((legacy_record = get_card_instance(p, card_00),
                  legacy_record->info_slot_fn == quicksilver_wall_legacy &&
                      ((legacy_record->targets[0].player == player &&
                        (legacy_record->targets[0].card == card))))))
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
        create_legacy_activate_for_opponent(player, card, quicksilver_wall_legacy, player, card);
      }
    }
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_CLEANUP)
    return generic_activated_ability(player, card, event, 0, 4, 0, 0, 0, 0, 0, 0, NULL, NULL);
  if (event == EVENT_ACTIVATE)
    return generic_activated_ability(player, card, event, 0, 4, 0, 0, 0, 0, 0, 0, NULL, NULL);
  if (event != EVENT_RESOLVE_ACTIVATION)
    return 0;

  instance = get_card_instance(player, card);
  legacy_record = in_play(instance->parent_controller, instance->parent_card);
  if (legacy_record != NULL)
  {
    bounce_permanent(instance->parent_controller, instance->parent_card);
  }
  return generic_activated_ability(player, card, event, 0, 4, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x2605770; original cards/prophecy.c:3648. */
int card_rebel_informer(int player, int card, event_t event)
{
  /* Rebel Informer	|2|B
   * Creature - Human Mercenary Rebel 1/2
   * This creature can't be the target of white spells or abilities from white sources.
   * |3: Put target nontoken Rebel on the bottom of its owner's library. */

  card_instance_t *instance;
  unsigned int attacker_color;
  unsigned int protected_colors;
  target_definition_t td;

  special_abilities2(player, card, event, SP_KEYWORD2_PROT_MISC);
  if (event == EVENT_PROT_MISC)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (((unsigned int)event_result >> 24 & 0xff) == '\x05'))
    {
      attacker_color = get_color(attacking_card_controller, attacking_card);
      protected_colors = get_sleighted_color_test(player, card, 0x20);
      if ((attacker_color & protected_colors) != 0)
      {
        event_result = event_result | 1;
      }
    }
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TARGET_TYPE_TOKEN;
    td.required_subtype = 0x9b;
  }
  else
  {
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TARGET_TYPE_TOKEN;
    td.required_subtype = 0x9b;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (valid_target(&td))
      {

        instance = get_card_instance(player, card);
        put_on_bottom_of_deck(instance->targets[0].player, instance->targets[0].card);
      }
    }
  }
  return generic_activated_ability(player, card, event, 6, 3, 0, 0, 0, 0, 0, 0, &td,
                                   "Select target nontoken Rebel card.");
}

/* ManalinkEh 2022: 0x2605900; original cards/prophecy.c:3679. */
int card_rhystic_cave(int player, int card, event_t event)
{
  /* Rhystic Cave	""
   * Land
   * |T: Choose a color. Add one mana of that color unless any player pays |1. Activate only as an instant. */

  bool unpaid;
  card_instance_t *instance;
  int result;
  unsigned int argument1;
  unsigned int pass;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if ((((unsigned int)land_can_be_played >> 16 & 0xff) & 4) == 0)
      {
        return mana_producer(player, card, EVENT_CAN_ACTIVATE);
      }
    }
    else
    {

      if ((ai_is_speculating == 1) ||
          ((result = 0, player == 1 && (((unsigned char)trace_mode & 2) == 0))))
      {
        result = get_deck_color(player, player);
      }
      add_state(player, card, 0x10);

      pass = 0;
      result = choose_a_color(player, result);
      instance->info_slot = 1 << ((unsigned char)result & 0x1f);
      unpaid = true;
      do
      {
        argument1 = current_turn ^ pass;
        result = has_mana(argument1, 7, 1);
        if ((unpaid) && (result != 0))
        {

          result = do_dialog(argument1, player, card, -1, -1, " Pay 1 stop mana\n Pass",
                             (unsigned int)(player == argument1));
          if (result == 0)
          {
            charge_mana_while_resolving(player, card, EVENT_ACTIVATE, argument1, COLOR_COLORLESS,
                                        1);
            if (spell_fizzled != 1)
            {
              tapped_for_mana_color = -1;
              unpaid = false;
            }
          }
        }
        pass = pass + 1;
      } while (pass != 2);
      if (unpaid)
      {
        produce_mana_all_one_color(player, instance->info_slot, 1);
      }
    }
  }
  else if (((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP)) ||
           ((event == EVENT_COUNT_MANA &&
             ((card == affected_card && (player == affected_card_controller))))))
  {

    get_card_instance(player, card);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2605b00; original cards/prophecy.c:3724. */
int card_rhystic_lightning(int player, int card, event_t event)
{
  /* Rhystic Lightning	|2|R
   * Instant
   * ~ deals 4 damage to any target unless that permanent's controller or that player pays |2. If they do, ~ deals 2 damage to the permanent or player. */

  bool unpaid;
  int result;
  unsigned int argument1;
  unsigned int pass;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {

      unpaid = true;
      pass = 0;
      do
      {
        argument1 = current_turn ^ pass;
        result = has_mana(argument1, 7, 2);

        if ((((unpaid) && (result != 0)) &&
             (result = do_dialog(argument1, player, card, -1, -1, " Pay 2 less damage\n Pass",
                                 (unsigned int)(player == argument1)),
              result == 0)) &&
            (charge_mana_while_resolving(player, card, EVENT_RESOLVE_SPELL, argument1,
                                         COLOR_COLORLESS, 2),
             spell_fizzled != 1))
        {
          unpaid = false;
        }
        pass = pass + 1;
      } while (pass != 2);
      if (unpaid)
      {
        damage_target0(player, card, 4);
      }
      else
      {
        damage_target0(player, card, 2);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_ANY", 1, NULL);
}

/* ManalinkEh 2022: 0x2605d30; original cards/prophecy.c:3765. */
int card_rhystic_scrying(int player, int card, event_t event)
{
  /* Rhystic Scrying	|2|U|U
   * Sorcery
   * Draw three cards. Then if any player pays |2, discard three cards. */

  bool paid;
  int result;
  unsigned int argument1;
  unsigned int pass;

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
      if ((event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET) &&
          (event != EVENT_CHANGE_TARGET))
      {
        return 0;
      }
      if (event == EVENT_RESOLVE_SPELL)
      {

        draw_cards(player, 3);

        paid = false;
        pass = 0;
        do
        {
          argument1 = current_turn ^ pass;
          result = has_mana(argument1, 7, 2);

          if (((result != 0) &&
               (result = do_dialog(argument1, player, card, -1, -1, " Pay 2 discard 3\n Pass",
                                   (unsigned int)(player == argument1)),
                result == 0)) &&
              (charge_mana_while_resolving(player, card, EVENT_RESOLVE_SPELL, argument1,
                                           COLOR_COLORLESS, 2),
               spell_fizzled != 1))
          {
            paid = true;
          }
          pass = pass + 1;
        } while (pass != 2);
        if (paid)
        {
          multidiscard(player, 3, 0);
        }
        kill_card(player, card, KILL_DESTROY);
      }
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x2605ee0; original cards/prophecy.c:3798. */
int card_rhystic_shield(int player, int card, event_t event)
{
  /* Rhystic Shield	|1|W
   * Instant
   * Creatures you control get +0/+1 until end of turn. They get an additional +0/+2 until end of turn unless any player pays |2. */

  int i;
  bool unpaid;
  int available_or_choice;
  int unblocked;
  unsigned int argument1;
  unsigned int ai_or_pass;

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

      ai_or_pass = 1;
      if ((ai_is_speculating != 1) && (ai_or_pass = 0, player == 1))
      {
        ai_or_pass = (unsigned int)(((unsigned char)trace_mode & 2) == 0);
      }
      if (current_phase == 0x18)
      {
        if (0 < active_cards_count[ai_or_pass])
        {
          i = 0;
          do
          {
            unblocked = is_unblocked(ai_or_pass, i);
            if ((unblocked == 0) || (current_turn != ai_or_pass))
            {
              ai_modifier = ai_modifier + 0x18;
            }
            else
            {
              ai_modifier = ai_modifier + -0x40;
            }
            i = i + 1;
          } while (i < active_cards_count[ai_or_pass]);
        }
      }
      else
      {
        ai_modifier = ai_modifier + -0x10;
      }
    }
    else
    {
      if ((event != EVENT_CAN_CHANGE_TARGET && event != EVENT_RESOLVE_SPELL) &&
          (event != EVENT_CHANGE_TARGET))
      {
        return 0;
      }
      if (event == EVENT_RESOLVE_SPELL)
      {

        pump_subtype_until_eot(player, card, player, -1, 0, 1, 0, 0);

        unpaid = true;
        ai_or_pass = 0;
        do
        {
          argument1 = current_turn ^ ai_or_pass;
          available_or_choice = has_mana(argument1, 7, 2);

          if ((((unpaid) && (available_or_choice != 0)) &&
               (available_or_choice =
                    do_dialog(argument1, player, card, -1, -1, " Pay 2 or extra tough\n Pass",
                              (unsigned int)(player == argument1)),
                available_or_choice == 0)) &&
              (charge_mana_while_resolving(player, card, EVENT_RESOLVE_SPELL, argument1,
                                           COLOR_COLORLESS, 2),
               spell_fizzled != 1))
          {
            unpaid = false;
          }
          ai_or_pass = ai_or_pass + 1;
        } while (ai_or_pass != 2);
        if (unpaid)
        {
          pump_subtype_until_eot(player, card, player, -1, 0, 2, 0, 0);
        }
        kill_card(player, card, KILL_DESTROY);
      }
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x2606180; original cards/prophecy.c:3848. */
int card_rhystic_syphon(int player, int card, event_t event)
{
  /* Rhystic Syphon	|3|B|B
   * Sorcery
   * Unless target player pays |3, that player loses 5 life and you gain 5 life. */

  card_instance_t *instance;
  int p;
  int payment_choice;
  unsigned int argument7;
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
    }
    else
    {
      if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
      {
        default_target_definition(player, card, &td, 0);
        td.preferred_controller = 1 - player;
        td.zone = TARGET_ZONE_PLAYERS;
        instance = get_card_instance(player, card);
        if (event != EVENT_RESOLVE_SPELL)
          return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
        if (valid_target(&td) != 0)
        {

          p = instance->targets[0].player;
          if (has_mana(p, 7, 3) != 0)
          {

            if (ai_is_speculating == 1)
            {
              argument7 = 0;
            }
            else
            {
              argument7 = 1;
              if (p == 1)
              {
                argument7 = (unsigned int)trace_mode >> 1 & 1;
              }
            }
            payment_choice =
                do_dialog(p, player, card, -1, -1, " Pay 3 or lose life\n Pass", argument7);
            if ((payment_choice == 0) &&
                (charge_mana_while_resolving(player, card, EVENT_RESOLVE_SPELL, p, COLOR_COLORLESS,
                                             3),
                 spell_fizzled != 1))
            {
              kill_card(player, card, KILL_DESTROY);
              return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
            }
          }
          lose_life(p, 5);
          gain_life(player, 5);
        }

        kill_card(player, card, KILL_DESTROY);
        return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
      }
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
    }
  }
  default_target_definition(player, card, &td, 0);
  td.preferred_controller = 1 - player;
  td.zone = TARGET_ZONE_PLAYERS;
  get_card_instance(player, card);

  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}

/* ManalinkEh 2022: 0x25f9030; original cards/prophecy.c:3888. */
static int ribbon_snake_legacy(int player, int card, event_t event)
{
  int p;
  int target_card;
  card_instance_t *instance;
  int flags_or_effect;
  int source_id;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_id = get_id(p, instance->targets[0].card);
        if (source_id != 0x1311)
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
    return 0;
  }
  instance = get_card_instance(player, card);
  if (instance->targets[0].player == -1)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  p = instance->targets[0].player;
  target_card = instance->targets[0].card;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_ACTIVATE)
  {
    instance = get_card_instance(p, target_card);
    ai_modifier =
        ai_modifier + (unsigned int)(((instance->regen_status & 0x20) == 0) ? -96 : 0) + 0x30;
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    flags_or_effect = pump_ability_until_eot(p, target_card, p, target_card, 0, 0, 0x20, 0);
    if (flags_or_effect != -1)
    {
      instance = get_card_instance(p, flags_or_effect);
      instance->targets[4].player = 0;
    }
  }
  flags_or_effect = 1;
  if (ai_is_speculating != 1)
  {
    flags_or_effect = 0x2000;
    if ((trace_mode & 2U) == 0 && player == 0)
      return granted_generic_activated_ability(p, target_card, player, card, event, flags_or_effect, 2,
                                               0, 0, 0, 0, 0, 0, NULL, NULL);
    flags_or_effect = 0;
  }
  flags_or_effect = (unsigned int)(1 - player == flags_or_effect) << 0xd;
  return granted_generic_activated_ability(p, target_card, player, card, event, flags_or_effect, 2, 0,
                                           0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x26063c0; original cards/prophecy.c:3928. */
int card_ribbon_snake(int player, int card, event_t event)
{
  /* Ribbon Snake	|1|U|U
   * Creature - Snake 2/3
   * Flying
   * |2: This creature loses flying until end of turn. Any player may activate this ability. */

  int p;
  bool legacy_found;
  card_instance_t *instance;
  int effect_card;
  card_instance_t *legacy_record;
  int card_00;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {

      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      do
      {
        card_00 = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, card_00);

            if (((legacy_record != NULL) && (is_what(p, card_00, 0x80) != 0)) &&
                ((legacy_record = get_card_instance(p, card_00),
                  legacy_record->info_slot_fn == ribbon_snake_legacy &&
                      ((legacy_record->targets[0].player == player &&
                        (legacy_record->targets[0].card == card))))))
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
        create_legacy_activate_for_opponent(player, card, ribbon_snake_legacy, player, card);
      }
    }
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_CLEANUP)
    return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
  if (event == EVENT_ACTIVATE)
  {
    instance = get_card_instance(player, card);
    ai_modifier =
        ai_modifier + (unsigned int)(((instance->regen_status & 0x20) == 0) ? -96 : 0) + 0x30;
    return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
    return 0;

  instance = get_card_instance(player, card);
  effect_card =
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                             instance->parent_controller, instance->parent_card, 0, 0, 0x20, 0);
  if (effect_card != -1)
  {
    instance = get_card_instance(instance->parent_controller, effect_card);
    instance->targets[4].player = 0;
  }
  return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x2606670; original cards/prophecy.c:3984. */
int card_root_cage(int player, int card, event_t event)
{
  /* Root Cage	|1|G
   * Enchantment
   * Mercenaries don't untap during their controllers' untap steps. */

  int result;

  if ((current_phase == 1) && (event == EVENT_UNTAP))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      result = is_what(affected_card_controller, affected_card, 2);
      if (result != 0)
      {
        result = has_subtype(affected_card_controller, affected_card, SUBTYPE_MERCENARY);
        if (result != 0)
        {
          does_not_untap(affected_card_controller, affected_card, EVENT_UNTAP);
        }
      }
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25f92c0; original cards/prophecy.c:3999. */
static int samite_sanctuary_legacy(int player, int card, event_t event)
{
  int p;
  int target_player;
  card_instance_t *instance;
  int source_card_or_id;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_card_or_id = get_id(p, instance->targets[0].card);
        if (source_card_or_id != 0x1313)
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
      source_card_or_id = instance->targets[0].card;
      target_player = instance->targets[0].player;
      if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
      {
        default_target_definition(target_player, source_card_or_id, &td, 0xff);
        td.extra = damage_card;
        td.special = TARGET_SPECIAL_DAMAGE_CREATURE;
        if (event == EVENT_CAN_ACTIVATE)
        {
          if (generic_activated_ability(target_player, source_card_or_id, EVENT_CAN_ACTIVATE, 0x4000004,
                                        2, 0, 0, 0, 0, 0, 0, &td, "TARGET_DAMAGE") == 0)
          {
            return 0;
          }
          return 99;
        }
      }
      else
      {
        if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        {
          return 0;
        }
        default_target_definition(target_player, source_card_or_id, &td, 0xff);
      }
      td.extra = damage_card;
      td.special = TARGET_SPECIAL_DAMAGE_CREATURE;
      if (event == EVENT_ACTIVATE)
      {
        instance->number_of_targets = '\x01';
        if (new_pick_target(&td, "TARGET_DAMAGE", 0, 1) != 0)
        {
          return granted_generic_activated_ability(target_player, source_card_or_id, player, card,
                                                   EVENT_ACTIVATE, 0, 2, 0, 0, 0, 0, 0, 0, NULL,
                                                   NULL);
        }
      }
      else if (event == EVENT_RESOLVE_ACTIVATION)
      {

        instance = get_card_instance(target_player, source_card_or_id);
        if (validate_target(player, card, &td, 1) != 0)
        {
          prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
        }
      }
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x2606730; original cards/prophecy.c:4053. */
int card_samite_sanctuary(int player, int card, event_t event)
{
  /* Samite Sanctuary	|2|W
   * Enchantment
   * |2: Prevent the next 1 damage that would be dealt to target creature this turn. Any player may activate this ability. */

  int p;
  bool legacy_found;
  card_instance_t *instance;
  int valid;
  card_instance_t *legacy_record;
  int card_00;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);

      p = 0;
      legacy_found = false;
      do
      {
        card_00 = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, card_00);

            if (((legacy_record != NULL) && (is_what(p, card_00, 0x80) != 0)) &&
                ((legacy_record = get_card_instance(p, card_00),
                  legacy_record->info_slot_fn == samite_sanctuary_legacy &&
                      ((legacy_record->targets[0].player == player &&
                        (legacy_record->targets[0].card == card))))))
            {
              if (player == p)
              {
                kill_card(player, card_00, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                  return global_enchantment(player, card, event);
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
        create_legacy_activate_for_opponent(player, card, samite_sanctuary_legacy, player, card);
      }
    }
  }
  else if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
            (event == EVENT_RESOLVE_ACTIVATION)) ||
           (event == EVENT_CLEANUP))
  {
    default_target_definition(player, card, &td, 0xff);
    td.extra = damage_card;
    td.special = TARGET_SPECIAL_DAMAGE_CREATURE;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) &&
        (valid = validate_target(player, card, &td, 0), valid != 0))
    {
      prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
    }
    return generic_activated_ability(player, card, event, 0x1000004, 2, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_DAMAGE");
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x26069b0; original cards/prophecy.c:4108. */
int card_scoria_cat(int player, int card, event_t event)
{
  /* Scoria Cat	|3|R|R
   * Creature - Cat 3/3
   * This creature gets +3/+3 as long as you control no untapped lands. */

  int result;

  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = control_an_untapped_land(player);
    if (result == 0)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        event_result = event_result + 3;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2606a20; original cards/prophecy.c:4127. */
int card_sheltering_prayers(int player, int card, event_t event)
{
  /* Sheltering Prayers	|W
   * Enchantment
   * Basic lands each player controls have shroud as long as that player controls three or fewer lands. (They can't be the targets of spells or abilities.) */

  int result;
  card_instance_t *instance;

  if (((event == EVENT_ABILITIES) && (instance = in_play(player, card), instance != NULL)) &&
      (result = is_humiliated(player, card, AL_STATIC), result == 0))
  {
    if (((affected_card_controller == player) &&
         (result = is_basic_land(affected_card_controller, affected_card), result != 0)) &&
        (result = count_permanents_by_type(affected_card_controller, TYPE_LAND), result < 4))
    {
      event_result = event_result | 0x20000;
    }
    if (((1 - player == affected_card_controller) &&
         (result = is_basic_land(affected_card_controller, affected_card), result != 0)) &&
        (result = count_permanents_by_type(affected_card_controller, TYPE_LAND), result < 4))
    {
      event_result = event_result | 0x20000;
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2606b30; original cards/prophecy.c:4150. */
int card_shrouded_serpent(int player, int card, event_t event)
{
  /* Shrouded Serpent	|4|U|U|U
   * Creature - Serpent 4/4
   * Whenever this creature attacks, defending player may pay |4. If that player doesn't, this creature can't be blocked this turn. */

  int result;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if ((event == EVENT_DAT_ABILITY) &&
      (((result = has_mana(1 - current_turn, 7, 4),
         result == 0 || (result = do_dialog(1 - current_turn, player, card, -1, -1,
                                            " Pay 4 or this ublocked\n Decline", 0),
                         result != 0)) ||
        (result = charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, 1 - current_turn,
                                              COLOR_ANY, 4),
         result == 0))))
  {
    pump_ability_until_eot_no_repeat_alt(player, card, player, card, 0, 0x10, 1);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2606c50; original cards/prophecy.c:4173. */
int card_silt_crawler(int player, int card, event_t event)
{
  /* Silt Crawler	|2|G
   * Creature - Beast 3/3
   * When this creature enters, tap all lands you control. */

  test_definition_t this_test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  default_test_definition(&this_test, 1);
  new_manipulate_all(player, card, player, &this_test, ACT_TAP);
  return 0;
}

/* ManalinkEh 2022: 0x25fb260; original cards/prophecy.c:4190. */
int gain_2_life_unless_they_pay_2(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  if (event == EVENT_RESOLVE_TRIGGER)
  {

    instance = get_card_instance(player, card);
    result = has_mana((int)instance->damage_target_player, 7, 2);
    if ((result == 0) ||
        (result = charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER,
                                              (int)instance->damage_target_player, COLOR_ANY, 2),
         result == 0))
    {
      gain_life(player, 2);
    }
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x25f8e00; original cards/prophecy.c:4209. */
void soul_charmer_effect(intptr_t player, intptr_t card, int t_player, int t_card)
{
  create_targetted_legacy_effect(player, card, gain_2_life_unless_they_pay_2, t_player, t_card);
  return;
}

/* ManalinkEh 2022: 0x2606ce0; original cards/prophecy.c:4212. */
int card_soul_charmer(int player, int card, event_t event)
{
  /* Soul Charmer	|2|W
   * Creature - Human Rebel 2/2
   * Whenever this creature deals combat damage to a creature, you gain 2 life unless that creature's controller pays |2. */

  if (is_humiliated(player, card, AL_TRIG))
  {
    return 0;
  }
  for_each_creature_damaged_by_me(player, card, event, 4, soul_charmer_effect, player, card);
  return 0;
}

/* ManalinkEh 2022: 0x2606d50; original cards/prophecy.c:4224. */
int card_soul_strings(int player, int card, event_t event)
{
  /* Soul Strings	|X|B
   * Sorcery
   * Return two target creature cards from your graveyard to your hand unless any player pays |X. */

  bool unpaid;
  int result;
  card_instance_t *instance;
  unsigned int pass;
  unsigned int argument1;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    result = count_graveyard_by_type(player, 2);
    if (result < 2)
    {
      return 0;
    }
    result = graveyard_has_shroud(player);
    return (unsigned int)(result == 0);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      new_default_test_definition(&test, 2, "Select two target creature cards.");
      instance = get_card_instance(player, card);
      select_multiple_cards_from_graveyard(player, player, -1, 1, &test, 2, instance->targets);
    }
    return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
  }
  if (event != EVENT_RESOLVE_SPELL)
    return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);

  instance = get_card_instance(player, card);
  unpaid = true;
  pass = 0;
  do
  {
    argument1 = current_turn ^ pass;
    result = has_mana(argument1, 7, instance->info_slot);

    if ((((result != 0) && (unpaid)) &&
         (result = do_dialog(argument1, player, card, -1, -1, " Pay x or cards back to hand\n Pass",
                             (unsigned int)(player == argument1)),
          result == 0)) &&
        (charge_mana_while_resolving(player, card, EVENT_RESOLVE_SPELL, argument1, COLOR_COLORLESS,
                                     instance->info_slot),
         spell_fizzled != 1))
    {
      kill_card(player, card, KILL_DESTROY);
      unpaid = false;
    }
    pass = pass + 1;
  } while (pass != 2);
  if (!unpaid)
    return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);

  result = validate_target_from_grave(player, card, player, 0);
  if (result == -1)
  {
    result = validate_target_from_grave(player, card, player, 1);
    if (result == -1)
    {
      spell_fizzled = 1;
    }
    else
    {
      from_grave_to_hand(player, result, 1);
    }
  }
  else
  {
    from_grave_to_hand(player, result, 1);
    result = validate_target_from_grave(player, card, player, 1);
    if (result != -1)
      from_grave_to_hand(player, result, 1);
  }
  kill_card(player, card, KILL_DESTROY);
  return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x2607050; original cards/prophecy.c:4277. */
int card_spur_grappler(int player, int card, event_t event)
{
  /* Spur Grappler	|2|R
   * Creature - Beast 2/1
   * This creature gets +2/+1 as long as you control no untapped lands. */

  int result;

  if (event == EVENT_POWER)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = control_an_untapped_land(player);
      if (result == 0)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {
          event_result = event_result + 2;
        }
      }
    }
  }
  else if (((event == EVENT_TOUGHNESS) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    result = control_an_untapped_land(player);
    if (result == 0)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        event_result = event_result + 1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2607110; original cards/prophecy.c:4292. */
int card_stormwatch_eagle(int player, int card, event_t event)
{
  /* Stormwatch Eagle	|3|U
   * Creature - Bird 2/1
   * Flying
   * Sacrifice a land: Return this creature to its owner's hand. */

  card_instance_t *instance;
  generic_activated_ability_flags_t flags;
  int result;
  test_definition_t test;

  if (event == EVENT_ACTIVATE)
  {
    new_default_test_definition(&test, 1, "Select a land to sacrifice.");
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    new_default_test_definition(&test, 1, "Select a land to sacrifice.");
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      bounce_permanent(instance->parent_controller, instance->parent_card);
    }
  }
  if (ai_is_speculating == 1)
  {
    flags = GAA_ONCE_PER_TURN;
  }
  else
  {
    flags = GAA_NONE;
    if (player == 1)
    {
      flags = (unsigned int)(((unsigned char)trace_mode & 2) == 0) << 0xd;
    }
  }
  result = generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, flags, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test, SARV_0);
  return result;
}

/* ManalinkEh 2022: 0x2607280; original cards/prophecy.c:4315. */
int card_sunken_field(int player, int card, event_t event)
{
  /* Sunken Field	|1|U
   * Enchantment - Aura
   * Enchant land
   * Enchanted land has "|T: Counter target spell unless its controller pays |1." */

  target_definition_t td;
  card_instance_t *instance = in_play(player, card);
  if (instance && (instance = get_card_instance(player, card), instance->damage_target_player >= 0))
  {
    instance = get_card_instance(player, card);
    if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE &&
        event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
      return 0;
    int attached_player = instance->damage_target_player;
    int attached_card = instance->damage_target_card;
    counterspell_target_definition(attached_player, attached_card, &td, 0xff);
    if (event == EVENT_CAN_ACTIVATE)
      return granted_generic_activated_ability(player, card, attached_player, attached_card, event,
                                               0x401, 0, 0, 0, 0, 0, 0, 0, &td, NULL);
    if (event == EVENT_ACTIVATE)
    {
      card_instance_t *land = get_card_instance(attached_player, attached_card);
      land->targets[0].player = card_on_stack_controller;
      land->targets[0].card = card_on_stack;
      instance->number_of_targets = 1;
      return granted_generic_activated_ability(player, card, attached_player, attached_card, event,
                                               0x401, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
      counterspell_resolve_unless_pay_x(player, card, &td, 0, 1);
  }

  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;

  default_target_definition(player, card, &td, 1);
  td.preferred_controller = player;
  return targeted_aura(player, card, event, &td, "TARGET_LAND");
}

/* ManalinkEh 2022: 0x25f9620; original cards/prophecy.c:4367. */
static int task_mage_assembly_legacy(int player, int card, event_t event)
{
  int p;
  int granting_card;
  card_instance_t *instance;
  int source_player_or_valid;
  int activation_flags;
  event_t event_00;
  target_definition_t *td_00;
  const char *prompt;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_player_or_valid = get_id(p, instance->targets[0].card);
        if (source_player_or_valid != 0x1324)
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
      if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
      {
        td_00 = &td;
        source_player_or_valid = instance->targets[0].player;
        granting_card = instance->targets[0].card;
        default_target_definition(player, card, td_00, 2);
        if (event == EVENT_CAN_ACTIVATE)
        {
          prompt = "TARGET_CREATURE";
          activation_flags = 0x14;
          event_00 = EVENT_CAN_ACTIVATE;
          return granted_generic_activated_ability(source_player_or_valid, granting_card, player,
                                                   card, event_00, activation_flags, 2, 0, 0, 0, 0,
                                                   0, 0, td_00, prompt);
        }
        if (event == EVENT_ACTIVATE)
        {
          instance->number_of_targets = '\x01';
          if (new_pick_target(td_00, "TARGET_CREATURE", 0, 0) == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          prompt = NULL;
          td_00 = NULL;
          activation_flags = 0x10;
          event_00 = EVENT_ACTIVATE;
          return granted_generic_activated_ability(source_player_or_valid, granting_card, player,
                                                   card, event_00, activation_flags, 2, 0, 0, 0, 0,
                                                   0, 0, td_00, prompt);
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
      if ((event == EVENT_RESOLVE_ACTIVATION) &&
          (source_player_or_valid = validate_target(player, card, &td, 0),
           source_player_or_valid != 0))
      {
        damage_target0(player, card, 1);
      }
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x2607580; original cards/prophecy.c:4420. */
int card_task_mage_assembly(int player, int card, event_t event)
{
  /* Task Mage Assembly	|2|R
   * Enchantment
   * When there are no creatures on the battlefield, sacrifice this enchantment.
   * |2: This enchantment deals 1 damage to target creature. Any player may activate this ability but only as a sorcery. */

  int p;
  bool legacy_found;
  int creatures_or_valid;
  card_instance_t *instance;
  card_instance_t *legacy_record;
  int card_00;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {

      if (count_subtype(2, 2, -1) == 0)
      {
        kill_card(player, card, KILL_SACRIFICE);
      }

      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      do
      {
        card_00 = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, card_00);

            if (((legacy_record != NULL) && (is_what(p, card_00, 0x80) != 0)) &&
                ((legacy_record = get_card_instance(p, card_00),
                  legacy_record->info_slot_fn == task_mage_assembly_legacy &&
                      ((legacy_record->targets[0].player == player &&
                        (legacy_record->targets[0].card == card))))))
            {
              if (player == p)
              {
                kill_card(player, card_00, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                {
                  count_subtype(2, 2, -1);
                  return global_enchantment(player, card, event);
                }
                legacy_found = true;
              }
            }
            card_00 = card_00 + 1;
          } while (card_00 <= active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
      creatures_or_valid = count_subtype(2, 2, -1);
      if (((!legacy_found) && (creatures_or_valid != 0)) && (instance->targets[9].card != 0x42))
      {
        create_legacy_activate_for_opponent(player, card, task_mage_assembly_legacy, player, card);
      }
    }
  }
  else if (((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
           ((event == EVENT_RESOLVE_ACTIVATION || (event == EVENT_CLEANUP))))
  {
    default_target_definition(player, card, &td, 2);
    if ((event == EVENT_RESOLVE_ACTIVATION) &&
        (creatures_or_valid = valid_target(&td), creatures_or_valid != 0))
    {
      damage_target0(player, card, 1);
    }
    return generic_activated_ability(player, card, event, 0x14, 2, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2607880; original cards/prophecy.c:4476. */
int card_thresher_beast(int player, int card, event_t event)
{
  /* Thresher Beast	|3|G|G
   * Creature - Beast 4/4
   * Whenever this creature becomes blocked, defending player sacrifices a land of their choice. */

  int result;

  if (event == EVENT_DECLARE_BLOCKERS)
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
          player_sacrifices_a_permanent(player, card, 1 - current_turn, 1,
                                        SAC_CAUSED | SAC_NO_CANCEL);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2607920; original cards/prophecy.c:4488. */
int card_trenching_steed(int player, int card, event_t event)
{
  /* Trenching Steed	|3|W
   * Creature - Horse Rebel 2/3
   * Sacrifice a land: This creature gets +0/+3 until end of turn. */

  int result;
  result = generic_husk(player, card, event, 1, 0, 3, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x2607970; original cards/prophecy.c:4515. */
int card_troubled_healer(int player, int card, event_t event)
{
  /* Troubled Healer	|2|W
   * Creature - Human Cleric 1/2
   * Sacrifice a land: Prevent the next 2 damage that would be dealt to any target this turn. */

  if (event != EVENT_ACTIVATE && (uint32_t)(event - EVENT_RESOLVE_ACTIVATION) > 1 && event != EVENT_CLEANUP)
    return 0;
  test_definition_t test;
  target_definition_t td;
  new_default_test_definition(&test, 1, "Select a land to sacrifice.");
  default_target_definition(player, card, &td, 0);
  td.extra = damage_card;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION)
    prevent_damage(instance->targets[0].player, instance->targets[0].card, 2);
  generic_activated_ability_flags_t flags = GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET;
  if ((ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0)) &&
      count_subtype(player, 1, -1) < 5)
    flags |= GAA_ONCE_PER_TURN;
  return generic_activated_ability_with_sacrifice_as_cost(player, card, event, flags, 0, 0, 0, 0, 0,
                                                          0, 0, &td, "TARGET_DAMAGE", TYPE_NONE,
                                                          &test, SARV_0);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2607b60; original cards/prophecy.c:4542. */
int card_verdant_field(int player, int card, event_t event)
{
  /* Verdant Field	|2|G
   * Enchantment - Aura
   * Enchant land
   * Enchanted land has "|T: Target creature gets +1/+1 until end of turn." */

  int attached_card;
  card_instance_t *instance;
  int p;
  int flags_or_valid;
  card_instance_t *land_record;
  event_t event_00;
  target_definition_t *td;
  const char *prompt;
  target_definition_t td_la;
  if (is_humiliated(player, card, AL_STATIC) == 0)
  {
    instance = get_card_instance(player, card);
    td = &td_la;
    if (-1 < instance->damage_target_player)
    {
      instance = get_card_instance(player, card);
      p = (int)instance->damage_target_player;
      attached_card = instance->damage_target_card;
      default_target_definition(p, attached_card, td, 2);
      td_la.preferred_controller = player;
      if (event == EVENT_CAN_ACTIVATE)
      {
        prompt = "TARGET_CREATURE";
        flags_or_valid = 5;
        event_00 = EVENT_CAN_ACTIVATE;
        return granted_generic_activated_ability(player, card, p, attached_card, event_00,
                                                 flags_or_valid, 0, 0, 0, 0, 0, 0, 0, td,
                                                 (char *)prompt);
      }
      if (event == EVENT_ACTIVATE)
      {
        instance->number_of_targets = '\0';
        if (pick_target(td, "TARGET_CREATURE") != 0)
        {
          prompt = NULL;
          td = NULL;
          flags_or_valid = 1;
          event_00 = EVENT_ACTIVATE;
          return granted_generic_activated_ability(player, card, p, attached_card, event_00,
                                                   flags_or_valid, 0, 0, 0, 0, 0, 0, 0, td,
                                                   (char *)prompt);
        }
      }
      else if ((event == EVENT_RESOLVE_ACTIVATION) &&
               (flags_or_valid = validate_target(p, attached_card, td, 0),
                flags_or_valid != 0))
      {
        land_record = get_card_instance(p, attached_card);
        pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                               land_record->targets[0].player, land_record->targets[0].card, 1, 1,
                               0, 0);
      }
    }
  }
  default_target_definition(player, card, &td_la, 1);
  td_la.preferred_controller = player;
  return aura_literal_prompt(player, card, event, &td_la, "Select target land.");
}
#endif

/* ManalinkEh 2022: 0x25f9900; original cards/prophecy.c:4585. */
static int vintara_elephant_legacy(int player, int card, event_t event)
{
  int p;
  int target_card;
  card_instance_t *instance;
  int flags_or_effect;
  int source_id;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_id = get_id(p, instance->targets[0].card);
        if (source_id != 0x132c)
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
    return 0;
  }
  instance = get_card_instance(player, card);
  if (instance->targets[0].player == -1)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  p = instance->targets[0].player;
  target_card = instance->targets[0].card;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_ACTIVATE)
  {
    instance = get_card_instance(p, target_card);
    ai_modifier =
        ai_modifier + (unsigned int)(((instance->regen_status & 0x80) == 0) ? -96 : 0) + 0x30;
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    flags_or_effect = pump_ability_until_eot(p, target_card, p, target_card, 0, 0, 0x80, 0);
    if (flags_or_effect != -1)
    {
      instance = get_card_instance(p, flags_or_effect);
      instance->targets[4].player = 0;
    }
  }
  flags_or_effect = 1;
  if (ai_is_speculating != 1)
  {
    flags_or_effect = 0x2000;
    if ((trace_mode & 2U) == 0 && player == 0)
      return granted_generic_activated_ability(p, target_card, player, card, event, flags_or_effect, 3,
                                               0, 0, 0, 0, 0, 0, NULL, NULL);
    flags_or_effect = 0;
  }
  flags_or_effect = (unsigned int)(1 - player == flags_or_effect) << 0xd;
  return granted_generic_activated_ability(p, target_card, player, card, event, flags_or_effect, 3, 0,
                                           0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x2607dd0; original cards/prophecy.c:4625. */
int card_vintara_elephant(int player, int card, event_t event)
{
  /* Vintara Elephant	|4|G
   * Creature - Elephant 4/3
   * Trample
   * |3: This creature loses trample until end of turn. Any player may activate this ability. */

  int p;
  bool legacy_found;
  card_instance_t *instance;
  int effect_card;
  card_instance_t *legacy_record;
  int card_00;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {

      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      do
      {
        card_00 = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, card_00);

            if (((legacy_record != NULL) && (is_what(p, card_00, 0x80) != 0)) &&
                ((legacy_record = get_card_instance(p, card_00),
                  legacy_record->info_slot_fn == vintara_elephant_legacy &&
                      ((legacy_record->targets[0].player == player &&
                        (legacy_record->targets[0].card == card))))))
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
        create_legacy_activate_for_opponent(player, card, vintara_elephant_legacy, player, card);
      }
    }
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_CLEANUP)
    return generic_activated_ability(player, card, event, 0, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
  if (event == EVENT_ACTIVATE)
  {
    instance = get_card_instance(player, card);
    ai_modifier =
        ai_modifier + (unsigned int)(((instance->regen_status & 0x80) == 0) ? -96 : 0) + 0x30;
    return generic_activated_ability(player, card, event, 0, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
    return 0;

  instance = get_card_instance(player, card);
  effect_card =
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                             instance->parent_controller, instance->parent_card, 0, 0, 0x80, 0);
  if (effect_card != -1)
  {
    instance = get_card_instance(instance->parent_controller, effect_card);
    instance->targets[4].player = 0;
  }
  return generic_activated_ability(player, card, event, 0, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x2608080; original cards/prophecy.c:4681. */
int card_vintara_snapper(int player, int card, event_t event)
{
  /* Vintara Snapper	|G|G
   * Creature - Turtle 2/2
   * This creature has shroud as long as you control no untapped lands. (It can't be the target of spells or abilities.) */

  int result;

  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = control_an_untapped_land(player);
    if (result == 0)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        event_result = event_result | 0x20000;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25fad80; original cards/prophecy.c:4693. */
static int wall_of_vipers_legacy(int player, int card, event_t event)
{
  int p;
  int granting_card;
  card_instance_t *instance;
  int valid;
  int source_player_or_result;
  target_definition_t *blocking_target;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_player_or_result = get_id(p, instance->targets[0].card);
        if (source_player_or_result != 0x132f)
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
      source_player_or_result = instance->targets[0].player;
      granting_card = instance->targets[0].card;
      if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
      {
        default_target_definition(player, card, &td, 2);
        if (event == EVENT_CAN_ACTIVATE)
        {
          if (current_phase != 0x18)
          {
            return 0;
          }
          if (current_turn == source_player_or_result)
          {
            return 0;
          }
          if (instance->blocking == 0xff)
          {
            return 0;
          }
          if (would_validate_arbitrary_target(&td, 1 - source_player_or_result,
                                              (unsigned int)instance->blocking) == 0)
          {
            return 0;
          }
          return granted_generic_activated_ability(source_player_or_result, granting_card, player,
                                                   card, EVENT_CAN_ACTIVATE, 4, 3, 0, 0, 0, 0, 0, 0,
                                                   &td, "TARGET_CREATURE");
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
      blocking_target = &td;
      if (event == EVENT_ACTIVATE)
      {
        if ((((current_phase == 0x18) && (current_turn != source_player_or_result)) &&
             ((instance->blocking != 0xff &&
               ((valid =
                     would_validate_arbitrary_target(blocking_target, 1 - source_player_or_result,
                                                     (unsigned int)instance->blocking),
                 valid != 0 &&
                     (source_player_or_result = granted_generic_activated_ability(
                          source_player_or_result, granting_card, player, card, EVENT_ACTIVATE, 4,
                          3, 0, 0, 0, 0, 0, 0, blocking_target, "TARGET_CREATURE"),
                      source_player_or_result != 0)))))) &&
            (source_player_or_result = new_pick_target(
                 blocking_target, "Select target creature Wall of Vipers is blocking.", 0, 0x80),
             source_player_or_result != 0))
        {
          instance->info_slot = 1;
          instance->number_of_targets = '\x01';
          instance->targets[0].player = 1 - player;
          instance->targets[0].card = (unsigned int)instance->blocking;
        }
      }
      else if (((event == EVENT_RESOLVE_ACTIVATION) &&
                (kill_card(player, card, KILL_DESTROY), 0 < instance->info_slot)) &&
               (source_player_or_result = valid_target(blocking_target),
                source_player_or_result != 0))
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x26080e0; original cards/prophecy.c:4761. */
int card_wall_of_vipers(int player, int card, event_t event)
{
  /* Wall of Vipers	|2|B
   * Creature - Snake Wall 2/4
   * Defender (This creature can't attack.)
   * |3: Destroy this creature and target creature it's blocking. Any player may activate this ability. */

  int p;
  unsigned char blocked_card;
  bool legacy_found;
  card_instance_t *instance;
  int available_or_valid;
  card_instance_t *legacy_record;
  int card_00;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {

      if (count_subtype(2, 2, -1) == 0)
      {
        kill_card(player, card, KILL_SACRIFICE);
      }

      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      bool scan_stopped = false;
      do
      {
        card_00 = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, card_00);

            if (((legacy_record != NULL) && (is_what(p, card_00, 0x80) != 0)) &&
                ((legacy_record = get_card_instance(p, card_00),
                  legacy_record->info_slot_fn == wall_of_vipers_legacy &&
                      ((legacy_record->targets[0].player == player &&
                        (legacy_record->targets[0].card == card))))))
            {
              if (player == p)
              {
                kill_card(player, card_00, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                {
                  scan_stopped = true;
                  break;
                }
                legacy_found = true;
              }
            }
            card_00 = card_00 + 1;
          } while (card_00 <= active_cards_count[p]);
        }
        if (scan_stopped)
          break;
        p = p + 1;
      } while (p != 2);
      if ((!scan_stopped && !legacy_found) && (instance->targets[9].card != 0x42))
      {
        create_legacy_activate_for_opponent(player, card, wall_of_vipers_legacy, player, card);
      }
    }
    default_target_definition(player, card, &td, 2);
  }
  else
  {
    default_target_definition(player, card, &td, 2);
    if (event == EVENT_CAN_ACTIVATE)
    {

      instance = get_card_instance(player, card);
      if ((((current_phase == 0x18) && (current_turn != player)) && (instance->blocking != 0xff)) &&
          (available_or_valid =
               would_validate_arbitrary_target(&td, 1 - player, (unsigned int)instance->blocking),
           available_or_valid != 0))
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 3, 0, 0, 0, 0, 0, 0,
                                         NULL, NULL);
      }
    }
    else if (event == EVENT_ACTIVATE)
    {

      instance = get_card_instance(player, card);
      if (((current_phase == 0x18) && (current_turn != player)) && (instance->blocking != 0xff))
      {
        available_or_valid =
            would_validate_arbitrary_target(&td, 1 - player, (unsigned int)instance->blocking);
        if (((available_or_valid != 0) && (available_or_valid = charge_mana_for_activated_ability(
                                               player, card, 3, 0, 0, 0, 0, 0),
                                           available_or_valid != 0)) &&
            (available_or_valid = new_pick_target(
                 &td, "Select target creature Wall of Vipers is blocking.", 0, 0x80),
             available_or_valid != 0))
        {
          blocked_card = instance->blocking;
          instance->targets[0].player = 1 - player;
          instance->targets[0].card = (unsigned int)blocked_card;
          instance->info_slot = 1;
          instance->number_of_targets = '\x01';
        }
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {

      instance = get_card_instance(player, card);
      kill_card(player, card, KILL_DESTROY);
      if ((0 < instance->info_slot) &&
          (available_or_valid = valid_target(&td), available_or_valid != 0))
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        return 0;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x26084e0; original cards/prophecy.c:4842. */
int card_whipstitched_zombie(int player, int card, event_t event)
{
  /* Whipstitched Zombie	|1|B
   * Creature - Zombie 2/2
   * At the beginning of your upkeep, sacrifice this creature unless you pay |B. */

  basic_upkeep(player, card, event, 0, 1, 0, 0, 0, 0);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25fb3e0; original cards/prophecy.c:4852. */
int bounce_at_end_of_combat(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;
  result = end_of_combat_trigger(player, card, event, 2);
  if (result != 0)
  {
    instance = get_card_instance(player, card);
    result =
        check_state((int)instance->damage_target_player, instance->damage_target_card, 0x800000);
    if (result == 0)
    {
      bounce_permanent((int)instance->damage_target_player, instance->damage_target_card);
    }
  }
  if ((((trigger_condition == TRIGGER_END_COMBAT) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((reason_for_trigger_controller == player && (event == EVENT_END_TRIGGER))))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2608540; original cards/prophecy.c:4867. */
int card_windscouter(int player, int card, event_t event)
{
  /* Windscouter	|3|U
   * Creature - Human Scout 3/3
   * Flying
   * When this creature attacks or blocks, return it to its owner's hand at end of combat. (Return it only if it's on the battlefield.) */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY &&
      (!blocking(player, card, event) || is_humiliated(player, card, AL_TRIG)))
    return 0;
  create_targetted_legacy_effect(player, card, bounce_at_end_of_combat, player, card);
  return 0;
}
#endif

/* ManalinkEh 2022: 0x26085e0; original cards/prophecy.c:4881. */
int card_wing_storm(int player, int card, event_t event)
{
  /* Wing Storm	|2|G
   * Sorcery
   * ~ deals damage to each player equal to twice the number of creatures that player controls with flying. */

  int result;
  unsigned int pass;
  unsigned int t_player;
  test_definition_t this_test;

  if (event == EVENT_RESOLVE_SPELL)
  {

    pass = 0;
    default_test_definition(&this_test, 2);
    this_test.keyword = 0x20;
    do
    {

      t_player = current_turn ^ pass;
      result = (int)((unsigned int)check_battlefield_for_special_card(
                         player, card, t_player, 4, &this_test) * 2U);
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

/* ManalinkEh 2022: 0x26086b0; original cards/prophecy.c:4905. */
int card_wintermoon_mesa(int player, int card, event_t event)
{
  /* Wintermoon Mesa	""
   * Land
   * This land enters tapped.
   * |T: Add |C.
   * |2, |T, Sacrifice this land: Tap two target lands. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_TAP_LANDS = 2
  };

  int i;
  int choice;
  int available;
  card_instance_t *instance;
  bool not_speculating;
  target_definition_t *land_target;
  target_definition_t td;

  comes_into_play_tapped(player, card, event);
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      default_target_definition(player, card, &td, 1);
      instance = get_card_instance(player, card);

      available = paying_mana();
      if ((available == 0) &&
          (available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xd, 3, 0, 0, 0,
                                                 0, 0, 0, &td, NULL),
           available != 0))
      {
        not_speculating = ai_is_speculating != 1;
        instance->info_slot = 2;
        if ((not_speculating) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
        {
          choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Get mana", 1,
                               1, "Tap 2 target lands", 1, 1, 0, 0, 0, 0);
          if (choice == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          if (choice == CHOICE_MANA)
            return mana_producer(player, card, EVENT_ACTIVATE);
          if (choice != CHOICE_TAP_LANDS)
          {
            return 0;
          }
        }
        add_state(player, card, 0x10);
        generic_activated_ability(player, card, EVENT_ACTIVATE, 4, 2, 0, 0, 0, 0, 0, 0, &td,
                                  "TARGET_LAND");
        if (spell_fizzled == 1)
        {
          remove_state(player, card, 0x10);
        }
        else
        {
          state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
          if (new_pick_target(&td, "TARGET_LAND", 1, 1) == 0)
          {
            state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
            remove_state(player, card, 0x10);
            return 0;
          }
          kill_card(player, card, KILL_SACRIFICE);
          state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
          tapped_for_mana_color = -1;
        }
        return 0;
      }
      instance->info_slot = 1;
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      default_target_definition(player, card, &td, 1);
      instance = get_card_instance(player, card);
      land_target = &td;
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot != 2)
      {
        return 0;
      }

      if (instance->number_of_targets == '\0')
      {
        return 0;
      }
      i = 0;
      do
      {
        if (validate_target(player, card, land_target, i) != 0)
        {
          tap_card(instance->targets[i].player, instance->targets[i].card);
        }
        i = i + 1;
      } while (i < (int)(unsigned int)instance->number_of_targets);
      return 0;
    }
  }
  return mana_producer(player, card, event);
}

/* ManalinkEh 2022: 0x25f9ba0; original cards/prophecy.c:4986. */
static int zerapa_minotaur_legacy(int player, int card, event_t event)
{
  int p;
  int target_card;
  card_instance_t *instance;
  int flags_or_effect;
  int source_id;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_id = get_id(p, instance->targets[0].card);
        if (source_id != 0x1339)
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
    return 0;
  }
  instance = get_card_instance(player, card);
  if (instance->targets[0].player == -1)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  p = instance->targets[0].player;
  target_card = instance->targets[0].card;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_ACTIVATE)
  {
    instance = get_card_instance(p, target_card);
    ai_modifier =
        ai_modifier + (unsigned int)(((instance->regen_status & 0x100) == 0) ? -48 : 0) + 0x18;
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    flags_or_effect = pump_ability_until_eot(p, target_card, p, target_card, 0, 0, 0x100, 0);
    if (flags_or_effect != -1)
    {
      instance = get_card_instance(p, flags_or_effect);
      instance->targets[4].player = 0;
    }
  }
  flags_or_effect = 1;
  if (ai_is_speculating != 1)
  {
    flags_or_effect = 0x2000;
    if ((trace_mode & 2U) == 0 && player == 0)
      return granted_generic_activated_ability(p, target_card, player, card, event, flags_or_effect, 2,
                                               0, 0, 0, 0, 0, 0, NULL, NULL);
    flags_or_effect = 0;
  }
  flags_or_effect = (unsigned int)(1 - player == flags_or_effect) << 0xd;
  return granted_generic_activated_ability(p, target_card, player, card, event, flags_or_effect, 2, 0,
                                           0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x2608ac0; original cards/prophecy.c:5026. */
int card_zerapa_minotaur(int player, int card, event_t event)
{
  /* Zerapa Minotaur	|2|R|R
   * Creature - Minotaur 3/3
   * First strike
   * |2: This creature loses first strike until end of turn. Any player may activate this ability. */

  int p;
  bool legacy_found;
  card_instance_t *instance;
  int effect_card;
  card_instance_t *legacy_record;
  int card_00;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {

      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      do
      {
        card_00 = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, card_00);

            if (((legacy_record != NULL) && (is_what(p, card_00, 0x80) != 0)) &&
                ((legacy_record = get_card_instance(p, card_00),
                  legacy_record->info_slot_fn == zerapa_minotaur_legacy &&
                      ((legacy_record->targets[0].player == player &&
                        (legacy_record->targets[0].card == card))))))
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
        create_legacy_activate_for_opponent(player, card, zerapa_minotaur_legacy, player, card);
      }
    }
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_CLEANUP)
    return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
  if (event == EVENT_ACTIVATE)
  {
    instance = get_card_instance(player, card);
    ai_modifier =
        ai_modifier + (unsigned int)(((instance->regen_status & 0x100) == 0) ? -96 : 0) + 0x30;
    return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
    return 0;

  instance = get_card_instance(player, card);
  effect_card =
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                             instance->parent_controller, instance->parent_card, 0, 0, 0x100, 0);
  if (effect_card != -1)
  {
    instance = get_card_instance(instance->parent_controller, effect_card);
    instance->targets[4].player = 0;
  }
  return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}
