#include "manalink.h"

/****************************
 * 2018 Christmas Gift Pack *
 ****************************/

int doublecast_legacy(int player, int card, event_t event);
static int patient_rebuilding_legacy(int player, int card, event_t event);

static int open_the_graves_effect(int player, int card, event_t event);
static int pta_effect(int player, int card, event_t event);

int isareth_legacy(int player, int card, event_t event);

int palladia_mors_legacy(int player, int card, event_t event);

#ifndef SHANDALAR

static int test_not_in_array4(int iid, int arrptr, int player, int card);

#endif

#ifndef SHANDALAR

int card_arcades_the_strategist(int player, int card, event_t event);

#endif

#ifndef SHANDALAR

int card_chromium_the_mutable(int player, int card, event_t event);
int card_aethershield_artificer(int player, int card, event_t event);

#endif

/* Abnormal Endurance	|1|B
 * Instant
 * Until end of turn, target creature gets +2/+0 and gains "When this creature dies, return it to the battlefield tapped under its owner's control." */

/* ManalinkEh 2022: 0x22067b0; original cards/m19.c:7. */
int card_aegis_of_the_heavens(int player, int card, event_t event)
{
  /* Aegis of the Heavens	|1|W
   * Instant
   * Target creature gets +1/+7 until end of turn. */

  int result;
  result = vanilla_instant_pump(player, card, event, 2, player, 1, 7, 0, 0);
  return result;
}

/* Aerial Engineer	|2|W|U
 * Creature - Human Artificer 2/4
 * As long as you control an artifact, ~ gets +2/+0 and has flying. */

/* Aether Tunnel	|1|U
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +1/+0 and can't be blocked. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2206800; original cards/m19.c:24. */
int card_aethershield_artificer(int player, int card, event_t event)
{
  /* Aethershield Artificer	|3|W
   * Creature - Dwarf Artificer 3/3
   * At the beginning of combat on your turn, target artifact creature you control gets +2/+2 and gains indestructible until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  default_target_definition(player, card, &td, 2);
  td.special = TARGET_SPECIAL_ARTIFACT_CREATURE;
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.allow_cancel = 0;
  if (event == EVENT_CHANGE_TYPE)
  {
    result = engine_any_legal_attacker(player);
    if ((result == 0) && (current_turn == player))
    {
      event_flags = event_flags | EA_FORCE_ATTACK;
    }
  }
  else if ((event == EVENT_BEGIN_COMBAT) && (current_turn == player))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      result = can_target(&td);
      if (result != 0)
      {
        result = new_pick_target(&td, "Select target artifact creature you control", 0, 0x80);
        if (result != 0)
        {
          instance = get_card_instance(player, card);
          result = instance->targets[0].card;
          instance = get_card_instance(player, card);
          pump_ability_until_eot(player, card, instance->targets[0].player, result, 2, 2, 0,
                                      0x800);
        }
      }
    }
  }
  return 0;
}
#endif

/* Aggressive Mammoth	|3|G|G|G
 * Creature - Elephant 8/8
 * Trample
 * Other creatures you control have trample. */

/* Ajani's Influence	|2|W|W
 * Sorcery
 * Put two +1/+1 counters on target creature.
 * Look at the top five cards of your library. You may reveal a |Swhite card from among them and put it into your hand. Put the rest on the bottom of your library in a random order. */

/* Ajani's Last Stand	|2|W|W
 * Enchantment
 * Whenever a creature or planeswalker you control dies, you may sacrifice ~. If you do, create a 4/4 |Swhite Avatar creature token with flying.
 * When a spell or ability an opponent controls causes you to discard this card, if you control |Ha Plains, create a 4/4 |Swhite Avatar creature token with flying. */

/* ManalinkEh 2022: 0x2206930; original cards/m19.c:59. */
int card_ajanis_welcome(int player, int card, event_t event)
{
  /* Ajani's Welcome	|W
   * Enchantment
   * Whenever a creature enters the battlefield under your control, you gain 1 life. */

  int result;
  test_definition_t test;
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 2);
    result = new_specific_cip(player, card, event, player, 2, &test);
    if (result != 0)
    {
      gain_life(player, 1);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22069e0; original cards/m19.c:76. */
int card_ajani_adversary_of_tyrants_emblem(int player, int card, event_t event)
{
  /* Ajani, Adversary of Tyrants Emblem	""
   * Emblem - Ajani
   * At the beginning of your end step, create three 1/1 white Cat creature tokens with lifelink. */

  int result;
  card_instance_t *instance;
  token_generation_t token;

  result = is_humiliated(player, card, AL_TRIG);
  if ((result == 0) && (current_turn == player))
  {
    result = eot_trigger(player, card, event);
    if (result != 0)
    {
      default_token_definition(player, card, CARD_ID_CAT, &token);
      token.tou = 1;
      token.pow = 1;
      instance = get_card_instance(player, card);
      token.qty = '\x03';
      token.s_key_plus = 2;
      token.color_forced = (unsigned char)instance->targets[1].card;
      generate_token(&token);
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2206a90; original cards/m19.c:91. */
int card_ajani_adversary_of_tyrants(int player, int card, event_t event)
{
  /* Ajani, Adversary of Tyrants	|2|W|W
   * Legendary Planeswalker - Ajani (4)
   * +1: Put a +1/+1 counter on each of up to two target creatures.
   * -2: Return target creature card with converted mana cost 2 or less from your graveyard to the battlefield.
   * -7: You get an emblem with "At the beginning of your end step, create three 1/1 |Swhite Cat creature tokens with lifelink." */

  enum
  {
    CHOICE_COUNTERS = 1,
    CHOICE_REANIMATE = 2,
    CHOICE_EMBLEM = 3
  };

  int i;
  int choice;
  target_t *ret_location;
  card_instance_t *instance;
  int targets_or_mode;
  int target_available_or_selected;
  unsigned int reanimate_available;
  target_t *target_cursor;
  target_t *next_target_cursor;
  target_definition_t td1;
  test_definition_t test_resurrect;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td1, 2);
    td1.preferred_controller = player;
    td1.allow_cancel = 3;
    default_test_definition(&test_resurrect, 2);
    test_resurrect.cmc = 3;
    test_resurrect.cmc_flag = 3;
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      targets_or_mode = new_special_count_grave(player, &test_resurrect);
      reanimate_available = 0;
      if (0 < targets_or_mode)
      {
        targets_or_mode = graveyard_has_shroud(player);
        reanimate_available = (unsigned int)(targets_or_mode == 0);
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Add counters", 1,
                           10, 1, "Reanimate a creature", reanimate_available, 10, 0xfffffffe,
                           "Create an emblem", 1, 10, 0xfffffff9, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_COUNTERS)
      {

        if (can_target(&td1) != 0)
        {
          ret_location = instance->targets;
          if (select_target(player, card, &td1, " Select target creature.", ret_location) != 0)
          {
            state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
            target_available_or_selected = can_target(&td1);
            targets_or_mode = 1;
            if ((target_available_or_selected != 0) &&
                (target_available_or_selected = select_target(
                     player, card, &td1, " Select target creature.", instance->targets + 1),
                 target_available_or_selected != 0))
            {
              state_untargettable(instance->targets[1].player, instance->targets[1].card, 1);
              targets_or_mode = 2;
            }
            target_cursor = ret_location;
            do
            {
              next_target_cursor = target_cursor + 1;
              state_untargettable(target_cursor->player, target_cursor->card, 0);
              target_cursor = next_target_cursor;
            } while (next_target_cursor != ret_location + targets_or_mode);
          }
        }
      }
      else if (choice == CHOICE_REANIMATE)
      {
        select_target_from_grave_source(player, card, player, SFG_0, 1, &test_resurrect, 0);
      }
      return planeswalker(player, card, event, 4);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 4);
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td1, 2);
    td1.preferred_controller = player;
    td1.allow_cancel = 3;
    default_test_definition(&test_resurrect, 2);
  }
  test_resurrect.cmc_flag = 3;
  test_resurrect.cmc = 3;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    targets_or_mode = instance->info_slot;
    if (targets_or_mode == 1)
    {

      if (instance->number_of_targets != '\0')
      {
        i = 0;
        do
        {
          state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
          add_counters(player, card, instance->targets[i].player, instance->targets[i].card,
                       COUNTER_P1_P1, 1);
          i = i + 1;
        } while (i < (int)(unsigned int)instance->number_of_targets);
      }
      instance->number_of_targets = '\0';
    }
    else if (targets_or_mode == 2)
    {

      targets_or_mode = validate_target_from_grave_source(player, card, player, 0);
      if (-1 < targets_or_mode)
      {
        reanimate_permanent(player, card, player, targets_or_mode, REANIMATE_DEFAULT);
      }
    }
    else if (targets_or_mode == 3)
    {

      instance = get_card_instance(player, generate_token_by_id(player, card, CARD_ID_AJANI_ADVERSARY_OF_TYRANTS_EMBLEM));
      targets_or_mode = get_sleighted_color_test(player, card, 0x20);
      instance->targets[1].card = targets_or_mode;
    }
  }
  return planeswalker(player, card, event, 4);
}
#endif

/* Ajani, Wise Counselor	|3|W|W
 * Legendary Planeswalker - Ajani (5)
 * +2: You gain 1 life for each creature you control.
 * -3: Creatures you control get +2/+2 until end of turn.
 * -9: Put X +1/+1 counters on target creature, where X is your life total. */

/* Alpine Moon	|R
 * Enchantment
 * As ~ enters the battlefield, choose a nonbasic land card name.
 * Lands your opponents control with the chosen name lose all land types and abilities, and they gain "|T: Add one mana of any color." */

/* Amulet of Safekeeping	|2
 * Artifact
 * Whenever you become the target of a spell or ability an opponent controls, counter that spell or ability unless its controller pays |1.
 * Creature tokens get -1/-0. */

/* Angel of the Dawn	|4|W
 * Creature - Angel 3/3
 * Flying
 * When ~ enters the battlefield, creatures you control get +1/+1 and gain vigilance until end of turn. */

/* ManalinkEh 2022: 0x2206f30; original cards/m19.c:207. */
int card_apex_of_power(int player, int card, event_t event)
{
  /* Apex of Power	|7|R|R|R
   * Sorcery
   * Exile the top seven cards of your library. Until end of turn, you may cast nonland cards exiled this way.
   * If this spell was cast from your hand, add ten mana of any one color. */

  int result;
  test_definition_t this_test;
  if (!IS_GS_EVENT(player, card, event))
    return 0;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&this_test, 1);
    this_test.type_flag = 1;
    exile_top_x_card_you_may_play_them(player, card, 7, &this_test, player, MPCFE_UNTIL_EOT);
    result = not_played_from_hand(player, card);
    if ((result == 0) && (result = check_special_flags(player, card, 0x40000), result == 0))
    {
      produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 10);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2207060; original cards/m19.c:228. */
int card_arcades_the_strategist(int player, int card, event_t event)
{
  /* Arcades, the Strategist	|1|G|W|U
   * Legendary Creature - Elder Dragon 3/5
   * Flying, vigilance
   * Whenever a creature with defender enters the battlefield under your control, draw a card.
   * Each creature you control with defender assigns combat damage equal to its toughness rather than its power and can attack as though it didn't have defender. */

  int result;
  card_instance_t *instance;
  test_definition_t test;
  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    default_test_definition(&test, 2);
    test.keyword = KEYWORD_DEFENDER;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event != EVENT_ETB)
  {
    if ((event == EVENT_ABILITIES) && (affected_card_controller == player))
    {
      result = check_for_ability(player, affected_card, KEYWORD_DEFENDER);
      if (result != 0)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {
          instance = get_card_instance(affected_card_controller, affected_card);
          instance->token_status = instance->token_status | 0x800;
        }
      }
    }
    return 0;
  }
  draw_a_card(player);
  return 0;
}
#endif

