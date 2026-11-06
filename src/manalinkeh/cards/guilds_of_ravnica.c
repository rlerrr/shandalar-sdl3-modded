#include "manalink.h"

int jumpstart(int player, int card, event_t event, int id);

static int effect_nullhide_ferox(int player, int card, event_t event);
static int mnemonic_betrayal_legacy(int player, int card, event_t event);

static void mentor(int player, int card, event_t event);

static int midnight_reaper_effect(int player, int card, event_t event);

int hybrid_split_card(int player, int card, event_t event, int card_id_first_half,
                      int can_play_first_half, int priority_first_half, int id_second_half,
                      int can_play_second_half, int priority_second_half);

#ifndef SHANDALAR

int card_portcullis_vine(int player, int card, event_t event);

#endif

/* ManalinkEh 2022: 0x26198e0; original cards/guilds_of_ravnica.c:4. */
void surveil(int player, int number)
{
  int to_look = count_deck(player);
  if (number < to_look)
    to_look = number;
  if (to_look != 0)
  {
    int added[to_look];
    int ac = 0;
    do
    {
      test_definition_t this_test;
      new_default_test_definition(&this_test, TYPE_ANY, "Select a card to put into your graveyard");
      this_test.create_minideck = (unsigned short)to_look;
      int selected =
          new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MIN_VALUE, -1, &this_test);
      if (selected == -1)
        break;
      added[ac] = deck_ptr[player][selected];
      remove_card_from_deck(player, selected);
      ++ac;
      --to_look;
    } while (to_look != 0);
    if (ac != 0)
    {
      for (int i = 0; i < ac; ++i)
      {
        int card_added = add_card_to_hand(player, added[i]);
        put_on_top_of_deck(player, card_added);
      }
      mill(player, ac);
    }
    if (to_look != 0)
      rearrange_top_x(player, player, to_look);
  }
  if (number > 0)
    dispatch_event(player, -1, EVENT_SURVEIL);
}

/* ManalinkEh 2022: 0x2619aa0; original cards/guilds_of_ravnica.c:41. */
int jumpstart(int player, int card, event_t event, int id)
{
  int result;
  test_definition_t this_test;
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = has_mana_to_cast_id(player, EVENT_GRAVEYARD_ABILITY, id);
    if ((result != 0) && (hand_count[player] != 0))
    {
      result = get_internal_card_id_from_csv_id(id);
      result = can_legally_play_iid_now(player, result, EVENT_GRAVEYARD_ABILITY);
      if (result != 0)
      {
        return 2;
      }
    }
  }
  else if ((event == EVENT_PAY_FLASHBACK_COSTS) &&
           (charge_mana_from_id(player, -1, EVENT_PAY_FLASHBACK_COSTS, id), spell_fizzled != 1))
  {
    new_default_test_definition(&this_test, 0xff, "Select a card to discard.");
    result = new_select_a_card(player, player, 4, 0, 3, -1, &this_test);
    if (result == -1)
    {
      spell_fizzled = 1;
      return 0;
    }
    discard_card(player, result);
    return 2;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2619bf0; original cards/guilds_of_ravnica.c:69. */
int gnr_locket(int player, int card, event_t event)
{
  enum
  {
    CHOICE_MANA = 1,
    CHOICE_DRAW = 2
  };

  unsigned char mana_colors;
  int color_or_cost;
  card_instance_t *instance;
  unsigned int color_bit;
  int color_or_available;
  bool speculating;
  int second_color;
  int first_color;

  if ((event != EVENT_CAN_ACTIVATE) &&
      (((event == EVENT_ACTIVATE || (event == EVENT_RESOLVE_ACTIVATION)) ||
        (event == EVENT_CLEANUP))))
  {
    color_or_cost = 1;
    instance = get_card_instance(player, card);
    second_color = -1;
    first_color = -1;
    mana_colors = instance->mana_color;
    do
    {
      color_bit = color_to_color_test(color_or_cost);
      color_or_available = first_color;
      if (((mana_colors & 0x3e & color_bit) != 0) &&
          (color_or_available = color_or_cost, first_color != -1))
      {
        second_color = color_or_cost;
        color_or_available = first_color;
      }
      first_color = color_or_available;
      color_or_cost = color_or_cost + 1;
    } while (color_or_cost != 5);
    if (event != EVENT_ACTIVATE)
    {
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot != CHOICE_DRAW)
      {
        return 0;
      }
      draw_cards(player, 2);
      return 0;
    }
    color_or_cost = get_cost_mod_for_activated_abilities(player, card, 0, 0, 0, 0, 0, 0);
    color_or_available = paying_mana();
    if (((color_or_available == 0) &&
         (color_or_available = has_mana_hybrid(player, 5, first_color, second_color, color_or_cost),
          color_or_available != 0)) &&
        (color_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 0, 0,
                                                        0, 0, 0, 0, 0, NULL, NULL),
         color_or_available != 0))
    {
      speculating = ai_is_speculating == 1;
      instance->info_slot = CHOICE_DRAW;
      bool draw_mode = speculating || (player == 1 && !((unsigned char)trace_mode & 2));
      if (!draw_mode)
      {
        if (!dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1, "Draw 2 cards",
                         1, 1, 0, 0, 0, 0))
        {
          spell_fizzled = 1;
          return 0;
        }
        if (instance->info_slot == CHOICE_DRAW)
          draw_mode = true;
        else if (instance->info_slot != CHOICE_MANA)
          return 0;
      }
      if (draw_mode)
      {
        instance->state = instance->state | 0x10;
        if (charge_mana_hybrid(player, card, 4, first_color, second_color, color_or_cost) != 0)
        {
          generic_activated_ability(player, card, EVENT_ACTIVATE, 8, 0, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL);
        }
        if (spell_fizzled == 1)
        {
          untap_card_no_event(player, card);
        }
        tapped_for_mana_color = -1;
        return 0;
      }
    }
    else
    {
      instance->info_slot = CHOICE_MANA;
    }
    event = EVENT_ACTIVATE;
  }

  return mana_producer(player, card, event);
}

/* ManalinkEh 2022: 0x26196c0; original cards/guilds_of_ravnica.c:133. */
static void mentor(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;
  char buffer[75];
  target_definition_t td;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {

    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_ATTACKING;
    result = get_power(player, card);
    td.power_requirement = result - 1U | 0x2000;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    instance->number_of_targets = '\0';
    result = get_power(player, card);
    scnprintf(buffer, 0x4b, " Choose target attacking creature with power %d or less.",
              result + -1);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, buffer, 0, 0x80);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_P1_P1, 1);
      }
    }
  }
  return;
}

/* ManalinkEh 2022: 0x2619f70; original cards/guilds_of_ravnica.c:157. */
int hybrid_split_card(int player, int card, event_t event, int card_id_first_half,
                      int can_play_first_half, int priority_first_half, int id_second_half,
                      int can_play_second_half, int priority_second_half)
{
  int result;

  modify_cost_for_hybrid_spells(player, card, event, 0);
  result = generic_split_card(player, card, event, card_id_first_half, can_play_first_half,
                                   priority_first_half, id_second_half, can_play_second_half,
                                   priority_second_half);
  return result;
}

/* ManalinkEh 2022: 0x261a000; original cards/guilds_of_ravnica.c:171. */
int card_arboretum_elemental(int player, int card, event_t event)
{
  /* Arboretum Elemental	|7|G|G
   * Creature - Elemental 7/5
   * Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
   * Hexproof (This creature can't be the target of spells or abilities your opponents control.) */

  hexproof(player, card, event);
  return generic_spell_with_convoke(player, card, event);
}

