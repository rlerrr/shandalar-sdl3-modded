// -*- c-basic-offset:2 -*-
// Low-level event, trigger, and card function handling.

#include "manalink.h"
#include "magic/src/duel_host.h"

/* October 2022 DWARF globals, original events.c:786-787; DLL .data contains 1. */
int raw_base_pt = 1;              /* 0x02721d64 */
int raw_characteristic_pt = 1;    /* 0x02721d60 */

// These are fairly common here, but shouldn't be used elsewhere
// A frontend for call_card_fn() where both address and instance should be computed from {player,card}.
int call_card_function(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  int iid = instance->internal_card_id;
  if (iid == -1)
    iid = instance->backup_internal_card_id;
  CardFunction fn = cards_data[iid].code_pointer;
  instance = get_card_instance(player, card);
  return call_card_fn(fn, instance, player, card, event);
}

// A frontend for call_card_fn() where address should be computed from instance.
int call_card_function_i(card_instance_t* instance, int player, int card, event_t event)
{
  CardFunction fn = cards_data[instance->internal_card_id].code_pointer;
  return call_card_fn(fn, instance, player, card, event);
}

int call_card_fn_impl(CardFunction address, card_instance_t* instance, int player, int card, event_t event);	// never call directly
// Puts instance into esi, then calls address(player, card, event).
int call_card_fn(CardFunction address, card_instance_t *instance, int player, int card,
                 event_t event)
{
  int old_trigger_condition = trigger_condition;
  if (event != EVENT_TRIGGER && event != EVENT_RESOLVE_TRIGGER && event != EVENT_END_TRIGGER)
    trigger_condition = -1;
  int rval = call_card_fn_impl(address, instance, player, card, event);
  trigger_condition = old_trigger_condition;
  return rval;
}

extern int timestamp_card[500];
extern int timestamp_player[500];
void dispatch_event_raw(event_t event)
{
  // 0x435a40

  if (duel_host_is_headless() && duel_host_capture().remaining_steps < 8)
    fprintf(stderr,"EVENT budget exhausted: event=%x affected=(%d,%d) trigger=%x phase=%d stack=%d result=%d\n",event,affected_card_controller,affected_card,trigger_condition,current_phase,stack_size,event_result);
  duel_host_checkpoint("execution budget exhausted during event dispatch");minimize_nondraining_mana();	// Needs to happen quite frequently.  This'll do it.

  // Begin additions
  // These two are because something calls this without stashing them when it really should; http://www.slightlymagic.net/forum/viewtopic.php?t=13665 is a symptom.
  int old_affected_card_controller = affected_card_controller;
  int old_affected_card = affected_card;
  /* And this is an optimization; compare e.g. upkeep_trigger_ability_mode(), which just checks trigger_condition before calling expensive things like
   * count_upkeeps(), even though it'll only respond to EVENT_TRIGGER and EVENT_RESOLVE_TRIGGER.  When called from dispatch_event() or similar, it's already set
   * to -1, but plenty of things call dispatch_event_raw instead.  Shame on them. */
  int old_trigger_condition = trigger_condition;
  if (event != EVENT_TRIGGER && event != EVENT_RESOLVE_TRIGGER && event != EVENT_END_TRIGGER)
	trigger_condition = -1;
  // End additions

  //if (ai_is_speculating!=1)logg("dispatch_event_raw", "%s%s%s for (%d,%d)=%s%s", trigger_condition <= 0 ? "" : trigger_t_str(trigger_condition), trigger_condition <= 0 ? "" : "/", event_t_str(event), affected_card_controller, affected_card, get_name(affected_card_controller, affected_card));

  // Begin additions
  // In the exe version, cards attacking cards were tapped in the same loop as the event was dispatched.  Instead, do it before dispatching.
  // If this seems out-of-place here, well, it did to me too; but I'm not going to try to shoehorn it into this function's callers.
  unsigned int ts;
  unsigned char attackers[151];
  if (event == EVENT_DECLARE_ATTACKERS)
	{
	  memset(attackers, 0xff, sizeof(attackers));
	  number_of_attackers_declared = 0;
	  for (ts = 0; ts < 500 && timestamp_player[ts] != -1; ++ts)
		{
		  int player = timestamp_player[ts];
		  int card = timestamp_card[ts];

		  card_instance_t* instance;
		  if (player == current_turn && (instance = in_play(player, card)) && instance->timestamp == ts
			  && (instance->state & STATE_ATTACKING))
			{
			  attackers[number_of_attackers_declared++] = card;
			  if (!(instance->state & STATE_TAPPED)
				  && !(get_special_abilities_by_instance(instance) & SP_KEYWORD_VIGILANCE))
				tap_card(player, card);
			}
		}
	}

  if (event == EVENT_CAN_SKIP_TURN)	// First event dispatched every turn, used only by skip-turn effects and housekeeping.
	enable_xtrigger_flags = 0;
  // End additions

  for (ts = 0; ts < 500 && timestamp_player[ts] != -1; ++ts)
	{
	  int player = timestamp_player[ts];
	  int card = timestamp_card[ts];

	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->timestamp == ts
		  && instance->internal_card_id != -1
		  && !(instance->state & STATE_OUBLIETTED)
		  && (instance->state & (STATE_INVISIBLE | STATE_IN_PLAY)))
		{
		  CardFunction fn = cards_data[instance->internal_card_id].code_pointer;
		  call_card_fn(fn, instance, player, card, event);
		  // Begin removals
		  // if event == EVENT_DECLARE_ATTACKERS, then tap the card if it's attacking, untapped, and doesn't have vigilance; then resolve damage cards and run regeneration/graveyard triggers
		  // End removals
		}
	}

  if (event == EVENT_DECLARE_ATTACKERS && number_of_attackers_declared > 0)
	{
	  // Begin additions
	  dispatch_xtrigger2(current_turn, XTRIGGER_ATTACKING, "attacking", 0, 0, 0);

	  /* These were run after each card tapped.  That was both too many times *and* not enough, since they didn't run for non-attacking cards handling
	   * EVENT_DECLARE_ATTACKERS. */
	  process_damage_prevention();	// resolve_damage_cards_and_prevent_damage();
	  regenerate_or_graveyard_triggers();	// regenerate_or_graveyard_triggers();
	  // End additions

	  recalculate_all_cards_in_play();
	}

  // Begin additions
  affected_card_controller = old_affected_card_controller;
  affected_card = old_affected_card;
  trigger_condition = old_trigger_condition;

  if (event == EVENT_TAPPED_TO_PLAY_ABILITY || event == EVENT_PLAY_ABILITY
	  || (event == EVENT_CAST_SPELL && !check_state(affected_card_controller, affected_card, STATE_IN_PLAY)))	// actually cast, not put_into_play
	recalculate_all_cards_in_play();
  // End additions
}

// dispatch_event() forces the initial value of event_result to 0; this lets you set it arbitrarily.
int dispatch_event_with_initial_event_result(int new_affected_card_controller,
                                             int new_affected_card, event_t event,
                                             int initial_event_result)
{
  return dispatch_event_with_attacker_and_initial_event_result(
      new_affected_card_controller, new_affected_card, event, 1 - new_affected_card_controller, -1,
      initial_event_result);
}

/* dispatch_event() forces attacking_card_controller to 1-player and attacking_card to -1; this lets you set them explicitly.  Returns the value of event_result
 * as set by the cards' functions (before restoring event_result's previous value). */
int dispatch_event_with_attacker(int new_affected_card_controller, int new_affected_card,
                                 event_t event, int new_attacking_card_controller,
                                 int new_attacking_card)
{
  return dispatch_event_with_attacker_and_initial_event_result(
      new_affected_card_controller, new_affected_card, event, new_attacking_card_controller,
      new_attacking_card, 0);
}

