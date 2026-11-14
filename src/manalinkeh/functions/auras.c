#include "manalink.h"

/* Canonical recovered helper declarations. */
void put_into_play_aura_attached_to_target(int aura_player, int aura_card, int t_player,
                                                int t_card);
int attach_aura_to_target(int aura_player, int aura_card, event_t event, int t_player,
                               int t_card);
int aura_attached_to_me(int player, int card, event_t event, resolve_trigger_t trigger_mode,
                             test_definition_t *test2);
int vanilla_aura(int player, int card, event_t event, int pref_controller);
int disabling_aura(int player, int card, int event);
int aura_with_variable_pt_boost_depending_on_condition(int player, int card, event_t event,
                                                            target_definition_t *td,
                                                            test_definition_t *this_test,
                                                            int pow, int tou, int pow2, int tou2);
int gain_control_and_attach_as_aura(int player, int card, event_t event, int t_player,
                                         int t_card);
void end_of_stealing_aura_effect(int player, int card);
static int when_enchanted_permanent_dies_return_aura_to_hand_legacy(int player, int card,
                                                                         event_t event);
void when_enchanted_permanent_dies_return_aura_to_hand(int player, int card, event_t event);
void cannot_be_enchanted_granted(int granting_player, int granting_card, event_t event,
                                      int t_player, int t_card);
void cannot_be_enchanted(int player, int card, event_t event);
int generic_aura(int player, int card, event_t event, int pref_controller, int p_plus,
                      int t_plus, int k_plus, int k_special, int subtype, int clr, int flags);
static int targeted_aura_impl(int player, int card, event_t event,
                                   target_definition_t *td, const char *prompt, int custom_prompt,
                                   int flags);
int targeted_aura(int player, int card, event_t event, target_definition_t *td,
                       const char *prompt);
int targeted_aura_custom_prompt(int player, int card, event_t event,
                                     target_definition_t *td, const char *prompt);
int disabling_targeted_aura(int player, int card, event_t event, target_definition_t *td,
                                 const char *prompt, int mode);
int generic_stealing_aura(int player, int card, event_t event, target_definition_t *td,
                               const char *prompt);
static int generic_animating_aura_impl(int player, int card, event_t event,
                                            target_definition_t *td, const char *prompt, int pow,
                                            int tou, int key, int s_key, int custom_prompt,
                                            subtype_t added_subtype);
int generic_animating_aura(int player, int card, event_t event, target_definition_t *td,
                                const char *prompt, int pow, int tou, int key, int s_key);
int generic_animating_aura_custom_prompt(int player, int card, event_t event,
                                              target_definition_t *td, const char *prompt, int pow,
                                              int tou, int key, int s_key);
int generic_animating_aura_with_added_subtype(int player, int card, event_t event,
                                                   target_definition_t *td, const char *prompt,
                                                   int pow, int tou, int key, int s_key,
                                                   subtype_t subtype);

// An aura that does absolutely nothing itself.

// An aura with enchant creature that does absolutely nothing itself.

// A frontend to vanilla_aura() with AI settings for Pacifism-like auras.

void attached_gets_pt(int player, int card, event_t event, int power, int toughness);

static void attached_gets_pt_part(int player, int card, event_t event, int power,
                                       int toughness);

static void attached_set_pt_part(int player, int card, event_t event, int power, int toughness);

static void disabling_aura_ai_part(int player, int card, event_t event);

void attached_is_additional_subtype(int player, int card, event_t event, subtype_t subtype);
void attached_is_forced_subtype(int player, int card, event_t event, subtype_t subtype);
void attached_is_additional_color(int player, int card, event_t event, color_test_t color);

void attached_gets_kw(int player, int card, event_t event, keyword_t keyword,
                      sp_keyword_t sp_keyword);

void attached_gets_sp_kw2(int player, int card, event_t event, sp_keyword2_t sp_keyword2);

#ifndef SHANDALAR

static int aura_impl(int player, int card, event_t event, target_definition_t *td,
                     const char *prompt, int literal_prompt);
int aura(int player, int card, event_t event, target_definition_t *td, char *prompt);
int aura_pref_controller(int player, int card, event_t event, int pref_controller, char *prompt);

#endif

#ifndef SHANDALAR

static int targeted_aura_impl(int player, int card, event_t event,
                                   target_definition_t *td, const char *prompt, int custom_prompt,
                                   int flags);
static int when_enchanted_permanent_dies_return_aura_to_hand_legacy(int player, int card,
                                                                         event_t event);

#endif

#ifndef SHANDALAR

static int generic_animating_aura_impl(int player, int card, event_t event,
                                            target_definition_t *td, const char *prompt, int pow,
                                            int tou, int key, int s_key, int custom_prompt,
                                            subtype_t added_subtype);

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e6850; original functions/auras.c:3. */
int attach_aura_to_target(int aura_player, int aura_card, event_t event, int t_player,
                               int t_card)
{
  int result;
  card_instance_t *instance;
  result = check_for_guardian_beast_protection(t_player, t_card);
  if (result != 0)
  {
    return 0;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    set_special_flags(aura_player, aura_card, 0x2000000);
  }
  instance = get_card_instance(aura_player, aura_card);
  instance->damage_target_player = (int8_t)t_player;
  instance->damage_target_card = t_card;
  return 1;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e7730; original functions/auras.c:16. */
int aura_attached_to_me(int player, int card, event_t event, resolve_trigger_t trigger_mode,
                             test_definition_t *test2)
{
  if (trigger_condition != TRIGGER_COMES_INTO_PLAY || card != affected_card ||
      player != affected_card_controller || reason_for_trigger_controller != player ||
      is_humiliated(player, card, AL_TRIG))
    return 0;
  test_definition_t this_test;
  default_test_definition(&this_test, TYPE_ENCHANTMENT);
  this_test.subtype = 0x4000;
  if (!new_make_test_in_play(trigger_cause_controller, trigger_cause, -1, &this_test))
    return 0;
  card_instance_t *aura = get_card_instance(trigger_cause_controller, trigger_cause);
  if (aura->damage_target_player != player || aura->damage_target_card != card)
    return 0;
  if (test2 && !new_make_test_in_play(trigger_cause_controller, trigger_cause, -1, test2))
    return 0;
  if (event == EVENT_TRIGGER)
  {
    event_result |= trigger_mode;
    return 0;
  }
  return event == EVENT_RESOLVE_TRIGGER;
}
#endif

/* ManalinkEh 2022: 0x26e7870; original functions/auras.c:41. */
void attached_gets_pt(int player, int card, event_t event, int power, int toughness)
{
  if (1 < (uint32_t)(event - EVENT_POWER))
  {
    return;
  }
  attached_gets_pt_part(player, card, event, power, toughness);
  return;
}

/* ManalinkEh 2022: 0x26e65a0; original functions/auras.c:41. */
static void attached_gets_pt_part(int player, int card, event_t event, int power,
                                       int toughness)
{
  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if ((instance != NULL) && (-1 < instance->damage_target_card))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      if (event == EVENT_POWER)
      {
        if ((instance->damage_target_card == affected_card) &&
            (instance->damage_target_player == affected_card_controller))
        {
          event_result = event_result + power;
        }
      }
      else if (((event == EVENT_TOUGHNESS) && (instance->damage_target_card == affected_card)) &&
               (instance->damage_target_player == affected_card_controller))
      {
        event_result = event_result + toughness;
      }
    }
  }
  return;
}

