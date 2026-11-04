#include "manalink.h"

static int verdant_succession_effect(int player, int card, event_t event);

/* ManalinkEh 2022: 0x250a4a0; original cards/odyssey.c:2626. */
int card_fervent_denial(int player, int card, event_t event)
{
  /* Fervent Denial	|3|U|U
   * Instant
   * Counter target spell.
   * Flashback |5|U|U (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

  int result;
  card_instance_t *instance;
  if ((event == EVENT_GRAVEYARD_ABILITY) || (event == EVENT_PAY_FLASHBACK_COSTS))
  {
    result = do_flashback(player, card, event, 5, 0, 2, 0, 0, 0);
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
    else
    {
      if (event == EVENT_RESOLVE_SPELL)
      {
        instance = get_card_instance(player, card);
        result = counterspell_validate(player, card, NULL, 0);
        if (result != 0)
        {
          real_counter_a_spell(player, card, instance->targets[0].player,
                               instance->targets[0].card);
        }
        instance->number_of_targets = '\0';
        result = get_flashback(player, card);
        if (result != 0)
        {
          kill_card(player, card, KILL_EXILE);
          return 0;
        }
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
      if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
      {
        return 0;
      }
    }
  }
  result = counterspell(player, card, event, NULL, 0);
  return result;
}

/* ManalinkEh 2022: 0x250a820; original cards/odyssey.c:2687. */
int card_flame_burst(int player, int card, event_t event)
{
  /* Flame Burst	|1|R	0x200349b
   * Instant
   * ~ deals X damage to any target, where X is 2 plus the number of cards named ~ in all graveyards. */

  int result_or_flame_burst_count;
  int firecat_count;
  target_definition_t td;
  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 0x102);
  td.zone = 0x1200;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result_or_flame_burst_count = valid_target(&td);
    if (result_or_flame_burst_count != 0)
    {
      result_or_flame_burst_count = get_id(player, card);
      result_or_flame_burst_count = count_graveyard_by_id(2, result_or_flame_burst_count);
      firecat_count = count_graveyard_by_id(2, CARD_ID_PARDIC_FIRECAT);
      damage_target0(player, card, result_or_flame_burst_count + 2 + firecat_count);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result_or_flame_burst_count = generic_spell(player, card, event, 1, &td, "TARGET_ANY", 1, NULL);
  return result_or_flame_burst_count;
}

/* ManalinkEh 2022: 0x2510090; original cards/odyssey.c:4402. */
int card_predict(int player, int card, event_t event)
{
  /* Predict	|1|U
   * Instant
   * Choose a card name, then target player mills a card. If a card with the chosen name was milled this way, you draw two cards. Otherwise, you draw a card. */

  int t_player;
  int top_iid;
  int name_or_result;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t test;
  default_target_definition(player, card, &td, 0);
  td.zone = 0x1000;
  if (event == EVENT_RESOLVE_SPELL)
  {
    name_or_result = valid_target(&td);
    if (name_or_result != 0)
    {
      new_default_test_definition(&test, 0xff, "Choose a card name.");
      if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
      {
        name_or_result = ai_name_a_random_csvid(player, card, 1 - player, TYPE_NONE, &test);
      }
      else
      {
        name_or_result = new_card_from_list(player, 3, &test);
      }
      if (-1 < name_or_result)
      {
        instance = get_card_instance(player, card);
        t_player = instance->targets[0].player;
        top_iid = *deck_ptr[t_player];
        if (top_iid != -1)
        {
          mill(t_player, 1);
          name_or_result =
              has_same_name(-1, name_or_result, -1, (unsigned int)cards_data[top_iid].id);
          if (name_or_result == 0)
          {
            draw_a_card(player);
          }
          else
          {
            draw_cards(player, 2);
          }
        }
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  name_or_result = generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
  return name_or_result;
}

/* ManalinkEh 2022: 0x2510270; original cards/odyssey.c:4444. */
int card_price_of_glory(int player, int card, event_t event)
{
  /* Price of Glory	|2|R
   * Enchantment
   * Whenever a player taps a land for mana, if it's not that player's turn, destroy that land. */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if ((((event == EVENT_TAP_CARD) && (instance != NULL)) && (-1 < tapped_for_mana_color)) &&
      (current_turn != affected_card_controller))
  {
    result = is_what(affected_card_controller, affected_card, 1);
    if (result != 0)
    {
      result = is_humiliated(player, card, AL_TRIG);
      if (result == 0)
      {
        kill_card(affected_card_controller, affected_card, KILL_DESTROY);
      }
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2511ec0; original cards/odyssey.c:4960. */
int card_seize_the_day(int player, int card, event_t event)
{
  /* Seize the Day	|3|R
   * Sorcery
   * Untap target creature. After this main phase, there is an additional combat phase followed by an additional main phase.
   * Flashback |2|R (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event != EVENT_CAN_CAST && event != EVENT_CAST_SPELL && event != EVENT_RESOLVE_SPELL &&
      event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL && (card != affected_card || player != affected_card_controller))
    return 0;
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = player;
  if (event == EVENT_CAN_CAST ||
      (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller))
    return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    result = instance->targets[0].card;
    instance = get_card_instance(player, card);
    untap_card(instance->targets[0].player, result);
    extra_combat_effect(player, card, EVENT_RESOLVE_SPELL, 0, NULL, 1);
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
  result = do_flashback(player, card, event, 2, 0, 0, 0, 1, 0);
  return result;
}

/* ManalinkEh 2022: 0x2512e50; original cards/odyssey.c:5362. */
int card_spellbane_centaur(int player, int card, event_t event)
{
  /* Spellbane Centaur	|2|G
   * Creature - Centaur 3/2
   * Creatures you control can't be the targets of blue spells or abilities from blue sources. */

  card_instance_t *instance;
  int result;
  unsigned int attacker_colors;
  unsigned int protected_color;

  if (event == EVENT_ABILITIES)
  {
    result = is_what(affected_card_controller, affected_card, 2);
    if ((result != 0) && (affected_card_controller == player))
    {
      special_abilities2(player, affected_card, EVENT_ABILITIES, SP_KEYWORD2_GRANTED_PROT_MISC);
    }
  }
  else if (event == EVENT_PROT_MISC)
  {
    instance = get_card_instance(affected_card_controller, affected_card);
    if (instance != NULL)
    {
      result = is_what(affected_card_controller, affected_card, 2);
      if (((result != 0) && (affected_card_controller == player)) &&
          (((unsigned int)event_result >> 24 & 0xff) == '\x05'))
      {
        attacker_colors = get_color(attacking_card_controller, attacking_card);
        protected_color = get_sleighted_color_test(player, card, 4);
        if ((attacker_colors & protected_color) != 0)
        {
          event_result = event_result | 1;
        }
      }
    }
  }
  return 0;
}

int card_verdant_succession(int player, int card, event_t event)
{
  /* CARD_ID_VERDANT_SUCCESSION	5747
   * Verdant Succession	|4|G
   * Enchantment
   * Whenever a |Sgreen nontoken creature dies, that creature's controller may search his or her library for a card with the same name as that creature
   * and put it onto the battlefield. If that player does, he or she shuffles his or her library. */

  int stored_count_or_result;
  card_instance_t *instance;
  int stored_name;
  target_t *stored_target;
  int (*stored_entry)[2];
  int tutor_count;
  int added[10][2];
  char msg[100];
  test_definition_t this_test;
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      bool legacy_found = false;
      stored_count_or_result = 0;
      if (0 < active_cards_count[player])
      {
        do
        {
          instance = in_play(player, stored_count_or_result);
          if ((((instance != NULL) &&
                (stored_name = is_what(player, stored_count_or_result, 0x80), stored_name != 0)) &&
               (instance = get_card_instance(player, stored_count_or_result),
                instance->info_slot_fn == verdant_succession_effect)) &&
              (((instance->damage_target_player == player &&
                 (instance->damage_target_card == card)) ||
                ((instance->targets[0].player == player && (instance->targets[0].card == card))))))
          {
            legacy_found = true;
            break;
          }
          stored_count_or_result = stored_count_or_result + 1;
        } while (stored_count_or_result < active_cards_count[player]);
      }
      if (!legacy_found)
      {
        stored_count_or_result =
            create_targetted_legacy_effect(player, card, verdant_succession_effect, player, card);
        instance = get_card_instance(player, stored_count_or_result);
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        add_status(player, stored_count_or_result, 0x10000);
      }
    }
  }
  stored_count_or_result = is_humiliated(player, card, AL_TRIG);
  if (stored_count_or_result == 0)
  {
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {
      default_test_definition(&this_test, 2);
      this_test.color = get_sleighted_color_test(player, card, 8);
      count_for_gfp_ability_and_store_values(player, card, 0x77, 2, 0, &this_test, 0x10, 0);
    }
    stored_count_or_result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (stored_count_or_result != 0)
    {
      stored_count_or_result = 0;
      instance = get_card_instance(player, card);
      stored_target = instance->targets;
      do
      {
        if (stored_target->player != -1)
        {
          added[stored_count_or_result][0] = stored_target->player;
          stored_name = get_card_name(-1, stored_target->card);
          stored_target->card = -1;
          added[stored_count_or_result][1] = stored_name;
          stored_target->player = -1;
          stored_count_or_result = stored_count_or_result + 1;
        }
        stored_target = stored_target + 1;
      } while (stored_target != instance->targets + 10);
      if (0 < stored_count_or_result)
      {
        stored_entry = added;
        do
        {
          stored_name = (*stored_entry)[1];
          tutor_count = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
          do
          {
            if (stored_name == -1)
            {
              scnprintf(msg, 100,
                        "You won\'t be able to tutor any card as you\'re searching for a card with "
                        "no name.");
            }
            else
            {
              scnprintf(msg, 100, "Select a card named %s", cards_ptr[stored_name]->name);
            }
            new_default_test_definition(&this_test, 0xff, msg);
            this_test.id = stored_name;
            new_global_tutor((*stored_entry)[0], (*stored_entry)[0], 1, 4, 0, 5, &this_test);
            tutor_count = tutor_count + -1;
          } while (tutor_count != 0);
          stored_entry = stored_entry + 1;
        } while (stored_entry != added + stored_count_or_result);
      }
    }
  }
  stored_count_or_result = global_enchantment(player, card, event);
  return stored_count_or_result;
}

/* ManalinkEh 2022: 0x2501eb0; original cards/odyssey.c:5852. */
static int verdant_succession_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  int stored_count_or_result;
  card_instance_t *dying_permanent;
  int stored_name;
  target_t *stored_target;
  int (*stored_entry)[2];
  int tutor_count;
  int added[9][2];
  char msg[100];
  test_definition_t this_test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      if ((-1 < instance->damage_target_player) &&
          (stored_count_or_result =
               get_id((int)instance->damage_target_player, instance->damage_target_card),
           stored_count_or_result != 0x1673))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  instance = get_card_instance(player, card);
  if ((-1 < instance->damage_target_player) &&
      (stored_count_or_result = effect_follows_control_of_attachment(player, card, event),
       stored_count_or_result != 0))
  {
    return 0;
  }
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    instance = get_card_instance(player, card);
    dying_permanent = in_play(affected_card_controller, affected_card);
    if ((dying_permanent != NULL) &&
        (stored_count_or_result = is_this_dying(affected_card_controller, affected_card),
         stored_count_or_result != 0))
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
      default_test_definition(&this_test, 2);
      this_test.color = get_sleighted_color_test(player, card, 8);
      count_for_gfp_ability_and_store_values(player, card, 0x77, 2, 0, &this_test, 0x10, 1);
    }
  }
  stored_count_or_result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (stored_count_or_result == 0 && event == EVENT_GRAVEYARD_FROM_PLAY)
    return 0;
  if (stored_count_or_result != 0)
  {
    stored_count_or_result = check_status(player, card, 0x10000);
    if (stored_count_or_result == 0)
    {

      instance = get_card_instance(player, card);
      stored_target = instance->targets + 1;
      stored_count_or_result = 0;
      do
      {
        if (stored_target->player != -1)
        {
          added[stored_count_or_result][0] = stored_target->player;
          stored_name = get_card_name(-1, stored_target->card);
          stored_target->card = -1;
          added[stored_count_or_result][1] = stored_name;
          stored_target->player = -1;
          stored_count_or_result = stored_count_or_result + 1;
        }
        stored_target = stored_target + 1;
      } while (stored_target != instance->targets + 10);

      if (0 < stored_count_or_result)
      {

        stored_entry = added;
        do
        {
          stored_name = (*stored_entry)[1];
          tutor_count = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
          do
          {
            if (stored_name == -1)
            {
              scnprintf(msg, 100,
                        "You won\'t be able to tutor any card as you\'re searching for a card with "
                        "no name.");
            }
            else
            {

              scnprintf(msg, 100, "Select a card named %s", cards_ptr[stored_name]->name);
            }
            new_default_test_definition(&this_test, 0xff, msg);
            this_test.id = stored_name;
            new_global_tutor((*stored_entry)[0], (*stored_entry)[0], 1, 4, 0, 5, &this_test);
            tutor_count = tutor_count + -1;
          } while (tutor_count != 0);
          stored_entry = stored_entry + 1;
        } while (stored_entry != added + stored_count_or_result);
      }
      kill_card(player, card, KILL_EXILE);
    }
  }
  if ((event == EVENT_CLEANUP) &&
      (stored_count_or_result = check_status(player, card, 0x10000), stored_count_or_result == 0))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}
