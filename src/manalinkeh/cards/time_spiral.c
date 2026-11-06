#include "manalink.h"

// Functions

int card_ali_from_cairo(int player, int card, event_t event);

int card_ali_from_cairo(int player, int card, event_t event);

int card_candelabra_of_tawnos(int player, int card, event_t event);

int card_mirror_universe(int player, int card, event_t event);

int glass_asp_legacy(int player, int card, event_t event);

#ifndef SHANDALAR

static int re_enable_abilities_at_eot(int player, int card, event_t event);

#endif

int generic_suspend_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  xtrigger_t milled_trigger;
  int csvid_count_or_result;
  int32_t dialog_default_or_owner;
  int opponent_trigger_or_mode;
  int remaining_upkeeps;
  token_generation_t token;

  instance = get_card_instance(player, card);
  if (event == EVENT_SET_LEGACY_EFFECT_NAME)
  {
    scnprintf(set_legacy_effect_name_addr, 100, cards_ptr[instance->targets[9].card]->name);
  }
  else if (event == EVENT_SET_LEGACY_EFFECT_TEXT)
  {
    scnprintf(set_legacy_effect_name_addr, 400,
              "Suspend %d - At the beginning of your upkeep, remove a time counter. When the last "
              "is removed, cast %s without paying its mana cost.",
              instance->targets[8].player, cards_ptr[instance->targets[9].card]->name);
  }
  if (instance->targets[7].player == 0x2a)
  {
    return 0;
  }
  if ((instance->targets[6].player == current_turn) ||
      (csvid_count_or_result = instance->targets[9].card, csvid_count_or_result == 0x2140))
  {
    if (upkeep_trigger(player, card, event) != 0)
    {
      remaining_upkeeps = count_upkeeps(current_turn);
      csvid_count_or_result = instance->targets[9].card;
      if (csvid_count_or_result == 0x2140)
      {
        if (remaining_upkeeps > 0)
        {
          do
          {
            csvid_count_or_result = count_counters(player, card, COUNTER_TIME);
            if (csvid_count_or_result < 1)
              break;
            if (instance->targets[6].card == 0)
            {
              instance->targets[6].card = 1;
            }
            else
            {
              csvid_count_or_result = count_counters(player, card, COUNTER_TIME);
              bool ask_curse = false;
              bool refresh_owner = false;
              if (csvid_count_or_result < 2)
              {
                dialog_default_or_owner = 0;
                if (instance->targets[6].player != player)
                  ask_curse = true;
                else
                {
                  dialog_default_or_owner = 1;
                  ask_curse = player != 1;
                }
                refresh_owner = ask_curse;
              }
              else
              {
                if (player != 1)
                {
                  dialog_default_or_owner = 1;
                  ask_curse = true;
                }
                refresh_owner = true;
              }
              if (ask_curse)
              {
                csvid_count_or_result = do_dialog(
                    current_turn, player, card, -1, -1,
                    " Add 2 time counters to Curse of the Cabal\n Pass", dialog_default_or_owner);
                if (csvid_count_or_result == 0 &&
                    (csvid_count_or_result =
                         sacrifice(player, card, current_turn, 0, 0x47, 0, 0, 0, 0, 0, 0, 0, -1, 0),
                     csvid_count_or_result != 0))
                  add_counters(player, card, player, card, COUNTER_TIME, 2);
              }
              if (refresh_owner)
                dialog_default_or_owner = instance->targets[6].player;
              if (dialog_default_or_owner == current_turn)
              {
                remove_counters(player, card, player, card, COUNTER_TIME, 1);
              }
            }
            remaining_upkeeps = remaining_upkeeps + -1;
          } while (remaining_upkeeps != 0);
        }
      }
      else
      {
        if (remaining_upkeeps > 0)
        {
          do
          {
            csvid_count_or_result = count_counters(player, card, COUNTER_TIME);
            if (csvid_count_or_result < 1)
              break;
            if (instance->targets[6].card == 0)
            {
              instance->targets[6].card = 1;
            }
            else
            {
              remove_counters(player, card, player, card, COUNTER_TIME, 1);
            }
            remaining_upkeeps = remaining_upkeeps + -1;
          } while (remaining_upkeeps != 0);
        }
      }
    }
    csvid_count_or_result = instance->targets[9].card;
  }
  if (csvid_count_or_result == 0x2317)
  {
    csvid_count_or_result = duh_mode(player);
    if (when_a_card_is_put_into_graveyard_from_anywhere_but_library_do_something(
            player, card, event, 1 - player,
            (csvid_count_or_result != 0) + RESOLVE_TRIGGER_OPTIONAL, NULL) != 0)
    {
      remove_counters(player, card, player, card, COUNTER_TIME, 1);
    }
    enable_xtrigger_flags = enable_xtrigger_flags | ENABLE_XTRIGGER_MILLED;
    milled_trigger = trigger_condition;
    if (trigger_condition == TRIGGER_XTRIGGER)
    {
      milled_trigger = xtrigger_impl_value_dont_use_directly;
    }
    if ((((milled_trigger == XTRIGGER_MILLED) && (card == affected_card)) &&
         (player == affected_card_controller)) &&
        ((reason_for_trigger_controller == player && (1 - player == trigger_cause_controller))))
    {
      if (event == EVENT_TRIGGER)
      {
        csvid_count_or_result = duh_mode(player);
        event_result = event_result | (csvid_count_or_result != 0) + 1;
        csvid_count_or_result = instance->targets[9].card;
      }
      if (event == EVENT_RESOLVE_TRIGGER)
      {
        remove_counters(player, card, player, card, COUNTER_TIME, num_cards_milled);
      }
    }
    csvid_count_or_result = instance->targets[9].card;
  }
  if (csvid_count_or_result == 0x2145)
  {
    if (specific_spell_played(player, card, event, 1 - player, 2, 0xff, 0, 0, 0, 0, 0, 0, 0, -1,
                              0) != 0)
    {
      remove_counters(player, card, player, card, COUNTER_TIME, 1);
    }
    csvid_count_or_result = instance->targets[9].card;
  }
  if (csvid_count_or_result == 0x21b2)
  {
    csvid_count_or_result = 1 - player;
    opponent_trigger_or_mode = specific_spell_played(player, card, event, csvid_count_or_result, 2,
                                                     0xff, 0, 0, 0, 0, 0, 0, 0, -1, 0);
    if ((opponent_trigger_or_mode != 0) &&
        ((opponent_trigger_or_mode = duh_mode(csvid_count_or_result),
          opponent_trigger_or_mode != 0 ||
              (csvid_count_or_result = do_dialog(csvid_count_or_result, player, card, -1, -1,
                                                 " Put a time counter on Pardic Dragon\n Pass", 0),
               csvid_count_or_result == 0))))
    {
      add_counters(player, card, player, card, COUNTER_TIME, 1);
    }
  }
  if ((event == EVENT_STATIC_EFFECTS) &&
      (csvid_count_or_result = count_counters(player, card, COUNTER_TIME),
       csvid_count_or_result < 1))
  {
    csvid_count_or_result = instance->targets[9].card;
    instance->targets[7].player = 0x2a;
    if (check_rfg(instance->targets[6].player, csvid_count_or_result) != 0)
    {
      csvid_count_or_result = instance->targets[6].player;
      csvid_count_or_result = play_card_in_exile_for_free(
          csvid_count_or_result, csvid_count_or_result, instance->targets[9].card);
      if (is_what(instance->targets[6].player, csvid_count_or_result, 2) != 0)
      {
        pump_ability_t haste_pump;
        default_pump_ability_definition(player, card, &haste_pump, 0, 0, 0, SP_KEYWORD_HASTE);
        haste_pump.paue_flags = PAUE_END_IF_TARGET_CHANGES_CONTROL;
        pump_ability(player, card, instance->targets[6].player, csvid_count_or_result, &haste_pump);
      }
      if (instance->targets[9].card == 0x2285)
      {
        default_token_definition(player, card, CARD_ID_KNIGHT, &token);
        token.key_plus = get_sleighted_protection(player, card, 0x8000);
        token.s_key_plus = 0x2004;
        token.color_forced = '\x02';
        generate_token(&token);
      }
    }
    kill_card(player, card, KILL_EXILE);
    recalculate_all_cards_in_play();
  }
  if ((instance->targets[6].card == 0) && (current_phase != 2))
  {
    instance->targets[6].card = 1;
  }
  return 0;
}

int is_suspend_legacy(int player, int card){
	if( is_what(player, card, TYPE_EFFECT) ){
		return get_card_instance(player, card)->info_slot == (intptr_t)generic_suspend_legacy ? 1 : 0;
	}
	return 0;
}

int suspend_a_card(int player, int card, int t_player, int t_card, int turns)
{
  int result;
  card_instance_t *instance;
  unsigned int suspended_csvid;
  int suspend_legacy;

  suspended_csvid = t_player == -1 ? (unsigned int)cards_data[t_card].id : get_id(t_player, t_card);
  if (suspended_csvid != 0x2177)
  {
    suspend_legacy = create_legacy_effect(player, card, generic_suspend_legacy);
    instance = get_card_instance(player, suspend_legacy);
    instance->targets[6].card = 0;
    result = t_player == -1 ? player : t_player;
    instance->targets[6].player = result;
    instance->targets[7].player = 0;
    result = get_id(player, card);
    instance->targets[7].card = result;
    instance->targets[8].player = turns;
    add_counters(player, card, player, suspend_legacy, COUNTER_TIME, turns);
    instance->targets[9].card = suspended_csvid;
    instance->eot_toughness = 0x300;
  }
  else
  {
    result = get_internal_card_id_from_csv_id(CARD_ID_GREATER_GARGADON_SUSPENDED);
    result = add_card_to_hand(player, result);
    add_counters(player, card, player, result, COUNTER_TIME, turns);
    put_into_play(player, result);
  }
  if (t_player != -1)
    kill_card(t_player, t_card, KILL_EXILE);
  return 0;
}

int suspend(int player, int card, event_t event, int turns, int colorless, int black, int blue,
            int green, int red, int white, target_definition_t *td, const char *prompt)
{
  if (event == EVENT_MODIFY_COST)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int csvid = get_id(player, card);
    int cless = colorless;
    if (cless == -1)
    {
      cless = 0;
    }
    if (csvid == CARD_ID_DETRITIVORE || csvid == CARD_ID_BENALISH_COMMANDER ||
        csvid == CARD_ID_FUNGAL_BEHEMOTH || csvid == CARD_ID_ROILING_HORROR)
    {
      cless = 1; // Cannot be suspended with X = 0
    }
    if (has_mana_multi(player, cless, black, blue, green, red, white))
    {
      null_casting_cost(player, card);
      instance->info_slot = 1;
    }
    else
    {
      instance->info_slot = 0;
    }
  }

  if (event == EVENT_CAN_CAST)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->info_slot && !played_for_free(player, card) && !is_token(player, card))
    {
      return 1;
    }
    return !td || can_target(td);
  }

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    enum
    {
      CHOICE_NORMAL = 1,
      CHOICE_SUSPEND
    } choice = CHOICE_NORMAL;
    int ff = played_for_free(player, card) | is_token(player, card);
    if (!ff && instance->info_slot == 1)
    {
      int cless = colorless;
      if (cless == -1)
      {
        cless = 0;
      }
      int csvid = get_id(player, card);
      choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1, "Play normally",
                      has_mana_to_cast_id(player, event, csvid) && (!td || can_target(td)), 2,
                      "Suspend", has_mana_multi(player, cless, black, blue, green, red, white), 1);
    }
    if (choice == CHOICE_NORMAL)
    {
      if (instance->info_slot == 1 && !ff)
      {
        if (!charge_mana_from_id(player, card, event, get_id(player, card)))
        {
          spell_fizzled = 1;
          return 0;
        }
      }
      if (td)
      {
        instance->number_of_targets = 0;
        pick_target(td, prompt);
      }
    }
    else if (choice == CHOICE_SUSPEND)
    {
      int csvid = get_id(player, card);
      int cless = colorless;
      if (cless == -1)
      {
        cless = 0;
      }
      if (charge_mana_multi(player, cless, black, blue, green, red, white))
      {
        if (csvid == CARD_ID_DETRITIVORE || csvid == CARD_ID_BENALISH_COMMANDER ||
            csvid == CARD_ID_FUNGAL_BEHEMOTH || csvid == CARD_ID_ROILING_HORROR)
        {
          charge_mana(player, COLOR_COLORLESS, -1);
          if (x_value == 0)
          { // Cannot be suspended with X = 0
            spell_fizzled = 1;
            return 0;
          }
        }
        if (turns == -1)
        {
          turns = x_value;
        }
        suspend_a_card(player, card, player, card, turns);
      }
    }
    else
    {
      spell_fizzled = 1;
    }
  }

  return 0;
}

int suspend_only(int player, int card, event_t event, int turns, int colorless, int black, int blue, int green, int red, int white)
{
	card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_MODIFY_COST && !played_for_free(player, card))
	{
		if( has_mana_multi(player, colorless, black, blue, green, red, white) ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
			infinite_casting_cost();
		}
	}

  if (event == EVENT_CAN_CAST)
	return 1;

  if (event == EVENT_CAST_SPELL && affect_me(player, card) && !played_for_free(player, card) && !is_token(player, card))
	{
		if( instance->info_slot == 1 )
			charge_mana_multi(player, colorless, black, blue, green, red, white);
		if( spell_fizzled != 1 ){
			suspend_a_card(player, card, player, card, turns);
			land_can_be_played &= ~LCBP_SPELL_BEING_PLAYED;
		}
	}

  return 0;
}

int is_suspend_only_card(int player, int card)
{
  int csvid = player == -1 ? cards_data[card].id : get_id(player, card);
  switch (csvid)
  {
  case CARD_ID_HYPERGENESIS:
    return 1;
  case CARD_ID_ANCESTRAL_VISION:
    return 1;
  case CARD_ID_LIVING_END:
    return 1;
  case CARD_ID_RESTORE_BALANCE:
    return 1;
  case CARD_ID_WHEEL_OF_FATE:
    return 1;
  case CARD_ID_LOTUS_BLOOM:
    return 1;
  case CARD_ID_CRASHING_FOOTFALLS:
  case CARD_ID_MOX_TANTALITE:
  case CARD_ID_GAEAS_WILL:
  case CARD_ID_GLIMPSE_OF_TOMORROW:
  case CARD_ID_INEVITABLE_BETRAYAL:
  case CARD_ID_PROFANE_TUTOR:
  case CARD_ID_RESURGENT_BELIEF:
  case CARD_ID_SOL_TALISMAN:
    return 1;
  };
  return 0;
}

void holy_nimbus(int player, int card, event_t event, int cost)
{
	if (land_can_be_played & LCBP_REGENERATION){
		card_instance_t* instance = get_card_instance(player, card);

		if( instance->info_slot == 1 ){
			return;
		}

		if( instance->targets[1].player != 66 && instance->kill_code == KILL_DESTROY && can_regenerate(player, card) ){
			int choice = 0;
			if( has_mana(1-player, COLOR_COLORLESS, cost) ){
				choice = do_dialog(1-player, player, card, -1, -1, " Let it regenerate\n Negate this ability until EOT", 1);
			}
			if (choice == 1 && charge_mana_while_resolving(player, card, EVENT_ACTIVATE, 1-player, COLOR_COLORLESS, cost)){
				instance->targets[1].player = 66;
			} else {
				cancel = 0;
				regenerate_target(player, card);
			}
		}

		instance->info_slot = 0;
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
	}
}

static int can_make_saproling_from_fungus(int player, int card)
{
	return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3), NULL, NULL);
}

static void saproling_from_fungus(int player, int card)
{
	generic_activated_ability(player, card, EVENT_ACTIVATE, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3), NULL, NULL);
}

// Cards

int card_academy_ruins(int player, int card, event_t event){
/* CARD_ID_ACADEMY_RUINS	8474
Academy Ruins	""
Legendary Land
|T: Add |C to your mana pool.
|1|U, |T: Put target artifact card from your graveyard on top of your library. */

  check_legend_rule(player, card, event);

  return recycling_land(player, card, event, TYPE_ARTIFACT, MANACOST_XU(1, 1));
}

int card_aether_web(int player, int card, event_t event){
/* CARD_ID_AETHER_WEB	8482
Aether Web	|1|G
Enchantment - Aura
Flash (You may cast this spell any time you could cast an instant.)
Enchant creature
Enchanted creature gets +1/+1, has reach, and can block creatures with shadow as though they didn't have shadow.
 (Creatures with reach can block creatures with flying.) */

  return generic_aura(player, card, event, player, 1, 1, KEYWORD_REACH, SP_KEYWORD_SHADOW_HOSER, 0, 0, 0);
}

int card_aetherflame_wall(int player, int card, event_t event){
/* CARD_ID_AETHERFLAME_WALL	8481
Aetherflame Wall	|1|R
Creature - Wall 0/4
Defender
~ can block creatures with shadow as though they didn't have shadow.
|R: ~ gets +1/+0 until end of turn. */

	if( event == EVENT_ABILITIES ){
		special_abilities(player, card, event, SP_KEYWORD_SHADOW_HOSER, player, card);
	}

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1, 0);
}

int card_amrou_seekers(int player, int card, event_t event){
/* CARD_ID_AMROU_SEEKERS	8476
Amrou Seekers	|2|W
Creature - Kithkin Rebel 2/2
~ can't be blocked except by artifact creatures and/or |Swhite creatures. */

	if(event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card, AL_STATIC) ){
		if( (player == attacking_card_controller && card == attacking_card) ){
			if( ! is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) &&
				!( get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_WHITE)) )
			{
				event_result = 1;
			}
		}
	}

	return 0;
}

int card_ancestral_vision(int player, int card, event_t event)
{
  /* Ancestral Vision	""
   * Sorcery
   * Suspend 4-|U
   * Target player draws three cards. */

  suspend_only(player, card, event, 4, MANACOST_U(1));

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE );
  td.zone = TARGET_ZONE_PLAYERS;
  td.allow_cancel = 0;

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CAN_CAST)
	{
	  if (instance->info_slot && !played_for_free(player, card) && !is_token(player, card))
		return 1;

	  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

  if (event == EVENT_CAST_SPELL && affect_me(player, card)
	  && get_card_instance(player, card)->internal_card_id != -1	// Don't target if was just suspended
	  && can_target(&td))
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		draw_cards(instance->targets[0].player, 3);

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

static int angels_grace_legacy(int player, int card, event_t event){

	// Prevents damage from reducing life below 1
	card_ali_from_cairo(player, card, event);

	// Delays life loss from reducing life below 1 or poison counters from increasing above 9 until this leaves play; suppresses draws on empty deck
	cannot_lose_the_game(player, card, event, player);

	if( eot_trigger(player, card, event) ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_angels_grace(int player, int card, event_t event){
/* CARD_ID_ANGELS_GRACE	8479
Angel's Grace	|W
Instant
Split second (As long as this spell is on the stack, players can't cast spells or activate abilities that aren't mana abilities.)
You can't lose the game this turn and your opponents can't win the game this turn. Until end of turn,
damage that would reduce your life total to less than 1 reduces it to 1 instead. */

	cannot_be_countered(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_effect(player, card, &angels_grace_legacy);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[2].card = get_id(player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_ashcoat_bear(int player, int card, event_t event){
	/* Ashcoat Bear	|1|G
	 * Creature - Bear 2/2
	 * Flash (You may cast this spell any time you could cast an instant.) */
	/* Aven Reedstalker	|3|U
	 * Creature - Bird Warrior 2/3
	 * Flash
	 * Flying */
	/* Benalish Knight	|2|W
	 * Creature - Human Knight 2/2
	 * Flash (You may cast this spell any time you could cast an instant.)
	 * First strike (This creature deals combat damage before creatures without first strike.) */
	/* Cloaked Siren	|3|U
	 * Creature - Siren 3/2
	 * Flash (You may cast this spell any time you could cast an instant.)
	 * Flying */
	/* Havenwood Wurm	|6|G
	 * Creature - Wurm 5/6
	 * Flash (You may cast this spell any time you could cast an instant.)
	 * Trample */
	/* Hushwing Gryff	|2|W
	 * Creature - Hippogriff 2/1
	 * Flash
	 * Flying
	 * Creatures entering the battlefield don't cause abilities to trigger. */
	/* King Cheetah	|3|G
	 * Creature - Cat 3/2
	 * Flash */
	/* Nephalia Seakite	|3|U
	 * Creature - Bird 2/3
	 * Flash (You may cast this spell any time you could cast an instant.)
	 * Flying */
	/* Pouncing Cheetah	|2|G
	 * Creature - Cat 3/2
	 * Flash */
	/* Sentinels of Glen Elendra	|3|U
	 * Creature - Faerie Soldier 2/3
	 * Flash
	 * Flying */
	/* Skyline Predator	|4|U|U
	 * Creature - Drake 3/4
	 * Flash (You may cast this spell any time you could cast an instant.)
	 * Flying */
	/* Spire Monitor	|4|U
	 * Creature - Drake 3/3
	 * Flash (You may cast this spell any time you could cast an instant.)
	 * Flying */
	/* Stormrider Spirit	|4|U
	 * Creature - Spirit 3/3
	 * Flash (You may cast this spell any time you could cast an instant.)
	 * Flying */
	/* Swift Spinner	|3|G
	 * Creature - Spider 2/3
	 * Flash (You may cast this spell any time you could cast an instant.)
	 * Reach (This creature can block creatures with flying.) */
	/* Tangle Spider	|4|G|G
	 * Creature - Spider 3/4
	 * Flash (You may cast this spell any time you could cast an instant.)
	 * Reach (This creature can block creatures with flying.) */
	/* Wind Strider	|4|U
	 * Creature - Merfolk Wizard 3/3
	 * Flash
	 * Flying */

	// Hushwing Gryff's last ability handled elsewhere.
	return flash(player, card, event);
}

int card_aspect_of_the_mangoose(int player, int card, event_t event){
/* CARD_ID_ASPECT_OF_MONGOOSE	8480
Aspect of Mongoose	|1|G
Enchantment - Aura
Enchant creature
Enchanted creature has shroud. (It can't be the target of spells or abilities.)
When ~ is put into a graveyard from the battlefield, return ~ to its owner's hand. */

	immortal_enchantment(player, card, event);

	return generic_aura(player, card, event, player, 0, 0, KEYWORD_SHROUD, 0, 0, 0, 0 );
}

int card_assassinate(int player, int card, event_t event){
/* CARD_ID_ASSASSINATE	1618
Assassinate	|2|B
Sorcery
Destroy target tapped creature. */

	// Used externally
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target tapped creature.", 1, NULL);
}

int card_barbed_shocker(int player, int card, event_t event){
/*CARD_ID_BARBED_SHOCKER	8483
Barbed Shocker	|3|R
Creature - Insect 2/2
Trample, haste
Whenever ~ deals damage to a player, that player discards all the cards in his or her hand, then draws that many cards.*/

	haste(player, card, event);

	return card_shocker(player, card, event);
}

int card_basal_sliver(int player, int card, event_t event){
/* CARD_ID_BASAL_SLIVER	8484
Basal Sliver	|2|B
Creature - Sliver 2/2
All Slivers have "Sacrifice this permanent: Add |B|B to your mana pool." */

	if( ! IS_GAA_EVENT(event) && !(event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		return slivercycling(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Sliver to sacrifice.");
	test.subtype = SUBTYPE_SLIVER;

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( new_can_sacrifice_as_cost(player, card, &test) ){
			declare_mana_available(player, COLOR_BLACK, 2);
		}
	}

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) && new_can_sacrifice_as_cost(player, card, &test) ){
		return 1;
	}

	if(event == EVENT_ACTIVATE ){
		if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
			produce_mana(player, COLOR_BLACK, 2);
		}
		else{
			spell_fizzled = 1;
		}
	}

	return 0;
}

int card_benalish_cavalry(int player, int card, event_t event){
/* CARD_ID_BENALISH_CAVALRY	8486
Benalish Cavalry	|1|W
Creature - Human Knight 2/2
Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.) */

  flanking(player, card, event);
  return 0;
}

int card_bonesplitter_sliver(int player, int card, event_t event){
/* CARD_ID_BONESPLITTER_SLIVER	1381
Bonesplitter Sliver	|3|R
Creature - Sliver 2/2
All Sliver creatures get +2/+0. */

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 2, 0, 0, BCT_INCLUDE_SELF);
	return slivercycling(player, card, event);
}

int card_bogardan_hellkite(int player, int card, event_t event){
	/* Bogardan Hellkite	|6|R|R	0x200b3b2
	 * Creature - Dragon 5/5
	 * Flash
	 * Flying
	 * When ~ enters the battlefield, it deals 5 damage divided as you choose among any number of targets. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_and_divide_damage(player, card, NULL, NULL, 5);
		get_card_instance(player, card)->number_of_targets = 0;
	}

	return flash(player, card, event);
}

int card_brine_elemental(int player, int card, event_t event){
/* CARD_ID_BRINE_ELEMENTAL	8491
Brine Elemental	|4|U|U
Creature - Elemental 5/4
Morph |5|U|U (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, each opponent skips his or her next untap step. */

	if( event == EVENT_TURNED_FACE_UP ){
		target_player_skips_next_untap(player, card, 1-player);
	}
	return morph(player, card, event, MANACOST_XU(5, 2));
}

int card_call_to_the_netherworld(int player, int card, event_t event){
/* CARD_ID_CALL_TO_THE_NETHERWORLD	8493
Call to the Netherworld	|B
Sorcery
Return target |Sblack creature card from your graveyard to your hand.
Madness |0 (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

	if( event == EVENT_CAN_PAY_MADNESS_COST || event == EVENT_PAY_MADNESS_COST ){
		return madness(player, card, event, MANACOST0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, get_sleighted_color_text(player, card, "Select a %s creature card.", COLOR_BLACK));

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
}

int card_candles_of_leng(int player, int card, event_t event){
/* CARD_ID_CANDLES_OF_LENG	8494
Candles of Leng	|2
Artifact
|4, |T: Reveal the top card of your library. If it has the same name as a card in your graveyard, put it into your graveyard. Otherwise, draw a card. */

	if( event == EVENT_ACTIVATE ){
		int total = get_total_available_lands(player);
		ai_modifier += (24 + (total <= 3 || total >= 6 ? 24 : 0));
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int *deck = deck_ptr[player];
		show_deck( player, deck, 1, "Here's the first card of deck.", 0, gs_done_008b40e0 );
		if( is_id_in_grave(player, cards_data[deck[0]].id) ){
			mill(player, 1);
		}
		else{
			draw_cards(player, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL);
}

int card_careful_consideration(int player, int card, event_t event){
/* CARD_ID_CAREFUL_CONSIDERATION	8495
Careful Consideration	|2|U|U
Instant
Target player draws four cards, then discards three cards. If you cast this spell during your main phase, instead that player draws four cards, then discards two cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			int amount = current_turn == player && (current_phase == PHASE_MAIN1 || current_phase == PHASE_MAIN2) ? 3 : 2;
			ai_modifier_for_targeted_card_drawing(player, card, 0, amount);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(instance->targets[0].player, 4);
			int to_discard = 3;
			if( current_turn == player && (current_phase == PHASE_MAIN1 || current_phase == PHASE_MAIN2) ){
				to_discard = 2;
			}
			new_multidiscard(instance->targets[0].player, to_discard, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_cavalry_master(int player, int card, event_t event)
{
  /* CARD_ID_CAVALRY_MASTER	8497
   * Cavalry Master	|2|W|W
   * Creature - Human Knight 3/3
   * Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)
   * Other creatures you control with flanking have flanking. (Each instance of flanking triggers separately.) */

  int p;
  int humiliated;
  card_instance_t *instance;
  int creature_or_result;
  unsigned int card_00;
  int blocker_index;
  flanking(player, card, event);
  if ((((event == EVENT_DECLARE_BLOCKERS) &&
        (humiliated = is_humiliated(player, card, AL_STATIC), humiliated == 0)) &&
       (current_turn == player)) &&
      (card_00 = active_cards_count[player] - 1, -1 < (int)card_00))
  {
    p = 1 - player;
    do
    {
      instance = in_play(player, card_00);
      if (((instance != NULL) &&
           (creature_or_result = is_attacking(player, card_00), creature_or_result != 0)) &&
          ((creature_or_result = check_for_special_ability(player, card_00, 0x2000),
            creature_or_result != 0 &&
                ((creature_or_result = is_humiliated(player, card_00, AL_KEY),
                  creature_or_result == 0 &&
                      (blocker_index = active_cards_count[p] + -1, -1 < blocker_index))))))
      {
        do
        {
          instance = in_play(p, blocker_index);
          if ((instance != NULL) &&
              (((creature_or_result = is_what(p, blocker_index, 2),
                 creature_or_result != 0 && (instance = get_card_instance(p, blocker_index),
                                             instance->blocking == card_00)) &&
                (creature_or_result = check_for_special_ability(p, blocker_index, 0x2000),
                 creature_or_result == 0))))
          {
            pump_until_eot(player, card, p, blocker_index, -1, -1);
          }
          blocker_index = blocker_index + -1;
        } while (blocker_index != -1);
      }
      card_00 = card_00 - 1;
    } while (card_00 != 0xffffffff);
  }
  return 0;
}

int card_celestial_crusader(int player, int card, event_t event){
/* CARD_ID_CELESTIAL_CRUSADER	8498
Celestial Crusader	|2|W|W
Creature - Spirit 2/2
Flash (You may cast this spell any time you could cast an instant.)
Split second (As long as this spell is on the stack, players can't cast spells or activate abilities that aren't mana abilities.)
Flying
Other |Swhite creatures get +1/+1. */

	cannot_be_countered(player, card, event);

	boost_creature_by_color(player, card, event, COLOR_TEST_WHITE, 1, 1, 0, 0);

	return flash(player, card, event);
}

int card_chameleon_blur(int player, int card, event_t event){
  /* Chameleon Blur |3|G
   * Instant
   * Prevent all damage that creatures would deal to players this turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		fog_special(player, card, ANYBODY, FOG_ONLY_DAMAGE_TO_T_PLAYER);
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return card_fog(player, card, event);
}

int card_children_of_korlis(int player, int card, event_t event){
/* CARD_ID_CHILDREN_OF_KORLIS	8500
Children of Korlis	|W
Creature - Human Rebel Cleric 1/1
Sacrifice ~: You gain life equal to the life you've lost this turn. (Damage causes loss of life.) */

	if( event == EVENT_ACTIVATE ){
		int mod = life[player] < 6 ? 12 : 8;
		ai_modifier += (get_trap_condition(player, TRAP_LIFE_LOST) * mod);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, get_trap_condition(player, TRAP_LIFE_LOST));
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_chromatic_star(int player, int card, event_t event){
/* CARD_ID_CHROMATIC_STAR	8501
Chromatic Star	|1
Artifact
|1, |T, Sacrifice ~: Add one mana of any color to your mana pool.
When ~ is put into a graveyard from the battlefield, draw a card. */

	if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY)){ draw_a_card(player); }

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
	    }
	}
	return 0;
}

int card_chronatog_totem(int player, int card, event_t event){
/* CARD_ID_CHRONATOG_TOTEM	8502
Chronatog Totem	|3
Artifact
|T: Add |U to your mana pool.
|1|U: ~ becomes a 1/2 |Sblue Atog artifact creature until end of turn.
|0: ~ gets +3/+3 until end of turn. You skip your next turn. Activate this ability only once each turn and only if ~ is a creature. */

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_ANIMATE,
		CHOICE_PUMP_SKIP_TURN,
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_XU(1, 2), 0, NULL, NULL) ){
			return 1;
		}
		if( is_what(player, card, TYPE_CREATURE) ){
			if( generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST0, 0, NULL, NULL) ){
				return 1;
			}
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[4] = {0, mana_producer(player, card, EVENT_CAN_ACTIVATE), 0, 0};
		if( ! paying_mana() ){
			int fake_tap = 0;
			if( IS_AI(player) && ! is_tapped(player, card) ){
				add_state(player, card, STATE_TAPPED);
				fake_tap = 1;
			}
			abils[2] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_TYPE_CHANGE, MANACOST_XU(1, 1), 0, NULL, NULL);
			if( is_what(player, card, TYPE_CREATURE) ){
				abils[3] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_ONCE_PER_TURN, MANACOST0, 0, NULL, NULL);
			}
			if( IS_AI(player) && fake_tap){
				remove_state(player, card, STATE_TAPPED);
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Get mana", abils[1], 1,
							"Animate", abils[2], 5,
							"+3/+3 and skip next turn", abils[3], 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		int fake_tap = 0;
		if( IS_AI(player) && ! is_tapped(player, card) ){
			add_state(player, card, STATE_TAPPED);
			fake_tap = 1;
		}
		if( choice == CHOICE_ANIMATE ){
			return generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_XU(1, 1), 0, NULL, NULL);
		}
		if( choice == CHOICE_PUMP_SKIP_TURN ){
			return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST0, 0, NULL, NULL);
		}
		if( IS_AI(player) && fake_tap){
			remove_state(player, card, STATE_TAPPED);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_ANIMATE ){
			int legacy = artifact_animation(instance->parent_controller, instance->parent_card,
											instance->parent_controller, instance->parent_card, 1, 1, 2, 0, 0);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[6].player = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
			leg->targets[9].card = SUBTYPE_ATOG;
		}
		if( instance->info_slot == CHOICE_PUMP_SKIP_TURN){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 3, 3);
			skip_next_turn(instance->parent_controller, instance->parent_card, player);
		}
	}

	return 0;
}

