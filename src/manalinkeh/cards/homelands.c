#include "manalink.h"

// Functions

int aether_storm_effect(int player, int card, event_t event);

int hom_clockwork(int player, int card, event_t event, int amount)
{
  card_instance_t *instance;
  int parent_player_cost_or_result;
  int parent_card_or_counters;
  card_instance_t *parent_in_play;
  int current_counters;
  unsigned int missing_counter_cost;

  instance = get_card_instance(player, card);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P0, amount);
  if (((event == EVENT_DECLARE_ATTACKERS) &&
       (parent_player_cost_or_result = is_attacking(player, card),
        parent_player_cost_or_result != 0)) ||
      (parent_player_cost_or_result = blocking(player, card, event),
       parent_player_cost_or_result != 0))
  {
    instance->targets[1].card = 0x42;
  }
  if (instance->targets[1].card == 0x42)
  {
    if (end_of_combat_trigger(player, card, event, 2) != 0)
    {
      remove_counters(player, card, player, card, COUNTER_P1_P0, 1);
      instance->targets[1].card = 0;
    }
  }
  parent_player_cost_or_result = displayed_x_value;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    parent_player_cost_or_result = instance->parent_controller;
    parent_card_or_counters = instance->parent_card;
    parent_in_play = in_play(parent_player_cost_or_result, parent_card_or_counters);
    if (parent_in_play != NULL)
    {
      current_counters =
          count_counters(parent_player_cost_or_result, parent_card_or_counters, COUNTER_P1_P0);
      if (current_counters < amount)
      {
        add_counters(player, card, parent_player_cost_or_result, parent_card_or_counters,
                     COUNTER_P1_P0, instance->info_slot);
        current_counters =
            count_counters(parent_player_cost_or_result, parent_card_or_counters, COUNTER_P1_P0);
        if (amount < current_counters)
        {
          remove_counters(player, card, parent_player_cost_or_result, parent_card_or_counters,
                          COUNTER_P1_P0, current_counters - amount);
        }
        else
        {
          ai_modifier = ai_modifier + 0x80;
        }
      }
      else
      {
        ai_modifier = ai_modifier + -0x100;
      }
    }
  }
  else
  {
    if (event == EVENT_GET_SELECTED_CARD)
    {
      parent_player_cost_or_result = count_counters(player, card, COUNTER_P1_P0);
      missing_counter_cost = amount - parent_player_cost_or_result;
      if ((int)missing_counter_cost < 2)
      {
        missing_counter_cost =
            missing_counter_cost & (int)(missing_counter_cost ^ 0xffffffff) >> 0x1f;
      }
      else
      {
        parent_player_cost_or_result = get_updated_counters_number(player, card, COUNTER_P1_P0, 1);
        missing_counter_cost =
            missing_counter_cost & (int)(missing_counter_cost ^ 0xffffffff) >> 0x1f;
        if (1 < parent_player_cost_or_result)
        {
          missing_counter_cost = (int)((missing_counter_cost - 1) + parent_player_cost_or_result) /
                                 parent_player_cost_or_result;
        }
      }
      parent_player_cost_or_result = has_mana(player, 7, 1);
      displayed_x_value = missing_counter_cost;
      if (parent_player_cost_or_result < (int)missing_counter_cost)
      {
        displayed_x_value = parent_player_cost_or_result;
      }
    }
    else
    {
      if (event == EVENT_ACTIVATE)
      {
        if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
        {
          parent_card_or_counters =
              generic_activated_ability(player, card, EVENT_ACTIVATE, 0x18001, displayed_x_value, 0,
                                        0, 0, 0, 0, 0, NULL, NULL);
          if (parent_player_cost_or_result == -1)
          {
            return parent_card_or_counters;
          }
          instance->info_slot = parent_player_cost_or_result;
          return parent_card_or_counters;
        }
      }
    }
  }
  parent_player_cost_or_result =
      generic_activated_ability(player, card, event, 0x18001, -1, 0, 0, 0, 0, 0, 0, NULL, NULL);
  if ((((parent_player_cost_or_result != 0) && (event == EVENT_CAN_ACTIVATE)) &&
       ((ai_is_speculating == 1 || ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))))) &&
      (parent_card_or_counters = count_counters(player, card, COUNTER_P1_P0),
       parent_card_or_counters <= amount / 2))
  {
    g_activation_event_flags = g_activation_event_flags | 3;
  }
  return parent_player_cost_or_result;
}

static int homelands_triple_land(int player, int card, event_t event, color_test_t primary,
                                 color_test_t secondary)
{
  card_instance_t *inst = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    inst->info_slot = COLOR_TEST_COLORLESS;
    play_land_sound_effect_force_color(player, card, primary | secondary | COLOR_TEST_COLORLESS);
    count_mana();
    return 0;
  }
  if (event == EVENT_CHANGE_TYPE)
  {
    int avail = has_mana(player, COLOR_COLORLESS, 1);
    inst->info_slot = mana_colors_added[player] | COLOR_TEST_COLORLESS |
                      (avail < 2 ? 0 : primary | (avail < 3 ? 0 : secondary));
    return 0;
  }
  if (event == EVENT_COUNT_MANA)
  {
    if (affect_me(player, card) && CAN_TAP_FOR_MANA(player, card) && inst->targets[1].player != 66)
    {
      int avail = has_mana(player, COLOR_ANY, 1);
      color_test_t colors = mana_colors_added[player] | COLOR_TEST_COLORLESS;
      if (avail == 1)
        colors |= primary;
      else if (avail > 1)
        colors |= primary | secondary;
      declare_mana_available_hex(player, colors, 1);
    }
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE)
    return inst->targets[1].player != 66 && mana_producer(player, card, event);
  if (event == EVENT_ACTIVATE)
  {
    int avail = has_mana(player, COLOR_COLORLESS, 1);
    int choice = COLOR_COLORLESS;
    for (int i = 0; i < 7; ++i)
      tapped_for_mana[i] = 0;
    if (check_special_flags2(player, card, SF2_CONTAMINATION) && is_what(player, card, TYPE_LAND))
    {
      produce_mana(player, COLOR_BLACK, 1);
      tapped_for_mana_color = COLOR_BLACK;
    }
    else if (check_special_flags2(player, card, SF2_DEEP_WATER) && card >= 0 &&
             is_what(player, card, TYPE_LAND))
    {
      produce_mana(player, COLOR_BLUE, 1);
      tapped_for_mana_color = COLOR_BLUE;
    }
    else
    {
      if (inst->targets[1].player == 66)
        return 0;
      color_test_t colors = mana_colors_added[player];
      if (avail >= 2)
      {
        colors |= primary;
        if (avail > 2)
          colors |= secondary;
      }
      if (avail >= 2 || colors)
        choice = choose_mana_all_one_color(player, colors | COLOR_TEST_COLORLESS, 1,
                                           "What kind of mana?", COLOR_TEST_COLORLESS);
      if (choice == -1)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
    if (!((mana_colors_added[player] | COLOR_TEST_COLORLESS) & color_to_color_test(choice)))
    {
      inst->targets[1].player = 66;
      charge_mana(player, COLOR_COLORLESS, (primary & (1 << choice)) ? 1 : 2);
      inst->targets[1].player = 0;
      if (spell_fizzled == 1)
        return 0;
    }
    if (!check_special_flags2(player, card, SF2_CONTAMINATION | SF2_DEEP_WATER) &&
        is_what(player, card, TYPE_LAND))
    {
      produce_mana(player, choice, 1);
      tapped_for_mana_color = choice;
    }
    get_card_instance(player, card)->state |= STATE_TAPPED;
  }
  return 0;
}

/* Abbey Gargoyles	|2|W|W|W	=>invasion.c:card_galinas_knight
 * Creature - Gargoyle 3/4
 * Flying, protection from |Sred */

// Cards

int card_abbey_matron(int player, int card, event_t event)
{
  /* Abbey Matron	|2|W	0x200d699
   * Creature - Human Cleric 1/3
   * |W, |T: ~ gets +0/+3 until end of turn. */
  return generic_shade_tap(player, card, event, MANACOST_W(1), 0,3);
}

