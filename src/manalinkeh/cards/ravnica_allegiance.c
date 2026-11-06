#include "manalink.h"

static int unique_name(int unused1, intptr_t unused2, int player, int card);

static void afterlife(int player, int card, event_t event, int qty);

static int judith_the_scourge_diva_effect(int player, int card, event_t event);

int dovin_grand_arbiter_legacy(int player, int card, event_t event);
void riot(int player, int card, event_t event);

static int vindictive_vampire_effect(int player, int card, event_t event);

int adapt(int player, int card, event_t event, int number, int cl, int b, int u, int g, int r,
          int w);

#ifndef SHANDALAR

static int tin_street_dodger_legacy(int player, int card, event_t event);

#endif

/* ManalinkEh 2022: 0x25bb240; original cards/ravnica_allegiance.c:3. */
static int biomancers_familiar_legacy(int player, int card, event_t event)
{
  if (!eot_trigger(player, card, event))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x25bb1c0; original cards/ravnica_allegiance.c:9. */
static int find_bf_legacy_effect(int target_player_id, int target_card)
{
  card_instance_t *instance;
  int card;

  if (0 < active_cards_count[target_player_id])
  {
    card = 0;
    do
    {
      while ((((instance = in_play(target_player_id, card),
                instance == NULL || (instance->internal_card_id != LEGACY_EFFECT_CUSTOM)) ||
               (instance->info_slot_fn != biomancers_familiar_legacy)) ||
              (instance->damage_target_player != target_player_id)))
      {
        card = card + 1;
        if (active_cards_count[target_player_id] <= card)
        {
          return -1;
        }
      }
      if (instance->damage_target_card == target_card)
      {
        return card;
      }
      card = card + 1;
    } while (card < active_cards_count[target_player_id]);
  }
  return -1;
}

/* ManalinkEh 2022: 0x25bbcd0; original cards/ravnica_allegiance.c:23. */
/* Concrete compiler clone: only EAX/EDX are ABI arguments. */
/* The surviving release worker covers fx == NULL. */
static int can_adapt_without_fx(int player, int card, int *fx)
{
  int result;
  result = count_counters(player, card, COUNTER_P1_P1);
  if (result < 1)
  {
    return 1;
  }
  result = find_bf_legacy_effect(player, card);
  return (unsigned int)(result != -1);
}

/* ManalinkEh 2022: 0x25bc030; original cards/ravnica_allegiance.c:34. */
int adapt(int player, int card, event_t event, int number, int cl, int b, int u, int g, int r,
          int w)
{
  int p;
  int target_card;
  int adapt_available;
  card_instance_t *instance;
  int card_00;
  int existing_counters;

  if ((event == EVENT_CAN_ACTIVATE) && (player == 1))
  {
    adapt_available = can_adapt_without_fx(1, card, NULL);
    if ((adapt_available == 0) || (instance = has_activation_on_stack(1, card), instance != NULL))
    {
      ai_modifier = ai_modifier + -99;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    p = instance->parent_controller;
    instance = get_card_instance(player, card);
    target_card = instance->parent_card;
    card_00 = find_bf_legacy_effect(p, target_card);
    existing_counters = count_counters(p, target_card, COUNTER_P1_P1);
    if (((card_00 != -1) || (existing_counters < 1)) &&
        (add_counters(p, target_card, p, target_card, COUNTER_P1_P1, number), card_00 != -1))
    {
      kill_card(p, card_00, KILL_EXILE);
    }
  }
  else if ((((uint32_t)(event - EVENT_POW_BOOST) < 2) &&
            (adapt_available = can_adapt_without_fx(player, card, NULL), adapt_available != 0)) &&
           (adapt_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, cl, b,
                                                        u, g, r, w, 0, NULL, NULL),
            adapt_available != 0))
  {
    return number;
  }
  return generic_activated_ability(player, card, event, 0, cl, b, u, g, r, w, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x25bbd20; original cards/ravnica_allegiance.c:59. */
static void afterlife(int player, int card, event_t event, int qty)
{
  int result;
  token_generation_t token;

  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return;
  }
  default_token_definition(player, card, CARD_ID_SPIRIT, &token);
  token.color_forced = '\"';
  token.no_sleight = '\x01';
  token.key_plus = 0x20;
  token.qty = (unsigned char)qty;
  generate_token(&token);
  return;
}

/* ManalinkEh 2022: 0x25bc240; original cards/ravnica_allegiance.c:71. */
void riot(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
                         "+1/+1 counter", 1, 5, "Haste", 1, 5, 0, 0, 0, 0);
    if (result == 1)
    {
      add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    }
    else if (result == 2)
    {

      result = pump_ability_until_eot_no_repeat(player, card, player, card, 0, -0x7ffffffc);
      if (result != -1)
      {
        instance = get_card_instance(player, result);
        instance->token_status = instance->token_status | 0x10000;
      }
      instance = get_card_instance(player, card);
      result = is_humiliated(player, card, AL_KEY);
      if (result == 0)
      {
        instance->state = instance->state & 0xfffdffff;
      }
    }
  }
  return;
}

#ifndef SHANDALAR
static void cmc_mod_for_spectacle(int player, int card, event_t event, int colorless,
                                  int black, int blue, int green, int red, int white)
{
  if (event == EVENT_MODIFY_COST)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int cost = get_updated_casting_cost(player, card, -1, EVENT_MODIFY_COST, colorless);
    if (has_mana_multi(player, cost, black, blue, green, red, white) &&
        get_trap_condition(1 - player, TRAP_LIFE_LOST))
    {
      instance->info_slot = 1;
      null_casting_cost(player, card);
    }
    else
      instance->info_slot = 0;
  }
}
#endif

