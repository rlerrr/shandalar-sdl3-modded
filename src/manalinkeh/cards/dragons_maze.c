#include "manalink.h"

// Functions

int card_goblin_war_drums(int player, int card, event_t event);

int notion_thief_effect(int player, int card, event_t event);

static void gatekeeper(int player, int card, event_t event, int trig_mode){
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		if( count_subtype(player, TYPE_LAND, SUBTYPE_GATE) >= 2 ){
			etb_ability(player, card, event, trig_mode);
		}
	}
}

// Cards

// White

int card_boros_mastiff(int player, int card, event_t event)
{
  /* CARD_ID_BOROS_MASTIFF	13129
  Boros Mastiff	|1|W
  Creature - Hound 2/2
  Battalion - Whenever ~ and at least two other creatures attack, ~ gains lifelink until end of
  turn. (Damage dealt by a creature with lifelink also causes its controller to gain that much
  life.) */

  battalion(player, card, event);

  if (event == EVENT_DAT_ABILITY)
  {
    pump_ability_until_eot(player, card, player, card, 0, 0, 0, SP_KEYWORD_LIFELINK);
  }
  return 0;
}

int card_haazda_snare_squad(int player, int card, event_t event)
{
/* CARD_ID_HAAZDA_SNARE_SQUAD	13157
Haazda Snare Squad	|2|W
Creature - Human Soldier 1/4
Whenever ~ attacks, you may pay |W. If you do, tap target creature an opponent controls. */

	dat_ability(player, card, event, duh_mode(player) ? DAT_RESOLVE_TRIGGER_MANDATORY : DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
	if (event == EVENT_DAT_ABILITY){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		card_instance_t* instance = get_card_instance(player, card);
		if (charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_WHITE, 1) && pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS")){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
		instance->info_slot = 0;
	}
	return 0;
}

int card_lyev_decree(int player, int card, event_t event){
/* CARD_ID_LYEV_DECREE	13169
Lyev Decree	|1|W
Sorcery
Detain up to two target creatures your opponents control.
(Until your next turn, those creatures can't attack or block and their activated abilities can't be activated.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				detain(player, card, instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature your opponent controls.", 2, NULL);
}

int card_maze_sentinel(int player, int card, event_t event){
/* CARD_ID_MAZE_SENTINEL	13176
Maze Sentinel	|5|W
Creature - Elemental 3/6
Vigilance
Multicolored creatures you control have vigilance. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_ABILITIES && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
		{
			if( affected_card == card || count_colors(affected_card_controller, affected_card) > 1 ){
				vigilance(affected_card_controller, affected_card, event);
			}
		}
	}

	return 0;
}

int card_renounce_the_guilds(int player, int card, event_t event){
/* CARD_ID_RENOUNCE_THE_GUILDS	13206
Renounce the Guilds	|1|W
Instant
Each player sacrifices a multicolored permanent. */

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p,{
					test_definition_t test;
					new_default_test_definition(&test, TYPE_PERMANENT, "Select a multicolored permanent to sacrifice.");
					test.color_flag = F3_MULTICOLORED;
					int sac = new_sacrifice(player, card, p, SAC_CAUSED|SAC_NO_CANCEL|SAC_JUST_MARK|SAC_RETURN_CHOICE, &test);
					if (sac){
						kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
					}

				}
		);
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);

}

static int riot_control_legacy(int player, int card, event_t event)
{
	card_instance_t* damage = damage_being_prevented(event);
	if (damage && damage->damage_target_player == player && damage->damage_target_card == -1 && !damage_is_to_planeswalker(damage))
		prevent_damage(affected_card_controller, affected_card, -1);

	if (event == EVENT_CLEANUP)
		kill_card(player, card, KILL_EXILE);

	return 0;
}

int card_riot_control(int player, int card, event_t event){
/* CARD_ID_RIOT_CONTROL	13208
Riot Control	|2|W
Instant
You gain 1 life for each creature your opponents control. Prevent all damage that would be dealt to you this turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &riot_control_legacy);
		gain_life(player, count_subtype(1-player, TYPE_CREATURE, -1));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_scion_of_vitu_ghazi(int player, int card, event_t event){
	/* Scion of Vitu-Ghazi	|3|W|W
	 * Creature - Elemental 4/4
	 * When ~ enters the battlefield, if you cast it from your hand, put a 1/1 |Swhite Bird creature token with flying onto the battlefield, then populate. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->info_slot = not_played_from_hand(player, card);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( get_card_instance(player, card)->info_slot != 1 ){
			generate_token_by_id(player, card, CARD_ID_BIRD);
			populate(player, card);
		}
	}

	return 0;
}

// steeple rock --> vanilla

int card_sunspire_gatekeeper(int player, int card, event_t event){
/* CARD_ID_SUNSPIRE_GATEKEEPERS	13229
Sunspire Gatekeepers	|3|W
Creature - Human Soldier 2/4
When ~ enters the battlefield, if you control two or more Gates, create a 2/2 |Swhite Knight creature token with vigilance. */

	gatekeeper(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_KNIGHT, &token);
		token.color_forced = COLOR_TEST_WHITE;
		token.pow = token.tou = 2;
		token.s_key_plus = SP_KEYWORD_VIGILANCE;
		generate_token(&token);
	}

	return 0;
}