/* ManalinkEh 2022: 0x211fcb0; original cards/homelands.c:167. */
int aether_storm_effect(int player, int card, event_t event)
{
  int p;
  bool source_found;
  card_instance_t *instance;
  card_instance_t *scanned_source;
  int source_player_or_id;
  int c;
  event_t event_00;
  uint32_t variable_costs;

  instance = get_card_instance(player, card);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) && (stack_size == 0)) &&
        (source_player_or_id = instance->targets[0].player, -1 < source_player_or_id))
    {

      source_player_or_id = get_id(source_player_or_id, instance->targets[0].card);
      if (source_player_or_id != 0x883)
      {
        instance->targets[0].card = -1;
        instance->targets[0].player = -1;
        kill_card(player, card, KILL_EXILE);
      }
      source_found = false;
      p = 0;
      do
      {
        c = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            scanned_source = in_play(p, c);
            if (((scanned_source != NULL) && (instance->targets[0].player == p)) &&
                (instance->targets[0].card == c))
            {
              source_found = true;
              break;
            }
            c = c + 1;
          } while (c <= active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
      if (!source_found)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      source_player_or_id = instance->targets[0].player;
      c = instance->targets[0].card;
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (can_pay_life_as_cost_for_spells_or_activated_abilities(player, 4) == 0)
        {
          return 0;
        }
        variable_costs = 0;
        event_00 = EVENT_CAN_ACTIVATE;
        return granted_generic_activated_ability(source_player_or_id, c, player, card, event_00, 0,
                                                 0, 0, 0, 0, 0, 0, variable_costs, NULL, NULL);
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      source_player_or_id = instance->targets[0].player;
      c = instance->targets[0].card;
    }
    else
    {
      if (event != EVENT_CLEANUP)
      {
        return 0;
      }
      source_player_or_id = instance->targets[0].player;
      c = instance->targets[0].card;
    }
    if (event == EVENT_ACTIVATE)
    {
      if (((ai_is_speculating == 1) ||
           ((source_player_or_id == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
          (current_turn != source_player_or_id))
      {
        ai_modifier = ai_modifier + -0x30;
      }
      variable_costs = 4;
      event_00 = EVENT_ACTIVATE;
      return granted_generic_activated_ability(source_player_or_id, c, player, card, event_00, 0, 0,
                                               0, 0, 0, 0, 0, variable_costs, NULL, NULL);
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) && (-1 < instance->targets[0].player))
    {
      kill_card(source_player_or_id, c, KILL_BURY);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2121430; original cards/homelands.c:219. */
int card_aether_storm(int player, int card, event_t event)
{
  /* AEther Storm	|3|U	0x000000
   * Enchantment
   * Creature spells can't be cast.
   * Pay 4 life: Destroy ~. It can't be regenerated. Any player may activate this ability. */

  int p;
  int c;
  card_instance_t *instance;
  int turn_or_result;
  int legacy_type;
  int temporary_card;
  int found[2];

  turn_or_result = current_turn;
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    instance = in_play(player, card);
    if ((((instance != NULL) &&
          (turn_or_result = is_humiliated(player, card, AL_STATIC), turn_or_result == 0)) &&
         (turn_or_result = is_what(affected_card_controller, affected_card, 2),
          turn_or_result != 0)) &&
        (turn_or_result = get_cmc(affected_card_controller, affected_card), turn_or_result != 0))
    {
      infinite_casting_cost();
    }
  }
  else
  {
    if (event == EVENT_STATIC_EFFECTS)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {

        found[0] = -1;
        found[1] = -1;
        c = active_cards_count[current_turn] + -1;
        if (-1 < c)
        {
          do
          {
            instance = in_play(turn_or_result, c);

            if (((instance != NULL) &&
                 (legacy_type = is_what(turn_or_result, c, 0x80), legacy_type != 0)) &&
                ((instance = get_card_instance(turn_or_result, c),
                  instance->info_slot_fn == aether_storm_effect &&
                      ((instance->targets[0].player == player &&
                        (instance->targets[0].card == card))))))
            {
              found[turn_or_result] = c;
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
            instance = in_play(p, c);

            if ((((instance != NULL) && (legacy_type = is_what(p, c, 0x80), legacy_type != 0)) &&
                 (instance = get_card_instance(p, c),
                  instance->info_slot_fn == aether_storm_effect)) &&
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))
            {
              found[p] = c;
            }
            c = c + -1;
          } while (c != -1);
        }
        if (-1 < found[player])
        {
          kill_card(player, found[player], KILL_EXILE);
        }
        p = 1 - player;
        if (found[p] == -1)
        {

          instance = get_card_instance(player, card);
          temporary_card = add_card_to_hand(p, instance->internal_card_id);
          instance =
              get_card_instance(p, create_legacy_activate(p, temporary_card, aether_storm_effect));
          instance->targets[0].card = card;
          instance->targets[0].player = player;
          instance->number_of_targets = '\x01';
          obliterate_card(p, temporary_card);
        }
      }
      return global_enchantment(player, card, event);
    }
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (can_pay_life_as_cost_for_spells_or_activated_abilities(player, 4) == 0)
      {
        return 0;
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {
      if (((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
          (current_turn != player))
      {
        ai_modifier = ai_modifier + -0x30;
      }
      if (spell_fizzled == 1)
      {
        return 0;
      }
      lose_life(player, 4);
      return 0;
    }
  }
  if (event == EVENT_CLEANUP)
  {
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    kill_card(instance->parent_controller, instance->parent_card, KILL_BURY);
    return 0;
  }
  return global_enchantment(player, card, event);
}

int card_alibans_tower(int player, int card, event_t event)
{
  /* Aliban's Tower	|1|R	0x200a44e
   * Instant
   * Target blocking creature gets +3/+1 until end of turn. */

	if (event == EVENT_CHECK_PUMP && has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id) &&
		current_turn != player)
	{
	  pumpable_power[player] += 3;
	  pumpable_toughness[player] += 1;
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_BLOCKING;

	return vanilla_pump(player, card, event, &td, 3, 1, 0, 0);
}

int card_ambush(int player, int card, event_t event)
{
  /* Ambush	|3|R	0x200d671
   * Instant
   * Blocking creatures gain first strike until end of turn. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.state = STATE_BLOCKING;
	  pump_creatures_until_eot(player, card, ANYBODY, 0, 0,0, KEYWORD_FIRST_STRIKE, 0, &test);
	  kill_card(player, card, KILL_DESTROY);
	}

  return basic_spell(player, card, event);
}

int card_ambush_party(int player, int card, event_t event)
{
  /* Ambush Party	|4|R
   * Creature - Human Rogue 3/1
   * First strike, haste */
  /* Blackcleave Goblin	|3|B
   * Haste
   * Infect (This creature deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.) */
  /* Brazen Scourge	|1|R|R
   * Creature - Gremlin 3/3
   * Haste */
  /* Brimstone Dragon	|6|R|R
   * Creature - Dragon 6/6
   * Flying, haste */
  /* Chandra's Phoenix	|1|R|R
   * Creature - Phoenix 2/2
   * Flying
   * Haste (This creature can attack and |T as soon as it comes under your control.)
   * Whenever an opponent is dealt damage by a |Sred instant or sorcery spell you control or by a |Sred planeswalker you control, return ~ from your graveyard to your hand. */
  /* Charging Monstrosaur	|4|R
   * Creature - Dinosaur 5/5
   * Trample, haste */
  /* Crazed Skirge	|3|B
   * Creature - Imp 2/2
   * Flying, haste */
  /* Gerrard's Irregulars	|4|R
   * Creature - Human Soldier 4/2
   * Trample, haste */
  /* Goblin Berserker	|3|R
   * Creature - Goblin Berserker 2/2
   * First strike, haste */
  /* Goblin Chariot	|2|R
   * Creature - Goblin Warrior 2/2
   * Haste (This creature can attack the turn it comes under your control.) */
  /* Goblin Striker	|1|R
   * Creature - Goblin Berserker 1/1
   * First strike, haste */
  /* Gurmag Swiftwing	|1|B
   * Creature - Bat 1/2
   * Flying, first strike, haste */
  /* Lightning Elemental	|3|R
   * Creature - Elemental 4/1
   * Haste (This creature can attack and |T as soon as it comes under your control.) */
  /* Minotaur Aggressor	|6|R
   * Creature - Minotaur Berserker 6/2
   * First strike, haste */
  /* Mountain Bandit	|R
   * Creature - Human Soldier Rogue 1/1
   * Haste */
  /* Nest Robber	|1|R
   * Creature - Dinosaur 2/1
   * Haste */
  /* Ogre Berserker	|4|R
   * Creature - Ogre Berserker 4/2
   * Haste */
  /* Raging Cougar	|2|R
   * Creature - Cat 2/2
   * Haste */
  /* Raging Goblin	|R
   * Creature - Goblin Berserker 1/1
   * Haste (This creature can attack and |T as soon as it comes under your control.) */
  /* Raging Minotaur	|2|R|R
   * Creature - Minotaur Berserker 3/3
   * Haste */
  /* Renegade Troops	|4|R
   * Creature - Human Soldier 4/2
   * Haste */
  /* Rip-Clan Crasher	|R|G
   * Creature - Human Warrior 2/2
   * Haste */
  /* Skyknight Legionnaire	|1|R|W
   * Creature - Human Knight 2/2
   * Flying, haste */
  /* Snare Thopter	|4
   * Artifact Creature - Thopter 3/2
   * Flying, haste */
  /* Speedway Fanatic	|1|R
   * Creature - Human Pilot 2/1
   * Haste
   * Whenever ~ crews a Vehicle, that Vehicle gains haste until end of turn. */
  /* Spike Jester	|B|R
   * Creature - Goblin Warrior 3/1
   * Haste */
  /* Talruum Minotaur	|2|R|R
   * Creature - Minotaur Berserker 3/3
   * Haste */
  /* Tenement Crasher	|5|R
   * Creature - Beast 5/4
   * Haste */
  /* Thundering Giant	|3|R|R
   * Creature - Giant 4/3
   * Haste (This creature can attack and |T as soon as it comes under your control.) */
  /* Tombspawn	(token)
   * Creature - Zombie 2/2
   * Haste */
  /* Volcanic Dragon	|4|R|R
   * Creature - Dragon 4/4
   * Flying
   * Haste (This creature can attack and |T as soon as it comes under your control.) */
  /* Vulshok Berserker	|3|R
   * Creature - Human Berserker 3/2
   * Haste (This creature can attack and |T as soon as it comes under your control.) */
  /* Warchief Giant	|3|R|R
   * Creature - Giant Warrior 5/3
   * Haste
   * Myriad (Whenever this creature attacks, for each opponent other than defending player, you may create a token that's a copy of this creature that's tapped and attacking that player or a planeswalker he or she controls. Exile the tokens at end of combat.) */
  /* Wild Colos	|2|R
   * Creature - Goat Beast 2/2
   * Haste */

  /* The first vanilla card with haste, so it gets everything *else* that's vanilla-with-haste pointed at it too.
   * Chandra's Phoenix's and Speeway Fanatic's last abilities are handled elsewhere.
   * Warchief Giant's last ability is just ignored. */

  haste(player, card, event);
  return 0;
}

int card_an_havva_constable(int player, int card, event_t event)
{
  /* An-Havva Constable	|1|G|G	0x200d676
   * Creature - Human 2/101
   * ~'s toughness is equal to 1 plus the number of |Sgreen creatures on the battlefield. */

  int result;
  if ((((event == EVENT_TOUGHNESS) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    result = get_sleighted_color_test(player, card, 8);
    result = count_permanents_by_color(2, 2, result);
    set_characteristic_pt(result + 1);
  }
  return 0;
}

int card_an_havva_inn(int player, int card, event_t event)
{
  /* An-Havva Inn	|1|G|G	0x200d67b
   * Sorcery
   * You gain X plus 1 life, where X is the number of |Sgreen creatures on the battlefield. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  gain_life(player, 1 + count_permanents_by_color(ANYBODY, TYPE_CREATURE, get_sleighted_color_test(player, card, COLOR_TEST_GREEN)));
	  kill_card(player, card, KILL_DESTROY);
	}
  return basic_spell(player, card, event);
}

int card_an_havva_township(int player, int card, event_t event)
{
  /* An-Havva Township	""	0x200d680
   * Land
   * |T: Add |1 to your mana pool.
   * |1, |T: Add |G to your mana pool.
   * |2, |T: Add |R or |W to your mana pool. */
  return homelands_triple_land(player, card, event, COLOR_TEST_GREEN, COLOR_TEST_RED|COLOR_TEST_WHITE);
}

int card_an_zerrin_ruins(int player, int card, event_t event){
	/* An-Zerrin Ruins	|2|R|R	0x200a3db
	 * Enchantment
	 * As ~ enters the battlefield, choose a creature type.
	 * Creatures of the chosen type don't untap during their controllers' untap steps. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = select_a_subtype(player, card);
	}

	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->info_slot > 0 && has_subtype(affected_card_controller, affected_card, instance->info_slot) ){
			card_instance_t *trg= get_card_instance(affected_card_controller, affected_card);
			trg->untap_status &= ~3;
		}
	}

	return global_enchantment(player, card, event);
}

int card_anaba_ancestor(int player, int card, event_t event){
	/* Anaba Ancestor	|1|R	0x200a3e0
	 * Creature - Minotaur Spirit 1/1
	 * |T: Another target Minotaur creature gets +1/+1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_MINOTAUR;
	td.special = TARGET_SPECIAL_NOT_ME;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT,
									MANACOST0, 0, &td, "Select another target Minotaur creature.");
}

/* Anaba Bodyguard	|3|R	=>vanilla
 * Creature - Minotaur 2/3
 * First strike */

int card_anaba_shaman(int player, int card, event_t event){
	/* Anaba Shaman	|3|R	0x200a3e5
	 * Creature - Minotaur Shaman 2/2
	 * |R, |T: ~ deals 1 damage to any target. */
	 
	 	  // Let AI know that it can damage blockers when considering what to attack with
    if (event == EVENT_CHECK_PUMP && CAN_TAP(player, card) && CAN_ACTIVATE(player, card, MANACOST_R(1)))
	{
		pumpable_toughness[1 - player] -= 1;
	}
	
	if (event == EVENT_SHOULD_AI_PLAY && player == AI)
	{
	  // Discourage AI from tapping during its own turn if it could tap to deal damage.
	  if (current_turn == player && CAN_TAP(player, card) && CAN_ACTIVATE(player, card, MANACOST_R(1)))
		  ai_modifier -= 24;
	}
	  /* Discourage AI from attacking with this if it could tap to deal damage.  Prodigal Sorcerer also discourages blocking by subtracking 48 from
       ai_defensive_modifier during EVENT_BLOCK_RATING; I'm omitting it for this. */
    if (event == EVENT_ATTACK_RATING && affect_me(player, card) && CAN_ACTIVATE(player, card, MANACOST_R(1)))
	    ai_defensive_modifier += 48;
	
	if (event == EVENT_BLOCK_RATING && affect_me(player, card) && CAN_ACTIVATE(player, card, MANACOST_R(1)))
        ai_defensive_modifier -= 24;

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

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_ANY");
}

int card_anaba_spirit_crafter(int player, int card, event_t event){
	/* Anaba Spirit Crafter	|2|R|R	0x2004bbb
	 * Creature - Minotaur Shaman 1/3
	 * Minotaur creatures get +1/+0. */

	// original code : 0x1201D44
	return boost_creature_type(player, card, event, SUBTYPE_MINOTAUR, 1, 0, 0, BCT_INCLUDE_SELF);
}

int card_apocalypse_chime(int player, int card, event_t event)
{
  /* Apocalypse Chime	|2	0x200d69e
   * Artifact
   * |2, |T, Sacrifice ~: Destroy all nontoken permanents originally printed in the Homelands expansion. They can't be regenerated. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  char bury[2][151] = {{0}};
	  int p, c, csvid;
	  for (p = 0; p <= 1; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if (in_play(p, c) && is_what(p, c, TYPE_PERMANENT) && !is_token(p, c) && (csvid = get_id(p, c))
			  // They're irritatingly scattered.
			  && ((csvid >= CARD_ID_ABBEY_GARGOYLES && csvid <= CARD_ID_WILLOW_PRIESTESS)
				  || (csvid >= CARD_ID_ABBEY_MATRON && csvid <= CARD_ID_WIZARDS_SCHOOL)
				  || csvid == CARD_ID_ANABA_SHAMAN
				  || csvid == CARD_ID_IHSANS_SHADE
				  || csvid == CARD_ID_MEMORY_LAPSE
				  || csvid == CARD_ID_MERCHANT_SCROLL))
			bury[p][c] = 1;
	  APNAP(pl,
			for (c = 0; c < active_cards_count[pl]; ++c)
			  if (bury[pl][c] && in_play(pl, c))
				kill_card(pl, c, KILL_BURY));
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL);
}

int card_autumn_willow(int player, int card, event_t event){
	/* Autumn Willow	|4|G|G	0x200acd2
	 * Legendary Creature - Avatar 4/4
	 * Shroud
	 * |G: Until end of turn, ~ can be the target of spells and abilities controlled by target player as though it didn't have shroud. */

	check_legend_rule(player, card, event);

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *parent = get_card_instance( player, instance->parent_card );
			if( parent->targets[1].player < 0 ){
				parent->targets[1].player = 0;
			}
			parent->targets[1].player |= (1<<instance->targets[0].player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, "TARGET_PLAYER");
}

int card_aysen_abbey(int player, int card, event_t event)
{
  /* Aysen Abbey	""	0x200d685
   * Land
   * |T: Add |1 to your mana pool.
   * |1, |T: Add |W to your mana pool.
   * |2, |T: Add |G or |U to your mana pool. */
  return homelands_triple_land(player, card, event, COLOR_TEST_WHITE, COLOR_TEST_GREEN|COLOR_TEST_BLUE);
}

int card_aysen_bureaucrats(int player, int card, event_t event){
	/* Aysen Bureaucrats	|1|W	0x200a3ea
	 * Creature - Human Advisor 1/1
	 * |T: Tap target creature with power 2 or less. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = 2 | TARGET_PT_LESSER_OR_EQUAL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									 &td, "Select target creature with power 2 or less.");
}

int card_aysen_crusader(int player, int card, event_t event)
{
  /* Aysen Crusader	|2|W|W	0x200a3ef
   * Creature - Human Knight 102/102
   * ~'s power and toughness are each equal to 2 plus the number of Soldiers and Warriors you control. */

  card_instance_t *instance;
  int qualifying_subtype;
  int candidate;
  int characteristic_value;
  if (((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    if (active_cards_count[player] < 1)
    {
      characteristic_value = 2;
    }
    else
    {
      candidate = 0;
      characteristic_value = 0;
      do
      {
        instance = in_play(player, candidate);
        if ((instance != NULL) &&
            ((qualifying_subtype = has_subtype(player, candidate, SUBTYPE_SOLDIER),
              qualifying_subtype != 0 ||
                  (qualifying_subtype = has_subtype(player, candidate, SUBTYPE_WARRIOR),
                   qualifying_subtype != 0))))
        {
          characteristic_value = characteristic_value + 1;
        }
        candidate = candidate + 1;
      } while (candidate < active_cards_count[player]);
      characteristic_value = characteristic_value + 2;
    }
    set_characteristic_pt(characteristic_value);
  }
  return 0;
}

int card_aysen_highway(int player, int card, event_t event)
{
  /* Aysen Highway	|3|W|W|W	0x200d6a3
   * Enchantment
   * |SWhite creatures have |H2plainswalk. */

  if (event == EVENT_ABILITIES && in_play(player, card) && in_play(affected_card_controller, affected_card) && !is_humiliated(player, card, AL_STATIC)
	  && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
	  && (get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_WHITE)))
	event_result |= get_hacked_walk(player, card, KEYWORD_PLAINSWALK);

  return global_enchantment(player, card, event);
}

int card_bakis_curse(int player, int card, event_t event)
{
  /* Baki's Curse	|2|U|U	0x200d6a8
   * Sorcery
   * ~ deals 2 damage to each creature for each Aura attached to that creature. */
  if (event == EVENT_RESOLVE_SPELL)
	{
	  int p, c, dmg[2][151] = {{0}};
	  card_instance_t* aura;
	  /* Count auras attached to each creature, all in one pass.  In particular, don't call count_auras_enchanting_me() repeatedly, since it also loops over all
	   * cards. */
	  for (p = 0; p <= 1; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if ((aura = in_play(p, c)) && is_what(p, c, TYPE_ENCHANTMENT) && has_subtype(p, c, SUBTYPE_AURA)
			  && aura->damage_target_card >= 0 && in_play(aura->damage_target_player, aura->damage_target_card)
			  && is_what(aura->damage_target_player, aura->damage_target_card, TYPE_CREATURE))
			dmg[aura->damage_target_player][aura->damage_target_card] += 2;

	  for (p = 0; p <= 1; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if (dmg[p][c] >= 0 && in_play(p, c) && is_what(p, c, TYPE_CREATURE))
			damage_creature(p, c, dmg[p][c], player, card);

	  kill_card(player, card, KILL_DESTROY);
	}
  return basic_spell(player, card, event);
}

int card_baron_sengir(int player, int card, event_t event)
{
  /* Baron Sengir	|5|B|B|B	0x2002D20
   * Legendary Creature - Vampire 5/5
   * Flying
   * Whenever a creature dealt damage by ~ this turn dies, put a +2/+2 counter on ~.
   * |T: Regenerate another target Vampire. */

  int result;
  card_instance_t *instance;
  target_definition_t *vampire_target;
  target_definition_t td;

  check_legend_rule(player, card, event);
  result = sengir_vampire_trigger(player, card, event, 2);
  if (result != 0)
  {
    instance = get_card_instance(player, card);
    add_counters(player, card, player, card, COUNTER_P2_P2, instance->targets[0xb].card);
    instance = get_card_instance(player, card);
    instance->targets[0xb].card = 0;
  }
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.required_state = TARGET_STATE_DESTROYED;
    td.required_subtype = 0xcd;
    if (player == 1)
    {
      td.special = TARGET_SPECIAL_NOT_ME | TARGET_SPECIAL_REGENERATION;
      get_card_instance(1, card);
      vampire_target = &td;
      return generic_activated_ability(player, card, event, 0x800007, 0, 0, 0, 0, 0, 0, 0,
                                       vampire_target, "Select target Vampire to regenerate.");
    }
    td.special = TARGET_SPECIAL_NOT_ME;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.special = ((player == 1) ? 0x804000U : 16384U);
  }
  vampire_target = &td;
  td.required_state = TARGET_STATE_DESTROYED;
  td.required_subtype = 0xcd;
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if (((event == EVENT_RESOLVE_ACTIVATION) &&
       (result = valid_target(vampire_target), result != 0)) &&
      (result = can_be_regenerated(instance->targets[0].player, instance->targets[0].card),
       result != 0))
  {
    regenerate_target(instance->targets[0].player, instance->targets[0].card);
  }
  vampire_target = &td;
  return generic_activated_ability(player, card, event, 0x800007, 0, 0, 0, 0, 0, 0, 0,
                                   vampire_target, "Select target Vampire to regenerate.");
}

int card_beast_walkers(int player, int card, event_t event)
{
  /* Beast Walkers	|1|W|W	0x200d6ad
   * Creature - Human Beast Soldier 2/2
   * |G: ~ gains banding until end of turn. */
  return generic_shade(player, card, event, 0, MANACOST_G(1), 0, 0, KEYWORD_BANDING, 0);
}

int card_black_carriage(int player, int card, event_t event)
{
  /* Black Carriage	|3|B|B	0x200a3f4
   * Creature - Horse 4/4
   * Trample
   * ~ doesn't untap during your untap step.
   * Sacrifice a creature: Untap ~. Activate this ability only during your upkeep. */

  does_not_untap(player, card, event);

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (in_play(inst->parent_controller, inst->parent_card))
		untap_card(inst->parent_controller, inst->parent_card);
	}

  int rval = generic_activated_ability(player, card, event,
									   GAA_IN_YOUR_TURN|GAA_ONLY_ON_UPKEEP|GAA_SACRIFICE_CREATURE|(IS_AI(player) ? GAA_NOT_ME_AS_TARGET : 0),
									   MANACOST0, 0, NULL, NULL);

  if (event == EVENT_CAN_ACTIVATE && rval && player == AI && IS_AI(player) && creature_count[player] >= 2 && is_tapped(player, card))
	g_activation_event_flags |= 3;	// Force AI to activate. (Same as allowing it to.)

  return rval;
}

int card_broken_visage(int player, int card, event_t event){
	/* Broken Visage	|4|B	0x200a3f9
	 * Instant
	 * Destroy target nonartifact attacking creature. It can't be regenerated. Put a |Sblack Spirit creature token with that creature's power and toughness onto the battlefield. Sacrifice the token at the beginning of the next end step. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_state = TARGET_STATE_ATTACKING;
	td.illegal_type = TYPE_ARTIFACT;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SPIRIT, &token);
			token.pow = get_power(instance->targets[0].player, instance->targets[0].card);
			token.tou = get_toughness(instance->targets[0].player, instance->targets[0].card);
			token.color_forced = COLOR_TEST_BLACK;
			token.special_infos = 67;
			generate_token(&token);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		kill_card(player, card, KILL_SACRIFICE);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nonartifact attacking creature.",
						1, NULL);
}

int base_carapace(int player, int card, event_t event, int pow, int tgh)
{
  if (IS_GAA_EVENT(event))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int p = in_play(player, card) ? instance->damage_target_player : -1;
	  int c = instance->damage_target_card;
	  if (event == EVENT_RESOLVE_ACTIVATION && p >= 0 && can_be_regenerated(p, c))
		regenerate_target(p, c);

	  if (event == EVENT_CAN_ACTIVATE
		  && (p < 0
			  || !(land_can_be_played & LCBP_REGENERATION)
			  || !can_be_regenerated(p, c)))
		return 0;

	  // Not GAA_REGENERATION, that checks if {player,card} can regenerate *itself*
	  return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL) ? 99 : 0;
	}

  return generic_aura(player, card, event, player, pow,tgh, 0,0, 0, 0, 0);
}

int card_carapace(int player, int card, event_t event)
{
  /* Carapace	|G	0x200d6b2
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +0/+2.
   * Sacrifice ~: Regenerate enchanted creature. */
  return base_carapace(player, card, event, 0,2);
}

int card_castle_sengir(int player, int card, event_t event)
{
  /* Castle Sengir	""	0x200d68a
   * Land
   * |T: Add |1 to your mana pool.
   * |1, |T: Add |B to your mana pool.
   * |2, |T: Add |U or |R to your mana pool. */
  return homelands_triple_land(player, card, event, COLOR_TEST_BLACK, COLOR_TEST_BLUE|COLOR_TEST_RED);
}

/* Cemetery Gate	|2|B	=>vanilla
 * Creature - Wall 0/5
 * Defender
 * Protection from |Sblack */

/* ManalinkEh 2022: 0x21233c0; original cards/homelands.c:900. */
int card_chain_stasis(int player, int card, event_t event)
{
  /* Chain Stasis	|U	0x000000
   * Instant
   * You may tap or untap target creature. Then that creature's controller may pay |2|U. If the player does, he or she may copy this spell and may choose a new target for that copy. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    result = generic_spell(player, card, EVENT_CAN_CAST, 0x81, &td,
                           "Select target creature to tap or untap.", 1, NULL);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                      "Select target creature to tap or untap.", 1, NULL);
        if (spell_fizzled != 1)
        {
          ai_modifier_twiddle(player, card, 0);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    if (event == EVENT_RESOLVE_SPELL)
    {

      instance = get_card_instance(player, card);
      result = valid_target(&td);
      if (result != 0)
      {
        twiddle(player, card, 0);
        result = has_mana_multi(instance->targets[0].player, 2, 0, 1, 0, 0, 0);
        if (result != 0)
        {
          result = can_legally_play_iid(instance->targets[0].player, instance->internal_card_id);
          if (result != 0)
          {
            result = do_dialog(instance->targets[0].player, player, card, -1, -1,
                               " Copy Chain Stasis\n Pass", 1);
            if (result == 0)
            {
              charge_mana_multi(instance->targets[0].player, 2, 0, 1, 0, 0, 0);
              if (spell_fizzled != 1)
              {
                result = get_id(player, card);
                copy_spell(instance->targets[0].player, result);
              }
            }
          }
        }
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 2);
  }
  return 0;
}

int card_chandler(int player, int card, event_t event){
	/* Chandler	|4|R	0x200a3fe
	 * Legendary Creature - Human Rogue 3/3
	 * |R|R|R, |T: Destroy target artifact creature. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.special = TARGET_SPECIAL_ARTIFACT_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_LITERAL_PROMPT | GAA_CAN_TARGET, MANACOST_R(3), 0,
									&td, "Select target artifact creature.");
}

int card_clockwork_gnomes(int player, int card, event_t event)
{
  /* Clockwork Gnomes	|4	0x200d6b7
   * Artifact Creature - Gnome 2/2
   * |3, |T: Regenerate target artifact creature. */

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;
  td.required_state = TARGET_STATE_DESTROYED;
  td.special = TARGET_SPECIAL_ARTIFACT_CREATURE;
  if (player == AI)
	td.special |= TARGET_SPECIAL_REGENERATION;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (can_be_regenerated(inst->targets[0].player, inst->targets[0].card))
		regenerate_target(inst->targets[0].player, inst->targets[0].card);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_ARTIFACTCREATURE") ? 99 : 0;
}

int card_clockwork_steed(int player, int card, event_t event){
	/* Clockwork Steed	|4	0x200a403
	 * Artifact Creature - Horse 0/3
	 * ~ enters the battlefield with four +1/+0 counters on it.
	 * ~ can't be blocked by artifact creatures.
	 * At end of combat, if ~ attacked or blocked this combat, remove a +1/+0 counter from it.
	 * |X, |T: Put up to X +1/+0 counters on ~. This ability can't cause the total number of +1/+0 counters on ~ to be greater than four. Activate this ability only during your upkeep. */

	if(event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card, AL_STATIC) ){
		if( player == attacking_card_controller && card == attacking_card ){
			if( is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) ){
				event_result = 1;
			}
		}
	}
	return hom_clockwork(player, card, event, 4);
}

int card_clockwork_swarm(int player, int card, event_t event){
	/* Clockwork Swarm	|4	0x200a408
	 * Artifact Creature - Insect 0/3
	 * ~ enters the battlefield with four +1/+0 counters on it.
	 * ~ can't be blocked by Walls.
	 * At end of combat, if ~ attacked or blocked this combat, remove a +1/+0 counter from it.
	 * |X, |T: Put up to X +1/+0 counters on ~. This ability can't cause the total number of +1/+0 counters on ~ to be greater than four. Activate this ability only during your upkeep. */

	if(event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card, AL_STATIC) ){
		if( player == attacking_card_controller && card == attacking_card ){
			if( has_subtype(affected_card_controller, affected_card, SUBTYPE_WALL) ){
				event_result = 1;
			}
		}
	}
	return hom_clockwork(player, card, event, 4);
}

int card_coral_reef(int player, int card, event_t event)
{
  /* Coral Reef	|U|U	0x200d6bc
   * Enchantment
   * ~ enters the battlefield with four polyp counters on it.
   * Sacrifice |Han Island: Put two polyp counters on ~.
   * |U, Tap an untapped |Sblue creature you control, Remove a polyp counter from ~: Put a +0/+1 counter on target creature. */

  enum
  {
    CHOICE_POLYP = 1,
    CHOICE_01 = 2
  };

  int mode_counter_or_result;
  int sacrifice_or_target_card;
  int targets_available;
  card_instance_t *reef;
  const char *prompt;
  unsigned int counter_transfer_available;
  card_instance_t *target_in_play;
  target_definition_t td_blue;
  target_definition_t td_creature;
  enters_the_battlefield_with_counters(player, card, event, COUNTER_POLYP, 4);
  if (((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2) || (event == EVENT_ACTIVATE))
  {
    if ((event == EVENT_CAN_ACTIVATE) &&
        (mode_counter_or_result = can_use_activated_abilities(player, card),
         mode_counter_or_result == 0))
    {
      return 0;
    }
    base_target_definition(player, card, &td_blue, 2);
    td_blue.allowed_controller = player;
    td_blue.preferred_controller = player;
    td_blue.required_color = get_sleighted_color_test(player, card, 4);
    td_blue.illegal_state = TARGET_STATE_TAPPED;
    default_target_definition(player, card, &td_creature, 2);
    td_creature.preferred_controller = player;
    mode_counter_or_result = count_counters(player, card, COUNTER_POLYP);
    sacrifice_or_target_card = can_sacrifice_as_cost(player, 1, 1, 0, 0x5002, 0, 0, 0, 0, 0, -1, 0);
    targets_available = can_target(&td_blue);
    counter_transfer_available = 0;
    if (targets_available != 0)
    {
      targets_available = can_target(&td_creature);
      counter_transfer_available =
          (unsigned int)(targets_available != 0 && 0 < mode_counter_or_result);
    }
    mode_counter_or_result =
        dialog_impl(player, card, event, DLG_RANDOM, "Add Polyp counter", sacrifice_or_target_card,
                    (unsigned int)((mode_counter_or_result == 0) ? 1 : -1), "Add +0/+1 counter",
                    counter_transfer_available, 9, DLG_MANA6(0, 0, 1, 0, 0, 0), 0, 0, 0, 0);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      reef = get_card_instance(player, card);
      if (mode_counter_or_result == CHOICE_POLYP)
      {
        target_in_play = in_play(reef->parent_controller, reef->parent_card);
        if (target_in_play != NULL)
        {
          add_counters(player, card, reef->parent_controller, reef->parent_card, COUNTER_POLYP, 2);
        }
      }
      else if ((mode_counter_or_result == CHOICE_01) &&
               (target_in_play = in_play(reef->targets[0].player, reef->targets[0].card),
                target_in_play != NULL))
      {
        add_counters(player, card, reef->targets[0].player, reef->targets[0].card, COUNTER_P0_P1,
                     1);
      }
    }
    else
    {
      if (event == EVENT_CAN_ACTIVATE)
      {
        return mode_counter_or_result;
      }
      if (event == EVENT_ACTIVATE)
      {
        if (mode_counter_or_result == CHOICE_POLYP)
        {
          mode_counter_or_result = pick_special_permanent_for_sacrifice(
              player, card, 0, 1, 0, 0x5002, 0, 0, 0, 0, 0, -1, 0);
          if (mode_counter_or_result == -1)
          {
            spell_fizzled = 1;
          }
          else
          {
            kill_card(player, mode_counter_or_result, KILL_SACRIFICE);
          }
        }
        else if (mode_counter_or_result == CHOICE_01)
        {
          reef = get_card_instance(player, card);
          reef->number_of_targets = '\0';
          prompt = get_sleighted_color_text(player, card,
                                            "Select an untapped %s creature you control.", 2);
          if (new_pick_target(&td_blue, prompt, 0, 0x81) == 0)
          {
            return 0;
          }
          mode_counter_or_result = reef->targets[0].player;
          sacrifice_or_target_card = reef->targets[0].card;
          reef->number_of_targets = '\0';
          if (new_pick_target(&td_creature, "TARGET_CREATURE", 0, 1) == 0)
          {
            return 0;
          }
          tap_card(mode_counter_or_result, sacrifice_or_target_card);
          remove_counters(player, card, player, card, COUNTER_POLYP, 1);
        }
      }
    }
  }
  return global_enchantment(player, card, event);
}

int fx_dark_maze(int player, int card, event_t event)
{
  if (event == EVENT_ABILITIES)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (!affect_me(inst->damage_target_player, inst->damage_target_card))
		return 0;
	  add_status(affected_card_controller, affected_card, STATUS_WALL_CAN_ATTACK);
	}
  if (eot_trigger(player, card, event))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  kill_card(inst->damage_target_player, inst->damage_target_card, KILL_EXILE);
	}
  if (trigger_condition == TRIGGER_EOT && event == EVENT_END_TRIGGER && player == reason_for_trigger_controller)
	kill_card(player, card, KILL_EXILE);	// presumeably the trigger was countered; don't run it again next turn
  return 0;
}

int card_dark_maze(int player, int card, event_t event)
{
  /* Dark Maze	|4|U	0x200d6c1
   * Creature - Wall 4/5
   * Defender
   * |0: ~ can attack this turn as though it didn't have defender. Exile it at the beginning of the next end step. */

  if (!IS_GAA_EVENT(event))
	return 0;

  card_instance_t* inst = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION && in_play(inst->parent_controller, inst->parent_card))
	create_targetted_legacy_effect(inst->parent_controller, inst->parent_card, &fx_dark_maze,
								   inst->parent_controller, inst->parent_card);

  if (event == EVENT_ACTIVATE && IS_AI(player)
	  && ((inst->token_status & STATUS_WALL_CAN_ATTACK)	// once is enough, thanks
		  || has_activation_on_stack(player, card)		// no, really, stop repeating yourself
		  || !(inst->regen_status & KEYWORD_DEFENDER)))	// no need
	{
	  if (ai_is_speculating == 1)
		cancel = 1;	// don't consider this branch
	  else
		ai_modifier -= 48;
	}

  return generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2120600; original cards/homelands.c:1177. */
static char *damage_dealt_to_white_creature(int who_chooses, int player, int card,
                                            int target_player, int target_card)
{
  card_instance_t *instance;
  char *requirement_error;
  int result;
  unsigned int damaged_colors;
  unsigned int required_color;

  instance = get_card_instance(player, card);
  if ((instance->internal_card_id == damage_card) && (-1 < instance->damage_target_card))
  {
    result = is_what((int)instance->damage_target_player, instance->damage_target_card, 2);
    if (result != 0)
    {
      damaged_colors = get_color((int)instance->damage_target_player, instance->damage_target_card);
      required_color = get_sleighted_color_test(target_player, target_card, 0x20);
      if ((damaged_colors & required_color) != 0)
      {
        return NULL;
      }
    }
  }
  requirement_error = (char *)get_sleighted_color_text(
      target_player, target_card, "Must be damage card dealt to target %s creature.", 5);
  return requirement_error;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2124160; original cards/homelands.c:1189. */
int card_daughter_of_autumn(int player, int card, event_t event)
{
  /* Daughter of Autumn	|2|G|G	0x000000	[See also Hazduhr the Abbot below]
   * Legendary Creature - Avatar 2/4
   * |W: The next 1 damage that would be dealt to target |Swhite creature this turn is dealt to ~ instead. */

  card_instance_t *instance;
  const char *prompt;
  int mana_toughness_or_player;
  int redirect_amount_or_valid;
  card_instance_t *damage;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (int32_t)damage_dealt_to_white_creature;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x1000004, 0, 0, 0, 0, 0,
                                       1, 0, &td, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      prompt = get_sleighted_color_text(player, card,
                                        "Select damage card dealt to target %s creature.", 5);
      generic_activated_ability(player, card, EVENT_ACTIVATE, 6, 0, 0, 0, 0, 0, 1, 0, &td,
                                (char *)prompt);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      damage = get_card_instance(instance->targets[0].player, instance->targets[0].card);
      mana_toughness_or_player = has_mana(player, 5, 1);
      redirect_amount_or_valid = damage->info_slot;
      if (mana_toughness_or_player < damage->info_slot)
      {
        redirect_amount_or_valid = mana_toughness_or_player;
      }
      mana_toughness_or_player = instance->targets[1].player;
      if (mana_toughness_or_player == player)
      {
        mana_toughness_or_player = get_toughness(player, instance->targets[1].card);
        if (mana_toughness_or_player < redirect_amount_or_valid)
        {
          mana_toughness_or_player = instance->targets[1].player;
          ai_modifier = ai_modifier + 0x18;
        }
        else
        {
          ai_modifier = ai_modifier + -0x30;
          mana_toughness_or_player = instance->targets[1].player;
        }
      }
      if (1 - player != mana_toughness_or_player)
      {
        return 0;
      }
      mana_toughness_or_player = get_toughness(mana_toughness_or_player, instance->targets[1].card);
      if (mana_toughness_or_player <= redirect_amount_or_valid)
      {
        ai_modifier = ai_modifier + 0x30;
        return 0;
      }
      ai_modifier = ai_modifier + 0x18;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (int32_t)damage_dealt_to_white_creature;
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (redirect_amount_or_valid = validate_target(player, card, &td, 0),
       redirect_amount_or_valid != 0))
  {
    damage = get_card_instance(instance->targets[0].player, instance->targets[0].card);
    prevent_the_next_n_damage(instance->parent_controller, instance->parent_card,
                              (int)damage->damage_target_player, damage->damage_target_card, 1,
                              PREVENT_REDIRECT, instance->parent_controller, instance->parent_card);
    return 0;
  }
  return 0;
}
#endif

/* Death Speakers	|W	=>vanilla
 * Creature - Human Cleric 1/1
 * Protection from |Sblack */

int card_didgeridoo(int player, int card, event_t event){
	/* Didgeridoo	|1	0x2002D25
	 * Artifact
	 * |3: You may put a Minotaur permanent card from your hand onto the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}
	test_definition_t this_test2;
	new_default_test_definition(&this_test2, TYPE_PERMANENT, "Select a Minotaur permanent card.");
	this_test2.subtype = SUBTYPE_MINOTAUR;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test2);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_X(3), 0, NULL, NULL);
}

int card_drudge_spell(int player, int card, event_t event){
/* Drudge Spell	|B|B	0x2002D2A
 * Enchantment
 * |B, Exile two creature cards from your graveyard: Put a 1/1 |Sblack Skeleton creature token onto the battlefield.
		It has "|B: Regenerate this creature."
 * When ~ leaves the battlefield, destroy all Skeleton tokens. They can't be regenerated. */

	leaves_play(player, card, event, 1, 1);
	if (event == EVENT_LEAVES_PLAY_ABILITY){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_SKELETON;
		test.type_flag = F1_IS_TOKEN;
		new_manipulate_all(player, card, ANYBODY, &test, KILL_BURY);
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL) ){
			return count_graveyard_by_type(player, TYPE_CREATURE) > 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_B(1)) ){
			test_definition_t this_test2;
			new_default_test_definition(&this_test2, TYPE_CREATURE, "Select a creature card to exile.");
			if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test2) >= 0 ){
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 1, AI_MIN_VALUE, &this_test2);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SKELETON);
	}

	return 0;
}

