#include "manalink.h"

void unattach(int equipment_controller, int equipment_card)
{
  if (is_equipping(equipment_controller, equipment_card))
    call_card_function(equipment_controller, equipment_card, EVENT_UNATTACH);

  card_instance_t* instance = get_card_instance(equipment_controller, equipment_card);
  if (instance->damage_target_player >= 0)
    {
      card_instance_t* attached = in_play(instance->damage_target_player, instance->damage_target_card);
      if (attached)
        attached->regen_status |= KEYWORD_RECALC_ALL;
    }
  instance->damage_target_player = -1;
  instance->damage_target_card = -1;
}

int equipment_legacy(int player, int card, event_t event)
{
  if (event == EVENT_STATIC_EFFECTS)
    {
      card_instance_t* instance = get_card_instance(player, card);
      int crp = instance->damage_target_player;
      int crc = instance->damage_target_card;
      int eqp = instance->damage_source_player;
      int eqc = instance->damage_source_card;
      card_instance_t* equipment;
      if (crp < 0 || crc < 0 || eqp < 0 || eqc < 0 || !(equipment = in_play(eqp, eqc)))
        kill_card(player, card, KILL_EXILE);
      else if (!in_play(crp, crc))
        {
          kill_card(player, card, KILL_EXILE);
          if (equipment->damage_target_player == crp && equipment->damage_target_card == crc)
            unattach(eqp, eqc);
        }
      else if (equipment->damage_target_player != crp || equipment->damage_target_card != crc)
        kill_card(player, card, KILL_EXILE);
      else if (!is_what(crp, crc, TYPE_CREATURE) || is_what(eqp, eqc, TYPE_CREATURE)
               || is_protected_from(crp, crc, eqp, eqc, IPF_ATTACH_EQUIPMENT))
        {
          kill_card(player, card, KILL_EXILE);
          unattach(eqp, eqc);
        }
    }
  return 0;
}

/* Called from the exe to determine whether a non-effect card_instance_t with damage_target_player and damage_target_card set should be visually attached to its
 * damage_target.
 *
 * Like with ability icons, it's important that any functions called from within this always work only with displayed_card_instances, and never with anything
 * returned from get_card_instance(), or it'll randomly crash.
 *
 * Being stdcall saves three bytes at the call site; a normal (cdecl) function would be one byte too long to fit into the third injection. */
int __stdcall is_displayed_as_attached(int player, int card)
{
  uint16_t displayed_subtype;
  card_instance_t *instance;
  int result;

  instance = get_displayed_card_instance(player, card);
  if (((((cards_data[instance->internal_card_id].type & 4) != 0) &&
        (displayed_subtype = cards_ptr[cards_data[instance->internal_card_id].id]->subtype1,
         displayed_subtype != 0xcd)) &&
       (displayed_subtype != 0xd3)) ||
      (result = has_subtype(player, card, SUBTYPE_AURA), result != 0))
  {
    return 1;
  }
  result = get_setting(0x10);
  if (result == 0)
  {
    return 0;
  }
  result = has_subtype(player, card, SUBTYPE_EQUIPMENT);
  if (result == 0)
  {
    return 0;
  }
  if (instance->damage_target_player < '\0')
  {
    return 0;
  }
  if (instance->damage_target_card < 0)
  {
    return 0;
  }
  instance = get_displayed_card_instance((int)instance->damage_target_player,
                                         instance->damage_target_card);
  return (unsigned int)((instance->state & 0x22) == 2);
}

void equip_target_creature(int equipment_controller, int equipment_card, int t_player, int t_card)
{
  unattach(equipment_controller, equipment_card);
  card_instance_t* instance = get_card_instance(equipment_controller, equipment_card);
  instance->damage_target_player = t_player;
  instance->damage_target_card = t_card;
  int leg = create_targetted_legacy_effect(equipment_controller, equipment_card, &equipment_legacy, t_player, t_card);
  if (get_setting(SETTING_ATTACH_EQUIPMENT))
    get_card_instance(equipment_controller, leg)->token_status |= STATUS_INVISIBLE_FX;
  play_sound_effect(WAV_EQUIP);
  call_card_function(equipment_controller, equipment_card, EVENT_EQUIPMENT_ATTACHED);
}

card_instance_t* is_equipping(int player, int card)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0 && instance->damage_target_card >= 0)
    {
      if (in_play(instance->damage_target_player, instance->damage_target_card))
        return instance;
      if (get_card_instance(instance->damage_target_player, instance->damage_target_card)->internal_card_id == -1)
        {
          instance->damage_target_card = -1;
          instance->damage_target_player = -1;
        }
    }
  return NULL;
}

int check_for_equipment_targets(int player, int card)
{
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.flags |= 5;
  td.allowed_controller = player;
  td.preferred_controller = player;
  return can_target(&td);
}

