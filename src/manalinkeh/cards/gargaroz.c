#include "manalink.h"

// Cards that couls change targets

static const char *has_only_1_target_and_could_legally_change_target(int who_chooses, int player,
                                                                     int card, int unused_source_player, int unused_source_card)
{
  card_instance_t *eff = get_card_instance(player, card);
  if (eff->number_of_targets == 0 || eff->number_of_targets > 1)
    return "must have only 1 target";
  if (!call_card_function(player, card, EVENT_CAN_CHANGE_TARGET))
    return "this spell cannot change its target";

  return NULL;
}

int card_deflection(int player, int card, event_t event)
{
  /* Deflection	|3|U	0x000000
   * Instant
   * Change the target of target spell with a single target. */

  return deflect_one_target(player, card, event, NULL);
}

int card_divert(int player, int card, event_t event)
{
  /* Divert	|U
   * Instant
   * Change the target of target spell with a single target unless that spell's controller pays |2. */

  if (!IS_GS_EVENT(player, card, event))
    return 0;
  target_definition_t td_spell;
  counterspell_target_definition(player, card, &td_spell, TYPE_ANY);
  td_spell.special |= TARGET_SPECIAL_EXTRA_FUNCTION;
  td_spell.extra = target_predicate_extra(has_only_1_target_and_could_legally_change_target);
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAST_SPELL && affect_me(player, card) &&
      has_mana(card_on_stack_controller, COLOR_COLORLESS, 2))
    ai_modifier -= 25;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (counterspell_validate(player, card, &td_spell, 0))
    {
      charge_mana(instance->targets[0].player, COLOR_COLORLESS, 2);
      if (spell_fizzled == 1)
        get_card_instance(instance->targets[0].player, instance->targets[0].card)->targets[0] =
            instance->targets[1];
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return deflect_one_target(player, card, event, &td_spell);
}