int card_dry_spell(int player, int card, event_t event){
	/* Dry Spell	|1|B	0x200a412
	 * Sorcery
	 * ~ deals 1 damage to each creature and each player. */

	if( event == EVENT_RESOLVE_SPELL ){
		new_damage_all(player, card, ANYBODY, 1, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_dwarven_pony(int player, int card, event_t event)
{
	/* Dwarven Pony	|R	0x200b51f
	 * Creature - Horse 1/1
	 * |1|R, |T: Target Dwarf creature gains |H2mountainwalk until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.required_subtype = SUBTYPE_DWARF;
  td.preferred_controller = player;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							 0, 0, get_hacked_walk(player, card, KEYWORD_MOUNTAINWALK), 0);
	}

  return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_UNTAPPED, MANACOST_XR(1, 1), 0, &td, "TARGET_DWARF_CREATURE");
}

static const char*
target_controls_island(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  return (basiclandtypes_controlled[player][get_hacked_color(targeting_player, targeting_card, COLOR_BLUE)] >= 1
		  ? NULL : get_hacked_land_text(targeting_player, targeting_card, "controls %a", SUBTYPE_ISLAND));
}

static int
fx_dwarven_sea_clan(int player, int card, event_t event)
{ 
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.required_state = TARGET_STATE_IN_COMBAT;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (int)target_controls_island;
  if ((trigger_condition == TRIGGER_END_COMBAT || event == EVENT_SHOULD_AI_PLAY) && affect_me(player, card) && valid_target(&td))
	{
	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_MANDATORY;

	  if (event == EVENT_RESOLVE_TRIGGER || event == EVENT_SHOULD_AI_PLAY)
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  if (inst->damage_target_player != -1)
			damage_creature(inst->damage_target_player, inst->damage_target_card, 2,
							inst->damage_source_player, inst->damage_source_card);
		  kill_card(player, card, KILL_EXILE);
		}
	}

  if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);
  return 0;
}

int card_dwarven_sea_clan(int player, int card, event_t event)
{
  /* Dwarven Sea Clan	|2|R	0x200d6c6
   * Creature - Dwarf 1/1
   * |T: Choose target attacking or blocking creature whose controller controls |Han Island. ~ deals 2 damage to that creature at end of combat. Activate this ability only before the end of combat step. */
  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.required_state = TARGET_STATE_IN_COMBAT;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = target_predicate_extra(target_controls_island);

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  if(valid_target(&td)){
	  card_instance_t* inst = get_card_instance(player, card);
	  create_targetted_legacy_effect(inst->parent_controller, inst->parent_card, &fx_dwarven_sea_clan,
									 inst->targets[0].player, inst->targets[0].card);
	  }
	}

  return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_UNTAPPED, MANACOST0, 0, &td, "TARGET_ATTACKING_BLOCKING_CREATURE");
}

/* Dwarven Trader	|R	=>vanilla
 * Creature - Dwarf 1/1 */

/* Ebony Rhino	|7	=>vanilla
 * Artifact Creature - Rhino 4/5
 * Trample */

int card_eron_the_relentless(int player, int card, event_t event){
	/* Eron the Relentless	|3|R|R	0x200a417
	 * Legendary Creature - Human Rogue 5/2
	 * Haste
	 * |R|R|R: Regenerate ~. */

	check_legend_rule(player, card, event);
	haste(player, card, event);
	return regeneration(player, card, event, MANACOST_R(3));
}

int card_evaporate(int player, int card, event_t event)
{
  /* Evaporate	|2|R	0x200d6cb
   * Sorcery
   * ~ deals 1 damage to each |Swhite and/or |Sblue creature. */
	if (event == EVENT_RESOLVE_SPELL)
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE|COLOR_TEST_WHITE);
		new_damage_all(player, card, ANYBODY, 1, 0, &test);
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

int card_faerie_noble(int player, int card, event_t event)
{
	/* Faerie Noble	|2|G	0x200a41c
	 * Creature - Faerie 1/2
	 * Flying
	 * Other Faerie creatures you control get +0/+1.
	 * |T: Other Faerie creatures you control get +1/+0 until end of turn. */

  // Other Faerie creatures you control get +0/+1.
  boost_creature_type(player, card, event, SUBTYPE_FAERIE, 0, 1, 0, BCT_CONTROLLER_ONLY);

  // |T: Other Faerie creatures you control get +1/+0 until end of turn.
  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.not_me = 1;
	  test.subtype = SUBTYPE_FAERIE;

	  card_instance_t* instance = get_card_instance(player, card);
	  pump_creatures_until_eot(instance->parent_controller, instance->parent_card, player, 0, 1,0, 0,0, &test);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(0), 0, NULL, NULL);
}

