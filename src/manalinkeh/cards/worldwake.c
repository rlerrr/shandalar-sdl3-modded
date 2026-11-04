#include "manalink.h"

int zendikon(int player, int card, event_t event, int pow, int tgh, color_test_t cols,
             subtype_t styp, keyword_t kws, sp_keyword_t spkws);

int do_multikicker(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white){
	int result = 0;
	int diff = 0;
	if( ! played_for_free(player, card) ){
		diff = MIN(0, cards_ptr[get_id(player, card)]->req.req_colorless+true_get_updated_casting_cost(player, card, -1, event, -1));
	}
	int cless = colorless+diff;
	while( 1 ){
			int ctp = MAX(0, cless);
			if( cless > 0 ){
				ctp = MIN(cless, colorless);
			}
			cless+=colorless;
			if( has_mana_multi(player, ctp, black, blue, green, red, white) ){
				int choice = do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass", 0);
				if( choice == 0 ){
					charge_mana_multi(player, ctp, black, blue, green, red, white);
					if( spell_fizzled != 1 ){
						set_special_flags(player, card, SF_KICKED);
						result++;
					}
				}
				else{
					break;
				}
			}
			else{
				break;
			}
	}
	return result;
}

int multikicker(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white){

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->info_slot = 0;
		if( ! is_token(player, card) && ! check_special_flags(player, card, SF_NOT_CAST) ){
			if( spell_fizzled != 1 ){
				instance->info_slot = do_multikicker(player, card, event, colorless, black, blue, green, red, white);
			}
		}
	}
	return 0;
}

static int multikicked(int player, int card){
	card_instance_t *instance = get_card_instance(player, card);
	return instance->info_slot;
}