int card_chronosavant(int player, int card, event_t event){
	/* Chronosavant	|5|W
	 * Creature - Giant 5/5
	 * |1|W: Return ~ from your graveyard to the battlefield tapped. You skip your next turn. */

	if( event == EVENT_GRAVEYARD_ABILITY && has_mana_multi(player, MANACOST_XW(1, 1))){
		return GA_RETURN_TO_PLAY_MODIFIED | GA_RETURN_TO_PLAY_WITH_EFFECT;
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana_multi(player, MANACOST_XW(1, 1));
		if( spell_fizzled != 1){
			return GAPAID_REMOVE;
		}
	}

	if( event == EVENT_RETURN_TO_PLAY_FROM_GRAVE_MODIFIED ){
		add_state(player, card, STATE_TAPPED);
	}

	if( event == EVENT_RESOLVE_ACTIVATED_GRAVEYARD_ABILITY ){
		skip_next_turn(player, card, player);
	}

	return 0;
}

static const char *target_must_be_permanent_or_suspend(int who_chooses, int player, int card,
                                               int targeting_player, int targeting_card)
{
  int result;
  card_instance_t *instance;

  result = is_what(player, card, 0x47);
  if (result == 0)
  {
    instance = get_card_instance(player, card);
    if (((instance->internal_card_id != LEGACY_EFFECT_CUSTOM) ||
         (instance->info_slot_fn != generic_suspend_legacy)) &&
        (cards_data[instance->internal_card_id].id != CARD_ID_GREATER_GARGADON_SUSPENDED))
    {
      return (char *)gs_target_error_type;
    }
  }
  return NULL;
}

static const char* target_must_be_permanent_or_suspend_and_have_counters(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  const char* perm_or_susp = target_must_be_permanent_or_suspend(who_chooses, player, card, targeting_player, targeting_card);
  if (perm_or_susp)
	return perm_or_susp;
  return count_counters(player, card, -1) ? NULL : "must have counters";
}

int card_clockspinning(int player, int card, event_t event)
{
  /* Clockspinning	|U
   * Instant
   * Buyback |3
   * Choose a counter on target permanent or suspended card. Remove that counter from that permanent or card or put another of those counters on it. */

  int t_player;
  int count_card_or_buyback;
  counter_t type;
  int counter_action;
  uint32_t remove_priority;
  card_instance_t *instance;
  target_definition_t td;
  char prompt[100];

  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      return 0;
  }
  else if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL &&
           event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 199);
  bool ai = ai_is_speculating == 1 || (player == 1 && (((unsigned char)trace_mode & 2) == 0));
  td.extra = (int32_t)(ai && event != EVENT_RESOLVE_SPELL
                           ? target_must_be_permanent_or_suspend_and_have_counters
                           : target_must_be_permanent_or_suspend);
  if (event == EVENT_CAN_CAST)
  {
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.preferred_controller = 2;
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
  }
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.preferred_controller = 2;
  if (event == EVENT_CAST_SPELL)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                       "Select target permanent or Suspend legacy.", 1, NULL),
         spell_fizzled != 1))
    {
      count_card_or_buyback = buyback(player, card, 3, 0, 0, 0, 0, 0);
      instance->info_slot = count_card_or_buyback;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td) != 0)
    {
      type = choose_existing_counter_type(player, player, card, instance->targets[0].player,
                                          instance->targets[0].card, CECT_ADD_OR_REMOVE, -1, -1);
      remove_priority = 1;
      if ((type & 0x200) != COUNTER_P1_P1)
      {
        type = type & 0xfffffdff;
        remove_priority = 9;
      }
      if (type != COUNTER_invalid)
      {
        count_card_or_buyback =
            count_counters(instance->targets[0].player, instance->targets[0].card, type);
        scnprintf(prompt, 100, "%s: %d", counter_names[type], count_card_or_buyback);
        counter_action =
            dialog_impl(player, card, EVENT_CAST_SPELL, DLG_FULLCARD(instance->targets[0].player,
                        instance->targets[0].card), DLG_HEADER(prompt), DLG_WHO_CHOOSES(player),
                        DLG_NO_STORAGE, DLG_RANDOM, DLG_NO_CANCEL, "Add counter", 1, 3, "Remove counter",
                        1, remove_priority, 0, 0, 0, 0);
        count_card_or_buyback = instance->targets[0].card;
        t_player = instance->targets[0].player;
        if (counter_action == 1)
        {
          add_counters(player, card, t_player, count_card_or_buyback, type, 1);
        }
        else
        {
          remove_counters(player, card, t_player, count_card_or_buyback, type, 1);
        }
      }
      if (instance->info_slot == 1)
      {
        bounce_permanent(player, card);
        return 0;
      }
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

int card_clockwork_hydra(int player, int card, event_t event)
{
	/* Clockwork Hydra	|5	0x2006f0b
	 * Artifact Creature - Hydra 0/0
	 * ~ enters the battlefield with four +1/+1 counters on it.
	 * Whenever ~ attacks or blocks, remove a +1/+1 counter from it. If you do, ~ deals 1 damage to any target.
	 * |T: Put a +1/+1 counter on ~. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 4);
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if ((event == EVENT_DAT_ABILITY || (blocking(player, card, event) && !is_humiliated(player, card, AL_TRIG))) && count_1_1_counters(player, card) > 0){
	    remove_counters(player, card, player, card, COUNTER_P1_P1, 1);
	    target_definition_t td;
	    default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	    td.allow_cancel = 0;
	    card_instance_t* instance = get_card_instance(player, card);
	    if (can_target(&td) && pick_target(&td, "TARGET_ANY")){ damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card); }
	    instance->number_of_targets = 0;
	}
	if (event == EVENT_RESOLVE_ACTIVATION){
	    card_instance_t* instance = get_card_instance(player, card);
	    add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 1);
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_cloudchaser_kestrel(int player, int card, event_t event){
/* CARD_ID_CLOUDCHASER_KESTREL	8506
Cloudchaser Kestrel	|1|W|W
Creature - Bird 2/2
Flying
When ~ enters the battlefield, destroy target enchantment.
|W: Target permanent becomes |Swhite until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td_enchantment;
		default_target_definition(player, card, &td_enchantment, TYPE_ENCHANTMENT);
		td_enchantment.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if (can_target(&td_enchantment) && pick_target(&td_enchantment, "TARGET_ENCHANTMENT")){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td_permanent;
	default_target_definition(player, card, &td_permanent, TYPE_PERMANENT);

	if (event == EVENT_RESOLVE_ACTIVATION){
		if(valid_target(&td_permanent)){
			card_instance_t *instance = get_card_instance(player, card);
			change_color(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
						COLOR_TEST_WHITE, CHANGE_COLOR_SET|CHANGE_COLOR_END_AT_EOT);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_W(1), 0, &td_permanent, "TARGET_PERMANENT");
}

int card_coal_stoker(int player, int card, event_t event){
/* CARD_ID_COAL_STOKER	8507
Coal Stoker	|3|R
Creature - Elemental 3/3
When ~ enters the battlefield, if you cast it from your hand, add |R|R|R to your mana pool. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->info_slot = not_played_from_hand(player, card);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( get_card_instance(player, card)->info_slot != 1 ){
			produce_mana(player, COLOR_RED, 3);
		}
	}

	return 0;
}

int card_conflagrate(int player, int card, event_t event){
	/* Conflagrate	|X|X|R	0x2006f1a
	 * Sorcery
	 * ~ deals X damage divided as you choose among any number of targets.
	 * Flashback-|R|R, Discard X cards. */
	if( ! IS_GS_EVENT(player, card, event) && event != EVENT_GRAVEYARD_ABILITY && event != EVENT_PAY_FLASHBACK_COSTS ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td.allow_cancel = 3;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, NULL, 1, NULL);
	}

	if( (event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CHANGE_TARGET ){
		instance->number_of_targets = 0;
		if( (played_for_free(player, card) || is_token(player, card)) && ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) && ! get_flashback(player, card) ){
			return 0;
		}
		int trgs = 0;
		int targs[10][2];
		while( trgs < 10 ){
				if( get_flashback(player, card) && trgs+1 > hand_count[player] ){
					break;
				}
				if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) && !get_flashback(player, card) && ! has_mana(player, COLOR_COLORLESS, ((trgs+1)*2)) ){
					break;
				}
				if( trgs >= instance->info_slot && check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
					break;
				}
				if( new_pick_target(&td, "TARGET_ANY", trgs, 0) ){
					if( instance->targets[trgs].card != -1 ){
						add_state(instance->targets[trgs].player, instance->targets[trgs].card, STATE_TARGETTED);
					}
					targs[trgs][0] = instance->targets[trgs].player;
					targs[trgs][1] = instance->targets[trgs].card;
					trgs++;
				}
				else{
					break;
				}
		}
		if( get_flashback(player, card) ){
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
			while( ohc && tdc < instance->number_of_targets ){
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
			if( tdc == instance->number_of_targets ){
				for(i=0; i<tdc; i++){
					discard_card(player, to_discard[i]);
				}
				instance->info_slot = tdc;
			}
		}
		else{
			if( ! is_token(player, card) ){
				charge_mana(player, COLOR_COLORLESS, instance->number_of_targets * 2);
				if( spell_fizzled != 1 ){
					instance->info_slot = instance->number_of_targets;
				}
			}
		}
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( instance->targets[i].card != -1 ){
				remove_state(instance->targets[i].player, instance->targets[i].card, STATE_TARGETTED);
			}
			instance->targets[i].player = targs[i][0];
			instance->targets[i].card = targs[i][1];
		}
	}

	if(event == EVENT_RESOLVE_SPELL){
		divide_damage(player, card, &td);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	if( event == EVENT_GRAVEYARD_ABILITY ){
		if( can_target(&td) && hand_count[player] > 0 ){
			return do_flashback(player, card, event, MANACOST_R(2));
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_R(2));
	}

	return 0;
}

int card_coral_trickster(int player, int card, event_t event){
/* CARD_ID_CORAL_TRICKSTER	8509
Coral Trickster	|1|U
Creature - Merfolk Rogue 2/1
Morph |U (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, you may tap or untap target permanent. */

	if( event == EVENT_TURNED_FACE_UP ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
			instance->number_of_targets = 1;
			if( is_tapped(instance->targets[0].player, instance->targets[0].card) ){
				untap_card(instance->targets[0].player, instance->targets[0].card);
			}
			else{
				tap_card(instance->targets[0].player, instance->targets[0].card);
			}
		}
		instance->number_of_targets = 0;
	}

	return morph(player, card, event, MANACOST_U(1));
}

/* ManalinkEh 2022: 0x20ee6c0; original cards/time_spiral.c:1372. */
int card_corpulent_corpse(int player, int card, event_t event)
{
  /* Corpulent Corpse	|5|B
   * Creature - Zombie 3/3
   * Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
   * Suspend 5-|B (Rather than cast this card from your hand, you may pay |B and exile it with five time counters on it. At the beginning of your upkeep, remove a time counter. When the last is removed, you may cast it without paying its mana cost. It has haste.) */

  int result;
  fear(player, card, event);
  result = suspend(player, card, event, 5, 0, 1, 0, 0, 0, 0, NULL, NULL);
  return result;
}

int card_crookclaw_transmuter(int player, int card, event_t event){
/* CARD_ID_CROOKCLAW_TRANSMUTER	8511
Crookclaw Transmuter	|3|U
Creature - Bird Wizard 3/1
Flash (You may cast this spell any time you could cast an instant.)
Flying
When ~ enters the battlefield, switch target creature's power and toughness until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.preferred_controller = ANYBODY;
	  td.allow_cancel = 0;

	  card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_CREATURE"))
			switch_power_and_toughness_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
		instance->number_of_targets = 0;
	}

  // Flash
  return flash(player, card, event);
}

int card_curse_of_the_cabal(int player, int card, event_t event){
	/* Curse of the Cabal	|9|B
	 * Sorcery
	 * Target player sacrifices half the permanents he or she controls, rounded down.
	 * Suspend 2-|2|B|B
	 * At the beginning of each player's upkeep, if ~ is suspended, that player may sacrifice a permanent. If he or she does, put two time counters on ~. */

	if( ! IS_GS_EVENT(player, card, event) && event != EVENT_MODIFY_COST ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);
			int amount = count_subtype(instance->targets[0].player, TYPE_PERMANENT, -1);
			amount/=2;

			test_definition_t test;
			new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent to sacrifice.");
			test.qty = amount;
			new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED | SAC_NO_CANCEL, &test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return suspend(player, card, event, 2, MANACOST_XB(2, 2), &td, "TARGET_PLAYER");
}

int card_dark_withering(int player, int card, event_t event){
/* CARD_ID_DARK_WITHERING	8515
Dark Withering	|4|B|B
Instant
Destroy target non|Sblack creature.
Madness |B (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

	if( event == EVENT_CAN_PAY_MADNESS_COST || event == EVENT_PAY_MADNESS_COST ){
		return madness(player, card, event, MANACOST_B(1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target target non%s creature.", COLOR_BLACK), 1, NULL);
}

int card_deathspore_thallid(int player, int card, event_t event){

	/* Deathspore Thallid	|1|B
	 * Creature - Zombie Fungus 1/1
	 * At the beginning of your upkeep, put a spore counter on ~.
	 * Remove three spore counters from ~: Put a 1/1 |Sgreen Saproling creature token onto the battlefield.
	 * Sacrifice a Saproling: Target creature gets -1/-1 until end of turn. */

	add_spore_counters(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_SAPROLING = 1,
		CHOICE_WEAKEN
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Saproling to sacrifice.");
	test.subtype = SUBTYPE_SAPROLING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_make_saproling_from_fungus(player, card) ){
			return 1;
		}
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL, 0, &test, 0);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int abils[3] = {
						0,
						can_make_saproling_from_fungus(player, card),
						generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST0, 0,
																		&td, NULL, 0, &test, 0)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Generate a Saproling", abils[1], 5,
							"Sac & give -1/-1", abils[2], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_SAPROLING ){
			saproling_from_fungus(player, card);
		}
		if( choice == CHOICE_WEAKEN ){
			return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE",
																	0, &test, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAPROLING ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
		}
		if( instance->info_slot == CHOICE_WEAKEN && valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							-1, -1);
		}
	}


	return 0;
}

int card_deep_sea_kraken(int player, int card, event_t event){
	/* Deep-Sea Kraken	|7|U|U|U
	 * Creature - Kraken 6/6
	 * ~ is unblockable.
	 * Suspend 9-|2|U
	 * Whenever an opponent casts a spell, if ~ is suspended, remove a time counter from it. */
	unblockable(player, card, event);
	return suspend(player, card, event, 9, MANACOST_XU(2, 1), NULL, NULL);
}

int card_demonic_collusion(int player, int card, event_t event){
/* CARD_ID_DEMONIC_COLLUSION	8519
Demonic Collusion	|3|B|B
Sorcery
Buyback-Discard two cards. (You may discard two cards in addition to any other costs as you cast this spell.
If you do, put this card into your hand as it resolves.)
Search your library for a card and put that card into your hand. Then shuffle your library. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_STORAGE,
							"Play normally", 1, 1,
							"Buyback", hand_count[player] >= 2, 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == 2 ){
			multidiscard(player, 2, 0);
			get_card_instance( player, card )->info_slot = 1;
		}
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
		if( get_card_instance( player, card )->info_slot == 1 ){
			bounce_permanent(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_draining_whelk(int player, int card, event_t event){
/* CARD_ID_DRAINING_WHELK	8522
Draining Whelk	|4|U|U
Creature - Illusion 1/1
Flash (You may cast this spell any time you could cast an instant.)
Flying
When ~ enters the battlefield, counter target spell. Put X +1/+1 counters on ~, where X is that spell's converted mana cost. */

	if( event == EVENT_CAN_CAST ){
		if( card_counterspell(player, card, EVENT_CAN_CAST) ){
			get_card_instance( player, card )->info_slot = 1;
			return 0x63;
		}
		else{
			get_card_instance( player, card )->info_slot = 0;
			return 1;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier -= (current_turn == player ? 10 : 0);
		return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if (event == EVENT_ETB){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->info_slot == 1 ){
			int cmc = get_cmc(instance->targets[0].player, instance->targets[0].card);
			if( counterspell_validate(player, card, NULL, 0) ){
				real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			}
			add_counters(player, card, player, card, COUNTER_P1_P1, cmc);
		}
	}
	return 0;
}

int card_dralnu_lich_lord(int player, int card, event_t event ){

	/* Dralnu, Lich Lord	|3|U|B
	 * Legendary Creature - Zombie Wizard 3/3
	 * If damage would be dealt to ~, sacrifice that many permanents instead.
	 * |T: Target instant or sorcery card in your graveyard gains flashback until end of turn. The flashback cost is equal to its mana cost. */

	check_legend_rule(player, card, event);

	if (IS_ACTIVATING(event)){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_SPELL, "Select target instant or sorcery card.");

		if( event == EVENT_CAN_ACTIVATE ){
			if( generic_activated_ability(player, card, event, GAA_UNTAPPED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL) ){
				if( new_special_count_grave(player, &this_test) && ! graveyard_has_shroud(2) ){
					return 1;
				}
			}
		}

		if( event == EVENT_ACTIVATE ){
			if( new_select_target_from_grave(player, card, player, 0, AI_MAX_VALUE, &this_test, 0) != -1 ){
				tap_card(player, card);
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			int selected = validate_target_from_grave(player, card, player, 0);
			create_spell_has_flashback_legacy(player, card, selected, 0);
		}
	}

	if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG)){
		get_card_instance(player, card)->destroys_if_blocked |= DIFB_ASK_CARD;
	}

	if (event == EVENT_CHECK_DESTROY_IF_BLOCKED && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG)
		&& get_power(attacking_card_controller, attacking_card) >= 1){
		event_result |= 2;
	}


	if( !is_humiliated(player, card, AL_TRIG) ){
		card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->damage_target_player == player && damage->damage_target_card == card){
			if( damage->info_slot > 0 ){
				int amount = damage->info_slot;
				prevent_damage(affected_card_controller, affected_card, amount);
				test_definition_t test;
				new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent to sacrifice.");
				test.qty = amount;
				new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test);
			}
		}
	}

	return 0;
}