int card_wake_the_reflections(int player, int card, event_t event){
/* CARD_ID_WAKE_THE_REFLECTIONS	13245
Wake the Reflections	|W
Sorcery
Populate. (Create a token that's a copy of a creature token you control.) */

	if( event == EVENT_RESOLVE_SPELL ){
		populate(player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// Blue

int card_aetherling(int player, int card, event_t event){
/* CARD_ID_AETHERLING	13110
Aetherling	|4|U|U
Creature - Shapeshifter 4/5
|U: Exile ~. Return it to the battlefield under its owner's control at the beginning of the next end step.
|U: ~ can't be blocked this turn.
|1: ~ gets +1/-1 until end of turn.
|1: ~ gets -1/+1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_EXILE = 1,
		CHOICE_UNBLOCKABLE,
		CHOICE_P1_M1,
		CHOICE_M1_P1
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL) ){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int priorities[4] = {	5,
								current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS && ! check_for_special_ability(player, card, SP_KEYWORD_UNBLOCKABLE) ? 15 : 0,
								is_attacking(player, card) && is_unblocked(player, card) ? 10 : 0,
								is_attacking(player, card) && ! is_unblocked(player, card) ? 10 : 0
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
						"Exile until eot", has_mana_for_activated_ability(player, card, MANACOST_U(1)), priorities[0],
						"Unblockable until eot", has_mana_for_activated_ability(player, card, MANACOST_U(1)), priorities[1],
						"+1/-1", has_mana_for_activated_ability(player, card, MANACOST_X(1)), priorities[2],
						"-1/+1", has_mana_for_activated_ability(player, card, MANACOST_X(1)), priorities[3]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_XU(((choice == 3 || choice == 4) ? 1 : 0), ((choice == 1 || choice == 2) ? 1 : 0))) ){
			instance->info_slot = choice;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_EXILE ){
			remove_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card);
		}
		if( instance->info_slot == CHOICE_UNBLOCKABLE ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
		}
		if( instance->info_slot == CHOICE_P1_M1 ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 1, -1);
		}
		if( instance->info_slot == CHOICE_M1_P1 ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, -1, 1);
		}
	}

	return 0;
}

int card_hidden_strings(int player, int card, event_t event){
  /* Hidden Strings |1|U
   * Sorcery
   * You may tap or untap target permanent, then you may tap or untap another target permanent.
   * Cipher (Then you may exile this spell card encoded on a creature you control. Whenever that creature deals combat damage to a player,
   * 		its controller may cast a copy of the encoded card without paying its mana cost.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->info_slot = played_for_free(player, card) | is_token(player, card);
		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_PERMANENT") ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			if( can_target(&td) ){
				new_pick_target(&td, "TARGET_PERMANENT", 1, 0);
			}
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}


	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		int good = 0;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				good++;
				twiddle(player, card, i);
			}
		}
		if( good > 0 ){
			cipher(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_maze_glider(int player, int card, event_t event)
{
  /* CARD_ID_MAZE_GLIDER	13174
   * Maze Glider	|5|U
   * Creature - Elemental 3/5
   * Flying
   * Multicolored creatures you control have flying. */

  card_instance_t *instance;
  int result;
  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (((event == EVENT_ABILITIES) && (result == 0)) && (affected_card_controller == player))
    {
      result = is_what(player, affected_card, 2);
      if (result != 0)
      {
        result = count_colors(affected_card_controller, affected_card);
        if (1 < result)
        {
          event_result = event_result | 0x20;
        }
      }
    }
  }
  return 0;
}

int card_mindstatic(int player, int card, event_t event){
	/* Mindstatic	|3|U
	 * Instant
	 * Counter target spell unless its controller pays |6. */

	if( event == EVENT_CAN_CAST ){
		return counterspell(player, card, event, NULL, 0);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return counterspell(player, card, event, NULL, 0);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, 6);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_opal_lake_gatekeeper(int player, int card, event_t event){
/* CARD_ID_OPAL_LAKE_GATEKEEPERS	13188
Opal Lake Gatekeepers	|3|U
Creature - Vedalken Soldier 2/4
When ~ enters the battlefield, if you control two or more Gates, you may draw a card. */

	gatekeeper(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_runners_bane(int player, int card, event_t event){
/* CARD_ID_RUNNERS_BANE	13212
Runner's Bane	|1|U
Enchantment - Aura
Enchant creature with power 3 or less
When ~ enters the battlefield, tap enchanted creature.
Enchanted creature doesn't untap during its controller's untap step. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			tap_card(instance->damage_target_player, instance->damage_target_card);
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = 3 | TARGET_PT_LESSER_OR_EQUAL;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target creature with power 3 or less.");
}

// trait doctoring --> impossible

int card_uncovered_clues(int player, int card, event_t event){
/* CARD_ID_UNCOVERED_CLUES	13239
Uncovered Clues	|2|U
Sorcery
Look at the top four cards of your library. You may reveal up to two instant and/or sorcery cards from among them and put the revealed cards into your hand.
Put the rest on the bottom of your library in any order. */

	if( event == EVENT_RESOLVE_SPELL ){
		int max = MIN(4, count_deck(player));
		int deck_count = count_deck(player);
		int can_tutor = MIN(2, deck_count);

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_SPELL, "Select an instant or sorcery card.");
		this_test.type_flag = F1_NO_CREATURE;
		this_test.create_minideck = max;
		this_test.no_shuffle = 1;
		this_test.qty = can_tutor;

		int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		if (deck_count == 1){ result = result >= 0; }
		max-=result;
		if( max > 0 ){
			put_top_x_on_bottom(player, player, max);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

// Black

// bane alley blackguard --> vanilla

int card_blood_scrivener(int player, int card, event_t event){
	/* Blood Scrivener	|1|B
	 * Creature - Zombie Wizard 2/1
	 * If you would draw a card while you have no cards in hand, instead draw two cards and lose 1 life. */

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && reason_for_trigger_controller == player &&
		hand_count[player] <= 0 && !suppress_draw )
	{
		card_instance_t* instance = get_card_instance(player, card);
		if (instance->info_slot == 1 || is_humiliated(player, card, AL_STATIC) ){
			return 0;
		}
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				instance->info_slot = 1;	// Prevent from triggering on its own draws
				draw_cards(player, 2);
				lose_life(player, 1);
				instance->info_slot = 0;
				suppress_draw = 1;
		}
	}
	return 0;
}

int card_crypt_incursion(int player, int card, event_t event){
/* CARD_ID_CRYPT_INCURSION	13138
Crypt Incursion	|2|B
Instant
Exile all creature cards from target player's graveyard. You gain 3 life for each card exiled this way. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance( player, card );

			const int *grave = get_grave(instance->targets[0].player);
			int amount = 0;
			int cg = count_graveyard(instance->targets[0].player)-1;
			while( cg > -1 ){
					if( is_what(-1, grave[cg], TYPE_CREATURE) ){
						rfg_card_from_grave(instance->targets[0].player, cg);
						amount+=3;
					}
					cg--;
			}
			gain_life(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_fatal_fumes(int player, int card, event_t event){
	/* Fatal Fumes	|3|B
	 * Instant
	 * Target creature gets -4/-2 until end of turn. */
	return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -4, -2, 0, 0);
}

int card_hired_torturer(int player, int card, event_t event){
/* CARD_ID_HIRED_TORTURER	13160
Hired Torturer	|2|B
Creature - Human Rogue 2/3
Defender
|3|B, |T: Target opponent loses 2 life, then reveals a card at random from his or her hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 2);
			if( hand_count[instance->targets[0].player] > 0 ){
				int t_hand[100];
				t_hand[0] = -1;	// to silence warning
				int th_count = 0;
				int revealed[1];
				int count = 0;
				while( count < active_cards_count[instance->targets[0].player] ){
						if( in_hand(instance->targets[0].player, count) ){
							card_instance_t *crd = get_card_instance( instance->targets[0].player, count );
							t_hand[th_count] = crd->internal_card_id;
							th_count++;
						}
						count++;
				}
				revealed[0] = t_hand[0];
				if( th_count > 1 ){
					revealed[0] = t_hand[internal_rand(th_count)];
				}
				show_deck(HUMAN, revealed, 1, "Hired Torturer revealed...", 0, gs_done_008b40e0 );
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK+GAA_CAN_ONLY_TARGET_OPPONENT, 3, 1, 0, 0, 0, 0, 0, &td, "TARGET_PLAYER");
}

int card_maze_abomination(int player, int card, event_t event)
{
/* CARD_ID_MAZE_ABOMINATION	13172
Maze Abomination	|5|B
Creature - Elemental 4/5
Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.)
Multicolored creatures you control have deathtouch. */

	deathtouch(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_ABILITIES && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
			count_colors(affected_card_controller, affected_card) > 1 )
		{
			deathtouch(affected_card_controller, affected_card, event);
		}
	}

	return 0;
}

int card_pontiff_of_blight(int player, int card, event_t event){
/* CARD_ID_PONTIFF_OF_BLIGHT	13193
Pontiff of Blight	|4|B|B
Creature - Zombie Cleric 2/7
Extort (Whenever you cast a spell, you may pay |WB. If you do, each opponent loses 1 life and you gain that much life.)
Other creatures you control have extort. (If a creature has multiple instances of extort, each triggers separately.) */

	sp_ability_trigger(player, event, RESOLVE_TRIGGER_AI(player), TYPE_ANY, NULL, player, card);
	if (event == EVENT_SPA){
	    int marked[151] = {0};
	    CYCLE_ALL_CARDS(p, c, {
	        if (p == player && in_play(p, c) && is_what(p, c, TYPE_CREATURE) && !is_humiliated(p, c, AL_TRIG)){ marked[c] = 1; }
	    });
	    int i;
	    for (i = 0; i < 151; ++i){
	        if (!has_mana_hybrid(player, COLOR_BLACK, COLOR_WHITE, 1, 0)){ break; }
	        if (marked[i]){
	            charge_mana_hybrid(player, card, COLOR_BLACK, COLOR_WHITE, 1, 0);
	            if (spell_fizzled != 1){ int result = lose_life(1-player, 1); gain_life(player, result); }
	            spell_fizzled = 0;
	        }
	    }
	}
	return 0;
}

// rakdos drake --> dead reveler

int card_sinister_possession(int player, int card, event_t event)
{
/* CARD_ID_SINISTER_POSSESSION	13222
Sinister Possession	|B
Enchantment - Aura
Enchant creature
Whenever enchanted creature attacks or blocks, its controller loses 2 life. */

	card_instance_t* instance = get_card_instance(player, card);
	if (instance->damage_target_player >= 0){
		instance = get_card_instance(player, card);
		int p = instance->damage_target_player, c = instance->damage_target_card;
		store_attackers(player, card, event, 0, p, c, NULL);
		dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
		if (event == EVENT_DAT_ABILITY){ lose_life(instance->damage_target_player, 2); }
		if (blocking(p, c, event) && !is_humiliated(player, card, AL_TRIG)){
			lose_life(instance->damage_target_player, 2);
		}
	}
	return vanilla_aura(player, card, event, 1-player);
}

int card_ubal_sar_gatekeeper(int player, int card, event_t event){
/* CARD_ID_UBUL_SAR_GATEKEEPERS	13238
Ubul Sar Gatekeepers	|3|B
Creature - Zombie Soldier 2/4
When ~ enters the battlefield, if you control two or more Gates, target creature an opponent controls gets -2/-2 until end of turn. */

	gatekeeper(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -2, -2);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

// Red

int card_awe_for_the_guilds(int player, int card, event_t event){
/* CARD_ID_AWE_FOR_THE_GUILDS	13116
Awe for the Guilds	|2|R
Sorcery
Monocolored creatures can't block this turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = 0;
		this_test.color_flag = F3_MONOCOLORED;
		creatures_cannot_block(player, card, &this_test, 1);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_clear_a_path(int player, int card, event_t event){
/* CARD_ID_CLEAR_A_PATH	13136
Clear a Path	|R
Sorcery
Destroy target creature with defender. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_abilities = KEYWORD_DEFENDER;

	if (event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature with defender.", 1, NULL);
}

int card_maze_rusher(int player, int card, event_t event){
/* Maze Rusher	|5|R
 * Creature - Elemental 6/3
 * Haste
 * Multicolored creatures you control have haste. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_ABILITIES && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
		{
			if( (count_colors(affected_card_controller, affected_card) > 1 || affected_card == card) ){
				haste(affected_card_controller, affected_card, event);
			}
		}
	}

	return 0;
}

int card_possibility_storm(int player, int card, event_t event){
  /* Possibility Storm |3|R|R
   *   Enchantment
   *   Whenever a player casts a spell from his or her hand, that player exiles it, then exiles cards from the top of his or her library until
   *   he or she exiles a card that shares a card type with it. That player may cast that card without paying its mana cost.
   *   Then he or she puts all cards exiled with Possibility Storm on the bottom of his or her library in a random order. */

	if (in_play(player, card) && !is_humiliated(player, card, AL_TRIG)
        && trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)
        && !not_played_from_hand(trigger_cause_controller, trigger_cause)){
        sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
    }
    if (event == EVENT_SP_ABILITY){
	int t_player = trigger_cause_controller;
	int iid = get_original_internal_card_id(trigger_cause_controller, trigger_cause);
	int type = get_type(trigger_cause_controller, trigger_cause);
	put_on_top_of_deck(trigger_cause_controller, trigger_cause);
	rfg_top_card_of_deck(t_player);
	int *deck = deck_ptr[t_player];
	int count = 0;
	if( deck[0] != -1 ){
		int good = 0;
		while( deck[count] != -1 ){
				if( is_what(-1, deck[count], type)){
					good = 1;
					break;
				}
				count++;
		}
		show_deck(HUMAN, deck, count+1, "Cards revealed by Possibility Storm", 0, gs_done_008b40e0 );
		if( good == 1 && can_legally_play_iid(t_player, deck[count]) ){
			int csvid = cards_data[deck[count]].id;
			card_ptr_t* c = cards_ptr[ csvid ];
			char buffer[100];
			scnprintf(buffer, 100, " Play %s\n Decline", c->name);
			int choice = do_dialog(t_player, player, card, -1, -1, buffer, 0);
			if( choice == 0 ){
				rfg_card_in_deck(t_player, count);
				play_card_in_exile_for_free(t_player, t_player, csvid);
			}
			else{
				count++;
			}
		}
	}
	raw_put_iid_on_top_of_deck(t_player, iid);
	count++;
	put_top_x_on_bottom_in_random_order(t_player, count);

    }

	return global_enchantment(player, card, event);
}

int card_punish_the_enemy(int player, int card, event_t event){
	/* Punish the Enemy	|4|R	0x200a77d
	 * Instant
	 * ~ deals 3 damage to target player or planeswalker and 3 damage to target creature. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td1, NULL, 1, NULL) ;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
		if( spell_fizzled != 1 ){
			new_pick_target(&td1, "TARGET_CREATURE", 1, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			damage_target0(player, card, 3);
		}
		if( validate_target(player, card, &td1, 1) ){
			damage_creature(instance->targets[1].player, instance->targets[1].card, 3, player, card);
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return 0;
}

int card_pyrewild_shaman(int player, int card, event_t event){
/* CARD_ID_PYREWILD_SHAMAN	13198
Pyrewild Shaman	|2|R
Creature - Goblin Shaman 3/1
Bloodrush - |1|R, Discard ~: Target attacking creature gets +3/+1 until end of turn.
Whenever one or more creatures you control deal combat damage to a player, if ~ is in your graveyard, you may pay |3. If you do, return ~ to your hand.*/

	return bloodrush(player, card, event, MANACOST_XR(1, 1), 3, 1, 0, 0);
}

int card_rubblebelt_maaka(int player, int card, event_t event){
/* CARD_ID_RUBBLEBELT_MAAKA	13211
Rubblebelt Maaka	|3|R
Creature - Cat 3/3
Bloodrush - |R, Discard ~: Target attacking creature gets +3/+3 until end of turn. */

	return bloodrush(player, card, event, MANACOST_R(1), 3, 3, 0, 0);
}

int card_smelt_ward_gatekeeper(int player, int card, event_t event){
/* CARD_ID_SMELT_WARD_GATEKEEPERS	13225
Smelt-Ward Gatekeepers	|3|R
Creature - Human Warrior 2/4
When ~ enters the battlefield, if you control two or more Gates, gain control of target creature an opponent controls until end of turn.
Untap that creature. It gains haste until end of turn. */

	gatekeeper(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_weapon_surge(int player, int card, event_t event){
/* CARD_ID_WEAPON_SURGE	13248
Weapon Surge	|R
Instant
Target creature you control gets +1/+0 and gains first strike until end of turn.
Overload |1|R (You may cast this spell for its overload cost. If you do, change its text by replacing all instances of "target" with "each.") */

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
				result = pay_overload(player, card, MANACOST_XR(1, 1));
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
				pump_creatures_until_eot(player, card, player, 0, 1, 0, KEYWORD_FIRST_STRIKE, 0, &this_test);
			}
			else{
				if( valid_target(&td) ){
					pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1, 0, KEYWORD_FIRST_STRIKE, 0);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return overload(player, card, event, MANACOST_XR(1, 1));
}

// Green

// battering krasis --> cloudfin_raptor

int card_kraul_warrior(int player, int card, event_t event){
/* CARD_ID_KRAUL_WARRIOR	13165
Kraul Warrior	|1|G
Creature - Insect Warrior 2/2
|5|G: ~ gets +3/+3 until end of turn. */

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_XG(5, 1), 3, 3);
}

int card_maze_behemoth(int player, int card, event_t event){
/* CARD_ID_MAZE_BEHEMOTH	13173
Maze Behemoth	|5|G
Creature - Elemental 5/4
Trample
Multicolored creatures you control have trample. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_ABILITIES && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
		{
			if( count_colors(affected_card_controller, affected_card) > 1 ){
				event_result |= KEYWORD_TRAMPLE;
			}
		}
	}

	return 0;
}

int card_mending_touch(int player, int card, event_t event){
/* CARD_ID_MENDING_TOUCH	13179
Mending Touch	|G
Instant
Regenerate target creature. */

	return card_death_ward(player, card, event);
}

int card_mutants_prey(int player, int card, event_t event){
	/* Mutant's Prey	|G
	 * Instant
	 * Target creature you control with a +1/+1 counter on it fights target creature an opponent controls. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_P1_P1;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller = 1-player;
	td1.preferred_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "Select target creature you control with a +1/+1 counter.", 0, 1 | GS_LITERAL_PROMPT) ){
			new_pick_target(&td1, "Select target creature opponent controls", 1, 1 | GS_LITERAL_PROMPT);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td1, 1) ){
			fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player, instance->targets[1].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_phytoburst(int player, int card, event_t event){
	/* Phytoburst	|1|G
	 * Sorcery
	 * Target creature gets +5/+5 until end of turn. */
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	return vanilla_pump(player, card, event, &td, 5, 5, 0, 0);
}

static int has_1_1_counter(int unused1, int unused2, int player, int card){
	return count_1_1_counters(player, card) ? 1 : 0;
}

int card_renegade_krasis(int player, int card, event_t event)
{
  /* CARD_ID_RENEGADE_KRASIS	13205
   * Renegade Krasis	|1|G|G
   * Creature - Beast Mutant 3/2
   * Evolve (Whenever a creature enters the battlefield under your control, if that creature has greater power or toughness than this creature,
   * put a +1/+1 counter on this creature.) Whenever ~ evolves, put a +1/+1 counter on each other creature you control with a +1/+1 counter on it. */

  int entering_stat;
  int own_stat;
  test_definition_t test;

  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (trigger_cause_controller == player))
  {
    if (is_what(player, trigger_cause, 2) != 0)
    {
      entering_stat = get_power(trigger_cause_controller, trigger_cause);
      own_stat = get_power(player, card);
      if (entering_stat <= own_stat)
      {
        entering_stat = get_toughness(trigger_cause_controller, trigger_cause);
        own_stat = get_toughness(player, card);
      }
      if (entering_stat > own_stat)
      {
        default_test_definition(&test, 2);
        test.not_me = 1;
        etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
      }
    }
  }
  if (event != EVENT_ETB)
  {
    return 0;
  }
  add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  default_test_definition(&test, 2);
  test.not_me = 1;
  test.special_selection_function = has_1_1_counter;
  new_manipulate_all(player, card, player, &test, ACT_ADD_COUNTERS_BASE | ACT_KILL_BURY);
  return 0;
}

int card_skylasher(int player, int card, event_t event){
/* CARD_ID_SKYLASHER	13224
Skylasher	|1|G
Creature - Insect 2/2
Flash
~ can't be countered.
Reach, protection from |Sblue */

	cannot_be_countered(player, card, event);
	return flash(player, card, event);
}

int card_surali_gatekeeper(int player, int card, event_t event){
/* CARD_ID_SARULI_GATEKEEPERS	13214
Saruli Gatekeepers	|3|G
Creature - Elf Warrior 2/4
When ~ enters the battlefield, if you control two or more Gates, you gain 7 life. */

	gatekeeper(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 7);
	}

	return 0;
}

int card_thrashing_mossdog(int player, int card, event_t event){
/* CARD_ID_THRASHING_MOSSDOG	13233
Thrashing Mossdog	|3|G
Creature - Plant Hound 3/3
Reach (This creature can block creatures with flying.)
Scavenge |4|G|G (|4|G|G, Exile this card from your graveyard: Put a number of +1/+1 counters equal to this card's power on target creature. Scavenge only as a sorcery.)*/

	return scavenge(player, card, event, MANACOST_XG(4, 2));
}

// gold

int card_advent_of_the_wurm(int player, int card, event_t event){
/* CARD_ID_ADVENT_OF_THE_WURM	13109
Advent of the Wurm	|1|G|G|W
Instant
Create a 5/5 |Sgreen Wurm creature token with trample. */

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_WURM, &token);
		token.pow = 5;
		token.tou = 5;
		token.key_plus = KEYWORD_TRAMPLE;
		token.color_forced = COLOR_TEST_GREEN;
		generate_token(&token);
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

// armored wolf rider --> vanilla

// ascended lawmage --> aven_fleetwing

int card_beetleform_mage(int player, int card, event_t event){
/* CARD_ID_BEETLEFORM_MAGE	13121
Beetleform Mage	|1|G|U
Creature - Human Insect Wizard 2/2
|G|U: ~ gets +2/+2 and gains flying until end of turn. Activate this ability only once each turn. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST_UG(1, 1), 0, NULL, NULL) ){
			return 2;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade(player, card, event, 0, MANACOST_UG(1, 1), 2, 2, KEYWORD_FLYING, 0);
	}

	return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST_UG(1, 1), 0, NULL, NULL);
}

int card_blast_of_genius(int player, int card, event_t event){
	/* Blast of Genius	|4|U|R	0x200a700
	 * Sorcery
	 * Choose any target. Draw three cards, then discard a card. ~ deals damage equal to the discarded card's converted mana cost to that permanent or player. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			draw_cards(player, 3);
			if( hand_count[player] > 0 ){
				char msg[100] = "Select a card to discard.";
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, msg);
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
				int dmg = get_cmc(player, selected);
				discard_card(player, selected);
				damage_target0(player, card, dmg);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_blaze_commando(int player, int card, event_t event){
/* Blaze Commando	|3|R|W
 * Creature - Minotaur Soldier 5/3
 * Whenever an instant or sorcery spell you control deals damage,
 * 		put two 1/1 |Sred and |Swhite Soldier creature tokens with haste onto the battlefield. */

if( event == EVENT_DEAL_DAMAGE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_SPELL);
		check_damage_test(player, -1, event, 0, player, card, 0, &test);
	}

	int result = resolve_damage_trigger(player, card, event, 0, player, card);
	  if( result ){
		  token_generation_t token;
		  default_token_definition(player, card, CARD_ID_SOLDIER, &token);
		  token.qty = 2;
		  token.color_forced = COLOR_TEST_RED | COLOR_TEST_WHITE;
		  token.s_key_plus = SP_KEYWORD_HASTE;
		  generate_token(&token);
	   }

	return global_enchantment(player, card, event);
}

int card_blood_baron_of_vizkopa(int player, int card, event_t event){
/* CARD_ID_BLOOD_BARON_OF_VIZKOPA	13125
Blood Baron of Vizkopa	|3|W|B
Creature - Vampire 4/4
Lifelink, protection from |Swhite and from |Sblack
As long as you have 30 or more life and an opponent has 10 or less life, ~ gets +6/+6 and has flying. */

	lifelink(player, card, event);

	if( in_play(player, card) && !is_humiliated(player, card, AL_STATIC) ){
		if( (event == EVENT_ABILITIES || event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			if( life[player] >= 30 && life[1-player] <= 10 ){
				switch( event ){
						case EVENT_POWER:
						case EVENT_TOUGHNESS:
							event_result += 6;
							break;
						case EVENT_ABILITIES:
							event_result |= KEYWORD_FLYING;
							break;
						default:
							break;
				}
			}
		}
	}
	return 0;
}

// boros battleshaper --> impossible

int card_bred_for_the_hunt(int player, int card, event_t event){
  /* Bred for the Hunt |1|U|G
   * Enchantment
   * Whenever a creature you control with a +1/+1 counter on it deals combat damage to a player, you may draw a card. */

	if( event == EVENT_DEAL_DAMAGE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.special_selection_function = &has_1_1_counter;
		check_damage_test(player, -1, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE,
							player, card, 0, &test);
	}

	if( resolve_damage_trigger(player, -1, event, RESOLVE_TRIGGER_AI(player) | DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS |
								DDBM_MUST_BE_COMBAT_DAMAGE, player, card) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		APNAP(p,{
					if( times_damaged[p] ){
						draw_cards(player, 1);
					}
				};
		);
	}

	return global_enchantment(player, card, event);
}

int card_bronzebeak_moa(int player, int card, event_t event){
/* CARD_ID_BRONZEBEAK_MOA	13132
Bronzebeak Moa	|2|G|W
Creature - Bird 2/2
Whenever another creature enters the battlefield under your control, ~ gets +3/+3 until end of turn. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		pump_until_eot_merge_previous(player, card, player, card, 3, 3);
	}

	return 0;
}

int card_carnage_gladiator(int player, int card, event_t event){
/* CARD_ID_CARNAGE_GLADIATOR	13134
Carnage Gladiator	|2|B|R
Creature - Skeleton Warrior 4/2
Whenever a creature blocks, that creature's controller loses 1 life.
|1|B|R: Regenerate ~. */

	if( event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card, AL_TRIG) ){
		int i;
		for(i = active_cards_count[1-current_turn]-1; i > -1; i--){
			if( in_play(1-current_turn, i) && is_what(1-current_turn, i, TYPE_CREATURE) && blocking(1-current_turn, i, event) ){
		       lose_life(1-current_turn, 1);
	        }
		}
	}

	return regeneration(player, card, event, MANACOST_XBR(1, 1, 1));

}

int card_council_of_the_absolute(int player, int card, event_t event){
  /* Council of the Absolute |2|U|W
   * Creature — Human Advisor 2/4
   * As Council of the Absolute enters the battlefield, name a card other than a creature or land card.
   * Your opponents can't cast cards with the chosen name.
   * Spells with the chosen name you cast cost {2} less to cast. */

	if( event == EVENT_RESOLVE_SPELL ){
		int csvid;
		test_definition_t test;
		if (IS_AI(player)){
			default_test_definition(&test, TYPE_LAND);
			test.type_flag = DOESNT_MATCH;
			csvid = ai_name_a_random_csvid(player, card, 1-player, TYPE_NONE, &test);
		}else{
			new_default_test_definition(&test, TYPE_LAND | TYPE_CREATURE, "Choose a noncreature, nonland card name.");
			test.type_flag = DOESNT_MATCH;
			csvid = new_card_from_list(player, 3, &test);
		}
		get_card_instance(player, card)->targets[1].card = csvid;
		create_card_name_legacy(player, card, csvid);
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card, AL_STATIC)){
		int csvid = get_card_instance( player, card)->targets[1].card;
		if( get_id(affected_card_controller, affected_card) == csvid ){
			if( affected_card_controller == player ){
				COST_COLORLESS-=2;
			}
			else{
				infinite_casting_cost();
			}
		}
	}
	return 0;
}