/* ManalinkEh 2022: 0x2583370; original cards/worldwake.c:55. */
static int zendikon_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = in_play(player, card);
  if (instance && instance->targets[0].player != -1 &&
      resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    int i = count_graveyard(instance->targets[0].player) - 1;
    if (i >= 0)
    {
      int target_player = instance->targets[0].player;
      uint16_t land_csv_id = cards_data[instance->targets[0].card].id;
      for (; i >= 0; --i)
        if (cards_data[graveyard_ptr_mutable[target_player][i]].id == land_csv_id)
        {
          from_grave_to_hand(target_player, i, 0);
          break;
        }
    }
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2583ed0; original cards/worldwake.c:72. */
int zendikon(int player, int card, event_t event, int pow, int tgh, color_test_t cols,
             subtype_t styp, keyword_t kws, sp_keyword_t spkws)
{
  enum
  {
    SP_KEYWORD_NOT_SIMPLE_BITS = 357809216
  };

  int land_card_or_iid;
  card_instance_t *instance;
  int death_check_or_owner;
  int target_player;
  target_definition_t td;

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = in_play(player, card);
    if ((instance != NULL) &&
        (instance = get_card_instance(player, card), instance->damage_target_player != -1))
    {

      instance = get_card_instance(player, card);
      target_player = (int)instance->damage_target_player;
      land_card_or_iid = instance->damage_target_card;
      instance = get_card_instance(target_player, land_card_or_iid);
      if (((land_card_or_iid == affected_card) &&
           (((target_player == affected_card_controller &&
              ((unsigned char)(instance->kill_code - 2) < 2)) &&
             (death_check_or_owner = check_special_flags2(target_player, land_card_or_iid, 0x10000000),
              death_check_or_owner == 0)))) &&
          (death_check_or_owner = check_for_death_trigger_removal(target_player, land_card_or_iid),
           death_check_or_owner == 0))
      {

        instance = get_card_instance(player, create_legacy_effect(player, card, zendikon_legacy));
        death_check_or_owner = get_owner(target_player, land_card_or_iid);
        instance->targets[0].player = death_check_or_owner;
        land_card_or_iid = get_original_internal_card_id(target_player, land_card_or_iid);
        instance->targets[0xb].player = 1;
        instance->targets[0].card = land_card_or_iid;
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
  default_target_definition(player, card, &td, 1);
  td.preferred_controller = player;
  attached_is_additional_subtype(player, card, event, styp);
  if (cols != COLOR_TEST_0)
  {
    attached_is_additional_color(player, card, event, cols);
  }
  if ((spkws & SP_KEYWORD_NOT_SIMPLE_BITS) != SP_KEYWORD_0)
  {
    backtrace_and_abort("cards/worldwake.c:109\n!(spkws & SP_KEYWORD_NOT_SIMPLE_BITS)");
  }
  return generic_animating_aura(player, card, event, &td, "TARGET_LAND", pow, tgh, kws, spkws);
}

int card_abyssal_persecutor(int player, int card, event_t event){
/* CARD_ID_ABYSSAL_PERSECUTOR	10716
Abyssal Persecutor	|2|B|B
Creature - Demon 6/6
Flying, trample
You can't win the game and your opponents can't lose the game. */

	cannot_lose_the_game(player, card, event, 1-player);
	return 0;
}

int card_admonition_angel(int player, int card, event_t event)
{
  /* Admonition Angel	|3|W|W|W
   * Creature - Angel 6/6
   * Flying
   * Landfall - Whenever a land enters the battlefield under your control, you may exile target nonland permanent other than ~.
   * When ~ leaves the battlefield, return all cards exiled with it to the battlefield under their owners' control. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  result = duh_mode(player);
  landfall_mode(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TYPE_LAND;
    td.special = TARGET_SPECIAL_NOT_ME;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_NONLAND_PERMANENT", 0, 0);
      if (result != 0)
      {
        exile_card_and_remember_it_on_exiledby(player, card, instance->targets[0].player,
                                               instance->targets[0].card);
        instance->number_of_targets = '\0';
      }
    }
    leaves_play(player, card, EVENT_ETB, 1, 0);
    return 0;
  }
  leaves_play(player, card, event, 1, 0);
  if (event != EVENT_LEAVES_PLAY_ABILITY)
  {
    return 0;
  }
  return_cards_exiled_by_me_to_play(player, card);
  return 0;
}

int card_aether_tradwinds(int player, int card, event_t event){
/* CARD_ID_AETHER_TRADEWINDS	10725 => double entry also coded in kaladesh
Aether Tradewinds	|2|U
Instant
Return target permanent you control and target permanent you don't control to their owners' hands. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_PERMANENT);
	td2.allowed_controller = td.preferred_controller = 1-player;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return can_target(&td) && can_target(&td2) ? 1 : 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "Select target permanent you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			new_pick_target(&td2, "Select target permanent you don't control.", 1, 1 | GS_LITERAL_PROMPT);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			action_on_target(player, card, 0, ACT_BOUNCE);
		}
		if( validate_target(player, card, &td2, 1) ){
			action_on_target(player, card, 1, ACT_BOUNCE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_agadeem_occultist(int player, int card, event_t event){
  /* Agadeem Occultist |2|B
   * Creature — Human Shaman Ally 0/2
   * {T}: Put target creature card from an opponent's graveyard onto the battlefield under your control if its converted mana cost is less
   * 	than or equal to the number of Allies you control. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
	if( IS_AI(player) ){
		test.cmc = count_subtype(player, TYPE_PERMANENT, SUBTYPE_ALLY);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_ACTIVATE ){
		int amount = count_subtype(player, TYPE_PERMANENT, SUBTYPE_ALLY);
		scnprintf(test.message, 100, " Select a creature card with cmc equal to %d Allies you control.", amount);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, 1-player, 0);
		if( selected > -1 ){
			const int *grave = get_grave(1-player);
			if( get_cmc_by_internal_id(grave[selected]) <= count_subtype(player, TYPE_PERMANENT, SUBTYPE_ALLY) ){
				reanimate_permanent(instance->parent_controller, instance->parent_card, 1-player, selected, REANIMATE_DEFAULT);
			}
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER_OPP_GRAVE);
}

int card_akoum_battlesinger(int player, int card, event_t event){
/* CARD_ID_AKOUM_BATTLESINGER	10719
Akoum Battlesinger	|1|R
Creature - Human Berserker Ally 1/1
Haste
Whenever ~ or another Ally enters the battlefield under your control, you may have Ally creatures you control get +1/+0 until end of turn. */

	haste(player, card, event);
	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( event == EVENT_ETB_ABILITY ){
		pump_subtype_until_eot(player, card, player, SUBTYPE_ALLY, 1, 0, 0, 0);
	}
	return 0;
}

int card_amulet_of_vigor(int player, int card, event_t event)
{
/* CARD_ID_AMULET_OF_VIGOR	10720
Amulet of Vigor	|1
Artifact
Whenever a permanent enters the battlefield tapped and under your control, untap it. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.state = STATE_TAPPED;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		untap_card(trigger_cause_controller, trigger_cause);
	}

	return 0;
}

int card_anowon_the_ruin_sage(int player, int card, event_t event){
  /* Anowon, the Ruin Sage |3|B|B
   * Legendary Creature — Vampire Shaman 4/3, 3BB (5)
   * At the beginning of your upkeep, each player sacrifices a non-Vampire creature. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a non-Vampire creature to sacrifice.");
		test.subtype = SUBTYPE_VAMPIRE;
		test.subtype_flag = DOESNT_MATCH;

		APNAP(p, { new_sacrifice(player, card, p, SAC_NO_CANCEL | SAC_CAUSED, &test); };);
	}

	return 0;
}

int card_arbor_elf(int player, int card, event_t event){
/* CARD_ID_ARBOR_ELF	10723
Arbor Elf	|G
Creature - Elf Druid 1/1
|T: Untap target |H2Forest. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player,card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST0, 0, &td,
									get_hacked_land_text(player, card, "Select target %s.", SUBTYPE_FOREST));
		if( spell_fizzled != 1 ){
			ai_mod_for_untap_effects(player, card, 0, 0);
		}
		return 0;
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			untap_card(instance->targets[0].player, instance->targets[0].card );
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST0, 0, &td, NULL);
}

int card_archon_of_redemption(int player, int card, event_t event){
/* Archon of Redemption	|3|W|W
 * Creature - Archon 3/4
 * Flying
 * Whenever ~ or another creature with flying enters the battlefield under your control, you may gain life equal to that creature's power. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
	if (!is_humiliated(player, card, AL_TRIG) && trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.keyword = KEYWORD_FLYING;
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), TYPE_NONE, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		get_card_instance(trigger_cause_controller, trigger_cause)->regen_status |= KEYWORD_RECALC_TOUGHNESS|KEYWORD_RECALC_POWER;
		gain_life(player, get_power(trigger_cause_controller, trigger_cause));
	}

	return 0;
}

int card_avenger_of_zendikar(int player, int card, event_t event)
{
  /* Avenger of Zendikar	|5|G|G
   * Creature - Elemental 5/5
   * When ~ enters the battlefield, put a 0/1 |Sgreen Plant creature token onto the battlefield for each land you control.
   * Landfall - Whenever a land enters the battlefield under your control, you may put a +1/+1 counter on each Plant creature you control. */

  int count;
  card_instance_t *instance;
  int plant_card_or_duh;
  test_definition_t test;
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    if ((trigger_cause_controller == player) && (trigger_cause == card))
    {
      instance->info_slot = 1;
      etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
      instance->info_slot = 0;
    }
    else
    {
      default_test_definition(&test, 1);
      plant_card_or_duh = duh_mode(player);
      etb_ability_global(player, event, (plant_card_or_duh != 0) + 1, TYPE_NONE, &test, player,
                         card);
    }
  }
  if (event == EVENT_ETB)
  {
    instance = get_card_instance(player, card);
    if (instance->info_slot == 1)
    {
      count = count_permanents_by_type(player, TYPE_LAND);
      if (0 < count)
      {
        generate_tokens_by_id(player, card, CARD_ID_PLANT, count);
      }
    }
    else
    {
      plant_card_or_duh = active_cards_count[player] + -1;
      if (-1 < plant_card_or_duh)
      {
        do
        {
          instance = in_play(player, plant_card_or_duh);
          if (((instance != NULL) &&
               (has_subtype(player, plant_card_or_duh, SUBTYPE_PLANT) != 0)) &&
              (is_what(player, plant_card_or_duh, 2) != 0))
          {
            add_counters(player, card, player, plant_card_or_duh, COUNTER_P1_P1, 1);
          }
          plant_card_or_duh = plant_card_or_duh + -1;
        } while (plant_card_or_duh != -1);
      }
    }
  }
  return 0;
}

int card_basilisk_collar(int player, int card, event_t event)
{
/* CARD_ID_BASILISK_COLLAR	10727
Basilisk Collar	|1
Artifact - Equipment
Equipped creature has deathtouch and lifelink.
Equip |2 */

  return vanilla_equipment(player, card, event, 2, 0,0, 0,SP_KEYWORD_DEATHTOUCH|SP_KEYWORD_LIFELINK);
}

int card_bazaar_trader(int player, int card, event_t event){
/* CARD_ID_BAZAAR_TRADER	10729
Bazaar Trader	|1|R
Creature - Goblin 1/1
|T: Target player gains control of target artifact, creature, or land you control. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_LAND);
	td2.preferred_controller = td2.allowed_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			return can_target(&td2);
		}
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
		if( spell_fizzled != 1 &&
			new_pick_target(&td2, "Select target artifact, creature or land you control.", 1, 1 | GS_LITERAL_PROMPT) )
		{
			add_state(player, card, STATE_TAPPED);
			tapped_for_mana_color = -1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td2, 1) ){
			gain_control_permanently(instance->targets[0].player, instance->targets[1].player, instance->targets[1].card);
		}
	}

	return 0;
}

int card_bestial_menace(int player, int card, event_t event){
	/* Bestial Menace	|3|G|G
	 * Sorcery
	 * Put a 1/1 |Sgreen Snake creature token, a 2/2 |Sgreen Wolf creature token, and a 3/3 |Sgreen Elephant creature token onto the battlefield. */

	if( event == EVENT_RESOLVE_SPELL){
		generate_token_by_id(player, card, CARD_ID_ELEPHANT);
		generate_token_by_id(player, card, CARD_ID_WOLF);
		generate_token_by_id(player, card, CARD_ID_SNAKE);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_bloodhusk_ritualist(int player, int card, event_t event){
/* CARD_ID_BLOODHUSK_RITUALIST	10731
Bloodhusk Ritualist	|2|B
Creature - Vampire Shaman 2/2
Multikicker |B (You may pay an additional |B any number of times as you cast this spell.)
When ~ enters the battlefield, target opponent discards a card for each time it was kicked. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;
        td.allowed_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( instance->info_slot > 0 && can_target(&td) ){
			if( pick_target(&td, "TARGET_OPPONENT") ){
				new_multidiscard(instance->targets[0].player, instance->info_slot, 0, player);
			}
		}
		instance->number_of_targets = 0;
	}
	return multikicker(player, card, event, MANACOST_B(1));
}

int card_bojuka_bog(int player, int card, event_t event){
	/* Bojuka Bog	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, exile all cards from target player's graveyard.
	 * |T: Add |B to your mana pool. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && !is_humiliated(player, card, AL_TRIG) ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			rfg_whole_graveyard(instance->targets[0].player);
		}
		instance->number_of_targets = 0;
	}

	return plain_mana_producer_tapped(player, card, event);
}

static int butcher_of_malakir_effect(int player, int card, event_t event)
{

  // In case something is copying Pawn of Ulamog and then reverts to its original form.
  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_BUTCHER_OF_MALAKIR)
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
            // Will trigger also if this is not a creature when it goes into the graveyard;
            if (!is_what(p, c, TYPE_CREATURE))
            {
              count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
            }
            instance->damage_target_player = instance->damage_target_card = -1;
            remove_status(player, card, STATUS_INVISIBLE_FX);
            if (instance->targets[11].player < 0)
            {
              instance->targets[11].player = 0;
            }
            SET_BYTE1(instance->targets[11].player) = 1;
          }
        }
        count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    test_definition_t test;
    new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
    test.qty = BYTE0(result);
    new_sacrifice(player, card, 1 - player, SAC_CAUSED | SAC_NO_CANCEL, &test);
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_butcher_of_malakir(int player, int card, event_t event){
/* CARD_ID_BUTCHER_OF_MALAKIR	10736
Butcher of Malakir	|5|B|B
Creature - Vampire Warrior 5/4
Flying
Whenever ~ or another creature you control dies, each opponent sacrifices a creature. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)butcher_of_malakir_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &butcher_of_malakir_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);

			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			test.qty = amount;
			new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &test);
		}
	}

	return 0;
}

int card_calcite_snapper(int player, int card, event_t event)
{
  /* CARD_ID_CALCITE_SNAPPER	10737
Calcite Snapper	|1|U|U
Creature - Turtle 1/4
Shroud (This creature can't be the target of spells or abilities.)
Landfall - Whenever a land enters the battlefield under your control, you may switch ~'s power and toughness until end of turn. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		if( new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) ){
			switch_power_and_toughness_until_eot(player, card, player, card);
		}
	}

	return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25858a0; original cards/worldwake.c:658. */
int card_canopy_cover(int player, int card, event_t event)
{
  /* Canopy Cover	|1|G
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature can't be blocked except by creatures with flying or reach.
   * Enchanted creature can't be the target of spells or abilities your opponents control. */

  card_instance_t *instance;
  unsigned int blocker_abilities;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {
      instance = get_card_instance(player, card);
      if (-1 < instance->damage_target_player)
      {

        instance = get_card_instance(player, card);
        if (event == EVENT_BLOCK_LEGALITY)
        {
          if ((instance->damage_target_player == attacking_card_controller) &&
              (instance->damage_target_card == attacking_card))
          {

            blocker_abilities =
                get_abilities(affected_card_controller, affected_card, EVENT_ABILITIES, -1);
            if ((blocker_abilities & 0x420) == 0)
            {
              event_result = 1;
            }
          }
          attached_gets_sp_kw2(player, card, EVENT_BLOCK_LEGALITY, SP_KEYWORD2_GRANTED_PROT_MISC);
          get_card_instance(player, card);
          return aura(player, card, event, NULL, NULL);
        }
      }
    }
  }
  attached_gets_sp_kw2(player, card, event, SP_KEYWORD2_GRANTED_PROT_MISC);
  instance = get_card_instance(player, card);
  if ((((event == EVENT_PROT_MISC) && (instance->damage_target_card == affected_card)) &&
       (instance->damage_target_player == affected_card_controller)) &&
      ((1 - player == attacking_card_controller &&
        (((unsigned int)event_result >> 24 & 0xff) == '\x05'))))
  {
    event_result = event_result | 1;
  }
  return aura(player, card, event, NULL, NULL);
}
#endif

int card_celestial_colonnade(int player, int card, event_t event){
/* CARD_ID_CELESTIAL_COLONNADE	10740
Celestial Colonnade	""
Land
~ enters the battlefield tapped.
|T: Add |W or |U to your mana pool.
|3|W|U: Until end of turn, ~ becomes a 4/4 |Swhite and |Sblue Elemental creature with flying and vigilance. It's still a land. */

	if (!is_humiliated(player, card, AL_TRIG))
    comes_into_play_tapped(player, card, event);
	return manland_normal(player, card, event, MANACOST_XUW(3, 1, 1));
}

int card_celestial_colonnade_animated(int player, int card, event_t event){
	vigilance(player, card, event);
	return manland_animated(player, card, event, MANACOST_XUW(3, 1, 1));
}

int card_chain_reaction(int player, int card, event_t event){
/* CARD_ID_CHAIN_REACTION	10742
Chain Reaction	|2|R|R
Sorcery
~ deals X damage to each creature, where X is the number of creatures on the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		int num = count_subtype(ANYBODY, TYPE_CREATURE, -1);
		new_damage_all(player, card, ANYBODY, num, NDA_ALL_CREATURES, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_comet_storm(int player, int card, event_t event){
	/* Comet Storm	|X|R|R	0x20084f0
	 * Instant
	 * Multikicker |1
	 * Choose any target, then choose another target for each time this spell was kicked. ~ deals X damage to each of them. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( spell_fizzled != 1 ){
			//A very particular case, so we couldn't use the "set_x_for_x_spell" function
			instance->info_slot = x_value;
			int value_before_x = 0;
			if( (uint8_t)cards_data[instance->internal_card_id].cc[1] <= 15 ){
				value_before_x += (uint8_t)cards_data[instance->internal_card_id].cc[1];
			}
			int diff = value_before_x - true_get_updated_casting_cost(player, card, -1, event, -1);

			int max_available_targets = 0;
			int i;
			for(i=0; i<18; i++){
				if( has_mana(player, COLOR_COLORLESS, i+1) ){
					max_available_targets++;
				}
				else{
					break;
				}
			}
			if( diff < 0 ){
				max_available_targets -= diff;
			}
			max_available_targets++;
			int trgs = 0;
			if( IS_AI(player) ){
				if( would_validate_arbitrary_target(&td, 1-player, -1) ){
					instance->targets[0].player = 1-player;
					instance->targets[0].card = -1;
					instance->number_of_targets = 1;
					trgs++;
				}
				while( can_target(&td1) && trgs < max_available_targets ){
						if( select_target(player, card, &td1, "Select target creature or planeswalker.", &(instance->targets[trgs])) ){
							state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
							trgs++;
						}
						else{
							break;
						}
				}
			}
			else{
				int targeted_players[2] = {0, 0};
				while( trgs < max_available_targets ){
						if( ! can_target(&td1) ){
							if( targeted_players[0] ){
								if( ! would_validate_arbitrary_target(&td, 1, -1) ){
									break;
								}
							}
							if( targeted_players[1] ){
								if( ! would_validate_arbitrary_target(&td, 0, -1) ){
									break;
								}
							}
						}

						if( select_target(player, card, &td, "Select target creature, player, or planeswalker.", &(instance->targets[trgs])) ){
							if( instance->targets[trgs].card != -1 ){
								state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
								trgs++;
							}
							else{
								if( targeted_players[instance->targets[trgs].player] != 1 ){
									targeted_players[instance->targets[trgs].player] = 1;
									trgs++;
								}
								else{
									break;
								}
							}
						}
						else{
							break;
						}
				}
			}
			for(i=0; i<trgs; i++){
				if( instance->targets[i].card != -1 ){
					state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
				}
			}
			instance->number_of_targets = trgs;
			if( diff < 0 ){
				int orig_trgs = trgs;
				trgs += diff;
				diff += orig_trgs;
				if( trgs < 0 ){
					trgs = 0;
				}
			}
			charge_mana(player, COLOR_COLORLESS, trgs-1);
			if( spell_fizzled != 1 ){
				if( trgs > 1 ){
					set_special_flags(player, card, SF_KICKED);
				}
				if( diff < 0 ){
					instance->info_slot -= diff;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				damage_creature(instance->targets[i].player, instance->targets[i].card, instance->info_slot, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

/* ManalinkEh 2022: 0x2586250; original cards/worldwake.c:858. */
int card_corrupted_zendikon(int player, int card, event_t event)
{
  /* Corrupted Zendikon	|1|B
   * Enchantment - Aura
   * Enchant land
   * Enchanted land is a 3/3 black Ooze creature. It's still a land.
   * When enchanted land dies, return that card to its owner's hand. */

  int result;

  result =
      zendikon(player, card, event, 3, 3, COLOR_TEST_BLACK, SUBTYPE_OOZE, KEYWORD_0, SP_KEYWORD_0);
  return result;
}

int card_creeping_tar_pit(int player, int card, event_t event){
/* CARD_ID_CREEPING_TAR_PIT	10747
Creeping Tar Pit	""
Land
~ enters the battlefield tapped.
|T: Add |U or |B to your mana pool.
|1|U|B: ~ becomes a 3/2 |Sblue and |Sblack Elemental creature until end of turn and can't be blocked this turn. It's still a land. */

	if (!is_humiliated(player, card, AL_TRIG))
    comes_into_play_tapped(player, card, event);
	return manland_normal(player, card, event, MANACOST_XUB(1, 1, 1));
}

int card_creeping_tar_pit_animated(int player, int card, event_t event){
	unblockable(player, card, event);
	return manland_animated(player, card, event, MANACOST_XUB(1, 1, 1));
}

/* ManalinkEh 2022: 0x25863b0; original cards/worldwake.c:887. */
int card_crusher_zendikon(int player, int card, event_t event)
{
  /* Crusher Zendikon	|2|R
   * Enchantment - Aura
   * Enchant land
   * Enchanted land is a 4/2 red Beast creature with trample. It's still a land.
   * When enchanted land dies, return that card to its owner's hand. */

  int result;

  result = zendikon(player, card, event, 4, 2, COLOR_TEST_RED, SUBTYPE_BEAST, KEYWORD_TRAMPLE,
                    SP_KEYWORD_0);
  return result;
}

int card_cunning_sparkmage(int player, int card, event_t event){
	/* Cunning Sparkmage	|2|R	0x200cbe5
	 * Creature - Human Shaman 0/1
	 * Haste
	 * |T: ~ deals 1 damage to any target. */
	/* Vulshok Sorcerer	|1|R|R	0x200cbe5
	 * Creature - Human Shaman 1/1
	 * Haste
	 * |T: ~ deals 1 damage to any target. */
	haste(player, card, event);
	return card_prodigal_sorcerer(player, card, event);
}

int card_deaths_shadow(int player, int card, event_t event){
/* CARD_ID_DEATHS_SHADOW	10752
Death's Shadow	|B
Creature - Avatar 13/13
~ gets -X/-X, where X is your life total. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && player > -1 ){
		if( affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) && ! check_battlefield_for_id(player, CARD_ID_LICH) ){
			event_result -= life[player];
		}
	}
	return 0;
}

int card_dragonmaster_outcast( int player, int card, event_t event){
	/* Dragonmaster Outcast	|R
	 * Creature - Human Shaman 1/1
	 * At the beginning of your upkeep, if you control six or more lands, put a 5/5 |Sred Dragon creature token with flying onto the battlefield. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY && count_permanents_by_type(player, TYPE_LAND) >= 6){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_DRAGON, &token);
		token.color_forced = COLOR_TEST_RED;
		token.pow = token.tou = 5;
		generate_token(&token);
	}

	return 0;
}

int card_dread_statuary(int player, int card, event_t event){
/* CARD_ID_DREAD_STATUARY	10756
Dread Statuary	""
Land
|T: Add |C to your mana pool.
|4: ~ becomes a 4/2 Golem artifact creature until end of turn. It's still a land. */

	return manland_normal(player, card, event, MANACOST_X(4));
}

int card_dread_statuary_animated(int player, int card, event_t event){
	return manland_animated(player, card, event, MANACOST_X(4));
}

int card_everflowing_chalice(int player, int card, event_t event)
{
  /* Everflowing Chalice	|0
   * Artifact
   * Multikicker |2
   * ~ enters the battlefield with a charge counter on it for each time it was kicked.
   * |T: Add |1 to your mana pool for each charge counter on ~. */
	card_instance_t* instance = get_card_instance(player, card);

	// do not let the AI cast this for 0
	if (IS_AI(player) && event == EVENT_MODIFY_COST && !has_mana(AI, COLOR_ANY, 2))
		COST_COLORLESS += 2;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) && ! check_special_flags(player, card, SF_NOT_CAST) ){
			instance->info_slot = 0;
			int diff = 0;
			if( ! played_for_free(player, card) ){
				diff = MIN(0, cards_ptr[get_id(player, card)]->req.req_colorless+true_get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, -1));
			}
			int cless = 2+diff;
			while( 1 ){
					int ctp = MAX(0, cless);
					if( cless > 0 ){
						ctp = MIN(cless, 2);
					}
					cless+=2;
					if( has_mana(player, COLOR_ARTIFACT, ctp) ){
						int choice = do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass", 0);
						if( choice == 0 ){
							charge_mana(player, COLOR_ARTIFACT, ctp);
							if( spell_fizzled != 1 ){
								set_special_flags(player, card, SF_KICKED);
								instance->info_slot++;
							}
						}
						else{
							break;
						}
					}
					else{
						break;
					}
			}
			if( spell_fizzled != 1 ){
				enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, instance->info_slot);
			}
		}
	}
	if( event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE && affect_me(player, card)){
		enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, instance->info_slot);
	}

  if (event == EVENT_COUNT_MANA && affect_me(player, card) && CAN_TAP_FOR_MANA(player, card))
	declare_mana_available(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_CHARGE));

  if (event == EVENT_CAN_ACTIVATE)
	return CAN_TAP_FOR_MANA(player, card) && (!IS_AI(player) || count_counters(player, card, COUNTER_CHARGE) > 0);

  if (event == EVENT_ACTIVATE)
	produce_mana_tapped(player, card, COLOR_COLORLESS, count_counters(player, card, COUNTER_CHARGE));

  if (event == EVENT_SHOULD_AI_PLAY)
	ai_modifier += (player == AI ? 1 : -1) * count_counters(player, card, COUNTER_CHARGE);

  return 0;
}

