#include "manalink.h"

void memcpy_suppress_size_warning(void *dest, const void *src, size_t size);

/* Canonical recovered helper declarations. */
int has_no_mana_cost(int player, int card);
int copy_spell_from_stack(int player, int t_player, int t_card);
int copy_spell(int player, int csvid);
int effect_may_play_card_from_exile(int player, int card, event_t event);
int create_may_play_card_from_exile_effect(int player, int card, int whose_exile_zone,
                                                int csvid, mpcfe_mode_t mode);
static int real_copy_spell(int player, int internal_id, int special_flags, int special_flags3,
                                card_instance_t *copied, target_t *forced_target);
static int free_spell_card = -1;
static int free_spell_player = -1;

int played_for_free(int player, int card){
	if( player > -1 && card > -1 ){
		return check_special_flags(player, card, SF_PLAYED_FOR_FREE);
	}
	return 0;
}

int not_played_from_hand(int player, int card){
	if( check_special_flags(player, card, SF_NOT_CAST | SF_PLAYED_FROM_GRAVE | SF_PLAYED_FROM_DECK | SF_PLAYED_FROM_EXILE) ){
		return 1;
	}
	return 0;
}

target_t totally_bletcherous_hack_dont_reset_x_value = {-1, -1};
static int totally_bletcherous_hack_put_card_on_stack0_without_resetting_x_value(int player, int card)
{
  /* This circumlocution is needed because put_card_on_stack() clears 0x78676C itself; fortunately, a function we have control over is called before its value
   * is used.  Yet another consequence of always forcing these to be free. */
  target_t old_hack_val = totally_bletcherous_hack_dont_reset_x_value;	// struct copy
  totally_bletcherous_hack_dont_reset_x_value.player = player;
  totally_bletcherous_hack_dont_reset_x_value.card = card;
  int rval = put_card_on_stack(player, card, 0);
  totally_bletcherous_hack_dont_reset_x_value = old_hack_val;	// struct copy
  g_adventure_world_state = 0;
  return rval;
}

/* Shared in October 2022 (COFF external); also used by newer sets. */
int real_play_spell_for_free(int player, int card, int special_flags)
{
  free_spell_player = player;
  free_spell_card = card;
  int internal_id = get_card_instance(player, card)->internal_card_id;
  card_data_t *card_d = &cards_data[internal_id];
  set_special_flags(player, card, SF_PLAYED_FOR_FREE | special_flags);

  if (totally_bletcherous_hack_put_card_on_stack0_without_resetting_x_value(player, card))
  {
    put_card_on_stack(player, card, 1);
    put_card_on_stack3(player, card);
    if (card_d->type & TYPE_LAND)
      land_can_be_played |= LCBP_LAND_HAS_BEEN_PLAYED;
  }
  else if (can_legally_play_iid(player, internal_id) ||
           (card_d->cc[2] == 24 && (special_flags & SF_FLASHBACK)))
  {
    --hand_count[player];
    real_put_into_play(player, card);
    dispatch_trigger2(current_turn, TRIGGER_SPELL_CAST, gs_prompt_specialfephase_6, 0, player, card);
    card_on_stack = card;
    card_on_stack_controller = player;
    if (card_d->type & TYPE_LAND)
      land_can_be_played |= LCBP_LAND_HAS_BEEN_PLAYED;
    else
    {
      int mode = 0;
      if ((card_d->type & TYPE_CREATURE) && player == current_turn)
        mode = STORM_CREATURE;
      increment_storm_count(player, mode);
    }
  }
  else
  {
    remove_special_flags(player, card, SF_PLAYED_FOR_FREE | special_flags);
    free_spell_card = -1;
    return 0;
  }

  if (!check_special_flags2(player, card, SF2_COMMANDER))
  {
    int commander_code = 1;
    if (get_commander_id(player, 1) != internal_id)
    {
      if (get_commander_id(player, 2) != internal_id)
      {
        free_spell_card = -1;
        return 1;
      }
      commander_code = 2;
    }
    commander_enters_the_battlefield(player, locate_edh_card(player), player, card, commander_code);
  }
  free_spell_card = -1;
  return 1;
}

int play_card_in_deck_for_free(int player, int t_player, int deck_pos){
	int *deck = deck_ptr[t_player];
	int card_added = -1;
	if( deck[deck_pos] != -1 ){
		int iid = deck[deck_pos];
		if( can_legally_play_iid(player, iid) ){
			remove_card_from_deck(t_player, deck_pos);
			card_added = add_card_to_hand(player, iid);
			if (player != t_player){
				get_card_instance(player, card_added)->state ^= STATE_OWNED_BY_OPPONENT;
			}
			if( is_what(player, card_added, TYPE_LAND) ){
				set_special_flags3(player, card_added, SF3_EMULATING_LANDS_PLAYED);
			}
			if ( ! real_play_spell_for_free(player, card_added, SF_PLAYED_FROM_DECK) ){
				obliterate_card(player, card_added);
				card_added = -1;
			}
		}
	}
	return card_added;
}

int fake_play_card_in_zone_for_free(int player, int t_player, int iid, int special_flags){
// See Talent of the Telepath

	int card_added = -1;
	if( can_legally_play_iid(player, iid) ){
		card_added = add_card_to_hand(player, iid);
		if (player != t_player){
			get_card_instance(player, card_added)->state ^= STATE_OWNED_BY_OPPONENT;
		}
		if ( ! real_play_spell_for_free(player, card_added, special_flags) ){
			obliterate_card(player, card_added);
			card_added = -1;
		}
	}
	return card_added;
}