/* ManalinkEh 2022: 0x261a040; original cards/guilds_of_ravnica.c:200. */
int card_assassins_trophy(int player, int card, event_t event)
{
  /* Assassin's Trophy	|B|G
   * Instant
   * Destroy target permanent an opponent controls. Its controller may search their library for a basic land card, put it onto the battlefield, then shuffle. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  test_definition_t this_test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.allowed_controller = 1 - player;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {

      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      result = instance->targets[0].player;
      if ((result == 1) ||
          (result = do_dialog(result, player, card, -1, -1, " Search a basic land\n Pass", 0),
           result == 0))
      {
        new_default_test_definition(&this_test, 1, "Select a basic land card.");
        this_test.subtype = 0x2000;
        result = instance->targets[0].player;
        new_global_tutor(result, result, 1, 4, 0, 5, &this_test);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td,
                         "Select target permanent an opponent controls.", 1, NULL);
}

/* ManalinkEh 2022: 0x261a290; original cards/guilds_of_ravnica.c:268. */
int card_aurelia_exemplar_of_justice(int player, int card, event_t event)
{
  /* Aurelia, Exemplar of Justice	|2|R|W
   * Legendary Creature - Angel 2/5
   * Flying
   * Mentor (Whenever this creature attacks, put a +1/+1 counter on target attacking creature with lesser power.)
   * At the beginning of combat on your turn, choose up to one target creature you control. Until end of turn, that creature gets +2/+0, gains trample if it's red, and gains vigilance if it's white. */

  int result;
  card_instance_t *instance;
  unsigned int target_colors;
  target_definition_t td;

  check_legend_rule(player, card, event);
  mentor(player, card, event);
  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.allow_cancel = 2;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        target_colors = get_color(instance->targets[0].player, instance->targets[0].card);
        pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                               2, 0, (target_colors & 0x10) << 3, target_colors >> 2 & 8);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x261a3a0; original cards/guilds_of_ravnica.c:295. */
int card_barging_sergeant(int player, int card, event_t event)
{
  /* Barging Sergeant	|4|R
   * Creature - Minotaur Soldier 4/2
   * Haste
   * Mentor (Whenever this creature attacks, put a +1/+1 counter on target attacking creature with lesser power.) */

  haste(player, card, event);
  mentor(player, card, event);
  return 0;
}

/* ManalinkEh 2022: 0x261a3e0; original cards/guilds_of_ravnica.c:324. */
int card_beacon_bolt(int player, int card, event_t event)
{
  /* Beacon Bolt	|1|U|R
   * Sorcery
   * ~ deals damage to target creature equal to the total number of instant and sorcery cards you own in exile and in your graveyard.
   * Jump-start (You may cast this card from your graveyard by discarding a card in addition to paying its other costs. Then exile this card.) */

  int exiled_spells;
  int grave_spells;
  target_definition_t td;

  if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
  {
    return jumpstart(player, card, event, 0x4b5b);
  }
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
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
  }
  else
  {
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 2);
      if (event == EVENT_RESOLVE_SPELL)
      {

        exiled_spells = count_rfg_by_type(player, 0x38);
        grave_spells = count_graveyard_by_type(player, 0x38);
        damage_target0(player, card, grave_spells + exiled_spells);
        kill_card(player, card, KILL_DESTROY);
      }
      return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  default_target_definition(player, card, &td, 2);
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x261a5c0; original cards/guilds_of_ravnica.c:360. */
int card_beast_whisperer(int player, int card, event_t event)
{
  /* Beast Whisperer	|2|G|G
   * Creature - Elf Druid 2/3
   * Whenever you cast a creature spell, draw a card. */

  sp_ability_trigger(player, event, 2, TYPE_CREATURE, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  draw_cards(player, 1);
  return 0;
}

/* ManalinkEh 2022: 0x261a630; original cards/guilds_of_ravnica.c:377. */
int card_blade_instructor(int player, int card, event_t event)
{
  /* Blade Instructor	|2|W
   * Creature - Human Soldier 3/1
   * Mentor (Whenever this creature attacks, put a +1/+1 counter on target attacking creature with lesser power.) */

  mentor(player, card, event);
  return 0;
}

/* ManalinkEh 2022: 0x261a650; original cards/guilds_of_ravnica.c:405. */
int card_boros_challenger(int player, int card, event_t event)
{
  /* Boros Challenger	|R|W
   * Creature - Human Soldier 2/3
   * Mentor (Whenever this creature attacks, put a +1/+1 counter on target attacking creature with lesser power.)
   * |2|R|W: This creature gets +1/+1 until end of turn. */

  int result;

  mentor(player, card, event);
  result = generic_shade(player, card, event, 0, 2, 0, 0, 0, 1, 1, 1, 1, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x261a6e0; original cards/guilds_of_ravnica.c:449. */
int card_camaraderie(int player, int card, event_t event)
{
  /* Camaraderie	|4|G|W
   * Sorcery
   * You gain X life and draw X cards, where X is the number of creatures you control. Creatures you control get +1/+1 until end of turn. */

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
      result = count_permanents_by_type(player, TYPE_CREATURE);
      gain_life(player, result);
      draw_cards(player, result);
      pump_creatures_until_eot_merge_pt(player, card, player, 1, 1, NULL);
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return basic_spell(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x261a7d0; original cards/guilds_of_ravnica.c:504. */
int card_chance_for_glory(int player, int card, event_t event)
{
  /* Chance for Glory	|1|R|W
   * Instant
   * Creatures you control gain indestructible. Take an extra turn after this one. At the beginning of that turn's end step, you lose the game. */

  int result;
  card_instance_t *instance;

  if (event != EVENT_RESOLVE_SPELL)
  {
    result = basic_spell(player, card, event);
    return result;
  }
  pump_creatures_until_eot(player, card, player, 1, 0, 0, 0, -0x7ffff800, NULL);
  result = check_battlefield_for_id(1 - player, CARD_ID_STRANGLEHOLD);
  if (result == 0)
  {

    result = create_legacy_effect(player, card, last_chance_legacy);
    alternate_legacy_text(2, player, result);
    instance = get_card_instance(player, result);
    instance->targets[1].player = 2;
    result = time_walk_effect(player, card);
    alternate_legacy_text(2, player, result);
  }
  else
  {
    kill_card(player, card, KILL_DESTROY);
  }
  kill_card(player, card, KILL_DESTROY);
  return 0;
}
#endif

/* ManalinkEh 2022: 0x261a8f0; original cards/guilds_of_ravnica.c:539. */
int card_chemisters_insight(int player, int card, event_t event)
{
  /* Chemister's Insight	|3|U
   * Instant
   * Draw two cards.
   * Jump-start (You may cast this card from your graveyard by discarding a card in addition to paying its other costs. Then exile this card.) */

  int result;
  if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
  {
    result = jumpstart(player, card, event, 0x4b6e);
    return result;
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
      draw_cards(player, 2);
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

/* ManalinkEh 2022: 0x261a9c0; original cards/guilds_of_ravnica.c:565. */
int card_circuitous_route(int player, int card, event_t event)
{
  /* Circuitous Route	|3|G
   * Sorcery
   * Search your library for up to two basic land cards and/or Gate cards, put them onto the battlefield tapped, then shuffle. */

  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      int available_lands = get_total_available_lands(player);
      ai_modifier = ai_modifier + (unsigned int)((available_lands < 6) ? 0 : -72) + 0x30;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    test_definition_t test;
    new_default_test_definition(&test, 1, "Choose a basic land card or Gate card.");
    test.subtype = 0x2000;
    test.sub2 = 0x500c;
    test.subtype_flag = 3;
    test.qty = 2;
    test.no_shuffle = 1;
    new_global_tutor(player, player, 1, 5, 0, 1, &test);
    shuffle(player);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x261aae0; original cards/guilds_of_ravnica.c:638. */
int card_conclave_tribunal(int player, int card, event_t event)
{
  /* Conclave Tribunal	|3|W
   * Enchantment
   * Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
   * When this enchantment enters, exile target nonland permanent an opponent controls until this enchantment leaves the battlefield. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  return_from_oblivion(player, card, event);
  generic_spell_with_convoke(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 0x47);
    td.allowed_controller = 1 - player;
    td.illegal_type = TYPE_LAND;
    result = can_target(&td);
    if (result != 0)
    {
      result =
          new_pick_target(&td, "Select target nonland permanent an opponent controls.", 0, 0x80);
      if (result != 0)
      {
        instance = get_card_instance(player, card);
        obliviation(player, card, instance->targets[0].player, instance->targets[0].card);
        instance->number_of_targets = '\0';
      }
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x261abf0; original cards/guilds_of_ravnica.c:699. */
int card_crackling_drake(int player, int card, event_t event)
{
  /* Crackling Drake	|U|U|R|R
   * Creature - Drake * /4
   * Flying
   * ~'s power is equal to the total number of instant and sorcery cards you own in exile and in your graveyard.
   * When this creature enters, draw a card. */

  int exiled_spells;
  int grave_spells;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    if (((event == EVENT_POWER) && (card == affected_card)) && (player == affected_card_controller))
    {
      exiled_spells = count_rfg_by_type(player, 0x38);
      grave_spells = count_graveyard_by_type(player, 0x38);
      set_characteristic_pt(exiled_spells + grave_spells);
    }
    return 0;
  }
  draw_a_card(player);
  return 0;
}

/* ManalinkEh 2022: 0x261ac90; original cards/guilds_of_ravnica.c:719. */
int card_creeping_chill(int player, int card, event_t event)
{
  /* Creeping Chill	|3|B
   * Sorcery
   * ~ deals 3 damage to each opponent and you gain 3 life.
   * When ~ is put into your graveyard from your library, you may exile it. If you do, ~ deals 3 damage to each opponent and you gain 3 life. */

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
  damage_player(1 - player, 3, player, card);
  gain_life(player, 3);
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

/* ManalinkEh 2022: 0x261ad20; original cards/guilds_of_ravnica.c:740. */
int card_crush_contraband(int player, int card, event_t event)
{
  /* Crush Contraband	|3|W
   * Instant
   * Choose one or both -
   * * Exile target artifact.
   * * Exile target enchantment. */

  enum
  {
    CHOICE_ARTIFACT = 1,
    CHOICE_ENCHANTMENT = 2,
    CHOICE_BOTH = 3
  };

  int artifact_priority_or_valid;
  unsigned int selected_modes;
  card_instance_t *instance;
  uint32_t both_priority;
  uint32_t enchantment_priority;
  int enchantment_available;
  bool opponent_artifact;
  target_definition_t *artifact_target;
  target_definition_t *enchantment_target;
  unsigned int both_available;
  int artifact_available;
  target_definition_t td_a;
  target_definition_t td_e;
  target_definition_t td_opp_a;

  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td_a, 0x40);
  default_target_definition(player, card, &td_e, 4);
  both_priority = 3;
  enchantment_priority = 1;
  artifact_priority_or_valid = 1;
  if (event == EVENT_RESOLVE_SPELL)
  {
    enchantment_available = 0;
    artifact_available = 0;
    both_available = 0;
  }
  else
  {
    artifact_available = can_target(&td_a);
    enchantment_available = can_target(&td_e);
    opponent_artifact = false;
    if (artifact_available != 0 && player == 1)
    {
      default_target_definition(1, card, &td_opp_a, 0x40);
      td_opp_a.allowed_controller = 0;
      opponent_artifact = can_target(&td_opp_a) != 0;
    }
    both_available = enchantment_available != 0 && artifact_available != 0;
    bool opponent_enchantment = false;
    if (enchantment_available != 0 && player == 1)
    {
      default_target_definition(1, card, &td_opp_a, 4);
      td_opp_a.allowed_controller = 0;
      opponent_enchantment = can_target(&td_opp_a) != 0;
    }
    if (opponent_enchantment)
    {
      enchantment_priority = 5;
      if (opponent_artifact)
      {
        both_priority = 0x19;
        artifact_priority_or_valid = 5;
      }
    }
    else
      artifact_priority_or_valid = (opponent_artifact ? 5U : 1U);
  }
  enchantment_target = &td_e;
  artifact_target = &td_a;
  selected_modes =
      dialog_impl(player, card, event, DLG_RANDOM, "Exile artifact", artifact_available,
                  artifact_priority_or_valid, "Exile enchantment", enchantment_available,
                  enchantment_priority, "Both", both_available, both_priority, 0, 0, 0, 0);
  if (event != EVENT_CAN_CAST)
  {
    if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
        (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
      if ((selected_modes & CHOICE_ARTIFACT) != 0)
      {
        new_pick_target(artifact_target, "TARGET_ARTIFACT", 0, 1);
      }
      if ((selected_modes & CHOICE_ENCHANTMENT) != 0)
      {
        new_pick_target(enchantment_target, "TARGET_ENCHANTMENT", selected_modes & 1, 1);
      }
    }
    else
    {

      instance = get_card_instance(player, card);
      if (((selected_modes & CHOICE_ARTIFACT) != 0) &&
          (artifact_priority_or_valid = validate_target(player, card, artifact_target, 0),
           artifact_priority_or_valid != 0))
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      }
      if ((selected_modes & CHOICE_ENCHANTMENT) != 0)
      {
        selected_modes = selected_modes & CHOICE_ARTIFACT;
        if (validate_target(player, card, enchantment_target, selected_modes) != 0)
        {
          kill_card(instance->targets[selected_modes].player,
                    instance->targets[selected_modes].card, KILL_EXILE);
        }
      }
      kill_card(player, card, KILL_DESTROY);
    }
    return 0;
  }
  return selected_modes;
}

/* ManalinkEh 2022: 0x261b1b0; original cards/guilds_of_ravnica.c:828. */
int card_dawn_of_hope(int player, int card, event_t event)
{
  /* Dawn of Hope	|1|W
   * Enchantment
   * Whenever you gain life, you may pay |2. If you do, draw a card.
   * |3|W: Create a 1/1 white Soldier creature token with lifelink. */

  int result;
  token_generation_t token;

  result = duh_mode(player);
  result = trigger_gain_life(player, card, event, player, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (result != 0)
  {
    result = has_mana(player, 7, 2);
    if (result != 0)
    {
      result =
          charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 2);
      if (result != 0)
      {
        draw_cards(player, 1);
      }
    }
  }
  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      result = global_enchantment(player, card, event);
      return result;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      default_token_definition(player, card, CARD_ID_SOLDIER, &token);
      token.s_key_plus = 2;
      generate_token(&token);
    }
  }
  return generic_activated_ability(player, card, event, 0, 3, 0, 0, 0, 0, 1, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x261b340; original cards/guilds_of_ravnica.c:868. */
int card_deafening_clarion(int player, int card, event_t event)
{
  /* Deafening Clarion	|1|R|W
   * Sorcery
   * Choose one or both -
   * * ~ deals 3 damage to each creature.
   * * Creatures you control gain lifelink until end of turn. */

  enum
  {
    CHOICE_DAMAGE = 1,
    CHOICE_LIFELINK = 2,
    CHOICE_BOTH = 3
  };

  unsigned int selected_modes;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    return dialog_impl(player, card, EVENT_CAN_CAST, DLG_RANDOM, "Damage all creatures", 1, 5,
                       "Your creatures gain lifelink", 1, 5, "Both", 1, 7, 0, 0, 0, 0);
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
    selected_modes =
        dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, "Damage all creatures", 1, 5,
                    "Your creatures gain lifelink", 1, 5, "Both", 1, 7, 0, 0, 0, 0);
    if ((card == affected_card) && (player == affected_card_controller))
    {
      return basic_spell(player, card, EVENT_CAST_SPELL);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET) &&
        (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }

    selected_modes = dialog_impl(player, card, event, DLG_RANDOM, "Damage all creatures", 1, 5,
                                 "Your creatures gain lifelink", 1, 5, "Both", 1, 7, 0, 0, 0, 0);
  }
  if ((selected_modes & CHOICE_DAMAGE) != 0)
  {
    default_test_definition(&test, 2);
    new_damage_all(player, card, 2, 3, 1, &test);
  }
  if ((selected_modes & CHOICE_LIFELINK) != 0)
  {
    pump_subtype_until_eot(player, card, player, -1, 0, 0, 0, 2);
  }
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

/* ManalinkEh 2022: 0x261b640; original cards/guilds_of_ravnica.c:956. */
int card_direct_current(int player, int card, event_t event)
{
  /* Direct Current	|1|R|R
   * Sorcery
   * ~ deals 2 damage to any target.
   * Jump-start (You may cast this card from your graveyard by discarding a card in addition to paying its other costs. Then exile this card.) */

  target_definition_t td;
  if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
  {
    return jumpstart(player, card, event, 0x4b89);
  }
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_ANY", 1, NULL);
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
  }
  else
  {
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 2);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      if (event == EVENT_RESOLVE_SPELL)
      {
        if (valid_target(&td))
          damage_target0(player, card, 2);
        kill_card(player, card, KILL_DESTROY);
      }
      return generic_spell(player, card, event, 1, &td, "TARGET_ANY", 1, NULL);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  default_target_definition(player, card, &td, 2);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  return generic_spell(player, card, event, 1, &td, "TARGET_ANY", 1, NULL);
}

/* ManalinkEh 2022: 0x261b800; original cards/guilds_of_ravnica.c:991. */
int card_discovery_dispersal(int player, int card, event_t event)
{
  /* Discovery // Dispersal	|1|UB // |3|U|B
   * Sorcery // Instant
   * Discovery:
   * Surveil 2, then draw a card. (To surveil 2, look at the top two cards of your library, then put any number of them into your graveyard and the rest on top of your library in any order.)
   *
   * Dispersal:
   * Each opponent returns a nonland permanent they control with the greatest mana value among permanents they control to its owner's hand, then discards a card. */

  card_instance_t *instance;
  unsigned int selected_halves;
  card_instance_t *candidate;
  int cmc_or_target_card;
  int scan_or_target_player;
  int opponent;
  int highest_ties;
  int highest_cmc;
  target_definition_t td;
  char msg[100];

  if (event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET)
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      selected_halves = instance->info_slot;
      if ((selected_halves & 1) != 0)
      {
        surveil(player, 2);
        draw_a_card(player);
        selected_halves = instance->info_slot;
      }
      if ((selected_halves & 2) != 0)
      {

        opponent = 1 - player;
        highest_ties = 1;
        instance->targets[0].card = -1;
        instance->targets[0].player = -1;
        highest_cmc = -1;
        scan_or_target_player = active_cards_count[opponent] + -1;
        if (-1 < scan_or_target_player)
        {
          for (; scan_or_target_player >= 0; --scan_or_target_player)
          {
            candidate = in_play(opponent, scan_or_target_player);
            if (!candidate || !is_what(opponent, scan_or_target_player, 2))
              continue;
            cmc_or_target_card = get_cmc(opponent, scan_or_target_player);
            if (cmc_or_target_card <= highest_cmc)
            {
              cmc_or_target_card = get_cmc(opponent, scan_or_target_player);
              highest_ties += cmc_or_target_card == highest_cmc;
            }
            else
            {
              instance->targets[0].card = scan_or_target_player;
              instance->targets[0].player = opponent;
              highest_cmc = get_cmc(opponent, scan_or_target_player);
              highest_ties = 1;
            }
          }
          if (1 < highest_ties)
          {
            default_target_definition(player, card, &td, 2);
            td.illegal_abilities = 0;
            td.allow_cancel = 0;
            td.extra = highest_cmc;
            td.special = TARGET_SPECIAL_CMC_GREATER_OR_EQUAL;
            td.who_chooses = opponent;
            td.allowed_controller = opponent;
            td.preferred_controller = opponent;
            scnprintf(msg, 100, "Select a creature you control with CMC %d.", highest_cmc);
            instance->number_of_targets = '\0';
            new_pick_target(&td, msg, 0, 0x80);
          }
          scan_or_target_player = instance->targets[0].player;
          if ((scan_or_target_player != -1) &&
              (cmc_or_target_card = instance->targets[0].card, cmc_or_target_card != -1))
          {
            bounce_permanent(scan_or_target_player, cmc_or_target_card);
          }
        }
        discard(opponent, 0, player);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  else if (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET ||
           (event == EVENT_CAST_SPELL && card == affected_card &&
            player == affected_card_controller))
    get_card_instance(player, card);
  return hybrid_split_card(player, card, event, 0x4b8a, can_sorcery_be_played(player, event), 6,
                           0x4b8d, 1, 10);
}

/* ManalinkEh 2022: 0x261bb10; original cards/guilds_of_ravnica.c:1058. */
int card_disinformation_campaign(int player, int card, event_t event)
{
  /* Disinformation Campaign	|1|U|B
   * Enchantment
   * When this enchantment enters, you draw a card and each opponent discards a card.
   * Whenever you surveil, return this enchantment to its owner's hand. */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_a_card(player);
    discard(1 - player, 2, player);
  }
  else if (event == EVENT_SURVEIL)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      bounce_permanent(player, card);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x261bbb0; original cards/guilds_of_ravnica.c:1098. */
int card_doom_whisperer(int player, int card, event_t event)
{
  /* Doom Whisperer	|3|B|B
   * Creature - Nightmare Demon 6/6
   * Flying, trample
   * Pay 2 life: Surveil 2. (Look at the top two cards of your library, then put any number of them into your graveyard and the rest on top of your library in any order.) */

  int result;

  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    surveil(player, 2);
  }
  result = generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 0, 2, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x261bc60; original cards/guilds_of_ravnica.c:1137. */
int card_electrostatic_field(int player, int card, event_t event)
{
  /* Electrostatic Field	|1|R
   * Creature - Wall 0/4
   * Defender
   * Whenever you cast an instant or sorcery spell, this creature deals 1 damage to each opponent. */

  sp_ability_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  damage_player(1 - player, 1, player, card);
  return 0;
}

/* ManalinkEh 2022: 0x261bce0; original cards/guilds_of_ravnica.c:1160. */
int card_emmara_soul_of_the_accord(int player, int card, event_t event)
{
  /* Emmara, Soul of the Accord	|G|W
   * Legendary Creature - Elf Cleric 2/2
   * Whenever Emmara becomes tapped, create a 1/1 white Soldier creature token with lifelink. */

  token_generation_t token;

  check_legend_rule(player, card, event);
  if (((event == EVENT_TAP_CARD) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      default_token_definition(player, card, CARD_ID_SOLDIER, &token);
      token.s_key_plus = 2;
      generate_token(&token);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x261bd70; original cards/guilds_of_ravnica.c:1215. */
int card_expansion_explosion(int player, int card, event_t event)
{
  /* Expansion // Explosion	|UR|UR // |X|U|U|R|R
   * Instant // Instant
   * Expansion:
   * Copy target instant or sorcery spell with mana value 4 or less. You may choose new targets for the copy.
   *
   * Explosion:
   * Explosion deals X damage to any target. Target player draws X cards. */

  card_instance_t *instance;
  unsigned int selected_halves;
  int result;
  int can_play_first_half;
  target_definition_t td_spell;
  target_definition_t td_any;
  target_definition_t td_player;

  counterspell_target_definition(player, card, &td_spell, 0x18);
  td_spell.special = td_spell.special | 0x800;
  td_spell.extra = 4;
  default_target_definition(player, card, &td_any, 0x102);
  td_any.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  default_target_definition(player, card, &td_player, 0);
  td_player.zone = TARGET_ZONE_PLAYERS;
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    return twincast(player, card, 0x6c, &td_spell, NULL);
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    new_pick_target(&td_any, "Select target for damage", 0, 0x81);
    new_pick_target(&td_player, "Select target player to draw cards", 1, 0x81);
    return generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 0, NULL);
  }
  if (event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET)
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      selected_halves = instance->info_slot;
      if ((selected_halves & 1) != 0)
      {
        twincast(player, card, 0x71, &td_spell, NULL);
        selected_halves = instance->info_slot;
      }
      if (((selected_halves & 2) != 0) &&
          ((result = validate_target(player, card, &td_any, 0),
            result != 0 || (result = validate_target(player, card, &td_player, 1), result != 0))))
      {
        damage_target0(player, card, x_value);
        draw_cards(instance->targets[1].player, x_value);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  else if (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET ||
           (event == EVENT_CAST_SPELL && card == affected_card &&
            player == affected_card_controller))
    get_card_instance(player, card);
  result = can_target(&td_any);
  can_play_first_half = counterspell(player, card, EVENT_CAN_CAST, &td_spell, 0);
  return hybrid_split_card(player, card, event, 0x4b9b, can_play_first_half, 5, 0x4b9e, result, 8);
}

/* ManalinkEh 2022: 0x261c0e0; original cards/guilds_of_ravnica.c:1267. */
int card_experimental_frenzy(int player, int card, event_t event)
{
  /* Experimental Frenzy	|3|R
   * Enchantment
   * You may look at the top card of your library any time.
   * You may play lands and cast spells from the top of your library.
   * You can't play lands or cast spells from your hand.
   * |3|R: Destroy this enchantment. */

  enum
  {
    CHOICE_REVEAL = 1,
    CHOICE_PLAY = 2,
    CHOICE_DESTROY = 3
  };

  int choice;
  int iid_or_destroy_available;
  int cast_available;
  card_instance_t *instance;
  uint32_t destroy_priority;
  unsigned int play_available;
  uint32_t inspect_available;

  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if (((affected_card_controller == player) &&
         (instance = in_hand(player, affected_card), instance != NULL)) &&
        (iid_or_destroy_available = check_state(affected_card_controller, affected_card, 0x20),
         iid_or_destroy_available == 0))
    {
      infinite_casting_cost();
    }
    return global_enchantment(player, card, event);
  }
  if (event == EVENT_CAN_ACTIVATE)
  {

    bool check_destroy_cost = true;
    iid_or_destroy_available = *deck_ptr[player];
    if (iid_or_destroy_available != -1)
    {
      cast_available = has_mana_to_cast_iid(player, EVENT_CAN_ACTIVATE, iid_or_destroy_available);
      if (cast_available != 0 &&
          (iid_or_destroy_available = can_legally_play_iid_now(player, iid_or_destroy_available,
                                                               EVENT_CAN_ACTIVATE)) != 0 &&
          ai_is_speculating == 0)
        return iid_or_destroy_available;
      check_destroy_cost = *deck_ptr[player] == -1;
    }
    if (check_destroy_cost && !generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 3, 0,
                                                         0, 0, 0, 0, 0, NULL, NULL))
      return 0;
    instance = has_activation_on_stack(player, card);
    return (unsigned int)(instance == NULL);
  }
  if (event != EVENT_ACTIVATE)
  {
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      if (instance->info_slot != 3)
      {
        return 0;
      }
      instance = get_card_instance(player, card);
      iid_or_destroy_available = instance->parent_card;
      instance = get_card_instance(player, card);
      kill_card(instance->parent_controller, iid_or_destroy_available, KILL_DESTROY);
      return 0;
    }
    if (event == EVENT_CLEANUP)
    {
      return 0;
    }
    return global_enchantment(player, card, event);
  }

  iid_or_destroy_available = *deck_ptr[player];
  inspect_available = ai_is_speculating != 1 && (player != 1 || ((unsigned char)trace_mode & 2));
  if (iid_or_destroy_available == -1)
    inspect_available = 0;
  play_available = 0;
  if (iid_or_destroy_available != -1 &&
      can_legally_play_iid_now(player, iid_or_destroy_available, EVENT_ACTIVATE) &&
      has_mana_to_cast_iid(player, EVENT_ACTIVATE, *deck_ptr[player]))
  {
    instance = has_activation_on_stack(player, card);
    play_available = instance == NULL;
  }
  iid_or_destroy_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 3, 0, 0,
                                                       0, 1, 0, 0, NULL, NULL);
  destroy_priority = play_available ? 0xfffffff6 : 5;
  choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                       "Check top card of deck", inspect_available, 0, "Play top card",
                       play_available, 10, "Destroy Experimental Frenzy", iid_or_destroy_available,
                       destroy_priority, 0, 0, 0, 0);
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
                            (unsigned int)cards_data[*deck_ptr[player]].id) != 0)
    {
      play_card_in_deck_for_free(player, player, 0);
      cant_be_responded_to = 1;
    }
  }
  else if (choice == CHOICE_DESTROY)
  {
    return generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 3, 0, 0, 0, 1, 0, 0, NULL,
                                     NULL);
  }
  return 0;
}

