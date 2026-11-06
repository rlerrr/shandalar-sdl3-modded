#include "manalink.h"
#include "platform/file_io.h"
#include "magic/src/duel_host.h"

static int get_protections_from_part(int player, int card);

/* Canonical recovered helper declarations. */
void base_target_definition(int player, int card, target_definition_t *td, int type);
void default_target_definition(int player, int card, target_definition_t *td, int type);
void opponent_or_pw_target_definition(int player, int card, target_definition_t *td);
void counterspell_target_definition(int player, int card, target_definition_t *td,
                                         int type);
void counter_activated_target_definition(int player, int card, target_definition_t *td,
                                              int type);
void counter_triggered_target_definition(int player, int card, target_definition_t *td,
                                              int type);
int can_target(target_definition_t *td);
int target_available(int player, int card, target_definition_t *td);
int would_validate_arbitrary_target(target_definition_t *td, int tgt_player, int tgt_card);
int validate_arbitrary_target(target_definition_t *td, int tgt_player, int tgt_card);
int would_validate_target(int player, int card, target_definition_t *td,
                               int target_number);
int would_valid_target(target_definition_t *td);
int validate_target(int player, int card, target_definition_t *td, int target_number);
int valid_target(target_definition_t *td);
int pick_next_target_noload_arbitrary(target_definition_t *td, const char *prompt,
                                           int player, int card);
int pick_next_target_arbitrary(target_definition_t *td, const char *prompt, int player,
                                    int card);
int choose_default_target(int player, int card, target_definition_t *td);
int select_target(int player, int card, target_definition_t *td, const char *prompt,
                       target_t *ret_location);
int pick_target(target_definition_t *td, const char *prompt);
int new_pick_target(target_definition_t *td, const char *prompt, int ret_location,
                         int mode);
int pick_next_target_noload(target_definition_t *td, const char *prompt);
int pick_up_to_n_targets_noload(target_definition_t *td, const char *prompt, int num);
int pick_up_to_n_targets(target_definition_t *td, const char *prompt, int num);
int pick_player_duh(int player, int card, int preferred_controller, int allow_cancel);
int target_opponent(int player, int card);
int opponent_is_valid_target(int player, int card);
int mark_up_to_n_targets_noload(target_definition_t *td, const char *prompt, int num,
                                     char (*marked)[151]);
int get_protections_from(int player, int card);
static int target_match_type(card_instance_t *tgt_instance, int tgt_player, int tgt_card,
                                  int special, int type);
static int target_available_maybe_force(
    int player, int card, int *return_count, int test_damage_target_or_source,
    int who_chooses, int allowed_controller, int preferred_controller, int zone,
    int required_type, int illegal_type, int required_abilities, int illegal_abilities,
    int required_color, int illegal_color, intptr_t extra, int required_subtype, int power_requirement,
    int toughness_requirement, int special, int required_state, int illegal_state, int flags,
    int allow_cancel);
int real_validate_target(int tgt_player, int tgt_card, char *return_error_str, int who_chooses,
                              int allowed_controller, int preferred_controller, int zone,
                              int required_type, int illegal_type, int required_abilities,
                              int illegal_abilities, int required_color, int illegal_color,
                              intptr_t extra, int required_subtype, int power_requirement,
                              int toughness_requirement, int special, int required_state,
                              int illegal_state);
int real_target_available(int *return_count, int test_damage_target_or_source, int who_chooses,
                               int allowed_controller, int preferred_controller, int zone,
                               int required_type, int illegal_type, int required_abilities,
                               int illegal_abilities, int required_color, int illegal_color,
                               intptr_t extra, int required_subtype, int power_requirement,
                               int toughness_requirement, int special, int required_state,
                               int illegal_state);
int real_select_target(int who_chooses, int allowed_controller, int preferred_controller,
                            int zone, int required_type, int illegal_type, int required_abilities,
                            int illegal_abilities, int required_color, int illegal_color, intptr_t extra,
                            int required_subtype, int power_requirement, int toughness_requirement,
                            int special, int required_state, int illegal_state, const char *prompt,
                            int allow_cancel, target_t *ret_tgt);
int is_protected_from(int protected_player, int protected_card, int from_player, int from_card,
                           is_protected_from_mode_t new_protection_mode);
static int validate_target_impl(int player, int card, int tgt_player, int tgt_card,
                                     char *return_error_str, int who_chooses,
                                     int allowed_controller, int preferred_controller, int zone,
                                     int required_type, int illegal_type, int required_abilities,
                                     int illegal_abilities, int required_color, int illegal_color,
                                     intptr_t extra, int required_subtype, int power_requirement,
                                     int toughness_requirement, int special, int required_state,
                                     int illegal_state, int flags);
static int target_available_impl(int player, int card, int *return_count,
                                      int test_damage_target_or_source, int who_chooses,
                                      int allowed_controller, int preferred_controller, int zone,
                                      int required_type, int illegal_type, int required_abilities,
                                      int illegal_abilities, int required_color, int illegal_color,
                                      intptr_t extra, int required_subtype, int power_requirement,
                                      int toughness_requirement, int special, int required_state,
                                      int illegal_state, int flags);
static int select_target_impl(int player, int card, int who_chooses, int allowed_controller,
                                   int preferred_controller, int zone, int required_type,
                                   int illegal_type, int required_abilities, int illegal_abilities,
                                   int required_color, int illegal_color, intptr_t extra,
                                   int required_subtype, int power_requirement,
                                   int toughness_requirement, int special, int required_state,
                                   int illegal_state, int flags, const char *prompt,
                                   int allow_cancel, target_t *ret_tgt);
static int select_target_maybe_force(
    int player, int card, int who_chooses, int allowed_controller, int preferred_controller,
    int zone, int required_type, int illegal_type, int required_abilities, int illegal_abilities,
    int required_color, int illegal_color, intptr_t extra, int required_subtype, int power_requirement,
    int toughness_requirement, int special, int required_state, int illegal_state, int flags,
    const char *prompt, int allow_cancel, target_t *ret_tgt);
int autoselect_target(int player, int card, int i, int j, int k, int who_chooses,
                           int allowed_controller, int preferred_controller, int zone,
                           int required_type, int illegal_type, int required_abilities,
                           int illegal_abilities, int required_color, int illegal_color, intptr_t extra,
                           int required_subtype, int power_requirement, int toughness_requirement,
                           int special, int required_state, int illegal_state, int flags,
                           int prompt, int allow_cancel, int ret_location);

extern int target_controller, target_card;
int sub_4378E0(int, int, const char*, int, int, int, int, int, int*, target_t*, int, int);
int sub_45CEC0(void);
int sub_45CF00(void);
int sub_47E980(int, int);
int sub_47EF10(int, int);
int sub_47EF50(int, int);
int sub_498F20(void);
int sub_499050(void);

static int hack_gpf_player = -1;
static int hack_gpf_card = -1;

is_protected_from_mode_t protection_mode = IPF_TARGET;

const char* hack_prepend_prompt = NULL;

int auto_targets[500];
int auto_targets_target[500];
int read_auto_target_file = 0;

// Like default_target_definition, but doesn't set illegal_abilities.

// Like default_target_definition_t, but sets TARGET_TYPE_PLANESWALKER, TARGET_ZONE_CREATURE_OR_PLAYER, and TARGET_SPECIAL_OPPONENT.

// Selects a target into (td->player, td->card)->targets[0].  Loads prompt from Tetx.res.  Sets spell_fizzled if cancelled.

/* Selects a target into (td->player, td->card)->targets[ret_location], or into the next unused slot (counting by instance->number_of_targets) if
	ret_location is -1. */

/* Selects a target into next (td->player, td->card)'s unused target slot (counting by instance->number_of_targets).  prompt is used literally, not loaded from Text.res.  Sets
 * spell_fizzled if cancelled. */

// Just like pick_up_to_n_targets(), but uses prompt literally instead of loading from Text.res.

/* Selects up to num targets starting from (td->player, td->card)->targets[0].  Returns actual number chosen (also in number_of_targets).  Set allow_cancel to 3
 * to show both Done and Cancel buttons.  Sets spell_fizzled only if the Cancel button is clicked, not if the Done button is. */

/* Just like pick_up_to_n_targets_noload(), but doesn't change (td->player,td->card)'s number_of_targets or targets array, instead marking targets in
 * marked[][].  If the Cancel button is pushed, returns 0 and sets cancel to 1, but leaves previously-chosen targets set in marked.  marked should be a char
 * array[2][151] initialized to 0.  Set num to -1 to allow any number of targets.  This should only be used for non-targetted things resolving immediately after
 * chosen, since they won't track change of control or be seen by when-this-becomes-targeted triggers. */

/* Selects a target into next (player, card)'s unused target slot (counting by instance->number_of_targets), while using (td->player, td->card) as the
 * targeting source.  Sets spell_fizzled if cancelled. */

/* Selects a target into next (player, card)'s unused target slot (counting by instance->number_of_targets), while using (td->player, td->card) as the targeting
 * source.  prompt is used literally, not loaded from Text.res.  Sets spell_fizzled if cancelled. */

/* Selects a target into ret_location, or (player, card)->targets[0] if NULL.  Targeting source is player/card instead of td->player/td->card.  prompt is used
 * literally, not loaded from Text.res.  Never sets spell_fizzled. */

/* If duh mode is on, pick preferred_controller (which is opponent by default) if he's a valid target, or else cancel if that's allowed.
 * Otherwise, forwards to pick_target(td, "TARGET_PLAYER"). */

// (player/card) targets 1-player in its targets[0] and returns nonzero, else returns 0.

/* ManalinkEh 2022: 0x265d540; original functions/targets.c:16. */
keyword_t color_to_protection(color_t color) { return (color & 0x3e) << 10; }

/* ManalinkEh 2022: 0x265d580; original functions/targets.c:107. */
int has_protection_from_impl(int protected_player, int protected_card, int from_player,
    int from_card, is_protected_from_mode_t new_protection_mode, keyword_t keywords)
{
  keyword_t protection_types = KEYWORD_PROT_CREATURES | KEYWORD_PROT_SORCERIES |
      KEYWORD_PROT_INSTANTS | KEYWORD_PRISTINE;
  if ((keywords & protection_types) && from_card != -1 && from_player != -1 &&
      (keywords & protection_types & get_protections_from_part(from_player, from_card)))
    return 1;
  uint32_t initial_event_result = (uint32_t)new_protection_mode << 24;
  if (protected_card < 0)
  {
    if (!(player_bits[protected_player] & PB_PROTECTION))
      return 0;
  }
  else
  {
    card_instance_t *instance = get_card_instance(protected_player, protected_card);
    sp_keyword2_t abilities = instance->ext->sp_keywords2;
    if (!(abilities & (SP_KEYWORD2_PROT_MISC | SP_KEYWORD2_GRANTED_PROT_MISC)) &&
        (!(abilities & SP_KEYWORD2_HEXPROOF_MISC) || new_protection_mode != IPF_TARGET ||
         from_player == protected_player))
      return 0;
    if (!(abilities & SP_KEYWORD2_GRANTED_PROT_MISC))
    {
      if (is_humiliated(protected_player, protected_card, AL_STATIC))
        return 0;
      return (uint32_t)dispatch_event_arbitrary_to_one_card_with_initial_event_result(
          protected_player, protected_card, EVENT_PROT_MISC, protected_player, protected_card,
          from_player, from_card, initial_event_result) & 0xffffffU;
    }
  }
  return (uint32_t)dispatch_event_with_attacker_and_initial_event_result(protected_player,
      protected_card, EVENT_PROT_MISC, from_player, from_card, initial_event_result) & 0xffffffU;
}

/* ManalinkEh 2022: 0x265d6c0; original functions/targets.c:116. */
int is_protected_from(int protected_player, int protected_card, int from_player, int from_card,
                           is_protected_from_mode_t new_protection_mode)
{
  keyword_t keywords;
  int result;

  keywords = KEYWORD_0;
  if (-1 < protected_card)
  {
    keywords = get_abilities(protected_player, protected_card, EVENT_ABILITIES, -1);
  }
  result = has_protection_from_impl(protected_player, protected_card, from_player, from_card,
                                    new_protection_mode, keywords);
  return result;
}

void give_shroud_to_player(int player, int card, event_t event){
	player_bits[player] |= PB_PLAYER_HAS_SHROUD;
}

void give_hexproof_to_player(int player, int card, event_t event){
	player_bits[player] |= PB_PLAYER_HAS_HEXPROOF;
}

/* Legacy 80-byte compatibility wrapper retained; the 84-byte 2022 implementation is below. */

