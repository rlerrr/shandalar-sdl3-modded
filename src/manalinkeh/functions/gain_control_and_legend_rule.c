#include "manalink.h"
static int effect_on_all_control_effects_attached_to_me(int player,int card,int this_legacy_player,int this_legacy_card,int effect);
#include <windows.h>

int suppress_legend_rule = 0;	// temporarily disables
int is_legendary(int player, int card)
{
  int value;
  if (player == -1)
  {
    if (!has_subtype_by_id(-1, cards_data[card].id, SUBTYPE_LEGEND))
      return 0;
    value = cards_data[card].subtype;
  }
  else
  {
    if (check_special_flags3(player, card, SF3_LEGEND_RULE_DISABLED) ||
        check_status(player, card, STATUS_TRIGGER_CARD) || get_original_id(player, card) == 0x4e25)
      return 0;
    if (get_original_id(player, card) == CARD_ID_SAKASHIMA_THE_IMPOSTOR)
      return 1;
    if (!has_subtype(player, card, SUBTYPE_LEGEND))
      return 0;
    value = get_card_data(player, card)->subtype;
  }
  return value == 0x0e || value == 0x0f;
}

void check_legend_rule(int player, int card, event_t event)
{
  if (suppress_legend_rule || player < 0 || card < 0)
    return;
  if (is_legendary(player, card) && !(player_bits[player] & PB_LEGEND_RULE_DISABLED) &&
      event == EVENT_STATIC_EFFECTS)
  {
    int id = get_card_name(player, card);
    if (id != -1)
      true_verify_legend_rule(player, card, id);
  }
}

int true_verify_legend_rule(int player, int card, int id)
{
  if (id == -1)
    return 0;
  uint32_t sakashima = get_internal_card_id_from_csv_id(CARD_ID_SAKASHIMA_OF_A_THOUSAND_FACES); /* Sakashima of a Thousand Faces */
  card_instance_t *inst = get_card_instance(player, card);
  if (inst && inst->original_internal_card_id == sakashima &&
      !is_humiliated(player, card, AL_STATIC))
    return 0;
  int l_count = 0;
  for (int c = 0; c < active_cards_count[player]; ++c)
  {
    inst = in_play(player, c);
    if (inst && !is_what(player, c, TYPE_EFFECT))
    {
      if (get_card_name(player, c) == id && !is_this_dying(player, c) && is_legendary(player, c) &&
          (inst->original_internal_card_id != sakashima || is_humiliated(player, c, AL_STATIC)))
        ++l_count;
      else
        state_untargettable(player, c, 1);
    }
  }
  if (l_count > 1)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_PERMANENT);
    td.illegal_abilities = 0;
    td.allow_cancel = 0;
    td.allowed_controller = player;
    td.preferred_controller = player;
    card_instance_t *instance = get_card_instance(player, card);
    instance->number_of_targets = 0;
    if (select_target(player, card, &td, "Legend Rule: choose a permanent to sacrifice.",
                      &instance->targets[0]))
    {
      remove_state(player, -1, STATE_CANNOT_TARGET);
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_STATE_BASED_ACTION);
    }
  }
  remove_state(player, -1, STATE_CANNOT_TARGET);
  return l_count > 1;
}

int verify_legend_rule(int player, int card, int id)
{
  if (is_legendary(player, card) && !suppress_legend_rule &&
      !(player_bits[player] & PB_LEGEND_RULE_DISABLED) && id != -1)
    return true_verify_legend_rule(player, card, id);
  return 0;
}

extern int timestamp_card[500];
extern int timestamp_player[500];

