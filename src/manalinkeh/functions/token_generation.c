#include "manalink.h"

/* Canonical recovered helper declarations. */
static int legacy_negate_static_abilities(int player, int card, event_t event);
void default_token_definition(int player, int card, int id, token_generation_t *token);
void copy_token_definition(int player, int card, token_generation_t *token, int t_player,
                                int t_card);
void generate_token(token_generation_t *token);
static void set_token(int player, int card, token_generation_t *token);
void generate_tokens_by_id(int player, int card, int csvid, int howmany);
int generate_reserved_token_by_id(int player, int csvid);
void copy_token(int player, int card, int tok_player, int tok_card);
int generic_token(int player, int card, event_t event);
int token_characteristic_setting_effects(int player, int card, event_t event, int t_player,
                                              int t_card);
int copy_token_characteristics_for_clone(int player, int card, int t_player, int t_card);

static int generate_token_by_id_impl(int player, int card, int csvid, int number, int reserved);
int generate_token_by_id(int player, int card, int csvid);

static int get_copyable_special_infos(int t_player, int t_card);

// Create a token definition that's a copy of (t_player,t_card).

static void copy_token_part(int player, int card, int tok_player, int tok_card);

/* October 2022 token creation component. */
void generate_token(token_generation_t *token);
static void set_token(int player, int card, token_generation_t *token);
void default_token_definition(int player, int card, int id, token_generation_t *token);
static int resolve_token_iid(iid_t int_id, token_generation_t *token);
static int get_copyable_special_infos(int t_player, int t_card);
void copy_token_definition(int player, int card, token_generation_t *token, int t_player,
                                int t_card);
int token_characteristic_setting_effects(int player, int card, event_t event, int t_player,
                                              int t_card);
int generic_token(int player, int card, event_t event);

/* October 2022 token ID/copy entry points. */
static int generate_token_by_id_impl(int player, int card, int csvid, int number, int reserved);
void copy_token(int player, int card, int tok_player, int tok_card);
int generate_token_by_id(int player, int card, int csvid);
void generate_tokens_by_id(int player, int card, int csvid, int howmany);
int generate_reserved_token_by_id(int player, int csvid);

/* ManalinkEh 2022: 0x2665470; original functions/token_generation.c:5. */
void default_token_definition(int player, int card, int id, token_generation_t *token)
{
  token->id = id;
  token->eff_player = player;
  token->eff_card = card;
  token->t_player = player;
  if (player >= 0 && card >= 0)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int source_iid = instance->internal_card_id;
    if (source_iid == activation_card)
    {
      player = instance->parent_controller;
      card = instance->parent_card;
    }
    else if (source_iid >= 0 && (unsigned int)cards_data[source_iid].id - 901U < 8)
    {
      player = instance->damage_source_player;
      card = instance->damage_source_card;
    }
  }
  if (player >= 0 && card >= 0)
  {
    token->s_player = player;
    token->s_card = card;
    token->no_sleight = 0;
  }
  else
  {
    token->s_card = -1;
    token->s_player = -1;
    token->no_sleight = 1;
  }
  token->pow = -1;
  token->tou = -1;
  token->key_plus = 0;
  token->s_key_plus = 0;
  token->color_forced = 0;
  token->special_code_for_legacy = empty;
  token->special_code_on_generation = NULL;
  token->legacy = 0;
  token->qty = 1;
  token->special_infos = 0;
  token->action = 0;
  token->action_argument = 0;
  token->keep_track_of_tokens_generated = 0;
  token->special_flags2 = 0;
  token->special_flags3 = 0;
}

