#include "manalink.h"

/* Canonical recovered helper declarations. */
void spirit_link_effect(int player, int card, event_t event, int who_gains);

// Functions

static int song(int player, int card, event_t event, int clr)
{
  // Any number of target creatures become "clr" until end of turn.

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;
  td.allow_cancel = 0;

  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
  }

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
  {
    int trgs = 0;
    while (can_target(&td) && trgs < 10)
    {
      if (new_pick_target(&td, "TARGET_CREATURE", trgs, 0))
      {
        state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
        trgs++;
        td.allow_cancel = 2;
      }
      else
      {
        break;
      }
    }
    int i;
    for (i = 0; i < trgs; i++)
    {
      state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
    }
  }

  if (event == EVENT_RESOLVE_SPELL)
  {
    int i;
    for (i = 0; i < instance->number_of_targets; i++)
    {
      change_color(player, card, instance->targets[i].player, instance->targets[i].card, clr,
                   CHANGE_COLOR_SET | CHANGE_COLOR_END_AT_EOT);
    }
    kill_card(player, card, KILL_DESTROY);
  }

  return 0;
}

int mana_battery_generic(int player, int card, event_t event, color_t color, int colorless_cost)
{
  enum
  {
    CHOICE_MANA = 1,
    CHOICE_CHARGE = 2
  };

  int num_counters;
  int charge_priority;
  char prompt1[300];
  char prompt2[600];
  if (((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2) || event == EVENT_ACTIVATE)
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (is_tapped(player, card) != 0)
        return 0;
      if (is_animated_and_sick(player, card) != 0)
        return 0;
    }
    else if (event == EVENT_ACTIVATE)
      load_text(NULL, "MANABATTERY");

    num_counters = count_counters(player, card, COUNTER_CHARGE);
    charge_priority = 1;
    if (current_phase == PHASE_DISCARD)
      charge_priority = (unsigned int)(current_turn != player) * 2 + 1;
    unsigned int charge_available = 0;
    if (paying_mana() == 0 && can_use_activated_abilities(player, card) != 0)
    {
      int has_charge_mana = has_mana_for_activated_ability(
          player, card, num_counters + 1 + colorless_cost, 0, 0, 0, 0, 0);
      charge_available = (unsigned int)(has_charge_mana != 0);
    }
    int mana_available = can_produce_mana(player, card);
    int choice = dialog_impl(
        player, card, event, DLG_AUTOCHOOSE_IF_1, text_lines, mana_available, 2, 0x715dd8,
        charge_available, charge_priority, 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
      return choice;
    if (event == EVENT_ACTIVATE)
    {
      if (choice == CHOICE_MANA)
      {
        int amount;
        if (num_counters == 0)
          amount = 1;
        else
        {
          if (ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0))
            amount = recorded_rand(player, num_counters + 1);
          else
          {
            load_text(NULL, "MANABATTERY");
            scnprintf(prompt1, 300, text_lines[3], num_counters);
            int id = get_id(player, card);
            scnprintf(prompt2, 600, "%s\n\n%s", cards_ptr[id]->full_name, prompt1);
            amount = choose_a_number(player, prompt2, 0);
          }
          if (amount == -1)
          {
            spell_fizzled = 1;
            return 0;
          }
          if (num_counters < amount)
            amount = num_counters;
          remove_counters(player, card, player, card, COUNTER_CHARGE, amount);
          amount = amount + 1;
        }
        produce_mana_tapped(player, card, color, amount);
      }
      else if (choice == CHOICE_CHARGE)
      {
        card_instance_t *instance = get_card_instance(player, card);
        instance->state = instance->state | STATE_TAPPED;
        if (charge_mana_for_activated_ability(player, card, colorless_cost, 0, 0, 0, 0, 0) == 0)
          untap_card_no_event(player, card);
        tapped_for_mana_color = -1;
      }
      return 0;
    }
    if (choice == CHOICE_CHARGE)
      add_counters(player, card, player, card, COUNTER_CHARGE, 1);
  }
  if (event == EVENT_COUNT_MANA)
  {
    if (card != affected_card)
      return 0;
    if (player != affected_card_controller)
      return 0;
    if (is_tapped(player, card) != 0)
      return 0;
    if (is_animated_and_sick(player, card) != 0)
      return 0;
    if (can_produce_mana(player, card) == 0)
      return 0;
    num_counters = count_counters(player, card, COUNTER_CHARGE);
    declare_mana_available(player, color, num_counters + 1);
    return 0;
  }
  if (event == EVENT_CAN_WASTE_MANA)
  {
    if (is_tapped(player, card) != 0)
      return 0;
    if (is_animated_and_sick(player, card) != 0)
      return 0;
    if (can_use_activated_abilities(player, card) == 0)
      return 0;
    num_counters = count_counters(player, card, COUNTER_CHARGE);
    if (has_mana_for_activated_ability(player, card, colorless_cost + 1 + num_counters,
                                       0, 0, 0, 0, 0) == 0)
      return 0;
    event_result = event_result | 1;
    return 0;
  }
  if (event == EVENT_SHOULD_AI_PLAY && player == 1)
  {
    num_counters = count_counters(1, card, COUNTER_CHARGE);
    ai_modifier = ai_modifier + num_counters * 0xc;
    return 0;
  }
  return 0;
}

static int mana_battery(int player, int card, event_t event, color_t color)
{
	return mana_battery_generic(player, card, event, color, 2);
}

void landwalk_disabling_card(int player, int card, event_t event, player_bits_t landwalk_disabled){
	if( event == EVENT_ABILITIES ){
		player_bits[player] |= landwalk_disabled;
		player_bits[1-player] |= landwalk_disabled;
	}
}

static int leg_color_hoser(int player, int card, event_t event, int permanent_color, int land_subtype){

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	unsigned int color_shift = get_sleighted_color_test(player, card, permanent_color);
	td.required_color = 1U << (color_shift & 31);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_LAND);
	td2.required_subtype = get_hacked_subtype(player, card, land_subtype);

	card_instance_t *instance = get_card_instance( player, card);

	enum{
		CHOICE_KILL_PERMANENT = 1,
		CHOICE_BOUNCE_LAND
	};

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return can_target(&td) || can_target(&td2);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int priority_bounce_land = MIN(5, 15-(count_subtype(1-player, TYPE_LAND, -1)*5));
			char buffer1[100];
			char buffer2[100];
			scnprintf(buffer1, 100, "%s", get_sleighted_color_text(player, card, "Destroy target %s permanent", permanent_color));
			scnprintf(buffer2, 100, "%s", get_hacked_land_text(player, card, "Bounce target %s", land_subtype));
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								buffer1, can_target(&td), 10,
								buffer2, can_target(&td2), priority_bounce_land);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_KILL_PERMANENT ){
			new_pick_target(&td, get_sleighted_color_text(player, card, "Select target %s permanent.", permanent_color), 0,
							1 | GS_LITERAL_PROMPT);
		}
		if( instance->info_slot == CHOICE_BOUNCE_LAND ){
			new_pick_target(&td2, get_hacked_land_text(player, card, "Select target %s.", land_subtype), 0, 1 | GS_LITERAL_PROMPT);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_KILL_PERMANENT && valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( instance->info_slot == CHOICE_BOUNCE_LAND && valid_target(&td2) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

// Cards
static void destroy_green_or_white_at_end_of_combat(int player, int card, int t_player, int t_card)
{
  if (get_color(t_player, t_card) & (get_sleighted_color_test(player, card, COLOR_TEST_GREEN)
									 | get_sleighted_color_test(player, card, COLOR_TEST_WHITE)))
	create_legacy_effect_exe(player, card, LEGACY_EFFECT_STONING, t_player, t_card);
}

int card_abomination(int player, int card, event_t event){
/* CARD_ID_ABOMINATION	551
Abomination	|3|B|B
Creature - Horror 2/6
Whenever ~ blocks or becomes blocked by a |Sgreen or |Swhite creature, destroy that creature at end of combat. */

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG))
	instance->destroys_if_blocked |= DIFB_ASK_CARD;

  if (event == EVENT_CHECK_DESTROY_IF_BLOCKED && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG)
	  && (get_color(attacking_card_controller, attacking_card) & (get_sleighted_color_test(player, card, COLOR_TEST_GREEN)
																  | get_sleighted_color_test(player, card, COLOR_TEST_WHITE))))
	event_result |= 1;

  if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card, AL_TRIG))
	{
	  if (player == current_turn && (instance->state & STATE_ATTACKING))
		for_each_creature_blocking_me(player, card, destroy_green_or_white_at_end_of_combat, player, card);

	  if (player == 1-current_turn && (instance->state & STATE_BLOCKING))
		for_each_creature_blocked_by_me(player, card, destroy_green_or_white_at_end_of_combat, player, card);
	}

  return 0;
}

int card_acid_rain(int player, int card, event_t event){
/* CARD_ID_ACID_RAIN	552
Acid Rain	|3|U
Sorcery
Destroy all |H1Forests. */

	return basic_landtype_killer(player, card, event, SUBTYPE_FOREST);
}

int card_active_volcano(int player, int card, event_t event){
/* CARD_ID_ACTIVE_VOLCANO	553
Active Volcano	|R
Instant
Choose one -
* Destroy target |Sblue permanent.
* Return target |H2Island to its owner's hand. */

	return leg_color_hoser(player, card, event, COLOR_BLUE, SUBTYPE_ISLAND);
}

int card_adun_oakenshield(int player, int card, event_t event){
	/* Adun Oakenshield	|B|R|G
	 * Legendary Creature - Human Knight 1/2
	 * |B|R|G, |T: Return target creature card from your graveyard to your hand. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");

	if( event == EVENT_ACTIVATE ){
		if( current_turn == player && current_phase < PHASE_DECLARE_BLOCKERS ){
			ai_modifier -= 24;
		}
		if( current_turn == 1-player && current_phase >= PHASE_MAIN2 ){
			ai_modifier += 24;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST_BRG(1, 1, 1), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER);
}

// Adventurers' Guildhouse --> uncodeable
/* CARD_ID_ADVENTURERS_GUILDHOUSE	555
Adventurers' Guildhouse	""
Land
|SGreen legendary creatures you control have "bands with other legendary creatures." (Any legendary creatures can attack in a band
as long as at least one has "bands with other legendary creatures." Bands are blocked as a group. If at least two legendary creatures you control,
one of which has "bands with other legendary creatures," are blocking or being blocked by the same creature, you divide that creature's combat damage,
not its controller, among any of the creatures it's being blocked by or is blocking.) */

int card_aerathi_berserker(int player, int card, event_t event)
{
  /* AErathi Berserker	|2|R|R|R
   * Creature - Human Berserker 2/4
   * Rampage 3 */

  // 0x415c90



  rampage(player, card, event, 3);
  return 0;
}

static void becomes_green(int player, int card, int t_player, int t_card)
{
	change_color(player, card, t_player, t_card, get_sleighted_color_test(player, card, COLOR_TEST_GREEN), CHANGE_COLOR_SET);
}

int card_aisling_leprechaun(int player, int card, event_t event){
/* CARD_ID_AISLING_LEPRECHAUN	557
Aisling Leprechaun	|G
Creature - Faerie 1/1
Whenever ~ blocks or becomes blocked by a creature, that creature becomes |Sgreen. (This effect lasts indefinitely.) */

	if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card, AL_TRIG)){
		card_instance_t *instance = get_card_instance(player, card);
		if (player == current_turn && (instance->state & STATE_ATTACKING))
			for_each_creature_blocking_me(player, card, becomes_green, player, card);

		if (player == 1-current_turn && (instance->state & STATE_BLOCKING))
			for_each_creature_blocked_by_me(player, card, becomes_green, player, card);
	}

	return 0;
}

int card_akron_legionnaire(int player, int card, event_t event){
/* CARD_ID_AKRON_LEGIONNAIRE	558
Akron Legionnaire	|6|W|W
Creature - Giant Soldier 8/4
Except for creatures named ~ and artifact creatures, creatures you control can't attack. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_ATTACK_LEGALITY && affected_card_controller == player ){
			if( !(is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) ||
				get_id(affected_card_controller, affected_card) == CARD_ID_AKRON_LEGIONNAIRE)
			  ){
				event_result = 1;
			}
		}
	}

	return 0;
}

static int effect_carpet(int player, int card, event_t event){

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->damage_target_player == player && damage->damage_target_card == -1 && damage->info_slot > 0 ){
			if( damage->targets[4].player == -1 && damage->targets[4].card == -1 ){
				if( is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE) &&
					! check_for_ability(damage->damage_source_player, damage->damage_source_card, KEYWORD_FLYING) ){
					prevent_damage(affected_card_controller, affected_card, -1);
				}
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_al_abaras_carpet(int player, int card, event_t event){
  /* Al-abara's Carpet |5
   * Artifact
   * {5}, {T}: Prevent all damage that would be dealt to you this turn by attacking creatures without flying. */

	if( event == EVENT_ACTIVATE && current_phase == PHASE_AFTER_BLOCKING ){
		int mod = 0;
		CYCLE_ALL_CARDS(p, c, 	{
									if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) && is_attacking(p, c) ){
										if( get_card_instance(p, c)->regen_status & KEYWORD_FLYING ){
											if( p == player ){
												mod -= (get_power(p, c)*2);
											}
											else{
												mod += (get_power(p, c)*2);
											}
										}
									}
								};
		);
		ai_modifier += mod;
	}


	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		create_legacy_effect(instance->parent_controller, instance->parent_card, &effect_carpet);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(5), 0, NULL, NULL);
}

int card_alabaster_potion(int player, int card, event_t event){
	/* Alabaster Potion	|X|W|W	0x200569c
	 * Instant
	 * Choose one -
	 * * Target player gains X life.
	 * * Prevent the next X damage that would be dealt to any target this turn. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, 0);
	td2.extra = damage_card;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_GAIN_LIFE = 1,
		CHOICE_PREVENT_DAMAGE,
	};

	if( event == EVENT_CAN_CAST ){
		int rvalue = generic_spell(player, card, event, GS_DAMAGE_PREVENTION | GS_CAN_TARGET | GS_X_SPELL, &td2, NULL, 1, NULL);
		if( rvalue ){
			return 99;
		}
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, NULL, 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 1, NULL); //So it will store the correct X value, integrated with cost modifiers
			int modes[3] = {
							0,
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET | GS_X_SPELL, &td, NULL, 1, NULL),
							generic_spell(player, card, EVENT_CAN_CAST, GS_DAMAGE_PREVENTION | GS_CAN_TARGET | GS_X_SPELL, &td2, NULL, 1,
											NULL)
			};
			if( modes[CHOICE_GAIN_LIFE] ){
				instance->targets[1].player = CHOICE_GAIN_LIFE;
			}
			if( modes[CHOICE_PREVENT_DAMAGE] ){
				instance->targets[1].player = CHOICE_PREVENT_DAMAGE;
			}
			if( player == HUMAN ){
				int choice = DIALOG(player, card, event, DLG_NO_STORAGE,
									"Gain X life", modes[1], 1,
									"Prevent X damage", modes[2], 1);
				if( ! choice ){
					spell_fizzled = 1;
					return 0;
				}
				instance->targets[1].player = choice;
			}
		}
		if( instance->targets[1].player == CHOICE_GAIN_LIFE ){
			pick_target(&td, "TARGET_PLAYER");
		}
		if( instance->targets[1].player == CHOICE_PREVENT_DAMAGE ){
			pick_target(&td2, "TARGET_DAMAGE");
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->targets[1].player == CHOICE_GAIN_LIFE && valid_target(&td) ){
			gain_life(instance->targets[0].player, instance->info_slot);
		}
		if( instance->targets[1].player == CHOICE_PREVENT_DAMAGE && valid_target(&td2) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_alchors_tomb(int player, int card, event_t event){
/* CARD_ID_ALCHORS_TOMB	561
Alchor's Tomb	|4
Artifact
|2, |T: Target permanent you control becomes the color of your choice. (This effect lasts indefinitely.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			change_color(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
						1<<choose_a_color(player, get_deck_color(player, 1-player)), CHANGE_COLOR_SET);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_PERMANENT");
}

static int effect_all_hallows_eve(int player, int card, event_t event)
{
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  int iid = get_internal_card_id_from_csv_id(CARD_ID_ALL_HALLOWS_EVE);
	  int exiledpos = find_iid_in_rfg(player, iid);
	  if (exiledpos == -1)
		{
		  kill_card(player, card, KILL_EXILE);
		  return -1;
		}

	  remove_counters(player, card, player, card, COUNTER_SCREAM, 1);
	  if (!count_counters(player, card, COUNTER_SCREAM))
		{
			from_exile_to_graveyard(player, exiledpos);
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			APNAP(p, {new_reanimate_all(p, -1, p, &test, REANIMATE_DEFAULT);};);
			kill_card(player, card, KILL_EXILE);
			return -1;
		}
	}

  return 0;
}

int card_all_hallows_eve(int player, int card, event_t event)
{
  /* All Hallow's Eve	|2|B|B
   * Sorcery
   * Exile ~ with two scream counters on it.
   * At the beginning of your upkeep, if ~ is exiled with a scream counter on it, remove a scream counter from it. If there are no more scream counters on it,
   * put it into your graveyard and each player returns all creature cards from his or her graveyard to the battlefield. */

  int result;
  if (event == EVENT_CAN_CAST)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    return result;
  }
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }
  result = create_legacy_effect(player, card, effect_all_hallows_eve);
  add_counters(player, card, player, result, COUNTER_SCREAM, 2);
  kill_card(player, card, KILL_EXILE);
  return 0;
}

int card_amrou_kithkin(int player, int card, event_t event){
/* CARD_ID_AMROU_KITHKIN	563
Amrou Kithkin	|W|W
Creature - Kithkin 1/1
~ can't be blocked by creatures with power 3 or greater. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_BLOCK_LEGALITY && attacking_card_controller == player && attacking_card == card ){
			if( get_power(affected_card_controller, affected_card) > 2 ){
				event_result = 1;
			}
		}
	}

	return 0;
}

int card_angelic_voices(int player, int card, event_t event){
/* CARD_ID_ANGELIC_VOICES	564
Angelic Voices	|2|W|W
Enchantment
Creatures you control get +1/+1 as long as you control no nonartifact, non|Swhite creatures. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == player ){
			int i;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_play(player, i) && is_what(player, i, TYPE_CREATURE) ){
					if( ! is_what(player, i, TYPE_ARTIFACT) &&
						!(get_color(player, i) & get_sleighted_color_test(player, card, COLOR_TEST_WHITE)) )
					{
						return 0;
					}
				}
			}
			event_result++;
		}
	}

	return global_enchantment(player, card, event);
}

int card_angus_mackenzie(int player, int card, event_t event){
  /* Angus Mackenzie	|G|W|U
   * Legendary Creature - Human Cleric 2/2
   * |G|W|U, |T: Prevent all combat damage that would be dealt this turn. Activate this ability only before the combat damage step. */

	// original code : 00406250



	check_legend_rule(player, card, event);

	if( event == EVENT_CAN_ACTIVATE && current_phase >= PHASE_FIRST_STRIKE_DAMAGE ){
		return 0;
	}

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (current_turn == player ? -24 : 24);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		fog_effect(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_GWU(1, 1, 1), 0, NULL,NULL);
}

int card_anti_magic_aura(int player, int card, event_t event){
/* CARD_ID_ANTI_MAGIC_AURA	566
Anti-Magic Aura	|2|U
Enchantment - Aura
Enchant creature
Enchanted creature can't be the target of spells and can't be enchanted by other Auras. */

	card_instance_t* instance = get_card_instance(player, card);
	if (in_play(player, card) && instance->damage_target_player >= 0){
		cannot_be_enchanted_granted(player, card, event, instance->damage_target_player, instance->damage_target_card);
	}
	attached_gets_sp_kw2(player, card, event, SP_KEYWORD2_GRANTED_PROT_MISC);
	if (event == EVENT_PROT_MISC && instance->damage_target_player == affected_card_controller && instance->damage_target_card == affected_card && BYTE3(event_result) == 5 && is_being_cast(attacking_card_controller, attacking_card)){
		event_result |= 1;
	}
	return aura(player, card, event, NULL, NULL);
}

int card_arboria(int player, int card, event_t event){
/* CARD_ID_ARBORIA	567
Arboria	|2|G|G
World Enchantment
Creatures can't attack a player unless that player cast a spell or put a nontoken permanent onto the battlefield during his or her last turn. */

  card_instance_t* instance = get_card_instance(player, card);

  enchant_world(player, card, event);

  // Approximation: Creatures can't attack you or planeswalkers you control unless you're unsafe.  (Shouldn't affect planeswalkers.)
  if (event == EVENT_ATTACK_LEGALITY && instance->targets[1 - affected_card_controller].player <= 0)
	event_result = 1;

  // Reset "safeness" each turn
  if (event == EVENT_BEGIN_TURN)
	instance->targets[current_turn].player = 0;

  // Keep track of if a player played a spell on their turn
  if (trigger_condition == TRIGGER_SPELL_CAST && event == EVENT_END_TRIGGER
	  && affect_me(player, card) && trigger_cause_controller == current_turn)
	instance->targets[current_turn].player = 1;

  /* Approximation: Did a nontoken permanent enter the battlefield under a player's control?
   * (Should look at who put it on the battlefield, not whose control it came under.) */
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && event == EVENT_END_TRIGGER
	  && affect_me(player, card) && trigger_cause_controller == current_turn
	  && is_what(trigger_cause_controller, trigger_cause, TYPE_PERMANENT)
	  && !is_token(trigger_cause_controller, trigger_cause))
	instance->targets[current_turn].player = 1;

  return global_enchantment(player, card, event);
}

int card_arcades_sabboth(int player, int card, event_t event){
/* CARD_ID_ARCADES_SABBOTH	568
Arcades Sabboth	|2|G|G|W|W|U|U
Legendary Creature - Elder Dragon 7/7
Flying
At the beginning of your upkeep, sacrifice ~ unless you pay |G|W|U.
Each untapped creature you control gets +0/+2 as long as it's not attacking.
|W: ~ gets +0/+1 until end of turn. */

	check_legend_rule(player, card, event);

	basic_upkeep(player, card, event, MANACOST_GWU(1, 1, 1));

	if( ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_TOUGHNESS && affected_card_controller == player && ! is_tapped(affected_card_controller, affected_card) &&
			! is_attacking(affected_card_controller, affected_card) && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
			&& in_play(player, card) )
		{
			event_result+=2;
		}
	}

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_W(1), 0, 1);
}

int card_arena_of_the_ancients(int player, int card, event_t event){

	/* Arena of the Ancients	|3
	 * Artifact
	 * Legendary creatures don't untap during their controllers' untap steps.
	 * When ~ enters the battlefield, tap all legendary creatures. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		if (!check_battlefield_for_id(ANYBODY, CARD_ID_LEYLINE_OF_SINGULARITY)){
			this_test.subtype = SUBTYPE_LEGEND;
		}
		new_manipulate_all(player, card, ANYBODY, &this_test, ACT_TAP);
	}

	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && ! is_humiliated(player, card, AL_STATIC) &&
		is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
		is_legendary(affected_card_controller, affected_card) )
	{
		does_not_untap(affected_card_controller, affected_card, event);
	}

	return 0;
}

int card_avoid_fate(int player, int card, event_t event){

	/* Avoid Fate	|G
	 * Instant
	 * Counter target instant or Aura spell that targets a permanent you control. */
	if(event == EVENT_CAN_CAST ){
		int result = card_counterspell(player, card, event);
		if( result > 0 ){
			if( is_what(card_on_stack_controller, card_on_stack, TYPE_INSTANT | TYPE_INTERRUPT) ||
				has_subtype(card_on_stack_controller, card_on_stack, SUBTYPE_AURA) )
			{
				card_instance_t *spell = get_card_instance(card_on_stack_controller, card_on_stack);
				int i = 0;
				for(i=0; i<spell->number_of_targets; i++){
					if( spell->targets[i].player == player && spell->targets[i].card != -1 ){
						return result;
					}
				}
			}
		}
	}
	if( (event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_RESOLVE_SPELL ){
		return card_counterspell(player, card, event);
	}
	return 0;
}

int card_axelrod_gunnarson(int player, int card, event_t event){
	/* Axelrod Gunnarson	|4|B|B|R|R	0x2002b4a
	 * Legendary Creature - Giant 5/5
	 * Trample
	 * Whenever a creature dealt damage by ~ this turn dies, you gain 1 life and ~ deals 1 damage to target player or planeswalker. */
	if( sengir_vampire_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		// All the life gains and damage done are in separate 1-point packets.  Only target once as an interface convenience, though.
		int num = instance->targets[11].card;
		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER") ){
			instance->number_of_targets = 1;
		} else {
			instance->number_of_targets = 0;
		}
		int i;
		for (i = 0; i < num; ++i){
			gain_life(player, 1);
			if (instance->number_of_targets == 1){
				damage_target0(player, card, instance->targets[11].card);
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_ayesha_tanaka(int player, int card, event_t event)
{
/* CARD_ID_AYESHA_TANAKA	572
Ayesha Tanaka	|W|W|U|U
Legendary Creature - Human Artificer 2/2
Banding
|T: Counter target activated ability from an artifact source unless that ability's controller pays |W. (Mana abilities can't be targeted.) */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counter_activated_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_CAN_ACTIVATE)
	{
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) &&
			can_counter_activated_ability(player, card, event, &td) )
		{
			return 99;
		}
	}

	if (event == EVENT_ACTIVATE)
	{
		instance->number_of_targets = 0;
		generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			cast_counter_activated_ability(player, card, 0);
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION)
	{
		if (validate_counter_activated_ability(player, card, &td, 0))
		{
			ldoubleclicked = 0;
			charge_mana_while_resolving(player, card, event, instance->targets[0].player, COLOR_WHITE, 1);
			if (spell_fizzled == 1)
				raw_counter_activated_ability(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

// azure drake --> vanilla

static const char* damage_from_sorcery_source(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	card_instance_t* dmg = get_card_instance(player, card);
	if ( dmg->internal_card_id == damage_card ){
		int good = dmg->targets[3].player & TYPE_SORCERY;
		if( in_play(dmg->damage_source_player, dmg->damage_source_card) ){
			good = is_what(dmg->damage_source_player, dmg->damage_source_card, TYPE_SORCERY);
		}
		if( good ){
			return NULL;
		}
	}

	return "must be damage from sorcery source";
}

int card_backdraft(int player, int card, event_t event){
/* CARD_ID_BACKDRAFT	574
Backdraft	|1|R
Instant
Choose a player who cast one or more sorcery spells this turn. ~ deals damage to that player equal to half the damage dealt
by one of those sorcery spells this turn, rounded down. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	// Approximation
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_EFFECT);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(damage_from_sorcery_source);
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			damage_player(target->damage_source_player, round_down_value(target->info_slot), player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, "TARGET_DAMAGE", 1, NULL);
}

int card_backfire(int player, int card, event_t event){
/* CARD_ID_BACKFIRE	575
Backfire	|U
Enchantment - Aura
Enchant creature
Whenever enchanted creature deals damage to you, ~ deals that much damage to that creature's controller. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 && ! is_humiliated(player, card, AL_TRIG) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if (event == EVENT_DEAL_DAMAGE){
			card_instance_t* damage = get_card_instance(affected_card_controller, affected_card);
			if (damage->internal_card_id == damage_card && damage->damage_source_card == c && damage->damage_source_player == p &&
				damage->damage_target_card == -1 && ! damage_is_to_planeswalker(damage)
			  ){
				if( damage->damage_target_player == player ){
					int amount = damage->info_slot;
					if( amount > 0 ){
						if( instance->targets[1].player < 0 ){
							instance->targets[1].player = 0;
						}
						instance->targets[1].player += amount;
					}
				}
			}
		}
		if( instance->targets[1].player > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
			reason_for_trigger_controller == player
		  ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					damage_player(p, instance->targets[1].player, player, card);
					instance->targets[1].player = 0;
			}
			else if (event == EVENT_END_TRIGGER){
					instance->targets[1].player = 0;
			}
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return disabling_aura(player, card, event);
}

// barbary apes --> vanilla

int card_barktooth_warbeard(int player, int card, event_t event){
  /* Barktooth Warbeard	|4|B|R|R
   * Legendary Creature - Human Warrior 6/5 */

	// Also code for Jasmine Boreal, Jedit Ojanen, Jerrard of the Closed Fist, Kasimir the Lone Wolf, Lady Orca, Ramirez DePietro,
	// Sir Shandlar of Eberyn, Sivitri Scarzam, The Lady of the Mountain, Tobias Andrion, Torsten Von Ursus,
	check_legend_rule(player, card, event);
	return 0;
}

int card_bartel_runeaxe(int player, int card, event_t event){
/* CARD_ID_BARTEL_RUNEAXE	578
Bartel Runeaxe	|3|B|R|G
Legendary Creature - Giant Warrior 6/5
Vigilance
~ can't be the target of Aura spells. */

	check_legend_rule(player, card, event);
	vigilance(player, card, event);
	return 0;
}

int card_beasts_of_bogardan(int player, int card, event_t event){
/* CARD_ID_BEASTS_OF_BOGARDAN	579
Beasts of Bogardan	|4|R
Creature - Beast 3/3
Protection from |Sred
~ gets +1/+1 as long as an opponent controls a nontoken |Swhite permanent. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		protection_from_red(player, card, event);
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_PERMANENT);
			this_test.type_flag = F1_NO_TOKEN;
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
			if( check_battlefield_for_special_card(player, card, 1-player, 0, &this_test) ){
				event_result++;
			}
		}
	}

	return 0;
}

int card_black_mana_battery(int player, int card, event_t event)
{
/* CARD_ID_BLACK_MANA_BATTERY	580
Black Mana Battery	|4
Artifact
|2, |T: Put a charge counter on ~.
|T, Remove any number of charge counters from ~: Add |B to your mana pool, then add an additional |B to your mana pool for each charge counter removed this way.*/

  // 0x4235E0
  return mana_battery(player, card, event, COLOR_BLACK);
}

int card_blazing_effigy(int player, int card, event_t event){
/* CARD_ID_BLAZING_EFFIGY	581
Blazing Effigy	|1|R
Creature - Elemental 0/3
When ~ dies, it deals X damage to target creature, where X is 3 plus the amount of damage dealt to ~ this turn by other sources named ~. */

	if (event == EVENT_DEAL_DAMAGE){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t* damage = get_card_instance(affected_card_controller, affected_card);
		if (damage->internal_card_id == damage_card && damage->damage_target_card == card && damage->damage_target_player == player &&
			damage->info_slot > 0 )
		{
			if( damage->display_pic_csv_id == CARD_ID_BLAZING_EFFIGY ){
				if( instance->targets[1].player < 0 ){
					instance->targets[1].player = 0;
				}
				instance->targets[1].player+=damage->info_slot;
			}
		}
	}

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance(player, card);
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			damage_target0(player, card, 3+(instance->targets[1].player > 0 ? instance->targets[1].player : 0));
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	return 0;
}

int card_blight2(int player, int card, int event){
/* CARD_ID_BLIGHT	582
Blight	|B|B
Enchantment - Aura
Enchant land
When enchanted land becomes tapped, destroy it. */

	if( ! (IS_AURA_EVENT(player, card, event) || event == EVENT_TAP_CARD || event == EVENT_STATIC_EFFECTS) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t* instance = get_card_instance(player, card);

	if( instance->damage_target_player > -1 ){
		if( event == EVENT_TAP_CARD && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			kill_card(instance->damage_target_player, instance->damage_target_card, KILL_DESTROY);
		}
	}
	return targeted_aura(player, card, event, &td, "TARGET_LAND");
}

int card_blue_mana_battery(int player, int card, event_t event)
{
/* CARD_ID_BLUE_MANA_BATTERY	584
Blue Mana Battery	|4
Artifact
|2, |T: Put a charge counter on ~.
|T, Remove any number of charge counters from ~: Add |U to your mana pool, then add an additional |U to your mana pool for each charge counter removed this way.*/

  // 0x423560
  return mana_battery(player, card, event, COLOR_BLUE);
}

int card_blood_lust(int player, int card, event_t event){
/* CARD_ID_BLOOD_LUST	583
Blood Lust	|1|R
Instant
If target creature has toughness 5 or greater, it gets +4/-4 until end of turn. Otherwise, it gets +4/-X until end of turn, where X is its toughness minus 1. */

	card_instance_t *instance = get_card_instance(player, card);
	if( event == EVENT_CHECK_PUMP ){
		if ( has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id) ){
			if( instance->targets[0].player > -1 && instance->targets[0].card > -1 ){
				pumpable_power[player] += 4;
				pumpable_toughness[player] -= (get_toughness(instance->targets[0].player, instance->targets[0].card) > 4 ? 4 : get_toughness(instance->targets[0].player, instance->targets[0].card)-1);

			}
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	if (event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td)){
			int tp = get_toughness(instance->targets[0].player, instance->targets[0].card) > 4 ? -4 : -(get_toughness(instance->targets[0].player, instance->targets[0].card)-1);
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 4, tp);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_boomerang(int player, int card, event_t event){
/* CARD_ID_BOOMERANG	585
Boomerang	|U|U
Instant
Return target permanent to its owner's hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
}

int card_boris_devilboon(int player, int card, event_t event){
	/* Boris Devilboon	|3|B|R
	 * Legendary Creature - Zombie Wizard 2/2
	 * |2|B|R, |T: Put a 1/1 |Sblack and |Sred Demon creature token named Minor Demon onto the battlefield. */

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_MINOR_DEMON);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XBR(2, 1, 1), 0, NULL, NULL);
}

int card_bronze_horse(int player, int card, event_t event){
/* CARD_ID_BRONZE_HORSE	588
Bronze Horse	|7
Artifact Creature - Horse 4/4
Trample
As long as you control another creature, prevent all damage that would be dealt to ~ by spells that target it. */

	if (!is_humiliated(player, card, AL_STATIC) && event == EVENT_PREVENT_DAMAGE){
		card_instance_t* damage = get_card_instance(affected_card_controller, affected_card);
		if (damage->internal_card_id == damage_card && damage->damage_target_card == card && damage->damage_target_player == player && damage->info_slot > 0){
			if ((damage->targets[3].player & TYPE_SPELL) && !(damage->targets[3].player & TYPE_PERMANENT) && count_subtype(player, TYPE_CREATURE, -1) > 1 && damage->targets[7].player == 1){
				damage->info_slot = 0;
			}
		}
	}
	return 0;
}

static int brine_hag_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_BRINE_HAG)
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
            if (check_for_death_trigger_removal(p, c))
            {
              kill_card(player, card, KILL_EXILE);
              return 0;
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
      }
    }
  }

  if (!check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (result)
    {
      card_instance_t *instance = get_card_instance(player, card);
      int amount = BYTE0(result);
      int i;
      for (i = 1; i < amount; i++)
      {
        int legacy = real_set_pt(player, card, instance->targets[i].player,
                                 instance->targets[i].card, 2, 0, 0);
        add_status(player, legacy, STATUS_INVISIBLE_FX);
      }
      kill_card(player, card, KILL_EXILE);
    }
  }

  if (event == EVENT_DEAL_DAMAGE)
  {
    card_instance_t *instance = get_card_instance(player, card);
    card_instance_t *damage = damage_being_dealt(event);
    if (damage)
    {
      if (damage->damage_target_player == instance->damage_target_player &&
          damage->damage_target_card == instance->damage_target_card &&
          is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE))
      {
        if (instance->targets[11].player < 0)
        {
          instance->targets[11].player = 0;
          SET_BYTE0(instance->targets[11].player) = 1;
        }
        int pos = BYTE0(instance->targets[11].player);
        if (pos < 10)
        {
          instance->targets[pos].player = damage->damage_source_player;
          instance->targets[pos].card = damage->damage_source_card;
          SET_BYTE0(instance->targets[11].player)++;
        }
      }
    }
  }

  if (event == EVENT_CLEANUP)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (!check_status(player, card, STATUS_INVISIBLE_FX))
    {
      kill_card(player, card, KILL_EXILE);
      return 0;
    }
    instance->targets[11].player = 0;
    int i;
    for (i = 1; i < 10; i++)
    {
      instance->targets[i].player = instance->targets[i].card = -1;
    }
  }

  return 0;
}