static int play_card_in_grave_for_free_impl(int player, int t_player, int g_pos, int special_flags)
{
  const int *grave = get_grave(t_player);
  int card_added = -1;
  if (grave[g_pos] != -1)
  {
    int iid = grave[g_pos];
    if (can_legally_play_iid(player, iid) ||
        (cards_data[iid].cc[2] == 24 && (special_flags & SF_FLASHBACK)))
    {
      remove_card_from_grave(t_player, g_pos);
      card_added = add_card_to_hand(player, iid);
      if (is_what(player, card_added, TYPE_LAND))
      {
        set_special_flags3(player, card_added, SF3_EMULATING_LANDS_PLAYED);
      }
      if (player != t_player)
      {
        get_card_instance(player, card_added)->state ^= STATE_OWNED_BY_OPPONENT;
      }
      if (!real_play_spell_for_free(player, card_added, (special_flags | SF_PLAYED_FROM_GRAVE)))
      {
        obliterate_card(player, card_added);
        card_added = -1;
      }
    }
  }
  return card_added;
}

int play_card_in_grave_for_free(int player, int t_player, int g_pos){
	return play_card_in_grave_for_free_impl(player, t_player, g_pos, 0);
}

int play_card_in_grave_for_free_and_exile_it(int player, int t_player, int g_pos){
	return play_card_in_grave_for_free_impl(player, t_player, g_pos, SF_EXILE_ON_RESOLUTION);
}

int flashback_spell(int player, int g_pos){
	return play_card_in_grave_for_free_impl(player, player, g_pos, SF_EXILE_ON_RESOLUTION | SF_FLASHBACK);
}

/* ManalinkEh 2022: 0x2669160; original functions/play_spell_for_free.c:277. */
int escape_spell(int player, int g_pos)
{
  int escaped_iid_or_card;

  escaped_iid_or_card = graveyard_ptr_mutable[player][g_pos];
  if ((escaped_iid_or_card != -1) && (can_legally_play_iid(player, escaped_iid_or_card) != 0))
  {
    remove_card_from_grave(player, g_pos);
    escaped_iid_or_card = add_card_to_hand(player, escaped_iid_or_card);
    set_special_flags3(player, escaped_iid_or_card, 0x80000);
    if (is_what(player, escaped_iid_or_card, 1) != 0)
    {
      set_special_flags3(player, escaped_iid_or_card, 0x20000);
    }
    if (real_play_spell_for_free(player, escaped_iid_or_card, 0x10000000) != 0)
    {
      return escaped_iid_or_card;
    }
    obliterate_card(player, escaped_iid_or_card);
  }
  return -1;
}

int play_card_in_exile_for_free_impl(int player, int t_player, int csvid, int special_flags2, int mana_paid_for_this){
	int card_added = -1;
	int iid = get_internal_card_id_from_csv_id(csvid);
	if( can_legally_play_iid(player, iid) ){
		remove_card_from_rfg(t_player, csvid);
		card_added = add_card_to_hand(player, iid);
		get_card_instance(player, card_added)->targets[3].card = mana_paid_for_this;
		set_special_flags2(player, card_added, special_flags2);
		if (player != t_player){
			if( player == HUMAN && t_player == AI ){
				add_state(player, card_added, STATE_OWNED_BY_OPPONENT);
			}
			else{
				remove_state(player, card_added, STATE_OWNED_BY_OPPONENT);
			}
		}
		if (!real_play_spell_for_free(player, card_added, SF_PLAYED_FROM_EXILE)){
			obliterate_card(player, card_added);
			card_added = -1;
		}
	}
	return card_added;
}

// Needed for counterspells to correctly allow the Commander to be putted into the Command Zone and to cards that need to know how many mana was paid to play them
int play_card_in_exile_for_free(int player, int t_player, int csvid)
{
  return play_card_in_exile_for_free_impl(player, t_player, csvid, 0, -1);
}

int play_card_in_exile_for_free_commander(int player, int t_player, int csvid,
                                          int mana_paid_for_this)
{
  int iid = get_internal_card_id_from_csv_id(csvid);
  if (!can_legally_play_iid(player, iid))
    return -1;

  int card_added = add_card_to_hand(player, iid);
  get_card_instance(player, card_added)->targets[3].card = mana_paid_for_this;
  set_special_flags2(player, card_added, SF2_COMMANDER);
  if (player == HUMAN && t_player == AI)
    add_state(player, card_added, STATE_OWNED_BY_OPPONENT);
  else
    remove_state(player, card_added, STATE_OWNED_BY_OPPONENT);

  if (!real_play_spell_for_free(player, card_added, 0))
  {
    obliterate_card(player, card_added);
    return -1;
  }
  return card_added;
}

void play_card_in_hand_for_free(int player, int card){
	real_play_spell_for_free(player, card, 0);
}

void play_card_in_hand_for_free_miracle(int player, int card){
	set_special_flags2(player, card, SF2_MIRACLE);
	if( ! real_play_spell_for_free(player, card, 0) ){
		remove_special_flags2(player, card, SF2_MIRACLE);
	}
}

void play_card_in_hand_madness(int player, int card){
	//We aren't really playing a card from exile, as per Madness ruling, just faking it.
	real_play_spell_for_free(player, card, SF_PLAYED_FROM_EXILE);
}

