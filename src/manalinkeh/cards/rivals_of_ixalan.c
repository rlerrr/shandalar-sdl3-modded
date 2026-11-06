#include "manalink.h"

/*********
 * White *
 *********/

// Divine Verdict - reprint from Magic 2010

// Legion Conquistador - reprint from Ixalan

// Raptor Companion - reprint from Ixalan

/********
 * Blue *
 ********/

// Negate - reprint from Morningtide

// Sailor of Means - reprint from Ixalan

// Silvergill Adept - reprint from Lorwyn

/*********
 * Black *
 ********/

// Recover - reprint from Invasion

/*******
 * Red *
 *******/

// Shatter - reprint from Alpha

/*********
 * Green *
 *********/

// Aggressive Urge - reprint from Invasion

// Colossal Dreadmaw - reprint from Ixalan

// Hunt the Weak - reprint from Magic 2014

// Naturalize - reprint from Onslaught

// Plummet - reprint from Magic 2011

/********
 * Gold *
 ********/

/************
 * Artifact *
 ************/

// Strider Harness - reprint from Scars of Mirrodin

// Traveler's Amulet - reprint from Innistrad

/********
 * Land *
 ********/

// Evolving Wilds - reprint from Rise of the Eldrazi

// Forsaken Sanctuary - reprint from Shadows over Innistrad

// Foul Orchard - reprint from Shadows over Innistrad

// Highland Lake - reprint from Shadows over Innistrad

// Stone Quarry - reprint from Shadows over Innistrad

// Woodland Stream - reprint from Shadows over Innistrad

// Plains, Island, Swamp, Mountain, Forest

/**********************
 * Planeswalker Decks *
 **********************/

// Cinder Barrens - reprint from Oath of the Gatewatch

static int has_the_citys_blessing(int player);
static void ascend(int player, int card, event_t event);

static int pitiless_plunderer_effect(int player, int card, event_t event);

static int golden_guardian_legacy(int player, int card, event_t event);

#ifndef SHANDALAR

static void remove_token_at_end_of_next_turn_unless_cb(token_generation_t *token, int beast,
                                                       int number);

#endif

/* ManalinkEh 2022: 0x218f4e0; original cards/rivals_of_ixalan.c:5. */
static void ascend(int player, int card, event_t event)
{
  int type;
  if (event == EVENT_STATIC_EFFECTS)
    type = 0x47;
  else if (event == EVENT_RESOLVE_SPELL)
    type = 0x38;
  else
    return;
  if (is_what(player, card, type) && count_permanents_by_type(player, TYPE_PERMANENT) > 9)
  {
    bool found = false;
    for (int i = 0; i < active_cards_count[player]; ++i)
      if (in_play(player, i) && get_id(player, i) == CARD_ID_THE_CITYS_BLESSING)
        found = true;
    if (!found)
      generate_reserved_token_by_id(player, CARD_ID_THE_CITYS_BLESSING);
  }
}

/* ManalinkEh 2022: 0x218ed50; original cards/rivals_of_ixalan.c:25. */
static int has_the_citys_blessing(int player)
{
  for (int card = 0; card < active_cards_count[player]; ++card)
    if (in_play(player, card) && get_id(player, card) == CARD_ID_THE_CITYS_BLESSING)
      return card;
  return 0;
}

/* ManalinkEh 2022: 0x218f700; original cards/rivals_of_ixalan.c:39. */
int card_baffling_end(int player, int card, event_t event)
{
  /* Baffling End	|1|W
   * Enchantment
   * When ~ enters the battlefield, exile target creature an opponent controls with converted mana cost 3 or less.
   * When ~ leaves the battlefield, target opponent creates a 3/3 |Sgreen Dinosaur creature token with trample. */

  card_instance_t *instance;
  int result;


  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  instance = get_card_instance(player, card);
  if (event == EVENT_ETB)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    td.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
    td.extra = 3;
    td.allow_cancel = 0;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(
          &td,
          "Select target creature an opponent controls with converted mana cost 3 or less.", 0,
          0x80);
      if (result != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      }
    }
    leaves_play(player, card, EVENT_ETB, 1, 1);
  }
  else
  {
    leaves_play(player, card, event, 1, 1);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      target_opponent(player, card);
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_DINOSAUR, &token);
      token.t_player = instance->targets[0].player;
      generate_token(&token);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* Bishop of Binding	|3|W
 * Creature - Vampire Cleric 1/1
 * When ~ enters the battlefield, exile target creature an opponent controls until ~ leaves the battlefield.
 * Whenever ~ attacks, target Vampire gets +X/+X until end of turn, where X is the power of the exiled card. */

/* Blazing Hope	|W
 * Instant
 * Exile target creature with power greater than or equal to your life total. */

/* Cleansing Ray	|1|W
 * Sorcery
 * Choose one -
 * * Destroy target Vampire.
 * * Destroy target enchantment. */

/* Everdawn Champion	|1|W|W
 * Creature - Human Soldier 2/2
 * Prevent all combat damage that would be dealt to ~. */

/* Exultant Skymarcher	|1|W|W
 * Creature - Vampire Soldier 2/3
 * Flying */

/* Famished Paladin	|1|W
 * Creature - Vampire Knight 3/3
 * ~ doesn't untap during your untap step.
 * Whenever you gain life, untap ~. */

/* ManalinkEh 2022: 0x218f890; original cards/rivals_of_ixalan.c:102. */
int card_forerunner_of_the_legion(int player, int card, event_t event)
{
  /* Forerunner of the Legion	|2|W
   * Creature - Vampire Knight 2/2
   * When ~ enters the battlefield, you may search your library for a Vampire card, reveal it, then shuffle your library and put that card on top of it.
   * Whenever another Vampire enters the battlefield under your control, target creature gets +1/+1 until end of turn. */

  int result;
  card_instance_t *instance;

  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    test_definition_t test;
    default_test_definition(&test, 0x47);
    test.subtype = 0xcd;
    test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    test_definition_t test2;
    if ((trigger_cause_controller != player) || (trigger_cause != card))
    {
      target_definition_t td;
      instance = get_card_instance(player, card);
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = (int32_t)(player);
      td.allow_cancel = 0;
      result = can_target(&td);
      if (result != 0)
      {
        result = pick_target(&td, "TARGET_CREATURE");
        if (result != 0)
        {
          pump_until_eot_merge_previous(player, card, instance->targets[0].player,
                                        instance->targets[0].card, 1, 1);
        }
      }
      spell_fizzled = 0;
      instance->number_of_targets = '\0';
      return 0;
    }
    new_default_test_definition(&test2, 0xff, "Select a Vampire card.");
    test2.subtype = 0xcd;
    new_global_tutor(player, player, 1, 2, 0, 1, &test2);
  }
  return 0;
}

/* Imperial Ceratops	|4|W
 * Creature - Dinosaur 3/5
 * Enrage - Whenever ~ is dealt damage, you gain 2 life. */

/* Luminous Bonds	|2|W
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature can't attack or block. */

/* Majestic Heliopterus	|3|W
 * Creature - Dinosaur 2/2
 * Flying
 * Whenever ~ attacks, another target Dinosaur you control gains flying until end of turn. */

/* Martyr of Dusk	|1|W
 * Creature - Vampire Soldier 2/1
 * When ~ dies, create a 1/1 |Swhite Vampire creature token with lifelink. */

/* Moment of Triumph	|W
 * Instant
 * Target creature gets +2/+2 until end of turn. You gain 2 life. */

/* Paladin of Atonement	|1|W
 * Creature - Vampire Knight 1/1
 * At the beginning of each upkeep, if you lost life last turn, put a +1/+1 counter on ~.
 * When ~ dies, you gain life equal to its toughness. */

