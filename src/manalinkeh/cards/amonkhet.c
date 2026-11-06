#include "manalink.h"

/* Canonical recovered helper declarations. */
static int embalm(int player, int card, event_t event, card_id_t cid, int x, int b, int u,
                  int g, int r, int w);

int card_revoke_existence(int player, int card, event_t event);

static int effect_insult(int player, int card, event_t event);
int aftermath(int player, int card, event_t event, int colorless, int black, int blue, int green,
              int red, int white, int interrupt);

static int iid_has_cycling(int iid, intptr_t me, int player, int card);

static int gate_to_the_afterlife_effect(int player, int card, event_t event);

#ifndef SHANDALAR

int card_approach_of_the_second_sun(int player, int card, event_t event);

#endif

#ifndef SHANDALAR
static int can_pay_aftermath(int player, int iid, event_t event, int colorless,
                             int black, int blue, int green, int red, int white)
{
  int cost = get_updated_casting_cost(player, -1, iid, event, colorless);
  return has_mana_multi(player, cost, black, blue, green, red, white);
}
#endif

#ifndef SHANDALAR
static int pay_aftermath(int player, int card, int iid, event_t event, int colorless,
                         int black, int blue, int green, int red, int white)
{
  int cost = get_updated_casting_cost(player, -1, iid, event, colorless);
  if (colorless == -1 && cost == 0)
    charge_mana_multi(player, -1, black, blue, green, red, white);
  else
  {
    charge_mana_multi(player, cost, black, blue, green, red, white);
    if (colorless == -1 && cost != 0)
      charge_mana(player, COLOR_COLORLESS, -1);
  }
  card_instance_t *instance = get_card_instance(player, card);
  int success = spell_fizzled != 1;
  instance->targets[1].card = x_value;
  return success;
}
#endif

/* ManalinkEh 2022: 0x22d75d0; original cards/amonkhet.c:26. */
int aftermath(int player, int card, event_t event, int colorless, int black, int blue, int green,
              int red, int white, int interrupt)
{
  card_instance_t *instance;
  int result;
  bool paid;

  instance = get_card_instance(player, card);
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      instance->info_slot = 0;
      result = is_x_spell(player, card);
      if (result != 0)
      {
        instance->targets[1].card = x_value;
      }
      if (spell_fizzled == 1)
      {
        instance->info_slot = 0;
      }
      else
      {
        result = get_aftermath(player, card);
        if (result == 0)
        {
          instance->info_slot = 1;
          call_card_function(player, card, EVENT_PLAY_FIRST_HALF);
          if (spell_fizzled == 1)
          {
            instance->info_slot = 0;
            return 0;
          }
        }
        else
        {
          instance->info_slot = 2;
          call_card_function(player, card, EVENT_PLAY_SECOND_HALF);
          if (spell_fizzled == 1)
          {
            instance->info_slot = 0;
            return 0;
          }
        }
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = get_id(player, card);
    result = get_internal_card_id_from_csv_id(result);
    result = get_updated_casting_cost(player, -1, result, EVENT_GRAVEYARD_ABILITY, colorless);
    result = has_mana_multi(player, result, black, blue, green, red, white);
    if (result != 0)
    {
      if (interrupt < 1)
      {
        return 2;
      }
      return 0x402;
    }
  }
  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    result = get_id(player, card);
    result = get_internal_card_id_from_csv_id(result);
    result = get_updated_casting_cost(player, -1, result, EVENT_PAY_FLASHBACK_COSTS, colorless);
    if ((colorless != -1) || (result != 0))
    {
      charge_mana_multi(player, result, black, blue, green, red, white);
      if ((colorless == -1) && (result != 0))
      {
        charge_mana(player, COLOR_COLORLESS, -1);
      }
    }
    else
    {
      charge_mana_multi(player, -1, black, blue, green, red, white);
    }
    instance = get_card_instance(player, card);
    paid = spell_fizzled != 1;
    instance->targets[1].card = x_value;
    if (paid)
    {
      return 2;
    }
  }
  return 0;
}

static void
cb_embalm(token_generation_t* token, int c, int toknum)
{
  set_special_flags3(token->t_player, c, SF3_EMBALMED_OR_ETERNALIZED);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d74b0; original cards/amonkhet.c:80. */
static int embalm(int player, int card, event_t event, card_id_t cid, int x, int b, int u,
                  int g, int r, int w)
{
  int result;
  token_generation_t token;
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = has_mana_multi(player, x, b, u, g, r, w);
    if ((result != 0) &&
        (result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY), result != 0))
    {
      return GA_EMBALM | GA_BASIC;
    }
  }
  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    charge_mana_multi(player, x, b, u, g, r, w);
    if (spell_fizzled != 1)
    {
      return GAPAID_EXILE;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATED_GRAVEYARD_ABILITY)
  {
    default_token_definition(player, card, cid, &token);
    token.color_forced = ' ';
    token.no_sleight = '\x01';
    token.special_flags3 = SF3_EMBALMED_OR_ETERNALIZED;
    generate_token(&token);
  }
  return (unsigned int)(event == EVENT_CAN_CAST);
}
#endif

static void
vigilant_white_warriors(int player, int card, int num)
{
  token_generation_t token;
  default_token_definition(player, card, CARD_ID_WARRIOR, &token);
  token.qty = num;
  token.s_key_plus = SP_KEYWORD_VIGILANCE;
  token.color_forced = COLOR_TEST_WHITE;
  generate_token(&token);
}

int card_angel_of_sanctions(int player, int card, event_t event)
{
  /* Angel of Sanctions	|3|W|W
   * Creature - Angel 3/4
   * Flying
   * When ~ enters the battlefield, you may exile target nonland permanent an opponent controls until ~ leaves the battlefield.
   * Embalm |5|W */

  return_from_oblivion(player, card, event);

  etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

  if (event == EVENT_ETB_ABILITY)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_PERMANENT);
	  td.illegal_type = TYPE_LAND;
	  td.allowed_controller = 1-player;

	  if (can_target(&td)
		  && new_pick_target(&td, "Select target nonland permanent an opponent controls.", 0, GS_LITERAL_PROMPT))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  obliviation(player, card, inst->targets[0].player, inst->targets[0].card);
		  inst->number_of_targets = 0;
		}
	}

  return embalm(player, card, event, CARD_ID_ANGEL_OF_SANCTIONS, 5, 0, 0, 0, 0, 1);
}

/* Anointed Procession	|3|W	=>innistrad.c:card_parallel_lives 0x2003f31
 * Enchantment
 * If an effect would create one or more tokens under your control, it creates twice that many of those tokens instead. */

int card_anointer_priest(int player, int card, event_t event)
{
  /* Anointer Priest	|1|W
   * Creature - Human Cleric 1/3
   * Whenever a creature token enters the battlefield under your control, you gain 1 life.
   * Embalm |3|W */

  if (specific_cip(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE,F1_IS_TOKEN, 0,0, 0,0, 0,0,
				   -1,0))
	gain_life(player, 1);

  return embalm(player, card, event, CARD_ID_ANOINTER_PRIEST, 3, 0, 0, 0, 0, 1);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d7ab0; original cards/amonkhet.c:175. */
int card_approach_of_the_second_sun(int player, int card, event_t event)
{
  /* Approach of the Second Sun	|6|W
   * Sorcery
   * If this spell was cast from your hand and you've cast another spell named ~ this game, you win the game. Otherwise, put ~ into its owner's library seventh from the top and you gain 7 life. */

  int result;
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
      {
        return 0;
      }
      if ((player_bits[player] & PB_CAST_APPROACH_OF_THE_SECOND_SUN1) != 0)
      {
        player_bits[player] = player_bits[player] | PB_CAST_APPROACH_OF_THE_SECOND_SUN2;
      }
      result = check_special_flags2(player, card, -0x80000000);
      if ((result == 0) && ((player_bits[player] & PB_CAST_APPROACH_OF_THE_SECOND_SUN1) == 0))
      {
        player_bits[player] = player_bits[player] | PB_CAST_APPROACH_OF_THE_SECOND_SUN1;
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      if (((player_bits[player] & PB_CAST_APPROACH_OF_THE_SECOND_SUN2) == 0) ||
          (result = not_played_from_hand(player, card), result != 0))
      {
        gain_life(player, 7);
        put_permanent_under_the_first_x_card_of_its_owners_library(player, card, 6);
      }
      else
      {
        lose_the_game(1 - player);
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
#endif

// Aven Mindcensor => future_sight.c

int card_binding_mummy(int player, int card, event_t event)
{
  /* Binding Mummy	|1|W
   * Creature - Zombie 2/2
   * Whenever another Zombie enters the battlefield under your control, you may tap target artifact or creature. */

  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card)
	  && player == reason_for_trigger_controller)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_PERMANENT, "");
	  test.subtype = SUBTYPE_ZOMBIE;
	  test.not_me = 1;

	  etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

  if (event == EVENT_ETB_ABILITY)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_CREATURE);
	  td.preferred_controller = 1-player;

	  card_instance_t* inst = get_card_instance(player, card);

	  if (can_target(&td) && pick_target(&td, "PUTREFY"))
		tap_card(inst->targets[0].player, inst->targets[0].card);
	  inst->number_of_targets = 0;
	  cancel = 0;
	}
  return 0;
}

int card_cartouche_of_solidarity(int player, int card, event_t event)
{
  /* Cartouche of Solidarity	|W
   * Enchantment - Aura Cartouche
   * Enchant creature you control
   * When ~ enters the battlefield, create a 1/1 |Swhite Warrior creature token with vigilance.
   * Enchanted creature gets +1/+1 and has first strike. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
    vigilant_white_warriors(player, card, 1);
  if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS)
    return 0;
  attached_gets_pt(player, card, event, 1, 1);
  attached_gets_kw(player, card, event, KEYWORD_FIRST_STRIKE, 0);
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = player;
  return aura(player, card, event, &td, "TARGET_CREATURE_YOU_CONTROL");
}

int card_cast_out(int player, int card, event_t event)
{
  /* Cast Out	|3|W
   * Enchantment
   * Flash
   * When ~ enters the battlefield, exile target nonland permanent an opponent controls until ~ leaves the battlefield.
   * Cycling |W */

  if (IS_ACTIVATING_FROM_HAND(event))
	return cycling(player, card, event, MANACOST_W(1));

  return_from_oblivion(player, card, event);

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

  if (event == EVENT_ETB_ABILITY)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_PERMANENT);
	  td.illegal_type = TYPE_LAND;
	  td.allowed_controller = 1-player;
	  td.allow_cancel = 0;

	  if (can_target(&td)
		  && new_pick_target(&td, "Select target nonland permanent an opponent controls.", 0, GS_LITERAL_PROMPT))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  obliviation(player, card, inst->targets[0].player, inst->targets[0].card);
		  inst->number_of_targets = 0;
		}
	}

  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x22d8020; original cards/amonkhet.c:303. */