/* ManalinkEh 2022: 0x2661490; original functions/targets.c:1795. */
int choose_player(int player, int card, int preferred_controller, int allow_cancel)
{
  card_instance_t *instance;
  int result;
  target_t chosen_player;
  target_definition_t td;

  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 0);
  td.preferred_controller = preferred_controller;
  td.zone = TARGET_ZONE_PLAYERS;
  td.allow_cancel = allow_cancel;
  td.illegal_abilities = 0;
  result = duh_mode(player);
  if (result != 0)
  {
    instance->number_of_targets = '\x01';
    instance->targets[0].player = preferred_controller;
    instance->targets[0].card = -1;
    result = would_validate_target(td.player, td.card, &td, 0);
    if (result != 0)
    {
      return 1;
    }
    if ((allow_cancel & 1U) != 0)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  instance->number_of_targets = '\0';
  result = can_target(&td);
  if (result != 0)
  {
    select_target(player, card, &td, "Choose a player.", &chosen_player);
  }
  if (chosen_player.card != -1)
  {
    backtrace_and_abort("functions/targets.c:1824\nchosen_player.card == -1");
  }
  return chosen_player.player;
}

/* Returns nonzero if (player/card) can target 1-player, else 0.
 * The difference from target_opponent() is that it doesn't assign the target, increase number_of_targets, or set cancel. */

int is_protected_from_me(int player, int card, int t_player, int t_card){

	card_instance_t *instance = get_card_instance(player, card);
	int clr = instance->card_color;
	if( get_id(t_player, t_card) == CARD_ID_GAEAS_REVENGE && !(clr & COLOR_TEST_GREEN) ){
		return 1;
	}

	if( get_id(t_player, t_card) == CARD_ID_SUQATA_FIREWALKER && (clr & COLOR_TEST_RED) ){
		return 1;
	}

	if( get_id(t_player, t_card) == CARD_ID_AZORIUS_FIRST_WING && is_what(player, card, TYPE_ENCHANTMENT) ){
		return 1;
	}

	if( get_id(t_player, t_card) == CARD_ID_ENEMY_OF_THE_GUILDPACT && count_colors(player, card) > 1 ){
		return 1;
	}

	if( get_id(t_player, t_card) == CARD_ID_GUARDIAN_OF_THE_GUILDPACT && count_colors(player, card) == 1 ){
		return 1;
	}

	if( get_id(t_player, t_card) == CARD_ID_EMRAKUL_THE_AEONS_TORN && clr != COLOR_TEST_COLORLESS && is_what(player, card, TYPE_SPELL) &&
		! is_what(player, card, TYPE_PERMANENT)
	  ){
		return 1;
	}
	/*
	if( is_what(t_player, t_card, TYPE_CREATURE) && check_special_flags2(t_player, t_card, SF2_DENSE_FOLIAGE) && is_what(player, card, TYPE_SPELL) &&
		! is_what(player, card, TYPE_PERMANENT)
	  ){
		return 1;
	}
	*/
	if( player != t_player && get_id(t_player, t_card) == CARD_ID_FIENDSLAYER_PALADIN && ((clr & COLOR_TEST_BLACK) || (clr & COLOR_TEST_RED)) ){
		return 1;
	}
	return 0;
}

// Populates arr with array of unique creatures controlled by controller_of_target targeted by (player,card), then returns it.
/* ManalinkEh 2022: 0x2661860; original functions/targets.c:2034. */
int can_legally_target(int targeting_player, int targeting_card, target_t *targeted)
{
  card_instance_t *instance = get_card_instance(targeting_player, targeting_card);
  if (!instance->number_of_targets)
    return 0;
  int selected_card = targeted->card;
  instance->targets[9].player = targeted->player;
  instance->targets[9].card = selected_card;
  set_special_flags2(targeting_player, targeting_card, SF2_SPELLSKITE);
  int result = call_card_function(targeting_player, targeting_card, EVENT_CAN_CAST);
  instance->targets[9].player = -1;
  instance->targets[9].card = -1;
  remove_special_flags2(targeting_player, targeting_card, SF2_SPELLSKITE);
  return result;
}

int target_me(int t_player, int t_card, int player, int card)
{
  card_instance_t* instance = get_card_instance(player, card);
  int i;
  for (i = 0; i < instance->number_of_targets; ++i)
	if (instance->targets[i].player == t_player && instance->targets[i].card == t_card)
	  return 1;

  return 0;
}

/* When {t_player,t_card} becomes targeted by a spell controlled by controlled_by, returns the spell (else NULL).
 * {player,card} is solely the card that highlights for the trigger.  Doesn't check is_humiliated() or in_play(). */
const target_t* becomes_target_of_spell(int player, int card, event_t event, int t_player, int t_card, int controlled_by, resolve_trigger_t trigger_mode)
{
  static target_t rval;
  if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller
	  && (controlled_by == ANYBODY || trigger_cause_controller == controlled_by))
	{
	  if (event != EVENT_TRIGGER && event != EVENT_RESOLVE_TRIGGER)
		return NULL;

	  if (target_me(t_player, t_card, trigger_cause_controller, trigger_cause) && !is_what(trigger_cause_controller, trigger_cause, TYPE_LAND | TYPE_EFFECT))
		{
		  if (event == EVENT_TRIGGER)
			{
			  if (trigger_mode == RESOLVE_TRIGGER_DUH)
				trigger_mode = duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;

			  event_result |= trigger_mode;
			  return NULL;
			}
		  else // event == EVENT_RESOLVE_TRIGGER;
			{
			  rval.player = trigger_cause_controller;
			  rval.card = trigger_cause;
			  return &rval;
			}
		}
	}

  return NULL;
}

// "(Creature this effect is attached to) gains 'When this creature becomes the target of a spell or ability, sacrifice it.'"
/* When {t_player,t_card} becomes targeted by a spell or ability controlled by controlled_by, returns the spell or effect.
 * {player,card} is solely the card that highlights for the trigger.  Doesn't check is_humiliated() or in_play(). */
const target_t* becomes_target_of_spell_or_effect(int player, int card, event_t event, int t_player, int t_card, int controlled_by)
{
  const target_t* btos;
  if ((btos = becomes_target_of_spell(player, card, event, t_player, t_card, controlled_by, RESOLVE_TRIGGER_MANDATORY)))
	return btos;

  static target_t rval;
  if ((event == EVENT_PLAY_ABILITY || event == EVENT_TAPPED_TO_PLAY_ABILITY || event == EVENT_TRIGGER_RESOLVED)
	  && (controlled_by == ANYBODY || affected_card_controller == controlled_by)
	  && target_me(t_player, t_card, affected_card_controller, affected_card)
	  && get_card_instance(affected_card_controller, affected_card)->info_slot != (intptr_t)legacy_effect_pump_ability_until_eot
	  )
	{
	  rval.player = affected_card_controller;
	  rval.card = affected_card;
	  return &rval;
	}

  return NULL;
}

// "When enchanted creature becomes the target of a spell or ability, sacrifice/destroy/bury/exile (this aura)."
static int is_targeting_type(int player, int card, int controller_of_target, int or_player, type_t targeted_type, int subtype)
{
  card_instance_t* instance = get_card_instance(player, card);
  int i;
  for (i = 0; i < instance->number_of_targets; ++i)
	if ((controller_of_target != ANYBODY ? instance->targets[i].player == controller_of_target
		 : (instance->targets[i].player == 0 || instance->targets[i].player == 1))
		&& (instance->targets[i].card == -1 ? or_player
			: (in_play(instance->targets[i].player, instance->targets[i].card)
			   && is_what(instance->targets[i].player, instance->targets[i].card, targeted_type)
			   && (subtype == -1 || has_subtype(instance->targets[i].player, instance->targets[i].card, subtype)))))
	  return 1;

  return 0;
}

// "When enchanted creature becomes the target of a spell, sacrifice/destroy/bury/exile (this aura)."
static target_t* fill_arr(int player, int card, int controller_of_target, int or_player, type_t targeted_type, int subtype, target_t* arr)
{
  card_instance_t* instance = get_card_instance(player, card);
  ASSERT(instance->number_of_targets < 20);
  int i, j, last = -1;
  for (i = 0; i < instance->number_of_targets; ++i)
	if ((controller_of_target != ANYBODY ? instance->targets[i].player == controller_of_target
		 : (instance->targets[i].player == 0 || instance->targets[i].player == 1))
		&& (instance->targets[i].card == -1 ? or_player
			: (in_play(instance->targets[i].player, instance->targets[i].card)
			   && is_what(instance->targets[i].player, instance->targets[i].card, targeted_type)
			   && (subtype == -1 || has_subtype(instance->targets[i].player, instance->targets[i].card, subtype)))))
	  {
		for (j = 0; j < i; ++j)
		  if (instance->targets[i].player == instance->targets[j].player
			  && instance->targets[i].card == instance->targets[j].card)
			goto outer_continue;	// only triggers once per creature even if it's targeted more than once by the same spell/effect, per Wild Defiance ruling

		arr[++last] = instance->targets[i];	// struct copy

	  outer_continue:;
	  }

  arr[++last].player = -1;
  arr[last].card = -1;

  return arr;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* The general case.  Returns array of unique targeted_types (and players, if or_player is nonzero) controlled by controller_of_target currently being targeted
 * by a targeting_type (TYPE_EFFECT: an effect; other types: a spell of that type being cast) controlled by controller_of_trigger, ending in a {-1,-1} entry; or
 * NULL if none. */
const target_t* any_becomes_target(int player, int card, event_t event, int controller_of_target, int or_player, type_t targeted_type, int subtype, type_t targeting_type, int controller_of_trigger, resolve_trigger_t resolve_trigger)
{
  static target_t arr[20];

  if (!in_play(player, card) || is_humiliated(player, card, AL_TRIG))
	return NULL;

  if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller)
	{
	  if (!(event == EVENT_TRIGGER || event == EVENT_RESOLVE_TRIGGER)
		  || !(targeting_type & TYPE_NONEFFECT)
		  || (controller_of_trigger != ANYBODY && trigger_cause_controller != controller_of_trigger))
		return NULL;

	  if (is_what(trigger_cause_controller, trigger_cause, targeting_type & TYPE_NONEFFECT)
		  && is_targeting_type(trigger_cause_controller, trigger_cause, controller_of_target, or_player, targeted_type, subtype))
		{
		  if (event == EVENT_TRIGGER)
			{
			  if (resolve_trigger == RESOLVE_TRIGGER_DUH)
				resolve_trigger = duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;

			  event_result |= resolve_trigger;
			  return NULL;
			}
		  else // event == EVENT_RESOLVE_TRIGGER;
			return fill_arr(trigger_cause_controller, trigger_cause, controller_of_target, or_player, targeted_type, subtype, arr);
		}
	}

  if (event == EVENT_PLAY_ABILITY || event == EVENT_TAPPED_TO_PLAY_ABILITY || event == EVENT_TRIGGER_RESOLVED)
	{
	  if ((targeting_type & TYPE_EFFECT)
		  && (controller_of_trigger == ANYBODY || controller_of_trigger == affected_card_controller)
		  && is_targeting_type(affected_card_controller, affected_card, controller_of_target, or_player, targeted_type, subtype))
		return fill_arr(affected_card_controller, affected_card, controller_of_target, or_player, targeted_type, subtype, arr);
	}

  return NULL;
}

/* A common case of any_becomes_target().  Returns array of unique creatures controlled by controller_of_target currently being targeted, ending in a {-1,-1}
 * entry; or NULL if none. */
const target_t* any_creature_becomes_target_of_spell_or_effect(int player, int card, event_t event, int controller_of_target)
{
  return any_becomes_target(player, card, event, controller_of_target, 0, TYPE_CREATURE, -1, TYPE_EFFECT|TYPE_NONEFFECT, ANYBODY, RESOLVE_TRIGGER_MANDATORY);
}

int attached_creature_gains_sacrifice_when_becomes_target(int player, int card, event_t event)
{
  card_instance_t* instance;
  if ((instance = in_play(player, card))
	  && instance->damage_target_card >= 0
	  && in_play(instance->damage_target_player, instance->damage_target_card)
	  && !is_humiliated(instance->damage_target_player, instance->damage_target_card, AL_TRIG)
	  && becomes_target_of_spell_or_effect(player, card, event, instance->damage_target_player, instance->damage_target_card, ANYBODY))
	{
	  kill_card(instance->damage_target_player, instance->damage_target_card, KILL_SACRIFICE);
	  return 1;
	}
  return 0;
}

#ifndef SHANDALAR
static int get_protections_from_part(int player, int card);
#endif

int kill_attachment_when_creature_is_targeted(int player, int card, event_t event, kill_t kill_code)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (in_play(player, card)
	  && instance->damage_target_card >= 0
	  && in_play(instance->damage_target_player, instance->damage_target_card)
	  && !is_humiliated(player, card, AL_TRIG)
	  && becomes_target_of_spell_or_effect(player, card, event, instance->damage_target_player, instance->damage_target_card, ANYBODY))
	{
	  kill_card(player, card, kill_code);
	  return 1;
	}
  return 0;
}