int card_brine_hag(int player, int card, event_t event){
  /* Brine Hag |2|U|U
   * Creature — Hag 2/2
   * When Brine Hag dies, change the base power and toughness of all creatures that dealt damage to it this turn to 0/2.
   * 	(This effect lasts indefinitely.) */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)brine_hag_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &brine_hag_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	return 0;
}

int card_carrion_ants(int player, int card, event_t event){
/* CARD_ID_CARRION_ANTS	589
Carrion Ants	|2|B|B
Creature - Insect 0/1
|1: ~ gets +1/+1 until end of turn. */

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(1), 1, 1);
}

// cat warriors --> vanilla

// cathedral of serra --> uncodeable
/* CARD_ID_CATHEDRAL_OF_SERRA	591
Cathedral of Serra	""
Land
|SWhite legendary creatures you control have "bands with other legendary creatures." (Any legendary creatures can attack in a band
as long as at least one has "bands with other legendary creatures." Bands are blocked as a group. If at least two legendary creatures you control,
one of which has "bands with other legendary creatures," are blocking or being blocked by the same creature, you divide that creature's combat damage,
not its controller, among any of the creatures it's being blocked by or is blocking.) */

int card_caverns_of_despair(int player, int card, event_t event){
/* CARD_ID_CAVERNS_OF_DESPAIR	592
Caverns of Despair	|2|R|R
World Enchantment
No more than two creatures can attack each combat.
No more than two creatures can block each combat. */

	if( event == EVENT_ATTACK_LEGALITY && count_attackers(player) > 1 ){
		event_result = 1;
	}
	if( event == EVENT_BLOCK_LEGALITY && count_blockers(player, event) > 1 ){
		event_result = 1;
	}
	return enchant_world(player, card, event);
}

int card_chain_lightning(int player, int card, event_t event){
	/* Chain Lightning	|R	0x200c3d9
	 * Sorcery
	 * ~ deals 3 damage to any target. Then that player or that permanent's controller may pay |R|R. If the player does, they may copy this spell and may choose a new target for that copy. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td)){
			damage_target0(player, card, 3);
			if( has_mana(instance->targets[0].player, COLOR_RED, 2) && can_legally_play_iid(instance->targets[0].player, instance->internal_card_id) ){
				if( do_dialog(instance->targets[0].player, player, card, -1, -1, " Copy Chain Lightning\n Pass", 0) == 0 ){
					charge_mana(instance->targets[0].player, COLOR_RED, 2);
					if( spell_fizzled != 1 ){
						copy_spell(instance->targets[0].player, get_id(player, card));
					}
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_chains_of_mephistopheles(int player, int card, event_t event){

	/* Chains of Mephistopheles	|1|B
	 * Enchantment
	 * If a player would draw a card except the first one he or she draws in his or her draw step each turn, that player discards a card instead. If the player
	 * discards a card this way, he or she draws a card. If the player doesn't discard a card this way, he or she puts the top card of his or her library into
	 * his or her graveyard. */


	card_instance_t *instance = get_card_instance(player, card);

	if( ! is_humiliated(player, card, AL_TRIG) ){
		if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && instance->targets[1].player != 66 && !suppress_draw ){
			if( event == EVENT_TRIGGER){
				event_result |= 2u;
			}
			else if( event == EVENT_RESOLVE_TRIGGER ){
					if( instance->targets[8+reason_for_trigger_controller].player < 0 ){
						instance->targets[8+reason_for_trigger_controller].player = 0;
					}
					instance->targets[8+reason_for_trigger_controller].player++;
					if( instance->targets[8+reason_for_trigger_controller].player > 1 ){
						if( hand_count[reason_for_trigger_controller] > 0 ){
							discard(reason_for_trigger_controller, 0, player);
							instance->targets[1].player = 66;
							draw_cards(reason_for_trigger_controller, 1);
							instance->targets[1].player = 0;
						}
						else{
							mill(reason_for_trigger_controller, 1);
						}
						suppress_draw = 1;
					}
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		instance->targets[8].player = 0;
		instance->targets[9].player = 0;
	}

	return global_enchantment(player, card, event);
}

int card_chromium(int player, int card, event_t event){
/* CARD_ID_CHROMIUM	595
Chromium	|2|W|W|U|U|B|B
Legendary Creature - Elder Dragon 7/7
Flying
Rampage 2 (Whenever this creature becomes blocked, it gets +2/+2 until end of turn for each creature blocking it beyond the first.)
At the beginning of your upkeep, sacrifice ~ unless you pay |W|U|B. */

	check_legend_rule(player, card, event);

	basic_upkeep(player, card, event, 0, 1, 1, 0, 0, 1);

	rampage(player, card, event, 2);

	return 0;
}

int card_cleanse(int player, int card, event_t event){
/* CARD_ID_CLEANSE	596
Cleanse	|2|W|W
Sorcery
Destroy all |Sblack creatures. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		new_manipulate_all(player, card, ANYBODY, &this_test, KILL_DESTROY);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_clergy_of_the_holy_nimbus(int player, int card, event_t event)
{
/* CARD_ID_CLERGY_OF_THE_HOLY_NIMBUS	597
Clergy of the Holy Nimbus	|W
Creature - Human Cleric 1/1
If ~ would be destroyed, regenerate it.
|1: ~ can't be regenerated this turn. Only any opponent may activate this ability. */

  holy_nimbus(player, card, event, 1);
  return 0;
}

int card_cocoon(int player, int card, event_t event)
{
  /* Cocoon	|G
   * Enchantment - Aura
   * Enchant creature you control
   * Enchanted creature doesn't untap during your untap step if ~ has a pupa counter on it.
   * When ~ enters the battlefield, tap enchanted creature and put three pupa counters on ~.
   * At the beginning of your upkeep, remove a pupa counter from ~. If you can't, sacrifice it, put a +1/+1 counter on enchanted creature, and that creature gains flying. */

  char enchanted_player;
  card_instance_t *instance;
  int enchanted_card_or_result;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    instance = get_card_instance(player, card);
    add_counters(player, card, player, card, COUNTER_PUPA, 3);
    tap_card(instance->targets[0].player, instance->targets[0].card);
  }
  instance = in_play(player, card);
  if (((instance != NULL) && (enchanted_card_or_result = is_humiliated(player, card, AL_STATIC),
                              enchanted_card_or_result == 0)) &&
      (instance = get_card_instance(player, card), -1 < instance->damage_target_player))
  {
    instance = get_card_instance(player, card);
    enchanted_player = instance->damage_target_player;
    enchanted_card_or_result = instance->damage_target_card;
    if (count_counters(player, card, COUNTER_PUPA) != 0)
    {
      does_not_untap((int)enchanted_player, enchanted_card_or_result, event);
    }
  }
  instance = in_play(player, card);
  if (!(((instance == NULL) || (enchanted_card_or_result = is_humiliated(player, card, AL_STATIC),
                                enchanted_card_or_result != 0)) ||
        (instance = get_card_instance(player, card), instance->damage_target_player < '\0')))
  {

    instance = get_card_instance(player, card);
    enchanted_player = instance->damage_target_player;
    enchanted_card_or_result = instance->damage_target_card;
    upkeep_trigger_ability(player, card, event, player);
    if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    {
      if (count_counters(player, card, COUNTER_PUPA) == 0)
      {
        add_counters(player, card, (int)enchanted_player, enchanted_card_or_result, COUNTER_P1_P1,
                     1);
        pump_ability_until_eot(player, card, (int)enchanted_player, enchanted_card_or_result, 0, 0,
                               0x20, -0x80000000);
        kill_card(player, card, KILL_SACRIFICE);
        return 0;
      }
      remove_counters(player, card, player, card, COUNTER_PUPA, 1);
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
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td.allowed_controller = player;
  return targeted_aura(player, card, event, &td, "ASHNODS_BATTLEGEAR");
}

int card_concordant_crossroads(int player, int card, event_t event)
{
  /* Concordant Crossroads	|G
   * World Enchantment
   * All creatures have haste. */

  // 0x416220



  boost_subtype(player, card, event, -1, 0,0, 0,SP_KEYWORD_HASTE, BCT_INCLUDE_SELF);
  return enchant_world(player, card, event);
}

int card_cosmic_horror(int player, int card, event_t event){
/* CARD_ID_COSMIC_HORROR	600
Cosmic Horror	|3|B|B|B
Creature - Horror 7/7
First strike
At the beginning of your upkeep, destroy ~ unless you pay |3|B|B|B. If ~ is destroyed this way, it deals 7 damage to you. */

	if( basic_upkeep_unpaid(player, card, event, MANACOST_XB(3, 3)) ){
		damage_player(player, 7, player, card);
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_craw_giant(int player, int card, event_t event)
{
  /* Craw Giant	|3|G|G|G|G
   * Creature - Giant 6/4
   * Trample
   * Rampage 2 */

  // 0x415ca0

  /* Craw Giant	|3|G|G|G|G
   * Creature - Giant 6/4
   * Trample
   * Rampage 2 */

  /* Frost Giant	|3|R|R|R
   * Creature - Giant 4/4
   * Rampage 2 */

  /* Wolverine Pack	|2|G|G
   * Creature - Wolverine 2/4
   * Rampage 2 */

  rampage(player, card, event, 2);
  return 0;
}

int card_crevasse(int player, int card, event_t event){
/* CARD_ID_CREVASSE	602
Crevasse	|2|R
Enchantment
Creatures with |H2mountainwalk can be blocked as though they didn't have |H2mountainwalk. */

	landwalk_disabling_card(player, card, event, PB_MOUNTAINWALK_DISABLED);
	return global_enchantment(player, card, event);
}

// crimson kobolds --> vanilla

int card_crimson_manticore(int player, int card, event_t event){
/* CARD_ID_CRIMSON_MANTICORE	604
Crimson Manticore	|2|R|R
Creature - Manticore 2/2
Flying
|R, |T: ~ deals 1 damage to target attacking or blocking creature. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_IN_COMBAT;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
		damage_target0(player, card, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_ATTACKING_BLOCKING_CREATURE");
}

// Crookshank Kobolds --> vanilla

int card_cyclopean_mummy(int player, int card, event_t event){
/* CARD_ID_CYCLOPEAN_MUMMY	606
Cyclopean Mummy	|1|B
Creature - Zombie 2/1
When ~ dies, exile it. */

	if (event == EVENT_GRAVEYARD_FROM_PLAY && in_play(player, card) && affect_me(player, card) && !check_for_death_trigger_removal(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		if( instance->kill_code >= KILL_DESTROY && instance->kill_code <= KILL_SACRIFICE ){
			instance->kill_code = KILL_EXILE;
		}
	}
	return 0;
}

int card_davenant_archer(int player, int card, event_t event){
/* CARD_ID_DAVENANT_ARCHER	607
D'Avenant Archer	|2|W
Creature - Human Soldier Archer 1/2
|T: ~ deals 1 damage to target attacking or blocking creature. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_IN_COMBAT;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
		damage_target0(player, card, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target attacking or blocking creature.");
}

int card_dakkon_blackblade(int player, int card, event_t event){
	/* Dakkon Blackblade	|2|W|U|U|B
	 * Legendary Creature - Human Warrior 100/100
	 * ~'s power and toughness are each equal to the number of lands you control. */
	/* Molimo, Maro-Sorcerer	|4|G|G|G
	 * Legendary Creature - Elemental 100/100
	 * Trample
	 * ~'s power and toughness are each equal to the number of lands you control. */

	check_legend_rule(player, card, event);

	if( (event == EVENT_TOUGHNESS || event == EVENT_POWER) && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		set_characteristic_pt(count_subtype(player, TYPE_LAND, -1));
	}

	return 0;
}

int card_deadfall(int player, int card, event_t event){
/* CARD_ID_DEADFALL	610
Deadfall	|2|G
Enchantment
Creatures with |H2forestwalk can be blocked as though they didn't have |H2forestwalk. */

	landwalk_disabling_card(player, card, event, PB_FORESTWALK_DISABLED);
	return global_enchantment(player, card, event);
}

int card_demonic_torment(int player, int card, event_t event){
/* CARD_ID_DEMONIC_TORMENT	611
Demonic Torment	|2|B
Enchantment - Aura
Enchant creature
Enchanted creature can't attack.
Prevent all combat damage that would be dealt by enchanted creature. */

	if (event == EVENT_CHECK_ABILITIES){
		card_instance_t* attached = get_card_instance(player, card);
		if (affect_me(attached->damage_target_player, attached->damage_target_card) && in_play(player, card)){
			check_abilities_power -= get_power(attached->damage_target_player, attached->damage_target_card);
		}
	}
	card_instance_t* instance;

	if( (instance = in_play(player, card)) && instance->damage_target_player != -1 ){
		cannot_attack(instance->damage_target_player, instance->damage_target_card, event);

		card_instance_t* damage = combat_damage_being_prevented(event);
		if( damage &&
			damage->damage_source_player == instance->damage_target_player && damage->damage_source_card == instance->damage_target_card )
		{
			prevent_damage(affected_card_controller, affected_card, -1);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, 1-player);
}

int card_disharmony(int player, int card, event_t event){
/* CARD_ID_DISHARMONY	613
Disharmony	|2|R
Instant
Cast ~ only during combat before blockers are declared.
Untap target attacking creature and remove it from combat. Gain control of that creature until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	if (event == EVENT_CAN_CAST && current_phase != PHASE_DECLARE_ATTACKERS && current_phase != PHASE_BEFORE_BLOCKING){ return 0; }

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
			remove_from_combat(instance->targets[0].player, instance->targets[0].card);
			if( instance->targets[0].player != player ){
				gain_control_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target attacking creature.", 1, NULL);
}

int card_divine_intervention(int player, int card, event_t event)
{
  /* Divine Intervention	|6|W|W	0x200d667
   * Enchantment
   * ~ enters the battlefield with two intervention counters on it.
   * At the beginning of your upkeep, remove an intervention counter from ~.
   * When you remove the last intervention counter from ~, the game is a draw. */

  enters_the_battlefield_with_counters(player, card, event, COUNTER_INTERVENTION, 2);
  upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	remove_counters(player, card, player, card, COUNTER_INTERVENTION, 1);

  if (event == EVENT_STATIC_EFFECTS && in_play(player, card))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (count_counters(player, card, COUNTER_INTERVENTION) > 0)
		inst->info_slot = CARD_ID_DIVINE_INTERVENTION;
	  else if (inst->info_slot == CARD_ID_DIVINE_INTERVENTION)	// had counters previously; doesn't anymore
		{
		  if (is_humiliated(player, card, AL_STATIC))
			inst->info_slot = 0;	// so it doesn't trigger later when it stops being humiliated
		  else
			lose_the_game(ANYBODY);
		}
	}

  if (event == EVENT_SHOULD_AI_PLAY)
	ai_modifier += 24 * (life[HUMAN] - life[AI]);

  return global_enchantment(player, card, event);
}

int card_divine_transformation(int player, int card, event_t event){
/* CARD_ID_DIVINE_TRANSFORMATION	616
Divine Transformation	|2|W|W
Enchantment - Aura
Enchant creature
Enchanted creature gets +3/+3. */

	return generic_aura(player, card, event, player, 3, 3, 0, 0, 0, 0, 0);
}

int card_divine_offering(int player, int card, event_t event){
/* CARD_ID_DIVINE_OFFERING	615
Divine Offering	|1|W
Instant
Destroy target artifact. You gain life equal to its converted mana cost. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = get_cmc(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			gain_life(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
}

int card_dream_coat(int player, int card, event_t event){
	/* Dream Coat	|U
	 * Enchantment - Aura
	 * Enchant creature
	 * |0: Enchanted creature becomes the color or colors of your choice. Activate this ability only once each turn. */

	card_instance_t *instance = get_card_instance(player, card);

	if( in_play(player, card) && instance->damage_target_player != -1 ){
		if( instance->targets[1].card > -1 && event == EVENT_SET_COLOR && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			event_result = instance->targets[1].card;
		}

		if( event == EVENT_CLEANUP ){
			instance->targets[2].player = 0;
		}

		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST0, 0, NULL, NULL);
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			int clr = 1<<get_deck_color(player, 1-player);
			if( player != AI ){
				clr = 0;
				while( 1 ){
						int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
											"Black", !(clr & COLOR_TEST_BLACK), 1,
											"Blue", !(clr & COLOR_TEST_BLUE), 1,
											"Green", !(clr & COLOR_TEST_GREEN), 1,
											"Red", !(clr & COLOR_TEST_RED), 1,
											"White", !(clr & COLOR_TEST_WHITE), 1);
						if( choice ){
							clr |= (1<<choice);
						}
						else{
							break;
						}
				}
			}
			get_card_instance(instance->parent_controller, instance->parent_card)->targets[1].card = clr;
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, player);
}

int card_dwarven_song(int player, int card, event_t event){
/* CARD_ID_DWARVEN_SONG	619
Dwarven Song	|R
Instant
Any number of target creatures become |Sred until end of turn. */

	return song(player, card, event, COLOR_TEST_RED);
}

int card_elder_land_wurm(int player, int card, event_t event){
  /* Elder Land Wurm	|4|W|W|W
   * Creature - Dragon Wurm 5/5
   * Defender, trample
   * When this creature blocks, it loses defender. */

 /* Ruling for Elder Land Wurm: 10/4/2004: Another effect can give Elder Land Wurm defender after it has lost it. Blocking again will
  * cause it to lose that instance of defender too. */
	if (current_turn == 1-player && blocking(player, card, event) && !is_humiliated(player, card, AL_TRIG) &&
		check_for_ability(player, card, KEYWORD_DEFENDER))
	{
		// Probably should look for other instances of the this-loses-defender effect and get rid of them first to reduce clutter.
		int leg = pump_ability_until_eot(player, card, player, card, 0, 0, KEYWORD_DEFENDER, SP_KEYWORD_DOES_NOT_END_AT_EOT);
		if (leg != -1)
			get_card_instance(player, leg)->targets[4].player = 0;	// remove keyword
	}

	return 0;
}

int card_elder_spawn(int player, int card, event_t event){
/* CARD_ID_ELDER_SPAWN	621
Elder Spawn	|4|U|U|U
Creature - Spawn 6/6
At the beginning of your upkeep, unless you sacrifice |Han Island, sacrifice ~ and it deals 6 damage to you.
~ can't be blocked by |Sred creatures. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int kill = 1;
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, get_hacked_land_text(player, card, "Select %a card to sacrifice.", SUBTYPE_ISLAND));
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_ISLAND);
		if( new_can_sacrifice_as_cost(player, card, &this_test) ){
			int ai_choice = 0;
			if( (! can_attack(player, card) && life[player] > 11) ||
				check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test) < count_upkeeps(player)
			  ){
				ai_choice = 1;
			}
			int choice = do_dialog(player, player, card, -1, -1,
			                       get_hacked_land_text(player, card, " Select %s to sacrifice\n or sacrifice Elder Spawn.", SUBTYPE_ISLAND), 
								                         ai_choice);
			if( choice == 0 ){
				if( new_sacrifice(player, card, player, 0, &this_test) ){
					kill--;
				}
			}
		}
		if( kill == 1 ){
			damage_player(player, 6, player, card);
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( event == EVENT_BLOCK_LEGALITY && attacking_card_controller == player && attacking_card == card && ! is_humiliated(player, card, AL_STATIC) ){
		if( get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_RED) ){
			event_result = 1;
		}
	}

	return 0;
}

int card_elven_riders(int player, int card, event_t event)
{
/* CARD_ID_ELVEN_RIDERS	622
Elven Riders	|3|G|G
Creature - Elf 3/3
~ can't be blocked except by Walls and/or creatures with flying. */

  // 0x4c41a0
  if (event == EVENT_BLOCK_LEGALITY
	  && attacking_card_controller == player && attacking_card == card
	  && !check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING)
	  && !has_subtype(affected_card_controller, affected_card, SUBTYPE_WALL)
	  && !is_humiliated(player, card, AL_STATIC))
	event_result = 1;

  return 0;
}

int card_emerald_dragonfly(int player, int card, event_t event)
{
  /* CARD_ID_EMERALD_DRAGONFLY	623
Emerald Dragonfly	|1|G
Creature - Insect 1/1
Flying
|G|G: ~ gains first strike until end of turn. */

  if (event == EVENT_CHECK_ABILITIES && affect_me(player, card) &&
      generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_G(2), 0, NULL, NULL))
    check_abilities_keywords |= KEYWORD_FIRST_STRIKE;
  return generic_shade(player, card, event, 0, MANACOST_G(2), 0, 0, KEYWORD_FIRST_STRIKE, 0);
}

int card_enchanted_being(int player, int card, event_t event)
{
/* CARD_ID_ENCHANTED_BEING	624
Enchanted Being	|1|W|W
Creature - Human 2/2
Prevent all combat damage that would be dealt to ~ by enchanted creatures. */

  card_instance_t* damage = combat_damage_being_prevented(event);
  if (damage
	  && damage->damage_target_card == card && damage->damage_target_player == player
	  && (damage->targets[3].player & TYPE_CREATURE) // probably redundant to status check
	  && is_enchanted(damage->damage_source_player, damage->damage_source_card)
	  && ! is_humiliated(player, card, AL_STATIC))
	damage->info_slot = 0;

  return 0;
}

static const char* ee_legal_target(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	card_instance_t* instance = get_card_instance(player, card);
	if( instance->damage_target_player > -1 ){
		int type = get_type(instance->damage_target_player, instance->damage_target_card);
		if( type & (TYPE_LAND | TYPE_CREATURE) ){
			if( call_card_function(player, card, EVENT_CAN_MOVE_AURA) ){
				return NULL;
			}
		}
	}
	return "cannot move this aura";
}

int card_enchantment_alteration(int player, int card, event_t event){
/* CARD_ID_ENCHANTMENT_ALTERATION	625
Enchantment Alteration	|U
Instant
Attach target Aura attached to a creature or land to another permanent of that type. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);
	td.required_subtype = SUBTYPE_AURA;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(ee_legal_target);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int sf3 = player == 0 ? SF3_ENCHANTMENT_ALTERATION_P0 : SF3_ENCHANTMENT_ALTERATION_P1;
			set_special_flags3(instance->targets[0].player, instance->targets[0].card, sf3);
			set_special_flags(instance->targets[0].player, instance->targets[0].card, SF_MOVE_AURA_LEGAL_TARGET);
			if( call_card_function(instance->targets[0].player, instance->targets[0].card, EVENT_CAN_MOVE_AURA) ){
				call_card_function(instance->targets[0].player, instance->targets[0].card, EVENT_MOVE_AURA);
				call_card_function(instance->targets[0].player, instance->targets[0].card, EVENT_RESOLVE_MOVING_AURA);
			}
			remove_special_flags3(instance->targets[0].player, instance->targets[0].card, sf3);
			remove_special_flags(instance->targets[0].player, instance->targets[0].card, SF_MOVE_AURA_LEGAL_TARGET);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target Aura enchanting a creature or a land", 1, NULL);
}

int card_energy_tap(int player, int card, event_t event){
/* CARD_ID_ENERGY_TAP	626
Energy Tap	|U
Sorcery
Tap target untapped creature you control. If you do, add an amount of |C to your mana pool equal to that creature's converted mana cost. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int cmc = get_cmc(instance->targets[0].player, instance->targets[0].card);
			tap_card(instance->targets[0].player, instance->targets[0].card);
			produce_mana(player, COLOR_COLORLESS, cmc);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target untapped creature you control.", 1, NULL);
}

int is_land_destroyer(int player, int card){
	return 0;
}

static const char* land_killer_spell(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	if( is_land_destroyer(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int k;
		for(k=0; k<instance->number_of_targets; k++){
			if( instance->targets[k].player == who_chooses && instance->targets[k].card != -1 ){
				if( is_what(instance->targets[k].player, instance->targets[k].card, TYPE_LAND) ){
					return NULL;
				}
			}
		}
	}
	return "must be a spell that directly destroy one or more of your lands";
}

int card_equinox(int player, int card, event_t event){
/* CARD_ID_EQUINOX	627
Equinox	|W
Enchantment - Aura
Enchant land
Enchanted land has "|T: Counter target spell if it would destroy a land you control." */

	card_instance_t *instance = get_card_instance(player, card);

	if( in_play(player, card) && instance->damage_target_player > -1 ){
		if( ! IS_GAA_EVENT(event)  ){
			return 0;
		}
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		target_definition_t td1;
		counterspell_target_definition(player, card, &td1, TYPE_SPELL);
		td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		td1.extra = target_predicate_extra(land_killer_spell);

		if( event == EVENT_CAN_ACTIVATE ){
			card_instance_t *enc = get_card_instance(p, c);
			if (validate_target(p, c, &td1, 0) && pre_counterspell(player, card, enc->targets[0].player, enc->targets[0].card)){
				kill_card(enc->targets[0].player, enc->targets[0].card, KILL_BURY);
			}
		}
		return granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED | GAA_SPELL_ON_STACK, MANACOST0, 0, &td1, "TARGET_SPELL");
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.allowed_controller = player;
	td.preferred_controller = player;

	if( event == EVENT_RESOLVE_SPELL ){
		return targeted_aura(player, card, event, &td, "TARGET_LAND");
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_AURA, &td, "TARGET_LAND", 1, NULL);
}

int card_eternal_warrior(int player, int card, event_t event){
/* CARD_ID_ETERNAL_WARRIOR	628
Eternal Warrior	|R
Enchantment - Aura
Enchant creature
Enchanted creature has vigilance. */

	return generic_aura(player, card, event, player, 0, 0, 0, SP_KEYWORD_VIGILANCE, 0, 0, 0);
}

int card_eureka(int player, int card, event_t event){
/* CARD_ID_EUREKA	629
Eureka	|2|G|G
Sorcery
Starting with you, each player may put a permanent card from his or her hand onto the battlefield.
Repeat this process until no one puts a card onto the battlefield. */

	// Used externally
	if( event == EVENT_RESOLVE_SPELL ){
		char msg[100] = "Choose a permanent to put into play.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, msg);

    int first, second;
    do
    {
      first = new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
      second = new_global_tutor(1-player, 1-player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
    } while (first >= 0 && second >= 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_evil_eye_of_orms_by_gore(int player, int card, event_t event){
/* CARD_ID_EVIL_EYE_OF_ORMS_BY_GORE	630
Evil Eye of Orms-by-Gore	|4|B
Creature - Eye 3/6
Non-Eye creatures you control can't attack.
~ can't be blocked except by Walls. */

	if( event == EVENT_ATTACK_LEGALITY && affected_card_controller == player && ! is_humiliated(player, card, AL_STATIC) &&
		! has_subtype(affected_card_controller, affected_card, SUBTYPE_EYE) ){
		event_result = 1;
	}

	if( event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card, AL_STATIC) ){
		if( player == attacking_card_controller && card == attacking_card ){
			if( ! has_subtype(affected_card_controller, affected_card, SUBTYPE_WALL) ){
				event_result = 1;
			}
		}
	}

	return 0;
}

int card_fallen_angel(int player, int card, event_t event){
/* CARD_ID_FALLEN_ANGEL	631
Fallen Angel	|3|B|B
Creature - Angel 3/3
Flying
Sacrifice a creature: ~ gets +2/+1 until end of turn. */

	return generic_husk(player, card, event, TYPE_CREATURE, 2, 1, 0, 0);
}

int card_fallen_star(int player, int card, event_t event){
/* CARD_ID_FALLING_STAR	632
Falling Star	|2|R
Sorcery
Flip ~ onto the playing area from a height of at least one foot. ~ deals 3 damage to each creature it lands on.
Tap all creatures dealt damage by ~. If ~ doesn't turn completely over at least once during the flip, it has no effect. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( internal_rand(100) < 75 ){
			int affected_creatures = MIN(count_subtype(ANYBODY, TYPE_CREATURE, -1), internal_rand(4)+1);
			int found_creatures = 0;
			while( found_creatures < affected_creatures ){
					int p = 0;
					for(p=0; p<2; p++){
						int c;
						for(c=0; c<active_cards_count[p]; c++){
							if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
								if( internal_rand(100)+1 > 50 ){
									add_state(p, c, STATE_TARGETTED);
									found_creatures++;
								}
							}
							if( found_creatures == affected_creatures ){
								break;
							}
						}
						if( found_creatures == affected_creatures ){
							break;
						}
					}
			}
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.state = STATE_TARGETTED;
			new_damage_all(player, card, ANYBODY, 3, 0, &this_test);
			int p = 0;
			for(p=0; p<2; p++){
				int c;
				for(c=0; c<active_cards_count[p]; c++){
					if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) && check_state(p, c, STATE_TARGETTED) ){
						tap_card(p, c);
						remove_state(p, c, STATE_TARGETTED);
					}
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

static void tap_and_maze_of_ith(int player, int card, int t_player, int t_card){
	tap_card(t_player, t_card);
	maze_of_ith_effect(player, card, t_player, t_card);
}

int card_feint(int player, int card, event_t event){
/* CARD_ID_FEINT	633
Feint	|R
Instant
Tap all creatures blocking target attacking creature. Prevent all combat damage that would be dealt this turn by that creature and each creature blocking it. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			maze_of_ith_effect(player, card, instance->targets[0].player, instance->targets[0].card);
			for_each_creature_blocking_me(instance->targets[0].player, instance->targets[0].card, tap_and_maze_of_ith,
											instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target attacking creature.", 1, NULL);
}

int card_field_of_dreams(int player, int card, event_t event){
/* CARD_ID_FIELD_OF_DREAMS	634
Field of Dreams	|U
World Enchantment
Players play with the top card of their libraries revealed. */

  if (event == EVENT_CAN_CAST)
	return 1;

  card_instance_t *instance = get_card_instance(player, card);
  int p;

  if (event == EVENT_RESOLVE_SPELL)
	for (p = 0; p <= 1; ++p)
	  {
		instance->targets[p].player = player;
		if (deck_ptr[p][0] == -1)
		  instance->targets[p].card = -1;
		else
		  instance->targets[p].card = create_card_name_legacy(player, card, cards_data[deck_ptr[p][0]].id);
	  }

  if (affect_me(player, card) && instance->targets[0].player >= 0)
	{
	  for (p = 0; p <= 1; ++p)
		if (instance->targets[p].card == -1)
		  {
			if (deck_ptr[p][0] != -1)	// was empty, now has a card
			instance->targets[p].card = create_card_name_legacy(instance->targets[p].player, card, cards_data[deck_ptr[p][0]].id);
		  }
		else
		  {
			if (deck_ptr[p][0] == -1)	// had a card, now empty
			  {
				kill_card(instance->targets[p].player, instance->targets[p].card, KILL_EXILE);
				instance->targets[p].card = -1;
			  }
			else	// had a card, still does
			  {
				card_instance_t* legacy = get_card_instance(instance->targets[p].player, instance->targets[p].card);
				int csvid = cards_data[deck_ptr[p][0]].id;
				legacy->info_slot = -csvid - 1;	// update csvid if it's different
			  }
		  }
	}

  return enchant_world(player, card, event);
}

int card_fire_sprites(int player, int card, event_t event){
/* CARD_ID_FIRE_SPRITES	635
Fire Sprites	|1|G
Creature - Faerie 1/1
Flying
|G, |T: Add |R to your mana pool. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_G(1), 0, NULL, NULL) ){
			return can_produce_mana(player, card);
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_G(1)) ){
			mana_producer(player, card, event);
		}
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && can_produce_mana(player, card) ){
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_G(1), 0, NULL, NULL) ){
			mana_producer(player, card, event);
		}
	}

	return 0;
}

int firestorm_phoenix_legacy(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( current_turn != instance->targets[0].player && event == EVENT_CLEANUP ){
		remove_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_BLOOD, 1);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_firestorm_phoenix(int player, int card, event_t event){
/* CARD_ID_FIRESTORM_PHOENIX	636
Firestorm Phoenix	|4|R|R
Creature - Phoenix 3/2
Flying
If ~ would die, return ~ to its owner's hand instead. Until that player's next turn, that player plays with that card revealed in his or her hand and can't play it. */

	if( event == EVENT_MODIFY_COST && count_counters(player, card, COUNTER_BLOOD) ){
		infinite_casting_cost();
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		remove_counters(player, card, player, card, COUNTER_BLOOD, 1);
	}

	int owner, position;
	if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) && find_in_owners_graveyard(player, card, &owner, &position)){
		int iid = get_grave(owner)[position];
		obliterate_card_in_grave(owner, position);
		int card_added = add_card_to_hand(owner, iid);
		add_counters(player, card, player, card_added, COUNTER_BLOOD, 1);
		int legacy = create_legacy_effect(player, card, &firestorm_phoenix_legacy);
		get_card_instance(player, legacy)->targets[0].player = player;
		get_card_instance(player, legacy)->targets[0].card = card_added;
		add_status(player, legacy, STATUS_INVISIBLE_FX);
	}

	return 0;
}

int card_flash_counter(int player, int card, event_t event){
/* CARD_ID_FLASH_COUNTER	637
Flash Counter	|1|U
Instant
Counter target instant spell. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_INSTANT | TYPE_INTERRUPT);

	return counterspell(player, card, event, &td, 0);
}

int card_flash_flood(int player, int card, event_t event){
/* CARD_ID_FLASH_FLOOD	638
Flash Flood	|U
Instant
Choose one -
* Destroy target |Sred permanent.
* Return target |H2Mountain to its owner's hand. */

	return leg_color_hoser(player, card, event, COLOR_RED, SUBTYPE_MOUNTAIN);
}

int card_floral_spuzzem(int player, int card, event_t event){
/* CARD_ID_FLORAL_SPUZZEM	639
Floral Spuzzem	|3|G
Creature - Elemental 2/2
Whenever ~ attacks and isn't blocked, you may destroy target artifact defending player controls. If you do, ~ assigns no combat damage this turn. */

	card_instance_t *instance = get_card_instance(player, card);

	if( ! is_humiliated(player, card, AL_TRIG) && event == EVENT_DECLARE_BLOCKERS && (instance->state & STATE_ATTACKING) &&
		is_unblocked(player, card) )
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.allowed_controller =  1-player;

		instance->number_of_targets = 0;
		if( can_target(&td) && pick_target(&td, "TARGET_ARTIFACT") ){
			negate_combat_damage_this_turn(player, card, player, card, 0);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_force_spike(int player, int card, event_t event){
	/* Force Spike	|U
	 * Instant
	 * Counter target spell unless its controller pays |1. */
	// Used externally
	if( event == EVENT_RESOLVE_SPELL ){
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

int card_forethought_amulet(int player, int card, event_t event){
/* CARD_ID_FORETHOUGHT_AMULET	641
Forethought Amulet	|5
Artifact
At the beginning of your upkeep, sacrifice ~ unless you pay |3.
If an instant or sorcery source would deal 3 or more damage to you, it deals 2 damage to you instead. */

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == -1 && damage->damage_target_player == player && damage->info_slot > 0 ){
				if( (damage->targets[3].player & TYPE_SPELL) && !(damage->targets[3].player & TYPE_CREATURE) ){
					int amount = damage->info_slot-2;
					if( amount > 0 ){
						prevent_damage(affected_card_controller, affected_card, amount);
					}
				}
			}
		}
	}

	basic_upkeep(player, card, event, MANACOST_X(3));

	return 0;
}

int card_fortified_area(int player, int card, event_t event){
/* CARD_ID_FORTIFIED_AREA	642
Fortified Area	|1|W|W
Enchantment
Wall creatures you control get +1/+0 and have banding. (Any creatures with banding, and up to one without, can attack in a band.
Bands are blocked as a group. If any creatures with banding you control are blocking or being blocked by a creature,
you divide that creature's combat damage, not its controller, among any of the creatures it's being blocked by or is blocking.) */
 // 0x4b8b90

  boost_creature_type(player, card, event, SUBTYPE_WALL, 1, 0, KEYWORD_BANDING, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
  return global_enchantment(player, card, event);
}

int card_gabriel_angelfire(int player, int card, event_t event){
/* CARD_ID_GABRIEL_ANGELFIRE	644
Gabriel Angelfire	|3|G|G|W|W
Legendary Creature - Angel 4/4
At the beginning of your upkeep, choose flying, first strike, trample, or rampage 3. ~ gains that ability until your next upkeep.
(Whenever a creature with rampage 3 becomes blocked, it gets +3/+3 until end of turn for each creature blocking it beyond the first.) */

	check_legend_rule(player, card, event);

	if( current_turn == player && upkeep_trigger(player, card, event) ){
		int choice = do_dialog(player, player, card, -1, -1,
								" Gabriel gains flying\n Gabriel gains first strike\n Gabriel gains trample\n Gabriel gains rampage: 3",
								internal_rand(4));
		get_card_instance(player, card)->info_slot = 66 + choice;
	}

	if(event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		int result = get_card_instance(player, card)->info_slot;
		if( result == 66){
			event_result |= KEYWORD_FLYING;
		}

		if( result == 67){
			event_result |= KEYWORD_FIRST_STRIKE;
		}

		if( result == 68){
			event_result |= KEYWORD_TRAMPLE;
		}
	}

	if( get_card_instance(player, card)->info_slot == 69 ){
		rampage(player, card, event, 3);
	}

	return 0;
}

int card_gaseous_form(int player, int card, event_t event){
/* CARD_ID_GASEOUS_FORM	645
Gaseous Form	|2|U
Enchantment - Aura
Enchant creature
Prevent all combat damage that would be dealt to and dealt by enchanted creature. */

	if (event == EVENT_CHECK_ABILITIES){
		card_instance_t* instance = get_card_instance(player, card);
		if (affect_me(instance->damage_target_player, instance->damage_target_card) && in_play(player, card)){
			check_abilities_power -= get_power(instance->damage_target_player, instance->damage_target_card);
			check_abilities_toughness += 0x3fff;
		}
	}
	card_instance_t* instance;

	if( (instance = in_play(player, card)) && instance->damage_target_player != -1 && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t* damage = combat_damage_being_prevented(event);
		if( damage && ((damage->token_status & STATUS_COMBAT_DAMAGE) || (damage->token_status & STATUS_FIRST_STRIKE_DAMAGE)) ){
			if( (damage->damage_source_player == instance->damage_target_player && damage->damage_source_card == instance->damage_target_card) ||
				(damage->damage_target_player == instance->damage_target_player && damage->damage_target_card == instance->damage_target_card)
		      ){
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, 1-player);
}

int card_gauntlets_of_chaos(int player, int card, event_t event){
	/* Gauntlets of Chaos	|5
	 * Artifact
	 * |5, Sacrifice ~: Exchange control of target artifact, creature, or land you control and target permanent an opponent controls
		that shares one of those types with it. If those permanents are exchanged this way, destroy all Auras attached to them. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_LAND);
	td1.allowed_controller = player;
	td1.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_LAND);
	td2.allowed_controller = 1-player;
	td2.preferred_controller = 1-player;


	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(5), 0, &td1, "TARGET_PERMANENT") ){
			return can_target(&td2);
		}
	}

	if(event == EVENT_ACTIVATE){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(5)) ){
			if( new_pick_target(&td1, "Select target artifact, creature, or land you control.", 0, 1 | GS_LITERAL_PROMPT) ){
				td2.required_type = get_type(instance->targets[0].player, instance->targets[0].card) & (TYPE_ARTIFACT | TYPE_CREATURE | TYPE_LAND);
				if( can_target(&td2) ){
					if( new_pick_target(&td2, "Select target artifact, creature, or land an opponent controls.", 1, 1 | GS_LITERAL_PROMPT) ){
						kill_card(player, card, KILL_SACRIFICE);
					}
				}
				else{
					spell_fizzled = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( validate_target(player, card, &td1, 0) &&  validate_target(player, card, &td2, 1) ){
			int new_cards[2];
			new_cards[1-player] = gain_control_permanently(1-player, instance->targets[0].player, instance->targets[0].card);
			new_cards[player] = gain_control_permanently(player, instance->targets[1].player, instance->targets[1].card);
			APNAP(p,{
						int c;
						for(c = active_cards_count[p]-1; c > -1; c--){
							if( in_play(p, c) && is_what(p, c, TYPE_ENCHANTMENT) && has_subtype(p, c, SUBTYPE_AURA) ){
								card_instance_t *this = get_card_instance(p, c);
								if( (this->damage_target_player == p && this->damage_target_card == new_cards[p]) ||
									(this->damage_target_player == 1-p && this->damage_target_card == new_cards[1-p]) )
								{
									kill_card(p, c, KILL_DESTROY);
								}
							}
						}
					}
			);
		}
	}

	return 0;
}

int card_ghosts_of_the_damned(int player, int card, event_t event){
/* CARD_ID_GHOSTS_OF_THE_DAMNED	647
Ghosts of the Damned	|1|B|B
Creature - Spirit 0/2
|T: Target creature gets -1/-0 until end of turn. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, -1, 0);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

static int giant_slug_legacy(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( current_turn == player && instance->damage_target_player > -1 && upkeep_trigger(player, card, event) ){
		int ai_choice = get_deck_color(player, 1-player)-1;
		int choice = do_dialog(player, instance->damage_target_player, instance->damage_target_card, -1, -1,
								" Swampwalk\n Islandwalk\n Forestwalk\n Mountainwalk\n Plainswalk", ai_choice);
		int key = 1 << choice;
		pump_ability_until_eot(instance->damage_target_player, instance->damage_target_card,
								instance->damage_target_player, instance->damage_target_card, 0, 0, key, 0);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_giant_slug(int player, int card, event_t event){
/* CARD_ID_GIANT_SLUG	648
Giant Slug	|1|B
Creature - Slug 1/1
|5: At the beginning of your next upkeep, choose a basic land type.
~ gains landwalk of the chosen type until the end of that turn. (It can't be blocked as long as defending player controls a land of that type.) */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &giant_slug_legacy,
										instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_X(5), 0, NULL, NULL);
}

int card_giant_turtle(int player, int card, event_t event){
  /* Giant Turtle |1|G|G
   * Creature � Turtle 2/4
   * Giant Turtle can't attack if it attacked during your last turn. */

	if( event == EVENT_DECLARE_ATTACKERS && is_attacking(player, card) && ! is_humiliated(player, card, AL_STATIC) )
		get_card_instance(player, card)->targets[1].player = 66;

	if (event == EVENT_CLEANUP && affect_me(player, card))
		if (player == current_turn)
			get_card_instance(player, card)->info_slot = get_card_instance(player, card)->state & STATE_ATTACKED;

	if( get_card_instance(player, card)->info_slot && ! is_humiliated(player, card, AL_STATIC) )
		cannot_attack(player, card, event);

	return 0;
}

static int glyph_of_delusion_wall_card = 0;
/* ManalinkEh 2022: 0x21f3eb0; original cards/legends.c:2732. */
int card_giant_strength(int player, int card, event_t event)
{
  /* CARD_ID_GIANT_STRENGTH	649
   * Giant Strength	|R|R
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+2. */

  int result;
  result = generic_aura(player, card, event, player, 2, 2, 0, 0, 0, 0, 0);
  return result;
}
static const char* target_is_blocked_by_glyph_of_delusion_wall_card(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
	if (player == current_turn && is_blocking(glyph_of_delusion_wall_card, card)){
		return NULL;
	} else {
		return "not blocked by Wall";
	}
}

int card_glyph_of_delusion(int player, int card, event_t event)
{
  /* Glyph of Delusion	|U
   * Instant
   * Put X glyph counters on target creature that target Wall blocked this turn, where X is the power of that blocked creature. The creature gains "This
   * creature doesn't untap during your untap step if it has a glyph counter on it" and "At the beginning of your upkeep, remove a glyph counter from this
   * creature." */

  card_instance_t *instance;
  int result;
  target_definition_t td_wall;
  target_definition_t td_blocked_by_wall;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td_wall, 2);
    td_wall.preferred_controller = player;
    td_wall.required_state = TARGET_STATE_BLOCKING;
    td_wall.required_subtype = 0xd1;
    default_target_definition(player, card, &td_blocked_by_wall, 2);
    get_card_instance(player, card);
    result = can_target(&td_wall);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td_wall, 2);
      td_wall.preferred_controller = player;
      td_wall.required_state = TARGET_STATE_BLOCKING;
      td_wall.required_subtype = 0xd1;
      default_target_definition(player, card, &td_blocked_by_wall, 2);
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        result = pick_next_target_noload(&td_wall, "Select target blocking Wall.");
        if (result != 0)
        {
          if (instance->targets[0].player != 1 - current_turn)
          {
            backtrace_and_abort(
                "cards/legends.c:2799\ninstance->targets[0].player == 1-current_turn");
          }
          glyph_of_delusion_wall_card = instance->targets[0].card;
          td_blocked_by_wall.extra = target_predicate_extra(target_is_blocked_by_glyph_of_delusion_wall_card);
          td_blocked_by_wall.special = TARGET_SPECIAL_EXTRA_FUNCTION;
          result = can_target(&td_blocked_by_wall);
          if (result == 0)
          {
            spell_fizzled = 1;
          }
          else
          {
            result = pick_next_target_noload(&td_blocked_by_wall,
                                             "Select target creature blocked by target Wall.");
            if (result != 0)
            {
              ai_modifier = ai_modifier + 0x18;
            }
          }
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td_wall, 2);
    td_wall.preferred_controller = player;
    td_wall.required_state = TARGET_STATE_BLOCKING;
    td_wall.required_subtype = 0xd1;
    default_target_definition(player, card, &td_blocked_by_wall, 2);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = valid_target(&td_wall);
      if (result != 0)
      {
        result = validate_target(player, card, &td_blocked_by_wall, 1);
        if (result != 0)
        {
          result = get_power(instance->targets[1].player, instance->targets[1].card);
          add_counters(player, card, instance->targets[1].player,
                                   instance->targets[1].card, COUNTER_GLYPH, result);
          gains_doesnt_untap_while_has_a_counter_and_remove_a_counter_at_upkeep(
              player, card, instance->targets[1].player, instance->targets[1].card, COUNTER_GLYPH);
        }
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td_wall, 2);
    td_wall.preferred_controller = player;
    td_wall.required_state = TARGET_STATE_BLOCKING;
    td_wall.required_subtype = 0xd1;
    default_target_definition(player, card, &td_blocked_by_wall, 2);
    get_card_instance(player, card);
  }
  return 0;
}

static int glyph_of_destruction_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_target_card == c && damage->damage_target_player == p &&
					damage->info_slot > 0 )
				{
				  prevent_damage(affected_card_controller, affected_card, damage->info_slot);
				}
			}
		}

		if( current_turn == 1-player && eot_trigger(player, card, event) ){
			kill_card(p, c, KILL_DESTROY);
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

static int glyph_of_destruction_boost_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_POWER && affect_me(p, c) ){
			event_result += 10;
		}
		//if (current_turn == player && current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2)
		if(end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY)){
		kill_card(player, card, KILL_DESTROY);
		}
	}
	return 0;
}