void opponent_plays_card_in_your_hand_for_free(int player, int card){
	int iid = get_card_instance(player, card)->internal_card_id;
	obliterate_card(player, card);
	int card_added = add_card_to_hand(1-player, iid);
	if( player == AI ){
		add_state(1-player, card_added, STATE_OWNED_BY_OPPONENT);
	}
	else{
		remove_state(1-player, card_added, STATE_OWNED_BY_OPPONENT);
	}
	real_play_spell_for_free(1-player, card_added, 0);
}

void memset_suppress_size_warning(void* addr, int val, size_t sz);

int put_into_play(int player, int card)
{
  if (player < 0 || card < 0)
    return -1;

  set_special_flags(player, card, SF_NOT_CAST | SF_PLAYED_FOR_FREE);
  if (in_hand(player, card))
    --hand_count[player];

  int good = 1;
  if (has_subtype(player, card, SUBTYPE_AURA))
  {
    good = call_card_function(player, card, EVENT_CAN_CAST);
    if (good)
      set_special_flags3(player, card, SF3_IN_PLAY_DIRECTLY);
  }
  if (good)
    real_put_into_play(player, card);
  remove_special_flags3(player, card, SF3_IN_PLAY_DIRECTLY);
  return card;
}

// If MPCFE_FACE_DOWN is set, the card should be obliterated instead of being put in to exile; otherwise, it should already be in exile before calling this.

int compute_and_check_casting_cost(int player, int unused, int card)
{
  // 200bb14
  // 0x402930.  Parameter unused is always the same as player when called from the exe.

  int result = 0;

  int old_affected_card = affected_card;
  int old_affected_card_controller = affected_card_controller;

  if (player == -1 || card == -1)
    goto epilogue;

  int8_t *cost_xbugrwaU = (int8_t *)&COST_COLORLESS;
  int8_t *surplus_free_mana = (int8_t *)byte_55CF20;

  memset_suppress_size_warning(cost_xbugrwaU, 0, 8);
  memset(surplus_free_mana, 0, 8); // Amount of surplus free mana?

  card_instance_t *instance = get_card_instance(player, card);
  affected_card_controller = player;
  affected_card = card;

  int iid = instance->internal_card_id;
  if (iid < 0)
    goto epilogue;

  int free_spell = card == free_spell_card && player == free_spell_player;

  card_instance_t *inst;
  int p, c;
  for (p = 0; p <= 1; ++p)
    for (c = 0; c < active_cards_count[p]; ++c)
      if ((inst = in_play(p, c)))
      {
        card_data_t *cd = &cards_data[inst->internal_card_id];
        if (cd->extra_ability & EA_PLAY_COST)
          call_card_fn(cd->code_pointer, inst, p, c, EVENT_MODIFY_COST_GLOBAL);
      }

  card_data_t *cd = &cards_data[iid];
  if (cd->new_field & 1) // "Modifies Casting Cost" column in ct_all.csv
    call_card_fn(cd->code_pointer, instance, player, card, EVENT_MODIFY_COST);

  card_ptr_t *cp = cards_ptr[cd->id];

  if (!free_spell)
  {
    int cless = cp->req.req_colorless;
    if (cless == 40)
      cless = 0;
    cost_xbugrwaU[0] += cless;
  }
  if (cost_xbugrwaU[0] < 0)
  {
    surplus_free_mana[0] = cost_xbugrwaU[0];
    cost_xbugrwaU[0] = 0;
  }

#define COMPUTE_COLOR(col, req_col)                                                                \
  if (!free_spell)                                                                                 \
    cost_xbugrwaU[col] += cp->req.req_col;                                                             \
  if (cost_xbugrwaU[col] < 0)                                                                      \
  {                                                                                                \
    surplus_free_mana[col] = cost_xbugrwaU[col];                                                         \
    cost_xbugrwaU[col] = 0;                                                                        \
  }

  COMPUTE_COLOR(COLOR_BLACK, req_black);
  COMPUTE_COLOR(COLOR_BLUE, req_blue);
  COMPUTE_COLOR(COLOR_GREEN, req_green);
  COMPUTE_COLOR(COLOR_RED, req_red);
  COMPUTE_COLOR(COLOR_WHITE, req_white);
#undef COMPUTE_COLOR

  cost_xbugrwaU[COLOR_ARTIFACT] = 0;

  if (cd->type & TYPE_ARTIFACT)
    result = has_mana_multi_a(player, cost_xbugrwaU[COLOR_COLORLESS], cost_xbugrwaU[COLOR_BLACK],
                              cost_xbugrwaU[COLOR_BLUE], cost_xbugrwaU[COLOR_GREEN],
                              cost_xbugrwaU[COLOR_RED], cost_xbugrwaU[COLOR_WHITE]);
  else
  {
    if ((card_flags[player] & CFLAGS_KRRIK) && COST_BLACK > 0)
    {
      int8_t old_black = COST_BLACK;
      COST_BLACK -= how_much_life_can_i_pay(player) / 2;
      if (COST_BLACK < 0)
        COST_BLACK = 0;
      memcpy_suppress_size_warning(&MANA_COLORLESS, cost_xbugrwaU, 8);
      result = check_mana_multi(player);
      COST_BLACK = old_black;
    }
    else
    {
      memcpy_suppress_size_warning(&MANA_COLORLESS, cost_xbugrwaU, 8);
      result = check_mana_multi(player);
    }
  }

epilogue:
  if (TENTATIVE_next_spell_always_castable ==
      1) // TENTATIVE_next_spell_always_castable - set only by card_chain_lightning()
  {
    TENTATIVE_next_spell_always_castable = 0;
    result = 1;
  }

  affected_card_controller = old_affected_card_controller;
  affected_card = old_affected_card;

  return result;
}

