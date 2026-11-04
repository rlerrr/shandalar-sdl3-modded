#include "manalink.h"

static int teferis_protection_legacy(int player, int card, event_t event);

/* Alms Collector	|3|W
 * Creature - Cat Cleric 3/4
 * Flash
 * If an opponent would draw two or more cards, instead you and that player each draw a card. */

int card_balan_wandering_knight(int player, int card, event_t event)
{
  /* Balan, Wandering Knight	|2|W|W
   * Legendary Creature - Cat Knight 3/3
   * First strike
   * ~ has double strike as long as two or more Equipment are attached to it.
   * |1|W: Attach all Equipment you control to ~. */

  check_legend_rule(player, card, event);

  if (event == EVENT_ABILITIES && affect_me(player, card) && in_play(player, card)
	  && equipments_attached_to_me(player, card, EATM_REPORT_TOTAL) >= 2)
	event_result |= KEYWORD_DOUBLE_STRIKE;


  if (event == EVENT_RESOLVE_ACTIVATION){
	card_instance_t* instance = get_card_instance(player, card);
	for (int c = 0; c < active_cards_count[player]; ++c)
	  if (in_play(player, c) && is_what(player, c, TYPE_ARTIFACT) && has_subtype(player, c, SUBTYPE_EQUIPMENT))
		equip_target_creature(player, c, instance->parent_controller, instance->parent_card);
  }

  return generic_activated_ability(player, card, event, GAA_NONE, MANACOST_XW(1, 1), 0, NULL, NULL);
}

int card_curse_of_vitality(int player, int card, event_t event)
{
  /* Curse of Vitality	|2|W
   * Enchantment - Aura Curse
   * Enchant player
   * Whenever enchanted player is attacked, you gain 2 life. Each opponent attacking that player does the same. */

  card_instance_t *instance;
  int result;
  instance = get_card_instance(player, card);
  if (-1 < instance->targets[0].player)
  {
    instance = get_card_instance(player, card);
    store_attackers(player, card, event, DAT_ATTACKS_PLAYER, 1 - instance->targets[0].player, -1,
                    NULL);
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
    if (event == EVENT_DAT_ABILITY)
    {
      gain_life(player, 2);
    }
  }
  result = curse(player, card, event);
  return result;
}

int card_fortunate_few(int player, int card, event_t event)
{
  /* Fortunate Few	|3|W|W
   * Sorcery
   * Choose a nonland permanent you don't control, then each other player chooses a nonland permanent they don't control that hasn't been chosen this way. Destroy all other nonland permanents. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_PERMANENT);
  td.illegal_type = TYPE_LAND;
  td.illegal_abilities = 0;  // Choices, not targets: shroud and protection do not apply.
  td.allow_cancel = 0;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  APNAP(p,
			inst->targets[p].player = inst->targets[p].card = -1;
			td.who_chooses = p;
			td.allowed_controller = td.preferred_controller= 1-p;
			if (can_target(&td)
				&& new_pick_target(&td, "Select a nonland permanent you don't control.", p, GS_LITERAL_PROMPT))
			  get_card_instance(inst->targets[p].player, inst->targets[p].card)->state |= STATE_CANNOT_TARGET);
	  inst->number_of_targets = 2;

	  int p, c;
	  for (p = 0; p <= 1; ++p)
		if (inst->targets[p].player >= 0)
		  get_card_instance(inst->targets[p].player, inst->targets[p].card)->state &= ~STATE_CANNOT_TARGET;

	  for (p = 0; p <= 1; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if (in_play(p, c)
			  && !(c == inst->targets[0].card && p == inst->targets[0].player)
			  && !(c == inst->targets[1].card && p == inst->targets[1].player)
			  && is_what(p, c, TYPE_PERMANENT)
			  && !is_what(p, c, TYPE_LAND))
			kill_card(p, c, KILL_DESTROY);

	  kill_card(player, card, KILL_DESTROY);
	}

  return basic_spell(player, card, event);
}

/* Kindred Boon	|2|W|W
 * Enchantment
 * As ~ enters the battlefield, choose a creature type.
 * |1|W: Put a divinity counter on target creature you control of the chosen type.
 * Each creature you control with a divinity counter on it has indestructible. */

/* Scalelord Reckoner	|3|W|W
 * Creature - Dragon 4/4
 * Flying
 * Whenever a Dragon you control becomes the target of a spell or ability an opponent controls, destroy target nonland permanent that player controls. */

/* Stalking Leonin	|2|W
 * Creature - Cat Archer 3/3
 * When ~ enters the battlefield, secretly choose an opponent.
 * Reveal the player you chose: Exile target creature that's attacking you if it's controlled by the chosen player. Activate this ability only once. */

/* ManalinkEh 2022: 0x228d4a0; original cards/commander_2017.c:115. */
static int teferis_protection_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;

  instance = in_play(player, card);
  if ((instance != NULL) &&
      (is_humiliated(player, card, AL_STATIC) == 0))
  {
    player_bits[player] =
        player_bits[player] | 0x4200000;
  }
  if (event == EVENT_PROT_MISC)
  {
    if ((affected_card_controller == player) && (affected_card == -1))
    {
      event_result = event_result | 1;
    }
  }
  else if ((event == EVENT_BEGIN_TURN) && (player == current_turn))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x228da90; original cards/commander_2017.c:131. */
int card_teferis_protection(int player, int card, event_t event)
{
  /* Teferi's Protection	|2|W
   * Instant
   * Until your next turn, your life total can't change and you have protection from everything. All permanents you control phase out.
   * Exile ~. */

  int result;
  card_instance_t *instance;
  test_definition_t this_test;

  if (event == EVENT_RESOLVE_SPELL)
  {

    result = create_legacy_effect(player, card, teferis_protection_legacy);
    instance = get_card_instance(player, result);
    result = get_id(player, card);
    instance->targets[2].card = result;
    result = get_id(player, card);
    instance->targets[3].card = result;
    default_test_definition(&this_test, 0x147);
    new_manipulate_all(player, card, player, &this_test, ACT_PHASE_OUT);
    kill_card(player, card, KILL_EXILE);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* Curse of Verbosity	|2|U
 * Enchantment - Aura Curse
 * Enchant player
 * Whenever enchanted player is attacked, you draw a card. Each opponent attacking that player does the same. */

/* ManalinkEh 2022: 0x228db60; original cards/commander_2017.c:159. */
int card_galecaster_colossus(int player, int card, event_t event)
{
  /* Galecaster Colossus	|5|U|U
   * Creature - Giant Wizard 5/6
   * Tap an untapped Wizard you control: Return target nonland permanent you don't control to its owner's hand. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  target_definition_t td_subtype;
  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      result = instance->targets[0].card;
      instance = get_card_instance(player, card);
      bounce_permanent(instance->targets[0].player, result);
    }
  }
  default_target_definition(player, card, &td, 0x47);
  td.allowed_controller = 1 - player;
  td.illegal_type = TYPE_LAND;
  default_target_definition(player, card, &td_subtype, 0x47);
  td_subtype.allowed_controller = player;
  td_subtype.preferred_controller = player;
  td_subtype.illegal_state = TARGET_STATE_TAPPED;
  td_subtype.illegal_abilities = 0;
  td_subtype.required_subtype = 0xd6;
  result = generic_activated_ability_with_tapsubtype_as_cost(
      player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, 0, 0, 0, 0, 0, 0, 0, &td,
      "Select target nonland permanent you don\'t control", &td_subtype, 1);
  return result;
}

/* ManalinkEh 2022: 0x228dcb0; original cards/commander_2017.c:185. */
int card_kindred_discovery(int player, int card, event_t event)
{
  /* Kindred Discovery	|3|U|U
   * Enchantment
   * As ~ enters the battlefield, choose a creature type.
   * Whenever a creature you control of the chosen type enters the battlefield or attacks, draw a card. */

  unsigned int chosen_subtype;
  int result;
  card_instance_t *instance;
  test_definition_t test;

  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    result = select_a_subtype(player, card);
    instance->info_slot = result;
  }
  else if (event == EVENT_DECLARE_ATTACKERS && !is_humiliated(player, card, AL_TRIG))
  {
    instance = get_card_instance(player, card);
    chosen_subtype = instance->info_slot;
    default_test_definition(&test, 2);
    test.subtype = chosen_subtype;
    store_attackers(player, card, EVENT_DECLARE_ATTACKERS,
                    DAT_DONT_REPORT_VALUE | DAT_SEPARATE_TRIGGERS, player, -1, &test);
  }
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      chosen_subtype = instance->info_slot;
      default_test_definition(&test, 2);
      test.subtype = chosen_subtype;
      etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
    }
  }
  if ((event == EVENT_ETB) || (event == EVENT_DAT_ABILITY))
  {
    draw_a_card(player);
  }
  return global_enchantment(player, card, event);
}

/* Magus of the Mind	|4|U|U
 * Creature - Human Wizard 4/5
 * |U, |T, Sacrifice ~: Shuffle your library, then exile the top X cards, where X is one plus the number of spells cast this turn. Until end of turn, you may play cards exiled this way without paying their mana costs. */

/* Portal Mage	|2|U
 * Creature - Human Wizard 2/2
 * Flash
 * When ~ enters the battlefield during the declare attackers step, you may reselect which player or planeswalker target attacking creature is attacking. */

/* ManalinkEh 2022: 0x228deb0; original cards/commander_2017.c:230. */
int card_bloodline_necromancer(int player, int card, event_t event)
{
  /* Bloodline Necromancer	|4|B
   * Creature - Vampire Wizard 3/2
   * Lifelink
   * When ~ enters the battlefield, you may return target Vampire or Wizard creature card from your graveyard to the battlefield. */

  int result;
  test_definition_t test;
  lifelink(player, card, event);
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 2, "Select target Vampire or Wizard creature card.");
    test.subtype = 0xcd;
    test.sub2 = 0xd6;
    test.subtype_flag = 3;
    result = new_special_count_grave(player, &test);
    if (result != 0)
    {
      result = graveyard_has_shroud(player);
      if (result == 0)
      {
        new_global_tutor(player, player, 2, 4, 0, 2, &test);
      }
    }
  }
  return 0;
}