int card_glyph_of_destruction(int player, int card, event_t event){
  /* Glyph of Destruction |R
   * Instant
   * Target blocking Wall you control gets +10/+0 until end of combat.
   * Prevent all damage that would be dealt to it this turn.
   * Destroy it at the beginning of the next end step. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_BLOCKING;
	td.required_subtype = SUBTYPE_WALL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &glyph_of_destruction_legacy,
											instance->targets[0].player, instance->targets[0].card);
			create_targetted_legacy_effect(player, card, &glyph_of_destruction_boost_legacy,
											instance->targets[0].player, instance->targets[0].card);								
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target blocking Wall you control.", 1, NULL);
}

static int glyph_of_doom_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( end_of_combat_trigger(player, card, event, 2) ){
			kill_card(p, c, KILL_DESTROY);
		}
	}

	return 0;
}

static void add_glyph_of_doom_legacy(int player, int card, int t_player, int t_card){
	create_targetted_legacy_effect(player, card, &glyph_of_doom_legacy, t_player, t_card);
}

int card_glyph_of_doom(int player, int card, event_t event){
/* CARD_ID_GLYPH_OF_DOOM	653
Glyph of Doom	|B
Instant
Choose target Wall creature. At this turn's next end of combat, destroy all creatures that were blocked by that creature this turn. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_BLOCKING;
	td.required_subtype = SUBTYPE_WALL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target blocking Wall you control.", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier += 24;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if (valid_target(&td)){
			for_each_creature_blocked_by_me(instance->targets[0].player, instance->targets[0].card, add_glyph_of_doom_legacy, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int glyph_of_life_legacy(int player, int card, event_t event)
{
  card_instance_t* damage = damage_being_dealt(event);
  if (damage
	  && (damage->targets[3].player & TYPE_CREATURE)
	  && is_attacking(damage->damage_source_player, damage->damage_source_card))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (damage->damage_target_card == instance->damage_target_card
		  && damage->damage_target_player == instance->damage_target_player)
		gain_life(player, damage->info_slot);
	}

  if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

int card_glyph_of_life(int player, int card, event_t event){
/* CARD_ID_GLYPH_OF_LIFE	654
Glyph of Life	|W
Instant
Choose target Wall creature. Whenever that creature is dealt damage by an attacking creature this turn, you gain that much life. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_WALL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target Wall.", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier += 24;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &glyph_of_life_legacy, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int glyph_of_reincarnation_legacy(int player, int card, event_t event){

	if( end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance(player, card);
		instance->damage_target_player = instance->damage_target_card = -1;
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		if( count_graveyard_by_type(instance->targets[0].player, TYPE_CREATURE) > 0 ){
			char msg[100] = "Select a creature card.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, msg);
			int selected = new_select_a_card(player, instance->targets[0].player, TUTOR_FROM_GRAVE, 1, AI_MIN_CMC, -1, &this_test);
			reanimate_permanent(instance->targets[0].player, -1, instance->targets[0].player, selected, REANIMATE_DEFAULT);
		}
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_glyph_of_reincarnation(int player, int card, event_t event){
  /* Glyph of Reincarnation |G
   * Instant
   * Cast Glyph of Reincarnation only after combat.
   * Destroy all creatures that were blocked by target Wall this turn. They can't be regenerated.
   * For each creature that died this way, put a creature card from the graveyard of the player who controlled that creature the last
   * 	time it became blocked by that Wall onto the battlefield under its owner's control. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_BLOCKING;
	td.required_subtype = SUBTYPE_WALL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target blocking Wall", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier += 24;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			char marked[2][151] = {{0}};

			mark_each_creature_blocked_by_me(instance->targets[0].player, instance->targets[0].card, marked);

			APNAP(p,
					{
						int c;
						for (c = 0; c < active_cards_count[p]; ++c){
							if (marked[p][c] && in_play(p, c)){
								create_targetted_legacy_effect(player, card, &glyph_of_reincarnation_legacy, p, c);
							}
						}
					}
			);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_gosta_dirk(int player, int card, event_t event){
/* CARD_ID_GOSTA_DIRK	656
Gosta Dirk	|3|W|W|U|U
Legendary Creature - Human Warrior 4/4
First strike
Creatures with |H2islandwalk can be blocked as though they didn't have |H2islandwalk. */

	check_legend_rule(player, card, event);
	landwalk_disabling_card(player, card, event, PB_ISLANDWALK_DISABLED);
	return 0;
}

int card_gravity_sphere(int player, int card, event_t event)
{
  /* Gravity Sphere	|2|R
   * World Enchantment
   * All creatures lose flying. */

  // 0x416280



  if (event == EVENT_ABILITIES && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
	  && in_play(player, card) && in_play(affected_card_controller, affected_card)
	  && !is_humiliated(player, card, AL_STATIC))
	event_result &= ~KEYWORD_FLYING;

  return enchant_world(player, card, event);
}

int card_great_defender(int player, int card, event_t event){
/* CARD_ID_GREAT_DEFENDER	658
Great Defender	|W
Instant
Target creature gets +0/+X until end of turn, where X is its converted mana cost. */

	if( event == EVENT_CHECK_PUMP ){
		card_instance_t *instance = get_card_instance(player, card);
		if ( has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id) ){
			if( instance->targets[0].player > -1 && instance->targets[0].card > -1 ){
				pumpable_toughness[player] += get_cmc(instance->targets[0].player, instance->targets[0].card);

			}
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	if (event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0,
							get_cmc(instance->targets[0].player, instance->targets[0].card));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_great_wall(int player, int card, event_t event){
/* CARD_ID_GREAT_WALL	659
Great Wall	|2|W
Enchantment
Creatures with |H2plainswalk can be blocked as though they didn't have |H2plainswalk. */

	landwalk_disabling_card(player, card, event, PB_PLAINSWALK_DISABLED);
	return global_enchantment(player, card, event);
}

static const char* red_black_damage_source(int who_chooses, int player, int card, int targeting_player, int targeting_card){
	card_instance_t* dmg = get_card_instance(player, card);
	if ( dmg->internal_card_id == damage_card ){
		int clr = dmg->initial_color;
		if( in_play(dmg->damage_source_player, dmg->damage_source_card) ){
			clr = get_color(dmg->damage_source_player, dmg->damage_source_card);
		}
		if( clr & (get_sleighted_color_test(targeting_player, targeting_card, COLOR_TEST_RED) |
					get_sleighted_color_test(targeting_player, targeting_card, COLOR_TEST_BLACK))
		  ){
			return NULL;
		}
	}

	return gs_target_error_color;//",color"
}

int card_greater_realm_of_protection(int player, int card, event_t event){
/* CARD_ID_GREATER_REALM_OF_PRESERVATION	660
Greater Realm of Preservation	|1|W
Enchantment
|1|W: The next time a |Sblack or |Sred source of your choice would deal damage to you this turn, prevent that damage. */

	if( in_play(player, card) && IS_GAA_EVENT(event)){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_EFFECT);
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION | TARGET_SPECIAL_DAMAGE_PLAYER;
		td.extra = target_predicate_extra(red_black_damage_source);

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				get_card_instance(instance->targets[0].player, instance->targets[0].card)->info_slot = 0;
			}
		}

		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_XW(1, 1), 0,
										&td, "TARGET_DAMAGE");
	}

	return global_enchantment(player, card, event);
}

int card_greed(int player, int card, event_t event){
/* CARD_ID_GREED	661
Greed	|3|B
Enchantment
|B, Pay 2 life: Draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}
	if( event == EVENT_ACTIVATE ){
		ai_modifier += ((life[player]-14) + (count_subtype(player, TYPE_LAND, -1) * 2));
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}
	return generic_activated_ability(player, card, event, 0, MANACOST_B(1), 2, NULL, NULL);
}

int card_green_mana_battery(int player, int card, event_t event)
{
  /* Green Mana Battery	|4
   * Artifact
   * |2, |T: Put a charge counter on this artifact.
   * |T, Remove any number of charge counters from this artifact: Add |G, then add an additional |G for each charge counter removed this way. */

  // 0x4235A0
  return mana_battery(player, card, event, COLOR_GREEN);
}