/* ManalinkEh 2022: 0x26e78b0; original functions/auras.c:62. */
void attached_set_pt(int player, int card, event_t event, int power, int toughness)
{
  if (1 < (uint32_t)(event - EVENT_POWER))
  {
    return;
  }
  attached_set_pt_part(player, card, event, power, toughness);
  return;
}

/* ManalinkEh 2022: 0x26e6650; original functions/auras.c:62. */
static void attached_set_pt_part(int player, int card, event_t event, int power, int toughness)
{
  card_instance_t *instance = in_play(player, card);
  if (instance && instance->damage_target_card >= 0 && !is_humiliated(player, card, AL_STATIC) &&
      instance->damage_target_card == affected_card &&
      instance->damage_target_player == affected_card_controller)
  {
    if (event == EVENT_POWER && power >= 0)
      set_base_pt(power);
    else if (event == EVENT_TOUGHNESS && toughness >= 0)
      set_base_pt(toughness);
  }
}

/* ManalinkEh 2022: 0x26e78f0; original functions/auras.c:82. */
void attached_gets_kw(int player, int card, event_t event, keyword_t keyword,
                      sp_keyword_t sp_keyword)
{
  card_instance_t *instance;
  int result;

  if (event == EVENT_ABILITIES)
  {

    instance = in_play(player, card);
    if (instance == NULL)
    {
      return;
    }
    if (instance->damage_target_card < 0)
    {
      return;
    }
    result = is_humiliated(player, card, AL_STATIC);
    if (result != 0)
    {
      return;
    }
    if ((instance->damage_target_card == affected_card) &&
        (instance->damage_target_player == affected_card_controller))
    {
      get_sleighted_hacked_suppressed_ability(player, card, (int)instance->damage_target_player,
                                              &keyword, &sp_keyword);
      event_result = event_result | keyword;
    }
  }

  if ((((sp_keyword != SP_KEYWORD_0) && (instance = in_play(player, card), instance != NULL)) &&
       (-1 < instance->damage_target_card)) &&
      (result = is_humiliated(player, card, AL_STATIC), result == 0))
  {
    special_abilities((int)instance->damage_target_player, instance->damage_target_card, event,
                      sp_keyword, player, card);
  }
  return;
}

/* ManalinkEh 2022: 0x26e7a30; original functions/auras.c:109. */
void attached_gets_sp_kw2(int player, int card, event_t event, sp_keyword2_t sp_keyword2)
{
  card_instance_t *instance;
  int result;

  if (event == EVENT_ABILITIES)
  {

    instance = in_play(player, card);
    if ((instance != NULL) && (-1 < instance->damage_target_card))
    {
      result = is_humiliated(player, card, AL_STATIC);

      if ((sp_keyword2 != SP_KEYWORD2_0) && ((result == 0 && (-1 < instance->damage_target_card))))
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {
          special_abilities2((int)instance->damage_target_player, instance->damage_target_card,
                             EVENT_ABILITIES, sp_keyword2);
          return;
        }
      }
    }
  }
  return;
}

/* ManalinkEh 2022: 0x26e7ad0; original functions/auras.c:129. */
void disable_attached(int player, int card, event_t event, int disabling_mode)
{
  if (event != EVENT_ABILITIES && event != EVENT_EQUIPMENT_ATTACHED &&
      (event != EVENT_RESOLVE_SPELL || !has_subtype(player, card, SUBTYPE_AURA)))
    return;
  card_instance_t *instance = in_play(player, card);
  if (instance && instance->damage_target_card >= 0 && !is_humiliated(player, card, AL_STATIC))
  {
    char enchanted_player = instance->damage_target_player;
    int enchanted_card = instance->damage_target_card;
    if ((event == EVENT_ABILITIES && enchanted_card == affected_card &&
         enchanted_player == affected_card_controller) ||
        (event == EVENT_RESOLVE_SPELL && has_subtype(player, card, SUBTYPE_AURA)) ||
        event == EVENT_EQUIPMENT_ATTACHED)
      remove_abilities(enchanted_player, enchanted_card, event, disabling_mode);
  }
}

/* ManalinkEh 2022: 0x26e7bf0; original functions/auras.c:144. */
void attached_is_additional_subtype(int player, int card, event_t event, subtype_t subtype)
{
  card_instance_t *instance;
  int result;
  int id;
  int target_player;

  if (event != EVENT_CHANGE_TYPE)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = has_subtype(player, card, SUBTYPE_AURA);
      if (result == 0)
      {
        return;
      }
    }
    else if (event != EVENT_EQUIPMENT_ATTACHED)
    {
      return;
    }
  }

  instance = in_play(player, card);
  if ((((instance != NULL) && (-1 < instance->damage_target_card)) &&
       (result = is_humiliated(player, card, AL_STATIC), subtype != SUBTYPE_INVALID)) &&
      (result == 0))
  {
    target_player = (int)instance->damage_target_player;
    result = instance->damage_target_card;
    add_a_subtype(target_player, result, subtype);
    if (subtype == SUBTYPE_LEGEND)
    {
      id = get_card_name(target_player, result);
      verify_legend_rule(target_player, result, id);
      return;
    }
  }
  return;
}

/* ManalinkEh 2022: 0x26e7cc0; original functions/auras.c:161. */
void attached_is_forced_subtype(int player, int card, event_t event, subtype_t subtype)
{
  card_instance_t *instance;
  int result;
  int id;
  int target_player;

  if (event != EVENT_CHANGE_TYPE)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = has_subtype(player, card, SUBTYPE_AURA);
      if (result == 0)
      {
        return;
      }
    }
    else if (event != EVENT_EQUIPMENT_ATTACHED)
    {
      return;
    }
  }

  instance = in_play(player, card);
  if ((((instance != NULL) && (-1 < instance->damage_target_card)) &&
       (result = is_humiliated(player, card, AL_STATIC), subtype != SUBTYPE_INVALID)) &&
      (result == 0))
  {
    target_player = (int)instance->damage_target_player;
    result = instance->damage_target_card;
    force_a_subtype(target_player, result, subtype);
    if (subtype == SUBTYPE_LEGEND)
    {
      id = get_card_name(target_player, result);
      verify_legend_rule(target_player, result, id);
      return;
    }
  }
  return;
}

