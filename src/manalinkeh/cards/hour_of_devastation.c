#include "manalink.h"

// Sandblast => reprint from FRF

// Strategic Planning => reprint from P3K

// Unsummon => reprint from LEA

// Kindled Fury => reprint from LOR

// Manalith => reprint from M12

// Traveler's Amulet => reprint from ISD

// Woodland Stream => reprint from SOI

// Cinder Barrens => reprint from OGW

static int hou_finality_effect(int player, int card, event_t event);
static int hou_god(int player, int card, event_t event);
static int hou_god_legacy(int player, int card, event_t event);

static int afflict(int player, int card, event_t event, int amount);

int eternalize(int player, int card, event_t event, card_id_t cid, int x, int b, int u, int g,
               int r, int w);

#ifndef SHANDALAR

int card_god_pharaohs_gift(int player, int card, event_t event);
int card_majestic_myriarch(int player, int card, event_t event);

#endif

/* ManalinkEh 2022: 0x20cd780; original cards/hour_of_devastation.c:4. */
int eternalize(int player, int card, event_t event, card_id_t cid, int x, int b, int u, int g,
               int r, int w)
{
  int result;
  token_generation_t token;

  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = has_mana_multi(player, x, b, u, g, r, w);
    if ((result != 0) &&
        (result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY), result != 0))
    {
      return 0x1020;
    }
  }
  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    charge_mana_multi(player, x, b, u, g, r, w);
    if (spell_fizzled != 1)
    {
      return 2;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATED_GRAVEYARD_ABILITY)
  {
    default_token_definition(player, card, cid, &token);
    token.color_forced = '\x02';
    token.no_sleight = '\x01';
    token.tou = 4;
    token.pow = 4;
    token.special_flags3 = 0x40000;
    generate_token(&token);
  }
  return (unsigned int)(event == EVENT_CAN_CAST);
}

/* ManalinkEh 2022: 0x20cd6f0; original cards/hour_of_devastation.c:37. */
static int hou_god_legacy(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;
  result = eot_trigger(player, card, event);
  if (result == 0)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  result = instance->targets[0].player;
  seek_grave_for_id_to_reanimate(result, -1, result,
                                 (unsigned int)cards_data[instance->targets[0].card].id,
                                 REANIMATE_RETURN_TO_HAND);
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x20cd640; original cards/hour_of_devastation.c:50. */
static int hou_god(int player, int card, event_t event)
{
  int created_legacy;
  card_instance_t *instance;
  int owner_or_iid;
  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) == 0)
  {
    return 0;
  }
  created_legacy = create_legacy_effect(player, card, hou_god_legacy);
  instance = get_card_instance(player, created_legacy);
  owner_or_iid = get_owner(player, card);
  instance->targets[0].player = owner_or_iid;
  instance = get_card_instance(player, created_legacy);
  owner_or_iid = get_original_internal_card_id(player, card);
  instance->targets[0].card = owner_or_iid;
  instance = get_card_instance(player, created_legacy);
  instance->targets[0xb].player = 1;
  return 0;
}

/* ManalinkEh 2022: 0x20cd4e0; original cards/hour_of_devastation.c:62. */
static int hou_finality_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;
  if (((current_phase == 1) && (event == EVENT_UNTAP)) && (affected_card_controller == player))
  {
    result = is_what(player, affected_card, 1);
    if (result != 0)
    {
      instance = get_card_instance(affected_card_controller, affected_card);
      instance->untap_status = instance->untap_status & 0xfffffffc;
    }
  }
  if ((trigger_condition == TRIGGER_UPKEEP) && (current_turn == player))
  {
    instance = get_card_instance(player, card);
    result = instance->targets[1].card + -1;
    instance->targets[1].card = result;
    if (result < 1)
    {
      kill_card(player, card, KILL_EXILE);
    }
  }
  return 0;
}

#ifndef SHANDALAR
static int counter_activated_or_triggered_ability_cycling(int player, int card, event_t event,
                                                          int colorless, int black, int blue,
                                                          int green, int red, int white)
{
  int32_t ability_player;
  int32_t ability_card;
  int opponent_or_result;
  card_instance_t *instance;
  int count_or_valid;
  int cost_increasers;
  unsigned int cycling_cost;
  target_definition_t td1;
  target_definition_t td2;
  if (2U < (unsigned int)event - (unsigned int)EVENT_CAN_ACTIVATE_FROM_HAND)
  {
    return 0;
  }
  counter_activated_target_definition(player, card, &td1, 0xff);
  opponent_or_result = 1 - player;
  td1.allowed_controller = opponent_or_result;
  counter_triggered_target_definition(player, card, &td2, 0xff);
  td2.allowed_controller = opponent_or_result;
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE_FROM_HAND)
  {
    opponent_or_result = count_cards_by_id(player, CARD_ID_FLUCTUATOR);
    count_or_valid = count_cards_by_id(player, CARD_ID_SUPPRESSION_FIELD);
    cycling_cost = colorless - 2 * opponent_or_result + 2 * count_or_valid;
    cycling_cost = cycling_cost & (int)(cycling_cost ^ 0xffffffff) >> 0x1f;
    opponent_or_result =
        can_counter_activated_ability(player, card, EVENT_CAN_ACTIVATE_FROM_HAND, &td1);
    if (((opponent_or_result == 0) ||
         (opponent_or_result = has_mana_multi(player, cycling_cost, black, blue, green, red, white),
          opponent_or_result == 0)) &&
        ((opponent_or_result =
              can_counter_triggered_ability(player, card, EVENT_CAN_ACTIVATE_FROM_HAND, &td2),
          opponent_or_result == 0 || (opponent_or_result = has_mana_multi(
                                          player, cycling_cost, black, blue, green, red, white),
                                      opponent_or_result == 0))))
    {
      opponent_or_result = has_mana_multi(player, cycling_cost, black, blue, green, red, white);
      return (unsigned int)(opponent_or_result != 0);
    }
    return 99;
  }
  if (event != EVENT_ACTIVATE_FROM_HAND)
  {
    if (spell_fizzled == 1)
    {
      return 0;
    }
    if ((player_bits[player] & PB_GY_ABILITIES_DISABLED) != 0)
    {
      return 0;
    }
    opponent_or_result = instance->info_slot;
    if (opponent_or_result == 1)
    {
      resolve_counter_activated_ability(player, card, NULL, 0);
      opponent_or_result = instance->info_slot;
    }
    if (opponent_or_result == 2)
    {
      resolve_counter_triggered_ability(player, card, NULL, 0);
    }
    draw_a_card(player);
    return 0;
  }
  count_or_valid = count_cards_by_id(player, CARD_ID_FLUCTUATOR);
  cost_increasers = count_cards_by_id(player, CARD_ID_SUPPRESSION_FIELD);
  cycling_cost = colorless - 2 * count_or_valid + 2 * cost_increasers;
  bool offer_activated = true;
  if ((((stack_size < 1) ||
        (count_or_valid = would_validate_arbitrary_target(&td1, stack_cards[stack_size + -1].player,
                                                          stack_cards[stack_size + -1].card),
         count_or_valid == 0)) ||
       ((char)stack_data[stack_size - 1].generating_event == '~')) ||
      ((cards_data[stack_data[(stack_size + -1)].internal_card_id].id == 0x388 ||
        (opponent_or_result != stack_cards[stack_size + -1].player))))
  {
    offer_activated = false;
    if (stack_size >= 2)
    {
      count_or_valid = would_validate_arbitrary_target(&td1, stack_cards[stack_size + -2].player,
                                                       stack_cards[stack_size + -2].card);
      if (((count_or_valid == 0) ||
           (1 < (unsigned char)((char)stack_data[stack_size - 2].generating_event + 0x94U))) ||
          (opponent_or_result != stack_cards[stack_size + -2].player))
      {
        if ((((1 < stack_size) &&
              (count_or_valid = would_validate_arbitrary_target(&td2, player, card),
               count_or_valid != 0)) &&
             ((char)stack_data[stack_size - 2].generating_event == '~')) &&
            ((opponent_or_result == stack_cards[stack_size + -2].player &&
              (opponent_or_result =
                   do_dialog(player, player, card, -1, -1, " Counter triggered ability\n Pass", 0),
               opponent_or_result == 0))))
        {
          if (1 < stack_size)
          {
            ability_player = stack_cards[stack_size - 2].player;
            ability_card = stack_cards[stack_size - 2].card;
            ai_modifier = ai_modifier + 100;
            instance->number_of_targets = instance->number_of_targets + '\x01';
            instance->targets[0].player = ability_player;
            instance->targets[0].card = ability_card;
          }
          instance->info_slot = 2;
        }
      }
      else
        offer_activated = true;
    }
  }
  if (offer_activated &&
      do_dialog(player, player, card, -1, -1, " Counter activated ability\n Pass", 0) == 0)
  {
    if (1 < stack_size)
    {
      ability_player = stack_cards[stack_size - 2].player;
      ability_card = stack_cards[stack_size - 2].card;
      ai_modifier = ai_modifier + 100;
      instance->number_of_targets = instance->number_of_targets + '\x01';
      instance->targets[0].player = ability_player;
      instance->targets[0].card = ability_card;
    }
    instance->info_slot = 1;
  }
  charge_mana_multi(player, cycling_cost & (int)(cycling_cost ^ 0xffffffff) >> 0x1f, black, blue,
                    green, red, white);
  if (spell_fizzled != 1)
  {
    card_is_being_cycled = 1;
    discard_card(player, card);
    card_is_being_cycled = 0;
    return 2;
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x20cd5c0; original cards/hour_of_devastation.c:184. */
static int afflict(int player, int card, event_t event, int amount)
{
  int result;

  if (event != EVENT_DECLARE_BLOCKERS)
  {
    return 0;
  }
  result = is_attacking(player, card);
  if (result != 0)
  {
    result = is_unblocked(player, card);
    if (result == 0)
    {
      result = is_humiliated(player, card, AL_TRIG);
      if (result == 0)
      {
        lose_life(1 - current_turn, amount);
      }
    }
  }
  return 0;
}

/* Act of Heroism	|1|W
 * Instant
 * Untap target creature. It gets +2/+2 until end of turn and can block an additional creature this turn. */

/* ManalinkEh 2022: 0x20cd8b0; original cards/hour_of_devastation.c:198. */
int card_adorned_pouncer(int player, int card, event_t event)
{
  /* Adorned Pouncer	|1|W
   * Creature - Cat 1/1
   * Double strike
   * Eternalize |3|W|W */

  int result;

  result = eternalize(player, card, event, CARD_ID_ADORNED_POUNCER, 3, 0, 0, 0, 0, 2);
  return result;
}

/* ManalinkEh 2022: 0x20cd910; original cards/hour_of_devastation.c:207. */
int card_angel_of_condemnation(int player, int card, event_t event)
{
  /* Angel of Condemnation	|2|W|W
   * Creature - Angel 3/3
   * Flying, vigilance
   * |2|W, |T: Exile another target creature. Return that card to the battlefield under its owner's control at the beginning of the next end step.
   * |2|W, |T, Exert ~: Exile another target creature until ~ leaves the battlefield. */

  enum
  {
    CHOICE_BLINK = 1,
    CHOICE_OBLIVIATE = 2
  };

  card_instance_t *instance;
  int result;
  target_definition_t td;

  vigilance(player, card, event);
  return_from_oblivion(player, card, event);
  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 2);
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {

      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Blink a creature", 1, 5,
                           "Obliviate a creature", 1, 5, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
      }
      else if (result == CHOICE_BLINK)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 0x40005, 2, 0, 0, 0, 0, 1, 0, &td,
                                  "TARGET_ANOTHER_CREATURE");
      }
      else if (result == CHOICE_OBLIVIATE)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 0x40005, 2, 0, 0, 0, 0, 1, 0, &td,
                                  "TARGET_ANOTHER_CREATURE");
        does_not_untap_effect(player, card, player, card, 0, 1);
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (instance->info_slot == CHOICE_BLINK)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          remove_until_eot(instance->parent_controller, instance->parent_card,
                           instance->targets[0].player, instance->targets[0].card);
          instance->number_of_targets = '\0';
        }
      }
      else if ((instance->info_slot == CHOICE_OBLIVIATE) && (result = valid_target(&td), result != 0))
      {
        obliviation(instance->parent_controller, instance->parent_card, instance->targets[0].player,
                    instance->targets[0].card);
        instance->number_of_targets = '\0';
      }
    }
    return 0;
  }
  result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x40005, 2, 0, 0, 0, 0, 1, 0,
                                     &td, "TARGET_ANOTHER_CREATURE");
  return result;
}

/* Angel of the God-Pharaoh	|4|W|W
 * Creature - Angel 4/4
 * Flying
 * Cycling |2 */

/* Aven of Enduring Hope	|4|W	=>portal_1_2_3k.c:card_angel_of_mercy 0x20056e2
 * Creature - Bird Cleric 3/3
 * Flying
 * When ~ enters the battlefield, you gain 3 life. */

/* ManalinkEh 2022: 0x20cdc30; original cards/hour_of_devastation.c:276. */
int card_crested_sunmare(int player, int card, event_t event)
{
  /* Crested Sunmare	|3|W|W
   * Creature - Horse 5/5
   * Other Horses you control have indestructible.
   * At the beginning of each end step, if you gained life this turn, create a 5/5 |Swhite Horse creature token. */

  int result;
  token_generation_t token;
  boost_subtype(player, card, event, SUBTYPE_HORSE, 0, 0, 0, 0x800, BCT_CONTROLLER_ONLY);
  result = eot_trigger(player, card, event);
  if (result != 0)
  {
    result = get_trap_condition(player, 1);
    if (result != 0)
    {
      default_token_definition(player, card, CARD_ID_HORSE, &token);
      token.tou = 5;
      token.pow = 5;
      token.color_forced = ' ';
      generate_token(&token);
      return 0;
    }
  }
  return 0;
}

/* Dauntless Aven	|2|W
 * Creature - Bird Warrior 2/1
 * Flying
 * Whenever ~ attacks, untap target creature you control. */

/* Desert's Hold	|2|W
 * Enchantment - Aura
 * Enchant creature
 * When ~ enters the battlefield, if you control a Desert or there is a Desert card in your graveyard, you gain 3 life.
 * Enchanted creature can't attack or block, and its activated abilities can't be activated. */

/* Disposal Mummy	|2|W
 * Creature - Zombie Jackal 2/3
 * When ~ enters the battlefield, exile target card from an opponent's graveyard. */

/* ManalinkEh 2022: 0x20cdd00; original cards/hour_of_devastation.c:310. */
int card_djeru_with_eyes_wide_open(int player, int card, event_t event)
{
  /* Djeru, With Eyes Open	|3|W|W
   * Legendary Creature - Human Warrior 4/3
   * Vigilance
   * When ~ enters the battlefield, you may search your library for a planeswalker card, reveal it, put it into your hand, then shuffle your library.
   * If a source would deal damage to a planeswalker you control, prevent 1 of that damage. */

  card_instance_t *instance;
  int result;
  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 0xff, "Select a Planeswalker card.");
    test.subtype = 0x1006;
    new_global_tutor(player, player, 1, 1, 0, 1, &test);
  }
  instance = damage_being_prevented(event);
  if ((instance != NULL) && (instance->damage_target_player == player))
  {
    result = damage_is_to_planeswalker(instance);
    if (result != 0)
    {
      instance->info_slot = instance->info_slot + -1;
    }
  }
  return 0;
}

/* Djeru's Renunciation	|1|W
 * Instant
 * Tap up to two target creatures.
 * Cycling |W */

/* Dutiful Servants	|3|W	0x401000
 * Creature - Zombie 2/5 */

/* Gideon's Defeat	|W
 * Instant
 * Exile target |Swhite creature that's attacking or blocking. If it was a Gideon planeswalker, you gain 5 life. */

/* God-Pharaoh's Faithful	|W
 * Creature - Human Wizard 0/4
 * Whenever you cast a |Sblue, |Sblack, or |Sred spell, you gain 1 life. */

/* ManalinkEh 2022: 0x20cddd0; original cards/hour_of_devastation.c:349. */
int card_hour_of_revelation(int player, int card, event_t event)
{
  /* Hour of Revelation	|3|W|W|W
   * Sorcery
   * This spell costs |3 less to cast if there are ten or more nonland permanents on the battlefield.
   * Destroy all nonland permanents. */

  int result;
  test_definition_t this_test;
  if (event == EVENT_MODIFY_COST)
  {
    result = count_subtype(2, 0x146, -1);
    if (9 < result)
    {
      COST_COLORLESS = COST_COLORLESS + -3;
    }
    return 0;
  }
  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&this_test, 1);
    this_test.type_flag = 1;
    new_manipulate_all(player, card, 2, &this_test, ACT_KILL_DESTROY);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* Mummy Paramount	|1|W
 * Creature - Zombie 2/2
 * Whenever another Zombie enters the battlefield under your control, ~ gets +1/+1 until end of turn. */

/* Oketra's Avenger	|1|W
 * Creature - Human Warrior 3/1
 * You may exert ~ as it attacks. When you do, prevent all combat damage that would be dealt to it this turn. */

/* ManalinkEh 2022: 0x20cdf20; original cards/hour_of_devastation.c:382. */
int card_oketras_last_mercy(int player, int card, event_t event)
{
  /* Oketra's Last Mercy	|1|W|W
   * Sorcery
   * Your life total becomes equal to your starting life total. Lands you control don't untap during your next untap step. */

  card_instance_t *instance;
  int result;
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
      {
        return 0;
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      result = get_starting_life_total(player);
      set_life_total(player, result);
      result = create_legacy_effect(player, card, hou_finality_effect);
      instance = get_card_instance(player, result);
      instance->targets[1].card = 1;
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x20ce000; original cards/hour_of_devastation.c:400. */
int card_overwhelming_splendor(int player, int card, event_t event)
{
  /* Overwhelming Splendor	|6|W|W
   * Enchantment - Aura Curse
   * Enchant player
   * Creatures enchanted player controls lose all abilities and have base power and toughness 1/1.
   * Enchanted player can't activate abilities that aren't mana abilities or loyalty abilities. */

  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  result = instance->targets[0].player;
  if ((uint32_t)(event - EVENT_POWER) < 3)
  {
    if (affected_card_controller == result)
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        instance = in_play(affected_card_controller, affected_card);
        if (instance != NULL)
        {
          result = is_what(affected_card_controller, affected_card, 0x47);
          if (result != 0)
          {
            instance = get_card_instance(affected_card_controller, affected_card);
            instance->targets[0x11].card = instance->targets[0x11].card | 8;
          }
          result = is_what(affected_card_controller, affected_card, 2);
          if (result != 0)
          {
            if ((uint32_t)(event - EVENT_POWER) < 2)
            {
              set_base_pt(1);
            }
            else
            {

              event_result = event_result & 0x1f000000;
              humiliate(player, card, affected_card_controller, affected_card, 1);
              instance = get_card_instance(affected_card_controller, affected_card);
              instance->state = instance->state & 0xffffdfff;
            }
            return curse(player, card, event);
          }
        }
      }
      return 0;
    }
  }
  else if ((((event == EVENT_STATIC_EFFECTS) && (card == affected_card)) &&
            (affected_card_controller == player)) &&
           (-1 < result))
  {
    player_bits[result] =
        player_bits[result] | 0x3000000;
  }
  return curse(player, card, event);
}

/* Saving Grace	|1|W
 * Enchantment - Aura
 * Flash
 * Enchant creature you control
 * When ~ enters the battlefield, all damage that would be dealt this turn to you and permanents you control is dealt to enchanted creature instead.
 * Enchanted creature gets +0/+3. */

/* Solemnity	|2|W
 * Enchantment
 * Players can't get counters.
 * Counters can't be put on artifacts, creatures, enchantments, or lands. */

/* Solitary Camel	|2|W
 * Creature - Camel 3/2
 * ~ has lifelink as long as you control a Desert or there is a Desert card in your graveyard. */

/* ManalinkEh 2022: 0x20ce1c0; original cards/hour_of_devastation.c:461. */
int card_steadfast_sentinel(int player, int card, event_t event)
{
  /* Steadfast Sentinel	|3|W
   * Creature - Human Cleric 2/3
   * Vigilance
   * Eternalize |4|W|W */

  int result;

  vigilance(player, card, event);
  result = eternalize(player, card, event, CARD_ID_STEADFAST_SENTINEL, 4, 0, 0, 0, 0, 2);
  return result;
}