#ifndef SHANDALAR
static int pay_spectacle(int player, int card, event_t event, int colorless, int black, int blue,
                         int green, int red, int white)
{
  card_instance_t *instance;
  int cost_or_available, mana_available_or_choice;
  instance = get_card_instance(player, card);
  cost_or_available = played_for_free(player, card);
  if (((cost_or_available == 0) &&
       (cost_or_available = is_token(player, card), cost_or_available == 0)) &&
      (instance->info_slot == 1))
  {
    cost_or_available = get_updated_casting_cost(player, card, -1, event, colorless);
    mana_available_or_choice =
        has_mana_multi(player, cost_or_available, black, blue, green, red, white);
    if (mana_available_or_choice != 0)
    {
      mana_available_or_choice = has_mana_to_cast_iid(player, event, instance->internal_card_id);
      if ((mana_available_or_choice == 0) ||
          (mana_available_or_choice =
               do_dialog(player, player, card, -1, -1,
                         " Pay Spectacle cost\n Play the spell normally\n Cancel", 0),
           mana_available_or_choice == 0))
      {
        charge_mana_multi(player, cost_or_available, black, blue, green, red, white);
        if (spell_fizzled == 1)
        {
          return 0;
        }
        instance->info_slot = instance->info_slot | 2;
        return 2;
      }
      if (mana_available_or_choice != 1)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
    cost_or_available = get_id(player, card);
    charge_mana_from_id(player, card, event, cost_or_available);
    return 1;
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25bbda0; original cards/ravnica_allegiance.c:139. */
static int spectacle_impl(int player, int card, event_t event, int colorless, int black,
                          int blue, int green, int red, int white)
{
  card_instance_t *instance;
  int cost_or_available;
  int normal_available_or_choice;

  if (event == EVENT_MODIFY_COST)
  {
    instance = get_card_instance(player, card);
    cost_or_available = has_mana_multi(
        player, get_updated_casting_cost(player, card, -1, EVENT_MODIFY_COST, colorless), black, blue,
        green, red, white);
    if ((cost_or_available == 0) ||
        (cost_or_available = get_trap_condition(1 - player, TRAP_LIFE_LOST),
         cost_or_available == 0))
    {
      instance->info_slot = 0;
    }
    else
    {
      instance->info_slot = 1;
      null_casting_cost(player, card);
    }
  }
  else
  {
    if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
        (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      cost_or_available = played_for_free(player, card);
      if (((cost_or_available == 0) &&
           (cost_or_available = is_token(player, card), cost_or_available == 0)) &&
          (instance->info_slot == 1))
      {
        cost_or_available = get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, colorless);
        if (has_mana_multi(player, cost_or_available, black, blue, green, red, white) != 0)
        {
          normal_available_or_choice =
              has_mana_to_cast_iid(player, EVENT_CAST_SPELL, instance->internal_card_id);
          if ((normal_available_or_choice == 0) ||
              (normal_available_or_choice =
                   do_dialog(player, player, card, -1, -1,
                             " Pay Spectacle cost\n Play the spell normally\n Cancel", 0),
               normal_available_or_choice == 0))
          {
            charge_mana_multi(player, cost_or_available, black, blue, green, red, white);
            if (spell_fizzled == 1)
            {
              return 0;
            }
            instance->info_slot = instance->info_slot | 2;
            return 2;
          }
          if (normal_available_or_choice != 1)
          {
            spell_fizzled = 1;
            return 0;
          }
        }
        charge_mana_from_id(player, card, EVENT_CAST_SPELL, get_id(player, card));
        return 1;
      }
    }
  }
  return 0;
}
#endif

#ifndef SHANDALAR
static int spectacle_paid(int player, int card)
{
  return get_card_instance(player, card)->info_slot & 2;
}
#endif

/* ManalinkEh 2022: 0x25bc3c0; original cards/ravnica_allegiance.c:154. */
int card_aeromunculus(int player, int card, event_t event)
{
  /* Aeromunculus	|1|G|U
   * Creature - Homunculus Mutant 2/3
   * Flying
   * |2|G|U: Adapt 1. (If this creature has no +1/+1 counters on it, put a +1/+1 counter on it.) */

  int result;

  result = adapt(player, card, event, 1, 2, 0, 1, 1, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x25bc420; original cards/ravnica_allegiance.c:198. */
int card_archway_angel(int player, int card, event_t event)
{
  /* Archway Angel	|5|W
   * Creature - Angel 3/4
   * Flying
   * When this creature enters, you gain 2 life for each Gate you control. */

  int result;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  result = count_subtype(player, -1, 0x500c);
  gain_life(player, result * 2);
  return 0;
}

/* ManalinkEh 2022: 0x25bc490; original cards/ravnica_allegiance.c:241. */
int card_azorius_knight_arbiter(int player, int card, event_t event)
{
  /* Azorius Knight-Arbiter	|3|W|U
   * Creature - Human Knight 2/5
   * Vigilance
   * This creature can't be blocked. */

  vigilance(player, card, event);
  unblockable(player, card, event);
  return 0;
}

/* ManalinkEh 2022: 0x25bc4d0; original cards/ravnica_allegiance.c:278. */
int card_basilica_bell_haunt(int player, int card, event_t event)
{
  /* Basilica Bell-Haunt	|W|W|B|B
   * Creature - Spirit 3/4
   * When this creature enters, each opponent discards a card and you gain 3 life. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  discard(1 - player, 0, player);
  gain_life(player, 3);
  return 0;
}

/* ManalinkEh 2022: 0x25bc550; original cards/ravnica_allegiance.c:316. */
int card_bedevil(int player, int card, event_t event)
{
  /* Bedevil	|B|B|R
   * Instant
   * Destroy target artifact, creature, or planeswalker. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x142);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td,
                         "Select target artifact, creature or planeswalker.", 1, NULL);
}

/* ManalinkEh 2022: 0x25bc6a0; original cards/ravnica_allegiance.c:366. */
int card_biomancers_familiar(int player, int card, event_t event)
{
  /* Biomancer's Familiar	|G|U
   * Creature - Mutant 2/2
   * Activated abilities of creatures you control cost |2 less to activate. This effect can't reduce the mana in that cost to less than one mana.
   * |T: The next time target creature adapts this turn, it adapts as though it had no +1/+1 counters on it. */

  target_definition_t td;
  int result;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 2);
    set_cost_mod_for_activated_abilities(player, -1, 2, &this_test);
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((affected_card_controller == player) &&
        (result = is_what(player, affected_card, 2), result != 0))
    {
      set_cost_mod_for_activated_abilities(affected_card_controller, affected_card, 2, NULL);
    }
  }
  leaves_play(player, card, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    test_definition_t this_test2;
    default_test_definition(&this_test2, 2);
    remove_cost_mod_for_activated_abilities(player, -1, 2, &this_test2);
    return 0;
  }
  if ((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE) &&
      (event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = instance->targets[0].card;
    instance = get_card_instance(player, card);
    create_targetted_legacy_effect(player, card, biomancers_familiar_legacy,
                                   instance->targets[0].player, result);
  }
  result = generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0,
                                     &td, "TARGET_CREATURE");
  return result;
}

/* ManalinkEh 2022: 0x25bc930; original cards/ravnica_allegiance.c:460. */
int card_captive_audience(int player, int card, event_t event)
{
  /* Captive Audience	|5|B|R
   * Enchantment
   * This enchantment enters under the control of an opponent of your choice.
   * At the beginning of your upkeep, choose one that hasn't been chosen -
   * * Your life total becomes 4.
   * * Discard your hand.
   * * Each opponent creates five 2/2 black Zombie creature tokens. */

  card_instance_t *instance;
  int choice;
  unsigned int life_available;
  unsigned int zombies_available;
  unsigned int discard_available;
  token_generation_t token;

  instance = get_card_instance(player, card);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (instance->info_slot < 7)
  {
    upkeep_trigger_ability(player, card, event, player);
  }
  if (event == EVENT_ETB)
  {
    give_control_of_self(player, card);
    instance->info_slot = 0;
    cant_be_responded_to = 1;
  }
  else if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {

    life_available = instance->info_slot;
    if (life_available == 0xffffffff)
    {
      instance->info_slot = 0;
      life_available = 1;
      discard_available = 1;
      zombies_available = 1;
    }
    else
    {
      zombies_available = (unsigned int)((life_available & 4) == 0);
      discard_available = (unsigned int)((life_available & 2) == 0);
      life_available = (unsigned int)((life_available & 1) == 0);
    }
    choice =
        dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
                    "Your life total becomes 4", life_available, 0xfffffff6, "Discard your hand",
                    discard_available, 0xfffffffb, "Opponent creates five Zombies",
                    zombies_available, 0xfffffff1, 0, 0, 0, 0);
    if (choice == 1)
    {
      set_life_total(player, 4);
      instance->info_slot = instance->info_slot | 1;
    }
    else if (choice == 2)
    {
      new_discard_all(player, player);
      instance->info_slot = instance->info_slot | 2;
    }
    else if (choice == 3)
    {
      default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
      token.t_player = 1 - player;
      token.qty = '\x05';
      generate_token(&token);
      instance->info_slot = instance->info_slot | 4;
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25bcb70; original cards/ravnica_allegiance.c:546. */
int card_cavalcade_of_calamity(int player, int card, event_t event)
{
  /* Cavalcade of Calamity	|1|R
   * Enchantment
   * Whenever a creature you control with power 1 or less attacks, this enchantment deals 1 damage to the player or planeswalker that creature is attacking. */

  if (event == EVENT_DECLARE_ATTACKERS)
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_CREATURE);
    test.power = 2;
    test.power_flag = F5_POWER_LESSER_THAN_VALUE;
    store_attackers(player, card, event, DAT_TRACK, player, -1, &test);
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  }
  else
  {
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      card_instance_t *instance = get_card_instance(player, card);
      const uint8_t *attackers = (const uint8_t *)&instance->targets + 2 * sizeof(target_t);
      for (int index = instance->info_slot - 1; index >= 0; --index)
      {
        int target_card = planeswalker_its_attacking(player, attackers[index]);
        if (target_card != -2)
          damage_creature(1 - current_turn, target_card, 1, player, card);
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25bcce0; original cards/ravnica_allegiance.c:594. */
int card_cindervines(int player, int card, event_t event)
{
  /* Cindervines	|R|G
   * Enchantment
   * Whenever an opponent casts a noncreature spell, this enchantment deals 1 damage to that player.
   * |1, Sacrifice this enchantment: Destroy target artifact or enchantment. This enchantment deals 2 damage to that permanent's controller. */

  card_instance_t *instance;
  int result;


  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 2);
    this_test.type_flag = DOESNT_MATCH;
    sp_ability_trigger(1 - player, event, 2, TYPE_NONE, &this_test, player, card);
  }
  if (event == EVENT_SPA)
  {
    damage_player(trigger_cause_controller, 1, player, card);
  }
  else if (((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
           ((event == EVENT_RESOLVE_ACTIVATION || (event == EVENT_CLEANUP))))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x44);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        damage_player(instance->targets[0].player, 2, player, card);
      }
      ai_modifier = ai_modifier + -0xc;
    }
    result = generic_activated_ability(player, card, event, 0xc, 1, 0, 0, 0, 0, 0, 0,
                                       &td, "DISENCHANT");
    return result;
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25bcf00; original cards/ravnica_allegiance.c:717. */
int card_consecrate_consume(int player, int card, event_t event)
{
  /* Consecrate // Consume	|1|WB // |2|W|B
   * Instant // Sorcery
   * Consecrate:
   * Exile target card from a graveyard.
   * Draw a card.
   *
   * Consume:
   * Target player sacrifices a creature with the greatest power among creatures they control. You gain life equal to its power. */

  int power;
  card_instance_t *instance;
  unsigned int selected_halves;
  int result_or_scan;
  int highest_power_or_power;
  card_instance_t *candidate;
  int32_t player_00;
  int card_00;
  int highest_ties;
  target_definition_t td_player;
  target_definition_t td;
  test_definition_t test;
  test_definition_t this_test;

  new_default_test_definition(&test, 0xff, "Select target card to exile.");
  default_target_definition(player, card, &td_player, 0);
  td_player.zone = TARGET_ZONE_PLAYERS;
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    return generic_spell(player, card, EVENT_CAST_SPELL, 0x1000, NULL, NULL, 1, &test);
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    new_pick_target(&td_player, "TARGET_PLAYER", 0, 0);
    return 0;
  }
  if (event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET)
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      selected_halves = instance->info_slot;
      if ((selected_halves & 1) != 0)
      {

        result_or_scan =
            validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
        if (result_or_scan != -1)
        {
          rfg_card_from_grave(instance->targets[0].player, result_or_scan);
          draw_cards(player, 1);
        }
        selected_halves = instance->info_slot;
      }
      if (((selected_halves & 2) != 0) &&
          (result_or_scan = can_sacrifice(player, instance->targets[0].player, 1, TYPE_CREATURE,
                                          SUBTYPE_INVALID),
           result_or_scan != 0))
      {

        default_test_definition(&this_test, 2);
        highest_power_or_power = check_battlefield_for_special_card(
            player, card, instance->targets[0].player, 0x80, &this_test);
        player_00 = instance->targets[0].player;
        highest_ties = 0;
        result_or_scan = active_cards_count[player_00];
        if (0 < result_or_scan)
        {
          card_00 = 0;
          do
          {
            candidate = in_play(player_00, card_00);
            if ((candidate != NULL) &&
                (result_or_scan = is_what(instance->targets[0].player, card_00, 2),
                 result_or_scan != 0))
            {
              power = get_power(instance->targets[0].player, card_00);
              if (power == highest_power_or_power)
              {
                highest_ties = highest_ties + 1;
              }
              else
              {
                state_untargettable(instance->targets[0].player, card_00, 1);
              }
            }
            player_00 = instance->targets[0].player;
            card_00 = card_00 + 1;
            result_or_scan = active_cards_count[player_00];
          } while (card_00 < result_or_scan);
          if (1 < highest_ties)
          {
            default_target_definition(player, card, &td, 2);
            td.who_chooses = instance->targets[0].player;
            td.allow_cancel = 0;
            td.allowed_controller = td.who_chooses;
            if (new_pick_target(&td, "Select a creature to sacrifice.", 0, 0x80) != 0)
            {
              add_state(instance->targets[0].player, instance->targets[0].card, 0x200000);
            }
            player_00 = instance->targets[0].player;
            result_or_scan = active_cards_count[player_00];
          }
        }
        result_or_scan = result_or_scan + -1;
        if (-1 < result_or_scan)
        {
          while (true)
          {
            candidate = in_play(player_00, result_or_scan);
            if ((candidate != NULL) &&
                (highest_power_or_power = is_what(instance->targets[0].player, result_or_scan, 2),
                 highest_power_or_power != 0))
            {
              highest_power_or_power =
                  check_state(instance->targets[0].player, result_or_scan, 0x100000);
              if (highest_power_or_power == 0)
              {
                if ((highest_ties == 1) ||
                    ((1 < highest_ties &&
                      (highest_power_or_power =
                           check_state(instance->targets[0].player, result_or_scan, 0x200000),
                       highest_power_or_power != 0))))
                {

                  highest_power_or_power = get_power(instance->targets[0].player, result_or_scan);
                  kill_card(instance->targets[0].player, result_or_scan, KILL_SACRIFICE);
                  gain_life(player, highest_power_or_power);
                }
              }
              else
              {
                state_untargettable(instance->targets[0].player, result_or_scan, 0);
              }
            }
            result_or_scan = result_or_scan + -1;
            if (result_or_scan == -1)
              break;
            player_00 = instance->targets[0].player;
          }
        }
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  else if (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET ||
           (event == EVENT_CAST_SPELL && card == affected_card &&
            player == affected_card_controller))
    get_card_instance(player, card);
  result_or_scan = can_sorcery_be_played(player, event);
  selected_halves = 0;
  if (result_or_scan != 0)
  {
    result_or_scan = can_target(&td_player);
    selected_halves = (unsigned int)(result_or_scan != 0);
  }
  return hybrid_split_card(
      player, card, event, 0x4c82,
      generic_spell(player, card, EVENT_CAN_CAST, 0x1000, NULL, NULL, 1, &test), 4, 0x4c85,
      selected_halves, 8);
}

/* ManalinkEh 2022: 0x25bb290; original cards/ravnica_allegiance.c:827. */
static int effect_cry_of_the_carnarium(int player, int card, event_t event)
{
  if (event == EVENT_CAST_SPELL)
  {
    if (is_what(affected_card_controller, affected_card, 2))
    {
      exile_if_would_be_put_into_graveyard(player, card, affected_card_controller, affected_card,
                                           1);
    }
  }
  if (!eot_trigger(player, card, event))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25bd4d0; original cards/ravnica_allegiance.c:834. */
int card_cry_of_the_carnarium(int player, int card, event_t event)
{
  /* Cry of the Carnarium	|1|B|B
   * Sorcery
   * All creatures get -2/-2 until end of turn. Exile all creature cards in all graveyards that were put there from the battlefield this turn. If a creature would die this turn, exile it instead. */

  if (event == EVENT_RESOLVE_SPELL)
  {
    APNAP(p, {
      for (int c = active_cards_count[p] - 1; c >= 0; --c)
      {
        if (in_play(p, c) && is_what(p, c, TYPE_CREATURE))
          exile_if_would_be_put_into_graveyard(player, card, p, c, 1);
      }
    });
    APNAP(p, {
      for (int c = active_cards_count[p] - 1; c >= 0; --c)
      {
        if (in_play(p, c) && is_what(p, c, TYPE_CREATURE))
          pump_until_eot(player, card, p, c, -2, -2);
      }
    });
    seek_grave_to_reanimate_this_turn(player, 2, 2, REANIMATEXTRA_EXILE_FROM_GRAVEYARD);
    create_legacy_effect(player, card, effect_cry_of_the_carnarium);
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}
#endif

/* ManalinkEh 2022: 0x25bd690; original cards/ravnica_allegiance.c:891. */
int card_debtors_transport(int player, int card, event_t event)
{
  /* Debtors' Transport	|5|B
   * Creature - Thrull 5/3
   * Afterlife 2 (When this creature dies, create two 1/1 white and black Spirit creature tokens with flying.) */

  afterlife(player, card, event, 2);
  return 0;
}

/* ManalinkEh 2022: 0x25bd6b0; original cards/ravnica_allegiance.c:926. */
int card_depose_deploy(int player, int card, event_t event)
{
  /* Depose // Deploy	|1|WU // |2|W|U
   * Instant // Instant
   * Depose:
   * Tap target creature.
   * Draw a card.
   *
   * Deploy:
   * Create two 1/1 colorless Thopter artifact creature tokens with flying, then you gain 1 life for each creature you control. */

  card_instance_t *instance;
  unsigned int selected_halves;
  int result;
  card_instance_t *target_record;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    return generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
  }
  if (event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET)
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      selected_halves = instance->info_slot;
      if ((selected_halves & 1) != 0)
      {
        result = valid_target(&td);
        if (result != 0)
        {

          target_record = get_card_instance(player, card);
          if ((player == 1) && (target_record = get_card_instance(target_record->targets[0].player,
                                                                  target_record->targets[0].card),
                                (target_record->state & 0x10) != 0))
          {
            ai_modifier = ai_modifier + -0x30;
          }
          tap_card(instance->targets[0].player, instance->targets[0].card);
          draw_a_card(player);
        }
        selected_halves = instance->info_slot;
      }
      if ((selected_halves & 2) != 0)
      {
        generate_tokens_by_id(player, card, CARD_ID_THOPTER, 2);
        result = count_permanents_by_type(player, TYPE_CREATURE);
        gain_life(player, result);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  else if (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET ||
           (event == EVENT_CAST_SPELL && card == affected_card &&
            player == affected_card_controller))
    get_card_instance(player, card);
  result = can_target(&td);
  return hybrid_split_card(player, card, event, 0x4c8e, result, 3, 0x4c8d, 1, 8);
}

/* ManalinkEh 2022: 0x25bd8e0; original cards/ravnica_allegiance.c:969. */
int card_deputy_of_detention(int player, int card, event_t event)
{
  /* Deputy of Detention	|1|W|U
   * Creature - Vedalken Wizard 1/3
   * When this creature enters, exile target nonland permanent an opponent controls and all other nonland permanents that player controls with the same name as that permanent until this creature leaves the battlefield. */

  int p;
  int c;
  int iid;
  int returned_or_scan_card;
  unsigned int *exile_entry;
  card_instance_t *instance;
  int turn_snapshot_or_scan;
  int chosen_name;
  card_instance_t *candidate;
  int original_iid;
  int owner_or_eligible;
  unsigned int player_00;
  unsigned int packed_exile;
  unsigned int remembered_csvid;
  int leg;
  int idx;
  target_definition_t td;
  int marked[2][151];

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    leaves_play(player, card, event, 1, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {

      td.who_chooses = 0;
      marked[0][0] = 0;
      while (exile_entry =
                 (unsigned int *)exiledby_find_any(player, card, &td.who_chooses, marked[0]),
             exile_entry != NULL)
      {

        packed_exile = *exile_entry;
        *exile_entry = 0xffffffff;
        player_00 = packed_exile >> 0x1f;
        if (check_rfg(player_00, (unsigned int)cards_data[packed_exile & 0x7fffffff].id) != 0)
        {

          returned_or_scan_card = add_card_to_hand(player_00, packed_exile & 0x7fffffff);
          remove_card_from_rfg(player_00, (unsigned int)cards_data[packed_exile & 0x7fffffff].id);
          put_into_play(player_00, returned_or_scan_card);
        }
      }
    }
    return 0;
  }

  default_target_definition(player, card, &td, 0x47);
  p = 1 - player;
  td.illegal_type = TYPE_LAND;
  td.allow_cancel = 0;
  td.allowed_controller = p;
  instance = get_card_instance(player, card);
  turn_snapshot_or_scan = can_target(&td);
  if ((turn_snapshot_or_scan != 0) &&
      (turn_snapshot_or_scan =
           new_pick_target(&td, "Select a nonland permanent an opponent controls.", 0, 0x80),
       turn_snapshot_or_scan != 0))
  {

    chosen_name = get_card_name(instance->targets[0].player, instance->targets[0].card);
    turn_snapshot_or_scan = current_turn;
    memset(marked, 0, sizeof(marked));
    if (chosen_name == -1)
    {
      iid = get_original_internal_card_id(instance->targets[0].player, instance->targets[0].card);
      remembered_csvid = (unsigned int)cards_data[iid].id;
      exile_card_and_remember_it_on_exiledby(player, card, instance->targets[0].player,
                                             instance->targets[0].card);
    }
    else
    {

      leg = 0;
      idx = 0;
      c = active_cards_count[current_turn] + -1;
      if (c < 0)
      {
        remembered_csvid = 0xffffffff;
      }
      else
      {

        remembered_csvid = 0xffffffff;
        do
        {
          if ((((p == turn_snapshot_or_scan) && (candidate = in_play(p, c), candidate != NULL)) &&
               (owner_or_eligible = is_what(p, c, 0x47), owner_or_eligible != 0)) &&
              (owner_or_eligible =
                   has_same_name(instance->targets[0].player, instance->targets[0].card, p, c),
               owner_or_eligible != 0))
          {
            owner_or_eligible = is_token(p, c);
            if (owner_or_eligible == 0)
            {
              owner_or_eligible = get_owner(p, c);
              original_iid = get_original_internal_card_id(p, c);
              remembered_csvid = (unsigned int)cards_data[original_iid].id;
              exiledby_remember(player, card, owner_or_eligible, original_iid, &leg, &idx);
            }
            marked[turn_snapshot_or_scan][c] = 1;
          }
          c = c + -1;
        } while (c != -1);
      }
      turn_snapshot_or_scan = current_turn;
      p = 1 - current_turn;
      c = active_cards_count[p] + -1;
      if (-1 < c)
      {
        do
        {
          if (((player == turn_snapshot_or_scan) &&
               (candidate = in_play(p, c), candidate != NULL)) &&
              ((owner_or_eligible = is_what(p, c, 0x47),
                owner_or_eligible != 0 &&
                    (owner_or_eligible = has_same_name(instance->targets[0].player,
                                                       instance->targets[0].card, p, c),
                     owner_or_eligible != 0))))
          {
            owner_or_eligible = is_token(p, c);
            if (owner_or_eligible == 0)
            {
              owner_or_eligible = get_owner(p, c);
              original_iid = get_original_internal_card_id(p, c);
              remembered_csvid = (unsigned int)cards_data[original_iid].id;
              exiledby_remember(player, card, owner_or_eligible, original_iid, &leg, &idx);
            }
            marked[p][c] = 1;
          }
          c = c + -1;
        } while (c != -1);
      }

      turn_snapshot_or_scan = current_turn;
      returned_or_scan_card = 0x96;
      do
      {
        if (marked[turn_snapshot_or_scan][returned_or_scan_card] != 0)
        {
          kill_card(turn_snapshot_or_scan, returned_or_scan_card, KILL_EXILE);
        }
        returned_or_scan_card = returned_or_scan_card + -1;
      } while (returned_or_scan_card != -1);
      packed_exile = current_turn ^ 1;
      turn_snapshot_or_scan = 0x96;
      do
      {
        if (marked[packed_exile][turn_snapshot_or_scan] != 0)
        {
          kill_card(packed_exile, turn_snapshot_or_scan, KILL_EXILE);
        }
        turn_snapshot_or_scan = turn_snapshot_or_scan + -1;
      } while (turn_snapshot_or_scan != -1);
    }
    if (chosen_name == -1 || (int)remembered_csvid >= 0)
      create_card_name_legacy(player, card, remembered_csvid);
  }
  leaves_play(player, card, EVENT_ETB, 1, 0);
  return 0;
}

/* ManalinkEh 2022: 0x25bb340; original cards/ravnica_allegiance.c:1117. */
int dovin_grand_arbiter_legacy(int player, int card, event_t event)
{
  int target_card;
  int target_player;
  card_instance_t *instance;
  int result;

  instance = combat_damage_being_dealt(event);
  if ((((instance != NULL) && (instance->damage_source_player == player)) &&
       ((instance->targets[3].player & 2) != 0)) &&
      ((instance->damage_target_card == -1 &&
        (result = check_special_flags(player, instance->damage_source_card, 0x40), result == 0))))
  {
    instance = get_card_instance(player, card);
    if (instance->targets[0x10].player < 0)
    {
      instance = get_card_instance(player, card);
      instance->targets[0x10].player = 0;
    }
    instance = get_card_instance(player, card);
    instance->targets[0x10].player = instance->targets[0x10].player + 1;
  }
  if (((trigger_condition == TRIGGER_DEAL_DAMAGE) && (card == affected_card)) &&
      ((player == affected_card_controller && (reason_for_trigger_controller == player))))
  {

    instance = get_card_instance(player, card);
    result = instance->targets[0x10].player;
    if (result < 1)
    {
      return 0;
    }
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      target_card = instance->targets[0].card;
      target_player = instance->targets[0].player;
      add_counters(target_player, target_card, target_player, target_card, COUNTER_LOYALTY, result);
      instance->targets[0x10].player = 0;
    }
  }
  result = eot_trigger(player, card, event);
  if (result == 0)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x25bdea0; original cards/ravnica_allegiance.c:1147. */
int card_dovin_grand_arbiter(int player, int card, event_t event)
{
  /* Dovin, Grand Arbiter	|1|W|U
   * Legendary Planeswalker - Dovin (3)
   * +1: Until end of turn, whenever a creature you control deals combat damage to a player, put a loyalty counter on Dovin.
   * -1: Create a 1/1 colorless Thopter artifact creature token with flying. You gain 1 life.
   * -7: Look at the top ten cards of your library. Put three of them into your hand and the rest on the bottom of your library in a random order. */

  enum
  {
    CHOICE_COUNTERS = 1,
    CHOICE_THOPTER = 2,
    CHOICE_DIG = 3
  };

  int loyalty_or_mode_or_reveal_count;
  card_instance_t *instance;
  int loyalty_or_picks;
  int priority_or_remaining;
  bool activation_event;
  int picks_remaining;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  activation_event = event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE;
  if ((event == EVENT_RESOLVE_ACTIVATION) || (activation_event))
  {

    instance = get_card_instance(player, card);
    loyalty_or_mode_or_reveal_count = count_counters(player, card, COUNTER_LOYALTY);
    priority_or_remaining = ((7 < loyalty_or_mode_or_reveal_count) ? 20U : 1U);
    loyalty_or_mode_or_reveal_count = priority_or_remaining;
    loyalty_or_picks = count_counters(player, card, COUNTER_LOYALTY);
    loyalty_or_mode_or_reveal_count = dialog_impl(
        player, card, event, DLG_RANDOM, DLG_PLANESWALKER, "Add counters for combat damage", 1, 5, 1,
        "Create a thopter & gain life", 1, ((1 < loyalty_or_picks) ? 15U : 1U),
        0xffffffff, "Dig ten cards deep", 1, priority_or_remaining, 0xfffffff9, 0, 0, 0, 0,
        loyalty_or_mode_or_reveal_count);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (loyalty_or_mode_or_reveal_count == 0)
      {
        return 0;
      }
    }
    else if (activation_event)
    {
      instance->number_of_targets = '\0';
    }
    else if (loyalty_or_mode_or_reveal_count == CHOICE_THOPTER)
    {
      generate_token_by_id(player, card, CARD_ID_THOPTER);
      gain_life(player, 1);
    }
    else if (loyalty_or_mode_or_reveal_count == CHOICE_DIG)
    {

      loyalty_or_mode_or_reveal_count = count_deck(player);
      if (loyalty_or_mode_or_reveal_count != 0)
      {
        if (10 < loyalty_or_mode_or_reveal_count)
        {
          loyalty_or_mode_or_reveal_count = 10;
        }

        loyalty_or_picks = loyalty_or_mode_or_reveal_count;
        if (3 < loyalty_or_mode_or_reveal_count)
        {
          loyalty_or_picks = 3;
        }
        do
        {
          picks_remaining = loyalty_or_picks;
          priority_or_remaining = loyalty_or_mode_or_reveal_count;
          new_default_test_definition(&this_test, 0xff, "Select a card to add to your hand");
          this_test.create_minideck = (unsigned short)priority_or_remaining;
          this_test.no_shuffle = 1;
          new_global_tutor(player, player, 1, 1, 1, 1, &this_test);
          loyalty_or_picks = picks_remaining + -1;
          if (loyalty_or_picks == 0)
            break;
          loyalty_or_mode_or_reveal_count = priority_or_remaining + -1;
        } while (priority_or_remaining + -1 != 0);
        priority_or_remaining = priority_or_remaining - picks_remaining;
        if (0 < priority_or_remaining)
        {
          put_top_x_on_bottom_in_random_order(player, priority_or_remaining);
        }
      }
    }
    else if (loyalty_or_mode_or_reveal_count == CHOICE_COUNTERS)
    {
      create_my_legacy(instance->parent_controller, instance->parent_card,
                       dovin_grand_arbiter_legacy);
    }
  }
  return planeswalker(player, card, event, 3);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25be180; original cards/ravnica_allegiance.c:1239. */
int card_drill_bit(int player, int card, event_t event)
{
  /* Drill Bit	|2|B
   * Sorcery
   * Spectacle |B (You may cast this spell for its spectacle cost rather than its mana cost if an opponent lost life this turn.)
   * Target player reveals their hand. You choose a nonland card from it. That player discards that card. */

  card_instance_t *instance;
  ec_definition_t this_definition;
  target_definition_t td;
  test_definition_t this_test;

  spectacle_impl(player, card, event, 0, 1, 0, 0, 0, 0);
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.zone = TARGET_ZONE_PLAYERS;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      default_test_definition(&this_test, 1);
      this_test.type_flag = 1;
      default_ec_definition(instance->targets[0].player, player, &this_definition);
      new_effect_coercion(&this_definition, &this_test);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}
#endif

/* ManalinkEh 2022: 0x25be390; original cards/ravnica_allegiance.c:1274. */
int card_electrodominance(int player, int card, event_t event)
{
  /* Electrodominance	|X|R|R
   * Instant
   * ~ deals X damage to any target. You may cast a spell with mana value X or less from your hand without paying its mana cost. */

  card_instance_t *instance;
  int result;
  char buffer[75];
  target_definition_t td;
  test_definition_t test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      damage_target0(player, card, instance->info_slot);
      scnprintf(buffer, 0x4b, " Choose a card with converted mana cost %d or less to cast.",
                instance->info_slot);
      new_default_test_definition(&test, 1, buffer);
      test.type_flag = 1;
      test.cmc = instance->info_slot + 1;
      test.cmc_flag = 3;
      test.zone = 0x100;
      if (((0 < hand_count[player]) &&
           (result = check_battlefield_for_special_card(player, card, player, 0, &test),
            result != 0)) &&
          (result = new_select_a_card(player, player, 4, 0, 1, -1, &test), -1 < result))
      {
        play_card_in_hand_for_free(player, result);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 5, &td, "TARGET_ANY", 1, NULL);
}

/* ManalinkEh 2022: 0x25be5f0; original cards/ravnica_allegiance.c:1321. */
int card_emergency_powers(int player, int card, event_t event)
{
  /* Emergency Powers	|5|W|U
   * Instant
   * Each player shuffles their hand and graveyard into their library, then draws seven cards. Exile ~.
   * Addendum - If you cast this spell during your main phase, you may put a permanent card with mana value 7 or less from your hand onto the battlefield. */

  int result;
  unsigned int player_00;
  test_definition_t test;
  result = current_turn;
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
      ai_modifier = ai_modifier + (hand_count[1 - player] - hand_count[player]) * 0xc + 0x18;
    }
    else if (event != EVENT_CAN_CHANGE_TARGET)
    {
      if (event == EVENT_RESOLVE_SPELL)
      {
        reshuffle_hand_into_deck(current_turn, 1);
        reshuffle_grave_into_deck(result, 0);
        draw_cards(result, 7);
        player_00 = current_turn ^ 1;
        reshuffle_hand_into_deck(player_00, 1);
        reshuffle_grave_into_deck(player_00, 0);
        draw_cards(player_00, 7);
        if ((current_turn == player) && ((current_phase == 0x14 || (current_phase == 0x1e))))
        {
          new_default_test_definition(
              &test, 0x47, "Select a permanent card with converted mana cost 7 or less.");
          test.cmc = 8;
          test.cmc_flag = 3;
          new_global_tutor(player, player, 4, 4, 0, 2, &test);
        }
        kill_card(player, card, KILL_EXILE);
      }
      else if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
    }
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25be7a0; original cards/ravnica_allegiance.c:1356. */
int card_end_raze_forerunners(int player, int card, event_t event)
{
  /* End-Raze Forerunners	|5|G|G|G
   * Creature - Boar 7/7
   * Vigilance, trample, haste
   * When this creature enters, other creatures you control get +2/+2 and gain vigilance and trample until end of turn. */

  test_definition_t test;
  vigilance(player, card, event);
  haste(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  default_test_definition(&test, 2);
  test.not_me = 1;
  pump_creatures_until_eot(player, card, player, 1, 2, 2, 0x80, 8, &test);
  return 0;
}

/* ManalinkEh 2022: 0x25be870; original cards/ravnica_allegiance.c:1383. */
int card_essence_capture(int player, int card, event_t event)
{
  /* Essence Capture	|U|U
   * Instant
   * Counter target creature spell. Put a +1/+1 counter on up to one target creature you control. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  target_definition_t td1;

  if (event == EVENT_CAN_CAST)
  {
    counterspell_target_definition(player, card, &td, 2);
    default_target_definition(player, card, &td1, 2);
    td1.allowed_controller = player;
    get_card_instance(player, card);
    result = counterspell(player, card, EVENT_CAN_CAST, &td, 0);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      counterspell_target_definition(player, card, &td, 2);
      default_target_definition(player, card, &td1, 2);
      td1.allowed_controller = player;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        counterspell(player, card, EVENT_CAST_SPELL, &td, 0);
        result = can_target(&td1);
        if (result != 0)
        {
          new_pick_target(&td1, "TARGET_CREATURE_YOU_CONTROL", 1, 0);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    counterspell_target_definition(player, card, &td, 2);
    default_target_definition(player, card, &td1, 2);
    td1.allowed_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = counterspell_validate(player, card, &td, 0);
      if (result != 0)
      {
        real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
      }
      result = validate_target(player, card, &td1, 1);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[1].player,
                                 instance->targets[1].card, COUNTER_P1_P1, 1);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    counterspell_target_definition(player, card, &td, 2);
    default_target_definition(player, card, &td1, 2);
    td1.allowed_controller = player;
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25beba0; original cards/ravnica_allegiance.c:1426. */
int card_ethereal_absolution(int player, int card, event_t event)
{
  /* Ethereal Absolution	|4|W|B
   * Enchantment
   * Creatures you control get +1/+1.
   * Creatures your opponents control get -1/-1.
   * |2|W|B: Exile target card from an opponent's graveyard. If it was a creature card, you create a 1/1 white and black Spirit creature token with flying. */

  int grave_target;
  int was_creature;
  int whose_graveyard;
  token_generation_t token;
  test_definition_t test;
  boost_creature_type(player, card, event, ~SUBTYPE_INVALID, 1, 1, 0, BCT_CONTROLLER_ONLY);
  boost_creature_type(player, card, event, ~SUBTYPE_INVALID, -1, -1, 0, BCT_OPPONENT_ONLY);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_test_definition(&test, 0xff);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE,
                                                            GAA_NONE, 2, 1, 0, 0, 0, 1, 0, NULL,
                                                            NULL, TYPE_ANY, &test, 0x800);
    }
    if (event == EVENT_ACTIVATE)
    {
      if (player != 1)
      {
        return generic_activated_ability_with_target_in_grave(player, card, EVENT_ACTIVATE,
                                                              GAA_NONE, 2, 1, 0, 0, 0, 1, 0, NULL,
                                                              NULL, TYPE_ANY, &test, 0x800);
      }
      generic_activated_ability_with_target_in_grave(1, card, EVENT_ACTIVATE, GAA_NONE, 2, 1, 0, 0,
                                                     0, 1, 0, NULL, NULL, TYPE_CREATURE, NULL,
                                                     0x800);
      if (spell_fizzled != 1)
      {
        return 0;
      }
      spell_fizzled = 0;
      return generic_activated_ability_with_target_in_grave(1, card, EVENT_ACTIVATE, GAA_NONE, 2, 1,
                                                            0, 0, 0, 1, 0, NULL, NULL, TYPE_ANY,
                                                            &test, 0x800);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return global_enchantment(player, card, event);
    }
    default_test_definition(&test, 0xff);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    whose_graveyard = 1 - player;
    grave_target = validate_target_from_grave_source(player, card, whose_graveyard, 1);
    if (grave_target != -1)
    {
      was_creature = is_what(-1, graveyard_ptr_mutable[whose_graveyard][grave_target], 2);
      rfg_card_from_grave(whose_graveyard, grave_target);
      if (was_creature != 0)
      {
        default_token_definition(player, card, CARD_ID_SPIRIT, &token);
        token.color_forced = '\"';
        token.key_plus = 0x20;
        generate_token(&token);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25befb0; original cards/ravnica_allegiance.c:1541. */
int card_footlight_fiend(int player, int card, event_t event)
{
  /* Footlight Fiend	|BR
   * Creature - Devil 1/1
   * When this creature dies, it deals 1 damage to any target. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_ANY");
      if (result != 0)
      {
        damage_target_creature_or_player(instance->targets[0].player, instance->targets[0].card, 1,
                                         player, card);
      }
    }
  }
  result = hybrid(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25bf090; original cards/ravnica_allegiance.c:1579. */
int card_frilled_mystic(int player, int card, event_t event)
{
  /* Frilled Mystic	|G|G|U|U
   * Creature - Elf Lizard Wizard 3/2
   * Flash
   * When this creature enters, you may counter target spell. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    if (card_counterspell(player, card, event))
    {
      instance->info_slot = 1;
      return 99;
    }
    instance->info_slot = 0;
    return 1;
  }

  if (event == EVENT_CAST_SPELL)
  {
    if (affect_me(player, card) && current_turn == player)
      ai_modifier -= 10;
  }
  else if (event == EVENT_ETB)
  {
    const char *spell_name = get_card_name_by_id(get_id(card_on_stack_controller, card_on_stack));
    char buffer[100];
    scnprintf(buffer, sizeof buffer, " Counter %s\n Pass", spell_name);
    if (instance->info_slot == 1 &&
        do_dialog(player, player, card, card_on_stack_controller, card_on_stack, buffer,
                   card_on_stack_controller == player) == 0)
      real_counter_a_spell(player, card, card_on_stack_controller, card_on_stack);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25bf230; original cards/ravnica_allegiance.c:1623. */
int card_gate_colossus(int player, int card, event_t event)
{
  /* Gate Colossus	|8
   * Artifact Creature - Construct 8/8
   * Affinity for Gates (This spell costs |1 less to cast for each Gate you control.)
   * This creature can't be blocked by creatures with power 2 or less.
   * Whenever a Gate you control enters, you may put this card from your graveyard on top of your library. */

  int result;

  if (event != EVENT_MODIFY_COST)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((((event == EVENT_BLOCK_LEGALITY) && (result == 0)) &&
         (attacking_card_controller == player)) &&
        (attacking_card == card))
    {
      result = get_power(affected_card_controller, affected_card);
      if (result < 3)
      {
        event_result = 1;
      }
    }
    return 0;
  }
  result = count_subtype(player, -1, 0x500c);
  COST_COLORLESS = COST_COLORLESS - (char)result;
  is_humiliated(player, card, AL_STATIC);
  return 0;
}

/* ManalinkEh 2022: 0x25bf300; original cards/ravnica_allegiance.c:1645. */
int card_gatebreaker_ram(int player, int card, event_t event)
{
  /* Gatebreaker Ram	|2|G
   * Creature - Sheep 2/2
   * This creature gets +1/+1 for each Gate you control.
   * As long as you control two or more Gates, this creature has vigilance and trample. */

  int result;

  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = count_subtype(player, 0x47, 0x500c);
        event_result = event_result + result;
      }
    }
  }
  else if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      result = count_subtype(player, 0x47, 0x500c);
      if (1 < result)
      {
        event_result = event_result | 0x80;
        vigilance(player, card, EVENT_ABILITIES);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25bf410; original cards/ravnica_allegiance.c:1662. */
int card_gates_ablaze(int player, int card, event_t event)
{
  /* Gates Ablaze	|2|R
   * Sorcery
   * ~ deals X damage to each creature, where X is the number of Gates you control. */

  int result;
  test_definition_t test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&test, 2);
    result = count_subtype(player, 0x47, 0x500c);
    new_damage_all(player, card, 2, result, 1, &test);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25bf4c0; original cards/ravnica_allegiance.c:1694. */
int card_ghor_clan_wrecker(int player, int card, event_t event)
{
  /* Ghor-Clan Wrecker	|3|R
   * Creature - Human Warrior 2/2
   * Riot (This creature enters with your choice of a +1/+1 counter or haste.)
   * Menace (This creature can't be blocked except by two or more creatures.) */

  menace(player, card, event);
  riot(player, card, event);
  return 0;
}

/* ManalinkEh 2022: 0x25bf500; original cards/ravnica_allegiance.c:1744. */
int card_growth_spiral(int player, int card, event_t event)
{
  /* Growth Spiral	|G|U
   * Instant
   * Draw a card. You may put a land card from your hand onto the battlefield. */

  test_definition_t test;
  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    draw_a_card(player);
    new_default_test_definition(&test, 1, "Select a land card.");
    new_global_tutor(player, player, 4, 4, 0, 1, &test);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x25bf5f0; original cards/ravnica_allegiance.c:1764. */
int card_growth_chamber_guardian(int player, int card, event_t event)
{
  /* Growth-Chamber Guardian	|1|G
   * Creature - Elf Crab Warrior 2/2
   * |2|G: Adapt 2. (If this creature has no +1/+1 counters on it, put two +1/+1 counters on it.)
   * Whenever one or more +1/+1 counters are put on this creature, you may search your library for a card named ~, reveal it, put it into your hand, then shuffle. */

  int result;
  test_definition_t this_test;

  if (((event == EVENT_COUNTERS) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&this_test, 2);
    this_test.id = get_card_name(player, card);
    result = get_card_name(player, card);
    scnprintf(this_test.message, 100, "Select a card named %s.", cards_ptr[result]->name);
    new_global_tutor(player, player, 1, 1, 0, 5, &this_test);
  }
  result = adapt(player, card, event, 2, 2, 0, 0, 1, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x25bf710; original cards/ravnica_allegiance.c:1798. */
int card_gruul_spellbreaker(int player, int card, event_t event)
{
  /* Gruul Spellbreaker	|1|R|G
   * Creature - Ogre Warrior 3/3
   * Riot (This creature enters with your choice of a +1/+1 counter or haste.)
   * Trample
   * During your turn, you and this creature have hexproof. */

  riot(player, card, event);
  if (current_turn != player)
  {
    return 0;
  }
  hexproof(player, card, event);
  player_bits[player] =
      player_bits[player] | 0x20000;
  return 0;
}

/* ManalinkEh 2022: 0x25bb4e0; original cards/ravnica_allegiance.c:1815. */
static int unique_name(int unused1, intptr_t unused2, int player, int card)
{
  int result;
  result = get_card_name(player, card);
  result = count_permanents_by_name(player, TYPE_CREATURE, result);
  return (unsigned int)(result == 1);
}

/* ManalinkEh 2022: 0x25bf780; original cards/ravnica_allegiance.c:1820. */
int card_guardian_project(int player, int card, event_t event)
{
  /* Guardian Project	|3|G
   * Enchantment
   * Whenever a nontoken creature you control enters, if it doesn't have the same name as another creature you control or a creature card in your graveyard, draw a card. */

  int result;
  test_definition_t test;
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 2);
    test.type_flag = 3;
    test.special_selection_function = unique_name;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    draw_a_card(player);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25bf840; original cards/ravnica_allegiance.c:1841. */
int card_gutterbones(int player, int card, event_t event)
{
  /* Gutterbones	|B
   * Creature - Skeleton Warrior 2/1
   * This creature enters tapped.
   * |1|B: Return this card from your graveyard to your hand. Activate only during your turn and only if an opponent lost life this turn. */

  int result;
  comes_into_play_tapped(player, card, event);
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = has_mana_multi(player, 1, 1, 0, 0, 0, 0);
    if (((result != 0) && (current_turn == player)) &&
        (result = get_trap_condition(1 - player, 2), 0 < result))
    {
      return 4;
    }
  }
  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    charge_mana_multi(player, 1, 1, 0, 0, 0, 0);
    return (unsigned int)(spell_fizzled != 1);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25bf950; original cards/ravnica_allegiance.c:1867. */
int card_gyre_engineer(int player, int card, event_t event)
{
  /* Gyre Engineer	|1|G|U
   * Creature - Vedalken Wizard 1/1
   * |T: Add |G|U. */

  int result;
  result = mana_producing_creature_multi(player, card, event, 0, 0, 0, 1, 1, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x25bf9b0; original cards/ravnica_allegiance.c:1893. */
int card_hero_of_precinct_one(int player, int card, event_t event)
{
  /* Hero of Precinct One	|1|W
   * Creature - Human Warrior 2/2
   * Whenever you cast a multicolored spell, create a 1/1 white Human creature token. */

  test_definition_t test;

  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 0xff);
    test.color_flag = 2;
    sp_ability_trigger(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event != EVENT_SPA)
  {
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_HUMAN);
  return 0;
}

/* ManalinkEh 2022: 0x25bfa70; original cards/ravnica_allegiance.c:1914. */
int card_high_alert(int player, int card, event_t event)
{
  /* High Alert	|1|W|U
   * Enchantment
   * Each creature you control assigns combat damage equal to its toughness rather than its power.
   * Creatures you control can attack as though they didn't have defender.
   * |2|W|U: Untap target creature. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event == EVENT_ABILITIES)
  {
    if (affected_card_controller == player)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        instance = get_card_instance(affected_card_controller, affected_card);
        instance->token_status = instance->token_status | 0x800;
      }
    }
  }
  else if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
            (event == EVENT_RESOLVE_ACTIVATION)) ||
           (event == EVENT_CLEANUP))
  {
    default_target_definition(player, card, &td, 2);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        instance = get_card_instance(player, card);
        result = instance->targets[0].card;
        instance = get_card_instance(player, card);
        untap_card(instance->targets[0].player, result);
      }
    }
    result = generic_activated_ability(player, card, event, 4, 2, 0, 1, 0, 0, 1, 0, &td,
                                       "TARGET_CREATURE");
    return result;
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25bfc00; original cards/ravnica_allegiance.c:1947. */
int card_hydroid_krasis(int player, int card, event_t event)
{
  /* Hydroid Krasis	|X|G|U
   * Creature - Jellyfish Hydra Beast 0/0
   * When you cast this spell, you gain half X life and draw half X cards. Round down each time.
   * Flying, trample
   * This creature enters with X +1/+1 counters on it. */

  card_instance_t *instance;
  int result;
  unsigned int half_x;

  if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 0, NULL);
    enters_the_battlefield_with_counters(player, card, EVENT_CAST_SPELL, COUNTER_P1_P1,
                                         instance->info_slot);
    result = is_token(player, card);
    if (result == 0)
    {
      result = check_special_flags(player, card, 0x40000);
      if (result == 0)
      {
        half_x = round_down_value(instance->info_slot);
        gain_life(player, half_x);
        half_x = round_down_value(instance->info_slot);
        draw_cards(player, half_x);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25bfd20; original cards/ravnica_allegiance.c:1996. */
int card_imperious_oligarch(int player, int card, event_t event)
{
  /* Imperious Oligarch	|W|B
   * Creature - Human Cleric 2/1
   * Vigilance
   * Afterlife 1 (When this creature dies, create a 1/1 white and black Spirit creature token with flying.) */

  vigilance(player, card, event);
  afterlife(player, card, event, 1);
  return 0;
}

/* ManalinkEh 2022: 0x25bfd60; original cards/ravnica_allegiance.c:2023. */
int card_incubation_incongruity(int player, int card, event_t event)
{
  /* Incubation // Incongruity	|GU // |1|G|U
   * Sorcery // Instant
   * Incubation:
   * Look at the top five cards of your library. You may reveal a creature card from among them and put it into your hand. Put the rest on the bottom of your library in a random order.
   *
   * Incongruity:
   * Exile target creature. That creature's controller creates a 3/3 green Frog Lizard creature token. */

  card_instance_t *instance;
  unsigned int selected_halves;
  int reveal_count_or_available;
  int revealed_card;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
  }
  if (event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET)
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      selected_halves = instance->info_slot;
      if ((selected_halves & 1) != 0)
      {
        reveal_count_or_available = count_deck(player);
        if (reveal_count_or_available != 0)
        {
          test_definition_t this_test;
          if (5 < reveal_count_or_available)
          {
            reveal_count_or_available = 5;
          }
          new_default_test_definition(&this_test, 2, "Select a creature card.");
          this_test.create_minideck = (unsigned short)reveal_count_or_available;
          this_test.no_shuffle = 1;
          revealed_card = new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
          if ((ai_is_speculating == 1 || (player == 1 && !((unsigned char)trace_mode & 2))) &&
              revealed_card < 0)
          {
            this_test.type = 0xff;
            revealed_card = new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
          }
          if (revealed_card >= 0)
          {
            reveal_card(player, card, player, revealed_card);
            --reveal_count_or_available;
          }
          if (0 < reveal_count_or_available)
          {
            put_top_x_on_bottom_in_random_order(player, reveal_count_or_available);
          }
        }
        selected_halves = instance->info_slot;
      }
      if (((selected_halves & 2) != 0) &&
          (reveal_count_or_available = valid_target(&td), reveal_count_or_available != 0))
      {
        token_generation_t token;
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
        default_token_definition(player, card, CARD_ID_FROG_LIZARD, &token);
        token.t_player = (int32_t)(instance->targets[0].player);
        token.pow = 3;
        token.tou = 3;
        generate_token(&token);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  else if (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET ||
           (event == EVENT_CAST_SPELL && card == affected_card &&
            player == affected_card_controller))
    get_card_instance(player, card);
  reveal_count_or_available =
      generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
  return hybrid_split_card(player, card, event, 0x4ccd, can_sorcery_be_played(player, event), 5,
                           0x4ccc, reveal_count_or_available, 10);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c0110; original cards/ravnica_allegiance.c:2084. */
int card_incubation_druid(int player, int card, event_t event)
{
  /* Incubation Druid	|1|G
   * Creature - Elf Druid 0/2
   * |T: Add one mana of any type that a land you control could produce. If this creature has a +1/+1 counter on it, add three mana of that type instead.
   * |3|G|G: Adapt 3. (If this creature has no +1/+1 counters on it, put three +1/+1 counters on it.) */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ADAPT = 2
  };

  int choice;
  int mana_amount_or_result;
  color_test_t available;
  int paying_or_available;
  bool produce_mana = false;
  card_instance_t *instance;

  available = get_color_of_mana_produced_by_id(get_id(player, card), 0x3e, player);
  mana_amount_or_result = count_counters(player, card, COUNTER_P1_P1);
  mana_amount_or_result = ((mana_amount_or_result == 0) ? 1U : 3U);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (mana_producing_creature_all_one_color(player, card, EVENT_CAN_ACTIVATE, 0x18, available,
                                              mana_amount_or_result) == 0)
    {
      return adapt(player, card, EVENT_CAN_ACTIVATE, 3, 3, 0, 0, 2, 0, 0);
    }
    return 1;
  }
  if (event != EVENT_ACTIVATE)
  {
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      if (((unsigned char *)&instance->info_slot)[0] == '\x02')
      {
        instance = get_card_instance(instance->parent_controller, instance->parent_card);
        instance->info_slot = 0;
        return adapt(player, card, EVENT_RESOLVE_ACTIVATION, 3, 3, 0, 0, 2, 0, 0);
      }
    }
    else if (event != EVENT_CLEANUP)
    {
      return mana_producing_creature_all_one_color(player, card, event, 0x18, available,
                                                   mana_amount_or_result);
    }
    return 0;
  }
  paying_or_available = paying_mana();
  if ((paying_or_available == 0) &&
      (paying_or_available = adapt(player, card, EVENT_CAN_ACTIVATE, 3, 3, 0, 0, 2, 0, 0),
       paying_or_available != 0))
  {
    if (player == 1)
    {
      instance = get_card_instance(1, card);
      ((unsigned char *)&instance->info_slot)[0] = 2;
    }
    else
    {
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, "Get mana", 1, 1,
                           "Adapt", 1, 1, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      instance = get_card_instance(player, card);
      ((unsigned char *)&instance->info_slot)[0] = (char)choice;
      if (choice == CHOICE_MANA)
        produce_mana = true;
      if (choice != CHOICE_MANA && choice != CHOICE_ADAPT)
      {
        return 0;
      }
    }
    if (!produce_mana)
    {
      ((unsigned char *)&instance->info_slot)[1] = 1;
      mana_amount_or_result = adapt(player, card, EVENT_ACTIVATE, 3, 3, 0, 0, 2, 0, 0);
      ((unsigned char *)&instance->info_slot)[1] = 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    ((unsigned char *)&instance->info_slot)[0] = 1;
    produce_mana = true;
  }
  if (produce_mana)
  {
    if (((unsigned char *)&instance->info_slot)[1] == '\x01')
    {
      ((unsigned char *)&instance->info_slot)[0] = 2;
    }
    mana_amount_or_result = mana_producing_creature_all_one_color(
        player, card, EVENT_ACTIVATE, 0x18, available, mana_amount_or_result);
  }

  return mana_amount_or_result;
}
#endif

/* ManalinkEh 2022: 0x25bb520; original cards/ravnica_allegiance.c:2153. */
static int judith_the_scourge_diva_effect(int player, int card, event_t event)
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
           result != 0x4cd0))
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

      if ((dying != NULL) && (dying = get_card_instance(affected_card_controller, affected_card),
                              dying->kill_code != '\0'))
      {
        test_definition_t test;

        if ((-1 < instance->damage_target_player) &&
            ((instance->damage_target_card == affected_card &&
              (instance->damage_target_player == affected_card_controller))))
        {
          instance->damage_target_card = -1;
          instance->damage_target_player = -1;
          remove_status(player, card, 0x10000);
          if (instance->targets[0xb].player < 0)
          {
            instance->targets[0xb].player = 0;
          }
          SET_BYTE1(instance->targets[0xb].player) = 1;
        }
        default_test_definition(&test, 2);
        test.type_flag = 3;
        count_for_gfp_ability_and_store_values(player, card, 0x77, player, 0, &test, 0x40, 0);
      }
    }
  }
  pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_deaths != 0)
  {
    result = check_status(player, card, 0x10000);
    if (result == 0)
    {
      target_definition_t td;
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td.allow_cancel = 0;

      for (pending_deaths = pending_deaths & 0xff; pending_deaths != 0;
           pending_deaths = pending_deaths - 1)
      {
        result = can_target(&td);
        if ((result != 0) && (result = pick_target(&td, "TARGET_ANY"), result != 0))
        {
          damage_target0(player, card, 1);
        }
        instance = get_card_instance(player, card);
        instance->number_of_targets = '\0';
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) && (result = check_status(player, card, 0x10000), result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c04f0; original cards/ravnica_allegiance.c:2217. */
int card_judith_the_scourge_diva(int player, int card, event_t event)
{
  /* Judith, the Scourge Diva	|1|B|R
   * Legendary Creature - Human Shaman 2/2
   * Other creatures you control get +1/+0.
   * Whenever a nontoken creature you control dies, Judith deals 1 damage to any target. */

  int c;
  int created_or_valid;
  card_instance_t *instance;
  unsigned int pending_deaths;
  target_definition_t td;

  bool legacy_found = false;
  if (((event == EVENT_STATIC_EFFECTS) && (card == affected_card)) &&
      (player == affected_card_controller))
  {

    if (0 < active_cards_count[player])
    {
      c = 0;
      do
      {
        instance = in_play(player, c);

        if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
             (instance = get_card_instance(player, c),
              instance->info_slot_fn == judith_the_scourge_diva_effect)) &&
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
      created_or_valid = create_targetted_legacy_effect(
          player, card, judith_the_scourge_diva_effect, player, card);
      instance = get_card_instance(player, created_or_valid);
      instance->targets[0].player = player;
      instance->targets[0].card = card;
      instance->number_of_targets = '\x01';
      add_status(player, created_or_valid, 0x10000);
    }
  }
  boost_creature_type(player, card, event, ~SUBTYPE_INVALID, 1, 0, 0, BCT_CONTROLLER_ONLY);
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  td.allow_cancel = 0;
  count_for_gfp_ability_and_store_values(player, card, event, player, 2, NULL, 0x40, 0);
  if (resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) != 0)
  {

    instance = get_card_instance(player, card);
    for (pending_deaths = (unsigned int)(unsigned char)instance->targets[0xb].card;
         pending_deaths != 0; pending_deaths = pending_deaths - 1)
    {
      created_or_valid = can_target(&td);
      if ((created_or_valid != 0) &&
          (created_or_valid = pick_target(&td, "TARGET_ANY"), created_or_valid != 0))
      {
        damage_target0(player, card, 1);
      }
      instance->number_of_targets = '\0';
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c0740; original cards/ravnica_allegiance.c:2282. */
int card_kayas_wrath(int player, int card, event_t event)
{
  /* Kaya's Wrath	|W|W|B|B
   * Sorcery
   * Destroy all creatures. You gain life equal to the number of creatures you controlled that were destroyed this way. */

  int result;
  int amount;
  unsigned int t_player;
  test_definition_t test;
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
      default_test_definition(&test, 2);
      result = current_turn;
      amount = new_manipulate_all(player, card, current_turn, &test, ACT_KILL_DESTROY);
      if (player != result)
      {
        amount = 0;
      }
      t_player = current_turn ^ 1;
      result = new_manipulate_all(player, card, t_player, &test, ACT_KILL_DESTROY);
      if (player == t_player)
      {
        amount = amount + result;
      }
      gain_life(player, amount);
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

/* ManalinkEh 2022: 0x25c0870; original cards/ravnica_allegiance.c:2331. */
int card_lavinia_azorius_renegade(int player, int card, event_t event)
{
  /* Lavinia, Azorius Renegade	|W|U
   * Legendary Creature - Human Soldier 2/2
   * Each opponent can't cast noncreature spells with mana value greater than the number of lands that player controls.
   * Whenever an opponent casts a spell, if no mana was spent to cast it, counter that spell. */

  int controller_or_cmc;
  int land_count;

  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    controller_or_cmc = affected_card_controller;
    if (affected_card_controller == -1)
    {
      controller_or_cmc = event_result;
    }
    if ((1 - player == controller_or_cmc) &&
        (controller_or_cmc = is_what(affected_card_controller, affected_card, 2),
         controller_or_cmc == 0))
    {
      controller_or_cmc = get_cmc(affected_card_controller, affected_card);
      land_count = count_permanents_by_type(affected_card_controller, TYPE_LAND);
      if ((land_count < controller_or_cmc) &&
          (controller_or_cmc = is_humiliated(player, card, AL_STATIC), controller_or_cmc == 0))
      {
        infinite_casting_cost();
        return 0;
      }
    }
    if (trigger_condition != TRIGGER_SPELL_CAST)
    {
      return 0;
    }
  }

  if (trigger_condition == TRIGGER_SPELL_CAST &&
      ((card == affected_card) && (player == affected_card_controller)) &&
      (controller_or_cmc = played_for_free(trigger_cause_controller, trigger_cause),
       controller_or_cmc != 0))
  {
    sp_ability_trigger(1 - player, event, 2, TYPE_ANY, NULL, player, card);
  }
  if (event != EVENT_SPA)
  {
    return 0;
  }
  real_counter_a_spell(player, card, trigger_cause_controller, trigger_cause);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c0a00; original cards/ravnica_allegiance.c:2366. */
int card_light_up_the_stage(int player, int card, event_t event)
{
  /* Light Up the Stage	|2|R
   * Sorcery
   * Spectacle |R (You may cast this spell for its spectacle cost rather than its mana cost if an opponent lost life this turn.)
   * Exile the top two cards of your library. Until the end of your next turn, you may play those cards. */

  test_definition_t this_test;
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
        spectacle_impl(player, card, event, 0, 0, 0, 0, 1, 0);
        if (event == EVENT_RESOLVE_SPELL)
        {
          default_test_definition(&this_test, 0xff);
          exile_top_x_card_you_may_play_them(player, card, 2, &this_test, player,
                                                  MPCFE_UNTIL_END_OF_YOUR_NEXT_TURN);
          kill_card(player, card, KILL_DESTROY);
        }
        return basic_spell(player, card, event);
      }
      if ((event != EVENT_CHANGE_TARGET) && (event != EVENT_MODIFY_COST))
      {
        return 0;
      }
    }
  }
  spectacle_impl(player, card, event, 0, 0, 0, 0, 1, 0);
  return basic_spell(player, card, event);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c0b30; original cards/ravnica_allegiance.c:2408. */
int card_mass_manipulation(int player, int card, event_t event)
{
  /* Mass Manipulation	|X|X|U|U|U|U
   * Sorcery
   * Gain control of X target creatures and/or planeswalkers. */

  target_definition_t td;
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 4, &td, NULL, 1, NULL);
  }
  if (event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 0x102);
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_CAST_SPELL || event == EVENT_CHANGE_TARGET)
  {
    if (event == EVENT_CHANGE_TARGET && check_special_flags2(player, card, 0x10000))
    {
      int count = (unsigned int)instance->number_of_targets;
      int target_iids[count];
      int shown = 0;
      if (instance->number_of_targets != 0)
      {
        do
        {
          int c = instance->targets[shown].card;
          int p = instance->targets[shown].player;
          ++shown;
          if (c == -1)
            target_iids[shown - 1] =
                get_internal_card_id_from_csv_id(p == player ? 0x615a : 0x6161);
          else
            target_iids[shown - 1] = get_card_instance(p, c)->internal_card_id;
        } while (shown < (unsigned int)instance->number_of_targets);
      }
      return show_deck(player, target_iids, shown,
                       " Select the target to change (Rules Engine = you, Deadbox = opponent)", 0,
                       gs_done_008b40e0);
    }
    bool forced_target = check_special_flags2(player, card, 0x2000) != 0;
    if (!forced_target)
      forced_target = check_special_flags2(player, card, 0x8000) != 0;
    if (event == EVENT_CHANGE_TARGET && forced_target)
    {
      td.allow_cancel = 0;
      td.who_chooses = attacking_card_controller;
    }
    set_special_flags2(player, card, 0x20000);
    if (!played_for_free(player, card) && !is_token(player, card))
    {
      instance->info_slot = 0;
      if (player == 0)
      {
        int paid = charge_mana_for_double_x(0, 7);
        set_x_for_x_spells(0, card, event, paid / 2);
      }
      else
      {
        int cost = target_available(1 - player, card, &td) * 2;
        while (!has_mana(player, 7, cost))
          cost -= 2;
        if (cost == 0)
          ai_modifier -= 0x32;
        charge_mana(player, COLOR_ANY, cost);
        if (spell_fizzled != 1)
          set_x_for_x_spells(player, card, event, cost);
      }
    }
    int targets = instance->info_slot;
    int available = target_available(-1, card, &td);
    if (available < targets)
      targets = available;
    pick_up_to_n_targets(&td, "TARGET_CREATURE_OR_PLANESWALKER", targets);
    if (event == EVENT_CHANGE_TARGET && instance->targets[0].card != -1)
      state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
    return 0;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    for (int i = 0; i < (int)(unsigned int)instance->number_of_targets; ++i)
      if (validate_target(player, card, &td, i))
      {
        card_instance_t *spell = get_card_instance(player, card);
        int target_card = spell->targets[i].card;
        spell = get_card_instance(player, card);
        gain_control_permanently(player, spell->targets[i].player, target_card);
      }
    kill_card(player, card, KILL_DESTROY);
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x25c1150; original cards/ravnica_allegiance.c:2515. */
int card_mirror_march(int player, int card, event_t event)
{
  /* Mirror March	|5|R
   * Enchantment
   * Whenever a nontoken creature you control enters, flip a coin until you lose a flip. For each flip you won, create a token that's a copy of that creature. Those tokens gain haste. Exile them at the beginning of the next end step. */

  int result;
  char copy_count;

  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.type_flag = 3;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    token_generation_t token;
    copy_count = '\0';
    do
    {
      result = flip_a_coin(player, card);
      if (result == 0)
        break;
      copy_count = copy_count + '\x01';
    } while (result == 1);
    copy_token_definition(player, card, &token, trigger_cause_controller, trigger_cause);
    token.legacy = '\x01';
    token.special_code_for_legacy = haste_and_remove_eot;
    token.qty = copy_count;
    generate_token(&token);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25c1280; original cards/ravnica_allegiance.c:2548. */
int card_nikya_of_the_old_ways(int player, int card, event_t event)
{
  /* Nikya of the Old Ways	|3|R|G
   * Legendary Creature - Centaur Druid 5/5
   * You can't cast noncreature spells.
   * Whenever you tap a land for mana, add one mana of any type that land produced. */

  int result;

  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((result == 0) && (affected_card_controller == player))
    {
      result = is_what(player, affected_card, 3);
      if (result == 0)
      {
        infinite_casting_cost();
      }
    }
  }
  result = card_mana_flare(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25c1300; original cards/ravnica_allegiance.c:2579. */
int card_orzhov_enforcer(int player, int card, event_t event)
{
  /* Orzhov Enforcer	|1|B
   * Creature - Human Rogue 1/2
   * Deathtouch
   * Afterlife 1 (When this creature dies, create a 1/1 white and black Spirit creature token with flying.) */

  deathtouch(player, card, event);
  afterlife(player, card, event, 1);
  return 0;
}

/* ManalinkEh 2022: 0x25c1340; original cards/ravnica_allegiance.c:2639. */
int card_plaza_of_harmony(int player, int card, event_t event)
{
  /* Plaza of Harmony	""
   * Land
   * When this land enters, if you control two or more Gates, you gain 3 life.
   * |T: Add |C.
   * |T: Add one mana of any type that a Gate you control could produce. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    if (count_subtype(player, -1, 0x500c) > 1)
      gain_life(player, 3);
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    int id = get_id(player, card);
    unsigned int colors = get_color_of_mana_produced_by_id(id, 0x3e, player);
    play_land_sound_effect_force_color(player, card, colors & 0x80000000U ? 0 : colors);
    return 0;
  }
  return mana_producer(player, card, event);
}

/* ManalinkEh 2022: 0x25c1410; original cards/ravnica_allegiance.c:2665. */
int card_precognitive_perception(int player, int card, event_t event)
{
  /* Precognitive Perception	|3|U|U
   * Instant
   * Draw three cards.
   * Addendum - If you cast this spell during your main phase, instead scry 3, then draw three cards. */

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
      if ((current_turn == player) && ((current_phase == 0x14 || (current_phase == 0x1e))))
      {
        scry(player, 3);
      }
      draw_cards(player, 3);
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x25c14e0; original cards/ravnica_allegiance.c:2684. */
int card_priest_of_forgotten_gods(int player, int card, event_t event)
{
  /* Priest of Forgotten Gods	|1|B
   * Creature - Human Cleric 1/2
   * |T, Sacrifice two other creatures: Any number of target players each lose 2 life and sacrifice a creature of their choice. You add |B|B and draw a card. (Activate only as an instant.) */

  int i;
  target_t *target_cursor;
  card_instance_t *instance;
  int available_or_target_player;
  test_definition_t *sacrifice_test;
  target_definition_t *player_target;
  target_definition_t td;
  test_definition_t test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    td.allow_cancel = 3;
    new_default_test_definition(&test, 2, "Select another creature to sacrifice.");
    test.not_me = 1;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      test.qty = 2;
      if (new_can_sacrifice_as_cost(player, card, &test) == 0)
      {
        return 0;
      }
      available_or_target_player = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0,
                                                             0, 0, 0, 0, 0, 0, &td, NULL);
      return (unsigned int)(available_or_target_player != 0);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      if (new_sacrifice(player, card, player, SAC_AS_COST, &test) == 0)
      {
        return 0;
      }
      if (new_sacrifice(player, card, player, SAC_AS_COST | SAC_NO_CANCEL, &test) == 0)
      {
        return 0;
      }
      instance->number_of_targets = '\0';
      while (true)
      {
        if (can_target(&td) == 0)
        {
          return 0;
        }
        if (new_pick_target(&td, "TARGET_PLAYER", -1, 0) == 0)
        {
          if (instance->targets[instance->number_of_targets].card != -1)
          {
            return 0;
          }
          spell_fizzled = 1;
          return 0;
        }
        if (td.allowed_controller != 2)
        {
          return 0;
        }
        available_or_target_player = instance->targets[0].player;
        if (((ai_is_speculating == 1) ||
             ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
            (1 - player == available_or_target_player))
          break;
        td.allowed_controller = 1 - available_or_target_player;
        td.preferred_controller = td.allowed_controller;
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
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    td.allow_cancel = 3;
    new_default_test_definition(&test, 2, "Select another creature to sacrifice.");
    test.not_me = 1;
    instance = get_card_instance(player, card);
  }
  player_target = &td;
  sacrifice_test = &test;
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }

  for (i = 0; i < (int)(unsigned int)instance->number_of_targets;)
  {
    if (validate_target(player, card, player_target, i))
    {
      new_sacrifice(player, card, instance->targets[i].player, SAC_AS_COST | SAC_NO_CANCEL,
                    sacrifice_test);
      target_cursor = instance->targets + i;
      ++i;
      lose_life(target_cursor->player, 2);
    }
    else
      ++i;
  }
  produce_mana(player, 1, 2);
  draw_a_card(player);
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

/* ManalinkEh 2022: 0x25c18c0; original cards/ravnica_allegiance.c:2748. */
int card_prime_speaker_vannifar(int player, int card, event_t event)
{
  /* Prime Speaker Vannifar	|2|G|U
   * Legendary Creature - Elf Ooze Wizard 2/4
   * |T, Sacrifice another creature: Search your library for a creature card with mana value equal to 1 plus the sacrificed creature's mana value, put that card onto the battlefield, then shuffle. Activate only as a sorcery. */

  card_instance_t *instance;
  unsigned int chosen_sacrifice;
  int result;
  unsigned int player_00;
  test_definition_t test;
  test_definition_t test2;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
    test.not_me = 1;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x40a11, 0, 0, 0, 0, 0, 0,
                                       0, NULL, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      chosen_sacrifice = new_sacrifice(player, card, player,
                                       SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
      if (chosen_sacrifice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      add_state(player, card, 0x10);
      player_00 = (int)chosen_sacrifice >> 0x10 & 0xff;
      result = get_cmc(player_00, chosen_sacrifice >> 0x18);
      instance->info_slot = result;
      if (result < 6)
      {
        ai_modifier = ai_modifier + result * 8;
      }
      kill_card(player_00, chosen_sacrifice >> 0x18, KILL_SACRIFICE);
      tapped_for_mana_color = -1;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
    test.not_me = 1;
    instance = get_card_instance(player, card);
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  result = instance->info_slot;
  default_test_definition(&test2, 2);
  test2.cmc = result + 1;
  scnprintf(test2.message, 100, "Select a creature card with CMC %d.", test2.cmc);
  new_global_tutor(player, player, 1, 4, 0, 1, &test2);
  return 0;
}

/* ManalinkEh 2022: 0x25c1b40; original cards/ravnica_allegiance.c:2815. */
int card_pteramander(int player, int card, event_t event)
{
  /* Pteramander	|U
   * Creature - Salamander Drake 1/1
   * Flying
   * |7|U: Adapt 4. This ability costs |1 less to activate for each instant and sorcery card in your graveyard. (If this creature has no +1/+1 counters on it, put four +1/+1 counters on it.) */

  if (!IS_GAA_EVENT(event))
    return 0;
  int cless = 7 - count_graveyard_by_type(player, TYPE_INSTANT | TYPE_INTERRUPT | TYPE_SORCERY);
  return adapt(player, card, event, 4, cless, 0, 1, 0, 0, 0);
}

/* ManalinkEh 2022: 0x25c1bd0; original cards/ravnica_allegiance.c:2853. */
int card_rakdos_firewheeler(int player, int card, event_t event)
{
  /* Rakdos Firewheeler	|B|B|R|R
   * Creature - Human Rogue 4/3
   * When this creature enters, it deals 2 damage to target opponent and 2 damage to up to one target creature or planeswalker. */

  int result;
  card_instance_t *instance;
  target_definition_t td_opp;
  target_definition_t td_creature_or_planeswalker;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  default_target_definition(player, card, &td_opp, 0);
  td_opp.allowed_controller = 1 - player;
  td_opp.zone = TARGET_ZONE_PLAYERS;
  result = can_target(&td_opp);
  if (result != 0)
  {
    result = pick_target(&td_opp, "TARGET_OPPONENT");
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      default_target_definition(player, card, &td_creature_or_planeswalker, 0x102);
      result = can_target(&td_creature_or_planeswalker);
      if ((result == 0) || (result = new_pick_target(&td_creature_or_planeswalker,
                                                     "TARGET_CREATURE_OR_PLANESWALKER", 1, 0),
                            result == 0))
      {
        damage_target0(player, card, 2);
      }
      else
      {
        damage_target0(player, card, 2);
        damage_creature(instance->targets[1].player, instance->targets[1].card, 2, player, card);
      }
      instance->number_of_targets = '\0';
      return 0;
    }
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c1d30; original cards/ravnica_allegiance.c:2908. */
int card_rakdos_the_showstopper(int player, int card, event_t event)
{
  /* Rakdos, the Showstopper	|4|B|R
   * Legendary Creature - Demon 6/6
   * Flying, trample
   * When Rakdos enters, flip a coin for each creature that isn't a Demon, Devil, or Imp. Destroy each creature whose coin comes up tails. */

  card_instance_t *instance;
  int eligible_or_count;
  unsigned int player_00;
  int i;
  int *next_marked;
  int *marked_cursor;
  unsigned int player_01;
  unsigned int player_pass_or_controller;
  int count[2];
  int marked[2][150];

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    count[0] = 0;
    count[1] = 0;
    player_pass_or_controller = 0;
    player_00 = current_turn;
    do
    {
      player_01 = player_pass_or_controller ^ player_00;

      i = active_cards_count[player_01] + -1;
      if (-1 < i)
      {
        do
        {
          instance = in_play(player_01, i);
          if ((((instance != NULL) &&
                (eligible_or_count = is_what(player_01, i, 2), eligible_or_count != 0)) &&
               (eligible_or_count = has_subtype(player_01, i, SUBTYPE_DEMON),
                eligible_or_count == 0)) &&
              ((eligible_or_count = has_subtype(player_01, i, SUBTYPE_DEVIL),
                eligible_or_count == 0 &&
                    (eligible_or_count = has_subtype(player_01, i, SUBTYPE_IMP),
                     eligible_or_count == 0))))
          {
            add_status(player_01, i, 0x20000);
            eligible_or_count = flip_a_coin(player, card);
            if (eligible_or_count != 0)
            {
              eligible_or_count = count[player_01];
              count[player_01] = eligible_or_count + 1;
              marked[player_01][eligible_or_count] = i;
            }
            remove_status(player_01, i, 0x20000);
          }
          i = i + -1;
          player_00 = current_turn;
        } while (i != -1);
      }
      player_pass_or_controller = player_pass_or_controller + 1;
    } while (player_pass_or_controller != 2);

    i = count[player_00];

    player_pass_or_controller = player_00;
    if (0 < i)
    {
      marked_cursor = marked[player_00];
      do
      {
        next_marked = marked_cursor + 1;
        kill_card(player_00, *marked_cursor, KILL_DESTROY);
        marked_cursor = next_marked;
        player_pass_or_controller = current_turn;
      } while (next_marked != marked[player_00] + i);
    }
    player_pass_or_controller = player_pass_or_controller ^ 1;
    i = count[player_pass_or_controller];
    if (0 < i)
    {
      marked_cursor = marked[player_pass_or_controller];
      do
      {
        next_marked = marked_cursor + 1;
        kill_card(player_pass_or_controller, *marked_cursor, KILL_DESTROY);
        marked_cursor = next_marked;
      } while (next_marked != marked[player_pass_or_controller] + i);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25bb8b0; original cards/ravnica_allegiance.c:2999. */
static int can_counter_activated_or_triggered_ability(int player, int card, event_t event)
{
  int result;

  result = can_counter_activated_ability(player, card, event, NULL);
  if (result == 0)
  {
    result = can_counter_triggered_ability(player, card, event, NULL);
    if (result == 0)
    {
      return 0;
    }
  }
  return 99;
}

/* ManalinkEh 2022: 0x25c1f60; original cards/ravnica_allegiance.c:3004. */
int card_repudiate_replicate(int player, int card, event_t event)
{
  /* Repudiate // Replicate	|GU|GU // |1|G|U
   * Instant // Sorcery
   * Repudiate:
   * Counter target activated or triggered ability. (Mana abilities can't be targeted.)
   *
   * Replicate:
   * Create a token that's a copy of target creature you control. */

  enum { COUNTER_ACTIVATED = 1, COUNTER_TRIGGERED = 2, REPUDIATE = 1, REPLICATE = 2 };
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = td.preferred_controller = player;
  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_PLAY_SECOND_HALF)
    return generic_spell(player, card, EVENT_CAST_SPELL, GS_CAN_TARGET | GS_LITERAL_PROMPT,
                           &td, "Select target creature you control.", 1, NULL);

  if (event == EVENT_PLAY_FIRST_HALF)
  {
    if (instance->targets[9].card == COUNTER_ACTIVATED)
      cast_counter_activated_ability(player, card, 0);
    else if (instance->targets[9].card == COUNTER_TRIGGERED)
      cast_counter_triggered_ability(player, card, 0);
  }
  else if (event == EVENT_CAN_CAST)
  {
    // The second successful query wins; neither query clears an existing marker.
    if (can_counter_activated_ability(player, card, event, NULL))
      instance->targets[9].card = COUNTER_ACTIVATED;
    if (can_counter_triggered_ability(player, card, event, NULL))
      instance->targets[9].card = COUNTER_TRIGGERED;
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    unsigned int halves = instance->info_slot;
    if (halves & REPUDIATE)
    {
      if (instance->targets[9].card == COUNTER_ACTIVATED)
      {
        resolve_counter_activated_ability(player, card, NULL, 0);
        halves = instance->info_slot;
      }
      else if (instance->targets[9].card == COUNTER_TRIGGERED)
      {
        resolve_counter_triggered_ability(player, card, NULL, 0);
        halves = instance->info_slot;
      }
    }
    if ((halves & REPLICATE) && valid_target(&td))
      copy_token(player, card, player, instance->targets[0].card);
    kill_card(player, card, KILL_DESTROY);
  }

  int replicate_available = 0;
  if (can_target(&td))
    replicate_available = can_sorcery_be_played(player, event) != 0;
  int repudiate_available = can_counter_activated_or_triggered_ability(player, card, event);
  return hybrid_split_card(player, card, event, CARD_ID_REPUDIATE, repudiate_available, 10,
                             CARD_ID_REPLICATE, replicate_available, 8);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c2250; original cards/ravnica_allegiance.c:3080. */
int card_revival_revenge(int player, int card, event_t event)
{
  /* Revival // Revenge	|WB|WB // |4|W|B
   * Sorcery // Sorcery
   * Revival:
   * Return target creature card with mana value 3 or less from your graveyard to the battlefield.
   *
   * Revenge:
   * Double your life total. Target opponent loses half their life, rounded up. */

  card_instance_t *instance;
  int can_play_first_half;
  unsigned int selected_halves_or_life_loss;
  int result;

  target_definition_t td;
  char msg[100];
  test_definition_t this_test;
  memcpy(msg, "Select a creature card with CMC 3 or less.", 43);
  memset(msg + 43, 0, 57);
  new_default_test_definition(&this_test, 2, msg);
  this_test.cmc = 4;
  this_test.cmc_flag = 3;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.allowed_controller = 1 - player;
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    return generic_spell(player, card, EVENT_CAST_SPELL, 8, NULL, NULL, 1, &this_test);
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    new_pick_target(&td, "TARGET_OPPONENT", 0, 1);
    return 0;
  }
  if (event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET)
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      selected_halves_or_life_loss = instance->info_slot;
      if ((selected_halves_or_life_loss & 1) != 0)
      {
        result = validate_target_from_grave_source(player, card, player, 0);
        if (result != -1)
        {
          reanimate_permanent(player, card, player, result, REANIMATE_DEFAULT);
        }
        selected_halves_or_life_loss = instance->info_slot;
      }
      if ((selected_halves_or_life_loss & 2) != 0)
      {
        gain_life(player, life[player]);
        result = valid_target(&td);
        if (result != 0)
        {
          selected_halves_or_life_loss = round_up_value(life[instance->targets[0].player]);
          lose_life(instance->targets[0].player, selected_halves_or_life_loss);
        }
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  else if (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET ||
           (event == EVENT_CAST_SPELL && card == affected_card &&
            player == affected_card_controller))
    get_card_instance(player, card);
  result = can_target(&td);
  can_play_first_half = generic_spell(player, card, EVENT_CAN_CAST, 8, NULL, NULL, 1, &this_test);
  return hybrid_split_card(player, card, event, 0x4d02, can_play_first_half, 5, 0x4d01, result, 10);
}
#endif

/* ManalinkEh 2022: 0x25c25a0; original cards/ravnica_allegiance.c:3133. */
int card_rhythm_of_the_wild(int player, int card, event_t event)
{
  /* Rhythm of the Wild	|1|R|G
   * Enchantment
   * Creature spells you control can't be countered.
   * Nontoken creatures you control have riot. (They enter with your choice of a +1/+1 counter or haste.) */

  int result;
  card_instance_t *instance;
  test_definition_t test;

  type_uncounterable(player, card, event, player, 2, NULL);
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 2);
    test.type_flag = 3;
    result = new_specific_cip(player, card, event, player, 2, &test);
    if (result != 0)
    {

      result = dialog_impl(trigger_cause_controller, trigger_cause, EVENT_ACTIVATE, DLG_RANDOM,
                           DLG_NO_CANCEL, "+1/+1 counter", 1, 5, "Haste", 1, 5, 0, 0, 0, 0);
      if (result == 1)
      {
        add_counters(trigger_cause_controller, trigger_cause, trigger_cause_controller,
                                 trigger_cause, COUNTER_P1_P1, 1);
      }
      else if (result == 2)
      {

        instance = get_card_instance(trigger_cause_controller, trigger_cause);
        pump_ability_until_eot_no_repeat(player, card, trigger_cause_controller, trigger_cause, 0,
                                         -0x7ffffffc);
        result = is_humiliated(trigger_cause_controller, trigger_cause, AL_KEY);
        if (result == 0)
        {
          instance->state = instance->state & 0xfffdffff;
        }
      }
    }
  }
  return global_enchantment(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c27a0; original cards/ravnica_allegiance.c:3166. */
int card_rix_maadi_reveler(int player, int card, event_t event)
{
  /* Rix Maadi Reveler	|1|R
   * Creature - Human Shaman 2/2
   * Spectacle |2|B|R (You may cast this spell for its spectacle cost rather than its mana cost if an opponent lost life this turn.)
   * When this creature enters, discard a card, then draw a card. If this creature's spectacle cost was paid, instead discard your hand, then draw three cards. */

  int result;
  card_instance_t *instance;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    instance = get_card_instance(player, card);
    if (((unsigned int)instance->info_slot & 2) == 0)
    {
      discard(player, 0, player);
      draw_a_card(player);
    }
    else
    {
      new_discard_all(player, player);
      draw_cards(player, 3);
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
    }
  }
  result = spectacle_impl(player, card, event, 2, 1, 0, 0, 1, 0);
  return result;
}
#endif

/* ManalinkEh 2022: 0x25c2870; original cards/ravnica_allegiance.c:3250. */
int card_sauroform_hybrid(int player, int card, event_t event)
{
  /* Sauroform Hybrid	|1|G
   * Creature - Human Lizard Warrior 2/2
   * |4|G|G: Adapt 4. (If this creature has no +1/+1 counters on it, put four +1/+1 counters on it.) */

  int result;

  result = adapt(player, card, event, 4, 4, 0, 0, 2, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x25c28d0; original cards/ravnica_allegiance.c:3325. */
int card_seraph_of_the_scales(int player, int card, event_t event)
{
  /* Seraph of the Scales	|2|W|B
   * Creature - Angel 4/3
   * Flying
   * |W: This creature gains vigilance until end of turn.
   * |B: This creature gains deathtouch until end of turn.
   * Afterlife 2 (When this creature dies, create two 1/1 white and black Spirit creature tokens with flying.) */

  enum
  {
    CHOICE_VIGILANCE = 1,
    CHOICE_DEATHTOUCH = 2
  };

  int choice;
  card_instance_t *instance;
  uint32_t deathtouch_priority;
  unsigned int current_keywords;
  int vigilance_available_or_black_cost;
  int deathtouch_available_or_white_cost;
  int special_keyword;
  int vigilance_priority;

  afterlife(player, card, event, 2);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0, 0, 0, 1, 0, NULL,
                                  NULL) != 0)
    {
      return 1;
    }
    vigilance_available_or_black_cost = generic_activated_ability(
        player, card, EVENT_CAN_ACTIVATE, 0, 0, 1, 0, 0, 0, 0, 0, NULL, NULL);
    return (unsigned int)(vigilance_available_or_black_cost != 0);
  }
  if (event != EVENT_ACTIVATE)
  {
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      return 0;
    }

    instance = get_card_instance(player, card);
    if (instance->info_slot == CHOICE_VIGILANCE)
    {
      special_keyword = 8;
      deathtouch_available_or_white_cost = 1;
      vigilance_available_or_black_cost = 0;
    }
    else
    {
      if (instance->info_slot != CHOICE_DEATHTOUCH)
      {
        return 0;
      }
      special_keyword = 0x200;
      deathtouch_available_or_white_cost = 0;
      vigilance_available_or_black_cost = 1;
    }
    return generic_shade(player, card, EVENT_RESOLVE_ACTIVATION, 0, 0,
                         vigilance_available_or_black_cost, 0, 0, 0,
                         deathtouch_available_or_white_cost, 0, 0, 0, special_keyword);
  }

  vigilance_available_or_black_cost = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0,
                                                                0, 0, 0, 0, 0, 1, 0, NULL, NULL);
  deathtouch_available_or_white_cost = generic_activated_ability(
      player, card, EVENT_CAN_ACTIVATE, 0, 0, 1, 0, 0, 0, 0, 0, NULL, NULL);
  instance = get_card_instance(player, card);
  current_keywords = 0;
  if (-1 < instance->targets[1].card)
  {
    instance = get_card_instance(player, card);
    current_keywords = instance->targets[1].card;
  }
  vigilance_priority = 1;
  if (current_phase < 0x15)
  {
    vigilance_priority = 1;
    special_keyword = check_for_special_ability(player, card, 8);
    if (special_keyword == 0)
    {
      vigilance_priority = (((current_keywords & 8) == 0) ? 10U : 1U);
    }
  }
  if (((current_phase == 0x18) &&
       (special_keyword = is_unblocked(player, card), special_keyword == 0)) &&
      (special_keyword = check_for_special_ability(player, card, 0x200),
       (current_keywords & 0x200) == 0 && special_keyword == 0))
  {
    deathtouch_priority = 10;
  }
  else
  {
    deathtouch_priority = 1;
  }
  choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Vigilance",
                       vigilance_available_or_black_cost, vigilance_priority, "Deathtouch",
                       deathtouch_available_or_white_cost, deathtouch_priority, 0, 0, 0, 0);
  if (choice != 0)
  {
    if (choice == CHOICE_VIGILANCE)
    {
      deathtouch_available_or_white_cost = 1;
      vigilance_available_or_black_cost = 0;
    }
    else
    {
      if (choice != CHOICE_DEATHTOUCH)
      {
        return 0;
      }
      deathtouch_available_or_white_cost = 0;
      vigilance_available_or_black_cost = 1;
    }
    return generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 0,
                                     vigilance_available_or_black_cost, 0, 0, 0,
                                     deathtouch_available_or_white_cost, 0, NULL, NULL);
  }
  spell_fizzled = 1;
  return 0;
}

/* ManalinkEh 2022: 0x25c2df0; original cards/ravnica_allegiance.c:3395. */
int card_shimmer_of_possibility(int player, int card, event_t event)
{
  /* Shimmer of Possibility	|1|U
   * Sorcery
   * Look at the top four cards of your library. Put one of them into your hand and the rest on the bottom of your library in a random order. */

  int result;
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = 0x18;
      if (1 - player == current_turn)
      {
        result = ((0x1d < current_phase) ? 48U : 24U);
      }
      ai_modifier = ai_modifier + result;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    impulse_effect(player, 4, 3);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x25c2eb0; original cards/ravnica_allegiance.c:3421. */
int card_simic_ascendancy(int player, int card, event_t event)
{
  /* Simic Ascendancy	|G|U
   * Enchantment
   * |1|G|U: Put a +1/+1 counter on target creature you control.
   * Whenever one or more +1/+1 counters are put on a creature you control, put that many growth counters on this enchantment.
   * At the beginning of your upkeep, if this enchantment has twenty or more growth counters on it, you win the game. */

  upkeep_trigger_ability(player, card, event, player);
  enable_xtrigger_flags |= ENABLE_XTRIGGER_1_1_COUNTERS;
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY && count_counters(player, card, COUNTER_GROWTH) >= 20)
    lose_the_game(1-player);

  if (xtrigger_condition() == XTRIGGER_1_1_COUNTERS && affect_me(player, card) &&
      trigger_cause_controller == player && reason_for_trigger_controller == player &&
      !is_humiliated(player, card, AL_TRIG))
  {
    if (event == EVENT_RESOLVE_TRIGGER)
    {
      add_counters(player, card, player, card, COUNTER_GROWTH, counters_added);
    }
    else if (event == EVENT_TRIGGER)
      event_result |= RESOLVE_TRIGGER_MANDATORY;
  }
  if (!IS_GAA_EVENT(event))
    return global_enchantment(player, card, event);

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.allow_cancel = 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    int target_card = get_card_instance(player, card)->targets[0].card;
    card_instance_t *instance = get_card_instance(player, card);
    add_counters(player, card, instance->targets[0].player, target_card, COUNTER_P1_P1, 1);
  }
  return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XGU(1, 1, 1),
                                    0, &td, "TARGET_CREATURE_YOU_CONTROL");
}

/* ManalinkEh 2022: 0x25c3100; original cards/ravnica_allegiance.c:3467. */
int card_skarrgan_hellkite(int player, int card, event_t event)
{
  /* Skarrgan Hellkite	|3|R|R
   * Creature - Dragon 4/4
   * Riot (This creature enters with your choice of a +1/+1 counter or haste.)
   * Flying
   * |3|R: This creature deals 2 damage divided as you choose among one or two targets. Activate only if this creature has a +1/+1 counter on it. */

  int result;
  target_definition_t td;

  riot(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = count_counters(player, card, COUNTER_P1_P1);
      if (result == 0)
      {
        return 0;
      }
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 3, 0, 0, 0, 1, 0, 0,
                                         &td, "TARGET_ANY");
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 4, 3, 0, 0, 0, 1, 0, 0, &td,
                                "TARGET_ANY");
      if (spell_fizzled == 1)
      {
        return 0;
      }
      new_pick_target(&td, "TARGET_ANY", 1, 1);
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x102);
  }
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  divide_damage(player, card, &td);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c32f0; original cards/ravnica_allegiance.c:3513. */
int card_skewer_the_critics(int player, int card, event_t event)
{
  /* Skewer the Critics	|2|R
   * Sorcery
   * Spectacle |R (You may cast this spell for its spectacle cost rather than its mana cost if an opponent lost life this turn.)
   * ~ deals 3 damage to any target. */

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
        spectacle_impl(player, card, event, 0, 0, 0, 0, 1, 0);
        default_target_definition(player, card, &td, 0x102);
        td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
        if (event == EVENT_RESOLVE_SPELL)
        {
          if (valid_target(&td))
          {
            damage_target0(player, card, 3);
          }
          kill_card(player, card, KILL_DESTROY);
        }
        return generic_spell(player, card, event, 1, &td, "TARGET_ANY", 1, NULL);
      }
      if ((event != EVENT_CHANGE_TARGET) && (event != EVENT_MODIFY_COST))
      {
        return 0;
      }
    }
  }
  spectacle_impl(player, card, event, 0, 0, 0, 0, 1, 0);
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  return generic_spell(player, card, event, 1, &td, "TARGET_ANY", 1, NULL);
}
#endif

/* ManalinkEh 2022: 0x25c3480; original cards/ravnica_allegiance.c:3541. */
int card_skitter_eel(int player, int card, event_t event)
{
  /* Skitter Eel	|3|U
   * Creature - Fish Crab 3/3
   * |2|U: Adapt 2. (If this creature has no +1/+1 counters on it, put two +1/+1 counters on it.) */

  int result;

  result = adapt(player, card, event, 2, 2, 0, 1, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x25c34e0; original cards/ravnica_allegiance.c:3575. */
int card_smothering_tithe(int player, int card, event_t event)
{
  /* Smothering Tithe	|3|W
   * Enchantment
   * Whenever an opponent draws a card, that player may pay |2. If the player doesn't, you create a Treasure token. (It's an artifact with "|T, Sacrifice this token: Add one mana of any color.") */

  int result;
  result = card_drawn_trigger(player, card, event, 1 - player, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    charge_mana(1 - player, COLOR_ANY, 2);
    if (spell_fizzled == 1)
    {
      generate_token_by_id(player, card, CARD_ID_TREASURE);
    }
  }
  return global_enchantment(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c3580; original cards/ravnica_allegiance.c:3591. */
int card_spawn_of_mayhem(int player, int card, event_t event)
{
  /* Spawn of Mayhem	|2|B|B
   * Creature - Demon 4/4
   * Spectacle |1|B|B (You may cast this spell for its spectacle cost rather than its mana cost if an opponent lost life this turn.)
   * Flying, trample
   * At the beginning of your upkeep, this creature deals 1 damage to each player. Then if you have 10 or less life, put a +1/+1 counter on this creature. */

  int result;
  card_instance_t *instance;
  if (((((trigger_condition == TRIGGER_UPKEEP) && (card == affected_card)) &&
        (player == affected_card_controller)) &&
       ((reason_for_trigger_controller == player && (current_turn == player)))) &&
      (result = is_humiliated(player, card, AL_TRIG), result == 0))
  {
    result = count_upkeeps(current_turn);
    if ((event == EVENT_TRIGGER) && (0 < result))
    {
      event_result = event_result | 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      if (0 < result)
      {
        do
        {
          instance = in_play(player, card);
          if (instance == NULL)
            break;
          APNAP(p, {
            damage_player(p, 1, player, card);
          });
          result = result + -1;
        } while (result != 0);
      }
    }
    else if (0 < result)
    {
      do
      {
        instance = in_play(player, card);
        if (instance == NULL)
          break;
        if (life[player] < 0xb)
        {
          add_counters(player, card, player, card, COUNTER_P1_P1, 1);
        }
        result = result + -1;
      } while (result != 0);
    }
  }
  return spectacle_impl(player, card, event, 1, 2, 0, 0, 0, 0);
}
#endif

/* ManalinkEh 2022: 0x25c3720; original cards/ravnica_allegiance.c:3649. */
int card_sphinx_of_the_guildpact(int player, int card, event_t event)
{
  /* Sphinx of the Guildpact	|7
   * Artifact Creature - Sphinx 5/5
   * ~ is all colors.
   * Flying
   * Hexproof from monocolored (This creature can't be the target of monocolored spells or abilities your opponents control.) */

  unsigned int spell_colors_or_count;

  misc_hexproof(player, card, event);
  if (((event == EVENT_PROT_MISC) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    spell_colors_or_count = get_color(attacking_card_controller, attacking_card);
    spell_colors_or_count = num_bits_set(spell_colors_or_count);
    if (spell_colors_or_count == 1)
    {
      event_result = event_result | 1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c37a0; original cards/ravnica_allegiance.c:3665. */
int card_sphinxs_insight(int player, int card, event_t event)
{
  /* Sphinx's Insight	|2|W|U
   * Instant
   * Draw two cards.
   * Addendum - If you cast this spell during your main phase, you gain 2 life. */

  int result;
  if (event == EVENT_RESOLVE_SPELL)
  {
    draw_cards(player, 2);
    if ((current_turn == player) && ((current_phase == 0x14 || (current_phase == 0x1e))))
    {
      gain_life(player, 2);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c3830; original cards/ravnica_allegiance.c:3683. */
int card_spikewheel_acrobat(int player, int card, event_t event)
{
  /* Spikewheel Acrobat	|3|R
   * Creature - Human Rogue 5/2
   * Spectacle |2|R (You may cast this spell for its spectacle cost rather than its mana cost if an opponent lost life this turn.) */

  int result;
  result = spectacle_impl(player, card, event, 2, 0, 0, 0, 1, 0);
  return result;
}
#endif

/* ManalinkEh 2022: 0x25c3860; original cards/ravnica_allegiance.c:3739. */
int card_sunder_shaman(int player, int card, event_t event)
{
  /* Sunder Shaman	|R|R|G|G
   * Creature - Giant Shaman 5/5
   * This creature can't be blocked by more than one creature.
   * Whenever this creature deals combat damage to a player, destroy target artifact or enchantment that player controls. */

  maxblockers_1(player, card, event);
  if (damage_dealt_by_me(player, card, event,
                         DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                             DDBM_MUST_BE_COMBAT_DAMAGE))
  {
    card_instance_t *instance = get_card_instance(player, card);
    unsigned int packed_damage = instance->targets[1].player;
    instance->targets[1].player = 0;
    int times_damaged[2] = {packed_damage & 0xff, packed_damage >> 8 & 0xff};
    APNAP(p, {
      if (times_damaged[p])
      {
        target_definition_t td;
        default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);
        td.allow_cancel = 0;
        td.allowed_controller = p;
        td.preferred_controller = p;
        if (can_target(&td))
        {
          const char *prompt = player == p
              ? "Select target artifact or enchantment you control."
              : "Select target artifact or enchantment controlled by opponent.";
          if (new_pick_target(&td, prompt, 0, 0x80))
            action_on_target(player, card, 0, ACT_KILL_DESTROY);
        }
      }
      instance->number_of_targets = 0;
    });
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c39a0; original cards/ravnica_allegiance.c:3805. */
int card_syndicate_messenger(int player, int card, event_t event)
{
  /* Syndicate Messenger	|3|W
   * Creature - Bird 2/3
   * Flying
   * Afterlife 1 (When this creature dies, create a 1/1 white and black Spirit creature token with flying.) */

  afterlife(player, card, event, 1);
  return 0;
}

/* ManalinkEh 2022: 0x25c39c0; original cards/ravnica_allegiance.c:3832. */
int card_teysa_karlov(int player, int card, event_t event)
{
  /* Teysa Karlov	|2|W|B
   * Legendary Creature - Human Advisor 2/4
   * If a creature dying causes a triggered ability of a permanent you control to trigger, that ability triggers an additional time.
   * Creature tokens you control have vigilance and lifelink. */

  int result;

  check_legend_rule(player, card, event);
  if (event != EVENT_STATIC_EFFECTS)
  {
    if (event == EVENT_ABILITIES)
    {
      result = is_token(affected_card_controller, affected_card);
      if (result != 0)
      {
        result = is_what(affected_card_controller, affected_card, 2);
        if (result != 0)
        {
          vigilance(affected_card_controller, affected_card, EVENT_ABILITIES);
          lifelink(affected_card_controller, affected_card, EVENT_ABILITIES);
        }
      }
    }
    return 0;
  }
  card_counts[player][CCOUNTS_TEYSA_KARLOV] = card_counts[player][CCOUNTS_TEYSA_KARLOV] + '\x01';
  return 0;
}

/* ManalinkEh 2022: 0x25c3aa0; original cards/ravnica_allegiance.c:3890. */
int card_thrash_threat(int player, int card, event_t event)
{
  /* Thrash // Threat	|RG|RG // |2|R|G
   * Instant // Sorcery
   * Thrash:
   * Target creature you control deals damage equal to its power to target creature or planeswalker you don't control.
   *
   * Threat:
   * Create a 4/4 red and green Beast creature token with trample. */

  card_instance_t *instance;
  int source_card_or_available;
  int source_player_or_available;
  unsigned int selected_halves_or_available;
  int amt;
  token_generation_t token;
  target_definition_t td_control;
  target_definition_t td_dont_control;

  default_target_definition(player, card, &td_control, 2);
  td_control.preferred_controller = player;
  td_control.allowed_controller = player;
  default_target_definition(player, card, &td_dont_control, 0x102);
  td_dont_control.allowed_controller = 1 - player;
  td_dont_control.preferred_controller = td_dont_control.allowed_controller;
  if (event == EVENT_PLAY_FIRST_HALF)
  {

    instance = get_card_instance(player, card);
    source_card_or_available = pick_target(&td_control, "ASHNODS_BATTLEGEAR");
    if ((source_card_or_available == 0) ||
        (source_card_or_available = pick_next_target_noload(
             &td_dont_control, "Select target creature or planeswalker you don\'t control."),
         source_card_or_available == 0))
    {
      instance->number_of_targets = '\0';
    }
  }
  if (event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET)
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      selected_halves_or_available = instance->info_slot;
      if ((selected_halves_or_available & 1) != 0)
      {
        source_card_or_available = valid_target(&td_control);
        if ((source_card_or_available != 0) &&
            (source_card_or_available = validate_target(player, card, &td_dont_control, 1),
             source_card_or_available != 0))
        {
          source_card_or_available = instance->targets[0].card;
          source_player_or_available = instance->targets[0].player;
          amt = get_power(source_player_or_available, source_card_or_available);
          damage_creature(instance->targets[1].player, instance->targets[1].card, amt,
                          source_player_or_available, source_card_or_available);
        }
        selected_halves_or_available = instance->info_slot;
      }
      if ((selected_halves_or_available & 2) != 0)
      {
        default_token_definition(player, card, CARD_ID_BEAST, &token);
        token.color_forced = '\x18';
        token.tou = 4;
        token.pow = 4;
        token.key_plus = 0x80;
        generate_token(&token);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  else if (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET ||
           (event == EVENT_CAST_SPELL && card == affected_card &&
            player == affected_card_controller))
    get_card_instance(player, card);
  source_card_or_available = can_sorcery_be_played(player, event);
  source_player_or_available = can_target(&td_control);
  selected_halves_or_available = 0;
  if (source_player_or_available != 0)
  {
    source_player_or_available = can_target(&td_dont_control);
    selected_halves_or_available = (unsigned int)(source_player_or_available != 0);
  }
  return hybrid_split_card(player, card, event, 0x4d3e, selected_halves_or_available, 5, 0x4d40,
                           source_card_or_available, 8);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25bb910; original cards/ravnica_allegiance.c:3947. */
static int tin_street_dodger_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  card_instance_t *effect;
  int result;

  instance = get_card_instance(player, card);
  if (event == EVENT_BLOCK_LEGALITY)
  {
    effect = get_card_instance(player, card);
    if (((-1 < effect->damage_target_player) &&
         (instance->damage_target_player == attacking_card_controller)) &&
        (instance->damage_target_card == attacking_card))
    {
      result = check_for_ability(affected_card_controller, affected_card, KEYWORD_DEFENDER);
      if (result == 0)
      {
        event_result = 1;
      }
    }
  }
  else if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c3d60; original cards/ravnica_allegiance.c:3958. */
int card_tin_street_dodger(int player, int card, event_t event)
{
  /* Tin Street Dodger	|R
   * Creature - Goblin Rogue 1/1
   * Haste
   * |R: This creature can't be blocked this turn except by creatures with defender. */

  haste(player, card, event);
  if (!IS_GAA_EVENT(event))
    return 0;

  if (event == EVENT_ACTIVATE)
  {
    if (current_turn != player || current_phase > 0x17)
      ai_modifier -= 1000;
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    card_instance_t *instance = get_card_instance(player, card);
    create_targetted_legacy_effect(player, card, tin_street_dodger_legacy,
                                   instance->parent_controller, instance->parent_card);
  }
  return generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 1, 0, 0, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x25c3e80; original cards/ravnica_allegiance.c:3991. */
int card_tithe_taker(int player, int card, event_t event)
{
  /* Tithe Taker	|1|W
   * Creature - Human Soldier 2/1
   * During your turn, spells your opponents cast cost |1 more to cast and abilities your opponents activate cost |1 more to activate unless they're mana abilities.
   * Afterlife 1 (When this creature dies, create a 1/1 white and black Spirit creature token with flying.) */

  test_definition_t this_test;
  if ((event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_SKIP_TURN) && current_turn == player)
  {
    default_test_definition(&this_test, 0x47);
    set_cost_mod_for_activated_abilities(1 - player, -1, 5, &this_test);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (current_turn == player && 1 - player == affected_card_controller &&
        is_what(1 - player, affected_card, 0x47))
      set_cost_mod_for_activated_abilities(affected_card_controller, affected_card, 5, NULL);
    leaves_play(player, card, EVENT_CAST_SPELL, 0, 0);
  }
  else
  {
    leaves_play(player, card, event, 0, 0);
    if ((event == EVENT_LEAVES_PLAY_ABILITY && current_turn == player) ||
        (event == EVENT_CAN_SKIP_TURN && 1 - player == current_turn))
    {
      int taxed_player = event == EVENT_CAN_SKIP_TURN ? current_turn : 1 - player;
      default_test_definition(&this_test, 0x47);
      remove_cost_mod_for_activated_abilities(taxed_player, -1, 3, &this_test);
    }
    if (event == EVENT_MODIFY_COST_GLOBAL && !is_humiliated(player, card, AL_STATIC) &&
        1 - player == affected_card_controller && current_turn == player)
    {
      card_data_t *spell = get_card_data(1 - player, affected_card);
      if (!(spell->type & 1))
        COST_COLORLESS += 1;
    }
  }
  afterlife(player, card, event, 1);
  return 0;
}

/* ManalinkEh 2022: 0x25c40d0; original cards/ravnica_allegiance.c:4054. */
int card_unbreakable_formation(int player, int card, event_t event)
{
  /* Unbreakable Formation	|2|W
   * Instant
   * Creatures you control gain indestructible until end of turn.
   * Addendum - If you cast this spell during your main phase, put a +1/+1 counter on each of those creatures and they gain vigilance until end of turn. */

  int c;
  card_instance_t *instance;
  bool opponents_turn;
  bool not_first_main;
  bool not_second_main;

  if (event == EVENT_RESOLVE_SPELL)
  {

    opponents_turn = current_turn != player;
    not_first_main = current_phase != 0x14;
    not_second_main = current_phase != 0x1e;
    for (c = 0; c < active_cards_count[player]; ++c)
    {
      instance = in_play(player, c);
      if (instance && is_what(player, c, 2))
      {
        if (opponents_turn || (not_second_main && not_first_main))
          pump_ability_until_eot(player, card, player, c, 0, 0, 0, 0x800);
        else
        {
          pump_ability_until_eot(player, card, player, c, 0, 0, 0, 0x808);
          add_counters(player, card, player, c, COUNTER_P1_P1, 1);
        }
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x25bb9c0; original cards/ravnica_allegiance.c:4098. */
static int vindictive_vampire_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  card_instance_t *dying;
  unsigned int pending_deaths;
  test_definition_t this_test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (get_id((int)instance->damage_target_player, instance->damage_target_card) != 0x4d4b))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (effect_follows_control_of_attachment(player, card, 0x4700) != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (effect_follows_control_of_attachment(player, card, event) != 0))
    {
      return 0;
    }
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {

      instance = get_card_instance(player, card);
      dying = in_play(affected_card_controller, affected_card);
      if ((dying != NULL) &&
          (is_this_dying(affected_card_controller, affected_card) != 0))
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
        default_test_definition(&this_test, 2);
        this_test.not_me = 1;
        count_for_gfp_ability(player, card, 0x77, player, 0, &this_test);
      }
    }
  }
  pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_deaths != 0)
  {
    if (!check_status(player, card, 0x10000))
    {

      for (pending_deaths = pending_deaths & 0xff; pending_deaths != 0;
           pending_deaths = pending_deaths - 1)
      {
        damage_player(1 - player, 1, player, card);
        gain_life(player, 1);
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) && (check_status(player, card, 0x10000) == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c4240; original cards/ravnica_allegiance.c:4152. */
int card_vindictive_vampire(int player, int card, event_t event)
{
  /* Vindictive Vampire	|3|B
   * Creature - Vampire 2/3
   * Whenever another creature you control dies, this creature deals 1 damage to each opponent and you gain 1 life. */

  int c;
  unsigned int pending_deaths;
  card_instance_t *instance;
  int created_legacy;
  test_definition_t this_test;

  bool legacy_found = false;
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      c = 0;
      if (0 < active_cards_count[player])
      {
        do
        {
          instance = in_play(player, c);

          if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
               (instance = get_card_instance(player, c),
                instance->info_slot_fn == vindictive_vampire_effect)) &&
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
        created_legacy =
            create_targetted_legacy_effect(player, card, vindictive_vampire_effect, player, card);
        instance = get_card_instance(player, created_legacy);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, created_legacy, 0x10000);
      }
    }
  }
  else if ((event == EVENT_GRAVEYARD_FROM_PLAY) &&
           (instance = in_play(player, card), instance != NULL))
  {
    default_test_definition(&this_test, 2);
    this_test.not_me = 1;
    count_for_gfp_ability(player, card, 0x77, player, 0, &this_test);
  }
  pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

  if (((pending_deaths != 0) &&
       (instance = get_card_instance(player, card), instance->kill_code == '\0')) &&
      (pending_deaths = pending_deaths & 0xff, pending_deaths != 0))
  {
    do
    {
      damage_player(1 - player, 1, player, card);
      gain_life(player, 1);
      pending_deaths = pending_deaths - 1;
    } while (pending_deaths != 0);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c4470; original cards/ravnica_allegiance.c:4231. */
int card_warrant_warden(int player, int card, event_t event)
{
  /* Warrant // Warden	|WU|WU // |3|W|U
   * Instant // Sorcery
   * Warrant:
   * Put target attacking or blocking creature on top of its owner's library.
   *
   * Warden:
   * Create a 4/4 white and blue Sphinx creature token with flying and vigilance. */

  card_instance_t *instance;
  unsigned int selected_halves;
  int result;
  int can_play_first_half;
  token_generation_t token;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  td.required_state = TARGET_STATE_IN_COMBAT;
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_ATTACKING_BLOCKING_CREATURE", 1,
                  NULL);
  }
  if (event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET)
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      selected_halves = instance->info_slot;
      if ((selected_halves & 1) != 0)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
        }
        selected_halves = instance->info_slot;
      }
      if ((selected_halves & 2) != 0)
      {
        default_token_definition(player, card, CARD_ID_SPHINX, &token);
        token.color_forced = '$';
        token.key_plus = 0x20;
        token.s_key_plus = 8;
        generate_token(&token);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  else if (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET ||
           (event == EVENT_CAST_SPELL && card == affected_card &&
            player == affected_card_controller))
    get_card_instance(player, card);
  result = can_sorcery_be_played(player, event);
  can_play_first_half = can_target(&td);
  return hybrid_split_card(player, card, event, 0x4d4f, can_play_first_half, 5, 0x4d4e, result, 8);
}

/* ManalinkEh 2022: 0x25c4680; original cards/ravnica_allegiance.c:4280. */
int card_wilderness_reclamation(int player, int card, event_t event)
{
  /* Wilderness Reclamation	|3|G
   * Enchantment
   * At the beginning of your end step, untap all lands you control. */

  if (current_turn == player)
  {
    if (eot_trigger(player, card, event))
    {
      manipulate_type(player, card, player, TYPE_LAND, ACT_UNTAP);
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25c46f0; original cards/ravnica_allegiance.c:4293. */
int card_windstorm_drake(int player, int card, event_t event)
{
  /* Windstorm Drake	|4|U
   * Creature - Drake 3/3
   * Flying
   * Other creatures you control with flying get +1/+0. */

  card_instance_t *instance;
  int result;

  if (((event == EVENT_POWER) && (affected_card_controller == player)) && (affected_card != card))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      instance = in_play(affected_card_controller, affected_card);
      if (instance != NULL)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {
          result = check_for_ability(affected_card_controller, affected_card, 0x20);
          if (result != 0)
          {
            event_result = event_result + 1;
          }
        }
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c47a0; original cards/ravnica_allegiance.c:4319. */
int card_zegana_utopian_speaker(int player, int card, event_t event)
{
  /* Zegana, Utopian Speaker	|2|G|U
   * Legendary Creature - Merfolk Wizard 4/4
   * When Zegana enters, if you control another creature with a +1/+1 counter on it, draw a card.
   * |4|G|U: Adapt 4. (If this creature has no +1/+1 counters on it, put four +1/+1 counters on it.)
   * Each creature you control with a +1/+1 counter on it has trample. */

  card_instance_t *instance;
  int eligible_or_counters;
  int c;
  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if ((event == EVENT_ETB) && (-1 < active_cards_count[player]))
  {
    c = 0;
    do
    {
      instance = in_play(player, c);
      if ((((instance != NULL) && (eligible_or_counters = is_what(player, c, 2), card != c)) &&
           (eligible_or_counters != 0)) &&
          (eligible_or_counters = count_counters(player, c, COUNTER_P1_P1),
           eligible_or_counters != 0))
      {
        draw_a_card(player);
        return 0;
      }
      c = c + 1;
    } while (c <= active_cards_count[player]);
  }
  boost_creatures_by_counters(player, card, event, 0, 1, 0, 0, 0x80, 0,
                              BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  return adapt(player, card, event, 4, 4, 0, 1, 1, 0, 0);
}
#endif

/* ManalinkEh 2022: 0x25c4900; original cards/ravnica_allegiance.c:4343. */
int card_zhur_taa_goblin(int player, int card, event_t event)
{
  /* Zhur-Taa Goblin	|R|G
   * Creature - Goblin Berserker 2/2
   * Riot (This creature enters with your choice of a +1/+1 counter or haste.) */

  riot(player, card, event);
  return 0;
}