int kill_attachment_when_creature_is_targeted_by_spell(int player, int card, event_t event, kill_t kill_code)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (in_play(player, card)
	  && instance->damage_target_card >= 0
	  && in_play(instance->damage_target_player, instance->damage_target_card)
	  && !is_humiliated(player, card, AL_TRIG)
	  && becomes_target_of_spell(player, card, event, instance->damage_target_player, instance->damage_target_card, ANYBODY, RESOLVE_TRIGGER_MANDATORY))
	{
	  kill_card(player, card, kill_code);
	  return 1;
	}
  return 0;
}

#ifndef SHANDALAR

/* ManalinkEh 2022: 0x265d2e0; original functions/targets.c:21. */
static int get_protections_from_part(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  while (instance->internal_card_id == activation_card)
  {
    player = instance->parent_controller;
    card = instance->parent_card;
    instance = get_card_instance(player, card);
  }
  int iid = instance->internal_card_id;
  if (iid == -1)
    iid = instance->backup_internal_card_id;
  uint8_t type = cards_data[iid].type;
  unsigned int protections = KEYWORD_SHROUD;
  if (type & TYPE_SORCERY)
    protections |= KEYWORD_PROT_SORCERIES;
  if ((type & (TYPE_INSTANT | TYPE_INTERRUPT)) && !(type & (TYPE_PERMANENT | TYPE_SORCERY)))
    protections |= KEYWORD_PROT_INSTANTS;
  if (type & TYPE_CREATURE)
    protections |= KEYWORD_PROT_CREATURES;
  if (type & TYPE_LAND)
    protections |= KEYWORD_PROT_LANDS;
  if (type & TYPE_ARTIFACT)
    protections |= KEYWORD_PROT_ARTIFACTS;
  unsigned int colors = get_color(player, card);
  hack_gpf_player = player;
  hack_gpf_card = card;
  return ((colors & COLOR_TEST_ANY_COLORED) << 10) | protections;
}

/* ManalinkEh 2022: 0x265d550; original functions/targets.c:21. */
int get_protections_from(int player, int card)
{
  int result;

  if ((player != -1) && (card != -1))
  {
    result = get_protections_from_part(player, card);
    return result;
  }
  return 0;
}

static int has_misc_protection_from(int protected_player, int protected_card, int from_player,
                                    int from_card, is_protected_from_mode_t new_protection_mode,
                                    int allow_granted_protection)
{
  uint32_t initial = (uint32_t)new_protection_mode << 24;
  if (protected_card < 0)
  {
    if (!(player_bits[protected_player] & PB_PROTECTION))
      return 0;
  }
  else
  {
    uint32_t flags = get_card_instance(protected_player, protected_card)->ext->sp_keywords2;
    int granted = allow_granted_protection && (flags & SP_KEYWORD2_GRANTED_PROT_MISC);
    if (!(flags & SP_KEYWORD2_PROT_MISC) &&
        (!(flags & 4) || new_protection_mode != IPF_TARGET || from_player == protected_player) && !granted)
      return 0;
    if (!granted)
    {
      if (is_humiliated(protected_player, protected_card, AL_STATIC))
        return 0;
      return dispatch_event_arbitrary_to_one_card_with_initial_event_result(
          protected_player, protected_card, EVENT_PROT_MISC, protected_player, protected_card,
          from_player, from_card, (int)initial) & 0xffffff;
    }
  }
  return dispatch_event_with_attacker_and_initial_event_result(
      protected_player, protected_card, EVENT_PROT_MISC, from_player, from_card, (int)initial) & 0xffffff;
}

/* ManalinkEh 2022: 0x265d1a0; original functions/targets.c:217. */
static int target_match_type(card_instance_t *tgt_instance, int tgt_player, int tgt_card,
    int special, int type)
{
  int iid = tgt_instance->internal_card_id;
  int matches;
  if (special & TARGET_SPECIAL_EFFECT_CARD)
  {
    matches = is_what(-1, tgt_instance->original_internal_card_id, type & 0x1ff);
    type |= TYPE_EFFECT;
  }
  else
    matches = is_what(tgt_player, tgt_card, type & 0x1ff);
  if (matches || ((type & TARGET_TYPE_TOKEN) && is_token(tgt_player, tgt_card)) ||
      ((type & TARGET_TYPE_NONCREATURE_CAN_BLOCK) &&
       (tgt_instance->state & STATE_NONCREATURE_CAN_BLOCK)))
    return 1;
  if (type & TYPE_EFFECT)
  {
    uint32_t id_word = cards_data[iid].id | 0;
    if (((id_word - 0x387U) & 0xfffdU) == 0)
      return 1;
  }
  if ((type & TARGET_TYPE_DAMAGE_LEGACY) && cards_data[iid].id == 0x385)
    return 1;
  if ((type & TARGET_TYPE_HACK_SLEIGHT_LEGACY) && cards_data[iid].id == 0x386)
    return 1;
  return (type & TARGET_TYPE_DRAW_CARD_LEGACY) && cards_data[iid].id == 0x388;
}

