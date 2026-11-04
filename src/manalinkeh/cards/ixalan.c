#include "manalink.h"

// Demystify: reprint from Onslaught

// Air Elemental: reprint from Alpha

// Cancel: reprint from Time Spiral

// Favorable Winds: reprint from Avacyn Restored

// Opt: reprint from Invasion

// Spell Pierce: reprint from Zendikar

// Duress: reprint from Urza's Saga

// Mark of the Vampire: reprint from Magic 2013

// Demolish: reprint from Odyssey

// Dual Shot: reprint from Shadows over Innistrad

// Hijack: reprint from Kaladesh

// Lightning Strike: reprint from Theros

// Rummaging Goblin: reprint from Magic 2013

// Sure Strike: reprint from Battle for Zendikar

// Slice in Twain: reprint from Scars of Mirrodin

// Cobbled Wings: reprint from Innistrad

// Dragonskull Summit: reprint from Magic 2010

// Drowned Catacomb: reprint from Magic 2010

// Glacial Fortress: reprint from Magic 2010

// Rootbound Crag: reprint from Magic 2010

// Sunpetal Grove: reprint from Magic 2010

// Unknown Shores: reprint from Theros

// Plains
// Plains
// Plains
// Plains
// Island
// Island
// Island
// Island
// Swamp
// Swamp
// Swamp
// Swamp
// Mountain
// Mountain
// Mountain
// Mountain
// Forest
// Forest
// Forest
// Forest

// Woodland Stream: reprint from Shadows over Innistrad

// Stone Quarry: reprint from Shadows over Innistrad

static int revel_in_riches_effect(int player, int card, event_t event);
int explore(int player, int card, event_t event);

int card_deeproot_champion(int player, int card, event_t event);

static int test_dinosaur_or_land(int iid, int value, int player, int card);

#ifndef SHANDALAR

int card_dowsing_dagger(int player, int card, event_t event);

#endif

#ifndef SHANDALAR
static int whenever_a_treasure_is_sacrificed(int player, int card, event_t event,
                                               int t_player, int trig_mode)
{
  if (event == EVENT_GRAVEYARD_FROM_PLAY && in_play(affected_card_controller, affected_card) &&
      get_card_instance(affected_card_controller, affected_card)->kill_code == KILL_SACRIFICE &&
      get_id(affected_card_controller, affected_card) == CARD_ID_TREASURE &&
      !check_special_flags(affected_card_controller, affected_card, 0x800))
    count_for_gfp_ability(player, card, event, t_player, 0x47, NULL);
  return resolve_gfp_ability(player, card, event, trig_mode);
}
#endif

/* ManalinkEh 2022: 0x2212970; original cards/ixalan.c:17. */
int explore(int player, int card, event_t event)
{
  int *deck;
  int revealed_iid;
  int result;

  deck = deck_ptr[player];
  revealed_iid = *deck;
  if (revealed_iid != -1)
  {
    reveal_card_iid(player, card, revealed_iid);
    result = is_what(-1, *deck, 1);
    if (result == 0)
    {
      add_counters(player, card, player, card, COUNTER_P1_P1, 1);
      result = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
                           DLG_FULLCARD_ID(*deck), "Put card on top of library", 1, 5,
                           "Put card in graveyard", 1, 5, 0, 0, 0, 0);
      if (result == 2)
      {
        mill(player, 1);
      }
    }
    else
    {
      add_card_to_hand(player, *deck);
      remove_card_from_deck(player, 0);
    }
  }
  dispatch_event(player, card, 0x5700);
  return revealed_iid;
}

/* ManalinkEh 2022: 0x2212ae0; original cards/ixalan.c:41. */
int card_adanto_vanguard(int player, int card, event_t event)
{
  /* Adanto Vanguard	|1|W
   * Creature - Vampire Soldier 1/1
   * As long as ~ is attacking, it gets +2/+0.
   * Pay 4 life: ~ gains indestructible until end of turn. */

  card_instance_t *instance;
  if (event == EVENT_POWER)
  {
    instance = get_card_instance(player, card);
    if ((instance->state & 4) != 0)
    {
      event_result = event_result + 2;
    }
    return 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                           instance->parent_controller, instance->parent_card, 0, 0, 0, 0x800);
  }
  return generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 0, 4, NULL, NULL);
}

/* Ashes of the Abhorrent	|1|W
 * Enchantment
 * Players can't cast spells from graveyards or activate abilities of cards in graveyards.
 * Whenever a creature dies, you gain 1 life. */

