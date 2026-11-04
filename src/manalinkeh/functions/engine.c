// -*- c-basic-offset:2 -*-
#include "manalink.h"

/* Canonical recovered helper declarations. */
void allow_response_to_activation(int player, int card);
int effect_asterisk(int player, int card, event_t event);
int create_legacy_effect_exe(int player, int card, int iid, int t_player, int t_card);
int create_legacy_effect_from_iid(int player, int iid, int (*func_ptr)(int, int, event_t),
                                       int t_player, int t_card);
int can_attack(int player, int card);
int card_instances_should_be_displayed_identically(card_instance_t *inst1,
                                                        card_instance_t *inst2);
int is_anyone_dead(void);
int charge_mana(int player, color_t color, int amount);
int is_a_tappable_mana_source(int player, int card);
void force_activation_for_mana(int player, int card, color_test_t colors);
void tap_card_for_mana(int player, int card);
int autotap_mana_source(int player, int card);
void format_manacost_into_global_allpurpose_buffer(int ignored, int *mana_cost_array,
                                                        int x_so_far, int max_x);
int is_legal_block(int blocking_player, int blocking_card, int blocked_player,
                        int blocked_card);
void recalculate_all_cards_in_play(void);
void human_assign_blockers(int player);
void put_card_or_activation_onto_stack(int player, int card, event_t event, int a4, int a5);
void recopy_card_onto_stack(int a1);
int finalize_activation(int player, int card);
int put_card_on_stack3(int player, int card);
void count_colors_of_lands_in_play(void);
void phase_changed(int player, int new_phase);
int determine_start_duel_winner(void);
int untap_phase(int player);
int upkeep_phase(int player);
int sub_495BE0(int plr);
int is_legal_block_impl(int blocking_player, int blocking_card, int blocked_player,
                             int blocked_card, keyword_t abils, int ignored_previously_landwalks);

/* October 2022 per-player card and counterability flag storage. */
uint32_t card_flags[2];
uint8_t your_spells_cant_be_countered_by_color[2];

/* October 2022 mana modifier table storage. */
int mana_colors_added[2];
int mana_color_override[2][8];
int mana_quantity_override[2][8];
int multi_mana_transmutation[2][2];

/* October 2022 dice modifier cache storage. */
int add_to_die_rolls[2];
int roll_extra_dice[2];

/* October 2022 .bss cache @ 0x2797200; declared in target manalink.h. */
uint8_t card_counts[2][7];

// These are fairly common here, but shouldn't be used elsewhere
int target_player_skips_untap(int player, int card, event_t event);

extern int hack_ai_decision_phase_upkeep;
int ai_decision_phase(int player, int *phase_code_to_go_to, int *becomes_second_arg_of_main_phase_and_discard_phase, int *becomes_third_arg_of_main_phase);
void mana_burn(void);

static int check_attack_legality_if_enchanting_affected_card(int player, int card)
{
  //0x4b7110
  // The exe version is a callback for call_function_for_each_card_in_play(), and the temporaries are saved in can_attack_instead of here.

  push_affected_card_stack();

  event_result = 0;
  affected_card_controller = player;
  affected_card = card;

  // Begin call_function_for_each_card_in_play()
  /* (This differs from dispatch_event_raw() in that the cards are examined in id order, not in timestamp order.  Also, it calls an arbitrary function instead
   * of calling each card's own function with a given event.) */
  {
	int p, c;
	for (p = 0; p < 2; ++p)
	  for (c = 0; c < active_cards_count[p]; ++c)
		if (in_play(p, c))
		  {
			// Begin exe version of check_attack_legality_if_enchanting_affected_card()
			card_instance_t* instance = get_card_instance(player, card);
			if (affect_me(instance->damage_target_player, instance->damage_target_card) && affected_card != -1)
			  {
				call_card_function_i(instance, player, card, EVENT_ATTACK_LEGALITY);
				if (cancel == 1)
				  {
					++event_result;
					cancel = 0;
				  }
			  }
			// End exe version of check_attack_legality_if_enchanting_affected_card()
		  }
  }
  // End call_function_for_each_card_in_play()

  int result = event_result;

  pop_affected_card_stack();

  return result;
}

// If an effect is created with source hack_force_effect_change_source.from, change it to be hack_force_effect_change_source.to before sending it to the exe.
HackForceEffectChangeSource hack_force_effect_change_source = {{-1,-1}, {-1,-1}};

// Creates a sourceless effect card, with name/text/image from iid.

int hack_xx = 1;

void event_activate_then_duplicate_into_stack(int player, int card, int event, int new_attacking_card_controller, int new_attacking_card);
void human_autotap_for_mana(int player, int* amt, int* unknown_v46, int unknown_v47);
int pay_mana_maximally_satisfied(int* pay_mana_array, int x_val, int max_x_val);
int try_to_pay_for_mana_by_autotapping(int player, int* amt, int* unknown_v46, int autotap_flags, int unknown_v47);
extern int pay_mana_xbugrwaU[8];
#define APPLY_MANA_IN_POOL_TO_COSTS(total_paid)	\
	apply_mana_in_pool_to_cost(player, amtspaid, total_paid, amt_of_nonartifact_mana);	\
	apply_mana_in_pool_to_colorless_cost(player, amtspaid, total_paid, amt_of_nonartifact_mana, &x_value, max_x_value);	\
	apply_mana_in_pool_to_x_cost(player, amtspaid, total_paid, amt_of_nonartifact_mana, &x_value, max_x_value)

#define amt_to_apply_to_cost					get_mana_payment_amount
#define apply_mana_in_pool_to_colorless_cost	auto_pay_generic_mana
#define apply_mana_in_pool_to_cost				auto_pay_colored_mana
#define apply_mana_in_pool_to_x_cost			auto_pay_variable_mana
#define apply_mana_to_cost						spend_mana_for_cost
#define calculate_needed_mana_colors			get_required_mana_color_mask
#define TENTATIVE_update_mana_spent				refund_paid_mana

#define EXE_2D_ARR(typ, dim1, dim2, addr)		((typ(*)[dim2])addr)	// dim1 for documentation only

#define TENTATIVE_color_clicked_in_mana_summary_pane	g_recorded_action_phase
#define global_all_purpose_buffer				((char*)(global_all_purpose_buffer))
#define WILDGUESS_phase_to_run_to				g_stop_phase

#define WILDGUESS_whose_phase_to_run_to			g_stop_phase_player

#undef APPLY_MANA_IN_POOL_TO_COSTS

#undef amt_to_apply_to_cost
#undef apply_mana_in_pool_to_colorless_cost
#undef apply_mana_in_pool_to_cost
#undef apply_mana_in_pool_to_x_cost
#undef apply_mana_to_cost

#undef calculate_needed_mana_colors
#undef copy_mana_pool_to_display

#undef resolve_top_card_on_stack
#undef single_color_test_bit_to_color_t
#undef TENTATIVE_update_mana_spent

#undef EXE_2D_ARR

#undef TENTATIVE_color_clicked_in_mana_summary_pane
#undef global_all_purpose_buffer
#undef WILDGUESS_phase_to_run_to

#undef WILDGUESS_whose_phase_to_run_to

extern target_t totally_bletcherous_hack_dont_reset_x_value;

static void comes_into_play_trigger(int player, int card)
{
  // 0x401bb0.  Not moved out of exe, since it has a non-standard calling convention and is only called from put_card_on_stack3().

  push_affected_card_stack();
  get_card_instance(player, card)->regen_status |= KEYWORD_RECALC_ALL;
  dispatch_trigger2(current_turn, TRIGGER_COMES_INTO_PLAY, gs_card_into_play_0091c840, 0, player, card);	//PROMPT_SPECIALFEPHASE[0]
  pop_affected_card_stack();
}

extern int event_rval;	// set only when calling our event dispatching functions directly

void event_activate_then_duplicate_into_stack(int player, int card, int event, int new_attacking_card_controller, int new_attacking_card);
// Interface for exe calls.

// Note that this doesn't check EVENT_CAN_ACTIVATE itself, or whether {player,card} has EA_MANA_SOURCE.

// Returns the most common color either in Deck[] (if plr == -1), or in initial_library[plr].

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x26a8b30; original functions/engine.c:226. */
void hook_helper_for_startduel(void)
{
  reset_duel_globals();
  for (int player = 0; player < 2; ++player)
    for (int card = 0; card < 151; ++card)
    {
      get_card_instance(player, card)->ext = &card_ext[player][card];
      get_displayed_card_instance(player, card)->ext = &disp_card_ext[player][card];
    }
}