int card_explore2(int player, int card, event_t event){
	/* Explore	|1|G
	 * Sorcery
	 * You may play an additional land this turn.
	 * Draw a card. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.zone = TARGET_ZONE_HAND;
		ai_modifier += (check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test) * 12);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 1);
		int effect = create_legacy_effect(player, card, &check_playable_lands_legacy);
		card_instance_t *leg = get_card_instance(player, effect);
		leg->targets[2].card = get_id(player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_eye_of_ugin(int player, int card, event_t event){
/* CARD_ID_EYE_OF_UGIN	10761
Eye of Ugin	""
Legendary Land
Colorless Eldrazi spells you cast cost |2 less to cast.
|7, |T: Search your library for a colorless creature card, reveal it, and put it into your hand. Then shuffle your library. */

	check_legend_rule(player, card, event);

	if(event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card, AL_STATIC) ){
		int p = affected_card_controller;
		int c = affected_card;
		if(has_subtype(p, c, SUBTYPE_ELDRAZI) && is_colorless(p, c) ){
			COST_COLORLESS-=2;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (current_turn == 1-player && current_phase >= PHASE_MAIN2 ? 48 : -48);
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a colorless creature card.");
		this_test.color = COLOR_TEST_ANY_COLORED;
		this_test.color_flag = DOESNT_MATCH;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(7), 0, NULL, NULL);
}

/* October 2022 native Worldwake handler, 0x2586db0. */
int card_guardian_zendikon(int player, int card, event_t event)
{
  /* Guardian Zendikon	|2|W
   * Enchantment - Aura
   * Enchant land
   * Enchanted land is a 2/6 white Wall creature with defender. It's still a land.
   * When enchanted land dies, return that card to its owner's hand. */

  int result;
  result = zendikon(player, card, event, 2, 6, COLOR_TEST_WHITE, SUBTYPE_WALL, KEYWORD_DEFENDER,
                    SP_KEYWORD_0);
  return result;
}