/* ManalinkEh 2022: 0x2212c00; original cards/ixalan.c:65. */
int card_axis_of_mortality(int player, int card, event_t event)
{
  /* Axis of Mortality	|4|W|W
   * Enchantment
   * At the beginning of your upkeep, you may have two target players exchange life totals. */

  int result;

  result = duh_mode(player);
  upkeep_trigger_ability_mode(player, card, event, player,
                              (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    result = opponent_is_valid_target(player, card);
    if (result != 0)
    {
      result = do_dialog(player, player, card, -1, -1,
                         " Switch life totals\n Keep life totals as they are", 0);
      if (result == 0)
      {
        exchange_life_totals(player, 1 - player);
      }
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* Bellowing Aegisaur	|5|W
 * Creature - Dinosaur 3/5
 * Enrage - Whenever ~ is dealt damage, put a +1/+1 counter on each other creature you control. */

/* Bishop of Rebirth	|3|W|W
 * Creature - Vampire Cleric 3/4
 * Vigilance
 * Whenever ~ attacks, you may return target creature card with converted mana cost 3 or less from your graveyard to the battlefield. */

/* Bishop's Soldier	|1|W	=>m10.c:card_child_of_night 0x200415c
 * Creature - Vampire Soldier 2/2
 * Lifelink */

/* Bright Reprisal	|4|W
 * Instant
 * Destroy target attacking creature.
 * Draw a card. */

/* Duskborne Skymarcher	|W
 * Creature - Vampire Cleric 1/1
 * Flying
 * |W, |T: Target attacking Vampire gets +1/+1 until end of turn. */

/* Emissary of Sunrise	|2|W
 * Creature - Human Cleric 2/1
 * First strike
 * When ~ enters the battlefield, it explores. */

/* Encampment Keeper	|W
 * Creature - Hound 1/1
 * First strike
 * |7|W, |T, Sacrifice ~: Creatures you control get +2/+2 until end of turn. */

/* Glorifier of Dusk	|3|W|W
 * Creature - Vampire Soldier 4/4
 * Pay 2 life: ~ gains flying until end of turn.
 * Pay 2 life: ~ gains vigilance until end of turn. */

/* Goring Ceratops	|5|W|W
 * Creature - Dinosaur 3/3
 * Double strike
 * Whenever ~ attacks, other creatures you control gain double strike until end of turn. */

/* Imperial Aerosaur	|3|W
 * Creature - Dinosaur 3/3
 * Flying
 * When ~ enters the battlefield, another target creature you control gets +1/+1 and gains flying until end of turn. */

/* Imperial Lancer	|W
 * Creature - Human Knight 1/1
 * ~ has double strike as long as you control a Dinosaur. */

/* Inspiring Cleric	|2|W	=>rise_of_the_eldrazi.c:card_lone_missionary 0x20056dd
 * Creature - Vampire Cleric 3/2
 * When ~ enters the battlefield, you gain 4 life. */

/* ManalinkEh 2022: 0x2212cc0; original cards/ixalan.c:136. */
int card_ixalans_binding(int player, int card, event_t event)
{
  /* Ixalan's Binding	|3|W
   * Enchantment
   * When ~ enters the battlefield, exile target nonland permanent an opponent controls until ~ leaves the battlefield.
   * Your opponents can't cast spells with the same name as the exiled card. */

  if (event == EVENT_CAN_CAST)
    return basic_spell(player, card, EVENT_CAN_CAST);

  if (event == EVENT_MODIFY_COST_GLOBAL && !is_humiliated(player, card, AL_STATIC))
  {
    card_instance_t *instance = get_card_instance(player, card);
    for (int i = 1; i < instance->info_slot; ++i)
    {
      int iid = get_internal_card_id_from_csv_id(instance->targets[i].card);
      if (has_same_name(affected_card_controller, affected_card, -1, iid) &&
          affected_card_controller != player)
        infinite_casting_cost();
    }
  }
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  return_from_oblivion(player, card, event);
  if (event == EVENT_ETB)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_PERMANENT);
    td.allowed_controller = 1 - player;
    td.illegal_type = TYPE_LAND;
    td.allow_cancel = 0;
    td.preferred_controller = td.allowed_controller;
    card_instance_t *instance = get_card_instance(player, card);
    if (!can_target(&td))
    {
      if (player == AI)
        ai_modifier -= 0x30;
    }
    else if (new_pick_target(&td, "Select target nonland permanent an opponent controls.", 0, 0x80))
    {
      int pos = MAX(1, instance->info_slot);
      instance->targets[pos].card = get_card_name(instance->targets[0].player,
                                                 instance->targets[0].card);
      obliviation(player, card, instance->targets[0].player, instance->targets[0].card);
      instance->info_slot = pos + 1;
    }
  }
  return global_enchantment(player, card, event);
}

/* Kinjalli's Caller	|W
 * Creature - Human Cleric 0/3
 * Dinosaur spells you cast cost |1 less to cast. */

/* Kinjalli's Sunwing	|2|W	=>m14.c:card_imposing_sovereign 0x200aa84
 * Creature - Dinosaur 2/3
 * Flying
 * Creatures your opponents control enter the battlefield tapped. */

/* Legion Conquistador	|2|W
 * Creature - Vampire Soldier 2/2
 * When ~ enters the battlefield, you may search your library for any number of cards named ~, reveal them, put them into your hand, then shuffle your library. */

/* Legion's Judgment	|2|W	=>innistrad.c:card_smite_the_monstrous 0x2004a44
 * Sorcery
 * Destroy target creature with power 4 or greater. */

/* ManalinkEh 2022: 0x2212f30; original cards/ixalan.c:201. */
int card_legions_landing(int player, int card, event_t event)
{
  /* Legion's Landing	|W
   * Legendary Enchantment
   * When ~ enters the battlefield, create a 1/1 |Swhite Vampire creature token with lifelink.
   * When you attack with three or more creatures, transform ~. */

  int result;
  token_generation_t token;

  check_legend_rule(player, card, event);
  double_faced_card(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_token_definition(player, card, CARD_ID_VAMPIRE, &token);
    token.color_forced = ' ';
    token.tou = 1;
    token.pow = 1;
    token.s_key_plus = 2;
    generate_token(&token);
    store_attackers(player, card, EVENT_ETB, DAT_ATTACKS_WITH_3_OR_MORE, player, -1, NULL);
    dat_ability(player, card, EVENT_ETB, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  }
  else
  {
    store_attackers(player, card, event, DAT_ATTACKS_WITH_3_OR_MORE, player, -1, NULL);
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      transform(player, card);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22130b0; original cards/ixalan.c:226. */
int card_adanto_the_first_fort(int player, int card, event_t event)
{
  /* Adanto, the First Fort	""
   * Legendary Land
   * |T: Add |W.
   * |2|W, |T: Create a 1/1 |Swhite Vampire creature token with lifelink. */

  int choice;
  card_instance_t *instance;
  int token_available;
  int paying_for_mana;
  token_generation_t token;

  check_legend_rule(player, card, event);
  double_faced_card(player, card, event);
  if (event == EVENT_COUNT_MANA)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      return mana_producer(player, card, EVENT_COUNT_MANA);
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        return mana_producer(player, card, EVENT_CAN_ACTIVATE);
      }
      if (event == EVENT_ACTIVATE)
      {

        token_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 2, 0, 0, 0,
                                                    0, 2, 0, NULL, NULL);
        paying_for_mana = paying_mana();
        if ((paying_for_mana == 0) && (token_available != 0))
        {
          choice = do_dialog(player, player, card, -1, -1,
                             " Produce mana\n Generate a Vampire\n Cancel", 1);
          instance->info_slot = choice;
          if (choice != 0)
          {
            if (choice == 1)
            {
              add_state(player, card, 0x10);
              tapped_for_mana_color = -1;
              if (charge_mana_for_activated_ability(player, card, 2, 0, 0, 0, 0, 1) == 0)
              {
                untap_card_no_event(player, card);
              }
            }
            else
            {
              spell_fizzled = 1;
            }
            tapped_for_mana_color = -1;
            return 0;
          }
        }
        else
        {
          instance->info_slot = 0;
        }
        return mana_producer(player, card, EVENT_ACTIVATE);
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
    if ((event == EVENT_RESOLVE_ACTIVATION) && (instance->info_slot == 1))
    {
      default_token_definition(player, card, CARD_ID_VAMPIRE, &token);
      token.color_forced = ' ';
      token.tou = 1;
      token.pow = 1;
      token.s_key_plus = 2;
      generate_token(&token);
    }
  }
  return 0;
}

/* Looming Altisaur	|3|W	0x401000
 * Creature - Dinosaur 1/7 */

/* Mavren Fein, Dusk Apostle	|2|W
 * Legendary Creature - Vampire Cleric 2/2
 * Whenever one or more nontoken Vampires you control attack, create a 1/1 |Swhite Vampire creature token with lifelink. */

/* Paladin of the Bloodstained	|3|W
 * Creature - Vampire Knight 3/2
 * When ~ enters the battlefield, create a 1/1 |Swhite Vampire creature token with lifelink. */

/* Pious Interdiction	|3|W
 * Enchantment - Aura
 * Enchant creature
 * When ~ enters the battlefield, you gain 2 life.
 * Enchanted creature can't attack or block. */

/* Priest of the Wakening Sun	|W
 * Creature - Human Cleric 1/1
 * At the beginning of your upkeep, you may reveal a Dinosaur card from your hand. If you do, you gain 2 life.
 * |3|W|W, Sacrifice ~: Search your library for a Dinosaur card, reveal it, put it into your hand, then shuffle your library. */

/* Pterodon Knight	|3|W
 * Creature - Human Knight 3/3
 * ~ has flying as long as you control a Dinosaur. */

/* Queen's Commission	|2|W
 * Sorcery
 * Create two 1/1 |Swhite Vampire creature tokens with lifelink. */

/* Rallying Roar	|2|W
 * Instant
 * Creatures you control get +1/+1 until end of turn. Untap them. */

/* Raptor Companion	|1|W	0x401000
 * Creature - Dinosaur 3/1 */

/* Ritual of Rejuvenation	|2|W
 * Instant
 * You gain 4 life.
 * Draw a card. */

/* Sanguine Sacrament	|X|W|W
 * Instant
 * You gain twice X life. Put ~ on the bottom of its owner's library. */

/* ManalinkEh 2022: 0x2213380; original cards/ixalan.c:333. */
int card_settle_the_wreckage(int player, int card, event_t event)
{
  /* Settle the Wreckage	|2|W|W
   * Instant
   * Exile all attacking creatures target player controls. That player may search their library for that many basic land cards, put those cards onto the battlefield tapped, then shuffle their library. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t this_test1;
  test_definition_t this_test2;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.zone = TARGET_ZONE_PLAYERS;
  if (event == EVENT_RESOLVE_SPELL)
  {

    default_test_definition(&this_test1, 2);
    this_test1.state = 4;
    instance = get_card_instance(player, card);
    result = new_manipulate_all(player, card, instance->targets[0].player, &this_test1,
                                ACT_KILL_REMOVE);
    if (0 < result)
    {
      default_test_definition(&this_test2, 1);
      this_test2.qty = (unsigned short)result;
      this_test2.subtype = 0x2000;
      instance = get_card_instance(player, card);
      result = instance->targets[0].player;
      instance = get_card_instance(player, card);
      new_global_tutor(instance->targets[0].player, result, 1, 5, 0, 5, &this_test2);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}

/* Sheltering Light	|W
 * Instant
 * Target creature gains indestructible until end of turn. Scry 1. */

/* Shining Aerosaur	|4|W	0x401000
 * Creature - Dinosaur 3/4
 * Flying */

/* Skyblade of the Legion	|1|W	0x401000
 * Creature - Vampire Soldier 1/3
 * Flying */

/* Slash of Talons	|W	=>avacyn_restored.c:card_righteous_blow 0x2005cff
 * Instant
 * ~ deals 2 damage to target attacking or blocking creature. */

/* Steadfast Armasaur	|3|W
 * Creature - Dinosaur 2/3
 * Vigilance
 * |1|W, |T: ~ deals damage equal to its toughness to target creature blocking or blocked by it. */

/* Sunrise Seeker	|4|W
 * Creature - Human Scout 3/3
 * Vigilance
 * When ~ enters the battlefield, it explores. */

/* Territorial Hammerskull	|2|W
 * Creature - Dinosaur 2/3
 * Whenever ~ attacks, tap target creature an opponent controls. */

/* Tocatli Honor Guard	|1|W	=>0x401000; =CARD_ID_TORPOR_ORB
 * Creature - Human Soldier 1/3
 * Creatures entering the battlefield don't cause abilities to trigger. */

/* Vampire's Zeal	|W
 * Instant
 * Target creature gets +2/+2 until end of turn. If it's a Vampire, it gains first strike until end of turn. */

/* Wakening Sun's Avatar	|5|W|W|W
 * Creature - Dinosaur Avatar 7/7
 * When ~ enters the battlefield, if you cast it from your hand, destroy all non-Dinosaur creatures. */

/* ManalinkEh 2022: 0x22135a0; original cards/ixalan.c:409. */
int card_arcane_adaptation(int player, int card, event_t event)
{
  /* Arcane Adaptation	|2|U
   * Enchantment
   * As ~ enters the battlefield, choose a creature type.
   * Creatures you control are the chosen type in addition to their other types. The same is true for creature spells you control and creature cards you own that aren't on the battlefield. */

  uint32_t data;
  card_instance_t *instance;
  inst_info_t stored_subtype;

  if (event == EVENT_RESOLVE_SPELL)
  {
    data = select_a_subtype(player, card);
    instance = get_card_instance(player, card);
    set_inst_info(instance, 0x43bf, data, 0);
  }
  else if (event == EVENT_CHANGE_TYPE)
  {
    instance = in_play(player, card);
    if ((instance != NULL) && (affected_card_controller == player))
    {
      instance = in_play(player, affected_card);
      if (instance != NULL)
      {
        if (is_what(affected_card_controller, affected_card, 2) != 0)
        {
          if (is_humiliated(player, card, AL_STATIC) == 0)
          {

            instance = get_card_instance(player, card);
            stored_subtype = inst_info(instance, 0x43bf);
            add_a_subtype_not_in_play(player, EVENT_CHANGE_TYPE, stored_subtype.data);
            stored_subtype = inst_info(instance, 0x43bf);
            if (has_subtype(affected_card_controller, affected_card, stored_subtype.data) == 0)
            {
              stored_subtype = inst_info(instance, 0x43bf);
              add_a_subtype(affected_card_controller, affected_card, stored_subtype.data);
            }
          }
        }
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2213730; original cards/ixalan.c:428. */
int card_chart_a_course(int player, int card, event_t event)
{
  /* Chart a Course	|1|U
   * Sorcery
   * Draw two cards. Then discard a card unless you attacked with a creature this turn. */

  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      ai_modifier = ai_modifier + (unsigned int)((current_turn != player) ? 0 : -48) + 0x18;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    draw_cards(player, 2);
    if (get_trap_condition(player, 0x606) < 1)
    {
      discard(player, 0, player);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* Daring Saboteur	|1|U
 * Creature - Human Pirate 2/1
 * |2|U: ~ can't be blocked this turn.
 * Whenever ~ deals combat damage to a player, you may draw a card. If you do, discard a card. */

/* Deadeye Quartermaster	|3|U
 * Creature - Human Pirate 2/2
 * When ~ enters the battlefield, you may search your library for an Equipment or Vehicle card, reveal it, put it into your hand, then shuffle your library. */

/* Deeproot Waters	|2|U
 * Enchantment
 * Whenever you cast a Merfolk spell, create a 1/1 |Sblue Merfolk creature token with hexproof. */

/* Depths of Desire	|2|U
 * Instant
 * Return target creature to its owner's hand. Create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Dive Down	|U	=>dragons_of_tarkir.c:card_glint 0x200da27
 * Instant
 * Target creature you control gets +0/+3 and gains hexproof until end of turn. */

/* Dreamcaller Siren	|2|U|U
 * Creature - Siren Pirate 3/3
 * Flash
 * Flying
 * ~ can block only creatures with flying.
 * When ~ enters the battlefield, if you control another Pirate, tap up to two target nonland permanents. */

/* ManalinkEh 2022: 0x2213800; original cards/ixalan.c:475. */
int card_entrancing_melody(int player, int card, event_t event)
{
  /* Entrancing Melody	|X|U|U
   * Sorcery
   * Gain control of target creature with converted mana cost X. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      gain_control_permanently(player, instance->targets[0].player,
                               instance->targets[0].card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 5, &td, "TARGET_CREATURE", 1, NULL);
}

/* Fleet Swallower	|5|U|U
 * Creature - Fish 6/6
 * Whenever ~ attacks, target player puts the top half of their library, rounded up, into their graveyard. */

/* Headwater Sentries	|3|U	0x401000
 * Creature - Merfolk Warrior 2/5 */

/* ManalinkEh 2022: 0x2213940; original cards/ixalan.c:506. */
int card_herald_of_secret_streams(int player, int card, event_t event)
{
  /* Herald of Secret Streams	|3|U
   * Creature - Merfolk Warrior 2/3
   * Creatures you control with +1/+1 counters on them can't be blocked. */

  card_instance_t *instance;
  int result;

  if (((event == EVENT_ABILITIES) && (affected_card_controller == player)) &&
      (card != affected_card))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        instance = in_play(affected_card_controller, affected_card);
        if (instance != NULL)
        {
          result = is_what(affected_card_controller, affected_card, 2);
          if (result != 0)
          {
            result = count_counters(affected_card_controller, affected_card, COUNTER_P1_P1);
            if (0 < result)
            {
              unblockable(affected_card_controller, affected_card, EVENT_ABILITIES);
            }
          }
        }
      }
    }
  }
  return 0;
}

/* Jace, Cunning Castaway	|1|U|U
 * Legendary Planeswalker - Jace (3)
 * +1: Whenever one or more creatures you control deal combat damage to a player this turn, draw a card, then discard a card.
 * -2: Create a 2/2 |Sblue Illusion creature token with "When this creature becomes the target of a spell, sacrifice it."
 * -5: Create two tokens that are copies of ~, except they're not legendary. */

/* Kopala, Warden of Waves	|1|U|U
 * Legendary Creature - Merfolk Wizard 2/2
 * Spells your opponents cast that target a Merfolk you control cost |2 more to cast.
 * Abilities your opponents activate that target a Merfolk you control cost |2 more to activate. */

/* Lookout's Dispersal	|2|U
 * Instant
 * This spell costs |1 less to cast if you control a Pirate.
 * Counter target spell unless its controller pays |4. */

/* Navigator's Ruin	|2|U
 * Enchantment
 * Raid - At the beginning of your end step, if you attacked with a creature this turn, target opponent puts the top four cards of their library into their graveyard. */

/* One With the Wind	|1|U	=>innistrad.c:card_spectral_flight 0x2004a4e
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+2 and has flying. */

/* Overflowing Insight	|4|U|U|U
 * Sorcery
 * Target player draws seven cards. */

/* Perilous Voyage	|1|U
 * Instant
 * Return target nonland permanent you don't control to its owner's hand. If its converted mana cost was 2 or less, scry 2. */

/* Pirate's Prize	|3|U
 * Sorcery
 * Draw two cards. Create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* ManalinkEh 2022: 0x2213a30; original cards/ixalan.c:563. */
int card_prosperous_pirates(int player, int card, event_t event)
{
  /* Prosperous Pirates	|4|U
   * Creature - Human Pirate 3/4
   * When ~ enters the battlefield, create two colorless Treasure artifact tokens with "|T, Sacrifice this artifact: Add one mana of any color." */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  generate_tokens_by_id(player, card, CARD_ID_TREASURE, 2);
  return 0;
}

/* River Sneak	|1|U
 * Creature - Merfolk Warrior 1/1
 * ~ can't be blocked.
 * Whenever another Merfolk enters the battlefield under your control, ~ gets +1/+1 until end of turn. */

/* ManalinkEh 2022: 0x2213aa0; original cards/ixalan.c:581. */
int card_rivers_rebuke(int player, int card, event_t event)
{
  /* River's Rebuke	|4|U|U
   * Sorcery
   * Return all nonland permanents target player controls to their owner's hand. */

  card_instance_t *instance;
  target_definition_t td;
  test_definition_t this_test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.zone = TARGET_ZONE_PLAYERS;
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&this_test, 4);
    new_manipulate_all(player, card, instance->targets[0].player, &this_test, ACT_BOUNCE);
    default_test_definition(&this_test, 1);
    this_test.type_flag = 1;
    new_manipulate_all(player, card, instance->targets[0].player, &this_test, ACT_BOUNCE);
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}

/* Run Aground	|3|U
 * Instant
 * Put target artifact or creature on top of its owner's library. */

/* ManalinkEh 2022: 0x2213c70; original cards/ixalan.c:615. */
int card_sailor_of_means(int player, int card, event_t event)
{
  /* Sailor of Means	|2|U
   * Creature - Human Pirate 1/4
   * When ~ enters the battlefield, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_TREASURE);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2213cd0; original cards/ixalan.c:628. */
int card_search_for_azcanta(int player, int card, event_t event)
{
  /* Search for Azcanta	|1|U
   * Legendary Enchantment
   * At the beginning of your upkeep, look at the top card of your library. You may put it into your graveyard. Then if you have seven or more cards in your graveyard, you may transform ~. */

  int result;
  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    if (*deck_ptr[player] != -1)
    {
      result =
          show_deck(player, deck_ptr[player], 1, "Top card of library", 0, "Into grave?");
      if (result == -1)
      {
        mill(player, 1);
      }
    }
    result = count_graveyard(player);
    if (6 < result)
    {
      transform(player, card);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}
#endif

/* ManalinkEh 2022: 0x2213db0; original cards/ixalan.c:650. */
int card_azcanta_the_sunken_ruin(int player, int card, event_t event)
{
  /* Azcanta, the Sunken Ruin	""
   * Legendary Land
   * |T: Add |U.
   * |2|U, |T: Look at the top four cards of your library. You may reveal a noncreature, nonland card from among them and put it into your hand. Put the rest on the bottom of your library in any order. */

  int choice_count_or_result;
  card_instance_t *instance;
  int tutored_iid;
  test_definition_t test;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  if (event == EVENT_CAN_ACTIVATE)
  {
    return mana_producer(player, card, event);
  }
  if (event == EVENT_ACTIVATE)
  {

    instance = get_card_instance(player, card);
    instance->info_slot = 0;
    choice_count_or_result = paying_mana();
    if (((choice_count_or_result != 0) ||
         (choice_count_or_result = has_mana_for_activated_ability(player, card, 2, 0, 2, 0, 0, 0),
          choice_count_or_result == 0)) ||
        (choice_count_or_result =
             do_dialog(player, player, card, -1, -1, " Add mana\n Look at top four\n Cancel", 1),
         choice_count_or_result == 0))
    {
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
    if (choice_count_or_result == 1)
    {
      instance = get_card_instance(player, card);
      instance->state = instance->state | 0x10;
      if (charge_mana_for_activated_ability(player, card, 2, 0, 1, 0, 0, 0) == 0)
      {
        spell_fizzled = 1;
        untap_card_no_event(player, card);
      }
      else
      {
        instance = get_card_instance(player, card);
        instance->info_slot = 1;
      }
      tapped_for_mana_color = -1;
    }
    else
    {
      spell_fizzled = 1;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);

    if ((instance->info_slot == 1) &&
        (choice_count_or_result = count_deck(player), 0 < choice_count_or_result))
    {
      if (4 < choice_count_or_result)
      {
        choice_count_or_result = 4;
      }

      new_default_test_definition(&test, 3, "Select a nonland, noncreature card.");
      test.type_flag = 1;
      test.create_minideck = (unsigned short)choice_count_or_result;
      test.no_shuffle = 1;
      tutored_iid = new_global_tutor(player, player, 1, 1, 0, 1, &test);
      if ((tutored_iid < 0) ||
          (choice_count_or_result = choice_count_or_result + -1, 0 < choice_count_or_result))
      {
        put_top_x_on_bottom(player, player, choice_count_or_result);
      }
    }
  }
  else if (event != EVENT_CLEANUP)
    return mana_producer(player, card, event);
  return 0;
}

/* Shaper Apprentice	|1|U
 * Creature - Merfolk Wizard 2/1
 * ~ has flying as long as you control another Merfolk. */

/* Shipwreck Looter	|1|U
 * Creature - Human Pirate 2/1
 * Raid - When ~ enters the battlefield, if you attacked with a creature this turn, you may draw a card. If you do, discard a card. */

/* Shore Keeper	|U
 * Creature - Trilobite 0/3
 * |7|U, |T, Sacrifice ~: Draw three cards. */

/* Siren Lookout	|2|U
 * Creature - Siren Pirate 1/2
 * Flying
 * When ~ enters the battlefield, it explores. */

/* ManalinkEh 2022: 0x2214060; original cards/ixalan.c:733. */
int card_siren_stormtamer(int player, int card, event_t event)
{
  /* Siren Stormtamer	|U
   * Creature - Siren Pirate Wizard 1/1
   * Flying
   * |U, Sacrifice ~: Counter target spell or ability that targets you or a creature you control. */

  int i;
  int card_00;
  int activation_available;
  card_instance_t *instance;
  int target_player_or_result;
  if (event == EVENT_CAN_ACTIVATE)
  {
    activation_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x408, 0, 0,
                                                     1, 0, 0, 0, 0, NULL, NULL);
    if ((activation_available != 0) &&
        (instance = get_card_instance(card_on_stack_controller, card_on_stack),
         instance->number_of_targets != '\0'))
    {
      i = 0;
      do
      {
        target_player_or_result = instance->targets[i].player;
        card_00 = instance->targets[i].card;
        if (target_player_or_result == player && card_00 == -1)
          return 99;
        if (card_00 != -1 && is_what(target_player_or_result, card_00, 2) &&
            instance->targets[i].player == player)
          return 99;
        i = i + 1;
      } while (i < (int)(unsigned int)instance->number_of_targets);
    }
    return 0;
  }
  if (event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    if (counterspell_validate(player, card, NULL, 0) != 0)
    {
      real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
    }
    kill_card(player, card, KILL_SACRIFICE);
  }
  return generic_activated_ability(player, card, event, 0x408, 0, 0, 1, 0, 0, 0, 0, NULL, NULL);
}

/* Siren's Ruse	|1|U
 * Instant
 * Exile target creature you control, then return that card to the battlefield under its owner's control. If a Pirate was exiled this way, draw a card. */

/* ManalinkEh 2022: 0x2214270; original cards/ixalan.c:774. */
int card_spell_swindle(int player, int card, event_t event)
{
  /* Spell Swindle	|3|U|U
   * Instant
   * Counter target spell. Create X colorless Treasure artifact tokens, where X is that spell's converted mana cost. They have "|T, Sacrifice this artifact: Add one mana of any color." */

  if (event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)))
    return counterspell(player, card, event, NULL, 0);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (counterspell_validate(player, card, NULL, 0))
    {
      card_instance_t *instance = get_card_instance(player, card);
      int mana_value = get_cmc(instance->targets[0].player, instance->targets[0].card);
      real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_TREASURE, &token);
      token.qty = (unsigned char)mana_value;
      generate_token(&token);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return 0;
}

/* Storm Fleet Aerialist	|1|U
 * Creature - Human Pirate 1/2
 * Flying
 * Raid - ~ enters the battlefield with a +1/+1 counter on it if you attacked with a creature this turn. */

/* Storm Fleet Spy	|2|U
 * Creature - Human Pirate 2/2
 * Raid - When ~ enters the battlefield, if you attacked with a creature this turn, draw a card. */

/* Storm Sculptor	|3|U
 * Creature - Merfolk Wizard 3/2
 * ~ can't be blocked.
 * When ~ enters the battlefield, return a creature you control to its owner's hand. */

/* Tempest Caller	|2|U|U
 * Creature - Merfolk Wizard 2/3
 * When ~ enters the battlefield, tap all creatures target opponent controls. */

/* Watertrap Weaver	|2|U	=>m15.c:card_frost_lynx 0x200c8c0
 * Creature - Merfolk Wizard 2/2
 * When ~ enters the battlefield, tap target creature an opponent controls. That creature doesn't untap during its controller's next untap step. */

/* Wind Strider	|4|U	=>time_spiral.c:card_ashcoat_bear 0x200cb63
 * Creature - Merfolk Wizard 3/3
 * Flash
 * Flying */

/* Anointed Deacon	|4|B
 * Creature - Vampire Cleric 3/3
 * At the beginning of combat on your turn, you may have target Vampire get +2/+0 until end of turn. */

/* ManalinkEh 2022: 0x22143b0; original cards/ixalan.c:831. */
int card_arguels_blood_fast(int player, int card, event_t event)
{
  /* Arguel's Blood Fast	|1|B
   * Legendary Enchantment
   * |1|B, Pay 2 life: Draw a card.
   * At the beginning of your upkeep, if you have 5 or less life, you may transform ~. */

  int life_or_trigger_mode;
  int land_count;
  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  life_or_trigger_mode = duh_mode(player);
  upkeep_trigger_ability_mode(player, card, event, player,
                              (life_or_trigger_mode != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    if (life[player] < 6)
      transform(player, card);
    return 0;
  }
  if (event == EVENT_ACTIVATE)
  {
    life_or_trigger_mode = life[player];
    land_count = count_subtype(player, 1, -1);
    ai_modifier += life_or_trigger_mode - 0xe + land_count * 2;
    return generic_activated_ability(player, card, event, 0, 1, 1, 0, 0, 0, 0, 2, NULL, NULL);
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_CLEANUP)
    return generic_activated_ability(player, card, event, 0, 1, 1, 0, 0, 0, 0, 2, NULL, NULL);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_cards(player, 1);
    return generic_activated_ability(player, card, event, 0, 1, 1, 0, 0, 0, 0, 2, NULL, NULL);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2214540; original cards/ixalan.c:857. */
int card_temple_of_aclazotz(int player, int card, event_t event)
{
  /* Temple of Aclazotz	""
   * Legendary Land
   * |T: Add |B.
   * |T, Sacrifice a creature: You gain life equal to the sacrificed creature's toughness. */

  card_instance_t *instance;
  unsigned int available_or_chosen_sacrifice;
  int result;
  unsigned int player_00;
  test_definition_t test;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  if (event == EVENT_CAN_ACTIVATE)
  {

    available_or_chosen_sacrifice = mana_producer(player, card, EVENT_CAN_ACTIVATE);
    if ((((available_or_chosen_sacrifice == 0) &&
          (result = is_tapped(player, card), result == 0)) &&
         (result = is_animated_and_sick(player, card), result == 0)) &&
        (result = can_use_activated_abilities(player, card), result != 0))
    {
      result = can_sacrifice_as_cost(player, 1, 2, 0, 0, 0, 0, 0, 0, 0, -1, 0);
      available_or_chosen_sacrifice = (unsigned int)(result != 0);
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->info_slot = 0;
      result = paying_mana();
      if ((((result == 0) && (result = is_tapped(player, card), result == 0)) &&
           ((result = is_animated_and_sick(player, card),
             result == 0 && ((result = can_use_activated_abilities(player, card),
                              result != 0 && (result = can_sacrifice_as_cost(player, 1, 2, 0, 0, 0,
                                                                             0, 0, 0, 0, -1, 0),
                                              result != 0)))))) &&
          (result =
               do_dialog(player, player, card, -1, -1, " Add mana\n Sac and gain life\n Cancel", 1),
           result != 0))
      {
        if (result != 1)
        {
          untap_card_no_event(player, card);
          spell_fizzled = 1;
          return 0;
        }
        add_state(player, card, 0x10);
        result = charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0);
        if (result == 0)
        {
          untap_card_no_event(player, card);
          spell_fizzled = 1;
        }
        else
        {

          new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
          available_or_chosen_sacrifice = new_sacrifice(
              player, card, player, SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
          if (available_or_chosen_sacrifice == 0)
          {
            untap_card_no_event(player, card);
            spell_fizzled = 1;
            return 0;
          }
          player_00 = (int)available_or_chosen_sacrifice >> 0x10 & 0xff;
          result = get_toughness(player_00, available_or_chosen_sacrifice >> 0x18);
          instance->info_slot = result;
          kill_card(player_00, available_or_chosen_sacrifice >> 0x18, KILL_SACRIFICE);
        }
        tapped_for_mana_color = -1;
        return 0;
      }
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) && (0 < instance->info_slot))
    {
      gain_life(player, instance->info_slot);
    }
    available_or_chosen_sacrifice = mana_producer(player, card, event);
  }
  return available_or_chosen_sacrifice;
}

/* Bishop of the Bloodstained	|3|B|B
 * Creature - Vampire Cleric 3/3
 * When ~ enters the battlefield, target opponent loses 1 life for each Vampire you control. */

/* Blight Keeper	|B
 * Creature - Bat Imp 1/1
 * Flying
 * |7|B, |T, Sacrifice ~: Target opponent loses 4 life and you gain 4 life. */

/* Bloodcrazed Paladin	|1|B
 * Creature - Vampire Knight 1/1
 * Flash
 * ~ enters the battlefield with a +1/+1 counter on it for each creature that died this turn. */

/* Boneyard Parley	|5|B|B
 * Sorcery
 * Exile up to five target creature cards from graveyards. An opponent separates those cards into two piles. Put all cards from the pile of your choice onto the battlefield under your control and the rest into their owners' graveyards. */

/* Contract Killing	|3|B|B
 * Sorcery
 * Destroy target creature. Create two colorless Treasure artifact tokens with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Costly Plunder	|1|B
 * Instant
 * As an additional cost to cast this spell, sacrifice an artifact or creature.
 * Draw two cards. */

/* Dark Nourishment	|4|B	=>darksteel.c:card_essence_drain 0x2006768
 * Instant
 * ~ deals 3 damage to any target. You gain 3 life. */

/* Deadeye Tormentor	|2|B
 * Creature - Human Pirate 2/2
 * Raid - When ~ enters the battlefield, if you attacked with a creature this turn, target opponent discards a card. */

/* Deadeye Tracker	|B
 * Creature - Human Pirate 1/1
 * |1|B, |T: Exile two target cards from an opponent's graveyard. ~ explores. */

/* Deathless Ancient	|4|B|B
 * Creature - Vampire Knight 4/4
 * Flying
 * Tap three untapped Vampires you control: Return ~ from your graveyard to your hand. */

/* Desperate Castaways	|1|B
 * Creature - Human Pirate 2/3
 * ~ can't attack unless you control an artifact. */

/* Dire Fleet Hoarder	|1|B
 * Creature - Human Pirate 2/1
 * When ~ dies, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Dire Fleet Interloper	|3|B
 * Creature - Human Pirate 2/2
 * Menace
 * When ~ enters the battlefield, it explores. */

/* Dire Fleet Ravager	|3|B|B
 * Creature - Orc Pirate Wizard 4/4
 * Menace, deathtouch
 * When ~ enters the battlefield, each player loses a third of their life, rounded up. */

/* Fathom Fleet Captain	|1|B
 * Creature - Human Pirate 2/1
 * Menace
 * Whenever ~ attacks, if you control another nontoken Pirate, you may pay |2. If you do, create a 2/2 |Sblack Pirate creature token with menace. */

/* Fathom Fleet Cutthroat	|3|B
 * Creature - Human Pirate 3/3
 * When ~ enters the battlefield, destroy target creature an opponent controls that was dealt damage this turn. */

/* Grim Captain's Call	|2|B
 * Sorcery
 * Return a Pirate card from your graveyard to your hand, then do the same for Vampire, Dinosaur, and Merfolk. */

/* Heartless Pillage	|2|B
 * Sorcery
 * Target opponent discards two cards.
 * Raid - If you attacked with a creature this turn, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* ManalinkEh 2022: 0x2214900; original cards/ixalan.c:1006. */
int card_kitesail_freebooter(int player, int card, event_t event)
{
  /* Kitesail Freebooter	|1|B
   * Creature - Human Pirate 1/2
   * Flying
   * When ~ enters the battlefield, target opponent reveals their hand. You choose a noncreature, nonland card from it. Exile that card until ~ leaves the battlefield. */

  card_instance_t *instance;
  int result;
  int id;
  test_definition_t this_test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  instance = get_card_instance(player, card);
  if (event == EVENT_ETB)
  {
    result = target_opponent(player, card);
    if ((result != 0) && (hand_count[1 - player] != 0))
    {

      new_default_test_definition(&this_test, 3, "Select a nonland, noncreature card.");
      this_test.type_flag = 1;
      this_test.zone = 0x100;
      result = check_battlefield_for_special_card(player, card, instance->targets[0].player, 0,
                                                  &this_test);
      result = new_select_a_card(player, instance->targets[0].player, 4, result, 1, -1, &this_test);
      if (-1 < result)
      {
        id = get_id(instance->targets[0].player, result);
        create_card_name_legacy(player, card, id);
        exile_card_and_remember_it_on_exiledby(player, card, instance->targets[0].player, result);
      }
    }
    leaves_play(player, card, EVENT_ETB, 1, 0);
    return 0;
  }
  leaves_play(player, card, event, 1, 0);
  if (event != EVENT_LEAVES_PLAY_ABILITY)
  {
    return 0;
  }
  return_cards_exiled_by_me_to_hand(player, card);
  return 0;
}

/* Lurking Chupacabra	|3|B
 * Creature - Beast Horror 2/3
 * Whenever a creature you control explores, target creature an opponent controls gets -2/-2 until end of turn. */

/* March of the Drowned	|B
 * Sorcery
 * Choose one -
 * * Return target creature card from your graveyard to your hand.
 * * Return two target Pirate cards from your graveyard to your hand. */

/* Queen's Agent	|5|B
 * Creature - Vampire Scout 3/3
 * Lifelink
 * When ~ enters the battlefield, it explores. */

/* ManalinkEh 2022: 0x2212420; original cards/ixalan.c:1062. */
static int revel_in_riches_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  unsigned int pending_deaths;
  card_instance_t *dying;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (get_id((int)instance->damage_target_player, instance->damage_target_card) != 0x445c))
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
        count_for_gfp_ability(player, card, 0x77, 1 - player, 2, NULL);
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
        draw_a_card(player);
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

/* Queen's Bay Soldier	|1|B	0x401000
 * Creature - Vampire Soldier 2/2 */

/* Raiders' Wake	|3|B
 * Enchantment
 * Whenever an opponent discards a card, that player loses 2 life.
 * Raid - At the beginning of your end step, if you attacked with a creature this turn, target opponent discards a card. */

/* ManalinkEh 2022: 0x2214ad0; original cards/ixalan.c:1117. */
int card_revel_in_riches(int player, int card, event_t event)
{
  /* Revel in Riches	|4|B
   * Enchantment
   * Whenever a creature an opponent controls dies, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color."
   * At the beginning of your upkeep, if you control ten or more Treasures, you win the game. */

  card_instance_t *instance;
  int scan_or_result;
  unsigned int pending_deaths;

  if (((event == EVENT_STATIC_EFFECTS) && (card == affected_card)) &&
      (player == affected_card_controller))
  {

    bool legacy_found = false;
    if (0 < active_cards_count[player])
    {
      scan_or_result = 0;
      do
      {
        instance = in_play(player, scan_or_result);

        if ((((instance != NULL) && (is_what(player, scan_or_result, 0x80) != 0)) &&
             (instance = get_card_instance(player, scan_or_result),
              instance->info_slot_fn == revel_in_riches_effect)) &&
            (((instance->damage_target_player == player &&
               (instance->damage_target_card == card)) ||
              ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
        {
          legacy_found = true;
          break;
        }
        scan_or_result = scan_or_result + 1;
      } while (scan_or_result < active_cards_count[player]);
    }

    if (!legacy_found)
    {
      scan_or_result =
          create_targetted_legacy_effect(player, card, revel_in_riches_effect, player, card);
      instance = get_card_instance(player, scan_or_result);
      instance->targets[0].player = player;
      instance->targets[0].card = card;
      instance->number_of_targets = '\x01';
      add_status(player, scan_or_result, 0x10000);
    }
  }
  count_for_gfp_ability(player, card, event, 1 - player, 2, NULL);
  instance = get_card_instance(player, card);
  if (instance->kill_code == '\0')
  {

    pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    generate_tokens_by_id(player, card, CARD_ID_TREASURE, pending_deaths & 0xff);
  }
  if ((((current_turn == player) &&
        (scan_or_result = upkeep_trigger(player, card, event), scan_or_result != 0)) &&
       (scan_or_result = is_humiliated(player, card, AL_TRIG), scan_or_result == 0)) &&
      (scan_or_result = count_subtype(player, -1, 0x3006), 9 < scan_or_result))
  {
    lose_the_game(1 - player);
  }
  return global_enchantment(player, card, event);
}

/* Ruin Raider	|2|B
 * Creature - Orc Pirate 3/2
 * Raid - At the beginning of your end step, if you attacked with a creature this turn, reveal the top card of your library and put that card into your hand. You lose life equal to the card's converted mana cost. */

/* Ruthless Knave	|2|B
 * Creature - Orc Pirate 3/2
 * |2|B, Sacrifice a creature: Create two colorless Treasure artifact tokens with "|T, Sacrifice this artifact: Add one mana of any color."
 * Sacrifice three Treasures: Draw a card. */

/* Sanctum Seeker	|2|B|B
 * Creature - Vampire Knight 3/4
 * Whenever a Vampire you control attacks, each opponent loses 1 life and you gain 1 life. */

/* Seekers' Squire	|1|B
 * Creature - Human Scout 1/2
 * When ~ enters the battlefield, it explores. */

/* Skittering Heartstopper	|B	=>morningtide.c:card_moonglove_changeling 0x2007b63
 * Creature - Insect 1/2
 * |B: ~ gains deathtouch until end of turn. */

/* Skulduggery	|B
 * Instant
 * Until end of turn, target creature you control gets +1/+1 and target creature an opponent controls gets -1/-1. */

/* ManalinkEh 2022: 0x2214d00; original cards/ixalan.c:1188. */
int card_skymarch_bloodletter(int player, int card, event_t event)
{
  /* Skymarch Bloodletter	|2|B
   * Creature - Vampire Soldier 2/2
   * Flying
   * When ~ enters the battlefield, target opponent loses 1 life and you gain 1 life. */

  int result;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    result = would_validate_arbitrary_target(&td, 1 - player, -1);
    if (result != 0)
    {
      lose_life(1 - player, 1);
      gain_life(player, 1);
    }
  }
  return 0;
}

/* Spreading Rot	|4|B
 * Sorcery
 * Destroy target land. Its controller loses 2 life. */

/* Sword-Point Diplomacy	|2|B
 * Sorcery
 * Reveal the top three cards of your library. For each of those cards, put that card into your hand unless any opponent pays 3 life. Then exile the rest. */

/* Vanquish the Weak	|2|B	=>origins.c:card_reave_soul 0x200dd01
 * Instant
 * Destroy target creature with power 3 or less. */

/* Vicious Conquistador	|B	=>worldwake.c:card_pulse_tracker 0x200292e
 * Creature - Vampire Soldier 1/2
 * Whenever ~ attacks, each opponent loses 1 life. */

/* ManalinkEh 2022: 0x2214db0; original cards/ixalan.c:1225. */
int card_vraskas_contempt(int player, int card, event_t event)
{
  /* Vraska's Contempt	|2|B|B
   * Instant
   * Exile target creature or planeswalker. You gain 2 life. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x102);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (validate_target(player, card, &td, 0))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      gain_life(player, 2);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target creature or planeswalker.",
                         1, NULL);
}

/* ManalinkEh 2022: 0x2214f20; original cards/ixalan.c:1249. */
int card_walk_the_plank(int player, int card, event_t event)
{
  /* Walk the Plank	|B|B
   * Sorcery
   * Destroy target non-Merfolk creature. */

  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.required_subtype = 0x74;
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
  return generic_spell(player, card, event, 0x81, &td, "Select target non-Merfolk creature.", 1, NULL);
}

/* Wanted Scoundrels	|1|B
 * Creature - Human Pirate 4/3
 * When ~ dies, target opponent creates two colorless Treasure artifact tokens with "|T, Sacrifice this artifact: Add one mana of any color." */

/* ManalinkEh 2022: 0x22150a0; original cards/ixalan.c:1276. */
int card_angraths_marauders(int player, int card, event_t event)
{
  /* Angrath's Marauders	|5|R|R
   * Creature - Human Pirate 4/4
   * If a source you control would deal damage to a permanent or player, it deals double that damage to that permanent or player instead. */

  card_instance_t *instance;

  instance = damage_being_dealt(event);
  if ((instance != NULL) && (instance->damage_source_player == player))
  {
    instance->info_slot = instance->info_slot * 2;
    return 0;
  }
  return 0;
}

/* Bonded Horncrest	|3|R
 * Creature - Dinosaur 5/5
 * ~ can't attack or block alone. */

/* Brazen Buccaneers	|3|R
 * Creature - Human Pirate 2/2
 * Haste
 * When ~ enters the battlefield, it explores. */

/* Burning Sun's Avatar	|3|R|R|R
 * Creature - Dinosaur Avatar 6/6
 * When ~ enters the battlefield, it deals 3 damage to target opponent or planeswalker and 3 damage to up to one target creature. */

/* ManalinkEh 2022: 0x22150d0; original cards/ixalan.c:1302. */
int card_captain_lannery_storm(int player, int card, event_t event)
{
  /* Captain Lannery Storm	|2|R
   * Legendary Creature - Human Pirate 2/2
   * Haste
   * Whenever ~ attacks, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color."
   * Whenever you sacrifice a Treasure, ~ gets +1/+0 until end of turn. */

  int result;
  card_instance_t *instance;

  check_legend_rule(player, card, event);
  haste(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    generate_token_by_id(player, card, CARD_ID_TREASURE);
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = in_play(affected_card_controller, affected_card);
    if (instance != NULL)
    {
      instance = get_card_instance(affected_card_controller, affected_card);
      if (instance->kill_code == '\x03')
      {
        result = get_id(affected_card_controller, affected_card);
        if (result == 0x4998)
        {
          result = check_special_flags(affected_card_controller, affected_card, 0x800);
          if (result == 0)
          {
            count_for_gfp_ability(player, card, 0x77, player, 0x47, NULL);
          }
        }
      }
    }
  }
  result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  pump_until_eot_merge_previous(player, card, player, card, result, 0);
  return 0;
}

/* Captivating Crew	|3|R
 * Creature - Human Pirate 4/3
 * |3|R: Gain control of target creature an opponent controls until end of turn. Untap that creature. It gains haste until end of turn. Activate this ability only any time you could cast a sorcery. */

/* Charging Monstrosaur	|4|R	=>homelands.c:card_ambush_party	0x200bb46
 * Creature - Dinosaur 5/5
 * Trample, haste */

/* Dinosaur Stampede	|2|R
 * Instant
 * Attacking creatures get +2/+0 until end of turn. Dinosaurs you control gain trample until end of turn. */

/* Fathom Fleet Firebrand	|1|R	=> card_wildfire_emissary 200bb0a
 * Creature - Human Pirate 2/2
 * |1|R: ~ gets +1/+0 until end of turn. */

/* ManalinkEh 2022: 0x2215260; original cards/ixalan.c:1345. */
int card_fiery_cannonade(int player, int card, event_t event)
{
  /* Fiery Cannonade	|2|R
   * Instant
   * ~ deals 2 damage to each non-Pirate creature. */

  test_definition_t this_test;
  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&this_test, 2);
    this_test.subtype = 0x95;
    this_test.subtype_flag = 1;
    new_damage_all(player, card, 2, 2, 0, &this_test);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* Fire Shrine Keeper	|R
 * Creature - Elemental 1/1
 * Menace
 * |7|R, |T, Sacrifice ~: It deals 3 damage to each of up to two target creatures. */

/* Firecannon Blast	|1|R|R
 * Sorcery
 * ~ deals 3 damage to target creature.
 * Raid - ~ deals 6 damage to that creature instead if you attacked with a creature this turn. */

/* Frenzied Raptor	|2|R
 * Creature - Dinosaur 4/2 */

/* Headstrong Brute	|2|R
 * Creature - Orc Pirate 3/3
 * ~ can't block.
 * ~ has menace as long as you control another Pirate. */

/* Lightning-Rig Crew	|2|R
 * Creature - Goblin Pirate 0/5
 * |T: ~ deals 1 damage to each opponent.
 * Whenever you cast a Pirate spell, untap ~. */

/* ManalinkEh 2022: 0x2215350; original cards/ixalan.c:1390. */
int card_makeshift_munitions(int player, int card, event_t event)
{
  /* Makeshift Munitions	|1|R
   * Enchantment
   * |1, Sacrifice an artifact or creature: ~ deals 1 damage to any target. */

  int result;
  target_definition_t td;

  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    damage_target0(player, card, 1);
  }
  result = generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_CAN_TARGET, 1, 0, 0, 0, 0, 0, 0, &td, "TARGET_ANY",
      TYPE_ARTIFACT | TYPE_CREATURE, NULL, SARV_0);
  return result;
}

/* Nest Robber	|1|R	=>homelands.c:card_ambush_party	0x200bb46
 * Creature - Dinosaur 2/1
 * Haste */

/* Otepec Huntmaster	|1|R
 * Creature - Human Shaman 1/2
 * Dinosaur spells you cast cost |1 less to cast.
 * |T: Target Dinosaur gains haste until end of turn. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2215490; original cards/ixalan.c:1420. */
int card_rampaging_ferocidon(int player, int card, event_t event)
{
  /* Rampaging Ferocidon	|2|R
   * Creature - Dinosaur 3/3
   * Menace
   * Players can't gain life.
   * Whenever another creature enters the battlefield, ~ deals 1 damage to that creature's controller. */

  card_instance_t *instance;
  test_definition_t test;
  menace(player, card, event);
  instance = in_play(player, card);
  if (event == EVENT_STATIC_EFFECTS && instance)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      player_bits[0] |= PB_CANT_GAIN_LIFE;
      player_bits[1] |= PB_CANT_GAIN_LIFE;
    }
    if (trigger_condition != TRIGGER_COMES_INTO_PLAY)
      return 0;
  }
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && card == affected_card &&
      affected_card_controller == player)
  {
    default_test_definition(&test, 2);
    test.not_me = 1;
    etb_ability_global(2, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event != EVENT_ETB)
  {
    return 0;
  }
  damage_player(trigger_cause_controller, 1, player, card);
  return 0;
}
#endif

/* Raptor Hatchling	|1|R
 * Creature - Dinosaur 1/1
 * Enrage - Whenever ~ is dealt damage, create a 3/3 |Sgreen Dinosaur creature token with trample. */

/* Repeating Barrage	|1|R|R
 * Sorcery
 * ~ deals 3 damage to any target.
 * Raid - |3|R|R: Return ~ from your graveyard to your hand. Activate this ability only if you attacked with a creature this turn. */

/* Rigging Runner	|R
 * Creature - Goblin Pirate 1/1
 * First strike
 * Raid - ~ enters the battlefield with a +1/+1 counter on it if you attacked with a creature this turn. */

/* Rile	|R
 * Sorcery
 * ~ deals 1 damage to target creature you control. That creature gains trample until end of turn.
 * Draw a card. */

/* Rowdy Crew	|2|R|R
 * Creature - Human Pirate 3/3
 * Trample
 * When ~ enters the battlefield, draw three cards, then discard two cards at random. If two cards that share a card type are discarded this way, put two +1/+1 counters on ~. */

/* ManalinkEh 2022: 0x2215610; original cards/ixalan.c:1476. */
int card_star_of_extinction(int player, int card, event_t event)
{
  /* Star of Extinction	|5|R|R
   * Sorcery
   * Destroy target land. ~ deals 20 damage to each creature and each planeswalker. */

  int p;
  int player_00;
  int pass_or_result;
  card_instance_t *instance;
  int card_00;
  int player_pass;
  target_definition_t *land_target;
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
        if (event == EVENT_RESOLVE_SPELL)
        {
          if (valid_target(&td) != 0)
          {
            instance = get_card_instance(player, card);
            kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
          }
          player_pass = 0;
          p = current_turn;
          player_00 = p;
          do
          {
            card_00 = active_cards_count[player_00] - 1;
            if (player_pass != 0)
            {
              p = current_turn;
              current_turn = p;
            }
            while (card_00 >= 0)
            {
              if (is_what(player_00, card_00, 2) != 0)
                damage_creature(player_00, card_00, 0x14, player, card);
              pass_or_result = is_what(player_00, card_00, 0x100);
              p = current_turn;
              if (pass_or_result != 0)
              {
                damage_creature(player_00, card_00, 0x14, player, card);
                p = current_turn;
              }
              card_00 = card_00 - 1;
              current_turn = p;
            }
            player_pass = player_pass + 1;
            if (player_pass == 2)
              break;
            player_00 = 1 - p;
          } while (true);
          kill_card(player, card, KILL_DESTROY);
        }
        land_target = &td;
        return generic_spell(player, card, event, 1, land_target, "TARGET_LAND", 1, NULL);
      }
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
    }
  }
  default_target_definition(player, card, &td, 1);
  land_target = &td;
  return generic_spell(player, card, event, 1, land_target, "TARGET_LAND", 1, NULL);
}

/* Storm Fleet Arsonist	|4|R
 * Creature - Orc Pirate 4/4
 * Raid - When ~ enters the battlefield, if you attacked with a creature this turn, target opponent sacrifices a permanent. */

/* Storm Fleet Pyromancer	|4|R	=>khans_of_tarkir.c:card_mardu_heart_piercer 0x200cea6
 * Creature - Human Pirate Wizard 3/2
 * Raid - When ~ enters the battlefield, if you attacked with a creature this turn, ~ deals 2 damage to any target. */

/* Sun-Crowned Hunters	|4|R|R
 * Creature - Dinosaur 5/4
 * Enrage - Whenever ~ is dealt damage, it deals 3 damage to target opponent or planeswalker. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2215840; original cards/ixalan.c:1521. */
int card_sunbirds_invocation(int player, int card, event_t event)
{
  /* Sunbird's Invocation	|5|R
   * Enchantment
   * Whenever you cast a spell from your hand, reveal the top X cards of your library, where X is that spell's converted mana cost. You may cast a card revealed this way with converted mana cost X or less without paying its mana cost. Put the rest on the bottom of your library in a random order. */

  if (trigger_condition == TRIGGER_SPELL_CAST && card == affected_card && player == affected_card_controller &&
      reason_for_trigger_controller == player)
  {
    if (!not_played_from_hand(trigger_cause_controller, trigger_cause))
      sp_ability_trigger(player, event, 2, TYPE_ANY, NULL, player, card);
  }
  if (event == EVENT_SPA)
  {
    int amount = get_cmc(trigger_cause_controller, trigger_cause);
    int num_cards = count_deck(player);
    if (amount <= num_cards)
      num_cards = amount;
    if (num_cards == 0)
      return 0;
    show_deck(player, deck_ptr[player], num_cards, "Cards revealed by Sunbird's Invocation.",
                   0, gs_done_008b40e0);
    int selection_array[2][num_cards];
    int eligible = 0;
    if (num_cards > 0)
    {
      for (int i = 0; i != num_cards; ++i)
      {
        int iid = deck_ptr[player][i];
        selection_array[0][i] = iid;
        selection_array[1][i] = iid;
        if (!can_legally_play_iid(player, iid) ||
            get_cmc_by_internal_id(deck_ptr[player][i]) >
                get_cmc(trigger_cause_controller, trigger_cause) ||
            (cards_data[deck_ptr[player][i]].type & TYPE_LAND))
        {
          selection_array[1][i] = iid_draw_a_card;
        }
        else
          ++eligible;
      }
      for (int i = 0; i != num_cards; ++i)
        remove_card_from_deck(player, 0);
    }
    int selected_iid = -1;
    if (eligible != 0)
    {
      test_definition_t this_test;
      new_default_test_definition(&this_test, TYPE_LAND, "Select a spell to cast.");
      this_test.type_flag = 1;
      this_test.id = 0x388;
      this_test.id_flag = 1;
      int s = select_card_from_zone(player, player, selection_array[1], num_cards, 0, 2, -1,
                                         &this_test);
      if (s != -1)
      {
        selected_iid = selection_array[0][s];
        selection_array[0][s] = -1;
        selection_array[1][s] = iid_draw_a_card;
      }
    }
    for (int i = num_cards - 1; i >= 0; --i)
      if (selection_array[0][i] != -1)
        raw_put_iid_on_top_of_deck(player, selection_array[0][i]);
    int bottom_count = num_cards;
    if (selected_iid != -1)
    {
      fake_play_card_in_zone_for_free(player, player, selected_iid, 0x10000);
      bottom_count = num_cards - 1;
    }
    put_top_x_on_bottom_in_random_order(player, bottom_count);
  }
  return global_enchantment(player, card, event);
}
#endif

/* Swashbuckling	|1|R	=>zendikar.c:card_goblin_war_paint 0x2001b2d
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+2 and has haste. */

/* Thrash of Raptors	|3|R
 * Creature - Dinosaur 3/3
 * As long as you control another Dinosaur, ~ gets +2/+0 and has trample. */

/* Tilonalli's Knight	|1|R
 * Creature - Human Knight 2/2
 * Whenever ~ attacks, if you control a Dinosaur, ~ gets +1/+1 until end of turn. */

/* Tilonalli's Skinshifter	|2|R
 * Creature - Human Shaman 0/1
 * Haste
 * Whenever ~ attacks, it becomes a copy of another target nonlegendary attacking creature until end of turn. */

/* Trove of Temptation	|3|R
 * Enchantment
 * Each opponent must attack you or a planeswalker you control with at least one creature each combat if able.
 * At the beginning of your end step, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Unfriendly Fire	|4|R	=>tempest.c:card_lightning_blast 0x200cce4
 * Instant
 * ~ deals 4 damage to any target. */

/* ManalinkEh 2022: 0x2215c10; original cards/ixalan.c:1627. */
int card_vances_blasting_cannons(int player, int card, event_t event)
{
  /* Vance's Blasting Cannons	|3|R
   * Legendary Enchantment
   * At the beginning of your upkeep, exile the top card of your library. If it's a nonland card, you may cast that card this turn.
   * Whenever you cast your third spell in a turn, you may transform ~. */

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    unsigned short csvid = cards_data[*deck_ptr[player]].id;
    rfg_top_card_of_deck(player);
    if (!is_what(-1, csvid, TYPE_LAND))
      create_may_play_card_from_exile_effect(player, card, player, csvid, MPCFE_UNTIL_EOT);
  }
  if (get_specific_storm_count(player) == 2)
  {
    int mode = duh_mode(player) ? 2 : 1;
    if (new_specific_spell_played(player, card, event, player, mode, NULL))
      transform(player, card);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2215d40; original cards/ixalan.c:1648. */
int card_spitfire_bastion(int player, int card, event_t event)
{
  /* Spitfire Bastion	""
   * Legendary Land
   * |T: Add |R.
   * |2|R, |T: ~ deals 3 damage to any target. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  double_faced_card(player, card, event);
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      instance = get_card_instance(player, card);

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = paying_mana();
      if (((result == 0) &&
           (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 3, 0, 0, 0, 1,
                                               0, 0, &td, "TARGET_ANY"),
            result != 0)) &&
          (result =
               do_dialog(player, player, card, -1, -1, " Produce mana\n Deal damage\n Cancel", 1),
           result != 0))
      {
        if (result == 1)
        {
          instance->state = instance->state | 0x10;
          generic_activated_ability(player, card, EVENT_ACTIVATE, 4, 2, 0, 0, 0, 1, 0, 0, &td,
                                    "TARGET_ANY");
          if (spell_fizzled == 0)
          {
            instance->info_slot = 1;
          }
          else
          {
            instance->state = instance->state & 0xffffffef;
          }
          tapped_for_mana_color = -1;
        }
        else
        {
          spell_fizzled = 1;
        }
        return 0;
      }
      result = mana_producer(player, card, EVENT_ACTIVATE);
      return result;
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot == 1)
      {
        damage_target0(player, card, 3);
        return 0;
      }
      return 0;
    }
  }
  result = mana_producer(player, card, event);
  return result;
}

/* Wily Goblin	|R|R
 * Creature - Goblin Pirate 1/1
 * When ~ enters the battlefield, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Ancient Brontodon	|6|G|G	0x401000
 * Creature - Dinosaur 9/9 */

/* Atzocan Archer	|2|G
 * Creature - Human Archer 1/4
 * Reach
 * When ~ enters the battlefield, you may have it fight another target creature. */

/* Blinding Fog	|2|G
 * Instant
 * Prevent all damage that would be dealt to creatures this turn. Creatures you control gain hexproof until end of turn. */

/* Blossom Dryad	|2|G	=>unlimited.c:card_ley_druid 0x2003c39
 * Creature - Dryad 2/2
 * |T: Untap target land. */

/* ManalinkEh 2022: 0x2215fe0; original cards/ixalan.c:1721. */
int card_carnage_tyrant(int player, int card, event_t event)
{
  /* Carnage Tyrant	|4|G|G
   * Creature - Dinosaur 7/6
   * This spell can't be countered.
   * Trample, hexproof */

  cannot_be_countered(player, card, event);
  hexproof(player, card, event);
  return 0;
}

/* Colossal Dreadmaw	|4|G|G	0x401000
 * Creature - Dinosaur 6/6
 * Trample */

/* ManalinkEh 2022: 0x2212840; original cards/ixalan.c:1737. */
static int test_dinosaur_or_land(int iid, int value, int player, int card)
{
  int result;

  result = is_what(-1, iid, 1);
  if (result != 0)
  {
    return 1;
  }
  result = has_subtype_by_id(player, (unsigned int)cards_data[iid].id, SUBTYPE_DINOSAUR);
  return (unsigned int)(result != 0);
}

/* ManalinkEh 2022: 0x2216020; original cards/ixalan.c:1740. */
int card_commune_with_dinosaurs(int player, int card, event_t event)
{
  /* Commune with Dinosaurs	|G
   * Sorcery
   * Look at the top five cards of your library. You may reveal a Dinosaur or land card from among them and put it into your hand. Put the rest on the bottom of your library in any order. */

  int look_count;
  int tutored_iid;
  test_definition_t test;

  if (event == EVENT_RESOLVE_SPELL)
  {

    look_count = count_deck(player);
    if (0 < look_count)
    {
      if (5 < look_count)
      {
        look_count = 5;
      }
      new_default_test_definition(&test, 0, "Select a Dinosaur or land card.");
      test.special_selection_function = test_dinosaur_or_land;
      test.create_minideck = (unsigned short)look_count;
      test.no_shuffle = 1;
      tutored_iid = new_global_tutor(player, player, 1, 1, 0, 1, &test);
      if ((tutored_iid < 0) || (look_count = look_count + -1, 0 < look_count))
      {
        put_top_x_on_bottom(player, player, look_count);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* Crash the Ramparts	|2|G	=>mirrodin.c:card_predators_strike 0x200532c
 * Instant
 * Target creature gets +3/+3 and gains trample until end of turn. */

/* ManalinkEh 2022: 0x2216130; original cards/ixalan.c:1771. */
int card_crushing_canopy(int player, int card, event_t event)
{
  /* Crushing Canopy	|2|G
   * Instant
   * Choose one -
   * * Destroy target creature with flying.
   * * Destroy target enchantment. */

  enum
  {
    CHOICE_FLYER = 1,
    CHOICE_ENCHANTMENT = 2
  };

  int choice;
  card_instance_t *instance;
  int enchantment_available_or_valid;
  target_definition_t *td;
  const char *prompt;
  target_definition_t td1;
  target_definition_t td2;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td1, 2);
    td1.required_abilities = 0x20;
    default_target_definition(player, card, &td2, 4);
    get_card_instance(player, card);
    if (generic_spell(player, card, EVENT_CAN_CAST, 1, &td1, "TARGET_FLYING", 1, NULL) == 0)
    {
      return generic_spell(player, card, EVENT_CAN_CAST, 1, &td2, "TARGET_ENCHANTMENT", 1, NULL);
    }
    return 1;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      td = &td2;
      default_target_definition(player, card, &td1, 2);
      td1.required_abilities = 0x20;
      default_target_definition(player, card, td, 4);
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        if (check_special_flags2(player, card, -0x80000000) == 0)
        {

          enchantment_available_or_valid = can_target(td);
          choice =
              dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
                          "Destroy a flying creature", can_target(&td1), 5,
                          "Destroy an enchantment", enchantment_available_or_valid, 5, 0, 0, 0, 0);
          if (choice == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          if (choice == CHOICE_FLYER)
          {
            td = &td1;
            prompt = "TARGET_FLYING";
          }
          else
          {
            if (choice != CHOICE_ENCHANTMENT)
            {
              return 0;
            }
            prompt = "TARGET_ENCHANTMENT";
          }
          return generic_spell(player, card, EVENT_CAST_SPELL, 1, td, prompt, 1, NULL);
        }
      }
    }
    return 0;
  }
  if ((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET))
  {
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
    default_target_definition(player, card, &td1, 2);
    td1.required_abilities = 0x20;
    default_target_definition(player, card, &td2, 4);
    get_card_instance(player, card);
    return 0;
  }
  default_target_definition(player, card, &td1, 2);
  td1.required_abilities = 0x20;
  default_target_definition(player, card, &td2, 4);
  instance = get_card_instance(player, card);
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }

  if (instance->info_slot == CHOICE_FLYER)
  {
    enchantment_available_or_valid = valid_target(&td1);
  }
  else
  {
    if (instance->info_slot != CHOICE_ENCHANTMENT)
    {
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
    enchantment_available_or_valid = valid_target(&td2);
  }
  if (enchantment_available_or_valid != 0)
  {
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
  }
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

/* ManalinkEh 2022: 0x2216540; original cards/ixalan.c:1830. */
int card_deathgorge_scavenger(int player, int card, event_t event)
{
  /* Deathgorge Scavenger	|2|G
   * Creature - Dinosaur 3/2
   * Whenever ~ enters the battlefield or attacks, you may exile target card from a graveyard. If a creature card is exiled this way, you gain 2 life. If a noncreature card is exiled this way, ~ gets +1/+1 until end of turn. */

  unsigned char exiled_types;
  int target_player;
  int result;
  card_instance_t *instance;
  test_definition_t test;

  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if ((event == EVENT_ETB) || (event == EVENT_DAT_ABILITY))
  {
    new_default_test_definition(&test, 0xff, "Select target card to exile.");
    result = select_target_from_either_grave(player, card, SFG_0, 3, 1, &test, 0, 1);
    if (result != -1)
    {

      instance = get_card_instance(player, card);
      result = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
      if (result != -1)
      {

        target_player = instance->targets[0].player;
        exiled_types = cards_data[graveyard_ptr_mutable[target_player][result]].type;
        rfg_card_from_grave(target_player, result);
        if ((exiled_types & 2) == 0)
        {
          pump_until_eot_merge_previous(player, card, player, card, 1, 1);
        }
        else
        {
          gain_life(player, 2);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22166c0; original cards/ixalan.c:1860. */
int card_deeproot_champion(int player, int card, event_t event)
{
  /* Deeproot Champion	|1|G
   * Creature - Merfolk Shaman 1/1
   * Whenever you cast a noncreature spell, put a +1/+1 counter on ~. */

  test_definition_t test;

  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 2);
    test.type_flag = 1;
    sp_ability_trigger(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event != EVENT_SPA)
  {
    return 0;
  }
  add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  return 0;
}

/* Deeproot Warrior	|1|G
 * Creature - Merfolk Warrior 2/2
 * Whenever ~ becomes blocked, it gets +1/+1 until end of turn. */

/* ManalinkEh 2022: 0x22167a0; original cards/ixalan.c:1884. */
int card_drover_of_the_mighty(int player, int card, event_t event)
{
  /* Drover of the Mighty	|1|G
   * Creature - Human Druid 1/1
   * ~ gets +2/+2 as long as you control a Dinosaur.
   * |T: Add one mana of any color. */

  int result;
  card_instance_t *instance;
  test_definition_t test;

  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      new_default_test_definition(&test, 0, "");
      test.subtype = 0xf0;
      result = check_battlefield_for_special_card(player, card, player, 0, &test);
      if (result != 0)
      {
        event_result = event_result + 2;
      }
    }
  }
  result = card_generic_combat_1_mana_producing_creature(player, card, event);
  return result;
}

/* Emergent Growth	|3|G
 * Sorcery
 * Target creature gets +5/+5 until end of turn and must be blocked this turn if able. */

/* Emperor's Vanguard	|3|G
 * Creature - Human Scout 4/3
 * Whenever ~ deals combat damage to a player, it explores. */

/* Grazing Whiptail	|2|G|G
 * Creature - Dinosaur 3/4
 * Reach */

/* ManalinkEh 2022: 0x2216860; original cards/ixalan.c:1914. */
int card_growing_rites_of_itlimoc(int player, int card, event_t event)
{
  /* Growing Rites of Itlimoc	|2|G
   * Legendary Enchantment
   * When ~ enters the battlefield, look at the top four cards of your library. You may reveal a creature card from among them and put it into your hand. Put the rest on the bottom of your library in any order.
   * At the beginning of your end step, if you control four or more creatures, transform ~. */

  int look_count_or_result;
  int tutored_iid;
  test_definition_t test;
  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if ((event == EVENT_ETB) && (look_count_or_result = count_deck(player), 0 < look_count_or_result))
  {
    if (4 < look_count_or_result)
    {
      look_count_or_result = 4;
    }
    new_default_test_definition(&test, 2, "Select a creature card.");
    test.create_minideck = (unsigned short)look_count_or_result;
    test.no_shuffle = 1;
    tutored_iid = new_global_tutor(player, player, 1, 1, 0, 1, &test);
    if ((tutored_iid < 0) ||
        (look_count_or_result = look_count_or_result + -1, 0 < look_count_or_result))
    {
      put_top_x_on_bottom(player, player, look_count_or_result);
    }
  }
  if (((current_turn == player) &&
       (look_count_or_result = eot_trigger(player, card, event), look_count_or_result != 0)) &&
      (look_count_or_result = count_permanents_by_type(player, TYPE_CREATURE),
       3 < look_count_or_result))
  {
    transform(player, card);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x22169f0; original cards/ixalan.c:1951. */
int card_itlimoc_cradle_of_the_sun(int player, int card, event_t event)
{
  /* Itlimoc, Cradle of the Sun	""
   * Legendary Land
   * |T: Add |G.
   * |T: Add |G for each creature you control. */

  int result;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  if (event == EVENT_CAN_ACTIVATE)
  {

    result = mana_producer(player, card, EVENT_CAN_ACTIVATE);
    if ((result != 0) &&
        (((ai_is_speculating != 1 && ((player != 1 || (((unsigned char)trace_mode & 2) != 0)))) ||
          (result = count_subtype(player, 2, -1), result != 0))))
    {
      return 1;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    result = count_subtype(player, 2, -1);
    result = do_dialog(player, player, card, -1, -1, " Produce G\n Gaea\'s Cradle\n Cancel",
                       (unsigned int)(1 < result));
    if (result == 0)
    {
      event = EVENT_ACTIVATE;

      return mana_producer(player, card, event);
    }
    if (result == 1)
    {
      result = count_subtype(player, 2, -1);
      produce_mana_tapped(player, card, COLOR_GREEN, result);
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
      if ((result != 0) && (result = count_subtype(player, 2, -1), 0 < result))
      {
        result = count_subtype(player, 2, -1);
        declare_mana_available(player, 3, result);
        return 0;
      }
      result = mana_producer(player, card, EVENT_CAN_ACTIVATE);
      if (result != 0)
      {
        declare_mana_available(player, 3, 1);
        return 0;
      }
    }
  }
  return 0;
}

/* Ixalli's Diviner	|1|G
 * Creature - Human Druid 0/3
 * When ~ enters the battlefield, it explores. */

/* Ixalli's Keeper	|1|G
 * Creature - Human Shaman 2/2
 * |7|G, |T, Sacrifice ~: Target creature gets +5/+5 and gains trample until end of turn. */

/* Jade Guardian	|3|G
 * Creature - Merfolk Shaman 2/2
 * Hexproof
 * When ~ enters the battlefield, put a +1/+1 counter on target Merfolk you control. */

/* Jungle Delver	|G
 * Creature - Merfolk Warrior 1/1
 * |3|G: Put a +1/+1 counter on ~. */

/* Kumena's Speaker	|G
 * Creature - Merfolk Shaman 1/1
 * ~ gets +1/+1 as long as you control another Merfolk or |Han Island. */

/* ManalinkEh 2022: 0x2216c50; original cards/ixalan.c:2028. */
int card_merfolk_branchwalker(int player, int card, event_t event)
{
  /* Merfolk Branchwalker	|1|G
   * Creature - Merfolk Scout 2/1
   * When ~ enters the battlefield, it explores. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  explore(player, card, EVENT_ETB);
  return 0;
}

/* New Horizons	|2|G
 * Enchantment - Aura
 * Enchant land
 * When ~ enters the battlefield, put a +1/+1 counter on target creature you control.
 * Enchanted land has "|T: Add two mana of any one color." */

/* Old-Growth Dryads	|G
 * Creature - Dryad 3/3
 * When ~ enters the battlefield, each opponent may search their library for a basic land card, put it onto the battlefield tapped, then shuffle their library. */

/* Pounce	|1|G	=>innistrad.c:card_prey_upon 0x20049db
 * Instant
 * Target creature you control fights target creature you don't control. */

/* Ranging Raptors	|2|G
 * Creature - Dinosaur 2/3
 * Enrage - Whenever ~ is dealt damage, you may search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library. */

/* Ravenous Daggertooth	|2|G
 * Creature - Dinosaur 3/2
 * Enrage - Whenever ~ is dealt damage, you gain 2 life. */

/* ManalinkEh 2022: 0x2216cb0; original cards/ixalan.c:2060. */
int card_ripjaw_raptor(int player, int card, event_t event)
{
  /* Ripjaw Raptor	|2|G|G
   * Creature - Dinosaur 4/5
   * Enrage - Whenever ~ is dealt damage, draw a card. */

  int result;

  result = is_humiliated(player, card, AL_TRIG);
  if (result == 0)
  {
    result = damage_dealt_to_me_arbitrary(player, card, event, DDBM_0, player, card);
    if (result != 0)
    {
      draw_a_card(player);
      return 0;
    }
  }
  return 0;
}

/* River Heralds' Boon	|1|G
 * Instant
 * Put a +1/+1 counter on target creature and a +1/+1 counter on up to one target Merfolk. */

/* Savage Stomp	|2|G
 * Sorcery
 * This spell costs |2 less to cast if it targets a Dinosaur you control.
 * Put a +1/+1 counter on target creature you control. Then that creature fights target creature you don't control. */

/* Shapers' Sanctuary	|G
 * Enchantment
 * Whenever a creature you control becomes the target of a spell or ability an opponent controls, you may draw a card. */

/* Snapping Sailback	|4|G
 * Creature - Dinosaur 4/4
 * Flash
 * Enrage - Whenever ~ is dealt damage, put a +1/+1 counter on it. */

/* Spike-Tailed Ceratops	|4|G	=>unlimited.c:card_two_headed_giant_of_foriys 0x200b263
 * Creature - Dinosaur 4/4
 * ~ can block an additional creature each combat. */

/* ManalinkEh 2022: 0x2216d20; original cards/ixalan.c:2095. */
int card_thundering_spineback(int player, int card, event_t event)
{
  /* Thundering Spineback	|5|G|G
   * Creature - Dinosaur 5/5
   * Other Dinosaurs you control get +1/+1.
   * |5|G: Create a 3/3 |Sgreen Dinosaur creature token with trample. */

  int result;
  boost_creature_type(player, card, event, SUBTYPE_DINOSAUR, 1, 1, 0, BCT_CONTROLLER_ONLY);
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    generate_token_by_id(player, card, CARD_ID_DINOSAUR);
  }
  result = generic_activated_ability(player, card, event, 0, 5, 0, 0, 1, 0, 0, 0, NULL, NULL);
  return result;
}

/* Tishana's Wayfinder	|2|G
 * Creature - Merfolk Scout 2/2
 * When ~ enters the battlefield, it explores. */

/* Verdant Rebirth	|1|G
 * Instant
 * Until end of turn, target creature gains "When this creature dies, return it to its owner's hand."
 * Draw a card. */

/* ManalinkEh 2022: 0x2216e00; original cards/ixalan.c:2123. */
int card_verdant_suns_avatar(int player, int card, event_t event)
{
  /* Verdant Sun's Avatar	|5|G|G
   * Creature - Dinosaur Avatar 5/5
   * Whenever ~ or another creature enters the battlefield under your control, you gain life equal to that creature's toughness. */

  int result;
  card_instance_t *instance;
  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  result = is_humiliated(player, card, AL_TRIG);
  if ((((result == 0) && (trigger_condition == TRIGGER_COMES_INTO_PLAY)) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 2);
    test.not_me = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event != EVENT_ETB)
  {
    return 0;
  }
  instance = get_card_instance(trigger_cause_controller, trigger_cause);
  instance->regen_status = instance->regen_status | 0x6000000;
  result = get_toughness(trigger_cause_controller, trigger_cause);
  gain_life(player, result);
  return 0;
}

/* Vineshaper Mystic	|2|G
 * Creature - Merfolk Shaman 1/3
 * When ~ enters the battlefield, put a +1/+1 counter on each of up to two target Merfolk you control. */

/* Waker of the Wilds	|2|G|G
 * Creature - Merfolk Shaman 3/3
 * |X|G|G: Put X +1/+1 counters on target land you control. That land becomes a 0/0 Elemental creature with haste. It's still a land. */

/* ManalinkEh 2022: 0x2216f20; original cards/ixalan.c:2153. */
int card_wildgrowth_walker(int player, int card, event_t event)
{
  /* Wildgrowth Walker	|1|G
   * Creature - Elemental 1/3
   * Whenever a creature you control explores, put a +1/+1 counter on ~ and you gain 3 life. */

  if ((event == EVENT_EXPLORE) && (affected_card_controller == player))
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    gain_life(player, 3);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2216f90; original cards/ixalan.c:2164. */
int card_admiral_beckett_brass(int player, int card, event_t event)
{
  /* Admiral Beckett Brass	|1|U|B|R
   * Legendary Creature - Human Pirate 3/3
   * Other Pirates you control get +1/+1.
   * At the beginning of your end step, gain control of target nonland permanent controlled by a player who was dealt combat damage by three or more Pirates this turn. */

  card_instance_t *instance;
  int result;
  card_instance_t *source_in_play;
  target_definition_t td;

  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    boost_creature_type(player, card, event, SUBTYPE_PIRATE, 1, 1, 0, BCT_CONTROLLER_ONLY);
    get_card_instance(player, card);
    get_trap_condition(player, 0x40a);
    return 0;
  }
  instance = get_card_instance(player, card);
  result = get_trap_condition(player, 0x40a);
  if (event != EVENT_PHASE_CHANGED)
    return 0;
  if (result >= 3 && current_phase == 0x22)
  {
    if (current_turn != player)
    {
      return 0;
    }
    if (instance->info_slot != 0)
    {
      return 0;
    }
    result = is_humiliated(player, card, AL_TRIG);
    if ((result == 0) && (source_in_play = in_play(player, card), source_in_play != NULL))
    {
      default_target_definition(player, card, &td, 0x47);
      td.allowed_controller = 1 - current_turn;
      td.illegal_type = TYPE_LAND;
      td.allow_cancel = 0;
      result = can_target(&td);
      if ((result != 0) &&
          (result = new_pick_target(&td, "TARGET_NONLAND_PERMANENT", 0, 0), result != 0))
      {
        gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
        instance->info_slot = 1;
      }
    }
  }
  if (current_phase != 0x22)
    instance->info_slot = 0;
  return 0;
}

/* Belligerent Brontodon	|5|G|W
 * Creature - Dinosaur 4/6
 * Each creature you control assigns combat damage equal to its toughness rather than its power. */

/* Call to the Feast	|2|W|B
 * Sorcery
 * Create three 1/1 |Swhite Vampire creature tokens with lifelink. */

/* Deadeye Plunderers	|3|U|B
 * Creature - Human Pirate 3/3
 * ~ gets +1/+1 for each artifact you control.
 * |2|U|B: Create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Dire Fleet Captain	|B|R
 * Creature - Orc Pirate 2/2
 * Whenever ~ attacks, it gets +1/+1 until end of turn for each other attacking Pirate. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2217170; original cards/ixalan.c:2213. */
int card_gishath_suns_avatar(int player, int card, event_t event)
{
  /* Gishath, Sun's Avatar	|5|R|G|W
   * Legendary Creature - Dinosaur Avatar 7/6
   * Vigilance, trample, haste
   * Whenever ~ deals combat damage to a player, reveal that many cards from the top of your library. Put any number of Dinosaur creature cards from among them onto the battlefield and the rest on the bottom of your library in a random order. */

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  haste(player, card, event);
  if (damage_dealt_by_me(player, card, event,
                         DDBM_MUST_DAMAGE_PLAYER | DDBM_REPORT_DAMAGE_DEALT |
                             DDBM_MUST_BE_COMBAT_DAMAGE))
  {
    int amount = get_card_instance(player, card)->targets[16].player;
    if (amount == -1)
      return 0;
    int count = count_deck(player);
    if (count < amount)
      amount = count;
    if (amount != 0)
    {
      int gc_flag = grafdiggers_cages(player, TYPE_CREATURE, TUTOR_FROM_DECK);
      show_deck(0, deck_ptr[player], amount, "Cards revealed by Gishath, Sun's Avatar", 0,
                     gs_done_008b40e0);
      show_deck(1, deck_ptr[player], amount, "Cards revealed by Gishath, Sun's Avatar", 0,
                     gs_done_008b40e0);
      if (gc_flag == 0)
      {
        int deck_copy[amount];
        for (int i = 0; i < amount; ++i)
        {
          deck_copy[i] = deck_ptr[player][0];
          remove_card_from_deck(player, 0);
        }
        char msg[100];
        scnprintf(msg, 100, "Select a Dinosaur creature card.");
        test_definition_t test;
        new_default_test_definition(&test, TYPE_CREATURE, msg);
        test.subtype = 0xf0;
        int remaining = amount;
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
              put_top_x_on_bottom_in_random_order(player, remaining);
            }
            break;
          }
          if (deck_copy[selected] != -1)
            fake_play_card_in_zone_for_free(player, player, deck_copy[selected], 0x10000);
          --remaining;
          for (int i = selected; i < remaining; ++i)
            deck_copy[i] = deck_copy[i + 1];
        } while (remaining != 0);
      }
    }
  }
  if (event == EVENT_PHASE_CHANGED && current_phase == 0x22)
    get_card_instance(player, card)->targets[16].player = 0;
  return 0;
}
#endif

/* ManalinkEh 2022: 0x22174d0; original cards/ixalan.c:2275. */
int card_hostage_taker(int player, int card, event_t event)
{
  /* Hostage Taker	|2|U|B
   * Creature - Human Pirate 2/3
   * When ~ enters the battlefield, exile another target creature or artifact until ~ leaves the battlefield. You may cast that card for as long as it remains exiled, and you may spend mana as though it were mana of any type to cast that spell. */

  card_instance_t *instance;
  int csvid;
  int result;
  target_definition_t td;
  return_from_oblivion(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 0x42);
    td.special = TARGET_SPECIAL_NOT_ME;
    td.allow_cancel = 0;
    result = can_target(&td);
    if ((result != 0) && (result = new_pick_target(&td, "PUTREFY", 0, 0), result != 0))
    {
      instance = get_card_instance(player, card);
      result = instance->targets[0].card;
      instance = get_card_instance(player, card);
      csvid = get_id(instance->targets[0].player, result);
      instance = get_card_instance(player, card);
      result = instance->targets[0].card;
      instance = get_card_instance(player, card);
      obliviation(player, card, instance->targets[0].player, result);
      instance = get_card_instance(player, card);
      create_may_play_card_from_exile_effect(player, card, instance->targets[0].player, csvid,
                                             MPCFE_FOR_CMC);
    }
    return 0;
  }
  if (event == EVENT_CAN_CAST)
  {
    result = basic_spell(player, card, EVENT_CAN_CAST);
    return result;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2217650; original cards/ixalan.c:2300. */
int card_huatli_warrior_poet(int player, int card, event_t event)
{
  /* Huatli, Warrior Poet	|3|R|W
   * Legendary Planeswalker - Huatli (3)
   * +2: You gain life equal to the greatest power among creatures you control.
   * 0: Create a 3/3 |Sgreen Dinosaur creature token with trample.
   * -X: ~ deals X damage divided as you choose among any number of target creatures. Creatures dealt damage this way can't block this turn. */

  enum
  {
    CHOICE_LIFE = 1,
    CHOICE_TOKEN = 2,
    CHOICE_DAMAGE = 3
  };

  int i;
  int choice;
  unsigned int target_count;
  target_t *duplicate_or_target_cursor;
  card_instance_t *damage;
  uint32_t argument3;
  unsigned char live_target_count;
  target_t *target_cursor;
  int card_or_next_target;
  uint32_t ret_location;
  int mode_index_or_result;
  int divided_damage;
  target_definition_t *creature_target;
  card_instance_t *instance;
  token_generation_t token;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event != EVENT_ACTIVATE)
      return planeswalker(player, card, event, 6);

    instance->number_of_targets = '\0';
    instance->info_slot = 0;
    choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Gain life", 1, 5, 2,
                         "Dino Token", 1, 10, 0, "Target & Divide Damage", can_target(&td), 10, 0,
                         0, 0, 0, 0);
    if (choice == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (choice == CHOICE_DAMAGE)
    {

      mode_index_or_result = count_counters(player, card, COUNTER_LOYALTY);
      argument3 = mode_index_or_result - 1;
      if (player == 0)
      {
        argument3 = choose_a_number(0, "Pay how much Loyalty ?", argument3);
      }
      if (spell_fizzled != 1)
      {
        if (((int)argument3 < 1) ||
            (mode_index_or_result = count_counters(player, card, COUNTER_LOYALTY),
             mode_index_or_result < (int)argument3))
        {
          spell_fizzled = 1;
        }
        else
        {

          remove_counters(player, card, player, card, COUNTER_LOYALTY, argument3);
          instance->eot_toughness = argument3;
          ret_location = 0;
          instance->number_of_targets = '\0';
          do
          {
            new_pick_target(&td, "TARGET_ANY", ret_location, 1);
            duplicate_or_target_cursor = instance->targets + ret_location;
            target_cursor = instance->targets + ret_location;
            ret_location = ret_location + 1;
            add_state(target_cursor->player, duplicate_or_target_cursor->card, 0x200000);
          } while (argument3 != ret_location);
          if (instance->number_of_targets != '\0')
          {
            i = 0;
            do
            {
              card_or_next_target = instance->targets[i].card;
              if (card_or_next_target != -1)
              {
                remove_state(instance->targets[i].player, card_or_next_target, 0x200000);
              }
              i = i + 1;
            } while (i < (int)(unsigned int)instance->number_of_targets);
          }
        }
      }
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      mode_index_or_result = 3;
      return planeswalker(player, card, event, mode_index_or_result);
    }
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    creature_target = &td;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      mode_index_or_result = instance->info_slot;
      if (mode_index_or_result == CHOICE_LIFE)
      {
        gain_life(player, get_highest_power(player));
      }
      else if (mode_index_or_result == CHOICE_TOKEN)
      {
        default_token_definition(player, card, CARD_ID_DINOSAUR, &token);
        token.special_infos = 0x42;
        generate_token(&token);
      }
      else if (mode_index_or_result == CHOICE_DAMAGE)
      {

        live_target_count = instance->number_of_targets;
        target_count = (unsigned int)live_target_count;
        if (target_count != 0)
        {
          target_cursor = instance->targets;
          mode_index_or_result = 0;
          do
          {
            target_cursor = target_cursor + 1;
            card_or_next_target = mode_index_or_result + 1;
            if (-1 < target_cursor[-1].player)
            {

              if (card_or_next_target < (int)target_count)
              {
                divided_damage = 1;
                duplicate_or_target_cursor = target_cursor;
                for (; duplicate_or_target_cursor != instance->targets + target_count;
                     ++duplicate_or_target_cursor)
                  if (duplicate_or_target_cursor->card == target_cursor[-1].card &&
                      duplicate_or_target_cursor->player == target_cursor[-1].player)
                  {
                    duplicate_or_target_cursor->card = -1;
                    duplicate_or_target_cursor->player = -1;
                    ++divided_damage;
                  }
              }
              else
              {
                divided_damage = 1;
              }
              if (validate_target(player, card, creature_target, mode_index_or_result) == 0)
              {
                target_cursor[-1].card = -1;
                target_cursor[-1].player = -1;
                live_target_count = instance->number_of_targets;
              }
              else
              {

                damage = get_card_instance(player, damage_creature(target_cursor[-1].player,
                                                                   target_cursor[-1].card,
                                                                   divided_damage, player, card));
                damage->targets[10].card = 0x441d;
                live_target_count = instance->number_of_targets;
              }
            }
            target_count = (unsigned int)live_target_count;
            mode_index_or_result = card_or_next_target;
          } while (card_or_next_target < (int)target_count);
        }
      }
    }
  }
  mode_index_or_result = 6;
  return planeswalker(player, card, event, mode_index_or_result);
}

/* Marauding Looter	|2|U|R
 * Creature - Human Pirate 4/3
 * Raid - At the beginning of your end step, if you attacked with a creature this turn, you may draw a card. If you do, discard a card. */

/* ManalinkEh 2022: 0x2217b50; original cards/ixalan.c:2411. */
int card_raging_swordtooth(int player, int card, event_t event)
{
  /* Raging Swordtooth	|3|R|G
   * Creature - Dinosaur 5/5
   * Trample
   * When ~ enters the battlefield, it deals 1 damage to each other creature. */

  test_definition_t this_test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  default_test_definition(&this_test, 2);
  this_test.not_me = 1;
  new_damage_all(player, card, 2, 1, 0, &this_test);
  return 0;
}

/* ManalinkEh 2022: 0x2217bf0; original cards/ixalan.c:2428. */
int card_regisaur_alpha(int player, int card, event_t event)
{
  /* Regisaur Alpha	|3|R|G
   * Creature - Dinosaur 4/4
   * Other Dinosaurs you control have haste.
   * When ~ enters the battlefield, create a 3/3 |Sgreen Dinosaur creature token with trample. */

  token_generation_t token;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_token_definition(player, card, CARD_ID_DINOSAUR, &token);
    token.tou = 3;
    token.pow = 3;
    token.color_forced = '\b';
    generate_token(&token);
    return 0;
  }
  if (event != EVENT_ABILITIES)
  {
    return 0;
  }
  boost_subtype(player, card, EVENT_ABILITIES, SUBTYPE_DINOSAUR, 0, 0, 0, 4, BCT_CONTROLLER_ONLY);
  return 0;
}

/* Shapers of Nature	|1|G|U
 * Creature - Merfolk Shaman 3/3
 * |3|G: Put a +1/+1 counter on target creature.
 * |2|U, Remove a +1/+1 counter from a creature you control: Draw a card. */

/* Sky Terror	|R|W
 * Creature - Dinosaur 2/2
 * Flying, menace */

/* ManalinkEh 2022: 0x2217cd0; original cards/ixalan.c:2459. */
int card_tishana_voice_of_thunder(int player, int card, event_t event)
{
  /* Tishana, Voice of Thunder	|5|G|U
   * Legendary Creature - Merfolk Shaman 100/100
   * ~'s power and toughness are each equal to the number of cards in your hand.
   * You have no maximum hand size.
   * When ~ enters the battlefield, draw a card for each creature you control. */

  card_instance_t *instance;
  int result;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    if ((uint32_t)(event - EVENT_POWER) < 2)
    {
      if (((card == affected_card) && (affected_card_controller == player)) && (player != -1))
      {
        set_characteristic_pt(hand_count[player]);
      }
    }
    else if ((current_turn == player) && (event == EVENT_MAX_HAND_SIZE))
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {
          event_result = event_result + 99;
        }
      }
    }
    return 0;
  }
  result = count_permanents_by_type(player, TYPE_CREATURE);
  draw_cards(player, result);
  return 0;
}

/* ManalinkEh 2022: 0x2217dd0; original cards/ixalan.c:2480. */
int card_vona_butcher_of_magan(int player, int card, event_t event)
{
  /* Vona, Butcher of Magan	|3|W|B
   * Legendary Creature - Vampire Knight 4/4
   * Vigilance, lifelink
   * |T, Pay 7 life: Destroy target nonland permanent. Activate this ability only during your turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  lifelink(player, card, event);
  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 0x47);
  td.illegal_type = TYPE_LAND;
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    instance = get_card_instance(player, card);
    result = instance->targets[0].card;
    instance = get_card_instance(player, card);
    kill_card(instance->targets[0].player, result, KILL_DESTROY);
  }
  result = generic_activated_ability(player, card, event, 0x10005, 0, 0, 0, 0, 0, 0, 7, &td,
                                     "TARGET_PERMANENT");
  return result;
}

/* ManalinkEh 2022: 0x2217f60; original cards/ixalan.c:2504. */
int card_vraska_relic_seeker(int player, int card, event_t event)
{
  /* Vraska, Relic Seeker	|4|B|G
   * Legendary Planeswalker - Vraska (6)
   * +2: Create a 2/2 |Sblack Pirate creature token with menace.
   * -3: Destroy target artifact, creature, or enchantment. Create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color."
   * -10: Target player's life total becomes 1. */

  enum
  {
    CHOICE_TOKEN = 1,
    CHOICE_DESTROY = 2,
    CHOICE_ULTIMATE = 3
  };

  int choice;
  card_instance_t *instance;
  int opponent_available_or_mode;
  target_definition_t *permanent_target;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x46);
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      opponent_available_or_mode = opponent_is_valid_target(player, card);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Pirate Token", 1,
                           5, 2, "Destroy", can_target(&td), 10, 0xfffffffd, "Set life to 1",
                           opponent_available_or_mode, 0x14, 0xfffffff6, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_DESTROY)
      {
        char buffer[50];
        scnprintf(buffer, 0x32, "Select target artifact, creature, or enchantment");
        new_pick_target(&td, buffer, 0, 0x81);
      }
      else if (choice == CHOICE_ULTIMATE)
      {
        td.zone = TARGET_ZONE_PLAYERS;
        new_pick_target(&td, "TARGET_PLAYER", 0, 1);
      }
      return planeswalker(player, card, event, 6);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 6);
    default_target_definition(player, card, &td, 0x46);
    instance = get_card_instance(player, card);
  }
  permanent_target = &td;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    opponent_available_or_mode = instance->info_slot;
    if (opponent_available_or_mode == CHOICE_TOKEN)
    {
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_PIRATE, &token);
      token.s_key_plus = 0x10000;
      generate_token(&token);
    }
    else if (opponent_available_or_mode == CHOICE_DESTROY)
    {
      if (validate_target(player, card, permanent_target, 0) != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        token_generation_t token;
        default_token_definition(player, card, CARD_ID_TREASURE, &token);
        generate_token(&token);
      }
    }
    else if (opponent_available_or_mode == CHOICE_ULTIMATE)
    {
      td.zone = TARGET_ZONE_PLAYERS;
      if (validate_target(player, card, permanent_target, 0) != 0)
      {
        set_life_total(instance->targets[0].player, 1);
      }
    }
  }
  return planeswalker(player, card, event, 6);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22128b0; original cards/ixalan.c:2582. */