int card_dread_return(int player, int card, event_t event){
	/* Dread Return	|2|B|B
	 * Sorcery
	 * Return target creature card from your graveyard to the battlefield.
	 * Flashback-Sacrifice three creatures. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_GRAVEYARD_ABILITY ){
			// can only play as sorcery
		if( can_sorcery_be_played(player, event) ){
			if( can_pay_flashback(player, instance->internal_card_id, event, MANACOST0) ){
				test_definition_t this_test;
		        default_test_definition(&this_test, TYPE_CREATURE);
				this_test.qty = 3;
				if( new_can_sacrifice_as_cost(player, card, &this_test) && any_in_graveyard_by_type(player, TYPE_CREATURE) ){
					return GA_PLAYABLE_FROM_GRAVE;
				}
			}
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		if( pay_flashback(player, instance->internal_card_id, event, MANACOST0) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			test.qty = 3;
			if( !  new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
				spell_fizzled = 1;
				return 0;
			}
			return GAPAID_EXILE;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card");

	if(event == EVENT_RESOLVE_SPELL){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
		}

		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 0, &this_test);
}

int card_dream_stalker(int player, int card, event_t event){
/* CARD_ID_DREAM_STALKER	8526
Dream Stalker	|1|U
Creature - Illusion 1/5
When ~ enters the battlefield, return a permanent you control to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_PERMANENT);
		td1.allowed_controller = player;
		td1.preferred_controller = player;
		td1.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td1) ){
			if( pick_target(&td1, "TARGET_PERMANENT") ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_drifter_il_dal(int player, int card, event_t event){
/* CARD_ID_DRIFTER_IL_DAL	8527
Drifter il-Dal	|U
Creature - Human Wizard 2/1
Shadow (This creature can block or be blocked by only creatures with shadow.)
At the beginning of your upkeep, sacrifice ~ unless you pay |U. */

	shadow(player, card, event);
	basic_upkeep(player, card, event, MANACOST_U(1));
	return 0;
}

int card_durkwood_baloth(int player, int card, event_t event){
	/* Durkwood Baloth	|4|G|G
	 * Creature - Beast 5/5
	 * Suspend 5-|G */
	return suspend(player, card, event, 5, MANACOST_G(1), NULL, NULL);
}

int card_duskrider_peregrine(int player, int card, event_t event){
	/* Duskrider Peregrine	|5|W
	 * Creature - Bird 3/3
	 * Flying, protection from |Sblack
	 * Suspend 3-|1|W */
	return suspend(player, card, event, 3, MANACOST_XW(1,1), NULL, NULL);
}

int card_empty_the_warrens(int player, int card, event_t event){

	/* Empty the Warrens	|3|R
	 * Sorcery
	 * Put two 1/1 |Sred Goblin creature tokens onto the battlefield.
	 * Storm */

	if (event == EVENT_QUERY && attacking_card == 4){
		return 1;
	}

	if(event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_GOBLIN, &token);
		token.pow = token.tou = 1;
		token.color_forced = COLOR_TEST_RED;
		token.qty = 2;
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_endrek_sahr_master_breeder(int player, int card, event_t event){
/* CARD_ID_ENDREK_SAHR_MASTER_BREEDER	8533
Endrek Sahr, Master Breeder	|4|B
Legendary Creature - Human Wizard 2/2
Whenever you cast a creature spell, create X 1/1 |Sblack Thrull creature tokens, where X is that spell's converted mana cost.
When you control seven or more Thrulls, sacrifice ~. */

	check_legend_rule(player, card, event);
	sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);
	if (event == EVENT_SPA){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_THRULL, &token);
		token.pow = token.tou = 1;
		token.qty = get_cmc(trigger_cause_controller, trigger_cause);
		generate_token(&token);
	}
	else if (event == EVENT_STATIC_EFFECTS && !is_humiliated(player, card, AL_STATIC) && count_subtype(player, TYPE_PERMANENT, SUBTYPE_THRULL) > 6){
		kill_card(player, card, KILL_SACRIFICE);
	}
	return 0;
}

int card_errant_doomsayer(int player, int card, event_t event){
/* CARD_ID_ERRANT_DOOMSAYERS	8534
Errant Doomsayers	|1|W
Creature - Human Rebel 1/1
|T: Tap target creature with toughness 2 or less. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.toughness_requirement = 2 | TARGET_PT_LESSER_OR_EQUAL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target creature with POW 2 or less.");
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

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0, &td, NULL);
}

int card_errant_ephemeron(int player, int card, event_t event){
	/* Errant Ephemeron	|6|U
	 * Creature - Illusion 4/4
	 * Flying
	 * Suspend 4-|1|U */
	return suspend(player, card, event, 4, MANACOST_XU(1, 1), NULL, NULL);
}

int card_eternity_snare(int player, int card, event_t event)
{
  /* Eternity Snare |5|U
   * Enchantment - Aura
   * Enchant creature
   * When ~ enters the battlefield, draw a card.
   * Enchanted creature doesn't untap during its controller's untap step. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
		draw_cards(player, 1);

  card_instance_t* instance = get_card_instance(player, card);
  int p = instance->damage_target_player, c = instance->damage_target_card;
  if (p >= 0 && c >= 0 && ! is_humiliated(player, card, AL_STATIC) )
	does_not_untap(p, c, event);

  if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

  return disabling_aura(player, card, event);
}

int card_evangelize(int player, int card, event_t event){
/* CARD_ID_EVANGELIZE	8537
Evangelize	|4|W
Sorcery
Buyback |2|W|W (You may pay an additional |2|W|W as you cast this spell. If you do, put this card into your hand as it resolves.)
Gain control of target creature of an opponent's choice he or she controls. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = td.who_chooses = 1 - player;
	td.allow_cancel = 0;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature you control.", 1, NULL);
		if( spell_fizzled != 1 ){
			instance->info_slot = buyback(player, card, MANACOST_XW(2, 2));
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

/* ManalinkEh 2022: 0x20f0580; original cards/time_spiral.c:1974. */
int card_fallen_ideal(int player, int card, event_t event)
{
  /* Fallen Ideal	|2|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has flying and "Sacrifice a creature: This creature gets +2/+1 until end of turn."
   * When this Aura is put into a graveyard from the battlefield, return it to its owner's hand. */

  card_instance_t *instance;
  int available_or_result;
  int sacrifice_available;
  int attached_card;
  unsigned int chosen_sacrifice;
  unsigned int player_00;
  int granted_to_player;
  test_definition_t test;

  immortal_enchantment(player, card, event);
  instance = get_card_instance(player, card);
  granted_to_player = (int)instance->damage_target_player;
  attached_card = instance->damage_target_card;
  if (event == EVENT_CAN_ACTIVATE)
  {
    sacrifice_available = can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE);
    available_or_result = 0;
    if (sacrifice_available != 0)
    {
      available_or_result =
          granted_generic_activated_ability(player, card, granted_to_player, attached_card,
                                            EVENT_CAN_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
    }
  }
  else
  {
    if (event == EVENT_ACTIVATE)
    {
      granted_generic_activated_ability(player, card, granted_to_player, attached_card,
                                        EVENT_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
      new_default_test_definition(&test, 2, "Select creature to sacrifice.");
      chosen_sacrifice = new_sacrifice(granted_to_player, attached_card, player,
                                       SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
      if (chosen_sacrifice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      player_00 = (int)chosen_sacrifice >> 0x10 & 0xff;
      state_untargettable(player_00, chosen_sacrifice >> 0x18, 0);
      if (spell_fizzled != 1)
      {
        kill_card(player_00, chosen_sacrifice >> 0x18, KILL_SACRIFICE);
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      return pump_until_eot_merge_previous(granted_to_player, attached_card, granted_to_player,
                                           attached_card, 2, 1);
    }
    available_or_result = generic_aura(player, card, event, player, 0, 0, 0x20, 0, 0, 0, 0);
  }
  return available_or_result;
}

int card_fathom_seer(int player, int card, event_t event)
{
/* CARD_ID_FATHOM_SEER	8541
Fathom Seer	|1|U
Creature - Illusion 1/3
Morph-Return two |H1Islands you control to their owner's hand. (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, draw two cards. */

  if (event == EVENT_CAN_UNMORPH)
	return count_subtype(player, TYPE_LAND, SUBTYPE_ISLAND) >= 2;

  if (event == EVENT_UNMORPH)
	{
	  target_definition_t td;
	  base_target_definition(player, card, &td, TYPE_LAND);
	  td.allowed_controller = player;
	  td.preferred_controller = player;
	  td.required_subtype = SUBTYPE_ISLAND;

	  if (pick_up_to_n_targets_noload(&td, "Select an Island you control.", 2) == 2)
		{
		  card_instance_t* instance = get_card_instance(player, card);

		  bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		  if (in_play(instance->targets[1].player, instance->targets[1].card))	// Some stupid Manalink non-stack-using trigger might've removed it
			bounce_permanent(instance->targets[1].player, instance->targets[1].card);

		  instance->number_of_targets = 0;
		}
	  else
		cancel = 1;

	  return 0;
	}

  // When ~ is turned face up, draw two cards.
  if (event == EVENT_TURNED_FACE_UP)
	draw_cards(player, 2);

  return morph(player, card, event, MANACOST_X(0));
}

int card_fertile_ground(int player, int card, event_t event)
{
  /* Fertile Ground	|1|G
   * Enchantment - Aura
   * Enchant land
   * Whenever enchanted land is tapped for mana, its controller adds one mana of any color to his or her mana pool. */
  return wild_growth_aura_all_one_color(player, card, event, -1, COLOR_TEST_ANY_COLORED, 1);
}

int firewake_sliver_shared_ability(int player, int card, event_t event)
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
  default_target_definition(player, card, &td2, 2);
  td2.required_subtype = 0xb1;
  td2.preferred_controller = player;
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = would_validate_arbitrary_target(&td2, instance->targets[2].player,
                                                instance->targets[2].card),
       result != 0))
  {
    pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[2].player,
                   instance->targets[2].card, 2, 2);
  }
  return shared_sliver_activated_ability(player, card, event, 0xe, 1, 0, 0, 0, 0, 0, 0, &td2,
                                         "Select target Sliver to pump.");
}

int card_firewake_sliver(int player, int card, event_t event){
  /* Firewake Sliver |1|R|G
   * 	  Creature - Sliver 1/1
   * 	  All Sliver creatures have haste.
   * 	  All Slivers have "{1}, Sacrifice this permanent: Target Sliver creature gets +2/+2 until end of turn." */

	// All Sliver creatures have haste.
	boost_subtype(player, card, event, SUBTYPE_SLIVER, 0,0, 0,SP_KEYWORD_HASTE, BCT_INCLUDE_SELF);

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &firewake_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &firewake_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int card_flagstones_of_trokair(int player, int card, event_t event){
/* CARD_ID_FLAGSTONES_OF_TROKAIR	8545
Flagstones of Trokair	""
Legendary Land
|T: Add |W to your mana pool.
When ~ is put into a graveyard from the battlefield, you may search your library for |Ha Plains card and put it onto the battlefield tapped.
If you do, shuffle your library. */

	check_legend_rule(player, card, event);

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) && !is_humiliated(player, card, AL_TRIG) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.subtype = get_hacked_subtype(player, card, SUBTYPE_PLAINS);
		scnprintf(test.message, 100, "%s", get_hacked_land_text(player, card, "Select a %s card.", SUBTYPE_PLAINS));
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY_TAPPED, 0, AI_MAX_VALUE, &test);
	}

	return plain_mana_producer(player, card, event);
}

int card_fledgling_mawcor(int player, int card, event_t event){
	/* Fledgling Mawcor	|3|U	0x2006f6f
	 * Creature - Beast 2/2
	 * Flying
	 * |T: ~ deals 1 damage to any target.
	 * Morph |U|U */
	if( event == EVENT_TURNED_FACE_UP ){
		get_card_instance(player, card)->targets[1].player = 66;
	}

	if( get_card_instance(player, card)->targets[1].player == 66 ){
		return card_prodigal_sorcerer(player, card, event);
	}

	return morph(player, card, event, MANACOST_U(2));
}

int card_flickering_spirit(int player, int card, event_t event){
/* CARD_ID_FLICKERING_SPIRIT	8548
Flickering Spirit	|3|W
Creature - Spirit 2/2
Flying
|3|W: Exile ~, then return it to the battlefield under its owner's control. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(instance->parent_controller, instance->parent_card) ){
			blink_effect(instance->parent_controller, instance->parent_card, 0);
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XW(3, 1), 0, NULL, NULL);
}

int card_flowstone_channeler(int player, int card, event_t event){
/* CARD_ID_FLOWSTONE_CHANNELER	8549
Flowstone Channeler	|2|R
Creature - Human Spellshaper 2/2
|1|R, |T, Discard a card: Target creature gets +1/-1 and gains haste until end of turn. */

	if (event == EVENT_CHECK_ABILITIES){
		if (card_flowstone_channeler(player, card, EVENT_CAN_ACTIVATE)){
			if (affected_card_controller == player){ ++check_abilities_power; }
			if (affected_card_controller == 1-player){ --check_abilities_toughness; }
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
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 1, -1, 0, SP_KEYWORD_HASTE);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD | GAA_CAN_TARGET, MANACOST_XR(1, 1), 0,
									&td, "TARGET_CREATURE");
}

int card_foriysian_interceptor(int player, int card, event_t event)
{
/* CARD_ID_FORIYSIAN_INTERCEPTOR	8551
Foriysian Interceptor	|3|W
Creature - Human Soldier 0/5
Flash (You may cast this spell any time you could cast an instant.)
Defender
~ can block an additional creature each combat. */

  creature_can_block_additional(player, card, event, 1);
  return flash(player, card, event);
}

int card_foriysian_totem(int player, int card, event_t event){
/* CARD_ID_FORIYSIAN_TOTEM	8552
Foriysian Totem	|3
Artifact
|T: Add |R to your mana pool.
|4|R: ~ becomes a 4/4 |Sred Giant artifact creature with trample until end of turn.
As long as ~ is a creature, it can block an additional creature each combat. */

	if( is_what(player, card, TYPE_CREATURE) && ! is_humiliated(player, card, AL_STATIC) ){
		creature_can_block_additional(player, card, event, 1);
	}

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_ANIMATE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_XR(4, 2), 0, NULL, NULL) ){
			return 1;
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, mana_producer(player, card, EVENT_CAN_ACTIVATE), 0};
		if( ! paying_mana() ){
			int fake_tap = 0;
			if( IS_AI(player) && ! is_tapped(player, card) ){
				add_state(player, card, STATE_TAPPED);
				fake_tap = 1;
			}
			abils[2] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_TYPE_CHANGE, MANACOST_XR(4, 1), 0, NULL, NULL);
			if( IS_AI(player) && fake_tap){
				remove_state(player, card, STATE_TAPPED);
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Get mana", abils[1], 1,
							"Animate", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		int fake_tap = 0;
		if( IS_AI(player) && ! is_tapped(player, card) ){
			add_state(player, card, STATE_TAPPED);
			fake_tap = 1;
		}
		if( choice == CHOICE_ANIMATE ){
			return generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_XR(4, 1), 0, NULL, NULL);
		}
		if( IS_AI(player) && fake_tap){
			remove_state(player, card, STATE_TAPPED);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_ANIMATE ){
			int legacy = artifact_animation(instance->parent_controller, instance->parent_card,
											instance->parent_controller, instance->parent_card, 1, 4, 4, 0, 0);
			card_instance_t *leg = get_card_instance(instance->parent_controller, legacy);
			int color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
			leg->targets[6].player = color;
			leg->targets[9].card = SUBTYPE_GIANT;
			get_card_instance(instance->parent_controller, instance->parent_card)->targets[1].player = 66;
		}
	}
	return 0;
}

int card_fortify(int player, int card, event_t event){
/* CARD_ID_FORTIFY	8553
Fortify	|2|W
Instant
Choose one -
* Creatures you control get +2/+0 until end of turn.
* Creatures you control get +0/+2 until end of turn. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"+2/+0 to your crits", 1, current_turn == player ? 10 : 1,
								"+0/+2 to your crits", 1, current_turn != player ? 10 : 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		return 0;
	}


	if( event == EVENT_RESOLVE_SPELL ){
		int pp = 2;
		int tp = 0;
		if( get_card_instance(player, card)->info_slot == 2 ){
			pp = 0;
			tp = 2;
		}
		pump_subtype_until_eot(player, card, player, -1, pp, tp, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_fortune_thief(int player, int card, event_t event){
/* CARD_ID_FORTUNE_THIEF	8554
Fortune Thief	|4|R
Creature - Human Rogue 0/1
Damage that would reduce your life total to less than 1 reduces it to 1 instead.
Morph |R|R (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	card_ali_from_cairo(player, card, event);
	return morph(player, card, event, MANACOST_R(2));
}

static int effect_fungus_sliver(int player, int card, event_t event)
{
  card_instance_t *instance;
  card_instance_t *attached_in_play;
  if (event == EVENT_AFTER_DAMAGE)
  {
    instance = get_card_instance(player, card);
    attached_in_play = in_play((int)instance->damage_target_player, instance->damage_target_card);
    if (attached_in_play != NULL)
    {
      add_counters(player, card, (int)instance->damage_target_player, instance->damage_target_card,
                   COUNTER_P1_P1, 1);
    }
  }
  else if (((event != EVENT_CLEANUP) || (card != affected_card)) ||
           (player != affected_card_controller))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

int card_fungus_sliver(int player, int card, event_t event)
{
  /* CARD_ID_FUNGUS_SLIVER	8556
   * Fungus Sliver	|3|G
   * Creature - Fungus Sliver 2/2
   * All Sliver creatures have "Whenever this creature is dealt damage, put a +1/+1 counter on it." (The damage is dealt before the counter is put on.) */

  int result;
  card_instance_t *damage;
  card_instance_t *instance;
  int p;

  if ((((event == EVENT_DEAL_DAMAGE) &&
        (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
       (damage = get_card_instance(affected_card_controller, affected_card),
        damage->internal_card_id == damage_card)) &&
      (((damage->damage_target_card != -1 &&
         (result = is_what((int)damage->damage_target_player, damage->damage_target_card, 2),
          result != 0)) &&
        ((result = has_subtype((int)damage->damage_target_player, damage->damage_target_card,
                               SUBTYPE_SLIVER),
          result != 0 && (result = is_humiliated((int)damage->damage_target_player,
                                                 damage->damage_target_card, AL_TRIG),
                          result == 0))))))
  {
    p = 0;
    if ((damage->info_slot < 1) && ((damage->targets[0x10].card & 0x4000) == 0))
    {
      if ((damage->regen_status & 0x20000000) == 0)
        return slivercycling(player, card, event);
      p = 0;
    }
    do
    {
      result = 0;
      if (0 < active_cards_count[player])
      {
        do
        {
          instance = get_card_instance(p, result);
          if (((((instance->damage_source_card == card) &&
                 (instance->damage_source_player == player)) &&
                (instance->damage_target_card == damage->damage_target_card)) &&
               ((instance->damage_target_player == damage->damage_target_player &&
                 (instance->internal_card_id == LEGACY_EFFECT_CUSTOM)))) &&
              (instance->info_slot_fn == effect_fungus_sliver))
          {
            return 0;
          }
          result = result + 1;
        } while (result < active_cards_count[player]);
      }
      p = p + 1;
    } while (p != 2);
    create_targetted_legacy_effect(player, card, effect_fungus_sliver,
                                   (int)damage->damage_target_player, damage->damage_target_card);
  }
  return slivercycling(player, card, event);
}

int card_fury_sliver(int player, int card, event_t event){
/* CARD_ID_FURY_SLIVER	8557
Fury Sliver	|5|R
Creature - Sliver 3/3
All Sliver creatures have double strike. */

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 0, 0, KEYWORD_DOUBLE_STRIKE, BCT_INCLUDE_SELF);
	return slivercycling(player, card, event);
}

int card_gauntlet_of_power(int player, int card, event_t event)
{
  /* Gauntlet of Power	|5
   * Artifact
   * As ~ enters the battlefield, choose a color.
   * Creatures of the chosen color get +1/+1.
   * Whenever a basic land is tapped for mana of the chosen color, its controller adds one mana of that color to his or her mana pool. */

  card_instance_t *instance;
  int result;
  subtype_t subtype;

  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    result = choose_a_color_and_show_legacy(player, card, player, -1);
    instance->info_slot = result;
    return 0;
  }
  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    instance = in_play(player, card);
    if ((instance != NULL) &&
        (result = is_humiliated(player, card, AL_STATIC), result == 0))
    {
      result = get_color(affected_card_controller, affected_card);
      instance = get_card_instance(player, card);
      if ((result >> ((unsigned char)instance->info_slot & 0x1f) & 1U) != 0)
      {
        event_result = event_result + 1;
      }
    }
  }
  else if (((((uint32_t)(event - EVENT_COUNT_MANA) & 0xfffffffd) == 0) &&
            (instance = in_play(player, card), instance != NULL)) &&
           (result = is_humiliated(player, card, AL_TRIG), result == 0))
  {
    instance = get_card_instance(player, card);
    result = instance->info_slot;
    if (result == 1)
    {
      subtype = SUBTYPE_SWAMP;
    }
    else if (result == 2)
    {
      subtype = SUBTYPE_ISLAND;
    }
    else if (result == 3)
    {
      subtype = SUBTYPE_FOREST;
    }
    else if (result == 4)
    {
      subtype = SUBTYPE_MOUNTAIN;
    }
    else
    {
      if (result != 5)
      {
        return 0;
      }
      subtype = SUBTYPE_PLAINS;
    }
    result = has_subtype(affected_card_controller, affected_card, subtype);
    if ((result != 0) &&
        (result = is_basic_land(affected_card_controller, affected_card), result != 0))
    {
      if (event == EVENT_TAP_CARD)
      {
        result = instance->info_slot;
        if ((result == tapped_for_mana_color) ||
            ((tapped_for_mana_color == 0x100 && (0 < tapped_for_mana[result]))))
        {
          produce_mana(affected_card_controller, result, 1);
        }
      }
      else if ((((event == EVENT_COUNT_MANA) &&
                 (result = is_tapped(affected_card_controller, affected_card), result == 0)) &&
                (result = is_animated_and_sick(affected_card_controller, affected_card),
                 result == 0)) &&
               (result = can_produce_mana(affected_card_controller, affected_card), result != 0))
      {
        declare_mana_available(affected_card_controller, instance->info_slot, 1);
      }
    }
  }
  return 0;
}

int card_gemhide_sliver(int player, int card, event_t event){
/* CARD_ID_GEMHIDE_SLIVER	8559
Gemhide Sliver	|1|G
Creature - Sliver 1/1
All Slivers have "|T: Add one mana of any color to your mana pool." */

	int result = permanents_you_control_can_tap_for_mana_all_one_color(player, card, event, TYPE_CREATURE, SUBTYPE_SLIVER, COLOR_TEST_ANY_COLORED, 1);
	if (event == EVENT_CAN_ACTIVATE){
		return result;
	}

	return slivercycling(player, card, event);
}

int card_gemstone_caverns(int player, int card, event_t event){

  /* Gemstone Caverns	""
   * Legendary Land
   * If ~ is in your opening hand and you're not playing first, you may begin the game with ~ on the battlefield with a luck counter on it. If you do, exile a
   * card from your hand.
   * |T: Add |1 to your mana pool. If ~ has a luck counter on it, instead add one mana of any color to your mana pool. */

	check_legend_rule(player, card, event);

	if( event == EVENT_CHANGE_TYPE ){
		get_card_instance(player, card)->info_slot = COLOR_TEST_COLORLESS;
		if( count_counters(player, card, COUNTER_LUCK) ){
			get_card_instance(player, card)->info_slot = COLOR_TEST_ANY_COLORED;
		}
	}

	return plain_mana_producer(player, card, event);
}

/* ManalinkEh 2022: 0x20e9460; original cards/time_spiral.c:2543. */
int glass_asp_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  if (event != EVENT_PHASE_CHANGED)
  {
    return 0;
  }
  if (current_phase == 10)
  {
    instance = get_card_instance(player, card);
    if ((instance->targets[1].player != 0x42) &&
        (instance = get_card_instance(player, card), instance->targets[0].player == current_turn))
    {

      result = has_mana(current_turn, 0, 2);
      if ((result == 0) || (charge_mana_while_resolving(player, card, EVENT_DRAW_PHASE,
                                                        current_turn, COLOR_COLORLESS, 2),
                            spell_fizzled == 1))
      {
        instance = get_card_instance(player, card);
        damage_player(instance->targets[0].player, 2, player, card);
      }
      instance = get_card_instance(player, card);
      instance->targets[1].player = 0x42;
      kill_card(player, card, KILL_EXILE);
    }
    if (current_phase == 10)
    {
      return 0;
    }
  }
  instance = get_card_instance(player, card);
  instance->targets[1].player = 0;
  return 0;
}

/* ManalinkEh 2022: 0x20f1d10; original cards/time_spiral.c:2569. */
int card_glass_asp(int player, int card, event_t event)
{
  /* Glass Asp	|1|G|G
   * Creature - Snake 2/1
   * Whenever this creature deals combat damage to a player, that player loses 2 life at the beginning of their next draw step unless they pay |2 before that step. */

  if (!damage_dealt_by_me(player, card, event,
                           DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER))
    return 0;
  int32_t value = get_card_instance(player, card)->targets[1].player;
  int legacy = create_legacy_effect(player, card, glass_asp_legacy);
  card_instance_t *instance = get_card_instance(player, legacy);
  instance->targets[0].player = (unsigned int)((char)value == 0);
  return 0;
}