int card_hada_freeblade(int player, int card, event_t event){
/* CARD_ID_HADA_FREEBLADE	10772
Hada Freeblade	|W
Creature - Human Soldier Ally 0/1
Whenever ~ or another Ally enters the battlefield under your control, you may put a +1/+1 counter on ~. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		add_counters(player, card, player, card, COUNTER_P1_P1, 1);
	}

	return 0;
}

int card_halimar_depths(int player, int card, event_t event)
{
  /* Halimar Depths	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, look at the top three cards of your library, then put them back in any order.
	 * |T: Add |U to your mana pool. */

  comes_into_play_tapped(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
    rearrange_top_x(player, player, 3);
  return mana_producer_fixed(player, card, event, COLOR_BLUE);
}

int card_halimar_excavator(int player, int card, event_t event){
/* CARD_ID_HALIMAR_EXCAVATOR	10774
Halimar Excavator	|1|U
Creature - Human Wizard Ally 1/3
Whenever ~ or another Ally enters the battlefield under your control, target player puts the top X cards of his or her library into his or her graveyard,
where X is the number of Allies you control. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;
		card_instance_t *instance = get_card_instance( player, card );
		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			mill(instance->targets[0].player, count_subtype(player, TYPE_PERMANENT, SUBTYPE_ALLY));
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_hammer_of_ruin(int player, int card, event_t event)
{
  /* Hammer of Ruin	|2
   * Artifact - Equipment
   * Equipped creature gets +2/+0.
   * Whenever equipped creature deals combat damage to a player, you may destroy target Equipment that player controls.
   * Equip |2 */

  if (equipped_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_TRACE_DAMAGED_PLAYERS|DDBM_TRIGGER_OPTIONAL))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
	  instance->targets[1].player = 0;

	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_ARTIFACT);
	  td.required_subtype = SUBTYPE_EQUIPMENT;

	  int p;
	  for (p = 0; p <= 1; ++p)
		{
		  td.allowed_controller = td.preferred_controller = p;
		  for (; times_damaged[p] > 0; --times_damaged[p])
			{
			  instance->number_of_targets = 0;
			  if (pick_target(&td, "TARGET_EQUIPMENT"))
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
		}
	}

  return vanilla_equipment(player, card, event, 2, 2, 0, 0, 0);
}

int card_harabaz_druid(int player, int card, event_t event){
/* CARD_ID_HARABAZ_DRUID	10776
Harabaz Druid	|1|G
Creature - Human Druid Ally 0/1
|T: Add X mana of any one color to your mana pool, where X is the number of Allies you control. */

	int count = 0;
	if (event == EVENT_COUNT_MANA || event == EVENT_ACTIVATE || (event == EVENT_CAN_ACTIVATE && player == AI)){	// Only count if it'll actually be needed
		count = count_subtype(player, TYPE_PERMANENT, SUBTYPE_ALLY);
	}
	return mana_producing_creature_all_one_color(player, card, event, 24, COLOR_TEST_ANY_COLORED, count);
}

int card_jace_the_mind_sculptor(int player, int card, event_t event)
{
  /* Jace, the Mind Sculptor	|2|U|U
   * Planeswalker - Jace (3)
   * +2: Look at the top card of target player's library. You may put that card on the bottom of
   * that player's library. 0: Draw three cards, then put two cards from your hand on top of your
   * library in any order. -1: Return target creature to its owner's hand. -12: Exile all cards from
   * target player's library, then that player shuffles his or her hand into his or her library. */

  check_legend_rule(player, card, event);



  if (IS_ACTIVATING(event))
  {

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);

    target_definition_t td1;
    default_target_definition(player, card, &td1, TYPE_CREATURE);
    td1.zone = TARGET_ZONE_PLAYERS;

    card_instance_t *instance = get_card_instance(player, card);

    int priority_bstorm = 0;
    int priority_bounce = 0;

    if (event == EVENT_ACTIVATE)
    {
      priority_bstorm = (hand_count[1 - player] - hand_count[player]) * 5;
      td.allowed_controller = 1 - player;
      priority_bounce = life[player] < 10 && can_target(&td) ? 15 : 5;
      td.allowed_controller = ANYBODY;
    }

    enum
    {
      CHOICE_SCRY1 = 1,
      CHOICE_BRAINSTORM,
      CHOICE_BOUNCE,
      CHOICE_ULTIMATUM
    } choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, "Scry 1", can_target(&td1),
                      10, 2, "Brainstorm", 1, priority_bstorm, 0, "Bounce creature",
                      can_target(&td), priority_bounce, -1, "Ultimatum", can_target(&td1), 30, -12);

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
      case CHOICE_SCRY1:
      case CHOICE_ULTIMATUM:
        pick_target(&td1, "TARGET_PLAYER");
        break;

      case CHOICE_BOUNCE:
        pick_target(&td, "TARGET_CREATURE");
        break;

      default:
        break;
      }
    }
    else // event == EVENT_RESOLVE_ACTIVATION
      switch (choice)
      {
      case CHOICE_SCRY1:
        if (valid_target(&td1))
        {
          show_deck(player, deck_ptr[instance->targets[0].player], 1, "Jace Reveals...", 0,
                    gs_done_008b40e0);
          int ai_choice = 1;
          if (is_what(-1, deck_ptr[instance->targets[0].player][0], TYPE_LAND) &&
              count_subtype(1 - player, TYPE_LAND, -1) > 4)
          {
            ai_choice = 0;
          }
          if (do_dialog(player, player, card, -1, -1, " Top\n Bottom", ai_choice) == 1)
          {
            put_top_card_of_deck_to_bottom(instance->targets[0].player);
          }
        }
        break;

      case CHOICE_BRAINSTORM:
      {
        draw_cards(player, 3);
        int max = MIN(2, hand_count[player]);
        char msg[100] = "Select a card to put on top of deck.";
        test_definition_t this_test;
        new_default_test_definition(&this_test, TYPE_ANY, msg);
        while (max > 0)
        {
          int selected =
              new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &this_test);
          put_on_top_of_deck(player, selected);
          max--;
        }
      }
      break;

      case CHOICE_BOUNCE:
        if (valid_target(&td))
        {
          bounce_permanent(instance->targets[0].player, instance->targets[0].card);
        }
        break;

      case CHOICE_ULTIMATUM:
      {
        if (valid_target(&td1))
        {
          int p = instance->targets[0].player;
          rfg_whole_library(p);
          int i = active_cards_count[p] - 1;
          while (i > -1)
          {
            if (in_hand(p, i))
            {
              put_on_top_of_deck(p, i);
            }
            i--;
          }
          shuffle(p);
        }
      }
      break;
      }
  }

  return planeswalker(player, card, event, 3);
}

int card_join_the_ranks(int player, int card, event_t event){
	/* Join the Ranks	|3|W
	 * Instant
	 * Put two 1/1 |Swhite Soldier Ally creature tokens onto the battlefield. */

	if(event == EVENT_RESOLVE_SPELL){
		generate_tokens_by_id(player, card, CARD_ID_SOLDIER_ALLY, 2);
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_joraga_warcaller(int player, int card, event_t event)
{
  /* Joraga Warcaller	|G
   * Creature - Elf Warrior 1/1
   * Multikicker |1|G
   * ~ enters the battlefield with a +1/+1 counter on it for each time it was kicked.
   * Other Elf creatures you control get +1/+1 for each +1/+1 counter on ~. */

	multikicker(player, card, event, MANACOST_XG(1, 1));

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, multikicked(player, card));

  int toughness = count_counters(player, card, COUNTER_P1_P1);
  int power = count_counters(player, card, COUNTER_P1_P1);
  boost_creature_type(player, card, event, SUBTYPE_ELF, power, toughness, 0, BCT_CONTROLLER_ONLY);
  return 0;
}

int card_jwari_shapeshifter(int player, int card, event_t event)
{
  /* Jwari Shapeshifter	|1|U
   * Creature - Shapeshifter Ally 0/0
   * You may have ~ enter the battlefield as a copy of any Ally creature on the battlefield. */

  target_definition_t td;
  if (event == EVENT_RESOLVE_SPELL)
	{
	  base_target_definition(player, card, &td, TYPE_CREATURE);
	  td.required_subtype = SUBTYPE_ALLY;
	}

  enters_the_battlefield_as_copy_of_any(player, card, event, &td, "SELECT_AN_ALLY_CREATURE");

  return 0;
}

static int kalastria_highborn_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_KALASTRIA_HIGHBORN)
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
            if (!is_what(p, c, TYPE_CREATURE))
              count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
            instance->damage_target_player = instance->damage_target_card = -1;
            remove_status(player, card, STATUS_INVISIBLE_FX);
            if (instance->targets[11].player < 0)
            {
              instance->targets[11].player = 0;
            }
            SET_BYTE1(instance->targets[11].player) = 1;
            if (is_what(p, c, TYPE_CREATURE))
              count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
            return 0;
          }
        }
        test_definition_t test;
        default_test_definition(&test, TYPE_PERMANENT);
        test.subtype = SUBTYPE_VAMPIRE;
        count_for_gfp_ability(player, card, event, player, 0, &test);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);
    while (amount && has_mana(player, COLOR_BLACK, 1))
    {
      target_definition_t td;
      default_target_definition(player, card, &td, TYPE_NONE);
      td.zone = TARGET_ZONE_PLAYERS;
      card_instance_t *instance = get_card_instance(player, card);
      if (charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_BLACK,
                                      1) &&
          new_pick_target(&td, "TARGET_PLAYER", 1, 0))
      {
        lose_life(instance->targets[1].player, 2);
        gain_life(player, 2);
        --amount;
        instance->number_of_targets = 1;
      }
      else
        break;
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_kalastria_highborn(int player, int card, event_t event){
  /* Kalastria Highborn |B|B
   * Creature - Vampire Shaman 2/2
   * Whenever Kalastria Highborn or another Vampire you control dies, you may pay {B}. If you do, target player loses 2 life and you gain 2 life. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)kalastria_highborn_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &kalastria_highborn_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( affect_me(player, card) ){
			count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
		}
		else{
			test_definition_t test;
			default_test_definition(&test, TYPE_PERMANENT);
			test.subtype = SUBTYPE_VAMPIRE;
			count_for_gfp_ability(player, card, event, player, 0, &test);
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			int amount = BYTE0(result);
			while( amount && has_mana(player, COLOR_BLACK, 1) ){
					target_definition_t td;
					default_target_definition(player, card, &td, 0);
					td.zone = TARGET_ZONE_PLAYERS;
					card_instance_t *instance = get_card_instance(player, card);
					if( charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_BLACK, 1) &&
						new_pick_target(&td, "TARGET_PLAYER", 0, 0) )
					{
						lose_life(instance->targets[0].player, 2);
						gain_life(player, 2);
						amount--;
						instance->number_of_targets = 0;
					}
					else{
						break;
					}
			}
		}
	}

	return 0;
}

int card_kazuul_tyrant_of_the_cliffs(int player, int card, event_t event)
{
	/* Kazuul, Tyrant of the Cliffs	|3|R|R
	 * Legendary Creature - Ogre Warrior 5/4
	 * Whenever a creature an opponent controls attacks, if you're the defending player, put a 3/3 |Sred Ogre creature token onto the battlefield unless that creature's controller pays |3. */

	check_legend_rule(player, card, event);
	store_attackers(player, card, event, 0, 1-player, -1, NULL);
	dat_ability(player, card, event, DAT_SEPARATE_TRIGGERS | DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY && (!has_mana(1-player, COLOR_ANY, 3) || !charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, 1-player, COLOR_COLORLESS, 3))){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_OGRE, &token);
		token.pow = token.tou = 3;
		token.color_forced = COLOR_TEST_RED;
		generate_token(&token);
	}
	return 0;
}