static int transform_at_end_of_combat(int player, int card, event_t event)
{
  if (!end_of_combat_trigger(player, card, event, 2))
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  if (in_play(instance->damage_target_player, instance->damage_target_card) &&
      !check_state(instance->damage_target_player, instance->damage_target_card, STATE_OUBLIETTED))
    blink_effect_controller(instance->damage_target_player, instance->damage_target_card,
                            return_to_bf_transformed_impl);
  kill_card(player, card, KILL_EXILE);
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22182e0; original cards/ixalan.c:2593. */
int card_conquerors_galleon(int player, int card, event_t event)
{
  /* Conqueror's Galleon	|4
   * Artifact - Vehicle 2/10
   * When ~ attacks, exile it at end of combat, then return it to the battlefield transformed under your control.
   * Crew 4 */

  int result;
  card_instance_t *instance;

  double_faced_card(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    result = create_targetted_legacy_effect(player, card, transform_at_end_of_combat, player, card);
    alternate_legacy_text(2, player, result);
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    result = turn_into_creature(instance->parent_controller, instance->parent_card,
                                instance->parent_controller, instance->parent_card, 1, 0, 0);
    result = alternate_legacy_text(1, player, result);
    return result;
  }
  return vehicle(player, card, event, 4);
}
#endif

/* ManalinkEh 2022: 0x22183f0; original cards/ixalan.c:2614. */
int card_conquerors_foothold(int player, int card, event_t event)
{
  /* Conqueror's Foothold	""
   * Land
   * |T: Add |C.
   * |2, |T: Draw a card, then discard a card.
   * |4, |T: Draw a card.
   * |6, |T: Return target card from your graveyard to your hand. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_DRAW_DISCARD = 2,
    CHOICE_DRAW = 3,
    CHOICE_REGROW = 4
  };

  int choice;
  int available_cost_or_grave_index;
  int draw_available;
  card_instance_t *instance;
  int32_t activation_mode;
  test_definition_t test;

  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event != EVENT_ACTIVATE)
    {
      if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
      {
        new_default_test_definition(&test, 0xff, "Select target card.");
        instance = get_card_instance(player, card);
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          activation_mode = instance->info_slot;
          if (activation_mode == 2)
          {
            draw_a_card(player);
            discard(player, 0, player);
            activation_mode = instance->info_slot;
          }
          if (activation_mode == 3)
          {
            draw_a_card(player);
            activation_mode = instance->info_slot;
          }

          if ((activation_mode == 4) &&
              (available_cost_or_grave_index =
                   validate_target_from_grave_source(player, card, player, 0),
               available_cost_or_grave_index != -1))
          {
            from_grave_to_hand(player, available_cost_or_grave_index, 1);
          }
        }
        return 0;
      }
      return mana_producer(player, card, event);
    }
    new_default_test_definition(&test, 0xff, "Select target card.");
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
    if (paying_mana() == 0)
    {

      available_cost_or_grave_index = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1,
                                                                3, 0, 0, 0, 0, 0, 0, NULL, NULL);
      draw_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 5, 0, 0, 0, 0,
                                                 0, 0, NULL, NULL);
      choice = dialog_impl(
          player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, "Produce mana", 1, 5,
          "Draw & discard", available_cost_or_grave_index, 1, "Draw", draw_available, 3, "Regrowth",
          generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE,
                                                         GAA_UNTAPPED, 7, 0, 0, 0, 0, 0, 0, NULL,
                                                         NULL, TYPE_NONE, &test, 8),
          5, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      instance->info_slot = choice;
      if (choice != CHOICE_MANA)
      {
        add_state(player, card, 0x10);
        if (choice == CHOICE_DRAW_DISCARD)
        {
          available_cost_or_grave_index = 2;
        }
        else
        {
          if (choice != CHOICE_DRAW)
          {
            if (choice != CHOICE_REGROW)
            {
              tapped_for_mana_color = -1;
              return 0;
            }
            generic_activated_ability_with_target_in_grave(player, card, EVENT_ACTIVATE, GAA_NONE,
                                                           6, 0, 0, 0, 0, 0, 0, NULL, NULL,
                                                           TYPE_NONE, &test, 8);
            if (spell_fizzled == 1)
              remove_state(player, card, 0x10);
            tapped_for_mana_color = -1;
            return 0;
          }
          available_cost_or_grave_index = 4;
        }
        generic_activated_ability(player, card, EVENT_ACTIVATE, 0, available_cost_or_grave_index, 0,
                                  0, 0, 0, 0, 0, NULL, NULL);
        if (spell_fizzled == 1)
        {
          remove_state(player, card, 0x10);
        }
        tapped_for_mana_color = -1;
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

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2218990; original cards/ixalan.c:2702. */
int card_dowsing_dagger(int player, int card, event_t event)
{
  /* Dowsing Dagger	|2
   * Artifact - Equipment
   * When ~ enters the battlefield, target opponent creates two 0/2 |Sgreen Plant creature tokens with defender.
   * Equipped creature gets +2/+1.
   * Whenever equipped creature deals combat damage to a player, you may transform ~.
   * Equip |2 */

  int result;
  card_instance_t *instance;
  token_generation_t token;
  double_faced_card(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    target_opponent(player, card);
    default_token_definition(player, card, CARD_ID_PLANT, &token);
    token.qty = '\x02';
    token.tou = 2;
    instance = get_card_instance(player, card);
    token.key_plus = KEYWORD_DEFENDER;
    token.t_player = instance->targets[0].player;
    generate_token(&token);
  }
  result = equipped_creature_deals_damage(player, card, event,
                                          DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE |
                                              DDBM_TRIGGER_OPTIONAL);
  if (result != 0)
  {
    unattach(player, card);
    transform(player, card);
  }
  result = vanilla_equipment(player, card, event, 2, 2, 1, 0, 0);
  return result;
}
#endif

/* ManalinkEh 2022: 0x2218ab0; original cards/ixalan.c:2729. */
int card_lost_vale(int player, int card, event_t event)
{
  /* Lost Vale	""
   * Land
   * |T: Add three mana of any one color. */

  double_faced_card(player, card, event);
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
    produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 3);
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION || event == EVENT_CLEANUP)
    return 0;
  if (event != EVENT_COUNT_MANA || card != affected_card || player != affected_card_controller)
    return mana_producer(player, card, event);
  if (mana_producer(player, card, EVENT_CAN_ACTIVATE))
  {
    declare_mana_available_hex(player, 0x3e, 3);
    if (life[player] < 1 && mana_colors_added[player] != COLOR_TEST_0)
      declare_mana_available_maybe_hex(player, mana_colors_added[player], 1);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2218d00; original cards/ixalan.c:2783. */
int card_dusk_legion_dreadnought(int player, int card, event_t event)
{
  /* Dusk Legion Dreadnought	|5
   * Artifact - Vehicle 4/6
   * Vigilance
   * Crew 2 */

  int result;
  vigilance(player, card, event);
  result = vehicle(player, card, event, 2);
  return result;
}

/* Elaborate Firecannon	|2
 * Artifact
 * ~ doesn't untap during your untap step.
 * |4, |T: ~ deals 2 damage to any target.
 * At the beginning of your upkeep, you may discard a card. If you do, untap ~. */

/* Fell Flagship	|3
 * Artifact - Vehicle 3/3
 * Pirates you control get +1/+0.
 * Whenever ~ deals combat damage to a player, that player discards a card.
 * Crew 3 */

/* Gilded Sentinel	|4	0x401000
 * Artifact Creature - Golem 3/3 */

/* Hierophant's Chalice	|3
 * Artifact
 * When ~ enters the battlefield, target opponent loses 1 life and you gain 1 life.
 * |T: Add |C. */

/* Pillar of Origins	|2
 * Artifact
 * As ~ enters the battlefield, choose a creature type.
 * |T: Add one mana of any color. Spend this mana only to cast a creature spell of the chosen type. */

/* ManalinkEh 2022: 0x2218d50; original cards/ixalan.c:2818. */
int card_pirates_cutlass(int player, int card, event_t event)
{
  /* Pirate's Cutlass	|3
   * Artifact - Equipment
   * When ~ enters the battlefield, attach it to target Pirate you control.
   * Equipped creature gets +2/+1.
   * Equip |2 */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 2);
    td.required_subtype = 0x95;
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "Select target Pirate you control.", 0, 0x80);
      if (result != 0)
      {
        equip_target_creature(player, card, instance->targets[0].player, instance->targets[0].card);
      }
    }
    instance->number_of_targets = '\0';
  }
  result = vanilla_equipment(player, card, event, 2, 2, 1, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x22126f0; original cards/ixalan.c:2843. */
static int primal_wellspring_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if (event == EVENT_CAST_SPELL)
  {
    result = played_for_free(affected_card_controller, affected_card);
    if (result == 0)
    {
      result = is_token(affected_card_controller, affected_card);
      if (result == 0)
      {
        result = check_special_flags(affected_card_controller, affected_card, 0x40000);
        if ((result == 0) && (affected_card_controller == player))
        {
          result = is_what(player, affected_card, 0x38);
          if ((result != 0) && (0 < mana_paid[instance->targets[6].player]))
          {
            result = get_id(affected_card_controller, affected_card);
            copy_spell(player, result);
            kill_card(player, card, KILL_EXILE);
          }
        }
      }
    }
  }
  else if (((event == EVENT_STATIC_EFFECTS) && (stack_size == 0)) &&
           (mana_pool[player][instance->targets[6].player] < 1))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2218e70; original cards/ixalan.c:2864. */
int card_primal_amulet(int player, int card, event_t event)
{
  /* Primal Amulet	|4
   * Artifact
   * Instant and sorcery spells you cast cost |1 less to cast.
   * Whenever you cast an instant or sorcery spell, put a charge counter on ~. Then if there are four or more charge counters on it, you may remove those counters and transform it. */

  int result;

  double_faced_card(player, card, event);
  if (event != EVENT_MODIFY_COST_GLOBAL)
  {
    sp_ability_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
    if (event == EVENT_SPA)
    {
      add_counters(player, card, player, card, COUNTER_CHARGE, 1);
      result = count_counters(player, card, COUNTER_CHARGE);
      if (3 < result)
      {
        remove_all_counters(player, card, player, card, COUNTER_CHARGE);
        transform(player, card);
      }
    }
    return 0;
  }
  if (affected_card_controller == player)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      result = is_what(affected_card_controller, affected_card, 0x38);
      if (result != 0)
      {
        COST_COLORLESS = COST_COLORLESS + -1;
      }
    }
  }
  sp_ability_trigger(player, EVENT_MODIFY_COST_GLOBAL, 2, TYPE_SPELL, NULL, player, card);
  return 0;
}