int card_goblin_skycutter(int player, int card, event_t event){
/* CARD_ID_GOBLIN_SKYCUTTER	8564
Goblin Skycutter	|1|R
Creature - Goblin Warrior 2/1
Sacrifice ~: ~ deals 2 damage to target creature with flying. That creature loses flying until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			negate_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, KEYWORD_FLYING);
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target creature with flying.");
}

int card_gorgon_recluse(int player, int card, event_t event)
{
/* CARD_ID_GORGON_RECLUSE	8565
Gorgon Recluse	|3|B|B
Creature - Gorgon 2/4
Whenever ~ blocks or becomes blocked by a non|Sblack creature, destroy that creature at end of combat.
Madness |B|B (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

	if( event == EVENT_CAN_PAY_MADNESS_COST || event == EVENT_PAY_MADNESS_COST ){
		return madness(player, card, event, MANACOST_B(2));
	}

	return card_deathgazer(player, card, event);
}

int card_grapeshot(int player, int card, event_t event){
	/* Grapeshot	|1|R	0x2006fa1
	 * Sorcery
	 * ~ deals 1 damage to any target.
	 * Storm */

	if (event == EVENT_QUERY){ return attacking_card == QUERY_STORM; }
	if (!IS_GS_EVENT(player, card, event)){ return 0; }
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	if (event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td)){ damage_target0(player, card, 1); }
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_greater_gargadon_suspended(int player, int card, event_t event){

	/* Greater Gargadon	|9|R
	 * Creature - Beast 9/7
	 * Suspend 10-|R
	 * Sacrifice an artifact, creature, or land: Remove a time counter from ~. Activate this ability only if ~ is suspended. */

	card_instance_t *instance = get_card_instance(player, card);

	if (instance->targets[7].player == 42){ return 0; }

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		remove_counters(player, card, player, card, COUNTER_TIME, 1);
	}

	if (IS_ACTIVATING(event)){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ARTIFACT | TYPE_CREATURE | TYPE_LAND, "");

		if (event == EVENT_CAN_ACTIVATE){
			return new_can_sacrifice_as_cost(player, card, &test);
		}

		if( event == EVENT_ACTIVATE ){
			new_sacrifice(player, card, player, 0, &test);
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			remove_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_TIME, 1);
		}
	}

	if( event == EVENT_STATIC_EFFECTS && count_counters(player, card, COUNTER_TIME) <= 0){
		instance->targets[7].player = 42;
		int id = CARD_ID_GREATER_GARGADON;
		if( check_rfg(player, id) ){
			int crd = play_card_in_exile_for_free(player, player, id);
			pump_ability_t haste_pump;
			default_pump_ability_definition(player, card, &haste_pump, 0, 0, 0, SP_KEYWORD_HASTE);
			haste_pump.paue_flags = PAUE_END_IF_TARGET_CHANGES_CONTROL;
			pump_ability(player, card, player,crd, &haste_pump);
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return 0;
}

int card_greater_gargadon(int player, int card, event_t event)
{
  /* Greater Gargadon	|9|R
   * Creature - Beast 9/7
   * Suspend 10-|R
   * Sacrifice an artifact, creature, or land: Remove a time counter from ~. Activate this ability only if ~ is suspended. */

  card_instance_t *instance;
  int result;
  card_instance_t *spell_instance;
  if (event != EVENT_MODIFY_COST)
  {
    if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
        (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      result = played_for_free(player, card);
      if (result == 0)
      {
        result = is_token(player, card);
        if (result == 0)
        {
          spell_instance = get_card_instance(player, card);
          if (spell_instance->info_slot == 1)
          {
            spell_instance = get_card_instance(player, card);
            result =
                has_mana_to_cast_iid(player, EVENT_CAST_SPELL, spell_instance->internal_card_id);
            result = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_STORAGE,
                                 "Play normally", result, 5, "Suspend", 1, 1, 0, 0, 0, 0);
            if (result == 0)
            {
              spell_fizzled = 1;
            }
            else if (result == 1)
            {
              charge_mana_from_id(player, card, EVENT_CAST_SPELL, -1);
            }
            else if (result == 2)
            {
              charge_mana(player, COLOR_RED, 1);
              if (spell_fizzled != 1)
              {
                result = get_internal_card_id_from_csv_id(CARD_ID_GREATER_GARGADON_SUSPENDED);
                result = add_card_to_hand(player, result);
                add_counters(player, card, player, result, COUNTER_TIME, 10);
                put_into_play(player, result);
                add_card_to_rfg(player, instance->internal_card_id);
                obliterate_card(player, card);
                land_can_be_played = land_can_be_played & 0xffffffdf;
              }
            }
          }
        }
      }
    }
    return 0;
  }
  result = has_mana(player, 4, 1);
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

int card_greenseeker(int player, int card, event_t event){
	/* Greenseeker	|G
	 * Creature - Elf Spellshaper 1/1
	 * |G, |T, Discard a card: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library. */

	if( event == EVENT_ACTIVATE ){
		int total = get_total_available_lands(player);
		if( total <= 3 ){
			ai_modifier += 48;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		tutor_basic_lands(player, TUTOR_HAND, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD, MANACOST_G(1), 0, NULL, NULL);
}

// Griffin --> Vanilla

int card_griffin_guide(int player, int card, event_t event)
{
  /* Griffin Guide	|2|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+2 and has flying.
   * When enchanted creature dies, put a 2/2 |Swhite Griffin creature token with flying onto the battlefield. */

  if (attached_creature_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	generate_token_by_id(player, card, CARD_ID_GRIFFIN);

  return generic_aura(player, card, event, player, 2, 2, KEYWORD_FLYING, 0, 0, 0, 0);
}

int card_ground_rift(int player, int card, event_t event){

	/* Ground Rift	|R
	 * Sorcery
	 * Target creature without flying can't block this turn.
	 * Storm */

	if (event == EVENT_QUERY){ return attacking_card == QUERY_STORM; }
	if (!IS_GS_EVENT(player, card, event)){ return 0; }
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_abilities |= KEYWORD_FLYING;
	card_instance_t* instance = get_card_instance(player, card);
	if (event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td)){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
}

int card_harmonic_sliver(int player, int card, event_t event){
/* CARD_ID_HARMONIC_SLIVER	8573
Harmonic Sliver	|1|G|W
Creature - Sliver 1/1
All Slivers have "When this permanent enters the battlefield, destroy target artifact or enchantment." */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_SLIVER;

		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		int p = trigger_cause_controller;
		int c = trigger_cause;

		target_definition_t td;
		default_target_definition(p, c, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT );
		td.allow_cancel = 0;
		td.who_chooses = p;

		card_instance_t *instance = get_card_instance(p, c);

		if (can_target(&td) && pick_target(&td, "DISENCHANT")){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return slivercycling(player, card, event);
}

int card_haunting_hymn(int player, int card, event_t event){

	/* Haunting Hymn	|4|B|B
	 * Instant
	 * Target player discards two cards. If you cast this spell during your main phase, that player discards four cards instead. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int to_discard = 2;
			if( current_turn == player && (current_phase == PHASE_MAIN1 || current_phase == PHASE_MAIN2) ){
				to_discard = 4;
			}
			new_multidiscard(instance->targets[0].player, to_discard, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_herd_gnarr(int player, int card, event_t event){
/* CARD_ID_HERD_GNARR	8575
Herd Gnarr	|3|G
Creature - Beast 2/2
Whenever another creature enters the battlefield under your control, ~ gets +2/+2 until end of turn. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		pump_until_eot_merge_previous(player, card, player, card, 2, 2);
	}

	return 0;
}

int card_hivestone(int player, int card, event_t event)
{
  /* CARD_ID_HIVESTONE	8576
Hivestone	|2
Artifact
Creatures you control are Slivers in addition to their other creature types. */

if (event == EVENT_CHANGE_TYPE && affected_card_controller == player &&
      is_what(affected_card_controller, affected_card, TYPE_CREATURE) && in_play(player, card) &&
      !is_humiliated(player, card, AL_STATIC))
    add_a_subtype(affected_card_controller, affected_card, SUBTYPE_SLIVER);
  return 0;
}

int card_hypergenesis(int player, int card, event_t event)
{
  /* Hypergenesis	""
   * Sorcery
   * Suspend 3-|1|G|G
   * Starting with you, each player may put an artifact, creature, enchantment, or land card from his or her hand onto the battlefield. Repeat this process
   * until no one puts a card onto the battlefield. */

  if (event == EVENT_RESOLVE_SPELL)
	card_eureka(player, card, event);

  return suspend_only(player, card, event, 3, MANACOST_XG(1,2));
}

void damage_all_my_blockers(int player, int card, int dmg){
	int i;
	for(i=active_cards_count[1-player]-1; i>-1; i--){
		if( in_play(1-player, i) && is_what(1-player, i, TYPE_CREATURE) ){
			card_instance_t *this = get_card_instance(1-player, i);
			if( this->blocking == card ){
				damage_creature(1-player, i, dmg, player, card);
			}
		}
	}
}

static void sac_and_damage_blockers(int player, int card, int t_player, int t_card)
{
  if (in_play(player, card))
  {
    damage_creature(t_player, t_card, 4, player, card);
    kill_card(player, card, KILL_SACRIFICE);
  }
}

int card_ib_halfheart_goblin_tactician(int player, int card, event_t event)
{
  /* Ib Halfheart, Goblin Tactician	|3|R
   * Legendary Creature - Goblin Advisor 3/2
   * Whenever another Goblin you control becomes blocked, sacrifice it. If you do, it deals 4 damage to each creature blocking it.
   * Sacrifice two Mountains: Create two 1/1 red Goblin creature tokens. */

  int goblin_or_result;
  const char *sacrifice_prompt;
  unsigned int chosen_sacrifice;
  int attacking_or_goblin;
  card_instance_t *instance;
  unsigned int player_00;
  int sacs[2];
  test_definition_t test;
  check_legend_rule(player, card, event);
  if (event == EVENT_DECLARE_BLOCKERS)
  {
    if (((current_turn == player) &&
         (goblin_or_result = is_humiliated(player, card, AL_TRIG), goblin_or_result == 0)) &&
        (goblin_or_result = 0, 0 < active_cards_count[player]))
    {
      do
      {
        instance = in_play(player, goblin_or_result);
        if (((instance != NULL) && (attacking_or_goblin = is_what(player, goblin_or_result, 2),
                                    attacking_or_goblin != 0)) &&
            ((attacking_or_goblin = is_attacking(player, goblin_or_result),
              attacking_or_goblin != 0 &&
                  ((attacking_or_goblin =
                        has_subtype(current_turn, goblin_or_result, SUBTYPE_GOBLIN),
                    card != goblin_or_result && (attacking_or_goblin != 0))))))
        {
          for_each_creature_blocking_me(player, goblin_or_result, sac_and_damage_blockers, player,
                                        goblin_or_result);
        }
        goblin_or_result = goblin_or_result + 1;
      } while (goblin_or_result < active_cards_count[player]);
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      if (is_humiliated(player, card, AL_NM_NL_ACT) != 0)
      {
        return 0;
      }
      default_test_definition(&test, 1);
      test.subtype = get_hacked_subtype(player, card, 0x5006);
      if (event == EVENT_CAN_ACTIVATE)
      {
        test.qty = 2;
        return generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE,
                                                                GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL,
                                                                NULL, TYPE_NONE, &test, SARV_0);
      }
      if (event == EVENT_ACTIVATE)
      {
        sacrifice_prompt = get_hacked_land_text(player, card, "Select a %s to sacrifice", 0x5006);
        scnprintf(test.message, 100, "%s", sacrifice_prompt);
        bool sacrifices_selected = false;
        sacs[1] = 0;
        chosen_sacrifice = new_sacrifice(player, card, player,
                                         SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
        sacs[0] = chosen_sacrifice;
        if (chosen_sacrifice != 0)
        {
          sacs[1] = new_sacrifice(player, card, player,
                                  SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
          if (sacs[1] != 0)
          {
            if (spell_fizzled != 1)
            {
              generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 0, NULL,
                                        NULL);
            }
            sacrifices_selected = true;
          }
        }
        if (!sacrifices_selected)
          spell_fizzled = 1;
        for (int i = 0; i < 2; ++i)
        {
          chosen_sacrifice = (unsigned int)sacs[i];
          if (chosen_sacrifice != 0)
          {
            player_00 = (int)chosen_sacrifice >> 0x10 & 0xff;
            state_untargettable(player_00, chosen_sacrifice >> 0x18, 0);
            if (spell_fizzled != 1)
            {
              kill_card(player_00, chosen_sacrifice >> 0x18, KILL_SACRIFICE);
            }
          }
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
      if (is_humiliated(player, card, AL_NM_NL_ACT) != 0)
      {
        return 0;
      }
      default_test_definition(&test, 1);
      test.subtype = get_hacked_subtype(player, card, 0x5006);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      generate_tokens_by_id(instance->parent_controller, instance->parent_card, CARD_ID_GOBLIN, 2);
    }
  }
  return 0;
}

int card_icatian_crier(int player, int card, event_t event){
/* CARD_ID_ICATIAN_CRIER	8579
Icatian Crier	|2|W
Creature - Human Spellshaper 1/1
|1|W, |T, Discard a card: Create two 1/1 |Swhite Citizen creature tokens. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_tokens_by_id(player, card, CARD_ID_CITIZEN, 2);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD, MANACOST_XW(1, 1), 0, NULL, NULL);
}

int card_ith_high_arcanist(int player, int card, event_t event){

	/* Ith, High Arcanist	|5|W|U
	 * Legendary Creature - Human Wizard 3/5
	 * Vigilance
	 * |T: Untap target attacking creature. Prevent all combat damage that would be dealt to and dealt by that creature this turn.
	 * Suspend 4-|W|U */

	check_legend_rule(player, card, event);

	vigilance(player, card, event);

	suspend(player, card, event, 4, MANACOST_WU(1, 1), NULL, NULL);

	return card_maze_of_ith(player, card, event);
}

int card_ivory_giant(int player, int card, event_t event){

	/* Ivory Giant	|5|W|W
	 * Creature - Giant 3/4
	 * When ~ enters the battlefield, tap all non|Swhite creatures.
	 * Suspend 5-|W */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		this_test.color_flag = 1;
		new_manipulate_all(player, card, ANYBODY, &this_test, ACT_TAP);
	}

	return suspend(player, card, event, 5, MANACOST_W(1), NULL, NULL);
}

static int ixidron_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_CHANGE_TYPE && affect_me(p, c) ){
			if( instance->dummy3 > 0 ){
				event_result = instance->dummy3;
			}
		}
	}

	return 0;
}

int card_ixidron(int player, int card, event_t event)
{
  /* Ixidron	|3|U|U
   * Creature - Illusion 100/100
   * As ~ enters the battlefield, turn all other nontoken creatures face down.
   * ~'s power and toughness are each equal to the number of face-down creatures on the battlefield. */

  int count;
  int face_down_iid;
  card_instance_t *instance;
  int legacy_or_result;
  int card_00;
  int player_00;
  if (event == EVENT_RESOLVE_SPELL)
  {
    player_00 = 0;
    face_down_iid = get_internal_card_id_from_csv_id(CARD_ID_FACE_DOWN_CREATURE);
    do
    {
      card_00 = active_cards_count[player_00] + -1;
      if (-1 < card_00)
      {
        do
        {
          instance = in_play(player_00, card_00);
          if ((((instance != NULL) &&
                (legacy_or_result = is_what(player_00, card_00, 2), legacy_or_result != 0)) &&
               ((player != player_00 || (card != card_00)))) &&
              ((legacy_or_result = is_token(player, card),
                legacy_or_result == 0 &&
                    (legacy_or_result = get_id(player_00, card_00), legacy_or_result != 0x33c4))))
          {
            legacy_or_result = call_card_function(player_00, card_00, EVENT_HAS_MORPH);
            if (legacy_or_result == 0)
            {
              legacy_or_result =
                  create_targetted_legacy_effect(player, card, ixidron_legacy, player_00, card_00);
              instance = get_card_instance(player, legacy_or_result);
              instance->dummy3 = face_down_iid;
              instance->token_status = instance->token_status | 0x10000;
            }
            turn_face_down(player_00, card_00);
          }
          card_00 = card_00 + -1;
        } while (card_00 != -1);
      }
      player_00 = player_00 + 1;
    } while (player_00 != 2);
    return 0;
  }
  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    count = count_cards_by_id(2, CARD_ID_FACE_DOWN_CREATURE);
    set_characteristic_pt(count);
  }
  return 0;
}

int card_jaya_ballard_task_mage(int player, int card, event_t event){
	/* Jaya Ballard, Task Mage	|1|R|R	0x2002843
	 * Legendary Creature - Human Spellshaper 2/2
	 * |R, |T, Discard a card: Destroy target |Sblue permanent.
	 * |1|R, |T, Discard a card: ~ deals 3 damage to any target. A creature dealt damage this way can't be regenerated this turn.
	 * |5|R|R, |T, Discard a card: ~ Ballard deals 6 damage to each creature and each player. */
	check_legend_rule(player, card, event);

	if (!IS_ACTIVATING(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE &&
		!(can_use_activated_abilities(player, card)  && ! is_sick(player, card) && ! is_tapped(player, card) &&
		  hand_count[player] > 0)
	  ){
		return 0;
	}

	enum{
		CHOICE_KILL_BLUE_PERMANENT = 1,
		CHOICE_BOLT = 2,
		CHOICE_INFERNO = 3
	};
	unsigned int can_kill_blue_permanent = can_target(&td);
	unsigned int can_bolt = can_target(&td1);
	unsigned int can_inferno = 1;
	unsigned int inferno_value = life[player] > 6 && life[1-player] < 7 ? 10 : 0;
	int choice = DIALOG(player, card, event,
						DLG_RANDOM,
						"Destroy blue permanent", can_kill_blue_permanent, 5, DLG_MANA6(0, 0, 0, 0, 1, 0),
						"Deal 3 damage", can_bolt, 10, DLG_MANA6(1, 0, 0, 0, 1, 0),
						"Deal 6 damage to all", can_inferno, 5+inferno_value, DLG_MANA6(5, 0, 0, 0, 2, 0));

	if (event == EVENT_CAN_ACTIVATE){
		return choice;
	}
	else if (event == EVENT_ACTIVATE){
		if( choice == CHOICE_KILL_BLUE_PERMANENT ){
			if( new_pick_target(&td, get_sleighted_color_text(player, card, "Select target %s permanent.", COLOR_BLUE), 0, GS_LITERAL_PROMPT) ){
				discard(player, 0, player);
				instance->number_of_targets = 1;
				tap_card(player, card);
                add_state(player, card, STATE_TAPPED);
			}
		}
		else if( choice == CHOICE_BOLT ){
			if( pick_target(&td1, "TARGET_ANY") ){
				discard(player, 0, player);
				instance->number_of_targets = 1;
				add_state(player, card, STATE_TAPPED);
			}
		}
		else if( choice == CHOICE_INFERNO ){
			discard(player, 0, player);
			add_state(player, card, STATE_TAPPED);
		}
	}
	else if( event == EVENT_RESOLVE_ACTIVATION ){
		if( choice == CHOICE_KILL_BLUE_PERMANENT ){
			if( valid_target(&td) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY );
			}
		}
		else if( choice == CHOICE_BOLT ){
			if( valid_target(&td1) ){
				int dc = damage_target0(player, card, 3);
				if( instance->targets[0].card != -1 && is_what(instance->targets[0].player, instance->targets[0].card, TYPE_CREATURE) ){
					get_card_instance(player, dc)->targets[3].card |= DMG_CANT_REGENERATE_IF_DEALT_DAMAGE_THIS_WAY;
				}
			}
		}
		else if( choice == CHOICE_INFERNO ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			new_damage_all(player, card, 2, 6, NDA_ALL_CREATURES+NDA_PLAYER_TOO, &this_test);
		}
	}
	return 0;
}

int card_jediths_dragoons(int player, int card, event_t event){
/* CARD_ID_JEDITS_DRAGOONS	8586
Jedit's Dragoons	|5|W
Creature - Cat Soldier 2/5
Vigilance
When ~ enters the battlefield, you gain 4 life. */

	vigilance(player, card, event);
	return cip_lifegain(player, card, event, 4);
}

static const char *target_must_be_permanent_or_suspend_and_have_time_counters(int who_chooses, int player,
                                                                       int card,
                                                                       int targeting_player,
                                                                       int targeting_card)
{
  const char *perm_or_susp = target_must_be_permanent_or_suspend(
      who_chooses, player, card, targeting_player, targeting_card);
  if (!perm_or_susp && !count_counters(player, card, COUNTER_TIME))
    return "must have counters";
  return perm_or_susp;
}

int card_jhoiras_timebug(int player, int card, event_t event)
{
  /* Jhoira's Timebug	|2
   * Artifact Creature - Insect 1/2
   * |T: Choose target permanent you control or suspended card you own. If that permanent or card has a time counter on it, you may remove a time counter from
   * it or put another time counter on it. */

  int t_player;
  const char *(*target_requirement)(int, int, int, int, int);
  card_instance_t *instance;
  int target_card_or_legacy;
  int choice;
  target_definition_t td;

  if (((event != EVENT_ACTIVATE) && (1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION))) &&
      (event != EVENT_CLEANUP))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 199);
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
  {
    target_requirement = target_must_be_permanent_or_suspend_and_have_time_counters;
  }
  else
  {
    target_requirement = target_must_be_permanent_or_suspend;
  }
  td.extra = (int32_t)target_requirement;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (valid_target(&td) != 0)
    {
      if (count_counters(instance->targets[0].player, instance->targets[0].card, COUNTER_TIME) != 0)
      {
        target_card_or_legacy =
            is_what(instance->targets[0].player, instance->targets[0].card, 0x80);
        choice =
            do_dialog(player, player, card, -1, -1, " Add a time counter\n Remove a time counter",
                      (unsigned int)(target_card_or_legacy != 0));
        target_card_or_legacy = instance->targets[0].card;
        t_player = instance->targets[0].player;
        if (choice == 0)
        {
          add_counters(player, card, t_player, target_card_or_legacy, COUNTER_TIME, 1);
        }
        else
        {
          remove_counters(player, card, t_player, target_card_or_legacy, COUNTER_TIME, 1);
        }
      }
    }
  }
  return generic_activated_ability(player, card, event, 7, 0, 0, 0, 0, 0, 0, 0, &td,
                                   "Select target permanent or Suspend legacy.");
}