int gain_control_impl(int player, int card)
{
  card_instance_t *src = get_card_instance(player, card);
  uint32_t old_timestamp = src->timestamp;
  int new_player = 1 - player;
  int new_card = add_card_to_hand_exe(new_player, src->internal_card_id);
  if (new_card != -1)
  {
    card_instance_t *dest = get_card_instance(new_player, new_card);
    memcpy_inst_and_ext(dest, src);
    dest->ext->host_summoning_sickness = 1;
    dest->regen_status |= KEYWORD_RECALC_ABILITIES;
    dest->state = (dest->state & 0xfffffff3U) | STATE_SUMMONSICK_BOTH;
    timestamp_card[old_timestamp] = new_card;
    timestamp_player[old_timestamp] = new_player;
    for (int p = 0; p < 2; ++p)
      for (int c = 0; c < active_cards_count[p]; ++c)
      {
        card_instance_t *instance = get_card_instance(p, c);
        if (instance->damage_target_player == player && instance->damage_target_card == card)
        {
          instance->damage_target_player = new_player;
          instance->damage_target_card = new_card;
        }
        if (instance->damage_source_player == player && instance->damage_source_card == card)
        {
          instance->damage_source_player = new_player;
          instance->damage_source_card = new_card;
        }
        unsigned int count = instance->number_of_targets;
        for (unsigned int i = 0; i < count; ++i)
          if (instance->targets[i].player == player && instance->targets[i].card == card)
          {
            instance->targets[i].player = new_player;
            instance->targets[i].card = new_card;
          }
      }
  }
  uint8_t type = cards_data[src->internal_card_id].type;
  if ((type & 2) != 0)
    ++creature_count[new_player];
  if ((type & 0x40) != 0)
    ++artifact_count[new_player];
  if ((type & 4) != 0)
    ++enchantment_count[new_player];
  types_of_cards_on_bf[new_player] |= type;
  redraw_libraries();
  src->token_status |= 0x40000008U;
  kill_card_exe(player, card, KILL_EXILE);
  if (((uint32_t)event_flags & 0x10000000U) != 0)
    dispatch_event(new_player, new_card, (event_t)0x94);
  return new_card;
}

int switch_control(int player, int card, int tar_player, int tar_card)
{
  int legacy_card =
      create_legacy_effect_exe(player, card, LEGACY_EFFECT_ALADDIN, tar_player, tar_card);
  card_instance_t *legacy = get_card_instance(player, legacy_card);
  legacy->damage_source_player = -1;
  legacy->damage_source_card = -1;
  legacy->token_status |= 0x20;
  legacy->targets[0].player = player;
  legacy->targets[0].card = card;
  legacy->number_of_targets = 1;
  u4585f0(player, legacy_card);
  legacy->token_status |= 0x1000000;
  play_sound_effect(WAV_CONTROL);
  int result = gain_control_impl(legacy->damage_target_player, legacy->damage_target_card);
  legacy->damage_target_player = 1 - tar_player;
  legacy->damage_target_card = result;
  if (check_special_flags(1 - tar_player, result, SF_ECHO_TO_PAY))
  {
    remove_special_flags3(1 - tar_player, result, SF3_ECHO_PAID);
  }
  // legacy->info_slot = 333;
  return legacy_card;
}

int gain_control(int player, int card, int tar_player, int tar_card ){
	if( player == tar_player || check_for_guardian_beast_protection(tar_player, tar_card) ){
		return -1;
	}
	return switch_control(player, card, tar_player, tar_card );
}