static int spell_has_flashback_legacy(int player, int card, event_t event)
{
  /* targets:
   * [0].card: iid of chosen card
   * [1].player: graveyard_source value
   * [1].card: tentative position in graveyard
   * [2].player: flags. If it's left to "-1", it just acts like a normal flashback
						1<<0 --> can play the card without paying its mana cost (Sins of the Past)
   */

  if (event == EVENT_CLEANUP
	  || (event == EVENT_STATIC_EFFECTS && get_card_instance(player, card)->targets[1].card == -1))	// no longer in graveyard
	kill_card(player, card, KILL_EXILE);

  if (!IS_ACTIVATING(event))
	return 0;

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CAN_ACTIVATE)
	{
	  instance->targets[1].card = find_in_graveyard_by_source(player, instance->targets[1].player, instance->targets[1].card);

		if (instance->targets[1].card != -1	// must still be in graveyard
			&& ((instance->targets[2].player & FBL_NO_MANA_COST)
				|| (has_mana_to_cast_iid(player, event, instance->targets[0].card) && ! has_no_mana_cost(-1, instance->targets[0].card) ))
		  ){
			int rval = can_legally_play_iid(player, instance->targets[0].card);
			if( rval == 99 && (cards_data[instance->targets[0].card].type & (TYPE_INSTANT | TYPE_INTERRUPT)) ){
				return 99;
			}
			if( rval && !(cards_data[instance->targets[0].card].type & (TYPE_INSTANT | TYPE_INTERRUPT)) && ! can_sorcery_be_played(player, event) ){
				rval = 0;
			}
			return rval;
		}
	}

  if (event == EVENT_ACTIVATE
	  && ((instance->targets[2].player & FBL_NO_MANA_COST)
		  || charge_mana_from_id(player, -1, event, cards_data[instance->targets[0].card].id)))
	{
		kill_card(player, card, KILL_EXILE);
		if( !(instance->targets[2].player & FBL_PLAYING_PERMANENT_CARD) )
			play_card_in_grave_for_free_and_exile_it(player, player, instance->targets[1].card);
		else
			play_card_in_grave_for_free(player, player, instance->targets[1].card);
		cant_be_responded_to = 1;	// already had opportunity to respond to the spell; no response to this fake activation
	}

  return 0;
}

void create_spell_has_flashback_legacy(int player, int card, int graveyard_pos,
                                       flashback_legacy_t mode)
{
  if (graveyard_pos < 0)
    return;

  int leg = create_legacy_activate(player, card, &spell_has_flashback_legacy);
  card_instance_t *legacy = get_card_instance(player, leg);
  legacy->targets[0].card = get_grave(player)[graveyard_pos];
  legacy->targets[1].player = graveyard_source[player][graveyard_pos];
  legacy->targets[1].card = graveyard_pos;
  legacy->targets[2].player = mode;
  create_card_name_legacy(player, leg, cards_data[get_grave(player)[graveyard_pos]].id);
}

int can_activate_to_play_cards_from_graveyard(int player, int card, event_t event, type_t types){

	// Activation
	if (event == EVENT_CAN_ACTIVATE){
		const int* grave = get_grave(player);
		int i, rval, result = 0;
		for (i = 0; grave[i] != -1; ++i){
			if ((cards_data[grave[i]].type & types) &&	// deliberately not is_what()
				has_mana_to_cast_iid(player, event, grave[i]) &&
				! is_suspend_only_card(-1, grave[i]) &&
				(rval = can_legally_play_iid_now(player, grave[i], event))
			   ){
				if (rval == 99){
					return 99;
				}
				result = rval;
			}
		}
		return result;
	}

	if (event == EVENT_ACTIVATE){
		const int* grave = get_grave(player);
		int iids[500], positions[500];
		int i, num_cards = 0;
		for (i = 0; grave[i] != -1; ++i){
			if ((cards_data[grave[i]].type & types) &&	// deliberately not is_what()
				has_mana_to_cast_iid(player, event, grave[i]) &&
				! is_suspend_only_card(-1, grave[i]) &&
				can_legally_play_iid_now(player, grave[i], event)
			   ){
				iids[num_cards] = grave[i];
				positions[num_cards] = i;
				++num_cards;
			}
		}

		test_definition_t test;
		default_test_definition(&test, 0);
		int selected = select_card_from_zone(player, player, iids, num_cards, 0, AI_MAX_VALUE, -1, &test);
		if (selected != -1 && charge_mana_from_id(player, -1, event, cards_data[iids[selected]].id)){
			play_card_in_grave_for_free(player, player, positions[selected]);
			cant_be_responded_to = 1;	// Response allowed above, just none from this activation
		} else {
			cancel = 1;
		}
	}

	return 0;
}

