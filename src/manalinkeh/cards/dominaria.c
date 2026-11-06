#include "manalink.h"

static int give_token_haste_legacy(int player, int card, event_t event);

int tiana_effect(int player, int card, event_t event);

int saga(int player, int card, event_t event, int chapters);

static int slimefoot_effect(int player, int card, event_t event);

int the_mirari_conjecture_legacy(int player, int card, event_t event);

static int is_historic_by_internal_id(int iid, intptr_t unused, int player, int card);

static char *can_cast_historic_spell(int who_chooses, int player, int card, int targeting_player,
                                     int targeting_card);

/* ManalinkEh 2022: 0x22ca500; original cards/dominaria.c:3. */
static int is_historic_by_internal_id(int iid, intptr_t unused, int player, int card)
{
  if (cards_data[iid].type & TYPE_ARTIFACT)
    return 1;
  if (is_legendary(-1, iid))
    return 1;
  return has_subtype_by_id(player, (unsigned int)cards_data[iid].id, SUBTYPE_SAGA) != 0;
}

/* ManalinkEh 2022: 0x22ca380; original cards/dominaria.c:7. */
static int is_historic(int iid, intptr_t unused, int player, int card)
{
  int result;

  result = is_what(player, card, 0x40);
  if (result == 0)
  {
    result = is_legendary(player, card);
    if (result == 0)
    {
      result = has_subtype(player, card, SUBTYPE_SAGA);
      return (unsigned int)(result != 0);
    }
  }
  return 1;
}

/* ManalinkEh 2022: 0x22ca3f0; original cards/dominaria.c:11. */
static void cast_historic_spell(int player, int card, event_t event, int trigger_mode)
{
  int result;
  if ((trigger_condition == TRIGGER_SPELL_CAST) && (affected_card == card))
  {
    if (((affected_card_controller == player) && (player == reason_for_trigger_controller)) &&
        ((result = is_what(trigger_cause_controller, trigger_cause, 0x40),
          result != 0 || ((result = is_legendary(trigger_cause_controller, trigger_cause),
                           result != 0 || (result = has_subtype(trigger_cause_controller,
                                                                trigger_cause, SUBTYPE_SAGA),
                                           result != 0))))))
    {
      result = duh_mode(player);
      sp_ability_trigger(player, event, (result != 0) + 1, TYPE_ANY, NULL, player, card);
    }
    return;
  }
  return;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22ca5c0; original cards/dominaria.c:20. */
static int dominaria_memorial_impl(int player, int card, event_t event, int x, int b, int u, int g,
                                   int r, int w, int target_in_grave, const char *label,
                                   target_definition_t *td, char *prompt,
                                   void (*impl)(int, int, target_definition_t *))
{
  enum
  {
    CHOICE_MANA = 1,
    CHOICE_SACRIFICE_FOR_EFFECT = 2
  };

  int lands_or_choice;
  int paying_or_available;
  int mana_available;
  int paying;
  card_instance_t *instance;
  unsigned int ability_available;
  const char *payment_mode;
  test_definition_t test;
  if ((event != EVENT_RESOLVE_ACTIVATION) &&
      (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE))
  {
    return mana_producer_tapped(player, card, event);
  }
  new_default_test_definition(&test, 2, "Select target creature card.");
  lands_or_choice = landsofcolor_controlled[player][7];
  paying_or_available = paying_mana();
  ability_available = 0;
  if (paying_or_available == 0)
  {
    if (target_in_grave == 0)
    {
      paying_or_available = generic_activated_ability(
          player, card, EVENT_CAN_ACTIVATE, ((td == NULL) ? 9U : 13U), x + 1,
          b, u, g, r, w, 0, td, prompt);
      ability_available = (unsigned int)(paying_or_available != 0);
    }
    else
    {
      paying_or_available = generic_activated_ability_with_target_in_grave(
          player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_UNTAPPED, x + 1, b, u, g, r, w,
          0, NULL, prompt, TYPE_NONE, &test, 8);
      ability_available = (unsigned int)(paying_or_available != 0);
    }
  }
  paying_or_available = paying_mana();
  mana_available = mana_producer(player, card, EVENT_CAN_ACTIVATE);
  paying = paying_mana();
  payment_mode = DLG_NO_DISPLAY_FOR_AI;
  if (paying == 0)
  {
    payment_mode = DLG_NO_OP;
  }
  lands_or_choice =
      dialog_impl(player, card, event, DLG_AUTOCHOOSE_IF_1, payment_mode, "Produce mana", mana_available,
                  paying_or_available, label, ability_available, lands_or_choice + -6, 0, 0, 0, 0);
  if (event == EVENT_CAN_ACTIVATE)
  {
    return lands_or_choice;
  }
  if (lands_or_choice == 0)
  {
    return 0;
  }
  if (lands_or_choice != CHOICE_SACRIFICE_FOR_EFFECT)
    return mana_producer_tapped(player, card, event);
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE)
  {
    (*impl)(player, card, td);
    return 0;
  }
  add_state(player, card, 0x10);
  charge_mana_for_activated_ability(player, card, x, b, u, g, r, w);
  if (target_in_grave == 0)
  {
    if (td != NULL)
    {
      if (spell_fizzled == 1)
      {
        remove_state(player, card, 0x10);
        tapped_for_mana_color = -1;
        return 0;
      }
      instance = get_card_instance(player, card);
      if ((td->zone == 0x1000) && (td->allowed_controller == 1 - player))
      {
        instance->targets[0].player = 1 - player;
        instance->targets[0].card = -1;
        instance->number_of_targets = '\x01';
      }
      else
      {
        instance->number_of_targets = '\0';
        pick_target(td, prompt);
      }
    }
  }
  else
  {
    if (spell_fizzled == 1)
    {
      remove_state(player, card, 0x10);
      tapped_for_mana_color = -1;
      return 0;
    }
    if (select_target_from_grave_source(player, card, player, SFG_0, 2, &test, 0) == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  if (spell_fizzled != 1)
  {
    kill_card(player, card, KILL_SACRIFICE);
    tapped_for_mana_color = -1;
    return 0;
  }
  remove_state(player, card, 0x10);
  tapped_for_mana_color = -1;
  return 0;
}
#endif

/* ManalinkEh 2022: 0x22cabe0; original cards/dominaria.c:99. */
int saga(int player, int card, event_t event, int chapters)
{
  int effective_chapters = chapters;
  int lore_count;
  int current_lore;
  unsigned int chapter_mask;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    ++hack_silent_counters;
    add_counters(player, card, player, card, COUNTER_LORE, 1);
    --hack_silent_counters;
    card_instance_t *instance = get_card_instance(player, card);
    instance->eot_toughness = 1;
  }
  else if (event == EVENT_PHASE_CHANGED && current_phase != 0x14)
  {
    card_instance_t *instance = get_card_instance(player, card);
    instance->eot_toughness = 0;
  }
  if (is_humiliated(player, card, AL_STATIC))
    effective_chapters = 0;
  if (event == EVENT_STATIC_EFFECTS)
  {
    lore_count = count_counters(player, card, COUNTER_LORE);
    if (effective_chapters <= lore_count)
      kill_card(player, card, KILL_SACRIFICE);
  }
  if (player != -1 && card != -1 && beginning_of_phase(player, card, event, player, card, 0x14))
  {
    add_counters(player, card, player, card, COUNTER_LORE, 1);
    lore_count = count_counters(player, card, COUNTER_LORE);
    if (effective_chapters <= lore_count)
      kill_card(player, card, KILL_SACRIFICE);
  }
  if (((event == EVENT_COUNTERS) && (counters_added_type == 0x41)) &&
      ((card == affected_card &&
        ((player == affected_card_controller &&
          (lore_count = is_humiliated(player, card, AL_TRIG), lore_count == 0))))))
  {

    if (counters_added == 1)
    {
      lore_count = count_counters(player, card, COUNTER_LORE);
      chapter_mask = 1 << ((unsigned char)lore_count & 0x1f);
    }
    else
    {

      lore_count = count_counters(player, card, COUNTER_LORE);
      chapter_mask = 0;
      for (; current_lore = count_counters(player, card, COUNTER_LORE),
             current_lore - counters_added < lore_count;
           lore_count = lore_count + -1)
      {
        chapter_mask = chapter_mask | 1 << ((unsigned char)lore_count & 0x1f);
      }
    }
    if ((chapter_mask & 2) != 0)
    {
      call_card_function(player, card, EVENT_CHAPTER1);
    }
    if ((chapter_mask & 4) != 0)
    {
      call_card_function(player, card, EVENT_CHAPTER2);
    }
    if (((chapter_mask & 8) != 0) && (2 < effective_chapters))
    {
      call_card_function(player, card, EVENT_CHAPTER3);
    }
    if (((chapter_mask & 0x10) != 0) && (3 < effective_chapters))
    {
      call_card_function(player, card, EVENT_CHAPTER4);
    }
  }
  return global_enchantment(player, card, event);
}

/* Academy Drake	|2|U
 * Creature - Drake 2/2
 * Kicker |4
 * Flying
 * If ~ was kicked, it enters the battlefield with two +1/+1 counters on it. */

/* Academy Journeymage	|4|U
 * Creature - Human Wizard 3/2
 * This spell costs |1 less to cast if you control a Wizard.
 * When ~ enters the battlefield, return target creature an opponent controls to its owner's hand. */

/* Adamant Will	|1|W
 * Instant
 * Target creature gets +2/+2 and gains indestructible until end of turn. */

/* ManalinkEh 2022: 0x22caf90; original cards/dominaria.c:163. */
int card_adeliz_the_cinder_wind(int player, int card, event_t event)
{
  /* Adeliz, the Cinder Wind	|1|U|R
   * Legendary Creature - Human Wizard 2/2
   * Flying, haste
   * Whenever you cast an instant or sorcery spell, Wizards you control get +1/+1 until end of turn. */

  test_definition_t wiz_test;

  check_legend_rule(player, card, event);
  haste(player, card, event);
  sp_ability_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  default_test_definition(&wiz_test, 2);
  wiz_test.subtype = 0xd6;
  pump_creatures_until_eot_merge_pt(player, card, player, 1, 1, &wiz_test);
  return 0;
}

/* Adventurous Impulse	|G
 * Sorcery
 * Look at the top three cards of your library. You may reveal a creature or land card from among them and put it into your hand. Put the rest on the bottom of your library in any order. */

/* Amaranthine Wall	|4
 * Artifact Creature - Wall 0/6
 * Defender
 * |2: ~ gains indestructible until end of turn. */

/* Ancient Animus	|1|G
 * Instant
 * Put a +1/+1 counter on target creature you control if it's legendary. Then it fights target creature an opponent controls. */

/* ManalinkEh 2022: 0x22cb060; original cards/dominaria.c:198. */
int card_arbor_armament(int player, int card, event_t event)
{
  /* Arbor Armament	|G
   * Instant
   * Put a +1/+1 counter on target creature. That creature gains reach until end of turn. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {

      instance = get_card_instance(player, card);
      pump_ability_until_eot(player, card, instance->targets[0].player,
                             instance->targets[0].card, 0, 0, 0x400, 0);
      add_counters(player, card, instance->targets[0].player,
                               instance->targets[0].card, COUNTER_P1_P1, 1);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* Arcane Flight	|U
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +1/+1 and has flying. */

/* ManalinkEh 2022: 0x22cb200; original cards/dominaria.c:229. */
int card_artificers_assistant(int player, int card, event_t event)
{
  /* Artificer's Assistant	|U
   * Creature - Bird 1/1
   * Flying
   * Whenever you cast a historic spell, scry 1. */

  cast_historic_spell(player, card, event, 2);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  scry(player, 1);
  return 0;
}

/* ManalinkEh 2022: 0x22cb240; original cards/dominaria.c:241. */
int card_arvad_the_cursed(int player, int card, event_t event)
{
  /* Arvad the Cursed	|3|W|B
   * Legendary Creature - Vampire Knight 3/3
   * Deathtouch, lifelink
   * Other legendary creatures you control get +2/+2. */

  check_legend_rule(player, card, event);
  deathtouch(player, card, event);
  lifelink(player, card, event);
  boost_creature_type(player, card, event, SUBTYPE_LEGEND, 2, 2, 0, BCT_CONTROLLER_ONLY);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22cb2d0; original cards/dominaria.c:255. */
int card_aryel_knight_of_windgrace(int player, int card, event_t event)
{
  /* Aryel, Knight of Windgrace	|2|W|B
   * Legendary Creature - Human Knight 4/4
   * Vigilance
   * |2|W, |T: Create a 2/2 |Swhite Knight creature token with vigilance.
   * |B, |T, Tap X untapped Knights you control: Destroy target creature with power X or less. */

  enum
  {
    CHOICE_TOKEN = 1,
    CHOICE_DESTROY = 2
  };

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  target_definition_t td1, td;
  default_target_definition(player, card, &td1, 2);
  td1.allowed_controller = player;
  td1.preferred_controller = player;
  td1.special = TARGET_SPECIAL_NOT_ME;
  td1.illegal_abilities = 0;
  td1.illegal_state = TARGET_STATE_TAPPED;
  td1.allow_cancel = 3;
  default_target_definition(player, card, &td, 2);
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    td.power_requirement = target_available(player, card, &td1) | 0x2000;
    if (generic_activated_ability(player, card, event, 1, 2, 0, 0, 0, 0, 1, 0, NULL, NULL))
      return 1;
    return generic_activated_ability(player, card, event, 5, 0, 1, 0, 0, 0, 0, 0, &td, NULL) !=
           0;
  }
  if (event == EVENT_ACTIVATE)
  {
    td.power_requirement = target_available(player, card, &td1) | 0x2000;
    instance->info_slot = 0;
    int ab1 = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 2, 0, 0, 0, 0, 1,
                                             0, NULL, NULL);
    int ab2 = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 1, 0, 0, 0, 0,
                                             0, &td, NULL);
    int choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Knight token",
                             ab1, 5, "Destroy a creature", ab2, 10, 0, 0, 0, 0);
    if (choice == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (choice == CHOICE_TOKEN)
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 2, 0, 0, 0, 0, 1, 0,
                                            NULL, NULL);
    if (choice != CHOICE_DESTROY)
      return 0;
    if (!charge_mana_for_activated_ability(player, card, 0, 1, 0, 0, 0, 0))
    {
      remove_state(player, card, 0x10);
      spell_fizzled = 1;
      return 0;
    }
    add_state(player, card, 0x10);
    tapped_for_mana_color = -1;
    int to_tap[target_available(player, card, &td1)];
    int ttc = 0;
    while (ttc < target_available(player, card, &td1) && can_target(&td1))
    {
      instance->number_of_targets = 0;
      if (!new_pick_target(&td1, "Select a Knight you control to tap.", 0, 0x80))
        break;
      to_tap[ttc++] = instance->targets[0].card;
      state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
      instance->number_of_targets = 0;
    }
    for (int i = 0; i < ttc; ++i)
    {
      state_untargettable(player, to_tap[i], 0);
      if (spell_fizzled == 1)
        remove_state(player, card, 0x10);
      else
        tap_card(player, to_tap[i]);
    }
    instance->number_of_targets = 0;
    td.power_requirement = ttc | 0x2000;
    char buffer[75];
    scnprintf(buffer, 75, "Select target creature with power less than or equal to %d", ttc);
    new_pick_target(&td, buffer, 0, 0x81);
    instance->eot_toughness = ttc;
    if (spell_fizzled != 1)
      return 0;
    for (int i = 0; i < ttc; ++i)
      remove_state(player, to_tap[i], 0x10);
    remove_state(player, card, 0x10);
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->info_slot == CHOICE_TOKEN)
    {
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_KNIGHT, &token);
      _Static_assert(offsetof(token_generation_t, s_key_plus) == 36,
                     "native token keyword word");
      token.s_key_plus = 8;
      generate_token(&token);
    }
    if (instance->info_slot == CHOICE_DESTROY)
    {
      td.power_requirement = instance->eot_toughness | 0x2000;
      if (validate_target(player, card, &td, 0))
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
    instance->eot_toughness = 0xffffffff;
  }
  return 0;
}
#endif

/* Aven Sentry	|3|W
 * Creature - Bird Soldier 3/2
 * Flying */

/* ManalinkEh 2022: 0x22cbb50; original cards/dominaria.c:390. */
int card_baird_steward_of_argive(int player, int card, event_t event)
{
  /* Baird, Steward of Argive	|2|W|W
   * Legendary Creature - Human Soldier 2/4
   * Vigilance
   * Creatures can't attack you or a planeswalker you control unless their controller pays |1 for each of those creatures. */

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  tax_attack(player, card, event);
  return 0;
}

/* Baloth Gorger	|2|G|G
 * Creature - Beast 4/4
 * Kicker |4
 * If ~ was kicked, it enters the battlefield with three +1/+1 counters on it. */

/* ManalinkEh 2022: 0x22cbba0; original cards/dominaria.c:406. */
int card_befuddle(int player, int card, event_t event)
{
  /* Befuddle	|2|U
   * Instant
   * Target creature gets -4/-0 until end of turn.
   * Draw a card. */

  int result;
  result = vanilla_instant_pump(player, card, event, 2, 1 - player, -4, 0, 0x1000000, 0);
  return result;
}

/* Benalish Honor Guard	|1|W
 * Creature - Human Knight 2/2
 * ~ gets +1/+0 for each legendary creature you control. */

/* ManalinkEh 2022: 0x22cbc00; original cards/dominaria.c:420. */
int card_benalish_marshal(int player, int card, event_t event)
{
  /* Benalish Marshal	|W|W|W
   * Creature - Human Knight 3/3
   * Other creatures you control get +1/+1. */

  boost_subtype(player, card, event, ~SUBTYPE_INVALID, 1, 1, 0, 0, BCT_CONTROLLER_ONLY);
  return 0;
}

/* ManalinkEh 2022: 0x22cbc60; original cards/dominaria.c:428. */
int card_blackblade_reforged(int player, int card, event_t event)
{
  /* Blackblade Reforged	|2
   * Legendary Artifact - Equipment
   * Equipped creature gets +1/+1 for each land you control.
   * Equip legendary creature |3
   * Equip |7 */

  card_instance_t *instance;
  int result;
  card_instance_t *equipment;
  int p_mod;
  unsigned int legendary_equip_available;
  unsigned int normal_equip_available;
  const char *prompt;
  target_definition_t *legendary_target;
  target_definition_t *equip_target;
  target_definition_t td;
  target_definition_t td1;

  check_legend_rule(player, card, event);
  instance = get_card_instance(player, card);
  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    equipment = in_play(player, card);
    if ((equipment != NULL) && (equipment = is_equipping(player, card), equipment != NULL))
    {
      result = count_subtype(player, 1, -1);
      p_mod = count_subtype(player, 1, -1);
      modify_pt_and_abilities((int)instance->damage_target_player, instance->damage_target_card,
                              event, p_mod, result, 0);
    }
    return 0;
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    default_target_definition(player, card, &td1, 2);
    td1.required_subtype = 0x2001;
    td1.allowed_controller = player;
    td1.preferred_controller = player;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x100, 7, 0, 0, 0, 0, 0,
                                         0, &td, NULL);
      if (result != 0)
      {
        return 1;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x100, 3, 0, 0, 0, 0, 0, 0,
                                       &td1, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {

      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 7, 0, 0, 0, 0, 0, 0,
                                         &td, NULL);
      normal_equip_available = 0;
      if (result != 0)
      {
        result = can_sorcery_be_played(player, EVENT_ACTIVATE);
        normal_equip_available = (unsigned int)(result != 0);
      }
      equip_target = &td;
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 3, 0, 0, 0, 0, 0, 0,
                                         &td1, NULL);
      legendary_equip_available = 0;
      if (result != 0)
      {
        result = can_sorcery_be_played(player, EVENT_ACTIVATE);
        legendary_equip_available = (unsigned int)(result != 0);
      }
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Normal equip",
                           normal_equip_available, 1, "Legendary equip", legendary_equip_available,
                           5, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == 1)
      {
        prompt = "Select target creature you control.";
        result = 7;
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 0x102, result, 0, 0, 0, 0, 0,
                                         0, equip_target, prompt);
      }
      if (result == 2)
      {
        prompt = "Select target legendary creature you control.";
        result = 3;
        equip_target = &td1;
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 0x102, result, 0, 0, 0, 0, 0,
                                         0, equip_target, prompt);
      }
      return generic_activated_ability(player, card, event, 0x102, 7, 0, 0, 0, 0, 0, 0, &td,
                                       "Select target creature you control.");
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    default_target_definition(player, card, &td1, 2);
  }
  equip_target = &td;
  legendary_target = &td1;
  td1.allowed_controller = player;
  td1.required_subtype = 0x2001;
  td1.preferred_controller = td1.allowed_controller;
  if (!(event != EVENT_RESOLVE_ACTIVATION))
  {
    result = instance->info_slot;
    bool equip = false;
    if (result == 1)
    {
      if (validate_target(player, card, equip_target, 0))
        equip = true;
      else
        result = instance->info_slot;
    }
    if (!equip && result == 2 && validate_target(player, card, legendary_target, 0))
      equip = true;
    if (equip)
      equip_target_creature(instance->parent_controller, instance->parent_card,
                            instance->targets[0].player, instance->targets[0].card);
  }
  equip_target = &td;
  return generic_activated_ability(player, card, event, 0x102, 7, 0, 0, 0, 0, 0, 0, equip_target,
                                   "Select target creature you control.");
}

/* Blessed Light	|4|W
 * Instant
 * Exile target creature or enchantment. */

/* Blessing of Belzenlok	|B
 * Instant
 * Target creature gets +2/+1 until end of turn. If it's legendary, it also gains lifelink until end of turn. */

/* Blink of an Eye	|1|U
 * Instant
 * Kicker |1|U
 * Return target nonland permanent to its owner's hand. If this spell was kicked, draw a card. */

/* Bloodstone Goblin	|1|R
 * Creature - Goblin Warrior 2/2
 * Whenever you cast a spell, if that spell was kicked, ~ gets +1/+1 and gains menace until end of turn. */

/* Bloodtallow Candle	|1
 * Artifact
 * |6, |T, Sacrifice ~: Target creature gets -5/-5 until end of turn. */

/* ManalinkEh 2022: 0x22cc2b0; original cards/dominaria.c:514. */
int card_board_the_weatherlight(int player, int card, event_t event)
{
  /* Board the Weatherlight	|1|W
   * Sorcery
   * Look at the top five cards of your library. You may reveal a historic card from among them and put it into your hand. Put the rest on the bottom of your library in a random order. */

  int result;
  int revealed_card;
  test_definition_t this_test;

  if (event == EVENT_CAN_CAST)
    return basic_spell(player, card, event);
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
    return basic_spell(player, card, event);
  }
  if (event != EVENT_RESOLVE_SPELL)
  {
    if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
    return basic_spell(player, card, event);
  }

  result = count_deck(player);
  if (result != 0)
  {
    if (5 < result)
    {
      result = 5;
    }

    new_default_test_definition(&this_test, 0xff, "Select an historic card.");
    this_test.create_minideck = (unsigned short)result;
    this_test.special_selection_function = is_historic_by_internal_id;
    this_test.no_shuffle = 1;
    revealed_card = new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
    bool reveal = false;
    if (ai_is_speculating == 1 || (player == 1 && !((unsigned char)trace_mode & 2)))
    {
      if (revealed_card < 0)
      {
        this_test.type = 0xff;
        revealed_card = new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
        reveal = revealed_card >= 0;
      }
      else
        reveal = true;
    }
    else
      reveal = revealed_card >= 0;
    if (reveal)
    {
      reveal_card(player, card, player, revealed_card);
      result = result + -1;
    }
    if (0 < result)
    {
      put_top_x_on_bottom_in_random_order(player, result);
    }
  }
  kill_card(player, card, KILL_DESTROY);
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x22cc4a0; original cards/dominaria.c:550. */
int card_broken_bond(int player, int card, event_t event)
{
  /* Broken Bond	|1|G
   * Sorcery
   * Destroy target artifact or enchantment. You may put a land card from your hand onto the battlefield. */

  card_instance_t *instance;
  target_definition_t td;
  test_definition_t test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x44);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (validate_target(player, card, &td, 0))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      new_default_test_definition(&test, 1, "Select a land card.");
      new_global_tutor(player, player, 4, 4, 0, 1, &test);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "DISENCHANT", 1, NULL);
}