/* Steward of Solidarity	|1|W
 * Creature - Human Warrior 2/2
 * |T, Exert ~: Create a 1/1 |Swhite Warrior creature token with vigilance. */

/* Sunscourge Champion	|2|W
 * Creature - Human Wizard 2/3
 * When ~ enters the battlefield, you gain life equal to its power.
 * Eternalize-|2|W|W, Discard a card. */

/* Unconventional Tactics	|2|W
 * Sorcery
 * Target creature gets +3/+3 and gains flying until end of turn.
 * Whenever a Zombie enters the battlefield under your control, you may pay |W. If you do, return ~ from your graveyard to your hand. */

/* Vizier of the True	|3|W
 * Creature - Human Cleric 3/2
 * You may exert ~ as it attacks.
 * Whenever you exert a creature, tap target creature an opponent controls. */

/* Aerial Guide	|2|U
 * Creature - Drake 2/2
 * Flying
 * Whenever ~ attacks, another target attacking creature gains flying until end of turn. */

/* Aven Reedstalker	|3|U	=>time_spiral.c:card_ashcoat_bear 0x200cb63
 * Creature - Bird Warrior 2/3
 * Flash
 * Flying */

/* ManalinkEh 2022: 0x20ce240; original cards/hour_of_devastation.c:500. */
int card_champion_of_wits(int player, int card, event_t event)
{
  /* Champion of Wits	|2|U
   * Creature - Naga Wizard 2/1
   * When ~ enters the battlefield, you may draw cards equal to its power. If you do, discard two cards.
   * Eternalize |5|U|U */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    result = do_dialog(player, player, card, -1, -1, " Draw & Discard\n Pass", 0);
    if (result == 0)
    {
      result = get_power(player, card);
      draw_cards(player, result);
      new_multidiscard(player, 2, 0, player);
    }
  }
  result = eternalize(player, card, event, CARD_ID_CHAMPION_OF_WITS, 5, 0, 2, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x20ce340; original cards/hour_of_devastation.c:517. */
int card_countervailing_winds(int player, int card, event_t event)
{
  /* Countervailing Winds	|2|U
   * Instant
   * Counter target spell unless its controller pays |1 for each card in your graveyard.
   * Cycling |2 */

  int graveyard_size;
  int opponent_mana;

  if ((uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND) < 3)
  {
    return cycling(player, card, event, 2, 0, 0, 0, 0, 0);
  }
  if (event == EVENT_CAN_CAST ||
      (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller))
  {
    if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
    {
      graveyard_size = count_graveyard(player);
      opponent_mana = has_mana(1 - player, 7, count_graveyard(player));
      if (opponent_mana <= graveyard_size)
      {
        ai_modifier = ai_modifier + 0x32;
      }
    }
    return counterspell(player, card, event, NULL, 0);
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    counterspell_resolve_unless_pay_x(player, card, NULL, 0, count_graveyard(player));
    kill_card(player, card, KILL_DESTROY);
  }
  return 0;
}

/* ManalinkEh 2022: 0x20ce4e0; original cards/hour_of_devastation.c:544. */
int card_cunning_survivor(int player, int card, event_t event)
{
  /* Cunning Survivor	|1|U
   * Creature - Human Warrior 1/3
   * Whenever you cycle or discard a card, ~ gets +1/+0 until end of turn and can't be blocked this turn. */

  if (!is_humiliated(player, card, AL_TRIG))
  {
    if (discard_trigger(player, card, event, player, 2, 0))
    {
      pump_ability_until_eot(player, card, player, card, 1, 0, 0, 0x10);
      return 0;
    }
  }
  return 0;
}

/* Eternal of Harsh Truths	|2|U
 * Creature - Zombie Cleric 1/3
 * Afflict 2
 * Whenever ~ attacks and isn't blocked, draw a card. */

/* Fraying Sanity	|2|U
 * Enchantment - Aura Curse
 * Enchant player
 * At the beginning of each end step, enchanted player puts the top X cards of their library into their graveyard, where X is the number of cards put into that graveyard from anywhere this turn. */

/* ManalinkEh 2022: 0x20ce580; original cards/hour_of_devastation.c:565. */
int card_hour_of_eternity(int player, int card, event_t event)
{
  /* Hour of Eternity	|X|X|U|U|U
   * Sorcery
   * Exile X target creature cards from your graveyard. For each card exiled this way, create a token that's a copy of that card, except it's a 4/4 |Sblack Zombie. */

  int i;
  card_instance_t *instance;
  int grave_count_or_index;
  int cost_or_target_count;
  token_generation_t token;
  test_definition_t test;

  instance = get_card_instance(player, card);
  new_default_test_definition(&test, 2, "Select a creature card to exile.");
  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 4, NULL, NULL, 0, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      set_special_flags2(player, card, 0x20000);
      cost_or_target_count = played_for_free(player, card);
      if ((cost_or_target_count == 0) &&
          (cost_or_target_count = is_token(player, card), cost_or_target_count == 0))
      {
        instance->info_slot = 0;

        if (player == 0)
        {
          cost_or_target_count = count_graveyard_by_type(0, 2);
          max_x_value = cost_or_target_count * 2;
          cost_or_target_count = charge_mana_for_double_x(0, 7);
          set_x_for_x_spells(0, card, EVENT_CAST_SPELL, cost_or_target_count / 2);
        }
        else
        {

          cost_or_target_count = count_graveyard_by_type(player, 2);
          cost_or_target_count = cost_or_target_count * 2;
          while (grave_count_or_index = has_mana(player, 7, cost_or_target_count),
                 grave_count_or_index == 0)
          {
            cost_or_target_count = cost_or_target_count + -2;
          }
          if (cost_or_target_count == 0)
          {
            ai_modifier = ai_modifier + -0x32;
          }
          charge_mana(player, COLOR_ANY, cost_or_target_count);
          if (spell_fizzled != 1)
          {
            set_x_for_x_spells(player, card, EVENT_CAST_SPELL, cost_or_target_count);
          }
        }
      }

      cost_or_target_count = instance->info_slot;
      grave_count_or_index = count_graveyard_by_type(player, 2);
      if (grave_count_or_index < cost_or_target_count)
      {
        cost_or_target_count = grave_count_or_index;
      }
      cost_or_target_count = select_multiple_cards_from_graveyard(
          player, player, 0, 1, &test, cost_or_target_count, instance->targets + 1);
      instance->targets[0].player = cost_or_target_count;
      if (cost_or_target_count < 0)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {

    i = 1;
    if (0 < instance->targets[0].player)
    {
      do
      {
        grave_count_or_index = validate_target_from_grave(player, card, player, i);
        if (grave_count_or_index == -1)
        {
          spell_fizzled = 1;
          return 0;
        }

        i = i + 1;
        default_token_definition(
            player, card,
            (unsigned int)cards_data[graveyard_ptr_mutable[player][grave_count_or_index]].id,
            &token);
        token.action = TOKEN_ACTION_ADD_SUBTYPE;
        token.pow = 4;
        token.tou = 4;
        token.action_argument = 0xde;
        token.color_forced = '\x02';
        generate_token(&token);
        grave_count_or_index = instance->targets[0].player;
      } while (i <= grave_count_or_index);
      if (0 < grave_count_or_index)
      {
        i = 1;
        do
        {

          grave_count_or_index = validate_target_from_grave(player, card, player, i);
          i = i + 1;
          rfg_card_from_grave(player, grave_count_or_index);
        } while (i <= instance->targets[0].player);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return 0;
}

/* Imaginary Threats	|2|U|U
 * Instant
 * Creatures target opponent controls attack this turn if able. During that player's next untap step, creatures they control don't untap.
 * Cycling |2 */

/* Jace's Defeat	|1|U
 * Instant
 * Counter target |Sblue spell. If it was a Jace planeswalker spell, scry 2. */

/* ManalinkEh 2022: 0x20ce920; original cards/hour_of_devastation.c:647. */
int card_kefnets_last_word(int player, int card, event_t event)
{
  /* Kefnet's Last Word	|2|U|U
   * Sorcery
   * Gain control of target artifact, creature, or enchantment. Lands you control don't untap during your next untap step. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.illegal_type = TYPE_LAND | TARGET_TYPE_PLANESWALKER;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      result = instance->targets[0].card;
      instance = get_card_instance(player, card);
      gain_control_permanently(player, instance->targets[0].player, result);
    }
    result = create_legacy_effect(player, card, hou_finality_effect);
    instance = get_card_instance(player, result);
    instance->targets[1].card = 1;
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td,
                         "Select target artifact, creature, or enchantment", 1, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20ceab0; original cards/hour_of_devastation.c:672. */
int card_nimble_obstructionist(int player, int card, event_t event)
{
  /* Nimble Obstructionist	|2|U
   * Creature - Bird Wizard 3/1
   * Flash
   * Flying
   * Cycling |2|U
   * When you cycle ~, counter target activated or triggered ability you don't control. */

  int32_t ability_player;
  int32_t ability_card;
  int opponent_or_result;
  card_instance_t *instance;
  int count_or_valid;
  int cost_increasers;
  unsigned int cycling_cost;
  target_definition_t td1;
  target_definition_t td2;
  if (2U < (unsigned int)event - (unsigned int)EVENT_CAN_ACTIVATE_FROM_HAND)
  {
    return flash(player, card, event);
  }
  counter_activated_target_definition(player, card, &td1, 0xff);
  opponent_or_result = 1 - player;
  td1.allowed_controller = opponent_or_result;
  counter_triggered_target_definition(player, card, &td2, 0xff);
  td2.allowed_controller = opponent_or_result;
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE_FROM_HAND)
  {
    opponent_or_result = count_cards_by_id(player, CARD_ID_FLUCTUATOR);
    count_or_valid = count_cards_by_id(player, CARD_ID_SUPPRESSION_FIELD);
    cycling_cost = ((1 - opponent_or_result) + count_or_valid) * 2;
    cycling_cost = cycling_cost & (int)(cycling_cost ^ 0xffffffff) >> 0x1f;
    opponent_or_result =
        can_counter_activated_ability(player, card, EVENT_CAN_ACTIVATE_FROM_HAND, &td1);
    if (((opponent_or_result == 0) ||
         (opponent_or_result = has_mana_multi(player, cycling_cost, 0, 1, 0, 0, 0),
          opponent_or_result == 0)) &&
        ((opponent_or_result =
              can_counter_triggered_ability(player, card, EVENT_CAN_ACTIVATE_FROM_HAND, &td2),
          opponent_or_result == 0 ||
              (opponent_or_result = has_mana_multi(player, cycling_cost, 0, 1, 0, 0, 0),
               opponent_or_result == 0))))
    {
      opponent_or_result = has_mana_multi(player, cycling_cost, 0, 1, 0, 0, 0);
      return (unsigned int)(opponent_or_result != 0);
    }
    return 99;
  }
  if (event != EVENT_ACTIVATE_FROM_HAND)
  {
    if (spell_fizzled == 1)
    {
      return 0;
    }
    if ((player_bits[player] & PB_GY_ABILITIES_DISABLED) != 0)
    {
      return 0;
    }
    opponent_or_result = instance->info_slot;
    if (opponent_or_result == 1)
    {
      resolve_counter_activated_ability(player, card, NULL, 0);
      opponent_or_result = instance->info_slot;
    }
    if (opponent_or_result == 2)
    {
      resolve_counter_triggered_ability(player, card, NULL, 0);
    }
    draw_a_card(player);
    return 0;
  }
  count_or_valid = count_cards_by_id(player, CARD_ID_FLUCTUATOR);
  cost_increasers = count_cards_by_id(player, CARD_ID_SUPPRESSION_FIELD);
  cycling_cost = ((1 - count_or_valid) + cost_increasers) * 2;
  bool offer_activated = true;
  if ((((stack_size < 1) ||
        (count_or_valid = would_validate_arbitrary_target(&td1, stack_cards[stack_size + -1].player,
                                                          stack_cards[stack_size + -1].card),
         count_or_valid == 0)) ||
       ((char)stack_data[stack_size - 1].generating_event == '~')) ||
      ((cards_data[stack_data[(stack_size + -1)].internal_card_id].id == 0x388 ||
        (opponent_or_result != stack_cards[stack_size + -1].player))))
  {
    offer_activated = false;
    if (stack_size >= 2)
    {
      count_or_valid = would_validate_arbitrary_target(&td1, stack_cards[stack_size + -2].player,
                                                       stack_cards[stack_size + -2].card);
      if (((count_or_valid == 0) ||
           (1 < (unsigned char)((char)stack_data[stack_size - 2].generating_event + 0x94U))) ||
          (opponent_or_result != stack_cards[stack_size + -2].player))
      {
        if ((((1 < stack_size) &&
              (count_or_valid = would_validate_arbitrary_target(&td2, player, card),
               count_or_valid != 0)) &&
             ((char)stack_data[stack_size - 2].generating_event == '~')) &&
            ((opponent_or_result == stack_cards[stack_size + -2].player &&
              (opponent_or_result =
                   do_dialog(player, player, card, -1, -1, " Counter triggered ability\n Pass", 0),
               opponent_or_result == 0))))
        {
          if (1 < stack_size)
          {
            ability_player = stack_cards[stack_size - 2].player;
            ability_card = stack_cards[stack_size - 2].card;
            ai_modifier = ai_modifier + 100;
            instance->number_of_targets = instance->number_of_targets + '\x01';
            instance->targets[0].player = ability_player;
            instance->targets[0].card = ability_card;
          }
          instance->info_slot = 2;
        }
      }
      else
        offer_activated = true;
    }
  }
  if (offer_activated &&
      do_dialog(player, player, card, -1, -1, " Counter activated ability\n Pass", 0) == 0)
  {
    if (1 < stack_size)
    {
      ability_player = stack_cards[stack_size - 2].player;
      ability_card = stack_cards[stack_size - 2].card;
      ai_modifier = ai_modifier + 100;
      instance->number_of_targets = instance->number_of_targets + '\x01';
      instance->targets[0].player = ability_player;
      instance->targets[0].card = ability_card;
    }
    instance->info_slot = 1;
  }
  charge_mana_multi(player, cycling_cost & (int)(cycling_cost ^ 0xffffffff) >> 0x1f, 0, 1, 0, 0, 0);
  if (spell_fizzled != 1)
  {
    card_is_being_cycled = 1;
    discard_card(player, card);
    card_is_being_cycled = 0;
    return 2;
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x20cf060; original cards/hour_of_devastation.c:686. */
int card_ominous_sphinx(int player, int card, event_t event)
{
  /* Ominous Sphinx	|3|U|U
   * Creature - Sphinx 4/4
   * Flying
   * Whenever you cycle or discard a card, target creature an opponent controls gets -2/-0 until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  result = is_humiliated(player, card, AL_TRIG);
  if (result == 0)
  {
    result = discard_trigger(player, card, event, player, 2, 0);
    if (result != 0)
    {

      default_target_definition(player, card, &td, 2);
      td.allowed_controller = 1 - player;
      td.allow_cancel = 0;
      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
      result = can_target(&td);
      if (result != 0)
      {
        result = pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS");
        if (result != 0)
        {
          pump_until_eot_merge_previous(player, card, instance->targets[0].player,
                                        instance->targets[0].card, -2, 0);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x20cf160; original cards/hour_of_devastation.c:706. */
int card_proven_combatant(int player, int card, event_t event)
{
  /* Proven Combatant	|U
   * Creature - Human Warrior 1/1
   * Eternalize |4|U|U */

  int result;

  result = eternalize(player, card, event, CARD_ID_PROVEN_COMBATANT, 4, 0, 2, 0, 0, 0);
  return result;
}

/* Riddleform	|1|U
 * Enchantment
 * Whenever you cast a noncreature spell, you may have ~ become a 3/3 Sphinx creature with flying in addition to its other types until end of turn.
 * |2|U: Scry 1. */

/* Seer of the Last Tomorrow	|2|U
 * Creature - Naga Cleric 1/4
 * |U, |T, Discard a card: Target player puts the top three cards of their library into their graveyard. */

/* Sinuous Striker	|2|U
 * Creature - Naga Warrior 2/2
 * |U: ~ gets +1/-1 until end of turn.
 * Eternalize-|3|U|U, Discard a card. */

/* ManalinkEh 2022: 0x20cf1c0; original cards/hour_of_devastation.c:728. */
int card_spellweaver_eternal(int player, int card, event_t event)
{
  /* Spellweaver Eternal	|1|U
   * Creature - Zombie Naga Wizard 2/1
   * Prowess
   * Afflict 2 */

  int result;

  prowess(player, card, event);
  result = afflict(player, card, event, 2);
  return result;
}

/* ManalinkEh 2022: 0x20cf200; original cards/hour_of_devastation.c:740. */
int card_striped_riverwinder(int player, int card, event_t event)
{
  /* Striped Riverwinder	|6|U
   * Creature - Serpent 5/5
   * Hexproof
   * Cycling |U */

  int result;
  hexproof(player, card, event);
  result = cycling(player, card, event, 0, 0, 1, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x20cf270; original cards/hour_of_devastation.c:750. */
int card_supreme_will(int player, int card, event_t event)
{
  /* Supreme Will	|2|U
   * Instant
   * Choose one -
   * * Counter target spell unless its controller pays |3.
   * * Look at the top four cards of your library. Put one of them into your hand and the rest on the bottom of your library in any order. */

  enum
  {
    CHOICE_COUNTER = 1,
    CHOICE_ANTICIPATE = 2
  };

  card_instance_t *instance;
  int result;

  if (event == EVENT_CAN_CAST)
  {
    get_card_instance(player, card);
    result = generic_spell(player, card, EVENT_CAN_CAST, 2, NULL, NULL, 1, NULL);
    if (result != 0)
    {
      return 99;
    }
    event = EVENT_CAN_CAST;
    return basic_spell(player, card, event);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (instance = get_card_instance(player, card), card == affected_card)) &&
        (player == affected_card_controller))
    {
      instance->number_of_targets = '\0';
      result = check_special_flags2(player, card, -0x80000000);
      if (result == 0)
      {

        result = generic_spell(player, card, EVENT_CAN_CAST, 2, NULL, NULL, 1, NULL);
        result =
            dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Counter a spell",
                        result, 0x19, "Reveal top four, choose one", 1, 5, 0, 0, 0, 0);
        if (result == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
      }
      if (instance->info_slot == CHOICE_COUNTER)
      {
        return generic_spell(player, card, EVENT_CAST_SPELL, 2, NULL, NULL, 1, NULL);
      }
      if (instance->info_slot == CHOICE_ANTICIPATE)
      {
        event = EVENT_CAST_SPELL;
        ai_modifier = ai_modifier + 0x18;
        return basic_spell(player, card, event);
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = instance->info_slot;
      if (result == CHOICE_COUNTER)
      {
        counterspell_resolve_unless_pay_x(player, card, NULL, 0, 3);
        result = instance->info_slot;
      }
      if (result == CHOICE_ANTICIPATE)
      {
        reveal_top_cards_of_library_and_choose_type(player, card, player, 4, 1, TUTOR_HAND, 0,
                                                    TUTOR_BOTTOM_OF_DECK, 0, TYPE_ANY);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x20cf580; original cards/hour_of_devastation.c:804. */
int card_swarm_intelligence(int player, int card, event_t event)
{
  /* Swarm Intelligence	|6|U
   * Enchantment
   * Whenever you cast an instant or sorcery spell, you may copy that spell. You may choose new targets for the copy. */

  int result;

  result = duh_mode(player);
  sp_ability_trigger(player, event, (result != 0) + 1, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x20cf610; original cards/hour_of_devastation.c:817. */
int card_tragic_lesson(int player, int card, event_t event)
{
  /* Tragic Lesson	|2|U
   * Instant
   * Draw two cards. Then discard a card unless you return a land you control to its owner's hand. */

  int result;
  card_instance_t *instance;
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
        default_target_definition(player, card, &td, 1);
        td.allowed_controller = player;
        td.allow_cancel = 0;
        td.illegal_abilities = 0;
        if (event == EVENT_RESOLVE_SPELL)
        {

          draw_cards(player, 2);
          result = can_target(&td);
          result =
              dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_RANDOM, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1,
                          "Discard a card", 1, 5, "Bounce a land", result, 5, 0, 0, 0, 0);
          if (result == 1)
          {
            discard(player, 0, player);
          }
          else
          {

            instance = get_card_instance(player, card);
            if ((spell_fizzled != 1) &&
                (result = new_pick_target(
                     &td, "Select a land you control to return to its owner\'s hand.", 0, 0x81),
                 result != 0))
            {
              bounce_permanent(player, instance->targets[0].card);
            }
          }
          kill_card(player, card, KILL_DESTROY);
        }
        return basic_spell(player, card, event);
      }
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
    }
  }
  default_target_definition(player, card, &td, 1);
  td.allow_cancel = 0;
  td.illegal_abilities = 0;
  td.allowed_controller = player;
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x20cf810; original cards/hour_of_devastation.c:858. */
int card_unesh_criosphinx_sovereign(int player, int card, event_t event)
{
  /* Unesh, Criosphinx Sovereign	|4|U|U
   * Legendary Creature - Sphinx 4/4
   * Flying
   * Sphinx spells you cast cost |2 less to cast.
   * Whenever ~ or another Sphinx enters the battlefield under your control, reveal the top four cards of your library. An opponent separates those cards into two piles. Put one pile into your hand and the other into your graveyard. */

  if (event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player &&
      !is_humiliated(player, card, AL_STATIC) &&
      has_subtype(affected_card_controller, affected_card, SUBTYPE_SPHINX))
    COST_COLORLESS -= 2;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (!is_humiliated(player, card, AL_TRIG) && trigger_condition == TRIGGER_COMES_INTO_PLAY && card == affected_card &&
      player == affected_card_controller)
  {
    test_definition_t test;
    default_test_definition(&test, 0x47);
    test.subtype = 0xb9;
    test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
    effect_fof(player, player, 4, 3);
  return 0;
}

/* Unquenchable Thirst	|1|U
 * Enchantment - Aura
 * Enchant creature
 * When ~ enters the battlefield, if you control a Desert or there is a Desert card in your graveyard, tap enchanted creature.
 * Enchanted creature doesn't untap during its controller's untap step. */

/* Vizier of the Anointed	|3|U
 * Creature - Human Cleric 2/4
 * When ~ enters the battlefield, you may search your library for a creature card with eternalize or embalm, put that card into your graveyard, then shuffle your library.
 * Whenever you activate an eternalize or embalm ability, draw a card. */

/* ManalinkEh 2022: 0x20cf9c0; original cards/hour_of_devastation.c:900. */
int card_accursed_horde(int player, int card, event_t event)
{
  /* Accursed Horde	|3|B
   * Creature - Zombie 3/3
   * |1|B: Target attacking Zombie gains indestructible until end of turn. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.required_subtype = 0xde;
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
    td.required_subtype = 0xde;
    td.required_state = TARGET_STATE_ATTACKING;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                             instance->targets[0].player, instance->targets[0].card, 0, 0, 0,
                             0x800);
    }
  }
  result = generic_activated_ability(player, card, event, 6, 1, 1, 0, 0, 0, 0, 0, &td,
                                     "Select target attacking Zombie");
  return result;
}

/* Ammit Eternal	|2|B
 * Creature - Zombie Crocodile Demon 5/5
 * Afflict 3
 * Whenever an opponent casts a spell, put a -1/-1 counter on ~.
 * Whenever ~ deals combat damage to a player, remove all -1/-1 counters from it. */

/* Apocalypse Demon	|4|B|B
 * Creature - Demon 100/100
 * Flying
 * ~'s power and toughness are each equal to the number of cards in your graveyard.
 * At the beginning of your upkeep, tap ~ unless you sacrifice another creature. */

/* Banewhip Punisher	|2|B
 * Creature - Human Warrior 2/2
 * When ~ enters the battlefield, you may put a -1/-1 counter on target creature.
 * |B, Sacrifice ~: Destroy target creature that has a -1/-1 counter on it. */

/* ManalinkEh 2022: 0x20cfb50; original cards/hour_of_devastation.c:938. */
int card_bontus_last_reckoning(int player, int card, event_t event)
{
  /* Bontu's Last Reckoning	|1|B|B
   * Sorcery
   * Destroy all creatures. Lands you control don't untap during your next untap step. */

  int result;
  card_instance_t *instance;
  test_definition_t test;
  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = create_legacy_effect(player, card, hou_finality_effect);
    instance = get_card_instance(player, result);
    instance->targets[1].card = 1;
    default_test_definition(&test, 2);
    new_manipulate_all(player, card, 2, &test, ACT_KILL_DESTROY);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* Carrion Screecher	|3|B	0x401000
 * Creature - Zombie Bird 3/1
 * Flying */

/* ManalinkEh 2022: 0x20cfc50; original cards/hour_of_devastation.c:962. */
int card_doomfall(int player, int card, event_t event)
{
  /* Doomfall	|2|B
   * Sorcery
   * Choose one -
   * * Target opponent exiles a creature they control.
   * * Target opponent reveals their hand. You choose a nonland card from it. Exile that card. */

  enum
  {
    CHOICE_CREATURE = 1,
    CHOICE_CARD = 2
  };

  card_instance_t *instance;
  int creature_mode_available;
  int hand_mode_or_opponent;
  ec_definition_t this_definition;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 0x40, &td, NULL, 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.zone = TARGET_ZONE_PLAYERS;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        if (check_special_flags2(player, card, -0x80000000) == 0)
        {

          hand_mode_or_opponent = opponent_is_valid_target(player, card);
          creature_mode_available = opponent_is_valid_target(player, card);
          if (dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM,
                          "Opponent exiles a creature they control", creature_mode_available, 5,
                          "Choose and exile a card from opponent's hand", hand_mode_or_opponent, 5,
                          0, 0, 0, 0) == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
        }
        return generic_spell(player, card, EVENT_CAST_SPELL, 0x40, &td, NULL, 1, NULL);
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      if (instance->info_slot == CHOICE_CREATURE)
      {
        if (opponent_is_valid_target(player, card) != 0)
        {
          target_definition_t td1;
          hand_mode_or_opponent = 1 - player;
          base_target_definition(player, card, &td1, 2);
          td1.allow_cancel = 0;
          td1.who_chooses = hand_mode_or_opponent;
          td1.allowed_controller = hand_mode_or_opponent;
          td1.preferred_controller = hand_mode_or_opponent;
          new_pick_target(&td1, "Select a creature you control to exile.", 0, 0x80);
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
        }
      }
      else if ((instance->info_slot == CHOICE_CARD) &&
               (hand_mode_or_opponent = opponent_is_valid_target(player, card),
                hand_mode_or_opponent != 0))
      {
        test_definition_t this_test;
        default_test_definition(&this_test, 0xff);
        default_ec_definition(instance->targets[0].player, player, &this_definition);
        this_definition.effect = 2;
        this_test.type = TYPE_LAND;
        this_test.type_flag = DOESNT_MATCH;
        new_effect_coercion(&this_definition, &this_test);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x20d0040; original cards/hour_of_devastation.c:1030. */
int card_dreamstealer(int player, int card, event_t event)
{
  /* Dreamstealer	|2|B
   * Creature - Human Wizard 1/2
   * Menace
   * Whenever ~ deals combat damage to a player, that player discards that many cards.
   * Eternalize |4|B|B */

  unsigned int packed_damage;
  int result;
  card_instance_t *instance;
  unsigned int packed_counts;
  int times_damaged[2];
  int damages[2];

  menace(player, card, event);
  result = damage_dealt_by_me(player, card, event,
                              DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                                  DDBM_REPORT_DAMAGE_DEALT | DDBM_MUST_BE_COMBAT_DAMAGE);
  if (result != 0)
  {

    instance = get_card_instance(player, card);
    packed_counts = instance->targets[1].player;
    instance->targets[1].player = 0;
    times_damaged[1] = packed_counts >> 8 & 0xff;
    packed_damage = instance->targets[0x10].player;
    times_damaged[0] = packed_counts & 0xff;
    instance->targets[0x10].player = 0;

    damages[0] = packed_damage & 0xff;
    damages[1] = packed_damage >> 8 & 0xff;
    if (times_damaged[current_turn] != 0)
    {
      new_multidiscard(current_turn, damages[current_turn], 0, player);
    }
    int p = current_turn ^ 1;
    if (times_damaged[p] != 0)
    {
      new_multidiscard(p, damages[p], 0, player);
    }
  }
  return eternalize(player, card, event, CARD_ID_DREAMSTEALER, 4, 2, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x20d0180; original cards/hour_of_devastation.c:1057. */
int card_grisly_survivor(int player, int card, event_t event)
{
  /* Grisly Survivor	|2|B
   * Creature - Minotaur Warrior 2/3
   * Whenever you cycle or discard a card, ~ gets +2/+0 until end of turn. */

  if (!is_humiliated(player, card, AL_TRIG))
  {
    if (discard_trigger(player, card, event, player, 2, 0))
    {
      pump_until_eot_merge_previous(player, card, player, card, 2, 0);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x20d0210; original cards/hour_of_devastation.c:1068. */
int card_hour_of_glory(int player, int card, event_t event)
{
  /* Hour of Glory	|3|B
   * Instant
   * Exile target creature. If that creature was a God, its controller reveals their hand and exiles all cards from it with the same name as that creature. */

  card_instance_t *instance;
  int target_card_or_name;
  int nontoken;
  int target_player;
  ec_definition_t this_definition;
  target_definition_t td;
  test_definition_t this_test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td) != 0)
    {
      nontoken = has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_GOD);
      target_card_or_name = instance->targets[0].card;
      target_player = instance->targets[0].player;
      if ((nontoken != 0) && (0 < hand_count[target_player]))
      {
        target_card_or_name = get_card_name(target_player, target_card_or_name);
        if (-1 < target_card_or_name)
        {
          default_test_definition(&this_test, 0xff);
          target_player = instance->targets[0].player;
          this_test.id = target_card_or_name;
          default_ec_definition(target_player, target_player, &this_definition);
          this_definition.effect = 0x82;
          new_effect_coercion(&this_definition, &this_test);
        }
        target_player = instance->targets[0].player;
        target_card_or_name = instance->targets[0].card;
      }
      kill_card(target_player, target_card_or_name, KILL_EXILE);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x20d0450; original cards/hour_of_devastation.c:1104. */
int card_khenra_eternal(int player, int card, event_t event)
{
  /* Khenra Eternal	|1|B
   * Creature - Zombie Jackal Warrior 2/2
   * Afflict 1 */

  int result;

  result = afflict(player, card, event, 1);
  return result;
}

/* Lethal Sting	|2|B
 * Sorcery
 * As an additional cost to cast this spell, put a -1/-1 counter on a creature you control.
 * Destroy target creature. */

/* Liliana's Defeat	|B
 * Sorcery
 * Destroy target |Sblack creature or |Sblack planeswalker. If that permanent was a Liliana planeswalker, her controller loses 3 life. */

/* ManalinkEh 2022: 0x20d0470; original cards/hour_of_devastation.c:1121. */
int card_lurching_rotbeast(int player, int card, event_t event)
{
  /* Lurching Rotbeast	|3|B
   * Creature - Zombie Beast 4/2
   * Cycling |B */

  int result;
  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    return 0;
  }
  result = cycling(player, card, event, 0, 1, 0, 0, 0, 0);
  return result;
}

/* Marauding Boneslasher	|2|B
 * Creature - Zombie Minotaur 3/3
 * ~ can't block unless you control another Zombie. */

/* Merciless Eternal	|2|B
 * Creature - Zombie Cleric 2/2
 * Afflict 2
 * |2|B, Discard a card: ~ gets +2/+2 until end of turn. */

/* Moaning Wall	|2|B
 * Creature - Zombie Wall 0/5
 * Defender
 * Cycling |2 */

/* ManalinkEh 2022: 0x20d04d0; original cards/hour_of_devastation.c:1146. */
int card_razaketh_the_foulblooded(int player, int card, event_t event)
{
  /* Razaketh, the Foulblooded	|5|B|B|B
   * Legendary Creature - Demon 8/8
   * Flying, trample
   * Pay 2 life, Sacrifice another creature: Search your library for a card and put that card into your hand. Then shuffle your library. */

  int result;
  test_definition_t test;
  check_legend_rule(player, card, event);
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    new_default_test_definition(&test, 0xff, "Select a card.");
    new_global_tutor(player, player, 1, 1, 0, 1, &test);
  }
  result = generic_activated_ability(player, card, event, 0x40200, 0, 0, 0, 0, 0, 0, 2, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x20d05e0; original cards/hour_of_devastation.c:1166. */
int card_razakeths_rite(int player, int card, event_t event)
{
  /* Razaketh's Rite	|3|B|B
   * Sorcery
   * Search your library for a card and put that card into your hand. Then shuffle your library.
   * Cycling |B */

  if (IS_GS_EVENT(player, card, event))
    return card_demonic_tutor(player, card, event);
  return cycling(player, card, event, MANACOST_B(1));
}

/* Ruin Rat	|1|B
 * Creature - Rat 1/1
 * Deathtouch
 * When ~ dies, exile target card from an opponent's graveyard. */

/* Scrounger of Souls	|4|B	=>m10.c:card_child_of_night 0x200415c
 * Creature - Horror 3/4
 * Lifelink */

/* ManalinkEh 2022: 0x20d0680; original cards/hour_of_devastation.c:1186. */
int card_torment_of_hailfire(int player, int card, event_t event)
{
  /* Torment of Hailfire	|X|B|B
   * Sorcery
   * Repeat the following process X times. Each opponent loses 3 life unless that player sacrifices a nonland permanent or discards a card. */

  int choice;
  card_instance_t *instance;
  int sacrifice_available;
  int hand_size;
  int opponent;
  int i;

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
    else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {

      i = 0;
      opponent = 1 - player;
      if (event == EVENT_RESOLVE_SPELL)
      {
        for (; instance = get_card_instance(player, card), i < instance->info_slot; i = i + 1)
        {
          hand_size = hand_count[opponent];
          sacrifice_available = can_sacrifice(player, opponent, 1, TYPE_ANY, SUBTYPE_INVALID);
          choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_WHO_CHOOSES(opponent), DLG_RANDOM,
                               DLG_NO_CANCEL, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1, "Lose 3 life", 1,
                               (unsigned int)(6 < life[opponent]) * 4 + 1,
                               "Sacrifice a nonland permanent", sacrifice_available, 4,
                               "Discard a card", (unsigned int)(0 < hand_size), 6, 0, 0, 0, 0);
          if (choice == 2)
            impose_sacrifice(player, card, opponent, 1, 0x47, 0, 0, 0, 0, 0, 0, 0, -1, 0);
          if (choice == 3)
            discard(opponent, 0, player);
          else
            // Native sacrifice mode also loses 3 life (0x020d08fb -> 0x020d06d0).
            lose_life(opponent, 3);
        }
        kill_card(player, card, KILL_DESTROY);
      }
    }
    else if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
}

/* Torment of Scarabs	|3|B
 * Enchantment - Aura Curse
 * Enchant player
 * At the beginning of enchanted player's upkeep, that player loses 3 life unless they sacrifice a nonland permanent or discard a card. */

/* Torment of Venom	|2|B|B
 * Instant
 * Put three -1/-1 counters on target creature. Its controller loses 3 life unless they sacrifice another nonland permanent or discards a card. */

/* Vile Manifestation	|1|B
 * Creature - Horror 0/4
 * ~ gets +1/+0 for each card with cycling in your graveyard.
 * Cycling |2 */

/* Without Weakness	|1|B
 * Instant
 * Target creature you control gains indestructible until end of turn.
 * Cycling |2 */

/* Wretched Camel	|1|B
 * Creature - Zombie Camel 2/1
 * When ~ dies, if you control a Desert or there is a Desert card in your graveyard, target player discards a card. */

/* ManalinkEh 2022: 0x20d0910; original cards/hour_of_devastation.c:1235. */
int card_abrade(int player, int card, event_t event)
{
  /* Abrade	|1|R
   * Instant
   * Choose one -
   * * ~ deals 3 damage to target creature.
   * * Destroy target artifact. */

  enum
  {
    CHOICE_DAMAGE = 1,
    CHOICE_ARTIFACT = 2
  };

  int choice;
  card_instance_t *instance;
  int artifact_available_or_valid;
  target_definition_t *td;
  const char *prompt;
  target_definition_t td1;
  target_definition_t td2;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td1, 2);
    default_target_definition(player, card, &td2, 0x40);
    get_card_instance(player, card);
    if (generic_spell(player, card, EVENT_CAN_CAST, 1, &td1, "TARGET_CREATURE", 1, NULL) == 0)
    {
      return generic_spell(player, card, EVENT_CAN_CAST, 1, &td2, "TARGET_ARTIFACT", 1, NULL);
    }
    return 1;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      td = &td2;
      default_target_definition(player, card, &td1, 2);
      default_target_definition(player, card, td, 0x40);
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        if (check_special_flags2(player, card, -0x80000000) == 0)
        {

          artifact_available_or_valid = can_target(td);
          choice = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, "Damage target creature",
                               can_target(&td1), 5, "Destroy target artifact",
                               artifact_available_or_valid, 5, 0, 0, 0, 0);
          if (choice == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          if (choice == CHOICE_DAMAGE)
          {
            td = &td1;
            prompt = "TARGET_CREATURE";
          }
          else
          {
            if (choice != CHOICE_ARTIFACT)
            {
              return 0;
            }
            prompt = "TARGET_ARTIFACT";
          }
          return generic_spell(player, card, EVENT_CAST_SPELL, 1, td, prompt, 1, NULL);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td1, 2);
    default_target_definition(player, card, &td2, 0x40);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      if (instance->info_slot == CHOICE_DAMAGE)
      {
        if (valid_target(&td1) != 0)
        {
          damage_target0(player, card, 5);
        }
      }
      else if ((instance->info_slot == CHOICE_ARTIFACT) &&
               (artifact_available_or_valid = valid_target(&td2), artifact_available_or_valid != 0))
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td1, 2);
    default_target_definition(player, card, &td2, 0x40);
    get_card_instance(player, card);
  }
  return 0;
}

/* Blur of Blades	|1|R
 * Instant
 * Put a -1/-1 counter on target creature. ~ deals 2 damage to that creature's controller. */

/* Burning-Fist Minotaur	|1|R
 * Creature - Minotaur Wizard 2/1
 * First strike
 * |1|R, Discard a card: ~ gets +2/+0 until end of turn. */

/* Chandra's Defeat	|R
 * Instant
 * ~ deals 5 damage to target |Sred creature or |Sred planeswalker. If that permanent is a Chandra planeswalker, you may discard a card. If you do, draw a card. */

/* Chaos Maw	|5|R|R
 * Creature - Hellion 6/6
 * When ~ enters the battlefield, it deals 3 damage to each other creature. */

/* ManalinkEh 2022: 0x20d0d20; original cards/hour_of_devastation.c:1311. */
int card_crash_through(int player, int card, event_t event)
{
  /* Crash Through	|R
   * Sorcery
   * Creatures you control gain trample until end of turn.
   * Draw a card. */

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
      {
        return 0;
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {

      for (int creature = 0; creature < active_cards_count[player]; ++creature)
        if (in_play(player, creature) && is_what(player, creature, 2))
          pump_ability_until_eot(player, card, player, creature, 0, 0, 0x80, 0);
      draw_a_card(player);
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return basic_spell(player, card, event);
}

/* Defiant Khenra	|1|R	0x401000
 * Creature - Jackal Warrior 2/2 */

/* Earthshaker Khenra	|1|R
 * Creature - Jackal Warrior 2/1
 * Haste
 * When ~ enters the battlefield, target creature with power less than or equal to ~'s power can't block this turn.
 * Eternalize |4|R|R */

/* Fervent Paincaster	|2|R
 * Creature - Human Wizard 3/1
 * |T: ~ deals 1 damage to target player or planeswalker.
 * |T, Exert ~: It deals 1 damage to target creature. */

/* Firebrand Archer	|1|R
 * Creature - Human Archer 2/1
 * Whenever you cast a noncreature spell, ~ deals 1 damage to each opponent. */

/* Frontline Devastator	|3|R
 * Creature - Zombie Minotaur Warrior 3/3
 * Afflict 2
 * |1|R: ~ gets +1/+0 until end of turn. */

/* Gilded Cerodon	|4|R
 * Creature - Beast 4/4
 * Whenever ~ attacks, if you control a Desert or there is a Desert card in your graveyard, target creature can't block this turn. */

/* ManalinkEh 2022: 0x20d0e60; original cards/hour_of_devastation.c:1358. */
int card_granitic_titan(int player, int card, event_t event)
{
  /* Granitic Titan	|4|R|R
   * Creature - Elemental 5/4
   * Menace
   * Cycling |2 */

  int result;
  menace(player, card, event);
  result = cycling(player, card, event, 2, 0, 0, 0, 0, 0);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20d0ed0; original cards/hour_of_devastation.c:1369. */
int card_hazorets_undying_fury(int player, int card, event_t event)
{
  /* Hazoret's Undying Fury	|4|R|R
   * Sorcery
   * Shuffle your library, then exile the top four cards. You may cast any number of nonland cards with converted mana cost 5 or less from among them without paying their mana costs. Lands you control don't untap during your next untap step. */

  if (event == EVENT_RESOLVE_SPELL)
  {
    shuffle(player);
    if (deck_ptr[player][0] != -1)
    {
      int amount = count_deck(player);
      if (amount > 0)
      {
        if (amount > 4)
          amount = 4;
        int *deck = deck_ptr[player];
        show_deck(0, deck, amount, "Cards exiled by Hazoret's Undying Fury", 0,
                       gs_done_008b40e0);
        int haz[amount];
        int haz_count = 0;
        for (int i = amount - 1; i != -1; --i)
        {
          if (!is_what(-1, deck[i], TYPE_LAND) &&
              get_cmc_by_id((uint16_t)cards_data[deck[i]].id) < 6 &&
              can_legally_play_iid(player, deck[i]))
          {
            haz[haz_count++] = deck[i];
          }
          rfg_card_in_deck(player, i);
        }
        while (haz_count > 0)
        {
          test_definition_t this_test;
          new_default_test_definition(&this_test, TYPE_ANY, "Select a card to play for free");
          int selected =
              select_card_from_zone(player, player, haz, haz_count, 0, 1, -1, &this_test);
          if (selected == -1)
            break;
          unsigned int csvid = (uint16_t)cards_data[haz[selected]].id;
          for (int k = selected; k < haz_count; ++k)
            haz[k] = haz[k + 1];
          --haz_count;
          play_card_in_exile_for_free(player, 1 - player, csvid);
        }
      }
    }
    int legacy = create_legacy_effect(player, card, hou_finality_effect);
    get_card_instance(player, legacy)->targets[1].card = 1;
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}
#endif

/* ManalinkEh 2022: 0x20d1130; original cards/hour_of_devastation.c:1422. */
int card_hour_of_devastation(int player, int card, event_t event)
{
  /* Hour of Devastation	|3|R|R
   * Sorcery
   * All creatures lose indestructible until end of turn. ~ deals 5 damage to each creature and each non-Bolas planeswalker. */

  if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    // The second player's identity is read after the first scan's callbacks.
    for (int pass = 0; pass < 2; ++pass)
    {
      int scan_player = pass == 0 ? current_turn : 1 - current_turn;
      for (int target_card = active_cards_count[scan_player] - 1; target_card >= 0; --target_card)
      {
        if (is_what(scan_player, target_card, 2))
        {
          negate_ability_and_special_ability_until_eot(player, card, scan_player, target_card,
                                                       KEYWORD_0, 0x800);
          damage_creature(scan_player, target_card, 5, player, card);
        }
        if (is_what(scan_player, target_card, 0x100) &&
            !has_subtype(scan_player, target_card, SUBTYPE_BOLAS))
          damage_creature(scan_player, target_card, 5, player, card);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x20d1310; original cards/hour_of_devastation.c:1448. */
int card_imminent_doom(int player, int card, event_t event)
{
  /* Imminent Doom	|2|R
   * Enchantment
   * ~ enters the battlefield with a doom counter on it.
   * Whenever you cast a spell with converted mana cost equal to the number of doom counters on ~, ~ deals that much damage to any target. Then put a doom counter on ~. */

  int result;
  card_instance_t *instance;


  enters_the_battlefield_with_counters(player, card, event, COUNTER_DOOM, 1);
  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    test_definition_t test;
    default_test_definition(&test, 0xff);
    test.cmc = count_counters(player, card, COUNTER_DOOM);
    sp_ability_trigger(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_ANY");
      if (result != 0)
      {
        result = count_counters(player, card, COUNTER_DOOM);
        damage_target0(player, card, result);
        instance = get_card_instance(player, card);
        instance->number_of_targets = '\0';
        add_counters(player, card, player, card, COUNTER_DOOM, 1);
      }
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x20d14b0; original cards/hour_of_devastation.c:1479. */
int card_inferno_jet(int player, int card, event_t event)
{
  /* Inferno Jet	|5|R
   * Sorcery
   * ~ deals 6 damage to target opponent or planeswalker.
   * Cycling |2 */

  if ((uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND) < 3)
  {
    return cycling(player, card, event, 2, 0, 0, 0, 0, 0);
  }
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  target_definition_t td;
  opponent_or_pw_target_definition(player, card, &td);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      damage_target0(player, card, 6);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_OPPONENT_OR_PLANESWALKER", 1, NULL);
}

/* Khenra Scrapper	|2|R
 * Creature - Jackal Warrior 2/3
 * Menace
 * You may exert ~ as it attacks. When you do, it gets +2/+0 until end of turn. */

/* Magmaroth	|3|R
 * Creature - Elemental 5/5
 * At the beginning of your upkeep, put a -1/-1 counter on ~.
 * Whenever you cast a noncreature spell, remove a -1/-1 counter from ~. */

/* Manticore Eternal	|3|R|R
 * Creature - Zombie Manticore 5/4
 * Afflict 3
 * ~ attacks each combat if able. */

/* ManalinkEh 2022: 0x20d1620; original cards/hour_of_devastation.c:1523. */
int card_neheb_the_eternal(int player, int card, event_t event)
{
  /* Neheb, the Eternal	|3|R|R
   * Legendary Creature - Zombie Minotaur Warrior 4/6
   * Afflict 3
   * At the beginning of your postcombat main phase, add |R for each 1 life your opponents have lost this turn. */

  int result;

  check_legend_rule(player, card, event);
  result = beginning_of_phase(player, card, event, player, card, 0x1e);
  if (result != 0)
  {
    result = get_trap_condition(1 - player, 2);
    produce_mana(player, 4, result);
  }
  result = afflict(player, card, event, 3);
  return result;
}

/* Open Fire	|2|R	=>unlimited.c:card_lightning_bolt 0x200c078
 * Instant
 * ~ deals 3 damage to any target. */

/* Puncturing Blow	|2|R|R
 * Sorcery
 * ~ deals 5 damage to target creature. If that creature would die this turn, exile it instead. */

/* Sand Strangler	|3|R
 * Creature - Beast 3/3
 * When ~ enters the battlefield, if you control a Desert or there is a Desert card in your graveyard, you may have ~ deal 3 damage to target creature. */

/* Thorned Moloch	|2|R
 * Creature - Lizard 2/2
 * Prowess
 * ~ has first strike as long as it's attacking. */

/* ManalinkEh 2022: 0x20d16b0; original cards/hour_of_devastation.c:1555. */
int card_wildfire_eternal(int player, int card, event_t event)
{
  /* Wildfire Eternal	|3|R
   * Creature - Zombie Jackal Cleric 1/4
   * Afflict 4
   * Whenever ~ attacks and isn't blocked, you may cast an instant or sorcery card from your hand without paying its mana cost. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event == EVENT_DECLARE_BLOCKERS)
  {
    result = is_attacking(player, card);
    if (result != 0)
    {
      result = is_humiliated(player, card, AL_TRIG);
      if (result == 0)
      {
        result = is_unblocked(player, card);
        if (result == 0)
        {
          lose_life(1 - current_turn, 4);
        }
        else
        {

          default_target_definition(player, card, &td, 0x18);
          td.allowed_controller = player;
          td.preferred_controller = player;
          td.zone = TARGET_ZONE_HAND;
          td.illegal_abilities = 0;
          instance = get_card_instance(player, card);
          instance->number_of_targets = '\0';
          result = can_target(&td);
          if (result != 0)
          {
            result =
                pick_next_target_noload(&td, "Select an instant or sorcery card from your hand.");
            if (result != 0)
            {
              result = instance->targets[0].card;
              instance->number_of_targets = '\0';
              play_card_in_hand_for_free(instance->targets[0].player, result);
            }
          }
        }
      }
    }
  }
  return 0;
}

/* Ambuscade	|2|G
 * Instant
 * Target creature you control gets +1/+0 until end of turn. It deals damage equal to its power to target creature an opponent controls. */

/* Beneath the Sands	|2|G
 * Sorcery
 * Search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library.
 * Cycling |2 */

/* Bitterbow Sharpshooters	|4|G	=>unlimited.c:card_serra_angel 0x200c122
 * Creature - Jackal Archer 4/4
 * Vigilance, reach */

/* Devotee of Strength	|2|G
 * Creature - Naga Wizard 3/2
 * |4|G: Target creature gets +2/+2 until end of turn. */

/* Dune Diviner	|2|G
 * Creature - Naga Cleric 2/3
 * |1, Tap an untapped Desert you control: You gain 1 life. */

/* Feral Prowler	|1|G	=>scars_of_mirrodin.c:card_darkslick_drake 0x2004201
 * Creature - Cat 1/3
 * When ~ dies, draw a card. */

/* Frilled Sandwalla	|G	=>torment.c:card_basking_rootwalla 0x2002983
 * Creature - Lizard 1/1
 * |1|G: ~ gets +2/+2 until end of turn. Activate this ability only once each turn. */

/* Gift of Strength	|1|G
 * Instant
 * Target creature gets +3/+3 and gains reach until end of turn. */

/* Harrier Naga	|2|G	0x401000
 * Creature - Naga Warrior 3/3 */

/* Hope Tender	|1|G
 * Creature - Human Druid 2/2
 * |1, |T: Untap target land.
 * |1, |T, Exert ~: Untap two target lands. */

/* ManalinkEh 2022: 0x20d17e0; original cards/hour_of_devastation.c:1627. */
int card_hour_of_promise(int player, int card, event_t event)
{
  /* Hour of Promise	|4|G
   * Sorcery
   * Search your library for up to two land cards, put them onto the battlefield tapped, then shuffle your library. Then if you control three or more Deserts, create two 2/2 |Sblack Zombie creature tokens. */

  int result;
  token_generation_t token;
  test_definition_t this_test;
  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&this_test, 1);
    this_test.qty = 2;
    new_global_tutor(player, player, 1, 5, 0, 5, &this_test);
    result = count_subtype(player, 1, 0x5000);
    if (2 < result)
    {
      default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
      token.tou = 2;
      token.pow = 2;
      token.qty = '\x02';
      generate_token(&token);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* Life Goes On	|G
 * Instant
 * You gain 4 life. If a creature died this turn, you gain 8 life instead. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20d1930; original cards/hour_of_devastation.c:1659. */
int card_majestic_myriarch(int player, int card, event_t event)
{
  /* Majestic Myriarch	|4|G
   * Creature - Chimera 100/100
   * ~'s power and toughness are each equal to twice the number of creatures you control.
   * At the beginning of each combat, ~ gains flying until end of turn if you control a creature with flying. The same is true for first strike, double strike, deathtouch, haste, hexproof, indestructible, lifelink, menace, reach, trample, and vigilance. */

  int c;
  int creature_count_local;
  card_instance_t *instance;
  unsigned int abilities;
  unsigned int special_keywords;
  unsigned int keywords;
  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if (((card == affected_card) && (affected_card_controller == player)) && (player != -1))
    {
      creature_count_local = count_subtype(player, 2, -1);
      set_characteristic_pt(creature_count_local * 2);
    }
  }
  else if ((event == EVENT_CHANGE_TYPE) && (current_turn == player))
  {
    if (engine_any_legal_attacker(player) == 0)
    {
      event_flags = event_flags | EA_FORCE_ATTACK;
    }
  }
  if ((-1 < player) && (-1 < card))
  {
    if (beginning_of_phase(player, card, event, 2, -1, 0x15) != 0)
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        if (is_humiliated(player, card, AL_TRIG) == 0)
        {
          keywords = 0;
          if (active_cards_count[player] < 1)
          {
            special_keywords = 0;
          }
          else
          {
            c = 0;
            special_keywords = 0;
            do
            {
              instance = in_play(player, c);
              if (instance != NULL)
              {
                if (is_what(player, c, 2) != 0)
                {
                  abilities = get_abilities(player, c, EVENT_ABILITIES, -1);
                  keywords = keywords |
                             abilities & (KEYWORD_DOUBLE_STRIKE | KEYWORD_REACH |
                                          KEYWORD_FIRST_STRIKE | KEYWORD_TRAMPLE | KEYWORD_FLYING);
                  abilities = get_special_abilities(player, c);
                  special_keywords =
                      special_keywords |
                      abilities & (SP_KEYWORD_HEXPROOF | SP_KEYWORD_MENACE |
                                   SP_KEYWORD_INDESTRUCTIBLE | SP_KEYWORD_DEATHTOUCH |
                                   SP_KEYWORD_VIGILANCE | SP_KEYWORD_HASTE | SP_KEYWORD_LIFELINK);
                }
              }
              c = c + 1;
            } while (c < active_cards_count[player]);
          }
          pump_ability_until_eot(player, card, player, card, 0, 0, keywords, special_keywords);
        }
      }
    }
  }
  return 0;
}
#endif

/* Nissa's Defeat	|2|G
 * Sorcery
 * Destroy target |H2Forest, |Sgreen enchantment, or |Sgreen planeswalker. If that permanent was a Nissa planeswalker, draw a card. */

/* Oasis Ritualist	|3|G
 * Creature - Naga Druid 2/4
 * |T: Add one mana of any color.
 * |T, Exert ~: Add two mana of any one color. */

/* Overcome	|3|G|G
 * Sorcery
 * Creatures you control get +2/+2 and gain trample until end of turn. */

/* Pride Sovereign	|2|G
 * Creature - Cat 2/2
 * ~ gets +1/+1 for each other Cat you control.
 * |W, |T, Exert ~: Create two 1/1 |Swhite Cat creature tokens with lifelink. */

/* Quarry Beetle	|4|G
 * Creature - Insect 4/5
 * When ~ enters the battlefield, you may return target land card from your graveyard to the battlefield. */

/* Rampaging Hippo	|4|G|G
 * Creature - Hippo 5/6
 * Trample
 * Cycling |2 */

/* Ramunap Excavator	|2|G	=>fifth_dawn.c:card_crucible_of_worlds 0x2001628
 * Creature - Naga Cleric 2/3
 * You may play land cards from your graveyard. */

/* Ramunap Hydra	|3|G
 * Creature - Snake Hydra 3/3
 * Vigilance, reach, trample
 * ~ gets +1/+1 as long as you control a Desert.
 * ~ gets +1/+1 as long as there is a Desert card in your graveyard. */

/* Resilient Khenra	|1|G
 * Creature - Jackal Wizard 2/2
 * When ~ enters the battlefield, you may have target creature get +X/+X until end of turn, where X is ~'s power.
 * Eternalize |4|G|G */

/* ManalinkEh 2022: 0x20d1b20; original cards/hour_of_devastation.c:1736. */
int card_rhonass_last_stand(int player, int card, event_t event)
{
  /* Rhonas's Last Stand	|G|G
   * Sorcery
   * Create a 5/4 |Sgreen Snake creature token. Lands you control don't untap during your next untap step. */

  int result;
  card_instance_t *instance;
  token_generation_t token;
  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_token_definition(player, card, CARD_ID_SNAKE, &token);
    token.pow = 5;
    token.tou = 4;
    generate_token(&token);
    result = create_legacy_effect(player, card, hou_finality_effect);
    instance = get_card_instance(player, result);
    instance->targets[1].card = 1;
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* Rhonas's Stalwart	|1|G
 * Creature - Human Warrior 2/2
 * You may exert ~ as it attacks. When you do, it gets +1/+1 until end of turn and can't be blocked by creatures with power 2 or less this turn. */

/* Sidewinder Naga	|2|G
 * Creature - Naga Warrior 3/2
 * As long as you control a Desert or there is a Desert card in your graveyard, ~ gets +1/+0 and has trample. */

/* Sifter Wurm	|5|G|G
 * Creature - Wurm 7/7
 * Trample
 * When ~ enters the battlefield, scry 3, then reveal the top card of your library. You gain life equal to that card's converted mana cost. */

/* Tenacious Hunter	|2|G|G
 * Creature - Crocodile 4/4
 * As long as a creature has a -1/-1 counter on it, ~ has vigilance and deathtouch. */

/* ManalinkEh 2022: 0x20ccf70; original cards/hour_of_devastation.c:1776. */
static int test_not_in_array20(int iid, int arrptr, int player, int card)
{

  if (((((((*(int *)arrptr != iid) && (*(int *)(arrptr + 4) != iid)) &&
          (*(int *)(arrptr + 8) != iid)) &&
         ((*(int *)(arrptr + 0xc) != iid && (*(int *)(arrptr + 0x10) != iid)))) &&
        ((*(int *)(arrptr + 0x14) != iid &&
          ((*(int *)(arrptr + 0x18) != iid && (*(int *)(arrptr + 0x1c) != iid)))))) &&
       (*(int *)(arrptr + 0x20) != iid)) &&
      (((((*(int *)(arrptr + 0x24) != iid && (*(int *)(arrptr + 0x28) != iid)) &&
          (*(int *)(arrptr + 0x2c) != iid)) &&
         (((*(int *)(arrptr + 0x30) != iid && (*(int *)(arrptr + 0x34) != iid)) &&
           ((*(int *)(arrptr + 0x38) != iid &&
             ((*(int *)(arrptr + 0x3c) != iid && (*(int *)(arrptr + 0x40) != iid)))))))) &&
        ((*(int *)(arrptr + 0x44) != iid && (*(int *)(arrptr + 0x48) != iid))))))
  {
    return (unsigned int)(*(int *)(arrptr + 0x4c) != iid);
  }
  return 0;
}

/* ManalinkEh 2022: 0x20d1c10; original cards/hour_of_devastation.c:1782. */
int card_uncage_the_menagerie(int player, int card, event_t event)
{
  /* Uncage the Menagerie	|X|G|G
   * Sorcery
   * Search your library for up to X creature cards with different names that each have converted mana cost X, reveal them, put them into your hand, then shuffle your library. */

  card_instance_t *instance;
  card_instance_t *spell_record;
  int limit_or_selected;
  int *chosen_cursor;
  int next_index;
  int chosen[20];
  char buffer[100];
  test_definition_t test;
  int selected_count;

  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 4, NULL, NULL, 0, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (charge_mana(player, COLOR_COLORLESS, -1), spell_fizzled != 1))
    {
      return generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 0, NULL);
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {

    instance = get_card_instance(player, card);
    if (0 < instance->info_slot)
    {
      chosen_cursor = chosen;

      for (limit_or_selected = 0x14; limit_or_selected != 0;
           limit_or_selected = limit_or_selected + -1)
      {
        *chosen_cursor = -1;
        chosen_cursor = chosen_cursor + 1u;
      }
      limit_or_selected = instance->info_slot;
      if (0 < limit_or_selected)
      {
        selected_count = 0;
        do
        {
          next_index = selected_count + 1;
          scnprintf(buffer, 100,
                    "Select creature cards with different names that each have CMC %d. Card %d of "
                    "up to %d",
                    limit_or_selected, next_index, limit_or_selected);
          new_default_test_definition(&test, 2, buffer);
          test.cmc = instance->info_slot;
          test.value_for_special_selection_function = (int)chosen;
          test.storage = card;
          test.special_selection_function = test_not_in_array20;
          test.no_shuffle = 1;
          test.qty = 1;
          limit_or_selected = new_global_tutor(player, player, 1, 1, 0, 1, &test);
          if (limit_or_selected < 0)
            break;

          spell_record = get_card_instance(player, card);
          chosen[selected_count] = spell_record->targets[0].card;
          remove_card_from_deck(player, limit_or_selected);
          limit_or_selected = instance->info_slot;
          selected_count = next_index;
        } while (next_index < limit_or_selected);
      }
      shuffle(player);
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x20d1e80; original cards/hour_of_devastation.c:1836. */
int card_bloodwater_entity(int player, int card, event_t event)
{
  /* Bloodwater Entity	|1|U|R
   * Creature - Elemental 2/2
   * Flying
   * Prowess
   * When ~ enters the battlefield, you may put target instant or sorcery card from your graveyard on top of your library. */

  int result;
  test_definition_t this_test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  prowess(player, card, event);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&this_test, 0x38, "Select target instant or sorcery card.");
    result = new_special_count_grave(player, &this_test);
    if (result != 0)
    {
      result = graveyard_has_shroud(player);
      if (result == 0)
      {
        new_global_tutor(player, player, 2, 2, 0, 1, &this_test);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x20d1f50; original cards/hour_of_devastation.c:1856. */
int card_the_locust_god(int player, int card, event_t event)
{
  /* The Locust God	|4|U|R
   * Legendary Creature - God 4/4
   * Flying
   * Whenever you draw a card, create a 1/1 |Sblue and |Sred Insect creature token with flying and haste.
   * |2|U|R: Draw a card, then discard a card.
   * When ~ dies, return it to its owner's hand at the beginning of the next end step. */

  int result;
  token_generation_t token;
  check_legend_rule(player, card, event);
  if ((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE))
  {
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      draw_a_card(player);
      discard(player, 0, player);
    }
    result = card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);
    if (result != 0)
    {
      default_token_definition(player, card, CARD_ID_INSECT, &token);
      token.tou = 1;
      token.pow = 1;
      token.color_forced = '\x14';
      token.key_plus = 0x20;
      token.s_key_plus = 4;
      generate_token(&token);
    }
    result = hou_god(player, card, event);
    return result;
  }
  return generic_activated_ability(player, card, event, 0, 2, 0, 1, 0, 1, 0, 0, NULL, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20d20a0; original cards/hour_of_devastation.c:1887. */
int card_nicol_bolas_god_pharaoh(int player, int card, event_t event)
{
  /* Nicol Bolas, God-Pharaoh	|4|U|B|R
   * Legendary Planeswalker - Bolas (7)
   * +2: Target opponent exiles cards from the top of their library until they exile a nonland card. Until end of turn, you may cast that card without paying its mana cost.
   * +1: Each opponent exiles two cards from their hand.
   * -4: ~ deals 7 damage to target opponent, creature an opponent controls, or planeswalker an opponent controls.
   * -12: Exile each nonland permanent your opponents control. */

  enum
  {
    CHOICE_STEAL = 1,
    CHOICE_EXILE_HAND = 2,
    CHOICE_DAMAGE = 3,
    CHOICE_EXILE_ALL = 4
  };

  check_legend_rule(player, card, event);
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION ||
      event == EVENT_CLEANUP)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int opponent = 1 - player;
    target_definition_t td1, td3;
    default_target_definition(player, card, &td1, 0);
    td1.zone = TARGET_ZONE_PLAYERS;
    td1.allowed_controller = opponent;
    td1.preferred_controller = opponent;
    default_target_definition(player, card, &td3, 0x102);
    td3.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td3.allowed_controller = opponent;
    td3.preferred_controller = opponent;
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = 0;
      instance->info_slot = 0;
      int damage_available = can_target(&td3);
      int steal_available = can_target(&td1);
      int choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                               "Steal a spell", steal_available, 10, 2, "Opponents exile from hand",
                               1, 10, 1, "Deal 7 damage", damage_available, 10, -4,
                               "Exile opponents stuff", 1, 100, -12, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_STEAL)
        new_pick_target(&td1, "TARGET_OPPONENT", 0, 1);
      else if (choice == CHOICE_DAMAGE)
        new_pick_target(&td3,
                             "Choose an opponent, creature an opponent controls, or planeswalker "
                             "an opponent controls",
                             -1, 0x80);
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      int choice = instance->info_slot;
      if (choice == CHOICE_STEAL)
      {
        int t_player = instance->targets[0].player;
        int *deck = deck_ptr[t_player];
        int removed[count_deck(t_player)];
        int z = 0;
        while (*deck != -1)
        {
          removed[z] = *deck;
          ++z;
          rfg_top_card_of_deck(t_player);
          if ((cards_data[removed[z - 1]].type & 1) == 0)
          {
            show_deck(current_turn, removed, z, "These cards were revealed", 0,
                           gs_done_008b40e0);
            show_deck(current_turn ^ 1, removed, z, "These cards were revealed", 0,
                           gs_done_008b40e0);
            create_may_play_card_from_exile_effect(player, card, t_player,
                                                        (uint16_t)cards_data[removed[z - 1]].id, 9);
            break;
          }
        }
      }
      else if (choice == CHOICE_EXILE_HAND)
      {
        test_definition_t test;
        new_default_test_definition(&test, 0xff, "Select a card to exile.");
        new_global_tutor(opponent, opponent, 4, 6, 1, 3, &test);
        new_global_tutor(opponent, opponent, 4, 6, 1, 3, &test);
      }
      else if (choice == CHOICE_DAMAGE)
        damage_target0(player, card, 7);
      else if (choice == CHOICE_EXILE_ALL)
      {
        test_definition_t test;
        default_test_definition(&test, 1);
        test.type_flag = 1;
        new_manipulate_all(player, card, opponent, &test, ACT_KILL_REMOVE);
      }
    }
  }
  return planeswalker(player, card, event, 7);
}
#endif

/* Obelisk Spider	|1|B|G
 * Creature - Spider 1/4
 * Reach
 * Whenever ~ deals combat damage to a creature, put a -1/-1 counter on that creature.
 * Whenever you put one or more -1/-1 counters on a creature, each opponent loses 1 life and you gain 1 life. */

/* Resolute Survivors	|1|R|W
 * Creature - Human Warrior 3/3
 * You may exert ~ as it attacks.
 * Whenever you exert a creature, ~ deals 1 damage to each opponent and you gain 1 life. */

/* River Hoopoe	|G|U
 * Creature - Bird 1/3
 * Flying
 * |3|G|U: You gain 2 life and draw a card. */

/* ManalinkEh 2022: 0x20d2620; original cards/hour_of_devastation.c:2006. */
int card_samut_the_tested(int player, int card, event_t event)
{
  /* Samut, the Tested	|2|R|G
   * Legendary Planeswalker - Samut (4)
   * +1: Up to one target creature gains double strike until end of turn.
   * -2: ~ deals 2 damage divided as you choose among one or two targets.
   * -7: Search your library for up to two creature and/or planeswalker cards, put them onto the battlefield, then shuffle your library. */

  enum
  {
    CHOICE_ABILITY = 1,
    CHOICE_DAMAGE = 2,
    CHOICE_TUTOR = 3
  };

  int i;
  int choice;
  card_instance_t *instance;
  int selected_mode;
  int target_card;
  card_instance_t *target_record;
  target_definition_t *creature_target;
  target_definition_t *damage_target;
  target_definition_t td1;
  target_definition_t td2;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td1, 2);
    td1.allow_cancel = 3;
    default_target_definition(player, card, &td2, 0x102);
    td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td2.allow_cancel = 0;
    default_test_definition(&test, 0x102);
    test.qty = 2;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      choice =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Give Double Strike", 1,
                      5, 1, "Deal 2 damage", can_target(&td2), 5, 0xfffffffe,
                      "Tutor Creatures and/or Planeswalkers", 1, 0x14, 0xfffffff9, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_ABILITY)
      {
        if (can_target(&td1) != 0)
        {
          pick_up_to_n_targets(&td1, "TARGET_CREATURE", 1);
        }
      }
      else if (choice == CHOICE_DAMAGE)
      {

        instance = get_card_instance(player, card);
        instance->number_of_targets = '\0';
        bool cancelled = false;
        for (i = 0; i < 2; ++i)
        {
          if (!new_pick_target(&td2, "TARGET_ANY", i, 0))
          {
            cancelled = true;
            break;
          }
          target_card = instance->targets[i].card;
          if (target_card >= 0)
          {
            target_record = get_card_instance(instance->targets[i].player, target_card);
            target_record->state |= 0x200000;
          }
        }
        for (i = 0; i < (int)(unsigned int)instance->number_of_targets; ++i)
        {
          target_card = instance->targets[i].card;
          if (target_card >= 0)
          {
            target_record = get_card_instance(instance->targets[i].player, target_card);
            target_record->state &= 0xffdfffff;
          }
        }
        if (cancelled)
        {
          instance->number_of_targets = 0;
          return 0;
        }
      }
      return planeswalker(player, card, event, 4);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 4);
    default_target_definition(player, card, &td1, 2);
    td1.allow_cancel = 3;
    default_target_definition(player, card, &td2, 0x102);
    td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td2.allow_cancel = 0;
    default_test_definition(&test, 0x102);
    test.qty = 2;
    instance = get_card_instance(player, card);
  }
  damage_target = &td2;
  creature_target = &td1;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    selected_mode = instance->info_slot;
    if (selected_mode == CHOICE_ABILITY)
    {
      if (valid_target(creature_target) != 0)
      {
        pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                               0, 0, 0x40000000, 0);
      }
    }
    else if (selected_mode == CHOICE_DAMAGE)
    {
      if (valid_target(damage_target) != 0)
      {
        instance->number_of_targets = '\x02';
        divide_damage(player, card, NULL);
        instance->number_of_targets = '\0';
      }
    }
    else if (selected_mode == CHOICE_TUTOR)
    {
      new_global_tutor(player, player, 1, 4, 0, 2, &test);
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x20d2b10; original cards/hour_of_devastation.c:2106. */
int card_the_scarab_god(int player, int card, event_t event)
{
  /* The Scarab God	|3|U|B
   * Legendary Creature - God 5/5
   * At the beginning of your upkeep, each opponent loses X life and you scry X, where X is the number of Zombies you control.
   * |2|U|B: Exile target creature card from a graveyard. Create a token that's a copy of it, except it's a 4/4 |Sblack Zombie.
   * When ~ dies, return it to its owner's hand at the beginning of the next end step. */

  card_instance_t *instance;
  int result;
  token_generation_t token;
  test_definition_t test;

  check_legend_rule(player, card, event);
  instance = get_card_instance(player, card);
  new_default_test_definition(&test, 2, "Select a creature card to exile.");
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    result = count_subtype(player, 0x47, 0xde);
    lose_life(1 - player, result);
    scry(player, result);
  }
  else
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability_with_target_in_grave(
          player, card, event, GAA_NONE, 2, 1, 1, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test, 0x1000);
    }
    if (event == EVENT_ACTIVATE)
    {
      if ((card == affected_card) && (player == affected_card_controller))
        return generic_activated_ability_with_target_in_grave(
          player, card, event, GAA_NONE, 2, 1, 1, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test, 0x1000);
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
      if (result == -1)
      {
        spell_fizzled = 1;
        return 0;
      }
      default_token_definition(
          instance->parent_controller, instance->parent_card,
          (unsigned int)cards_data[graveyard_ptr_mutable[instance->targets[0].player][result]].id,
          &token);
      token.action = TOKEN_ACTION_ADD_SUBTYPE;
      token.pow = 4;
      token.tou = 4;
      token.action_argument = 0xde;
      token.color_forced = '\x02';
      generate_token(&token);
      rfg_card_from_grave(instance->targets[0].player, result);
    }
  }
  return hou_god(player, card, event);
}

/* ManalinkEh 2022: 0x20cd010; original cards/hour_of_devastation.c:2152. */
static int the_scorpion_god_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  unsigned int pending_deaths;
  card_instance_t *dying;
  int result;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (result = get_id((int)instance->damage_target_player, instance->damage_target_card),
           result != 0x4357))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (result = effect_follows_control_of_attachment(player, card, 0x4700), result != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (result = effect_follows_control_of_attachment(player, card, event), result != 0))
    {
      return 0;
    }
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {

      instance = get_card_instance(player, card);
      dying = in_play(affected_card_controller, affected_card);
      if ((dying != NULL) &&
          (result = is_this_dying(affected_card_controller, affected_card), result != 0))
      {

        result = affected_card_controller;
        if ((instance->damage_target_card == affected_card) &&
            (affected_card_controller == instance->damage_target_player))
        {
          instance->damage_target_card = -1;
          instance->damage_target_player = -1;
          remove_status(player, card, 0x10000);
          if (instance->targets[0xb].player < 0)
          {
            instance->targets[0xb].player = 0;
          }
          result = affected_card_controller;
          SET_BYTE2(instance->targets[0xb].player) = 1;
        }
        result = count_counters(result, affected_card, COUNTER_M1_M1);
        if (result != 0)
        {
          count_for_gfp_ability(player, card, 0x77, 2, 2, NULL);
        }
      }
    }
  }
  pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_deaths)
  {
    result = check_status(player, card, 0x10000);
    if (result == 0)
    {

      draw_cards(player, pending_deaths & 0xff);
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) && (result = check_status(player, card, 0x10000), result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x20d2d70; original cards/hour_of_devastation.c:2202. */
int card_the_scorpion_god(int player, int card, event_t event)
{
  /* The Scorpion God	|3|B|R
   * Legendary Creature - God 6/5
   * Whenever a creature with a -1/-1 counter on it dies, draw a card.
   * |1|B|R: Put a -1/-1 counter on another target creature.
   * When ~ dies, return it to its owner's hand at the beginning of the next end step. */

  int c;
  card_instance_t *instance;
  unsigned int pending_deaths;
  int created_or_counters;
  target_definition_t td;

  bool legacy_found = false;
  check_legend_rule(player, card, event);
  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 2);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    return generic_activated_ability(player, card, event, 0x40004, 1, 1, 0, 0, 1, 0, 0, &td,
                                     "TARGET_ANOTHER_CREATURE");
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                 COUNTER_M1_M1, 1);
  }
  else if (event == EVENT_STATIC_EFFECTS)
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
                instance->info_slot_fn == the_scorpion_god_effect)) &&
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
        created_or_counters =
            create_targetted_legacy_effect(player, card, the_scorpion_god_effect, player, card);
        instance = get_card_instance(player, created_or_counters);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, created_or_counters, 0x10000);
      }
    }
  }
  else if ((event == EVENT_GRAVEYARD_FROM_PLAY) &&
           (created_or_counters =
                count_counters(affected_card_controller, affected_card, COUNTER_M1_M1),
            created_or_counters != 0))
  {
    count_for_gfp_ability(player, card, 0x77, 2, 2, NULL);
  }
  pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_deaths != 0)
  {

    draw_cards(player, pending_deaths & 0xff);
  }
  return hou_god(player, card, event);
}

/* ManalinkEh 2022: 0x20d3040; original cards/hour_of_devastation.c:2264. */
int card_unraveling_mummy(int player, int card, event_t event)
{
  /* Unraveling Mummy	|1|W|B
   * Creature - Zombie 2/3
   * |1|W: Target attacking Zombie gains lifelink until end of turn.
   * |1|B: Target attacking Zombie gains deathtouch until end of turn. */

  enum
  {
    CHOICE_LIFELINK = 1,
    CHOICE_DEATHTOUCH = 2
  };

  int choice;
  card_instance_t *instance;
  int white_available_or_mode;
  int black_available;
  target_definition_t td;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.required_subtype = 0xde;
    td.preferred_controller = player;
    td.required_state = TARGET_STATE_ATTACKING;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 0, 0, 0, 0, 1, 0, &td,
                                    NULL) != 0)
      {
        return 1;
      }
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 1, 0, 0, 0, 0, 0, &td,
                                    NULL) == 0)
      {
        return 0;
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->info_slot = 0;
      white_available_or_mode =
          generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 0, 0, 0, 0, 1, 0, &td,
                                    "Select target attacking Zombie");
      black_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 1, 0, 0,
                                                  0, 0, 0, &td, "Select target attacking Zombie");
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Lifelink",
                           white_available_or_mode, 5, "Deathtouch", black_available,
                           ((current_phase == 0x18) ? 8U : 2U), 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_LIFELINK)
      {
        black_available = 1;
        white_available_or_mode = 0;
      }
      else
      {
        if (choice != CHOICE_DEATHTOUCH)
        {
          return 0;
        }
        black_available = 0;
        white_available_or_mode = 1;
      }
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 6, 1, white_available_or_mode,
                                       0, 0, 0, black_available, 0, &td,
                                       "Select target attacking Zombie");
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.required_subtype = 0xde;
    td.preferred_controller = player;
    td.required_state = TARGET_STATE_ATTACKING;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    white_available_or_mode = instance->info_slot;
    if (white_available_or_mode == CHOICE_LIFELINK)
    {
      if (valid_target(&td) != 0)
      {
        pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                               instance->targets[0].player, instance->targets[0].card, 0, 0, 0, 2);
      }
      white_available_or_mode = instance->info_slot;
    }
    if ((white_available_or_mode == CHOICE_DEATHTOUCH) &&
        (white_available_or_mode = valid_target(&td), white_available_or_mode != 0))
    {
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                             instance->targets[0].player, instance->targets[0].card, 0, 0, 0,
                             0x200);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x20d34f0; original cards/hour_of_devastation.c:2322. */
int card_farm_market(int player, int card, event_t event)
{
  /* Farm // Market	|2|W
   * Instant // Sorcery
   * Destroy target attacking or blocking creature.
   * //
   * Aftermath
   * Draw two cards, then discard two cards. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  td.required_state = TARGET_STATE_IN_COMBAT;
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
    return result;
  }
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    if ((result != 0) &&
        (result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY), result != 0))
    {
      result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 2, 0, 1, 0, 0, 0, 0);
      return result;
    }
  }
  else
  {
    if (event == EVENT_PLAY_FIRST_HALF)
    {
      result = generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
      return result;
    }
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = get_aftermath(player, card);
      if (result == 0)
      {
        result = validate_target(player, card, &td, 0);
        if (result != 0)
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
      }
      else
      {
        draw_cards(player, 2);
        multidiscard(player, 2, 0);
        kill_card(player, card, KILL_EXILE);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  result = aftermath(player, card, event, 2, 0, 1, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x20d3780; original cards/hour_of_devastation.c:2365. */
int card_consign_oblivion(int player, int card, event_t event)
{
  /* Consign // Oblivion	|1|U
   * Instant // Sorcery
   * Return target nonland permanent to its owner's hand.
   * //
   * Aftermath
   * Target opponent discards two cards. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  target_definition_t td_player;

  default_target_definition(player, card, &td, 0x47);
  td.illegal_type = TYPE_LAND;
  default_target_definition(player, card, &td_player, 0);
  td_player.zone = TARGET_ZONE_PLAYERS;
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    result = generic_spell(player, card, EVENT_CAN_CAST, 0x81, &td,
                           "Select target nonland permanent.", 1, NULL);
    return result;
  }
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY);
    if (result == 0)
    {
      return 0;
    }
    result = generic_spell(player, card, EVENT_CAN_CAST, 0x40, &td_player, NULL, 1, NULL);
    if (result != 0)
    {
      result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 4, 1, 0, 0, 0, 0, 0);
      return result;
    }
  }
  else
  {
    if (event == EVENT_PLAY_FIRST_HALF)
    {
      result = generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                             "Select target nonland permanent.", 1, NULL);
      return result;
    }
    if (event == EVENT_PLAY_SECOND_HALF)
    {
      result = generic_spell(player, card, EVENT_CAST_SPELL, 0x40, &td_player, NULL, 1, NULL);
      return result;
    }
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = get_aftermath(player, card);
      if (result == 0)
      {
        result = validate_target(player, card, &td, 0);
        if (result != 0)
        {
          bounce_permanent(instance->targets[0].player, instance->targets[0].card);
        }
      }
      else
      {
        result = validate_target(player, card, &td_player, 0);
        if (result != 0)
        {
          instance = get_card_instance(player, card);
          new_multidiscard(instance->targets[0].player, 2, 0, player);
        }
        kill_card(player, card, KILL_EXILE);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  result = aftermath(player, card, event, 4, 1, 0, 0, 0, 0, 0);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20d3ad0; original cards/hour_of_devastation.c:2416. */
int card_claim_fame(int player, int card, event_t event)
{
  /* Claim // Fame	|B
   * Sorcery // Sorcery
   * Return target creature card with converted mana cost 2 or less from your graveyard to the battlefield.
   * //
   * Aftermath
   * Target creature gets +2/+0 and gains haste until end of turn. */

  card_instance_t *instance;
  int result;

  target_definition_t td;
  char msg[100];
  test_definition_t this_test;
  memcpy(msg, "Select a creature card with CMC 2 or less.", 43);
  memset(msg + 43, 0, 57);
  new_default_test_definition(&this_test, 2, msg);
  this_test.cmc = 3;
  this_test.cmc_flag = 3;
  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_GRAVEYARD_ABILITY)
    {
      result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
      if ((result != 0) &&
          (result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY), result != 0))
      {
        result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 1, 0, 0, 0, 1, 0, 0);
        return result;
      }
    }
    else
    {
      if (event == EVENT_PLAY_FIRST_HALF)
      {
        result = generic_spell(player, card, EVENT_CAST_SPELL, 8, NULL, NULL, 1, &this_test);
        return result;
      }
      if (event == EVENT_PLAY_SECOND_HALF)
      {
        generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
      }
      else if (event == EVENT_RESOLVE_SPELL)
      {
        result = get_aftermath(player, card);
        if (result == 0)
        {
          result = validate_target_from_grave_source(player, card, player, 0);
          if (result != -1)
          {
            reanimate_permanent(player, card, player, result, REANIMATE_DEFAULT);
          }
        }
        else
        {
          pump_ability_until_eot(player, card, instance->targets[0].player,
                                      instance->targets[0].card, 2, 0, 0, 4);
          kill_card(player, card, KILL_EXILE);
        }
        kill_card(player, card, KILL_DESTROY);
      }
    }
    result = aftermath(player, card, event, 1, 0, 0, 0, 1, 0, 0);
    return result;
  }
  result = generic_spell(player, card, EVENT_CAN_CAST, 8, NULL, NULL, 1, &this_test);
  return result;
}
#endif