/* ManalinkEh 2022: 0x2218fe0; original cards/ixalan.c:2891. */
int card_primal_wellspring(int player, int card, event_t event)
{
  /* Primal Wellspring	""
   * Land
   * |T: Add one mana of any color. When that mana is spent to cast an instant or sorcery spell, copy that spell and you may choose new targets for the copy. */

  int result;
  card_instance_t *instance;

  double_faced_card(player, card, event);
  if (event != EVENT_ACTIVATE)
  {
    result = mana_producer(player, card, event);
    return result;
  }

  mana_producer(player, card, EVENT_ACTIVATE);
  result = create_legacy_effect(player, card, primal_wellspring_legacy);
  instance = get_card_instance(player, result);
  instance->targets[6].player = tapped_for_mana_color;
  add_status(player, result, 0x10000);
  return 0;
}

/* Prying Blade	|1
 * Artifact - Equipment
 * Equipped creature gets +1/+0.
 * Whenever equipped creature deals combat damage to a player, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color."
 * Equip |2 */

/* Sentinel Totem	|1
 * Artifact
 * When ~ enters the battlefield, scry 1.
 * |T, Exile ~: Exile all cards from all graveyards. */

/* Shadowed Caravel	|2
 * Artifact - Vehicle 2/2
 * Whenever a creature you control explores, put a +1/+1 counter on ~.
 * Crew 2 */