int card_feast_of_the_unicorn(int player, int card, event_t event){
	/* Feast of the Unicorn	|3|B	0x200a421
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature gets +4/+0. */

	return generic_aura(player, card, event, player, 4, 0, 0, 0, 0, 0, 0);
}

int card_ferozs_ban(int player, int card, event_t event){
	/* Feroz's Ban	|6	0x200a426
	 * Artifact
	 * Creature spells cost |2 more to cast. */

	if( event == EVENT_MODIFY_COST_GLOBAL && is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
		in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) )
	{
		COST_COLORLESS+=2;
	}
	return 0;
}

int card_folk_of_an_havva(int player, int card, event_t event)
{
  /* Folk of An-Havva	|G	0x200d6d0
   * Creature - Human 1/1
   * Whenever ~ blocks, it gets +2/+0 until end of turn. */
  if (event == EVENT_POW_BOOST && current_turn != player && current_phase <= PHASE_DECLARE_BLOCKERS)
	event_result += 2;
  if (blocking(player, card, event) && !is_humiliated(player, card, AL_TRIG))
	pump_until_eot(player, card, player, card, 2, 0);
  return 0;
}

int card_forget(int player, int card, event_t event){
	/* Forget	|U|U	0x200a42b
	 * Sorcery
	 * Target player discards two cards, then draws as many cards as he or she discarded this way. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = MIN(2, hand_count[instance->targets[0].player]);
			if( amount ){
				new_multidiscard(instance->targets[0].player, amount, 0, player);
				draw_cards(instance->targets[0].player, amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_funeral_march(int player, int card, event_t event)
{
  /* Funeral March	|1|B|B	0x200d6da
   * Enchantment - Aura
   * Enchant creature
   * When enchanted creature leaves the battlefield, its controller sacrifices a creature. */

   if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t* inst = get_card_instance(player, card);
		other_leaves_play(player, card, inst->damage_target_player, inst->damage_target_card, event, 1, 1);
		if (event == EVENT_LEAVES_PLAY_ABILITY)
		{
			card_instance_t* ench = in_play(inst->damage_target_player, inst->damage_target_card);
			if (ench){
				ench->state |= STATE_CANNOT_TARGET;	// Since TRIGGER_LEAVE_PLAY is, in most cases, sent with that card still on the battlefield
			}
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			new_sacrifice(player, card, inst->damage_target_player, SAC_NO_CANCEL | SAC_CAUSED, &test);
		}
	}
	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, 1-player);
}