/* ManalinkEh 2022: 0x20d3e80; original cards/hour_of_devastation.c:2467. */
int card_struggle_survive(int player, int card, event_t event)
{
  /* Struggle // Survive	|2|R
   * Instant // Sorcery
   * Struggle deals damage to target creature equal to the number of lands you control.
   * //
   * Aftermath
   * Each player shuffles their graveyard into their library. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
    return result;
  }
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    if ((result != 0) &&
        (result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY), result != 0))
    {
      result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 1, 0, 0, 1, 0, 0, 0);
      return result;
    }
  }
  else
  {
    if (event == EVENT_PLAY_FIRST_HALF)
    {
      result = generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
      return result;
    }
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = get_aftermath(player, card);
      if (result == 0)
      {
        result = validate_target(player, card, &td, 0);
        if (result != 0)
        {
          result = count_subtype(player, 1, -1);
          damage_creature(instance->targets[0].player, instance->targets[0].card, result, player,
                          card);
        }
      }
      else
      {
        reshuffle_grave_into_deck(current_turn, 0);
        reshuffle_grave_into_deck(current_turn ^ 1, 0);
        kill_card(player, card, KILL_EXILE);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  result = aftermath(player, card, event, 1, 0, 0, 1, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x20d4120; original cards/hour_of_devastation.c:2508. */
int card_appeal_authority(int player, int card, event_t event)
{
  /* Appeal // Authority	|G
   * Sorcery // Sorcery
   * Until end of turn, target creature gains trample and gets +X/+X, where X is the number of creatures you control.
   * //
   * Aftermath
   * Tap up to two target creatures your opponents control. Creatures you control gain vigilance until end of turn. */

  int i;
  card_instance_t *instance;
  int available_or_toughness;
  int power;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
  }
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    td.allowed_controller = 1 - player;
    available_or_toughness =
        generic_spell(player, card, EVENT_CAN_CAST, 0x20, &td, "TARGET_CREATURE", 2, NULL);
    if ((available_or_toughness != 0) &&
        (available_or_toughness = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY),
         available_or_toughness != 0))
    {
      return aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 1, 0, 0, 0, 0, 1, 0);
    }
  }
  else
  {
    if (event == EVENT_PLAY_FIRST_HALF)
    {
      return generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
    }
    if (event == EVENT_PLAY_SECOND_HALF)
    {
      td.allowed_controller = 1 - player;
      return generic_spell(player, card, EVENT_CAST_SPELL, 0x20, &td, "TARGET_CREATURE", 2, NULL);
    }
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (get_aftermath(player, card) == 0)
      {
        if (validate_target(player, card, &td, 0) != 0)
        {
          available_or_toughness = count_subtype(player, 2, -1);
          power = count_subtype(player, 2, -1);
          pump_ability_until_eot(player, card, instance->targets[0].player,
                                 instance->targets[0].card, power, available_or_toughness, 0x80, 0);
        }
      }
      else
      {
        td.allowed_controller = 1 - player;
        if (instance->number_of_targets != '\0')
        {
          i = 0;
          do
          {
            if (validate_target(player, card, &td, i) != 0)
            {
              tap_card(instance->targets[i].player, instance->targets[i].card);
            }
            i = i + 1;
          } while (i < (int)(unsigned int)instance->number_of_targets);
        }
        pump_subtype_until_eot(player, card, player, -1, 1, 1, 0, 8);
        kill_card(player, card, KILL_EXILE);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  return aftermath(player, card, event, 1, 0, 0, 0, 0, 1, 0);
}

/* ManalinkEh 2022: 0x20d44f0; original cards/hour_of_devastation.c:2562. */
int card_leave_chance(int player, int card, event_t event)
{
  /* Leave // Chance	|1|W
   * Instant // Sorcery
   * Return any number of target permanents you own to your hand.
   * //
   * Aftermath
   * Discard any number of cards, then draw that many cards. */

  int i;
  card_instance_t *instance;
  int discarded_count;
  int valid_or_selected;
  target_t *target_cursor;
  target_t *other_target_cursor;
  target_definition_t td;
  test_definition_t test;

  default_target_definition(player, card, &td, 0x47);
  td.allowed_controller = player;
  td.allow_cancel = 3;
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    discarded_count = basic_spell(player, card, EVENT_CAN_CAST);
    if ((discarded_count != 0) &&
        (discarded_count = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY),
         discarded_count != 0))
    {
      event = EVENT_GRAVEYARD_ABILITY;
    }
  }
  else if (event == EVENT_PLAY_FIRST_HALF)
  {
    i = 0;
    while (((valid_or_selected = can_target(&td), i < 10 && (valid_or_selected != 0)) &&
            (valid_or_selected = new_pick_target(&td, "TARGET_PERMANENT", i, 0),
             valid_or_selected != 0)))
    {
      other_target_cursor = instance->targets + i;
      target_cursor = instance->targets + i;
      i = i + 1;
      state_untargettable(target_cursor->player, other_target_cursor->card, 1);
    }
    if (0 < i)
    {
      target_cursor = instance->targets;
      do
      {
        other_target_cursor = target_cursor + 1;
        state_untargettable(target_cursor->player, target_cursor->card, 0);
        target_cursor = other_target_cursor;
      } while (other_target_cursor != instance->targets + i);
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    if (get_aftermath(player, card) == 0)
    {
      if (instance->number_of_targets != '\0')
      {
        for (i = 0; i < (int)(unsigned int)instance->number_of_targets;)
        {
          valid_or_selected = validate_target(player, card, &td, i);
          if (valid_or_selected)
          {
            other_target_cursor = instance->targets + i;
            target_cursor = instance->targets + i;
            ++i;
            bounce_permanent(target_cursor->player, other_target_cursor->card);
          }
          else
            ++i;
        }
      }
    }
    else
    {
      new_default_test_definition(&test, 0xff, "Select a card to discard.");
      discarded_count = 0;
      if (hand_count[player] != 0)
      {
        discarded_count = 0;
        while (valid_or_selected = new_select_a_card(player, player, 4, 0, 3, -1, &test),
               -1 < valid_or_selected)
        {
          discarded_count = discarded_count + 1;
          discard_card(player, valid_or_selected);
          if (((discarded_count == 3) &&
               ((ai_is_speculating == 1 ||
                 ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))))) ||
              (hand_count[player] == 0))
            break;
        }
      }
      draw_cards(player, discarded_count);
      kill_card(player, card, KILL_EXILE);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return aftermath(player, card, event, 3, 0, 0, 0, 1, 0, 0);
}