/* Sleek Schooner	|3	=>kaladesh.c:card_sky_stiff	0x200f4c1
 * Artifact - Vehicle 4/3
 * Crew 1 */

/* ManalinkEh 2022: 0x2219080; original cards/ixalan.c:2928. */
int card_sorcerous_spyglass(int player, int card, event_t event)
{
  /* Sorcerous Spyglass	|2
   * Artifact
   * As ~ enters the battlefield, look at an opponent's hand, then choose any card name.
   * Activated abilities of sources with the chosen name can't be activated unless they're mana abilities. */

  int result;
  card_instance_t *instance;
  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    reveal_target_player_hand(1 - player);
    new_default_test_definition(&test, 0xff, "Select a card.");
    result = new_card_from_list(player, 3, &test);
    instance = get_card_instance(player, card);
    instance->info_slot = result;
    activated_abilities_disabler_enters_bf(player, card, 0x2a);
  }
  result = activated_abilities_disabler(player, card, event, 0x2a);
  return result;
}

/* ManalinkEh 2022: 0x2219160; original cards/ixalan.c:2949. */
int card_thaumatic_compass(int player, int card, event_t event)
{
  /* Thaumatic Compass	|2
   * Artifact
   * |3, |T: Search your library for a basic land card, reveal it, put it into your hand, then shuffle your library.
   * At the beginning of your end step, if you control seven or more lands, transform ~. */

  double_faced_card(player, card, event);
  if (current_turn == player && eot_trigger(player, card, event) &&
      count_permanents_by_type(player, TYPE_LAND) > 6)
  {
    if (event == EVENT_TRIGGER)
    {
      event_result |= 2;
      return 0;
    }
    if (event == EVENT_RESOLVE_TRIGGER)
    {
      transform(player, card);
      return 0;
    }
  }
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
    tutor_basic_land(player, 0, 0);
  return generic_activated_ability(player, card, event, 1, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x22192b0; original cards/ixalan.c:2973. */
int card_spires_of_orazca(int player, int card, event_t event)
{
  /* Spires of Orazca	""
   * Land
   * |T: Add |C.
   * |T: Untap target attacking creature an opponent controls and remove it from combat. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ITH = 2
  };

  int result;
  card_instance_t *instance;
  bool human_or_trace;
  target_definition_t *creature_target;
  target_definition_t td;

  double_faced_card(player, card, event);
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = 1 - player;
      td.required_state = TARGET_STATE_ATTACKING;
      instance = get_card_instance(player, card);

      instance->number_of_targets = '\0';
      result = paying_mana();
      if ((result == 0) && (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5,
                                                               0, 0, 0, 0, 0, 0, 0, &td, NULL),
                            result != 0))
      {
        human_or_trace = ai_is_speculating != 1;
        instance->info_slot = 2;
        if ((human_or_trace) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
        {
          result = can_target(&td);
          result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Get mana", 1, 1,
                               "Remove a creature from combat", result, 1, 0, 0, 0, 0);
          if (result == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          if (result == CHOICE_MANA)
            return mana_producer(player, card, EVENT_ACTIVATE);
          if (result != CHOICE_ITH)
          {
            return 0;
          }
        }
        add_state(player, card, 0x10);
        generic_activated_ability(player, card, EVENT_ACTIVATE, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                  "TARGET_ATTACKING_CREATURE");
        if (spell_fizzled == 1)
        {
          remove_state(player, card, 0x10);
        }
        return 0;
      }
      instance->info_slot = 1;
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      default_target_definition(player, card, &td, 2);
      td.allowed_controller = 1 - player;
      td.required_state = TARGET_STATE_ATTACKING;
      instance = get_card_instance(player, card);
      creature_target = &td;
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot != 2)
      {
        return 0;
      }
      result = validate_target(player, card, creature_target, 0);
      if (result == 0)
      {
        return 0;
      }
      untap_card(instance->targets[0].player, instance->targets[0].card);
      remove_state(instance->targets[0].player, instance->targets[0].card, 4);
      return 0;
    }
  }
  return mana_producer(player, card, event);
}

/* ManalinkEh 2022: 0x2219620; original cards/ixalan.c:3041. */
int card_treasure_map(int player, int card, event_t event)
{
  /* Treasure Map	|2
   * Artifact
   * |1, |T: Scry 1. Put a landmark counter on ~. Then if there are three or more landmark counters on it, remove those counters, transform ~, and create three colorless Treasure artifact tokens with "|T, Sacrifice this artifact: Add one mana of any color." */

  int result;
  card_instance_t *instance;

  double_faced_card(player, card, event);
  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      scry(player, 1);
      add_counters(player, card, player, card, COUNTER_LANDMARK, 1);
      result = count_counters(player, card, COUNTER_LANDMARK);
      if (2 < result)
      {
        remove_all_counters(player, card, player, card, COUNTER_LANDMARK);
        generate_tokens_by_id(player, card, CARD_ID_TREASURE, 3);
        instance = get_card_instance(player, card);
        result = instance->parent_card;
        instance = get_card_instance(player, card);
        transform(instance->parent_controller, result);
      }
    }
  }
  result = generic_activated_ability(player, card, event, 1, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2219790; original cards/ixalan.c:3061. */
int card_treasure_cove(int player, int card, event_t event)
{
  /* Treasure Cove	""
   * Land
   * |T: Add |C.
   * |T, Sacrifice a Treasure: Draw a card. */

  int result;
  card_instance_t *instance;
  test_definition_t test;

  double_faced_card(player, card, event);
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      new_default_test_definition(&test, 0x47, "Select a Treasure to sacrifice.");
      test.id = 0x4998;
      instance = get_card_instance(player, card);

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = paying_mana();
      if (((result == 0) && (result = generic_activated_ability_with_sacrifice_as_cost(
                                 player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, 0, 0, 0, 0, 0, 0,
                                 0, NULL, NULL, TYPE_NONE, &test, SARV_0),
                             result != 0)) &&
          (result = do_dialog(player, player, card, -1, -1,
                              " Produce mana\n Sac a Treasure\n Cancel", 1),
           result != 0))
      {
        if (result == 1)
        {
          generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_ACTIVATE,
                                                           GAA_UNTAPPED, 0, 0, 0, 0, 0, 0, 0, NULL,
                                                           NULL, TYPE_NONE, &test, SARV_0);
          instance->info_slot = 1;
        }
        else
        {
          spell_fizzled = 1;
        }
        return 0;
      }
      result = mana_producer(player, card, EVENT_ACTIVATE);
      return result;
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      new_default_test_definition(&test, 0x47, "Select a Treasure to sacrifice.");
      test.id = 0x4998;
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot == 1)
      {
        draw_a_card(player);
        return 0;
      }
      return 0;
    }
  }
  result = mana_producer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2219a40; original cards/ixalan.c:3108. */
