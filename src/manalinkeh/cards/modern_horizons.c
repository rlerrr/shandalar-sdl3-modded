#include "manalink.h"

int tempered_sliver_legacy(int player, int card, event_t event);

static int bladeback_sliver_shared_ability(int player, int card, event_t event);
static int enduring_sliver_shared_ability(int player, int card, event_t event);
static int hollowhead_sliver_shared_ability(int player, int card, event_t event);
static int scuttling_sliver_shared_ability(int player, int card, event_t event);

static int pro_colorless_legacy(int player, int card, event_t event);

/* ManalinkEh 2022: 0x2335c60; original cards/modern_horizons.c:31. */
int card_archmages_charm(int player, int card, event_t event)
{
  /* Archmage's Charm	|U|U|U
   * Instant
   * Choose one -
   * * Counter target spell.
   * * Target player draws two cards.
   * * Gain control of target nonland permanent with mana value 1 or less. */

  enum
  {
    CHOICE_COUNTER = 1,
    CHOICE_DRAW = 2,
    CHOICE_STEAL = 3
  };

  card_instance_t *instance;
  int counter_available_or_mode;
  int draw_available;
  int steal_available;
  target_definition_t td2;
  target_definition_t td3;

  if (event == EVENT_CAN_COUNTER)
  {
    if (has_mana(player, 2, 3) != 0)
    {
      ai_modifier = ai_modifier + 0x18;
    }
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      get_card_instance(player, card);
      default_target_definition(player, card, &td2, 0);
      td2.zone = TARGET_ZONE_PLAYERS;
      default_target_definition(player, card, &td3, 0x47);
      td3.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
      td3.extra = 1;
      td3.illegal_type = TYPE_LAND;
      if (counterspell(player, card, EVENT_CAN_CAST, NULL, 0) != 0)
      {
        return 99;
      }
      if (generic_spell(player, card, EVENT_CAN_CAST, 1, &td2, NULL, 1, NULL) != 0)
      {
        return 1;
      }
      return generic_spell(player, card, EVENT_CAN_CAST, 1, &td3, NULL, 1, NULL);
    }
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance = get_card_instance(player, card);
        default_target_definition(player, card, &td2, 0);
        td2.zone = TARGET_ZONE_PLAYERS;
        default_target_definition(player, card, &td3, 0x47);
        td3.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
        td3.extra = 1;
        td3.illegal_type = TYPE_LAND;
        if ((card == affected_card) && (player == affected_card_controller))
        {
          instance->number_of_targets = '\0';
          if (check_special_flags2(player, card, -0x80000000) == 0)
          {

            counter_available_or_mode = counterspell(player, card, EVENT_CAN_CAST, NULL, 0);
            draw_available = can_target(&td2);
            steal_available = can_target(&td3);
            if (dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, "Counter a spell",
                            counter_available_or_mode, 1000000, "Draw", draw_available, 10,
                            "Steal a permanent", steal_available, 10, 0, 0, 0, 0) == 0)
            {
              spell_fizzled = 1;
              return 0;
            }
          }
          counter_available_or_mode = instance->info_slot;
          if (counter_available_or_mode == CHOICE_COUNTER)
          {
            return counterspell(player, card, EVENT_CAST_SPELL, NULL, 0);
          }
          if (counter_available_or_mode == CHOICE_DRAW)
          {
            return generic_spell(player, card, EVENT_CAST_SPELL, 1, &td2, "TARGET_PLAYER", 1, NULL);
          }
          if (counter_available_or_mode == CHOICE_STEAL)
          {
            return generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td3,
                                 "Select target nonland permanent with CMC 1 or less.", 1, NULL);
          }
        }
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      instance = get_card_instance(player, card);
      default_target_definition(player, card, &td2, 0);
      td2.zone = TARGET_ZONE_PLAYERS;
      default_target_definition(player, card, &td3, 0x47);
      td3.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
      td3.extra = 1;
      td3.illegal_type = TYPE_LAND;
      if (event == EVENT_RESOLVE_SPELL)
      {
        counter_available_or_mode = instance->info_slot;
        if (counter_available_or_mode == CHOICE_COUNTER)
        {
          return counterspell(player, card, EVENT_RESOLVE_SPELL, NULL, 0);
        }
        if (counter_available_or_mode == CHOICE_DRAW)
        {
          if (valid_target(&td2) != 0)
          {
            draw_cards(instance->targets[0].player, 2);
          }
          counter_available_or_mode = instance->info_slot;
        }
        if ((counter_available_or_mode == CHOICE_STEAL) &&
            (counter_available_or_mode = valid_target(&td3), counter_available_or_mode != 0))
        {
          gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
        }
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
    }
    else if (event == EVENT_CHANGE_TARGET)
    {
      get_card_instance(player, card);
      default_target_definition(player, card, &td2, 0);
      td2.zone = TARGET_ZONE_PLAYERS;
      default_target_definition(player, card, &td3, 0x47);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2336250; original cards/modern_horizons.c:120. */
int card_arcums_astrolabe(int player, int card, event_t event)
{
  /* Arcum's Astrolabe	|S
   * Snow Artifact
   * (|S can be paid with one mana from a snow source.)
   * When this artifact enters, draw a card.
   * |1, |T: Add one mana of any color. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_MODIFY_COST)
  {
    int c1 = get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, 0);
    if (!can_sorcery_be_played(player, EVENT_MODIFY_COST) || !has_snow_mana(player, 1, c1))
      infinite_casting_cost();
    else
      null_casting_cost(player, card);
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      if (played_for_free(player, card) || is_token(player, card))
        return 1;
      int c1 = get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, 0);
      if (!charge_snow_mana(player, card, 1, c1))
      {
        spell_fizzled = 1;
        return 0;
      }
    }
  }
  else if (event == EVENT_ETB)
    draw_a_card(player);
  return card_viridian_acolyte(player, card, event);
}

/* ManalinkEh 2022: 0x23363d0; original cards/modern_horizons.c:158. */
int card_aria_of_flame(int player, int card, event_t event)
{
  /* Aria of Flame	|2|R
   * Enchantment
   * When this enchantment enters, each opponent gains 10 life.
   * Whenever you cast an instant or sorcery spell, put a verse counter on this enchantment, then it deals damage equal to the number of verse counters on it to target player or planeswalker. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    gain_life(1 - player, 10);
    sp_ability_trigger(player, EVENT_ETB, 2, TYPE_SPELL, NULL, player, card);
  }
  else
  {
    sp_ability_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
    if (event == EVENT_SPA)
    {
      add_counters(player, card, player, card, COUNTER_VERSE, 1);
      default_target_definition(player, card, &td, 0x100);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td.allow_cancel = 0;
      result = can_target(&td);
      if (result != 0)
      {
        result = new_pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER", 0, 0);
        if (result != 0)
        {
          result = count_counters(player, card, COUNTER_VERSE);
          damage_target0(player, card, result);
        }
      }
      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2336570; original cards/modern_horizons.c:186. */
int card_astral_drift(int player, int card, event_t event)
{
  /* Astral Drift	|2|W
   * Enchantment
   * Whenever you cycle this card or cycle another card while this enchantment is on the battlefield, you may exile target creature. If you do, return that card to the battlefield under its owner's control at the beginning of the next end step.
   * Cycling |2|W (|2|W, Discard this card: Draw a card.) */

  int result;
  card_instance_t *instance;
  unsigned int argument7;
  target_definition_t td;

  if (1 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    if (((event == EVENT_RESOLVE_ACTIVATION_FROM_HAND) && (spell_fizzled == 0)) &&
        (((((unsigned int)player_bits[player] >> 24 & 0xff) & 2) == 0 || (affected_card != card))))
    {

      draw_cards(player, 1);
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = 1 - player;
      td.allow_cancel = 0;
      argument7 = 0;
      if (player == 1)
      {
        td.allowed_controller = 0;
        result = can_target(&td);
        td.allowed_controller = 2;
        argument7 = (unsigned int)(result == 0);
      }
      result = can_target(&td);
      if (result != 0)
      {

        result =
            do_dialog(player, player, card, -1, -1, " Activate Astral Drift\n Pass", argument7);
        if (result == 0)
        {
          result = pick_target(&td, "TARGET_CREATURE");
          if (result != 0)
          {

            instance = get_card_instance(player, card);
            result = instance->targets[0].card;
            instance->number_of_targets = '\x01';
            remove_until_eot(player, card, instance->targets[0].player, result);
          }
        }
      }
    }
    result = global_enchantment(player, card, event);
    return result;
  }
  return cycling(player, card, event, 2, 0, 0, 0, 0, 1);
}

/* ManalinkEh 2022: 0x2336740; original cards/modern_horizons.c:229. */
int card_ayulas_influence(int player, int card, event_t event)
{
  /* Ayula's Influence	|G|G|G
   * Enchantment
   * Discard a land card: Create a 2/2 green Bear creature token. */

  card_instance_t *instance;
  int result;
  test_definition_t this_test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    new_default_test_definition(&this_test, 1, "Select a land card to discard.");
    this_test.zone = 0x100;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 0,
                                         NULL, NULL);
      if (result == 0)
      {
        return 0;
      }
      result = check_battlefield_for_special_card(player, card, player, 0, &this_test);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      result = charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0);
      if (result == 0)
      {
        return 0;
      }
      instance->number_of_targets = '\0';
      result = new_select_a_card(player, player, 4, 0, 3, -1, &this_test);
      if (result != -1)
      {
        return 0;
      }
      spell_fizzled = 1;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      result = global_enchantment(player, card, event);
      return result;
    }
    new_default_test_definition(&this_test, 1, "Select a land card to discard.");
    this_test.zone = 0x100;
    get_card_instance(player, card);
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_BEAR);
  return 0;
}

/* ManalinkEh 2022: 0x2336980; original cards/modern_horizons.c:267. */
int card_ayula_queen_among_bears(int player, int card, event_t event)
{
  /* Ayula, Queen Among Bears	|1|G
   * Legendary Creature - Bear 2/2
   * Whenever another Bear you control enters, choose one -
   * * Put two +1/+1 counters on target Bear.
   * * Target Bear you control fights target creature you don't control. */

  card_instance_t *instance;
  int result;
  unsigned int fight_available;
  target_definition_t tdb;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    test_definition_t test;
    new_default_test_definition(&test, 0x47, "");
    test.subtype = 0x12;
    test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    target_definition_t td1;
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &tdb, 2);
    tdb.required_subtype = 0x12;
    tdb.preferred_controller = player;
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.allow_cancel = 0;
    default_target_definition(player, card, &td1, 2);
    td1.allow_cancel = 0;
    td1.allowed_controller = (int32_t)(1 - player);
    td1.preferred_controller = (int32_t)((uint32_t)(td1.allowed_controller));
    result = can_target(&td);
    fight_available = 0;
    if (result != 0)
    {
      result = can_target(&td1);
      fight_available = (unsigned int)(result != 0);
    }
    result = can_target(&tdb);
    result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_CANCEL, DLG_AUTOCHOOSE_IF_1,
                         "Counters", result, 5, "Fight", fight_available, 5, 0, 0, 0, 0);
    if (result == 1)
    {
      instance->number_of_targets = '\0';
      new_pick_target(&tdb, "Select target bear", 0, 0x80);
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                   COUNTER_P1_P1, 2);
    }
    else if (result == 2)
    {
      instance->number_of_targets = '\0';
      result = new_pick_target(&td, "Select target creature you control.", 0, 0x81);
      if (result != 0)
      {
        new_pick_target(&td1, "Select target creature opponent controls.", 1, 0x81);
      }
      fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player,
            instance->targets[1].card);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2336cb0; original cards/modern_horizons.c:327. */
int card_azra_smokeshaper(int player, int card, event_t event)
{
  /* Azra Smokeshaper	|3|B
   * Creature - Azra Ninja 3/3
   * Ninjutsu |1|B (|1|B, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand tapped and attacking.)
   * When this creature enters, target creature you control gains indestructible until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "ASHNODS_BATTLEGEAR", 0, 0);
      if (result != 0)
      {
        pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                               0, 0, 0, 0x800);
      }
    }
    instance->number_of_targets = '\0';
  }
  result = ninjutsu(player, card, event, 1, 1, 0, 0, 0, 0, 0x4e64);
  return result;
}

/* ManalinkEh 2022: 0x2334bb0; original cards/modern_horizons.c:372. */
static int bladeback_sliver_shared_ability(int player, int card, event_t event)
{
  int p;
  card_instance_t *instance;
  int source_card_or_result;
  card_instance_t *source_in_play;
  int eligible_or_valid;
  target_definition_t td2;

  instance = get_card_instance(player, card);
  p = instance->targets[1].player;
  source_card_or_result = 0;
  if (-1 < p)
  {

    source_card_or_result = instance->targets[1].card;
    leaves_play(p, source_card_or_result, event, 0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      add_status(player, card, 0x10000);
      in_play(p, source_card_or_result);
    }
    else
    {
      source_in_play = in_play(p, source_card_or_result);
      if (source_in_play == NULL)
      {
        if (event == EVENT_CAN_ACTIVATE)
        {
          return 0;
        }
        if (event == EVENT_CLEANUP)
        {
          kill_card(player, card, KILL_EXILE);
        }
      }
    }
    eligible_or_valid = is_humiliated(p, source_card_or_result, AL_NM_NL_ACT);
    if ((eligible_or_valid != 0) || (0 < hand_count[player]))
    {
      return 0;
    }
    if (event == EVENT_ACTIVATE)
    {
      default_target_definition(player, card, &td2, 0x100);
      td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td2.allow_cancel = 0;
    }
    else
    {
      if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
      {
        return 0;
      }
      default_target_definition(player, card, &td2, 0x100);
      td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td2.allow_cancel = 0;
      if ((event == EVENT_RESOLVE_ACTIVATION) &&
          (eligible_or_valid = would_validate_arbitrary_target(&td2, instance->targets[2].player,
                                                               instance->targets[2].card),
           eligible_or_valid != 0))
      {
        damage_creature(instance->targets[2].player, instance->targets[2].card, 1,
                        instance->targets[0].player, instance->targets[0].card);
      }
    }
    source_card_or_result = shared_sliver_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0,
                                                            0, 0, &td2, "TARGET_ANY");
  }
  return source_card_or_result;
}

/* ManalinkEh 2022: 0x2336e00; original cards/modern_horizons.c:420. */
int card_bladeback_sliver(int player, int card, event_t event)
{
  /* Bladeback Sliver	|1|R
   * Creature - Sliver 2/2
   * Hellbent - As long as you have no cards in hand, Sliver creatures you control have "|T: This creature deals 1 damage to target player or planeswalker." */

  int result;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {

    result = create_legacy_activate(player, card, bladeback_sliver_shared_ability);
    instance = get_card_instance(player, result);
    instance->targets[1].player = player;
    instance->targets[1].card = card;
    instance->number_of_targets = '\x01';
  }
  result = slivercycling(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2336e60; original cards/modern_horizons.c:452. */
int card_cabal_therapist(int player, int card, event_t event)
{
  /* Cabal Therapist	|B
   * Creature - Horror 1/1
   * Menace
   * At the beginning of your first main phase, you may sacrifice a creature. When you do, choose a nonland card name, then target player reveals their hand and discards all cards with that name. */

  int t_player;
  card_instance_t *instance;
  int result;
  ec_definition_t this_definition;
  target_definition_t td;
  test_definition_t test_sac;
  test_definition_t this_test;
  test_definition_t this_test2;

  menace(player, card, event);
  if (event != EVENT_PHASE_CHANGED)
  {
    return 0;
  }
  if ((current_turn == player) && (current_phase == 0x14))
  {
    instance = get_card_instance(player, card);
    if ((instance->info_slot != 0) &&
        (result = can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE), result != 0))
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
      instance->token_status = instance->token_status | 0x20000;
      new_default_test_definition(&test_sac, 2, "Select a creature to sacrifice.");
      result = new_sacrifice(player, card, player, 0, &test_sac);
      if (result == 0)
      {
        instance->token_status = instance->token_status & 0xfffdffff;
        return 0;
      }
      default_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      new_pick_target(&td, "TARGET_PLAYER", 0, 1);
      result = valid_target(&td);
      if ((result != 0) && (0 < hand_count[instance->targets[0].player]))
      {
        new_default_test_definition(&this_test, 1, "Select a nonland card.");
        this_test.type_flag = 1;
        result = name_a_card(player, instance->targets[0].player, &this_test, 3);
        if (-1 < result)
        {
          default_test_definition(&this_test2, 0xff);
          t_player = instance->targets[0].player;
          this_test2.id = result;
          default_ec_definition(t_player, t_player, &this_definition);
          this_definition.effect = 0x81;
          new_effect_coercion(&this_definition, &this_test2);
        }
      }
      instance->token_status = instance->token_status & 0xfffdffff;
    }
    if ((current_phase == 0x14) && (current_turn == player))
    {
      return 0;
    }
  }
  instance = get_card_instance(player, card);
  instance->info_slot = 0x42;
  return 0;
}