/* ManalinkEh 2022: 0x22071b0; original cards/m19.c:256. */
int card_arcane_encyclopedia(int player, int card, event_t event)
{
  /* Arcane Encyclopedia	|3
   * Artifact
   * |3, |T: Draw a card. */

  int result;
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_a_card(player);
  }
  result = generic_activated_ability(player, card, event, 1, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* Arisen Gorgon	|1|B|B
 * Creature - Zombie Gorgon 3/3
 * ~ has deathtouch as long as you control a Liliana planeswalker. */

/* Aven Wind Mage	|2|U
 * Creature - Bird Wizard 2/2
 * Flying
 * Whenever you cast an instant or sorcery spell, ~ gets +1/+1 until end of turn. */

/* Aviation Pioneer	|2|U
 * Creature - Human Artificer 1/2
 * When ~ enters the battlefield, create a 1/1 colorless Thopter artifact creature token with flying. */

/* Blood Divination	|3|B
 * Sorcery
 * As an additional cost to cast this spell, sacrifice a creature.
 * Draw three cards. */

/* Bogstomper	|4|B|B
 * Creature - Beast 6/5 */

/* Bone Dragon	|3|B|B
 * Creature - Dragon Skeleton 5/4
 * Flying
 * |3|B|B, Exile seven other cards from your graveyard: Return ~ from your graveyard to the battlefield tapped. */

/* Brawl-Bash Ogre	|2|B|R
 * Creature - Ogre Warrior 3/3
 * Menace
 * Whenever ~ attacks, you may sacrifice another creature. If you do, ~ gets +2/+2 until end of turn. */

/* Bristling Boar	|3|G
 * Creature - Boar 4/3
 * ~ can't be blocked by more than one creature. */

/* Catalyst Elemental	|2|R
 * Creature - Elemental 2/2
 * Sacrifice ~: Add |R|R. */

/* Cavalry Drillmaster	|1|W
 * Creature - Human Knight 2/1
 * When ~ enters the battlefield, target creature gets +2/+0 and gains first strike until end of turn. */

/* ManalinkEh 2022: 0x2207250; original cards/m19.c:312. */
int card_chaos_wand(int player, int card, event_t event)
{
  /* Chaos Wand	|3
   * Artifact
   * |4, |T: Target opponent exiles cards from the top of their library until they exile an instant or sorcery card. You may cast that card without paying its mana cost. Then put the exiled cards that weren't cast this way on the bottom of that library in a random order. */

  int choice;
  unsigned short spell_csvid;
  int *deck;
  int opponent;
  int iid_or_eligible;
  int amount;
  int pos;
  int num_cards;
  int *library_cursor;
  target_definition_t td;
  char buffer[100];

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    td.allowed_controller = 1 - player;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    opponent = 1 - player;
    td.allowed_controller = opponent;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      deck = deck_ptr[opponent];
      iid_or_eligible = *deck;
      amount = 0;
      num_cards = 0;
      library_cursor = deck;
      if (iid_or_eligible != -1)
      {
        bool spell_found = false;
        do
        {
          pos = num_cards;
          iid_or_eligible = is_what(-1, iid_or_eligible, 0x38);
          num_cards = pos + 1;
          if (iid_or_eligible != 0)
          {
            show_deck(0, deck, num_cards, "Cards revealed by Chaos Wand", 0, gs_done_008b40e0);
            iid_or_eligible = can_legally_play_iid(player, *library_cursor);
            amount = pos;
            if (iid_or_eligible != 0)
            {
              spell_csvid = cards_data[*library_cursor].id;
              scnprintf(buffer, 100, " Play %s\n Decline", cards_ptr[spell_csvid]->name);
              choice = do_dialog(player, player, card, -1, -1, buffer, 0);
              amount = num_cards;
              if (choice == 0)
              {
                rfg_card_in_deck(opponent, pos);
                play_card_in_exile_for_free(player, opponent, (unsigned int)spell_csvid);
                amount = pos;
              }
            }
            spell_found = true;
            break;
          }

          iid_or_eligible = library_cursor[1];
          library_cursor = library_cursor + 1;
        } while (iid_or_eligible != -1);
        if (!spell_found)
        {
          show_deck(0, deck, pos + 2, "Cards revealed by Chaos Wand", 0, gs_done_008b40e0);
          amount = num_cards;
        }
      }
      put_top_x_on_bottom_in_random_order(opponent, amount);
    }
  }
  return generic_activated_ability(player, card, event, 1, 4, 0, 0, 0, 0, 0, 0, &td,
                                   "TARGET_OPPONENT");
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2207550; original cards/m19.c:359. */
int card_chromium_the_mutable(int player, int card, event_t event)
{
  /* Chromium, the Mutable	|4|W|U|B
   * Legendary Creature - Elder Dragon 7/7
   * Flash
   * This spell can't be countered.
   * Flying
   * Discard a card: Until end of turn, ~ becomes a Human with base power and toughness 1/1, loses all abilities, and gains hexproof. It can't be blocked this turn. */

  check_legend_rule(player, card, event);
  cannot_be_countered(player, card, event);
  card_instance_t *instance = get_card_instance(player, card);
  if (!IS_GAA_EVENT(event))
    return flash(player, card, event);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.subtype = 0x5b;
    test.toughness = 1;
    test.power = 1;
    int legacy = humiliate_and_set_pt_abilities(
        player, card, instance->parent_controller, instance->parent_card, 4, &test);
    card_instance_t *leg = get_card_instance(player, legacy);
    leg->targets[4].player = SP_KEYWORD_HEXPROOF | SP_KEYWORD_UNBLOCKABLE;
  }
  return generic_activated_ability(player, card, event, 0x20, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x22076c0; original cards/m19.c:386. */
int card_cleansing_nova(int player, int card, event_t event)
{
  /* Cleansing Nova	|3|W|W
   * Sorcery
   * Choose one -
   * * Destroy all creatures.
   * * Destroy all artifacts and enchantments. */

  int opponent_creatures;
  int own_creatures;
  int opponent_artifacts_enchantments;
  int own_artifacts_enchantments;
  card_instance_t *instance;
  int types[2];
  test_definition_t this_test;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {

      opponent_creatures = count_subtype(1 - player, 2, -1);
      own_creatures = count_subtype(player, 2, -1);
      default_test_definition(&this_test, 0x44);
      this_test.type_flag = 4;
      opponent_artifacts_enchantments =
          check_battlefield_for_special_card(player, card, 1 - player, 4, &this_test);
      own_artifacts_enchantments =
          check_battlefield_for_special_card(player, card, player, 4, &this_test);
      dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, "Destroy all creatures", 1,
                  opponent_creatures - own_creatures, "Destroy all artifacts and enchantments", 1,
                  opponent_artifacts_enchantments - own_artifacts_enchantments, 0, 0, 0, 0);
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {

      types[0] = 2;
      types[1] = 0x44;
      instance = get_card_instance(player, card);
      default_test_definition(&this_test, types[instance->info_slot + -1]);
      new_manipulate_all(player, card, 2, &this_test, ACT_KILL_DESTROY);
      kill_card(player, card, KILL_DESTROY);
    }
    return basic_spell(player, card, event);
  }
  return basic_spell(player, card, EVENT_CAN_CAST);
}

/* ManalinkEh 2022: 0x22078d0; original cards/m19.c:421. */
int card_colossal_majesty(int player, int card, event_t event)
{
  /* Colossal Majesty	|2|G
   * Enchantment
   * At the beginning of your upkeep, if you control a creature with power 4 or greater, draw a card. */

  int result;
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    result = ferocious(player, card);
    if (result != 0)
    {
      draw_a_card(player);
    }
  }
  return global_enchantment(player, card, event);
}

/* Court Cleric	|W
 * Creature - Human Cleric 1/1
 * Lifelink
 * ~ gets +1/+1 as long as you control an Ajani planeswalker. */

/* Dark-Dweller Oracle	|1|R
 * Creature - Goblin Shaman 2/2
 * |1, Sacrifice a creature: Exile the top card of your library. You may play that card this turn. */

/* Daybreak Chaplain	|1|W
 * Creature - Human Cleric 1/3
 * Lifelink */

/* Declare Dominance	|3|G|G
 * Sorcery
 * Target creature gets +3/+3 until end of turn. All creatures able to block it this turn do so. */

/* Demanding Dragon	|3|R|R
 * Creature - Dragon 5/5
 * Flying
 * When ~ enters the battlefield, it deals 5 damage to target opponent unless that player sacrifices a creature. */

/* ManalinkEh 2022: 0x2207940; original cards/m19.c:454. */
int card_demon_of_catastrophes(int player, int card, event_t event)
{
  /* Demon of Catastrophes	|2|B|B
   * Creature - Demon 6/6
   * As an additional cost to cast this spell, sacrifice a creature.
   * Flying, trample */

  int result;
  test_definition_t test;
  if (event == EVENT_CAN_CAST)
  {
    result = can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE);
    return result;
  }
  if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
    result = new_sacrifice(player, card, player, SAC_AS_COST, &test);
    if (result == 0)
    {
      spell_fizzled = 1;
    }
  }
  return 0;
}

/* Departed Deckhand	|1|U
 * Creature - Spirit Pirate 2/2
 * When ~ becomes the target of a spell, sacrifice it.
 * ~ can't be blocked except by Spirits.
 * |3|U: Another target creature you control can't be blocked this turn except by Spirits. */

/* ManalinkEh 2022: 0x2207a00; original cards/m19.c:479. */
int card_desecrated_tomb(int player, int card, event_t event)
{
  /* Desecrated Tomb	|3
   * Artifact
   * Whenever one or more creature cards leave your graveyard, create a 1/1 |Sblack Bat creature token with flying. */

  card_instance_t *instance;
  token_generation_t token;

  instance = get_card_instance(player, card);
  if (event == EVENT_REMOVED_FROM_GRAVEYARD)
  {
    if (((affected_card_controller == player) && (instance->info_slot != 0x42)) &&
        (((unsigned int)event_result >> 16 & 0xff) != '\0'))
    {
      default_token_definition(player, card, CARD_ID_BAT, &token);
      token.tou = 1;
      token.pow = 1;
      generate_token(&token);
      instance->info_slot = 0x42;
    }
  }
  else if ((event == EVENT_STATIC_EFFECTS) && (instance->info_slot == 0x42))
  {
    instance->info_slot = 0;
    return 0;
  }
  return 0;
}

/* Detection Tower	""
 * Land
 * |T: Add |C.
 * |1, |T: Until end of turn, your opponents and creatures your opponents control with hexproof can be the targets of spells and abilities you control as though they didn't have hexproof. */

/* Diamond Mare	|2
 * Artifact Creature - Horse 1/3
 * As ~ enters the battlefield, choose a color.
 * Whenever you cast a spell of the chosen color, you gain 1 life. */

/* ManalinkEh 2022: 0x2207ab0; original cards/m19.c:509. */
int card_dismissive_pyromancer(int player, int card, event_t event)
{
  /* Dismissive Pyromancer	|1|R
   * Creature - Human Wizard 2/2
   * |R, |T, Discard a card: Draw a card.
   * |2|R, |T, Sacrifice ~: It deals 4 damage to target creature. */

  enum
  {
    CHOICE_LOOT = 1,
    CHOICE_DAMAGE = 2
  };

  int choice;
  card_instance_t *instance;
  int available_or_mode;
  target_definition_t td;
  target_definition_t td2;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    default_target_definition(player, card, &td2, 0x44);
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x21, 0, 0, 0, 0, 1, 0, 0,
                                    NULL, NULL) == 0)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xd, 2, 0, 0, 0, 1, 0, 0,
                                         &td, "TARGET_CREATURE");
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {

      available_or_mode = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x21, 0, 0, 0,
                                                    0, 1, 0, 0, NULL, NULL);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Loot", available_or_mode, 10,
                           "Damage creature",
                           generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xd, 2, 0, 0,
                                                     0, 1, 0, 0, &td, "TARGET_CREATURE"),
                           10, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice != CHOICE_LOOT)
      {
        if (choice != CHOICE_DAMAGE)
        {
          return 0;
        }
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 0xd, 2, 0, 0, 0, 1, 0, 0,
                                         &td, "TARGET_CREATURE");
      }
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 0x21, 0, 0, 0, 0, 1, 0, 0,
                                       NULL, NULL);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    default_target_definition(player, card, &td2, 0x44);
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    available_or_mode = instance->info_slot;
    if (available_or_mode == CHOICE_LOOT)
    {
      draw_a_card(player);
      available_or_mode = instance->info_slot;
    }
    if (available_or_mode == CHOICE_DAMAGE)
    {
      damage_target0(player, card, 4);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22058a0; original cards/m19.c:570. */
int doublecast_legacy(int player, int card, event_t event)
{
  int result;
  test_definition_t test;
  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = check_special_flags(trigger_cause_controller, trigger_cause, 0x40000);
    if (result == 0)
    {
      default_test_definition(&test, 0x38);
      result = new_specific_spell_played(player, card, event, player, 2, &test);
      if (result != 0)
      {
        copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x2207ef0; original cards/m19.c:585. */
int card_doublecast(int player, int card, event_t event)
{
  /* Doublecast	|R|R
   * Sorcery
   * When you cast your next instant or sorcery spell this turn, copy that spell. You may choose new targets for the copy. */

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
      create_legacy_effect(player, card, doublecast_legacy);
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

/* Draconic Disciple	|1|R|G
 * Creature - Human Shaman 2/2
 * |T: Add one mana of any color.
 * |7, |T, Sacrifice ~: Create a 5/5 |Sred Dragon creature token with flying. */

/* ManalinkEh 2022: 0x2207fa0; original cards/m19.c:606. */
int card_dragons_hoard(int player, int card, event_t event)
{
  /* Dragon's Hoard	|3
   * Artifact
   * Whenever a Dragon enters the battlefield under your control, put a gold counter on ~.
   * |T, Remove a gold counter from ~: Draw a card.
   * |T: Add one mana of any color. */

  enum
  {
    CHOICE_DRAW = 1,
    CHOICE_MANA = 2
  };

  int choice;
  int draw_available;
  int paying;
  card_instance_t *instance;
  test_definition_t test;

  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 2);
    test.subtype = 0x30;
    if (new_specific_cip(player, card, event, player, 2, &test) != 0)
    {
      add_counters(player, card, player, card, COUNTER_GOLD, 1);
    }
  }
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      instance = get_card_instance(player, card);

      draw_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0,
                                                 0, 0x1330100, NULL, NULL);
      instance->info_slot = 2;
      paying = paying_mana();
      if ((draw_available != 0) && (paying == 0))
      {
        choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Draw a card",
                             draw_available, 10, "Add mana", 1, 5, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice == CHOICE_DRAW)
        {
          return generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 0, 0,
                                           0x1330100, NULL, NULL);
        }
        if (choice != CHOICE_MANA)
        {
          return 0;
        }
      }
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      instance = get_card_instance(player, card);
      if ((event == EVENT_RESOLVE_ACTIVATION) && (instance->info_slot == 1))
      {
        draw_a_card(player);
      }
      return 0;
    }
  }
  return mana_producer(player, card, event);
}