int card_kaervek_the_merciless(int player, int card, event_t event){
	/* Kaervek the Merciless	|5|B|R	0x2006fe7
	 * Legendary Creature - Human Shaman 5/4
	 * Whenever an opponent casts a spell, ~ deals damage to any target equal to that spell's converted mana cost. */
	check_legend_rule(player, card, event);

	sp_ability_trigger(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
	if (event == EVENT_SPA){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
			int cmc = get_cmc(trigger_cause_controller, trigger_cause);
			damage_target0(player, card, cmc);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

/* ManalinkEh 2022: 0x20f4170; original cards/time_spiral.c:3286. */
int card_keldon_halberdier(int player, int card, event_t event)
{
  /* Keldon Halberdier	|4|R
   * Creature - Human Warrior 4/1
   * First strike
   * Suspend 4-|R (Rather than cast this card from your hand, you may pay |R and exile it with four time counters on it. At the beginning of your upkeep, remove a time counter. When the last is removed, you may cast it without paying its mana cost. It has haste.) */

  int result;
  result = suspend(player, card, event, 4, 0, 0, 0, 0, 1, 0, NULL, NULL);
  return result;
}

int card_kher_keep(int player, int card, event_t event){
  /* Kher Keep	""
   * Legendary Land
   * |T: Add |C.
   * |1|R, |T: Create a 0/1 red Kobold creature token named Kobolds of ~. */

	/*
	  Kher Keep
	  Legendary Land
	  {T}: Add {1} to your mana pool.
	  {1}{R}, {T}: Put a 0/1 red Kobold creature token named Kobolds of Kher Keep onto the battlefield.
	*/
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	if (event == EVENT_ACTIVATE){
		instance->info_slot = 0;
		int choice = 0;
		if( !paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_XR(2, 1), 0, NULL, NULL) ){
			choice = do_dialog(player, player, card, -1, -1, " Get mana\n Generate a Kobolds of Kher Keep\n Cancel", 1);
		}
		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		else if( choice == 1 ){
				add_state(player, card, STATE_TAPPED);
				if( charge_mana_for_activated_ability(player, card, MANACOST_XR(1, 1)) ){
					instance->info_slot = 1;
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
		if( instance->info_slot == 1 ){
			generate_token_by_id(player, card, CARD_ID_KOBOLDS_OF_KHER_KEEP);
		}
	}

	return 0;
}

int card_knight_of_the_holy_nimbus(int player, int card, event_t event){
/* CARD_ID_KNIGHT_OF_THE_HOLY_NIMBUS	8591
Knight of the Holy Nimbus	|W|W
Creature - Human Rebel Knight 2/2
Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)
If ~ would be destroyed, regenerate it.
|2: ~ can't be regenerated this turn. Only any opponent may activate this ability. */

	flanking(player, card, event);

	holy_nimbus(player, card, event, 2);

	return 0;
}

int card_krosan_grip(int player, int card, event_t event){
/* CARD_ID_KROSAN_GRIP	8592
Krosan Grip	|2|G
Instant
Split second (As long as this spell is on the stack, players can't cast spells or activate abilities that aren't mana abilities.)
Destroy target artifact or enchantment. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		add_state(player, card, STATE_CANNOT_TARGET);
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
		if( spell_fizzled != 1 ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		else{
			remove_state(player, card, STATE_CANNOT_TARGET);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		remove_state(player, card, STATE_CANNOT_TARGET);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_liege_of_the_pit(int player, int card, event_t event){
/* CARD_ID_LIEGE_OF_THE_PIT	8593
Liege of the Pit	|5|B|B|B
Creature - Demon 7/7
Flying, trample
At the beginning of your upkeep, sacrifice a creature other than ~. If you can't, ~ deals 7 damage to you.
Morph |B|B|B|B (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		state_untargettable(player, card, 1);
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_AS_COST | SAC_NO_CANCEL, &test);
		if (!sac){
			damage_player(player, 7, player, card);
		}
		state_untargettable(player, card, 0);
	}

	return morph(player, card, event, MANACOST_B(4));
}

int card_lightning_axe(int player, int card, event_t event)
{
  /* Lightning Axe	|R
   * Instant
   * As an additional cost to cast ~, discard a card or pay |5.
   * ~ deals 5 damage to target creature. */

  if (event == EVENT_MODIFY_COST)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, 0, "");
	  test.not_me = 1;
	  test.zone = TARGET_ZONE_HAND;

	  card_instance_t* instance = get_card_instance(player, card);
	  if (!check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test))
		{
		  instance->info_slot = 5;
		  COST_COLORLESS += 5;
		}
	  else
		instance->info_slot = 0;
	}

  if (!IS_CASTING(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CAN_CAST)
	return can_target(&td);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  if (played_for_free(player, card)	// no cost
		  || instance->info_slot == 5)	// already paid
		{
		  pick_target(&td, "TARGET_CREATURE");
		  return 0;
		}

	  enum
	  {
		CHOICE_CANCEL = 0,
		CHOICE_DISCARD = 1,
		CHOICE_MANA
	  } choice = DIALOG(player, card, EVENT_CAST_SPELL,
						DLG_RANDOM, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1,
						"Discard", hand_count[player] > 0, 1,
						"Pay 5", has_mana(player, COLOR_ANY, 5), 3);

	  switch (choice)
		{
		  case CHOICE_CANCEL:
			cancel = 1;	// Must be done manually, since DLG_NO_STORAGE set
			break;

		  case CHOICE_DISCARD:
			if (pick_target(&td, "TARGET_CREATURE"))
			  discard(player, 0, player);
			break;

		  case CHOICE_MANA:
			charge_mana(player, COLOR_COLORLESS, 5);
			if (cancel != 1)
			  pick_target(&td, "TARGET_CREATURE");
			break;
		}
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		damage_target0(player, card, 5);

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

static int lim_dul_the_necromancer_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_LIM_DUL_THE_NECROMANCER)
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
          }
        }
        count_for_gfp_ability_and_store_values(player, card, event, 1 - player, TYPE_CREATURE, NULL,
                                               GFPC_TRACK_DEAD_CREATURES | GFPC_STORE_OWNER, 1);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result)
  {
    if (!check_status(player, card, STATUS_INVISIBLE_FX))
    {
      card_instance_t *instance = get_card_instance(player, card);
      int dead_array[3][9];
      int dac = 0;
      int i;
      for (i = 1; i < 10; i++)
      {
        dead_array[0][dac] = instance->targets[i].player;
        dead_array[1][dac] = instance->targets[i].card;
        dead_array[2][dac] = dead_array[1][dac];
        instance->targets[i].player = instance->targets[i].card = -1;
        dac++;
      }
      int chosen = 0;
      test_definition_t test;
      new_default_test_definition(&test, TYPE_ANY, "Select a permanent to bring back.");
      test.id = cards_data[iid_draw_a_card].id;
      test.id_flag = DOESNT_MATCH;
      while (has_mana_multi(player, MANACOST_XB(chosen + 1, chosen + 1)))
      {
        int selected =
            select_card_from_zone(player, player, dead_array[1], dac, 0, AI_MAX_CMC, -1, &test);
        if (selected == -1)
        {
          break;
        }
        dead_array[1][selected] = iid_draw_a_card;
        chosen++;
      }
      if (chosen)
      {
        if (charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player,
                                              MANACOST_XB(chosen, chosen)))
        {
          for (i = 0; i < dac; i++)
          {
            if (dead_array[1][i] == iid_draw_a_card)
            {
              int owner = dead_array[0][i];
              int csvid = cards_data[dead_array[2][i]].id;
              seek_grave_for_id_to_reanimate(player, card, owner, csvid,
                                             REANIMATE_ADD_BLACK_ZOMBIE);
            }
          }
        }
      }
      kill_card(player, card, KILL_EXILE);
    }
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_lim_dul_the_necromancer(int player, int card, event_t event){
  /* Lim-Dûl the Necromancer |5|B|B
   * Legendary Creature — Human Wizard 4/4
   * Whenever a creature an opponent controls dies, you may pay {1}{B}. If you do, return that card to the battlefield under your control.
   * 	If it's a creature, it's a Zombie in addition to its other creature types.
   * {1}{B}: Regenerate target Zombie. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (intptr_t)lim_dul_the_necromancer_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &lim_dul_the_necromancer_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability_and_store_values(player, card, event, 1-player, TYPE_CREATURE, NULL, GFPC_TRACK_DEAD_CREATURES | GFPC_STORE_OWNER, 0);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			card_instance_t *instance = get_card_instance(player, card);
			int amount = BYTE0(result);
			int dead_array[3][amount];
			int i;
			for(i=0; i<amount; i++){
				dead_array[0][i] = instance->targets[i].player;
				dead_array[1][i] = instance->targets[i].card;
				dead_array[2][i] = dead_array[1][i];
				instance->targets[i].player = instance->targets[i].card = -1;
			}
			int chosen = 0;
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a permanent to bring back.");
			test.id = cards_data[iid_draw_a_card].id;
			test.id_flag = DOESNT_MATCH;
			while( has_mana_multi(player, MANACOST_XB(chosen+1, chosen+1)) ){
					int selected = select_card_from_zone(player, player, dead_array[1], amount, 0, AI_MAX_CMC, -1, &test);
					if( selected == -1 ){
						break;
					}
					dead_array[1][selected] = iid_draw_a_card;
					chosen++;
			}
			if( chosen ){
				if( charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, MANACOST_XB(chosen, chosen)) ){
					for(i=0; i<amount; i++){
						if( dead_array[1][i] == iid_draw_a_card ){
							int owner = dead_array[0][i];
							int csvid = cards_data[dead_array[2][i]].id;
							seek_grave_for_id_to_reanimate(player, card, owner, csvid, REANIMATE_ADD_BLACK_ZOMBIE);
						}
					}
				}
			}
		}
	}

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_ZOMBIE;
	td.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && (land_can_be_played & LCBP_REGENERATION) ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XB(1, 1), 0, &td, NULL) ){
			return 99;
		}
	}

	if( event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XB(1, 1), 0,
										&td, "Select target Zombie to regenerate.");
	}

	if( event == EVENT_RESOLVE_ACTIVATION && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
		regenerate_target(instance->targets[0].player, instance->targets[0].card);
	}

	return 0;
}

int card_living_end(int player, int card, event_t event)
{
	/* Living End	""
	 * Sorcery
	 * Suspend 3-|2|B|B
	 * Each player exiles all creature cards from his or her graveyard, then sacrifices all creatures he or she controls,
	 * then puts all cards he or she exiled
	 * this way onto the battlefield. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_reanimate_all(player, card, ANYBODY, &this_test, REANIMATE_SPECIAL_LIVING_DEAD);
		kill_card(player, card, KILL_DESTROY);
	}

	return suspend_only(player, card, event, 3, MANACOST_XB(2,2));
}

int card_lockets_of_yesterday(int player, int card, event_t event){
/* CARD_ID_LOCKET_OF_YESTERDAYS	8597
Locket of Yesterdays	|1
Artifact
Spells you cast cost |1 less to cast for each card with the same name as that spell in your graveyard. */

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card, AL_STATIC) ){
		if( get_grave(player)[0] > -1 ){
			COST_COLORLESS -= count_graveyard_by_id(player, get_id(affected_card_controller, affected_card));
		}
	}

	return 0;
}

int card_looter_il_kor(int player, int card, event_t event){
/* CARD_ID_LOOTER_IL_KOR	8598
Looter il-Kor	|1|U
Creature - Kor Rogue 1/1
Shadow (This creature can block or be blocked by only creatures with shadow.)
Whenever ~ deals damage to an opponent, draw a card, then discard a card. */

	shadow(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_DAMAGE_OPPONENT);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		while( amount ){
				draw_a_card(player);
				discard(player, 0, player);
				amount--;
		}
	}

	return 0;
}

int card_lotus_bloom(int player, int card, event_t event)
{
  /* Lotus Bloom	""
   * Artifact
   * Suspend 3-|0
   * |T, Sacrifice ~: Add three mana of any one color to your mana pool. */

  int rval = suspend_only(player, card, event, 3, MANACOST_X(0));

  return rval ? rval : card_black_lotus(player, card, event);
}

int card_magus_of_the_candelabra(int player, int card, event_t event)
{
/* CARD_ID_MAGUS_OF_THE_CANDELABRA	1282
Magus of the Candelabra	|G
Creature - Human Wizard 1/2
|X, |T: Untap X target lands. */

  if (event == EVENT_ATTACK_RATING && affect_me(player, card))
	ai_defensive_modifier += 24 / (landsofcolor_controlled[player][COLOR_ANY] + 2);
  else if (event == EVENT_BLOCK_RATING && affect_me(player, card))
	ai_defensive_modifier -= 96 / (landsofcolor_controlled[player][COLOR_ANY] + 2);
  else
	return card_candelabra_of_tawnos(player, card, event);

  return 0;
}

int card_magus_of_the_jar(int player, int card, event_t event){
/* CARD_ID_MAGUS_OF_THE_JAR	8600
Magus of the Jar	|3|U|U
Creature - Human Wizard 3/3
|T, Sacrifice ~: Each player exiles all cards from his or her hand face down and draws seven cards.
At the beginning of the next end step, each player discards his or her hand and returns to his or her hand each card he or she exiled this way. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return card_memory_jar(player, card, event);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_magus_of_the_mirror(int player, int card, event_t event){
  /* Magus of the Mirror	|4|B|B
   * Creature - Human Wizard 4/2
   * |T, Sacrifice this creature: Exchange life totals with target opponent. Activate only during your upkeep. */

	return card_mirror_universe(player, card, event);
}

int card_magus_of_the_scroll(int player, int card, event_t event){
	/* Magus of the Scroll	|R	0x2005494
	 * Creature - Human Wizard 1/1
	 * |3, |T: Choose a card name, then reveal a card at random from your hand. If that card has the chosen name, ~ deals 2 damage to any target. */
	// Used externally
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td1.allow_cancel = 0;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			if( hand_count[player] == 0 ){ return 0; }
			char msg[100] = "Select a card to name.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, msg);
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &this_test);
			if( player == AI ){
				do_dialog(HUMAN, player, card, player, selected, "AI Names...", 0);
			}
			int sel2 = get_random_card_in_hand(player);
			reveal_card(player, card, player, sel2);
			if( get_id(player, sel2) == get_id(player, selected) ){
				damage_target0(player, card, 2);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td1, "TARGET_ANY");
}

int card_mangara_of_corondor(int player, int card, event_t event){
/* CARD_ID_MANGARA_OF_CORONDOR	8603
Mangara of Corondor	|1|W|W
Legendary Creature - Human Wizard 1/1
|T: Exile ~ and target permanent. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			if( in_play(instance->parent_controller, instance->parent_card)  ){
				kill_card(instance->parent_controller, instance->parent_card, KILL_EXILE);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PERMANENT");
}

int card_might_of_old_krosa(int player, int card, event_t event){

	/* Might of Old Krosa	|G
	 * Instant
	 * Target creature gets +2/+2 until end of turn. If you cast this spell during your main phase, that creature gets +4/+4 until end of turn instead. */

	int pump = 2;
	if( current_turn == player && (current_phase == PHASE_MAIN1 || current_phase == PHASE_MAIN2) ){
		pump = 4;
	}
	return vanilla_instant_pump(player, card, event, ANYBODY, player, pump, pump, 0, 0);
}

int card_might_sliver(int player, int card, event_t event){
/* CARD_ID_MIGHT_SLIVER	1378
Might Sliver	|4|G
Creature - Sliver 2/2
All Sliver creatures get +2/+2. */

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 2, 2, 0, BCT_INCLUDE_SELF);
	return slivercycling(player, card, event);
}

int mindlash_sliver_shared_ability(int player, int card, event_t event)
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
    is_humiliated(result, card_00, AL_NM_NL_ACT);
    return 0;
  }
  instance = in_play(result, card_00);
  if (!instance && event == EVENT_CAN_ACTIVATE)
    return 0;
  if (!instance && event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);
  result = is_humiliated(result, card_00, AL_NM_NL_ACT);
  if (result != 0)
    return 0;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    APNAP(p1, {
      discard(p1, 0, player);
    });
  }
  return shared_sliver_activated_ability(player, card, event, 8, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

int card_mindlash_sliver(int player, int card, event_t event){
  /* Mindlash Sliver |B
   * 	  Creature - Sliver 1/1,
   * 	  All Slivers have "{1}, Sacrifice this permanent: Each player discards a card." */

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &mindlash_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &mindlash_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int card_mishra_artificer_prodigy(int player, int card, event_t event){
/* CARD_ID_MISHRA_ARTIFICER_PRODIGY	8607
Mishra, Artificer Prodigy	|1|U|B|R
Legendary Creature - Human Artificer 4/4
Whenever you cast an artifact spell, you may search your graveyard, hand, and/or library for a card with the same name as that spell
and put it onto the battlefield. If you search your library this way, shuffle it. */

	check_legend_rule(player, card, event);
	sp_ability_trigger(player, event, RESOLVE_TRIGGER_AI(player), TYPE_ARTIFACT, NULL, player, card);
	if (event == EVENT_SPA){
		int result = tutor_card_with_the_same_name(trigger_cause_controller, trigger_cause, player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0);
		if (result < 0){
			result = tutor_card_with_the_same_name(trigger_cause_controller, trigger_cause, player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0);
			if (result < 0){
				tutor_card_with_the_same_name(trigger_cause_controller, trigger_cause, player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0);
			}
		}
	}
	return 0;
}

int card_mogg_war_marshal(int player, int card, event_t event){
/* CARD_ID_MOGG_WAR_MARSHAL	8608
Mogg War Marshal	|1|R
Creature - Goblin Warrior 1/1
Echo |1|R (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)
When ~ enters the battlefield or dies, create a 1/1 |Sred Goblin creature token. */

	echo(player, card, event, MANACOST_XR(1, 1));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		generate_token_by_id(player, card, CARD_ID_GOBLIN);
	}

	return 0;
}

int card_molder2(int player, int card, event_t event){
/* CARD_ID_MOLDER	8609
Molder	|X|G
Instant
Destroy target artifact or enchantment with converted mana cost X. It can't be regenerated. You gain X life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td1, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td1, "DISENCHANT", 1, NULL);
		if( spell_fizzled != 1 ){
			int amount = get_cmc(instance->targets[0].player, instance->targets[0].card);
			int diff = true_get_updated_casting_cost(player, card, -1, event, -1);
			charge_mana(player, COLOR_COLORLESS, amount + diff);
			if( spell_fizzled != 1 ){
				instance->info_slot = amount;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			gain_life(player, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_momentary_blink(int player, int card, event_t event){
/* CARD_ID_MOMENTARY_BLINK	8611
Momentary Blink	|1|W
Instant
Exile target creature you control, then return it to the battlefield under its owner's control.
Flashback |3|U (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XU(3, 1));
	}

	return card_cloudshift(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20f6010; original cards/time_spiral.c:4059. */
int card_moonlace(int player, int card, event_t event)
{
  /* Moonlace	|U
   * Instant
   * Target spell or permanent becomes colorless. */

  int result;

  result = lace(player, card, event, 1);
  return result;
}
#endif

int card_mwonvuli_acid_moss(int player, int card, event_t event){
/* CARD_ID_MWONVULI_ACID_MOSS	8613
Mwonvuli Acid-Moss	|2|G|G
Sorcery
Destroy target land. Search your library for |Ha Forest card and put that card onto the battlefield tapped. Then shuffle your library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_LAND);

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			test_definition_t test;
			default_test_definition(&test, TYPE_LAND);
			test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
			scnprintf(test.message, 100, "%s", get_hacked_land_text(player, card, "Select a %s card.", SUBTYPE_FOREST));
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY_TAPPED, 0, AI_MAX_VALUE, &test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_LAND", 1, NULL);

}

static int is_instant_or_has_flash(int iid, int has_teferi, int unused1, int unused2){
	/* Approximation, but it works as the current implementation of "flash" requires that the card has TYPE_INSTANT or TYPE_INTERRUPT plus its other types.
	 * Deliberately don't use is_what(), which ignores the TYPE_INSTANT bit for cards with any permanent types. */
	return cards_data[iid].type & (TYPE_INSTANT | TYPE_INTERRUPT | (has_teferi ? TYPE_CREATURE : 0));
}

int card_mystical_teachings(int player, int card, event_t event){
/* CARD_ID_MYSTICAL_TEACHINGS	8614
Mystical Teachings	|3|U
Instant
Search your library for an instant card or a card with flash, reveal it, and put it into your hand. Then shuffle your library.
Flashback |5|B (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XB(5, 1));
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( current_turn == 1-player ){
			ai_modifier += 24;
		}
		if( count_subtype(player, TYPE_LAND, -1) >= 6 ){
			ai_modifier += 24;
		}
	}

	if(event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select an instant or a card with flash.");
		this_test.special_selection_function = &is_instant_or_has_flash;
		this_test.value_for_special_selection_function = check_battlefield_for_id(player, CARD_ID_TEFERI_MAGE_OF_ZHALFIR);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		if( get_flashback(player, card) ){
			  kill_card(player, card, KILL_EXILE);
		}
		else{
			 kill_card(player, card, KILL_DESTROY);
		}
	}

	return basic_spell(player, card, event);
}

int card_nantuko_shaman(int player, int card, event_t event){

	/* Nantuko Shaman	|2|G
	 * Creature - Insect Shaman 3/2
	 * When ~ enters the battlefield, if you control no tapped lands, draw a card.
	 * Suspend 1-|2|G|G */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.state = STATE_TAPPED;
		if( ! check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			draw_cards(player, 1);
		}
	}

	return suspend(player, card, event, 1, MANACOST_XG(2, 2), NULL, NULL);
}

int card_nether_traitor(int player, int card, event_t event){
/* CARD_ID_NETHER_TRAITOR	8616
Nether Traitor	|B|B
Creature - Spirit 1/1
Haste
Shadow (This creature can block or be blocked by only creatures with shadow.)
Whenever another creature is put into your graveyard from the battlefield, you may pay |B. If you do, return ~ from your graveyard to the battlefield. */

	haste(player, card, event);
	shadow(player, card, event);
	return 0;
}

int card_nightshade_assassin(int player, int card, event_t event){
/* CARD_ID_NIGHTSHADE_ASSASSIN	8617
Nightshade Assassin	|2|B|B
Creature - Human Assassin 2/1
First strike
When ~ enters the battlefield, you may reveal X |Sblack cards in your hand. If you do, target creature gets -X/-X until end of turn.
Madness |1|B (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td1) && new_pick_target(&td1, "TARGET_CREATURE", 0, 0) ){
			int amount = reveal_any_number_of_cards_of_selected_color(player, card, COLOR_TEST_BLACK);
			if( amount ){
				pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -amount, -amount);
			}
		}
		instance->number_of_targets = 0;
	}
	return madness(player, card, event, MANACOST_XB(1, 1));
}

int card_norin_the_wary(int player, int card, event_t event)
{
  /* CARD_ID_NORIN_THE_WARY	8618
Norin the Wary	|R
Legendary Creature - Human Warrior 2/1
When a player casts a spell or a creature attacks, exile ~. Return it to the battlefield under its owner's control at the beginning of the next end step. */

  check_legend_rule(player, card, event);
  sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
  store_attackers(player, card, event, 0, ANYBODY, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_SPA || event == EVENT_DAT_ABILITY)
    remove_until_eot(player, card, player, card);
  return 0;
}

int card_opaline_sliver(int player, int card, event_t event)
{
/* CARD_ID_OPALINE_SLIVER	8620
Opaline Sliver	|1|W|U
Creature - Sliver 2/2
All Slivers have "Whenever this permanent becomes the target of a spell an opponent controls, you may draw a card." */

  const target_t* targets = any_becomes_target(player, card, event, player, 0, TYPE_PERMANENT, SUBTYPE_SLIVER, TYPE_NONEFFECT, 1-player, RESOLVE_TRIGGER_AI(player));
  if (targets)
	{
	  int num = 0;
	  for (; targets->player != -1; ++targets)
		++num;

	  if (num == 1)
		draw_cards(player, 1);
	  else
		draw_up_to_n_cards(player, num);
	}

  return slivercycling(player, card, event);
}

int card_outrider_en_kor(int player, int card, event_t event){
/* CARD_ID_OUTRIDER_EN_KOR	8623
Outrider en-Kor	|2|W
Creature - Kor Rebel Knight 2/2
Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)
|0: The next 1 damage that would be dealt to ~ this turn is dealt to target creature you control instead. */

	flanking(player, card, event);
	return en_kor_damage_prevention_ability(player, card, event);
}

int card_paradise_plume(int player, int card, event_t event){

	/* Paradise Plume	|4
	 * Artifact
	 * As ~ enters the battlefield, choose a color.
	 * Whenever a player casts a spell of the chosen color, you may gain 1 life.
	 * |T: Add one mana of the chosen color to your mana pool. */

	if (event == EVENT_RESOLVE_SPELL){
		int color = choose_a_color_and_show_legacy(player, card, player, -1);
		get_card_instance(player, card)->info_slot = 1u << (color & 31);
	}
	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_card_instance(player, card)->info_slot;
		sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_AI(player), TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){ gain_life(player, 1); }
	return mana_producer(player, card, event);
}

int card_paradox_haze(int player, int card, event_t event){
/* CARD_ID_PARADOX_HAZE	8625
Paradox Haze	|2|U
Enchantment - Aura
Enchant player
At the beginning of enchanted player's first upkeep each turn, that player gets an additional upkeep step after this step. */

	if( ! IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;
	return enchant_player(player, card, event, &td);
}

int card_pardic_dragon(int player, int card, event_t event)
{
  /* Pardic Dragon	|4|R|R
	 * Creature - Dragon 4/4
	 * Flying
	 * |R: ~ gets +1/+0 until end of turn.
	 * Suspend 2-|R|R
	 * Whenever an opponent casts a spell, if ~ is suspended, that player may put a time counter on ~. */

  if (event == EVENT_POW_BOOST)
    // Native AI estimate uses green mana and a +1/+1 pump.
    return generic_shade(player, card, event, 0, MANACOST_G(1), 1, 1, 0, 0);
  suspend(player, card, event, 2, MANACOST_R(2), NULL, NULL);
  return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1, 0);
}

int card_pendelhaven_elder(int player, int card, event_t event){
/* CARD_ID_PENDELHAVEN_ELDER	8627
Pendelhaven Elder	|1|G
Creature - Elf Shaman 1/1
|T: Each 1/1 creature you control gets +1/+2 until end of turn. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.power = 1;
		this_test.toughness = 1;

		int count = active_cards_count[player]-1;
		while( count > -1 ){
				if( in_play(player, count) && new_make_test_in_play(player, count, -1, &this_test) ){
					pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card, player, count, 1, 2);
				}
				count--;
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_pentarch_paladin(int player, int card, event_t event){
/* CARD_ID_PENTARCH_PALADIN	8628
Pentarch Paladin	|2|W|W|W
Creature - Human Knight 3/3
Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)
As ~ enters the battlefield, choose a color.
|W|W, |T: Destroy target permanent of the chosen color. */

	flanking(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = choose_a_color_and_show_legacy(player, card, 1-player, -1);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.required_color = 1 << instance->info_slot;

	if( event == EVENT_ACTIVATE ){
		char msg[100];
		scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select target %s permanent.", instance->info_slot));
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_W(2), 0,
										&td, msg);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(2), 0, &td, NULL);
}

int card_penumbra_spider(int player, int card, event_t event){
	/* Penumbra Spider	|2|G|G
	 * Creature - Spider 2/4
	 * Reach
	 * When ~ dies, put a 2/4 |Sblack Spider creature token with reach onto the battlefield. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SPIDER, &token);
		token.pow = 2;
		token.tou = 4;
		token.color_forced = COLOR_TEST_BLACK;
		generate_token(&token);
	}
	return 0;
}

int card_phantom_wurm(int player, int card, event_t event){
	/* Phantom Wurm	|4|G|G
	 * Creature - Wurm Spirit 2/0
	 * ~ enters the battlefield with four +1/+1 counters on it.
	 * If damage would be dealt to ~, prevent that damage. Remove a +1/+1 counter from ~. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 4);

	phantom_effect(player, card, event, 0);

	return 0;
}

int card_phthisis(int player, int card, event_t event)
{
  /* Phthisis	|3|B|B|B|B
   * Sorcery
   * Destroy target creature. Its controller loses life equal to its power plus its toughness.
   * Suspend 5-|1|B */

	if( ! IS_GS_EVENT(player, card, event) && event != EVENT_MODIFY_COST ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  int amount = (get_power(instance->targets[0].player, instance->targets[0].card) +
						get_toughness(instance->targets[0].player, instance->targets[0].card));
		  kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		  lose_life(instance->targets[0].player, amount);
		}

	  kill_card(player, card, KILL_DESTROY);
	}

  return suspend(player, card, event, 5, MANACOST_XB(1,1), &td, "TARGET_CREATURE");
}

int card_phyrexian_totem(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_TOTEM	8633
Phyrexian Totem	|3
Artifact
|T: Add |B to your mana pool.
|2|B: ~ becomes a 5/5 |Sblack Horror artifact creature with trample until end of turn.
Whenever ~ is dealt damage, if it's a creature, sacrifice that many permanents. */

	if( is_what(player, card, TYPE_CREATURE) && ! is_humiliated(player, card, AL_TRIG) ){
		true_phyrexian_negator(player, card, event);
	}

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_ANIMATE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_XB(2, 2), 0, NULL, NULL) ){
			return 1;
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, mana_producer(player, card, EVENT_CAN_ACTIVATE), 0};
		if( ! paying_mana() ){
			int fake_tap = 0;
			if( IS_AI(player) && ! is_tapped(player, card) ){
				add_state(player, card, STATE_TAPPED);
				fake_tap = 1;
			}
			abils[2] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_TYPE_CHANGE, MANACOST_XB(2, 1), 0, NULL, NULL);
			if( IS_AI(player) && fake_tap){
				remove_state(player, card, STATE_TAPPED);
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Get mana", abils[1], 1,
							"Animate", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		int fake_tap = 0;
		if( IS_AI(player) && ! is_tapped(player, card) ){
			add_state(player, card, STATE_TAPPED);
			fake_tap = 1;
		}
		if( choice == CHOICE_ANIMATE ){
			return generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_XB(2, 1), 0, NULL, NULL);
		}
		if( IS_AI(player) && fake_tap){
			remove_state(player, card, STATE_TAPPED);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_ANIMATE ){
			int legacy = artifact_animation(instance->parent_controller, instance->parent_card,
											instance->parent_controller, instance->parent_card, 1, 5, 5, KEYWORD_TRAMPLE, 0);
			card_instance_t *leg = get_card_instance(instance->parent_controller, legacy);
			int color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
			leg->targets[9].card = SUBTYPE_HORROR;
			leg->targets[6].player = color;
		}
	}

	return 0;
}