int card_khalni_garden(int player, int card, event_t event)
{
  /* Khalni Garden	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, put a 0/1 |Sgreen Plant creature token onto the battlefield.
	 * |T: Add |G to your mana pool. */

  comes_into_play_tapped(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
    generate_token_by_id(player, card, CARD_ID_PLANT);
  return plain_mana_producer(player, card, event);
}

int card_kitesail(int player, int card, event_t event){
/* CARD_ID_KITESAIL	10788
Kitesail	|2
Artifact - Equipment
Equipped creature gets +1/+0 and has flying.
Equip |2 (|2: Attach to target creature you control. Equip only as a sorcery.) */

	return vanilla_equipment(player, card, event, 2, 1, 0, KEYWORD_FLYING, 0);
}

int card_kitesail_apprentice(int player, int card, event_t event){
/* CARD_ID_KITESAIL_APPRENTICE	10789
Kitesail Apprentice	|W
Creature - Kor Soldier 1/1
As long as ~ is equipped, it gets +1/+1 and has flying. */

	if( is_humiliated(player, card, AL_STATIC) ){
		return 0;
	}
	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
		if( equipments_attached_to_me(player, card, EATM_CHECK) ){
			event_result++;
		}
	}
	if( event == EVENT_ABILITIES && affect_me(player, card) ){
		if( equipments_attached_to_me(player, card, EATM_CHECK) ){
			event_result |= KEYWORD_FLYING;
		}
	}
	return 0;
}

int card_kor_firewalker(int player, int card, event_t event){
	/* Kor Firewalker	|W|W
	 * Creature - Kor Soldier 2/2
	 * Protection from |Sred
	 * Whenever a player casts a |Sred spell, you may gain 1 life. */

	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_AI(player), TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){
		gain_life(player, 1);
	}
	return 0;
}

int card_lavaclaw_reaches(int player, int card, event_t event){
/* CARD_ID_LAVACLAW_REACHES	10791
Lavaclaw Reaches	""
Land
~ enters the battlefield tapped.
|T: Add |B or |R to your mana pool.
|1|B|R: Until end of turn, ~ becomes a 2/2 |Sblack and |Sred Elemental creature with "|X: This creature gets +X/+0 until end of turn." It's still a land. */

	if (!is_humiliated(player, card, AL_TRIG))
    comes_into_play_tapped(player, card, event);
	return manland_normal(player, card, event, MANACOST_XBR(1, 1, 1));
}

int card_lavaclaw_reaches_animated(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( ! is_tapped(player, card) && ! is_animated_and_sick(player, card) && can_produce_mana(player, card) && instance->targets[1].player != 66 ){
			return 1;
		}
		if( ! paying_mana() && has_mana_for_activated_ability(player, card, MANACOST_X(2)) && can_use_activated_abilities(player, card)){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		instance->targets[2].player = 0;
		if( ! is_tapped(player, card) && can_produce_mana(player, card) && instance->targets[1].player != 66 ){
			if( ! paying_mana() && has_mana_for_activated_ability(player, card, MANACOST_X(2)) && can_use_activated_abilities(player, card)){
				choice = do_dialog(player, player, card, -1, -1, " Get mana\n +X/+0\n Cancel", 1);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		else if( choice == 1 ){
				instance->targets[1].player = 66;
				charge_mana_for_activated_ability(player, card, MANACOST_X(-1));
				instance->targets[1].player = 0;
				if( spell_fizzled != 1 ){
					instance->targets[2].card = x_value;
					instance->targets[2].player = 1;
				}
		}
		else{
			spell_fizzled = 1;
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION && instance->targets[2].player == 1 ){
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, instance->targets[2].card, 0);
	}

	if( eot_trigger(player, card, event ) ){
		remove_status(player, card, STATUS_ANIMATED);
		remove_special_flags(player, card, SF_TYPE_ALREADY_CHANGED);
		true_transform(player, card);
	}

	if( event == EVENT_SET_COLOR && affect_me(player, card) ){
		event_result = cards_data[ instance->internal_card_id ].color ;
	}

	return 0;
}

int card_leatherback_baloth(int player, int card, event_t event){
/* CARD_ID_LEATHERBACK_BALOTH	10793
Leatherback Baloth	|G|G|G
Creature - Beast 4/5 */

	if( event == EVENT_RESOLVE_SPELL && ! is_unlocked(player, card, event, 37) ){
		kill_card(player, card, KILL_SACRIFICE);
	}
	return 0;
}

int card_lightkeeper_of_emeria(int player, int card, event_t event)
{
/* CARD_ID_LIGHTKEEPER_OF_EMERIA	10794
Lightkeeper of Emeria	|3|W
Creature - Angel 2/4
Multikicker |W (You may pay an additional |W any number of times as you cast this spell.)
Flying
When ~ enters the battlefield, you gain 2 life for each time it was kicked. */

  multikicker(player, card, event, MANACOST_W(1));

  // When ~ enters the battlefield, you gain 2 life for each time it was kicked.
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
	{
	  int amt;
	  for (amt = multikicked(player, card); amt > 0; --amt)
		gain_life(player, 2);
	}

  return 0;
}

int card_lodestone_golem(int player, int card, event_t event){
/* CARD_ID_LODESTONE_GOLEM	10796
Lodestone Golem	|4
Artifact Creature - Golem 5/3
Nonartifact spells cost |1 more to cast. */

	if(event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card, AL_STATIC) ){
		card_data_t* card_d = get_card_data(affected_card_controller, affected_card);
		if(! (card_d->type & TYPE_LAND) && ! (card_d->type & TYPE_ARTIFACT)){
			COST_COLORLESS++;
		}
	}
	return 0;
}

int card_marshals_anthem(int player, int card, event_t event){
/* CARD_ID_MARSHALS_ANTHEM	10798
Marshal's Anthem	|2|W|W
Enchantment
Multikicker |1|W (You may pay an additional |1|W any number of times as you cast this spell.)
Creatures you control get +1/+1.
When ~ enters the battlefield, return up to X target creature cards from your graveyard to the battlefield, where X is the number of times ~ was kicked. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		instance->info_slot = 0;
		int amount = count_graveyard_by_type(player, TYPE_CREATURE);
		while( has_mana_multi(player, MANACOST_XW(1, 1)) ){
				int ai_choice = ! graveyard_has_shroud(player) && instance->info_slot < amount ? 1 : 0;
				int choice = do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass", ai_choice);
				if( choice == 0 ){
					charge_mana_multi(player, MANACOST_XW(1, 1));
					if( spell_fizzled != 1 ){
						instance->info_slot++;
					}
					else{
						break;
					}
				}
				else{
					break;
				}
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance( player, card );
		if( instance->info_slot ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
			this_test.qty = instance->info_slot;
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}
	boost_creature_type(player, card, event, -1, 1, 1, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
	return global_enchantment(player, card, event);
}

int card_mordant_dragon(int player, int card, event_t event){
  /* Mordant Dragon |3|R|R|R
   * Creature — Dragon 5/5
   * Flying
   * {1}{R}: Mordant Dragon gets +1/+0 until end of turn.
   * Whenever Mordant Dragon deals combat damage to a player, you may have it deal that much damage to target creature that player controls. */

	if (event == EVENT_POW_BOOST){ return generic_shade(player, card, event, 0, MANACOST_XR(1, 1), 1, 0, 0, 0); }

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_REPORT_DAMAGE_DEALT);
	if( result ){
		card_instance_t *instance = get_card_instance(player, card);
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		int dmg[2] = {BYTE0(instance->targets[16].player), BYTE1(instance->targets[16].player)};
		instance->targets[16].player = 0;
		int i;
		for(i=0; i<2; i++){
			if( dmg[i] && times_damaged[i] ){
				dmg[i] /= times_damaged[i];
			}
		}
		APNAP(p,{
					while( times_damaged[p] ){
							target_definition_t td;
							default_target_definition(player, card, &td, TYPE_CREATURE);
							td.allowed_controller = td.preferred_controller = p;
							char msg[100];
							if( p == player ){
								scnprintf(msg, 100, "Select target creature you control.");
							}
							else{
								scnprintf(msg, 100, "Select target creature your opponent controls.");
							}
							if( new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
								damage_target0(player, card, dmg[p]);
							}
							else{
								break;
							}
							times_damaged[p]--;
					}
					instance->number_of_targets = 0;
				}
		);
	}

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_XR(1, 1), 1, 0);
}

