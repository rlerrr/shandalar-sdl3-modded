#include "manalink.h"

#ifndef SHANDALAR
static void ghireds_special_populate(int player, int card);
#endif

static int atla_palani_effect(int player, int card, event_t event);

static int effect_volrath_the_shapestealer(int player, int card, event_t event);
static int volrath_the_shapestealer_impl(int player, int card, event_t event, int is_effect);

/* ManalinkEh 2022: 0x23b1fa0; original cards/commander_2019.c:2. */
static int goad_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_card)
  {
    instance = get_card_instance(player, card);
    attack_if_able((int)instance->damage_target_player, instance->damage_target_card, event);
  }
  if (event == EVENT_CAN_SKIP_TURN)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_source_player == current_turn)
    {
      kill_card(player, card, KILL_EXILE);
    }
  }
  return 0;
}

#ifndef SHANDALAR
static int goad(int player, int card, int t_player, int t_card)
{
  return create_targetted_legacy_effect(player, card, goad_effect, t_player, t_card);
}
#endif

/* ManalinkEh 2022: 0x23b2090; original cards/commander_2019.c:31. */
int card_anje_falkenrath(int player, int card, event_t event)
{
  /* Anje Falkenrath	|1|B|R
   * Legendary Creature - Vampire 1/3
   * Haste
   * |T, Discard a card: Draw a card.
   * Whenever you discard a card, if it has madness, untap ~. */

  int result;
  check_legend_rule(player, card, event);
  haste(player, card, event);
  result = discard_trigger(player, card, event, player, 2, 0);
  if ((result != 0) &&
      (result = get_original_internal_card_id(trigger_cause_controller, trigger_cause),
       cards_data[result].cc[2] == '\v'))
  {
    untap_card(player, card);
  }
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_a_card(player);
  }
  result = generic_activated_ability(player, card, event, 0x21, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x23b21b0; original cards/commander_2019.c:62. */
int card_apex_altisaur(int player, int card, event_t event)
{
  /* Apex Altisaur	|7|G|G
   * Creature - Dinosaur 10/10
   * When this creature enters, it fights up to one target creature you don't control.
   * Enrage - Whenever this creature is dealt damage, it fights up to one target creature you don't control. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if ((event != EVENT_ETB) && (result = damage_dealt_to_me_arbitrary(
                                   player, card, event, DDBM_TRIGGER_OPTIONAL, player, card),
                               result == 0))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  result = can_target(&td);
  if ((result != 0) &&
      (result = new_pick_target(&td, "Select target creature you don\'t control.", 0, 0x80),
       result != 0))
  {
    instance = get_card_instance(player, card);
    result = instance->targets[0].card;
    instance->number_of_targets = '\0';
    fight(player, card, instance->targets[0].player, result);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23b14c0; original cards/commander_2019.c:95. */
static int atla_palani_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  unsigned int egg_death_count;
  card_instance_t *dying_permanent;
  test_definition_t test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if (card == affected_card && player == affected_card_controller)
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (get_id((int)instance->damage_target_player, instance->damage_target_card) != 0x4fe3))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0 &&
      effect_follows_control_of_attachment(player, card, event) != 0)
    return 0;
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = get_card_instance(player, card);
    dying_permanent = in_play(affected_card_controller, affected_card);

    if ((dying_permanent != NULL) &&
        (dying_permanent = get_card_instance(affected_card_controller, affected_card),
         dying_permanent->kill_code != '\0'))
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
      }
      default_test_definition(&test, 0x47);
      test.subtype = 0xfb;
      count_for_gfp_ability(player, card, 0x77, player, 0, &test);
    }
  }
  egg_death_count = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (!check_status(player, card, 0x10000))
  {
    egg_death_count = egg_death_count & 0xff;
    if (egg_death_count != 0)
    {
      do
      {
        default_test_definition(&test, 2);
        egg_death_count = egg_death_count - 1;
        reveal_card_until_you_reveal_test(player, player, &test, 0x44);
        kill_card(player, card, KILL_EXILE);
      } while (egg_death_count != 0);
    }
  }
  if ((event == EVENT_CLEANUP) && (check_status(player, card, 0x10000) == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23b22a0; original cards/commander_2019.c:154. */
int card_atla_palani_nest_tender(int player, int card, event_t event)
{
  /* Atla Palani, Nest Tender	|1|R|G|W
   * Legendary Creature - Human Shaman 2/3
   * |2, |T: Create a 0/1 green Egg creature token with defender.
   * Whenever an Egg you control dies, reveal cards from the top of your library until you reveal a creature card. Put that card onto the battlefield and the rest on the bottom of your library in a random order. */

  int c;
  card_instance_t *instance;
  int legacy_card;
  unsigned int egg_death_count;
  test_definition_t test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      bool legacy_found = false;
      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          instance = in_play(player, c);

          if ((((instance != NULL) && (is_what(player, c, 0x80) != 0)) &&
               (instance = get_card_instance(player, c),
                instance->info_slot_fn == atla_palani_effect)) &&
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
        legacy_card =
            create_targetted_legacy_effect(player, card, atla_palani_effect, player, card);
        instance = get_card_instance(player, legacy_card);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, legacy_card, 0x10000);
      }
    }
  }
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    default_test_definition(&test, 0x47);
    test.subtype = 0xfb;
    count_for_gfp_ability(player, card, 0x77, player, 0, &test);
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_GRAVEYARD_FROM_PLAY && instance->kill_code != 0)
    return 0;
  if (instance->kill_code == 0)
  {
    egg_death_count = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (egg_death_count != 0)
    {

      egg_death_count = egg_death_count & 0xff;
      if (egg_death_count != 0)
      {
        do
        {
          default_test_definition(&test, 2);
          reveal_card_until_you_reveal_test(player, player, &test, 0x44);
          egg_death_count = egg_death_count - 1;
        } while (egg_death_count != 0);
      }
    }
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
    generate_token_by_id(player, card, CARD_ID_EGG);
  return generic_activated_ability(player, card, event, 1, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x23b25b0; original cards/commander_2019.c:235. */
int card_bone_miser(int player, int card, event_t event)
{
  /* Bone Miser	|4|B
   * Creature - Zombie Wizard 4/4
   * Whenever you discard a creature card, create a 2/2 black Zombie creature token.
   * Whenever you discard a land card, add |B|B.
   * Whenever you discard a noncreature, nonland card, draw a card. */

  if (!discard_trigger(player, card, event, player, 2, 0))
    return 0;
  int creature = is_what(trigger_cause_controller, trigger_cause, 2);
  int land = is_what(trigger_cause_controller, trigger_cause, 1);
  if (creature)
    generate_token_by_id(player, card, CARD_ID_ZOMBIE);
  if (land)
    produce_mana(player, 1, 2);
  else if (!creature)
    draw_a_card(player);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23b1850; original cards/commander_2019.c:260. */
static int chainers_legacy(int player, int card, event_t event)
{
  int iid_or_playability;
  int grave_byte_offset;
  int *current_graveyard;
  if (event == EVENT_CAN_ACTIVATE)
  {
    iid_or_playability = *graveyard_ptr_mutable[player];
    if (iid_or_playability != -1)
    {
      grave_byte_offset = 0;
      do
      {

        iid_or_playability = is_what(-1, iid_or_playability, 2);
        current_graveyard = graveyard_ptr_mutable[player];
        if (iid_or_playability != 0)
        {

          iid_or_playability =
              can_legally_play_iid(player, *(int *)((int)current_graveyard + grave_byte_offset));
          current_graveyard = graveyard_ptr_mutable[player];
          if (iid_or_playability != 0)
          {

            if (has_mana_to_cast_iid(player, EVENT_CAN_ACTIVATE,
                                     *(int *)((int)current_graveyard + grave_byte_offset)) == 0)
            {
              current_graveyard = graveyard_ptr_mutable[player];
            }
            else
            {
              if (iid_or_playability == 99)
              {
                return 99;
              }

              iid_or_playability = can_sorcery_be_played(player, EVENT_CAN_ACTIVATE);
              if ((iid_or_playability != 0) ||
                  (current_graveyard = graveyard_ptr_mutable[player],
                   (cards_data[*(int *)((int)current_graveyard + grave_byte_offset)].type & 0x30) !=
                       0))
              {
                return 1;
              }
            }
          }
        }
        grave_byte_offset = grave_byte_offset + 4;
        iid_or_playability = *(int *)((int)current_graveyard + grave_byte_offset);
      } while (iid_or_playability != -1);
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    int grave_count = count_graveyard(player);
    int playable[2][grave_count];
    int count = 0;
    int i = 0;
    int *grave = graveyard_ptr_mutable[player];
    int iid = *grave;
    test_definition_t this_test;
    while (iid != -1)
    {
      int creature = is_what(-1, iid, TYPE_CREATURE);
      grave = graveyard_ptr_mutable[player];
      if (creature)
      {
        int legal = can_legally_play_iid(player, grave[i]);
        grave = graveyard_ptr_mutable[player];
        if (legal)
        {
          if (!has_mana_to_cast_iid(player, EVENT_ACTIVATE, grave[i]))
            grave = graveyard_ptr_mutable[player];
          else
          {
            int sorcery_ok = can_sorcery_be_played(player, EVENT_ACTIVATE);
            grave = graveyard_ptr_mutable[player];
            iid = grave[i];
            if (sorcery_ok || (cards_data[iid].type & 0x30))
            {
              playable[0][count] = iid;
              playable[1][count] = i;
              ++count;
            }
          }
        }
      }
      iid = grave[i + 1];
      ++i;
    }
    new_default_test_definition(&this_test, 0xff, "Select a creature card to play.");
    int selected = select_card_from_zone(player, player, playable[0], count, 0, 1, -1, &this_test);
    if (selected != -1 &&
        charge_mana_from_id(player, -1, EVENT_ACTIVATE, cards_data[playable[0][selected]].id))
    {
      play_card_in_grave_for_free(player, player, playable[1][selected]);
      cant_be_responded_to = 1;
      kill_card(player, card, KILL_EXILE);
      return 0;
    }
    spell_fizzled = 1;
    return 0;
  }
  else if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23b26b0; original cards/commander_2019.c:329. */
int card_chainer_nightmare_adept(int player, int card, event_t event)
{
  /* Chainer, Nightmare Adept	|2|B|R
   * Legendary Creature - Human Minion 3/2
   * Discard a card: You may cast a creature spell from your graveyard this turn. Activate only once each turn.
   * Whenever a nontoken creature you control enters, if you didn't cast it from your hand, it gains haste until your next turn. */

  check_legend_rule(player, card, event);
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY &&
      not_played_from_hand(trigger_cause_controller, trigger_cause) && card == affected_card &&
      player == affected_card_controller && reason_for_trigger_controller == player)
  {
    test_definition_t test;
    new_default_test_definition(&test, TYPE_CREATURE, "");
    test.type_flag = 3;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
  {
    int leg = pump_ability_until_eot_no_repeat(player, card, trigger_cause_controller,
                                               trigger_cause, 0, 4);
    if (current_turn == player)
      get_card_instance(player, leg)->targets[7].player = 1;
    return 0;
  }
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
    create_legacy_activate(player, card, chainers_legacy);
  return generic_activated_ability(player, card, event, 0x2020, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x23b28a0; original cards/commander_2019.c:384. */
int card_dockside_extortionist(int player, int card, event_t event)
{
  /* Dockside Extortionist	|1|R
   * Creature - Goblin Pirate 1/2
   * When this creature enters, create X Treasure tokens, where X is the number of artifacts and enchantments your opponents control. (Treasure tokens are artifacts with "|T, Sacrifice this token: Add one mana of any color.") */

  int howmany;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  howmany = count_permanents_by_type(1 - player, TYPE_ARTIFACT | TYPE_ENCHANTMENT);
  generate_tokens_by_id(player, card, CARD_ID_TREASURE, howmany);
  return 0;
}

/* ManalinkEh 2022: 0x23b2920; original cards/commander_2019.c:397. */
int card_doomed_artisan(int player, int card, event_t event)
{
  /* Doomed Artisan	|2|W
   * Creature - Human Artificer 1/1
   * Sculptures you control can't attack or block.
   * At the beginning of your end step, create a colorless Sculpture artifact creature token with "This token's power and toughness are each equal to the number of Sculptures you control." */

  int result;
  token_generation_t token;

  if ((((((uint32_t)(event - EVENT_BLOCK_LEGALITY) < 2) || (event == EVENT_ABILITIES)) &&
        (result = has_subtype(affected_card_controller, affected_card, SUBTYPE_SCULPTURE),
         result != 0)) &&
       ((affected_card_controller == player &&
         (result = is_humiliated(player, card, AL_STATIC), result == 0)))) &&
      ((1 < (uint32_t)(event - EVENT_BLOCK_LEGALITY) || (event_result = 1, event == EVENT_ABILITIES))))
  {
    cannot_attack(affected_card_controller, affected_card, EVENT_ABILITIES);
    cannot_block(affected_card_controller, affected_card, EVENT_ABILITIES);
  }
  if ((current_turn == player) && (result = eot_trigger(player, card, event), result != 0))
  {
    default_token_definition(player, card, CARD_ID_SCULPTURE, &token);
    token.special_infos = 0x42;
    generate_token(&token);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23b2a50; original cards/commander_2019.c:424. */
int card_elsha_of_the_infinite(int player, int card, event_t event)
{
  /* Elsha of the Infinite	|2|U|R|W
   * Legendary Creature - Djinn Monk 3/3
   * Prowess (Whenever you cast a noncreature spell, this creature gets +1/+1 until end of turn.)
   * You may look at the top card of your library any time.
   * You may cast noncreature spells from the top of your library. If you cast a spell this way, you may cast it as though it had flash. */

  enum
  {
    CHOICE_REVEAL = 1,
    CHOICE_PLAY = 2
  };

  int result;
  uint32_t look_available;
  unsigned int play_available;

  check_legend_rule(player, card, event);
  prowess(player, card, event);
  if (event == EVENT_CAN_ACTIVATE)
  {
    return (unsigned int)(*deck_ptr[player] != -1);
  }
  if (event != EVENT_ACTIVATE)
  {
    return 0;
  }

  result = *deck_ptr[player];
  look_available = 0;
  if (ai_is_speculating != 1 && !(player == 1 && ((unsigned char)trace_mode & 2) == 0))
    look_available = result != -1;
  play_available = 0;
  if (result != -1)
  {
    result = is_what(-1, result, 2);
    if (result == 0)
    {
      result = is_what(-1, *deck_ptr[player], 1);
      if (result == 0)
      {
        result = has_mana_to_cast_iid(player, EVENT_ACTIVATE, *deck_ptr[player]);
        play_available = (unsigned int)(result != 0);
      }
    }
  }
  result =
      dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Check top card of deck",
                  look_available, 0, "Play top card", play_available, 10, 0, 0, 0, 0);
  if (result != 0)
  {
    if (result != CHOICE_REVEAL)
    {
      if (result == CHOICE_PLAY)
      {
        result = charge_mana_from_id(player, -1, EVENT_ACTIVATE,
                                     (unsigned int)cards_data[*deck_ptr[player]].id);
        if (result == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        play_card_in_deck_for_free(player, player, 0);
        cant_be_responded_to = 1;
      }
      return 0;
    }
    show_deck(player, deck_ptr[player], 1, "Here\'s the top card of your deck", 0,
              gs_done_008b40e0);
  }
  spell_fizzled = 1;
  return 0;
}

/* ManalinkEh 2022: 0x23b2cd0; original cards/commander_2019.c:486. */
int card_gerrard_weatherlight_hero(int player, int card, event_t event)
{
  /* Gerrard, Weatherlight Hero	|2|R|W
   * Legendary Creature - Human Soldier 3/3
   * First strike
   * When Gerrard dies, exile it and return to the battlefield all artifact and creature cards in your graveyard that were put there from the battlefield this turn. */

  int result;
  check_legend_rule(player, card, event);
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  exile_from_owners_graveyard(player, card);
  reanimate_all_dead_this_turn(player, -0x7fffffbe);
  return 0;
}

#ifndef SHANDALAR
static void ghireds_special_populate(int player, int card)
{
  if (player == AI || ai_is_speculating == 1)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, TYPE_CREATURE);
    this_test.type_flag = F1_IS_TOKEN;
    int selected = check_battlefield_for_special_card(player, card, player, 1, &this_test);
    if (selected >= 0)
    {
      token_generation_t token;
      copy_token_definition(player, card, &token, player, selected);
      token.action = TOKEN_ACTION_ATTACKING;
      generate_token(&token);
    }
  }
  else
  {
    target_definition_t td;
    base_target_definition(player, card, &td, TYPE_CREATURE);
    td.allowed_controller = td.preferred_controller = player;
    td.allow_cancel = 0;
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (int32_t)target_is_token_too;
    target_t tgt;
    if (can_target(&td) && select_target(player, card - 1000, &td,
                                        "Select a creature token you control.", &tgt))
    {
      token_generation_t token;
      copy_token_definition(player, card, &token, tgt.player, tgt.card);
      token.action = TOKEN_ACTION_ATTACKING;
      generate_token(&token);
    }
  }
}
#endif

/* ManalinkEh 2022: 0x23b2d40; original cards/commander_2019.c:546. */
int card_ghired_conclave_exile(int player, int card, event_t event)
{
  /* Ghired, Conclave Exile	|2|R|G|W
   * Legendary Creature - Human Shaman 2/5
   * When Ghired enters, create a 4/4 green Rhino creature token with trample.
   * Whenever Ghired attacks, populate. The token enters tapped and attacking. (To populate, create a token that's a copy of a creature token you control.) */

  int result;


  check_legend_rule(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_RHINO, &token);
    token.tou = 4;
    token.pow = 4;
    token.key_plus = 0x80;
    generate_token(&token);
    dat_ability(player, card, EVENT_ETB, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
    return 0;
  }
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
  {
    ghireds_special_populate(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23b2f70; original cards/commander_2019.c:572. */
int card_gift_of_doom(int player, int card, event_t event)
{
  /* Gift of Doom	|4|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has deathtouch and indestructible.
   * Morph-Sacrifice another creature. (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
   * As this Aura is turned face up, you may attach it to a creature. */

  int result;
  test_definition_t this_test;
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      morph(player, card, EVENT_CAST_SPELL, 0, 0, 0, 0, 0, 0);
      result = get_id(player, card);
      if (result == 0x33c4)
      {
        return 0;
      }
      return generic_aura(player, card, EVENT_CAST_SPELL, player, 0, 0, 0, 0xa00, 0, 0, 0);
    }
  }
  else if ((((event == EVENT_CAN_CAST) || (event == EVENT_RESOLVE_SPELL)) ||
            (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    morph(player, card, event, 0, 0, 0, 0, 0, 0);
    return generic_aura(player, card, event, player, 0, 0, 0, 0xa00, 0, 0, 0);
  }
  if ((((((event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA) &&
         (2 < (uint32_t)(event - EVENT_POWER))) &&
        ((event != EVENT_RESOLVE_MOVING_AURA &&
          ((event != EVENT_CHANGE_TYPE && (event != EVENT_SET_COLOR)))))) &&
       (event != EVENT_GET_SELECTED_CARD)) &&
      (event != EVENT_STATIC_EFFECTS))
  {
    if (event == EVENT_CAN_UNMORPH)
    {
      default_test_definition(&this_test, 2);
      this_test.qty = 1;
      this_test.not_me = 1;
      return new_can_sacrifice_as_cost(player, card, &this_test);
    }
    if (event == EVENT_UNMORPH)
    {
      new_default_test_definition(&this_test, 2, "Select a creature to sacrifice.");
      this_test.not_me = 1;
      result = new_sacrifice(player, card, player, SAC_AS_COST, &this_test);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
    else if (event == EVENT_TURNED_FACE_UP)
    {
      result = vanilla_aura(player, card, EVENT_CAN_CAST, player);
      if (result == 0)
      {
        kill_card(player, card, KILL_STATE_BASED_ACTION);
      }
      else
      {
        vanilla_aura(player, card, EVENT_CAST_SPELL, player);
        vanilla_aura(player, card, EVENT_RESOLVE_SPELL, player);
      }
    }
    return morph(player, card, event, 0, 0, 0, 0, 0, 0);
  }
  morph(player, card, event, 0, 0, 0, 0, 0, 0);
  return generic_aura(player, card, event, player, 0, 0, 0, 0xa00, 0, 0, 0);
}

/* ManalinkEh 2022: 0x23b32d0; original cards/commander_2019.c:622. */
int card_greven_predator_captain(int player, int card, event_t event)
{
  /* Greven, Predator Captain	|3|B|R
   * Legendary Creature - Phyrexian Human Warrior 5/5
   * Menace
   * Greven gets +X/+0, where X is the amount of life you've lost this turn.
   * Whenever Greven attacks, you may sacrifice another creature. If you do, you draw cards equal to that creature's power and you lose life equal to that creature's toughness. */

  int result;
  card_instance_t *instance;
  unsigned int sacrifice_choice_or_player;
  unsigned int card_00;
  test_definition_t test_sac;

  check_legend_rule(player, card, event);
  menace(player, card, event);
  if (event != EVENT_POWER)
  {
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
    if (event == EVENT_DAT_ABILITY)
    {
      result = can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        instance->token_status = instance->token_status | 0x20000;
        new_default_test_definition(&test_sac, 2, "Select another creature to sacrifice.");
        test_sac.not_me = 1;
        sacrifice_choice_or_player =
            new_sacrifice(player, card, player, SAC_RETURN_CHOICE | SAC_JUST_MARK, &test_sac);
        if (sacrifice_choice_or_player == 0)
        {
          spell_fizzled = 1;
          instance->token_status = instance->token_status & 0xfffdffff;
        }
        else
        {
          if (sacrifice_choice_or_player == 0xffffffff)
          {
            backtrace_and_abort("cards/commander_2019.c:653\nsac != -1");
          }
          card_00 = sacrifice_choice_or_player >> 0x18;
          sacrifice_choice_or_player = (int)sacrifice_choice_or_player >> 0x10 & 0xff;
          result = get_power(sacrifice_choice_or_player, card_00);
          draw_cards(player, result);
          result = get_toughness(sacrifice_choice_or_player, card_00);
          lose_life(player, result);
          kill_card(sacrifice_choice_or_player, card_00, KILL_SACRIFICE);
          instance->token_status = instance->token_status & 0xfffdffff;
        }
      }
    }
    return 0;
  }
  if ((card == affected_card) && (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      result = get_trap_condition(player, 2);
      event_result = event_result + result;
      instance = get_card_instance(player, card);
      instance->regen_status = instance->regen_status | 0x4000000;
    }
  }
  dat_ability(player, card, EVENT_POWER, DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
  return 0;
}

/* ManalinkEh 2022: 0x23b3540; original cards/commander_2019.c:662. */
int card_grismold_the_dreadsower(int player, int card, event_t event)
{
  /* Grismold, the Dreadsower	|1|B|G
   * Legendary Creature - Troll Shaman 3/3
   * Trample
   * At the beginning of your end step, each player creates a 1/1 green Plant creature token.
   * Whenever a creature token dies, put a +1/+1 counter on Grismold. */

  int first_token_player;
  unsigned int other_player_or_deaths;
  int end_step_triggered;
  token_generation_t token;

  check_legend_rule(player, card, event);
  if ((current_turn == player) && (end_step_triggered = eot_trigger(player, card, event),
                                   first_token_player = current_turn, end_step_triggered != 0))
  {

    default_token_definition(player, card, CARD_ID_PLANT, &token);
    token.t_player = first_token_player;
    token.pow = 1;
    token.tou = 1;
    generate_token(&token);
    other_player_or_deaths = current_turn ^ 1;
    default_token_definition(player, card, CARD_ID_PLANT, &token);
    token.pow = 1;
    token.tou = 1;
    token.t_player = other_player_or_deaths;
    generate_token(&token);
  }
  count_for_gfp_ability_and_store_values(player, card, event, 2, 2, NULL, 0x400, 0);
  other_player_or_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (other_player_or_deaths != 0)
  {

    for (other_player_or_deaths = other_player_or_deaths & 0xff; other_player_or_deaths != 0;
         other_player_or_deaths = other_player_or_deaths - 1)
    {
      add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23b36b0; original cards/commander_2019.c:720. */
int card_krrik_son_of_yawgmoth(int player, int card, event_t event)
{
  /* K'rrik, Son of Yawgmoth	|4|BP|BP|BP
   * Legendary Creature - Phyrexian Horror Minion 2/2
   * (|BP can be paid with either |B or 2 life.)
   * Lifelink
   * For each |B in a cost, you may pay 2 life rather than pay that mana.
   * Whenever you cast a black spell, put a +1/+1 counter on K'rrik. */

  test_definition_t test;

  check_legend_rule(player, card, event);
  lifelink(player, card, event);
  phyrexian_casting_costs(player, card, event, 4, 3, 0, 0, 0, 0, 0);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {
      card_flags[player] = card_flags[player] | CFLAGS_KRRIK;
    }
    if (trigger_condition != TRIGGER_SPELL_CAST)
    {
      return 0;
    }
  }
  if (trigger_condition == TRIGGER_SPELL_CAST && card == affected_card && player == affected_card_controller)
  {
    default_test_definition(&test, 0xff);
    test.color = get_sleighted_color_test(player, card, 2);
    sp_ability_trigger(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event != EVENT_SPA)
  {
    return 0;
  }
  add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  return 0;
}
#endif

/* ManalinkEh 2022: 0x23b3840; original cards/commander_2019.c:784. */
int card_marisi_breaker_of_the_coil(int player, int card, event_t event)
{
  /* Marisi, Breaker of the Coil	|1|R|G|W
   * Legendary Creature - Cat Warrior 5/4
   * Your opponents can't cast spells during combat.
   * Whenever a creature you control deals combat damage to a player, goad each creature that player controls. (Until your next turn, those creatures attack each combat if able and attack a player other than you if able.) */

  unsigned int damaged_player_flags;
  int damaged_player_or_result;
  card_instance_t *instance;
  int card_00;
  int num[2];
  test_definition_t test;

  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    damaged_player_or_result = affected_card_controller;
    if (affected_card_controller == -1)
    {
      damaged_player_or_result = event_result;
    }
    if ((((1 - player == damaged_player_or_result) && (current_phase - 0x15U < 7)) &&
         (damaged_player_or_result = is_what(affected_card_controller, affected_card, 1),
          damaged_player_or_result == 0)) &&
        (damaged_player_or_result = is_humiliated(player, card, AL_STATIC),
         damaged_player_or_result == 0))
    {
      infinite_casting_cost();
    }
  }
  default_test_definition(&test, 2);
  check_damage_test(player, -1, event,
                    DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                        DDBM_MUST_BE_COMBAT_DAMAGE,
                    player, card, 0, &test);
  if (resolve_damage_trigger(player, card, event,
                             DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                                 DDBM_MUST_BE_COMBAT_DAMAGE,
                             player, card) != 0)
  {

    damaged_player_or_result = 0;
    instance = get_card_instance(player, card);
    damaged_player_flags = instance->targets[1].player;
    instance->targets[1].player = 0;
    num[0] = damaged_player_flags & 0xff;
    num[1] = damaged_player_flags >> 8 & 0xff;
    damaged_player_flags = num[0];
    while (true)
    {
      if ((damaged_player_flags != 0) && (0 < active_cards_count[damaged_player_or_result]))
      {
        card_00 = 0;
        do
        {
          instance = in_play(damaged_player_or_result, card_00);
          if ((instance != NULL) && (is_what(damaged_player_or_result, card_00, 2) != 0))
          {

            create_targetted_legacy_effect(player, card, goad_effect, damaged_player_or_result,
                                           card_00);
          }
          card_00 = card_00 + 1;
        } while (card_00 < active_cards_count[damaged_player_or_result]);
      }
      damaged_player_or_result = damaged_player_or_result + 1;
      if (damaged_player_or_result == 2)
        break;
      damaged_player_flags = num[damaged_player_or_result];
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23b3a10; original cards/commander_2019.c:849. */
int card_ohran_frostfang(int player, int card, event_t event)
{
  /* Ohran Frostfang	|3|G|G
   * Snow Creature - Snake 2/6
   * Attacking creatures you control have deathtouch.
   * Whenever a creature you control deals combat damage to a player, draw a card. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_ABILITIES)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((result == 0) && (affected_card_controller == player))
    {
      result = is_what(player, affected_card, 2);
      if (result != 0)
      {
        result = is_attacking(affected_card_controller, affected_card);
        if (result != 0)
        {
          instance = in_play(player, card);
          if (instance != NULL)
          {
            deathtouch(affected_card_controller, affected_card, EVENT_ABILITIES);
          }
        }
      }
    }
  }
  result = subtype_deals_damage(player, card, event, player, -1, 0x14);
  if (result == 0)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  draw_cards(player, instance->targets[1].card);
  return 0;
}

/* ManalinkEh 2022: 0x23b2030; original cards/commander_2019.c:904. */
static char *commander_you_own(int who_chooses, int player, int card, int targeting_player,
                               int targeting_card)
{
  int result;
  char *rejection;
  result = get_owner(player, card);
  rejection = "owner";
  if (result == targeting_player)
  {
    result = check_special_flags2(player, card, 0x1000000);
    rejection = NULL;
    if (result == 0)
    {
      return "must be a commander";
    }
  }
  return rejection;
}

/* ManalinkEh 2022: 0x23b3b30; original cards/commander_2019.c:911. */
int card_sanctum_of_eternity(int player, int card, event_t event)
{
  /* Sanctum of Eternity	""
   * Land
   * |T: Add |C.
   * |2, |T: Return target commander you own from the battlefield to your hand. Activate only during your turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      default_target_definition(player, card, &td, 0x47);
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      td.extra = (int32_t)commander_you_own;
      instance = get_card_instance(player, card);

      instance->info_slot = 0;
      result = paying_mana();
      if (((result == 0) &&
           (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 7, 2, 0, 0, 0, 0,
                                               0, 0, &td, "Select a commander you own."),
            result != 0)) &&
          (result = do_dialog(player, player, card, -1, -1,
                              " Generate mana\n Bounce your commander\n Cancel", 1),
           result != 0))
      {
        if (result != 1)
        {
          return 0;
        }
        add_state(player, card, 0x10);
        result = generic_activated_ability(player, card, EVENT_ACTIVATE, 6, 2, 0, 0, 0, 0, 0, 0,
                                           &td, "Select a commander you own.");
        if (result == 0)
        {
          remove_state(player, card, 0x10);
        }
        else
        {
          instance->info_slot = 1;
        }
        tapped_for_mana_color = -1;
        return 0;
      }
      result = mana_producer(player, card, EVENT_ACTIVATE);
      return result;
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      default_target_definition(player, card, &td, 0x47);
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      td.extra = (int32_t)commander_you_own;
      instance = get_card_instance(player, card);
      if ((event == EVENT_RESOLVE_ACTIVATION) && (0 < instance->info_slot))
      {
        bounce_permanent(instance->targets[0].player, instance->targets[0].card);
        return 0;
      }
      return 0;
    }
  }
  result = mana_producer(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23b3dd0; original cards/commander_2019.c:983. */
int card_sevinnes_reclamation(int player, int card, event_t event)
{
  /* Sevinne's Reclamation	|2|W
   * Sorcery
   * Return target permanent card with mana value 3 or less from your graveyard to the battlefield. If this spell was cast from a graveyard, you may copy this spell and may choose a new target for the copy.
   * Flashback |4|W (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

  int result;
  test_definition_t test;
  if (((uint32_t)(event - EVENT_CAST_SPELL) & 0xfffffff7) != 0)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = check_special_flags(player, card, 0x10000000);
      if (result != 0)
      {
        result = check_special_flags2(player, card, -0x80000000);
        if (result == 0)
        {
          copy_spell_from_stack(player, player, card);
        }
      }
      result = get_flashback(player, card);
      if (result != 0)
      {
        kill_card(player, card, KILL_EXILE);
      }
      result = validate_target_from_grave_source(player, card, player, 0);
      if (result != -1)
      {
        reanimate_permanent(player, card, player, result, REANIMATE_DEFAULT);
      }
      kill_card(player, card, KILL_DESTROY);
    }
    result = do_flashback(player, card, event, 4, 0, 0, 0, 0, 1);
    return result;
  }
  new_default_test_definition(&test, 0x47, "Select a permanent card with mana value 3 or less.");
  test.cmc = 4;
  test.cmc_flag = 3;
  result = generic_spell(player, card, event, 8, NULL, NULL, 0, &test);
  return result;
}

/* ManalinkEh 2022: 0x23b3f90; original cards/commander_2019.c:1032. */
int card_song_of_the_worldsoul(int player, int card, event_t event)
{
  /* Song of the Worldsoul	|4|W|W
   * Enchantment
   * Whenever you cast a spell, populate. (Create a token that's a copy of a creature token you control.) */

  int result;

  sp_ability_trigger(player, event, 2, TYPE_ANY, NULL, player, card);
  if (event == EVENT_SPA)
  {
    populate(player, card);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x23b1bd0; original cards/commander_2019.c:1106. */
static int volrath_the_shapestealer_impl(int player, int card, event_t event, int is_effect)
{
  int p;
  char enchanted_player;
  card_instance_t *instance;
  int counter_target_available;
  card_instance_t *source_or_copy_effect;
  int legacy_type_match;
  int card_00;
  int copy_source_player;
  int copy_source_card;
  target_definition_t td_copy;

  instance = get_card_instance(player, card);
  source_or_copy_effect = instance;
  copy_source_player = player;
  copy_source_card = card;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    copy_source_player = instance->parent_controller;
    copy_source_card = instance->parent_card;
    source_or_copy_effect = get_card_instance(copy_source_player, copy_source_card);
  }
  if (is_effect == 0)
  {
    check_legend_rule(player, card, event);
    cloning_card(player, card, event);
    if (beginning_of_combat(player, card, event, player) != 0)
    {
      default_target_definition(player, card, &td_copy, 2);
      td_copy.preferred_controller = 1 - player;
      counter_target_available = can_target(&td_copy);
      if ((counter_target_available != 0) &&
          (counter_target_available = new_pick_target(&td_copy, "TARGET_CREATURE", 0, 1),
           counter_target_available != 0))
      {
        add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                     COUNTER_M1_M1, 1);
      }
    }
  }
  else
  {
    copy_source_player = (int)source_or_copy_effect->damage_target_player;
    copy_source_card = source_or_copy_effect->damage_target_card;
    enchanted_player = instance->damage_target_player;
    if (enchanted_player >= 0)
    {
      if (event == EVENT_TOUGHNESS)
      {
        if (instance->damage_target_card != affected_card ||
            enchanted_player != affected_card_controller || instance->counter_toughness < 0)
          return 0;
        set_base_pt(5);
        return 0;
      }
      if (event == EVENT_POWER && instance->damage_target_card == affected_card &&
          enchanted_player == affected_card_controller && instance->counter_power >= 0)
        set_base_pt(7);
    }
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
  {
    return 0;
  }
  default_target_definition(player, card, &td_copy, 2);
  td_copy.special = TARGET_SPECIAL_REQUIRES_COUNTER;
  td_copy.extra = 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    p = 0;
    do
    {
      card_00 = active_cards_count[p];
      if (-1 < card_00)
      {
        do
        {
          source_or_copy_effect = in_play(p, card_00);
          if ((((source_or_copy_effect != NULL) &&
                (source_or_copy_effect->damage_target_player == copy_source_player)) &&
               (source_or_copy_effect->damage_target_card == copy_source_card)) &&
              ((legacy_type_match = is_what(p, card_00, 0x80),
                legacy_type_match != 0 &&
                    (source_or_copy_effect->info_slot_fn == effect_volrath_the_shapestealer))))
          {
            kill_card(p, card_00, KILL_EXILE);
          }
          card_00 = card_00 + -1;
        } while (card_00 != -1);
      }
      p = p + 1;
    } while (p != 2);
    set_legacy_image(copy_source_player, 0x5018,
                     create_targetted_legacy_activate(copy_source_player, copy_source_card,
                                                      effect_volrath_the_shapestealer,
                                                      copy_source_player, copy_source_card));
    cloning_and_verify_legend(copy_source_player, copy_source_card, instance->targets[0].player,
                              instance->targets[0].card);
  }
  return generic_activated_ability(player, card, event, 6, 1, 0, 0, 0, 0, 0, 0, &td_copy,
                                   "Select target creature with a counter on it");
}

/* ManalinkEh 2022: 0x23b1f80; original cards/commander_2019.c:1176. */
static int effect_volrath_the_shapestealer(int player, int card, event_t event)
{
  int result;

  result = volrath_the_shapestealer_impl(player, card, event, 1);
  return result;
}

/* ManalinkEh 2022: 0x23b4000; original cards/commander_2019.c:1179. */
int card_volrath_the_shapestealer(int player, int card, event_t event)
{
  /* Volrath, the Shapestealer	|2|B|G|U
   * Legendary Creature - Phyrexian Shapeshifter 7/5
   * At the beginning of combat on your turn, put a -1/-1 counter on up to one target creature.
   * |1: Until your next turn, Volrath becomes a copy of target creature with a counter on it, except it's 7/5 and it has this ability. */

  int result;

  result = volrath_the_shapestealer_impl(player, card, event, 0);
  return result;
}