int card_gwendlyn_di_corci(int player, int card, event_t event){
/* CARD_ID_GWENDLYN_DI_CORCI	663
Gwendlyn Di Corci	|U|B|B|R
Legendary Creature - Human Rogue 3/5
|T: Target player discards a card at random. Activate this ability only during your turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (21-(hand_count[player]*3));
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			discard(instance->targets[0].player, DISC_RANDOM, player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_IN_YOUR_TURN, MANACOST0, 0,
									&td, "TARGET_PLAYER");
}

int card_halfdane(int player, int card, event_t event){
  /* Halfdane |1|W|U|B
   * Legendary Creature — Shapeshifter 3/3
   * At the beginning of your upkeep, change Halfdane's base power and toughness to the power and toughness of target creature
   * 	other than Halfdane until the end of your next upkeep. */

	check_legend_rule(player, card, event);

	if( !is_humiliated(player, card, AL_TRIG) && current_turn == player && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.special = TARGET_SPECIAL_NOT_ME;

		if( can_target(&td) ){
			upkeep_trigger_ability(player, card, event, player);
		}
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.special = TARGET_SPECIAL_NOT_ME;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "Select another target creature.", 0, GS_LITERAL_PROMPT) ){
			int legacy = real_set_pt(player, card, player, card, get_power(instance->targets[0].player, instance->targets[0].card),
						get_toughness(instance->targets[0].player, instance->targets[0].card), 4);
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

static int hammerheim_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_ABILITIES && affect_me(p, c) ){
			int i;
			for(i=0; i<5; i++){
				if( event_result & (1<<i) ){
					event_result &= ~(1<<i);
				}
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_hammerheim(int player, int card, event_t event){
/* CARD_ID_HAMMERHEIM	665
Hammerheim	""
Legendary Land
|T: Add |R to your mana pool.
|T: Target creature loses all landwalk abilities until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			instance->info_slot = do_dialog(player, player, card, -1, -1, " Produce mana\n Remove landwalk\n Cancel", 1);
		}

		if( instance->info_slot == 0){
			return mana_producer(player, card, event);
		}

		if( instance->info_slot == 1 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST0) && pick_target(&td, "TARGET_CREATURE") ){
				add_state(player, card, STATE_TAPPED);
				tapped_for_mana_color = -1;
			}
		}
		if( instance->info_slot == 2 ){
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
			if( valid_target(&td) ){
				create_targetted_legacy_effect(player, instance->parent_card, &hammerheim_legacy, instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

static int legacy_ht(int player, int card, event_t event){

	if( current_turn == player && upkeep_trigger(player, card, event) ){
		int warriors = count_permanents_by_type(player, TYPE_LAND);
		generate_tokens_by_id(player, card, CARD_ID_SAND_WARRIOR, warriors);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

// headless horseman --> vanilla
int card_hazezon_tamar(int player, int card, event_t event)
{
  /* Hazezon Tamar	|4|R|G|W
   * Legendary Creature - Human Warrior 2/4
   * When ~ enters the battlefield, put X 1/1 Sand Warrior creature tokens that are |Sred, |Sgreen, and |Swhite
   * 		onto the battlefield at the beginning of your next upkeep, where X is the number of lands you control at that time.
   * When Hazezon leaves the battlefield, exile all Sand Warriors. */

  card_instance_t *instance;
  int result;
  int card_00;
  int player_00;
  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    create_legacy_effect(player, card, legacy_ht);
    leaves_play(player, card, EVENT_ETB, 1, 1);
    return 0;
  }
  leaves_play(player, card, event, 1, 1);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    player_00 = 0;
    do
    {
      card_00 = active_cards_count[player] + -1;
      if (-1 < card_00)
      {
        do
        {
          instance = in_play(player_00, card_00);
          if ((((instance != NULL) && (result = is_what(player_00, card_00, 2), result != 0)) &&
               (result = is_token(player_00, card_00), result != 0)) &&
              ((result = has_subtype(player_00, card_00, SUBTYPE_WARRIOR),
                result != 0 &&
                    (result = has_subtype(player_00, card_00, SUBTYPE_SAND), result != 0))))
          {
            kill_card(player_00, card_00, KILL_EXILE);
          }
          card_00 = card_00 + -1;
        } while (card_00 != -1);
      }
      player_00 = player_00 + 1;
    } while (player_00 != 2);
    return 0;
  }
  return 0;
}

int card_heavens_gate(int player, int card, event_t event){
/* CARD_ID_HEAVENS_GATE	668
Heaven's Gate	|W
Instant
Any number of target creatures become |Swhite until end of turn. */

	return song(player, card, event, COLOR_TEST_WHITE);
}

int card_hell_swarm(int player, int card, event_t event){
/* CARD_ID_HELL_SWARM	669
Hell Swarm	|B
Instant
All creatures get -1/-0 until end of turn. */

	if (event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, ANYBODY, -1, -1, 0, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_hells_caretaker(int player, int card, event_t event){
/* CARD_ID_HELLS_CARETAKER	670
Hell's Caretaker	|3|B
Creature - Horror 1/1
|T, Sacrifice a creature: Return target creature card from your graveyard to the battlefield. Activate this ability only during your upkeep.*/

	if( event == EVENT_CAN_ACTIVATE ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) > 0 && ! graveyard_has_shroud(player)){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_ONLY_ON_UPKEEP | GAA_IN_YOUR_TURN | GAA_SACRIFICE_CREATURE, MANACOST0, 0,
											NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature to sacrifice.");

		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &this_test);
			if (!sac){
				cancel = 1;
				return 0;
			}

			get_card_instance(player, card)->number_of_targets = 0;
			int selected = select_target_from_grave_source(player, card, player, 0, AI_MAX_CMC, &this_test, 0);
			if( selected != -1 ){
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				add_state(player, card, STATE_TAPPED);
				tapped_for_mana_color = -1;
			}
			else{
				state_untargettable(BYTE2(sac), BYTE3(sac), 0);
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
		}
	}

	return 0;
}

// holy day --> fog

int card_hellfire(int player, int card, event_t event){
/* CARD_ID_HELLFIRE	671
Hellfire	|2|B|B|B
Sorcery
Destroy all non|Sblack creatures. ~ deals X plus 3 damage to you, where X is the number of creatures that died this way. */

	if(event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		this_test.color_flag = DOESNT_MATCH;
		int result = new_manipulate_all(player, card, ANYBODY, &this_test, KILL_DESTROY);

		damage_player(player, 3+result, player, card);

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

// hornet cobra

int card_horn_of_deafening(int player, int card, event_t event){
/* CARD_ID_HORN_OF_DEAFENING	673
Horn of Deafening	|4
Artifact
|2, |T: Prevent all combat damage that would be dealt by target creature this turn. */

	// original code : 00405F70

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		negate_combat_damage_this_turn(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 0);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_CREATURE");
}

int card_horror_of_horrors(int player, int card, event_t event){
/* CARD_ID_HORROR_OF_HORRORS	675
Horror of Horrors	|3|B|B
Enchantment
Sacrifice |Ha Swamp: Regenerate target |Sblack creature.
(The next time that creature would be destroyed this turn, it isn't. Instead tap it, remove all damage from it, and remove it from combat.) */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_LAND, get_hacked_land_text(player, card, "Select a %s to sacrifice.", SUBTYPE_SWAMP));
	this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_SWAMP);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.preferred_controller = player;
	td1.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
	td1.required_state = TARGET_STATE_DESTROYED;
	if( player == AI ){
		td1.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( new_can_sacrifice_as_cost(player, card, &this_test) ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_REGENERATION, MANACOST0, 0, &td1, "TARGET_CREATURE");
		}
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &this_test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		char msg[100];
		scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select target %s creature to regenerate", COLOR_BLACK));
		if( new_pick_target(&td1, msg, 0, 1 | GS_LITERAL_PROMPT) ){
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
		else{
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int
 card_hunding_gjornersen(int player, int card, event_t event){
/* CARD_ID_HUNDING_GJORNERSEN	676
Hunding Gjornersen	|3|W|U|U
Legendary Creature - Human Warrior 5/4
Rampage 1 (Whenever this creature becomes blocked, it gets +1/+1 until end of turn for each creature blocking it beyond the first.) */

	// also code for Marhault Elsdragon
	check_legend_rule(player, card, event);
	rampage(player, card, event, 1);
	return 0;
}

int card_hyperion_blacksmith(int player, int card, event_t event)
{
/* CARD_ID_HYPERION_BLACKSMITH	677
Hyperion Blacksmith	|1|R|R
Creature - Human Artificer 2/2
|T: You may tap or untap target artifact an opponent controls. */

  // 0x429f90

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	// |T: You may tap or untap target artifact an opponent controls.
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.allowed_controller = td.preferred_controller = 1-player;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
		twiddle(player, card, 0);

	int rval = generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(0), 0, &td, "HYPERION_BLACKSMITH");

	if (event == EVENT_ACTIVATE && player == AI && cancel != 1)
		ai_modifier_twiddle(player, card, 0);

	return rval;
}

int card_ichneumon_druid(int player, int card, event_t event){
  /* Ichneumon Druid |1|G|G
   * Creature — Human Druid 1/1
   * Whenever an opponent casts an instant spell other than the first instant spell that player casts each turn, Ichneumon Druid deals
   * 	4 damage to him or her. */

	if( event == EVENT_CAST_SPELL && affected_card_controller == 1-player &&
		is_what(affected_card_controller, affected_card, TYPE_INSTANT | TYPE_INTERRUPT) )
	{
		get_card_instance( player, card)->info_slot++;
	}

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		if (get_card_instance(player, card)->info_slot > 1){
			sp_ability_trigger(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_INSTANT | TYPE_INTERRUPT, NULL, player, card);
		}
	}

	if (event == EVENT_SPA){ damage_player(1-player, 4, player, card); return 0; }

	if( event == EVENT_RESOLVE_SPELL || event == EVENT_CLEANUP ){
		get_card_instance( player, card)->info_slot = 0;
	}

	return 0;
}

int card_immolation(int player, int card, event_t event){
/* CARD_ID_IMMOLATION	679
Immolation	|R
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/-2. */

	if (event == EVENT_CHECK_PUMP){	// will only be sent to this card if it has flash, e.g. for Maggot Therapy
		return vanilla_instant_pump(player, card, event, ANYBODY, ANYBODY, 2, -2, 0, 0);
	}

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_POWER && affect_me(p, c) ){
			event_result += 2;
		}

		if( event == EVENT_TOUGHNESS && affect_me(p, c) ){
			event_result -= 2;
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			if( instance->targets[0].player == player ){
				if( get_toughness(instance->targets[0].player, instance->targets[0].card) <= 2 ){
					ai_modifier -= 48;
				}
				else{
					ai_modifier += 12;
				}
			}
			if( instance->targets[0].player != player ){
				if( get_toughness(instance->targets[0].player, instance->targets[0].card) <= 2 ){
					ai_modifier += 48;
				}
				else{
					ai_modifier -= 12;
				}
			}
		}
		return 0;
	}

	return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

static int imprison_ability(int player, int card){
	if( has_mana(player, COLOR_COLORLESS, 1) ){
		if( do_dialog(player, player, card, -1, -1, " Activate Imprison\n Pass", 0) == 0 ){
			charge_mana(player, COLOR_COLORLESS, 1);
			if( spell_fizzled != 1 ){
				return 1;
			}
		}
	}
	return 0;
}

int card_imprison(int player, int card, event_t event){
/* CARD_ID_IMPRISON	680
Imprison	|B
Enchantment - Aura
Enchant creature
Whenever a player activates an ability of enchanted creature with |T in its activation cost that isn't a mana ability, you may pay |1.
If you do, counter that ability. If you don't, destroy ~.
Whenever enchanted creature attacks or blocks, you may pay |1. If you do, tap the creature, remove it from combat,
and creatures it was blocking that had become blocked by only that creature this combat become unblocked. If you don't, destroy ~. */

	if (in_play(player, card) && get_card_instance(player, card)->damage_target_player >= 0 && !is_humiliated(player, card, AL_TRIG)){
	    card_instance_t* instance = get_card_instance(player, card);
	    int p = instance->damage_target_player, c = instance->damage_target_card;
	    if (event == EVENT_DECLARE_ATTACKERS && is_attacking(p, c)){
	        if (imprison_ability(player, card)){ remove_state(p, c, STATE_ATTACKING); tap_card(p, c); }
	        else { kill_card(player, card, KILL_DESTROY); }
	    }
	    if (event == EVENT_DECLARE_BLOCKERS && blocking(p, c, event)){
	        if (imprison_ability(player, card)){
	            get_card_instance(p, c)->blocking = 255;
	            remove_state(p, c, STATE_BLOCKING);
	            remove_state(p, c, STATE_UNKNOWN8000);
	            tap_card(p, c);
	        } else { kill_card(player, card, KILL_DESTROY); }
	    }
	}
	if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS){ return disabling_aura(player, card, event); }
	return 0;
}

int card_in_the_eye_of_chaos(int player, int card, event_t event){
  /* In the Eye of Chaos |2|U
   * World Enchantment
   * Whenever a player casts an instant spell, counter it unless that player pays {X}, where X is its converted mana cost. */

	enchant_world(player, card, event);
	sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_INSTANT | TYPE_INTERRUPT, NULL, player, card);
	if (event == EVENT_SPA){
		int cmc = get_cmc(trigger_cause_controller, trigger_cause);
		if (!has_mana(trigger_cause_controller, COLOR_ANY, cmc) || !charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, trigger_cause_controller, COLOR_ANY, cmc) || spell_fizzled == 1){
			real_counter_a_spell(player, card, trigger_cause_controller, trigger_cause);
		}
	}
	return global_enchantment(player, card, event);
}

int card_indestructible_aura(int player, int card, event_t event){
/* CARD_ID_INDESTRUCTIBLE_AURA	682
Indestructible Aura	|W
Instant
Prevent all damage that would be dealt to target creature this turn. */

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
			ai_modifier += (instance->targets[0].player == player ? 24 : -24);
		}
		return 0;
	}

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			prevent_all_damage_to_target(player, card, instance->targets[0].player, instance->targets[0].card, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

static void destroy_nonwall_at_end_of_combat(int player, int card, int t_player, int t_card)
{
  if (!has_subtype(t_player, t_card, SUBTYPE_WALL))
	create_legacy_effect_exe(player, card, LEGACY_EFFECT_STONING, t_player, t_card);
}

static void destroy_creature_at_end_of_combat(int player, int card, int t_player, int t_card)
{
  create_legacy_effect_exe(player, card, LEGACY_EFFECT_STONING, t_player, t_card);
}

int card_infernal_medusa(int player, int card, event_t event)
{
/* CARD_ID_INFERNAL_MEDUSA	683
Infernal Medusa	|3|B|B
Creature - Gorgon 2/4
Whenever ~ blocks a creature, destroy that creature at end of combat.
Whenever ~ becomes blocked by a non-Wall creature, destroy that creature at end of combat. */

  // 0x4ca9e0
  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG))
	instance->destroys_if_blocked |= (current_turn == player ? DIFB_DESTROYS_NONWALLS : DIFB_DESTROYS_ALL);

  if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card, AL_TRIG))
	{
	  if (player == current_turn && (instance->state & STATE_ATTACKING))
		for_each_creature_blocking_me(player, card, destroy_nonwall_at_end_of_combat, player, card);

	  if (player == 1-current_turn && (instance->state & STATE_BLOCKING))
		for_each_creature_blocked_by_me(player, card, destroy_creature_at_end_of_combat, player, card);
	}

  return 0;
}

static int infinite_autority_legacy(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( end_of_combat_trigger(player, card, event, 2) ){
		instance->damage_target_player = instance->damage_target_card = -1; //Unattach the legacy or will be removed along the creature
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		instance->targets[2].card = 66;
	}

	if( eot_trigger(player, card, event) ){
		if( instance->targets[2].card == 66 && in_play(instance->targets[1].player, instance->targets[1].card) ){
			add_counters(player, card, instance->targets[1].player, instance->targets[1].card, COUNTER_P1_P1, 1);
		}
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

static void create_infinite_autority_legacy(int player, int card, int t_player, int t_card)
{
	if( get_toughness(t_player, t_card) < 4 ){
		int legacy = create_targetted_legacy_effect(player, card, &infinite_autority_legacy, t_player, t_card);
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *leg = get_card_instance(player, legacy);
		leg->targets[1].player = instance->damage_target_player;
		leg->targets[1].card = instance->damage_target_card;
		instance->number_of_targets = 2;
	}
}

int card_infinite_autority(int player, int card, event_t event){
/* CARD_ID_INFINITE_AUTHORITY	684
Infinite Authority	|W|W|W
Enchantment - Aura
Enchant creature
Whenever enchanted creature blocks or becomes blocked by a creature with toughness 3 or less, destroy the other creature at end of combat.
At the beginning of the next end step, if that creature was destroyed this way, put a +1/+1 counter on the first creature. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( current_turn == player && is_attacking(p, c) ){
			for_each_creature_blocking_me(p, c, create_infinite_autority_legacy, player, card);
		}
		if( current_turn != player && blocking(p, c, event) ){
			for_each_creature_blocked_by_me(p, c, create_infinite_autority_legacy, player, card);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, player);
}

static int get_permanents_color(int player, int type, int flag){

	int result = 0;
	int count = 0;

	while( count < active_cards_count[player] ){
			if( in_play(player, count) ){
				if( flag != is_what(player, count, type) ){
					result |= get_color(player, count);
				}
			}
			count++;
	}

	return result;
}

int card_invoke_prejudice(int player, int card, event_t event){
/* Invoke Prejudice	|U|U|U|U
 * Enchantment
 * Whenever an opponent casts a creature spell that doesn't share a color with a creature you control, counter that spell unless
 * its controller pays |X, where X is its converted mana cost. */

	enchant_world(player, card, event);
	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.color = get_permanents_color(player, TYPE_CREATURE, 0);
		test.color_flag = DOESNT_MATCH;
		sp_ability_trigger(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){
		int counter = 1;
		int cmc = get_cmc(trigger_cause_controller, trigger_cause);
		if (has_mana(trigger_cause_controller, COLOR_ANY, cmc) &&
			charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, trigger_cause_controller, COLOR_ANY, cmc)){
			if (spell_fizzled != 1){ counter = 0; }
		}
		if (counter){ real_counter_a_spell(player, card, trigger_cause_controller, trigger_cause); }
	}
	return global_enchantment(player, card, event);
}

int card_ivory_guardians(int player, int card, event_t event){
/* CARD_ID_IVORY_GUARDIANS	686
Ivory Guardians	|4|W|W
Creature - Giant Cleric 3/3
Protection from |Sred
Creatures named ~ get +1/+1 as long as an opponent controls a nontoken |Sred permanent. */

	if( ! is_humiliated(player, card, AL_STATIC) && in_play(player, card) ){
		protection_from_red(player, card, event);
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) &&
			get_id(affected_card_controller, affected_card) == CARD_ID_IVORY_GUARDIANS )
		{
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_PERMANENT);
			this_test.type_flag = F1_NO_TOKEN;
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
			if( check_battlefield_for_special_card(player, card, 1-affected_card_controller, 0, &this_test) ){
				event_result++;
			}
		}
	}

	return 0;
}

int card_jacques_le_vert(int player, int card, event_t event)
{
  /* Jacques le Vert	|1|R|G|W
   * Legendary Creature - Human Warrior 3/2
   * |SGreen creatures you control get +0/+2. */

	check_legend_rule(player, card, event);

	boost_creature_by_color(player, card, event, get_sleighted_color_test(player, card, COLOR_TEST_GREEN), 0, 2, 0, BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);

	return 0;
}

static int johan_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		vigilance(p, c, event);

		if( ! in_play(instance->targets[0].player, instance->targets[0].card) ||
			is_tapped(instance->targets[0].player, instance->targets[0].card) ||
			event == EVENT_CLEANUP )
		{
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_johan(int player, int card, event_t event){
  /* Johan |3|W|R|G
   * Legendary Creature — Human Wizard 5/4,  (6)
   * At the beginning of combat on your turn, you may have Johan gain "Johan can't attack" until end of combat.
   * 	If you do, attacking doesn't cause creatures you control to tap this combat if Johan is untapped. */

	check_legend_rule(player, card, event);

	if( (get_card_instance(player, card)->info_slot & 2) && ! is_humiliated(player, card, AL_STATIC) ){
		cannot_attack(player, card, event);
	}

	if( beginning_of_combat(player, card, event, player) && (!is_tapped(player, card) || player != AI) ){
		card_instance_t *instance = get_card_instance(player, card);
		int choice = do_dialog(player, player, card, -1, -1, " Give Vigilance to other creatures\n Pass", is_tapped(player, card) );
		if (choice == 0 && !is_tapped(player, card)){
			instance->info_slot |= 2;
			int count = active_cards_count[player];
			while( count > -1 ){
					if( count != card && in_play(player, count) && is_what(player, count, TYPE_CREATURE) ){
						int legacy = create_targetted_legacy_effect(player, card, &johan_legacy, player, count);
						card_instance_t *leg = get_card_instance(player, legacy);
						leg->targets[0].player = player;
						leg->targets[0].card = card;
						leg->number_of_targets = 1;
					}
					count--;
			}
		}
	}

	return 0;
}

int card_jovial_evil(int player, int card, event_t event){
/* CARD_ID_JOVIAL_EVIL	692
Jovial Evil	|2|B
Sorcery
~ deals X damage to target opponent, where X is twice the number of |Swhite creatures that player controls. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
			damage_player(instance->targets[0].player,
							check_battlefield_for_special_card(player, card, instance->targets[0].player, CBFSC_GET_COUNT, &this_test)*2,
							player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, NULL, NULL, 1, NULL);
}

static int get_permanent_with_highest_cmc(int player, int card, int sel_type, int t_player)
{
  int result = -1;
  int doubles = 0;
  int max_cmc = -1;
  CYCLE_ALL_CARDS(p, c, {
    if (p == t_player && in_play(p, c) && is_what(p, c, sel_type))
    {
      int this_cmc = get_cmc(p, c);
      if (this_cmc == max_cmc)
      {
        doubles++;
      }
      if (this_cmc > max_cmc)
      {
        max_cmc = this_cmc;
        doubles = 0;
        result = c;
      }
    }
  });
  if (doubles)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, sel_type);
    td.illegal_abilities = 0;
    td.extra = max_cmc;
    td.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
    td.allowed_controller = td.preferred_controller = t_player;
    td.who_chooses = t_player;
    td.allow_cancel = 0;

    card_instance_t *instance = get_card_instance(player, card);

    char msg[100];
    char ttext[100];
    scnprintf(msg, 100, "Select a %s you control with CMC %d", type_text(ttext, 100, sel_type, 0),
              max_cmc);
    new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT);
    result = instance->targets[0].card;
    instance->number_of_targets = 0;
  }
  return result;
}

int card_juxtapose(int player, int card, event_t event){
  /* Juxtapose |3|U
   * Sorcery
   * You and target player exchange control of the creature you each control with the highest converted mana cost.
   * Then exchange control of artifacts the same way.
   * If two or more permanents a player controls are tied for highest cost, their controller chooses one of them. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL){
		int result[2] = { 	get_permanent_with_highest_cmc(player, card, TYPE_CREATURE, player),
							get_permanent_with_highest_cmc(player, card, TYPE_CREATURE, 1-player)
		};
		if( result[0] != -1 && result[1] != -1 ){
			gain_control_permanently(player, 1-player, result[1]);
			gain_control_permanently(1-player, player, result[0]);
		}

		result[player] = get_permanent_with_highest_cmc(player, card, TYPE_ARTIFACT, player);
		result[1-player] =	get_permanent_with_highest_cmc(player, card, TYPE_ARTIFACT, 1-player);
		if( result[0] != -1 && result[1] != -1 ){
			gain_control_permanently(player, 1-player, result[1]);
			gain_control_permanently(1-player, player, result[0]);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

// keepers of the faith --> vanilla

int card_karakas(int player, int card, event_t event){
	/* Karakas	""
	 * Legendary Land
	 * |T: Add |W to your mana pool.
	 * |T: Return target legendary creature to its owner's hand. */

	// original code : 0x40D220

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_LEGEND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		int choice = instance->info_slot = instance->number_of_targets = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			choice = do_dialog(player, player, card, -1, -1, " Produce W\n Bounce a legend\n Cancel", 1);
		}
		if( choice == 0 ){
			if (player == AI && !paying_mana()){
				ai_modifier -= 128;
			}
			return mana_producer(player, card, event);
		}
		else if( choice == 1 && charge_mana_for_activated_ability(player, card, MANACOST0) && pick_target(&td, "TARGET_LEGENDARY_CREATURE")){
			tapped_for_mana_color = -1;
			instance->state |= STATE_TAPPED;	// no, not tap_card(), that dispatches a second EVENT_TAP_CARD
			instance->info_slot = 1;
		}
		else{
			cancel = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			if( valid_target(&td)  ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_kei_takahashi(int player, int card, event_t event){
/* CARD_ID_KEI_TAKAHASHI	697
Kei Takahashi	|2|G|W
Legendary Creature - Human Cleric 2/2
|T: Prevent the next 2 damage that would be dealt to target creature this turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.extra = damage_card;
	td.required_type = 0;
	td.special = TARGET_SPECIAL_DAMAGE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION_CREATURE, MANACOST0, 0,
									&td, "TARGET_DAMAGE");
}

int card_killer_bees(int player, int card, event_t event){
/* CARD_ID_KILLER_BEES	698
Killer Bees	|1|G|G
Creature - Insect 0/1
Flying
|G: ~ gets +1/+1 until end of turn. */

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_G(1), 1, 1);
}

int card_kismet(int player, int card, event_t event)
{
  /* Kismet	|3|W
   * Enchantment
   * Artifacts, creatures, and lands played by your opponents enter the battlefield tapped. */

  // 0x4b56e0



	permanents_enters_battlefield_tapped(player, card, event, 1-player, TYPE_ARTIFACT | TYPE_CREATURE | TYPE_LAND, NULL);

	return global_enchantment(player, card, event);
}

int card_knowledge_vault(int player, int card, event_t event)
{
  /* Knowledge Vault	|4
   * Artifact
   * |2, |T: Exile the top card of your library face down.
   * |0: Sacrifice ~. If you do, discard your hand, then put all cards exiled with ~ into their owner's hand.
   * When ~ leaves the battlefield, put all cards exiled with ~ into their owner's graveyard. */

  leaves_play(player, card, event, 1, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
	{
	  int leg = 0, idx = 0, *loc;
	  while ((loc = exiledby_find_any(player, card, &leg, &idx)))
		{
		  int owner = (*loc & 0x80000000) ? 1 : 0;
		  int iid = *loc & ~0x80000000;
		  *loc = -1;
		  int pos = add_card_to_rfg(owner, iid);
		  from_exile_to_graveyard(player, pos);
		}
	}

  if (!IS_GAA_EVENT(event))
	return 0;

  int cuaa = can_use_activated_abilities(player, card);

  enum
  {
	CHOICE_EXILE = 1,
	CHOICE_SACRIFICE
  } choice = DIALOG(player, card, event, DLG_RANDOM,
					"Exile the top card of your library", cuaa, 1, DLG_TAP, DLG_MANA(MANACOST_X(2)),
					"Sacrifice", cuaa && can_sacrifice_this_as_cost(player, card), 1, DLG_MANA(MANACOST_X(0)));

  if (event == EVENT_CAN_ACTIVATE)
	return choice;

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_ACTIVATE)
	switch (choice)
	  {
		case CHOICE_EXILE:
		  if (player == AI && current_turn == 1-player && current_phase == PHASE_DISCARD)
			ai_modifier += 48;
		  break;

		case CHOICE_SACRIFICE:
		  ;int ai_mod = 0;
		  if (instance->targets[0].card != -1)
			{
			  ai_mod -= 12 * BYTE0(instance->targets[0].card);
			  ai_mod += 12 * BYTE1(instance->targets[0].card);
			}
		  ai_modifier += (player == AI) ? ai_mod : -ai_mod;
		  instance->targets[0].player = exiledby_detach(player, card);
		  kill_card(player, card, KILL_SACRIFICE);
		  break;
	  }

  if (event == EVENT_RESOLVE_ACTIVATION)
	switch (choice)
	  {
		case CHOICE_EXILE:
		  if (deck_ptr[player][0] != -1)
			{
			  exiledby_remember(instance->parent_controller, instance->parent_card, player, deck_ptr[player][0], NULL, NULL);
			  play_sound_effect(WAV_DESTROY);
			  rfg_top_card_of_deck(player);

			  // Keep count of cards exiled by each player, for ai modifier
			  if (instance->targets[0].card == -1)
				instance->targets[0].card = 0;

			  if (player == 0)
				++SET_BYTE0(instance->targets[0].card);
			  else
				++SET_BYTE1(instance->targets[0].card);
			}
		  break;

		case CHOICE_SACRIFICE:
			discard_all(player);
		  int leg = 0, idx = 0, *loc;
		  while ((loc = exiledby_find_any(player-2, instance->targets[0].player, &leg, &idx)))
			{
			  int owner = (*loc & 0x80000000) ? 1 : 0;
			  int iid = *loc & ~0x80000000;
			  add_card_to_hand(owner, iid);
			  remove_card_from_rfg(owner, cards_data[iid].id);
			  *loc = -1;
			}

		  exiledby_destroy_detached(player, instance->targets[0].player);
		  break;
	  }

  return 0;
}

int card_kobold_drill_sergeant(int player, int card, event_t event)
{
/* CARD_ID_KOBOLD_DRILL_SERGEANT	701
Kobold Drill Sergeant	|1|R
Creature - Kobold Soldier 1/2
Other Kobold creatures you control get +0/+1 and have trample. */

  boost_creature_type(player, card, event, SUBTYPE_KOBOLD, 0, 1, KEYWORD_TRAMPLE, BCT_CONTROLLER_ONLY);
  return 0;
}

int card_kobold_overlord(int player, int card, event_t event){
/* CARD_ID_KOBOLD_OVERLORD	702
Kobold Overlord	|1|R
Creature - Kobold 1/2
First strike
Other Kobold creatures you control have first strike. */

	boost_creature_type(player, card, event, SUBTYPE_KOBOLD, 0, 0, KEYWORD_FIRST_STRIKE, BCT_CONTROLLER_ONLY);
	return 0;
}

// kobold of kher keep --> vanilla

int card_kobold_taskmaster(int player, int card, event_t event)
{
/* CARD_ID_KOBOLD_TASKMASTER	703
Kobold Taskmaster	|1|R
Creature - Kobold 1/2
Other Kobold creatures you control get +1/+0. */

  boost_creature_type(player, card, event, SUBTYPE_KOBOLD, 1, 0, 0, BCT_CONTROLLER_ONLY);
  return 0;
}

static int kry_shield_effect(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		card_instance_t *damage = damage_being_prevented(event);
		if( damage ){
			if( damage->damage_target_player == p && damage->damage_target_card == c ){
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
		if( event == EVENT_TOUGHNESS && affect_me(p, c) ){
			event_result += get_cmc(p, c);
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_kry_shield(int player, int card, event_t event){
  /* Kry Shield |2
   * Artifact
   * {2}, {T}: Prevent all damage that would be dealt this turn by target creature you control.
   * 	That creature gets +0/+X until end of turn, where X is its converted mana cost. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(instance->parent_controller, instance->parent_card,
											&kry_shield_effect, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_CREATURE");
}

int card_lady_caleria(int player, int card, event_t event){
/* CARD_ID_LADY_CALERIA	706
Lady Caleria	|3|G|G|W|W
Legendary Creature - Human Archer 3/6
|T: ~ deals 3 damage to target attacking or blocking creature. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_IN_COMBAT;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target attacking or blocking creature.");
}

int card_lady_evangela(int player, int card, event_t event)
{
  /* Lady Evangela	|W|U|B
   * Legendary Creature - Human Cleric 1/2
   * |W|B, |T: Prevent all combat damage that would be dealt by target creature this turn. */

  // original code : 0040CD80



	check_legend_rule(player, card, event);
	if (!IS_GAA_EVENT(event)){ return 0; }
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	card_instance_t* instance = get_card_instance(player, card);
	if (event == EVENT_CAN_ACTIVATE){ return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_WB(1, 1), 0, &td, NULL); }
	if (event == EVENT_ACTIVATE){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_WB(1, 1), 0, &td, "TARGET_CREATURE");
		if (spell_fizzled != 1){ ai_modifier += is_attacking(instance->targets[0].player, instance->targets[0].card) ? 48 : -48; }
	}
	if (event == EVENT_RESOLVE_ACTIVATION){ negate_combat_damage_this_turn(player, card, instance->targets[0].player, instance->targets[0].card, 0); }
	return 0;
}

int card_land_equilibrium(int player, int card, event_t event){
  /* Land Equilibrium |2|U|U
   * Enchantment
   * If an opponent who controls at least as many lands as you do would put a land onto the battlefield, that player instead puts that land
   * 	onto the battlefield then sacrifices a land. */

	if( ! is_humiliated(player, card, AL_STATIC) && trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) &&
		reason_for_trigger_controller == 1-player )
	{
		if( count_subtype(player, TYPE_LAND, -1) < count_subtype(1-player, TYPE_LAND, -1) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			if( new_specific_cip_global(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &this_test, player, card) ){
				new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &this_test);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_land_tax(int player, int card, event_t event)
{
/* CARD_ID_LAND_TAX	710
Land Tax	|W
Enchantment
At the beginning of your upkeep, if an opponent controls more lands than you, you may search your library for up to three basic land cards,
reveal them, and put them into your hand. If you do, shuffle your library. */

	if (landsofcolor_controlled[player][COLOR_ANY] < landsofcolor_controlled[1-player][COLOR_ANY]){
		upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));
	}

	if (event == EVENT_UPKEEP_TRIGGER_ABILITY){
		tutor_basic_lands(player, TUTOR_HAND, 3);
	}

	return global_enchantment(player, card, event);
}

static int lands_edge_effect(int player, int card, event_t event)
{
  int p;
  card_instance_t *instance;
  int source_player_or_choice;
  int source_card_or_result;
  test_definition_t *discard_test;
  target_definition_t *damage_target_definition;
  target_definition_t td;
  test_definition_t this_test;
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_player_or_choice = get_id(p, instance->targets[0].card);
        if (source_player_or_choice != 0x2c7)
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
    if (instance->targets[0].player == -1)
    {
      return 0;
    }
    get_card_instance(player, card);
    return 0;
  }
  instance = get_card_instance(player, card);
  if (instance->targets[0].player == -1)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  source_player_or_choice = instance->targets[0].player;
  source_card_or_result = instance->targets[0].card;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 0x100);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  new_default_test_definition(&this_test, 0xff, "Select a card to discard.");
  if (player == 1)
    this_test.type = 1;
  damage_target_definition = &td;
  discard_test = &this_test;
  this_test.zone = 0x100;
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 0, 0, 0, 0, 0, 0, 0,
                                  damage_target_definition, NULL) == 0)
    {
      return 0;
    }
    return check_battlefield_for_special_card(player, card, player, 0, discard_test);
  }
  damage_target_definition = &td;
  discard_test = &this_test;
  this_test.zone = 0x100;
  if (event == EVENT_ACTIVATE)
  {
    instance->number_of_targets = '\x01';
    instance->targets[2].player = 0;
    charge_mana(player, COLOR_COLORLESS,
                get_cost_mod_for_activated_abilities(source_player_or_choice, source_card_or_result,
                                                     0, 0, 0, 0, 0, 0));
    if (spell_fizzled != 1)
    {
      if (player == 1)
      {
        source_player_or_choice = has_mana(1, 7, 1);
        source_card_or_result = get_most_common_cmc_in_hand(1, ~TYPE_NONE);
        ai_modifier = ai_modifier + (source_player_or_choice - source_card_or_result) * 0x50;
        source_player_or_choice = new_select_a_card(1, 1, 4, 0, 3, -1, discard_test);
        if ((source_player_or_choice != -1) &&
            (source_card_or_result =
                 new_pick_target(damage_target_definition, "TARGET_PLAYER_OR_PLANESWALKER", 1, 1),
             source_card_or_result != 0))
        {
          instance->targets[2].player = 0x42;
          discard_card(1, source_player_or_choice);
        }
      }
      else
      {
        source_player_or_choice = new_select_a_card(player, player, 4, 0, 3, -1, discard_test);
        if (source_player_or_choice != -1)
        {
          if (is_what(player, source_player_or_choice, 1) == 0)
          {
            discard_card(player, source_player_or_choice);
          }
          else
          {
            if (new_pick_target(damage_target_definition, "TARGET_PLAYER_OR_PLANESWALKER", 1, 1) !=
                0)
            {
              instance->targets[2].player = 0x42;
              discard_card(player, source_player_or_choice);
            }
          }
        }
      }
    }
  }
  else if (((event == EVENT_RESOLVE_ACTIVATION) && (instance->targets[2].player == 0x42)) &&
           (validate_target(player, card, damage_target_definition, 1) != 0))
  {
    damage_player(instance->targets[1].player, 2, source_player_or_choice, source_card_or_result);
  }
  return 0;
}