/* ManalinkEh 2022: 0x261c660; original cards/guilds_of_ravnica.c:1358. */
int card_find_finality(int player, int card, event_t event)
{
  /* Find // Finality	|BG|BG // |4|B|G
   * Sorcery // Sorcery
   * Find:
   * Return up to two target creature cards from your graveyard to your hand.
   *
   * Finality:
   * You may put two +1/+1 counters on a creature you control. Then all creatures get -4/-4 until end of turn. */

  card_instance_t *instance;
  unsigned int selected_halves;
  int result;
  target_definition_t td;
  test_definition_t test;

  new_default_test_definition(&test, 2, "Select up to two target creature cards.");
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = player;
  td.illegal_abilities = 0;
  td.allow_cancel = 2;
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    return spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(
        player, card, EVENT_CAST_SPELL, 2, &test, 0);
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    new_pick_target(&td, "SELECT_A_CREATURE_YOU_CONTROL", 0, 0);
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
        spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(
            player, card, EVENT_RESOLVE_SPELL, 2, &test, 0);
        selected_halves = instance->info_slot;
      }
      if ((selected_halves & 2) != 0)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                       COUNTER_P1_P1, 2);
        }

        APNAP(p, {
          pump_subtype_until_eot(player, card, p, -1, -4, -4, 0, 0);
        });
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  else if (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET ||
           (event == EVENT_CAST_SPELL && card == affected_card &&
            player == affected_card_controller))
    get_card_instance(player, card);
  result = spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(
      player, card, EVENT_CAN_CAST, 2, &test, 0);
  return hybrid_split_card(player, card, event, 0x4ba1, result, 10, 0x4ba0, 1, 8);
}