int check_for_equipment_targets_custom(int player, int card, target_definition_t* td)
{
  td->flags |= 5;
  return can_target(td);
}

int get_updated_equip_cost(int player, int card, int equip_cost)
{
  int new_cost = get_cost_mod_for_activated_abilities(player, card, equip_cost, 0, 0, 0, 0, 0);
  int art_count = 0;
  int puresteel = 0;
  for (int count = 0; count < active_cards_count[player]; ++count)
    {
      if (in_play(player, count))
        {
          if (is_what(player, count, TYPE_ARTIFACT))
            ++art_count;
          if ((get_id(player, count) == CARD_ID_AURIOK_STEELSHAPER
               || (get_id(player, count) == CARD_ID_NAHIRI_STORM_OF_STONE && current_turn == player))
              && !is_humiliated(player, count, AL_STATIC))
            --new_cost;
          if (get_id(player, count) == CARD_ID_PURESTEEL_PALADIN && !is_humiliated(player, count, AL_STATIC))
            puresteel = 1;
        }
      if (new_cost < 1)
        break;
      if (puresteel && art_count > 2)
        {
          new_cost = 0;
          break;
        }
    }
  return new_cost;
}

int can_activate_basic_equipment(int player, int card, event_t event, int equip_cost)
{
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = player;
  td.preferred_controller = player;
  int cl[6] = {0};
  cl[((unsigned int)equip_cost >> 8) & 0xff] = ((unsigned int)equip_cost >> 16) & 0xff;
  if (can_sorcery_be_played(player, event) && !is_what(player, card, TYPE_CREATURE)
      && can_use_activated_abilities(player, card))
    {
      int new_cost = get_updated_equip_cost(player, card, equip_cost & 0xff);
      if (has_mana_for_activated_ability(player, card, new_cost, cl[1], cl[2], cl[3], cl[4], cl[5]))
        {
          td.flags |= 5;
          return can_target(&td);
        }
    }
  return 0;
}

int activate_basic_equipment(int player, int card, int equip_cost)
{
  card_instance_t* instance = get_card_instance(player, card);
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.flags = 5;
  td.allow_cancel = 0;
  td.allowed_controller = player;
  td.preferred_controller = player;
  instance->number_of_targets = 0;
  if (equip_cost >= -1)
    {
      int cl[6] = {0};
      cl[((unsigned int)equip_cost >> 8) & 0xff] = ((unsigned int)equip_cost >> 16) & 0xff;
      int new_cost = get_updated_equip_cost(player, card, equip_cost & 0xff);
      charge_mana_multi(player, new_cost, cl[1], cl[2], cl[3], cl[4], cl[5]);
    }
  if (spell_fizzled == 1 || !can_target(&td)
      || !select_target(player, card, &td, "Select creature to equip", NULL))
    return 0;
  if (is_equipping(player, card) && instance->targets[0].player == instance->damage_target_player
      && instance->targets[0].card == instance->damage_target_card)
    ai_modifier -= 96;
  return 1;
}

int resolve_activation_equipment(int player, int card, target_definition_t* td)
{
  if (valid_target(td))
    {
      card_instance_t* instance = get_card_instance(player, card);
      equip_target_creature(instance->parent_controller, instance->parent_card,
                            instance->targets[0].player, instance->targets[0].card);
    }
  return 0;
}

int resolve_activation_basic_equipment(int player, int card)
{
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.flags = 5;
  return resolve_activation_equipment(player, card, &td);
}

int basic_equipment(int player, int card, event_t event, int equip_cost)
{
  if (event == EVENT_CHANGE_TYPE)
    {
      if (!has_subtype(player, card, SUBTYPE_EQUIPMENT) || !is_what(player, card, TYPE_ARTIFACT)
          || is_what(player, card, TYPE_CREATURE))
        unattach(player, card);
    }
  else if (IS_GAA_EVENT(event))
    {
      target_definition_t td;
      default_target_definition(player, card, &td, TYPE_CREATURE);
      int cl[6] = {0};
      td.allowed_controller = player;
      td.preferred_controller = player;
      td.flags = 5;
      cl[((unsigned int)equip_cost >> 8) & 0xff] = ((unsigned int)equip_cost >> 16) & 0xff;
      if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
        {
          card_instance_t* instance = get_card_instance(player, card);
          equip_target_creature(instance->parent_controller, instance->parent_card,
                                instance->targets[0].player, instance->targets[0].card);
        }
      return generic_activated_ability(player, card, event, GAA_EQUIPMENT | GAA_LITERAL_PROMPT,
                                       equip_cost & 0xff, cl[1], cl[2], cl[3], cl[4], cl[5], 0, &td,
                                       "Select target creature you control.");
    }
  return 0;
}

int living_weapon(int player, int card, event_t event, int equip_cost)
{
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
    {
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_GERM, &token);
      token.action = TOKEN_ACTION_EQUIP;
      token.no_sleight = 1;
      generate_token(&token);
    }
  return basic_equipment(player, card, event, equip_cost);
}