int card_lands_edge(int player, int card, event_t event ){
	/* Land's Edge	|1|R|R	(no address assigned)
	 * World Enchantment
	 * Discard a card: If the discarded card was a land card, ~ deals 2 damage to target player or planeswalker. Any player may activate this ability. */
	card_instance_t *instance = get_card_instance(player, card);
	if (event == EVENT_STATIC_EFFECTS && affect_me(player, card)){
        int found = 0;
        for (int p = 0; p < 2; ++p){
            for (int c = 0; c <= active_cards_count[p]; ++c){
                if (in_play(p, c) && is_what(p, c, TYPE_EFFECT)){
                    card_instance_t *inst = get_card_instance(p, c);
                    if (inst->info_slot == (int)lands_edge_effect && inst->targets[0].player == player && inst->targets[0].card == card){
                        if (p == player){ kill_card(player, c, KILL_EXILE); }
                        else { found = 1; if (p == 1){ break; } }
                    }
                }
            }
        }
        if (!found){
            int fake = add_card_to_hand(1-player, instance->internal_card_id);
            int legacy = create_legacy_activate(1-player, fake, lands_edge_effect);
            card_instance_t *inst = get_card_instance(1-player, legacy);
            inst->targets[0].player = player;
            inst->targets[0].card = card;
            inst->number_of_targets = 1;
            obliterate_card(1-player, fake);
        }
    }

	enchant_world(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select a card to discard.");
	this_test.zone = TARGET_ZONE_HAND;
	if( player == AI ){
		this_test.type = TYPE_LAND;
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( player == AI ){
			if( generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, &td, "TARGET_PLAYER_OR_PLANESWALKER") ){
				if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
					return 1;
				}
			}
		}
		else{
			return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = instance->targets[2].player = 0;
		if( player == AI ){
            ai_modifier += (has_mana(player, COLOR_ANY, 1) - get_most_common_cmc_in_hand(player, -1)) * 80;
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
			if( selected != -1 && pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER") ){
				instance->targets[2].player = 66;
				discard_card(player, selected);
			}
		}
		else{
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
			if( selected != -1 ){
				if( is_what(player, selected, TYPE_LAND) ){
					if( pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER") ){
						instance->targets[2].player = 66;
						discard_card(player, selected);
					}
				}
				else{
					discard_card(player, selected);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->targets[2].player == 66 && valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return 0;
}

static const char* is_blocking_lw(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  if (is_blocking(card, targeting_card))
	return NULL;
  else
	return "must be blocking Lesser Werewolf";
}

static const char* lw_is_blocking_him(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  if (is_blocking(targeting_card, card))
	return NULL;
  else
	return "must be blocked by Lessere Werewolf";
}

int card_lesser_werewolf(int player, int card, event_t event){
/* Lesser Werewolf	|3|B
 * Creature - Werewolf 2/4
 * |B: If ~'s power is 1 or more, it gets -1/-0 until end of turn and put a -0/-1 counter on target creature blocking or blocked by ~.
 * Activate this ability only during the declare blockers step. */

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller =  1-player;
	td.preferred_controller = 1-player;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(is_blocking_lw);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller =  1-player;
	td1.preferred_controller = 1-player;
	td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td1.extra = target_predicate_extra(lw_is_blocking_him);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( current_phase == PHASE_AFTER_BLOCKING && generic_activated_ability(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL) ){
			if( current_turn == player ){
				return can_target(&td);
			}
			else{
				return can_target(&td1);
			}
		}
	}
	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int mod = instance->targets[1].player > -1 ? instance->targets[1].player : 0;
		if( player == AI && (get_power(player, card) - mod) < 1 ){
			ai_modifier -= 128;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_B(1)) ){
			if( current_turn == player ){
				if( select_target(player, card, &td, "Select target creature blocking Lesser Werewolf", &(instance->targets[0])) ){
					if( instance->targets[1].player < 0 ){
						instance->targets[1].player = 0;
					}
					instance->targets[1].player++;
				}
				else{
					spell_fizzled = 1;
				}
			}
			else{
				if( select_target(player, card, &td1, "Select target creature blocked by Lesser Werewolf", &(instance->targets[0])) ){
					if( instance->targets[1].player < 0 ){
						instance->targets[1].player = 0;
					}
					instance->targets[1].player++;
				}
				else{
					spell_fizzled = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( (current_turn == player && valid_target(&td)) || (current_turn != player && valid_target(&td1)) ){
			if( get_power(instance->parent_controller, instance->parent_card) > 0 ){
				pump_until_eot(instance->parent_controller, instance->parent_card,
								instance->parent_controller, instance->parent_card, -1, 0);
				add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_M0_M1, 1);
			}
		}
	}

	return 0;
}

int card_life_chisel(int player, int card, event_t event){
  /* Life Chisel |4
   * Artifact
   * Sacrifice a creature: You gain life equal to the sacrificed creature's toughness. Activate this ability only during your upkeep. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, instance->info_slot);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_ONLY_ON_UPKEEP | GAA_IN_YOUR_TURN, MANACOST0, 0, NULL, NULL,
															0, &test, SARV_REPORT_TOUGHNESS);
}
static int effect_life_matrix(int player, int card, event_t event)
{
  if (effect_follows_control_of_attachment(player, card, event))
	return 0;

  card_instance_t* instance = get_card_instance(player, card);
  int p = instance->damage_target_player, c = instance->damage_target_card;

  if (event == EVENT_ABILITIES && affect_me(p, c) && count_counters(p, c, COUNTER_MATRIX))
	event_result |= KEYWORD_REGENERATION;

  if (land_can_be_played & LCBP_REGENERATION)
	{
	  if (event == EVENT_CAN_ACTIVATE)
		return CAN_ACTIVATE0(p, c) && count_counters(p, c, COUNTER_MATRIX) && can_regenerate(p, c) ? 99 : 0;

	  if (event == EVENT_ACTIVATE && charge_mana_for_activated_ability(p, c, MANACOST0))
		remove_counters(player, card, p, c, COUNTER_MATRIX, 1);

	  if (event == EVENT_RESOLVE_ACTIVATION)
		regenerate_target(p, c);
	}

  return 0;
}

int card_life_matrix(int player, int card, event_t event)
{
  /* Life Matrix	|4
   * Artifact
   * |4, |T: Put a matrix counter on target creature and that creature gains "Remove a matrix counter from this creature: Regenerate this creature." Activate
   * this ability only during your upkeep. */

  int result;
  card_instance_t *instance;
  card_instance_t *candidate_legacy;
  int card_00;
  target_definition_t td;
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    if (event == EVENT_ACTIVATE)
    {
      add_state(player, card, 0x10);
      tapped_for_mana_color = -1;
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
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    instance = get_card_instance(player, card);
    add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                 COUNTER_MATRIX, 1);
    result = 0;
    do
    {
      card_00 = 0;
      if (0 < active_cards_count[result])
      {
        do
        {
          candidate_legacy = in_play(result, card_00);
          if ((((candidate_legacy != NULL) &&
                (candidate_legacy->internal_card_id == LEGACY_EFFECT_ACTIVATED)) &&
               (candidate_legacy->info_slot_fn == effect_life_matrix)) &&
              ((candidate_legacy->damage_target_card == instance->targets[0].card &&
                ((int)candidate_legacy->damage_target_player == instance->targets[0].player))))
          {
            return 0;
          }
          card_00 = card_00 + 1;
        } while (card_00 < active_cards_count[result]);
      }
      result = result + 1;
    } while (result != 2);
    create_targetted_legacy_activate(player, card, effect_life_matrix, instance->targets[0].player,
                                     instance->targets[0].card);
  }
  return generic_activated_ability(player, card, event, 0x18004, 4, 0, 0, 0, 0, 0, 0, &td,
                                   "TARGET_CREATURE");
}

int card_lifeblood(int player, int card, event_t event){
	/* Lifeblood	|2|W|W
	 * Enchantment
	 * Whenever |Ha Mountain an opponent controls becomes tapped, you gain 1 life. */
	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( event == EVENT_TAP_CARD && affected_card_controller == 1-player ){
			if( has_subtype(affected_card_controller, affected_card, get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN)) ){
				gain_life(player, 1);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_living_plane(int player, int card, event_t event){
/* CARD_ID_LIVING_PLANE	716
Living Plane	|2|G|G
World Enchantment
All lands are 1/1 creatures that are still lands. */

	if( event == EVENT_CHANGE_TYPE ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);

		global_type_change(player, card, event, 2, TYPE_CREATURE, &this_test, 1, 1, 0, 0, 0);
	}

	enchant_world(player, card, event);

	return global_enchantment(player, card, event);
}

int card_livonya_silone(int player, int card, event_t event){
/* CARD_ID_LIVONYA_SILONE	717
Livonya Silone	|2|R|R|G|G
Legendary Creature - Human Warrior 4/4
First strike; legendary landwalk (This creature can't be blocked as long as defending player controls a legendary land.) */

	check_legend_rule(player, card, event);

	if( event == EVENT_BLOCK_LEGALITY && !(player_bits[player] & PB_NONSTANDARD_LANDWALK_DISABLED) && ! is_humiliated(player, card, AL_STATIC) ){
		if( attacking_card_controller == player && attacking_card == card ){
			test_definition_t test;
			default_test_definition(&test, TYPE_LAND);
			test.subtype = SUBTYPE_LEGEND;
			if( check_battlefield_for_special_card(player, card, 1-current_turn, CBFSC_CHECK_ONLY, &test) ){
				event_result = 1;
			}
		}
	}

	return 0;
}

// lost soul --> vanilla

int card_lord_magnus(int player, int card, event_t event){
	/* Lord Magnus	|3|G|W|W	0x200d66c
	 * Legendary Creature - Human Druid 4/3
	 * First strike
	 * Creatures with |H2plainswalk can be blocked as though they didn't have |H2plainswalk.
	 * Creatures with |H2forestwalk can be blocked as though they didn't have |H2forestwalk. */
	landwalk_disabling_card(player, card, event, PB_PLAINSWALK_DISABLED | PB_FORESTWALK_DISABLED);
	check_legend_rule(player, card, event);
	return 0;
}

int effect_mana_drain(int player, int card, event_t event){

	if( event == EVENT_PHASE_CHANGED ){
		card_instance_t *instance = get_card_instance(player, card);
		if( current_turn == instance->targets[0].player && current_phase == instance->targets[0].card )
		{
			produce_mana(player, COLOR_COLORLESS, instance->targets[1].player);
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_mana_drain(int player, int card, event_t event){
  /* Mana Drain |U|U
   * Instant
   * Counter target spell. At the beginning of your next main phase, add an amount of {C} to your mana pool equal to that spell's converted
   * 	mana cost. */

	if( event == EVENT_CAN_CAST ){
		return counterspell(player, card, event, NULL, 0);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->info_slot = get_cmc(card_on_stack_controller, card_on_stack);
		return counterspell(player, card, event, NULL, 0);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0)  ){
			card_instance_t *instance = get_card_instance(player, card);
			if (pre_counterspell(player, card, instance->targets[0].player, instance->targets[0].card)){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_STATE_BASED_ACTION);
			}

			int legacy_card = create_legacy_effect(player, card, &effect_mana_drain );
			card_instance_t *legacy = get_card_instance(player, legacy_card);
			legacy->targets[0].player = player;
			legacy->targets[0].card = PHASE_MAIN1;
			legacy->targets[1].player = instance->info_slot;
			if( current_turn == player && current_phase < PHASE_MAIN2 ){
				legacy->targets[0].player = PHASE_MAIN2;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_mana_matrix(int player, int card, event_t event){
/* CARD_ID_MANA_MATRIX	721
Mana Matrix	|6
Artifact
Instant and enchantment spells you cast cost up to |2 less to cast. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_INSTANT | TYPE_INTERRUPT | TYPE_ENCHANTMENT)
		  ){
			COST_COLORLESS-=2;
		}
	}
	return 0;
}

int card_marble_priest(int player, int card, event_t event)
{
/* CARD_ID_MARBLE_PRIEST	722
Marble Priest	|5
Artifact Creature - Cleric 3/3
All Walls able to block ~ do so.
Prevent all combat damage that would be dealt to ~ by Walls. */

  subtype_must_block_me(player, card, event, SUBTYPE_WALL, "Choose defending Wall");

  // Prevent all combat damage that would be dealt to ~ by Walls.
  card_instance_t* damage = combat_damage_being_prevented(event);
  if (damage
	  && damage->damage_target_player == player && damage->damage_target_card == card
	  && has_subtype(damage->damage_source_player, damage->damage_source_card, SUBTYPE_WALL))
	prevent_damage(affected_card_controller, affected_card, -1);

  return 0;
}

int card_master_of_the_hunt(int player, int card, event_t event){
	/* Master of the Hunt	|2|G|G
	 * Creature - Human 2/2
	 * |2|G|G: Put a 1/1 |Sgreen Wolf creature token named Wolves of the Hunt onto the battlefield. It has "bands with other creatures named Wolves of the Hunt." */

	if( event == EVENT_RESOLVE_ACTIVATION){
		generate_token_by_id(player, card, CARD_ID_WOLVES_OF_THE_HUNT);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XG(2, 2), 0, NULL, NULL);
}

int card_mirror_universe(int player, int card, event_t event){
/* CARD_ID_MIRROR_UNIVERSE	725
Mirror Universe	|6
Artifact
|T, Sacrifice ~: Exchange life totals with target opponent. Activate this ability only during your upkeep. */
	// Used externally
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td)  ){
			exchange_life_totals(player, 1-player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_ONLY_TARGET_OPPONENT | GAA_SACRIFICE_ME |
									GAA_ONLY_ON_UPKEEP | GAA_IN_YOUR_TURN, MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_moat(int player, int card, event_t event){
/* CARD_ID_MOAT	726
Moat	|2|W|W
Enchantment
Creatures without flying can't attack. */

	if (event == EVENT_ATTACK_LEGALITY && !(get_card_instance(affected_card_controller, affected_card)->regen_status & KEYWORD_FLYING) &&
		! is_humiliated(player, card, AL_STATIC))
		++event_result;

	return global_enchantment(player, card, event);
}

// moss monster --> vanilla

// mountain stronghold --> undoable

// mountain yeti --> vanilla

int card_mold_demon(int player, int card, event_t event){
/* CARD_ID_MOLD_DEMON	727
Mold Demon	|5|B|B
Creature - Fungus Demon 6/6
When ~ enters the battlefield, sacrifice it unless you sacrifice two |H1Swamps. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_SWAMP);
		this_test.qty = 2;

		if( ! new_can_sacrifice(player, card, player, &this_test) ){
			ai_modifier-=128;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, get_hacked_land_text(player, card, "Select %a card to sacrifice.", SUBTYPE_SWAMP));
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_SWAMP);
		this_test.qty = 2;

		int kill_me = 1;
		if( new_can_sacrifice(player, card, player, &this_test) ){
			if( do_dialog(player, player, card, -1, -1, 
			              get_hacked_land_text(player, card, " Select two %s to sacrifice\n Sacrifice Mold Demon.", SUBTYPE_SWAMP), 0) == 0 ){
				new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &this_test);
				kill_me = 0;
			}
		}
		if( kill_me ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_nebuchadnezzar(int player, int card, event_t event){
/* CARD_ID_NEBUCHADNEZZAR	731
Nebuchadnezzar	|3|U|B
Legendary Creature - Human Wizard 3/3
|X, |T: Choose a card name. Target opponent reveals X cards at random from his or her hand.
Then that player discards all cards with that name revealed this way. Activate this ability only during your turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && hand_count[instance->targets[0].player] > 0 ){
			int t_player = instance->targets[0].player;
			int *deck = deck_ptr[t_player];
			int id = cards_data[deck[internal_rand(count_deck(t_player)-1)]].id;
			if( player == HUMAN ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ANY, "Choose a card name.");
				id = new_card_from_list(player, 3, &test);
			}
			if( player == AI ){
				card_ptr_t* c = cards_ptr[ id ];
				char buffer[100];
				scnprintf(buffer, 100, "Opponent named : %s", c->name);
				do_dialog(HUMAN, player, card, -1, -1, buffer, 0);
			}
			int howmany = MIN(instance->info_slot, hand_count[t_player]);
			int initial_hand[2][50];
			int initial_index = 0;
			int ai_hand[2][50];
			int hand_index = 0;
			int count = 0;
			while( count < active_cards_count[t_player] ){
					if( in_hand(t_player, count) ){
						card_instance_t *crd = get_card_instance(t_player, count);
						initial_hand[0][initial_index] = crd->internal_card_id;
						initial_hand[1][initial_index] = count;
						initial_index++;
					}
					count++;
			}
			while( hand_index < howmany && initial_index > -1 ){
					int rnd = initial_index;
					if( howmany < hand_count[t_player] ){
						rnd = internal_rand(initial_index);
					}
					ai_hand[0][hand_index] = initial_hand[0][rnd];
					ai_hand[1][hand_index] = initial_hand[1][rnd];
					int k;
					for(k=rnd; k<initial_index; k++){
						initial_hand[0][k] = initial_hand[0][k+1];
						initial_hand[1][k] = initial_hand[1][k+1];
					}
					hand_index++;
					initial_index--;
			}
			show_deck( HUMAN, ai_hand[0], hand_index, "Target revealed these cards", 0, gs_done_008b40e0 );
			int i;
			for(i=0; i<hand_index; i++){
				if( cards_data[ai_hand[0][i]].id == id ){
					new_discard_card(t_player, ai_hand[1][i], player, 0);
				}
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_IN_YOUR_TURN | GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST_X(-1), 0, &td, "TARGET_PLAYER");
}

int card_nether_void(int player, int card, event_t event){

	/* Nether Void	|3|B
	 * World Enchantment
	 * Whenever a player casts a spell, counter it unless its controller pays |3. */

	sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_SPELL, NULL, player, card);
	if (event == EVENT_SPA){
		if (!has_mana(trigger_cause_controller, COLOR_ANY, 3) || !charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, trigger_cause_controller, COLOR_ANY, 3)){
			real_counter_a_spell(player, card, trigger_cause_controller, trigger_cause);
		}
	}
	return enchant_world(player, card, event);
}

int card_nicol_bolas2(int player, int card, event_t event){
  /* Nicol Bolas |2|U|U|B|B|R|R
   * Legendary Creature — Elder Dragon 7/7
   * Flying
   * At the beginning of your upkeep, sacrifice Nicol Bolas unless you pay {U}{B}{R}.
   * Whenever Nicol Bolas deals damage to an opponent, that player discards his or her hand. */

	check_legend_rule(player, card, event);

	basic_upkeep(player, card, event, MANACOST_UBR(1, 1, 1));

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_OPPONENT) ){
		new_discard_all(1-player, player);
	}

	return 0;
}

int card_north_star(int player, int card, event_t event){
/* CARD_ID_NORTH_STAR	734
North Star	|4
Artifact
|4, |T: For one spell this turn, you may spend mana as though it were mana of any type to pay that spell's mana cost. (Additional costs are still paid normally.)*/

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL) ){
			int result = 0;
			int i;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_hand(player, i) ){
					int can_play_this_spell = can_legally_play_iid(player, get_card_instance(player, i)->internal_card_id);
					if( can_play_this_spell ){
						int mana_needed = get_updated_casting_cost(player, i, -1, EVENT_CAST_SPELL, get_cmc(player, i));
						if( has_mana_for_activated_ability(player, card, MANACOST_X(4+mana_needed)) ){
							if( can_play_this_spell == 99 ){
								return 99;
							}
							else{
								if( is_what(player, i, TYPE_INSTANT) || can_sorcery_be_played(player, event) ){
									result = 1;
								}
							}
						}
					}
				}
			}
			return result;
		}
	}

	if(event == EVENT_ACTIVATE ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a card to play with North Star.");

		int pwns[2][hand_count[player]];
		int pwnsc = 0;
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) ){
				int can_play_this_spell = can_legally_play_iid(player, get_card_instance(player, i)->internal_card_id);
				if( can_play_this_spell && (is_what(player, i, TYPE_INSTANT | TYPE_INTERRUPT) || can_sorcery_be_played(player, event))){
					int mana_needed = get_updated_casting_cost(player, i, -1, EVENT_CAST_SPELL, get_cmc(player, i));
					if( has_mana_for_activated_ability(player, card, MANACOST_X(4+mana_needed)) ){
						pwns[0][pwnsc] = get_card_instance(player, i)->internal_card_id;
						pwns[1][pwnsc] = i;
						pwnsc++;
					}
				}
			}
		}
		int selected = select_card_from_zone(player, player, pwns[0], pwnsc, 0, AI_MAX_VALUE, -1, &test);
		if( selected != -1 ){
			int mana_needed = get_updated_casting_cost(player, pwns[1][selected], -1, EVENT_CAST_SPELL, get_cmc(player, pwns[1][selected]));
			if( charge_mana_for_activated_ability(player, card, MANACOST_X(4+mana_needed)) ){
				tap_card(player, card);
				play_card_in_hand_for_free(player, pwns[1][selected]);
				cant_be_responded_to = 1;	// The spell will be respondable to, but this (fake) activation won't
			}
			else {
				spell_fizzled = 1;
			}
		}
	}

	return 0;
}

static const char* is_damaging_me(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	card_instance_t* dmg = get_card_instance(player, card);
	if ( dmg->internal_card_id == damage_card ){
		if( dmg->damage_target_player == who_chooses && dmg->damage_target_card == -1 ){
			return NULL;
		}
	}

	return "must be damage dealt to you";
}

int card_nova_pentacle(int player, int card, event_t event){
  /* Nova Pentacle |4
   * Artifact
   * {3}, {T}: The next time a source of your choice would deal damage to you this turn, that damage is dealt to target creature of
   * 	an opponent's choice instead. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_EFFECT);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(is_damaging_me);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.preferred_controller = player;
	td1.who_chooses = 1-player;
	td1.allow_cancel = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE){
		int result = generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DAMAGE_PREVENTION, MANACOST_X(3), 0,
												&td, "TARGET_DAMAGE");
		if( result && can_target(&td1) ){
			return 0x63;
		}
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(3)) ){
			if( pick_target(&td, "TARGET_DAMAGE") ){
				if( new_pick_target(&td1, "TARGET_CREATURE", 1, 1) ){
					add_state(player, card, STATE_TAPPED);
					tapped_for_mana_color = -1;
				}
			}
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && validate_target(player, card, &td1, 1) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			target->damage_target_player = instance->targets[1].player;
			target->damage_target_card = instance->targets[1].card;
		}
	}
	return 0;
}

int card_osai_vultures(int player, int card, event_t event)
{
  /* CARD_ID_OSAI_VULTURES	736
   * Osai Vultures	|1|W
   * Creature - Bird 1/1
   * Flying
   * At the beginning of each end step, if a creature died this turn, put a carrion counter on ~.
   * Remove two carrion counters from ~: ~ gets +1/+1 until end of turn. */

  int result;

  if ((0 < creatures_dead_this_turn) &&
      ((event == EVENT_SHOULD_AI_PLAY || (result = eot_trigger(player, card, event), result != 0))))
  {
    add_counters(player, card, player, card, COUNTER_CARRION, 1);
  }
  if ((uint32_t)(event - EVENT_POW_BOOST) < 2)
  {
    result = count_counters(player, card, COUNTER_CARRION);
    return result / 2;
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = count_counters(player, card, COUNTER_CARRION);
    if (1 < result)
    {
      result = generic_shade_merge_pt(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 1, 1);
      return result;
    }
    result = 0;
  }
  else
  {
    result = generic_shade_merge_pt(player, card, event, 0, 0, 0, 0, 0, 0, 0, 1, 1);
    if ((event == EVENT_ACTIVATE) && (spell_fizzled != 1))
    {
      remove_counters(player, card, player, card, COUNTER_CARRION, 2);
      return result;
    }
  }
  return result;
}

int card_palladia_mors(int player, int card, event_t event){
/* CARD_ID_PALLADIA_MORS	737
Palladia-Mors	|2|R|R|G|G|W|W
Legendary Creature - Elder Dragon 7/7
Flying, trample
At the beginning of your upkeep, sacrifice ~ unless you pay |R|G|W. */

	check_legend_rule(player, card, event);

	basic_upkeep(player, card, event, MANACOST_RGW(1, 1, 1));

	return 0;
}

int card_part_water(int player, int card, event_t event){
/* CARD_ID_PART_WATER	738
Part Water	|X|X|U
Sorcery
X target creatures gain |H2islandwalk until end of turn. (They can't be blocked as long as defending player controls |Han Island.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int trgs = 0;
		instance->number_of_targets = 0;
		while( has_mana(player, COLOR_COLORLESS, (trgs+1)*2) && can_target(&td) ){
				if( new_pick_target(&td, "TARGET_CREATURE", trgs, 0) ){
					state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
					trgs++;
				}
				else{
					break;
				}
		}
		if( trgs > 0 ){
			int i;
			for(i=0; i<trgs; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			}
			charge_mana(player, COLOR_COLORLESS, trgs*2);
			if( spell_fizzled != 1 ){
				instance->info_slot = trgs;
			}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->info_slot; i++){
			if( validate_target(player, card, &td, i) ){
				pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card,
										0, 0, get_hacked_walk(player, card, KEYWORD_ISLANDWALK), 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_pavel_maliki(int player, int card, event_t event)
{
  /* Pavel Maliki	|4|B|R
   * Legendary Creature - Human 5/3
   * |B|R: ~ gets +1/+0 until end of turn. */

  check_legend_rule(player, card, event);

  return generic_shade_merge_pt(player, card, event, 0, MANACOST_BR(1,1), 1, 0);
}