/* ManalinkEh 2022: 0x20d4870; original cards/hour_of_devastation.c:2638. */
int card_reason_believe(int player, int card, event_t event)
{
  /* Reason // Believe	|U
   * Sorcery // Sorcery
   * Scry 3.
   * //
   * Aftermath
   * Look at the top card of your library. You may put it onto the battlefield if it's a creature card. If you don't, put it into your hand. */

  int *library;
  int result;

  if (event == EVENT_CAN_CAST)
  {
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    if ((result != 0) &&
        (result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY), result != 0))
    {
      return aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 4, 0, 0, 1, 0, 0, 0);
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    result = get_aftermath(player, card);
    if (result == 0)
    {
      scry(player, 3);
    }
    else
    {

      library = deck_ptr[player];
      if (*library != -1)
      {
        reveal_card_iid(player, card, *library);
        result = is_what(-1, *library, 2);
        if ((result == 0) ||
            (result = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_CANCEL,
                                  DLG_NO_STORAGE, DLG_FULLCARD_ID(*library), "Put creature into play", 1, 10,
                                  "Put card into hand", 1, 1, 0, 0, 0, 0),
             result != 1))
        {
          add_card_to_hand(player, *library);
          remove_card_from_deck(player, 0);
        }
        else
        {
          put_into_play_a_card_from_deck(player, player, 0);
        }
      }
      kill_card(player, card, KILL_EXILE);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return aftermath(player, card, event, 4, 0, 0, 1, 0, 0, 0);
}