/* Druid of Horns	|3|G
 * Creature - Human Druid 2/3
 * Whenever you cast an Aura spell that targets ~, create a 3/3 |Sgreen Beast creature token. */

/* Dryad Greenseeker	|1|G
 * Creature - Dryad 1/3
 * |T: Look at the top card of your library. If it's a land card, you may reveal it and put it into your hand. */

/* Dwarven Priest	|3|W
 * Creature - Dwarf Cleric 2/4
 * When ~ enters the battlefield, you gain 1 life for each creature you control. */

/* Dwindle	|2|U
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets -6/-0.
 * When enchanted creature blocks, destroy it. */

/* Elvish Clancaller	|G|G
 * Creature - Elf Druid 1/1
 * Other Elves you control get +1/+1.
 * |4|G|G, |T: Search your library for a card named ~, put it onto the battlefield, then shuffle your library. */

/* ManalinkEh 2022: 0x22082a0; original cards/m19.c:688. */
int card_elvish_rejuvenator(int player, int card, event_t event)
{
  /* Elvish Rejuvenator	|2|G
   * Creature - Elf Druid 1/1
   * When ~ enters the battlefield, look at the top five cards of your library. You may put a land card from among them onto the battlefield tapped. Put the rest on the bottom of your library in a random order. */

  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      new_default_test_definition(&test, 1, "Select a land card.");
      reveal_top_cards_of_library_and_choose(player, card, player, 5, 0, TUTOR_PLAY_TAPPED, 1,
                                             TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
    }
  }
  return 0;
}

/* Epicure of Blood	|4|B
 * Creature - Vampire 4/4
 * Whenever you gain life, each opponent loses 1 life. */