/* ManalinkEh 2022: 0x265d720; original functions/targets.c:260. */
static int validate_target_impl(int player, int card, int tgt_player, int tgt_card,
                                char *return_error_str, int who_chooses, int allowed_controller,
                                int preferred_controller, int zone, int required_type,
                                int illegal_type, int required_abilities, int illegal_abilities,
                                int required_color, int illegal_color, intptr_t extra,
                                int required_subtype, int power_requirement,
                                int toughness_requirement, int special, int required_state,
                                int illegal_state, int flags)
{
  if (duel_host_scripted_input()) preferred_controller = allowed_controller;

  // Local names for native flag bits whose original spellings are unconfirmed.
  enum
  {
    TARGET_SPECIAL_SAME_PLAYER = 0x04000000,
    TARGET_SPECIAL_STACK_RESTRICTION = 0x08000000
  };
  int rval = 1;
  char error_str[200];

#define FAIL_STR(err_str)                                                                          \
  do                                                                                               \
  {                                                                                                \
    rval = 0;                                                                                      \
    if (err_str[0] != ',')                                                                         \
      strcat(&error_str[0], ",");                                                                  \
    strcat(&error_str[0], err_str);                                                                \
    goto epilog;                                                                                   \
  } while (0)

#define FAILURE(error_addr)                                                                        \
  do                                                                                               \
  {                                                                                                \
    rval = 0;                                                                                      \
    strcat(&error_str[0], error_addr);                                                    \
    goto epilog;                                                                                   \
  } while (0)

  if (tgt_player == -1 ||
      (tgt_card != -1 && (get_card_instance(tgt_player, tgt_card)->internal_card_id == -1 ||
                          (int32_t)get_card_instance(tgt_player, tgt_card)->token_status < 0)))
  {
    if (return_error_str)
      *return_error_str = 0;
    return 0;
  }

  error_str[0] = 0;
  if (tgt_card == -1)
  {
    // Targeting a player.

    int can_target_player_0, can_target_player_1;

    if (zone && !(zone & TARGET_ZONE_PLAYERS))
      can_target_player_0 = can_target_player_1 = 0;
    else if (((who_chooses == HUMAN || (trace_mode & 2)) && (allowed_controller & 2)) ||
             (who_chooses == AI && (preferred_controller & 2)))
      can_target_player_0 = can_target_player_1 = 1;
    else if (((who_chooses == HUMAN || (trace_mode & 2)) && (allowed_controller & 1)) ||
             (who_chooses == AI && (preferred_controller & 1)))
    {
      can_target_player_0 = 0;
      can_target_player_1 = 1;
    }
    else
    {
      can_target_player_0 = 1;
      can_target_player_1 = 0;
    }

    if ((special & TARGET_SPECIAL_OPPONENT) && (player >= 0 ? player : who_chooses) == tgt_player)
      FAIL_STR(",opponent");

    if ((special & TARGET_SPECIAL_SAME_PLAYER) &&
        (player >= 0 ? player : who_chooses) != tgt_player)
      FAIL_STR(",player");
    if ((illegal_abilities & KEYWORD_SHROUD) &&
        ((player_bits[tgt_player] & PB_PLAYER_HAS_SHROUD) ||
         (who_chooses != tgt_player && (player_bits[tgt_player] & PB_PLAYER_HAS_HEXPROOF) &&
          !check_battlefield_for_id(1 - tgt_player, CARD_ID_KAYA_BANE_OF_THE_DEAD)) ||
         is_protected_from(tgt_player, -1, player, card,
                           (flags & 2) ? IPF_ATTACH_AURA : IPF_TARGET)))
      FAILURE(gs_target_error_abilities);

    if ((tgt_player == 0 && !can_target_player_0) || (tgt_player == 1 && !can_target_player_1))
      FAILURE(gs_target_error_player); //",player"
  }
  else
  {
    card_instance_t *tgt_instance = get_card_instance(tgt_player, tgt_card);

    if (tgt_instance->internal_card_id < 0 ||
        (tgt_instance->state & (STATE_OUBLIETTED | STATE_CANNOT_TARGET)))
      FAILURE(gs_target_error_cannot_target_this_prefixed); //",can't target this"

    target_zone_t in_zone = in_play(tgt_player, tgt_card)   ? TARGET_ZONE_IN_PLAY
                            : in_hand(tgt_player, tgt_card) ? TARGET_ZONE_HAND
                                                            : TARGET_ZONE_ON_STACK;
    if (in_zone == TARGET_ZONE_IN_PLAY &&
        !(cards_data[tgt_instance->internal_card_id].type &
          (TYPE_PERMANENT | TYPE_EFFECT))) // raw typecheck instead of is_what() is deliberate
      FAILURE(gs_target_error_cannot_target_this_prefixed); //",can't target this"	// A non-permanent spell, currently resolving.
                         //Probably the card doing the targeting.

    if (zone && !(zone & in_zone))
      FAILURE(gs_target_error_where); //",where"

    {
      int query_controller =
          (who_chooses == HUMAN || (trace_mode & 2)) ? allowed_controller : preferred_controller;
      int ok_controller;
      int ok_owner;

      if (extra == damage_card)
      {
        query_controller =
            (who_chooses == HUMAN || (trace_mode & 2))
                ? allowed_controller
                : (preferred_controller = ANYBODY); // AI can prevent damage to itself.
      }

      if (query_controller & TARGET_PLAYER_OWNER)
      {
        ok_owner = (query_controller & 2) ? -1 : (query_controller & 1) ? 1 : 0;
        ok_controller = ((query_controller & TARGET_PLAYER_OWNER_AND_CONTROLLER) ==
                         TARGET_PLAYER_OWNER_AND_CONTROLLER)
                            ? ok_owner
                            : -1;
      }
      else
      {
        ok_controller = (query_controller & 2) ? -1 : (query_controller & 1) ? 1 : 0;
        ok_owner = ((query_controller & TARGET_PLAYER_OWNER_AND_CONTROLLER) ==
                    TARGET_PLAYER_OWNER_AND_CONTROLLER)
                       ? ok_controller
                       : -1;
      }

      if (ok_controller != -1 && tgt_player != ok_controller)
        FAILURE(gs_target_error_controller); //",controller"

      if (ok_owner != -1 &&
          ((tgt_instance->state & STATE_OWNED_BY_OPPONENT) ? !ok_owner : ok_owner))
        FAILURE(gs_target_error_owner); //",owner"
    }

    if ((special & TARGET_SPECIAL_EFFECT_CARD) &&
        !(cards_data[tgt_instance->internal_card_id].type & TYPE_EFFECT))
      FAIL_STR("effect");

    if (required_type &&
        !target_match_type(tgt_instance, tgt_player, tgt_card, special, required_type))
      FAILURE(gs_target_error_type); //",type"

    if (illegal_type &&
        target_match_type(tgt_instance, tgt_player, tgt_card, special, illegal_type))
      FAILURE(gs_target_error_type); //",type"

    if (cards_data[tgt_instance->internal_card_id].code_pointer ==
            MANALINKEH_CARD_MULTIBLOCKER // multiblocker shadow
        && !(special & TARGET_SPECIAL_ALLOW_MULTIBLOCKER))
      FAILURE(gs_target_error_type); //",type"

    if (required_abilities &&
        (tgt_instance->regen_status & required_abilities) != (uint32_t)required_abilities)
      FAILURE(gs_target_error_abilities); //",abilities"

    if (illegal_abilities && tgt_instance->internal_card_id != damage_card &&
        in_zone == TARGET_ZONE_IN_PLAY)
    {
      if (tgt_instance->regen_status & (illegal_abilities & ~KEYWORD_SHROUD))
        FAILURE(gs_target_error_abilities); //",abilities"

      // A nonzero mode checks attachment restrictions instead of targeting.
      if (illegal_abilities & KEYWORD_SHROUD)
      {
        if (flags == 0)
        {
          if (is_protected_from(tgt_player, tgt_card, player, card, IPF_TARGET))
            FAILURE(gs_target_error_abilities);
          if ((tgt_instance->regen_status & KEYWORD_SHROUD) &&
              (get_id(tgt_player, tgt_card) != CARD_ID_AUTUMN_WILLOW ||
               tgt_instance->targets[1].player <= 0 ||
               !((uint32_t)tgt_instance->targets[1].player >> ((unsigned)who_chooses & 31) & 1)))
            FAILURE(gs_target_error_abilities);
          if (who_chooses != tgt_player &&
              check_for_special_ability(tgt_player, tgt_card, SP_KEYWORD_HEXPROOF) &&
              (!is_what(tgt_player, tgt_card, TYPE_CREATURE) ||
               !check_special_flags(tgt_player, tgt_card, SF_HEXPROOF_OVERRIDE)))
            FAILURE(gs_target_error_abilities);
        }
        else if (((flags & 2) &&
                  is_protected_from(tgt_player, tgt_card, player, card, IPF_ATTACH_AURA)) ||
                 ((flags & 4) &&
                  is_protected_from(tgt_player, tgt_card, player, card, IPF_ATTACH_EQUIPMENT)))
          FAILURE(gs_target_error_abilities);
      }
    }

    if ((required_color && !(required_color & get_color(tgt_player, tgt_card))) ||
        (illegal_color && (illegal_color & get_color(tgt_player, tgt_card))))
      FAILURE(gs_target_error_color); //",color"

    if (special & TARGET_SPECIAL_EXTRA_FUNCTION)
    {
      if (extra == -1 || extra == 0)
        backtrace_and_abort("functions/targets.c:450\nextra != -1 && extra != 0");

      typedef const char *(*Predicate)(int, int, int, int, int);
      STATIC_ASSERT(sizeof(intptr_t) == sizeof(Predicate), Predicate_type_must_fit_extra);

      Predicate predicate = (Predicate)extra;
      int p = player, c = card;
      if (p != -1 && c != -1)
      {
        card_instance_t *instance = get_card_instance(p, c);
        if (instance->internal_card_id == activation_card)
        {
          p = instance->parent_controller;
          c = instance->parent_card;
        }
      }
      const char *err = predicate(who_chooses, tgt_player, tgt_card, p, c);
      if (err)
        FAIL_STR(err);
    }
    else if (extra != -1)
    {
      if (special & TARGET_SPECIAL_CMC_LESSER_OR_EQUAL)
      {
        if (extra >= 0 && !(get_cmc(tgt_player, tgt_card) <= extra))
          FAILURE(",converted mana cost");
      }
      else if (special & TARGET_SPECIAL_CMC_GREATER_OR_EQUAL)
      {
        if (extra >= 0 && !(get_cmc(tgt_player, tgt_card) >= extra))
          FAILURE(",converted mana cost");
      }
      else if (special & TARGET_SPECIAL_NOT_LAND_SUBTYPE)
      {
        if (has_subtype(tgt_player, tgt_card,
                        extra == 0   ? SUBTYPE_SWAMP
                        : extra == 1 ? SUBTYPE_ISLAND
                        : extra == 2 ? SUBTYPE_FOREST
                        : extra == 3 ? SUBTYPE_MOUNTAIN
                                     : SUBTYPE_PLAINS))
          FAILURE(gs_target_error_subtype); //",subtype"
      }
      else if (special & TARGET_SPECIAL_BASIC_LAND)
      {
        if (!sub_47EF50(tgt_instance->internal_card_id, extra))
          FAILURE(gs_target_error_subtype); //",subtype"
      }
      else if (special & TARGET_SPECIAL_REQUIRES_COUNTER)
      {
        int counter_num = MAX(1, BYTE1(extra));
        int counter_type = BYTE0(extra) ? BYTE0(extra) : -1;
        if (count_counters(tgt_player, tgt_card, counter_type) < counter_num)
          FAIL_STR("must have counters");
      }
      else if (extra > 4)
      {
        int match = (tgt_instance->internal_card_id == extra ||
                     cards_data[tgt_instance->internal_card_id].id == cards_data[extra].id);
        if (special & TARGET_SPECIAL_EXTRA_NOT_IID)
        {
          if (match)
            FAILURE(gs_target_error_card_type); //",card type"
        }
        else if (!match)
          FAILURE(gs_target_error_card_type); //",card type"
      }
      else if (!has_subtype(tgt_player, tgt_card,
                            extra == 0   ? SUBTYPE_SWAMP
                            : extra == 1 ? SUBTYPE_ISLAND
                            : extra == 2 ? SUBTYPE_FOREST
                            : extra == 3 ? SUBTYPE_MOUNTAIN
                                         : SUBTYPE_PLAINS))
        FAILURE(gs_target_error_subtype); //",subtype"
    }

    if (required_subtype >= 0)
    {
      int is_required_subtype;
      if (required_subtype ==
          SUBTYPE_NONBASIC) // Special-cased since it isn't actually set on anything
        is_required_subtype =
            !is_basic_land(tgt_player, tgt_card) && is_what(tgt_player, tgt_card, TYPE_LAND);
      else if (required_subtype == SUBTYPE_LEGEND) // Special-cased due to Leyline of Singularity
        is_required_subtype = (is_legendary(tgt_player, tgt_card) ||
                               (!is_what(tgt_player, tgt_card, TYPE_LAND) &&
                                check_battlefield_for_id(2, CARD_ID_LEYLINE_OF_SINGULARITY)));
      else
        is_required_subtype = has_subtype(tgt_player, tgt_card, required_subtype);

      if (special & TARGET_SPECIAL_ILLEGAL_SUBTYPE)
      {
        if (is_required_subtype)
          FAILURE(gs_target_error_subtype); //",subtype"
      }
      else if (!is_required_subtype ||
               (required_subtype == SUBTYPE_WALL &&
                cards_data[tgt_instance->internal_card_id].id == CARD_ID_WALL_OF_SHADOWS &&
                !is_humiliated(tgt_player, tgt_card, AL_STATIC)))
        FAILURE(gs_target_error_subtype); //",subtype"
    }

    if (power_requirement != -1)
    {
      int digits = power_requirement & TARGET_PT_MASK;
      int comp = power_requirement & (TARGET_PT_LESSER_OR_EQUAL | TARGET_PT_GREATER_OR_EQUAL);

      int power = tgt_instance->power;

      if ((!comp && power != digits) ||
          (comp == TARGET_PT_GREATER_OR_EQUAL && (int16_t)power < digits) ||
          (comp == TARGET_PT_LESSER_OR_EQUAL && (int16_t)power > digits))
        FAILURE(gs_target_error_power); //",power"
    }

    if (toughness_requirement != -1)
    {
      int digits = toughness_requirement & TARGET_PT_MASK;
      int comp = toughness_requirement & (TARGET_PT_LESSER_OR_EQUAL | TARGET_PT_GREATER_OR_EQUAL);

      int toughness = tgt_instance->toughness;
      if (toughness_requirement & TARGET_PT_INCLUDE_DAMAGE)
        toughness -= tgt_instance->damage_on_card;

      if ((!comp && toughness != digits) ||
          (comp == TARGET_PT_GREATER_OR_EQUAL && (int16_t)toughness < digits) ||
          (comp == TARGET_PT_LESSER_OR_EQUAL && (int16_t)toughness > digits))
        FAILURE(gs_target_error_toughness); //",toughness"
    }

    if (special)
    {
      if ((special & TARGET_SPECIAL_WALL) &&
          (!has_subtype(tgt_player, tgt_card, SUBTYPE_WALL) ||
           (cards_data[tgt_instance->internal_card_id].id == CARD_ID_WALL_OF_SHADOWS &&
            !is_humiliated(tgt_player, tgt_card, AL_STATIC))))
        FAILURE(gs_target_error_walls); //",walls"

      if ((special & TARGET_SPECIAL_NON_WALL) && has_subtype(tgt_player, tgt_card, SUBTYPE_WALL))
        FAILURE(gs_target_error_walls); //",walls"

      if ((special & TARGET_SPECIAL_SPELL_ON_STACK) &&
          (card_on_stack_controller == -1 || card_on_stack_controller != tgt_player ||
           card_on_stack != tgt_card || (tgt_instance->state & STATE_JUST_CAST)))
        FAILURE(gs_target_error_spell); //",spell"

      if (special & TARGET_SPECIAL_STACK_RESTRICTION)
      {
        // Native code reads the top entry before checking stack_size.
        card_instance_t *top =
            get_card_instance(stack_cards[stack_size - 1].player, stack_cards[stack_size - 1].card);
        if ((int32_t)top->state < 0 || card_on_stack_controller == tgt_player ||
            card_on_stack == tgt_card || stack_size == 0 || (int32_t)tgt_instance->state < 0)
          FAILURE(gs_target_error_spell);
      }

      if ((special & TARGET_SPECIAL_BASIC_LAND) && !sub_47EF10(tgt_player, tgt_card))
        FAILURE(gs_target_error_basic_land); //",basic land"

      if ((special & TARGET_SPECIAL_ARTIFACT_CREATURE) &&
          (cards_data[tgt_instance->internal_card_id].type & (TYPE_ARTIFACT | TYPE_CREATURE)) !=
              (TYPE_ARTIFACT | TYPE_CREATURE))
        FAILURE(gs_target_error_artifact_creature); //",artifact creature"

      if (special & TARGET_SPECIAL_DAMAGE_PLAYER)
      {
        if (tgt_instance->damage_target_player != who_chooses ||
            tgt_instance->damage_target_card != -1 ||
            tgt_instance->targets[4].player > -1 || // Damaging planeswalker
            tgt_instance->targets[4].card > -1      // Damaging planeswalker
        )
        {
          FAILURE(gs_target_error_target_player); //",target player"
        }
      }

      if ((special & TARGET_SPECIAL_DAMAGE_CREATURE) && tgt_instance->damage_target_card == -1)
        FAIL_STR("must damage creature");

      if ((special & TARGET_SPECIAL_CMC_EQUAL) &&
          get_cmc(tgt_instance->damage_target_player, tgt_instance->damage_target_card) != extra)
      {
        char buffer[100];
        scnprintf(buffer, 100, "CMC must be %d", extra);
        FAIL_STR(buffer);
      }

      if ((special & TARGET_SPECIAL_ELEPHANT_MAMMOTH) &&
          !has_subtype(tgt_player, tgt_card, SUBTYPE_ELEPHANT))
        FAILURE(gs_target_error_elephant_or_mammoth); //",elephant/mammoth"

      if ((special & TARGET_SPECIAL_DJINN_OR_EFREET) &&
          !has_subtype(tgt_player, tgt_card, SUBTYPE_EFREET) &&
          !has_subtype(tgt_player, tgt_card, SUBTYPE_DJINN))
        FAILURE(gs_target_error_djinn_or_efreet); //",djinn/efreet"

      if (special & TARGET_SPECIAL_NOT_ME)
      {
        if (player == -1 || card == -1)
          backtrace_and_abort("functions/targets.c:644\nplayer != -1 && card != -1");
        if (player == tgt_player && card == tgt_card)
          FAIL_STR("cannot target self");
      }
    }

    // Spells and abilities on the stack can't target themslves - 114.4
    if (in_zone == TARGET_ZONE_ON_STACK &&
        ((card == tgt_card && player == tgt_player // spell
          && !(special & TARGET_SPECIAL_NOT_ME))   // avoid repeating message
         ||
         (tgt_instance->internal_card_id == activation_card && card == tgt_instance->parent_card &&
          player == tgt_instance->parent_controller)) // ability
        && player != -1 && card != -1)
      FAIL_STR(",cannot target self");

    if (required_state)
    {
      if ((required_state & TARGET_STATE_TAPPED_OR_BLOCKING) &&
          !(tgt_instance->state & (STATE_TAPPED | STATE_BLOCKING)))
        FAILURE(gs_target_error_tapped); //",tapped"

      if (required_state & TARGET_STATE_DESTROYED)
      {
        if (tgt_instance->kill_code != KILL_DESTROY)
          FAILURE(gs_target_error_destroyed); //",destroyed"

        if ((special & TARGET_SPECIAL_REGENERATION) &&
            (tgt_instance->token_status & STATUS_CANNOT_REGENERATE))
          FAIL_STR("cannot be regenerated");
      }

      if ((required_state & TARGET_STATE_TAPPED) && !(tgt_instance->state & STATE_TAPPED))
        FAILURE(gs_target_error_tapped); //",tapped"

      if ((required_state & TARGET_STATE_ATTACKING) && !(tgt_instance->state & STATE_ATTACKING))
        FAILURE(gs_target_error_attacking); //",attacking"

      if ((required_state & TARGET_STATE_ATTACKED) && !(tgt_instance->state & STATE_ATTACKED))
        FAILURE(gs_target_error_attacked); //",attacked"

      if ((required_state & TARGET_STATE_ISBLOCKED) && !(tgt_instance->state & STATE_ISBLOCKED))
        FAILURE(gs_target_error_blocked); //",blocked"

      if ((required_state & TARGET_STATE_BLOCKING) &&
          (tgt_player == current_turn || !(tgt_instance->state & STATE_BLOCKING) ||
           tgt_instance->blocking == 255))
        FAILURE(gs_target_error_blocking); //",blocking"

      if (required_state & TARGET_STATE_IN_COMBAT)
      {
        int not_blocking = 0;
        int not_attacking = 0;

        if (!(tgt_instance->state & STATE_ATTACKING))
          not_attacking = 1;

        if (current_phase <= PHASE_MAIN1 || current_phase >= PHASE_MAIN2 ||
            tgt_player == current_turn || tgt_instance->blocking == 255)
          not_blocking = 1;

        if (not_attacking && not_blocking)
          FAILURE(gs_target_error_attacking_or_blocking); //",attacking/blocking"
      }

      if (required_state & TARGET_STATE_ENCHANTED)
      {
        int p, c;
        for (p = 0; p < 2; ++p)
          for (c = 0; c < active_cards_count[p]; ++c)
          {
            card_instance_t *aura_inst = get_card_instance(p, c);
            if (aura_inst->internal_card_id != -1 && is_what(p, c, TYPE_ENCHANTMENT) &&
                aura_inst->damage_target_player == tgt_player &&
                aura_inst->damage_target_card == tgt_card)
              goto is_enchanted;
          }
        // else
        FAILURE(gs_target_error_enchanted); //",enchanted"
      is_enchanted:;
      }

      if ((required_state & TARGET_STATE_JUST_CAST) && !(tgt_instance->state & STATE_JUST_CAST))
        FAILURE(gs_target_error_casted); //",casted"

      if ((required_state & TARGET_STATE_SPELL_RESOLVED) &&
          (!(tgt_instance->state & STATE_JUST_CAST) || (tgt_instance->state & STATE_INVISIBLE)))
        FAILURE(gs_target_error_cast_resolved); //",cast resolved"

      if ((required_state & TARGET_STATE_DAMAGED) && !sub_47E980(tgt_player, tgt_card))
        FAILURE(gs_target_error_damaged); //",damaged"

      if ((required_state & TARGET_STATE_COULD_UNTAP) && !(tgt_instance->untap_status & 1))
        FAILURE(gs_target_error_can_untap); //",can untap"

      if ((required_state & TARGET_STATE_WILL_UNTAP) && !(tgt_instance->untap_status & 2))
        FAILURE(gs_target_error_will_untap); //",will untap"

      if ((required_state & TARGET_STATE_SUMMONING_SICK) &&
          !tgt_instance->ext->host_summoning_sickness)
        FAILURE(gs_target_error_summoning_sickness); //",summoning sickness
    }

      if ((required_state & TARGET_STATE_SUMMONSICK_NOTAP) && !(tgt_instance->state & STATE_SUMMONSICK_NOTAP))
        FAILURE(gs_target_error_summoning_sickness);

    if (illegal_state)
    {
      if (illegal_state & TARGET_STATE_DESTROYED)
        backtrace_and_abort("functions/targets.c:761\n!(illegal_state & TARGET_STATE_DESTROYED)");
      if ((illegal_state & TARGET_STATE_TAPPED) && (tgt_instance->state & STATE_TAPPED))
        FAILURE(gs_target_error_tapped); //",tapped"

      if ((illegal_state & (TARGET_STATE_ATTACKING | TARGET_STATE_IN_COMBAT)) &&
          (tgt_instance->state & STATE_ATTACKING))
        FAILURE(gs_target_error_attacking); //",attacking"

      if ((illegal_state & TARGET_STATE_ATTACKED) && (tgt_instance->state & STATE_ATTACKED))
        FAILURE(gs_target_error_attacked); //",attacked"

      if ((illegal_state & TARGET_STATE_ISBLOCKED) && (tgt_instance->state & STATE_ISBLOCKED))
        FAILURE(gs_target_error_blocked); //",blocked"

      if ((illegal_state & (TARGET_STATE_BLOCKING | TARGET_STATE_IN_COMBAT)) &&
          tgt_instance->blocking != 255 && tgt_player != current_turn)
        FAILURE(gs_target_error_blocking); //",blocking"

      if (illegal_state & TARGET_STATE_ENCHANTED)
      {
        int p, c;
        for (p = 0; p < 2; ++p)
          for (c = 0; c < active_cards_count[p]; ++c)
          {
            card_instance_t *aura_inst = get_card_instance(p, c);
            if (aura_inst->internal_card_id != -1 && is_what(p, c, TYPE_ENCHANTMENT) &&
                aura_inst->damage_target_player == tgt_player &&
                aura_inst->damage_target_card == tgt_card)
              FAILURE(gs_target_error_enchanted); //",enchanted"
          }
      }

      if ((illegal_state & TARGET_STATE_JUST_CAST) && (tgt_instance->state & STATE_JUST_CAST))
        FAILURE(gs_target_error_casted); //",casted"

      if ((illegal_state & TARGET_STATE_SPELL_RESOLVED) &&
          (tgt_instance->state & STATE_JUST_CAST) && !(tgt_instance->state & STATE_INVISIBLE))
        FAILURE(gs_target_error_cast_resolved); //",cast resolved"

      if ((illegal_state & TARGET_STATE_DAMAGED) && sub_47E980(tgt_player, tgt_card))
        FAILURE(gs_target_error_damaged); //",damaged"

      if ((illegal_state & TARGET_STATE_COULD_UNTAP) && (tgt_instance->untap_status & 1))
        FAILURE(gs_target_error_can_untap); //",can untap"

      if ((illegal_state & TARGET_STATE_WILL_UNTAP) && (tgt_instance->untap_status & 2))
        FAILURE(gs_target_error_will_untap); //",will untap"

      if ((illegal_state & TARGET_STATE_SUMMONING_SICK) &&
          tgt_instance->ext->host_summoning_sickness &&
          (cards_data[tgt_instance->internal_card_id].type & TYPE_CREATURE))
        FAILURE(gs_target_error_summoning_sickness); //",summoning sickness"
      if ((illegal_state & TARGET_STATE_SUMMONSICK_NOTAP) && (tgt_instance->state & STATE_SUMMONSICK_NOTAP) &&
          (cards_data[tgt_instance->internal_card_id].type & TYPE_CREATURE))
        FAILURE(gs_target_error_summoning_sickness);
    }
  }

epilog:
  if (return_error_str)
  {
    if (error_str[0])
      strcpy(return_error_str, &error_str[1]); // skipping the initial comma
    else
      *return_error_str = 0;
  }

  return rval;
#undef FAIL_STR
#undef FAILURE
}