static int generic_control_legacy(int player, int card, event_t event)
{
  int controlled_card;
  int source_card;
  unsigned int spirit_link_flags;
  card_instance_t *instance;
  int granted_ability_or_check;
  int source_player_or_check;
  int controlled_player;
  subtype_t encoded_subtypes;
  unsigned int control_mode;
  bool abilities_event;

  if (player < 0)
  {
    return 0;
  }
  if (card < 0)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  if (instance->damage_target_player < '\0')
  {
    return 0;
  }
  controlled_player = (int)instance->damage_target_player;
  source_player_or_check = instance->targets[0].player;
  controlled_card = instance->damage_target_card;
  source_card = instance->targets[0].card;
  granted_ability_or_check = instance->targets[1].player;
  control_mode = instance->targets[1].card;
  if (0 < granted_ability_or_check)
  {
    special_abilities(controlled_player, controlled_card, event, granted_ability_or_check, player,
                      card);
  }
  granted_ability_or_check = instance->targets[2].player;
  if (granted_ability_or_check < 1 || event != EVENT_POWER)
  {
    granted_ability_or_check = instance->targets[2].card;
    if ((event == EVENT_TOUGHNESS) && (0 < granted_ability_or_check))
    {
      abilities_event = false;
      if ((controlled_card == affected_card) && (controlled_player == affected_card_controller))
      {
        event_result = event_result + granted_ability_or_check;
      }
    }
    else
    {
      granted_ability_or_check = instance->targets[3].player;
      abilities_event = event == EVENT_ABILITIES;
      if ((0 < granted_ability_or_check && abilities_event) &&
          ((abilities_event = true,
            controlled_card == affected_card && (controlled_player == affected_card_controller))))
      {
        event_result = event_result + granted_ability_or_check;
      }
    }
  }
  else
  {
    if ((controlled_card == affected_card) && (controlled_player == affected_card_controller))
    {
      event_result = event_result + granted_ability_or_check;
    }
    abilities_event = event == EVENT_ABILITIES;
  }
  encoded_subtypes = instance->targets[4].player;
  if (-1 < (int)encoded_subtypes)
  {
    if ((char)encoded_subtypes != '\0')
    {
      attached_is_additional_subtype(player, card, event, encoded_subtypes & 0xff);
      encoded_subtypes = instance->targets[4].player;
    }
    if ((char)(encoded_subtypes >> 0x10) != '\0')
    {
      attached_is_forced_subtype(player, card, event, (int)encoded_subtypes >> 0x10 & 0xff);
    }
  }
  spirit_link_flags = instance->targets[4].card;
  if (((0 < (int)spirit_link_flags) && ((spirit_link_flags & 1) != 0)) &&
      (instance->targets[3].card == 0x1ea5))
  {
    spirit_link_effect(controlled_player, controlled_card, event, instance->targets[0].player);
  }
  if (((event == EVENT_DECLARE_BLOCKERS) || (abilities_event)) &&
      (instance->targets[3].card == 0x3a6a))
  {
    fx_menace(player, card, event);
  }
  if (control_mode == 0)
  {
    return 0;
  }
  if ((((control_mode & 0x81) != 0) && (granted_ability_or_check = eot_trigger(player, card, event),
                                        granted_ability_or_check != 0)) &&
      (((control_mode & 0x80) == 0 || (player == current_turn))))
  {
    granted_ability_or_check = instance->targets[3].card;
    if (granted_ability_or_check == 0x286b)
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
      return 0;
    }
    if ((granted_ability_or_check == 0x7e9) || (granted_ability_or_check == 0x81b))
    {
      tap_card(controlled_player, controlled_card);
    }
    effect_on_all_control_effects_attached_to_me(controlled_player, controlled_card, player, card,
                                                 1);
    granted_ability_or_check = check_state(player, card, 0x400000);
    if ((granted_ability_or_check != 0) &&
        (granted_ability_or_check = check_status(player, card, 0x1000000),
         granted_ability_or_check != 0))
    {
      gain_control_impl(controlled_player, controlled_card);
    }
    kill_card(player, card, KILL_EXILE);
  }
  if ((control_mode & 0x7e) == 0)
  {
    return 0;
  }
  if (event != EVENT_CARDCONTROLLED && event != EVENT_STATIC_EFFECTS &&
      event != EVENT_LEAVES_PLAY_ABILITY)
  {
    if (trigger_condition != TRIGGER_LEAVE_PLAY || card != affected_card || player != affected_card_controller)
      return 0;
    if (instance->targets[5].player != -1)
      other_leaves_play(player, card, source_player_or_check, source_card, event, 0, 0);
    else
      instance->targets[5].player = 1;
    return 0;
  }
  if (instance->targets[5].player == -1)
  {
    instance->targets[5].player = 1;
    return 0;
  }
  other_leaves_play(player, card, source_player_or_check, source_card, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    if ((control_mode & 8) != 0)
      kill_card(controlled_player, controlled_card, KILL_BURY);
    else if ((control_mode & 0x40) == 0)
      return 0;
  }
  else if (event == EVENT_CARDCONTROLLED)
  {
    if (source_card != affected_card || source_player_or_check != affected_card_controller ||
        (control_mode & 2) == 0)
      return 0;
  }
  else
  {
    if (source_player_or_check < 0 || instance->targets[5].player != 1)
      return 0;
    bool release_control = false;
    if (is_tapped(source_player_or_check, source_card) == 0)
    {
      if ((control_mode & 8) != 0)
        kill_card(controlled_player, controlled_card, KILL_BURY);
      if ((control_mode & 4) != 0)
      {
        instance->targets[5].player = -1;
        release_control = true;
      }
    }
    if ((control_mode & 0x10) != 0)
    {
      granted_ability_or_check = get_power(controlled_player, controlled_card);
      source_player_or_check = get_power(source_player_or_check, source_card);
      if (granted_ability_or_check > source_player_or_check)
      {
        instance->targets[5].player = -1;
        release_control = true;
      }
    }
    if ((control_mode & 0x20) != 0 &&
        (source_player_or_check = is_enchanted(controlled_player, controlled_card),
         source_player_or_check == 0))
    {
      instance->targets[5].player = -1;
      release_control = true;
    }
    if (!release_control)
      return 0;
  }
  instance = in_play(controlled_player, controlled_card);
  if (instance != NULL)
  {
    effect_on_all_control_effects_attached_to_me(controlled_player, controlled_card, player, card,
                                                 1);
    source_player_or_check = check_state(player, card, 0x400000);
    if ((source_player_or_check != 0) &&
        (source_player_or_check = check_status(player, card, 0x1000000),
         source_player_or_check != 0))
    {
      gain_control_impl(controlled_player, controlled_card);
    }
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

static int effect_on_all_control_effects_attached_to_me(int player, int card, int this_legacy_player, int this_legacy_card, int effect){
// effect = 0 --> check only
// effect = 1 -> enable the "control legacy" with the highest timestamp (AKA the most recent)
// effect = -1 -> disable all "control legacies"
	int result = 0;
	int legacies[100][3];
	int lc = 0;
	card_instance_t* inst;
	int p, c;
	for (p = 0; p <= 1; ++p){
		for (c = active_cards_count[p]; c > -1; c--){
			if ( (inst = in_play(p, c)) && inst->damage_target_player == player && inst->damage_target_card == card ){
				if( is_what(p, c, TYPE_EFFECT) && inst->info_slot == (intptr_t)generic_control_legacy ){
					//STATE_POWER_STRUGGLE = This "control legacy" has stolen a permanent from someone, so it needs to be processed.
					if( check_state(p, c, STATE_POWER_STRUGGLE) ){
						if( effect == 1 ){
							if( ! check_status(p, c, STATUS_CONTROLLED) ){
								if( this_legacy_player == -1 || 
									(this_legacy_player != -1 && this_legacy_card != -1 && !(p == this_legacy_player && c == this_legacy_card)))
								{
									legacies[lc][0] = inst->timestamp;
									legacies[lc][1] = p;
									legacies[lc][2] = c;
									lc++;
									result = 1;
								}
							}
						}
						//STATUS_CONTROLLED = This "control legacy" is active and the enchanted permanent will return to its previous controller if this legacy leaves play
						if( effect == -1 ){
							remove_status(p, c, STATUS_CONTROLLED);
							result = 1;
						}
					}
				}
			}
		}
	}
	if( lc ){
		int max_timestamp = 0;
		int id_of_legacy_to_activate = 0;
		int i;
		for(i=0; i<lc; i++){
			if( legacies[i][0] > max_timestamp ){
				max_timestamp = legacies[i][0];
			}
			id_of_legacy_to_activate = i;
		}
		add_status(legacies[id_of_legacy_to_activate][1], legacies[id_of_legacy_to_activate][2], STATUS_CONTROLLED);
	}
	return result;
}

int disable_all_control_effects_attached_to_me(int player, int card){
	return effect_on_all_control_effects_attached_to_me(player, card, -1, -1, -1);
}

int re_enable_the_most_recent_control_effect_attached_to_me(int player, int card){
	return effect_on_all_control_effects_attached_to_me(player, card, -1, -1, 1);
}

int re_enable_the_other_most_recent_control_effect_attached_to_me(int player, int card, int legacy_p, int legacy_c){
	return effect_on_all_control_effects_attached_to_me(player, card, legacy_p, legacy_c, 1);
}

int gain_control_permanently_impl(int player, int t_player, int t_card, int sound)
{
  int result = -1;
  disable_all_control_effects_attached_to_me(t_player, t_card);
  if (player == t_player)
  {
    result = t_card;
  }
  else
  {
    if (check_for_guardian_beast_protection(t_player, t_card))
    {
      return -1;
    }
    result = gain_control_impl(t_player, t_card);
    if (result > -1)
    {
      if (check_special_flags(player, result, SF_ECHO_TO_PAY))
      {
        remove_special_flags3(player, result, SF3_ECHO_PAID);
      }
      if (sound)
      {
        play_sound_effect(WAV_CONTROL);
      }
    }
  }
  return result;
}

int gain_control_permanently(int player, int t_player, int t_card){
	return gain_control_permanently_impl(player, t_player, t_card, 1);
}

int gain_control_permanently_no_sound(int player, int t_player, int t_card){
	return gain_control_permanently_impl(player, t_player, t_card, 0);
}

void exchange_control_of_target_permanents(int player, int card, int player1, int card1, int player2, int card2)
{
  if (player1 != player2)
	{
	  suppress_legend_rule = 1;

	  int new_card1 = gain_control_permanently_no_sound(player1, player2, card2);
	  int new_card2 = gain_control_permanently_no_sound(player2, player1, card1);

	  suppress_legend_rule = 0;

	  play_sound_effect(WAV_CONTROL);

	  dispatch_event(player1, new_card1, EVENT_CARDCONTROLLED);
	  dispatch_event(player2, new_card2, EVENT_CARDCONTROLLED);
	}
}

static int gain_control_until_clause(int player, int card, int t_player, int t_card, int mode,
                                     int pow, int tou, int key, int s_key)
{
  // Mode:	see 'gcus_t' in 'defs.h'.

  int legacy_card = -1;
  int result = gain_control_permanently(player, t_player, t_card);
  if (result > -1)
  {
    legacy_card =
        create_targetted_legacy_effect(player, card, &generic_control_legacy, player, result);
    if (legacy_card > -1)
    {
      card_instance_t *legacy_instance = get_card_instance(player, legacy_card);
      if (player != t_player)
      {
        // STATUS_POWER_STRUGGLE = this "control legacy" has stolen a permanent from someone
        add_state(player, legacy_card, STATE_POWER_STRUGGLE);
        // STATUS_CONTROLLED = this "control legacy" is active and will return the permanent to his
        // previous controller when leave play
        add_status(player, legacy_card, STATUS_CONTROLLED);
      }
      legacy_instance->targets[0].player = player;
      legacy_instance->targets[0].card = card;
      legacy_instance->targets[1].player = s_key;
      legacy_instance->targets[1].card = mode;
      legacy_instance->targets[2].player = pow;
      legacy_instance->targets[2].card = tou;
      legacy_instance->targets[3].player = key;
      legacy_instance->targets[3].card = get_id(player, card);
    }
    recalculate_all_cards_in_play();
  }
  return legacy_card;
}

int give_control(int effect_source_player, int effect_source_card, int tar_player, int tar_card)
{
  disable_all_control_effects_attached_to_me(tar_player, tar_card);
  int result = gain_control_impl(tar_player, tar_card);
  play_sound_effect(WAV_CONTROL); // Since removing control effects doesn't do this itself
  int legacy = create_targetted_legacy_effect(effect_source_player, effect_source_card, &empty,
                                              1 - tar_player, result);
  return legacy;
}

int give_control_until_eot(int effect_source_player, int effect_source_card, int tar_player, int tar_card)
{
	card_instance_t* instance = get_card_instance(effect_source_player, effect_source_card);

	int iid = instance->internal_card_id == -1 ? (int)instance->original_internal_card_id : instance->internal_card_id;
	int fake = add_card_to_hand(1-effect_source_player, iid);
	card_instance_t* temp = get_card_instance(1-effect_source_player, fake);
	temp->state |= STATE_INVISIBLE;
	--hand_count[1-effect_source_player];

	int legacy = gain_control_until_eot(1-effect_source_player, fake, tar_player, tar_card);

	obliterate_card(1-effect_source_player, fake);

	return legacy;
}

void give_control_of_self(int player, int card)
{
  disable_all_control_effects_attached_to_me(player, card);
  gain_control_impl(player, card);
  play_sound_effect(WAV_CONTROL); // Since removing control effects doesn't do this itself
}

int effect_act_of_treason_and_modify_pt_or_abilities(int player, int card, int t_player, int t_card, int pow, int tou, int key, int s_key){
	int legacy_card = -1;
	if (in_play(t_player, t_card)){
		untap_card(t_player, t_card);
		if( player != t_player ){
			gain_control_until_clause(player, card, t_player, t_card, 1, pow, tou, key, s_key | SP_KEYWORD_HASTE);
		}
		else{
			legacy_card = pump_ability_until_eot(player, card, t_player, t_card, pow, tou, key, s_key | SP_KEYWORD_HASTE);
		}
	}
	return legacy_card;
}

int effect_act_of_treason(int player, int card, int t_player, int t_card)
{
	return effect_act_of_treason_and_modify_pt_or_abilities(player, card, t_player, t_card, 0, 0, 0, 0);
}

int gain_control_until_eot(int player, int card, int t_player, int t_card){
	return gain_control_until_clause(player, card, t_player, t_card, 1, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x2664aa0; original functions/gain_control_and_legend_rule.c:577. */
int gain_control_until_eot_haste(int player, int card, int t_player, int t_card)
{
  int result;

  result = gain_control_until_clause(player, card, t_player, t_card, 1, 0, 0, 0, 4);
  return result;
}

void gain_control_until_source_is_in_play_and_tapped(int player, int card, int t_player, int t_card,
                                                     gcus_t mode)
{
  if (in_play(t_player, t_card))
  {
    int source_player, source_card;
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->internal_card_id == activation_card)
    {
      source_player = instance->parent_controller;
      source_card = instance->parent_card;
    }
    else
    {
      source_player = player;
      source_card = card;
    }

    if (!in_play(player, card) || ((mode & GCUS_CONTROLLED) && player != source_player) ||
        ((mode & GCUS_TAPPED) && !is_tapped(player, card)))
      return;

    int leg =
        gain_control_until_clause(source_player, source_card, t_player, t_card, mode, 0, 0, 0, 0);
    if (leg != -1)
      alternate_legacy_text(1, player, leg);
  }
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2664c10; original functions/gain_control_and_legend_rule.c:611. */
int gain_control_until_eoyt(int player, int card, int t_player, int t_card)
{
  int result;

  result = gain_control_until_clause(player, card, t_player, t_card, 0x80, 0, 0, 0, 0);
  return result;
}

void get_back_your_permanents(int player, int card, type_t typ)
{
	// First, destroy any effect cards controlling permanents stolen from player.
	int marked[151];
	int mc = 0;
	int c;
	int p = 1-player;
	for(c=0; c<active_cards_count[p]; c++){
		if( in_play(p, c) && is_what(p, c, TYPE_EFFECT) ){
			card_instance_t *inst = get_card_instance(p, c);
			if( inst->info_slot == (intptr_t)generic_control_legacy ){
				if( is_stolen(inst->damage_target_player, inst->damage_target_card) && 
					is_what(inst->damage_target_player, inst->damage_target_card, typ) )
				{
					marked[mc] = c;
					mc++;
				}
			}
		}
	}

	for(c=0; c<mc; c++){
		kill_card(p, marked[c], KILL_EXILE);
	}

	// Anything left over.
	for(c=0; c<active_cards_count[p]; c++){
		if( in_play(p, c) && is_what(p, c, typ) ){
			if( is_stolen(p, c) ){
				gain_control_permanently(player, p, c);
			}
		}
	}
}
