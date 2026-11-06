#include "manalink.h"

static int is_not_commander(int iid, intptr_t val, int player, int card);
int count_creatures_with_p1_p1_counter(int player);

static void slurrk_trigger(int player, int card);
static int juri_master_of_the_revue_effect(int player, int card, event_t event);
static int slurrk_effect(int player, int card, event_t event);
static void ich_tekik_ability(int player, int card);
static int ich_tekik_effect(int player, int card, event_t event);

static int encore_granted_legacy(int player, int card, event_t event);
static int encore_legacy(int player, int card, event_t event);

static int count_how_many_times_a_commander_was_played(int player);

static int colfenor_effect(int player, int card, event_t event);

static int is_aura_or_equipment_by_iid(int iid, intptr_t unused, int player, int card);

#ifndef SHANDALAR

static int miara_effect(int player, int card, event_t event);

#endif

#ifndef SHANDALAR

int card_kodama_of_the_east_tree(int player, int card, event_t event);
int card_akroma_vision_of_ixidor(int player, int card, event_t event);

#endif

/* ManalinkEh 2022: 0x25a1e20; original cards/commander_legends.c:9. */
static int encore_legacy(int player, int card, event_t event)
{
  int target_card;
  card_instance_t *instance;
  int result;
  int target_player;

  if (event == EVENT_SET_LEGACY_EFFECT_NAME)
  {
    instance = get_card_instance(player, card);
    if (-1 < instance->targets[0].card)
    {
      scnprintf(set_legacy_effect_name_addr, 0x33, "Encore");
    }
  }
  else if (event == EVENT_SET_LEGACY_EFFECT_TEXT)
  {
    scnprintf(set_legacy_effect_name_addr, 400,
              "Encore\n Exile this card from your graveyard: For each opponent, create a token "
              "copy that attacks that opponent this turn if able. They gain haste. Sacrifice them "
              "at the beginning of the next end step.");
  }
  if (((unsigned int)player < 3) && ((unsigned int)card < 0x97))
  {

    instance = get_card_instance(player, card);
    if (instance != NULL)
    {

      target_player = (int)instance->damage_target_player;
      target_card = instance->damage_target_card;
      if ((-1 < target_player) && (-1 < target_card))
      {
        haste(target_player, target_card, event);
        attack_if_able(target_player, target_card, event);
        result = eot_trigger(player, card, event);
        if (result != 0)
        {
          result = check_state(target_player, target_card, STATE_OUBLIETTED);
          if (result == 0)
          {
            kill_card(target_player, target_card, KILL_SACRIFICE);
          }
          else
          {
            kill_card(player, card, KILL_EXILE);
          }
        }
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
static void generate_encore_token(int player, int card, int csvid)
{
  token_generation_t token;
  default_token_definition(player, card, csvid, &token);
  token.legacy = 1;
  token.special_code_for_legacy = encore_legacy;
  generate_token(&token);
}
#endif

/* ManalinkEh 2022: 0x25a2850; original cards/commander_legends.c:43. */
static int encore(int player, int card, event_t event, card_id_t cid, int x, int b, int u, int g,
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
      return 0x20;
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
    token.legacy = '\x01';
    token.special_code_for_legacy = encore_legacy;
    generate_token(&token);
  }
  return (unsigned int)(event == EVENT_CAN_CAST);
}

/* ManalinkEh 2022: 0x25a07d0; original cards/commander_legends.c:65. */
static int count_how_many_times_a_commander_was_played(int player)
{
  int edh_card = locate_edh_card(player);
  if (edh_card == -1)
    return 0;
  int amount = count_counters(player, edh_card, COUNTER_VORTEX);
  int second_counter = count_counters(player, edh_card, COUNTER_VELOCITY);
  return second_counter / 2 + amount / 2;
}

/* ManalinkEh 2022: 0x25a2970; original cards/commander_legends.c:76. */
int card_abomination_of_llanowar(int player, int card, event_t event)
{
  /* Abomination of Llanowar	|1|B|G
   * Legendary Creature - Elf Horror * / *
   * Vigilance; menace (This creature can't be blocked except by two or more creatures.)
   * ~'s power and toughness are each equal to the number of Elves you control plus the number of Elf cards in your graveyard. */

  int grave_elves;
  int battlefield_elves;

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  menace(player, card, event);
  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    if (is_humiliated(player, card, AL_STATIC) == 0)
    {
      grave_elves = count_graveyard_by_subtype(player, SUBTYPE_ELF);
      battlefield_elves = count_subtype(player, 0xff, 0x3c);
      set_base_pt(grave_elves + battlefield_elves);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a2a30; original cards/commander_legends.c:94. */
int card_aesi_tyrant_of_gyre_strait(int player, int card, event_t event)
{
  /* Aesi, Tyrant of Gyre Strait	|4|G|U
   * Legendary Creature - Serpent 5/5
   * You may play an additional land on each of your turns.
   * Landfall - Whenever a land you control enters, you may draw a card. */

  int result;
  check_legend_rule(player, card, event);
  result = duh_mode(player);
  landfall_mode(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    draw_a_card(player);
  }
  return card_exploration(player, card, event);
}

/* ManalinkEh 2022: 0x25a2ab0; original cards/commander_legends.c:112. */
int card_akromas_will(int player, int card, event_t event)
{
  /* Akroma's Will	|3|W
   * Instant
   * Choose one. If you control a commander as you cast this spell, you may choose both instead.
   * * Creatures you control gain flying, vigilance, and double strike until end of turn.
   * * Creatures you control gain lifelink, indestructible, and protection from each color until end of turn. */

  int result;
  int sp_ability;
  int ability;

  if (event == EVENT_RESOLVE_SPELL)
  {

    result = liutenant(player);
    result = dialog_impl(
        player, card, EVENT_RESOLVE_SPELL, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
        "Flying, vigilance, double strike", 1, (unsigned int)(current_phase < 0x19) * 9 + 1,
        "Lifelink, indestructible, protection from all colors", 1,
        ((current_turn != player) ? 15U : 1U), "Do both", result, 0x14, 0, 0, 0, 0);
    if (result == 2)
    {
      sp_ability = 0x802;
      ability = 0xf800;
    }
    else
    {
      sp_ability = ((result == 3) ? 2058U : 8U);
      ability = ((result == 3) ? 0x4000f820U : 0x40000020U);
    }
    pump_creatures_until_eot_no_repeat(player, card, player, result, ability, sp_ability, NULL);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25a2c20; original cards/commander_legends.c:143. */
int card_akroma_vision_of_ixidor(int player, int card, event_t event)
{
  /* Akroma, Vision of Ixidor	|5|W|W
   * Legendary Creature - Angel 6/6
   * Flying, first strike, vigilance, trample
   * At the beginning of each combat, until end of turn, each other creature you control gets +1/+1 if it has flying, +1/+1 if it has first strike, and so on for double strike, deathtouch, haste, hexproof, indestructible, lifelink, menace, protection, reach, trample, vigilance, and partner.
   * Partner */

  int c;
  int combat_triggered;
  card_instance_t *instance;
  int csvid_or_bonus;
  unsigned int keywords_or_partner;
  unsigned int special_keywords;
  unsigned int keyword_count;
  int result_or_next_card;
  unsigned int protection;
  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  combat_triggered = beginning_of_combat(player, card, event, 2);
  if ((combat_triggered != 0) && (0 < active_cards_count[player]))
  {
    c = 0;
    do
    {
      while (((instance = in_play(player, c),
               instance != NULL && (csvid_or_bonus = is_what(player, c, 2), card != c)) &&
              (csvid_or_bonus != 0)))
      {
        keywords_or_partner = get_abilities(player, c, EVENT_ABILITIES, -1);
        special_keywords = get_special_abilities(player, c);
        csvid_or_bonus = get_id(player, c);
        keyword_count = num_bits_set(keywords_or_partner &
                                     (KEYWORD_DOUBLE_STRIKE | KEYWORD_REACH | KEYWORD_FIRST_STRIKE |
                                      KEYWORD_TRAMPLE | KEYWORD_FLYING));
        special_keywords = num_bits_set(
            special_keywords & (SP_KEYWORD_HEXPROOF | SP_KEYWORD_MENACE |
                                SP_KEYWORD_INDESTRUCTIBLE | SP_KEYWORD_DEATHTOUCH |
                                SP_KEYWORD_VIGILANCE | SP_KEYWORD_HASTE | SP_KEYWORD_LIFELINK));
        protection = 1;
        if ((keywords_or_partner &
             (KEYWORD_PROT_LANDS | KEYWORD_PROT_CREATURES | KEYWORD_PROT_SORCERIES |
              KEYWORD_PROT_INSTANTS | KEYWORD_PROT_ARTIFACTS | KEYWORD_PROT_COLORED)) == 0)
        {
          result_or_next_card = is_protected_from(player, c, -1, -1, IPF_QUERY);
          protection = (unsigned int)(result_or_next_card != 0);
        }
        result_or_next_card = general_has_partner(csvid_or_bonus);
        keywords_or_partner = 1;
        if (result_or_next_card == 0)
        {
          keywords_or_partner = get_partner_with_id(csvid_or_bonus);
          keywords_or_partner = (keywords_or_partner ^ 0xffffffff) >> 0x1f;
        }
        csvid_or_bonus = protection + keyword_count + special_keywords + keywords_or_partner;
        if (csvid_or_bonus == 0)
          break;
        result_or_next_card = c + 1;
        pump_until_eot_merge_previous(player, card, player, c, csvid_or_bonus, csvid_or_bonus);
        c = result_or_next_card;
        if (active_cards_count[player] <= result_or_next_card)
        {
          return 0;
        }
      }
      c = c + 1;
    } while (c < active_cards_count[player]);
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x25a2de0; original cards/commander_legends.c:182. */
int card_alena_kessig_trapper(int player, int card, event_t event)
{
  /* Alena, Kessig Trapper	|4|R
   * Legendary Creature - Human Scout 4/3
   * First strike
   * |T: Add an amount of |R equal to the greatest power among creatures you control that entered this turn.
   * Partner (You can have two commanders if both have partner.) */

  card_instance_t *instance;
  int power_or_eligible;
  int card_00;
  int highest_power;

  check_legend_rule(player, card, event);
  if (((event == EVENT_COUNT_MANA) || (event == EVENT_ACTIVATE)) ||
      ((event == EVENT_CAN_ACTIVATE && (player == 1))))
  {

    highest_power = 0;
    card_00 = active_cards_count[player] + -1;
    if (-1 < card_00)
    {
      do
      {

        while (
            (((instance = in_play(player, card_00),
               instance == NULL ||
                   (power_or_eligible = is_what(player, card_00, 2), power_or_eligible == 0)) ||
              (power_or_eligible = check_special_flags(player, card_00, 0x1000),
               power_or_eligible == 0)) ||
             (power_or_eligible = get_power(player, card_00), power_or_eligible <= highest_power)))
        {
          card_00 = card_00 + -1;
          if (card_00 == -1)
            return mana_producing_creature(player, card, event, 0x30, COLOR_RED, highest_power);
        }
        card_00 = card_00 + -1;
        highest_power = power_or_eligible;
      } while (card_00 != -1);
    }
  }
  else
  {
    highest_power = 0;
  }
  return mana_producing_creature(player, card, event, 0x30, COLOR_RED, highest_power);
}

/* ManalinkEh 2022: 0x259ffa0; original cards/commander_legends.c:206. */
static int alhard_solemn_ritualist_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  int deaths_or_result;
  card_instance_t *dying;
  token_generation_t token;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (deaths_or_result =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           deaths_or_result != 0x5631))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (deaths_or_result = effect_follows_control_of_attachment(player, card, 0x4700),
         deaths_or_result != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (deaths_or_result = effect_follows_control_of_attachment(player, card, event),
         deaths_or_result != 0))
    {
      return 0;
    }
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {
      instance = get_card_instance(player, card);
      dying = in_play(affected_card_controller, affected_card);
      if (((dying != NULL) &&
           (deaths_or_result = is_this_dying(affected_card_controller, affected_card),
            deaths_or_result != 0)) &&
          (deaths_or_result =
               count_counters(affected_card_controller, affected_card, COUNTER_P1_P1),
           deaths_or_result != 0))
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
  deaths_or_result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_GRAVEYARD_FROM_PLAY && deaths_or_result == 0)
    return 0;
  if (deaths_or_result != 0)
  {
    if (check_status(player, card, 0x10000) == 0)
    {

      default_token_definition(player, card, CARD_ID_SPIRIT, &token);
      token.pow = 1;
      token.tou = 1;
      token.color_forced = ' ';
      token.key_plus = 0x20;
      token.qty = (unsigned char)deaths_or_result;
      generate_token(&token);
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) &&
      (deaths_or_result = check_status(player, card, 0x10000), deaths_or_result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a2ed0; original cards/commander_legends.c:262. */
int card_alharu_solemn_ritualist(int player, int card, event_t event)
{
  /* Alharu, Solemn Ritualist	|4|W
   * Legendary Creature - Human Monk 3/3
   * When Alharu enters, put a +1/+1 counter on each of up to two other target creatures.
   * Whenever a nontoken creature you control with a +1/+1 counter on it dies, create a 1/1 white Spirit creature token with flying.
   * Partner (You can have two commanders if both have partner.) */

  int i;
  card_instance_t *instance;
  int selected_or_type;
  int deaths_or_legacy;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    int tgs[3];
    state_untargettable(player, card, 1);
    instance = get_card_instance(player, card);
    target_definition_t td1;
    default_target_definition(player, card, &td1, 2);
    tgs[0] = card;
    td1.preferred_controller = player;
    td1.allowed_controller = player;
    tgs[1] = -1;
    tgs[2] = -1;
    i = 1;
    do
    {
      if (can_target(&td1) != 0)
      {
        if (new_pick_target(&td1, "TARGET_CREATURE_YOU_CONTROL", 0, 0) == 0)
          break;
        selected_or_type = instance->targets[0].card;
        tgs[i] = selected_or_type;
        state_untargettable(player, selected_or_type, 1);
        instance->number_of_targets = '\0';
      }
      i = i + 1;
    } while (i != 3);
    for (int selected = 0; selected < 3; ++selected)
      if (tgs[selected] != -1)
      {
        state_untargettable(player, tgs[selected], 0);
        if (selected != 0)
          add_counters(player, card, player, tgs[selected], COUNTER_P1_P1, 1);
      }
  }
  else if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      i = 0;
      bool found = false;
      if (0 < active_cards_count[player])
      {
        do
        {
          instance = in_play(player, i);
          if ((((instance != NULL) &&
                (selected_or_type = is_what(player, i, 0x80), selected_or_type != 0)) &&
               (instance = get_card_instance(player, i),
                instance->info_slot_fn == alhard_solemn_ritualist_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            found = true;
            break;
          }
          i = i + 1;
        } while (i < active_cards_count[player]);
      }
      if (!found)
      {
        deaths_or_legacy = create_targetted_legacy_effect(
            player, card, alhard_solemn_ritualist_effect, player, card);
        instance = get_card_instance(player, deaths_or_legacy);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, deaths_or_legacy, 0x10000);
      }
    }
  }
  else if (((event == EVENT_GRAVEYARD_FROM_PLAY) &&
            (instance = in_play(affected_card_controller, affected_card), instance != NULL)) &&
           ((deaths_or_legacy = is_this_dying(affected_card_controller, affected_card),
             deaths_or_legacy != 0 && (deaths_or_legacy = count_counters(
                                           affected_card_controller, affected_card, COUNTER_P1_P1),
                                       deaths_or_legacy != 0))))
  {
    count_for_gfp_ability_and_store_values(player, card, 0x77, player, 2, NULL, 0x40, 0);
  }
  instance = get_card_instance(player, card);
  if (instance->kill_code == '\0')
  {
    deaths_or_legacy = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_SPIRIT, &token);
    token.qty = (uint8_t)deaths_or_legacy;
    token.pow = 1;
    token.tou = 1;
    token.color_forced = 0x20;
    token.key_plus = 0x20;
    generate_token(&token);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a32b0; original cards/commander_legends.c:352. */
int card_amareth_the_lustrous(int player, int card, event_t event)
{
  /* Amareth, the Lustrous	|3|G|W|U
   * Legendary Creature - Dragon 6/6
   * Flying
   * Whenever another permanent you control enters, look at the top card of your library. If it shares a card type with that permanent, you may reveal that card and put it into your hand. */

  int *library;
  int iid;
  type_t entered_type;
  int preview_card;
  type_t revealed_type;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if ((trigger_condition == TRIGGER_COMES_INTO_PLAY) &&
      ((trigger_cause_controller != player || (trigger_cause != card))))
  {
    etb_ability_global(player, event, 2, TYPE_PERMANENT, NULL, player, card);
  }
  if (event == EVENT_ETB)
  {

    library = deck_ptr[player];
    entered_type = get_type(trigger_cause_controller, trigger_cause);
    iid = *library;
    if (iid != -1)
    {

      preview_card = add_card_to_hand(player, iid);
      if (preview_card == -1)
      {
        revealed_type = get_type_with_iid(-1, -1, iid);
      }
      else
      {
        revealed_type = get_type_with_iid(player, preview_card, iid);
        obliterate_card(player, preview_card);
      }
      new_default_test_definition(&test, 0xff, "Select a card to add to your hand");
      test.qty = (unsigned short)((revealed_type & entered_type) != TYPE_NONE);
      reveal_top_cards_of_library_and_choose(player, card, player, 1, 0, TUTOR_HAND, 1, TUTOR_0, 0,
                                             &test);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a3450; original cards/commander_legends.c:388. */
int card_amphin_mutineer(int player, int card, event_t event)
{
  /* Amphin Mutineer	|3|U
   * Creature - Salamander Pirate 3/3
   * When this creature enters, exile up to one target non-Salamander creature. That creature's controller creates a 4/3 blue Salamander Warrior creature token.
   * Encore |4|U|U (|4|U|U, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_AMPHIN_MUTINEER = 22067
  };

  int result;
  card_instance_t *instance;
  token_generation_t token;
  target_definition_t td1;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td1, 2);
    td1.required_subtype = 0xa0;
    td1.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
    instance = get_card_instance(player, card);
    result = can_target(&td1);
    if (result != 0)
    {
      result = new_pick_target(&td1, "Select up to one target non-Salamander creature", 0, 0x80);
      if (result != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
        default_token_definition(player, card, CARD_ID_SALAMANDER_WARRIOR, &token);
        token.t_player = instance->targets[0].player;
        token.color_forced = '\x04';
        generate_token(&token);
      }
    }
    instance->number_of_targets = '\0';
  }
  result = encore(player, card, event, CARD_ID_AMPHIN_MUTINEER, 4, 0, 2, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x25a35c0; original cards/commander_legends.c:423. */
int card_anara_wolvid_familiar(int player, int card, event_t event)
{
  /* Anara, Wolvid Familiar	|3|G
   * Legendary Creature - Wolf Beast 4/4
   * During your turn, commanders you control have indestructible. (Effects that say "destroy" don't destroy them. A creature with indestructible can't be destroyed by damage.)
   * Partner (You can have two commanders if both have partner.) */

  int result;
  card_instance_t *instance;

  check_legend_rule(player, card, event);
  if ((event == EVENT_ABILITIES) && (current_turn == player))
  {
    result = check_special_flags2(affected_card_controller, affected_card, 0x1000000);
    if (result != 0)
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if ((result == 0) && (affected_card_controller == player))
        {
          indestructible(player, affected_card, EVENT_ABILITIES);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a3670; original cards/commander_legends.c:445. */
int card_anointer_of_valor(int player, int card, event_t event)
{
  /* Anointer of Valor	|5|W
   * Creature - Angel 3/5
   * Flying
   * Whenever a creature attacks, you may pay |3. When you do, put a +1/+1 counter on that creature. */

  store_attackers(player, card, event, DAT_TRACK, player, -1, NULL);
  dat_ability(player, card, event, (duh_mode(player) != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, 1);
  if (event == EVENT_DAT_ABILITY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    const uint8_t *attackers = (const uint8_t *)&instance->targets + 2 * sizeof(target_t);
    for (int index = instance->info_slot - 1; index >= 0; --index)
    {
      if (!has_mana(player, COLOR_COLORLESS, 3))
        return 0;
      if (in_play(current_turn, attackers[index]))
      {
        add_status(current_turn, attackers[index], STATUS_RED_BORDER);
        charge_mana(player, COLOR_COLORLESS, 3);
        remove_status(current_turn, attackers[index], STATUS_RED_BORDER);
        if (spell_fizzled != 1)
          add_counters(player, card, current_turn, attackers[index], COUNTER_P1_P1, 1);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a3810; original cards/commander_legends.c:470. */
int card_apex_devastator(int player, int card, event_t event)
{
  /* Apex Devastator	|8|G|G
   * Creature - Chimera Hydra 10/10
   * Cascade, cascade, cascade, cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order. Multiple instances of cascade each trigger separately.) */

  multicascade(player, card, event, 4);
  return 0;
}

/* ManalinkEh 2022: 0x25a0440; original cards/commander_legends.c:481. */
static int encore_granted_legacy(int player, int card, event_t event)
{
  unsigned short csvid;
  int player_00;
  int source;
  card_instance_t *instance;
  int result;
  token_generation_t token;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = get_card_instance(player, card);
    if (instance->targets[1].card != -1)
      return 0;
    kill_card(player, card, KILL_EXILE);
  }
  else if (event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (instance->targets[5].player == 0x42)
      {
        return 0;
      }
      result = find_in_graveyard_by_source(instance->targets[2].card, instance->targets[1].player,
                                           instance->targets[1].card);
      instance->targets[1].card = result;
      if (result == -1)
      {
        return 0;
      }
      result = has_mana_to_cast_iid(player, EVENT_CAN_ACTIVATE, instance->targets[0].card);
      if (result == 0)
      {
        return 0;
      }
      result = can_sorcery_be_played(player, EVENT_CAN_ACTIVATE);
      if (result == 0)
      {
        return 0;
      }
      return 1;
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
  if (event == EVENT_ACTIVATE)
  {

    result = instance->targets[1].card;
    player_00 = instance->targets[2].card;
    csvid = cards_data[instance->targets[0].card].id;
    source = instance->targets[1].player;
    charge_mana_from_id(player, -1, EVENT_ACTIVATE, (unsigned int)csvid);
    if (spell_fizzled != 1)
    {
      result = find_in_graveyard_by_source(player_00, source, result);
      rfg_card_from_grave(player_00, result);
      instance->targets[5].player = 0x42;
      instance->targets[5].card = (unsigned int)csvid;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    default_token_definition(instance->parent_controller, instance->parent_card,
                             instance->targets[5].card, &token);
    token.legacy = '\x01';
    token.special_code_for_legacy = encore_legacy;
    generate_token(&token);
    instance->targets[1].card = -1;
    instance->targets[1].player = -1;
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

#ifndef SHANDALAR
static void create_encore_granted_legacy(int player, int card, int graveyard_pos)
{
  if (graveyard_pos < 0)
    backtrace_and_abort("cards/commander_legends.c:539\ngraveyard_pos >= 0");
  int legacy = create_legacy_activate(player, card, encore_granted_legacy);
  card_instance_t *instance = get_card_instance(player, legacy);
  const int *grave = get_grave(player);
  instance->targets[0].card = grave[graveyard_pos];
  instance->targets[1].player = graveyard_source[player][graveyard_pos];
  instance->targets[1].card = graveyard_pos;
  instance->targets[2].card = player;
  instance->targets[3].player = player;
  instance->targets[3].card = card;
  create_card_name_legacy(player, legacy, cards_data[grave[graveyard_pos]].id);
}
#endif

/* ManalinkEh 2022: 0x25a3840; original cards/commander_legends.c:552. */
int card_araumi_of_the_dead_tide(int player, int card, event_t event)
{
  /* Araumi of the Dead Tide	|1|U|B
   * Legendary Creature - Merfolk Wizard 1/4
   * |T, Exile cards from your graveyard equal to the number of opponents you have: Target creature card in your graveyard gains encore until end of turn. The encore cost is equal to its mana cost. (Exile the creature card and pay its mana cost: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  int *graveyard;
  int grave_source;
  int saved_iid_or_parent_card;
  int parent_player;
  int result_or_grave_index;
  int legacy_card;
  card_instance_t *legacy;
  card_instance_t *instance;
  test_definition_t test;
  test_definition_t test2;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    new_default_test_definition(&test, 2, "Select target creature card.");
    test.ai_selection_mode = 0x101;
    if (event == EVENT_CAN_ACTIVATE)
    {

      result_or_grave_index = generic_activated_ability_with_target_in_grave(
          player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, 0, 0, 0, 0, 0, 0, 0, NULL, NULL,
          TYPE_NONE, &test, 8);
      if (result_or_grave_index == 0)
      {
        return 0;
      }
      if (player != 0)
      {
        saved_iid_or_parent_card = count_graveyard(player);
        if (saved_iid_or_parent_card < 2)
        {
          return 0;
        }
        return result_or_grave_index;
      }
      return result_or_grave_index;
    }
    if (event == EVENT_ACTIVATE)
    {

      new_default_test_definition(&test2, 0xff, "Select a card to exile.");
      test2.ai_selection_mode = 0x303;
      test.id = 0x388;
      test.id_flag = 1;
      result_or_grave_index = new_select_a_card(player, player, 2, 0, 3, -1, &test2);
      if (result_or_grave_index == -1)
      {
        return 0;
      }

      saved_iid_or_parent_card = turn_card_in_grave_face_down(player, result_or_grave_index);
      if (new_special_count_grave(player, &test) == 0)
      {
        spell_fizzled = 1;
      }
      else
      {
        generic_activated_ability_with_target_in_grave(player, card, EVENT_ACTIVATE, GAA_UNTAPPED,
                                                       0, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE,
                                                       &test, 8);
      }
      turn_card_in_grave_face_up(player, result_or_grave_index, saved_iid_or_parent_card);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      rfg_card_from_grave(player, result_or_grave_index);
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
    new_default_test_definition(&test, 2, "Select target creature card.");
  }
  test.ai_selection_mode = 0x101;

  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result_or_grave_index = validate_target_from_grave_source(player, card, player, 0),
       result_or_grave_index != -1))
  {

    saved_iid_or_parent_card = instance->parent_card;
    parent_player = instance->parent_controller;
    if (result_or_grave_index < 0)
    {
      backtrace_and_abort("cards/commander_legends.c:539\ngraveyard_pos >= 0");
    }
    legacy_card =
        create_legacy_activate(parent_player, saved_iid_or_parent_card, encore_granted_legacy);
    legacy = get_card_instance(parent_player, legacy_card);
    graveyard = graveyard_ptr_mutable[parent_player];
    legacy->targets[0].card = graveyard[result_or_grave_index];
    grave_source = graveyard_source[parent_player][result_or_grave_index];
    legacy->targets[1].card = result_or_grave_index;
    legacy->targets[1].player = grave_source;
    legacy->targets[2].card = parent_player;
    legacy->targets[3].player = parent_player;
    legacy->targets[3].card = saved_iid_or_parent_card;
    create_card_name_legacy(parent_player, legacy_card,
                            (unsigned int)cards_data[graveyard[result_or_grave_index]].id);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a3c10; original cards/commander_legends.c:606. */
int card_archelos_lagoon_mystic(int player, int card, event_t event)
{
  /* Archelos, Lagoon Mystic	|1|B|G|U
   * Legendary Creature - Turtle Shaman 2/4
   * As long as Archelos is tapped, other permanents enter tapped.
   * As long as Archelos is untapped, other permanents enter untapped. */

  card_instance_t *instance;
  int result;

  check_legend_rule(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    card_flags[player] = card_flags[player] | CFLAGS_ARCHELOS;
    return 0;
  }
  if ((((event == EVENT_CAST_SPELL) || (event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE)) &&
       ((card != affected_card || (player != affected_card_controller)))) &&
      (((instance = in_play(player, card),
         instance != NULL &&
             (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
        (result = is_what(affected_card_controller, affected_card, 0x47), result != 0))))
  {
    result = is_tapped(player, card);
    if (result == 0)
    {
      result = check_state(affected_card_controller, affected_card, 0x10);
      if (result != 0)
      {
        remove_state(affected_card_controller, affected_card, 0x10);
      }
    }
    else
    {
      add_state(affected_card_controller, affected_card, 0x10);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a3d50; original cards/commander_legends.c:633. */
int card_archon_of_coronation(int player, int card, event_t event)
{
  /* Archon of Coronation	|4|W|W
   * Creature - Archon 5/5
   * Flying
   * When this creature enters, you become the monarch.
   * As long as you're the monarch, damage doesn't cause you to lose life. (When a creature deals combat damage to you, its controller still becomes the monarch.) */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    you_become_the_monarch(player);
    return 0;
  }
  if (event != EVENT_STATIC_EFFECTS)
  {
    return 0;
  }
  card_flags[player] = card_flags[player] | CFLAGS_ARCHON_OF_CORONATION;
  return 0;
}

/* ManalinkEh 2022: 0x25a1f80; original cards/commander_legends.c:653. */
static char *aura_or_equipment(int who_chooses, int player, int card)
{
  int result;
  result = has_subtype(player, card, SUBTYPE_EQUIPMENT);
  if ((result != 0) && (result = is_what(player, card, 2), result == 0))
  {
    return NULL;
  }
  result = has_subtype(player, card, SUBTYPE_AURA);
  if (result == 0)
  {
    return "must be an Equipment or Aura.";
  }
  if ((player == 1) && (result = has_subtype(1, card, SUBTYPE_AURA_PERMANENT), result == 0))
  {
    return "must be an Equipment or Aura.";
  }
  return NULL;
}

/* ManalinkEh 2022: 0x25a1dc0; original cards/commander_legends.c:664. */
static char *aura_enchanting_player(int who_chooses, int player, int card)
{
  int result;
  card_instance_t *instance;

  result = is_what(player, card, 4);

  if (((result != 0) && (instance = in_play(player, card), instance->damage_target_player != -1)) &&
      (instance->damage_target_card == -1))
  {
    return NULL;
  }
  return "must be an Equipment or Aura.";
}

/* ManalinkEh 2022: 0x25a3dd0; original cards/commander_legends.c:675. */
int card_ardenn_intrepid_archaeologist(int player, int card, event_t event)
{
  /* Ardenn, Intrepid Archaeologist	|2|W
   * Legendary Creature - Kor Scout 2/2
   * At the beginning of combat on your turn, you may attach any number of Auras and Equipment you control to target permanent or player.
   * Partner (You can have two commanders if both have partner.) */

  int target_player;
  card_instance_t *instance;
  card_instance_t *aura;
  int target_card;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (beginning_of_combat(player, card, event, player) != 0)
  {

    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0x147);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.preferred_controller = player;
    if (new_pick_target(&td, "Select target permanent or player.", 0, 0x80) != 0)
    {

      target_player = instance->targets[0].player;
      instance->number_of_targets = '\0';
      target_card = instance->targets[0].card;
      if (target_card == -1)
      {
        default_target_definition(player, card, &td, 0x47);
        td.preferred_controller = player;
        td.allowed_controller = player;
        td.illegal_abilities = 0;
        td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
        td.extra = (intptr_t)aura_enchanting_player;
        while (true)
        {
          if (can_target(&td) == 0)
            break;
          if (new_pick_target(&td, "Select an Aura you control enchanting a player.", 0, 0x80) == 0)
            break;

          state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
          aura = get_card_instance(instance->targets[0].player, instance->targets[0].card);
          aura->targets[0].player = target_player;
          aura->targets[4].player = target_player;
          aura->damage_target_player = (int8_t)target_player;
          aura->targets[0].card = -1;
          aura->targets[4].card = -1;
          aura->damage_target_card = -1;
          instance->number_of_targets = '\0';
        }
      }
      else
      {
        default_target_definition(player, card, &td, 0x47);
        td.preferred_controller = player;
        td.allowed_controller = player;
        td.illegal_abilities = 0;
        td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
        td.extra = (intptr_t)aura_or_equipment;
        td.allow_cancel = 2;
        while (true)
        {
          if (can_target(&td) == 0)
            break;
          if (new_pick_target(&td, "Select an Aura or Equipment", 0, 0x80) == 0)
            break;
          state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
          if (has_subtype(instance->targets[0].player, instance->targets[0].card,
                          SUBTYPE_EQUIPMENT) == 0)
          {

            set_special_flags(instance->targets[0].player, instance->targets[0].card, 0x20800000);
            aura = get_card_instance(instance->targets[0].player, instance->targets[0].card);
            aura->targets[0].player = target_player;
            aura->targets[0].card = target_card;
            if (call_card_function(instance->targets[0].player, instance->targets[0].card,
                                   EVENT_MOVE_AURA) != 0)
            {
              call_card_function(instance->targets[0].player, instance->targets[0].card,
                                 EVENT_RESOLVE_MOVING_AURA);
            }
            remove_special_flags(instance->targets[0].player, instance->targets[0].card,
                                 0x20800000);
          }
          else
          {
            if (is_what(target_player, target_card, 2) != 0)
            {
              equip_target_creature(instance->targets[0].player, instance->targets[0].card,
                                    target_player, target_card);
            }
          }
          instance->number_of_targets = '\0';
        }
      }
      remove_untargettable_from_all();
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a4100; original cards/commander_legends.c:755. */
int card_armix_filigree_thrasher(int player, int card, event_t event)
{
  /* Armix, Filigree Thrasher	|2|B
   * Legendary Artifact Creature - Golem 3/2
   * Whenever Armix attacks, you may discard a card. When you do, target creature defending player controls gets -X/-X until end of turn, where X is the number of artifacts you control plus the number of artifact cards in your graveyard.
   * Partner (You can have two commanders if both have partner.) */

  int value;
  card_instance_t *instance;
  int count;
  target_definition_t td;
  test_definition_t test;

  check_legend_rule(player, card, event);
  value = duh_mode(player);
  dat_ability(player, card, event, (value != 0) + DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
  if (event == EVENT_DAT_ABILITY)
  {

    instance = get_card_instance(player, card);
    new_default_test_definition(&test, 0xff, "Select a card to discard.");
    value = new_select_a_card(player, player, 4, 0, 3, -1, &test);
    if (value != -1)
    {
      new_discard_card(player, value, 0, player);
      default_target_definition(player, card, &td, 2);
      td.allow_cancel = 0;
      if (can_target(&td) != 0)
      {
        if (new_pick_target(&td, "Select target creature opponent controls.", 0, 0x80) != 0)
        {

          value = count_subtype(player, 0x40, -1);
          count = count_graveyard_by_type(player, 0x40);
          pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                         -(count + value), -(count + value));
        }
      }
      instance->number_of_targets = '\0';
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a0670; original cards/commander_legends.c:787. */
static int is_aura_or_equipment_by_iid(int iid, intptr_t unused, int player, int card)
{
  int result;

  result = is_what(-1, iid, 0x40);
  if (result != 0)
  {
    result = has_subtype_by_id(player, (unsigned int)cards_data[iid].id, SUBTYPE_EQUIPMENT);
    if (result != 0)
    {
      return 1;
    }
  }
  result = is_what(-1, iid, 4);
  if (result == 0)
  {
    return 0;
  }
  result = has_subtype_by_id(player, (unsigned int)cards_data[iid].id, SUBTYPE_AURA);
  return (unsigned int)(result != 0);
}

/* ManalinkEh 2022: 0x25a42c0; original cards/commander_legends.c:797. */
int card_armored_skyhunter(int player, int card, event_t event)
{
  /* Armored Skyhunter	|3|W
   * Creature - Cat Knight 3/3
   * Flying
   * Whenever this creature attacks, look at the top six cards of your library. You may put an Aura or Equipment card from among them onto the battlefield. If an Equipment is put onto the battlefield this way, you may attach it to a creature you control. Put the rest of those cards on the bottom of your library in a random order. */

  card_instance_t *instance;
  int card_00;
  int result;
  target_definition_t td;
  test_definition_t test;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  new_default_test_definition(&test, 0x47, "Select an Aura or Equipment card.");
  test.special_selection_function = is_aura_or_equipment_by_iid;
  card_00 = reveal_top_cards_of_library_and_choose(player, player, player, 6, 0, TUTOR_PLAY, 1,
                                                   TUTOR_BOTTOM_OF_DECK_RANDOM, 0, &test);
  if (-1 < card_00)
  {
    result = has_subtype(player, card_00, SUBTYPE_EQUIPMENT);
    if (result != 0)
    {
      default_target_definition(player, card_00, &td, 2);
      td.preferred_controller = player;
      td.allowed_controller = player;
      result = can_target(&td);
      if (result != 0)
      {
        result = new_pick_target(&td, "Select target creature to attach the Equipment.", 0, 0x80);
        if (result != 0)
        {
          equip_target_creature(player, card_00, instance->targets[0].player,
                                instance->targets[0].card);
        }
      }
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x25a4450; original cards/commander_legends.c:841. */
int card_azure_fleet_admiral(int player, int card, event_t event)
{
  /* Azure Fleet Admiral	|3|U
   * Creature - Human Pirate 3/3
   * When this creature enters, you become the monarch.
   * This creature can't be blocked by creatures the monarch controls. */

  card_instance_t *instance;
  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    if (event == EVENT_BLOCK_LEGALITY)
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {
          result = who_is_the_monarch();
          if (result != player)
          {
            event_result = 1;
          }
        }
      }
    }
    return 0;
  }
  you_become_the_monarch(player);
  return 0;
}

/* ManalinkEh 2022: 0x25a44f0; original cards/commander_legends.c:861. */
int card_belbe_corrupted_observer(int player, int card, event_t event)
{
  /* Belbe, Corrupted Observer	|B|G
   * Legendary Creature - Phyrexian Zombie Elf 2/2
   * At the beginning of each postcombat main phase, the active player adds |C|C for each of your opponents who lost life this turn. (Damage causes loss of life.) */

  int result;
  card_instance_t *instance;

  check_legend_rule(player, card, event);
  result = beginning_of_phase(player, card, event, 2, -1, 0x1e);
  if (result != 0)
  {
    result = get_trap_condition(1 - player, 2);
    if (result != 0)
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {
          produce_mana(current_turn, 0, 2);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a45b0; original cards/commander_legends.c:877. */
int card_bell_borca_spectral_sergeant(int player, int card, event_t event)
{
  /* Bell Borca, Spectral Sergeant	|2|R|W
   * Legendary Creature - Spirit Soldier * /5
   * Note the mana value of each card as it's put into exile.
   * Bell Borca's power is equal to the greatest number noted for it this turn.
   * At the beginning of your upkeep, exile the top card of your library. You may play that card this turn. */

  unsigned short exiled_csvid;
  card_instance_t *instance;
  int mana_value;
  unsigned int newval;
  inst_info_t exile_record;

  check_legend_rule(player, card, event);
  if (event == EVENT_CARD_TO_EXILE)
  {

    mana_value = get_cmc_by_internal_id(affected_card);
    instance = get_card_instance(player, card);
    exile_record = inst_info(instance, 0x5642);
    if ((int)(((uint32_t)exile_record.id | ((uint32_t)exile_record.data2 << 16)) >> 0x10) <
        mana_value)
    {
      set_inst_info(instance, 0x5642, 0, (uint16_t)mana_value);
    }
    upkeep_trigger_ability(player, card, EVENT_CARD_TO_EXILE, player);
    return 0;
  }
  if (event != EVENT_POWER)
  {
    upkeep_trigger_ability(player, card, event, player);
    if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
    {
      if (event == EVENT_CLEANUP)
      {

        instance = get_card_instance(player, card);
        clear_info(instance, 0x5642);
      }
      return 0;
    }

    exiled_csvid = cards_data[*deck_ptr[player]].id;
    rfg_top_card_of_deck(player);
    create_may_play_card_from_exile_effect(player, card, player, (unsigned int)exiled_csvid,
                                           MPCFE_UNTIL_EOT);
    return 0;
  }
  if ((card == affected_card) && (player == affected_card_controller))
  {

    if ((player < 0) || (card < 0))
    {
      newval = 0;
    }
    else
    {

      instance = get_card_instance(player, card);
      exile_record = inst_info(instance, 0x5642);
      newval = ((uint32_t)exile_record.id | ((uint32_t)exile_record.data2 << 16)) >> 0x10;
    }
    set_base_pt(newval);
  }
  upkeep_trigger_ability(player, card, EVENT_POWER, player);
  return 0;
}

/* ManalinkEh 2022: 0x25a4770; original cards/commander_legends.c:924. */
int card_biowaste_blob(int player, int card, event_t event)
{
  /* Biowaste Blob	|2|G|G
   * Creature - Ooze 0/0
   * Oozes you control get +1/+1.
   * At the beginning of your upkeep, if you control a commander, create a token that's a copy of this creature. */

  int result;
  token_generation_t token;

  if ((trigger_condition == TRIGGER_UPKEEP) && (current_turn == player))
  {
    result = liutenant(player);
    if (result != 0)
    {
      upkeep_trigger_ability(player, card, event, player);
    }
  }
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    result = liutenant(player);
    if (result != 0)
    {
      result = get_id(player, card);
      default_token_definition(player, card, result, &token);
      generate_token(&token);
    }
  }
  boost_creature_type(player, card, event, SUBTYPE_OOZE, 1, 1, 0,
                      BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  return 0;
}

/* ManalinkEh 2022: 0x25a4850; original cards/commander_legends.c:947. */
int card_bladegriff_prototype(int player, int card, event_t event)
{
  /* Bladegriff Prototype	|5
   * Artifact Creature - Griffin 3/2
   * Flying
   * Whenever this creature deals combat damage to a player, destroy target nonland permanent of that player's choice that one of your opponents controls. */

  if (!damage_dealt_by_me(player, card, event,
                           DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                               DDBM_MUST_BE_COMBAT_DAMAGE))
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  unsigned int packed_damage = instance->targets[1].player;
  instance->targets[1].player = 0;
  int times_damaged[2] = { packed_damage & 0xff, (packed_damage >> 8) & 0xff };
  int opponent = 1 - player;
  for (int p = 0; p != 2; ++p)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x47);
    td.illegal_type = TYPE_LAND;
    td.allow_cancel = 0;
    int remaining = times_damaged[p];
    td.allowed_controller = opponent;
    td.preferred_controller = opponent;
    td.who_chooses = p;
    while (remaining != 0 && can_target(&td))
    {
      if (opponent == p)
        new_pick_target(&td, "Select target nonland permanent you control.", 0, 0x80);
      else
        new_pick_target(&td, "Select target nonland permanent your opponent controls.", 0, 0x80);
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      --remaining;
      instance->number_of_targets = 0;
      times_damaged[p] = remaining;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a49c0; original cards/commander_legends.c:984. */
int card_blazing_sunsteel(int player, int card, event_t event)
{
  /* Blazing Sunsteel	|1|R
   * Artifact - Equipment
   * Equipped creature gets +1/+0 for each opponent you have.
   * Whenever equipped creature is dealt damage, it deals that much damage to any target.
   * Equip |4 */

  card_instance_t *instance;
  card_instance_t *equipped;
  int damage_count_or_result;
  card_instance_t *damage;
  int i;
  int target_available;
  int dam[10];
  target_definition_t td;
  char prompt[100];

  equipped = is_equipping(player, card);
  if ((equipped != NULL) &&
      (damage_count_or_result = is_humiliated(player, card, AL_TRIG), damage_count_or_result == 0))
  {

    instance = get_card_instance(player, card);
    damage = damage_being_dealt(event);
    if ((damage != NULL) &&
        (((damage->damage_target_card == instance->damage_target_card &&
           (damage->damage_target_player == instance->damage_target_player)) &&
          (damage_count_or_result = instance->info_slot, damage_count_or_result < 8))))
    {
      i = damage_count_or_result + 1;
      if (damage_count_or_result < 1)
      {
        i = 2;
        damage_count_or_result = 1;
      }
      instance->info_slot = i;
      instance->targets[damage_count_or_result].player = damage->info_slot;
    }
    if (((trigger_condition == TRIGGER_DEAL_DAMAGE) && (card == affected_card)) &&
        ((player == affected_card_controller &&
          ((reason_for_trigger_controller == player && (1 < instance->info_slot))))))
    {
      if (event == EVENT_TRIGGER)
      {
        event_result = event_result | 2;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {

        default_target_definition(player, card, &td, 0x102);
        td.illegal_abilities =
            get_protections_from((int)instance->damage_target_player, instance->damage_target_card);
        prompt[0] = '\0';
        damage_count_or_result = instance->info_slot;
        td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
        td.allow_cancel = 0;
        if (damage_count_or_result < 2)
        {
          instance->info_slot = 1;
        }
        else
        {
          i = 1;
          do
          {
            dam[i] = instance->targets[i].player;
            i = i + 1;
          } while (i != damage_count_or_result);
          instance->info_slot = 1;
          i = 1;
          do
          {
            if (ai_is_speculating != 1)
            {
              load_text(NULL, "TARGET_ANY");
              scnprintf(prompt, 100, "%s (%d damage)", text_lines[0], dam[i]);
            }
            instance->number_of_targets = '\0';
            target_available = can_target(&td);
            if ((target_available != 0) &&
                (target_available = pick_next_target_noload(&td, prompt), target_available != 0))
            {
              damage_creature(instance->targets[0].player, instance->targets[0].card, dam[i],
                              (int)instance->damage_target_player, instance->damage_target_card);
            }
            i = i + 1;
          } while (i != damage_count_or_result);
        }
      }
    }
  }
  return vanilla_equipment(player, card, event, 4, 1, 0, 0, 0);
}

/* ManalinkEh 2022: 0x25a4c60; original cards/commander_legends.c:1053. */
int card_blim_comedic_genius(int player, int card, event_t event)
{
  /* Blim, Comedic Genius	|2|B|R
   * Legendary Creature - Imp 4/3
   * Flying
   * Whenever Blim deals combat damage to a player, that player gains control of target permanent you control. Then each player loses life and discards cards equal to the number of permanents they control but don't own. */

  unsigned int packed_damage_counts;
  int count;
  card_instance_t *instance;
  int *damage_count;
  int times_damaged[2];
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (damage_dealt_by_me(player, card, event,
                         DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                             DDBM_MUST_BE_COMBAT_DAMAGE) != 0)
  {

    instance = get_card_instance(player, card);
    packed_damage_counts = instance->targets[1].player;
    instance->targets[1].player = 0;
    times_damaged[1] = packed_damage_counts >> 8 & 0xff;
    times_damaged[0] = packed_damage_counts & 0xff;
    damage_count = times_damaged;
    do
    {
      default_target_definition(player, card, &td, 0x47);
      td.allow_cancel = 0;
      count = *damage_count;
      td.preferred_controller = player;
      td.allowed_controller = player;
      while ((count != 0 && (can_target(&td) != 0)))
      {
        new_pick_target(&td, "Select target permanent you control.", 0, 0x80);
        give_control_of_self(instance->targets[0].player, instance->targets[0].card);
        count = count + -1;
        instance->number_of_targets = '\0';
        *damage_count = count;
      }
      damage_count = damage_count + 1;
    } while (damage_count != times_damaged + 2);

    count = count_permanents_you_control_but_you_dont_own(0, 0x47);
    lose_life(0, count);
    new_multidiscard(0, count, 2, player);
    count = count_permanents_you_control_but_you_dont_own(1, 0x47);
    lose_life(1, count);
    new_multidiscard(1, count, 2, player);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a4e20; original cards/commander_legends.c:1098. */
int card_body_of_knowledge(int player, int card, event_t event)
{
  /* Body of Knowledge	|3|U|U
   * Creature - Avatar * / *
   * ~'s power and toughness are each equal to the number of cards in your hand.
   * You have no maximum hand size.
   * Whenever this creature is dealt damage, draw that many cards. */

  int result;
  card_instance_t *instance;

  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    set_base_pt(hand_count[player]);
  }
  result = damage_dealt_to_me_arbitrary(player, card, event, DDBM_0, player, card);
  if (result != 0)
  {
    instance = get_card_instance(player, card);
    draw_cards(player, instance->targets[7].card);
    instance = get_card_instance(player, card);
    instance->targets[7].card = 0;
  }
  result = card_spellbook(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25a4ee0; original cards/commander_legends.c:1118. */
int card_breeches_brazen_plunderer(int player, int card, event_t event)
{
  /* Breeches, Brazen Plunderer	|3|R
   * Legendary Creature - Goblin Pirate 3/3
   * Menace
   * Whenever one or more Pirates you control deal damage to your opponents, exile the top card of each of those opponents' libraries. You may play those cards this turn, and you may spend mana as though it were mana of any color to cast those spells.
   * Partner (You can have two commanders if both have partner.) */

  int result;
  test_definition_t this_test;
  check_legend_rule(player, card, event);
  result = subtype_deals_damage(player, card, event, player, 0x95, 0x12);
  if (result == 0)
  {
    return 0;
  }
  default_test_definition(&this_test, 0xff);
  exile_top_x_card_you_may_play_them(player, card, 1, &this_test, 1 - player,
                                     MPCFE_UNTIL_EOT | MPCFE_FOR_CMC);
  return 0;
}

/* ManalinkEh 2022: 0x25a4f90; original cards/commander_legends.c:1138. */
int card_briarblade_adept(int player, int card, event_t event)
{
  /* Briarblade Adept	|4|B
   * Creature - Elf Assassin 3/4
   * Whenever this creature attacks, target creature an opponent controls gets -1/-1 until end of turn.
   * Encore |3|B (|3|B, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_BRIARBLADE_ADEPT = 22091
  };

  int result;
  card_instance_t *instance;
  target_definition_t td;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {

    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    td.allow_cancel = 0;
    td.preferred_controller = td.allowed_controller;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "Select target creature controlled by an opponent.", 0, 0x80);
      if (result != 0)
      {
        pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -1,
                       -1);
      }
    }
    instance->number_of_targets = '\0';
  }
  result = encore(player, card, event, CARD_ID_BRIARBLADE_ADEPT, 3, 1, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x25a0730; original cards/commander_legends.c:1163. */
static void brinelin_ability(int player, int card)
{
  int result;
  card_instance_t *instance;
  target_definition_t td1;

  default_target_definition(player, card, &td1, 0x47);
  td1.illegal_type = TYPE_LAND;
  result = can_target(&td1);
  if (result != 0)
  {
    result = new_pick_target(&td1, "Select target nonland permanent.", 0, 0x80);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      bounce_permanent(instance->targets[0].player, instance->targets[0].card);
    }
  }
  instance = get_card_instance(player, card);
  instance->number_of_targets = '\0';
  return;
}

/* ManalinkEh 2022: 0x25a50e0; original cards/commander_legends.c:1177. */
int card_brinelin_the_moon_kraken(int player, int card, event_t event)
{
  /* Brinelin, the Moon Kraken	|6|U|U
   * Legendary Creature - Kraken 6/8
   * When Brinelin enters and whenever you cast a spell with mana value 6 or greater, you may return target nonland permanent to its owner's hand.
   * Partner (You can have two commanders if both have partner.) */

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
    brinelin_ability(player, card);
  if (trigger_condition == TRIGGER_SPELL_CAST && trigger_cause_controller == player)
  {
    test_definition_t test;
    default_test_definition(&test, 0xff);
    test.cmc = 5;
    test.cmc_flag = 2;
    sp_ability_trigger(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA)
    brinelin_ability(player, card);
  return 0;
}

/* ManalinkEh 2022: 0x25a51e0; original cards/commander_legends.c:1207. */
int card_captain_vargus_wrath(int player, int card, event_t event)
{
  /* Captain Vargus Wrath	|U|R
   * Legendary Creature - Orc Pirate 1/1
   * Whenever ~ attacks, Pirates you control get +1/+1 until end of turn for each time you've cast a commander from the command zone this game. */

  int power;

  check_legend_rule(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }

  power = count_how_many_times_a_commander_was_played(player);
  pump_subtype_until_eot(player, card, player, 0x95, power, power, 0, 0);
  return 0;
}

/* ManalinkEh 2022: 0x25a5280; original cards/commander_legends.c:1224. */
int card_coastline_marauders(int player, int card, event_t event)
{
  /* Coastline Marauders	|2|R
   * Creature - Human Pirate 0/3
   * Trample
   * Whenever this creature attacks, it gets +1/+0 until end of turn for each land defending player controls.
   * Encore |4|R|R (|4|R|R, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_COASTLINE_MARAUDERS = 22094
  };

  int result;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {

    result = count_permanents_by_type(1 - player, TYPE_LAND);
    pump_until_eot(player, card, player, card, result, 0);
  }
  result = encore(player, card, event, CARD_ID_COASTLINE_MARAUDERS, 4, 0, 0, 0, 2, 0);
  return result;
}

/* ManalinkEh 2022: 0x25a5350; original cards/commander_legends.c:1241. */
int card_coercive_recruiter(int player, int card, event_t event)
{
  /* Coercive Recruiter	|4|R
   * Creature - Orc Pirate 4/3
   * Whenever this creature or another Pirate you control enters, gain control of target creature until end of turn. Untap that creature. Until end of turn, it gains haste and becomes a Pirate in addition to its other types. */

  card_instance_t *instance;
  int result;


  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    target_definition_t td1;

    default_target_definition(player, card, &td1, 2);
    instance = get_card_instance(player, card);
    result = can_target(&td1);
    if (result != 0)
    {
      result = new_pick_target(&td1, "TARGET_CREATURE", 0, 0);
      if (result != 0)
      {

        result = add_a_subtype_until_eot(player, card, instance->targets[0].player,
                                         instance->targets[0].card, SUBTYPE_PIRATE);
        add_status(player, result, 0x10000);
        effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
      }
    }
    instance->number_of_targets = '\0';
  }
  if ((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (trigger_cause_controller == player))
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 0x47);
    this_test.not_me = 1;
    this_test.subtype = SUBTYPE_PIRATE;
    etb_ability_global(player, event, 2, TYPE_NONE, &this_test, player, card);
  }
  return 0;
}

#ifndef SHANDALAR
static int store_iid_and_tou(card_instance_t *inst, int16_t stored_iid, int16_t tou, int pos)
{
  for (; pos < 10; ++pos)
  {
    if (inst->targets[pos].player == -1)
    {
      inst->targets[pos].player = stored_iid;
      inst->targets[pos].card = tou;
      return 1;
    }
  }
  return 0;
}
#endif

#ifndef SHANDALAR
static int retrieve_stored_iids_and_tou(card_instance_t *inst, int16_t *iids, int16_t *tou, int pos)
{
  int count = 0;
  for (; pos < 10; ++pos)
    if (inst->targets[pos].player != -1)
    {
      iids[count] = (int16_t)inst->targets[pos].player;
      tou[count] = (int16_t)inst->targets[pos].card;
      ++count;
    }
  return count;
}
#endif

#ifndef SHANDALAR
static void clear_stored_iids_and_tou(card_instance_t *inst, int pos)
{
  for (; pos < 10; ++pos)
  {
    inst->targets[pos].card = -1;
    inst->targets[pos].player = -1;
  }
}
#endif

/* ManalinkEh 2022: 0x25a0850; original cards/commander_legends.c:1305. */
static int colfenor_effect(int player, int card, event_t event)
{
  int iid;
  int csvid;
  short dying_iid;
  card_instance_t *instance;
  int value;
  card_instance_t *dying;
  unsigned int id;
  int death_count;
  int death_index;
  int attempts_remaining;
  int16_t iids[10];
  int16_t tou[10];
  int allowed[10][3];
  int csvids[10][3];
  test_definition_t test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (value = get_id((int)instance->damage_target_player, instance->damage_target_card),
           value != 0x5650))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (value = effect_follows_control_of_attachment(player, card, 0x4700), value != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (value = effect_follows_control_of_attachment(player, card, event), value != 0))
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
        if ((((player == affected_card_controller) &&
              (value = is_what(player, affected_card, 2), value != 0)) &&
             (value = is_this_dying(affected_card_controller, affected_card), value != 0)) &&
            (value = check_for_death_trigger_removal(affected_card_controller, affected_card),
             value == 0))
        {

          instance = get_card_instance(player, card);
          if (instance->targets[0xb].player < 0)
          {
            instance->targets[0xb].player = 0;
          }
          *(char *)&instance->targets[0xb].player = (char)instance->targets[0xb].player + '\x01';
          value = get_toughness(affected_card_controller, affected_card);
          if ((short)value != 0)
          {

            iid = get_original_internal_card_id(affected_card_controller, affected_card);

            int death_slot = 1;
            do
            {
              if (instance->targets[death_slot].player == -1)
              {
                instance->targets[death_slot].player = (int)(short)iid;
                instance->targets[death_slot].card = (int)(short)value;
                break;
              }
              death_slot = death_slot + 1;
            } while (death_slot != 10);
          }
        }
      }
    }
  }
  value = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_GRAVEYARD_FROM_PLAY && value == 0)
    return 0;
  if (value != 0)
  {
    value = check_status(player, card, 0x10000);

    if (value == 0)
    {
      instance = get_card_instance(player, card);

      death_count = 0;
      for (int slot = 1; slot < 10; ++slot)
      {
        if (instance->targets[slot].player != -1)
        {
          iids[death_count] = (int16_t)instance->targets[slot].player;
          tou[death_count] = (int16_t)instance->targets[slot].card;
          ++death_count;
        }
      }
      for (int slot = 1; slot < 10; ++slot)
      {
        instance->targets[slot].card = -1;
        instance->targets[slot].player = -1;
      }
      if (0 < death_count)
      {
        for (int death = 0; death < death_count; ++death)
        {
          unmeld((unsigned int)cards_data[iids[death]].id,
                 &csvids[death][1], &csvids[death][2]);
          int first_count = count_graveyard_by_id(player, csvids[death][1]);
          allowed[death][1] = first_count - 1;
          if (csvids[death][2] == -1)
            allowed[death][2] = 0;
          else
          {
            int second_count = count_graveyard_by_id(player, csvids[death][2]);
            allowed[death][2] = second_count - 1;
          }
        }

        death_index = 0;
        do
        {
          dying_iid = iids[death_index];
          id = (unsigned int)cards_data[dying_iid].id;
          default_test_definition(&test, 2);
          test.toughness_flag = 3;
          test.id_flag = 1;
          int toughness_limit = (int)tou[death_index];
          attempts_remaining = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
          test.toughness = toughness_limit;
          do
          {

            if (csvids[death_index][2] == -1)
            {
              if (allowed[death_index][1] == 0 && is_id_in_grave(player, id) &&
                  get_base_toughness_iid(player, (int)dying_iid) < toughness_limit)
                scnprintf(test.message, 100,
                          " Choose a creature card with toughness less than %d. (not %s)", toughness_limit,
                          cards_ptr[id]->name);
              else
                scnprintf(test.message, 100, " Choose a creature card with toughness less than %d.",
                          toughness_limit);
              test.id = id;
            }
            else
            {
              int first_iid = get_internal_card_id_from_csv_id(csvids[death_index][1]);
              int second_iid = get_internal_card_id_from_csv_id(csvids[death_index][2]);
              bool check_second = true;
              if (allowed[death_index][1] == 0)
              {
                int first_toughness = get_base_toughness_iid(player, first_iid);
                if (first_toughness < toughness_limit && is_id_in_grave(player, csvids[death_index][1]) &&
                    allowed[death_index][2] == 0 && get_base_toughness_iid(player, second_iid) < toughness_limit &&
                    is_id_in_grave(player, csvids[death_index][2]))
                {
                  scnprintf(test.message, 100,
                            " Choose a creature card with toughness less than %d. (not %s or %s)",
                            toughness_limit, cards_ptr[csvids[death_index][1]]->name, cards_ptr[csvids[death_index][2]]->name);
                  test.id = csvids[death_index][1];
                  test.id2 = csvids[death_index][2];
                  check_second = false;
                }
                else
                {
                  first_toughness = get_base_toughness_iid(player, first_iid);
                  if (toughness_limit > first_toughness && is_id_in_grave(player, csvids[death_index][1]))
                  {
                    scnprintf(test.message, 100,
                              " Choose a creature card with toughness less than %d. (not %s)",
                              toughness_limit, cards_ptr[csvids[death_index][1]]->name);
                    test.id = csvids[death_index][1];
                    check_second = false;
                  }
                }
              }
              if (check_second)
              {
                if (allowed[death_index][2] == 0 && get_base_toughness_iid(player, second_iid) < toughness_limit &&
                    is_id_in_grave(player, csvids[death_index][2]))
                {
                  scnprintf(test.message, 100,
                            " Choose a creature card with toughness less than %d. (not %s)", toughness_limit,
                            cards_ptr[csvids[death_index][2]]->name);
                  test.id = csvids[death_index][2];
                }
                else
                  scnprintf(test.message, 100,
                            " Choose a creature card with toughness less than %d.", toughness_limit);
              }
            }
            int selected;
            if (new_special_count_grave(player, &test) != 0 &&
                (selected = new_global_tutor(player, player, 2, 1, 0, 1, &test), selected >= 0))
            {
              unsigned int selected_csvid = get_id(player, selected);
              if (selected_csvid == id)
              {
                for (int later = death_index; later < death_count; ++later)
                  if (iids[later] == dying_iid)
                    --allowed[later][1];
              }
              else
              {
                csvid = get_id(player, selected);
                int first_csvid = csvids[death_index][1];
                if (csvid == first_csvid)
                {
                  for (int later = death_index; later < death_count; ++later)
                    if (csvids[later][1] == first_csvid)
                      --allowed[later][1];
                }
                else
                {
                  int selected_again = get_id(player, selected);
                  int second_csvid = csvids[death_index][2];
                  if (selected_again == second_csvid)
                    for (int later = death_index; later < death_count; ++later)
                      if (csvids[later][2] == second_csvid)
                        --allowed[later][2];
                }
              }
            }

            attempts_remaining = attempts_remaining + -1;
          } while (attempts_remaining != 0);
          death_index = death_index + 1;
        } while (death_index != death_count);
      }
      kill_card(player, card, KILL_EXILE);
    }
    else
    {

      instance = get_card_instance(player, card);

      for (int slot = 1; slot < 10; ++slot)
      {
        instance->targets[slot].card = -1;
        instance->targets[slot].player = -1;
      }
    }
  }
  if ((event == EVENT_CLEANUP) && (value = check_status(player, card, 0x10000), value == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a5500; original cards/commander_legends.c:1450. */
int card_colfenor_the_last_yew(int player, int card, event_t event)
{
  /* Colfenor, the Last Yew	|3|W|B|G
   * Legendary Creature - Treefolk Shaman 3/7
   * Reach, vigilance
   * Whenever Colfenor or another creature you control dies, return up to one other target creature card with lesser toughness from your graveyard to your hand. */

  int i;
  int csvid;
  short dying_iid;
  int value;
  card_instance_t *instance;
  unsigned int id;
  int death_count;
  int death_index;
  int attempts_remaining;
  int16_t iids[10];
  int16_t tou[10];
  int allowed[10][3];
  int csvids[10][3];
  test_definition_t test;

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      i = 0;
      bool found = false;
      if (0 < active_cards_count[player])
      {
        do
        {
          instance = in_play(player, i);

          if ((((instance != NULL) && (death_count = is_what(player, i, 0x80), death_count != 0)) &&
               (instance = get_card_instance(player, i),
                instance->info_slot_fn == colfenor_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            found = true;
            break;
          }
          i = i + 1;
        } while (i < active_cards_count[player]);
      }

      if (!found)
      {
        value = create_targetted_legacy_effect(player, card, colfenor_effect, player, card);
        instance = get_card_instance(player, value);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, value, 0x10000);
      }
    }
  }
  else if (((event == EVENT_GRAVEYARD_FROM_PLAY) && (affected_card_controller == player)) &&
           ((value = is_what(player, affected_card, 2),
             value != 0 && ((value = is_this_dying(affected_card_controller, affected_card),
                             value != 0 && (value = check_for_death_trigger_removal(
                                                affected_card_controller, affected_card),
                                            value == 0))))))
  {

    instance = get_card_instance(player, card);
    if (instance->targets[0xb].player < 0)
    {
      instance->targets[0xb].player = 0;
    }
    *(char *)&instance->targets[0xb].player = (char)instance->targets[0xb].player + '\x01';
    value = get_toughness(affected_card_controller, affected_card);
    if ((short)value != 0)
    {

      int iid = get_original_internal_card_id(affected_card_controller, affected_card);
      int death_slot = 0;

      do
      {
        if (instance->targets[death_slot].player == -1)
        {
          instance->targets[death_slot].player = (int)(short)iid;
          instance->targets[death_slot].card = (int)(short)value;
          break;
        }
        death_slot = death_slot + 1;
      } while (death_slot != 10);
    }
  }
  value = duh_mode(player);
  value = resolve_gfp_ability(player, card, event, (value != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if ((value != 0) && (instance = get_card_instance(player, card), instance->kill_code == '\0'))
  {

    instance = get_card_instance(player, card);

    death_count = 0;
    for (int slot = 0; slot < 10; ++slot)
    {
      if (instance->targets[slot].player != -1)
      {
        iids[death_count] = (int16_t)instance->targets[slot].player;
        tou[death_count] = (int16_t)instance->targets[slot].card;
        ++death_count;
      }
    }
    for (int slot = 0; slot < 10; ++slot)
    {
      instance->targets[slot].card = -1;
      instance->targets[slot].player = -1;
    }
    if (0 < death_count)
    {
      for (int death = 0; death < death_count; ++death)
      {
        unmeld((unsigned int)cards_data[iids[death]].id,
               &csvids[death][1], &csvids[death][2]);
        int first_count = count_graveyard_by_id(player, csvids[death][1]);
        allowed[death][1] = first_count - 1;
        if (csvids[death][2] == -1)
          allowed[death][2] = 0;
        else
        {
          int second_count = count_graveyard_by_id(player, csvids[death][2]);
          allowed[death][2] = second_count - 1;
        }
      }
      death_index = 0;

      do
      {
        dying_iid = iids[death_index];
        id = (unsigned int)cards_data[dying_iid].id;
        default_test_definition(&test, 2);
        test.toughness_flag = 3;
        test.id_flag = 1;
        int toughness_limit = (int)tou[death_index];
        attempts_remaining = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
        test.toughness = toughness_limit;
        do
        {

          if (csvids[death_index][2] == -1)
          {
            if (allowed[death_index][1] == 0 && is_id_in_grave(player, id) &&
                get_base_toughness_iid(player, (int)dying_iid) < toughness_limit)
              scnprintf(test.message, 100,
                        " Choose a creature card with toughness less than %d. (not %s)", toughness_limit,
                        cards_ptr[id]->name);
            else
              scnprintf(test.message, 100, " Choose a creature card with toughness less than %d.",
                        toughness_limit);
            test.id = id;
          }
          else
          {
            int first_iid = get_internal_card_id_from_csv_id(csvids[death_index][1]);
            int second_iid = get_internal_card_id_from_csv_id(csvids[death_index][2]);
            bool check_second = true;
            if (allowed[death_index][1] == 0)
            {
              int first_toughness = get_base_toughness_iid(player, first_iid);
              if (first_toughness < toughness_limit && is_id_in_grave(player, csvids[death_index][1]) &&
                  allowed[death_index][2] == 0 && get_base_toughness_iid(player, second_iid) < toughness_limit &&
                  is_id_in_grave(player, csvids[death_index][2]))
              {
                scnprintf(test.message, 100,
                          " Choose a creature card with toughness less than %d. (not %s or %s)",
                          toughness_limit, cards_ptr[csvids[death_index][1]]->name, cards_ptr[csvids[death_index][2]]->name);
                test.id = csvids[death_index][1];
                test.id2 = csvids[death_index][2];
                check_second = false;
              }
              else
              {
                first_toughness = get_base_toughness_iid(player, first_iid);
                if (toughness_limit > first_toughness && is_id_in_grave(player, csvids[death_index][1]))
                {
                  scnprintf(test.message, 100,
                            " Choose a creature card with toughness less than %d. (not %s)", toughness_limit,
                            cards_ptr[csvids[death_index][1]]->name);
                  test.id = csvids[death_index][1];
                  check_second = false;
                }
              }
            }
            if (check_second)
            {
              if (allowed[death_index][2] == 0 && get_base_toughness_iid(player, second_iid) < toughness_limit &&
                  is_id_in_grave(player, csvids[death_index][2]))
              {
                scnprintf(test.message, 100,
                          " Choose a creature card with toughness less than %d. (not %s)", toughness_limit,
                          cards_ptr[csvids[death_index][2]]->name);
                test.id = csvids[death_index][2];
              }
              else
                scnprintf(test.message, 100, " Choose a creature card with toughness less than %d.",
                          toughness_limit);
            }
          }
          int selected;
          if (new_special_count_grave(player, &test) != 0 &&
              (selected = new_global_tutor(player, player, 2, 1, 0, 1, &test), selected >= 0))
          {
            unsigned int selected_csvid = get_id(player, selected);
            if (selected_csvid == id)
            {
              for (int later = death_index; later < death_count; ++later)
                if (iids[later] == dying_iid)
                  --allowed[later][1];
            }
            else
            {
              csvid = get_id(player, selected);
              int first_csvid = csvids[death_index][1];
              if (csvid == first_csvid)
              {
                for (int later = death_index; later < death_count; ++later)
                  if (csvids[later][1] == first_csvid)
                    --allowed[later][1];
              }
              else
              {
                int selected_again = get_id(player, selected);
                int second_csvid = csvids[death_index][2];
                if (selected_again == second_csvid)
                  for (int later = death_index; later < death_count; ++later)
                    if (csvids[later][2] == second_csvid)
                      --allowed[later][2];
              }
            }
          }

          attempts_remaining = attempts_remaining + -1;
        } while (attempts_remaining != 0);
        death_index = death_index + 1;
      } while (death_index != death_count);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a0360; original cards/commander_legends.c:1588. */
static char *is_commander(int who_chooses, int player, int card, int targeting_player,
                          int targeting_card)
{
  int result;
  char *error;
  result = check_special_flags2(player, card, 0x1000000);
  error = NULL;
  if (result == 0)
  {
    error = "must be a Commander.";
  }
  return error;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25a2780; original cards/commander_legends.c:1595. */
/* The release specializes which_commander to 3 (both commanders). */
static int get_color_identity_of_all_commanders(int player, int which_commander)
{
  unsigned int clr = 0;
  for (int i = 1; i < 3; ++i)
  {
    int c1 = get_commander_id(player, i);
    if (c1 == -1)
      continue;
    unsigned char printed_colors = cards_data[c1].color;
    unsigned int text_colors = get_color_from_remainder_text((unsigned int)cards_data[c1].id);
    clr |= printed_colors | text_colors;
    if (cards_data[c1].id == CARD_ID_THE_PRISMATIC_PIPER)
    {
      int edh_card = locate_edh_card(player);
      if (edh_card >= 0)
      {
        card_instance_t *edh_inst = get_card_instance(player, edh_card);
        int chosen_colors = edh_inst->targets[6].card;
        if (chosen_colors >= 0)
          clr |= chosen_colors;
      }
    }
  }
  return clr;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25a5e60; original cards/commander_legends.c:1616. */
int card_commanders_plate(int player, int card, event_t event)
{
  /* Commander's Plate	|1
   * Artifact - Equipment
   * Equipped creature gets +3/+3 and has protection from each color that's not in your commander's color identity.
   * Equip commander |3
   * Equip |5 */

  int choice;
  card_instance_t *instance;
  int identity_or_result;
  int i;
  card_instance_t *equipped;
  unsigned int commander_equip_available;
  unsigned int normal_equip_available;
  uint32_t data;
  inst_info_t protection_record;
  target_definition_t *equip_target;
  const char *prompt;
  target_definition_t td;
  target_definition_t td1;
  if (event == EVENT_RESOLVE_SPELL)
  {
    identity_or_result = get_color_identity_of_all_commanders(player, 3);
    i = 0;
    data = 0;
    do
    {
      if ((identity_or_result >> ((unsigned char)i & 0x1f) & 1U) == 0)
      {
        data = data | 1 << ((unsigned char)i + 10 & 0x1f);
      }
      i = i + 1;
    } while (i != 5);
    instance = get_card_instance(player, card);
    set_inst_info(instance, 0x5651, data, 0);
    return 0;
  }
  if ((unsigned int)event - (unsigned int)EVENT_POWER < 3U)
  {
    instance = get_card_instance(player, card);
    if ((((instance->damage_target_card == affected_card) &&
          (instance->damage_target_player == affected_card_controller)) &&
         (equipped = is_equipping(player, card), equipped != NULL)) &&
        (identity_or_result = is_humiliated(player, card, AL_STATIC), identity_or_result == 0))
    {
      if ((unsigned int)event - (unsigned int)EVENT_POWER < 2U)
      {
        event_result = event_result + 3;
        return 0;
      }
      protection_record = inst_info(instance, 0x5651);
      event_result = event_result | protection_record.data;
    }
    return 0;
  }

  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  equip_target = &td;
  default_target_definition(player, card, equip_target, 2);
  td.allowed_controller = player;
  td.preferred_controller = player;
  default_target_definition(player, card, &td1, 2);
  td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td1.extra = (intptr_t)is_commander;
  td1.allowed_controller = player;
  td1.preferred_controller = player;
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x100, 5, 0, 0, 0, 0, 0, 0,
                                  equip_target, NULL) != 0)
    {
      return 1;
    }
    return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x100, 3, 0, 0, 0, 0, 0, 0,
                                     &td1, NULL);
  }
  if (event == EVENT_ACTIVATE)
  {
    identity_or_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 5, 0, 0, 0,
                                                   0, 0, 0, equip_target, NULL);
    normal_equip_available = 0;
    if (identity_or_result != 0)
    {
      identity_or_result = can_sorcery_be_played(player, EVENT_ACTIVATE);
      normal_equip_available = (unsigned int)(identity_or_result != 0);
    }
    identity_or_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 3, 0, 0, 0,
                                                   0, 0, 0, &td1, NULL);
    commander_equip_available = 0;
    if (identity_or_result != 0)
    {
      identity_or_result = can_sorcery_be_played(player, EVENT_ACTIVATE);
      commander_equip_available = (unsigned int)(identity_or_result != 0);
    }
    choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Normal equip",
                         normal_equip_available, 1, "Commander equip", commander_equip_available, 5,
                         0, 0, 0, 0);
    if (choice == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (choice == 1)
    {
      prompt = "Select target creature you control.";
      identity_or_result = 5;
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 0x102, identity_or_result, 0,
                                       0, 0, 0, 0, 0, equip_target, (char *)prompt);
    }
    if (choice == 2)
    {
      prompt = "Select target commander creature you control.";
      identity_or_result = 3;
      equip_target = &td1;
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 0x102, identity_or_result, 0,
                                       0, 0, 0, 0, 0, equip_target, (char *)prompt);
    }
    return generic_activated_ability(player, card, event, 0x102, 5, 0, 0, 0, 0, 0, 0, &td,
                                     "Select target creature you control.");
  }

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    int mode = instance->info_slot;
    bool valid = false;
    if (mode == 1)
    {
      valid = validate_target(player, card, &td, 0) != 0;
      if (!valid)
        mode = instance->info_slot;
    }
    if (!valid && mode == 2)
      valid = validate_target(player, card, &td1, 0) != 0;
    if (valid)
      equip_target_creature(instance->parent_controller, instance->parent_card,
                            instance->targets[0].player, instance->targets[0].card);
  }
  return generic_activated_ability(player, card, event, 0x102, 5, 0, 0, 0, 0, 0, 0, &td,
                                   "Select target creature you control.");
}
#endif

/* ManalinkEh 2022: 0x25a6530; original cards/commander_legends.c:1706. */
int card_court_of_ambition(int player, int card, event_t event)
{
  /* Court of Ambition	|2|B|B
   * Enchantment
   * When this enchantment enters, you become the monarch.
   * At the beginning of your upkeep, each opponent loses 3 life unless they discard a card. If you're the monarch, instead each opponent loses 6 life unless they discard two cards. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
    you_become_the_monarch(player);
  upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    int is_monarch = who_is_the_monarch() == player;
    int opponent = 1 - player;
    int quantity = is_monarch + 1;
    int life_loss = is_monarch * 3 + 3;
    int discard_cards = 0;
    if (hand_count[opponent] >= quantity)
    {
      char buffer[100];
      scnprintf(buffer, 100, " Lose %d life\n Discard %d cards", life_loss, quantity);
      discard_cards = do_dialog(opponent, player, card, -1, -1, buffer, life[opponent] < 7) != 0;
    }
    if (discard_cards)
      new_multidiscard(opponent, quantity, 2, player);
    else
      lose_life(opponent, life_loss);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25a66a0; original cards/commander_legends.c:1749. */
int card_court_of_bounty(int player, int card, event_t event)
{
  /* Court of Bounty	|2|G|G
   * Enchantment
   * When this enchantment enters, you become the monarch.
   * At the beginning of your upkeep, you may put a land card from your hand onto the battlefield. If you're the monarch, instead you may put a creature or land card from your hand onto the battlefield. */

  int result;
  int selection_mode;
  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    you_become_the_monarch(player);
    upkeep_trigger_ability(player, card, EVENT_ETB, player);
  }
  else
  {
    upkeep_trigger_ability(player, card, event, player);
    if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    {

      new_default_test_definition(&test, 1, "Select a Land card.");
      result = who_is_the_monarch();
      selection_mode = 1;
      if (result == player)
      {
        new_default_test_definition(&test, 3, "Select a Creature or Land card.");
        selection_mode = 2;
      }
      test.zone = 0x100;
      result = check_battlefield_for_special_card(player, card, player, 4, &test);
      if (result != 0)
      {
        test.zone = 0x200;
        new_global_tutor(player, player, 4, 4, 0, selection_mode, &test);
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25a6810; original cards/commander_legends.c:1781. */
int card_court_of_cunning(int player, int card, event_t event)
{
  /* Court of Cunning	|1|U|U
   * Enchantment
   * When this enchantment enters, you become the monarch.
   * At the beginning of your upkeep, any number of target players each mill two cards. If you're the monarch, each of those players mills ten cards instead. (To mill a card, a player puts the top card of their library into their graveyard.) */

  size_t __n_1;
  int result;
  uint32_t both_available;
  unsigned int choice;
  uint32_t own_available;
  uint32_t opponent_available;
  target_definition_t td;
  char str1[100];
  char str2[100];
  char str3[100];

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    you_become_the_monarch(player);
    upkeep_trigger_ability(player, card, EVENT_ETB, player);
  }
  else
  {
    upkeep_trigger_ability(player, card, event, player);
    if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    {

      result = who_is_the_monarch();
      __n_1 = (unsigned int)(result == player) * 8 + 2;
      default_target_definition(player, card, &td, 0xff);
      td.zone = TARGET_ZONE_PLAYERS;
      snprintf(str1, 100, "Opponent mills %d cards", __n_1);
      snprintf(str2, 100, "You mill %d cards", __n_1);
      snprintf(str3, 100, "Both players mill %d cards", __n_1);
      result = would_validate_arbitrary_target(&td, 1 - player, -1);
      if (result == 0)
      {
        result = would_validate_arbitrary_target(&td, player, -1);
        if (result == 0)
        {
          own_available = 0;
          both_available = 0;
          opponent_available = 0;
        }
        else
        {
          both_available = 0;
          own_available = 2;
          opponent_available = 0;
        }
      }
      else
      {
        result = would_validate_arbitrary_target(&td, player, -1);
        if (result == 0)
        {
          own_available = 0;
          opponent_available = 1;
          both_available = 0;
        }
        else
        {
          opponent_available = 1;
          own_available = 2;
          both_available = 4;
        }
      }
      choice = dialog_impl(player, card, EVENT_RESOLVE_ACTIVATION, DLG_RANDOM, DLG_NO_STORAGE, str1,
                           opponent_available, 0x32, str2, own_available, 0xffffffce, str3,
                           both_available, 0xffffffce, "Do Nothing", 1, 0xffffffce, 0, 0, 0, 0);
      if ((choice & 1) != 0)
      {
        mill(1 - player, __n_1);
      }
      if ((choice & 2) != 0)
      {
        mill(player, __n_1);
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25a6af0; original cards/commander_legends.c:1838. */
int card_court_of_grace(int player, int card, event_t event)
{
  /* Court of Grace	|2|W|W
   * Enchantment
   * When this enchantment enters, you become the monarch.
   * At the beginning of your upkeep, create a 1/1 white Spirit creature token with flying. If you're the monarch, create a 4/4 white Angel creature token with flying instead. */

  int result;
  token_generation_t token;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    you_become_the_monarch(player);
    upkeep_trigger_ability(player, card, EVENT_ETB, player);
  }
  else
  {
    upkeep_trigger_ability(player, card, event, player);
    if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    {
      default_token_definition(player, card, CARD_ID_SPIRIT, &token);
      token.tou = 1;
      token.pow = 1;
      token.color_forced = ' ';
      token.key_plus = 0x20;
      result = who_is_the_monarch();
      if (result == player)
      {
        default_token_definition(player, card, CARD_ID_ANGEL, &token);
        token.tou = 4;
        token.pow = 4;
      }
      generate_token(&token);
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25a6c00; original cards/commander_legends.c:1869. */
int card_court_of_ire(int player, int card, event_t event)
{
  /* Court of Ire	|3|R|R
   * Enchantment
   * When this enchantment enters, you become the monarch.
   * At the beginning of your upkeep, this enchantment deals 2 damage to any target. If you're the monarch, it deals 7 damage instead. */

  size_t __n_1;
  int result;
  card_instance_t *instance;
  target_definition_t td1;
  char msg[100];

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    you_become_the_monarch(player);
    upkeep_trigger_ability(player, card, EVENT_ETB, player);
  }
  else
  {
    upkeep_trigger_ability(player, card, event, player);
    if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    {

      result = who_is_the_monarch();
      __n_1 = (unsigned int)(result == player) * 5 + 2;
      default_target_definition(player, card, &td1, 2);
      td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      instance = get_card_instance(player, card);
      result = can_target(&td1);
      if (result != 0)
      {
        snprintf(msg, 100, "Select a target (%d damage).", __n_1);
        result = new_pick_target(&td1, msg, 0, 0x80);
        if (result != 0)
        {
          damage_creature(instance->targets[0].player, instance->targets[0].card, __n_1, player,
                          card);
        }
      }
      instance->number_of_targets = '\0';
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25a6d90; original cards/commander_legends.c:1924. */
int card_dawnglade_regent(int player, int card, event_t event)
{
  /* Dawnglade Regent	|5|G|G
   * Creature - Elk 8/8
   * When this creature enters, you become the monarch.
   * As long as you're the monarch, permanents you control have hexproof. */

  int result;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    if ((event == EVENT_ABILITIES) && (affected_card_controller == player))
    {
      result = is_what(player, affected_card, 0x47);
      if (result != 0)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {
          result = who_is_the_monarch();
          if (result == player)
          {
            hexproof(affected_card_controller, affected_card, EVENT_ABILITIES);
          }
        }
      }
    }
    return 0;
  }
  you_become_the_monarch(player);
  return 0;
}

/* ManalinkEh 2022: 0x25a6e60; original cards/commander_legends.c:1944. */
int card_demonic_lore(int player, int card, event_t event)
{
  /* Demonic Lore	|2|B
   * Enchantment
   * When this enchantment enters, draw three cards.
   * At the beginning of your end step, you lose 2 life for each card in your hand. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    draw_cards(player, 3);
  }
  if ((current_turn == player) && (eot_trigger(player, card, event) != 0))
  {
    lose_life(player, hand_count[player] * 2);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25a6f00; original cards/commander_legends.c:1977. */
int card_elvish_dreadlord(int player, int card, event_t event)
{
  /* Elvish Dreadlord	|3|B|B
   * Creature - Zombie Elf 3/3
   * Deathtouch
   * When this creature dies, non-Elf creatures get -3/-3 until end of turn.
   * Encore |5|B|B (|5|B|B, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_ELVISH_DREADLORD = 22109
  };

  int result;
  test_definition_t this_test;

  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    default_test_definition(&this_test, 2);
    this_test.subtype = 0x3c;
    this_test.subtype_flag = 1;
    pump_creatures_until_eot(player, card, 2, 0, -3, -3, 0, 0, &this_test);
  }
  result = encore(player, card, event, CARD_ID_ELVISH_DREADLORD, 5, 2, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x25a6ff0; original cards/commander_legends.c:1996. */
int card_emberwilde_captain(int player, int card, event_t event)
{
  /* Emberwilde Captain	|3|R
   * Creature - Djinn Pirate 4/2
   * When this creature enters, you become the monarch.
   * Whenever an opponent attacks you while you're the monarch, this creature deals damage to that player equal to the number of cards in their hand. */

  int attacker_player;
  int argument3;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  attacker_player = 1 - player;
  if (event != EVENT_ETB)
  {
    store_attackers(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, attacker_player, -1, NULL);
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      argument3 = who_is_the_monarch();
      if (argument3 == player)
      {
        damage_player(attacker_player, hand_count[attacker_player], argument3, card);
      }
    }
    return 0;
  }
  you_become_the_monarch(player);
  store_attackers(player, card, EVENT_ETB, DAT_RESOLVE_TRIGGER_MANDATORY, attacker_player, -1,
                  NULL);
  dat_ability(player, card, EVENT_ETB, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  return 0;
}

/* ManalinkEh 2022: 0x25a7130; original cards/commander_legends.c:2032. */
int card_exquisite_huntmaster(int player, int card, event_t event)
{
  /* Exquisite Huntmaster	|3|B
   * Creature - Elf Warrior 4/2
   * When this creature dies, create a 1/1 green Elf Warrior creature token.
   * Encore |4|B (|4|B, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_EXQUISITE_HUNTMASTER = 22113
  };

  int result;
  token_generation_t token;

  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    default_token_definition(player, card, CARD_ID_ELF_WARRIOR, &token);
    generate_token(&token);
  }
  result = encore(player, card, event, CARD_ID_EXQUISITE_HUNTMASTER, 4, 1, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x25a71e0; original cards/commander_legends.c:2048. */
int card_eyeblight_cullers(int player, int card, event_t event)
{
  /* Eyeblight Cullers	|4|B
   * Creature - Elf Warrior 3/3
   * When this creature dies, create three 1/1 green Elf Warrior creature tokens, then mill three cards. (Put the top three cards of your library into your graveyard.) */

  int result;
  token_generation_t token;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  default_token_definition(player, card, CARD_ID_ELF_WARRIOR, &token);
  token.qty = '\x03';
  generate_token(&token);
  mill(player, 3);
  return 0;
}

/* ManalinkEh 2022: 0x25a7260; original cards/commander_legends.c:2067. */
int card_fall_from_favor(int player, int card, event_t event)
{
  /* Fall from Favor	|2|U
   * Enchantment - Aura
   * Enchant creature
   * When this Aura enters, tap enchanted creature and you become the monarch.
   * Enchanted creature doesn't untap during its controller's untap step unless that player is the monarch. */

  int attached_card;
  card_instance_t *instance;
  int monarch;
  int target_player;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    you_become_the_monarch(player);
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) && (-1 < instance->damage_target_card))
    {
      tap_card((int)instance->damage_target_player, instance->damage_target_card);
    }
  }
  else if (event == EVENT_UNTAP)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      if (is_humiliated(player, card, AL_STATIC) == 0)
      {

        instance = get_card_instance(player, card);
        target_player = (int)instance->damage_target_player;
        attached_card = instance->damage_target_card;
        if ((((-1 < target_player) && (-1 < attached_card)) && (current_turn == target_player)) &&
            (((current_phase == 1 && (attached_card == affected_card)) &&
              (target_player == affected_card_controller))))
        {
          monarch = who_is_the_monarch();
          if (monarch != target_player)
          {
            does_not_untap(target_player, attached_card, EVENT_UNTAP);
          }
        }
      }
    }
  }
  return disabling_aura(player, card, event);
}

/* ManalinkEh 2022: 0x25a73a0; original cards/commander_legends.c:2098. */
int card_falthis_shadowcat_familiar(int player, int card, event_t event)
{
  /* Falthis, Shadowcat Familiar	|2|B
   * Legendary Creature - Nightmare Cat 2/2
   * Commanders you control have menace and deathtouch.
   * Partner (You can have two commanders if both have partner.) */

  int result;
  card_instance_t *instance;

  check_legend_rule(player, card, event);
  if ((event == EVENT_ABILITIES) && (affected_card_controller == player))
  {
    result = is_what(player, affected_card, 0x47);
    if (result != 0)
    {
      result = check_special_flags2(affected_card_controller, affected_card, 0x1000000);
      if (result != 0)
      {
        instance = in_play(player, card);
        if (instance != NULL)
        {
          result = is_humiliated(player, card, AL_STATIC);
          if (result == 0)
          {
            deathtouch(affected_card_controller, affected_card, EVENT_ABILITIES);
            menace(affected_card_controller, affected_card, EVENT_ABILITIES);
          }
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a7490; original cards/commander_legends.c:2115. */
int card_fathom_fleet_swordjack(int player, int card, event_t event)
{
  /* Fathom Fleet Swordjack	|3|R
   * Creature - Orc Pirate 4/3
   * Whenever this creature attacks, it deals damage to the player or planeswalker it's attacking equal to the number of artifacts you control.
   * Encore |5|R (|5|R, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_FATHOM_FLEET_SWORDJACK = 22117
  };

  int result;
  int amt;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {

    result = planeswalker_its_attacking(player, card);
    if (result < 0)
    {
      result = count_subtype(player, 0x40, -1);
      damage_player(1 - player, result, player, card);
    }
    else
    {
      amt = count_subtype(player, 0x40, -1);
      damage_creature(1 - player, result, amt, player, card);
    }
  }
  return encore(player, card, event, CARD_ID_FATHOM_FLEET_SWORDJACK, 5, 0, 0, 0, 1, 0);
}

/* ManalinkEh 2022: 0x25a75b0; original cards/commander_legends.c:2135. */
int card_feast_of_succession(int player, int card, event_t event)
{
  /* Feast of Succession	|4|B|B
   * Sorcery
   * All creatures get -4/-4 until end of turn. You become the monarch. */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    pump_creatures_until_eot(player, card, 2, 0, -4, -4, 0, 0, NULL);
    you_become_the_monarch(player);
    kill_card(player, card, KILL_BURY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25a7650; original cards/commander_legends.c:2151. */
int card_fin_clade_fugitives(int player, int card, event_t event)
{
  /* Fin-Clade Fugitives	|5|G
   * Creature - Elf Salamander Rogue 7/4
   * This creature can't be blocked by creatures with power 2 or less.
   * Encore |4|G (|4|G, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_FIN_CLADE_FUGITIVES = 22119
  };

  int result;

  if (event == EVENT_BLOCK_LEGALITY)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (((result == 0) && (attacking_card_controller == player)) && (attacking_card == card))
    {
      result = get_power(affected_card_controller, affected_card);
      if (result < 3)
      {
        event_result = event_result | 1;
      }
    }
  }
  result = encore(player, card, event, CARD_ID_FIN_CLADE_FUGITIVES, 4, 0, 0, 1, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x25a7720; original cards/commander_legends.c:2169. */
int card_flamekin_herald(int player, int card, event_t event)
{
  /* Flamekin Herald	|2|R
   * Creature - Elemental Wizard 3/2
   * Commander spells you cast have cascade. (Whenever you cast a commander, exile cards from the top of your library until you exile a nonland card with lesser mana value. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.) */

  int result;

  if (trigger_condition != TRIGGER_SPELL_CAST)
  {
    return 0;
  }
  if ((card == affected_card) && (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      result = check_special_flags2(trigger_cause_controller, trigger_cause, 0x1000000);
      if (result != 0)
      {
        result = new_specific_spell_played(player, card, event, player, 2, NULL);
        if (result != 0)
        {
          result = get_cmc(trigger_cause_controller, trigger_cause);
          do_cascade(player, card, result);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a7800; original cards/commander_legends.c:2186. */
int card_forceful_denial(int player, int card, event_t event)
{
  /* Forceful Denial	|3|U|U
   * Instant
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.)
   * Counter target spell. */

  int result;

  if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = 0;
    counterspell(player, card, EVENT_CAST_SPELL, NULL, 0);
    if (spell_fizzled != 1)
    {
      cascade(player, card, EVENT_CAST_SPELL);
      counterspell(player, card, EVENT_CAST_SPELL, NULL, 0);
    }
  }
  else
  {
    result = counterspell(player, card, event, NULL, 0);
  }
  return result;
}

/* ManalinkEh 2022: 0x25a78d0; original cards/commander_legends.c:2211. */
int card_ghen_arcanum_weaver(int player, int card, event_t event)
{
  /* Ghen, Arcanum Weaver	|R|W|B
   * Legendary Creature - Human Wizard 2/3
   * |R|W|B, |T, Sacrifice an enchantment: Return target enchantment card from your graveyard to the battlefield. */

  unsigned int sacrificed;
  int result;
  test_definition_t test_sac;
  test_definition_t test_return;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    new_default_test_definition(&test_sac, 4, "Select an enchantment to sacrifice.");
    new_default_test_definition(&test_return, 4, "Select an enchantment card.");
    test_return.ai_selection_mode = 1;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = new_can_sacrifice_as_cost(player, card, &test_sac);
      if (result == 0)
      {
        return 0;
      }
      return generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE,
                                                            GAA_UNTAPPED, 0, 1, 0, 0, 1, 1, 0, NULL,
                                                            NULL, TYPE_NONE, &test_return, 8);
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 1, 0, 0, 1, 1, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      sacrificed = new_sacrifice(player, card, player,
                                 SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test_sac);
      if (sacrificed != 0)
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
          kill_card((int)sacrificed >> 0x10 & 0xff, sacrificed >> 0x18, KILL_SACRIFICE);
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
    new_default_test_definition(&test_sac, 4, "Select an enchantment to sacrifice.");
    new_default_test_definition(&test_return, 4, "Select an enchantment card.");
  }
  test_return.ai_selection_mode = 1;
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = validate_target_from_grave_source(player, card, player, 0), result != -1))
  {
    reanimate_permanent(player, card, player, result, REANIMATE_DEFAULT);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a7bf0; original cards/commander_legends.c:2283. */
int card_glacian_powerstone_engineer(int player, int card, event_t event)
{
  /* Glacian, Powerstone Engineer	|5|U
   * Legendary Creature - Human Artificer 3/6
   * |T, Tap X untapped artifacts you control: Look at the top X cards of your library. Put one of those cards into your hand and the rest into your graveyard.
   * Partner (You can have two commanders if both have partner.) */

  int artifact;
  card_instance_t *instance;
  int selected_count;
  int *selected_cursor;
  target_definition_t td1;
  int selected[50];

  check_legend_rule(player, card, event);
  if (event == EVENT_CAN_ACTIVATE)
  {
    return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL,
                                     NULL);
  }
  if (event != EVENT_ACTIVATE)
  {
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      return 0;
    }
    instance = get_card_instance(player, card);
    reveal_top_cards_of_library_and_choose_type(player, card, player, instance->info_slot, 1,
                                                TUTOR_HAND, 0, TUTOR_GRAVE, 0, TYPE_ANY);
    return 0;
  }
  default_target_definition(player, card, &td1, 0x40);
  td1.preferred_controller = player;
  td1.allowed_controller = player;
  td1.illegal_state = TARGET_STATE_BLOCKING;
  td1.illegal_abilities = 0;
  instance = get_card_instance(player, card);
  selected_count = 0;
  while (true)
  {
    if (can_target(&td1) == 0)
      break;
    if (new_pick_target(&td1, "Select an artifact to tap.", 0, 0x80) == 0)
      break;
    artifact = instance->targets[0].card;
    selected[selected_count] = artifact;
    state_untargettable(player, artifact, 1);
    selected_count = selected_count + 1;
    instance->number_of_targets = '\0';
    if (current_turn != player)
    {
      ai_modifier = ai_modifier + 0xf;
    }
  }
  instance->info_slot = selected_count;
  generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  if (0 < selected_count)
  {
    selected_cursor = selected;
    do
    {
      artifact = *selected_cursor;
      state_untargettable(player, artifact, 1);
      if (spell_fizzled != 1)
      {
        tap_card(player, artifact);
      }
      selected_cursor = selected_cursor + 1;
    } while (selected_cursor != selected + selected_count);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a7ed0; original cards/commander_legends.c:2338. */
int card_gnostro_voice_of_the_crags(int player, int card, event_t event)
{
  /* Gnostro, Voice of the Crags	|1|U|R|W
   * Legendary Creature - Chimera 3/3
   * |T: Choose one. X is the number of spells you've cast this turn.
   * * Scry X.
   * * Gnostro deals X damage to target creature.
   * * You gain X life. */

  int choice;
  uint8_t selected_mode;
  card_instance_t *mode_record;
  card_instance_t *instance;
  int life_or_storm_count;
  target_definition_t td1;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td1, 2);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
      mode_record = get_card_instance(player, card);
      mode_record->mode = instance->number_of_targets;
      life_or_storm_count = life[player];
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, "Scry", 1, 10,
                           "Damage creature", can_target(&td1), 0x1e, "Gain life", 1,
                           ((life_or_storm_count < 6) ? 50U : 20U), 0, 0, 0, 0);
      if (choice == 2)
      {
        pick_target(&td1, "TARGET_CREATURE");
      }
      if (spell_fizzled == 1)
      {
        return 0;
      }
      generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      instance = get_card_instance(player, card);
      instance->mode = (uint8_t)choice;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td1, 2);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    selected_mode = instance->mode;
    life_or_storm_count = get_storm_count();
    if (selected_mode == '\x01')
    {
      scry(player, life_or_storm_count);
    }
    else if (selected_mode == '\x02')
    {
      if (valid_target(&td1) != 0)
      {
        damage_target0(player, card, life_or_storm_count);
      }
    }
    else if (selected_mode == '\x03')
    {
      gain_life(player, life_or_storm_count);
      return 0;
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x25a8200; original cards/commander_legends.c:2390. */
int card_gor_muldrak_amphinologist(int player, int card, event_t event)
{
  /* Gor Muldrak, Amphinologist	|1|G|U
   * Legendary Creature - Human Scout 3/2
   * You and permanents you control have protection from Salamanders.
   * At the beginning of your end step, each player who controls the fewest creatures creates a 4/3 blue Salamander Warrior creature token. */

  int result;
  card_instance_t *instance;
  unsigned int pass;
  unsigned int recipient;
  int counts[2];
  token_generation_t token;

  check_legend_rule(player, card, event);
  if ((((event == EVENT_STATIC_EFFECTS || event == EVENT_ABILITIES) ||
        (event == EVENT_PROT_MISC)) &&
       (instance = in_play(player, card), instance != NULL)) &&
      (result = is_humiliated(player, card, AL_STATIC), result == 0))
  {
    if (event == EVENT_STATIC_EFFECTS)
    {
      player_bits[player] = player_bits[player] | 0x4000000;
    }
    else if (event == EVENT_PROT_MISC)
    {
      if (affected_card == -1)
      {
        if (((unsigned int)event_result >> 24 & 0xff) == '\x06')
        {
          event_result = event_result | 1;
        }
        result = has_subtype(attacking_card_controller, attacking_card, SUBTYPE_SALAMANDER);
        if (result != 0)
        {
          event_result = event_result | 1;
        }
      }
      if (affected_card_controller == player)
      {
        protection_from_subtype(player, affected_card, EVENT_PROT_MISC, SUBTYPE_SALAMANDER);
      }
    }
    else if ((event == EVENT_ABILITIES) && (affected_card_controller == player))
    {
      special_abilities2(player, affected_card, EVENT_ABILITIES, SP_KEYWORD2_GRANTED_PROT_MISC);
    }
  }
  result = eot_trigger(player, card, event);
  if ((result != 0) && (current_turn == player))
  {
    counts[0] = count_permanents_by_type(0, TYPE_CREATURE);
    counts[1] = count_permanents_by_type(1, TYPE_CREATURE);

    pass = 0;
    do
    {
      recipient = current_turn ^ pass;
      if (counts[recipient] <= counts[1 - recipient])
      {
        default_token_definition(player, card, CARD_ID_SALAMANDER_WARRIOR, &token);
        token.t_player = recipient;
        generate_token(&token);
      }
      pass = pass + 1;
    } while (pass != 2);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a8430; original cards/commander_legends.c:2437. */
int card_guildless_commons(int player, int card, event_t event)
{
  /* Guildless Commons	""
   * Land
   * This land enters tapped.
   * When this land enters, return a land you control to its owner's hand.
   * |T: Add |C|C. */

  int result;
  result = karoo(player, card, event, 0, 0, 0x3d67);
  return result;
}

/* ManalinkEh 2022: 0x25a8470; original cards/commander_legends.c:2448. */
int card_halana_kessig_ranger(int player, int card, event_t event)
{
  /* Halana, Kessig Ranger	|3|G
   * Legendary Creature - Human Archer Ranger 3/4
   * Reach
   * Whenever another creature you control enters, you may pay |2. When you do, that creature deals damage equal to its power to target creature.
   * Partner (You can have two commanders if both have partner.) */

  int src_card;
  int result;
  card_instance_t *instance;
  int amt;


  check_legend_rule(player, card, event);
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    test_definition_t test;
    default_test_definition(&test, 2);
    test.not_me = 1;
    result = duh_mode(player);
    etb_ability_global(player, event, (result != 0) + 1, TYPE_NONE, &test,
                       player, card);
  }
  if (event == EVENT_ETB)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 2);
    result = can_target(&td);
    if (result != 0)
    {
      result = has_mana(player, 0, 2);
      if (result != 0)
      {
        charge_mana(player, COLOR_COLORLESS, 2);
        if (spell_fizzled != 1)
        {
          result = new_pick_target(&td, "TARGET_CREATURE", 0, 1);
          if (result != 0)
          {

            instance = get_card_instance(player, card);
            src_card = trigger_cause;
            result = trigger_cause_controller;
            amt = get_power(trigger_cause_controller, trigger_cause);
            damage_creature(instance->targets[0].player, instance->targets[0].card, amt, result,
                            src_card);
            instance->number_of_targets = '\0';
          }
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a8610; original cards/commander_legends.c:2487. */
int count_creatures_with_p1_p1_counter(int player)
{
  card_instance_t *instance;
  int result;
  int player_00;
  int card;
  int count;

  player_00 = 0;
  count = 0;
  do
  {

    if (((player == player_00) || (player_00 == 2)) && (0 < active_cards_count[player]))
    {
      card = 0;
      do
      {
        instance = in_play(player_00, card);
        if (instance != NULL)
        {
          result = is_what(player_00, card, 2);
          if (result != 0)
          {
            result = count_counters(player_00, card, COUNTER_P1_P1);
            count = (count + 1) - (unsigned int)(result == 0);
          }
        }
        card = card + 1;
      } while (card < active_cards_count[player]);
    }
    player_00 = player_00 + 1;
  } while (player_00 != 2);
  return count;
}

/* ManalinkEh 2022: 0x25a86a0; original cards/commander_legends.c:2501. */
int card_hamza_guardian_of_arashin(int player, int card, event_t event)
{
  /* Hamza, Guardian of Arashin	|4|G|W
   * Legendary Creature - Elephant Warrior 5/5
   * This spell costs |1 less to cast for each creature you control with a +1/+1 counter on it.
   * Creature spells you cast cost |1 less to cast for each creature you control with a +1/+1 counter on it. */

  card_instance_t *instance;
  int result;

  check_legend_rule(player, card, event);
  if (event == EVENT_MODIFY_COST)
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
    if (event != EVENT_MODIFY_COST_GLOBAL)
    {
      return 0;
    }
    instance = in_play(player, card);
    if (instance == NULL)
    {
      return 0;
    }
    result = is_humiliated(player, card, AL_STATIC);
    if (result != 0)
    {
      return 0;
    }
    if (affected_card_controller != player)
    {
      return 0;
    }
    result = is_what(player, affected_card, 2);
    if (result == 0)
    {
      return 0;
    }
  }
  result = count_creatures_with_p1_p1_counter(player);
  COST_COLORLESS = COST_COLORLESS - (char)result;
  return 0;
}

#ifndef SHANDALAR
static int put_into_play_a_card_from_deck_tapped_and_attacking(int player, int t_player, int deck_position)
{
  const int *deck = deck_ptr[t_player];
  if (is_what(-1, deck[deck_position], TYPE_CREATURE) &&
      grafdiggers_cages(player, TYPE_CREATURE, TUTOR_FROM_DECK))
    return -1;
  int card = add_card_to_hand(player, deck[deck_position]);
  remove_card_from_deck(t_player, deck_position);
  add_state(player, card, 0x14);
  set_special_flags(player, card, 0x50000);
  put_into_play(player, card);
  return card;
}
#endif

/* ManalinkEh 2022: 0x25a8760; original cards/commander_legends.c:2540. */
int card_hans_eriksson(int player, int card, event_t event)
{
  /* Hans Eriksson	|2|R|G
   * Legendary Creature - Human Scout 1/4
   * Whenever ~ attacks, reveal the top card of your library. If it's a creature card, put it onto the battlefield tapped and attacking defending player or a planeswalker they control. Otherwise, put that card into your hand. When you put a creature card onto the battlefield this way, it fights ~. */

  int *library;
  int result;

  check_legend_rule(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }
  reveal_top_cards_of_library(player, 1);
  result = is_what(-1, *deck_ptr[player], 2);
  if (result != 0)
  {

    library = deck_ptr[player];
    result = is_what(-1, *library, 2);
    if ((result == 0) ||
        (result = grafdiggers_cages(player, TYPE_CREATURE, TUTOR_FROM_DECK), result == 0))
    {
      result = add_card_to_hand(player, *library);
      remove_card_from_deck(player, 0);
      add_state(player, result, 0x14);
      set_special_flags(player, result, 0x50000);
      put_into_play(player, result);
      if (result == 0)
      {
        return 0;
      }
    }
    else
    {
      result = -1;
    }
    fight(player, result, player, card);
    return 0;
  }
  add_card_to_hand(player, *deck_ptr[player]);
  obliterate_top_card_of_deck(player);
  return 0;
}

/* ManalinkEh 2022: 0x25a2020; original cards/commander_legends.c:2568. */
static int hellkite_courser_effect(int player, int card, event_t event)
{
  char attached_player;
  card_instance_t *instance;

  if (event == EVENT_ABILITIES)
  {

    instance = get_card_instance(player, card);
    attached_player = instance->damage_target_player;

    if (((-1 < attached_player) && (instance->damage_target_card == affected_card)) &&
        (attached_player == affected_card_controller))
    {
      haste((int)attached_player, instance->damage_target_card, EVENT_ABILITIES);
    }
  }
  if (!eot_trigger(player, card, event))
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {

    put_commander_in_the_command_zone_from_play((int)instance->damage_target_player,
                                                instance->damage_target_card);
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25a88e0; original cards/commander_legends.c:2592. */
int card_hellkite_courser(int player, int card, event_t event)
{
  /* Hellkite Courser	|4|R|R
   * Creature - Dragon 6/5
   * Flying
   * When this creature enters, you may put a commander you own from the command zone onto the battlefield. It gains haste. Return it to the command zone at the beginning of the next end step. */

  int target_card;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    target_card = put_commander_from_command_zone_to(player, 4);
    if (-1 < target_card)
    {
      create_targetted_legacy_effect(player, card, hellkite_courser_effect, player, target_card);
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x25a8960; original cards/commander_legends.c:2611. */
int card_horizon_stone(int player, int card, event_t event)
{
  /* Horizon Stone	|5
   * Artifact
   * If you would lose unspent mana, that mana becomes colorless instead. */

  int c;
  uint8_t *next_flag;
  unsigned char artifact_flag;
  int result;
  uint8_t *flag_cursor;
  card_instance_t *instance;

  if ((event == EVENT_MANA_POOL_DRAINING) &&
      (result = is_humiliated(player, card, AL_STATIC), result == 0))
  {
    flag_cursor = mana_doesnt_drain_from_pool[player];
    do
    {

      next_flag = flag_cursor + 1;
      flag_cursor = flag_cursor + 1;
      *flag_cursor = *next_flag | 0x40;
    } while (flag_cursor != mana_doesnt_drain_from_pool[player] + 5);
    artifact_flag = mana_doesnt_drain_from_pool[player][6];
    mana_doesnt_drain_from_pool[player][0] = mana_doesnt_drain_from_pool[player][0] | 0x80;
    mana_doesnt_drain_from_pool[player][6] = artifact_flag | 0x80;
  }
  result = eot_trigger(player, card, event);
  if ((((result != 0) &&
        ((ai_is_speculating == 1 || ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))))) &&
       (result = is_humiliated(player, card, AL_TRIG), result == 0)) &&
      (((current_turn != player ||
         (result = check_battlefield_for_id(player, CARD_ID_SEEDBORN_MUSE), result != 0)) ||
        (result = check_battlefield_for_id(player, CARD_ID_PROPHET_OF_KRUPHIX), result != 0))))
  {

    needed_mana_colors = 0x7f;
    c = 0;
    if (0 < active_cards_count[player])
    {
      do
      {
        instance = in_play(player, c);
        if (((instance != NULL) &&
             ((cards_data[instance->internal_card_id].extra_ability & 0x1000) != 0)) &&
            (call_card_function_i(instance, player, c, EVENT_CAN_ACTIVATE) != 0))
        {
          force_activation_for_mana(player, c, COLOR_TEST_ARTIFACT | COLOR_TEST_ANY);
          needed_mana_colors = 0x7f;
        }
        c = c + 1;
      } while (c < active_cards_count[player]);
    }
    needed_mana_colors = 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a8b20; original cards/commander_legends.c:2655. */
int card_hullbreacher(int player, int card, event_t event)
{
  /* Hullbreacher	|2|U
   * Creature - Merfolk Pirate 3/2
   * Flash
   * If an opponent would draw a card except the first one they draw in each of their draw steps, instead you create a Treasure token. (It's an artifact with "|T, Sacrifice this token: Add one mana of any color.") */

  int result;

  result = notion_thief_effect(player, card, event);
  if (result != 0)
  {
    generate_token_by_id(player, card, CARD_ID_TREASURE);
  }
  result = flash(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25a19f0; original cards/commander_legends.c:2669. */
static void ich_tekik_ability(int player, int card)
{
  test_definition_t test;

  add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  default_test_definition(&test, 0x47);
  test.subtype = 0x4c;
  new_manipulate_all(player, card, player, &test, ACT_ADD_COUNTERS_BASE | ACT_KILL_BURY);
  return;
}

/* ManalinkEh 2022: 0x25a1a70; original cards/commander_legends.c:2677. */
static int ich_tekik_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  unsigned int deaths;
  card_instance_t *dying;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (get_id((int)instance->damage_target_player, instance->damage_target_card) != 0x5678))
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
        count_for_gfp_ability(player, card, 0x77, 2, 0x40, NULL);
      }
    }
  }
  deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_GRAVEYARD_FROM_PLAY && deaths == 0)
    return 0;
  if (deaths != 0)
  {
    if (!check_status(player, card, 0x10000))
    {

      for (deaths = deaths & 0xff; deaths != 0; deaths = deaths - 1)
      {
        ich_tekik_ability(player, card);
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

/* ManalinkEh 2022: 0x25a8b80; original cards/commander_legends.c:2729. */
int card_ich_tekik_salvage_splicer(int player, int card, event_t event)
{
  /* Ich-Tekik, Salvage Splicer	|4|G
   * Legendary Creature - Phyrexian Human Artificer 1/1
   * When Ich-Tekik enters, create a 3/3 colorless Phyrexian Golem artifact creature token.
   * Whenever an artifact is put into a graveyard from the battlefield, put a +1/+1 counter on Ich-Tekik and a +1/+1 counter on each Golem you control.
   * Partner (You can have two commanders if both have partner.) */

  int c;
  card_instance_t *instance;
  unsigned int deaths;
  int legacy_card;

  check_legend_rule(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      bool found = false;
      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          instance = in_play(player, c);

          if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
               (instance = get_card_instance(player, c),
                instance->info_slot_fn == ich_tekik_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            found = true;
            break;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }

      if (!found)
      {
        legacy_card = create_targetted_legacy_effect(player, card, ich_tekik_effect, player, card);
        instance = get_card_instance(player, legacy_card);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, legacy_card, 0x10000);
      }
    }
    etb_ability(player, card, EVENT_STATIC_EFFECTS, RESOLVE_TRIGGER_MANDATORY);
  }
  else
  {
    etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (event == EVENT_ETB)
    {
      generate_tokens_by_id(player, card, CARD_ID_GOLEM, 1);
    }
    else if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {
      count_for_gfp_ability(player, card, 0x77, 2, 0x40, NULL);
    }
  }
  instance = get_card_instance(player, card);

  if (((instance->kill_code == '\0') &&
       (deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY),
        deaths != 0)) &&
      (deaths = deaths & 0xff, deaths != 0))
  {
    do
    {
      ich_tekik_ability(player, card);
      deaths = deaths - 1;
    } while (deaths != 0);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a8da0; original cards/commander_legends.c:2787. */
int card_imoti_celebrant_of_bounty(int player, int card, event_t event)
{
  /* Imoti, Celebrant of Bounty	|3|G|U
   * Legendary Creature - Snake Druid 3/1
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.)
   * Spells you cast with mana value 6 or greater have cascade. */

  int result;

  cascade(player, card, event);
  check_legend_rule(player, card, event);
  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      result = get_cmc(trigger_cause_controller, trigger_cause);
      if (5 < result)
      {
        result = check_special_flags(trigger_cause_controller, trigger_cause, 0x40000);
        if (result == 0)
        {
          result = new_specific_spell_played(player, card, event, player, 2, NULL);
          if (result != 0)
          {
            result = get_cmc(trigger_cause_controller, trigger_cause);
            do_cascade(player, card, result);
          }
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a8eb0; original cards/commander_legends.c:2809. */
int card_impulsive_pilferer(int player, int card, event_t event)
{
  /* Impulsive Pilferer	|R
   * Creature - Goblin Pirate 1/1
   * When this creature dies, create a Treasure token. (It's an artifact with "|T, Sacrifice this token: Add one mana of any color.")
   * Encore |3|R (|3|R, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_IMPULSIVE_PILFERER = 22138
  };

  int result;

  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    generate_token_by_id(player, card, CARD_ID_TREASURE);
  }
  result = encore(player, card, event, CARD_ID_IMPULSIVE_PILFERER, 3, 0, 0, 0, 1, 0);
  return result;
}

/* ManalinkEh 2022: 0x25a8f40; original cards/commander_legends.c:2822. */
int card_ingenuity_engine(int player, int card, event_t event)
{
  /* Ingenuity Engine	|7
   * Artifact
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.)
   * |1, |T, Sacrifice an artifact: Return target artifact you control to its owner's hand. */

  int result;
  target_definition_t td;

  cascade(player, card, event);
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 0x40);
    td.allowed_controller = player;
    td.preferred_controller = player;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x40);
    td.preferred_controller = player;
    td.allowed_controller = player;
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      action_on_target(player, card, 0, ACT_BOUNCE);
    }
  }
  result = generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, 1, 0, 0, 0, 0, 0, 0, &td,
      "TARGET_ARTIFACT", TYPE_ARTIFACT, NULL, SARV_REPORT_CMC);
  return result;
}

/* ManalinkEh 2022: 0x259ff40; original cards/commander_legends.c:2847. */
int jared_carthalion_effect(int player, int card, event_t event)
{
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x25a90a0; original cards/commander_legends.c:2852. */
int card_jared_carthalion_true_heir(int player, int card, event_t event)
{
  /* Jared Carthalion, True Heir	|R|G|W
   * Legendary Creature - Human Warrior 3/3
   * When Jared Carthalion enters, target opponent becomes the monarch. You can't become the monarch this turn.
   * If damage would be dealt to Jared Carthalion while you're the monarch, prevent that damage and put that many +1/+1 counters on it. */

  char damaged_player;
  int num;
  card_instance_t *instance;
  int result;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    if (event == EVENT_PREVENT_DAMAGE)
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {

          instance = get_card_instance(affected_card_controller, affected_card);
          if ((((instance->internal_card_id == damage_card) && (0 < instance->info_slot)) &&
               (damaged_player = instance->damage_target_player, damaged_player == player)) &&
              (instance->damage_target_card == card))
          {
            result = who_is_the_monarch();
            if (result == damaged_player)
            {

              num = instance->info_slot;
              prevent_damage(affected_card_controller, affected_card, -1);
              add_counters(result, card, result, card, COUNTER_P1_P1, num);
            }
          }
        }
      }
    }
    return 0;
  }
  you_become_the_monarch(1 - player);
  create_legacy_effect(player, card, jared_carthalion_effect);
  return 0;
}

/* ManalinkEh 2022: 0x25a9200; original cards/commander_legends.c:2885. */
int card_jeskas_will(int player, int card, event_t event)
{
  /* Jeska's Will	|2|R
   * Sorcery
   * Choose one. If you control a commander as you cast this spell, you may choose both instead.
   * * Add |R for each card in target opponent's hand.
   * * Exile the top three cards of your library. You may play them this turn. */

  unsigned char resolved_mode;
  card_instance_t *instance;
  int opponent_valid;
  int commander_present;
  unsigned int both_available_or_choice;

  if (event == EVENT_CAN_CAST)
  {
    get_card_instance(player, card);
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        opponent_valid = opponent_is_valid_target(player, card);
        both_available_or_choice = 0;
        if (opponent_valid != 0)
        {
          commander_present = liutenant(player);
          both_available_or_choice = (unsigned int)(commander_present != 0);
        }
        both_available_or_choice =
            dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_MODE, "Mana",
                        opponent_valid, ((3 < hand_count[1 - player]) ? 20U : 1U),
                        "Exile", 1, (unsigned int)(current_turn != player) * 9 + 1, "Do both",
                        both_available_or_choice, 0x1e, 0, 0, 0, 0);
        if (both_available_or_choice == 0)
        {
          spell_fizzled = 1;
        }
        else
        {
          if ((both_available_or_choice & 1) != 0)
          {
            target_opponent(player, card);
            ai_modifier = ai_modifier + hand_count[1 - player] * 10;
          }
          if ((both_available_or_choice & 2) != 0)
          {
            ai_modifier = ai_modifier + 0x32;
          }
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      resolved_mode = instance->mode;
      if ((resolved_mode & 1) != 0)
      {
        if (opponent_is_valid_target(player, card) != 0)
        {
          produce_mana(player, 4, hand_count[1 - player]);
        }
      }
      if ((resolved_mode & 2) != 0)
      {
        exile_top_x_card_you_may_play_them(player, card, 3, NULL, player, MPCFE_UNTIL_EOT);
      }
      kill_card(player, card, KILL_BURY);
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    get_card_instance(player, card);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a1d20; original cards/commander_legends.c:2934. */
static int attached_creature_will_deal_triple_damage_until_eot(int player, int card, event_t event)
{
  card_instance_t *damage;
  card_instance_t *instance;

  damage = combat_damage_being_prevented(event);
  if (((damage != NULL) && (damage->damage_target_player != player)) &&
      (damage->damage_target_card == -1))
  {
    instance = get_card_instance(player, card);
    if (((instance != NULL) && (instance->damage_target_player != -1)) &&
        ((instance->damage_target_player == damage->damage_source_player &&
          (damage->damage_source_card == instance->damage_target_card))))
    {
      damage->info_slot = damage->info_slot * 3;
    }
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x25a94b0; original cards/commander_legends.c:2947. */
int card_jeska_thrice_reborn(int player, int card, event_t event)
{
  /* Jeska, Thrice Reborn	|2|R
   * Legendary Planeswalker - Jeska (0)
   * Jeska enters with a loyalty counter on her for each time you've cast a commander from the command zone this game.
   * 0: Choose target creature. Until your next turn, if that creature would deal combat damage to one of your opponents, it deals triple that damage to that player instead.
   * -X: Jeska deals X damage to each of up to three targets.
   * ~ can be your commander.
   * Partner */

  enum
  {
    CHOICE_TRIPLE_DAMAGE = 1,
    CHOICE_DAMAGE_3_TARGETS = 2
  };

  int i;
  bool too_many_loyalty;
  card_instance_t *instance;
  int choice_or_index;
  int target_or_loyalty;
  unsigned int amt;
  int next_index;
  inst_info_t damage_record;
  int player_is_tagetted[2];
  target_definition_t td;
  target_definition_t td3;
  char buffer[100];

  check_legend_rule(player, card, event);
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      add_counters(player, card, player, card, COUNTER_LOYALTY,
                   count_how_many_times_a_commander_was_played(player));
    }
  }
  else if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    default_target_definition(player, card, &td3, 0x102);
    td3.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    choice_or_index = can_target(&td3);
    choice_or_index =
        dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, DLG_MODE,
                    "Target creature deals triple damage until EOT", can_target(&td), 10, 0,
                    "X damage to up to 3 targets", choice_or_index, 5, 0, 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (choice_or_index == 0)
      {
        return 0;
      }
    }
    else
    {

      instance->targets[0].card = -1;
      instance->targets[0].player = -1;
      instance->targets[1].card = -1;
      instance->targets[1].player = -1;
      instance->targets[2].card = -1;
      instance->targets[2].player = -1;
      instance->targets[3].card = -1;
      instance->targets[3].player = -1;
      instance->number_of_targets = '\0';
      if (choice_or_index == CHOICE_TRIPLE_DAMAGE)
      {
        new_pick_target(&td, "TARGET_CREATURE", 0, 1);
      }
      else if (choice_or_index == CHOICE_DAMAGE_3_TARGETS)
      {

        player_is_tagetted[0] = 0;
        player_is_tagetted[1] = 0;
        choice_or_index = 0;
        do
        {
          next_index = choice_or_index + 1;
          scnprintf(buffer, 100, "Select target %d", next_index);
          if (new_pick_target(&td3, buffer, choice_or_index, 0x80) != 0)
          {
            target_or_loyalty = instance->targets[choice_or_index].card;
            choice_or_index = instance->targets[choice_or_index].player;
            if (target_or_loyalty == -1)
            {
              if (player_is_tagetted[choice_or_index] != 0)
              {
                spell_fizzled = 1;
                break;
              }
              player_is_tagetted[choice_or_index] = 1;
            }
            else
            {
              state_untargettable(choice_or_index, target_or_loyalty, 1);
            }
          }
          choice_or_index = next_index;
        } while (next_index != 3);
        i = 0;
        if (instance->number_of_targets != '\0')
        {
          do
          {
            target_or_loyalty = instance->targets[i].card;
            if (-1 < target_or_loyalty)
            {
              state_untargettable(instance->targets[i].player, target_or_loyalty, 0);
            }
            i = i + 1;
          } while (i < (int)(unsigned int)instance->number_of_targets);
        }
        choice_or_index = count_counters(player, card, COUNTER_LOYALTY);
        if (player == 0)
        {
          target_or_loyalty =
              choose_a_number(0, "How many Loyalty counters you will remove?", choice_or_index);
          too_many_loyalty = choice_or_index < target_or_loyalty;
        }
        else
        {
          target_or_loyalty = choice_or_index + -1;
          too_many_loyalty = false;
        }
        if ((target_or_loyalty < 0) || (too_many_loyalty))
        {
          spell_fizzled = 1;
          return 0;
        }
        remove_counters(player, card, player, card, COUNTER_LOYALTY, target_or_loyalty);
        set_inst_info(instance, 0x567e, 0, (uint16_t)target_or_loyalty);
      }
    }
  }
  else if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
  {

    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    default_target_definition(player, card, &td3, 0x102);
    td3.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    choice_or_index = can_target(&td3);
    choice_or_index =
        dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, DLG_MODE,
                    "Target creature deals triple damage until EOT", can_target(&td), 10, 0,
                    "X damage to up to 3 targets", choice_or_index, 5, 0, 0, 0, 0, 0);
    if (choice_or_index == CHOICE_TRIPLE_DAMAGE)
    {
      if (validate_target(player, card, &td, 0) != 0)
      {
        create_targetted_legacy_effect(player, card,
                                       attached_creature_will_deal_triple_damage_until_eot,
                                       instance->targets[0].player, instance->targets[0].card);
      }
    }
    else if (choice_or_index == CHOICE_DAMAGE_3_TARGETS)
    {

      damage_record = inst_info(instance, 0x567e);
      amt = ((uint32_t)damage_record.id | ((uint32_t)damage_record.data2 << 16)) >> 0x10;
      if ((amt != 0) && (instance->number_of_targets != '\0'))
      {
        i = 0;
        do
        {
          if (validate_target(player, card, &td3, i) != 0)
          {
            damage_creature(instance->targets[i].player, instance->targets[i].card, amt, player,
                            card);
          }
          i = i + 1;
        } while (i < (int)(unsigned int)instance->number_of_targets);
      }
    }
  }
  return planeswalker(player, card, event, 0);
}

/* ManalinkEh 2022: 0x25a1310; original cards/commander_legends.c:3060. */
static int juri_master_of_the_revue_effect(int player, int card, event_t event)
{
  uint8_t saved_kill_code;
  int target_card;
  card_instance_t *instance;
  card_instance_t *dying;
  int source_player_or_result;
  unsigned int sacrifices;
  int target_available;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (source_player_or_result =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           source_player_or_result != 0x5680))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (source_player_or_result = effect_follows_control_of_attachment(player, card, 0x4700),
         source_player_or_result != 0))
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((-1 < instance->damage_target_player) &&
        (source_player_or_result = effect_follows_control_of_attachment(player, card, event),
         source_player_or_result != 0))
    {
      return 0;
    }
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {
      instance = get_card_instance(player, card);
      dying = in_play(affected_card_controller, affected_card);
      if (dying != NULL)
      {

        dying = get_card_instance(affected_card_controller, affected_card);
        saved_kill_code = dying->kill_code;
        if (saved_kill_code != '\0')
        {

          if (((-1 < instance->damage_target_player) &&
               (instance->damage_target_card == affected_card)) &&
              (instance->damage_target_player == affected_card_controller))
          {
            remove_status(player, card, 0x10000);
            if (instance->targets[0xb].player < 0)
            {
              instance->targets[0xb].player = 0;
            }
            SET_BYTE1(instance->targets[0xb].player) = 1;
            return 0;
          }
          if (((player == affected_card_controller) &&
               (source_player_or_result = is_what(player, affected_card, 0x47),
                saved_kill_code == '\x03')) &&
              ((source_player_or_result != 0 &&
                (source_player_or_result =
                     check_special_flags(affected_card_controller, affected_card, 0x800),
                 source_player_or_result == 0))))
          {
            if (instance->targets[0xb].player < 0)
            {
              instance->targets[0xb].player = 0;
            }
            *(char *)&instance->targets[0xb].player = (char)instance->targets[0xb].player + '\x01';
          }
        }
      }
    }
  }
  sacrifices = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_GRAVEYARD_FROM_PLAY && sacrifices == 0)
    return 0;
  if (sacrifices != 0)
  {
    source_player_or_result = check_status(player, card, 0x10000);
    if (source_player_or_result == 0)
    {

      instance = get_card_instance(player, card);
      source_player_or_result = instance->targets[0].player;
      target_card = instance->targets[0].card;
      default_target_definition(source_player_or_result, target_card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td.allow_cancel = 0;
      add_counters(player, card, source_player_or_result, target_card, COUNTER_P1_P1,
                   sacrifices & 0xff);
      target_available = can_target(&td);
      if ((target_available != 0) &&
          (target_available = new_pick_target(&td, "Select a target.", 0, 0x80),
           target_available != 0))
      {
        damage_target0(source_player_or_result, target_card,
                       get_power(source_player_or_result, target_card));
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) &&
      (source_player_or_result = check_status(player, card, 0x10000), source_player_or_result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a9b50; original cards/commander_legends.c:3134. */
int card_juri_master_of_the_revue(int player, int card, event_t event)
{
  /* Juri, Master of the Revue	|B|R
   * Legendary Creature - Human Shaman 1/1
   * Whenever you sacrifice a permanent, put a +1/+1 counter on Juri.
   * When Juri dies, it deals damage equal to its power to any target. */

  int c;
  card_instance_t *instance;
  int result_or_legacy;
  unsigned int sacrifices;

  check_legend_rule(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      bool found = false;
      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          instance = in_play(player, c);

          if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
               (instance = get_card_instance(player, c),
                instance->info_slot_fn == juri_master_of_the_revue_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            found = true;
            break;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }

      if (!found)
      {
        result_or_legacy = create_targetted_legacy_effect(
            player, card, juri_master_of_the_revue_effect, player, card);
        instance = get_card_instance(player, result_or_legacy);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, result_or_legacy, 0x10000);
      }
    }
  }
  else
  {

    if (((event == EVENT_GRAVEYARD_FROM_PLAY) &&
         (instance = in_play(affected_card_controller, affected_card), instance != NULL)) &&
        ((affected_card_controller == player &&
          ((((affected_card != card &&
              (result_or_legacy = is_what(player, affected_card, 0x47), result_or_legacy != 0)) &&
             (instance->kill_code == '\x03')) &&
            (result_or_legacy = check_special_flags(affected_card_controller, affected_card, 0x800),
             result_or_legacy == 0))))))
    {

      instance = get_card_instance(player, card);
      if (instance->targets[0xb].player < 0)
      {
        instance->targets[0xb].player = 0;
      }
      *(char *)&instance->targets[0xb].player = (char)instance->targets[0xb].player + '\x01';
    }
  }
  instance = get_card_instance(player, card);

  if ((instance->kill_code == '\0') &&
      (sacrifices = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY),
       sacrifices != 0))
  {

    add_counters(player, card, player, card, COUNTER_P1_P1, sacrifices & 0xff);
    return 0;
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25a9da0; original cards/commander_legends.c:3195. */
int card_kamahls_will(int player, int card, event_t event)
{
  /* Kamahl's Will	|3|G
   * Instant
   * Choose one. If you control a commander as you cast this spell, you may choose both instead.
   * * Until end of turn, any number of target lands you control become 1/1 Elemental creatures with vigilance, indestructible, and haste. They're still lands.
   * * Choose target creature you don't control. Each creature you control deals damage equal to its power to that creature. */

  int i;
  target_t *target;
  target_t *selected_target;
  unsigned char damage_mode;
  card_instance_t *instance;
  int power_or_available;
  card_instance_t *legacy_or_creature;
  unsigned int selected_modes;
  int eligible;
  int *marked_cursor;
  int value;
  bool more_marked;

  unsigned int damage_target;
  target_definition_t td;
  target_definition_t td2;
  int marked[151];

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 1);
    td.preferred_controller = player;
    td.allowed_controller = player;
    default_target_definition(player, card, &td2, 2);
    td2.allowed_controller = 1 - player;
    td2.preferred_controller = td2.allowed_controller;
    get_card_instance(player, card);
    if (basic_spell(player, card, EVENT_CAN_CAST) == 0)
    {
      return 0;
    }
    if (can_target(&td) != 0)
    {
      return 1;
    }
    value = can_target(&td2);
    return (unsigned int)(value != 0);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 1);
      td.preferred_controller = player;
      td.allowed_controller = player;
      default_target_definition(player, card, &td2, 2);
      td2.allowed_controller = 1 - player;
      td2.preferred_controller = td2.allowed_controller;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        instance->info_slot = 0;
        value = can_target(&td);
        power_or_available = can_target(&td2);
        if ((power_or_available == 0) || (value == 0))
        {
          selected_modes = 0;
        }
        else
        {
          eligible = liutenant(player);
          selected_modes = (unsigned int)(eligible != 0);
        }
        selected_modes = dialog_impl(
            player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_MODE, "Animate lands", value, 10,
            "All your creatures deals damage to target creature you don't control",
            power_or_available, 0x14, "Do both", selected_modes, 0x1e, 0, 0, 0, 0);
        if (selected_modes == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if ((selected_modes & 1) != 0)
        {
          i = 0;
          value = can_target(&td);
          do
          {
            if ((value == 0) ||
                (value = new_pick_target(&td, "Select target land you control.", i, 0x80),
                 value == 0))
              break;
            state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
            i = i + 1;
            ai_modifier =
                ai_modifier +
                (current_phase <= (current_turn == player ? PHASE_MAIN1 : PHASE_BEFORE_BLOCKING)
                     ? 20
                     : -20);
            value = can_target(&td);
          } while (i < 10);
          if (instance->number_of_targets != '\0')
          {
            value = 0;
            do
            {
              selected_target = instance->targets + value;
              target = instance->targets + value;
              value = value + 1;
              state_untargettable(target->player, selected_target->card, 0);
            } while (value < (int)(unsigned int)instance->number_of_targets);
          }
        }
        if (((selected_modes & 2) != 0) &&
            (value = new_pick_target(&td2, "Select target creature you don\'t control.",
                                     (unsigned int)instance->number_of_targets, 0x81),
             value != 0))
        {
          value = count_subtype(player, 2, -1);
          power_or_available = value * 0x14;
          if ((current_turn != player) || (0x14 < current_phase))
          {
            power_or_available = value * -0x14;
          }
          ai_modifier = ai_modifier + power_or_available;
        }
      }
    }
    return 0;
  }
  if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
      (event != EVENT_CHANGE_TARGET))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 1);
  td.preferred_controller = player;
  td.allowed_controller = player;
  default_target_definition(player, card, &td2, 2);
  td2.allowed_controller = 1 - player;
  td2.preferred_controller = td2.allowed_controller;
  instance = get_card_instance(player, card);
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }

  damage_mode = instance->mode & 2;
  if (instance->mode & 1)
  {
    int land_count = instance->number_of_targets;
    if (damage_mode != 0)
      --land_count;
    for (int target_index = 0; target_index < land_count; ++target_index)
      if (validate_target(player, card, &td, target_index))
      {
        int legacy_card = turn_into_creature(player, card, instance->targets[target_index].player,
                                             instance->targets[target_index].card, 1, 1, 1);
        legacy_or_creature = get_card_instance(player, legacy_card);
        legacy_or_creature->targets[8].card = 0x80c;
        legacy_or_creature = get_card_instance(player, legacy_card);
        legacy_or_creature->targets[9].card = 0x3a;
        selected_target = instance->targets + target_index;
        target = instance->targets + target_index;
        get_abilities(target->player, selected_target->card, EVENT_CHANGE_TYPE, -1);
      }
  }
  if (damage_mode != 0)
  {
    damage_target = instance->number_of_targets - 1;
    damage_target = (int)(damage_target ^ 0xffffffff) >> 0x1f & damage_target;
    value = validate_target(player, card, &td2, damage_target);
    if ((value != 0) && (0 < active_cards_count[player]))
    {
      i = 0;
      value = 0;
      do
      {
        legacy_or_creature = in_play(player, i);
        if ((legacy_or_creature != NULL) && (eligible = is_what(player, i, 2), eligible != 0))
        {
          marked[value] = i;
          value = value + 1;
        }
        i = i + 1;
      } while (i < active_cards_count[player]);
      if (0 < value)
      {
        marked_cursor = marked + value + -1;
        do
        {
          value = *marked_cursor;
          power_or_available = get_power(player, value);
          damage_creature(instance->targets[damage_target].player,
                          instance->targets[damage_target].card, power_or_available, player, value);
          more_marked = marked != marked_cursor;
          marked_cursor = marked_cursor + -1;
        } while (more_marked);
      }
    }
  }
  kill_card(player, card, KILL_BURY);
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25aa4f0; original cards/commander_legends.c:3299. */
int card_kamahl_heart_of_krosa(int player, int card, event_t event)
{
  /* Kamahl, Heart of Krosa	|6|G|G
   * Legendary Creature - Human Druid 5/5
   * At the beginning of combat on your turn, creatures you control get +3/+3 and gain trample until end of turn.
   * |1|G: Until end of turn, target land you control becomes a 1/1 Elemental creature with vigilance, indestructible, and haste. It's still a land.
   * Partner (You can have two commanders if both have partner.) */

  int result;
  card_instance_t *instance;

  target_definition_t td;
  check_legend_rule(player, card, event);
  result = beginning_of_combat(player, card, event, player);
  if (result != 0)
  {
    pump_creatures_until_eot(player, card, player, 0, 3, 3, 0x80, 0, NULL);
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 1);
    td.preferred_controller = player;
    td.allowed_controller = player;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 0, 0, 1, 0, 0,
                                              0, &td, NULL);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 6, 1, 0, 0, 1, 0, 0, 0, &td,
                                     "Select target land you control.");
      if (spell_fizzled == 1)
      {
        return 0;
      }
      ai_modifier =
          ai_modifier +
          (current_phase <= (current_turn == player ? PHASE_MAIN1 : PHASE_BEFORE_BLOCKING) ? 20
                                                                                           : -20);
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 1);
  }
  td.allowed_controller = player;
  if ((event == EVENT_RESOLVE_ACTIVATION) && (td.preferred_controller = td.allowed_controller,
                                              result = valid_target(&td), result != 0))
  {
    instance = get_card_instance(player, card);
    result = turn_into_creature(player, card, instance->targets[0].player,
                                instance->targets[0].card, 1, 1, 1);
    instance = get_card_instance(player, result);
    instance->targets[8].card = 0x80c;
    instance = get_card_instance(player, result);
    instance->targets[9].card = 0x3a;
    return 0;
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x25aa7a0; original cards/commander_legends.c:3345. */
int card_kangees_lieutenant(int player, int card, event_t event)
{
  /* Kangee's Lieutenant	|2|W
   * Creature - Bird Soldier 1/1
   * Flying
   * Whenever this creature attacks, attacking creatures with flying get +1/+1 until end of turn.
   * Encore |5|W (|5|W, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_KANGEES_LIEUTENANT = 22147
  };

  int attacker_index;
  card_instance_t *instance;
  int eligible;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);

  if ((event == EVENT_DAT_ABILITY) &&
      (attacker_index = active_cards_count[player] + -1, -1 < attacker_index))
  {
    do
    {
      instance = in_play(player, attacker_index);
      if ((instance != NULL) &&
          (((eligible = is_what(player, attacker_index, 2),
             eligible != 0 && (eligible = is_attacking(player, attacker_index), eligible != 0)) &&
            (eligible = check_for_ability(player, attacker_index, 0x20), eligible != 0))))
      {
        pump_until_eot_merge_previous(player, card, player, attacker_index, 1, 1);
      }
      attacker_index = attacker_index + -1;
    } while (attacker_index != -1);
  }
  return encore(player, card, event, CARD_ID_KANGEES_LIEUTENANT, 5, 0, 0, 0, 0, 1);
}

/* ManalinkEh 2022: 0x25aa8c0; original cards/commander_legends.c:3367. */
int card_kangee_sky_warden(int player, int card, event_t event)
{
  /* Kangee, Sky Warden	|3|W|U
   * Legendary Creature - Bird Wizard 3/3
   * Flying, vigilance
   * Whenever Kangee attacks, attacking creatures with flying get +2/+0 until end of turn.
   * Whenever Kangee blocks, blocking creatures with flying get +0/+2 until end of turn. */

  int index_or_result;
  card_instance_t *instance;
  int eligible;

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);

  if ((event == EVENT_DAT_ABILITY) &&
      (index_or_result = active_cards_count[player] + -1, -1 < index_or_result))
  {
    do
    {
      instance = in_play(player, index_or_result);
      if ((instance != NULL) &&
          (((eligible = is_what(player, index_or_result, 2),
             eligible != 0 && (eligible = is_attacking(player, index_or_result), eligible != 0)) &&
            (eligible = check_for_ability(player, index_or_result, 0x20), eligible != 0))))
      {
        pump_until_eot_merge_previous(player, card, player, index_or_result, 2, 0);
      }
      index_or_result = index_or_result + -1;
    } while (index_or_result != -1);
  }
  index_or_result = blocking(player, card, event);

  if (((index_or_result != 0) &&
       (index_or_result = is_humiliated(player, card, AL_TRIG), index_or_result == 0)) &&
      ((current_turn != player &&
        (index_or_result = active_cards_count[player] + -1, -1 < index_or_result))))
  {
    do
    {
      instance = in_play(player, index_or_result);
      if (((instance != NULL) && (eligible = is_what(player, index_or_result, 2), eligible != 0)) &&
          ((eligible = blocking(player, index_or_result, event),
            eligible != 0 &&
                (eligible = check_for_ability(player, index_or_result, 0x20), eligible != 0))))
      {
        pump_until_eot_merge_previous(player, card, player, index_or_result, 0, 2);
      }
      index_or_result = index_or_result + -1;
    } while (index_or_result != -1);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25aaab0; original cards/commander_legends.c:3409. */
int card_keeper_of_the_accord(int player, int card, event_t event)
{
  /* Keeper of the Accord	|3|W
   * Creature - Human Soldier 3/4
   * At the beginning of each opponent's end step, if that player controls more creatures than you, create a 1/1 white Soldier creature token.
   * At the beginning of each opponent's end step, if that player controls more lands than you, you may search your library for a basic Plains card, put it onto the battlefield tapped, then shuffle. */

  int p;
  int c;
  int end_step;
  card_instance_t *instance;
  int other_player_or_choice;
  const char *land_prompt;
  int l_count[2];
  int c_count[2];
  char buffer[50];
  test_definition_t test;

  end_step = eot_trigger(player, card, event);
  p = current_turn;
  if ((end_step != 0) && (current_turn != player))
  {

    l_count[0] = 0;
    c = active_cards_count[current_turn] + -1;
    l_count[1] = 0;
    c_count[0] = 0;
    c_count[1] = 0;
    if (-1 < c)
    {
      do
      {
        instance = in_play(p, c);
        if (instance != NULL)
        {
          if (is_what(p, c, 2) != 0)
          {
            c_count[p] = c_count[p] + 1;
          }
          if (is_what(p, c, 1) != 0)
          {
            l_count[p] = l_count[p] + 1;
          }
        }
        c = c + -1;
      } while (c != -1);
    }
    other_player_or_choice = 1 - current_turn;
    c = active_cards_count[other_player_or_choice] + -1;
    if (-1 < c)
    {
      do
      {
        instance = in_play(other_player_or_choice, c);
        if (instance != NULL)
        {
          if (is_what(other_player_or_choice, c, 2) != 0)
          {
            c_count[other_player_or_choice] = c_count[other_player_or_choice] + 1;
          }
          if (is_what(other_player_or_choice, c, 1) != 0)
          {
            l_count[other_player_or_choice] = l_count[other_player_or_choice] + 1;
          }
        }
        c = c + -1;
      } while (c != -1);
    }
    if (c_count[player] < c_count[1 - player])
    {
      generate_token_by_id(player, card, CARD_ID_SOLDIER);
    }
    land_prompt = get_hacked_land_text(player, card, " Search for a basic %s\n Decline", 0x5007);
    scnprintf(buffer, 0x32, land_prompt);
    if ((l_count[player] < l_count[1 - player]) &&
        (other_player_or_choice = do_dialog(player, player, card, -1, -1, buffer, 0),
         other_player_or_choice == 0))
    {
      land_prompt = get_hacked_land_text(player, card, "Select a basic %s card.", 0x5007);
      new_default_test_definition(&test, 1, land_prompt);
      test.subtype = get_hacked_subtype(player, card, 0x5007);
      test.sub2 = 0x2000;
      test.subtype_flag = 4;
      new_global_tutor(player, player, 1, 5, 0, 5, &test);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25a0390; original cards/commander_legends.c:3447. */
static int commander_test(int iid, intptr_t unused, int player, int card)
{
  int result;

  result = check_special_flags2(player, card, 0x1000000);
  return (unsigned int)(result != 0);
}

/* ManalinkEh 2022: 0x25aad60; original cards/commander_legends.c:3452. */
int card_keleth_sunmane_familiar(int player, int card, event_t event)
{
  /* Keleth, Sunmane Familiar	|1|W
   * Legendary Creature - Horse 1/1
   * Whenever a commander you control attacks, put a +1/+1 counter on it.
   * Partner (You can have two commanders if both have partner.) */

  check_legend_rule(player, card, event);
  if (event == EVENT_DECLARE_ATTACKERS)
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_CREATURE);
    test.special_selection_function = commander_test;
    store_attackers(player, card, event, DAT_TRACK, player, -1, &test);
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    return 0;
  }
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    const uint8_t *attackers = (const uint8_t *)&instance->targets + 2 * sizeof(target_t);
    for (int index = instance->info_slot - 1; index >= 0; --index)
      if (in_play(current_turn, attackers[index]))
        add_counters(player, card, current_turn, attackers[index], COUNTER_P1_P1, 1);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25aaed0; original cards/commander_legends.c:3479. */
int card_keskit_the_flesh_sculptor(int player, int card, event_t event)
{
  /* Keskit, the Flesh Sculptor	|2|B
   * Legendary Creature - Phyrexian Human Artificer 1/3
   * |T, Sacrifice three other artifacts and/or creatures: Look at the top three cards of your library. Put two of them into your hand and the other into your graveyard.
   * Partner (You can have two commanders if both have partner.) */

  int hand_count_local;
  int minideck_count;
  test_definition_t this_test;
  test_definition_t this_test2;

  check_legend_rule(player, card, event);
  if (!IS_GAA_EVENT(event))
    return 0;

  new_default_test_definition(&this_test, 0x42, "Select an artifact or creature to sacrifice.");
  this_test.not_me = 1;
  this_test.qty = 3;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    hand_count_local = count_deck(player);
    minideck_count = count_deck(player);
    if (minideck_count != 0)
    {
      if (2 < hand_count_local)
      {
        hand_count_local = 2;
      }
      if (3 < minideck_count)
      {
        minideck_count = 3;
      }
      new_default_test_definition(&this_test2, 0xff, "Select a card.");
      this_test2.qty = (unsigned short)hand_count_local;
      this_test2.create_minideck = (unsigned short)minideck_count;
      this_test2.no_shuffle = 1;
      new_global_tutor(player, player, 1, 1, 1, 1, &this_test2);
      if (0 < minideck_count - hand_count_local)
      {
        mill(player, minideck_count - hand_count_local);
      }
    }
  }
  return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED, 0, 0,
                                                          0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE,
                                                          &this_test, SARV_0);
}

/* ManalinkEh 2022: 0x25ab110; original cards/commander_legends.c:3517. */
int card_kinsbaile_courier(int player, int card, event_t event)
{
  /* Kinsbaile Courier	|2|W
   * Creature - Kithkin Soldier 2/1
   * When this creature enters, put a +1/+1 counter on target creature.
   * Encore |2|W (|2|W, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_KINSBAILE_COURIER = 22153
  };

  int result;
  card_instance_t *instance;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 0;
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_CREATURE");
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_P1_P1, 1);
      }
    }
    instance->number_of_targets = '\0';
  }
  result = encore(player, card, event, CARD_ID_KINSBAILE_COURIER, 2, 0, 0, 0, 0, 1);
  return result;
}

/* ManalinkEh 2022: 0x25a12d0; original cards/commander_legends.c:3543. */
static char *attacking_the_same_player_or_planeswalker(int who_chooses, int player, int card,
                                                       int targeting_player, int targeting_card)
{
  int source_defender;
  int candidate_defender;
  char *error;

  source_defender = planeswalker_its_attacking(targeting_player, targeting_card);
  candidate_defender = planeswalker_its_attacking(player, card);
  error = NULL;
  if (source_defender != candidate_defender)
  {
    error = "target creature must attack the same player or planeswalker";
  }
  return error;
}

/* ManalinkEh 2022: 0x25ab240; original cards/commander_legends.c:3552. */
int card_kitesail_skirmisher(int player, int card, event_t event)
{
  /* Kitesail Skirmisher	|3|U
   * Creature - Human Pirate 3/1
   * Flying
   * Whenever this creature attacks, another target creature attacking the same player or planeswalker gains flying until end of turn.
   * Encore |4|U (|4|U, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_KITESAIL_SKIRMISHER = 22154
  };

  int result;
  card_instance_t *instance;
  target_definition_t td;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    base_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.required_state = TARGET_STATE_ATTACKING;
    td.extra = (intptr_t)attacking_the_same_player_or_planeswalker;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(
          &td, "Select target creature attacking the same player or planeswalker.", 0, 0x80);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                               0, 0, 0x20, 0);
      }
    }
  }
  result = encore(player, card, event, CARD_ID_KITESAIL_SKIRMISHER, 4, 0, 1, 0, 0, 0);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25ab390; original cards/commander_legends.c:3577. */
int card_kodama_of_the_east_tree(int player, int card, event_t event)
{
  /* Kodama of the East Tree	|4|G|G
   * Legendary Creature - Spirit 6/6
   * Reach
   * Whenever another permanent you control enters, if it wasn't put onto the battlefield with this ability, you may put a permanent card with equal or lesser mana value from your hand onto the battlefield.
   * Partner (You can have two commanders if both have partner.) */

  int result;
  char buffer[100];
  test_definition_t test;
  check_legend_rule(player, card, event);
  if ((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (trigger_cause_controller == player))
  {
    result = check_special_flags3(player, trigger_cause, SF3_IN_PLAY_VIA_KODAMA_EAST_TREE);
    if (result == 0)
    {
      default_test_definition(&test, 0x47);
      test.not_me = 1;
      result = duh_mode(player);
      etb_ability_global(player, event, (result != 0) + 1, TYPE_NONE, &test, player, card);
    }
  }
  if (event == EVENT_ETB)
  {
    result = get_cmc(trigger_cause_controller, trigger_cause);
    scnprintf(buffer, 100, "Select a permanent card with CMC %d or less.", result);
    new_default_test_definition(&test, 0x47, buffer);
    test.cmc_flag = 3;
    test.cmc = result + 1;
    result = new_select_a_card(player, player, 4, 0, 1, -1, &test);
    if (-1 < result)
    {
      set_special_flags(player, result, SF_NOT_CAST);
      set_special_flags3(player, result, SF3_IN_PLAY_VIA_KODAMA_EAST_TREE);
      put_into_play(player, result);
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x25ab540; original cards/commander_legends.c:3615. */
int card_krark_the_thumbless(int player, int card, event_t event)
{
  /* Krark, the Thumbless	|1|R
   * Legendary Creature - Goblin Wizard 2/2
   * Whenever you cast an instant or sorcery spell, flip a coin. If you lose the flip, return that spell to its owner's hand. If you win the flip, copy that spell, and you may choose new targets for the copy.
   * Partner (You can have two commanders if both have partner.) */

  int result;

  check_legend_rule(player, card, event);
  sp_ability_trigger(player, event, 2, TYPE_SPELL, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }

  result = flip_a_coin(player, card);
  if (result != 0)
  {
    copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
    return 0;
  }
  bounce_permanent(trigger_cause_controller, trigger_cause);
  return 0;
}

/* ManalinkEh 2022: 0x25ab600; original cards/commander_legends.c:3638. */
int card_kwain_itinerant_meddler(int player, int card, event_t event)
{
  /* Kwain, Itinerant Meddler	|W|U
   * Legendary Creature - Rabbit Wizard 1/3
   * |T: Each player may draw a card, then each player who drew a card this way gains 1 life. */

  int result;
  unsigned int pass;
  unsigned int player_00;
  int dc[2];

  check_legend_rule(player, card, event);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    dc[0] = 0;
    dc[1] = 0;

    pass = 0;
    do
    {
      player_00 = pass ^ current_turn;
      result = count_deck(player_00);
      result = dialog_impl(player, card, EVENT_RESOLVE_ACTIVATION, DLG_RANDOM, DLG_NO_STORAGE,
                           DLG_WHO_CHOOSES(player_00), DLG_NO_CANCEL, "Draw a card", 1,
                           (unsigned int)((9 < result) ? 100 : -100), "Decline", 1, 0, 0, 0, 0, 0);
      dc[player_00] = result;
      if (result == 1)
      {
        draw_a_card(player_00);
      }
      pass = pass + 1;
    } while (pass != 2);

    if (dc[current_turn] == 1)
    {
      gain_life(current_turn, 1);
    }
    if (dc[current_turn ^ 1U] == 1)
    {
      gain_life(current_turn ^ 1U, 1);
    }
  }
  return generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x25ab7c0; original cards/commander_legends.c:3664. */
int card_laboratory_drudge(int player, int card, event_t event)
{
  /* Laboratory Drudge	|3|U
   * Creature - Zombie Horror 3/4
   * At the beginning of each end step, draw a card if you've cast a spell from a graveyard or activated an ability of a card in a graveyard this turn. */

  card_instance_t *instance;
  inst_info_t cast_record;

  if (((event == EVENT_CAST_SPELL) &&
       (((card != affected_card || (player != affected_card_controller)) &&
         (instance = in_play(player, card), instance != NULL)))) &&
      (check_special_flags(affected_card_controller, affected_card, 0x10000000) != 0))
  {
    instance = get_card_instance(player, card);
    set_inst_info(instance, 0x568e, 0, 0x42);
  }
  if (eot_trigger(player, card, event) != 0)
  {
    instance = get_card_instance(player, card);
    cast_record = inst_info(instance, 0x568e);
    if (cast_record.data2 == 0x42)
    {
      draw_a_card(player);
      instance = get_card_instance(player, card);
      clear_info(instance, 0x568e);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25ab8e0; original cards/commander_legends.c:3683. */
int card_lathiel_the_bounteous_dawn(int player, int card, event_t event)
{
  /* Lathiel, the Bounteous Dawn	|2|G|W
   * Legendary Creature - Unicorn 2/2
   * Lifelink
   * At the beginning of each end step, if you gained life this turn, distribute up to that many +1/+1 counters among any number of other target creatures. */

  check_legend_rule(player, card, event);
  lifelink(player, card, event);
  if (!eot_trigger(player, card, event))
    return 0;

  int life_gained = get_trap_condition(player, TRAP_LIFE_GAINED);
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;
  td.special = TARGET_SPECIAL_NOT_ME;
  if (!life_gained || !can_target(&td) || !eot_trigger(player, card, event))
    return 0;

  card_instance_t *instance = get_card_instance(player, card);
  char buffer[100];
  for (int i = 0; i < life_gained;)
  {
    ++i;
    scnprintf(buffer, 100, "Select target creature which will get a +1/+1 counter (%d of %d)",
              i, life_gained);
    if (!new_pick_target(&td, buffer, 0, GS_LITERAL_PROMPT))
      break;
    add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                  COUNTER_P1_P1, 1);
  }
  instance->number_of_targets = 0;
  return 0;
}

/* ManalinkEh 2022: 0x25aba70; original cards/commander_legends.c:3717. */
int card_liesa_shroud_of_dusk(int player, int card, event_t event)
{
  /* Liesa, Shroud of Dusk	|2|W|W|B
   * Legendary Creature - Angel 5/5
   * Rather than pay |2 for each previous time you've cast this spell from the command zone this game, pay 2 life that many times.
   * Flying, lifelink
   * Whenever a player casts a spell, they lose 2 life. */

  check_legend_rule(player, card, event);
  lifelink(player, card, event);
  sp_ability_trigger(2, event, 2,
                     TARGET_TYPE_PLANESWALKER | TYPE_ARTIFACT | TYPE_SPELL | TYPE_ENCHANTMENT |
                         TYPE_CREATURE,
                     NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  lose_life(trigger_cause_controller, 2);
  return 0;
}

/* ManalinkEh 2022: 0x25a20f0; original cards/commander_legends.c:3737. */
static int card_exiled_with_aegis_counter(int player, int card, event_t event)
{

  card_instance_t *instance;
  int counters;
  int iid_or_exiled_count;
  int card_00;
  int matching_legacies;

  if (event == EVENT_SET_LEGACY_EFFECT_NAME)
  {
    instance = get_card_instance(player, card);
    if (-1 < instance->targets[0].card)
    {

      instance = get_card_instance(player, card);
      scnprintf(set_legacy_effect_name_addr, 0x33, "%s",
                cards_ptr[cards_data[instance->targets[0].card].id]->name);
    }
  }
  else
  {
    if (event == EVENT_SET_LEGACY_EFFECT_TEXT)
    {
      scnprintf(set_legacy_effect_name_addr, 0x33, "Exiled with an aegis counter.");
      return 0;
    }
    if (event == EVENT_STATIC_EFFECTS)
    {

      instance = get_card_instance(player, card);
      matching_legacies = 0;
      iid_or_exiled_count = instance->targets[0].card;
      if (0 < active_cards_count[player])
      {
        card_00 = 0;
        do
        {
          instance = in_play(player, card_00);
          if (instance != NULL)
          {
            if (is_what(player, card_00, 0x80) != 0)
            {

              instance = get_card_instance(player, card_00);
              if (instance->info_slot_fn == card_exiled_with_aegis_counter)
              {
                counters = count_counters(player, card_00, COUNTER_AEGIS);
                if ((counters != 0) && (iid_or_exiled_count == instance->targets[0].card))
                {
                  matching_legacies = matching_legacies + 1;
                }
              }
            }
          }
          card_00 = card_00 + 1;
        } while (card_00 < active_cards_count[player]);
      }
      iid_or_exiled_count = count_iid_in_rfg(player, iid_or_exiled_count);
      if (iid_or_exiled_count < matching_legacies)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
static int create_livio_legacy(int player, int card, int t_player, int t_card)
{
  int iid = get_original_internal_card_id(t_player, t_card);
  int owner = get_owner(t_player, t_card);
  int legacy = owner == player ? create_legacy_effect(player, card, card_exiled_with_aegis_counter)
      : create_legacy_effect_for_opponent(player, card, card_exiled_with_aegis_counter, -1, -1);
  if (legacy != -1)
  {
    add_counters(player, card, owner, legacy, COUNTER_AEGIS, 1);
    card_instance_t *instance = get_card_instance(owner, legacy);
    unsigned int csvid = (uint16_t)cards_data[iid].id;
    instance->targets[0].card = iid;
    instance->display_pic_csv_id = csvid;
    instance->targets[0].player = owner;
    instance->eot_toughness = 0x300;
    instance->display_pic_num = (uint16_t)get_card_image_number(csvid, t_player, t_card);
  }
  kill_card(t_player, t_card, KILL_EXILE);
  return legacy;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25abb10; original cards/commander_legends.c:3784. */
int card_livio_oathsworn_sentinel(int player, int card, event_t event)
{
  /* Livio, Oathsworn Sentinel	|1|W
   * Legendary Creature - Human Knight 2/2
   * |1|W: Choose another target creature. Its controller may exile it with an aegis counter on it.
   * |2|W, |T: Return all exiled cards with aegis counters on them to the battlefield under their owners' control.
   * Partner (You can have two commanders if both have partner.) */

  check_legend_rule(player, card, event);
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (generic_activated_ability(player, card, event, 0x40004, 1, 0, 0, 0, 0, 1, 0, &td,
                                       NULL))
      return 1;
    return generic_activated_ability(player, card, event, 1, 2, 0, 0, 0, 0, 1, 0, NULL, NULL);
  }
  if (event == EVENT_ACTIVATE)
  {
    instance->number_of_targets = 0;
    int abils[2];
    abils[0] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x40004, 1, 0, 0, 0,
                                              0, 1, 0, &td, NULL);
    abils[1] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 2, 0, 0, 0, 0, 1,
                                              0, NULL, NULL);
    int choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_MODE, DLG_AUTOCHOOSE_IF_1,
                             "Exile with Aegis counter", abils[0], 10, "Return the exiled cards",
                             abils[1], 20, 0, 0, 0, 0);
    if (choice == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (choice == 1)
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0x40004, 1, 0, 0, 0, 0, 1, 0,
                                     &td, "TARGET_CREATURE");
    else if (choice == 2)
      generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 2, 0, 0, 0, 0, 1, 0, NULL,
                                     NULL);
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->mode == 1)
    {
      if (valid_target(&td))
      {
        card_instance_t *target =
            get_card_instance(instance->targets[0].player, instance->targets[0].card);
        char buffer[100];
        scnprintf(buffer, 100, " Exile %s", get_card_name_by_instance(target));
        int controller = instance->targets[0].player;
        int choice =
            dialog_impl(player, card, EVENT_RESOLVE_ACTIVATION, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                        DLG_WHO_CHOOSES(controller), DLG_SMALLCARD(controller, instance->targets[0].card),
                        buffer, 1, controller == player ? 20 : -20, "Decline", 1, 10, 0, 0, 0, 0);
        if (choice == 1)
        {
          int target_card = instance->targets[0].card;
          int target_player = instance->targets[0].player;
          int iid = get_original_internal_card_id(target_player, target_card);
          int owner = get_owner(target_player, target_card);
          int legacy;
          if (player == owner)
            legacy = create_legacy_effect(player, card, card_exiled_with_aegis_counter);
          else
            legacy = create_legacy_effect_for_opponent(player, card, card_exiled_with_aegis_counter,
                                                       -1, -1);
          if (legacy != -1)
          {
            add_counters(player, card, owner, legacy, COUNTER_AEGIS, 1);
            card_instance_t *leg = get_card_instance(owner, legacy);
            unsigned csvid = (uint16_t)cards_data[iid].id;
            leg->targets[0].card = iid;
            leg->display_pic_csv_id = csvid;
            leg->targets[0].player = owner;
            leg->eot_toughness = 0x300;
            leg->display_pic_num =
                (uint16_t)get_card_image_number(csvid, target_player, target_card);
          }
          kill_card(target_player, target_card, KILL_EXILE);
        }
      }
    }
    else if (instance->mode == 2)
    {
      int marked[100][2];
      int count = 0;
      for (int phase = 0; phase < 2; ++phase)
      {
        int p = phase == 0 ? current_turn : 1 - current_turn;
        for (int c = active_cards_count[p] - 1; c >= 0; --c)
        {
          if (in_play(p, c) && is_what(p, c, 0x80) && count_counters(p, c, COUNTER_AEGIS) &&
              get_card_instance(p, c)->info_slot_fn == card_exiled_with_aegis_counter)
          {
            marked[count][0] = p;
            marked[count++][1] = c;
          }
        }
      }
      for (int i = count - 1; i >= 0; --i)
      {
        int p = marked[i][0], c = marked[i][1];
        card_instance_t *effect = get_card_instance(p, c);
        int owner = effect->targets[0].player, iid = effect->targets[0].card;
        kill_card(p, c, KILL_EXILE);
        int position = find_iid_in_rfg(owner, iid);
        if (position >= 0)
        {
          remove_card_from_rfg_by_position(owner, position);
          int added = add_card_to_hand(owner, iid);
          put_into_play(owner, added);
        }
      }
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x25ac340; original cards/commander_legends.c:3880. */
int card_magus_of_the_order(int player, int card, event_t event)
{
  /* Magus of the Order	|2|G|G
   * Creature - Human Wizard 3/3
   * |G, |T, Sacrifice this creature and another green creature: Search your library for a green creature card, put it onto the battlefield, then shuffle. */

  int result;
  unsigned int sacrificed;
  const char *color_prompt;
  char msg[100];
  test_definition_t this_test;
  test_definition_t this_test2;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_test_definition(&this_test, 2);
    this_test.color = get_sleighted_color_test(player, card, 8);
    this_test.not_me = 1;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 9, 0, 0, 0, 1, 0, 0, 0,
                                         NULL, NULL);
      if (result == 0)
      {
        return 0;
      }
      return new_can_sacrifice_as_cost(player, card, &this_test);
    }
    if (event == EVENT_ACTIVATE)
    {
      result = charge_mana_for_activated_ability(player, card, 0, 0, 0, 1, 0, 0);
      if (result == 0)
      {
        return 0;
      }
      color_prompt = get_sleighted_color_text(player, card, "Select a %s creature to sacrifice", 3);
      scnprintf(this_test.message, 100, color_prompt);
      sacrificed = new_sacrifice(player, card, player,
                                 SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &this_test);
      if (sacrificed != 0)
      {
        add_state(player, card, 0x10);
        tapped_for_mana_color = -1;
        kill_card(player, card, KILL_SACRIFICE);
        kill_card((int)sacrificed >> 0x10 & 0xff, sacrificed >> 0x18, KILL_SACRIFICE);
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
      return 0;
    }
    default_test_definition(&this_test, 2);
    this_test.color = get_sleighted_color_test(player, card, 8);
  }
  this_test.not_me = 1;
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  color_prompt = get_sleighted_color_text(player, card, "Select a %s creature card.", 3);
  scnprintf(msg, 100, color_prompt);
  new_default_test_definition(&this_test2, 2, msg);
  this_test2.color = get_sleighted_color_test(player, card, 8);
  new_global_tutor(player, player, 1, 4, 0, 2, &this_test2);
  return 0;
}

/* ManalinkEh 2022: 0x25ac660; original cards/commander_legends.c:3929. */
int card_malcolm_keen_eyed_navigator(int player, int card, event_t event)
{
  /* Malcolm, Keen-Eyed Navigator	|2|U
   * Legendary Creature - Siren Pirate 2/2
   * Flying
   * Whenever one or more Pirates you control deal damage to your opponents, you create a Treasure token for each opponent dealt damage. (It's an artifact with "|T, Sacrifice this token: Add one mana of any color.")
   * Partner (You can have two commanders if both have partner.) */

  int result;
  card_instance_t *instance;
  check_legend_rule(player, card, event);
  result = subtype_deals_damage(player, card, event, player, 0x95, 6);
  if (result == 0)
  {
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_TREASURE);
  instance = get_card_instance(player, card);
  instance->targets[1].card = 0;
  return 0;
}

/* ManalinkEh 2022: 0x25ac6f0; original cards/commander_legends.c:3947. */
int card_merchant_raiders(int player, int card, event_t event)
{
  /* Merchant Raiders	|3|U
   * Creature - Human Pirate 2/4
   * Whenever this creature or another Pirate you control enters, tap up to one target creature. That creature doesn't untap during its controller's untap step for as long as you control this creature. */

  int result;
  card_instance_t *instance;
  card_instance_t *legacy;

  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    target_definition_t td1;
    default_target_definition(player, card, &td1, 2);
    instance = get_card_instance(player, card);
    result = can_target(&td1);
    if (result != 0)
    {
      result = new_pick_target(&td1, "TARGET_CREATURE", 0, 0);
      if (result != 0)
      {
        tap_card(instance->targets[0].player, instance->targets[0].card);
        result =
            create_targetted_legacy_effect(player, card, dungeon_geist_effect,
                                           instance->targets[0].player, instance->targets[0].card);
        legacy = get_card_instance(player, result);
        legacy->targets[0].player = player;
        legacy->targets[0].card = card;
        legacy->number_of_targets = '\x01';
      }
    }
    instance->number_of_targets = '\0';
  }
  if ((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (trigger_cause_controller == player))
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 0x47);
    this_test.not_me = 1;
    this_test.subtype = SUBTYPE_PIRATE;
    etb_ability_global(player, event, 2, TYPE_NONE, &this_test, player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25ac870; original cards/commander_legends.c:3984. */
int card_meteoric_mace(int player, int card, event_t event)
{
  /* Meteoric Mace	|4|R|R
   * Artifact - Equipment
   * Equipped creature gets +4/+0 and has trample.
   * Equip |4
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.) */

  int result;

  cascade(player, card, event);
  result = vanilla_equipment(player, card, event, 4, 4, 0, 0x80, 0);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25a22a0; original cards/commander_legends.c:3997. */
static void miara_trigger(int player, int card, int amount)
{
  int result;
  if (amount < 1)
  {
    return;
  }
  do
  {
    result = has_mana(player, 0, 1);
    if (result == 0)
    {
      return;
    }
    result = can_pay_life(player, 1);
    if (result == 0)
    {
      return;
    }
    charge_mana(player, COLOR_COLORLESS, 1);
    if (spell_fizzled == 1)
    {
      return;
    }
    lose_life(player, 1);
    draw_cards(player, 1);
    amount = amount + -1;
  } while (amount != 0);
  return;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25a2340; original cards/commander_legends.c:4010. */
static int miara_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  unsigned int deaths;
  card_instance_t *dying;
  test_definition_t this_test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (get_id((int)instance->damage_target_player, instance->damage_target_card) != 0x5697))
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
        default_test_definition(&this_test, 0x47);
        this_test.subtype = 0x3c;

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
        count_for_gfp_ability(player, card, 0x77, player, 0, &this_test);
      }
    }
  }
  deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_GRAVEYARD_FROM_PLAY && deaths == 0)
    return 0;
  if (deaths != 0)
  {
    if (!check_status(player, card, 0x10000))
    {

      miara_trigger(player, card, deaths & 0xff);
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) && (check_status(player, card, 0x10000) == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25ac8e0; original cards/commander_legends.c:4065. */
int card_miara_thorn_of_the_glade(int player, int card, event_t event)
{
  /* Miara, Thorn of the Glade	|1|B
   * Legendary Creature - Elf Scout 1/2
   * Whenever Miara or another Elf you control dies, you may pay |1 and 1 life. If you do, draw a card.
   * Partner (You can have two commanders if both have partner.) */

  int c;
  card_instance_t *instance;
  unsigned int deaths;
  int legacy_card;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      bool found = false;
      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          instance = in_play(player, c);

          if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
               (instance = get_card_instance(player, c), instance->info_slot_fn == miara_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            found = true;
            break;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }

      if (!found)
      {
        legacy_card = create_targetted_legacy_effect(player, card, miara_effect, player, card);
        instance = get_card_instance(player, legacy_card);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, legacy_card, 0x10000);
      }
    }
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    default_test_definition(&this_test, 0x47);
    this_test.subtype = 0x3c;
    count_for_gfp_ability(player, card, 0x77, 2, 0, &this_test);
  }
  instance = get_card_instance(player, card);

  if ((instance->kill_code == '\0') &&
      (deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY), deaths != 0))
  {

    miara_trigger(player, card, deaths & 0xff);
    return 0;
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x25acac0; original cards/commander_legends.c:4117. */
int card_mnemonic_deluge(int player, int card, event_t event)
{
  /* Mnemonic Deluge	|6|U|U|U
   * Sorcery
   * Exile target instant or sorcery card from a graveyard. Copy that card three times. You may cast the copies without paying their mana costs. Exile ~. */

  int result;
  card_instance_t *instance;
  test_definition_t this_test;
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        new_default_test_definition(&this_test, 0x38, "Select an instant or sorcery card.");
        this_test.ai_selection_mode = 2;
        if ((card != affected_card) || (player != affected_card_controller))
          return generic_spell(player, card, event, 0x1000, NULL, NULL, 1, &this_test);
        generic_spell(player, card, EVENT_CAST_SPELL, 0x1000, NULL, NULL, 1, &this_test);
        if (spell_fizzled != 1)
        {
          instance = get_card_instance(player, card);
          ai_modifier = ai_modifier + cards_ptr[graveyard_ptr_mutable[instance->targets[0].player]
                                                                     [instance->targets[0].card]]
                                          ->ai_base_value;
        }
      }
      return 0;
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      new_default_test_definition(&this_test, 0x38, "Select an instant or sorcery card.");
      this_test.ai_selection_mode = 2;
      if (event == EVENT_RESOLVE_SPELL)
      {
        instance = get_card_instance(player, card);
        result = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
        if (result != -1)
        {
          result = graveyard_ptr_mutable[instance->targets[0].player][result];
          copy_spell(player, (unsigned int)cards_data[result].id);
          copy_spell(player, (unsigned int)cards_data[result].id);
          copy_spell(player, (unsigned int)cards_data[result].id);
        }
        kill_card(player, card, KILL_DESTROY);
      }
      return generic_spell(player, card, event, 0x1000, NULL, NULL, 1, &this_test);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  new_default_test_definition(&this_test, 0x38, "Select an instant or sorcery card.");
  this_test.ai_selection_mode = 2;
  return generic_spell(player, card, event, 0x1000, NULL, NULL, 1, &this_test);
}

/* ManalinkEh 2022: 0x25acd50; original cards/commander_legends.c:4156. */
int card_nadiers_nightblade(int player, int card, event_t event)
{
  /* Nadier's Nightblade	|2|B
   * Creature - Elf Warrior 1/3
   * Whenever a token you control leaves the battlefield, each opponent loses 1 life and you gain 1 life. */

  test_definition_t test;

  if ((((trigger_condition == TRIGGER_LEAVE_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    default_test_definition(&test, 0xff);
    test.type_flag = 6;
    test_leaves_play_mode(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test, 1, 1);
  }
  if (event != EVENT_LEAVES_PLAY_ABILITY)
  {
    return 0;
  }
  lose_life(1 - player, 1);
  gain_life(player, 1);
  return 0;
}

/* ManalinkEh 2022: 0x25ace40; original cards/commander_legends.c:4177. */
int card_nadier_agent_of_the_duskenel(int player, int card, event_t event)
{
  /* Nadier, Agent of the Duskenel	|5|B
   * Legendary Creature - Elf Warrior 3/3
   * Whenever a token you control leaves the battlefield, put a +1/+1 counter on Nadier.
   * When Nadier leaves the battlefield, create a number of 1/1 green Elf Warrior creature tokens equal to its power.
   * Partner (You can have two commanders if both have partner.) */

  int result;
  test_definition_t test;

  check_legend_rule(player, card, event);
  leaves_play(player, card, event, 1, 1);
  if ((((trigger_condition == TRIGGER_LEAVE_PLAY) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    default_test_definition(&test, 0xff);
    test.type_flag = 6;
    test_leaves_play(player, card, event, player, &test, 1, 1);
  }
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    result = is_token(trigger_cause_controller, trigger_cause);
    if (result != 0)
    {
      add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    }
    if ((trigger_cause == card) && (trigger_cause_controller == player))
    {
      result = get_power(player, card);
      generate_tokens_by_id(player, card, CARD_ID_ELF_WARRIOR, result);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25acfa0; original cards/commander_legends.c:4205. */
int card_natural_reclamation(int player, int card, event_t event)
{
  /* Natural Reclamation	|4|G
   * Instant
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.)
   * Destroy target artifact or enchantment. */

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);
    generic_spell(player, card, EVENT_CAST_SPELL, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
    if (spell_fizzled != 1)
      cascade(player, card, EVENT_CAST_SPELL);
    return 0;
  }
  return card_disenchant(player, card, event);
}

/* ManalinkEh 2022: 0x25ad060; original cards/commander_legends.c:4225. */
int card_necrotic_hex(int player, int card, event_t event)
{
  /* Necrotic Hex	|6|B
   * Sorcery
   * Each player sacrifices six creatures of their choice. You create six tapped 2/2 black Zombie creature tokens. */

  int i;
  unsigned int t_player;
  unsigned int pass;
  token_generation_t token;
  if (event == EVENT_RESOLVE_SPELL)
  {
    pass = 0;
    do
    {
      t_player = pass ^ current_turn;
      i = 6;
      do
      {
        if (player_sacrifices_a_permanent(player, card, t_player, 2,
                                          SAC_RETURN_CHOICE | SAC_CAUSED | SAC_NO_CANCEL) == 0)
          break;
        i = i + -1;
      } while (i != 0);
      pass = pass + 1;
    } while (pass != 2);
    default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
    token.qty = '\x06';
    token.action = TOKEN_ACTION_TAPPED;
    generate_token(&token);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x25ad130; original cards/commander_legends.c:4254. */
int card_nevinyrral_urborg_tyrant(int player, int card, event_t event)
{
  /* Nevinyrral, Urborg Tyrant	|3|W|U|B
   * Legendary Creature - Zombie Wizard 3/6
   * Hexproof from artifacts, creatures, and enchantments
   * When Nevinyrral enters, create a tapped 2/2 black Zombie creature token for each creature that died this turn.
   * When Nevinyrral dies, you may pay |1. When you do, destroy all artifacts, creatures, and enchantments. */

  int p;
  int c;
  int result_or_active_player;
  int mana_available;
  card_instance_t *instance;
  int type_or_mana_value;
  bool other_controller;

  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
    token.tou = 2;
    token.pow = 2;
    token.color_forced = '\x02';
    token.qty = (unsigned char)creatures_dead_this_turn;
    token.action = TOKEN_ACTION_TAPPED;
    generate_token(&token);
    misc_hexproof(player, card, EVENT_ETB);
  }
  else
  {
    misc_hexproof(player, card, event);
    if ((((event == EVENT_PROT_MISC) && (card == affected_card)) &&
         (player == affected_card_controller)) &&
        (result_or_active_player = is_what(attacking_card_controller, attacking_card, 0x46),
         result_or_active_player != 0))
    {
      event_result = event_result | 1;
    }
  }
  result_or_active_player = duh_mode(player);
  result_or_active_player = this_dies_trigger(
      player, card, event, (result_or_active_player != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if ((result_or_active_player != 0) &&
      (mana_available = has_mana(player, 0, 1), result_or_active_player = current_turn,
       mana_available != 0))
  {
    if (player == 1)
    {
      int totals[2][2];
      totals[0][0] = 0;
      totals[0][1] = 0;
      totals[1][0] = 0;
      totals[1][1] = 0;
      c = active_cards_count[current_turn] + -1;
      if (-1 < c)
      {
        other_controller = current_turn != 1;
        do
        {
          if (((card != c) || (other_controller)) &&
              ((instance = in_play(result_or_active_player, c),
                instance != NULL && (type_or_mana_value = is_what(result_or_active_player, c, 0x46),
                                     type_or_mana_value != 0))))
          {
            totals[result_or_active_player][0] = totals[result_or_active_player][0] + 1;
            type_or_mana_value = get_cmc(result_or_active_player, c);
            totals[result_or_active_player][1] =
                totals[result_or_active_player][1] + type_or_mana_value;
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
          if ((((card != c) || (p != 1)) && (instance = in_play(p, c), instance != NULL)) &&
              (type_or_mana_value = is_what(p, c, 0x46), type_or_mana_value != 0))
          {
            totals[p][0] = totals[p][0] + 1;
            type_or_mana_value = get_cmc(p, c);
            totals[p][1] = totals[p][1] + type_or_mana_value;
          }
          c = c + -1;
        } while (c != -1);
      }
      if (totals[0][0] - totals[1][0] < 2)
      {
        return 0;
      }
      if (totals[0][1] - totals[1][1] < 5)
      {
        return 0;
      }
    }
    if (charge_mana(player, COLOR_COLORLESS, 1) != 0)
    {
      test_definition_t this_test;
      default_test_definition(&this_test, 0x46);
      new_manipulate_all(player, card, 2, &this_test, ACT_KILL_DESTROY);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25ad4c0; original cards/commander_legends.c:4311. */
int card_nightshade_harvester(int player, int card, event_t event)
{
  /* Nightshade Harvester	|3|B
   * Creature - Elf Shaman 2/2
   * Whenever a land an opponent controls enters, that player loses 1 life. Put a +1/+1 counter on this creature. */

  test_definition_t test;

  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 1);
    etb_ability_global(1 - player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event != EVENT_ETB)
  {
    return 0;
  }
  lose_life(1 - player, 1);
  add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  return 0;
}

/* ManalinkEh 2022: 0x25ad5c0; original cards/commander_legends.c:4331. */
int card_numa_joraga_chieftain(int player, int card, event_t event)
{
  /* Numa, Joraga Chieftain	|2|G
   * Legendary Creature - Elf Warrior 2/2
   * At the beginning of combat on your turn, you may pay |X|X. When you do, distribute X +1/+1 counters among any number of target Elves.
   * Partner (You can have two commanders if both have partner.) */

  int i;
  int paid_or_triggered;
  int counter_count;
  int target_or_available;
  card_instance_t *instance;
  card_instance_t *target;
  int *target_cursor;
  int player_offset;
  int tc[2];
  target_definition_t td;
  char buffer[100];
  int targs[2][151];

  check_legend_rule(player, card, event);
  paid_or_triggered = beginning_of_combat(player, card, event, player);

  if ((paid_or_triggered != 0) &&
      (paid_or_triggered = charge_mana_for_double_x(player, 0), paid_or_triggered != 0))
  {

    counter_count = paid_or_triggered / 2;
    default_target_definition(player, card, &td, 0x47);
    td.preferred_controller = player;
    td.required_subtype = 0x3c;
    if (player == 1)
    {
      td.required_type = TYPE_CREATURE;
    }
    td.allow_cancel = 0;
    tc[0] = 0;
    tc[1] = 0;
    if ((counter_count != 0) && (target_or_available = can_target(&td), target_or_available != 0))
    {

      instance = get_card_instance(player, card);
      if (1 < paid_or_triggered)
      {
        i = 0;
        do
        {
          i = i + 1;
          scnprintf(buffer, 100, "Select target Elf which will get a +1/+1 counter (%d of %d)", i,
                    counter_count);
          if (new_pick_target(&td, buffer, 0, 0x80) == 0)
            break;
          target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
          target->state = target->state | 0x200000;
          target_or_available = instance->targets[0].player;
          instance->number_of_targets = '\0';
          targs[target_or_available][tc[target_or_available]] = instance->targets[0].card;
          tc[target_or_available] = tc[target_or_available] + 1;
        } while (i < counter_count);
      }
      player_offset = 0;
      i = 0;
      do
      {
        counter_count = tc[i];
        if (0 < counter_count)
        {
          target_cursor = targs[0] + player_offset;
          do
          {
            target_or_available = *target_cursor;
            target_cursor = target_cursor + 1;
            instance = get_card_instance(i, target_or_available);
            instance->state = instance->state & 0xffdfffff;
            add_counters(player, card, i, target_or_available, COUNTER_P1_P1, 1);
          } while (target_cursor != targs[0] + counter_count + player_offset);
        }
        i = i + 1;
        player_offset = player_offset + 0x97;
      } while (i != 2);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25ad860; original cards/commander_legends.c:4383. */
int card_nymris_oonas_trickster(int player, int card, event_t event)
{
  /* Nymris, Oona's Trickster	|3|U|B
   * Legendary Creature - Faerie Knight 1/6
   * Flash
   * Flying
   * Whenever you cast your first spell during each opponent's turn, look at the top two cards of your library. Put one of those cards into your hand and the other into your graveyard. */

  int result;
  test_definition_t test;

  check_legend_rule(player, card, event);
  if (((trigger_condition == TRIGGER_SPELL_CAST) && (1 - player == current_turn)) &&
      (trigger_cause_controller == player))
  {
    result = get_specific_storm_count(player);
    if (result == 0)
    {
      sp_ability_trigger(player, event, 2, TYPE_ANY, NULL, player, card);
    }
  }
  if (event == EVENT_SPA)
  {
    new_default_test_definition(&test, 0xff, "Select a card to put into your hand.");
    reveal_top_cards_of_library_and_choose(player, card, player, 2, 1, TUTOR_HAND, 0, TUTOR_GRAVE,
                                           0, &test);
  }
  result = flash(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25ad980; original cards/commander_legends.c:4406. */
int card_obeka_brute_chronologist(int player, int card, event_t event)
{
  /* Obeka, Brute Chronologist	|1|U|B|R
   * Legendary Creature - Ogre Wizard 3/4
   * |T: The player whose turn it is may end the turn. (Exile all spells and abilities from the stack. The player whose turn it is discards down to their maximum hand size. Damage wears off, and "this turn" and "until end of turn" effects end.) */

  check_legend_rule(player, card, event);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    int ai_choice = player != 1 || current_turn != player;
    int choice = do_dialog(current_turn, player, card, -1, -1,
                           " End current turn\n No thanks", ai_choice);
    if (choice == 0)
      end_the_turn(player, card, EVENT_RESOLVE_ACTIVATION);
  }
  return generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x25ada80; original cards/commander_legends.c:4434. */
int card_phyrexian_triniform(int player, int card, event_t event)
{
  /* Phyrexian Triniform	|9
   * Artifact Creature - Phyrexian Golem 9/9
   * When this creature dies, create three 3/3 colorless Phyrexian Golem artifact creature tokens.
   * Encore |12 (|12, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_PHYREXIAN_TRINIFORM = 22179
  };

  int result;
  token_generation_t token;

  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    default_token_definition(player, card, CARD_ID_PHYREXIAN_GOLEM, &token);
    token.tou = 3;
    token.pow = 3;
    token.qty = '\x03';
    generate_token(&token);
  }
  result = encore(player, card, event, CARD_ID_PHYREXIAN_TRINIFORM, 0xc, 0, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x25a2630; original cards/commander_legends.c:4452. */
static int plague_reaver_effect(int player, int card, event_t event)
{
  int target_player;
  int iid;
  int grave_owner;
  card_instance_t *instance;
  int grave_index;
  if (upkeep_trigger(player, card, event) != 0)
  {
    instance = get_card_instance(player, card);
    if (instance->targets[0].player == current_turn)
    {
      instance = get_card_instance(player, card);
      grave_owner = instance->targets[1].player;
      target_player = instance->targets[0].player;
      iid = instance->targets[1].card;
      grave_index = count_graveyard(grave_owner);
      grave_index = grave_index + -1;
      if (-1 < grave_index)
      {
        do
        {
          if (iid == graveyard_ptr_mutable[grave_owner][grave_index])
          {
            reanimate_permanent(target_player, -1, grave_owner, grave_index, REANIMATE_DEFAULT);
            break;
          }
          grave_index = grave_index + -1;
        } while (grave_index != -1);
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25adb40; original cards/commander_legends.c:4471. */
int card_plague_reaver(int player, int card, event_t event)
{
  /* Plague Reaver	|2|B
   * Creature - Beast 6/5
   * At the beginning of your end step, sacrifice each other creature you control.
   * Discard two cards, Sacrifice this creature: Choose target opponent. Return this creature to the battlefield under that player's control at the beginning of their next upkeep. */

  target_definition_t td;
  int32_t target_player;
  int value;
  card_instance_t *instance;
  card_instance_t *legacy;
  int opponent_creatures;
  bool upkeep_phase;

  value = eot_trigger(player, card, event);
  if ((value != 0) && (current_turn == player))
  {
    test_definition_t this_test;
    default_test_definition(&this_test, 2);
    this_test.not_me = 1;
    new_manipulate_all(player, card, player, &this_test, ACT_KILL_SACRIFICE);
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (hand_count[player] < 2)
      {
        return 0;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x400008, 0, 0, 0, 0, 0, 0,
                                       0, &td, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      value = count_subtype(player, 2, -1);
      opponent_creatures = count_subtype(1 - player, 2, -1);
      ai_modifier = ai_modifier + value * 0x14 + -0x3c + opponent_creatures * 10;
      value = get_owner(player, card);
      instance->targets[1].player = value;
      value = get_original_internal_card_id(player, card);
      instance->targets[1].card = value;
      new_multidiscard(player, 2, 4, player);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0x400008, 0, 0, 0, 0, 0, 0, 0, &td,
                                NULL);
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
    td.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  legacy = get_card_instance(player, create_legacy_effect(player, card, plague_reaver_effect));
  target_player = instance->targets[0].player;
  legacy->targets[1].card = instance->targets[1].card;
  upkeep_phase = current_phase == 4;
  legacy->targets[0].player = target_player;
  legacy->targets[1].player = (intptr_t)upkeep_phase;
  return 0;
}
#endif

/* ManalinkEh 2022: 0x25ade60; original cards/commander_legends.c:4531. */
int card_prava_of_the_steel_legion(int player, int card, event_t event)
{
  /* Prava of the Steel Legion	|2|W
   * Legendary Creature - Cat Soldier 1/4
   * During your turn, creature tokens you control get +1/+4.
   * |3|W: Create a 1/1 white Soldier creature token.
   * Partner (You can have two commanders if both have partner.) */

  int result;
  card_instance_t *instance;

  check_legend_rule(player, card, event);
  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if ((((affected_card_controller == player) && (current_turn == player)) &&
         (instance = in_play(player, card), instance != NULL)) &&
        ((result = is_humiliated(player, card, AL_STATIC),
          result == 0 &&
              (result = is_token(affected_card_controller, affected_card), result != 0))))
    {
      if (event == EVENT_POWER)
      {
        event_result = event_result + 1;
      }
      else
      {
        event_result = event_result + 4;
      }
    }
    return 0;
  }

  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_ACTIVATE && current_turn != player && current_phase < 0x17)
    ai_modifier += 0x30;
  if (event == EVENT_RESOLVE_ACTIVATION)
    generate_token_by_id(player, card, CARD_ID_SOLDIER);
  return generic_activated_ability(player, card, event, 0, 3, 0, 0, 0, 0, 1, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x25adff0; original cards/commander_legends.c:4564. */
int card_pride_of_the_perfect(int player, int card, event_t event)
{
  /* Pride of the Perfect	|3|B
   * Enchantment
   * Elves you control get +2/+0. */

  boost_creature_type(player, card, event, SUBTYPE_ELF, 2, 0, 0,
                      BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25ae050; original cards/commander_legends.c:4575. */
int card_profane_transfusion(int player, int card, event_t event)
{
  /* Profane Transfusion	|6|B|B|B
   * Sorcery
   * Two target players exchange life totals. You create an X/X colorless Horror artifact creature token, where X is the difference between those players' life totals. */

  int opponent_life;
  card_instance_t *instance;
  int life_or_valid;
  int difference;
  token_generation_t token;
  target_definition_t td;
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    life_or_valid = would_validate_arbitrary_target(&td, player, -1);
    if ((life_or_valid != 0) &&
        (life_or_valid = would_validate_arbitrary_target(&td, 1 - player, -1), life_or_valid != 0))
    {
      return basic_spell(player, card, EVENT_CAN_CAST);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance = get_card_instance(player, card);
        instance->targets[0].player = player;
        instance->targets[1].card = -1;
        instance->targets[0].card = -1;
        instance->targets[1].player = 1 - player;
        instance->number_of_targets = '\x02';
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    if (event == EVENT_RESOLVE_SPELL)
    {
      life_or_valid = validate_target(player, card, &td, 0);
      if ((life_or_valid != 0) &&
          (life_or_valid = validate_target(player, card, &td, 1), life_or_valid != 0))
      {
        if (life_total_can_change(0) == 0)
        {
          return 0;
        }
        if (life_total_can_change(1) == 0)
        {
          return 0;
        }
        life_or_valid = life[player];
        opponent_life = life[1 - player];
        if (life_or_valid < opponent_life)
        {
          difference = opponent_life - life_or_valid;
        }
        else
        {
          difference = 0;
          if (opponent_life < life_or_valid)
          {
            difference = life_or_valid - opponent_life;
          }
        }
        exchange_life_totals(player, 1 - player);
        default_token_definition(player, card, CARD_ID_HORROR, &token);
        token.color_forced = '\x01';
        token.action = TOKEN_ACTION_CONVERT_INTO_ARTIFACT;
        token.pow = difference;
        token.tou = difference;
        generate_token(&token);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25ae2d0; original cards/commander_legends.c:4628. */
int card_promise_of_tomorrow(int player, int card, event_t event)
{
  /* Promise of Tomorrow	|2|W
   * Enchantment
   * Whenever a creature you control dies, exile it.
   * At the beginning of each end step, if you control no creatures, sacrifice this enchantment and return all cards exiled with it to the battlefield under your control. */

  unsigned int packed_exiled_card;
  int result;
  unsigned int *exiled_entry;
  card_instance_t *instance;
  int iid;
  int leg;
  int idx;

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = in_play(affected_card_controller, affected_card);
    if (instance != NULL)
    {
      result = is_what(affected_card_controller, affected_card, 2);
      if ((result != 0) && (affected_card_controller == player))
      {
        result = is_token(player, affected_card);
        if (result == 0)
        {
          result = check_for_death_trigger_removal(affected_card_controller, affected_card);
          if (result == 0)
          {

            instance = get_card_instance(affected_card_controller, affected_card);
            if ((instance->kill_code & 0xfb) != 0)
            {

              result = get_owner(affected_card_controller, affected_card);
              iid = get_original_internal_card_id(affected_card_controller, affected_card);
              exiledby_remember(player, card, result, iid, NULL, NULL);
              instance->kill_code = '\x04';
            }
          }
        }
      }
    }
  }
  result = eot_trigger(player, card, event);
  if (result != 0)
  {
    result = count_subtype(player, 2, -1);
    if (result < 1)
    {

      state_untargettable(player, card, 1);
      leg = 0;
      idx = 0;
      while (true)
      {
        exiled_entry = (unsigned int *)exiledby_find_any(player, card, &leg, &idx);
        if (exiled_entry == NULL)
          break;
        packed_exiled_card = *exiled_entry;
        *exiled_entry = 0xffffffff;
        result = add_card_to_hand(player, packed_exiled_card & 0x7fffffff);
        if (player != packed_exiled_card >> 0x1f)
        {
          if (player == 1)
          {
            remove_state(1, result, 0x1000);
          }
          else
          {

            add_state(player, result, 0x1000);
          }
        }
        put_into_play(player, result);
      }
      kill_card(player, card, KILL_SACRIFICE);
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25ae510; original cards/commander_legends.c:4680. */
int card_radiant_serra_archangel(int player, int card, event_t event)
{
  /* Radiant, Serra Archangel	|6|W
   * Legendary Creature - Angel 6/4
   * Flying
   * Tap another untapped creature you control with flying: Radiant gains protection from the color of your choice until end of turn.
   * Partner (You can have two commanders if both have partner.) */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.illegal_state = TARGET_STATE_TAPPED;
    td.required_abilities = 0x20;
    td.illegal_abilities = 0;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x40004, 0, 0, 0, 0, 0,
                                         0, 0, &td, NULL);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0x40006, 0, 0, 0, 0, 0, 0, 0, &td,
                                "Select another untapped creature with flying to tap.");
      if (spell_fizzled == 1)
      {
        return 0;
      }
      tap_card(instance->targets[0].player, instance->targets[0].card);
      instance->number_of_targets = '\0';
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
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.illegal_state = TARGET_STATE_TAPPED;
    td.required_abilities = 0x20;
    td.illegal_abilities = 0;
    instance = get_card_instance(player, card);
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  result = select_a_protection(player);
  pump_ability_until_eot_no_repeat(player, card, instance->parent_controller, instance->parent_card,
                                   result, 0);
  return 0;
}

/* ManalinkEh 2022: 0x25ae740; original cards/commander_legends.c:4719. */
int card_rakshasa_debaser(int player, int card, event_t event)
{
  /* Rakshasa Debaser	|4|B|B
   * Creature - Demon 6/6
   * Whenever this creature attacks, put target creature card from defending player's graveyard onto the battlefield under your control.
   * Encore |6|B|B (|6|B|B, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_RAKSHASA_DEBASER = 22187
  };

  int result;
  int opponent;
  test_definition_t test;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    opponent = 1 - player;
    result = count_graveyard_by_type(opponent, 2);
    if (result != 0)
    {
      result = graveyard_has_shroud(opponent);
      if (result == 0)
      {
        new_default_test_definition(&test, 2, "Select a creature card.");
        new_global_tutor(player, opponent, 2, 4, 1, 2, &test);
      }
    }
  }
  result = encore(player, card, event, CARD_ID_RAKSHASA_DEBASER, 6, 2, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x25ae850; original cards/commander_legends.c:4738. */
int card_rebbec_architect_of_ascension(int player, int card, event_t event)
{
  /* Rebbec, Architect of Ascension	|3|W
   * Legendary Creature - Human Artificer 3/4
   * Artifacts you control have protection from each mana value among artifacts you control.
   * Partner (You can have two commanders if both have partner.) */

  int attacking_mana_value_or_type;
  card_instance_t *instance;
  int artifact_mana_value_or_type;
  int card_00;

  check_legend_rule(player, card, event);
  if (event == EVENT_ABILITIES)
  {
    if ((affected_card_controller == player) &&
        (attacking_mana_value_or_type = is_what(player, affected_card, 0x40),
         attacking_mana_value_or_type != 0))
    {
      special_abilities2(affected_card_controller, affected_card, EVENT_ABILITIES,
                         SP_KEYWORD2_GRANTED_PROT_MISC);
    }
  }
  else if (((event == EVENT_PROT_MISC) && (affected_card_controller == player)) &&
           (attacking_mana_value_or_type = is_what(player, affected_card, 0x40),
            attacking_mana_value_or_type != 0))
  {
    if (((unsigned int)event_result >> 24 & 0xff) == '\x06')
    {
      event_result = event_result | 1;
    }
    else
    {

      attacking_mana_value_or_type = get_cmc(attacking_card_controller, attacking_card);
      if (0 < active_cards_count[player])
      {
        card_00 = 0;
        do
        {
          instance = in_play(player, card_00);
          if (((instance != NULL) && (artifact_mana_value_or_type = is_what(player, card_00, 0x40),
                                      artifact_mana_value_or_type != 0)) &&
              (artifact_mana_value_or_type = get_cmc(player, card_00),
               artifact_mana_value_or_type == attacking_mana_value_or_type))
          {
            event_result = event_result | 1;
            return 0;
          }
          card_00 = card_00 + 1;
        } while (card_00 < active_cards_count[player]);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25ae9b0; original cards/commander_legends.c:4774. */
int card_reshape_the_earth(int player, int card, event_t event)
{
  /* Reshape the Earth	|6|G|G|G
   * Sorcery
   * Search your library for up to ten land cards, put them onto the battlefield tapped, then shuffle. */

  int result;
  test_definition_t test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    new_default_test_definition(&test, 1, "Select a land card.");
    test.qty = 10;
    new_global_tutor(player, player, 1, 5, 0, 1, &test);
    kill_card(player, card, KILL_BURY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25aea60; original cards/commander_legends.c:4791. */
int card_reyav_master_smith(int player, int card, event_t event)
{
  /* Reyav, Master Smith	|R|W
   * Legendary Creature - Dwarf Artificer 2/2
   * Whenever a creature you control that's enchanted or equipped attacks, that creature gains double strike until end of turn. */

  check_legend_rule(player, card, event);
  store_attackers(player, card, event, DAT_TRACK, player, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->info_slot > 0)
    {
      const uint8_t *attackers = (const uint8_t *)&instance->targets + 2 * sizeof(target_t);
      for (int index = instance->info_slot - 1; index >= 0; --index)
        if (is_enchanted(current_turn, attackers[index]) || is_equipped(current_turn, attackers[index]))
          pump_ability_until_eot_no_repeat(player, card, current_turn, attackers[index],
                                            KEYWORD_DOUBLE_STRIKE, 0);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25aeb80; original cards/commander_legends.c:4811. */
int card_rock_token(int player, int card, event_t event)
{
  enum
  {
    CHOICE_EQUIP = 1,
    CHOICE_SAC_DAMAGE = 2
  };

  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (is_equipping(player, card))
    {
      int equipped_player = instance->damage_target_player;
      int equipped_card = instance->damage_target_card;
      td.illegal_abilities = get_protections_from(equipped_player, equipped_card);
      if (granted_generic_activated_ability(player, card, equipped_player, equipped_card, event, 5,
                                            1, 0, 0, 0, 0, 0, 0, &td, NULL) &&
          can_sacrifice_this_as_cost(player, card))
        return 1;
    }
    return basic_equipment(player, card, event, 1);
  }
  if (event == EVENT_ACTIVATE)
  {
    int can_equip = can_activate_basic_equipment(player, card, event, 1);
    unsigned int can_damage = 0;
    if (is_equipping(player, card))
    {
      int equipped_player = instance->damage_target_player;
      int equipped_card = instance->damage_target_card;
      td.illegal_abilities = get_protections_from(equipped_player, equipped_card);
      if (granted_generic_activated_ability(player, card, equipped_player, equipped_card,
                                            EVENT_CAN_ACTIVATE, 5, 1, 0, 0, 0, 0, 0, 0, &td, NULL))
        can_damage = can_sacrifice_this_as_cost(player, card) != 0;
    }
    int choice = dialog_impl(player, card, event, DLG_RANDOM, "Equip", can_equip, 5, "Sac & damage",
                             can_damage, 10, 0, 0, 0, 0);
    if (choice == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (choice == CHOICE_EQUIP)
      return basic_equipment(player, card, event, 1);
    if (choice != CHOICE_SAC_DAMAGE || !is_equipping(player, card))
      return 0;
    int equipped_player = instance->damage_target_player;
    int equipped_card = instance->damage_target_card;
    td.illegal_abilities = get_protections_from(equipped_player, equipped_card);
    instance->targets[1].player = get_protections_from(equipped_player, equipped_card);
    if (granted_generic_activated_ability(player, card, equipped_player, equipped_card, event, 5, 1,
                                          0, 0, 0, 0, 0, 0, &td, "TARGET_ANY"))
      kill_card(player, card, KILL_SACRIFICE);
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->info_slot == CHOICE_EQUIP)
      return basic_equipment(player, card, event, 1);
    if (instance->info_slot == CHOICE_SAC_DAMAGE)
    {
      td.illegal_abilities = instance->targets[1].player;
      if (valid_target(&td))
        damage_target0(player, card, 2);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25af040; original cards/commander_legends.c:4899. */
int card_rograkh_son_of_rohgahh(int player, int card, event_t event)
{
  /* Rograkh, Son of Rohgahh	|0
   * Legendary Creature - Kobold Warrior 0/1
   * First strike, menace, trample
   * Partner (You can have two commanders if both have partner.) */

  check_legend_rule(player, card, event);
  menace(player, card, event);
  return 0;
}

/* ManalinkEh 2022: 0x25af080; original cards/commander_legends.c:4911. */
int card_rootweaver_druid(int player, int card, event_t event)
{
  /* Rootweaver Druid	|2|G
   * Creature - Elf Druid 2/1
   * When this creature enters, each opponent may search their library for up to three basic land cards. They each put one of those cards onto the battlefield tapped under your control and the rest onto the battlefield tapped under their control. Then each player who searched their library this way shuffles. */

  int i;
  int opponent_or_duh;
  card_instance_t *instance;
  int iid_or_marked;
  int selected_or_added;
  int found_count;
  int array[3];
  test_definition_t test;

  opponent_or_duh = duh_mode(player);
  etb_ability(player, card, event, (opponent_or_duh != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {

    new_default_test_definition(&test, 1, "Select a land card.");
    opponent_or_duh = 1 - player;
    test.qty = 3;
    new_global_tutor(opponent_or_duh, opponent_or_duh, 1, 0xd, 0, 1, &test);
    if (active_cards_count[opponent_or_duh] < 1)
    {
      found_count = 0;
    }
    else
    {
      i = 0;
      found_count = 0;
      do
      {
        instance = in_hand(opponent_or_duh, i);
        if ((instance != NULL) &&
            (iid_or_marked = check_state(opponent_or_duh, 1, 0x200000), iid_or_marked != 0))
        {
          array[found_count] = i;
          found_count = found_count + 1;
        }
        i = i + 1;
      } while (i < active_cards_count[opponent_or_duh]);
    }
    selected_or_added = select_card_from_zone(opponent_or_duh, opponent_or_duh, array, found_count,
                                              1, 3, -1, &test);
    if (-1 < selected_or_added)
    {

      iid_or_marked = get_original_internal_card_id(opponent_or_duh, selected_or_added);
      obliterate_card(opponent_or_duh, selected_or_added);
      selected_or_added = add_card_to_hand(player, iid_or_marked);
      if (player == 0)
      {
        add_state(0, selected_or_added, 0x1000);
      }
      else
      {
        remove_state(player, selected_or_added, 0x1000);
      }
      add_state(player, selected_or_added, 0x10);
      put_into_play(player, selected_or_added);
    }
    i = active_cards_count[opponent_or_duh] + -1;
    if (-1 < i)
    {
      do
      {
        instance = in_hand(opponent_or_duh, i);
        if ((instance != NULL) &&
            (iid_or_marked = check_state(opponent_or_duh, 1, 0x200000), iid_or_marked != 0))
        {
          add_state(opponent_or_duh, i, 0x10);
          remove_state(opponent_or_duh, i, 0x200000);
          put_into_play(opponent_or_duh, i);
        }
        i = i + -1;
      } while (i != -1);
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25a2700; original cards/commander_legends.c:4960. */
static int sakashima_effect(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;

  result = effect_follows_control_of_attachment(player, card, event);
  if ((result == 0) && (event == EVENT_STATIC_EFFECTS))
  {

    instance = in_play(player, card);

    if ((instance != NULL) &&
        ((-1 < instance->damage_target_player && (-1 < instance->damage_target_card))))
    {
      disable_legend_rule(player, card, EVENT_STATIC_EFFECTS, (int)instance->damage_target_player);
    }
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25af300; original cards/commander_legends.c:4976. */
int card_sakashima_of_a_thousand_faces(int player, int card, event_t event)
{
  /* Sakashima of a Thousand Faces	|3|U
   * Legendary Creature - Human Rogue 3/1
   * You may have Sakashima enter as a copy of another creature you control, except it has Sakashima's other abilities.
   * The "legend rule" doesn't apply to permanents you control.
   * Partner (You can have two commanders if both have partner.) */

  int c;
  card_instance_t *instance;
  int legacy_card;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    disable_legend_rule(player, card, EVENT_STATIC_EFFECTS, player);
    check_legend_rule(player, card, EVENT_STATIC_EFFECTS);
  }
  else
  {
    check_legend_rule(player, card, event);
    if (((event == EVENT_CHANGE_TYPE) && (card == affected_card)) &&
        (player == affected_card_controller))
    {

      bool found = false;
      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          instance = in_play(player, c);

          if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
               (instance = get_card_instance(player, c),
                instance->info_slot_fn == sakashima_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            found = true;
            break;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }

      if (!found)
      {
        legacy_card = create_targetted_legacy_effect(player, card, sakashima_effect, player, card);
        instance = get_card_instance(player, legacy_card);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, legacy_card, 0x10000);
      }
    }
  }
  base_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td.allowed_controller = player;
  enters_the_battlefield_as_copy_of_any(player, card, event, &td, "TARGET_CREATURE_YOU_CONTROL");
  return 0;
}
#endif

/* ManalinkEh 2022: 0x259ff70; original cards/commander_legends.c:5022. */
static char *is_entered_the_battlefield_this_turn(int who_chooses, int player, int card)
{
  int result;
  char *error;

  result = check_special_flags(player, card, 0x1000);
  error = NULL;
  if (result == 0)
  {
    error = "must have entered the battlefield this turn.";
  }
  return error;
}

/* ManalinkEh 2022: 0x25af4b0; original cards/commander_legends.c:5029. */
int card_sakashimas_protege(int player, int card, event_t event)
{
  /* Sakashima's Protege	|4|U|U
   * Creature - Shapeshifter 3/1
   * Flash
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.)
   * You may have this creature enter as a copy of any permanent that entered this turn. */

  cascade(player, card, event);
  int target_player = -1;
  int target_card = -1;
  if (event == EVENT_RESOLVE_SPELL)
  {
    target_definition_t td;
    base_target_definition(player, card, &td, 0x47);
    td.preferred_controller = player;
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (intptr_t)is_entered_the_battlefield_this_turn;
    card_instance_t *instance = get_card_instance(player, card);
    instance->state |= STATE_OUBLIETTED;
    int selected = new_pick_target(
        &td, "Select any permanent that entered the battlefield this turn.", 0, 0x80);
    instance->state &= 0xff7fffff;
    if (!selected)
      spell_fizzled = 0;
    else
    {
      target_card = instance->targets[0].card;
      instance->number_of_targets = 0;
      target_player = instance->targets[0].player;
      if (target_card == card && target_player == player)
      {
        target_card = -1;
        target_player = -1;
      }
    }
  }
  enters_the_battlefield_as_copy_of(player, card, event, target_player, target_card);
  return flash(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x259ff10; original cards/commander_legends.c:5067. */
static int test_exclude_this_creature(int iid, intptr_t val, int player, int card)
{
  if (card != (val & 0xffffU))
  {
    return 1;
  }
  return (unsigned int)((unsigned int)val >> 0x10 != player);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25af5d0; original cards/commander_legends.c:5070. */
int card_sakashimas_will(int player, int card, event_t event)
{
  /* Sakashima's Will	|3|U
   * Sorcery
   * Choose one. If you control a commander as you cast this spell, you may choose both instead.
   * * Target opponent chooses a creature they control. You gain control of it.
   * * Choose a creature you control. Each other creature you control becomes a copy of that creature until end of turn. */

  unsigned char resolved_mode;
  card_instance_t *instance;
  int opponent_valid;
  unsigned int both_available_or_choice;
  int opponent_or_result;
  int commander_present;
  target_definition_t td;
  target_definition_t td2;
  target_definition_t td3;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    default_target_definition(player, card, &td2, 2);
    td2.who_chooses = 1 - player;
    td2.illegal_abilities = 0;
    td2.allow_cancel = 0;
    td2.allowed_controller = td2.who_chooses;
    td2.preferred_controller = td2.who_chooses;
    default_target_definition(player, card, &td3, 2);
    td3.preferred_controller = player;
    td3.allowed_controller = player;
    td3.illegal_abilities = 0;
    td3.allow_cancel = 0;
    get_card_instance(player, card);
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.zone = TARGET_ZONE_PLAYERS;
      default_target_definition(player, card, &td2, 2);
      opponent_or_result = 1 - player;
      td2.illegal_abilities = 0;
      td2.allow_cancel = 0;
      td2.who_chooses = opponent_or_result;
      td2.allowed_controller = opponent_or_result;
      td2.preferred_controller = opponent_or_result;
      default_target_definition(player, card, &td3, 2);
      td3.preferred_controller = player;
      td3.allowed_controller = player;
      td3.illegal_abilities = 0;
      td3.allow_cancel = 0;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {

        instance->number_of_targets = '\0';
        instance->info_slot = 0;
        opponent_valid = would_validate_arbitrary_target(&td, opponent_or_result, -1);
        both_available_or_choice = 0;
        if (opponent_valid != 0)
        {
          commander_present = liutenant(player);
          both_available_or_choice = (unsigned int)(commander_present != 0);
        }
        both_available_or_choice =
            dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_MODE, "Steal a creature",
                        opponent_valid, 0x14, "Mass copy", 1, 10, "Do both",
                        both_available_or_choice, 0x1e, 0, 0, 0, 0);
        if (both_available_or_choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if ((both_available_or_choice & 1) != 0)
        {
          instance->targets[0].player = opponent_or_result;
          instance->targets[0].card = -1;
          instance->number_of_targets = '\x01';
        }
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    default_target_definition(player, card, &td2, 2);
    td2.who_chooses = 1 - player;
    td2.illegal_abilities = 0;
    td2.allow_cancel = 0;
    td2.allowed_controller = td2.who_chooses;
    td2.preferred_controller = td2.who_chooses;
    default_target_definition(player, card, &td3, 2);
    td3.preferred_controller = player;
    td3.allowed_controller = player;
    td3.illegal_abilities = 0;
    td3.allow_cancel = 0;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      resolved_mode = instance->mode;
      if ((((resolved_mode & 1) != 0) &&
           (opponent_or_result = valid_target(&td), opponent_or_result != 0)) &&
          (opponent_or_result = can_target(&td2), opponent_or_result != 0))
      {
        new_pick_target(&td2, "Select a creature you control.", 1, 0x80);
        gain_control(player, card, instance->targets[1].player, instance->targets[1].card);
      }
      if (((resolved_mode & 2) != 0) &&
          (opponent_or_result = can_target(&td3), opponent_or_result != 0))
      {
        new_pick_target(&td3, "Select a creature you control.", 2, 0x80);
        new_default_test_definition(&test, 2, "");
        test.special_selection_function = test_exclude_this_creature;
        test.value_for_special_selection_function =
            (unsigned int)(unsigned short)instance->targets[0].card | instance->targets[0].player
                                                                          << 0x10;
        shapeshift_all(player, card, player, &test, instance->targets[2].player,
                       instance->targets[2].card, 1);
      }
      kill_card(player, card, KILL_BURY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_PLAYERS;
    default_target_definition(player, card, &td2, 2);
    td2.who_chooses = 1 - player;
    td2.illegal_abilities = 0;
    td2.allow_cancel = 0;
    td2.allowed_controller = td2.who_chooses;
    td2.preferred_controller = td2.who_chooses;
    default_target_definition(player, card, &td3, 2);
    td3.preferred_controller = player;
    td3.allowed_controller = player;
    td3.illegal_abilities = 0;
    td3.allow_cancel = 0;
    get_card_instance(player, card);
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x25afc40; original cards/commander_legends.c:5150. */
int card_salamander_warrior(int player, int card, event_t event)
{
  /* Salamander Warrior	""
   * Token Creature - Salamander Warrior 4/3 */

  int result;
  result = generic_token(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25afc50; original cards/commander_legends.c:5154. */
int card_sengir_the_dark_baron(int player, int card, event_t event)
{
  /* Sengir, the Dark Baron	|4|B|B
   * Legendary Creature - Vampire Noble 4/4
   * Flying
   * Whenever another creature dies, put two +1/+1 counters on Sengir.
   * Whenever another player loses the game, you gain life equal to that player's life total as the turn began.
   * Partner (You can have two commanders if both have partner.) */

  check_legend_rule(player, card, event);
  when_another_permanent_dies_add_counters_on_this(player, card, event, 2, 0, 2);
  return 0;
}

/* ManalinkEh 2022: 0x25afcb0; original cards/commander_legends.c:5172. */
int card_seraphic_greatsword(int player, int card, event_t event)
{
  /* Seraphic Greatsword	|1|W
   * Artifact - Equipment
   * Equipped creature gets +2/+2.
   * Whenever equipped creature attacks the player with the most life or tied for most life, create a 4/4 white Angel creature token with flying that's tapped and attacking that player.
   * Equip |4 */

  enum
  {
    TOKEN_ACTION_SKIP_ATTACK_CHECK = 2097152
  };

  card_instance_t *instance;
  int result;
  token_generation_t token;

  if (((unsigned int)player < 3) && ((unsigned int)card < 0x97))
  {
    instance = is_equipping(player, card);
    if (instance != NULL)
    {
      store_attackers(player, card, event, DAT_ATTACKS_PLAYER | DAT_RESOLVE_TRIGGER_MANDATORY,
                      (int)instance->damage_target_player, instance->damage_target_card, NULL);
      dat_ability(player, card, event, DAT_DONT_REPORT_VALUE, 1);
      if ((event == EVENT_DAT_ABILITY) && (life[current_turn] <= life[1 - current_turn]))
      {
        default_token_definition(player, card, CARD_ID_ANGEL, &token);
        token.action = TOKEN_ACTION_SKIP_ATTACK_CHECK | TOKEN_ACTION_ATTACKING;
        generate_token(&token);
      }
    }
    result = vanilla_equipment(player, card, event, 4, 2, 2, 0, 0);
    return result;
  }
  return 0;
}

/* ManalinkEh 2022: 0x25afde0; original cards/commander_legends.c:5199. */
int card_siani_eye_of_the_storm(int player, int card, event_t event)
{
  /* Siani, Eye of the Storm	|3|U
   * Legendary Creature - Djinn Monk 3/2
   * Flying
   * Whenever Siani attacks, scry X, where X is the number of attacking creatures with flying.
   * Partner (You can have two commanders if both have partner.) */

  check_legend_rule(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
    return 0;
  int flying_attackers = 0;
  for (int candidate = 0; candidate < active_cards_count[player]; ++candidate)
    if (in_play(player, candidate) && is_attacking(player, candidate))
      flying_attackers += check_for_ability(player, candidate, 0x20) != 0;
  scry(player, flying_attackers);
  return 0;
}

/* ManalinkEh 2022: 0x25a03c0; original cards/commander_legends.c:5222. */
static int is_not_commander(int iid, intptr_t val, int player, int card)
{
  int result;

  result = check_special_flags2(player, card, 0x1000000);
  return (unsigned int)(result == 0);
}

/* ManalinkEh 2022: 0x25afec0; original cards/commander_legends.c:5225. */
int card_slash_the_ranks(int player, int card, event_t event)
{
  /* Slash the Ranks	|3|W|W
   * Sorcery
   * Destroy all creatures and planeswalkers except for commanders. */

  int result;
  test_definition_t this_test;

  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&this_test, 0x102);
    this_test.special_selection_function = is_not_commander;
    new_manipulate_all(player, card, 2, &this_test, ACT_KILL_DESTROY);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25a02b0; original cards/commander_legends.c:5242. */
static void slurrk_trigger(int player, int card)
{
  for (int c = 0; c < active_cards_count[player]; ++c)
    if (in_play(player, c) && is_what(player, c, TYPE_CREATURE) &&
        count_counters(player, c, COUNTER_P1_P1) > 0)
      add_counters(player, card, player, c, COUNTER_P1_P1, 1);
}

/* ManalinkEh 2022: 0x25a16e0; original cards/commander_legends.c:5250. */
static int slurrk_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (event == EVENT_STATIC_EFFECTS && card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player >= 0 &&
        get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_SLURRK_ALL_INGESTING)
      kill_card(player, card, KILL_EXILE);
  }
  instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0 &&
      effect_follows_control_of_attachment(player, card, event))
    return 0;
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = get_card_instance(player, card);
    card_instance_t *dying = in_play(affected_card_controller, affected_card);
    if (dying && get_card_instance(affected_card_controller, affected_card)->kill_code != 0)
    {
      if (instance->damage_target_player >= 0 && instance->damage_target_card == affected_card &&
          instance->damage_target_player == affected_card_controller)
      {
        instance->damage_target_card = -1;
        instance->damage_target_player = -1;
        remove_status(player, card, 0x10000);
        if (instance->targets[0xb].player < 0)
          instance->targets[0xb].player = 0;
        ((uint8_t *)&instance->targets[0xb].player)[2] = 1;
      }
      if (count_counters(affected_card_controller, affected_card, COUNTER_P1_P1))
        count_for_gfp_ability(player, card, 0x77, 2, 2, NULL);
    }
  }
  unsigned int deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (!check_status(player, card, 0x10000))
  {
    for (deaths &= 0xff; deaths != 0;)
    {
      --deaths;
      slurrk_trigger(player, card);
    }
    kill_card(player, card, KILL_EXILE);
  }
  if (event == EVENT_CLEANUP && !check_status(player, card, 0x10000))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x25aff60; original cards/commander_legends.c:5305. */
int card_slurrk_all_ingesting(int player, int card, event_t event)
{
  /* Slurrk, All-Ingesting	|5|G
   * Legendary Creature - Ooze 0/0
   * Slurrk enters with five +1/+1 counters on it.
   * Whenever Slurrk or another creature you control dies, if it had a +1/+1 counter on it, put a +1/+1 counter on each creature you control that has a +1/+1 counter on it.
   * Partner (You can have two commanders if both have partner.) */

  int c;
  card_instance_t *instance;
  int counters_or_legacy;
  unsigned int deaths;

  check_legend_rule(player, card, event);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      bool found = false;
      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          instance = in_play(player, c);

          if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
               (instance = get_card_instance(player, c),
                instance->info_slot_fn == slurrk_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            found = true;
            break;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }

      if (!found)
      {
        counters_or_legacy =
            create_targetted_legacy_effect(player, card, slurrk_effect, player, card);
        instance = get_card_instance(player, counters_or_legacy);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, counters_or_legacy, 0x10000);
      }
    }
  }
  else if ((event == EVENT_GRAVEYARD_FROM_PLAY) &&
           (counters_or_legacy =
                count_counters(affected_card_controller, affected_card, COUNTER_P1_P1),
            counters_or_legacy != 0))
  {
    count_for_gfp_ability(player, card, 0x77, player, 2, NULL);
  }
  instance = get_card_instance(player, card);

  if ((instance->kill_code == '\0') &&
      (deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY), deaths != 0))
  {

    for (deaths = deaths & 0xff; deaths != 0; deaths = deaths - 1)
    {
      slurrk_trigger(player, card);
    }
  }
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 5);
  return 0;
}

/* ManalinkEh 2022: 0x25b0170; original cards/commander_legends.c:5360. */
int card_soul_of_eternity(int player, int card, event_t event)
{
  /* Soul of Eternity	|5|W|W
   * Creature - Avatar * / *
   * ~'s power and toughness are each equal to your life total.
   * Encore |7|W|W (|7|W|W, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_SOUL_OF_ETERNITY = 22204
  };

  int result;

  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    set_base_pt(life[player]);
  }
  result = encore(player, card, event, CARD_ID_SOUL_OF_ETERNITY, 7, 0, 0, 0, 0, 2);
  return result;
}

/* ManalinkEh 2022: 0x25b0200; original cards/commander_legends.c:5373. */
int card_soulfire_eruption(int player, int card, event_t event)
{
  /* Soulfire Eruption	|6|R|R|R
   * Sorcery
   * Choose any number of target creatures, planeswalkers, and/or players. For each of them, exile the top card of your library, then ~ deals damage equal to that card's mana value to that permanent or player. You may play the exiled cards until the end of your next turn. */

  int i;
  card_instance_t *instance;
  int valid_or_mana_value;
  target_definition_t *target;
  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  td.allow_cancel = 3;
  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    if (instance->number_of_targets != '\0')
    {
      i = 0;
      do
      {
        target = &td;
        valid_or_mana_value = validate_target(player, card, target, i);
        if ((valid_or_mana_value != 0) && (*deck_ptr[player] != -1))
        {
          valid_or_mana_value = get_cmc_by_internal_id(*deck_ptr[player]);
          damage_creature(instance->targets[i].player, instance->targets[i].card,
                          valid_or_mana_value, player, card);
          exile_top_x_card_you_may_play_them(player, card, 1, NULL, player,
                                             MPCFE_UNTIL_END_OF_YOUR_NEXT_TURN);
        }
        i = i + 1;
      } while (i < (int)(unsigned int)instance->number_of_targets);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0xa1, &td,
                       "Select target creatures, planeswalkers, and/or players.", 10, NULL);
}

/* ManalinkEh 2022: 0x25b03e0; original cards/commander_legends.c:5419. */
int card_staunch_throneguard(int player, int card, event_t event)
{
  /* Staunch Throneguard	|5
   * Artifact Creature - Construct 2/5
   * Vigilance
   * When this creature enters, you become the monarch. */

  vigilance(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  you_become_the_monarch(player);
  return 0;
}

/* ManalinkEh 2022: 0x25a03f0; original cards/commander_legends.c:5437. */
static char *me_or_commander(int who_chooses, int player, int card, int targeting_player,
                             int targeting_card)
{
  int result;
  result = check_special_flags2(player, card, 0x1000000);
  if ((result == 0) && ((player != targeting_player || (card != targeting_card))))
  {
    return "must be Stumpsquall Hydra or a Commander.";
  }
  return NULL;
}

/* ManalinkEh 2022: 0x25b0450; original cards/commander_legends.c:5446. */
int card_stumpsquall_hydra(int player, int card, event_t event)
{
  /* Stumpsquall Hydra	|X|G|G|G
   * Creature - Hydra 1/1
   * When this creature enters, distribute X +1/+1 counters among it and any number of commanders. */

  card_instance_t *instance;
  int total_counters;
  int assigned;
  int next_counter;
  target_definition_t td;
  char buffer[100];

  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      if (is_token(player, card) == 0)
      {
        generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 0, NULL);
      }
    }
    etb_ability(player, card, EVENT_CAST_SPELL, RESOLVE_TRIGGER_MANDATORY);
    return 0;
  }
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    instance = get_card_instance(player, card);
    total_counters = instance->info_slot;
    if (total_counters != 0)
    {

      default_target_definition(player, card, &td, 0x47);
      td.preferred_controller = player;
      td.illegal_abilities = 0;
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      td.extra = (intptr_t)me_or_commander;
      td.allow_cancel = 2;
      if (0 < total_counters)
      {
        assigned = 0;
        do
        {
          next_counter = assigned + 1;
          scnprintf(buffer, 100, "Select Stumpsquall Hydra or a Commaner (counter %d of %d)",
                    next_counter, total_counters);
          if (!can_target(&td) || !new_pick_target(&td, buffer, 0, 0x80))
          {
            if (assigned < total_counters)
              add_counters(player, card, player, card, COUNTER_P1_P1, total_counters - assigned);
            break;
          }
          add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                       COUNTER_P1_P1, 1);
          assigned = next_counter;
        } while (next_counter != total_counters);
      }
      instance->number_of_targets = '\0';
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25b06b0; original cards/commander_legends.c:5495. */
int card_sweet_gum_recluse(int player, int card, event_t event)
{
  /* Sweet-Gum Recluse	|4|G|G
   * Creature - Spider 0/3
   * Flash
   * Cascade
   * Reach
   * When this creature enters, put three +1/+1 counters on each of any number of target creatures that entered this turn. */

  cascade(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.preferred_controller = player;
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (intptr_t)is_entered_the_battlefield_this_turn;
    card_instance_t *instance = get_card_instance(player, card);

    while (instance->number_of_targets < 10 && can_target(&td) &&
           new_pick_target(&td, "Select a creature that entered the battlefield this turn.", -1, 0x80))
      state_untargettable(instance->targets[instance->number_of_targets - 1].player,
                          instance->targets[instance->number_of_targets - 1].card, 1);

    for (int i = 0; i < instance->number_of_targets; ++i)
    {
      state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
      add_counters(player, card, instance->targets[i].player, instance->targets[i].card,
                   COUNTER_P1_P1, 3);
    }
    instance->number_of_targets = 0;
  }
  return flash(player, card, event);
}

/* ManalinkEh 2022: 0x25b0840; original cards/commander_legends.c:5533. */
int card_szats_will(int player, int card, event_t event)
{
  /* Szat's Will	|4|B
   * Instant
   * Choose one. If you control a commander as you cast this spell, you may choose both instead.
   * * Each opponent sacrifices a creature they control with the greatest power.
   * * Exile all opponents' graveyards, then create X 0/1 black Thrull creature tokens, where X is the greatest power among creature cards exiled this way. */

  int *grave_end;
  int result;
  unsigned int selected_modes;
  int *grave_cursor;
  short highest_power;
  token_generation_t token;

  if (event == EVENT_RESOLVE_SPELL)
  {

    result = liutenant(player);
    selected_modes = dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_RANDOM, DLG_NO_CANCEL,
                                 DLG_NO_STORAGE, "Sacrifice", 1, 0x14, "Exile graveyards", 1, 10,
                                 "Do both", result, 0x14, 0, 0, 0, 0);
    if ((selected_modes & 1) != 0)
    {
      player_sacrifices_the_creature_with_highest_power(player, card, 1 - player);
    }
    if ((selected_modes & 2) != 0)
    {

      highest_power = 0;
      grave_cursor = graveyard_ptr_mutable[1 - player];
      grave_end = grave_cursor + 500;
      do
      {
        if (*grave_cursor == -1)
          break;

        if (highest_power < cards_data[*grave_cursor].power)
        {
          highest_power = cards_data[*grave_cursor].power;
        }
        grave_cursor = grave_cursor + 1;
      } while (grave_cursor != grave_end);
      rfg_whole_graveyard(1 - player);
      default_token_definition(player, card, CARD_ID_THRULL, &token);
      token.qty = (unsigned char)highest_power;
      token.pow = 0;
      token.tou = 1;
      token.color_forced = '\x02';
      generate_token(&token);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x25b09e0; original cards/commander_legends.c:5572. */
int card_tevesh_szat_doom_of_fools(int player, int card, event_t event)
{
  /* Tevesh Szat, Doom of Fools	|4|B
   * Legendary Planeswalker - Szat (4)
   * +2: Create two 0/1 black Thrull creature tokens.
   * +1: You may sacrifice another creature or planeswalker. If you do, draw two cards, then draw another card if the sacrificed permanent was a commander.
   * -10: Gain control of all commanders. Put all commanders from the command zone onto the battlefield under your control.
   * ~ can be your commander.
   * Partner */

  enum
  {
    CHOICE_TOKEN = 1,
    CHOICE_SAC_DRAW = 2,
    CHOICE_STEAL_COMMANDERS = 3
  };

  int p;
  uint8_t resolved_mode;
  int available_or_commander;
  card_instance_t *instance;
  int result;
  unsigned int sacrificed;
  unsigned int player_00;
  int i;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0x42);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.illegal_abilities = 0;
    if (event == EVENT_ACTIVATE)
    {

      instance->number_of_targets = '\0';
      instance->mode = '\0';
      available_or_commander = target_available(player, card, &td);
      if (dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_PLANESWALKER, DLG_MODE,
                      "Create two 0/1 black Thrulls", 1, 10, 2, "Sac & draw", 1,
                      (available_or_commander - hand_count[player]) * 5 + 0x23, 1,
                      "Steal all Commanders", 100, 10, 0xfffffff6, 0, 0, 0, 0) == 0)
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

    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 0x42);
  }
  td.allowed_controller = player;
  td.illegal_abilities = 0;
  td.preferred_controller = td.allowed_controller;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    resolved_mode = instance->mode;
    if (resolved_mode == CHOICE_SAC_DRAW)
    {
      test_definition_t test;

      new_default_test_definition(&test, 0x102,
                                  "Select a creature or a planeswalker to sacrifice.");
      sacrificed = new_sacrifice(player, card, player, SAC_RETURN_CHOICE | SAC_JUST_MARK, &test);
      if (sacrificed != 0)
      {

        player_00 = sacrificed >> 0x10 & 0xff;
        available_or_commander = check_special_flags2(player_00, sacrificed >> 0x18, 0x1000000);
        kill_card(player_00, sacrificed >> 0x18, KILL_SACRIFICE);
        draw_cards(player, (available_or_commander != 0) + 2);
      }
    }
    else if (resolved_mode == CHOICE_STEAL_COMMANDERS)
    {
      int rv[3];

      rv[0] = -1;
      p = 1 - player;
      rv[1] = -1;
      rv[2] = -1;
      if (0 < active_cards_count[p])
      {
        i = 0;
        do
        {
          instance = in_play(p, i);
          if (((instance != NULL) && (result = is_what(p, i, 0x47), result != 0)) &&
              (result = check_special_flags2(p, i, 0x1000000), result != 0))
          {
            gain_control(player, card, p, i);
          }
          i = i + 1;
        } while (i < active_cards_count[p]);
      }
      i = 0;
      do
      {

        result = put_all_commanders_from_command_zone_to(i, 4, rv);
        if ((p == i) && (-1 < result))
        {
          if (-1 < rv[1])
          {
            gain_control(player, card, p, rv[1]);
          }
          if (-1 < rv[2])
          {
            gain_control(player, card, p, rv[2]);
          }
        }
        i = i + 1;
      } while (i != 2);
    }
    else if (resolved_mode == CHOICE_TOKEN)
    {
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_THRULL, &token);
      token.pow = 0;
      token.tou = 1;
      token.color_forced = '\x02';
      token.qty = '\x02';
      generate_token(&token);
    }
  }
  return planeswalker(player, card, event, 4);
}

/* ManalinkEh 2022: 0x25b0e30; original cards/commander_legends.c:5677. */
int card_thalisse_reverent_medium(int player, int card, event_t event)
{
  /* Thalisse, Reverent Medium	|3|W|B
   * Legendary Creature - Human Cleric 3/4
   * At the beginning of each end step, create X 1/1 white Spirit creature tokens with flying, where X is the number of tokens you created this turn. */

  int result;
  token_generation_t token;

  check_legend_rule(player, card, event);
  result = eot_trigger(player, card, event);
  if (result != 0)
  {
    result = get_trap_condition(player, 0x206);
    if (result != 0)
    {
      default_token_definition(player, card, CARD_ID_SPIRIT, &token);
      token.tou = 1;
      token.pow = 1;
      token.color_forced = ' ';
      token.key_plus = 0x20;
      result = get_trap_condition(player, 0x206);
      token.qty = (unsigned char)result;
      generate_token(&token);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25b0ee0; original cards/commander_legends.c:5697. */
int card_the_prismatic_piper(int player, int card, event_t event)
{
  /* The Prismatic Piper	|5
   * Legendary Creature - Shapeshifter 3/3
   * If ~ is your commander, choose a color before the game begins. ~ is the chosen color.
   * Partner (You can have two commanders if both have partner.) */

  card_instance_t *inst;
  unsigned int chosen_color;
  int player_00;
  int card_00;
  card_instance_t *edh_record;
  inst_info_t result;

  check_legend_rule(player, card, event);
  if (event != EVENT_SET_COLOR)
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

  inst = get_card_instance(player, card);
  result = inst_info(inst, 0x56c6);
  chosen_color = ((uint32_t)result.id | ((uint32_t)result.data2 << 16)) >> 0x10;
  if (chosen_color == 0)
  {

    player_00 = get_owner(player, card);
    card_00 = locate_edh_card(player_00);
    if (card_00 == -1)
    {
      set_inst_info(inst, 0x56c6, 0, 0x800);
      chosen_color = 0;
    }
    else
    {
      edh_record = get_card_instance(player_00, card_00);
      chosen_color = edh_record->targets[6].card;
      set_inst_info(inst, 0x56c6, 0, (uint16_t)chosen_color);
    }
  }
  chosen_color &= 0x3e;
  event_result = event_result | chosen_color;
  return 0;
}

/* ManalinkEh 2022: 0x25b0ff0; original cards/commander_legends.c:5732. */
int card_toggo_goblin_weaponsmith(int player, int card, event_t event)
{
  /* Toggo, Goblin Weaponsmith	|2|R
   * Legendary Creature - Goblin Artificer 2/2
   * Landfall - Whenever a land you control enters, create a colorless Equipment artifact token named Rock with "Equipped creature has '|1, |T, Sacrifice Rock: This creature deals 2 damage to any target'" and equip |1.
   * Partner (You can have two commanders if both have partner.) */

  check_legend_rule(player, card, event);
  landfall_mode(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  generate_token_by_id(player, card, CARD_ID_ROCK);
  return 0;
}

/* ManalinkEh 2022: 0x25b1060; original cards/commander_legends.c:5750. */
int card_tormod_the_desecrator(int player, int card, event_t event)
{
  /* Tormod, the Desecrator	|3|B
   * Legendary Creature - Zombie Wizard 4/2
   * Whenever one or more cards leave your graveyard, create a tapped 2/2 black Zombie creature token.
   * Partner (You can have two commanders if both have partner.) */

  token_generation_t token;

  check_legend_rule(player, card, event);
  if ((event == EVENT_REMOVED_FROM_GRAVEYARD) && (affected_card_controller == player))
  {
    default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
    token.action = TOKEN_ACTION_TAPPED;
    generate_token(&token);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25b10d0; original cards/commander_legends.c:5776. */
int card_trench_behemoth(int player, int card, event_t event)
{
  /* Trench Behemoth	|5|U|U
   * Creature - Kraken 7/7
   * Return a land you control to its owner's hand: Untap this creature. It gains hexproof until end of turn.
   * Landfall - Whenever a land you control enters, target creature an opponent controls attacks during its controller's next combat phase if able. */

  card_instance_t *instance;
  int result;
  target_definition_t td2;

  landfall_mode(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td2, 2);
    td2.allowed_controller = 1 - player;
    td2.allow_cancel = 0;
    td2.preferred_controller = td2.allowed_controller;
    instance = get_card_instance(player, card);
    result = can_target(&td2);
    if ((result != 0) &&
        (result = new_pick_target(&td2, "Select target creature controlled by opponent.", 0, 0x80),
         result != 0))
    {
      create_targetted_legacy_effect(player, card, effect_must_attack_on_next_combat,
                                     instance->targets[0].player, instance->targets[0].card);
    }
    instance->number_of_targets = '\0';
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td2, 1);
      td2.preferred_controller = player;
      td2.allowed_controller = player;
      td2.illegal_abilities = 0;
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 0, 0, 0, 0, 0, 0, 0,
                                           &td2, NULL);
        return result;
      }
      if (event == EVENT_ACTIVATE)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 6, 0, 0, 0, 0, 0, 0, 0, &td2,
                                  "Select a land you control.");
        if (spell_fizzled == 1)
        {
          return 0;
        }
        bounce_permanent(instance->targets[0].player, instance->targets[0].card);
        instance->number_of_targets = '\0';
        result = is_target(player, card, 1 - player);
        if (result == 0)
        {
          return 0;
        }
        ai_modifier = ai_modifier + 0x30;
        return 0;
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      {
        return 0;
      }
      default_target_definition(player, card, &td2, 1);
      td2.preferred_controller = player;
      td2.allowed_controller = player;
      td2.illegal_abilities = 0;
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      untap_card(instance->parent_controller, instance->parent_card);
      pump_ability_until_eot_no_repeat(player, card, instance->parent_controller,
                                       instance->parent_card, 0, (SP_KEYWORD_HEXPROOF));
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25b13f0; original cards/commander_legends.c:5829. */
int card_triumphant_reckoning(int player, int card, event_t event)
{
  /* Triumphant Reckoning	|6|W|W|W
   * Sorcery
   * Return all artifact, enchantment, and planeswalker cards from your graveyard to the battlefield. */

  int result;
  test_definition_t test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&test, 0x144);
    new_reanimate_all(player, card, player, &test, REANIMATE_DEFAULT);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25b1480; original cards/commander_legends.c:5846. */
int card_trove_tracker(int player, int card, event_t event)
{
  /* Trove Tracker	|2|U
   * Creature - Human Pirate 2/2
   * When this creature dies, draw a card.
   * Encore |5|U|U (|5|U|U, Exile this card from your graveyard: For each opponent, create a token copy that attacks that opponent this turn if able. They gain haste. Sacrifice them at the beginning of the next end step. Activate only as a sorcery.) */

  enum
  {
    CARD_ID_TROVE_TRACKER = 22221
  };

  int result;

  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    draw_a_card(player);
  }
  result = encore(player, card, event, CARD_ID_TROVE_TRACKER, 5, 0, 2, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x25b1510; original cards/commander_legends.c:5860. */
int card_tuya_bearclaw(int player, int card, event_t event)
{
  /* Tuya Bearclaw	|1|R|G
   * Legendary Creature - Human Warrior 2/2
   * Whenever ~ attacks, it gets +X/+X until end of turn, where X is the greatest power among other creatures you control. */

  int power;
  test_definition_t this_test;

  check_legend_rule(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }

  default_test_definition(&this_test, 2);
  this_test.not_me = 1;
  power = check_battlefield_for_special_card(player, card, player, 0x80, &this_test);
  pump_until_eot(player, card, player, card, power, power);
  return 0;
}

/* ManalinkEh 2022: 0x25b15e0; original cards/commander_legends.c:5894. */
int card_volcanic_torrent(int player, int card, event_t event)
{
  /* Volcanic Torrent	|4|R
   * Sorcery
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.)
   * ~ deals X damage to each creature and planeswalker your opponents control, where X is the number of spells you've cast this turn. */

  int result;

  cascade(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {

    result = get_specific_storm_count(player);
    new_damage_all(player, card, 1 - player, result, 0x21, NULL);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25b1670; original cards/commander_legends.c:5913. */
int card_vow_of_torment(int player, int card, event_t event)
{
  /* Vow of Torment	|2|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+2, has menace, and can't attack you or planeswalkers you control. (It can't be blocked except by two or more creatures.) */

  int result;

  result = vow(player, card, event, 2, 2, 0, 0x10000);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25b16c0; original cards/commander_legends.c:5923. */
int card_war_room(int player, int card, event_t event)
{
  /* War Room	""
   * Land
   * |T: Add |C.
   * |3, |T, Pay life equal to the number of colors in your commanders' color identity: Draw a card. */

  uint32_t identity_colors;
  int available_or_life;
  unsigned int extra_life_loss;
  int choice;
  card_instance_t *instance;

  if ((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE))
  {
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      instance = get_card_instance(player, card);
      if ((event == EVENT_RESOLVE_ACTIVATION) && (instance->mode != '\0'))
      {
        draw_a_card(player);
        return 0;
      }
      return 0;
    }
    return mana_producer(player, card, event);
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    identity_colors = real_count_colors(get_color_identity_of_all_commanders(player, 3));
    if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 4, 0, 0, 0, 0, 0,
                                  identity_colors, NULL, NULL) != 0)
    {
      return 1;
    }
    event = EVENT_CAN_ACTIVATE;
    return mana_producer(player, card, event);
  }
  identity_colors = real_count_colors(get_color_identity_of_all_commanders(player, 3));
  instance->mode = '\0';
  if (paying_mana() == 0)
  {
    available_or_life = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 4, 0, 0, 0,
                                                  0, 0, identity_colors, NULL, NULL);
    extra_life_loss = (unsigned int)(5 < (int)identity_colors);
    if (available_or_life == 0)
    {
      life[player] = life[player] - extra_life_loss;
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
    choice = do_dialog(player, player, card, -1, -1,
                       " Produce mana\n Draw a card and lose life\n Cancel", 1);
    available_or_life = life[player];
    life[player] = available_or_life - extra_life_loss;
    if (((available_or_life - extra_life_loss == 0) ||
         ((ai_is_speculating != 1 && ((player != 1 || (((unsigned char)trace_mode & 2) != 0)))))) &&
        (choice != 0))
    {
      if (choice != 1)
      {
        spell_fizzled = 1;
        return 0;
      }
      add_state(player, card, 0x10);
      generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 3, 0, 0, 0, 0, 0, identity_colors,
                                NULL, NULL);
      if (spell_fizzled != 1)
      {
        instance->mode = '\x01';
        return 0;
      }
      remove_state(player, card, 0x10);
      return 0;
    }
  }
  else
  {
    extra_life_loss = (unsigned int)(5 < (int)identity_colors);
    life[player] = life[player] - extra_life_loss;
  }
  event = EVENT_ACTIVATE;
  return mana_producer(player, card, event);
}
#endif

/* ManalinkEh 2022: 0x25b19d0; original cards/commander_legends.c:5982. */
int card_wheel_of_misfortune(int player, int card, event_t event)
{
  /* Wheel of Misfortune	|2|R
   * Sorcery
   * Each player secretly chooses a number 0 or greater, then all players reveal those numbers simultaneously and determine the highest and lowest numbers revealed this way. ~ deals damage equal to the highest number to each player who chose that number. Each player who didn't choose the lowest number discards their hand, then draws seven cards. */

  int player_00;
  int result;
  unsigned int argument1;
  int argument2;
  int number[2];
  char buffer[100];

  if (event == EVENT_RESOLVE_SPELL)
  {
    number[1] = 1;
    result = number[1];
    if (6 < life[1])
    {
      number[1] = internal_rand(life[1] + -6);
      result = number[1];
    }
    do
    {
      number[0] = choose_a_number(0, "Choose a number 0 or greater", 1);
    } while (number[0] < 0);
    scnprintf(buffer, 100, "Opponent chosen: %d", result);
    argument1 = 1 - player;
    do_dialog(0, player, card, -1, -1, buffer, 0);
    player_00 = current_turn;
    result = number[player];
    argument2 = number[argument1];
    if (argument2 < result)
    {
      damage_player(player, result, player, card);
      new_discard_all(player, player);
      draw_cards(player, 7);
    }
    else
    {
      if (argument2 <= result)
      {

        damage_player(current_turn, number[current_turn], player, card);
        new_discard_all(player_00, player);
        draw_cards(player_00, 7);
        argument1 = current_turn ^ 1;
        argument2 = number[argument1];
      }
      damage_player(argument1, argument2, player, card);
      new_discard_all(argument1, player);
      draw_cards(argument1, 7);
    }
    kill_card(player, card, KILL_BURY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25b1ba0; original cards/commander_legends.c:6021. */
int card_wrong_turn(int player, int card, event_t event)
{
  /* Wrong Turn	|2|U
   * Instant
   * Target opponent gains control of target creature. (If an attacking or blocking creature changes controllers, it's removed from combat.) */

  uint8_t target_count;
  card_instance_t *instance;
  int result;
  target_definition_t td;
  target_definition_t td2;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    default_target_definition(player, card, &td2, 0);
    td2.zone = TARGET_ZONE_PLAYERS;
    td2.allowed_controller = 1 - player;
    get_card_instance(player, card);
    result = would_validate_arbitrary_target(&td2, 1 - player, -1);
    if (result != 0)
    {
      return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      default_target_definition(player, card, &td2, 0);
      td2.zone = TARGET_ZONE_PLAYERS;
      td2.allowed_controller = 1 - player;
      instance = get_card_instance(player, card);
      if ((card == affected_card) &&
          ((player == affected_card_controller &&
            (generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_CREATURE", 1, NULL),
             spell_fizzled != 1))))
      {
        target_count = instance->number_of_targets;
        instance->targets[1].player = 1 - player;
        instance->targets[1].card = -1;
        instance->number_of_targets = target_count + '\x01';
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    default_target_definition(player, card, &td2, 0);
    td2.allowed_controller = 1 - player;
    td2.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = validate_target(player, card, &td, 0);
      if ((result != 0) && (result = validate_target(player, card, &td2, 1), result != 0))
      {
        gain_control_permanently(instance->targets[1].player, instance->targets[0].player,
                                 instance->targets[0].card);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25b1eb0; original cards/commander_legends.c:6063. */
int card_wyleth_soul_of_steel(int player, int card, event_t event)
{
  /* Wyleth, Soul of Steel	|1|R|W
   * Legendary Creature - Human Warrior 2/2
   * Trample
   * Whenever Wyleth attacks, draw a card for each Aura and Equipment attached to it. */

  int auras;
  int equipment;

  check_legend_rule(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }

  auras = count_auras_enchanting_me(player, card);
  equipment = equipments_attached_to_me(player, card, 1);
  draw_cards(player, auras + equipment);
  return 0;
}

/* ManalinkEh 2022: 0x25b1f50; original cards/commander_legends.c:6082. */
int card_yurlok_of_scorch_thrash(int player, int card, event_t event)
{
  /* Yurlok of Scorch Thrash	|1|B|R|G
   * Legendary Creature - Lizard Shaman 4/4
   * Vigilance
   * A player losing unspent mana causes that player to lose that much life.
   * |1, |T: Each player adds |B|R|G. */

  int *mana_entry;
  int index_or_total;
  unsigned int player_00;
  int total_or_index;

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  if (event == EVENT_MANA_POOL_DRAINING)
  {
    if (is_humiliated(player, card, AL_STATIC) == 0)
    {

      total_or_index = 0;
      index_or_total = 0;
      do
      {
        mana_entry = mana_pool[current_turn] + index_or_total;
        index_or_total = index_or_total + 1;
        total_or_index = total_or_index + *mana_entry;
      } while (index_or_total != 6);
      lose_life(current_turn, total_or_index + mana_pool[current_turn][6]);
      player_00 = current_turn ^ 1;
      total_or_index = 0;
      index_or_total = 0;
      do
      {
        mana_entry = mana_pool[player_00] + total_or_index;
        total_or_index = total_or_index + 1;
        index_or_total = index_or_total + *mana_entry;
      } while (total_or_index != 6);
      lose_life(player_00, index_or_total + mana_pool[player_00][6]);
    }
  }
  else
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x200001, 1, 0, 0, 0, 0, 0,
                                       0, NULL, NULL);
    }
    if ((event == EVENT_ACTIVATE) && (generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 1,
                                                                0, 0, 0, 0, 0, 0, NULL, NULL),
                                      spell_fizzled != 1))
    {
      produce_mana_multi(player, 0, 1, 0, 1, 1, 0);
      produce_mana_multi(1 - player, 0, 1, 0, 1, 1, 0);
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25b21a0; original cards/commander_legends.c:6120. */
int card_zara_renegade_recruiter(int player, int card, event_t event)
{
  /* Zara, Renegade Recruiter	|3|U|R
   * Legendary Creature - Human Pirate 4/3
   * Flying
   * Whenever Zara attacks, look at defending player's hand. You may put a creature card from it onto the battlefield under your control tapped and attacking that player or a planeswalker they control. Return that creature to its owner's hand at the beginning of the next end step. */

  int target_card;
  test_definition_t test;

  check_legend_rule(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if ((event == EVENT_DAT_ABILITY) && (hand_count[1 - player] != 0))
  {

    new_default_test_definition(&test, 2, "Select a creature card.");
    target_card = new_global_tutor(player, 1 - player, 4, 8, 0, 2, &test);
    if (-1 < target_card)
    {
      create_targetted_legacy_effect(player, card, bounce_at_eot, player, target_card);
    }
  }
  return 0;
}
#endif