/* ManalinkEh 2022: 0x2208370; original cards/m19.c:707. */
int card_exclusion_mage(int player, int card, event_t event)
{
  /* Exclusion Mage	|2|U
   * Creature - Human Wizard 2/2
   * When ~ enters the battlefield, return target creature an opponent controls to its owner's hand. */

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
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS", 0, 0);
    if (result != 0)
    {
      bounce_permanent(instance->targets[0].player, instance->targets[0].card);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x2208440; original cards/m19.c:731. */
int card_fell_specter(int player, int card, event_t event)
{
  /* Fell Specter	|3|B
   * Creature - Specter 1/3
   * Flying
   * When ~ enters the battlefield, target opponent discards a card.
   * Whenever an opponent discards a card, that player loses 2 life. */

  int result;
  card_instance_t *instance;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if ((event == EVENT_ETB) && (0 < hand_count[1 - player]))
  {
    result = target_opponent(player, card);
    if (result != 0)
    {
      discard(1 - player, 0, player);
      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
    }
  }
  result = card_megrim(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x22084e0; original cards/m19.c:747. */
int card_fiery_finish(int player, int card, event_t event)
{
  /* Fiery Finish	|4|R|R
   * Sorcery
   * ~ deals 7 damage to target creature. */

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
      damage_target0(player, card, 7);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* Fountain of Renewal	|1
 * Artifact
 * At the beginning of your upkeep, you gain 1 life.
 * |3, Sacrifice ~: Draw a card. */

/* ManalinkEh 2022: 0x2208600; original cards/m19.c:774. */
int card_fraying_omnipotence(int player, int card, event_t event)
{
  /* Fraying Omnipotence	|3|B|B
   * Sorcery
   * Each player loses half their life, then discards half the cards in their hand, then sacrifices half the creatures they control. Round up each time. */

  int result;
  unsigned int half_count;
  unsigned int pass;
  unsigned int affected_player;
  test_definition_t test;
  result = current_turn;
  if (event == EVENT_RESOLVE_SPELL)
  {
    pass = 0;
    half_count = round_up_value(life[current_turn]);
    lose_life(result, half_count);
    affected_player = current_turn ^ 1;
    half_count = round_up_value(life[affected_player]);
    lose_life(affected_player, half_count);
    result = current_turn;
    half_count = round_up_value(hand_count[current_turn]);
    new_multidiscard(result, half_count, 0, player);
    affected_player = current_turn ^ 1;
    half_count = round_up_value(hand_count[affected_player]);
    new_multidiscard(affected_player, half_count, 0, player);
    do
    {
      affected_player = current_turn ^ pass;
      pass = pass + 1;
      half_count = count_subtype(affected_player, 2, -1);
      half_count = round_up_value(half_count);
      new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
      test.qty = (unsigned short)half_count;
      new_sacrifice(player, card, affected_player, SAC_CAUSED | SAC_NO_CANCEL, &test);
    } while (pass != 2);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x2208780; original cards/m19.c:799. */
int card_frilled_sea_serpent(int player, int card, event_t event)
{
  /* Frilled Sea Serpent	|4|U|U
   * Creature - Serpent 4/6
   * |5|U|U: ~ can't be blocked this turn. */

  int result;
  result = generic_shade(player, card, event, 0, 5, 0, 2, 0, 0, 0, 0, 0, 0, 0x10);
  return result;
}

/* Gallant Cavalry	|3|W
 * Creature - Human Knight 2/2
 * Vigilance
 * When ~ enters the battlefield, create a 2/2 |Swhite Knight creature token with vigilance. */

/* Gearsmith Guardian	|5
 * Artifact Creature - Construct 3/5
 * ~ gets +2/+0 as long as you control a |Sblue creature. */

/* Gearsmith Prodigy	|U
 * Creature - Human Artificer 1/2
 * ~ gets +1/+0 as long as you control an artifact. */

/* Ghastbark Twins	|5|G|G
 * Creature - Treefolk 7/7
 * Trample
 * ~ can block an additional creature each combat. */

/* Gigantosaurus	|G|G|G|G|G
 * Creature - Dinosaur 10/10 */

/* ManalinkEh 2022: 0x2208800; original cards/m19.c:828. */
int card_goblin_instigator(int player, int card, event_t event)
{
  /* Goblin Instigator	|1|R
   * Creature - Goblin Rogue 1/1
   * When ~ enters the battlefield, create a 1/1 |Sred Goblin creature token. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_GOBLIN);
  return 0;
}

/* Goblin Motivator	|R
 * Creature - Goblin Warrior 1/1
 * |T: Target creature gains haste until end of turn. */

/* ManalinkEh 2022: 0x2208860; original cards/m19.c:843. */
int card_goblin_trashmaster(int player, int card, event_t event)
{
  /* Goblin Trashmaster	|2|R|R
   * Creature - Goblin Warrior 3/3
   * Other Goblins you control get +1/+1.
   * Sacrifice a Goblin: Destroy target artifact. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  test_definition_t test;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 0x40);
    get_card_instance(player, card);
    new_default_test_definition(&test, 0x47, "Select a Goblin to sacrifice.");
    test.subtype = 0x4b;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      result =
          boost_creature_type(player, card, event, SUBTYPE_GOBLIN, 1, 1, 0, BCT_CONTROLLER_ONLY);
      return result;
    }
    default_target_definition(player, card, &td, 0x40);
    instance = get_card_instance(player, card);
    new_default_test_definition(&test, 0x47, "Select a Goblin to sacrifice.");
    test.subtype = 0x4b;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
    }
  }
  result = generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_CAN_TARGET, 0, 0, 0, 0, 0, 0, 0, &td, "TARGET_ARTIFACT", TYPE_NONE,
      &test, SARV_0);
  return result;
}

/* ManalinkEh 2022: 0x2208a60; original cards/m19.c:867. */
int card_goreclaw_terror_of_qal_sisma(int player, int card, event_t event)
{
  /* Goreclaw, Terror of Qal Sisma	|3|G
   * Legendary Creature - Bear 4/3
   * Creature spells you cast with power 4 or greater cost |2 less to cast.
   * Whenever ~ attacks, each creature you control with power 4 or greater gets +1/+1 and gains trample until end of turn. */

  card_instance_t *instance;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if (affected_card_controller == player)
    {
      if (!is_humiliated(player, card, AL_STATIC))
      {
        instance = get_card_instance(affected_card_controller, affected_card);
        if (3 < cards_data[instance->internal_card_id].power)
        {
          COST_COLORLESS = COST_COLORLESS + -2;
        }
      }
    }
    dat_ability(player, card, EVENT_MODIFY_COST_GLOBAL, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
    return 0;
  }
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }
  default_test_definition(&this_test, 2);
  this_test.power = 3;
  this_test.power_flag = 2;
  pump_creatures_until_eot(player, card, player, 0, 1, 1, 0x80, 0, &this_test);
  return 0;
}

/* ManalinkEh 2022: 0x2208bd0; original cards/m19.c:894. */
int card_gravewaker(int player, int card, event_t event)
{
  /* Gravewaker	|4|B|B
   * Creature - Bird Spirit 5/5
   * Flying
   * |5|B|B: Return target creature card from your graveyard to the battlefield tapped. */

  int result;
  test_definition_t test;
  if (!IS_GAA_EVENT(event))
    return 0;

  new_default_test_definition(&test, 2, "Select target creature card.");
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = validate_target_from_grave_source(player, card, player, 0), -1 < result))
  {
    reanimate_permanent(player, card, player, result, REANIMATE_TAP);
  }
  result = generic_activated_ability_with_target_in_grave(player, card, event, GAA_NONE, 5, 2, 0, 0,
                                                          0, 0, 0, NULL, NULL, TYPE_NONE, &test, 8);
  return result;
}

/* Graveyard Marshal	|B|B
 * Creature - Zombie Soldier 3/2
 * |2|B, Exile a creature card from your graveyard: Create a tapped 2/2 |Sblack Zombie creature token. */

/* Greenwood Sentinel	|1|G
 * Creature - Elf Scout 2/2
 * Vigilance */

/* Havoc Devils	|2|R|R
 * Creature - Devil 4/3
 * Trample */

/* Herald of Faith	|3|W|W
 * Creature - Angel 4/3
 * Flying
 * Whenever ~ attacks, you gain 2 life. */

/* Heroic Reinforcements	|2|R|W
 * Sorcery
 * Create two 1/1 |Swhite Soldier creature tokens. Until end of turn, creatures you control get +1/+1 and gain haste. */

/* Hieromancer's Cage	|3|W
 * Enchantment
 * When ~ enters the battlefield, exile target nonland permanent an opponent controls until ~ leaves the battlefield. */

/* Hired Blade	|2|B
 * Creature - Human Assassin 3/2
 * Flash */

/* Hostile Minotaur	|3|R
 * Creature - Minotaur 3/3
 * Haste */

/* ManalinkEh 2022: 0x2208d20; original cards/m19.c:951. */
int card_hungering_hydra(int player, int card, event_t event)
{
  /* Hungering Hydra	|X|G
   * Creature - Hydra 0/0
   * ~ enters the battlefield with X +1/+1 counters on it.
   * ~ can't be blocked by more than one creature.
   * Whenever ~ is dealt damage, put that many +1/+1 counters on it. */

  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, x_value);

  if (event == EVENT_DECLARE_BLOCKERS && in_play(player, card) &&
      !is_humiliated(player, card, AL_STATIC))
  {
    int opponent = 1 - player;
    int blocker_count = 0;
    for (int candidate = 0; candidate < active_cards_count[opponent]; ++candidate)
    {
      if (in_play(opponent, candidate))
      {
        card_instance_t *blocker = get_card_instance(opponent, candidate);
        blocker_count += (unsigned int)blocker->blocking == card;
      }
    }
    if (blocker_count > 1)
    {
      for (int candidate = 0; candidate < active_cards_count[opponent]; ++candidate)
      {
        if (in_play(opponent, candidate))
        {
          card_instance_t *blocker = get_card_instance(opponent, candidate);
          if ((unsigned int)blocker->blocking == card)
          {
            blocker->blocking = 0xff;
            blocker->state &= 0xffffbff7;
          }
        }
      }
    }
  }

  if (!is_humiliated(player, card, AL_TRIG))
  {
    card_instance_t *damage = damage_being_dealt(event);
    if (damage && damage->damage_target_card == card && damage->damage_target_player == player)
    {
      card_instance_t *hydra = get_card_instance(player, card);
      int index = hydra->info_slot;
      if (index < 10)
      {
        int new_count = index + 1;
        if (index < 1)
        {
          new_count = 2;
          index = 1;
        }
        hydra->info_slot = new_count;
        hydra->targets[index].player = damage->info_slot;
      }
    }
    if (trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
        reason_for_trigger_controller == player)
    {
      card_instance_t *hydra = get_card_instance(player, card);
      int damage_count = hydra->info_slot;
      if (damage_count > 1)
      {
        if (event == EVENT_TRIGGER)
          event_result |= RESOLVE_TRIGGER_MANDATORY;
        else if (event == EVENT_RESOLVE_TRIGGER)
        {
          int damage_amounts[10];
          for (int index = 1; index < damage_count; ++index)
            damage_amounts[index] = hydra->targets[index].player;
          hydra->info_slot = 1;
          for (int index = 1; index < damage_count; ++index)
          {
            int amount = damage_amounts[index];
            hydra->number_of_targets = 0;
            add_counters(player, card, player, card, COUNTER_P1_P1, amount);
          }
        }
      }
    }
  }
  return 0;
}

/* Infernal Reckoning	|B
 * Instant
 * Exile target colorless creature. You gain life equal to its power. */

/* ManalinkEh 2022: 0x2208fa0; original cards/m19.c:1040. */
int card_inferno_hellion(int player, int card, event_t event)
{
  /* Inferno Hellion	|3|R
   * Creature - Hellion 7/3
   * Trample
   * At the beginning of each end step, if ~ attacked or blocked this turn, its owner shuffles it into their library. */

  card_instance_t *instance;
  if (trigger_condition != TRIGGER_EOT)
  {
    return 0;
  }
  if ((uint32_t)(event - EVENT_TRIGGER) < 2)
  {
    instance = get_card_instance(player, card);
    if ((instance->state & 0x4040) != 0)
    {
      if (eot_trigger(player, card, event))
      {
        shuffle_into_library(player, card);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22059c0; original cards/m19.c:1053. */
int isareth_legacy(int player, int card, event_t event)
{
  int target_card;
  card_instance_t *instance;
  card_instance_t *returned_creature;
  int iid;
  int result;

  instance = get_card_instance(player, card);
  result = instance->targets[0].player;
  target_card = instance->targets[0].card;
  if (instance->targets[1].player < 0)
  {
    returned_creature = in_play(result, target_card);
    if (returned_creature != NULL)
    {
      instance->targets[1].player = 0;
      set_special_flags(result, target_card, 0x4000);
    }
  }
  leaves_play(result, target_card, event, 1, 0);
  if (event != EVENT_LEAVES_PLAY_ABILITY)
  {
    return 0;
  }
  iid = get_original_internal_card_id(result, target_card);
  result = get_owner(result, target_card);
  add_card_to_rfg(result, iid);
  play_sound_effect(WAV_DESTROY);
  return 0;
}

/* ManalinkEh 2022: 0x2209020; original cards/m19.c:1070. */
int card_isareth_the_awakener(int player, int card, event_t event)
{
  /* Isareth the Awakener	|1|B|B
   * Legendary Creature - Human Wizard 3/3
   * Deathtouch
   * Whenever ~ attacks, you may pay |X. When you do, return target creature card with converted mana cost X from your graveyard to the battlefield with a corpse counter on it. If that creature would leave the battlefield, exile it instead of putting it anywhere else. */

  xtrigger_t trigger;
  declare_attackers_trigger_t mode;
  int result;
  int legacy_card;
  test_definition_t test;

  store_attackers(player, card, event, 0, player, card, NULL);
  trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    trigger = xtrigger_impl_value_dont_use_directly;
  }
  if ((event == EVENT_DECLARE_ATTACKERS) || (trigger == XTRIGGER_ATTACKING))
  {

    new_default_test_definition(&test, 2, "");
    result = has_mana(player, 7, 1);
    test.cmc = result + 1;
    test.cmc_flag = 3;
    result = graveyard_has_shroud(player);
    if (result != 0)
    {
      return 0;
    }
    result = new_special_count_grave(player, &test);
    if (result < 1)
    {
      return 0;
    }
    if ((player != 0) || (mode = DAT_RESOLVE_TRIGGER_OPTIONAL, ai_is_speculating == 1))
    {
      result = has_mana(player, 0, 1);
      mode = (unsigned int)(result != 0) * 2;
    }
    dat_ability(player, card, event, mode, 1);
  }
  if (event == EVENT_DAT_ABILITY)
  {

    new_default_test_definition(&test, 2, "");
    result = has_mana(player, 7, 1);
    test.cmc_flag = 3;
    test.cmc = result + 1;
    if (ai_is_speculating != 1)
    {
      result = has_mana(player, 7, 1);
      sprintf(test.message, "Select target creature card with converted mana cost %d or less.",
              result);
    }
    result = select_target_from_grave_source(player, card, player, SFG_0, 1, &test, 0);
    if (result != -1)
    {
      result = get_cmc_by_internal_id(result);
      result =
          charge_mana_while_resolving(player, card, EVENT_DAT_ABILITY, player, COLOR_ANY, result);
      if (result == 0)
      {
        spell_fizzled = 1;
      }
    }
    result = validate_target_from_grave_source(player, card, player, 0);
    if (result != -1)
    {

      result = reanimate_permanent(player, card, player, result, REANIMATE_DEFAULT);
      legacy_card = create_targetted_legacy_effect(player, card, isareth_legacy, player, result);
      add_status(player, legacy_card, 0x10000);
      add_counters(player, card, player, result, COUNTER_CORPSE, 1);
      convert_to_token(player, result);
      set_special_flags(player, result, 0x4000);
      set_special_flags2(player, result, 0x10000000);
    }
  }
  return 0;
}

/* Isolate	|W
 * Instant
 * Exile target permanent with converted mana cost 1. */

/* Kargan Dragonrider	|1|R
 * Creature - Human Warrior 2/2
 * As long as you control a Dragon, ~ has flying. */

/* Knight of the Tusk	|4|W|W
 * Creature - Human Knight 3/7
 * Vigilance */

/* Knight's Pledge	|1|W
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+2. */

/* ManalinkEh 2022: 0x2209330; original cards/m19.c:1144. */
int card_lathliss_dragon_queen(int player, int card, event_t event)
{
  /* Lathliss, Dragon Queen	|4|R|R
   * Legendary Creature - Dragon 6/6
   * Flying
   * Whenever another nontoken Dragon enters the battlefield under your control, create a 5/5 |Sred Dragon creature token with flying.
   * |1|R: Dragons you control get +1/+0 until end of turn. */

  check_legend_rule(player, card, event);
  if ((((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (is_humiliated(player, card, AL_TRIG) == 0))
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.subtype = 0x30;
    test.not_me = 1;
    test.type_flag = 3;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_DRAGON, &token);
    token.tou = 5;
    token.pow = 5;
    token.color_forced = '\x10';
    generate_token(&token);
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_CLEANUP)
    return generic_activated_ability(player, card, event, 0, 1, 0, 0, 0, 1, 0, 0, NULL, NULL);
  if (event != EVENT_RESOLVE_ACTIVATION)
    return 0;
  pump_subtype_until_eot(player, card, player, 0x30, 1, 0, 0, 0);
  return generic_activated_ability(player, card, event, 0, 1, 0, 0, 0, 1, 0, 0, NULL, NULL);
}

/* Lena, Selfless Champion	|4|W|W
 * Legendary Creature - Human Knight 3/3
 * When ~ enters the battlefield, create a 1/1 |Swhite Soldier creature token for each nontoken creature you control.
 * Sacrifice ~: Creatures you control with power less than ~'s power gain indestructible until end of turn. */

/* Leonin Vanguard	|W
 * Creature - Cat Soldier 1/1
 * At the beginning of combat on your turn, if you control three or more creatures, ~ gets +1/+1 until end of turn and you gain 1 life. */

/* ManalinkEh 2022: 0x2209530; original cards/m19.c:1187. */
int card_leonin_warleader(int player, int card, event_t event)
{
  /* Leonin Warleader	|2|W|W
   * Creature - Cat Soldier 4/4
   * Whenever ~ attacks, create two 1/1 |Swhite Cat creature tokens with lifelink that are tapped and attacking. */

  token_generation_t token;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }
  default_token_definition(player, card, CARD_ID_CAT, &token);
  token.tou = 1;
  token.pow = 1;
  token.color_forced = ' ';
  token.qty = '\x02';
  token.s_key_plus = 2;
  token.action = TOKEN_ACTION_ATTACKING;
  generate_token(&token);
  return 0;
}

/* Lich's Caress	|3|B|B
 * Sorcery
 * Destroy target creature. You gain 3 life. */

/* ManalinkEh 2022: 0x22095d0; original cards/m19.c:1211. */
int card_lightning_mare(int player, int card, event_t event)
{
  /* Lightning Mare	|R|R
   * Creature - Elemental Horse 3/1
   * This spell can't be countered.
   * ~ can't be blocked by |Sblue creatures.
   * |1|R: ~ gets +1/+0 until end of turn. */

  int cost_increment;
  int mana_available;
  unsigned int blocker_color;
  unsigned int protected_color;
  int cless;
  int pump_count;

  cannot_be_countered(player, card, event);
  if (event == EVENT_BLOCK_LEGALITY)
  {
    if ((attacking_card_controller == player) && (attacking_card == card))
    {
      blocker_color = get_color(affected_card_controller, affected_card);
      protected_color = get_sleighted_color_test(player, card, 4);
      if ((blocker_color & protected_color) != 0)
      {
        event_result = 1;
      }
    }
  }
  else if (event == EVENT_POW_BOOST)
  {
    if (has_mana_for_activated_ability(player, card, 1, 0, 0, 0, 1, 0) == 0)
    {
      return 0;
    }
    cost_increment = get_cost_mod_for_activated_abilities(player, card, 1, 0, 0, 0, 0, 0);
    pump_count = 0;
    cless = 0;
    while (true)
    {
      mana_available = has_mana_for_activated_ability(player, card, cless, 0, 0, 0, 1, 0);
      cless = cless + cost_increment;
      if (mana_available == 0)
        break;
      pump_count = pump_count + 1;
    }
    return pump_count;
  }
  return generic_shade_merge_pt(player, card, event, 0, 1, 0, 0, 0, 1, 0, 1, 0);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2205870; original cards/m19.c:1238. */
static int test_not_in_array4(int iid, int arrptr, int player, int card)
{
  const int *arr = (const int *)arrptr;
  return arr[0] != iid && arr[1] != iid && arr[2] != iid && arr[3] != iid;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22097a0; original cards/m19.c:1242. */
int card_lilianas_contract(int player, int card, event_t event)
{
  /* Liliana's Contract	|3|B|B
   * Enchantment
   * When ~ enters the battlefield, you draw four cards and you lose 4 life.
   * At the beginning of your upkeep, if you control four or more Demons with different names, you win the game. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_ETB)
  {
    draw_cards(player, 4);
    lose_life(player, 4);
  }
  else if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    int hell[4] = {-1, -1, -1, -1};
    test_definition_t test;
    default_test_definition(&test, 2);
    int first_player = current_turn;
    test.value_for_special_selection_function = (int)hell;
    test.subtype = 0x2c;
    test.special_selection_function = test_not_in_array4;
    int demon_count = 0;
    for (int index = active_cards_count[current_turn] - 1; index >= 0; --index)
    {
      if (player == first_player && in_play(player, index) &&
          new_make_test_in_play(player, index, -1, &test))
      {
        card_instance_t *instance = get_card_instance(player, index);
        hell[demon_count++] = instance->internal_card_id;
      }
    }
    int second_player = 1 - current_turn;
    for (int index = active_cards_count[second_player] - 1; index >= 0; --index)
    {
      if (second_player == player && in_play(second_player, index) &&
          new_make_test_in_play(second_player, index, -1, &test))
      {
        card_instance_t *instance = get_card_instance(second_player, index);
        hell[demon_count++] = instance->internal_card_id;
      }
    }
    if (demon_count > 3)
      lose_the_game(1 - player);
  }
  return global_enchantment(player, card, event);
}
#endif

/* ManalinkEh 2022: 0x2206280; original cards/m19.c:1283. */
static int liliana_mill_legacy(int player, int card, event_t event)
{
  bool zombie_milled;
  xtrigger_t trigger;
  card_instance_t *instance;
  int milled_index;

  enable_xtrigger_flags = enable_xtrigger_flags | ENABLE_XTRIGGER_MILLED;
  trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    trigger = xtrigger_impl_value_dont_use_directly;
  }
  if (trigger != XTRIGGER_MILLED)
  {
    return 0;
  }
  if (((card == affected_card) && (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    instance = get_card_instance(player, card);
    if (instance->targets[0].player == trigger_cause_controller)
    {
      if (event == EVENT_TRIGGER)
      {
        event_result = event_result | 2;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {

        zombie_milled = false;
        milled_index = 0;
        if (0 < num_cards_milled)
        {
          do
          {
            if (cards_milled[milled_index].source != -1)
            {
              if (has_subtype_by_id(
                      player,
                      (unsigned int)cards_data[cards_milled[milled_index].internal_card_id].id,
                      SUBTYPE_ZOMBIE) != 0)
              {
                zombie_milled = true;
              }
            }
            milled_index = milled_index + 1;
          } while (milled_index < num_cards_milled);
          if (zombie_milled)
          {
            lose_life(1 - player, 2);
            gain_life(player, 2);
          }
        }
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22063c0; original cards/m19.c:1308. */
static int liliana_reanimation_legacy(int player, int card, event_t event)
{
  if (!in_play(player, card) || is_humiliated(player, card, AL_STATIC))
    return 0;
  if (event == EVENT_CAN_ACTIVATE)
  {
    int iid = *graveyard_ptr_mutable[player];
    int index = 0;
    while (iid != -1)
    {
      int *grave;
      if (!has_subtype_by_id(player, (unsigned int)cards_data[iid].id, SUBTYPE_ZOMBIE))
        grave = graveyard_ptr_mutable[player];
      else
      {
        int creature = is_what(-1, graveyard_ptr_mutable[player][index], 2);
        grave = graveyard_ptr_mutable[player];
        if (creature)
        {
          int legal = can_legally_play_iid(player, grave[index]);
          grave = graveyard_ptr_mutable[player];
          if (legal)
          {
            if (!has_mana_to_cast_iid(player, EVENT_CAN_ACTIVATE, grave[index]))
              grave = graveyard_ptr_mutable[player];
            else
            {
              if (legal == 99)
                return 99;
              int sorcery_ok = can_sorcery_be_played(player, EVENT_CAN_ACTIVATE);
              if (sorcery_ok || (grave = graveyard_ptr_mutable[player],
                                 (cards_data[grave[index]].type & 0x30) != 0))
                return 1;
            }
          }
        }
      }
      iid = grave[++index];
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    int grave_count = count_graveyard(player);
    int playable[2][grave_count];
    int count = 0;
    int index = 0;
    int *grave = graveyard_ptr_mutable[player];
    int iid = *grave;
    while (iid != -1)
    {
      bool eligible = has_subtype_by_id(player, cards_data[iid].id, SUBTYPE_ZOMBIE) != 0;
      if (!eligible)
        grave = graveyard_ptr_mutable[player];
      else
      {
        iid = graveyard_ptr_mutable[player][index];
        eligible = is_what(-1, iid, TYPE_CREATURE) != 0;
        grave = graveyard_ptr_mutable[player];
      }
      if (eligible)
      {
        eligible = can_legally_play_iid(player, grave[index]) != 0;
        grave = graveyard_ptr_mutable[player];
      }
      if (eligible)
      {
        eligible = has_mana_to_cast_iid(player, EVENT_ACTIVATE, grave[index]) != 0;
        if (!eligible)
          grave = graveyard_ptr_mutable[player];
      }
      if (eligible)
      {
        int sorcery_ok = can_sorcery_be_played(player, EVENT_ACTIVATE);
        grave = graveyard_ptr_mutable[player];
        iid = grave[index];
        if (sorcery_ok || (cards_data[iid].type & 0x30))
        {
          playable[0][count] = iid;
          playable[1][count] = index;
          ++count;
        }
      }
      iid = grave[index + 1];
      ++index;
    }
    test_definition_t test;
    new_default_test_definition(&test, 0xff, "Select a Zombie card to play.");
    int selected = select_card_from_zone(player, player, playable[0], count, 0, 1, -1, &test);
    if (selected != -1 &&
        charge_mana_from_id(player, -1, EVENT_ACTIVATE, cards_data[playable[0][selected]].id))
    {
      // Keep the existing list-index argument, even though grave positions are stored above.
      play_card_in_grave_for_free(player, selected, 1);
      cant_be_responded_to = 1;
      return 0;
    }
    spell_fizzled = 1;
    return 0;
  }
  else if (event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);
  return 0;
}
#endif

/* Liliana's Spoils	|3|B
 * Sorcery
 * Target opponent discards a card.
 * Look at the top five cards of your library. You may reveal a |Sblack card from among them and put it into your hand. Put the rest on the bottom of your library in a random order. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2209a10; original cards/m19.c:1383. */
int card_liliana_untouched_by_death(int player, int card, event_t event)
{
  /* Liliana, Untouched by Death	|2|B|B
   * Legendary Planeswalker - Liliana (4)
   * +1: Put the top three cards of your library into your graveyard. If at least one of them is a Zombie card, each opponent loses 2 life and you gain 2 life.
   * -2: Target creature gets -X/-X until end of turn, where X is the number of Zombies you control.
   * -3: You may cast Zombie cards from your graveyard this turn. */

  enum
  {
    CHOICE_MILL = 1,
    CHOICE_DEBUFF = 2,
    CHOICE_GRAVEYARD = 3
  };

  int choice;
  int zombies_or_mode;
  int grave_zombies;
  card_instance_t *legacy;
  target_definition_t *creature_target;
  card_instance_t *instance;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 1 - player;
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      zombies_or_mode = count_subtype(player, 0x47, 0xde);
      grave_zombies = count_graveyard_by_subtype(player, SUBTYPE_ZOMBIE);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Mill yourself", 1,
                           5, 1, "Target creature gets -X/-X", can_target(&td), zombies_or_mode * 5,
                           0xfffffffe, "Cast Zombies from your graveyard", 1, grave_zombies * 5,
                           0xfffffffd, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_DEBUFF)
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
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = 1 - player;
  }
  creature_target = &td;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    zombies_or_mode = instance->info_slot;
    if (zombies_or_mode == CHOICE_MILL)
    {
      legacy = get_card_instance(player, create_legacy_effect(player, card, liliana_mill_legacy));
      legacy->targets[0].player = instance->parent_controller;
      mill(player, 3);
    }
    else if (zombies_or_mode == CHOICE_DEBUFF)
    {
      if (valid_target(creature_target) != 0)
      {
        zombies_or_mode = count_subtype(player, -1, 0xde);
        pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                      instance->targets[0].player, instance->targets[0].card,
                                      -zombies_or_mode, -zombies_or_mode);
      }
    }
    else if (zombies_or_mode == CHOICE_GRAVEYARD)
    {
      create_legacy_activate(player, card, liliana_reanimation_legacy);
    }
  }
  return planeswalker(player, card, event, 4);
}
#endif

/* Liliana, the Necromancer	|3|B|B
 * Legendary Planeswalker - Liliana (4)
 * +1: Target player loses 2 life.
 * -1: Return target creature card from your graveyard to your hand.
 * -7: Destroy up to two target creatures. Put up to two creature cards from graveyards onto the battlefield under your control. */

/* Loxodon Line Breaker	|2|W
 * Creature - Elephant Soldier 3/2 */

/* Marauder's Axe	|2
 * Artifact - Equipment
 * Equipped creature gets +2/+0.
 * Equip |2 */

/* Metamorphic Alteration	|1|U
 * Enchantment - Aura
 * Enchant creature
 * As ~ enters the battlefield, choose a creature.
 * Enchanted creature is a copy of the chosen creature. */

/* ManalinkEh 2022: 0x2209d10; original cards/m19.c:1473. */
int card_meteor_golem(int player, int card, event_t event)
{
  /* Meteor Golem	|7
   * Artifact Creature - Golem 3/3
   * When ~ enters the battlefield, destroy target nonland permanent an opponent controls. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.allowed_controller = 1 - player;
  td.illegal_type = TYPE_LAND;
  td.allow_cancel = 0;
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "Select target nonland permanent an opponent controls.", 0, 0x81);
    if (result != 0)
    {
      action_on_target(player, card, 0, ACT_KILL_DESTROY);
    }
  }
  instance = get_card_instance(player, card);
  instance->number_of_targets = '\0';
  return 0;
}

/* Militia Bugler	|2|W
 * Creature - Human Soldier 2/3
 * Vigilance
 * When ~ enters the battlefield, look at the top four cards of your library. You may reveal a creature card with power 2 or less from among them and put it into your hand. Put the rest on the bottom of your library in a random order. */

/* ManalinkEh 2022: 0x2209e00; original cards/m19.c:1500. */
int card_mirror_image(int player, int card, event_t event)
{
  /* Mirror Image	|2|U
   * Creature - Shapeshifter 0/0
   * You may have ~ enter the battlefield as a copy of any creature you control. */

  int result;
  target_definition_t td;
  if (event == EVENT_RESOLVE_SPELL)
  {
    base_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
  }
  result = enters_the_battlefield_as_copy_of_any(player, card, event, &td,
                                                 "SELECT_A_CREATURE_YOU_CONTROL");
  return result;
}

/* Mistcaller	|U
 * Creature - Merfolk Wizard 1/1
 * Sacrifice ~: Until end of turn, if a nontoken creature would enter the battlefield and it wasn't cast, exile it instead. */

/* ManalinkEh 2022: 0x2209e60; original cards/m19.c:1516. */
int card_mystic_archaeologist(int player, int card, event_t event)
{
  /* Mystic Archaeologist	|1|U
   * Creature - Human Wizard 2/1
   * |3|U|U: Draw two cards. */

  int result;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_cards(player, 2);
  }
  result = generic_activated_ability(player, card, event, 0, 3, 0, 2, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* Nexus of Fate	|5|U|U
 * Instant
 * Take an extra turn after this one.
 * If ~ would be put into a graveyard from anywhere, reveal ~ and shuffle it into its owner's library instead. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2209f00; original cards/m19.c:1533. */
int card_nicol_bolas_the_ravager(int player, int card, event_t event)
{
  /* Nicol Bolas, the Ravager	|1|U|B|R
   * Legendary Creature - Elder Dragon 4/4
   * Flying
   * When ~ enters the battlefield, each opponent discards a card.
   * |4|U|B|R: Exile ~, then return him to the battlefield transformed under his owner's control. Activate this ability only any time you could cast a sorcery. */

  card_instance_t *instance;
  int result;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    discard(1 - player, 0, player);
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_CLEANUP)
    return generic_activated_ability(player, card, event, 0x10, 4, 1, 1, 0, 1, 0, 0, NULL, NULL);
  if (event != EVENT_RESOLVE_ACTIVATION)
    return 0;
  instance = get_card_instance(player, card);
  result = instance->parent_card;
  instance = get_card_instance(player, card);
  exile_and_return_to_bf_transformed(instance->parent_controller, result);
  return generic_activated_ability(player, card, event, 0x10, 4, 1, 1, 0, 1, 0, 0, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x220a050; original cards/m19.c:1555. */
int card_nicol_bolas_the_arisen(int player, int card, event_t event)
{
  /* Nicol Bolas, the Arisen	""
   * Legendary Planeswalker - Bolas (7)
   * +2: Draw two cards.
   * -3: ~ deals 10 damage to target creature or planeswalker.
   * -4: Put target creature or planeswalker card from a graveyard onto the battlefield under your control.
   * -12: Exile all but the bottom card of target player's library. */

  enum
  {
    CHOICE_DRAW = 1,
    CHOICE_DAMAGE = 2,
    CHOICE_REANIMATE = 3,
    CHOICE_ULTIMATE = 4
  };

  int choice;
  card_instance_t *instance;
  int available_or_mode;
  int player_available;
  target_definition_t *permanent_target;
  target_definition_t *player_target;
  target_definition_t td;
  target_definition_t td_player;
  test_definition_t test;

  double_faced_card(player, card, event);
  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x102);
    new_default_test_definition(&test, 0x102, "Select a creature or planeswalker card");
    default_target_definition(player, card, &td_player, 2);
    td_player.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      available_or_mode = generic_activated_ability_with_target_in_grave(
          player, card, EVENT_CAN_ACTIVATE, GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE,
          &test, 0x1000);
      player_available = can_target(&td_player);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Draw 2 cards", 1,
                           3, 2, "Deal 10 damage", can_target(&td), 5, 0xfffffffd,
                           "Resurrect a Creature or PW", available_or_mode, 5, 0xfffffffc,
                           "Exile a Library", player_available, 10, 0xfffffff4, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_DAMAGE)
      {
        new_pick_target(&td, "TARGET_CREATURE_OR_PLANESWALKER", 0, 1);
      }
      else if (choice == CHOICE_REANIMATE)
      {
        generic_activated_ability_with_target_in_grave(player, card, EVENT_ACTIVATE, GAA_NONE, 0, 0,
                                                       0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test,
                                                       0x1000);
      }
      else if (choice == CHOICE_ULTIMATE)
      {
        new_pick_target(&td_player, "TARGET_PLAYER", 0, 1);
      }
      return planeswalker(player, card, event, 7);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 7);
    default_target_definition(player, card, &td, 0x102);
    new_default_test_definition(&test, 0x102, "Select a creature or planeswalker card");
    default_target_definition(player, card, &td_player, 2);
    td_player.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
  }
  player_target = &td_player;
  permanent_target = &td;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    available_or_mode = instance->info_slot;
    if (available_or_mode == CHOICE_DRAW)
    {
      draw_cards(player, 2);
    }
    else if (available_or_mode == CHOICE_DAMAGE)
    {
      if (valid_target(permanent_target) != 0)
      {
        damage_target0(player, card, 10);
      }
    }
    else if (available_or_mode == CHOICE_REANIMATE)
    {

      available_or_mode =
          validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
      if (available_or_mode != -1)
      {
        reanimate_permanent(player, card, instance->targets[0].player, available_or_mode,
                            REANIMATE_DEFAULT);
      }
    }
    else if ((available_or_mode == CHOICE_ULTIMATE) &&
             (available_or_mode = validate_target(player, card, player_target, 0),
              available_or_mode != 0))
    {

      available_or_mode = count_deck(instance->targets[0].player);
      rfg_top_n_cards_of_deck(instance->targets[0].player, available_or_mode + -1);
    }
  }
  return planeswalker(player, card, event, 7);
}

/* Nightmare's Thirst	|B
 * Instant
 * You gain 1 life. Target creature gets -X/-X until end of turn, where X is the amount of life you gained this turn. */

/* Novice Knight	|W
 * Creature - Human Knight 2/3
 * Defender
 * As long as ~ is enchanted or equipped, it can attack as though it didn't have defender. */

/* Onakke Ogre	|2|R
 * Creature - Ogre Warrior 4/2 */

/* ManalinkEh 2022: 0x220a580; original cards/m19.c:1651. */
int card_one_with_the_machine(int player, int card, event_t event)
{
  /* One with the Machine	|3|U
   * Sorcery
   * Draw cards equal to the highest converted mana cost among artifacts you control. */

  int result;
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = get_highest_cmc(player, TYPE_ARTIFACT);
      ai_modifier = ai_modifier + result * 0xc;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    result = get_highest_cmc(player, TYPE_ARTIFACT);
    draw_cards(player, result);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x2205a90; original cards/m19.c:1667. */
static int open_the_graves_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  int pending_or_result;
  card_instance_t *dying;
  token_generation_t token;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (pending_or_result =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           pending_or_result != 0x4a1f))
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
        }
        count_for_gfp_ability_and_store_values(player, card, 0x77, player, 2, NULL, 0x40, 0);
      }
    }
  }
  pending_or_result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_or_result != 0)
  {
    if (check_status(player, card, 0x10000) == 0)
    {

      default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
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

/* ManalinkEh 2022: 0x220a630; original cards/m19.c:1718. */
int card_open_the_graves(int player, int card, event_t event)
{
  /* Open the Graves	|3|B|B
   * Enchantment
   * Whenever a nontoken creature you control dies, create a 2/2 |Sblack Zombie creature token. */

  int c;
  int created_or_pending;
  card_instance_t *instance;
  token_generation_t token;

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
              instance->info_slot_fn == open_the_graves_effect)) &&
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
          create_targetted_legacy_effect(player, card, open_the_graves_effect, player, card);
      instance = get_card_instance(player, created_or_pending);
      instance->targets[0].player = player;
      instance->targets[0].card = card;
      instance->number_of_targets = '\x01';
      add_status(player, created_or_pending, 0x10000);
    }
  }
  count_for_gfp_ability_and_store_values(player, card, event, player, 2, NULL, 0x40, 0);
  created_or_pending = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if ((created_or_pending != 0) &&
      (instance = get_card_instance(player, card), instance->kill_code == '\0'))
  {

    default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
    token.qty = (unsigned char)created_or_pending;
    generate_token(&token);
  }
  return global_enchantment(player, card, event);
}

/* Ox	""(token)
 * Creature - Ox 2/4 */

/* ManalinkEh 2022: 0x2205d60; original cards/m19.c:1767. */
int palladia_mors_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  special_abilities((int)instance->damage_target_player, instance->damage_target_card, event,
                    (SP_KEYWORD_HEXPROOF), player, card);
  result = damage_dealt_by_me_arbitrary(instance->targets[0].player, instance->targets[0].card,
                                        event, DDBM_0, player, card);
  if (result == 0)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x220a810; original cards/m19.c:1774. */
int card_palladia_mors_the_ruiner(int player, int card, event_t event)
{
  /* Palladia-Mors, the Ruiner	|3|R|G|W
   * Legendary Creature - Elder Dragon 6/6
   * Flying, vigilance, trample
   * ~ has hexproof if it hasn't dealt damage yet. */

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    create_targetted_legacy_effect_no_repeat(player, card, palladia_mors_legacy, player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2206160; original cards/m19.c:1789. */
static int patient_rebuilding_legacy(int player, int card, event_t event)
{
  xtrigger_t trigger;
  card_instance_t *instance;
  int milled_index;
  int amount;
  enable_xtrigger_flags = enable_xtrigger_flags | ENABLE_XTRIGGER_MILLED;
  trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    trigger = xtrigger_impl_value_dont_use_directly;
  }
  if (trigger != XTRIGGER_MILLED)
  {
    return 0;
  }
  if (((card == affected_card) && (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    instance = get_card_instance(player, card);
    if (instance->targets[0].player == trigger_cause_controller)
    {
      if (event == EVENT_TRIGGER)
      {
        event_result = event_result | 2;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {
        amount = 0;
        for (milled_index = 0; milled_index < num_cards_milled; ++milled_index)
          if (cards_milled[milled_index].source != -1 &&
              is_what(-1, cards_milled[milled_index].internal_card_id, TYPE_LAND))
            ++amount;
        if (amount != 0)
          draw_cards(player, amount);
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x220a890; original cards/m19.c:1814. */
int card_patient_rebuilding(int player, int card, event_t event)
{
  /* Patient Rebuilding	|3|U|U
   * Enchantment
   * At the beginning of your upkeep, target opponent puts the top three cards of their library into their graveyard, then you draw a card for each land card put into that graveyard this way. */

  int result;
  card_instance_t *instance;
  card_instance_t *legacy;
  target_definition_t td;
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    td.zone = TARGET_ZONE_PLAYERS;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_OPPONENT", 0, 1);
      if (result != 0)
      {
        instance = get_card_instance(player, card);
        result = create_legacy_effect(player, card, patient_rebuilding_legacy);
        legacy = get_card_instance(player, result);
        result = instance->targets[0].player;
        legacy->targets[0].player = result;
        mill(result, 3);
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x220a990; original cards/m19.c:1835. */
int card_plague_mare(int player, int card, event_t event)
{
  /* Plague Mare	|1|B|B
   * Creature - Nightmare Horse 2/2
   * ~ can't be blocked by |Swhite creatures.
   * When ~ enters the battlefield, creatures your opponents control get -1/-1 until end of turn. */

  unsigned int blocker_color;
  unsigned int protected_color;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    if (((event == EVENT_BLOCK_LEGALITY) && (attacking_card_controller == player)) &&
        (attacking_card == card))
    {
      blocker_color = get_color(affected_card_controller, affected_card);
      protected_color = get_sleighted_color_test(player, card, 0x20);
      if ((blocker_color & protected_color) != 0)
      {
        event_result = 1;
      }
    }
    return 0;
  }
  pump_subtype_until_eot(player, card, 1 - player, -1, -1, -1, 0, 0);
  return 0;
}

/* ManalinkEh 2022: 0x2205e00; original cards/m19.c:1853. */
static int pta_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  card_instance_t *dying;
  unsigned int pending_deaths;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (get_id((int)instance->damage_target_player, instance->damage_target_card) != 0x4a24))
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
        count_for_gfp_ability(player, card, 0x77, 2, 2, NULL);
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

/* ManalinkEh 2022: 0x220aa80; original cards/m19.c:1908. */
int card_poison_tip_archer(int player, int card, event_t event)
{
  /* Poison-Tip Archer	|2|B|G
   * Creature - Elf Archer 2/3
   * Reach
   * Deathtouch
   * Whenever another creature dies, each opponent loses 1 life. */

  int c;
  card_instance_t *instance;
  unsigned int pending_deaths;
  int created_card;

  deathtouch(player, card, event);
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
               (instance = get_card_instance(player, c), instance->info_slot_fn == pta_effect)) &&
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
        created_card = create_targetted_legacy_effect(player, card, pta_effect, player, card);
        instance = get_card_instance(player, created_card);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, created_card, 0x10000);
      }
    }
  }
  else if ((event == EVENT_GRAVEYARD_FROM_PLAY) &&
           ((card != affected_card || (player != affected_card_controller))))
  {
    count_for_gfp_ability(player, card, 0x77, 2, 2, NULL);
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

/* Prodigious Growth	|4|G|G
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +7/+7 and has trample. */

/* ManalinkEh 2022: 0x220ac80; original cards/m19.c:1965. */
int card_psychic_corrosion(int player, int card, event_t event)
{
  /* Psychic Corrosion	|2|U
   * Enchantment
   * Whenever you draw a card, each opponent puts the top two cards of their library into their graveyard. */

  int result;
  result = card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    mill(1 - player, 2);
  }
  return global_enchantment(player, card, event);
}

/* Psychic Symbiont	|4|U|B
 * Creature - Nightmare Horror 3/3
 * Flying
 * When ~ enters the battlefield, target opponent discards a card and you draw a card. */

/* Ravenous Harpy	|2|B
 * Creature - Harpy 1/2
 * Flying
 * |1, Sacrifice another creature: Put a +1/+1 counter on ~. */

/* ManalinkEh 2022: 0x220acf0; original cards/m19.c:1984. */
int card_regal_bloodlord(int player, int card, event_t event)
{
  /* Regal Bloodlord	|3|W|B
   * Creature - Vampire Soldier 2/4
   * Flying
   * At the beginning of each end step, if you gained life this turn, create a 1/1 |Sblack Bat creature token with flying. */

  int result;
  token_generation_t token;
  result = get_trap_condition(player, 1);
  if (result != 0)
  {
    result = eot_trigger(player, card, event);
    if (result != 0)
    {
      default_token_definition(player, card, CARD_ID_BAT, &token);
      token.tou = 1;
      token.pow = 1;
      generate_token(&token);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x220ad70; original cards/m19.c:1999. */
int card_remorseful_cleric(int player, int card, event_t event)
{
  /* Remorseful Cleric	|1|W
   * Creature - Spirit Cleric 2/1
   * Flying
   * Sacrifice ~: Exile all cards from target player's graveyard. */

  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  card_instance_t *instance = get_card_instance(player, card);
  int result = generic_activated_ability(player, card, event, 0xc, 0, 0, 0, 0, 0, 0, 0, &td,
                                         "TARGET_PLAYER");
  if (event == EVENT_RESOLVE_ACTIVATION)
    rfg_whole_graveyard(instance->targets[0].player);
  else if (event == EVENT_ACTIVATE &&
           (ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0)))
  {
    int target_player = instance->targets[0].player;
    int relevant_cards = count_graveyard_by_type(target_player, 0x4a);
    int score = count_graveyard(target_player);
    score += relevant_cards * 2;
    score -= target_player == player ? 0x9e : 0x1e;
    if (player != 1)
      ai_modifier -= score;
    else
      ai_modifier += score;
  }
  return result;
}

/* ManalinkEh 2022: 0x220afd0; original cards/m19.c:2032. */
int card_resplendent_angel(int player, int card, event_t event)
{
  /* Resplendent Angel	|1|W|W
   * Creature - Angel 3/3
   * Flying
   * At the beginning of each end step, if you gained 5 or more life this turn, create a 4/4 |Swhite Angel creature token with flying and vigilance.
   * |3|W|W|W: Until end of turn, ~ gets +2/+2 and gains lifelink. */

  int result;
  card_instance_t *instance;
  token_generation_t token;
  result = get_trap_condition(player, 1);
  if ((4 < result) && (result = eot_trigger(player, card, event), result != 0))
  {
    default_token_definition(player, card, CARD_ID_ANGEL, &token);
    token.s_key_plus = 8;
    generate_token(&token);
  }
  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      result = instance->parent_card;
      instance = get_card_instance(player, card);
      pump_ability_until_eot(player, card, instance->parent_controller, result, 2, 2, 0, 2);
    }
  }
  result = generic_activated_ability(player, card, event, 0, 3, 0, 0, 0, 0, 3, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x220b140; original cards/m19.c:2053. */
int card_revitalize(int player, int card, event_t event)
{
  /* Revitalize	|1|W
   * Instant
   * You gain 3 life.
   * Draw a card. */

  int result;
  if (event == EVENT_RESOLVE_SPELL)
  {
    gain_life(player, 3);
    draw_a_card(player);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* Rhox Oracle	|4|G
 * Creature - Rhino Monk 4/2
 * When ~ enters the battlefield, draw a card. */

/* Riddlemaster Sphinx	|4|U|U
 * Creature - Sphinx 5/5
 * Flying
 * When ~ enters the battlefield, you may return target creature an opponent controls to its owner's hand. */

/* Root Snare	|1|G
 * Instant
 * Prevent all combat damage that would be dealt this turn. */

/* ManalinkEh 2022: 0x220b1a0; original cards/m19.c:2079. */
int card_runic_armasaur(int player, int card, event_t event)
{
  /* Runic Armasaur	|1|G|G
   * Creature - Dinosaur 2/5
   * Whenever an opponent activates an ability of a creature or land that isn't a mana ability, you may draw a card. */

  card_instance_t *instance;

  if (((event == EVENT_PLAY_ABILITY) || (event == EVENT_TAPPED_TO_PLAY_ABILITY)) &&
      (1 - player == affected_card_controller))
  {
    if (is_what(1 - player, affected_card, 3))
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
            draw_some_cards_if_you_want(player, card, player, 1);
          }
        }
      }
    }
  }
  return 0;
}

/* Rustwing Falcon	|W
 * Creature - Bird 1/2
 * Flying */

/* ManalinkEh 2022: 0x220b280; original cards/m19.c:2101. */
int card_sai_master_thopterist(int player, int card, event_t event)
{
  /* Sai, Master Thopterist	|2|U
   * Legendary Creature - Human Artificer 1/4
   * Whenever you cast an artifact spell, create a 1/1 colorless Thopter artifact creature token with flying.
   * |1|U, Sacrifice two artifacts: Draw a card. */

  check_legend_rule(player, card, event);
  sp_ability_trigger(player, event, 2, TYPE_ARTIFACT, NULL, player, card);
  if (event == EVENT_SPA)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_THOPTER, &token);
    token.tou = 1;
    token.pow = 1;
    token.key_plus = KEYWORD_FLYING;
    generate_token(&token);
  }
  else if (IS_GAA_EVENT(event))
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_ARTIFACT);
    test.qty = 2;
    if (event == EVENT_RESOLVE_ACTIVATION)
      draw_a_card(player);
    return generic_activated_ability_with_sacrifice_as_cost(
        player, card, event, GAA_NONE, 1, 0, 1, 0, 0, 0, 0, NULL, "SACRIFICE_ARTIFACT", TYPE_NONE,
        &test, SARV_0);
  }
  return 0;
}

/* Salvager of Secrets	|3|U|U
 * Creature - Merfolk Wizard 2/2
 * When ~ enters the battlefield, return target instant or sorcery card from your graveyard to your hand. */

/* Sarkhan's Dragonfire	|3|R|R
 * Sorcery
 * ~ deals 3 damage to any target.
 * Look at the top five cards of your library. You may reveal a |Sred card from among them and put it into your hand. Put the rest on the bottom of your library in a random order. */

/* ManalinkEh 2022: 0x220b420; original cards/m19.c:2140. */
int card_sarkhans_unsealing(int player, int card, event_t event)
{
  /* Sarkhan's Unsealing	|3|R
   * Enchantment
   * Whenever you cast a creature spell with power 4, 5, or 6, ~ deals 4 damage to any target.
   * Whenever you cast a creature spell with power 7 or greater, ~ deals 4 damage to each opponent and each creature and planeswalker they control. */

  int result;
  card_instance_t *instance;


  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.power = 3;
    test.power_flag = 2;
    sp_ability_trigger(player, event, 2, TYPE_CREATURE, &test, player, card);
  }
  if (event == EVENT_SPA)
  {

    result = get_power(trigger_cause_controller, trigger_cause);
    if (result < 7)
    {
      target_definition_t td;
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
        }
      }
      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
    }
    else
    {
      new_damage_all(player, card, 1 - player, 4, 0x23, NULL);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* Sarkhan's Whelp	|2|R
 * Creature - Dragon 2/2
 * Flying
 * Whenever you activate an ability of a Sarkhan planeswalker, ~ deals 1 damage to any target. */

/* Sarkhan, Dragonsoul	|4|R|R
 * Legendary Planeswalker - Sarkhan (5)
 * +2: ~ deals 1 damage to each opponent and each creature your opponents control.
 * -3: ~ deals 4 damage to target player or planeswalker.
 * -9: Search your library for any number of Dragon creature cards, put them onto the battlefield, then shuffle your library. */

/* Sarkhan, Fireblood	|1|R|R
 * Legendary Planeswalker - Sarkhan (3)
 * +1: You may discard a card. If you do, draw a card.
 * +1: Add two mana in any combination of colors. Spend this mana only to cast Dragon spells.
 * -7: Create four 5/5 |Sred Dragon creature tokens with flying. */

/* Satyr Enchanter	|1|G|W
 * Creature - Satyr Druid 2/2
 * Whenever you cast an enchantment spell, draw a card. */

/* Scholar of Stars	|3|U
 * Creature - Human Artificer 3/2
 * When ~ enters the battlefield, if you control an artifact, draw a card. */

/* Serra's Guardian	|4|W|W
 * Creature - Angel 5/5
 * Flying
 * Vigilance
 * Other creatures you control have vigilance. */

/* ManalinkEh 2022: 0x220b5d0; original cards/m19.c:2203. */
int card_shield_mare(int player, int card, event_t event)
{
  /* Shield Mare	|1|W|W
   * Creature - Horse 2/3
   * ~ can't be blocked by |Sred creatures.
   * When ~ enters the battlefield or becomes the target of a spell or ability an opponent controls, you gain 3 life. */

  card_instance_t *instance;
  int result;
  unsigned int blocker_color;
  unsigned int protected_color;
  const target_t *targeting_source;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    gain_life(player, 3);
    instance = in_play(player, card);
    if (instance == NULL)
    {
      return 0;
    }
  }
  else
    instance = in_play(player, card);
  if (instance)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if ((result == 0) && (targeting_source = becomes_target_of_spell_or_effect(
                              player, card, event, player, card, 1 - player),
                          targeting_source != NULL))
    {
      gain_life(player, 3);
    }
  }
  if (((event == EVENT_BLOCK_LEGALITY) && (attacking_card_controller == player)) &&
      (attacking_card == card))
  {
    blocker_color = get_color(affected_card_controller, affected_card);
    protected_color = get_sleighted_color_test(player, card, 0x10);
    if ((blocker_color & protected_color) != 0)
    {
      event_result = 1;
    }
  }
  return 0;
}

/* Siegebreaker Giant	|3|R|R
 * Creature - Giant Warrior 6/3
 * Trample
 * |3|R: Target creature can't block this turn. */

/* Sigiled Sword of Valeron	|3
 * Artifact - Equipment
 * Equipped creature gets +2/+0, has vigilance, and is a Knight in addition to its other types.
 * Whenever equipped creature attacks, create a 2/2 |Swhite Knight creature token with vigilance that's attacking.
 * Equip |3 */

/* Silverbeak Griffin	|W|W
 * Creature - Griffin 2/2
 * Flying */

/* Skalla Wolf	|3|G|G
 * Creature - Wolf Spirit 3/3
 * When ~ enters the battlefield, look at the top five cards of your library. You may reveal a |Sgreen card from among them and put it into your hand. Put the rest on the bottom of your library in a random order. */

/* Skeleton Archer	|3|B
 * Creature - Skeleton Archer 3/3
 * When ~ enters the battlefield, it deals 1 damage to any target. */

/* Skilled Animator	|2|U
 * Creature - Human Artificer 1/3
 * When ~ enters the battlefield, target artifact you control becomes an artifact creature with base power and toughness 5/5 for as long as ~ remains on the battlefield. */

/* Skyrider Patrol	|2|G|U
 * Creature - Elf Scout 2/3
 * Flying
 * At the beginning of combat on your turn, you may pay |G|U. When you do, put a +1/+1 counter on another target creature you control, and that creature gains flying until end of turn. */

/* Skyscanner	|3
 * Artifact Creature - Thopter 1/1
 * Flying
 * When ~ enters the battlefield, draw a card. */

/* Sovereign's Bite	|1|B
 * Sorcery
 * Target player loses 3 life and you gain 3 life. */

/* Sparktongue Dragon	|3|R|R
 * Creature - Dragon 3/3
 * Flying
 * When ~ enters the battlefield, you may pay |2|R. When you do, it deals 3 damage to any target. */

/* Spit Flame	|2|R
 * Instant
 * ~ deals 4 damage to target creature.
 * Whenever a Dragon enters the battlefield under your control, you may pay |R. If you do, return ~ from your graveyard to your hand. */

/* Star-Crowned Stag	|3|W
 * Creature - Elk 3/3
 * Whenever ~ attacks, tap target creature defending player controls. */

/* ManalinkEh 2022: 0x220b710; original cards/m19.c:2278. */
int card_stitchers_supplier(int player, int card, event_t event)
{
  /* Stitcher's Supplier	|B
   * Creature - Zombie 1/1
   * When ~ enters the battlefield or dies, put the top three cards of your library into your graveyard. */

  int result;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    mill(player, 3);
  }
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  mill(player, 3);
  return 0;
}

/* Strangling Spores	|3|B
 * Instant
 * Target creature gets -3/-3 until end of turn. */

/* Suncleanser	|1|W
 * Creature - Human Cleric 1/4
 * When ~ enters the battlefield, choose one -
 * * Remove all counters from target creature. It can't have counters put on it for as long as ~ remains on the battlefield.
 * * Target opponent loses all counters. That player can't get counters for as long as ~ remains on the battlefield. */

/* Supreme Phantom	|1|U
 * Creature - Spirit 1/3
 * Flying
 * Other Spirits you control get +1/+1. */

/* ManalinkEh 2022: 0x220b7a0; original cards/m19.c:2306. */
int card_surge_mare(int player, int card, event_t event)
{
  /* Surge Mare	|U|U
   * Creature - Horse Fish 0/5
   * ~ can't be blocked by |Sgreen creatures.
   * Whenever ~ deals damage to an opponent, you may draw a card. If you do, discard a card.
   * |1|U: ~ gets +2/-2 until end of turn. */

  int cost_or_eligible;
  int mana_available;
  unsigned int blocker_color;
  unsigned int protected_color;
  int cless;
  int pump_count;

  cost_or_eligible =
      damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_OPPONENT | DDBM_TRIGGER_OPTIONAL);
  if ((cost_or_eligible != 0) &&
      (cost_or_eligible = can_draw_cards_as_cost(player, 1), cost_or_eligible != 0))
  {
    draw_a_card(player);
    discard(player, 0, player);
  }
  if (event == EVENT_BLOCK_LEGALITY)
  {
    if ((attacking_card_controller == player) && (attacking_card == card))
    {
      blocker_color = get_color(affected_card_controller, affected_card);
      protected_color = get_sleighted_color_test(player, card, 8);
      if ((blocker_color & protected_color) != 0)
      {
        event_result = 1;
      }
    }
  }
  else if ((uint32_t)(event - EVENT_POW_BOOST) < 2)
  {
    if (has_mana_for_activated_ability(player, card, 1, 0, 1, 0, 0, 0) == 0)
    {
      return 0;
    }
    cost_or_eligible = get_cost_mod_for_activated_abilities(player, card, 1, 0, 0, 0, 0, 0);
    pump_count = 0;
    cless = 0;
    while (true)
    {
      mana_available = has_mana_for_activated_ability(player, card, cless, 0, 1, 0, 0, 0);
      cless = cless + cost_or_eligible;
      if (mana_available == 0)
        break;
      pump_count = pump_count + 1;
    }
    if (event != EVENT_POW_BOOST)
    {
      return pump_count * -2;
    }
    return pump_count * 2;
  }
  return generic_shade_merge_pt(player, card, event, 0, 1, 0, 1, 0, 0, 0, 2, -2);
}

/* ManalinkEh 2022: 0x220b9f0; original cards/m19.c:2337. */
int card_suspicious_bookcase(int player, int card, event_t event)
{
  /* Suspicious Bookcase	|2
   * Artifact Creature - Wall 0/4
   * Defender
   * |3, |T: Target creature can't be blocked this turn. */

  card_instance_t *instance;
  target_definition_t td;
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    if ((event == EVENT_RESOLVE_ACTIVATION) && (valid_target(&td) != 0))
    {
      instance = get_card_instance(player, card);
      pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                             0, 0, 0, 0x10);
    }
  }
  return generic_activated_ability(player, card, event, 5, 3, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
}