/* ManalinkEh 2022: 0x23370c0; original cards/modern_horizons.c:520. */
int card_cleaving_sliver(int player, int card, event_t event)
{
  /* Cleaving Sliver	|3|R
   * Creature - Sliver 2/2
   * Sliver creatures you control get +2/+0. */

  int result;

  boost_creature_type(player, card, event, SUBTYPE_SLIVER, 2, 0, 0,
                      BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  result = slivercycling(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2337120; original cards/modern_horizons.c:531. */
int card_cloudshredder_sliver(int player, int card, event_t event)
{
  /* Cloudshredder Sliver	|R|W
   * Creature - Sliver 1/1
   * Sliver creatures you control have flying and haste. */

  int result;

  boost_subtype(player, card, event, SUBTYPE_SLIVER, 0, 0, 0x20, 4,
                BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  result = slivercycling(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2337190; original cards/modern_horizons.c:594. */
int card_defile(int player, int card, event_t event)
{
  /* Defile	|B
   * Instant
   * Target creature gets -1/-1 until end of turn for each Swamp you control. */

  int result;

  result = get_hacked_subtype(player, card, 0x5009);
  result = count_subtype(player, 1, result);
  result = vanilla_instant_pump(player, card, event, 2, player, -result, -result, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x2337210; original cards/modern_horizons.c:605. */
int card_dregscape_sliver(int player, int card, event_t event)
{
  /* Dregscape Sliver	|1|B
   * Creature - Sliver 2/2
   * Each Sliver creature card in your graveyard has unearth |2.
   * Unearth |2 (|2: Return this card from your graveyard to the battlefield. It gains haste. Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

  int result;
  test_definition_t this_test;
  if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
  {
    result = unearth(player, event, 2, 0, 0, 0, 0, 0);
    return result;
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = has_mana_multi(player, 2, 0, 0, 0, 0, 0);
    if (((result != 0) &&
         (result = any_in_graveyard_by_subtype(player, SUBTYPE_SLIVER), result != 0)) &&
        (result = can_sorcery_be_played(player, EVENT_CAN_ACTIVATE), result != 0))
    {
      return 1;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    charge_mana_multi(player, 2, 0, 0, 0, 0, 0);
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    new_default_test_definition(&this_test, 2, "Select a Sliver to unearth.");
    this_test.subtype = 0xb1;
    result = new_select_a_card(player, player, 2, 1, 2, -1, &this_test);
    if (result != -1)
    {
      reanimate_permanent(player, card, player, result, REANIMATE_UNEARTH);
    }
  }
  result = slivercycling(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2337400; original cards/modern_horizons.c:641. */
int card_echo_of_eons(int player, int card, event_t event)
{
  /* Echo of Eons	|4|U|U
   * Sorcery
   * Each player shuffles their hand and graveyard into their library, then draws seven cards.
   * Flashback |2|U (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

  int result;
  if (((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS)) ||
      (((uint32_t)(event - EVENT_RESOLVE_ACTIVATED_GRAVEYARD_ABILITY) & 0xfffffeff) == 0))
  {
    result = do_flashback(player, card, event, 2, 0, 1, 0, 0, 0);
    return result;
  }
  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = get_flashback(player, card);
    if (result == 0)
    {
      resolve_timetwister(player, card, EVENT_RESOLVE_SPELL);
      kill_card(player, card, KILL_DESTROY);
    }
    else
    {
      resolve_timetwister(player, card, EVENT_RESOLVE_SPELL);
      kill_card(player, card, KILL_EXILE);
    }
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x23351f0; original cards/modern_horizons.c:669. */
int undying_legacy(int player, int card, event_t event)
{
  int iid;
  int owner;
  card_instance_t *instance;
  int attached_card_or_owner;
  int player_00;
  int eligible;

  instance = get_card_instance(player, card);

  if (instance->damage_target_player >= 0 &&
      (instance = get_card_instance(player, card), event == EVENT_GRAVEYARD_FROM_PLAY))
  {
    attached_card_or_owner = instance->damage_target_card;
    if ((((attached_card_or_owner == affected_card) &&
          (player_00 = (int)instance->damage_target_player,
           player_00 == affected_card_controller)) &&
         (eligible = is_token(player_00, attached_card_or_owner), eligible == 0)) &&
        (((eligible = count_counters(player_00, attached_card_or_owner, COUNTER_P1_P1),
           eligible < 1 &&
               (eligible = check_for_death_trigger_removal(player_00, attached_card_or_owner),
                eligible == 0)) &&
          (eligible = is_this_dying(player_00, attached_card_or_owner), eligible != 0))))
    {
      owner = get_owner(player_00, attached_card_or_owner);
      instance->targets[0].player = owner;
      iid = get_original_internal_card_id(player_00, attached_card_or_owner);
      instance->targets[0xb].player = 1;
      instance->damage_target_card = -1;
      instance->targets[0].card = iid;
      instance->damage_target_player = -1;
    }
  }
  if (resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    instance = get_card_instance(player, card);
    attached_card_or_owner = instance->targets[0].player;
    seek_grave_for_id_to_reanimate(attached_card_or_owner, -1, attached_card_or_owner,
                                   (unsigned int)cards_data[instance->targets[0].card].id,
                                   REANIMATE_PLUS1_PLUS1_COUNTER);
    kill_card(player, card, KILL_EXILE);
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x2337540; original cards/modern_horizons.c:700. */
int card_endling(int player, int card, event_t event)
{
  /* Endling	|2|B|B
   * Creature - Zombie Shapeshifter 3/3
   * |B: This creature gains menace until end of turn.
   * |B: This creature gains deathtouch until end of turn.
   * |B: This creature gains undying until end of turn. (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.)
   * |1: This creature gets +1/-1 or -1/+1 until end of turn. */

  enum
  {
    CHOICE_MENACE = 1,
    CHOICE_DEATHTOUCH = 2,
    CHOICE_UNDYING = 3,
    CHOICE_P1_M1 = 4,
    CHOICE_M1_P1 = 5
  };

  int choice;
  card_instance_t *instance;
  unsigned int special_keywords;
  int sick_or_dying;
  unsigned int stored_keywords;
  int power_priority;
  int toughness_priority;
  int available_or_power;
  int available_or_toughness;
  int menace_priority;
  uint32_t deathtouch_priority;

  if ((uint32_t)(event - EVENT_POW_BOOST) < 2)
  {
    if (current_turn == player)
    {
      available_or_toughness = -1;
      available_or_power = 1;
    }
    else
    {
      available_or_toughness = 1;
      available_or_power = -1;
    }
    return generic_shade(player, card, event, 0, 1, 0, 0, 0, 0, 0, available_or_power,
                         available_or_toughness, 0, 0);
  }
  if (event == EVENT_CLEANUP)
  {

    instance = get_card_instance(player, card);
    instance->targets[1].card = 0;
    instance->targets[1].player = 0;
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0,
                                         NULL, NULL);
      }

      available_or_power = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 1, 0,
                                                     0, 0, 0, 0, NULL, NULL);
      available_or_toughness = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0,
                                                         0, 0, 0, 0, 0, NULL, NULL);
      special_keywords = get_special_abilities(player, card);
      stored_keywords = instance->targets[1].card;
      special_keywords =
          special_keywords | stored_keywords & (int)(stored_keywords ^ 0xffffffff) >> 0x1f;
      menace_priority = 1;
      if (!(special_keywords & 0x10000) && current_phase <= 0x14 && current_turn == player)
      {
        sick_or_dying = is_sick(player, card);
        menace_priority = ((sick_or_dying == 0) ? 10U : 1U);
      }
      deathtouch_priority = !(special_keywords & 0x200) && current_phase < 0x19 ? 10 : 1;
      sick_or_dying = is_this_dying(player, card);
      if (current_turn == player)
      {
        toughness_priority = 1;
        power_priority = ((current_phase == 0x18) ? 8U : 1U);
      }
      else
      {
        power_priority = 1;
        toughness_priority = ((current_phase == 0x18) ? 8U : 1U);
      }
      choice = dialog_impl(
          player, card, EVENT_ACTIVATE, DLG_RANDOM, "Menace", available_or_power, menace_priority,
          "Deathtouch", available_or_power, deathtouch_priority, "Undying", available_or_power,
          ((sick_or_dying == 0) ? 1U : 10U), "+1/-1", available_or_toughness,
          power_priority, "-1/+1", available_or_toughness, toughness_priority, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice - 1U < 3)
      {
        if (charge_mana_for_activated_ability(player, card, 0, 1, 0, 0, 0, 0) == 0)
        {
          return 0;
        }
        stored_keywords = instance->targets[1].card;
        if ((int)stored_keywords < -1)
        {
          stored_keywords = 0;
        }
        if (choice == CHOICE_MENACE)
        {
          stored_keywords = stored_keywords | 0x10000;
        }
        else if (choice == CHOICE_DEATHTOUCH)
        {
          stored_keywords = stored_keywords | 0x200;
        }
        instance->targets[1].card = stored_keywords;
        return 0;
      }
      if (1 < choice - 4U)
      {
        return 0;
      }
      charge_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0);
      return 0;
    }
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      return 0;
    }
  }
  instance = get_card_instance(player, card);
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }

  available_or_power = instance->info_slot;
  if (available_or_power == CHOICE_MENACE)
  {
    available_or_power = 0x10000;
  }
  else
  {
    if (available_or_power != CHOICE_DEATHTOUCH)
    {
      if (available_or_power == CHOICE_UNDYING)
      {
        alternate_legacy_text(1, player,
                              create_targetted_legacy_effect(player, card, undying_legacy,
                                                             instance->parent_controller,
                                                             instance->parent_card));
        return 0;
      }
      if (available_or_power == CHOICE_P1_M1)
      {
        available_or_toughness = -1;
        available_or_power = 1;
      }
      else
      {
        if (available_or_power != CHOICE_M1_P1)
        {
          return 0;
        }
        available_or_toughness = 1;
        available_or_power = -1;
      }
      pump_until_eot_merge_previous_alt(player, card, instance->parent_controller,
                                        instance->parent_card, available_or_power,
                                        available_or_toughness, 2);
      return 0;
    }
    available_or_power = 0x200;
  }
  alternate_legacy_text(1, player,
                        pump_ability_until_eot(player, card, instance->parent_controller,
                                               instance->parent_card, 0, 0, 0, available_or_power));
  return 0;
}

/* ManalinkEh 2022: 0x2334e10; original cards/modern_horizons.c:806. */
static int enduring_sliver_shared_ability(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  int source_player = instance->targets[1].player;
  if (source_player < 0)
    return 0;
  int source_card = instance->targets[1].card;
  leaves_play(source_player, source_card, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    add_status(player, card, 0x10000);
    in_play(source_player, source_card);
    if (is_humiliated(source_player, source_card, AL_NM_NL_ACT))
      return 0;
    return 0;
  }
  if (!(in_play(source_player, source_card)))
  {
    if (event == EVENT_CAN_ACTIVATE)
      return 0;
    if (event == EVENT_CLEANUP)
    {
      kill_card(player, card, KILL_EXILE);
      if (is_humiliated(source_player, source_card, AL_NM_NL_ACT))
        return 0;
      return shared_sliver_activated_ability(player, card, event, 0x11, 2, 0, 0, 0, 0, 0, 0, NULL,
                                             NULL);
    }
  }
  if (is_humiliated(source_player, source_card, AL_NM_NL_ACT))
    return 0;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    int target_card = instance->targets[0].card;
    int target_player = instance->targets[0].player;
    add_counters(target_player, target_card, target_player, target_card, COUNTER_P1_P1, 1);
  }
  return shared_sliver_activated_ability(player, card, event, 0x11, 2, 0, 0, 0, 0, 0, 0, NULL,
                                         NULL);
}

/* ManalinkEh 2022: 0x2337bd0; original cards/modern_horizons.c:847. */
int card_enduring_sliver(int player, int card, event_t event)
{
  /* Enduring Sliver	|1|W
   * Creature - Sliver 2/2
   * Outlast |2 (|2, |T: Put a +1/+1 counter on this creature. Outlast only as a sorcery.)
   * Other Sliver creatures you control have outlast |2. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {

    result = create_legacy_activate(player, card, enduring_sliver_shared_ability);
    instance = get_card_instance(player, result);
    instance->targets[1].player = player;
    instance->targets[1].card = card;
    instance->number_of_targets = '\x01';
  }
  result = slivercycling(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2337c30; original cards/modern_horizons.c:866. */
int card_ephemerate(int player, int card, event_t event)
{
  /* Ephemerate	|W
   * Instant
   * Exile target creature you control, then return it to the battlefield under its owner's control.
   * Rebound (If you cast this spell from your hand, exile it as it resolves. At the beginning of your next upkeep, you may cast this card from exile without paying its mana cost.) */

  card_instance_t *instance;
  target_definition_t td;

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
    if (!valid_target(&td))
    {
      kill_card(player, card, KILL_DESTROY);
    }
    else
    {
      blink_effect(instance->targets[0].player, instance->targets[0].card, NULL);
      rebound(player, card);
    }
  }
  return generic_spell(player, card, event, 1, &td, "ASHNODS_BATTLEGEAR", 1, NULL);
}

/* ManalinkEh 2022: 0x2337da0; original cards/modern_horizons.c:898. */
int card_etchings_of_the_chosen(int player, int card, event_t event)
{
  /* Etchings of the Chosen	|1|W|B
   * Enchantment
   * As this enchantment enters, choose a creature type.
   * Creatures you control of the chosen type get +1/+1.
   * |1, Sacrifice a creature of the chosen type: Target creature you control gains indestructible until end of turn. (Damage and effects that say "destroy" don't destroy it.) */

  card_instance_t *instance;
  const char *msg;
  int result;
  subtype_t subtype;
  target_definition_t td;
  test_definition_t test;

  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    subtype = select_a_subtype(player, card);
    instance->info_slot = subtype;
    boost_creature_type(player, card, EVENT_RESOLVE_SPELL, subtype, 1, 1, 0,
                        BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  }
  else
  {
    boost_creature_type(player, card, event, instance->info_slot, 1, 1, 0,
                        BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
    if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
         (event == EVENT_RESOLVE_ACTIVATION)) ||
        (event == EVENT_CLEANUP))
    {
      msg = get_subtype_text("Select %a to sacrifice.", instance->info_slot);
      new_default_test_definition(&test, 2, msg);
      test.subtype = instance->info_slot;
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = player;
      td.preferred_controller = player;
      if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
      {
        pump_ability_until_eot_no_repeat(instance->parent_controller, instance->parent_card,
                                         instance->targets[0].player, instance->targets[0].card, 0,
                                         0x800);
      }
      result = generic_activated_ability_with_sacrifice_as_cost(
          player, card, event, GAA_CAN_TARGET, 1, 0, 0, 0, 0, 0, 0, &td, "ASHNODS_BATTLEGEAR",
          TYPE_NONE, &test, SARV_0);
      return result;
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2337fe0; original cards/modern_horizons.c:970. */
int card_fallen_shinobi(int player, int card, event_t event)
{
  /* Fallen Shinobi	|3|U|B
   * Creature - Zombie Ninja 5/4
   * Ninjutsu |2|U|B (|2|U|B, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand tapped and attacking.)
   * Whenever this creature deals combat damage to a player, that player exiles the top two cards of their library. Until end of turn, you may play those cards without paying their mana costs. */

  unsigned int packed_damage_counts;
  int result;
  card_instance_t *instance;
  test_definition_t this_test;
  result = damage_dealt_by_me(player, card, event,
                              DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                                  DDBM_MUST_BE_COMBAT_DAMAGE);
  if (result != 0)
  {
    instance = get_card_instance(player, card);
    packed_damage_counts = instance->targets[1].player;
    instance->targets[1].player = 0;
    if ((char)packed_damage_counts != '\0')
    {
      default_test_definition(&this_test, 0xff);
      exile_top_x_card_you_may_play_them(player, card, 2, &this_test, 0,
                                         MPCFE_UNTIL_EOT | MPCFE_FOR_FREE);
    }
    if ((packed_damage_counts >> 8 & 0xff) != 0)
    {
      default_test_definition(&this_test, 0xff);
      exile_top_x_card_you_may_play_them(player, card, 2, &this_test, 1,
                                         MPCFE_UNTIL_EOT | MPCFE_FOR_FREE);
    }
  }
  return ninjutsu(player, card, event, 2, 1, 1, 0, 0, 0, 0x4e85);
}

/* ManalinkEh 2022: 0x2338130; original cards/modern_horizons.c:1003. */
int card_feaster_of_fools(int player, int card, event_t event)
{
  /* Feaster of Fools	|4|B|B
   * Creature - Demon 3/3
   * Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
   * Flying
   * Devour 2 (As this creature enters, you may sacrifice any number of creatures. It enters with twice that many +1/+1 counters on it.) */

  devour(player, card, event, 2);
  return generic_spell_with_convoke(player, card, event);
}

/* ManalinkEh 2022: 0x2338170; original cards/modern_horizons.c:1022. */
int card_first_slivers_chosen(int player, int card, event_t event)
{
  /* First Sliver's Chosen	|4|W
   * Creature - Sliver 3/3
   * Sliver creatures you control have exalted. (Whenever a creature you control attacks alone, it gets +1/+1 until end of turn for each instance of exalted among permanents you control.) */

  int c;
  card_instance_t *instance;
  int humiliated;
  unsigned int card_00;
  int exalted_count;

  store_attackers(player, card, event, DAT_ATTACKS_ALONE | DAT_TRACK, player, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
  {

    instance = get_card_instance(player, card);
    card_00 = (unsigned int)(unsigned char)instance->targets[2].player;
    instance = in_play(player, card_00);

    if ((instance != NULL) && (0 < active_cards_count[player]))
    {
      exalted_count = 0;
      c = 0;
      do
      {
        instance = in_play(player, c);
        if (instance != NULL)
        {
          if (is_what(player, c, 2) != 0)
          {
            if (has_subtype(player, c, SUBTYPE_SLIVER) != 0)
            {
              humiliated = is_humiliated(player, c, AL_TRIG);
              exalted_count = exalted_count + (unsigned int)(humiliated == 0);
            }
          }
        }
        c = c + 1;
      } while (c < active_cards_count[player]);
      if (0 < exalted_count)
      {
        pump_until_eot(player, card, player, card_00, exalted_count, exalted_count);
      }
    }
  }
  return slivercycling(player, card, event);
}

/* ManalinkEh 2022: 0x23382e0; original cards/modern_horizons.c:1068. */
int card_force_of_negation(int player, int card, event_t event)
{
  /* Force of Negation	|1|U|U
   * Instant
   * If it's not your turn, you may exile a blue card from your hand rather than pay this spell's mana cost.
   * Counter target noncreature spell. If that spell is countered this way, exile it instead of putting it into its owner's graveyard. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event == EVENT_MODIFY_COST)
  {
    if (current_turn != player)
    {
      result = get_sleighted_color(player, card, 2);
      al_pitchspell(player, card, EVENT_MODIFY_COST, result, 0);
    }
    counterspell_target_definition(player, card, &td, 0xff);
    return 0;
  }
  counterspell_target_definition(player, card, &td, 0xff);
  td.illegal_type = TYPE_CREATURE;
  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 2, &td, NULL, 0, NULL);
  }
  if (event == EVENT_CAST_SPELL || event == EVENT_CHANGE_TARGET)
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
    if ((current_turn == player) ||
        (result = casting_al_pitchspell(player, card, event, 2, 0), result != 0))
    {
      return counterspell(player, card, event, &td, 0);
    }
    spell_fizzled = 1;
  }
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }
  result = counterspell_validate(player, card, &td, 0);
  if (result != 0)
  {

    instance = get_card_instance(player, card);
    result = pre_counterspell(player, card, instance->targets[0].player, instance->targets[0].card);
    if (result == 0)
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
    }
    else
    {
      put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
      rfg_top_card_of_deck(instance->targets[0].player);
    }
  }
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2338530; original cards/modern_horizons.c:1125. */
int card_force_of_vigor(int player, int card, event_t event)
{
  /* Force of Vigor	|2|G|G
   * Instant
   * If it's not your turn, you may exile a green card from your hand rather than pay this spell's mana cost.
   * Destroy up to two target artifacts and/or enchantments. */

  int i;
  target_t *target_cursor;
  target_t *other_target_cursor;
  card_instance_t *instance;
  int target_player;
  int target_card_or_available;
  target_definition_t td;

  if (event == EVENT_MODIFY_COST)
  {
    if (current_turn != player)
    {
      al_pitchspell(player, card, EVENT_MODIFY_COST, get_sleighted_color(player, card, 3), 0);
    }
    default_target_definition(player, card, &td, 0x44);
    return 0;
  }
  default_target_definition(player, card, &td, 0x44);
  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "DISENCHANT", 1, NULL);
  }
  if (event == EVENT_CAST_SPELL || event == EVENT_CHANGE_TARGET)
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
      instance = get_card_instance(player, card);
      if (check_special_flags2(player, card, 0x2000) == 0)
      {
        check_special_flags2(player, card, 0x8000);
      }
    }
    else
    {
      instance = get_card_instance(player, card);
      if (check_special_flags2(player, card, 0x10000) != 0)
      {
        int count = (unsigned int)instance->number_of_targets;
        int t_array[count];
        int tac = 0;
        if (instance->number_of_targets != 0)
        {
          do
          {
            int c = instance->targets[tac].card;
            int p = instance->targets[tac].player;
            ++tac;
            if (c == -1)
              t_array[tac - 1] = get_internal_card_id_from_csv_id(p == player ? 0x615a : 0x6161);
            else
              t_array[tac - 1] = get_card_instance(p, c)->internal_card_id;
          } while (tac < (unsigned int)instance->number_of_targets);
        }
        return show_deck(player, t_array, tac,
                         " Select the target to change (Rules Engine = you, Deadbox = opponent)", 0,
                         gs_done_008b40e0);
      }
      if (check_special_flags2(player, card, 0x4000) != 0)
      {
        td.zone = TARGET_ZONE_IN_PLAY;
        td.required_type = TYPE_CREATURE;
      }
      if (check_special_flags2(player, card, 0x2000) != 0 ||
          check_special_flags2(player, card, 0x8000) != 0)
      {
        td.who_chooses = attacking_card_controller;
        td.allow_cancel = 0;
      }
    }
    if (current_turn != player)
    {
      if (casting_al_pitchspell(player, card, event, 3, 0) == 0)
      {
        spell_fizzled = 1;
      }
    }
    instance->number_of_targets = '\0';
    i = 0;
    do
    {
      target_card_or_available = can_target(&td);
      if (target_card_or_available == 0)
        break;
      target_card_or_available = new_pick_target(&td, "DISENCHANT", i, 0);
      if (target_card_or_available == 0)
        break;
      other_target_cursor = instance->targets + i;
      target_cursor = instance->targets + i;
      i = i + 1;
      state_untargettable(target_cursor->player, other_target_cursor->card, 1);
    } while (i != 2);
    target_player = instance->targets[0].player;
    if ((target_player != -1) &&
        (target_card_or_available = instance->targets[0].card, target_card_or_available != -1))
    {
      state_untargettable(target_player, target_card_or_available, 0);
    }
    target_player = instance->targets[1].player;
    if ((target_player != -1) &&
        (target_card_or_available = instance->targets[1].card, target_card_or_available != -1))
    {
      state_untargettable(target_player, target_card_or_available, 0);
    }
    if (event == EVENT_CHANGE_TARGET)
    {
      target_player = instance->targets[0].player;
      if ((target_player != -1) &&
          (target_card_or_available = instance->targets[0].card, target_card_or_available != -1))
      {
        state_untargettable(target_player, target_card_or_available, 0);
      }
      target_player = instance->targets[1].player;
      if (target_player == -1)
      {
        return 0;
      }
      target_card_or_available = instance->targets[1].card;
      if (target_card_or_available == -1)
      {
        return 0;
      }
      state_untargettable(target_player, target_card_or_available, 0);
      return 0;
    }
  }
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }
  i = 0;
  instance = get_card_instance(player, card);
  if (instance->number_of_targets != '\0')
  {
    do
    {
      other_target_cursor = instance->targets + i;
      target_cursor = instance->targets + i;
      i = i + 1;
      kill_card(target_cursor->player, other_target_cursor->card, KILL_DESTROY);
    } while (i < (int)(unsigned int)instance->number_of_targets);
  }
  instance->number_of_targets = '\0';
  kill_card(player, card, KILL_DESTROY);
  return 0;
}
#endif

/* ManalinkEh 2022: 0x2338a10; original cards/modern_horizons.c:1254. */
int card_generous_gift(int player, int card, event_t event)
{
  /* Generous Gift	|2|W
   * Instant
   * Destroy target permanent. Its controller creates a 3/3 green Elephant creature token. */

  card_instance_t *instance;
  token_generation_t token;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      default_token_definition(player, card, CARD_ID_ELEPHANT, &token);
      token.t_player = instance->targets[0].player;
      generate_token(&token);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PERMANENT", 1, NULL);
}

/* ManalinkEh 2022: 0x2338ba0; original cards/modern_horizons.c:1284. */
int card_geomancers_gambit(int player, int card, event_t event)
{
  /* Geomancer's Gambit	|2|R
   * Sorcery
   * Destroy target land. Its controller may search their library for a basic land card, put it onto the battlefield, then shuffle.
   * Draw a card. */

  if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller))
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, 1);
  if (player == 1)
  {
    td.required_subtype = 0x2000;
    td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      card_instance_t *instance = get_card_instance(player, card);
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      int target_player = instance->targets[0].player;
      bool search = true;
      if (target_player == 0)
      {
        search = do_dialog(0, player, card, -1, -1, " Tutor a basic land\n Pass", 0) != 1;
        if (search)
          target_player = instance->targets[0].player;
      }
      if (search)
        tutor_basic_land(target_player, 1, 0);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_LAND", 1, NULL);
}