int card_imps_michief(int player, int card, event_t event){
  /* Imp's Mischief	|1|B
   * Instant
   * Change the target of target spell with a single target. You lose life equal to that spell's mana value. */

	target_definition_t td_spell;
	counterspell_target_definition(player, card, &td_spell, TYPE_SPELL);
	td_spell.extra = target_predicate_extra(has_only_1_target_and_could_legally_change_target);
	td_spell.special |= TARGET_SPECIAL_EXTRA_FUNCTION;

	card_instance_t *instance = get_card_instance( player, card );

	if (event == EVENT_CAN_CAST ){
		return card_deflection(player, card, event);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( life[player]-get_cmc(card_on_stack_controller, card_on_stack) < 6)
			ai_modifier-=25;
		return card_deflection(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL){
		if( counterspell_validate(player, card, &td_spell, 0) ){
			get_card_instance(instance->targets[0].player, instance->targets[0].card)->targets[0] = instance->targets[1];
			lose_life(player, get_cmc(instance->targets[0].player, instance->targets[0].card));
		}
		kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_misdirection(int player, int card, event_t event){
  /* Misdirection	|3|U|U
   * Instant
   * You may exile a blue card from your hand rather than pay this spell's mana cost.
   * Change the target of target spell with a single target. */

	al_pitchspell(player, card, event, COLOR_BLUE, 0);
	if (!IS_GS_EVENT(player, card, event)){ return 0; }
	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);
	td.special |= TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int32_t)has_only_1_target_and_could_legally_change_target;
	if (event == EVENT_CAN_CAST){ return deflect_one_target(player, card, event, &td); }
	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		if (casting_al_pitchspell(player, card, event, COLOR_BLUE, 0)){ deflect_one_target(player, card, event, &td); }
		else{ spell_fizzled = 1; }
	}
	if (event == EVENT_RESOLVE_SPELL){
		deflect_one_target(player, card, event, &td);
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int quicksilver_dragon_card = -1;
/* ManalinkEh 2022: 0x2427d10; original cards/gargaroz.c:57. */
int card_imps_mischief(int player, int card, event_t event)
{
  /* Imp's Mischief	|1|B
   * Instant
   * Change the target of target spell with a single target. You lose life equal to that spell's mana value. */

  int current_life;
  int spell_cmc;
  card_instance_t *instance;
  target_definition_t td_spell;

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
      counterspell_target_definition(player, card, &td_spell, 0xff);
      td_spell.special = td_spell.special | 0x8000;
      td_spell.extra = (int32_t)has_only_1_target_and_could_legally_change_target;
      get_card_instance(player, card);
      if (((card == affected_card) && (player == affected_card_controller)) &&
          (current_life = life[player],
           spell_cmc = get_cmc(card_on_stack_controller, card_on_stack),
           current_life - spell_cmc < 6))
      {
        ai_modifier = ai_modifier + -0x19;
      }
      return deflect_one_target(player, card, event, &td_spell);
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      counterspell_target_definition(player, card, &td_spell, 0xff);
      td_spell.special = td_spell.special | 0x8000;
      td_spell.extra = (int32_t)has_only_1_target_and_could_legally_change_target;
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_SPELL)
      {
        if (counterspell_validate(player, card, &td_spell, 0) != 0)
        {
          deflect_one_target(player, card, EVENT_RESOLVE_SPELL, &td_spell);
          lose_life(player, get_cmc(instance->targets[0].player, instance->targets[0].card));
        }
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
      return deflect_one_target(player, card, event, &td_spell);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  counterspell_target_definition(player, card, &td_spell, 0xff);
  td_spell.special = td_spell.special | 0x8000;
  td_spell.extra = (int32_t)has_only_1_target_and_could_legally_change_target;
  get_card_instance(player, card);
  return deflect_one_target(player, card, event, &td_spell);
}

static const char* has_only_1_target_and_could_legally_change_target_and_targets_me(int who_chooses, int player, int card)
{
	card_instance_t* eff = get_card_instance(player, card);
	if( eff->number_of_targets == 0 || eff->number_of_targets > 1)
		return "must have only 1 target";
	if( !(eff->targets[0].player == who_chooses && eff->targets[0].card == quicksilver_dragon_card) )
		return "must target Quicksilver Dragon";
	if( ! call_card_function(player, card, EVENT_CAN_CHANGE_TARGET) )
		return "has only 1 legal target";

	return NULL;
}

int card_quicksilver_dragon(int player, int card, event_t event){
  /* Quicksilver Dragon	|4|U|U
   * Creature - Dragon 5/5
   * Flying
   * |U: If target spell has only one target and that target is this creature, change that spell's target to another creature.
   * Morph |4|U (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	if (!IS_GAA_EVENT(event)){
		return morph(player, card, event, MANACOST_XU(4, 1));
	}
	return deflect_away_from_me(player, card, event, 0, MANACOST_U(1), 0, NULL, 1);
}

static const char* has_only_1_target_and_could_legally_change_target_and_targets_a_player(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
	card_instance_t* eff = get_card_instance(player, card);
	if( eff->number_of_targets == 0 || eff->number_of_targets > 1)
		return "must have only 1 target";
	if( eff->targets[0].card != -1 )
		return "must target a player";
	if( ! call_card_function(player, card, EVENT_CAN_CHANGE_TARGET) )
		return "has only 1 legal target";

	return NULL;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_rebound(int player, int card, event_t event){
  /* Rebound	|1|U
   * Instant
   * Change the target of target spell that targets only a player. The new target must be a player. */

	if (!IS_GS_EVENT(player, card, event)){ return 0; }
	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);
	td.special |= TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(has_only_1_target_and_could_legally_change_target_and_targets_a_player);
	if (event == EVENT_CAN_CAST){ return deflect_one_target(player, card, event, &td); }
	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		set_special_flags2(card_on_stack_controller, card_on_stack, SF2_REBOUND);
		deflect_one_target(player, card, event, &td);
		remove_special_flags2(card_on_stack_controller, card_on_stack, SF2_REBOUND);
	}
	if (event == EVENT_RESOLVE_SPELL){
		if (counterspell_validate(player, card, &td, 0)){ deflect_one_target(player, card, event, &td); }
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2428450; original cards/gargaroz.c:199. */
int card_ricochet_trap(int player, int card, event_t event)
{
  /* Ricochet Trap	|3|R
   * Instant - Trap
   * If an opponent cast a blue spell this turn, you may pay |R rather than pay this spell's mana cost.
   * Change the target of target spell with a single target. */

  int result;
  target_definition_t td_spell;

  if (event == EVENT_MODIFY_COST)
  {
    result = get_trap_condition(1 - player, 0x404);
    if (result < 1)
    {
      return 0;
    }
    COST_COLORLESS = '\0';
    COST_BLACK = '\0';
    COST_BLUE = '\0';
    COST_GREEN = '\0';
    COST_RED = '\x01';
    COST_WHITE = '\0';
    return 0;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      return 0;
  }
  else if (event != EVENT_CAN_CAST && event != EVENT_RESOLVE_SPELL &&
           event != EVENT_CAN_CHANGE_TARGET && event != EVENT_CHANGE_TARGET)
    return 0;
  counterspell_target_definition(player, card, &td_spell, 0xff);
  td_spell.special = td_spell.special | 0x8000;
  td_spell.extra = (int32_t)has_only_1_target_and_could_legally_change_target;
  if (event == EVENT_RESOLVE_SPELL)
  {
    deflect_one_target(player, card, EVENT_RESOLVE_SPELL, &td_spell);
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return deflect_one_target(player, card, event, &td_spell);
}

/* ManalinkEh 2022: 0x24279f0; original cards/gargaroz.c:234. */
static char *
has_only_1_target_and_could_legally_change_target_and_targets_a_creature(int who_chooses,
                                                                         int player, int card)
{
  card_instance_t *instance;
  int result;
  char *rejection;

  instance = get_card_instance(player, card);
  rejection = "must have only 1 target";
  if (instance->number_of_targets == '\x01')
  {
    result = is_what(instance->targets[0].player, instance->targets[0].card, 2);
    rejection = "must target a creature";
    if (result != 0)
    {
      result = call_card_function(player, card, EVENT_CAN_CHANGE_TARGET);
      rejection = NULL;
      if (result == 0)
      {
        rejection = "has only 1 legal target";
      }
    }
  }
  return rejection;
}

/* ManalinkEh 2022: 0x24285e0; original cards/gargaroz.c:247. */
int card_meddle(int player, int card, event_t event)
{
  /* Meddle	|1|U
   * Instant
   * If target spell has only one target and that target is a creature, change that spell's target to another creature. */

  int result;
  target_definition_t td_spell;

  if (event == EVENT_CAN_CAST)
  {
    counterspell_target_definition(player, card, &td_spell, 0xff);
    td_spell.special = td_spell.special | 0x8000;
    td_spell.extra =
        (int32_t)has_only_1_target_and_could_legally_change_target_and_targets_a_creature;
    result = deflect_one_target(player, card, EVENT_CAN_CAST, &td_spell);
    return result;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      counterspell_target_definition(player, card, &td_spell, 0xff);
      td_spell.special = td_spell.special | 0x8000;
      td_spell.extra =
          (int32_t)has_only_1_target_and_could_legally_change_target_and_targets_a_creature;
      if ((card == affected_card) && (player == affected_card_controller))
      {
        set_special_flags2(card_on_stack_controller, card_on_stack, 0x4000);
        deflect_one_target(player, card, EVENT_CAST_SPELL, &td_spell);
        remove_special_flags2(card_on_stack_controller, card_on_stack, 0x4000);
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    counterspell_target_definition(player, card, &td_spell, 0xff);
    td_spell.special = td_spell.special | 0x8000;
    td_spell.extra =
        (int32_t)has_only_1_target_and_could_legally_change_target_and_targets_a_creature;
    if (event == EVENT_RESOLVE_SPELL)
    {
      deflect_one_target(player, card, EVENT_RESOLVE_SPELL, &td_spell);
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    counterspell_target_definition(player, card, &td_spell, 0xff);
  }
  return 0;
}

/* ManalinkEh 2022: 0x24287b0; original cards/gargaroz.c:279. */
int card_redirect(int player, int card, event_t event)
{
  /* Redirect	|U|U
   * Instant
   * You may choose new targets for target spell. */

  int result;

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
        result = counterspell_validate(player, card, NULL, 0);
        if (result != 0)
        {
          deflect_multi_targets(player, card, EVENT_RESOLVE_SPELL, NULL);
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
  result = deflect_multi_targets(player, card, event, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2428890; original cards/gargaroz.c:298. */
int card_wild_ricochet(int player, int card, event_t event)
{
  /* Wild Ricochet	|2|R|R
   * Instant
   * You may choose new targets for target instant or sorcery spell. Then copy that spell. You may choose new targets for the copy. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        counterspell_target_definition(player, card, &td, 0x38);
        if ((card == affected_card) && (player == affected_card_controller))
          return deflect_multi_targets(player, card, event, &td);
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      counterspell_target_definition(player, card, &td, 0x38);
      if (event == EVENT_RESOLVE_SPELL)
      {
        result = counterspell_validate(player, card, &td, 0);
        if (result != 0)
        {
          instance = get_card_instance(player, card);
          deflect_multi_targets(player, card, EVENT_RESOLVE_SPELL, &td);
          copy_spell_from_stack(player, instance->targets[0].player, instance->targets[0].card);
        }
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
    }
    else if (event == EVENT_CHANGE_TARGET)
    {
      counterspell_target_definition(player, card, &td, 0x38);
    }
    return 0;
  }
  counterspell_target_definition(player, card, &td, 0x38);
  return deflect_multi_targets(player, card, event, &td);
}

/* ManalinkEh 2022: 0x2428a20; original cards/gargaroz.c:325. */
int card_goblin_flectomancer(int player, int card, event_t event)
{
  /* Goblin Flectomancer	|U|R|R
   * Creature - Goblin Wizard 2/2
   * Sacrifice this creature: You may change the targets of target instant or sorcery spell. */

  int result;
  target_definition_t td;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    counterspell_target_definition(player, card, &td, 0x38);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x408, 0, 0, 0, 0, 0, 0,
                                         0, &td, NULL);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0x408, 0, 0, 0, 0, 0, 0, 0, &td,
                                NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      deflect_multi_targets(player, card, EVENT_CAST_SPELL, &td);
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    counterspell_target_definition(player, card, &td, 0x38);
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  result = counterspell_validate(player, card, &td, 0);
  if (result != 0)
  {
    deflect_multi_targets(player, card, EVENT_RESOLVE_SPELL, &td);
  }
  kill_card(player, card, KILL_SACRIFICE);
  return 0;
}

/* ManalinkEh 2022: 0x2428c10; original cards/gargaroz.c:369. */
int card_willbender(int player, int card, event_t event)
{
  /* Willbender	|1|U
   * Creature - Human Wizard 1/2
   * Morph |1|U (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
   * When this creature is turned face up, change the target of target spell or ability with a single target. */

  card_instance_t *instance;
  int morph_or_deflect_result;
  int mana_available;

  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_UNMORPH)
  {
    mana_available = has_mana_multi(player, 1, 0, 1, 0, 0, 0);
    morph_or_deflect_result = 0;
    if (mana_available != 0)
    {
      morph_or_deflect_result = deflect_one_target(player, card, EVENT_CAN_CAST, NULL);
      instance->targets[9].card = morph_or_deflect_result;
      if (morph_or_deflect_result == 0)
      {
        morph_or_deflect_result = 1;
      }
    }
  }
  else
  {
    morph_or_deflect_result = morph(player, card, event, 1, 0, 1, 0, 0, 0);
    if (event == EVENT_UNMORPH)
    {
      if (((spell_fizzled == 1) || (card_on_stack_controller == -1)) ||
          (instance->targets[9].card != 99))
      {
        ai_modifier = ai_modifier + -0x60;
        return morph_or_deflect_result;
      }
      deflect_one_target(player, card, EVENT_CAST_SPELL, NULL);
      if (spell_fizzled == 1)
      {
        spell_fizzled = 0;
        ai_modifier = ai_modifier + -0x60;
        instance->targets[9].card = 0;
      }
    }
    else if (event == EVENT_TURNED_FACE_UP)
    {
      if (instance->targets[9].card == 99)
      {
        deflect_one_target(player, card, EVENT_RESOLVE_SPELL, NULL);
      }
      spell_fizzled = 0;
    }
  }
  return morph_or_deflect_result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static char *has_only_1_target_and_targets_a_creature_and_can_legally_change_target_to_me(
    int who_chooses, int player, int card, int targeting_player, int targeting_card);

/* ManalinkEh 2022: 0x24278a0; original cards/gargaroz.c:487. */
static char *has_only_1_target_and_targets_a_creature_and_can_legally_change_target_to_me(
    int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  int32_t saved_target_player;
  int32_t saved_target_card;
  card_instance_t *instance;
  char *rejection;
  int retarget_legal;

  instance = get_card_instance(player, card);
  rejection = "must have only 1 target";
  if (instance->number_of_targets == '\x01')
  {
    retarget_legal = is_what(instance->targets[0].player, instance->targets[0].card, 2);
    rejection = "must target a creature";
    if (retarget_legal != 0)
    {
      saved_target_player = instance->targets[9].player;
      instance->targets[9].player = targeting_player;
      saved_target_card = instance->targets[9].card;
      instance->targets[9].card = targeting_card;
      set_special_flags2(player, card, 0x10000);
      retarget_legal = call_card_function(player, card, EVENT_CAN_CAST);
      instance->targets[9].card = saved_target_card;
      instance->targets[9].player = saved_target_player;
      remove_special_flags2(player, card, 0x10000);
      rejection = "cannot target this creature";
      if (retarget_legal != 0)
      {
        rejection = NULL;
      }
    }
  }
  return rejection;
}

/* ManalinkEh 2022: 0x2428fe0; original cards/gargaroz.c:504. */
int card_muck_drubb(int player, int card, event_t event)
{
  /* Muck Drubb	|3|B|B
   * Creature - Beast 3/3
   * Flash
   * When this creature enters, change the target of target spell that targets only a single creature to this creature.
   * Madness |2|B (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

  target_definition_t td;
  counterspell_target_definition(player, card, &td, TYPE_ANY);
  td.special |= TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (int32_t)has_only_1_target_and_targets_a_creature_and_can_legally_change_target_to_me;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    if (counterspell(player, card, event, &td, 0))
    {
      instance->info_slot = 1;
      return 99;
    }
    instance->info_slot = 0;
    return 1;
  }

  if (event == EVENT_CAST_SPELL && affect_me(player, card) && current_turn == player)
    ai_modifier -= 10;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB && instance->info_slot == 1)
  {
    instance->number_of_targets = 1;
    instance->targets[0].player = card_on_stack_controller;
    instance->targets[0].card = card_on_stack;
    if (counterspell_validate(player, card, &td, 0))
    {
      card_instance_t *spell = get_card_instance(instance->targets[0].player, instance->targets[0].card);
      spell->targets[0].player = player;
      spell->targets[0].card = card;
    }
  }
  return madness(player, card, event, MANACOST_XB(2, 1));
}

/* ManalinkEh 2022: 0x24277b0; original cards/gargaroz.c:550. */
static char *has_only_1_target_and_can_legally_change_target_to_attached(
    int who_chooses, int spell_player, int spell_card, int targeting_player, int targeting_card)
{
  card_instance_t *spell = get_card_instance(spell_player, spell_card);
  if (spell->number_of_targets != 1)
    return "must have only 1 target";
  card_instance_t *targeting = get_card_instance(targeting_player, targeting_card);
  int attached_player = targeting->damage_target_player;
  int attached_card = targeting->damage_target_card;
  if (attached_player < 0 || attached_card < 0)
    return "targeting card must be attached";

  target_t prev_9 = spell->targets[9];
  spell->targets[9].player = attached_player;
  spell->targets[9].card = attached_card;
  set_special_flags2(spell_player, spell_card, 0x10000);
  int rvalue = call_card_function(spell_player, spell_card, EVENT_CAN_CAST);
  spell->targets[9] = prev_9;
  remove_special_flags2(spell_player, spell_card, 0x10000);
  return rvalue ? NULL : "cannot target this creature";
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x24291c0; original cards/gargaroz.c:571. */
int card_captured_by_the_consulate(int player, int card, event_t event)
{
  /* Captured by the Consulate	|3|W
   * Enchantment - Aura
   * Enchant creature you don't control
   * Enchanted creature can't attack.
   * Whenever an opponent casts a spell, if it has a single target, change the target to enchanted creature if able. */

  int32_t enchanted_card;
  card_instance_t *instance;
  int trigger_card_or_result;
  unsigned int trigger_flags;
  card_instance_t *triggering_spell;
  target_definition_t td;

  instance = in_play(player, card);
  if (!(((((((instance == NULL) || (instance->damage_target_player == -1)) ||
            (cannot_attack((int)instance->damage_target_player, instance->damage_target_card,
                           event),
             trigger_condition != TRIGGER_SPELL_CAST)) ||
           ((card != affected_card || (player != affected_card_controller)))) ||
          ((reason_for_trigger_controller != player ||
            ((trigger_card_or_result = is_humiliated(player, card, AL_TRIG),
              trigger_card_or_result != 0 || (trigger_cause_controller == player)))))) ||
         (trigger_card_or_result = is_what(trigger_cause_controller, trigger_cause, 1),
          trigger_card_or_result != 0)) ||
        ((trigger_card_or_result = is_token(trigger_cause_controller, trigger_cause),
          trigger_card_or_result != 0 || (trigger_card_or_result = check_special_flags(
                                              trigger_cause_controller, trigger_cause, 0x40000),
                                          trigger_card_or_result != 0)))))
  {
    if (event == EVENT_TRIGGER)
    {

      counterspell_target_definition(player, card, &td, 0xff);
      td.illegal_abilities = 0;
      td.extra = (int32_t)has_only_1_target_and_can_legally_change_target_to_attached;
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      instance->targets[0].player = trigger_cause_controller;
      trigger_card_or_result = trigger_cause;
      instance->number_of_targets = '\x01';
      instance->targets[0].card = trigger_card_or_result;
      trigger_card_or_result = valid_target(&td);
      trigger_flags = 0;
      if (trigger_card_or_result != 0)
      {
        trigger_flags = event_result | 2;
      }
      event_result = trigger_flags;
      instance->targets[0].card = -1;
      instance->targets[0].player = -1;
      instance->number_of_targets = '\0';
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      triggering_spell = get_card_instance(trigger_cause_controller, trigger_cause);
      enchanted_card = instance->damage_target_card;
      triggering_spell->targets[0].player = (int)instance->damage_target_player;
      triggering_spell->targets[0].card = enchanted_card;
    }
    else if (event == EVENT_END_TRIGGER)
    {
      instance->targets[0].card = -1;
      instance->targets[0].player = -1;
      instance->number_of_targets = '\0';
    }
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  td.preferred_controller = td.allowed_controller;
  return disabling_targeted_aura(player, card, event, &td,
                                 "Select target creature you don\'t control.", 0);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x24294b0; original cards/gargaroz.c:665. */
int card_aethersnatch(int player, int card, event_t event)
{
  /* AEthersnatch	|4|U|U	0x000000 --> Too much trouble. Waitlisted.
   * Instant
   * Gain control of target spell. You may choose new targets for it. */

  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
    return counterspell(player, card, EVENT_CAN_CAST, NULL, -1);
  if (event == EVENT_CAST_SPELL)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      card_instance_t *spell = get_card_instance(card_on_stack_controller, card_on_stack);
      if (spell->number_of_targets == 1)
      {
        instance->info_slot = 1;
        return deflect_one_target(player, card, EVENT_CAST_SPELL, NULL);
      }
      spell = get_card_instance(card_on_stack_controller, card_on_stack);
      if (spell->number_of_targets > 1)
      {
        instance->info_slot = 2;
        return deflect_multi_targets(player, card, EVENT_CAST_SPELL, NULL);
      }
      return counterspell(player, card, EVENT_CAST_SPELL, NULL, 0);
    }
  }
  else if (event == EVENT_RESOLVE_SPELL && counterspell_validate(player, card, NULL, 0))
  {
    int mode = instance->info_slot;
    if (mode == 1)
    {
      deflect_one_target(player, card, EVENT_RESOLVE_SPELL, NULL);
      mode = instance->info_slot;
    }
    if (mode == 2)
      deflect_multi_targets(player, card, EVENT_RESOLVE_SPELL, NULL);
    if (is_what(instance->targets[0].player, instance->targets[0].card, TYPE_PERMANENT))
    {
      card_instance_t *spell = get_card_instance(instance->targets[0].player, instance->targets[0].card);
      int iid = spell->internal_card_id;
      if (iid >= 0)
      {
        obliterate_card(instance->targets[0].player, instance->targets[0].card);
        int card_added = add_card_to_hand(player, iid);
        put_into_play(player, card_added);
        int controller = instance->targets[0].player;
        if (controller != player)
        {
          if (controller == AI)
            add_state(player, card_added, 0x1000);
          else
            remove_state(player, card_added, 0x1000);
        }
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2429740; original cards/gargaroz.c:722. */
int card_spelljack(int player, int card, event_t event)
{
  /* Spelljack	|3|U|U|U
   * Instant
   * Counter target spell. If that spell is countered this way, exile it instead of putting it into its owner's graveyard. You may play it without paying its mana cost for as long as it remains exiled. (If it has X in its mana cost, X is 0.) */

  int exiled_csv_id;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {

    if (counterspell_validate(player, card, NULL, 0) != 0)
    {

      instance = get_card_instance(player, card);
      exiled_csv_id = get_id(instance->targets[0].player, instance->targets[0].card);
      if (pre_counterspell(player, card, instance->targets[0].player, instance->targets[0].card) ==
          0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      }
      else
      {
        put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
        rfg_top_card_of_deck(instance->targets[0].player);
      }
      if (-1 < exiled_csv_id)
      {
        create_may_play_card_from_exile_effect(player, card, instance->targets[0].player,
                                               exiled_csv_id, MPCFE_FOR_FREE);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 2, NULL, NULL, 1, NULL);
}

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static char *can_legally_change_target_to_me(int who_chooses, int player, int card,
                                             int targeting_player, int targeting_card);

/* ManalinkEh 2022: 0x24281d0; original cards/gargaroz.c:142. */
int card_silver_wyvern(int player, int card, event_t event)
{
  /* Silver Wyvern	|3|U|U
   * Creature - Drake 4/3
   * Flying
   * |U: Change the target of target spell or ability that targets only this creature. The new target must be a creature. */

  if (((event != EVENT_ACTIVATE) && (1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION))) &&
      (event != EVENT_CLEANUP))
  {
    return 0;
  }
  return deflect_away_from_me(player, card, event, 0, 0, 0, 1, 0, 0, 0, 0, NULL, 1);
}

static int activate_deflect(int player, int card, int event, target_definition_t *td)
{
  return deflect_one_target(player, card, event, td);
}

/* ManalinkEh 2022: 0x24276f0; original cards/gargaroz.c:415. */
static char *can_legally_change_target_to_me(int who_chooses, int player, int card,
                                             int targeting_player, int targeting_card)
{
  card_instance_t *eff = get_card_instance(player, card);
  if (!eff->number_of_targets)
    return "must have at least 1 target";
  target_t prev_9;
  prev_9.player = eff->targets[9].player;
  eff->targets[9].player = targeting_player;
  prev_9.card = eff->targets[9].card;
  eff->targets[9].card = targeting_card;
  set_special_flags2(player, card, 0x10000);
  int rvalue = call_card_function(player, card, EVENT_CAN_CAST);
  eff->targets[9].card = prev_9.card;
  eff->targets[9].player = prev_9.player;
  remove_special_flags2(player, card, 0x10000);
  return rvalue ? NULL : "cannot target this creature";
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline helpers; existing native callers contain the expansions. */
/* ManalinkEh 2022: 0x2428df0; original cards/gargaroz.c:430. */
int card_mizzium_meddler(int player, int card, event_t event)
{
  /* Mizzium Meddler	|2|U	0x000000
   * Creature - Vedalken Wizard 1/4
   * Flash
   * When ~ enters the battlefield, you may change a target of target spell or ability to ~. */

  target_definition_t td;
  counterspell_target_definition(player, card, &td, TYPE_ANY);
  td.special |= TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (int32_t)can_legally_change_target_to_me;
  card_instance_t *instance = get_card_instance(player, card);
  etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

  if (event == EVENT_CAN_CAST)
  {
    if (counterspell(player, card, event, &td, 0))
    {
      instance->info_slot = 1;
      return 99;
    }
    instance->info_slot = 0;
    return 1;
  }

  if (event == EVENT_CAST_SPELL)
  {
    if (affect_me(player, card) && current_turn == player)
      ai_modifier -= 10;
  }
  else if (event == EVENT_ETB && instance->info_slot == 1)
  {
    int stack_controller = card_on_stack_controller;
    int stack_card = card_on_stack;
    instance->targets[0].card = stack_card;
    instance->targets[0].player = stack_controller;
    instance->number_of_targets = 1;
    set_special_flags2(stack_controller, stack_card, SF2_SPELLSKITE);
    int target_index = call_card_function(instance->targets[0].player, instance->targets[0].card,
                                          EVENT_CHANGE_TARGET);
    remove_special_flags2(instance->targets[0].player, instance->targets[0].card, SF2_SPELLSKITE);
    if (target_index == -1)
      spell_fizzled = 1;
    else
      instance->info_slot = target_index;

    if (counterspell_validate(player, card, &td, 0))
    {
      card_instance_t *spell = get_card_instance(instance->targets[0].player, instance->targets[0].card);
      target_index = instance->info_slot;
      spell->targets[target_index].player = player;
      spell->targets[target_index].card = card;
    }
  }
  return 0;
}
#endif