/* Take Vengeance	|1|W
 * Sorcery
 * Destroy target tapped creature. */

/* Talons of Wildwood	|1|G
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +1/+1 and has trample.
 * |2|G: Return ~ from your graveyard to your hand. */

/* Tezzeret's Gatebreaker	|4
 * Artifact
 * When ~ enters the battlefield, look at the top five cards of your library. You may reveal a |Sblue or artifact card from among them and put it into your hand. Put the rest on the bottom of your library in a random order.
 * |5|U, |T, Sacrifice ~: Creatures you control can't be blocked this turn. */

/* ManalinkEh 2022: 0x220bb50; original cards/m19.c:2379. */
int card_tezzeret_artifice_master_emblem(int player, int card, event_t event)
{
  /* Tezzeret, Artifice Master Emblem	""
   * Emblem - Tezzeret
   * At the beginning of your end step, search your library for a permanent card, put it onto the battlefield, then shuffle your library. */

  test_definition_t this_test;

  if (current_turn == player)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      if (eot_trigger(player, card, event))
      {
        new_default_test_definition(&this_test, 0x47, "Select a permanent card.");
        new_global_tutor(player, player, 1, 4, 0, 2, &this_test);
      }
    }
  }
  return 0;
}

/* Tezzeret's Strider	|3
 * Artifact Creature - Golem 3/1
 * As long as you control a Tezzeret planeswalker, ~ has menace. */