int card_natures_claim(int player, int card, event_t event){
/* CARD_ID_NATURES_CLAIM	10802
Nature's Claim	|G
Instant
Destroy target artifact or enchantment. Its controller gains 4 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT | TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			gain_life(instance->targets[0].player, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
}

int card_nemesis_trap(int player, int card, event_t event){
/* CARD_ID_NEMESIS_TRAP	10803
Nemesis Trap	|4|B|B
Instant - Trap
If a |Swhite creature is attacking, you may pay |B|B rather than pay ~'s mana cost.
Exile target attacking creature. Create a token that's a copy of that creature. Exile it at the beginning of the next end step. */

	if( ! IS_GS_EVENT(player, card, event) && event != EVENT_MODIFY_COST ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_MODIFY_COST ){
		td.required_color = COLOR_TEST_WHITE;	// do not sleight
		td.illegal_abilities = 0;	// doesn't actually have to be targetable
		if (can_target(&td)){
			COST_COLORLESS-=4;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			token_generation_t token;
			copy_token_definition(player, card, &token, instance->targets[0].player, instance->targets[0].card);
			token.legacy = 1;
			token.special_code_for_legacy = &remove_at_eot;
			generate_token(&token);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_novablast_wurm( int player, int card, event_t event)
{
/* CARD_ID_NOVABLAST_WURM	10804
Novablast Wurm	|3|G|G|W|W
Creature - Wurm 7/7
Whenever ~ attacks, destroy all other creatures. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.not_me = 1;
	  new_manipulate_all(player, card, ANYBODY, &test, KILL_DESTROY);
	}
  return 0;
}

int card_omnath_locus_of_mana(int player, int card, event_t event)
{
/* CARD_ID_OMNATH_LOCUS_OF_MANA	10805
Omnath, Locus of Mana	|2|G
Legendary Creature - Elemental 1/1
|SGreen mana doesn't empty from your mana pool as steps and phases end.
~ gets +1/+1 for each |Sgreen mana in your mana pool. */

  check_legend_rule(player, card, event);

  if( is_humiliated(player, card, AL_STATIC) )
	  return 0;

  // Green mana doesn't empty from your mana pool as steps and phases end.
  if (event == EVENT_MANA_POOL_DRAINING)
	mana_doesnt_drain_from_pool[player][get_sleighted_color(player, card, COLOR_GREEN)] |= MANADRAIN_DOESNT_DRAIN;
  // ~ gets +1/+1 for each green mana in your mana pool.
  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && !is_humiliated(player, card, AL_STATIC))
	event_result += mana_pool[player][get_sleighted_color(player, card, COLOR_GREEN)] + get_card_instance(player, card)->info_slot;
  // AI
  if (event == EVENT_CAN_ACTIVATE && (player == AI || ai_is_speculating == 1) && !is_humiliated(player, card, AL_STATIC))
	{
	  int clr = get_sleighted_color(player, card, COLOR_GREEN);
	  int more_avail = has_mana(player, clr, 1) - mana_pool[player][clr];
	  return more_avail > 0;
	}
  if (event == EVENT_ACTIVATE)
	{
	  int clr = get_sleighted_color(player, card, COLOR_GREEN);
	  card_instance_t* instance = get_card_instance(player, card);
	  int any = 0;
	  while (1)
		{
		  int prev_in_pool = mana_pool[player][clr];
		  instance->info_slot = prev_in_pool;	// Prevent p/t from changing while activating
		  mana_pool[player][clr] = 0;
		  mana_pool[player][COLOR_ANY] -= prev_in_pool;
		  charge_mana(player, clr, 1);
		  instance->info_slot = 0;
		  mana_pool[player][clr] += prev_in_pool;
		  mana_pool[player][COLOR_ANY] += prev_in_pool;
		  if (cancel == 1)
			break;
		  else
			{
			  any = 1;
			  mana_pool[player][clr] += 1;
			  mana_pool[player][COLOR_ANY] += 1;
			}
		}
	  if (any)
		cancel = 0;
	  cant_be_responded_to = 1;
	}
  if (event == EVENT_SHOULD_AI_PLAY)
	{
	  int clr = get_sleighted_color(player, card, COLOR_GREEN);
	  ai_modifier += 24 * mana_pool[player][clr];
	}
  return 0;
}

int card_perimeter_captain(int player, int card, event_t event){
/* CARD_ID_PERIMETER_CAPTAIN	10806
Perimeter Captain	|W
Creature - Human Soldier 0/4
Defender
Whenever a creature you control with defender blocks, you may gain 2 life. */

	if( ! is_humiliated(player, card, AL_TRIG) ){
		int c;
		if (event == EVENT_DECLARE_BLOCKERS && current_turn != player ){
			for (c = 0; c < active_cards_count[player]; ++c ){
				if (in_play(player, c) && get_card_instance(player, c)->blocking != 255 && is_what(player, c, TYPE_CREATURE) &&
					check_for_ability(player, c, KEYWORD_DEFENDER)
				 ){
					if( player == AI || do_dialog(player, player, c, -1, -1, " Gain 2 life\n Pass", 0) == 0 ){
						gain_life(player, 2);
					}
				}
			}
		}
	}
	return 0;
}

int card_permafrost_trap(int player, int card, event_t event){
/* CARD_ID_PERMAFROST_TRAP	10807
Permafrost Trap	|2|U|U
Instant - Trap
If an opponent had a |Sgreen creature enter the battlefield under his or her control this turn, you may pay |U rather than pay ~'s mana cost.
Tap up to two target creatures. Those creatures don't untap during their controller's next untap step. */

	if( event == EVENT_MODIFY_COST && get_trap_condition(1-player, TRAP_PERMAFROST_TRAP) ){
		null_casting_cost(player, card);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				effect_frost_titan(player, card, instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

int card_pilgrims_eye(int player, int card, event_t event){
	/* Pilgrim's Eye	|3
	 * Artifact Creature - Thopter 1/1
	 * Flying
	 * When ~ enters the battlefield, you may search your library for a basic land card, reveal it, put it into your hand, then shuffle your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		tutor_basic_lands(player, TUTOR_HAND, 1);
	}
	return 0;
}

int card_pulse_tracker(int player, int card, event_t event)
{
  /* Pulse Tracker	|B
   * Creature - Vampire Rogue 1/1
   * Whenever ~ attacks, each opponent loses 1 life. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
    lose_life(1-current_turn, 1);
  else if (event == EVENT_ATTACK_RATING && affect_me(player, card))
    ai_defensive_modifier -= 12;
  return 0;
}

int card_quest_for_renewal(int player, int card, event_t event)
{
  /* Quest for Renewal	|1|G
   * Enchantment
   * Whenever a creature you control becomes tapped, you may put a quest counter on ~.
   * As long as there are four or more quest counters on ~, untap all creatures you control during each other player's untap step. */

  int result;
  card_instance_t *instance;
  if ((event == EVENT_TAP_CARD) && (affected_card_controller == player))
  {
    result = is_what(player, affected_card, 2);
    if (result != 0)
    {
      result = is_humiliated(player, card, AL_TRIG);
      if (result == 0)
      {
        instance = in_play(player, card);
        if (instance != NULL)
        {
          add_counters(player, card, player, card, COUNTER_QUEST, 1);
        }
      }
    }
  }
  result = count_counters(player, card, COUNTER_QUEST);
  if (3 < result)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      instance = get_card_instance(player, card);
      untap_permanents_during_opponents_untap(player, card, TYPE_CREATURE, &instance->info_slot);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

int card_quest_for_the_goblin_lord(int player, int card, event_t event)
{
  /* Quest for the Goblin Lord	|R
   * Enchantment
   * Whenever a Goblin enters the battlefield under your control, you may put a quest counter on ~.
   * As long as ~ has five or more quest counters on it, creatures you control get +2/+0. */

  int result;
  test_definition_t test;
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      default_test_definition(&test, 0x47);
      test.subtype = 0x4b;
      result = duh_mode(player);
      etb_ability_global(player, event, (result != 0) + 1, TYPE_NONE, &test, player, card);
    }
  }
  if (event == EVENT_ETB)
  {
    add_counters(player, card, player, card, COUNTER_QUEST, 1);
  }
  else if ((event == EVENT_POWER) && (affected_card_controller == player))
  {
    result = count_counters(player, card, COUNTER_QUEST);
    if (4 < result)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        event_result = event_result + 2;
      }
    }
  }
  return global_enchantment(player, card, event);
}

int card_quest_for_the_nihil_stone(int player, int card, event_t event)
{
  /* Quest for the Nihil Stone	|B
   * Enchantment
   * Whenever an opponent discards a card, you may put a quest counter on ~.
   * At the beginning of each opponent's upkeep, if that player has no cards in hand and ~ has two or more quest counters on it, you may have that player lose 5
   * life. */

  int result;
  int t_player;
  t_player = 1 - player;
  result = discard_trigger(player, card, event, t_player, 3, 0);
  if (result != 0)
  {
    add_counters(player, card, player, card, COUNTER_QUEST, 1);
  }
  if ((trigger_condition == TRIGGER_UPKEEP) && (hand_count[t_player] == 0))
  {
    result = count_counters(player, card, COUNTER_QUEST);
    if (1 < result)
    {
      result = duh_mode(player);
      upkeep_trigger_ability_mode(player, card, event, t_player,
                                  (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
    }
  }
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    lose_life(t_player, 5);
  }
  result = global_enchantment(player, card, event);
  return result;
}

int card_quest_for_ulas_temple(int player, int card, event_t event)
{
  /* Quest for Ula's Temple	|U
   * Enchantment
   * At the beginning of your upkeep, you may look at the top card of your library. If it's a creature card, you may reveal it and put a quest counter on ~.
   * At the beginning of each end step, if there are three or more quest counters on ~, you may put a Kraken, Leviathan, Octopus, or Serpent creature card from
   * your hand onto the battlefield. */

  upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_DUH);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  int* deck = deck_ptr[player];

	  if (deck[0] != -1)
		{
		  if (is_what(-1, deck[0], TYPE_CREATURE))
			{
			  if (!(IS_AI(player) && count_counters(player, card, COUNTER_QUEST) >= 3)
				  && reveal_card_optional_iid(player, card, deck[0], NULL))
				add_counters(player, card, player, card, COUNTER_QUEST, 1);
			}
		  else if (!IS_AI(player))
			look_at_iid(player, card, deck[0], "Top card of deck:");
		}
	}

  if (trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player
	  && count_counters(player, card, COUNTER_QUEST) >= 3)
	{
	  if (event == EVENT_TRIGGER)
		event_result |= duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;

	  if (event == EVENT_RESOLVE_TRIGGER)
		{
		  test_definition_t test;
		  new_default_test_definition(&test, TYPE_CREATURE, "Select a Kraken, Leviathan, Octopus, or Serpent creature card.");
		  test.subtype = SUBTYPE_KRAKEN;
		  test.sub2 = SUBTYPE_LEVIATHAN;
		  test.sub3 = SUBTYPE_OCTOPUS;
		  test.sub4 = SUBTYPE_SERPENT;
		  test.subtype_flag = F2_MULTISUBTYPE;
		  new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
		}
	}

  return global_enchantment(player, card, event);
}

int card_raging_ravine(int player, int card, event_t event){
/* CARD_ID_RAGING_RAVINE	10815
Raging Ravine	""
Land
~ enters the battlefield tapped.
|T: Add |R or |G to your mana pool.
|2|R|G: Until end of turn, ~ becomes a 3/3 |Sred and |Sgreen Elemental creature with "Whenever this creature attacks, put a +1/+1 counter on it." It's still a land. */

	if (!is_humiliated(player, card, AL_TRIG))
    comes_into_play_tapped(player, card, event);
	return manland_normal(player, card, event, MANACOST_XRG(2, 1, 1));
}

int card_raging_ravine_animated(int player, int card, event_t event)
{
  dat_ability(player, card, event, DAT_DONT_REPORT_VALUE | DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  return manland_animated(player, card, event, MANACOST_XRG(2, 1, 1));
}

int card_rest_for_the_weary(int player, int card, event_t event){
/* CARD_ID_REST_FOR_THE_WEARY	10819
Rest for the Weary	|1|W
Instant
Target player gains 4 life.
Landfall - If you had a land enter the battlefield under your control this turn, that player gains 8 life instead. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( life[player] < 6 ){
			ai_modifier += 48;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = 4;
			if( get_trap_condition(player, TRAP_LANDS_PLAYED) > 0 ){
				amount+=4;
			}
			gain_life(instance->targets[0].player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

/* ManalinkEh 2022: 0x2589fc0; original cards/worldwake.c:2238. */
int card_roiling_terrain(int player, int card, event_t event)
{
  /* Roiling Terrain	|2|R|R
   * Sorcery
   * Destroy target land, then ~ deals damage to that land's controller equal to the number of land cards in that player's graveyard. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 1);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {

      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      result = count_graveyard_by_type(instance->targets[0].player, 1);
      damage_player(instance->targets[0].player, result + 1, player, card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_LAND", 1, NULL);
}

int card_ruin_ghost(int player, int card, event_t event){
/* CARD_ID_RUIN_GHOST	10822
Ruin Ghost	|1|W
Creature - Spirit 1/1
|W, |T: Exile target land you control, then return it to the battlefield under your control. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.allowed_controller = td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			blink_effect_controller(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST_W(1), 0,
									&td, "Select target land you control.");
}

int card_ruthless_cullblade(int player, int card, event_t event){
/* CARD_ID_RUTHLESS_CULLBLADE	10824
Ruthless Cullblade	|1|B
Creature - Vampire Warrior 2/1
~ gets +2/+1 as long as an opponent has 10 or less life. */

	if( is_humiliated(player, card, AL_STATIC) )
	   return 0;

	if( event == EVENT_POWER && affect_me(player, card) && life[1-player] <= 10 )
		event_result += 2;

	if( event == EVENT_TOUGHNESS && affect_me(player, card) && life[1-player] <= 10 )
		event_result ++;

	return 0;
}

int card_searing_blaze(int player, int card, event_t event){
	/* Searing Blaze	|R|R	0x2002578
	 * Instant
	 * ~ deals 1 damage to target player or planeswalker and 1 damage to target creature that player or that planeswalker's controller controls.
	 * Landfall - If you had a land enter the battlefield under your control this turn, ~ deals 3 damage to that player or planeswalker and 3 damage to that creature instead. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( can_target(&td) && basic_spell(player, card, event) ){
			int i;
			for(i=0; i<2; i++){
				if( would_validate_arbitrary_target(&td, i, -1) ){
					td2.allowed_controller = td2.preferred_controller = i;
					if( can_target(&td2) ){
						return 1;
					}
				}
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
		if( spell_fizzled != 1 ){
			if( instance->targets[0].player == player )
				ai_modifier -= 48;
			td2.allowed_controller = td2.preferred_controller = instance->targets[0].player;
			char msg[100];
			if( instance->targets[0].player == player ){
				scnprintf(msg, 100, "Select target creature you control.");
			}
			else{
				scnprintf(msg, 100, "Select target creature your opponent controls.");
			}
			new_pick_target(&td2, msg, 1, 1	| GS_LITERAL_PROMPT);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = 1;
		if( get_trap_condition(player, TRAP_LANDS_PLAYED) > 0 ){
			amount = 3;
		}
		if( validate_target(player, card, &td, 0) ){
			damage_target0(player, card, amount);
		}
		if( validate_target(player, card, &td2, 1) ){
			damage_creature(instance->targets[1].player, instance->targets[1].card, amount, player, card);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_seer_sundial(int player, int card, event_t event){
/* CARD_ID_SEERS_SUNDIAL	10827
Seer's Sundial	|4
Artifact
Landfall - Whenever a land enters the battlefield under your control, you may pay |2. If you do, draw a card. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		if( has_mana(player, COLOR_ANY, 2) && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 2) ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_sejiri_merfolk(int player, int card, event_t event)
{
/* CARD_ID_SEJIRI_MERFOLK	10828
Sejiri Merfolk	|1|U
Creature - Merfolk Soldier 2/1
As long as you control |Ha Plains, ~ has first strike and lifelink. (Damage dealt by a creature with lifelink also causes its controller to gain that much life.) */

  if (event == EVENT_ABILITIES
	  && affect_me(player, card)
	  && ! is_humiliated(player, card, AL_STATIC)
	  && basiclandtypes_controlled[player][get_hacked_color(player, card, COLOR_WHITE)] >= 1)
	{
	  event_result |= KEYWORD_FIRST_STRIKE;
	  lifelink(player, card, event);
	}
  return 0;
}

int card_sejiri_steppe(int player, int card, event_t event){
/* CARD_ID_SEJIRI_STEPPE	10829
Sejiri Steppe	""
Land
~ enters the battlefield tapped.
When ~ enters the battlefield, target creature you control gains protection from the color of your choice until end of turn.
|T: Add |W to your mana pool. */

	if (!is_humiliated(player, card, AL_TRIG)){
		comes_into_play_tapped(player, card, event);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if (event == EVENT_ETB_ABILITY && !is_humiliated(player, card, AL_TRIG)){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;
		card_instance_t* instance = get_card_instance(player, card);
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			int kw = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, kw, 0);
		}
		instance->number_of_targets = 0;
	}

	return plain_mana_producer(player, card, event);
}

int card_slavering_nulls(int player, int card, event_t event)
{
  /* Slavering Nulls	|1|R
   * Creature - Goblin Zombie 2/1
   * Whenever ~ deals combat damage to a player, if you control |Ha Swamp, you may have that player discard a card. */

  if ((event == EVENT_DEAL_DAMAGE || event == EVENT_GRAVEYARD_FROM_PLAY ||
       trigger_condition == TRIGGER_DEAL_DAMAGE || event == EVENT_CHANGE_TYPE) &&
      basiclandtypes_controlled[player][get_hacked_color(player, card, COLOR_BLACK)] > 0)
    whenever_i_deal_damage_to_a_player_he_discards_a_card(player, card, event,
        DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_TRIGGER_OPTIONAL, 0);
  return 0;
}

int card_smoldering_spires(int player, int card, event_t event){
/* CARD_ID_SMOLDERING_SPIRES	10835
Smoldering Spires	""
Land
~ enters the battlefield tapped.
When ~ enters the battlefield, target creature can't block this turn.
|T: Add |R to your mana pool. */

	if (!is_humiliated(player, card, AL_TRIG)){
		comes_into_play_tapped(player, card, event);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if (event == EVENT_ETB_ABILITY && !is_humiliated(player, card, AL_TRIG)){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;
		card_instance_t* instance = get_card_instance(player, card);
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
		}
		instance->number_of_targets = 0;
	}
	return mana_producer(player, card, event);
}

int card_stirring_wildwood(int player, int card, event_t event){
/* CARD_ID_STIRRING_WILDWOOD	10838
Stirring Wildwood	""
Land
~ enters the battlefield tapped.
|T: Add |G or |W to your mana pool.
|1|G|W: Until end of turn, ~ becomes a 3/4 |Sgreen and |Swhite Elemental creature with reach. It's still a land. */

	if (!is_humiliated(player, card, AL_TRIG))
    comes_into_play_tapped(player, card, event);
	return manland_normal(player, card, event, MANACOST_XGW(1, 1, 1));
}

int card_stirring_wildwood_animated(int player, int card, event_t event){

	return manland_animated(player, card, event, MANACOST_XGW(1, 1, 1));
}

int card_stoneforge_mystic( int player, int card, event_t event){
/* CARD_ID_STONEFORGE_MYSTIC	10841
Stoneforge Mystic	|1|W
Creature - Kor Artificer 1/2
When ~ enters the battlefield, you may search your library for an Equipment card, reveal it, put it into your hand, then shuffle your library.
|1|W, |T: You may put an Equipment card from your hand onto the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "Select an Equipment card.");
		this_test.subtype = SUBTYPE_EQUIPMENT;
		this_test.zone = TARGET_ZONE_HAND;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_CMC, &this_test);
	}

	if(event == EVENT_ACTIVATE && player == AI ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_EQUIPMENT;
		this_test.zone = TARGET_ZONE_HAND;
		if( ! check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			ai_modifier-=50;
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		char msg[100] = "Select an Equipment card.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, msg);
		this_test.subtype = SUBTYPE_EQUIPMENT;
		this_test.zone = TARGET_ZONE_HAND;

		new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XW(1, 1), 0, NULL, NULL);
}

int card_strength_of_the_tajuru(int player, int card, event_t event)
{
  /* CARD_ID_STRENGTH_OF_THE_TAJURU	10842
   * Strength of the Tajuru	|X|G|G
   * Instant
   * Multikicker |1 (You may pay an additional |1 any number of times as you cast this spell.)
   * Choose target creature, then choose another target creature for each time ~ was kicked. Put X +1/+1 counters on each of them. */

  target_t *chosen_player_record;
  target_t *chosen_card_record;
  card_instance_t *instance;
  int target_or_payment_result;
  int i;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 5, &td, NULL, 1, NULL);
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
            (generic_spell(player, card, EVENT_CAST_SPELL, 5, &td, "TARGET_CREATURE", 1, NULL),
             spell_fizzled != 1))))
      {
        i = 1;
        state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
        while (((((target_or_payment_result = has_mana(player, 0, 1),
                   target_or_payment_result != 0 &&
                       (target_or_payment_result = can_target(&td), i < 10)) &&
                  (target_or_payment_result != 0)) &&
                 ((target_or_payment_result =
                       do_dialog(player, player, card, -1, -1, " Pay kicker\n Pass", 0),
                   target_or_payment_result == 0 &&
                       (charge_mana(player, COLOR_COLORLESS, 1), spell_fizzled != 1)))) &&
                (target_or_payment_result = new_pick_target(&td, "TARGET_CREATURE", i, 1),
                 target_or_payment_result != 0)))
        {
          chosen_card_record = instance->targets + i;
          chosen_player_record = instance->targets + i;
          i = i + 1;
          state_untargettable(chosen_player_record->player, chosen_card_record->card, 1);
        }
        i = 0;
        if (instance->number_of_targets != '\0')
        {
          do
          {
            chosen_card_record = instance->targets + i;
            chosen_player_record = instance->targets + i;
            i = i + 1;
            state_untargettable(chosen_player_record->player, chosen_card_record->card, 0);
          } while (i < (int)(unsigned int)instance->number_of_targets);
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
          target_or_payment_result = validate_target(player, card, &td, i);
          if (target_or_payment_result != 0)
          {
            chosen_card_record = instance->targets + i;
            chosen_player_record = instance->targets + i;
            i = i + 1;
            add_counters(player, card, chosen_player_record->player, chosen_card_record->card,
                         COUNTER_P1_P1, instance->info_slot);
          }
          else
            i = i + 1;
        } while (i < (int)(unsigned int)instance->number_of_targets);
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

int card_summit_apes(int player, int card, event_t event){
	/* Summit Apes	|3|G
	 * Creature - Ape 5/2
	 * As long as you control |Ha Mountain, ~ has menace. */

	if( ! is_humiliated(player, card, AL_STATIC) && check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_MOUNTAIN) ){
		menace(player, card, event);
	}
	return 0;
}