/* Boneyard Scourge	|2|B|B
 * Creature - Zombie Dragon 4/3
 * Flying
 * Whenever a Dragon you control dies while ~ is in your graveyard, you may pay |1|B. If you do, return ~ from your graveyard to the battlefield. */

/* Curse of Disturbance	|2|B
 * Enchantment - Aura Curse
 * Enchant player
 * Whenever enchanted player is attacked, create a 2/2 |Sblack Zombie creature token. Each opponent attacking that player does the same. */

/* Kheru Mind-Eater	|2|B
 * Creature - Vampire 1/3
 * Menace
 * Whenever ~ deals combat damage to a player, that player exiles a card from their hand face down.
 * You may look at and play cards exiled with ~. */

/* Kindred Dominance	|5|B|B
 * Sorcery
 * Choose a creature type. Destroy all creatures that aren't of the chosen type. */

/* New Blood	|2|B|B
 * Sorcery
 * As an additional cost to cast this spell, tap an untapped Vampire you control.
 * Gain control of target creature. Change the text of that creature by replacing all instances of one creature type with Vampire. */

/* Patron of the Vein	|4|B|B
 * Creature - Vampire Shaman 4/4
 * Flying
 * When ~ enters the battlefield, destroy target creature an opponent controls.
 * Whenever a creature an opponent controls dies, exile it and put a +1/+1 counter on each Vampire you control. */

/* ManalinkEh 2022: 0x228dfa0; original cards/commander_2017.c:286. */
int card_vindictive_lich(int player, int card, event_t event)
{
  /* Vindictive Lich	|3|B
   * Creature - Zombie Wizard 4/1
   * When ~ dies, choose one or more. Each mode must target a different player.
   * * Target opponent sacrifices a creature.
   * * Target opponent discards two cards.
   * * Target opponent loses 5 life. */

  int result;
  card_instance_t *instance;
  test_definition_t test;

  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    result = opponent_is_valid_target(player, card);
    if (result != 0)
    {

      result = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                           "Sacrifice", 1, 1, "Discard 2", 1, 1, "Lose 5 life", 1, 1, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
      }
      else
      {
        target_opponent(player, card);
        instance = get_card_instance(player, card);
        if (result == 1)
        {
          new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
          new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED | SAC_NO_CANCEL,
                        &test);
        }
        else if (result == 2)
        {
          new_multidiscard(instance->targets[0].player, 2, 0, player);
        }
        else if (result == 3)
        {
          lose_life(instance->targets[0].player, 5);
        }
      }
    }
  }
  return 0;
}

/* Bloodsworn Steward	|2|R|R
 * Creature - Vampire Knight 4/4
 * Flying
 * Commander creatures you control get +2/+2 and have haste. */

/* Crimson Honor Guard	|3|R|R
 * Creature - Vampire Knight 4/5
 * Trample
 * At the beginning of each player's end step, ~ deals 4 damage to that player unless they control a commander. */

/* Curse of Opulence	|R
 * Enchantment - Aura Curse
 * Enchant player
 * Whenever enchanted player is attacked, create a colorless artifact token named Gold. It has "Sacrifice this artifact: Add one mana of any color." Each opponent attacking that player does the same. */