int card_pendelhaven(int player, int card, event_t event){
/* CARD_ID_PENDELHAVEN	740
Pendelhaven	""
Legendary Land
|T: Add |G to your mana pool.
|T: Target 1/1 creature gets +1/+2 until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.power_requirement = 1;
	td.toughness_requirement = 1;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE && affect_me(player, card) ){
		int choice = instance->info_slot = instance->number_of_targets = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Pump an 1/1\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		if( choice == 1 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST0) &&
				new_pick_target(&td, "Select target creature with P/T 1.", 0, 1 | GS_LITERAL_PROMPT)
			  ){
				tap_card(player, card);
				instance->info_slot = 1;
			}
		}

		if( choice == 2 ){
			spell_fizzled = 1;
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
			if( valid_target(&td) ){
				pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 2);
			}
		}
	}

	return 0;
}

int card_petra_sphinx(int player, int card, event_t event)
{
  /* Petra Sphinx	|2|W|W|W
   * Creature - Sphinx 3/4
   * |T: Target player names a card, then reveals the top card of his or her library. If that card is the named card, that player puts it into his or her
   * hand. If it isn't, the player puts it into his or her graveyard. */

  // 0x414d30



  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.preferred_controller = player;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  int p = get_card_instance(player, card)->targets[0].player;
	  int chosen, on_top = deck_ptr[p][0];
	  if (!IS_AI(p))
		chosen = choose_a_card_iid("Choose a card name.", ~COLOR_TEST_0, ~TYPE_NONE);
	  else
		{
		  chosen = get_internal_card_id_from_csv_id(CARD_ID_LABORATORY_MANIAC);	// as good a default as any
		  if (on_top != -1)
			{
			  // This has to be deterministic.  Same algorithm as petra_sphinx_helper() at 0x414d30; no doubt it could be improved.
			  target_t counts[501];
			  memset(counts, -1, sizeof(counts));

			  int i, j, highest_count = 1;
			  for (i = 0; deck_ptr[p][i] != -1; ++i)
				for (j = 0; ; ++j)
				  if (counts[j].card == deck_ptr[p][i])
					{
					  ++counts[j].player;
					  highest_count = MAX(highest_count, counts[j].player);
					  break;
					}
				  else if (counts[j].card == -1)
					{
					  counts[j].card = deck_ptr[p][i];
					  counts[j].player = 1;
					  break;
					}

			  // Now pick the topmost card in the deck whose count is at least tied for the highest
			  for (i = 0; i < 501; ++i)
				if (counts[i].player == highest_count)
				  {
					chosen = counts[i].card;
					break;
				  }
			}
		}

	  if (p == AI || (trace_mode & 2))
		look_at_iid(player, card, chosen, "chooses:");

	  if (on_top != -1)
		{
		  reveal_card_iid(player, card, on_top);
		  if (on_top == chosen)
			add_card_to_hand(p, on_top);
		  else
			mill(p, 1);
		}
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(0), 0, &td, "TARGET_PLAYER");
}

int card_pit_scorpion(int player, int card, event_t event){
  /* Pit Scorpion	|2|B
   * Creature - Scorpion 1/1
   * Whenever this creature deals damage to a player, that player gets a poison counter. (A player with ten or more poison counters loses the game.) */

/*
Pit Scorpion English
Creature — Scorpion 1/1, 2B (3)
Whenever Pit Scorpion deals damage to a player, that player gets a poison counter.
	(A player with ten or more poison counters loses the game.)
*/
	int result = whenever_i_deal_damage_to_a_player(player, card, event, 0);
	if( result ){
		poison(0, BYTE0(result));
		poison(1, BYTE1(result));
	}

	return 0;
}

int card_pixie_queen(int player, int card, event_t event){
/* CARD_ID_PIXIE_QUEEN	743
Pixie Queen	|2|G|G
Creature - Faerie 1/1
Flying
|G|G|G, |T: Target creature gains flying until end of turn. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	if( player == AI ){
		td.illegal_abilities |= KEYWORD_FLYING;
	}
	td.preferred_controller = player;


	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
								0, 0, KEYWORD_FLYING, 0);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_G(3), 0, &td, "TARGET_CREATURE");
}

int card_planar_gate(int player, int card, event_t event)
{
  /* Planar Gate	|6
   * Artifact
   * Creature spells you cast cost up to |2 less to cast. */

  if (event == EVENT_MODIFY_COST_GLOBAL
	  && (affected_card_controller == -1 ? event_result : affected_card_controller) == player
	  && is_what(affected_card_controller, affected_card, TYPE_CREATURE) && !is_humiliated(player, card, AL_STATIC))
	COST_COLORLESS -= 2;

  return 0;
}

int card_pradesh_gypsies(int player, int card, event_t event){
/* CARD_ID_PRADESH_GYPSIES	745
Pradesh Gypsies	|2|G
Creature - Human Nomad 1/1
|1|G, |T: Target creature gets -2/-0 until end of turn. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, -2, 0);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XG(1, 1), 0, &td, "TARGET_CREATURE");
}

int card_presence_of_the_master(int player, int card, event_t event){

	/* Presence of the Master	|3|W
	 * Enchantment
	 * Whenever a player casts an enchantment spell, counter it. */

	if( event == EVENT_CAST_SPELL && ! affect_me(player, card) ){
		int p = affected_card_controller;
		int c = affected_card;
		card_data_t* card_d = get_card_data(p, c);
		if( ( card_d->type & TYPE_ENCHANTMENT) && card_d->cc[2] != 9  ){
			real_counter_a_spell(player, card, p, c);
			land_can_be_played &= ~LCBP_SPELL_BEING_PLAYED;
		}
	}
	return global_enchantment(player, card, event);
}

int card_primordial_ooze(int player, int card, event_t event){
/* CARD_ID_PRIMORDIAL_OOZE	747
Primordial Ooze	|R
Creature - Ooze 1/1
~ attacks each turn if able.
At the beginning of your upkeep, put a +1/+1 counter on ~. Then you may pay |X, where X is the number of +1/+1 counters on it.
If you don't, tap ~ and it deals X damage to you. */

	attack_if_able(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counters(player, card, player, card, COUNTER_P1_P1, 1);
		int disable = 1;
		if( has_mana(player, COLOR_ANY, count_1_1_counters(player, card)) &&
			charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, count_1_1_counters(player, card)) )
		{
			disable = 0;
		}
		if( disable ){
			tap_card(player, card);
			damage_player(player, count_1_1_counters(player, card), player, card);
		}
	}

	return 0;
}

int card_princess_lucrezia(int player, int card, event_t event){
/* CARD_ID_PRINCESS_LUCREZIA	748
Princess Lucrezia	|3|U|U|B
Legendary Creature - Human Wizard 5/4
|T: Add |U to your mana pool. */

	check_legend_rule(player, card, event);
	return mana_producing_creature(player, card, event, 0, COLOR_BLUE, 1);
}