/* ManalinkEh 2022: 0x265fcf0; original functions/targets.c:840. */
int real_validate_target(int tgt_player, int tgt_card, char *return_error_str, int who_chooses,
                         int allowed_controller, int preferred_controller, int zone,
                         int required_type, int illegal_type, int required_abilities,
                         int illegal_abilities, int required_color, int illegal_color, intptr_t extra,
                         int required_subtype, int power_requirement, int toughness_requirement,
                         int special, int required_state, int illegal_state)
{
  int source_player = hack_gpf_player;
  int source_card = hack_gpf_card;
  unsigned int flags = 0;
  if (!(illegal_abilities & KEYWORD_SHROUD))
    source_player = source_card = -1;
  else if (source_player >= 0 && source_card >= 0)
  {
    if (has_subtype(source_player, source_card, SUBTYPE_AURA))
      flags = 2;
    if (has_subtype(source_player, source_card, SUBTYPE_EQUIPMENT))
      flags |= 4;
  }
  return validate_target_impl(
      source_player, source_card, tgt_player, tgt_card, return_error_str, who_chooses,
      allowed_controller, preferred_controller, zone, required_type, illegal_type,
      required_abilities, illegal_abilities, required_color, illegal_color, extra, required_subtype,
      power_requirement, toughness_requirement, special, required_state, illegal_state, flags);
}

/* ManalinkEh 2022: 0x265ef60; original functions/targets.c:875. */
static int target_available_impl(int player, int card, int *return_count,
    int test_damage_target_or_source, int who_chooses, int allowed_controller,
    int preferred_controller, int zone, int required_type, int illegal_type,
    int required_abilities, int illegal_abilities, int required_color, int illegal_color,
    intptr_t extra, int required_subtype, int power_requirement, int toughness_requirement,
    int special, int required_state, int illegal_state, int flags)
{
  if (g_ai_search_flags & 1)
    who_chooses = test_damage_target_or_source;
  int count = 0;
  for (int tgt_player = 0; tgt_player < 2; ++tgt_player)
    if (validate_target_impl(player, card, tgt_player, -1, NULL, who_chooses,
        allowed_controller, preferred_controller, zone, required_type, illegal_type,
        required_abilities, illegal_abilities, required_color, illegal_color, extra,
        required_subtype, power_requirement, toughness_requirement, special,
        required_state, illegal_state, flags))
    {
      if (!return_count)
        return 1;
      ++count;
    }
  int controller = who_chooses == HUMAN || (trace_mode & 2) ?
      !(allowed_controller & 2) : (preferred_controller & 3) != 0;
  for (int pass = 0; pass < 2; ++pass, controller ^= 1)
  {
    int scan_limit = active_cards_count[0];
    if (active_cards_count[1] > scan_limit)
      scan_limit = active_cards_count[1];
    for (int candidate = 0; candidate < scan_limit; ++candidate)
    {
      card_instance_t *instance = get_card_instance(controller, candidate);
      if (instance->internal_card_id == -1)
        continue;
      int tgt_player = controller;
      int tgt_card = candidate;
      if (test_damage_target_or_source)
      {
        if (instance->internal_card_id != damage_card)
          continue;
        if (test_damage_target_or_source == 1)
        {
          tgt_player = instance->damage_target_player;
          tgt_card = instance->damage_target_card;
        }
        else
        {
          tgt_player = instance->damage_source_player;
          tgt_card = instance->damage_source_card;
        }
      }
      if (validate_target_impl(player, card, tgt_player, tgt_card, NULL, who_chooses,
          allowed_controller, preferred_controller, zone, required_type, illegal_type,
          required_abilities, illegal_abilities, required_color, illegal_color, extra,
          required_subtype, power_requirement, toughness_requirement, special,
          required_state, illegal_state, flags))
      {
        if (!return_count)
          return 1;
        ++count;
      }
    }
  }
  if (return_count)
    *return_count = count;
  return count > 0;
}

/* ManalinkEh 2022: 0x265fe80; original functions/targets.c:970. */
int real_target_available(int *return_count, int test_damage_target_or_source, int who_chooses,
                          int allowed_controller, int preferred_controller, int zone,
                          int required_type, int illegal_type, int required_abilities,
                          int illegal_abilities, int required_color, int illegal_color, intptr_t extra,
                          int required_subtype, int power_requirement, int toughness_requirement,
                          int special, int required_state, int illegal_state)
{
  int source_player = hack_gpf_player;
  int source_card = hack_gpf_card;
  unsigned int flags = 0;
  if (!(illegal_abilities & KEYWORD_SHROUD))
    source_player = source_card = -1;
  else if (source_player >= 0 && source_card >= 0)
  {
    if (has_subtype(source_player, source_card, SUBTYPE_AURA))
      flags = 2;
    if (has_subtype(source_player, source_card, SUBTYPE_EQUIPMENT))
      flags |= 4;
  }
  if ((unsigned int)test_damage_target_or_source < 3)
  {
    return target_available_impl(
        source_player, source_card, return_count, test_damage_target_or_source, who_chooses,
        allowed_controller, preferred_controller, zone, required_type, illegal_type,
        required_abilities, illegal_abilities, required_color, illegal_color, extra,
        required_subtype, power_requirement, toughness_requirement, special, required_state,
        illegal_state, flags);
  }
  return 0;
}

#endif

#ifndef SHANDALAR