static void first_strike_if_white(int player, int card, int t_player, int t_card)
{
  if (get_color(t_player, t_card) & get_sleighted_color_test(player, card, COLOR_TEST_WHITE))
	pump_ability_until_eot(player, card, player, card, 0,0, KEYWORD_FIRST_STRIKE,0);
}

int card_ghost_hounds(int player, int card, event_t event)
{
  /* Ghost Hounds	|1|B	0x200d6df
   * Creature - Hound Spirit 1/1
   * Vigilance
   * Whenever ~ blocks or becomes blocked by a |Swhite creature, ~ gains first strike until end of turn. */

	vigilance(player, card, event);

	if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card, AL_TRIG))
	{
		card_instance_t* inst = get_card_instance(player, card);

		if (inst->state & STATE_ATTACKING)
			for_each_creature_blocking_me(player, card, first_strike_if_white, player, card);

		if (inst->state & STATE_BLOCKING)
			for_each_creature_blocked_by_me(player, card, first_strike_if_white, player, card);
	}

	return 0;
}

static int giant_albatross_legacy(int player, int card, event_t event)
{

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (affect_me(instance->targets[0].player, instance->targets[0].card) &&
        is_this_dying(instance->targets[0].player, instance->targets[0].card) &&
        !check_for_death_trigger_removal(instance->targets[0].player, instance->targets[0].card))
    {
      instance->targets[11].player = 1;
    }
  }

  if (resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int choice = 1;
    if (can_pay_life(instance->damage_target_player, 2))
    {
      choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                      DLG_WHO_CHOOSES(instance->damage_target_player), "Bury this creature", 1,
                      life[player] <= 6 ? 10 : 1, "Pay 2 life", life[player] > 6 ? 10 : 1);
    }
    if (choice == 1)
    {
      kill_card(instance->damage_target_player, instance->damage_target_card, KILL_BURY);
    }
    if (choice == 2)
    {
      lose_life(instance->damage_target_player, 2);
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_giant_albatross(int player, int card, event_t event)
{
/* Giant Albatross	|1|U	0x200f520
 * Creature - Bird 1/1
 * Flying
 * When ~ dies, you may pay |1|U. If you do, for each creature that dealt damage to ~ this turn, destroy that creature unless
	its controller pays 2 life. A creature destroyed this way can't be regenerated. */

	if( event == EVENT_DEAL_DAMAGE ){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == card && damage->damage_target_player == player && damage->info_slot > 0 ){
				if( instance->info_slot < 0 ){
					instance->info_slot = 0;
				}
				if( instance->info_slot < 10 ){
					instance->targets[instance->info_slot].player = damage->damage_source_player;
					instance->targets[instance->info_slot].card = damage->damage_source_card;
					instance->info_slot++;
				}
			}
		}
	}

	if( get_card_instance(player, card)->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
		reason_for_trigger_controller == player )
	{
		card_instance_t *instance = get_card_instance(player, card);
		int i;
		for(i=0; i<instance->info_slot; i++){
			int legacy = create_targetted_legacy_effect(player, card, &giant_albatross_legacy,
														instance->targets[i].player, instance->targets[i].card);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[0].player = player;
			leg->targets[0].card = card;
			leg->number_of_targets = 1;
		}
		instance->info_slot = 0;
	}

	if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG))
		get_card_instance(player, card)->destroys_if_blocked |= DIFB_DESTROYS_ALL;

	return 0;
}

static int giant_oyster_effect(int player, int card, event_t event)
{
  int t_card;
  int argument2;
  card_instance_t *instance;
  int tapped;
  int source_player;
  int t_player;
  instance = get_card_instance(player, card);
  if (instance->damage_target_player < '\0')
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  t_player = (int)instance->damage_target_player;
  t_card = instance->damage_target_card;
  source_player = instance->targets[0].player;
  argument2 = instance->targets[0].card;
  bool detach = false;
  if (event == EVENT_DRAW_PHASE)
  {
    if ((source_player == current_turn) &&
        (tapped = is_tapped(source_player, argument2), tapped != 0))
    {
      add_counters(player, card, t_player, t_card, COUNTER_M1_M1, 1);
    }
    does_not_untap(t_player, t_card, EVENT_DRAW_PHASE);
    other_leaves_play(player, card, source_player, argument2, EVENT_DRAW_PHASE, 1, 1);
    if (is_tapped(source_player, argument2) != 0)
    {
      return 0;
    }
    if (argument2 != affected_card)
    {
      return 0;
    }
    if (source_player == affected_card_controller)
      detach = true;
  }
  else
  {
    does_not_untap(t_player, t_card, event);
    other_leaves_play(player, card, source_player, argument2, event, 1, 1);
    tapped = is_tapped(source_player, argument2);
    if ((tapped == 0) && (argument2 == affected_card))
      detach = source_player == affected_card_controller;
  }
  if (!detach && event != EVENT_LEAVES_PLAY_ABILITY)
  {
    return 0;
  }
  remove_counters(player, card, t_player, t_card, COUNTER_M1_M1,
                  count_counters(t_player, t_card, COUNTER_M1_M1));
  kill_card(player, card, KILL_EXILE);
  return 0;
}

int card_giant_oyster(int player, int card, event_t event){
/* Giant Oyster	|2|U|U	0x000000:200a430
 * Creature - Oyster 0/3
 * You may choose not to untap ~ during your untap step.
 * |T: For as long as ~ remains tapped, target tapped creature doesn't untap during its controller's untap step,
		and at the beginning of each of your draw steps, put a -1/-1 counter on that creature.
 * When ~ leaves the battlefield or becomes untapped, remove all -1/-1 counters from the creature. */

	choose_to_untap(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int legacy = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &giant_oyster_effect,
														instance->targets[0].player, instance->targets[0].card);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[0].player = instance->parent_controller;
			leg->targets[0].card = instance->parent_card;
			leg->number_of_targets = 1;
			card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
			parent->targets[1] = instance->targets[0];
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target tapped creature.");
}

int card_grandmother_sengir(int player, int card, event_t event){
	/* Grandmother Sengir	|4|B	0x2002D2F
	 * Legendary Creature - Human Wizard 3/3
	 * |1|B, |T: Target creature gets -1/-1 until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							-1, -1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XB(1, 1), 0, &td, "TARGET_CREATURE");
}

static int greater_werewolf_legacy(int player, int card, event_t event){

	if( end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance(player, card);
		add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_M0_M2, 1);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

static void attach_greater_werewolf_legacy(int player, int card, int t_player, int t_card)
{
	create_targetted_legacy_effect(player, card, &greater_werewolf_legacy, t_player, t_card);
}

int card_greater_werewolf(int player, int card, event_t event){
	/* Greater Werewolf	|4|B	0x2002D34
	 * Creature - Werewolf 2/4
	 * At end of combat, put a -0/-2 counter on each creature blocking or blocked by ~. */

	if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card, AL_TRIG)){
		card_instance_t *instance = get_card_instance( player, card);
		if( player == current_turn && (instance->state & STATE_ATTACKING) ){
			for_each_creature_blocking_me(player, card, attach_greater_werewolf_legacy, player, card);
		}
		if( player == 1-current_turn && (instance->state & STATE_BLOCKING) ){
			for_each_creature_blocked_by_me(player, card, attach_greater_werewolf_legacy, player, card);
		}
	}

	return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2125a30; original cards/homelands.c:1809. */
int card_hazduhr_the_abbot(int player, int card, event_t event)
{
  /* Hazduhr the Abbot	|3|W|W	0x000000	[see also Daughter of Autumn above]
   * Legendary Creature - Human Cleric 2/5
   * |X, |T: The next X damage that would be dealt this turn to target |Swhite creature you control is dealt to ~ instead. */

  card_instance_t *instance;
  int result;
  card_instance_t *damage;
  const char *prompt;
  int32_t argument3;
  target_definition_t *damage_target;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (int32_t)damage_dealt_to_white_creature;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x1000005, 0, 0, 0, 0, 0,
                                         0, 0, &td, NULL);
      if (result == 0)
      {
        return 0;
      }
      return 99;
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->info_slot = 0;
      result = charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }

      prompt = get_sleighted_color_text(player, card,
                                        "Select damage card dealt to target %s creature.", 5);
      new_pick_target(&td, prompt, 0, 0x80);
      damage = get_card_instance(instance->targets[0].player, instance->targets[0].card);
      argument3 = damage->info_slot;
      if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
      {
        while (result = has_mana(player, 0, argument3), result == 0)
        {
          argument3 = argument3 + -1;
        }
        charge_mana(player, COLOR_COLORLESS, argument3);
        if (spell_fizzled != 1)
        {
          instance->info_slot = argument3;
        }
      }
      else
      {
        charge_mana(player, COLOR_COLORLESS, -1);
        if (spell_fizzled != 1)
        {
          instance->info_slot = x_value;
          add_state(player, card, 0x10);
          tapped_for_mana_color = -1;
        }
      }
      result = instance->targets[1].player;
      if (result == player)
      {
        result = get_toughness(player, instance->targets[1].card);
        if (result < argument3)
        {
          result = instance->targets[1].player;
          ai_modifier = ai_modifier + 0x18;
        }
        else
        {
          ai_modifier = ai_modifier + -0x30;
          result = instance->targets[1].player;
        }
      }
      if (1 - player != result)
      {
        return 0;
      }
      result = get_toughness(result, instance->targets[1].card);
      if (result <= argument3)
      {
        ai_modifier = ai_modifier + 0x30;
        return 0;
      }
      ai_modifier = ai_modifier + 0x18;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (int32_t)damage_dealt_to_white_creature;
    instance = get_card_instance(player, card);
  }
  damage_target = &td;
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = validate_target(player, card, damage_target, 0), result != 0))
  {

    damage = get_card_instance(instance->targets[0].player, instance->targets[0].card);
    prevent_the_next_n_damage(instance->parent_controller, instance->parent_card,
                              (int)damage->damage_target_player, damage->damage_target_card,
                              instance->info_slot, PREVENT_REDIRECT, instance->parent_controller,
                              instance->parent_card);
    return 0;
  }
  return 0;
}
#endif

int card_headstone(int player, int card, event_t event)
{
  /* Headstone	|1|B	0x200d6e4
   * Instant
   * Exile target card from a graveyard.
   * Draw a card at the beginning of the next turn's upkeep. */

	if (!IS_GS_EVENT(player, card, event))
		return 0;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select target card to exile.");

	if( event == EVENT_RESOLVE_SPELL 	)
	{
		card_instance_t* inst = get_card_instance(player, card);
		int selected = validate_target_from_grave_source(player, card, inst->targets[0].player, 1);
		if (selected != -1)
		{
			rfg_card_from_grave(inst->targets[0].player, selected);
			cantrip(player, card, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &test);
}

int card_heart_wolf(int player, int card, event_t event)
{
	/* Heart Wolf	|3|R	0x200b524
	 * Creature - Wolf 2/2
	 * First strike
	 * |T: Target Dwarf creature gets +2/+0 and gains first strike until end of turn.
		When that creature leaves the battlefield this turn, sacrifice ~. Activate this ability only during combat. */

	if (event == EVENT_CHECK_ABILITIES){
		if (affected_card_controller == player && has_subtype(player, affected_card, SUBTYPE_DWARF) && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL)){
			check_abilities_keywords |= KEYWORD_FIRST_STRIKE;
			check_abilities_power += 2;
		}
		return 0;
	}
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_DWARF;

	if (event == EVENT_RESOLVE_ACTIVATION)
	{
		if (valid_target(&td))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  int leg = pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
										   2, 0, KEYWORD_FIRST_STRIKE, 0);
			if (leg != -1)
			{
			  card_instance_t* legacy = get_card_instance(player, leg);
			  legacy->targets[3].player = PAUE_KILL_SOURCE_IF_LEAVES_BATTLEFIELD | PAUE_END_AT_EOT;
			  legacy->targets[3].card = KILL_SACRIFICE;
			}
		}
	}

	if (event == EVENT_CAN_ACTIVATE && !(current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2))
		return 0;

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_UNTAPPED, MANACOST_X(0), 0, &td, "TARGET_DWARF_CREATURE");
}