/* ManalinkEh 2022: 0x20d4af0; original cards/hour_of_devastation.c:2688. */
int card_grind_dust(int player, int card, event_t event)
{
  /* Grind // Dust	|1|B
   * Sorcery // Sorcery
   * Put a -1/-1 counter on each of up to two target creatures.
   * //
   * Aftermath
   * Exile any number of target creatures that have -1/-1 counters on them. */

  int i;
  card_instance_t *instance;
  int target_limit_or_available;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 0x20, &td, "TARGET_CREATURE", 2, NULL);
  }
  if (event != EVENT_GRAVEYARD_ABILITY)
  {
    if (event == EVENT_PLAY_FIRST_HALF)
    {
      target_limit_or_available = 2;
    }
    else
    {
      if (event != EVENT_PLAY_SECOND_HALF)
      {
        if (event == EVENT_RESOLVE_SPELL)
        {

          if (get_aftermath(player, card) == 0)
          {
            if (instance->number_of_targets != '\0')
            {
              i = 0;
              do
              {
                if (validate_target(player, card, &td, i) != 0)
                {
                  add_counters(player, card, instance->targets[i].player, instance->targets[i].card,
                               COUNTER_M1_M1, 1);
                }
                i = i + 1;
              } while (i < (int)(unsigned int)instance->number_of_targets);
            }
          }
          else
          {
            td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
            td.extra = 1;
            if (instance->number_of_targets != '\0')
            {
              i = 0;
              do
              {
                if (validate_target(player, card, &td, i) != 0)
                {
                  kill_card(instance->targets[i].player, instance->targets[i].card, KILL_EXILE);
                }
                i = i + 1;
              } while (i < (int)(unsigned int)instance->number_of_targets);
            }
          }
          instance->number_of_targets = '\0';
          kill_card(player, card, KILL_DESTROY);
        }
        return aftermath(player, card, event, 3, 0, 0, 0, 0, 1, 0);
      }
      td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
      td.extra = 1;
      target_limit_or_available = 0xb;
    }
    return generic_spell(player, card, EVENT_CAST_SPELL, 0x20, &td, "TARGET_CREATURE",
                         target_limit_or_available, NULL);
  }
  td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
  td.extra = 1;
  target_limit_or_available =
      generic_spell(player, card, EVENT_CAN_CAST, 0x20, &td, "TARGET_CREATURE", 0xb, NULL);
  if ((target_limit_or_available != 0) &&
      (target_limit_or_available = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY),
       target_limit_or_available != 0))
  {
    return aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 3, 0, 0, 0, 0, 1, 0);
  }
  return aftermath(player, card, event, 3, 0, 0, 0, 0, 1, 0);
}