int card_deadbridge_chant(int player, int card, event_t event){
/* CARD_ID_DEADBRIDGE_CHANT	13139
Deadbridge Chant	|4|B|G
Enchantment
When ~ enters the battlefield, put the top ten cards of your library into your graveyard.
At the beginning of your upkeep, choose a card at random in your graveyard. If it's a creature card, put it onto the battlefield. Otherwise, put it into your hand.*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		mill(player, 10);
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		const int *grave = get_grave(player);
		if( grave[0] != -1 ){
			int rnd = 0;
			if( count_graveyard(player) > 1 ){
				rnd = internal_rand(count_graveyard(player));
			}
			reveal_card_iid(player, card, grave[rnd]);
			if( is_what(-1, grave[rnd], TYPE_CREATURE) ){
				reanimate_permanent(player, card, player, rnd, REANIMATE_DEFAULT);
			}
			else{
				from_grave_to_hand(player, rnd, TUTOR_HAND);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_debt_to_the_deathless(int player, int card, event_t event){
/* CARD_ID_DEBT_TO_THE_DEATHLESS	13140
Debt to the Deathless	|X|W|W|B|B
Sorcery
Each opponent loses two times X life. You gain life equal to the life lost this way. */

	if(event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		int result = lose_life(1-player, 2*instance->info_slot);
		gain_life(player, result);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_deputy_of_acquittals(int player, int card, event_t event)
{
/* CARD_ID_DEPUTY_OF_ACQUITTALS	13141
Deputy of Acquittals	|W|U
Creature - Human Wizard 2/2
Flash (You may cast this spell any time you could cast an instant.)
When ~ enters the battlefield, you may return another target creature you control to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.special = TARGET_SPECIAL_NOT_ME;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return flash(player, card, event);
}

int card_dragonshift(int player, int card, event_t event){
  /* Dragonshift |1|U|R
   * 	  Instant
   * 	  Until end of turn, target creature you control becomes a 4/4 blue and red Dragon, loses all abilities, and gains flying.
   * 	  Overload {3}{U}{U}{R}{R} (You may cast this spell for its overload cost. If you do, change its text by replacing all instances of "target" with "each.") */

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
				result = pay_overload(player, card, MANACOST_XUR(3, 2, 2));
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
			test_definition_t hc;
			default_test_definition(&hc, 0);
			hc.power = 4;
			hc.toughness = 4;
			hc.subtype = SUBTYPE_DRAGON;
			hc.color = COLOR_TEST_BLUE | COLOR_TEST_RED;
			hc.keyword = KEYWORD_FLYING;
			if( instance->info_slot & 2 ){
				int c;
				for (c = active_cards_count[player]-1; c > -1; c--){
					if (in_play(player, c) && is_what(player, c, TYPE_CREATURE)){
						humiliate_and_set_pt_abilities(player, card, player, c, 4, &hc);
					}
				}
			}
			else{
				if (valid_target(&td)){
					humiliate_and_set_pt_abilities(player, card, instance->targets[0].player, instance->targets[0].card, 4, &hc);
				}
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return overload(player, card, event, MANACOST_XUR(3, 2, 2));
}

int card_drown_in_filth(int player, int card, event_t event){
/* CARD_ID_DROWN_IN_FILTH	13145
Drown in Filth	|B|G
Sorcery
Choose target creature. Put the top four cards of your library into your graveyard,
then that creature gets -1/-1 until end of turn for each land card in your graveyard. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			mill(player, 4);
			int amount = count_graveyard_by_type(player, TYPE_LAND);
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -amount, -amount);
		}
		else{
			cancel = 1;
		}
		kill_card(player, card, KILL_BURY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_emmara_tandris(int player, int card, event_t event){
/* CARD_ID_EMMARA_TANDRIS	13146
Emmara Tandris	|5|G|W
Legendary Creature - Elf Shaman 5/7
Prevent all damage that would be dealt to creature tokens you control. */

	check_legend_rule(player, card, event);

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card != -1 && damage->damage_target_player == player && damage->info_slot > 0 &&
				is_what(damage->damage_target_player, damage->damage_target_card, TYPE_CREATURE) &&
				is_token(damage->damage_target_player, damage->damage_target_card) )
			{
				int amount = damage->info_slot;
				prevent_damage(affected_card_controller, affected_card, amount);
			}
		}
	}

	return 0;
}

int card_exava_rakdos_blood_witch(int player, int card, event_t event){

	/* Exava, Rakdos Blood Witch	|2|B|R
	 * Legendary Creature - Human Cleric 3/3
	 * First strike, haste
	 * Unleash
	 * Each other creature you control with a +1/+1 counter on it has haste. */

	haste(player, card, event);

	check_legend_rule(player, card, event);

	unleash(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_ABILITIES && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
			count_1_1_counters(affected_card_controller, affected_card) >= 1 )
		{
			haste(affected_card_controller, affected_card, event);
		}
	}

	return 0;
}

int card_fluxcharger(int player, int card, event_t event)
{
/* CARD_ID_FLUXCHARGER	13149
Fluxcharger	|2|U|R
Creature - Weird 1/5
Flying
Whenever you cast an instant or sorcery spell, you may switch ~'s power and toughness until end of turn. */

	sp_ability_trigger(player, event, duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL, TYPE_SPELL, NULL, player, card);
	if (event == EVENT_SPA){
		switch_power_and_toughness_until_eot(player, card, player, card);
	}
	return 0;
}

int card_gaze_of_granite(int player, int card, event_t event){
/* CARD_ID_GAZE_OF_GRANITE	13150
Gaze of Granite	|X|B|B|G
Sorcery
Destroy each nonland permanent with converted mana cost X or less. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.type_flag = 1;
		this_test.cmc = get_card_instance(player, card)->info_slot+1;
		this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		new_manipulate_all(player, card, ANYBODY, &this_test, KILL_DESTROY);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_gleam_of_battle(int player, int card, event_t event)
{
/* CARD_ID_GLEAM_OF_BATTLE	13152
Gleam of Battle	|4|R|W
Enchantment
Whenever a creature you control attacks, put a +1/+1 counter on it. */

  store_attackers(player, card, event, DAT_TRACK, player, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int amt = instance->info_slot;
	  unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
	  for (--amt; amt >= 0; --amt)
		if (in_play(current_turn, attackers[amt]))
		  add_counters(player, card, current_turn, attackers[amt], COUNTER_P1_P1, 1);
	}

  return global_enchantment(player, card, event);
}

int card_goblin_test_pilot(int player, int card, event_t event){
	/* Goblin Test Pilot	|1|U|R	0x200a5f7
	 * Creature - Goblin Pilot Wizard 0/2
	 * Flying
	 * |T: ~ deals 2 damage to any target chosen at random. */
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

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int trgs[2][100];
			int t_count[2];
			int k;
			for(k=0; k<2; k++){
				t_count[k] = 0;
				instance->targets[0].player = k;
				instance->targets[0].card = -1;
				instance->number_of_targets = 1;
				if( would_valid_target(&td) ){
					trgs[k][0] = -1;
					t_count[k]++;
				}
			}
			for(k=0; k<2; k++){
				int count = 0;
				while( count < active_cards_count[k] ){
						if( in_play(k, count) ){
							instance->targets[0].player = k;
							instance->targets[0].card = count;
							instance->number_of_targets = 1;
							if( would_valid_target(&td) ){
								trgs[k][t_count[k]] = count;
								t_count[k]++;
							}
						}
						count++;
				}
			}
			if( t_count[1-player] > 0 ){
				instance->targets[0].player = 1-player;
				if( t_count[player] > 0 ){
					instance->targets[0].player = internal_rand(2);
				}
				int rnd = 0;
				if( t_count[instance->targets[0].player] > 1 ){
					rnd = trgs[instance->targets[0].player][internal_rand(t_count[instance->targets[0].player])];
				}
				instance->targets[0].card = rnd;
			}
			else{
				instance->targets[0].player = player;
				int rnd = 0;
				if( t_count[instance->targets[0].player] > 1 ){
					rnd = trgs[instance->targets[0].player][internal_rand(t_count[instance->targets[0].player])];
				}
				instance->targets[0].card = rnd;
			}
			instance->number_of_targets = 1;
			tap_card(player, card);
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return 0;
}

int card_gruul_war_chant(int player, int card, event_t event)
{
/* CARD_ID_GRUUL_WAR_CHANT	13156
Gruul War Chant	|2|R|G
Enchantment
Attacking creatures you control get +1/+0 and have menace. */

	if (in_play(player, card) && !is_humiliated(player, card, AL_STATIC)){
		if (event == EVENT_POWER){
			if (affected_card_controller == player && is_attacking(player, affected_card)){
				event_result += 1;
			}
		}
		else if (event == EVENT_DECLARE_BLOCKERS || event == EVENT_ABILITIES){
			for (int i = 0; i < active_cards_count[player]; ++i){
				if (in_play(player, i) && is_what(player, i, TYPE_CREATURE) && is_attacking(player, i)){
					menace(player, i, event);
				}
			}
		}
	}
	return global_enchantment(player, card, event);
}

int card_haunter_of_nightveil(int player, int card, event_t event){
/* CARD_ID_HAUNTER_OF_NIGHTVEIL	13158
Haunter of Nightveil	|3|U|B
Creature - Spirit 3/4
Creatures your opponents control get -1/-0. */

	boost_creature_type(player, card, event, -1, -1, 0, 0, BCT_OPPONENT_ONLY);
	return 0;
}

int card_jelenn_sphinx(int player, int card, event_t event){
/* CARD_ID_JELENN_SPHINX	13162
Jelenn Sphinx	|3|W|U
Creature - Sphinx 1/5
Flying, vigilance
Whenever ~ attacks, other attacking creatures get +1/+1 until end of turn. */

	vigilance(player, card, event);

	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.state = STATE_ATTACKING;
		this_test.not_me = 1;
		pump_creatures_until_eot(player, card, player, 0, 1, 1, 0, 0, &this_test);
	}
	return 0;
}

int card_korozda_gorgon(int player, int card, event_t event){

	/* Korozda Gorgon	|3|B|G
	 * Creature - Gorgon 2/5
	 * Deathtouch
	 * |2, Remove a +1/+1 counter from a creature you control: Target creature gets -1/-1 until end of turn. */

	deathtouch(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	base_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_P1_P1;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	deathtouch(player, card, event);

	if( event == EVENT_CAN_ACTIVATE && can_target(&td) ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(2), 0, &td1, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			instance->number_of_targets = 0;
			int selected = -1;
			if( new_pick_target(&td, "Select a creature you control with a +1/+1 counter.", 0, 1 | GS_LITERAL_PROMPT) ){
				selected = instance->targets[0].card;
				instance->number_of_targets = 0;
				if( pick_target(&td1, "TARGET_CREATURE") ){
					remove_counters(player, card, player, selected, COUNTER_P1_P1, 1);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, -1, -1);
		}
	}

	return 0;
}

int card_krasis_incubation(int player, int card, event_t event)
{
  /* Krasis Incubation	|2|G|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature can't attack or block, and its activated abilities can't be activated.
   * |1|G|U, Return ~ to its owner's hand: Put two +1/+1 counters on enchanted creature. */

   if( get_card_instance(player, card)->damage_target_player > -1){
		card_instance_t* instance = get_card_instance(player, card);
		if( event == EVENT_CAN_ACTIVATE ){
			return generic_activated_ability(player, card, event, GAA_BOUNCE_ME, MANACOST_XGU(1,1,1), 0, NULL, NULL);
		}
		if( event == EVENT_ACTIVATE ){
			instance->targets[0].player = instance->damage_target_player;
			instance->targets[0].card = instance->damage_target_card;
			return generic_activated_ability(player, card, event, GAA_BOUNCE_ME, MANACOST_XGU(1,1,1), 0, NULL, NULL);
		}
		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( instance->targets[0].player > -1 && in_play(instance->targets[0].player, instance->targets[0].card) ){
				add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 2);
			}
		}
	}

	return card_arrest(player, card, event);
}

int card_lavinia_of_the_tenth(int player, int card, event_t event){
  /* Lavinia of the Tenth	|3|W|U
   * Legendary Creature - Human Soldier 4/4
   * Protection from red
   * When Lavinia enters, detain each nonland permanent your opponents control with mana value 4 or less. (Until your next turn, those permanents can't attack or block and their activated abilities can't be activated.) */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.type_flag = DOESNT_MATCH;
		this_test.cmc = 5;
		this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		new_manipulate_all(player, card, 1-player, &this_test, ACT_DETAIN);
	}

	return 0;
}