int card_talus_paladin(int player, int card, event_t event){
/* CARD_ID_TALUS_PALADIN	10845
Talus Paladin	|3|W
Creature - Human Knight Ally 2/3
Whenever ~ or another Ally enters the battlefield under your control, you may have Allies you control gain lifelink until end of turn,
and you may put a +1/+1 counter on ~. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( event == EVENT_ETB_ABILITY ){	// but choose
		int choice = duh_mode(player) ? 3 : DIALOG(player, card, EVENT_ACTIVATE,
												   DLG_NO_STORAGE,
												   "Your Allies gain lifelink", 1, 1,
												   "+1/+1 counter", 1, 2,
												   "Both", 1, 3);
		if (choice & 1){
			pump_subtype_until_eot(player, card, player, SUBTYPE_ALLY, 0, 0, 0, SP_KEYWORD_LIFELINK);
		}
		if (choice & 2){
			add_counters(player, card, player, card, COUNTER_P1_P1, 1);
		}
	}
	return 0;
}

int card_tectonic_edge(int player, int card, event_t event){
  /* Tectonic Edge	""
   * Land
   * |T: Add |C.
   * |1, |T, Sacrifice this land: Destroy target nonbasic land. Activate only if an opponent controls four or more lands. */

	/*
	  Tectonic Edge
	  Land
	  {T}: Add {1} to your mana pool.
	  {1}, {T}, Sacrifice Tectonic Edge: Destroy target nonbasic land. Activate this ability only if an opponent controls four or more lands.
	*/
	card_instance_t *instance = get_card_instance(player, card);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = SUBTYPE_NONBASIC;

	if (event == EVENT_ACTIVATE){
		instance->info_slot = 0;
		int choice = 0;
		if( !paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_X(2), 0, &td, NULL)
			&& count_subtype(1-player, TYPE_LAND, -1) > 3
		  ){
			choice = do_dialog(player, player, card, -1, -1, " Get mana\n Destroy a nonbasic land\n Cancel", 1);
		}
		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		else if( choice == 1 ){
				add_state(player, card, STATE_TAPPED);
				if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) &&
					new_pick_target(&td, "Select target nonbasic land.", 0, 1 | GS_LITERAL_PROMPT)
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

static int effect_terastodon(int player, int card, event_t event)
{
  if (effect_put_into_a_graveyard_this_way(player, card, event))
	{
	  token_generation_t token;
	  default_token_definition(player, card, CARD_ID_ELEPHANT, &token);
	  token.t_player = get_card_instance(player, card)->targets[0].player;
	  generate_token(&token);
	}
  return 0;
}

int card_terastodon(int player, int card, event_t event)
{
  /* Terastodon	|6|G|G
   * Creature - Elephant 9/9
   * When ~ enters the battlefield, you may destroy up to three target noncreature permanents. For each permanent put into a graveyard this way, its controller
   * puts a 3/3 |Sgreen Elephant creature token onto the battlefield. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_PERMANENT);
	  td.illegal_type = TYPE_CREATURE;
	  td.allow_cancel = 3;	// done and cancel buttons

	  card_instance_t* instance = get_card_instance(player, card);

	  int i, count = pick_up_to_n_targets(&td, "TARGET_NONCREATURE_PERMANENT", 3);

	  for (i = 0; i < count; ++i)
		{
		  create_targetted_legacy_effect(player, card, effect_terastodon, instance->targets[i].player, instance->targets[i].card);
		  kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
		}
	}

  return 0;
}

int card_terra_eternal(int player, int card, event_t event){
/* CARD_ID_TERRA_ETERNAL	10848
Terra Eternal	|2|W
Enchantment
All lands have indestructible. */

	if( is_humiliated(player, card, AL_STATIC) )
		return 0;

	if( event == EVENT_CHECK_PUMP && is_what(affected_card_controller, affected_card, TYPE_LAND) &&
		is_what(affected_card_controller, affected_card, TYPE_CREATURE) && ! is_tapped(affected_card_controller, affected_card) )
	{
		pumpable_toughness[player] += 99;
	}

	if( event == EVENT_ABILITIES && is_what(affected_card_controller, affected_card, TYPE_LAND) &&
		in_play(player, card) )
	{
		special_abilities(affected_card_controller, affected_card, event, SP_KEYWORD_INDESTRUCTIBLE, player, card);
	}

	return global_enchantment(player, card, event);
}

int card_thada_adel_acquisitor(int player, int card, event_t event)
{
  /* Thada Adel, Acquisitor	|1|U|U
   * Legendary Creature - Merfolk Rogue 2/2
   * |H2Islandwalk
   * Whenever ~ deals combat damage to a player, search that player's library for an artifact card and exile it. Then that player shuffles his or her
   * library. Until end of turn, you may play that card. */

	check_legend_rule(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE );
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					while( times_damaged[p] ){
							test_definition_t this_test;
							new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an artifact card.");

							int csvid = new_global_tutor(player, p, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_MAX_VALUE, &this_test);
							if (csvid >= 0)
								create_may_play_card_from_exile_effect(player, card, 1-player, csvid, MPCFE_UNTIL_EOT);
							times_damaged[p]--;
					}
				}
		);
	}

	return 0;
}