int card_pit_keeper(int player, int card, event_t event){
/* CARD_ID_PIT_KEEPER	8634
Pit Keeper	|1|B
Creature - Human Wizard 2/1
When ~ enters the battlefield, if you have four or more creature cards in your graveyard, you may return target creature card from your graveyard to your hand.*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) >= 4 && ! graveyard_has_shroud(player) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

int card_plague_sliver(int player, int card, event_t event){
/* CARD_ID_PLAGUE_SLIVER	8635
Plague Sliver	|2|B|B
Creature - Sliver 5/5
All Slivers have "At the beginning of your upkeep, this permanent deals 1 damage to you." */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int count = count_subtype(current_turn, TYPE_PERMANENT, SUBTYPE_SLIVER);
		if( count > 0 ){
			damage_player(current_turn, count, player, card);
		}
	}
	return slivercycling(player, card, event);
}

int card_plated_pegasus(int player, int card, event_t event){
	/* Plated Pegasus	|2|W	0x2003013
	 * Creature - Pegasus 1/2
	 * Flash
	 * Flying
	 * If a spell would deal damage to a permanent or player, prevent 1 damage that spell would deal to that permanent or player. */
	card_benevolent_unicorn(player, card, event);

	return flash(player, card, event);
}

int card_primal_forcemage(int player, int card, event_t event){
/* CARD_ID_PRIMAL_FORCEMAGE	8639
Primal Forcemage	|2|G
Creature - Elf Shaman 2/2
Whenever another creature enters the battlefield under your control, that creature gets +3/+3 until end of turn. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		pump_until_eot_merge_previous(player, card, trigger_cause_controller, trigger_cause, 3, 3);
	}

	return 0;
}

int card_prismatic_lens(int player, int card, event_t event){
/* CARD_ID_PRISMATIC_LENS	8640
Prismatic Lens	|2
Artifact
|T: Add |C to your mana pool.
|1, |T: Add one mana of any color to your mana pool. */

	if (event == EVENT_RESOLVE_SPELL){
	    get_card_instance(player, card)->info_slot = COLOR_TEST_COLORLESS;
	    play_land_sound_effect_force_color(player, card, COLOR_TEST_ANY_COLORED);
	    count_mana();
	}
	if (event == EVENT_CHANGE_TYPE){
	    int colors = COLOR_TEST_ANY;
	    if (!has_mana(player, COLOR_COLORLESS, 2)){
	        colors = COLOR_TEST_COLORLESS;
	        if (is_what(player, card, TYPE_LAND)){ colors |= mana_colors_added[player]; }
	    }
	    get_card_instance(player, card)->info_slot = colors;
	}
	if (event == EVENT_COUNT_MANA && affect_me(player, card) && !is_tapped(player, card)
	    && !is_animated_and_sick(player, card) && can_produce_mana(player, card)){
	    if (has_mana(player, COLOR_ANY, 2)){ declare_mana_available_hex(player, COLOR_TEST_ANY, 1); }
	    else{ declare_mana_available(player, COLOR_COLORLESS, 1); }
	}
	if (event == EVENT_CAN_ACTIVATE){
	    return get_card_instance(player, card)->targets[1].player != 66 && mana_producer(player, card, event);
	}
	if (event == EVENT_ACTIVATE){
	    if (get_card_instance(player, card)->targets[1].player == 66){ return 0; }
	    int choice = COLOR_COLORLESS;
	    if (has_mana(player, COLOR_COLORLESS, 2)){
	        choice = choose_mana_all_one_color(player, COLOR_TEST_ANY, 1, "What kind of mana?", COLOR_TEST_COLORLESS);
	    } else if (is_what(player, card, TYPE_LAND) && mana_colors_added[player]){
	        choice = choose_a_color_exe(player, "What kind of mana?", 1, COLOR_COLORLESS, mana_colors_added[player] | COLOR_TEST_COLORLESS);
	    }
	    if (choice == -1){ spell_fizzled = 1; return 0; }
	    int chosen_color_mask = color_to_color_test(choice);
	    int free_colors = COLOR_TEST_COLORLESS;
	    if (is_what(player, card, TYPE_LAND)){ free_colors |= mana_colors_added[player]; }
	    if (!(chosen_color_mask & free_colors)){
	        get_card_instance(player, card)->targets[1].player = 66;
	        charge_mana(player, COLOR_COLORLESS, 1);
	        get_card_instance(player, card)->targets[1].player = 0;
	        if (spell_fizzled == 1){ return 0; }
	    }
	    produce_mana_tapped_impl(player, card, choice, 1, 0, 1);
	}
	return 0;
}

int psionic_sliver_shared_ability(int player, int card, event_t event)
{
  int p;
  int source_card_or_player;
  card_instance_t *instance;
  card_instance_t *source_in_play;
  int status_or_source_card;
  target_definition_t td2;

  instance = get_card_instance(player, card);
  p = instance->targets[1].player;
  if (p < 0)
  {
    return 0;
  }
  source_card_or_player = instance->targets[1].card;
  leaves_play(p, source_card_or_player, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    add_status(player, card, 0x10000);
    in_play(p, source_card_or_player);
    is_humiliated(p, source_card_or_player, AL_NM_NL_ACT);
    return 0;
  }
  source_in_play = in_play(p, source_card_or_player);
  if (!source_in_play && event == EVENT_CAN_ACTIVATE)
    return 0;
  if (!source_in_play && event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);
  status_or_source_card = is_humiliated(p, source_card_or_player, AL_NM_NL_ACT);
  if (status_or_source_card != 0)
    return 0;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td2, 0x102);
  td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (status_or_source_card = would_validate_arbitrary_target(&td2, instance->targets[2].player,
                                                               instance->targets[2].card),
       status_or_source_card != 0))
  {
    damage_creature(instance->targets[2].player, instance->targets[2].card, 2,
                    instance->targets[0].player, instance->targets[0].card);
    status_or_source_card = instance->targets[0].card;
    source_card_or_player = instance->targets[0].player;
    damage_creature(source_card_or_player, status_or_source_card, 3, source_card_or_player,
                    status_or_source_card);
  }
  return shared_sliver_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td2,
                                         "TARGET_ANY");
}

int card_psionic_sliver(int player, int card, event_t event){
	/* Psionic Sliver	|4|U	0x2002307
	 * Creature - Sliver 2/2
	 * All Sliver creatures have "|T: This creature deals 2 damage to any target and 3 damage to itself." */
	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &psionic_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &psionic_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int card_psychotic_episode(int player, int card, event_t event){
/* CARD_ID_PSYCHOTIC_EPISODE	8642
Psychotic Episode	|1|B|B
Sorcery
Target player reveals his or her hand and the top card of his or her library. You choose a card revealed this way.
That player puts the chosen card on the bottom of his or her library.
Madness |1|B (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			reveal_target_player_hand(instance->targets[0].player);
			int *deck = deck_ptr[instance->targets[0].player];
			show_deck( player, deck, 1, "Here's the first card of target player", 0, gs_done_008b40e0 );
			int ai_choice = 0;
			if( hand_count[instance->targets[0].player] < 1 ){
				ai_choice = 0;
			}
			int choice = do_dialog(player, player, card, -1, -1, " From hand\n From deck", ai_choice);
			if( choice == 0  ){
				ec_definition_t ec;
				default_ec_definition(instance->targets[0].player, player, &ec);
				ec.effect = EC_PUT_ON_BOTTOM;

				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_ANY);
				new_effect_coercion(&ec, &this_test);
			}
			else{
				put_top_x_on_bottom(player, instance->targets[0].player, 1);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_pull_from_eternity(int player, int card, event_t event){
/* CARD_ID_PULL_FROM_ETERNITY	8643
Pull from Eternity	|W
Instant
Put target face-up exiled card into its owner's graveyard. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if( count_rfg(instance->targets[0].player) > 0 ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_ANY);
				new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_RFG, TUTOR_GRAVE, 0, AI_MAX_VALUE, &this_test);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

static int pulmonic_sliver_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_PULMONIC_SLIVER ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(affected_card_controller, affected_card) ){
			int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
			if( kill_code ){
				if( instance->damage_target_player > -1 ){
					int p = instance->damage_target_player;
					int c = instance->damage_target_card;
					if( affect_me(p, c) ){
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
						if( instance->targets[11].player < 0 ){
							instance->targets[11].player = 0;
						}
						SET_BYTE1(instance->targets[11].player) = 1;
					}
				}
				test_definition_t test;
				default_test_definition(&test, TYPE_PERMANENT);
				test.subtype = SUBTYPE_SLIVER;
				count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, 0, &test, GFPC_TRACK_DEAD_CREATURES, 1);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			card_instance_t *instance = get_card_instance(player, card);
			int i;
			for(i=1; i<10; i++){
				int owner = instance->targets[i].player;
				if( owner > -1 ){
					int csvid = cards_data[instance->targets[i].card].id;
					instance->targets[i].player = instance->targets[i].card = -1;
					seek_grave_for_id_to_reanimate(owner, -1, owner, csvid, REANIMATEXTRA_RETURN_TO_HAND2);
				}
			}
			kill_card(player, card, KILL_EXILE);
		}
	}

	//Reset counts if the legacy is still invisible
	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && event == EVENT_END_TRIGGER ){
		get_card_instance(player, card)->targets[11].player = 0;
	}

	if( event == EVENT_CLEANUP && ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_pulmonic_sliver(int player, int card, event_t event)
{
  /* Pulmonic Sliver	|3|W|W
   * Creature - Sliver 3/3
   * All Sliver creatures have flying.
   * All Slivers have "If this permanent would be put into a graveyard, you may put it on top of its owner's library instead." */


	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (intptr_t)pulmonic_sliver_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &pulmonic_sliver_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_SLIVER;
		count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, 0, &test, GFPC_TRACK_DEAD_CREATURES, 0);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			card_instance_t *instance = get_card_instance(player, card);
			int amount = BYTE0(result);
			int i;
			for(i=0; i<amount; i++){
				int owner = instance->targets[i].player;
				int csvid = cards_data[instance->targets[i].card].id;
				instance->targets[i].player = instance->targets[i].card = -1;
				seek_grave_for_id_to_reanimate(owner, -1, owner, csvid, REANIMATEXTRA_RETURN_TO_HAND2);
			}
		}
	}

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 0, 0, KEYWORD_FLYING, BCT_INCLUDE_SELF);

	return slivercycling(player, card, event);
}

int quilled_sliver_shared_ability(int player, int card, event_t event)
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
  default_target_definition(player, card, &td2, 2);
  td2.required_state = TARGET_STATE_IN_COMBAT;
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = would_validate_arbitrary_target(&td2, instance->targets[2].player,
                                                instance->targets[2].card),
       result != 0))
  {
    damage_creature(instance->targets[2].player, instance->targets[2].card, 1,
                    instance->targets[0].player, instance->targets[0].card);
  }
  return shared_sliver_activated_ability(player, card, event, 7, 0, 0, 0, 0, 0, 0, 0, &td2,
                                         "Select target attacking or blocking creature.");
}

int card_quilled_sliver(int player, int card, event_t event){
  /* Quilled Sliver |W
   * 	  Creature - Sliver 1/1
   * 	  All Slivers have "{T}: This permanent deals 1 damage to target attacking or blocking creature." */

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &quilled_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &quilled_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int card_reiterate(int player, int card, event_t event)
{
/* CARD_ID_REITERATE	1431
Reiterate	|1|R|R
Instant
Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
Copy target instant or sorcery spell. You may choose new targets for the copy. */

  // original code : 004DDEEF

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	instance->info_slot = buyback(player, card, 3, 0, 0, 0, 0, 0);

  int rval = twincast(player, card, event, NULL, &instance->targets[2].card);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (instance->info_slot == 1 && instance->targets[2].card != -1)	// i.e. it wasn't countered by having no valid targets
		bounce_permanent(player, card);
	  else
		kill_card(player, card, KILL_DESTROY);
	}

  return rval;
}

int card_restore_balance(int player, int card, event_t event)
{
  /* Restore Balance	""
   * Sorcery
   * Suspend 6-|W
   * Each player chooses a number of lands he or she controls equal to the number of lands controlled by the player who controls the fewest,
   * then sacrifices the rest. Players sacrifice creatures and discard cards the same way. */

	if (event == EVENT_RESOLVE_SPELL){
		int diff[2][3];
		APNAP(p, {diff[p][0] = count_subtype(p, TYPE_LAND, -1)-count_subtype(1-p, TYPE_LAND, -1);};);
		APNAP(p, {diff[p][1] = hand_count[p]-hand_count[1-p];};);
		APNAP(p, {diff[p][2] = count_subtype(p, TYPE_CREATURE, -1)-count_subtype(1-p, TYPE_CREATURE, -1);};);

		int land_to_sac = diff[player][0] > diff[1-player][0] ? diff[player][0] : diff[1-player][0];
		if( land_to_sac ){
			int sac_player = diff[player][0] > diff[1-player][0] ? player : 1-player;
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
			while( land_to_sac ){
					new_sacrifice(player, card, sac_player, SAC_JUST_MARK, &test);
					land_to_sac--;
			}
			int c;
			for(c=active_cards_count[sac_player]-1; c>-1; c--){
				if( in_play(sac_player, c) && is_what(sac_player, c, TYPE_LAND) && check_state(sac_player, c, STATE_CANNOT_TARGET) ){
					kill_card(sac_player, c, KILL_SACRIFICE);
				}
			}
		}

		int crit_to_sac = diff[player][2] > diff[1-player][2] ? diff[player][2] : diff[1-player][2];
		if( crit_to_sac ){
			int sac_player = diff[player][2] > diff[1-player][2] ? player : 1-player;
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			while( crit_to_sac ){
					new_sacrifice(player, card, sac_player, SAC_JUST_MARK, &test);
					crit_to_sac--;
			}
			int c;
			for(c=active_cards_count[sac_player]-1; c>-1; c--){
				if( in_play(sac_player, c) && is_what(sac_player, c, TYPE_CREATURE) && check_state(sac_player, c, STATE_CANNOT_TARGET) ){
					kill_card(sac_player, c, KILL_SACRIFICE);
				}
			}
		}

		int cards_to_discard = diff[player][1] > diff[1-player][1] ? diff[player][1] : diff[1-player][1];
		if( cards_to_discard ){
			int sac_player = diff[player][1] > diff[1-player][1] ? player : 1-player;
			new_multidiscard(sac_player, diff[sac_player][1], 0, player);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return suspend_only(player, card, event, 6, MANACOST_W(1));
}

int card_return_to_dust(int player, int card, event_t event){
/* CARD_ID_RETURN_TO_DUST	8647
Return to Dust	|2|W|W
Instant
Exile target artifact or enchantment. If you cast this spell during your main phase, you may exile up to one other target artifact or enchantment. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( pick_target(&td, "DISENCHANT") ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			if( can_target(&td) && current_turn == player && (current_phase == PHASE_MAIN1 || current_phase == PHASE_MAIN2) ){
				select_target(player, card, &td, "Select target artifact or enchantment.", &(instance->targets[1]));
			}
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_EXILE);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_rift_bolt(int player, int card, event_t event)
{
	/* Rift Bolt	|2|R	0x20070af
	 * Sorcery
	 * ~ deals 3 damage to any target.
	 * Suspend 1-|R */
	if( ! IS_GS_EVENT(player, card, event) && event != EVENT_MODIFY_COST ){
		return 0;
	}
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (valid_target(&td))
		damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, card);

	  kill_card(player, card, KILL_DESTROY);
	}

  return suspend(player, card, event, 1, MANACOST_R(1), &td, "TARGET_ANY");
}

// By jatill

int card_riftwing_cloudskate(int player, int card, event_t event){
	/* Riftwing Cloudskate	|3|U|U
	 * Creature - Illusion 2/2
	 * Flying
	 * When ~ enters the battlefield, return target permanent to its owner's hand.
	 * Suspend 3-|1|U */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
			bounce_permanent( instance->targets[0].player, instance->targets[0].card );
		}
		instance->number_of_targets = 0;
	}

	return suspend(player, card, event, 3, MANACOST_XU(1, 1), NULL, NULL);
}

static int saffi_legacy(int player, int card, event_t event)
{

  if (get_card_instance(player, card)->damage_target_player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);

    int p = instance->damage_target_player;
    int c = instance->damage_target_card;
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {
      if (affect_me(p, c) && is_this_dying(p, c) && !check_for_death_trigger_removal(p, c))
      {
        instance->targets[0].player = get_owner(p, c);
        instance->targets[0].card = get_original_internal_card_id(p, c);
        instance->number_of_targets = 0;
        instance->damage_target_player = instance->damage_target_card = -1;
        instance->targets[11].player = 66;
      }
    }
  }

  if (resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int owner = instance->targets[0].player;
    int iid = instance->targets[0].card;
    seek_grave_for_id_to_reanimate(owner, -1, owner, cards_data[iid].id, REANIMATE_DEFAULT);
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_saffi_eriksdotter(int player, int card, event_t event){
  /* Saffi Eriksdotter |W|G
   * Legendary Creature — Human Scout 2/2
   * Sacrifice Saffi Eriksdotter: When target creature is put into your graveyard from the battlefield this turn, return that card to
   * 	the battlefield. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.allowed_controller = player;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &saffi_legacy, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_sage_of_epityr(int player, int card, event_t event){
/* CARD_ID_SAGE_OF_EPITYR	8651
Sage of Epityr	|U
Creature - Human Wizard 1/1
When ~ enters the battlefield, look at the top four cards of your library, then put them back in any order. */

// 0x200f589
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		rearrange_top_x(player, player, 4);
	}

	return 0;
}

int card_sangrophage(int player, int card, event_t event){
/* CARD_ID_SANGROPHAGE	8653
Sangrophage	|B|B
Creature - Zombie 3/3
At the beginning of your upkeep, tap ~ unless you pay 2 life. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int to_tap = 1;

		if( can_pay_life(player, 2) ){
			int ai_choice = 0;
			if(life[player] < 7){
			   ai_choice = 1;
			}

			int choice = do_dialog(player, player, card, -1, -1, " Pay 2 life\n Tap this", ai_choice);
			if( choice == 0){
				lose_life(player, 2);
				to_tap = 0;
			}
		}

		if( to_tap == 1 ){
			tap_card(player, card);
		}
	}

	return 0;
}

static int sarpadian_empires_legacy(int player, int card, event_t event){

	if (event == EVENT_SET_LEGACY_EFFECT_NAME ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			char clr[10];
			scnprintf(clr, 10, "%s", cards_ptr[get_card_instance(instance->damage_target_player, instance->damage_target_card)->targets[2].player]->name);
			char subt[20];
			scnprintf(subt, 20, "%s", raw_get_subtype_text(get_card_instance(instance->damage_target_player, instance->damage_target_card)->targets[2].card));
			scnprintf(set_legacy_effect_name_addr, 51, "%s %s", clr, subt);
		}
	}

	return 0;
}

int card_sarpadian_empires_vol_vii(int player, int card, event_t event){

	/* Sarpadian Empires, Vol. VII	|3
	 * Artifact
	 * As ~ enters the battlefield, choose |Swhite Citizen, |Sblue Camarid, |Sblack Thrull, |Sred Goblin, or |Sgreen Saproling.
	 * |3, |T: Put a 1/1 creature token of the chosen color and type onto the battlefield. */


	if( event == EVENT_RESOLVE_SPELL ){
		int clrs[5] = {COLOR_TEST_BLACK, COLOR_TEST_BLUE, COLOR_TEST_GREEN, COLOR_TEST_RED, COLOR_TEST_WHITE};
		int ids[5] = {CARD_ID_THRULL, CARD_ID_CAMARID, CARD_ID_SAPROLING, CARD_ID_GOBLIN, CARD_ID_CITIZEN};
		int subs[5] = {SUBTYPE_THRULL, SUBTYPE_CAMARID, SUBTYPE_SAPROLING, SUBTYPE_GOBLIN, SUBTYPE_CITIZEN};
		int choice = do_dialog(player, player, card, -1, -1, " Black Thrull\n Blue Camarid\n Green Saproling\n Red Goblin\n White Citizen", internal_rand(5));
		card_instance_t *instance = get_card_instance(player, card);
		instance->targets[1].player = clrs[choice];
		instance->targets[2].player = CARD_ID_BLACK+choice;
		instance->targets[1].card = ids[choice];
		instance->targets[2].card = subs[choice];
		int legacy_card = create_targetted_legacy_effect(player, card, &sarpadian_empires_legacy, player, card);
		get_card_instance(player, legacy_card)->eot_toughness = EVENT_SET_LEGACY_EFFECT_NAME;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		token_generation_t token;
		default_token_definition(player, card, instance->targets[1].card, &token);
		token.color_forced = instance->targets[1].player;
		token.pow = token.tou = 1;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
}

int card_savage_thallid(int player, int card, event_t event){

	/* Savage Thallid	|3|G|G
	 * Creature - Fungus 5/2
	 * At the beginning of your upkeep, put a spore counter on ~.
	 * Remove three spore counters from ~: Put a 1/1 |Sgreen Saproling creature token onto the battlefield.
	 * Sacrifice a Saproling: Regenerate target Fungus. */

	add_spore_counters(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_DESTROYED;
	td.required_subtype = SUBTYPE_FUNGUS;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Saproling to sacrifice.");
	test.subtype = SUBTYPE_SAPROLING;

	enum{
		CHOICE_SAPROLING = 1,
		CHOICE_REGENERATE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( land_can_be_played & LCBP_REGENERATION ){
			if( generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL, 0, &test, 0) ){
				return 0x63;
			}
		}
		return can_make_saproling_from_fungus(player, card);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int abils[3] = {0, can_make_saproling_from_fungus(player, card), 0};
		if( land_can_be_played & LCBP_REGENERATION ){
			if( generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL, 0, &test, 0) ){
				abils[2] = 1;
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Create a Saproling", abils[1], 1,
							"Regenerate target Fungus", abils[2], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_SAPROLING ){
			saproling_from_fungus(player, card);
		}
		if( choice == CHOICE_REGENERATE ){
			return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
																	&td, "Select target Fungus to regenerate.", 0, &test, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAPROLING ){
			generate_token_by_id(player, card, CARD_ID_SAPROLING);
		}
		if( instance->info_slot == CHOICE_REGENERATE && valid_target(&td) &&
			can_be_regenerated(instance->targets[0].player, instance->targets[0].card) )
		{
			regenerate_target(instance->targets[0].player, instance->targets[0].card);
		}
	}


	return 0;
}

int card_scion_of_the_ur_dragon(int player, int card, event_t event)
{
/* Scion of the Ur-Dragon	|W|U|B|R|G
 * Legendary Creature - Dragon Avatar 4/4
 * Flying
 * |2: Search your library for a Dragon permanent card and put it into your graveyard. If you do, ~ becomes a copy of that card until end
 * 		of turn. Then shuffle your library. */

	check_legend_rule(player, card, event);

	if (event == EVENT_RESOLVE_ACTIVATION)
	{
		card_instance_t* instance = get_card_instance(player, card);
		int pp = instance->parent_controller;
		int pc = instance->parent_card;

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_DRAGON;

		int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_GRAVE, 0, AI_MAX_CMC, &this_test);

		if (result >= 0 && in_play(pp, pc))
			shapeshift_target(pp, pc, pp, pc, player, result, SHAPESHIFT_UNTIL_EOT);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
}

int screeching_sliver_shared_ability(int player, int card, event_t event)
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
    mill(instance->targets[2].player, 1);
  }
  return shared_sliver_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td2,
                                         "TARGET_PLAYER");
}