/* Identical to dispatch_event_with_attacker(), but only sends the event to (new_affeced_card_controller,new_affected_card).  Differs from a bare
 * call_card_function() in that it preserves affected_card_controller, affected_card, trigger_condition, event_result, etc.  Returns the value of event_result
 * as set by the card function (before restoring event_result's previous value). */
int dispatch_event_with_attacker_to_one_card(int new_affected_card_controller,
                                             int new_affected_card, event_t event,
                                             int new_attacking_card_controller,
                                             int new_attacking_card)
{
  return dispatch_event_arbitrary_to_one_card_with_initial_event_result(
      new_affected_card_controller, new_affected_card, event, new_affected_card_controller,
      new_affected_card, new_attacking_card_controller, new_attacking_card, 0);
}

int event_rval;
/* Identical to dispatch_event_with_attacker_to_one_card(), but sends the event to an arbitrary (player, card) pair which can differ from
 * (affected_card_controller, affected_card). */
int dispatch_event_arbitrary_to_one_card(int player, int card, event_t event,
                                         int new_affected_card_controller, int new_affected_card,
                                         int new_attacking_card_controller, int new_attacking_card)
{
  return dispatch_event_arbitrary_to_one_card_with_initial_event_result(
      player, card, event, new_affected_card_controller, new_affected_card,
      new_attacking_card_controller, new_attacking_card, 0);
}

/* The semantics of this one are a bit unusual.  Avoid unless you know exactly why you need it. */
int dispatch_event_to_single_card(int player, int card, event_t event, int new_attacking_card_controller, int new_attacking_card)
{
  card_instance_t* inst = get_card_instance(player, card);
  if (inst->internal_card_id == -1)
	return 0;

  if (event == EVENT_CAN_ACTIVATE
	  && ((inst->state & STATE_OUBLIETTED) || inst->upkeep_flags & 8))
	return 0;

  push_affected_card_stack();

  event_result = 0;
  affected_card_controller = player;
  affected_card = card;
  attacking_card_controller = new_attacking_card_controller;
  attacking_card = new_attacking_card;
  int old_736808 = g_activation_event_flags;

  int old_trigger_condition = trigger_condition;
  if (event != EVENT_TRIGGER && event != EVENT_RESOLVE_TRIGGER && event != EVENT_END_TRIGGER)
    trigger_condition = -1;

  int rval = call_card_function_i(inst, player, card, event);

  trigger_condition = old_trigger_condition;

  if (rval == 99
	  || !(land_can_be_played & (LCBP_REGENERATION | LCBP_SPELL_BEING_PLAYED | LCBP_DAMAGE_PREVENTION))
	  || !(event == EVENT_CAN_CAST || event == EVENT_CAN_ACTIVATE)
	  || is_nonactivated_mana_source(player, card)	// is_mana_source_but_not_act_ability
	  || (land_can_be_played & LCBP_CHARGING_MANA))
	g_last_single_card_event_result = event_result;
  else
	{
	  g_activation_event_flags = old_736808;
	  rval = 0;
	}
  pop_affected_card_stack();
  return rval;
}

// Properly stashes globals then calls an arbitrary iid's card function for player/card (instead of that card's own one).  Returns what the function does.
int dispatch_event_to_single_card_overriding_function(int player, int card, event_t event, int iid)
{
  push_affected_card_stack();

  event_result = 0;
  affected_card_controller = player;
  affected_card = card;
  attacking_card_controller = 1-player;
  attacking_card = -1;

  int old_trigger_condition = trigger_condition;
  if (event != EVENT_TRIGGER && event != EVENT_RESOLVE_TRIGGER && event != EVENT_END_TRIGGER)
	trigger_condition = -1;

  CardFunction fn = cards_data[iid].code_pointer;
  int rval = call_card_fn(fn, get_card_instance(player, card), player, card, event);

  trigger_condition = old_trigger_condition;
  pop_affected_card_stack();

  return rval;
}

int dispatch_trigger_to_one_card(int player, int card, event_t event)
{
  // 0x477970
  if (event == EVENT_TRIGGER
	  && (g_ai_search_result	// suppress_this_trigger
		  || (get_card_instance(player, card)->state & STATE_PROCESSING)))
	return 0;

  if (trigger_condition < 200)
	return 0;

  push_affected_card_stack();	// missing in original (and the pop below)

  event_result = 0;
  affected_card_controller = player;
  affected_card = card;
  attacking_card = -1;
  dispatch_event_raw(event);
  int rval = event_result;

  pop_affected_card_stack();

  return rval;
}

xtrigger_t xtrigger_impl_value_dont_use_directly = 0;
enable_xtrigger_flags_t enable_xtrigger_flags = 0;

/* ManalinkEh 2022: 0x26cbdb0; original functions/events.c:155. */
int dispatch_event_with_attacker_and_initial_event_result(int new_affected_card_controller,
                                                          int new_affected_card, event_t event,
                                                          int new_attacking_card_controller,
                                                          int new_attacking_card,
                                                          int initial_event_result)
{
  int result;
  uint32_t saved_trigger_condition;

  push_affected_card_stack();
  saved_trigger_condition = trigger_condition;
  event_result = initial_event_result;
  affected_card_controller = new_affected_card_controller;
  affected_card = new_affected_card;
  attacking_card_controller = new_attacking_card_controller;
  attacking_card = new_attacking_card;
  if ((1 < (uint32_t)(event - EVENT_TRIGGER)) && (event != EVENT_END_TRIGGER))
  {
    trigger_condition = 0xffffffff;
  }
  dispatch_event_raw(event);
  result = event_result;
  trigger_condition = saved_trigger_condition;
  pop_affected_card_stack();
  return result;
}

/* ManalinkEh 2022: 0x26cb5b0; original functions/events.c:223. */
int dispatch_event_arbitrary_to_one_card_with_initial_event_result(
    int player, int card, event_t event, int new_affected_card_controller, int new_affected_card,
    int new_attacking_card_controller, int new_attacking_card, int initial_event_result)
{
  int result;
  uint32_t saved_trigger_condition;

  push_affected_card_stack();
  saved_trigger_condition = trigger_condition;
  event_result = initial_event_result;
  affected_card_controller = new_affected_card_controller;
  affected_card = new_affected_card;
  attacking_card_controller = new_attacking_card_controller;
  attacking_card = new_attacking_card;
  if ((1 < (uint32_t)(event - EVENT_TRIGGER)) && (event != EVENT_END_TRIGGER))
  {
    trigger_condition = 0xffffffff;
  }
  event_rval = call_card_function(player, card, event);
  result = event_result;
  trigger_condition = saved_trigger_condition;
  pop_affected_card_stack();
  return result;
}