int card_psionic_entity(int player, int card, event_t event){
	/* Psionic Entity	|4|U	0x200c4b0
	 * Creature - Illusion 2/2
	 * |T: ~ deals 2 damage to any target and 3 damage to itself. */
	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ANY");
		if( spell_fizzled != 1 ){
			if( instance->targets[0].player == 1-player && instance->targets[0].card == -1 && life[1-player] < 6 ){
				ai_modifier += 24;
			}
			int actual_tou = get_toughness(player, card) - instance->damage_on_card;
			int actual_color = get_color(player, card);
			int has_prot_from_himself = 0;
			int i;
			for(i=1; i<=5; i++){
				int c_test = 1<<i;
				int key_prot = 1<<(10+i);
				if( (actual_color & c_test) && (instance->regen_status & key_prot) ){
					has_prot_from_himself = 1;
					break;
				}
			}
			if( actual_tou <= 3 && ! has_prot_from_himself ){
				ai_modifier -= 48;
			}
			else{
				ai_modifier += 48;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		damage_target0(player, card, 2);
		damage_creature(instance->parent_controller, instance->parent_card, 3, instance->parent_controller, instance->parent_card);
	}

	return 0;
}

int card_psychic_purge(int player, int card, event_t event){
	/* Psychic Purge	|U	0x2009c56
	 * Sorcery
	 * ~ deals 1 damage to any target.
	 * When a spell or ability an opponent controls causes you to discard ~, that player loses 5 life. */
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int puppet_master_legacy(int player, int card, event_t event){

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance(player, card);
		int i;
		for(i=0; i<2; i++){
			int owner = instance->targets[i].player;
			int iid = instance->targets[i].card;
			int found = -1;
			int p;
			for( p = 499; p >= 0; --p ){
				if( get_grave(owner)[p] == iid ){
					found = p;
					break;
				}
			}
			if( found > -1 ){
				if( i == 0 ){
					add_card_to_hand(owner, iid);
					remove_card_from_grave(owner, found);
				}
				if( i == 1 ){
					if( has_mana(owner, COLOR_BLUE, 3) &&
						charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, owner, COLOR_BLUE, 3) )
					{
						add_card_to_hand(owner, iid);
						remove_card_from_grave(owner, found);
					}
				}
			}
		}
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_puppet_master(int player, int card, event_t event){
  /* Puppet Master |U|U|U
   * Enchantment — Aura
   * Enchant creature
   * When enchanted creature dies, return that card to its owner's hand.
   * If that card is returned to its owner's hand this way, you may pay {U}{U}{U}. If you do, return Puppet Master to its owner's hand. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(p, c) && ! is_token(p, c) ){
			if( is_this_dying(p, c) && !check_for_death_trigger_removal(p, c) ){
				int legacy = create_legacy_effect(player, card, &puppet_master_legacy);
				card_instance_t *l1 = get_card_instance(player, legacy);
				l1->targets[0].player = get_owner(p, c);
				l1->targets[0].card = get_original_internal_card_id(p, c);
				l1->targets[1].player = get_owner(player, card);
				l1->targets[1].card = get_original_internal_card_id(player, card);
				l1->targets[11].player = 1;
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier += is_token(instance->targets[0].player, instance->targets[0].card) ? -48 : 48;
		}
		return 0;
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, player);
}

int card_pyrotechnics(int player, int card, event_t event){
	/* Pyrotechnics	|4|R	0x200cda2
	 * Sorcery
	 * ~ deals 4 damage divided as you choose among any number of targets. */
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int i;
		for(i=0; i<4; i++ ){
			if( new_pick_target(&td, "TARGET_ANY", i, 1) ){
				if( instance->targets[i].card != -1 ){
					add_state(instance->targets[i].player, instance->targets[i].card, STATE_TARGETTED);
				}
			}
			else{
				break;
			}
		}
		for(i=0; i<instance->number_of_targets; i++ ){
			remove_state(instance->targets[i].player, instance->targets[i].card, STATE_TARGETTED);
		}

		if( instance->number_of_targets < 4 ){
			spell_fizzled = 1;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		divide_damage(player, card, &td);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_quagmire(int player, int card, event_t event){
/* CARD_ID_QUAGMIRE	753
Quagmire	|2|B
Enchantment
Creatures with |H2swampwalk can be blocked as though they didn't have |H2swampwalk. */

	landwalk_disabling_card(player, card, event, PB_SWAMPWALK_DISABLED);
	return global_enchantment(player, card, event);
}

static const char* not_already_disabled_by_qtg(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	if( ! check_special_flags2(player, card, SF2_QUARUM_TRENCH_GNOMES) ){
		return NULL;
	}

	return "AI helper function";
}

int card_quarum_trench_gnomes(int player, int card, event_t event){
  /* Quarum Trench Gnomes |3|R
   * Creature — Gnome 1/1
   * {T}: If target Plains is tapped for mana, it produces colorless mana instead of white mana. (This effect lasts indefinitely.) */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = SUBTYPE_PLAINS;
	if( player == AI ){
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		td.extra = target_predicate_extra(not_already_disabled_by_qtg);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
								&td, get_hacked_land_text(player, card, "Select target %s.", SUBTYPE_PLAINS));
		if( spell_fizzled != 1 ){
			ai_modifier += 48;
		}
		return 0;
	}

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		set_special_flags2(instance->targets[0].player, instance->targets[0].card, SF2_QUARUM_TRENCH_GNOMES);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_LAND");
}

int card_rabid_wombat(int player, int card, event_t event){
/* CARD_ID_RABID_WOMBAT	755
Rabid Wombat	|2|G|G
Creature - Wombat 0/1
Vigilance
~ gets +2/+2 for each Aura attached to it. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		vigilance(player, card, event);
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			event_result+=(count_auras_enchanting_me(player, card)*2);
		}
	}
	return 0;
}

// raging bull --> vanilla

int card_radjan_spirit(int player, int card, event_t event){
/* CARD_ID_RADJAN_SPIRIT	756
Radjan Spirit	|3|G
Creature - Spirit 3/2
|T: Target creature loses flying until end of turn. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			ai_modifier += (get_card_instance(instance->targets[0].player, instance->targets[0].card)->regen_status & KEYWORD_FLYING) ?
							48 : -48;
		}
		return 0;
	}

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		negate_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, KEYWORD_FLYING);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_ragnar(int player, int card, event_t event){
/* CARD_ID_RAGNAR	758
Ragnar	|G|W|U
Legendary Creature - Human Cleric 2/2
|G|W|U, |T: Regenerate target creature. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_DESTROYED;
	td.preferred_controller = player;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_REGENERATION, MANACOST_GWU(1, 1, 1), 0,
									&td, "TARGET_CREATURE");
}

int card_ramses_overdark(int player, int card, event_t event){
/* CARD_ID_RAMSES_OVERDARK	760
Ramses Overdark	|2|U|U|B|B
Legendary Creature - Human Assassin 4/3
|T: Destroy target enchanted creature. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ENCHANTED;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target enchanted creature.");
}

static int rapid_fire_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		rampage(p, c, event, 2);
	}

	if (event == EVENT_CLEANUP){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_rapid_fire(int player, int card, event_t event){
  /* Rapid Fire |3|W
   * Instant
   * Cast Rapid Fire only before blockers are declared.
   * Target creature gains first strike until end of turn. If it doesn't have rampage, that creature gains rampage 2 until end of turn.
   * 	(Whenever this creature becomes blocked, it gets +2/+2 until end of turn for each creature blocking it beyond the first.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && current_phase >= PHASE_DECLARE_BLOCKERS ){
		return 0;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier += (current_turn == player ? 24 : -12);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FIRST_STRIKE, 0);
			int legacy = create_targetted_legacy_effect(player, card, &rapid_fire_legacy,
														instance->targets[0].player, instance->targets[0].card);
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

static const char* target_must_be_damage_targeting_this_rasputin(int who_chooses, int player, int card,
																int targeting_player, int targeting_card)
{
	card_instance_t* instance = get_card_instance(player, card);
	if( instance->internal_card_id == damage_card && instance->damage_target_player == targeting_player &&
		instance->damage_target_card == targeting_card )
	{
		return NULL;
	}
	return "must be damaging Rasputin Dreamweaver";
}

// Rebirth --> hardcoded, for now

int card_rasputin_dreamweaver(int player, int card, event_t event){
/* Rasputin Dreamweaver	|4|W|U
 * Legendary Creature - Human Wizard 4/1
 * ~ enters the battlefield with seven dream counters on it.
 * Remove a dream counter from Rasputin: Add |1 to your mana pool.
 * Remove a dream counter from Rasputin: Prevent the next 1 damage that would be dealt to Rasputin this turn.
 * At the beginning of your upkeep, if Rasputin started the turn untapped, put a dream counter on it.
 * ~ can't have more than seven dream counters on it. */

	check_legend_rule(player, card, event);

	enters_the_battlefield_with_counters(player, card, event, COUNTER_DREAM, 7);

	if( event == EVENT_BEGIN_TURN ){
		card_instance_t *instance = get_card_instance( player, card);
		SET_BYTE1(instance->info_slot) = is_tapped(player, card) ? 0 : 1;
	}

	if (event == EVENT_STATIC_EFFECTS){
		while( count_counters(player, card, COUNTER_DREAM) > 7 ){
				remove_counters(player, card, player, card, COUNTER_DREAM, 1);
		}
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		card_instance_t *instance = get_card_instance( player, card);
		if( BYTE1(instance->info_slot) && count_counters(player, card, COUNTER_DREAM) < 7 ){
			add_counters(player, card, player, card, COUNTER_DREAM, 1);
			while( count_counters(player, card, COUNTER_DREAM) > 7 ){
					remove_counters(player, card, player, card, COUNTER_DREAM, 1);
			}
		}
		SET_BYTE1(instance->info_slot) = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	base_target_definition(player, card, &td, 0);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(target_must_be_damage_targeting_this_rasputin);

	card_instance_t *instance = get_card_instance( player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_PREVENT_DAMAGE
	};

	if( event == EVENT_CAN_ACTIVATE && count_counters(player, card, COUNTER_DREAM) > 0 ){
		if( generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			return 99;
		}
		return can_produce_mana(player, card);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		SET_BYTE0(instance->info_slot) = CHOICE_MANA;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			SET_BYTE0(instance->info_slot) = CHOICE_PREVENT_DAMAGE;
		}
		if( BYTE0(instance->info_slot) == CHOICE_MANA ){
			remove_counters(player, card, player, card, COUNTER_DREAM, 1);
			produce_mana(player, COLOR_COLORLESS, 1);
		}
		if( BYTE0(instance->info_slot) == CHOICE_PREVENT_DAMAGE ){
			return generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0,
											GVC_COUNTER(COUNTER_DREAM), &td, "TARGET_DAMAGE");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( BYTE0(instance->info_slot) == CHOICE_PREVENT_DAMAGE && valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
		}
	}

	return 0;
}

int card_recall(int player, int card, event_t event){
	/* Recall	|X|X|U
	 * Sorcery
	 * Discard X cards, then return a card from your graveyard to your hand for each card discarded this way. Exile ~. */

	if(event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		set_special_flags2(player, card, SF2_X_SPELL);
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			instance->info_slot = 0;
			if( player == HUMAN ){
				int result = charge_mana_for_double_x(player, COLOR_COLORLESS);
				set_x_for_x_spells(player, card, event, result);
			}
			else{
				int mana_to_charge = MIN(count_graveyard(player)*2, hand_count[player]*2);
				while( ! has_mana(player, COLOR_ANY, mana_to_charge) ){
						mana_to_charge -= 2;
				}
				if( mana_to_charge == 0 ){
					ai_modifier-=50;
				}
				charge_mana(player, COLOR_ANY, mana_to_charge);
				if( spell_fizzled != 1 ){
					set_x_for_x_spells(player, card, event, mana_to_charge);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card );
		int max = MIN((instance->info_slot/2), hand_count[player]);
		if( max ){
			new_multidiscard(player, max, 0, player);

			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to return to your hand.");
			this_test.qty = max;
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		}

		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_red_mana_battery(int player, int card, event_t event)
{
/* CARD_ID_RED_MANA_BATTERY	765
Red Mana Battery	|4
Artifact
|2, |T: Put a charge counter on ~.
|T, Remove any number of charge counters from ~: Add |R to your mana pool, then add an additional |R to your mana pool for each charge counter removed this way. */

  // 0x423580
  return mana_battery(player, card, event, COLOR_RED);
}

static int reincarnation_legacy(int player, int card, event_t event)
{

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (in_play(affected_card_controller, affected_card))
    {
      if (instance->damage_target_player > -1)
      {
        int p = instance->damage_target_player;
        int c = instance->damage_target_card;
        if (affect_me(p, c) && is_this_dying(p, c) && !check_for_death_trigger_removal(p, c))
        {
          if (instance->targets[11].player < 0)
          {
            instance->targets[11].player = 0;
          }
          SET_BYTE1(instance->targets[11].player) = 1;
          SET_BYTE0(instance->targets[11].player) = get_owner(p, c);
          instance->damage_target_card = instance->damage_target_player = -1;
        }
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result)
  {
    int owner = BYTE0(result);
    if (count_graveyard_by_type(owner, TYPE_CREATURE))
    {
      test_definition_t test;
      new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
      new_global_tutor(owner, owner, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_CMC, &test);
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_reincarnation(int player, int card, event_t event){
/* CARD_ID_REINCARNATION	766
Reincarnation	|1|G|G
Instant
Choose target creature. When that creature dies this turn, return a creature card from its owner's graveyard to the battlefield
under the control of that creature's owner. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.preferred_controller = player;
	td2.required_state = TARGET_STATE_DYING;

	if( event == EVENT_CAN_CAST ){
		if( player == HUMAN ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
		else{
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_REGENERATION, &td2, "TARGET_CREATURE", 1, NULL);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player == HUMAN ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
		else{
			generic_spell(player, card, event, GS_CAN_TARGET | GS_REGENERATION, &td2, "TARGET_CREATURE", 1, NULL);
			if( spell_fizzled != 1 && count_graveyard_by_type(player, TYPE_CREATURE) > 0 ){
				ai_modifier += (get_owner(instance->targets[0].player, instance->targets[0].card) == player ? 48 : -48);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &reincarnation_legacy, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_relic_barrier(int player, int card, event_t event){
/* CARD_ID_RELIC_BARRIER	767
Relic Barrier	|2
Artifact
|T: Tap target artifact. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		tap_card(instance->targets[0].player, instance->targets[0].card);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ARTIFACT");
}

int card_relic_bind2(int player, int card, event_t event){
	/* Relic Bind	|2|U	0x200575f
	 * Enchantment - Aura
	 * Enchant artifact an opponent controls
	 * Whenever enchanted artifact becomes tapped, choose one -
	 * * ~ deals 1 damage to target player or planeswalker.
	 * * Target player gains 1 life. */
	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( event == EVENT_TAP_CARD && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			instance->number_of_targets = 0;

			target_definition_t td2;
			default_target_definition(player, card, &td2, TARGET_TYPE_PLANESWALKER);
			td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			td2.allow_cancel = 0;

			target_definition_t td3;
			default_target_definition(player, card, &td3, 0);
			td3.zone = TARGET_ZONE_PLAYERS;
			td3.preferred_controller = player;
			td3.allow_cancel = 0;

			int choice = 0;
			if( can_target(&td2) ){
				if( can_target(&td3) ){
					int ai_choice = 0;
					if( life[player] < 6 && would_validate_arbitrary_target(&td3, player, -1) ){
						ai_choice = 1;
					}
					choice = do_dialog(player, player, card, player, card, " Deal 1 damage\n Target gain 1 life", ai_choice);
				}
			}
			else{
				choice = 1;
			}
			if( choice == 0 && pick_target(&td2, "TARGET_PLAYER_OR_PLANESWALKER") ){
				damage_target0(player, card, 1);
			}
			if( choice == 1 && pick_target(&td3, "TARGET_PLAYER") ){
				gain_life(instance->targets[0].player, 1);
			}
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.preferred_controller = 1-player;
	td.allowed_controller = 1-player;

	return targeted_aura(player, card, event, &td, "TARGET_ARTIFACT");
}

static int good_enchantment_for_remove_enchantments(int player, int t_player, int t_card){
	if( player == t_player && ! is_stolen(t_player, t_card) ){
		if( ! has_subtype(t_player, t_card, SUBTYPE_AURA) ){
			return 1;
		}
		else{
			card_instance_t* instance = get_card_instance(t_player, t_card);
			if( instance->damage_target_player == player ){
				return 1;
			}
			else{
				if( is_attacking(instance->damage_target_player, instance->damage_target_card) ){
					return 1;
				}
			}
		}
	}
	return 0;
}

static int bad_enchantment_for_remove_enchantments(int player, int t_player, int t_card){
	if( player != t_player && has_subtype(t_player, t_card, SUBTYPE_AURA) ){
		card_instance_t* instance = get_card_instance(t_player, t_card);
		if( instance->damage_target_player == player ){
			return 1;
		}
		else{
			if( is_attacking(instance->damage_target_player, instance->damage_target_card) ){
				return 1;
			}
		}
	}
	return 0;
}

int card_remove_enchantments(int player, int card, event_t event){
/* CARD_ID_REMOVE_ENCHANTMENTS	769
Remove Enchantments	|W
Instant
Return to your hand all enchantments you both own and control, all Auras you own attached to permanents you control,
and all Auras you own attached to attacking creatures your opponents control. Then destroy all other enchantments you control,
all other Auras attached to permanents you control, and all other Auras attached to attacking creatures your opponents control. */

	if (event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			int count = active_cards_count[player]-1;
			while( count > -1 ){
					if( in_play(i, count) && is_what(i, count, TYPE_ENCHANTMENT) ){
						if( good_enchantment_for_remove_enchantments(player, i, count) ){
							bounce_permanent(i, count);
						}
					}
					count--;
			}
		}
		for(i=0; i<2; i++){
			int count = active_cards_count[player]-1;
			while( count > -1 ){
					if( in_play(i, count) && is_what(i, count, TYPE_ENCHANTMENT) ){
						if( i == player || (i!= player && bad_enchantment_for_remove_enchantments(player, i, count)) ){
							kill_card(i, count, KILL_DESTROY);
						}
					}
					count--;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_remove_soul(int player, int card, event_t event){
/* CARD_ID_REMOVE_SOUL	770
Remove Soul	|1|U
Instant
Counter target creature spell. */

	// Used externally
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_CREATURE);

	return counterspell(player, card, event, &td, 0);
}

int card_reset(int player, int card, event_t event)
{
/* CARD_ID_RESET	771
Reset	|U|U
Instant
Cast ~ only during an opponent's turn after his or her upkeep step.
Untap all lands you control. */

  // 0x42d0a0

  // Cast ~ only during an opponent's turn after his or her upkeep step.
  if (event == EVENT_CAN_CAST && player != current_turn && current_phase >= PHASE_DRAW && current_phase <= PHASE_CLEANUP)
	return basic_spell(player, card, event);

  // Untap all lands you control.
  if (event == EVENT_RESOLVE_SPELL)
	{
	  int c;
	  for (c = 0; c < active_cards_count[player]; ++c)
		if (in_play(player, c) && is_what(player, c, TYPE_LAND))
		  untap_card(player, c);

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_revelation(int player, int card, event_t event)
{
/* CARD_ID_REVELATION	772
Revelation	|G
World Enchantment
Players play with their hands revealed. */

  enchant_world(player, card, event);

  if (event == EVENT_STATIC_EFFECTS)
	{
	  player_bits[0] |= PB_HAND_REVEALED;
	  player_bits[1] |= PB_HAND_REVEALED;
	}

  return global_enchantment(player, card, event);
}

// righteous avengers --> vanilla

int card_reverberation(int player, int card, event_t event){
/* CARD_ID_REVERBERATION	773
Reverberation	|2|U|U
Instant
All damage that would be dealt this turn by target sorcery spell is dealt to that spell's controller instead. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}
	// Approximation
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ANY);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(damage_from_sorcery_source);
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			target->damage_target_player = target->damage_source_player;
			target->damage_target_card = -1;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, "TARGET_DAMAGE", 1, NULL);
}

static const char* is_instant_or_aura_targetting_one_of_my_permanents(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){

	if( is_what(player, card, TYPE_INSTANT | TYPE_INTERRUPT) ||
		( is_what(player, card, TYPE_ENCHANTMENT) && has_subtype(player, card, SUBTYPE_AURA)) )
	{
		card_instance_t *instance = get_card_instance(player, card);
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( instance->targets[i].player == who_chooses && instance->targets[i].card != -1 ){
				return NULL;
			}
		}
	}

	return "must be an instant or an aura targetting a permanent you control";
}

int card_ring_of_immortals(int player, int card, event_t event){
	/* Ring of Immortals	|5
	 * Artifact
	 * |3, |T: Counter target instant or Aura spell that targets a permanent you control. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(is_instant_or_aura_targetting_one_of_my_permanents);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( counterspell_validate(player, card, &td, 0) ){
			real_counter_a_spell(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK | GAA_UNTAPPED, MANACOST_X(3), 0, &td, "TARGET_SPELL");
}

int card_riven_turnbull(int player, int card, event_t event){
/* CARD_ID_RIVEN_TURNBULL	776
Riven Turnbull	|5|U|B
Legendary Creature - Human Advisor 5/7
|T: Add |B to your mana pool. */

	check_legend_rule(player, card, event);
	return mana_producing_creature(player, card, event, 0, COLOR_BLACK, 1);
}

int card_rohgahh_of_kher_keep(int player, int card, event_t event){
/* CARD_ID_ROHGAHH_OF_KHER_KEEP	777
Rohgahh of Kher Keep	|2|B|B|R|R
Legendary Creature - Kobold 5/5
At the beginning of your upkeep, you may pay |R|R|R. If you don't, tap ~ and all creatures named Kobolds of Kher Keep, then an opponent gains control of them.
Creatures you control named Kobolds of Kher Keep get +2/+2. */

	check_legend_rule(player, card, event);

	if( current_turn == player && upkeep_trigger(player, card, event) ){
		int count = count_upkeeps(player);
		int sc = count;
		while( count > 0 ){
				if( has_mana(player, COLOR_RED, 3) ){
					charge_mana(player, COLOR_RED, 3);
					if( spell_fizzled != 1 ){
						sc--;
					}
					else{
						break;
					}
				}
				else{
					break;
				}
				count--;
		}
		if( sc > 0 ){
			tap_card(player, card);
			int i = active_cards_count[player]-1;
			while( i > -1 ){
					if( in_play(player, i) && get_id(player, i) == CARD_ID_KOBOLDS_OF_KHER_KEEP ){
						tap_card(player, i);
						give_control_of_self(player, i);
					}
					i--;
			}
			give_control_of_self(player, card);
		}
	}

	if( event == EVENT_POWER || event == EVENT_TOUGHNESS ){
		if( affected_card_controller == player && get_id(affected_card_controller, affected_card) == CARD_ID_KOBOLDS_OF_KHER_KEEP ){
			event_result += 2;
		}
	}

	return 0;
}

int card_rubinia_soulsinger(int player, int card, event_t event){

	/* Rubinia Soulsinger	|2|G|W|U
	 * Legendary Creature - Faerie 2/3
	 * You may choose not to untap ~ during your untap step.
	 * |T: Gain control of target creature for as long as you control ~ and ~ remains tapped. */

	check_legend_rule(player, card, event);

	choose_to_untap(player, card, event);

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	if (IS_AI(player)){
		td.allowed_controller = 1-player;
	}

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		card_instance_t* instance = get_card_instance(player, card);
		gain_control_until_source_is_in_play_and_tapped(player, card, instance->targets[0].player, instance->targets[0].card,
														GCUS_SOURCE_IN_PLAY | GCUS_CONTROLLED | GCUS_TAPPED);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(0), 0, &td, "TARGET_CREATURE");
}

int card_rust2(int player, int card, event_t event){
/* CARD_ID_RUST	778
Rust	|G
Instant
Counter target activated ability from an artifact source. (Mana abilities can't be targeted.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}
    target_definition_t td_default;
	counter_activated_target_definition(player, card, &td_default, TYPE_ARTIFACT);
	if (event == EVENT_CAN_CAST){
		return can_counter_activated_ability(player, card, event, &td_default);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		get_card_instance(player, card)->number_of_targets = 0;
		return cast_counter_activated_ability(player, card, 0);
	}

	if (event == EVENT_RESOLVE_SPELL){
		if (resolve_counter_activated_ability(player, card, NULL, 0))
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

// seafarer's quay --> impossible
/* CARD_ID_SEAFARERS_QUAY	780
Seafarer's Quay	""
Land
|SBlue legendary creatures you control have "bands with other legendary creatures." (Any legendary creatures can attack in a band as long as
at least one has "bands with other legendary creatures." Bands are blocked as a group. If at least two legendary creatures you control,
one of which has "bands with other legendary creatures," are blocking or being blocked by the same creature, you divide that creature's combat damage,
not its controller, among any of the creatures it's being blocked by or is blocking.) */

int card_sea_kings_blessing(int player, int card, event_t event){
/* CARD_ID_SEA_KINGS_BLESSING	779
Sea Kings' Blessing	|U
Instant
Any number of target creatures become |Sblue until end of turn. */

	return song(player, card, event, COLOR_TEST_BLUE);
}

// segovian leviathan --> vanilla

int card_seeker2(int player, int card, event_t event){
  /* Seeker |2|W|W
   * Enchantment — Aura
   * Enchant creature
   * Enchanted creature can't be blocked except by artifact creatures and/or white creatures. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( event == EVENT_BLOCK_LEGALITY && attacking_card_controller == instance->damage_target_player &&
			attacking_card == instance->damage_target_card )
		{
			if( ! is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) &&
				!(get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_WHITE)) )
			{
				event_result = 1;
			}
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, player);
}

static const char* is_blocking_s(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  if (is_blocking(card, targeting_card))
	return NULL;
  else
	return "must be blocking Sentinel";
}

static const char* s_is_blocking_him(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  if (is_blocking(targeting_card, card))
	return NULL;
  else
	return "must be blocked by Sentinel";
}

// poison snake --> pit scorpion

int card_sentinel(int player, int card, event_t event){
  /* Sentinel |4
   * Artifact Creature — Shapeshifter 1/1
   * {0}: Change Sentinel's base toughness to 1 plus the power of target creature blocking or blocked by Sentinel.
   * 	(This effect lasts indefinitely.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller =  1-player;
	td.preferred_controller = 1-player;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(is_blocking_s);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller =  1-player;
	td1.preferred_controller = 1-player;
	td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td1.extra = target_predicate_extra(s_is_blocking_him);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( current_phase == PHASE_AFTER_BLOCKING && generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			if( current_turn == player ){
				return can_target(&td);
			}
			else{
				return can_target(&td1);
			}
		}
	}
	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			if( current_turn == player ){
				new_pick_target(&td, "Select target creature blocking Sentinel", 0, 1 | GS_LITERAL_PROMPT);
			}
			else{
				new_pick_target(&td1, "Select target creature blocked by Sentinel", 0, 1 | GS_LITERAL_PROMPT);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if (in_play(instance->parent_controller, instance->parent_card) &&
			((current_turn == player && valid_target(&td)) || (current_turn != player && valid_target(&td1)))){
			real_set_pt(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, -1, get_power(instance->targets[0].player, instance->targets[0].card)+1, 8);
		}
	}

	return 0;
}

int card_serpent_generator(int player, int card, event_t event){
	/* Serpent Generator	|6
	 * Artifact
	 * |4, |T: Put a 1/1 colorless Snake artifact creature token onto the battlefield.
		It has "Whenever this creature deals damage to a player, that player gets a poison counter." */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_POISON_SNAKE);
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL);
}

int card_shelkin_brownie(int player, int card, event_t event){
/* CARD_ID_SHELKIN_BROWNIE	785
Shelkin Brownie	|1|G
Creature - Ouphe 1/1
|T: Target creature loses all "bands with other" abilities until end of turn. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	if( player == AI ){
		td.required_abilities = KEYWORD_BANDING;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		negate_ability_until_eot(instance->parent_controller, instance->parent_card,
								instance->targets[0].player, instance->targets[0].card, KEYWORD_BANDING);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_shield_wall(int player, int card, event_t event){
/* CARD_ID_SHIELD_WALL	786
Shield Wall	|1|W
Instant
Creatures you control get +0/+2 until end of turn. */

	if( event == EVENT_RESOLVE_SPELL){
		pump_subtype_until_eot(player, card, player, -1, 0, 2, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

static int shimian_night_stalker_effect(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		int sns_player = instance->targets[0].player;
		int sns_card = instance->targets[0].card;

		int aff_player = instance->targets[1].player;

		card_instance_t *damage = damage_being_prevented(event);

		if( damage ){
			if( damage->damage_source_player == p && damage->damage_source_card == c &&
				damage->damage_target_player == aff_player && damage->damage_target_card == -1 &&
				! damage_is_to_planeswalker(damage) )
			{
				damage->damage_target_player = sns_player;
				damage->damage_target_card = sns_card;
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

static int ai_modifier_for_shimian_night_stalker(int player, int card){
	card_instance_t *instance = get_card_instance(player, card);
	if( current_phase == PHASE_AFTER_BLOCKING && is_unblocked(instance->targets[0].player, instance->targets[0].card) ){
		if( life[player] - get_attack_power(instance->targets[0].player, instance->targets[0].card) < 6 ){
			return 96;
		}
	}
	if( get_power(instance->targets[0].player, instance->targets[0].card) < get_toughness(player, card) ){
		return 48;
	}
	if( get_power(instance->targets[0].player, instance->targets[0].card) >= get_toughness(player, card) ){
		return -24;
	}
	return 0;
}

int card_shimian_night_stalker(int player, int card, event_t event){
  /* Shimian Night Stalker |3|B|B
   * Creature — Nightstalker 4/4
   * {B}, {T}: All damage that would be dealt to you this turn by target attacking creature is dealt to Shimian Night Stalker instead. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1), 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			ai_modifier += ai_modifier_for_shimian_night_stalker(player, card);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			int leg = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &shimian_night_stalker_effect,
													instance->targets[0].player, instance->targets[0].card);
			card_instance_t *hag = get_card_instance(player, leg);
			hag->targets[0].player = instance->parent_controller;
			hag->targets[0].card = instance->parent_card;
			hag->targets[1].player = player;
			hag->targets[1].card = -1;
			hag->number_of_targets = 2;
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1), 0, &td, "TARGET_CREATURE");
}

static int silhouette_effect(int player, int card, event_t event){

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == instance->targets[0].card && damage->damage_target_player == instance->targets[0].player &&
				damage->info_slot > 0 && !(damage->token_status & (STATUS_COMBAT_DAMAGE|STATUS_FIRST_STRIKE_DAMAGE))
			  ){
				// targets[5]-targets[9] of the damage_card are a direct copy of the damage source first 5 targets
				int k;
				for(k=5; k<10; k++){
					if( damage->targets[k].player == instance->targets[0].player && damage->targets[k].card == instance->targets[0].card ){
						damage->info_slot = 0;
						break;
					}
				}
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_silhouette(int player, int card, event_t event){
/* CARD_ID_SILHOUETTE	788
Silhouette	|1|U
Instant
Choose target creature. If a spell or ability that targets that creature would cause a source to deal damage to that creature this turn, prevent that damage.*/

	// Approximation
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
			ai_modifier += 48;
		}
	}

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &silhouette_effect, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_sol_kanar_the_swamp_king(int player, int card, event_t event){
/* CARD_ID_SOLKANAR_THE_SWAMP_KING	791
Sol'kanar the Swamp King	|2|U|B|R
Legendary Creature - Demon 5/5
|H2Swampwalk (This creature can't be blocked as long as defending player controls |Ha Swamp.)
Whenever a player casts a |Sblack spell, you gain 1 life. */

	check_legend_rule(player, card, event);
	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){
		gain_life(player, 1);
	}
	return 0;
}

int card_spectral_cloak(int player, int card, event_t event){
/* CARD_ID_SPECTRAL_CLOAK	792
Spectral Cloak	|U|U
Enchantment - Aura
Enchant creature
Enchanted creature has shroud as long as it's untapped. (It can't be the target of spells or abilities.) */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_ABILITIES && affect_me(p, c) && ! is_tapped(p, c) ){
			event_result |= KEYWORD_SHROUD;
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, player);
}

int card_spinal_villain(int player, int card, event_t event){
/* CARD_ID_SPINAL_VILLAIN	793
Spinal Villain	|2|R
Creature - Beast 1/2
|T: Destroy target |Sblue creature. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, get_sleighted_color_text(player, card, "Select target %s creature.", COLOR_BLUE));
}

int card_spirit_link(int player, int card, event_t event){
  /* CARD_ID_SPIRIT_LINK	794
   * Spirit Link	|W
   * Enchantment - Aura
   * Enchant creature (Target a creature as you cast this. This card enters the battlefield attached to that creature.)
   * Whenever enchanted creature deals damage, you gain that much life. */

 //also code for Vampiric Link


	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		spirit_link_effect(instance->damage_target_player, instance->damage_target_card, event, player);
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	return vanilla_aura(player, card, event, player);
}

int card_spirit_shackle(int player, int card, event_t event)
{
  /* Spirit Shackle	|B|B
   * Enchantment - Aura
   * Enchant creature
   * Whenever enchanted creature becomes tapped, put a -0/-2 counter on it. */

	if (event == EVENT_TAP_CARD)
	{
	  card_instance_t* instance = in_play(player, card);
	  if (instance && affect_me(instance->damage_target_player, instance->damage_target_card))
			add_counters(player, card, instance->damage_target_player, instance->damage_target_card, COUNTER_M0_M2, 1);
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	return disabling_aura(player, card, event);
}

int card_spiritual_sanctury(int player, int card, event_t event){
	/* Spiritual Sanctuary	|2|W|W
	 * Enchantment
	 * At the beginning of each player's upkeep, if that player controls |Ha Plains, he or she gains 1 life. */

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		int cp = check_battlefield_for_subtype(current_turn, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_PLAINS));
		upkeep_trigger_ability_mode(player, card, event, ANYBODY, cp ? RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		gain_life(current_turn, 1);
	}

	return global_enchantment(player, card, event);
}

int card_stangg(int player, int card, event_t event)
{
  /* Stangg	|4|R|G
   * Legendary Creature - Human Warrior 3/4
   * When ~ enters the battlefield, put a legendary 3/4 |Sred and |Sgreen Human Warrior creature token named ~ Twin onto the battlefield.
   * When ~ leaves the battlefield, exile that token. When that token leaves the battlefield, sacrifice ~. */

  test_definition_t test;
  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    generate_token_by_id(player, card, CARD_ID_STANGG_TWIN);
    leaves_play(player, card, EVENT_ETB, 1, 0);
    return 0;
  }
  leaves_play(player, card, event, 1, 0);
  if (event != EVENT_LEAVES_PLAY_ABILITY)
  {
    return 0;
  }
  default_test_definition(&test, 0x47);
  test.id = 0x37d;
  new_manipulate_all(player, card, 2, &test, ACT_KILL_REMOVE);
  return 0;
}

int card_stangg_twin(int player, int card, event_t event){
  /* Stangg Twin	""
   * Token Legendary Creature - Human Warrior 3/4 */

	check_legend_rule(player, card, event);

	leaves_play(player, card, event, 1, 0);
	if (event == EVENT_LEAVES_PLAY_ABILITY){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.id = CARD_ID_STANGG;
		new_manipulate_all(player, card, ANYBODY, &test, KILL_EXILE);
	}

	return 0;
}

int card_storm_seeker(int player, int card, event_t event)
{
  /* Storm Seeker	|3|G
   * Instant
   * ~ deals damage equal to the number of cards in target player's hand to that player. */

  /* Sudden Impact	|3|R
   * Instant
   * ~ deals damage equal to the number of cards in target player's hand to that player. */
  // Used externally
  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		damage_target0(player, card, hand_count[get_card_instance(player, card)->targets[0].player]);

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_storm_world(int player, int card, event_t event)
{
  /* Storm World	|R
   * World Enchantment
   * At the beginning of each player's upkeep, ~ deals X damage to that player, where X is 4 minus the number of cards in his or her hand. */

  // 0x4160a0



  upkeep_trigger_ability(player, card, event, ANYBODY);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  int damage = 4 - hand_count[current_turn];
	  if (damage > 0)
		damage_player(current_turn, damage, player, card);
	}

  return enchant_world(player, card, event);
}

int card_subdue(int player, int card, event_t event){
/* CARD_ID_SUBDUE	800
Subdue	|G
Instant
Prevent all combat damage that would be dealt by target creature this turn. That creature gets +0/+X until end of turn, where X is its converted mana cost.*/

	if( event == EVENT_CHECK_PUMP ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->targets[0].player > -1 &&  instance->targets[0].card > -1 ){
			pumpable_toughness[player] += get_cmc(instance->targets[0].player, instance->targets[0].card);
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	/* Prevent all combat damage that would be dealt by target creature this turn. That creature gets +0/+X until end of turn, where X is its converted mana
	 * cost. */
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &kry_shield_effect, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_sunastian_falconer(int player, int card, event_t event){
/* CARD_ID_SUNASTIAN_FALCONER	801
Sunastian Falconer	|3|R|G
Legendary Creature - Human Shaman 4/4
|T: Add |C|C to your mana pool. */

	//0x4069b0
	check_legend_rule(player, card, event);
	return mana_producing_creature(player, card, event, 0, COLOR_COLORLESS, 2);
}

int card_sword_of_the_ages(int player, int card, event_t event){
	/* Sword of the Ages	|6	0x20029dd
	 * Artifact
	 * ~ enters the battlefield tapped.
	 * |T, Sacrifice ~ and any number of creatures you control: ~ deals X damage to any target, where X is the total power of the creatures sacrificed this way, then exile ~ and those creature cards. */
	comes_into_play_tapped(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0,
										&td, "TARGET_ANY");
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = instance->info_slot = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) && pick_target(&td, "TARGET_ANY") ){
			int sacced[9];
			int sc = 0;
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			while( new_can_sacrifice_as_cost(player, card, &test) && sc < 9 ){
					int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
					if( ! sac ){
						break;
					}
					sacced[sc] = BYTE3(sac);
					sc++;
			}
			add_state(player, card, STATE_TAPPED);
			tapped_for_mana_color = -1;
			int i;
			SET_BYTE1(instance->info_slot) = 1;
			for(i=0; i<sc; i++){
				state_untargettable(player, sacced[i], 0);
				SET_BYTE0(instance->info_slot) += get_power(player, sacced[i]);
				int pos = BYTE1(instance->info_slot);
				instance->targets[pos].player = get_owner(player, sacced[i]);
				instance->targets[pos].card = get_original_internal_card_id(player, sacced[i]);
				SET_BYTE1(instance->info_slot)++;
				kill_card(player, sacced[i], KILL_SACRIFICE);
			}
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = BYTE0(instance->info_slot);
			damage_target0(player, card, amount);
			int i;
			for(i=1; i<BYTE1(instance->info_slot); i++){
				int owner = instance->targets[i].player;
				int iid = instance->targets[i].card;
				int cg = count_graveyard(owner)-1;
				while( cg > -1 ){
						if( get_grave(owner)[cg] == iid ){
							rfg_card_from_grave(owner, iid);
							break;
						}
						cg--;
				}
			}
		}
	}

	return 0;
}

int card_sylvan_library(int player, int card, event_t event){
/* CARD_ID_SYLVAN_LIBRARY	803
Sylvan Library	|1|G
Enchantment
At the beginning of your draw step, you may draw two additional cards. If you do, choose two cards in your hand drawn this turn.
For each of those cards, pay 4 life or put the card on top of your library. */

	if( ! is_humiliated(player, card, AL_TRIG) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( current_turn == player && event == EVENT_DRAW_PHASE ){
			if( do_dialog(player, player, card, -1, -1, " Activate Sylvan Library\n Pass", count_deck(player) > 10 ? 0 : 1) == 0 ){
				get_card_instance(player, card)->info_slot = 1;
				event_result+=2;
			}
		}
		if (event == EVENT_PHASE_CHANGED && current_phase == PHASE_MAIN1 && !(instance->info_slot & 2) && in_play(player, card) && (instance->info_slot & 1) ){
			if( current_turn == player ){
				instance->info_slot |= 2;	// prevent being asked multiple times
				int dtt[2][100];
				int dtt_count = 0;
				int i;
				for(i=0; i<active_cards_count[player]; i++){
					if( in_hand(player, i) && check_state(player, i, STATE_DRAWN_THIS_TURN) ){
						dtt[0][dtt_count] = get_card_instance(player, i)->internal_card_id;
						dtt[1][dtt_count] = i;
						dtt_count++;
					}
				}
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, "Select a card to put on top of deck");
				int pb = 0;
				while( pb < 2 && dtt_count > 0){
						int selected = select_card_from_zone(player, player, dtt[0], dtt_count, 1-can_pay_life(player, 4), AI_MIN_VALUE, -1, &this_test);
						if( selected != -1 ){
							put_on_top_of_deck(player, dtt[1][selected]);
							int k;
							for(k=selected; k<dtt_count; k++){
								dtt[0][k] = dtt[0][k+1];
								dtt[1][k] = dtt[1][k+1];
							}
							dtt_count--;
						}
						else{
							lose_life(player, 4);
						}
						pb++;
				}
			}
		}

		if(event == EVENT_CLEANUP ){
			instance->info_slot = 0;
		}
	}

	return global_enchantment(player, card, event);
}

int card_sylvan_paradise(int player, int card, event_t event){
/* CARD_ID_SYLVAN_PARADISE	804
Sylvan Paradise	|G
Instant
Any number of target creatures become |Sgreen until end of turn. */

	return song(player, card, event, COLOR_TEST_GREEN);
}

int syphon_soul_legacy(int player, int card, event_t event){

	card_instance_t* damage;
	if( event == EVENT_DEAL_DAMAGE && (damage = get_card_instance(affected_card_controller, affected_card))
		&& damage->internal_card_id == damage_card)
	{
		if( damage->display_pic_csv_id == CARD_ID_SYPHON_SOUL ){
			card_instance_t* instance = get_card_instance(player, card);
			if( instance->targets[1].player < 0 ){
				instance->targets[1].player = 0;
			}
			SET_BYTE1(instance->targets[1].player) = 1;
			SET_BYTE0(instance->targets[1].player) += damage->info_slot;
		}
	}

	if( get_card_instance(player, card)->targets[1].player > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
		reason_for_trigger_controller == player )
	{
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				card_instance_t *instance = get_card_instance(player, card);
				int amount = BYTE0(instance->targets[1].player);
				gain_life(player, amount);
				kill_card(player, card, KILL_EXILE);
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_syphon_soul(int player, int card, event_t event){
/* CARD_ID_SYPHON_SOUL	805
Syphon Soul	|2|B
Sorcery
~ deals 2 damage to each other player. You gain life equal to the damage dealt this way. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &syphon_soul_legacy);
		damage_player(1-player, 2, player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int takklemaggot_legacy(int player, int card, event_t event){

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance(player, card);
		int tak_player = instance->targets[0].player;
		int tak_card = instance->targets[0].card;
		int crit_player = instance->targets[1].player;
		int count = count_graveyard(tak_player)-1;
		while( count > -1 ){
				if( get_grave(tak_player)[count] == tak_card ){
					int card_added = add_card_to_hand(tak_player, get_grave(tak_player)[count]);
					get_card_instance(tak_player, card_added)->targets[1].player = crit_player;
					get_card_instance(tak_player, card_added)->targets[1].card = -1;
					remove_card_from_grave(tak_player, count);
					put_into_play(tak_player, card_added);
					break;
				}
				count--;
		}
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_takklemaggot(int player, int card, event_t event){
/* Takklemaggot	|2|B|B
 * Enchantment - Aura
 * Enchant creature
 * At the beginning of the upkeep of enchanted creature's controller, put a -0/-1 counter on that creature.
 * When enchanted creature dies, that creature's controller chooses a creature that ~ could enchant.
 * If he or she does, return ~ to the battlefield under your control attached to that creature.
 * If he or she doesn't, return ~ to the battlefield under your control as a non-Aura enchantment.
 *		It loses "enchant creature" and gains "At the beginning of that player's upkeep, ~ deals 1 damage to him or her." */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			if( c != -1 ){
				add_counters(player, card, p, c, COUNTER_M0_M1, 1);
			}
			else{
				damage_player(p, 1, player, card);
			}
		}
		upkeep_trigger_ability(player, card, event, p);

		if( c > -1 ){
			if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(p, c) ){
				if (is_this_dying(p, c)){
					if (check_for_death_trigger_removal(p, c)){
						kill_card(player, card, KILL_STATE_BASED_ACTION);
						return 0;
					}
					int legacy = create_legacy_effect(player, card, &takklemaggot_legacy);
					card_instance_t *leg = get_card_instance(player, legacy);
					leg->targets[0].player = get_owner(player, card);
					leg->targets[0].card = get_original_internal_card_id(player, card);
					leg->targets[1].player = p;
					leg->targets[11].player = 1;
				}
			}
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	if( event == EVENT_CAN_CAST ){
		return disabling_aura(player, card, event);
	}

	if( event == EVENT_CAST_SPELL  && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->targets[1].player == -1 ){
			return disabling_aura(player, card, event);
		}
		else{
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.preferred_controller = 1-instance->targets[1].player;
			td.who_chooses = instance->targets[1].player;
			td.illegal_abilities = 0;
			td.allow_cancel = 0;
			if( can_target(&td) ){
				pick_target(&td, "TARGET_CREATURE");
			}
			else{
				instance->targets[0] = instance->targets[1];
				instance->number_of_targets = 1;
			}
			return 0;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->targets[0].card != -1 ){
			return disabling_aura(player, card, event);
		}
		else{
			instance->damage_target_player = instance->targets[0].player;
			instance->damage_target_card = -1;
		}
	}

	return disabling_aura(player, card, event);
}

static int effect_telekinesis(int player, int card, event_t event){

	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP ){
		card_instance_t *instance = get_card_instance( player, card );
		if(affect_me(instance->targets[0].player, instance->targets[0].card) ){
			if( instance->targets[2].card > 0 ){
				does_not_untap(instance->targets[0].player, instance->targets[0].card, event);
				instance->targets[2].card--;
			}
			else{
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( event == EVENT_DEAL_DAMAGE && get_card_instance( player, card )->targets[2].card == 3){
		card_instance_t *instance = get_card_instance( player, card );
		card_instance_t *damage = get_card_instance( affected_card_controller, affected_card);
		if( damage->damage_source_player == instance->targets[0].player &&
			damage->damage_source_card == instance->targets[0].card && damage->info_slot > 0 ){
			prevent_damage(affected_card_controller, affected_card, -1);
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[2].card--;
	}

	return 0;
}

int card_telekinesis(int player, int card, event_t event){
/* CARD_ID_TELEKINESIS	808
Telekinesis	|U|U
Instant
Tap target creature. Prevent all combat damage that would be dealt by that creature this turn. It doesn't untap during its controller's next two untap steps.*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			if( is_tapped(instance->targets[0].player, instance->targets[0].card) ){
				ai_modifier += (get_attack_power(instance->targets[0].player, instance->targets[0].card)*8);
			}
			if( is_attacking(instance->targets[0].player, instance->targets[0].card) ){
				ai_modifier += (get_attack_power(instance->targets[0].player, instance->targets[0].card)*8);
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
			int reincarnated = create_targetted_legacy_effect(player, card, &effect_telekinesis, instance->targets[0].player, instance->targets[0].card);
			card_instance_t *reinc = get_card_instance(player, reincarnated);
			reinc->targets[2].card = 3;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

// tempest efreet --> undoable

int card_teleport2(int player, int card, event_t event){
/* CARD_ID_TELEPORT	809
Teleport	|U|U|U
Instant
Cast ~ only during the declare attackers step.
Target creature can't be blocked this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && current_phase != PHASE_BEFORE_BLOCKING ){
		return 0;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier += (get_attack_power(instance->targets[0].player, instance->targets[0].card)*8);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

// At the beginning of each player's upkeep, destroy target nonartifact creature that player controls of his or her choice. It can't be regenerated.
int card_tetsuo_umezawa(int player, int card, event_t event){
/* CARD_ID_TETSUO_UMEZAWA	811
Tetsuo Umezawa	|U|B|R
Legendary Creature - Human Archer 3/3
~ can't be the target of Aura spells.
|U|B|B|R, |T: Destroy target tapped or blocking creature. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_TAPPED_OR_BLOCKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_UBR(1, 2, 1), 0,
									&td, "Select target tapped or blocking creature.");
}

void abyss_trigger(int player, int card, event_t event)
{
  upkeep_trigger_ability(player, card, event, ANYBODY);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.illegal_type = TYPE_ARTIFACT;
	  td.who_chooses = current_turn;
	  td.allowed_controller = current_turn;
	  td.preferred_controller = current_turn;
	  td.allow_cancel = 0;

	  card_instance_t* instance = get_card_instance(player, card);
	  instance->number_of_targets = 0;
	  if (can_target(&td) && pick_next_target_noload(&td, "Select target nonartifact creature you control."))
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
	}
}

int card_the_abyss(int player, int card, event_t event)
{
  /* The Abyss	|3|B
   * World Enchantment
   * At the beginning of each player's upkeep, destroy target nonartifact creature that player controls of his or her choice. It can't be regenerated. */

  // 0x416140



  abyss_trigger(player, card, event);
  return enchant_world(player, card, event);
}

int card_the_brute(int player, int card, event_t event){
/* CARD_ID_THE_BRUTE	813
The Brute	|1|R
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+0.
|R|R|R: Regenerate enchanted creature. */

	card_instance_t* instance = get_card_instance(player, card);
	if (instance->damage_target_player >= 0){
		instance = get_card_instance(player, card);
		if (IS_GAA_EVENT(event)){
			int p = instance->damage_target_player, c = instance->damage_target_card;
			if (event == EVENT_RESOLVE_ACTIVATION){
				if (can_be_regenerated(p, c)){ regenerate_target(p, c); }
			}else if (event == EVENT_CAN_ACTIVATE){
				if (p < 0 || !(land_can_be_played & LCBP_REGENERATION) || !can_be_regenerated(p, c)){ return 0; }
			}
			return generic_activated_ability(player, card, event, 0, MANACOST_R(3), 0, NULL, NULL) ? 99 : 0;
		}
	}
	return generic_aura(player, card, event, player, 1, 0, 0, 0, 0, 0, 0);
}

int card_the_tabernacle_at_pendrell_vale(int player, int card, event_t event)
{
  /* CARD_ID_THE_TABERNACLE_AT_PENDRELL_VALE	806
The Tabernacle at Pendrell Vale	""
Legendary Land
All creatures have "At the beginning of your upkeep, destroy this creature unless you pay |1." */

  if (event == EVENT_RESOLVE_SPELL)
    play_land_sound_effect(player, card);
  check_legend_rule(player, card, event);
  if (!is_humiliated(player, card, AL_TRIG))
    pendrell_effect(player, card, event, KILL_DESTROY);
  if (mana_colors_added[player])
    return plain_mana_producer(player, card, event);
  return 0;
}

// thunder spirit --> vanilla

int card_the_wretched(int player, int card, event_t event){
  /* The Wretched |3|B|B
   * Creature — Demon 2/5
   * At end of combat, gain control of all creatures blocking The Wretched for as long as you control The Wretched. */

	card_instance_t *instance = get_card_instance(player, card);

	if( current_turn == player && event == EVENT_DECLARE_BLOCKERS ){
		int count = 0;
		instance->info_slot = 0;
		while( count < active_cards_count[1-player] ){
				if( in_play(1-player, count) && is_what(1-player, count, TYPE_CREATURE) ){
					if( is_blocking(count, card) ){
						instance->targets[instance->info_slot].card = count;
						instance->info_slot++;
					}
				}
				count++;
		}
	}

	if( instance->info_slot > 0 && end_of_combat_trigger(player, card, event, 2) ){
		int amount = instance->info_slot;
		int i;
		for(i = 0; i < amount; i++ ){
			if( in_play( 1-player, instance->targets[i].card) ){
				gain_control_until_source_is_in_play_and_tapped(player, card, 1-player, instance->targets[i].card, GCUS_SOURCE_IN_PLAY | GCUS_CONTROLLED);
			}
		}
		instance->info_slot = 0;
	}

	return 0;
}

static int time_elemental_legacy(int player, int card, event_t event){
	if( event == EVENT_SHOULD_AI_PLAY || end_of_combat_trigger(player, card, event, 2) ){
		card_instance_t *instance = get_card_instance( player, card );
		damage_player(player, 5, player, card);
		if (in_play(instance->targets[0].player, instance->targets[0].card) && player == instance->targets[0].player &&
			! check_state(instance->targets[0].player, instance->targets[0].card, STATE_OUBLIETTED) )
		{
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
		}
		kill_card(player, card, KILL_SACRIFICE);
	}
	return 0;
}

int card_time_elemental(int player, int card, event_t event)
{
  /* CARD_ID_TIME_ELEMENTAL	817
   * Time Elemental	|2|U
   * Creature - Elemental 0/2
   * When ~ attacks or blocks, at end of combat, sacrifice it and it deals 5 damage to you.
   * |2|U|U, |T: Return target permanent that isn't enchanted to its owner's hand. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  bool create_effect = false;
  if (event == EVENT_DECLARE_ATTACKERS)
  {
    result = is_attacking(player, card);
    if (result != 0)
    {
      result = is_humiliated(player, card, AL_TRIG);
      create_effect = result == 0;
    }
    else if (player != current_turn)
    {
      result = blocking(player, card, event);
      if (result != 0)
      {
        result = is_humiliated(player, card, AL_TRIG);
        create_effect = result == 0;
      }
    }
  }
  else if (current_turn != player)
  {
    result = blocking(player, card, event);
    if (result != 0)
    {
      result = is_humiliated(player, card, AL_TRIG);
      create_effect = result == 0;
    }
  }
  if (create_effect)
  {
    result = create_legacy_effect(player, card, time_elemental_legacy);
    instance = get_card_instance(player, result);
    instance->targets[0].player = player;
    instance->targets[0].card = card;
    instance->number_of_targets = '\x01';
  }
  if (event == EVENT_ATTACK_RATING)
  {
    if (card == affected_card && player == affected_card_controller)
      ai_defensive_modifier = ai_defensive_modifier + 0x78;
  }
  else if (event == EVENT_BLOCK_RATING)
  {
    if (card == affected_card && player == affected_card_controller)
      ai_defensive_modifier = ai_defensive_modifier - 0x78;
  }
  else if (event == EVENT_CHECK_DESTROY_IF_BLOCKED)
  {
    if (card != affected_card || player != affected_card_controller)
      return 0;
    result = is_humiliated(player, card, AL_TRIG);
    if (result != 0)
      return 0;
    event_result = event_result | 2;
    return 0;
  }
  else if (event == EVENT_CHANGE_TYPE)
  {
    if (card == affected_card && player == affected_card_controller &&
        (result = is_humiliated(player, card, AL_TRIG), result == 0))
    {
      instance = get_card_instance(player, card);
      instance->destroys_if_blocked = instance->destroys_if_blocked | 1;
    }
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 0x47);
  td.illegal_state = TARGET_STATE_ENCHANTED;
  instance = get_card_instance(player, card);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    bounce_permanent(instance->targets[0].player, instance->targets[0].card);
  }
  return generic_activated_ability(player, card, event, 5, 2, 0, 2, 0, 0, 0, 0, &td,
                                   "TIME_ELEMENTAL");
}

static int tolaria_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_ABILITIES && affect_me(p, c) ){
			if( event_result & KEYWORD_BANDING ){
				event_result &= ~KEYWORD_BANDING;
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_tolaria(int player, int card, event_t event){
/* CARD_ID_TOLARIA	819
Tolaria	""
Legendary Land
|T: Add |U to your mana pool.
|T: Target creature loses banding and all "bands with other" abilities until end of turn. Activate this ability only during any upkeep step. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		int choice = instance->info_slot = instance->number_of_targets = 0;
		if( ! paying_mana() && current_phase == PHASE_UPKEEP &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL)
		  ){
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Remove Banding\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		if( choice == 1 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST0) && pick_target(&td, "TARGET_CREATURE") ){
				tap_card(player, card);
				instance->info_slot = 1;
			}
		}
		if( choice == 2 ){
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
			if( valid_target(&td) ){
				create_targetted_legacy_effect(player, instance->parent_card, &tolaria_legacy,
												instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_tor_wauki(int player, int card, event_t event){
/* CARD_ID_TOR_WAUKI	820
Tor Wauki	|2|B|B|R
Legendary Creature - Human Archer 3/3
|T: ~ deals 2 damage to target attacking or blocking creature. */

	check_legend_rule(player, card, event);

	return card_heavy_ballista(player, card, event);
}

int card_touch_of_darkness(int player, int card, event_t event){
/* CARD_ID_TOUCH_OF_DARKNESS	822
Touch of Darkness	|B
Instant
Any number of target creatures become |Sblack until end of turn. */

	return song(player, card, event, COLOR_TEST_BLACK);
}

int card_transmutation(int player, int card, event_t event){
/* CARD_ID_TRANSMUTATION	823
Transmutation	|1|B
Instant
Switch target creature's power and toughness until end of turn. */

	if( event == EVENT_CHECK_PUMP ){
		card_instance_t* instance = get_card_instance(player, card);
		if( has_mana_to_cast_iid(player, EVENT_CAN_CAST, instance->internal_card_id) ){
			if( instance->targets[0].player > -1 && instance->targets[0].card > -1 && instance->number_of_targets > 0 ){
				int pow = get_power(instance->targets[0].player, instance->targets[0].card);
				int tou = get_toughness(instance->targets[0].player, instance->targets[0].card);
				pumpable_power[player] += (tou-get_base_power(instance->targets[0].player, instance->targets[0].card));
				pumpable_toughness[player] += (pow-get_base_toughness(instance->targets[0].player, instance->targets[0].card));
			}
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	// Switch target creature's power and toughness until end of turn.
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td))
			switch_power_and_toughness_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

// tundra wolves

int card_triassic_egg(int player, int card, event_t event)
{
  /* Triassic Egg	|4
   * Artifact
   * |3, |T: Put a hatchling counter on ~.
   * Sacrifice ~: Choose one - You may put a creature card from your hand onto the battlefield;
   * or return target creature card from your graveyard to the battlefield.
   * Activate this ability only if two or more hatchling counters are on ~. */

  enum
  {
    CHOICE_COUNTER = 1,
    CHOICE_TUTOR_HAND = 2,
    CHOICE_TUTOR_GRAVE = 3
  };

  int choice;
  card_instance_t *instance;
  int hand_tutor_available;
  unsigned int grave_tutor_available;
  int32_t ability_choice;
  int charge_available_or_selection;
  int grave_count_or_shroud;
  bool end_of_turn;
  test_definition_t this_test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 3, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL) == 0)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 0, 0, 0, 0, 0, 0,
                                         0x1350200, NULL, NULL);
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {
      end_of_turn = current_phase == 0x1f;
      instance->info_slot = 0;
      if (end_of_turn)
      {
        ai_modifier = ai_modifier + 100;
      }
      charge_available_or_selection = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1,
                                                                3, 0, 0, 0, 0, 0, 0, NULL, NULL);
      hand_tutor_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 0, 0, 0,
                                                       0, 0, 0, 0x1350200, NULL, NULL);
      if ((hand_tutor_available == 0) ||
          (grave_count_or_shroud = count_graveyard_by_type(player, 2), grave_count_or_shroud == 0))
      {
        grave_tutor_available = 0;
      }
      else
      {
        grave_count_or_shroud = graveyard_has_shroud(player);
        grave_tutor_available = (unsigned int)(grave_count_or_shroud == 0);
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Add a counter",
                           charge_available_or_selection, 2, "Tutor creature from hand",
                           hand_tutor_available, 5, "Tutor creature grave", grave_tutor_available,
                           10, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_COUNTER)
      {
        if (charge_mana_for_activated_ability(player, card, 3, 0, 0, 0, 0, 0) == 0)
        {
          return 0;
        }
        add_state(player, card, 0x10);
        tapped_for_mana_color = -1;
        instance->info_slot = 1;
        ai_modifier = ai_modifier + 0x18;
        return 0;
      }
      if (charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) == 0)
      {
        return 0;
      }
      if (choice == CHOICE_TUTOR_HAND)
      {
        instance->info_slot = 2;
      }
      else
      {
        if (choice != CHOICE_TUTOR_GRAVE)
        {
          return 0;
        }
        new_default_test_definition(&this_test, 2, "Select a creature card.");
        charge_available_or_selection =
            select_target_from_grave_source(player, card, player, SFG_0, 2, &this_test, 0);
        if (charge_available_or_selection == -1)
        {
          spell_fizzled = 1;
          return 0;
        }
        instance->info_slot = 3;
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
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    ability_choice = instance->info_slot;
    if (ability_choice == 1)
    {
      add_counters(player, card, instance->parent_controller, instance->parent_card,
                   COUNTER_HATCHLING, 1);
      ability_choice = instance->info_slot;
    }
    if (ability_choice == 2)
    {
      new_default_test_definition(&this_test, 2, "Select a creature card.");
      new_global_tutor(player, player, 4, 4, 0, 2, &this_test);
      ability_choice = instance->info_slot;
    }
    if ((ability_choice == 3) &&
        (charge_available_or_selection = validate_target_from_grave_source(player, card, player, 0),
         charge_available_or_selection != -1))
    {
      reanimate_permanent(player, card, player, charge_available_or_selection, REANIMATE_DEFAULT);
      return 0;
    }
  }
  return 0;
}

int card_tuknir_deathlock(int player, int card, event_t event){
/* CARD_ID_TUKNIR_DEATHLOCK	825
Tuknir Deathlock	|R|R|G|G
Legendary Creature - Human Wizard 2/2
Flying
|R|G, |T: Target creature gets +2/+2 until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_GR(1, 1), 0, &td, "TARGET_CREATURE");
}

int card_typhoon(int player, int card, event_t event){
/* CARD_ID_TYPHOON	827
Typhoon	|2|G
Sorcery
~ deals damage to each opponent equal to the number of |H1Islands that player controls. */

	if( event == EVENT_RESOLVE_SPELL ){
		damage_player(1-player, count_subtype(1-player, TYPE_PERMANENT, get_hacked_subtype(player, card, SUBTYPE_ISLAND)), player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 1, NULL);
}

int card_undertow(int player, int card, event_t event){
/* CARD_ID_UNDERTOW	828
Undertow	|2|U
Enchantment
Creatures with |H2islandwalk can be blocked as though they didn't have |H2islandwalk. */

	landwalk_disabling_card(player, card, event, PB_ISLANDWALK_DISABLED);
	return global_enchantment(player, card, event);
}

//unholy citadel --> impossible
/* CARD_ID_UNHOLY_CITADEL	830
Unholy Citadel	""
Land
|SBlack legendary creatures you control have "bands with other legendary creatures." (Any legendary creatures can attack in a band as long
as at least one has "bands with other legendary creatures." Bands are blocked as a group. If at least two legendary creatures you control,
one of which has "bands with other legendary creatures," are blocking or being blocked by the same creature, you divide that creature's combat damage,
not its controller, among any of the creatures it's being blocked by or is blocking.) */

int card_underworld_dreams(int player, int card, event_t event){
/* CARD_ID_UNDERWORLD_DREAMS	829
Underworld Dreams	|B|B|B
Enchantment
Whenever an opponent draws a card, ~ deals 1 damage to him or her. */

	if (card_drawn_trigger(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY)){
		damage_player(1-player, 1, player, card);
	}
	return global_enchantment(player, card, event);
}

int card_untamed_wilds(int player, int card, event_t event){
/* CARD_ID_UNTAMED_WILDS	831
Untamed Wilds	|2|G
Sorcery
Search your library for a basic land card and put that card onto the battlefield. Then shuffle your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		tutor_basic_land(player, 1, 0);
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_ur_drago(int player, int card, event_t event){
/* CARD_ID_UR_DRAGO	832
Ur-Drago	|3|U|U|B|B
Legendary Creature - Elemental 4/4
First strike
Creatures with |H2swampwalk can be blocked as though they didn't have |H2swampwalk. */

	check_legend_rule(player, card, event);
	landwalk_disabling_card(player, card, event, PB_SWAMPWALK_DISABLED);
	return 0;
}

static int urborg_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_ABILITIES && affect_me(p, c) ){
			if( instance->targets[1].player == 66 && (event_result & KEYWORD_FIRST_STRIKE) ){
				event_result &= ~KEYWORD_FIRST_STRIKE;
			}
			if( instance->targets[1].player == 67 && (event_result & KEYWORD_BASIC_LANDWALK) ){
				event_result &= ~get_hacked_walk(player, card, KEYWORD_SWAMPWALK);
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_urborg(int player, int card, event_t event){
/* CARD_ID_URBORG	833
Urborg	""
Legendary Land
|T: Add |B to your mana pool.
|T: Target creature loses first strike or |H2swampwalk until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		int choice = instance->info_slot = instance->number_of_targets = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Remove ability\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		if( choice == 1 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST0) && pick_target(&td, "TARGET_CREATURE") ){
				tap_card(player, card);
				instance->info_slot = 1;
			}
		}
		if( choice == 2 ){
			spell_fizzled = 1;
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
				if( valid_target(&td) ){
					int choice = 0;
					int ai_choice = 0;
					keyword_t walk = get_hacked_walk(player, card, KEYWORD_SWAMPWALK);
					if( check_for_ability(instance->targets[0].player, instance->targets[0].card, KEYWORD_FIRST_STRIKE) ){
						if( check_for_ability(instance->targets[0].player, instance->targets[0].card, walk) ){
							if( current_phase < PHASE_DECLARE_BLOCKERS ){
								ai_choice = 0;
							}
							char buf[128];
							sprintf(buf, " Remove first strike\n Remove %swalk",
									walk == KEYWORD_SWAMPWALK ? "swamp"
									: walk == KEYWORD_ISLANDWALK ? "ISLAND"
									: walk == KEYWORD_FORESTWALK ? "FOREST"
									: walk == KEYWORD_MOUNTAINWALK ? "MOUNTAIN"
									: "PLAINS");
							choice = do_dialog(player, instance->targets[0].player, instance->targets[0].card, -1, -1, buf, ai_choice);
						}
					}
					else{
						choice = 1;
					}
					int legacy = create_targetted_legacy_effect(player, instance->parent_card, &urborg_legacy,
																instance->targets[0].player, instance->targets[0].card);
					card_instance_t *leg = get_card_instance( player, legacy );
					leg->targets[1].player = 66+choice;
				}
		}
	}

	return 0;
}

int card_vampire_bats(int player, int card, event_t event){
  /* CARD_ID_VAMPIRE_BATS	835
   * Vampire Bats	|B
   * Creature - Bat 0/1
   * Flying (This creature can't be blocked except by creatures with flying or reach.)
   * |B: ~ gets +1/+0 until end of turn. Activate this ability no more than twice each turn. */

 // => Pit Imp and Phyrexian Battleflies


	 card_instance_t *instance = get_card_instance(player, card);

	 if( event == EVENT_CAN_ACTIVATE && instance->info_slot < 2){
	     return generic_shade_merge_pt(player, card, event, 0, MANACOST_B(1), 1, 0);
	 }

	 if( event == EVENT_ACTIVATE && instance->info_slot < 2){
		  generic_shade_merge_pt(player, card, event, 0, MANACOST_B(1), 1, 0);
		  if( spell_fizzled != 1 ){
			  instance->info_slot++;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade_merge_pt(player, card, event, 0, MANACOST_B(1), 1, 0);
	}

	if( event == EVENT_CLEANUP ){
		instance->info_slot = 0;
	}

	return 0;
}

int card_vaevictis_asmadi(int player, int card, event_t event){
  /* Vaevictis Asmadi |2|B|B|R|R|G|G
   * Legendary Creature — Elder Dragon 7/7
   * Flying
   * At the beginning of your upkeep, sacrifice Vaevictis Asmadi unless you pay {B}{R}{G}.
   * {B}: Vaevictis Asmadi gets +1/+0 until end of turn.
   * {R}: Vaevictis Asmadi gets +1/+0 until end of turn.
   * {G}: Vaevictis Asmadi gets +1/+0 until end of turn. */

	check_legend_rule(player, card, event);

	basic_upkeep(player, card, event, MANACOST_BRG(1, 1, 1));

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		if( has_mana_for_activated_ability(player, card, MANACOST_B(1)) ){
			return 1;
		}
		if( has_mana_for_activated_ability(player, card, MANACOST_R(1)) ){
			return 1;
		}
		if( has_mana_for_activated_ability(player, card, MANACOST_G(1)) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		charge_mana_for_activated_ability(player, card, MANACOST_B(1));
		if( spell_fizzled == 1 ){
			spell_fizzled = 0;
			charge_mana_for_activated_ability(player, card, MANACOST_R(1));
			if( spell_fizzled == 1 ){
				spell_fizzled = 0;
				charge_mana_for_activated_ability(player, card, MANACOST_G(1));
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
										instance->parent_controller, instance->parent_card, 1, 0);
	}

	return 0;
}

int card_venarian_gold(int player, int card, event_t event)
{
  /* Venarian Gold	|X|U|U
   * Enchantment - Aura
   * Enchant creature
   * When ~ enters the battlefield, tap enchanted creature and put X sleep counters on it.
   * Enchanted creature doesn't untap during its controller's untap step if it has a sleep counter on it.
   * At the beginning of the upkeep of enchanted creature's controller, remove a sleep counter from that creature. */

  char enchanted_player;
  card_instance_t *instance;
  int enchanted_card_or_result;
  int target_player;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    instance = get_card_instance(player, card);
    enchanted_player = instance->damage_target_player;
    enchanted_card_or_result = instance->damage_target_card;
    add_counters(player, card, (int)enchanted_player, enchanted_card_or_result, COUNTER_SLEEP,
                 instance->info_slot);
    tap_card((int)enchanted_player, enchanted_card_or_result);
  }
  instance = in_play(player, card);
  if (!(((instance == NULL) || (enchanted_card_or_result = is_humiliated(player, card, AL_TRIG),
                                enchanted_card_or_result != 0)) ||
        (instance = get_card_instance(player, card), instance->damage_target_player < '\0')))
  {

    instance = get_card_instance(player, card);
    target_player = (int)instance->damage_target_player;
    enchanted_card_or_result = instance->damage_target_card;
    if (event == EVENT_UNTAP)
    {
      if (((enchanted_card_or_result == affected_card) &&
           (target_player == affected_card_controller)) &&
          (count_counters(target_player, enchanted_card_or_result, COUNTER_SLEEP) != 0))
      {
        does_not_untap(target_player, enchanted_card_or_result, EVENT_UNTAP);
      }
      upkeep_trigger_ability(player, card, EVENT_UNTAP, target_player);
    }
    else
    {
      upkeep_trigger_ability(player, card, event, target_player);
      if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
        remove_counters(player, card, target_player, enchanted_card_or_result, COUNTER_SLEEP, 1);
    }
  }

  if (event != EVENT_CAST_SPELL)
  {
    if (event == EVENT_CAN_CAST)
      return disabling_aura(player, card, event);
    if (event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET ||
        event == EVENT_CHANGE_TARGET)
      return disabling_aura(player, card, event);
  }
  else if (((card == affected_card) &&
            ((player != affected_card_controller ||
              (generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 0, NULL),
               card == affected_card)))) &&
           (player == affected_card_controller))
    return disabling_aura(player, card, event);
  if (event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET ||
      event == EVENT_CHANGE_TARGET)
    return disabling_aura(player, card, event);

  if (((((event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA) &&
        (2 < (uint32_t)(event - EVENT_POWER))) &&
       ((event != EVENT_RESOLVE_MOVING_AURA &&
         ((event != EVENT_CHANGE_TYPE && (event != EVENT_SET_COLOR)))))) &&
      ((event != EVENT_GET_SELECTED_CARD && (event != EVENT_STATIC_EFFECTS))))
  {
    return 0;
  }
  return disabling_aura(player, card, event);
}

int card_visions2(int player, int card, event_t event){
/* CARD_ID_VISIONS	837
Visions	|W
Sorcery
Look at the top five cards of target player's library. You may then have that player shuffle that library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int *deck = deck_ptr[player];
			show_deck( player, deck, 5, "Cards revealed by Visions", 0, gs_done_008b40e0 );
			int choice = do_dialog(player, player, card, -1, -1, " Reshuffle target player's deck\n Pass", instance->targets[0].player == player ? 1 : 0);
			if( ! choice ){
				shuffle(instance->targets[0].player);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_PLAYER", 1, NULL);
}

// walking dead --> drudge skeletons

// wall of caltrops --> impossible
/* CARD_ID_WALL_OF_CALTROPS	840
Wall of Caltrops	|1|W
Creature - Wall 2/1
Defender (This creature can't attack.)
Whenever ~ blocks a creature, if at least one other Wall creature is blocking that creature and no non-Wall creatures are blocking that creature,
~ gains banding until end of turn. (If any creatures with banding you control are blocking a creature, you divide that creature's combat damage,
not its controller, among any of the creatures it's being blocked by.) */

int card_voodoo_doll( int player, int card, event_t event){
	/* Voodoo Doll	|6	0x200205a
	 * Artifact
	 * At the beginning of your upkeep, put a pin counter on ~.
	 * At the beginning of your end step, if ~ is untapped, destroy ~ and it deals damage to you equal to the number of pin counters on it.
	 * |X|X, |T: ~ deals damage equal to the number of pin counters on it to any target. X is the number of pin counters on ~. */
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counters(player, card, player, card, COUNTER_PIN, 1);
	}

	if( player == current_turn && !is_tapped(player, card) && eot_trigger(player, card, event) ){
		int dam = count_counters(player, card, COUNTER_PIN);
		if (dam > 0){
			damage_player(player, dam, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td1) ){
			damage_target0(player, card, count_counters(player, card, COUNTER_PIN));
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(2 * count_counters(player, card, COUNTER_PIN)), 0, &td1, "TARGET_ANY");
}

static int effect_cant_attack_during_controllers_next_turn(int player, int card, event_t event)
{
  // 0x4a0c10
  effect_cannot_attack(player, card, event);

  if ((event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY) && affect_me(player, card))
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  if (instance->damage_target_player < 0 || instance->damage_target_card < 0
		  || !in_play(instance->damage_target_player, instance->damage_target_card)
		  || (current_turn == instance->damage_target_player
			  && --get_card_instance(player, card)->targets[1].player <= 0))
		kill_card(player, card, KILL_EXILE);
	}

  return 0;
}

static void add_effect_cant_attack_during_controllers_next_turn(int player, int card, int t_player, int t_card)
{
  int leg = create_targetted_legacy_effect(player, card, effect_cant_attack_during_controllers_next_turn, t_player, t_card);
  if (leg != -1)
	{
	  card_instance_t* legacy = get_card_instance(player, leg);
	  legacy->targets[1].player = (current_turn == t_player) ? 2 : 1;
	}
}

// wall of earth, wall of heat, wall of light --> vanilla

int card_wall_of_dust(int player, int card, event_t event)
{
/* CARD_ID_WALL_OF_DUST	841
Wall of Dust	|2|R
Creature - Wall 1/4
Defender (This creature can't attack.)
Whenever ~ blocks a creature, that creature can't attack during its controller's next turn. */

  //0x4cab80
  if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card, AL_TRIG)
	  && player == 1-current_turn && (get_card_instance(player, card)->state & STATE_BLOCKING))
	for_each_creature_blocked_by_me(player, card, add_effect_cant_attack_during_controllers_next_turn, player, card);

  return 0;
}

int card_wall_of_opposition(int player, int card, event_t event){
/* CARD_ID_WALL_OF_OPPOSITION	845
Wall of Opposition	|3|R|R
Creature - Wall 0/6
Defender (This creature can't attack.)
|1: ~ gets +1/+0 until end of turn. */

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(1), 1, 0);
}

int card_wall_of_putrid_flesh(int player, int card, event_t event){
/* CARD_ID_WALL_OF_PUTRID_FLESH	846
Wall of Putrid Flesh	|2|B
Creature - Wall 2/4
Defender (This creature can't attack.)
Protection from |Swhite
Prevent all damage that would be dealt to ~ by enchanted creatures. */

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->info_slot > 0 && damage->damage_target_card == card ){
			if( is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE) &&
				is_enchanted(damage->damage_source_player, damage->damage_source_card) )
			{
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
	}

	return 0;
}

int card_wall_of_shadows(int player, int card, event_t event)
{
  /* CARD_ID_WALL_OF_SHADOWS	847
   * Wall of Shadows	|1|B|B
   * Creature - Wall 0/1
   * Defender (This creature can't attack.)
   * Prevent all damage that would be dealt to ~ by creatures it's blocking.
   * ~ can't be the target of spells that can target only Walls or of abilities that can target only Walls. */

  if (event == EVENT_CHECK_ABILITIES && affect_me(player, card) &&
      1 - player == current_turn && current_phase < 0x18 &&
      !is_humiliated(player, card, AL_STATIC))
    check_abilities_toughness += 0x3fff;
  card_instance_t *damage = damage_being_prevented(event);
  if (damage && damage->damage_target_player == player && damage->damage_target_card == card &&
      (damage->targets[3].player & 2) && current_turn != player &&
      damage->damage_source_player == current_turn &&
      in_play(damage->damage_source_player, damage->damage_source_card) &&
      is_blocking(card, damage->damage_source_card) &&
      !is_humiliated(player, card, AL_STATIC))
    prevent_damage(affected_card_controller, affected_card, -1);
  return 0;
}

int card_wall_of_tombstones(int player, int card, event_t event)
{
  /* CARD_ID_WALL_OF_TOMBSTONES	848
Wall of Tombstones	|1|B
Creature - Wall 0/1
Defender (This creature can't attack.)
At the beginning of your upkeep, change ~'s base toughness to 1 plus the number of creature cards in your graveyard. (This effect lasts indefinitely.) */

  if (upkeep_trigger(player, card, event) && current_turn == player && !is_humiliated(player, card, AL_TRIG))
  {
    int toughness = count_graveyard_by_type(player, TYPE_CREATURE) + 1;
    int legacy = real_set_pt(player, card, player, card, -1, toughness, SET_PT_RESET_SUBTYPES_AT_END);
    add_status(player, legacy, STATUS_INVISIBLE_FX);
  }
  return 0;
}

int card_wall_of_vapor(int player, int card, event_t event)
{
  /* CARD_ID_WALL_OF_VAPOR	849
   * Wall of Vapor	|3|U
   * Creature - Wall 0/1
   * Defender (This creature can't attack.)
   * Prevent all damage that would be dealt to ~ by creatures it's blocking. */

  int result;
  card_instance_t *instance;
  card_instance_t *damage;
  if (event == EVENT_CHECK_ABILITIES)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (1 - player == current_turn)) &&
        (current_phase < 0x18))
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        check_abilities_toughness = check_abilities_toughness + 0x3fff;
      }
    }
  }
  else if (event == EVENT_PREVENT_DAMAGE)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      instance = get_card_instance(player, card);
      damage = get_card_instance(affected_card_controller, affected_card);
      if ((damage->internal_card_id == damage_card) && (0 < damage->info_slot))
      {
        result = is_what((int)damage->damage_source_player, damage->damage_source_card, 2);
        if ((result != 0) && (((unsigned int)instance->blocking == damage->damage_source_card &&
                               ((int)damage->damage_source_player == 1 - player))))
        {
          prevent_damage(affected_card_controller, affected_card, -1);
        }
      }
    }
  }
  return 0;
}

int card_wall_of_wonder(int player, int card, event_t event)
{
/* CARD_ID_WALL_OF_WONDER	850
Wall of Wonder	|2|U|U
Creature - Wall 1/5
Defender (This creature can't attack.)
|2|U|U: ~ gets +4/-4 until end of turn and can attack this turn as though it didn't have defender. */

  //0x42b670
  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 4, -4);
	  int leg = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, effect_defender_can_attack_until_eot,
											   instance->parent_controller, instance->parent_card);
	  if (leg != -1)
		get_card_instance(instance->parent_controller, leg)->token_status = STATUS_INVISIBLE_FX;
	}

  if (event == EVENT_POW_BOOST && has_mana_for_activated_ability(player, card, MANACOST_XU(2, 2)))
	return 4;

  return generic_activated_ability(player, card, event, 0, MANACOST_XU(2, 2), 0, NULL, NULL);
}

int card_whirling_dervish(int player, int card, event_t event){
  /* Whirling Dervish |G|G
   * Creature — Human Monk 1/1
   * Protection from black
   * At the beginning of each end step, if Whirling Dervish dealt damage to an opponent this turn, put a +1/+1 counter on it. */

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_OPPONENT) ){
		get_card_instance( player, card )->targets[1].player = 66;
	}
	if( get_card_instance( player, card )->targets[1].player == 66 && eot_trigger(player, card, event) && ! is_humiliated(player, card, AL_TRIG) ){
		add_counters(player, card, player, card, COUNTER_P1_P1, 1);
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return 0;
}