/* ManalinkEh 2022: 0x26a9b30; original functions/engine.c:1401. */
void count_mana(void)
{
  for (int color = 0; color < 8; ++color)
  {
    raw_mana_available[1][color] = 0;
    raw_mana_available[0][color] = 0;
    mana_color_override[1][color] = color;
    mana_color_override[0][color] = color;
    mana_quantity_override[1][color] = -1;
    mana_quantity_override[0][color] = -1;
  }
  raw_mana_available_hex[0][0] = -1;
  raw_mana_may_be_spent_as_color[1][0] = -1;
  raw_mana_may_be_spent_as_color[0][0] = -1;
  raw_mana_available_hex[1][0] = -1;
  mana_colors_added[1] = 0;
  mana_colors_added[0] = 0;
  multi_mana_transmutation[1][1] = 0;
  multi_mana_transmutation[0][1] = 0;
  multi_mana_transmutation[1][0] = 0;
  multi_mana_transmutation[0][0] = 0;

  int capacity = active_cards_count[0];
  if (capacity < active_cards_count[1])
    capacity = active_cards_count[1];
  int mana_sources[2][capacity];
  memset(mana_sources, 0xff, sizeof(mana_sources));
  int source_counts[2] = {0, 0};
  for (int player = 0; player < 2; ++player)
  {
    remove_special_flags2(player, -1, 0x40);
    dispatch_event(player, 0, EVENT_MODIFY_MANA_PROD);
    for (int card = 0; card < active_cards_count[player]; ++card)
    {
      card_instance_t *instance = in_play(player, card);
      if (instance != NULL)
      {
        if ((cards_data[instance->internal_card_id].extra_ability & 0x1000) != 0)
          mana_sources[player][source_counts[player]++] = card;
        uint16_t csvid = cards_data[instance->internal_card_id].id;
        if (csvid == CARD_ID_GLOOM || (csvid == CARD_ID_SUNGLASSES_OF_URZA && (instance->state & 0x10) == 0))
          dispatch_event_to_single_card(player, card, EVENT_COUNT_MANA, -1, -1);
      }
      instance = in_hand(player, card);
      if (instance != NULL)
      {
        uint16_t csvid = cards_data[instance->internal_card_id].id;
        if (csvid == CARD_ID_ELVISH_SPIRIT_GUIDE || csvid == CARD_ID_SIMIAN_SPIRIT_GUIDE)
          dispatch_event_to_single_card(player, card, EVENT_COUNT_MANA, -1, -1);
      }
    }
    if (is_id_in_grave(player, 0x176c) != 0 && ((uint32_t)player_bits[player] & 0x02000000U) == 0)
      mana_colors_added[player] |= 0x28;
  }
  for (int player = 0; player < 2; ++player)
    for (int source = 0; source < source_counts[player]; ++source)
      dispatch_event(player, mana_sources[player][source], EVENT_COUNT_MANA);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x26abfd0; original functions/engine.c:2502. */
void undeclare_attacker(int player, int card)
{
  card_instance_t *inst = get_card_instance(player, card);
  inst->state &= ~STATE_ATTACKING;
  if (inst->blocking == 0xff)
    return;
  inst->blocking = 0xff;

  int band_base = -1;
  int num_banded = 0;
  for (int c = 0; c < active_cards_count[player]; ++c)
  {
    card_instance_t *banded = in_play(player, c);
    if (banded && (banded->state & STATE_ATTACKING) && card == (unsigned int)banded->blocking)
    {
      banded->blocking = (uint8_t)c;
      if (band_base == -1)
        band_base = c;
      ++num_banded;
    }
  }
  if (num_banded == 1)
    get_card_instance(player, band_base)->blocking = 0xff;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int is_being_cast(int player, int card);

/* ManalinkEh 2022: 0x26a83e0; original functions/engine.c:2539. */
static int qsort_compare_attacker_effectiveness(const void *lhs, const void *rhs)
{
  return (unsigned int)(*(int *)((int)lhs + 4) < *(int *)((int)rhs + 4)) -
         (unsigned int)(*(int *)((int)rhs + 4) < *(int *)((int)lhs + 4));
}

/* ManalinkEh 2022: 0x26a8410; original functions/engine.c:2547. */
static int qsort_cardvalue_ascending(const void *lhs, const void *rhs)
{
  return (unsigned int)(*(int *)rhs < *(int *)lhs) - (unsigned int)(*(int *)lhs < *(int *)rhs);
}

/* ManalinkEh 2022: 0x26adb20; original functions/engine.c:3232. */
int is_being_cast(int player, int card)
{
  card_instance_t *inst = get_card_instance(player, card);
  if (!inst)
    return 0;
  if (inst->internal_card_id == damage_card)
    return inst->targets[7].player & 2;
  if (on_stack(player, card))
    return inst->internal_card_id != activation_card;
  if (in_hand(player, card))
    return (land_can_be_played & 0x400000) == 0;
  if (!in_play(player, card))
    return 0;
  return is_what(player, card, 199) == 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26af860; original functions/engine.c:1523. */
STATIC_ASSERT(offsetof(card_data_t, cc) + 1 == 0x2c, spell_cost_x_byte);
STATIC_ASSERT(offsetof(card_data_t, type) == 0x28, spell_cost_type_byte);
/* ManalinkEh 2022: 0x26a9420; original functions/engine.c:710. */
int create_legacy_activate_from_iid(int player, int iid, CardFunction func_ptr, int t_player,
                                    int t_card)
{
  int card = add_card_to_hand(player, LEGACY_EFFECT_ACTIVATED);
  if (card != -1)
  {
    card_instance_t *legacy = get_card_instance(player, card);
    --hand_count[player];
    legacy->state = player == 1 ? 0x1002 : 2;
    uint8_t color = cards_data[iid].color;
    legacy->damage_source_player = player;
    legacy->color = color;
    legacy->initial_color = color;
    legacy->card_color = color;
    legacy->mana_color = color;
    int csvid = (uint16_t)cards_data[iid].id;
    legacy->damage_source_card = card;
    legacy->display_pic_csv_id = csvid;
    int num_pics = cards_ptr[csvid]->num_pics;
    legacy->display_pic_num = num_pics > 1 ? internal_rand(num_pics) : 0;
    legacy->damage_target_player = t_player;
    legacy->damage_target_card = t_card;
    if (t_player != -1 && t_card != -1)
      get_card_instance(t_player, t_card)->regen_status |= 0x1f000000;
    legacy->info_slot_fn = func_ptr;
  }
  return card;
}

#endif

#ifndef SHANDALAR
/* Pay through source-owned mana pools and keep the first payment for X-cost rollback. */
int charge_spell_cost(card_data_t *cd, int player, int card)
{
  uint8_t printed_generic_cost;
  int8_t signed_mana_adjustment;
  uint32_t saved_mana_amount;
  bool mana_kind_changed;
  char *cost_or_mana_byte;
  int available_mana_or_pool;
  unsigned char color_index;
  int8_t *engine_mana_cursor;
  unsigned int available_color_bits;
  int *mana_value_cursor;
  int *engine_paid_cursor;
  int (*pool_clear_cursor)[8];
  int chosen_x_or_mana_adjustment;
  int payment_result_or_mana_value;
  int saved_mana_adjustments[8];
  int saved_mana_pool[8] = {0};

  pay_mana_xbugrwaU[6] = 0;
  cost_or_mana_byte = manalinkeh_cost_scratch;
  printed_generic_cost = cd->cc[1];
  do
  {
    pay_mana_xbugrwaU[(uintptr_t)cost_or_mana_byte - (uintptr_t)manalinkeh_cost_scratch] = (int)*cost_or_mana_byte;
    payment_result_or_mana_value = pay_mana_xbugrwaU[0];
    cost_or_mana_byte = cost_or_mana_byte + 1;
  } while (cost_or_mana_byte != (manalinkeh_cost_scratch + 6));
  if ((cd->type & 0x40) != 0)
  {
    pay_mana_xbugrwaU[0] = 0;
    pay_mana_xbugrwaU[6] = payment_result_or_mana_value;
  }
  payment_result_or_mana_value = pay_mana_xbugrwaU[6];
  if (((player == 0) || (((unsigned char)trace_mode & 2) != 0)) && (ai_is_speculating != 1))
  {
    payment_result_or_mana_value = 0;
    chosen_x_or_mana_adjustment = 0;
    if (printed_generic_cost != 0xff)
    {
      payment_result_or_mana_value = charge_mana_w_global_cost_mod(
          player, card, chosen_x_or_mana_adjustment, payment_result_or_mana_value);
      return payment_result_or_mana_value;
    }
    if (charge_mana_w_global_cost_mod(player, card, 0, 0) == 0)
    {
      return 0;
    }
    mana_value_cursor = mana_paid;
    engine_paid_cursor = saved_mana_pool;
    for (payment_result_or_mana_value = 7; payment_result_or_mana_value != 0;
         payment_result_or_mana_value = payment_result_or_mana_value + -1)
    {
      *engine_paid_cursor = *mana_value_cursor;
      mana_value_cursor = mana_value_cursor + 1u;
      engine_paid_cursor = engine_paid_cursor + 1u;
    }
    engine_mana_cursor = ((int8_t *)byte_55CF20);
    do
    {
      saved_mana_adjustments[(uintptr_t)engine_mana_cursor - (uintptr_t)byte_55CF20] = (int)*engine_mana_cursor;
      engine_mana_cursor = engine_mana_cursor + 1;
    } while (engine_mana_cursor != (byte_55CF20 + 8));
    if (((int8_t *)byte_55CF20)[0] < 0)
    {
      cost_or_mana_byte = (char *)&mana_pool[player][7];
      engine_mana_cursor = ((int8_t *)byte_55CF20);
      signed_mana_adjustment = ((int8_t *)byte_55CF20)[0];
      while (true)
      {
        engine_mana_cursor = engine_mana_cursor + 1;
        saved_mana_amount =
            (uint32_t)saved_mana_adjustments[(uintptr_t)engine_mana_cursor - (uintptr_t)(byte_55CF20 + 1)];
        mana_pool[player][(uintptr_t)engine_mana_cursor - (uintptr_t)(byte_55CF20 + 1)] =
            mana_pool[player][(uintptr_t)engine_mana_cursor - (uintptr_t)(byte_55CF20 + 1)] -
            (int)signed_mana_adjustment;
        mana_pool[player][7] -= (int)saved_mana_amount;
        if (engine_mana_cursor == (byte_55CF20 + 8))
          break;
        signed_mana_adjustment = *engine_mana_cursor;
      }
    }
    charge_mana(player, COLOR_COLORLESS, -1);
  }
  else
  {
    if ((cd->type & 0x40) != 0)
    {
      chosen_x_or_mana_adjustment = 6;
      pay_mana_xbugrwaU[6] = 0;
      payment_result_or_mana_value = charge_mana_w_global_cost_mod(
          player, card, chosen_x_or_mana_adjustment, payment_result_or_mana_value);
      return payment_result_or_mana_value;
    }
    payment_result_or_mana_value = charge_mana_w_global_cost_mod(player, card, 0, 0);
    if (printed_generic_cost != 0xff)
    {
      return payment_result_or_mana_value;
    }
    if (payment_result_or_mana_value == 0)
    {
      return 0;
    }
    mana_value_cursor = mana_paid;
    engine_paid_cursor = saved_mana_pool;
    for (payment_result_or_mana_value = 7; payment_result_or_mana_value != 0;
         payment_result_or_mana_value = payment_result_or_mana_value + -1)
    {
      *engine_paid_cursor = *mana_value_cursor;
      mana_value_cursor = mana_value_cursor + 1u;
      engine_paid_cursor = engine_paid_cursor + 1u;
    }
    engine_mana_cursor = ((int8_t *)byte_55CF20);
    do
    {
      saved_mana_adjustments[(uintptr_t)engine_mana_cursor - (uintptr_t)byte_55CF20] = (int)*engine_mana_cursor;
      engine_mana_cursor = engine_mana_cursor + 1;
    } while (engine_mana_cursor != (byte_55CF20 + 8));
    if (((int8_t *)byte_55CF20)[0] < 0)
    {
      cost_or_mana_byte = (char *)&mana_pool[player][7];
      engine_mana_cursor = ((int8_t *)byte_55CF20);
      signed_mana_adjustment = ((int8_t *)byte_55CF20)[0];
      while (true)
      {
        engine_mana_cursor = engine_mana_cursor + 1;
        mana_value_cursor = &mana_pool[player][(uintptr_t)engine_mana_cursor - (uintptr_t)(byte_55CF20 + 1)];
        *mana_value_cursor = *mana_value_cursor - (int)signed_mana_adjustment;
        mana_pool[player][7] -= saved_mana_adjustments[(uintptr_t)engine_mana_cursor - (uintptr_t)(byte_55CF20 + 1)];
        if (engine_mana_cursor == (byte_55CF20 + 8))
          break;
        signed_mana_adjustment = *engine_mana_cursor;
      }
    }
    payment_result_or_mana_value = max_x_value;
    if (player == 0)
    {
      max_x_value = has_mana(0, 7, 1);
      charge_mana(0, COLOR_COLORLESS, -1);
      max_x_value = payment_result_or_mana_value;
    }
    else
    {
      if (ai_is_speculating == 1)
      {
        if (internal_rand(2) == 0)
        {
          available_mana_or_pool = has_mana(player, 7, 1);
          payment_result_or_mana_value = 0;
          chosen_x_or_mana_adjustment = payment_result_or_mana_value;
          if (available_mana_or_pool != 0)
          {
            payment_result_or_mana_value = internal_rand(available_mana_or_pool);
            payment_result_or_mana_value = payment_result_or_mana_value + 1;
            chosen_x_or_mana_adjustment = payment_result_or_mana_value;
          }
        }
        else
        {
          payment_result_or_mana_value = has_mana(player, 7, 1);
          g_ai_recorded_choice = payment_result_or_mana_value;
          if ((life[0] < payment_result_or_mana_value) &&
              (chosen_x_or_mana_adjustment = internal_rand(3), chosen_x_or_mana_adjustment == 0))
          {
            g_ai_recorded_choice = life[0];
            payment_result_or_mana_value = life[0];
          }
          chosen_x_or_mana_adjustment = g_ai_recorded_choice;
          if ((max_x_value != -1) && (max_x_value < g_ai_recorded_choice))
          {
            g_ai_recorded_choice = max_x_value;
            payment_result_or_mana_value = max_x_value;
            chosen_x_or_mana_adjustment = g_ai_recorded_choice;
          }
        }
        g_ai_recorded_choice = chosen_x_or_mana_adjustment;
        record_ai_action_selection();
      }
      else
      {
        replay_ai_action_selection();
        payment_result_or_mana_value = g_ai_recorded_choice;
      }
      chosen_x_or_mana_adjustment = max_x_value;
      max_x_value = payment_result_or_mana_value;
      charge_mana(player, COLOR_COLORLESS, -1);
      max_x_value = chosen_x_or_mana_adjustment;
    }
  }
  signed_mana_adjustment = ((int8_t *)byte_55CF20)[0];
  payment_result_or_mana_value = (int)((int8_t *)byte_55CF20)[0];
  if (((int8_t *)byte_55CF20)[7] != 0)
  {
    engine_mana_cursor = ((int8_t *)byte_55CF20);
    mana_kind_changed = false;
    chosen_x_or_mana_adjustment = payment_result_or_mana_value;
    while (true)
    {
      engine_mana_cursor = engine_mana_cursor + 1;
      available_mana_or_pool = mana_pool[player][(uintptr_t)engine_mana_cursor - (uintptr_t)(byte_55CF20 + 1)];
      if (chosen_x_or_mana_adjustment != available_mana_or_pool >> 0x1f)
      {
        mana_kind_changed = true;
      }
      mana_pool[player][(uintptr_t)engine_mana_cursor - (uintptr_t)(byte_55CF20 + 1)] =
          chosen_x_or_mana_adjustment + available_mana_or_pool;
      if (engine_mana_cursor == (byte_55CF20 + 8))
        break;
      chosen_x_or_mana_adjustment = (int)*engine_mana_cursor;
    }
    if (mana_kind_changed)
    {
      available_color_bits = 0;
      payment_result_or_mana_value = 0;
      do
      {
        mana_value_cursor = mana_pool[player] + payment_result_or_mana_value;
        color_index = (unsigned char)payment_result_or_mana_value;
        payment_result_or_mana_value = payment_result_or_mana_value + 1;
        available_color_bits = available_color_bits | (unsigned int)(0 < *mana_value_cursor)
                                                          << (color_index & 0x1f);
      } while (payment_result_or_mana_value != 8);
      payment_result_or_mana_value = (int)(char)available_color_bits;
      record_paid_mana(0, payment_result_or_mana_value);
      pool_clear_cursor = mana_pool + player;
      for (chosen_x_or_mana_adjustment = 8; chosen_x_or_mana_adjustment != 0;
           chosen_x_or_mana_adjustment = chosen_x_or_mana_adjustment + -1)
      {
        (*pool_clear_cursor)[0] = 0;
        pool_clear_cursor = (int (*)[8])((int)pool_clear_cursor + 4u);
      }
    }
    else
    {
      *(int8_t *)(((char *)&mana_pool[0][7]) + player) =
          *(char *)&mana_pool[player][7] + signed_mana_adjustment;
    }
  }
  if (x_value == 0)
  {
    ai_modifier = ai_modifier + -100;
  }
  if (spell_fizzled == 1)
  {
    refund_paid_mana(saved_mana_pool);
    for (int c = 0; c < 7; ++c) {
      mana_pool[player][c] += saved_mana_pool[c];
      mana_pool[player][7] += saved_mana_pool[c];
    }
  }
  return payment_result_or_mana_value;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26aa220; original functions/engine.c:1955. */
int minimum_blockers(int player, int card)
{
  if (check_for_special_ability(player, card, 0x80000))
    return 3;
  return check_for_special_ability(player, card, 0x10000) != 0 ? 2 : 1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26aa280; original functions/engine.c:1965. */
static int is_legal_block_impl2(int blocking_player, int blocking_card, int blocked_player,
                         int blocked_card, keyword_t abils, int landwalks)
{
  card_instance_t *blocking = get_card_instance(blocking_player, blocking_card);
  if (blocking->internal_card_id < 0)
    return 0;
  uint32_t state = blocking->state;
  if (!(cards_data[blocking->internal_card_id].type & TYPE_CREATURE) && !(state & 0x2000000))
    return 0;
  uint32_t illegal_state = 0x800008 | ((card_flags[blocking_player] & CFLAGS_MASAKO) ? 0 : 0x10);
  if (state & illegal_state)
    return 0;
  if (state & 4)
  {
    if (!(get_special_abilities_by_instance(blocking) & 8))
      return 0;
    state = blocking->state;
  }
  if (!(state & 2))
    return 0;
  if ((abils & KEYWORD_FLYING) &&
      !(get_abilities(blocking_player, blocking_card, EVENT_ABILITIES, -1) & 0x420))
    return 0;
  if ((abils & KEYWORD_PROT_COLORED) &&
      (((uint32_t)get_color(blocking_player, blocking_card) << 10) & abils & KEYWORD_PROT_COLORED))
    return 0;
  if (abils & KEYWORD_PROT_CREATURES)
    return 0;
  if (is_what(blocking_player, blocking_card, TYPE_ARTIFACT) && (abils & KEYWORD_PROT_ARTIFACTS))
    return 0;
  if (has_protection_from_impl(blocked_player, blocked_card, blocking_player, blocking_card,
                               IPF_BLOCK, abils))
    return 0;
  uint32_t walks = abils & landwalks;
  if (walks & KEYWORD_BASIC_LANDWALK)
  {
    if ((walks & KEYWORD_SWAMPWALK) && !(player_bits[blocked_player] & 0x400))
      return 0;
    if ((walks & KEYWORD_ISLANDWALK) && !(player_bits[blocked_player] & 0x800))
      return 0;
    if ((walks & KEYWORD_FORESTWALK) && !(player_bits[blocked_player] & 0x1000))
      return 0;
    if ((walks & KEYWORD_MOUNTAINWALK) && !(player_bits[blocked_player] & 0x2000))
      return 0;
    if ((walks & KEYWORD_PLAINSWALK) && !(player_bits[blocked_player] & 0x4000))
      return 0;
  }
  card_instance_t *blocked = get_card_instance(blocked_player, blocked_card);
  uint32_t blocked_sp = is_humiliated(blocked_player, blocked_card, AL_KEY)
                            ? 0
                            : blocked->ext->sp_keywords;
  uint32_t blocking_sp = is_humiliated(blocking_player, blocking_card, AL_KEY)
                             ? 0
                             : blocking->ext->sp_keywords;
  if (blocking_sp & 0x20)
    return 0;
  if (blocked_sp & 0x10)
    return 0;
  if (blocked_sp & 0x100)
  {
    if (!(blocking_sp & 0x8000100))
      return 0;
  }
  else if (blocking_sp & 0x100)
    return 0;
  if ((blocked_sp & 0x40000000) && !(blocking_sp & 0x40000000))
    return 0;
  if ((blocked_sp & 1) && !(get_color(blocking_player, blocking_card) & 2) &&
      !is_what(blocking_player, blocking_card, TYPE_ARTIFACT))
    return 0;
  if ((blocked_sp & 0x2000000) &&
      !has_my_colors(blocking_player, blocking_card, blocked_player, blocked_card) &&
      !is_what(blocking_player, blocking_card, TYPE_ARTIFACT))
    return 0;
  if (blocked_sp & 0x20000)
  {
    int blocking_power = get_power(blocking_player, blocking_card);
    int blocked_power = get_power(blocked_player, blocked_card);
    if (blocked_power < blocking_power)
      return 0;
  }
  if (blocked_sp & 0x200000)
  {
    if (blocked_sp & 0x90000)
      return 0;
    if (dispatch_event_with_attacker(blocking_player, blocking_card, EVENT_BLOCK_LEGALITY,
                                     blocked_player, blocked_card) < 1)
      return -1;
    return 0;
  }
  if (dispatch_event_with_attacker(blocking_player, blocking_card, EVENT_BLOCK_LEGALITY,
                                   blocked_player, blocked_card) < 1)
    return minimum_blockers(blocked_player, blocked_card);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ab5a0; original functions/engine.c:2053. */
int is_legal_block_impl(int blocking_player, int blocking_card, int blocked_player,
                             int blocked_card, keyword_t abils, int ignored_previously_landwalks)
{
  int min_blk = is_legal_block_impl2(blocking_player, blocking_card, blocked_player, blocked_card,
                                     abils, ignored_previously_landwalks);
  if ((uint32_t)min_blk > 1)
  {
    if (min_blk < 2)
    {
      for (int c = 0; c < active_cards_count[blocking_player]; ++c)
      {
        if (c == blocking_card)
          continue;
        card_instance_t *inst = in_play(blocking_player, c);
        if (inst && inst->blocking == (uint32_t)blocked_card)
          return 0;
      }
      return 1;
    }
    int total_blockers = 1;
    for (int c = 0; c < active_cards_count[blocking_player]; ++c)
    {
      if (c == blocking_card)
        continue;
      card_instance_t *inst = in_play(blocking_player, c);
      if (inst && (inst->blocking == (uint32_t)blocked_card ||
                   is_legal_block_impl2(blocking_player, c, blocked_player, blocked_card, abils,
                                        ignored_previously_landwalks)))
      {
        if (++total_blockers >= min_blk)
          return 1;
      }
    }
    return 0;
  }
  return min_blk;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ab6b0; original functions/engine.c:2089. */
int is_legal_block(int blocking_player, int blocking_card, int blocked_player,
                        int blocked_card)
{
  keyword_t abils = get_abilities(blocked_player, blocked_card, EVENT_ABILITIES, -1);
  int player1_landwalks, player0_landwalks;
  get_landwalk_evasion_masks(&player1_landwalks, &player0_landwalks);
  return is_legal_block_impl(blocking_player, blocking_card, blocked_player, blocked_card,
                                  abils,
                                  blocking_player == 1 ? player1_landwalks : player0_landwalks);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26aa700; original functions/engine.c:2237. */
static void enforce_minimum_blockers(void)
{
  int atk = current_turn, def = 1 - atk;
  int num_blocking[151] = {0};
  for (int c = 0; c < active_cards_count[def]; ++c)
  {
    card_instance_t *inst = in_play(def, c);
    if (inst && inst->blocking < 151)
      ++num_blocking[inst->blocking];
  }
  for (unsigned int c = 0; (int)c < active_cards_count[atk]; ++c)
  {
    card_instance_t *inst = in_play(atk, c);
    if (inst && (inst->state & 4) && num_blocking[c] > 0)
    {
      int count = num_blocking[c];
      if (count < minimum_blockers(atk, c) ||
          (check_for_special_ability(atk, c, 0x200000) && count > 1))
      {
        for (int b = 0; b < active_cards_count[def]; ++b)
        {
          inst = in_play(def, b);
          if (inst && inst->blocking == c)
          {
            --count;
            inst->blocking = 255;
            inst->state &= 0xffff7ff7;
            num_blocking[c] = count;
          }
        }
        if (count != 0)
          backtrace_and_abort("functions/engine.c:2266\nnum_blocking[c] == 0");
      }
    }
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ab990; original functions/engine.c:2291. */
void human_assign_blockers(int player)
{
  if (ai_is_speculating == 1)
    return;
  int def = 1 - player, who_chooses = def;
  if (event_flags & 8)
  {
    if (current_turn == 1)
      return;
    who_chooses = player;
  }
  TENTATIVE_reassess_all_cards();
  target_definition_t td_choose_blocker;
  base_target_definition(def, 0, &td_choose_blocker, 0x2000002);
  td_choose_blocker.zone = TARGET_ZONE_IN_PLAY | TARGET_ZONE_0x2000;
  td_choose_blocker.special = TARGET_SPECIAL_ALLOW_MULTIBLOCKER;
  td_choose_blocker.illegal_state = 0;
  td_choose_blocker.allow_cancel = 2;
  td_choose_blocker.who_chooses = who_chooses;
  td_choose_blocker.allowed_controller = def;
  td_choose_blocker.preferred_controller = def;
  target_definition_t td_block_which_attacker;
  base_target_definition(player, 0, &td_block_which_attacker, TYPE_CREATURE);
  td_block_which_attacker.allowed_controller = player;
  td_block_which_attacker.preferred_controller = player;
  td_block_which_attacker.required_state = TARGET_STATE_ATTACKING;
  td_block_which_attacker.who_chooses = who_chooses;
  target_t blocker, blocked;
  for (;;)
  {
    int any = 0;
    for (int i = 0; i < active_cards_count[player] && !any; ++i)
    {
      if (!(get_card_instance(player, i)->state & 4))
        continue;
      for (int j = 0; j < active_cards_count[def]; ++j)
      {
        card_instance_t *inst = get_card_instance(def, j);
        uint32_t mask = 10 | ((card_flags[def] & CFLAGS_MASAKO) ? 0 : 0x10);
        if ((inst->state & mask) == 2 && is_legal_block(def, j, player, i))
        {
          any = 1;
          break;
        }
      }
    }
    if (!any)
      break;
    load_text(NULL, "PROMPT_CHOOSEBLOCKERS");
    if (!select_target(def, -1000, &td_choose_blocker, text_lines[0], &blocker))
      break;
    forbid_attack = 0;
    if (g_battlefield_extra_ability_flags & EA_PAID_BLOCK)
    {
      push_affected_card_stack();
      trigger_cause_controller = blocker.player;
      trigger_cause = blocker.card;
      dispatch_trigger(def, TRIGGER_PAY_TO_BLOCK, gs_prompt_turnsequence_0, 1);
      pop_affected_card_stack();
      if (forbid_attack)
        continue;
    }
    if (select_target(player, -1000, &td_block_which_attacker, text_lines[1], &blocked))
    {
      if (!assign_blocker_to_attacker(blocker.player, blocker.card, blocked.player,
                                                     blocked.card))
      {
        load_text(NULL, "PROMPT_CHOOSEBLOCKERS");
        set_centerwindow_txt(text_lines[2]);
        Sleep(2000);
        set_centerwindow_txt("");
      }
      else
      {
        card_instance_t *inst = get_card_instance(blocked.player, blocked.card);
        int band = inst->blocking == 255 ? blocked.card : inst->blocking;
        inst = get_card_instance(blocker.player, blocker.card);
        inst->blocking = band;
        inst->state |= 8;
        get_card_instance(blocked.player, blocked.card)->state |= 0x4000;
        play_sound_effect(WAV_BLOCK2);
        TENTATIVE_reassess_all_cards();
        if (g_battlefield_extra_ability_flags & EA_SELECT_BLOCK)
          dispatch_trigger2(current_turn, TRIGGER_BLOCKER_CHOSEN, gs_prompt_blockerselection_0, 0,
                            blocker.player, blocker.card);
      }
    }
  }
  enforce_minimum_blockers();
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26adc00; original functions/engine.c:3257. */
int fx_activation_906(int player, int card, event_t event)
{
  if (event != EVENT_RESOLVE_ACTIVATION && event != EVENT_RESOLVE_TRIGGER)
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  g_card_on_stack_controller = instance->parent_controller;
  g_card_on_stack = instance->parent_card;
  int result = call_card_fn(cards_data[instance->original_internal_card_id].code_pointer, instance,
                            player, card, event);
  g_card_on_stack_controller = -1;
  g_card_on_stack = -1;
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26afd30; original functions/engine.c:3508. */
int put_card_on_stack(int player, int card, int second_call)
{
  card_instance_t *instance = get_card_instance(player, card);
  int iid = instance->internal_card_id;
  if (iid < 0)
    return 0;
  int old_iid, old_controller, old_card;
  if (!second_call)
  {
    if (trace_mode & 2)
    {
      char str[500];
      int count = g_duel_trace_counter++;
      sprintf(str, "%d: Player %d is playing %s(%d).\n", count, player, cards_data[iid].name,
              card);
      append_to_trace_txt(str);
    }
    if (!compute_and_check_casting_cost(player, player, card))
      return 0;
    engine_selected_card = -1;
    g_adventure_world_state = 0;
    max_x_value = -1;
    if ((cards_data[iid].type & 0x3c) &&
        !dispatch_event_to_single_card(player, card, EVENT_CAN_CAST, 1 - player, -1))
      return 0;
    spell_fizzled = -1;
    old_iid = g_current_casting_internal_card_id;
    old_controller = card_on_stack_controller;
    old_card = card_on_stack;
    g_current_casting_internal_card_id = iid;
    card_on_stack_controller = player;
    card_on_stack = card;
    if (stack_size < 32)
      put_card_or_activation_onto_stack(player, card, EVENT_RESOLVE_SPELL, player, 0);
    begin_mana_payment_record();
    if (!g_adventure_world_state)
      charge_spell_cost(&cards_data[iid], player, card);
    max_x_value = -1;
    if (!(instance->token_status & 8))
    {
      --hand_count[player];
      if (player != -1)
      {
        uint8_t type = cards_data[iid].type;
        if (type & 2)
          ++creature_count[player];
        if (type & 0x40)
          ++artifact_count[player];
        if (type & 4)
          ++enchantment_count[player];
        types_of_cards_on_bf[player] |= type;
      }
    }
    redraw_libraries();
    instance->state |= 0x30020;
    land_can_be_played |= 0x20;
    g_current_casting_internal_card_id = old_iid;
    card_on_stack_controller = old_controller;
    card_on_stack = old_card;
    if (spell_fizzled != 1)
    {
      if (dispatch_event(player, card, (event_t)0x6c))
        spell_fizzled = 1;
      if (spell_fizzled == 1)
        restore_recorded_mana_to_pool(player);
      end_mana_payment_record();
      if (spell_fizzled == 1)
        goto failed;
      if (player == 1 && !(trace_mode & 2) && ai_is_speculating == 1 &&
          cards_data[iid].cc[1] == 0xff)
        ai_modifier -= 12 * count_colored_cards_in_hand(1);
      if (instance->internal_card_id == -1)
      {
        obliterate_top_card_of_stack();
        land_can_be_played &= ~0x20;
        call_sub_437E20_unless_ai_is_speculating();
      }
      return 1;
    }
    end_mana_payment_record();
  }
  else
  {
    old_iid = g_current_casting_internal_card_id;
    old_controller = card_on_stack_controller;
    old_card = card_on_stack;
    g_current_casting_internal_card_id = iid;
    card_on_stack_controller = player;
    card_on_stack = card;
    if (!(cards_data[iid].type & 1))
    {
      recopy_card_onto_stack(1);
      if (!(cards_data[iid].type & 1))
      {
        if (g_duel_state_007a7d78)
          g_duel_state_007a7d78 |= 2;
        const char *name =
            get_displayed_card_name(card_on_stack_controller, card_on_stack);
        sprintf(global_all_purpose_buffer, gs_trying_to_cast_007a79c0, name);
        allow_response(-2, current_phase, global_all_purpose_buffer,
                                                           0xd3);
        if (g_duel_state_007a7d78 & 2)
        {
          call_sub_437E20_unless_ai_is_speculating();
          Sleep(3000);
        }
      }
    }
    g_current_casting_internal_card_id = old_iid;
    card_on_stack_controller = old_controller;
    card_on_stack = old_card;
    if (instance->internal_card_id == -1)
      spell_fizzled = 1;
    land_can_be_played &= ~0x20;
    instance->state |= player == 0 ? 0x80 : 0x400080;
    if (ai_is_speculating != 1)
      set_stack_damage_targets();
    if (spell_fizzled == 1)
      goto failed;
    if (player == 1 && ai_is_speculating != 1 && !g_duel_state_007a7d78 &&
        !(cards_data[iid].type & 1))
    {
      char str[300];
      load_text(NULL, "PROMPT_CAST1");
      sprintf(str, text_lines[0], opponent_name);
      if (cards_data[iid].cc[1] == 0xff)
        sprintf(str, "%s (X=%d)", opponent_name, x_value);
      target_t target = {-1, -1};
      if (instance->number_of_targets == 1)
        target = instance->targets[0];
      raw_do_dialog(1, card, target.player,
                                                                   target.card, str, 0);
    }
  }
  call_sub_437E20_unless_ai_is_speculating();
  if (spell_fizzled != 1)
  {
    TENTATIVE_set_timestamps(player, card);
    return 1;
  }
failed:;
  int p = player;
  if (instance->token_status & 8)
    kill_card(player, card, KILL_BURY);
  else if (instance->internal_card_id != -1)
    ++hand_count[player];
  uint8_t type = cards_data[iid].type;
  if (type & 2)
    --creature_count[p];
  if (type & 0x40)
    --artifact_count[p];
  if (type & 4)
    --enchantment_count[p];
  redraw_libraries();
  instance->state &= 0xfffcff5d;
  if (player == 1 && !(trace_mode & 2))
    g_ai_speculation_spell_fizzled = 1;
  spell_fizzled = 0;
  obliterate_top_card_of_stack();
  land_can_be_played &= ~0x20;
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ae510; original functions/engine.c:3479. */
int put_card_on_stack2(int player, int card)
{
  int changed = g_current_action_event_code == 0xd3;
  if (changed)
    g_duel_state_00791418 = 1;
  if (!put_card_on_stack(player, card, 0) || !put_card_on_stack(player, card, 1))
  {
    if (changed)
      g_duel_state_00791418 = 0;
    return 0;
  }
  int result = put_card_on_stack3(player, card) != 0;
  if (changed)
    g_duel_state_00791418 = 0;
  return result;
}

#endif

#ifndef SHANDALAR
typedef struct
{
  int card, effectiveness;
  int8_t bands, one_of_best_7;
} PotentialAttacker;

typedef ai_combat_eval_entry_t ai_combat_data_t;

/* ManalinkEh 2022: 0x26a8440; original functions/engine.c:1707. */
static int mod_pow_unblocked(int pow_unblocked, int player, int card)
{
  int id = get_id(player, card), result = pow_unblocked;
  if (id == 0x1744)
    result += count_graveyard(ai_blocker_player);
  else if (id < 0x1745)
  {
    if (id == 0x8fe)
      result = 2;
    else if (id == 0x908)
      result = 4;
    else if (id == 0x173)
      result += 2;
  }
  else if (id == 0x1916)
    result = 9999;
  else if (id == 0x3372)
    result = life[1 - player] - 1;
  if (has_subtype(player, card, SUBTYPE_ROGUE) && check_battlefield_for_id(player, CARD_ID_STINKDRINKER_BANDIT))
    result += 2;
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26aa890; original functions/engine.c:2195. */
static void ai_assign_one_blocker(int player, int blockernum)
{
  int *assignment = ai_candidate_block_assignments;
  int *attack_cards = combat_damage_attacker_cards;
  int *num_blocks = ai_attacker_blocker_counts;
  int *blocker_cards = ai_blocker_cards;
  if (ai_blocker_count != blockernum)
  {
    card_instance_t *inst =
        get_card_instance(ai_combat_defending_player, blocker_cards[blockernum]);
    if (!(inst->state & 0x8000) || inst->blocking == 255)
    {
      assignment[blockernum] = -1;
      ai_assign_one_blocker(player, blockernum + 1);
    }
    uint32_t bit = 1u << (blockernum & 31);
    for (int a = 0; a < combat_damage_attacker_count; ++a)
    {
      int count = num_blocks[a], card = attack_cards[a];
      if (count < g_ai_combat_best_score ||
          count < minimum_blockers(1 - ai_combat_defending_player, card))
      {
        if ((!check_for_special_ability(1 - ai_combat_defending_player, card, 0x200000) ||
             num_blocks[a] < 1) &&
            ((uint32_t)g_ai_attacker_power[a] & bit) &&
            (!(inst->state & 0x8000) || inst->blocking == (uint32_t)attack_cards[a]))
        {
          ++num_blocks[a];
          assignment[blockernum] = attack_cards[a];
          ai_assign_one_blocker(player, blockernum + 1);
          --num_blocks[a];
        }
      }
    }
    return;
  }
  ai_defensive_modifier = 0;
  int life_if_unblocked = life[ai_combat_defending_player];
  int cop_times = defending_cop_available_mana;
  int outflanked[blockernum + 1];
  memset(outflanked, 0, (blockernum + 1) * sizeof(int));
  int cop_benefit[16], blockers_that_will_be_destroyed[16];
  int num_cop_benefit = 0, rating = 0, rampage = 0;
  for (int a = 0; a < combat_damage_attacker_count; ++a)
  {
    int attack_power = combat_damage_attacker_damage[a];
    int attack_toughness = combat_damage_attacker_toughness[a];
    int attacker = attack_cards[a];
    ai_defensive_modifier += g_ai_attacker_toughness[a];
    int attacker_flanking =
        check_for_special_ability(1 - ai_combat_defending_player, attacker, 0x2000);
    int num_destroyed = 0, any_blockers_have_banding = 0, any_destroyed = 0;
    int total_pow = 0, total_tgh = 0, num_blocking = 0;
    uint32_t attacker_bit = 1u << (a & 31);
    for (int b = 0; b < ai_blocker_count; ++b)
    {
      if (assignment[b] != attacker)
        continue;
      ++num_blocking;
      if (num_blocking == 2 &&
          check_for_special_ability(1 - ai_combat_defending_player, attacker, 0x40))
        rampage = dispatch_event_with_attacker(1 - ai_combat_defending_player, attacker,
                                               (event_t)0x5e00, -1, 0x40);
      ai_defensive_modifier += g_ai_blocker_power[b];
      int original_tgh = combat_damage_blocker_toughness[b];
      int tgh = original_tgh, flanked = 0;
      if (attacker_flanking && !check_for_special_ability(ai_combat_defending_player,
                                                               blocker_cards[b], 0x2000))
      {
        if (original_tgh < 2)
        {
          outflanked[b] = 1;
          any_destroyed = 1;
          blockers_that_will_be_destroyed[num_destroyed++] = b;
          continue;
        }
        --tgh;
        flanked = 1;
      }
      outflanked[b] = 0;
      int pow = combat_damage_blocker_damage[b] - flanked;
      total_pow += pow > 0 ? pow : 0;
      total_tgh += tgh > 0 ? tgh : 0;
      if (combat_damage_blocker_abilities[b] & 0x40)
        any_blockers_have_banding = 1;
      if ((uint32_t)g_ai_blocker_ability_counts[b] & attacker_bit)
        total_pow += 99;
      int destroys_this = ((uint32_t)g_ai_attacker_ability_counts[a] >> (b & 31)) & 1;
      if (destroys_this)
        any_destroyed = destroys_this;
      else if (!any_destroyed && attack_power < original_tgh)
      {
        any_destroyed = 0;
        continue;
      }
      if (!(combat_damage_blocker_abilities[b] & 0x200))
        blockers_that_will_be_destroyed[num_destroyed++] = b;
    }
    int32_t combinations = 1u << (num_destroyed & 31);
    if (rampage > 0)
    {
      rampage *= num_blocking - 1;
      attack_power += rampage;
      attack_toughness += rampage;
    }
    uint32_t attack_abils = combat_damage_attacker_abilities[a];
    int new_life = life_if_unblocked, evaluate_damage = 0;
    if (num_blocking < minimum_blockers(1 - ai_combat_defending_player, attacker))
    {
      if (!untapped_royal_assassin_count || untapped_royal_assassin_count <= g_ai_blocker_damage[a])
      {
        if (player_bits[1 - ai_combat_defending_player] & 0x80000)
        {
          int commander_damage =
              check_special_flags2(1 - ai_combat_defending_player, attacker, 0x1000000);
          if (!commander_damage)
          {
            int commander = get_commander_id(1 - ai_combat_defending_player, 1);
            if (commander == get_id(1 - ai_combat_defending_player, attacker))
              commander_damage = 1;
            else
            {
              commander = get_commander_id(1 - ai_combat_defending_player, 2);
              if (commander == get_id(1 - ai_combat_defending_player, attacker))
                commander_damage = 1;
            }
          }
          if (commander_damage)
          {
            int edh = locate_edh_card(1 - ai_combat_defending_player);
            if (count_counters(1 - ai_combat_defending_player, edh, (counter_t)0x1c) +
                    attack_power >=
                21)
              new_life -= 9999;
          }
        }
        new_life -= mod_pow_unblocked(attack_power, 1 - ai_combat_defending_player, attacker);
        evaluate_damage = 1;
      }
    }
    else if ((attack_abils & 0x80) && total_tgh < attack_power)
    {
      new_life -= attack_power - total_tgh;
      evaluate_damage = 1;
    }
    if (evaluate_damage)
    {
      if (defending_cop_color_mask && new_life < life_if_unblocked &&
          (defending_cop_color_mask & get_color(1 - ai_combat_defending_player, attacker)))
        cop_benefit[num_cop_benefit++] = life_if_unblocked - new_life;
      life_if_unblocked = new_life;
    }
    if (num_blocking > 0 &&
        ((attack_toughness <= total_pow && (!(attack_abils & 0x300) || num_destroyed < 1)) ||
         (untapped_royal_assassin_count && g_ai_blocker_damage[a] < untapped_royal_assassin_count)))
    {
      int64_t convert_64 = (int64_t)g_ai_attacker_scores[a] *
                           g_ai_combat_value_weights[3 - ai_combat_defending_player];
      rating += (int)(convert_64 / 8);
    }
    int best_opinion = -1;
    int effective_attack_power = any_destroyed ? 99 : attack_power;
    total_pow -= num_blocking;
    for (int32_t combo = 0; combo < combinations; ++combo)
    {
      int remaining = effective_attack_power, attack_rating = 0;
      if (!any_destroyed && any_blockers_have_banding)
      {
        if (total_pow < effective_attack_power)
        {
          attack_rating = combo ? 9999 : 0;
          for (int k = 0; k < num_destroyed; ++k)
          {
            if (!(combo & (1u << (k & 31))))
              continue;
            int n = blockers_that_will_be_destroyed[k];
            if (combat_damage_blocker_toughness[n] <= effective_attack_power)
            {
              int value = g_ai_combat_candidate_cards[n];
              remaining = is_protected_from(ai_combat_defending_player, blocker_cards[n],
                                                 1 - ai_combat_defending_player, attacker,
                                                 IPF_DAMAGE);
              if (!remaining)
              {
                if (value < attack_rating)
                  attack_rating = value;
                break;
              }
            }
            attack_rating = 0;
            remaining = effective_attack_power;
          }
        }
      }
      else
      {
        for (int l = 0; l < num_destroyed; ++l)
        {
          if (!(combo & (1u << (l & 31))))
            continue;
          int n = blockers_that_will_be_destroyed[l];
          if (!(combat_damage_blocker_abilities[n] & 0x100) ||
              combat_damage_blocker_damage[n] < attack_toughness)
          {
            if (outflanked[n] || !is_protected_from(
                                     ai_combat_defending_player, blocker_cards[n],
                                     1 - ai_combat_defending_player, attacker, IPF_DAMAGE))
              attack_rating += g_ai_combat_candidate_cards[n];
          }
          remaining -= combat_damage_blocker_toughness[n];
          if (remaining < 0)
            break;
        }
      }
      if (remaining >= 0 && best_opinion < attack_rating)
        best_opinion = attack_rating;
    }
    rating -= best_opinion;
  }
  rating += ai_defensive_modifier;
  while (cop_times != 0 && num_cop_benefit > 0)
  {
    int best = 0, best_idx = 0;
    for (int i = 0; i < num_cop_benefit; ++i)
    {
      if (best < cop_benefit[i])
      {
        best = cop_benefit[i];
        best_idx = i;
      }
    }
    if (!best)
      break;
    --cop_times;
    life_if_unblocked += cop_benefit[best_idx];
    cop_benefit[best_idx] = 0;
  }
  int penalty;
  if (life_if_unblocked < 1)
    penalty = life_if_unblocked * -96 + 999;
  else
  {
    int64_t rating_64 = (int64_t)(life[ai_combat_defending_player] - life_if_unblocked) *
                        g_ai_combat_value_weights[ai_combat_defending_player];
    penalty = (int)((rating_64 * 6) / life_if_unblocked);
  }
  if (g_ai_attacker_count < rating - penalty)
  {
    g_ai_attacker_count = rating - penalty;
    memcpy(g_ai_combat_blocking_card_indices, assignment, 16 * sizeof(int));
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26abd60; original functions/engine.c:2408. */
void ai_assign_blockers(int player)
{
  setup_combat_damage_simulation(player);
  g_ai_attacker_count = -9999;
  memset(ai_block_damage_orders, 0, 16 * sizeof(int));
  ai_assign_one_blocker(player, 0);
  if (ai_is_speculating != 1)
  {
    if (current_turn != 0)
    {
      enforce_minimum_blockers();
      return;
    }
    if (event_flags & 8)
    {
      human_assign_blockers(player);
      return;
    }
  }
  for (int i = 0; i < ai_blocker_count; ++i)
  {
    uint32_t assignment = g_ai_combat_blocking_card_indices[i];
    card_instance_t *instance =
        get_card_instance(ai_blocker_player, ai_blocker_cards[i]);
    instance->blocking = assignment;
  }
  enforce_minimum_blockers();
  for (int i = 0; i < ai_blocker_count; ++i)
  {
    uint32_t assignment = g_ai_combat_blocking_card_indices[i];
    card_instance_t *inst = get_card_instance(ai_blocker_player, ai_blocker_cards[i]);
    if (inst->blocking == 255)
      continue;
    uint8_t blk = assignment;
    if (g_ai_combat_blocking_card_indices[i] != -1)
    {
      forbid_attack = 0;
      if (g_battlefield_extra_ability_flags & EA_PAID_BLOCK)
      {
        push_affected_card_stack();
        trigger_cause_controller = ai_blocker_player;
        trigger_cause = ai_blocker_cards[i];
        dispatch_trigger(1 - current_turn, TRIGGER_PAY_TO_BLOCK, gs_prompt_turnsequence_0, 1);
        pop_affected_card_stack();
        if (forbid_attack)
        {
          inst->blocking = 255;
          forbid_attack = 0;
          inst->state &= 0xffff7fff;
          continue;
        }
      }
      if (g_battlefield_extra_ability_flags & EA_SELECT_BLOCK)
        dispatch_trigger2(current_turn, TRIGGER_BLOCKER_CHOSEN, gs_prompt_blockerselection_0, 0,
                          ai_blocker_player, ai_blocker_cards[i]);
      if (blk != 255)
      {
        get_card_instance(1 - ai_blocker_player, assignment & 255)->state |= 0x4000;
        inst->state |= 8;
      }
    }
    int redisplay = ai_is_speculating != 1;
    inst->blocking = blk;
    if (redisplay)
    {
      request_duel_display_refresh();
      if (g_ai_combat_blocking_card_indices[i] != -1)
        play_sound_effect(WAV_BLOCK2);
    }
  }
  enforce_minimum_blockers();
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ac0a0; original functions/engine.c:2586. */
void setup_ai_combat_data(int player)
{
  ai_combat_data_t(*data)[151] = ai_combat_eval_table;
  int old_ai_is_speculating = ai_is_speculating;
  int old_speculating_during_combat = g_duel_state_0093f4b0;
  ai_is_speculating = 1;
  g_duel_state_0093f4b0 = 1;
  untapped_royal_assassin_count = 0;
  memset(data, 0, sizeof(ai_combat_eval_table));
  int final_check_abilities_cop_colors = 0;
  int p = player;
  for (int pass = 0; pass < 2; ++pass)
  {
    defending_cop_color_mask = 0;
    for (int c = 0; c < active_cards_count[p]; ++c)
    {
      card_instance_t *inst = in_play(p, c);
      if (!inst)
        continue;
      uint32_t old_state = inst->state;
      if (p == 1 - player)
        inst->state = old_state | 8;
      else if (p == 1)
      {
        if (check_for_special_ability(1, c, 8))
          inst->state |= 4;
        else
          inst->state |= 0x14;
      }
      inst->regen_status |= 0xe000000;
      check_abilities_power = get_abilities(p, c, EVENT_POWER, -1);
      check_abilities_toughness = get_abilities(p, c, EVENT_TOUGHNESS, -1);
      check_abilities_keywords = get_abilities(p, c, EVENT_ABILITIES, -1);
      int old_activating_player = g_card_on_stack_controller;
      g_card_on_stack_controller = p;
      dispatch_event(p, c, EVENT_CHECK_ABILITIES);
      g_card_on_stack_controller = old_activating_player;
      data[p][c].potential_power += check_abilities_power;
      data[p][c].potential_toughness += check_abilities_toughness;
      data[p][c].abils |= check_abilities_keywords;
      int indestructible = check_for_special_ability(p, c, 0x800);
      uint32_t abils = data[p][c].abils;
      if (indestructible)
      {
        abils |= 0x200;
        data[p][c].abils = abils;
      }
      if (abils & 0x40000000)
      {
        data[p][c].abils = abils | 0x100;
        if (data[p][c].potential_power > 0)
          data[p][c].potential_power *= 2;
      }
      inst->state = old_state;
    }
    if (p == 1 - player)
      final_check_abilities_cop_colors = defending_cop_color_mask;
    p = 1 - p;
  }
  defending_cop_color_mask = final_check_abilities_cop_colors;
  defending_cop_available_mana = final_check_abilities_cop_colors ? has_mana(player, 7, 1) : 0;
  recalculate_all_cards_in_play();
  ai_is_speculating = old_ai_is_speculating;
  g_duel_state_0093f4b0 = old_speculating_during_combat;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ac340; original functions/engine.c:2674. */
int ai_choose_attackers(int player)
{
  typedef struct
  {
    int card, pow;
  } CardPower;
  ai_combat_data_t(*data)[151] = ai_combat_eval_table;
  setup_ai_combat_data(player);
  int store_ai_is_speculating = ai_is_speculating;
  if (store_ai_is_speculating == 1)
    g_ai_search_flags ^= 2;
  for (int c = 0; c < active_cards_count[player]; ++c)
    get_card_instance(player, c)->state &= ~4;
  int banding_attack_indices[16], attack_indices[16] = {0}, attack_value_assessment[16] = {0};
  int num_banding_attack_indices = 0, num_attack_indices = 0, total_potential_power_banding = 0;
  uint32_t bitfield_of_unk8000 = 0, ai_combat_data_abils_not_present = UINT32_MAX;
  int combos = 1, has_banding_attackers = 0;
  {
    PotentialAttacker pot_atck[150];
    int num_pot_atck = 0, num_pot_atck_nonbanding = 0;
    for (int c = 0; c < active_cards_count[player]; ++c)
    {
      card_instance_t *inst = in_play(player, c);
      if (!inst)
        continue;
      if (!is_what(player, c, TYPE_CREATURE) && !(inst->state & 0x1000000))
        continue;
      if ((inst->state & 0x20010) || !can_attack(player, c))
        continue;
      pot_atck[num_pot_atck].card = c;
      pot_atck[num_pot_atck].effectiveness = score_ai_attacker_candidate(player, c);
      int bands =
          (g_ai_search_flags & 2) && (inst->state & 0x8000) && (data[player][c].abils & 0x40);
      pot_atck[num_pot_atck].bands = bands;
      pot_atck[num_pot_atck].one_of_best_7 = 0;
      if (!bands)
        ++num_pot_atck_nonbanding;
      ++num_pot_atck;
    }
    if (num_pot_atck_nonbanding >= 8)
    {
      qsort(pot_atck, num_pot_atck, sizeof(PotentialAttacker),
            qsort_compare_attacker_effectiveness);
      int num_fast_attackers = 0;
      for (int i = 0; i < num_pot_atck && num_fast_attackers < 16; ++i)
      {
        if (!pot_atck[i].bands)
          attack_indices[++num_fast_attackers] = pot_atck[i].card;
      }
      int attacker_power[16] = {0};
      for (int i = 0; i < num_fast_attackers; ++i)
        attacker_power[i] = data[player][attack_indices[i]].potential_power;
      int num_potential_blockers = 0;
      for (int c = 0; c < active_cards_count[ai_combat_defending_player]; ++c)
      {
        card_instance_t *inst = in_play(ai_combat_defending_player, c);
        if (inst && !(inst->state & 0x10) &&
            is_what(ai_combat_defending_player, c, TYPE_CREATURE))
          ++num_potential_blockers;
      }
      for (int i = 0; i < num_potential_blockers; ++i)
      {
        int highest_potential_power = 0, highest_potential_power_idx = 0;
        for (int j = 0; j < num_fast_attackers; ++j)
        {
          if (highest_potential_power < attacker_power[j])
          {
            highest_potential_power = attacker_power[j];
            highest_potential_power_idx = j;
          }
        }
        attacker_power[highest_potential_power_idx] = 0;
      }
      int total_potential_unblocked_power = 0;
      for (int i = 0; i < num_fast_attackers; ++i)
        total_potential_unblocked_power += attacker_power[i];
      if (life[1 - player] <= total_potential_unblocked_power)
      {
        for (int i = 0; i < num_fast_attackers; ++i)
          get_card_instance(player, attack_indices[i])->state |= 4;
        int valid = 1;
        if (event_flags & 0x80)
        {
          event_result = 0;
          dispatch_event_raw(EVENT_ATTACK_LEGALITY_ALL);
          valid = event_result == 0;
        }
        if (valid)
        {
          must_attack = (1u << (num_fast_attackers & 31)) - 1;
          return must_attack;
        }
        for (int i = 0; i < num_fast_attackers; ++i)
          get_card_instance(player, attack_indices[i])->state &= ~4;
      }
    }
    int best_7_marked = 0;
    for (int i = 0; i < num_pot_atck && best_7_marked < 7; ++i)
    {
      if (!pot_atck[i].bands)
      {
        pot_atck[i].one_of_best_7 = 1;
        ++best_7_marked;
      }
    }
    if (num_pot_atck_nonbanding > 7)
      qsort(pot_atck, num_pot_atck, sizeof(PotentialAttacker),
            qsort_cardvalue_ascending);
    uint32_t banding_regen = 0;
    for (int i = 0; i < num_pot_atck; ++i)
    {
      int c = pot_atck[i].card;
      if (pot_atck[i].bands)
      {
        if (num_banding_attack_indices < 16)
        {
          keyword_t abils = data[player][c].abils;
          total_potential_power_banding += data[player][c].potential_power;
          ai_combat_data_abils_not_present &= abils;
          banding_regen |= abils & KEYWORD_REGENERATION;
          banding_attack_indices[num_banding_attack_indices++] = c;
        }
      }
      else if (pot_atck[i].one_of_best_7)
      {
        if (num_attack_indices > 6)
          backtrace_and_abort("functions/engine.c:2829\nnum_attack_indices < 7");
        card_instance_t *inst = get_card_instance(player, c);
        attack_indices[num_attack_indices] = c;
        attack_value_assessment[num_attack_indices] = pot_atck[i].effectiveness;
        uint32_t state = inst->state;
        inst->state = state | 4;
        if (state & 0x8000)
          bitfield_of_unk8000 |= 1u << (num_attack_indices & 31);
        ++num_attack_indices;
      }
    }
    has_banding_attackers = num_banding_attack_indices != 0;
    ai_combat_data_abils_not_present |= banding_regen;
    combos = 1u << (num_attack_indices & 31);
  }
  setup_combat_damage_simulation(player);
  int local_ai_combat_attack_power[16], local_ai_combat_attack_toughness[16];
  keyword_t local_ai_combat_attack_abils[16];
  int local_ai_combat_attack_rating[16], local_ai_combat_blocking_choices_bitmask[16];
  int local_ai_combat_attack_defensive_modifier[16], local_ai_combat_block_defensive_modifier[16];
  memcpy(local_ai_combat_attack_power, combat_damage_attacker_damage, 64);
  memcpy(local_ai_combat_attack_toughness, combat_damage_attacker_toughness, 64);
  memcpy(local_ai_combat_attack_abils, combat_damage_attacker_abilities, 64);
  memcpy(local_ai_combat_attack_rating, g_ai_attacker_scores, 64);
  memcpy(local_ai_combat_blocking_choices_bitmask, g_ai_attacker_power, 64);
  memcpy(local_ai_combat_attack_defensive_modifier, g_ai_attacker_toughness, 64);
  memcpy(local_ai_combat_block_defensive_modifier, g_ai_blocker_power, 64);
  save_combat_simulation_state();
  ai_is_speculating = 1;
  dispatch_event_raw(EVENT_SHOULD_AI_PLAY);
  ai_is_speculating = store_ai_is_speculating;
  int blocker_power[16], blocker_toughness[16];
  uint8_t card_to_blocker_pt_idx[150], card_to_attacker_pt_idx[150];
  int highest_blocker_toughness = 0, num_blocker_pt = 0;
  for (int c = 0; c < active_cards_count[ai_combat_defending_player]; ++c)
  {
    card_instance_t *inst = in_play(ai_combat_defending_player, c);
    if (!inst || !is_what(ai_combat_defending_player, c, TYPE_CREATURE))
      continue;
    int p = ai_combat_defending_player;
    int pow = data[ai_combat_defending_player][c].potential_power;
    int tgh = data[ai_combat_defending_player][c].potential_toughness;
    blocker_power[num_blocker_pt] = pow;
    blocker_toughness[num_blocker_pt] = tgh;
    uint32_t ea = cards_data[inst->internal_card_id].extra_ability;
    CardFunction fn = cards_data[inst->internal_card_id].code_pointer;
    inst = get_card_instance(ai_combat_defending_player, c);
    if (ea & 0x18)
    {
      int8_t old_activating_player = g_card_on_stack_controller;
      g_card_on_stack_controller = p;
      int swapped = check_special_flags3(p, c, 0x100000);
      if (!swapped)
      {
        if (ea & 8)
          blocker_power[num_blocker_pt] = pow + call_card_fn(fn, inst, p, c, EVENT_POW_BOOST);
        if (ea & 0x10)
        {
          tgh += call_card_fn(fn, inst, p, c, EVENT_TOU_BOOST);
          blocker_toughness[num_blocker_pt] = tgh;
        }
      }
      else
      {
        if (ea & 8)
        {
          tgh += call_card_fn(fn, inst, p, c, EVENT_POW_BOOST);
          blocker_toughness[num_blocker_pt] = tgh;
        }
        if (ea & 0x10)
          blocker_power[num_blocker_pt] = pow + call_card_fn(fn, inst, p, c, EVENT_TOU_BOOST);
      }
      g_card_on_stack_controller = old_activating_player;
    }
    if (highest_blocker_toughness < tgh)
      highest_blocker_toughness = tgh;
    card_to_blocker_pt_idx[c] = num_blocker_pt;
    if (++num_blocker_pt > 15)
      break;
  }
  int band = -1;
  for (int i = 0; i < num_attack_indices; ++i)
  {
    if (local_ai_combat_attack_power[i] < highest_blocker_toughness &&
        highest_blocker_toughness <=
            local_ai_combat_attack_power[i] + total_potential_power_banding &&
        local_ai_combat_attack_abils[i] ==
            (local_ai_combat_attack_abils[i] & ai_combat_data_abils_not_present))
    {
      local_ai_combat_attack_power[i] += total_potential_power_banding;
      band = attack_indices[i];
      break;
    }
  }
  int attacker_power[16] = {0}, attacker_toughness[16] = {0}, num_attacker_pt = 0;
  for (int c = 0; c < active_cards_count[player]; ++c)
  {
    card_instance_t *inst = in_play(player, c);
    if (!inst || !is_what(player, c, TYPE_CREATURE))
      continue;
    int pow = data[player][c].potential_power, tgh = data[player][c].potential_toughness;
    attacker_power[num_attacker_pt] = pow;
    attacker_toughness[num_attacker_pt] = tgh;
    attack_value_assessment[num_attacker_pt] = (pow + 1) * (tgh + 1);
    for (int i = 0; i < combat_damage_attacker_count; ++i)
      if (combat_damage_attacker_cards[i] == c)
        attack_value_assessment[num_attacker_pt] = g_ai_attacker_scores[i];
    card_to_attacker_pt_idx[c] = num_attacker_pt;
    if (++num_attacker_pt > 15)
      break;
  }
  restore_combat_simulation_state();
  int chosen_combo = 0, combo_rating = 9999;
  for (int combo = 0; combo < combos; ++combo)
  {
    combat_damage_attacker_count = 0;
    g_ai_destroy_if_blocked_attacker_mask = 0;
    int8_t combo_valid = 1;
    for (int i = 0; i < num_attack_indices; ++i)
    {
      int c = attack_indices[i];
      card_instance_t *inst = get_card_instance(player, c);
      int csvid = get_id(player, c);
      if (((uint32_t)combo >> (i & 31)) & 1)
      {
        inst->state |= 4;
        int n = combat_damage_attacker_count;
        combat_damage_attacker_cards[n] = c;
        combat_damage_attacker_damage[n] = local_ai_combat_attack_power[i];
        combat_damage_attacker_toughness[n] = local_ai_combat_attack_toughness[i];
        combat_damage_attacker_abilities[n] = local_ai_combat_attack_abils[i];
        g_ai_attacker_scores[n] = local_ai_combat_attack_rating[i];
        g_ai_attacker_power[n] = local_ai_combat_blocking_choices_bitmask[i];
        g_ai_attacker_toughness[n] = local_ai_combat_attack_defensive_modifier[i];
        g_ai_blocker_power[n] = local_ai_combat_block_defensive_modifier[i];
        if (csvid == CARD_ID_COCKATRICE || csvid == CARD_ID_LURE)
          g_ai_destroy_if_blocked_attacker_mask |= 1u << (n & 31);
        ++combat_damage_attacker_count;
      }
      else
      {
        inst->state &= ~4;
        if (csvid == CARD_ID_ERG_RAIDERS || csvid == CARD_ID_JUGGERNAUT)
          combo_valid = 0;
        else
          combo_valid &= (bitfield_of_unk8000 & (1u << (i & 31))) == 0;
      }
    }
    if (combat_damage_attacker_count > 16)
      backtrace_and_abort("functions/engine.c:2971\nnum_attackers <= 16");
    if (event_flags & 0x80)
    {
      event_result = 0;
      dispatch_event_raw(EVENT_ATTACK_LEGALITY_ALL);
      if (event_result)
        continue;
    }
    if (!combo_valid)
      continue;
    if (untapped_royal_assassin_count && combat_damage_attacker_count > 0)
    {
      int assassin_tgt[16];
      for (int i = 0; i < combat_damage_attacker_count; ++i)
        assassin_tgt[i] =
            (get_special_abilities(player, combat_damage_attacker_cards[i]) & 8) ? 9999 : i;
      if (combat_damage_attacker_count > 0)
      {
        for (int j = 1; j < combat_damage_attacker_count; ++j)
        {
          int lhs = assassin_tgt[0], rhs = assassin_tgt[j];
          if (lhs != 9999 && rhs != 9999 &&
              combat_damage_attacker_damage[lhs] < combat_damage_attacker_damage[rhs])
          {
            assassin_tgt[j] = lhs;
            assassin_tgt[0] = rhs;
          }
        }
        for (int i = 0; i < combat_damage_attacker_count; ++i)
        {
          if (assassin_tgt[i] == 9999)
            g_ai_blocker_damage[i] = 9999;
          else
            g_ai_blocker_damage[assassin_tgt[i]] = i;
        }
      }
    }
    g_ai_current_attacker = 1;
    ai_assign_blockers(player);
    save_combat_simulation_state();
    ai_is_speculating = 1;
    dispatch_event_raw(EVENT_SHOULD_AI_PLAY);
    ai_is_speculating = store_ai_is_speculating;
    CardPower cardpow[8];
    for (int i = 0; i < 8; ++i)
      cardpow[i].pow = -1;
    for (int c = 0; c < active_cards_count[ai_combat_defending_player]; ++c)
    {
      card_instance_t *inst = in_play(ai_combat_defending_player, c);
      if (!inst || !is_what(ai_combat_defending_player, c, TYPE_CREATURE) ||
          !(inst->state & 0x400))
        continue;
      int pow_unblocked = mod_pow_unblocked(blocker_power[card_to_blocker_pt_idx[c]],
                                            ai_combat_defending_player, c);
      if (pow_unblocked < 0)
        pow_unblocked = 0;
      int pos = 0;
      while (pos < 8 && pow_unblocked <= cardpow[pos].pow)
        ++pos;
      if (pos == 8)
        continue;
      for (int j = 7; j > pos; --j)
        cardpow[j] = cardpow[j - 1];
      cardpow[pos].card = c;
      cardpow[pos].pow = pow_unblocked;
    }
    int total_unblocked_opp_power = 0;
    for (int i = 0; i < 8 && cardpow[i].pow != -1; ++i)
    {
      int oppcard = cardpow[i].card;
      keyword_t abils =
          get_abilities(ai_combat_defending_player, oppcard, EVENT_ABILITIES, -1);
      int opp_tgh = blocker_toughness[card_to_blocker_pt_idx[oppcard]];
      int opp_pow = mod_pow_unblocked(blocker_power[card_to_blocker_pt_idx[oppcard]],
                                      ai_combat_defending_player, oppcard);
      int my_chump = 0, have_chump = 0, free_block = 0;
      int64_t chump_assessment = 32767;
      for (int c = 0; c < active_cards_count[player]; ++c)
      {
        card_instance_t *inst = in_play(player, c);
        if (!inst || !(inst->state & 0x400) || !is_what(player, c, TYPE_CREATURE))
          continue;
        uint32_t state = inst->state;
        int vigilant = check_for_special_ability(player, c, 8);
        if (state & (8 | (vigilant ? 0 : 4)))
          continue;
        if (!is_legal_block_impl(player, c, ai_combat_defending_player, oppcard, abils,
                                      0))
          continue;
        int idx = card_to_attacker_pt_idx[c];
        if (opp_pow < attacker_toughness[idx] || opp_tgh <= attacker_power[idx])
        {
          inst->state |= 8;
          free_block = 1;
          break;
        }
        have_chump = 1;
        int64_t value = attack_value_assessment[idx];
        if (value < chump_assessment)
        {
          my_chump = c;
          chump_assessment = value;
        }
      }
      if (free_block)
        continue;
      int denominator = life[player] - opp_pow;
      if (denominator < 1)
        denominator = 1;
      else if (denominator > 99)
        denominator = 99;
      int cost = (int32_t)((uint32_t)g_ai_combat_value_weights[player] * (uint32_t)opp_pow * 6u);
      cost /= denominator;
      if (have_chump)
      {
        int chump = (int)(((int64_t)g_ai_combat_value_weights[player + 2] * chump_assessment) / 8);
        if (cost >= chump)
        {
          g_ai_attacker_count += chump;
          get_card_instance(player, my_chump)->state |= 8;
          continue;
        }
      }
      g_ai_attacker_count += cost;
      total_unblocked_opp_power += opp_pow;
    }
    restore_combat_simulation_state();
    if (life[player] > 0 && life[player] <= total_unblocked_opp_power && life[1 - player] > 0)
      g_ai_attacker_count += (total_unblocked_opp_power - life[player] + 2) * 128;
    if (g_ai_attacker_count < combo_rating)
    {
      chosen_combo = combo;
      combo_rating = g_ai_attacker_count;
    }
  }
  if (chosen_combo && (event_flags & 0x2000000))
    dispatch_event_raw(EVENT_BEFORE_COMBAT);
  for (int i = 0; i < num_attack_indices; ++i)
  {
    int c = attack_indices[i];
    card_instance_t *inst = get_card_instance(player, c);
    if (!(((uint32_t)chosen_combo >> (i & 31)) & 1))
      inst->state &= ~4;
    else
    {
      inst->state |= 4;
      if (band == -1 && has_banding_attackers &&
          local_ai_combat_attack_abils[i] ==
              (ai_combat_data_abils_not_present & local_ai_combat_attack_abils[i]))
        band = c;
    }
  }
  if (band == -1 || !has_banding_attackers || !(get_card_instance(player, band)->state & 4))
  {
    if (!highest_blocker_toughness && has_banding_attackers)
    {
      for (int i = 0; i < num_banding_attack_indices; ++i)
        get_card_instance(player, banding_attack_indices[i])->state |= 4;
      chosen_combo = 1;
    }
  }
  else
  {
    for (int i = 0; i < num_banding_attack_indices; ++i)
    {
      card_instance_t *inst = get_card_instance(player, banding_attack_indices[i]);
      inst->blocking = band;
      inst->state |= 4;
    }
    get_card_instance(player, band)->blocking = band;
  }
  if (event_flags & 0x80)
  {
    do
    {
      event_result = 0;
      dispatch_event_raw(EVENT_ATTACK_LEGALITY_ALL);
      if (!event_result)
      {
        must_attack = chosen_combo;
        return chosen_combo;
      }
      undeclare_attacker(player, event_result & 0xffff);
    } while (event_result != 0);
  }
  must_attack = chosen_combo;
  return chosen_combo;
}

#endif

#ifndef SHANDALAR
#define payment_amt_to_apply_to_cost get_mana_payment_amount
#define payment_apply_mana_in_pool_to_colorless_cost auto_pay_generic_mana
#define payment_apply_mana_in_pool_to_cost auto_pay_colored_mana
#define payment_apply_mana_in_pool_to_x_cost auto_pay_variable_mana
#define payment_apply_mana_to_cost spend_mana_for_cost
#define payment_calculate_needed_mana_colors get_required_mana_color_mask
#define payment_copy_mana_pool_to_display copy_mana_pool_to_display
#define payment_TENTATIVE_reassess_all_cards TENTATIVE_reassess_all_cards
#define payment_resolve_top_card_on_stack resolve_top_card_on_stack
#define payment_single_color_test_bit_to_color_t single_color_test_bit_to_color_t
#define payment_TENTATIVE_update_mana_spent refund_paid_mana
#define payment_TENTATIVE_color_clicked_in_mana_summary_pane g_recorded_action_phase
#define payment_global_all_purpose_buffer global_all_purpose_buffer
#define payment_WILDGUESS_phase_to_run_to g_stop_phase
#define payment_raw_mana_may_be_spent_as_color g_mana_color_conversions
#define payment_WILDGUESS_whose_phase_to_run_to g_stop_phase_player

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ae610; original functions/engine.c:865. */
int charge_mana(int player, color_t color, int amount)
{
  int old_affected_card_controller = affected_card_controller;
  int old_affected_card = affected_card;

  int life_to_pay_for_krrik = 0;

  if (color == COLOR_ANY)
    color = COLOR_COLORLESS;

  unknown62BCEC = 0;
  unknown_charge_mana_004d7cb0 = 200;

  int amtspaid[COLOR_ARTIFACT + 1] = {0};
  int total_paid = 0;

  if (!(land_can_be_played & LCBP_CARD_BEING_COPIED))
  {
    int old_land_can_be_played = land_can_be_played;
    land_can_be_played |= LCBP_CHARGING_MANA;

    pay_mana_xbugrwaU[color] += amount;

    int amt_of_nonartifact_mana =
        has_mana(player, COLOR_ARTIFACT, 1) - has_mana(player, COLOR_ANY, 1);
    int autotap_matching_colors_in_pool = 1;
    int autotap_colored_in_pool_for_colorless, autotap_pool_for_x, autotap_sources_human,
        autotap_sources_ai;
    if (!ldoubleclicked && !IS_AI(player))
    {
      autotap_colored_in_pool_for_colorless = autotap_pool_for_x = autotap_sources_human = 0;
      autotap_sources_ai = 0;
    }
    else
    {

      autotap_colored_in_pool_for_colorless = autotap_pool_for_x = autotap_sources_human = 1;
      if (!IS_AI(player))
        autotap_sources_ai = 0;
      else
        autotap_sources_ai = 1;
    }

    if (player == AI && !(trace_mode & 2))
    {
      int any_x_cost = 0;
      int x = 0;

      color_t c;
      for (c = COLOR_COLORLESS; c <= COLOR_ARTIFACT; ++c)
        if (pay_mana_xbugrwaU[c] == -1)
        {
          any_x_cost = 1;
          if (ai_is_speculating == 1)
          {
            int avail = has_mana(player, c, 1);
            if (c == COLOR_COLORLESS)
            {
              color_t d;
              for (d = COLOR_COLORLESS; d <= COLOR_ARTIFACT; ++d)
                if (pay_mana_xbugrwaU[d] > 0)
                  avail -= pay_mana_xbugrwaU[d];
            }

            if (avail > 0)
              x += avail;
          }
        }

      if (any_x_cost)
      {
        if (ai_is_speculating == 1)
        {
          if (x > 1 && internal_rand(3))
            x = internal_rand(x - 1) + 1;

          if (x > max_x_value && max_x_value != -1)
            x = max_x_value;

          g_ai_recorded_choice = x;
          record_ai_action_selection();
        }
        else
        {
          replay_ai_action_selection();

          if (g_ai_recorded_choice == 99)
            g_ai_recorded_choice = 0;

          x = g_ai_recorded_choice;
        }
      }

      max_x_value = x;
    }

    x_value = 0;

    if (max_x_value == 0)
    {
      color_t c;
      for (c = COLOR_COLORLESS; c <= COLOR_ARTIFACT; ++c)
        if (pay_mana_xbugrwaU[c] == -1)
          pay_mana_xbugrwaU[c] = 0;
    }

    int pay_mana_has_x = 0;
    color_t c;
    for (c = COLOR_COLORLESS; c <= COLOR_ARTIFACT; ++c)
      if (pay_mana_xbugrwaU[c] == -1)
        pay_mana_has_x = 1;

    if ((card_flags[player] & CFLAGS_KRRIK) != 0 && pay_mana_xbugrwaU[COLOR_BLACK] != 0 &&
        charge_mana_called_from_pay_phyrexian_mana == 0)
    {
      int can_pay = can_pay_life(player, 2);
      int max_life = how_much_life_can_i_pay(player);
      if (can_pay)
      {
        int maximum = max_life / 2;
        if (pay_mana_xbugrwaU[COLOR_BLACK] < maximum)
          maximum = pay_mana_xbugrwaU[COLOR_BLACK];
        int minimum = pay_mana_xbugrwaU[COLOR_BLACK] - has_mana(player, COLOR_BLACK, 1);
        if (minimum < 0)
          minimum = 0;
        char buffer[150];
        sprintf(buffer, " Pay 2 life for how much black mana? (max %d, min %d)", maximum, minimum);
        int chosen;
        while (1)
        {
          chosen = choose_a_number(player, buffer, maximum);
          if (chosen == -1)
          {
            spell_fizzled = 1;
            return 0;
          }

          if (chosen <= maximum)
            break;
          if (player == 1)
          {
            spell_fizzled = 1;
            ai_modifier -= 100;
            return 0;
          }
        }
        if ((max_life - 6) / 2 <= chosen && player == 1)
        {
          ai_modifier -= 999;
          spell_fizzled = 1;
          return 0;
        }
        life_to_pay_for_krrik = chosen * 2;
        pay_mana_xbugrwaU[COLOR_BLACK] -= chosen;
      }
    }

    if (autotap_matching_colors_in_pool &&
        !pay_mana_maximally_satisfied(pay_mana_xbugrwaU, x_value, max_x_value))
    {
      payment_apply_mana_in_pool_to_cost(player, amtspaid, &total_paid, amt_of_nonartifact_mana);
      payment_copy_mana_pool_to_display();
    }

    if (autotap_colored_in_pool_for_colorless &&
        !pay_mana_maximally_satisfied(pay_mana_xbugrwaU, x_value, max_x_value))
    {
      payment_apply_mana_in_pool_to_colorless_cost(player, amtspaid, &total_paid,
                                                   amt_of_nonartifact_mana, &x_value, max_x_value);
      payment_copy_mana_pool_to_display();
    }

    if (autotap_pool_for_x && pay_mana_has_x &&
        !pay_mana_maximally_satisfied(pay_mana_xbugrwaU, x_value, max_x_value))
    {
      payment_apply_mana_in_pool_to_x_cost(player, amtspaid, &total_paid, amt_of_nonartifact_mana,
                                           &x_value, max_x_value);
      payment_copy_mana_pool_to_display();
    }

    if (autotap_sources_human)
      human_autotap_for_mana(player, amtspaid, &total_paid, 1);

    if (autotap_sources_ai &&
        !pay_mana_maximally_satisfied(pay_mana_xbugrwaU, x_value, max_x_value))
    {
      try_to_pay_for_mana_by_autotapping(player, amtspaid, &total_paid,
                                         AUTOTAP_NO_CREATURES | AUTOTAP_NO_DONT_AUTO_TAP, 1);

      if (!pay_mana_maximally_satisfied(pay_mana_xbugrwaU, x_value, max_x_value))
        try_to_pay_for_mana_by_autotapping(player, amtspaid, &total_paid, AUTOTAP_NO_DONT_AUTO_TAP,
                                           1);

      if (!pay_mana_maximally_satisfied(pay_mana_xbugrwaU, x_value, max_x_value))
        try_to_pay_for_mana_by_autotapping(
            player, amtspaid, &total_paid,
            AUTOTAP_NO_CREATURES | AUTOTAP_NO_ARTIFACTS | AUTOTAP_NO_NONBASIC_LANDS, 1);

      if (!pay_mana_maximally_satisfied(pay_mana_xbugrwaU, x_value, max_x_value))
        try_to_pay_for_mana_by_autotapping(player, amtspaid, &total_paid,
                                           AUTOTAP_NO_CREATURES | AUTOTAP_NO_ARTIFACTS, 1);

      if (!pay_mana_maximally_satisfied(pay_mana_xbugrwaU, x_value, max_x_value))
        try_to_pay_for_mana_by_autotapping(player, amtspaid, &total_paid, AUTOTAP_NO_CREATURES, 1);

      if (!pay_mana_maximally_satisfied(pay_mana_xbugrwaU, x_value, max_x_value))
        try_to_pay_for_mana_by_autotapping(player, amtspaid, &total_paid, 0, 1);
    }

    if (IS_AI(player))
    {
      if (!pay_mana_maximally_satisfied(pay_mana_xbugrwaU, x_value, max_x_value) && !pay_mana_has_x)
        spell_fizzled = 1;
    }
    else
      while (!pay_mana_maximally_satisfied(pay_mana_xbugrwaU, x_value, max_x_value))
      {
        int any_nonx_mana = 0;
        for (c = COLOR_COLORLESS; c <= COLOR_ARTIFACT; ++c)
          if (pay_mana_xbugrwaU[c] > 0)
          {
            any_nonx_mana = 1;
            break;
          }

        format_manacost_into_global_allpurpose_buffer(0, pay_mana_xbugrwaU, x_value, max_x_value);
        int card = select_card_for_action(
            player, player, player, 0, 0, payment_global_all_purpose_buffer, any_nonx_mana ? 1 : 3);
        if (g_target_player_choice != -1 || (card != -1 && card != -2))
        {
          if (g_target_player_choice == -1 || card != -1)
          {
            card_instance_t *inst = get_card_instance(player, card);
            int iid = inst->internal_card_id;
            if (iid >= 0 && (cards_data[iid].extra_ability & EA_MANA_SOURCE) &&
                ((cards_data[iid].type & TYPE_INTERRUPT) ||
                 (inst->state & (STATE_IN_PLAY | STATE_OUBLIETTED)) == STATE_IN_PLAY))
            {
              int old_mana_pool[8], old_pay_mana[8], old_x_value, old_max_x_value, i;
              for (i = 0; i < 8; ++i)
                old_mana_pool[i] = mana_pool[player][i];

              int (*old_charge_mana_addr_of_pre_mana_pool)[8] = charge_mana_addr_of_pre_mana_pool;
              charge_mana_addr_of_pre_mana_pool = &old_mana_pool;
              int *old_charge_mana_addr_of_x_value = charge_mana_addr_of_x_value;
              charge_mana_addr_of_x_value = &old_x_value;
              int (*old_charge_mana_addr_of_pay_mana_xbugrw)[8] =
                  charge_mana_addr_of_pay_mana_xbugrw;
              charge_mana_addr_of_pay_mana_xbugrw = &old_pay_mana;
              int *old_charge_mana_addr_of_max_x_value = charge_mana_addr_of_max_x_value;
              charge_mana_addr_of_max_x_value = &old_max_x_value;

              old_x_value = x_value;
              x_value = 0;
              old_max_x_value = max_x_value;
              max_x_value = -1;
              for (i = 0; i < 8; ++i)
              {
                old_pay_mana[i] = pay_mana_xbugrwaU[i];
                pay_mana_xbugrwaU[i] = 0;
              }

              if ((inst->state & (STATE_OUBLIETTED | STATE_IN_PLAY)) != STATE_IN_PLAY)
              {
                put_card_on_stack2(player, card);
                payment_TENTATIVE_reassess_all_cards();
              }
              else if (dispatch_event_to_single_card(player, card, EVENT_CAN_ACTIVATE, 1 - player,
                                                     -1))
              {
                if (stack_size < 32)
                  put_card_or_activation_onto_stack(player, card, EVENT_RESOLVE_ACTIVATION, player,
                                                    0);
                needed_mana_colors = payment_calculate_needed_mana_colors(old_pay_mana);
                tapped_for_mana_color = -1;
                int was_tapped = inst->state & STATE_TAPPED;
                event_activate_then_duplicate_into_stack(player, card, EVENT_ACTIVATE, 1 - player,
                                                         -1);

                needed_mana_colors = 0;

                if (spell_fizzled == 1)
                {
                  spell_fizzled = 0;
                  obliterate_top_card_of_stack();
                }
                else
                {
                  land_can_be_played &= ~LCBP_CHARGING_MANA;

                  if (!was_tapped && (inst->state & STATE_TAPPED))
                  {
                    dispatch_event(player, card, EVENT_TAP_CARD);
                    dispatch_trigger2(player, TRIGGER_TAP_CARD, gs_prompt_specialfephase_7, 1, player, card);
                    play_sound_effect(WAV_TAP);
                  }
                  payment_resolve_top_card_on_stack();

                  land_can_be_played |= LCBP_CHARGING_MANA;

                  payment_TENTATIVE_reassess_all_cards();
                }
              }

              charge_mana_addr_of_pre_mana_pool = old_charge_mana_addr_of_pre_mana_pool;
              charge_mana_addr_of_x_value = old_charge_mana_addr_of_x_value;
              charge_mana_addr_of_pay_mana_xbugrw = old_charge_mana_addr_of_pay_mana_xbugrw;
              charge_mana_addr_of_max_x_value = old_charge_mana_addr_of_max_x_value;

              x_value = old_x_value;
              max_x_value = old_max_x_value;

              for (i = 0; i < 8; ++i)
                pay_mana_xbugrwaU[i] = old_pay_mana[i];

              for (i = 0; i < 8; ++i)
                mana_pool[player][i] -= old_mana_pool[i];

              amt_of_nonartifact_mana = 0;
              payment_apply_mana_in_pool_to_cost(player, amtspaid, &total_paid,
                                                 amt_of_nonartifact_mana);
              payment_apply_mana_in_pool_to_colorless_cost(
                  player, amtspaid, &total_paid, amt_of_nonartifact_mana, &x_value, max_x_value);
              payment_apply_mana_in_pool_to_x_cost(player, amtspaid, &total_paid,
                                                   amt_of_nonartifact_mana, &x_value, max_x_value);

              for (i = 0; i < 8; ++i)
                mana_pool[player][i] += old_mana_pool[i];

              payment_copy_mana_pool_to_display();
            }
          }
        }
        else if (g_target_selection_status_code == -2)
        {
          if (payment_WILDGUESS_whose_phase_to_run_to == -1 &&
              payment_WILDGUESS_phase_to_run_to == -1)
          {
            if (card == -1)
              spell_fizzled = 1;
            break;
          }
          else if (!any_nonx_mana)
            break;
        }
        else if (g_target_selection_status_code == -3 && g_recorded_action_controller == player &&
                 payment_TENTATIVE_color_clicked_in_mana_summary_pane != -1)
        {
          if (mana_pool[player][payment_TENTATIVE_color_clicked_in_mana_summary_pane] > 0)
          {

            if (payment_TENTATIVE_color_clicked_in_mana_summary_pane != COLOR_ARTIFACT ||
                pay_mana_xbugrwaU[COLOR_ARTIFACT])
            {
              int col_to_apply_to = -1;
              if (pay_mana_xbugrwaU[payment_TENTATIVE_color_clicked_in_mana_summary_pane])
                col_to_apply_to = payment_TENTATIVE_color_clicked_in_mana_summary_pane;
              else if (pay_mana_xbugrwaU[COLOR_ARTIFACT])
                col_to_apply_to = COLOR_ARTIFACT;
              else if (pay_mana_xbugrwaU[COLOR_COLORLESS])
                col_to_apply_to = COLOR_COLORLESS;

              if (col_to_apply_to != -1)
              {
                payment_apply_mana_to_cost(
                    pay_mana_xbugrwaU, col_to_apply_to,
                    payment_amt_to_apply_to_cost(
                        pay_mana_xbugrwaU, col_to_apply_to, mana_pool[player],
                        payment_TENTATIVE_color_clicked_in_mana_summary_pane, ldoubleclicked,
                        max_x_value, x_value),
                    &x_value, max_x_value, player,
                    payment_TENTATIVE_color_clicked_in_mana_summary_pane, amtspaid, &total_paid);
                payment_copy_mana_pool_to_display();
                continue;
              }
            }

            {
              color_test_t colors = 0;
              int n;
              for (n = 0; n < 10 && payment_raw_mana_may_be_spent_as_color[player][n] != -1; ++n)
                if (payment_TENTATIVE_color_clicked_in_mana_summary_pane ==
                    LOWORD(payment_raw_mana_may_be_spent_as_color[player][n]))
                  colors |= 1u << (BYTE3(payment_raw_mana_may_be_spent_as_color[player][n]) & 31);

              color_test_t paymask = 0;
              for (c = COLOR_COLORLESS; c <= COLOR_ARTIFACT; ++c)
                if (pay_mana_xbugrwaU[c])
                  paymask |= 1 << c;
              colors &= paymask;

              if (colors)
              {
                int num_colors = num_bits_set(colors);

                color_t col_to_apply_to;
                if (num_colors == 1)
                  col_to_apply_to = payment_single_color_test_bit_to_color_t(colors);
                else
                {
                  load_text(0, "PROMPT_GRABMANA_DUALUSE");
                  col_to_apply_to = choose_a_color_exe(
                      player, text_lines[0], 1,
                      payment_TENTATIVE_color_clicked_in_mana_summary_pane, colors);
                }

                payment_apply_mana_to_cost(
                    pay_mana_xbugrwaU, col_to_apply_to,
                    payment_amt_to_apply_to_cost(
                        pay_mana_xbugrwaU, col_to_apply_to, mana_pool[player],
                        payment_TENTATIVE_color_clicked_in_mana_summary_pane, ldoubleclicked,
                        max_x_value, x_value),
                    &x_value, max_x_value, player,
                    payment_TENTATIVE_color_clicked_in_mana_summary_pane, amtspaid, &total_paid);
                payment_copy_mana_pool_to_display();
              }
            }
          }
        }
      }

    if (!(old_land_can_be_played & LCBP_CHARGING_MANA))
      land_can_be_played &= ~LCBP_CHARGING_MANA;
  }

  if (spell_fizzled == 1)
  {
    payment_TENTATIVE_update_mana_spent(amtspaid);
    color_t c;
    for (c = COLOR_COLORLESS; c <= COLOR_ARTIFACT; ++c)
    {
      produce_mana(player, c, amtspaid[c]);
      amtspaid[c] = 0;
    }
    total_paid = 0;
    x_value = 0;
  }

  int c;
  for (c = 0; c < 7; ++c)
  {
    mana_paid[c] = amtspaid[c];
    pay_mana_xbugrwaU[c] = 0;
  }
  pay_mana_xbugrwaU[7] = 0;

  if ((card_flags[player] & CFLAGS_KRRIK) != 0)
    lose_life(player, life_to_pay_for_krrik);

  max_x_value = -1;
  unknown62BCEC = -1;

  affected_card_controller = old_affected_card_controller;
  affected_card = old_affected_card;

  return total_paid;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26adc90; original functions/engine.c:3282. */
void allow_response_to_activation(int player, int card)
{

  const char *name = get_displayed_card_name(player, card);
  sprintf(global_all_purpose_buffer, gs_prompt_checkfephase_3, name);

  int a1 = (g_phase_stop_suppressed && (player == 0 || (trace_mode & 2))) ? -1 : -2;
  allow_response(a1, current_phase, global_all_purpose_buffer,
                                                     EVENT_ACTIVATE);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26add20; original functions/engine.c:3293. */
int finalize_activation(int player, int card)
{

  if (cancel == 1)
    return 1;

  card_instance_t *instance = get_card_instance(player, card);
  int iid = get_card_instance(player, card)->internal_card_id;

  if (iid == -1)
    iid = instance->backup_internal_card_id;

  int tapping_for_mana =
      (cards_data[iid].extra_ability & EA_MANA_SOURCE) && tapped_for_mana_color != -1;

  if (!tapping_for_mana && !cant_be_responded_to)
    allow_response_to_activation(player, card);

  resolve_top_card_on_stack();

  dispatch_trigger2(current_turn, TRIGGER_TAP_CARD, gs_prompt_specialfephase_7, 0, player, card);

  return 1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a8540; original functions/engine.c:3320. */
static void put_card_or_activation_onto_stack_impl(int player, int card, event_t event, int a4,
                                                 int a5)
{

  card_instance_t *instance = get_card_instance(player, card);

  stack_data[stack_size].internal_card_id = instance->internal_card_id;
  stack_data[stack_size].generating_event = event;
  stack_data[stack_size].unknown_sdt = a4;

  int stack_card;
  if (event == EVENT_RESOLVE_SPELL || event == EVENT_RESOLVE_TRIGGER ||
      instance->internal_card_id <= 4)
    stack_card = card;
  else
  {
    stack_card = add_card_to_hand(player, activation_card);
    if (stack_card == -1)
      return;
    card_instance_t *stack_inst = get_card_instance(player, stack_card);
    int orig_timestamp = stack_inst->timestamp;
    memcpy_inst_and_ext(stack_inst, instance);
    stack_inst->internal_card_id = activation_card;
    stack_inst->unknown0x14 = 0;
    stack_inst->kill_code = 0;
    if (instance->internal_card_id != -1)
      stack_inst->original_internal_card_id = instance->internal_card_id;

    else
      stack_inst->original_internal_card_id = instance->backup_internal_card_id;

    stack_inst->state |= STATE_IN_PLAY;
    --hand_count[player];
    stack_inst->parent_controller = player;
    stack_inst->parent_card = card;
    stack_inst->timestamp = orig_timestamp;

    stack_inst->token_status &= ~STATUS_DYING;
  }

  stack_cards[stack_size].player = player;
  stack_cards[stack_size].card = stack_card;

  stack_damage_targets[stack_size].player = instance->damage_target_player;
  stack_damage_targets[stack_size].card = instance->damage_target_card;

  if (trigger_condition == (trigger_t)(-1))
    stack_phase[stack_size] = current_phase;
  else
    stack_phase[stack_size] = trigger_condition;

  if (ai_is_speculating != 1)
    g_stack_prompt_flags[stack_size] = a5;

  ++stack_size;
  stack_cards[stack_size].player = -1;

  if (totally_bletcherous_hack_dont_reset_x_value.player >= 0 && event == EVENT_RESOLVE_SPELL &&
      totally_bletcherous_hack_dont_reset_x_value.player == player &&
      totally_bletcherous_hack_dont_reset_x_value.card == card)
  {
    totally_bletcherous_hack_dont_reset_x_value.player = -1;
    g_adventure_world_state = 1;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ade00; original functions/engine.c:3320. */
void put_card_or_activation_onto_stack(int player, int card, event_t event, int a4, int a5)
{
  if (stack_size >= 32)
    return;
  put_card_or_activation_onto_stack_impl(player, card, event, a4, a5);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ade40; original functions/engine.c:3390. */
void recopy_card_onto_stack(int a1)
{

  int pos = stack_size - 1;

  card_instance_t *stack_inst = get_card_instance(stack_cards[pos].player, stack_cards[pos].card);
  if (stack_inst->internal_card_id == activation_card)
  {
    int pc = stack_inst->parent_card;
    int pp = stack_inst->parent_controller;
    int ts = stack_inst->timestamp;

    card_instance_t *parent_inst =
        get_card_instance(stack_inst->parent_controller, stack_inst->parent_card);

    if (parent_inst->state & STATE_DONT_RECOPY_ONTO_STACK)
      parent_inst->state &= ~STATE_DONT_RECOPY_ONTO_STACK;
    else
    {
      memcpy_inst_and_ext(stack_inst, parent_inst);

      stack_inst->internal_card_id = activation_card;
      stack_inst->unknown0x14 = 0;
      stack_inst->kill_code = 0;
      stack_inst->state |= STATE_IN_PLAY;
      stack_inst->parent_controller = pp;
      stack_inst->parent_card = pc;
      stack_inst->timestamp = ts;

      if (parent_inst->internal_card_id != -1)
        stack_inst->original_internal_card_id = parent_inst->internal_card_id;

      else
        stack_inst->original_internal_card_id = parent_inst->backup_internal_card_id;

      if ((int)stack_inst->original_internal_card_id < damage_card ||
          (int)stack_inst->original_internal_card_id >= damage_card + 47)
      {
        stack_inst->display_pic_csv_id = cards_data[stack_inst->original_internal_card_id].id;

        stack_inst->display_pic_num = get_card_image_number(stack_inst->display_pic_csv_id, pp, pc);
      }
    }

    if (stack_inst->internal_card_id == activation_card)
      stack_inst->token_status &= ~STATUS_DYING;
  }

  if (ai_is_speculating != 1)
    g_stack_prompt_flags[pos] = a1;

  stack_damage_targets[pos].player = stack_inst->damage_target_player;
  stack_damage_targets[pos].card = stack_inst->damage_target_card;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26adfa0; original functions/engine.c:3761. */
int put_card_on_stack3(int player, int card)
{
  enum
  {
    STATE_BEING_CAST = 0x80000000u
  };

  card_instance_t *instance = get_card_instance(player, card);

  int iid = instance->internal_card_id;
  instance->state |= STATE_BEING_CAST;
  if (iid == -1)
  {
    obliterate_top_card_of_stack();
    land_can_be_played &= ~LCBP_SPELL_BEING_PLAYED;
    return 0;
  }

  if (is_what(player, card, TYPE_LAND))
    dispatch_event(player, card, EVENT_LAND_PLAYED);
  else if (!check_special_flags(player, card, SF_NOT_CAST))
  {
    unsigned int amount = instance->ext->times_replicate_paid;
    if (query(player, card, iid, QUERY_STORM))
      amount = get_storm_count();
    if (query(player, card, iid, QUERY_GRAVESTORM))
      amount = get_dead_count(2, 0xff);
    while (amount != 0)
    {
      copy_spell_from_stack(player, player, card);
      --amount;
    }
    dispatch_event(player, card, EVENT_SPELL_CAST);
  }
  dispatch_event_to_single_card(player, card, EVENT_THIS_SPELL_CAST, -1, -1);

  dispatch_trigger2(current_turn, TRIGGER_SPELL_CAST, gs_prompt_specialfephase_6, 0, player, card);

  land_can_be_played &= ~LCBP_SPELL_BEING_PLAYED;
  iid = instance->internal_card_id;
  if (iid == -1)
  {
    obliterate_top_card_of_stack();
    return 0;
  }

  card_data_t *cd = get_card_data(player, card);

  if (!is_what(player, card, TYPE_LAND))
  {
    const char *name = get_displayed_card_name(player, card);
    sprintf(global_all_purpose_buffer, gs_prompt_checkfephase_2, name);
    allow_response(-2, current_phase, global_all_purpose_buffer,
                                                       EVENT_CAST_SPELL);
  }
  instance->state &= ~STATE_INVISIBLE;
  instance->ext->host_summoning_sickness = 1;
  instance->state |= STATE_IN_PLAY | STATE_SUMMONSICK_BOTH;

  uint8_t type = cd->type;
  types_of_cards_on_bf[player] |= type;
  if (ai_is_speculating != 1)
  {
    if (type & TYPE_CREATURE)
      play_sound_effect(WAV_SUMMON);
    if (type & TYPE_ARTIFACT)
      play_sound_effect(WAV_ARTIFACT);
    if (type & TYPE_ENCHANTMENT)
    {
      if (cd->cc[2] == 9)
        play_sound_effect(WAV_PLANESWALKER);
      else
        play_sound_effect(WAV_ENCHANT);
    }
    if (type & TYPE_INSTANT)
      play_sound_effect(WAV_INSTANT);
    if (type & TYPE_INTERRUPT)
      play_sound_effect(WAV_INTERUPT);
    if (type & TYPE_SORCERY)
      play_sound_effect(WAV_SORCERY);
    if (has_subtype(player, card, SUBTYPE_SNOW))
      play_sound_effect((wav_t)WAV_SNOW);
  }

  instance->state &= ~STATE_BEING_CAST;
  if (enable_xtrigger_flags & ENABLE_XTRIGGER_REPLACE_ETB)
    dispatch_xtrigger2(player, (xtrigger_t)XTRIGGER_REPLACE_ETB, "enters the battlefield", 0,
                       player, card);

  resolve_top_card_on_stack();

  if (!(instance->internal_card_id == -1 && is_what(-1, iid, TYPE_PERMANENT)))
  {
    push_affected_card_stack();
    get_card_instance(player, card)->regen_status |= KEYWORD_RECALC_ALL;
    dispatch_trigger2(current_turn, TRIGGER_COMES_INTO_PLAY, gs_card_into_play_0091c840, 0, player, card);
    pop_affected_card_stack();
  }

  process_damage_prevention();
  event_flags &= ~EF_RERUN_DAMAGE_PREVENTION;
  #ifdef MANALINKEH_STATIC
  pending_killed_card_handler = process_killed_card;
#else
  pending_killed_card_handler = 0x477B30;
  #endif
  regenerate_or_graveyard_triggers();
  process_damage_prevention();
  g_current_casting_internal_card_id = -1;

  if (cd->type & TYPE_LAND)
    ++lands_played;

  if (cancel == 1)
  {
    if (ai_is_speculating != 1)
    {
      load_text(0, "PROMPT_FIZZLE");
      set_duel_prompt_text(global_all_purpose_buffer);
      Sleep(2000);
      set_duel_prompt_text("");
    }
    g_ai_speculation_spell_fizzled = 1;
    cancel = 0;
    instance->state &= ~STATE_BEING_CAST;
    return 0;
  }
  else
  {
    cancel = 0;
    TENTATIVE_reassess_all_cards();
    return 1;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26b0480; original functions/engine.c:3905. */
int is_a_tappable_mana_source(int player, int card)
{
  card_instance_t *instance = in_play(player, card);
  if (instance && (cards_data[instance->internal_card_id].extra_ability & EA_MANA_SOURCE))
  {
    dispatch_event_with_attacker_to_one_card(player, card, EVENT_CAN_ACTIVATE, 1 - player, -1);
    return event_rval;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26b0500; original functions/engine.c:3959. */
void force_activation_for_mana(int player, int card, color_test_t colors)
{
  if (stack_size < 32)
    put_card_or_activation_onto_stack_impl(player, card, EVENT_RESOLVE_ACTIVATION, player, 0);
  needed_mana_colors = colors;
  tapped_for_mana_color = -1;
  int was_tapped = is_tapped(player, card);
  event_activate_then_duplicate_into_stack(player, card, EVENT_ACTIVATE, 1 - player, -1);
  needed_mana_colors = 0;
  if (cancel == 1)
  {
    cancel = 0;
    obliterate_top_card_of_stack();
  }
  else
  {
    if (was_tapped || !is_tapped(player, card))
      dispatch_event(player, card, EVENT_PLAY_ABILITY);
    else
    {
      dispatch_event(player, card, EVENT_TAP_CARD);
      dispatch_event(player, card, EVENT_TAPPED_TO_PLAY_ABILITY);
      dispatch_trigger2(player, TRIGGER_TAP_CARD, gs_prompt_specialfephase_7, 1, player, card);
    }
    play_sound_effect(WAV_TAP);
    resolve_top_card_on_stack();
    TENTATIVE_reassess_all_cards();
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26b0650; original functions/engine.c:3937. */
void tap_card_for_mana(int player, int card)
{
  force_activation_for_mana(player, card, COLOR_TEST_COLORLESS);
}

#endif

#ifndef SHANDALAR
/* October 2022 public X-format override; native .bss @ 0x2775a80. */
int override_xx;
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a8700; original functions/engine.c:8. */
void count_colors_of_lands_in_play(void)
{

  int i;
  for (i = 0; i < 8; ++i)
  {
    basiclandtypes_controlled[0][i] = 0;
    basiclandtypes_controlled[1][i] = 0;
    landsofcolor_controlled[0][i] = 0;
    landsofcolor_controlled[1][i] = 0;
  }

  colors_of_lands_in_play[0] = 0;
  colors_of_lands_in_play[1] = 0;

  int player, card;
  for (player = 0; player < 2; ++player)
    for (card = 0; card < active_cards_count[player]; ++card)
      if (in_play(player, card) && is_what(player, card, TYPE_LAND))
      {
        card_instance_t *instance = get_card_instance(player, card);

        int colors = instance->card_color | mana_colors_added[player];
        int basics;

        ++landsofcolor_controlled[player][COLOR_ANY];
        ++basiclandtypes_controlled[player][COLOR_ANY];

        if (check_special_flags2(player, card, SF2_PRISMATIC_OMEN))
        {
          colors |= COLOR_TEST_ANY_COLORED;
          basics = COLOR_TEST_ANY_COLORED;
        }
        else
        {
          basics = 0;

          if (has_subtype(player, card, SUBTYPE_SWAMP))
            basics |= COLOR_TEST_BLACK;
          if (has_subtype(player, card, SUBTYPE_ISLAND))
            basics |= COLOR_TEST_BLUE;
          if (has_subtype(player, card, SUBTYPE_FOREST))
            basics |= COLOR_TEST_GREEN;
          if (has_subtype(player, card, SUBTYPE_MOUNTAIN))
            basics |= COLOR_TEST_RED;
          if (has_subtype(player, card, SUBTYPE_PLAINS))
            basics |= COLOR_TEST_WHITE;

          colors |= basics;
        }

        if (check_special_flags2(player, card, SF2_CONTAMINATION))
          colors = COLOR_TEST_BLACK;

        if (colors & COLOR_TEST_BLACK)
          ++landsofcolor_controlled[player][COLOR_BLACK];
        if (basics & COLOR_TEST_BLACK)
          ++basiclandtypes_controlled[player][COLOR_BLACK];

        if (colors & COLOR_TEST_BLUE)
          ++landsofcolor_controlled[player][COLOR_BLUE];
        if (basics & COLOR_TEST_BLUE)
          ++basiclandtypes_controlled[player][COLOR_BLUE];

        if (colors & COLOR_TEST_GREEN)
          ++landsofcolor_controlled[player][COLOR_GREEN];
        if (basics & COLOR_TEST_GREEN)
          ++basiclandtypes_controlled[player][COLOR_GREEN];

        if (colors & COLOR_TEST_RED)
          ++landsofcolor_controlled[player][COLOR_RED];
        if (basics & COLOR_TEST_RED)
          ++basiclandtypes_controlled[player][COLOR_RED];

        if (colors & COLOR_TEST_WHITE)
          ++landsofcolor_controlled[player][COLOR_WHITE];
        if (basics & COLOR_TEST_WHITE)
          ++basiclandtypes_controlled[player][COLOR_WHITE];

        if (colors & COLOR_TEST_ARTIFACT)
          ++landsofcolor_controlled[player][COLOR_ARTIFACT];

        if (!(colors & (COLOR_TEST_ANY_COLORED | COLOR_TEST_ARTIFACT)))
          ++landsofcolor_controlled[player][COLOR_COLORLESS];

        if (!(basics & COLOR_TEST_ANY_COLORED))
          ++basiclandtypes_controlled[player][COLOR_COLORLESS];

        colors_of_lands_in_play[player] |= colors;
      }

  colors_of_lands_in_play[0] &= COLOR_TEST_ANY_COLORED;
  colors_of_lands_in_play[1] &= COLOR_TEST_ANY_COLORED;

  if ((player_bits[current_turn] & PB_COUNT_TOTAL_PLAYABLE_LANDS) &&
      (land_can_be_played & LCBP_LAND_HAS_BEEN_PLAYED) &&
      lands_played < total_playable_lands(current_turn))
    land_can_be_played &= ~LCBP_LAND_HAS_BEEN_PLAYED;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a9e40; original functions/engine.c:120. */
void recalculate_all_cards_in_play(void)
{
  memset(card_counts, 0, sizeof(card_counts));
  player_bits[0] &= 0xf0c0000fu;
  player_bits[1] &= 0xf0c0000fu;
  memset(card_flags, 0, sizeof(card_flags));
  memset(add_to_die_rolls, 0, sizeof(add_to_die_rolls));
  memset(roll_extra_dice, 0, sizeof(roll_extra_dice));

  for (int i = 0; i < 15; ++i)
  {
    added_subtypes_not_in_play[1][i].subtype = SUBTYPE_INVALID;
    added_subtypes_not_in_play[0][i].subtype = SUBTYPE_INVALID;
    added_subtypes_not_in_play[1][i].added_forced = 0;
    added_subtypes_not_in_play[0][i].added_forced = 0;
  }

  minimize_nondraining_mana();

  int p, c;

  for (p = 0; p < 2; ++p)
  {
    card_instance_t *instance = get_card_instance(p, 0);
    for (c = 0; c < active_cards_count[p]; ++c, ++instance)
    {
      if (instance->internal_card_id == -1)
        continue;
      if ((instance->state & (STATE_OUBLIETTED | STATE_IN_PLAY)) != STATE_IN_PLAY)
      {
        reset_subtypes_from_instance(instance, instance->internal_card_id);
        continue;
      }
      instance->regen_status |= KEYWORD_RECALC_CHANGE_TYPE;
      get_abilities(p, c, EVENT_CHANGE_TYPE, -1);
      instance->regen_status |= KEYWORD_RECALC_SET_COLOR | KEYWORD_RECALC_ABILITIES |
                                KEYWORD_RECALC_POWER | KEYWORD_RECALC_TOUGHNESS;
      get_abilities(p, c, EVENT_SET_COLOR, -1);
      get_abilities(p, c, EVENT_ABILITIES, -1);
    }
  }

  count_colors_of_lands_in_play();

  count_mana();

  player_bits[0] &= ~PB_HAND_REVEALED;
  player_bits[1] &= ~PB_HAND_REVEALED;

  card_instance_t *instance;
  for (p = 0; p < 2; ++p)
    for (c = 0; c < active_cards_count[p]; ++c)
      if ((instance = in_play(p, c)))
      {
        if (is_what(p, c, TYPE_CREATURE))
        {
          get_abilities(p, c, EVENT_TOUGHNESS, -1);
          if (instance->internal_card_id == -1)
            continue;
          get_abilities(p, c, EVENT_POWER, -1);
        }

        if (instance->counters_byte > 0 && instance->counters_m1m1 > 0 &&
            is_what(p, c, TYPE_PERMANENT) &&
            !(instance->toughness <= 0 && is_what(p, c, TYPE_CREATURE)))
        {
          int n = MIN(instance->counters_byte, instance->counters_m1m1);
          instance->counters_byte -= n;
          instance->counters_m1m1 -= n;
        }

        dispatch_event_with_attacker_to_one_card(p, c, EVENT_STATIC_EFFECTS, 1 - p, -1);
      }

  if (ai_is_speculating == 0)
  {
    if (!(player_bits[0] & PB_HAND_REVEALED) != !(g_duel_state_008cfd20 & (1 << 0)))
    {
      if (player_bits[0] & PB_HAND_REVEALED)
        g_duel_state_008cfd20 |= 1 << 0;
      else
        g_duel_state_008cfd20 &= ~(1 << 0);
    }

    if (!(player_bits[1] & PB_HAND_REVEALED) != !(g_duel_state_008cfd20 & (1 << 1)))
    {
      if (player_bits[1] & PB_HAND_REVEALED)
        g_duel_state_008cfd20 |= 1 << 1;
      else
        g_duel_state_008cfd20 &= ~(1 << 1);

      resize_duel_hand_window(hwnd_HandClass_opponent);
    }
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a8bb0; original functions/engine.c:241. */
void phase_changed(int player, int new_phase)
{

  if (ai_is_speculating != 1)
    update_duel_selection_display_if_human(player, new_phase);

  cancel = 0;

  dispatch_event(player, 0, EVENT_PHASE_CHANGED);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a8c00; original functions/engine.c:443. */
int effect_asterisk(int player, int card, event_t event)
{

  card_instance_t *instance = get_card_instance(player, card);
  if (!affect_me(instance->damage_target_player, instance->damage_target_card) ||
      affected_card == -1 ||
      !((event == EVENT_POWER && (instance->eot_toughness & ASTERISK_AFFECTS_POWER)) ||
        (event == EVENT_TOUGHNESS && (instance->eot_toughness & ASTERISK_AFFECTS_TOUGHNESS))))
    return 0;

  int p, c;
  int tp = instance->damage_target_player, tc = instance->damage_target_card;
  int modifier = 0;
  int count_who = instance->eot_toughness & (ASTERISK_COUNT_CONTROLLER | ASTERISK_COUNT_OPPONENT);
  int category =
      instance->eot_toughness &
      (ASTERISK_SUBTYPE_OF_INFO_SLOT | ASTERISK_HAND_COUNT |
       ASTERISK_CREATURES_IN_GRAVEYARD_PLUS_POWER_IN_BYTE0_TOUGHNESS_IN_BYTE1_OF_INFO_SLOT |
       ASTERISK_POWER_IN_BYTE0_TOUGHNESS_IN_BYTE1_MIN1_OF_INFO_SLOT | ASTERISK_NONWALL_CREATURES |
       ASTERISK_POWER_IN_BYTE0_TOUGHNESS_IN_BYTE1_OF_INFO_SLOT | ASTERISK_IID_OF_INFO_SLOT |
       ASTERISK_BASICLAND_TYPES_OF_INFO_SLOT);

  switch (category)
  {
  case ASTERISK_BASICLAND_TYPES_OF_INFO_SLOT:

    if (instance->eot_toughness & ASTERISK_COUNT_CONTROLLER)
      modifier += basiclandtypes_controlled[tp][get_hacked_color(tp, tc, instance->info_slot)];

    if (instance->eot_toughness & ASTERISK_COUNT_OPPONENT)
      modifier += basiclandtypes_controlled[1 - tp][get_hacked_color(tp, tc, instance->info_slot)];

    break;

  case ASTERISK_IID_OF_INFO_SLOT:

    for (p = 0; p < 2; ++p)
      if (count_who & (p == tp ? ASTERISK_COUNT_CONTROLLER : ASTERISK_COUNT_OPPONENT))
        for (c = 0; c < active_cards_count[p]; ++c)
          if (in_play(p, c))
          {
            if (instance->eot_toughness &
                ASTERISK_TYPE_OF_INFO_SLOT__MUST_ALSO_SET_IID_OF_INFO_SLOT)
            {
              if (is_what(p, c, instance->info_slot))
                ++modifier;
            }
            else if (get_id(p, c) == cards_data[instance->info_slot].id)
              ++modifier;
          }
    break;

  case ASTERISK_NONWALL_CREATURES:

    for (p = 0; p < 2; ++p)
      if (count_who & (p == tp ? ASTERISK_COUNT_CONTROLLER : ASTERISK_COUNT_OPPONENT))
        for (c = 0; c < active_cards_count[p]; ++c)
          if (in_play(p, c) && is_what(p, c, TYPE_CREATURE) && !has_subtype(p, c, SUBTYPE_WALL))
            ++modifier;
    break;

  case ASTERISK_CREATURES_IN_GRAVEYARD_PLUS_POWER_IN_BYTE0_TOUGHNESS_IN_BYTE1_OF_INFO_SLOT:

    for (p = 0; p < 2; ++p)
      if (count_who & (p == tp ? ASTERISK_COUNT_CONTROLLER : ASTERISK_COUNT_OPPONENT))
        for (c = 0; c < 500; ++c)
        {
          int iid = get_grave(p)[c];
          if (iid >= 0 && is_what(-1, iid, TYPE_CREATURE))
            ++modifier;
        }

    if (event == EVENT_POWER)
      modifier += BYTE0(instance->info_slot);
    else
      modifier += BYTE1(instance->info_slot);
    break;

  case ASTERISK_HAND_COUNT:

    if (count_who & ASTERISK_COUNT_CONTROLLER)
      modifier += hand_count[player];
    if (count_who & ASTERISK_COUNT_OPPONENT)
      modifier += hand_count[1 - player];
    break;

  case ASTERISK_SUBTYPE_OF_INFO_SLOT:

    for (p = 0; p < 2; ++p)
      if (count_who & (p == tp ? ASTERISK_COUNT_CONTROLLER : ASTERISK_COUNT_OPPONENT))
        for (c = 0; c < active_cards_count[p]; ++c)
          if (in_play(p, c) &&
              has_subtype(p, c, from_hardcodedsubtype_to_subtype(instance->info_slot)))
            ++modifier;
    break;

  case ASTERISK_POWER_IN_BYTE0_TOUGHNESS_IN_BYTE1_OF_INFO_SLOT:

    if (event == EVENT_POWER)
      modifier = BYTE0(instance->info_slot);
    else
      modifier = BYTE1(instance->info_slot);
    break;

  case ASTERISK_POWER_IN_BYTE0_TOUGHNESS_IN_BYTE1_MIN1_OF_INFO_SLOT:

    if (event == EVENT_POWER)
      modifier = BYTE0(instance->info_slot);
    else
    {
      modifier = BYTE1(instance->info_slot);
      if (event_result + modifier < 1)
      {
        if (event_result > 1)
          modifier = 1 - event_result;
        else
          modifier = 0;
      }
    }
    break;
  }

  event_result += modifier;

  if (event == EVENT_POWER)
    instance->counter_power = modifier;
  else
    instance->counter_toughness = modifier;

  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a9140; original functions/engine.c:621. */
int create_legacy_effect_exe(int player, int card, int iid, int t_player, int t_card)
{

  if (hack_force_effect_change_source.from.player != -1 &&
      player == hack_force_effect_change_source.from.player &&
      card == hack_force_effect_change_source.from.card)
  {
    player = hack_force_effect_change_source.to.player;
    card = hack_force_effect_change_source.to.card;
  }

  card_instance_t *instance = get_card_instance(player, card);
  int csvid;

  if (instance->internal_card_id == activation_card)
    return shandalar_create_legacy_effect(instance->parent_controller, instance->parent_card, iid, t_player, t_card);

  if (instance->internal_card_id < 0)
    return shandalar_create_legacy_effect(player, card, iid, t_player, t_card);

  csvid = cards_data[instance->internal_card_id].id;
  if (csvid >= 901 && csvid <= 908)
  {
    int leg = shandalar_create_legacy_effect(player, card, iid, t_player, t_card);
    if (leg != -1)
    {
      card_instance_t *legacy = get_card_instance(player, leg);
      legacy->damage_source_player = instance->damage_source_player;
      legacy->damage_source_card = instance->damage_source_card;
      legacy->original_internal_card_id = instance->original_internal_card_id;
      legacy->display_pic_csv_id = instance->display_pic_csv_id;
      legacy->display_pic_num = instance->display_pic_num;
    }
    return leg;
  }

  if (csvid == CARD_ID_FACE_DOWN_CREATURE && instance->targets[12].player >= 0 &&
      instance->targets[12].player <= available_slots)
  {
    int leg = shandalar_create_legacy_effect(player, card, iid, t_player, t_card);
    if (leg != -1)
    {
      card_instance_t *legacy = get_card_instance(player, leg);
      legacy->original_internal_card_id =
          get_internal_card_id_from_csv_id(instance->targets[12].player);
      legacy->display_pic_csv_id = instance->targets[12].player;
      legacy->display_pic_num = get_card_image_number(instance->targets[12].player, player, card);
    }
    return leg;
  }

  return shandalar_create_legacy_effect(player, card, iid, t_player, t_card);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a9320; original functions/engine.c:675. */
int create_legacy_effect_from_iid(int player, int iid, int (*func_ptr)(int, int, event_t),
                                       int t_player, int t_card)
{
  int legacy_card = add_card_to_hand(player, LEGACY_EFFECT_CUSTOM);
  if (legacy_card == -1)
    return -1;

  card_instance_t *legacy = get_card_instance(player, legacy_card);
  legacy->state = (player == 1) ? STATE_IN_PLAY | STATE_OWNED_BY_OPPONENT : STATE_IN_PLAY;
  --hand_count[player];

  card_data_t *cd = &cards_data[iid];
  legacy->mana_color = legacy->card_color = legacy->initial_color = legacy->color = cd->color;
  legacy->damage_source_player = player;
  legacy->damage_source_card = legacy_card;

  int csvid = cards_data[iid].id;

  legacy->display_pic_csv_id = csvid;
  int num_pics = cards_ptr[csvid]->num_pics;
  legacy->display_pic_num = num_pics <= 1 ? 0 : internal_rand(num_pics);

  legacy->damage_target_player = t_player;
  legacy->damage_target_card = t_card;

  if (t_player != -1 && t_card != -1)
  {
    card_instance_t *attached_to = get_card_instance(t_player, t_card);
    attached_to->regen_status |= KEYWORD_RECALC_ALL;
  }

  legacy->info_slot = (int)func_ptr;

  return legacy_card;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a9520; original functions/engine.c:747. */
void format_manacost_into_global_allpurpose_buffer(int ignored, int *mana_cost_array,
                                                        int x_so_far, int max_x)
{

  char intro[300];
  intro[0] = 0;

  if (stack_size > 0)

    format_stack_action_text(intro, stack_data[stack_size - 1].generating_event,
                stack_cards[stack_size - 1].player, stack_cards[stack_size - 1].card);

  char str[100];
  str[0] = 0;
  char *p = str;

  if (mana_cost_array[COLOR_COLORLESS] == -1 || mana_cost_array[COLOR_ARTIFACT] == -1)
  {

    int our_xx = hack_xx;
    const char *cost;
    if (hack_xx == 1 && stack_size > 0 &&
        stack_data[stack_size - 1].generating_event == EVENT_RESOLVE_SPELL &&
        (cost =
             cards_ptr[get_id(stack_cards[stack_size - 1].player, stack_cards[stack_size - 1].card)]
                 ->mana_cost_text) &&
        strstr(cost, "|X|X"))
    {
      our_xx = 2;
      if (strstr(cost, "|X|X|X"))
        our_xx = 3;
    }

    if (override_xx != 0)
      our_xx = override_xx;

    int divisor = our_xx == 2 ? 2 : our_xx == 3 ? 3 : 1;
    const char *fmt = divisor == 1 ? "|X" : divisor == 2 ? "|X|X" : "|X|X|X";

    if (max_x == -1)
      p += sprintf(p, gs_prompt_grabmana_1, fmt, x_so_far / divisor);
    else if (max_x > x_so_far)
      p += sprintf(p, gs_prompt_grabmana_2, fmt, x_so_far / divisor, max_x / divisor);
  }
  else if (mana_cost_array[COLOR_COLORLESS] || mana_cost_array[COLOR_ARTIFACT])
  {
    int generic = mana_cost_array[COLOR_COLORLESS] + mana_cost_array[COLOR_ARTIFACT];
    if (generic > 20)
      while (generic >= 10)
      {
        p += sprintf(p, "|10");
        generic -= 10;
      }

    if (generic)
      p += sprintf(p, "|%d", generic);
  }

  char mana_symbol[3] = {'|', 'B', 0};
  const char *mana_colors = "XBUGRW";
  int i, j;
  for (i = COLOR_BLACK; i <= COLOR_WHITE; ++i)
    if (mana_cost_array[i])
    {
      mana_symbol[1] = mana_colors[i];

      if (mana_cost_array[i] == -1)
      {
        if (max_x == -1)
          p += sprintf(p, gs_prompt_grabmana_1, mana_symbol, x_so_far);
        else if (max_x > x_so_far)
          p += sprintf(p, gs_prompt_grabmana_2, mana_symbol, x_so_far, max_x);
      }
      else
        for (j = 0; mana_cost_array[i] > j; ++j)
          p += sprintf(p, "%s", mana_symbol);
    }

  p = global_all_purpose_buffer;
  p += sprintf(p, "%s, ", intro);
  sprintf(p, gs_prompt_grabmana_0, str);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a98c0; original functions/engine.c:1320. */
int autotap_mana_source(int player, int card)
{
  card_instance_t *inst = get_card_instance(player, card);
  if (inst->state & STATE_TAPPED)
    return 0;

  if ((inst->state & STATE_SUMMONSICK_NOTAP) && is_what(player, card, TYPE_CREATURE))
    return 0;

  int old_land_can_be_played = land_can_be_played;
  land_can_be_played |= LCBP_CHARGING_MANA;

  int old_pay_mana[8], old_x_value, old_max_x_value;

  int *old_charge_mana_addr_of_x_value = charge_mana_addr_of_x_value;
  charge_mana_addr_of_x_value = &old_x_value;
  int (*old_charge_mana_addr_of_pay_mana_xbugrw)[8] = charge_mana_addr_of_pay_mana_xbugrw;
  charge_mana_addr_of_pay_mana_xbugrw = &old_pay_mana;
  int *old_charge_mana_addr_of_max_x_value = charge_mana_addr_of_max_x_value;
  charge_mana_addr_of_max_x_value = &old_max_x_value;

  old_x_value = x_value;
  x_value = 0;
  old_max_x_value = max_x_value;
  max_x_value = -1;

  int c;
  for (c = 0; c < 8; ++c)
  {
    old_pay_mana[c] = pay_mana_xbugrwaU[c];
    pay_mana_xbugrwaU[c] = 0;
  }

  if (stack_size < 32)
    put_card_or_activation_onto_stack_impl(player, card, EVENT_RESOLVE_ACTIVATION, player, 0);
  needed_mana_colors = payment_calculate_needed_mana_colors(old_pay_mana);
  tapped_for_mana_color = -1;
  int was_tapped = inst->state & STATE_TAPPED;

  event_activate_then_duplicate_into_stack(player, card, EVENT_ACTIVATE, 1 - player, -1);

  needed_mana_colors = 0;

  int rval;
  if (cancel == 1)
  {
    cancel = 0;
    obliterate_top_card_of_stack();
    rval = 0;
  }
  else
  {
    land_can_be_played &= ~LCBP_CHARGING_MANA;

    if (!was_tapped && (inst->state & STATE_TAPPED))
    {
      dispatch_event(player, card, EVENT_TAP_CARD);
      dispatch_trigger2(player, TRIGGER_TAP_CARD, gs_prompt_specialfephase_7, 1, player, card);
    }
    play_sound_effect(WAV_TAP);
    payment_resolve_top_card_on_stack();

    rval = 1;
  }

  charge_mana_addr_of_x_value = old_charge_mana_addr_of_x_value;
  charge_mana_addr_of_pay_mana_xbugrw = old_charge_mana_addr_of_pay_mana_xbugrw;
  charge_mana_addr_of_max_x_value = old_charge_mana_addr_of_max_x_value;

  x_value = old_x_value;
  max_x_value = old_max_x_value;

  for (c = 0; c < 8; ++c)
    pay_mana_xbugrwaU[c] = old_pay_mana[c];

  land_can_be_played = old_land_can_be_played;

  return rval;
}

#endif

#ifndef SHANDALAR
static int ai_blockers_guts(void)
{
  int *assignment = ai_candidate_block_assignments;
  int *attack_cards = combat_damage_attacker_cards;
  int *num_blocks = ai_attacker_blocker_counts;
  int *blocker_cards = ai_blocker_cards;
  ai_defensive_modifier = 0;
  int life_if_unblocked = life[ai_combat_defending_player];
  int cop_times = defending_cop_available_mana;
  int outflanked[ai_blocker_count + 1];
  memset(outflanked, 0, (ai_blocker_count + 1) * sizeof(int));
  int cop_benefit[16], blockers_that_will_be_destroyed[16];
  int num_cop_benefit = 0, rating = 0, rampage = 0;
  for (int a = 0; a < combat_damage_attacker_count; ++a)
  {
    int attack_power = combat_damage_attacker_damage[a];
    int attack_toughness = combat_damage_attacker_toughness[a];
    int attacker = attack_cards[a];
    ai_defensive_modifier += g_ai_attacker_toughness[a];
    int attacker_flanking =
        check_for_special_ability(1 - ai_combat_defending_player, attacker, 0x2000);
    int num_destroyed = 0, any_blockers_have_banding = 0, any_destroyed = 0;
    int total_pow = 0, total_tgh = 0, num_blocking = 0;
    uint32_t attacker_bit = 1u << (a & 31);
    for (int b = 0; b < ai_blocker_count; ++b)
    {
      if (assignment[b] != attacker)
        continue;
      ++num_blocking;
      if (num_blocking == 2 &&
          check_for_special_ability(1 - ai_combat_defending_player, attacker, 0x40))
        rampage = dispatch_event_with_attacker(1 - ai_combat_defending_player, attacker,
                                               (event_t)0x5e00, -1, 0x40);
      ai_defensive_modifier += g_ai_blocker_power[b];
      int original_tgh = combat_damage_blocker_toughness[b];
      int tgh = original_tgh, flanked = 0;
      if (attacker_flanking && !check_for_special_ability(ai_combat_defending_player,
                                                               blocker_cards[b], 0x2000))
      {
        if (original_tgh < 2)
        {
          outflanked[b] = 1;
          any_destroyed = 1;
          blockers_that_will_be_destroyed[num_destroyed++] = b;
          continue;
        }
        --tgh;
        flanked = 1;
      }
      outflanked[b] = 0;
      int pow = combat_damage_blocker_damage[b] - flanked;
      total_pow += pow > 0 ? pow : 0;
      total_tgh += tgh > 0 ? tgh : 0;
      if (combat_damage_blocker_abilities[b] & 0x40)
        any_blockers_have_banding = 1;
      if ((uint32_t)g_ai_blocker_ability_counts[b] & attacker_bit)
        total_pow += 99;
      int destroys_this = ((uint32_t)g_ai_attacker_ability_counts[a] >> (b & 31)) & 1;
      if (destroys_this)
        any_destroyed = destroys_this;
      else if (!any_destroyed && attack_power < original_tgh)
      {
        any_destroyed = 0;
        continue;
      }
      if (!(combat_damage_blocker_abilities[b] & 0x200))
        blockers_that_will_be_destroyed[num_destroyed++] = b;
    }
    int32_t combinations = 1u << (num_destroyed & 31);
    if (rampage > 0)
    {
      rampage *= num_blocking - 1;
      attack_power += rampage;
      attack_toughness += rampage;
    }
    uint32_t attack_abils = combat_damage_attacker_abilities[a];
    int new_life = life_if_unblocked, evaluate_damage = 0;
    if (num_blocking < minimum_blockers(1 - ai_combat_defending_player, attacker))
    {
      if (!untapped_royal_assassin_count || untapped_royal_assassin_count <= g_ai_blocker_damage[a])
      {
        if (player_bits[1 - ai_combat_defending_player] & 0x80000)
        {
          int commander_damage =
              check_special_flags2(1 - ai_combat_defending_player, attacker, 0x1000000);
          if (!commander_damage)
          {
            int commander = get_commander_id(1 - ai_combat_defending_player, 1);
            if (commander == get_id(1 - ai_combat_defending_player, attacker))
              commander_damage = 1;
            else
            {
              commander = get_commander_id(1 - ai_combat_defending_player, 2);
              if (commander == get_id(1 - ai_combat_defending_player, attacker))
                commander_damage = 1;
            }
          }
          if (commander_damage)
          {
            int edh = locate_edh_card(1 - ai_combat_defending_player);
            if (count_counters(1 - ai_combat_defending_player, edh, (counter_t)0x1c) +
                    attack_power >=
                21)
              new_life -= 9999;
          }
        }
        new_life -= mod_pow_unblocked(attack_power, 1 - ai_combat_defending_player, attacker);
        evaluate_damage = 1;
      }
    }
    else if ((attack_abils & 0x80) && total_tgh < attack_power)
    {
      new_life -= attack_power - total_tgh;
      evaluate_damage = 1;
    }
    if (evaluate_damage)
    {
      if (defending_cop_color_mask && new_life < life_if_unblocked &&
          (defending_cop_color_mask & get_color(1 - ai_combat_defending_player, attacker)))
        cop_benefit[num_cop_benefit++] = life_if_unblocked - new_life;
      life_if_unblocked = new_life;
    }
    if (num_blocking > 0 &&
        ((attack_toughness <= total_pow && (!(attack_abils & 0x300) || num_destroyed < 1)) ||
         (untapped_royal_assassin_count && g_ai_blocker_damage[a] < untapped_royal_assassin_count)))
    {
      int64_t convert_64 = (int64_t)g_ai_attacker_scores[a] *
                           g_ai_combat_value_weights[3 - ai_combat_defending_player];
      rating += (int)(convert_64 / 8);
    }
    int best_opinion = -1;
    int effective_attack_power = any_destroyed ? 99 : attack_power;
    total_pow -= num_blocking;
    for (int32_t combo = 0; combo < combinations; ++combo)
    {
      int remaining = effective_attack_power, attack_rating = 0;
      if (!any_destroyed && any_blockers_have_banding)
      {
        if (total_pow < effective_attack_power)
        {
          attack_rating = combo ? 9999 : 0;
          for (int k = 0; k < num_destroyed; ++k)
          {
            if (!(combo & (1u << (k & 31))))
              continue;
            int n = blockers_that_will_be_destroyed[k];
            if (combat_damage_blocker_toughness[n] <= effective_attack_power)
            {
              int value = g_ai_combat_candidate_cards[n];
              remaining = is_protected_from(ai_combat_defending_player, blocker_cards[n],
                                                 1 - ai_combat_defending_player, attacker,
                                                 IPF_DAMAGE);
              if (!remaining)
              {
                if (value < attack_rating)
                  attack_rating = value;
                break;
              }
            }
            attack_rating = 0;
            remaining = effective_attack_power;
          }
        }
      }
      else
      {
        for (int l = 0; l < num_destroyed; ++l)
        {
          if (!(combo & (1u << (l & 31))))
            continue;
          int n = blockers_that_will_be_destroyed[l];
          if (!(combat_damage_blocker_abilities[n] & 0x100) ||
              combat_damage_blocker_damage[n] < attack_toughness)
          {
            if (outflanked[n] || !is_protected_from(
                                     ai_combat_defending_player, blocker_cards[n],
                                     1 - ai_combat_defending_player, attacker, IPF_DAMAGE))
              attack_rating += g_ai_combat_candidate_cards[n];
          }
          remaining -= combat_damage_blocker_toughness[n];
          if (remaining < 0)
            break;
        }
      }
      if (remaining >= 0 && best_opinion < attack_rating)
        best_opinion = attack_rating;
    }
    rating -= best_opinion;
  }
  rating += ai_defensive_modifier;
  while (cop_times != 0 && num_cop_benefit > 0)
  {
    int best = 0, best_idx = 0;
    for (int i = 0; i < num_cop_benefit; ++i)
    {
      if (best < cop_benefit[i])
      {
        best = cop_benefit[i];
        best_idx = i;
      }
    }
    if (!best)
      break;
    --cop_times;
    life_if_unblocked += cop_benefit[best_idx];
    cop_benefit[best_idx] = 0;
  }
  int penalty;
  if (life_if_unblocked < 1)
    penalty = life_if_unblocked * -96 + 999;
  else
  {
    int64_t rating_64 = (int64_t)(life[ai_combat_defending_player] - life_if_unblocked) *
                        g_ai_combat_value_weights[ai_combat_defending_player];
    penalty = (int)((rating_64 * 6) / life_if_unblocked);
  }
  return rating - penalty;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ab730; original functions/engine.c:2152. */
int can_attack(int player, int card)
{

  card_instance_t *instance = get_card_instance(player, card);

  if (instance->internal_card_id >= 0 &&
      (!(instance->regen_status & KEYWORD_DEFENDER) ||
       (instance->token_status & STATUS_WALL_CAN_ATTACK)) &&
      (is_what(player, card, TYPE_CREATURE) || (instance->state & STATE_NONCREATURE_CAN_ATTACK)) &&
      !(instance->state & (STATE_OUBLIETTED | STATE_SUMMONSICK_NOATTACK | STATE_TAPPED)) &&
      !(instance->token_status & STATUS_CANT_ATTACK))
  {
    int old_cancel = cancel;
    int old_event_result = event_result;
    int old_affected_card_controller = affected_card_controller;
    int old_affected_card = affected_card;

    event_result = 0;
    affected_card_controller = player;
    affected_card = card;

    call_card_function_i(instance, player, card, EVENT_ATTACK_LEGALITY);
    int new_event_result = event_result;

    affected_card = old_affected_card;
    affected_card_controller = old_affected_card_controller;
    event_result = old_event_result;
    cancel = old_cancel;

    if (new_event_result)
      return 0;

    if (player != AI || (trace_mode & 2) ||
        !check_attack_legality_if_enchanting_affected_card(player, card))
      return ((!(player_bits[1 - player] & PB_OPPONENT_CHECKS_ATTACK_LEGALITY) &&
               !(event_flags & EA_DECLARE_ATTACK)) ||
              !dispatch_event(player, card, EVENT_ATTACK_LEGALITY));
  }

  return 0;
}

#endif

#ifndef SHANDALAR
static int any_can_block(int player)
{
  int defender = 1 - player;
  for (int attacker = 0; attacker < active_cards_count[player]; ++attacker)
  {
    if (!(get_card_instance(player, attacker)->state & STATE_ATTACKING))
      continue;
    for (int blocker = 0; blocker < active_cards_count[defender]; ++blocker)
    {
      card_instance_t *inst = get_card_instance(defender, blocker);
      uint32_t mask = 10 | ((card_flags[defender] & CFLAGS_MASAKO) ? 0 : 0x10);
      if ((inst->state & mask) == 2 && is_legal_block(defender, blocker, player, attacker))
        return 1;
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
static void check_pow_tgh_boost(iid_t iid, int player, int card, int *pow_loc, int *tgh_loc)
{
  uint32_t ea = cards_data[iid].extra_ability;
  CardFunction fn = cards_data[iid].code_pointer;
  card_instance_t *inst = get_card_instance(player, card);
  int base_power = *pow_loc, base_toughness = *tgh_loc;
  if (ea & 0x18)
  {
    int8_t old_activating_player = g_card_on_stack_controller;
    g_card_on_stack_controller = player;
    int swapped = check_special_flags3(player, card, 0x100000);
    if (!swapped)
    {
      if (ea & 8)
        *pow_loc = base_power + call_card_fn(fn, inst, player, card, EVENT_POW_BOOST);
      if (ea & 0x10)
        *tgh_loc = base_toughness + call_card_fn(fn, inst, player, card, EVENT_TOU_BOOST);
    }
    else
    {
      if (ea & 8)
        *tgh_loc = base_toughness + call_card_fn(fn, inst, player, card, EVENT_POW_BOOST);
      if (ea & 0x10)
        *pow_loc = base_power + call_card_fn(fn, inst, player, card, EVENT_TOU_BOOST);
    }
    g_card_on_stack_controller = old_activating_player;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26b0680; original functions/engine.c:3991. */
int card_instances_should_be_displayed_identically(card_instance_t *inst1,
                                                        card_instance_t *inst2)
{

  return (inst1 == inst2 ||
          !(inst1->internal_card_id != inst2->internal_card_id ||
            ((inst1->state ^ inst2->state) &
             (STATE_SUMMONSICK_BOTH | STATE_TAPPED | STATE_OWNED_BY_OPPONENT | STATE_TARGETTED |
              STATE_CANNOT_TARGET | STATE_OUBLIETTED | STATE_IS_TRIGGERING)) ||
            inst1->damage_on_card != inst2->damage_on_card || inst1->power != inst2->power ||
            inst1->toughness != inst2->toughness || inst1->mana_color != inst2->mana_color ||
            inst1->color != inst2->color || inst1->blocking != inst2->blocking ||
            inst1->info_slot != inst2->info_slot ||
            inst1->special_counters_byte != inst2->special_counters_byte ||
            inst1->counters2 != inst2->counters2 || inst1->counters3 != inst2->counters3 ||
            inst1->counters4 != inst2->counters4 || inst1->unknown0x11a != inst2->unknown0x11a ||
            inst1->counters_byte != inst2->counters_byte || inst1->counters5 != inst2->counters5 ||
            inst1->unknown0x122 != inst2->unknown0x122 ||
            ((inst1->token_status ^ inst2->token_status) & STATUS_RED_BORDER) ||
            inst1->display_pic_csv_id != inst2->display_pic_csv_id ||
            inst1->display_pic_num != inst2->display_pic_num ||
            inst1->untap_status != inst2->untap_status || inst1->kill_code != inst2->kill_code ||
            inst1->counters_m1m1 != inst2->counters_m1m1 ||
            inst1->targets[18].player != inst2->targets[18].player ||
            inst1->targets[18].card != inst2->targets[18].card));
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26b07b0; original functions/engine.c:4026. */
int determine_start_duel_winner(void)
{

  int lost0 = life[0] <= 0 || POISON_COUNTERS(0) >= 10;
  int lost1 = life[1] <= 0 || POISON_COUNTERS(1) >= 10;

  if (!lost0 && lost1)
    return 1;
  else if (!lost1 && lost0)
    return 0;
  else
    return -1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26b0830; original functions/engine.c:4041. */
int is_anyone_dead(void)
{
  if (life[0] <= 0 || life[1] <= 0 || POISON_COUNTERS(0) >= 10 || POISON_COUNTERS(1) >= 10)
  {
    if (ai_is_speculating != 1)
    {
      TENTATIVE_reassess_all_cards();
      return 1;
    }
    else
    {
      if (POISON_COUNTERS(0) >= 10)
        life[0] = -99;

      if (POISON_COUNTERS(1) >= 10)
        life[1] = -99;
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 private inline definitions. See per-helper limits in the recovery evidence. */
/* ManalinkEh 2022: 0x26b08d0; original functions/engine.c:256. */
int untap_phase(int player)
{


  memset(your_spells_cant_be_countered_by_color, 0, sizeof(your_spells_cant_be_countered_by_color));

  player_bits[0] &= ~PB_SEND_EVENT_UNTAP_CARD_TO_ALL;
  player_bits[1] &= ~PB_SEND_EVENT_UNTAP_CARD_TO_ALL;
  event_flags &= ~EF_ATTACKER_CHOOSES_BLOCKERS;
  dispatch_event(player, 0, EVENT_BEGIN_TURN);

  if (trace_mode & 2)
  {
    char buf[200];
    sprintf(buf, "%d: Entering Untap Phase.\n", ++g_duel_trace_counter);
    append_to_trace_txt(buf);
  }

  card_instance_t *instance;

  int shimmering_lands = 0;
  int curse_flag = 0;
  int skip = 0;
  int p, c;
  for (p = 0; p < 2; ++p)
    for (c = 0; c < active_cards_count[p]; ++c)
      if ((instance = in_play(p, c)))
      {
        if (p == player)
          instance->state &= ~STATE_SUMMON_SICK_BACKUP;
        if (instance->internal_card_id == LEGACY_EFFECT_CUSTOM &&
            instance->info_slot == (int)target_player_skips_untap &&
            instance->targets[0].player == player && !skip)
        {
          kill_card(p, c, KILL_EXILE);
          skip = 1;
        }
        else if (cards_data[instance->internal_card_id].id == CARD_ID_SHIMMER)
        {
          shimmering_lands |= instance->info_slot;
        }
        else if (p == 1 - player &&
                 (cards_data[instance->internal_card_id].id == CARD_ID_TEFERIS_CURSE ||
                  cards_data[instance->internal_card_id].id == CARD_ID_CLOAK_OF_INVISIBILITY) &&
                 instance->damage_target_player == player)
          curse_flag = 1;
      }

  if (skip || check_battlefield_for_id(2, CARD_ID_STASIS) ||
      check_battlefield_for_id(2, CARD_ID_THE_EON_FOG) ||
      check_battlefield_for_id(2, CARD_ID_SANDS_OF_TIME))
    return 0;

  untap_phasing(player, shimmering_lands, curse_flag);

  untap_phase_exe(player);

  int untapped[151] = {0};
  for (c = 0; c < active_cards_count[player]; ++c)
    if ((instance = in_play(player, c)))
    {
      if ((instance->untap_status & 3) == 3)
      {

        untapped[c] = 1;

        instance->state &= ~STATE_TAPPED;
      }

      SET_BYTE0(instance->untap_status) = 0;
    }

  count_colors_of_lands_in_play();
  count_mana();

  for (c = 0; c < active_cards_count[player]; ++c)
    if (untapped[c])
    {
      if (player_bits[player] & PB_SEND_EVENT_UNTAP_CARD_TO_ALL)
        dispatch_event(player, c, EVENT_UNTAP_CARD);
      else
        dispatch_event_with_attacker_to_one_card(player, c, EVENT_UNTAP_CARD, 1 - player, -1);
    }

  recalculate_all_cards_in_play();
  TENTATIVE_reassess_all_cards();

  if (!g_phase_was_skipped)
    prompt_stop_phase_anyway(1);

  dispatch_event(player, 0, EVENT_END_OF_UNTAP_STEP);

  resolve_mana_burn();

  return is_anyone_dead();
}

/* ManalinkEh 2022: 0x26b0ca0; original functions/engine.c:367. */
int upkeep_phase(int player)
{

  if (trace_mode & 2)
  {
    char str[100];
    sprintf(str, "%d: Entering UpKeep Phase.\n", g_duel_trace_counter++);
    append_to_trace_txt(str);
  }

  g_ai_speculation_depth = 0;
  g_phase_was_skipped = 0;
  g_phase_response_window_open = 0;

  if (trace_mode & 2)
  {
    if (g_stop_phase == -1)
      g_phase_stop_suppressed = 0;
    else
      g_phase_stop_suppressed = ((g_stop_phase != 4 || g_stop_phase_player != player) &&
                             (g_previous_stop_phase != 4 || g_previous_stop_phase_player != player));
  }
  else if (ai_is_speculating != 1)
    g_phase_stop_suppressed =
        (g_stop_phase != -1 && (g_stop_phase != 4 || g_stop_phase_player != player));

  #ifndef MANALINKEH_STATICif (ai_is_speculating != 1 && !(trace_mode & 2))
  {
    TENTATIVE_reassess_all_cards();
    start_ai_decision_search(17, 10);
  }
speculate_more:
  if (g_ai_decision_code == 17)
  {

    reset_ai_search_trial_state();
    g_duel_state_008a8de4 = 0;
    ai_modifier = 0;
  }

  #endif
current_phase = PHASE_BEGIN_UPKEEP;
  phase_changed(player, PHASE_BEGIN_UPKEEP);
  dispatch_trigger2(player, TRIGGER_UPKEEP, gs_prompt_specialfephase_8, 0, 0, 0);

  g_phase_response_window_open = 1;
  current_phase = PHASE_UPKEEP;
  allow_response(-1, current_phase, PROMPT_CHECKFEPHASE_5,
                                                     EVENT_UPKEEP_PHASE);
  g_phase_response_window_open = 0;

  dispatch_trigger2(player, TRIGGER_END_UPKEEP, gs_prompt_specialfephase_9, 0, 0, 0);

  g_phase_stop_suppressed = 0;
  g_response_processing_flags = 1;
  process_damage_prevention();

  g_response_processing_flags = 0;
  g_ai_speculation_depth = 0;
  mana_burn();

  #ifndef MANALINKEH_STATIC
  int phase_code_to_go_to, becomes_second_arg_of_main_phase_and_discard_phase,
      becomes_third_arg_of_main_phase;
  hack_ai_decision_phase_upkeep = 1;
  ai_decision_phase(player, &phase_code_to_go_to,
                    &becomes_second_arg_of_main_phase_and_discard_phase,
                    &becomes_third_arg_of_main_phase);
  if (phase_code_to_go_to == 4)
    goto speculate_more;

  #endif
return is_anyone_dead();
}

/* ManalinkEh 2022: 0x26b0f60; original functions/engine.c:4064. */
int sub_495BE0(int plr)
{
  int num_per_col[COLOR_WHITE + 1] = {0};
  int i;

  if (plr == -1)
  {
    for (i = 0; i < 500; ++i)
      if (Deck[i] != -1 && !(Deck[i] & 0x88000))
      {
        color_test_t cols = cards_data[Deck[i] & 0x7FFF].color;
        if (cols & COLOR_TEST_BLACK)
          ++num_per_col[COLOR_BLACK];
        if (cols & COLOR_TEST_WHITE)
          ++num_per_col[COLOR_WHITE];
        if (cols & COLOR_TEST_GREEN)
          ++num_per_col[COLOR_GREEN];
        if (cols & COLOR_TEST_RED)
          ++num_per_col[COLOR_RED];
        if (cols & COLOR_TEST_BLUE)
          ++num_per_col[COLOR_BLUE];
      }
  }
  else
  {
    for (i = 0; i < 200; ++i)
    {
      csvid_t csvid = initial_library[plr][i].csvid;
      int num = initial_library[plr][i].numcards;
      if (csvid != -1 && num > 0)
      {
        color_test_t cols = cards_data[get_internal_card_id_from_csv_id(csvid)].color;
        if (cols & COLOR_TEST_BLACK)
          num_per_col[COLOR_BLACK] += num;
        if (cols & COLOR_TEST_WHITE)
          num_per_col[COLOR_WHITE] += num;
        if (cols & COLOR_TEST_GREEN)
          num_per_col[COLOR_GREEN] += num;
        if (cols & COLOR_TEST_RED)
          num_per_col[COLOR_RED] += num;
        if (cols & COLOR_TEST_BLUE)
          num_per_col[COLOR_BLUE] += num;
      }
    }
  }

  int highest_idx = 0, highest_num = -1;
  for (i = COLOR_BLACK; i <= COLOR_WHITE; ++i)
    if (num_per_col[i] > highest_num)
    {
      highest_idx = i;
      highest_num = num_per_col[i];
    }

  return highest_idx;
}
#endif