int card_tideforce_elemental(int player, int card, event_t event){
/* CARD_ID_TIDEFORCE_ELEMENTAL	10850
Tideforce Elemental	|2|U
Creature - Elemental 2/1
|U, |T: You may tap or untap another target creature.
Landfall - Whenever a land enters the battlefield under your control, you may untap ~. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		untap_card(player, card);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			twiddle(player, card, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_NOT_ME_AS_TARGET, MANACOST_U(1), 0,
									&td, "TARGET_CREATURE");
}

int card_treasure_hunt(int player, int card, event_t event){
/* CARD_ID_TREASURE_HUNT	10852
Treasure Hunt	|1|U
Sorcery
Reveal cards from the top of your library until you reveal a nonland card, then put all cards revealed this way into your hand. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 24;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( deck_ptr[player][0] > -1 ){
			int count = 0;
			while( deck_ptr[player][count] != -1 ){
					int iid = deck_ptr[player][count];
					if( ! is_what(-1, iid, TYPE_LAND) ){
						break;
					}
					count++;
			}
			show_deck( 0, deck_ptr[player], count+1, "Cards revealed by Treasure Hunt", 0, gs_done_008b40e0 );
			show_deck( 1, deck_ptr[player], count+1, "Cards revealed by Treasure Hunt", 0, gs_done_008b40e0 );
			int i;
			for(i=0; i<=count; i++){
				add_card_to_hand(player, deck_ptr[player][0]);
				remove_card_from_deck(player, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_tuktuk_scrapper(int player, int card, event_t event){
/* CARD_ID_TUKTUK_SCRAPPER	10853
Tuktuk Scrapper	|3|R
Creature - Goblin Artificer Ally 2/2
Whenever ~ or another Ally enters the battlefield under your control, you may destroy target artifact.
If that artifact is put into a graveyard this way, ~ deals damage to that artifact's controller equal to the number of Allies you control. */

	ally_trigger(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && new_pick_target(&td, "TARGET_ARTIFACT", 0, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			damage_player(instance->targets[0].player, count_subtype(player, TYPE_PERMANENT, SUBTYPE_ALLY), player, card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_urge_to_feed(int player, int card, event_t event){
/* CARD_ID_URGE_TO_FEED	10854
Urge to Feed	|B|B
Instant
Target creature gets -3/-3 until end of turn. You may tap any number of untapped Vampire creatures you control.
If you do, put a +1/+1 counter on each of those Vampires. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_SPELL  ){
		if(  valid_target(&td) ){
			card_instance_t *instance= get_card_instance(player, card);

			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -3, -3);

			target_definition_t td2;
			default_target_definition(player, card, &td2, TYPE_CREATURE);
			td2.required_subtype = SUBTYPE_VAMPIRE;
			td2.allowed_controller = td2.preferred_controller = player;
			td2.illegal_abilities = 0;
			td2.illegal_state = TARGET_STATE_TAPPED;

			while( can_target(&td2) ){
					if( new_pick_target(&td2, "Choose an untapped Vampire to tap", 0, GS_LITERAL_PROMPT) ){
						tap_card( instance->targets[0].player, instance->targets[0].card );
						add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
					}
					else{
						break;
					}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x258c250; original cards/worldwake.c:2926. */
int card_vastwood_zendikon(int player, int card, event_t event)
{
  /* Vastwood Zendikon	|4|G
   * Enchantment - Aura
   * Enchant land
   * Enchanted land is a 6/4 green Elemental creature. It's still a land.
   * When enchanted land dies, return that card to its owner's hand. */

  int result;

  result = zendikon(player, card, event, 6, 4, COLOR_TEST_GREEN, SUBTYPE_ELEMENTAL, KEYWORD_0,
                    SP_KEYWORD_0);
  return result;
}

/* ManalinkEh 2022: 0x258c2b0; original cards/worldwake.c:2939. */
int card_wind_zendikon(int player, int card, event_t event)
{
  /* Wind Zendikon	|U
   * Enchantment - Aura
   * Enchant land
   * Enchanted land is a 2/2 blue Elemental creature with flying. It's still a land.
   * When enchanted land dies, return that card to its owner's hand. */

  int result;

  result = zendikon(player, card, event, 2, 2, COLOR_TEST_BLUE, SUBTYPE_ELEMENTAL, KEYWORD_FLYING,
                    SP_KEYWORD_0);
  return result;
}

// walking atlas --> Sakura-Tribe Scout

int card_wolfbriar_elemental(int player, int card, event_t event){
	/* Wolfbriar Elemental	|2|G|G
	 * Creature - Elemental 4/4
	 * Multikicker |G
	 * When ~ enters the battlefield, put a 2/2 |Sgreen Wolf creature token onto the battlefield for each time it was kicked. */

	multikicker(player, card, event, MANACOST_G(1));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_tokens_by_id(player, card, CARD_ID_WOLF, multikicked(player, card));
	}

	return 0;
}

int card_wrexial_the_risen_deep(int player, int card, event_t event){
  /* Wrexial, the Risen Deep |3|U|U|B
   * Legendary Creature — Kraken 5/8
   * Islandwalk, swampwalk (This creature can't be blocked as long as defending player controls an Island or a Swamp.)
   * Whenever Wrexial, the Risen Deep deals combat damage to a player, you may cast target instant or sorcery card from that player's graveyard
   * 	without paying its mana cost. If that card would be put into a graveyard this turn, exile it instead. */

	check_legend_rule(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE );
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					while( times_damaged[p] ){
							test_definition_t this_test;
							new_default_test_definition(&this_test, TYPE_SPELL, "Select an Instant or Sorcery card.");
							this_test.can_legally_play = 1;
							if( ! graveyard_has_shroud(p) ){
								int selected = new_select_a_card(player, p, TUTOR_FROM_GRAVE, 0, AI_MAX_CMC, -1, &this_test);
								if( selected != -1 ){
									play_card_in_grave_for_free_and_exile_it(player, p, selected);
								}
							}
							times_damaged[p]--;
					}
				}
		);
	}

	return 0;
}