static int legions_initiative_legacy(int player, int card, event_t event)
{
  if (beginning_of_combat(player, card, event, ANYBODY))
  {
    card_instance_t *instance = get_card_instance(player, card);
    for (int i = 0; i < 19; ++i)
    {
      int owner = instance->targets[i].player;
      if (owner >= 0)
      {
        int iid = instance->targets[i].card;
        if (check_rfg(owner, cards_data[iid].id))
        {
          int added = add_card_to_hand(owner, iid);
          remove_card_from_rfg(owner, cards_data[iid].id);
          put_into_play(owner, added);
          pump_ability_until_eot(player, card, owner, added, 0, 0, 0, SP_KEYWORD_HASTE);
          get_card_instance(owner, added)->regen_status |= KEYWORD_RECALC_ABILITIES;
          get_abilities(owner, added, EVENT_ABILITIES, -1);
        }
      }
    }
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

int card_legions_initiative(int player, int card, event_t event)
{
  /* Legion's Initiative	|R|W
   * Enchantment
   * Red creatures you control get +1/+0.
   * White creatures you control get +0/+1.
   * |R|W, Exile this enchantment: Exile all creatures you control. At the beginning of the next combat, return those cards to the battlefield under their owner's control and those creatures gain haste until end of turn. */

  boost_creature_by_color(player, card, event,
                          get_sleighted_color_test(player, card, COLOR_TEST_RED), 1, 0, 0,
                          BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
  boost_creature_by_color(player, card, event,
                          get_sleighted_color_test(player, card, COLOR_TEST_WHITE), 0, 1, 0,
                          BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
  if (!IS_GAA_EVENT(event))
    return global_enchantment(player, card, event);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    int marked[151] = {0};
    CYCLE_ALL_CARDS(p, c, {
      if (p == player && in_play(p, c) && is_what(p, c, TYPE_CREATURE))
        marked[c] = 1;
    });
    int legacy = -1, removed_count = 0;
    for (int i = 150; i >= 0; --i)
    {
      if (marked[i])
      {
        int csvid = get_id(player, i);
        int melded = csvid == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES;
        if (!melded)
        {
          csvid = get_id(player, i);
          melded = csvid == CARD_ID_CHITTERING_HOST;
        }
        if (!melded)
        {
          csvid = get_id(player, i);
          melded = csvid == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP;
        }
        if (legacy == -1)
          legacy = create_legacy_effect(player, card, legions_initiative_legacy);
        int pos;
        if (removed_count > 18 || (melded && removed_count == 18))
        {
          legacy = create_legacy_effect(player, card, legions_initiative_legacy);
          removed_count = 1;
          pos = 0;
        }
        else
          pos = removed_count++;
        card_instance_t *leg = get_card_instance(player, legacy);
        leg->targets[pos].player = get_owner(player, i);
        if (melded)
        {
          int csvida, csvidb;
          unmeld(csvid, &csvida, &csvidb);
          leg->targets[pos].card = get_internal_card_id_from_csv_id(csvida);
          leg->targets[removed_count].card = get_internal_card_id_from_csv_id(csvidb);
          leg->targets[removed_count].player = get_owner(player, i);
          removed_count = pos + 2;
        }
        else
          leg->targets[pos].card = get_original_internal_card_id(player, i);
        kill_card(player, i, KILL_EXILE);
      }
    }
  }
  return generic_activated_ability(player, card, event, GAA_RFG_ME, MANACOST_RW(1, 1), 0, NULL,
                                   NULL);
}

int card_master_of_cruelties(int player, int card, event_t event)
{
  /* Master of Cruelties	|3|B|R
   * Creature - Demon 1/4
   * First strike, deathtouch
   * This creature can only attack alone.
   * Whenever this creature attacks a player and isn't blocked, that player's life total becomes 1. This creature assigns no combat damage this combat. */

  deathtouch(player, card, event);

  if (event == EVENT_ATTACK_LEGALITY && affected_card_controller == player && ! is_humiliated(player, card, AL_TRIG) )
	{
	  card_instance_t* aff = get_card_instance(affected_card_controller, affected_card);
	  if (aff->state & STATE_ATTACKING)	// Already attacking
		return 0;

	  if (affected_card != card)
		{
		  if (get_card_instance(player, card)->state & STATE_ATTACKING)
			event_result = 1;
		}
	  else
		{
		  int c;
		  for (c = 0; c < active_cards_count[player]; ++c)
			if ((aff = in_play(player, c)) && (aff->state & STATE_ATTACKING))
			  {
				event_result = 1;
				break;
			  }
		}
	}

	if (event == EVENT_DECLARE_BLOCKERS && is_attacking(player, card) && is_unblocked(player, card) && ! is_humiliated(player, card, AL_TRIG) &&
		! check_special_flags(player, card, SF_ATTACKING_PWALKER) )
	{
	  negate_combat_damage_this_turn(player, card, player, card, 0);
	  if (life[1-player] > 1)
		lose_life(1-player, life[1-player] - 1);
	  else if (life[1-player] < 1)
		gain_life(1-player, 1 - life[1-player]);
	  /* Amusingly, the latter will only work when the defending player should have already lost the game from having 0 or less life, but it hasn't kicked in
	   * yet because Manalink only checks that at end of phase; not when a Platinum Angel or such has prevented him from losing due to having 0 or less life. */
	}

  return 0;
}

int card_maw_of_obzedat(int player, int card, event_t event){
  /* Maw of the Obzedat	|3|W|B
   * Creature - Thrull 3/3
   * Sacrifice a creature: Creatures you control get +1/+1 until end of turn. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1, 1, 1, 0, 0);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL);
}

int card_melek_izzet_paragon(int player, int card, event_t event)
{
  /* Melek, Izzet Paragon	|4|U|R
   * Legendary Creature - Weird Wizard 2/4
   * Play with the top card of your library revealed.
   * You may cast the top card of your library if it's an instant or sorcery card.
   * Whenever you cast an instant or sorcery spell from your library, copy it. You may choose new targets for the copy. */

  check_legend_rule(player, card, event);

  reveal_top_card(player, card, event);

  if (event == EVENT_CAN_ACTIVATE && ! is_humiliated(player, card, AL_STATIC))
	{
	  int iid = deck_ptr[player][0];
	  if (iid != -1 && is_what(-1, iid, TYPE_INSTANT | TYPE_SORCERY) && has_mana_to_cast_iid(player, event, iid))
		return can_legally_play_iid_now(player, iid, event);
	}

  if (event == EVENT_ACTIVATE && charge_mana_from_id(player, -1, event, cards_data[deck_ptr[player][0]].id))
	{
	  play_card_in_deck_for_free(player, player, 0);	// Yet another case where it's not free.
	  cant_be_responded_to = 1;
	}

  if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && check_special_flags(trigger_cause_controller, trigger_cause, SF_PLAYED_FROM_DECK)){
    sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_SPELL, NULL, player, card);
  }
  if (event == EVENT_SPA){ copy_spell_from_stack(player, trigger_cause_controller, trigger_cause); }

  return 0;
}

int card_mirko_vosk_mind_drinker(int player, int card, event_t event){
  /* Mirko Vosk, Mind Drinker	|3|U|B
   * Legendary Creature - Vampire 2/4
   * Flying
   * Whenever Mirko Vosk deals combat damage to a player, that player reveals cards from the top of their library until they reveal four land cards, then puts those cards into their graveyard. */

	check_legend_rule(player, card, event);

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE+DDBM_MUST_DAMAGE_OPPONENT) ){
		mind_funeral_effect(player, card, 1-player);
	}

	return 0;
}

int card_morgue_burst(int player, int card, event_t event){
	/* Morgue Burst	|4|B|R	0x200a7c8
	 * Sorcery
	 * Return target creature card from your graveyard to your hand. ~ deals damage to any target equal to the power of the card returned this way. */
	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	if( event == EVENT_CAN_CAST && ! graveyard_has_shroud(2) && any_in_graveyard_by_type(player, TYPE_CREATURE) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != -1 ){
			if( select_target_from_grave_source(player, card, player, 0, AI_MAX_CMC, &this_test, 1) == -1 ){
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 1);
		if( selected != -1 ){
			int iid = get_grave(player)[selected];
			from_grave_to_hand(player, selected, TUTOR_HAND);
			/* Note that the card may no longer be in player's hand by now, as in the case of a Golgari Brownscale
			 * (triggers immediately for life gain); plenty of other things trigger on life gain,
			 * which may eventually result in a discard. */
			if( valid_target(&td) ){
				damage_target0(player, card, get_base_power_iid(player, iid));
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_nivix_cyclops(int player, int card, event_t event){
  /* Nivix Cyclops	|1|U|R
   * Creature - Cyclops 1/4
   * Defender
   * Whenever you cast an instant or sorcery spell, this creature gets +3/+0 until end of turn and can attack this turn as though it didn't have defender. */

	sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_SPELL, NULL, player, card);
	if (event == EVENT_SPA){
		pump_until_eot_merge_previous(player, card, player, card, 3, 0);
		int leg = create_targetted_legacy_effect_no_repeat(player, card, effect_defender_can_attack_until_eot, player, card);
		if (leg != -1){
			get_card_instance(player, leg)->token_status |= STATUS_INVISIBLE_FX;
		}
	}
	return 0;
}

/* ManalinkEh 2022: 0x2297700; original cards/dragons_maze.c:2176. */
int notion_thief_effect(int player, int card, event_t event)
{
  int32_t draw_count;
  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (((((instance != NULL) &&
         (result = is_humiliated(player, card, AL_STATIC), trigger_condition == TRIGGER_REPLACE_CARD_DRAW)) &&
        (card == affected_card)) &&
       ((player == affected_card_controller && (reason_for_trigger_controller != player)))) &&
      (result == 0 && suppress_draw == 0))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
      return 0;
    }
    if (event == EVENT_RESOLVE_TRIGGER)
    {

      instance = get_card_instance(player, card);
      if ((current_phase == 10) &&
          (draw_count = (uint32_t)instance->info_slot + 1u,
           instance->info_slot = draw_count, draw_count < 2))
      {
        return 0;
      }
      suppress_draw = 1;
      return 1;
    }
  }
  if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
  }
  return 0;
}

int card_notion_thief(int player, int card, event_t event)
{
  /* Notion Thief	|2|U|B
   * Creature - Human Rogue 3/1
   * Flash
   * If an opponent would draw a card except the first one he or she draws in each of his or her draw steps,
   * instead that player skips that draw and you draw a card. */

  int result;
  result = notion_thief_effect(player, card, event);
  if (result != 0)
  {
    draw_cards(player, 1);
  }
  result = flash(player, card, event);
  return result;
}

int card_obzedats_aid(int player, int card, event_t event){
  /* Obzedat's Aid	|3|W|B
   * Sorcery
   * Return target permanent card from your graveyard to the battlefield. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	char msg[100] = "Select a permanent card.";
	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_PERMANENT, msg);

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 0, &this_test);
}

int card_pilfered_plans(int player, int card, event_t event){
  /* Pilfered Plans	|1|U|B
   * Sorcery
   * Target player mills two cards. Draw two cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			mill(instance->targets[0].player, 2);
			draw_cards(player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

static int effect_plasm_capture(int player, int card, event_t event){

	if (event == EVENT_PHASE_CHANGED && current_phase == PHASE_MAIN1 && get_card_instance(player, card)->targets[0].player != 1 ){
		card_instance_t *instance = get_card_instance(player, card);
		FORCE(produce_mana_any_combination_of_colors(player, COLOR_TEST_ANY_COLORED, instance->targets[0].card, NULL));
		instance->targets[0].player = 1;
		kill_card(player, card, KILL_EXILE);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[0].player = 0;
	}
	return 0;
}

int card_plasm_capture(int player, int card, event_t event){

	/* Plasm Capture	|G|G|U|U
	 * Instant
	 * Counter target spell. At the beginning of your next precombat main phase, add X mana in any combination of colors to your mana pool, where X is that
	 * spell's converted mana cost. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return counterspell(player, card, event, NULL, 0);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		counterspell(player, card, event, NULL, 0);
		if( spell_fizzled != 1 ){
			instance->info_slot = get_cmc(card_on_stack_controller, card_on_stack);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);

			int legacy_card = create_legacy_effect(player, card, &effect_plasm_capture );
			card_instance_t *legacy = get_card_instance(player, legacy_card);
			legacy->targets[0].card = instance->info_slot;
			legacy->targets[1].card = get_id(player, card);
			legacy->targets[0].player = 0;
			if( current_turn == player ){
				legacy->targets[0].player++;
			}
		}

		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

static int effect_progenitor_mimic(int player, int card, event_t event)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (!is_token(instance->damage_target_player, instance->damage_target_card))
	upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	copy_token(instance->damage_target_player, instance->damage_target_card, instance->damage_target_player, instance->damage_target_card);

  return 0;
}

int card_progenitor_mimic(int player, int card, event_t event)
{
	/* Progenitor Mimic	|4|G|U	0x200a624
	 * Creature - Shapeshifter 0/0
	 * You may have ~ enter the battlefield as a copy of any creature on the battlefield, except it has "At the beginning of your upkeep, if this creature isn't a token, create a token that's a copy of this creature." */
	if (enters_the_battlefield_as_copy_of_any_creature(player, card, event)){
		set_legacy_image(player, CARD_ID_PROGENITOR_MIMIC, create_targetted_legacy_effect(player, card, effect_progenitor_mimic, player, card));
	}
	return 0;
}

int card_ral_zarek(int player, int card, event_t event){
	/* Ral Zarek	|2|U|R	0x200a629
	 * Legendary Planeswalker - Ral (4)
	 * +1: Tap target permanent, then untap another target permanent.
	 * -2: ~ deals 3 damage to any target.
	 * -7: Flip five coins. Take an extra turn after this one for each coin that comes up heads. */
	check_legend_rule(player, card, event);

	if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_PERMANENT);
		td1.preferred_controller = player;

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		enum {
			CHOICE_TAP_AND_UNTAP = 1,
			CHOICE_DAMAGE_TARGET,
			CHOICE_COIN_WALK
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Tap & untap", target_available(player, card, &td) > 1, 10, 1,
						"Lightning Bolt", can_target(&td2), 5, -2,
						"Coin Walk", 1, 15, -7);
		if (event == EVENT_CAN_ACTIVATE){
			if (!choice)
				return 0;	// else fall through to planeswalker()
		}

		else if (event == EVENT_ACTIVATE){
				instance->number_of_targets = 0;
				if( choice == CHOICE_TAP_AND_UNTAP ){
					if( new_pick_target(&td, "Select a permanent to tap.", 0, 1 | GS_LITERAL_PROMPT) ){
						state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
						new_pick_target(&td1, "Select a permanent to untap.", 1, 1 | GS_LITERAL_PROMPT);
					}
					state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
				}
				if( choice == CHOICE_DAMAGE_TARGET ){
					pick_target(&td2, "TARGET_ANY");
				}
		}

		else{	// event == EVENT_RESOLVE_ACTIVATION
			if( choice == CHOICE_TAP_AND_UNTAP ){
				if( validate_target(player, card, &td, 0) ){
					tap_card(instance->targets[0].player, instance->targets[0].card);
				}
				if( validate_target(player, card, &td1, 1) ){
					untap_card(instance->targets[1].player, instance->targets[1].card);
				}
			}
			else if( choice == CHOICE_DAMAGE_TARGET ){
					if( valid_target(&td2) ){
						damage_target0(player, card, 3);
					}
			}
			else if( choice == CHOICE_COIN_WALK ){
					int tw = 0;
					int i;
					for(i=0; i<5; i++){
						tw += flip_a_coin(instance->parent_controller, instance->parent_card);
					}
					for(i=0; i<tw; i++){
						time_walk_effect(instance->parent_controller, instance->parent_card);
					}
			}
		}
	}
	return planeswalker(player, card, event, 4);
}