static void dispatch_trigger_impl(int player, trigger_t trig, xtrigger_t xtrig, const char *prompt,
                                  int TENTATIVE_allow_response)
{
  // Original at 0x4371E0
#define TRIGGER_DEPTH g_nested_trigger_depth

  ++TRIGGER_DEPTH;
  int orig_tcc = trigger_cause_controller;
  int orig_tc = trigger_cause;
  int old_62C180 = g_activation_dispatch_flags;
  int old_reason_for_trigger_controller = reason_for_trigger_controller;
  int old_62BCE8 = g_current_action_event_code;
  g_current_action_event_code = trig;
  reason_for_trigger_controller = player;
  int p, c;

  // Begin additions
  char processing[2][150];
  int processing_set = 0;
  if (TRIGGER_DEPTH != 1)
  {
    processing_set = 1;
    for (p = 0; p < 2; ++p)
    {
      ASSERT(active_cards_count[p] <= 150);
      for (c = 0; c < active_cards_count[p]; ++c)
      {
        card_instance_t *instance = get_card_instance(p, c);
        if (instance->internal_card_id == -1)
          processing[p][c] = 2;
        else if (instance->state & STATE_PROCESSING)
        {
          processing[p][c] = 1;
          instance->state &= ~STATE_PROCESSING;
        }
        else
          processing[p][c] = 0;

        if (instance->state & STATE_IS_TRIGGERING)
        {
          processing[p][c] |= 4;
          instance->state &= ~STATE_IS_TRIGGERING;
        }
      }

      for (c = active_cards_count[p]; c < 150; ++c)
        processing[p][c] = 2;
    }
  }
  // End additions

  int old_620860 = g_response_processing_flags;
  int old_60A554 = g_response_action_mask;
  unsigned int v8;
  do
  {
    if (trace_mode & 2)
      g_response_processing_flags = 1;
    else if (player)
      g_response_processing_flags = 2;
    else
      g_response_processing_flags = 1;

    trigger_condition = trig;
    xtrigger_impl_value_dont_use_directly = xtrig;
    if (TENTATIVE_allow_response)
      g_response_action_mask = TYPE_INSTANT | TYPE_INTERRUPT;
    else
      g_response_action_mask = 0;
    cancel = 0;
    g_duel_state_current_card = 0;
    g_activation_dispatch_flags = 0;
    v8 = process_response_actions(player, prompt);
    g_response_card_type_mask = old_60A554 & (TYPE_INSTANT | TYPE_INTERRUPT);
  } while (((v8 < 1 ? 4 : 6) & g_activation_dispatch_flags) || (TENTATIVE_allow_response && v8));
  g_response_action_mask = old_60A554;
  g_response_processing_flags = old_620860;

  // Begin additions
  // Not just dispatch_event(... EVENT_END_TRIGGER), since that'll clear trigger_condition
  // Set again, just in case something changed them and didn't restore
  trigger_condition = trig;
  xtrigger_impl_value_dont_use_directly = xtrig;
  reason_for_trigger_controller = player;
  trigger_cause = orig_tc;
  trigger_cause_controller = orig_tcc;
  int old_aff_cc = affected_card_controller, old_aff_c = affected_card, old_ev = event_result;
  card_instance_t *inst;
  for (p = 0; p < 2; ++p)
    for (c = 0; c < active_cards_count[p]; ++c)
      if ((inst = in_play(p, c)))
      {
        affected_card_controller = p;
        affected_card = c;
        call_card_function_i(inst, p, c, EVENT_END_TRIGGER);
      }
  affected_card_controller = old_aff_cc;
  affected_card = old_aff_c;
  event_result = old_ev;
  trigger_cause = orig_tc;
  trigger_cause_controller = orig_tcc;
  // End additions

  trigger_condition = -1;

  --TRIGGER_DEPTH;
  if (TRIGGER_DEPTH == 0)
  {
    for (p = 0; p < 2; ++p)
      for (c = 0; c < active_cards_count[p]; ++c)
        get_card_instance(p, c)->state &= ~(STATE_PROCESSING | STATE_IS_TRIGGERING);

    if (!g_trigger_dispatch_depth)
    {
      g_response_window_serial = 0;
      if (ai_is_speculating != 1)
        g_phase_stop_suppressed = 0;
    }
  }
  // Begin additions
  else
  {
    ASSERT(processing_set);
    for (p = 0; p < 2; ++p)
    {
      ASSERT(active_cards_count[p] <= 150);
      for (c = 0; c < active_cards_count[p]; ++c)
      {
        card_instance_t *instance = get_card_instance(p, c);

        if (instance->internal_card_id ==
            -1) // Any newly-obliterated cards always get processing removed.
          instance->state &= ~(STATE_PROCESSING | STATE_IS_TRIGGERING);
        else
        {
          if (processing[p][c] & 4) // Restore the previous is-triggering state
          {
            instance->state |= STATE_IS_TRIGGERING;
            processing[p][c] &= ~4;
          }

          if (processing[p][c]) // Newly-created cards (processing[p][c] == 2) and cards that had
                                // processing before (processing[p][c] == 1)
            instance->state |= STATE_PROCESSING;
          else // Unprocessed cards that existed before and after get it removed.
            instance->state &= ~STATE_PROCESSING;
        }
      }
    }
  }
  // End additions

  g_activation_dispatch_flags = old_62C180;
  reason_for_trigger_controller = old_reason_for_trigger_controller;
  g_current_action_event_code = old_62BCE8;

#undef TRIGGER_DEPTH
}

int dispatch_trigger(int player, trigger_t trig, const char *prompt, int TENTATIVE_allow_response)
{
  xtrigger_t old_xtrigger = xtrigger_impl_value_dont_use_directly;
  dispatch_trigger_impl(player, trig, 0, prompt, TENTATIVE_allow_response);
  xtrigger_impl_value_dont_use_directly = old_xtrigger;
  return 0;
}

int dispatch_trigger2(int player, trigger_t trig, const char *prompt, int TENTATIVE_allow_response, int new_trigger_cause_controller, int new_trigger_cause)
{
  int old_trigger_cause = trigger_cause;
  int old_trigger_cause_controller = trigger_cause_controller;
  xtrigger_t old_xtrigger = xtrigger_impl_value_dont_use_directly;

  trigger_cause_controller = new_trigger_cause_controller;
  trigger_cause = new_trigger_cause;

  dispatch_trigger_impl(  player, trig, 0, prompt, TENTATIVE_allow_response);
  dispatch_trigger_impl(1-player, trig, 0, prompt, TENTATIVE_allow_response);

  trigger_cause = old_trigger_cause;
  trigger_cause_controller = old_trigger_cause_controller;
  xtrigger_impl_value_dont_use_directly = old_xtrigger;

  return 0;
}

int dispatch_xtrigger2(int player, xtrigger_t xtrig, const char *prompt, int TENTATIVE_allow_response, int new_trigger_cause_controller, int new_trigger_cause)
{
  int old_trigger_cause = trigger_cause;
  int old_trigger_cause_controller = trigger_cause_controller;
  xtrigger_t old_xtrigger = xtrigger_impl_value_dont_use_directly;

  trigger_cause_controller = new_trigger_cause_controller;
  trigger_cause = new_trigger_cause;

  dispatch_trigger_impl(  player, TRIGGER_XTRIGGER, xtrig, prompt, TENTATIVE_allow_response);
  dispatch_trigger_impl(1-player, TRIGGER_XTRIGGER, xtrig, prompt, TENTATIVE_allow_response);

  trigger_cause = old_trigger_cause;
  trigger_cause_controller = old_trigger_cause_controller;
  xtrigger_impl_value_dont_use_directly = old_xtrigger;

  return 0;
}