int vanilla_equipment(int player, int card, event_t event, int equip_cost, int p_plus, int t_plus, int k_plus, int k_special)
{
  card_instance_t* instance = in_play(player, card);
  if (instance && is_equipping(player, card))
    {
      if (p_plus || t_plus || k_plus)
        modify_pt_and_abilities(instance->damage_target_player, instance->damage_target_card, event, p_plus, t_plus, k_plus);
      if (k_special > 0)
        special_abilities(instance->damage_target_player, instance->damage_target_card, event, k_special, player, card);
    }
  return basic_equipment(player, card, event, equip_cost);
}

int equipments_attached_to_me(int player, int card, int mode)
{
  int result = 0;
  for (int p = 0; p < 2; ++p)
    for (int c = active_cards_count[p] - 1; c >= 0; --c)
      if (in_play(p, c) && is_what(p, c, TYPE_ARTIFACT) && has_subtype(p, c, SUBTYPE_EQUIPMENT))
        {
          card_instance_t* instance = get_card_instance(p, c);
          if (instance->damage_target_player == player && instance->damage_target_card == card)
            {
              if (mode == EATM_CHECK)
                return 1;
              if (mode & EATM_REPORT_TOTAL)
                ++result;
              if (mode & EATM_UNATTACH)
                unattach(p, c);
              if (mode & EATM_DESTROY)
                kill_card(p, c, KILL_DESTROY);
            }
        }
  return result;
}

int is_equipment(int player, int card ){
	return has_subtype(player, card, SUBTYPE_EQUIPMENT );
}

int altar_equipment(int player, int card, event_t event, int req_type, int p_plus, int t_plus, int k_plus)
{
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.allow_cancel = 0;
  td.flags = 5;
  card_instance_t* instance = get_card_instance(player, card);
  if (is_equipping(player, card))
    modify_pt_and_abilities(instance->damage_target_player, instance->damage_target_card, event, p_plus, t_plus, k_plus);
  if (event == EVENT_CAN_ACTIVATE)
    {
      if (can_use_activated_abilities(player, card) && can_sorcery_be_played(player, event)
          && (can_sacrifice_as_cost(player, 1, req_type, 0, 0, 0, 0, 0, 0, 0, -1, 0)
              || (metalcraft(player, card) && check_battlefield_for_id(player, CARD_ID_PURESTEEL_PALADIN))))
        return check_for_equipment_targets(player, card);
    }
  else if (event == EVENT_ACTIVATE)
    {
      int choice = 1;
      if (can_target(&td))
        {
          choice = 0;
          if (metalcraft(player, card) && check_battlefield_for_id(player, CARD_ID_PURESTEEL_PALADIN))
            choice = do_dialog(player, player, card, -1, -1, " Sac & equip\n Equip for 0\n Do nothing", 1);
        }
      instance->number_of_targets = 0;
      if (choice == 0)
        {
          if (!controller_sacrifices_a_permanent(player, card, req_type, 0))
            {
              spell_fizzled = 1;
              return 0;
            }
        }
      else if (choice != 1)
        {
          spell_fizzled = 1;
          return 0;
        }
      if (can_target(&td) && select_target(player, card, &td, "Select creature to equip", NULL)
          && is_equipping(player, card) && instance->targets[0].player == instance->damage_target_player
          && instance->targets[0].card == instance->damage_target_card)
        ai_modifier -= 96;
    }
  else if (event == EVENT_RESOLVE_ACTIVATION && validate_target(player, card, &td, 0))
    equip_target_creature(instance->parent_controller, instance->parent_card,
                          instance->targets[0].player, instance->targets[0].card);
  return 0;
}

int is_equipped(int player, int card){
	return equipments_attached_to_me(player, card, EATM_CHECK);
}

#ifndef SHANDALAR
/* Recovered October 2022 inline helpers; existing native callers contain the expansions. */
static int check_for_equipment_targets_impl(int player, int card, target_definition_t *td)
{
  td->flags |= 5;
  return can_target(td);
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline card mechanics; original native callers contain the expansions. */
static int can_activate_basic_equipment_impl(int player, int card, event_t event,
                                              int equip_cost, target_definition_t *td)
{
  int cl[6] = {0};
  cl[((unsigned int)equip_cost >> 8) & 0xff] = ((unsigned int)equip_cost >> 16) & 0xff;
  if (can_sorcery_be_played(player, event) && !is_what(player, card, TYPE_CREATURE) &&
      can_use_activated_abilities(player, card))
  {
    int cost = get_updated_equip_cost(player, card, equip_cost & 0xff);
    if (has_mana_for_activated_ability(player, card, cost, cl[1], cl[2], cl[3], cl[4], cl[5]))
      return check_for_equipment_targets_impl(player, card, td);
  }
  return 0;
}
#endif