/* ManalinkEh 2022: 0x20d4e40; original cards/hour_of_devastation.c:2742. */
int card_refuse_cooperate(int player, int card, event_t event)
{
  /* Refuse // Cooperate	|3|R
   * Instant // Instant
   * Refuse deals damage to target spell's controller equal to that spell's converted mana cost.
   * //
   * Aftermath
   * Copy target instant or sorcery spell. You may choose new targets for the copy. */

  int result;
  int copy_available;
  card_instance_t *instance;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_GRAVEYARD_ABILITY)
    {
      copy_available = twincast(player, card, 0x74, NULL, NULL);
      result = 0;
      if (copy_available != 0)
      {
        result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 2, 0, 1, 0, 0, 0, 1);
      }
    }
    else if (event == EVENT_PLAY_FIRST_HALF)
    {
      result = counterspell(player, card, EVENT_CAST_SPELL, NULL, 0);
    }
    else if (event == EVENT_PLAY_SECOND_HALF)
    {
      result = twincast(player, card, 0x6c, NULL, NULL);
    }
    else
    {
      if (event == EVENT_RESOLVE_SPELL)
      {
        if (get_aftermath(player, card) == 0)
        {
          if (counterspell_validate(player, card, NULL, 0) != 0)
          {

            instance = get_card_instance(player, card);
            result = get_cmc(instance->targets[0].player, instance->targets[0].card);
            damage_player(instance->targets[0].player, result, player, card);
          }
        }
        else
        {
          twincast(player, card, 0x71, NULL, NULL);
          kill_card(player, card, KILL_EXILE);
        }
        kill_card(player, card, KILL_DESTROY);
      }
      result = aftermath(player, card, event, 2, 0, 1, 0, 0, 0, 1);
    }
    return result;
  }
  return counterspell(player, card, EVENT_CAN_CAST, NULL, 0);
}

/* ManalinkEh 2022: 0x20cd3f0; original cards/hour_of_devastation.c:2785. */
static int effect_driven(int player, int card, event_t event)
{
  unsigned int packed_damage_counts;
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  modify_pt_and_abilities((int)instance->damage_target_player, instance->damage_target_card, event,
                          0, 0, 0x80);
  result = damage_dealt_by_me_arbitrary(
      (int)instance->damage_target_player, instance->damage_target_card, event,
      DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER, player, card);
  if (result != 0)
  {
    packed_damage_counts = instance->targets[1].player;
    instance->targets[1].player = 0;
    draw_cards(player, packed_damage_counts & 0xff);
    draw_cards(player, packed_damage_counts >> 8 & 0xff);
  }
  if ((event != EVENT_CLEANUP) && (event != EVENT_SHOULD_AI_PLAY))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x20cd2e0; original cards/hour_of_devastation.c:2799. */
static int effect_despair(int player, int card, event_t event)
{
  unsigned int packed_damage_counts;
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  special_abilities((int)instance->damage_target_player, instance->damage_target_card, event,
                    0x10000, player, card);
  result = damage_dealt_by_me_arbitrary(
      (int)instance->damage_target_player, instance->damage_target_card, event,
      DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER, player, card);
  if (result != 0)
  {
    packed_damage_counts = instance->targets[1].player;
    instance->targets[1].player = 0;
    new_multidiscard(0, packed_damage_counts & 0xff, 0, player);
    new_multidiscard(1, packed_damage_counts >> 8 & 0xff, 0, player);
  }
  if ((event != EVENT_CLEANUP) && (event != EVENT_SHOULD_AI_PLAY))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x20d50d0; original cards/hour_of_devastation.c:2813. */
int card_driven_despair(int player, int card, event_t event)
{
  /* Driven // Despair	|1|G
   * Sorcery // Sorcery
   * Until end of turn, creatures you control gain trample and "Whenever this creature deals combat damage to a player, draw a card."
   * //
   * Aftermath
   * Until end of turn, creatures you control gain menace and "Whenever this creature deals combat damage to a player, that player discards a card." */

  int i;
  int available_or_result;
  card_instance_t *instance;
  int creature_or_legacy;

  if (event == EVENT_CAN_CAST)
  {
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    available_or_result = basic_spell(player, card, EVENT_CAN_CAST);
    if ((available_or_result != 0) &&
        (available_or_result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY),
         available_or_result != 0))
    {
      return aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 1, 1, 0, 0, 0, 0, 0);
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    if (get_aftermath(player, card) == 0)
    {

      i = 0;
      if (0 < active_cards_count[player])
      {
        do
        {
          instance = in_play(player, i);
          if ((instance != NULL) &&
              (creature_or_legacy = is_what(player, i, 2), creature_or_legacy != 0))
          {
            creature_or_legacy =
                create_targetted_legacy_effect(player, card, effect_driven, player, i);
            alternate_legacy_text(1, player, creature_or_legacy);
          }
          i = i + 1;
        } while (i < active_cards_count[player]);
      }
    }
    else
    {

      i = 0;
      if (0 < active_cards_count[player])
      {
        do
        {
          instance = in_play(player, i);
          if ((instance != NULL) &&
              (creature_or_legacy = is_what(player, i, 2), creature_or_legacy != 0))
          {
            creature_or_legacy =
                create_targetted_legacy_effect(player, card, effect_despair, player, i);
            alternate_legacy_text(2, player, creature_or_legacy);
          }
          i = i + 1;
        } while (i < active_cards_count[player]);
      }
      kill_card(player, card, KILL_EXILE);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return aftermath(player, card, event, 1, 1, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x20ccff0; original cards/hour_of_devastation.c:2849. */
static int has_cycling(int iid, int me, int player, int card)
{
  return (unsigned int)(cards_data[iid].cc[2] == '\x04');
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20d5330; original cards/hour_of_devastation.c:2855. */
int card_abandoned_sarcophagus(int player, int card, event_t event)
{
  /* Abandoned Sarcophagus	|3
   * Artifact
   * You may cast nonland cards with cycling from your graveyard.
   * If a card with cycling would be put into your graveyard from anywhere and it wasn't cycled, exile it instead. */

  enable_xtrigger_flags |= 0xa;
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    if (card_is_being_cycled != 0)
      return 0;
    test_definition_t test;
    default_test_definition(&test, 0xff);
    test.special_selection_function = has_cycling;
    if_a_card_would_be_put_into_graveyard_from_anywhere_exile_it_instead(player, card, event,
                                                                              player, &test);
  }
  else if ((xtrigger_condition() == XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY ||
            xtrigger_condition() == 8) &&
           card_is_being_cycled == 0)
  {
    test_definition_t test;
    default_test_definition(&test, 0xff);
    test.special_selection_function = has_cycling;
    if_a_card_would_be_put_into_graveyard_from_anywhere_exile_it_instead(player, card, event,
                                                                              player, &test);
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (is_humiliated(player, card, AL_STATIC))
    return 0;
  if (event == EVENT_CAN_ACTIVATE)
  {
    for (int i = 0; get_grave(player)[i] != -1; ++i)
    {
      int iid = get_grave(player)[i];
      if (cards_data[iid].cc[2] == 4 && !is_what(-1, iid, 1))
      {
        int legal = can_legally_play_iid(player, get_grave(player)[i]);
        if (legal != 0 && has_mana_to_cast_iid(player, EVENT_CAN_ACTIVATE, get_grave(player)[i]))
        {
          if (legal == 99)
            return 99;
          if (can_sorcery_be_played(player, EVENT_CAN_ACTIVATE) ||
              (cards_data[get_grave(player)[i]].type & 0x30) != 0)
            return 1;
        }
      }
    }
    return 0;
  }
  if (event != EVENT_ACTIVATE)
    return 0;
  unsigned capacity = (unsigned)count_graveyard(player);
  int playable[2][capacity];
  int count = 0;
  for (int i = 0; get_grave(player)[i] != -1; ++i)
  {
    int iid = get_grave(player)[i];
    if (cards_data[iid].cc[2] == 4 && !is_what(-1, iid, 1) &&
        can_legally_play_iid(player, get_grave(player)[i]) &&
        has_mana_to_cast_iid(player, EVENT_ACTIVATE, get_grave(player)[i]) &&
        (can_sorcery_be_played(player, EVENT_ACTIVATE) ||
         (cards_data[get_grave(player)[i]].type & 0x30) != 0))
    {
      playable[0][count] = get_grave(player)[i];
      playable[1][count++] = i;
    }
  }
  test_definition_t test;
  _Static_assert(sizeof(test) == (sizeof(void *) == 8 ? 224 : 216), "native Sarcophagus filter");
  new_default_test_definition(&test, 0xff, "Select a card to play.");
  int selected = select_card_from_zone(player, player, playable[0], count, 0, 1, -1, &test);
  if (selected != -1 && charge_mana_from_id(player, -1, EVENT_ACTIVATE,
                                            (uint16_t)cards_data[playable[0][selected]].id))
  {
    play_card_in_grave_for_free(player, player, playable[1][selected]);
    cant_be_responded_to = 1;
    return 0;
  }
  spell_fizzled = 1;
  return 0;
}
#endif

/* ManalinkEh 2022: 0x20d57a0; original cards/hour_of_devastation.c:2937. */
int card_crook_of_condemnation(int player, int card, event_t event)
{
  /* Crook of Condemnation	|2
   * Artifact
   * |1, |T: Exile target card from a graveyard.
   * |1, Exile ~: Exile all cards from all graveyards. */

  enum
  {
    CHOICE_TARGET_EXILES = 1,
    CHOICE_EXILE_ALL = 2
  };

  int choice;
  card_instance_t *instance;
  int mass_exile_available;
  int available_or_mode;
  target_definition_t td;
  test_definition_t test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 1, 0, 0, 0, 0, 0, 0, &td,
                                    NULL) != 0)
      {
        return 1;
      }
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL) == 0)
      {
        return 0;
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      available_or_mode = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0,
                                                    0, 0, 0, &td, NULL);
      mass_exile_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0,
                                                       0, 0, 0, 0, NULL, NULL);
      choice =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Exile a card from a graveyard",
                      available_or_mode, 5, "Exile all graveyards", mass_exile_available,
                      count_graveyard(1 - player), 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_TARGET_EXILES)
      {
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                         "TARGET_PLAYER");
      }
      if (choice != CHOICE_EXILE_ALL)
      {
        return 0;
      }
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      kill_card(player, card, KILL_EXILE);
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
    td.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    available_or_mode = instance->info_slot;
    if (available_or_mode == CHOICE_TARGET_EXILES)
    {
      available_or_mode = valid_target(&td);
      if ((available_or_mode != 0) &&
          (available_or_mode = count_graveyard(instance->targets[0].player),
           available_or_mode != 0))
      {
        new_default_test_definition(&test, 0xff, "Select a card to exile.");
        new_global_tutor(player, instance->targets[0].player, 2, 6, 1, 3, &test);
      }
      available_or_mode = instance->info_slot;
    }
    if (available_or_mode == CHOICE_EXILE_ALL)
    {

      APNAP(p, {
        rfg_whole_graveyard(p);
      });
      return 0;
    }
  }
  return 0;
}