int card_hungry_mist(int player, int card, event_t event){
	/* Hungry Mist	|2|G|G	0x200a435
	 * Creature - Elemental 6/2
	 * At the beginning of your upkeep, sacrifice ~ unless you pay |G|G. */

	basic_upkeep(player, card, event, MANACOST_G(2));
	return 0;
}

int card_ihsans_shade(int player, int card, event_t event){
	/* Ihsan's Shade	|3|B|B|B	0x200accd
	 * Legendary Creature - Shade Knight 5/5
	 * Protection from |Swhite */

	check_legend_rule(player, card, event);
	return 0;
}

int card_irini_sengir(int player, int card, event_t event)
{
  /* Irini Sengir	|2|B|B	0x200cb4f
   * Legendary Creature - Vampire Dwarf 2/2
   * |SGreen enchantment spells and |Swhite enchantment spells cost |2 more to cast. */

  if (event == EVENT_MODIFY_COST_GLOBAL)
	{
	  if (is_what(affected_card_controller, affected_card, TYPE_ENCHANTMENT)
		  && (get_color_real_or_iid(event_result, affected_card_controller, affected_card)
			  & get_sleighted_color_test(player, card, COLOR_TEST_WHITE | COLOR_TEST_GREEN))
		  && !is_humiliated(player, card, AL_STATIC))
		COST_COLORLESS += 2;
	  return 0;
	}

  check_legend_rule(player, card, event);
  return 0;
}

int card_ironclaw_curse(int player, int card, event_t event)
{
  /* Ironclaw Curse	|R	0x200d6e9
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets -0/-1.
   * Enchanted creature can't block creatures with power equal to or greater than the enchanted creature's toughness. */

	if (event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card, AL_STATIC) )
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (inst->damage_target_player < 0 || !affect_me(inst->damage_target_player, inst->damage_target_card))
		return 0;
	  if (get_power(attacking_card_controller, attacking_card) >= get_toughness(inst->damage_target_player, inst->damage_target_card))
		event_result = 1;
	}

	return generic_aura(player, card, event, 1-player, 0, -1, 0,0, 0, 0, 0);
}

int fx_change_landtype(int player, int card, event_t event)
{
  // changes to the basic landtype set by the color in targets[1].player.  COLOR_BLACK => SUBTYPE_SWAMP, etc.
  if (event == EVENT_CHANGE_TYPE && !(land_can_be_played & LCBP_DURING_EVENT_CHANGE_TYPE_SECOND_PASS))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (!affect_me(inst->damage_target_player, inst->damage_target_card))
		return 0;
	  event_result = inst->targets[1].player - 1;	// Since iids 0-4 are conveniently Swamp, Island, Forest, Mountain, Plains
	}

  if (event == EVENT_CLEANUP && !(get_card_instance(player, card)->token_status & STATUS_PERMANENT))
	kill_card(player, card, KILL_EXILE);
  return 0;
}

int card_jinx(int player, int card, event_t event)
{
  /* Jinx	|1|U	0x200d6ee
   * Instant
   * Target land becomes the basic land type of your choice until end of turn.
   * Draw a card at the beginning of the next turn's upkeep. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  card_instance_t* inst = get_card_instance(player, card);

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_LAND);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  int color = choose_a_land(player, COLOR_BLACK + recorded_rand(player, COLOR_WHITE - COLOR_BLACK + 1));
		  int leg = create_targetted_legacy_effect(player, card, &fx_change_landtype, inst->targets[0].player, inst->targets[0].card);
		  card_instance_t* legacy = get_card_instance(player, leg);
		  legacy->targets[1].player = color;
		  legacy->eot_toughness = 1;
		  if (get_id(player, card) == CARD_ID_SHIMMERING_MIRAGE){ draw_a_card(player); }
		  else{ get_card_instance(player, cantrip(player, card, 1))->eot_toughness = 2; }
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target land it becomes the basic land type of your choice.", 1, NULL);
}

int card_joven(int player, int card, event_t event){
	/* Joven	|3|R|R	0x200a43a
	 * Legendary Creature - Human Rogue 3/3
	 * |R|R|R, |T: Destroy target noncreature artifact. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.illegal_type = TYPE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_LITERAL_PROMPT | GAA_CAN_TARGET, MANACOST_R(3), 0,
									&td, "Select target noncreature artifact");
}

static int effect_jovens_ferrets(int player, int card, event_t event)
{
  if (end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  /* Ruling 10/1/2008: The second ability will trigger only if Joven's Ferrets is still on the battlefield at end of combat. If it's been dealt lethal
	   * combat damage and destroyed during that combat (or has left the battlefield during that combat by any other means), its ability won't trigger and the
	   * creatures that blocked it won't be affected. */
	  if (in_play(instance->damage_source_player, instance->damage_source_card))
		  alternate_legacy_text(2, instance->damage_source_player,
								does_not_untap_effect(instance->damage_source_player, instance->damage_source_card,
													  instance->damage_target_player, instance->damage_target_card,
													  EDNT_TAP_TARGET, 1));
	  // And don't remove the effect now, in case there's another combat this turn.
	}

  if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

int card_jovens_ferrets(int player, int card, event_t event)
{
	/* Joven's Ferrets	|G	0x200a43f
	 * Creature - Ferret 1/1
	 * Whenever ~ attacks, it gets +0/+2 until end of turn.
	 * At end of combat, tap all creatures that blocked ~ this turn. They don't untap during their controller's next untap step. */

  // Whenever ~ attacks, it gets +0/+2 until end of turn.
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
	alternate_legacy_text(1, player, pump_until_eot(player, card, player, card, 0, 2));

	// At end of combat, tap all creatures that blocked ~ this turn. They don't untap during their controller's next untap step.
  else if (event == EVENT_DECLARE_BLOCKERS && is_attacking(player, card))
	{
	  char marked[2][151] = {{0}};
	  mark_each_creature_blocking_me(player, card, marked);
	  int c;
	  for (c = 0; c < active_cards_count[1-player]; ++c)
		if (marked[1-player][c])
		  alternate_legacy_text(2, player, create_targetted_legacy_effect(player, card, effect_jovens_ferrets, 1-player, c));
	}

  return 0;
}

static int effect_cant_be_blocked_except_by_walls(int player, int card, event_t event){
  if (event == EVENT_BLOCK_LEGALITY)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->damage_target_card >= 0
		  && attacking_card_controller == instance->damage_target_player && attacking_card == instance->damage_target_card
		  && !has_subtype(affected_card_controller, affected_card, SUBTYPE_WALL))
		event_result = 1;
	}

  if (event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

int card_jovens_tools(int player, int card, event_t event){
	/* Joven's Tools	|6	0x200a444
	 * Artifact
	 * |4, |T: Target creature can't be blocked this turn except by Walls. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player ;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &effect_cant_be_blocked_except_by_walls,
											instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(4), 0, &td, "TARGET_CREATURE");
}

int card_koskun_falls(int player, int card, event_t event){
	/* Koskun Falls	|2|B|B	0x200a449
	 * World Enchantment
	 * At the beginning of your upkeep, sacrifice ~ unless you tap an untapped creature you control.
	 * Creatures can't attack you unless their controller pays |2 for each creature he or she controls that's attacking you. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		base_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_state = TARGET_STATE_TAPPED;

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			card_instance_t *instance = get_card_instance( player, card );
			tap_card(instance->targets[0].player, instance->targets[0].card);
			instance->number_of_targets = 0;
		}
		else{
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	tax_attack(player, card, event);

	return enchant_world(player, card, event);
}

int card_koskun_keep(int player, int card, event_t event)
{
  /* Koskun Keep	""	0x200d68f
   * Land
   * |T: Add |1 to your mana pool.
   * |1, |T: Add |R to your mana pool.
   * |2, |T: Add |B or |G to your mana pool. */
  return homelands_triple_land(player, card, event, COLOR_TEST_RED, COLOR_TEST_BLACK|COLOR_TEST_GREEN);
}

/* Labyrinth Minotaur	|3|U	=>m11.c:card_wall_of_frost
 * Creature - Minotaur 1/4
 * Whenever ~ blocks a creature, that creature doesn't untap during its controller's next untap step. */

int card_leaping_lizard(int player, int card, event_t event)
{
  /* Leaping Lizard	|1|G|G	0x200d6f3
   * Creature - Lizard 2/3
   * |1|G: ~ gets -0/-1 and gains flying until end of turn. */
  return generic_shade(player, card, event, 0, MANACOST_XG(1, 1), 0,-1, KEYWORD_FLYING,0);
}

int card_leeches(int player, int card, event_t event){
	/* Leeches	|1|W|W	0x200a453
	 * Sorcery
	 * Target player loses all poison counters. ~ deals that much damage to that player. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = POISON_COUNTERS(instance->targets[0].player);
			raw_set_poison(instance->targets[0].player, 0);
			damage_player(instance->targets[0].player, amount, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

static void give_first_strike(int player, int card, int t_player, int t_card)
{
  pump_ability_until_eot(player, card, t_player, t_card, 0,0, KEYWORD_FIRST_STRIKE,0);
}

int card_mammoth_harness(int player, int card, event_t event)
{
  /* Mammoth Harness	|3|G	0x200d6f8
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature loses flying.
   * Whenever enchanted creature blocks or becomes blocked by a creature, the other creature gains
   * first strike until end of turn. */

  if (event == EVENT_ABILITIES)
  {
    card_instance_t *inst = get_card_instance(player, card);
    if (!affect_me(inst->damage_target_player, inst->damage_target_card))
      return 0;
    event_result &= ~KEYWORD_FLYING;
  }

  if (event == EVENT_DECLARE_BLOCKERS)
  {
    card_instance_t *inst = get_card_instance(player, card);
    card_instance_t *ench = get_card_instance(inst->damage_target_player, inst->damage_target_card);

    if (ench->state & STATE_ATTACKING)
      for_each_creature_blocking_me(inst->damage_target_player, inst->damage_target_card,
                                    give_first_strike, player, card);

    if (ench->state & STATE_BLOCKING)
      for_each_creature_blocked_by_me(inst->damage_target_player, inst->damage_target_card,
                                      give_first_strike, player, card);
  }

  if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS)
    return vanilla_aura(player, card, event, 1 - player);
  return 0;
}

int card_marjhan(int player, int card, event_t event){
	/* Marjhan	|5|U|U	0x2002D39
	 * Creature - Leviathan 8/8
	 * ~ doesn't untap during your untap step.
	 * |U|U, Sacrifice a creature: Untap ~. Activate this ability only during your upkeep.
	 * ~ can't attack unless defending player controls |Han Island.
	 * |U|U: ~ gets -1/-0 until end of turn and deals 1 damage to target attacking creature without flying.
	 * When you control no |H1Islands, sacrifice ~. */

	does_not_untap(player, card, event);

	islandhome(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_abilities |= KEYWORD_FLYING;
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_UNTAP = 1,
		CHOICE_DAMAGE_ATTACKER
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE | GAA_ONLY_ON_UPKEEP, MANACOST_U(2), 0, NULL, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_U(2), 0, &td, NULL) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_CREATURE | GAA_ONLY_ON_UPKEEP, MANACOST_U(2), 0,
													NULL, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_U(2), 0, &td, NULL)
		};
		int choice = CHOICE_DAMAGE_ATTACKER;
		instance->info_slot = choice;
		choice = DIALOG(player, card, event, DLG_RANDOM,
						"Untap", abils[1], 10,
						"Damage attacking creature", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_UNTAP ){
			return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE | GAA_ONLY_ON_UPKEEP, MANACOST_U(2), 0, NULL, NULL);
		}
		if( choice == CHOICE_DAMAGE_ATTACKER ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_U(2), 0,
											&td, "Select target attacking creature without flying.");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_UNTAP ){
			untap_card(instance->parent_controller, instance->parent_card);
		}
		if( instance->info_slot == CHOICE_DAMAGE_ATTACKER ){
			if( valid_target(&td) ){
				pump_until_eot(instance->parent_controller, instance->parent_card,
								instance->parent_controller, instance->parent_card, -1, 0);
				damage_target0(player, card, 1);
			}
		}
	}

	return 0;
}

int card_memory_lapse(int player, int card, event_t event)
{
  /* Memory Lapse	|1|U	0x200ccc6
   * Instant
   * Counter target spell. If that spell is countered this way, put it on top of its owner's library instead of into that player's graveyard. */

  if (event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)))
    return counterspell(player, card, event, NULL, 0);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (counterspell_validate(player, card, NULL, 0))
    {
      card_instance_t *instance = get_card_instance(player, card);
      if (pre_counterspell(player, card, instance->targets[0].player, instance->targets[0].card))
        put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return 0;
}

int card_merchant_scroll(int player, int card, event_t event){
	/* Merchant Scroll	|1|U	0x2003F95
	 * Sorcery
	 * Search your library for a |Sblue instant card, reveal that card, and put it into your hand. Then shuffle your library. */
	// original code : 0040EA00

	if( event == EVENT_RESOLVE_SPELL ){
		char msg[100] = "Select a blue instant card.";
		test_definition_t this_test2;
		new_default_test_definition(&this_test2, TYPE_INSTANT | TYPE_INTERRUPT, msg);
		this_test2.type_flag = F1_NO_CREATURE;
		this_test2.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);

		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test2);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/* Mesa Falcon	|1|W	=>mirage.c:card_pearl_dragon
 * Creature - Bird 1/1
 * Flying
 * |1|W: ~ gets +0/+1 until end of turn. */