int card_screeching_sliver(int player, int card, event_t event){
  /* Screeching Sliver |U
   * 	  Creature - Sliver 1/1
   * 	  All Slivers have "{T}: Target player puts the top card of his or her library into his or her graveyard." */

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &screeching_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &screeching_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int card_scryb_ranger(int player, int card, event_t event){
/* CARD_ID_SCRYB_RANGER	8659
Scryb Ranger	|1|G
Creature - Faerie 1/1
Flash (You may cast this spell any time you could cast an instant.)
Flying, protection from |Sblue
Return |Ha Forest you control to its owner's hand: Untap target creature. Activate this ability only once each turn. */

	if (flash(player, card, event)){
		return 1;
	}

	return card_quirion_ranger(player, card, event);
}

int card_search_for_tomorrow(int player, int card, event_t event){

	/* Search for Tomorrow	|2|G
	 * Sorcery
	 * Search your library for a basic land card and put it onto the battlefield. Then shuffle your library.
	 * Suspend 2-|G */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int total = get_total_available_lands(player);
		if( total <= 3 || total >= 6 ){
			ai_modifier += 48;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, 4, TYPE_LAND, 0, SUBTYPE_BASIC, 0, 0, 0, 0, 0, -1, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return suspend(player, card, event, 2, MANACOST_G(1), NULL, NULL);
}

int sedge_sliver_shared_ability(int player, int card, event_t event)
{
  int p;
  card_instance_t *instance;
  int ability_available;
  int scan_player_or_source_card;
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
    ability_available = is_humiliated(p, c, AL_NM_NL_ACT);
    if (ability_available == 0)
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
          scan_player_or_source_card = 0;
          do
          {
            c = 0;
            if (0 < active_cards_count[scan_player_or_source_card])
            {
              do
              {
                instance = in_play(scan_player_or_source_card, c);
                if ((((instance != NULL) &&
                      (ability_available = is_what(scan_player_or_source_card, c, 0x47),
                       ability_available != 0)) &&
                     (ability_available =
                          has_subtype(scan_player_or_source_card, c, SUBTYPE_SLIVER),
                      ability_available != 0)) &&
                    (ability_available = regeneration(scan_player_or_source_card, c,
                                                      EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0),
                     ability_available != 0))
                {
                  return ability_available;
                }
                c = c + 1;
              } while (c < active_cards_count[scan_player_or_source_card]);
            }
            scan_player_or_source_card = scan_player_or_source_card + 1;
            if (scan_player_or_source_card == 2)
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
      scan_player_or_source_card = instance->targets[0].card;
      instance->number_of_targets = '\x01';
      if (can_use_activated_abilities(instance->targets[0].player, scan_player_or_source_card) == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      charge_mana_for_activated_ability(instance->targets[0].player, instance->targets[0].card, 0,
                                        1, 0, 0, 0, 0);
      return 0;
    }
  }
  return 0;
}

int card_sedge_sliver(int player, int card, event_t event){
  /* Sedge Sliver |2|R
   * 	  Creature - Sliver 2/2
   * 	  All Sliver creatures have "This creature gets +1/+1 as long as you control a Swamp."
   * 	  All Slivers have "{B}: Regenerate this permanent." */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && ! is_humiliated(player, card, AL_STATIC) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) && has_subtype(affected_card_controller, affected_card, SUBTYPE_SLIVER) ){
			if( check_battlefield_for_subtype(affected_card_controller, TYPE_LAND, SUBTYPE_SWAMP) ){
				event_result++;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &sedge_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &sedge_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}
	return slivercycling(player, card, event);
}

int card_sengir_nosferatu(int player, int card, event_t event){
/* CARD_ID_SENGIR_NOSFERATU	8662
Sengir Nosferatu	|3|B|B
Creature - Vampire 4/4
Flying
|1|B, Exile ~: Create a 1/2 |Sblack Bat creature token with flying. It has "|1|B, Sacrifice this creature:
Return an exiled card named ~ to the battlefield under its owner's control." */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_BAT, &token);
		token.tou = 2;
		token.special_infos = 66;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, GAA_RFG_ME, MANACOST_XB(1, 1), 0, NULL, NULL);
}

int card_serra_avenger(int player, int card, event_t event)
{
/* CARD_ID_SERRA_AVENGER	8663
Serra Avenger	|W|W
Creature - Angel 3/3
You can't cast ~ during your first, second, or third turns of the game.
Flying
Vigilance (Attacking doesn't cause this creature to tap.) */

  if (event == EVENT_MODIFY_COST && affect_me(player, card)
	  && current_turn == player && get_specific_turn_count(player) <= 3)
	infinite_casting_cost();

  vigilance(player, card, event);
  return 0;
}

int card_shadow_sliver(int player, int card, event_t event){
/* CARD_ID_SHADOW_SLIVER	8664
Shadow Sliver	|2|U
Creature - Sliver 1/1
All Sliver creatures have shadow. (They can block or be blocked by only creatures with shadow.) */

	if (event == EVENT_ABILITIES && in_play(player, card) && !is_humiliated(player, card, AL_STATIC)
		&& has_creature_type(affected_card_controller, affected_card, SUBTYPE_SLIVER)){
		shadow(affected_card_controller, affected_card, event);
	}

	return slivercycling(player, card, event);
}

int card_sidewinder_sliver(int player, int card, event_t event){
/* CARD_ID_SIDEWINDER_SLIVER	8665
Sidewinder Sliver	|W
Creature - Sliver 1/1
All Sliver creatures have flanking. (Whenever a creature without flanking blocks a Sliver, the blocking creature gets -1/-1 until end of turn.) */

	if( event == EVENT_ABILITIES && in_play(player, card) &&
		has_subtype(affected_card_controller, affected_card, SUBTYPE_SLIVER)
	  ){
		flanking(affected_card_controller, affected_card, event);
	}

	if( event == EVENT_DECLARE_BLOCKERS ){
		int p = current_turn;
		int count = active_cards_count[p]-1;
		while( count > -1 ){
				if( in_play(p, count) && has_subtype(p, count, SUBTYPE_SLIVER) && is_attacking(p, count) &&
					check_for_special_ability(p, count, SP_KEYWORD_FLANKING)
				  ){
					int c2 = active_cards_count[1-p]-1;
					while( c2 > -1 ){
							if( in_play(1-p, c2) && is_what(1-p, c2, TYPE_CREATURE) ){
								card_instance_t *this = get_card_instance(1-p, c2);
								if( this->blocking == count ){
									if( ! check_for_special_ability(1-p, c2, SP_KEYWORD_FLANKING) ){
										pump_until_eot(player, card, 1-p, c2, -1, -1);
									}
								}
							}
							c2--;
					}
				}
				count--;
		}
	}

	return slivercycling(player, card, event);
}

int card_smallpox(int player, int card, event_t event)
{
/* CARD_ID_SMALLPOX	8669
Smallpox	|B|B
Sorcery
Each player loses 1 life, discards a card, sacrifices a creature, then sacrifices a land. */

	if (event == EVENT_RESOLVE_SPELL)
	{
		APNAP(p, { lose_life(p, 1); });
		APNAP(p, { discard(p, 0, player); });
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		APNAP(p, { new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test); });
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		APNAP(p, { new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test); });
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_snapback(int player, int card, event_t event){
/* CARD_ID_SNAPBACK	8670
Snapback	|1|U
Instant
You may exile a |Sblue card from your hand rather than pay ~'s mana cost.
Return target creature to its owner's hand. */

	al_pitchspell(player, card, event, COLOR_BLUE, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_CREATURE", 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( casting_al_pitchspell(player, card, event, COLOR_BLUE, 0) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_CREATURE", 1, NULL);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_spectral_force(int player, int card, event_t event)
{
/* CARD_ID_SPECTRAL_FORCE	8671
Spectral Force	|3|G|G
Creature - Elemental Spirit 8/8
Trample
Whenever ~ attacks, if defending player controls no |Sblack permanents, it doesn't untap during your next untap step. */

	if (event == EVENT_DECLARE_ATTACKERS && count_permanents_by_color(1-current_turn, TYPE_PERMANENT, get_sleighted_color(player, card, COLOR_TEST_BLACK)) == 0){
		store_attackers(player, card, event, 0, player, card, NULL);
	}
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY){
		does_not_untap_effect(player, card, player, card, EDNT_CHECK_ORIGINAL_CONTROLLER, 1);
	}
	return 0;
}

int card_spell_burst(int player, int card, event_t event){
  /* Spell Burst	|X|U
   * Instant
   * Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Counter target spell with mana value X. */

/* ARD_ID_SPELL_BURST	1435
Spell Burst	|X|U
Instant
Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
Counter target spell with converted mana cost X. */
	// original code : 0x4DE16F

	if( event == EVENT_CAN_CAST ){
		int result = counterspell(player, card, event, NULL, 0);
		if( result > 0 ){
			if( has_mana(player, COLOR_COLORLESS, get_cmc(card_on_stack_controller, card_on_stack)) ){
				return result;
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		charge_mana(player, COLOR_COLORLESS, get_cmc(card_on_stack_controller, card_on_stack));
		if( spell_fizzled != 1 ){
			card_instance_t *instance = get_card_instance(player, card);
			instance->info_slot = buyback(player, card, MANACOST_X(3));
			return counterspell(player, card, event, NULL, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		int resolved = 0;
		if (counterspell_validate(player, card, NULL, 0) ){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			resolved++;
		}
		if( instance->info_slot == 1 && resolved){
			bounce_permanent(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return 0;
}

int card_spiketail_drakeling(int player, int card, event_t event){
	/* Spiketail Drakeling	|1|U|U
	 * Creature - Drake 2/2
	 * Flying
	 * Sacrifice ~: Counter target spell unless its controller pays |2. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		counterspell_resolve_unless_pay_x(player, card, &td, 0, 2);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_SPELL_ON_STACK, MANACOST0, 0, &td, NULL);
}

int card_spinneret_sliver(int player, int card, event_t event){
/* CARD_ID_SPINNERET_SLIVER	1387
Spinneret Sliver	|1|G
Creature - Sliver 2/2
All Sliver creatures have reach. (They can block creatures with flying.) */

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 0, 0, KEYWORD_REACH, BCT_INCLUDE_SELF);
	return slivercycling(player, card, event);
}

int card_sporesower_thallid(int player, int card, event_t event){

	/* Sporesower Thallid	|2|G|G
	 * Creature - Fungus 4/4
	 * At the beginning of your upkeep, put a spore counter on each Fungus you control.
	 * Remove three spore counters from ~: Put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "");
		test.subtype = SUBTYPE_FUNGUS;
		new_manipulate_all(player, card, player, &test, ACT_ADD_COUNTERS(COUNTER_SPORE, 1));
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return can_make_saproling_from_fungus(player, card);
	}

	if( event == EVENT_ACTIVATE ){
		saproling_from_fungus(player, card);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
	}

	return 0;
}

int card_sprite_noble(int player, int card, event_t event){
/* CARD_ID_SPRITE_NOBLE	8676
Sprite Noble	|1|U|U
Creature - Faerie 2/2
Flying
Other creatures you control with flying get +0/+1.
|T: Other creatures you control with flying get +1/+0 until end of turn. */

	if( event == EVENT_TOUGHNESS && affected_card_controller == player && affected_card != card && ! is_humiliated(player, card, AL_STATIC) &&
		in_play(affected_card_controller, affected_card) && check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING) )
	{
		event_result++;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t* instance = get_card_instance(player, card);
		int count = active_cards_count[player]-1;
		while( count > -1 ){
				if( count != instance->parent_controller && in_play(player, count) && check_for_ability(player, count, KEYWORD_FLYING) ){
					pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card, player, count, 1, 0);
				}
				count--;
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_stonebrow_krosan_hero(int player, int card, event_t event)
{
  /* Stonebrow, Krosan Hero	|3|R|G
   * Legendary Creature - Centaur Warrior 4/4
   * Trample
   * Whenever a creature you control with trample attacks, it gets +2/+2 until end of turn. */

	check_legend_rule(player, card, event);
	if (event == EVENT_DECLARE_ATTACKERS){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "");
		test.keyword = KEYWORD_TRAMPLE;
		store_attackers(player, card, event, DAT_TRACK, player, -1, &test);
	}
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY){
		card_instance_t* instance = get_card_instance(player, card);
		unsigned char* attackers = (unsigned char*)&instance->targets[2].player;
		for (int i = instance->info_slot - 1; i >= 0; --i){
			if (in_play(current_turn, attackers[i])){ pump_until_eot(player, card, current_turn, attackers[i], 2, 2); }
		}
	}
	return 0;
}

int card_stonewood_invocation(int player, int card, event_t event){
	/* Stonewood Invocation	|3|G
	 * Instant
	 * Split second
	 * Target creature gets +5/+5 and gains shroud until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		state_untargettable(player, card, 1);
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 5, 5, KEYWORD_SHROUD, 0);
		}
		else{
			state_untargettable(player, card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		state_untargettable(player, card, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_strangling_soot(int player, int card, event_t event){
	/* Strangling Soot	|2|B
	 * Instant
	 * Destroy target creature with toughness 3 or less.
	 * Flashback |5|R */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XR(5,1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.toughness_requirement = 3 | TARGET_PT_LESSER_OR_EQUAL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature with TOU 3 or less.", 1, NULL);
}

int card_strength_in_numbers(int player, int card, event_t event){
	/* Strength in Numbers	|1|G
	 * Instant
	 * Until end of turn, target creature gains trample and gets +X/+X, where X is the number of attacking creatures. */
	int amt = (event == EVENT_CHECK_PUMP || event == EVENT_RESOLVE_SPELL) ? count_attackers(current_turn) : 0;	// Likely won't be any use during EVENT_CHECK_PUMP.  Oh well.
	return vanilla_instant_pump(player, card, event, ANYBODY, player, amt, amt, KEYWORD_TRAMPLE, 0);
}

int card_stronghold_overseer(int player, int card, event_t event){
/* CARD_ID_STRONGHOLD_OVERSEER	8684
Stronghold Overseer	|3|B|B|B
Creature - Demon 5/5
Flying
Shadow (This creature can block or be blocked by only creatures with shadow.)
|B|B: Creatures with shadow get +1/+0 until end of turn and creatures without shadow get -1/-0 until end of turn. */

	shadow(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		APNAP(p,{
				int count = active_cards_count[p]-1;
				while( count > -1 ){
						if( in_play(p, count) && is_what(p, count, TYPE_CREATURE) ){
							if( check_for_special_ability(p, count, SP_KEYWORD_SHADOW) ){
								pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card, p, count, 1, 0);
							}
							else{
								pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card, p, count, -1, 0);
							}
						}
						count--;
				}
			}
		);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_B(2), 0, NULL, NULL);
}

int card_stuffy_doll(int player, int card, event_t event){
/* CARD_ID_STUFFY_DOLL	8685
Stuffy Doll	|5
Artifact Creature - Construct 0/1
Indestructible
As ~ enters the battlefield, choose a player.
Whenever ~ is dealt damage, it deals that much damage to the chosen player.
|T: ~ deals 1 damage to itself. */

	indestructible(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;
		td.illegal_abilities = 0;
		card_instance_t *instance = get_card_instance(player, card);
		if( new_pick_target(&td, "Choose a player.", 0, GS_LITERAL_PROMPT) ){
			instance->targets[1] = instance->targets[0];
		}
		instance->number_of_targets = 0;
	}

	if( !is_humiliated(player, card, AL_TRIG) && damage_dealt_to_me_arbitrary(player, card, event, 0, player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->targets[1].player > -1 ){
			damage_player(instance->targets[1].player, instance->targets[7].card, player, card);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		damage_creature(instance->parent_controller, instance->parent_card, 1, instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_subterranean_shambler(int player, int card, event_t event){
/* CARD_ID_SUBTERRANEAN_SHAMBLER	8686
Subterranean Shambler	|3|R
Creature - Elemental 2/3
Echo |3|R (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)
When ~ enters the battlefield or leaves the battlefield, it deals 1 damage to each creature without flying. */

	echo(player, card, event, MANACOST_XR(1, 1));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		this_test.keyword_flag = 1;
		new_damage_all(player, card, 2, 1, 0, &this_test);
	}

	return 0;
}

int card_sudden_death(int player, int card, event_t event){
	/* Sudden Death	|1|B|B
	 * Instant
	 * Split second
	 * Target creature gets -4/-4 until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		state_untargettable(player, card, 1);
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -4, -4);
		}
		else{
			state_untargettable(player, card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		state_untargettable(player, card, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_sudden_shock(int player, int card, event_t event){
	/* Sudden Shock	|1|R	0x200710e
	 * Instant
	 * Split second
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
		state_untargettable(player, card, 1);
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 ){
			damage_target0(player, card, 2);
		}
		else{
			state_untargettable(player, card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		state_untargettable(player, card, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_sudden_spoiling(int player, int card, event_t event){
  /* Sudden Spoiling |1|B|B
   * 	  Instant
   * 	  Split second (As long as this spell is on the stack, players can't cast spells or activate abilities that aren't mana abilities.)
   * 	  Creatures target player controls become 0/2 and lose all abilities until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.preferred_controller = player;
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		state_untargettable(player, card, 1);
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			int p = instance->targets[0].player;

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			test_definition_t hc;
			default_test_definition(&hc, 0);
			hc.power = 0;
			hc.toughness = 2;

			int c;
			for (c = active_cards_count[p]-1; c > -1; c--){
				if (in_play(p, c) && is_what(p, c, TYPE_CREATURE)){
					humiliate_and_set_pt_abilities(player, card, p, c, 4, &hc);
				}
			}
		}
		else{
			state_untargettable(player, card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		state_untargettable(player, card, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_sulfurous_blast(int player, int card, event_t event){
/* CARD_ID_SULFUROUS_BLAST	8690
Sulfurous Blast	|2|R|R
Instant
~ deals 2 damage to each creature and each player. If you cast this spell during your main phase, ~ deals 3 damage to each creature and each player instead.*/

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = 2;
		if( current_turn == player && (current_phase == PHASE_MAIN1 || current_phase == PHASE_MAIN2) ){
			amount = 3;
		}
		new_damage_all(player, card, ANYBODY, amount, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static const char* is_saprofite(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	if( has_subtype(player, card, SUBTYPE_INSECT) || has_subtype(player, card, SUBTYPE_RAT) ||
		has_subtype(player, card, SUBTYPE_SPIDER) || has_subtype(player, card, SUBTYPE_SQUIRREL) )
	{
		return NULL;
	}
	return "must be an Insect, Rat, Spider, or Squirrel";
}

int card_swarmyard(int player, int card, event_t event){
/* CARD_ID_SWARMYARD	8691
Swarmyard	""
Land
|T: Add |C to your mana pool.
|T: Regenerate target Insect, Rat, Spider, or Squirrel. */

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.preferred_controller = player;
	td.extra = target_predicate_extra(is_saprofite);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td.special |= TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum
	{
		CHOICE_MANA = 1,
		CHOICE_REGENERATE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( land_can_be_played & LCBP_REGENERATION ){
			if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
				return 99;
			}
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int abils[3] = {0, mana_producer(player, card, EVENT_CAN_ACTIVATE), 0};
		if( land_can_be_played & LCBP_REGENERATION ){
			if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
				abils[2] = 1;
			}
		}
		int choice = CHOICE_MANA;
		if( ! paying_mana() && abils[2] ){
			choice = CHOICE_REGENERATE;
		}
		instance->info_slot = choice;

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_REGENERATE ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
										&td, "Select target Insect, Rat, Spider, or Squirrel to regenerate.");
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_REGENERATE && valid_target(&td) ){
			if( can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
				regenerate_target(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_teferi_mage_of_zhalfir(int player, int card, event_t event){
/* CARD_ID_TEFERI_MAGE_OF_ZHALFIR	8693
Teferi, Mage of Zhalfir	|2|U|U|U
Legendary Creature - Human Wizard 3/4
Flash (You may cast this spell any time you could cast an instant.)
Creature cards you own that aren't on the battlefield have flash.
Each opponent can cast spells only any time he or she could cast a sorcery. */

	check_legend_rule(player, card, event);

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller != player && ! is_humiliated(player, card, AL_STATIC) ){
		if( ! can_sorcery_be_played(affected_card_controller, event) ){
			infinite_casting_cost();
		}
	}

	if( ! in_play(player, card) || is_humiliated(player, card, AL_STATIC) ){
		return flash(player, card, event);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) && is_what(player, i, TYPE_CREATURE) &&
				has_mana_to_cast_iid(player, event, get_original_internal_card_id(player, i)) )
			{
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int pc[2][hand_count[player]];
		int pcc = 0;
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) && is_what(player, i, TYPE_CREATURE) &&
				has_mana_to_cast_iid(player, event, get_original_internal_card_id(player, i)) )
			{
				pc[0][pcc] = get_original_internal_card_id(player, i);
				pc[1][pcc] = i;
				pcc++;
			}
		}
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card to play.");
		int selected = select_card_from_zone(player, player, pc[0], pcc, 0, AI_MAX_VALUE, -1, &this_test);
		if( selected == -1 ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_from_id(player, pc[1][selected], event, get_id(player, pc[1][selected])) ){
			play_card_in_hand_for_free(player, pc[1][selected]);
			cant_be_responded_to = 1;	// The spell will be respondable to, but this (fake) activation won't
		}
		else{
			spell_fizzled = 1;
		}
	}

	return 0;
}

int telekinetik_sliver_shared_ability(int player, int card, event_t event)
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
  default_target_definition(player, card, &td2, 0x47);
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = would_validate_arbitrary_target(&td2, instance->targets[2].player,
                                                instance->targets[2].card),
       result != 0))
  {
    tap_card(instance->targets[2].player, instance->targets[2].card);
  }
  return shared_sliver_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td2,
                                         "TARGET_PERMANENT");
}

int card_telekinetik_sliver(int player, int card, event_t event){
  /* Telekinetic Sliver |2|U|U
   * 	  Creature - Sliver 2/2
   * 	  All Slivers have "{T}: Tap target permanent." */

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &telekinetik_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &telekinetik_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int card_temporal_isolation(int player, int card, event_t event){
/* CARD_ID_TEMPORAL_ISOLATION	8696
Temporal Isolation	|1|W
Enchantment - Aura
Flash (You may cast this spell any time you could cast an instant.)
Enchant creature
Enchanted creature has shadow. (It can block or be blocked by only creatures with shadow.)
Prevent all damage that would be dealt by enchanted creature. */

	if (event == EVENT_CHECK_ABILITIES){
		card_instance_t* instance = get_card_instance(player, card);
		if (affect_me(instance->damage_target_player, instance->damage_target_card) && in_play(player, card)){
			check_abilities_power -= get_power(instance->damage_target_player, instance->damage_target_card);
		}
	}
	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *source = get_card_instance(affected_card_controller, affected_card);
			if( damage_card == source->internal_card_id  && source->damage_source_player == p &&
				source->damage_source_card == c )
			{
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
		if( event == EVENT_ABILITIES && affect_me(p, c) ){
			special_abilities(player, card, event, SP_KEYWORD_SHADOW, p, c );
		}
	}

	return generic_aura(player, card, event, 1-player, 0, 0, 0, SP_KEYWORD_SHADOW, 0, 0, 0);
}

int card_tendrils_of_corruption(int player, int card, event_t event){
/* CARD_ID_TENDRILS_OF_CORRUPTION	1592
Tendrils of Corruption	|3|B
Instant
~ deals X damage to target creature and you gain X life, where X is the number of |H1Swamps you control. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = count_subtype(player, TYPE_LAND, SUBTYPE_SWAMP);
			damage_target0(player, card, amount);
			gain_life(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_thallid_germinator(int player, int card, event_t event){

	/* Thallid Germinator	|2|G
	 * Creature - Fungus 2/2
	 * At the beginning of your upkeep, put a spore counter on ~.
	 * Remove three spore counters from ~: Put a 1/1 |Sgreen Saproling creature token onto the battlefield.
	 * Sacrifice a Saproling: Target creature gets +1/+1 until end of turn. */

	add_spore_counters(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_SAPROLING = 1,
		CHOICE_PUMP
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Saproling to sacrifice.");
	test.subtype = SUBTYPE_SAPROLING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_make_saproling_from_fungus(player, card) ){
			return 1;
		}
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL, 0, &test, 0);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int abils[3] = {
						0,
						can_make_saproling_from_fungus(player, card),
						generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST0, 0,
																		&td, NULL, 0, &test, 0)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Generate a Saproling", abils[1], 5,
							"Sac & give +1/+1", abils[2], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_SAPROLING ){
			saproling_from_fungus(player, card);
		}
		if( choice == CHOICE_PUMP ){
			return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE",
																	0, &test, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAPROLING ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
		}
		if( instance->info_slot == CHOICE_PUMP && valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							1, 1);
		}
	}


	return 0;
}

int card_thelon_of_havenwood(int player, int card, event_t event){
  /* Thelon of Havenwood	|G|G
   * Legendary Creature - Elf Druid 2/2
   * Each Fungus creature gets +1/+1 for each spore counter on it.
   * |B|G, Exile a Fungus card from a graveyard: Put a spore counter on each Fungus on the battlefield. */

	/* Thelon of Havenwood
	 * |G|G
	 * Legendary Creature - Elf Druid 2/2
	 * Each Fungus creature gets +1/+1 for each spore counter on it.
	 * |B|G, Exile a Fungus card from a graveyard: Put a spore counter on each Fungus on the battlefield. */

	check_legend_rule(player, card, event);

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( has_subtype(affected_card_controller, affected_card, SUBTYPE_FUNGUS) && in_play(affected_card_controller, affected_card) ){
			event_result += count_counters(affected_card_controller, affected_card, COUNTER_SPORE);
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_BG(1, 1), 0, NULL, NULL) ){
			if( count_graveyard_by_subtype(0, SUBTYPE_FUNGUS) || count_graveyard_by_subtype(1, SUBTYPE_FUNGUS) ){
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int t_player = 1-player;
		if( count_graveyard_by_subtype(1-player, SUBTYPE_FUNGUS) ){
			if( count_graveyard_by_subtype(player, SUBTYPE_FUNGUS) ){
				target_definition_t td;
				default_target_definition(player, card, &td, TYPE_CREATURE);
				td.zone = TARGET_ZONE_PLAYERS;

				card_instance_t *instance = get_card_instance(player, card);

				new_pick_target(&td, "Select a graveyard owner.", 0, 1 | GS_LITERAL_PROMPT);
				if( spell_fizzled == 1 ){
					return 0;
				}
				t_player = instance->targets[0].player;
				instance->number_of_targets = 0;
			}
		}
		else{
			t_player = player;
		}

		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a Fungus card to exile.");
		test.subtype = SUBTYPE_FUNGUS;
		int mode = t_player == player ? AI_MIN_VALUE : AI_MAX_VALUE;
		int selected = new_select_a_card(player, t_player, TUTOR_FROM_GRAVE, 0, mode, -1, &test);
		if( selected == -1 ){
			spell_fizzled = 1;
			return 0;
		}
		generic_activated_ability(player, card, event, 0, MANACOST_BG(1, 1), 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			rfg_card_from_grave(t_player, selected);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "");
		test.subtype = SUBTYPE_FUNGUS;
		new_manipulate_all(player, card, ANYBODY, &test, ACT_ADD_COUNTERS(COUNTER_SPORE, 1));
	}

	return 0;
}