/* ManalinkEh 2022: 0x261c980; original cards/guilds_of_ravnica.c:1414. */
int card_fireminds_research(int player, int card, event_t event)
{
  /* Firemind's Research	|U|R
   * Enchantment
   * Whenever you cast an instant or sorcery spell, put a charge counter on this enchantment.
   * |1|U, Remove two charge counters from this enchantment: Draw a card.
   * |1|R, Remove five charge counters from this enchantment: It deals 5 damage to any target. */

  enum
  {
    CHOICE_DRAW = 1,
    CHOICE_DAMAGE = 2
  };

  int choice;
  card_instance_t *instance;
  int available_or_mode;
  target_definition_t *damage_target;
  target_definition_t td;

  sp_ability_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 1, 0, 0, 0,
                                      0x17b0200, NULL, NULL) == 0)
        {
          return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 0, 0, 0, 1, 0,
                                           0x17b0500, &td, "TARGET_ANY");
        }
        return 1;
      }
      if (event == EVENT_ACTIVATE)
      {

        available_or_mode = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 1,
                                                      0, 0, 0, 0x17b0200, NULL, NULL);
        choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Draw a card",
                             available_or_mode, 5, "Deal 5 damage",
                             generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 0, 0,
                                                       0, 1, 0, 0x17b0500, &td, "TARGET_ANY"),
                             10, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice != CHOICE_DRAW)
        {
          if (choice != CHOICE_DAMAGE)
          {
            return 0;
          }
          return generic_activated_ability(player, card, EVENT_ACTIVATE, 4, 1, 0, 0, 0, 1, 0,
                                           0x17b0500, &td, "TARGET_ANY");
        }
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 1, 0, 1, 0, 0, 0,
                                         0x17b0200, NULL, NULL);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        return global_enchantment(player, card, event);
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      instance = get_card_instance(player, card);
    }
    damage_target = &td;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      available_or_mode = instance->info_slot;
      if (available_or_mode == CHOICE_DRAW)
      {
        draw_a_card(player);
        available_or_mode = instance->info_slot;
      }
      if ((available_or_mode == CHOICE_DAMAGE) &&
          (available_or_mode = valid_target(damage_target), available_or_mode != 0))
      {
        damage_target0(player, card, 5);
      }
    }
    return 0;
  }
  add_counters(player, card, player, card, COUNTER_CHARGE, 1);
  return global_enchantment(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2619160; original cards/guilds_of_ravnica.c:1502. */
static int is_basic_land_by_internal_id(int iid, int unused, int unused1, int unused2)
{
  int result;

  result = is_basic_land_by_id((unsigned int)cards_data[iid].id);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x261ce20; original cards/guilds_of_ravnica.c:1505. */
int card_flower_flourish(int player, int card, event_t event)
{
  /* Flower // Flourish	|GW // |4|G|W
   * Sorcery // Sorcery
   * Flower:
   * Search your library for a basic Forest or Plains card, reveal it, put it into your hand, then shuffle.
   *
   * Flourish:
   * Creatures you control get +2/+2 until end of turn. */

  const char *msg;
  card_instance_t *instance;
  unsigned int selected_halves;
  test_definition_t this_test;

  msg = get_hacked_land_text(player, card, "Select a basic %s or %s card.", 0x5001, 0x5007);
  new_default_test_definition(&this_test, 1, msg);
  this_test.subtype = get_hacked_subtype(player, card, 0x5001);
  this_test.sub2 = get_hacked_subtype(player, card, 0x5007);
  this_test.subtype_flag = 3;
  this_test.special_selection_function = is_basic_land_by_internal_id;
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
        return hybrid_split_card(player, card, event, 0x4ba7, 1, 10, 0x4ba6, 1, 8);
    }
    else
    {
      if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
      {
        instance = get_card_instance(player, card);
        if (event == EVENT_RESOLVE_SPELL)
        {
          selected_halves = instance->info_slot;
          if ((selected_halves & 1) != 0)
          {
            new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
            selected_halves = instance->info_slot;
          }
          if ((selected_halves & 2) != 0)
          {
            pump_subtype_until_eot(player, card, player, -1, 2, 2, 0, 0);
          }
          kill_card(player, card, KILL_DESTROY);
        }
        return hybrid_split_card(player, card, event, 0x4ba7, 1, 10, 0x4ba6, 1, 8);
      }
      if (event != EVENT_CHANGE_TARGET)
        return hybrid_split_card(player, card, event, 0x4ba7, 1, 10, 0x4ba6, 1, 8);
    }
  }
  get_card_instance(player, card);
  return hybrid_split_card(player, card, event, 0x4ba7, 1, 10, 0x4ba6, 1, 8);
}
#endif