int card_mystic_decree(int player, int card, event_t event){
	/* Mystic Decree	|2|U|U	0x200a458
	 * World Enchantment
	 * All creatures lose flying and |H2islandwalk. */

	if( event == EVENT_ABILITIES && (event_result & (KEYWORD_FLYING | KEYWORD_BASIC_LANDWALK)) &&
		is_what(affected_card_controller, affected_card, TYPE_CREATURE) && ! is_humiliated(player, card, AL_STATIC)
	  ){
		event_result &= ~(KEYWORD_FLYING | get_hacked_walk(player, card, KEYWORD_ISLANDWALK));
	}

	return enchant_world(player, card, event);
}

/* Narwhal	|2|U|U	=>invasion.c:card_galinas_knight
 * Creature - Whale 2/2
 * First strike, protection from |Sred */

int card_orcish_mine(int player, int card, event_t event)
{
  /* Orcish Mine	|1|R|R	0x200d6fd
   * Enchantment - Aura
   * Enchant land
   * ~ enters the battlefield with three ore counters on it.
   * At the beginning of your upkeep or whenever enchanted land becomes tapped, remove an ore counter from ~.
   * When the last ore counter is removed from ~, destroy enchanted land and ~ deals 2 damage to that land's controller. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  enters_the_battlefield_with_counters(player, card, event, COUNTER_ORE, 3);
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    remove_counters(player, card, player, card, COUNTER_ORE, 1);
  else if (event == EVENT_TAP_CARD)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_card == affected_card &&
        instance->damage_target_player == affected_card_controller &&
        !is_humiliated(player, card, AL_TRIG))
      remove_counters(player, card, player, card, COUNTER_ORE, 1);
  }
  else if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      instance = get_card_instance(player, card);
      result = count_counters(player, card, COUNTER_ORE);
      if (result < 1)
      {
        if (instance->info_slot == 0x8b3)
        {
          result = is_humiliated(player, card, AL_TRIG);
          if (result == 0)
          {
            damage_player((int)instance->damage_target_player, 2, player, card);
            kill_card((int)instance->damage_target_player, instance->damage_target_card,
                      KILL_DESTROY);
          }
          else
          {
            instance->info_slot = 0;
          }
        }
      }
      else
      {
        instance->info_slot = 0x8b3;
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
  td.preferred_controller = 1 - player;
  return targeted_aura(player, card, event, &td, "TARGET_LAND");
}

int card_primal_order(int player, int card, event_t event){
	/* Primal Order	|2|G|G	0x2002D43
	 * Enchantment
	 * At the beginning of each player's upkeep, ~ deals damage to that player equal to the number of nonbasic lands he or she controls. */

	upkeep_trigger_ability(player, card, event, 2);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int dmg = count_subtype(current_turn, TYPE_LAND, -1)-count_subtype(current_turn, TYPE_LAND, SUBTYPE_BASIC);
		if( dmg > 0 ){
			damage_player(current_turn, dmg, player, card);
		}
	}

	return global_enchantment(player, card, event);
}

int card_prophecy(int player, int card, event_t event)
{
  /* Prophecy	|W	0x200d702
   * Sorcery
   * Reveal the top card of target opponent's library. If it's a land, you gain 1 life. Then that player shuffles his or her library.
   * Draw a card at the beginning of the next turn's upkeep. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.allowed_controller = 1-player;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  int tgt = get_card_instance(player, card)->targets[0].player;
		  int iid = deck_ptr[tgt][0];
		  if (iid != -1)
			{
			  reveal_card_iid(player, card, iid);
			  if (is_what(-1, iid, TYPE_LAND))
				gain_life(player, 1);
			}
		  shuffle(tgt);	// even if empty - rule 701.16e
		  cantrip(player, card, 1);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, "TARGET_OPPONENT", 1, NULL);
}

static void set_val_if_color(int color_test, int val, int t_player, int t_card)
{
  if (get_color(t_player, t_card) & color_test)
	*((int*)val) = 1;
}

int card_rashka_the_slayer(int player, int card, event_t event)
{
  /* Rashka the Slayer	|3|W|W	0x200d707
   * Legendary Creature - Human Archer 3/3
   * Reach
   * Whenever ~ blocks one or more |Sblack creatures, ~ gets +1/+2 until end of turn. */

  check_legend_rule(player, card, event);

  if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card, AL_TRIG))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  int color_test_black = 1<<get_sleighted_color(player, card, COLOR_BLACK);
	  int blocking_black = 0;
	  if (inst->state & STATE_BLOCKING)
		for_each_creature_blocked_by_me(player, card, set_val_if_color, color_test_black, (int)(&blocking_black));
	  if (blocking_black)
		pump_until_eot(player, card, player, card, 1,2);
	}

  return 0;
}

int card_reef_pirates(int player, int card, event_t event)
{
	/* Reef Pirates	|1|U|U	0x200b529
	 * Creature - Zombie Pirate 2/2
	 * Whenever ~ deals damage to an opponent, that player puts the top card of his or her library into his or her graveyard. */

  if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_OPPONENT))
	mill(1-player, 1);

  return 0;
}

int card_renewal(int player, int card, event_t event)
{
  /* Renewal	|2|G	0x200d70c
   * Sorcery
   * As an additional cost to cast ~, sacrifice a land.
   * Search your library for a basic land card and put that card onto the battlefield. Then shuffle your library.
   * Draw a card at the beginning of the next turn's upkeep. */

if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

	if (event == EVENT_CAN_CAST && basic_spell(player, card, event))
		return new_can_sacrifice_as_cost(player, card, &test);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
			spell_fizzled = 1;
		}
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
		tutor_basic_land(player, 1, 0);
		cantrip(player, card, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_retribution(int player, int card, event_t event){
	/* Retribution	|2|R|R	0x2002D48
	 * Sorcery
	 * Choose two target creatures an opponent controls. That player chooses and sacrifices one of those creatures. Put a -1/-1 counter on the other. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = 1-player;
	td.preferred_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL){
		int i;
		int trgs[2] = {0, 0};
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				trgs[i] = 1;
			}
		}

		if( trgs[0] + trgs[1] > 0 ){
			if( trgs[0] + trgs[1] > 1 ){
				int sacced = 0;
				i = 0;
				while( ! sacced ){
						if( trgs[i] ){
							if( do_dialog(instance->targets[i].player, instance->targets[i].player, instance->targets[i].card, -1, -1, " Sac this creature\n Pass", 0) == 0 ){
								if( can_sacrifice(player, instance->targets[i].player, 1, TYPE_CREATURE, 0) ){
									kill_card(instance->targets[i].player, instance->targets[i].card, KILL_SACRIFICE);
								}
								trgs[i] |= 2;
								sacced = 1;
							}
						}
						i++;
						if( i > 1 ){
							i = 0;
						}
				}
				for(i=0; i<2; i++){
					if( !(trgs[i] & 2) ){
						add_counters(player, card, instance->targets[i].player, instance->targets[i].card, COUNTER_M1_M1, 1);
						break;
					}
				}
			}
			else{
				for(i=0; i<2; i++){
					if(	trgs[i] ){
						if( can_sacrifice(player, instance->targets[i].player, 1, TYPE_CREATURE, 0) ){
							kill_card(instance->targets[i].player, instance->targets[i].card, KILL_SACRIFICE);
						}
						break;
					}
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature opponent controls.", 2, NULL);
}

int card_reveka_wizard_savant(int player, int card, event_t event){
	/* Reveka, Wizard Savant	|2|U|U	0x200a45d
	 * Legendary Creature - Dwarf Wizard 0/1
	 * |T: ~ deals 2 damage to any target and doesn't untap during your next untap step. */
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
			if (in_play(instance->parent_controller, instance->parent_card) ){
			  does_not_untap_effect(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									EDNT_CHECK_ORIGINAL_CONTROLLER, 1);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ANY");
}

int card_root_spider(int player, int card, event_t event)
{
  /* Root Spider	|3|G	0x200d6d5
   * Creature - Spider 2/2
   * Whenever ~ blocks, it gets +1/+0 and gains first strike until end of turn. */
  if (event == EVENT_CHECK_ABILITIES && affect_me(player, card) && current_turn != player && current_phase <= PHASE_DECLARE_BLOCKERS){
	check_abilities_keywords |= KEYWORD_FIRST_STRIKE;
	check_abilities_power += 1;
  }
  if (blocking(player, card, event) && !is_humiliated(player, card, AL_TRIG))
	pump_ability_until_eot(player, card, player, card, 1,0, KEYWORD_FIRST_STRIKE,0);
  return 0;
}

int card_roots(int player, int card, event_t event)
{
  /* Roots	|3|G	0x200a462
   * Enchantment - Aura
   * Enchant creature without flying
   * When ~ enters the battlefield, tap enchanted creature.
   * Enchanted creature doesn't untap during its controller's untap step. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if (event == EVENT_ETB_ABILITY){
		card_instance_t* inst = get_card_instance(player, card);
		tap_card(inst->damage_target_player, inst->damage_target_card);
	}

	if( get_card_instance(player, card)->damage_target_player >= 0)
	{
		card_instance_t* inst = get_card_instance(player, card);
		does_not_untap(inst->damage_target_player, inst->damage_target_card, event);
		if (event == EVENT_STATIC_EFFECTS && check_for_ability(inst->damage_target_player, inst->damage_target_card, KEYWORD_FLYING))
			kill_card(player, card, KILL_STATE_BASED_ACTION);
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = 1-player;
	td.illegal_abilities |= KEYWORD_FLYING;
	if (event == EVENT_ABILITIES && affect_me(player, card)){
		card_instance_t* inst = get_card_instance(player, card);
		int enchanted_card = inst->damage_target_card;
		inst = get_card_instance(player, card);
		if (get_abilities(inst->damage_target_player, enchanted_card, EVENT_ABILITIES, -1) & KEYWORD_FLYING){ kill_card(player, card, KILL_STATE_BASED_ACTION); }
	}

	return targeted_aura(player, card, event, &td, "FLOOD");	// Select target creature without flying.
}

int card_roterothopter(int player, int card, event_t event)
{
  /* Roterothopter	|1	0x200a467
   * Artifact Creature - Thopter 0/2
   * Flying
   * |2: ~ gets +1/+0 until end of turn. Activate this ability no more than twice each turn. */

	int rval = generic_shade_merge_pt(player, card, event, 0, MANACOST_X(2), 1,0);

	if (event == EVENT_CAN_ACTIVATE && rval && get_card_instance(player, card)->info_slot >= 2)
		return 0;

	if (event == EVENT_ACTIVATE && cancel != 1){
		card_instance_t* inst = get_card_instance(player, card);
		inst->info_slot = MAX(inst->info_slot + 1, 1);
	}

	if (event == EVENT_POW_BOOST){
		card_instance_t* inst = get_card_instance(player, card);
		rval = MIN(rval, 2 - inst->info_slot);
	}

	if (event == EVENT_CLEANUP)
		get_card_instance(player, card)->info_slot = 0;

	return rval;
}

int card_rysorian_badger(int player, int card, event_t event){
	/* Rysorian Badger	|2|G	0x000000:200a46c
	 * Creature - Badger 2/2
	 * Whenever ~ attacks and isn't blocked, you may exile up to two target creature cards from defending player's graveyard.
	 * If you do, you gain 1 life for each card exiled this way and ~ assigns no combat damage this turn. */

	if (event == EVENT_DECLARE_BLOCKERS){
	    int total_power = 0;
	    for (int c = active_cards_count[current_turn] - 1; c >= 0; --c){
	        if (in_play(current_turn, c) && is_what(current_turn, c, TYPE_CREATURE) && is_attacking(current_turn, c) && is_unblocked(current_turn, c)){
	            total_power += get_power(current_turn, c);
	        }
	    }
	    if (is_unblocked(player, card) && !is_humiliated(player, card, AL_TRIG) && !graveyard_has_shroud(1-player) && count_graveyard_by_type(1-player, TYPE_CREATURE) > 0){
	        int choice = 1;
	        if (IS_AI(player) && current_turn == player){ choice = life[1-player] <= total_power; }
	        if (do_dialog(player, player, card, -1, -1, " Exile creatures from gy\n Make combat damage", choice) == 0){
	            test_definition_t test;
	            new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");
	            test.qty = MIN(count_graveyard(1-current_turn), 2);
	            if (count_graveyard_by_type(1-current_turn, TYPE_CREATURE) > 0){
	                int result = new_global_tutor(player, 1-current_turn, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MAX_VALUE, &test);
	                if (result > 0){ gain_life(player, result); }
	            }
	            negate_combat_damage_this_turn(player, card, player, card, 0);
	        }
	    }
	}
	return 0;
}

/* ManalinkEh 2022: 0x2128b20; original cards/homelands.c:2870. */
int card_samite_alchemist(int player, int card, event_t event)
{
  /* Samite Alchemist	|3|W	0x000000
   * Creature - Human Cleric 0/2
   * |W|W, |T: Prevent the next 4 damage that would be dealt this turn to target creature you control. Tap that creature. It doesn't untap during your next untap step. */

  int result;
  card_instance_t *instance;
  target_definition_t td_prev;
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td_prev, 2);
    td_prev.allowed_controller = player;
    td_prev.preferred_controller = player;
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
      result = generic_activated_ability(
          player, card, EVENT_ACTIVATE, 7, 0, 0, 0, 0, 0, 2, 0, &td_prev,
          "Select target creature you control to prevent up to 4 damage.");
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
    default_target_definition(player, card, &td_prev, 2);
  }
  td_prev.allowed_controller = player;
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (td_prev.preferred_controller = td_prev.allowed_controller, result = valid_target(&td_prev),
       result != 0))
  {
    instance = get_card_instance(player, card);
    prevent_the_next_n_damage(player, card, instance->targets[0].player, instance->targets[0].card,
                              4, 0, 0, 0);
    result = does_not_untap_effect(player, card, instance->targets[0].player,
                                   instance->targets[0].card, 8, 1);
    alternate_legacy_text(2, player, result);
    return 0;
  }
  return 0;
}