int card_reap_intellect(int player, int card, event_t event){
/* Reap Intellect	|X|2|U|B
 * Sorcery
 * Target opponent reveals his or her hand. You choose up to X nonland cards from it and exile them.
 * For each card exiled this way, search that player's graveyard, hand, and library for any number of cards with the same
 * 		name as that card and exile them. Then that player shuffles his or her library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			card_instance_t *instance= get_card_instance(player, card);

			int amount = instance->info_slot;

			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, "Select a nonland card to exile.");
			this_test.type_flag = DOESNT_MATCH;

			int exiled[amount];
			int ec = 0;
			while( ec < amount ){
					int selected = new_select_a_card(player, 1-player, TUTOR_FROM_HAND, 0, AI_MAX_VALUE, -1, &this_test);
					if( selected != -1 ){
						exiled[ec] = get_id(1-player, selected);
						rfg_card_in_hand(1-player, selected);
						ec++;
					}
			}

			int i;
			for(i=0; i<ec; i++){
				lobotomy_effect(player, 1-player, exiled[i], -1, LOBOTOMY_NO_SHUFFLE);
			}
			shuffle(1-player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		get_card_instance(player, card)->number_of_targets = 0;
		if (!check_special_flags2(player, card, SF2_COPIED_FROM_STACK)){
			set_special_flags2(player, card, SF2_X_SPELL);
			charge_mana(player, COLOR_COLORLESS, -1);
			if (spell_fizzled != 1){ set_x_for_x_spells(player, card, event, x_value); }
		}
	}
	if (event == EVENT_CAN_CAST){ return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL); }
	return 0;
}

int card_render_silent(int player, int card, event_t event){
  /* Render Silent |W|U|U|
   * Instant
   * Counter target spell. Its controller can't cast spells this turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			target_player_cant_cast_type(player, card, instance->targets[0].player, TYPE_CREATURE|TYPE_ENCHANTMENT|TYPE_ARTIFACT|TARGET_TYPE_PLANESWALKER|
			                             TYPE_SPELL|TYPE_EFFECT);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}
	return card_counterspell(player, card, event);
}

// restore the peace --> impossible

int card_rot_farm_skeleton(int player, int card, event_t event){
	/* Rot Farm Skeleton	|2|B|G
	 * Creature - Plant Skeleton 4/1
	 * ~ can't block.
	 * |2|B|G, Put the top four cards of your library into your graveyard: Return ~ from your graveyard to the battlefield. Activate this ability only any time you could cast a sorcery. */

	cannot_block(player, card, event);

	if(event == EVENT_GRAVEYARD_ABILITY && can_sorcery_be_played(player, event) ){
		if( has_mana_multi(player, MANACOST_XBG(2, 1, 1)) ){
			return GA_RETURN_TO_PLAY;
		}
	}
	else if( event == EVENT_PAY_FLASHBACK_COSTS ){
			charge_mana_multi(player, MANACOST_XBG(2, 1, 1));
			if( spell_fizzled != 1 ){
				mill(player, 4);
				return GAPAID_REMOVE;
			}
	}

	return 0;
}

int card_ruric_thar_the_unbowed(int player, int card, event_t event){
  /* Ruric Thar, the Unbowed	|4|R|G
   * Legendary Creature - Ogre Warrior 6/6
   * Reach, vigilance
   * Ruric Thar attacks each combat if able.
   * Whenever a player casts a noncreature spell, Ruric Thar deals 6 damage to that player. */

	check_legend_rule(player, card, event);
	vigilance(player, card, event);
	attack_if_able(player, card, event);
	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.type_flag = DOESNT_MATCH;
		sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){
		damage_player(trigger_cause_controller, 6, player, card);
	}
	return 0;
}

int card_savageborn_hydra(int player, int card, event_t event){
  /* Savageborn Hydra	|X|R|G
   * Creature - Hydra 0/0
   * Double strike
   * This creature enters with X +1/+1 counters on it.
   * |1|RG: Put a +1/+1 counter on this creature. Activate only as a sorcery. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST0, 0, NULL, NULL) ){
			int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_XG(1, 1));
			return has_mana_hybrid(player, 1, COLOR_RED, COLOR_GREEN, c1);
		}
	}

	if(event == EVENT_ACTIVATE  ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_XG(1, 1));
		charge_mana_hybrid(player, card, 1, COLOR_RED, COLOR_GREEN, c1);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 1);
	}

	return card_ivy_elemental(player, card, event);
}

int card_scab_clan_giant(int player, int card, event_t event){
	/* Scab-Clan Giant	|4|R|G
	 * Creature - Giant Warrior 4/5
	 * When ~ enters the battlefield, it fights target creature an opponent controls chosen at random. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = 1-player;

		int marked[151] = {0};
		int found = -1;
		int num_founded = 0;
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == 1-player && in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
										if( would_validate_arbitrary_target(&td, p, c) ){
											marked[c] = 1;
											found = c;
											num_founded++;
										}
									}
								};
		);

		if( found > -1 ){
			int rnd = found;
			if( num_founded > 1 ){
				rnd = internal_rand(150);
				while( marked[rnd] == 0 ){
						rnd++;
						if( rnd == 151 ){
							rnd = 0;
						}
				}
			}
			card_instance_t *instance = get_card_instance(player, card);
			instance->targets[0].player = 1-player;
			instance->targets[0].card = marked[rnd];
			instance->number_of_targets = 1;
			fight(player, card, 1-player, marked[rnd]);
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

static int showstopper_legacy(int player, int card, event_t event){

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, 0);
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( result ){
		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.allowed_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		while( result ){
				if( can_target(&td2) && pick_target(&td2, "TARGET_CREATURE") ){
					damage_creature(instance->targets[0].player, instance->targets[0].card, 2, player, card);
				}
				instance->number_of_targets = 0;
				result--;
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_showstopper(int player, int card, event_t event){
  /* Showstopper	|1|B|R
   * Instant
   * Until end of turn, creatures you control gain "When this creature dies, it deals 2 damage to target creature an opponent controls." */

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_effect(player, card, &showstopper_legacy);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[0].card = get_id(player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_sin_collector(int player, int card, event_t event){
  /* Sin Collector	|1|W|B
   * Creature - Human Cleric 2/1
   * When this creature enters, target opponent reveals their hand. You choose an instant or sorcery card from it and exile that card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		instance->targets[0].player = 1-player;
		instance->targets[0].card = -1;
		instance->number_of_targets = 1;
		if( would_validate_target(player, card, &td2, 0) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_PERMANENT);
			this_test.type_flag = DOESNT_MATCH;

			ec_definition_t this_definition;
			default_ec_definition(1-player, player, &this_definition);
			this_definition.effect = EC_RFG;
			new_effect_coercion(&this_definition, &this_test);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_sire_of_insanity(int player, int card, event_t event){
  /* Sire of Insanity	|4|B|R
   * Creature - Demon 6/4
   * At the beginning of each end step, each player discards their hand. */

	if( eot_trigger(player, card, event) ){
		APNAP(p, {new_discard_all(p, player);};);
	}

	return 0;
}

int card_species_gorger(int player, int card, event_t event){
  /* Species Gorger	|3|G|U
   * Creature - Frog Beast 6/6
   * At the beginning of your upkeep, return a creature you control to its owner's hand. */

	if( trigger_condition == TRIGGER_UPKEEP ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.allow_cancel = 0;
		td.illegal_abilities = 0;

		return bounce_permanent_at_upkeep(player, card, event, &td);
	}
	return 0;
}

// spike jester --> vanilla

int card_tajic_blade_of_the_legion(int player, int card, event_t event)
{
  /* Tajic, Blade of the Legion	|2|R|W
   * Legendary Creature - Human Soldier 2/2
   * Indestructible
   * Battalion - Whenever Tajic and at least two other creatures attack, Tajic gets +5/+5 until end of turn. */

  check_legend_rule(player, card, event);

  indestructible(player, card, event);

  battalion(player, card, event);

  if (event == EVENT_DAT_ABILITY)
  {
    pump_until_eot(player, card, player, card, 5, 5);
  }

  return 0;
}

int card_teysa_envoy_of_ghosts(int player, int card, event_t event){
/* Teysa, Envoy of Ghosts	|5|W|B
 * Legendary Creature - Human Advisor 4/4
 * Vigilance, protection from creatures
 * Whenever a creature deals combat damage to you, destroy that creature.
 * 		Put a 1/1 |Swhite and |Sblack Spirit creature token with flying onto the battlefield. */

	check_legend_rule(player, card, event);

	vigilance(player, card, event);


	int flags = DDBM_TRACE_DAMAGING_SOURCES | DDBM_MUST_DAMAGE_TRIGGERING_PLAYER | DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE;

	check_damage_test(ANYBODY, -1, event, flags, player, card, TYPE_CREATURE, NULL);

	if( resolve_damage_trigger(ANYBODY, -1, event, flags, player, card) )
	{
		card_instance_t *instance = get_card_instance(player, card);

		int i;
		for(i=0; i<instance->info_slot; i++){
			if( instance->targets[i].player != -1 && instance->targets[i].card != -1 &&
				in_play(instance->targets[i].player, instance->targets[i].card) )
			{
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);

				token_generation_t token;
				default_token_definition(player, card, CARD_ID_SPIRIT, &token);
				token.pow = token.tou = 1;
				token.key_plus = KEYWORD_FLYING;
				token.color_forced = COLOR_TEST_WHITE | COLOR_TEST_BLACK;
				generate_token(&token);
			}
		}
		instance->info_slot = 0;
	}

	return 0;
}

int card_tithe_drinker(int player, int card, event_t event){
  /* Tithe Drinker	|W|B
   * Creature - Vampire 2/1
   * Lifelink (Damage dealt by this creature also causes you to gain that much life.)
   * Extort (Whenever you cast a spell, you may pay |WB. If you do, each opponent loses 1 life and you gain that much life.) */

	extort(player, card, event);
	lifelink(player, card, event);
	return 0;
}

int card_trostanis_summoner(int player, int card, event_t event){
/* Trostani's Summoner	|5|G|W
 * Creature - Elf Shaman 1/1
 * When ~ enters the battlefield, put a 2/2 |Swhite Knight creature token with vigilance, a 3/3 |Sgreen Centaur creature token,
 * 		and a 4/4 |Sgreen Rhino creature token with trample onto the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_KNIGHT, &token);
		token.pow = 2;
		token.tou = 2;
		token.s_key_plus = SP_KEYWORD_VIGILANCE;
		token.color_forced = COLOR_TEST_WHITE;
		generate_token(&token);

		default_token_definition(player, card, CARD_ID_CENTAUR, &token);
		token.pow = 3;
		token.tou = 3;
		token.color_forced = COLOR_TEST_GREEN;
		generate_token(&token);

		default_token_definition(player, card, CARD_ID_RHINO, &token);
		token.pow = 4;
		token.tou = 4;
		token.key_plus = KEYWORD_TRAMPLE;
		token.color_forced = COLOR_TEST_GREEN;
		generate_token(&token);
	}

	return 0;
}

int card_unflinching_courage(int player, int card, event_t event){
  /* Unflinching Courage	|1|G|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+2 and has trample and lifelink. (Damage dealt by the creature also causes its controller to gain that much life.) */

	return generic_aura(player, card, event, player, 2, 2, KEYWORD_TRAMPLE, SP_KEYWORD_LIFELINK, 0, 0, 0);
}

int card_varolz_the_scar_striped(int player, int card, event_t event){
	/* Varolz, the Scar-Striped	|1|B|G
	 * Legendary Creature - Troll Warrior 2/2
	 * Each creature card in your graveyard has scavenge. The scavenge cost is equal to its mana cost.
	 * Sacrifice another creature: Regenerate ~. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);
	test.not_me = 1;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_REGENERATE = 1,
		CHOICE_SCAVENGE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_REGENERATION | GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL);
		if( player == AI ){
			result = generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_REGENERATION, MANACOST0, 0, NULL, NULL,
																		0, &test, 0);
		}
		if( result ){
			return result;
		}
		return can_sorcery_be_played(player, event) && can_cast_creature_from_grave(player, 1, event) ? 1 : 0;
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_REGENERATION | GAA_SACRIFICE_CREATURE, MANACOST0, 0,
												NULL, NULL);
		if( player == AI ){
			result = generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, GAA_REGENERATION, MANACOST0, 0,
																		NULL, NULL, 0, &test, 0);
		}
		if( result ){
			if( player == AI ){
				return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_REGENERATION, MANACOST0, 0, NULL, NULL,
																		0, &test, 0);
			}
			else{
				generic_activated_ability(player, card, event, GAA_REGENERATION | GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL);
			}
			if( spell_fizzled != 1 ){
				instance->info_slot = CHOICE_REGENERATE;
			}
		}
		else{
			char msg[100] = "Select a creature card.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, msg);

			int selected = -1;
			if( player == AI ){
				selected = can_cast_creature_from_grave(player, 2, event);
			}
			else{
				selected = new_select_a_card(player, player, TUTOR_FROM_GRAVE, 0, 1, -1, &this_test);
			}
			if( selected != -1 ){
				int good = 1;
				const int *grave = get_grave(player);
				if( ! is_what(-1, grave[selected], TYPE_CREATURE) || ! has_mana_to_cast_id(player, event, cards_data[grave[selected]].id) ){
					good = 0;
				}
				if( good == 1 ){
					charge_mana_from_id(player, -1, event, cards_data[grave[selected]].id);
					if( spell_fizzled != 1 && pick_target(&td, "TARGET_CREATURE") ){
						instance->number_of_targets = 1;
						instance->targets[1].player = get_base_power_iid(player, grave[selected]);
						instance->info_slot = CHOICE_SCAVENGE;
						rfg_card_from_grave(player, selected);
					}
				}
				else{
					spell_fizzled = 1;
				}
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( instance->info_slot == CHOICE_REGENERATE && can_regenerate(player, instance->parent_card) ){
			regenerate_target(player, instance->parent_card);
		}
		if( instance->info_slot == CHOICE_SCAVENGE && valid_target(&td) ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, instance->targets[1].player);
		}
	}

	return 0;
}

int card_viashino_firstblade(int player, int card, event_t event){
  /* Viashino Firstblade	|1|R|W
   * Creature - Lizard Soldier 2/2
   * Haste
   * When this creature enters, it gets +2/+2 until end of turn. */

	haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		pump_until_eot_merge_previous(player, card, player, card, 2, 2);
	}

	return 0;
}

int card_voice_of_resurgence(int player, int card, event_t event){
	/* Voice of Resurgence	|G|W
	 * Creature - Elemental 2/2
	 * Whenever an opponent casts a spell during your turn or when ~ dies, put a |Sgreen and |Swhite Elemental creature token onto the battlefield with "This creature's power and toughness are each equal to the number of creatures you control." */

	if (current_turn == player){
		sp_ability_trigger(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
	}
	if (event == EVENT_SPA ||
		this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) )
	{
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
		token.special_infos = 67;
		token.color_forced = COLOR_TEST_WHITE | COLOR_TEST_GREEN;
		generate_token(&token);
	}

	return 0;
}

int card_vorel_of_the_hull_clade(int player, int card, event_t event)
{
  /* Vorel of the Hull Clade	|1|G|U
   * Legendary Creature - Human Merfolk 1/4
   * |G|U, |T: Double the number of each kind of counter on target artifact, creature, or land. */

  int t_player;
  card_instance_t *instance;
  int result;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 0x43);
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    result = instance->targets[0].card;
    t_player = instance->targets[0].player;
    copy_counters(player, card, t_player, result, t_player, result, 0);
  }
  result = generic_activated_ability(player, card, event, 5, 0, 0, 1, 1, 0, 0, 0, &td, "TWIDDLE");
  return result;
}

int card_warleaders_helix(int player, int card, event_t event){
	/* Warleader's Helix	|2|R|W	0x200a674
	 * Instant
	 * ~ deals 4 damage to any target and you gain 4 life. */
	/* Vampiric Feast	|5|B|B	0x200a674
	 * Sorcery
	 * ~ deals 4 damage to any target and you gain 4 life. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
			gain_life(player, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_warped_physique(int player, int card, event_t event){
	/* Warped Physique	|U|B
	 * Instant
	 * Target creature gets +X/-X until end of turn, where X is the number of cards in your hand. */
	return vanilla_instant_pump(player, card, event, 2, 1-player, hand_count[player], -hand_count[player], 0, 0);
}

// woodlot crawler --> vanilla

// zhur thaa ancient --> mana flare