int card_thelonite_hermit(int player, int card, event_t event){
	/* Thelonite Hermit	|3|G
	 * Creature - Elf Shaman 1/1
	 * Saproling creatures get +1/+1.
	 * Morph |3|G|G
	 * When ~ is turned face up, put four 1/1 |Sgreen Saproling creature tokens onto the battlefield. */

	if(event == EVENT_TURNED_FACE_UP ){
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, 4);
	}
	boost_creature_type(player, card, event, SUBTYPE_SAPROLING, 1, 1, 0, BCT_INCLUDE_SELF);
	return morph(player, card, event, MANACOST_XG(3, 2));
}

int card_thick_skinned_goblin(int player, int card, event_t event){
/* CARD_ID_THICK_SKINNED_GOBLIN	8701
Thick-Skinned Goblin	|1|R
Creature - Goblin Shaman 2/1
You may pay |0 rather than pay the echo cost for permanents you control.
|R: ~ gains protection from |Sred until end of turn. */

	return generic_shade(player, card, event, 0, MANACOST_R(1), 0, 0, KEYWORD_PROT_RED, 0);
}

int card_think_twice(int player, int card, event_t event){
/* CARD_ID_THINK_TWICE	8702
Think Twice	|1|U
Instant
Draw a card.
Flashback |2|U (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XU(2, 1));
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int total = get_total_available_lands(player);
		if( total <= 3 || total >= 6 ){
			ai_modifier += 48;
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		draw_cards(player, 1);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return basic_spell(player, card, event);
}

int card_thunder_totem(int player, int card, event_t event){
/* CARD_ID_THUNDER_TOTEM	8704
Thunder Totem	|3
Artifact
|T: Add |W to your mana pool.
|1|W|W: ~ becomes a 2/2 |Swhite Spirit artifact creature with flying and first strike until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_ANIMATE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_XW(1, 3), 0, NULL, NULL) ){
			return 1;
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, mana_producer(player, card, EVENT_CAN_ACTIVATE), 0};
		if( ! paying_mana() ){
			int fake_tap = 0;
			if( IS_AI(player) && ! is_tapped(player, card) ){
				add_state(player, card, STATE_TAPPED);
				fake_tap = 1;
			}
			abils[2] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_TYPE_CHANGE, MANACOST_XW(1, 2), 0, NULL, NULL);
			if( IS_AI(player) && fake_tap){
				remove_state(player, card, STATE_TAPPED);
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Get mana", abils[1], 1,
							"Animate", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		int fake_tap = 0;
		if( IS_AI(player) && ! is_tapped(player, card) ){
			add_state(player, card, STATE_TAPPED);
			fake_tap = 1;
		}
		if( choice == CHOICE_ANIMATE ){
			return generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_XW(1, 2), 0, NULL, NULL);
		}
		if( IS_AI(player) && fake_tap){
			remove_state(player, card, STATE_TAPPED);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_ANIMATE ){
			int legacy = artifact_animation(instance->parent_controller, instance->parent_card,
											instance->parent_controller, instance->parent_card, 1,
											2, 2, KEYWORD_FLYING | KEYWORD_FIRST_STRIKE, 0);
			card_instance_t *leg = get_card_instance(instance->parent_controller, legacy);
			leg->targets[6].player = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
			leg->targets[9].card = SUBTYPE_ELEMENTAL;
		}
	}

	return 0;
}

int card_tivadar_of_thorn(int player, int card, event_t event){
/* CARD_ID_TIVADAR_OF_THORN	8705
Tivadar of Thorn	|1|W|W
Legendary Creature - Human Knight 2/2
First strike, protection from |Sred
When ~ enters the battlefield, destroy target Goblin. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.required_subtype = SUBTYPE_GOBLIN;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target Goblin.", 0,  GS_LITERAL_PROMPT) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_trespasser_il_vec(int player, int card, event_t event){
/* CARD_ID_TRESPASSER_IL_VEC	8708
Trespasser il-Vec	|2|B
Creature - Human Rogue 3/1
Discard a card: ~ gains shadow until end of turn. (It can block or be blocked by only creatures with shadow.) */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card);
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								0, 0, 0, SP_KEYWORD_SHADOW);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20ea990; original cards/time_spiral.c:6930. */
static int re_enable_abilities_at_eot(int player, int card, event_t event)
{
  card_instance_t *instance;

  if (event != EVENT_CLEANUP)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {
    disable_all_activated_abilities((int)instance->damage_target_player,
                                         instance->damage_target_card, 0);
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20fde10; original cards/time_spiral.c:6943. */
int card_trickbind(int player, int card, event_t event)
{
  /* Trickbind	|1|U
   * Instant
   * Split second (As long as this spell is on the stack, players can't cast spells or activate abilities that aren't mana abilities.)
   * Counter target activated or triggered ability. If a permanent's ability is countered this way, activated abilities of that permanent can't be activated this turn. (Mana abilities can't be targeted.) */

  card_instance_t *instance;
  card_instance_t *spell_instance;
  int result;

  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    result = can_counter_activated_ability(player, card, EVENT_CAN_CAST, NULL);
    if (result != 0)
    {
      instance->info_slot = 1;
      return 99;
    }
    result = can_counter_triggered_ability(player, card, EVENT_CAN_CAST, NULL);
    if (result != 0)
    {
      instance->info_slot = 2;
      return 99;
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      spell_instance = get_card_instance(player, card);
      spell_instance->number_of_targets = '\0';
      state_untargettable(player, card, 1);
      if (instance->info_slot == 1)
      {
        cast_counter_activated_ability(player, card, 0);
      }
      else
      {
        if (instance->info_slot != 2)
        {
          spell_fizzled = 1;
          return 0;
        }
        cast_counter_triggered_ability(player, card, 0);
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    if (instance->info_slot == 1)
    {

      instance =
          get_card_instance(stack_cards[stack_size + -1].player, stack_cards[stack_size + -1].card);
      disable_all_activated_abilities(instance->parent_controller, instance->parent_card, 1);
      create_targetted_legacy_effect(player, card, re_enable_abilities_at_eot,
                                     instance->parent_controller, instance->parent_card);
      resolve_counter_activated_ability(player, card, NULL, 0);
    }
    else if (instance->info_slot == 2)
    {
      disable_all_activated_abilities(instance->targets[0].player, instance->targets[0].card, 1);
      create_targetted_legacy_effect(player, card, re_enable_abilities_at_eot,
                                     instance->targets[0].player, instance->targets[0].card);
      resolve_counter_triggered_ability(player, card, NULL, 0);
    }
    state_untargettable(player, card, 0);
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}
#endif

int card_triskelavus(int player, int card, event_t event){
	/* Triskelavus	|7	0x200299c
	 * Artifact Creature - Construct 1/1
	 * Flying
	 * ~ enters the battlefield with three +1/+1 counters on it.
	 * |1, Remove a +1/+1 counter from ~: Create a 1/1 colorless Triskelavite artifact creature token with flying. It has "Sacrifice this creature: This creature deals 1 damage to any target." */
	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_TRISKELAVITE);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_X(1), GVC_COUNTER(COUNTER_P1_P1), NULL, NULL);
}

int card_trump_the_domains(int player, int card, event_t event){
/* CARD_ID_TROMP_THE_DOMAINS	8711
Tromp the Domains	|5|G
Sorcery
Domain - Until end of turn, creatures you control gain trample and get +1/+1 for each basic land type among lands you control. */

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, count_domain(player, card), count_domain(player, card), KEYWORD_TRAMPLE, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20fe1e0; original cards/time_spiral.c:7029. */
int card_undying_rage(int player, int card, event_t event)
{
  /* Undying Rage	|2|R
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+2 and can't block.
   * When this Aura is put into a graveyard from the battlefield, return it to its owner's hand. */

  int result;

  immortal_enchantment(player, card, event);
  attached_gets_pt(player, card, event, 2, 2);
  attached_gets_kw(player, card, event, KEYWORD_0, SP_KEYWORD_CANNOT_BLOCK);
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

int card_unyaro_bees(int player, int card, event_t event){
	/* Unyaro Bees	|G|G|G	0x2007145
	 * Creature - Insect 0/1
	 * Flying
	 * |G: ~ gets +1/+1 until end of turn.
	 * |3|G, Sacrifice ~: It deals 2 damage to any target. */
	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( current_phase < PHASE_MAIN2 ){
			return generic_shade(player, card, event, 0, MANACOST_G(1), 1, 1, 0, 0);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	enum{
		CHOICE_PUMP = 1,
		CHOICE_SAC
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						1,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XG(3, 1), 0,
													&td, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"+1/+1 until EOT", abils[1], 5,
							"Sac & deal 2 damages", abils[2], current_phase >= PHASE_MAIN2 ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_PUMP ){
			return generic_activated_ability(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL);
		}
		if( choice == CHOICE_SAC ){
			return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XG(3, 1), 0,
													&td, "TARGET_ANY");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PUMP ){
			return generic_shade_merge_pt(player, card, event, 0, MANACOST_G(1), 1, 1);
		}
		if( instance->info_slot == CHOICE_SAC && valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return 0;
}

int card_urborg_syphon_mage(int player, int card, event_t event){
/* CARD_ID_URBORG_SYPHON_MAGE	8716
Urborg Syphon-Mage	|2|B
Creature - Human Spellshaper 2/2
|2|B, |T, Discard a card: Each other player loses 2 life. You gain life equal to the life lost this way. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int result = lose_life(1-player, 2);
		gain_life(player, result);
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DISCARD, MANACOST_XB(2, 1), 0, NULL, NULL);
}

int card_urzas_factory(int player, int card, event_t event){
/* CARD_ID_URZAS_FACTORY	8717
Urza's Factory	""
Land - Urza's
|T: Add |C to your mana pool.
|7, |T: Create a 2/2 colorless Assembly-Worker artifact creature token. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_TOKEN
	};

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(8), 0, NULL, NULL) )
		{
			choice = CHOICE_TOKEN;
		}
		if( ! IS_AI(player) && choice == CHOICE_TOKEN ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Create an Assembly Worker", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_TOKEN ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, 0, MANACOST_X(7), 0, NULL, NULL);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_TOKEN ){
			token_generation_t token;
			default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_ASSEMBLY_WORKER, &token);
			token.special_infos = 66;
			generate_token(&token);
		}
	}

	return 0;
}

static int vampiric_sliver_legacy(int player, int card, event_t event)
{

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (affect_me(instance->damage_target_player, instance->damage_target_card) &&
        !check_for_death_trigger_removal(instance->damage_target_player,
                                         instance->damage_target_card))
    {
      instance->damage_target_player = instance->damage_target_card = -1;
      instance->targets[11].player = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
    }
  }

  int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (amount)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->targets[1].player > -1 &&
        in_play(instance->targets[1].player, instance->targets[1].card))
    {
      add_counters(player, card, instance->targets[1].player, instance->targets[1].card,
                   COUNTER_P1_P1, amount);
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (current_phase == PHASE_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_vampiric_sliver(int player, int card, event_t event){
/* CARD_ID_VAMPIRIC_SLIVER	8718
Vampiric Sliver	|3|B
Creature - Sliver 3/3
All Sliver creatures have "Whenever a creature dealt damage by this creature this turn dies, put a +1/+1 counter on this creature." */

	if( event == EVENT_DEAL_DAMAGE && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->info_slot > 0 ){
			if( is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE) &&
				has_subtype(damage->damage_source_player, damage->damage_source_card, SUBTYPE_SLIVER)
			  ){
				int legacy = create_targetted_legacy_effect(player, card, &vampiric_sliver_legacy,
															damage->damage_target_player, damage->damage_target_card);
				get_card_instance(player, legacy)->targets[1].player = damage->damage_source_player;
				get_card_instance(player, legacy)->targets[1].card = damage->damage_source_card;
			}
		}
	}

	return slivercycling(player, card, event);
}

int card_verdant_embrace(int player, int card, event_t event){
	/* Verdant Embrace	|3|G|G
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature gets +3/+3 and has "At the beginning of each upkeep, put a 1/1 |Sgreen Saproling creature token onto the battlefield." */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		upkeep_trigger_ability(player, card, event, ANYBODY);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SAPROLING, &token);
			token.t_player = instance->damage_target_player;
			token.qty = count_upkeeps(current_turn);
			generate_token(&token);
		}
	}

	return generic_aura(player, card, event, player, 3, 3, 0, 0, 0, 0, 0);
}

int card_vesuva(int player, int card, event_t event)
{
  /* Vesuva	""
   * Land
   * You may have ~ enter the battlefield tapped as a copy of any land on the battlefield. */

	target_definition_t td;
	if (event == EVENT_RESOLVE_SPELL){
		base_target_definition(player, card, &td, TYPE_LAND);
		td.allowed_controller = ANYBODY;
		td.preferred_controller = player;
	}
	if (!is_humiliated(player, card, AL_TRIG)){
		if (enters_the_battlefield_as_copy_of_any(player, card, event, &td, "SELECT_A_LAND")){
			get_card_instance(player, card)->state |= STATE_TAPPED;
		}
	}
	else if (event == EVENT_RESOLVE_SPELL){
		play_land_sound_effect(player, card);
	}
	return mana_colors_added[player] ? mana_producer(player, card, event) : 0;
}

static int effect_vesuvan_shapeshifter(int player, int card, event_t event)
{
  if (effect_follows_control_of_attachment(player, card, event))
	return 0;

  if (current_turn == player && upkeep_trigger_mode(player, card, event, RESOLVE_TRIGGER_OPTIONAL))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  turn_face_down(instance->damage_target_player, instance->damage_target_card);
	  kill_card(player, card, KILL_EXILE);
	}

  return 0;
}

int card_vesuvan_shapeshifter(int player, int card, event_t event)
{
	/* Vesuvan Shapeshifter	|3|U|U	0x2003f77
	 * Creature - Shapeshifter 0/0
	 * As ~ enters the battlefield or is turned face up, you may choose another creature on the battlefield. If you do, until ~ is turned face down, it becomes a copy of that creature, except it has "At the beginning of your upkeep, you may turn this creature face down."
	 * Morph |1|U */
  target_definition_t td;
  if (event == EVENT_RESOLVE_SPELL || event == EVENT_TURNED_FACE_UP)
	{
	  base_target_definition(player, card, &td, TYPE_CREATURE);
	  td.special = TARGET_SPECIAL_NOT_ME;
	}

  if (enters_the_battlefield_as_copy_of_any(player, card, event, &td, "SELECT_ANOTHER_CREATURE"))
	set_legacy_image(player, CARD_ID_VESUVAN_SHAPESHIFTER, create_targetted_legacy_activate(player, card, &effect_vesuvan_shapeshifter, player, card));

  if (event == EVENT_TURNED_FACE_UP)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  instance->number_of_targets = 0;
	  if (pick_target(&td, "SELECT_ANOTHER_CREATURE"))
		{
		  instance->number_of_targets = 0;

		  cloning(player, card, instance->targets[0].player, instance->targets[0].card);	// legend rule will be verified by flip_card()
		  set_legacy_image(player, CARD_ID_VESUVAN_SHAPESHIFTER, create_targetted_legacy_activate(player, card, &effect_vesuvan_shapeshifter, player, card));

		  dispatch_event_to_single_card_overriding_function(player, card, EVENT_TURNED_FACE_UP, instance->targets[12].card);
		}
	}

  return morph(player, card, event, MANACOST_XU(1,1));
}

/* ManalinkEh 2022: 0x20fef10; original cards/time_spiral.c:7330. */
int card_voidmage_husher(int player, int card, event_t event)
{
  /* Voidmage Husher	|3|U
   * Creature - Human Wizard 2/2
   * Flash (You may cast this spell any time you could cast an instant.)
   * When this creature enters, counter target activated ability. (Mana abilities can't be targeted.)
   * Whenever you cast a spell, you may return this creature to its owner's hand. */

  int trigger_controller;
  int stolen;
  card_instance_t *instance;

  trigger_controller = 1;
  if (player == 1)
  {
    stolen = is_stolen(1, card);
    trigger_controller = 0;
    if (stolen != 0)
    {
      trigger_controller = 2;
    }
  }
  sp_ability_trigger(player, event, trigger_controller, TYPE_ANY, NULL, player, card);
  if (event != EVENT_SPA)
  {
    if (event == EVENT_CAN_CAST)
    {
      return can_counter_activated_ability(player, card, EVENT_CAN_CAST, NULL);
    }
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance = get_card_instance(player, card);
        instance->number_of_targets = '\0';
        return cast_counter_activated_ability(player, card, 0);
      }
      etb_ability(player, card, EVENT_CAST_SPELL, RESOLVE_TRIGGER_MANDATORY);
    }
    else
    {
      etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
      if (event == EVENT_ETB)
      {
        resolve_counter_activated_ability(player, card, NULL, 0);
        return 0;
      }
    }
    return 0;
  }
  bounce_permanent(player, card);
  etb_ability(player, card, EVENT_SPA, RESOLVE_TRIGGER_MANDATORY);
  return 0;
}

int card_volcanic_awakening(int player, int card, event_t event){

	/* Volcanic Awakening	|4|R|R
	 * Sorcery
	 * Destroy target land.
	 * Storm */

	if (event == EVENT_QUERY){ return attacking_card == QUERY_STORM; }
	if (!IS_GS_EVENT(player, card, event)){ return 0; }
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	card_instance_t* instance = get_card_instance(player, card);
	if (event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td)){ kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY); }
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
}

int card_walk_the_aeons(int player, int card, event_t event){
/* CARD_ID_WALK_THE_AEONS	8727
Walk the Aeons	|4|U|U
Sorcery
Buyback-Sacrifice three |H1Islands. (You may sacrifice three |H1Islands in addition to any other costs as you cast this spell. If you do, put this card into your hand as it resolves.)
Target player takes an extra turn after this one. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.preferred_controller = player;
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled == 1 ){
			return 0;
		}
		int tgt_player = instance->targets[0].player;

		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select an Island to sacrifice.");
		test.subtype = SUBTYPE_ISLAND;
		test.qty = 2;
		if( ! is_token(player, card) && new_can_sacrifice_as_cost(player, card, &test) ){
			int ai_choice = count_subtype(player, TYPE_LAND, SUBTYPE_ISLAND) >= 4 ? 1 : 0;
			int choice = do_dialog(player, player, card, -1, -1," Play normally\n Buyback", ai_choice);
			if( choice == 1 ){
				int sacs[2] = {0};
				int i;
				for(i=0; i<2; i++){
					sacs[i] = new_sacrifice(player, card, player, SAC_AS_COST | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
					if( ! sacs[i] ){
						spell_fizzled = 1;
						break;
					}
				}
				for(i=0; i<2; i++){
					if( sacs[i] ){
						state_untargettable(BYTE2(sacs[i]), BYTE3(sacs[i]), 0);
						if( spell_fizzled != 1 ){
							kill_card(BYTE2(sacs[i]), BYTE3(sacs[i]), KILL_SACRIFICE);
						}
					}
				}
			}
			instance->info_slot = choice;
		}
		instance->number_of_targets = 1;
		instance->targets[0].player = tgt_player;
		instance->targets[0].card = -1;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if( instance->targets[0].player == player ){
				time_walk_effect(player, card);
			}
			else{
				int fake = add_card_to_hand(instance->targets[0].player, get_original_internal_card_id(player, card));
				time_walk_effect(instance->targets[0].player, fake);
				obliterate_card(instance->targets[0].player, fake);
			}
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_watcher_sliver(int player, int card, event_t event){
/* CARD_ID_WATCHER_SLIVER	1391
Watcher Sliver	|3|W
Creature - Sliver 2/2
All Sliver creatures get +0/+2. */

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 0, 2, 0, BCT_INCLUDE_SELF);
	return slivercycling(player, card, event);
}

int card_weathered_bodyguards(int player, int card, event_t event)
{
/* CARD_ID_WEATHERED_BODYGUARDS	8728
Weathered Bodyguards	|5|W
Creature - Human Soldier 2/5
As long as ~ is untapped, all combat damage that would be dealt to you by unblocked creatures is dealt to ~ instead.
Morph |3|W (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

  card_instance_t* damage = combat_damage_being_prevented(event);
  if (damage
	  && ! is_humiliated(player, card, AL_STATIC)
	  && damage->damage_target_player == player
	  && damage->damage_target_card == -1
	  && !damage_is_to_planeswalker(damage)
	  && is_unblocked(damage->damage_source_player, damage->damage_source_card)
	  && !is_tapped(player, card))
	damage->damage_target_card = card;

  return morph(player, card, event, MANACOST_XW(3, 1));
}

int card_weatherseed_totem(int player, int card, event_t event){
/* CARD_ID_WEATHERSEED_TOTEM	8729
Weatherseed Totem	|3
Artifact
|T: Add |G to your mana pool.
|2|G|G|G: ~ becomes a 5/3 |Sgreen Treefolk artifact creature with trample until end of turn.
When ~ is put into a graveyard from the battlefield, if it was a creature, return this card to its owner's hand. */

	if( is_what(player, card, TYPE_CREATURE) && ! is_humiliated(player, card, AL_TRIG) ){
		nice_creature_to_sacrifice(player, card);

		int owner, position;
		if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) && find_in_owners_graveyard(player, card, &owner, &position) )
		{
			add_card_to_hand(owner, get_grave(owner)[position]);
			remove_card_from_grave(owner, position);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_ANIMATE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_XG(2, 4), 0, NULL, NULL) ){
			return 1;
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, mana_producer(player, card, EVENT_CAN_ACTIVATE), 0};
		if( ! paying_mana() ){
			int fake_tap = 0;
			if( IS_AI(player) && ! is_tapped(player, card) ){
				add_state(player, card, STATE_TAPPED);
				fake_tap = 1;
			}
			abils[2] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_TYPE_CHANGE, MANACOST_XG(2, 3), 0, NULL, NULL);
			if( IS_AI(player) && fake_tap){
				remove_state(player, card, STATE_TAPPED);
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Get mana", abils[1], 1,
							"Animate", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		int fake_tap = 0;
		if( IS_AI(player) && ! is_tapped(player, card) ){
			add_state(player, card, STATE_TAPPED);
			fake_tap = 1;
		}
		if( choice == CHOICE_ANIMATE ){
			return generic_activated_ability(player, card, event, GAA_TYPE_CHANGE, MANACOST_XG(2, 3), 0, NULL, NULL);
		}
		if( IS_AI(player) && fake_tap){
			remove_state(player, card, STATE_TAPPED);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 67 ){
			int legacy = artifact_animation(instance->parent_controller, instance->parent_card,
											instance->parent_controller, instance->parent_card, 1, 5, 3, KEYWORD_TRAMPLE, 0);
			card_instance_t *leg = get_card_instance(instance->parent_controller, legacy);
			int color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
			leg->targets[9].card = SUBTYPE_TREEFOLK;
			leg->targets[6].player = color;
		}
	}

	return 0;
}

int card_wheel_of_fate(int player, int card, event_t event)
{
  /* Wheel of Fate	""
   * Sorcery
   * Suspend 4-|1|R
   * Each player discards his or her hand, then draws seven cards. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  APNAP(p, { new_discard_all(p, player); });
	  APNAP(p, { draw_cards(p, 7); });
	  kill_card(player, card, KILL_DESTROY);
	}

  return suspend_only(player, card, event, 4, MANACOST_XR(1,1));
}

int card_wipe_away(int player, int card, event_t event){
	/* Wipe Away	|1|U|U
	 * Instant
	 * Split second
	 * Return target permanent to its owner's hand. */

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
		state_untargettable(player, card, 1);
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
		if( spell_fizzled != 1 ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		else{
			state_untargettable(player, card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		state_untargettable(player, card, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_word_of_seizing(int player, int card, event_t event){
/* CARD_ID_WORD_OF_SEIZING	8732
Word of Seizing	|3|R|R
Instant
Split second (As long as this spell is on the stack, players can't cast spells or activate abilities that aren't mana abilities.)
Untap target permanent and gain control of it until end of turn. It gains haste until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		state_untargettable(player, card, 1);
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier_for_act_of_treason(player, card);
			effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		else{
			state_untargettable(player, card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		state_untargettable(player, card, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_wurmcalling(int player, int card, event_t event){
/* CARD_ID_WURMCALLING	8734
Wurmcalling	|X|G
Sorcery
Buyback |2|G (You may pay an additional |2|G as you cast this spell. If you do, put this card into your hand as it resolves.)
Create an X/X |Sgreen Wurm creature token. */

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
		if( spell_fizzled != 1 && ! is_token(player, card) ){
			get_card_instance(player, card)->targets[0].player = buyback(player, card, MANACOST_XG(2, 1));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);

		token_generation_t token;
		default_token_definition(player, card, CARD_ID_WURM, &token);
		token.pow = token.tou = instance->info_slot;
		generate_token(&token);

		if( instance->targets[0].player == 1 ){
			bounce_permanent(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_yavimaya_dryad(int player, int card, event_t event){
/* CARD_ID_YAVIMAYA_DRYAD	8735
Yavimaya Dryad	|1|G|G
Creature - Dryad 2/1
|H2Forestwalk (This creature can't be blocked as long as defending player controls |Ha Forest.)
When ~ enters the battlefield, you may search your library for |Ha Forest card and put it onto the battlefield tapped under target player's control.
If you do, shuffle your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		if (basiclandtypes_controlled[1-player][get_hacked_color(player, card, COLOR_GREEN)] > 0){
			td.preferred_controller = player;
		}
		else{
			td.preferred_controller = 1 - player;
		}

		if (can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, get_hacked_land_text(player, card, "Select a %s card.", SUBTYPE_FOREST));
			test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
			int mode = instance->targets[0].player == player ? AI_MAX_VALUE : AI_MIN_VALUE;
			int selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, mode, -1, &test);
			if (selected != -1){
				int *deck = deck_ptr[player];
				int card_added = add_card_to_hand(instance->targets[0].player, deck[selected]);
				add_state(instance->targets[0].player, card_added, STATE_TAPPED);
				if (player != instance->targets[0].player){
					get_card_instance(instance->targets[0].player, card_added)->state ^= STATE_OWNED_BY_OPPONENT;
				}
				remove_card_from_deck(player, selected);
				put_into_play(instance->targets[0].player, card_added);
			}
			shuffle(player);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}