/* ManalinkEh 2022: 0x220bc00; original cards/m19.c:2390. */
int card_tezzeret_artifice_master(int player, int card, event_t event)
{
  /* Tezzeret, Artifice Master	|3|U|U
   * Legendary Planeswalker - Tezzeret (5)
   * +1: Create a 1/1 colorless Thopter artifact creature token with flying.
   * 0: Draw a card. If you control three or more artifacts, draw two cards instead.
   * -9: You get an emblem with "At the beginning of your end step, search your library for a permanent card, put it onto the battlefield, then shuffle your library." */

  enum
  {
    CHOICE_THOPTER = 1,
    CHOICE_DRAW = 2,
    CHOICE_EMBLEM = 3
  };

  card_instance_t *instance;
  int mode_or_draw_count;
  int metalcraft_active;
  token_generation_t token;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      if (dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, "Create a Thopter", 1,
                      10, 1, "Draw", 1, 10, 0, "Emblem", 1, 100, 0xfffffff7, 0, 0, 0, 0) == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    mode_or_draw_count = instance->info_slot;
    if (mode_or_draw_count == CHOICE_THOPTER)
    {
      default_token_definition(player, card, CARD_ID_THOPTER, &token);
      token.tou = 1;
      token.pow = 1;
      token.key_plus = 0x20;
      generate_token(&token);
    }
    else if (mode_or_draw_count == CHOICE_DRAW)
    {
      metalcraft_active = metalcraft(player, card);
      mode_or_draw_count = 2;
      if (metalcraft_active == 0)
      {
        mode_or_draw_count = 1;
      }
      draw_cards(player, mode_or_draw_count);
    }
    else if (mode_or_draw_count == CHOICE_EMBLEM)
    {
      generate_token_by_id(player, card, CARD_ID_TEZZERET_ARTIFICE_MASTER_EMBLEM);
    }
  }
  return planeswalker(player, card, event, 5);
}