/* ManalinkEh 2022: 0x265f1f0; original functions/targets.c:1003. */
static int target_available_maybe_force(
    int player, int card, int *return_count, int test_damage_target_or_source,
    int who_chooses, int allowed_controller, int preferred_controller, int zone,
    int required_type, int illegal_type, int required_abilities, int illegal_abilities,
    int required_color, int illegal_color, intptr_t extra, int required_subtype, int power_requirement,
    int toughness_requirement, int special, int required_state, int illegal_state, int flags,
    int allow_cancel)
{
  int result;

  result = target_available_impl(
      player, card, return_count, test_damage_target_or_source, who_chooses, allowed_controller, preferred_controller, zone,
      required_type, illegal_type, required_abilities, illegal_abilities, required_color,
      illegal_color, extra, required_subtype, power_requirement, toughness_requirement, special,
      required_state, illegal_state, flags);
  if (((result == 0) && ((allow_cancel & 1U) == 0)) &&
      ((who_chooses == 1 || (ai_is_speculating == 1))))
  {
    result = 0;
    if ((allowed_controller & 2U) != 0)
    {
      if ((preferred_controller & 2U) == 0)
      {
        result = target_available_impl(
            player, card, return_count, test_damage_target_or_source, who_chooses, allowed_controller,
            preferred_controller ^ 1, zone, required_type, illegal_type, required_abilities,
            illegal_abilities, required_color, illegal_color, extra, required_subtype,
            power_requirement, toughness_requirement, special, required_state, illegal_state,
            flags);
        return result;
      }
      result = 0;
    }
  }
  return result;
}

/* ManalinkEh 2022: 0x265f3d0; original functions/targets.c:1042. */
static int select_target_impl(int player, int card, int who_chooses, int allowed_controller,
                              int preferred_controller, int zone, int required_type,
                              int illegal_type, int required_abilities, int illegal_abilities,
                              int required_color, int illegal_color, intptr_t extra,
                              int required_subtype, int power_requirement,
                              int toughness_requirement, int special, int required_state,
                              int illegal_state, int flags, const char *prompt, int allow_cancel,
                              target_t *ret_tgt)
{
  if ((who_chooses != 1 || trace_mode & 2) && ai_is_speculating != 1)
  {
    int can_target_player_0, can_target_player_1, hwnd_SpellMinimized_is_visible, rval;

    if (zone && !(zone & TARGET_ZONE_PLAYERS))
      can_target_player_0 = can_target_player_1 = 0;
    else if (allowed_controller & 2)
      can_target_player_0 = can_target_player_1 = 1;
    else if (allowed_controller & 1)
    {
      can_target_player_0 = 0;
      can_target_player_1 = 1;
    }
    else
    {
      can_target_player_0 = 1;
      can_target_player_1 = 0;
    }

    // If can target a spell or ability on the stack, and there's at least one there besides this,
    // don't minimize the spellchain window.
    if ((zone & TARGET_ZONE_ON_STACK) &&
        (stack_size > 1 ||
         (stack_size == 1 && !(stack_cards[0].card == card && stack_cards[0].player == player))))
      hwnd_SpellMinimized_is_visible = 1;
    else
      hwnd_SpellMinimized_is_visible = sub_45CEC0();

    target_t tgt = {-1, -3};
    target_t tmp_tgt;
    int selection_status;

    if (prompt && hack_prepend_prompt && ai_is_speculating != 1)
    {
      sprintf((char *)global_all_purpose_buffer, "%s: %s", hack_prepend_prompt, prompt);
      prompt = global_all_purpose_buffer;
    }
    else if (!prompt)
      prompt = global_all_purpose_buffer; // global_all_purpose_buffer

    while (1)
    {

      rval = sub_4378E0(who_chooses, -1, prompt, allow_cancel, -1, -1, -1, -1, &selection_status,
                        &tmp_tgt, can_target_player_1, can_target_player_0);
      if (rval)
      {
        card_instance_t *instance;
        if (tmp_tgt.card < 0 ||
            (instance = get_card_instance(tmp_tgt.player, tmp_tgt.card))->internal_card_id < 0 ||
            (special & TARGET_SPECIAL_ALLOW_MULTIBLOCKER) ||
            cards_at_7c7000[instance->internal_card_id]->code_pointer !=
                MANALINKEH_CARD_MULTIBLOCKER) // multiblocker card
        {
          char err_reason[208];

          if (validate_target_impl(player, card, tmp_tgt.player, tmp_tgt.card, err_reason,
                                   who_chooses, allowed_controller, preferred_controller, zone,
                                   required_type, illegal_type, required_abilities,
                                   illegal_abilities, required_color, illegal_color, extra,
                                   required_subtype, power_requirement, toughness_requirement,
                                   special, required_state, illegal_state, flags))
          {
            tgt = tmp_tgt;
            break;
          }

          if (ai_is_speculating != 1)
          {
            if (*err_reason)
            {
              char illegal_tgt[200];
              sprintf(illegal_tgt, gs_illegal_target_format /*"Illegal target (%s)."*/, err_reason);
              display_error_message(illegal_tgt);
            }
            else
              display_error_message(gs_illegal_target /*"Illegal target."*/);
          }
        }
        else if (ai_is_speculating != 1)
        {
          char illegal_tgt[200];
          sprintf(illegal_tgt, gs_illegal_target_format /*"Illegal target (%s)."*/,
                  gs_target_error_type /*",type"*/);
          display_error_message(illegal_tgt);
        }

        continue;
      }

      if (selection_status != -2)
        continue;

      if (g_stop_phase_player == -1 && g_stop_phase == -1)
      {
        tgt = tmp_tgt;
        break;
      }

      if (zone & TARGET_ZONE_0x2000)
        break;
    }

    *ret_tgt = tgt;
    if (!hwnd_SpellMinimized_is_visible)
      sub_45CF00();
    if (rval)
      g_ai_action_replay_available = 0;
    set_centerwindow_txt("");
    text_lines[0][0] = 0;
    return rval;
  }
  else
  {
    int can_target_player_0, can_target_player_1, num_candidates, p, c;
    target_t candidates[60];

    if ((g_ai_search_flags & 1) && (allowed_controller & 2))
      preferred_controller = allowed_controller;

    if (zone && !(zone & TARGET_ZONE_PLAYERS))
      can_target_player_0 = can_target_player_1 = 0;
    else if (preferred_controller & 2)
      can_target_player_0 = can_target_player_1 = 1;
    else if (preferred_controller & 1)
    {
      can_target_player_0 = 0;
      can_target_player_1 = 1;
    }
    else
    {
      can_target_player_0 = 1;
      can_target_player_1 = 0;
    }

    if (spell_fizzled == 1)
      return 0;

    num_candidates = 0;

    for (p = 0; p < 2 && num_candidates < 58; ++p)
      for (c = 0; c < active_cards_count[p] && num_candidates < 58; ++c)
      {
        card_instance_t *instance = get_card_instance(p, c);
        if (instance->internal_card_id != -1 &&
            validate_target_impl(
                player, card, p, c, NULL, who_chooses, allowed_controller, preferred_controller,
                zone, required_type, illegal_type, required_abilities, illegal_abilities,
                required_color, illegal_color, extra, required_subtype, power_requirement,
                toughness_requirement, special, required_state, illegal_state, flags))
        {
          candidates[num_candidates].player = p;
          candidates[num_candidates].card = c;
          ++num_candidates;
        }
      }

    if (can_target_player_1)
    {
      candidates[num_candidates].player = 1;
      candidates[num_candidates].card = -1;
      ++num_candidates;
    }

    if (can_target_player_0)
    {
      candidates[num_candidates].player = 0;
      candidates[num_candidates].card = -1;
      ++num_candidates;
    }

    if (num_candidates == 0)
      return 0;

    g_ai_recorded_action_type = 3;

    if (ai_is_speculating == 1)
    {
      g_ai_recorded_choice = internal_rand(num_candidates);
      g_ai_recorded_action = (candidates[g_ai_recorded_choice].player < 1 ? 0 : 0x100) |
                            (candidates[g_ai_recorded_choice].card & 0xff) | 0x4000;

      sub_498F20();
    }
    else
    {
      sub_499050();

      if (g_ai_recorded_choice == 99 || num_candidates <= g_ai_recorded_choice)
        g_ai_recorded_choice = internal_rand(num_candidates);
    }

    g_target_player_choice = candidates[g_ai_recorded_choice].player;
    *ret_tgt = candidates[g_ai_recorded_choice];
    return 1;
  }
}

/* ManalinkEh 2022: 0x265fb20; original functions/targets.c:1258. */
static int select_target_maybe_force(
    int player, int card, int who_chooses, int allowed_controller, int preferred_controller,
    int zone, int required_type, int illegal_type, int required_abilities, int illegal_abilities,
    int required_color, int illegal_color, intptr_t extra, int required_subtype, int power_requirement,
    int toughness_requirement, int special, int required_state, int illegal_state, int flags,
    const char *prompt, int allow_cancel, target_t *ret_tgt)
{
  if (duel_host_scripted_input())
  {
#ifdef MANALINKEH_STATIC
    int probe_player,probe_card;
    if (duel_host_target_probe_candidate(&probe_player,&probe_card)) {
      /* Read the real selector predicate, then stop this isolated inspection.
       * The caller restores its complete snapshot; no target is admitted. */
      int legal=validate_target_impl(player,card,probe_player,probe_card,NULL,
          who_chooses,allowed_controller,allowed_controller,zone,
          required_type,illegal_type,required_abilities,illegal_abilities,
          required_color,illegal_color,extra,required_subtype,power_requirement,
          toughness_requirement,special,required_state,illegal_state,flags);
      duel_host_observe_target_legal(legal);
      return 0;
    }
#endif
    int target_card;
    int target_player = duel_host_choose(DUEL_CHOICE_TARGET, who_chooses, prompt, 0, NULL, &target_card);
    /* Both Cancel (1) and Done (2) end this target selection. The caller
       decides whether an empty result cancels the action or stops an optional
       target list, exactly as it does for the normal selector. */
    if (target_player == -1 && (allow_cancel & 3)) return 0;
    if (target_player < 0 || target_player > 1 || target_card < -1 || target_card >= 150 ||
        !validate_target_impl(player, card, target_player, target_card, NULL,
          who_chooses, allowed_controller, allowed_controller, zone,
          required_type, illegal_type, required_abilities, illegal_abilities,
          required_color, illegal_color, extra, required_subtype,
          power_requirement, toughness_requirement, special, required_state, illegal_state, flags))
      duel_host_fail("illegal scripted target");
    ret_tgt->player = target_player; ret_tgt->card = target_card;
    return 1;
  }

  int result;

  result = select_target_impl(
      player, card, who_chooses, allowed_controller, preferred_controller, zone, required_type,
      illegal_type, required_abilities, illegal_abilities, required_color, illegal_color, extra,
      required_subtype, power_requirement, toughness_requirement, special, required_state,
      illegal_state, flags, prompt, allow_cancel, ret_tgt);
  if ((result == 0) && ((allow_cancel & 1U) == 0))
  {
    if (((who_chooses == AI) || (ai_is_speculating == 1)) &&
        (allowed_controller & 2U) && !(preferred_controller & 2U))
    {
      result = select_target_impl(
          player, card, who_chooses, allowed_controller, preferred_controller ^ 1, zone,
          required_type, illegal_type, required_abilities, illegal_abilities, required_color,
          illegal_color, extra, required_subtype, power_requirement, toughness_requirement, special,
          required_state, illegal_state, flags, prompt, allow_cancel, ret_tgt);
      if (result != 0)
      {
        ai_modifier = ai_modifier + -0x20;
      }
    }
  }
  return result;
}

/* ManalinkEh 2022: 0x2660010; original functions/targets.c:1295. */
int real_select_target(int who_chooses, int allowed_controller, int preferred_controller, int zone,
                       int required_type, int illegal_type, int required_abilities,
                       int illegal_abilities, int required_color, int illegal_color, intptr_t extra,
                       int required_subtype, int power_requirement, int toughness_requirement,
                       int special, int required_state, int illegal_state, const char *prompt,
                       int allow_cancel, target_t *ret_tgt)
{
  int source_player = hack_gpf_player;
  int source_card = hack_gpf_card;
  unsigned int flags = 0;
  if (!(illegal_abilities & KEYWORD_SHROUD))
    source_player = source_card = -1;
  else if (source_player >= 0 && source_card >= 0)
  {
    if (has_subtype(source_player, source_card, SUBTYPE_AURA))
      flags = 2;
    if (has_subtype(source_player, source_card, SUBTYPE_EQUIPMENT))
      flags |= 4;
  }
  return select_target_maybe_force(
      source_player, source_card, who_chooses, allowed_controller, preferred_controller, zone,
      required_type, illegal_type, required_abilities, illegal_abilities, required_color,
      illegal_color, extra, required_subtype, power_requirement, toughness_requirement, special,
      required_state, illegal_state, flags, prompt, allow_cancel, ret_tgt);
}