/* ManalinkEh 2022: 0x23353f0; original cards/modern_horizons.c:1319. */
static int pro_colorless_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  unsigned int attacker_color;

  attached_gets_sp_kw2(player, card, event, SP_KEYWORD2_GRANTED_PROT_MISC);
  if (event == EVENT_PROT_MISC)
  {
    instance = get_card_instance(player, card);
    if (((instance != NULL) && (instance->damage_target_card == affected_card)) &&
        (instance->damage_target_player == affected_card_controller))
    {
      attacker_color = get_color(attacking_card_controller, attacking_card);
      if ((attacker_color & 0x3e) == 0)
      {
        event_result = event_result | 1;
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

/* ManalinkEh 2022: 0x2338d80; original cards/modern_horizons.c:1331. */
int card_giver_of_runes(int player, int card, event_t event)
{
  /* Giver of Runes	|W
   * Creature - Kor Cleric 1/2
   * |T: Another target creature you control gains protection from colorless or from the color of your choice until end of turn. */

  card_instance_t *instance;
  int result;
  color_t color;
  keyword_t protection;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {

      result = get_deck_color(player, 1 - player);
      color = choose_a_color_from(player, result, COLOR_TEST_ANY);
      if (color == COLOR_COLORLESS)
      {
        create_targetted_legacy_effect(player, card, pro_colorless_legacy,
                                       instance->targets[0].player, instance->targets[0].card);
      }
      else
      {
        protection = color_to_protection(color);
        pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                               instance->targets[0].player, instance->targets[0].card, 0, 0,
                               protection | KEYWORD_RECALC_SET_COLOR, 0);
      }
    }
  }
  return generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                   "TARGET_CREATURE");
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2338f60; original cards/modern_horizons.c:1369. */
int card_gluttonous_slug(int player, int card, event_t event)
{
  /* Gluttonous Slug	|1|B
   * Creature - Slug Horror 0/3
   * Menace
   * Evolve (Whenever a creature you control enters, if that creature has greater power or toughness than this creature, put a +1/+1 counter on this creature.) */

  menace(player, card, event);
  evolve(player, card, event);
  return 0;
}
#endif

/* ManalinkEh 2022: 0x2338fa0; original cards/modern_horizons.c:1394. */
int card_goblin_engineer(int player, int card, event_t event)
{
  /* Goblin Engineer	|1|R
   * Creature - Goblin Artificer 1/2
   * When this creature enters, you may search your library for an artifact card, put it into your graveyard, then shuffle.
   * |R, |T, Sacrifice an artifact: Return target artifact card with mana value 3 or less from your graveyard to the battlefield. */

  int result;
  unsigned int chosen_sacrifice;
  test_definition_t test_sac;
  test_definition_t test_return;

  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test_return, 0x40,
                                "Select an artifact card to put into your graveyard.");
    new_global_tutor(player, player, 1, 3, 0, 6, &test_return);
    return 0;
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    new_default_test_definition(&test_sac, 0x40, "Select an artifact to sacrifice.");
    new_default_test_definition(&test_return, 0x40, "Select an artifact card with CMC 3 or less.");
    test_return.cmc = 4;
    test_return.cmc_flag = 3;
    test_return.ai_selection_mode = 1;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = new_can_sacrifice_as_cost(player, card, &test_sac);
      if (result == 0)
      {
        return 0;
      }
      return generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE,
                                                            GAA_UNTAPPED, 0, 0, 0, 0, 1, 0, 0, NULL,
                                                            NULL, TYPE_NONE, &test_return, 8);
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 1, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      chosen_sacrifice = new_sacrifice(player, card, player,
                                       SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test_sac);
      if (chosen_sacrifice != 0)
      {
        if (spell_fizzled == 1)
        {
          return 0;
        }
        result = select_target_from_grave_source(player, card, player, SFG_0, 1, &test_return, 0);
        if (result != 0)
        {
          if (spell_fizzled == 1)
          {
            return 0;
          }
          kill_card((int)chosen_sacrifice >> 0x10 & 0xff, chosen_sacrifice >> 0x18, KILL_SACRIFICE);
          return 0;
        }
      }
      spell_fizzled = 1;
      remove_untargettable_from_all();
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    new_default_test_definition(&test_sac, 0x40, "Select an artifact to sacrifice.");
    new_default_test_definition(&test_return, 0x40, "Select an artifact card with CMC 3 or less.");
  }
  test_return.ai_selection_mode = 1;
  test_return.cmc_flag = 3;
  test_return.cmc = 4;
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = validate_target_from_grave_source(player, card, player, 0), result != -1))
  {
    reanimate_permanent(player, card, player, result, REANIMATE_DEFAULT);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2339350; original cards/modern_horizons.c:1486. */
int card_hall_of_heliods_generosity(int player, int card, event_t event)
{
  /* Hall of Heliod's Generosity	""
   * Legendary Land
   * |T: Add |C.
   * |1|W, |T: Put target enchantment card from your graveyard on top of your library. */

  int result;
  check_legend_rule(player, card, event);
  result = recycling_land(player, card, event, 4, 1, 0, 0, 0, 0, 1);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23393d0; original cards/modern_horizons.c:1511. */
int card_hexdrinker(int player, int card, event_t event)
{
  /* Hexdrinker	|G
   * Creature - Snake 2/1
   * Level up |1 (|1: Put a level counter on this. Level up only as a sorcery.)
   * LEVEL 3-7
   * 4/4
   * Protection from instants
   * LEVEL 8+
   * 6/6
   * Protection from everything */

  int result;

  if (((event == EVENT_PROT_MISC) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = count_counters(player, card, COUNTER_LEVEL);
    if (7 < result)
    {
      protection_from_everything(player, card, EVENT_PROT_MISC);
    }
  }
  result = level_up(player, card, event, 1, 0, 0, 0, 0, 0, 2, 1, 3, 4, 4,
                         KEYWORD_PROT_INSTANTS, SP_KEYWORD_0, 8, 6, 6, KEYWORD_0, SP_KEYWORD_0);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2335930; original cards/modern_horizons.c:1533. */
static int has_convoked_mana_for_hogaak_impl(int player, int card, int cless, int black, int blue,
                                               int green, int red, int white)
{
  int size = active_cards_count[player];
  int tapped[size];
  int tapped_for_color[size][5];
  memset(tapped_for_color, 0, sizeof tapped_for_color);
  int attempt = 0;
  int scan_size = size;
  for (;;)
  {
    int clrs[6] = {cless, black, blue, green, red, white};
    int result = 0;
    int tc = 0;
    if (scan_size > 0)
    {
      int i = 0;
      do
      {
        if (in_play(player, i) && !is_tapped(player, i) && is_what(player, i, TYPE_CREATURE))
        {
          unsigned int color = get_color(player, i);
          for (int k = 1; k != 6; ++k)
          {
            if ((color >> k & 1U) && clrs[k] > 0 && tapped_for_color[i][k - 1] != 1)
            {
              --clrs[k];
              tapped_for_color[i][k - 1] = 1;
              add_state(player, i, STATE_TAPPED);
              tapped[tc++] = i;
              goto after_creature;
            }
          }
          if (clrs[0] != 0)
          {
            --clrs[0];
            add_state(player, i, STATE_TAPPED);
            tapped[tc++] = i;
          }
        }
      after_creature:
        if (clrs[0] + clrs[1] + clrs[2] + clrs[3] + clrs[4] + clrs[5] == 0)
          result = 1;
        ++i;
      } while (i < active_cards_count[player]);
      for (int i = 0; i < tc; ++i)
        remove_state(player, tapped[i], STATE_TAPPED);
      if (tc > 0 && result != 0)
        return 2;
    }
    ++attempt;
    if (attempt > 49 || result != 0)
      return result;
    scan_size = active_cards_count[player];
  }
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23394f0; original cards/modern_horizons.c:1581. */
int can_cast_hogaak(int player, int card, event_t event, int from_grave)
{
  int cost_or_available;
  int graveyard_size;
  unsigned int convoke_cost;

  cost_or_available = get_updated_casting_cost(player, card, -1, event, -1);
  graveyard_size = count_graveyard(player);
  convoke_cost = graveyard_size - (unsigned int)(from_grave != 0);
  convoke_cost = cost_or_available - (convoke_cost & (int)(convoke_cost ^ 0xffffffff) >> 0x1f);
  convoke_cost = convoke_cost & (int)(convoke_cost ^ 0xffffffff) >> 0x1f;
  cost_or_available = has_convoked_mana_for_hogaak_impl(player, card, convoke_cost, 2, 0, 0, 0, 0);
  if (cost_or_available != 2)
  {
    cost_or_available = has_convoked_mana_for_hogaak_impl(player, card, convoke_cost, 1, 0, 1, 0, 0);
    if (cost_or_available != 2)
    {
      cost_or_available = has_convoked_mana_for_hogaak_impl(player, card, convoke_cost, 0, 0, 2, 0, 0);
      return (unsigned int)(cost_or_available == 2);
    }
  }
  return 1;
}
#endif

#ifndef SHANDALAR
static int hogaak_convoke(int player, int card, int cless, int black, int blue, int green, int red, int white)
{
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = player;
  td.preferred_controller = player;
  int size = active_cards_count[player];
  td.illegal_state = 1;
  td.illegal_abilities = 0;
  td.allow_cancel = 1;
  int tapped[size];
  memset(tapped, 0xff, sizeof tapped);
  card_instance_t *instance = get_card_instance(player, card);
  if (player != 0)
  {
    int color_size = active_cards_count[player];
    int tapped_for_color[color_size][5];
    memset(tapped_for_color, 0, sizeof tapped_for_color);
    int attempts_left = 50;
    int scan_size = color_size;
    for (;;)
    {
      int clrs[6] = {cless, black, blue, green, red, white};
      int tc = 0;
      if (scan_size > 0)
      {
        int i = 0;
        do
        {
          if (in_play(player, i) && !is_tapped(player, i) && is_what(player, i, TYPE_CREATURE))
          {
            unsigned int color = get_color(player, i);
            for (int k = 1; k != 6; ++k)
            {
              if ((color >> k & 1U) && clrs[k] > 0 && tapped_for_color[i][k - 1] != 1)
              {
                --clrs[k];
                tapped_for_color[i][k - 1] = 1;
                add_state(player, i, STATE_TAPPED);
                tapped[tc++] = i;
                goto after_creature;
              }
            }
            if (clrs[0] != 0)
            {
              add_state(player, i, STATE_TAPPED);
              tapped[tc++] = i;
              --clrs[0];
            }
          }
        after_creature:
          if (clrs[0] + clrs[1] + clrs[2] + clrs[3] + clrs[4] + clrs[5] == 0)
          {
            if (spell_fizzled != 1)
            {
              for (int j = 0; j < tc; ++j)
                tap_card(player, tapped[j]);
              return (unsigned int)(spell_fizzled != 1);
            }
            break;
          }
          ++i;
        } while (i < active_cards_count[player]);
        for (int j = 0; j < tc; ++j)
          remove_state(player, tapped[j], STATE_TAPPED);
      }
      --attempts_left;
      if (attempts_left == 0)
      {
        spell_fizzled = 1;
        return (unsigned int)(spell_fizzled != 1);
      }
      scan_size = active_cards_count[player];
    }
  }
  int clrs[6] = {cless, black, blue, green, red, white};
  int tc = 0;
  do
  {
    if (!can_target(&td))
      break;
    if (!select_target(0, card, &td, "Select a creature to tap for Convoke.", NULL))
    {
      spell_fizzled = 1;
      goto rollback;
    }
    instance->number_of_targets = 0;
    if (will_tap_permanent_for_convoke(instance->targets[0].player, instance->targets[0].card,
                                            clrs))
    {
      add_state(instance->targets[0].player, instance->targets[0].card, STATE_TAPPED);
      tapped[tc++] = instance->targets[0].card;
    }
  } while (clrs[1] + clrs[0] + clrs[2] + clrs[3] + clrs[4] + clrs[5] != 0);
  if (spell_fizzled == 1)
  {
  rollback:
    for (int i = 0; i < tc; ++i)
      remove_state(0, tapped[i], STATE_TAPPED);
  }
  else
  {
    for (int i = 0; i < tc; ++i)
      tap_card(0, tapped[i]);
  }
  return (unsigned int)(spell_fizzled != 1);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2339600; original cards/modern_horizons.c:1725. */
int charge_mana_hybrid_to_hogaak_convoke(int player, int card, int number, int first_color,
                                         int second_color, int colorless)
{
  int mana[6] = {0, 0, 0, 0, 0, 0};
  int mana2[10];
  char buffer[500];
  test_definition_t this_test;
  int pos = scnprintf(buffer, 500, " Cancel\n");
  int options = 1;
  if (number >= 0)
  {
    int i = number;
    do
    {
      mana[first_color] = i;
      mana[second_color] = number - i;
      if (has_convoked_mana_for_hogaak_impl(player, card, colorless, mana[1], mana[2], mana[3], mana[4],
          mana[5]))
      {
        mana2[options] = 0;
        mana_into_string(colorless, mana[1], mana[2], mana[3], mana[4], mana[5], &this_test);
        pos += scnprintf(buffer + pos, 500 - pos, " Pay %s\n", this_test.message);
        for (int color = 1; color != 6; ++color)
        {
          for (int k = 0; k < mana[color]; ++k)
          {
            mana2[options] |= 1U << (((color - 1) * 6 + k) & 31);
          }
        }
        ++options;
      }
      --i;
    } while (i != -1);
  }
  int choice = 1;
  if (number >= 0 && options >= 3 && player == 0)
  {
    choice = do_dialog(0, 0, card, -1, -1, buffer, 1);
    if (choice == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  unsigned int encoded = mana2[choice];
  for (int i = 0; i != 6; ++i)
    mana[i] = 0;
  for (int color = 1; color != 6; ++color)
  {
    for (int k = 0; k != 6; ++k)
    {
      if (encoded >> ((color - 1) * 6 + k) & 1U)
        ++mana[color];
    }
  }
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = player;
  td.preferred_controller = player;
  int size = active_cards_count[player];
  td.illegal_state = TARGET_STATE_TAPPED;
  td.illegal_abilities = 0;
  td.allow_cancel = 1;
  int tapped[size];
  memset(tapped, 0xff, sizeof tapped);
  card_instance_t *instance = get_card_instance(player, card);
  if (player != 0)
  {
    int color_size = active_cards_count[player];
    int tapped_for_color[color_size][5];
    memset(tapped_for_color, 0, sizeof tapped_for_color);
    int attempts_left = 50;
    int scan_size = color_size;
    for (;;)
    {
      int clrs[6] = {colorless, mana[1], mana[2], mana[3], mana[4], mana[5]};
      int tc = 0;
      if (scan_size > 0)
      {
        int i = 0;
        do
        {
          if (in_play(player, i) && !is_tapped(player, i) && is_what(player, i, TYPE_CREATURE))
          {
            unsigned int color = get_color(player, i);
            for (int k = 1; k != 6; ++k)
            {
              if ((color >> k & 1U) && clrs[k] > 0 && tapped_for_color[i][k - 1] != 1)
              {
                --clrs[k];
                tapped_for_color[i][k - 1] = 1;
                add_state(player, i, STATE_TAPPED);
                tapped[tc++] = i;
                goto after_creature;
              }
            }
            if (clrs[0] != 0)
            {
              add_state(player, i, STATE_TAPPED);
              tapped[tc++] = i;
              --clrs[0];
            }
          }
        after_creature:
          if (clrs[0] + clrs[1] + clrs[2] + clrs[3] + clrs[4] + clrs[5] == 0)
          {
            if (spell_fizzled != 1)
            {
              for (int j = 0; j < tc; ++j)
                tap_card(player, tapped[j]);
              return (unsigned int)(spell_fizzled != 1);
            }
            break;
          }
          ++i;
        } while (i < active_cards_count[player]);
        for (int j = 0; j < tc; ++j)
          remove_state(player, tapped[j], STATE_TAPPED);
      }
      --attempts_left;
      if (attempts_left == 0)
      {
        spell_fizzled = 1;
        return (unsigned int)(spell_fizzled != 1);
      }
      scan_size = active_cards_count[player];
    }
  }
  int clrs[6] = {colorless, mana[1], mana[2], mana[3], mana[4], mana[5]};
  int tc = 0;
  do
  {
    if (!can_target(&td))
      break;
    if (!select_target(0, card, &td, "Select a creature to tap for Convoke.", NULL))
    {
      spell_fizzled = 1;
      goto rollback;
    }
    instance->number_of_targets = 0;
    if (will_tap_permanent_for_convoke(instance->targets[0].player, instance->targets[0].card,
                                            clrs))
    {
      add_state(instance->targets[0].player, instance->targets[0].card, STATE_TAPPED);
      tapped[tc++] = instance->targets[0].card;
    }
  } while (clrs[1] + clrs[0] + clrs[2] + clrs[3] + clrs[4] + clrs[5] != 0);
  if (spell_fizzled == 1)
  {
  rollback:
    for (int i = 0; i < tc; ++i)
      remove_state(0, tapped[i], STATE_TAPPED);
  }
  else
  {
    for (int i = 0; i < tc; ++i)
      tap_card(0, tapped[i]);
  }
  return (unsigned int)(spell_fizzled != 1);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2339e70; original cards/modern_horizons.c:1781. */
int cast_hogaak(int player, int card, event_t event, int from_grave)
{
  int choice;
  unsigned short grave_csvid;
  card_ptr_t *card_data;
  int *graveyard;
  int32_t *grave_position;
  card_instance_t *instance;
  int available_or_hidden_iid;
  unsigned int cless;
  unsigned int grave_index;
  unsigned int source_csvid;
  int delve_count;
  target_t *target_cursor;
  unsigned int delve_limit;
  test_definition_t this_test;

  instance = get_card_instance(player, card);
  available_or_hidden_iid = get_id(player, card);
  card_data = cards_ptr[available_or_hidden_iid];
  cless = get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, -1);
  available_or_hidden_iid = has_convoked_mana_for_hogaak_impl(player, card, cless, (unsigned int)card_data->req.req_black,
      (unsigned int)card_data->req.req_blue, (unsigned int)card_data->req.req_green,
      (unsigned int)card_data->req.req_red, (unsigned int)card_data->req.req_white);
  if (available_or_hidden_iid == 2)
  {
    choice = do_dialog(player, player, card, -1, -1, " Use Delve\n Don\'t use Delve\n Cancel", 1);
    if (choice == 2)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (choice != 0)
    {
      if (choice != 1)
      {
        return 0;
      }
      charge_mana_hybrid_to_hogaak_convoke(player, card, 2, 1, 3, cless);
      return (unsigned int)(spell_fizzled != 1);
    }
  }
  available_or_hidden_iid = count_graveyard(player);
  grave_index = available_or_hidden_iid - (unsigned int)(from_grave != 0);
  grave_index = grave_index & (int)(grave_index ^ 0xffffffff) >> 0x1f;
  delve_limit = cless;
  if ((int)grave_index < (int)cless)
  {
    delve_limit = grave_index;
  }
  new_default_test_definition(&this_test, 0xff, "Select a card to exile for Delve.");
  available_or_hidden_iid = -1;
  if (from_grave != 0)
  {
    graveyard = graveyard_ptr_mutable[player];
    do
    {
      grave_csvid = cards_data[graveyard[grave_index]].id;
      source_csvid = get_id(player, card);
      if (grave_csvid == source_csvid)
      {
        available_or_hidden_iid = turn_card_in_grave_face_down(player, grave_index);
        break;
      }
      grave_index = grave_index - 1;
    } while (grave_index != 0xffffffff);
  }
  delve_count = select_multiple_cards_from_graveyard(player, player, 0, 3, &this_test, delve_limit,
                                                     instance->targets);
  for (int i = 9; i > 0; --i)
  {
    instance->targets[i] = instance->targets[i - 1];
  }
  if (from_grave != 0)
  {
    turn_card_in_grave_face_up(player, grave_index, available_or_hidden_iid);
  }
  charge_mana_hybrid_to_hogaak_convoke(player, card, 2, 1, 3, cless - delve_count);
  if (spell_fizzled != 1)
  {
    if (0 < delve_count)
    {
      target_cursor = instance->targets + 1;
      do
      {
        grave_position = &target_cursor->player;
        target_cursor = target_cursor + 1;
        rfg_card_from_grave(player, *grave_position);
      } while (target_cursor != instance->targets + delve_count + 1);
    }
    return 1;
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x233a1b0; original cards/modern_horizons.c:1846. */
int card_hogaak_arisen_necropolis(int player, int card, event_t event)
{
  /* Hogaak, Arisen Necropolis	|5|BG|BG
   * Legendary Creature - Avatar 8/8
   * You can't spend mana to cast this spell.
   * Convoke, delve (Each creature you tap while casting this spell pays for |1 or one mana of that creature's color. Each card you exile from your graveyard pays for |1.)
   * You may cast this card from your graveyard.
   * Trample */

  int result;
  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST)
  {
    result = can_cast_hogaak(player, card, EVENT_MODIFY_COST, 0);
    if (result == 0)
    {
      infinite_casting_cost();
    }
    else
    {
      null_casting_cost(player, card);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = played_for_free(player, card);
      if ((result != 0) || (result = is_token(player, card), result != 0))
      {
        return 1;
      }
      result = cast_hogaak(player, card, EVENT_CAST_SPELL, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY);
    if ((result != 0) &&
        (result = can_cast_hogaak(player, card, EVENT_GRAVEYARD_ABILITY, 1), result != 0))
    {
      return 2;
    }
  }
  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    result = cast_hogaak(player, card, EVENT_PAY_FLASHBACK_COSTS, 1);
    return (unsigned int)(result != 0);
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x2335010; original cards/modern_horizons.c:1895. */
static int hollowhead_sliver_shared_ability(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  int source_player = instance->targets[1].player;
  if (source_player < 0)
    return 0;
  int source_card = instance->targets[1].card;
  leaves_play(source_player, source_card, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    add_status(player, card, 0x10000);
    in_play(source_player, source_card);
    if (is_humiliated(source_player, source_card, AL_NM_NL_ACT))
      return 0;
    return 0;
  }
  if (!(instance = in_play(source_player, source_card)))
  {
    if (event == EVENT_CAN_ACTIVATE)
      return 0;
    if (event == EVENT_CLEANUP)
    {
      kill_card(player, card, KILL_EXILE);
      if (is_humiliated(source_player, source_card, AL_NM_NL_ACT))
        return 0;
      return shared_sliver_activated_ability(player, card, event, 0x21, 0, 0, 0, 0, 0, 0, 0, NULL,
                                             NULL);
    }
  }
  if (is_humiliated(source_player, source_card, AL_NM_NL_ACT))
    return 0;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_a_card(player);
  }
  return shared_sliver_activated_ability(player, card, event, 0x21, 0, 0, 0, 0, 0, 0, 0, NULL,
                                         NULL);
}

/* ManalinkEh 2022: 0x233a360; original cards/modern_horizons.c:1936. */
int card_hollowhead_sliver(int player, int card, event_t event)
{
  /* Hollowhead Sliver	|2|R
   * Creature - Sliver 2/2
   * Sliver creatures you control have "|T, Discard a card: Draw a card." */

  int result;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {

    result = create_legacy_activate(player, card, hollowhead_sliver_shared_ability);
    instance = get_card_instance(player, result);
    instance->targets[1].player = player;
    instance->targets[1].card = card;
    instance->number_of_targets = '\x01';
  }
  result = slivercycling(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x233a3c0; original cards/modern_horizons.c:1953. */
int card_ice_fang_coatl(int player, int card, event_t event)
{
  /* Ice-Fang Coatl	|G|U
   * Snow Creature - Snake 1/1
   * Flash
   * Flying
   * When this creature enters, draw a card.
   * This creature has deathtouch as long as you control at least three other snow permanents. */

  int scan_card;
  card_instance_t *instance;
  int snow;
  int snow_count;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_a_card(player);
  }
  else
  {

    if ((((event == EVENT_ABILITIES) && (card == affected_card)) &&
         (player == affected_card_controller)) &&
        (scan_card = active_cards_count[player] + -1, -1 < scan_card))
    {
      snow_count = 0;
      do
      {
        instance = in_play(player, scan_card);
        if ((card != scan_card) && (instance != NULL))
        {
          snow = is_snow_permanent(player, scan_card);
          snow_count = (snow_count + 1) - (unsigned int)(snow == 0);
        }
        scan_card = scan_card + -1;
        if (scan_card < 0)
        {
          if (snow_count < 3)
            return flash(player, card, event);
          break;
        }
      } while (snow_count < 3);
      deathtouch(player, card, EVENT_ABILITIES);
    }
  }
  return flash(player, card, event);
}

/* ManalinkEh 2022: 0x233a4b0; original cards/modern_horizons.c:2005. */
int card_ingenious_infiltrator(int player, int card, event_t event)
{
  /* Ingenious Infiltrator	|2|U|B
   * Creature - Vedalken Ninja 2/3
   * Ninjutsu |U|B (|U|B, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand tapped and attacking.)
   * Whenever a Ninja you control deals combat damage to a player, draw a card. */

  int result;
  test_definition_t test;

  if (event == EVENT_DEAL_DAMAGE)
  {
    default_test_definition(&test, 2);
    test.subtype = 0x83;
    check_damage_test(player, -1, EVENT_DEAL_DAMAGE,
                      DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                          DDBM_MUST_BE_COMBAT_DAMAGE,
                      player, card, 0, &test);
  }
  result = resolve_damage_trigger(
      player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE, player, card);
  if (result != 0)
  {
    draw_cards(player, result);
  }
  result = ninjutsu(player, card, event, 0, 1, 1, 0, 0, 0, 0x4eaa);
  return result;
}

/* ManalinkEh 2022: 0x233a5c0; original cards/modern_horizons.c:2035. */
int card_kayas_guile(int player, int card, event_t event)
{
  /* Kaya's Guile	|1|W|B
   * Instant
   * Choose two -
   * * Each opponent sacrifices a creature of their choice.
   * * Exile all opponents' graveyards.
   * * Create a 1/1 white and black Spirit creature token with flying.
   * * You gain 4 life.
   * Entwine |3 (Choose all if you pay the entwine cost.) */

  enum
  {
    CHOICE_SACRIFICE = 1,
    CHOICE_GRAVE = 2,
    CHOICE_TOKEN = 3,
    CHOICE_LIFE = 4,
    CHOICE_ENTWINE = 5
  };

  int i;
  int first_choice_or_available;
  card_instance_t *instance;
  unsigned int selected_modes;
  int life_priority_or_choice;
  int *priority_cursor;
  uint32_t entwine_priority;
  uint32_t dialog_tail1;
  uint32_t dialog_tail2;
  uint32_t dialog_tail3;
  uint32_t sacrifice_priority;
  uint32_t grave_priority;
  int spirit_priority;

  if (event == EVENT_CAN_CAST)
  {
    return 1;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      int priority[6];
      if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
      {
        priority[1] = 10;
        priority[2] = 6;
        life_priority_or_choice = count_permanents_by_type(1 - player, TYPE_CREATURE);
        first_choice_or_available = count_permanents_by_type(player, TYPE_CREATURE);
        entwine_priority = 0x14;
        priority[5] = 0x14;
        sacrifice_priority = 10;
        grave_priority = 6;
        spirit_priority = (life_priority_or_choice - first_choice_or_available) * 3;
        life_priority_or_choice = 0xf - life[player];
        priority[3] = spirit_priority;
        priority[4] = life_priority_or_choice;
      }
      else
      {
        priority_cursor = priority;
        for (i = 6; i != 0; i = i + -1)
        {
          *priority_cursor = 0;
          priority_cursor = priority_cursor + 1;
        }
        sacrifice_priority = 0;
        grave_priority = 0;
        spirit_priority = 0;
        life_priority_or_choice = 0;
        entwine_priority = 0;
      }
      first_choice_or_available = has_mana(player, 7, 3);
      dialog_tail3 = 0;
      dialog_tail2 = 0;
      dialog_tail1 = 0;
      first_choice_or_available = dialog_impl(
          player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE, "Opponent sacrifices", 1,
          sacrifice_priority, "Exile graveyard", 1, grave_priority, "Create a Spirit", 1,
          spirit_priority, "Gain 4 life", 1, life_priority_or_choice, "Entwine",
          first_choice_or_available, entwine_priority, 0, 0, 0, 0);
      if (first_choice_or_available == CHOICE_ENTWINE)
      {
        charge_mana(player, COLOR_ANY, 3);
        if (spell_fizzled == 1)
        {
          return 0;
        }
        instance = get_card_instance(player, card);
        selected_modes = 0x1e;
      }
      else
      {
        if ((first_choice_or_available == 0) ||
            (life_priority_or_choice = dialog_impl(
                 player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE, "Opponent sacrifices",
                 (unsigned int)(first_choice_or_available != 1), sacrifice_priority,
                 "Exile graveyard", (unsigned int)(first_choice_or_available != 2), grave_priority,
                 "Create a Spirit", (unsigned int)(first_choice_or_available != 3), spirit_priority,
                 "Gain 4 life", (unsigned int)(first_choice_or_available != 4),
                 life_priority_or_choice, 0, 0, 0, 0, dialog_tail1, dialog_tail2, dialog_tail3),
             life_priority_or_choice == 0))
        {
          spell_fizzled = 1;
          return 0;
        }
        instance = get_card_instance(player, card);
        selected_modes = 1 << ((unsigned char)first_choice_or_available & 0x1f) |
                         1 << ((unsigned char)life_priority_or_choice & 0x1f);
      }
      instance->info_slot = selected_modes;
    }
    return 0;
  }
  if ((event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET) &&
      (event != EVENT_CHANGE_TARGET))
  {
    return 0;
  }
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }

  for (int mode = CHOICE_SACRIFICE; mode <= CHOICE_LIFE; ++mode)
  {
    instance = get_card_instance(player, card);
    if (!(instance->info_slot >> mode & 1U))
      continue;
    switch (mode)
    {
    case CHOICE_SACRIFICE:
    {
      test_definition_t test;
      new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
      new_sacrifice(player, card, 1 - player, SAC_CAUSED | SAC_NO_CANCEL, &test);
      break;
    }
    case CHOICE_GRAVE:
      rfg_whole_graveyard(1 - player);
      break;
    case CHOICE_TOKEN:
    {
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_SPIRIT, &token);
      token.key_plus = KEYWORD_FLYING;
      token.color_forced = COLOR_TEST_BLACK | COLOR_TEST_WHITE;
      generate_token(&token);
      break;
    }
    case CHOICE_LIFE:
      gain_life(player, 4);
      break;
    }
  }
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

/* ManalinkEh 2022: 0x233aaa0; original cards/modern_horizons.c:2166. */
int card_lancer_sliver(int player, int card, event_t event)
{
  /* Lancer Sliver	|2|W
   * Creature - Sliver 2/2
   * Sliver creatures you control have first strike. */

  int result;

  boost_subtype(player, card, event, SUBTYPE_SLIVER, 0, 0, 0x100, 0,
                BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  result = slivercycling(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x233ab10; original cards/modern_horizons.c:2177. */
int card_lavabelly_sliver(int player, int card, event_t event)
{
  /* Lavabelly Sliver	|1|R|W
   * Creature - Sliver 2/2
   * Sliver creatures you control have "When this creature enters, it deals 1 damage to target player or planeswalker and you gain 1 life." */

  int card_00;
  int trigger_player;
  card_instance_t *instance;

  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 0x47);
    this_test.subtype = 0xb1;
    etb_ability_global(player, event, 2, TYPE_NONE, &this_test, player, card);
  }
  card_00 = trigger_cause;
  trigger_player = trigger_cause_controller;
  if (event == EVENT_ETB)
  {
    target_definition_t td;
    default_target_definition(trigger_cause_controller, trigger_cause, &td, 0x100);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    instance = get_card_instance(trigger_player, card_00);
    if (can_target(&td) != 0)
    {
      if (new_pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER", 0, 0) != 0)
      {
        damage_creature(instance->targets[0].player, instance->targets[0].card, 1, trigger_player,
                        card_00);
        gain_life(player, 1);
      }
    }
    instance->number_of_targets = '\0';
  }
  return slivercycling(player, card, event);
}

/* ManalinkEh 2022: 0x233acb0; original cards/modern_horizons.c:2219. */
int card_lightning_skelemental(int player, int card, event_t event)
{
  /* Lightning Skelemental	|B|R|R
   * Creature - Elemental Skeleton 6/1
   * Trample, haste
   * Whenever this creature deals combat damage to a player, that player discards two cards.
   * At the beginning of the end step, sacrifice this creature. */

  unsigned int packed_damage_counts;
  int result;
  card_instance_t *instance;

  result = damage_dealt_by_me(player, card, event,
                              DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                                  DDBM_MUST_BE_COMBAT_DAMAGE);
  if (result != 0)
  {

    instance = get_card_instance(player, card);
    packed_damage_counts = instance->targets[1].player;
    instance->targets[1].player = 0;
    new_multidiscard(0, (packed_damage_counts & 0xff) * 2, 0, player);
    new_multidiscard(1, (packed_damage_counts >> 8 & 0xff) * 2, 0, player);
  }
  return card_ball_lightning(player, card, event);
}

/* ManalinkEh 2022: 0x233ad60; original cards/modern_horizons.c:2239. */
int card_llanowar_tribe(int player, int card, event_t event)
{
  /* Llanowar Tribe	|G|G|G
   * Creature - Elf Druid 3/3
   * |T: Add |G|G|G. */

  int result;
  result = mana_producing_creature(player, card, event, 5, COLOR_GREEN, 3);
  return result;
}

/* ManalinkEh 2022: 0x233ada0; original cards/modern_horizons.c:2277. */
int card_mirrodin_besieged(int player, int card, event_t event)
{
  /* Mirrodin Besieged	|2|U
   * Enchantment
   * As this enchantment enters, choose Mirran or Phyrexian.
   * * Mirran - Whenever you cast an artifact spell, create a 1/1 colorless Myr artifact creature token.
   * * Phyrexian - At the beginning of your end step, draw a card, then discard a card. Then if there are fifteen or more artifact cards in your graveyard, target opponent loses the game. */

  card_instance_t *instance;
  int mode_or_value;
  int grave_artifacts;

  if (event == EVENT_RESOLVE_SPELL)
  {
    mode_or_value = get_hand_sum(player, TYPE_ARTIFACT);
    grave_artifacts = count_graveyard_by_type(player, 0x40);
    mode_or_value = do_dialog(player, player, card, -1, -1, " Mirran\n Phyrexian",
                              (unsigned int)(mode_or_value <= grave_artifacts));
    instance = get_card_instance(player, card);
    instance->targets[1].player = mode_or_value;
  }
  instance = get_card_instance(player, card);
  mode_or_value = instance->targets[1].player;
  if (mode_or_value == 0)
  {
    sp_ability_trigger(player, event, 2, TYPE_ARTIFACT, NULL, player, card);
    if (event != EVENT_RESOLVE_SPELL && event == EVENT_SPA)
      generate_token_by_id(player, card, CARD_ID_MYR);
    mode_or_value = instance->targets[1].player;
  }
  if ((mode_or_value == 1) && (current_turn == player))
  {
    if (eot_trigger(player, card, event) != 0)
    {
      draw_a_card(player);
      discard(player, 0, player);
      mode_or_value = count_graveyard_by_type(player, 0x40);
      if (0xe < mode_or_value)
      {
        target_opponent(player, card);
        lose_the_game(instance->targets[0].player);
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x233af90; original cards/modern_horizons.c:2312. */
int card_mist_syndicate_naga(int player, int card, event_t event)
{
  /* Mist-Syndicate Naga	|2|U
   * Creature - Snake Ninja 3/1
   * Ninjutsu |2|U (|2|U, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand tapped and attacking.)
   * Whenever this creature deals combat damage to a player, create a token that's a copy of this creature. */

  int result;
  result =
      damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE);
  if (result != 0)
  {
    copy_token(player, card, player, card);
  }
  result = ninjutsu(player, card, event, 2, 0, 1, 0, 0, 0, 0x4eb9);
  return result;
}

/* ManalinkEh 2022: 0x233b030; original cards/modern_horizons.c:2334. */
int card_moonblade_shinobi(int player, int card, event_t event)
{
  /* Moonblade Shinobi	|3|U
   * Creature - Human Ninja 3/2
   * Ninjutsu |2|U (|2|U, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand tapped and attacking.)
   * Whenever this creature deals combat damage to a player, create a 1/1 blue Illusion creature token with flying. */

  int result;
  token_generation_t token;
  result =
      damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE);
  if (result != 0)
  {
    default_token_definition(player, card, CARD_ID_ILLUSION_TOKEN, &token);
    token.tou = 1;
    token.pow = 1;
    token.key_plus = 0x20;
    generate_token(&token);
  }
  result = ninjutsu(player, card, event, 2, 0, 1, 0, 0, 0, 0x4ebb);
  return result;
}

/* ManalinkEh 2022: 0x233b0f0; original cards/modern_horizons.c:2353. */
int card_morophon_the_boundless(int player, int card, event_t event)
{
  /* Morophon, the Boundless	|7
   * Legendary Creature - Shapeshifter 6/6
   * Changeling (This card is every creature type.)
   * As Morophon enters, choose a creature type.
   * Spells of the chosen type you cast cost |W|U|B|R|G less to cast. This effect reduces only the amount of colored mana you pay.
   * Other creatures you control of the chosen type get +1/+1. */

  subtype_t subtype;
  card_instance_t *instance;
  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    if (player != 1)
    {
      instance = get_card_instance(player, card);
      result = select_a_subtype(player, card);
      instance->info_slot = result;
      return 0;
    }
    state_untargettable(1, card, 1);
    instance = get_card_instance(1, card);
    result = select_a_subtype(1, card);
    instance->info_slot = result;
    state_untargettable(1, card, 0);
  }
  else if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if (affected_card_controller == player)
    {
      instance = get_card_instance(player, card);
      result = has_subtype(affected_card_controller, affected_card, instance->info_slot);
      if (result != 0)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {
          COST_RED = COST_RED + -1;
          COST_WHITE = COST_WHITE + -1;
          COST_BLUE = COST_BLUE + -1;
          COST_BLACK = COST_BLACK + -1;
          COST_GREEN = COST_GREEN + -1;
        }
      }
    }
  }
  else if ((((uint32_t)(event - EVENT_POWER) < 2) && (affected_card_controller == player)) &&
           (affected_card != card))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {

      instance = get_card_instance(player, card);
      subtype = instance->info_slot;
      if (subtype - SUBTYPE_ALLY < 0x113)
      {
        instance = in_play(affected_card_controller, affected_card);
        if (instance != NULL)
        {
          result = has_subtype(affected_card_controller, affected_card, subtype);
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

/* ManalinkEh 2022: 0x233b2e0; original cards/modern_horizons.c:2399. */
int card_mox_tantalite(int player, int card, event_t event)
{
  /* Mox Tantalite	""
   * Artifact
   * Suspend 3-|0 (Rather than cast this card from your hand, pay |0 and exile it with three time counters on it. At the beginning of your upkeep, remove a time counter. When the last is removed, you may cast it without paying its mana cost.)
   * |T: Add one mana of any color. */

  int result;

  result = suspend_only(player, card, event, 3, 0, 0, 0, 0, 0, 0);
  if (result != 0)
  {
    return result;
  }
  return artifact_mana_all_one_color(player, card, event, 1, 0);
}

/* ManalinkEh 2022: 0x233b370; original cards/modern_horizons.c:2426. */
int card_natures_chant(int player, int card, event_t event)
{
  /* Nature's Chant	|1|GW
   * Instant
   * Destroy target artifact or enchantment. */

  card_instance_t *instance;
  target_definition_t td;

  hybrid(player, card, event);
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
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "DISENCHANT", 1, NULL);
}

/* ManalinkEh 2022: 0x233b4d0; original cards/modern_horizons.c:2454. */
int card_ninja_of_the_new_moon(int player, int card, event_t event)
{
  /* Ninja of the New Moon	|3|B|B
   * Creature - Spirit Ninja 6/3
   * Ninjutsu |3|B (|3|B, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand tapped and attacking.) */

  int result;
  result = ninjutsu(player, card, event, 3, 1, 0, 0, 0, 0, 0x4ec2);
  return result;
}

/* ManalinkEh 2022: 0x233b530; original cards/modern_horizons.c:2519. */
int card_plague_engineer(int player, int card, event_t event)
{
  /* Plague Engineer	|2|B
   * Creature - Phyrexian Carrier 2/2
   * Deathtouch
   * As this creature enters, choose a creature type.
   * Creatures of the chosen type your opponents control get -1/-1. */

  card_instance_t *instance;
  subtype_t subtype;

  deathtouch(player, card, event);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    subtype = select_a_subtype(player, card);
    instance->info_slot = subtype;
  }
  else
  {
    subtype = instance->info_slot;
  }
  boost_creature_type(player, card, event, subtype, -1, -1, 0, BCT_OPPONENT_ONLY);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2335b80; original cards/modern_horizons.c:2540. */
static int sac_at_eot(int player, int card, event_t event)
{
  char attached_player;
  int turns_or_attached_card;
  card_instance_t *instance;
  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {
    instance = get_card_instance(player, card);
    if ((((trigger_condition == TRIGGER_EOT) && (card == affected_card)) &&
         (player == affected_card_controller)) &&
        (player == reason_for_trigger_controller))
    {
      turns_or_attached_card = instance->targets[1].player;
      if (turns_or_attached_card < 1)
      {
        if (event == EVENT_TRIGGER)
        {
          event_result = event_result | 2;
        }
        else if (event == EVENT_RESOLVE_TRIGGER)
        {
          attached_player = instance->damage_target_player;
          turns_or_attached_card = instance->damage_target_card;
          if (check_state((int)attached_player, turns_or_attached_card, STATE_OUBLIETTED) == 0)
          {
            kill_card((int)attached_player, turns_or_attached_card, KILL_SACRIFICE);
          }
          else
          {
            kill_card(player, card, KILL_EXILE);
          }
        }
      }
      else
      {
        instance->targets[1].player = turns_or_attached_card + -1;
      }
    }
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x233b5c0; original cards/modern_horizons.c:2569. */
int card_planebound_accomplice(int player, int card, event_t event)
{
  /* Planebound Accomplice	|2|R
   * Creature - Human Wizard 1/3
   * |R: You may put a planeswalker card from your hand onto the battlefield. Sacrifice it at the beginning of the next end step. */

  int result;
  card_instance_t *instance;
  test_definition_t this_test;

  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    new_default_test_definition(&this_test, 0x100, "Select a planeswalker card.");
    result = new_global_tutor(player, player, 4, 4, 0, 2, &this_test);
    if (-1 < result)
    {
      create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, sac_at_eot,
                                     player, result);
    }
  }
  result = generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 1, 0, 0, NULL, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x233b6f0; original cards/modern_horizons.c:2590. */
int card_pondering_mage(int player, int card, event_t event)
{
  /* Pondering Mage	|3|U|U
   * Creature - Human Wizard 3/4
   * When this creature enters, look at the top three cards of your library, then put them back in any order. You may shuffle. Draw a card. */

  int i;
  int library_or_land_count;
  int land_or_creatures;
  unsigned int argument7;
  int controlled_lands;
  int top_count;
  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }

  if ((ai_is_speculating == 1) ||
      ((argument7 = 0, player == 1 && (((unsigned char)trace_mode & 2) == 0))))
  {

    library_or_land_count = count_deck(player);
    top_count = library_or_land_count;
    if (3 < library_or_land_count)
    {
      top_count = 3;
    }
    if (library_or_land_count < 1)
    {
      count_subtype(player, 1, -1);
      argument7 = 0;
      default_test_definition(&test, 1);
      test.zone = 0x100;
      check_battlefield_for_special_card(player, card, player, 4, &test);
    }
    else
    {
      i = 0;
      library_or_land_count = 0;
      do
      {
        land_or_creatures = is_what(-1, deck_ptr[player][i], 1);
        library_or_land_count =
            (library_or_land_count + 1) - (unsigned int)(land_or_creatures == 0);
        i = i + 1;
      } while (i < top_count);
      controlled_lands = count_subtype(player, 1, -1);
      default_test_definition(&test, 1);
      test.zone = 0x100;
      land_or_creatures = check_battlefield_for_special_card(player, card, player, 4, &test);
      if ((library_or_land_count < 2) || (argument7 = 1, land_or_creatures + controlled_lands < 6))
      {
        argument7 =
            (unsigned int)(land_or_creatures + controlled_lands < 3 && library_or_land_count != 0);
      }
    }
  }
  rearrange_top_x(player, player, 3);
  if (do_dialog(player, player, card, -1, -1, " Shuffle\n Pass", argument7) == 0)
  {
    shuffle(player);
  }
  draw_cards(player, 1);
  return 0;
}

/* ManalinkEh 2022: 0x233b920; original cards/modern_horizons.c:2630. */
int card_prismatic_vista(int player, int card, event_t event)
{
  /* Prismatic Vista	""
   * Land
   * |T, Pay 1 life, Sacrifice this land: Search your library for a basic land card, put it onto the battlefield, then shuffle. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ABILITY = 2
  };

  card_instance_t *instance;
  int result;
  bool not_speculating;
  test_definition_t this_test;

  if (event == EVENT_RESOLVE_SPELL)
  {
    play_land_sound_effect(player, card);
  }
  else
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (mana_colors_added[player] == 0)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 0, 0, 0, 0, 0, 0, 1,
                                         NULL, NULL);
      }
      return mana_producer(player, card, EVENT_CAN_ACTIVATE);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance = get_card_instance(player, card);
      if (6 < life[player])
      {
        ai_modifier = ai_modifier + 1000;
      }
      if (mana_colors_added[player] == 0)
      {
        instance->info_slot = 2;
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 9, 0, 0, 0, 0, 0, 0, 1, NULL,
                                         NULL);
      }

      result = paying_mana();
      if ((result == 0) && (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9,
                                                               1, 0, 0, 0, 0, 0, 1, NULL, NULL),
                            result != 0))
      {
        not_speculating = ai_is_speculating != 1;
        instance->info_slot = 2;
        if ((not_speculating) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
        {
          result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1, "Fetch",
                               1, 1, 0, 0, 0, 0);
          if (result == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          if (result == CHOICE_MANA)
          {
            mana_producer(player, card, EVENT_ACTIVATE);
            return 0;
          }
          if (result != CHOICE_ABILITY)
          {
            return 0;
          }
        }
        generic_activated_ability(player, card, EVENT_ACTIVATE, 9, 0, 0, 0, 0, 0, 0, 1, NULL, NULL);
        return 0;
      }
      instance->info_slot = 1;
      mana_producer(player, card, EVENT_ACTIVATE);
      return 0;
    }
    if ((event == EVENT_CLEANUP) || (event == EVENT_RESOLVE_ACTIVATION))
    {
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot != 2)
      {
        return 0;
      }
      new_default_test_definition(&this_test, 1, "Select a basic land");
      this_test.subtype = 0x2000;
      new_global_tutor(player, player, 1, 4, 0, 1, &this_test);
      return 0;
    }
  }
  if (mana_colors_added[player] == 0)
  {
    return 0;
  }
  return mana_producer(player, card, event);
}

/* ManalinkEh 2022: 0x233bd50; original cards/modern_horizons.c:2706. */
int card_putrid_goblin(int player, int card, event_t event)
{
  /* Putrid Goblin	|1|B
   * Creature - Zombie Goblin 2/2
   * Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.) */

  persist(player, card, event);
  return 0;
}

/* ManalinkEh 2022: 0x233bd80; original cards/modern_horizons.c:2736. */
int card_ranger_captain_of_eos(int player, int card, event_t event)
{
  /* Ranger-Captain of Eos	|1|W|W
   * Creature - Human Soldier Ranger 3/3
   * When this creature enters, you may search your library for a creature card with mana value 1 or less, reveal it, put it into your hand, then shuffle.
   * Sacrifice this creature: Your opponents can't cast noncreature spells this turn. */

  test_definition_t this_test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&this_test, 2, "Select a creature with CMC 1 or less");
    this_test.cmc = 2;
    this_test.cmc_flag = 3;
    new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_CLEANUP)
    return generic_activated_ability(player, card, event, 8, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  if (event != EVENT_RESOLVE_ACTIVATION)
    return 0;
  target_player_cant_cast_type(player, card, 1 - player, 0x17c);
  return generic_activated_ability(player, card, event, 8, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x233bee0; original cards/modern_horizons.c:2768. */
int card_ransack_the_lab(int player, int card, event_t event)
{
  /* Ransack the Lab	|1|B
   * Sorcery
   * Look at the top three cards of your library. Put one of them into your hand and the rest into your graveyard. */

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
      reveal_top_cards_of_library_and_choose_type(player, card, player, 3, 1, TUTOR_HAND, 0,
                                                  TUTOR_GRAVE, 0, TYPE_ANY);
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

/* ManalinkEh 2022: 0x2335750; original cards/modern_horizons.c:2883. */
static int scuttling_sliver_shared_ability(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  int source_player = instance->targets[1].player;
  if (source_player < 0)
    return 0;
  int source_card = instance->targets[1].card;
  leaves_play(source_player, source_card, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    add_status(player, card, 0x10000);
    in_play(source_player, source_card);
    if (is_humiliated(source_player, source_card, AL_NM_NL_ACT))
      return 0;
    return 0;
  }
  if (!(in_play(source_player, source_card)))
  {
    if (event == EVENT_CAN_ACTIVATE)
      return 0;
    if (event == EVENT_CLEANUP)
    {
      kill_card(player, card, KILL_EXILE);
      if (is_humiliated(source_player, source_card, AL_NM_NL_ACT))
        return 0;
      return shared_sliver_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL,
                                             NULL);
    }
  }
  if (is_humiliated(source_player, source_card, AL_NM_NL_ACT))
    return 0;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    untap_card(instance->targets[0].player, instance->targets[0].card);
  }
  return shared_sliver_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x233bfd0; original cards/modern_horizons.c:2924. */
int card_scuttling_sliver(int player, int card, event_t event)
{
  /* Scuttling Sliver	|2|U
   * Creature - Sliver Trilobite 2/2
   * Sliver creatures you control have "|2: Untap this creature." */

  int result;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {

    result = create_legacy_activate(player, card, scuttling_sliver_shared_ability);
    instance = get_card_instance(player, result);
    instance->targets[1].player = player;
    instance->targets[1].card = card;
    instance->number_of_targets = '\x01';
  }
  result = slivercycling(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x233c030; original cards/modern_horizons.c:2954. */
int card_serra_the_benevolent(int player, int card, event_t event)
{
  /* Serra the Benevolent	|2|W|W
   * Legendary Planeswalker - Serra (4)
   * +2: Creatures you control with flying get +1/+1 until end of turn.
   * -3: Create a 4/4 white Angel creature token with flying and vigilance.
   * -6: You get an emblem with "If you control a creature, damage that would reduce your life total to less than 1 reduces it to 1 instead." */

  enum
  {
    CHOICE_PUMP = 1,
    CHOICE_ANGEL = 2,
    CHOICE_EMBLEM = 3
  };

  card_instance_t *instance;
  int result;
  token_generation_t token;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_test_definition(&test, 2);
    test.keyword = 0x20;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = check_battlefield_for_id(player, 0x4ee9);
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Pump flyers", 1,
                           5, 2, "Angel", 1, 10, 0xfffffffd, "Emblem", 1,
                           (unsigned int)((result == 0) ? 20 : -20), 0xfffffffa, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      return planeswalker(player, card, event, 4);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 4);
    default_test_definition(&test, 2);
    test.keyword = 0x20;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    result = instance->info_slot;
    if (result == CHOICE_PUMP)
    {
      pump_creatures_until_eot_merge_pt(player, card, player, 1, 1, &test);
    }
    else if (result == CHOICE_ANGEL)
    {
      default_token_definition(player, card, CARD_ID_ANGEL, &token);
      token.s_key_plus = 8;
      generate_token(&token);
    }
    else if (result == CHOICE_EMBLEM)
    {
      generate_token_by_id(player, card, CARD_ID_SERRA_THE_BENEVOLENT_EMBLEM);
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x233c290; original cards/modern_horizons.c:3017. */
int card_serra_the_benevolent_emblem(int player, int card, event_t event)
{
  /* Serra the Benevolent Emblem	""
   * Emblem - Serra
   * If you control a creature, damage that would reduce your life total to less than 1 reduces it to 1 instead. */

  card_instance_t *instance;
  int result;

  if (event == EVENT_DAMAGE_REDUCTION)
  {

    if (((((event_result < 1) &&
           (instance = get_card_instance(affected_card_controller, affected_card),
            instance != NULL)) &&
          (result = (int)instance->damage_target_player, result == player)) &&
         ((instance->damage_target_card == -1 && (0 < creature_count[result])))) &&
        (event_result = life[result], 0 < event_result))
    {
      event_result = 1;
    }
  }
  else if (((event == EVENT_SHOULD_AI_PLAY) &&
            (instance = in_play(player, card), instance != NULL)) &&
           (0 < creature_count[player]))
  {
    if (player == 1)
    {
      ai_modifier = ai_modifier + 0x1e0;
    }
    else
    {
      ai_modifier = ai_modifier + -0x1e0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x233c360; original cards/modern_horizons.c:3059. */
int card_shenanigans(int player, int card, event_t event)
{
  /* Shenanigans	|1|R
   * Sorcery
   * Destroy target artifact.
   * Dredge 1 (If you would draw a card, you may mill a card instead. If you do, return this card from your graveyard to your hand.) */

  target_definition_t td;

  if (event == EVENT_DREDGE)
  {
    return 1;
  }
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x40);
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_ARTIFACT", 1, NULL);
}

/* ManalinkEh 2022: 0x233c4e0; original cards/modern_horizons.c:3104. */
int card_sisay_weatherlight_captain(int player, int card, event_t event)
{
  /* Sisay, Weatherlight Captain	|2|W
   * Legendary Creature - Human Soldier 2/2
   * Sisay gets +1/+1 for each color among other legendary permanents you control.
   * |W|U|B|R|G: Search your library for a legendary permanent card with mana value less than Sisay's power, put that card onto the battlefield, then shuffle. */

  int p;
  int c;
  int turn_or_power;
  int humiliated;
  card_instance_t *instance;
  int eligible;
  unsigned int color_or_count;
  bool controller_turn;
  unsigned int legendary_colors;
  char buffer[75];
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (humiliated = is_humiliated(player, card, AL_STATIC), turn_or_power = current_turn,
         humiliated == 0))
    {

      c = active_cards_count[current_turn] + -1;
      if (c < 0)
      {
        legendary_colors = 0;
      }
      else
      {
        controller_turn = player == current_turn;
        legendary_colors = 0;
        do
        {
          if (((card != c) && (controller_turn)) &&
              ((instance = in_play(turn_or_power, c),
                instance != NULL && ((eligible = is_what(turn_or_power, c, 0x47),
                                      eligible != 0 && (eligible = is_legendary(turn_or_power, c),
                                                        eligible != 0))))))
          {
            color_or_count = get_color(turn_or_power, c);
            legendary_colors = legendary_colors | color_or_count & 0x3e;
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
          if ((((card != c) && (player == p)) && (instance = in_play(p, c), instance != NULL)) &&
              ((eligible = is_what(p, c, 0x47),
                eligible != 0 && (eligible = is_legendary(p, c), eligible != 0))))
          {
            color_or_count = get_color(p, c);
            legendary_colors = legendary_colors | color_or_count & 0x3e;
          }
          c = c + -1;
        } while (c != -1);
      }
      color_or_count = num_bits_set(legendary_colors);
      event_result = event_result + color_or_count;
    }
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_CLEANUP)
    return generic_activated_ability(player, card, event, 0x100000, 0, 1, 1, 1, 1, 1, 0, NULL,
                                     NULL);
  if (event != EVENT_RESOLVE_ACTIVATION)
    return 0;
  turn_or_power = get_power(player, card);
  scnprintf(buffer, 0x4b, "Select a legendary permanent card with converted mana cost %d or less",
            turn_or_power);
  new_default_test_definition(&this_test, 0x47, buffer);
  this_test.subtype = 0x2001;
  turn_or_power = get_power(player, card);
  this_test.cmc = turn_or_power + 1;
  this_test.cmc_flag = 3;
  new_global_tutor(player, player, 1, 4, 0, 1, &this_test);
  return generic_activated_ability(player, card, event, 0x100000, 0, 1, 1, 1, 1, 1, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x233c830; original cards/modern_horizons.c:3148. */
int card_smiting_helix(int player, int card, event_t event)
{
  /* Smiting Helix	|3|B
   * Sorcery
   * ~ deals 3 damage to any target and you gain 3 life.
   * Flashback |R|W (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

  int result;
  target_definition_t td;
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    return do_flashback(player, card, event, 0, 0, 0, 0, 1, 1);
  }
  if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    ai_modifier = ai_modifier + (unsigned int)((life[player] < 6) ? 0 : -24) + 0x30;
    return do_flashback(player, card, event, 0, 0, 0, 0, 1, 1);
  }
  if (((uint32_t)(event - EVENT_RESOLVE_ACTIVATED_GRAVEYARD_ABILITY) & 0xfffffeff) == 0)
    return do_flashback(player, card, event, 0, 0, 0, 0, 1, 1);
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
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      if ((card == affected_card) && (player == affected_card_controller))
      {
        ai_modifier = ai_modifier + (unsigned int)((life[player] < 6) ? 0 : -24) + 0x30;
      }
      return generic_spell(player, card, event, 1, &td, "TARGET_ANY", 1, NULL);
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      if (event == EVENT_RESOLVE_SPELL)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          damage_target0(player, card, 3);
          gain_life(player, 3);
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
      return generic_spell(player, card, event, 1, &td, "TARGET_ANY", 1, NULL);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  return generic_spell(player, card, event, 1, &td, "TARGET_ANY", 1, NULL);
}

/* ManalinkEh 2022: 0x233cad0; original cards/modern_horizons.c:3205. */
int card_soulherder(int player, int card, event_t event)
{
  /* Soulherder	|1|W|U
   * Creature - Spirit 1/1
   * Whenever a creature is exiled from the battlefield, put a +1/+1 counter on this creature.
   * At the beginning of your end step, you may exile another target creature you control, then return that card to the battlefield under its owner's control. */

  int result;
  resolve_trigger_t trig_mode;
  card_instance_t *instance;

  if (trigger_condition == TRIGGER_EOT && card == affected_card && player == affected_card_controller &&
      current_turn == player)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.special = TARGET_SPECIAL_NOT_ME;
    instance = get_card_instance(player, card);
    instance->number_of_targets = '\0';
    result = can_target(&td);
    trig_mode = RESOLVE_TRIGGER_0;
    if (result != 0)
    {
      result = duh_mode(player);
      trig_mode = (result != 0) + RESOLVE_TRIGGER_OPTIONAL;
    }
    result = eot_trigger_mode(player, card, event, player, trig_mode);
    if ((result != 0) && (result = pick_target(&td, "ASHNODS_BATTLEGEAR"), result != 0))
    {
      blink_effect(instance->targets[0].player, instance->targets[0].card, NULL);
    }
  }
  if (((trigger_condition == TRIGGER_LEAVE_PLAY) && (card == affected_card)) &&
      ((player == affected_card_controller &&
        ((reason_for_trigger_controller == player &&
          (instance = get_card_instance(trigger_cause_controller, trigger_cause),
           instance->kill_code == '\x04'))))))
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test_leaves_play(player, card, event, 2, &test, 1, 1);
  }
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x23354b0; original cards/modern_horizons.c:3255. */
int spiteful_sliver_legacy(int player, int card, event_t event)
{
  int attached_card;
  card_instance_t *legacy_record;
  card_instance_t *sliver;
  int target_player;
  target_definition_t td;

  if (((trigger_condition == TRIGGER_DEAL_DAMAGE) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    legacy_record = get_card_instance(player, card);
    if (legacy_record != NULL)
    {
      target_player = (int)legacy_record->damage_target_player;
      attached_card = legacy_record->damage_target_card;
      sliver = get_card_instance(target_player, attached_card);
      if ((sliver != NULL) && (reason_for_trigger_controller == target_player))
      {
        if (event == EVENT_TRIGGER)
        {
          event_result = event_result | 2;
        }
        else if (event == EVENT_RESOLVE_TRIGGER)
        {
          default_target_definition(target_player, attached_card, &td, 0x100);
          td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
          td.allow_cancel = 0;
          sliver = get_card_instance(target_player, attached_card);
          if (can_target(&td) != 0)
          {
            if (new_pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER", 0, 0) != 0)
            {
              damage_creature(sliver->targets[0].player, sliver->targets[0].card,
                              legacy_record->targets[9].card, target_player, attached_card);
            }
          }
          kill_card(player, card, KILL_EXILE);
        }
      }
    }
  }
  if (eot_trigger(player, card, event) == 0)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x233cce0; original cards/modern_horizons.c:3282. */
int card_spiteful_sliver(int player, int card, event_t event)
{
  /* Spiteful Sliver	|2|R
   * Creature - Sliver 2/2
   * Sliver creatures you control have "Whenever this creature is dealt damage, it deals that much damage to target player or planeswalker." */

  int damaged_card;
  card_instance_t *damage;
  card_instance_t *legacy;

  if (event == EVENT_DEAL_DAMAGE)
  {

    damage = damage_being_dealt(EVENT_DEAL_DAMAGE);
    if ((damage != NULL) && (damage->damage_target_player == player))
    {
      damaged_card = damage->damage_target_card;
      if (damaged_card != -1)
      {
        if (is_what(player, damaged_card, 2) != 0)
        {
          if (has_subtype(player, damaged_card, SUBTYPE_SLIVER) != 0)
          {
            if (is_humiliated(player, damaged_card, AL_TRIG) == 0)
            {

              legacy = get_card_instance(
                  player, create_targetted_legacy_effect(
                              player, damaged_card, spiteful_sliver_legacy, player, damaged_card));
              legacy->targets[9].card = damage->info_slot;
            }
          }
        }
      }
    }
  }
  return slivercycling(player, card, event);
}

/* ManalinkEh 2022: 0x233cde0; original cards/modern_horizons.c:3311. */
int card_springbloom_druid(int player, int card, event_t event)
{
  /* Springbloom Druid	|2|G
   * Creature - Elf Druid 1/1
   * When this creature enters, you may sacrifice a land. If you do, search your library for up to two basic land cards, put them onto the battlefield tapped, then shuffle. */

  int result;
  test_definition_t test_sac;
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test_sac, 1, "Select a land to sacrifice.");
    result = new_sacrifice(player, card, player, 0, &test_sac);
    if (result != 0)
    {
      tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 2);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x233ce90; original cards/modern_horizons.c:3356. */
int card_sword_of_sinew_and_steel(int player, int card, event_t event)
{
  /* Sword of Sinew and Steel	|3
   * Artifact - Equipment
   * Equipped creature gets +2/+2 and has protection from black and from red.
   * Whenever equipped creature deals combat damage to a player, destroy up to one target planeswalker and up to one target artifact.
   * Equip |2 */

  int damage_triggers;
  unsigned int black_protection;
  unsigned int red_protection;
  card_instance_t *instance;
  int target_available_or_selected;
  target_definition_t td1;
  target_definition_t td2;

  damage_triggers = equipped_creature_deals_damage(
      player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE);
  if (damage_triggers != 0)
  {
    default_target_definition(player, card, &td1, 0x100);
    default_target_definition(player, card, &td2, 0x40);
    td2.allow_cancel = 1;
    td1.allow_cancel = 1;
    instance = get_card_instance(player, card);
    if (0 < damage_triggers)
    {
      do
      {
        instance->number_of_targets = '\0';
        target_available_or_selected = can_target(&td1);
        if ((target_available_or_selected != 0) &&
            (target_available_or_selected = new_pick_target(&td1, "TARGET_PLANESWALKER", 0, 0),
             target_available_or_selected != 0))
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
        instance->number_of_targets = '\0';
        target_available_or_selected = can_target(&td2);
        if ((target_available_or_selected != 0) &&
            (target_available_or_selected = new_pick_target(&td2, "TARGET_ARTIFACT", 0, 0),
             target_available_or_selected != 0))
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
        damage_triggers = damage_triggers + -1;
      } while (damage_triggers != 0);
    }
  }
  black_protection = get_sleighted_protection(player, card, 0x800);
  red_protection = get_sleighted_protection(player, card, 0x4000);
  return vanilla_equipment(player, card, event, 2, 2, 2, black_protection | red_protection, 0);
}

/* ManalinkEh 2022: 0x233d080; original cards/modern_horizons.c:3390. */
int card_sword_of_truth_and_justice(int player, int card, event_t event)
{
  /* Sword of Truth and Justice	|3
   * Artifact - Equipment
   * Equipped creature gets +2/+2 and has protection from white and from blue.
   * Whenever equipped creature deals combat damage to a player, put a +1/+1 counter on a creature you control, then proliferate. (Choose any number of permanents and/or players, then give each another counter of each kind already there.)
   * Equip |2 */

  unsigned int blue_protection;
  unsigned int white_protection;
  int result;
  card_instance_t *instance;
  int damage_triggers;
  target_definition_t td;

  damage_triggers = equipped_creature_deals_damage(
      player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE);
  if (damage_triggers != 0)
  {

    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    if (0 < damage_triggers)
    {
      do
      {
        instance->number_of_targets = '\0';
        result = can_target(&td);
        if ((result != 0) && (result = pick_target(&td, "ASHNODS_BATTLEGEAR"), result != 0))
        {
          add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                       COUNTER_P1_P1, 1);
        }
        proliferate(player, card);
        damage_triggers = damage_triggers + -1;
      } while (damage_triggers != 0);
    }
  }
  blue_protection = get_sleighted_protection(player, card, 0x1000);
  white_protection = get_sleighted_protection(player, card, 0x8000);
  return vanilla_equipment(player, card, event, 2, 2, 2, blue_protection | white_protection, 0);
}

/* ManalinkEh 2022: 0x233d1e0; original cards/modern_horizons.c:3456. */
int card_tectonic_reformation(int player, int card, event_t event)
{
  /* Tectonic Reformation	|1|R
   * Enchantment
   * Each land card in your hand has cycling |R.
   * Cycling |2 (|2, Discard this card: Draw a card.) */

  int selected_or_result;
  int library_effect_present;
  unsigned int amount;
  test_definition_t this_test;

  if ((uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND) < 3)
  {
    return us_cycling(player, card, event);
  }
  if (event == EVENT_CAN_ACTIVATE)
  {

    selected_or_result = check_battlefield_for_id(player, CARD_ID_NEW_PERSPECTIVES);
    if ((selected_or_result == 0) || (hand_count[player] < 7))
    {
      check_hand_for_type(player, card, 1);
    }
    else
    {
      if (check_hand_for_type(player, card, 1) != 0)
      {
        return 1;
      }
    }
    selected_or_result = has_mana(player, 4, 1);
  }
  else
  {
    if (event == EVENT_ACTIVATE)
    {

      new_default_test_definition(&this_test, 1, "Select a land card to cycle");
      selected_or_result = new_select_a_card(player, player, 4, 0, 3, -1, &this_test);
      library_effect_present = check_battlefield_for_id(player, CARD_ID_NEW_PERSPECTIVES);
      amount = 1;
      if (library_effect_present != 0)
      {
        amount = (unsigned int)(hand_count[player] < 7);
      }
      charge_mana(player, COLOR_RED, amount);
      if (spell_fizzled != 1)
      {
        discard_card(player, selected_or_result);
      }
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) && (spell_fizzled != 1))
    {
      draw_a_card(player);
    }
    selected_or_result = global_enchantment(player, card, event);
  }
  return selected_or_result;
}

/* ManalinkEh 2022: 0x2335650; original cards/modern_horizons.c:3488. */
int tempered_sliver_legacy(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;
  int target_player;

  if (((trigger_condition == TRIGGER_DEAL_DAMAGE) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      target_player = (int)instance->damage_target_player;
      result = instance->damage_target_card;
      instance = in_play(target_player, result);
      if ((instance != NULL) && (reason_for_trigger_controller == target_player))
      {
        if (event == EVENT_TRIGGER)
        {
          event_result = event_result | 2;
        }
        else if (event == EVENT_RESOLVE_TRIGGER)
        {
          add_counters(target_player, result, target_player, result, COUNTER_P1_P1, 1);
          kill_card(player, card, KILL_EXILE);
        }
      }
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

/* ManalinkEh 2022: 0x233d3a0; original cards/modern_horizons.c:3506. */
int card_tempered_sliver(int player, int card, event_t event)
{
  /* Tempered Sliver	|2|G
   * Creature - Sliver 2/2
   * Sliver creatures you control have "Whenever this creature deals combat damage to a player, put a +1/+1 counter on it." */

  int c;
  int sliver_player;
  card_instance_t *instance;

  if (event == EVENT_DEAL_DAMAGE)
  {

    instance = combat_damage_being_dealt(EVENT_DEAL_DAMAGE);
    if ((((instance != NULL) &&
          (sliver_player = (int)instance->damage_source_player, sliver_player == player)) &&
         (c = instance->damage_source_card, c != -1)) &&
        ((0 < instance->info_slot && (instance->damage_target_card == -1))))
    {
      if (check_special_flags(sliver_player, c, 0x40) == 0)
      {
        if (is_what(sliver_player, c, 2) != 0)
        {
          if (has_subtype(sliver_player, c, SUBTYPE_SLIVER) != 0)
          {
            if (is_humiliated(sliver_player, card, AL_STATIC) == 0)
            {
              if (is_humiliated(sliver_player, c, AL_TRIG) == 0)
              {
                create_targetted_legacy_effect(sliver_player, c, tempered_sliver_legacy,
                                               sliver_player, c);
              }
            }
          }
        }
      }
    }
  }
  return slivercycling(player, card, event);
}

/* ManalinkEh 2022: 0x233d4d0; original cards/modern_horizons.c:3529. */
int card_the_first_sliver(int player, int card, event_t event)
{
  /* The First Sliver	|W|U|B|R|G
   * Legendary Creature - Sliver 7/7
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.)
   * Sliver spells you cast have cascade. */

  card_instance_t *instance;
  int result;

  check_legend_rule(player, card, event);
  cascade(player, card, event);
  if (event == EVENT_CAST_SPELL)
  {
    instance = in_play(player, card);
    if ((instance != NULL) && (affected_card_controller == player))
    {
      result = has_subtype(player, affected_card, SUBTYPE_SLIVER);
      if (result != 0)
      {
        result = is_token(affected_card_controller, affected_card);
        if (result == 0)
        {
          result = check_special_flags(affected_card_controller, affected_card, 0x40000);
          if (result == 0)
          {
            result = is_humiliated(player, card, AL_TRIG);
            if (result == 0)
            {
              do_cascade(affected_card_controller, affected_card, -1);
            }
          }
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x233d5d0; original cards/modern_horizons.c:3552. */
int card_throatseeker(int player, int card, event_t event)
{
  /* Throatseeker	|2|B
   * Creature - Vampire Ninja 3/2
   * Unblocked attacking Ninjas you control have lifelink. */

  int result;
  if ((event == EVENT_ABILITIES) && (affected_card_controller == player))
  {
    result = has_subtype(affected_card_controller, affected_card, SUBTYPE_NINJA);
    if (result != 0)
    {
      result = is_unblocked(affected_card_controller, affected_card);
      if (result != 0)
      {
        lifelink(affected_card_controller, affected_card, EVENT_ABILITIES);
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x233d650; original cards/modern_horizons.c:3566. */
int card_throes_of_chaos(int player, int card, event_t event)
{
  /* Throes of Chaos	|3|R
   * Sorcery
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.)
   * Retrace (You may cast this card from your graveyard by discarding a land card in addition to paying its other costs.) */

  int result;

  if (event != EVENT_CAN_CAST)
  {
    cascade(player, card, event);
    if (event == EVENT_RESOLVE_SPELL)
    {
      kill_card(player, card, KILL_DESTROY);
    }
    result = retrace(player, card, event, 0x4f0a);
    return result;
  }
  result = basic_spell(player, card, EVENT_CAN_CAST);
  return result;
}
#endif

/* ManalinkEh 2022: 0x233d6d0; original cards/modern_horizons.c:3609. */
int card_tribute_mage(int player, int card, event_t event)
{
  /* Tribute Mage	|2|U
   * Creature - Human Wizard 2/2
   * When this creature enters, you may search your library for an artifact card with mana value 2, reveal that card, put it into your hand, then shuffle. */

  int result;
  test_definition_t this_test;
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  new_default_test_definition(&this_test, 0x40, "Select an artifact card with CMC 2");
  this_test.cmc = 2;
  this_test.cmc_flag = 0;
  new_global_tutor(player, player, 1, 1, 0, 1, &this_test);
  return 0;
}

/* ManalinkEh 2022: 0x233d790; original cards/modern_horizons.c:3692. */
int card_urza_lord_high_artificer(int player, int card, event_t event)
{
  /* Urza, Lord High Artificer	|2|U|U
   * Legendary Creature - Human Artificer 1/4
   * When Urza enters, create a 0/0 colorless Construct artifact creature token with "This token gets +1/+1 for each artifact you control."
   * Tap an untapped artifact you control: Add |U.
   * |5: Shuffle your library, then exile the top card. Until end of turn, you may play that card without paying its mana cost. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_DECK = 2
  };

  int choice;
  unsigned short exiled_csvid;
  int tap_available;
  card_instance_t *instance;
  token_generation_t token;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_token_definition(player, card, CARD_ID_CONSTRUCT, &token);
    token.tou = 0;
    token.pow = 0;
    token.special_infos = 0x42;
    generate_token(&token);
  }
  else
  {
    if (event == EVENT_COUNT_MANA)
    {
      return tap_a_permanent_you_control_for_mana(player, card, EVENT_COUNT_MANA, TYPE_ARTIFACT, 0,
                                                  COLOR_BLUE, 1);
    }
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (tap_a_permanent_you_control_for_mana(player, card, EVENT_CAN_ACTIVATE, TYPE_ARTIFACT, 0,
                                               COLOR_BLUE, 1) == 0)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 5, 0, 0, 0, 0, 0, 0,
                                         NULL, NULL);
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      instance = get_card_instance(player, card);
      instance->info_slot = 0;
      tap_available = tap_a_permanent_you_control_for_mana(player, card, EVENT_CAN_ACTIVATE,
                                                           TYPE_ARTIFACT, 0, COLOR_BLUE, 1);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                           "Tap an artifact for mana", tap_available, 1, "Cast from library",
                           generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 5, 0, 0,
                                                     0, 0, 0, 0, NULL, NULL),
                           5, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice != CHOICE_MANA)
      {
        if (choice != CHOICE_DECK)
        {
          return 0;
        }
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 5, 0, 0, 0, 0, 0, 0, NULL,
                                         NULL);
      }
      return tap_a_permanent_you_control_for_mana(player, card, EVENT_ACTIVATE, TYPE_ARTIFACT, 0,
                                                  COLOR_BLUE, 1);
    }
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (instance = get_card_instance(player, card), instance->info_slot == CHOICE_DECK))
  {

    shuffle(player);
    if (*deck_ptr[player] != -1)
    {

      exiled_csvid = cards_data[*deck_ptr[player]].id;
      rfg_card_in_deck(player, 0);
      create_may_play_card_from_exile_effect(player, card, player, (unsigned int)exiled_csvid,
                                             MPCFE_UNTIL_EOT | MPCFE_FOR_FREE);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x233dbb0; original cards/modern_horizons.c:3827. */
int card_weather_the_storm(int player, int card, event_t event)
{
  /* Weather the Storm	|1|G
   * Instant
   * You gain 3 life.
   * Storm (When you cast this spell, copy it for each spell cast before it this turn.) */

  if (event == EVENT_CAN_CAST)
    return basic_spell(player, card, event);

  if (event == EVENT_CAST_SPELL)
  {
    if (affect_me(player, card))
    {
      basic_spell(player, card, event);
      if (spell_fizzled != 1 && !check_special_flags(player, card, SF_NOT_CAST))
      {
        // The release rereads the storm count after each gain-life call.
        for (int copy = 0; copy < get_storm_count(); ++copy)
          gain_life(player, 3);
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    gain_life(player, 3);
    kill_card(player, card, KILL_DESTROY);
  }
  return 0;
}

/* ManalinkEh 2022: 0x233dca0; original cards/modern_horizons.c:3876. */
int card_winding_way(int player, int card, event_t event)
{
  /* Winding Way	|1|G
   * Sorcery
   * Choose creature or land. Reveal the top four cards of your library. Put all cards of the chosen type revealed this way into your hand and the rest into your graveyard. */

  int choice;
  int mana_available;
  int library_size_or_iid;
  int remaining_top_cards;

  if (event == EVENT_RESOLVE_SPELL)
  {

    mana_available = has_mana(player, 7, 1);
    choice = dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_NO_STORAGE, DLG_NO_CANCEL, "Creature", 1,
                         4, "Land", 1, 9 - mana_available, 0, 0, 0, 0);
    if (choice == 0)
    {
      spell_fizzled = 1;
    }
    else
    {

      library_size_or_iid = count_deck(player);
      remaining_top_cards = library_size_or_iid;
      if (4 < library_size_or_iid)
      {
        remaining_top_cards = 4;
      }
      if (library_size_or_iid != 0)
      {

        show_deck(0, deck_ptr[player], remaining_top_cards, "Here are the first 4 card of deck", 0,
                  gs_done_008b40e0);
        do
        {
          library_size_or_iid = *deck_ptr[player];
          if (is_what(-1, library_size_or_iid, (choice == 1) + 1) == 0)
          {
            mill(player, 1);
          }
          else
          {

            add_card_to_hand(player, library_size_or_iid);
            remove_card_from_deck(player, 0);
          }
          remaining_top_cards = remaining_top_cards + -1;
        } while (remaining_top_cards != 0);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x233de70; original cards/modern_horizons.c:3912. */
int card_winds_of_abandon(int player, int card, event_t event)
{
  /* Winds of Abandon	|1|W
   * Sorcery
   * Exile target creature you don't control. For each creature exiled this way, its controller searches their library for a basic land card. Those players put those cards onto the battlefield tapped, then shuffle.
   * Overload |4|W|W (You may cast this spell for its overload cost. If you do, change "target" in its text to "each.") */

  card_instance_t *instance;
  int exiled_count;
  unsigned int overload_mode;
  int opponent_or_choice;
  target_definition_t td;
  test_definition_t test;
  test_definition_t this_test;
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    instance = get_card_instance(player, card);
    if (generic_spell(player, card, EVENT_CAN_CAST, 0, NULL, NULL, 0, NULL) != 0)
    {
      if (instance->info_slot == 1)
      {
        return 1;
      }
      return can_target(&td);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = 1 - player;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        opponent_or_choice = check_special_flags2(player, card, -0x80000000);
        if ((opponent_or_choice != 0) ||
            (overload_mode = pay_overload(player, card, 4, 0, 0, 0, 0, 2), overload_mode == 1))
        {
          new_pick_target(&td, "Select target creature you don\'t control.", 0, 0x81);
          if (spell_fizzled == 1)
          {
            return 0;
          }
          overload_mode = 1;
        }
        instance->info_slot = instance->info_slot | overload_mode;
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    opponent_or_choice = 1 - player;
    td.allowed_controller = opponent_or_choice;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (((unsigned int)instance->info_slot & 2) == 0)
      {
        if (valid_target(&td) != 0)
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
          opponent_or_choice = instance->targets[0].player;
          if ((opponent_or_choice == 1) ||
              (opponent_or_choice = do_dialog(opponent_or_choice, player, card, -1, -1,
                                              " Search a basic land\n Pass", 0),
               opponent_or_choice == 0))
          {
            new_default_test_definition(&this_test, 1, "Select a basic land card.");
            this_test.subtype = 0x2000;
            opponent_or_choice = instance->targets[0].player;
            new_global_tutor(opponent_or_choice, opponent_or_choice, 1, 5, 0, 5, &this_test);
          }
        }
      }
      else
      {
        default_test_definition(&test, 2);
        exiled_count = new_manipulate_all(player, card, opponent_or_choice, &test, ACT_KILL_REMOVE);
        if ((player == 0) || (do_dialog(opponent_or_choice, player, card, -1, -1,
                                        " Search for basic lands\n Pass", 0) == 0))
        {
          new_default_test_definition(&this_test, 1, "Select a basic land card.");
          this_test.subtype = 0x2000;
          this_test.qty = (unsigned short)exiled_count;
          new_global_tutor(opponent_or_choice, opponent_or_choice, 1, 5, 0, 5, &this_test);
        }
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  return overload(player, card, event, 4, 0, 0, 0, 0, 2);
}
#endif

/* ManalinkEh 2022: 0x233e340; original cards/modern_horizons.c:4000. */
int card_wrenn_and_six(int player, int card, event_t event)
{
  /* Wrenn and Six	|R|G
   * Legendary Planeswalker - Wrenn (3)
   * +1: Return up to one target land card from your graveyard to your hand.
   * -1: ~ deals 1 damage to any target.
   * -7: You get an emblem with "Instant and sorcery cards in your graveyard have retrace." (You may cast instant and sorcery cards from your graveyard by discarding a land card in addition to paying their other costs.) */

  enum
  {
    CHOICE_LAND_AND_DRAW = 1,
    CHOICE_DAMAGE = 2,
    CHOICE_EMBLEM = 3
  };

  int c;
  int choice;
  card_instance_t *instance;
  int scan_player_or_result;
  int toughness_or_creature;
  int loyalty_or_priority;
  int saved_turn;
  uint32_t damage_priority;
  target_definition_t *damage_target;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    new_default_test_definition(&test, 1, "Select target land card.");
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      if ((player == 1) && (loyalty_or_priority = count_counters(1, card, COUNTER_LOYALTY),
                            scan_player_or_result = current_turn, 1 < loyalty_or_priority))
      {

        saved_turn = current_turn;
        damage_priority = 1;
        c = active_cards_count[current_turn] + -1;
        if (c < 0)
        {
          damage_priority = 1;
        }
        else
        {
          do
          {
            if (((scan_player_or_result != 1) &&
                 (instance = in_play(scan_player_or_result, c), instance != NULL)) &&
                (toughness_or_creature = is_what(scan_player_or_result, c, 2),
                 toughness_or_creature != 0))
            {
              toughness_or_creature = get_toughness(scan_player_or_result, c);
              instance = get_card_instance(scan_player_or_result, c);
              if (toughness_or_creature - (unsigned int)instance->damage_on_card == 1)
              {
                saved_turn = current_turn;
                damage_priority = 0x19;
                break;
              }
            }
            c = c + -1;
          } while (c != -1);
          saved_turn = current_turn;
        }
        scan_player_or_result = 1 - saved_turn;
        c = active_cards_count[scan_player_or_result] + -1;
        current_turn = saved_turn;
        if (-1 < c)
        {
          do
          {
            if (((scan_player_or_result != 1) &&
                 (instance = in_play(scan_player_or_result, c), instance != NULL)) &&
                (toughness_or_creature = is_what(scan_player_or_result, c, 2),
                 toughness_or_creature != 0))
            {
              toughness_or_creature = get_toughness(scan_player_or_result, c);
              instance = get_card_instance(scan_player_or_result, c);
              if (toughness_or_creature - (unsigned int)instance->damage_on_card == 1)
              {
                damage_priority = 0x19;
                break;
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
      scan_player_or_result = check_battlefield_for_id(player, 0x4f2a);
      loyalty_or_priority = -100;
      if (scan_player_or_result == 0)
      {
        scan_player_or_result = count_counters(player, card, COUNTER_LOYALTY);
        loyalty_or_priority = ((7 < scan_player_or_result) ? 30U : 10U);
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                           "Return land from grave", 1, 10, 1, "Deal 1 damage", can_target(&td),
                           damage_priority, 0xffffffff, "Emblem", 1, loyalty_or_priority,
                           0xfffffff9, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_LAND_AND_DRAW)
      {
        scan_player_or_result = new_special_count_grave(player, &test);
        if ((scan_player_or_result != 0) &&
            (scan_player_or_result = graveyard_has_shroud(player), scan_player_or_result == 0))
        {
          select_target_from_grave_source(player, card, player, SFG_0, 1, &test, 0);
        }
      }
      else if (choice == CHOICE_DAMAGE)
      {
        pick_target(&td, "TARGET_ANY");
      }
      return planeswalker(player, card, event, 3);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 3);
    new_default_test_definition(&test, 1, "Select target land card.");
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
  }
  damage_target = &td;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    scan_player_or_result = instance->info_slot;
    if (scan_player_or_result == 1)
    {

      scan_player_or_result = validate_target_from_grave_source(player, card, player, 0);
      if (scan_player_or_result != -1)
      {
        from_grave_to_hand(player, scan_player_or_result, 1);
      }
    }
    else if (scan_player_or_result == 2)
    {
      if (valid_target(damage_target) != 0)
      {
        damage_target0(player, card, 1);
      }
    }
    else if (scan_player_or_result == 3)
    {
      generate_token_by_id(player, card, CARD_ID_WRENN_AND_SIX_EMBLEM);
    }
  }
  return planeswalker(player, card, event, 3);
}

/* ManalinkEh 2022: 0x233e8b0; original cards/modern_horizons.c:4088. */
/* DWARF gives positions[500] and test_hand the same stack location in
 * different lexical scopes. Keep them as separate source objects. */
int card_wrenn_and_six_emblem(int player, int card, event_t event)
{
  /* Wrenn and Six Emblem	""
   * Emblem - Wrenn
   * Instant and sorcery cards in your graveyard have retrace. (You may cast instant and sorcery cards from your graveyard by discarding a land card in addition to paying their other costs.) */

  int *graveyard;
  int grave_index_or_selected;
  int playable_or_result;
  int scan_index_or_available;
  int activation_result;
  test_definition_t test;
  test_definition_t this_test;
  int iids[500];
  int positions[500];
  test_definition_t test_hand;
  if (event == EVENT_CAN_ACTIVATE)
  {
    default_test_definition(&test_hand, 1);
    test_hand.zone = 0x100;
    if (check_battlefield_for_special_card(player, card, player, 0, &test_hand) != 0)
    {
      graveyard = graveyard_ptr_mutable[player];
      grave_index_or_selected = count_graveyard(player);
      if (-1 < grave_index_or_selected)
      {
        activation_result = 0;
        do
        {
          playable_or_result = graveyard[grave_index_or_selected];
          if (((playable_or_result != -1) &&
               (scan_index_or_available = is_what(-1, playable_or_result, 0x38),
                scan_index_or_available != 0)) &&
              (scan_index_or_available =
                   has_mana_to_cast_iid(player, EVENT_CAN_ACTIVATE, playable_or_result),
               scan_index_or_available != 0))
          {
            playable_or_result =
                can_legally_play_iid_now(player, playable_or_result, EVENT_CAN_ACTIVATE);
            if (playable_or_result == 99)
            {
              return 99;
            }
            if (playable_or_result != 0)
            {
              activation_result = playable_or_result;
            }
          }
          grave_index_or_selected = grave_index_or_selected + -1;
        } while (grave_index_or_selected != -1);
        return activation_result;
      }
    }
  }
  else
  {
    if (event != EVENT_ACTIVATE)
    {
      return 0;
    }
    playable_or_result = 0;
    graveyard = graveyard_ptr_mutable[player];
    grave_index_or_selected = *graveyard;
    if (grave_index_or_selected != -1)
    {
      scan_index_or_available = 0;
      do
      {
        if ((((cards_data[grave_index_or_selected].type & 0x38) != 0) &&
             ((cards_data[grave_index_or_selected].type & 2) == 0)) &&
            ((grave_index_or_selected =
                  has_mana_to_cast_iid(player, EVENT_ACTIVATE, grave_index_or_selected),
              grave_index_or_selected != 0 &&
                  ((grave_index_or_selected =
                        is_suspend_only_card(-1, graveyard[scan_index_or_available]),
                    grave_index_or_selected == 0 &&
                        (grave_index_or_selected = can_legally_play_iid_now(
                             player, graveyard[scan_index_or_available], EVENT_ACTIVATE),
                         grave_index_or_selected != 0))))))
        {
          grave_index_or_selected = graveyard[scan_index_or_available];
          positions[playable_or_result] = scan_index_or_available;
          iids[playable_or_result] = grave_index_or_selected;
          playable_or_result = playable_or_result + 1;
        }
        scan_index_or_available = scan_index_or_available + 1;
        grave_index_or_selected = graveyard[scan_index_or_available];
      } while (grave_index_or_selected != -1);
    }
    default_test_definition(&test, 0);
    grave_index_or_selected =
        select_card_from_zone(player, player, iids, playable_or_result, 0, 1, -1, &test);
    if ((grave_index_or_selected == -1) ||
        (playable_or_result =
             charge_mana_from_id(player, -1, EVENT_ACTIVATE,
                                 (unsigned int)cards_data[iids[grave_index_or_selected]].id),
         playable_or_result == 0))
    {
      spell_fizzled = 1;
    }
    else
    {
      new_default_test_definition(&this_test, 1, "Select a land card to discard.");
      playable_or_result = new_select_a_card(player, player, 4, 0, 3, -1, &this_test);
      if (playable_or_result != -1)
      {
        discard_card(player, playable_or_result);
        play_card_in_grave_for_free(player, player, positions[grave_index_or_selected]);
        cant_be_responded_to = 1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x233ebe0; original cards/modern_horizons.c:4161. */
int card_yawgmoth_thran_physician(int player, int card, event_t event)
{
  /* Yawgmoth, Thran Physician	|2|B|B
   * Legendary Creature - Human Cleric 2/4
   * Protection from Humans
   * Pay 1 life, Sacrifice another creature: Put a -1/-1 counter on up to one target creature and draw a card.
   * |B|B, Discard a card: Proliferate. (Choose any number of permanents and/or players, then give each another counter of each kind already there.) */

  enum
  {
    CHOICE_SAC = 1,
    CHOICE_DISCARD = 2
  };

  int choice;
  card_instance_t *instance;
  int available_or_target_card;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  protection_from_subtype(player, card, event, SUBTYPE_HUMAN);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    new_default_test_definition(&test, 2, "Select creature to sacrifice");
    test.not_me = 1;
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE,
                                                           GAA_NONE, 0, 0, 0, 0, 0, 0, 1, NULL,
                                                           NULL, TYPE_NONE, &test, SARV_0) == 0)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x20, 0, 2, 0, 0, 0, 0,
                                         0, NULL, NULL);
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->info_slot = 0;
      available_or_target_card = generic_activated_ability_with_sacrifice_as_cost(
          player, card, EVENT_CAN_ACTIVATE, GAA_NONE, 0, 0, 0, 0, 0, 0, 1, NULL, NULL, TYPE_NONE,
          &test, SARV_0);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                           "-1/-1 counter & draw", available_or_target_card, 1, "Proliferate",
                           generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x20, 0, 2,
                                                     0, 0, 0, 0, 0, NULL, NULL),
                           1, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice != CHOICE_SAC)
      {
        if (choice != CHOICE_DISCARD)
        {
          return 0;
        }
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 0x20, 0, 2, 0, 0, 0, 0, 0,
                                         NULL, NULL);
      }
      generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_ACTIVATE, GAA_NONE, 0, 0,
                                                       0, 0, 0, 0, 1, NULL, NULL, TYPE_NONE, &test,
                                                       SARV_0);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      if (can_target(&td) == 0)
      {
        return 0;
      }
      new_pick_target(&td, "TARGET_CREATURE", 0, 0);
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    new_default_test_definition(&test, 2, "Select creature to sacrifice");
    test.not_me = 1;
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 1;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->info_slot == CHOICE_SAC)
    {
      available_or_target_card = instance->targets[0].card;
      if (available_or_target_card != -1)
      {
        add_counters(player, card, instance->targets[0].player, available_or_target_card,
                     COUNTER_M1_M1, 1);
      }
      draw_a_card(player);
    }
    else if (instance->info_slot == CHOICE_DISCARD)
    {
      proliferate(player, card);
      return 0;
    }
  }

  return 0;
}