int card_compulsory_rest(int player, int card, event_t event)
{
  /* Compulsory Rest	|1|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature can't attack or block.
   * Enchanted creature has "|2, Sacrifice this creature: You gain 2 life." */

  int result;
  card_instance_t *instance;

  get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    gain_life(instance->targets[9].player, 2);
  }
  else if ((event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE) || (event == EVENT_CLEANUP))
  {
    instance = in_play(player, card);
    if ((instance != NULL) && (instance->damage_target_player == player))
    {
      result = aura_granting_activated_ability(player, card, event, player, 8, 2, 0, 0, 0, 0, 0, 0,
                                               NULL, NULL);
      return result;
    }
  }
  result = card_pacifism(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22d8130; original cards/amonkhet.c:320. */
int card_devoted_crop_mate(int player, int card, event_t event)
{
  /* Devoted Crop-Mate	|2|W
   * Creature - Human Warrior 3/2
   * You may exert ~ as it attacks. When you do, return target creature card with converted mana cost 2 or less from your graveyard to the battlefield. */

  int result;
  test_definition_t this_test;

  result = duh_mode(player);
  dat_ability(player, card, event, (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
  if (event == EVENT_DAT_ABILITY)
  {

    result = does_not_untap_effect(player, card, player, card, 0, 1);
    if (result != 0)
    {
      new_default_test_definition(&this_test, 2, "Select a creature card with cmc 2 or less.");
      this_test.cmc = 3;
      this_test.cmc_flag = 3;
      result = new_special_count_grave(player, &this_test);
      if (result != 0)
      {
        new_global_tutor(player, player, 2, 4, 1, 1, &this_test);
      }
    }
  }
  return 0;
}

int card_djerus_resolve(int player, int card, event_t event)
{
  /* Djeru's Resolve	|W
   * Instant
   * Untap target creature. Prevent all damage that would be dealt to it this turn.
   * Cycling |2 */

  if (IS_ACTIVATING_FROM_HAND(event))
	return cycling(player, card, event, MANACOST_X(2));

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  untap_card(inst->targets[0].player, inst->targets[0].card);
		  prevent_all_damage_to_target(player, card, inst->targets[0].player, inst->targets[0].card, 1);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

/* Fan Bearer	|W	=>shadowmoor.c:card_trip_noose 0x2007ebf
 * Creature - Zombie 1/2
 * |2, |T: Tap target creature. */

int card_forsake_the_worldly(int player, int card, event_t event)
{
  /* Forsake the Worldly	|2|W
   * Instant
   * Exile target artifact or enchantment.
   * Cycling |2 */

  if (IS_ACTIVATING_FROM_HAND(event))
	return cycling(player, card, event, MANACOST_X(2));

  return card_revoke_existence(player, card, event);
}

/* ManalinkEh 2022: 0x22d7370; original cards/amonkhet.c:392. */
static int effect_prevent_damage_from_until_my_next_turn(int player, int card, event_t event)
{
  card_instance_t *damage;
  card_instance_t *instance;

  damage = damage_being_prevented(event);
  if (damage != NULL)
  {
    instance = get_card_instance(player, card);
    if (((instance != NULL) && (damage->damage_source_card == instance->damage_target_card)) &&
        (damage->damage_source_player == instance->damage_target_player))
    {
      prevent_damage(affected_card_controller, affected_card, damage->info_slot);
    }
  }
  if ((event == EVENT_BEGIN_TURN) && (player == current_turn))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

// Ancient Crab => vanilla, first printed in OGW

// Cancel => unlimited.c:card_counterspell(), first printed in TSP

// Essence Scatter => legends.c:card_remove_soul(), first printed in M10

// Gravedigger => portal_1_2_3k.c

// Unburden => scourge.c

// Brute Strength => oath_of_the_gatewatch.c

// Fling => m11.c

// Magma Spray => shards_of_alara.c

// Tormenting Voice => khans_of_tarkir.c

// Giant Spider => vanilla, first printed in A

// Spidery Grasp => innistrad.c

// Evolving Wilds => m11.c:card_terramorphic_expanse(), first printed in ROE

// Stone Quarry => produce_mana.c:mana_producer_tapped(), first printed in SOI

// Foul Orchard => produce_mana.c:mana_producer_tapped(), first printed in SOI

// Cinder Barrens => produce_mana.c:mana_producer_tapped(), first printed in OGW

// Forsaken Sanctuary => produce_mana.c:mana_producer_tapped(), first printed in SOI

// Highland Lake => produce_mana.c:mana_producer_tapped(), first printed in SOI

// Meandering River => produce_mana.c:mana_producer_tapped(), first printed in OGW

// Submerged Boneyard => produce_mana.c:mana_producer_tapped(), first printed in OGW

// Timber Gorge => produce_mana.c:mana_producer_tapped(), first printed in OGW

// Tranquil Expanse => produce_mana.c:mana_producer_tapped(), first printed in OGW

// Woodland Stream => produce_mana.c:mana_producer_tapped(), first printed in SOI

/* ManalinkEh 2022: 0x22d8470; original cards/amonkhet.c:409. */
int card_gideon_of_the_trials_emblem(int player, int card, event_t event)
{
  /* Gideon of the Trials Emblem	""
   * Emblem - Gideon
   * As long as you control a Gideon planeswalker, you can't lose the game and your opponents can't win the game. */

  int result;
  result = check_battlefield_for_subtype(player, 0x100, 0x7005);
  if (result == 0)
  {
    return 0;
  }
  cannot_lose_the_game(player, card, event, player);
  return 0;
}

/* ManalinkEh 2022: 0x22d84d0; original cards/amonkhet.c:420. */
int card_gideon_of_the_trials(int player, int card, event_t event)
{
  /* Gideon of the Trials	|1|W|W
   * Legendary Planeswalker - Gideon (3)
   * +1: Until your next turn, prevent all damage target permanent would deal.
   * 0: Until end of turn, ~ becomes a 4/4 Human Soldier creature with indestructible that's still a planeswalker. Prevent all damage that would be dealt to him this turn.
   * 0: You get an emblem with "As long as you control a ~ planeswalker, you can't lose the game and your opponents can't win the game." */

  enum
  {
    CHOICE_PREVENT_DAMAGE = 1,
    CHOICE_ANIMATE = 2,
    CHOICE_EMBLEM = 3
  };

  int p;
  int mode_or_value;
  card_instance_t *instance;
  int loyalty_or_source_card;
  int emblem_or_power;
  bool activation_event;
  target_definition_t td;

  check_legend_rule(player, card, event);
  activation_event = event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE;
  if ((event == EVENT_RESOLVE_ACTIVATION) || (activation_event))
  {

    instance = get_card_instance(player, card);
    loyalty_or_source_card = count_counters(player, card, COUNTER_LOYALTY);
    default_target_definition(player, card, &td, 0x47);
    td.preferred_controller = 1 - player;
    emblem_or_power = check_battlefield_for_id(player, 0x4214);
    mode_or_value = -1;
    if (emblem_or_power == 0)
    {
      mode_or_value = loyalty_or_source_card * 3;
    }
    emblem_or_power = get_highest_power(1 - player);
    mode_or_value = dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, "Prevent damage",
                                can_target(&td), (emblem_or_power * 2 - loyalty_or_source_card) + 3,
                                1, "Animate", 1, loyalty_or_source_card * 2, 0, "Gideon's emblem",
                                1, mode_or_value, 0, 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (mode_or_value == 0)
      {
        return 0;
      }
    }
    else if (activation_event)
    {
      instance->number_of_targets = '\0';
      if (mode_or_value == CHOICE_PREVENT_DAMAGE)
      {
        pick_target(&td, "TARGET_PERMANENT");
      }
    }
    else if (mode_or_value == CHOICE_ANIMATE)
    {
      p = instance->parent_controller;
      loyalty_or_source_card = instance->parent_card;
      instance = in_play(p, loyalty_or_source_card);
      if (instance != NULL)
      {
        instance = get_card_instance(p, prevent_all_damage_to_target(p, loyalty_or_source_card, p,
                                                                     loyalty_or_source_card, 1));
        instance->token_status = instance->token_status | 0x10000;
        alternate_legacy_text(3, p,
                              animate_self(p, loyalty_or_source_card, 4, 4, KEYWORD_0,
                                           SP_KEYWORD_INDESTRUCTIBLE, COLOR_TEST_0, 0));
      }
    }
    else if (mode_or_value == CHOICE_EMBLEM)
    {
      generate_reserved_token_by_id(player, CARD_ID_GIDEON_OF_THE_TRIALS_EMBLEM);
    }
    else
    {

      if ((mode_or_value == CHOICE_PREVENT_DAMAGE) && (mode_or_value = valid_target(&td), mode_or_value != 0))
      {
        mode_or_value = create_targetted_legacy_effect(
            player, card, effect_prevent_damage_from_until_my_next_turn,
            instance->targets[0].player, instance->targets[0].card);
        alternate_legacy_text(1, instance->parent_controller, mode_or_value);
        if (((ai_is_speculating == 1) ||
             ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
            (mode_or_value = is_what(instance->targets[0].player, instance->targets[0].card, 2),
             mode_or_value != 0))
        {
          mode_or_value = get_power(instance->targets[0].player, instance->targets[0].card);
          ai_modifier = ai_modifier + mode_or_value * 0xc;
        }
      }
    }
  }
  has_subtype_if_animated_self(player, card, event, SUBTYPE_HUMAN);
  has_subtype_if_animated_self(player, card, event, SUBTYPE_SOLDIER);
  return planeswalker(player, card, event, 3);
}

/* Gideon's Intervention	|2|W|W
 * Enchantment
 * As ~ enters the battlefield, choose a card name.
 * Your opponents can't cast spells with the chosen name.
 * Prevent all damage that would be dealt to you and permanents you control by sources with the chosen name. */

/* ManalinkEh 2022: 0x22d88a0; original cards/amonkhet.c:506. */
int card_glory_bound_initiate(int player, int card, event_t event)
{
  /* Glory-Bound Initiate	|1|W
   * Creature - Human Warrior 3/1
   * You may exert ~ as it attacks. When you do, it gets +1/+3 and gains lifelink until end of turn. */

  int result;

  result = duh_mode(player);
  dat_ability(player, card, event, (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
  if (event == EVENT_DAT_ABILITY)
  {

    result = does_not_untap_effect(player, card, player, card, 0, 1);
    result = alternate_legacy_text(2, player, result);
    if (result != 0)
    {
      result = pump_ability_until_eot(player, card, player, card, 1, 3, 0, 2);
      alternate_legacy_text(1, player, result);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22d8990; original cards/amonkhet.c:522. */
int card_gust_walker(int player, int card, event_t event)
{
  /* Gust Walker	|1|W
   * Creature - Human Wizard 2/2
   * You may exert ~ as it attacks. When you do, it gets +1/+1 and gains flying until end of turn. */

  int result;

  result = duh_mode(player);
  dat_ability(player, card, event, (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
  if (event == EVENT_DAT_ABILITY)
  {

    result = does_not_untap_effect(player, card, player, card, 0, 1);
    result = alternate_legacy_text(2, player, result);
    if (result != 0)
    {
      result = pump_ability_until_eot(player, card, player, card, 1, 1, 0x20, 0);
      alternate_legacy_text(1, player, result);
    }
  }
  return 0;
}

// Impeccable Timing => kaladesh.c

int card_in_oketras_name(int player, int card, event_t event)
{
  /* In Oketra's Name	|1|W
   * Instant
   * Zombies you control get +2/+1 until end of turn. Other creatures you control get +1/+1 until end of turn. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.subtype = SUBTYPE_ZOMBIE;
	  pump_creatures_until_eot(player, card, player, 1, 2,1, 0,0, &test);

	  test.subtype_flag = DOESNT_MATCH;
	  pump_creatures_until_eot(player, card, player, 2, 1,1, 0,0, &test);

	  kill_card(player, card, KILL_DESTROY);
	}

  return basic_spell(player, card, event);
}

// Mighty Leap => m11.c

int card_oketra_the_true(int player, int card, event_t event)
{
  /* Oketra the True	|3|W
   * Legendary Creature - God 3/6
   * Double strike, indestructible
   * ~ can't attack or block unless you control at least three other creatures.
   * |3|W: Create a 1/1 |Swhite Warrior creature token with vigilance. */

  check_legend_rule(player, card, event);

  indestructible(player, card, event);

  if (event == EVENT_ATTACK_LEGALITY && affect_me(player, card)
	  && !is_humiliated(player, card, AL_STATIC)
	  && count_permanents_by_type(player, TYPE_CREATURE) <= 3)
	event_result = 1;

  if (event == EVENT_ABILITIES && affect_me(player, card)
	  && !is_humiliated(player, card, AL_STATIC)
	  && count_permanents_by_type(player, TYPE_CREATURE) <= 3)
	cannot_block(player, card, event);

  if (event == EVENT_RESOLVE_ACTIVATION)
	vigilant_white_warriors(player, card, 1);

  return generic_activated_ability(player, card, event, GAA_NONE, MANACOST_XW(3, 1), 0, NULL, NULL);
}

int card_oketras_attendant(int player, int card, event_t event)
{
  /* Oketra's Attendant	|3|W|W
   * Creature - Bird Soldier 3/3
   * Flying
   * Cycling |2
   * Embalm |3|W|W */

  if (IS_ACTIVATING_FROM_HAND(event))
	return cycling(player, card, event, MANACOST_X(2));

  return embalm(player, card, event, CARD_ID_OKETRAS_ATTENDANT, 3, 0, 0, 0, 0, 2);
}

int card_protection_of_the_hekma(int player, int card, event_t event)
{
  /* Protection of the Hekma	|4|W
   * Enchantment
   * If a source an opponent controls would deal damage to you, prevent 1 of that damage. */

  card_instance_t* damage = damage_being_prevented(event);
  if (damage && damage->damage_target_player == player && damage->damage_target_card == -1
	  && damage->damage_source_player == 1-player
	  && !is_humiliated(player, card, AL_STATIC))
	prevent_damage(affected_card_controller, affected_card, 1);

  return global_enchantment(player, card, event);
}

int card_regal_caracal(int player, int card, event_t event)
{
  /* Regal Caracal	|3|W|W
   * Creature - Cat 3/3
   * Other Cats you control get +1/+1 and have lifelink.
   * When ~ enters the battlefield, create two 1/1 |Swhite Cat creature tokens with lifelink. */

  boost_subtype(player, card, event, SUBTYPE_CAT, 1,1, 0,SP_KEYWORD_LIFELINK, BCT_CONTROLLER_ONLY);

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

  if (event == EVENT_ETB_ABILITY)
	{
	  token_generation_t token;
	  default_token_definition(player, card, CARD_ID_CAT, &token);
	  token.qty = 2;
	  token.pow = 1;
	  token.tou = 1;
	  token.s_key_plus = SP_KEYWORD_LIFELINK;
	  generate_token(&token);
	}
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d8f10; original cards/amonkhet.c:648. */
int card_rhet_crop_spearmaster(int player, int card, event_t event)
{
  /* Rhet-Crop Spearmaster	|2|W
   * Creature - Human Warrior 3/1
   * You may exert ~ as it attacks. When you do, it gets +1/+0 and gains first strike until end of turn. */

  if (event == EVENT_CHECK_ABILITIES && affect_me(player, card) && current_turn == player)
  {
    check_abilities_keywords |= KEYWORD_FIRST_STRIKE;
    check_abilities_power += 1;
  }
  dat_ability(player, card, event, RESOLVE_TRIGGER_AI(player), 0);
  if (event == EVENT_DAT_ABILITY)
  {
    int exerted = does_not_untap_effect(player, card, player, card, 0, 1);
    if (alternate_legacy_text(2, player, exerted))
    {
      int pump = pump_ability_until_eot(player, card, player, card, 1, 0,
                                      KEYWORD_FIRST_STRIKE, 0);
      alternate_legacy_text(1, player, pump);
    }
  }
  return 0;
}
#endif

// Renewed Faith => onslaught.c

int card_sacred_cat(int player, int card, event_t event)
{
  /* Sacred Cat	|W
   * Creature - Cat 1/1
   * Lifelink
   * Embalm |W */

  lifelink(player, card, event);

  return embalm(player, card, event, CARD_ID_SACRED_CAT, 0, 0, 0, 0, 0, 1);
}

/* Seraph of the Suns	|5|W|W	=>scars_of_mirrodin.c:card_darksteel_myr 0x200422e
 * Creature - Angel 4/4
 * Flying
 * Indestructible */

int card_sparring_mummy(int player, int card, event_t event)
{
  /* Sparring Mummy	|3|W
   * Creature - Zombie 3/3
   * When ~ enters the battlefield, untap target creature. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

  if (event == EVENT_ETB_ABILITY)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.preferred_controller = player;
	  td.allow_cancel = 0;

	  card_instance_t* inst = get_card_instance(player, card);

	  if (can_target(&td) && pick_target(&td, "TARGET_CREATURE"))
		untap_card(inst->targets[0].player, inst->targets[0].card);

	  cancel = 0;
	  inst->number_of_targets = 0;
	}

  return 0;
}

/* ManalinkEh 2022: 0x22d9220; original cards/amonkhet.c:713. */
int card_supply_caravan(int player, int card, event_t event)
{
  /* Supply Caravan	|4|W
   * Creature - Camel 3/5
   * When ~ enters the battlefield, if you control a tapped creature, create a 1/1 |Swhite Warrior creature token with vigilance. */

  int result;
  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_test_definition(&test, 2);
    test.color_flag = 1;
    test.state = 0x10;
    result = check_battlefield_for_special_card(player, card, player, 0, &test);
    if (result != 0)
    {
      vigilant_white_warriors(player, card, 1);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22d92c0; original cards/amonkhet.c:732. */
int card_tah_crop_elite(int player, int card, event_t event)
{
  /* Tah-Crop Elite	|3|W
   * Creature - Bird Warrior 2/2
   * Flying
   * You may exert ~ as it attacks. When you do, creatures you control get +1/+1 until end of turn. */

  int result;

  result = duh_mode(player);
  dat_ability(player, card, event, (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
  if (event == EVENT_DAT_ABILITY)
  {

    result = does_not_untap_effect(player, card, player, card, 0, 1);
    result = alternate_legacy_text(2, player, result);
    if (result != 0)
    {
      pump_creatures_until_eot(player, card, player, 1, 1, 1, 0, 0, NULL);
    }
  }
  return 0;
}

/* Those Who Serve	|2|W	0x401000
 * Creature - Zombie 2/4 */

/* Time to Reflect	|W
 * Instant
 * Exile target creature that blocked or was blocked by a Zombie this turn. */

/* ManalinkEh 2022: 0x22d93a0; original cards/amonkhet.c:789. */
int card_trial_of_solidarity(int player, int card, event_t event)
{
  /* Trial of Solidarity	|2|W
   * Enchantment
   * When ~ enters the battlefield, creatures you control get +2/+1 and gain vigilance until end of turn.
   * When a Cartouche enters the battlefield under your control, return ~ to its owner's hand. */

  int result;
  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    pump_subtype_until_eot(player, card, player, -1, 2, 1, 0, 8);
  }
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 4);
    test.subtype = 0x4011;
    result = new_specific_cip(player, card, event, player, 2, &test);
    if (result != 0)
    {
      bounce_permanent(player, card);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d94b0; original cards/amonkhet.c:814. */
int card_trueheart_duelist(int player, int card, event_t event)
{
  /* Trueheart Duelist	|1|W
   * Creature - Human Warrior 2/2
   * ~ can block an additional creature each combat.
   * Embalm |2|W */

  int result;
  creature_can_block_additional(player, card, event, 1);
  result = embalm(player, card, event, 0x429f, 2, 0, 0, 0, 0, 1);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d9520; original cards/amonkhet.c:825. */
int card_unwavering_initiate(int player, int card, event_t event)
{
  /* Unwavering Initiate	|2|W
   * Creature - Human Warrior 3/2
   * Vigilance
   * Embalm |4|W */

  int result;
  vigilance(player, card, event);
  result = embalm(player, card, event, 0x42a1, 4, 0, 0, 0, 0, 1);
  return result;
}
#endif

/* ManalinkEh 2022: 0x22d9580; original cards/amonkhet.c:835. */
int card_vizier_of_deferment(int player, int card, event_t event)
{
  /* Vizier of Deferment	|2|W
   * Creature - Human Cleric 2/2
   * Flash
   * When ~ enters the battlefield, you may exile target creature if it attacked or blocked this turn. Return that card to the battlefield under its owner's control at the beginning of the next end step. */

  int result;
  card_instance_t *instance;
  card_instance_t *target_record;
  target_definition_t td;

  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_ETB)
  {
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(
          &td, "Select target creature, if it attacked or blocked this turn remove it until eot.",
          0, 0x80);
      if (result != 0)
      {
        target_record = get_card_instance(current_turn, instance->targets[0].card);
        if ((target_record->state & 0x404c) != 0)
        {
          remove_until_eot(current_turn, card, instance->targets[0].player,
                           instance->targets[0].card);
        }
        target_record = get_card_instance(1 - current_turn, instance->targets[0].card);
        if ((target_record->state & 0x404c) != 0)
        {
          remove_until_eot(1 - current_turn, card, instance->targets[0].player,
                           instance->targets[0].card);
        }
      }
    }
    instance->number_of_targets = '\0';
  }
  return flash(player, card, event);
}

/* ManalinkEh 2022: 0x22d96e0; original cards/amonkhet.c:864. */
int card_vizier_of_remedies(int player, int card, event_t event) {
  /* Vizier of Remedies	|1|W
   * Creature - Human Cleric 2/1
   * If one or more -1/-1 counters would be put on a creature you control, that many -1/-1 counters minus one are put on it instead. */

 return 0; }

/* ManalinkEh 2022: 0x22d96f0; original cards/amonkhet.c:873. */
int card_winged_shepherd(int player, int card, event_t event)
{
  /* Winged Shepherd	|5|W
   * Creature - Angel 3/3
   * Flying, vigilance
   * Cycling |W */

  vigilance(player, card, event);
  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    return 0;
  }
  return cycling(player, card, event, 0, 0, 0, 0, 0, 1);
}

/* Angler Drake	|4|U|U	=>return_to_ravnica.c:card_voidwielder 0x2007596
 * Creature - Drake 4/4
 * Flying
 * When ~ enters the battlefield, you may return target creature to its owner's hand. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d9770; original cards/amonkhet.c:894. */
int card_as_foretold(int player, int card, event_t event)
{
  /* As Foretold	|2|U
   * Enchantment
   * At the beginning of your upkeep, put a time counter on ~.
   * Once each turn, you may pay |0 rather than pay the mana cost for a spell you cast with converted mana cost X or less, where X is the number of time counters on ~. */

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    add_counters(player, card, player, card, COUNTER_TIME, 1);
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    for (int i = 0; i < active_cards_count[player]; ++i)
    {
      if (in_hand(player, i) && !is_what(player, i, 1) &&
          get_cmc(player, i) <= count_counters(player, card, COUNTER_TIME) &&
          get_card_instance(player, card)->targets[3].player != 0x42)
      {
        int legal = can_legally_play_card_in_hand(player, i);
        if ((legal == 99 && (is_what(player, i, 0x20) || is_what(player, i, 0x22))) ||
            (legal == 1 &&
             (is_what(player, i, 0x10) || can_sorcery_be_played(player, EVENT_CAN_ACTIVATE) ||
              is_what(player, i, 2) || can_sorcery_be_played(player, EVENT_CAN_ACTIVATE) ||
              flash(player, i, EVENT_CAN_ACTIVATE))))
        {
          return legal;
        }
      }
    }
    return 0;
  }
  else if (event == EVENT_ACTIVATE)
  {
    unsigned capacity = (unsigned)hand_count[player];
    int playable[2][capacity];
    int count = 0;
    for (int i = 0; i < active_cards_count[player]; ++i)
    {
      if (in_hand(player, i) && !is_what(player, i, 1) &&
          get_cmc(player, i) <= count_counters(player, card, COUNTER_TIME) &&
          get_card_instance(player, card)->targets[3].player != 0x42)
      {
        int legal = can_legally_play_card_in_hand(player, i);
        if ((legal == 99 && (is_what(player, i, 0x20) || is_what(player, i, 0x22))) ||
            (legal == 1 &&
             (is_what(player, i, 0x10) || can_sorcery_be_played(player, EVENT_ACTIVATE) ||
              is_what(player, i, 2) || can_sorcery_be_played(player, EVENT_ACTIVATE) ||
              flash(player, i, EVENT_ACTIVATE))))
        {
          playable[0][count] = get_card_instance(player, i)->internal_card_id;
          playable[1][count++] = i;
        }
      }
    }
    test_definition_t test;
    _Static_assert(sizeof(test) == (sizeof(void *) == 8 ? 232 : 216), "native As Foretold filter");
    new_default_test_definition(&test, 1, "Select a spell to cast.");
    test.type_flag = 1;
    int selected = select_card_from_zone(player, player, playable[0], count, 0, 2, -1, &test);
    if (selected == -1)
    {
      spell_fizzled = 1;
      return 0;
    }
    play_card_in_hand_for_free(player, playable[1][selected]);
    cant_be_responded_to = 1;
    get_card_instance(player, card)->targets[3].player = 0x42;
  }
  else if (event == EVENT_CLEANUP)
  {
    get_card_instance(player, card)->targets[3].player = 0;
  }
  return global_enchantment(player, card, event);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d9c50; original cards/amonkhet.c:980. */
int card_aven_initiate(int player, int card, event_t event)
{
  /* Aven Initiate	|3|U
   * Creature - Bird Warrior 3/2
   * Flying
   * Embalm |6|U */

  int result;
  result = embalm(player, card, event, 0x41c9, 6, 0, 1, 0, 0, 0);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22d9ca0; original cards/amonkhet.c:991. */
int card_cartouche_of_knowledge(int player, int card, event_t event)
{
  /* Cartouche of Knowledge	|1|U
   * Enchantment - Aura Cartouche
   * Enchant creature you control
   * When ~ enters the battlefield, draw a card.
   * Enchanted creature gets +1/+1 and has flying. */

  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_a_card(player);
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  attached_gets_pt(player, card, event, 1, 1);
  attached_gets_kw(player, card, event, KEYWORD_FLYING, SP_KEYWORD_0);
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = player;
  return aura(player, card, event, &td, "TARGET_CREATURE_YOU_CONTROL");
}
#endif

/* ManalinkEh 2022: 0x22d9e20; original cards/amonkhet.c:1017. */
int card_censor(int player, int card, event_t event)
{
  /* Censor	|1|U
   * Instant
   * Counter target spell unless its controller pays |1.
   * Cycling |U */

  if (!IS_ACTIVATING_FROM_HAND(event))
    return card_force_spike(player, card, event);
  return cycling(player, card, event, 0, 0, 1, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22d9e90; original cards/amonkhet.c:1030. */
int card_compelling_argument(int player, int card, event_t event)
{
  /* Compelling Argument	|1|U
   * Sorcery
   * Target player puts the top five cards of their library into their graveyard.
   * Cycling |U */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = card_tome_scour(player, card, event);
    return result;
  }
  return cycling(player, card, event, 0, 0, 1, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22d9f00; original cards/amonkhet.c:1042. */
int card_cryptic_serpent(int player, int card, event_t event)
{
  /* Cryptic Serpent	|5|U|U
   * Creature - Serpent 6/5
   * This spell costs |1 less to cast for each instant and sorcery card in your graveyard. */

  int result;
  if (event != EVENT_MODIFY_COST)
  {
    return 0;
  }
  result = count_graveyard_by_type(player, 0x38);
  COST_COLORLESS = COST_COLORLESS - (char)result;
  return 0;
}

/* ManalinkEh 2022: 0x22d9f40; original cards/amonkhet.c:1053. */
int card_curator_of_mysteries(int player, int card, event_t event)
{
  /* Curator of Mysteries	|2|U|U
   * Creature - Sphinx 4/4
   * Flying
   * Whenever you cycle or discard another card, scry 1.
   * Cycling |U */

  int result;
  result = discard_trigger(player, card, event, player, 2, 0);
  if (result != 0)
  {
    scry(player, 1);
  }
  result = cycling(player, card, event, 0, 0, 1, 0, 0, 0);
  return result;
}

/* Decision Paralysis	|3|U	=>m12.c:card_frost_breath 0x2004035
 * Instant
 * Tap up to two target creatures. Those creatures don't untap during their controller's next untap step. */

/* ManalinkEh 2022: 0x22d9fe0; original cards/amonkhet.c:1074. */
int card_drake_haven(int player, int card, event_t event)
{
  /* Drake Haven	|2|U
   * Enchantment
   * Whenever you cycle or discard a card, you may pay |1. If you do, create a 2/2 |Sblue Drake creature token with flying. */

  int result;

  result = discard_trigger(player, card, event, player, 2, 0);
  if (result != 0)
  {
    result = has_mana(player, 7, 1);
    if (result != 0)
    {
      result = charge_mana(player, COLOR_COLORLESS, 1);
      if (result != 0)
      {
        generate_token_by_id(player, card, CARD_ID_DRAKE);
      }
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22da080; original cards/amonkhet.c:1091. */
int card_floodwaters(int player, int card, event_t event)
{
  /* Floodwaters	|4|U|U
   * Sorcery
   * Return up to two target creatures to their owners' hands.
   * Cycling |2 */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = card_into_the_void(player, card, event);
    return result;
  }
  return cycling(player, card, event, 2, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22da0f0; original cards/amonkhet.c:1103. */
int card_galestrike(int player, int card, event_t event)
{
  /* Galestrike	|2|U
   * Instant
   * Return target tapped creature to its owner's hand.
   * Draw a card. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.required_state = TARGET_STATE_TAPPED;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      bounce_permanent(instance->targets[0].player, instance->targets[0].card);
      draw_a_card(player);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target tapped creature.", 1, NULL);
}

/* Glyph Keeper	|3|U|U
 * Creature - Sphinx 5/3
 * Flying
 * Whenever ~ becomes the target of a spell or ability for the first time each turn, counter that spell or ability.
 * Embalm |5|U|U */

/* ManalinkEh 2022: 0x22da250; original cards/amonkhet.c:1135. */
int card_hekma_sentinels(int player, int card, event_t event)
{
  /* Hekma Sentinels	|2|U
   * Creature - Human Cleric 2/3
   * Whenever you cycle or discard a card, ~ gets +1/+1 until end of turn. */

  if (!discard_trigger(player, card, event, player, 2, 0))
  {
    return 0;
  }
  pump_until_eot_merge_previous(player, card, player, card, 1, 1);
  return 0;
}

/* ManalinkEh 2022: 0x22da2d0; original cards/amonkhet.c:1146. */
int card_hieroglyphic_illumination(int player, int card, event_t event)
{
  /* Hieroglyphic Illumination	|3|U
   * Instant
   * Draw two cards.
   * Cycling |U */

  int result;
  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      draw_cards(player, 2);
      kill_card(player, card, KILL_DESTROY);
    }
    result = basic_spell(player, card, event);
    return result;
  }
  result = cycling(player, card, event, 0, 0, 1, 0, 0, 0);
  return result;
}

/* Illusory Wrappings	|2|U
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature has base power and toughness 0/2. */

/* ManalinkEh 2022: 0x22da380; original cards/amonkhet.c:1168. */
int card_kefnet_the_mindful(int player, int card, event_t event)
{
  /* Kefnet the Mindful	|2|U
   * Legendary Creature - God 5/5
   * Flying, indestructible
   * ~ can't attack or block unless you have seven or more cards in hand.
   * |3|U: Draw a card, then you may return a land you control to its owner's hand. */

  check_legend_rule(player, card, event);
  indestructible(player, card, event);
  if (event == EVENT_ATTACK_LEGALITY)
  {
    if (affect_me(player, card) && hand_count[player] < 7 &&
        !is_humiliated(player, card, AL_STATIC))
      event_result = 1;
  }
  else if (event == EVENT_ABILITIES)
  {
    if (affect_me(player, card) && hand_count[player] < 7 &&
        !is_humiliated(player, card, AL_STATIC))
      cannot_block(player, card, EVENT_ABILITIES);
  }

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_LAND);
  td.preferred_controller = player;
  td.allowed_controller = player;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_a_card(player);
    if (can_target(&td) &&
        new_pick_target(&td, "Select a land you control to return to your hand.", 0, 0x80))
      bounce_permanent(player, instance->targets[0].card);
  }
  return generic_activated_ability(player, card, event, 0, MANACOST_XU(3, 1), 0, NULL, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22da5b0; original cards/amonkhet.c:1207. */
int card_labyrinth_guardian(int player, int card, event_t event)
{
  /* Labyrinth Guardian	|1|U
   * Creature - Illusion Warrior 2/3
   * When ~ becomes the target of a spell, sacrifice it.
   * Embalm |3|U */

  int result;
  if ((event != EVENT_GRAVEYARD_ABILITY && event != EVENT_PAY_FLASHBACK_COSTS) &&
      (event != EVENT_RESOLVE_ACTIVATED_GRAVEYARD_ABILITY))
  {
    result = card_skulking_ghost(player, card, event);
    return result;
  }
  return embalm(player, card, event, 0x423c, 3, 0, 1, 0, 0, 0);
}
#endif

/* ManalinkEh 2022: 0x22da630; original cards/amonkhet.c:1220. */
int card_lay_claim(int player, int card, event_t event)
{
  /* Lay Claim	|5|U|U
   * Enchantment - Aura
   * Enchant permanent
   * You control enchanted permanent.
   * Cycling |2 */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = card_confiscate(player, card, event);
    return result;
  }
  return cycling(player, card, event, 2, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22da6a0; original cards/amonkhet.c:1233. */
int card_naga_oracle(int player, int card, event_t event)
{
  /* Naga Oracle	|3|U
   * Creature - Naga Cleric 2/4
   * When ~ enters the battlefield, look at the top three cards of your library. Put any number of them into your graveyard and the rest back on top of your library in any order. */

  int result;
  int number;
  test_definition_t this_test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    result = count_deck(player);
    number = result;
    if (3 < result)
    {
      number = 3;
    }
    new_default_test_definition(&this_test, 0xff, "Select a card to put into your graveyard.");
    this_test.no_shuffle = 1;
    while (result != 0)
    {
      this_test.create_minideck = (unsigned short)number;
      result = new_global_tutor(player, player, 1, 3, 0, 3, &this_test);
      if (result < 0)
      {
        if (number < 2)
        {
          return 0;
        }
        rearrange_top_x(player, player, number);
        return 0;
      }
      number = number + -1;
      result = number;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22da7b0; original cards/amonkhet.c:1263. */
int card_new_perspectives(int player, int card, event_t event)
{
  /* New Perspectives	|5|U
   * Enchantment
   * When ~ enters the battlefield, draw three cards.
   * As long as you have seven or more cards in hand, you may pay |0 rather than pay cycling costs. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_cards(player, 3);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x22d6bd0; original cards/amonkhet.c:1277. */
static int effect_open_into_wonder(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  if ((((event == EVENT_DEAL_DAMAGE) || (event == EVENT_GRAVEYARD_FROM_PLAY)) ||
       (trigger_condition == TRIGGER_DEAL_DAMAGE)) ||
      (event == EVENT_CHANGE_TYPE))
  {

    instance = get_card_instance(player, card);
    if (-1 < instance->damage_target_card)
    {
      result = is_humiliated((int)instance->damage_target_player,
                                         instance->damage_target_card, AL_TRIG);
      if (result == 0)
      {
        result = equipped_creature_deals_damage(
            player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE);
        if (result != 0)
        {
          draw_cards(player, result);
        }
      }
    }
  }
  if ((event != EVENT_CLEANUP) && (event != EVENT_SHOULD_AI_PLAY))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x22da810; original cards/amonkhet.c:1294. */
int card_open_into_wonder(int player, int card, event_t event)
{
  /* Open into Wonder	|X|U|U
   * Sorcery
   * X target creatures can't be blocked this turn. Until end of turn, those creatures gain "Whenever this creature deals combat damage to a player, draw a card." */

  card_instance_t *instance;
  int available_or_x;
  int i;
  target_t *target_cursor;
  target_t *other_target_cursor;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 5, &td, NULL, 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        i = 0;
        instance->number_of_targets = '\0';
        while (
            ((available_or_x = has_mana(player, 0, i),
              available_or_x != 0 && (available_or_x = can_target(&td), available_or_x != 0)) &&
             (available_or_x = new_pick_target(&td, "TARGET_CREATURE", i, 0), available_or_x != 0)))
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
            available_or_x = x_value;
            target_cursor = other_target_cursor;
          } while (instance->targets + i != other_target_cursor);
          if (spell_fizzled == 1)
          {
            return 0;
          }
          instance->info_slot = x_value;
          if (available_or_x <= i)
          {
            return 0;
          }
        }
        spell_fizzled = 1;
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      i = 0;
      if (0 < instance->info_slot)
      {
        do
        {
          if (available_or_x = validate_target(player, card, &td, i), available_or_x == 0)
          {
            i = i + 1;
            continue;
          }
          alternate_legacy_text(1, player,
                                pump_ability_until_eot(player, card, instance->targets[i].player,
                                                       instance->targets[i].card, 0, 0, 0, 0x10));
          available_or_x = create_targetted_legacy_effect(player, card, effect_open_into_wonder,
                                                          instance->targets[i].player,
                                                          instance->targets[i].card);
          i = i + 1;
          alternate_legacy_text(2, player, available_or_x);
        } while (i < instance->info_slot);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x22dab70; original cards/amonkhet.c:1358. */
int card_pull_from_tomorrow(int player, int card, event_t event)
{
  /* Pull from Tomorrow	|X|U|U
   * Instant
   * Draw X cards, then discard a card. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    draw_cards(player, instance->info_slot);
    discard(player, 0, player);
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
  return result;
}

/* ManalinkEh 2022: 0x22dac10; original cards/amonkhet.c:1373. */
int card_river_serpent(int player, int card, event_t event)
{
  /* River Serpent	|5|U
   * Creature - Serpent 5/5
   * ~ can't attack unless there are five or more cards in your graveyard.
   * Cycling |U */

  int result;

  if (((event == EVENT_ATTACK_LEGALITY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      result = count_graveyard(player);
      if (result < 5)
      {
        event_result = 1;
      }
    }
  }
  result = cycling(player, card, event, 0, 0, 1, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22d6b60; original cards/amonkhet.c:1386. */
static int iid_has_cycling(int iid, intptr_t me, int player, int card)
{
  return (unsigned int)(cards_data[iid].cc[2] == '\x04');
}

/* ManalinkEh 2022: 0x22dacc0; original cards/amonkhet.c:1393. */
int card_sacred_excavation(int player, int card, event_t event)
{
  /* Sacred Excavation	|3|U
   * Sorcery
   * Return up to two target cards with cycling from your graveyard to your hand. */

  int result;
  test_definition_t this_test;

  new_default_test_definition(&this_test, 0xff, "Select a card with Cycling.");
  this_test.special_selection_function = iid_has_cycling;
  result = spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(player, card, event,
                                                                              2, &this_test, 0);
  return result;
}

/* ManalinkEh 2022: 0x22dad30; original cards/amonkhet.c:1405. */
int card_scribe_of_the_mindful(int player, int card, event_t event)
{
  /* Scribe of the Mindful	|2|U
   * Creature - Human Cleric 2/2
   * |1, |T, Sacrifice ~: Return target instant or sorcery card from your graveyard to your hand. */

  int result;
  test_definition_t this_test;
  if (!IS_GAA_EVENT(event))
    return 0;

  new_default_test_definition(&this_test, 0x38, "Select an instant or sorcery card.");
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = validate_target_from_grave_source(player, card, player, 0), result != -1))
  {
    from_grave_to_hand(player, result, 1);
  }
  result = generic_activated_ability_with_target_in_grave(
      player, card, event, GAA_SACRIFICE_ME | GAA_LITERAL_PROMPT | GAA_UNTAPPED, 1, 0, 0, 0, 0, 0,
      0, NULL, NULL, TYPE_NONE, &this_test, 8);
  return result;
}

/* ManalinkEh 2022: 0x22dae70; original cards/amonkhet.c:1428. */
int card_seeker_of_insight(int player, int card, event_t event)
{
  /* Seeker of Insight	|1|U
   * Creature - Human Wizard 1/3
   * |T: Draw a card, then discard a card. Activate this ability only if you've cast a noncreature spell this turn. */

  int activation_result;
  int total_spells;
  int creature_spells;

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_cards(player, 1);
    discard(player, 0, player);
    return generic_activated_ability(player, card, EVENT_RESOLVE_ACTIVATION, 1, 0, 0, 0, 0, 0, 0, 0,
                                     NULL, NULL);
  }
  activation_result =
      generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  if ((event == EVENT_CAN_ACTIVATE) && (activation_result != 0))
  {
    total_spells = get_specific_storm_count(player);
    creature_spells = get_stormcreature_count(player);
    if (total_spells <= creature_spells)
    {
      return 0;
    }
  }
  return activation_result;
}

/* Shimmerscale Drake	|4|U
 * Creature - Drake 3/4
 * Flying
 * Cycling |2 */

/* Slither Blade	|U	=>avacyn_restored.c:card_latch_seeker 0x2005b0b
 * Creature - Naga Rogue 1/2
 * ~ can't be blocked. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22dafd0; original cards/amonkhet.c:1455. */
int card_tah_crop_skirmisher(int player, int card, event_t event)
{
  /* Tah-Crop Skirmisher	|1|U
   * Creature - Naga Warrior 2/1
   * Embalm |3|U */

  int result;
  result = embalm(player, card, event, 0x4292, 3, 0, 1, 0, 0, 0);
  return result;
}
#endif

/* ManalinkEh 2022: 0x22db020; original cards/amonkhet.c:1463. */
int card_trial_of_knowledge(int player, int card, event_t event)
{
  /* Trial of Knowledge	|3|U
   * Enchantment
   * When ~ enters the battlefield, draw three cards, then discard a card.
   * When a Cartouche enters the battlefield under your control, return ~ to its owner's hand. */

  int result;
  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_cards(player, 3);
    discard(player, 0, player);
  }
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 4);
    test.subtype = 0x4011;
    result = new_specific_cip(player, card, event, player, 2, &test);
    if (result != 0)
    {
      bounce_permanent(player, card);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22db120; original cards/amonkhet.c:1488. */
int card_vizier_of_many_faces(int player, int card, event_t event)
{
  /* Vizier of Many Faces	|2|U|U
   * Creature - Shapeshifter Cleric 0/0
   * You may have ~ enter the battlefield as a copy of any creature on the battlefield, except if ~ was embalmed, the token has no mana cost, it's |Swhite, and it's a Zombie in addition to its other types.
   * Embalm |3|U|U */

  int result;
  card_instance_t *instance;
  result = enters_the_battlefield_as_copy_of_any_creature(player, card, event);
  if (result != 0)
  {
    result = check_special_flags3(player, card, 0x40000);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      instance->initial_color = ' ';
    }
  }
  result = embalm(player, card, event, 0x42a4, 3, 0, 2, 0, 0, 0);
  return result;
}
#endif

/* ManalinkEh 2022: 0x22db1c0; original cards/amonkhet.c:1504. */
int card_vizier_of_tumbling_sands(int player, int card, event_t event)
{
  /* Vizier of Tumbling Sands	|2|U
   * Creature - Human Cleric 1/3
   * |T: Untap another target permanent.
   * Cycling |1|U
   * When you cycle ~, untap target permanent. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event == EVENT_RESOLVE_ACTIVATION_FROM_HAND)
  {
    if ((((unsigned int)player_bits[player] >> 24 & 0xff) & 2) == 0)
    {

      default_target_definition(player, card, &td, 0x47);
      instance = get_card_instance(player, card);
      result = can_target(&td);
      if (result != 0)
      {
        result = new_pick_target(&td, "Select target permanent to untap.", 0, 0x80);
        if (result != 0)
        {
          untap_card(instance->targets[0].player, instance->targets[0].card);
        }
      }
    }
  }
  else if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
            (event == EVENT_RESOLVE_ACTIVATION)) ||
           (event == EVENT_CLEANUP))
  {
    default_target_definition(player, card, &td, 0x47);
    td.special = TARGET_SPECIAL_NOT_ME;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = valid_target(&td);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        untap_card(instance->targets[0].player, instance->targets[0].card);
      }
    }
    result = generic_activated_ability(player, card, event, 7, 0, 0, 0, 0, 0, 0, 0, &td,
                                       "Select another target permanent to untap.");
    return result;
  }
  result = cycling(player, card, event, 1, 0, 1, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22db3a0; original cards/amonkhet.c:1540. */
int card_winds_of_rebuke(int player, int card, event_t event)
{
  /* Winds of Rebuke	|1|U
   * Instant
   * Return target nonland permanent to its owner's hand. Each player puts the top two cards of their library into their graveyard. */

  card_instance_t *instance;
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
    if (valid_target(&td))
    {
      bounce_permanent(instance->targets[0].player, instance->targets[0].card);
      mill(current_turn, 2);
      mill(current_turn ^ 1, 2);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td,
                         "Select target nonland permanent to bounce.", 1, NULL);
}

/* ManalinkEh 2022: 0x22db520; original cards/amonkhet.c:1566. */
int card_zenith_seeker(int player, int card, event_t event)
{
  /* Zenith Seeker	|3|U
   * Creature - Bird Wizard 2/2
   * Flying
   * Whenever you cycle or discard a card, target creature gains flying until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  result = discard_trigger(player, card, event, player, 2, 0);
  if (result != 0)
  {
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    instance->number_of_targets = '\0';
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_CREATURE");
      if (result != 0)
      {
        pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                               0, 0, 0x20, 0);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22db610; original cards/amonkhet.c:1586. */
int card_arcfiend_of_ifnir(int player, int card, event_t event)
{
  /* Archfiend of Ifnir	|3|B|B
   * Creature - Demon 5/4
   * Flying
   * Whenever you cycle or discard another card, put a -1/-1 counter on each creature your opponents control.
   * Cycling |2 */

  int result;
  result = discard_trigger(player, card, event, player, 2, 0);
  if (result != 0)
  {
    manipulate_type(player, card, 1 - player, TYPE_CREATURE, 0x1010001);
  }
  result = cycling(player, card, event, 2, 0, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22db6d0; original cards/amonkhet.c:1600. */
int card_baleful_ammit(int player, int card, event_t event)
{
  /* Baleful Ammit	|2|B
   * Creature - Crocodile Demon 4/3
   * Lifelink
   * When ~ enters the battlefield, put a -1/-1 counter on target creature you control. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  lifelink(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }

  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.allow_cancel = 0;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0);
    if (result != 0)
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                               COUNTER_M1_M1, 1);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x22db7d0; original cards/amonkhet.c:1627. */
int card_blighted_bat(int player, int card, event_t event)
{
  /* Blighted Bat	|2|B
   * Creature - Zombie Bat 2/1
   * Flying
   * |1: ~ gains haste until end of turn. */

  int result;
  result = generic_shade(player, card, event, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 4);
  return result;
}

/* ManalinkEh 2022: 0x22db850; original cards/amonkhet.c:1636. */
int card_bone_picker(int player, int card, event_t event)
{
  /* Bone Picker	|3|B
   * Creature - Bird 3/2
   * This spell costs |3 less to cast if a creature died this turn.
   * Flying, deathtouch */

  deathtouch(player, card, event);
  if ((event == EVENT_MODIFY_COST) && (0 < creatures_dead_this_turn))
  {
    COST_COLORLESS = COST_COLORLESS + -3;
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22db8a0; original cards/amonkhet.c:1652. */
int card_bontu_the_glorified(int player, int card, event_t event)
{
  /* Bontu the Glorified	|2|B
   * Legendary Creature - God 4/6
   * Menace, indestructible
   * ~ can't attack or block unless a creature died under your control this turn.
   * |1|B, Sacrifice another creature: Scry 1. Each opponent loses 1 life and you gain 1 life. */

  int result;

  menace(player, card, event);
  indestructible(player, card, event);
  if (event == EVENT_ATTACK_LEGALITY)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = get_dead_count(player, 2);
        if (result == 0)
        {
          event_result = 1;
        }
      }
    }
  }
  else if (event == EVENT_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = get_dead_count(player, 2);
        if (result == 0)
        {
          cannot_block(player, card, EVENT_ABILITIES);
        }
      }
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    scry(player, 1);
    lose_life(1 - player, 1);
    gain_life(player, 1);
  }
  result = generic_activated_ability(player, card, event, 0x40200, 1, 1, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22dba60; original cards/amonkhet.c:1681. */
int card_cartouche_of_ambition(int player, int card, event_t event)
{
  /* Cartouche of Ambition	|2|B
   * Enchantment - Aura Cartouche
   * Enchant creature you control
   * When ~ enters the battlefield, you may put a -1/-1 counter on target creature.
   * Enchanted creature gets +1/+1 and has lifelink. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if ((result != 0) &&
        (result = new_pick_target(&td, "Select target creature to put a -1/-1 counter.", 0, 0x80),
         result != 0))
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                   COUNTER_M1_M1, 1);
    }
    instance->number_of_targets = '\0';
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  attached_gets_pt(player, card, event, 1, 1);
  attached_gets_kw(player, card, event, KEYWORD_0, SP_KEYWORD_LIFELINK);
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = player;
  return aura(player, card, event, &td, "TARGET_CREATURE_YOU_CONTROL");
}
#endif

/* ManalinkEh 2022: 0x22dbc70; original cards/amonkhet.c:1716. */
int card_cruel_reality(int player, int card, event_t event)
{
  /* Cruel Reality	|5|B|B
   * Enchantment - Aura Curse
   * Enchant player
   * At the beginning of enchanted player's upkeep, that player sacrifices a creature or planeswalker. If the player can't, they lose 5 life. */

  card_instance_t *instance;
  int result;
  test_definition_t test;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = get_card_instance(player, card);
    if (instance->targets[0].player != -1)
    {

      instance = get_card_instance(player, card);
      upkeep_trigger_ability(player, card, event, instance->targets[0].player);
      if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
      {

        new_default_test_definition(&test, 0x102,
                                    "Select a creature or planeswalker to sacrifice.");
        result = new_sacrifice(player, card, instance->targets[0].player,
                               SAC_CAUSED | SAC_NO_CANCEL, &test);
        if (result == 0)
        {
          lose_life(instance->targets[0].player, 5);
        }
      }
    }
  }
  result = curse(player, card, event);
  return result;
}

/* Cursed Minotaur	|2|B
 * Creature - Zombie Minotaur 3/2
 * Menace */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22dbd60; original cards/amonkhet.c:1745. */
int card_dispossess(int player, int card, event_t event)
{
  /* Dispossess	|2|B
   * Sorcery
   * Choose an artifact card name. Search target opponent's graveyard, hand, and library for any number of cards with the chosen name and exile them. Then that player shuffles their library. */

  if (event == EVENT_CAST_SPELL)
  {
    if (!affect_me(player, card))
      return 0;
  }
  else if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL &&
           event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  int opponent = 1 - player;
  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.allowed_controller = opponent;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      uint32_t csvid;
      if (ai_is_speculating == 1 || (player == 1 && (trace_mode & 2) == 0))
      {
        int iid = -1;
        for (int i = 0; i < 500 && deck_ptr[opponent][i] != -1; ++i)
          if (is_what(-1, deck_ptr[opponent][i], 0x40))
          {
            iid = deck_ptr[opponent][i];
            break;
          }
        if (iid == -1)
        {
          const int *grave = get_grave(opponent);
          for (int i = 0; i < 500 && grave[i] != -1; ++i)
            if (is_what(-1, grave[i], 0x40))
            {
              iid = grave[i];
              break;
            }
        }
        if (player == 1)
          ai_modifier += iid == -1 ? -100 : 100;
        csvid = (uint16_t)cards_data[iid].id;
        const char *name = get_card_name_by_id(csvid);
        dialog_impl(player, card, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_SMALLCARD_ID(iid), name, 1,
                    1, 0, 0, 0, 0);
      }
      else
      {
        csvid = choose_a_card("Choose an artifact card name.", (color_test_t)-1,
                              TYPE_ARTIFACT);
      }
      lobotomy_effect(player, opponent, csvid, -1, 0);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x40, &td, "TARGET_OPPONENT", 1, NULL);
}
#endif

/* ManalinkEh 2022: 0x22dc0a0; original cards/amonkhet.c:1817. */
int card_doomed_dissenter(int player, int card, event_t event)
{
  /* Doomed Dissenter	|1|B
   * Creature - Human 1/1
   * When ~ dies, create a 2/2 |Sblack Zombie creature token. */

  int result;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_ZOMBIE);
  return 0;
}

/* ManalinkEh 2022: 0x22dc100; original cards/amonkhet.c:1829. */
int card_dread_wanderer(int player, int card, event_t event)
{
  /* Dread Wanderer	|B
   * Creature - Zombie Jackal 2/1
   * ~ enters the battlefield tapped.
   * |2|B: Return ~ from your graveyard to the battlefield. Activate this ability only any time you could cast a sorcery and only if you have one or fewer cards in hand. */

  int result;
  comes_into_play_tapped(player, card, event);
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = has_mana_multi(player, 2, 1, 0, 0, 0, 0);
    if (((result != 0) &&
         (result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY), result != 0)) &&
        (hand_count[player] < 2))
    {
      return 0x88;
    }
  }
  else if (event == EVENT_PAY_FLASHBACK_COSTS)
  {
    charge_mana_multi(player, 2, 1, 0, 0, 0, 0);
    return (unsigned int)(spell_fizzled != 1);
  }
  return 0;
}

/* Dune Beetle	|1|B
 * Creature - Insect 1/4 */

/* ManalinkEh 2022: 0x22dc200; original cards/amonkhet.c:1856. */
int card_faith_of_the_devoted(int player, int card, event_t event)
{
  /* Faith of the Devoted	|2|B
   * Enchantment
   * Whenever you cycle or discard a card, you may pay |1. If you do, each opponent loses 2 life and you gain 2 life. */

  int result;

  result = discard_trigger(player, card, event, player, 2, 0);
  if (result != 0)
  {
    result = has_mana(player, 7, 1);
    if (result != 0)
    {
      result = charge_mana(player, COLOR_COLORLESS, 1);
      if (result != 0)
      {
        lose_life(1 - player, 2);
        gain_life(player, 2);
      }
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* Festering Mummy	|B	=>scars_of_mirrodin.c:card_necropede 0x2004337
 * Creature - Zombie 1/1
 * When ~ dies, you may put a -1/-1 counter on target creature. */

/* Final Reward	|4|B	=>oath_of_the_gatewatch.c:card_oblivion_strike 0x200e56c
 * Instant
 * Exile target creature. */

/* ManalinkEh 2022: 0x22dc2c0; original cards/amonkhet.c:1881. */
int card_grim_strider(int player, int card, event_t event)
{
  /* Grim Strider	|3|B
   * Creature - Horror 6/6
   * ~ gets -1/-1 for each card in your hand. */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((((result == 0) && ((uint32_t)(event - EVENT_POWER) < 2)) && (card == affected_card)) &&
        (player == affected_card_controller))
    {
      event_result = event_result - hand_count[player];
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22dc340; original cards/amonkhet.c:1894. */
int card_horror_of_the_broken_lands(int player, int card, event_t event)
{
  /* Horror of the Broken Lands	|4|B
   * Creature - Horror 4/4
   * Whenever you cycle or discard another card, ~ gets +2/+1 until end of turn.
   * Cycling |B */

  int result;
  result = discard_trigger(player, card, event, player, 2, 0);
  if (result != 0)
  {
    pump_until_eot_merge_previous(player, card, player, card, 2, 1);
  }
  result = cycling(player, card, event, 0, 1, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22dc3f0; original cards/amonkhet.c:1907. */
int card_lay_bare_the_heart(int player, int card, event_t event)
{
  /* Lay Bare the Heart	|1|B
   * Sorcery
   * Target opponent reveals their hand. You choose a nonlegendary, nonland card from it. That player discards that card. */

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
      new_default_test_definition(&this_test, 1, "Select a nonlegendary, nonland card.");
      this_test.subtype = 0x2001;
      this_test.type_flag = 1;
      this_test.subtype_flag = 1;
      new_effect_coercion(&ec, &this_test);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x40, &td, NULL, 1, NULL);
}

/* ManalinkEh 2022: 0x22dc5b0; original cards/amonkhet.c:1940. */
int card_liliana_deaths_majesty(int player, int card, event_t event)
{
  /* Liliana, Death's Majesty	|3|B|B
   * Legendary Planeswalker - Liliana (5)
   * +1: Create a 2/2 |Sblack Zombie creature token. Put the top two cards of your library into your graveyard.
   * -3: Return target creature card from your graveyard to the battlefield. That creature is a |Sblack Zombie in addition to its other colors and types.
   * -7: Destroy all non-Zombie creatures. */

  enum
  {
    CHOICE_TOKEN = 1,
    CHOICE_REANIMATE = 2,
    CHOICE_WIPE = 3
  };

  int choice;
  card_instance_t *instance;
  int reanimate_available_or_mode;
  int own_nonzombies;
  int opponent_nonzombies;
  test_definition_t *creature_test;
  token_generation_t token;
  test_definition_t test;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    new_default_test_definition(&test, 2, "Select target creature card.");
    default_test_definition(&this_test, 2);
    this_test.subtype = 0xde;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      instance->targets[0].card = -1;
      instance->targets[0].player = -1;
      reanimate_available_or_mode = any_in_graveyard_by_type(player, 2);
      own_nonzombies = check_battlefield_for_special_card(player, card, player, 4, &this_test);
      opponent_nonzombies =
          check_battlefield_for_special_card(player, card, 1 - player, 4, &this_test);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                           "Make a token & self-mill", 1, 5, 1, "Reanimate",
                           reanimate_available_or_mode, 8, 0xfffffffd, "Destroy all non-Zombies", 1,
                           (own_nonzombies - opponent_nonzombies) * 3, 0xfffffff9, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_REANIMATE)
      {
        generic_activated_ability_with_target_in_grave(player, card, EVENT_ACTIVATE, GAA_NONE, 0, 0,
                                                       0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test,
                                                       8);
      }
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    new_default_test_definition(&test, 2, "Select target creature card.");
    default_test_definition(&this_test, 2);
    this_test.subtype = 0xde;
    instance = get_card_instance(player, card);
  }
  creature_test = &this_test;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    reanimate_available_or_mode = instance->info_slot;
    if (reanimate_available_or_mode == CHOICE_TOKEN)
    {
      default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
      token.tou = 2;
      token.pow = 2;
      token.color_forced = '\x02';
      generate_token(&token);
      mill(player, 2);
    }
    else if (reanimate_available_or_mode == CHOICE_REANIMATE)
    {

      reanimate_available_or_mode = validate_target_from_grave_source(player, card, player, 0);
      if (reanimate_available_or_mode != -1)
      {
        reanimate_permanent(player, card, player, reanimate_available_or_mode, REANIMATE_DEFAULT);
      }
    }
    else if (reanimate_available_or_mode == CHOICE_WIPE)
    {
      this_test.subtype_flag = 1;
      new_manipulate_all(player, card, 2, creature_test, ACT_KILL_DESTROY);
    }
  }
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x22dc980; original cards/amonkhet.c:2020. */
int card_lilianas_mastery(int player, int card, event_t event)
{
  /* Liliana's Mastery	|3|B|B
   * Enchantment
   * Zombies you control get +1/+1.
   * When ~ enters the battlefield, create two 2/2 |Sblack Zombie creature tokens. */

  int result;
  token_generation_t token;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
    token.qty = '\x02';
    generate_token(&token);
  }
  boost_creature_type(player, card, event, SUBTYPE_ZOMBIE, 1, 1, 0,
                      BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22dca40; original cards/amonkhet.c:2040. */
int card_lord_of_the_accursed(int player, int card, event_t event)
{
  /* Lord of the Accursed	|2|B
   * Creature - Zombie 2/3
   * Other Zombies you control get +1/+1.
   * |1|B, |T: All Zombies gain menace until end of turn. */

  int result;
  boost_creature_type(player, card, event, SUBTYPE_ZOMBIE, 1, 1, 0, BCT_CONTROLLER_ONLY);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    pump_subtype_until_eot(player, card, 2, 0xde, 0, 0, 0, 0x10000);
  }
  result = generic_activated_ability(player, card, event, 1, 1, 1, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* Miasmic Mummy	|1|B	=>dragons_of_tarkir.c:card_sibsig_icebreakers 0x200dae0
 * Creature - Zombie Jackal 2/2
 * When ~ enters the battlefield, each player discards a card. */

/* ManalinkEh 2022: 0x22dcb40; original cards/amonkhet.c:2059. */
int card_nest_of_scarabs(int player, int card, event_t event)
{
  /* Nest of Scarabs	|2|B
   * Enchantment
   * Whenever you put one or more -1/-1 counters on a creature, create that many 1/1 |Sblack Insect creature tokens. */

  int result;
  token_generation_t token;

  if ((event == EVENT_COUNTERS) && (attacking_card_controller == player))
  {
    result = is_what(affected_card_controller, affected_card, 2);
    if ((result != 0) && (counters_added_type == 1))
    {
      result = is_humiliated(player, card, AL_TRIG);
      if (result == 0)
      {
        default_token_definition(player, card, CARD_ID_INSECT, &token);
        token.qty = (unsigned char)counters_added;
        token.color_forced = '\x02';
        generate_token(&token);
      }
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* Painful Lesson	|2|B	=>m11.c:card_sign_in_blood 0x2002505
 * Sorcery
 * Target player draws two cards and loses 2 life. */

/* ManalinkEh 2022: 0x22dcc00; original cards/amonkhet.c:2081. */
int card_pitiless_vizier(int player, int card, event_t event)
{
  /* Pitiless Vizier	|3|B
   * Creature - Minotaur Cleric 4/2
   * Whenever you cycle or discard a card, ~ gains indestructible until end of turn. */

  if (!discard_trigger(player, card, event, player, 2, 0))
  {
    return 0;
  }
  pump_ability_until_eot(player, card, player, card, 0, 0, 0, 0x800);
  return 0;
}

/* ManalinkEh 2022: 0x22d6ca0; original cards/amonkhet.c:2092. */
static int plague_belcher_effect(int player, int card, event_t event)
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
          (get_id((int)instance->damage_target_player, instance->damage_target_card) != 0x4261))
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
          SET_BYTE2(instance->targets[0xb].player) = 1;
          return 0;
        }
        default_test_definition(&this_test, 0x47);
        this_test.subtype = 0xde;
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
        lose_life(1 - player, 1);
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

/* ManalinkEh 2022: 0x22dcc90; original cards/amonkhet.c:2149. */
int card_plague_belcher(int player, int card, event_t event)
{
  /* Plague Belcher	|2|B
   * Creature - Zombie Beast 5/4
   * Menace
   * When ~ enters the battlefield, put two -1/-1 counters on target creature you control.
   * Whenever another Zombie you control dies, each opponent loses 1 life. */

  int c;
  card_instance_t *instance;
  unsigned int pending_deaths;
  int created_or_result;

  menace(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  bool legacy_found = false;
  if (event == EVENT_ETB)
  {
    target_definition_t td;

    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    created_or_result = can_target(&td);
    if ((created_or_result != 0) &&
        (created_or_result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0),
         created_or_result != 0))
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                   COUNTER_M1_M1, 2);
    }
    instance->number_of_targets = '\0';
  }
  else if (event == EVENT_STATIC_EFFECTS)
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
                instance->info_slot_fn == plague_belcher_effect)) &&
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
        created_or_result =
            create_targetted_legacy_effect(player, card, plague_belcher_effect, player, card);
        instance = get_card_instance(player, created_or_result);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, created_or_result, 0x10000);
      }
    }
  }
  else if ((event == EVENT_GRAVEYARD_FROM_PLAY) &&
           ((card != affected_card || (player != affected_card_controller))))
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 0x47);
    this_test.subtype = SUBTYPE_ZOMBIE;
    this_test.not_me = 1;
    count_for_gfp_ability(player, card, 0x77, player, 0, &this_test);
  }
  instance = get_card_instance(player, card);

  if (((instance->kill_code == '\0') &&
       (pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY),
        pending_deaths != 0)) &&
      (pending_deaths = pending_deaths & 0xff, pending_deaths != 0))
  {
    do
    {
      lose_life(1 - player, 1);
      pending_deaths = pending_deaths - 1;
    } while (pending_deaths != 0);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22dcfa0; original cards/amonkhet.c:2220. */
int card_ruthless_sniper(int player, int card, event_t event)
{
  /* Ruthless Sniper	|B
   * Creature - Human Archer 1/2
   * Whenever you cycle or discard a card, you may pay |1. If you do, put a -1/-1 counter on target creature. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  result = discard_trigger(player, card, event, player, 2, 0);
  if (result != 0)
  {
    result = has_mana(player, 7, 1);
    if (result != 0)
    {
      result = charge_mana(player, COLOR_COLORLESS, 1);
      if (result != 0)
      {

        default_target_definition(player, card, &td, 2);
        td.preferred_controller = 1 - player;
        td.allow_cancel = 0;
        instance = get_card_instance(player, card);
        instance->number_of_targets = '\0';
        result = can_target(&td);
        if (result != 0)
        {
          result = pick_target(&td, "TARGET_CREATURE");
          if (result != 0)
          {
            add_counters(player, card, instance->targets[0].player,
                                     instance->targets[0].card, COUNTER_M1_M1, 1);
          }
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22dd0c0; original cards/amonkhet.c:2244. */
int card_scarab_feast(int player, int card, event_t event)
{
  /* Scarab Feast	|B
   * Instant
   * Exile up to three target cards from a single graveyard.
   * Cycling |B */

  int decompose_result;
  int result;

  decompose_result = card_decompose(player, card, event);
  result = 1;
  if (decompose_result == 0)
  {
    result = cycling(player, card, event, 0, 1, 0, 0, 0, 0);
  }
  return result;
}

/* Shadow of the Grave	|1|B
 * Instant
 * Return to your hand all cards in your graveyard that you cycled or discarded this turn. */

/* ManalinkEh 2022: 0x22dd140; original cards/amonkhet.c:2260. */
int card_soulstinger(int player, int card, event_t event)
{
  /* Soulstinger	|3|B
   * Creature - Scorpion Demon 4/5
   * When ~ enters the battlefield, put two -1/-1 counters on target creature you control.
   * When ~ dies, you may put a -1/-1 counter on target creature for each -1/-1 counter on ~. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_M1_M1, 2);
      }
    }
    instance->number_of_targets = '\0';
  }
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {

    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_CREATURE");
      if (result != 0)
      {
        result = count_counters(player, card, COUNTER_M1_M1);
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_M1_M1, result);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22dd2e0; original cards/amonkhet.c:2294. */
int card_splendid_agony(int player, int card, event_t event)
{
  /* Splendid Agony	|2|B
   * Instant
   * Distribute two -1/-1 counters among one or two target creatures. */

  card_instance_t *instance;
  int i;
  target_definition_t td2;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = 1 - player;
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td2, NULL, 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td2, 2);
      td2.preferred_controller = 1 - player;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        if (check_special_flags2(player, card, 0x2000) == 0)
        {
          check_special_flags2(player, card, 0x8000);
        }
        instance->number_of_targets = '\0';
        if (new_pick_target(&td2, "Select target creature for the first -1/-1 counter.", 0, 0x81) !=
            0)
        {
          new_pick_target(&td2, "Select target creature for the second -1/-1 counter.", 1, 0x81);
        }
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = 1 - player;
    instance = get_card_instance(player, card);

    i = 0;
    if (event == EVENT_RESOLVE_SPELL)
    {
      for (i = 0; i < 2; ++i)
        if (validate_target(player, card, &td2, i))
          add_counters(player, card, instance->targets[i].player, instance->targets[i].card,
                       COUNTER_M1_M1, 1);
      kill_card(player, card, KILL_DESTROY);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22dd550; original cards/amonkhet.c:2372. */
int card_stir_the_sands(int player, int card, event_t event)
{
  /* Stir the Sands	|4|B|B
   * Sorcery
   * Create three 2/2 |Sblack Zombie creature tokens.
   * Cycling |3|B
   * When you cycle ~, create a 2/2 |Sblack Zombie creature token. */

  if (1 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    if (event != EVENT_RESOLVE_ACTIVATION_FROM_HAND)
    {
      if (event != EVENT_CAN_CAST)
      {
        if (event == EVENT_CAST_SPELL)
        {
          if ((card != affected_card) || (player != affected_card_controller))
            return cycling(player, card, event, 3, 1, 0, 0, 0, 0);
        }
        else if (event == EVENT_RESOLVE_SPELL)
        {
          generate_tokens_by_id(player, card, CARD_ID_ZOMBIE, 3);
          kill_card(player, card, KILL_DESTROY);
        }
        else if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
          return cycling(player, card, event, 3, 1, 0, 0, 0, 0);
      }
      return basic_spell(player, card, event);
    }
    if ((((unsigned int)player_bits[player] >> 24 & 0xff) & 2) == 0)
    {
      generate_tokens_by_id(player, card, CARD_ID_ZOMBIE, 1);
    }
  }
  return cycling(player, card, event, 3, 1, 0, 0, 0, 0);
}

/* Supernatural Stamina	|B
 * Instant
 * Until end of turn, target creature gets +2/+0 and gains "When this creature dies, return it to the battlefield tapped under its owner's control." */

/* ManalinkEh 2022: 0x22dd6a0; original cards/amonkhet.c:2402. */
int card_trespassers_curse(int player, int card, event_t event)
{
  /* Trespasser's Curse	|1|B
   * Enchantment - Aura Curse
   * Enchant player
   * Whenever a creature enters the battlefield under enchanted player's control, that player loses 1 life and you gain 1 life. */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = get_card_instance(player, card);
    if (instance->targets[0].player != -1)
    {

      instance = get_card_instance(player, card);
      etb_ability_global(instance->targets[0].player, event, 2, TYPE_CREATURE, NULL, player, card);
      if (event == EVENT_ETB)
      {
        lose_life(instance->targets[0].player, 1);
        gain_life(player, 1);
      }
    }
  }
  result = curse(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22dd770; original cards/amonkhet.c:2421. */
int card_trial_of_ambition(int player, int card, event_t event)
{
  /* Trial of Ambition	|1|B
   * Enchantment
   * When ~ enters the battlefield, target opponent sacrifices a creature.
   * When a Cartouche enters the battlefield under your control, return ~ to its owner's hand. */

  int result;
  card_instance_t *instance;
  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    result = target_opponent(player, card);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      player_sacrifices_a_permanent(player, card, instance->targets[0].player, 2, SAC_NO_CANCEL);
    }
  }
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 4);
    test.subtype = 0x4011;
    result = new_specific_cip(player, card, event, player, 2, &test);
    if (result != 0)
    {
      bounce_permanent(player, card);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22dd880; original cards/amonkhet.c:2449. */
int card_wander_in_death(int player, int card, event_t event)
{
  /* Wander in Death	|2|B
   * Sorcery
   * Return up to two target creature cards from your graveyard to your hand.
   * Cycling |2 */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = card_morbid_plunder(player, card, event);
    return result;
  }
  return cycling(player, card, event, 2, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22dd8f0; original cards/amonkhet.c:2461. */
int card_wasteland_scorpion(int player, int card, event_t event)
{
  /* Wasteland Scorpion	|2|B
   * Creature - Scorpion 2/2
   * Deathtouch
   * Cycling |2 */

  int result;
  deathtouch(player, card, event);
  result = cycling(player, card, event, 2, 0, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22dd960; original cards/amonkhet.c:2471. */
int card_ahn_crop_crasher(int player, int card, event_t event)
{
  /* Ahn-Crop Crasher	|2|R
   * Creature - Minotaur Warrior 3/2
   * Haste
   * You may exert ~ as it attacks. When you do, target creature can't block this turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  haste(player, card, event);
  result = duh_mode(player);
  result = declare_attackers_trigger(player, card, event,
                                     (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, player, card);
  if (result != 0)
  {

    result = does_not_untap_effect(player, card, player, card, 0, 1);
    result = alternate_legacy_text(2, player, result);
    if (result != 0)
    {

      default_target_definition(player, card, &td, 2);
      td.allow_cancel = 0;
      instance = get_card_instance(player, card);
      result = can_target(&td);
      if (result != 0)
      {
        result = pick_target(&td, "TARGET_CREATURE");
        if (result != 0)
        {
          pump_ability_until_eot(player, card, instance->targets[0].player,
                                 instance->targets[0].card, 0, 0, 0, 0x20);
        }
      }
      instance->number_of_targets = '\0';
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22ddab0; original cards/amonkhet.c:2497. */
int card_battlefield_scavenger(int player, int card, event_t event)
{
  /* Battlefield Scavenger	|1|R
   * Creature - Jackal Rogue 2/2
   * You may exert ~ as it attacks.
   * Whenever you exert a creature, you may discard a card. If you do, draw a card. */

  declare_attackers_trigger_t mode;
  int result;
  test_definition_t this_test;

  if (hand_count[player] == 0)
  {
    mode = 0;
  }
  else
  {
    result = duh_mode(player);
    mode = (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL;
  }
  result = declare_attackers_trigger(player, card, event, mode, player, card);
  if (result != 0)
  {

    result = does_not_untap_effect(player, card, player, card, 0, 1);
    result = alternate_legacy_text(2, player, result);
    if (result != 0)
    {

      new_default_test_definition(&this_test, 0xff, "Select a card to discard.");
      result = new_select_a_card(player, player, 4, 0, 3, -1, &this_test);
      if (result != -1)
      {
        new_discard_card(player, result, player, 0);
        draw_cards(player, 1);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22ddbe0; original cards/amonkhet.c:2518. */
int card_blazing_volley(int player, int card, event_t event)
{
  /* Blazing Volley	|R
   * Sorcery
   * ~ deals 1 damage to each creature your opponents control. */

  int result;
  if (event == EVENT_RESOLVE_SPELL)
  {
    new_damage_all(player, card, 1 - player, 1, 1, NULL);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* Bloodlust Inciter	|R	=>saviors_of_kamigawa.c:card_akki_drillmaster 0x2006330
 * Creature - Human Warrior 1/1
 * |T: Target creature gains haste until end of turn. */

/* ManalinkEh 2022: 0x22ddc60; original cards/amonkhet.c:2535. */
int card_bloodrage_brawler(int player, int card, event_t event)
{
  /* Bloodrage Brawler	|1|R
   * Creature - Minotaur Warrior 4/3
   * When ~ enters the battlefield, discard a card. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  discard(player, 0, player);
  return 0;
}

/* ManalinkEh 2022: 0x22ddcc0; original cards/amonkhet.c:2551. */
int card_by_force(int player, int card, event_t event)
{
  /* By Force	|X|R
   * Sorcery
   * Destroy X target artifacts. */

  int i;
  target_t *target_cursor;
  target_t *other_target_cursor;
  card_instance_t *instance;
  int available_or_valid;
  int target_count;
  target_definition_t td;

  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 0x40);
  td.allow_cancel = 3;
  if (player == 1)
  {
    td.preferred_controller = 0;
    td.illegal_abilities = get_protections_from(1, card);
  }
  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 5, &td, NULL, 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      set_special_flags2(player, card, 0x20000);
      target_count = has_mana(player, 7, 1);

      available_or_valid = target_available(-1, card, &td);
      if (available_or_valid < target_count)
      {
        target_count = available_or_valid;
      }
      target_count = pick_up_to_n_targets(&td, "TARGET_ARTIFACT", target_count);
      if (target_count == 0)
      {
        spell_fizzled = 1;
      }
      else
      {

        available_or_valid = played_for_free(player, card);
        if ((available_or_valid == 0) &&
            (available_or_valid = is_token(player, card), available_or_valid == 0))
        {
          instance->info_slot = 0;
          if (player == 0)
          {

            set_x_for_x_spells(0, card, EVENT_CAST_SPELL,
                               charge_mana(0, COLOR_COLORLESS, target_count));
          }
          else
          {

            target_count = target_available(player, card, &td);
            while (available_or_valid = has_mana(player, 7, target_count), available_or_valid == 0)
            {
              target_count = target_count + -1;
            }
            if (target_count == 0)
            {
              ai_modifier = ai_modifier + -0x32;
            }
            charge_mana(player, COLOR_ANY, target_count);
            if (spell_fizzled != 1)
            {
              set_x_for_x_spells(player, card, EVENT_CAST_SPELL, target_count);
            }
          }
        }
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {

    if (instance->number_of_targets != '\0')
    {
      i = 0;
      do
      {
        if (available_or_valid = validate_target(player, card, &td, i), available_or_valid == 0)
        {
          i = i + 1;
          continue;
        }
        other_target_cursor = instance->targets + i;
        target_cursor = instance->targets + i;
        i = i + 1;
        kill_card(target_cursor->player, other_target_cursor->card, KILL_DESTROY);
      } while (i < (int)(unsigned int)instance->number_of_targets);
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22ddfd0; original cards/amonkhet.c:2614. */
int card_cartouche_of_zeal(int player, int card, event_t event)
{
  /* Cartouche of Zeal	|R
   * Enchantment - Aura Cartouche
   * Enchant creature you control
   * When ~ enters the battlefield, target creature can't block this turn.
   * Enchanted creature gets +1/+1 and has haste. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if ((result != 0) && (result = new_pick_target(&td, "TARGET_CREATURE", 1, 0), result != 0))
    {
      pump_ability_until_eot(player, card, instance->targets[1].player, instance->targets[1].card,
                             0, 0, 0, 0x20);
    }
    instance->number_of_targets = '\0';
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  attached_gets_pt(player, card, event, 1, 1);
  attached_gets_kw(player, card, event, KEYWORD_0, SP_KEYWORD_HASTE);
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = player;
  return aura(player, card, event, &td, "TARGET_CREATURE_YOU_CONTROL");
}
#endif

/* ManalinkEh 2022: 0x22de200; original cards/amonkhet.c:2649. */
int card_combat_celebrant(int player, int card, event_t event)
{
  /* Combat Celebrant	|2|R
   * Creature - Human Warrior 4/1
   * If ~ hasn't been exerted this turn, you may exert it as it attacks. When you do, untap all other creatures you control and after this phase, there is an additional combat phase. */

  card_instance_t *instance;
  int result;
  test_definition_t this_test;

  instance = get_card_instance(player, card);
  result = duh_mode(player);
  result = declare_attackers_trigger(player, card, event,
                                     (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, player, card);
  if ((result != 0) && (instance->info_slot == 0))
  {

    result = does_not_untap_effect(player, card, player, card, 0, 1);
    result = alternate_legacy_text(2, player, result);
    if (result != 0)
    {
      instance->info_slot = 0x42;
      default_test_definition(&this_test, 2);
      this_test.not_me = 1;
      new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
      extra_combat_effect(player, card, event, 0, NULL, 1);
    }
  }
  if (event == EVENT_CLEANUP)
  {
    instance->info_slot = 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22de330; original cards/amonkhet.c:2674. */
int card_consuming_fervor(int player, int card, event_t event)
{
  /* Consuming Fervor	|R
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +3/+3 and has "At the beginning of your upkeep, put a -1/-1 counter on this creature." */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player != -1)
    {

      instance = get_card_instance(player, card);
      result = is_humiliated((int)instance->damage_target_player,
                                         instance->damage_target_card, AL_TRIG);
      if (result == 0)
      {
        upkeep_trigger_ability(player, card, event, player);
        if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
        {
          add_counters(player, card, (int)instance->damage_target_player,
                                   instance->damage_target_card, COUNTER_M1_M1, 1);
        }
      }
    }
  }
  result = generic_aura(player, card, event, player, 3, 3, 0, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22de440; original cards/amonkhet.c:2696. */
int card_deem_worthy(int player, int card, event_t event)
{
  /* Deem Worthy	|4|R
   * Instant
   * ~ deals 7 damage to target creature.
   * Cycling |3|R
   * When you cycle ~, you may have it deal 2 damage to target creature. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if ((uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND) < 2)
  {
    return cycling(player, card, event, 3, 0, 0, 0, 1, 0);
  }
  if (event == EVENT_RESOLVE_ACTIVATION_FROM_HAND)
  {
    if ((spell_fizzled != 1) && ((((unsigned int)player_bits[player] >> 24 & 0xff) & 2) == 0))
    {

      instance = get_card_instance(player, card);
      default_target_definition(player, card, &td, 2);
      result = can_target(&td);
      if ((result != 0) && (result = pick_target(&td, "TARGET_CREATURE"), result != 0))
      {
        damage_creature(instance->targets[0].player, instance->targets[0].card, 2, player, card);
      }
    }
    return cycling(player, card, event, 3, 0, 0, 0, 1, 0);
  }
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card != affected_card) || (player != affected_card_controller))
        return cycling(player, card, event, 3, 0, 0, 0, 1, 0);
    }
    else
    {
      if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
      {
        default_target_definition(player, card, &td, 2);
        if (event == EVENT_RESOLVE_SPELL)
        {
          result = valid_target(&td);
          if (result != 0)
          {
            damage_target0(player, card, 7);
          }
          kill_card(player, card, KILL_DESTROY);
        }
        return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
      }
      if (event != EVENT_CHANGE_TARGET)
        return cycling(player, card, event, 3, 0, 0, 0, 1, 0);
    }
  }
  default_target_definition(player, card, &td, 2);
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x22de680; original cards/amonkhet.c:2734. */
int card_desert_cerodon(int player, int card, event_t event)
{
  /* Desert Cerodon	|5|R
   * Creature - Beast 6/4
   * Cycling |R */

  int result;
  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    return 0;
  }
  result = cycling(player, card, event, 0, 0, 0, 0, 1, 0);
  return result;
}

/* Electrify	|3|R	=>rise_of_the_eldrazi.c:card_flame_slash 0x200547b
 * Instant
 * ~ deals 4 damage to target creature. */

/* ManalinkEh 2022: 0x22de6e0; original cards/amonkhet.c:2749. */
int card_emberhorn_minotaur(int player, int card, event_t event)
{
  /* Emberhorn Minotaur	|3|R
   * Creature - Minotaur Warrior 4/3
   * You may exert ~ as it attacks. When you do, it gets +1/+1 and gains menace until end of turn. */

  int result;

  result = duh_mode(player);
  result = declare_attackers_trigger(player, card, event,
                                     (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, player, card);
  if (result != 0)
  {

    result = does_not_untap_effect(player, card, player, card, 0, 1);
    result = alternate_legacy_text(2, player, result);
    if (result != 0)
    {
      result = pump_ability_until_eot(player, card, player, card, 1, 1, 0, 0x10000);
      alternate_legacy_text(1, player, result);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22de7c0; original cards/amonkhet.c:2763. */
int card_flameblade_adept(int player, int card, event_t event)
{
  /* Flameblade Adept	|R
   * Creature - Jackal Warrior 1/2
   * Menace
   * Whenever you cycle or discard a card, ~ gets +1/+0 until end of turn. */

  if (!discard_trigger(player, card, event, player, 2, 0))
  {
    return 0;
  }
  pump_until_eot_merge_previous(player, card, player, card, 1, 0);
  return 0;
}

/* ManalinkEh 2022: 0x22d7410; original cards/amonkhet.c:2777. */
static int glorious_end_legacy(int player, int card, event_t event)
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
        lose_the_game(player);
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

/* ManalinkEh 2022: 0x22de840; original cards/amonkhet.c:2795. */
int card_glorious_end(int player, int card, event_t event)
{
  /* Glorious End	|2|R
   * Instant
   * End the turn.
   * At the beginning of your next end step, you lose the game. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {

    end_the_turn(player, card, EVENT_RESOLVE_SPELL);
    kill_card(player, card, KILL_EXILE);
    result = create_legacy_effect(player, card, glorious_end_legacy);
    if ((current_turn == player) && (current_phase == 0x1f))
    {
      instance = get_card_instance(player, result);
      instance->targets[1].player = 1;
    }
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22de8e0; original cards/amonkhet.c:2813. */
int card_glorybringer(int player, int card, event_t event)
{
  /* Glorybringer	|3|R|R
   * Creature - Dragon 4/4
   * Flying, haste
   * You may exert ~ as it attacks. When you do, it deals 4 damage to target non-Dragon creature an opponent controls. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  haste(player, card, event);
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  td.required_subtype = 0x30;
  td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
  td.allow_cancel = 1;
  td.preferred_controller = td.allowed_controller;
  result = duh_mode(player);
  result = declare_attackers_trigger(player, card, event,
                                     (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, player, card);
  if (result != 0)
  {
    result = can_target(&td);
    if (result != 0)
    {
      new_pick_target(&td, "Choose a non-Dragon creature an opponent controls", -1, 0x80);
      if (spell_fizzled != 1)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          damage_target0(player, card, 4);
          does_not_untap_effect(player, card, player, card, 8, 1);
          instance = get_card_instance(player, card);
          instance->number_of_targets = '\0';
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22dea10; original cards/amonkhet.c:2840. */
int card_harsh_mentor(int player, int card, event_t event)
{
  /* Harsh Mentor	|1|R
   * Creature - Human Cleric 2/2
   * Whenever an opponent activates an ability of an artifact, creature, or land on the battlefield, if it isn't a mana ability, ~ deals 2 damage to that player. */

  card_instance_t *instance;

  if (((event == EVENT_PLAY_ABILITY) || (event == EVENT_TAPPED_TO_PLAY_ABILITY)) &&
      (1 - player == affected_card_controller))
  {
    if (is_what(1 - player, affected_card, 0x43))
    {
      instance = get_card_instance(affected_card_controller, affected_card);
      if (((cards_data[instance->internal_card_id].extra_ability & 0x1000) == 0) ||
          (tapped_for_mana_color < 0))
      {
        instance = in_play(player, card);
        if (instance != NULL)
        {
          if (!is_humiliated(player, card, AL_TRIG))
          {
            damage_player(affected_card_controller, 2, player, card);
          }
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22deaf0; original cards/amonkhet.c:2858. */
int card_hazoret_the_fervent(int player, int card, event_t event)
{
  /* Hazoret the Fervent	|3|R
   * Legendary Creature - God 5/4
   * Indestructible, haste
   * ~ can't attack or block unless you have one or fewer cards in hand.
   * |2|R, Discard a card: ~ deals 2 damage to each opponent. */

  int result;

  check_legend_rule(player, card, event);
  indestructible(player, card, event);
  haste(player, card, event);
  if (event == EVENT_ATTACK_LEGALITY)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (1 < hand_count[player]))
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        event_result = 1;
      }
    }
  }
  else if (event == EVENT_ABILITIES)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (1 < hand_count[player]))
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        cannot_block(player, card, EVENT_ABILITIES);
      }
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    damage_player(1 - player, 2, player, card);
  }
  result = generic_activated_ability(player, card, event, 0x20, 2, 0, 0, 0, 1, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x22dec90; original cards/amonkhet.c:2888. */
int card_hazorets_favor(int player, int card, event_t event)
{
  /* Hazoret's Favor	|2|R
   * Enchantment
   * At the beginning of combat on your turn, you may have target creature you control get +2/+0 and gain haste until end of turn. If you do, sacrifice it at the beginning of the next end step. */

  int result;
  card_instance_t *instance;
  card_instance_t *target_record;
  target_definition_t td;

  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {

    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {

      result = do_dialog(player, player, card, -1, -1, " Boost creature you control\n Decline", 0);
      if ((result == 0) && (spell_fizzled != 1))
      {
        result = new_pick_target(&td, "Select target creature you control.", 0, 0x80);
        if (result != 0)
        {

          result = pump_ability_until_eot(player, card, instance->targets[0].player,
                                          instance->targets[0].card, 2, 0, 0, 4);
          if (result != 1)
          {
            haste_and_remove_at_beginning_of_next_end_step(
                player, card, instance->targets[0].player, instance->targets[0].card, 3);
            target_record =
                get_card_instance(instance->targets[0].player, instance->targets[0].card);
            target_record->regen_status = target_record->regen_status | 0x8000000;
            get_abilities(instance->targets[0].player, instance->targets[0].card, EVENT_ABILITIES,
                          -1);
          }
        }
      }
    }
    instance->number_of_targets = '\0';
  }
  return global_enchantment(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22dee30; original cards/amonkhet.c:2921. */
int card_heart_piercer_manticore(int player, int card, event_t event)
{
  /* Heart-Piercer Manticore	|2|R|R
   * Creature - Manticore 4/3
   * When ~ enters the battlefield, you may sacrifice another creature. When you do, ~ deals damage equal to that creature's power to any target.
   * Embalm |5|R */

  int result;
  unsigned int chosen_sacrifice;
  unsigned int player_00;
  target_definition_t td2;
  test_definition_t test;
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 2, "Select another creature to sacrifice.");
    test.not_me = 1;
    default_target_definition(player, card, &td2, 0x102);
    td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    chosen_sacrifice = new_sacrifice(player, card, player,
                                     SAC_RETURN_CHOICE | SAC_JUST_MARK | SAC_NO_CANCEL, &test);
    if (chosen_sacrifice != 0)
    {
      result = can_target(&td2);
      if (result != 0)
      {
        result = new_pick_target(&td2, "TARGET_ANY", 0, 0);
        if (result != 0)
        {
          player_00 = (int)chosen_sacrifice >> 0x10 & 0xff;
          result = get_power(player_00, chosen_sacrifice >> 0x18);
          kill_card(player_00, chosen_sacrifice >> 0x18, KILL_SACRIFICE);
          damage_target0(player, card, result);
        }
      }
    }
  }
  return embalm(player, card, event, 0x422a, 5, 0, 0, 0, 1, 0);
}
#endif

/* Hyena Pack	|2|R|R	0x401000
 * Creature - Hyena 3/4 */

/* ManalinkEh 2022: 0x22defe0; original cards/amonkhet.c:2955. */
int card_limits_of_solidarity(int player, int card, event_t event)
{
  /* Limits of Solidarity	|3|R
   * Sorcery
   * Gain control of target creature until end of turn. Untap that creature. It gains haste until end of turn.
   * Cycling |2 */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = card_act_of_treason(player, card, event);
    return result;
  }
  return cycling(player, card, event, 2, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22df050; original cards/amonkhet.c:2969. */
int card_manticore_of_the_gauntlet(int player, int card, event_t event)
{
  /* Manticore of the Gauntlet	|4|R
   * Creature - Manticore 5/4
   * When ~ enters the battlefield, put a -1/-1 counter on target creature you control. ~ deals 3 damage to target opponent or planeswalker. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  target_definition_t td2;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_M1_M1, 1);
      }
    }
    instance->number_of_targets = '\0';
    opponent_or_pw_target_definition(player, card, &td2);
    result = can_target(&td2);
    if (result != 0)
    {
      result = new_pick_target(&td2, "TARGET_OPPONENT_OR_PLANESWALKER", 0, 1);
      if (result != 0)
      {
        damage_target0(player, card, 3);
      }
    }
  }
  return 0;
}

/* Minotaur Sureshot	|2|R	=>return_to_ravnica.c:card_bellows_lizard 0x200760e
 * Creature - Minotaur Archer 2/3
 * Reach
 * |1|R: ~ gets +1/+0 until end of turn. */

/* ManalinkEh 2022: 0x22df1c0; original cards/amonkhet.c:3005. */
int card_nef_crop_entangler(int player, int card, event_t event)
{
  /* Nef-Crop Entangler	|1|R
   * Creature - Human Warrior 2/1
   * Trample
   * You may exert ~ as it attacks. When you do, it gets +1/+2 until end of turn. */

  int result;

  result = duh_mode(player);
  result = declare_attackers_trigger(player, card, event,
                                     (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, player, card);
  if (result == 0)
  {
    return 0;
  }
  result = pump_until_eot(player, card, player, card, 1, 2);
  alternate_legacy_text(1, player, result);
  result = does_not_untap_effect(player, card, player, card, 8, 1);
  alternate_legacy_text(2, player, result);
  return 0;
}

/* Nimble-Blade Khenra	|1|R	=>khans_of_tarkir.c:card_jeskai_windscout 0x200ce74
 * Creature - Jackal Warrior 1/3
 * Prowess */

/* Pathmaker Initiate	|1|R	=>unlimited.c:card_dwarven_warriors 0x200bf9c
 * Creature - Human Wizard 2/1
 * |T: Target creature with power 2 or less can't be blocked this turn. */

/* ManalinkEh 2022: 0x22df290; original cards/amonkhet.c:3020. */
int card_pursue_glory(int player, int card, event_t event)
{
  /* Pursue Glory	|3|R
   * Instant
   * Attacking creatures get +2/+0 until end of turn.
   * Cycling |2 */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = card_trumpet_blast(player, card, event);
    return result;
  }
  return cycling(player, card, event, 2, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22df300; original cards/amonkhet.c:3032. */
int card_soul_scar_mage(int player, int card, event_t event)
{
  /* Soul-Scar Mage	|R
   * Creature - Human Wizard 1/2
   * Prowess
   * If a source you control would deal noncombat damage to a creature an opponent controls, put that many -1/-1 counters on that creature instead. */

  int damage_to_prevent;
  card_instance_t *instance;

  prowess(player, card, event);
  instance = noncombat_damage_being_prevented(event);
  if (((instance != NULL) && ((int)instance->damage_target_player == 1 - player)) &&
      (instance->damage_target_card != -1))
  {

    damage_to_prevent = instance->info_slot;
    prevent_damage(affected_card_controller, affected_card, damage_to_prevent);
    add_counters(player, card, (int)instance->damage_target_player,
                             instance->damage_target_card, COUNTER_M1_M1, damage_to_prevent);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22df3b0; original cards/amonkhet.c:3049. */
int card_sweltering_suns(int player, int card, event_t event)
{
  /* Sweltering Suns	|1|R|R
   * Sorcery
   * ~ deals 3 damage to each creature.
   * Cycling |3 */

  int result;
  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      new_damage_all(player, card, 2, 3, 1, NULL);
      kill_card(player, card, KILL_DESTROY);
    }
    result = basic_spell(player, card, event);
    return result;
  }
  result = cycling(player, card, event, 3, 0, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22df480; original cards/amonkhet.c:3065. */
int card_thresher_lizard(int player, int card, event_t event)
{
  /* Thresher Lizard	|2|R
   * Creature - Lizard 3/2
   * ~ gets +1/+2 as long as you have one or fewer cards in hand. */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((result == 0) && (hand_count[player] < 2))
    {
      if (event == EVENT_POWER)
      {
        if ((card == affected_card) && (player == affected_card_controller))
        {
          event_result = event_result + 1;
        }
      }
      else if (((event == EVENT_TOUGHNESS) && (card == affected_card)) &&
               (player == affected_card_controller))
      {
        event_result = event_result + 2;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22df520; original cards/amonkhet.c:3083. */
int card_trial_of_zeal(int player, int card, event_t event)
{
  /* Trial of Zeal	|2|R
   * Enchantment
   * When ~ enters the battlefield, it deals 3 damage to any target.
   * When a Cartouche enters the battlefield under your control, return ~ to its owner's hand. */

  int result;
  card_instance_t *instance;


  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
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
        damage_target0(player, card, 3);
      }
    }
    instance = get_card_instance(player, card);
    instance->number_of_targets = '\0';
  }
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    test_definition_t test;
    default_test_definition(&test, 4);
    test.subtype = SUBTYPE_CARTOUCHE;
    result = new_specific_cip(player, card, event, player, 2, &test);
    if (result != 0)
    {
      bounce_permanent(player, card);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* Trueheart Twins	|4|R
 * Creature - Jackal Warrior 4/4
 * You may exert ~ as it attacks.
 * Whenever you exert a creature, creatures you control get +1/+0 until end of turn. */

/* ManalinkEh 2022: 0x22df680; original cards/amonkhet.c:3123. */
int card_violent_impact(int player, int card, event_t event)
{
  /* Violent Impact	|3|R
   * Sorcery
   * Destroy target artifact or land.
   * Cycling |2 */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = card_demolish(player, card, event);
    return result;
  }
  return cycling(player, card, event, 2, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22df6f0; original cards/amonkhet.c:3135. */
int card_warfire_javelineer(int player, int card, event_t event)
{
  /* Warfire Javelineer	|3|R
   * Creature - Minotaur Warrior 2/3
   * When ~ enters the battlefield, it deals X damage to target creature an opponent controls, where X is the number of instant and sorcery cards in your graveyard. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  td.allow_cancel = 0;
  td.preferred_controller = td.allowed_controller;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "Select target creature an opponent controls.", 0, 0x80);
    if (result != 0)
    {
      result = count_graveyard_by_type(player, 0x38);
      damage_creature(instance->targets[0].player, instance->targets[0].card, result, player, card);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x22df7f0; original cards/amonkhet.c:3160. */
int card_benefaction_of_rhonas(int player, int card, event_t event)
{
  /* Benefaction of Rhonas	|2|G
   * Sorcery
   * Reveal the top five cards of your library. You may put a creature card and/or an enchantment card from among them into your hand. Put the rest into your graveyard. */

  int remaining_top_cards;
  int first_selected;
  unsigned int second_selected;
  test_definition_t test;
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      ai_modifier = ai_modifier + 0x30;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    if (*deck_ptr[player] != -1)
    {
      remaining_top_cards = count_deck(player);
      if (5 < remaining_top_cards)
      {
        remaining_top_cards = 5;
      }
      new_default_test_definition(&test, 2, "Select a creature card.");
      test.create_minideck = (unsigned short)remaining_top_cards;
      test.no_shuffle = 1;
      first_selected = new_global_tutor(player, player, 1, 1, 0, 1, &test);
      remaining_top_cards = remaining_top_cards - (unsigned int)(first_selected != -1);
      if (*deck_ptr[player] != -1)
      {
        new_default_test_definition(&test, 4, "Select an enchantment card.");
        test.create_minideck = (unsigned short)remaining_top_cards;
        test.no_shuffle = 1;
        second_selected = new_global_tutor(player, player, 1, 1, 0, 1, &test);
        remaining_top_cards = remaining_top_cards - (unsigned int)(second_selected < 0x80000000);
      }
      if (0 < remaining_top_cards)
      {
        mill(player, remaining_top_cards);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x22df980; original cards/amonkhet.c:3203. */
int card_bitterblade_warrior(int player, int card, event_t event)
{
  /* Bitterblade Warrior	|1|G
   * Creature - Jackal Warrior 2/2
   * You may exert ~ as it attacks. When you do, it gets +1/+0 and gains deathtouch until end of turn. */

  int result;

  result = duh_mode(player);
  result = declare_attackers_trigger(player, card, event,
                                     (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, player, card);
  if (result != 0)
  {

    result = does_not_untap_effect(player, card, player, card, 0, 1);
    result = alternate_legacy_text(2, player, result);
    if (result != 0)
    {
      result = pump_ability_until_eot(player, card, player, card, 1, 0, 0, 0x200);
      alternate_legacy_text(1, player, result);
      return 0;
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22dfa60; original cards/amonkhet.c:3217. */
int card_cartouche_of_strength(int player, int card, event_t event)
{
  /* Cartouche of Strength	|2|G
   * Enchantment - Aura Cartouche
   * Enchant creature you control
   * When ~ enters the battlefield, you may have enchanted creature fight target creature an opponent controls.
   * Enchanted creature gets +1/+1 and has trample. */

  int t_card;
  char attached_player;
  int duh;
  card_instance_t *instance;
  int selected;
  target_definition_t td2;

  duh = duh_mode(player);
  etb_ability(player, card, event, (duh != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {

    instance = get_card_instance(player, card);
    attached_player = instance->damage_target_player;
    t_card = instance->damage_target_card;
    default_target_definition(player, card, &td2, 2);
    td2.allowed_controller = 1 - player;
    td2.allow_cancel = 0;
    td2.preferred_controller = td2.allowed_controller;
    selected = can_target(&td2);
    if ((selected != 0) &&
        (selected = new_pick_target(&td2, "TARGET_CREATURE_OPPONENT_CONTROLS", 0, 1),
         selected != 0))
    {
      fight((int)attached_player, t_card, instance->targets[0].player, instance->targets[0].card);
    }
    instance->number_of_targets = '\0';
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  attached_gets_pt(player, card, event, 1, 1);
  attached_gets_kw(player, card, event, KEYWORD_TRAMPLE, SP_KEYWORD_0);
  default_target_definition(player, card, &td2, 2);
  td2.allowed_controller = player;
  return aura(player, card, event, &td2, "TARGET_CREATURE_YOU_CONTROL");
}
#endif

/* ManalinkEh 2022: 0x22dfca0; original cards/amonkhet.c:3256. */
int card_champion_of_rhonas(int player, int card, event_t event)
{
  /* Champion of Rhonas	|3|G
   * Creature - Jackal Warrior 3/3
   * You may exert ~ as it attacks. When you do, you may put a creature card from your hand onto the battlefield. */

  int result;
  test_definition_t test;

  result = duh_mode(player);
  result = declare_attackers_trigger(player, card, event,
                                     (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, player, card);
  if (result != 0)
  {

    result = does_not_untap_effect(player, card, player, card, 0, 1);
    if (result != 0)
    {
      new_default_test_definition(&test, 2, "Select a creature card to put onto the battlefield.");
      new_global_tutor(player, player, 4, 4, 0, 2, &test);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22dfd80; original cards/amonkhet.c:3272. */
int card_channeler_initiate(int player, int card, event_t event)
{
  /* Channeler Initiate	|1|G
   * Creature - Human Druid 3/4
   * When ~ enters the battlefield, put three -1/-1 counters on target creature you control.
   * |T, Remove a -1/-1 counter from ~: Add one mana of any color. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    td.preferred_controller = player;
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if ((result != 0) &&
        (result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0), result != 0))
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                               COUNTER_M1_M1, 3);
    }
    instance->number_of_targets = '\0';
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    result = is_tapped(player, card);
    if (((result == 0) && (result = is_animated_and_sick(player, card), result == 0)) &&
        (result = can_produce_mana(player, card), result != 0))
    {
      result = count_counters(player, card, COUNTER_M1_M1);
      return (unsigned int)(0 < result);
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    result = produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 1);
    if (result != 0)
    {
      remove_counters(player, card, player, card, COUNTER_M1_M1, 1);
    }
  }
  else if (((((event == EVENT_COUNT_MANA) && (card == affected_card)) &&
             ((player == affected_card_controller &&
               ((result = is_tapped(player, card),
                 result == 0 && (result = is_animated_and_sick(player, card), result == 0)))))) &&
            (result = can_produce_mana(player, card), result != 0)) &&
           (result = count_counters(player, card, COUNTER_M1_M1), 0 < result))
  {
    declare_mana_available_hex(player, 0x3e, 1);
    return 0;
  }
  return 0;
}

/* Colossapede	|4|G	0x401000
 * Creature - Insect 5/5 */

/* ManalinkEh 2022: 0x22dffd0; original cards/amonkhet.c:3312. */
int card_crocodile_of_the_crossing(int player, int card, event_t event)
{
  /* Crocodile of the Crossing	|3|G
   * Creature - Crocodile 5/4
   * Haste
   * When ~ enters the battlefield, put a -1/-1 counter on target creature you control. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  haste(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }

  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.allow_cancel = 0;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0);
    if (result != 0)
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                               COUNTER_M1_M1, 1);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x22e00d0; original cards/amonkhet.c:3339. */
int card_defiant_greatmaw(int player, int card, event_t event)
{
  /* Defiant Greatmaw	|2|G
   * Creature - Hippo 4/5
   * When ~ enters the battlefield, put two -1/-1 counters on target creature you control.
   * Whenever you put one or more -1/-1 counters on ~, remove a -1/-1 counter from another target creature you control. */

  enum
  {
    ENABLE_XTRIGGER_M1_M1_COUNTERS = 64,
    XTRIGGER_M1_M1_COUNTERS = 16780544
  };

  xtrigger_t trigger;
  int result;
  card_instance_t *instance;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    td.preferred_controller = player;
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                     COUNTER_M1_M1, 2);
      }
    }
    instance->number_of_targets = '\0';
  }
  enable_xtrigger_flags = enable_xtrigger_flags | ENABLE_XTRIGGER_M1_M1_COUNTERS;
  trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    trigger = xtrigger_impl_value_dont_use_directly;
  }
  if ((((trigger == XTRIGGER_M1_M1_COUNTERS) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((reason_for_trigger_controller == player && (trigger_cause == card))))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      if (event == EVENT_TRIGGER)
      {
        event_result = event_result | 2;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {

        default_target_definition(player, card, &td, 2);
        td.allowed_controller = player;
        td.special = TARGET_SPECIAL_NOT_ME;
        instance = get_card_instance(player, card);
        result = can_target(&td);
        if (result != 0)
        {
          result = new_pick_target(&td, "Select another target creature you control.", 0, 0x80);
          if (result != 0)
          {
            remove_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                            COUNTER_M1_M1, 1);
          }
        }
        instance->number_of_targets = '\0';
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22e02e0; original cards/amonkhet.c:3387. */
int card_dissenters_deliverance(int player, int card, event_t event)
{
  /* Dissenter's Deliverance	|1|G
   * Instant
   * Destroy target artifact.
   * Cycling |G */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = card_shatter(player, card, event);
    return result;
  }
  return cycling(player, card, event, 0, 0, 0, 1, 0, 0);
}

/* ManalinkEh 2022: 0x22e0350; original cards/amonkhet.c:3399. */
int card_exemplar_of_strength(int player, int card, event_t event)
{
  /* Exemplar of Strength	|1|G
   * Creature - Human Warrior 4/4
   * When ~ enters the battlefield, put three -1/-1 counters on target creature you control.
   * Whenever ~ attacks, remove a -1/-1 counter from it. If you do, you gain 1 life. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    td.preferred_controller = player;
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_M1_M1, 3);
      }
    }
    instance->number_of_targets = '\0';
  }
  result = declare_attackers_trigger(player, card, event, 0, player, card);
  if (result != 0)
  {
    result = count_counters(player, card, COUNTER_M1_M1);
    if (0 < result)
    {
      remove_counters(player, card, player, card, COUNTER_M1_M1, 1);
      if (spell_fizzled != -1)
      {
        gain_life(player, 1);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22e04c0; original cards/amonkhet.c:3433. */
int card_gift_of_paradise(int player, int card, event_t event)
{
  /* Gift of Paradise	|2|G
   * Enchantment - Aura
   * Enchant land
   * When ~ enters the battlefield, you gain 3 life.
   * Enchanted land has "|T: Add two mana of any one color." */

  int result;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    gain_life(player, 3);
  }
  result = enchanted_land_has_T_add_n_mana_of_any_one_color(player, card, event, 2);
  return result;
}

/* ManalinkEh 2022: 0x22e0530; original cards/amonkhet.c:3450. */
int card_greater_sandwurm(int player, int card, event_t event)
{
  /* Greater Sandwurm	|5|G|G
   * Creature - Wurm 7/7
   * ~ can't be blocked by creatures with power 2 or less.
   * Cycling |2 */

  int result;

  if (2 < (uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND))
  {
    result = card_outland_boar(player, card, event);
    return result;
  }
  return cycling(player, card, event, 2, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22e05a0; original cards/amonkhet.c:3462. */
int card_hapatras_mark(int player, int card, event_t event)
{
  /* Hapatra's Mark	|G
   * Instant
   * Target creature you control gains hexproof until end of turn. Remove all -1/-1 counters from it. */

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
    if (valid_target(&td))
    {
      pump_ability_until_eot(player, card, instance->targets[0].player,
                             instance->targets[0].card, 0, 0, 0, (SP_KEYWORD_HEXPROOF));
      remove_all_counters(player, card, instance->targets[0].player,
                                      instance->targets[0].card, COUNTER_M1_M1);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target creature you control.", 1, NULL);
}

/* ManalinkEh 2022: 0x22e0740; original cards/amonkhet.c:3488. */
int card_harvest_season(int player, int card, event_t event)
{
  /* Harvest Season	|2|G
   * Sorcery
   * Search your library for up to X basic land cards, where X is the number of tapped creatures you control, and put those cards onto the battlefield tapped. Then shuffle your library. */

  int result;
  test_definition_t this_test;
  test_definition_t this_test2;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&this_test, 2);
    this_test.state = 0x10;
    result = check_battlefield_for_special_card(player, card, player, 4, &this_test);
    if (0 < result)
    {
      new_default_test_definition(&this_test2, 1, "Select a basic land card.");
      this_test2.subtype = 0x2000;
      this_test2.qty = (unsigned short)result;
      new_global_tutor(player, player, 1, 5, 0, 5, &this_test2);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22e0850; original cards/amonkhet.c:3513. */
int card_haze_of_pollen(int player, int card, event_t event)
{
  /* Haze of Pollen	|1|G
   * Instant
   * Prevent all combat damage that would be dealt this turn.
   * Cycling |3 */

  if (!IS_ACTIVATING_FROM_HAND(event))
    return card_fog(player, card, event);
  return cycling(player, card, event, 3, 0, 0, 0, 0, 0);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22e08c0; original cards/amonkhet.c:3525. */
int card_honored_hydra(int player, int card, event_t event)
{
  /* Honored Hydra	|5|G
   * Creature - Snake Hydra 6/6
   * Trample
   * Embalm |3|G */

  int result;
  result = embalm(player, card, event, 0x4230, 3, 0, 0, 1, 0, 0);
  return result;
}
#endif

/* ManalinkEh 2022: 0x22e0910; original cards/amonkhet.c:3534. */
int card_hooded_brawler(int player, int card, event_t event)
{
  /* Hooded Brawler	|2|G
   * Creature - Naga Warrior 3/2
   * You may exert ~ as it attacks. When you do, it gets +2/+2 until end of turn. */

  int result;

  result = duh_mode(player);
  result = declare_attackers_trigger(player, card, event,
                                     (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, player, card);
  if (result != 0)
  {

    result = does_not_untap_effect(player, card, player, card, 0, 1);
    result = alternate_legacy_text(2, player, result);
    if (result != 0)
    {
      result = pump_ability_until_eot(player, card, player, card, 2, 2, 0, 0);
      alternate_legacy_text(1, player, result);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22e09f0; original cards/amonkhet.c:3548. */
int card_initiates_companion(int player, int card, event_t event)
{
  /* Initiate's Companion	|1|G
   * Creature - Cat 3/1
   * Whenever ~ deals combat damage to a player, untap target creature or land. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  result =
      damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE);
  if (result == 0)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 7);
  td.preferred_controller = player;
  td.allow_cancel = 0;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "Select a creature or land to untap.", 0, 0x80);
    if (result != 0)
    {
      untap_card(instance->targets[0].player, instance->targets[0].card);
    }
  }
  instance->number_of_targets = '\0';
  spell_fizzled = 0;
  return 0;
}

/* ManalinkEh 2022: 0x22e0ac0; original cards/amonkhet.c:3570. */
int card_manglehorn(int player, int card, event_t event)
{
  /* Manglehorn	|2|G
   * Creature - Beast 2/2
   * When ~ enters the battlefield, you may destroy target artifact.
   * Artifacts your opponents control enter the battlefield tapped. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 0x40);
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_ARTIFACT");
      if (result != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
    }
    instance->number_of_targets = '\0';
  }
  permanents_enters_battlefield_tapped(player, card, event, 1 - player, 0x40, NULL);
  return 0;
}

/* ManalinkEh 2022: 0x22e0bb0; original cards/amonkhet.c:3595. */
int card_naga_vitalist(int player, int card, event_t event)
{
  /* Naga Vitalist	|1|G
   * Creature - Naga Druid 1/2
   * |T: Add one mana of any type that a land you control could produce. */

  int result;
  color_test_t available;

  if (event == EVENT_RESOLVE_SPELL)
  {
    result = get_id(player, card);
    get_color_of_mana_produced_by_id(result, 0x3e, player);
  }
  result = get_id(player, card);
  available = get_color_of_mana_produced_by_id(result, 0x3e, player);
  result = mana_producing_creature_all_one_color(player, card, event, 0, available, 1);
  return result;
}

/* ManalinkEh 2022: 0x22e0c40; original cards/amonkhet.c:3611. */
int card_oashra_cultivator(int player, int card, event_t event)
{
  /* Oashra Cultivator	|G
   * Creature - Human Druid 0/3
   * |2|G, |T, Sacrifice ~: Search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library. */

  int result;
  if (event == EVENT_ACTIVATE)
  {
    if (1 - player == current_turn)
    {
      result = count_subtype(player, 1, -1);
      if (result < 4)
      {
        result = count_subtype(player, 2, -1);
        ai_modifier = ai_modifier + result * 8;
      }
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 1);
  }
  return generic_activated_ability(player, card, event, 9, 2, 0, 0, 1, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x22e0d50; original cards/amonkhet.c:3629. */
int card_ornery_kudu(int player, int card, event_t event)
{
  /* Ornery Kudu	|2|G
   * Creature - Antelope 3/4
   * When ~ enters the battlefield, put a -1/-1 counter on target creature you control. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }

  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.allow_cancel = 0;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0);
    if (result != 0)
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                               COUNTER_M1_M1, 1);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* Pouncing Cheetah	|2|G	=>time_spiral.c:card_ashcoat_bear 0x200cb63
 * Creature - Cat 3/2
 * Flash */

/* ManalinkEh 2022: 0x22e0e40; original cards/amonkhet.c:3657. */
int card_prowling_serpopard(int player, int card, event_t event)
{
  /* Prowling Serpopard	|1|G|G
   * Creature - Cat Snake 4/3
   * This spell can't be countered.
   * Creature spells you control can't be countered. */

  if (is_humiliated(player, card, AL_STATIC))
  {
    return 0;
  }
  cannot_be_countered(player, card, event);
  type_uncounterable(player, card, event, player, 2, NULL);
  return 0;
}

/* ManalinkEh 2022: 0x22e0ec0; original cards/amonkhet.c:3672. */
int card_quarry_hauler(int player, int card, event_t event)
{
  /* Quarry Hauler	|3|G
   * Creature - Camel 4/3
   * When ~ enters the battlefield, for each kind of counter on target permanent, put another counter of that kind on it or remove one from it. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.allow_cancel = 0;
  etb_ability(player, card, EVENT_ETB, RESOLVE_TRIGGER_MANDATORY);

  instance = get_card_instance(player, card);
  instance->number_of_targets = '\0';
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "TARGET_PERMANENT", 0, 0);
    if (result != 0)
    {
      quarry_hauler_helper(player, card, instance->targets[0].player, instance->targets[0].card);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x22e0fc0; original cards/amonkhet.c:3700. */
int card_rhonas_the_indomitable(int player, int card, event_t event)
{
  /* Rhonas the Indomitable	|2|G
   * Legendary Creature - God 5/5
   * Deathtouch, indestructible
   * ~ can't attack or block unless you control another creature with power 4 or greater.
   * |2|G: Another target creature gets +2/+0 and gains trample until end of turn. */

  target_definition_t td;
  int result;
  card_instance_t *instance;

  check_legend_rule(player, card, event);
  indestructible(player, card, event);
  deathtouch(player, card, event);
  if (event == EVENT_ATTACK_LEGALITY)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (result = is_humiliated(player, card, AL_STATIC), result == 0))
    {
      test_definition_t this_test;
      default_test_definition(&this_test, 2);
      this_test.power = 3;
      this_test.power_flag = F5_POWER_GREATER_THAN_VALUE;
      this_test.not_me = 1;
      result = check_battlefield_for_special_card(player, card, player, 0, &this_test);
      if (result == 0)
      {
        event_result = 1;
      }
    }
  }
  else
  {
    if (event == EVENT_ABILITIES)
    {
      test_definition_t this_test2;
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
      result = is_humiliated(player, card, AL_STATIC);
      if (result != 0)
      {
        return 0;
      }
      default_test_definition(&this_test2, 2);
      this_test2.power = 3;
      this_test2.power_flag = F5_POWER_GREATER_THAN_VALUE;
      this_test2.not_me = 1;
      result = check_battlefield_for_special_card(player, card, player, 0, &this_test2);
      if (result != 0)
      {
        return 0;
      }
      cannot_block(player, card, EVENT_ABILITIES);
      return 0;
    }
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.special = TARGET_SPECIAL_NOT_ME;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                           instance->targets[0].player, instance->targets[0].card, 2, 0, 0x80, 0);
  }
  return generic_activated_ability(player, card, event, 4, 2, 0, 0, 1, 0, 0, 0, &td,
                                   "TARGET_CREATURE");
}

/* ManalinkEh 2022: 0x22e1290; original cards/amonkhet.c:3751. */
int card_sandwurm_convergence(int player, int card, event_t event)
{
  /* Sandwurm Convergence	|6|G|G
   * Enchantment
   * Creatures with flying can't attack you or planeswalkers you control.
   * At the beginning of your end step, create a 5/5 |Sgreen Wurm creature token. */

  int result;
  token_generation_t token;

  if ((event == EVENT_ATTACK_LEGALITY) && (affected_card_controller != player))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      result = check_for_ability(affected_card_controller, affected_card, 0x20);
      if (result != 0)
      {
        event_result = 1;
      }
    }
  }
  if (current_turn == player)
  {
    result = eot_trigger(player, card, event);
    if (result != 0)
    {
      default_token_definition(player, card, CARD_ID_WURM, &token);
      token.pow = 5;
      token.tou = 5;
      generate_token(&token);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* Scaled Behemoth	|4|G|G	=>m12.c:card_aven_fleetwing 0x2004021
 * Creature - Crocodile 6/7
 * Hexproof */

/* ManalinkEh 2022: 0x22e1370; original cards/amonkhet.c:3778. */
int card_shed_weakness(int player, int card, event_t event)
{
  /* Shed Weakness	|G
   * Instant
   * Target creature gets +2/+2 until end of turn. You may remove a -1/-1 counter from it. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CHECK_PUMP)
  {
    result = vanilla_instant_pump(player, card, EVENT_CHECK_PUMP, 2, player, 2, 2, 0, 0);
    return result;
  }
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
    return result;
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
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
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
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (event != EVENT_CHANGE_TARGET)
    {
      if (event != EVENT_RESOLVE_SPELL)
      {
        return 0;
      }
      result = valid_target(&td);
      if (result != 0)
      {
        pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
        result =
            count_counters(instance->targets[0].player, instance->targets[0].card, COUNTER_M1_M1);
        if ((0 < result) &&
            (result = do_dialog(player, player, card, -1, -1, " Remove a counter.\n Decline.", 0),
             result == 0))
        {
          remove_counters(player, card, instance->targets[0].player,
                                      instance->targets[0].card, COUNTER_M1_M1, 1);
        }
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else
  {
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    get_card_instance(player, card);
  }
  if (spell_fizzled == 1)
  {
    return 0;
  }
  result = generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
  return result;
}

/* ManalinkEh 2022: 0x22e1670; original cards/amonkhet.c:3822. */
int card_shefet_monitor(int player, int card, event_t event)
{
  /* Shefet Monitor	|5|G
   * Creature - Lizard 6/5
   * Cycling |3|G
   * When you cycle ~, you may search your library for a basic land card or a Desert card, put it onto the battlefield, then shuffle your library. */

  int result;
  const char *msg;
  unsigned int land_subtype;
  test_definition_t this_test;

  if (((((uint32_t)(event - EVENT_CAN_ACTIVATE_FROM_HAND) < 2) ||
        (event != EVENT_RESOLVE_ACTIVATION_FROM_HAND)) ||
       (spell_fizzled == 1)) ||
      ((((unsigned int)player_bits[player] >> 24 & 0xff) & 2) != 0))
    return cycling(player, card, event, 3, 0, 0, 1, 0, 0);
  result = do_dialog(player, player, card, -1, -1, " Tutor land\n Cancel", 0);
  if ((result != 0) || (spell_fizzled == -1))
    return cycling(player, card, event, 3, 0, 0, 1, 0, 0);

  bool basic_land = false;
  if (ai_is_speculating != 1 && (player != 1 || ((unsigned char)trace_mode & 2)))
    basic_land = do_dialog(player, player, card, -1, -1,
                           " Tutor a Desert card\n Tutor a basic land", 0) == 1;
  if (basic_land)
  {
    msg = "Select a basic land card.";
    land_subtype = 0x2000;
    result = 1;
  }
  else
  {
    msg = "Select a Desert card.";
    result = 0x47;
    land_subtype = 0x5000;
  }
  new_default_test_definition(&this_test, result, msg);
  this_test.subtype = land_subtype;
  new_global_tutor(player, player, 1, 4, 0, 1, &this_test);
  return cycling(player, card, event, 3, 0, 0, 1, 0, 0);
}

/* ManalinkEh 2022: 0x22e1810; original cards/amonkhet.c:3859. */
int card_sixth_sense(int player, int card, event_t event)
{
  /* Sixth Sense	|G
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has "Whenever this creature deals combat damage to a player, you may draw a card." */

  card_instance_t *instance;
  int result;
  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
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
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
        return vanilla_aura(player, card, event, player);
    }
    else if ((event == EVENT_RESOLVE_SPELL) ||
             ((event == EVENT_CAN_CHANGE_TARGET || (event == EVENT_CHANGE_TARGET))))
      return vanilla_aura(player, card, event, player);
    if ((((event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA) &&
         (((2 < (uint32_t)(event - EVENT_POWER) && (event != EVENT_RESOLVE_MOVING_AURA)) &&
           (event != EVENT_CHANGE_TYPE)))) &&
        (((event != EVENT_SET_COLOR && (event != EVENT_GET_SELECTED_CARD)) &&
          (event != EVENT_STATIC_EFFECTS))))
    {
      return 0;
    }
  }
  return vanilla_aura(player, card, event, player);
}

/* ManalinkEh 2022: 0x22e1930; original cards/amonkhet.c:3881. */
int card_stinging_shot(int player, int card, event_t event)
{
  /* Stinging Shot	|G
   * Instant
   * Put three -1/-1 counters on target creature with flying.
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
  default_target_definition(player, card, &td, 2);
  td.required_abilities = 0x20;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      add_counters(player, card, instance->targets[0].player,
                               instance->targets[0].card, COUNTER_M1_M1, 3);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_FLYING", 1, NULL);
}

/* ManalinkEh 2022: 0x22e1b10; original cards/amonkhet.c:3911. */
int card_synchronized_strike(int player, int card, event_t event)
{
  /* Synchronized Strike	|2|G
   * Instant
   * Untap up to two target creatures. They each get +2/+2 until end of turn. */

  target_t *target_cursor;
  target_t *other_target_cursor;
  card_instance_t *instance;
  int i;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 3;
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 0x20, &td, "TARGET_CREATURE", 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.allow_cancel = 3;
      instance = get_card_instance(player, card);
      if (check_special_flags2(player, card, 0x2000) == 0)
      {
        check_special_flags2(player, card, 0x8000);
      }
      if (new_pick_target(&td, "TARGET_CREATURE", 0, 0) != 0)
      {
        state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
      }
      if (can_target(&td) != 0)
      {
        new_pick_target(&td, "TARGET_CREATURE", 1, 0);
        state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 3;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      i = 0;
      if (instance->number_of_targets != '\0')
      {
        do
        {
          if ((instance->targets[i].card == -1 || (validate_target(player, card, &td, i) == 0)))
          {
            i = i + 1;
            continue;
          }
          untap_card(instance->targets[i].player, instance->targets[i].card);
          other_target_cursor = instance->targets + i;
          target_cursor = instance->targets + i;
          i = i + 1;
          pump_until_eot(player, card, target_cursor->player, other_target_cursor->card, 2, 2);
        } while (i < (int)(unsigned int)instance->number_of_targets);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 3;
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x22e1e30; original cards/amonkhet.c:3996. */
int card_trial_of_strength(int player, int card, event_t event)
{
  /* Trial of Strength	|2|G
   * Enchantment
   * When ~ enters the battlefield, create a 4/2 |Sgreen Beast creature token.
   * When a Cartouche enters the battlefield under your control, return ~ to its owner's hand. */

  int result;


  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_BEAST, &token);
    token.pow = 4;
    token.tou = 2;
    generate_token(&token);
  }
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    test_definition_t test;
    default_test_definition(&test, 4);
    test.subtype = SUBTYPE_CARTOUCHE;
    result = new_specific_cip(player, card, event, player, 2, &test);
    if (result != 0)
    {
      bounce_permanent(player, card);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22e1f40; original cards/amonkhet.c:4024. */
int card_vizier_of_the_menagerie(int player, int card, event_t event)
{
  /* Vizier of the Menagerie	|3|G
   * Creature - Naga Cleric 3/4
   * You may look at the top card of your library.
   * You may cast the top card of your library if it's a creature card.
   * You may spend mana as though it were mana of any type to cast creature spells. */

  unsigned char color_requirement;
  unsigned char white_requirement;
  card_ptr_t *card_data;
  unsigned char generic_symbols;
  int result;
  card_instance_t *instance;
  int *deck;
  int card_00;

  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if ((affected_card_controller == player) &&
        (result = is_what(player, affected_card, 2), result != 0))
    {

      bool reached_self = false;
      for (result = 0; result != 2 && !reached_self; ++result)
        for (card_00 = 0; card_00 < active_cards_count[result]; ++card_00)
        {
          if (card == card_00 && player == result)
          {
            reached_self = true;
            break;
          }
          instance = in_play(result, card_00);
          if (instance != NULL && cards_data[instance->internal_card_id].id == CARD_ID_VIZIER_OF_THE_MENAGERIE)
            return 0;
        }
      result = get_id(affected_card_controller, affected_card);
      card_data = cards_ptr[result];
      generic_symbols = -(card_data->req.req_black < 0xf) & card_data->req.req_black;
      COST_BLACK = COST_BLACK - card_data->req.req_black;
      color_requirement = card_data->req.req_blue;
      if (color_requirement < 0xf)
      {
        generic_symbols = generic_symbols + color_requirement;
      }
      COST_BLUE = COST_BLUE - color_requirement;
      color_requirement = card_data->req.req_green;
      if (color_requirement < 0xf)
      {
        generic_symbols = generic_symbols + color_requirement;
      }
      COST_GREEN = COST_GREEN - color_requirement;
      color_requirement = card_data->req.req_red;
      if (color_requirement < 0xf)
      {
        generic_symbols = generic_symbols + color_requirement;
      }
      white_requirement = card_data->req.req_white;
      COST_RED = COST_RED - color_requirement;
      if (white_requirement < 0xf)
      {
        generic_symbols = generic_symbols + white_requirement;
      }
      COST_WHITE = COST_WHITE - white_requirement;
      COST_COLORLESS = COST_COLORLESS + generic_symbols;
    }
  }
  else if (((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
             (event == EVENT_RESOLVE_ACTIVATION)) ||
            (event == EVENT_CLEANUP)) &&
           ((player != 1 && (result = is_humiliated(player, card, AL_STATIC), result == 0))))
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      deck = deck_ptr[player];
      if (*deck != -1)
      {
        result = is_what(-1, *deck, 2);
        if ((result != 0) &&
            (result = can_legally_play_iid_now(player, *deck_ptr[player], EVENT_ACTIVATE),
             result != 0))
        {

          result = is_what(-1, *deck_ptr[player], 2);
          if (result == 0)
          {
            result = get_cmc_by_internal_id(*deck_ptr[player]);
            result = true_get_updated_casting_cost(player, -1, *deck_ptr[player], EVENT_ACTIVATE,
                                                   result);
            result = has_mana(player, 0, result);
          }
          else
          {
            result = get_cmc_by_internal_id(*deck_ptr[player]);
            result = true_get_updated_casting_cost(player, -1, *deck_ptr[player], EVENT_ACTIVATE,
                                                   result);
            result = has_mana_multi_a(player, result, 0, 0, 0, 0, 0);
          }
          if ((result != 0) &&
              (result = do_dialog(player, player, card, -1, -1,
                                  " Check top card of deck\n Play top card\n Cancel", 0),
               result != 0))
          {
            if (result != 1)
            {
              return 0;
            }

            result = is_what(-1, *deck_ptr[player], 0x40);
            if (result == 0)
            {
              result = get_cmc_by_internal_id(*deck_ptr[player]);
              result = true_get_updated_casting_cost(player, -1, *deck_ptr[player], EVENT_ACTIVATE,
                                                     result);
              result = charge_mana(player, COLOR_COLORLESS, result);
            }
            else
            {
              result = get_cmc_by_internal_id(*deck_ptr[player]);
              result = true_get_updated_casting_cost(player, -1, *deck_ptr[player], EVENT_ACTIVATE,
                                                     result);
              result = charge_mana_multi_a(player, 0, 0, 0, 0, 0, 0, result);
            }
            if (result == 0)
            {
              spell_fizzled = 1;
              return 0;
            }
            play_card_in_deck_for_free(player, player, 0);
            cant_be_responded_to = 1;
            return 0;
          }
        }
        deck = deck_ptr[player];
      }
      show_deck(player, deck, 1, "Here\'s the top card of your deck", 0, gs_done_008b40e0);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22e2400; original cards/amonkhet.c:4115. */
int card_watchful_naga(int player, int card, event_t event)
{
  /* Watchful Naga	|2|G
   * Creature - Naga Wizard 2/2
   * You may exert ~ as it attacks. When you do, draw a card. */

  int result;

  result = duh_mode(player);
  result = declare_attackers_trigger(player, card, event,
                                     (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, player, card);
  if (result != 0)
  {

    result = does_not_untap_effect(player, card, player, card, 0, 1);
    if (result != 0)
    {
      draw_a_card(player);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22e2490; original cards/amonkhet.c:4129. */
int card_ahn_crop_champion(int player, int card, event_t event)
{
  /* Ahn-Crop Champion	|2|G|W
   * Creature - Human Warrior 4/4
   * You may exert ~ as it attacks. When you do, untap all other creatures you control. */

  int result;
  test_definition_t this_test;

  result = duh_mode(player);
  result = declare_attackers_trigger(player, card, event,
                                     (result != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, player, card);
  if (result != 0)
  {

    result = does_not_untap_effect(player, card, player, card, 0, 1);
    if (result != 0)
    {
      default_test_definition(&this_test, 2);
      this_test.not_me = 1;
      new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
      return 0;
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22e2560; original cards/amonkhet.c:4146. */
int card_aven_wind_guide(int player, int card, event_t event)
{
  /* Aven Wind Guide	|2|W|U
   * Creature - Bird Warrior 2/3
   * Flying, vigilance
   * Creature tokens you control have flying and vigilance.
   * Embalm |4|W|U */

  int result;
  card_instance_t *instance;
  vigilance(player, card, event);
  result = is_humiliated(player, card, AL_STATIC);
  if (result == 0)
  {
    instance = in_play(player, card);
    if (((event == EVENT_ABILITIES) && (instance != NULL)) && (affected_card_controller == player))
    {
      result = is_token(player, affected_card);
      if (result != 0)
      {
        vigilance(affected_card_controller, affected_card, EVENT_ABILITIES);
        event_result = event_result | 0x20;
      }
    }
  }
  result = embalm(player, card, event, 0x41ca, 4, 0, 1, 0, 0, 1);
  return result;
}
#endif

/* ManalinkEh 2022: 0x22e2650; original cards/amonkhet.c:4166. */
int card_bounty_of_the_luxa(int player, int card, event_t event)
{
  /* Bounty of the Luxa	|2|G|U
   * Enchantment
   * At the beginning of your precombat main phase, remove all flood counters from ~. If no counters were removed this way, put a flood counter on ~ and draw a card. Otherwise, add |C|G|U. */

  int result;
  card_instance_t *instance;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    instance = get_card_instance(player, card);
    instance->info_slot = 1;
  }
  result = beginning_of_phase(player, card, event, player, -1, 0x14);
  if (result != 0)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      result = count_counters(player, card, COUNTER_FLOOD);
      if (result == 0)
      {
        add_counters(player, card, player, card, COUNTER_FLOOD, 1);
        draw_a_card(player);
      }
      else
      {
        produce_mana_multi(player, 1, 0, 1, 1, 0, 0);
        remove_all_counters(player, card, player, card, COUNTER_FLOOD);
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x22e27b0; original cards/amonkhet.c:4190. */
int card_decimator_beetle(int player, int card, event_t event)
{
  /* Decimator Beetle	|3|B|G
   * Creature - Insect 4/5
   * When ~ enters the battlefield, put a -1/-1 counter on target creature you control.
   * Whenever ~ attacks, remove a -1/-1 counter from target creature you control and put a -1/-1 counter on up to one target creature defending player controls. */

  int result;
  card_instance_t *instance;
  target_definition_t td_1;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_M1_M1, 1);
      }
    }
    instance->number_of_targets = '\0';
  }
  result =
      declare_attackers_trigger(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, player, card);
  if (result != 0)
  {

    default_target_definition(player, card, &td_1, 2);
    td_1.preferred_controller = player;
    td_1.allowed_controller = player;
    if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
    {
      td_1.special = TARGET_SPECIAL_REQUIRES_COUNTER;
      td_1.extra = 1;
    }
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    td.preferred_controller = td.allowed_controller;
    instance = get_card_instance(player, card);
    result = can_target(&td_1);
    if (result != 0)
    {
      result = new_pick_target(&td_1, "TARGET_CREATURE_YOU_CONTROL", 0, 0);
      if (result != 0)
      {
        remove_counters(player, card, instance->targets[0].player,
                                    instance->targets[0].card, COUNTER_M1_M1, 1);
      }
    }
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS", 1, 0);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[1].player,
                                 instance->targets[1].card, COUNTER_M1_M1, 1);
      }
    }
  }
  return 0;
}

/* Enigma Drake	|1|U|R	=>theros.c:card_spellheart_chimera 0x200b0dd
 * Creature - Drake 100/4
 * Flying
 * ~'s power is equal to the number of instant and sorcery cards in your graveyard. */

/* ManalinkEh 2022: 0x22e2a60; original cards/amonkhet.c:4247. */
int card_hapatra_vizier_of_poisons(int player, int card, event_t event)
{
  /* Hapatra, Vizier of Poisons	|B|G
   * Legendary Creature - Human Cleric 2/2
   * Whenever ~ deals combat damage to a player, you may put a -1/-1 counter on target creature.
   * Whenever you put one or more -1/-1 counters on a creature, create a 1/1 |Sgreen Snake creature token with deathtouch. */

  card_instance_t *instance;
  int result;


  check_legend_rule(player, card, event);
  instance = get_card_instance(player, card);
  result =
      damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE);
  if (result != 0)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 2);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE", 0, 0);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_M1_M1, 1);
      }
    }
  }
  if ((event == EVENT_COUNTERS) && (attacking_card_controller == player))
  {
    result = is_what(affected_card_controller, affected_card, 2);
    if ((result != 0) && (counters_added_type == 1))
    {
      result = is_humiliated(player, card, AL_TRIG);
      if (result == 0)
      {
        token_generation_t token;
        default_token_definition(player, card, CARD_ID_SNAKE, &token);
        token.tou = 1;
        token.pow = 1;
        token.color_forced = 8;
        token.s_key_plus = 0x200;
        generate_token(&token);
      }
    }
  }
  return 0;
}

/* Honored Crop-Captain	|R|W	=>mirrodin_besieged.c:card_accorder_paladin 0x2004481
 * Creature - Human Warrior 3/2
 * Whenever ~ attacks, other attacking creatures get +1/+0 until end of turn. */

/* ManalinkEh 2022: 0x22e2bf0; original cards/amonkhet.c:4283. */
int card_khenra_charioteer(int player, int card, event_t event)
{
  /* Khenra Charioteer	|1|R|G
   * Creature - Jackal Warrior 3/3
   * Trample
   * Other creatures you control have trample. */

  boost_creature_type(player, card, event, ~SUBTYPE_INVALID, 0, 0, 0x80, BCT_CONTROLLER_ONLY);
  return (unsigned int)(event == EVENT_CAN_CAST);
}

/* ManalinkEh 2022: 0x22e2c50; original cards/amonkhet.c:4294. */
int card_merciless_javelineer(int player, int card, event_t event)
{
  /* Merciless Javelineer	|2|B|R
   * Creature - Minotaur Warrior 4/2
   * |2, Discard a card: Put a -1/-1 counter on target creature. That creature can't block this turn. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                               COUNTER_M1_M1, 1);
      pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                             0, 0, 0, 0x20);
    }
  }
  result = generic_activated_ability(player, card, event, 0x24, 2, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
  return result;
}

/* ManalinkEh 2022: 0x22e2de0; original cards/amonkhet.c:4318. */
int card_neheb_the_worthy(int player, int card, event_t event)
{
  /* Neheb, the Worthy	|1|B|R
   * Legendary Creature - Minotaur Warrior 2/2
   * First strike
   * Other Minotaurs you control have first strike.
   * As long as you have one or fewer cards in hand, Minotaurs you control get +2/+0.
   * Whenever ~ deals combat damage to a player, each player discards a card. */

  int result;
  check_legend_rule(player, card, event);
  boost_subtype(player, card, event, SUBTYPE_MINOTAUR, 0, 0, 0x100, 0, BCT_CONTROLLER_ONLY);
  if (hand_count[player] < 2)
  {
    boost_subtype(player, card, event, SUBTYPE_MINOTAUR, 2, 0, 0, 0,
                  BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  }
  result =
      damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE);
  if (result != 0)
  {
    discard(current_turn, 0, player);
    discard(current_turn ^ 1, 0, player);
  }
  return 0;
}

/* ManalinkEh 2022: 0x22e2ef0; original cards/amonkhet.c:4338. */
int card_nissa_steward_of_elements(int player, int card, event_t event)
{
  /* Nissa, Steward of Elements	|X|G|U
   * Legendary Planeswalker - Nissa (X)
   * +2: Scry 2.
   * 0: Look at the top card of your library. If it's a land card or a creature card with converted mana cost less than or equal to the number of loyalty counters on ~, you may put that card onto the battlefield.
   * -6: Untap up to two target lands you control. They become 5/5 Elemental creatures with flying and haste until end of turn. They're still lands. */

  enum
  {
    CHOICE_SCRY = 1,
    CHOICE_REVEAL = 2,
    CHOICE_MEGALANDS = 3
  };

  check_legend_rule(player, card, event);
  card_instance_t *instance = get_card_instance(player, card);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_LOYALTY, instance->info_slot);
  target_definition_t td;
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 1);
    td.allowed_controller = player;
    td.preferred_controller = player;
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = 0;
      instance->targets[1].card = -1;
      instance->targets[1].player = -1;
      instance->targets[0].card = -1;
      instance->targets[0].player = -1;
      int choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Scry 2", 1,
                               10, 2, "Look at the top card of your library", 1, 10, 0,
                               "Megapump two lands you control", can_target(&td), 0xf, 0xfffffffa,
                               0, 0, 0, 0);
      if (!choice)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_MEGALANDS)
        pick_up_to_n_targets(&td, "TARGET_LAND", 2);
    }
    return planeswalker(player, card, event, instance->info_slot);
  }
  if (event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
  {
    if (event != EVENT_CAST_SPELL || card != affected_card || player != affected_card_controller)
      return planeswalker(player, card, event, 0);
    if (player == affected_card_controller)
    {
      set_special_flags2(player, card, 0x20000);
      if (!played_for_free(player, card) && !is_token(player, card))
      {
        if (player == 0)
        {
          int paid_x = charge_mana(0, COLOR_COLORLESS, -1);
          enters_the_battlefield_with_counters(0, card, EVENT_CAST_SPELL, COUNTER_LOYALTY, paid_x);
        }
        else
        {
          int paid_x = charge_mana_for_multi_x(player, 0, 1);
          if (spell_fizzled != 1)
            enters_the_battlefield_with_counters(player, card, EVENT_CAST_SPELL, COUNTER_LOYALTY,
                                                 paid_x);
        }
      }
    }
  }
  default_target_definition(player, card, &td, 1);
  int32_t loyalty = instance->info_slot;
  td.allowed_controller = player;
  td.preferred_controller = td.allowed_controller;
  if (event != EVENT_RESOLVE_ACTIVATION)
    return planeswalker(player, card, event, loyalty);
  if (loyalty == CHOICE_SCRY)
    scry(player, 2);
  else if (loyalty == CHOICE_MEGALANDS)
  {
    if (!instance->number_of_targets)
      return planeswalker(player, card, event, loyalty);
    int i = 0;
    do
    {
      untap_card(player, instance->targets[i].card);
      target_t *land = instance->targets + i;
      ++i;
      card_instance_t *animation =
          get_card_instance(player, land_animation2(player, card, land->player, land->card, 1, 5, 5,
                                                    0x20, 4, 0, NULL));
      animation->targets[9].card = 0x3a;
    } while (i < (int)(unsigned int)instance->number_of_targets);
  }
  else if (loyalty == CHOICE_REVEAL)
  {
    int *deck = deck_ptr[player];
    int iid = *deck;
    if (iid == -1)
      return planeswalker(player, card, event, loyalty);
    if (player != 1)
    {
      show_deck(player, deck, 1, "Card revealed by Nissa", 0, gs_done_008b40e0);
      iid = *deck;
    }
    if (cards_data[iid].type & 1)
      put_into_play_a_card_from_deck(player, player, 0);
    else if (cards_data[iid].type & 2)
    {
      int cmc = get_cmc_by_internal_id(iid);
      int counters = count_counters(player, card, COUNTER_LOYALTY);
      if (cmc <= counters)
        put_into_play_a_card_from_deck(player, player, 0);
    }
  }
  else
    return planeswalker(player, card, event, loyalty);
  return planeswalker(player, card, event, instance->info_slot);
}

/* ManalinkEh 2022: 0x22e33e0; original cards/amonkhet.c:4435. */
int card_samut_voice_of_dissent(int player, int card, event_t event)
{
  /* Samut, Voice of Dissent	|3|R|G
   * Legendary Creature - Human Warrior 3/4
   * Flash
   * Double strike, vigilance, haste
   * Other creatures you control have haste.
   * |W, |T: Untap another target creature. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  check_legend_rule(player, card, event);
  if (event == EVENT_ABILITIES)
  {
    vigilance(player, card, EVENT_ABILITIES);
    haste(player, card, EVENT_ABILITIES);
    boost_subtype(player, card, EVENT_ABILITIES, ~SUBTYPE_INVALID, 0, 0, 0, 4, BCT_CONTROLLER_ONLY);
  }
  else if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
            (event == EVENT_RESOLVE_ACTIVATION)) ||
           (event == EVENT_CLEANUP))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.illegal_abilities = 0;
    td.special = TARGET_SPECIAL_NOT_ME;
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
    result = generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 1, 0, &td,
                                       "SELECT_ANOTHER_CREATURE");
    return result;
  }
  result = flash(player, card, event);
  return result;
}

/* Shadowstorm Vizier	|U|B
 * Creature - Human Cleric 1/3
 * Flying
 * Whenever you cycle or discard a card, ~ gets +1/+1 until end of turn. */

/* ManalinkEh 2022: 0x22d6fa0; original cards/amonkhet.c:4475. */
const char *target_is_token_too(int who_chooses, int player, int card, int targeting_player,
                                int targeting_card)
{
  int result;
  const char *requirement_error;
  result = is_token(player, card);
  requirement_error = NULL;
  if (result == 0)
  {
    requirement_error = "token";
  }
  return requirement_error;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22e35a0; original cards/amonkhet.c:4481. */
int card_temmet_vizier_of_naktamun(int player, int card, event_t event)
{
  /* Temmet, Vizier of Naktamun	|W|U
   * Legendary Creature - Human Cleric 2/2
   * At the beginning of combat on your turn, target creature token you control gets +1/+1 until end of turn and can't be blocked this turn.
   * Embalm |3|W|U */

  int result;
  target_t tgt;
  target_definition_t td;
  check_legend_rule(player, card, event);
  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {
    base_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.allow_cancel = 0;
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (intptr_t)target_is_token_too;
    result = can_target(&td);
    if (result != 0)
    {
      result = select_target(player, card + -1000, &td,
                                  "Select target creature token you control.", &tgt);
      if (result != 0)
      {
        pump_ability_until_eot(player, card, tgt.player, tgt.card, 1, 1, 0, 0x10);
      }
    }
  }
  result = embalm(player, card, event, 0x4294, 3, 0, 1, 0, 0, 1);
  return result;
}
#endif

/* ManalinkEh 2022: 0x22e36e0; original cards/amonkhet.c:4510. */
int card_wayward_servant(int player, int card, event_t event)
{
  /* Wayward Servant	|W|B
   * Creature - Zombie 2/2
   * Whenever another Zombie enters the battlefield under your control, each opponent loses 1 life and you gain 1 life. */

  test_definition_t test;
  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    new_default_test_definition(&test, 0x47, "");
    test.subtype = 0xde;
    test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event != EVENT_ETB)
  {
    return 0;
  }
  lose_life(1 - player, 1);
  gain_life(player, 1);
  return 0;
}

/* Weaver of Currents	|1|G|U	=>scars_of_mirrodin.c:card_palladium_myr 0x200acf5
 * Creature - Naga Druid 2/2
 * |T: Add |C|C. */

/* ManalinkEh 2022: 0x22e37d0; original cards/amonkhet.c:4537. */
int card_dusk_dawn(int player, int card, event_t event)
{
  /* Dusk // Dawn	|2|W|W|/|3|W|W
   * Sorcery // Sorcery
   * Destroy all creatures with power 3 or greater.
   * //
   * Aftermath
   * Return all creature cards with power 2 or less from your graveyard to your hand. */

  int result;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    return result;
  }
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    if ((result != 0) &&
        (result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY), result != 0))
    {
      result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 3, 0, 0, 0, 0, 2, 0);
      return result;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    result = get_aftermath(player, card);
    if (result == 0)
    {
      default_test_definition(&test, 2);
      test.power = 2;
      test.power_flag = 2;
      new_manipulate_all(player, card, 2, &test, ACT_KILL_BURY);
    }
    result = get_aftermath(player, card);
    if (result != 0)
    {
      default_test_definition(&test, 2);
      test.power = 3;
      test.power_flag = 3;
      from_grave_to_hand_multiple(player, &test);
      kill_card(player, card, KILL_EXILE);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = aftermath(player, card, event, 3, 0, 0, 0, 0, 2, 0);
  return result;
}

/* ManalinkEh 2022: 0x22e39e0; original cards/amonkhet.c:4577. */
int card_commit_memory(int player, int card, event_t event)
{
  /* Commit // Memory	|3|U|/|4|U|U
   * Instant // Sorcery
   * Put target spell or nonland permanent into its owner's library second from the top.
   * //
   * Aftermath
   * Each player shuffles their hand and graveyard into their library, then draws seven cards. */

  int p;
  int *library;
  card_instance_t *instance;
  int result_or_iid;
  int aftermath_mode;
  unsigned int player_00;
  unsigned char valid_modes;
  target_definition_t td;
  target_definition_t td1;

  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    if (can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY) == 0)
    {
      return 0;
    }
    result_or_iid = basic_spell(player, card, EVENT_GRAVEYARD_ABILITY);
    if ((result_or_iid != 0) &&
        (result_or_iid = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY),
         result_or_iid != 0))
    {
      return aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 4, 0, 2, 0, 0, 0, 0);
    }
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TYPE_LAND;
    counterspell_target_definition(player, card, &td1, 0xff);
    get_card_instance(player, card);
    return aftermath(player, card, event, 4, 0, 2, 0, 0, 0, 0);
  }
  default_target_definition(player, card, &td, 0x47);
  td.illegal_type = TYPE_LAND;
  counterspell_target_definition(player, card, &td1, 0xff);
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    if (counterspell(player, card, EVENT_CAN_CAST, &td1, 0) != 0)
    {
      return 99;
    }
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    if (get_aftermath(player, card) == 0)
    {
      instance->info_slot = 0;
      if (counterspell(player, card, EVENT_CAN_CAST, &td1, 0) != 0)
      {
        instance->info_slot = 0x42;
        return counterspell(player, card, EVENT_PLAY_FIRST_HALF, &td1, 0);
      }
      generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                    "Select target spell or nonland permanent to put into it\'s owner\'s library.",
                    1, NULL);
      if (spell_fizzled != 1)
      {
        instance->info_slot = 0x43;
      }
    }
    return aftermath(player, card, event, 4, 0, 2, 0, 0, 0, 0);
  }
  if (event != EVENT_RESOLVE_SPELL)
    return aftermath(player, card, event, 4, 0, 2, 0, 0, 0, 0);
  if (get_aftermath(player, card) == 0)
  {
    valid_modes = 0;
    if (instance->info_slot == 0x42)
      valid_modes = counterspell_validate(player, card, &td1, 0) != 0;
    if (instance->info_slot == 0x43)
      valid_modes |= (valid_target(&td) != 0) * 2;
    if (valid_modes != 0)
    {
      if ((valid_modes & 1) != 0)
      {
        put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
        library = deck_ptr[instance->targets[0].player];
        result_or_iid = library[1];
        library[1] = *library;
        *library = result_or_iid;
      }
      if ((valid_modes & 2) != 0)
      {
        put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
        library = deck_ptr[instance->targets[0].player];
        result_or_iid = *library;
        *library = library[1];
        library[1] = result_or_iid;
      }
    }
  }
  aftermath_mode = get_aftermath(player, card);
  p = current_turn;
  if (aftermath_mode != 0)
  {
    reshuffle_hand_into_deck(current_turn, 1);
    reshuffle_grave_into_deck(p, 0);
    draw_cards(p, 7);
    player_00 = current_turn ^ 1;
    reshuffle_hand_into_deck(player_00, 1);
    reshuffle_grave_into_deck(player_00, 0);
    draw_cards(player_00, 7);
  }
  kill_card(player, card, KILL_DESTROY);
  return aftermath(player, card, event, 4, 0, 2, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22e3ef0; original cards/amonkhet.c:4660. */
int card_never_return(int player, int card, event_t event)
{
  /* Never // Return	|1|B|B|/|3|B
   * Sorcery // Sorcery
   * Destroy target creature or planeswalker.
   * //
   * Aftermath
   * Exile target card from a graveyard. Create a 2/2 |Sblack Zombie creature token. */

  int result;
  card_instance_t *instance;
  target_definition_t *td_00;
  const char *prompt;
  test_definition_t *this_test_00;
  target_definition_t td;
  test_definition_t this_test;

  td_00 = &td;
  this_test_00 = &this_test;
  default_target_definition(player, card, td_00, 0x102);
  new_default_test_definition(this_test_00, 0xff, "Select a card to exile.");
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY);
    if (result == 0)
    {
      return 0;
    }
    result = generic_spell(player, card, EVENT_CAN_CAST, 0x1000, NULL, NULL, 1, this_test_00);
    if (result != 0)
    {
      return aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 3, 1, 0, 0, 0, 0, 0);
    }
    return aftermath(player, card, event, 3, 1, 0, 0, 0, 0, 0);
  }
  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 1, td_00, NULL, 1, NULL);
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    this_test_00 = NULL;
    prompt = "TARGET_CREATURE_OR_PLANESWALKER";
    result = 1;
  }
  else
  {
    if (event != EVENT_PLAY_SECOND_HALF)
    {
      if (event == EVENT_RESOLVE_SPELL)
      {
        instance = get_card_instance(player, card);
        result = get_aftermath(player, card);
        if ((result == 0) && (result = valid_target(td_00), result != 0))
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
        result = get_aftermath(player, card);
        if ((result != 0) && (result = validate_target_from_grave_source(
                                  player, card, instance->targets[0].player, 1),
                              result != -1))
        {
          rfg_card_from_grave(instance->targets[0].player, result);
          generate_token_by_id(player, card, CARD_ID_ZOMBIE);
        }
        kill_card(player, card, KILL_DESTROY);
      }
      return aftermath(player, card, event, 3, 1, 0, 0, 0, 0, 0);
    }
    prompt = NULL;
    td_00 = NULL;
    result = 0x1000;
  }
  return generic_spell(player, card, EVENT_CAST_SPELL, result, td_00, prompt, 1, this_test_00);
}

/* ManalinkEh 2022: 0x22d6fd0; original cards/amonkhet.c:4712. */
static int effect_insult(int player, int card, event_t event)
{
  card_instance_t *instance;

  instance = damage_being_dealt(event);
  if ((instance != NULL) && (instance->damage_source_player == player))
  {
    instance->info_slot = instance->info_slot * 2;
  }
  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22e4230; original cards/amonkhet.c:4725. */
int card_insult_injury(int player, int card, event_t event)
{
  /* Insult // Injury	|2|R|/|2|R
   * Sorcery // Sorcery
   * Damage can't be prevented this turn. If a source you control would deal damage this turn, it deals double that damage instead.
   * //
   * Aftermath
   * Injury deals 2 damage to target creature and 2 damage to target player or planeswalker. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  target_definition_t td1;

  default_target_definition(player, card, &td, 2);
  default_target_definition(player, card, &td1, 0x100);
  td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  instance = get_card_instance(player, card);
  if (event != EVENT_GRAVEYARD_ABILITY)
  {
    if (event == EVENT_CAN_CAST)
    {
      result = basic_spell(player, card, EVENT_CAN_CAST);
    }
    else
    {
      if (event == EVENT_PLAY_SECOND_HALF)
      {
        instance->number_of_targets = '\0';
        generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
        if (spell_fizzled != 1)
        {
          new_pick_target(&td1, "TARGET_PLAYER_OR_PLANESWALKER", 1, 1);
        }
      }
      else if (event == EVENT_RESOLVE_SPELL)
      {
        result = get_aftermath(player, card);
        if (result == 0)
        {
          result = damage_cannot_be_prevented_until_eot(player, card);
          alternate_legacy_text(1, player, result);
          result = create_legacy_effect(player, card, effect_insult);
          alternate_legacy_text(2, player, result);
        }
        result = get_aftermath(player, card);
        if (result != 0)
        {
          result = valid_target(&td);
          if (result != 0)
          {
            damage_target0(player, card, 2);
          }
          result = validate_target(player, card, &td1, 1);
          if (result != 0)
          {
            damage_creature(instance->targets[1].player, instance->targets[1].card, 2, player,
                            card);
          }
        }
        kill_card(player, card, KILL_DESTROY);
      }
      result = aftermath(player, card, event, 2, 0, 0, 0, 1, 0, 0);
    }
    return result;
  }
  result = generic_spell(player, card, EVENT_GRAVEYARD_ABILITY, 1, &td, "TARGET_CREATURE", 1, NULL);
  if (result != 0)
  {
    result = generic_spell(player, card, EVENT_GRAVEYARD_ABILITY, 1, &td1,
                           "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
    return result;
  }
  result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 2, 0, 0, 0, 1, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22e45d0; original cards/amonkhet.c:4781. */
int card_mouth_feed(int player, int card, event_t event)
{
  /* Mouth // Feed	|2|G|/|3|G
   * Sorcery // Sorcery
   * Create a 3/3 |Sgreen Hippo creature token.
   * //
   * Aftermath
   * Draw a card for each creature you control with power 3 or greater. */

  int result;


  if (event == EVENT_CAN_CAST)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    return result;
  }
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    if (result != 0)
    {
      result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 3, 0, 0, 1, 0, 0, 0);
      return result;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    result = get_aftermath(player, card);
    if (result == 0)
    {
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_HIPPO, &token);
      token.tou = 3;
      token.pow = 3;
      generate_token(&token);
    }
    result = get_aftermath(player, card);
    if (result != 0)
    {
      test_definition_t this_test;
      default_test_definition(&this_test, 2);
      this_test.power = 2;
      this_test.power_flag = F5_POWER_GREATER_THAN_VALUE;
      result =
          check_battlefield_for_special_card(player, card, player, 4, &this_test);
      draw_cards(player, result);
      kill_card(player, card, KILL_EXILE);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = aftermath(player, card, event, 3, 0, 0, 1, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22e47e0; original cards/amonkhet.c:4819. */
int card_start_finish(int player, int card, event_t event)
{
  /* Start // Finish	|2|W|/|2|B
   * Instant // Sorcery
   * Create two 1/1 |Swhite Warrior creature tokens with vigilance.
   * //
   * Aftermath
   * As an additional cost to cast Finish, sacrifice a creature.
   * Destroy target creature. */

  card_instance_t *instance;
  int result;
  unsigned int chosen_sacrifice;
  target_definition_t td;
  test_definition_t test;

  default_target_definition(player, card, &td, 2);
  new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
  instance = get_card_instance(player, card);
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY);
    if (result == 0)
    {
      return 0;
    }
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
    if ((result != 0) && (result = new_can_sacrifice_as_cost(player, card, &test), result != 0))
    {
      return aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 2, 1, 0, 0, 0, 0, 0);
    }
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      return basic_spell(player, card, EVENT_CAN_CAST);
    }
    if (event == EVENT_PLAY_SECOND_HALF)
    {

      chosen_sacrifice = new_sacrifice(player, card, player,
                                       SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
      if (chosen_sacrifice == 0)
      {
        spell_fizzled = 1;
      }
      else
      {
        generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
        if (spell_fizzled != 1)
        {
          kill_card((int)chosen_sacrifice >> 0x10 & 0xff, chosen_sacrifice >> 0x18, KILL_SACRIFICE);
        }
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      result = get_aftermath(player, card);
      if (result == 0)
      {
        vigilant_white_warriors(player, card, 2);
      }
      result = get_aftermath(player, card);
      if (result != 0)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
        kill_card(player, card, KILL_EXILE);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  return aftermath(player, card, event, 2, 1, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x22e4b10; original cards/amonkhet.c:4880. */
int card_reduce_rubble(int player, int card, event_t event)
{
  /* Reduce // Rubble	|2|U|/|2|R
   * Instant // Sorcery
   * Counter target spell unless its controller pays |3.
   * //
   * Aftermath
   * Up to three target lands don't untap during their controller's next untap step. */

  target_definition_t td;
  target_definition_t td1;

  counterspell_target_definition(player, card, &td, 0);
  default_target_definition(player, card, &td1, 1);
  td1.allow_cancel = 3;
  card_instance_t *inst = get_card_instance(player, card);
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    if (can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY) == 0)
    {
      return 0;
    }
    if (generic_spell(player, card, EVENT_CAN_CAST, 0x20, &td1, "TARGET_LAND", 3, NULL) != 0)
    {
      return aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 2, 0, 0, 0, 1, 0, 0);
    }
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      return generic_spell(player, card, EVENT_CAN_CAST, 2, NULL, NULL, 1, NULL);
    }
    if (event == EVENT_PLAY_FIRST_HALF)
    {
      generic_spell(player, card, EVENT_CAST_SPELL, 2, NULL, NULL, 1, NULL);
    }
    else if (event == EVENT_PLAY_SECOND_HALF)
    {
      generic_spell(player, card, EVENT_CAST_SPELL, 0x20, &td1, "TARGET_LAND", 3, NULL);
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      if (get_aftermath(player, card) == 0)
      {
        counterspell_resolve_unless_pay_x(player, card, NULL, 0, 3);
      }
      if (get_aftermath(player, card) != 0)
      {
        for (int i = 0; i < inst->number_of_targets; ++i)
          if (validate_target(player, card, &td1, i))
            does_not_untap_effect(player, card, inst->targets[i].player,
                                 inst->targets[i].card, 0, 1);
        kill_card(player, card, KILL_EXILE);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  return aftermath(player, card, event, 2, 0, 0, 0, 1, 0, 0);
}

/* ManalinkEh 2022: 0x22e4ea0; original cards/amonkhet.c:4937. */
int card_destined_lead(int player, int card, event_t event)
{
  /* Destined // Lead	|1|B|/|3|G
   * Instant // Sorcery
   * Target creature gets +1/+0 and gains indestructible until end of turn.
   * //
   * Aftermath
   * All creatures able to block target creature this turn do so. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY);
    if (result == 0)
    {
      return 0;
    }
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
    if (result != 0)
    {
      result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 3, 0, 0, 1, 0, 0, 0);
      return result;
    }
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
      return result;
    }
    if (event == EVENT_PLAY_FIRST_HALF)
    {
      generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
    }
    else
    {
      if (event == EVENT_PLAY_SECOND_HALF)
      {
        generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
        if (spell_fizzled == 1)
        {
          return 0;
        }
        instance = get_card_instance(instance->targets[0].player, instance->targets[0].card);
        ai_modifier = ai_modifier +
                      (unsigned int)((instance->destroys_if_blocked == '\0') ? -96 : 0) + 0x30;
        return 0;
      }
      if (event == EVENT_RESOLVE_SPELL)
      {
        result = get_aftermath(player, card);
        if ((result == 0) && (result = valid_target(&td), result != 0))
        {
          result = pump_ability_until_eot(player, card, instance->targets[0].player,
                                          instance->targets[0].card, 1, 0, 0, 0x800);
          alternate_legacy_text(1, player, result);
        }
        result = get_aftermath(player, card);
        if (result != 0)
        {
          result = valid_target(&td);
          if (result != 0)
          {
            result = pump_ability_until_eot(player, card, instance->targets[0].player,
                                            instance->targets[0].card, 0, 0, 0, (SP_KEYWORD_LURE));
            alternate_legacy_text(2, player, result);
          }
          kill_card(player, card, KILL_EXILE);
        }
        kill_card(player, card, KILL_DESTROY);
      }
    }
  }
  result = aftermath(player, card, event, 3, 0, 0, 1, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22e5260; original cards/amonkhet.c:4993. */
int card_onward_victory(int player, int card, event_t event)
{
  /* Onward // Victory	|2|R|/|2|W
   * Instant // Sorcery
   * Target creature gets +X/+0 until end of turn, where X is its power.
   * //
   * Aftermath
   * Target creature gains double strike until end of turn. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY);
    if (result == 0)
    {
      return 0;
    }
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
    if (result != 0)
    {
      result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 2, 0, 0, 0, 0, 1, 0);
      return result;
    }
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
      return result;
    }
    if ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_PLAY_FIRST_HALF)
    {
      result = generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
      return result;
    }
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = get_aftermath(player, card);
      if ((result == 0) && (result = valid_target(&td), result != 0))
      {
        result = get_power(instance->targets[0].player, instance->targets[0].card);
        result = pump_until_eot(player, card, instance->targets[0].player,
                                instance->targets[0].card, result, 0);
        alternate_legacy_text(1, player, result);
      }
      result = get_aftermath(player, card);
      if (result != 0)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          result = pump_ability_until_eot(player, card, instance->targets[0].player,
                                          instance->targets[0].card, 0, 0, 0x40000000, 0);
          alternate_legacy_text(2, player, result);
        }
        kill_card(player, card, KILL_EXILE);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  return aftermath(player, card, event, 2, 0, 0, 0, 0, 1, 0);
}

/* ManalinkEh 2022: 0x22e5590; original cards/amonkhet.c:5040. */
int card_spring_mind(int player, int card, event_t event)
{
  /* Spring // Mind	|2|G|/|4|U|U
   * Sorcery // Instant
   * Search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library.
   * //
   * Aftermath
   * Draw two cards. */

  if (event == EVENT_CAN_CAST)
  {
    if (can_sorcery_be_played(player, EVENT_CAN_CAST))
      return basic_spell(player, card, EVENT_CAN_CAST);
  }
  else if (event == EVENT_GRAVEYARD_ABILITY)
  {
    if (basic_spell(player, card, EVENT_CAN_CAST))
      return aftermath(player, card, event, MANACOST_XU(4, 2), 0);
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    if (!get_aftermath(player, card))
      tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 1);
    // The release queries again after the tutor; preserve that call boundary.
    if (get_aftermath(player, card))
      draw_cards(player, 2);
    kill_card(player, card, KILL_DESTROY);
  }
  return aftermath(player, card, event, MANACOST_XU(4, 2), 0);
}

/* ManalinkEh 2022: 0x22e5730; original cards/amonkhet.c:5072. */
int card_prepare_fight(int player, int card, event_t event)
{
  /* Prepare // Fight	|1|W|/|3|G
   * Instant // Sorcery
   * Untap target creature. It gets +2/+2 and gains lifelink until end of turn.
   * //
   * Aftermath
   * Target creature you control fights target creature an opponent controls. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  target_definition_t td_control;
  target_definition_t td_dont_control;

  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td_control, 2);
  td_control.preferred_controller = player;
  td_control.allowed_controller = player;
  default_target_definition(player, card, &td_dont_control, 2);
  td_dont_control.allowed_controller = 1 - player;
  td_dont_control.preferred_controller = td_dont_control.allowed_controller;
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY);
    if (result == 0)
    {
      return 0;
    }
    result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td_control, NULL, 1, NULL);
    if ((result != 0) && (result = can_target(&td_dont_control), result != 0))
    {
      result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 3, 0, 0, 1, 0, 0, 0);
      return result;
    }
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
      return result;
    }
    if (event == EVENT_PLAY_FIRST_HALF)
    {
      generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
    }
    else if (event == EVENT_PLAY_SECOND_HALF)
    {
      result = pick_target(&td_control, "ASHNODS_BATTLEGEAR");
      if ((result == 0) ||
          (result = new_pick_target(&td_dont_control, "TARGET_CREATURE_OPPONENT_CONTROLS", 1, 1),
           result == 0))
      {
        instance->number_of_targets = '\0';
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      result = get_aftermath(player, card);
      if ((result == 0) && (result = valid_target(&td), result != 0))
      {
        untap_card(instance->targets[0].player, instance->targets[0].card);
        pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                               2, 2, 0, 2);
      }
      result = get_aftermath(player, card);
      if (result != 0)
      {
        result = validate_target(player, card, &td_dont_control, 1);
        if ((result != 0) && (result = valid_target(&td_control), result != 0))
        {
          fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player,
                instance->targets[1].card);
        }
        kill_card(player, card, KILL_EXILE);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  result = aftermath(player, card, event, 3, 0, 0, 1, 0, 0, 0);
  return result;
}

/* Failure // Comply	|1|U|/|W
 * Instant // Sorcery
 * Return target spell to its owner's hand.
 * //
 * Aftermath
 * Choose a card name. Until your next turn, your opponents can't cast spells with the chosen name. */

/* ManalinkEh 2022: 0x22e5b40; original cards/amonkhet.c:5142. */
int card_rags_riches(int player, int card, event_t event)
{
  /* Rags // Riches	|2|B|B|/|5|U|U
   * Sorcery // Sorcery
   * All creatures get -2/-2 until end of turn.
   * //
   * Aftermath
   * Each opponent chooses a creature they control. You gain control of those creatures. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    return result;
  }
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = basic_spell(player, card, EVENT_GRAVEYARD_ABILITY);
    if (result != 0)
    {
      result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, 5, 0, 2, 0, 0, 0, 0);
      return result;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    result = get_aftermath(player, card);
    if (result == 0)
    {
      pump_creatures_until_eot(player, card, 2, 1, -2, -2, 0, 0, NULL);
    }
    result = get_aftermath(player, card);
    if (result != 0)
    {
      base_target_definition(player, card, &td, 2);
      td.who_chooses = 1 - player;
      td.allow_cancel = 0;
      td.allowed_controller = td.who_chooses;
      td.preferred_controller = td.who_chooses;
      instance = get_card_instance(player, card);
      result = can_target(&td);
      if ((result != 0) &&
          (result = pick_target(&td, "SELECT_A_CREATURE_YOU_CONTROL"), result != 0))
      {
        gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = aftermath(player, card, event, 5, 0, 2, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22e5d70; original cards/amonkhet.c:5181. */
int card_cut_ribbons(int player, int card, event_t event)
{
  /* Cut // Ribbons	|1|R|/|X|B|B
   * Sorcery // Sorcery
   * Cut deals 4 damage to target creature.
   * //
   * Aftermath
   * Each opponent loses X life. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = generic_spell(player, card, EVENT_GRAVEYARD_ABILITY, 4, NULL, NULL, 0, NULL);
    if (result != 0)
    {
      aftermath(player, card, EVENT_GRAVEYARD_ABILITY, -1, 2, 0, 0, 0, 0, 0);
    }
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      result = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
      return result;
    }
    if (event == EVENT_PLAY_FIRST_HALF)
    {
      result = generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL);
      return result;
    }
    if (event == EVENT_PLAY_SECOND_HALF)
    {
      result = generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 0, NULL);
      return result;
    }
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = get_aftermath(player, card);
      if ((result == 0) && (result = valid_target(&td), result != 0))
      {
        damage_target0(player, card, 4);
      }
      result = get_aftermath(player, card);
      if (result != 0)
      {
        instance = get_card_instance(player, card);
        result = x_value;
        instance->targets[1].card = x_value;
        lose_life(1 - player, result);
        kill_card(player, card, KILL_EXILE);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  result = aftermath(player, card, event, -1, 2, 0, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22e6050; original cards/amonkhet.c:5225. */
int card_heaven_earth(int player, int card, event_t event)
{
  /* Heaven // Earth	|X|G|/|X|R|R
   * Instant // Sorcery
   * Heaven deals X damage to each creature with flying.
   * //
   * Aftermath
   * Earth deals X damage to each creature without flying. */

  int result;
  card_instance_t *instance;
  test_definition_t this_test;

  if (event == EVENT_CAN_CAST)
  {
    result = generic_spell(player, card, EVENT_CAN_CAST, 4, NULL, NULL, 1, NULL);
    return result;
  }
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = can_sorcery_be_played(player, EVENT_GRAVEYARD_ABILITY);
    if (result == 0)
    {
      return 0;
    }
    result = generic_spell(player, card, EVENT_GRAVEYARD_ABILITY, 4, NULL, NULL, 1, NULL);
    if (result != 0)
    {
      result = aftermath(player, card, EVENT_GRAVEYARD_ABILITY, -1, 0, 0, 0, 2, 0, 0);
      return result;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    instance->info_slot = x_value;
    result = get_aftermath(player, card);
    if (result == 0)
    {
      default_test_definition(&this_test, 2);
      this_test.keyword = 0x20;
      new_damage_all(player, card, 2, instance->info_slot, 0, &this_test);
    }
    result = get_aftermath(player, card);
    if (result != 0)
    {
      default_test_definition(&this_test, 2);
      this_test.keyword = 0x20;
      this_test.keyword_flag = 1;
      new_damage_all(player, card, 2, instance->info_slot, 0, &this_test);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = aftermath(player, card, event, -1, 0, 0, 0, 2, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22e62f0; original cards/amonkhet.c:5268. */
int card_bontus_monument(int player, int card, event_t event)
{
  /* Bontu's Monument	|3
   * Legendary Artifact
   * |SBlack creature spells you cast cost |1 less to cast.
   * Whenever you cast a creature spell, each opponent loses 1 life and you gain 1 life. */

  unsigned int spell_colors;
  unsigned int affected_color;

  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if (affected_card_controller == player)
    {
      if (!is_humiliated(player, card, AL_STATIC))
      {
        if (is_what(affected_card_controller, affected_card, 2))
        {
          spell_colors = get_color(affected_card_controller, affected_card);
          affected_color = get_sleighted_color_test(affected_card_controller, affected_card, 2);
          if ((spell_colors & affected_color) != 0)
          {
            COST_COLORLESS = COST_COLORLESS + -1;
          }
        }
      }
    }
    sp_ability_trigger(player, EVENT_MODIFY_COST_GLOBAL, 2, TYPE_CREATURE, NULL, player, card);
    return 0;
  }
  sp_ability_trigger(player, event, 2, TYPE_CREATURE, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  lose_life(1 - player, 1);
  gain_life(player, 1);
  return 0;
}

/* ManalinkEh 2022: 0x22e6460; original cards/amonkhet.c:5294. */
int card_edifice_of_authority(int player, int card, event_t event)
{
  /* Edifice of Authority	|3
   * Artifact
   * |1, |T: Target creature can't attack this turn. Put a brick counter on ~.
   * |1, |T: Until your next turn, target creature can't attack or block and its activated abilities can't be activated. Activate this ability only if there are three or more brick counters on ~. */

  enum
  {
    CHOICE_NO_ATTACK = 1,
    CHOICE_NO_ABILS = 2
  };

  card_instance_t *instance;
  int result;
  bool not_speculating;
  bool first_prompt = false;
  const char *prompt;
  target_definition_t td;

  if ((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE))
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 1 - player;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      result = instance->info_slot;
      if (result == 1)
      {
        create_targetted_legacy_effect(instance->parent_controller, instance->parent_card,
                                       effect_cannot_attack_until_eot, instance->targets[0].player,
                                       instance->targets[0].card);
        add_counters(player, card, player, card, COUNTER_BRICK, 1);
        result = instance->info_slot;
      }
      if (result == 2)
      {
        detain(player, card, instance->targets[0].player, instance->targets[0].card);
      }
    }
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = 1 - player;
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 1, 0, 0, 0, 0, 0, 0, &td,
                                     NULL);
  }

  instance->number_of_targets = '\0';
  result = count_counters(player, card, COUNTER_BRICK);
  if ((result < 3) || (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0,
                                                          0, 0, 0, 0, 0, &td, NULL),
                       result == 0))
  {
    instance->info_slot = 1;
    prompt = "Select target creature it can\'t attack this turn.";
  }
  else
  {
    not_speculating = ai_is_speculating != 1;
    instance->info_slot = 2;
    if ((not_speculating) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
    {
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                           "Target creature can't attack", 1, 2, "Target creature no abilities", 1,
                           10, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_NO_ATTACK)
        first_prompt = true;
      if (result != CHOICE_NO_ATTACK && result != CHOICE_NO_ABILS)
      {
        return 0;
      }
    }
    prompt = "Select target creature it can\'t attack or block nor activate abilities.";
    if (first_prompt)
      prompt = "Select target creature it can\'t attack this turn.";
  }
  return generic_activated_ability(player, card, EVENT_ACTIVATE, 7, 1, 0, 0, 0, 0, 0, 0, &td,
                                   prompt);
}

/* Embalmer's Tools	|2
 * Artifact
 * Activated abilities of creature cards in your graveyard cost |1 less to activate.
 * Tap an untapped Zombie you control: Target player puts the top card of their library into their graveyard. */

/* ManalinkEh 2022: 0x22d7030; original cards/amonkhet.c:5371. */
static int gate_to_the_afterlife_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  card_instance_t *dying;
  unsigned int pending_deaths;
  int result;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (result = get_id((int)instance->damage_target_player, instance->damage_target_card),
           result != 0x4212))
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
          return 0;
        }
        count_for_gfp_ability_and_store_values(player, card, 0x77, player, 2, NULL, 0x40, 0);
      }
    }
  }
  pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_deaths != 0)
  {
    result = check_status(player, card, 0x10000);
    if (result == 0)
    {

      pending_deaths = pending_deaths & 0xff;
      if (pending_deaths != 0)
      {
        do
        {
          gain_life(player, 1);
          result = count_deck(player);
          result = do_dialog(player, player, card, -1, -1, " Draw & discard\n Pass",
                             (unsigned int)(result < 0xb));
          if (result != 0)
            break;
          draw_cards(player, 1);
          pending_deaths = pending_deaths - 1;
          discard(player, 0, player);
        } while (0 < (int)pending_deaths);
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

/* ManalinkEh 2022: 0x22e6820; original cards/amonkhet.c:5432. */
int card_gate_to_the_afterlife(int player, int card, event_t event)
{
  /* Gate to the Afterlife	|3
   * Artifact
   * Whenever a nontoken creature you control dies, you gain 1 life. Then you may draw a card. If you do, discard a card.
   * |2, |T, Sacrifice ~: Search your graveyard, hand, and/or library for a card named God-Pharaoh's Gift and put it onto the battlefield. If you search your library this way, shuffle it. Activate this ability only if there are six or more creature cards in your graveyard. */

  int c;
  card_instance_t *instance;
  int created_or_result;
  unsigned int pending_deaths;
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
                instance->info_slot_fn == gate_to_the_afterlife_effect)) &&
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
        created_or_result = create_targetted_legacy_effect(
            player, card, gate_to_the_afterlife_effect, player, card);
        instance = get_card_instance(player, created_or_result);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, created_or_result, 0x10000);
      }
    }
    count_for_gfp_ability_and_store_values(player, card, 0x4700, player, 2, NULL, 0x40, 0);
    instance = get_card_instance(player, card);
    if (instance->kill_code != '\0')
    {
      return 0;
    }
  }
  else
  {
    count_for_gfp_ability_and_store_values(player, card, event, player, 2, NULL, 0x40, 0);
    instance = get_card_instance(player, card);
  }

  if (instance->kill_code == '\0')
  {
    pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    pending_deaths = pending_deaths & 0xff;
    if (pending_deaths != 0)
    {
      do
      {
        gain_life(player, 1);
        created_or_result = count_deck(player);
        if (do_dialog(player, player, card, -1, -1, " Draw & discard\n Pass",
                      (unsigned int)(created_or_result < 0xb)) != 0)
          break;
        draw_cards(player, 1);
        pending_deaths = pending_deaths - 1;
        discard(player, 0, player);
      } while (0 < (int)pending_deaths);
    }
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    created_or_result = count_graveyard_by_type(player, 2);
    if (5 < created_or_result)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 2, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    created_or_result = count_graveyard_by_type(player, 2);
    if (5 < created_or_result)
    {
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 9, 2, 0, 0, 0, 0, 0, 0, NULL,
                                       NULL);
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {

    new_default_test_definition(&this_test, 0x47, "Select a card named God-Pharaoh\'s Gift.");
    this_test.id = CARD_ID_GOD_PHARAOHS_GIFT;
    this_test.zone = 0x100;
    created_or_result = check_battlefield_for_special_card(player, card, player, 0, &this_test);
    if ((created_or_result != 0) ||
        (created_or_result = new_global_tutor(player, player, 4, 4, 0, 5, &this_test),
         created_or_result < 0))
    {
      if (new_special_count_grave(player, &this_test) != 0)
      {
        new_global_tutor(player, player, 2, 4, 0, 5, &this_test);
      }
      new_global_tutor(player, player, 1, 4, 0, 5, &this_test);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22e6d00; original cards/amonkhet.c:5517. */
int card_hazorets_monument(int player, int card, event_t event)
{
  /* Hazoret's Monument	|3
   * Legendary Artifact
   * |SRed creature spells you cast cost |1 less to cast.
   * Whenever you cast a creature spell, you may discard a card. If you do, draw a card. */

  int result;
  unsigned int spell_colors;
  unsigned int affected_color;

  check_legend_rule(player, card, event);
  if (event != EVENT_MODIFY_COST_GLOBAL)
  {
    result = duh_mode(player);
    sp_ability_trigger(player, event, (result != 0) + 1, TYPE_CREATURE, NULL, player, card);
    if (event != EVENT_SPA)
    {
      return 0;
    }
    draw_cards(player, 1);
    discard(player, 0, player);
    return 0;
  }
  if ((affected_card_controller == player) &&
      (result = is_humiliated(player, card, AL_STATIC), result == 0))
  {
    result = is_what(affected_card_controller, affected_card, 2);
    if (result != 0)
    {
      spell_colors = get_color(affected_card_controller, affected_card);
      affected_color = get_sleighted_color_test(affected_card_controller, affected_card, 0x10);
      if ((spell_colors & affected_color) != 0)
      {
        COST_COLORLESS = COST_COLORLESS + -1;
      }
    }
    result = duh_mode(player);
    if (result != 0)
    {
      sp_ability_trigger(player, EVENT_MODIFY_COST_GLOBAL, 2, TYPE_CREATURE, NULL, player, card);
      return 0;
    }
    sp_ability_trigger(player, EVENT_MODIFY_COST_GLOBAL, 1, TYPE_CREATURE, NULL, player, card);
    return 0;
  }
  result = duh_mode(player);
  if (result != 0)
  {
    sp_ability_trigger(player, EVENT_MODIFY_COST_GLOBAL, 2, TYPE_CREATURE, NULL, player, card);
    return 0;
  }
  sp_ability_trigger(player, EVENT_MODIFY_COST_GLOBAL, 1, TYPE_CREATURE, NULL, player, card);
  return 0;
}

/* Honed Khopesh	|1	=>mirrodin.c:card_leonin_scimitar 0x200529b
 * Artifact - Equipment
 * Equipped creature gets +1/+1.
 * Equip |1 */

/* ManalinkEh 2022: 0x22e6f40; original cards/amonkhet.c:5548. */
int card_kefnets_monument(int player, int card, event_t event)
{
  /* Kefnet's Monument	|3
   * Legendary Artifact
   * |SBlue creature spells you cast cost |1 less to cast.
   * Whenever you cast a creature spell, target creature an opponent controls doesn't untap during its controller's next untap step. */

  card_instance_t *instance;
  int result;
  unsigned int spell_colors;
  unsigned int affected_color;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if (affected_card_controller == player)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = is_what(affected_card_controller, affected_card, 2);
        if (result != 0)
        {
          spell_colors = get_color(affected_card_controller, affected_card);
          affected_color = get_sleighted_color_test(affected_card_controller, affected_card, 4);
          if ((spell_colors & affected_color) != 0)
          {
            COST_COLORLESS = COST_COLORLESS + -1;
          }
        }
      }
    }
    sp_ability_trigger(player, EVENT_MODIFY_COST_GLOBAL, 2, TYPE_CREATURE, NULL, player, card);
    return 0;
  }
  sp_ability_trigger(player, event, 2, TYPE_CREATURE, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }

  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  td.allow_cancel = 0;
  td.preferred_controller = td.allowed_controller;
  if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
  {
    td.illegal_state = TARGET_STATE_TAPPED;
    result = can_target(&td);
    if (result == 0)
    {
      td.illegal_state = 0;
    }
  }
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS");
    if (result != 0)
    {
      does_not_untap_effect(player, card, instance->targets[0].player, instance->targets[0].card, 0,
                            1);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x22e7180; original cards/amonkhet.c:5587. */
int card_luxa_river_shrine(int player, int card, event_t event)
{
  /* Luxa River Shrine	|3
   * Artifact
   * |1, |T: You gain 1 life. Put a brick counter on ~.
   * |T: You gain 2 life. Activate this ability only if there are three or more brick counters on ~. */

  enum
  {
    CHOICE_1_LIFE = 1,
    CHOICE_2_LIFE = 2
  };

  card_instance_t *instance;
  int result;
  bool not_speculating;
  bool first_choice = false;

  if ((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE))
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = instance->info_slot;
      if (result == 1)
      {
        gain_life(player, 1);
        add_counters(player, card, player, card, COUNTER_BRICK, 1);
        result = instance->info_slot;
      }
      if (result == 2)
      {
        gain_life(player, 2);
        return 0;
      }
    }
    return 0;
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = count_counters(player, card, COUNTER_BRICK);
    return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1,
                                     (unsigned int)(result < 3), 0, 0, 0, 0, 0, 0, NULL, NULL);
  }

  bool pay_one = true;
  result = count_counters(player, card, COUNTER_BRICK);
  if ((result < 3) || (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0,
                                                          0, 0, 0, 0, 0, NULL, NULL),
                       result == 0))
  {
    instance->info_slot = 1;
  }
  else
  {
    not_speculating = ai_is_speculating != 1;
    instance->info_slot = 2;
    if ((not_speculating) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
    {
      result =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                      "Gain 1 life & a brick counter.", 1, 2, "Gain 2 life.", 1, 10, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_1_LIFE)
        first_choice = true;
      if (result != CHOICE_1_LIFE && result != CHOICE_2_LIFE)
      {
        return 0;
      }
    }
    if (!first_choice)
    {
      result = count_counters(player, card, COUNTER_BRICK);
      if (result < 3)
        return 0;
      pay_one = false;
    }
  }
  return generic_activated_ability(player, card, EVENT_ACTIVATE, 1, pay_one, 0, 0, 0, 0, 0, 0, NULL,
                                   NULL);
}

/* ManalinkEh 2022: 0x22e7530; original cards/amonkhet.c:5651. */
int card_oketras_monument(int player, int card, event_t event)
{
  /* Oketra's Monument	|3
   * Legendary Artifact
   * |SWhite creature spells you cast cost |1 less to cast.
   * Whenever you cast a creature spell, create a 1/1 |Swhite Warrior creature token with vigilance. */

  unsigned int spell_colors;
  unsigned int affected_color;

  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if (affected_card_controller == player)
    {
      if (!is_humiliated(player, card, AL_STATIC))
      {
        if (is_what(affected_card_controller, affected_card, 2))
        {
          spell_colors = get_color(affected_card_controller, affected_card);
          affected_color = get_sleighted_color_test(affected_card_controller, affected_card, 0x20);
          if ((spell_colors & affected_color) != 0)
          {
            COST_COLORLESS = COST_COLORLESS + -1;
          }
        }
      }
    }
    sp_ability_trigger(player, EVENT_MODIFY_COST_GLOBAL, 2, TYPE_CREATURE, NULL, player, card);
    return 0;
  }
  sp_ability_trigger(player, event, 2, TYPE_CREATURE, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  vigilant_white_warriors(player, card, 1);
  return 0;
}

/* ManalinkEh 2022: 0x22e7680; original cards/amonkhet.c:5676. */
int card_oracles_vault(int player, int card, event_t event)
{
  /* Oracle's Vault	|4
   * Artifact
   * |2, |T: Exile the top card of your library. Until end of turn, you may play that card. Put a brick counter on ~.
   * |T: Exile the top card of your library. Until end of turn, you may play that card without paying its mana cost. Activate this ability only if there are three or more brick counters on ~. */

  enum
  {
    CHOICE_EXILE_PAY = 1,
    CHOICE_EXILE_FREE = 2
  };

  int choice;
  card_instance_t *instance;
  int cost_or_mode;
  int counters_or_available;
  unsigned int free_play_available;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      cost_or_mode = count_counters(player, card, COUNTER_BRICK);
      if (2 < cost_or_mode)
      {
        generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL,
                                  NULL);
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 2, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      cost_or_mode = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 2, 0, 0, 0, 0,
                                               0, 0, NULL, NULL);
      counters_or_available = count_counters(player, card, COUNTER_BRICK);
      free_play_available = 0;
      if (2 < counters_or_available)
      {
        counters_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0,
                                                          0, 0, 0, 0, 0, NULL, NULL);
        free_play_available = (unsigned int)(counters_or_available != 0);
      }
      instance->info_slot = 0;
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                           "Exile & play that card", cost_or_mode, 2, "Exile play for free",
                           free_play_available, 10, 0, 0, 0, 0);
      if (choice != 0)
      {
        if (choice == CHOICE_EXILE_PAY)
        {
          cost_or_mode = 2;
        }
        else
        {
          if (choice != CHOICE_EXILE_FREE)
          {
            return 0;
          }
          cost_or_mode = 0;
        }
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 1, cost_or_mode, 0, 0, 0, 0,
                                         0, 0, NULL, NULL);
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
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    cost_or_mode = instance->info_slot;
    if (cost_or_mode == CHOICE_EXILE_PAY)
    {
      if (*deck_ptr[player] == -1)
      {
        return 0;
      }
      create_may_play_card_from_exile_effect(
          player, card, player, (unsigned int)cards_data[*deck_ptr[player]].id, MPCFE_UNTIL_EOT);
      rfg_top_card_of_deck(player);
      add_counters(player, card, player, card, COUNTER_BRICK, 1);
      cost_or_mode = instance->info_slot;
    }
    if ((cost_or_mode == CHOICE_EXILE_FREE) && (*deck_ptr[player] != -1))
    {
      create_may_play_card_from_exile_effect(player, card, player,
                                             (unsigned int)cards_data[*deck_ptr[player]].id,
                                             MPCFE_UNTIL_EOT | MPCFE_FOR_FREE);
      rfg_top_card_of_deck(player);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22e7b20; original cards/amonkhet.c:5746. */
int card_pyramid_of_the_pantheon(int player, int card, event_t event)
{
  /* Pyramid of the Pantheon	|1
   * Artifact
   * |2, |T: Add one mana of any color. Put a brick counter on ~.
   * |T: Add three mana of any one color. Activate this ability only if there are three or more brick counters on ~. */

  enum
  {
    CHOICE_1MANA = 1,
    CHOICE_3MANA = 2
  };

  if (event == EVENT_CAN_ACTIVATE)
  {
    int mana_amount;
    if (!generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 2, 0, 0, 0, 0, 0, 0, NULL,
                                   NULL))
    {
      if (count_counters(player, card, COUNTER_BRICK) < 3 ||
          !generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL,
                                     NULL))
      {
        get_card_instance(player, card);
        return 0;
      }
      mana_amount = 3;
    }
    else
      mana_amount = 1;
    return artifact_mana_all_one_color(player, card, EVENT_CAN_ACTIVATE, mana_amount, 0);
  }
  if (event != EVENT_ACTIVATE)
  {
    if (event == EVENT_RESOLVE_ACTIVATION || event == EVENT_CLEANUP)
      get_card_instance(player, card);
    return 0;
  }
  card_instance_t *instance = get_card_instance(player, card);
  int one_mana_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 3, 0, 0,
                                                     0, 0, 0, 0, NULL, NULL);
  bool add_brick = true;
  if (count_counters(player, card, COUNTER_BRICK) < 3)
  {
    instance->info_slot = 1;
    paying_mana();
  }
  else
  {
    int three_mana_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0,
                                                         0, 0, 0, 0, 0, NULL, NULL);
    instance->info_slot = 1;
    if (!three_mana_available)
      paying_mana();
    else if (!paying_mana())
    {
      int choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Get 1 Mana",
                               one_mana_available, 5, "Get 3 mana", 1, 10, 0, 0, 0, 0);
      if (!choice)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice != CHOICE_1MANA)
      {
        if (choice != CHOICE_3MANA || count_counters(player, card, COUNTER_BRICK) < 3 ||
            !generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL,
                                       NULL))
          return 0;
        add_brick = false;
      }
    }
  }
  if (add_brick)
  {
    if (!generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 2, 0, 0, 0, 0, 0, 0, NULL,
                                   NULL))
      return 0;
    add_counters(player, card, player, card, COUNTER_BRICK, 1);
  }
  return artifact_mana_all_one_color(player, card, EVENT_ACTIVATE, add_brick ? 1 : 3, 0);
}

/* ManalinkEh 2022: 0x22e8010; original cards/amonkhet.c:5801. */
int card_rhonass_monument(int player, int card, event_t event)
{
  /* Rhonas's Monument	|3
   * Legendary Artifact
   * |SGreen creature spells you cast cost |1 less to cast.
   * Whenever you cast a creature spell, target creature you control gets +2/+2 and gains trample until end of turn. */

  card_instance_t *instance;
  int result;
  unsigned int spell_colors;
  unsigned int affected_color;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if (affected_card_controller == player)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = is_what(affected_card_controller, affected_card, 2);
        if (result != 0)
        {
          spell_colors = get_color(affected_card_controller, affected_card);
          affected_color = get_sleighted_color_test(affected_card_controller, affected_card, 8);
          if ((spell_colors & affected_color) != 0)
          {
            COST_COLORLESS = COST_COLORLESS + -1;
          }
        }
      }
    }
    sp_ability_trigger(player, EVENT_MODIFY_COST_GLOBAL, 2, TYPE_CREATURE, NULL, player, card);
    return 0;
  }
  sp_ability_trigger(player, event, 2, TYPE_CREATURE, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }

  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.allow_cancel = 0;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = pick_target(&td, "TARGET_CREATURE_YOU_CONTROL");
    if (result != 0)
    {
      pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                             2, 2, 0x80, 0);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x22e8210; original cards/amonkhet.c:5836. */
int card_throne_of_the_god_pharaoh(int player, int card, event_t event)
{
  /* Throne of the God-Pharaoh	|2
   * Legendary Artifact
   * At the beginning of your end step, each opponent loses life equal to the number of tapped creatures you control. */

  int result;
  test_definition_t this_test;
  check_legend_rule(player, card, event);
  if (current_turn == player)
  {
    result = eot_trigger(player, card, event);
    if (result != 0)
    {
      default_test_definition(&this_test, 2);
      this_test.state = 0x10;
      result = check_battlefield_for_special_card(player, card, player, 4, &this_test);
      lose_life(1 - player, result);
    }
  }
  return 0;
}

/* Watchers of the Dead	|2
 * Artifact Creature - Cat 2/2
 * Exile ~: Each opponent chooses two cards in their graveyard and exiles the rest. */

/* Canyon Slough	""	=>urza_saga.c:card_drifting_meadow 0x200a21e
 * Land - |H2Swamp |H2Mountain
 * ~ enters the battlefield tapped.
 * Cycling |2 */

/* ManalinkEh 2022: 0x22e82c0; original cards/amonkhet.c:5862. */
int card_cascading_cataracts(int player, int card, event_t event)
{
  /* Cascading Cataracts	""
   * Land
   * Indestructible
   * |T: Add |C.
   * |5, |T: Add five mana in any combination of colors. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_5MANA = 2
  };

  card_instance_t *instance;
  int result;

  indestructible(player, card, event);
  if (event == EVENT_COUNT_MANA)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = is_tapped(player, card), result == 0)) &&
        ((result = is_animated_and_sick(player, card),
          result == 0 && (result = can_produce_mana(player, card), result != 0))))
    {
      result = has_mana(player, 7, 5);
      if (result == 0)
      {
        declare_mana_available(player, 0, 1);
      }
      else
      {
        declare_mana_available_any_combination_of_colors(player, COLOR_TEST_ANY_COLORED, 5);
      }
    }
    get_card_instance(player, card);
    return 0;
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 6, 0, 0, 0, 0, 0, 0, NULL, NULL);
    result = mana_producer(player, card, EVENT_CAN_ACTIVATE);
    return result;
  }
  if (event != EVENT_ACTIVATE)
  {
    return 0;
  }

  instance->info_slot = 1;
  result = has_mana(player, 7, 6);
  if (result != 0)
  {
    result = has_mana(player, 7, 6);
    result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                         "Get 5 mana any compination", result, 1, 0, 0, 0, 0);
    if (result == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (result != CHOICE_MANA)
    {
      if (result != CHOICE_5MANA)
      {
        return 0;
      }
      add_state(player, card, 0x10);
      result = charge_mana_for_activated_ability(player, card, 5, 0, 0, 0, 0, 0);
      if (result != 0)
      {
        produce_mana_any_combination_of_colors(player, COLOR_TEST_ANY_COLORED, 5, NULL);
        return 0;
      }
      untap_card_no_event(player, card);
      return 0;
    }
  }
  produce_mana_tapped(player, card, COLOR_COLORLESS, 1);
  return 0;
}

/* ManalinkEh 2022: 0x22e85d0; original cards/amonkhet.c:5921. */
int card_cradle_of_the_accursed(int player, int card, event_t event)
{
  /* Cradle of the Accursed	""
   * Land - Desert
   * |T: Add |C.
   * |3, |T, Sacrifice ~: Create a 2/2 |Sblack Zombie creature token. Activate this ability only any time you could cast a sorcery. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_SAC = 2
  };

  int result;
  card_instance_t *instance;
  bool not_speculating;
  bool produce_mana = false;
  token_generation_t token;

  if (event == EVENT_CAN_ACTIVATE)
  {
    return mana_producer(player, card, event);
  }
  if (event != EVENT_ACTIVATE)
  {
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
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
      default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
      generate_token(&token);
      return 0;
    }
    return mana_producer(player, card, event);
  }
  instance = get_card_instance(player, card);

  result = paying_mana();
  if (((result == 0) && (result = can_sorcery_be_played(player, EVENT_ACTIVATE), result != 0)) &&
      (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 4, 0, 0, 0, 0, 0, 0,
                                          NULL, NULL),
       result != 0))
  {
    not_speculating = ai_is_speculating != 1;
    instance->info_slot = 2;
    if ((not_speculating) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
    {
      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                           "Sacrifice for a Zombie", 1, 1, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_MANA)
        produce_mana = true;
      if (result != CHOICE_MANA && result != CHOICE_SAC)
      {
        return 0;
      }
    }
    if (!produce_mana)
    {
      add_state(player, card, 0x10);
      generic_activated_ability(player, card, EVENT_ACTIVATE, 8, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        remove_state(player, card, 0x10);
      }
    }
  }
  else
  {
    instance->info_slot = 1;
    produce_mana = true;
  }
  if (produce_mana)
    mana_producer(player, card, EVENT_ACTIVATE);
  return 0;
}

/* Fetid Pools	""	=>urza_saga.c:card_drifting_meadow 0x200a21e
 * Land - |H2Island |H2Swamp
 * ~ enters the battlefield tapped.
 * Cycling |2 */

/* ManalinkEh 2022: 0x22e88b0; original cards/amonkhet.c:5984. */
int card_grasping_dunes(int player, int card, event_t event)
{
  /* Grasping Dunes	""
   * Land - Desert
   * |T: Add |C.
   * |1, |T, Sacrifice ~: Put a -1/-1 counter on target creature. Activate this ability only any time you could cast a sorcery. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_SAC = 2
  };

  int result;
  card_instance_t *instance;
  bool not_speculating;
  target_definition_t td;

  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      default_target_definition(player, card, &td, 2);
      instance = get_card_instance(player, card);

      result = paying_mana();
      if (((result != 0) ||
           (result = can_sorcery_be_played(player, EVENT_ACTIVATE), result == 0)) ||
          (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xc, 2, 0, 0, 0, 0,
                                              0, 0, &td, NULL),
           result == 0))
      {
        instance->info_slot = 1;
        mana_producer(player, card, EVENT_ACTIVATE);
        return 0;
      }
      not_speculating = ai_is_speculating != 1;
      instance->info_slot = 2;
      if ((not_speculating) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
      {
        result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                             "Sacrifice for a counter", 1, 1, 0, 0, 0, 0);
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
        if (result != CHOICE_SAC)
        {
          return 0;
        }
      }
      add_state(player, card, 0x10);
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0xc, 1, 0, 0, 0, 0, 0, 0, &td,
                                "TARGET_CREATURE");
      if (spell_fizzled == 1)
      {
        remove_state(player, card, 0x10);
      }
      return 0;
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      default_target_definition(player, card, &td, 2);
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot == 2)
      {
        add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                     COUNTER_M1_M1, 1);
        return 0;
      }
      return 0;
    }
  }
  return mana_producer(player, card, event);
}

/* Irrigated Farmland	""	=>urza_saga.c:card_drifting_meadow 0x200a21e
 * Land - |H2Plains |H2Island
 * ~ enters the battlefield tapped.
 * Cycling |2 */

/* Painted Bluffs	""
 * Land - Desert
 * |T: Add |C.
 * |1, |T: Add one mana of any color. */

/* Scattered Groves	""	=>urza_saga.c:card_drifting_meadow 0x200a21e
 * Land - |H2Forest |H2Plains
 * ~ enters the battlefield tapped.
 * Cycling |2 */

/* Sheltered Thicket	""	=>urza_saga.c:card_drifting_meadow 0x200a21e
 * Land - |H2Mountain |H2Forest
 * ~ enters the battlefield tapped.
 * Cycling |2 */

/* ManalinkEh 2022: 0x22e8be0; original cards/amonkhet.c:6062. */
int card_sunscorched_desert(int player, int card, event_t event)
{
  /* Sunscorched Desert	""
   * Land - Desert
   * When ~ enters the battlefield, it deals 1 damage to target player or planeswalker.
   * |T: Add |C. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 0x100);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER");
      if (result != 0)
      {
        damage_target0(player, card, 1);
      }
    }
    instance->number_of_targets = '\0';
  }
  result = mana_producer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22e8cb0; original cards/amonkhet.c:6088. */
int card_gideon_martial_paragon(int player, int card, event_t event)
{
  /* Gideon, Martial Paragon	|4|W
   * Legendary Planeswalker - Gideon (5)
   * +2: Untap all creatures you control. Those creatures get +1/+1 until end of turn.
   * 0: Until end of turn, ~ becomes a 5/5 Human Soldier creature with indestructible that's still a planeswalker. Prevent all damage that would be dealt to him this turn.
   * -10: Creatures you control get +2/+2 until end of turn. Tap all creatures your opponents control. */

  enum
  {
    CHOICE_UNTAP = 1,
    CHOICE_ANIMATE = 2,
    CHOICE_BOOST_TAP = 3
  };

  int p;
  int loyalty_or_mode;
  card_instance_t *instance;
  int count_or_toughness;
  int card_00;
  uint32_t ultimate_priority;
  target_definition_t td;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_RESOLVE_ACTIVATION) ||
      (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE))
  {

    instance = get_card_instance(player, card);
    loyalty_or_mode = count_counters(player, card, COUNTER_LOYALTY);
    default_target_definition(player, card, &td, 0x47);
    td.preferred_controller = 1 - player;
    count_or_toughness = count_subtype(player, 2, -1);
    if ((loyalty_or_mode < 10) || (ultimate_priority = 0x5a, count_or_toughness < 4))
    {
      ultimate_priority = 5;
    }
    loyalty_or_mode =
        dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, "Untap your creatures", 1, 10, 2,
                    "Animate Gideon", 1, 5, 0, "+2/+2 & tap opponent crits", 1, ultimate_priority,
                    0xfffffff6, 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
    {
      if (loyalty_or_mode == 0)
      {
        return 0;
      }
    }
    else if (loyalty_or_mode == CHOICE_ANIMATE)
    {
      p = instance->parent_controller;
      count_or_toughness = instance->parent_card;
      instance = in_play(p, count_or_toughness);
      if (instance != NULL)
      {
        card_00 = prevent_all_damage_to_target(p, count_or_toughness, p, count_or_toughness, 1);
        instance = get_card_instance(p, card_00);
        instance->token_status = instance->token_status | 0x10000;
        alternate_legacy_text(2, p,
                              animate_self(p, count_or_toughness, 5, 5, KEYWORD_0,
                                           SP_KEYWORD_INDESTRUCTIBLE, COLOR_TEST_0, 0));
      }
    }
    else if (loyalty_or_mode == CHOICE_UNTAP || loyalty_or_mode == CHOICE_BOOST_TAP)
    {
      default_test_definition(&this_test, 2);
      if (loyalty_or_mode == CHOICE_BOOST_TAP)
      {
        new_manipulate_all(player, card, 1 - player, &this_test, ACT_TAP);
        count_or_toughness = 2;
        loyalty_or_mode = 2;
      }
      else
      {
        new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
        count_or_toughness = 1;
        loyalty_or_mode = 1;
      }
      pump_creatures_until_eot(player, card, player, 1, loyalty_or_mode, count_or_toughness, 0, 0,
                               NULL);
    }
  }
  has_subtype_if_animated_self(player, card, event, SUBTYPE_HUMAN);
  has_subtype_if_animated_self(player, card, event, SUBTYPE_SOLDIER);
  return planeswalker(player, card, event, 5);
}

/* ManalinkEh 2022: 0x22e9080; original cards/amonkhet.c:6166. */
int card_companion_of_the_trials(int player, int card, event_t event)
{
  /* Companion of the Trials	|2|W
   * Creature - Bird Soldier 2/2
   * Flying
   * |1|W: Untap target creature. Activate this ability only if you control a Gideon planeswalker. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = count_subtype(player, 0x47, 0x7005);
      if (result < 1)
      {
        return 0;
      }
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 0, 0, 0, 0, 1, 0,
                                         &td, "TARGET_CREATURE");
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      result = count_subtype(player, 0x47, 0x7005);
      if (0 < result)
      {
        result = generic_activated_ability(player, card, EVENT_ACTIVATE, 4, 1, 0, 0, 0, 0, 1, 0,
                                           &td, "TARGET_CREATURE");
        return result;
      }
      if (spell_fizzled == 1)
      {
        return 0;
      }
      ai_mod_for_untap_effects(player, card, 0, 0);
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    untap_card(instance->targets[0].player, instance->targets[0].card);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x22e92a0; original cards/amonkhet.c:6207. */
int card_gideons_resolve(int player, int card, event_t event)
{
  /* Gideon's Resolve	|4|W
   * Enchantment
   * When ~ enters the battlefield, you may search your library and/or graveyard for a card named Gideon, Martial Paragon, reveal it, and put it into your hand. If you search your library this way, shuffle it.
   * Creatures you control get +1/+1. */

  int result;
  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 0xff, "Select a card named Gideon, Martial Paragon.");
    test.id = CARD_ID_GIDEON_MARTIAL_PARAGON;
    result = new_global_tutor(player, player, 1, 1, 0, 5, &test);
    if (result < 0)
    {
      new_global_tutor(player, player, 2, 1, 0, 5, &test);
    }
  }
  boost_creature_type(player, card, event, ~SUBTYPE_INVALID, 1, 1, 0, BCT_CONTROLLER_ONLY);
  result = global_enchantment(player, card, event);
  return result;
}

/* Graceful Cat	|2|W	=>m12.c:card_benalish_veteran 0x2003ff9
 * Creature - Cat 2/2
 * Whenever ~ attacks, it gets +1/+1 until end of turn. */

/* ManalinkEh 2022: 0x22e93d0; original cards/amonkhet.c:6238. */
int card_liliana_death_wielder(int player, int card, event_t event)
{
  /* Liliana, Death Wielder	|5|B|B
   * Legendary Planeswalker - Liliana (5)
   * +2: Put a -1/-1 counter on up to one target creature.
   * -3: Destroy target creature with a -1/-1 counter on it.
   * -10: Return all creature cards from your graveyard to the battlefield. */

  enum
  {
    CHOICE_M_COUNTER = 1,
    CHOICE_DESTROY = 2,
    CHOICE_REANIMATE_ALL = 3
  };

  int opponent_or_mode;
  int grave_creatures;
  target_definition_t *counter_target;
  target_definition_t *destroy_target;
  card_instance_t *instance;
  target_definition_t td;
  target_definition_t td2;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    opponent_or_mode = 1 - player;
    td.preferred_controller = opponent_or_mode;
    default_target_definition(player, card, &td2, 2);
    td2.special = TARGET_SPECIAL_REQUIRES_COUNTER;
    td2.extra = 1;
    td2.preferred_controller = opponent_or_mode;
    if (event == EVENT_ACTIVATE)
    {

      instance->info_slot = 0;
      instance->number_of_targets = '\0';
      opponent_or_mode = can_target(&td2);
      grave_creatures = count_graveyard_by_type(player, 2);
      opponent_or_mode = dialog_impl(
          player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Put a -1/-1 counter", 1, 5, 2,
          "Destroy creature with -1/-1 counter", opponent_or_mode, 8, 0xfffffffd, "Reanimate all",
          1, ((3 < grave_creatures) ? 80U : 20U), 0xfffffff6, 0, 0, 0, 0);
      if (opponent_or_mode == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      instance->info_slot = opponent_or_mode;
      if (opponent_or_mode == CHOICE_M_COUNTER)
      {
        if (can_target(&td) != 0)
        {
          new_pick_target(&td, "TARGET_CREATURE", 0, 0);
        }
      }
      else if ((opponent_or_mode == CHOICE_DESTROY) &&
               (opponent_or_mode = can_target(&td2), opponent_or_mode != 0))
      {
        new_pick_target(&td2, "TARGET_CREATURE", 0, 1);
      }
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);

    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 1 - player;
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = 1 - player;
  }
  destroy_target = &td2;
  counter_target = &td;
  td2.special = TARGET_SPECIAL_REQUIRES_COUNTER;
  td2.extra = 1;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    opponent_or_mode = instance->info_slot;
    if (opponent_or_mode == 1)
    {
      if (valid_target(counter_target) != 0)
      {
        add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                     COUNTER_M1_M1, 1);
      }
    }
    else if (opponent_or_mode == 2)
    {
      if (valid_target(destroy_target) != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
    }
    else if (opponent_or_mode == 3)
    {
      default_test_definition(&test, 2);
      new_reanimate_all(player, card, player, &test, REANIMATE_DEFAULT);
    }
  }
  return planeswalker(player, card, event, 5);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22e9800; original cards/amonkhet.c:6321. */
int card_desiccated_naga(int player, int card, event_t event)
{
  /* Desiccated Naga	|2|B
   * Creature - Zombie Naga 3/2
   * |3|B: Target opponent loses 2 life and you gain 2 life. Activate this ability only if you control a Liliana planeswalker. */

  card_instance_t *instance;
  int result;
  target_definition_t td2;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td2, 0);
    td2.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = count_subtype(player, 0x47, 0x7009);
      if (result < 1)
      {
        return 0;
      }
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE,
                                              GAA_CAN_ONLY_TARGET_OPPONENT, 3, 1, 0, 0, 0, 0, 0,
                                              &td2, NULL);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      result = count_subtype(player, 0x47, 0x7009);
      if (result < 1)
      {
        return 0;
      }
      result =
          generic_activated_ability(player, card, EVENT_ACTIVATE, GAA_CAN_ONLY_TARGET_OPPONENT,
                                         3, 1, 0, 0, 0, 0, 0, &td2, NULL);
      return result;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td2, 0);
    td2.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td2), result != 0))
  {
    lose_life(instance->targets[0].player, 2);
    gain_life(player, 2);
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x22e9a10; original cards/amonkhet.c:6359. */
int card_lilianas_influence(int player, int card, event_t event)
{
  /* Liliana's Influence	|4|B|B
   * Sorcery
   * Put a -1/-1 counter on each creature you don't control. You may search your library and/or graveyard for a card named Liliana, Death Wielder, reveal it, and put it into your hand. If you search your library this way, shuffle it. */

  int result;
  test_definition_t test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    manipulate_type(player, card, 1 - player, TYPE_CREATURE, 0x1010001);
    new_default_test_definition(&test, 0xff, "Select a card named Liliana, Death Wielder.");
    test.id = CARD_ID_LILIANA_DEATH_WIELDER;
    result = new_global_tutor(player, player, 1, 1, 0, 5, &test);
    if (result < 0)
    {
      new_global_tutor(player, player, 2, 1, 0, 5, &test);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* Tattered Mummy	|1|B	=>scars_of_mirrodin.c:card_blistergrub 0x20041c0
 * Creature - Zombie Jackal 1/2
 * When ~ dies, each opponent loses 2 life. */