/* Tezzeret, Cruel Machinist	|4|U|U
 * Legendary Planeswalker - Tezzeret (4)
 * +1: Draw a card.
 * 0: Until your next turn, target artifact you control becomes a 5/5 creature in addition to its other types.
 * -7: Put any number of cards from your hand onto the battlefield face down. They're 5/5 artifact creatures. */

/* Thorn Lieutenant	|1|G
 * Creature - Elf Warrior 2/3
 * Whenever ~ becomes the target of a spell or ability an opponent controls, create a 1/1 |Sgreen Elf Warrior creature token.
 * |5|G: ~ gets +4/+4 until end of turn. */

/* Thud	|R
 * Sorcery
 * As an additional cost to cast this spell, sacrifice a creature.
 * ~ deals damage equal to the sacrificed creature's power to any target. */

/* ManalinkEh 2022: 0x220bdf0; original cards/m19.c:2464. */
int card_transmogrifying_wand(int player, int card, event_t event)
{
  /* Transmogrifying Wand	|3
   * Artifact
   * ~ enters the battlefield with three charge counters on it.
   * |1, |T, Remove a charge counter from ~: Destroy target creature. Its controller creates a 2/4 |Swhite Ox creature token. Activate this ability only any time you could cast a sorcery. */

  int result;
  card_instance_t *instance;
  token_generation_t token;
  target_definition_t td;
  enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, 3);
  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 2);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    instance = get_card_instance(player, card);
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    default_token_definition(player, card, CARD_ID_OX, &token);
    token.t_player = instance->targets[0].player;
    generate_token(&token);
  }
  result = generic_activated_ability(player, card, event, 5, 1, 0, 0, 0, 0, 0, 0x17b0100, &td,
                                     "TARGET_CREATURE");
  return result;
}