/* Dagger of the Worthy	|2
 * Artifact - Equipment
 * Equipped creature gets +2/+0 and has afflict 1.
 * Equip |2 */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20d5cb0; original cards/hour_of_devastation.c:3014. */
int card_god_pharaohs_gift(int player, int card, event_t event)
{
  /* God-Pharaoh's Gift	|7
   * Artifact
   * At the beginning of combat on your turn, you may exile a creature card from your graveyard. If you do, create a token that's a copy of that card, except it's a 4/4 |Sblack Zombie. It gains haste until end of turn. */

  int *graveyard;
  card_instance_t *instance;
  int result;
  token_generation_t token;
  test_definition_t test;

  new_default_test_definition(&test, 2, "Select a creature card to exile.");
  instance = get_card_instance(player, card);
  instance->number_of_targets = '\0';
  if (event == EVENT_CHANGE_TYPE)
  {
    result = engine_any_legal_attacker(player);
    if (result == 0)
    {
      event_flags = event_flags | EA_FORCE_ATTACK;
      return 0;
    }
  }
  else if ((event == EVENT_BEGIN_COMBAT) && (current_turn == player))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        result = count_graveyard_by_type(player, 2);
        if (0 < result)
        {
          result = graveyard_has_shroud(player);
          if (result == 0)
          {
            graveyard = graveyard_ptr_mutable[player];
            result = new_select_a_card(player, player, 2, 0, 2, -1, &test);
            if (result != -1)
            {
              default_token_definition(player, card, (unsigned int)cards_data[graveyard[result]].id,
                                       &token);
              token.action = TOKEN_ACTION_ADD_SUBTYPE | TOKEN_ACTION_HASTE;
              token.pow = 4;
              token.tou = 4;
              token.action_argument = 0xde;
              token.color_forced = '\x02';
              generate_token(&token);
              rfg_card_from_grave(player, result);
            }
          }
        }
      }
    }
  }
  return 0;
}
#endif

/* Graven Abomination	|3
 * Artifact Creature - Horror 3/1
 * Whenever ~ attacks, exile target card from defending player's graveyard. */

/* ManalinkEh 2022: 0x20d5e60; original cards/hour_of_devastation.c:3056. */
int card_hollow_one(int player, int card, event_t event)
{
  /* Hollow One	|5
   * Artifact Creature - Golem 4/4
   * This spell costs |2 less to cast for each card you've cycled or discarded this turn.
   * Cycling |2 */

  int result;
  if (event == EVENT_MODIFY_COST)
  {
    result = get_trap_condition(player, 0x203);
    COST_COLORLESS = COST_COLORLESS + (char)result * -2;
    return 0;
  }
  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    return 0;
  }
  result = cycling(player, card, event, 2, 0, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x20d5ef0; original cards/hour_of_devastation.c:3072. */
int card_mirage_mirror(int player, int card, event_t event)
{
  /* Mirage Mirror	|3
   * Artifact
   * |2: ~ becomes a copy of target artifact, creature, enchantment, or land until end of turn. */

  int src_card;
  int source_player;
  card_instance_t *instance;
  int valid;
  card_instance_t *source_in_play;
  target_definition_t td;

  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 0x47);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    source_player = instance->parent_controller;
    src_card = instance->parent_card;
    valid = valid_target(&td);
    if ((valid != 0) && (source_in_play = in_play(player, card), source_in_play != NULL))
    {
      shapeshift_target(source_player, src_card, source_player, src_card,
                        instance->targets[0].player, instance->targets[0].card, 1);
    }
  }
  return generic_activated_ability(player, card, event, 4, 2, 0, 0, 0, 0, 0, 0, &td,
                                   "TARGET_PERMANENT");
}

/* Sunset Pyramid	|2
 * Artifact
 * ~ enters the battlefield with three brick counters on it.
 * |2, |T, Remove a brick counter from ~: Draw a card.
 * |2, |T: Scry 1. */

/* Wall of Forgotten Pharaohs	|2
 * Artifact Creature - Wall 0/4
 * Defender
 * |T: ~ deals 1 damage to target player or planeswalker. Activate this ability only if you control a Desert or there is a Desert card in your graveyard. */

/* Crypt of the Eternals	""
 * Land
 * When ~ enters the battlefield, you gain 1 life.
 * |T: Add |C.
 * |1, |T: Add |U, |B, or |R. */

/* ManalinkEh 2022: 0x20d6060; original cards/hour_of_devastation.c:3115. */
int card_desert_of_the_fervent(int player, int card, event_t event)
{
  /* Desert of the Fervent	""
   * Land - Desert
   * ~ enters the battlefield tapped.
   * |T: Add |R.
   * Cycling |1|R */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = plain_mana_producer_tapped(player, card, event);
    return result;
  }
  return cycling(player, card, event, 1, 0, 0, 0, 1, 0);
}

/* ManalinkEh 2022: 0x20d60d0; original cards/hour_of_devastation.c:3129. */
int card_desert_of_the_glorified(int player, int card, event_t event)
{
  /* Desert of the Glorified	""
   * Land - Desert
   * ~ enters the battlefield tapped.
   * |T: Add |B.
   * Cycling |1|B */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = plain_mana_producer_tapped(player, card, event);
    return result;
  }
  return cycling(player, card, event, 1, 1, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x20d6140; original cards/hour_of_devastation.c:3143. */
int card_desert_of_the_indomitable(int player, int card, event_t event)
{
  /* Desert of the Indomitable	""
   * Land - Desert
   * ~ enters the battlefield tapped.
   * |T: Add |G.
   * Cycling |1|G */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = plain_mana_producer_tapped(player, card, event);
    return result;
  }
  return cycling(player, card, event, 1, 0, 0, 1, 0, 0);
}

/* ManalinkEh 2022: 0x20d61b0; original cards/hour_of_devastation.c:3157. */
int card_desert_of_the_mindful(int player, int card, event_t event)
{
  /* Desert of the Mindful	""
   * Land - Desert
   * ~ enters the battlefield tapped.
   * |T: Add |U.
   * Cycling |1|U */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = plain_mana_producer_tapped(player, card, event);
    return result;
  }
  return cycling(player, card, event, 1, 0, 1, 0, 0, 0);
}

/* ManalinkEh 2022: 0x20d6220; original cards/hour_of_devastation.c:3171. */
int card_desert_of_the_true(int player, int card, event_t event)
{
  /* Desert of the True	""
   * Land - Desert
   * ~ enters the battlefield tapped.
   * |T: Add |W.
   * Cycling |1|W */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = plain_mana_producer_tapped(player, card, event);
    return result;
  }
  return cycling(player, card, event, 1, 0, 0, 0, 0, 1);
}

/* ManalinkEh 2022: 0x20d6290; original cards/hour_of_devastation.c:3185. */
int card_dunes_of_the_dead(int player, int card, event_t event)
{
  /* Dunes of the Dead	""
   * Land - Desert
   * |T: Add |C.
   * When ~ is put into a graveyard from the battlefield, create a 2/2 |Sblack Zombie creature token. */

  int result;
  token_generation_t token;

  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
      generate_token(&token);
    }
  }
  result = plain_mana_producer(player, card, event);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20d6320; original cards/hour_of_devastation.c:3199. */