// Fixes a bug in the exe.  Do not call directly.
void resolve_trigger(int player, int card, int TENTATIVE_reason_for_trigger_controller)
{
  if ((uint8_t)trace_mode & 2)
  {
    char buf[500];
    card_instance_t *traced = get_card_instance(player, card);
    int sequence = g_duel_trace_counter;
    g_duel_trace_counter = sequence + 1;
    sprintf(buf, "%d: Player #%d is processing %s(%d).\n", sequence, player,
            cards_data[traced->internal_card_id].name, card);
    append_to_trace_txt(buf);
  }
  g_ai_search_result = 0;
  put_card_or_activation_onto_stack(player, card, EVENT_RESOLVE_TRIGGER,
                                    TENTATIVE_reason_for_trigger_controller, 0);
  if (spell_fizzled == 1)
  {
    obliterate_top_card_of_stack();
    g_ai_search_result = 0;
    return;
  }
  if (ai_is_speculating != 1)
  {
    set_stack_damage_targets();
    if (reason_for_trigger_controller != 0)
    {
      char buf[300];
      load_text(NULL, "PROMPT_PROC1");
      sprintf(buf, text_lines[0], opponent_name);
      raw_do_dialog(player, card, -1, -1, buf, 0);
    }
    for (int p = 0; p < 2; ++p)
      for (int c = 0; c < active_cards_count[p]; ++c)
      {
        card_instance_t *instance = get_card_instance(p, c);
        instance->state &= ~STATE_IS_TRIGGERING;
      }
  }
  card_instance_t *source = get_card_instance(player, card);
  unsigned int saved_condition = trigger_condition;
  source->state |= STATE_IS_TRIGGERING | STATE_PROCESSING;
  xtrigger_t saved_xtrigger = xtrigger_impl_value_dont_use_directly;
  int saved_cause = trigger_cause;
  int saved_cause_controller = trigger_cause_controller;

  int proxy_card = -1;
  card_instance_t *proxy = NULL;
  unsigned int response_condition = saved_condition;
  if (!(source->token_status & 0x80) && !source->kill_code && (int)source->token_status >= 0)
  {
    if (in_play(player, card))
    {
      int iid = source->internal_card_id;
      uint16_t id = cards_data[iid].id;
      if (cards_data[iid].cc[2] != 3 && (uint16_t)(id - 901) >= 8 && (uint16_t)(id - 911) >= 19 &&
          (trigger_condition & ~4U) != 0xd8 && (unsigned int)trigger_condition - 0xd4 >= 2 &&
          cards_data[iid].type != TYPE_EFFECT && ai_is_speculating != 1 && player != -1 &&
          card != -1)
      {
        proxy_card = create_legacy_effect(player, card, empty);
        if (proxy_card == -1)
          backtrace_and_abort("functions/events.c:633\nstack_card != -1");
        proxy = get_card_instance(player, proxy_card);
        proxy->state = 0x800022;
        proxy->token_status = 0x80010000;
        proxy->damage_source_player = (int8_t)player;
        proxy->damage_source_card = card;
        unsigned int timestamp = proxy->timestamp;
        memcpy_inst_and_ext(proxy, source);
        proxy->kill_code = 0;
        proxy->state = 0x800022;
        unsigned int source_iid = source->internal_card_id;
        proxy->token_status = 0x80010000;
        proxy->original_internal_card_id = source_iid;
        proxy->unknown0x14 = 0;
        if (source_iid == UINT32_MAX)
          proxy->original_internal_card_id = source->backup_internal_card_id;
        int image = get_card_image_number(proxy->display_pic_csv_id, player, card);
        proxy->parent_controller = player;
        proxy->display_pic_num = (uint16_t)image;
        proxy->parent_card = card;
        proxy->timestamp = timestamp;
      }
    }
    response_condition = trigger_condition;
  }
  cant_be_responded_to |= response_condition == 0xcd;
  if (!cant_be_responded_to)
    allow_response_to_trigger(player, card);
  trigger_cause_controller = saved_cause_controller;
  trigger_cause = saved_cause;
  trigger_condition = saved_condition;
  xtrigger_impl_value_dont_use_directly = saved_xtrigger;

  int completed = 0;
  if (!source->kill_code && in_play(player, card))
  {
    if (!(source->state & 0x40000000U) && (!proxy || !(proxy->state & 0x40000000U)))
    {
      resolve_top_card_on_stack();
      if (proxy)
        obliterate_card_and_recycle(player, proxy_card);
      completed = 1;
    }
    else if (proxy)
      obliterate_card_and_recycle(player, proxy_card);
  }
  else if (!proxy)
  {
    if (!(source->state & 0x40000000U))
    {
      resolve_top_card_on_stack();
      completed = 1;
    }
  }
  else
  {
    source->state &= ~(STATE_IS_TRIGGERING | STATE_PROCESSING);
    proxy->state &= ~0x800022U;
    proxy->token_status &= ~0x10000U;
    dispatch_event_to_single_card(player, proxy_card, EVENT_TRIGGER, player, proxy_card);
    int stack_index = stack_size;
    stack_data[stack_index].internal_card_id = (int16_t)proxy->internal_card_id;
    stack_data[stack_index].generating_event = EVENT_RESOLVE_TRIGGER;
    stack_data[stack_index].unknown_sdt = (uint8_t)TENTATIVE_reason_for_trigger_controller;
    stack_cards[stack_index].player = player;
    stack_cards[stack_index].card = proxy_card;
    stack_damage_targets[stack_index].player = source->damage_target_player;
    stack_damage_targets[stack_index].card = source->damage_target_card;
    unsigned int phase = trigger_condition;
    if (phase == UINT32_MAX)
      phase = current_phase;
    stack_phase[stack_index] = phase;
    if (ai_is_speculating != 1)
    {
      g_stack_prompt_flags[stack_index] = 0;
      stack_index = stack_size;
    }
    stack_size = stack_index + 1;
    stack_cards[stack_size].player = -1;
    if (affected_card == card)
      affected_card = proxy_card;
    if (trigger_cause == card)
      trigger_cause = saved_cause = proxy_card;
    proxy->state |= STATE_IS_TRIGGERING | STATE_PROCESSING;
    if (!cant_be_responded_to)
      allow_response_to_trigger(player, proxy_card);
    trigger_cause = saved_cause;
    trigger_cause_controller = saved_cause_controller;
    trigger_condition = saved_condition;
    xtrigger_impl_value_dont_use_directly = saved_xtrigger;
    if (!(source->state & 0x40000000U) && !(proxy->state & 0x40000000U))
    {
      resolve_top_card_on_stack();
      completed = 1;
    }
    obliterate_card_and_recycle(player, proxy_card);
  }
  if (completed)
  {
    xtrigger_impl_value_dont_use_directly = saved_xtrigger;
    source->state &= ~STATE_IS_TRIGGERING;
    trigger_cause = saved_cause;
    g_ai_search_result = 0;
    trigger_cause_controller = saved_cause_controller;
    trigger_condition = saved_condition;
    dispatch_event(player, card, EVENT_TRIGGER_RESOLVED);
    trigger_condition = saved_condition;
    xtrigger_impl_value_dont_use_directly = saved_xtrigger;
    trigger_cause = saved_cause;
    trigger_cause_controller = saved_cause_controller;
  }
  else
  {
    trigger_cause = saved_cause;
    xtrigger_impl_value_dont_use_directly = saved_xtrigger;
    source->state &= ~(STATE_IS_TRIGGERING | 0x40000000U);
    trigger_condition = saved_condition;
    trigger_cause_controller = saved_cause_controller;
  }
}