/* Trusty Packbeast	|2|W
 * Creature - Beast 2/3
 * When ~ enters the battlefield, return target artifact card from your graveyard to your hand. */

/* Two-Headed Zombie	|3|B
 * Creature - Zombie 4/2
 * Menace */

/* ManalinkEh 2022: 0x220bf90; original cards/m19.c:2500. */
int card_uncomfortable_chill(int player, int card, event_t event)
{
  /* Uncomfortable Chill	|2|U
   * Instant
   * Creatures your opponents control get -2/-0 until end of turn.
   * Draw a card. */

  int result;
  if (event == EVENT_RESOLVE_SPELL)
  {
    pump_subtype_until_eot(player, card, 1 - player, -1, -2, 0, 0, 0);
    draw_cards(player, 1);
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
  return result;
}

/* Ursine Champion	|1|G
 * Creature - Human Berserker 2/2
 * |5|G: ~ gets +3/+3 and becomes a Bear Berserker until end of turn. Activate this ability only once each turn. */

/* ManalinkEh 2022: 0x220c050; original cards/m19.c:2517. */
int card_vaevictis_asmadi_the_dire(int player, int card, event_t event)
{
  /* Vaevictis Asmadi, the Dire	|3|B|R|G
   * Legendary Creature - Elder Dragon 6/6
   * Flying
   * Whenever ~ attacks, for each player, choose target permanent that player controls. Those players sacrifice those permanents. Each player who sacrificed a permanent this way reveals the top card of their library, then puts it onto the battlefield if it's a permanent card. */

  int *library;
  card_instance_t *instance;
  const char *fmt;
  int result;
  unsigned int affected_player;
  unsigned int pass;
  char buffer[75];
  target_definition_t td;

  check_legend_rule(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {

    instance = get_card_instance(player, card);

    pass = 0;
    default_target_definition(player, card, &td, 0x47);
    td.allow_cancel = 0;
    do
    {
      affected_player = current_turn ^ pass;
      if (player == affected_player)
      {
        fmt = "Select target permanent you control.";
      }
      else
      {
        fmt = "Select target permanent your opponent controls.";
      }
      td.allowed_controller = affected_player;
      scnprintf(buffer, 0x4b, fmt);
      result = can_target(&td);
      if (result != 0)
      {
        new_pick_target(&td, buffer, affected_player, 0x80);
      }
      pass = pass + 1;
    } while (pass != 2);

    pass = 0;
    do
    {
      affected_player = current_turn ^ pass;
      td.allowed_controller = affected_player;
      result = validate_target(player, card, &td, affected_player);
      if (result != 0)
      {

        kill_card(instance->targets[affected_player].player,
                  instance->targets[affected_player].card, KILL_SACRIFICE);
        library = deck_ptr[instance->targets[affected_player].player];
        result = *library;
        if (result != -1)
        {
          reveal_card_iid(player, card, result);
          result = is_what(-1, *library, 0x47);
          if (result != 0)
          {
            result = instance->targets[affected_player].player;
            put_into_play_a_card_from_deck(result, result, 0);
          }
        }
      }
      pass = pass + 1;
    } while (pass != 2);
  }
  return 0;
}

/* Valiant Knight	|3|W
 * Creature - Human Knight 3/4
 * Other Knights you control get +1/+1.
 * |3|W|W: Knights you control gain double strike until end of turn. */

/* ManalinkEh 2022: 0x220c250; original cards/m19.c:2563. */
int card_vampire_neonate(int player, int card, event_t event)
{
  /* Vampire Neonate	|B
   * Creature - Vampire 0/3
   * |2, |T: Each opponent loses 1 life and you gain 1 life. */

  int result;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    lose_life(1 - player, 1);
    gain_life(player, 1);
  }
  result = generic_activated_ability(player, card, event, 1, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* Vampire Sovereign	|3|B|B
 * Creature - Vampire 3/4
 * Flying
 * When ~ enters the battlefield, target opponent loses 3 life and you gain 3 life. */

/* ManalinkEh 2022: 0x220c300; original cards/m19.c:2581. */
int card_viashino_pyromancer(int player, int card, event_t event)
{
  /* Viashino Pyromancer	|1|R
   * Creature - Viashino Wizard 2/1
   * When ~ enters the battlefield, it deals 2 damage to target player or planeswalker. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x100);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  td.allow_cancel = 0;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER", 0, 0);
    if (result != 0)
    {
      damage_target0(player, card, 2);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* Vigilant Baloth	|3|G|G
 * Creature - Beast 5/5
 * Vigilance */

/* ManalinkEh 2022: 0x220c3e0; original cards/m19.c:2608. */
int card_vine_mare(int player, int card, event_t event)
{
  /* Vine Mare	|2|G|G
   * Creature - Elemental Horse 5/3
   * Hexproof
   * ~ can't be blocked by |Sblack creatures. */

  unsigned int blocker_color;
  unsigned int protected_color;
  hexproof(player, card, event);
  if (((event == EVENT_BLOCK_LEGALITY) && (attacking_card_controller == player)) &&
      (attacking_card == card))
  {
    blocker_color = get_color(affected_card_controller, affected_card);
    protected_color = get_sleighted_color_test(player, card, 2);
    if ((blocker_color & protected_color) != 0)
    {
      event_result = 1;
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22060e0; original cards/m19.c:2621. */
static char *is_artifact_or_enchantment_or_flying_creature(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
  int result;

  result = is_what(player, card, 0x44);
  if (result != 0)
  {
    return NULL;
  }
  result = is_what(player, card, 2);
  if ((result != 0) && (result = check_for_ability(player, card, 0x20), result != 0))
  {
    return NULL;
  }
  return "must be an artifact, enchantment, or creature with flying.";
}
#endif

/* ManalinkEh 2022: 0x220c470; original cards/m19.c:2630. */
int card_vivien_reid_emblem(int player, int card, event_t event)
{
  /* Vivien Reid Emblem	""
   * Emblem - Vivien
   * Creatures you control get +2/+2 and have vigilance, trample, and indestructible. */

  boost_subtype(player, card, event, ~SUBTYPE_INVALID, 2, 2, 0x80, 0x808, BCT_CONTROLLER_ONLY);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x220c4d0; original cards/m19.c:2637. */
int card_vivien_reid(int player, int card, event_t event)
{
  /* Vivien Reid	|3|G|G
   * Legendary Planeswalker - Vivien (5)
   * +1: Look at the top four cards of your library. You may reveal a creature or land card from among them and put it into your hand. Put the rest on the bottom of your library in a random order.
   * -3: Destroy target artifact, enchantment, or creature with flying.
   * -8: You get an emblem with "Creatures you control get +2/+2 and have vigilance, trample, and indestructible." */

  enum
  {
    CHOICE_LIBRARY = 1,
    CHOICE_DESTROY = 2,
    CHOICE_EMBLEM = 3
  };

  int result;
  target_definition_t *permanent_target;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0x47);
    td.extra = (intptr_t)is_artifact_or_enchantment_or_flying_creature;
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      result = can_target(&td);
      result =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER,
                      "Look at the top cards of your library", 1, 5, 1, "Destroy target", result,
                      10, 0xfffffffd, "Create an Emblem", 1, 0xf, 0xfffffff8, 0, 0, 0, 0);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (result == CHOICE_DESTROY)
      {
        new_pick_target(&td, "Choose target artifact, enchantment, or creature with flying", -1,
                        0x80);
      }
      return planeswalker(player, card, event, 5);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      return planeswalker(player, card, event, 5);
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0x47);
  }
  permanent_target = &td;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (intptr_t)is_artifact_or_enchantment_or_flying_creature;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    result = instance->info_slot;
    if (result == CHOICE_LIBRARY)
    {
      new_default_test_definition(&test, 3, "Select a creature or land card.");
      reveal_top_cards_of_library_and_choose(player, card, player, 4, 0, TUTOR_HAND, 1,
                                             TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
    }
    else if (result == CHOICE_DESTROY)
    {
      result = valid_target(permanent_target);
      if (result != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
    }
    else if (result == CHOICE_EMBLEM)
    {
      generate_token_by_id(player, card, CARD_ID_VIVIEN_REID_EMBLEM);
    }
  }
  return planeswalker(player, card, event, 5);
}
#endif

/* Vivien of the Arkbow	|4|G|G
 * Legendary Planeswalker - Vivien (5)
 * +2: Put two +1/+1 counters on up to one target creature.
 * -3: Target creature you control deals damage equal to its power to target creature you don't control.
 * -9: Creatures you control get +4/+4 and gain trample until end of turn. */

/* Vivien's Invocation	|5|G|G
 * Sorcery
 * Look at the top seven cards of your library. You may put a creature card from among them onto the battlefield. Put the rest on the bottom of your library in a random order. When a creature is put onto the battlefield this way, it deals damage equal to its power to target creature an opponent controls. */

/* Vivien's Jaguar	|2|G
 * Creature - Cat Spirit 3/2
 * Reach
 * |2|G: Return ~ from your graveyard to your hand. Activate this ability only if you control a Vivien planeswalker. */

/* ManalinkEh 2022: 0x220c7d0; original cards/m19.c:2717. */
int card_volley_veteran(int player, int card, event_t event)
{
  /* Volley Veteran	|3|R
   * Creature - Goblin Warrior 4/2
   * When ~ enters the battlefield, it deals damage to target creature an opponent controls equal to the number of Goblins you control. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  td.allow_cancel = 0;
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS", 0, 0);
    if (result != 0)
    {
      new_default_test_definition(&test, 0x47, "");
      test.subtype = 0x4b;
      result = check_battlefield_for_special_card(player, card, player, 4, &test);
      damage_target0(player, card, result);
    }
  }
  instance = get_card_instance(player, card);
  instance->number_of_targets = '\0';
  return 0;
}

/* Wall of Mist	|1|U
 * Creature - Wall 0/5
 * Defender */

/* Angelic Guardian	|4|W|W
 * Creature - Angel 5/5
 * Flying
 * Whenever one or more creatures you control attack, they gain indestructible until end of turn. */

/* Angler Turtle	|5|U|U
 * Creature - Turtle 5/7
 * Hexproof
 * Creatures your opponents control attack each combat if able. */

/* ManalinkEh 2022: 0x220c900; original cards/m19.c:2759. */
int card_vengeant_vampire(int player, int card, event_t event)
{
  /* Vengeant Vampire	|4|B|B
   * Creature - Vampire 4/4
   * Lifelink
   * When ~ dies, destroy target creature an opponent controls and you gain 4 life. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
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
        action_on_target(player, card, 0, ACT_KILL_DESTROY);
        gain_life(player, 4);
        instance->number_of_targets = '\0';
      }
    }
  }
  return 0;
}

/* Immortal Phoenix	|4|R|R
 * Creature - Phoenix 5/3
 * Flying
 * When ~ dies, return it to its owner's hand. */

/* ManalinkEh 2022: 0x220c9e0; original cards/m19.c:2788. */
int card_rampaging_brontodon(int player, int card, event_t event)
{
  /* Rampaging Brontodon	|5|G|G
   * Creature - Dinosaur 7/7
   * Trample
   * Whenever ~ attacks, it gets +1/+1 until end of turn for each land you control. */

  int power;
  test_definition_t test;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }

  new_default_test_definition(&test, 1, "");
  power = check_battlefield_for_special_card(player, card, player, 4, &test);
  pump_until_eot(player, card, player, card, power, power);
  return 0;
}