/* ManalinkEh 2022: 0x261d010; original cards/guilds_of_ravnica.c:1594. */
int card_glowspore_shaman(int player, int card, event_t event)
{
  /* Glowspore Shaman	|B|G
   * Creature - Elf Shaman 3/1
   * When this creature enters, mill three cards. You may put a land card from your graveyard on top of your library. (To mill a card, put the top card of your library into your graveyard.) */

  int result;
  test_definition_t this_test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    mill(player, 3);
    new_default_test_definition(&this_test, 1, "Select a land card.");
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

/* ManalinkEh 2022: 0x261d0d0; original cards/guilds_of_ravnica.c:1623. */
int card_goblin_cratermaker(int player, int card, event_t event)
{
  /* Goblin Cratermaker	|1|R
   * Creature - Goblin Warrior 2/2
   * |1, Sacrifice this creature: Choose one -
   * * This creature deals 2 damage to target creature.
   * * Destroy target colorless nonland permanent. */

  enum
  {
    CHOICE_DAMAGE = 1,
    CHOICE_DESTROY = 2
  };

  int choice;
  card_instance_t *instance;
  int available_or_flags;
  target_definition_t *td;
  const char *prompt;
  target_definition_t td1;
  target_definition_t td2;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    td = &td1;
    default_target_definition(player, card, td, 2);
    default_target_definition(player, card, &td2, 0x47);
    td2.illegal_type = TYPE_LAND;
    td2.illegal_color = 0x3e;
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xc, 1, 0, 0, 0, 0, 0, 0, td,
                                    NULL) == 0)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xc, 1, 0, 0, 0, 0, 0, 0,
                                         &td2, NULL);
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      available_or_flags = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xc, 1, 0, 0,
                                                     0, 0, 0, 0, &td2, NULL);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "2 damage to target creature",
                           generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xc, 1, 0, 0,
                                                     0, 0, 0, 0, td, NULL),
                           5, "Destroy a colorless nonland permanent", available_or_flags, 5, 0, 0,
                           0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_DAMAGE)
      {
        prompt = "TARGET_CREATURE";
        available_or_flags = 0xc;
      }
      else
      {
        if (choice != CHOICE_DESTROY)
        {
          return 0;
        }
        prompt = "Select target colorless nonland permanent.";
        available_or_flags = 0xe;
        td = &td2;
      }
      return generic_activated_ability(player, card, EVENT_ACTIVATE, available_or_flags, 1, 0, 0, 0,
                                       0, 0, 0, td, prompt);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td1, 2);
    default_target_definition(player, card, &td2, 0x47);
  }
  td2.illegal_color = 0x3e;
  td2.illegal_type = TYPE_LAND;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    if (instance->info_slot == CHOICE_DAMAGE)
    {
      if (valid_target(&td1) != 0)
      {
        damage_target0(player, card, 2);
      }
    }
    else if ((instance->info_slot == CHOICE_DESTROY) &&
             (available_or_flags = valid_target(&td2), available_or_flags != 0))
    {
      kill_target0(player, card, KILL_DESTROY);
      return 0;
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x261d540; original cards/guilds_of_ravnica.c:1690. */
int card_golgari_findbroker(int player, int card, event_t event)
{
  /* Golgari Findbroker	|B|B|G|G
   * Creature - Elf Shaman 3/4
   * When this creature enters, return target permanent card from your graveyard to your hand. */

  int result;
  test_definition_t this_test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&this_test, 0x47, "Select target permanent card.");
    result = new_special_count_grave(player, &this_test);
    if (result != 0)
    {
      result = graveyard_has_shroud(player);
      if (result == 0)
      {
        new_global_tutor(player, player, 2, 1, 1, 1, &this_test);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x261d5f0; original cards/guilds_of_ravnica.c:1749. */
int card_guild_summit(int player, int card, event_t event)
{
  /* Guild Summit	|2|U
   * Enchantment
   * When this enchantment enters, you may tap any number of untapped Gates you control. Draw a card for each Gate tapped this way.
   * Whenever a Gate you control enters, draw a card. */

  int result;
  card_instance_t *instance;


  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if ((event == EVENT_ETB) && (trigger_cause_controller == player) &&
      (trigger_cause == card))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x47);
    td.required_subtype = 0x500c;
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.illegal_abilities = 0;
    td.illegal_state = TARGET_STATE_TAPPED;
    td.allow_cancel = 3;
    int count = 0;
    while ((result = can_target(&td),
            result != 0 && (result = new_pick_target(&td,
                                                     "Choose an untapped Gate to tap", 0, 0x80),
                            result != 0)))
    {
      instance = get_card_instance(player, card);
      result = instance->targets[0].card;
      instance = get_card_instance(player, card);
      tap_card(instance->targets[0].player, result);
      count = count + 1;
    }
    if (count != 0)
    {
      draw_cards(player, count);
    }
  }
  if ((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card) &&
      (player == affected_card_controller))
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 0x47);
    this_test.subtype = SUBTYPE_GATE;
    etb_ability_global(player, event, 2, TYPE_NONE, &this_test, player, card);
  }
  if ((event == EVENT_ETB) && (trigger_cause != card))
  {
    draw_cards(player, 1);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x261d840; original cards/guilds_of_ravnica.c:1856. */
int card_hunted_witness(int player, int card, event_t event)
{
  /* Hunted Witness	|W
   * Creature - Human 1/1
   * When this creature dies, create a 1/1 white Soldier creature token with lifelink. */

  int result;
  token_generation_t token;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  default_token_definition(player, card, CARD_ID_SOLDIER, &token);
  token.s_key_plus = 2;
  generate_token(&token);
  return 0;
}

/* ManalinkEh 2022: 0x261d8b0; original cards/guilds_of_ravnica.c:1880. */
int card_impervious_greatwurm(int player, int card, event_t event)
{
  /* Impervious Greatwurm	|7|G|G|G
   * Creature - Wurm 16/16
   * Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
   * Indestructible */

  indestructible(player, card, event);
  return generic_spell_with_convoke(player, card, event);
}

/* ManalinkEh 2022: 0x261d8f0; original cards/guilds_of_ravnica.c:1900. */
int card_inspiring_unicorn(int player, int card, event_t event)
{
  /* Inspiring Unicorn	|2|W|W
   * Creature - Unicorn 2/2
   * Whenever this creature attacks, creatures you control get +1/+1 until end of turn. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }
  pump_creatures_until_eot(player, card, player, 0, 1, 1, 0, 0, NULL);
  return 0;
}

/* ManalinkEh 2022: 0x261d990; original cards/guilds_of_ravnica.c:1961. */
int card_invert_invent(int player, int card, event_t event)
{
  /* Invert // Invent	|UR // |4|U|R
   * Instant // Instant
   * Invert:
   * Switch the power and toughness of each of up to two target creatures until end of turn.
   *
   * Invent:
   * Search your library for an instant card and/or a sorcery card, reveal them, put them into your hand, then shuffle. */

  int i;
  card_instance_t *instance;
  unsigned int selected_halves;
  card_instance_t *target_record;
  target_definition_t td;
  test_definition_t test_instant;
  test_definition_t test_sorcery;

  default_target_definition(player, card, &td, 2);
  td.allow_cancel = 3;
  new_default_test_definition(&test_instant, 0x10, "Select an instant card.");
  test_instant.no_shuffle = 1;
  new_default_test_definition(&test_sorcery, 8, "Select a sorcery card.");
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    return generic_spell(player, card, EVENT_CAST_SPELL, 0x20, &td, "TARGET_CREATURE", 2, NULL);
  }
  if (event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET)
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      selected_halves = instance->info_slot;
      if ((selected_halves & 1) != 0)
      {

        target_record = get_card_instance(player, card);
        if (target_record->number_of_targets != '\0')
        {
          i = 0;
          do
          {
            if (validate_target(player, card, &td, i) != 0)
            {
              switch_power_and_toughness_until_eot(player, card, instance->targets[i].player,
                                                   instance->targets[i].card);
            }
            i = i + 1;
          } while (i < (int)(unsigned int)target_record->number_of_targets);
        }
        selected_halves = instance->info_slot;
      }
      if ((selected_halves & 2) != 0)
      {
        new_global_tutor(player, player, 1, 1, 0, 1, &test_instant);
        new_global_tutor(player, player, 1, 1, 0, 1, &test_sorcery);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  else if (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET ||
           (event == EVENT_CAST_SPELL && card == affected_card &&
            player == affected_card_controller))
    get_card_instance(player, card);
  return hybrid_split_card(player, card, event, 0x4bcd, can_target(&td), 8, 0x4bcc, 1, 10);
}

/* ManalinkEh 2022: 0x261dc70; original cards/guilds_of_ravnica.c:2009. */
int card_ionize(int player, int card, event_t event)
{
  /* Ionize	|1|U|R
   * Instant
   * Counter target spell. ~ deals 2 damage to that spell's controller. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  counterspell_target_definition(player, card, &td, 0xff);
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {

      instance = get_card_instance(player, card);
      result = counterspell_validate(player, card, &td, 0);
      if (result != 0)
      {
        real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
      }
      damage_player(instance->targets[0].player, 2, player, card);
      instance->number_of_targets = '\0';
      kill_card(player, card, KILL_DESTROY);
    }
    result = counterspell(player, card, event, NULL, 0);
    return result;
  }
  td.illegal_abilities = 0;
  return counterspell(player, card, EVENT_CAN_CAST, NULL, 0);
}

/* ManalinkEh 2022: 0x261ddb0; original cards/guilds_of_ravnica.c:2037. */
int card_izoni_thousand_eyed(int player, int card, event_t event)
{
  /* Izoni, Thousand-Eyed	|2|B|B|G|G
   * Legendary Creature - Elf Shaman 2/3
   * Undergrowth - When Izoni enters, create a 1/1 black and green Insect creature token for each creature card in your graveyard.
   * |B|G, Sacrifice another creature: You gain 1 life and draw a card. */

  int result;
  token_generation_t token;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_token_definition(player, card, CARD_ID_INSECT, &token);
    token.color_forced = '\n';
    result = count_graveyard_by_type(player, 2);
    token.qty = (unsigned char)result;
    generate_token(&token);
    return 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event != EVENT_RESOLVE_ACTIVATION)
    return generic_activated_ability(player, card, event, 0x40200, 0, 1, 0, 1, 0, 0, 0, NULL, NULL);
  gain_life(player, 1);
  draw_a_card(player);
  return generic_activated_ability(player, card, event, 0x40200, 0, 1, 0, 1, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x261dee0; original cards/guilds_of_ravnica.c:2086. */
int card_knight_of_autumn(int player, int card, event_t event)
{
  /* Knight of Autumn	|1|G|W
   * Creature - Dryad Knight 2/1
   * When this creature enters, choose one -
   * * Put two +1/+1 counters on this creature.
   * * Destroy target artifact or enchantment.
   * * You gain 4 life. */

  int result;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 0x44);
    td.allow_cancel = 0;
    result = can_target(&td);
    result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Add two +1/+1 counters", 1, 5,
                         "Destroy an artifact or enchantment", result, 8, "Gain 4 life", 1, 3, 0, 0,
                         0, 0);
    if (result == 2)
    {
      result = can_target(&td);
      if (result != 0)
      {
        result = pick_target(&td, "DISENCHANT");
        if (result != 0)
        {
          action_on_target(player, card, 0, ACT_KILL_DESTROY);
        }
      }
    }
    else if (result == 3)
    {
      gain_life(player, 4);
    }
    else if (result == 1)
    {
      add_counters(player, card, player, card, COUNTER_P1_P1, 2);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x261e080; original cards/guilds_of_ravnica.c:2130. */
int card_kraul_harpooner(int player, int card, event_t event)
{
  /* Kraul Harpooner	|1|G
   * Creature - Insect Warrior 3/2
   * Reach
   * Undergrowth - When this creature enters, choose up to one target creature you don't control with flying. This creature gets +X/+0 until end of turn, where X is the number of creature cards in your graveyard, then you may have this creature fight that creature. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    td.required_abilities = 0x20;
    td.allow_cancel = 2;
    result = count_graveyard_by_type(player, 2);
    pump_until_eot(player, card, player, card, result, 0);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(
          &td, "Choose up to one target creature with flying you don\'t control.", 0, 0x80);
      if (result != 0)
      {
        instance = get_card_instance(player, card);
        result = instance->targets[0].card;
        instance = get_card_instance(player, card);
        fight(player, card, instance->targets[0].player, result);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x261e190; original cards/guilds_of_ravnica.c:2167. */
int card_lava_coil(int player, int card, event_t event)
{
  /* Lava Coil	|1|R
   * Sorcery
   * ~ deals 4 damage to target creature. If that creature would die this turn, exile it instead. */

  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      card_instance_t *instance = get_card_instance(player, card);
      exile_if_would_be_put_into_graveyard(player, card, instance->targets[0].player,
                                           instance->targets[0].card, 1);
      damage_target0(player, card, 4);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x2619110; original cards/guilds_of_ravnica.c:2237. */
static void haste_and_must_attack_until_eot(token_generation_t *token, int beast, int number)
{
  pump_ability_until_eot(token->s_player, token->s_card, token->t_player, beast, 0, 0, 0, 0x404);
  return;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x261e2f0; original cards/guilds_of_ravnica.c:2241. */
int card_legion_warboss(int player, int card, event_t event)
{
  /* Legion Warboss	|2|R
   * Creature - Goblin Soldier 2/2
   * Mentor (Whenever this creature attacks, put a +1/+1 counter on target attacking creature with lesser power.)
   * At the beginning of combat on your turn, create a 1/1 red Goblin creature token. That token gains haste until end of turn and attacks this combat if able. */

  int result;
  token_generation_t token;
  mentor(player, card, event);
  result = beginning_of_combat(player, card, event, player);
  if (result == 0)
  {
    return 0;
  }
  default_token_definition(player, card, CARD_ID_GOBLIN, &token);
  result = engine_any_legal_attacker(player);
  if (result == 0)
  {
    token.action = TOKEN_ACTION_ATTACKING;
  }
  token.special_code_on_generation = haste_and_must_attack_until_eot;
  generate_token(&token);
  return 0;
}
#endif

/* ManalinkEh 2022: 0x261e380; original cards/guilds_of_ravnica.c:2289. */
int card_march_of_the_multitudes(int player, int card, event_t event)
{
  /* March of the Multitudes	|X|G|W|W
   * Instant
   * Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
   * Create X 1/1 white Soldier creature tokens with lifelink. */

  card_ptr_t *card_data;
  card_instance_t *instance;
  int result;
  token_generation_t token;

  if (event == EVENT_MODIFY_COST)
  {
    result = get_id(player, card);
    card_data = cards_ptr[result];
    result = has_convoked_mana_extended(
        player, card, EVENT_MODIFY_COST, (unsigned int)card_data->req.req_colorless,
        (unsigned int)card_data->req.req_black, (unsigned int)card_data->req.req_blue,
        (unsigned int)card_data->req.req_green, (unsigned int)card_data->req.req_red,
        (unsigned int)card_data->req.req_white);
    if (result == 2)
    {
      null_casting_cost(player, card);
      instance = get_card_instance(player, card);
      instance->targets[1].player = 1;
    }
    else
    {
      instance = get_card_instance(player, card);
      instance->targets[1].player = 0;
    }
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      get_card_instance(player, card);
      return generic_spell(player, card, EVENT_CAN_CAST, 4, NULL, NULL, 0, NULL);
    }
    if (event == EVENT_CAST_SPELL)
    {
      if ((((card == affected_card) && (player == affected_card_controller)) &&
           (instance = get_card_instance(player, card), card == affected_card)) &&
          (player == affected_card_controller))
      {
        result = check_special_flags2(player, card, -0x80000000);
        if (result == 0)
        {
          result = played_for_free(player, card);
          if ((result == 0) && (result = is_token(player, card), result == 0))
          {
            if (instance->targets[1].player == 1)
            {
              result =
                  charge_convoked_mana_extended(player, card, EVENT_CAST_SPELL, 0, 0, 0, 1, 0, 2);
              if (result != 0)
              {
                x_value = charge_convoked_mana_extended_generic(player, card, EVENT_CAST_SPELL, -1,
                                                                0, 0, 0, 0, 0, 0);
                if (x_value < 0)
                {
                  x_value = x_value & 0x7fffffff;
                }
                instance->info_slot = x_value;
              }
            }
            else
            {
              instance->info_slot = x_value;
            }
          }
          else
          {
            instance->info_slot = 0;
          }
        }
        else
        {
          instance->info_slot = 0;
        }
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {
        x_value = 0;
        default_token_definition(player, card, CARD_ID_SOLDIER, &token);
        token.s_key_plus = 2;
        token.qty = (unsigned char)instance->info_slot;
        generate_token(&token);
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
    }
    else if (event == EVENT_CHANGE_TARGET)
    {
      get_card_instance(player, card);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x261e6e0; original cards/guilds_of_ravnica.c:2355. */
int card_mausoleum_secrets(int player, int card, event_t event)
{
  /* Mausoleum Secrets	|1|B
   * Instant
   * Undergrowth - Search your library for a black card with mana value less than or equal to the number of creature cards in your graveyard, reveal it, put it into your hand, then shuffle. */

  if (event == EVENT_CAST_SPELL)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      int amount = count_graveyard_by_type(player, TYPE_CREATURE);
      ai_modifier += amount * 0xc;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    int amount = count_graveyard_by_type(player, TYPE_CREATURE);
    test_definition_t test;
    default_test_definition(&test, 0xff);
    test.color = get_sleighted_color_test(player, card, 2);
    const char *color_name = get_sleighted_color_text(player, card, "%s", 1);
    scnprintf(test.message, 100, "Select a %s card with CMC %d or less.", color_name, amount);
    test.cmc_flag = 3;
    test.cmc = amount + 1;
    new_global_tutor(player, player, 1, 1, 0, 1, &test);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x261e850; original cards/guilds_of_ravnica.c:2380. */
int card_maximize_altitude(int player, int card, event_t event)
{
  /* Maximize Altitude	|U
   * Sorcery
   * Target creature gets +1/+1 and gains flying until end of turn.
   * Jump-start (You may cast this card from your graveyard by discarding a card in addition to paying its other costs. Then exile this card.) */

  int result;
  if ((event != EVENT_GRAVEYARD_ABILITY) && (event != EVENT_PAY_FLASHBACK_COSTS))
  {
    result = vanilla_instant_pump(player, card, event, 2, player, 1, 1, 0x20, 0);
    return result;
  }
  result = jumpstart(player, card, event, 0x4be6);
  return result;
}

/* ManalinkEh 2022: 0x261e8d0; original cards/guilds_of_ravnica.c:2393. */
int card_maximize_velocity(int player, int card, event_t event)
{
  /* Maximize Velocity	|R
   * Sorcery
   * Target creature gets +1/+1 and gains haste until end of turn.
   * Jump-start (You may cast this card from your graveyard by discarding a card in addition to paying its other costs. Then exile this card.) */

  int result;
  if ((event != EVENT_GRAVEYARD_ABILITY) && (event != EVENT_PAY_FLASHBACK_COSTS))
  {
    result = vanilla_instant_pump(player, card, event, 2, player, 1, 1, 0, 4);
    return result;
  }
  result = jumpstart(player, card, event, 0x4be7);
  return result;
}

/* ManalinkEh 2022: 0x2619180; original cards/guilds_of_ravnica.c:2414. */
static int midnight_reaper_effect(int player, int card, event_t event)
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
          (get_id((int)instance->damage_target_player, instance->damage_target_card) != 0x4be9))
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

      if ((dying != NULL) && (dying = get_card_instance(affected_card_controller, affected_card),
                              dying->kill_code != '\0'))
      {

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
        count_for_gfp_ability(player, card, 0x77, player, 0, &test);
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
        damage_player(player, 1, player, card);
      }
      draw_cards(player, 0);
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) && (check_status(player, card, 0x10000) == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x261e950; original cards/guilds_of_ravnica.c:2471. */
int card_midnight_reaper(int player, int card, event_t event)
{
  /* Midnight Reaper	|2|B
   * Creature - Zombie Knight 3/2
   * Whenever a nontoken creature you control dies, this creature deals 1 damage to you and you draw a card. */

  int c;
  card_instance_t *instance;
  int created_legacy;
  unsigned int pending_deaths;
  test_definition_t test;

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
                instance->info_slot_fn == midnight_reaper_effect)) &&
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
            create_targetted_legacy_effect(player, card, midnight_reaper_effect, player, card);
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
    test.type_flag = 3;
    count_for_gfp_ability(player, card, 0x77, player, 0, &test);
  }
  instance = get_card_instance(player, card);

  if ((instance->kill_code == '\0') &&
      (pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY),
       pending_deaths != 0))
  {

    damage_player(player, pending_deaths & 0xff, player, card);
    draw_cards(player, pending_deaths & 0xff);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x261eb60; original cards/guilds_of_ravnica.c:2525. */
int card_mission_briefing(int player, int card, event_t event)
{
  /* Mission Briefing	|U|U
   * Instant
   * Surveil 2, then choose an instant or sorcery card in your graveyard. You may cast it this turn. If that spell would be put into your graveyard, exile it instead. (To surveil 2, look at the top two cards of your library, then put any number of them into your graveyard and the rest on top of your library in any order.) */

  int result;
  test_definition_t test;

  if (event == EVENT_RESOLVE_SPELL)
  {
    surveil(player, 2);
    result = count_graveyard_by_type(player, 0x38);
    if (result != 0)
    {

      new_default_test_definition(&test, 0x38, "Select target instant or sorcery card.");
      result = new_select_a_card(player, player, 2, 1, 1, -1, &test);
      if (-1 < result)
      {
        create_spell_has_flashback_legacy(player, card, result, 0);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2619800; original cards/guilds_of_ravnica.c:2547. */
static int mnemonic_betrayal_legacy(int player, int card, event_t event)
{
  int effect_card;
  card_instance_t *instance;
  int type_or_exile_index;

  if (eot_trigger(player, card, event) != 0)
  {

    effect_card = active_cards_count[player] + -1;
    if (-1 < effect_card)
    {
      do
      {
        instance = in_play(player, effect_card);
        if (instance != NULL)
        {
          type_or_exile_index = is_what(player, effect_card, 0x80);
          if ((type_or_exile_index != 0) && (instance->targets[9].player == 0x4beb))
          {
            type_or_exile_index = find_iid_in_rfg(1 - player, instance->targets[9].card);
            if (-1 < type_or_exile_index)
            {
              from_exile_to_graveyard(1 - player, type_or_exile_index);
              kill_card(player, effect_card, KILL_EXILE);
            }
          }
        }
        effect_card = effect_card + -1;
      } while (effect_card != -1);
    }
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x261ec50; original cards/guilds_of_ravnica.c:2565. */
int card_mnemonic_betrayal(int player, int card, event_t event)
{
  /* Mnemonic Betrayal	|1|U|B
   * Sorcery
   * Exile all opponents' graveyards. You may cast spells from among those cards this turn, and mana of any type can be spent to cast them. At the beginning of the next end step, if any of those cards remain exiled, return them to their owners' graveyards.
   * Exile ~. */

  int *graveyard_start;
  int opponent;
  int iid_or_effect;
  card_instance_t *instance;
  int source_csvid;
  int *next_grave_entry;
  int *grave_cursor;

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
      opponent = 1 - player;
      iid_or_effect = count_graveyard(opponent);
      if ((iid_or_effect != 0) &&
          (iid_or_effect = graveyard_has_shroud(opponent), iid_or_effect == 0))
      {

        graveyard_start = graveyard_ptr_mutable[opponent];
        grave_cursor = graveyard_start;
        do
        {
          iid_or_effect = *grave_cursor;
          if (iid_or_effect == -1)
            break;
          if ((cards_data[iid_or_effect].type & 1) == 0)
          {

            iid_or_effect = create_may_play_card_from_exile_effect(
                player, card, opponent, (unsigned int)cards_data[iid_or_effect].id, MPCFE_FOR_CMC);
          }
          else
          {
            iid_or_effect = create_legacy_effect(player, card, empty);
          }
          next_grave_entry = grave_cursor + 1;
          set_legacy_image(player, (unsigned int)cards_data[*grave_cursor].id, iid_or_effect);
          instance = get_card_instance(player, iid_or_effect);
          source_csvid = get_id(player, card);
          instance->targets[9].player = source_csvid;
          instance = get_card_instance(player, iid_or_effect);
          instance->targets[9].card = *grave_cursor;
          grave_cursor = next_grave_entry;
        } while (next_grave_entry != graveyard_start + 500);
        rfg_whole_graveyard(opponent);
        create_legacy_effect(player, card, mnemonic_betrayal_legacy);
      }
      kill_card(player, card, KILL_EXILE);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x261ee30; original cards/guilds_of_ravnica.c:2620. */
int card_murmuring_mystic(int player, int card, event_t event)
{
  /* Murmuring Mystic	|3|U
   * Creature - Human Wizard 1/5
   * Whenever you cast an instant or sorcery spell, create a 1/1 blue Bird Illusion creature token with flying. */

  sp_ability_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_BIRD_ILLUSION);
  return 0;
}

/* ManalinkEh 2022: 0x261eea0; original cards/guilds_of_ravnica.c:2658. */
int card_nightveil_predator(int player, int card, event_t event)
{
  /* Nightveil Predator	|U|U|B|B
   * Creature - Vampire 3/3
   * Flying, deathtouch
   * Hexproof (This creature can't be the target of spells or abilities your opponents control.) */

  deathtouch(player, card, event);
  hexproof(player, card, event);
  return 0;
}

/* ManalinkEh 2022: 0x261eee0; original cards/guilds_of_ravnica.c:2679. */
int card_niv_mizzet_parun(int player, int card, event_t event)
{
  /* Niv-Mizzet, Parun	|U|U|U|R|R|R
   * Legendary Creature - Dragon Wizard 5/5
   * This spell can't be countered.
   * Flying
   * Whenever you draw a card, Niv-Mizzet deals 1 damage to any target.
   * Whenever a player casts an instant or sorcery spell, you draw a card. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  check_legend_rule(player, card, event);
  cannot_be_countered(player, card, event);
  result = card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {

    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    instance->number_of_targets = '\0';
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_ANY");
      if (result != 0)
      {
        damage_target0(player, card, 1);
      }
    }
  }
  sp_ability_trigger(2, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  draw_a_card(player);
  return 0;
}

/* ManalinkEh 2022: 0x261f010; original cards/guilds_of_ravnica.c:2715. */
int card_notion_rain(int player, int card, event_t event)
{
  /* Notion Rain	|1|U|B
   * Sorcery
   * Surveil 2, then draw two cards. ~ deals 2 damage to you. (To surveil 2, look at the top two cards of your library, then put any number of them into your graveyard and the rest on top of your library in any order.) */

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
      surveil(player, 2);
      draw_cards(player, 2);
      damage_player(player, 2, player, card);
      kill_card(player, card, KILL_DESTROY);
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x2619460; original cards/guilds_of_ravnica.c:2734. */
static int effect_nullhide_ferox(int player, int card, event_t event)
{
  int p;
  int target_card;
  card_instance_t *instance;
  int source_or_cost;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_or_cost = get_id(p, instance->targets[0].card);
        if (source_or_cost != 0x4bf6)
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
    if (instance->targets[0].player != -1)
    {
      get_card_instance(player, card);
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if (instance->targets[0].player != -1)
    {

      instance = get_card_instance(player, card);
      source_or_cost = instance->targets[0].player;
      target_card = instance->targets[0].card;
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (can_use_activated_abilities(source_or_cost, target_card) != 0)
        {

          source_or_cost =
              get_cost_mod_for_activated_abilities(source_or_cost, target_card, 2, 0, 0, 0, 0, 0);
          if (has_mana_multi(player, source_or_cost, 0, 0, 0, 0, 0) != 0)
          {
            return 1;
          }
        }
      }
      else
      {
        if (event == EVENT_ACTIVATE)
        {

          charge_mana_multi(
              player,
              get_cost_mod_for_activated_abilities(source_or_cost, target_card, 2, 0, 0, 0, 0, 0), 0, 0,
              0, 0, 0);
          return 0;
        }
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          humiliate_and_set_pt_abilities(player, card, source_or_cost, target_card, 4, NULL);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x261f0f0; original cards/guilds_of_ravnica.c:2774. */
int card_nullhide_ferox(int player, int card, event_t event)
{
  /* Nullhide Ferox	|2|G|G
   * Creature - Beast 6/6
   * Hexproof
   * You can't cast noncreature spells.
   * |2: This creature loses all abilities until end of turn. Any player may activate this ability.
   * If a spell or ability an opponent controls causes you to discard this card, put it onto the battlefield instead of putting it into your graveyard. */

  int p;
  int c;
  bool opponent_effect_found;
  card_instance_t *instance;
  int source_or_cost;
  card_instance_t *existing_effect;
  int opponent_or_eligible;

  hexproof(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    source_or_cost = is_humiliated(player, card, AL_STATIC);
    if (((source_or_cost == 0) && (affected_card_controller == player)) &&
        (source_or_cost = is_what(player, affected_card, 3), source_or_cost == 0))
    {
      infinite_casting_cost();
    }
    get_card_instance(player, card);
  }
  else
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_STATIC_EFFECTS)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        opponent_effect_found = false;
        p = 0;
        do
        {

          c = 0;
          if (-1 < active_cards_count[p])
          {
            do
            {
              existing_effect = in_play(p, c);

              if (((existing_effect != NULL) &&
                   (opponent_or_eligible = is_what(p, c, 0x80), opponent_or_eligible != 0)) &&
                  ((existing_effect = get_card_instance(p, c),
                    existing_effect->info_slot_fn == effect_nullhide_ferox &&
                        ((existing_effect->targets[0].player == player &&
                          (existing_effect->targets[0].card == card))))))
              {
                if (player == p)
                {
                  kill_card(player, c, KILL_EXILE);
                }
                else
                {
                  if (p == 1)
                  {
                    opponent_effect_found = true;
                    break;
                  }
                  opponent_effect_found = true;
                }
              }
              c = c + 1;
            } while (c <= active_cards_count[p]);
          }
          p = p + 1;
        } while (p != 2);
        if (!opponent_effect_found)
        {

          opponent_or_eligible = 1 - player;
          source_or_cost = add_card_to_hand(opponent_or_eligible, instance->internal_card_id);
          instance = get_card_instance(
              opponent_or_eligible,
              create_legacy_activate(opponent_or_eligible, source_or_cost, effect_nullhide_ferox));
          instance->targets[0].player = player;
          instance->targets[0].card = card;
          instance->number_of_targets = '\x01';
          obliterate_card(opponent_or_eligible, source_or_cost);
        }
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      humiliate_and_set_pt_abilities(player, card, instance->parent_controller,
                                     instance->parent_card, 4, NULL);
    }
  }
  source_or_cost = is_humiliated(player, card, AL_TRIG);
  if ((source_or_cost != 0) &&
      (source_or_cost = eot_trigger(player, card, event), source_or_cost != 0))
  {
    humiliate(player, card, player, card, 0);
  }
  return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x261f3f0; original cards/guilds_of_ravnica.c:2868. */
int card_parhelion_patrol(int player, int card, event_t event)
{
  /* Parhelion Patrol	|3|W
   * Creature - Human Knight 2/3
   * Flying
   * Vigilance (Attacking doesn't cause this creature to tap.)
   * Mentor (Whenever this creature attacks, put a +1/+1 counter on target attacking creature with lesser power.) */

  vigilance(player, card, event);
  mentor(player, card, event);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x261f430; original cards/guilds_of_ravnica.c:2896. */
int card_pelt_collector(int player, int card, event_t event)
{
  /* Pelt Collector	|G
   * Creature - Elf Warrior 1/1
   * Whenever another creature you control enters or dies, if that creature's power is greater than this creature's, put a +1/+1 counter on this creature.
   * As long as this creature has three or more +1/+1 counters on it, it has trample. */

  _Static_assert(offsetof(card_instance_t, targets) == (sizeof(void *) == 8 ? 136 : 116) && sizeof(target_t) == 8,
                 "native split power slots");
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card))
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.power = get_power(player, card);
    test.power_flag = 2;
    test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    if (is_this_dying(affected_card_controller, affected_card) &&
        affected_card_controller == player && affected_card != card &&
        is_what(player, affected_card, 2))
    {
      int dying_power = get_power(affected_card_controller, affected_card);
      if (get_power(player, card) < dying_power &&
          !check_for_death_trigger_removal(affected_card_controller, affected_card))
      {
        card_instance_t *instance = get_card_instance(player, card);
        if (instance->targets[11].player < 0)
          instance->targets[11].player = 0;
        uint8_t *pending = (uint8_t *)&instance->targets[11].player;
        *pending = (uint8_t)(*pending + 1);
        for (int pos = 0; pos != 21; ++pos)
        {
          int32_t *slot =
              pos < 10 ? &instance->targets[pos].player : &instance->targets[pos - 10].card;
          if (*slot == -1)
          {
            *slot = get_power(affected_card_controller, affected_card);
            break;
          }
        }
      }
    }
  }
  int resolved = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (!resolved && (event == EVENT_ETB || event == EVENT_GRAVEYARD_FROM_PLAY))
    return 0;
  if (resolved)
  {
    card_instance_t *instance = get_card_instance(player, card);
    unsigned amount = (uint32_t)instance->targets[11].card & 0xff;
    int array[amount];
    for (unsigned pos = 0; pos < amount; ++pos)
      array[pos] = pos < 10 ? instance->targets[pos].player : instance->targets[pos - 10].card;
    for (unsigned i = 1; i < amount; ++i)
    {
      for (unsigned j = i; j < amount; ++j)
      {
        if (array[j] < array[i - 1])
        {
          int temp = array[i - 1];
          array[i - 1] = array[j];
          array[j] = temp;
        }
      }
    }
    for (unsigned i = 0; i < amount; ++i)
      if (array[i] > get_power(player, card))
        add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    instance->targets[11].card = 0;
    for (int i = 0; i != 9; ++i)
    {
      instance->targets[i].player = -1;
      instance->targets[i].card = -1;
    }
  }
  if (event == EVENT_ABILITIES && !is_humiliated(player, card, AL_STATIC) &&
      count_counters(player, card, COUNTER_P1_P1) > 2)
    event_result |= 0x80;
  return 0;
}
#endif

/* ManalinkEh 2022: 0x261f880; original cards/guilds_of_ravnica.c:3018. */
int card_pitiless_gorgon(int player, int card, event_t event)
{
  /* Pitiless Gorgon	|1|BG|BG
   * Creature - Gorgon 2/2
   * Deathtouch */

  deathtouch(player, card, event);
  return hybrid(player, card, event);
}

/* ManalinkEh 2022: 0x261f8c0; original cards/guilds_of_ravnica.c:3029. */
int card_plaguecrafter(int player, int card, event_t event)
{
  /* Plaguecrafter	|2|B
   * Creature - Human Shaman 3/2
   * When this creature enters, each player sacrifices a creature or planeswalker of their choice. Each player who can't discards a card. */

  int first_sacrifice;
  int first_player_or_second_sacrifice;
  unsigned int t_player;
  int sac[2];
  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 0x102, "Select a creature or planeswalker to sacrifice.");
    first_player_or_second_sacrifice = current_turn;
    sac[0] = 0;
    sac[1] = 0;
    first_sacrifice = new_sacrifice(player, card, current_turn, SAC_CAUSED | SAC_NO_CANCEL, &test);
    sac[first_player_or_second_sacrifice] = first_sacrifice;
    t_player = current_turn ^ 1;
    first_player_or_second_sacrifice =
        new_sacrifice(player, card, t_player, SAC_CAUSED | SAC_NO_CANCEL, &test);
    sac[t_player] = first_player_or_second_sacrifice;
    if (sac[current_turn] == 0)
    {
      discard(current_turn, 2, player);
    }
    if (sac[current_turn ^ 1U] == 0)
    {
      discard(current_turn ^ 1U, 2, player);
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x261f9f0; original cards/guilds_of_ravnica.c:3054. */
int card_portcullis_vine(int player, int card, event_t event)
{
  /* Portcullis Vine	|G
   * Creature - Plant Wall 0/3
   * Defender (This creature can't attack.)
   * |2, |T, Sacrifice a creature with defender: Draw a card. */

  int result;
  test_definition_t test;
  if (!IS_GAA_EVENT(event))
    return 0;

  new_default_test_definition(&test, 2, "Select a creature with defender to sacrifice.");
  test.keyword = KEYWORD_DEFENDER;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_a_card(player);
  }
  result = generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_LITERAL_PROMPT | GAA_UNTAPPED, 2, 0, 0, 0, 0, 0, 0, NULL,
      "Select a creature with defender to sacrifice.", ~TYPE_NONE, &test, SARV_0);
  return result;
}
#endif

/* ManalinkEh 2022: 0x261fb10; original cards/guilds_of_ravnica.c:3091. */
int card_quasiduplicate(int player, int card, event_t event)
{
  /* Quasiduplicate	|1|U|U
   * Sorcery
   * Create a token that's a copy of target creature you control.
   * Jump-start (You may cast this card from your graveyard by discarding a card in addition to paying its other costs. Then exile this card.) */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
  {
    return jumpstart(player, card, event, 0x4c06);
  }
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = player;
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      copy_token(player, card, player, instance->targets[0].card);
    }
    result = get_flashback(player, card);
    if (result == 0)
    {
      kill_card(player, card, KILL_DESTROY);
    }
    else
    {
      kill_card(player, card, KILL_EXILE);
    }
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE_YOU_CONTROL", 1, NULL);
}

/* ManalinkEh 2022: 0x261fcf0; original cards/guilds_of_ravnica.c:3128. */
int card_radical_idea(int player, int card, event_t event)
{
  /* Radical Idea	|1|U
   * Instant
   * Draw a card.
   * Jump-start (You may cast this card from your graveyard by discarding a card in addition to paying its other costs. Then exile this card.) */

  int result;
  if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
  {
    result = jumpstart(player, card, event, 0x4c07);
    return result;
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
      draw_a_card(player);
      result = get_flashback(player, card);
      if (result == 0)
      {
        kill_card(player, card, KILL_DESTROY);
      }
      else
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x261fdf0; original cards/guilds_of_ravnica.c:3179. */
int card_ral_izzet_viceroy(int player, int card, event_t event)
{
  /* Ral, Izzet Viceroy	|3|U|R
   * Legendary Planeswalker - Ral (5)
   * +1: Look at the top two cards of your library. Put one of them into your hand and the other into your graveyard.
   * -3: Ral deals damage to target creature equal to the total number of instant and sorcery cards you own in exile and in your graveyard.
   * -8: You get an emblem with "Whenever you cast an instant or sorcery spell, this emblem deals 4 damage to any target and you draw two cards." */

  enum
  {
    CHOICE_LOOK_2_CARDS = 1,
    CHOICE_DAMAGE = 2,
    CHOICE_EMBLEM = 3
  };

  int loyalty_or_mode;
  card_instance_t *instance;
  int loyalty_or_grave_spells;
  bool activation_event;
  target_definition_t td;

  check_legend_rule(player, card, event);
  activation_event = event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE;
  if ((event == EVENT_RESOLVE_ACTIVATION) || (activation_event))
  {

    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    loyalty_or_mode = count_counters(player, card, COUNTER_LOYALTY);
    loyalty_or_grave_spells = count_counters(player, card, COUNTER_LOYALTY);
    loyalty_or_mode = dialog_impl(
        player, card, event, DLG_RANDOM, DLG_PLANESWALKER, "Check the first two cards of your deck", 1, 5,
        1, "Damage target creature", can_target(&td),
        ((3 < loyalty_or_grave_spells) ? 15U : 1U), 0xfffffffd, "Emblem", 1,
        ((8 < loyalty_or_mode) ? 20U : 1U), 0xfffffff8, 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (loyalty_or_mode == 0)
      {
        return 0;
      }
    }
    else if (activation_event)
    {
      instance->number_of_targets = '\0';
      if (loyalty_or_mode == CHOICE_DAMAGE)
      {
        new_pick_target(&td, "TARGET_CREATURE", 0, 1);
      }
    }
    else if (loyalty_or_mode == CHOICE_DAMAGE)
    {
      if (valid_target(&td) != 0)
      {

        loyalty_or_mode = count_rfg_by_type(player, 0x38);
        loyalty_or_grave_spells = count_graveyard_by_type(player, 0x38);
        damage_target0(player, card, loyalty_or_grave_spells + loyalty_or_mode);
      }
    }
    else if (loyalty_or_mode == CHOICE_EMBLEM)
    {
      generate_token_by_id(player, card, CARD_ID_RAL_IZZET_VICEROY_EMBLEM);
    }
    else if (loyalty_or_mode == CHOICE_LOOK_2_CARDS)
    {
      select_one_and_mill_the_rest(player, player, 2, 0xff);
    }
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x2620070; original cards/guilds_of_ravnica.c:3259. */
int card_ral_izzet_viceroy_emblem(int player, int card, event_t event)
{
  /* Ral, Izzet Viceroy Emblem	""
   * Emblem - Ral
   * Whenever you cast an instant or sorcery spell, this emblem deals 4 damage to any target and you draw two cards. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  sp_ability_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  td.allow_cancel = 0;
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "TARGET_ANY", 0, 0);
    if (result != 0)
    {
      damage_target0(player, card, 4);
      draw_cards(player, 2);
    }
  }
  instance = get_card_instance(player, card);
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x2620160; original cards/guilds_of_ravnica.c:3300. */
int card_response_resurgence(int player, int card, event_t event)
{
  /* Response // Resurgence	|RW|RW // |3|R|W
   * Instant // Sorcery
   * Response:
   * Response deals 5 damage to target attacking or blocking creature.
   *
   * Resurgence:
   * Creatures you control gain first strike and vigilance until end of turn. After this main phase, there is an additional combat phase followed by an additional main phase. */

  card_instance_t *instance;
  unsigned int selected_halves;
  int result;
  int can_play_first_half;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  td.required_state = TARGET_STATE_IN_COMBAT;
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    return generic_spell(player, card, EVENT_CAST_SPELL, 1, &td,
                         "TARGET_ATTACKING_BLOCKING_CREATURE", 1, NULL);
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
          damage_target0(player, card, 5);
        }
        selected_halves = instance->info_slot;
      }
      if ((selected_halves & 2) != 0)
      {
        pump_subtype_until_eot(player, card, player, -1, 0, 0, 0x100, 8);
        extra_combat_effect(player, card, EVENT_RESOLVE_SPELL, 0, NULL, 1);
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
  return hybrid_split_card(player, card, event, 0x4c0e, can_play_first_half, 5, 0x4c10, result, 8);
}

/* ManalinkEh 2022: 0x2620390; original cards/guilds_of_ravnica.c:3352. */
int card_risk_factor(int player, int card, event_t event)
{
  /* Risk Factor	|2|R
   * Instant
   * Target opponent may have ~ deal 4 damage to them. If that player doesn't, you draw three cards.
   * Jump-start (You may cast this card from your graveyard by discarding a card in addition to paying its other costs. Then exile this card.) */

  int choice;
  card_instance_t *instance;
  int opponent;
  target_definition_t td;

  if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
  {
    return jumpstart(player, card, event, 0x4c12);
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
    else
    {
      if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
      {
        default_target_definition(player, card, &td, 0);
        opponent = 1 - player;
        td.zone = TARGET_ZONE_PLAYERS;
        td.allowed_controller = opponent;
        if (event == EVENT_RESOLVE_SPELL)
        {
          if (valid_target(&td) != 0)
          {

            instance = get_card_instance(player, card);
            choice = do_dialog(instance->targets[0].player, player, card, -1, -1,
                               " Take 4 damage\n Opponent draws 3",
                               (unsigned int)(life[opponent] < 0xb));
            if (choice == 0)
            {
              damage_player(instance->targets[0].player, 4, player, card);
            }
            else if (choice == 1)
            {
              draw_cards(player, 3);
            }
          }
          kill_card(player, card, KILL_DESTROY);
        }
        return generic_spell(player, card, event, 1, &td, "TARGET_OPPONENT", 1, NULL);
      }
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
    }
  }
  default_target_definition(player, card, &td, 0);
  td.allowed_controller = 1 - player;
  td.zone = TARGET_ZONE_PLAYERS;
  return generic_spell(player, card, event, 1, &td, "TARGET_OPPONENT", 1, NULL);
}

/* ManalinkEh 2022: 0x26205b0; original cards/guilds_of_ravnica.c:3394. */
int card_ritual_of_soot(int player, int card, event_t event)
{
  /* Ritual of Soot	|2|B|B
   * Sorcery
   * Destroy all creatures with mana value 3 or less. */

  int result;
  test_definition_t test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&test, 2);
    test.cmc = 4;
    test.cmc_flag = 3;
    new_manipulate_all(player, card, 2, &test, ACT_KILL_DESTROY);
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2620680; original cards/guilds_of_ravnica.c:3436. */
int card_runaway_steam_kin(int player, int card, event_t event)
{
  /* Runaway Steam-Kin	|1|R
   * Creature - Elemental 1/1
   * Whenever you cast a red spell, if this creature has fewer than three +1/+1 counters on it, put a +1/+1 counter on this creature.
   * Remove three +1/+1 counters from this creature: Add |R|R|R. */

  int result;
  test_definition_t test;

  if ((((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (result = count_counters(player, card, COUNTER_P1_P1), result < 3))
  {
    default_test_definition(&test, 0xff);
    test.color = get_sleighted_color_test(player, card, 0x10);
    sp_ability_trigger(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA)
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  }
  else
  {
    if (event == EVENT_COUNT_MANA)
    {
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
      result = can_produce_mana(player, card);
      if (result == 0)
      {
        return 0;
      }
      result = count_counters(player, card, COUNTER_P1_P1);
      if (result < 3)
      {
        return 0;
      }
      result = count_counters(player, card, COUNTER_P1_P1);
      declare_mana_available(player, 4, (result / 3) * 3);
      return 0;
    }
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = can_produce_mana(player, card);
      if (result == 0)
      {
        return 0;
      }
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0, 0, 0, 0,
                                         0x1000300, NULL, NULL);
      return (unsigned int)(result != 0);
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 0x1000300, NULL,
                                NULL);
      tapped_for_mana_color = -2;
      return 0;
    }
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  produce_mana(player, 4, 3);
  return 0;
}

/* ManalinkEh 2022: 0x2620970; original cards/guilds_of_ravnica.c:3477. */
int card_selective_snare(int player, int card, event_t event)
{
  /* Selective Snare	|X|U
   * Sorcery
   * Return X target creatures of the creature type of your choice to their owner's hand. */

  target_t *target_cursor;
  target_t *other_target_cursor;
  int i;
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
      default_target_definition(player, card, &td, 2);
      instance = get_card_instance(player, card);
      td.required_subtype = select_a_subtype(player, card);
      instance->targets[0x11].card = td.required_subtype;
      instance->number_of_targets = instance->number_of_targets + 0xff;
      return x_spell_with_x_targets(player, card, event, &td, "TARGET_CREATURE", 0);
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 2);
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {
        td.required_subtype = instance->targets[0x11].card;
        for (i = 0; i < (int)(unsigned int)instance->number_of_targets;)
        {
          if (validate_target(player, card, &td, i))
          {
            other_target_cursor = instance->targets + i;
            target_cursor = instance->targets + i;
            ++i;
            bounce_permanent(target_cursor->player, other_target_cursor->card);
          }
          else
            ++i;
        }
        kill_card(player, card, KILL_DESTROY);
      }
      return x_spell_with_x_targets(player, card, event, &td, "TARGET_CREATURE", 0);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  default_target_definition(player, card, &td, 2);
  get_card_instance(player, card);
  return x_spell_with_x_targets(player, card, event, &td, "TARGET_CREATURE", 0);
}

/* ManalinkEh 2022: 0x2620b60; original cards/guilds_of_ravnica.c:3538. */
int card_sinister_sabotage(int player, int card, event_t event)
{
  /* Sinister Sabotage	|1|U|U
   * Instant
   * Counter target spell.
   * Surveil 1. (Look at the top card of your library. You may put it into your graveyard.) */

  int result;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {
    result = counterspell_validate(player, card, NULL, 0);
    if (result != 0)
    {

      instance = get_card_instance(player, card);
      real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
      surveil(player, 1);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, event, 2, NULL, NULL, 0, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2620c30; original cards/guilds_of_ravnica.c:3696. */
int card_thief_of_sanity(int player, int card, event_t event)
{
  /* Thief of Sanity	|1|U|B
   * Creature - Specter 2/2
   * Flying
   * Whenever this creature deals combat damage to a player, look at the top three cards of that player's library, exile one of them face down, then put the rest into their graveyard. You may cast that card for as long as it remains exiled, and mana of any type can be spent to cast that spell. */

  int exiled_iid;
  int remaining_hits;
  card_instance_t *instance;
  int reveal_count;
  int pos;
  unsigned int player_pass;
  unsigned int packed_damage_or_player;
  int times_damaged[2];
  test_definition_t test;
  if (damage_dealt_by_me(player, card, event,
                         DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                             DDBM_MUST_BE_COMBAT_DAMAGE) != 0)
  {
    player_pass = 0;
    instance = get_card_instance(player, card);
    packed_damage_or_player = instance->targets[1].player;
    instance->targets[1].player = 0;
    times_damaged[0] = packed_damage_or_player & 0xff;
    times_damaged[1] = packed_damage_or_player >> 8 & 0xff;
    do
    {
      packed_damage_or_player = current_turn ^ player_pass;
      remaining_hits = times_damaged[packed_damage_or_player];
      if (remaining_hits != 0)
      {
        reveal_count = count_deck(packed_damage_or_player);
        if (reveal_count != 0)
        {
          new_default_test_definition(&test, 0xff, "Select a card to exile.");
          if (player == 1)
          {
            test.type = 1;
            test.type_flag = 1;
          }
          if (3 < reveal_count)
          {
            reveal_count = 3;
          }
          test.create_minideck = (unsigned short)reveal_count;
          test.no_shuffle = 1;
          pos = new_select_a_card(player, packed_damage_or_player, 1, (unsigned int)(player == 0),
                                  1, -1, &test);
          if (pos == -1)
          {
            new_default_test_definition(&test, 0xff, "Select a card to exile.");
            pos = new_select_a_card(player, packed_damage_or_player, 1, 1, 1, -1, &test);
          }
          exiled_iid = deck_ptr[packed_damage_or_player][pos];
          rfg_card_in_deck(packed_damage_or_player, pos);
          remove_card_from_rfg(packed_damage_or_player, (unsigned int)cards_data[exiled_iid].id);
          create_may_play_card_from_exile_effect(player, card, packed_damage_or_player,
                                                 (unsigned int)cards_data[exiled_iid].id,
                                                 MPCFE_FACE_DOWN | MPCFE_FOR_CMC);
          if (reveal_count + -1 != 0)
          {
            put_top_x_on_bottom_in_random_order(packed_damage_or_player, reveal_count + -1);
          }
        }
        times_damaged[packed_damage_or_player] = remaining_hits + -1;
      }
      player_pass = player_pass + 1;
    } while (player_pass != 2);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2620eb0; original cards/guilds_of_ravnica.c:3742. */
int card_thought_erasure(int player, int card, event_t event)
{
  /* Thought Erasure	|U|B
   * Sorcery
   * Target opponent reveals their hand. You choose a nonland card from it. That player discards that card.
   * Surveil 1. (Look at the top card of your library. You may put it into your graveyard.) */

  card_instance_t *instance;
  ec_definition_t ec;
  target_definition_t td;
  test_definition_t this_test;

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
      default_ec_definition(instance->targets[0].player, player, &ec);
      new_default_test_definition(&this_test, 1, "Select a nonland card.");
      this_test.type_flag = 1;
      new_effect_coercion(&ec, &this_test);
      surveil(player, 1);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x40, &td, NULL, 1, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2621080; original cards/guilds_of_ravnica.c:3786. */
int card_thousand_year_storm(int player, int card, event_t event)
{
  /* Thousand-Year Storm	|4|U|R
   * Enchantment
   * Whenever you cast an instant or sorcery spell, copy it for each other instant and sorcery spell you've cast before it this turn. You may choose new targets for the copies. */

  int i;
  int spell_count;
  sp_ability_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event == EVENT_SPA)
  {
    for (i = 0; spell_count = get_storminstantsorcery_count(player), i < spell_count; i = i + 1)
    {
      copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
    }
  }
  return global_enchantment(player, card, event);
}
#endif

/* ManalinkEh 2022: 0x2621120; original cards/guilds_of_ravnica.c:3813. */
int card_trostani_discordant(int player, int card, event_t event)
{
  /* Trostani Discordant	|3|G|W
   * Legendary Creature - Dryad 1/4
   * Other creatures you control get +1/+1.
   * When Trostani enters, create two 1/1 white Soldier creature tokens with lifelink.
   * At the beginning of your end step, each player gains control of all creatures they own. */

  token_generation_t token;
  check_legend_rule(player, card, event);
  boost_creature_type(player, card, event, ~SUBTYPE_INVALID, 1, 1, 0, BCT_CONTROLLER_ONLY);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_token_definition(player, card, CARD_ID_SOLDIER, &token);
    token.s_key_plus = 2;
    token.qty = '\x02';
    generate_token(&token);
  }
  if ((current_turn == player) && (eot_trigger(player, card, event) != 0))
  {
    get_back_your_permanents(current_turn, -1, TYPE_CREATURE);
    get_back_your_permanents(current_turn ^ 1, -1, TYPE_CREATURE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2621250; original cards/guilds_of_ravnica.c:3860. */
int card_underrealm_lich(int player, int card, event_t event)
{
  /* Underrealm Lich	|3|B|G
   * Creature - Zombie Elf Shaman 4/3
   * If you would draw a card, instead look at the top three cards of your library, then put one into your hand and the rest into your graveyard.
   * Pay 4 life: This creature gains indestructible until end of turn. Tap it. */

  int parent_card;
  int deck_size_or_source_player;
  card_instance_t *instance;
  int dying;
  test_definition_t this_test;
  if ((((trigger_condition == TRIGGER_REPLACE_CARD_DRAW) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((suppress_draw == 0 && (reason_for_trigger_controller == player))))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
      return generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 0, 4, NULL, NULL);
    }
    if (event == EVENT_RESOLVE_TRIGGER)
    {
      deck_size_or_source_player = count_deck(player);
      if (deck_size_or_source_player < 3)
      {
        deck_size_or_source_player = count_deck(player);
        if (deck_size_or_source_player != 0)
        {
          default_test_definition(&this_test, 0xff);
          this_test.no_shuffle = 1;
          this_test.create_minideck = 3;
          new_global_tutor(player, player, 1, 1, 1, 1, &this_test);
          deck_size_or_source_player = deck_size_or_source_player + -1;
          if (0 < deck_size_or_source_player)
            mill(player, deck_size_or_source_player);
        }
      }
      else
      {
        default_test_definition(&this_test, 0xff);
        this_test.no_shuffle = 1;
        this_test.create_minideck = 3;
        new_global_tutor(player, player, 1, 1, 1, 1, &this_test);
        deck_size_or_source_player = 2;
        mill(player, deck_size_or_source_player);
      }
      suppress_draw = 1;
      return generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 0, 4, NULL, NULL);
    }
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    deck_size_or_source_player = instance->parent_controller;
    instance = get_card_instance(player, card);
    parent_card = instance->parent_card;
    pump_ability_until_eot(deck_size_or_source_player, parent_card, deck_size_or_source_player, parent_card,
                           0, 0, 0, 0x800);
    tap_card(deck_size_or_source_player, parent_card);
    if (check_for_special_ability(deck_size_or_source_player, parent_card, 0x800) != 0)
    {
      ai_modifier = ai_modifier + -0x30;
    }
    dying = is_this_dying(deck_size_or_source_player, parent_card);
    if ((dying != 0) && (deck_size_or_source_player =
                             check_for_special_ability(deck_size_or_source_player, parent_card, 0x800),
                         deck_size_or_source_player == 0))
    {
      ai_modifier = ai_modifier + life[player] * 2 + -10;
    }
  }
  return generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 0, 4, NULL, NULL);
}

/* ManalinkEh 2022: 0x2621530; original cards/guilds_of_ravnica.c:3913. */
int card_unmoored_ego(int player, int card, event_t event)
{
  /* Unmoored Ego	|1|U|B
   * Sorcery
   * Choose a card name. Search target opponent's graveyard, hand, and library for up to four cards with that name and exile them. That player shuffles, then draws a card for each card exiled from their hand this way. */

  int count;
  int deck_iid;
  card_instance_t *instance;
  int deck_index_or_removed;
  unsigned int csvid;
  int32_t player_00;
  int completed_passes;
  target_definition_t td;
  test_definition_t test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td) != 0)
    {

      bool name_chosen = false;
      if (player == 1)
      {
        count = count_deck(instance->targets[0].player);
        deck_index_or_removed = internal_rand(count);
        player_00 = instance->targets[0].player;
        completed_passes = 0;
        do
        {
          deck_iid = deck_ptr[player_00][deck_index_or_removed];
          if (deck_iid == -1)
          {
            completed_passes = completed_passes + 1;
            deck_index_or_removed = 0;
          }
          else
          {
            if (has_subtype_by_id(1, (unsigned int)cards_data[deck_iid].id, SUBTYPE_BASIC) == 0)
            {

              csvid = (unsigned int)cards_data[deck_iid].id;
              name_chosen = true;
              break;
            }
            deck_index_or_removed = deck_index_or_removed + 1;
            player_00 = instance->targets[0].player;
          }
        } while (completed_passes < 2);
        deck_index_or_removed = 0;
      }
      else
      {
        new_default_test_definition(&test, 0xff, "Choose a card name.");
        csvid = new_card_from_list(player, 3, &test);
        if ((int)csvid < 0)
        {
          player_00 = instance->targets[0].player;
          deck_index_or_removed = 0;
        }
        else
          name_chosen = true;
      }
      if (name_chosen)
      {
        deck_index_or_removed = lobotomy_effect(player, instance->targets[0].player, csvid, 4, 4);
        player_00 = instance->targets[0].player;
      }
      draw_cards(player_00, deck_index_or_removed);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}

/* ManalinkEh 2022: 0x26217e0; original cards/guilds_of_ravnica.c:3990. */
int card_venerated_loxodon(int player, int card, event_t event)
{
  /* Venerated Loxodon	|4|W
   * Creature - Elephant Cleric 4/4
   * Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
   * When this creature enters, put a +1/+1 counter on each creature that convoked it. */

  int result;
  card_instance_t *instance;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    result = 0;
    do
    {
      instance = get_card_instance(player, card);
      if (-1 < instance->targets[result].card)
      {
        instance = get_card_instance(player, card);
        add_counters(player, card, player, instance->targets[result].card,
                                 COUNTER_P1_P1, 1);
        instance = get_card_instance(player, card);
        instance->targets[result].card = -1;
      }
      result = result + 1;
    } while (result != 0x12);
  }
  result = generic_spell_with_convoke(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x26218b0; original cards/guilds_of_ravnica.c:4048. */
int card_vraska_golgari_queen(int player, int card, event_t event)
{
  /* Vraska, Golgari Queen	|2|B|G
   * Legendary Planeswalker - Vraska (4)
   * +2: You may sacrifice another permanent. If you do, you gain 1 life and draw a card.
   * -3: Destroy target nonland permanent with mana value 3 or less.
   * -9: You get an emblem with "Whenever a creature you control deals combat damage to a player, that player loses the game." */

  enum
  {
    CHOICE_SAC = 1,
    CHOICE_DESTROY = 2,
    CHOICE_EMBLEM = 3
  };

  card_instance_t *instance;
  int loyalty_or_mode;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if (((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0x47);
    td.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
    td.extra = 3;
    td.illegal_type = TYPE_LAND;
    loyalty_or_mode = count_counters(player, card, COUNTER_LOYALTY);
    loyalty_or_mode = dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
                                  "Sac to draw and gain life", 1, 5, 2, "Destroy target nonland",
                                  can_target(&td), ((3 < loyalty_or_mode) ? 15U : 1U),
                                  0xfffffffd, "Emblem", 1, 0x14, 0xfffffff7, 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (loyalty_or_mode == 0)
      {
        return 0;
      }
    }
    else if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      if (loyalty_or_mode == CHOICE_DESTROY)
      {
        new_pick_target(&td, "Select target nonland permanent with converted mana cost 3 or less.",
                        0, 0x81);
      }
    }
    else if (loyalty_or_mode == CHOICE_DESTROY)
    {
      if (valid_target(&td) != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
    }
    else if (loyalty_or_mode == CHOICE_EMBLEM)
    {
      generate_token_by_id(player, card, CARD_ID_VRASKA_GOLGARI_QUEEN_EMBLEM);
    }
    else if (loyalty_or_mode == CHOICE_SAC)
    {
      new_default_test_definition(&test, 0x47, "Select a permanent to sacrifice.");
      test.not_me = 1;
      if (new_sacrifice(player, card, player, SAC_DONE, &test) != 0)
      {
        gain_life(player, 1);
        draw_a_card(player);
      }
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x2621b60; original cards/guilds_of_ravnica.c:4138. */
int card_vraska_golgari_queen_emblem(int player, int card, event_t event)
{
  /* Vraska, Golgari Queen Emblem	""
   * Emblem - Vraska
   * Whenever a creature you control deals combat damage to a player, that player loses the game. */

  int result;
  card_instance_t *instance;
  unsigned int first_hits;
  unsigned int second_hits;
  test_definition_t test;
  default_test_definition(&test, 2);
  check_damage_test(player, -1, event,
                    DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                        DDBM_MUST_BE_COMBAT_DAMAGE,
                    player, card, 0, &test);
  result = resolve_damage_trigger(player, card, event,
                                  DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                                      DDBM_MUST_BE_COMBAT_DAMAGE,
                                  player, card);
  if (result != 0)
  {
    instance = get_card_instance(player, card);
    first_hits = instance->targets[1].player;
    instance->targets[1].player = 0;
    second_hits = first_hits >> 8 & 0xff;
    for (first_hits = first_hits & 0xff; first_hits != 0; first_hits = first_hits - 1)
    {
      lose_the_game(0);
    }
    if (second_hits != 0)
    {
      do
      {
        lose_the_game(1);
        second_hits = second_hits - 1;
      } while (second_hits != 0);
      return 0;
    }
  }
  return 0;
}