int get_abilities(int player, int card, event_t event, int new_attacking_card)
{
  card_instance_t *inst;
  unsigned int iid;
  for (;;)
  {
    inst = get_card_instance(player, card);
    iid = inst->internal_card_id;
    if (iid != activation_card)
      break;
    if (event == EVENT_CHANGE_TYPE)
      return iid;
    card = inst->parent_card;
    player = inst->parent_controller;
  }
  if (g_duel_active)
    push_affected_card_stack();
  affected_card_controller = player;
  affected_card = card;
  if (iid == UINT32_MAX)
  {
    iid = inst->backup_internal_card_id;
    inst->regen_status &= ~KEYWORD_RECALC_ALL;
  }
  int saved_raw_base = raw_base_pt;
  attacking_card = new_attacking_card;
  int recalculate = 0;
  switch (event)
  {
  case EVENT_POWER:
    if (inst->regen_status & KEYWORD_RECALC_POWER)
    {
      inst->regen_status &= ~KEYWORD_RECALC_POWER;
      int swap = get_special_flags3_by_instance(inst) & SF3_SWITCH_POW_TGH;
      raw_characteristic_pt = swap ? cards_data[iid].toughness : cards_data[iid].power;
      raw_base_pt = raw_characteristic_pt;
      if ((inst->state & (STATE_OUBLIETTED | STATE_IN_PLAY)) == STATE_IN_PLAY &&
          raw_characteristic_pt > 0)
        raw_base_pt =
            (int16_t)((swap ? cards_data[iid].toughness : cards_data[iid].power) & 0xbfff);
      event_result = raw_base_pt + (swap ? inst->counter_toughness : inst->counter_power);
      if (swap)
        event = EVENT_TOUGHNESS;
      recalculate = 1;
    }
    else
      event_result = inst->power;
    break;
  case EVENT_TOUGHNESS:
    if (inst->regen_status & KEYWORD_RECALC_TOUGHNESS)
    {
      inst->regen_status &= ~KEYWORD_RECALC_TOUGHNESS;
      int swap = get_special_flags3_by_instance(inst) & SF3_SWITCH_POW_TGH;
      raw_characteristic_pt = swap ? cards_data[iid].power : cards_data[iid].toughness;
      raw_base_pt = raw_characteristic_pt;
      if ((inst->state & (STATE_OUBLIETTED | STATE_IN_PLAY)) == STATE_IN_PLAY &&
          raw_characteristic_pt > 0)
        raw_base_pt =
            (int16_t)((swap ? cards_data[iid].power : cards_data[iid].toughness) & 0xbfff);
      event_result = raw_base_pt + (swap ? inst->counter_power : inst->counter_toughness);
      if (swap)
        event = EVENT_POWER;
      recalculate = 1;
    }
    else
      event_result = inst->toughness;
    break;
  case EVENT_ABILITIES:
  {
    unsigned int keywords = inst->regen_status;
    if (keywords & KEYWORD_RECALC_ABILITIES)
    {
      inst->regen_status = keywords & ~KEYWORD_RECALC_ABILITIES;
      if (damage_card == iid)
        keywords &= ~KEYWORD_RECALC_ABILITIES;
      else
      {
        unsigned int base_keywords = cards_data[iid].static_ability;
        keywords = (keywords & 0x17000000U) | base_keywords;
        if (base_keywords & 0xf81f)
        {
          if (base_keywords & KEYWORD_PROT_COLORED)
          {
            unsigned int protections =
                get_sleighted_protection(player, card, base_keywords & KEYWORD_PROT_COLORED);
            keywords =
                (((keywords & 0xffff0000U) | (base_keywords & 0xffffU)) & ~KEYWORD_PROT_COLORED) |
                protections;
          }
          if (keywords & KEYWORD_BASIC_LANDWALK)
          {
            unsigned int landwalk =
                get_hacked_walk(player, card, keywords & KEYWORD_BASIC_LANDWALK);
            keywords = (keywords & ~KEYWORD_BASIC_LANDWALK) | landwalk;
          }
        }
        if (g_duel_active && (cards_data[iid].type & TYPE_PERMANENT) &&
            !(cards_data[iid].type & TYPE_EFFECT))
        {
          inst->ext->sp_keywords = 0;
          inst->ext->sp_keywords2 = 0;
          inst->targets[17].card &= 0xffffff81;
          inst->state &= ~(STATE_VIGILANCE | STATE_SUMMONSICK_BOTH);
          if (inst->ext->host_summoning_sickness) inst->state |= STATE_SUMMONSICK_BOTH;
          inst->token_status &= 0xffff77ff;
          remove_special_flags(player, card, 0x28000);
          remove_special_flags2(player, card, 0x1000);
          /* Haste permissions are owned by their granting effects. */
        }
        recalculate = 1;
      }
    }
    event_result = keywords;
    break;
  }
  case EVENT_RECALC_DAMAGE:
    event_result = inst->damage_on_card;
    recalculate = 1;
    break;
  case EVENT_SET_COLOR:
    if (!(inst->regen_status & KEYWORD_RECALC_SET_COLOR))
      event_result = inst->color;
    else
    {
      int is_damage = damage_card == iid;
      inst->regen_status &= ~KEYWORD_RECALC_SET_COLOR;
      if (is_damage)
        event_result = inst->color;
      else
      {
        unsigned char color = inst->initial_color;
        if (color)
          event_result = color & COLOR_TEST_ANY_COLORED;
        else if (cards_data[iid].type & TYPE_LAND)
          event_result = cards_data[iid].id == CARD_ID_DRYAD_ARBOR ? COLOR_TEST_GREEN : 0;
        else
        {
          int mana_artifact = 0;
          if ((cards_data[iid].type & TYPE_ARTIFACT) &&
              (cards_data[iid].extra_ability & EA_MANA_SOURCE))
          {
            int original_iid = get_internal_card_id_from_csv_id(cards_data[iid].id);
            mana_artifact = (cards_data[original_iid].type & TYPE_ARTIFACT) &&
                            (cards_data[original_iid].extra_ability & EA_MANA_SOURCE);
          }
          event_result = mana_artifact ? 0 : cards_data[iid].color & COLOR_TEST_ANY_COLORED;
        }
        recalculate = 1;
      }
    }
    break;
  case EVENT_CHANGE_TYPE:
    if (inst->internal_card_id == -1)
      event_result = -1;
    else if ((inst->regen_status & KEYWORD_RECALC_CHANGE_TYPE) &&
             ((int)iid < iid_legacy_data_card || (int)iid > iid_legacy_swap_power_toughness))
    {
      unsigned int original_iid = inst->original_internal_card_id;
      inst->regen_status &= ~KEYWORD_RECALC_CHANGE_TYPE;
      inst->internal_card_id = original_iid;
      inst->backup_internal_card_id = (uint16_t)original_iid;
      inst->mana_color = cards_data[original_iid].color;
      inst->destroys_if_blocked = 0;
      inst->token_status &= ~STATUS_LEGACY_TYPECHANGE;
      remove_special_flags2(player, card, 0x484);
      if (!is_what(player, card, TYPE_EFFECT))
        reset_subtypes_from_instance(inst, iid);
      event_result = original_iid;
      recalculate = 1;
    }
    else
      event_result = iid;
    break;
  default:
    event_result = 0;
    recalculate = 1;
    break;
  }

  if (recalculate)
  {
    if (g_duel_active)
    {
      dispatch_event_raw(event);
      if ((land_can_be_played & 0x10000) && event == EVENT_CHANGE_TYPE)
      {
        int second_pass_iid = event_result;
        inst->internal_card_id = second_pass_iid;
        if (second_pass_iid != -1)
          inst->backup_internal_card_id = (uint16_t)second_pass_iid;
        land_can_be_played = (land_can_be_played & ~0x10000U) | 0x20000;
        dispatch_event_raw(EVENT_CHANGE_TYPE);
        land_can_be_played &= ~0x20000U;
      }
    }
    if (event == EVENT_ABILITIES)
    {
      card_instance_t *counter_instance = get_card_instance(player, card);
      uint32_t packed_types = counter_instance->targets[18].player;
      uint32_t packed_counts = counter_instance->targets[18].card;
      uint8_t counter_types[7] = {counter_instance->special_counter_type,
                                  packed_types,
                                  packed_types >> 8,
                                  packed_types >> 16,
                                  packed_types >> 24,
                                  packed_counts >> 16,
                                  packed_counts >> 24};
      uint8_t counter_counts[7] = {counter_instance->special_counters_byte,
                                   counter_instance->counters2,
                                   counter_instance->counters3,
                                   counter_instance->counters4,
                                   counter_instance->counters5,
                                   packed_counts,
                                   packed_counts >> 8};
      int has_ability_counter = 0;
      for (int i = 0; i < 7; ++i)
        if ((uint8_t)(counter_types[i] + 0x70U) < 11 && counter_counts[i])
        {
          has_ability_counter = 1;
          break;
        }
      if (has_ability_counter && (cards_data[iid].type & TYPE_PERMANENT) &&
          !(cards_data[iid].type & TYPE_EFFECT))
      {
        if (count_counters(player, card, COUNTER_DEATHTOUCH))
          inst->ext->sp_keywords |= SP_KEYWORD_DEATHTOUCH;
        if (count_counters(player, card, COUNTER_HEXPROOF))
          inst->ext->sp_keywords |= SP_KEYWORD_HEXPROOF;
        if (count_counters(player, card, COUNTER_INDESTRUCTIBLE))
          inst->ext->sp_keywords |= SP_KEYWORD_INDESTRUCTIBLE;
        if (count_counters(player, card, COUNTER_LIFELINK))
          inst->ext->sp_keywords |= SP_KEYWORD_LIFELINK;
        if (count_counters(player, card, COUNTER_MENACE))
          inst->ext->sp_keywords |= SP_KEYWORD_MENACE;
        if (count_counters(player, card, COUNTER_VIGILANCE))
          inst->ext->sp_keywords |= SP_KEYWORD_VIGILANCE;
        if (count_counters(player, card, COUNTER_DOUBLE_STRIKE))
          event_result |= KEYWORD_DOUBLE_STRIKE;
        if (count_counters(player, card, COUNTER_FIRST_STRIKE))
          event_result |= KEYWORD_FIRST_STRIKE;
        if (count_counters(player, card, COUNTER_FLYING))
          event_result |= KEYWORD_FLYING;
        if (count_counters(player, card, COUNTER_REACH))
          event_result |= KEYWORD_REACH;
        if (count_counters(player, card, COUNTER_TRAMPLE))
          event_result |= KEYWORD_TRAMPLE;
      }
      unsigned int restrictions = player_bits[player];
      if ((restrictions & 0x1001f0) && (cards_data[iid].type & TYPE_PERMANENT) &&
          !(cards_data[iid].type & TYPE_EFFECT))
      {
        if (restrictions & 0x10)
          event_result &= ~KEYWORD_FIRST_STRIKE;
        if (restrictions & 0x20)
          event_result &= ~KEYWORD_FLYING;
        if (restrictions & 0x40)
          inst->ext->sp_keywords &= ~SP_KEYWORD_DEATHTOUCH;
        if (restrictions & 0x80)
          event_result &= ~KEYWORD_TRAMPLE;
        if (restrictions & 0x100000)
          event_result &= ~KEYWORD_SHROUD;
        if (restrictions & 0x100)
          inst->ext->sp_keywords &= ~SP_KEYWORD_HEXPROOF;
      }
      if (is_humiliated(player, card, AL_KEY))
      {
        if (is_humiliated(player, card, AL_DEFENDER))
          event_result |= KEYWORD_DEFENDER;
        if (is_humiliated(player, card, AL_INDESTRUCTIBLE))
          inst->ext->sp_keywords |= SP_KEYWORD_INDESTRUCTIBLE;
      }
      if ((inst->ext->sp_keywords & SP_KEYWORD_HASTE) &&
          !is_humiliated(player, card, AL_KEY))
        inst->state &= ~STATE_SUMMONSICK_BOTH;
    }
    else if (event == EVENT_POWER)
    {
      if (inst->token_status & STATUS_BERSERK)
        event_result = (unsigned int)event_result * 2U;
    }
  }
  unsigned int result = event_result;
  if (event == EVENT_TOUGHNESS &&
      (inst->state & (STATE_OUBLIETTED | STATE_INVISIBLE | STATE_IN_PLAY)) == STATE_IN_PLAY &&
      (cards_data[iid].type & TYPE_CREATURE) &&
      (event_result <= 0 || event_result <= inst->damage_on_card) &&
      !(inst->token_status & STATUS_CANNOT_BE_DESTROYED) && !(land_can_be_played & 0x204) &&
      !g_nested_trigger_depth)
  {
    kill_card(player, card, event_result < 1 ? KILL_STATE_BASED_ACTION : KILL_DESTROY);
    regenerate_or_graveyard_triggers();
  }
  if (g_duel_active)
    pop_affected_card_stack();
  if (event == EVENT_CHANGE_TYPE)
  {
    inst->internal_card_id = result;
    if (result != UINT32_MAX)
      inst->backup_internal_card_id = (uint16_t)result;
    if ((cards_data[result].extra_ability & EA_MANA_SOURCE) ||
        (is_what(player, card, TYPE_LAND) && mana_colors_added[player]))
    {
      uint16_t id = cards_data[result].id;
      unsigned int mana = get_color_of_mana_produced_by_id(id, inst->info_slot, player);
      if (mana == UINT32_MAX)
        mana = (uint8_t)cards_ptr[id]->mana_source_colors;
      inst->mana_color = (uint8_t)mana;
      if (is_what(player, card, TYPE_LAND))
        inst->mana_color |= (uint8_t)mana_colors_added[player];
      int override = check_special_flags2(player, card, 0x40);
      uint8_t current_mana = inst->mana_color;
      if (override)
        for (int color = 0; color < 7; ++color)
        {
          unsigned int bit = color_to_color_test(color);
          if (current_mana & bit)
          {
            int bit_to_remove = color_to_color_test(color);
            inst->mana_color &= (uint8_t)bit_to_remove ^ 0xff;
            unsigned int replacement = color_to_color_test(mana_color_override[player][color]);
            current_mana = (uint8_t)(replacement | inst->mana_color);
            inst->mana_color = current_mana;
          }
          else
            current_mana = inst->mana_color;
        }
      inst->card_color = current_mana;
    }
    if ((inst->token_status & STATUS_ANIMATED) && !(cards_data[result].type & TYPE_CREATURE))
      for (int p = 0; p < 2; ++p)
        for (int c = 0; c <= active_cards_count[p]; ++c)
          if (in_play(p, c))
          {
            card_instance_t *aura = get_card_instance(p, c);
            if (aura->damage_target_player == player && aura->damage_target_card == card &&
                (cards_data[aura->internal_card_id].type & TYPE_ENCHANTMENT) &&
                cards_ptr[cards_data[aura->internal_card_id].id]->subtype1 == 44)
              kill_card(p, c, KILL_STATE_BASED_ACTION);
          }
    if (inst->state & STATE_IN_PLAY)
      event_flags |= cards_data[inst->internal_card_id].extra_ability & 0x1ffc0000;
    return result;
  }
  if (event == EVENT_ABILITIES)
  {
    inst->regen_status = result;
    if ((inst->ext->sp_keywords & SP_KEYWORD_DEATHTOUCH) &&
        (cards_data[iid].type & TYPE_PERMANENT) && !(cards_data[iid].type & TYPE_EFFECT))
      inst->destroys_if_blocked |= 0x10;
  }
  else if (event == EVENT_POWER)
    inst->power = (int16_t)result;
  else if (event == EVENT_TOUGHNESS)
    inst->toughness = (int16_t)result;
  else if (event == EVENT_SET_COLOR)
    inst->color = (uint8_t)result & COLOR_TEST_ANY_COLORED;
  raw_base_pt = saved_raw_base;
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
void set_base_pt(int newval);
void set_characteristic_pt(int newval);

/* ManalinkEh 2022: 0x26cd650; original functions/events.c:1190. */
void set_base_pt(int newval)
{
  int old_base_pt;

  old_base_pt = raw_base_pt;
  raw_base_pt = newval;
  raw_characteristic_pt = raw_characteristic_pt & 0xffffbfff;
  event_result = (int32_t)((uint32_t)event_result +
                          ((uint32_t)newval - (uint32_t)old_base_pt));
  return;
}

/* ManalinkEh 2022: 0x26cd690; original functions/events.c:1200. */
void set_characteristic_pt(int newval)
{
  if ((((unsigned int)raw_characteristic_pt >> 8 & 0xff) & 0x40) != 0)
  {
    event_result = event_result + (newval - raw_base_pt);
    raw_base_pt = newval;
  }
  return;
}

void event_activate_then_duplicate_into_stack(int player, int card, int event, int new_attacking_card_controller, int new_attacking_card)
{
  /* Normally, when a card is activated, 12 steps happen in activate() (at 0x434040) and two more happen elsewhere:
   * 1) If it's the AI playing the activation, do some unclear proprocessing, including (among much else) sending EVENT_GET_SELECTED_CARD to player/card and
   *    showing a dialog announcing the activation.
   * 2) an activation card is put onto the stack (put_card_or_activation_onto_stack() at 0x436550).  This involves:
   *   a) creating an activation card
   *   b) memcpy()ing the card as it currently exists into the activation card
   *   c) setting the activation card's internal_card_id (back to iid_activation_card), original_internal_card_id (to the creating card's internal_card_id),
   *      unknown0x14 and kill_code (both to 0), timestamp (back to what it was before the memcpy), and parent_controller and parent_card
   *   d) setting various stack variables (stack_size, stack_cards[], stack_damage_targets[], stack_phase[], and dword_628674[stack_size].
   *   For basic lands and for EVENT_RESOLVE_SPELL and EVENT_RESOLVE_TRIGGER, a, b, and c are skipped; a references to actual card is left on the stack.
   * 3) EVENT_UNKNOWN80 is dispatched, setting affected_card_controller/affected_card to player/card.  If event_result is set to nonzero, the top card of the
   *    stack is obliterated, and nothing else is done.  Nothing in the exe handles this event, and it seems dangerous to me - what if the AI keeps on trying
   *    to activate a card, and something responding to EVENT_UNKNOWN80 keeps cancelling it?
   * 4) Set tapped_for_mana_color = -1, and add an entry to the mana_spent stack (sub_430150)
   * 5) EVENT_ACTIVATE is dispatched to player/card (not the activation card) with dispatch_event_to_single_card() at 0x435B50 (almost the same as
   *    dispatch_event_with_attacker_to_one_card() above)
   * 6) If it's a network game and the remote player is activating, then (usually) show a dialog saying what he's doing.
   * 7) If cancel == 1, refund all mana spent since the mana_spent stack state was stored (sub_430200), obliterate the top card of the stack, and return.
   * 8) Remove the mana_stack entry (so cancelling afterwards won't refund mana spent so far) (sub_4301E0)
   * 9) Dispatch either EVENT_PLAY_ABILITY or both EVENT_TAP_CARD and EVENT_TAPPED_TO_PLAY_ABILITY, with affected_card_controller/affected_card == player/card.
   *10) Copy player/card's current state into the activation card again, preserving the same things as before; and set the activation card's display_pic_csv_id
   *    to the original card's csvid and display_pic_num to 0.  (Digression: The original_internal_card_id saved when the activation_card was put on the stack
   *    is copied anew from player/card here, instead of preserving it; if player/card's internal_card_id is now -1 since it's no longer in play due to
   *    sacrificing or exiling or bouncing itself during activation or anything else that immediately gets rid of it, then it's pulled from player/card's
   *    original_internal_card.  This is why sacrifice-self abilities from Clone etc. get screwed up.)  Also set dword_628674[] and stack_damage_targets[] again
   *    here.  This happens in recopy_card_onto_stack().  If the top card on the stack wasn't an activation card (see the note at the end of step 2), then none
   *    of this except setting dword_628674[] and stack_damage_targets[] is done.
   *11) Play WAV_TAP if the card is flagged EA_MANA_SOURCE and tapped_for_mana_color != -1; otherwise, play WAV_FASTFX.
   *12) And call set_stack_damage_targets() at 0x436700, which updates stack_damage_targets[] for everything on the stack.  I suspect it's redundant.
   *13) Outside of activate(), (usually) allow response.
   *14) Eventually, resolve_top_card_on_stack() at 0x436740 is called.  This puts the stack down to the point it was at when this instance of activate() was
   *    called.
   *
   * When a mana source is activated during charge_mana(), an abbreviated sequence occurs:
   * 1) Send EVENT_CAN_ACTIVATE to the card, unless it's a land.  (I think that exception should be removed, but it's a problem for another day.)  On failure,
   *    do nothing else.
   * 2) Same as step 2 from above - activation card onto the stack.
   * 3) Store the colors of mana currently needed in needed_mana_colors.
   * 4) set tapped_for_mana_color = -1.
   * 5) Dispatch EVENT_ACTIVATE, just as in step 5 from above.
   * 6) (nothing)
   * 7) If cancel == 1, obliterate the top card of the stack, and continue onto the rest of charge_mana().
   * 8) (nothing)
   * 9) If the activating card is newly tapped, dispatch EVENT_TAP_CARD, otherwise do nothing.
   *10) (nothing)
   *11) Play WAV_TAP.
   *12) (nothing)
   *13) (nothing)
   *14) call resolve_top_card_on_stack() at 0x436740 and TENTATIVE_reassess_all_cards() at 0x472260.
   *
   * The sequence during autotap_mana_source() at 0x42FED0 is similar, though it at least always checks EVENT_CAN_ACTIVATE.
   *
   * The bug is that step 10 is missing - if a card sets its internal targets or info_slot during EVENT_ACTIVATE, and its EVENT_RESOLVE_ACTIVATION looks at
   * them, it'll see them as they were *before* EVENT_ACTIVATE was called.  This breaks mana sources that have non-mana abilities.  I'm currently seeing it with
   * Iceberg - if you activate it to add an ice counter (which takes place at resolution), and then cast something and pay for its mana with Iceberg's mana
   * ability, you get a free counter.  I'm mostly just amazed that it hasn't seemed to occur on lands with non-mana abilities.
   *
   * So what I'm doing is replacing the call to dispatch_event_to_single_card(player, card, EVENT_ACTIVATE, 1-player, -1) with a call here, ignoring the last
   * three parameters.  Here, we dispatch EVENT_ACTIVATE, then manually duplicate most of recopy_card_onto_stack() from step 10 in the full sequence. */

  dispatch_event_with_attacker_to_one_card(player, card, event, new_attacking_card_controller, new_attacking_card);

  // 0x436450
  int idx = stack_size - 1;
  card_instance_t* instance = get_card_instance(stack_cards[idx].player, stack_cards[idx].card);
  if (instance->internal_card_id == activation_card)
	{
	  int old_parent_card = instance->parent_card;
	  int old_parent_controller = instance->parent_controller;
	  uint32_t old_timestamp = instance->timestamp;
	  card_instance_t* parent_instance = get_card_instance(instance->parent_controller, instance->parent_card);

	  if (parent_instance->state & STATE_DONT_RECOPY_ONTO_STACK)
		parent_instance->state &= ~STATE_DONT_RECOPY_ONTO_STACK;
	  else
		{
		  STATIC_ASSERT(sizeof(card_instance_t) == 300, card_instance_t_size_differs_from_exe);
		  memcpy_inst_and_ext(instance, parent_instance);
		  instance->internal_card_id = activation_card;
		  instance->unknown0x14 = 0;
		  instance->kill_code = 0;
		  instance->state |= STATE_IN_PLAY;
		  instance->parent_controller = old_parent_controller;
		  instance->parent_card = old_parent_card;
		  instance->timestamp = old_timestamp;

		  if (parent_instance->internal_card_id != -1)
			instance->original_internal_card_id = parent_instance->internal_card_id;
		  // Begin additions
		  else
			instance->original_internal_card_id = parent_instance->backup_internal_card_id;
		  // End additions

		  // Begin removals
		  // if (instance->original_internal_card_id < iid_damage_card
		  //     || instance->original_internal_card_id >= iid_damage_card + 47)
		  //   {
		  //     instance->display_pic_csv_id = cards_data[parent_instance->original_internal_card_id].id;
		  //     instance->display_pic_num = 0;
		  //   }
		  // End removals
		}

	  // Begin additions
	  instance->token_status &= ~STATUS_DYING;	// so kill_card() will reap it, even if the original card sacrificed itself as an activation card
	  // End additions
	}
  // Begin removals
  // if (ai_is_speculating != 1)
  //   dword_628674[idx] = arg1;	// 0 for EA_MANA_SOURCE cards that set tapped_for_mana_color; else 1
  // stack_damage_targets[idx].player = instance->damage_target_player;
  // stack_damage_targets[idx].card = instance->damage_target_card;
  // End removals
}

int current_trigger_or_event_is_forced(void)
{
  // 0x476F10

  /* I'm not positive what this function's exact effects are, but if a trigger that's not listed here runs, and the player has a stop on the current phase, he's
   * always prompted to select "Done" even if there's only one (forced) trigger handler, or none at all. */

  uint8_t ctoe = g_current_action_event_code & 0xff;	// TENTATIVE_current_trigger_or_event
  if (ctoe == EVENT_UNKNOWN8E)
	return must_attack ? 1 : 0;

  if (ctoe != EVENT_CAN_SKIP_TURN
	  && ctoe != 107	//0x6B or -148 - I can't find any uses.
	  && ctoe != EVENT_CAST_SPELL
	  && ctoe != EVENT_ACTIVATE
	  && ctoe != EVENT_DEAL_DAMAGE
	  && ctoe != 111	//0x6F or -144 - I can't find any uses.
	  && ctoe != EVENT_REGENERATE
	  && ctoe != EVENT_RESOLVE_SPELL
	  && ctoe != EVENT_RESOLVE_ACTIVATION
	  && ctoe != EVENT_CAN_ACTIVATE
	  && ctoe != EVENT_CAN_CAST
	  && ctoe != 117	//0x75 or -138 - I can't find any uses.
	  && ctoe != 118	//0x76 or -137 - I can't find any uses.
	  && ctoe != EVENT_GRAVEYARD_FROM_PLAY
	  && ctoe != EVENT_BLOCK_LEGALITY
	  && ctoe != EVENT_ATTACK_LEGALITY
	  && ctoe != 122	//0x7A or -133 - I can't find any uses.
	  && ctoe != 123	//0x7B or -132 - I can't find any uses.
	  && ctoe != 124	//0x7C or -131 - I can't find any uses.
	  && ctoe != EVENT_TRIGGER
	  && ctoe != EVENT_RESOLVE_TRIGGER
	  && ctoe != EVENT_COUNT_MANA
	  && ctoe != EVENT_UNKNOWN80
	  && ctoe != EVENT_TAP_CARD
	  && ctoe != EVENT_UNTAP
	  && ctoe != EVENT_UNTAP_CARD
	  && ctoe != EVENT_SET_UNTAP_COST
	  && ctoe != EVENT_SETUP_UPKEEP_COSTS
	  && ctoe != EVENT_UPKEEP_COSTS_UNPAID
	  && ctoe != EVENT_CHECK_UPK_PAYMENT
	  && ctoe != EVENT_CHECK_UNTAP_PAYMENT
	  && ctoe != EVENT_MUST_ATTACK
	  && ctoe != EVENT_UNKNOWN8E
	  && ctoe != EVENT_SHOULD_AI_PLAY
	  && ctoe != 200	//0xC8 or -55 - I didn't even attempt to search.
	  && ctoe != TRIGGER_UPKEEP
	  && ctoe != TRIGGER_DURING_UPKEEP
	  && ctoe != TRIGGER_END_UPKEEP
	  && ctoe != TRIGGER_END_COMBAT
	  && ctoe != TRIGGER_EOT
	  && ctoe != TRIGGER_DRAW_PHASE
	  && ctoe != TRIGGER_REPLACE_CARD_DRAW
	  && ctoe != TRIGGER_CARD_DRAWN
	  && ctoe != TRIGGER_DISCARD
	  && ctoe != TRIGGER_TAP_CARD
	  && ctoe != TRIGGER_SPELL_CAST
	  && ctoe != TRIGGER_LEAVE_PLAY
	  && ctoe != TRIGGER_GRAVEYARD_FROM_PLAY
	  && ctoe != TRIGGER_GRAVEYARD_ORDER
	  && ctoe != TRIGGER_DEAL_DAMAGE
	  && ctoe != TRIGGER_BOUNCE_PERMANENT
	  && ctoe != TRIGGER_MUST_ATTACK
	  && ctoe != TRIGGER_COMES_INTO_PLAY
	  && ctoe != TRIGGER_PAY_TO_ATTACK
	  && ctoe != TRIGGER_XTRIGGER
	  )
	return 1;

  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26cbf50; original functions/events.c:553. */
void allow_response_to_trigger(int player, int card)
{
  const char *name = get_displayed_card_name(player, card);
  sprintf((char *)(global_all_purpose_buffer), "Trigger %s", name);
  int a1 = g_phase_stop_suppressed && (player == 0 || (trace_mode & 2)) ? -1 : -2;
  allow_response(a1, current_phase, global_all_purpose_buffer,
                                                     EVENT_RESOLVE_TRIGGER);
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline-only helpers; native code remains in their callers. */
static int has_ability_counters(int player, int card)
{
  /* Extra counter slots share the final target entry in the native layout. */
  const unsigned char *p = (const unsigned char *)get_card_instance(player, card);
  return (p[0x51] >= COUNTER_DEATHTOUCH && p[0x51] <= COUNTER_VIGILANCE && p[0]) ||
         (p[0x104] >= COUNTER_DEATHTOUCH && p[0x104] <= COUNTER_VIGILANCE && p[1]) ||
         (p[0x105] >= COUNTER_DEATHTOUCH && p[0x105] <= COUNTER_VIGILANCE && p[2]) ||
         (p[0x106] >= COUNTER_DEATHTOUCH && p[0x106] <= COUNTER_VIGILANCE && p[3]) ||
         (p[0x107] >= COUNTER_DEATHTOUCH && p[0x107] <= COUNTER_VIGILANCE && p[0x121]) ||
         (p[0x10a] >= COUNTER_DEATHTOUCH && p[0x10a] <= COUNTER_VIGILANCE && p[0x108]) ||
         (p[0x10b] >= COUNTER_DEATHTOUCH && p[0x10b] <= COUNTER_VIGILANCE && p[0x109]);
}
#endif