/* ManalinkEh 2022: 0x26601a0; original functions/targets.c:1332. */
void base_target_definition(int player, int card, target_definition_t *td, int type)
{
  td->who_chooses = player;
  td->preferred_controller = 1 - player;
  td->player = player;
  td->allowed_controller = 2;
  td->zone = 0x200;
  td->required_type = type;
  td->illegal_type = 0;
  td->required_abilities = 0;
  td->illegal_abilities = 0;
  td->required_color = 0;
  td->illegal_color = 0;
  td->extra = -1;
  td->required_subtype = -1;
  td->power_requirement = -1;
  td->toughness_requirement = -1;
  td->special = 0;
  td->required_state = 0;
  td->illegal_state = 0;
  td->allow_cancel = 1;
  td->card = card;
  td->flags = 0;
  return;
}

/* ManalinkEh 2022: 0x2660240; original functions/targets.c:1357. */
void default_target_definition(int player, int card, target_definition_t *td, int type)
{
  int result;
  td->preferred_controller = 1 - player;
  td->who_chooses = player;
  td->allowed_controller = 2;
  td->zone = 0x200;
  td->required_type = type;
  td->illegal_type = 0;
  td->required_abilities = 0;
  td->illegal_abilities = 0;
  td->required_color = 0;
  td->illegal_color = 0;
  td->extra = -1;
  td->required_subtype = -1;
  td->power_requirement = -1;
  td->toughness_requirement = -1;
  td->special = 0;
  td->required_state = 0;
  td->illegal_state = 0;
  td->allow_cancel = 1;
  td->player = player;
  td->card = card;
  td->flags = 0;
  if ((player != -1) && (card != -1))
  {
    result = get_protections_from_part(player, card);
    td->illegal_abilities = result;
    return;
  }
  td->illegal_abilities = 0;
  return;
}

/* ManalinkEh 2022: 0x2660300; original functions/targets.c:1364. */
void opponent_or_pw_target_definition(int player, int card, target_definition_t *td)
{
  default_target_definition(player, card, td, 0x100);
  td->zone = 0x1200;
  td->special = 0x2000000;
  return;
}

/* ManalinkEh 2022: 0x2660340; original functions/targets.c:1371. */
void counterspell_target_definition(int player, int card, target_definition_t *td,
                                         int type)
{
  td->who_chooses = player;
  td->preferred_controller = 1 - player;
  td->player = player;
  td->allowed_controller = 2;
  td->required_type = type;
  td->illegal_type = 0;
  td->required_abilities = 0;
  td->illegal_abilities = 0;
  td->required_color = 0;
  td->illegal_color = 0;
  td->extra = -1;
  td->required_subtype = -1;
  td->power_requirement = -1;
  td->toughness_requirement = -1;
  td->required_state = 0;
  td->illegal_state = 0;
  td->allow_cancel = 1;
  td->card = card;
  td->flags = 0;
  td->zone = 0;
  td->special = 2;
  return;
}

/* ManalinkEh 2022: 0x26603e0; original functions/targets.c:1378. */
void counter_activated_target_definition(int player, int card, target_definition_t *td,
                                              int type)
{
  td->who_chooses = player;
  td->preferred_controller = 1 - player;
  td->player = player;
  td->allowed_controller = 2;
  td->required_type = type;
  td->illegal_type = 0;
  td->required_abilities = 0;
  td->illegal_abilities = 0;
  td->required_color = 0;
  td->illegal_color = 0;
  td->extra = -1;
  td->required_subtype = -1;
  td->power_requirement = -1;
  td->toughness_requirement = -1;
  td->required_state = 0;
  td->illegal_state = 0;
  td->allow_cancel = 1;
  td->card = card;
  td->flags = 0;
  td->zone = 0;
  td->special = 0x20000;
  return;
}

/* ManalinkEh 2022: 0x2660480; original functions/targets.c:1385. */
void counter_triggered_target_definition(int player, int card, target_definition_t *td,
                                              int type)
{
  default_target_definition(player, card, td, type);
  td->zone = 0;
  td->special = 0x8000000;
  return;
}

/* ManalinkEh 2022: 0x26604c0; original functions/targets.c:1393. */
int can_target(target_definition_t *td)
{
  int result;
#ifdef MANALINKEH_STATIC
  int probe_player,probe_card;
  if (duel_host_target_probe_candidate(&probe_player,&probe_card)) {
    int saved_card=target_card,saved_controller=target_controller;
    duel_host_observe_target_legal(validate_arbitrary_target(td,probe_player,probe_card));
    target_card=saved_card; target_controller=saved_controller;
  }
#endif
  result = target_available_maybe_force(td->player, td->card, NULL, 0, td->who_chooses, td->allowed_controller,
      td->preferred_controller, td->zone, td->required_type, td->illegal_type,
      td->required_abilities, td->illegal_abilities, td->required_color, td->illegal_color,
      td->extra, td->required_subtype, td->power_requirement, td->toughness_requirement, td->special,
      td->required_state, td->illegal_state, td->flags, td->allow_cancel);
  return result;
}

/* ManalinkEh 2022: 0x2660570; original functions/targets.c:1419. */
int target_available(int player, int card, target_definition_t *td)
{
  int count;
  count = 0;
  target_available_maybe_force(player, card, &count, 0, td->who_chooses, td->allowed_controller,
      td->preferred_controller, td->zone, td->required_type, td->illegal_type,
      td->required_abilities, td->illegal_abilities, td->required_color, td->illegal_color,
      td->extra, td->required_subtype, td->power_requirement, td->toughness_requirement, td->special,
      td->required_state, td->illegal_state, td->flags, td->allow_cancel);
  return count;
}

/* ManalinkEh 2022: 0x2660620; original functions/targets.c:1700. */
int pick_next_target_noload_arbitrary(target_definition_t *td, const char *prompt,
                                           int player, int card)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  result = select_target_maybe_force(
      td->player, td->card, td->who_chooses, td->allowed_controller, td->preferred_controller,
      td->zone, td->required_type, td->illegal_type, td->required_abilities, td->illegal_abilities,
      td->required_color, td->illegal_color, td->extra, td->required_subtype, td->power_requirement,
      td->toughness_requirement, td->special, td->required_state, td->illegal_state, td->flags,
      prompt, td->allow_cancel, instance->targets + instance->number_of_targets);
  if (result != 0)
  {
    instance->number_of_targets = instance->number_of_targets + '\x01';
    return result;
  }
  spell_fizzled = 1;
  return 0;
}

/* ManalinkEh 2022: 0x2660710; original functions/targets.c:1692. */
int pick_next_target_arbitrary(target_definition_t *td, const char *prompt, int player,
                                    int card)
{
  int result;
  load_text(NULL, prompt);
  result = pick_next_target_noload_arbitrary(td, text_lines[0], player, card);
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2660750; original functions/targets.c:1895. */
int would_validate_arbitrary_target(target_definition_t *td, int tgt_player, int tgt_card)
{
  int result;
  target_card = tgt_card;
  target_controller = tgt_player;
  result = validate_target_impl(
      td->player, td->card, tgt_player, tgt_card, NULL, td->who_chooses, td->allowed_controller,
      td->preferred_controller, td->zone, td->required_type, td->illegal_type,
      td->required_abilities, td->illegal_abilities, td->required_color, td->illegal_color,
      td->extra, td->required_subtype, td->power_requirement, td->toughness_requirement,
      td->special, td->required_state, td->illegal_state, td->flags);
  return result;
}

/* ManalinkEh 2022: 0x2660810; original functions/targets.c:1888. */
int validate_arbitrary_target(target_definition_t *td, int tgt_player, int tgt_card)
{
  int32_t saved_preferred;
  int result;

  saved_preferred = td->preferred_controller;
  td->preferred_controller = td->allowed_controller;
  result = would_validate_arbitrary_target(td, tgt_player, tgt_card);
  td->preferred_controller = saved_preferred;
  return result;
}

/* ManalinkEh 2022: 0x2660850; original functions/targets.c:1915. */
int would_validate_target(int player, int card, target_definition_t *td,
                               int target_number)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if ((int)(unsigned int)instance->number_of_targets <= target_number)
  {
    target_number = 0;
  }
  target_controller = instance->targets[target_number].player;
  target_card = instance->targets[target_number].card;
  result = validate_target_impl(
      player, card, target_controller, target_card, NULL, td->who_chooses, td->allowed_controller,
      td->preferred_controller, td->zone, td->required_type, td->illegal_type,
      td->required_abilities, td->illegal_abilities, td->required_color, td->illegal_color,
      td->extra, td->required_subtype, td->power_requirement, td->toughness_requirement,
      td->special, td->required_state, td->illegal_state, td->flags);
  return result;
}