/* ManalinkEh 2022: 0x218fa90; original cards/rivals_of_ixalan.c:171. */
int card_pride_of_conquerors(int player, int card, event_t event)
{
  /* Pride of Conquerors	|1|W
   * Instant
   * Ascend
   * Creatures you control get +1/+1 until end of turn. If you have the city's blessing, those creatures get +2/+2 until end of turn instead. */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {

    ascend(player, card, EVENT_RESOLVE_SPELL);
    result = has_the_citys_blessing(player);
    result = (result != 0) + 1;
    pump_creatures_until_eot_merge_pt(player, card, player, result, result, NULL);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x218fb20; original cards/rivals_of_ixalan.c:186. */
int card_radiant_destiny(int player, int card, event_t event)
{
  /* Radiant Destiny	|2|W
   * Enchantment
   * Ascend
   * As ~ enters the battlefield, choose a creature type.
   * Creatures you control of the chosen type get +1/+1. As long as you have the city's blessing, they also have vigilance. */

  card_instance_t *instance;
  int result;

  ascend(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (player == 1)
    {
      state_untargettable(1, card, 1);
      instance = get_card_instance(1, card);
      result = select_a_subtype(1, card);
      instance->info_slot = result;
      state_untargettable(1, card, 0);
    }
    else
    {
      instance = get_card_instance(player, card);
      result = select_a_subtype(player, card);
      instance->info_slot = result;
    }
    in_play(player, card);
  }
  else
  {
    instance = in_play(player, card);
    if (((instance != NULL) && ((uint32_t)(event - EVENT_POWER) < 3)) && (affected_card_controller == player))
    {
      instance = get_card_instance(player, card);
      result = has_creature_type(player, affected_card, instance->info_slot);
      if (result != 0)
      {
        if (event < EVENT_ABILITIES)
        {
          event_result = event_result + 1;
        }
        else
        {
          result = has_the_citys_blessing(player);
          if (result != 0)
          {
            vigilance(player, affected_card, EVENT_ABILITIES);
          }
        }
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* Sanguine Glorifier	|3|W
 * Creature - Vampire Cleric 3/3
 * When ~ enters the battlefield, put a +1/+1 counter on another target Vampire you control. */

/* ManalinkEh 2022: 0x218fc80; original cards/rivals_of_ixalan.c:232. */
int card_skymarcher_aspirant(int player, int card, event_t event)
{
  /* Skymarcher Aspirant	|W
   * Creature - Vampire Soldier 2/1
   * Ascend
   * ~ has flying as long as you have the city's blessing. */

  int result;

  ascend(player, card, event);
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = has_the_citys_blessing(player);
    if (result != 0)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        event_result = event_result | 0x20;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x218fcf0; original cards/rivals_of_ixalan.c:244. */
int card_slaughter_the_strong(int player, int card, event_t event)
{
  /* Slaughter the Strong	|1|W|W
   * Sorcery
   * Each player chooses any number of creatures they control with total power 4 or less, then sacrifices all other creatures they control. */

  int c;
  int result_or_power;
  card_instance_t *instance;
  card_instance_t *chosen_creature;
  unsigned int power_budget_or_pass;
  unsigned int chooser;
  unsigned int selection_pass;
  target_definition_t td;
  char marked[2][151];

  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {

    instance = get_card_instance(player, card);
    memset(marked, 0, sizeof(marked));

    selection_pass = 0;
    do
    {
      chooser = selection_pass ^ current_turn;

      power_budget_or_pass = 4;
      do
      {
        char msg[100];
        default_target_definition(player, card, &td, 2);
        td.power_requirement = power_budget_or_pass | 0x2000;
        td.allow_cancel = 2;
        td.illegal_abilities = 0;
        td.who_chooses = chooser;
        td.allowed_controller = chooser;
        td.preferred_controller = chooser;
        scnprintf(msg, 100, "Select a creature card with pow %d or less.", power_budget_or_pass);
        result_or_power = can_target(&td);
        if ((result_or_power == 0) ||
            (result_or_power = new_pick_target(&td, msg, 0, 0x80), result_or_power == 0))
          break;

        marked[instance->targets[0].player][instance->targets[0].card] = '\x01';
        chosen_creature =
            get_card_instance(instance->targets[0].player, instance->targets[0].card);
        chosen_creature->state = chosen_creature->state | 0x100000;
        result_or_power = get_power(instance->targets[0].player, instance->targets[0].card);
        power_budget_or_pass = power_budget_or_pass - result_or_power;
      } while (0 < (int)power_budget_or_pass);
      for (c = 0; c < active_cards_count[chooser]; ++c)
        if (marked[chooser][c])
        {
          chosen_creature = get_card_instance(chooser, c);
          chosen_creature->state &= 0xffefffff;
        }
      selection_pass = selection_pass + 1;
    } while (selection_pass != 2);

    power_budget_or_pass = 0;
    do
    {
      chooser = current_turn ^ power_budget_or_pass;
      if (can_sacrifice(player, chooser, 1, TYPE_CREATURE, SUBTYPE_INVALID) != 0)
      {
        test_definition_t this_test;
        default_test_definition(&this_test, 2);
        this_test.value_for_special_selection_function = (int32_t)(intptr_t)marked;
        this_test.special_selection_function = not_marked;
        new_manipulate_all(player, card, chooser, &this_test, ACT_KILL_SACRIFICE);
      }
      power_budget_or_pass = power_budget_or_pass + 1;
    } while (power_budget_or_pass != 2);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x2190030; original cards/rivals_of_ixalan.c:303. */
int card_snubhorn_sentry(int player, int card, event_t event)
{
  /* Snubhorn Sentry	|W
   * Creature - Dinosaur 0/3
   * Ascend
   * ~ gets +3/+0 as long as you have the city's blessing. */

  int result;

  ascend(player, card, event);
  if (((event == EVENT_POWER) && (card == affected_card)) && (player == affected_card_controller))
  {
    result = has_the_citys_blessing(player);
    if (result != 0)
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

/* Sphinx's Decree	|1|W
 * Sorcery
 * Each opponent can't cast instant or sorcery spells during that player's next turn. */

/* Squire's Devotion	|2|W
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +1/+1 and has lifelink.
 * When ~ enters the battlefield, create a 1/1 |Swhite Vampire creature token with lifelink. */

/* Sun Sentinel	|1|W
 * Creature - Human Soldier 2/2
 * Vigilance */

/* Sun-Crested Pterodon	|4|W
 * Creature - Dinosaur 2/5
 * Flying
 * ~ has vigilance as long as you control another Dinosaur. */

/* ManalinkEh 2022: 0x21900a0; original cards/rivals_of_ixalan.c:333. */
int card_temple_altisaur(int player, int card, event_t event)
{
  /* Temple Altisaur	|4|W
   * Creature - Dinosaur 3/4
   * If a source would deal damage to another Dinosaur you control, prevent all but 1 of that damage. */

  if (event != EVENT_PREVENT_DAMAGE)
    return 0;

  card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
  if (damage->internal_card_id != damage_card)
    return 0;

  int target_player = damage->damage_target_player;
  if (target_player < 0)
    return 0;
  int target_card = damage->damage_target_card;
  if (target_card >= 0 && target_card != card && target_player == player &&
      has_subtype(target_player, target_card, SUBTYPE_DINOSAUR) && damage->info_slot > 1)
    prevent_damage(affected_card_controller, affected_card, damage->info_slot - 1);
  return 0;
}

/* ManalinkEh 2022: 0x2190140; original cards/rivals_of_ixalan.c:355. */
int card_trapjaw_tyrant(int player, int card, event_t event)
{
  /* Trapjaw Tyrant	|3|W|W
   * Creature - Dinosaur 5/5
   * Enrage - Whenever ~ is dealt damage, exile target creature an opponent controls until ~ leaves the battlefield. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  return_from_oblivion(player, card, event);
  result = is_humiliated(player, card, AL_TRIG);
  if (result == 0)
  {
    result = damage_dealt_to_me_arbitrary(player, card, event, DDBM_0, player, card);
    if (result != 0)
    {

      default_target_definition(player, card, &td, 2);
      td.allowed_controller = 1 - player;
      td.allow_cancel = 0;
      instance = get_card_instance(player, card);
      result = can_target(&td);
      if (result != 0)
      {
        result = new_pick_target(&td, "TARGET_CREATURE", 0, 0);
        if (result != 0)
        {
          obliviation(player, card, instance->targets[0].player, instance->targets[0].card);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2190240; original cards/rivals_of_ixalan.c:378. */
int card_zetalpa_primal_dawn(int player, int card, event_t event)
{
  /* Zetalpa, Primal Dawn	|6|W|W
   * Legendary Creature - Elder Dinosaur 4/8
   * Flying, double strike, vigilance, trample, indestructible */

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  indestructible(player, card, event);
  return 0;
}

/* Admiral's Order	|1|U|U
 * Instant
 * Raid - If you attacked with a creature this turn, you may pay |U rather than pay this spell's mana cost.
 * Counter target spell. */

/* Aquatic Incursion	|3|U
 * Enchantment
 * When ~ enters the battlefield, create two 1/1 |Sblue Merfolk creature tokens with hexproof.
 * |3|U: Target Merfolk can't be blocked this turn. */

/* Crafty Cutpurse	|3|U
 * Creature - Human Pirate 2/2
 * Flash
 * When ~ enters the battlefield, each token that would be created under an opponent's control this turn is created under your control instead. */

/* Crashing Tide	|2|U
 * Sorcery
 * ~ has flash as long as you control a Merfolk.
 * Return target creature to its owner's hand.
 * Draw a card. */

/* ManalinkEh 2022: 0x2190290; original cards/rivals_of_ixalan.c:415. */
int card_curious_obsession(int player, int card, event_t event)
{
  /* Curious Obsession	|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+1 and has "Whenever this creature deals combat damage to a player, you may draw a card."
   * At the beginning of your end step, if you didn't attack with a creature this turn, sacrifice ~. */

  int result;
  card_instance_t *instance;

  result = is_humiliated(player, card, AL_TRIG);
  if ((result == 0) &&
      (instance = get_card_instance(player, card), -1 < instance->damage_target_player))
  {
    result = duh_mode(player);
    result = attached_creature_deals_damage(
        player, card, event,
        (result != 0) + (DDBM_MUST_DAMAGE_PLAYER | DDBM_REPORT_DAMAGE_DEALT |
                         DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_TRIGGER_OPTIONAL));
    if (result != 0)
    {
      draw_cards(player, 1);
    }
  }
  if (((current_turn == player) && (result = eot_trigger(player, card, event), result != 0)) &&
      (result = get_trap_condition(player, 0x606), result == 0))
  {
    kill_card(player, card, KILL_SACRIFICE);
  }
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
        return generic_aura(player, card, event, player, 1, 1, 0, 0, 0, 0, 0);
    }
    else if ((event == EVENT_RESOLVE_SPELL) ||
             ((event == EVENT_CAN_CHANGE_TARGET || (event == EVENT_CHANGE_TARGET))))
      return generic_aura(player, card, event, player, 1, 1, 0, 0, 0, 0, 0);
    if (((event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA) &&
        (((((2 < (uint32_t)(event - EVENT_POWER) && (event != EVENT_RESOLVE_MOVING_AURA)) &&
            (event != EVENT_CHANGE_TYPE)) &&
           ((event != EVENT_SET_COLOR && (event != EVENT_GET_SELECTED_CARD)))) &&
          (event != EVENT_STATIC_EFFECTS))))
    {
      return 0;
    }
  }
  return generic_aura(player, card, event, player, 1, 1, 0, 0, 0, 0, 0);
}

/* Deadeye Rig-Hauler	|3|U
 * Creature - Human Pirate 3/2
 * Raid - When ~ enters the battlefield, if you attacked with a creature this turn, you may return target creature to its owner's hand. */

/* Expel from Orazca	|1|U
 * Instant
 * Ascend
 * Return target nonland permanent to its owner's hand. If you have the city's blessing, you may put that permanent on top of its owner's library instead. */

/* ManalinkEh 2022: 0x2190490; original cards/rivals_of_ixalan.c:449. */
int card_flood_of_recollection(int player, int card, event_t event)
{
  /* Flood of Recollection	|U|U
   * Sorcery
   * Return target instant or sorcery card from your graveyard to your hand. Exile ~. */

  int result;
  test_definition_t this_test;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  new_default_test_definition(&this_test, 0x38, "Select a instant or sorcery spell.");
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = validate_target_from_grave_source(player, card, player, 0);
    if (result != -1)
    {
      from_grave_to_hand(player, result, 1);
    }
    kill_card(player, card, KILL_EXILE);
  }
  return generic_spell(player, card, event, 8, NULL, NULL, 1, &this_test);
}

/* Hornswoggle	|2|U
 * Instant
 * Counter target creature spell. You create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Induced Amnesia	|2|U
 * Enchantment
 * When ~ enters the battlefield, target player exiles all cards from their hand face down, then draws that many cards.
 * When ~ is put into a graveyard from the battlefield, return the exiled cards to their owner's hand. */

/* ManalinkEh 2022: 0x21905d0; original cards/rivals_of_ixalan.c:480. */
int card_kitesail_corsair(int player, int card, event_t event)
{
  /* Kitesail Corsair	|1|U
   * Creature - Human Pirate 2/1
   * ~ has flying as long as it's attacking. */

  card_instance_t *instance;
  int result;

  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_attacking(player, card);
      if (result != 0)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {
          event_result = event_result | 0x20;
        }
      }
    }
  }
  return 0;
}

/* Kumena's Awakening	|2|U|U
 * Enchantment
 * Ascend
 * At the beginning of your upkeep, each player draws a card. If you have the city's blessing, instead only you draw a card. */

/* Mist-Cloaked Herald	|U
 * Creature - Merfolk Warrior 1/1
 * ~ can't be blocked. */

/* ManalinkEh 2022: 0x2190650; original cards/rivals_of_ixalan.c:502. */
int card_nezahal_primal_tide(int player, int card, event_t event)
{
  /* Nezahal, Primal Tide	|5|U|U
   * Legendary Creature - Elder Dinosaur 7/7
   * ~ can't be countered.
   * You have no maximum hand size.
   * Whenever an opponent casts a noncreature spell, draw a card.
   * Discard three cards: Exile ~. Return it to the battlefield tapped under its owner's control at the beginning of the next end step. */

  card_instance_t *instance;
  card_instance_t *parent_in_play;
  int result;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  cannot_be_countered(player, card, event);
  if (event == EVENT_MAX_HAND_SIZE)
  {
    if ((current_turn == player) && (result = is_humiliated(player, card, AL_STATIC), result == 0))
    {
      event_result = 1000;
    }
    if (trigger_condition != TRIGGER_SPELL_CAST)
    {
      return 0;
    }
  }

  if (trigger_condition == TRIGGER_SPELL_CAST && card == affected_card && player == affected_card_controller)
  {
    default_test_definition(&this_test, 2);
    this_test.type_flag = 1;
    sp_ability_trigger(1 - player, event, 2, TYPE_NONE, &this_test, player, card);
  }
  if (event == EVENT_SPA)
  {
    draw_a_card(player);
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    if (2 < hand_count[player])
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    result = charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0);
    if (result != 0)
    {
      new_multidiscard(player, 3, 0, player);
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    parent_in_play = in_play(instance->parent_controller, instance->parent_card);
    if (parent_in_play != NULL)
    {
      remove_until_eot(instance->parent_controller, instance->parent_card,
                       instance->parent_controller, instance->parent_card);
      return 0;
    }
  }
  return 0;
}

/* Release to the Wind	|2|U
 * Instant
 * Exile target nonland permanent. For as long as that card remains exiled, its owner may cast it without paying its mana cost. */

/* River Darter	|2|U
 * Creature - Merfolk Warrior 2/3
 * ~ can't be blocked by Dinosaurs. */

/* Riverwise Augur	|3|U
 * Creature - Merfolk Wizard 2/2
 * When ~ enters the battlefield, draw three cards, then put two cards from your hand on top of your library in any order. */

/* Sea Legs	|U
 * Enchantment - Aura
 * Flash
 * Enchant creature
 * Enchanted creature gets +0/+2 as long as it's a Pirate. Otherwise, it gets -2/-0. */

/* Seafloor Oracle	|2|U|U
 * Creature - Merfolk Wizard 2/3
 * Whenever a Merfolk you control deals combat damage to a player, draw a card. */

/* Secrets of the Golden City	|1|U|U
 * Sorcery
 * Ascend
 * Draw two cards. If you have the city's blessing, draw three cards instead. */

/* Siren Reaver	|3|U
 * Creature - Siren Pirate 3/2
 * Raid - This spell costs |1 less to cast if you attacked with a creature this turn.
 * Flying */

/* Slippery Scoundrel	|2|U
 * Creature - Human Pirate 2/2
 * Ascend
 * As long as you have the city's blessing, ~ has hexproof and can't be blocked. */

/* Soul of the Rapids	|3|U|U
 * Creature - Elemental 3/2
 * Flying
 * Hexproof */

/* Spire Winder	|3|U
 * Creature - Snake 2/3
 * Flying
 * Ascend
 * ~ gets +1/+1 as long as you have the city's blessing. */

/* Sworn Guardian	|1|U
 * Creature - Merfolk Warrior 1/3 */

/* ManalinkEh 2022: 0x21908e0; original cards/rivals_of_ixalan.c:602. */
int card_timestream_navigator(int player, int card, event_t event)
{
  /* Timestream Navigator	|1|U
   * Creature - Human Pirate Wizard 1/1
   * Ascend
   * |2|U|U, |T, Put ~ on the bottom of its owner's library: Take an extra turn after this one. Activate this ability only if you have the city's blessing. */

  ascend(player, card, event);
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE &&
      event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
    return 0;
  if (!has_the_citys_blessing(player))
    return 0;

  if (event == EVENT_ACTIVATE)
  {
    int *library = deck_ptr[player];
    int deck_size = count_deck(player);
    int original_iid = get_original_internal_card_id(player, card);
    library[deck_size] = original_iid;
    obliterate_card(player, card);
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    int parent_card = get_card_instance(player, card)->parent_card;
    int parent_player = get_card_instance(player, card)->parent_controller;
    time_walk_effect(parent_player, parent_card);
  }
  return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XU(2, 2),
                                    0, NULL, NULL);
}

/* ManalinkEh 2022: 0x2190ab0; original cards/rivals_of_ixalan.c:626. */
int card_warkite_marauder(int player, int card, event_t event)
{
  /* Warkite Marauder	|1|U
   * Creature - Human Pirate 2/1
   * Flying
   * Whenever ~ attacks, target creature defending player controls loses all abilities and has base power and toughness 0/1 until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t hc;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - current_turn;
    td.allow_cancel = 0;
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS");
      if (result != 0)
      {
        default_test_definition(&hc, 2);
        hc.power = 0;
        hc.toughness = 1;
        instance = get_card_instance(player, card);
        result = instance->targets[0].card;
        instance = get_card_instance(player, card);
        humiliate_and_set_pt_abilities(player, card, instance->targets[0].player, result, 4, &hc);
      }
    }
  }
  return 0;
}

/* Waterknot	|1|U|U
 * Enchantment - Aura
 * Enchant creature
 * When ~ enters the battlefield, tap enchanted creature.
 * Enchanted creature doesn't untap during its controller's untap step. */

/* Arterial Flow	|1|B|B
 * Sorcery
 * Each opponent discards two cards. If you control a Vampire, each opponent loses 2 life and you gain 2 life. */

/* Canal Monitor	|4|B
 * Creature - Lizard 5/3 */

/* ManalinkEh 2022: 0x2190bd0; original cards/rivals_of_ixalan.c:666. */
int card_champion_of_dusk(int player, int card, event_t event)
{
  /* Champion of Dusk	|3|B|B
   * Creature - Vampire Knight 4/4
   * When ~ enters the battlefield, you draw X cards and you lose X life, where X is the number of Vampires you control. */

  int amount;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  amount = count_subtype(player, -1, 0xcd);
  lose_life(player, amount);
  draw_cards(player, amount);
  return 0;
}

/* Dark Inquiry	|2|B
 * Sorcery
 * Target opponent reveals their hand. You choose a nonland card from it. That player discards that card. */

/* Dead Man's Chest	|1|B
 * Enchantment - Aura
 * Enchant creature an opponent controls
 * When enchanted creature dies, exile cards equal to its power from the top of its owner's library. You may cast nonland cards from among them for as long as they remain exiled, and you may spend mana as though it were mana of any type to cast those spells. */

/* Dinosaur Hunter	|1|B
 * Creature - Human Pirate 2/2
 * Whenever ~ deals damage to a Dinosaur, destroy that creature. */

/* Dire Fleet Poisoner	|1|B
 * Creature - Human Pirate 2/2
 * Flash
 * Deathtouch
 * When ~ enters the battlefield, target attacking Pirate you control gets +1/+1 and gains deathtouch until end of turn. */

/* Dusk Charger	|3|B
 * Creature - Horse 3/3
 * Ascend
 * ~ gets +2/+2 as long as you have the city's blessing. */

/* Dusk Legion Zealot	|1|B
 * Creature - Vampire Soldier 1/1
 * When ~ enters the battlefield, you draw a card and you lose 1 life. */

/* Fathom Fleet Boarder	|2|B
 * Creature - Orc Pirate 3/3
 * When ~ enters the battlefield, you lose 2 life unless you control another Pirate. */

/* ManalinkEh 2022: 0x2190c50; original cards/rivals_of_ixalan.c:713. */
int card_forerunner_of_the_coalition(int player, int card, event_t event)
{
  /* Forerunner of the Coalition	|2|B
   * Creature - Human Pirate 2/2
   * When ~ enters the battlefield, you may search your library for a Pirate card, reveal it, then shuffle your library and put that card on top of it.
   * Whenever another Pirate enters the battlefield under your control, each opponent loses 1 life. */

  int result;
  test_definition_t test;
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 0x47);
    test.subtype = 0x95;
    test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    if ((trigger_cause_controller != player) || (trigger_cause != card))
    {
      lose_life(1 - player, 1);
      return 0;
    }
    new_default_test_definition(&test, 0xff, "Select a Pirate card.");
    test.subtype = 0x95;
    new_global_tutor(player, player, 1, 2, 0, 1, &test);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2190db0; original cards/rivals_of_ixalan.c:742. */
int card_golden_demise(int player, int card, event_t event)
{
  /* Golden Demise	|1|B|B
   * Sorcery
   * Ascend
   * All creatures get -2/-2 until end of turn. If you have the city's blessing, instead only creatures your opponents control get -2/-2 until end of turn. */

  int result;
  int t_player;
  test_definition_t test;

  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {

    ascend(player, card, EVENT_RESOLVE_SPELL);
    default_test_definition(&test, 2);
    result = has_the_citys_blessing(player);
    t_player = 2;
    if (result != 0)
    {
      t_player = 1 - player;
    }
    pump_creatures_until_eot(player, card, t_player, 0, -2, -2, 0, 0, NULL);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* Grasping Scoundrel	|B
 * Creature - Human Pirate 1/1
 * ~ gets +1/+0 as long as it's attacking. */

/* Gruesome Fate	|2|B
 * Sorcery
 * Each opponent loses 1 life for each creature you control. */

/* Impale	|2|B|B
 * Sorcery
 * Destroy target creature. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2190ed0; original cards/rivals_of_ixalan.c:774. */
int card_masterminds_acquisition(int player, int card, event_t event)
{
  /* Mastermind's Acquisition	|2|B|B
   * Sorcery
   * Choose one -
   * * Search your library for a card, put it into your hand, then shuffle your library.
   * * Choose a card you own from outside the game and put it into your hand. */

  enum
  {
    CHOICE_TUTOR = 1,
    CHOICE_WISH = 2
  };

  card_instance_t *instance;
  int result;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    get_card_instance(player, card);
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
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
    instance = get_card_instance(player, card);
    if (card != affected_card)
    {
      return 0;
    }
    if (player != affected_card_controller)
    {
      return 0;
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    instance = get_card_instance(player, card);
  }
  else
  {
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_CAST_SPELL || event == EVENT_CHANGE_TARGET)
  {
    instance->number_of_targets = '\0';
    result = check_special_flags2(player, card, -0x80000000);
    if (result != 0)
    {
      return 0;
    }

    result = dialog_impl(player, card, event, DLG_RANDOM, "Tutor", 1, 5, "Wish", 1, 5, 0, 0, 0, 0);
    if (result == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
  }

  if (event == EVENT_RESOLVE_SPELL)
  {

    if (instance->info_slot == CHOICE_TUTOR)
    {
      new_default_test_definition(&test, 0xff, "SELECT_CARD");
      new_global_tutor(player, player, 1, 1, 0, 1, &test);
    }
    else if (instance->info_slot == CHOICE_WISH)
    {
      default_test_definition(&test, 0xff);
      generic_wish_effect(player, card, &test, 0x312a);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return 0;
}
#endif

/* Mausoleum Harpy	|4|B
 * Creature - Harpy 3/3
 * Flying
 * Ascend
 * Whenever another creature you control dies, if you have the city's blessing, put a +1/+1 counter on ~. */

/* ManalinkEh 2022: 0x2191130; original cards/rivals_of_ixalan.c:836. */
int card_moment_of_craving(int player, int card, event_t event)
{
  /* Moment of Craving	|1|B
   * Instant
   * Target creature gets -2/-2 until end of turn. You gain 2 life. */

  int result;
  if (event == EVENT_RESOLVE_SPELL)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    if (valid_target(&td))
      gain_life(player, 2);
  }
  result = vanilla_instant_pump(player, card, event, 2, 1 - player, -2, -2, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x218ee10; original cards/rivals_of_ixalan.c:850. */
static int pitiless_plunderer_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  card_instance_t *dying;
  int pending_or_result;
  token_generation_t token;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (pending_or_result =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           pending_or_result != 0x4756))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (pending_or_result = effect_follows_control_of_attachment(player, card, 0x4700),
         pending_or_result != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (pending_or_result = effect_follows_control_of_attachment(player, card, event),
         pending_or_result != 0))
    {
      return 0;
    }
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {

      instance = get_card_instance(player, card);
      dying = in_play(affected_card_controller, affected_card);
      if ((dying != NULL) &&
          (pending_or_result = is_this_dying(affected_card_controller, affected_card),
           pending_or_result != 0))
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
          return 0;
        }
        count_for_gfp_ability(player, card, 0x77, player, 2, NULL);
      }
    }
  }
  pending_or_result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_or_result != 0)
  {
    if (check_status(player, card, 0x10000) == 0)
    {

      default_token_definition(player, card, CARD_ID_TREASURE, &token);
      token.qty = (unsigned char)pending_or_result;
      generate_token(&token);
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) &&
      (pending_or_result = check_status(player, card, 0x10000), pending_or_result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* Oathsworn Vampire	|1|B
 * Creature - Vampire Knight 2/2
 * ~ enters the battlefield tapped.
 * You may cast ~ from your graveyard if you gained life this turn. */

/* ManalinkEh 2022: 0x21911b0; original cards/rivals_of_ixalan.c:902. */
int card_pitiless_plunderer(int player, int card, event_t event)
{
  /* Pitiless Plunderer	|3|B
   * Creature - Human Pirate 1/4
   * Whenever another creature you control dies, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

  int c;
  int created_or_pending;
  card_instance_t *instance;

  bool legacy_found = false;
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          instance = in_play(player, c);

          if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
               (instance = get_card_instance(player, c),
                instance->info_slot_fn == pitiless_plunderer_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            legacy_found = true;
            break;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }

      if (!legacy_found)
      {
        created_or_pending =
            create_targetted_legacy_effect(player, card, pitiless_plunderer_effect, player, card);
        instance = get_card_instance(player, created_or_pending);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, created_or_pending, 0x10000);
      }
    }
  }
  else if ((event == EVENT_GRAVEYARD_FROM_PLAY) &&
           (instance = in_play(player, card), instance != NULL))
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 2);
    this_test.not_me = 1;
    count_for_gfp_ability(player, card, 0x77, player, 0, &this_test);
  }
  created_or_pending = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (created_or_pending != 0)
  {
    token_generation_t token;

    default_token_definition(player, card, CARD_ID_TREASURE, &token);
    token.qty = (char)created_or_pending;
    generate_token(&token);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x21913b0; original cards/rivals_of_ixalan.c:951. */
int card_ravenous_chupacabra(int player, int card, event_t event)
{
  /* Ravenous Chupacabra	|2|B|B
   * Creature - Beast Horror 2/2
   * When ~ enters the battlefield, destroy target creature an opponent controls. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  td.allow_cancel = 0;
  result = can_target(&td);
  if (result != 0)
  {
    result = pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS");
    if (result != 0)
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* Reaver Ambush	|2|B
 * Instant
 * Exile target creature with power 3 or less. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2191480; original cards/rivals_of_ixalan.c:979. */
int card_sadistic_skymarcher(int player, int card, event_t event)
{
  /* Sadistic Skymarcher	|2|B
   * Creature - Vampire Soldier 2/2
   * As an additional cost to cast this spell, reveal a Vampire card from your hand or pay |1.
   * Flying, lifelink */

  int result;

  lifelink(player, card, event);
  result = buddy_creature(player, card, event, SUBTYPE_VAMPIRE, 1);
  return result;
}
#endif

/* ManalinkEh 2022: 0x21914d0; original cards/rivals_of_ixalan.c:989. */
int card_tetzimoc_primal_death(int player, int card, event_t event)
{
  /* Tetzimoc, Primal Death	|4|B|B
   * Legendary Creature - Elder Dinosaur 6/6
   * Deathtouch
   * |B, Reveal ~ from your hand: Put a prey counter on target creature. Activate this ability only during your turn.
   * When ~ enters the battlefield, destroy each creature your opponents control with a prey counter on it. */

  card_instance_t *instance;
  int opponent_or_result;
  int creature_or_counters;
  int card_00;
  target_definition_t td;

  check_legend_rule(player, card, event);
  deathtouch(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    opponent_or_result = 1 - player;
    if (0 < active_cards_count[opponent_or_result])
    {
      card_00 = 0;
      do
      {
        instance = in_play(opponent_or_result, card_00);
        if (((instance != NULL) && (creature_or_counters = is_what(opponent_or_result, card_00, 2),
                                    creature_or_counters != 0)) &&
            (creature_or_counters = count_counters(opponent_or_result, card_00, COUNTER_PREY),
             0 < creature_or_counters))
        {
          kill_card(opponent_or_result, card_00, KILL_DESTROY);
        }
        card_00 = card_00 + 1;
      } while (card_00 < active_cards_count[opponent_or_result]);
    }
  }
  else if ((uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND) < 3)
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 1 - player;
    if (event == EVENT_CAN_ACTIVATE_FROM_HAND)
    {
      if (can_target(&td) == 0)
      {
        return 0;
      }
      if (current_turn != player)
      {
        return 0;
      }
      opponent_or_result = has_mana(player, 1, 1);
    }
    else
    {
      if (event != EVENT_ACTIVATE_FROM_HAND)
      {
        if (event != EVENT_RESOLVE_ACTIVATION_FROM_HAND)
        {
          return 0;
        }
        if (spell_fizzled == 1)
        {
          return 0;
        }
        add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                     COUNTER_PREY, 1);
        return 0;
      }
      if (charge_mana_for_activated_ability(player, card, 0, 1, 0, 0, 0, 0) == 0)
      {
        return 0;
      }
      opponent_or_result = pick_target(&td, "TARGET_CREATURE");
    }
    if (opponent_or_result != 0)
    {
      return 1;
    }
  }
  return 0;
}

/* Tomb Robber	|2|B
 * Creature - Human Pirate 1/1
 * Menace
 * |1, Discard a card: ~ explores. */

/* ManalinkEh 2022: 0x2191710; original cards/rivals_of_ixalan.c:1040. */
int card_twilight_prophet(int player, int card, event_t event)
{
  /* Twilight Prophet	|2|B|B
   * Creature - Vampire Cleric 2/4
   * Flying
   * Ascend
   * At the beginning of your upkeep, if you have the city's blessing, reveal the top card of your library and put it into your hand. Each opponent loses X life and you gain X life, where X is that card's converted mana cost. */

  int result;
  int amount;

  ascend(player, card, event);
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    result = has_the_citys_blessing(player);
    if ((result != 0) && (result = *deck_ptr[player], result != -1))
    {

      remove_card_from_deck(player, 0);
      reveal_card_iid(player, card, result);
      add_card_to_hand(player, result);
      amount = get_cmc_by_internal_id(result);
      lose_life(1 - player, amount);
      result = get_cmc_by_internal_id(result);
      gain_life(player, result);
    }
  }
  return 0;
}

/* Vampire Revenant	|3|B
 * Creature - Vampire Spirit 3/1
 * Flying */

/* ManalinkEh 2022: 0x21917e0; original cards/rivals_of_ixalan.c:1068. */
int card_vonas_hunger(int player, int card, event_t event)
{
  /* Vona's Hunger	|2|B
   * Instant
   * Ascend
   * Each opponent sacrifices a creature. If you have the city's blessing, instead each opponent sacrifices half the creatures they control, rounded up. */

  unsigned int sacrifice_count;
  test_definition_t test;

  ascend(player, card, event);
  if (event != EVENT_RESOLVE_SPELL)
    return basic_spell(player, card, event);
  if (has_the_citys_blessing(player) || count_permanents_by_type(player, TYPE_PERMANENT) > 9)
  {
    sacrifice_count = count_subtype(1 - player, 2, -1);
    sacrifice_count = round_up_value(sacrifice_count);
    new_default_test_definition(&test, 2, "SACRIFICE_CREATURE");
    test.qty = (unsigned short)sacrifice_count;
  }
  else
    new_default_test_definition(&test, 2, "SACRIFICE_CREATURE");
  new_sacrifice(player, card, 1 - player, SAC_CAUSED | SAC_NO_CANCEL, &test);
  kill_card(player, card, KILL_DESTROY);
  return basic_spell(player, card, event);
}

/* Voracious Vampire	|2|B
 * Creature - Vampire Knight 2/2
 * Menace
 * When ~ enters the battlefield, target Vampire you control gets +1/+1 and gains menace until end of turn. */

/* ManalinkEh 2022: 0x2191920; original cards/rivals_of_ixalan.c:1103. */
int card_blood_sun(int player, int card, event_t event)
{
  /* Blood Sun	|2|R
   * Enchantment
   * When ~ enters the battlefield, draw a card.
   * All lands lose all abilities except mana abilities. */

  unsigned int saved_abilities;
  card_instance_t *instance;
  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
    draw_a_card(player);
  instance = in_play(player, card);
  if (!instance && event == EVENT_ETB)
    return global_enchantment(player, card, event);
  if (instance && !is_humiliated(player, card, AL_STATIC))
  {
    default_test_definition(&test, 1);
    remove_abilities_by_test(2, event, &test, AL_ALL_NM);
  }
  if (event == EVENT_ABILITIES)
  {

    instance = in_play(player, card);
    if ((((instance == NULL) ||
          (instance = in_play(affected_card_controller, affected_card), instance == NULL)) ||
         (is_humiliated(player, card, AL_STATIC) != 0)) ||
        (is_what(affected_card_controller, affected_card, 1) == 0))
    {
      return 0;
    }
    event_result = event_result & 0x9f080000U | 0x1f000000;
    instance = get_card_instance(affected_card_controller, affected_card);
    saved_abilities = instance->targets[0x11].card;
    instance->state = instance->state & 0xffffdfff;
    instance->targets[0x11].card = saved_abilities | 0x6e;
    instance->ext->sp_keywords = instance->ext->sp_keywords & 0x993c00c0;
  }
  return global_enchantment(player, card, event);
}

/* Bombard	|2|R
 * Instant
 * ~ deals 4 damage to target creature. */

/* ManalinkEh 2022: 0x2191ae0; original cards/rivals_of_ixalan.c:1157. */
int card_brasss_bounty(int player, int card, event_t event)
{
  /* Brass's Bounty	|6|R
   * Sorcery
   * For each land you control, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

  int result;
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
      result = count_permanents_by_type(player, TYPE_LAND);
      default_token_definition(player, card, CARD_ID_TREASURE, &token);
      token.qty = (unsigned char)result;
      generate_token(&token);
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  result = basic_spell(player, card, event);
  return result;
}

/* Brazen Freebooter	|3|R
 * Creature - Human Pirate 3/3
 * When ~ enters the battlefield, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Buccaneer's Bravado	|1|R
 * Instant
 * Choose one -
 * * Target creature gets +1/+1 and gains first strike until end of turn.
 * * Target Pirate gets +1/+1 and gains double strike until end of turn. */

/* Charging Tuskodon	|3|R|R
 * Creature - Dinosaur 4/4
 * Trample
 * If ~ would deal combat damage to a player, it deals double that damage to that player instead. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2191bb0; original cards/rivals_of_ixalan.c:1191. */
int card_daring_buccaneer(int player, int card, event_t event)
{
  /* Daring Buccaneer	|R
   * Creature - Human Pirate 2/2
   * As an additional cost to cast this spell, reveal a Pirate card from your hand or pay |2. */

  int result;

  result = buddy_creature(player, card, event, SUBTYPE_PIRATE, 2);
  return result;
}
#endif

/* ManalinkEh 2022: 0x2191bf0; original cards/rivals_of_ixalan.c:1199. */
int card_dire_fleet_daredevil(int player, int card, event_t event)
{
  /* Dire Fleet Daredevil	|1|R
   * Creature - Human Pirate 2/1
   * First strike
   * When ~ enters the battlefield, exile target instant or sorcery card from an opponent's graveyard. You may cast that card this turn, and you may spend mana as though it were mana of any type to cast that spell. If that card would be put into a graveyard this turn, exile it instead. */

  enum
  {
    MPCFE_EXILE_ON_RESOLUTION = 128
  };

  unsigned short exiled_csvid;
  int result;
  int opponent;
  test_definition_t this_test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    opponent = 1 - player;
    new_default_test_definition(&this_test, 0x38, "Select an instant or sorcery card to exile.");
    this_test.type_flag = 2;
    result = new_special_count_grave(opponent, &this_test);
    if (0 < result)
    {
      result = graveyard_has_shroud(opponent);
      if (result == 0)
      {

        result = new_select_a_card(player, opponent, 2, 1, 2, -1, &this_test);
        if (-1 < result)
        {

          exiled_csvid = cards_data[graveyard_ptr_mutable[opponent][result]].id;
          rfg_card_from_grave(opponent, result);
          create_may_play_card_from_exile_effect(
              player, card, opponent, (unsigned int)exiled_csvid,
              MPCFE_EXILE_ON_RESOLUTION | MPCFE_UNTIL_EOT | MPCFE_FOR_CMC);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2191d00; original cards/rivals_of_ixalan.c:1226. */
int card_etali_primal_storm(int player, int card, event_t event)
{
  /* Etali, Primal Storm	|4|R|R
   * Legendary Creature - Elder Dinosaur 6/6
   * Whenever ~ attacks, exile the top card of each player's library, then you may cast any number of nonland cards exiled this way without paying their mana costs. */

  unsigned short exiled_csvid;
  int iid_or_next_index;
  int player_or_selected;
  int i;
  int exiled[2];
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {

    exiled[0] = -1;
    exiled[1] = -1;
    player_or_selected = 0;
    i = 0;
    do
    {

      iid_or_next_index = *deck_ptr[player_or_selected];
      if (iid_or_next_index != -1)
      {
        rfg_top_card_of_deck(player_or_selected);
      }
      if (((cards_data[iid_or_next_index].type & 0xfe) != 0) &&
          (can_legally_play_iid(player, iid_or_next_index) != 0))
      {
        exiled[player_or_selected] = iid_or_next_index;
        i = i + 1;
      }
      player_or_selected = player_or_selected + 1;
    } while (player_or_selected != 2);
    if (0 < i)
    {
      do
      {
        new_default_test_definition(&this_test, 0xff, "Select a card to play for free");
        player_or_selected = select_card_from_zone(player, player, exiled, i, 0, 1, -1, &this_test);
        if (player_or_selected == -1)
        {
          return 0;
        }

        exiled_csvid = cards_data[exiled[player_or_selected]].id;
        if (player_or_selected < i)
        {
          iid_or_next_index = player_or_selected + 1;
          exiled[player_or_selected] = exiled[iid_or_next_index];
          if (iid_or_next_index < i)
          {
            exiled[iid_or_next_index] = *(int *)(this_test.message + player_or_selected * 4);
          }
        }
        i = i + -1;
        play_card_in_exile_for_free(player, player_or_selected, (unsigned int)exiled_csvid);
      } while (i != 0);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2191ec0; original cards/rivals_of_ixalan.c:1271. */
int card_fanatical_firebrand(int player, int card, event_t event)
{
  /* Fanatical Firebrand	|R
   * Creature - Goblin Pirate 1/1
   * Haste
   * |T, Sacrifice ~: It deals 1 damage to any target. */

  int result;
  target_definition_t td;
  haste(player, card, event);
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    result = validate_target(player, card, &td, 0);
    if (result != 0)
    {
      damage_target0(player, card, 1);
    }
  }
  result =
      generic_activated_ability(player, card, event, 0xd, 0, 0, 0, 0, 0, 0, 0, &td, "TARGET_ANY");
  return result;
}

/* ManalinkEh 2022: 0x2191fc0; original cards/rivals_of_ixalan.c:1289. */
int card_forerunner_of_the_empire(int player, int card, event_t event)
{
  /* Forerunner of the Empire	|3|R
   * Creature - Human Soldier 1/3
   * When ~ enters the battlefield, you may search your library for a Dinosaur card, reveal it, then shuffle your library and put that card on top of it.
   * Whenever a Dinosaur enters the battlefield under your control, you may have ~ deal 1 damage to each creature. */

  int result;
  test_definition_t test;
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 0x47);
    test.subtype = 0xf0;
    test.not_me = 1;
    result = duh_mode(player);
    etb_ability_global(player, event, (result != 0) + 1, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    if ((trigger_cause_controller != player) || (trigger_cause != card))
    {
      default_test_definition(&test, 2);
      new_damage_all(player, card, 2, 1, 1, &test);
      return 0;
    }
    new_default_test_definition(&test, 0xff, "Select a Dinosaur card.");
    test.subtype = 0xf0;
    new_global_tutor(player, player, 1, 2, 0, 1, &test);
  }
  return 0;
}

/* Form of the Dinosaur	|4|R|R
 * Enchantment
 * When ~ enters the battlefield, your life total becomes 15.
 * At the beginning of your upkeep, ~ deals 15 damage to target creature an opponent controls and that creature deals damage equal to its power to you. */

/* Frilled Deathspitter	|2|R
 * Creature - Dinosaur 3/2
 * Enrage - Whenever ~ is dealt damage, it deals 2 damage to target opponent or planeswalker. */

/* Goblin Trailblazer	|1|R
 * Creature - Goblin Pirate 2/1
 * Menace */

/* Mutiny	|R
 * Sorcery
 * Target creature an opponent controls deals damage equal to its power to another target creature that player controls. */

/* Needletooth Raptor	|3|R
 * Creature - Dinosaur 2/2
 * Enrage - Whenever ~ is dealt damage, it deals 5 damage to target creature an opponent controls. */

/* Orazca Raptor	|2|R|R
 * Creature - Dinosaur 3/4 */

/* Pirate's Pillage	|3|R
 * Sorcery
 * As an additional cost to cast this spell, discard a card.
 * Draw two cards and create two colorless Treasure artifact tokens with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Reckless Rage	|R
 * Instant
 * ~ deals 4 damage to target creature you don't control and 2 damage to target creature you control. */

/* ManalinkEh 2022: 0x2192160; original cards/rivals_of_ixalan.c:1355. */
int card_rekindling_phoenix(int player, int card, event_t event)
{
  /* Rekindling Phoenix	|2|R|R
   * Creature - Phoenix 4/3
   * Flying
   * When ~ dies, create a 0/1 |Sred Elemental creature token with "At the beginning of your upkeep, sacrifice this creature and return target card named ~ from your graveyard to the battlefield. It gains haste until end of turn." */

  int result;
  token_generation_t token;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
  token.pow = 0;
  token.tou = 1;
  token.special_infos = 0x44;
  generate_token(&token);
  return 0;
}

/* See Red	|1|R
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+1 and has first strike.
 * At the beginning of your end step, if you didn't attack with a creature this turn, sacrifice ~. */

/* Shake the Foundations	|2|R
 * Instant
 * ~ deals 1 damage to each creature without flying.
 * Draw a card. */

/* ManalinkEh 2022: 0x21921e0; original cards/rivals_of_ixalan.c:1389. */
int card_silverclad_ferocidons(int player, int card, event_t event)
{
  /* Silverclad Ferocidons	|5|R|R
   * Creature - Dinosaur 8/5
   * Enrage - Whenever ~ is dealt damage, each opponent sacrifices a permanent. */

  int result;
  test_definition_t test;

  result = is_humiliated(player, card, AL_TRIG);
  if (result == 0)
  {
    result = damage_dealt_to_me_arbitrary(player, card, event, DDBM_0, player, card);
    if (result != 0)
    {
      new_default_test_definition(&test, 0x47, "Select a permanent to sacrifice");
      new_sacrifice(player, card, 1 - player, SAC_CAUSED | SAC_NO_CANCEL, &test);
      return 0;
    }
  }
  return 0;
}

/* Stampeding Horncrest	|4|R
 * Creature - Dinosaur 4/4
 * ~ has haste as long as you control another Dinosaur. */

/* Storm Fleet Swashbuckler	|1|R
 * Creature - Human Pirate 2/2
 * Ascend
 * ~ has double strike as long as you have the city's blessing. */

/* Sun-Collared Raptor	|1|R
 * Creature - Dinosaur 1/2
 * Trample
 * |2|R: ~ gets +3/+0 until end of turn. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x218f5b0; original cards/rivals_of_ixalan.c:1427. */
static int remove_eot_unless_cb(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->damage_target_player < 0)
    return 0;

  instance = get_card_instance(player, card);
  if (trigger_condition != TRIGGER_EOT || card != affected_card ||
      player != affected_card_controller || player != reason_for_trigger_controller)
    return 0;

  int turns_left = instance->targets[1].player;
  if (turns_left > 0)
    instance->targets[1].player = turns_left - 1;
  else if (event == EVENT_TRIGGER)
    event_result |= RESOLVE_TRIGGER_MANDATORY;
  else if (event == EVENT_RESOLVE_TRIGGER)
  {
    int attached_player = instance->damage_target_player;
    int attached_card = instance->damage_target_card;
    if (!check_state(attached_player, attached_card, STATE_OUBLIETTED) &&
        !has_the_citys_blessing(player))
    {
      kill_card(attached_player, attached_card, KILL_EXILE);
      return 0;
    }
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}
#endif

/* Swaggering Corsair	|2|R
 * Creature - Human Pirate 2/2
 * Raid - ~ enters the battlefield with a +1/+1 counter on it if you attacked with a creature this turn. */

/* Tilonalli's Crown	|1|R
 * Enchantment - Aura
 * Enchant creature
 * When ~ enters the battlefield, it deals 1 damage to enchanted creature.
 * Enchanted creature gets +3/+0 and has trample. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x218eda0; original cards/rivals_of_ixalan.c:1456. */
static void remove_token_at_end_of_next_turn_unless_cb(token_generation_t *token, int beast,
                                                       int number)
{
  int card;
  card_instance_t *instance;

  card = create_targetted_legacy_effect(token->s_player, token->s_card, remove_eot_unless_cb,
                                        token->t_player, beast);
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
/* ManalinkEh 2022: 0x2192290; original cards/rivals_of_ixalan.c:1464. */
int card_tilonallis_summoner(int player, int card, event_t event)
{
  /* Tilonalli's Summoner	|1|R
   * Creature - Human Shaman 1/1
   * Ascend
   * Whenever ~ attacks, you may pay |X|R. If you do, create X 1/1 |Sred Elemental creature tokens that are tapped and attacking. At the beginning of the next end step, exile those tokens unless you have the city's blessing. */

  xtrigger_t trigger;
  declare_attackers_trigger_t mode;
  card_instance_t *instance;
  int result;
  token_generation_t token;

  ascend(player, card, event);
  store_attackers(player, card, event, 0, player, card, NULL);
  trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    trigger = xtrigger_impl_value_dont_use_directly;
  }
  if (((event == EVENT_DECLARE_ATTACKERS) || (trigger == XTRIGGER_ATTACKING)) &&
      (result = has_mana(player, 4, 1), result != 0))
  {

    if ((player != 0) || (mode = DAT_RESOLVE_TRIGGER_OPTIONAL, ai_is_speculating == 1))
    {
      result = has_mana_multi(player, 1, 0, 0, 0, 1, 0);
      mode = (unsigned int)(result != 0) * 2;
    }
    dat_ability(player, card, event, mode, 1);
  }
  if (event == EVENT_DAT_ABILITY)
  {

    instance = get_card_instance(player, card);
    instance->number_of_targets = '\0';
    result = charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, -1, 0,
                                               0, 0, 1, 0);
    if (result != 0)
    {
      default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
      token.tou = 1;
      token.pow = 1;
      token.qty = (unsigned char)x_value;
      token.action = TOKEN_ACTION_ATTACKING;
      token.special_code_on_generation = remove_token_at_end_of_next_turn_unless_cb;
      generate_token(&token);
    }
  }
  return 0;
}
#endif

/* Cacophodon	|3|G
 * Creature - Dinosaur 2/5
 * Enrage - Whenever ~ is dealt damage, untap target permanent. */

/* Cherished Hatchling	|1|G
 * Creature - Dinosaur 2/1
 * When ~ dies, you may cast Dinosaur spells this turn as though they had flash, and whenever you cast a Dinosaur spell this turn, it gains "When this creature enters the battlefield, you may have it fight another target creature." */

/* Crested Herdcaller	|3|G|G
 * Creature - Dinosaur 3/3
 * Trample
 * When ~ enters the battlefield, create a 3/3 |Sgreen Dinosaur creature token with trample. */

/* Deeproot Elite	|1|G
 * Creature - Merfolk Warrior 1/1
 * Whenever another Merfolk enters the battlefield under your control, put a +1/+1 counter on target Merfolk you control. */

/* Enter the Unknown	|G
 * Sorcery
 * Target creature you control explores.
 * You may play an additional land this turn. */

/* ManalinkEh 2022: 0x2192460; original cards/rivals_of_ixalan.c:1534. */
int card_forerunner_of_the_heralds(int player, int card, event_t event)
{
  /* Forerunner of the Heralds	|3|G
   * Creature - Merfolk Scout 3/2
   * When ~ enters the battlefield, you may search your library for a Merfolk card, reveal it, then shuffle your library and put that card on top of it.
   * Whenever another Merfolk enters the battlefield under your control, put a +1/+1 counter on ~. */

  int result;
  test_definition_t test;

  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 0x47);
    test.subtype = 0x74;
    test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    if ((trigger_cause_controller != player) || (trigger_cause != card))
    {
      add_counters(player, card, player, card, COUNTER_P1_P1, 1);
      return 0;
    }
    new_default_test_definition(&test, 0xff, "Select a Merfolk card.");
    test.subtype = 0x74;
    new_global_tutor(player, player, 1, 2, 0, 1, &test);
  }
  return 0;
}

/* ManalinkEh 2022: 0x21925d0; original cards/rivals_of_ixalan.c:1566. */
int card_ghalta_primal_hunger(int player, int card, event_t event)
{
  /* Ghalta, Primal Hunger	|10|G|G
   * Legendary Creature - Elder Dinosaur 12/12
   * ~ costs |X less to cast, where X is the total power of creatures you control.
   * Trample */

  int result;
  check_legend_rule(player, card, event);
  if (event != EVENT_MODIFY_COST)
  {
    return 0;
  }
  result = get_power_sum(player);
  COST_COLORLESS = COST_COLORLESS - (char)result;
  return 0;
}

/* Giltgrove Stalker	|1|G
 * Creature - Merfolk Warrior 2/1
 * ~ can't be blocked by creatures with power 2 or less. */

/* Hardy Veteran	|1|G
 * Creature - Human Warrior 2/2
 * As long as it's your turn, ~ gets +0/+2. */

/* Jade Bearer	|G
 * Creature - Merfolk Shaman 1/1
 * When ~ enters the battlefield, put a +1/+1 counter on another target Merfolk you control. */

/* Jadecraft Artisan	|3|G
 * Creature - Merfolk Shaman 3/3
 * When ~ enters the battlefield, target creature gets +2/+2 until end of turn. */

/* ManalinkEh 2022: 0x2192630; original cards/rivals_of_ixalan.c:1598. */
int card_jadelight_ranger(int player, int card, event_t event)
{
  /* Jadelight Ranger	|1|G|G
   * Creature - Merfolk Scout 2/1
   * When ~ enters the battlefield, it explores, then it explores again. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  explore(player, card, EVENT_ETB);
  explore(player, card, EVENT_ETB);
  return 0;
}

/* Jungleborn Pioneer	|2|G
 * Creature - Merfolk Scout 2/2
 * When ~ enters the battlefield, create a 1/1 |Sblue Merfolk creature token with hexproof. */

/* Knight of the Stampede	|3|G
 * Creature - Human Knight 2/4
 * Dinosaur spells you cast cost |2 less to cast. */

/* Orazca Frillback	|2|G
 * Creature - Dinosaur 4/2 */

/* Overgrown Armasaur	|3|G|G
 * Creature - Dinosaur 4/4
 * Enrage - Whenever ~ is dealt damage, create a 1/1 |Sgreen Saproling creature token. */

/* ManalinkEh 2022: 0x21926b0; original cards/rivals_of_ixalan.c:1627. */
int card_path_of_discovery(int player, int card, event_t event)
{
  /* Path of Discovery	|3|G
   * Enchantment
   * Whenever a creature enters the battlefield under your control, it explores. */

  int result;

  etb_ability_global(player, event, 2, TYPE_CREATURE, NULL, player, card);
  if (event == EVENT_ETB)
  {
    explore(trigger_cause_controller, trigger_cause, EVENT_ETB);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2192730; original cards/rivals_of_ixalan.c:1642. */
int card_polyraptor(int player, int card, event_t event)
{
  /* Polyraptor	|6|G|G
   * Creature - Dinosaur 5/5
   * Enrage - Whenever ~ is dealt damage, create a token that's a copy of ~. */

  int result;

  result = is_humiliated(player, card, AL_TRIG);
  if (result == 0)
  {
    result = damage_dealt_to_me_arbitrary(player, card, event, DDBM_0, player, card);
    if (result != 0)
    {
      copy_token(player, card, player, card);
      return 0;
    }
  }
  return 0;
}

/* Strength of the Pack	|4|G|G
 * Sorcery
 * Put two +1/+1 counters on each creature you control. */

/* Swift Warden	|1|G|G
 * Creature - Merfolk Warrior 3/3
 * Flash
 * When ~ enters the battlefield, target Merfolk you control gains hexproof until end of turn. */

/* ManalinkEh 2022: 0x21927b0; original cards/rivals_of_ixalan.c:1662. */
int card_tendershoot_dryad(int player, int card, event_t event)
{
  /* Tendershoot Dryad	|4|G
   * Creature - Dryad 2/2
   * Ascend
   * At the beginning of each upkeep, create a 1/1 |Sgreen Saproling creature token.
   * Saprolings you control get +2/+2 as long as you have the city's blessing. */

  int result;

  ascend(player, card, event);
  upkeep_trigger_ability(player, card, event, 2);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    if ((uint32_t)(event - EVENT_POWER) < 2)
    {
      result = has_the_citys_blessing(player);
      if (result != 0)
      {
        boost_creature_type(player, card, event, SUBTYPE_SAPROLING, 2, 2, 0, BCT_CONTROLLER_ONLY);
      }
    }
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_SAPROLING);
  return 0;
}

/* ManalinkEh 2022: 0x2192870; original cards/rivals_of_ixalan.c:1681. */
int card_thrashing_brontodon(int player, int card, event_t event)
{
  /* Thrashing Brontodon	|1|G|G
   * Creature - Dinosaur 3/4
   * |1, Sacrifice ~: Destroy target artifact or enchantment. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 0x44);
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return (unsigned int)(event == EVENT_CAN_CAST);
    }
    default_target_definition(player, card, &td, 0x44);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = validate_target(player, card, &td, 0);
      if (result != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
      }
    }
  }
  result = generic_activated_ability(player, card, event, 0xc, 1, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_ARTIFACT_ENCHANTMENT");
  return result;
}

/* Thunderherd Migration	|1|G
 * Sorcery
 * As an additional cost to cast this spell, reveal a Dinosaur card from your hand or pay |1.
 * Search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library. */

/* ManalinkEh 2022: 0x21929d0; original cards/rivals_of_ixalan.c:1709. */
int card_wayward_swordtooth(int player, int card, event_t event)
{
  /* Wayward Swordtooth	|2|G
   * Creature - Dinosaur 5/5
   * Ascend
   * You may play an additional land on each of your turns.
   * ~ can't attack or block unless you have the city's blessing. */

  int result;

  check_playable_lands(player);
  ascend(player, card, event);
  if (event == EVENT_ATTACK_LEGALITY)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = has_the_citys_blessing(player);
      if (result == 0)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {
          event_result = 1;
        }
      }
    }
  }
  else if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    result = has_the_citys_blessing(player);
    if (result == 0)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        cannot_block(player, card, EVENT_ABILITIES);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2192ab0; original cards/rivals_of_ixalan.c:1733. */
int card_world_shaper(int player, int card, event_t event)
{
  /* World Shaper	|3|G
   * Creature - Merfolk Shaman 3/3
   * Whenever ~ attacks, you may put the top three cards of your library into your graveyard.
   * When ~ dies, put all land cards from your graveyard onto the battlefield tapped. */

  int result;

  result = duh_mode(player);
  dat_ability(player, card, event, (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    mill(player, 3);
  }
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  reanimate_all(player, card, player, 1, 0, 0, 0, 0, 0, 0, 0, -1, 0, REANIMATE_TAP);
  return 0;
}

/* ManalinkEh 2022: 0x2192bc0; original cards/rivals_of_ixalan.c:1752. */
int card_angrath_the_flame_chained(int player, int card, event_t event)
{
  /* Angrath, the Flame-Chained	|3|B|R
   * Legendary Planeswalker - Angrath (4)
   * +1: Each opponent discards a card and loses 2 life.
   * -3: Gain control of target creature until end of turn. Untap it. It gains haste until end of turn. Sacrifice it at the beginning of the next end step if it has converted mana cost 3 or less.
   * -8: Each opponent loses life equal to the number of cards in their graveyard. */

  enum
  {
    CHOICE_DISCARD = 1,
    CHOICE_STEAL = 2,
    CHOICE_GY = 3
  };

  card_instance_t *instance;
  int result;
  target_definition_t *creature_target;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(&td);
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                           "Opponent discards & loses life", 1, 5, 1, "Steal a creature", result,
                           10, 0xfffffffd, "Opponent loses life from graveyard cards", 1, 0x14,
                           0xfffffff8, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_STEAL)
      {
        new_pick_target(&td, "TARGET_CREATURE", 0, 1);
      }
      return planeswalker(player, card, event, 4);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 4);
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
  }
  creature_target = &td;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    result = instance->info_slot;
    if (result == CHOICE_DISCARD)
    {
      discard(1 - player, 0, player);
      lose_life(1 - player, 2);
    }
    else if (result == CHOICE_STEAL)
    {
      result = validate_target(player, card, creature_target, 0);
      if (result != 0)
      {
        effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
        result = get_cmc(instance->targets[0].player, instance->targets[0].card);
        if (result < 4)
        {
          haste_and_remove_at_beginning_of_next_end_step(player, card, instance->targets[0].player,
                                                         instance->targets[0].card, 3);
        }
      }
    }
    else if (result == CHOICE_GY)
    {
      result = count_graveyard(1 - player);
      lose_life(1 - player, result);
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x2192ea0; original cards/rivals_of_ixalan.c:1816. */
int card_atzocan_seer(int player, int card, event_t event)
{
  /* Atzocan Seer	|1|G|W
   * Creature - Human Druid 2/3
   * |T: Add one mana of any color.
   * Sacrifice ~: Return target Dinosaur card from your graveyard to your hand. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_DINO = 2
  };

  card_instance_t *instance;
  int result;
  event_t event_00;
  const char *prompt;
  test_definition_t test;

  if ((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE))
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return mana_producing_creature(player, card, event, 0x18, COLOR_GREEN, 1);
    }
    instance = get_card_instance(player, card);
    new_default_test_definition(&test, 2, "Select target Dinosaur creature card.");
    test.subtype = 0xf0;

    if (((event == EVENT_RESOLVE_ACTIVATION) && (instance->info_slot == CHOICE_DINO)) &&
        (result = validate_target_from_grave_source(player, card, player, 0), result != -1))
    {
      from_grave_to_hand(player, result, 1);
    }
    return 0;
  }
  instance = get_card_instance(player, card);
  new_default_test_definition(&test, 2, "Select target Dinosaur creature card.");
  test.subtype = 0xf0;
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = mana_producing_creature(player, card, EVENT_CAN_ACTIVATE, 0x18, COLOR_GREEN, 1);
    if (result != 0)
    {
      return 1;
    }
    prompt = NULL;
    event_00 = EVENT_CAN_ACTIVATE;
  }
  else
  {

    result = paying_mana();
    if ((result != 0) ||
        (result = generic_activated_ability_with_target_in_grave(
             player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME, 0, 0, 0, 0, 0, 0, 0, NULL,
             "Select target Dinosaur creature card.", TYPE_CREATURE, &test, 8),
         result == 0))
    {
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
    if (player != 1)
    {
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                           "Return a dino", 1, 1, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_MANA)
        return mana_producer(player, card, EVENT_ACTIVATE);
      if (result != CHOICE_DINO)
      {
        return 0;
      }
    }
    prompt = "Select target Dinosaur creature card.";
    event_00 = EVENT_ACTIVATE;
  }

  return generic_activated_ability_with_target_in_grave(player, card, event_00, GAA_SACRIFICE_ME, 0,
                                                        0, 0, 0, 0, 0, 0, NULL, prompt, TYPE_NONE,
                                                        &test, 8);
}

/* ManalinkEh 2022: 0x218f6a0; original cards/rivals_of_ixalan.c:1878. */
static int azor_the_lawbringer_legacy(int player, int card, event_t event)
{
  if ((event == EVENT_CAN_SKIP_TURN) && (1 - player == current_turn))
  {
    target_player_cant_cast_type(player, card, 1 - player, 0x38);
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2193290; original cards/rivals_of_ixalan.c:1888. */
int card_azor_the_lawbringer(int player, int card, event_t event)
{
  /* Azor, the Lawbringer	|2|W|W|U|U
   * Legendary Creature - Sphinx 6/6
   * Flying
   * When ~ enters the battlefield, each opponent can't cast instant or sorcery spells during that player's next turn.
   * Whenever ~ attacks, you may pay |X|W|U|U. If you do, you gain X life and draw X cards. */

  xtrigger_t trigger;
  declare_attackers_trigger_t mode;
  card_instance_t *instance;
  int card_or_result;

  bool legacy_found = false;
  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    if (0 < active_cards_count[player])
    {
      card_or_result = 0;
      do
      {
        if ((instance = in_play(player, card_or_result),
             instance == NULL || (is_what(player, card_or_result, 0x80) == 0)))
        {
          card_or_result = card_or_result + 1;
          continue;
        }

        instance = get_card_instance(player, card_or_result);
        if (instance->info_slot_fn == azor_the_lawbringer_legacy)
        {
          legacy_found = true;
          break;
        }
        card_or_result = card_or_result + 1;
      } while (card_or_result < active_cards_count[player]);
    }

    if (!legacy_found)
    {
      card_or_result = create_legacy_effect(player, card, azor_the_lawbringer_legacy);
      add_status(player, card_or_result, 0x10000);
    }
  }
  store_attackers(player, card, event, 0, player, card, NULL);
  trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    trigger = xtrigger_impl_value_dont_use_directly;
  }
  if (!(((event != EVENT_DECLARE_ATTACKERS) && (trigger != XTRIGGER_ATTACKING)) ||
        (card_or_result = has_mana_multi(player, 0, 0, 2, 0, 0, 1), card_or_result == 0)))
  {

    if ((player == 0) && (ai_is_speculating != 1))
    {
      card_or_result = duh_mode(0);
      if (card_or_result == 0)
      {
        mode = DAT_RESOLVE_TRIGGER_OPTIONAL;
      }
      else
      {
        mode = DAT_RESOLVE_TRIGGER_MANDATORY;
      }
    }
    else
    {
      card_or_result = has_mana_multi(player, 1, 0, 2, 0, 0, 1);
      mode = 0;
      if (card_or_result != 0)
        mode = DAT_RESOLVE_TRIGGER_MANDATORY;
    }
    dat_ability(player, card, event, mode, 1);
  }
  if ((event == EVENT_DAT_ABILITY) &&
      (card_or_result = charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER,
                                                          player, -1, 0, 2, 0, 0, 1),
       card_or_result != 0))
  {
    gain_life(player, x_value);
    draw_cards(player, x_value);
  }
  return 0;
}

/* Deadeye Brawler	|2|U|B
 * Creature - Human Pirate 2/4
 * Deathtouch
 * Ascend
 * Whenever ~ deals combat damage to a player, if you have the city's blessing, draw a card. */

/* Dire Fleet Neckbreaker	|2|B|R
 * Creature - Orc Pirate 3/2
 * Attacking Pirates you control get +2/+0. */

/* ManalinkEh 2022: 0x2193530; original cards/rivals_of_ixalan.c:1949. */
int card_elenda_the_dusk_rose(int player, int card, event_t event)
{
  /* Elenda, the Dusk Rose	|2|W|B
   * Legendary Creature - Vampire Knight 1/1
   * Lifelink
   * Whenever another creature dies, put a +1/+1 counter on ~.
   * When ~ dies, create X 1/1 |Swhite Vampire creature tokens with lifelink, where X is ~'s power. */

  uint16_t power_snapshot;
  unsigned int pending_deaths;
  int result;
  card_instance_t *instance;
  token_generation_t token;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  lifelink(player, card, event);
  default_test_definition(&this_test, 2);
  this_test.not_me = 1;
  count_for_gfp_ability(player, card, event, 2, 0, &this_test);
  pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_deaths != 0)
  {

    for (pending_deaths = pending_deaths & 0xff; pending_deaths != 0;
         pending_deaths = pending_deaths - 1)
    {
      add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    }
  }
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  default_token_definition(player, card, CARD_ID_VAMPIRE, &token);
  token.tou = 1;
  token.pow = 1;
  token.color_forced = ' ';
  token.s_key_plus = 2;
  instance = get_card_instance(player, card);
  power_snapshot = instance->power;
  token.qty = (unsigned char)power_snapshot;
  generate_token(&token);
  return 0;
}

/* ManalinkEh 2022: 0x21936a0; original cards/rivals_of_ixalan.c:1985. */
int card_hadanas_climb(int player, int card, event_t event)
{
  /* Hadana's Climb	|1|G|U
   * Legendary Enchantment
   * At the beginning of combat on your turn, put a +1/+1 counter on target creature you control. Then if that creature has three or more +1/+1 counters on it, transform ~. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {

    instance = get_card_instance(player, card);
    if (current_turn == player)
    {
      default_target_definition(player, card, &td, 2);
      td.allow_cancel = 0;
      instance->number_of_targets = '\0';
      td.preferred_controller = player;
      td.allowed_controller = player;
      result = can_target(&td);
      if (result != 0)
      {
        result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0);
        if (result != 0)
        {
          add_counters(player, card, instance->targets[0].player,
                                   instance->targets[0].card, COUNTER_P1_P1, 1);
          result =
              count_counters(instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1);
          if (2 < result)
          {
            transform(player, card);
          }
        }
      }
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x21937f0; original cards/rivals_of_ixalan.c:2016. */
int card_winged_temple_of_orazca(int player, int card, event_t event)
{
  /* Winged Temple of Orazca	""
   * Legendary Land
   * (Transforms from Hadana's Climb.)
   * |T: Add one mana of any color.
   * |1|G|U, |T: Target creature you control gains flying and gets +X/+X until end of turn, where X is its power. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_FLY = 2
  };

  int choice;
  int pow;
  int available_or_valid;
  card_instance_t *instance;
  target_definition_t *creature_target;
  target_definition_t td;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = player;
      get_card_instance(player, card);

      available_or_valid = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 1, 0, 1,
                                                     1, 0, 0, 0, &td, NULL);
      if ((available_or_valid != 0) && (paying_mana() == 0))
      {
        choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                             "Pump target creature", available_or_valid, 5, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice != CHOICE_MANA)
        {
          if (choice != CHOICE_FLY)
          {
            return 0;
          }
          add_state(player, card, 0x10);
          generic_activated_ability(player, card, EVENT_ACTIVATE, 5, 1, 0, 1, 1, 0, 0, 0, &td,
                                    "TARGET_CREATURE_YOU_CONTROL");
          if (spell_fizzled != 1)
          {
            return 0;
          }
          remove_state(player, card, 0x10);
          return 0;
        }
      }
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = player;
      instance = get_card_instance(player, card);
      creature_target = &td;
      if ((event == EVENT_RESOLVE_ACTIVATION) &&
          ((instance->info_slot == CHOICE_FLY &&
            (available_or_valid = validate_target(player, card, creature_target, 0),
             available_or_valid != 0))))
      {

        pow = get_power(instance->targets[0].player, instance->targets[0].card);
        pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                               pow, pow, 0x20, 0);
        return 0;
      }
      return 0;
    }
  }
  return mana_producer(player, card, event);
}

/* ManalinkEh 2022: 0x2193b30; original cards/rivals_of_ixalan.c:2081. */
int card_huatli_radiant_champion(int player, int card, event_t event)
{
  /* Huatli, Radiant Champion	|2|G|W
   * Legendary Planeswalker - Huatli (3)
   * +1: Put a loyalty counter on ~ for each creature you control.
   * -1: Target creature gets +X/+X until end of turn, where X is the number of creatures you control.
   * -8: You get an emblem with "Whenever a creature enters the battlefield under your control, you may draw a card." */

  enum
  {
    CHOICE_COUNTERS = 1,
    CHOICE_PUMP = 2,
    CHOICE_EMBLEM = 3
  };

  card_instance_t *instance;
  int result;
  int power;
  target_definition_t *creature_target;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(&td);
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                           "Get Loyalty from creatures", 1, 5, 1, "Pump a creature", result, 10,
                           0xffffffff, "Emblem", 1, 0x14, 0xfffffff8, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_PUMP)
      {
        new_pick_target(&td, "TARGET_CREATURE", 0, 1);
      }
      return planeswalker(player, card, event, 3);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 3);
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
  }
  creature_target = &td;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    result = instance->info_slot;
    if (result == CHOICE_COUNTERS)
    {
      result = count_subtype(instance->parent_controller, 2, -1);
      add_counters(player, card, instance->parent_controller, instance->parent_card,
                   COUNTER_LOYALTY, result);
    }
    else if (result == CHOICE_PUMP)
    {
      result = validate_target(player, card, creature_target, 0);
      if (result != 0)
      {
        result = count_subtype(instance->parent_controller, 2, -1);
        power = count_subtype(instance->parent_controller, 2, -1);
        pump_until_eot_merge_previous(player, card, instance->targets[0].player,
                                      instance->targets[0].card, power, result);
      }
    }
    else if (result == CHOICE_EMBLEM)
    {
      generate_token_by_id(player, card, CARD_ID_HUATLI_RADIANT_CHAMPION_EMBLEM);
    }
  }
  return planeswalker(player, card, event, 3);
}

/* ManalinkEh 2022: 0x2193e30; original cards/rivals_of_ixalan.c:2141. */
int card_huatli_radiant_champion_emblem(int player, int card, event_t event)
{
  /* Huatli, Radiant Champion Emblem	""
   * Emblem - Huatli
   * Whenever a creature enters the battlefield under your control, you may draw a card. */

  int result;
  result = duh_mode(player);
  etb_ability_global(player, event, (result != 0) + 1, TYPE_CREATURE, NULL, player, card);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  draw_cards(player, 1);
  return 0;
}

/* ManalinkEh 2022: 0x218f0d0; original cards/rivals_of_ixalan.c:2155. */
static int journey_to_eternity_legacy(int player, int card, event_t event)
{
  int count;
  int original_iid;
  card_instance_t *instance;
  int owner;
  int grave_index;

  instance = get_card_instance(player, card);
  if (resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) != 0)
  {

    seek_grave_for_id_to_reanimate(player, card, instance->targets[0].player,
                                   instance->targets[0].card, REANIMATE_DEFAULT);
  }
  other_leaves_play(player, card, instance->targets[1].player, instance->targets[1].card, event, 1,
                    0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {

    owner = instance->targets[2].player;
    original_iid = instance->targets[2].card;
    count = count_graveyard(owner);
    grave_index = count + -1;
    if (-1 < grave_index)
    {
      do
      {
        if (graveyard_ptr_mutable[owner][grave_index] == original_iid)
        {
          reanimate_permanent(player, card, owner, grave_index,
                              REANIMATE_RETURN_TO_PLAY_TRANSFORMED);
          break;
        }
        grave_index = grave_index + -1;
      } while (grave_index != -1);
    }
    kill_card(player, card, KILL_EXILE);
  }
  if (eot_trigger(player, card, event) == 0)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2193eb0; original cards/rivals_of_ixalan.c:2184. */
int card_journey_to_eternity(int player, int card, event_t event)
{
  /* Journey to Eternity	|1|B|G
   * Legendary Enchantment - Aura
   * Enchant creature you control
   * When enchanted creature dies, return it to the battlefield under your control, then return ~ to the battlefield transformed under your control. */

  card_instance_t *instance;
  card_instance_t *source_or_legacy;
  int target_or_owner;
  int dying;
  int player_00;
  target_definition_t td;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  instance = get_card_instance(player, card);
  source_or_legacy = in_play(player, card);
  if (source_or_legacy && instance->damage_target_player >= 0 &&
      !is_humiliated(player, card, AL_TRIG))
    nice_creature_to_sacrifice((int)instance->damage_target_player, instance->damage_target_card);
  if (instance->damage_target_player >= 0 && in_play(player, card) &&
      !is_humiliated(player, card, AL_TRIG) && event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    target_or_owner = instance->damage_target_card;
    if (((target_or_owner == affected_card) && (player_00 = (int)instance->damage_target_player,
                                                player_00 == affected_card_controller)) &&
        ((dying = is_this_dying(player_00, target_or_owner),
          dying != 0 &&
              (target_or_owner = check_for_death_trigger_removal(player_00, target_or_owner),
               target_or_owner == 0))))
    {

      source_or_legacy =
          get_card_instance(player, create_legacy_effect(player, card, journey_to_eternity_legacy));
      target_or_owner =
          get_owner((int)instance->damage_target_player, instance->damage_target_card);
      source_or_legacy->targets[0].player = target_or_owner;
      target_or_owner = get_id((int)instance->damage_target_player, instance->damage_target_card);
      source_or_legacy->targets[1].player = player;
      source_or_legacy->targets[1].card = card;
      source_or_legacy->targets[0].card = target_or_owner;
      target_or_owner = get_owner(player, card);
      source_or_legacy->targets[2].player = target_or_owner;
      target_or_owner = get_original_internal_card_id(player, card);
      source_or_legacy->targets[0xb].player = 1;
      source_or_legacy->targets[2].card = target_or_owner;
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
  td.allowed_controller = player;
  return aura(player, card, event, &td, "TARGET_CREATURE_YOU_CONTROL");
}
#endif

/* ManalinkEh 2022: 0x2194220; original cards/rivals_of_ixalan.c:2232. */
int card_atzal_cave_of_eternity(int player, int card, event_t event)
{
  /* Atzal, Cave of Eternity	""
   * Legendary Land
   * (Transforms from Journey to Eternity.)
   * |T: Add one mana of any color.
   * |3|B|G, |T: Return target creature card from your graveyard to the battlefield. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_REANIMATE = 2
  };

  int choice;
  int available_or_grave_index;
  card_instance_t *instance;
  test_definition_t test;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      new_default_test_definition(&test, 2, "Select target creature card.");
      get_card_instance(player, card);

      available_or_grave_index = generic_activated_ability_with_target_in_grave(
          player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, 3, 1, 0, 1, 0, 0, 0, NULL, NULL,
          TYPE_NONE, &test, 8);
      if ((available_or_grave_index != 0) && (paying_mana() == 0))
      {
        choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                             "Reanimate a creature", available_or_grave_index, 5, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice != CHOICE_MANA)
        {
          if (choice != CHOICE_REANIMATE)
          {
            return 0;
          }
          add_state(player, card, 0x10);
          generic_activated_ability_with_target_in_grave(player, card, EVENT_ACTIVATE, GAA_UNTAPPED,
                                                         3, 1, 0, 1, 0, 0, 0, NULL, NULL, TYPE_NONE,
                                                         &test, 8);
          if (spell_fizzled != 1)
          {
            return 0;
          }
          remove_state(player, card, 0x10);
          return 0;
        }
      }
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      new_default_test_definition(&test, 2, "Select target creature card.");
      instance = get_card_instance(player, card);

      if ((event == EVENT_RESOLVE_ACTIVATION) &&
          ((instance->info_slot == CHOICE_REANIMATE &&
            (available_or_grave_index = validate_target_from_grave_source(player, card, player, 0),
             available_or_grave_index != -1))))
      {
        reanimate_permanent(player, card, player, available_or_grave_index, REANIMATE_DEFAULT);
        return 0;
      }
      return 0;
    }
  }
  return mana_producer(player, card, event);
}

/* Jungle Creeper	|1|B|G
 * Creature - Elemental 3/3
 * |3|B|G: Return ~ from your graveyard to your hand. */

/* ManalinkEh 2022: 0x2194550; original cards/rivals_of_ixalan.c:2301. */
int card_kumena_tyrant_of_orazca(int player, int card, event_t event)
{
  /* Kumena, Tyrant of Orazca	|1|G|U
   * Legendary Creature - Merfolk Shaman 2/4
   * Tap another untapped Merfolk you control: ~ can't be blocked this turn.
   * Tap three untapped Merfolk you control: Draw a card.
   * Tap five untapped Merfolk you control: Put a +1/+1 counter on each Merfolk you control. */

  enum
  {
    CHOICE_UNBLOCKABLE = 1,
    CHOICE_DRAW = 2,
    CHOICE_PUMP = 3
  };

  int choice;
  card_instance_t *instance;
  int unblockable_available_or_tap_count;
  int draw_available;
  int pump_available;
  uint32_t unblockable_priority;
  target_definition_t *td_subtype;
  int32_t selected_mode;
  target_definition_t td;
  target_definition_t td1;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x47);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.required_subtype = 0x74;
    td.illegal_abilities = 0;
    td.illegal_state = TARGET_STATE_TAPPED;
    td.special = TARGET_SPECIAL_NOT_ME;
    default_target_definition(player, card, &td1, 0x47);
    td1.allowed_controller = player;
    td1.preferred_controller = player;
    td1.required_subtype = 0x74;
    td1.illegal_abilities = 0;
    td1.illegal_state = TARGET_STATE_TAPPED;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability_with_tapsubtype_as_cost(player, card, EVENT_CAN_ACTIVATE,
                                                            GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL,
                                                            NULL, &td1, 3) != 0)
      {
        return 1;
      }
      return generic_activated_ability_with_tapsubtype_as_cost(
          player, card, EVENT_CAN_ACTIVATE, GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, &td, 1);
    }
    if (event == EVENT_ACTIVATE)
    {

      unblockable_available_or_tap_count = generic_activated_ability_with_tapsubtype_as_cost(
          player, card, EVENT_CAN_ACTIVATE, GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, &td, 1);
      draw_available = generic_activated_ability_with_tapsubtype_as_cost(
          player, card, EVENT_CAN_ACTIVATE, GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, &td1, 3);
      pump_available = generic_activated_ability_with_tapsubtype_as_cost(
          player, card, EVENT_CAN_ACTIVATE, GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, &td1, 5);
      if (((current_turn == player) && (current_phase < 0x17)) &&
          (check_for_special_ability(player, card, 0x10) == 0))
      {
        unblockable_priority = 6;
      }
      else
      {
        unblockable_priority = 1;
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                           "Make Kumena unblockable", unblockable_available_or_tap_count,
                           unblockable_priority, "Draw a card", draw_available, 5,
                           "Pump your Merfolk", pump_available, 10, 0, 0, 0, 0);
      if (choice != 0)
      {
        if (choice == CHOICE_UNBLOCKABLE)
        {
          unblockable_available_or_tap_count = 1;
          td_subtype = &td;
        }
        else
        {
          if (choice == CHOICE_DRAW)
          {
            unblockable_available_or_tap_count = 3;
          }
          else
          {
            if (choice != CHOICE_PUMP)
            {
              return 0;
            }
            unblockable_available_or_tap_count = 5;
          }
          td_subtype = &td1;
        }
        return generic_activated_ability_with_tapsubtype_as_cost(
            player, card, EVENT_ACTIVATE, GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, td_subtype,
            unblockable_available_or_tap_count);
      }
      spell_fizzled = 1;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x47);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.required_subtype = 0x74;
    td.illegal_abilities = 0;
    td.illegal_state = TARGET_STATE_TAPPED;
    td.special = TARGET_SPECIAL_NOT_ME;
    default_target_definition(player, card, &td1, 0x47);
    td1.allowed_controller = player;
    td1.preferred_controller = player;
    td1.required_subtype = 0x74;
    td1.illegal_abilities = 0;
    td1.illegal_state = TARGET_STATE_TAPPED;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    selected_mode = instance->info_slot;
    if (selected_mode == CHOICE_UNBLOCKABLE)
    {
      pump_ability_until_eot_no_repeat(player, card, instance->parent_controller,
                                       instance->parent_card, 0, 0x10);
      selected_mode = instance->info_slot;
    }
    if (selected_mode == CHOICE_DRAW)
    {
      draw_a_card(player);
      selected_mode = instance->info_slot;
    }
    if (selected_mode == CHOICE_PUMP)
    {
      default_test_definition(&this_test, 0x47);
      this_test.subtype = 0x74;
      new_manipulate_all(instance->parent_controller, instance->parent_card, player, &this_test,
                         ACT_ADD_COUNTERS_BASE | ACT_KILL_BURY);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2194c00; original cards/rivals_of_ixalan.c:2389. */
int card_legion_lieutenant(int player, int card, event_t event)
{
  /* Legion Lieutenant	|W|B
   * Creature - Vampire Knight 2/2
   * Other Vampires you control get +1/+1. */

  if (1 < (uint32_t)(event - EVENT_POWER))
  {
    return 0;
  }
  boost_creature_type(player, card, event, SUBTYPE_VAMPIRE, 1, 1, 0, BCT_CONTROLLER_ONLY);
  return 0;
}

/* ManalinkEh 2022: 0x2194c60; original cards/rivals_of_ixalan.c:2400. */
int card_merfolk_mistbinder(int player, int card, event_t event)
{
  /* Merfolk Mistbinder	|G|U
   * Creature - Merfolk Shaman 2/2
   * Other Merfolk you control get +1/+1. */

  if (1 < (uint32_t)(event - EVENT_POWER))
  {
    return 0;
  }
  boost_creature_type(player, card, event, SUBTYPE_MERFOLK, 1, 1, 0, BCT_CONTROLLER_ONLY);
  return 0;
}

/* ManalinkEh 2022: 0x218f2c0; original cards/rivals_of_ixalan.c:2411. */
static int not_fs_ds_v_or_h(int unused, intptr_t unused2, int player, int card)
{
  int result;
  result = check_for_ability(player, card, 0x100);
  if (result == 0)
  {
    result = check_for_ability(player, card, 0x40000000);
    if (result == 0)
    {
      result = check_for_special_ability(player, card, 8);
      if (result == 0)
      {
        result = check_for_special_ability(player, card, 4);
        return (unsigned int)(result == 0);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x218f230; original cards/rivals_of_ixalan.c:2423. */
static int has_fs_ds_v_or_h(int unused, intptr_t unused2, int player, int card)
{
  int result;
  result = check_for_ability(player, card, 0x100);
  if (result == 0)
  {
    result = check_for_ability(player, card, 0x40000000);
    if (result == 0)
    {
      result = check_for_special_ability(player, card, 8);
      if (result == 0)
      {
        result = check_for_special_ability(player, card, 4);
        return (unsigned int)(result != 0);
      }
    }
  }
  return 1;
}

/* ManalinkEh 2022: 0x2194cc0; original cards/rivals_of_ixalan.c:2435. */
int card_path_of_mettle(int player, int card, event_t event)
{
  /* Path of Mettle	|R|W
   * Legendary Enchantment
   * When ~ enters the battlefield, it deals 1 damage to each creature that doesn't have first strike, double strike, vigilance, or haste.
   * Whenever you attack with at least two creatures that have first strike, double strike, vigilance, and/or haste, transform ~. */

  card_instance_t *instance;
  test_definition_t etb_test;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_test_definition(&etb_test, 2);
    etb_test.special_selection_function = not_fs_ds_v_or_h;
    new_damage_all(player, card, 2, 1, 0, &etb_test);
    dat_ability(player, card, EVENT_ETB, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  }
  else if (event == EVENT_DECLARE_ATTACKERS)
  {
    default_test_definition(&etb_test, 2);
    etb_test.special_selection_function = has_fs_ds_v_or_h;
    store_attackers(player, card, EVENT_DECLARE_ATTACKERS, DAT_TRACK, player, -1, &etb_test);
    dat_ability(player, card, EVENT_DECLARE_ATTACKERS, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  }
  else
  {
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      instance = get_card_instance(player, card);
      if (1 < instance->info_slot)
      {
        transform(player, card);
      }
    }
  }
  return global_enchantment(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2194e80; original cards/rivals_of_ixalan.c:2467. */
int card_metzali_tower_of_triumph(int player, int card, event_t event)
{
  /* Metzali, Tower of Triumph	""
   * Legendary Land
   * (Transforms from Path of Mettle.)
   * |T: Add one mana of any color.
   * |1|R, |T: Metzali deals 2 damage to each opponent.
   * |2|W, |T: Choose a creature at random that attacked this turn. Destroy that creature. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_DAMAGE = 2,
    CHOICE_DESTROY = 3
  };

  int choice;
  int mode_or_generic_cost;
  card_instance_t *instance;
  int white_cost;
  int red_cost;
  target_definition_t td;

  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
      return mana_producer(player, card, event);
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.required_state = current_phase > 0x1d ? 4 : 2;
    td.illegal_abilities = 0;
    if (event != EVENT_ACTIVATE)
    {
      if (event == EVENT_RESOLVE_ACTIVATION)
      {
        td.illegal_abilities = 0;
        mode_or_generic_cost = instance->info_slot;
        if (mode_or_generic_cost == 2)
        {
          damage_player(1 - player, 2, player, card);
          mode_or_generic_cost = instance->info_slot;
        }
        if (mode_or_generic_cost == 3)
        {
          if (target_available(player, card, &td) != 0)
          {
            int capacity = target_available(player, card, &td);
            int targ_array[capacity];
            memset(targ_array, 0xff, capacity * sizeof(int));
            int count = 0;
            for (int c = 0; c < active_cards_count[current_turn]; ++c)
            {
              instance = in_play(current_turn, c);
              if (instance != NULL && would_validate_arbitrary_target(&td, current_turn, c))
                targ_array[count++] = c;
            }
            int selected = internal_rand(target_available(player, card, &td));
            kill_card(current_turn, targ_array[selected], KILL_DESTROY);
          }
        }
      }
      return 0;
    }
    instance->info_slot = 0;
    if (paying_mana() == 0)
    {

      int abil_damage = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 1, 0, 0, 0,
                                                  1, 0, 0, NULL, NULL);
      int abil_destroy = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 2, 0, 0, 0,
                                                   0, 1, 0, NULL, NULL);
      int priority_destroy = -100;
      if (current_turn != player)
        priority_destroy = target_available(player, card, &td) ? 15 : 0;
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, "Produce mana", 1,
                           8, "Damage opponent", abil_damage, 10, "Destroy Creature that attacked",
                           abil_destroy, priority_destroy, 0, 0, 0, 0);

      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      instance->info_slot = choice;
      if (choice != CHOICE_MANA)
      {
        if (choice == CHOICE_DAMAGE)
        {
          add_state(player, card, 0x10);
          white_cost = 0;
          red_cost = 1;
          mode_or_generic_cost = 1;
        }
        else
        {
          if (choice != CHOICE_DESTROY)
          {
            return 0;
          }
          add_state(player, card, 0x10);
          white_cost = 1;
          red_cost = 0;
          mode_or_generic_cost = 2;
        }
        generic_activated_ability(player, card, EVENT_ACTIVATE, 1, mode_or_generic_cost, 0, 0, 0,
                                  red_cost, white_cost, 0, NULL, NULL);
        if (spell_fizzled != 1)
        {
          return 0;
        }
        remove_state(player, card, 0x10);
        return 0;
      }
    }
    else
    {
      instance->info_slot = 1;
    }
    event = EVENT_ACTIVATE;
  }
  return mana_producer(player, card, event);
}
#endif

/* ManalinkEh 2022: 0x21953f0; original cards/rivals_of_ixalan.c:2548. */
int card_profane_procession(int player, int card, event_t event)
{
  /* Profane Procession	|1|W|B
   * Legendary Enchantment
   * |3|W|B: Exile target creature. Then if there are three or more cards exiled with ~, transform it. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  default_target_definition(player, card, &td, 2);
  if ((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE))
  {
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      result = validate_target(player, card, &td, 0);
      if (result != 0)
      {
        exile_card_and_remember_it_on_exiledby(instance->parent_controller, instance->parent_card,
                                               instance->targets[0].player,
                                               instance->targets[0].card);
      }
      result = exiledby_count(player, instance->parent_card, 2);
      if (2 < result)
      {
        transform(player, instance->parent_card);
      }
    }
    result = global_enchantment(player, card, event);
    return result;
  }
  result = generic_activated_ability(player, card, event, 4, 3, 1, 0, 0, 0, 1, 0, &td,
                                     "TARGET_CREATURE");
  return result;
}

/* ManalinkEh 2022: 0x2195590; original cards/rivals_of_ixalan.c:2572. */
int card_tomb_of_the_dusk_rose(int player, int card, event_t event)
{
  /* Tomb of the Dusk Rose	""
   * Legendary Land
   * (Transforms from Profane Procession.)
   * |T: Add one mana of any color.
   * |2|W|B, |T: Put a creature card exiled with this permanent onto the battlefield under your control. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_RETURN = 2
  };

  unsigned int packed_exile;
  int result;
  card_instance_t *instance;
  int *remembered_entry;
  int32_t *chosen_entry;
  card_instance_t *returned_record;
  int legacy_card;
  bool paid;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      instance = get_card_instance(player, card);
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 3, 1, 0, 0, 0, 1, 0,
                                         NULL, NULL);
      if (((result != 0) &&
           (result = exiledby_choose(player, card, 0x4759, EXBY_FIRST_FOUND, 0, "Card", 1),
            result != 0)) &&
          (result = paying_mana(), result == 0))
      {
        result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                             "Return a creature", 1, 5, 0, 0, 0, 0);
        if (result == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (result != CHOICE_MANA)
        {
          if (result != CHOICE_RETURN)
          {
            return 0;
          }

          add_state(player, card, 0x10);
          generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 2, 1, 0, 0, 0, 1, 0, NULL,
                                    NULL);
          chosen_entry =
              (int32_t *)exiledby_choose(player, card, 0x4759, EXBY_CHOOSE, 0, "creature", 1);
          if (chosen_entry == NULL)
          {
            spell_fizzled = 1;
          }
          else
          {
            instance->targets[1].player = -1;
            paid = spell_fizzled != 1;
            instance->targets[1].card = *chosen_entry;
            if (paid)
            {
              return 0;
            }
          }
          remove_state(player, card, 0x10);
          return 0;
        }
      }
      event = EVENT_ACTIVATE;
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot == CHOICE_RETURN)
      {

        remembered_entry =
            exiledby_find(player, instance->parent_card, instance->targets[1].card, NULL, NULL);
        if (remembered_entry != NULL)
        {

          *remembered_entry = -1;
          packed_exile = instance->targets[1].card;
          result = remove_card_from_rfg(
              packed_exile >> 0x1f,
              (unsigned int)*(
                  unsigned short *)(cards_data[0].cc +
                                    ((packed_exile & 0x7fffffff) + packed_exile * 8) * 8 + -7));
          if (result != 0)
          {

            result = add_card_to_hand(player, packed_exile & 0x7fffffff);
            if (player != packed_exile >> 0x1f)
            {
              returned_record = get_card_instance(player, result);
              returned_record->state = returned_record->state ^ 0x1000;
            }
            legacy_card = create_targetted_legacy_effect(player, instance->parent_card, empty, player,
                                                     result);
            add_status(player, legacy_card, 0x10000);
            put_into_play(player, result);
            return 0;
          }
        }
        spell_fizzled = 1;
        return 0;
      }
      return 0;
    }
  }

  return mana_producer(player, card, event);
}

/* Protean Raider	|1|U|R
 * Creature - Shapeshifter Pirate 2/2
 * Raid - If you attacked with a creature this turn, you may have ~ enter the battlefield as a copy of any creature on the battlefield. */

/* ManalinkEh 2022: 0x2195990; original cards/rivals_of_ixalan.c:2661. */
int card_raging_regisaur(int player, int card, event_t event)
{
  /* Raging Regisaur	|2|R|G
   * Creature - Dinosaur 4/4
   * Whenever ~ attacks, it deals 1 damage to any target. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    instance->number_of_targets = '\0';
    result = pick_target(&td, "TARGET_ANY");
    if (result != 0)
    {
      damage_target0(player, card, 1);
    }
  }
  return 0;
}

/* Relentless Raptor	|R|W
 * Creature - Dinosaur 3/3
 * Vigilance
 * ~ attacks or blocks each combat if able. */

/* Resplendent Griffin	|1|W|U
 * Creature - Griffin 2/2
 * Flying
 * Ascend
 * Whenever ~ attacks, if you have the city's blessing, put a +1/+1 counter on it. */

/* ManalinkEh 2022: 0x2195a40; original cards/rivals_of_ixalan.c:2695. */
int card_siegehorn_ceratops(int player, int card, event_t event)
{
  /* Siegehorn Ceratops	|G|W
   * Creature - Dinosaur 2/2
   * Enrage - Whenever ~ is dealt damage, put two +1/+1 counters on it. */

  int result;

  result = is_humiliated(player, card, AL_TRIG);
  if (result == 0)
  {
    result = damage_dealt_to_me_arbitrary(player, card, event, DDBM_0, player, card);
    if (result != 0)
    {
      add_counters(player, card, player, card, COUNTER_P1_P1, 2);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2195ad0; original cards/rivals_of_ixalan.c:2706. */
int card_storm_fleet_sprinter(int player, int card, event_t event)
{
  /* Storm Fleet Sprinter	|1|U|R
   * Creature - Human Pirate 2/2
   * Haste
   * ~ can't be blocked. */

  haste(player, card, event);
  unblockable(player, card, event);
  return 0;
}

/* ManalinkEh 2022: 0x2195b10; original cards/rivals_of_ixalan.c:2718. */
int card_storm_the_vault(int player, int card, event_t event)
{
  /* Storm the Vault	|2|U|R
   * Legendary Enchantment
   * Whenever one or more creatures you control deal combat damage to a player, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color."
   * At the beginning of your end step, if you control five or more artifacts, transform ~. */

  card_instance_t *instance;
  int result;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  instance = combat_damage_being_dealt(event);
  if (((instance != NULL) && (instance->damage_source_player == player)) &&
      (instance->damage_target_card == -1))
  {
    result = check_special_flags(player, instance->damage_source_card, 0x40);
    if ((result == 0) && ((instance->targets[3].player & 2) != 0))
    {
      instance = get_card_instance(player, card);
      instance->targets[8].player = 1;
    }
  }
  result = resolve_damage_trigger(player, card, event, DDBM_0, player, card);
  if (result != 0)
  {
    generate_token_by_id(player, card, CARD_ID_TREASURE);
  }
  if (((current_turn == player) && (event == EVENT_PHASE_CHANGED)) && (current_phase == 0x22))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      result = count_permanents_by_type(player, TYPE_ARTIFACT);
      if (4 < result)
      {
        transform(player, card);
      }
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2195c70; original cards/rivals_of_ixalan.c:2747. */
int card_vault_of_catlacan(int player, int card, event_t event)
{
  /* Vault of Catlacan	""
   * Legendary Land
   * (Transforms from Storm the Vault.)
   * |T: Add one mana of any color.
   * |T: Add |U for each artifact you control. */

  int result;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  if (event == EVENT_CAN_ACTIVATE)
  {

    result = mana_producer(player, card, EVENT_CAN_ACTIVATE);
    if ((result != 0) &&
        (((ai_is_speculating != 1 && ((player != 1 || (((unsigned char)trace_mode & 2) != 0)))) ||
          (result = count_subtype(player, 0x40, -1), result != 0))))
    {
      return 1;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    result = count_subtype(player, 0x40, -1);
    result = do_dialog(player, player, card, -1, -1,
                       " Produce one mana of any color\n Tolarian Academy\n Cancel",
                       (unsigned int)(1 < result));
    if (result == 0)
    {
      event = EVENT_ACTIVATE;

      return mana_producer(player, card, event);
    }
    if (result == 1)
    {
      result = count_subtype(player, 0x40, -1);
      produce_mana_tapped(player, card, COLOR_BLUE, result);
    }
    else
    {
      spell_fizzled = 1;
    }
  }
  else
  {
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      if (event != EVENT_COUNT_MANA)
      {
        return 0;
      }
    }
    else if (((event != EVENT_COUNT_MANA) || (card != affected_card)) ||
             (player != affected_card_controller))
      return mana_producer(player, card, event);
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = mana_producer(player, card, EVENT_CAN_ACTIVATE);
      if ((result != 0) && (result = count_subtype(player, 0x40, -1), 1 < result))
      {
        result = count_subtype(player, 0x40, -1);
        declare_mana_available(player, 2, result);
        return 0;
      }
      result = mana_producer(player, card, EVENT_CAN_ACTIVATE);
      if (result != 0)
      {
        declare_mana_available(player, 7, 1);
        return 0;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2195ed0; original cards/rivals_of_ixalan.c:2809. */
int card_zacama_primal_calamity(int player, int card, event_t event)
{
  /* Zacama, Primal Calamity	|6|R|G|W
   * Legendary Creature - Elder Dinosaur 9/9
   * Vigilance, reach, trample
   * When ~ enters the battlefield, if you cast it, untap all lands you control.
   * |2|R: ~ deals 3 damage to target creature.
   * |2|G: Destroy target artifact or enchantment.
   * |2|W: You gain 3 life. */

  enum
  {
    CHOICE_DAMAGE = 1,
    CHOICE_DESTROY = 2,
    CHOICE_LIFE = 3
  };

  int choice;
  card_instance_t *instance;
  int red_available_or_green_cost;
  int green_available_or_red_cost;
  int life_available;
  int32_t selected_mode;
  target_definition_t *td;
  const char *prompt;
  target_definition_t td1;
  target_definition_t td2;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  vigilance(player, card, event);
  if (event == EVENT_ETB)
  {
    if (check_special_flags(player, card, 0x40000) == 0)
    {
      manipulate_type(player, card, player, TYPE_LAND, ACT_UNTAP);
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td1, 2);
      default_target_definition(player, card, &td2, 0x44);
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        red_available_or_green_cost = generic_activated_ability(
            player, card, EVENT_CAN_ACTIVATE, 4, 2, 0, 0, 0, 1, 0, 0, &td1, "TARGET_CREATURE");
        if ((red_available_or_green_cost == 0) &&
            (red_available_or_green_cost =
                 generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 2, 0, 0, 1, 0, 0, 0,
                                           &td2, "TARGET_ARTIFACT_ENCHANTMENT"),
             red_available_or_green_cost == 0))
        {
          return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 2, 0, 0, 0, 0, 1, 0,
                                           NULL, NULL);
        }
        return 1;
      }
      if (event == EVENT_ACTIVATE)
      {

        red_available_or_green_cost = generic_activated_ability(
            player, card, EVENT_CAN_ACTIVATE, 4, 2, 0, 0, 0, 1, 0, 0, &td1, "TARGET_CREATURE");
        green_available_or_red_cost =
            generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 2, 0, 0, 1, 0, 0, 0,
                                      &td2, "TARGET_ARTIFACT_ENCHANTMENT");
        life_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 2, 0, 0, 0,
                                                   0, 1, 0, NULL, NULL);
        choice = dialog_impl(
            player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Damage creature",
            red_available_or_green_cost, 10, "Destroy artifact or enchantment",
            green_available_or_red_cost, 10, "Gain Life", life_available,
            0xfU - life[player] & (int)(0xfU - life[player] ^ 0xffffffff) >> 0x1f, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice == CHOICE_DAMAGE)
        {
          td = &td1;
          prompt = "TARGET_CREATURE";
          green_available_or_red_cost = 1;
          red_available_or_green_cost = 0;
        }
        else
        {
          if (choice != CHOICE_DESTROY)
          {
            if (choice != CHOICE_LIFE)
            {
              return 0;
            }
            return generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 2, 0, 0, 0, 0, 1, 0,
                                             NULL, NULL);
          }
          td = &td2;
          prompt = "TARGET_ARTIFACT_ENCHANTMENT";
          green_available_or_red_cost = 0;
          red_available_or_green_cost = 1;
        }
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 4, 2, 0, 0,
                                         red_available_or_green_cost, green_available_or_red_cost,
                                         0, 0, td, prompt);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      {
        return 0;
      }
      default_target_definition(player, card, &td1, 2);
      default_target_definition(player, card, &td2, 0x44);
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      selected_mode = instance->info_slot;
      if (selected_mode == CHOICE_DAMAGE)
      {
        damage_target0(player, card, 3);
        selected_mode = instance->info_slot;
      }
      if (selected_mode == CHOICE_DESTROY)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        selected_mode = instance->info_slot;
      }
      if (selected_mode == CHOICE_LIFE)
      {
        gain_life(player, 3);
      }
    }
  }
  return 0;
}

/* Awakened Amalgam	|4
 * Artifact Creature - Golem 100/100
 * ~'s power and toughness are each equal to the number of differently named lands you control. */

/* ManalinkEh 2022: 0x2196530; original cards/rivals_of_ixalan.c:2900. */
int card_azors_gateway(int player, int card, event_t event)
{
  /* Azor's Gateway	|2
   * Legendary Artifact
   * |1, |T: Draw a card, then exile a card from your hand. If cards with five or more different converted mana costs are exiled with ~, you gain 5 life, untap ~, and transform it. */

  enum
  {
    CHOICE_DRAW_EXILE = 1,
    CHOICE_SHOW_EXILED = 2
  };

  int choice;
  card_instance_t *instance;
  int selected_or_count;
  int original_iid;
  int leg;
  int idx;
  char buffer[100];
  test_definition_t this_test;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if ((player == 0) &&
          (selected_or_count = exiledby_choose(0, card, 0x46f0, EXBY_FIRST_FOUND, 0, "Card", 1),
           selected_or_count != 0))
      {
        return 1;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 1, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      selected_or_count = exiledby_choose(player, card, 0x46f0, EXBY_FIRST_FOUND, 0, "Card", 1);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Draw and exile",
                           generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 1, 0, 0,
                                                     0, 0, 0, 0, NULL, NULL),
                           5, "Show exiled cards",
                           (unsigned int)(selected_or_count != 0 && player == 0), 0, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice != CHOICE_SHOW_EXILED)
      {
        if (choice != CHOICE_DRAW_EXILE)
        {
          return 0;
        }
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 1, 0, 0, 0, 0, 0, 0, NULL,
                                         NULL);
      }
      exiledby_choose(player, card, 0x46f0, EXBY_CHOOSE, 0, "Card", 1);
      spell_fizzled = 1;
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
  if ((event == EVENT_RESOLVE_ACTIVATION) && (instance->info_slot == CHOICE_DRAW_EXILE))
  {
    draw_cards(player, 1);
    if (0 < hand_count[player])
    {

      leg = 0;
      idx = 0;
      scnprintf(buffer, 100, "Select a card to exile");
      new_default_test_definition(&this_test, 0xff, buffer);
      selected_or_count = new_select_a_card(player, player, 4, 1, 3, -1, &this_test);
      original_iid = get_original_internal_card_id(player, selected_or_count);
      exiledby_remember(player, instance->parent_card, player, original_iid, &leg, &idx);
      rfg_card_in_hand(player, selected_or_count);
      exile_card_and_remember_it_on_exiledby(player, card, player, selected_or_count);
    }
    selected_or_count = exiledby_count_cmcs(player, instance->parent_card, 2);
    if (4 < selected_or_count)
    {
      gain_life(player, 5);
      untap_card(player, instance->parent_card);
      transform(player, instance->parent_card);
      return 0;
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x21969f0; original cards/rivals_of_ixalan.c:2977. */
int card_sanctum_of_the_sun(int player, int card, event_t event)
{
  /* Sanctum of the Sun	""
   * Legendary Land
   * (Transforms from Azor's Gateway.)
   * |T: Add X mana of any one color, where X is your life total. */

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (!mana_producer(player, card, EVENT_CAN_ACTIVATE))
      return 0;
    if (ai_is_speculating != 1 && (player != 1 || ((unsigned char)trace_mode & 2)))
      return 1;
    return life[player] >= 1;
  }
  if (event == EVENT_ACTIVATE)
  {
    if (mana_colors_added[player])
    {
      int choice = do_dialog(player, player, card, -1, -1,
                             " Basic land ability\n Multiple Mana ability\n Cancel", 1);
      if (!choice)
      {
        produce_mana_tapped_all_one_color(player, card, mana_colors_added[player], 1);
        return 0;
      }
      if (choice == 2)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
    produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, life[player]);
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION || event == EVENT_CLEANUP)
    return 0;
  if (event != EVENT_COUNT_MANA || card != affected_card || player != affected_card_controller)
    return mana_producer(player, card, event);
  if (mana_producer(player, card, EVENT_CAN_ACTIVATE))
  {
    declare_mana_available_hex(player, 0x3e, life[player]);
    if (life[player] < 1 && mana_colors_added[player] != COLOR_TEST_0)
      declare_mana_available_maybe_hex(player, mana_colors_added[player], 1);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2196c50; original cards/rivals_of_ixalan.c:3033. */
int card_captains_hook(int player, int card, event_t event)
{
  /* Captain's Hook	|3
   * Artifact - Equipment
   * Equipped creature gets +2/+0, has menace, and is a Pirate in addition to its other creature types.
   * Whenever ~ becomes unattached from a permanent, destroy that permanent.
   * Equip |1 */

  int result;
  card_instance_t *instance;

  leaves_play(player, card, event, 1, 0);
  if ((event == EVENT_UNATTACH) || (event == EVENT_LEAVES_PLAY_ABILITY))
  {
    instance = is_equipping(player, card);
    if (instance != NULL)
    {

      instance = get_card_instance(player, card);
      if (instance->damage_target_player == player)
      {
        kill_card(player, instance->damage_target_card, KILL_SACRIFICE);
      }
    }
  }
  attached_is_additional_subtype(player, card, event, SUBTYPE_PIRATE);
  result = vanilla_equipment(player, card, event, 1, 2, 0, 0, 0x10000);
  return result;
}

/* Gleaming Barrier	|2
 * Artifact Creature - Wall 0/4
 * Defender
 * When ~ dies, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* ManalinkEh 2022: 0x218f350; original cards/rivals_of_ixalan.c:3061. */
static int golden_guardian_legacy(int player, int card, event_t event)
{
  char attached_player;
  card_instance_t *instance;
  int result;
  int owner;
  int position;

  instance = get_card_instance(player, card);
  other_leaves_play(player, card, (int)instance->damage_target_player, instance->damage_target_card,
                    event, 1, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    attached_player = instance->damage_target_player;
    result = instance->damage_target_card;
    instance->targets[6].player = (int)attached_player;
    instance->targets[6].card = result;
    result = find_in_owners_graveyard((int)attached_player, result, &owner, &position);
    if (result != 0)
    {

      remove_card_from_grave(owner, position);
      result = get_internal_card_id_from_csv_id(CARD_ID_GOLD_FORGE_GARRISON);
      result = add_card_to_hand(player, result);
      set_special_flags3(player, result, 0x201);
      put_into_play(player, result);
      dispatch_event_with_initial_event_result(player, result,
                                               EVENT_ANOTHER_PERMANENT_HAS_TRANSFORMED, 0);
      verify_legend_rule(player, result, (unsigned int)cards_data[instance->targets[0xc].card].id);
    }
    kill_card(player, card, KILL_EXILE);
  }
  result = eot_trigger(player, card, event);
  if (result == 0)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x2196d40; original cards/rivals_of_ixalan.c:3084. */
int card_golden_guardian(int player, int card, event_t event)
{
  /* Golden Guardian	|4
   * Artifact Creature - Golem 4/4
   * Defender
   * |2: ~ fights another target creature you control. When ~ dies this turn, return it to the battlefield transformed under your control. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  double_faced_card(player, card, event);
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.special = TARGET_SPECIAL_NOT_ME;
    td.allowed_controller = player;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.special = TARGET_SPECIAL_NOT_ME;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      instance = get_card_instance(player, card);
      fight(instance->parent_controller, instance->parent_card, instance->targets[0].player,
            instance->targets[0].card);
      create_targetted_legacy_effect_no_repeat(player, card, golden_guardian_legacy,
                                               instance->parent_controller, instance->parent_card);
    }
  }
  result = generic_activated_ability(player, card, event, 4, 2, 0, 0, 0, 0, 0, 0, &td,
                                     "SELECT_A_CREATURE_YOU_CONTROL");
  return result;
}

/* ManalinkEh 2022: 0x2196eb0; original cards/rivals_of_ixalan.c:3110. */
int card_gold_forge_garrison(int player, int card, event_t event)
{
  /* Gold-Forge Garrison	""
   * Land
   * (Transforms from Golden Guardian.)
   * |T: Add two mana of any one color.
   * |4, |T: Create a 4/4 colorless Golem artifact creature token. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_TOKEN = 2
  };

  int choice;
  card_instance_t *instance;
  int available_or_result;
  token_generation_t token;

  double_faced_card(player, card, event);
  if (event == EVENT_COUNT_MANA)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (available_or_result = mana_producer(player, card, EVENT_CAN_ACTIVATE),
         available_or_result != 0))
    {
      declare_mana_available_maybe_hex(player, COLOR_TEST_ANY_COLORED, 2);
    }
    get_card_instance(player, card);
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    available_or_result = mana_producer(player, card, EVENT_CAN_ACTIVATE);
  }
  else
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      available_or_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 6, 0, 0,
                                                      0, 0, 0, 0, NULL, NULL);
      if ((available_or_result != 0) && (paying_mana() == 0))
      {
        choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                             "Create a token", available_or_result, 5, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice != CHOICE_MANA)
        {
          if (choice != CHOICE_TOKEN)
          {
            return 0;
          }
          add_state(player, card, 0x10);
          generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 4, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL);
          if (spell_fizzled != 1)
          {
            return 0;
          }
          remove_state(player, card, 0x10);
          return 0;
        }
      }
      instance->info_slot = 0;
      return produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 2);
    }
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      return 0;
    }
    if (instance->info_slot != CHOICE_TOKEN)
    {
      return 0;
    }
    default_token_definition(player, card, CARD_ID_GOLEM, &token);
    token.tou = 4;
    token.pow = 4;
    generate_token(&token);
    available_or_result = 0;
    instance->info_slot = 0;
  }
  return available_or_result;
}

/* ManalinkEh 2022: 0x21971d0; original cards/rivals_of_ixalan.c:3178. */
int card_the_immortal_sun(int player, int card, event_t event)
{
  /* The Immortal Sun	|6
   * Legendary Artifact
   * Players can't activate planeswalkers' loyalty abilities.
   * At the beginning of your draw step, draw an additional card.
   * Spells you cast cost |1 less to cast.
   * Creatures you control get +1/+1. */

  card_instance_t *instance;

  if (event == EVENT_ABILITIES)
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {
      if (is_what(affected_card_controller, affected_card, 0x100))
      {
        instance = get_card_instance(affected_card_controller, affected_card);
        instance->targets[0x11].card = instance->targets[0x11].card | 0x20;
      }
    }
  }
  else if (event == EVENT_DRAW_PHASE)
  {
    if (current_turn == player)
    {
      if (!is_humiliated(player, card, AL_STATIC))
      {
        event_result = event_result + 1;
      }
    }
  }
  else if ((event == EVENT_MODIFY_COST_GLOBAL) && (affected_card_controller == player))
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {
      COST_COLORLESS = COST_COLORLESS + -1;
    }
  }
  boost_creature_type(player, card, event, ~SUBTYPE_INVALID, 1, 1, 0, BCT_CONTROLLER_ONLY);
  return 0;
}

/* Orazca Relic	|3
 * Artifact
 * Ascend
 * |T: Add |C.
 * |T, Sacrifice ~: You gain 3 life and draw a card. Activate this ability only if you have the city's blessing. */

/* ManalinkEh 2022: 0x2197320; original cards/rivals_of_ixalan.c:3206. */
int card_silent_gravestone(int player, int card, event_t event)
{
  /* Silent Gravestone	|1
   * Artifact
   * Cards in graveyards can't be the targets of spells or abilities.
   * |4, |T: Exile ~ and all cards from all graveyards. Draw a card. */

  card_instance_t *instance;
  card_instance_t *source_in_play;

  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    APNAP(p, {
      rfg_whole_graveyard(p);
    });
    draw_a_card(player);
    instance = get_card_instance(player, card);
    source_in_play = in_play(instance->parent_controller, instance->parent_card);
    if (source_in_play != NULL)
    {
      kill_card(instance->parent_controller, instance->parent_card, KILL_EXILE);
    }
  }
  return generic_activated_ability(player, card, event, 1, 4, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* Arch of Orazca	""
 * Land
 * Ascend
 * |T: Add |C.
 * |5, |T: Draw a card. Activate this ability only if you have the city's blessing. */

/* Vraska, Scheming Gorgon	|4|B|B
 * Legendary Planeswalker - Vraska (5)
 * +2: Creatures you control get +1/+0 until end of turn.
 * -3: Destroy target creature.
 * -10: Until end of turn, creatures you control gain deathtouch and "Whenever this creature deals damage to an opponent, that player loses the game." */

/* Vampire Champion	|3|B
 * Creature - Vampire Soldier 3/3
 * Deathtouch */

/* Vraska's Conquistador	|1|B
 * Creature - Vampire Soldier 2/1
 * Whenever ~ attacks or blocks, if you control a Vraska planeswalker, target opponent loses 2 life and you gain 2 life. */

/* Vraska's Scorn	|2|B|B
 * Sorcery
 * Target opponent loses 4 life. You may search your library and/or graveyard for a card named Vraska, Scheming Gorgon, reveal it, and put it into your hand. If you search your library this way, shuffle it. */

/* ManalinkEh 2022: 0x2197420; original cards/rivals_of_ixalan.c:3274. */
int card_angrath_minotaur_pirate(int player, int card, event_t event)
{
  /* Angrath, Minotaur Pirate	|4|B|R
   * Legendary Planeswalker - Angrath (5)
   * +2: ~ deals 1 damage to target opponent or planeswalker and each creature that player or that planeswalker's controller controls.
   * -3: Return target Pirate card from your graveyard to the battlefield.
   * -11: Destroy all creatures target opponent controls. ~ deals damage to that player equal to their total power. */

  enum
  {
    CHOICE_DAMAGE = 1,
    CHOICE_REANIMATE = 2,
    CHOICE_KILL_DAMAGE = 3
  };

  int p;
  int c;
  int choice;
  card_instance_t *instance;
  int available_or_mode;
  int first_player_or_available;
  int loyalty_count;
  int toughness_or_creature;
  int damage_priority;
  target_definition_t *creature_target;
  target_definition_t td;
  target_definition_t td_player;
  test_definition_t test;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x100);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    default_target_definition(player, card, &td_player, 2);
    td_player.zone = TARGET_ZONE_PLAYERS;
    new_default_test_definition(&test, 0x47, "Select target Pirate card.");
    test.subtype = 0x95;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      available_or_mode = generic_activated_ability_with_target_in_grave(
          player, card, EVENT_CAN_ACTIVATE, GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE,
          &test, 8);
      if ((player == 1) && (loyalty_count = count_counters(1, card, COUNTER_LOYALTY),
                            first_player_or_available = current_turn, 1 < loyalty_count))
      {

        damage_priority = 1;
        c = active_cards_count[current_turn] + -1;
        if (-1 < c)
        {
          do
          {
            if (((first_player_or_available != 1) &&
                 (instance = in_play(first_player_or_available, c), instance != NULL)) &&
                (toughness_or_creature = is_what(first_player_or_available, c, 2),
                 toughness_or_creature != 0))
            {
              toughness_or_creature = get_toughness(first_player_or_available, c);
              instance = get_card_instance(first_player_or_available, c);
              if (toughness_or_creature - (unsigned int)instance->damage_on_card == 1)
              {
                damage_priority = damage_priority + 3;
              }
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
            if (((p != 1) && (instance = in_play(p, c), instance != NULL)) &&
                (toughness_or_creature = is_what(p, c, 2), toughness_or_creature != 0))
            {
              toughness_or_creature = get_toughness(p, c);
              instance = get_card_instance(p, c);
              if (toughness_or_creature - (unsigned int)instance->damage_on_card == 1)
              {
                damage_priority = damage_priority + 3;
              }
            }
            c = c + -1;
          } while (c != -1);
        }
      }
      else
      {
        damage_priority = 1;
      }
      first_player_or_available = opponent_is_valid_target(player, card);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                           "1 damage to opp and opp creatures", can_target(&td), damage_priority, 2,
                           "Resurrect a Pirate", available_or_mode, 6, 0xfffffffd,
                           "Wipe creatures & damage", first_player_or_available, 0xf, 0xfffffff5, 0,
                           0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_DAMAGE)
      {
        new_pick_target(&td, "TARGET_OPPONENT_OR_PLANESWALKER", 0, 1);
      }
      else if (choice == CHOICE_REANIMATE)
      {
        generic_activated_ability_with_target_in_grave(player, card, EVENT_ACTIVATE, GAA_NONE, 0, 0,
                                                       0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test,
                                                       8);
      }
      else if (choice == CHOICE_KILL_DAMAGE)
      {
        target_opponent(player, card);
      }
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    default_target_definition(player, card, &td, 0x100);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    default_target_definition(player, card, &td_player, 2);
    td_player.zone = TARGET_ZONE_PLAYERS;
    new_default_test_definition(&test, 0x47, "Select target Pirate card.");
    test.subtype = 0x95;
    instance = get_card_instance(player, card);
  }
  creature_target = &td;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    available_or_mode = instance->info_slot;
    if (available_or_mode == 1)
    {
      if (validate_target(player, card, creature_target, 0) != 0)
      {
        damage_target0(player, card, 1);
        new_damage_all(player, card, instance->targets[0].player, 1, 1, NULL);
      }
    }
    else if (available_or_mode == 2)
    {

      available_or_mode = validate_target_from_grave_source(player, card, player, 0);
      if (available_or_mode != -1)
      {
        reanimate_permanent(player, card, player, available_or_mode, REANIMATE_DEFAULT);
      }
    }
    else if ((available_or_mode == 3) &&
             (available_or_mode = validate_target(player, card, &td_player, 0),
              available_or_mode != 0))
    {

      available_or_mode = get_power_sum(instance->targets[0].player);
      default_test_definition(&this_test, 2);
      new_manipulate_all(player, card, 1 - player, &this_test, ACT_KILL_DESTROY);
      damage_target0(player, card, available_or_mode);
    }
  }
  return planeswalker(player, card, event, 5);
}

/* Angrath's Ambusher	|2|B
 * Creature - Orc Pirate 2/3
 * ~ gets +2/+0 as long as you control an Angrath planeswalker. */

/* Swab Goblin	|1|R
 * Creature - Goblin Pirate 2/2 */

/* Angrath's Fury	|3|B|R
 * Sorcery
 * Destroy target creature. ~ deals 3 damage to target player or planeswalker. You may search your library and/or graveyard for a card named Angrath, Minotaur Pirate, reveal it, and put it into your hand. If you search your library this way, shuffle it. */