int real_put_into_play(int player, int card)
{
  // 0x4b5d90

  if (player < 0 || card < 0
	  || player > 1 || card >= 150)
	return 0;

  card_instance_t* inst = get_card_instance(player, card);
  int csvid = cards_data[inst->internal_card_id].id;
  if ((csvid == CARD_ID_VESUVAN_DOPPELGANGER || csvid == 39 /* original clone */)
	  && !dispatch_event_to_single_card(player, card, EVENT_CAN_CAST, 1 - player, -1))
	{
	  kill_card(player, card, KILL_DESTROY);
	  return 0;
	}

  // Use raw type here, not interpreted through get_type() or is_what(), since the other things that maintain these counts also use raw type
  type_t typ = cards_data[inst->internal_card_id].type;
  if (typ & TYPE_CREATURE)
	++creature_count[player];
  if (typ & TYPE_ARTIFACT)
	++artifact_count[player];
  if (typ & TYPE_ENCHANTMENT)
	++enchantment_count[player];

  types_of_cards_on_bf[player] |= typ;
  redraw_libraries();
  inst->ext->host_summoning_sickness = 1;
  inst->state |= STATE_SUMMON_SICK|STATE_SUMMON_SICK_BACKUP|STATE_INVISIBLE|STATE_IN_PLAY;
  dispatch_event(player, card, EVENT_CAST_SPELL);
  inst->state |= STATE_JUST_CAST;
  if (player == 1)
	inst->state |= STATE_POWER_STRUGGLE;
  dispatch_event_to_single_card(player, card, EVENT_RESOLVE_SPELL, 1 - player, -1);
  inst->state &= ~STATE_INVISIBLE;

  // The point - the executable's version just set trigger_cause_controller and trigger_cause without saving their values
  inst->regen_status |= KEYWORD_RECALC_ALL;
  dispatch_trigger2(current_turn, TRIGGER_COMES_INTO_PLAY, gs_card_into_play_0091c840/*PROMPT_SPECIALFEPHASE[0] == "Card into play"*/, 0, player, card);

  if (typ & TYPE_LAND)	// This seems dubious, but whatever
	++lands_played;

  return 0;
}

void play_card_in_opponent_hand_by_test(int player, int card, test_definition_t *this_test){
	int playable[2][hand_count[1-player]];
	int pc = 0;
	int i;
	for(i=0; i<active_cards_count[1-player]; i++){
		if( in_hand(1-player, i) ){
			//Basic check for playability
			if( is_what(1-player, i, TYPE_CREATURE | TYPE_ARTIFACT) || call_card_function(1-player, i, EVENT_CAN_CAST) ){
				if( new_make_test_in_play(1-player, i, -1, this_test) ){
					playable[0][pc] = get_original_internal_card_id(1-player, i);
					playable[1][pc] = i;
					pc++;
				}
			}
		}
	}

	int selected = select_card_from_zone(player, player, playable[0], pc, 0, AI_MAX_CMC, -1, this_test);
	if( selected != -1 ){
		opponent_plays_card_in_your_hand_for_free(1-player, playable[1][selected]);
	}
}

int card_special_effect(int player, int card, event_t event)
{
  int flags = get_card_instance(player, card)->targets[0].card;
  card_instance_t *instance = get_card_instance(player, card);
  if (flags < 0)
    return 0;
  if ((flags & SE_EPIC) && instance->targets[1].card >= 0 &&
      current_turn == instance->targets[0].player && upkeep_trigger(player, card, event))
    real_copy_spell(player, instance->targets[1].card, SF_NOT_CAST | SF_PLAYED_FOR_FREE, SF3_EPIC,
                    NULL, NULL);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if ((flags & (SE_CSVID_CANT_BE_PLAYED | SE_TYPE_CANT_BE_PLAYED)) &&
        (instance->targets[0].player == ANYBODY ||
         instance->targets[0].player == affected_card_controller))
    {
      if ((flags & SE_CSVID_CANT_BE_PLAYED) &&
          get_id(affected_card_controller, affected_card) == instance->targets[1].player)
        infinite_casting_cost();
      if ((flags & SE_TYPE_CANT_BE_PLAYED) && instance->targets[1].player >= 0 &&
          is_what(affected_card_controller, affected_card, instance->targets[1].player))
        infinite_casting_cost();
    }
    if ((flags & SE_MODIFY_NEXT_SPELL_COST) &&
        instance->targets[1].card == CARD_ID_SAHEELI_THE_GIFTED &&
        instance->targets[0].player == affected_card_controller)
      COST_COLORLESS -= count_permanents_by_type(instance->targets[0].player, TYPE_ARTIFACT);
  }
  if (flags & SE_CREATURES_CANT_ATTACK)
    nobody_can_attack(player, card, event, instance->targets[0].player);
  if (event == EVENT_ABILITIES)
  {
    int id = instance->targets[1].card;
    if ((id == CARD_ID_ABEYANCE || id == CARD_ID_SEN_TRIPLETS) &&
        (instance->targets[0].player == ANYBODY ||
         instance->targets[0].player == affected_card_controller))
    {
      card_instance_t *affected = get_card_instance(affected_card_controller, affected_card);
      if (id == CARD_ID_ABEYANCE)
        affected->targets[17].card |= AL_NM_ACT;
      else if (instance->targets[1].card == CARD_ID_SEN_TRIPLETS)
        humiliate(player, card, affected_card_controller, affected_card, 1);
    }
  }
  else if (((flags & SE_END_AT_EOT) && event == EVENT_CLEANUP) ||
           ((flags & SE_UNTIL_YOUR_NEXT_TURN) && event == EVENT_CAN_SKIP_TURN &&
            current_turn == player))
  {
    if (instance->targets[2].player < 1)
    {
      kill_card(player, card, KILL_EXILE);
      cant_be_responded_to = 1;
    }
    else
      --instance->targets[2].player;
  }
  if ((flags & SE_MODIFY_NEXT_SPELL_COST) && trigger_condition == TRIGGER_SPELL_CAST &&
      !is_what(trigger_cause_controller, trigger_cause, TYPE_LAND) &&
      instance->targets[0].player == trigger_cause_controller && spell_fizzled != 1)
    kill_card(player, card, KILL_EXILE);
  return 0;
}