int card_endless_sands(int player, int card, event_t event)
{
  /* Endless Sands	""
   * Land - Desert
   * |T: Add |C.
   * |2, |T: Exile target creature you control.
   * |4, |T, Sacrifice ~: Return each creature card exiled with ~ to the battlefield under its owner's control. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_STORE = 2,
    CHOICE_FREE = 3,
    CHOICE_SHOW = 4
  };

  if (event == EVENT_RESOLVE_SPELL)
  {
    play_land_sound_effect(player, card);
    return mana_producer(player, card, event);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (affect_me(player, card) && !is_humiliated(player, card, AL_TRIG))
      ai_modifier += count_subtype(player, 1, -1) * 12;
    return mana_producer(player, card, event);
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return mana_producer(player, card, event);
  card_instance_t *instance = get_card_instance(player, card);
  target_definition_t td;
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = td.preferred_controller = player;
  if (player == 1)
  {
    td.required_state = TARGET_STATE_DESTROYED;
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(1, card, event, 0x800005, 2, 0, 0, 0, 0, 0, 0, &td,
                                         "TARGET_CREATURE"))
        return 99;
      if (generic_activated_ability(player, card, event, 9, 5, 0, 0, 0, 0, 0, 0, NULL, NULL))
        return 1;
      return mana_producer(player, card, event);
    }
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    if (generic_activated_ability(player, card, event, 5, 3, 0, 0, 0, 0, 0, 0, &td,
                                       "SELECT_A_CREATURE_YOU_CONTROL"))
      return 1;
    if (exiledby_count(player, card, 2))
      return 1;
    if (generic_activated_ability(player, card, event, 9, 5, 0, 0, 0, 0, 0, 0, NULL, NULL))
      return 1;
    return mana_producer(player, card, event);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->info_slot == CHOICE_STORE)
    {
      if (valid_target(&td))
      {
        int target_card = instance->targets[0].card;
        int target_player = instance->targets[0].player;
        if (player == 1)
        {
          regenerate_target(target_player, target_card);
          target_card = instance->targets[0].card;
          target_player = instance->targets[0].player;
        }
        if (!is_token(target_player, target_card))
        {
          exile_card_and_remember_it_on_exiledby(instance->parent_controller, instance->parent_card,
                                                 instance->targets[0].player,
                                                 instance->targets[0].card);
          card_instance_t *parent =
              get_card_instance(instance->parent_controller, instance->parent_card);
          int previous = parent->targets[1].player;
          int count = previous == -1 ? 1 : previous + 1;
          get_card_instance(instance->parent_controller, instance->parent_card)->targets[1].player =
              count;
        }
        else
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      }
    }
    if (instance->info_slot == CHOICE_FREE)
    {
      int leg = 0, idx = 0;
      int *location;
      while ((location = exiledby_find_any(player - 2, instance->targets[1].card, &leg, &idx)) !=
             NULL)
      {
        uint32_t packed = (uint32_t)*location;
        *location = -1;
        int owner = packed >> 31;
        int iid = packed & 0x7fffffff;
        if (check_rfg(owner, (uint16_t)cards_data[iid].id))
        {
          int added = add_card_to_hand(player, iid);
          remove_card_from_rfg(owner, (uint16_t)cards_data[iid].id);
          put_into_play(player, added);
        }
      }
    }
    return 0;
  }
  if (event != EVENT_ACTIVATE)
    return 0;
  int choice = 1;
  if (!paying_mana())
  {
    int can_exile = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 3, 0, 0, 0,
                                                   0, 0, 0, &td, "TARGET_CREATURE");
    if (can_exile)
      choice = 6;
    else if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 5, 0, 0, 0, 0, 0,
                                            0, NULL, NULL))
      choice = 6;
  }
  instance->info_slot = choice;
  int can_mana = ((instance->state >> 4) ^ 1) & 1;
  int can_exile = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 2, 0, 0, 0, 0,
                                                 0, 0, &td, "SELECT_A_CREATURE_YOU_CONTROL");
  int can_return = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 4, 0, 0, 0,
                                                  0, 0, 0, NULL, NULL);
  int can_show = player == 0 && exiledby_count(0, card, 2) != 0;
  if (choice == 6)
  {
    if (ai_is_speculating == 1 || (player == 1 && (trace_mode & 2) == 0))
    {
      spell_fizzled = 1;
      return 0;
    }
    int previous = instance->targets[1].player;
    int return_weight = previous < 0 ? 0 : previous * 5;
    choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, "Get mana", can_mana,
                         10, "Exile a creature", can_exile, 10, "Free buried creatures", can_return,
                         return_weight, "Show buried creatures", can_show, 1, 0, 0, 0, 0);
    if (choice == 6)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  if (choice == CHOICE_MANA)
    return mana_producer(player, card, EVENT_ACTIVATE);
  if (choice == CHOICE_STORE)
  {
    add_state(player, card, 0x10);
    if (charge_mana_for_activated_ability(player, card, 2, 0, 0, 0, 0, 0))
      pick_target(&td, "TARGET_CREATURE");
    tapped_for_mana_color = -1;
    if (spell_fizzled == 1)
      remove_state(player, card, 0x10);
  }
  else
  {
    if (choice == CHOICE_FREE)
    {
      add_state(player, card, 0x10);
      if (charge_mana_for_activated_ability(player, card, 4, 0, 0, 0, 0, 0))
      {
        instance->targets[1].card = exiledby_detach(player, card);
        kill_card(player, card, KILL_SACRIFICE);
      }
      tapped_for_mana_color = -1;
      if (spell_fizzled != 1)
      {
        instance->info_slot = choice;
        return 0;
      }
    }
    if (spell_fizzled == 1)
      remove_state(player, card, 0x10);
    if (choice == CHOICE_SHOW)
    {
      exiledby_choose(player, card, 0x42e9, EXBY_CHOOSE, 0, "Creature", 0);
      spell_fizzled = 1;
      return 0;
    }
  }
  if (spell_fizzled != 1)
    instance->info_slot = choice;
  return 0;
}
#endif

/* ManalinkEh 2022: 0x20d6e10; original cards/hour_of_devastation.c:3344. */
int card_hashep_oasis(int player, int card, event_t event)
{
  /* Hashep Oasis	""
   * Land - Desert
   * |T: Add |C.
   * |T, Pay 1 life: Add |G.
   * |1|G|G, |T, Sacrifice a Desert: Target creature gets +3/+3 until end of turn. Activate this ability only any time you could cast a sorcery. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_SAC = 2
  };

  card_instance_t *instance;
  int result;
  bool not_speculating;
  target_definition_t td;
  test_definition_t test;

  if (event == EVENT_COUNT_MANA)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = is_tapped(player, card), result == 0)) &&
        ((result = is_animated_and_sick(player, card),
          result == 0 && (result = can_produce_mana(player, card), result != 0))))
    {
      result = check_special_flags2(player, card, 0x40);
      if ((result == 0) || (result = is_what(player, card, 1), result == 0))
      {

        instance = get_card_instance(player, card);
        if (((ai_is_speculating == 1) ||
             ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
            (life[player] < 2))
        {
          if ((instance->mana_color & 1) != 0)
          {
            declare_mana_available(player, 0, 1);
          }
        }
        else
        {
          declare_mana_available_hex(player, (unsigned int)instance->mana_color, 1);
        }
      }
      else
      {
        declare_mana_available(player, 1, 1);
      }
    }
    return mana_producer(player, card, event);
  }
  if (event == EVENT_CAN_ACTIVATE)
    return mana_producer(player, card, event);
  if (event != EVENT_ACTIVATE)
  {
    if ((event == EVENT_CLEANUP) || (event == EVENT_RESOLVE_ACTIVATION))
    {
      default_target_definition(player, card, &td, 2);
      new_default_test_definition(&test, 0x47, "Select a Desert to sacrifice.");
      test.subtype = 0x5000;
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot == 2)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          pump_until_eot_merge_previous(player, card, instance->targets[0].player,
                                        instance->targets[0].card, 3, 3);
          return 0;
        }
        return 0;
      }
      return 0;
    }
    return mana_producer(player, card, event);
  }
  default_target_definition(player, card, &td, 2);
  new_default_test_definition(&test, 0x47, "Select a Desert to sacrifice.");
  test.subtype = 0x5000;
  instance = get_card_instance(player, card);

  bool produce = false;
  result = paying_mana();
  if (((result == 0) && (result = can_sorcery_be_played(player, EVENT_ACTIVATE), result != 0)) &&
      (result = generic_activated_ability_with_sacrifice_as_cost(
           player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, 1, 0, 0, 3, 0, 0, 0, &td,
           "TARGET_CREATURE", TYPE_NONE, &test, SARV_0),
       result != 0))
  {
    not_speculating = ai_is_speculating != 1;
    instance->info_slot = 2;
    if ((not_speculating) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
    {
      result = can_target(&td);
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                           "Sac a desert and grow a creature", result, 1, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_MANA)
        produce = true;
      else if (result != CHOICE_SAC)
      {
        return 0;
      }
    }
    if (!produce)
    {
      add_state(player, card, 0x10);
      generic_activated_ability_with_sacrifice_as_cost(
          player, card, EVENT_ACTIVATE, GAA_CAN_SORCERY_BE_PLAYED | GAA_CAN_TARGET | GAA_UNTAPPED,
          1, 0, 0, 2, 0, 0, 0, &td, "TARGET_CREATURE", TYPE_NONE, &test, SARV_0);
      if (spell_fizzled == 1)
      {
        remove_state(player, card, 0x10);
      }
    }
  }
  else
  {
    instance->info_slot = 1;
    produce = true;
  }
  if (produce)
  {

    instance = get_card_instance(player, card);
    if ((((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
         (life[player] < 2)) &&
        ((instance->mana_color & 1) != 0))
    {
      produce_mana_tapped_all_one_color(player, card, COLOR_TEST_COLORLESS, 1);
    }
    else
    {
      produce_mana_tapped_all_one_color(player, card, (unsigned int)instance->mana_color, 1);
      if ((chosen_colors & ((mana_colors_added[player] | COLOR_TEST_COLORLESS) ^ ~COLOR_TEST_0)) !=
          COLOR_TEST_0)
      {
        lose_life(player, 1);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x20d7400; original cards/hour_of_devastation.c:3436. */
int card_hostile_desert(int player, int card, event_t event)
{
  /* Hostile Desert	""
   * Land - Desert
   * |T: Add |C.
   * |2, Exile a land card from your graveyard: ~ becomes a 3/4 Elemental creature until end of turn. It's still a land. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ANIMATE = 2
  };

  unsigned int token_flags;
  int available_or_choice;
  card_instance_t *instance;
  int mana_available;
  unsigned int animation_available;
  test_definition_t this_test;

  new_default_test_definition(&this_test, 1, "Select a land card to exile.");
  if ((event == EVENT_RESOLVE_ACTIVATION) ||
      (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE))
  {

    instance = get_card_instance(player, card);
    token_flags = instance->token_status;
    available_or_choice = paying_mana();
    if ((available_or_choice == 0) &&
        ((available_or_choice = can_use_activated_abilities(player, card),
          available_or_choice != 0 &&
              (available_or_choice = count_graveyard_by_type(player, 1), 0 < available_or_choice))))
    {
      available_or_choice = has_mana(player, 7, 3);
      animation_available = (unsigned int)(available_or_choice != 0);
    }
    else
    {
      animation_available = 0;
    }
    available_or_choice = paying_mana();
    mana_available = mana_producer(player, card, EVENT_CAN_ACTIVATE);
    available_or_choice =
        dialog_impl(player, card, event, DLG_AUTOCHOOSE_IF_1, DLG_NO_DISPLAY_FOR_AI, "Produce mana", mana_available,
                    available_or_choice, "Become Elemental", animation_available,
                    (token_flags >> 0xc ^ 1) & 1, DLG_MANA6(2, 0, 0, 0, 0, 0), 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return available_or_choice;
    }
    if (available_or_choice == 0)
    {
      return 0;
    }
    if (available_or_choice == CHOICE_ANIMATE)
    {
      if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
      {
        if (((card == affected_card) && (player == affected_card_controller)) &&
            (available_or_choice = new_global_tutor(player, player, 2, 6, 0, 3, &this_test),
             available_or_choice < 0))
        {
          spell_fizzled = 1;
        }
        if (event != EVENT_RESOLVE_ACTIVATION)
        {
          return 0;
        }
      }
      animate_self(player, card, 3, 4, KEYWORD_0, SP_KEYWORD_0, COLOR_TEST_0, 0);
      return 0;
    }
  }
  has_subtype_if_animated_self(player, card, event, SUBTYPE_ELEMENTAL);
  return mana_producer(player, card, event);
}

/* ManalinkEh 2022: 0x20d76b0; original cards/hour_of_devastation.c:3481. */
int card_ifnir_deadlands(int player, int card, event_t event)
{
  /* Ifnir Deadlands	""
   * Land - Desert
   * |T: Add |C.
   * |T, Pay 1 life: Add |B.
   * |2|B|B, |T, Sacrifice a Desert: Put two -1/-1 counters on target creature an opponent controls. Activate this ability only any time you could cast a sorcery. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_SAC = 2
  };

  card_instance_t *instance;
  int result;
  bool not_speculating;
  target_definition_t td;
  test_definition_t test;

  if (event == EVENT_COUNT_MANA)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = is_tapped(player, card), result == 0)) &&
        ((result = is_animated_and_sick(player, card),
          result == 0 && (result = can_produce_mana(player, card), result != 0))))
    {
      result = check_special_flags2(player, card, 0x40);
      if ((result == 0) || (result = is_what(player, card, 1), result == 0))
      {

        instance = get_card_instance(player, card);
        if (((ai_is_speculating == 1) ||
             ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
            (life[player] < 2))
        {
          if ((instance->mana_color & 1) != 0)
          {
            declare_mana_available(player, 0, 1);
          }
        }
        else
        {
          declare_mana_available_hex(player, (unsigned int)instance->mana_color, 1);
        }
      }
      else
      {
        declare_mana_available(player, 1, 1);
      }
    }
    return mana_producer(player, card, event);
  }
  if (event == EVENT_CAN_ACTIVATE)
    return mana_producer(player, card, event);
  if (event != EVENT_ACTIVATE)
  {
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = 1 - player;
      new_default_test_definition(&test, 0x47, "Select a Desert to sacrifice.");
      test.subtype = 0x5000;
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot == 2)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                       COUNTER_M1_M1, 2);
          return 0;
        }
        return 0;
      }
      return 0;
    }
    return mana_producer(player, card, event);
  }
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  new_default_test_definition(&test, 0x47, "Select a Desert to sacrifice.");
  test.subtype = 0x5000;
  instance = get_card_instance(player, card);

  bool produce = false;
  result = paying_mana();
  if (((result == 0) && (result = can_sorcery_be_played(player, EVENT_ACTIVATE), result != 0)) &&
      (result = generic_activated_ability_with_sacrifice_as_cost(
           player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, 2, 3, 0, 0, 0, 0, 0, &td,
           "TARGET_CREATURE_OPPONENT_CONTROLS", TYPE_NONE, &test, SARV_0),
       result != 0))
  {
    not_speculating = ai_is_speculating != 1;
    instance->info_slot = 2;
    if ((not_speculating) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
    {
      result = can_target(&td);
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                           "Sac a desert and shrink a creature", result, 1, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_MANA)
        produce = true;
      else if (result != CHOICE_SAC)
      {
        return 0;
      }
    }
    if (!produce)
    {
      add_state(player, card, 0x10);
      generic_activated_ability_with_sacrifice_as_cost(
          player, card, EVENT_ACTIVATE, GAA_CAN_TARGET, 2, 2, 0, 0, 0, 0, 0, &td,
          "TARGET_CREATURE_OPPONENT_CONTROLS", TYPE_NONE, &test, SARV_0);
      if (spell_fizzled == 1)
      {
        remove_state(player, card, 0x10);
      }
    }
  }
  else
  {
    instance->info_slot = 1;
    produce = true;
  }
  if (produce)
  {

    instance = get_card_instance(player, card);
    if ((((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
         (life[player] < 2)) &&
        ((instance->mana_color & 1) != 0))
    {
      produce_mana_tapped_all_one_color(player, card, COLOR_TEST_COLORLESS, 1);
    }
    else
    {
      produce_mana_tapped_all_one_color(player, card, (unsigned int)instance->mana_color, 1);
      if ((chosen_colors & ((mana_colors_added[player] | COLOR_TEST_COLORLESS) ^ ~COLOR_TEST_0)) !=
          COLOR_TEST_0)
      {
        lose_life(player, 1);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x20d7c90; original cards/hour_of_devastation.c:3574. */
int card_ipnu_rivulet(int player, int card, event_t event)
{
  /* Ipnu Rivulet	""
   * Land - Desert
   * |T: Add |C.
   * |T, Pay 1 life: Add |U.
   * |1|U, |T, Sacrifice a Desert: Target player puts the top four cards of their library into their graveyard. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_SAC = 2
  };

  card_instance_t *instance;
  int result;
  bool not_speculating;
  target_definition_t td;
  test_definition_t test;

  if (event == EVENT_COUNT_MANA)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = is_tapped(player, card), result == 0)) &&
        ((result = is_animated_and_sick(player, card),
          result == 0 && (result = can_produce_mana(player, card), result != 0))))
    {
      result = check_special_flags2(player, card, 0x40);
      if ((result == 0) || (result = is_what(player, card, 1), result == 0))
      {

        instance = get_card_instance(player, card);
        if (((ai_is_speculating == 1) ||
             ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
            (life[player] < 2))
        {
          if ((instance->mana_color & 1) != 0)
          {
            declare_mana_available(player, 0, 1);
          }
        }
        else
        {
          declare_mana_available_hex(player, (unsigned int)instance->mana_color, 1);
        }
      }
      else
      {
        declare_mana_available(player, 1, 1);
      }
    }
    return mana_producer(player, card, event);
  }
  if (event == EVENT_CAN_ACTIVATE)
    return mana_producer(player, card, event);
  if (event != EVENT_ACTIVATE)
  {
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      default_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      new_default_test_definition(&test, 0x47, "Select a Desert to sacrifice.");
      test.subtype = 0x5000;
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot == 2)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          mill(instance->targets[0].player, 4);
          return 0;
        }
        return 0;
      }
      return 0;
    }
    return mana_producer(player, card, event);
  }
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  new_default_test_definition(&test, 0x47, "Select a Desert to sacrifice.");
  test.subtype = 0x5000;
  instance = get_card_instance(player, card);

  bool produce = false;
  result = paying_mana();
  if ((result == 0) && (result = generic_activated_ability_with_sacrifice_as_cost(
                            player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, 1, 0, 2, 0, 0, 0, 0,
                            &td, "TARGET_PLAYER", TYPE_NONE, &test, SARV_0),
                        result != 0))
  {
    not_speculating = ai_is_speculating != 1;
    instance->info_slot = 2;
    if ((not_speculating) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
    {
      result = can_target(&td);
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                           "Sac a desert and mill four cards", result, 1, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_MANA)
        produce = true;
      else if (result != CHOICE_SAC)
      {
        return 0;
      }
    }
    if (!produce)
    {
      add_state(player, card, 0x10);
      generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_ACTIVATE, GAA_CAN_TARGET,
                                                       1, 0, 1, 0, 0, 0, 0, &td, "TARGET_OPPONENT",
                                                       TYPE_NONE, &test, SARV_0);
      if (spell_fizzled == 1)
      {
        remove_state(player, card, 0x10);
      }
    }
  }
  else
  {
    instance->info_slot = 1;
    produce = true;
  }
  if (produce)
  {

    instance = get_card_instance(player, card);
    if ((((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
         (life[player] < 2)) &&
        ((instance->mana_color & 1) != 0))
    {
      produce_mana_tapped_all_one_color(player, card, COLOR_TEST_COLORLESS, 1);
    }
    else
    {
      produce_mana_tapped_all_one_color(player, card, (unsigned int)instance->mana_color, 1);
      if ((chosen_colors & ((mana_colors_added[player] | COLOR_TEST_COLORLESS) ^ ~COLOR_TEST_0)) !=
          COLOR_TEST_0)
      {
        lose_life(player, 1);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x20d8250; original cards/hour_of_devastation.c:3667. */
int card_ramunap_ruins(int player, int card, event_t event)
{
  /* Ramunap Ruins	""
   * Land - Desert
   * |T: Add |C.
   * |T, Pay 1 life: Add |R.
   * |2|R|R, |T, Sacrifice a Desert: ~ deals 2 damage to each opponent. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_SAC = 2
  };

  card_instance_t *instance;
  int result;
  bool not_speculating;
  test_definition_t test;

  if (event == EVENT_COUNT_MANA)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = is_tapped(player, card), result == 0)) &&
        ((result = is_animated_and_sick(player, card),
          result == 0 && (result = can_produce_mana(player, card), result != 0))))
    {
      result = check_special_flags2(player, card, 0x40);
      if ((result == 0) || (result = is_what(player, card, 1), result == 0))
      {

        instance = get_card_instance(player, card);
        if (((ai_is_speculating == 1) ||
             ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
            (life[player] < 2))
        {
          if ((instance->mana_color & 1) != 0)
          {
            declare_mana_available(player, 0, 1);
          }
        }
        else
        {
          declare_mana_available_hex(player, (unsigned int)instance->mana_color, 1);
        }
      }
      else
      {
        declare_mana_available(player, 1, 1);
      }
    }
    return mana_producer(player, card, event);
  }
  if (event == EVENT_CAN_ACTIVATE)
    return mana_producer(player, card, event);
  if (event != EVENT_ACTIVATE)
  {
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      new_default_test_definition(&test, 0x47, "Select a Desert to sacrifice.");
      test.subtype = 0x5000;
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot == 2)
      {
        damage_player(1 - player, 2, player, card);
        return 0;
      }
      return 0;
    }
    return mana_producer(player, card, event);
  }
  new_default_test_definition(&test, 0x47, "Select a Desert to sacrifice.");
  test.subtype = 0x5000;
  instance = get_card_instance(player, card);

  bool produce = false;
  result = paying_mana();
  if ((result == 0) && (result = generic_activated_ability_with_sacrifice_as_cost(
                            player, card, EVENT_CAN_ACTIVATE, GAA_NONE, 2, 0, 0, 0, 3, 0, 0, NULL,
                            NULL, TYPE_NONE, &test, SARV_0),
                        result != 0))
  {
    not_speculating = ai_is_speculating != 1;
    instance->info_slot = 2;
    if ((not_speculating) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
    {
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                           "Sac a desert and damage opponent", 1, 1, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_MANA)
        produce = true;
      else if (result != CHOICE_SAC)
      {
        return 0;
      }
    }
    if (!produce)
    {
      add_state(player, card, 0x10);
      generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_ACTIVATE, GAA_NONE, 2, 0,
                                                       0, 0, 2, 0, 0, NULL, NULL, TYPE_NONE, &test,
                                                       SARV_0);
      if (spell_fizzled == 1)
      {
        remove_state(player, card, 0x10);
      }
    }
  }
  else
  {
    instance->info_slot = 1;
    produce = true;
  }
  if (produce)
  {

    instance = get_card_instance(player, card);
    if ((((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
         (life[player] < 2)) &&
        ((instance->mana_color & 1) != 0))
    {
      produce_mana_tapped_all_one_color(player, card, COLOR_TEST_COLORLESS, 1);
    }
    else
    {
      produce_mana_tapped_all_one_color(player, card, (unsigned int)instance->mana_color, 1);
      if ((chosen_colors & ((mana_colors_added[player] | COLOR_TEST_COLORLESS) ^ ~COLOR_TEST_0)) !=
          COLOR_TEST_0)
      {
        lose_life(player, 1);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x20d87a0; original cards/hour_of_devastation.c:3755. */
int card_scavenger_grounds(int player, int card, event_t event)
{
  /* Scavenger Grounds	""
   * Land - Desert
   * |T: Add |C.
   * |2, |T, Sacrifice a Desert: Exile all cards from all graveyards. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_SAC = 2
  };

  int result;
  card_instance_t *instance;
  bool not_speculating;
  test_definition_t test;

  if (event == EVENT_CAN_ACTIVATE)
  {
    return mana_producer(player, card, event);
  }
  if (event != EVENT_ACTIVATE)
  {
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      new_default_test_definition(&test, 0x47, "Select a Desert to sacrifice.");
      test.subtype = 0x5000;
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot != 2)
      {
        return 0;
      }

      APNAP(p, {
        rfg_whole_graveyard(p);
      });
      return 0;
    }
    return mana_producer(player, card, event);
  }
  new_default_test_definition(&test, 0x47, "Select a Desert to sacrifice.");
  test.subtype = 0x5000;
  instance = get_card_instance(player, card);

  bool produce = false;
  result = paying_mana();
  if ((result == 0) && (result = generic_activated_ability_with_sacrifice_as_cost(
                            player, card, EVENT_CAN_ACTIVATE, GAA_NONE, 3, 0, 0, 0, 0, 0, 0, NULL,
                            NULL, TYPE_NONE, &test, SARV_0),
                        result != 0))
  {
    not_speculating = ai_is_speculating != 1;
    instance->info_slot = 2;
    if ((not_speculating) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
    {
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                           "Sac a desert and exile graveyards", 1, 1, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_MANA)
        produce = true;
      else if (result != CHOICE_SAC)
      {
        return 0;
      }
    }
    if (!produce)
    {
      add_state(player, card, 0x10);
      generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_ACTIVATE, GAA_UNTAPPED,
                                                       2, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE,
                                                       &test, SARV_0);
      if (spell_fizzled == 1)
      {
        remove_state(player, card, 0x10);
      }
    }
  }
  else
  {
    instance->info_slot = 1;
    produce = true;
  }
  if (produce)
  {
    mana_producer(player, card, EVENT_ACTIVATE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x20d8af0; original cards/hour_of_devastation.c:3813. */
int card_shefet_dunes(int player, int card, event_t event)
{
  /* Shefet Dunes	""
   * Land - Desert
   * |T: Add |C.
   * |T, Pay 1 life: Add |W.
   * |2|W|W, |T, Sacrifice a Desert: Creatures you control get +1/+1 until end of turn. Activate this ability only any time you could cast a sorcery. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_SAC = 2
  };

  card_instance_t *instance;
  int result;
  bool not_speculating;
  test_definition_t test;

  if (event == EVENT_COUNT_MANA)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = is_tapped(player, card), result == 0)) &&
        ((result = is_animated_and_sick(player, card),
          result == 0 && (result = can_produce_mana(player, card), result != 0))))
    {
      result = check_special_flags2(player, card, 0x40);
      if ((result == 0) || (result = is_what(player, card, 1), result == 0))
      {

        instance = get_card_instance(player, card);
        if (((ai_is_speculating == 1) ||
             ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
            (life[player] < 2))
        {
          if ((instance->mana_color & 1) != 0)
          {
            declare_mana_available(player, 0, 1);
          }
        }
        else
        {
          declare_mana_available_hex(player, (unsigned int)instance->mana_color, 1);
        }
      }
      else
      {
        declare_mana_available(player, 1, 1);
      }
    }
    return mana_producer(player, card, event);
  }
  if (event == EVENT_CAN_ACTIVATE)
    return mana_producer(player, card, event);
  if (event != EVENT_ACTIVATE)
  {
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      new_default_test_definition(&test, 0x47, "Select a Desert to sacrifice.");
      test.subtype = 0x5000;
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot == 2)
      {
        pump_creatures_until_eot(player, card, player, 0, 1, 1, 0, 0, NULL);
        return 0;
      }
      return 0;
    }
    return mana_producer(player, card, event);
  }
  new_default_test_definition(&test, 0x47, "Select a Desert to sacrifice.");
  test.subtype = 0x5000;
  instance = get_card_instance(player, card);

  bool produce = false;
  result = paying_mana();
  if (((result == 0) && (result = can_sorcery_be_played(player, EVENT_ACTIVATE), result != 0)) &&
      (result = generic_activated_ability_with_sacrifice_as_cost(
           player, card, EVENT_CAN_ACTIVATE, GAA_NONE, 2, 0, 0, 0, 0, 3, 0, NULL, NULL, TYPE_NONE,
           &test, SARV_0),
       result != 0))
  {
    not_speculating = ai_is_speculating != 1;
    instance->info_slot = 2;
    if ((not_speculating) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
    {
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                           "Sac a desert to pump your creatures", 1, 1, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_MANA)
        produce = true;
      else if (result != CHOICE_SAC)
      {
        return 0;
      }
    }
    if (!produce)
    {
      add_state(player, card, 0x10);
      generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_ACTIVATE, GAA_NONE, 2, 0,
                                                       0, 0, 0, 2, 0, NULL, NULL, TYPE_NONE, &test,
                                                       SARV_0);
      if (spell_fizzled == 1)
      {
        remove_state(player, card, 0x10);
      }
    }
  }
  else
  {
    instance->info_slot = 1;
    produce = true;
  }
  if (produce)
  {

    instance = get_card_instance(player, card);
    if ((((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
         (life[player] < 2)) &&
        ((instance->mana_color & 1) != 0))
    {
      produce_mana_tapped_all_one_color(player, card, COLOR_TEST_COLORLESS, 1);
    }
    else
    {
      produce_mana_tapped_all_one_color(player, card, (unsigned int)instance->mana_color, 1);
      if ((chosen_colors & ((mana_colors_added[player] | COLOR_TEST_COLORLESS) ^ ~COLOR_TEST_0)) !=
          COLOR_TEST_0)
      {
        lose_life(player, 1);
      }
    }
  }
  return 0;
}

/* Survivors' Encampment	""	=>oath_of_the_gatewatch.c:card_holdout_settlement 0x200e70b
 * Land - Desert
 * |T: Add |C.
 * |T, Tap an untapped creature you control: Add one mana of any color. */

/* Nissa, Genesis Mage	|5|G|G
 * Legendary Planeswalker - Nissa (5)
 * +2: Untap up to two target creatures and up to two target lands.
 * -3: Target creature gets +5/+5 until end of turn.
 * -10: Look at the top ten cards of your library. You may put any number of creature and/or land cards from among them onto the battlefield. Put the rest on the bottom of your library in a random order. */

/* Avid Reclaimer	|2|G
 * Creature - Human Druid 2/2
 * |T: Add |G or |U. If you control a Nissa planeswalker, you gain 2 life. */

/* Brambleweft Behemoth	|4|G|G	0x401000
 * Creature - Elemental 6/6
 * Trample */

/* Nissa's Encouragement	|4|G
 * Sorcery
 * Search your library and graveyard for a card named |H2Forest, a card named Brambleweft Behemoth, and a card named Nissa, Genesis Mage. Reveal those cards, put them into your hand, then shuffle your library. */

/* Nicol Bolas, the Deceiver	|5|U|B|R
 * Legendary Planeswalker - Bolas (5)
 * +3: Each opponent loses 3 life unless that player sacrifices a nonland permanent or discards a card.
 * -3: Destroy target creature. Draw a card.
 * -11: ~ deals 7 damage to each opponent. You draw seven cards. */

/* Wasp of the Bitter End	|1|B
 * Creature - Insect Horror 2/1
 * Flying
 * Whenever you cast a Bolas planeswalker spell, you may sacrifice ~. If you do, destroy target creature. */

/* Zealot of the God-Pharaoh	|3|R
 * Creature - Minotaur Archer 4/3
 * |4|R: ~ deals 2 damage to target opponent or planeswalker. */

/* Visage of Bolas	|4
 * Artifact
 * When ~ enters the battlefield, you may search your library and/or graveyard for a card named Nicol Bolas, the Deceiver, reveal it, and put it into your hand. If you search your library this way, shuffle it.
 * |T: Add |U, |B, or |R. */