/* Disrupt Decorum	|2|R|R
 * Sorcery
 * Goad all creatures you don't control. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x228e160; original cards/commander_2017.c:340. */
int card_izzet_chemister(int player, int card, event_t event)
{
  /* Izzet Chemister	|2|R
   * Creature - Goblin Wizard 1/3
   * Haste
   * |R, |T: Exile target instant or sorcery card from your graveyard.
   * |1|R, |T, Sacrifice ~: Cast any number of cards exiled with ~ without paying their mana costs. */

  enum
  {
    CHOICE_EXILE = 1,
    CHOICE_CAST = 2,
    CHOICE_SHOW_EXILED = 3
  };

  haste(player, card, event);
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  char buffer[50];
  scnprintf(buffer, 50, "Select a card to exile");
  test_definition_t test;
  new_default_test_definition(&test, 0x38, buffer);
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (generic_activated_ability(player, card, event, 9, 1, 0, 0, 0, 1, 0, 0, NULL, NULL) &&
        exiledby_count(player, card, player) > 0 &&
        exiledby_choose(player, card, 0x4398, EXBY_CAN_CAST, 1, "", 1))
      return exiledby_choose(player, card, 0x4398, EXBY_CAN_CAST, 1, "", 1);
    if (generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, 0, 0,
                                                            0, 0, 1, 0, 0, NULL, NULL, TYPE_NONE,
                                                            &test, 8))
      return 1;
    return player == 0 && exiledby_count(0, card, 0) > 0;
  }
  if (event == EVENT_ACTIVATE)
  {
    int have_exile = exiledby_count(player, card, player) > 0;
    int abilities[5];
    abilities[0] = generic_activated_ability_with_target_in_grave(
        player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, 0, 0, 0, 0, 1, 0, 0, NULL, NULL, TYPE_NONE,
        &test, 8);
    abilities[1] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 1, 0, 0, 0,
                                                  1, 0, 0, NULL, NULL);
    abilities[2] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 1, 0, 0, 0,
                                                  1, 0, 0, NULL, NULL);
    abilities[3] = player == 0 && have_exile;
    abilities[4] = 0;
    int weight = exiledby_count(player, card, player) * 2;
    int can_cast = player == 1 ? abilities[1] != 0 && have_exile : abilities[2];
    int choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Exile from graveyard",
                             abilities[0], 6, "Cast spells from exile", can_cast, weight,
                             "Show exiled cards", abilities[3], 0, 0, 0, 0, 0);
    if (choice == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (choice == CHOICE_SHOW_EXILED)
    {
      exiledby_choose(player, card, 0x4398, EXBY_CHOOSE, 0, "Card", 1);
      spell_fizzled = 1;
      return 0;
    }
    if (choice == CHOICE_EXILE)
    {
      instance->number_of_targets = 0;
      generic_activated_ability_with_target_in_grave(player, card, EVENT_ACTIVATE,
                                                          GAA_UNTAPPED, 0, 0, 0, 0, 1, 0, 0, NULL,
                                                          NULL, TYPE_NONE, &test, 8);
      return 0;
    }
    if (choice != CHOICE_CAST)
      return 0;
    instance->number_of_targets = 0;
    add_state(player, card, 0x10);
    if (charge_mana_for_activated_ability(player, card, 1, 0, 0, 0, 1, 0))
    {
      instance->targets[1].player = exiledby_count(player, card, player);
      instance->targets[1].card = exiledby_detach(player, card);
      kill_card(player, card, KILL_SACRIFICE);
    }
    if (spell_fizzled == 1)
      remove_state(player, card, 0x10);
    tapped_for_mana_color = -1;
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->info_slot == CHOICE_EXILE)
    {
      int selected = validate_target_from_grave_source(player, card, player, 0);
      if (selected >= 0)
      {
        int leg = 0, idx = 0;
        exiledby_remember(player, instance->parent_card, player, get_grave(player)[selected], &leg,
                          &idx);
        rfg_card_from_grave(player, selected);
      }
    }
    if (instance->info_slot == CHOICE_CAST)
    {
      int leg = 0, idx = 0;
      int amount = instance->targets[1].player;
      if (amount > 0)
      {
        int ic[amount];
        int count = 0;
        int *location;
        while ((location = exiledby_find_any(player - 2, instance->targets[1].card, &leg, &idx)) !=
               NULL)
        {
          uint32_t packed = (uint32_t)*location;
          *location = -1;
          int iid = packed & 0x7fffffff;
          if (check_rfg(packed >> 31, (uint16_t)cards_data[iid].id))
            ic[count++] = iid;
        }
        test_definition_t this_test;
        while (count > 0)
        {
          new_default_test_definition(&this_test, 0xff, "Select a card to play for free");
          int selected = select_card_from_zone(player, player, ic, count, 0, 1, -1, &test);
          if (selected == -1)
            break;
          unsigned csvid = (uint16_t)cards_data[ic[selected]].id;
          for (int k = selected; k < count; ++k)
            ic[k] = ic[k + 1];
          --count;
          play_card_in_exile_for_free(player, 1 - player, csvid);
        }
        exiledby_destroy_detached(player, instance->targets[1].card);
      }
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x228eaa0; original cards/commander_2017.c:494. */
int card_kindred_charge(int player, int card, event_t event)
{
  /* Kindred Charge	|4|R|R
   * Sorcery
   * Choose a creature type. For each creature you control of the chosen type, create a token that's a copy of that creature. Those tokens gain haste. Exile them at the beginning of the next end step. */

  int c;
  subtype_t subtype;
  token_generation_t token;

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

      subtype = select_a_subtype(player, card);
      char marked[151] = {0};
      for (int c = 0; c < active_cards_count[player]; ++c)
        if (in_play(player, c) && is_what(player, c, 2) && has_creature_type(player, c, subtype))
          marked[c] = 1;
      if (active_cards_count[player] > 0)
      {
        c = 0;
        do
        {
          if (marked[c])
          {
            copy_token_definition(player, card, &token, player, c);
            token.special_code_on_generation = haste_and_remove_token_at_end_of_next_turn;
            generate_token(&token);
          }
          ++c;
        } while (c < active_cards_count[player]);
      }
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x228ec90; original cards/commander_2017.c:523. */
int card_shifting_shadow(int player, int card, event_t event)
{
  /* Shifting Shadow	|2|R
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has haste and "At the beginning of your upkeep, destroy this creature. Reveal cards from the top of your library until you reveal a creature card. Put that card onto the battlefield and attach ~ to it, then put all other cards revealed this way on the bottom of your library in a random order." */

  int result;
  card_instance_t *instance;
  test_definition_t test;
  if ((current_turn == player) && (result = upkeep_trigger(player, card, event), result != 0))
  {
    default_test_definition(&test, 2);
    instance = get_card_instance(player, card);
    kill_card((int)instance->damage_target_player, instance->damage_target_card, KILL_DESTROY);
    result = reveal_card_until_you_reveal_test(player, player, &test, 0x244);
    if (result < 0)
    {
      kill_card(player, card, KILL_DESTROY);
    }
    else
    {
      instance->targets[0].player = player;
      instance->targets[0].card = result;
      call_card_function(player, card, EVENT_RESOLVE_MOVING_AURA);
    }
  }
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
        return generic_aura(player, card, event, 1 - player, 0, 0, 0, 4, 0, 0, 0);
    }
    else if ((event == EVENT_RESOLVE_SPELL) ||
             ((event == EVENT_CAN_CHANGE_TARGET || (event == EVENT_CHANGE_TARGET))))
      return generic_aura(player, card, event, 1 - player, 0, 0, 0, 4, 0, 0, 0);
    if ((((event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA) &&
         (((2 < (uint32_t)(event - EVENT_POWER) && (event != EVENT_RESOLVE_MOVING_AURA)) &&
           (event != EVENT_CHANGE_TYPE)))) &&
        (((event != EVENT_SET_COLOR && (event != EVENT_GET_SELECTED_CARD)) &&
          (event != EVENT_STATIC_EFFECTS))))
    {
      return 0;
    }
  }
  return generic_aura(player, card, event, 1 - player, 0, 0, 0, 4, 0, 0, 0);
}

/* Territorial Hellkite	|2|R|R
 * Creature - Dragon 6/5
 * Flying, haste
 * At the beginning of combat on your turn, choose an opponent at random that ~ didn't attack during your last combat. ~ attacks that player this combat if able. If you can't choose an opponent this way, tap ~. */

/* Curse of Bounty	|1|G
 * Enchantment - Aura Curse
 * Enchant player
 * Whenever enchanted player is attacked, untap all nonland permanents you control. Each opponent attacking that player untaps all nonland permanents they control. */

/* Hungry Lynx	|1|G
 * Creature - Cat 2/2
 * Cats you control have protection from Rats.
 * At the beginning of your end step, target opponent creates a 1/1 |Sblack Rat creature token with deathtouch.
 * Whenever a Rat dies, put a +1/+1 counter on each Cat you control. */

/* Kindred Summons	|5|G|G
 * Instant
 * Choose a creature type. Reveal cards from the top of your library until you reveal X creature cards of the chosen type, where X is the number of creatures you control of that type. Put those cards onto the battlefield, then shuffle the rest of the revealed cards into your library. */

/* ManalinkEh 2022: 0x228ee90; original cards/commander_2017.c:570. */
int card_qasali_slingers(int player, int card, event_t event)
{
  /* Qasali Slingers	|4|G
   * Creature - Cat Warrior 3/5
   * Reach
   * Whenever ~ or another Cat enters the battlefield under your control, you may destroy target artifact or enchantment. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t test;
  default_test_definition(&test, 2);
  test.subtype = 0x1f;
  result = duh_mode(player);
  etb_ability_global(player, event, (result != 0) + 1, TYPE_CREATURE, &test, player, card);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 0x44);
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "DISENCHANT");
      if (result != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
    }
    instance->number_of_targets = '\0';
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x228efb0; original cards/commander_2017.c:597. */
int card_traverse_the_outlands(int player, int card, event_t event)
{
  /* Traverse the Outlands	|4|G
   * Sorcery
   * Search your library for up to X basic land cards, where X is the greatest power among creatures you control. Put those cards onto the battlefield tapped, then shuffle your library. */

  int result;
  test_definition_t this_test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = get_highest_power(player);
    if (0 < result)
    {
      default_test_definition(&this_test, 1);
      this_test.subtype = 0x2000;
      this_test.qty = (unsigned short)result;
      new_global_tutor(player, player, 1, 5, 0, 5, &this_test);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x228f070; original cards/commander_2017.c:618. */
int card_arahbo_roar_of_the_world(int player, int card, event_t event)
{
  /* Arahbo, Roar of the World	|3|G|W
   * Legendary Creature - Cat Avatar 5/5
   * Eminence - At the beginning of combat on your turn, if ~ is in the command zone or on the battlefield, another target Cat you control gets +3/+3 until end of turn.
   * Whenever another Cat you control attacks, you may pay |1|G|W. If you do, it gains trample and gets +X/+X until end of turn, where X is its power. */

  check_legend_rule(player, card, event);
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CHANGE_TYPE)
  {
    if (!engine_any_legal_attacker(player))
      event_flags |= 0x1000000;
  }
  else if (event == EVENT_BEGIN_COMBAT)
  {
    if (current_turn == player && !is_humiliated(player, card, AL_TRIG))
    {
      target_definition_t td;
      default_target_definition(player, card, &td, TYPE_CREATURE);
      td.required_subtype = SUBTYPE_CAT;
      td.preferred_controller = player;
      td.allowed_controller = player;
      td.special = TARGET_SPECIAL_NOT_ME;
      instance->number_of_targets = 0;
      if (can_target(&td) && new_pick_target(&td, "Select target Cat you control", 0, 0x81))
        pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                                3, 3, 0, 0);
    }
  }
  else if (event == EVENT_DECLARE_ATTACKERS && !is_humiliated(player, card, AL_TRIG))
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_CREATURE);
    test.subtype = SUBTYPE_CAT;
    test.not_me = 1;
    store_attackers(player, card, event,
                    (duh_mode(player) != 0) +
                        (DAT_STORE_IN_INFO_SLOT | DAT_TRACK | DAT_RESOLVE_TRIGGER_OPTIONAL),
                    player, -1, &test);
  }
  int count = resolve_declare_attackers_trigger(player, card, event,
      (duh_mode(player) != 0) + (DAT_STORE_IN_INFO_SLOT | DAT_TRACK | DAT_RESOLVE_TRIGGER_OPTIONAL));
  if (count && has_mana_multi(player, MANACOST_XGW(1, 1, 1)))
  {
    const uint8_t *attackers = (const uint8_t *)&instance->targets + 2 * sizeof(target_t);
    for (int index = count - 1; index >= 0; --index)
      if (in_play(current_turn, attackers[index]) && has_mana_multi(player, MANACOST_XGW(1, 1, 1)))
      {
        card_instance_t *attacker = get_card_instance(current_turn, attackers[index]);
        attacker->token_status |= STATUS_RED_BORDER;
        if (charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER,
                                               player, MANACOST_XGW(1, 1, 1)))
        {
          int power = get_power(current_turn, attackers[index]);
          pump_ability_until_eot(player, card, current_turn, attackers[index], power, power,
                                  KEYWORD_TRAMPLE, 0);
        }
        attacker = get_card_instance(current_turn, attackers[index]);
        attacker->token_status &= ~STATUS_RED_BORDER;
      }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x228f460; original cards/commander_2017.c:670. */
int card_edgar_markov(int player, int card, event_t event)
{
  /* Edgar Markov	|3|R|W|B
   * Legendary Creature - Vampire Knight 4/4
   * Eminence - Whenever you cast another Vampire spell, if ~ is in the command zone or on the battlefield, create a 1/1 |Sblack Vampire creature token.
   * First strike, haste
   * Whenever ~ attacks, put a +1/+1 counter on each Vampire you control. */

  token_generation_t token;

  check_legend_rule(player, card, event);
  haste(player, card, event);
  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    test_definition_t test;
    default_test_definition(&test, 0x47);
    test.subtype = 0xcd;
    sp_ability_trigger(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event != EVENT_SPA)
  {
    test_definition_t test2;
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
    if (event != EVENT_DAT_ABILITY)
    {
      return 0;
    }
    default_test_definition(&test2, 0x47);
    test2.subtype = 0xcd;
    new_manipulate_all(player, card, player, &test2,
                       ACT_ADD_COUNTERS_BASE | ACT_KILL_BURY);
    return 0;
  }
  default_token_definition(player, card, CARD_ID_VAMPIRE, &token);
  token.tou = 1;
  token.pow = 1;
  generate_token(&token);
  dat_ability(player, card, EVENT_SPA, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  return 0;
}

/* ManalinkEh 2022: 0x228f600; original cards/commander_2017.c:706. */
int card_fractured_identity(int player, int card, event_t event)
{
  /* Fractured Identity	|3|W|U
   * Sorcery
   * Exile target nonland permanent. Each player other than its controller creates a token that's a copy of it. */

  card_instance_t *instance;
  int result;
  token_generation_t token;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.illegal_type = TYPE_LAND;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      copy_token_definition(player, card, &token, instance->targets[0].player,
                            instance->targets[0].card);
      result = instance->targets[0].player;
      token.t_player = 1 - result;
      kill_card(result, instance->targets[0].card, KILL_EXILE);
      generate_token(&token);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_NONLAND_PERMANENT", 1, NULL);
}

/* ManalinkEh 2022: 0x228f7b0; original cards/commander_2017.c:736. */
int card_inalla_archmage_ritualist(int player, int card, event_t event)
{
  /* Inalla, Archmage Ritualist	|2|U|B|R
   * Legendary Creature - Human Wizard 4/5
   * Eminence - Whenever another nontoken Wizard enters the battlefield under your control, if ~ is in the command zone or on the battlefield, you may pay |1. If you do, create a token that's a copy of that Wizard. The token gains haste. Exile it at the beginning of the next end step.
   * Tap five untapped Wizards you control: Target player loses 7 life. */

  card_instance_t *instance;
  int result;
  target_definition_t td1;

  target_definition_t td2;

  check_legend_rule(player, card, event);
  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.subtype = SUBTYPE_WIZARD;
    test.not_me = 1;
    test.type_flag = F1_NO_TOKEN;
    result = duh_mode(player);
    etb_ability_global(player, event, (result != 0) + 1, TYPE_NONE, &test,
                       player, card);
  }
  if (event == EVENT_ETB)
  {
    result = has_mana(player, 0, 1);
    if (result != 0)
    {
      result = charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player,
                                           COLOR_COLORLESS, 1);
      if (result != 0)
      {
        token_generation_t token;
        copy_token_definition(player, card, &token, trigger_cause_controller,
                              trigger_cause);
        token.special_code_on_generation =
            haste_and_remove_token_at_end_of_next_turn;
        generate_token(&token);
      }
    }
  }
  else if (((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
           ((event == EVENT_RESOLVE_ACTIVATION || (event == EVENT_CLEANUP))))
  {
    default_target_definition(player, card, &td1, 0x47);
    td1.illegal_state = TARGET_STATE_TAPPED;
    td1.required_subtype = 0xd6;
    td1.preferred_controller = player;
    td1.allowed_controller = player;
    td1.illegal_abilities = 0;
    default_target_definition(player, card, &td2, 2);
    td2.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = valid_target(&td2);
      if (result != 0)
      {
        lose_life(instance->targets[0].player, 7);
      }
    }
    result = generic_activated_ability_with_tapsubtype_as_cost(
        player, card, event, GAA_CAN_TARGET, 0, 0, 0, 0, 0, 0, 0, &td2,
        "TARGET_PLAYER", &td1, 5);
    return result;
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x228fa80; original cards/commander_2017.c:792. */
int card_kess_dissident_mage(int player, int card, event_t event)
{
  /* Kess, Dissident Mage	|1|U|B|R
   * Legendary Creature - Human Wizard 3/4
   * Flying
   * During each of your turns, you may cast an instant or sorcery card from your graveyard. If a card cast this way would be put into your graveyard, exile it instead. */

  int iid_or_result;
  card_instance_t *instance;
  int grave_index_or_available;
  int *grave_cursor;
  int playable_count;
  test_definition_t test;
  int iids[500];
  int positions[500];
  check_legend_rule(player, card, event);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if ((((current_turn == player) &&
          (instance = get_card_instance(player, card), instance->info_slot != 0x42)) &&
         (instance = in_play(player, card), instance != NULL)) &&
        (iid_or_result = is_humiliated(player, card, AL_STATIC), iid_or_result == 0))
    {
      grave_cursor = graveyard_ptr_mutable[player];
      iid_or_result = *grave_cursor;
      grave_index_or_available = 0;
      if (iid_or_result != -1)
      {
        while ((((cards_data[iid_or_result].type & 0x38) == 0 ||
                 (iid_or_result = has_mana_to_cast_iid(player, EVENT_CAN_ACTIVATE, iid_or_result),
                  iid_or_result == 0)) ||
                ((iid_or_result = is_suspend_only_card(-1, *grave_cursor),
                  iid_or_result != 0 ||
                      ((iid_or_result =
                            can_legally_play_iid_now(player, *grave_cursor, EVENT_CAN_ACTIVATE),
                        iid_or_result == 0 ||
                            (grave_index_or_available = iid_or_result, iid_or_result != 99)))))))
        {
          iid_or_result = grave_cursor[1];
          grave_cursor = grave_cursor + 1;
          if (iid_or_result == -1)
          {
            return grave_index_or_available;
          }
        }
        return 99;
      }
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    grave_cursor = graveyard_ptr_mutable[player];
    playable_count = 0;
    iid_or_result = *grave_cursor;
    if (iid_or_result != -1)
    {
      grave_index_or_available = 0;
      do
      {
        if (((((cards_data[iid_or_result].type & 0x38) != 0) &&
              (iid_or_result = has_mana_to_cast_iid(player, EVENT_ACTIVATE, iid_or_result),
               iid_or_result != 0)) &&
             (iid_or_result = is_suspend_only_card(-1, grave_cursor[grave_index_or_available]),
              iid_or_result == 0)) &&
            (iid_or_result = can_legally_play_iid_now(
                 player, grave_cursor[grave_index_or_available], EVENT_ACTIVATE),
             iid_or_result != 0))
        {
          iids[playable_count] = grave_cursor[grave_index_or_available];
          positions[playable_count] = grave_index_or_available;
          playable_count = playable_count + 1;
        }
        grave_index_or_available = grave_index_or_available + 1;
        iid_or_result = grave_cursor[grave_index_or_available];
      } while (iid_or_result != -1);
    }
    default_test_definition(&test, 0);
    iid_or_result = select_card_from_zone(player, player, iids, playable_count, 0, 1, -1, &test);
    if ((iid_or_result == -1) ||
        (grave_index_or_available = charge_mana_from_id(
             player, -1, EVENT_ACTIVATE, (unsigned int)cards_data[iids[iid_or_result]].id),
         grave_index_or_available == 0))
    {
      spell_fizzled = 1;
    }
    else
    {
      play_card_in_grave_for_free_and_exile_it(player, player, positions[iid_or_result]);
      cant_be_responded_to = 1;
      instance = get_card_instance(player, card);
      instance->info_slot = 0x42;
    }
  }
  else if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x228fd70; original cards/commander_2017.c:861. */
int card_licia_sanguine_tribune(int player, int card, event_t event)
{
  /* Licia, Sanguine Tribune	|5|R|W|B
   * Legendary Creature - Vampire Soldier 4/4
   * This spell costs |1 less to cast for each 1 life you gained this turn.
   * First strike, lifelink
   * Pay 5 life: Put three +1/+1 counters on ~. Activate this ability only during your turn and only once each turn. */

  int result;

  check_legend_rule(player, card, event);
  lifelink(player, card, event);
  if (event == EVENT_MODIFY_COST)
  {
    result = get_trap_condition(player, 1);
    COST_COLORLESS = COST_COLORLESS - (char)result;
    return 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
    add_counters(player, card, player, card, COUNTER_P1_P1, 3);
  return generic_activated_ability(player, card, event, 0x12000, 0, 0, 0, 0, 0, 0, 5, NULL, NULL);
}

/* Mairsil, the Pretender	|1|U|B|R
 * Legendary Creature - Human Wizard 4/4
 * When ~ enters the battlefield, you may exile an artifact or creature card from your hand or graveyard and put a cage counter on it.
 * ~ has all activated abilities of all cards you own in exile with cage counters on them. You may activate each of those abilities only once each turn. */

/* Mathas, Fiend Seeker	|R|W|B
 * Legendary Creature - Vampire 3/3
 * Menace
 * At the beginning of your end step, put a bounty counter on target creature an opponent controls. For as long as that creature has a bounty counter on it, it has "When this creature dies, each opponent draws a card and gains 2 life." */

/* Mirri, Weatherlight Duelist	|1|G|W
 * Legendary Creature - Cat Warrior 3/2
 * First strike
 * Whenever ~ attacks, each opponent can't block with more than one creature this combat.
 * As long as ~ is tapped, no more than one creature can attack you each combat. */

/* Nazahn, Revered Bladesmith	|4|G|W
 * Legendary Creature - Cat Artificer 5/4
 * When ~ enters the battlefield, search your library for an Equipment card and reveal it. If you reveal a card named Hammer of ~ this way, put it onto the battlefield. Otherwise, put that card into your hand. Then shuffle your library.
 * Whenever an equipped creature you control attacks, you may tap target creature defending player controls. */

/* O-Kagachi, Vengeful Kami	|1|W|U|B|R|G
 * Legendary Creature - Dragon Spirit 6/6
 * Flying, trample
 * Whenever ~ deals combat damage to a player, if that player attacked you during their last turn, exile target nonland permanent that player controls. */

/* Taigam, Ojutai Master	|2|W|U
 * Legendary Creature - Human Monk 3/4
 * Instant, sorcery, and Dragon spells you control can't be countered.
 * Whenever you cast an instant or sorcery spell from your hand, if ~ attacked this turn, that spell gains rebound. */

/* ManalinkEh 2022: 0x228fe90; original cards/commander_2017.c:916. */
int card_taigam_sidisis_hand(int player, int card, event_t event)
{
  /* Taigam, Sidisi's Hand	|3|U|B
   * Legendary Creature - Human Wizard 3/4
   * Skip your draw step.
   * At the beginning of your upkeep, look at the top three cards of your library. Put one of them into your hand and the rest into your graveyard.
   * |B, |T, Exile X cards from your graveyard: Target creature gets -X/-X until end of turn. */

  int t_card;
  int t_player;
  card_instance_t *instance;
  unsigned int must_select;
  int result;
  int amount;
  int activation_result;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  skip_your_draw_step(player, event);
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    select_one_and_mill_the_rest(player, player, 3, 0xff);
    return 0;
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    activation_result = generic_activated_ability(player, card, event, 4, 0, 1, 0, 0, 0, 0, 0, &td,
                                                  "TARGET_CREATURE");
    if ((event == EVENT_CAN_ACTIVATE) && (activation_result != 0))
    {
      result = count_graveyard(player);
      if (result < 1)
      {
        return 0;
      }
      return activation_result;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    activation_result = generic_activated_ability(player, card, event, 4, 0, 1, 0, 0, 0, 0, 0, &td,
                                                  "TARGET_CREATURE");
  }
  if (event == EVENT_ACTIVATE)
  {
    if (spell_fizzled != 1)
    {
      new_default_test_definition(&test, 0xff, "SELECT_CARD");
      amount = 0;
      while (true)
      {
        if (ai_is_speculating == 1)
        {
          must_select = (unsigned int)(amount == 0);
        }
        else
        {
          must_select = 0;
          if ((player == 1) && (((unsigned char)trace_mode & 2) == 0))
          {
            must_select = (unsigned int)(amount == 0);
          }
        }
        result = new_global_tutor(player, player, 5, 6, must_select, 3, &test);
        if (result < 0)
          break;
        amount = amount + 1;
      }
      if (amount == 0)
      {
        spell_fizzled = 1;
      }
      else
      {
        instance = get_card_instance(player, card);
        instance->info_slot = amount;
      }
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = instance->info_slot;
    instance = get_card_instance(player, card);
    t_card = instance->targets[0].card;
    instance = get_card_instance(player, card);
    t_player = instance->targets[0].player;
    instance = get_card_instance(player, card);
    pump_until_eot(player, instance->parent_card, t_player, t_card, -result, -result);
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
  }
  return activation_result;
}

/* ManalinkEh 2022: 0x22901f0; original cards/commander_2017.c:970. */
int card_the_ur_dragon(int player, int card, event_t event)
{
  /* The Ur-Dragon	|4|W|U|B|R|G
   * Legendary Creature - Dragon Avatar 10/10
   * Eminence - As long as ~ is in the command zone or on the battlefield, other Dragon spells you cast cost |1 less to cast.
   * Flying
   * Whenever one or more Dragons you control attack, draw that many cards, then you may put a permanent card from your hand onto the battlefield. */

  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player &&
      affected_card != card && is_what(player, affected_card, TYPE_CREATURE) &&
      has_subtype(affected_card_controller, affected_card, SUBTYPE_DRAGON))
    --COST_COLORLESS;

  test_definition_t test;
  default_test_definition(&test, TYPE_CREATURE);
  test.subtype = SUBTYPE_DRAGON;
  int count = declare_attackers_trigger_test(player, card, event, DAT_TRACK, player, -1, &test);
  if (count)
  {
    card_instance_t *instance = get_card_instance(player, card);
    const uint8_t *attackers = (const uint8_t *)&instance->targets + 2 * sizeof(target_t);
    for (int index = count - 1; index >= 0; --index)
      if (in_play(player, attackers[index]))
      {
        // The release draws/tutors once per surviving tracked attacker.
        draw_cards(player, count);
        test_definition_t permanent;
        new_default_test_definition(&permanent, TYPE_PERMANENT, "Select a permanent card.");
        new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &permanent);
      }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22903a0; original cards/commander_2017.c:1009. */
int card_wasitora_nekoru_queen(int player, int card, event_t event)
{
  /* Wasitora, Nekoru Queen	|2|B|R|G
   * Legendary Creature - Cat Dragon 5/4
   * Flying, trample
   * Whenever ~ deals combat damage to a player, that player sacrifices a creature. If the player can't, you create a 3/3 |Sblack, |Sred, and |Sgreen Cat Dragon creature token with flying. */

  int result;
  card_instance_t *instance;
  unsigned int player_pass;
  int times_damaged[2];
  token_generation_t token;
  test_definition_t test;
  check_legend_rule(player, card, event);
  result = damage_dealt_by_me(player, card, event,
                              DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                                  DDBM_MUST_BE_COMBAT_DAMAGE);
  if (result != 0)
  {
    new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
    instance = get_card_instance(player, card);
    times_damaged[0] = (int)(unsigned char)instance->targets[1].player;
    instance = get_card_instance(player, card);
    times_damaged[1] = (int)SET_BYTE1(instance->targets[1].player);
    instance = get_card_instance(player, card);
    player_pass = 0;
    instance->targets[1].player = 0;
    do
    {
      if ((times_damaged[current_turn ^ player_pass] != 0) &&
          (result = new_sacrifice(player, card, current_turn ^ player_pass,
                                  SAC_RETURN_CHOICE | SAC_CAUSED | SAC_NO_CANCEL, &test),
           result == 0))
      {
        default_token_definition(player, card, CARD_ID_CAT_DRAGON, &token);
        generate_token(&token);
      }
      player_pass = player_pass + 1;
    } while (player_pass != 2);
  }
  return 0;
}

/* ManalinkEh 2022: 0x22904e0; original cards/commander_2017.c:1040. */
int card_bloodforged_battle_axe(int player, int card, event_t event)
{
  /* Bloodforged Battle-Axe	|1
   * Artifact - Equipment
   * Equipped creature gets +2/+0.
   * Whenever equipped creature deals combat damage to a player, create a token that's a copy of ~.
   * Equip |2 */

  int result;
  result = equipped_creature_deals_damage(player, card, event,
                                          DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE);
  if (result != 0)
  {
    copy_token(player, card, player, card);
  }
  result = vanilla_equipment(player, card, event, 2, 2, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x2290570; original cards/commander_2017.c:1053. */
int card_hammer_of_nazahn(int player, int card, event_t event)
{
  /* Hammer of Nazahn	|4
   * Legendary Artifact - Equipment
   * Whenever ~ or another Equipment enters the battlefield under your control, you may attach that Equipment to target creature you control.
   * Equipped creature gets +2/+0 and has indestructible.
   * Equip |4 */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  check_legend_rule(player, card, event);
  if ((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (reason_for_trigger_controller == player))
  {
    result = is_equipment(trigger_cause_controller, trigger_cause);
    if (result != 0)
    {
      etb_ability_global(player, event, 2, TYPE_PERMANENT, NULL, player, card);
    }
  }
  if (event == EVENT_ETB)
  {
    default_target_definition(trigger_cause_controller, trigger_cause, &td, 2);
    td.preferred_controller = player;
    td.allowed_controller = player;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "Attaching equipment: select target creature.", 0, 0x80);
      if (result != 0)
      {
        instance = get_card_instance(trigger_cause_controller, trigger_cause);
        equip_target_creature(trigger_cause_controller, trigger_cause, instance->targets[0].player,
                              instance->targets[0].card);
        instance->number_of_targets = '\0';
      }
    }
  }
  result = vanilla_equipment(player, card, event, 4, 2, 0, 0, 0x800);
  return result;
}

/* Heirloom Blade	|3
 * Artifact - Equipment
 * Equipped creature gets +3/+1.
 * Whenever equipped creature dies, you may reveal cards from the top of your library until you reveal a creature card that shares a creature type with it. Put that card into your hand and the rest on the bottom of your library in a random order.
 * Equip |1 */

/* Herald's Horn	|3
 * Artifact
 * As ~ enters the battlefield, choose a creature type.
 * Creature spells you cast of the chosen type cost |1 less to cast.
 * At the beginning of your upkeep, look at the top card of your library. If it's a creature card of the chosen type, you may reveal it and put it into your hand. */

/* ManalinkEh 2022: 0x2290700; original cards/commander_2017.c:1096. */
int card_mirror_of_the_forebears(int player, int card, event_t event)
{
  /* Mirror of the Forebears	|2
   * Artifact
   * As ~ enters the battlefield, choose a creature type.
   * |1: Until end of turn, ~ becomes a copy of target creature you control of the chosen type, except it's an artifact in addition to its other types. */

  int src_card;
  card_instance_t *instance;
  int parent_player_or_result;
  card_instance_t *source_in_play;
  target_definition_t td;

  if (event != EVENT_RESOLVE_SPELL)
  {
    if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
         (event == EVENT_RESOLVE_ACTIVATION)) ||
        (event == EVENT_CLEANUP))
    {
      default_target_definition(player, card, &td, 2);
      instance = get_card_instance(player, card);
      td.required_subtype = instance->info_slot;
      if (event == EVENT_RESOLVE_ACTIVATION)
      {

        instance = get_card_instance(player, card);
        parent_player_or_result = instance->parent_controller;
        src_card = instance->parent_card;
        if (valid_target(&td) != 0)
        {
          source_in_play = in_play(player, card);
          if (source_in_play != NULL)
          {
            shapeshift_target(parent_player_or_result, src_card, parent_player_or_result, src_card,
                              instance->targets[0].player, instance->targets[0].card, 1);
          }
        }
      }
      parent_player_or_result = generic_activated_ability(player, card, event, 4, 1, 0, 0, 0, 0, 0,
                                                          0, &td, "TARGET_CREATURE");
    }
    else
    {
      parent_player_or_result = 0;
    }
    return parent_player_or_result;
  }
  instance = get_card_instance(player, card);
  parent_player_or_result = select_a_subtype(player, card);
  instance->info_slot = parent_player_or_result;
  return 0;
}

/* ManalinkEh 2022: 0x22908a0; original cards/commander_2017.c:1127. */
int card_ramos_dragon_engine(int player, int card, event_t event)
{
  /* Ramos, Dragon Engine	|6
   * Legendary Artifact Creature - Dragon 4/4
   * Flying
   * Whenever you cast a spell, put a +1/+1 counter on ~ for each of that spell's colors.
   * Remove five +1/+1 counters from ~: Add |W|W|U|U|B|B|R|R|G|G. Activate this ability only once each turn. */

  int result;

  check_legend_rule(player, card, event);
  sp_ability_trigger(player, event, 2, TYPE_ANY, NULL, player, card);
  if (event == EVENT_SPA)
  {
    result = count_colors(trigger_cause_controller, trigger_cause);
    add_counters(player, card, player, card, COUNTER_P1_P1, result);
  }
  else
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x2000, 0, 0, 0, 0, 0, 0,
                                         0x1000500, NULL, NULL);
      return result;
    }
    if ((event == EVENT_ACTIVATE) &&
        (result = generic_activated_ability(player, card, EVENT_ACTIVATE, 0x2000, 0, 0, 0, 0, 0, 0,
                                            0x1000500, NULL, NULL),
         result != 0))
    {
      produce_mana_multi(player, 0, 2, 2, 2, 2, 2);
      tapped_for_mana_color = -2;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x228d2e0; original cards/commander_2017.c:1158. */
static int path_of_ancestry_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if (event == EVENT_CAST_SPELL)
  {
    if (((((card != affected_card) || (affected_card_controller != player)) &&
          (result = played_for_free(affected_card_controller, affected_card), result == 0)) &&
         (((result = is_token(affected_card_controller, affected_card),
            result == 0 && (result = check_special_flags2(affected_card_controller, affected_card,
                                                          -0x80000000),
                            result == 0)) &&
           ((affected_card_controller == player &&
             ((result = is_what(player, affected_card, 2),
               result != 0 && (0 < mana_paid[instance->targets[0].player])))))))) &&
        ((result = shares_creature_subtype(-1, instance->targets[2].player,
                                           affected_card_controller, affected_card),
          result != 0 || ((result = instance->targets[2].card,
                           -1 < result && (result = shares_creature_subtype(
                                               -1, result, affected_card_controller, affected_card),
                                           result != 0))))))
    {
      scry(player, 1);
      kill_card(player, card, KILL_EXILE);
    }
  }
  else if (((event == EVENT_STATIC_EFFECTS) && (stack_size == 0)) &&
           (mana_pool[player][instance->targets[0].player] < 1))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2290a90; original cards/commander_2017.c:1186. */
int card_path_of_ancestry(int player, int card, event_t event)
{
  /* Path of Ancestry	""
   * Land
   * ~ enters the battlefield tapped.
   * |T: Add one mana of any color in your commander's color identity. When that mana is spent to cast a creature spell that shares a creature type with your commander, scry 1. */

  card_instance_t *instance;
  int scan_iid_or_legacy;
  card_instance_t *edh_or_legacy;
  unsigned int commander_colors;

  instance = get_card_instance(player, card);
  if (is_humiliated(player, card, AL_TRIG) == 0)
  {
    comes_into_play_tapped(player, card, event);
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    bool commander_found = false;
    if (0 < active_cards_count[player])
    {
      scan_iid_or_legacy = 0;
      do
      {
        edh_or_legacy = in_play(player, scan_iid_or_legacy);
        if ((edh_or_legacy != NULL) && (get_id(player, scan_iid_or_legacy) == CARD_ID_ELDER_DRAGON_HIGHLANDER))
        {
          edh_or_legacy = get_card_instance(player, scan_iid_or_legacy);
          scan_iid_or_legacy = edh_or_legacy->targets[2].player;
          instance->targets[2].player = scan_iid_or_legacy;
          instance->info_slot = (unsigned int)cards_data[scan_iid_or_legacy].color;
          commander_colors =
              get_color_from_remainder_text((unsigned int)cards_data[scan_iid_or_legacy].id);
          commander_colors = commander_colors | instance->info_slot;
          scan_iid_or_legacy = edh_or_legacy->targets[2].card;
          instance->info_slot = commander_colors;
          if (-1 < scan_iid_or_legacy)
          {
            instance->info_slot = commander_colors | cards_data[scan_iid_or_legacy].color;
            commander_colors =
                get_color_from_remainder_text((unsigned int)cards_data[scan_iid_or_legacy].id);
            commander_colors = commander_colors | instance->info_slot;
            instance->info_slot = commander_colors;
            instance->targets[2].card = edh_or_legacy->targets[2].card;
          }
          commander_found = true;
          break;
        }
        scan_iid_or_legacy = scan_iid_or_legacy + 1;
      } while (scan_iid_or_legacy < active_cards_count[player]);
    }
    if (!commander_found)
      commander_colors = instance->info_slot;
    play_land_sound_effect_force_color(player, card, commander_colors);
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    event = EVENT_CAN_ACTIVATE;
  }
  else if ((event == EVENT_ACTIVATE) && (0 < instance->info_slot))
  {
    mana_producer(player, card, EVENT_ACTIVATE);
    scan_iid_or_legacy = create_legacy_effect(player, card, path_of_ancestry_legacy);
    edh_or_legacy = get_card_instance(player, scan_iid_or_legacy);
    edh_or_legacy->targets[0].player = tapped_for_mana_color;
    edh_or_legacy = get_card_instance(player, scan_iid_or_legacy);
    edh_or_legacy->targets[2].player = instance->targets[2].player;
    edh_or_legacy = get_card_instance(player, scan_iid_or_legacy);
    edh_or_legacy->targets[2].card = instance->targets[2].card;
    add_status(player, scan_iid_or_legacy, 0x10000);
    return 0;
  }
  return mana_producer(player, card, event);
}
#endif