/* Cabal Evangel	|1|B
 * Creature - Human Cleric 2/2 */

/* Cabal Paladin	|3|B
 * Creature - Human Knight 4/2
 * Whenever you cast a historic spell, ~ deals 2 damage to each opponent. */

/* ManalinkEh 2022: 0x22cc670; original cards/dominaria.c:583. */
int card_cabal_stronghold(int player, int card, event_t event)
{
  /* Cabal Stronghold	""
   * Land
   * |T: Add |C.
   * |3, |T: Add |B for each basic |H2Swamp you control. */

  enum { CHOICE_COLORLESS = 1, CHOICE_SWAMPS = 2 };

  if (event == EVENT_RESOLVE_SPELL)
  {
    get_card_instance(player, card)->info_slot = 1;
    play_land_sound_effect_force_color(player, card, COLOR_TEST_BLACK);
    count_mana();
  }
  else if (event == EVENT_CHANGE_TYPE)
  {
    int available = has_mana(player, COLOR_COLORLESS, 4);
    get_card_instance(player, card)->info_slot = available ? 2 : 1;
  }

  if (event != EVENT_ACTIVATE)
  {
    if (event == EVENT_RESOLVE_ACTIVATION || event == EVENT_CLEANUP)
    {
      get_card_instance(player, card);
      return 0;
    }
    return mana_producer(player, card, event);
  }

  card_instance_t *instance = get_card_instance(player, card);
  if (paying_mana() || !generic_activated_ability(player, card, EVENT_CAN_ACTIVATE,
                                                GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL))
  {
    instance->info_slot = 1;
    produce_mana_tapped(player, card, COLOR_COLORLESS, 1);
    return 0;
  }

  int speculating = ai_is_speculating == 1;
  instance->info_slot = 2;
  if (!speculating && (player != AI || ((unsigned char)trace_mode & 2)))
  {
    int swamps = count_basic_lands(player, get_hacked_subtype(player, card, SUBTYPE_SWAMP));
    int coffers_available = !is_tapped(player, card) && !is_animated_and_sick(player, card) &&
                            can_produce_mana(player, card) && has_mana(player, COLOR_COLORLESS, 4);
    int colorless_available = !is_tapped(player, card) && !is_animated_and_sick(player, card) &&
                              can_produce_mana(player, card);
    int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1,
                        "Produce colorless mana", colorless_available, 5,
                        "Raid the Coffers", coffers_available, swamps > 3 ? 10 : 0);
    if (!choice)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (choice == CHOICE_COLORLESS)
    {
      // The release leaves info_slot at 2 on this dialog branch.
      produce_mana_tapped(player, card, COLOR_COLORLESS, 1);
      return 0;
    }
    if (choice != CHOICE_SWAMPS)
      return 0;
  }

  add_state(player, card, STATE_TAPPED);
  charge_mana(player, COLOR_ANY, 3);
  if (spell_fizzled == 1)
  {
    remove_state(player, card, STATE_TAPPED);
    return 0;
  }
  // Charging can change the battlefield; count again rather than reuse the dialog count.
  int swamps = count_basic_lands(player, get_hacked_subtype(player, card, SUBTYPE_SWAMP));
  produce_mana_tapped_impl(player, card, COLOR_BLACK, swamps, 0, 1);
  ai_modifier += swamps >= 4 ? 48 : -96;
  return 0;
}

/* Caligo Skin-Witch	|1|B
 * Creature - Human Wizard 1/3
 * Kicker |3|B
 * When ~ enters the battlefield, if it was kicked, each opponent discards two cards. */

/* Call the Cavalry	|3|W
 * Sorcery
 * Create two 2/2 |Swhite Knight creature tokens with vigilance. */

/* ManalinkEh 2022: 0x22ccaa0; original cards/dominaria.c:662. */
int card_cast_down(int player, int card, event_t event)
{
  /* Cast Down	|1|B
   * Instant
   * Destroy target nonlegendary creature. */

  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.required_subtype = 0x2001;
  td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
  if (event == EVENT_RESOLVE_SPELL)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (validate_target(player, card, &td, 0))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target non-Legendary creature.", 1,
                         NULL);
}

/* ManalinkEh 2022: 0x22ccc20; original cards/dominaria.c:685. */
int card_chainers_torment(int player, int card, event_t event)
{
  /* Chainer's Torment	|3|B
   * Enchantment - Saga
   * I, II - ~ deals 2 damage to each opponent and you gain 2 life.
   * III - Create an X/X |Sblack Nightmare Horror creature token, where X is half your life total, rounded up. It deals X damage to you. */

  if ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CHAPTER1)
  {
    damage_player(1 - player, 2, player, card);
    gain_life(player, 2);
  }
  else if (event == EVENT_CHAPTER3)
  {
    unsigned int x = round_up_value(life[player]);
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_NIGHTMARE_HORROR, &token);
    token.keep_track_of_tokens_generated = 10;
    token.pow = x;
    token.tou = x;
    generate_token(&token);
    for (int i = 0; i < 9; ++i)
    {
      card_instance_t *instance = in_play(player, card);
      if (instance && instance->targets[i].player != -1 && instance->targets[i].card != -1)
        damage_player(player, x, instance->targets[i].player, instance->targets[i].card);
    }
  }
  return saga(player, card, event, 3);
}

/* Nightmare Horror	""(token)
 * Creature - Nightmare Horror 100/100 */

/* Champion of the Flame	|1|R
 * Creature - Human Warrior 1/1
 * Trample
 * ~ gets +2/+2 for each Aura and Equipment attached to it. */

/* Chandra's Outburst	|3|R|R
 * Sorcery
 * ~ deals 4 damage to target player or planeswalker.
 * Search your library and/or graveyard for a card named Chandra, Bold Pyromancer, reveal it, and put it into your hand. If you search your library this way, shuffle it. */

/* Chandra, Bold Pyromancer	|4|R|R
 * Legendary Planeswalker - Chandra (5)
 * +1: Add |R|R. ~ deals 2 damage to target player.
 * -3: ~ deals 3 damage to target creature or planeswalker.
 * -7: ~ deals 10 damage to target player and each creature and planeswalker they control. */

/* Charge	|W
 * Instant
 * Creatures you control get +1/+1 until end of turn. */

/* Cloudreader Sphinx	|4|U
 * Creature - Sphinx 3/4
 * Flying
 * When ~ enters the battlefield, scry 2. */

/* Cold-Water Snapper	|5|U
 * Creature - Turtle 4/5
 * Hexproof */

/* Corrosive Ooze	|1|G
 * Creature - Ooze 2/2
 * Whenever ~ blocks or becomes blocked by an equipped creature, destroy all Equipment attached to that creature at end of combat. */

/* ManalinkEh 2022: 0x22ccd40; original cards/dominaria.c:748. */
int card_curators_ward(int player, int card, event_t event)
{
  /* Curator's Ward	|2|U
   * Enchantment - Aura
   * Enchant permanent
   * Enchanted permanent has hexproof.
   * When enchanted permanent leaves the battlefield, if it was historic, draw two cards. */

  card_instance_t *ward;
  card_instance_t *source_in_play;
  int result;
  target_definition_t td;

  ward = get_card_instance(player, card);
  other_leaves_play(player, card, (intptr_t)ward->damage_target_player, ward->damage_target_card, event,
                    1, 1);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    result = is_what((intptr_t)ward->damage_target_player, ward->damage_target_card, 0x40);
    if (((result != 0) ||
         (result = is_legendary((intptr_t)ward->damage_target_player, ward->damage_target_card),
          result != 0)) ||
        (result =
             has_subtype((intptr_t)ward->damage_target_player, ward->damage_target_card, SUBTYPE_SAGA),
         result != 0))
    {
      draw_cards(player, 2);
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
  if (event == EVENT_ABILITIES)
  {
    if (((ward->damage_target_card == affected_card) &&
         (ward->damage_target_player == affected_card_controller)) &&
        ((source_in_play = in_play(player, card),
          source_in_play != NULL &&
              (result = is_humiliated(player, card, AL_STATIC), result == 0))))
    {
      hexproof((intptr_t)ward->damage_target_player, ward->damage_target_card, EVENT_ABILITIES);
    }
  }
  default_target_definition(player, card, &td, 0x47);
  td.preferred_controller = player;
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_PERMANENT", 1, NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      result = get_cmc(ward->targets[0].player, ward->targets[0].card);
      ai_modifier = ai_modifier + result * 8;
      return 0;
    }
  }
  return targeted_aura(player, card, event, &td, "TARGET_PERMANENT");
}

/* D'Avenant Trapper	|2|W
 * Creature - Human Archer 3/2
 * Whenever you cast a historic spell, tap target creature an opponent controls. */