int card_vanquishers_banner(int player, int card, event_t event)
{
  /* Vanquisher's Banner	|5
   * Artifact
   * As ~ enters the battlefield, choose a creature type.
   * Creatures you control of the chosen type get +1/+1.
   * Whenever you cast a creature spell of the chosen type, draw a card. */

  card_instance_t *instance;
  int result;

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
  }
  else if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    instance = get_card_instance(player, card);
    boost_creature_type(player, card, event, instance->info_slot, 1, 1, 0, BCT_CONTROLLER_ONLY);
  }
  if ((((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((reason_for_trigger_controller == player && (trigger_cause_controller == player))))
  {
    instance = get_card_instance(player, card);
    result = has_subtype(trigger_cause_controller, trigger_cause, instance->info_slot);
    if ((result != 0) &&
        (result = is_what(trigger_cause_controller, trigger_cause, 2), result != 0))
    {
      if (event == EVENT_TRIGGER)
      {
        event_result = event_result | 2;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {
        draw_a_card(player);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2219c10; original cards/ixalan.c:3145. */
int card_field_of_ruin(int player, int card, event_t event)
{
  /* Field of Ruin	""
   * Land
   * |T: Add |C.
   * |2, |T, Sacrifice ~: Destroy target nonbasic land an opponent controls. Each player searches their library for a basic land card, puts it onto the battlefield, then shuffles their library. */

  int choice;
  int turn_player;
  int choice_or_result;
  card_instance_t *instance;
  unsigned int player_pass;
  target_definition_t td;

  if (event == EVENT_CAN_ACTIVATE)
  {
    return mana_producer(player, card, event);
  }
  if (event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
    return mana_producer(player, card, event);
  default_target_definition(player, card, &td, 1);
  if (player == 1)
  {
    td.required_subtype = 0x2000;
    td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_ACTIVATE)
  {

    instance->number_of_targets = '\0';
    instance->info_slot = 0;
    choice_or_result = paying_mana();
    if (((choice_or_result == 0) &&
         (choice_or_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xd, 3, 0,
                                                       0, 0, 0, 0, 0, &td, NULL),
          choice_or_result != 0)) &&
        (choice_or_result =
             do_dialog(player, player, card, -1, -1, " Produce mana\n Kill a land\n Cancel", 1),
         choice_or_result != 0))
    {
      if (choice_or_result == 1)
      {
        add_state(player, card, 0x10);
        choice_or_result = charge_mana_for_activated_ability(player, card, 2, 0, 0, 0, 0, 0);
        if ((choice_or_result == 0) ||
            (choice_or_result = new_pick_target(&td, "TARGET_LAND", 0, 1), choice_or_result == 0))
        {
          untap_card_no_event(player, card);
          spell_fizzled = 1;
        }
        else
        {
          instance->info_slot = 1;
          kill_card(player, card, KILL_SACRIFICE);
        }
        tapped_for_mana_color = -1;
        return 0;
      }
      spell_fizzled = 1;
      return 0;
    }
    return mana_producer(player, card, EVENT_ACTIVATE);
  }
  if (((event == EVENT_RESOLVE_ACTIVATION) && (instance->info_slot == 1)) &&
      (choice_or_result = validate_target(player, card, &td, 0), choice_or_result != 0))
  {

    player_pass = 0;
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    choice_or_result = 1;
    do
    {
      turn_player = current_turn;
      if (current_turn == player_pass)
      {
        choice = do_dialog(0, player, card, -1, -1, " Tutor a basic land\n Pass", 0);
        choice_or_result = 1 - choice;
      }
      if (choice_or_result != 0)
      {
        tutor_basic_land(turn_player ^ player_pass, 1, 0);
      }
      player_pass = player_pass + 1;
    } while (player_pass != 2);
  }
  return 0;
}

/* Unclaimed Territory	""
 * Land
 * As ~ enters the battlefield, choose a creature type.
 * |T: Add |C.
 * |T: Add one mana of any color. Spend this mana only to cast a creature spell of the chosen type. */

/* Jace, Ingenious Mind-Mage	|4|U|U
 * Legendary Planeswalker - Jace (5)
 * +1: Draw a card.
 * +1: Untap all creatures you control.
 * -9: Gain control of up to three target creatures. */

/* Castaway's Despair	|3|U	=>innistrad.c:card_claustrophobia 0x200485f
 * Enchantment - Aura
 * Enchant creature
 * When ~ enters the battlefield, tap enchanted creature.
 * Enchanted creature doesn't untap during its controller's untap step. */

/* Grasping Current	|4|U
 * Sorcery
 * Return up to two target creatures to their owner's hand.
 * Search your library and/or graveyard for a card named Jace, Ingenious Mind-Mage, reveal it, and put it into your hand. If you searched your library this way, shuffle it. */

/* Jace's Sentinel	|1|U
 * Creature - Merfolk Warrior 1/3
 * As long as you control a Jace planeswalker, ~ gets +1/+0 and can't be blocked. */

/* Huatli, Dinosaur Knight	|4|R|W
 * Legendary Planeswalker - Huatli (4)
 * +2: Put two +1/+1 counters on up to one target Dinosaur you control.
 * -3: Target Dinosaur you control deals damage equal to its power to target creature you don't control.
 * -7: Dinosaurs you control get +4/+4 until end of turn. */

/* Huatli's Snubhorn	|1|W	=>unlimited.c:card_serra_angel 0x200c122
 * Creature - Dinosaur 2/2
 * Vigilance */

/* Huatli's Spurring	|R
 * Instant
 * Target creature gets +2/+0 until end of turn. If you control a Huatli planeswalker, that creature gets +4/+0 until end of turn instead. */

/* Sun-Blessed Mount	|3|R|W
 * Creature - Dinosaur 4/4
 * When ~ enters the battlefield, you may search your library and/or graveyard for a card named Huatli, Dinosaur Knight, reveal it, then put it into your hand. If you searched your library this way, shuffle it. */