/* ManalinkEh 2022: 0x2660930; original functions/targets.c:1447. */
int choose_default_target(int player, int card, target_definition_t *td)
{
  card_instance_t *instance = get_card_instance(player, card);

  // if there is only 1 target, then pick it
  if (!duel_host_is_headless() &&get_setting(SETTING_SMART_TARGET))
  {

    // read in which cards should auto target
    if (read_auto_target_file == 0)
    {
      char buffer[500];
      SDL_IOStream *file = shandalar_open_file_io("TargetsHuman.txt", "r");
      int count = 0;
      while (shandalar_io_scanf(file, "%[.]", buffer) == 1)
      {
        shandalar_io_scanf(file, "%[0-9]", buffer);
        int num = atoi(buffer);
        auto_targets[count] = num;
        auto_targets_target[count] = HUMAN;
        shandalar_io_scanf(file, "%[^\n]", buffer);
        shandalar_io_scanf(file, "%[\n]", buffer);
        count++;
      }
      shandalar_io_close(file);
      file = shandalar_open_file_io("TargetsAI.txt", "r");
      while (shandalar_io_scanf(file, "%[.]", buffer) == 1)
      {
        shandalar_io_scanf(file, "%[0-9]", buffer);
        int num = atoi(buffer);
        auto_targets[count] = num;
        auto_targets_target[count] = AI;
        shandalar_io_scanf(file, "%[^\n]", buffer);
        shandalar_io_scanf(file, "%[\n]", buffer);
        count++;
      }
      shandalar_io_close(file);
      auto_targets[count] = -1;
      read_auto_target_file = 1;
    }

    // see if the card being used has a default target
    int id = get_id(player, card);

    int i;
    int original_allowed_controller = td->allowed_controller;
    int original_preferred_controller = td->preferred_controller;

    for (i = 0; i < 500; i++)
    {
      if (auto_targets[i] >= 0 && auto_targets[i] == id)
      {
        td->allowed_controller = auto_targets_target[i];
        td->preferred_controller = auto_targets_target[i];
        break;
      }
    }

    if (target_available(player, card, td) == 1)
    {
      int p;
      for (p = 0; p < 2; p++)
      {
        for (i = -1; i < active_cards_count[p]; i++)
        {
          instance->targets[0].player = p;
          instance->targets[0].card = i;
          instance->number_of_targets = 0;
          if (would_validate_target(player, card, td, 0))
          {
            return 1;
          }
        }
      }
    }
    else
    {
      td->allowed_controller = original_allowed_controller;
      td->preferred_controller = original_preferred_controller;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2660bb0; original functions/targets.c:1722. */
int select_target(int player, int card, target_definition_t *td, const char *prompt,
                       target_t *ret_location)
{
  int result;
  card_instance_t *instance;

  if (card < -500)
  {
    card = card + 1000;
    instance = NULL;
    if (ret_location == NULL)
    {
      backtrace_and_abort("functions/targets.c:1727\nret_location");
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if (ret_location == NULL)
    {
      ret_location = instance->targets;
    }
    result = check_special_flags(player, card, 0x20000000);
    if ((result != 0) && (ret_location->player != -1))
    {
      return 1;
    }
    result = choose_default_target(player, card, td);
    if (result != 0)
    {
      return 1;
    }
  }
  result = check_special_flags(player, card, 0x20000000);
  if ((result == 0) || (ret_location->player == -1))
  {
    result = select_target_maybe_force(
        player, card, td->who_chooses, td->allowed_controller, td->preferred_controller, td->zone,
        td->required_type, td->illegal_type, td->required_abilities, td->illegal_abilities,
        td->required_color, td->illegal_color, td->extra, td->required_subtype,
        td->power_requirement, td->toughness_requirement, td->special, td->required_state,
        td->illegal_state, td->flags, prompt, td->allow_cancel, ret_location);
    if (result == 0)
    {
      return 0;
    }
    if (instance != NULL)
    {
      instance->number_of_targets = instance->number_of_targets + '\x01';
    }
  }
  return 1;
}

/* ManalinkEh 2022: 0x2660d50; original functions/targets.c:1520. */
int pick_target(target_definition_t *td, const char *prompt)
{
  int result;

  load_text(NULL, prompt);
  result = select_target(td->player, td->card, td, text_lines[0], NULL);
  if (result == 0)
  {
    spell_fizzled = 1;
  }
  return result;
}

/* ManalinkEh 2022: 0x2660db0; original functions/targets.c:1531. */
int new_pick_target(target_definition_t *td, const char *prompt, int ret_location,
                         int mode)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(td->player, td->card);
  if (ret_location == -1)
  {
    ret_location = (int)instance->number_of_targets;
  }
  if ((mode & 0x80U) == 0)
  {
    load_text(NULL, prompt);
    prompt = text_lines[0];
  }
  result = select_target(td->player, td->card, td, prompt, instance->targets + ret_location);
  if ((result == 0) && ((mode & 1U) != 0))
  {
    spell_fizzled = 1;
  }
  return result;
}

/* ManalinkEh 2022: 0x2660e50; original functions/targets.c:1552. */
int pick_next_target_noload(target_definition_t *td, const char *prompt)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(td->player, td->card);
  result = select_target(td->player, td->card, td, prompt,
                              instance->targets + instance->number_of_targets);
  if (result == 0)
  {
    spell_fizzled = 1;
  }
  return result;
}

/* ManalinkEh 2022: 0x2660eb0; original functions/targets.c:1562. */
int pick_up_to_n_targets_noload(target_definition_t *td, const char *prompt, int num)
{
  if (num <= 0)
    return 0;

  card_instance_t *instance = get_card_instance(td->player, td->card);
  instance->number_of_targets = 0;

  int len = strlen(prompt) + 50;
  char prompt_w_count[len];
  prompt_w_count[0] = 0;

  char fmt[100];
  if (IS_AI(td->who_chooses))
    strcpy(fmt, "%s %d %d");
  else
  {
    load_text(0, "TARGET_COUNT");
    strcpy(fmt, (td->allow_cancel & 2) ? text_lines[1] : text_lines[0]);
  }

  int already_chose_player[2] = {0};
  target_t tgt;

  int i, picked_cancel = 0;
  for (i = 0; i < num && can_target(td); ++i)
  {
    if (!IS_AI(td->who_chooses))
      scnprintf(prompt_w_count, len, fmt, prompt, i + 1, num);

    if (select_target(td->player, td->card - 1000, td, prompt_w_count, &tgt))
    {
      if (tgt.card != -1)
        get_card_instance(tgt.player, tgt.card)->state |= STATE_TARGETTED | STATE_CANNOT_TARGET;
      else if (!already_chose_player[tgt.player])
        already_chose_player[tgt.player] = 1;
      else
      {
        if (!IS_AI(td->who_chooses))
        {
          load_text(0, "FIREBALL");
          display_error_message(text_lines[1]);
        }
        --i;
        continue;
      }

      instance->targets[i] = tgt;
      ++instance->number_of_targets;
    }
    else
    {
      picked_cancel = tgt.card == -1;
      break;
    }
  }

  for (i = 0; i < instance->number_of_targets; ++i)
    if (instance->targets[i].card != -1)
      get_card_instance(instance->targets[i].player, instance->targets[i].card)->state &=
          ~(STATE_TARGETTED | STATE_CANNOT_TARGET);

  if (picked_cancel)
  {
    instance->number_of_targets = 0;
    spell_fizzled = 1;
  }

  return instance->number_of_targets;
}

/* ManalinkEh 2022: 0x2661180; original functions/targets.c:1636. */
int pick_up_to_n_targets(target_definition_t *td, const char *prompt, int num)
{
  int result;
  char loaded[300];
  if (ai_is_speculating == 1)
  {
    loaded[0] = '\0';
  }
  else
  {
    load_text(NULL, prompt);
    scnprintf(loaded, 300, "%s", text_lines[0]);
  }
  result = pick_up_to_n_targets_noload(td, loaded, num);
  return result;
}

/* ManalinkEh 2022: 0x2661200; original functions/targets.c:1653. */
int mark_up_to_n_targets_noload(target_definition_t *td, const char *prompt, int num,
                                char (*marked)[151])
{
  if (num == 0)
    return 0;
  if (num < 0)
    num = 500;

  target_t tgt;

  int p, c, i, picked_cancel = 0, num_tgts = 0;
  for (i = 0; i < num && can_target(td); ++i)
    if (select_target(td->player, td->card - 1000, td, prompt, &tgt))
    {
      ++num_tgts;
      marked[tgt.player][tgt.card] = 1;
      get_card_instance(tgt.player, tgt.card)->state |= STATE_TARGETTED | STATE_CANNOT_TARGET;
    }
    else
    {
      picked_cancel = tgt.card == -1;
      break;
    }

  for (p = 0; p <= 1; ++p)
    for (c = 0; c < active_cards_count[p]; ++c)
      if (marked[p][c])
        get_card_instance(p, c)->state &= ~(STATE_TARGETTED | STATE_CANNOT_TARGET);

  if (picked_cancel)
  {
    num_tgts = 0;
    spell_fizzled = 1;
  }

  return num_tgts;
}

/* ManalinkEh 2022: 0x2661390; original functions/targets.c:1766. */
int pick_player_duh(int player, int card, int preferred_controller, int allow_cancel)
{
  card_instance_t *instance;
  int result;
  target_definition_t td;

  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 0);
  td.preferred_controller = preferred_controller;
  td.zone = TARGET_ZONE_PLAYERS;
  td.allow_cancel = allow_cancel;
  result = duh_mode(player);
  if (result != 0)
  {
    instance->number_of_targets = '\x01';
    instance->targets[0].player = preferred_controller;
    instance->targets[0].card = -1;
    result = would_validate_target(td.player, td.card, &td, 0);
    if (result != 0)
    {
      return 1;
    }
    if ((allow_cancel & 1U) != 0)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  instance->number_of_targets = '\0';
  result = can_target(&td);
  if (result == 0)
  {
    return 0;
  }
  result = pick_target(&td, "TARGET_PLAYER");
  return (unsigned int)(result != 0);
}

/* ManalinkEh 2022: 0x26615c0; original functions/targets.c:1829. */
int target_opponent(int player, int card)
{
  card_instance_t *instance;
  int result;
  target_definition_t td;

  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 0);
  td.preferred_controller = 1 - player;
  td.allow_cancel = 0;
  instance->number_of_targets = '\x01';
  instance->targets[0].player = td.preferred_controller;
  instance->targets[0].card = -1;
  td.zone = TARGET_ZONE_PLAYERS;
  result = would_validate_target(td.player, td.card, &td, 0);
  if (result == 0)
  {
    instance->targets[0].player = -1;
    instance->number_of_targets = '\0';
    spell_fizzled = 1;
    return 0;
  }
  return 1;
}

/* ManalinkEh 2022: 0x2661670; original functions/targets.c:1856. */
int opponent_is_valid_target(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.allow_cancel = 0;
  uint8_t saved_count = instance->number_of_targets;
  td.preferred_controller = 1 - player;
  instance->number_of_targets = 1;
  target_t saved_target = instance->targets[0];
  instance->targets[0].player = td.preferred_controller;
  instance->targets[0].card = -1;
  td.zone = TARGET_ZONE_PLAYERS;
  int result = would_validate_target(td.player, td.card, &td, 0);
  instance->targets[0] = saved_target;
  instance->number_of_targets = saved_count;
  return result;
}

/* ManalinkEh 2022: 0x2661750; original functions/targets.c:1881. */
int valid_target(target_definition_t *td)
{
  int32_t saved_preferred;
  int result;

  saved_preferred = td->preferred_controller;
  td->preferred_controller = td->allowed_controller;
  result = would_validate_target(td->player, td->card, td, 0);
  td->preferred_controller = saved_preferred;
  return result;
}

/* ManalinkEh 2022: 0x2661360; original functions/targets.c:1884. */
int would_valid_target(target_definition_t *td)
{
  int result;
  result = would_validate_target(td->player, td->card, td, 0);
  return result;
}

/* ManalinkEh 2022: 0x2661710; original functions/targets.c:1908. */
int validate_target(int player, int card, target_definition_t *td, int target_number)
{
  int32_t saved_preferred;
  int result;

  saved_preferred = td->preferred_controller;
  td->preferred_controller = td->allowed_controller;
  result = would_validate_target(player, card, td, target_number);
  td->preferred_controller = saved_preferred;
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2661790; original functions/targets.c:1953. */
int autoselect_target(int player, int card, int i, int j, int k, int who_chooses,
                           int allowed_controller, int preferred_controller, int zone,
                           int required_type, int illegal_type, int required_abilities,
                           int illegal_abilities, int required_color, int illegal_color, intptr_t extra,
                           int required_subtype, int power_requirement, int toughness_requirement,
                           int special, int required_state, int illegal_state, int flags,
                           int prompt, int allow_cancel, int ret_location)
{
  int result;
  target_definition_t td;

  if (who_chooses != 1)
  {
    default_target_definition(player, card, &td, 0xff);
    td.allowed_controller = allowed_controller;
    td.preferred_controller = preferred_controller;
    td.zone = zone;
    td.required_type = required_type;
    td.illegal_type = illegal_type;
    td.required_color = required_color;
    td.illegal_color = illegal_color;
    td.extra = extra;
    td.required_subtype = required_subtype;
    td.power_requirement = power_requirement;
    td.toughness_requirement = toughness_requirement;
    td.special = special;
    td.required_state = required_state;
    td.illegal_state = illegal_state;
    td.allow_cancel = allow_cancel;
    td.flags = flags;
    result = choose_default_target(player, card, &td);
    return (unsigned int)(result != 0);
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline card mechanics; original native callers contain the expansions. */
/* ManalinkEh 2022: 0x2661a70; original functions/targets.c:2092. */
target_t *becomes_target_of_subtype_spell(int player, int card, event_t event, int t_player,
                                               int t_card, int controlled_by,
                                               resolve_trigger_t trigger_mode,
                                               int targeting_subtype)
{
  static target_t rval;
  int result;
  if (trigger_condition != TRIGGER_SPELL_CAST)
  {
    return NULL;
  }
  if ((((((card == affected_card) && (player == affected_card_controller)) &&
         (reason_for_trigger_controller == player)) &&
        ((controlled_by == 2 || (trigger_cause_controller == controlled_by)))) &&
       (((unsigned int)event - (unsigned int)EVENT_TRIGGER < 2U &&
         ((result = target_me(t_player, t_card, trigger_cause_controller, trigger_cause),
           result != 0 &&
               (result = is_what(trigger_cause_controller, trigger_cause, TYPE_LAND | TYPE_EFFECT),
                result == 0)))))) &&
      (result = has_subtype(trigger_cause_controller, trigger_cause, targeting_subtype),
       result != 0))
  {
    if (event != EVENT_TRIGGER)
    {
      rval.player = trigger_cause_controller;
      rval.card = trigger_cause;
      return &rval;
    }
    if (trigger_mode == RESOLVE_TRIGGER_DUH)
    {
      result = duh_mode(player);
      trigger_mode = (result != 0) + RESOLVE_TRIGGER_OPTIONAL;
    }
    event_result = event_result | trigger_mode;
  }
  return NULL;
}
#endif

/* SDL host helper retained during the 2022 migration. */
static int replay_ai_target_choice(const target_t* candidates, int num_candidates)
{
  if (g_shandalar_difficulty == 0)
    {
      if (g_ai_recorded_choice == 99 || num_candidates <= g_ai_recorded_choice)
        g_ai_recorded_choice = internal_rand(num_candidates);
      return 1;
    }

  if (g_ai_recorded_choice != 99 && g_ai_recorded_action != -1 && (g_ai_recorded_action & 0x4000))
    {
      int p = (g_ai_recorded_action >> 8) & 1;
      int c = g_ai_recorded_action & 0xff;
      int i;
      if (c == 0xff)
        c = -1;
      for (i = 0; i < num_candidates; ++i)
        if (candidates[i].player == p && candidates[i].card == c)
          {
            g_ai_recorded_choice = i;
            return 1;
          }
    }

  spell_fizzled = 1;
  return 0;
}

/* SDL host helper retained during the 2022 migration. */
static int choose_ai_target(const target_t* candidates, int num_candidates)
{
  g_ai_recorded_action_type = 3;
  if (ai_is_speculating == 1)
    {
      g_ai_recorded_choice = internal_rand(num_candidates);
      g_ai_recorded_action = (candidates[g_ai_recorded_choice].player < 1 ? 0 : 0x100)
                            | (candidates[g_ai_recorded_choice].card & 0xff) | 0x4000;
      sub_498F20();
      return 1;
    }
  sub_499050();
  return replay_ai_target_choice(candidates, num_candidates);
}