/* ManalinkEh 2022: 0x22cd050; original cards/dominaria.c:786. */
int card_damping_sphere(int player, int card, event_t event)
{
  /* Damping Sphere	|2
   * Artifact
   * If a land is tapped for two or more mana, it produces |C instead of any other type and amount.
   * Each spell a player casts costs |1 more to cast for each other spell that player has cast this turn. */

  card_instance_t *instance;
  int result;

  if (event == EVENT_MODIFY_MANA_PROD)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        multi_mana_transmutation[1][0] = 1;
        multi_mana_transmutation[0][0] = 1;
        multi_mana_transmutation[1][1] = 1;
        multi_mana_transmutation[0][1] = 1;
      }
    }
  }
  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((event == EVENT_MODIFY_COST_GLOBAL) && (result == 0))
    {
      result = is_what(affected_card_controller, affected_card, 1);
      if (result == 0)
      {
        result = get_specific_storm_count(affected_card_controller);
        COST_COLORLESS = COST_COLORLESS + (char)result;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22cd150; original cards/dominaria.c:804. */
int card_danitha_capashen_paragon(int player, int card, event_t event)
{
  /* Danitha Capashen, Paragon	|2|W
   * Legendary Creature - Human Knight 2/2
   * First strike, vigilance, lifelink
   * Aura and Equipment spells you cast cost |1 less to cast. */

  int result;

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  lifelink(player, card, event);
  if ((((event == EVENT_MODIFY_COST_GLOBAL) && (affected_card_controller == player)) &&
       (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
      ((result = has_subtype(affected_card_controller, affected_card, SUBTYPE_EQUIPMENT),
        result != 0 || (result = has_subtype(affected_card_controller, affected_card, SUBTYPE_AURA),
                        result != 0))))
  {
    COST_COLORLESS = COST_COLORLESS + -1;
  }
  return 0;
}

/* Darigaaz Reincarnated	|4|B|R|G
 * Legendary Creature - Dragon 7/7
 * Flying, trample, haste
 * If ~ would die, instead exile it with three egg counters on it.
 * At the beginning of your upkeep, if ~ is exiled with an egg counter on it, remove an egg counter from it. Then if ~ has no egg counters on it, return it to the battlefield. */

/* Daring Archaeologist	|3|W
 * Creature - Human Artificer 3/3
 * When ~ enters the battlefield, you may return target artifact card from your graveyard to your hand.
 * Whenever you cast a historic spell, put a +1/+1 counter on ~. */

/* Dark Bargain	|3|B
 * Instant
 * Look at the top three cards of your library. Put two of them into your hand and the other into your graveyard. ~ deals 2 damage to you. */

/* ManalinkEh 2022: 0x22cd220; original cards/dominaria.c:838. */
int card_dauntless_bodyguard(int player, int card, event_t event)
{
  /* Dauntless Bodyguard	|W
   * Creature - Human Knight 2/1
   * As ~ enters the battlefield, choose another creature you control.
   * Sacrifice ~: The chosen creature gains indestructible until end of turn. */

  int card_00;
  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_RESOLVE_SPELL)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.special = TARGET_SPECIAL_NOT_ME;
    td.allow_cancel = 0;
    td.illegal_abilities = 0;
    result = can_target(&td);
    if (result != 0)
    {
      new_pick_target(&td, "TARGET_ANOTHER_CREATURE_CONTROL", 9, 1);
    }
    return 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event != EVENT_RESOLVE_ACTIVATION)
    return generic_activated_ability(player, card, event, 8, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  instance = get_card_instance(player, card);
  result = instance->targets[9].player;
  instance = get_card_instance(player, card);
  card_00 = instance->targets[9].card;
  instance = in_play(result, card_00);
  if (instance != NULL)
  {
    pump_ability_until_eot(player, card, result, card_00, 0, 0, 0, 0x800);
  }
  return generic_activated_ability(player, card, event, 8, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* Deathbloom Thallid	|2|B
 * Creature - Fungus 3/2
 * When ~ dies, create a 1/1 |Sgreen Saproling creature token. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22cd3d0; original cards/dominaria.c:877. */
int card_deep_freeze(int player, int card, event_t event)
{
  /* Deep Freeze	|2|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has base power and toughness 0/4, has defender, loses all other abilities, and is a |Sblue Wall in addition to its other colors and types. */

  int result;

  attached_set_pt(player, card, event, 0, 4);
  attached_is_additional_color(player, card, event, COLOR_TEST_BLUE);
  attached_is_additional_subtype(player, card, event, SUBTYPE_WALL);
  disable_attached(player, card, event, 0x17e);
  result = aura(player, card, event, NULL, NULL);
  disabling_aura_ai(player, card, event);
  return result;
}
#endif

/* Demonic Vigor	|B
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +1/+1.
 * When enchanted creature dies, return that card to its owner's hand. */

/* ManalinkEh 2022: 0x22cd490; original cards/dominaria.c:899. */
int card_demonlord_belzenlok(int player, int card, event_t event)
{
  /* Demonlord Belzenlok	|4|B|B
   * Legendary Creature - Elder Demon 6/6
   * Flying, trample
   * When ~ enters the battlefield, exile cards from the top of your library until you exile a nonland card, then put that card into your hand. If the card's converted mana cost is 4 or greater, repeat this process. ~ deals 1 damage to you for each card put into your hand this way. */

  int result;
  int argument2;
  test_definition_t test;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }

  argument2 = 0;
  default_test_definition(&test, 1);
  test.type_flag = 1;
  do
  {
    result = reveal_card_until_you_reveal_test(player, player, &test, 0x228);
    if (result < 0)
      break;

    result = get_cmc(player, result);
    argument2 = argument2 + 1;
  } while (3 < result);
  damage_player(player, argument2, player, card);
  return 0;
}

/* Diligent Excavator	|1|U
 * Creature - Human Artificer 1/3
 * Whenever you cast a historic spell, target player puts the top two cards of their library into their graveyard. */

/* ManalinkEh 2022: 0x22cd560; original cards/dominaria.c:937. */
int card_divest(int player, int card, event_t event)
{
  /* Divest	|B
   * Sorcery
   * Target player reveals their hand. You choose an artifact or creature card from it. That player discards that card. */

  card_instance_t *instance;
  ec_definition_t this_definition;
  target_definition_t td;
  test_definition_t this_test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      new_default_test_definition(&this_test, 0x42, "Select target artifact or creature.");
      default_ec_definition(instance->targets[0].player, player, &this_definition);
      new_effect_coercion(&this_definition, &this_test);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}

/* Dread Shade	|B|B|B
 * Creature - Shade 3/3
 * |B: ~ gets +1/+1 until end of turn. */

/* Drudge Sentinel	|2|B
 * Creature - Skeleton Warrior 2/1
 * |3: Tap ~. It gains indestructible until end of turn. */

/* Dub	|2|W
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+2, has first strike, and is a Knight in addition to its other types. */

/* Elfhame Druid	|1|G
 * Creature - Elf Druid 0/2
 * |T: Add |G.
 * |T: Add |G|G. Spend this mana only to cast kicked spells. */

/* Eviscerate	|3|B
 * Sorcery
 * Destroy target creature. */

/* ManalinkEh 2022: 0x22cd720; original cards/dominaria.c:989. */
int card_evra_halcyon_witness(int player, int card, event_t event)
{
  /* Evra, Halcyon Witness	|4|W|W
   * Legendary Creature - Avatar 4/4
   * Lifelink
   * |4: Exchange your life total with ~'s power. */

  card_instance_t *instance;
  int pow;
  card_instance_t *source_record;
  card_instance_t *parent;

  check_legend_rule(player, card, event);
  if (event == EVENT_POWER)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (source_record = get_card_instance(player, card), -1 < source_record->targets[1].card))
    {
      instance = get_card_instance(player, card);
      event_result = instance->targets[1].card;
    }
    return 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event != EVENT_RESOLVE_ACTIVATION)
    return generic_activated_ability(player, card, event, 0, 4, 0, 0, 0, 0, 0, 0, NULL, NULL);
  instance = get_card_instance(player, card);
  parent = in_play(instance->parent_controller, instance->parent_card);
  if (parent != NULL)
  {

    pow = get_power(instance->parent_controller, instance->parent_card);
    parent->targets[1].card = life[player];
    set_life_total(player, pow);
  }
  return generic_activated_ability(player, card, event, 0, 4, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* Excavation Elephant	|4|W
 * Creature - Elephant 3/5
 * Kicker |1|W
 * When ~ enters the battlefield, if it was kicked, return target artifact card from your graveyard to your hand. */

/* ManalinkEh 2022: 0x22cd8b0; original cards/dominaria.c:1021. */
int card_fall_of_the_thran(int player, int card, event_t event)
{
  /* Fall of the Thran	|5|W
   * Enchantment - Saga
   * I - Destroy all lands.
   * II, III - Each player returns two land cards from their graveyard to the battlefield. */

  int result;
  unsigned int player_pass;
  unsigned int player_00;
  test_definition_t this_test;

  if (event == EVENT_CHAPTER1)
  {
    default_test_definition(&this_test, 1);
    new_manipulate_all(player, card, 2, &this_test, ACT_KILL_DESTROY);
  }
  else if (((uint32_t)(event - EVENT_CHAPTER2) & 0xfffffeff) == 0)
  {

    player_pass = 0;
    do
    {
      player_00 = current_turn ^ player_pass;
      result = graveyard_has_shroud(player_00);
      if (result == 0)
      {
        new_default_test_definition(&this_test, 1, "Select target land card.");
        this_test.qty = 2;
        new_global_tutor(player_00, player_00, 2, 4, 0, 1, &this_test);
      }
      player_pass = player_pass + 1;
    } while (player_pass != 2);
  }
  return saga(player, card, event, 3);
}

/* Feral Abomination	|5|B
 * Creature - Thrull 5/5
 * Deathtouch */

/* Fervent Strike	|R
 * Instant
 * Target creature gets +1/+0 and gains first strike and haste until end of turn. */

/* Fiery Intervention	|4|R
 * Sorcery
 * Choose one -
 * * ~ deals 5 damage to target creature.
 * * Destroy target artifact. */

/* ManalinkEh 2022: 0x22cd9d0; original cards/dominaria.c:1061. */
int card_fight_with_fire(int player, int card, event_t event)
{
  /* Fight with Fire	|2|R
   * Sorcery
   * Kicker |5|R
   * ~ deals 5 damage to target creature. If this spell was kicked, it deals 10 damage divided as you choose among any number of targets instead. */

  int available_or_index;
  card_instance_t *instance;
  int selected_or_target_card;
  card_instance_t *target_record;
  target_t *target_cursor;
  target_definition_t td;
  target_definition_t tdk;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 2;
    default_target_definition(player, card, &tdk, 0x102);
    tdk.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    get_card_instance(player, card);
    available_or_index = has_mana_multi(player, 7, 0, 0, 0, 2, 0);
    if ((available_or_index != 0) &&
        (available_or_index =
             generic_spell(player, card, EVENT_CAN_CAST, 1, &tdk, "TARGET_ANY", 1, NULL),
         available_or_index != 0))
    {
      return 1;
    }
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = 2;
      default_target_definition(player, card, &tdk, 0x102);
      tdk.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        if (do_kicker(player, card, 5, 0, 0, 0, 1, 0) == 0)
        {
          generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
        }
        else
        {
          available_or_index = 0;
          instance->number_of_targets = '\0';
          while (selected_or_target_card =
                     new_pick_target(&tdk, "TARGET_ANY", available_or_index, 1),
                 selected_or_target_card != 0)
          {
            selected_or_target_card = instance->targets[available_or_index].card;
            if (selected_or_target_card != -1)
            {
              target_record = get_card_instance(instance->targets[available_or_index].player,
                                                selected_or_target_card);
              target_record->state = target_record->state | 0x200000;
            }
            available_or_index = available_or_index + 1;
            if (available_or_index == 10)
            {
              target_cursor = instance->targets;
              do
              {
                if (target_cursor->card != -1)
                {
                  target_record = get_card_instance(target_cursor->player, target_cursor->card);
                  target_record->state = target_record->state & 0xffdfffff;
                }
                target_cursor = target_cursor + 1;
              } while (instance->targets + 10 != target_cursor);
              return basic_spell(player, card, EVENT_CAST_SPELL);
            }
          }
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 2;
    default_target_definition(player, card, &tdk, 0x102);
    tdk.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (kicked(player, card) == 0)
      {
        if (valid_target(&td) != 0)
        {
          damage_target0(player, card, 5);
        }
      }
      else
      {
        divide_damage(player, card, &tdk);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 2;
    default_target_definition(player, card, &tdk, 0x102);
    tdk.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x22cde70; original cards/dominaria.c:1122. */
int card_final_parting(int player, int card, event_t event)
{
  /* Final Parting	|3|B|B
   * Sorcery
   * Search your library for two cards. Put one into your hand and the other into your graveyard. Then shuffle your library. */

  int result;
  test_definition_t this_test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    new_default_test_definition(&this_test, 0xff, "Select a card to put into your hand.");
    this_test.no_shuffle = 1;
    new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
    new_default_test_definition(&this_test, 0xff, "Select a card to put into your graveyard.");
    new_global_tutor(player, player, 1, 3, 0, 2, &this_test);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* Firefist Adept	|4|R
 * Creature - Human Wizard 3/3
 * When ~ enters the battlefield, it deals X damage to target creature an opponent controls, where X is the number of Wizards you control. */

/* ManalinkEh 2022: 0x22cdf60; original cards/dominaria.c:1144. */
int card_firesong_and_sunspeaker(int player, int card, event_t event)
{
  /* Firesong and Sunspeaker	|4|R|W
   * Legendary Creature - Minotaur Cleric 4/6
   * |SRed instant and sorcery spells you control have lifelink.
   * Whenever a |Swhite instant or sorcery spell causes you to gain life, ~ deals 3 damage to target creature or player. */

  int card_00;
  card_instance_t *instance;
  int result;
  unsigned int spell_color;
  unsigned int required_color;
  card_instance_t *spell_record;
  target_definition_t td;

  check_legend_rule(player, card, event);
  instance = in_play(player, card);
  if (event == EVENT_CAST_SPELL)
  {
    result = is_what(affected_card_controller, affected_card, 0x38);
    if (result != 0)
    {
      spell_color = get_color(affected_card_controller, affected_card);
      required_color = get_sleighted_color_test(player, card, 0x20);
      if ((spell_color & required_color) != 0)
      {
        instance->targets[9].player = affected_card_controller;
        instance->targets[9].card = affected_card;
      }
    }
  }
  if (trigger_gain_life(player, card, event, player, RESOLVE_TRIGGER_MANDATORY))
  {
    result = instance->targets[9].player;

    if ((((-1 < result) && (card_00 = instance->targets[9].card, card_00 != -1)) &&
         (spell_record = in_play(result, card_00), spell_record != NULL)) &&
        ((spell_record->state & 2) != 0))
    {
      default_target_definition(player, card, &td, 0xff);
      td.illegal_type = TARGET_TYPE_PLANESWALKER;
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td.allow_cancel = 0;
      result = can_target(&td);
      if ((result != 0) && (result = pick_target(&td, "TARGET_CREATURE_OR_PLAYER"), result != 0))
      {
        damage_target0(player, card, 3);
      }
      instance->targets[9].player = -1;
      instance->targets[9].card = -1;
    }
  }
  if ((event == EVENT_STATIC_EFFECTS) && (stack_size == 0))
  {
    instance->targets[9].player = -1;
    instance->targets[9].card = -1;
    return 0;
  }
  return 0;
}

/* Forebear's Blade	|3
 * Artifact - Equipment
 * Equipped creature gets +3/+0 and has vigilance and trample.
 * Whenever equipped creature dies, attach ~ to target creature you control.
 * Equip |3 */

/* Frenzied Rage	|1|R
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+1 and has menace. */

/* Fungal Infection	|B
 * Instant
 * Target creature gets -1/-1 until end of turn. Create a 1/1 |Sgreen Saproling creature token. */

/* Fungal Plots	|1|G
 * Enchantment
 * |1|G, Exile a creature card from your graveyard: Create a 1/1 |Sgreen Saproling creature token.
 * Sacrifice two Saprolings: You gain 2 life and draw a card. */

/* Gaea's Protector	|3|G
 * Creature - Elemental Warrior 4/2
 * ~ must be blocked if able. */

/* ManalinkEh 2022: 0x22ce170; original cards/dominaria.c:1210. */
int card_garna_the_bloodflame(int player, int card, event_t event)
{
  /* Garna, the Bloodflame	|3|B|R
   * Legendary Creature - Human Warrior 3/3
   * Flash
   * When ~ enters the battlefield, return to your hand all creature cards in your graveyard that were put there from anywhere this turn.
   * Other creatures you control have haste. */

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  boost_subtype(player, card, event, ~SUBTYPE_INVALID, 0, 0, 0, 4, BCT_CONTROLLER_ONLY);
  if (event == EVENT_ETB)
  {
    return_all_dead_this_turn_to_hand(player, 2);
  }
  return flash(player, card, event);
}

/* Ghitu Chronicler	|1|R
 * Creature - Human Wizard 1/3
 * Kicker |3|R
 * When ~ enters the battlefield, if it was kicked, return target instant or sorcery card from your graveyard to your hand. */

/* Ghitu Journeymage	|2|R
 * Creature - Human Wizard 3/2
 * When ~ enters the battlefield, if you control another Wizard, ~ deals 2 damage to each opponent. */

/* ManalinkEh 2022: 0x22ce220; original cards/dominaria.c:1236. */
int card_ghitu_lavarunner(int player, int card, event_t event)
{
  /* Ghitu Lavarunner	|R
   * Creature - Human Wizard 1/2
   * As long as there are two or more instant and/or sorcery cards in your graveyard, ~ gets +1/+0 and has haste. */

  card_instance_t *instance;
  int result;

  if (((uint32_t)(event - EVENT_POWER) & 0xfffffffd) == 0)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {
      result = count_graveyard_by_type(player, 0x38);
      if (1 < result)
      {
        if (event == EVENT_POWER)
        {
          event_result = event_result + 1;
        }
        else
        {
          haste(player, card, EVENT_ABILITIES);
        }
      }
    }
  }
  return 0;
}

/* Gift of Growth	|1|G
 * Instant
 * Kicker |2
 * Untap target creature. It gets +2/+2 until end of turn. If this spell was kicked, that creature gets +4/+4 until end of turn instead. */

/* Goblin Barrage	|3|R
 * Sorcery
 * Kicker-Sacrifice an artifact or Goblin.
 * ~ deals 4 damage to target creature. If this spell was kicked, it also deals 4 damage to target player or planeswalker. */

/* ManalinkEh 2022: 0x22ce2b0; original cards/dominaria.c:1261. */
int card_goblin_chainwhirler(int player, int card, event_t event)
{
  /* Goblin Chainwhirler	|R|R|R
   * Creature - Goblin Warrior 3/3
   * First strike
   * When ~ enters the battlefield, it deals 1 damage to each opponent and each creature and planeswalker they control. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  new_damage_all(player, card, 1 - player, 1, 0x23, NULL);
  return 0;
}

/* Grand Warlord Radha	|2|R|G
 * Legendary Creature - Elf Warrior 3/4
 * Haste
 * Whenever one or more creatures you control attack, add that much mana in any combination of |R and/or |G. Until end of turn, you don't lose this mana as steps and phases end. */

/* ManalinkEh 2022: 0x22ce330; original cards/dominaria.c:1280. */
int card_grow_from_the_ashes(int player, int card, event_t event)
{
  /* Grow from the Ashes	|2|G
   * Sorcery
   * Kicker |2
   * Search your library for a basic land card, put it onto the battlefield, then shuffle your library. If this spell was kicked, instead search your library for two basic land cards, put them onto the battlefield, then shuffle your library. */

  int result;
  test_definition_t this_test;
  kicker(player, card, event, 2, 0, 0, 0, 0, 0);
  if (event == EVENT_RESOLVE_SPELL)
  {
    new_default_test_definition(&this_test, 1, "Select a basic land card.");
    this_test.subtype = 0x2000;
    result = kicked(player, card);
    if (result != 0)
    {
      this_test.qty = 2;
    }
    new_global_tutor(player, player, 1, 4, 0, 5, &this_test);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22ce440; original cards/dominaria.c:1302. */
int card_grunn_the_lonely_king(int player, int card, event_t event)
{
  /* Grunn, the Lonely King	|4|G|G
   * Legendary Creature - Ape Warrior 5/5
   * Kicker |3
   * If ~ was kicked, it enters the battlefield with five +1/+1 counters on it.
   * Whenever ~ attacks alone, double its power and toughness until end of turn. */

  int result;
  int power;

  check_legend_rule(player, card, event);
  kicker(player, card, event, 3, 0, 0, 0, 0, 0);
  result = kicked(player, card);
  if (result != 0)
  {
    enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 5);
  }
  dat_ability(player, card, event, DAT_ATTACKS_ALONE | DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }
  result = get_toughness(player, card);
  power = get_power(player, card);
  pump_until_eot_merge_previous(player, card, player, card, power, result);
  return 0;
}

/* Guardians of Koilos	|5
 * Artifact Creature - Construct 4/4
 * When ~ enters the battlefield, you may return another target historic permanent you control to its owner's hand. */

/* Hallar, the Firefletcher	|1|R|G
 * Legendary Creature - Elf Archer 3/3
 * Trample
 * Whenever you cast a spell, if that spell was kicked, put a +1/+1 counter on ~, then ~ deals damage equal to the number of +1/+1 counters on it to each opponent. */

/* Haphazard Bombardment	|5|R
 * Enchantment
 * When ~ enters the battlefield, choose four nonenchantment permanents you don't control and put an aim counter on each of them.
 * At the beginning of your end step, if two or more permanents you don't control have an aim counter on them, destroy one of those permanents at random. */

/* ManalinkEh 2022: 0x22ca270; original cards/dominaria.c:1340. */
static int give_token_haste_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {
    instance = get_card_instance(player, card);
    haste((int)instance->damage_target_player, instance->damage_target_card, event);
  }
  return 0;
}

/* Healing Grace	|W
 * Instant
 * Prevent the next 3 damage that would be dealt to any target this turn by a source of your choice. You gain 3 life. */

/* ManalinkEh 2022: 0x22c95e0; original cards/dominaria.c:1352. */
static void give_token_haste(token_generation_t *token, int beast, int number)
{
  int card;

  card = create_targetted_legacy_effect(token->s_player, token->s_card, give_token_haste_legacy,
                                        token->t_player, beast);
  add_status(token->t_player, card, STATUS_INVISIBLE_FX);
  return;
}

/* ManalinkEh 2022: 0x22ce550; original cards/dominaria.c:1356. */
int card_helm_of_the_host(int player, int card, event_t event)
{
  /* Helm of the Host	|4
   * Legendary Artifact - Equipment
   * At the beginning of combat on your turn, create a token that's a copy of equipped creature, except the token isn't legendary if equipped creature is legendary. That token gains haste.
   * Equip |5 */

  enum
  {
    TOKEN_ACTION_ISNT_LEGENDARY = 256
  };

  int result;
  card_instance_t *instance;
  token_generation_t token;

  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {
    instance = is_equipping(player, card);
    if (instance != NULL)
    {
      copy_token_definition(player, card, &token, (int)instance->damage_target_player,
                            instance->damage_target_card);
      token.special_code_on_generation = give_token_haste;
      token.action = TOKEN_ACTION_ISNT_LEGENDARY;
      generate_token(&token);
    }
  }
  result = basic_equipment(player, card, event, 5);
  return result;
}

/* ManalinkEh 2022: 0x22ce5f0; original cards/dominaria.c:1374. */
int card_history_of_benalia(int player, int card, event_t event)
{
  /* History of Benalia	|1|W|W
   * Enchantment - Saga
   * I, II - Create a 2/2 |Swhite Knight creature token with vigilance.
   * III - Knights you control get +2/+1 until end of turn. */

  test_definition_t test;

  if ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CHAPTER1)
  {
    generate_token_by_id(player, card, CARD_ID_KNIGHT);
  }
  else if (event == EVENT_CHAPTER3)
  {
    new_default_test_definition(&test, 2, "");
    test.subtype = 0x67;
    pump_creatures_until_eot_merge_pt(player, card, player, 2, 1, &test);
  }
  return saga(player, card, event, 3);
}

/* Homarid Explorer	|3|U
 * Creature - Homarid Scout 3/3
 * When ~ enters the battlefield, target player puts the top four cards of their library into their graveyard. */

/* Howling Golem	|3
 * Artifact Creature - Golem 2/3
 * Whenever ~ attacks or blocks, each player draws a card. */

/* ManalinkEh 2022: 0x22ce6c0; original cards/dominaria.c:1400. */
int card_in_bolass_clutches(int player, int card, event_t event)
{
  /* In Bolas's Clutches	|4|U|U
   * Legendary Enchantment - Aura
   * Enchant permanent
   * You control enchanted permanent.
   * Enchanted permanent is legendary. */

  int result;
  target_definition_t td;

  check_legend_rule(player, card, event);
  default_target_definition(player, card, &td, 0x47);
  attached_is_additional_subtype(player, card, event, SUBTYPE_LEGEND);
  result = generic_stealing_aura(player, card, event, &td, "TARGET_PERMANENT");
  return result;
}

/* ManalinkEh 2022: 0x22ce740; original cards/dominaria.c:1415. */
int card_invoke_the_divine(int player, int card, event_t event)
{
  /* Invoke the Divine	|2|W
   * Instant
   * Destroy target artifact or enchantment. You gain 4 life. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x44);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      gain_life(player, 4);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "DISENCHANT", 1, NULL);
}

/* Jaya Ballard	|2|R|R|R
 * Legendary Planeswalker - Jaya (5)
 * +1: Add |R|R|R. Spend this mana only to cast instant or sorcery spells.
 * +1: Discard up to three cards, then draw that many cards.
 * -8: You get an emblem with "You may cast instant and sorcery cards from your graveyard. If a card cast this way would be put into your graveyard, exile it instead." */

/* ManalinkEh 2022: 0x22ce8a0; original cards/dominaria.c:1449. */
int card_jayas_immolating_inferno(int player, int card, event_t event)
{
  /* Jaya's Immolating Inferno	|X|R|R
   * Legendary Sorcery
   * ~ deals X damage to each of up to three targets. */

  target_t *target_cursor;
  target_t *other_target_cursor;
  card_instance_t *instance;
  int i;
  target_definition_t *damage_target;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (instance->number_of_targets != '\0')
    {
      i = 0;
      do
      {
        if (validate_target(player, card, &td, i) == 0)
        {
          i = i + 1;
          continue;
        }
        other_target_cursor = instance->targets + i;
        target_cursor = instance->targets + i;
        i = i + 1;
        damage_creature(target_cursor->player, other_target_cursor->card, instance->info_slot,
                        player, card);
      } while (i < (int)(unsigned int)instance->number_of_targets);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  damage_target = &td;
  return generic_spell(player, card, event, 0x24, damage_target, "TARGET_ANY", 3, NULL);
}

/* ManalinkEh 2022: 0x22cea50; original cards/dominaria.c:1476. */
int card_jhoiras_familiar(int player, int card, event_t event)
{
  /* Jhoira's Familiar	|4
   * Artifact Creature - Bird 2/2
   * Flying
   * Historic spells you cast cost |1 less to cast. */

  int result;

  if ((((event == EVENT_MODIFY_COST_GLOBAL) && (affected_card_controller == player)) &&
       (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
      (((result = is_what(affected_card_controller, affected_card, 0x40),
         result != 0 ||
             (result = is_legendary(affected_card_controller, affected_card), result != 0)) ||
        (result = has_subtype(affected_card_controller, affected_card, SUBTYPE_SAGA),
         result != 0))))
  {
    COST_COLORLESS = COST_COLORLESS + -1;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22ceb10; original cards/dominaria.c:1491. */
int card_jhoira_weatherlight_captain(int player, int card, event_t event)
{
  /* Jhoira, Weatherlight Captain	|2|U|R
   * Legendary Creature - Human Artificer 3/3
   * Whenever you cast a historic spell, draw a card. */

  check_legend_rule(player, card, event);
  cast_historic_spell(player, card, event, 2);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  draw_a_card(player);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22ceb70; original cards/dominaria.c:1505. */
int card_jodah_archmage_eternal(int player, int card, event_t event)
{
  /* Jodah, Archmage Eternal	|1|U|R|W
   * Legendary Creature - Human Wizard 4/3
   * Flying
   * You may pay |W|U|B|R|G rather than pay the mana cost for spells that you cast. */

  check_legend_rule(player, card, event);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (has_mana_multi(player, 0, 1, 1, 1, 1, 1))
    {
      int result = 0;
      for (int i = 0; i < active_cards_count[player]; ++i)
      {
        if (in_hand(player, i) && !is_what(player, i, 1))
        {
          int legal = can_legally_play_iid(player, get_original_internal_card_id(player, i));
          if (legal != 0 && (player == 0 || (player == 1 && !has_mana_to_cast_id(1, EVENT_CAN_CAST,
                                                                                 get_id(1, i)))))
          {
            result = legal;
            if (legal == 99)
              return 99;
          }
        }
      }
      return result;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    charge_mana_multi(player, 0, 1, 1, 1, 1, 1);
    if (spell_fizzled != 1)
    {
      unsigned capacity = (unsigned)hand_count[player];
      int playable[2][capacity];
      int count = 0;
      for (int i = 0; i < active_cards_count[player]; ++i)
      {
        if (in_hand(player, i) && !is_what(player, i, 1))
        {
          int legal = can_legally_play_iid(player, get_original_internal_card_id(player, i));
          if (legal != 0 && (player == 0 || (player == 1 && !has_mana_to_cast_id(1, EVENT_CAN_CAST,
                                                                                 get_id(1, i)))))
          {
            playable[0][count] = get_original_internal_card_id(player, i);
            playable[1][count++] = i;
          }
        }
      }
      test_definition_t test;
      _Static_assert(sizeof(test) == (sizeof(void *) == 8 ? 232 : 216), "native Jodah filter");
      new_default_test_definition(&test, 0xff, "Select a card to play.");
      test.zone = 0x100;
      int selected =
          select_card_from_zone(player, player, playable[0], count, 0, 2, -1, &test);
      if (selected == -1)
        spell_fizzled = 1;
      else
      {
        play_card_in_hand_for_free(player, playable[1][selected]);
        cant_be_responded_to = 1;
      }
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x22ceec0; original cards/dominaria.c:1562. */
int card_josu_vess_lich_knight(int player, int card, event_t event)
{
  /* Josu Vess, Lich Knight	|2|B|B
   * Legendary Creature - Zombie Knight 4/5
   * Kicker |5|B
   * Menace
   * When ~ enters the battlefield, if it was kicked, create eight 2/2 |Sblack Zombie Knight creature tokens with menace. */

  int result;
  token_generation_t token;
  check_legend_rule(player, card, event);
  kicker(player, card, event, 5, 1, 0, 0, 0, 0);
  menace(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    result = kicked(player, card);
    if (result != 0)
    {
      default_token_definition(player, card, CARD_ID_ZOMBIE_KNIGHT, &token);
      token.s_key_plus = 0x10000;
      token.qty = '\b';
      generate_token(&token);
    }
  }
  return 0;
}

/* Zombie Knight	""(token)
 * Creature - Zombie Knight 2/2
 * Menace */

/* Jousting Lance	|2
 * Artifact - Equipment
 * Equipped creature gets +2/+0.
 * As long as it's your turn, equipped creature has first strike.
 * Equip |3 */

/* ManalinkEh 2022: 0x22ca320; original cards/dominaria.c:1594. */
static int is_land_or_legendary_by_internal_id(int iid, intptr_t unused, int unused1, int unused2)
{
  int matches;
  int legendary;

  matches = 1;
  if ((cards_data[iid].type & 1) == 0)
  {
    legendary = is_legendary(-1, iid);
    matches = 0;
    if (legendary != 0)
    {
      return (unsigned int)((cards_data[iid].type & 0x47) != 0);
    }
  }
  return matches;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22cefa0; original cards/dominaria.c:1597. */
int card_kamahls_druidic_vow(int player, int card, event_t event)
{
  /* Kamahl's Druidic Vow	|X|G|G
   * Legendary Sorcery
   * Look at the top X cards of your library. You may put any number of land and/or legendary permanent cards with converted mana cost X or less from among them onto the battlefield. Put the rest into your graveyard. */

  if (event == EVENT_CAN_CAST)
  {
    if (check_battlefield_for_subtype(player, TYPE_CREATURE, 0x2001))
      return 1;
    return check_battlefield_for_subtype(player, 0x100, -1) != 0;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
      if (spell_fizzled != 1)
      {
        card_instance_t *instance = get_card_instance(player, card);
        ai_modifier += instance->info_slot > 5 ? instance->info_slot * 12 : -48;
      }
    }
    return 0;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int num_cards = count_deck(player);
    if (instance->info_slot < num_cards)
      num_cards = instance->info_slot;
    if (num_cards != 0)
    {
      int gc_flag = grafdiggers_cages(player, TYPE_CREATURE, TUTOR_FROM_DECK);
      show_deck(player, deck_ptr[player], num_cards, "Cards revealed by Kamahl's Druidic Vow",
                     0, gs_done_008b40e0);
      if (gc_flag == 0)
      {
        int deck_copy[num_cards];
        for (int i = 0; i < num_cards; ++i)
        {
          deck_copy[i] = deck_ptr[player][0];
          remove_card_from_deck(player, 0);
        }
        char msg[100];
        scnprintf(msg, 100, "Select a land and/or legendary permanent card with CMC <= %d.",
                  num_cards);
        test_definition_t test;
        new_default_test_definition(&test, 0x47, msg);
        test.cmc = num_cards + 1;
        test.special_selection_function = is_land_or_legendary_by_internal_id;
        test.cmc_flag = 3;
        int remaining = num_cards;
        do
        {
          int selected =
              select_card_from_zone(player, player, deck_copy, remaining, 0, 1, -1, &test);
          if (selected == -1)
          {
            if (remaining != 0)
            {
              for (int i = 0; i < remaining; ++i)
                raw_put_iid_on_top_of_deck(player, deck_copy[i]);
              mill(player, remaining);
            }
            break;
          }
          int added = add_card_to_hand(player, deck_copy[selected]);
          set_special_flags(player, added, 0x10000);
          put_into_play(player, added);
          --remaining;
          for (int i = selected; i < remaining; ++i)
            deck_copy[i] = deck_copy[i + 1];
        } while (remaining != 0);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  else if (event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
}
#endif

/* ManalinkEh 2022: 0x22cf390; original cards/dominaria.c:1667. */
int card_karns_temporal_sundering(int player, int card, event_t event)
{
  /* Karn's Temporal Sundering	|4|U|U
   * Legendary Sorcery
   * Target player takes an extra turn after this one. Return up to one target nonland permanent to its owner's hand. Exile ~. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  target_definition_t td1;
  if (event == EVENT_CAN_CAST)
  {
    result = check_battlefield_for_subtype(player, 2, 0x2001);
    if (result != 0)
    {
      return 1;
    }
    result = check_battlefield_for_subtype(player, 0x100, -1);
    return (unsigned int)(result != 0);
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
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    default_target_definition(player, card, &td1, 0x47);
    td1.illegal_type = TYPE_LAND;
    td1.allow_cancel = 1;
    new_pick_target(&td, "TARGET_PLAYER", 0, 0);
    result = can_target(&td1);
    if (result != 0)
    {
      new_pick_target(&td1, "TARGET_NONLAND_PERMANENT", 1, 0);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET))
    {
      if (event == EVENT_CHANGE_TARGET)
      {
        default_target_definition(player, card, &td, 0);
        td.zone = TARGET_ZONE_PLAYERS;
        default_target_definition(player, card, &td1, 0x47);
        td1.illegal_type = TYPE_LAND;
        td1.allow_cancel = 1;
        result = basic_spell(player, card, EVENT_CHANGE_TARGET);
        return result;
      }
      return 0;
    }
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    default_target_definition(player, card, &td1, 0x47);
    td1.illegal_type = TYPE_LAND;
    td1.allow_cancel = 1;
    if (event == EVENT_RESOLVE_SPELL)
    {
      instance = get_card_instance(player, card);
      time_walk_effect(instance->targets[0].player, card);
      result = validate_target(player, card, &td1, 1);
      if (result != 0)
      {
        instance = get_card_instance(player, card);
        result = instance->targets[1].card;
        instance = get_card_instance(player, card);
        bounce_permanent(instance->targets[1].player, result);
      }
      kill_card(player, card, KILL_EXILE);
      result = basic_spell(player, card, EVENT_RESOLVE_SPELL);
      return result;
    }
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22cf690; original cards/dominaria.c:1706. */
int card_karn_scion_of_urza(int player, int card, event_t event)
{
  /* Karn, Scion of Urza	|4
   * Legendary Planeswalker - Karn (5)
   * +1: Reveal the top two cards of your library. An opponent chooses one of them. Put that card into your hand and exile the other with a silver counter on it.
   * -1: Put a card you own with a silver counter on it from exile into your hand.
   * -2: Create a 0/0 colorless Construct artifact creature token with "This creature gets +1/+1 for each artifact you control." */

  enum
  {
    CHOICE_REVEAL = 1,
    CHOICE_RETURN = 2,
    CHOICE_CONSTRUCT = 3
  };

  card_instance_t *instance;
  int result;
  int32_t *chosen_exile;
  int *exile_entry;
  int pos;
  unsigned int iid;
  int reveal_count;
  int leg;
  int idx;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event != EVENT_ACTIVATE)
      return planeswalker(player, card, event, 5);

    instance->number_of_targets = '\0';
    instance->info_slot = 0;
    result = exiledby_count(player, card, player);
    result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Reveal top 2", 1, 5,
                         1, "Return a card from exile", (unsigned int)(0 < result), 10, 0xffffffff,
                         "Make a construct", 1, 10, 0xfffffffe, 0, 0, 0, 0);
    if (result == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (result != CHOICE_RETURN)
      return planeswalker(player, card, event, 5);

    chosen_exile = (int32_t *)exiledby_choose(player, card, 0x4809, EXBY_CHOOSE, 0, "Card", 0);
    if (chosen_exile != NULL)
    {
      instance->targets[6].player = -1;
      instance->targets[6].card = *chosen_exile;
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event != EVENT_RESOLVE_ACTIVATION)
      return planeswalker(player, card, event, 5);

    result = instance->info_slot;
    if (result == CHOICE_REVEAL)
    {

      result = count_deck(player);
      if (result != 0)
      {
        test_definition_t test;
        reveal_count = result;
        if (2 < result)
        {
          reveal_count = 2;
        }

        leg = 0;
        idx = 0;
        show_deck(player, deck_ptr[player], reveal_count, "Cards revealed by Karn", 0,
                  gs_done_008b40e0);
        new_default_test_definition(&test, 0xff, "Select a card.");
        test.create_minideck = (unsigned short)reveal_count;
        test.no_shuffle = 1;
        pos = new_select_a_card(1 - player, player, 1, 1, 3, -1, &test);
        exiledby_remember(player, instance->parent_card, player, deck_ptr[player][pos], &leg, &idx);
        play_sound_effect(WAV_DESTROY);
        rfg_card_in_deck(player, pos);
        if (result != 1)
        {
          add_card_to_hand(player, *deck_ptr[player]);
          remove_card_from_deck(player, 0);
        }
      }
      return planeswalker(player, card, event, 5);
    }
    if (result != CHOICE_RETURN)
    {
      if (result == CHOICE_CONSTRUCT)
      {
        token_generation_t token;
        default_token_definition(player, card, CARD_ID_CONSTRUCT, &token);
        token.tou = 0;
        token.pow = 0;
        token.special_infos = 66;
        generate_token(&token);
      }
      return planeswalker(player, card, event, 5);
    }

    exile_entry =
        exiledby_find(player, instance->parent_card, instance->targets[6].card, NULL, NULL);
    if (exile_entry != NULL)
    {

      *exile_entry = -1;
      iid = instance->targets[6].card & 0x7fffffff;
      add_card_to_hand(player, iid);
      remove_card_from_rfg(player, (unsigned int)cards_data[iid].id);
      return planeswalker(player, card, event, 5);
    }
  }
  spell_fizzled = 1;
  return planeswalker(player, card, event, 5);
}

/* Karplusan Hound	|3|R
 * Creature - Hound 3/3
 * Whenever ~ attacks, if you control a Chandra planeswalker, this creature deals 2 damage to any target. */

/* ManalinkEh 2022: 0x22cfb50; original cards/dominaria.c:1808. */
int card_kazarov_sengir_pureblood(int player, int card, event_t event)
{
  /* Kazarov, Sengir Pureblood	|5|B|B
   * Legendary Creature - Vampire 4/4
   * Flying
   * Whenever a creature an opponent controls is dealt damage, put a +1/+1 counter on ~.
   * |3|R: ~ deals 2 damage to target creature. */

  int result;
  card_instance_t *instance;
  card_instance_t *damage;
  card_instance_t *damage_source;
  int next_pending_count;
  int32_t damaged_player;
  target_definition_t td;

  result = is_humiliated(player, card, AL_TRIG);
  if (result == 0)
  {

    instance = get_card_instance(player, card);
    if (event == EVENT_DEAL_DAMAGE)
    {
      bool record_damage = true;
      damage = get_card_instance(affected_card_controller, affected_card);
      if ((damage->internal_card_id == damage_card) &&
          (((damage->damage_target_card != -1 &&
             (result = is_what((int)damage->damage_target_player, damage->damage_target_card, 2),
              result != 0)) &&
            (damaged_player = (int32_t)damage->damage_target_player,
             damaged_player == 1 - player))))
      {

        if (damage->info_slot < 1)
        {

          damage_source =
              get_card_instance((int)damage->damage_source_player, damage->damage_source_card);
          if (damage_source->targets[0x10].player < 1)
            record_damage = false;
          damaged_player = (int32_t)damage->damage_target_player;
        }
        if (record_damage)
        {
          result = instance->info_slot;
          if (result < 0)
          {
            result = 0;
            next_pending_count = 1;
          }
          else
          {
            next_pending_count = result + 1;
          }
          instance->targets[result].player = damaged_player;
          instance->targets[result].card = damage->damage_target_card;
          instance->info_slot = next_pending_count;
        }
      }
      instance = get_card_instance(player, card);
      if (instance->info_slot < 1)
        return 0;
    }
    if (instance->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE)
    {
      if (((card == affected_card) && (player == affected_card_controller)) &&
          (reason_for_trigger_controller == player))
      {
        if (event == EVENT_TRIGGER)
        {
          event_result = event_result | 2;
          return 0;
        }
        if (event == EVENT_RESOLVE_TRIGGER)
        {

          instance = get_card_instance(player, card);
          add_counters(player, card, player, card, COUNTER_P1_P1, instance->info_slot);
          instance->info_slot = 0;
          return 0;
        }
      }
    }
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 2);
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = validate_target(player, card, &td, 0), result != 0))
  {
    damage_target0(player, card, 2);
  }
  result = generic_activated_ability(player, card, event, 4, 3, 0, 0, 0, 1, 0, 0, &td,
                                     "TARGET_CREATURE");
  return result;
}

/* Keldon Overseer	|2|R
 * Creature - Human Warrior 3/1
 * Kicker |3|R
 * Haste
 * When ~ enters the battlefield, if it was kicked, gain control of target creature until end of turn. Untap that creature. It gains haste until end of turn. */

/* Keldon Raider	|2|R|R
 * Creature - Human Warrior 4/3
 * When ~ enters the battlefield, you may discard a card. If you do, draw a card. */

/* Keldon Warcaller	|1|R
 * Creature - Human Warrior 2/2
 * Whenever ~ attacks, put a lore counter on target Saga you control. */

/* ManalinkEh 2022: 0x22cfe60; original cards/dominaria.c:1888. */
int card_knight_of_grace(int player, int card, event_t event)
{
  /* Knight of Grace	|1|W
   * Creature - Human Knight 2/2
   * First strike
   * Hexproof from |Sblack
   * ~ gets +1/+0 as long as any player controls a |Sblack permanent. */

  unsigned int spell_color;
  unsigned int required_color;
  int result;

  misc_hexproof(player, card, event);
  if (event == EVENT_PROT_MISC)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      spell_color = get_color(attacking_card_controller, attacking_card);
      required_color = get_sleighted_color_test(player, card, 2);
      if ((spell_color & required_color) != 0)
      {
        event_result = event_result | 1;
      }
    }
  }
  else if (((event == EVENT_POWER) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    result = get_sleighted_color_test(player, card, 2);
    result = count_permanents_by_color(2, 0x47, result);
    if (result != 0)
    {
      event_result = event_result + 1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22cff50; original cards/dominaria.c:1906. */
int card_knight_of_malice(int player, int card, event_t event)
{
  /* Knight of Malice	|1|B
   * Creature - Human Knight 2/2
   * First strike
   * Hexproof from |Swhite
   * ~ gets +1/+0 as long as any player controls a |Swhite permanent. */

  unsigned int spell_color;
  unsigned int required_color;
  int result;

  misc_hexproof(player, card, event);
  if (event == EVENT_PROT_MISC)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      spell_color = get_color(attacking_card_controller, attacking_card);
      required_color = get_sleighted_color_test(player, card, 0x20);
      if ((spell_color & required_color) != 0)
      {
        event_result = event_result | 1;
      }
    }
  }
  else if (((event == EVENT_POWER) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    result = get_sleighted_color_test(player, card, 0x20);
    result = count_permanents_by_color(2, 0x47, result);
    if (result != 0)
    {
      event_result = event_result + 1;
    }
  }
  return 0;
}

/* Knight of New Benalia	|1|W
 * Creature - Human Knight 3/1 */

/* Krosan Druid	|2|G
 * Creature - Centaur Druid 2/3
 * Kicker |4|G
 * When ~ enters the battlefield, if it was kicked, you gain 10 life. */

/* ManalinkEh 2022: 0x22d0040; original cards/dominaria.c:1932. */
int card_kwende_pride_of_femeref(int player, int card, event_t event)
{
  /* Kwende, Pride of Femeref	|3|W
   * Legendary Creature - Human Knight 2/2
   * Double strike
   * Creatures you control with first strike have double strike. */

  card_instance_t *instance;
  int result;

  check_legend_rule(player, card, event);
  if (event == EVENT_ABILITIES)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if ((result == 0) && (affected_card_controller == player))
      {
        result = is_what(player, affected_card, 2);
        if (result != 0)
        {
          result = check_for_ability(affected_card_controller, affected_card, 0x100);
          if (result != 0)
          {
            event_result = event_result | 0x40000000;
          }
        }
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22ca570; original cards/dominaria.c:1949. */
static char *dominaria_target_only_permanents_unless_in_hand(int who_chooses, int player, int card)
{
  card_instance_t *instance;
  int result;
  instance = in_hand(player, card);
  if (instance == NULL)
  {
    result = is_what(player, card, 0x47);
    if (result == 0)
    {
      return (char *)gs_target_error_type;
    }
  }
  return NULL;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d0100; original cards/dominaria.c:1954. */
void lichs_mastery_lifeloss_trigger(int player, int card, int amount)
{
  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_HAND | TARGET_ZONE_IN_PLAY;
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.illegal_abilities = 0;
  td.allow_cancel = 0;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (intptr_t)dominaria_target_only_permanents_unless_in_hand;
  int grave[amount];
  target_t tgts[amount + 1];
  card_instance_t *instance = get_card_instance(player, card);
  int num_tgts = 0;
  int i = 0;
  while (1)
  {
    int available = can_target(&td);
    if ((!available && get_grave(player)[0] == -1) || amount <= i)
      break;
    int grave_available = get_grave(player)[0] != -1;
    int hand_or_play_available = can_target(&td);
    int choice =
        dialog_impl(player, card, EVENT_RESOLVE_ACTIVATION, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_STORAGE, DLG_NO_CANCEL,
                    DLG_NO_DISPLAY_FOR_AI, "Exile from hand or play", hand_or_play_available, 5,
                    "Exile from graveyard", grave_available, 10, 0, 0, 0, 0);
    int next_count = num_tgts + 1;
    if (choice == 1)
    {
      char buffer[100];
      scnprintf(buffer, 100,
                "Select a permanent you control or a card from your hand to exile. (%d of %d).",
                next_count, amount);
      if (select_target(player, card, &td, buffer, &tgts[i]))
      {
        instance->number_of_targets = 0;
        state_untargettable(tgts[i].player, tgts[i].card, 1);
        num_tgts = next_count;
        grave[i] = 0;
      }
      ++i;
    }
    else
    {
      test_definition_t test;
      default_test_definition(&test, 0xff);
      scnprintf(test.message, 100, "Select a card to exile (%d of %d).", next_count, amount);
      new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 1, 3,
                            &test);
      num_tgts = next_count;
      grave[i] = 1;
      ++i;
    }
  }
  for (i = 0; i < num_tgts; ++i)
    if (!grave[i])
      state_untargettable(tgts[i].player, tgts[i].card, 0);
  for (i = 0; i < num_tgts; ++i)
  {
    if (grave[i])
      continue;
    if (in_hand(tgts[i].player, tgts[i].card))
    {
      put_on_top_of_deck(tgts[i].player, tgts[i].card);
      rfg_top_card_of_deck(tgts[i].player);
    }
    else
      kill_card(tgts[i].player, tgts[i].card, KILL_EXILE);
  }
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d04c0; original cards/dominaria.c:2011. */
int card_lichs_mastery(int player, int card, event_t event)
{
  /* Lich's Mastery	|3|B|B|B
   * Legendary Enchantment
   * Hexproof
   * You can't lose the game.
   * Whenever you gain life, draw that many cards.
   * Whenever you lose life, for each 1 life you lost, exile a permanent you control or a card from your hand or graveyard.
   * When ~ leaves the battlefield, you lose the game. */

  int result;
  card_instance_t *instance;

  check_legend_rule(player, card, event);
  hexproof(player, card, event);
  cannot_lose_the_game(player, card, event, player);
  result = trigger_gain_life(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);
  if ((result != 0) && (instance = in_play(player, card), instance != NULL))
  {
    draw_cards(player, result);
  }
  if ((((trigger_condition == TRIGGER_LEAVE_PLAY) &&
        (result = is_humiliated(player, card, AL_TRIG), result == 0)) &&
       (card == affected_card)) &&
      (((player == affected_card_controller && (trigger_cause == card)) &&
        ((trigger_cause_controller == player && (reason_for_trigger_controller == player))))))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      suspend_cannot_lose_the_game = 1;
      lose_the_game(player);
    }
    else if (event == EVENT_END_TRIGGER)
    {
      suspend_cannot_lose_the_game = 0;
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}
#endif

/* Lingering Phantom	|5|B
 * Creature - Spirit 5/4
 * Whenever you cast a historic spell, you may pay |B. If you do, return ~ from your graveyard to your hand. */

/* Llanowar Envoy	|2|G
 * Creature - Elf Scout 3/2
 * |1|G: Add one mana of any color. */

/* Llanowar Scout	|1|G
 * Creature - Elf Scout 1/3
 * |T: You may put a land card from your hand onto the battlefield. */

/* ManalinkEh 2022: 0x22d0610; original cards/dominaria.c:2059. */
int card_lyra_dawnbringer(int player, int card, event_t event)
{
  /* Lyra Dawnbringer	|3|W|W
   * Legendary Creature - Angel 5/5
   * Flying, first strike, lifelink
   * Other Angels you control get +1/+1 and have lifelink. */

  check_legend_rule(player, card, event);
  lifelink(player, card, event);
  boost_subtype(player, card, event, SUBTYPE_ANGEL, 1, 1, 0, 2, BCT_CONTROLLER_ONLY);
  return 0;
}

/* Mammoth Spider	|4|G
 * Creature - Spider 3/5
 * Reach */

/* ManalinkEh 2022: 0x22d0690; original cards/dominaria.c:2075. */
int card_marwyn_the_nurturer(int player, int card, event_t event)
{
  /* Marwyn, the Nurturer	|2|G
   * Legendary Creature - Elf Druid 1/1
   * Whenever another Elf enters the battlefield under your control, put a +1/+1 counter on ~.
   * |T: Add an amount of |G equal to ~'s power. */

  int result;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    new_default_test_definition(&test, 0x47, "");
    test.subtype = 0x3c;
    test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    result = 0;
  }
  else if (((event == EVENT_COUNT_MANA) || (event == EVENT_ACTIVATE)) ||
           ((event == EVENT_CAN_ACTIVATE && (player == 1))))
  {
    result = get_power(player, card);
  }
  else
  {
    result = 0;
  }
  return mana_producing_creature(player, card, event, 0x30, COLOR_GREEN, result);
}

/* ManalinkEh 2022: 0x22c9630; original cards/dominaria.c:2103. */
static void memorial_to_folly_impl(int player, int card, target_definition_t *td)
{
  int grave_position;

  grave_position = validate_target_from_grave_source(player, card, player, 0);
  if (grave_position == -1)
  {
    return;
  }
  from_grave_to_hand(player, grave_position, 1);
  return;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d07e0; original cards/dominaria.c:2109. */
int card_memorial_to_folly(int player, int card, event_t event)
{
  /* Memorial to Folly	""
   * Land
   * ~ enters the battlefield tapped.
   * |T: Add |B.
   * |2|B, |T, Sacrifice ~: Return target creature card from your graveyard to your hand. */

  int result;
  result = dominaria_memorial_impl(player, card, event, 2, 1, 0, 0, 0, 0, 1,
                                        "Return a creature to your hand", NULL,
                                        "Select target creature card.", memorial_to_folly_impl);
  return result;
}
#endif

/* ManalinkEh 2022: 0x22c95d0; original cards/dominaria.c:2119. */
static void memorial_to_genius_impl(int player, int card, target_definition_t *td)
{
  draw_cards(player, 2);
  return;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d0850; original cards/dominaria.c:2122. */
int card_memorial_to_genius(int player, int card, event_t event)
{
  /* Memorial to Genius	""
   * Land
   * ~ enters the battlefield tapped.
   * |T: Add |U.
   * |4|U, |T, Sacrifice ~: Draw two cards. */

  int result;
  result = dominaria_memorial_impl(player, card, event, 4, 0, 1, 0, 0, 0, 0, "Draw", NULL,
                                        NULL, memorial_to_genius_impl);
  return result;
}
#endif

/* ManalinkEh 2022: 0x22c9680; original cards/dominaria.c:2131. */
static void memorial_to_glory_impl(int player, int card, target_definition_t *td)
{
  generate_tokens_by_id(player, card, CARD_ID_SOLDIER, 2);
  return;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d08c0; original cards/dominaria.c:2134. */
int card_memorial_to_glory(int player, int card, event_t event)
{
  /* Memorial to Glory	""
   * Land
   * ~ enters the battlefield tapped.
   * |T: Add |W.
   * |3|W, |T, Sacrifice ~: Create two 1/1 |Swhite Soldier creature tokens. */

  int result;
  result = dominaria_memorial_impl(player, card, event, 3, 0, 0, 0, 0, 1, 0, "Create tokens",
                                        NULL, NULL, memorial_to_glory_impl);
  return result;
}
#endif

/* ManalinkEh 2022: 0x22c94f0; original cards/dominaria.c:2143. */
static void memorial_to_unity_impl(int player, int card, target_definition_t *td)
{
  int amount;
  int selected;
  test_definition_t this_test;

  amount = count_deck(player);
  if (amount != 0)
  {
    if (5 < amount)
    {
      amount = 5;
    }
    new_default_test_definition(&this_test, 2, "Select a creature card.");
    this_test.create_minideck = (unsigned short)amount;
    this_test.no_shuffle = 1;
    selected = new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
    if (-1 < selected)
    {
      amount = amount + -1;
      reveal_card(player, card, player, selected);
    }
    if (0 < amount)
    {
      put_top_x_on_bottom_in_random_order(player, amount);
      return;
    }
  }
  return;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d0930; original cards/dominaria.c:2160. */
int card_memorial_to_unity(int player, int card, event_t event)
{
  /* Memorial to Unity	""
   * Land
   * ~ enters the battlefield tapped.
   * |T: Add |G.
   * |2|G, |T, Sacrifice ~: Look at the top five cards of your library. You may reveal a creature card from among them and put it into your hand. Then put the rest on the bottom of your library in a random order. */

  int result;
  result = dominaria_memorial_impl(player, card, event, 2, 0, 0, 1, 0, 0, 0,
                                        "Dig for a creature", NULL, NULL, memorial_to_unity_impl);
  return result;
}
#endif

/* ManalinkEh 2022: 0x22ca2c0; original cards/dominaria.c:2169. */
static void memorial_to_war_impl(int player, int card, target_definition_t *td)
{
  int result;
  card_instance_t *instance;

  result = valid_target(td);
  if (result == 0)
  {
    return;
  }
  instance = get_card_instance(player, card);
  result = instance->targets[0].card;
  instance = get_card_instance(player, card);
  kill_card(instance->targets[0].player, result, KILL_DESTROY);
  return;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d09a0; original cards/dominaria.c:2173. */
int card_memorial_to_war(int player, int card, event_t event)
{
  /* Memorial to War	""
   * Land
   * ~ enters the battlefield tapped.
   * |T: Add |R.
   * |4|R, |T, Sacrifice ~: Destroy target land. */

  int result;
  target_definition_t td;
  if (((unsigned int)((uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) < 2) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 1);
  }
  result = dominaria_memorial_impl(player, card, event, 4, 0, 0, 0, 1, 0, 0, "Destroy a land",
                                        &td, "TARGET_LAND", memorial_to_war_impl);
  return result;
}
#endif

/* ManalinkEh 2022: 0x22d0a60; original cards/dominaria.c:2186. */
int card_merfolk_trickster(int player, int card, event_t event)
{
  /* Merfolk Trickster	|U|U
   * Creature - Merfolk Wizard 2/2
   * Flash
   * When ~ enters the battlefield, tap target creature an opponent controls. It loses all abilities until end of turn. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  td.allow_cancel = 0;
  instance = get_card_instance(player, card);
  if (event == EVENT_ETB)
  {
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS");
      if (result != 0)
      {
        tap_card(instance->targets[0].player, instance->targets[0].card);
        humiliate_and_set_pt_abilities(player, card, instance->targets[0].player,
                                       instance->targets[0].card, 4, NULL);
      }
    }
  }
  result = flash(player, card, event);
  return result;
}

/* Mesa Unicorn	|1|W
 * Creature - Unicorn 2/2
 * Lifelink */

/* Mishra's Self-Replicator	|5
 * Artifact Creature - Assembly-Worker 2/2
 * Whenever you cast a historic spell, you may pay |1. If you do, create a token that's a copy of ~. */

/* ManalinkEh 2022: 0x22d0b60; original cards/dominaria.c:2217. */
int card_mox_amber(int player, int card, event_t event)
{
  /* Mox Amber	|0
   * Legendary Artifact
   * |T: Add one mana of any color among legendary creatures and planeswalkers you control. */

  if (event != EVENT_RESOLVE_SPELL)
    return mana_producer(player, card, event);

  int id = get_id(player, card);
  int mana_colors = get_color_of_mana_produced_by_id(id, COLOR_TEST_ANY_COLORED, player);
  play_land_sound_effect_force_color(player, card, mana_colors < 0 ? 0 : mana_colors);
  return 0;
}

/* ManalinkEh 2022: 0x22c96b0; original cards/dominaria.c:2248. */
static int muldrotha_legacy(int player, int card, event_t event)
{
  if (event == EVENT_STATIC_EFFECTS && card == affected_card && player == affected_card_controller)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player < 0)
      kill_card(player, card, KILL_EXILE);
    if (!check_status(player, card, 0x10000))
      add_status(player, card, 0x10000);
  }
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->damage_target_player < 0)
  {
    if (event == EVENT_CLEANUP)
      get_card_instance(player, card)->targets[1].player = 0;
    return 0;
  }
  if (!effect_follows_control_of_attachment(player, card, event) && event == EVENT_CLEANUP)
    get_card_instance(player, card)->targets[1].player = 0;
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d0bd0; original cards/dominaria.c:2270. */
int card_muldrotha_the_gravetide(int player, int card, event_t event)
{
  /* Muldrotha, the Gravetide	|3|B|G|U
   * Legendary Creature - Elemental Avatar 6/6
   * During each of your turns, you may play up to one permanent card of each permanent type from your graveyard. */

  check_legend_rule(player, card, event);
  card_instance_t *instance = get_card_instance(player, card);
  for (int i = 0; i < active_cards_count[player]; ++i)
  {
    card_instance_t *leg = in_play(player, i);
    if (leg && leg->internal_card_id == LEGACY_EFFECT_CUSTOM &&
        leg->info_slot == (int32_t)(intptr_t)muldrotha_legacy &&
        leg->damage_source_player == player && leg->damage_source_card == card &&
        leg->damage_target_player == player && leg->damage_target_card == card)
    {
      instance = get_card_instance(player, i);
      break;
    }
  }
  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    int found = 0;
    for (int i = 0; i < active_cards_count[player]; ++i)
    {
      if (in_play(player, i) && is_what(player, i, 0x80))
      {
        card_instance_t *leg = get_card_instance(player, i);
        if (leg->info_slot == (int32_t)(intptr_t)muldrotha_legacy &&
            ((leg->damage_target_player == player && leg->damage_target_card == card) ||
             (leg->targets[0].player == player && leg->targets[0].card == card)))
        {
          found = 1;
          break;
        }
      }
    }
    if (!found)
    {
      int legacy = create_targetted_legacy_effect(player, card, muldrotha_legacy, player, card);
      card_instance_t *leg = get_card_instance(player, legacy);
      leg->targets[0].player = player;
      leg->targets[0].card = card;
      leg->number_of_targets = 1;
      add_status(player, legacy, 0x10000);
    }
  }
  if (!in_play(player, card) || is_humiliated(player, card, AL_STATIC))
    return 0;
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (current_turn == player)
    {
      int used_types = instance->targets[1].player;
      if (used_types == -1)
        instance->targets[1].player = used_types = 0;
      for (int i = 0; get_grave(player)[i] != -1; ++i)
      {
        int types = get_type_with_iid(-1, -1, get_grave(player)[i]);
        if ((types & 0x147) != 0 && (types | used_types) != used_types)
        {
          int legal = can_legally_play_iid_now(player, get_grave(player)[i], EVENT_CAN_ACTIVATE);
          if (legal != 0 && has_mana_to_cast_iid(player, EVENT_CAN_ACTIVATE, get_grave(player)[i]))
            return legal;
        }
      }
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    int used_types = instance->targets[1].player;
    unsigned capacity = (unsigned)count_graveyard(player);
    int playable[2][capacity];
    int count = 0;
    for (int i = 0; get_grave(player)[i] != -1; ++i)
    {
      int types = get_type_with_iid(-1, -1, get_grave(player)[i]);
      if ((types & 0x147) != 0 && (types | used_types) != used_types &&
          can_legally_play_iid_now(player, get_grave(player)[i], EVENT_ACTIVATE) &&
          has_mana_to_cast_iid(player, EVENT_ACTIVATE, get_grave(player)[i]))
      {
        playable[0][count] = get_grave(player)[i];
        playable[1][count++] = i;
      }
    }
    test_definition_t test;
    _Static_assert(sizeof(test) == (sizeof(void *) == 8 ? 232 : 216), "native Muldrotha filter");
    new_default_test_definition(&test, 0xff, "Select a permanent card to play.");
    int selected = select_card_from_zone(player, player, playable[0], count, 0, 1, -1, &test);
    if (selected != -1 && charge_mana_from_id(player, -1, EVENT_ACTIVATE,
                                              (uint16_t)cards_data[playable[0][selected]].id))
    {
      int types = get_type_with_iid(-1, -1, playable[0][selected]);
      if (num_bits_set(types) < 2)
        instance->targets[1].player |= types;
      else
      {
        int choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                                 DLG_OMIT_ILLEGAL, DLG_AUTOCHOOSE_IF_1, "Artifact",
                                 (types & 0x40) != 0 && (used_types & 0x40) == 0, 1, "Creature",
                                 (types & 2) != 0 && (used_types & 2) == 0, 1, "Enchantment",
                                 (types & 4) != 0 && (used_types & 4) == 0, 1, "Land",
                                 (types & 1) != 0 && (used_types & 1) == 0, 1, "Planeswalker",
                                 (types & 0x100) != 0 && (used_types & 0x100) == 0, 1, 0, 0, 0, 0);
        switch (choice)
        {
        case 1:
          instance->targets[1].player |= 0x40;
          break;
        case 2:
          instance->targets[1].player |= 2;
          break;
        case 3:
          instance->targets[1].player |= 4;
          break;
        case 4:
          instance->targets[1].player |= 1;
          break;
        case 5:
          instance->targets[1].player |= 0x100;
          break;
        default:
          spell_fizzled = 1;
          return 0;
        }
      }
      play_card_in_grave_for_free(player, player, playable[1][selected]);
      cant_be_responded_to = 1;
      return 0;
    }
    spell_fizzled = 1;
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x22d1350; original cards/dominaria.c:2402. */
int card_multani_yavimayas_avatar(int player, int card, event_t event)
{
  /* Multani, Yavimaya's Avatar	|4|G|G
   * Legendary Creature - Elemental Avatar 0/0
   * Reach, trample
   * ~ gets +1/+1 for each land you control and each land card in your graveyard.
   * |1|G, Return two lands you control to their owner's hand: Return ~ from your graveyard to your hand. */

  int i;
  target_t *target_cursor;
  target_t *other_target_cursor;
  card_instance_t *instance;
  int land_card;
  int next_target;
  int result_or_index;
  bool cancelled;
  int to_bounce[2];
  char msg[50];
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        ((player == -1 ||
          (((-1 < player && (instance = in_play(player, card), instance != NULL)) &&
            (result_or_index = is_humiliated(player, card, AL_STATIC), result_or_index == 0))))))
    {
      result_or_index = count_graveyard_by_type(player, 1);
      event_result = event_result + result_or_index + landsofcolor_controlled[player][7];
    }
    default_target_definition(player, card, &td, 1);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.illegal_abilities = 0;
    get_card_instance(player, card);
  }
  else
  {
    default_target_definition(player, card, &td, 1);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.illegal_abilities = 0;
    instance = get_card_instance(player, card);
    if (event == EVENT_GRAVEYARD_ABILITY)
    {
      result_or_index = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 1,
                                                  0, 0, 0, NULL, NULL);
      if ((result_or_index != 0) &&
          (result_or_index = target_available(player, card, &td), 1 < result_or_index))
      {
        return 4;
      }
    }
    else if (event == EVENT_PAY_FLASHBACK_COSTS)
    {

      charge_mana_for_activated_ability(player, card, 1, 0, 0, 1, 0, 0);
      result_or_index = 0;
      do
      {
        next_target = result_or_index + 1;
        scnprintf(msg, 0x32, " Select a land you control (%d of 2)", next_target);
        if (new_pick_target(&td, msg, result_or_index, 0x80) == 0)
          break;
        land_card = instance->targets[result_or_index].card;
        to_bounce[result_or_index] = land_card;
        state_untargettable(instance->targets[result_or_index].player, land_card, 1);
        result_or_index = next_target;
      } while (next_target != 2);
      if (instance->number_of_targets != '\0')
      {
        i = 0;
        do
        {
          other_target_cursor = instance->targets + i;
          target_cursor = instance->targets + i;
          i = i + 1;
          state_untargettable(target_cursor->player, other_target_cursor->card, 0);
        } while (i < (int)(unsigned int)instance->number_of_targets);
        if (1 < instance->number_of_targets)
        {
          cancelled = spell_fizzled == 1;
          instance->number_of_targets = '\0';
          if (cancelled)
          {
            return 0;
          }
          bounce_permanent(player, to_bounce[0]);
          bounce_permanent(player, to_bounce[1]);
          return 1;
        }
      }
      spell_fizzled = 1;
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22d1690; original cards/dominaria.c:2465. */
int card_naban_dean_of_iteration(int player, int card, event_t event)
{
  /* Naban, Dean of Iteration	|1|U
   * Legendary Creature - Human Wizard 2/1
   * If a Wizard entering the battlefield under your control causes a triggered ability of a permanent you control to trigger, that ability triggers an additional time. */

  check_legend_rule(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    card_counts[player][CCOUNTS_NABAN_ITERATION] = card_counts[player][CCOUNTS_NABAN_ITERATION] + '\x01';
  }
  return 0;
}

/* ManalinkEh 2022: 0x22d16e0; original cards/dominaria.c:2475. */
int card_naru_meha_master_wizard(int player, int card, event_t event)
{
  /* Naru Meha, Master Wizard	|2|U|U
   * Legendary Creature - Human Wizard 3/3
   * Flash
   * When ~ enters the battlefield, copy target instant or sorcery spell you control. You may choose new targets for the copy.
   * Other Wizards you control get +1/+1. */

  check_legend_rule(player, card, event);
  card_instance_t *instance = get_card_instance(player, card);
  target_definition_t td;
  counterspell_target_definition(player, card, &td, TYPE_INSTANT | TYPE_SORCERY);
  td.allowed_controller = player;
  if (event == EVENT_CAN_CAST)
  {
    if (twincast(player, card, event, &td, NULL))
    {
      instance->info_slot = 1;
      return 99;
    }
    instance->info_slot = 0;
    return 1;
  }

  if (event == EVENT_CAST_SPELL && affect_me(player, card) && instance->info_slot == 1)
    return counterspell(player, card, event, &td, 0);

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB && counterspell_validate(player, card, &td, 0) && instance->info_slot == 1)
    copy_spell_from_stack(player, instance->targets[0].player, instance->targets[0].card);
  return 0;
}

/* Navigator's Compass	|1
 * Artifact
 * When ~ enters the battlefield, you gain 3 life.
 * |T: Until end of turn, target land you control becomes the basic land type of your choice in addition to its other types. */

/* ManalinkEh 2022: 0x22d1880; original cards/dominaria.c:2513. */
int card_niambi_faithful_healer(int player, int card, event_t event)
{
  /* Niambi, Faithful Healer	|1|W|U
   * Legendary Creature - Human Cleric 2/2
   * When ~ enters the battlefield, you may search your library and/or graveyard for a card named Teferi, Timebender, reveal it, and put it into your hand. If you search your library this way, shuffle it. */

  int result;
  test_definition_t test;
  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  new_default_test_definition(&test, 0xff, "Select a card named Teferi, Timebender.");
  test.id = CARD_ID_TEFERI_TIMEBENDER;
  result = new_special_count_grave(player, &test);
  if ((result != 0) && (result = new_global_tutor(player, player, 2, 1, 0, 5, &test), -1 < result))
  {
    return 0;
  }
  new_global_tutor(player, player, 1, 1, 0, 5, &test);
  return 0;
}

/* ManalinkEh 2022: 0x22d1980; original cards/dominaria.c:2535. */
int card_oath_of_teferi(int player, int card, event_t event)
{
  /* Oath of Teferi	|3|W|U
   * Legendary Enchantment
   * When ~ enters the battlefield, exile another target permanent you control. Return it to the battlefield under its owner's control at the beginning of the next end step.
   * You may activate the loyalty abilities of planeswalkers you control twice each turn rather than only once. */

  int i;
  int result_or_card;
  card_instance_t *instance;
  target_definition_t td;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 0x47);
    td.allowed_controller = player;
    td.allow_cancel = 0;
    td.special = TARGET_SPECIAL_NOT_ME;
    instance = get_card_instance(player, card);
    result_or_card = can_target(&td);
    if ((result_or_card != 0) && (result_or_card = new_pick_target(
                                      &td, "Select another target permanent you control.", 0, 0x80),
                                  result_or_card != 0))
    {
      remove_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
    }
    instance->number_of_targets = '\0';
    leaves_play(player, card, EVENT_ETB, 0, 0);
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    result_or_card = count_cards_by_id(player, get_id(player, card));

    if ((result_or_card == 1) && (0 < active_cards_count[player]))
    {
      i = 0;
      do
      {
        instance = in_play(player, i);
        if ((instance != NULL) && (is_what(player, i, 0x100) != 0))
        {
          instance = get_card_instance(player, i);
          instance->targets[9].card = instance->targets[9].card + 1;
        }
        i = i + 1;
      } while (i < active_cards_count[player]);
    }
    leaves_play(player, card, EVENT_RESOLVE_SPELL, 0, 0);
  }
  else
  {
    leaves_play(player, card, event, 0, 0);

    if ((((event == EVENT_LEAVES_PLAY_ABILITY) &&
          (result_or_card = is_humiliated(player, card, AL_STATIC), result_or_card == 0)) &&
         (result_or_card = check_battlefield_for_id(player, CARD_ID_OATH_OF_TEFERI), result_or_card == 0)) &&
        (result_or_card = 0, 0 < active_cards_count[player]))
    {
      do
      {
        instance = in_play(player, result_or_card);
        if ((instance != NULL) && (is_what(player, result_or_card, 0x100) != 0))
        {
          instance = get_card_instance(player, result_or_card);
          instance->targets[9].card = instance->targets[9].card + -1;
        }
        result_or_card = result_or_card + 1;
      } while (result_or_card < active_cards_count[player]);
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x22d1c40; original cards/dominaria.c:2578. */
int card_on_serras_wings(int player, int card, event_t event)
{
  /* On Serra's Wings	|3|W
   * Legendary Enchantment - Aura
   * Enchant creature
   * Enchanted creature is legendary, gets +1/+1, and has flying, vigilance, and lifelink. */

  int result;
  result = generic_aura(player, card, event, player, 1, 1, 0x20, 10, 0x2001, 0, 8);
  return result;
}

/* Orcish Vandal	|1|R
 * Creature - Orc Warrior 1/1
 * |T, Sacrifice an artifact: ~ deals 2 damage to any target. */

/* Pardic Wanderer	|6
 * Artifact Creature - Golem 5/5
 * Trample */

/* Pegasus Courser	|2|W
 * Creature - Pegasus 1/3
 * Flying
 * Whenever ~ attacks, another target attacking creature gains flying until end of turn. */

/* ManalinkEh 2022: 0x22d1ca0; original cards/dominaria.c:2601. */
int card_phyrexian_scriptures(int player, int card, event_t event)
{
  /* Phyrexian Scriptures	|2|B|B
   * Enchantment - Saga
   * I - Put a +1/+1 counter on up to one target creature. That creature becomes an artifact in addition to its other types.
   * II - Destroy all nonartifact creatures.
   * III - Exile all cards from all opponents' graveyards. */

  int result;
  card_instance_t *instance;


  if (event == EVENT_CHAPTER1)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE", 0, 1);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_P1_P1, 1);
        turn_into_artifact(player, card, instance->targets[0].player, instance->targets[0].card, 0);
      }
    }
  }
  else if (event == EVENT_CHAPTER2)
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.type_flag = F1_NONARTIFACT_CREATURE;
    new_manipulate_all(player, card, 2, &test, ACT_KILL_DESTROY);
  }
  else if (event == EVENT_CHAPTER3)
  {
    rfg_whole_graveyard(1 - player);
  }
  result = saga(player, card, event, 3);
  return result;
}

/* Pierce the Sky	|1|G
 * Instant
 * ~ deals 7 damage to target creature with flying. */

/* Powerstone Shard	|3
 * Artifact
 * |T: Add |C for each artifact you control named ~. */

/* ManalinkEh 2022: 0x22d1e20; original cards/dominaria.c:2638. */
int card_precognition_field(int player, int card, event_t event)
{
  /* Precognition Field	|3|U
   * Enchantment
   * You may look at the top card of your library.
   * You may cast the top card of your library if it's an instant or sorcery card.
   * |3: Exile the top card of your library. */

  enum
  {
    CHOICE_REVEAL = 1,
    CHOICE_PLAY = 2,
    CHOICE_EXILE = 3
  };

  int choice;
  int iid_or_exile_available;
  card_instance_t *instance;
  int play_available;
  uint32_t exile_priority;
  unsigned int cast_available;
  uint32_t inspect_available;

  if (event == EVENT_CAN_ACTIVATE)
  {

    iid_or_exile_available = *deck_ptr[player];
    if (iid_or_exile_available != -1)
    {
      play_available = is_what(-1, iid_or_exile_available, 0x38);
      if ((play_available != 0) && (play_available = has_mana_to_cast_iid(
                                        player, EVENT_CAN_ACTIVATE, iid_or_exile_available),
                                    play_available != 0))
      {
        return can_legally_play_iid_now(player, iid_or_exile_available, EVENT_CAN_ACTIVATE);
      }
      if (*deck_ptr[player] != -1)
      {
        return 1;
      }
    }
    iid_or_exile_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 3, 0, 0,
                                                       0, 0, 0, 0, NULL, NULL);
    return (unsigned int)(iid_or_exile_available != 0);
  }
  if (event != EVENT_ACTIVATE)
  {
    if ((event == EVENT_RESOLVE_ACTIVATION) &&
        (instance = get_card_instance(player, card), instance->info_slot == CHOICE_EXILE))
    {
      rfg_top_card_of_deck(player);
    }
    return global_enchantment(player, card, event);
  }

  iid_or_exile_available = *deck_ptr[player];
  inspect_available = ai_is_speculating != 1 && (player != 1 || ((unsigned char)trace_mode & 2));
  if (iid_or_exile_available == -1)
    inspect_available = 0;
  cast_available = 0;
  exile_priority = 10;
  if (iid_or_exile_available != -1 && is_what(-1, iid_or_exile_available, 0x38) &&
      can_legally_play_iid_now(player, *deck_ptr[player], EVENT_ACTIVATE))
  {
    play_available = has_mana_to_cast_iid(player, EVENT_ACTIVATE, *deck_ptr[player]);
    cast_available = play_available != 0;
    if (play_available != 0)
      exile_priority = 0xfffffff6;
  }
  iid_or_exile_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 3, 0, 0,
                                                     0, 0, 0, 0, NULL, NULL);
  choice =
      dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Check top card of deck",
                  inspect_available, 0, "Play top card", cast_available, 10, "Exile top card",
                  iid_or_exile_available, exile_priority, 0, 0, 0, 0);
  if (choice == 0)
  {
    spell_fizzled = 1;
    return 0;
  }
  if (choice == CHOICE_REVEAL)
  {
    show_deck(player, deck_ptr[player], 1, "Here\'s the top card of your deck", 0,
              gs_done_008b40e0);
    spell_fizzled = 1;
  }
  else if (choice == CHOICE_PLAY)
  {
    if (charge_mana_from_id(player, -1, EVENT_ACTIVATE,
                            (unsigned int)cards_data[*deck_ptr[player]].id) == 0)
    {
      spell_fizzled = 1;
    }
    else
    {
      play_card_in_deck_for_free(player, player, 0);
      cant_be_responded_to = 1;
    }
  }
  else if (choice == CHOICE_EXILE)
  {
    generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x22d2320; original cards/dominaria.c:2694. */
int card_primevals_glorious_rebirth(int player, int card, event_t event)
{
  /* Primevals' Glorious Rebirth	|5|W|B
   * Legendary Sorcery
   * Return all legendary permanent cards from your graveyard to the battlefield. */

  unsigned int cast_result;
  int result;
  test_definition_t test;
  if (event == EVENT_CAN_CAST)
  {
    result = check_battlefield_for_subtype(player, 2, 0x2001);
    cast_result = 1;
    if (result == 0)
    {
      result = check_battlefield_for_subtype(player, 0x100, -1);
      cast_result = (unsigned int)(result != 0);
    }
  }
  else
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      default_test_definition(&test, 0x47);
      test.subtype = 0x2001;
      new_reanimate_all(player, -1, player, &test, REANIMATE_DEFAULT);
      kill_card(player, card, KILL_DESTROY);
    }
    cast_result = basic_spell(player, card, event);
  }
  return cast_result;
}

/* Primordial Wurm	|4|G|G
 * Creature - Wurm 7/6 */

/* Pyromantic Pilgrim	|2|R
 * Creature - Human Wizard 3/1
 * Haste */

/* ManalinkEh 2022: 0x22c97b0; original cards/dominaria.c:2728. */
static char *can_cast_historic_spell(int who_chooses, int player, int card, int targeting_player,
                                     int targeting_card)
{
  if (!is_what(player, card, TYPE_ARTIFACT) && !is_legendary(player, card) &&
      !has_subtype(player, card, SUBTYPE_SAGA))
    return "must be historic";
  if (!has_mana_to_cast_id(who_chooses, 0, get_id(player, card)))
    return "not enough mana";
  if (!can_legally_play_csvid(who_chooses, get_id(player, card)))
    return "can\'t play this card";
  if (is_what(player, card, TYPE_LAND))
    return "not a spell";
  return NULL;
}

/* Radiating Lightning	|3|R
 * Instant
 * ~ deals 3 damage to target player and 1 damage to each creature that player controls. */

/* ManalinkEh 2022: 0x22d2410; original cards/dominaria.c:2740. */
int card_raff_capashen_ships_mage(int player, int card, event_t event)
{
  /* Raff Capashen, Ship's Mage	|2|W|U
   * Legendary Creature - Human Wizard 3/3
   * Flash
   * Flying
   * You may cast historic spells as though they had flash. */

  check_legend_rule(player, card, event);
  if (flash(player, card, event))
    return 1;
  if (!IS_ACTIVATING(event))
    return 0;
  target_definition_t td;
  base_target_definition(player, card, &td, 0xff);
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (intptr_t)can_cast_historic_spell;
  td.zone = TARGET_ZONE_HAND;
  const char *prompt = event == EVENT_ACTIVATE ? "Select an historic card." : NULL;
  return can_play_cards_as_though_they_had_flash(player, card, event, &td, prompt, 1);
}

/* Rampaging Cyclops	|3|R
 * Creature - Cyclops 4/4
 * ~ gets -2/-0 as long as two or more creatures are blocking it. */

/* Rat Colony	|1|B
 * Creature - Rat 2/1
 * ~ gets +1/+0 for each other Rat you control.
 * A deck can have any number of cards named ~. */

/* Relic Runner	|1|U
 * Creature - Human Rogue 2/1
 * ~ can't be blocked if you've cast a historic spell this turn. */

/* ManalinkEh 2022: 0x22d24e0; original cards/dominaria.c:2778. */
int card_rite_of_belzenlok(int player, int card, event_t event)
{
  /* Rite of Belzenlok	|2|B|B
   * Enchantment - Saga
   * I, II - Create two 0/1 |Sblack Cleric creature tokens.
   * III - Create a 6/6 |Sblack Demon creature token with flying, trample, and "At the beginning of your upkeep, sacrifice another creature. If you can't, this creature deals 6 damage to you." */

  if ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CHAPTER1)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_CLERIC, &token);
    token.pow = 0;
    token.color_forced = '\x02';
    token.qty = '\x02';
    generate_token(&token);
  }
  else if (event == EVENT_CHAPTER3)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_DEMON, &token);
    token.tou = 6;
    token.pow = 6;
    token.key_plus = 0xa0;
    token.special_infos = 0x42;
    generate_token(&token);
  }
  return saga(player, card, event, 3);
}

/* Rona, Disciple of Gix	|1|U|B
 * Legendary Creature - Human Artificer 2/2
 * When ~ enters the battlefield, you may exile target historic card from your graveyard.
 * You may cast nonland cards exiled with ~.
 * |4, |T: Exile the top card of your library. */

/* ManalinkEh 2022: 0x22d25b0; original cards/dominaria.c:2809. */
int card_run_amok(int player, int card, event_t event)
{
  /* Run Amok	|1|R
   * Instant
   * Target attacking creature gets +3/+3 and gains trample until end of turn. */

  target_definition_t td;
  card_instance_t *instance;
  int result;
  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET && event != EVENT_CHECK_PUMP)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  if (event == EVENT_CHECK_PUMP)
  {
    td.required_state = TARGET_STATE_ATTACKING;
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    result = has_mana_to_cast_iid(player, EVENT_CAN_CAST, instance->internal_card_id);
    if (result == 0)
    {
      return 0;
    }
    result = can_target(&td);
    if (result == 0)
    {
      return 0;
    }
    pumpable_power[player] = pumpable_power[player] + 3;
    pumpable_toughness[player] = pumpable_toughness[player] + 3;
    return 0;
  }
  td.required_state = TARGET_STATE_ATTACKING;
  td.preferred_controller = player;
  return vanilla_pump(player, card, event, &td, 3, 3, 0x80, 0);
}

/* Sanctum Spirit	|3|W
 * Creature - Spirit 3/2
 * Lifelink
 * Discard a historic card: ~ gains indestructible until end of turn. */

/* ManalinkEh 2022: 0x22d27d0; original cards/dominaria.c:2844. */
int card_saproling_migration(int player, int card, event_t event)
{
  /* Saproling Migration	|1|G
   * Sorcery
   * Kicker |4
   * Create two 1/1 |Sgreen Saproling creature tokens. If this spell was kicked, create four of those tokens instead. */

  int result;
  token_generation_t token;
  kicker(player, card, event, 4, 0, 0, 0, 0, 0);
  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_token_definition(player, card, CARD_ID_SAPROLING, &token);
    result = kicked(player, card);
    token.qty = (-(result == 0) & 0xfeU) + 4;
    generate_token(&token);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x22d28e0; original cards/dominaria.c:2863. */
int card_seal_away(int player, int card, event_t event)
{
  /* Seal Away	|1|W
   * Enchantment
   * Flash
   * When ~ enters the battlefield, exile target tapped creature an opponent controls until ~ leaves the battlefield. */

  card_instance_t *instance;
  target_definition_t td;
  return_from_oblivion(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    td.allow_cancel = 0;
    td.required_state = TARGET_STATE_TAPPED;
    if (can_target(&td) &&
        new_pick_target(&td, "Select target tapped creature an opponent controls.", 0, 0x81))
    {
      instance = get_card_instance(player, card);
      obliviation(player, card, instance->targets[0].player, instance->targets[0].card);
    }
    else if (player == AI)
    {
      ai_modifier = ai_modifier + -0x30;
    }
  }
  return flash(player, card, event);
}

/* Seismic Shift	|3|R
 * Sorcery
 * Destroy target land. Up to two target creatures can't block this turn. */

/* Sentinel of the Pearl Trident	|4|U
 * Creature - Merfolk Soldier 3/3
 * Flash
 * When ~ enters the battlefield, you may exile target historic permanent you control. If you do, return that card to the battlefield under its owner's control at the beginning of the next end step. */

/* Sergeant-at-Arms	|2|W
 * Creature - Human Soldier 2/3
 * Kicker |2|W
 * When ~ enters the battlefield, if it was kicked, create two 1/1 |Swhite Soldier creature tokens. */

/* Serra Disciple	|1|W
 * Creature - Bird Cleric 1/1
 * Flying, first strike
 * Whenever you cast a historic spell, ~ gets +1/+1 until end of turn. */

/* ManalinkEh 2022: 0x22d29f0; original cards/dominaria.c:2910. */
int card_settle_the_score(int player, int card, event_t event)
{
  /* Settle the Score	|2|B|B
   * Sorcery
   * Exile target creature. Put two loyalty counters on a planeswalker you control. */

  int result;
  card_instance_t *instance;
  target_t pw;
  target_definition_t td;
  target_definition_t tdpw;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {

      instance = get_card_instance(player, card);
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      base_target_definition(player, card, &tdpw, 0x100);
      tdpw.preferred_controller = player;
      tdpw.allowed_controller = player;
      tdpw.allow_cancel = 0;
      result = can_target(&tdpw);
      if ((result != 0) && (result = select_target(player, card, &tdpw,
                                                   "Select a planeswalker you control.", &pw),
                            result != 0))
      {
        add_counters(player, card, pw.player, pw.card, COUNTER_LOYALTY, 2);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x22d2be0; original cards/dominaria.c:2943. */
int card_shalai_voice_of_plenty(int player, int card, event_t event)
{
  /* Shalai, Voice of Plenty	|3|W
   * Legendary Creature - Angel 3/4
   * Flying
   * You, planeswalkers you control, and other creatures you control have hexproof.
   * |4|G|G: Put a +1/+1 counter on each creature you control. */

  card_instance_t *instance;
  test_definition_t this_test;
  check_legend_rule(player, card, event);
  instance = in_play(player, card);
  if ((instance != NULL) && (is_humiliated(player, card, AL_STATIC) == 0))
  {
    give_hexproof_to_player(player, card, event);
  }
  if (event == EVENT_ABILITIES)
  {
    instance = in_play(player, card);
    if ((((instance != NULL) && (is_humiliated(player, card, AL_STATIC) == 0)) &&
         (is_what(affected_card_controller, affected_card, 0x102) != 0)) &&
        ((affected_card_controller == player && (affected_card != card))))
    {
      instance = get_card_instance(player, affected_card);
      instance->ext->sp_keywords = instance->ext->sp_keywords | (SP_KEYWORD_HEXPROOF);
    }
    return 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event != EVENT_RESOLVE_ACTIVATION)
    return generic_activated_ability(player, card, event, 0, 4, 0, 0, 2, 0, 0, 0, NULL, NULL);
  default_test_definition(&this_test, 2);
  new_manipulate_all(player, card, player, &this_test, ACT_ADD_COUNTERS_BASE | ACT_KILL_BURY);
  return generic_activated_ability(player, card, event, 0, 4, 0, 0, 2, 0, 0, 0, NULL, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d2db0; original cards/dominaria.c:2976. */
int card_shanna_sisays_legacy(int player, int card, event_t event)
{
  /* Shanna, Sisay's Legacy	|G|W
   * Legendary Creature - Human Warrior 0/0
   * ~ can't be the target of abilities your opponents control.
   * ~ gets +1/+1 for each creature you control. */

  check_legend_rule(player, card, event);
  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      int creatures = count_permanents_by_type(player, TYPE_CREATURE);
      event_result += creatures;
    }
  }
  else if ((((event == EVENT_PROT_MISC) && (card == affected_card)) &&
            (player == affected_card_controller)) &&
           ((((unsigned int)event_result >> 24 & 0xff) == '\x05' &&
             (attacking_card_controller != player))))
  {
    cannot_be_target_of_abilities(player, card, EVENT_PROT_MISC);
  }
  return 0;
}
#endif

/* Shield of the Realm	|2
 * Artifact - Equipment
 * If a source would deal damage to equipped creature, prevent 2 of that damage.
 * Equip |1 */

/* ManalinkEh 2022: 0x22d2e60; original cards/dominaria.c:2996. */
int card_shivan_fire(int player, int card, event_t event)
{
  /* Shivan Fire	|R
   * Instant
   * Kicker |4
   * ~ deals 2 damage to target creature. If this spell was kicked, it deals 4 damage to that creature instead. */

  int result;
  target_definition_t td;
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 1 - player;
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = 1 - player;
      if ((card == affected_card) && (player == affected_card_controller))
      {
        generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
        if (spell_fizzled != 1)
        {
          do_kicker(player, card, 4, 0, 0, 0, 0, 0);
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 1 - player;
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        result = kicked(player, card);
        damage_target0(player, card, ((result == 0) ? 2U : 4U));
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

/* Short Sword	|1
 * Artifact - Equipment
 * Equipped creature gets +1/+1.
 * Equip |1 */

/* ManalinkEh 2022: 0x22c9890; original cards/dominaria.c:3037. */
static int slimefoot_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  unsigned int pending_deaths;
  card_instance_t *dying;
  test_definition_t test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (get_id((int)instance->damage_target_player, instance->damage_target_card) != 0x484c))
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
        }
        default_test_definition(&test, 2);
        test.subtype = 0xa3;
        count_for_gfp_ability(player, card, 0x77, player, 0, &test);
      }
    }
  }
  pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_deaths != 0)
  {
    if (!check_status(player, card, 0x10000))
    {

      pending_deaths = pending_deaths & 0xff;
      if (pending_deaths != 0)
      {
        do
        {
          pending_deaths = pending_deaths - 1;
          damage_player(1 - player, 1, player, card);
          gain_life(player, 1);
        } while (0 < (int)pending_deaths);
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

/* Skittering Surveyor	|3
 * Artifact Creature - Construct 1/2
 * When ~ enters the battlefield, you may search your library for a basic land card, reveal it, put it into your hand, then shuffle your library. */

/* ManalinkEh 2022: 0x22d3090; original cards/dominaria.c:3092. */
int card_slimefoot_the_stowaway(int player, int card, event_t event)
{
  /* Slimefoot, the Stowaway	|1|B|G
   * Legendary Creature - Fungus 2/3
   * Whenever a Saproling you control dies, ~ deals 1 damage to each opponent and you gain 1 life.
   * |4: Create a 1/1 |Sgreen Saproling creature token. */

  int c;
  card_instance_t *instance;
  int created_legacy;
  unsigned int pending_deaths;
  test_definition_t test;

  check_legend_rule(player, card, event);
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
                instance->info_slot_fn == slimefoot_effect)) &&
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
            create_targetted_legacy_effect(player, card, slimefoot_effect, player, card);
        instance = get_card_instance(player, created_legacy);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, created_legacy, 0x10000);
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    default_test_definition(&test, 2);
    test.subtype = 0xa3;
    count_for_gfp_ability(player, card, event, player, 0, &test);
  }
  instance = get_card_instance(player, card);
  if (instance->kill_code == 0)
  {
    pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (pending_deaths != 0)
    {
      pending_deaths &= 0xff;
      while (pending_deaths != 0)
      {
        damage_player(1 - player, 1, player, card);
        gain_life(player, 1);
        --pending_deaths;
      }
    }
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
    generate_token_by_id(player, card, CARD_ID_SAPROLING);
  return generic_activated_ability(player, card, event, 0, 4, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x22d33d0; original cards/dominaria.c:3154. */
int card_slinn_voda_the_rising_deep(int player, int card, event_t event)
{
  /* Slinn Voda, the Rising Deep	|6|U|U
   * Legendary Creature - Leviathan 8/8
   * Kicker |1|U
   * When ~ enters the battlefield, if it was kicked, return all creatures to their owners' hands except for Merfolk, Krakens, Leviathans, Octopuses, and Serpents. */

  int p;
  int remaining_or_index;
  card_instance_t *instance;
  int eligible;
  int card_00;
  int marked[2][151];

  check_legend_rule(player, card, event);
  kicker(player, card, event, 1, 0, 1, 0, 0, 0);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if ((event == EVENT_ETB) && (remaining_or_index = kicked(player, card), remaining_or_index != 0))
  {

    memset(marked, 0, sizeof(marked));
    p = 0;
    do
    {
      if (0 < active_cards_count[p])
      {
        card_00 = 0;
        do
        {
          instance = in_play(p, card_00);
          if ((((instance != NULL) && (eligible = is_what(p, card_00, 2), eligible != 0)) &&
               (eligible = has_subtype(p, card_00, SUBTYPE_MERFOLK), eligible == 0)) &&
              (((eligible = has_subtype(p, card_00, SUBTYPE_KRAKEN),
                 eligible == 0 &&
                     (eligible = has_subtype(p, card_00, SUBTYPE_LEVIATHAN), eligible == 0)) &&
                ((eligible = has_subtype(p, card_00, SUBTYPE_OCTOPUS),
                  eligible == 0 &&
                      (eligible = has_subtype(p, card_00, SUBTYPE_SERPENT), eligible == 0))))))
          {
            marked[p][card_00] = 1;
          }
          card_00 = card_00 + 1;
        } while (card_00 < active_cards_count[p]);
      }
      p = p + 1;
    } while (p != 2);
    remaining_or_index = 0;
    if (0 < active_cards_count[0])
    {
      do
      {
        if (marked[0][remaining_or_index] != 0)
        {
          bounce_permanent(0, remaining_or_index);
        }
        remaining_or_index = remaining_or_index + 1;
      } while (remaining_or_index < active_cards_count[0]);
    }
    remaining_or_index = 0;
    if (0 < active_cards_count[1])
    {
      do
      {
        if (marked[1][remaining_or_index] != 0)
        {
          bounce_permanent(1, remaining_or_index);
        }
        remaining_or_index = remaining_or_index + 1;
      } while (remaining_or_index < active_cards_count[1]);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22ca9a0; original cards/dominaria.c:3187. */
static int song_of_freyalise_legacy(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;

  result = permanents_you_control_can_tap_for_mana_all_one_color(player, card, event, TYPE_CREATURE,
                                                                 0, COLOR_TEST_ANY_COLORED, 1);
  if ((event != EVENT_CAN_ACTIVATE) && (result = 0, event == EVENT_CLEANUP))
  {
    instance = get_card_instance(player, card);
    if (instance->targets[2].player < 1)
    {
      kill_card(player, card, KILL_EXILE);
      cant_be_responded_to = 1;
    }
    else
    {
      instance = get_card_instance(player, card);
      instance->targets[2].player = instance->targets[2].player + -1;
    }
  }
  return result;
}

/* ManalinkEh 2022: 0x22d3610; original cards/dominaria.c:3205. */
int card_song_of_freyalise(int player, int card, event_t event)
{
  /* Song of Freyalise	|1|G
   * Enchantment - Saga
   * I, II - Until your next turn, creatures you control gain "|T: Add one mana of any color."
   * III - Put a +1/+1 counter on each creature you control. Those creatures gain vigilance, trample, and indestructible until end of turn. */

  int c;
  card_instance_t *instance;
  int mana_effect;

  if ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CHAPTER1)
  {

    mana_effect = create_legacy_activate(player, card, song_of_freyalise_legacy);
    if (current_turn == player)
    {
      instance = get_card_instance(player, mana_effect);
      instance->targets[2].player = 1;
    }
  }
  else
  {

    if ((event == EVENT_CHAPTER3) && (0 < active_cards_count[player]))
    {
      for (c = 0; c < active_cards_count[player]; ++c)
      {
        instance = in_play(player, c);
        if (instance && is_what(player, c, 2))
        {
          add_counters(player, card, player, c, COUNTER_P1_P1, 1);
          pump_ability_until_eot(player, card, player, c, 0, 0, 0x80, 0x808);
        }
      }
    }
  }
  return saga(player, card, event, 3);
}

/* Sorcerer's Wand	|1
 * Artifact - Equipment
 * Equipped creature has "|T: This creature deals 1 damage to target player or planeswalker. If this creature is a Wizard, it deals 2 damage to that player or planeswalker instead."
 * Equip |3 */

/* Soul Salvage	|2|B
 * Sorcery
 * Return up to two target creature cards from your graveyard to your hand. */

/* Sparring Construct	|1
 * Artifact Creature - Construct 1/1
 * When ~ dies, put a +1/+1 counter on target creature you control. */

/* Spore Swarm	|3|G
 * Instant
 * Create three 1/1 |Sgreen Saproling creature tokens. */

/* ManalinkEh 2022: 0x22d3750; original cards/dominaria.c:3247. */
int card_sporecrown_thallid(int player, int card, event_t event)
{
  /* Sporecrown Thallid	|1|G
   * Creature - Fungus 2/2
   * Each other creature you control that's a Fungus or Saproling gets +1/+1. */

  int result;

  if (((((uint32_t)(event - EVENT_POWER) < 2) && (affected_card_controller == player)) &&
       (card != affected_card)) &&
      (((result = has_subtype(player, affected_card, SUBTYPE_FUNGUS),
         result != 0 ||
             (result = has_subtype(affected_card_controller, affected_card, SUBTYPE_SAPROLING),
              result != 0)) &&
        (result = is_humiliated(player, card, AL_STATIC), result == 0))))
  {
    event_result = event_result + 1;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22d37f0; original cards/dominaria.c:3264. */
int card_squee_the_immortal(int player, int card, event_t event)
{
  /* Squee, the Immortal	|1|R|R
   * Legendary Creature - Goblin 2/1
   * You may cast ~ from your graveyard or from exile. */

  int result;
  check_legend_rule(player, card, event);
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY);
    if (result != 0)
    {
      result = get_original_internal_card_id(player, card);
      result = has_mana_to_cast_iid(player, EVENT_GRAVEYARD_ABILITY, result);
      if (result != 0)
      {
        return 2;
      }
    }
  }
  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    result = get_id(player, card);
    result = charge_mana_from_id(player, -1, EVENT_PAY_FLASHBACK_COSTS, result);
    return (unsigned int)(result != 0);
  }
  return 0;
}

/* Steel Leaf Champion	|G|G|G
 * Creature - Elf Knight 5/4
 * ~ can't be blocked by creatures with power 2 or less. */

/* Stronghold Confessor	|B
 * Creature - Human Cleric 1/1
 * Kicker |3
 * Menace
 * If ~ was kicked, it enters the battlefield with two +1/+1 counters on it. */

/* ManalinkEh 2022: 0x22d38b0; original cards/dominaria.c:3292. */
int card_sylvan_awakening(int player, int card, event_t event)
{
  /* Sylvan Awakening	|2|G
   * Sorcery
   * Until your next turn, all lands you control become 2/2 Elemental creatures with reach, indestructible, and haste. They're still lands. */

  card_instance_t *instance;
  int animation_card;
  int i;

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

      for (i = 0; i < active_cards_count[player]; ++i)
      {
        instance = in_play(player, i);
        if (instance && is_what(player, i, 1))
        {
          animation_card = turn_into_creature(player, card, player, i, 0x20, 2, 2);
          instance = get_card_instance(player, animation_card);
          instance->targets[8].player = 0x400;
          instance = get_card_instance(player, animation_card);
          instance->targets[8].card = 0x804;
          instance = get_card_instance(player, animation_card);
          instance->targets[9].card = 0x3a;
        }
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

/* ManalinkEh 2022: 0x22d3a30; original cards/dominaria.c:3319. */
int card_tatyova_benthic_druid(int player, int card, event_t event)
{
  /* Tatyova, Benthic Druid	|3|G|U
   * Legendary Creature - Merfolk Druid 3/3
   * Whenever a land enters the battlefield under your control, you gain 1 life and draw a card. */

  landfall(player, card, event);
  check_legend_rule(player, card, event);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  gain_life(player, 1);
  draw_a_card(player);
  return 0;
}

/* Jaya Ballard Emblem	""
 * Emblem - Jaya
 * You may cast instant and sorcery cards from your graveyard. If a card cast this way would be put into your graveyard, exile it instead. */

/* ManalinkEh 2022: 0x22d3aa0; original cards/dominaria.c:3338. */
int card_teferi_hero_of_dominaria_emblem(int player, int card, event_t event)
{
  /* Teferi, Hero of Dominaria Emblem	""
   * Emblem - Teferi
   * Whenever you draw a card, exile target permanent an opponent controls. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  result = card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.allowed_controller = 1 - player;
  td.allow_cancel = 0;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "TARGET_PERMANENT", 0, 0);
    if (result != 0)
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* Teferi's Sentinel	|5
 * Artifact Creature - Golem 2/6
 * As long as you control a Teferi planeswalker, ~ gets +4/+0. */

/* ManalinkEh 2022: 0x22caa60; original cards/dominaria.c:3358. */
static int teferi_hero_of_dominaria_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if ((((current_turn == player) && (trigger_condition == TRIGGER_EOT)) && (card == affected_card)) &&
      ((player == affected_card_controller && (reason_for_trigger_controller == player))))
  {
    result = instance->targets[1].player;
    if (result < 1)
    {
      result = eot_trigger(player, card, event);
      if (result != 0)
      {
        untap_lands(player, card, 2);
        kill_card(player, card, KILL_EXILE);
      }
    }
    else
    {
      instance->targets[1].player = result + -1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22d3b80; original cards/dominaria.c:3375. */
int card_teferi_hero_of_dominaria(int player, int card, event_t event)
{
  /* Teferi, Hero of Dominaria	|3|W|U
   * Legendary Planeswalker - Teferi (4)
   * +1: Draw a card. At the beginning of the next end step, untap two lands.
   * -3: Put target nonland permanent into its owner's library third from the top.
   * -8: You get an emblem with "Whenever you draw a card, exile target permanent an opponent controls." */

  enum { CHOICE_DRAW_UNTAP = 1, CHOICE_TUCK = 2, CHOICE_EMBLEM = 3 };
  check_legend_rule(player, card, event);
  if (IS_ACTIVATING(event))
  {
    card_instance_t *instance = get_card_instance(player, card);
    target_definition_t td;
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TYPE_LAND;
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = 0;
      int priority_emblem = count_counters(player, card, COUNTER_LOYALTY) * 5 + 0xf;
      int choice = dialog_impl(
          player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Draw & untap lands", 1, 10, 1,
          "Tuck a nonland permanent", target_available(player, card, &td), 10, 0xfffffffd,
          "Emblem", 1, priority_emblem, 0xfffffff8, 0, 0, 0, 0);
      instance->number_of_targets = 0;
      if (choice == CHOICE_TUCK)
        new_pick_target(&td, "TARGET_NONLAND_PERMANENT", 0, 1);
    }
    else
    {
      int choice = dialog_impl(
          player, card, event, DLG_RANDOM, DLG_PLANESWALKER, "Draw & untap lands", 1, 10, 1,
          "Tuck a nonland permanent", target_available(player, card, &td), 10, 0xfffffffd,
          "Emblem", 1, 0, 0xfffffff8, 0, 0, 0, 0);
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (choice == 0)
          return 0;
      }
      else if (choice == CHOICE_TUCK)
      {
        if (validate_target(player, card, &td, 0))
          put_permanent_under_the_first_x_card_of_its_owners_library(
              instance->targets[0].player, instance->targets[0].card, 2);
      }
      else if (choice == CHOICE_EMBLEM)
        generate_token_by_id(player, card, CARD_ID_TEFERI_HERO_OF_DOMINARIA_EMBLEM);
      else if (choice == CHOICE_DRAW_UNTAP)
      {
        draw_a_card(player);
        int legacy = create_legacy_effect(player, card, teferi_hero_of_dominaria_legacy);
        if (current_turn == player && current_phase == 0x1f)
          get_card_instance(player, legacy)->targets[1].player = 1;
      }
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x22d3ec0; original cards/dominaria.c:3455. */
int card_teferi_timebender(int player, int card, event_t event)
{
  /* Teferi, Timebender	|4|W|U
   * Legendary Planeswalker - Teferi (5)
   * +2: Untap up to one target artifact or creature.
   * -3: You gain 2 life and draw two cards.
   * -9: Take an extra turn after this one. */

  enum
  {
    CHOICE_UNTAP = 1,
    CHOICE_LIFE_DRAW = 2,
    CHOICE_TIME_WALK = 3
  };

  int result;
  card_instance_t *instance;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2) || (event == EVENT_ACTIVATE))
  {

    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0x42);
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                           "Untap an artifact or creature", 1, 3, 2, "Gain 2 life & draw 2", 1, 8,
                           0xfffffffd, "Time Walk", 1, 10, 0xfffffff7, 0, 0, 0, 0);
      instance->number_of_targets = '\0';
      if ((result == CHOICE_UNTAP) && (result = can_target(&td), result != 0))
      {
        new_pick_target(&td, "PUTREFY", 0, 0);
      }
    }
    else
    {
      result = dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
                           "Untap an artifact or creature", 1, 3, 2, "Gain 2 life & draw 2", 1, 8,
                           0xfffffffd, "Time Walk", 1, 10, 0xfffffff7, 0, 0, 0, 0);
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (result == 0)
        {
          return 0;
        }
      }
      else if (result == CHOICE_LIFE_DRAW)
      {
        gain_life(player, 2);
        draw_cards(player, 2);
      }
      else if (result == CHOICE_TIME_WALK)
      {
        time_walk_effect(player, card);
      }
      else if (((result == CHOICE_UNTAP) && (instance->number_of_targets == '\x01')) &&
               (result = valid_target(&td), result != 0))
      {
        untap_card(instance->targets[0].player, instance->targets[0].card);
      }
    }
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x22d4190; original cards/dominaria.c:3529. */
int card_tempest_djinn(int player, int card, event_t event)
{
  /* Tempest Djinn	|U|U|U
   * Creature - Djinn 0/4
   * Flying
   * ~ gets +1/+0 for each basic |H2Island you control. */

  card_instance_t *instance;
  int result;

  if (((event == EVENT_POWER) && (card == affected_card)) && (player == affected_card_controller))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = get_hacked_subtype(player, card, 0x5002);
        result = count_basic_lands(player, result);
        event_result = event_result + result;
      }
    }
  }
  return 0;
}

/* Temporal Machinations	|2|U
 * Sorcery
 * Return target creature to its owner's hand. If you control an artifact, draw a card. */

/* Territorial Allosaurus	|2|G|G
 * Creature - Dinosaur 5/5
 * Kicker |2|G
 * When ~ enters the battlefield, if it was kicked, it fights another target creature. */

/* ManalinkEh 2022: 0x22d4230; original cards/dominaria.c:3548. */
int card_teshar_ancestors_apostle(int player, int card, event_t event)
{
  /* Teshar, Ancestor's Apostle	|3|W
   * Legendary Creature - Bird Cleric 2/2
   * Flying
   * Whenever you cast a historic spell, return target creature card with converted mana cost 3 or less from your graveyard to the battlefield. */

  int result;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  cast_historic_spell(player, card, event, 2);
  if ((event == EVENT_SPA) && (spell_fizzled != 1))
  {

    new_default_test_definition(&this_test, 2, "Select a creature card with CMC 3 or less.");
    this_test.cmc = 4;
    this_test.cmc_flag = 3;
    result = new_special_count_grave(player, &this_test);
    if (0 < result)
    {
      result = graveyard_has_shroud(player);
      if (result == 0)
      {
        new_global_tutor(player, player, 2, 4, 1, 1, &this_test);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22d4300; original cards/dominaria.c:3574. */
int card_tetsuko_umezawa_fugitive(int player, int card, event_t event)
{
  /* Tetsuko Umezawa, Fugitive	|1|U
   * Legendary Creature - Human Rogue 1/3
   * Creatures you control with power or toughness 1 or less can't be blocked. */

  card_instance_t *instance;
  int result;

  check_legend_rule(player, card, event);
  if ((((event == EVENT_ABILITIES) && (instance = in_play(player, card), instance != NULL)) &&
       (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
      ((affected_card_controller == player &&
        (result = is_what(player, affected_card, 2), result != 0))))
  {
    result = get_power(affected_card_controller, affected_card);
    if ((1 < result) &&
        (result = get_toughness(affected_card_controller, affected_card), 1 < result))
    {
      return 0;
    }
    special_abilities(affected_card_controller, affected_card, EVENT_ABILITIES, 0x10, player, card);
  }
  return 0;
}

/* Thallid Omnivore	|3|B
 * Creature - Fungus 3/3
 * |1, Sacrifice another creature: ~ gets +2/+2 until end of turn. If a Saproling was sacrificed this way, you gain 2 life. */

/* ManalinkEh 2022: 0x22d43f0; original cards/dominaria.c:3593. */
int card_thallid_soothsayer(int player, int card, event_t event)
{
  /* Thallid Soothsayer	|3|B
   * Creature - Fungus 2/3
   * |2, Sacrifice a creature: Draw a card. */

  int result;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_a_card(player);
  }
  result = generic_activated_ability(player, card, event, 0x200, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x22c9c40; original cards/dominaria.c:3604. */
static int the_antiquities_war_legacy(int player, int card, event_t event)
{
  int result;
  test_definition_t this_test;

  if (event == EVENT_CHANGE_TYPE)
  {
    default_test_definition(&this_test, 0x40);
    this_test.type_flag = 2;
    global_type_change(player, card, EVENT_CHANGE_TYPE, player, 2, &this_test, 5, 5, 0, 0, 0);
    return 0;
  }
  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    result = is_what(affected_card_controller, affected_card, 0x40);
    if (result != 0)
    {
      set_base_pt(5);
    }
  }
  else if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22d4480; original cards/dominaria.c:3619. */
int card_the_antiquities_war(int player, int card, event_t event)
{
  /* The Antiquities War	|3|U
   * Enchantment - Saga
   * I, II - Look at the top five cards of your library. You may reveal an artifact card from among them and put it into your hand. Put the rest on the bottom of your library in a random order.
   * III - Artifacts you control become artifact creatures with base power and toughness 5/5 until end of turn. */

  int result;
  int revealed_card;
  test_definition_t this_test;

  if ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CHAPTER1)
  {
    if (event == EVENT_CHAPTER3)
    {
      create_legacy_effect(player, card, the_antiquities_war_legacy);
    }
    return saga(player, card, event, 3);
  }

  result = count_deck(player);
  if (result == 0)
    return saga(player, card, event, 3);
  if (5 < result)
  {
    result = 5;
  }

  new_default_test_definition(&this_test, 0x40, "Select an Artifact card.");
  this_test.create_minideck = (unsigned short)result;
  this_test.no_shuffle = 1;
  revealed_card = new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
  bool reveal = false;
  if (ai_is_speculating == 1 || (player == 1 && !((unsigned char)trace_mode & 2)))
  {
    if (revealed_card < 0)
    {
      this_test.type = 0xff;
      revealed_card = new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
      reveal = revealed_card >= 0;
    }
    else
      reveal = true;
  }
  else
    reveal = revealed_card >= 0;
  if (reveal)
  {
    result = result + -1;
    reveal_card(player, card, player, revealed_card);
  }
  if (0 < result)
  {
    put_top_x_on_bottom_in_random_order(player, result);
  }
  return saga(player, card, event, 3);
}

/* ManalinkEh 2022: 0x22d4610; original cards/dominaria.c:3652. */
int card_the_eldest_reborn(int player, int card, event_t event)
{
  /* The Eldest Reborn	|4|B
   * Enchantment - Saga
   * I - Each opponent sacrifices a creature or planeswalker.
   * II - Each opponent discards a card.
   * III - Put target creature or planeswalker card from a graveyard onto the battlefield under your control. */

  int result;
  card_instance_t *instance;
  char buffer[75];
  test_definition_t test;

  new_default_test_definition(&test, 0x102, buffer);
  if (event == EVENT_CHAPTER1)
  {
    scnprintf(buffer, 0x4b, "Select a creature or planeswalker to sacrifice");
    new_sacrifice(player, card, 1 - player, SAC_CAUSED | SAC_NO_CANCEL, &test);
  }
  else if (event == EVENT_CHAPTER2)
  {
    discard(1 - player, 0, player);
  }
  else if (event == EVENT_CHAPTER3)
  {
    scnprintf(buffer, 0x4b, "Select a creature or planeswalker card");
    result = new_special_count_grave(player, &test);
    if ((result == 0) || (result = graveyard_has_shroud(player), result != 0))
    {
      result = new_special_count_grave(1 - player, &test);
      if ((result == 0) || (result = graveyard_has_shroud(1 - player), result != 0))
        return saga(player, card, event, 3);
    }

    select_target_from_either_grave(player, card, SFG_0, 2, 2, &test, 0, 1);
    instance = get_card_instance(player, card);
    result = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
    if (result != -1)
    {
      reanimate_permanent(player, card, instance->targets[0].player, result, REANIMATE_DEFAULT);
    }
  }
  return saga(player, card, event, 3);
}

/* ManalinkEh 2022: 0x22d4810; original cards/dominaria.c:3683. */
int card_the_first_eruption(int player, int card, event_t event)
{
  /* The First Eruption	|2|R
   * Enchantment - Saga
   * I - ~ deals 1 damage to each creature without flying.
   * II - Add |R|R.
   * III - Sacrifice |Ha Mountain. If you do, ~ deals 3 damage to each creature. */

  test_definition_t this_test;
  default_test_definition(&this_test, TYPE_CREATURE);
  if (event == EVENT_CHAPTER1)
  {
    this_test.keyword = KEYWORD_FLYING;
    this_test.keyword_flag = 1;
    new_damage_all(player, card, 2, 1, 0, &this_test);
  }
  else if (event == EVENT_CHAPTER2)
    produce_mana(player, COLOR_RED, 2);
  else if (event == EVENT_CHAPTER3)
  {
    test_definition_t test;
    new_default_test_definition(&test, TYPE_LAND, "Select a mountain to sacrifice");
    test.subtype = 0x5006;
    if (new_can_sacrifice(player, card, player, &test) &&
        new_sacrifice(player, card, player, SAC_NO_CANCEL, &test))
      new_damage_all(player, card, 2, 3, 0, &this_test);
  }
  return saga(player, card, event, 3);
}

/* ManalinkEh 2022: 0x22c9d50; original cards/dominaria.c:3711. */
static int flame_of_keld_legacy(int player, int card, event_t event)
{
  card_instance_t *damage;
  card_instance_t *instance;
  unsigned int source_color;

  damage = damage_being_dealt(event);
  instance = get_card_instance(player, card);
  if (damage != NULL)
  {
    source_color = get_color((int)damage->damage_source_player, damage->damage_source_card);
    if ((instance->targets[2].card & source_color) != 0)
    {
      damage->info_slot = damage->info_slot + 2;
    }
  }
  if (!eot_trigger(player, card, event))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x22d4980; original cards/dominaria.c:3723. */
int card_the_flame_of_keld(int player, int card, event_t event)
{
  /* The Flame of Keld	|1|R
   * Enchantment - Saga
   * I - Discard your hand.
   * II - Draw two cards.
   * III - If a |Sred source you control would deal damage to a permanent or player this turn, it deals that much damage plus 2 to that permanent or player instead. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_CHAPTER1)
  {
    discard_all(player);
  }
  else if (event == EVENT_CHAPTER2)
  {
    draw_cards(player, 2);
  }
  else if (event == EVENT_CHAPTER3)
  {

    result = create_legacy_effect(player, card, flame_of_keld_legacy);
    instance = get_card_instance(player, result);
    result = get_sleighted_color_test(player, card, 0x10);
    instance->targets[2].card = result;
  }
  return saga(player, card, event, 3);
}

/* ManalinkEh 2022: 0x22d4a40; original cards/dominaria.c:3741. */
int card_the_mending_of_dominaria(int player, int card, event_t event)
{
  /* The Mending of Dominaria	|3|G|G
   * Enchantment - Saga
   * I, II - Put the top two cards of your library into your graveyard, then you may return a creature card from your graveyard to your hand.
   * III - Return all land cards from your graveyard to the battlefield, then shuffle your graveyard into your library. */

  int result;
  test_definition_t test;

  if ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CHAPTER1)
  {
    mill(player, 2);
    new_default_test_definition(&test, 2, "Select target creature card.");
    result = count_graveyard_by_type(player, 2);
    if (result != 0)
    {
      result = graveyard_has_shroud(player);
      if (result == 0)
      {
        new_global_tutor(player, player, 2, 1, 0, 1, &test);
      }
    }
  }
  else if (event == EVENT_CHAPTER3)
  {
    reanimate_all(player, card, player, 1, 0, 0, 0, 0, 0, 0, 0, -1, 0, REANIMATE_DEFAULT);
    reshuffle_grave_into_deck(player, 0);
  }
  return saga(player, card, event, 3);
}

/* ManalinkEh 2022: 0x22c9b90; original cards/dominaria.c:3762. */
int the_mirari_conjecture_legacy(int player, int card, event_t event)
{

  sp_ability_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
  }
  if (!eot_trigger(player, card, event))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x22d4ba0; original cards/dominaria.c:3774. */
int card_the_mirari_conjecture(int player, int card, event_t event)
{
  /* The Mirari Conjecture	|4|U
   * Enchantment - Saga
   * I - Return target instant card from your graveyard to your hand.
   * II - Return target sorcery card from your graveyard to your hand.
   * III - Until end of turn, whenever you cast an instant or sorcery spell, copy it. You may choose new targets for the copy. */

  int result;
  test_definition_t test;

  if ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CHAPTER1)
  {
    if (event == EVENT_CHAPTER1)
    {
      default_test_definition(&test, 0x30);
      test.type_flag = 2;
    }
    else
    {
      default_test_definition(&test, 8);
    }
    result = new_special_count_grave(player, &test);
    if (result != 0)
    {
      result = graveyard_has_shroud(player);
      if (result == 0)
      {
        new_global_tutor(player, player, 2, 1, 0, 1, &test);
      }
    }
  }
  else if (event == EVENT_CHAPTER3)
  {
    create_legacy_effect(player, card, the_mirari_conjecture_legacy);
  }
  result = saga(player, card, event, 3);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d4cb0; original cards/dominaria.c:3801. */
int card_thran_temporal_gateway(int player, int card, event_t event)
{
  /* Thran Temporal Gateway	|4
   * Legendary Artifact
   * |4, |T: You may put a historic permanent card from your hand onto the battlefield. */

  int result;
  char msg[50];
  test_definition_t this_test;
  if (event != EVENT_ACTIVATE)
  {
    if ((1U < (unsigned int)event - (unsigned int)EVENT_RESOLVE_ACTIVATION) &&
        (event != EVENT_CLEANUP))
    {
      return 0;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      memset(msg, 0, sizeof msg);
      memcpy(msg, "Select an historical permanent.", 32);
      new_default_test_definition(&this_test, 0x47, msg);
      this_test.special_selection_function = is_historic;
      new_global_tutor(player, player, 4, 4, 0, 2, &this_test);
    }
  }
  result = generic_activated_ability(player, card, event, 1, 4, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x22ca150; original cards/dominaria.c:3820. */
static int tiana_reanimation_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  result = duh_mode(player);
  result = eot_trigger_mode(player, card, event, 2, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (result != 0)
  {
    seek_grave_for_id_to_reanimate(player, card, instance->targets[0].player,
                                   (unsigned int)cards_data[instance->targets[0].card].id,
                                   REANIMATEXTRA_RETURN_TO_HAND2);
    kill_card(player, card, KILL_EXILE);
  }
  if (event == EVENT_CAN_SKIP_TURN)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22c9df0; original cards/dominaria.c:3833. */
int tiana_effect(int player, int card, event_t event)
{
  char attached_player;
  uint16_t returned_csvid;
  int32_t returned_iid;
  card_instance_t *instance;
  unsigned int pending_deaths;
  int legacy_card;
  card_instance_t *dying_or_legacy;
  int result_or_index;
  unsigned int stored_limit;
  test_definition_t test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (result_or_index =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           result_or_index != 0x486b))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (result_or_index = effect_follows_control_of_attachment(player, card, 0x4700),
         result_or_index != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (result_or_index = effect_follows_control_of_attachment(player, card, event),
         result_or_index != 0))
    {
      return 0;
    }
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {

      instance = get_card_instance(player, card);
      attached_player = instance->damage_target_player;

      if ((((-1 < attached_player) && (instance->damage_target_card == affected_card)) &&
           (attached_player == affected_card_controller)) &&
          (dying_or_legacy = get_card_instance((int)attached_player, affected_card),
           dying_or_legacy->kill_code != '\0'))
      {
        instance->damage_target_card = -1;
        instance->damage_target_player = -1;
        remove_status(player, card, 0x10000);
        if (instance->targets[0xb].player < 0)
        {
          instance->targets[0xb].player = 0;
        }
        SET_BYTE2(instance->targets[0xb].player) = 1;
      }
      default_test_definition(&test, 0x47);
      test.subtype = 0x4000;
      test.sub2 = 0x3001;
      test.subtype_flag = 3;
      count_for_gfp_ability_and_store_values(player, card, 0x77, player, 0, &test, 0xd0, 1);
    }
  }
  pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_deaths != 0)
  {
    result_or_index = check_status(player, card, 0x10000);
    if (result_or_index == 0)
    {

      instance = get_card_instance(player, card);

      stored_limit = (pending_deaths & 0xff) + 1;
      if (10 < stored_limit)
      {
        stored_limit = 10;
      }
      if ((pending_deaths & 0xff) != 0)
      {
        result_or_index = 1;
        do
        {
          if (instance->targets[result_or_index].player != -1)
          {

            legacy_card = create_legacy_effect(player, card, tiana_reanimation_legacy);
            dying_or_legacy = get_card_instance(player, legacy_card);
            returned_iid = instance->targets[result_or_index].card;
            dying_or_legacy->targets[0].player = instance->targets[result_or_index].player;
            dying_or_legacy->targets[0].card = returned_iid;
            returned_csvid = cards_data[instance->targets[result_or_index].card].id;
            dying_or_legacy->display_pic_csv_id = returned_csvid;
          }
          instance->targets[result_or_index].card = -1;
          instance->targets[result_or_index].player = -1;
          result_or_index = result_or_index + 1;
        } while (result_or_index < (int)stored_limit);
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) &&
      (result_or_index = check_status(player, card, 0x10000), result_or_index == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x22d4e30; original cards/dominaria.c:3892. */
int card_tiana_ships_caretaker(int player, int card, event_t event)
{
  /* Tiana, Ship's Caretaker	|3|R|W
   * Legendary Creature - Angel Artificer 3/3
   * Flying, first strike
   * Whenever an Aura or Equipment you control is put into a graveyard from the battlefield, you may return that card to its owner's hand at the beginning of the next end step. */

  card_instance_t *instance;
  int c;
  uint16_t returned_csvid;
  int32_t returned_iid;
  card_instance_t *existing_legacy;
  unsigned int pending_deaths;
  int is_effect;
  card_instance_t *return_effect;
  int created_or_index;
  unsigned int stored_limit;
  test_definition_t test;

  bool legacy_found = false;
  check_legend_rule(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          existing_legacy = in_play(player, c);

          if (((existing_legacy != NULL) &&
               (is_effect = is_what(player, c, 0x80), is_effect != 0)) &&
              (existing_legacy = get_card_instance(player, c),
               existing_legacy->info_slot_fn == tiana_effect))
          {
            legacy_found = true;
            break;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }

      if (!legacy_found)
      {
        created_or_index = create_targetted_legacy_effect(player, card, tiana_effect, player, card);
        instance = get_card_instance(player, created_or_index);
        instance->number_of_targets = '\0';
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        add_status(player, created_or_index, 0x10000);
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    default_test_definition(&test, 0x47);
    test.subtype = 0x4000;
    test.sub2 = 0x3001;
    test.subtype_flag = 3;
    count_for_gfp_ability_and_store_values(player, card, 0x77, player, 0, &test, 0xd0, 0);
  }
  instance = get_card_instance(player, card);

  if ((instance->kill_code == '\0') &&
      (pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY),
       pending_deaths != 0))
  {

    instance = get_card_instance(player, card);
    pending_deaths = pending_deaths & 0xff;

    stored_limit = pending_deaths;
    if (10 < pending_deaths)
    {
      stored_limit = 10;
    }
    if (pending_deaths != 0)
    {
      created_or_index = 0;
      do
      {
        if (instance->targets[created_or_index].player != -1)
        {

          return_effect = get_card_instance(
              player, create_legacy_effect(player, card, tiana_reanimation_legacy));
          returned_iid = instance->targets[created_or_index].card;
          return_effect->targets[0].player = instance->targets[created_or_index].player;
          return_effect->targets[0].card = returned_iid;
          returned_csvid = cards_data[instance->targets[created_or_index].card].id;
          return_effect->display_pic_csv_id = returned_csvid;
        }
        instance->targets[created_or_index].card = -1;
        instance->targets[created_or_index].player = -1;
        created_or_index = created_or_index + 1;
      } while (created_or_index < (int)stored_limit);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22cab10; original cards/dominaria.c:3951. */
int time_of_ice_effect(int player, int card, event_t event)
{
  card_instance_t *effect;
  card_instance_t *source_in_play;

  effect = in_play(player, card);
  if (effect != NULL)
  {
    effect = get_card_instance(player, card);
    if (-1 < effect->damage_target_player)
    {

      effect = get_card_instance(player, card);
      does_not_untap((int)effect->damage_target_player, effect->damage_target_card, event);
      source_in_play = in_play(effect->targets[0].player, effect->targets[0].card);
      if (source_in_play == NULL)
      {
        kill_card(player, card, KILL_EXILE);
      }
      if (((event == EVENT_CARDCONTROLLED) && (effect->targets[0].card == affected_card)) &&
          (effect->targets[0].player == affected_card_controller))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22d50a0; original cards/dominaria.c:3966. */
int card_time_of_ice(int player, int card, event_t event)
{
  /* Time of Ice	|3|U
   * Enchantment - Saga
   * I, II - Tap target creature an opponent controls. It doesn't untap during its controller's untap step for as long as you control ~.
   * III - Return all tapped creatures to their owners' hands. */

  int result;
  card_instance_t *instance;
  card_instance_t *freeze_effect;

  if ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CHAPTER1)
  {
    target_definition_t td;

    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    td.preferred_controller = td.allowed_controller;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_CREATURE");
      if (result != 0)
      {

        tap_card(instance->targets[0].player, instance->targets[0].card);
        result =
            create_targetted_legacy_effect(player, card, time_of_ice_effect,
                                           instance->targets[0].player, instance->targets[0].card);
        freeze_effect = get_card_instance(player, result);
        freeze_effect->targets[0].player = player;
        freeze_effect->targets[0].card = card;
        freeze_effect->number_of_targets = '\x01';
      }
    }
    instance->number_of_targets = '\0';
  }
  else if (event == EVENT_CHAPTER3)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 2);
    this_test.state = STATE_TAPPED;
    new_manipulate_all(player, card, 2, &this_test, ACT_BOUNCE);
  }
  return saga(player, card, event, 3);
}

/* Tolarian Scholar	|2|U
 * Creature - Human Wizard 2/3 */

/* ManalinkEh 2022: 0x22d5200; original cards/dominaria.c:4004. */
int card_torgaar_famine_incarnate(int player, int card, event_t event)
{
  /* Torgaar, Famine Incarnate	|6|B|B
   * Legendary Creature - Avatar 7/6
   * As an additional cost to cast this spell, you may sacrifice any number of creatures. This spell costs |2 less to cast for each creature sacrificed this way.
   * When ~ enters the battlefield, up to one target player's life total becomes half their starting life total, rounded down. */

  enum
  {
    CHOICE_SAC = 1,
    CHOICE_NORMAL = 2
  };

  int result_or_index;
  int available_sacrifices;
  unsigned int life_or_cost;
  card_instance_t *instance;
  char sac[151];

  check_legend_rule(player, card, event);
  result_or_index = duh_mode(player);
  etb_ability(player, card, event, (result_or_index != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    target_definition_t td;

    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    result_or_index = can_target(&td);
    if ((result_or_index != 0) &&
        (result_or_index = new_pick_target(&td, "TARGET_PLAYER", 0, 0), result_or_index != 0))
    {
      life_or_cost = get_starting_life_total(instance->targets[0].player);
      life_or_cost = round_down_value(life_or_cost);
      set_life_total(instance->targets[0].player, life_or_cost);
    }
  }
  else
  {
    if (event == EVENT_MODIFY_COST)
    {
      test_definition_t test;

      default_test_definition(&test, 2);
      result_or_index = get_updated_casting_cost(player, card, -1, EVENT_MODIFY_COST, -1);
      available_sacrifices = max_can_sacrifice_as_cost(player, card, &test);
      life_or_cost = result_or_index + available_sacrifices * -2;
      if (has_mana_multi(player, life_or_cost & (int)(life_or_cost ^ 0xffffffff) >> 0x1f, 2, 0, 0,
                         0, 0) == 0)
      {
        return 0;
      }
      null_casting_cost(player, card);
      return 1;
    }
    if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
        (player == affected_card_controller))
    {

      result_or_index = get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, -1);
      result_or_index = has_mana_multi(player, result_or_index, 2, 0, 0, 0, 0);
      available_sacrifices = card_torgaar_famine_incarnate(player, card, EVENT_MODIFY_COST);
      result_or_index =
          dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Sacrifice creatures",
                      available_sacrifices, 5, "Cast Normally", result_or_index, 10, 0, 0, 0, 0);
      if (result_or_index == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      life_or_cost = get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, -1);
      if (result_or_index == CHOICE_SAC)
      {
        test_definition_t test2;

        int max_required = round_up_value(life_or_cost);
        new_default_test_definition(&test2, 2, "Select a creature to sacrifice.");

        int available = max_can_sacrifice_as_cost(player, card, &test2);
        test2.qty = (uint16_t)MIN(max_required, available);
        result_or_index =
            mark_sacrifice(player, card, player, SAC_AS_COST | SAC_JUST_MARK, &test2, sac);
        if (spell_fizzled != 1)
        {

          life_or_cost = life_or_cost + result_or_index * -2;
          charge_mana_multi(player, life_or_cost & (int)(life_or_cost ^ 0xffffffff) >> 0x1f, 2, 0,
                            0, 0, 0);
        }
        if (spell_fizzled != 1)
        {

          if (0 < active_cards_count[player])
          {
            result_or_index = 0;
            do
            {
              if (sac[result_or_index] != '\0')
              {
                instance = get_card_instance(player, result_or_index);
                instance->state = instance->state & 0xffefffff;
                kill_card(player, result_or_index, KILL_SACRIFICE);
              }
              result_or_index = result_or_index + 1;
            } while (result_or_index < active_cards_count[player]);
          }
          return get_id(player, card);
        }
        ai_modifier = ai_modifier + -99;
        return 0;
      }
      if (result_or_index == CHOICE_NORMAL)
      {
        charge_mana_multi(player, life_or_cost, 2, 0, 0, 0, 0);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22d56b0; original cards/dominaria.c:4090. */
int card_traxos_scourge_of_kroog(int player, int card, event_t event)
{
  /* Traxos, Scourge of Kroog	|4
   * Legendary Artifact Creature - Construct 7/7
   * Trample
   * ~ enters the battlefield tapped and doesn't untap during your untap step.
   * Whenever you cast a historic spell, untap ~. */

  check_legend_rule(player, card, event);
  comes_into_play_tapped(player, card, event);
  does_not_untap(player, card, event);
  cast_historic_spell(player, card, event, 2);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  untap_card(player, card);
  return 0;
}

/* ManalinkEh 2022: 0x22ca230; original cards/dominaria.c:4107. */
static char *has_highest_power(int who_chooses, int player, int card, int targeting_player,
                               int targeting_card)
{
  int creature_power;
  int highest_power;
  char *requirement_error;
  creature_power = get_power(player, card);
  highest_power = get_highest_power(player);
  requirement_error = NULL;
  if (creature_power != highest_power)
  {
    requirement_error = "must have the greatest power among creatures you control";
  }
  return requirement_error;
}

/* ManalinkEh 2022: 0x22d5730; original cards/dominaria.c:4114. */
int card_triumph_of_gerrard(int player, int card, event_t event)
{
  /* Triumph of Gerrard	|1|W
   * Enchantment - Saga
   * I, II - Put a +1/+1 counter on target creature you control with the greatest power.
   * III - Target creature you control with the greatest power gains flying, first strike, and lifelink until end of turn. */

  int result;
  card_instance_t *instance;
  char buffer[75];
  target_definition_t td;

  base_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (intptr_t)has_highest_power;
  td.allow_cancel = 0;
  if ((event == EVENT_CHAPTER3) || ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CHAPTER1))
  {

    result = can_target(&td);
    if (result != 0)
    {
      result = get_highest_power(player);
      scnprintf(buffer, 0x4b, "Select target creature with power %d", result);
      new_pick_target(&td, buffer, 0, 0x81);
    }
    instance = get_card_instance(player, card);
    if ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CHAPTER1)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_P1_P1, 1);
      }
    }
    else
    {
      pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                             0, 0, 0x120, 2);
    }
  }
  result = saga(player, card, event, 3);
  return result;
}

/* Two-Headed Giant	|2|R|R
 * Creature - Giant Warrior 4/4
 * Whenever ~ attacks, flip two coins. If both coins come up heads, ~ gains double strike until end of turn. If both coins come up tails, ~ gains menace until end of turn. */

/* Untamed Kavu	|1|G
 * Creature - Kavu 2/2
 * Kicker |3
 * Vigilance, trample
 * If ~ was kicked, it enters the battlefield with three +1/+1 counters on it. */

/* ManalinkEh 2022: 0x22d58d0; original cards/dominaria.c:4156. */
int card_unwind(int player, int card, event_t event)
{
  /* Unwind	|2|U
   * Instant
   * Counter target noncreature spell. Untap up to three lands. */

  int result;
  target_definition_t td;
  counterspell_target_definition(player, card, &td, 0xff);
  td.illegal_type = TYPE_CREATURE;
  if (event == EVENT_RESOLVE_SPELL)
  {
    counterspell(player, card, EVENT_RESOLVE_SPELL, &td, 0);
    untap_lands(player, card, 3);
  }
  result = generic_spell(player, card, event, 2, &td, NULL, 1, NULL);
  return result;
}

/* ManalinkEh 2022: 0x22d5980; original cards/dominaria.c:4172. */
int card_urgoros_the_empty_one(int player, int card, event_t event)
{
  /* Urgoros, the Empty One	|4|B|B
   * Legendary Creature - Specter 4/3
   * Flying
   * Whenever ~ deals combat damage to a player, that player discards a card at random. If the player can't, you draw a card. */

  card_instance_t *instance;
  check_legend_rule(player, card, event);
  if (damage_dealt_by_me(player, card, event,
                         DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER) == 0)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  int first_hand = hand_count[0];
  unsigned int damage_counts = instance->targets[1].player;
  instance->targets[1].player = 0;
  int first_discards = 0;
  int second_discards = 0;
  int draws = 0;
  for (unsigned int hit = 0; hit != (damage_counts & 0xff); ++hit)
  {
    if (first_hand == 0)
      ++draws;
    else
    {
      ++first_discards;
      --first_hand;
    }
  }
  unsigned int second_hits = (damage_counts >> 8) & 0xff;
  if (second_hits)
  {
    int second_hand = hand_count[1];
    for (unsigned int hit = 0; hit != second_hits; ++hit)
    {
      if (second_hand == 0)
        ++draws;
      else
      {
        ++second_discards;
        --second_hand;
      }
    }
  }
  new_multidiscard(0, first_discards, 1, player);
  new_multidiscard(1, second_discards, 1, player);
  draw_cards(player, draws);
  return 0;
}

/* ManalinkEh 2022: 0x22c94c0; original cards/dominaria.c:4212. */
static int is_not_legendary_test(int unused1, intptr_t unused2, int player, int card)
{
  int result;
  result = is_legendary(player, card);
  return (unsigned int)(result == 0);
}

/* ManalinkEh 2022: 0x22d5a90; original cards/dominaria.c:4215. */
int card_urzas_ruinous_blast(int player, int card, event_t event)
{
  /* Urza's Ruinous Blast	|4|W
   * Legendary Sorcery
   * Exile all nonland permanents that aren't legendary. */

  unsigned int cast_result;
  int result;
  test_definition_t this_test;

  if (event == EVENT_CAN_CAST)
  {
    result = check_battlefield_for_subtype(player, 2, 0x2001);
    cast_result = 1;
    if (result == 0)
    {
      result = check_battlefield_for_subtype(player, 0x100, -1);
      cast_result = (unsigned int)(result != 0);
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
    else if (event == EVENT_RESOLVE_SPELL)
    {
      default_test_definition(&this_test, 1);
      this_test.type_flag = 1;
      this_test.special_selection_function = is_not_legendary_test;
      new_manipulate_all(player, card, 2, &this_test, ACT_KILL_REMOVE);
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
    cast_result = basic_spell(player, card, event);
  }
  return cast_result;
}

/* Urza's Tome	|2
 * Artifact
 * |3, |T: Draw a card. Then discard a card unless you exile a historic card from your graveyard. */

/* ManalinkEh 2022: 0x22d5bc0; original cards/dominaria.c:4245. */
int card_valduk_keeper_of_the_flame(int player, int card, event_t event)
{
  /* Valduk, Keeper of the Flame	|2|R
   * Legendary Creature - Human Shaman 3/2
   * At the beginning of combat on your turn, for each Aura and Equipment attached to ~, create a 3/1 |Sred Elemental creature token with trample and haste. Exile those tokens at the beginning of the next end step. */

  int equipment_count;
  int aura_count;
  token_generation_t token;

  check_legend_rule(player, card, event);
  if (beginning_of_combat(player, card, event, player) == 0)
  {
    return 0;
  }
  default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
  token.pow = 3;
  token.tou = 1;
  token.color_forced = '\x10';
  token.key_plus = 0x80;
  equipment_count = equipments_attached_to_me(player, card, 1);
  aura_count = count_auras_enchanting_me(player, card);
  token.qty = (char)aura_count + (char)equipment_count;
  token.special_code_on_generation = haste_and_remove_token_at_end_of_next_turn;
  generate_token(&token);
  return 0;
}

/* ManalinkEh 2022: 0x22d5c80; original cards/dominaria.c:4267. */
int card_verix_bladewing(int player, int card, event_t event)
{
  /* Verix Bladewing	|2|R|R
   * Legendary Creature - Dragon 4/4
   * Kicker |3
   * Flying
   * When ~ enters the battlefield, if it was kicked, create Karox Bladewing, a legendary 4/4 |Sred Dragon creature token with flying. */

  int result;
  check_legend_rule(player, card, event);
  kicker(player, card, event, 3, 0, 0, 0, 0, 0);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    result = kicked(player, card);
    if (result != 0)
    {
      generate_token_by_id(player, card, CARD_ID_KAROX_BLADEWING);
    }
  }
  return 0;
}

/* Karox Bladewing	""(token)
 * Legendary Creature - Dragon 4/4
 * Flying */

/* Vicious Offering	|1|B
 * Instant
 * Kicker-Sacrifice a creature.
 * Target creature gets -2/-2 until end of turn. If this spell was kicked, that creature gets -5/-5 until end of turn instead. */

/* Vodalian Arcanist	|1|U
 * Creature - Merfolk Wizard 1/3
 * |T: Add |C. Spend this mana only to cast an instant or sorcery spell. */

/* Voltaic Servant	|2
 * Artifact Creature - Construct 1/3
 * At the beginning of your end step, untap target artifact. */

/* Warcry Phoenix	|3|R
 * Creature - Phoenix 2/2
 * Flying, haste
 * Whenever you attack with three or more creatures, you may pay |2|R. If you do, return ~ from your graveyard to the battlefield tapped and attacking. */

/* Warlord's Fury	|R
 * Sorcery
 * Creatures you control gain first strike until end of turn.
 * Draw a card. */

/* ManalinkEh 2022: 0x22d5d40; original cards/dominaria.c:4310. */
int card_weatherlight(int player, int card, event_t event)
{
  /* Weatherlight	|4
   * Legendary Artifact - Vehicle 4/5
   * Flying
   * Whenever ~ deals combat damage to a player, look at the top five cards of your library. You may reveal a historic card from among them and put it into your hand. Put the rest on the bottom of your library in a random order.
   * Crew 3 */

  int result;
  int revealed_card;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  result =
      damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE);
  if (result == 0)
    return vehicle(player, card, event, 3);

  result = count_deck(player);
  if (result == 0)
    return vehicle(player, card, event, 3);
  if (5 < result)
  {
    result = 5;
  }

  new_default_test_definition(&this_test, 0xff, "Select an historic card.");
  this_test.create_minideck = (unsigned short)result;
  this_test.special_selection_function = is_historic_by_internal_id;
  this_test.no_shuffle = 1;
  revealed_card = new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
  bool reveal = false;
  if (ai_is_speculating == 1 || (player == 1 && !((unsigned char)trace_mode & 2)))
  {
    if (revealed_card < 0)
    {
      this_test.type = 0xff;
      revealed_card = new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
      reveal = revealed_card >= 0;
    }
    else
      reveal = true;
  }
  else
    reveal = revealed_card >= 0;
  if (reveal)
  {
    reveal_card(player, card, player, revealed_card);
    result = result + -1;
  }
  if (0 < result)
  {
    put_top_x_on_bottom(player, player, result);
  }
  return vehicle(player, card, event, 3);
}

/* Weight of Memory	|3|U|U
 * Sorcery
 * Draw three cards. Target player puts the top three cards of their library into their graveyard. */

/* ManalinkEh 2022: 0x22d5ef0; original cards/dominaria.c:4349. */
int card_whisper_blood_liturgist(int player, int card, event_t event)
{
  /* Whisper, Blood Liturgist	|3|B
   * Legendary Creature - Human Cleric 2/2
   * |T, Sacrifice two creatures: Return target creature card from your graveyard to the battlefield. */

  unsigned int first_sacrifice;
  unsigned int second_sacrifice;
  int result;
  test_definition_t test;
  test_definition_t sac_test;
  check_legend_rule(player, card, event);
  if ((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE))
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    new_default_test_definition(&test, 2, "Select target creature card to reanimate.");
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      return 0;
    }
    result = validate_target_from_grave_source(player, card, player, 0);
    if (result == -1)
    {
      return 0;
    }
    reanimate_permanent(player, card, player, result, REANIMATE_DEFAULT);
    return 0;
  }
  new_default_test_definition(&test, 2, "Select target creature card to reanimate.");
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = can_sacrifice_type_as_cost(player, 2, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    result = generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE,
                                                            GAA_UNTAPPED, 0, 0, 0, 0, 0, 0, 0, NULL,
                                                            NULL, TYPE_NONE, &test, 8);
    return (unsigned int)(result != 0);
  }
  new_default_test_definition(&sac_test, 2, "Select a creature to sacrifice.");
  first_sacrifice = new_sacrifice(player, card, player,
                                  SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &sac_test);
  if (first_sacrifice == 0)
  {
    second_sacrifice = 0xffffffff;
    spell_fizzled = 1;
  }
  else
  {
    second_sacrifice = new_sacrifice(player, card, player,
                                     SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &sac_test);
    if (second_sacrifice == 0)
      spell_fizzled = 1;
  }
  if (-1 < (int)first_sacrifice)
  {
    state_untargettable((int)first_sacrifice >> 0x10 & 0xff, first_sacrifice >> 0x18, 0);
  }
  if (-1 < (int)second_sacrifice)
  {
    state_untargettable((int)second_sacrifice >> 0x10 & 0xff, second_sacrifice >> 0x18, 0);
  }
  generic_activated_ability_with_target_in_grave(player, card, EVENT_ACTIVATE, GAA_UNTAPPED, 0, 0,
                                                 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test, 8);
  if (spell_fizzled != 1)
  {
    kill_card((int)first_sacrifice >> 0x10 & 0xff, first_sacrifice >> 0x18, KILL_SACRIFICE);
    kill_card((int)second_sacrifice >> 0x10 & 0xff, second_sacrifice >> 0x18, KILL_SACRIFICE);
  }
  return 0;
}

/* Wild Onslaught	|3|G
 * Instant
 * Kicker |4
 * Put a +1/+1 counter on each creature you control. If this spell was kicked, put two +1/+1 counters on each creature you control instead. */

/* Windgrace Acolyte	|4|B
 * Creature - Cat Warrior 3/2
 * Flying
 * When ~ enters the battlefield, put the top three cards of your library into your graveyard and you gain 3 life. */

/* ManalinkEh 2022: 0x22d6240; original cards/dominaria.c:4413. */
int card_wizards_lightning(int player, int card, event_t event)
{
  /* Wizard's Lightning	|2|R
   * Instant
   * This spell costs |2 less to cast if you control a Wizard.
   * ~ deals 3 damage to any target. */

  int result;

  if (event == EVENT_MODIFY_COST)
  {
    result = check_battlefield_for_subtype(player, -1, 0xd6);
    if (result != 0)
    {
      COST_COLORLESS = COST_COLORLESS + -2;
    }
  }
  result = card_lightning_bolt(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22d62a0; original cards/dominaria.c:4426. */
int card_wizards_retort(int player, int card, event_t event)
{
  /* Wizard's Retort	|1|U|U
   * Instant
   * This spell costs |1 less to cast if you control a Wizard.
   * Counter target spell. */

  int result;
  if (event == EVENT_MODIFY_COST)
  {
    result = check_battlefield_for_subtype(player, -1, 0xd6);
    if (result != 0)
    {
      COST_COLORLESS = COST_COLORLESS + -1;
    }
  }
  result = card_counterspell(player, card, event);
  return result;
}

/* Yargle, Glutton of Urborg	|4|B
 * Legendary Creature - Frog Spirit 9/3 */

/* ManalinkEh 2022: 0x22d6300; original cards/dominaria.c:4442. */
int card_yavimaya_sapherd(int player, int card, event_t event)
{
  /* Yavimaya Sapherd	|2|G
   * Creature - Fungus 2/2
   * When ~ enters the battlefield, create a 1/1 |Sgreen Saproling creature token. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_SAPROLING);
  return 0;
}

/* ManalinkEh 2022: 0x22d6360; original cards/dominaria.c:4455. */
int card_yawgmoths_vile_offering(int player, int card, event_t event)
{
  /* Yawgmoth's Vile Offering	|4|B
   * Legendary Sorcery
   * Put up to one target creature or planeswalker card from a graveyard onto the battlefield under your control. Destroy up to one target creature or planeswalker. Exile ~. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    result = check_battlefield_for_subtype(player, 2, 0x2001);
    if (result != 0)
    {
      return 1;
    }
    result = check_battlefield_for_subtype(player, 0x100, -1);
    return (unsigned int)(result != 0);
  }
  if (event != EVENT_CAST_SPELL)
  {
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      new_default_test_definition(&test, 0x102, "Select target creature or planeswalker card.");
      default_target_definition(player, card, &td, 0x102);
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {
        result = validate_target_from_grave_source(player, card, instance->targets[1].player, 2);
        if (result != -1)
        {
          reanimate_permanent(player, card, instance->targets[1].player, result, REANIMATE_DEFAULT);
        }
        result = validate_target(player, card, &td, 0);
        if (result != 0)
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
        kill_card(player, card, KILL_EXILE);
      }
    }
    else
    {
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
      new_default_test_definition(&test, 0x102, "Select target creature or planeswalker card.");
      default_target_definition(player, card, &td, 0x102);
      get_card_instance(player, card);
    }
    return basic_spell(player, card, event);
  }
  if (card != affected_card)
  {
    return 0;
  }
  if (player != affected_card_controller)
  {
    return 0;
  }
  new_default_test_definition(&test, 0x102, "Select target creature or planeswalker card.");
  default_target_definition(player, card, &td, 0x102);
  get_card_instance(player, card);
  bool grave_target_available = false;
  result = new_special_count_grave(player, &test);
  if (result != 0 && !graveyard_has_shroud(player))
    grave_target_available = true;
  else
    grave_target_available =
        new_special_count_grave(1 - player, &test) != 0 && !graveyard_has_shroud(1 - player);
  if (grave_target_available)
    select_target_from_either_grave(player, card, SFG_NO_SPELL_FIZZLED, 2, 2, &test, 1, 2);
  result = generic_spell(player, card, EVENT_CAN_CAST, 0x20, &td, "TARGET_CREATURE_OR_PLANESWALKER",
                         1, NULL);
  if (result != 0)
  {
    generic_spell(player, card, EVENT_CAST_SPELL, 0x20, &td, "TARGET_CREATURE_OR_PLANESWALKER", 1,
                  NULL);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x22d6710; original cards/dominaria.c:4499. */
int card_zahid_djinn_of_the_lamp(int player, int card, event_t event)
{
  /* Zahid, Djinn of the Lamp	|4|U|U
   * Legendary Creature - Djinn 5/6
   * You may pay |3|U and tap an untapped artifact you control rather than pay this spell's mana cost.
   * Flying */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  test_definition_t this_test;

  default_test_definition(&this_test, 0x40);
  this_test.state = 0x10;
  this_test.state_flag = 1;
  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    result = has_mana_multi(player, 3, 0, 1, 0, 0, 0);
    if ((result != 0) &&
        (result = check_battlefield_for_special_card(player, card, player, 0, &this_test),
         result != 0))
    {
      null_casting_cost(player, card);
      instance->info_slot = 1;
      return 0;
    }

    instance->info_slot = 0;
    result = get_updated_casting_cost(player, card, -1, EVENT_MODIFY_COST, 4);
    result = has_mana_multi(player, result, 0, 1, 0, 0, 0);
    if (result != 0)
    {
      return 0;
    }
    infinite_casting_cost();
    return 0;
  }
  if (event != EVENT_CAST_SPELL)
  {
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
  if (instance->info_slot != 1)
  {
    return 0;
  }

  result = get_id(player, card);
  bool alternative = !has_mana_to_cast_id(player, EVENT_CAST_SPELL, result);
  bool cancelled = false;
  if (!alternative && has_mana_multi(player, 3, 0, 1, 0, 0, 0) &&
      check_battlefield_for_special_card(player, card, player, 0, &this_test))
  {
    result = do_dialog(player, player, card, -1, -1,
                       " Play normally\n Pay 3U and tap an artifact\n Cancel", 1);
    if (result == 1)
      alternative = true;
    else if (result != 0)
    {
      spell_fizzled = 1;
      cancelled = true;
    }
  }
  if (!cancelled)
  {
    if (alternative)
    {
      charge_mana_multi(player, 3, 0, 1, 0, 0, 0);
      if (spell_fizzled != 1)
      {
        if (player == 1)
        {
          result = check_battlefield_for_special_card(1, card, 1, 2, &this_test);
          if (result == -1)
            spell_fizzled = 1;
          else
            tap_card(1, result);
        }
        else
        {
          default_target_definition(player, card, &td, 0x40);
          td.allowed_controller = player;
          td.preferred_controller = player;
          td.illegal_abilities = 0;
          td.illegal_state = TARGET_STATE_BLOCKING;
          if (new_pick_target(&td, "Select an artifact to tap", 0, 0x81))
            tap_card(instance->targets[0].player, instance->targets[0].card);
        }
      }
    }
    else if (instance->info_slot == 1)
    {
      result = get_id(player, card);
      charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
    }
  }
  if (spell_fizzled == 1)
  {
    result = instance->targets[0].card;
    if (result != -1)
      untap_card_no_event(instance->targets[0].player, result);
  }
  return 0;
}

/* ManalinkEh 2022: 0x22d6b00; original cards/dominaria.c:4580. */
int card_zhalfirin_void(int player, int card, event_t event)
{
  /* Zhalfirin Void	""
   * Land
   * When ~ enters the battlefield, scry 1.
   * |T: Add |C. */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    scry(player, 1);
  }
  result = plain_mana_producer(player, card, event);
  return result;
}