/* Sea Sprite	|1|U	=>vanilla
 * Creature - Faerie 1/1
 * Flying, protection from |Sred */

int card_sea_troll(int player, int card, event_t event)
{
  /* Sea Troll	|2|U	0x200d71b
   * Creature - Troll 2/1
   * |U: Regenerate ~. Activate this ability only if ~ blocked or was blocked by a |Sblue creature this turn. */

  if (event == EVENT_DECLARE_BLOCKERS)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  int color_test_blue = 1<<get_sleighted_color(player, card, COLOR_BLUE);
	  int blocking_blue = 0;
	  if (inst->state & STATE_ATTACKING)
		for_each_creature_blocking_me(player, card, set_val_if_color, color_test_blue, (int)(&blocking_blue));
	  if (inst->state & STATE_BLOCKING)
		for_each_creature_blocked_by_me(player, card, set_val_if_color, color_test_blue, (int)(&blocking_blue));
	  if (blocking_blue)
		inst->info_slot = CARD_ID_SEA_TROLL;
	}

  if (event == EVENT_CLEANUP)
	get_card_instance(player, card)->info_slot = 0;

  if (event == EVENT_CAN_ACTIVATE && get_card_instance(player, card)->info_slot != CARD_ID_SEA_TROLL)
	return 0;

  return regeneration(player, card, event, MANACOST_U(1));
}

int card_sengir_autocrat(int player, int card, event_t event)
{
  /* Sengir Autocrat	|3|B	0x2002D4D
   * Creature - Human 2/2
   * When ~ enters the battlefield, put three 0/1 |Sblack Serf creature tokens onto the battlefield.
   * When ~ leaves the battlefield, exile all Serf tokens. */

  test_definition_t this_test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    generate_tokens_by_id(player, card, CARD_ID_SERF, 3);
    leaves_play(player, card, EVENT_ETB, 1, 1);
    return 0;
  }
  leaves_play(player, card, event, 1, 1);
  if (event != EVENT_LEAVES_PLAY_ABILITY)
  {
    return 0;
  }
  default_test_definition(&this_test, 2);
  this_test.subtype = 0xa8;
  new_manipulate_all(player, card, 2, &this_test, ACT_KILL_REMOVE);
  return 0;
}

int card_sengir_bats(int player, int card, event_t event){
	/* Sengir Bats	|1|B|B	0x2007456
	 * Creature - Bat 1/2
	 * Flying
	 * Whenever a creature dealt damage by ~ this turn dies, put a +1/+1 counter on ~. */
	// also code for Sengir Vampire
	if( sengir_vampire_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		add_counters(player, card, player, card, COUNTER_P1_P1, get_card_instance(player, card)->targets[11].card);
	}
	return 0;
}

int card_serra_aviary(int player, int card, event_t event){
	/* Serra Aviary	|3|W	0x200a471
	 * World Enchantment
	 * Creatures with flying get +1/+1. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && in_play(player, card) && check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING) &&
		! is_humiliated(player, card, AL_STATIC) )
	{
		event_result++;
	}
	return enchant_world(player, card, event);
}

/* ManalinkEh 2022: 0x2129110; original cards/homelands.c:2988. */
int card_serra_bestiary(int player, int card, event_t event)
{
  /* Serra Bestiary	|W|W	0x000000
   * Enchantment - Aura
   * Enchant creature
   * At the beginning of your upkeep, sacrifice ~ unless you pay |W|W.
   * Enchanted creature can't attack or block, and its activated abilities with |T in their costs can't be activated. */

  card_instance_t *instance;
  int result;
  card_instance_t *source_in_play;
  card_instance_t *aura;
  target_definition_t td;

  basic_upkeep(player, card, event, 0, 0, 0, 0, 0, 2);
  result = is_humiliated(player, card, AL_STATIC);
  if ((result == 0) && (aura = get_card_instance(player, card), -1 < aura->damage_target_player))
  {

    instance = get_card_instance(player, card);
    source_in_play = in_play(player, card);
    if (source_in_play != NULL)
    {
      cannot_attack((int)instance->damage_target_player, instance->damage_target_card, event);
      cannot_block((int)instance->damage_target_player, instance->damage_target_card, event);
      set_special_flags2((int)instance->damage_target_player, instance->damage_target_card, 0x10);
    }
  }
  leaves_play(player, card, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {

    instance = get_card_instance(player, card);
    remove_special_flags2((int)instance->damage_target_player, instance->damage_target_card, 0x10);
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
  return disabling_targeted_aura(player, card, event, &td, "Select target creature.", 0);
}

int card_serra_inquisitors(int player, int card, event_t event)
{
  /* Serra Inquisitors	|4|W	0x200d711
   * Creature - Human Cleric 3/3
   * Whenever ~ blocks or becomes blocked by one or more |Sblack creatures, ~ gets +2/+0 until end of turn. */

  if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card, AL_STATIC))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  int color_test_black = 1<<get_sleighted_color(player, card, COLOR_BLACK);
	  int blocking_black = 0;
	  if (inst->state & STATE_ATTACKING)
		for_each_creature_blocking_me(player, card, set_val_if_color, color_test_black, (int)(&blocking_black));
	  if (inst->state & STATE_BLOCKING)
		for_each_creature_blocked_by_me(player, card, set_val_if_color, color_test_black, (int)(&blocking_black));
	  if (blocking_black)
		pump_until_eot(player, card, player, card, 2,0);
	}

  return 0;
}

int card_serra_paladin(int player, int card, event_t event)
{
  /* Serra Paladin	|2|W|W	0x200a476
   * Creature - Human Knight 2/2
   * |T: Prevent the next 1 damage that would be dealt to any target this turn.
   * |1|W|W, |T: Target creature gains vigilance until end of turn. */

  if (!IS_GAA_EVENT(event))
	return 0;

  if (land_can_be_played & LCBP_DAMAGE_PREVENTION)
	return card_samite_healer(player, card, event);

    target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	if( IS_AI(player) ){
	    td.special = TARGET_SPECIAL_NOT_ME;
	}

  return vanilla_creature_pumper(player, card, event, MANACOST_XW(1, 2), GAA_UNTAPPED, 0,0, 0,SP_KEYWORD_VIGILANCE, &td);
}

int card_serrated_arrows(int player, int card, event_t event){
	/* Serrated Arrows	|4	0x200181C
	 * Artifact
	 * ~ enters the battlefield with three arrowhead counters on it.
	 * At the beginning of your upkeep, if there are no arrowhead counters on ~, sacrifice it.
	 * |T, Remove an arrowhead counter from ~: Put a -1/-1 counter on target creature. */

	if (event == EVENT_SHOULD_AI_PLAY){
		ai_modifier += (player == AI ? 1 : -1) * count_counters(player, card, COUNTER_ARROWHEAD);
	}

	enters_the_battlefield_with_counters(player, card, event, COUNTER_ARROWHEAD, 3);

	if( trigger_condition == TRIGGER_UPKEEP && current_turn == player && count_counters(player, card, COUNTER_ARROWHEAD) <= 0 &&
		upkeep_trigger(player, card, event) )
	{
	   kill_card(player, card, KILL_SACRIFICE);
	}

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td, 0) ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_M1_M1, 1);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(0), GVC_COUNTERS(COUNTER_ARROWHEAD, 1),
									&td, "TARGET_CREATURE");
}

/* Shrink	|G	=>m13.c:card_hydrosurge
 * Instant
 * Target creature gets -5/-0 until end of turn. */

int card_soraya_the_falconer(int player, int card, event_t event){
	/* Soraya the Falconer	|1|W|W	0x20011A0
	 * Legendary Creature - Human 2/2
	 * Bird creatures get +1/+1.
	 * |1|W: Target Bird creature gains banding until end of turn. */

	check_legend_rule(player, card, event);

	boost_creature_type(player, card, event, SUBTYPE_BIRD, 1, 1, 0, BCT_INCLUDE_SELF);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_BIRD;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_BANDING, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XW(1, 1), 0,
									&td, "Select target Bird creature.");
}

int card_spectral_bears(int player, int card, event_t event){
	/* Spectral Bears	|1|G	0x200a480
	 * Creature - Bear Spirit 3/3
	 * Whenever ~ attacks, if defending player controls no |Sblack nontoken permanents, it doesn't untap during your next untap step. */

	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "");
		test.type_flag = F1_NO_TOKEN;
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		if (!check_battlefield_for_special_card(player, card, 1-current_turn, 0, &test)){
			does_not_untap_effect(player, card, player, card, EDNT_CHECK_ORIGINAL_CONTROLLER, 1);
		}
	}
	return 0;
}

/* Timmerian Fiends	|1|B|B	0x000000
 * Creature - Horror 1/1
 * Remove ~ from your deck before playing if you're not playing for ante.
 * |B|B|B, Sacrifice ~: The owner of target artifact may ante the top card of his or her library. If that player doesn't, exchange ownership of that artifact and ~. Put the artifact card into your graveyard and ~ from anywhere into that player's graveyard. This change in ownership is permanent. */

int card_torture(int player, int card, event_t event)
{
	/* Torture	|B	0x200b52e
	 * Enchantment - Aura
	 * Enchant creature
	 * |1|B: Put a -1/-1 counter on enchanted creature. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		if( IS_GAA_EVENT(event) ){
			if (event == EVENT_RESOLVE_ACTIVATION)
			{
				card_instance_t* instance = get_card_instance(player, card);
				add_counters(player, card, instance->damage_target_player, instance->damage_target_card, COUNTER_M1_M1, 1);
			}
			return generic_activated_ability(player, card, event, 0, MANACOST_XB(1,1), 0, NULL, NULL);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return disabling_aura(player, card, event);
}

int card_trade_caravan(int player, int card, event_t event){
	/* Trade Caravan	|W	0x200a485
	 * Creature - Human Nomad 1/1
	 * At the beginning of your upkeep, put a currency counter on ~.
	 * Remove two currency counters from ~: Untap target basic land. Activate this ability only during an opponent's upkeep. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counters(player, card, player, card, COUNTER_CURRENCY, 1);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_BASIC;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_CAN_ACTIVATE && current_phase == PHASE_UPKEEP && current_turn != player ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, GVC_COUNTERS(COUNTER_CURRENCY, 2), &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, GVC_COUNTERS(COUNTER_CURRENCY, 2),
										&td, "TARGET_BASICLAND");
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

/* Truce	|2|W	=>portal_1_2_3k.c:card_temporary_truce
 * Instant
 * Each player may draw up to two cards. For each card less than two a player draws this way, that player gains 2 life. */

int card_veldrane_of_sengir(int player, int card, event_t event)
{
  /* Veldrane of Sengir	|5|B|B	0x200d716
   * Legendary Creature - Human Rogue 5/5
   * |1|B|B: ~ gets -3/-0 and gains |H2forestwalk until end of turn. */
  check_legend_rule(player, card, event);
  return generic_shade(player, card, event, 0, MANACOST_XB(1, 2), -3, 0, KEYWORD_FORESTWALK,0);
}

int card_wall_of_kelp(int player, int card, event_t event){
	/* Wall of Kelp	|U|U	0x200173B
	 * Creature - Plant Wall 0/3
	 * Defender
	 * |U|U, |T: Put a 0/1 |Sblue Plant Wall creature token with defender named Kelp onto the battlefield. */

	if(event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_KELP);
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_U(2), 0, NULL, NULL);
}

/* Willow Faerie	|1|G	=>vanilla
 * Creature - Faerie 1/2
 * Flying */

int card_willow_priestess(int player, int card, event_t event){
	/* Willow Priestess	|2|G|G	0x2002D52
	 * Creature - Faerie Druid 2/2
	 * |T: You may put a Faerie permanent card from your hand onto the battlefield.
	 * |2|G: Target |Sgreen creature gains protection from |Sblack until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.preferred_controller = player;
	td1.required_color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a Faerie permanent card.");
	this_test.subtype = SUBTYPE_FAERIE;
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_FAERIE = 1,
		CHOICE_PROT_BLACK
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XG(2, 1), 0, &td1, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_XG(2, 1), 0, &td1, NULL)
						
		};
		int priority_faerie = check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &this_test) ? 10 : 1;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
		                    "Put a Faerie into play", 	abils[1], priority_faerie,
							"Give Prot. to Black", 		abils[2], 5);
							
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_FAERIE ){
			generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
		if( choice == CHOICE_PROT_BLACK ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XG(2, 1), 0,
										&td1, get_sleighted_color_text(player, card, "Select target %s creature", COLOR_GREEN));
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( instance->info_slot == CHOICE_PROT_BLACK && valid_target(&td1) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0,
									get_sleighted_protection(instance->parent_controller, instance->parent_card, KEYWORD_PROT_BLACK), 0);
		}
		if( instance->info_slot == CHOICE_FAERIE && hand_count[player] > 0 ){
			new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}

	return 0;
}

int card_winter_sky(int player, int card, event_t event)
{
  /* Winter Sky	|R	0x200d720
   * Sorcery
   * Flip a coin. If you win the flip, ~ deals 1 damage to each creature and each player. If you lose the flip, each player draws a card. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (flip_a_coin(player, card))
		new_damage_all(player, card, ANYBODY, 1, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
	  else
		APNAP(p, draw_a_card(p));
	  kill_card(player, card, KILL_DESTROY);
	}

  return basic_spell(player, card, event);
}

int card_wizards_school(int player, int card, event_t event)
{
  /* Wizards' School	""	0x200d694
   * Land
   * |T: Add |1 to your mana pool.
   * |1, |T: Add |U to your mana pool.
   * |2, |T: Add |W or |B to your mana pool. */
  return homelands_triple_land(player, card, event, COLOR_TEST_BLUE, COLOR_TEST_WHITE|COLOR_TEST_BLACK);
}