int target_player_cant_cast_csvid(int player, int card, int t_player, int csvid){
	int card_added = generate_reserved_token_by_id(player, CARD_ID_SPECIAL_EFFECT);
	card_instance_t *instance = get_card_instance(player, card_added);
	instance->targets[0].player = t_player;
	instance->targets[0].card = SE_CSVID_CANT_BE_PLAYED | SE_END_AT_EOT;
	instance->targets[1].player = csvid;
	instance->targets[1].card = get_id(player, card);
	put_into_play(player, card_added);
	create_card_name_legacy(player, card_added, get_id(player, card));
	return card_added;
}

int target_player_cant_cast_type(int player, int card, int t_player, int type){
	int card_added = generate_reserved_token_by_id(player, CARD_ID_SPECIAL_EFFECT);
	card_instance_t *instance = get_card_instance(player, card_added);
	instance->targets[0].player = t_player;
	instance->targets[0].card = SE_TYPE_CANT_BE_PLAYED | SE_END_AT_EOT;
	instance->targets[1].player = type;
	instance->targets[1].card = get_id(player, card);
	put_into_play(player, card_added);
	create_card_name_legacy(player, card_added, get_id(player, card));
	return card_added;
}

int target_player_cant_attack(int player, int card, int t_player){
	int card_added = generate_reserved_token_by_id(player, CARD_ID_SPECIAL_EFFECT);
	card_instance_t *instance = get_card_instance(player, card_added);
	instance->targets[0].player = t_player;
	instance->targets[0].card = SE_CREATURES_CANT_ATTACK | SE_END_AT_EOT;
	instance->targets[1].card = get_id(player, card);
	put_into_play(player, card_added);
	create_card_name_legacy(player, card_added, get_id(player, card));
	return card_added;
}