/* ManalinkEh 2022: 0x26e7d90; original functions/auras.c:178. */
void attached_is_additional_color(int player, int card, event_t event, color_test_t color)
{
  card_instance_t *aura_instance;
  int result;
  card_instance_t *active_aura;
  unsigned int sleighted_color;

  if (event != EVENT_SET_COLOR)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = has_subtype(player, card, SUBTYPE_AURA);
      if (result == 0)
      {
        return;
      }
    }
    else if (event != EVENT_EQUIPMENT_ATTACHED)
    {
      return;
    }
  }

  aura_instance = in_play(player, card);
  if ((((aura_instance != NULL) && (-1 < aura_instance->damage_target_card)) &&
       (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
      ((active_aura = in_play(player, card), active_aura != NULL && (-1 < aura_instance->damage_target_card))))
  {

    if (event == EVENT_SET_COLOR)
    {
      if (aura_instance->damage_target_card != affected_card)
      {
        return;
      }
      if (aura_instance->damage_target_player != affected_card_controller)
      {
        return;
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      result = has_subtype(player, card, SUBTYPE_AURA);
      if (result == 0)
      {
        return;
      }
    }
    else if (event != EVENT_EQUIPMENT_ATTACHED)
    {
      return;
    }
    sleighted_color = get_sleighted_color_test(player, card, color);
    event_result = event_result | sleighted_color;
  }
  return;
}

/* ManalinkEh 2022: 0x26e7eb0; original functions/auras.c:194. */
void attached_is_forced_color(int player, int card, event_t event, color_test_t color)
{
  card_instance_t *aura_instance;
  int result;
  card_instance_t *active_aura;

  if (event != EVENT_SET_COLOR)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = has_subtype(player, card, SUBTYPE_AURA);
      if (result == 0)
      {
        return;
      }
    }
    else if (event != EVENT_EQUIPMENT_ATTACHED)
    {
      return;
    }
  }

  aura_instance = in_play(player, card);
  if ((((aura_instance != NULL) && (-1 < aura_instance->damage_target_card)) &&
       (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
      ((active_aura = in_play(player, card), active_aura != NULL && (-1 < aura_instance->damage_target_card))))
  {

    if (event == EVENT_SET_COLOR)
    {
      if (aura_instance->damage_target_card != affected_card)
      {
        return;
      }
      if (aura_instance->damage_target_player != affected_card_controller)
      {
        return;
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      result = has_subtype(player, card, SUBTYPE_AURA);
      if (result == 0)
      {
        return;
      }
    }
    else if (event != EVENT_EQUIPMENT_ATTACHED)
    {
      return;
    }
    event_result = get_sleighted_color_test(player, card, color);
  }
  return;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e7fd0; original functions/auras.c:213. */
int this_effect_doesnt_remove_this(int player, int card, event_t event)
{
  static target_t recalculating_ward = {-1, -1};
  int enchanted_card;
  card_instance_t *aura_instance;
  card_instance_t *enchanted_instance;
  int result;
  int enchanted_player;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    aura_instance = in_play(player, card);
    if ((aura_instance != NULL) && (aura_instance->damage_target_player != -1))
    {
      if (recalculating_ward.card != card)
      {
        enchanted_instance = in_play((int)aura_instance->damage_target_player, aura_instance->damage_target_card);
        if (enchanted_instance == NULL)
        {
          kill_card(player, card, KILL_STATE_BASED_ACTION);
          return 1;
        }
        recalculating_ward.player = player;
        enchanted_player = (int)aura_instance->damage_target_player;
        recalculating_ward.card = card;
        enchanted_card = aura_instance->damage_target_card;
        enchanted_instance->regen_status = enchanted_instance->regen_status | 0x8000000;
        get_abilities(enchanted_player, enchanted_card, EVENT_ABILITIES, -1);
        default_target_definition(player, card, &td, 2);
        td.flags = td.flags | 3;
        result = validate_arbitrary_target(&td, enchanted_player, enchanted_card);
        recalculating_ward.player = -1;
        recalculating_ward.card = -1;
        enchanted_instance->regen_status = enchanted_instance->regen_status | 0x8000000;
        if (result == 0)
        {
          kill_card(player, card, KILL_STATE_BASED_ACTION);
        }
        else
        {
          get_abilities(enchanted_player, enchanted_card, EVENT_ABILITIES, -1);
        }
      }
      return 1;
    }
  }
  else if ((event == EVENT_ABILITIES) && (recalculating_ward.player == player))
  {
    return (unsigned int)(recalculating_ward.card == card);
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e8180; original functions/auras.c:255. */
void pump_attached(int player, int card, event_t event, pump_ability_t *ability_mod)
{
  int power;
  color_test_t color;
  subtype_t subtype;
  card_instance_t *aura_instance;
  int result;
  card_instance_t *instance;
  unsigned int granted_keywords;
  pump_ability_t default_ability_mod;

  aura_instance = in_play(player, card);
  /* Direct entry resolves the Aura while it is still invisible to in_play(). */
  if (aura_instance == NULL)
  {
    return;
  }
  if ((((event == EVENT_RESOLVE_SPELL) &&
        (result = has_subtype(player, card, SUBTYPE_AURA), player == 1)) &&
       (result != 0)) &&
      ((((unsigned char)trace_mode & 2) == 0 &&
        (((0 < ability_mod->pow_mod || 0 < ability_mod->tou_mod ||
           (ability_mod->key_mod != KEYWORD_0)) ||
          ((ability_mod->skey_mod & ~SP_KEYWORD_CANNOT_BLOCK) != SP_KEYWORD_0))))))
  {
    instance = get_card_instance(aura_instance->targets[0].player, aura_instance->targets[0].card);
    granted_keywords = get_special_abilities_by_instance(instance);
    if ((((granted_keywords & 0x20) != 0) && ((instance->regen_status & 0x800000) != 0)) &&
        ((instance->token_status & 0x800) == 0))
    {
      ai_modifier = ai_modifier + -0x18;
    }
  }
  if (((-1 < aura_instance->damage_target_card) &&
       (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
      (result = is_what((int)aura_instance->damage_target_player, aura_instance->damage_target_card, 2),
       result != 0))
  {
    if (ability_mod == NULL)
    {
      ability_mod = &default_ability_mod;
      default_pump_ability_definition(player, card, ability_mod, 0, 0, 0, 0);
    }
    result = ability_mod->tou_mod;
    power = ability_mod->pow_mod;
    if (result != 0 || power != 0)
    {
      if ((ability_mod->paue_flags & 0x100000) == 0)
      {
        if ((uint32_t)event - (uint32_t)EVENT_POWER < 2U)
        {
          attached_gets_pt_part(player, card, event, power, result);
        }
      }
      else if ((uint32_t)event - (uint32_t)EVENT_POWER < 2U)
      {
        attached_set_pt_part(player, card, event, power, result);
      }
    }
    if (ability_mod->skey_mod != SP_KEYWORD_0 || ability_mod->key_mod != KEYWORD_0)
    {
      attached_gets_kw(player, card, event, ability_mod->key_mod, ability_mod->skey_mod);
    }
    if (ability_mod->disabling_mode != 0)
    {
      disable_attached(player, card, event, ability_mod->disabling_mode);
    }
    color = ability_mod->color;
    if (color != COLOR_TEST_0)
    {
      if ((ability_mod->paue_flags & 0x20000) == 0)
      {
        attached_is_additional_color(player, card, event, color);
      }
      else
      {
        attached_is_forced_color(player, card, event, color);
      }
    }
    subtype = ability_mod->subtype;
    if (subtype != SUBTYPE_INVALID)
    {
      if ((ability_mod->paue_flags & 0x40000U) == 0)
      {
        if ((ability_mod->paue_flags & 0x80000U) != 0)
        {
          attached_is_additional_subtype(player, card, event, subtype);
        }
      }
      else
      {
        attached_is_forced_subtype(player, card, event, subtype);
      }
    }
    if (ability_mod->can_block_additional_creatures != 0)
    {
      event_flags = event_flags | 0x100000;
      attached_creature_can_block_additional(player, card, event,
                                             ability_mod->can_block_additional_creatures);
    }
  }
  return;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e68d0; original functions/auras.c:331. */
static int aura_impl(int player, int card, event_t event, target_definition_t *td,
                     const char *prompt, int literal_prompt)
{
  if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA &&
      (uint32_t)(uint32_t)(event - EVENT_POWER) > 2 && event != EVENT_RESOLVE_MOVING_AURA &&
      event != EVENT_CHANGE_TYPE && event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD &&
      event != EVENT_STATIC_EFFECTS)
    return 0;

  card_instance_t *instance = get_card_instance(player, card);
  target_definition_t td_default;
  if (!td)
  {
    default_target_definition(player, card, &td_default, TYPE_CREATURE);
    td = &td_default;
  }
  if (!prompt && !literal_prompt)
    prompt = "TARGET_CREATURE";
  if (check_special_flags3(player, card, 0x40))
    td->illegal_abilities &= ~KEYWORD_SHROUD;
  if (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET || event == EVENT_CAN_CHANGE_TARGET)
    return generic_spell(player, card, event, 0x201, td, prompt, 1, NULL);

  if (event == EVENT_CAST_SPELL)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      if (check_special_flags(player, card, 0x20000000) ||
          (instance->damage_target_player >= 0 && instance->damage_target_card >= 0))
      {
        instance->targets[0].player = instance->damage_target_player;
        instance->targets[0].card = instance->damage_target_card;
        instance->number_of_targets = 1;
        remove_special_flags(player, card, 0x20000000);
      }
      else
        generic_spell(player, card, event, literal_prompt ? 0x281 : 0x201, td, prompt, 1, NULL);
      if (player == AI && !(trace_mode & 2))
        ai_modifier = (unsigned int)ai_modifier +
                      (instance->targets[0].player == td->preferred_controller ? 24U : -24U);
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    int can_attach = instance->damage_target_player >= 0 && instance->damage_target_card >= 0;
    if (!can_attach && valid_target(td))
    {
      can_attach = is_humiliated(instance->targets[0].player, instance->targets[0].card, AL_STATIC);
      if (!can_attach)
      {
        int target_id = get_id(instance->targets[0].player, instance->targets[0].card);
        can_attach = target_id != 0x32b && target_id != 0x242;
      }
    }
    if (can_attach && attach_aura_to_target(player, card, event, instance->targets[0].player,
                                            instance->targets[0].card))
    {
      remove_special_flags3(player, card, 0x40);
      instance->number_of_targets = 0;
      return 1;
    }
    kill_card(player, card, KILL_STATE_BASED_ACTION);
    instance->number_of_targets = 0;
    spell_fizzled = 1;
  }

  if (in_play(player, card) && instance->damage_target_player >= 0)
  {
    char enchanted_player = instance->damage_target_player;
    int enchanted_card = instance->damage_target_card;
    if (enchanted_card >= 0)
      switch (event)
      {
      case EVENT_STATIC_EFFECTS:
      {
        if (card != affected_card || player != affected_card_controller)
          return 0;
        td->preferred_controller = td->allowed_controller;
        td->flags |= 3;
        int target_id = get_id(enchanted_player, enchanted_card);
        if (validate_arbitrary_target(td, instance->damage_target_player,
                                      instance->damage_target_card))
        {
          if (is_humiliated(instance->damage_target_player, instance->damage_target_card,
                            AL_STATIC) ||
              (target_id != 0x32b && target_id != 0x242))
            return 0;
        }
        kill_card(player, card, KILL_STATE_BASED_ACTION);
        return 0;
      }
      case EVENT_CAN_MOVE_AURA:
        td->preferred_controller = ANYBODY;
        if (!check_special_flags(player, card, SF_MOVE_AURA_LEGAL_TARGET))
          td->illegal_abilities = 0;
        if (check_special_flags(player, card, SF_MOVE_AURA_SAME_CONTROLLER))
        {
          char controller = instance->damage_target_player;
          td->allowed_controller = controller;
          td->preferred_controller = controller;
        }
        if (check_special_flags3(player, card, SF3_ENCHANTMENT_ALTERATION_P0) ||
            check_special_flags3(player, card, SF3_ENCHANTMENT_ALTERATION_P1))
          td->required_type =
              get_type(instance->damage_target_player, instance->damage_target_card);
        return target_available(player, card, td) > 1;
      case EVENT_MOVE_AURA:
      {
        state_untargettable(enchanted_player, enchanted_card, 1);
        td->preferred_controller = ANYBODY;
        if (!check_special_flags(player, card, SF_MOVE_AURA_LEGAL_TARGET))
          td->illegal_abilities = 0;
        if (check_special_flags(player, card, SF_MOVE_AURA_SAME_CONTROLLER))
        {
          char controller = instance->damage_target_player;
          td->allowed_controller = controller;
          td->preferred_controller = controller;
        }
        if (check_special_flags3(player, card, SF3_ENCHANTMENT_ALTERATION_P0) ||
            check_special_flags3(player, card, SF3_ENCHANTMENT_ALTERATION_P1))
        {
          td->required_type =
              get_type(instance->damage_target_player, instance->damage_target_card);
          td->who_chooses = check_special_flags3(player, card, SF3_ENCHANTMENT_ALTERATION_P0) == 0;
        }
        int result;
        if (check_special_flags(player, card, 0x20000000))
          result = validate_target(player, card, td, 0);
        else
          result = new_pick_target(td, prompt, 0, literal_prompt ? 0x80 : 0);
        state_untargettable(instance->damage_target_player, instance->damage_target_card, 0);
        return result;
      }
      case EVENT_RESOLVE_MOVING_AURA:
        if (!check_special_flags(player, card, SF_MOVE_AURA_LEGAL_TARGET))
          td->illegal_abilities = 0;
        if (check_special_flags(player, card, SF_MOVE_AURA_SAME_CONTROLLER))
        {
          char controller = instance->damage_target_player;
          td->allowed_controller = controller;
          td->preferred_controller = controller;
        }
        if (check_special_flags3(player, card, SF3_ENCHANTMENT_ALTERATION_P0) ||
            check_special_flags3(player, card, SF3_ENCHANTMENT_ALTERATION_P1))
        {
          td->required_type =
              get_type(instance->damage_target_player, instance->damage_target_card);
          td->who_chooses = check_special_flags3(player, card, SF3_ENCHANTMENT_ALTERATION_P0) == 0;
        }
        return valid_target(td) &&
               attach_aura_to_target(player, card, event, instance->targets[0].player,
                                     instance->targets[0].card) != 0;
      default:
        break;
      }
  }
  if (event == EVENT_GET_SELECTED_CARD)
    engine_selected_card =
        ((uint32_t)(int32_t)instance->damage_target_player << 8) | instance->damage_target_card;
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e84a0; original functions/auras.c:505. */
int aura(int player, int card, event_t event, target_definition_t *td, char *prompt)
{

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
        return aura_impl(player, card, event, td, prompt, 0);
    }
    else if ((event == EVENT_RESOLVE_SPELL) ||
             ((event == EVENT_CAN_CHANGE_TARGET || (event == EVENT_CHANGE_TARGET))))
      return aura_impl(player, card, event, td, prompt, 0);
    if ((((event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA) &&
         (((2 < (uint32_t)(event - EVENT_POWER) && (event != EVENT_RESOLVE_MOVING_AURA)) &&
           (event != EVENT_CHANGE_TYPE)))) &&
        (((event != EVENT_SET_COLOR && (event != EVENT_GET_SELECTED_CARD)) &&
          (event != EVENT_STATIC_EFFECTS))))
    {
      return 0;
    }
  }
  return aura_impl(player, card, event, td, prompt, 0);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e8560; original functions/auras.c:511. */
int aura_literal_prompt(int player, int card, event_t event, target_definition_t *td,
                        char *prompt)
{

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
        return aura_impl(player, card, event, td, prompt, 1);
    }
    else if ((event == EVENT_RESOLVE_SPELL) ||
             ((event == EVENT_CAN_CHANGE_TARGET || (event == EVENT_CHANGE_TARGET))))
      return aura_impl(player, card, event, td, prompt, 1);
    if ((((event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA) &&
         (((2 < (uint32_t)(event - EVENT_POWER) && (event != EVENT_RESOLVE_MOVING_AURA)) &&
           (event != EVENT_CHANGE_TYPE)))) &&
        (((event != EVENT_SET_COLOR && (event != EVENT_GET_SELECTED_CARD)) &&
          (event != EVENT_STATIC_EFFECTS))))
    {
      return 0;
    }
  }
  return aura_impl(player, card, event, td, prompt, 1);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e8620; original functions/auras.c:517. */
int aura_pref_controller(int player, int card, event_t event, int pref_controller, char *prompt)
{
  if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA &&
      (uint32_t)(uint32_t)(event - EVENT_POWER) > 2 && event != EVENT_RESOLVE_MOVING_AURA &&
      event != EVENT_CHANGE_TYPE && event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD &&
      event != EVENT_STATIC_EFFECTS)
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = pref_controller;
  return aura_impl(player, card, event, &td, prompt, 0);
}
#endif

/* ManalinkEh 2022: 0x26e8710; original functions/auras.c:528. */
void disabling_aura_ai(int player, int card, event_t event)
{
  if ((player == 1) && (((unsigned char)trace_mode & 2) == 0))
  {
    disabling_aura_ai_part(1, card, event);
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26e6710; original functions/auras.c:528. */
static void disabling_aura_ai_part(int player, int card, event_t event)
{
  card_instance_t *instance;
  card_instance_t *target_instance;
  int result;
  if (event == EVENT_CAST_SPELL)
  {
    if (((affected_card == card) && (affected_card_controller == player)) && (spell_fizzled != 1))
    {
      instance = get_card_instance(player, card);
      target_instance = get_card_instance(instance->targets[0].player, instance->targets[0].card);
      ai_modifier =
          (unsigned int)((instance->targets[0].player == 1 - player) ? 24 : -24) + ai_modifier;
      if (((target_instance->regen_status & 0x800000) != 0) && ((target_instance->token_status & 0x800) == 0))
      {
        ai_modifier = ai_modifier + -0x30;
      }
      if ((instance->damage_target_player < '\0') && (instance->damage_target_card < 0))
      {
        instance = get_card_instance(player, card);
        result = another_copy_attached(player, card, instance->internal_card_id);
        if (result != 0)
        {
          ai_modifier = ai_modifier + -0x60;
        }
      }
    }
  }
  else if (event == EVENT_SHOULD_AI_PLAY)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player == '\0')
    {
      instance = get_card_instance(0, instance->damage_target_card);
      ai_modifier = ai_modifier + instance->attack_rating / 2;
    }
  }
  return;
}

#ifndef SHANDALAR
static int generic_aura_impl(int player, int card, event_t event, target_definition_t *td,
                               char *prompt, int p_plus, int t_plus, int k_plus, int k_special,
                               int subtype, int clr, int flags)
{
  pump_ability_t ability;
  default_pump_ability_definition(player, card, &ability, p_plus, t_plus, k_plus, k_special);
  ability.subtype = subtype;
  ability.color = clr;
  if (flags & 8) ability.paue_flags |= 0x80000;
  if (flags & 16) ability.paue_flags |= 0x40000;
  if (flags & 32) ability.paue_flags |= 0x20000;
  if (flags & 1) ability.disabling_mode |= 0x28;
  if (flags & 2) ability.disabling_mode |= 0x7e;
  if (flags & 4) ability.disabling_mode |= 0x38;
  pump_attached(player, card, event, &ability);
  return aura(player, card, event, td, prompt);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e8740; original functions/auras.c:580. */
int generic_aura(int player, int card, event_t event, int pref_controller, int p_plus,
                      int t_plus, int k_plus, int k_special, int subtype, int clr, int flags)
{
  int result;
  pump_ability_t ability_mod;
  target_definition_t td;
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = pref_controller;
  default_pump_ability_definition(player, card, &ability_mod, p_plus, t_plus, k_plus,
                                       k_special);
  ability_mod.subtype = subtype;
  ability_mod.color = clr;
  if ((flags & 8U) != 0)
  {
    ability_mod.paue_flags = ability_mod.paue_flags | 0x80000;
  }
  if ((flags & 0x10U) != 0)
  {
    ability_mod.paue_flags = ability_mod.paue_flags | 0x40000;
  }
  if ((flags & 0x20U) != 0)
  {
    ability_mod.paue_flags = ability_mod.paue_flags | 0x20000;
  }
  if ((flags & 1U) != 0)
  {
    ability_mod.disabling_mode = ability_mod.disabling_mode | 0x28;
  }
  if ((flags & 2U) != 0)
  {
    ability_mod.disabling_mode = ability_mod.disabling_mode | 0x7e;
  }
  if ((flags & 4U) != 0)
  {
    ability_mod.disabling_mode = ability_mod.disabling_mode | 0x38;
  }
  pump_attached(player, card, event, &ability_mod);
  result = aura(player, card, event, &td, "TARGET_CREATURE");
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e7160; original functions/auras.c:587. */
static int targeted_aura_impl(int player, int card, event_t event,
                                   target_definition_t *td, const char *prompt, int custom_prompt,
                                   int flags)
{
  char enchanted_player;
  card_instance_t *instance;
  card_instance_t *active_aura;
  ability_loss_t removed_abilities;

  instance = get_card_instance(player, card);
  active_aura = in_play(player, card);
  if (active_aura != NULL)
  {
    enchanted_player = instance->damage_target_player;
    if ((((event == EVENT_ABILITIES) && (-1 < enchanted_player)) &&
         (instance->damage_target_card == affected_card)) &&
        (enchanted_player == affected_card_controller))
    {
      removed_abilities = -(flags & 1U) & AL_NM_ACT;
      if ((flags & 2U) != 0)
      {
        removed_abilities = AL_ALL;
      }
      if ((flags & 4U) != 0)
      {
        removed_abilities = removed_abilities | AL_ALL_ACT;
      }
      remove_abilities((int)enchanted_player, instance->damage_target_card, EVENT_ABILITIES, removed_abilities);
    }
  }
  return aura_impl(player, card, event, td, prompt, custom_prompt);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e8860; original functions/auras.c:610. */
int targeted_aura(int player, int card, event_t event, target_definition_t *td,
                       const char *prompt)
{
  int result;
  result = targeted_aura_impl(player, card, event, td, prompt, 0, 0);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e8890; original functions/auras.c:614. */
int targeted_aura_custom_prompt(int player, int card, event_t event,
                                     target_definition_t *td, const char *prompt)
{
  int result;
  result = targeted_aura_impl(player, card, event, td, prompt, 1, 0);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e88c0; original functions/auras.c:618. */
int disabling_targeted_aura(int player, int card, event_t event, target_definition_t *td,
                                 const char *prompt, int mode)
{
  int result;

  result = targeted_aura_impl(player, card, event, td, prompt, 1, mode);
  if ((player == 1) && (((unsigned char)trace_mode & 2) == 0))
  {
    disabling_aura_ai_part(1, card, event);
  }
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e8920; original functions/auras.c:628. */
int vanilla_aura(int player, int card, event_t event, int pref_controller)
{
  int result;
  target_definition_t td;
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = pref_controller;
  result = aura(player, card, event, &td, "TARGET_CREATURE");
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e8980; original functions/auras.c:637. */
int disabling_aura(int player, int card, int event)
{
  int result;
  target_definition_t td;
  default_target_definition(player, card, &td, 2);
  result = disabling_targeted_aura(player, card, event, &td, "Select target creature.", 0);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e89e0; original functions/auras.c:644. */
int aura_with_variable_pt_boost_depending_on_condition(int player, int card, event_t event,
                                                            target_definition_t *td,
                                                            test_definition_t *this_test,
                                                            int pow, int tou, int pow2, int tou2)
{
  card_instance_t *instance;
  card_instance_t *active_aura;
  int result;
  int enchanted_player;
  int toughness_boost;
  int power_boost;

  power_boost = pow;
  toughness_boost = tou;
  instance = get_card_instance(player, card);
  active_aura = in_play(player, card);
  if ((active_aura != NULL) && (-1 < instance->damage_target_player))
  {
    enchanted_player = (int)instance->damage_target_player;
    result = instance->damage_target_card;
    if (event == EVENT_POWER)
    {
      if ((result == affected_card) && (enchanted_player == affected_card_controller))
      {
        result = new_make_test_in_play(enchanted_player, result, -1, this_test);
        if (result == 0)
        {
          power_boost = pow2;
        }
        event_result = event_result + power_boost;
      }
    }
    else if (((event == EVENT_TOUGHNESS) && (result == affected_card)) &&
             (enchanted_player == affected_card_controller))
    {
      result = new_make_test_in_play(enchanted_player, result, -1, this_test);
      if (result == 0)
      {
        toughness_boost = tou2;
      }
      event_result = event_result + toughness_boost;
    }
  }
  return targeted_aura_impl(player, card, event, td, "TARGET_CREATURE", 0, 0);
}
#endif

int count_auras_enchanting_me(int player, int card){
	card_instance_t* aura;
	int result = 0;
	int i, count;
	for (i = 0; i < 2; i++){
		for (count = 0; count < active_cards_count[i]; ++count){
			if( (aura = in_play(i, count)) && is_what(i, count, TYPE_ENCHANTMENT) && has_subtype(i, count, SUBTYPE_AURA) &&
				aura->damage_target_player == player && aura->damage_target_card == card
			  ){
				result++;
			}
		}
	}
	return result;
}

static int is_enchanted_impl(int player, int card, int subtype){
	card_instance_t* aura;
	int i, count;
	for (i = 0; i < 2; i++){
		for (count = 0; count < active_cards_count[i]; ++count){
			if( (aura = in_play(i, count)) && is_what(i, count, TYPE_ENCHANTMENT) && has_subtype(i, count, SUBTYPE_AURA) &&
				aura->damage_target_player == player && aura->damage_target_card == card
			  ){
				int result = 1;
				if( subtype > 0 && ! has_subtype(i, count, subtype) ){
					result = 0;
				}
				if( result ){
					return 1;
				}
			}
		}
	}
	return 0;
}

int is_enchanted(int player, int card){
	return is_enchanted_impl(player, card, 0);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e8c90; original functions/auras.c:707. */
void put_into_play_aura_attached_to_target(int aura_player, int aura_card, int t_player,
                                                int t_card)
{
  card_instance_t *instance;

  set_special_flags(aura_player, aura_card, 0x20000000);
  instance = get_card_instance(aura_player, aura_card);
  instance->targets[0].player = t_player;
  instance->targets[0].card = t_card;
  instance->number_of_targets = '\x01';
  instance->damage_target_player = (int8_t)t_player;
  instance->damage_target_card = t_card;
  put_into_play(aura_player, aura_card);
  return;
}
#endif

/* Uses targets[9] to store the player/card it was attached to at the time it was activated.  Should be called *before* the EVENT_RESOLVE_ACTIVATION handler if
 * td is provided. */
int aura_granting_activated_ability(int player, int card, event_t event, int preferred_controller, int mode,
									int cless, int black, int blue, int green, int red, int white,
									uint32_t variable_costs, target_definition_t *td, const char *prompt)
{
  int rval = attachment_granting_activated_ability(player, card, event, mode, cless, black, blue, green, red, white, variable_costs, td, prompt);
  return rval ? rval : vanilla_aura(player, card, event, preferred_controller);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e8d90; original functions/auras.c:729. */
int gain_control_and_attach_as_aura(int player, int card, event_t event, int t_player,
                                         int t_card)
{
  char enchanted_player;
  int result;
  card_instance_t *instance;

  result = check_special_flags3(t_player, t_card, 0x100);
  if (result == 0)
  {
    t_card = gain_control_permanently(player, t_player, t_card);
    if (-1 < t_card)
    {
      if (event == EVENT_RESOLVE_SPELL)
      {
        set_special_flags3(player, card, 0x100);
      }
      if (player == t_player)
      {
        result = check_state(player, card, 0x400000);
        if (result != 0)
        {
          remove_state(player, card, 0x400000);
        }
      }
      else
      {
        add_state(player, card, 0x400000);
      }
      return attach_aura_to_target(player, card, event, player, t_card);
    }
  }
  else
  {
    instance = get_card_instance(t_player, t_card);
    enchanted_player = instance->damage_target_player;
    result = gain_control_permanently(player, (int)enchanted_player, instance->damage_target_card);
    if (-1 < result)
    {
      if (event == EVENT_RESOLVE_SPELL)
      {
        set_special_flags3(player, card, 0x100);
      }
      if (enchanted_player == player)
      {
        result = check_state(player, card, 0x400000);
        if (result != 0)
        {
          remove_state(player, card, 0x400000);
        }
      }
      else
      {
        add_state(player, card, 0x400000);
      }
      t_card = gain_control_permanently_no_sound(player, t_player, t_card);
      return attach_aura_to_target(player, card, event, player, t_card);
    }
  }
  return 0;
}
#endif

static int is_enchanted_by_another_stealing_aura(int player, int card, int orig_aura_player, int orig_aura_card){
	card_instance_t* aura;
	int i, count;
	for( i = 0; i < 2; i++ ){
		for( count = 0; count < active_cards_count[i]; ++count ){
			if( (aura = in_play(i, count)) && is_what(i, count, TYPE_ENCHANTMENT) && has_subtype(i, count, SUBTYPE_AURA) ){
				if( !( i == orig_aura_player && count == orig_aura_card ) ){
					if( aura->damage_target_player == player && aura->damage_target_card == card &&
						check_special_flags3(i, count, SF3_IS_STEALING_AURA)
					  ){
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e8f40; original functions/auras.c:792. */
void end_of_stealing_aura_effect(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (!check_state(player, card, STATE_POWER_STRUGGLE) ||
      !in_play(instance->damage_target_player, instance->damage_target_card))
    return;

  int is_stealing_aura = check_special_flags3(instance->damage_target_player,
                                              instance->damage_target_card, SF3_IS_STEALING_AURA);
  int enchanted_card = instance->damage_target_card;
  if (!is_stealing_aura)
  {
    char enchanted_player = instance->damage_target_player;
    int another_stealer = 0;
    for (int p = 0; p < 2 && !another_stealer; ++p)
      for (int c = 0; c < active_cards_count[p]; ++c)
      {
        card_instance_t *other = in_play(p, c);
        if (other && is_what(p, c, TYPE_ENCHANTMENT) && has_subtype(p, c, SUBTYPE_AURA) &&
            (p != player || c != card) && other->damage_target_player == enchanted_player &&
            other->damage_target_card == enchanted_card &&
            check_special_flags3(p, c, SF3_IS_STEALING_AURA))
        {
          another_stealer = 1;
          break;
        }
      }
    if (!another_stealer)
      re_enable_the_most_recent_control_effect_attached_to_me(instance->damage_target_player,
                                                              instance->damage_target_card);
  }
  else
  {
    card_instance_t *aura = get_card_instance(instance->damage_target_player, enchanted_card);
    gain_control_permanently_no_sound(1 - aura->damage_target_player, aura->damage_target_player,
                                      aura->damage_target_card);
  }
  gain_control_permanently_no_sound(1 - instance->damage_target_player,
                                    instance->damage_target_player, instance->damage_target_card);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e9110; original functions/auras.c:810. */
int generic_stealing_aura(int player, int card, event_t event, target_definition_t *td,
                               const char *prompt)
{
  int result;
  card_instance_t *instance;
  if ((-1 < player) && (-1 < card))
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      instance = get_card_instance(player, card);
      result = valid_target(td);
      if (result == 0)
      {
        kill_card(player, card, KILL_SACRIFICE);
      }
      else
      {
        gain_control_and_attach_as_aura(player, card, EVENT_RESOLVE_SPELL,
                                             instance->targets[0].player,
                                             instance->targets[0].card);
      }
      leaves_play(player, card, EVENT_RESOLVE_SPELL, 0, 0);
    }
    else
    {
      if ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CAN_MOVE_AURA)
      {
        result = targeted_aura_impl(player, card, event, td, prompt, 0, 0);
        return result;
      }
      if (event == EVENT_RESOLVE_MOVING_AURA)
      {
        result = valid_target(td);
        if (result != 0)
        {
          end_of_stealing_aura_effect(player, card);
          instance = get_card_instance(player, card);
          gain_control_and_attach_as_aura(player, card, EVENT_RESOLVE_MOVING_AURA,
                                               instance->targets[0].player,
                                               instance->targets[0].card);
        }
        leaves_play(player, card, EVENT_RESOLVE_MOVING_AURA, 0, 0);
      }
      else
      {
        leaves_play(player, card, event, 0, 0);
        if (event == EVENT_LEAVES_PLAY_ABILITY)
        {
          result = check_special_flags(player, card, 0x2000000);
          if (result != 0)
          {
            end_of_stealing_aura_effect(player, card);
          }
        }
      }
    }
    result = generic_spell(player, card, event, 0x201, td, prompt, 1, NULL);
    return result;
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e7250; original functions/auras.c:847. */
static int generic_animating_aura_impl(int player, int card, event_t event, target_definition_t *td,
                                       const char *prompt, int pow, int tou, int key, int s_key,
                                       int custom_prompt, subtype_t added_subtype)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0 && (instance = in_play(player, card)) &&
      !is_humiliated(player, card, AL_STATIC))
  {
    instance = get_card_instance(player, card);
    int enchanted_card = instance->damage_target_card;
    int enchanted_player;
    if (enchanted_card == affected_card &&
        (enchanted_player = instance->damage_target_player) == affected_card_controller)
    {
      switch (event)
      {
      case EVENT_ABILITIES:
        event_result |= key;
        special_abilities(enchanted_player, enchanted_card, event, s_key, player, card);
        break;
      case EVENT_POWER:
        if (pow > 0)
          set_base_pt(pow);
        break;
      case EVENT_TOUGHNESS:
        if (tou > 0)
          set_base_pt(tou);
        break;
      case EVENT_CHANGE_TYPE:
        if (!is_what(-1, event_result, TYPE_CREATURE))
        {
          if (instance->dummy3 < 1)
          {
            int animated_type = create_a_card_type(event_result);
            uint8_t existing_type = cards_at_7c7000[animated_type]->type;
            cards_at_7c7000[animated_type]->type =
                cards_data[event_result].type | existing_type | TYPE_CREATURE;
            if (pow)
            {
              if (pow == -1)
                pow = get_cmc(enchanted_player, enchanted_card);
              cards_at_7c7000[animated_type]->power = (int16_t)pow;
            }
            if (tou)
            {
              if (tou == -1)
                tou = get_cmc(enchanted_player, enchanted_card);
              cards_at_7c7000[animated_type]->toughness = (int16_t)tou;
            }
            instance->dummy3 = animated_type;
            add_status(enchanted_player, enchanted_card, STATUS_ANIMATED);
          }
          event_result = instance->dummy3;
        }
        else
          remove_status(enchanted_player, enchanted_card, STATUS_ANIMATED);
        if (added_subtype != SUBTYPE_INVALID)
          add_a_subtype(instance->targets[0].player, instance->targets[0].card, added_subtype);
        break;
      case EVENT_RESOLVE_MOVING_AURA:
        if (!check_special_flags(player, card, SF_MOVE_AURA_LEGAL_TARGET))
          td->illegal_abilities = 0;
        if (check_special_flags(player, card, SF_MOVE_AURA_SAME_CONTROLLER))
        {
          char controller = instance->damage_target_player;
          td->allowed_controller = controller;
          td->preferred_controller = controller;
        }
        if (check_special_flags3(player, card, SF3_ENCHANTMENT_ALTERATION_P0) ||
            check_special_flags3(player, card, SF3_ENCHANTMENT_ALTERATION_P1))
        {
          td->required_type =
              get_type(instance->damage_target_player, instance->damage_target_card);
          td->who_chooses = check_special_flags3(player, card, SF3_ENCHANTMENT_ALTERATION_P0) == 0;
        }
        if (valid_target(td) &&
            attach_aura_to_target(player, card, event, instance->targets[0].player,
                                  instance->targets[0].card))
          instance->dummy3 = 0;
        return 0;
      default:
        break;
      }
    }
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(td))
    {
      instance = get_card_instance(player, card);
      if (attach_aura_to_target(player, card, event, instance->targets[0].player,
                                instance->targets[0].card))
        return 0;
    }
    kill_card(player, card, KILL_STATE_BASED_ACTION);
    return 0;
  }
  return targeted_aura_impl(player, card, event, td, prompt, custom_prompt != 0, 0);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e9370; original functions/auras.c:950. */
int generic_animating_aura(int player, int card, event_t event, target_definition_t *td,
                                const char *prompt, int pow, int tou, int key, int s_key)
{
  int result;
  result = generic_animating_aura_impl(player, card, event, td, prompt, pow, tou, key, s_key,
                                            0, SUBTYPE_INVALID);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e93e0; original functions/auras.c:954. */
int generic_animating_aura_custom_prompt(int player, int card, event_t event,
                                              target_definition_t *td, const char *prompt, int pow,
                                              int tou, int key, int s_key)
{
  int result;
  result = generic_animating_aura_impl(player, card, event, td, prompt, pow, tou, key, s_key,
                                            1, SUBTYPE_INVALID);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e9450; original functions/auras.c:958. */
int generic_animating_aura_with_added_subtype(int player, int card, event_t event,
                                                   target_definition_t *td, const char *prompt,
                                                   int pow, int tou, int key, int s_key,
                                                   subtype_t subtype)
{
  int result;
  result = generic_animating_aura_impl(player, card, event, td, prompt, pow, tou, key, s_key,
                                            1, subtype);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e6500; original functions/auras.c:962. */
static int when_enchanted_permanent_dies_return_aura_to_hand_legacy(int player, int card,
                                                                         event_t event)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  seek_grave_for_id_to_reanimate(player, card, instance->targets[0].player,
                                 (unsigned int)cards_data[instance->targets[0].card].id,
                                 REANIMATE_RETURN_TO_HAND);
  kill_card(player, card, KILL_EXILE);
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e94c0; original functions/auras.c:974. */
void when_enchanted_permanent_dies_return_aura_to_hand(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  card_instance_t *active_aura = in_play(player, card);
  int dying_card = affected_card;
  if (active_aura && instance->damage_target_player != -1 && event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    int enchanted_player;
    if (instance->damage_target_card == affected_card &&
        (enchanted_player = instance->damage_target_player) == affected_card_controller &&
        in_play(enchanted_player, affected_card))
    {
      card_instance_t *dying = get_card_instance(enchanted_player, dying_card);
      if ((unsigned char)(dying->kill_code - 1) < 3)
        set_special_flags3(player, card, SF3_ENCHANTED_PERMANENT_DYING);
    }
    if (card == affected_card && player == affected_card_controller && in_play(player, card) &&
        (unsigned char)(instance->kill_code - 1) < 3 &&
        check_special_flags3(player, card, SF3_ENCHANTED_PERMANENT_DYING))
    {
      int legacy = create_legacy_effect(player, card,
                                        when_enchanted_permanent_dies_return_aura_to_hand_legacy);
      card_instance_t *legacy_instance = get_card_instance(player, legacy);
      int owner = get_owner(player, card);
      legacy_instance->targets[0].player = owner;
      legacy_instance = get_card_instance(player, legacy);
      int original_iid = get_original_internal_card_id(player, card);
      legacy_instance->targets[0].card = original_iid;
      get_card_instance(player, legacy)->targets[11].player = 66;
    }
  }
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e9630; original functions/auras.c:1003. */
void cannot_be_enchanted_granted(int granting_player, int granting_card, event_t event,
                                      int t_player, int t_card)
{
  char query_mode;
  if (granting_player == -1)
  {
    misc_protection(t_player, t_card, event);
  }
  else
  {
    special_abilities2(t_player, t_card, event, SP_KEYWORD2_GRANTED_PROT_MISC);
  }
  if (((((event == EVENT_PROT_MISC) && (t_card == affected_card)) &&
        (t_player == affected_card_controller)) &&
       ((query_mode = (char)((unsigned int)event_result >> 0x18),
         query_mode == '\x02' ||
             ((query_mode == '\x05' &&
               (has_subtype(attacking_card_controller, attacking_card, SUBTYPE_AURA) != 0)))))) &&
      ((granting_player == -1 ||
        ((granting_player != attacking_card_controller || (granting_card != attacking_card))))))
  {
    event_result = event_result | 1;
  }
  return;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e9710; original functions/auras.c:1014. */
void cannot_be_enchanted(int player, int card, event_t event)
{
  cannot_be_enchanted_granted(-1, -1, event, player, card);
  return;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e9750; original functions/auras.c:1018. */
int enchant_player(int player, int card, event_t event, target_definition_t *td)
{
  int32_t selected_card;
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  td->flags = td->flags | 2;
  if (event == EVENT_SHOULD_AI_PLAY)
  {
    return should_ai_play(player, card);
  }
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        if (check_special_flags(player, card, 0x20000000) == 0)
        {
          load_text(NULL, "TARGET_PLAYER");
          if (select_target(td->player, td->card, td, text_lines[0], NULL) == 0)
          {
            spell_fizzled = 1;
          }
        }
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      if (valid_target(td) != 0)
      {
        selected_card = instance->targets[0].card;
        instance->targets[4].player = instance->targets[0].player;
        instance->damage_target_player = (int8_t)instance->targets[0].player;
        instance->targets[4].card = selected_card;
        return 0;
      }
      kill_card(player, card, KILL_SACRIFICE);
    }
    return 0;
  }
  return can_target(td);
}
#endif