/* ManalinkEh 2022: 0x2665580; original functions/token_generation.c:62. */
void copy_token_definition(int player, int card, token_generation_t *token, int t_player,
                                int t_card)
{
  int csvid = get_id(t_player, t_card);
  default_token_definition(player, card, csvid, token);
  token->no_sleight = 1;
  if (cards_ptr[csvid]->card_type == 8)
  {
    card_instance_t *instance = get_card_instance(t_player, t_card);
    if (!check_special_flags(t_player, t_card, 0x40000000))
    {
      token->s_player = instance->damage_source_player;
      token->s_card = instance->damage_source_card;
    }
    else
    {
      token->s_card = -1;
      token->s_player = -1;
    }
    special_flags2_t copied_flags = instance->ext->special_flags2;
    token->s_key_plus = instance->ext->sp_keywords;
    token->special_flags2 = copied_flags;
    unsigned int special_infos = 0;
    if (t_player >= 0 && t_card >= 0)
      special_infos = get_copyable_special_infos(t_player, t_card);
    token->special_infos = special_infos;
    if (check_special_flags2(t_player, t_card, 0x80))
      token->special_flags2 = 0x80;
  }
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2665690; original functions/token_generation.c:87. */
int copy_token_characteristics_for_clone(int player, int card, int t_player, int t_card)
{
  int csvid;
  uint8_t original_color;
  int32_t saved_target_component;
  unsigned char copied_color;
  int copy_result;
  card_instance_t *instance;
  card_instance_t *token_instance;
  uint32_t copied_iid_or_result;

  csvid = get_id(t_player, t_card);
  copied_iid_or_result = 0;
  if (cards_ptr[csvid]->card_type == 8)
  {
    instance = get_card_instance(player, card);
    token_instance = get_card_instance(t_player, t_card);
    copied_iid_or_result = token_instance->original_internal_card_id;
    if (copied_iid_or_result == 0xffffffff)
    {
      copied_iid_or_result = (uint32_t)token_instance->backup_internal_card_id;
    }
    saved_target_component = token_instance->targets[7].player;
    instance->targets[7].card = token_instance->targets[7].card;
    instance->targets[7].player = saved_target_component;
    saved_target_component = token_instance->targets[8].card;
    instance->targets[8].player = token_instance->targets[8].player;
    instance->targets[8].card = saved_target_component;
    original_color = cards_data[copied_iid_or_result].color;
    copy_result = get_internal_card_id_from_csv_id(csvid);
    if (original_color == cards_data[copy_result].color)
    {
      copied_color = instance->initial_color & 0x3e;
    }
    else
    {
      copied_color = cards_data[copied_iid_or_result].color & 0x3e;
      instance->initial_color = copied_color;
    }
    instance->color = copied_color;
    instance->regen_status = instance->regen_status | 0x10000000;
    if ((t_player < 0) || (t_card < 0))
    {
      copy_result = 0;
    }
    else
    {
      copy_result = get_copyable_special_infos(t_player, t_card);
    }
    instance = get_card_instance(player, card);
    instance->targets[0xe].player = copy_result;
    copy_result = check_special_flags2(t_player, t_card, 0x80);
    copied_iid_or_result = 1;
    if (copy_result != 0)
    {
      copied_iid_or_result = 1;
      set_special_flags2(player, card, 0x80);
    }
  }
  return copied_iid_or_result;
}
#endif

/* ManalinkEh 2022: 0x2665820; original functions/token_generation.c:124. */
int token_characteristic_setting_effects(int player, int card, event_t event, int t_player,
                                              int t_card)
{
  special_flags2_t value;
  int result;
  card_instance_t *instance;

  if (card != -1)
  {
    result = check_special_flags2(player, card, 0x400);
    if (result == 0)
    {
      instance = get_card_instance(player, card);
      special_abilities(t_player, t_card, event, instance->unused0x124, t_player,
                        t_card);
      if ((((event == EVENT_CHANGE_TYPE) && (t_card == affected_card)) &&
           (t_player == affected_card_controller)) &&
          (value = instance->ext->special_flags2, 0 < value))
      {
        if ((land_can_be_played & 0x20000U) == 0)
        {
          land_can_be_played = land_can_be_played | 0x10000;
        }
        else
        {
          set_special_flags2(t_player, t_card, value);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2665900; original functions/token_generation.c:146. */
int generic_token(int player, int card, event_t event)
{
  int result;
  result = token_characteristic_setting_effects(player, card, event, player, card);
  return result;
}

/* ManalinkEh 2022: 0x2664eb0; original functions/token_generation.c:150. */
static void set_token(int player, int card, token_generation_t *token)
{
  uint32_t special_keywords;
  unsigned int copied_infos;
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  instance->damage_source_player = (int8_t)token->s_player;
  instance->damage_source_card = token->s_card;
  special_keywords = token->s_key_plus;
  instance->ext->sp_keywords = special_keywords;
  instance->unused0x124 = special_keywords;
  set_special_flags2(player, card, token->special_flags2);
  instance = get_card_instance(player, card);
  copied_infos = token->special_infos;
  instance->token_status = instance->token_status | 0x10;
  instance = get_card_instance(player, card);
  instance->targets[0xe].player = copied_infos;
  return;
}

/* Just like generic_token(), but supports storing the values on an effect card to set them on a different permanent.  The idea is that changing a permanent to
 * temporarily be a copy of a token shouldn't have to overwrite all of its targets. */
void convert_to_token(int player, int card){
	card_instance_t *instance = get_card_instance(player, card);
	instance->token_status |= STATUS_TOKEN;
}

static int get_updated_tokens_number(int player, int number)
{
  card_instance_t* instance;
  int p, c;
  for (p = 0; p <= 1; ++p)
	for (c = 0; c < active_cards_count[p]; ++c)
	  if ((instance = in_play(p, c)))
		switch (cards_data[instance->internal_card_id].id)
		  {
			case CARD_ID_DOUBLING_SEASON:		// self only: doubles
			case CARD_ID_PARALLEL_LIVES:
			case CARD_ID_ANOINTED_PROCESSION:
			  if (p != player)
				continue;
			  // else fall through
			case CARD_ID_SELESNYA_LOFT_GARDENS:	// everyone: doubles
			case CARD_ID_PRIMAL_VIGOR:
			  number *= 2;
			  break;
		  }
  return number;
}

static int is_reserved_id(int csvid){
	int iid = get_internal_card_id_from_csv_id(csvid);
	return (cards_data[iid].cc[2] == 3	// vanguard card, edh, etc.
			|| cards_data[iid].type == TYPE_EFFECT);
}

void set_special_infos(int player, int card, int infos){
	card_instance_t *instance = get_card_instance(player, card);
	instance->targets[14].player = infos;
}

int get_special_infos(int player, int card)
{
  if (player < 0 || card < 0)
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->targets[14].player > -1)
  {
    return instance->targets[14].player;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2665380; original functions/token_generation.c:218. */
static int get_copyable_special_infos(int t_player, int t_card)
{
  card_instance_t *instance = get_card_instance(t_player, t_card);
  int infos = instance->targets[14].player;
  if (infos <= 0)
    return 0;
  switch (get_backup_id(t_player, t_card))
  {
  case CARD_ID_ASSEMBLY_WORKER:
  case CARD_ID_ASSASSIN:
  case CARD_ID_BAT:
  case CARD_ID_CLERIC:
  case CARD_ID_ELDRAZI_SPAWN:
  case CARD_ID_ILLUSION_TOKEN:
  case CARD_ID_SAPROLING:
  case CARD_ID_SPIRIT:
  case CARD_ID_SCULPTURE:
    return infos == 66 ? infos : 0;
  case CARD_ID_ELEMENTAL:
  case CARD_ID_GOBLIN:
  case CARD_ID_INSECT:
    return infos == 67 ? infos : 0;
  case CARD_ID_DRAGON:
  case CARD_ID_OOZE:
  case CARD_ID_WOLF:
    return infos == 66 || infos == 67 ? infos : 0;
  default:
    return 0;
  }
}

#ifndef SHANDALAR
static int compare_iid_token(iid_t int_id, token_generation_t *token)
{
  if (token->id != cards_data[int_id].id ||
      (token->pow >= 0 && token->pow != cards_data[int_id].power) ||
      (token->tou >= 0 && token->tou != cards_data[int_id].toughness) ||
      !(cards_data[int_id].static_ability & token->key_plus))
    return 0;
  int artifact_matches = !(token->action & TOKEN_ACTION_CONVERT_INTO_ARTIFACT) ||
                          (cards_data[int_id].type & TYPE_ARTIFACT);
  unsigned int color = token->color_forced ? token->color_forced : cards_data[int_id].color;
  if (token->s_player >= 0 && token->s_card >= 0 && !token->no_sleight)
    color = get_sleighted_color_test(token->s_player, token->s_card, color & COLOR_TEST_ANY_COLORED);
  return cards_data[int_id].color == color && artifact_matches;
}
#endif

#ifndef SHANDALAR
static void release_dynamic_card_type(iid_t dynamic_iid)
{
  if (dynamic_iid >= engine_dynamic_iid_start &&
      dynamic_iid <= engine_dynamic_iid_start + 15)
    cards_data[dynamic_iid].id = -1;
}
#endif

/* ManalinkEh 2022: 0x2664f30; original functions/token_generation.c:295. */
static int resolve_token_iid(iid_t int_id, token_generation_t *token)
{
  int original_iid = get_internal_card_id_from_csv_id(cards_data[int_id].id);
  int dynamic_start = engine_dynamic_iid_start;
  if (cards_data[original_iid].cc[2] == 3 || cards_data[original_iid].type == TYPE_EFFECT)
    return int_id;

  int dynamic_end = engine_dynamic_iid_start + 16;
  for (int candidate = engine_dynamic_iid_start; candidate < dynamic_end; ++candidate)
  {
    uint32_t id_word = cards_data[candidate].id | 0;
    if ((uint16_t)id_word == cards_data[int_id].id && token->id == (id_word & 0xffff) &&
        (token->pow < 0 || token->pow == cards_data[candidate].power) &&
        (token->tou < 0 || token->tou == cards_data[candidate].toughness) &&
        (cards_data[candidate].static_ability & token->key_plus))
    {
      int artifact_matches = !(token->action & TOKEN_ACTION_CONVERT_INTO_ARTIFACT) ||
                             (cards_data[candidate].type & TYPE_ARTIFACT);
      unsigned int existing_color = cards_data[candidate].color;
      unsigned char desired_color =
          token->color_forced ? token->color_forced : cards_data[candidate].color;
      unsigned int color = desired_color;
      if (token->s_player >= 0 && token->s_card >= 0 && !token->no_sleight)
      {
        color = get_sleighted_color_test(token->s_player, token->s_card, desired_color & 0x3e);
        existing_color = cards_data[candidate].color;
      }
      if (existing_color == color && artifact_matches)
        return candidate;
    }
  }
  unsigned int color = token->color_forced ? token->color_forced : cards_data[int_id].color;
  if (token->s_player >= 0 && token->s_card >= 0 && !token->no_sleight)
    color = get_sleighted_color_test(token->s_player, token->s_card, color & 0x3e);
  if (!((token->pow >= 0 && token->pow != cards_data[int_id].power) ||
        (token->tou >= 0 && token->tou != cards_data[int_id].toughness) ||
        (token->key_plus && token->key_plus != cards_data[int_id].static_ability) ||
        cards_data[int_id].color != color ||
        (!(cards_data[int_id].type & TYPE_ARTIFACT) &&
         (token->action & TOKEN_ACTION_CONVERT_INTO_ARTIFACT)) ||
        (token->action & TOKEN_ACTION_ISNT_LEGENDARY)))
    return int_id;

  int used[16] = {0};
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < 150; ++c)
    {
      card_instance_t *instance = get_card_instance(p, c);
      uint32_t iid = instance->original_internal_card_id;
      if (iid == UINT32_MAX)
        iid = instance->backup_internal_card_id;
      if ((int)iid >= dynamic_start && (int)iid <= dynamic_start + 15)
        used[iid - dynamic_start] = 1;
      int animated_iid = instance->dummy3;
      if (animated_iid >= dynamic_start && animated_iid <= dynamic_start + 15 && iid != 0)
        used[animated_iid - dynamic_start] = 1;
    }
  for (int iid = dynamic_start; iid < dynamic_end; ++iid)
    if (!used[iid - dynamic_start] && iid >= engine_dynamic_iid_start &&
        iid <= engine_dynamic_iid_start + 15)
      cards_data[iid].id = -1;
  int created = create_a_card_type(int_id);
  if (created < 0) return -1;
  cards_data[created] = cards_data[int_id];
  int power = token->pow;
  if (power >= 0 && power != cards_data[int_id].power)
    cards_data[created].power = (int16_t)power;
  int toughness = token->tou;
  if (toughness >= 0 && toughness != cards_data[int_id].toughness)
    cards_data[created].toughness = (int16_t)toughness;
  if (token->key_plus)
    cards_data[created].static_ability |= token->key_plus;
  token_action_t action = token->action;
  if (!(cards_data[int_id].type & TYPE_ARTIFACT) && (action & TOKEN_ACTION_CONVERT_INTO_ARTIFACT))
    cards_data[created].type |= TYPE_ARTIFACT;
  if (action & TOKEN_ACTION_ISNT_LEGENDARY)
    cards_data[created].subtype = 0;
  cards_data[created].color = (uint8_t)color;
  return created;
}

static int real_generate_token(int player, int int_id, token_generation_t *token, int delay_put_into_play){
	int card_added = add_card_to_hand( player, int_id );
	set_token(player, card_added, token);
	if (delay_put_into_play){
		get_card_instance(player, card_added)->state |= STATE_INVISIBLE;
		--hand_count[player];
	} else {
		put_into_play(player, card_added);
	}
	return card_added;
}

#ifndef SHANDALAR
static int generate_tokens_impl(token_generation_t *token, int int_id, int cycle_number)
{
  card_instance_t *effect_instance = NULL;
  if (token->eff_card == -1)
  {
    if (token->keep_track_of_tokens_generated || token->legacy == 1 ||
        (token->action & (TOKEN_ACTION_PUMP_TOUGHNESS | TOKEN_ACTION_PUMP_POWER |
                          TOKEN_ACTION_HASTE | TOKEN_ACTION_EQUIP)))
      backtrace_and_abort("functions/token_generation.c:392\ntoken->eff_card != -1 || "
                          "!(token->keep_track_of_tokens_generated > 0 || token->legacy == "
                          "1 || (token->action & (TOKEN_ACTION_EQUIP | TOKEN_ACTION_HASTE "
                          "| TOKEN_ACTION_PUMP_POWER | TOKEN_ACTION_PUMP_TOUGHNESS)))");
  }
  else if (token->keep_track_of_tokens_generated)
    effect_instance = get_card_instance(token->eff_player, token->eff_card);

  int token_player = token->t_player;
  int created = add_card_to_hand(token_player, int_id);
  set_token(token_player, created, token);
  card_instance_t *instance = get_card_instance(token_player, created);
  instance->state |= STATE_INVISIBLE;
  --hand_count[token_player];
  instance = get_card_instance(token->t_player, created);
  if (token->legacy == 1)
  {
    int legacy =
        create_targetted_legacy_effect(token->eff_player, token->eff_card,
                                       token->special_code_for_legacy, token->t_player, created);
    card_instance_t *legacy_instance = get_card_instance(token->eff_player, legacy);
    legacy_instance->eot_toughness = 0x300;
    legacy_instance->display_pic_csv_id = cards_data[int_id].id;
  }
  token_action_t action = token->action;
  if (action & TOKEN_ACTION_TAPPED)
    instance->state |= STATE_TAPPED;
  if (action & TOKEN_ACTION_ATTACKING_UNTAPPED)
  {
    instance->state |= STATE_ATTACKING;
    if (!(action & TOKEN_ACTION_SKIP_ATTACK_CHECK))
    {
      choose_who_attack(token->t_player, created);
      action = token->action;
    }
  }
  if (action & TOKEN_ACTION_BLOCKING)
  {
    card_instance_t *attacker = get_card_instance(1 - token->t_player, token->action_argument);
    uint8_t blocking = attacker->blocking;
    if (blocking == 255)
      blocking = (uint8_t)token->action_argument;
    instance->blocking = blocking;
    action = token->action;
    instance->state |= STATE_BLOCKING | STATE_UNKNOWN8000;
  }
  if ((action & TOKEN_ACTION_EQUIP) && cycle_number == 0)
  {
    equip_target_creature(token->eff_player, token->eff_card, token->t_player, created);
    action = token->action;
  }
  if ((action & TOKEN_ACTION_EQUIP_ANY) && cycle_number == 0)
  {
    target_definition_t td;
    default_target_definition(token->eff_player, token->eff_card, &td, 0xff);
    td.required_subtype = SUBTYPE_EQUIPMENT;
    td.illegal_abilities = 0;
    card_instance_t *source = get_card_instance(token->eff_player, token->eff_card);
    target_t saved_target = source->targets[0];
    if (can_target(&td) && new_pick_target(&td, "Select an equipment you control", 0, 0x80))
      equip_target_creature(source->targets[0].player, source->targets[0].card, token->t_player,
                            created);
    source->targets[0] = saved_target;
    action = token->action;
  }
  if (action & TOKEN_ACTION_DONT_COPY_TOKEN_SOURCE)
  {
    set_special_flags(token->t_player, created, 0x40000000);
    action = token->action;
  }
  if (action & (TOKEN_ACTION_PUMP_TOUGHNESS | TOKEN_ACTION_PUMP_POWER | TOKEN_ACTION_HASTE))
  {
    int toughness = action & TOKEN_ACTION_PUMP_TOUGHNESS ? token->action_argument : 0;
    int power = action & TOKEN_ACTION_PUMP_POWER ? token->action_argument : 0;
    pump_ability_until_eot(token->eff_player, token->eff_card, token->t_player, created, power,
                           toughness, 0, (action >> 2) & 4);
    action = token->action;
  }
  if (action & TOKEN_ACTION_ADD_SUBTYPE)
  {
    add_a_subtype_permanently(token->eff_player, token->eff_card, token->t_player, created,
                              token->action_argument);
    action = token->action;
  }
  if (action & TOKEN_ACTION_RECORD_SOURCE_ID)
    instance->damage_source_card = get_id(token->s_player, token->s_card);
  if (token->special_code_on_generation)
    token->special_code_on_generation(token, created, cycle_number);
  token_player = token->t_player;
  if (cycle_number < (int)token->keep_track_of_tokens_generated)
  {
    effect_instance->targets[cycle_number].player = token_player;
    effect_instance->targets[cycle_number].card = created;
  }
  put_into_play(token_player, created);
  action = token->action;
  if (action & TOKEN_ACTION_ADD_COUNTERS)
  {
    ++hack_silent_counters;
    add_counters(token->s_player, token->s_card, token->t_player, created,
                 (unsigned int)token->action_argument >> 16, token->action_argument & 0xffff);
    --hack_silent_counters;
    action = token->action;
  }
  if ((action & TOKEN_ACTION_BLOCKING) && in_play(token->t_player, created))
  {
    play_sound_effect(WAV_BLOCK2);
    if (event_flags & 0x100000)
      dispatch_trigger2(current_turn, TRIGGER_BLOCKER_CHOSEN, gs_prompt_blockerselection_0, 0, token->t_player,
                        created);
  }
  if (token->special_infos == 99)
  {
    int token_id = get_id(token->t_player, created);
    int duplicate = 0;
    for (int p = 0; p < 2; ++p)
      for (int c = active_cards_count[p] - 1; c >= 0; --c)
        if (in_play(p, c) && (token->t_player != p || created != c) && get_id(p, c) == token_id &&
            get_card_instance(p, c)->targets[14].player == 99)
        {
          kill_card(p, c, KILL_SACRIFICE);
          duplicate = 1;
        }
    if (duplicate)
      kill_card(token->t_player, created, KILL_SACRIFICE);
  }
  if (cycle_number < (int)token->keep_track_of_tokens_generated)
  {
    card_instance_t *source = get_card_instance(token->eff_player, token->eff_card);
    source->targets[cycle_number].player = token->t_player;
    source->targets[cycle_number].card = created;
  }
  return created;
}
#endif

/* ManalinkEh 2022: 0x26659d0; original functions/token_generation.c:503. */
void generate_token(token_generation_t *token)
{
  int iid = get_internal_card_id_from_csv_id(token->id);
  unsigned int quantity = token->qty;
  if (!is_what(-1, iid, TYPE_EFFECT))
  {
    int recipient = token->t_player;
    for (int p = 0; p < 2; ++p)
      for (int c = 0; c < active_cards_count[p]; ++c)
      {
        card_instance_t *instance = in_play(p, c);
        if (instance)
        {
          int id = cards_data[instance->internal_card_id].id;
          if (id == CARD_ID_PRIMAL_VIGOR || id == CARD_ID_SELESNYA_LOFT_GARDENS ||
              (recipient == p && (id == CARD_ID_DOUBLING_SEASON || id == CARD_ID_PARALLEL_LIVES ||
                                  id == CARD_ID_ANOINTED_PROCESSION)))
            quantity *= 2;
        }
      }
  }
  if ((int)quantity < 1)
    return;

  int group_id = token->id;
  struct
  {
    int id;
    unsigned int quantity;
  } groups[4] = {{group_id, quantity}, {-1, 0}, {-1, 0}, {-1, 0}};
  int recipient = token->t_player;
  int manufactors = 0, chatterfangs = 0, visitation = 0;
  for (int c = 0; c < active_cards_count[recipient]; ++c)
  {
    if (in_play(recipient, c) && !is_humiliated(token->t_player, c, AL_STATIC))
    {
      manufactors += get_id(token->t_player, c) == CARD_ID_ACADEMY_MANUFACTOR;
      chatterfangs += get_id(token->t_player, c) == CARD_ID_CHATTERFANG_SQUIRREL_GENERAL;
      if (get_id(token->t_player, c) == CARD_ID_DIVINE_VISITATION)
        visitation = 1;
    }
    recipient = token->t_player;
  }
  for (int remaining = manufactors; remaining > 0; --remaining)
  {
    switch (group_id)
    {
    case CARD_ID_CLUE:
      groups[1].id = CARD_ID_FOOD;
      groups[2].id = CARD_ID_TREASURE;
      break;
    case CARD_ID_FOOD:
      groups[1].id = CARD_ID_CLUE;
      groups[2].id = CARD_ID_TREASURE;
      break;
    case CARD_ID_TREASURE:
      groups[1].id = CARD_ID_FOOD;
      groups[2].id = CARD_ID_CLUE;
      break;
    default:
      continue;
    }
    if (remaining > 1)
      groups[0].quantity += quantity;
    groups[1].quantity += quantity;
    groups[2].quantity += quantity;
  }
  if (chatterfangs)
  {
    groups[3].id = CARD_ID_SQUIRREL;
    groups[3].quantity = (unsigned int)chatterfangs * quantity;
  }
  for (int group = 0; group < 4; ++group)
  {
    group_id = groups[group].id;
    if (group_id < 0)
      continue;
    if (visitation)
    {
      int original_iid = get_internal_card_id_from_csv_id(group_id);
      if (cards_data[original_iid].cc[2] != 3 && cards_data[original_iid].type != TYPE_EFFECT)
        group_id = groups[group].id = CARD_ID_ANGEL;
    }
    iid = get_internal_card_id_from_csv_id(group_id);
    int group_quantity = groups[group].quantity;
    for (int n = 0; n < group_quantity; ++n)
    {
      if (group > 0)
      {
        default_token_definition(token->s_player, token->s_card, group_id, token);
        if (group_id == CARD_ID_SQUIRREL)
        {
          token->tou = 1;
          token->pow = 1;
          token->color_forced = 8;
        }
      }
      if (visitation)
      {
        int original_iid = get_internal_card_id_from_csv_id(group_id);
        if (cards_data[original_iid].cc[2] != 3 && cards_data[original_iid].type != TYPE_EFFECT)
        {
          token->key_plus = 0x20;
          token->s_key_plus = 8;
          token->tou = 4;
          token->pow = 4;
          token->color_forced = 0x20;
          token->special_infos = 0;
        }
      }
      iid = resolve_token_iid(iid, token);
      card_instance_t *effect_instance = NULL;
      if (token->eff_card == -1)
      {
        if (token->keep_track_of_tokens_generated || token->legacy == 1 ||
            (token->action & (TOKEN_ACTION_PUMP_TOUGHNESS | TOKEN_ACTION_PUMP_POWER |
                              TOKEN_ACTION_HASTE | TOKEN_ACTION_EQUIP)))
          backtrace_and_abort("functions/token_generation.c:392\ntoken->eff_card != -1 || "
                              "!(token->keep_track_of_tokens_generated > 0 || token->legacy == "
                              "1 || (token->action & (TOKEN_ACTION_EQUIP | TOKEN_ACTION_HASTE "
                              "| TOKEN_ACTION_PUMP_POWER | TOKEN_ACTION_PUMP_TOUGHNESS)))");
      }
      else if (token->keep_track_of_tokens_generated)
        effect_instance = get_card_instance(token->eff_player, token->eff_card);

      int token_player = token->t_player;
      int created = add_card_to_hand(token_player, iid);
      set_token(token_player, created, token);
      card_instance_t *instance = get_card_instance(token_player, created);
      instance->state |= STATE_INVISIBLE;
      --hand_count[token_player];
      instance = get_card_instance(token->t_player, created);
      if (token->legacy == 1)
      {
        int legacy = create_targetted_legacy_effect(token->eff_player, token->eff_card,
                                                    token->special_code_for_legacy, token->t_player,
                                                    created);
        card_instance_t *legacy_instance = get_card_instance(token->eff_player, legacy);
        legacy_instance->eot_toughness = 0x300;
        legacy_instance->display_pic_csv_id = cards_data[iid].id;
      }
      token_action_t action = token->action;
      if (action & TOKEN_ACTION_TAPPED)
        instance->state |= STATE_TAPPED;
      if (action & TOKEN_ACTION_ATTACKING_UNTAPPED)
      {
        instance->state |= STATE_ATTACKING;
        if (!(action & TOKEN_ACTION_SKIP_ATTACK_CHECK))
        {
          choose_who_attack(token->t_player, created);
          action = token->action;
        }
      }
      if (action & TOKEN_ACTION_BLOCKING)
      {
        card_instance_t *attacker = get_card_instance(1 - token->t_player, token->action_argument);
        uint8_t blocking = attacker->blocking;
        if (blocking == 255)
          blocking = (uint8_t)token->action_argument;
        instance->blocking = blocking;
        action = token->action;
        instance->state |= STATE_BLOCKING | STATE_UNKNOWN8000;
      }
      if ((action & TOKEN_ACTION_EQUIP) && n == 0)
      {
        equip_target_creature(token->eff_player, token->eff_card, token->t_player, created);
        action = token->action;
      }
      if ((action & TOKEN_ACTION_EQUIP_ANY) && n == 0)
      {
        target_definition_t td;
        default_target_definition(token->eff_player, token->eff_card, &td, 0xff);
        td.required_subtype = SUBTYPE_EQUIPMENT;
        td.illegal_abilities = 0;
        card_instance_t *source = get_card_instance(token->eff_player, token->eff_card);
        target_t saved_target = source->targets[0];
        if (can_target(&td) && new_pick_target(&td, "Select an equipment you control", 0, 0x80))
          equip_target_creature(source->targets[0].player, source->targets[0].card, token->t_player,
                                created);
        source->targets[0] = saved_target;
        action = token->action;
      }
      if (action & TOKEN_ACTION_DONT_COPY_TOKEN_SOURCE)
      {
        set_special_flags(token->t_player, created, 0x40000000);
        action = token->action;
      }
      if (action & (TOKEN_ACTION_PUMP_TOUGHNESS | TOKEN_ACTION_PUMP_POWER | TOKEN_ACTION_HASTE))
      {
        int toughness = action & TOKEN_ACTION_PUMP_TOUGHNESS ? token->action_argument : 0;
        int power = action & TOKEN_ACTION_PUMP_POWER ? token->action_argument : 0;
        pump_ability_until_eot(token->eff_player, token->eff_card, token->t_player, created, power,
                               toughness, 0, (action >> 2) & 4);
        action = token->action;
      }
      if (action & TOKEN_ACTION_ADD_SUBTYPE)
      {
        add_a_subtype_permanently(token->eff_player, token->eff_card, token->t_player, created,
                                  token->action_argument);
        action = token->action;
      }
      if (action & TOKEN_ACTION_RECORD_SOURCE_ID)
        instance->damage_source_card = get_id(token->s_player, token->s_card);
      if (token->special_code_on_generation)
        token->special_code_on_generation(token, created, n);
      token_player = token->t_player;
      if (n < (int)token->keep_track_of_tokens_generated)
      {
        effect_instance->targets[n].player = token_player;
        effect_instance->targets[n].card = created;
      }
      put_into_play(token_player, created);
      action = token->action;
      if (action & TOKEN_ACTION_ADD_COUNTERS)
      {
        ++hack_silent_counters;
        add_counters(token->s_player, token->s_card, token->t_player, created,
                     (unsigned int)token->action_argument >> 16, token->action_argument & 0xffff);
        --hack_silent_counters;
        action = token->action;
      }
      if ((action & TOKEN_ACTION_BLOCKING) && in_play(token->t_player, created))
      {
        play_sound_effect(WAV_BLOCK2);
        if (event_flags & 0x100000)
          dispatch_trigger2(current_turn, TRIGGER_BLOCKER_CHOSEN, gs_prompt_blockerselection_0, 0,
                            token->t_player, created);
      }
      if (token->special_infos == 99)
      {
        int token_id = get_id(token->t_player, created);
        int duplicate = 0;
        for (int p = 0; p < 2; ++p)
          for (int c = active_cards_count[p] - 1; c >= 0; --c)
            if (in_play(p, c) && (token->t_player != p || created != c) &&
                get_id(p, c) == token_id && get_card_instance(p, c)->targets[14].player == 99)
            {
              kill_card(p, c, KILL_SACRIFICE);
              duplicate = 1;
            }
        if (duplicate)
          kill_card(token->t_player, created, KILL_SACRIFICE);
      }
      if (n < (int)token->keep_track_of_tokens_generated)
      {
        card_instance_t *source = get_card_instance(token->eff_player, token->eff_card);
        source->targets[n].player = token->t_player;
        source->targets[n].card = created;
      }
    }
    increase_trap_condition(token->t_player, 0x206, group_quantity);
  }
  if (token->keep_track_of_tokens_generated)
    token->keep_track_of_tokens_generated = token->keep_track_of_tokens_generated < quantity
                                                ? token->keep_track_of_tokens_generated
                                                : quantity;
}

/* ManalinkEh 2022: 0x2666640; original functions/token_generation.c:614. */
static void copy_token_part(int player, int card, int tok_player, int tok_card)
{
  token_generation_t token;
  copy_token_definition(player, card, &token, tok_player, tok_card);
  generate_token(&token);
  return;
}

/* ManalinkEh 2022: 0x2666680; original functions/token_generation.c:614. */
void copy_token(int player, int card, int tok_player, int tok_card)
{
  if ((-1 < tok_player) && (-1 < tok_card))
  {
    copy_token_part(player, card, tok_player, tok_card);
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x2666580; original functions/token_generation.c:624. */
static int generate_token_by_id_impl(int player, int card, int csvid, int number, int reserved)
{
  int last_card;
  int iid;
  int i;
  token_generation_t token;

  default_token_definition(player, card, csvid, &token);
  if (reserved == 0)
  {
    last_card = -1;
    token.qty = (unsigned char)number;
    generate_token(&token);
  }
  else
  {
    token.no_sleight = '\x01';
    iid = resolve_token_iid(get_internal_card_id_from_csv_id(csvid), &token);
    if (number < 1)
    {
      return -1;
    }
    i = 0;
    do
    {
      i = i + 1;
      last_card = add_card_to_hand(player, iid);
      set_token(player, last_card, &token);
      put_into_play(player, last_card);
    } while (number != i);
  }
  return last_card;
}

/* ManalinkEh 2022: 0x26666b0; original functions/token_generation.c:649. */
int generate_token_by_id(int player, int card, int csvid)
{
  int result;
  unsigned int reserved;

  result = get_internal_card_id_from_csv_id(csvid);
  if (cards_data[result].cc[2] == '\x03')
  {
    reserved = 1;
  }
  else
  {
    reserved = (unsigned int)(cards_data[result].type == 0x80);
  }
  result = generate_token_by_id_impl(player, card, csvid, 1, reserved);
  return result;
}

/* ManalinkEh 2022: 0x2666720; original functions/token_generation.c:653. */
void generate_tokens_by_id(int player, int card, int csvid, int howmany)
{
  int result;
  unsigned int reserved;

  result = get_internal_card_id_from_csv_id(csvid);
  if (cards_data[result].cc[2] == '\x03')
  {
    reserved = 1;
  }
  else
  {
    reserved = (unsigned int)(cards_data[result].type == 0x80);
  }
  if (0 < howmany)
  {
    generate_token_by_id_impl(player, card, csvid, howmany, reserved);
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x2666790; original functions/token_generation.c:657. */
int generate_reserved_token_by_id(int player, int csvid)
{
  int result;

  result = get_internal_card_id_from_csv_id(csvid);
  if ((cards_data[result].cc[2] != '\x03') && (cards_data[result].type != 0x80))
  {
    backtrace_and_abort("functions/token_generation.c:658\nis_reserved_id(csvid)");
  }
  return generate_token_by_id_impl(player, -1, csvid, 1, 1);
}

void copy_all_tokens(int player, int card, int type, test_definition_t *test)
{
  int c, marked[151] = {0};
  for (c = 0; c < active_cards_count[player]; ++c)
  {
    if (in_play(player, c) && is_token(player, c))
    {
      if ((type && is_what(player, c, type)) ||
          (test != NULL && new_make_test_in_play(player, c, -1, test)))
      {
        marked[c] = 1;
      }
    }
  }

  for (c = active_cards_count[player] - 1; c > -1; c--)
  {
    if (marked[c] && player >= 0)
    {
      copy_token_part(player, card, player, c);
    }
  }
}

/* ManalinkEh 2022: 0x26668f0; original functions/token_generation.c:680. */
void copy_all_tokens_both_players(int player, int card, int type, test_definition_t *test)
{
  for (int pass = 0; pass < 2; ++pass)
  {
    int p = pass ? 1 - current_turn : current_turn;
    int limit = active_cards_count[p];
    if (limit > 0)
    {
      int marked[151] = {0};
      for (int c = 0; c < limit; ++c)
      {
        card_instance_t *instance = in_play(p, c);
        if (instance && is_token(p, c) &&
            ((type && is_what(p, c, type)) || (test && new_make_test_in_play(p, c, -1, test))))
          marked[c] = 1;
        limit = active_cards_count[p];
      }
      for (int c = limit - 1; c >= 0; --c)
        if (marked[c] && p >= 0)
          copy_token_part(p, c, p, c);
    }
  }
}

static int from_subtype_to_token_id(int subtype)
{
  /* Native signed 16-bit token IDs, indexed by creature subtype minus two. */
  static const int16_t token_ids[221] = {
      18687, 0,     0,     18689, 0,     0,     0,     18691, 0,     0,     0,     18692, 0,
      0,     0,     18693, 18694, 18695, 0,     0,     18696, 0,     18698, 0,     0,     18701,
      0,     18702, 0,     18704, 18708, 0,     0,     18709, 18710, 0,     18712, 0,     0,
      0,     0,     0,     18714, 18715, 18716, 0,     18719, 18721, 0,     0,     0,     0,
      0,     0,     0,     18722, 18726, 18729, 0,     0,     0,     18735, 0,     18738, 0,
      0,     0,     0,     18740, 18741, 18742, 18744, 18745, 18746, 18752, 0,     18753, 0,
      18755, 0,     18756, 18757, 18758, 0,     0,     18759, 18761, 0,     18763, 18764, 18768,
      0,     18769, 0,     0,     18770, 0,     0,     18773, 0,     0,     18776, 0,     0,
      18780, 0,     0,     0,     0,     0,     18783, 0,     0,     0,     18785, 0,     18787,
      18788, 0,     0,     18789, 0,     0,     18792, 0,     0,     0,     0,     0,     0,
      0,     0,     18794, 18795, 18796, 18797, 0,     0,     0,     0,     0,     18798, 18799,
      18800, 0,     0,     18801, 0,     18803, 0,     18804, 0,     18806, 0,     18807, 18808,
      0,     0,     0,     0,     0,     18813, 18814, 0,     0,     0,     18815, 0,     0,
      0,     18817, 18818, 0,     18819, 0,     18820, 0,     18821, 18822, 0,     18824, 0,
      0,     18826, 18827, 18828, 18829, 18831, 0,     18832, 18833, 18834, 18836, 0,     0,
      0,     18837, 18838, 18841, 18844, 0,     0,     0,     18848, 0,     0,     0,     0,
      18852, 18853, 0,     18854, 0,     18856, 0,     0,     18858, 0,     0,     0,     18860,
  };
  unsigned int index = (unsigned int)subtype - 2;
  return index < 221 ? token_ids[index] : 0;
}

static void store_selected_subtype(token_generation_t* token, int beast, int unused)
{
	get_card_instance(token->t_player, beast)->targets[1].card = token->action_argument;
}

/* ManalinkEh 2022: 0x2664e30; original functions/token_generation.c:945. */
static int legacy_negate_static_abilities(int player, int card, event_t event)
{
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  if (event == EVENT_ABILITIES)
  {
    if ((instance->damage_target_card == affected_card) &&
        (instance->damage_target_player == affected_card_controller))
    {
      event_result = event_result & 0xffffffdf;
    }
  }
  else if (((event == EVENT_CHANGE_TYPE) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    instance->regen_status = instance->regen_status | 0x1f000000;
  }
  return 0;
}

void generate_custom_token_from_subtype(int player, int card, int subtype, int color, int pow,
                                        int tou, int amount)
{
  int csvid = from_subtype_to_token_id(subtype);
  token_generation_t token;
  if (csvid)
  {
    default_token_definition(player, card, csvid, &token);
    int iid = get_internal_card_id_from_csv_id(csvid);
    if (cards_data[iid].static_ability)
    {
      token.legacy = 1;
      token.special_code_for_legacy = legacy_negate_static_abilities;
    }
  }
  else
  {
    default_token_definition(player, card, CARD_ID_RIPTIDE_REPLICATOR_TOKEN, &token);
    token.action = TOKEN_ACTION_ADD_SUBTYPE;
    token.special_code_on_generation = store_selected_subtype;
    token.action_argument = subtype;
  }
  token.qty = amount;
  token.color_forced = color;
  token.pow = pow;
  token.tou = tou;
  token.no_sleight = 1;
  generate_token(&token);
}