void add_flag_to_special_effect(int player, int card, int flag){
	card_instance_t *instance = get_card_instance(player, card);
	if( instance->targets[0].card < 0 ){
		instance->targets[0].card = 0;
	}
	instance->targets[0].card |= flag;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int check_for_special_effect_card(int player, int csvid){
	int i;
	for(i=0; i<active_cards_count[player]; i++){
		if( in_play(player, i) && get_id(player, i) == CARD_ID_SPECIAL_EFFECT ){
			if( get_card_instance(player, i)->targets[1].card == csvid ){
				return i;
			}
		}
	}
	return -1;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2667b10; original functions/play_spell_for_free.c:5. */
int has_no_mana_cost(int player, int card)
{
  int csvid = player == -1 ? cards_data[card].id : get_id(player, card);
  switch (csvid)
  {
  case CARD_ID_EVERMIND:
  case CARD_ID_ANCESTRAL_VISION:
  case CARD_ID_HYPERGENESIS:
  case CARD_ID_LIVING_END:
  case CARD_ID_LOTUS_BLOOM:
  case CARD_ID_RESTORE_BALANCE:
  case CARD_ID_WHEEL_OF_FATE:
  case CARD_ID_CRASHING_FOOTFALLS:
  case CARD_ID_MOX_TANTALITE:
  case CARD_ID_ASMORANOMARDICADAISTINACULDACAR:
  case CARD_ID_GAEAS_WILL:
  case CARD_ID_GLIMPSE_OF_TOMORROW:
  case CARD_ID_INEVITABLE_BETRAYAL:
  case CARD_ID_PROFANE_TUTOR:
  case CARD_ID_RESURGENT_BELIEF:
  case CARD_ID_SOL_TALISMAN:
    return 1;
  default:
    return 0;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2668490; original functions/play_spell_for_free.c:47. */
static int real_copy_spell(int player, int internal_id, int special_flags, int special_flags3,
                           card_instance_t *copied, target_t *forced_target)
{
  unsigned char copied_type;
  int32_t target_player_value;
  int card;
  card_instance_t *instance;
  int i;
  unsigned int copied_flags;

  card = add_card_to_hand(player, internal_id);
  instance = get_card_instance(player, card);
  if (copied == NULL)
  {
    instance->info_slot = x_value;
  }
  else
  {
    instance->info_slot = copied->info_slot;
    instance->eot_toughness = copied->eot_toughness;
    i = 0;
    do
    {
      target_player_value = copied->targets[i].player;
      instance->targets[i].card = copied->targets[i].card;
      instance->targets[i].player = target_player_value;
      i = i + 1;
    } while (i != 0x13);
    copied_flags = get_special_flags_by_instance(copied);
    set_special_flags(player, card, copied_flags | special_flags);
    copied_flags = get_special_flags2_by_instance(copied);
    set_special_flags2(player, card, copied_flags | SF2_COPIED_FROM_STACK);
    copied_flags = get_special_flags3_by_instance(copied);
    set_special_flags3(player, card, copied_flags | special_flags3);
  }
  if (forced_target != NULL)
  {
    target_player_value = forced_target->player;
    instance->targets[0].card = forced_target->card;
    instance->targets[0].player = target_player_value;
    instance->number_of_targets = '\x01';
  }
  free_spell_player = player;
  free_spell_card = card;
  convert_to_token(player, card);
  if (put_card_on_stack(player, card, 0) == 0)
  {
    if (can_legally_play_iid(player, internal_id) == 0)
    {
      obliterate_card(player, card);
      free_spell_card = -1;
      return card;
    }
    hand_count[player] = hand_count[player] + -1;
    real_put_into_play(player, card);
    copied_type = cards_data[internal_id].type;
    card_on_stack_controller = player;
    card_on_stack = card;
  }
  else
  {
    put_card_on_stack(player, card, 1);
    put_card_on_stack3(player, card);
    copied_type = cards_data[internal_id].type;
  }
  if ((copied_type & 1) != 0)
  {
    land_can_be_played = land_can_be_played | 1;
  }
  dispatch_event(player, card, EVENT_SPELL_COPIED);
  free_spell_card = -1;
  return card;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2668690; original functions/play_spell_for_free.c:111. */
int copy_spell(int player, int csvid)
{
  int result;
  result = get_internal_card_id_from_csv_id(csvid);
  result = real_copy_spell(player, result, (SF_NOT_CAST | SF_PLAYED_FOR_FREE), 0, NULL, NULL);
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26686d0; original functions/play_spell_for_free.c:116. */
int copy_spell_from_stack(int player, int t_player, int t_card)
{
  card_instance_t *instance;
  int result;
  unsigned int internal_id;

  instance = get_card_instance(t_player, t_card);
  internal_id = instance->internal_card_id;
  if (internal_id == 0xffffffff)
  {
    internal_id = (unsigned int)instance->backup_internal_card_id;
  }
  result = real_copy_spell(player, internal_id, (SF_NOT_CAST | SF_PLAYED_FOR_FREE), 0,
                                instance, NULL);
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2668720; original functions/play_spell_for_free.c:125. */
int copy_spell_from_stack_set_target0(int player, int iid, card_instance_t *copied,
                                      int target0_player, int target0_card)
{
  int result;
  target_t targ;
  targ.player = target0_player;
  targ.card = target0_card;
  result = real_copy_spell(
      player, iid, (SF_NOT_CAST | SF_PLAYED_FOR_FREE | SF_TARGETS_ALREADY_SET), 0, copied, &targ);
  return result;
}

#endif

#ifndef SHANDALAR
static int play_card_in_exile_for_free_and_exile_it(int player, int t_player, int csvid)
{
  int iid = get_internal_card_id_from_csv_id(csvid);
  if (!can_legally_play_iid(player, iid))
    return 0;
  remove_card_from_rfg(t_player, csvid);
  int card = add_card_to_hand(player, iid);
  set_special_flags(player, card, SF_EXILE_ON_RESOLUTION);
  if (player != t_player)
  {
    if (player == 0 && t_player == 1)
      add_state(player, card, STATE_OWNED_BY_OPPONENT);
    else
      remove_state(player, card, STATE_OWNED_BY_OPPONENT);
  }
  int result = real_play_spell_for_free(player, card, (int)0x80000000U);
  if (!result)
    obliterate_card(player, card);
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26695a0; original functions/play_spell_for_free.c:535. */
int effect_may_play_card_from_exile(int player, int card, event_t event)
{
  int cmc;
  card_instance_t *instance;
  int play_check_or_choice;
  int exiled_csv_id_or_card;
  int exile_owner;
  unsigned int look_available;
  unsigned int play_mode;
  int generic_cost;
  int play_available;
  unsigned int face_down;
  int iid;
  test_definition_t test;

  instance = get_card_instance(player, card);
  exile_owner = instance->targets[0].player;
  exiled_csv_id_or_card = instance->targets[0].card;
  play_mode = instance->targets[1].player;
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((play_mode & 4) != 0)
    {
      return 0;
    }
    if (check_rfg(exile_owner, exiled_csv_id_or_card) != 0)
    {
      return 0;
    }
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  if (event == EVENT_CLEANUP)
  {
    if ((play_mode & 8) != 0)
      kill_card(player, card, KILL_EXILE);
    return 0;
  }
  if (event == EVENT_BEGIN_TURN)
  {
    if ((play_mode & 0x20) != 0 && current_turn == player)
      instance->targets[1].player = play_mode | 8;
    if ((play_mode & 0x400) != 0)
    {
      if (current_turn != player)
        return 0;
      instance->targets[1].card = 0;
      if ((play_mode & 0x40) != 0)
        kill_card(player, card, KILL_EXILE);
      return 0;
    }
    if ((play_mode & 0x40) != 0 && current_turn == player)
      kill_card(player, card, KILL_EXILE);
    return 0;
  }
  if ((1U < (unsigned int)event - (unsigned int)EVENT_RESOLVE_ACTIVATION) &&
      (event != EVENT_ACTIVATE))
  {
    return 0;
  }
  look_available = 0;
  if ((((play_mode & 4) != 0) && (look_available = 0, ai_is_speculating != 1)) &&
      (look_available = 1, player == 1))
  {
    look_available = (unsigned int)trace_mode >> 1 & 1;
  }
  face_down = play_mode & 4;
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (instance->targets[1].card == 0x2a)
    {
      return 0;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  iid = get_internal_card_id_from_csv_id(exiled_csv_id_or_card);
  play_available = can_legally_play_iid_now(player, iid, event);
  if (((play_mode & 0x100) != 0) &&
      (play_check_or_choice = get_trap_condition(player, 0x60d), play_check_or_choice == 0))
  {
    play_available = 0;
  }
  if (((play_mode & 0x400) != 0) && ((current_turn != player || (instance->targets[1].card == 1))))
  {
    play_available = 0;
  }
  if ((play_mode & 0x200) != 0)
  {
    default_test_definition(&test, 0x100);
    test.subtype = 0x7041;
    play_check_or_choice = check_battlefield_for_special_card(player, card, player, 0, &test);
    if (play_check_or_choice == 0)
    {
      play_available = 0;
    }
  }
  generic_cost = 0;
  bool can_pay = true;
  if (face_down == 0 && (play_check_or_choice = check_rfg(exile_owner, exiled_csv_id_or_card),
                         play_check_or_choice == 0))
    can_pay = false;
  else if ((play_mode & 1) == 0)
  {
    if ((play_mode & 2) == 0)
    {
      play_check_or_choice = has_mana_to_cast_iid(player, event, iid);
      if (play_check_or_choice == 0 ||
          (play_check_or_choice = has_no_mana_cost(-1, iid), play_check_or_choice != 0))
        can_pay = false;
    }
    else
    {
      cmc = get_cmc_by_id(exiled_csv_id_or_card);
      generic_cost = get_updated_casting_cost(player, -1, iid, event, cmc);
      play_check_or_choice = is_what(-1, iid, 0x40);
      play_check_or_choice = has_mana(
          player, (unsigned int)(play_check_or_choice == 0 ? COLOR_COLORLESS : COLOR_ARTIFACT), generic_cost);
      if (play_check_or_choice == 0 ||
          (play_check_or_choice = has_no_mana_cost(-1, iid), play_check_or_choice != 0))
        can_pay = false;
    }
  }
  if (!can_pay)
    play_available = 0;
  if (event == EVENT_CAN_ACTIVATE)
    return play_available != 0 ? play_available : look_available;
  if (event != EVENT_ACTIVATE)
  {
    return 0;
  }
  if ((look_available != 0) &&
      (play_check_or_choice =
           dialog_impl(player, card, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_FULLCARD_CSVID(
                       (unsigned int)instance->display_pic_csv_id), "Play card", play_available, 1,
                       "Look at card", 1, 0xffffffff, 0, 0, 0, 0),
       play_check_or_choice == 2))
  {
    show_deck(player, &iid, 1, "Face-down card in exile", 0, gs_ok_00924800);
    spell_fizzled = 1;
    return 0;
  }
  instance->targets[1].card = 0x2a;
  if ((play_mode & 1) == 0)
  {
    if ((play_mode & 2) == 0)
    {
      charge_mana_from_id(player, -1, EVENT_ACTIVATE, exiled_csv_id_or_card);
    }
    else
    {
      play_check_or_choice = is_what(-1, iid, 0x40);
      charge_mana(player,
                  (unsigned int)(play_check_or_choice == 0 ? COLOR_COLORLESS : COLOR_ARTIFACT),
                  generic_cost);
    }
    play_check_or_choice = spell_fizzled;
    instance->targets[1].card = -1;
    if (play_check_or_choice == 1)
    {
      return 0;
    }
  }
  else
  {
    spell_fizzled = 0;
    instance->targets[1].card = -1;
  }
  kill_card(player, card, KILL_EXILE);
  if (face_down != 0)
  {
    add_csvid_to_rfg(exile_owner, exiled_csv_id_or_card);
  }
  if ((play_mode & 0x80) == 0)
  {
    play_card_in_exile_for_free_impl(player, exile_owner, exiled_csv_id_or_card, 0, -1);
  }
  else
  {
    play_check_or_choice = get_internal_card_id_from_csv_id(exiled_csv_id_or_card);
    if (can_legally_play_iid(player, play_check_or_choice) != 0)
    {
      remove_card_from_rfg(exile_owner, exiled_csv_id_or_card);
      exiled_csv_id_or_card = add_card_to_hand(player, play_check_or_choice);
      set_special_flags(player, exiled_csv_id_or_card, 0x100000);
      if (player != exile_owner)
      {
        if ((player == 0) && (exile_owner == 1))
        {
          add_state(0, exiled_csv_id_or_card, 0x1000);
        }
        else
        {
          remove_state(player, exiled_csv_id_or_card, 0x1000);
        }
      }
      if (real_play_spell_for_free(player, exiled_csv_id_or_card, -0x80000000) == 0)
      {
        obliterate_card(player, exiled_csv_id_or_card);
      }
    }
  }
  cant_be_responded_to = 1;
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline mechanics; native callers retain their expansions. */
/* ManalinkEh 2022: 0x2668150; original functions/play_spell_for_free.c:665. */
int create_may_play_card_from_exile_effect(int player, int card, int whose_exile_zone,
                                                int csvid, mpcfe_mode_t mode)
{
  int card_00;
  card_instance_t *instance;

  if ((mode & MPCFE_ATTACH) == 0)
  {
    card_00 = create_legacy_activate(player, card, effect_may_play_card_from_exile);
  }
  else
  {
    card_00 = create_targetted_legacy_activate(player, card, effect_may_play_card_from_exile,
                                               player, card);
  }
  if (card_00 != -1)
  {
    instance = get_card_instance(player, card_00);
    instance->targets[1].player = mode;
    instance->targets[0].player = whose_exile_zone;
    instance->targets[0].card = csvid;
    if ((mode & MPCFE_FACE_DOWN) == 0)
    {
      create_card_name_legacy(player, card_00, csvid);
      return card_00;
    }
  }
  return card_00;
}
#endif