int card_zhur_taa_druid(int player, int card, event_t event){
  /* Zhur-Taa Druid	|R|G
   * Creature - Human Druid 1/1
   * |T: Add |G.
   * Whenever you tap this creature for mana, it deals 1 damage to each opponent. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		damage_player(1-player, 1, instance->parent_controller, instance->parent_card);
	}
	else{
		return mana_producer(player, card, event);
	}

	return 0;
}

// Split

int card_alive_well(int player, int card, event_t event)
{
  /* Alive // Well	|3|G // |W
   * Sorcery // Sorcery
   * Alive:
   * Create a 3/3 green Centaur creature token.
   * Fuse (You may cast one or both halves of this card from your hand.)
   *
   * Well:
   * You gain 2 life for each creature you control.
   * Fuse (You may cast one or both halves of this card from your hand.) */

  card_instance_t *instance;
  int well_priority_or_creatures;
  int creature_count_local;
  unsigned int half_flags;
  token_generation_t token;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4ac9, 1, 0, 0x4b47, 1, 0);
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
      if ((player == 1) &&
          (well_priority_or_creatures = count_subtype(1, 2, -1), well_priority_or_creatures != 0))
      {
        well_priority_or_creatures = 0x14 - life[1];
        creature_count_local = count_subtype(1, 2, -1);
        well_priority_or_creatures = (creature_count_local + well_priority_or_creatures) * 2;
        return split_card_with_fuse(player, card, event, 0x4ac9, 1, 10, 0x4b47, 1,
                                    well_priority_or_creatures);
      }
    }
    else
    {
      if (event == EVENT_RESOLVE_SPELL)
      {
        half_flags = instance->info_slot;
        if ((half_flags & 1) != 0)
        {
          default_token_definition(player, card, CARD_ID_CENTAUR, &token);
          token.pow = 3;
          token.tou = 3;
          token.color_forced = '\b';
          generate_token(&token);
          half_flags = instance->info_slot;
        }
        if ((half_flags & 2) != 0)
        {
          well_priority_or_creatures = count_subtype(player, 2, -1);
          gain_life(player, well_priority_or_creatures * 2);
        }
        kill_card(player, card, KILL_DESTROY);
        well_priority_or_creatures = 0;
        return split_card_with_fuse(player, card, event, 0x4ac9, 1, 10, 0x4b47, 1,
                                    well_priority_or_creatures);
      }
      if (event != EVENT_CAN_CHANGE_TARGET)
      {
        well_priority_or_creatures = 0;
        if (event != EVENT_CHANGE_TARGET)
        {
          return 0;
        }
        return split_card_with_fuse(player, card, event, 0x4ac9, 1, 10, 0x4b47, 1,
                                    well_priority_or_creatures);
      }
    }
  }
  well_priority_or_creatures = 0;
  return split_card_with_fuse(player, card, event, 0x4ac9, 1, 10, 0x4b47, 1,
                              well_priority_or_creatures);
}