int card_white_mana_battery(int player, int card, event_t event)
{
/* CARD_ID_WHITE_MANA_BATTERY	852
White Mana Battery	|4
Artifact
|2, |T: Put a charge counter on ~.
|T, Remove any number of charge counters from ~: Add |W to your mana pool, then add an additional |W to your mana pool for each charge counter removed this way.*/

  // 0x4235C0
  return mana_battery(player, card, event, COLOR_WHITE);
}

int card_willow_satyr(int player, int card, event_t event){
  /* Willow Satyr	|2|G|G
   * Creature - Satyr 1/1
   * You may choose not to untap ~ during your untap step.
   * |T: Gain control of target legendary creature for as long as you control ~ and ~ remains tapped. */

	// original code : 0x407680



	choose_to_untap(player, card, event);

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_LEGEND;
	if (IS_AI(player)){
		td.allowed_controller = 1-player;
	}

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		card_instance_t* instance = get_card_instance(player, card);
		gain_control_until_source_is_in_play_and_tapped(player, card, instance->targets[0].player, instance->targets[0].card,
														GCUS_SOURCE_IN_PLAY | GCUS_CONTROLLED | GCUS_TAPPED);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_LEGENDARY_CREATURE");
}

int card_winds_of_change(int player, int card, event_t event){
/* CARD_ID_WINDS_OF_CHANGE	854
Winds of Change	|R
Sorcery
Each player shuffles the cards from his or her hand into his or her library, then draws that many cards. */

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p,{
					int result = reshuffle_hand_into_deck(p, 0);
					draw_cards(p, result);
				};
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_winter_blast(int player, int card, event_t event){
/* CARD_ID_WINTER_BLAST	855
Winter Blast	|X|G
Sorcery
Tap X target creatures. ~ deals 2 damage to each of those creatures with flying. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t* instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int max = check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ? instance->number_of_targets : 10;
		instance->number_of_targets = 0;
		int tg = 0;
		while( can_target(&td) && tg < max &&
				(check_special_flags2(player, card, SF2_COPIED_FROM_STACK) || has_mana(player, COLOR_COLORLESS, tg+1)) )
		{
				if( new_pick_target(&td, "TARGET_CREATURE", tg, 0) ){
					state_untargettable(instance->targets[tg].player, instance->targets[tg].card, 1);
					tg++;
				}
				else{
					break;
				}
		}
		int i;
		for(i=0; i<tg; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			if( (get_card_instance(instance->targets[i].player, instance->targets[i].card)->regen_status & KEYWORD_FLYING) &&
				get_toughness(instance->targets[i].player, instance->targets[i].card) <= 2 )
			{
				ai_modifier += 24;
			}
		}
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			charge_mana(player, COLOR_COLORLESS, tg);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				tap_card(instance->targets[i].player, instance->targets[i].card);
				if( check_for_ability(instance->targets[i].player, instance->targets[i].card, KEYWORD_FLYING) ){
					damage_creature(instance->targets[i].player, instance->targets[i].card, 2, player, card);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_wood_elemental(int player, int card, event_t event)
{
  /* Wood Elemental	|3|G
   * Creature - Elemental 100/100
   * As ~ enters the battlefield, sacrifice any number of untapped |H1Forests.
   * ~'s power and toughness are each equal to the number of |H1Forests sacrificed as it entered the battlefield. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  instance->state |= STATE_OUBLIETTED;

	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_LAND, get_hacked_land_text(player, card, "Select an untapped %s.", SUBTYPE_FOREST));
	  test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
	  test.state = STATE_TAPPED;
	  test.state_flag = DOESNT_MATCH;
	  test.qty = 500;

	  int sacced = new_sacrifice(player, card, player, SAC_DONE, &test);
	  instance->info_slot = sacced;
	  instance->regen_status |= KEYWORD_RECALC_POWER | KEYWORD_RECALC_TOUGHNESS;
	  instance->state &= ~STATE_OUBLIETTED;
	}

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card))
	{
	  if (card == -1)	// no forests sacrificed yet if not on the bf
		return 0;

	  set_characteristic_pt(get_card_instance(player, card)->info_slot);
	}

  return 0;
}

// zephyr falcon --> serra angel

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_xira_arien(int player, int card, event_t event){
/* CARD_ID_XIRA_ARIEN	858
Xira Arien	|B|R|G
Legendary Creature - Insect Wizard 1/2
Flying
|B|R|G, |T: Target player draws a card. */

	check_legend_rule(player, card, event);
	if (!IS_GAA_EVENT(event)){ return 0; }
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_NONE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;
	card_instance_t* instance = get_card_instance(player, card);
	if (event == EVENT_CAN_ACTIVATE){ return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_BRG(1, 1, 1), 0, &td, NULL); }
	if (event == EVENT_ACTIVATE){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_BRG(1, 1, 1), 0, &td, "TARGET_PLAYER");
		if (spell_fizzled != 1){ ai_modifier_for_targeted_card_drawing(player, card, 0, 1); }
	}
	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){ draw_cards(instance->targets[0].player, 1); }
	return 0;
}

#ifndef SHANDALAR
/* Recovered October 2022 pump effects and cleanup helpers. */
static void spirit_link_effect_part(int player, int card, event_t event, int who_gains);

/* ManalinkEh 2022: 0x21ecf20; original cards/legends.c:7383. */
static void spirit_link_effect_part(int player, int card, event_t event, int who_gains)
{
  card_instance_t *instance;

  instance = get_card_instance(affected_card_controller, affected_card);
  if ((((instance->internal_card_id == damage_card) && (0 < instance->info_slot)) &&
       (instance->damage_source_player == player)) &&
      (instance->damage_source_card == card))
  {
    gain_life(who_gains, instance->info_slot);
  }
  return;
}

/* ManalinkEh 2022: 0x22006a0; original cards/legends.c:7383. */
void spirit_link_effect(int player, int card, event_t event, int who_gains)
{
  if (event != EVENT_DEAL_DAMAGE)
  {
    return;
  }
  spirit_link_effect_part(player, card, EVENT_DEAL_DAMAGE, who_gains);
  return;
}

#endif