int card_armed_dangerous(int player, int card, event_t event)
{
  /* Armed // Dangerous	|1|R // |3|G
   * Sorcery // Sorcery
   * Armed:
   * Target creature gets +1/+1 and gains double strike until end of turn.
   * Fuse (You may cast one or both halves of this card from your hand.)
   *
   * Dangerous:
   * All creatures able to block target creature this turn do so.
   * Fuse (You may cast one or both halves of this card from your hand.) */

  card_instance_t *instance;
  int dangerous_available;
  unsigned int half_flags_or_target_count;
  target_definition_t td;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    dangerous_available = can_target(&td);
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4acb, can_target(&td), 0, 0x4ae3,
                                dangerous_available, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    if (new_pick_target(&td, "Select target creature for Armed.", -1, 0x81) == 0)
    {
      instance->number_of_targets = '\0';
    }
    return 0;
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    if (new_pick_target(&td, "Select target creature for Dangerous.", -1, 0x81) != 0)
    {
      return 0;
    }
    instance->number_of_targets = '\0';
    return 0;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      return 0;
  }
  else if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL &&
           event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;

  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  if (event == EVENT_RESOLVE_SPELL)
  {
    half_flags_or_target_count = instance->info_slot;
    if ((half_flags_or_target_count & 1) != 0)
    {
      if (validate_target(player, card, &td, 0) == 0)
      {
        half_flags_or_target_count = instance->info_slot;
      }
      else
      {
        pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                               1, 1, 0x40000000, 0);
        half_flags_or_target_count = instance->info_slot;
      }
    }
    if ((half_flags_or_target_count & 2) != 0)
    {
      half_flags_or_target_count = (unsigned int)instance->number_of_targets;
      if (validate_target(player, card, &td, half_flags_or_target_count - 1) != 0)
      {
        pump_ability_until_eot(
            player, card, instance->targets[half_flags_or_target_count - 1].player,
            instance->targets[half_flags_or_target_count - 1].card, 0, 0, 0, 0x400000);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  dangerous_available = can_target(&td);
  return split_card_with_fuse(player, card, event, 0x4acb, can_target(&td), 10, 0x4ae3,
                              dangerous_available, 5);
}

static int beck_legacy(int player, int card, event_t event){
	if( specific_cip(player, card, event, 2, 2, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0) ){
		draw_some_cards_if_you_want(player, card, player, 1);
	}
	if( eot_trigger(player, card, event) ){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_beck_call(int player, int card, event_t event)
{
  /* Beck // Call	|G|U // |4|W|U
   * Sorcery // Sorcery
   * Beck:
   * Whenever a creature enters this turn, you may draw a card.
   * Fuse (You may cast one or both halves of this card from your hand.)
   *
   * Call:
   * Create four 1/1 white Bird creature tokens with flying.
   * Fuse (You may cast one or both halves of this card from your hand.) */

  card_instance_t *instance;
  unsigned int half_flags;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4ad0, 1, 0, 0x4ad8, 1, 0);
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
    else if (event == EVENT_RESOLVE_SPELL)
    {
      half_flags = instance->info_slot;
      if ((half_flags & 1) != 0)
      {
        create_legacy_effect(player, card, beck_legacy);
        half_flags = instance->info_slot;
      }
      if ((half_flags & 2) != 0)
      {
        generate_tokens_by_id(player, card, CARD_ID_BIRD, 4);
      }
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return split_card_with_fuse(player, card, event, 0x4ad0, 1, 5, 0x4ad8, 1, 10);
}

int card_breaking_entering(int player, int card, event_t event)
{
  /* Breaking // Entering	|U|B // |4|B|R
   * Sorcery // Sorcery
   * Breaking:
   * Target player mills eight cards.
   * Fuse (You may cast one or both halves of this card from your hand.)
   *
   * Entering:
   * Put a creature card from a graveyard onto the battlefield under your control. It gains haste until end of turn.
   * Fuse (You may cast one or both halves of this card from your hand.) */

  card_instance_t *instance;
  unsigned int half_flags;
  int creature_count_local;
  int selected_grave_index;
  int grave_owner;
  target_definition_t *breaking_target_definition;
  int counts[2];
  target_definition_t td;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    target_definition_t td2;
    default_target_definition(player, card, &td2, 2);
    td2.zone = TARGET_ZONE_PLAYERS;
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4ad5, can_target(&td2), 0,
                                0x4af5, 1, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    target_definition_t td3;
    default_target_definition(player, card, &td3, 2);
    td3.zone = TARGET_ZONE_PLAYERS;
    if (new_pick_target(&td3, "Select target player for Breaking.", 0, 0x81) == 0)
    {
      instance->number_of_targets = '\0';
    }
    return 0;
  }
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.zone = TARGET_ZONE_PLAYERS;
  if (event == EVENT_RESOLVE_SPELL)
  {
    half_flags = instance->info_slot;
    if ((half_flags & 1) != 0)
    {
      if (validate_target(player, card, &td, 0) != 0)
      {
        mill(instance->targets[0].player, 8);
      }
      half_flags = instance->info_slot;
    }
    if ((half_flags & 2) != 0)
    {
      test_definition_t this_test;
      default_test_definition(&this_test, 2);
      grave_owner = 1 - player;
      creature_count_local = count_graveyard_by_type(0, 2);
      counts[0] = creature_count_local;
      counts[1] = count_graveyard_by_type(1, 2);
      if (0 < creature_count_local + counts[1])
      {
        creature_count_local = counts[grave_owner];
        bool selected_opponent = false;
        do
        {
          do
          {
            if (creature_count_local != 0)
            {
              memcpy(this_test.message,
                     "Select a creature card for Entering from (opponent's graveyard).\000", 65);
              selected_grave_index =
                  new_select_a_card(player, grave_owner, 2, 0, 2, -1, &this_test);
              if (selected_grave_index != -1)
              {
                selected_opponent = true;
                break;
              }
            }
          } while (counts[player] == 0);
          if (selected_opponent)
            break;
          memcpy(this_test.message,
                 "Select a creature card for Entering from (your graveyard).\000", 59);
          selected_grave_index = new_select_a_card(player, player, 2, 0, 2, -1, &this_test);
        } while (selected_grave_index == -1);
        if (!selected_opponent)
          grave_owner = player;
        reanimate_permanent(player, card, grave_owner, selected_grave_index,
                            REANIMATE_HASTE_UNTIL_EOT);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  breaking_target_definition = &td;
  return split_card_with_fuse(player, card, event, 0x4ad5, can_target(breaking_target_definition),
                              5, 0x4af5, 1, 10);
}

int card_catch_release(int player, int card, event_t event)
{
  /* Catch // Release	|1|U|R // |4|R|W
   * Sorcery // Sorcery
   * Catch:
   * Gain control of target permanent until end of turn. Untap it. It gains haste until end of turn.
   * Fuse (You may cast one or both halves of this card from your hand.)
   *
   * Release:
   * Each player sacrifices an artifact, a creature, an enchantment, a land, and a planeswalker of their choice.
   * Fuse (You may cast one or both halves of this card from your hand.) */

  card_instance_t *instance;
  unsigned int half_flags;
  int result;
  type_t type;
  type_t *sacrifice_type_cursor;
  type_t typs[5];
  target_definition_t td;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    default_target_definition(player, card, &td, 0x47);
    result = can_target(&td);
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4ad9, result, 0, 0x4b28, 1, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    default_target_definition(player, card, &td, 0x47);
    result = new_pick_target(&td, "Select target permanent for Catch.", -1, 0x81);
    if (result == 0)
    {
      instance->number_of_targets = '\0';
    }
    return 0;
  }
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  if (event == EVENT_RESOLVE_SPELL)
  {
    half_flags = instance->info_slot;
    if ((half_flags & 1) != 0)
    {
      result = validate_target(player, card, &td, 0);
      if (result != 0)
      {
        effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
      }
      half_flags = instance->info_slot;
    }
    if ((half_flags & 2) != 0)
    {
      sacrifice_type_cursor = typs;
      typs[0] = TYPE_ARTIFACT;
      typs[1] = TYPE_CREATURE;
      typs[2] = TYPE_ENCHANTMENT;
      typs[3] = TYPE_LAND;
      typs[4] = TARGET_TYPE_PLANESWALKER;
      type = TYPE_ARTIFACT;
      while (true)
      {
        impose_sacrifice(player, card, player, 1, type, 0, 0, 0, 0, 0, 0, 0, -1, 0);
        sacrifice_type_cursor = sacrifice_type_cursor + 1;
        impose_sacrifice(player, card, 1 - player, 1, type, 0, 0, 0, 0, 0, 0, 0, -1, 0);
        if (sacrifice_type_cursor == typs + 5)
          break;
        type = *sacrifice_type_cursor;
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = can_target(&td);
  return split_card_with_fuse(player, card, event, 0x4ad9, result, 10, 0x4b28, 1, 5);
}

int card_down_dirty(int player, int card, event_t event)
{
  /* Down // Dirty	|3|B // |2|G
   * Sorcery // Sorcery
   * Down:
   * Target player discards two cards.
   * Fuse (You may cast one or both halves of this card from your hand.)
   *
   * Dirty:
   * Return target card from your graveyard to your hand.
   * Fuse (You may cast one or both halves of this card from your hand.) */

  target_definition_t td3;
  uint8_t existing_target_count;
  card_instance_t *instance;
  int grave_count_or_selection;
  unsigned int half_flags;
  int opponent;
  unsigned int dirty_available;
  int down_priority;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    grave_count_or_selection = count_graveyard(player);
    dirty_available = 0;
    if (grave_count_or_selection != 0)
    {
      grave_count_or_selection = graveyard_has_shroud(player);
      dirty_available = (unsigned int)(grave_count_or_selection == 0);
    }
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4aef, can_target(&td), 0, 0x4aee,
                                dirty_available, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    target_definition_t td2;
    default_target_definition(player, card, &td2, 0);
    td2.zone = TARGET_ZONE_PLAYERS;
    if (new_pick_target(&td2, "Select target player for Down.", -1, 0x81) == 0)
    {
      instance->number_of_targets = '\0';
    }
    return 0;
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    test_definition_t this_test;
    existing_target_count = instance->number_of_targets;
    new_default_test_definition(&this_test, 0xff, "Select target creature card for Entering.");
    grave_count_or_selection = select_target_from_grave_source(
        player, card, player, SFG_0, 1, &this_test, (unsigned int)(existing_target_count != '\0'));
    if (grave_count_or_selection != -1)
    {
      return 0;
    }
    spell_fizzled = 1;
    instance->number_of_targets = '\0';
    return 0;
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET)
    return 0;
  default_target_definition(player, card, &td3, 2);
  td3.zone = TARGET_ZONE_PLAYERS;
  grave_count_or_selection = count_graveyard(player);
  opponent = 1 - player;
  dirty_available = 0;
  if (grave_count_or_selection != 0)
  {
    grave_count_or_selection = graveyard_has_shroud(player);
    dirty_available = (unsigned int)(grave_count_or_selection == 0);
  }
  down_priority = hand_count[opponent] * 5;
  if (event == EVENT_RESOLVE_SPELL)
  {
    half_flags = instance->info_slot;
    if ((half_flags & 1) != 0)
    {
      grave_count_or_selection = validate_target(player, card, &td3, 0);
      if (grave_count_or_selection == 0)
      {
        half_flags = instance->info_slot;
      }
      else
      {
        new_multidiscard(instance->targets[0].player, 2, 0, player);
        half_flags = instance->info_slot;
      }
    }
    if (((half_flags & 2) != 0) && (grave_count_or_selection = validate_target_from_grave_source(
                                        player, card, player, instance->number_of_targets - 1),
                                    grave_count_or_selection != -1))
    {
      from_grave_to_hand(player, grave_count_or_selection, 1);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return split_card_with_fuse(player, card, event, 0x4aef, can_target(&td3), down_priority, 0x4aee,
                              dirty_available, 10);
}

int card_far_away(int player, int card, event_t event)
{
  /* Far // Away	|1|U // |2|B
   * Instant // Instant
   * Far:
   * Return target creature to its owner's hand.
   * Fuse (You may cast one or both halves of this card from your hand.)
   *
   * Away:
   * Target player sacrifices a creature of their choice.
   * Fuse (You may cast one or both halves of this card from your hand.) */

  unsigned char away_target_count;
  card_instance_t *instance;
  int away_available;
  unsigned int half_flags;
  target_definition_t *away_target_definition;
  target_definition_t td;
  target_definition_t td2;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    default_target_definition(player, card, &td, 2);
    default_target_definition(player, card, &td2, 2);
    td2.zone = TARGET_ZONE_PLAYERS;
    away_available = can_target(&td2);
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4afa, can_target(&td), 0, 0x4ace,
                                away_available, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    default_target_definition(player, card, &td2, 2);
    if (new_pick_target(&td2, "Select target creature for Far.", -1, 0x81) == 0)
    {
      instance->number_of_targets = '\0';
    }
    return 0;
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    default_target_definition(player, card, &td2, 2);
    td2.zone = TARGET_ZONE_PLAYERS;
    if (new_pick_target(&td2, "Select target player for Away.", -1, 0x81) != 0)
    {
      return 0;
    }
    instance->number_of_targets = '\0';
    return 0;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      return 0;
  }
  else if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL &&
           event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;

  default_target_definition(player, card, &td, 2);
  default_target_definition(player, card, &td2, 2);
  td2.zone = TARGET_ZONE_PLAYERS;
  if (event == EVENT_RESOLVE_SPELL)
  {
    half_flags = instance->info_slot;
    if ((half_flags & 1) != 0)
    {
      if (validate_target(player, card, &td, 0) == 0)
      {
        half_flags = instance->info_slot;
      }
      else
      {
        bounce_permanent(instance->targets[0].player, instance->targets[0].card);
        half_flags = instance->info_slot;
      }
    }
    away_target_definition = &td2;
    if ((half_flags & 2) != 0)
    {
      away_target_count = instance->number_of_targets;
      if (validate_target(player, card, away_target_definition, away_target_count - 1) != 0)
      {
        impose_sacrifice(player, card, instance->targets[away_target_count - 1].player, 1, 2, 0, 0,
                         0, 0, 0, 0, 0, -1, 0);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  away_target_definition = &td2;
  away_available = can_target(away_target_definition);
  return split_card_with_fuse(player, card, event, 0x4afa, can_target(&td), 8, 0x4ace,
                              away_available, 10);
}

int card_flesh_blood(int player, int card, event_t event)
{
  /* Flesh // Blood	|3|B|G|/|R|G	0x200a6a6
   * Sorcery // Sorcery
   * Exile target creature card from a graveyard. Put X +1/+1 counters on target creature, where X is the power of the card you exiled.
   * //
   * Target creature you control deals damage equal to its power to any target.
   * Fuse */

  target_definition_t td5;
  card_instance_t *instance;
  int amt;
  unsigned int blood_available;
  int source_player_or_base_power;
  int source_card_or_result;
  unsigned int flesh_available;
  int blood_target_index;
  target_definition_t td;
  target_definition_t td2;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    target_definition_t td3;
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td2 = td;
    td2.allowed_controller = player;
    default_target_definition(player, card, &td3, 0x102);
    td3.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    source_card_or_result = count_graveyard_by_type(2, 2);
    if ((source_card_or_result == 0) ||
        (source_card_or_result = graveyard_has_shroud(2), source_card_or_result != 0))
    {
      flesh_available = 0;
    }
    else
    {
      source_card_or_result = can_target(&td);
      flesh_available = (unsigned int)(source_card_or_result != 0);
    }
    source_card_or_result = can_target(&td2);
    blood_available = 0;
    if (source_card_or_result != 0)
    {
      source_card_or_result = can_target(&td3);
      blood_available = (unsigned int)(source_card_or_result != 0);
    }
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4b00, flesh_available, 0, 0x4ad2,
                                blood_available, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    test_definition_t this_test;
    new_default_test_definition(&this_test, 2, "Select target creature card for Flesh.");
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = player;
    if (new_pick_target(&td2, "Select target creature for Flesh.", 0, 0x81) == 0)
    {
      spell_fizzled = 1;
      instance->number_of_targets = '\0';
    }
    else
    {
      source_card_or_result =
          select_target_from_either_grave(player, card, SFG_0, 2, 2, &this_test, 1, 2);
      instance->number_of_targets = source_card_or_result != -1;
    }
    return 0;
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    target_definition_t td4;
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = player;
    td2.allowed_controller = player;
    default_target_definition(player, card, &td4, 0x102);
    td4.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    source_card_or_result = 0;
    if ((instance->info_slot & 1U) != 0)
    {
      source_card_or_result = 3;
    }
    source_player_or_base_power = new_pick_target(
        &td2, "Select target creature you control for Blood.", source_card_or_result, 0x81);
    if ((source_player_or_base_power != 0) &&
        (source_card_or_result =
             new_pick_target(&td4, "Select target creature, player, or planeswalker for Blood.",
                             source_card_or_result + 1, 0x81),
         source_card_or_result != 0))
    {
      return 0;
    }
    instance->number_of_targets = '\0';
    return 0;
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET)
    return 0;
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td2 = td;
  td2.allowed_controller = player;
  default_target_definition(player, card, &td5, 0x102);
  td5.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  source_card_or_result = count_graveyard_by_type(2, 2);
  if ((source_card_or_result == 0) ||
      (source_card_or_result = graveyard_has_shroud(2), source_card_or_result != 0))
  {
    flesh_available = 0;
  }
  else
  {
    source_card_or_result = can_target(&td);
    flesh_available = (unsigned int)(source_card_or_result != 0);
  }
  source_card_or_result = can_target(&td2);
  blood_available = 0;
  if (source_card_or_result != 0)
  {
    source_card_or_result = can_target(&td5);
    blood_available = (unsigned int)(source_card_or_result != 0);
  }
  if (event != EVENT_RESOLVE_SPELL)
    return split_card_with_fuse(player, card, event, 0x4b00, flesh_available, 8, 0x4ad2,
                                blood_available, 10);
  if ((instance->info_slot & 1U) != 0)
  {
    source_card_or_result = validate_target(player, card, &td, 0);
    if ((source_card_or_result != 0) && (source_card_or_result = validate_target_from_grave_source(
                                             player, card, instance->targets[1].player, 2),
                                         source_card_or_result != -1))
    {
      source_player_or_base_power = instance->targets[1].player;
      source_player_or_base_power = get_base_power_iid(
          source_player_or_base_power,
          graveyard_ptr_mutable[source_player_or_base_power][source_card_or_result]);
      rfg_card_from_grave(instance->targets[1].player, source_card_or_result);
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                   COUNTER_P1_P1, source_player_or_base_power);
    }
  }
  if ((instance->info_slot & 2U) != 0)
  {
    blood_target_index = (instance->info_slot & 1U) != 0 ? 3 : 0;
    source_card_or_result = validate_target(player, card, &td2, blood_target_index);
    if ((source_card_or_result != 0) &&
        (source_card_or_result = validate_target(player, card, &td5, blood_target_index + 1),
         source_card_or_result != 0))
    {
      source_card_or_result = instance->targets[blood_target_index].card;
      source_player_or_base_power = instance->targets[blood_target_index].player;
      amt = get_power(source_player_or_base_power, source_card_or_result);
      damage_creature(instance->targets[blood_target_index + 1].player,
                      instance->targets[blood_target_index + 1].card, amt,
                      source_player_or_base_power, source_card_or_result);
    }
  }
  kill_card(player, card, KILL_DESTROY);
  return split_card_with_fuse(player, card, event, 0x4b00, flesh_available, 8, 0x4ad2,
                              blood_available, 10);
}

int card_give_take(int player, int card, event_t event)
{
  /* Give // Take	|2|G // |2|U
   * Sorcery // Sorcery
   * Give:
   * Put three +1/+1 counters on target creature.
   * Fuse (You may cast one or both halves of this card from your hand.)
   *
   * Take:
   * Remove all +1/+1 counters from target creature you control. Draw that many cards.
   * Fuse (You may cast one or both halves of this card from your hand.) */

  card_instance_t *instance;
  int take_available_or_counters;
  unsigned int half_flags_or_target_count;
  target_definition_t *give_target_definition;
  target_definition_t td;
  target_definition_t td2;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = player;
    td2.allowed_controller = player;
    if (player == 1)
    {
      td2.special = TARGET_SPECIAL_REQUIRES_COUNTER;
      td2.extra = 0x200;
    }
    take_available_or_counters = can_target(&td2);
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4b01, can_target(&td), 0, 0x4b3c,
                                take_available_or_counters, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = player;
    if (new_pick_target(&td2, "Select target creature for Give.", -1, 0x81) == 0)
    {
      instance->number_of_targets = '\0';
    }
    return 0;
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = player;
    td2.allowed_controller = player;
    if (player == 1)
    {
      td2.special = TARGET_SPECIAL_REQUIRES_COUNTER;
      td2.extra = 0x200;
    }
    if (new_pick_target(&td2, "Select target creature for Take.", -1, 0x81) != 0)
    {
      return 0;
    }
    instance->number_of_targets = '\0';
    return 0;
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET)
    return 0;
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  default_target_definition(player, card, &td2, 2);
  td2.preferred_controller = player;
  td2.allowed_controller = player;
  if (player == 1)
  {
    td2.special = TARGET_SPECIAL_REQUIRES_COUNTER;
    td2.extra = 0x200;
  }
  give_target_definition = &td;
  td2.allowed_controller = player;
  td2.preferred_controller = td2.allowed_controller;
  if (event == EVENT_RESOLVE_SPELL)
  {
    half_flags_or_target_count = instance->info_slot;
    if ((half_flags_or_target_count & 1) != 0)
    {
      if (validate_target(player, card, give_target_definition, 0) == 0)
      {
        half_flags_or_target_count = instance->info_slot;
      }
      else
      {
        add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                     COUNTER_P1_P1, 3);
        half_flags_or_target_count = instance->info_slot;
      }
    }
    if ((half_flags_or_target_count & 2) != 0)
    {
      half_flags_or_target_count = (unsigned int)instance->number_of_targets;
      if (validate_target(player, card, &td2, half_flags_or_target_count - 1) != 0)
      {
        take_available_or_counters =
            count_counters(instance->targets[half_flags_or_target_count - 1].player,
                           instance->targets[half_flags_or_target_count - 1].card, COUNTER_P1_P1);
        remove_counters(player, card, instance->targets[half_flags_or_target_count - 1].player,
                        instance->targets[half_flags_or_target_count - 1].card, COUNTER_P1_P1,
                        take_available_or_counters);
        draw_cards(player, take_available_or_counters);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  give_target_definition = &td;
  take_available_or_counters = can_target(&td2);
  return split_card_with_fuse(player, card, event, 0x4b01, can_target(give_target_definition), 10,
                              0x4b3c, take_available_or_counters, 8);
}

int card_profit_loss(int player, int card, event_t event)
{
  /* Profit // Loss	|1|W // |2|B
   * Instant // Instant
   * Profit:
   * Creatures you control get +1/+1 until end of turn.
   * Fuse (You may cast one or both halves of this card from your hand.)
   *
   * Loss:
   * Creatures your opponents control get -1/-1 until end of turn.
   * Fuse (You may cast one or both halves of this card from your hand.) */

  card_instance_t *instance;
  unsigned int half_flags;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4b1e, 1, 0, 0x4b0e, 1, 0);
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
    else if (event == EVENT_RESOLVE_SPELL)
    {
      half_flags = instance->info_slot;
      if ((half_flags & 1) != 0)
      {
        pump_subtype_until_eot(player, card, player, -1, 1, 1, 0, 0);
        half_flags = instance->info_slot;
      }
      if ((half_flags & 2) != 0)
      {
        pump_subtype_until_eot(player, card, 1 - player, -1, -1, -1, 0, 0);
      }
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return split_card_with_fuse(player, card, event, 0x4b1e, 1, 8, 0x4b0e, 1, 10);
}

int card_protect_serve(int player, int card, event_t event)
{
  /* Protect // Serve	|2|W // |1|U
   * Instant // Instant
   * Protect:
   * Target creature gets +2/+4 until end of turn.
   * Fuse (You may cast one or both halves of this card from your hand.)
   *
   * Serve:
   * Target creature gets -6/-0 until end of turn.
   * Fuse (You may cast one or both halves of this card from your hand.) */

  card_instance_t *instance;
  int serve_available;
  unsigned int half_flags_or_target_count;
  target_definition_t *serve_target_definition;
  target_definition_t td;
  target_definition_t td2;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    default_target_definition(player, card, &td2, 2);
    serve_available = can_target(&td2);
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4b1f, can_target(&td), 0, 0x4b32,
                                serve_available, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = player;
    if (new_pick_target(&td2, "Select target creature for Protect.", -1, 0x81) == 0)
    {
      instance->number_of_targets = '\0';
    }
    return 0;
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    default_target_definition(player, card, &td2, 2);
    if (new_pick_target(&td2, "Select target creature for Serve.", -1, 0x81) != 0)
    {
      return 0;
    }
    instance->number_of_targets = '\0';
    return 0;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      return 0;
  }
  else if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL &&
           event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;

  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  default_target_definition(player, card, &td2, 2);
  if (event == EVENT_RESOLVE_SPELL)
  {
    half_flags_or_target_count = instance->info_slot;
    if ((half_flags_or_target_count & 1) != 0)
    {
      if (validate_target(player, card, &td, 0) == 0)
      {
        half_flags_or_target_count = instance->info_slot;
      }
      else
      {
        pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 4);
        half_flags_or_target_count = instance->info_slot;
      }
    }
    if ((half_flags_or_target_count & 2) != 0)
    {
      half_flags_or_target_count = (unsigned int)instance->number_of_targets;
      if (validate_target(player, card, &td, half_flags_or_target_count - 1) != 0)
      {
        pump_until_eot(player, card, instance->targets[half_flags_or_target_count - 1].player,
                       instance->targets[half_flags_or_target_count - 1].card, -6, 0);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  serve_target_definition = &td2;
  serve_available = can_target(serve_target_definition);
  return split_card_with_fuse(player, card, event, 0x4b1f, can_target(&td), 5, 0x4b32,
                              serve_available, 5);
}

int card_ready_willing(int player, int card, event_t event)
{
  /* Ready // Willing	|1|G|W // |1|W|B
   * Instant // Instant
   * Ready:
   * Creatures you control gain indestructible until end of turn. Untap each creature you control.
   * Fuse (You may cast one or both halves of this card from your hand.)
   *
   * Willing:
   * Creatures you control gain deathtouch and lifelink until end of turn.
   * Fuse (You may cast one or both halves of this card from your hand.) */

  card_instance_t *instance;
  unsigned int half_flags;
  test_definition_t this_test;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4b23, 1, 0, 0x4b4d, 1, 0);
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
    else if (event == EVENT_RESOLVE_SPELL)
    {
      half_flags = instance->info_slot;
      if ((half_flags & 1) != 0)
      {
        pump_creatures_until_eot(player, card, player, 1, 0, 0, 0, 0x800, NULL);
        default_test_definition(&this_test, 2);
        new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
        half_flags = instance->info_slot;
      }
      if ((half_flags & 2) != 0)
      {
        pump_creatures_until_eot(player, card, player, 2, 0, 0, 0, 0x202, NULL);
      }
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return split_card_with_fuse(player, card, event, 0x4b23, 1, 10, 0x4b4d, 1, 8);
}

int card_toil_trouble(int player, int card, event_t event)
{
  /* Toil // Trouble	|2|B // |2|R
   * Sorcery // Sorcery
   * Toil:
   * Target player draws two cards and loses 2 life.
   * Fuse (You may cast one or both halves of this card from your hand.)
   *
   * Trouble:
   * Trouble deals damage to target player equal to the number of cards in that player's hand.
   * Fuse (You may cast one or both halves of this card from your hand.) */

  unsigned char trouble_target_count;
  card_instance_t *instance;
  unsigned int half_flags;
  int trouble_available;
  int can_play_first_half;
  int trouble_priority_or_player;
  int toil_priority;
  target_definition_t *trouble_target_definition;
  target_definition_t td;
  target_definition_t td2;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    default_target_definition(player, card, &td, 0);
    td.preferred_controller = player;
    td.zone = TARGET_ZONE_PLAYERS;
    default_target_definition(player, card, &td2, 0);
    td2.zone = TARGET_ZONE_PLAYERS;
    trouble_priority_or_player = can_target(&td2);
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4b3e, can_target(&td), 0, 0x4b40,
                                trouble_priority_or_player, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    default_target_definition(player, card, &td2, 0);
    td2.zone = TARGET_ZONE_PLAYERS;
    td2.preferred_controller = player;
    if (new_pick_target(&td2, "Select target player for Toil.", -1, 0x81) == 0)
    {
      instance->number_of_targets = '\0';
    }
    return 0;
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    default_target_definition(player, card, &td2, 0);
    td2.zone = TARGET_ZONE_PLAYERS;
    if (new_pick_target(&td2, "Select target player for Trouble.", -1, 0x81) != 0)
    {
      return 0;
    }
    instance->number_of_targets = '\0';
    return 0;
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET)
    return 0;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.preferred_controller = player;
  default_target_definition(player, card, &td2, 0);
  td2.zone = TARGET_ZONE_PLAYERS;
  trouble_priority_or_player = 5;
  toil_priority = hand_count[player] * -3 + 0x15 + life[player];
  if (event == EVENT_RESOLVE_SPELL)
  {
    half_flags = instance->info_slot;
    if ((half_flags & 1) != 0)
    {
      if (validate_target(player, card, &td, 0) != 0)
      {
        draw_cards(instance->targets[0].player, 2);
        lose_life(instance->targets[0].player, 2);
      }
      half_flags = instance->info_slot;
    }
    if ((half_flags & 2) != 0)
    {
      trouble_target_count = instance->number_of_targets;
      if (validate_target(player, card, &td, trouble_target_count - 1) != 0)
      {
        trouble_priority_or_player = instance->targets[trouble_target_count - 1].player;
        damage_player(trouble_priority_or_player, hand_count[trouble_priority_or_player], player,
                      card);
      }
    }
    kill_card(player, card, KILL_DESTROY);
    trouble_priority_or_player = 5;
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if (((card == affected_card) && (affected_card_controller == player)) &&
        ((player == 1 && (trouble_priority_or_player = would_validate_arbitrary_target(&td2, 0, -1),
                          trouble_priority_or_player != 0))))
    {
      trouble_priority_or_player = hand_count[0] * 3;
    }
    else
      trouble_priority_or_player = 5;
  }
  trouble_target_definition = &td2;
  trouble_available = can_target(trouble_target_definition);
  can_play_first_half = can_target(&td);
  return split_card_with_fuse(player, card, event, 0x4b3e, can_play_first_half, toil_priority,
                              0x4b40, trouble_available, trouble_priority_or_player);
}

int card_turn_burn(int player, int card, event_t event)
{
  /* Turn // Burn	|2|U|/|1|R	0x200a6c4
   * Instant // Instant
   * Until end of turn, target creature loses all abilities and becomes a |Sred Weird with base power and toughness 0/1.
   * //
   * Burn deals 2 damage to any target.
   * Fuse */

  card_instance_t *instance;
  int burn_available;
  unsigned int half_flags_or_target_count;
  target_definition_t *burn_target_definition;
  target_definition_t td;
  target_definition_t td2;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    target_definition_t td3;
    default_target_definition(player, card, &td2, 2);
    default_target_definition(player, card, &td3, 0x102);
    td3.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    burn_available = can_target(&td3);
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4b42, can_target(&td2), 0,
                                0x4ad6, burn_available, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    target_definition_t td4;
    default_target_definition(player, card, &td4, 2);
    if (new_pick_target(&td4, "Select target creature for Turn.", -1, 0x81) == 0)
    {
      instance->number_of_targets = '\0';
    }
    return 0;
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    target_definition_t td5;
    default_target_definition(player, card, &td5, 0x102);
    td5.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if (new_pick_target(&td5, "Select target creature, player, or planeswalker for Burn.", -1,
                        0x81) != 0)
    {
      return 0;
    }
    instance->number_of_targets = '\0';
    return 0;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      return 0;
  }
  else if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL &&
           event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;

  default_target_definition(player, card, &td, 2);
  default_target_definition(player, card, &td2, 0x102);
  td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if (event == EVENT_RESOLVE_SPELL)
  {
    half_flags_or_target_count = instance->info_slot;
    if ((half_flags_or_target_count & 1) != 0)
    {
      if (validate_target(player, card, &td, 0) == 0)
      {
        half_flags_or_target_count = instance->info_slot;
      }
      else
      {
        test_definition_t hc;
        default_test_definition(&hc, 0);
        hc.power = 0;
        hc.toughness = 1;
        hc.subtype = SUBTYPE_WEIRD;
        hc.color = get_sleighted_color_test(player, card, 0x10);
        humiliate_and_set_pt_abilities(player, card, instance->targets[0].player,
                                       instance->targets[0].card, 4, &hc);
        half_flags_or_target_count = instance->info_slot;
      }
    }
    burn_target_definition = &td2;
    if ((half_flags_or_target_count & 2) != 0)
    {
      half_flags_or_target_count = (unsigned int)instance->number_of_targets;
      if (validate_target(player, card, burn_target_definition, half_flags_or_target_count - 1) !=
          0)
      {
        damage_creature(instance->targets[half_flags_or_target_count - 1].player,
                        instance->targets[half_flags_or_target_count - 1].card, 2, player, card);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  burn_target_definition = &td2;
  burn_available = can_target(burn_target_definition);
  return split_card_with_fuse(player, card, event, 0x4b42, can_target(&td), 5, 0x4ad6,
                              burn_available, 10);
}

int card_wear_tear(int player, int card, event_t event)
{
  /* Wear // Tear	|1|R // |W
   * Instant // Instant
   * Wear:
   * Destroy target artifact.
   * Fuse (You may cast one or both halves of this card from your hand.)
   *
   * Tear:
   * Destroy target enchantment.
   * Fuse (You may cast one or both halves of this card from your hand.) */

  card_instance_t *instance;
  int tear_available;
  unsigned int half_flags_or_target_count;
  target_definition_t *tear_target_definition;
  target_definition_t td;
  target_definition_t td2;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    default_target_definition(player, card, &td, 0x40);
    default_target_definition(player, card, &td2, 4);
    tear_available = can_target(&td2);
    return split_card_with_fuse(player, card, EVENT_MODIFY_COST, 0x4b46, can_target(&td), 0, 0x4b3d,
                                tear_available, 0);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    default_target_definition(player, card, &td2, 0x40);
    if (new_pick_target(&td2, "Select target artifact for Wear.", -1, 0x81) == 0)
    {
      instance->number_of_targets = '\0';
    }
    return 0;
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    default_target_definition(player, card, &td2, 4);
    if (new_pick_target(&td2, "Select target enchantment for Tear.", -1, 0x81) != 0)
    {
      return 0;
    }
    instance->number_of_targets = '\0';
    return 0;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      return 0;
  }
  else if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL &&
           event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;

  default_target_definition(player, card, &td, 0x40);
  default_target_definition(player, card, &td2, 4);
  if (event == EVENT_RESOLVE_SPELL)
  {
    half_flags_or_target_count = instance->info_slot;
    if ((half_flags_or_target_count & 1) != 0)
    {
      if (validate_target(player, card, &td, 0) == 0)
      {
        half_flags_or_target_count = instance->info_slot;
      }
      else
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        half_flags_or_target_count = instance->info_slot;
      }
    }
    tear_target_definition = &td2;
    if ((half_flags_or_target_count & 2) != 0)
    {
      half_flags_or_target_count = (unsigned int)instance->number_of_targets;
      if (validate_target(player, card, tear_target_definition, half_flags_or_target_count - 1) !=
          0)
      {
        kill_card(instance->targets[half_flags_or_target_count - 1].player,
                  instance->targets[half_flags_or_target_count - 1].card, KILL_DESTROY);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  tear_target_definition = &td2;
  tear_available = can_target(tear_target_definition);
  return split_card_with_fuse(player, card, event, 0x4b46, can_target(&td), 8, 0x4b3d,
                              tear_available, 10);
}

// Artifacts

int card_azorius_cluestone(int player, int card, event_t event){
	/* Abzan Banner	|3
	 * Artifact
	 * |T: Add |W, |B, or |G to your mana pool.
	 * |W|B|G, |T, Sacrifice ~: Draw a card. */
	/* Azorius Cluestone	|3
	 * Artifact
	 * |T: Add |W or |U to your mana pool.
	 * |W|U, |T, Sacrifice ~: Draw a card. */
	/* Boros Cluestone	|3
	 * Artifact
	 * |T: Add |R or |W to your mana pool.
	 * |R|W, |T, Sacrifice ~: Draw a card. */
	/* Dimir Cluestone	|3
	 * Artifact
	 * |T: Add |U or |B to your mana pool.
	 * |U|B, |T, Sacrifice ~: Draw a card. */
	/* Golgari Cluestone	|3
	 * Artifact
	 * |T: Add |B or |G to your mana pool.
	 * |B|G, |T, Sacrifice ~: Draw a card. */
	/* Gruul Cluestone	|3
	 * Artifact
	 * |T: Add |R or |G to your mana pool.
	 * |R|G, |T, Sacrifice ~: Draw a card. */
	/* Izzet Cluestone	|3
	 * Artifact
	 * |T: Add |U or |R to your mana pool.
	 * |U|R, |T, Sacrifice ~: Draw a card. */
	/* Jeskai Banner	|3
	 * Artifact
	 * |T: Add |U, |R, or |W to your mana pool.
	 * |U|R|W, |T, Sacrifice ~: Draw a card. */
	/* Mardu Banner	|3
	 * Artifact
	 * |T: Add |R, |W, or |B to your mana pool.
	 * |R|W|B, |T, Sacrifice ~: Draw a card. */
	/* Orzhov Cluestone	|3
	 * Artifact
	 * |T: Add |W or |B to your mana pool.
	 * |W|B, |T, Sacrifice ~: Draw a card. */
	/* Rakdos Cluestone	|3
	 * Artifact
	 * |T: Add |B or |R to your mana pool.
	 * |B|R, |T, Sacrifice ~: Draw a card. */
	/* Selesnya Cluestone	|3
	 * Artifact
	 * |T: Add |G or |W to your mana pool.
	 * |G|W, |T, Sacrifice ~: Draw a card. */
	/* Simic Cluestone	|3
	 * Artifact
	 * |T: Add |G or |U to your mana pool.
	 * |G|U, |T, Sacrifice ~: Draw a card. */
	/* Sultai Banner	|3
	 * Artifact
	 * |T: Add |B, |G, or |U to your mana pool.
	 * |B|G|U, |T, Sacrifice ~: Draw a card. */
	/* Temur Banner	|3
	 * Artifact
	 * |T: Add |G, |U, or |R to your mana pool.
	 * |G|U|R, |T, Sacrifice ~: Draw a card. */

	card_instance_t *instance = get_card_instance( player, card );

	if( ! IS_GAA_EVENT(event) && event != EVENT_COUNT_MANA ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( instance->targets[1].player == 66 ){
			return 0;
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( instance->targets[1].player == 66 ){
			return 0;
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int ai_choice = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL) ){
			instance->targets[1].player = 66;
			int cl[5] = {0, 0, 0, 0, 0};
			int i;
			for(i=0; i<5; i++){
				if( cards_data[instance->internal_card_id].color & (1<<(i+1)) ){
					cl[i]++;
				}
			}
			if( has_mana_for_activated_ability(player, card, 0, cl[0], cl[1], cl[2], cl[3], cl[4]) &&
				can_sacrifice_as_cost(player, 1, TYPE_ARTIFACT, 0, 0, 0, 0, 0, 0, 0, -1, 0)
			  ){
				ai_choice = 1;
			}
			instance->targets[1].player = 0;
		}
		int choice = 0;
		if( ai_choice == 1 ){
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Sac & draw\n Cancel", ai_choice);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		if( choice == 1 ){
			instance->targets[1].player = 66;
			int cl[COLOR_WHITE + 1] = {0};
			int i;
			for (i = COLOR_BLACK; i <= COLOR_WHITE; ++i){
				if (cards_data[instance->internal_card_id].color & (1 << i)){
					++cl[i];
				}
			}
			charge_mana_for_activated_ability(player, card, cl[0], cl[1], cl[2], cl[3], cl[4], cl[5]);
			if( spell_fizzled != 1 ){
				instance->info_slot = 1;
				tap_card(player, card);
				kill_card(player, card, KILL_SACRIFICE);
			}
			else{
				 instance->targets[1].player = 0;
			}
		}
		if( choice == 2){
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
			draw_cards(player, 1);
		}
		else{
			return mana_producer(player, card, event);
		}
	}

	return 0;
}

// Lands

static int get_gate_code(int csvid){
	int gates[10] = { CARD_ID_AZORIUS_GUILDGATE, CARD_ID_BOROS_GUILDGATE, CARD_ID_DIMIR_GUILDGATE, CARD_ID_GOLGARI_GUILDGATE,
					CARD_ID_GRUUL_GUILDGATE, CARD_ID_IZZET_GUILDGATE, CARD_ID_ORZHOV_GUILDGATE, CARD_ID_RAKDOS_GUILDGATE,
					CARD_ID_SELESNYA_GUILDGATE, CARD_ID_SIMIC_GUILDGATE };
	int k;
	for (k = 0; k < 10; ++k){
		if (csvid == gates[k]){
			return 1<<k;
		}
	}

	return 0;
}

static int get_global_gate_code(int player){
	int score = 0;
	int c;
	for (c = 0; c < active_cards_count[player]; ++c){
		if (in_play(player, c) && has_subtype(player, c, SUBTYPE_GATE)){
			score |= get_gate_code(get_id(player, c));
		}
	}

	return score;
}

static int count_gates(int player){
	return num_bits_set(get_global_gate_code(player));
}

int card_mazes_end(int player, int card, event_t event)
{
  /* Maze's End	""
   * Land
   * ~ enters the battlefield tapped.
   * |T: Add |C to your mana pool.
   * |3, |T, Return ~ to its owner's hand: Search your library for a Gate card, put it onto the
   * battlefield, then shuffle your library. If you control ten or more Gates with different names,
   * you win the game. */

  card_instance_t *instance = get_card_instance(player, card);

  if (!is_humiliated(player, card, AL_TRIG))
    comes_into_play_tapped(player, card, event);

  if (event == EVENT_SHOULD_AI_PLAY)
  {
    int gates = count_gates(player);
    if (player == AI)
    {
      ai_modifier += gates * gates * gates;
    }
    else if (gates >= 7)
    {
      ai_modifier -= gates * gates * gates;
    }
    if (gates >= 10)
    {
      lose_the_game(1 - player);
    }
  }

  if (event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION)
  {
    return mana_producer(player, card, event);
  }

  if (event == EVENT_ACTIVATE)
  {
    int choice = instance->info_slot = 0;
    if (!paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED,
                                                    MANACOST_X(4), 0, NULL, NULL))
    {
      choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Tutor a Gate\n Cancel", 1);
    }

    if (choice == 0)
    {
      return mana_producer(player, card, event);
    }

    if (choice == 1)
    {
      add_state(player, card, STATE_TAPPED);
      charge_mana_for_activated_ability(player, card, MANACOST_X(3));
      if (spell_fizzled != 1)
      {
        instance->info_slot = 1;
        tap_card(player, card);
        bounce_permanent(player, card);
      }
      else
      {
        remove_state(player, card, STATE_TAPPED);
      }
      tapped_for_mana_color = -1;
    }
    if (choice == 2)
    {
      spell_fizzled = 1;
    }
  }

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->info_slot > 0)
    {
      test_definition_t this_test;
      new_default_test_definition(&this_test, TYPE_LAND, "Select a Gate card.");
      this_test.subtype = SUBTYPE_GATE;
      if (player == HUMAN)
      {
        new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_FIRST_FOUND,
                         &this_test);
      }
      else
      {
        int code = get_global_gate_code(player);
        int *deck = deck_ptr[player];
        int gate_array[2][count_deck(player)];
        int gac = 0;
        int count = 0;
        while (deck[count] != -1)
        {
          if (has_subtype_by_id(player, cards_data[deck[count]].id, SUBTYPE_GATE))
          {
            if (!(get_gate_code(cards_data[deck[count]].id) & code))
            {
              gate_array[0][gac] = deck[count];
              gate_array[1][gac] = count;
              gac++;
            }
          }
          count++;
        }
        int selected = select_card_from_zone(player, player, gate_array[0], gac, 0, AI_MAX_VALUE,
                                             -1, &this_test);
        if (selected != -1)
        {
          put_into_play_a_card_from_deck(player, player, gate_array[1][selected]);
        }
      }
      if (count_gates(player) >= 10)
      {
        lose_the_game(1 - player);
      }
    }
    else
    {
      return mana_producer(player, card, event);
    }
  }

  return 0;
}
