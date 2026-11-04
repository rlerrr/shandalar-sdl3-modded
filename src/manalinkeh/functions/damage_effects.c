#include "manalink.h"

void damage_effects(int player, int card, event_t event)
{
  enum
  {
    MODE_NONE = 0,
    MODE_FREEZE_WHEN_DAMAGE = 128
  };
  enum
  {
    GLOBALMODE_NONE = 0,
    GLOBALMODE_SHRIVELING_ROT = 64
  };

  if (event != EVENT_DEAL_DAMAGE)
    return;

  card_instance_t *instance = get_card_instance(player, card);
  card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
  if (damage->internal_card_id != damage_card)
    return;

  int id = get_id(player, card);
  int exile_damaged = check_for_special_ability(player, card, 0x1000000);
  bool tracks_damaged_creatures = id == CARD_ID_KUMANO_MASTER_YAMABUSHI || exile_damaged != 0;
  bool destroys_damaged_creatures = false;
  if (is_what(player, card, TYPE_EFFECT))
    destroys_damaged_creatures = instance->targets[2].card == CARD_ID_SHRIVELING_ROT;

  int source_player = damage->damage_source_player;
  if (source_player == player && damage->damage_source_card == card && damage->info_slot > 0)
  {
    if (damage->damage_target_card == -1)
    {
      if (id == CARD_ID_SZADEK_LORD_OF_SECRETS &&
          (damage->token_status & (STATUS_FIRST_STRIKE_DAMAGE | STATUS_COMBAT_DAMAGE)))
      {
        add_counters(player, card, player, card, COUNTER_P1_P1, damage->info_slot);
        mill(damage->damage_target_player, damage->info_slot);
        damage->info_slot = 0;
      }
      return;
    }
    if (!tracks_damaged_creatures ||
        !in_play(damage->damage_target_player, damage->damage_target_card))
      return;
    int slot = instance->targets[1].player;
    if (slot < 2)
      instance->targets[1].player = slot = 2;
    else if (slot > 9)
      return;
    instance->targets[slot].player = damage->damage_target_player;
    instance->targets[slot].card = damage->damage_target_card;
    ++instance->targets[1].player;
    return;
  }

  if (id == CARD_ID_DRALNU_LICH_LORD)
  {
    if (damage->damage_target_player == player && damage->damage_target_card == card)
    {
      int amount = damage->info_slot;
      if (amount < 1)
        amount = get_card_instance(source_player, damage->damage_source_card)->targets[16].player;
      if (amount > 0)
      {
        if (instance->targets[9].player < 0)
          instance->targets[9].player = 0;
        instance->targets[9].player += amount;
      }
      damage->info_slot = 0;
    }
  }
  else if (id == CARD_ID_LIVING_ARTIFACT)
  {
    if (damage->damage_target_player == player && damage->damage_target_card == -1)
      add_counters(player, card, player, card, COUNTER_VITALITY, damage->info_slot);
  }
  else if (id == CARD_ID_WAR_ELEMENTAL)
  {
    if (damage->damage_target_player == 1 - player && damage->damage_target_card == -1)
      add_counters(player, card, player, card, COUNTER_P1_P1, damage->info_slot);
  }
  else if (id == CARD_ID_NIGHT_DEALINGS && damage->damage_target_player == 1 - player &&
           source_player == player)
    add_counters(player, card, player, card, COUNTER_THEFT, damage->info_slot);

  if (is_what(player, card, TYPE_EFFECT) && instance->targets[2].card == CARD_ID_DRAIN_LIFE &&
      damage->damage_target_player == instance->targets[0].player &&
      damage->damage_target_card == instance->targets[0].card)
  {
    int amount = damage->info_slot;
    int target_player = damage->damage_target_player;
    int target_card = damage->damage_target_card;
    if (target_card == -1)
    {
      if (life[target_player] < amount)
        amount = life[target_player];
    }
    else if (get_toughness(target_player, target_card) < amount)
      amount = get_toughness(instance->targets[0].player, instance->targets[0].card);
    gain_life(player, amount);
    kill_card(player, card, KILL_EXILE);
  }
  if (is_what(player, card, TYPE_EFFECT) && instance->targets[2].card == CARD_ID_BRIGHTFLAME &&
      get_id(damage->damage_source_player, damage->damage_source_card) == CARD_ID_BRIGHTFLAME)
    gain_life(player, damage->info_slot);

  if (is_what(player, card, TYPE_EFFECT) && instance->targets[1].card == CARD_ID_SIMIC_BASILISK &&
      damage->damage_source_player == instance->targets[0].player &&
      damage->damage_source_card == instance->targets[0].card && damage->info_slot > 0 &&
      damage->damage_target_card != -1)
  {
    int target_card = damage->damage_target_card;
    int slot = instance->targets[1].player;
    if (slot < 2)
      instance->targets[1].player = slot = 2;
    if (slot <= 9)
    {
      instance->targets[slot].player = damage->damage_target_player;
      instance->targets[slot].card = target_card;
      ++instance->targets[1].player;
    }
  }

  if ((id == CARD_ID_SPROUTING_PHYTOHYDRA || id == CARD_ID_VOLATILE_RIG ||
       id == CARD_ID_DEEP_SLUMBER_TITAN) &&
      damage->damage_target_player == player)
  {
    if (damage->damage_target_card == card && damage->info_slot > 0)
      instance->targets[1].player =
          instance->targets[1].player < 0 ? 1 : instance->targets[1].player + 1;
  }
  else if (id == CARD_ID_CIRCLE_OF_AFFLICTION)
  {
    if (damage->damage_target_player == player && damage->damage_target_card == -1 &&
        damage->info_slot > 0)
    {
      unsigned int source_color =
          get_color(damage->damage_source_player, damage->damage_source_card);
      if (instance->info_slot & source_color)
      {
        if (instance->targets[1].player < 0)
          instance->targets[1].player = 0;
        ++instance->targets[1].player;
      }
    }
  }
  else if (id == CARD_ID_GREATBOW_DOYEN)
  {
    if (damage->damage_source_player == player && damage->damage_target_card != -1 &&
        in_play(damage->damage_target_player, damage->damage_target_card) &&
        has_subtype(damage->damage_source_player, damage->damage_source_card, SUBTYPE_ARCHER))
    {
      int slot = instance->targets[1].player;
      if (slot < 2)
        instance->targets[1].player = slot = 2;
      if (slot <= 9)
      {
        instance->targets[slot].player = damage->damage_target_player;
        instance->targets[slot].card = damage->info_slot;
        ++instance->targets[1].player;
      }
    }
  }
  else if (id == CARD_ID_OONAS_BLACKGUARD)
  {
    if (damage->damage_source_player == player && damage->damage_target_card == -1 &&
        in_play(player, damage->damage_source_card) &&
        count_counters(damage->damage_source_player, damage->damage_source_card, COUNTER_P1_P1) >
            0 &&
        (damage->token_status & (STATUS_FIRST_STRIKE_DAMAGE | STATUS_COMBAT_DAMAGE)))
    {
      if (instance->targets[1].player < 0)
        instance->targets[1].player = 0;
      ++instance->targets[1].player;
    }
  }
  else if (id == CARD_ID_DEUS_OF_CALAMITY && damage->damage_source_player == player &&
           damage->damage_source_card == card && damage->damage_target_card == -1 &&
           damage->info_slot > 5)
    instance->targets[1].player = damage->damage_target_player + 2;

  if (is_what(player, card, TYPE_EFFECT) &&
      (instance->targets[2].card == CARD_ID_RUNESWORD ||
       instance->targets[2].card == CARD_ID_BONE_SHAMAN) &&
      damage->damage_source_player == instance->damage_source_player &&
      damage->damage_source_card == instance->damage_source_card &&
      damage->damage_target_card >= 0 &&
      in_play(damage->damage_target_player, damage->damage_target_card))
  {
    int slot = instance->targets[2].player;
    if (slot < 3)
      instance->targets[2].player = slot = 3;
    if (slot <= 9)
    {
      instance->targets[slot].player = damage->damage_target_player;
      instance->targets[slot].card = damage->damage_target_card;
      ++instance->targets[2].player;
    }
  }
  if (destroys_damaged_creatures && damage->damage_target_card != -1 &&
      in_play(damage->damage_target_player, damage->damage_target_card))
    kill_card(damage->damage_target_player, damage->damage_target_card, KILL_DESTROY);
}

int damage_creature(int tgt_player, int tgt_card, int32_t amt, int src_player, int src_card)
{
  if (src_player == -1 || amt < 1 || tgt_player == -1)
    return -1;
  char target_player = (char)tgt_player;
  uint32_t damage_slot;
  int damage_player;
  if (tgt_card == -1)
  {
    damage_slot = add_card_to_hand(tgt_player, damage_card);
    if (damage_slot == UINT32_MAX)
      return -1;
    land_can_be_played |= 2;
    damage_player = tgt_player;
  }
  else
  {
    damage_slot = add_card_to_hand(src_player, damage_card);
    if (damage_slot == UINT32_MAX)
      return -1;
    land_can_be_played |= 2;
    card_instance_t *target = get_card_instance(tgt_player, tgt_card);
    damage_player = src_player;
    if ((target->token_status & 0x8000000) &&
        cards_data[target->internal_card_id].code_pointer == MANALINKEH_CARD_MULTIBLOCKER)
    {
      target_player = target->damage_source_player;
      tgt_player = target_player;
      tgt_card = target->damage_source_card;
    }
  }
  card_instance_t *damage = get_card_instance(damage_player, damage_slot);
  damage->damage_source_player = (int8_t)src_player;
  damage->state |= STATE_IN_PLAY;
  --hand_count[damage_player];
  damage->damage_target_player = target_player;
  damage->damage_target_card = tgt_card;
  damage->info_slot = amt;
  damage->damage_source_card = src_card;
  if (src_card == -1)
  {
    damage->display_pic_csv_id = 0xef;
    damage->display_pic_num = 0;
    return damage_slot;
  }

  card_instance_t *source = get_card_instance(src_player, src_card);
  int iid = source->internal_card_id;
  type_t source_type;
  card_instance_t *keyword_source;
  unsigned int source_state = 0;
  unsigned int image_data = 0;
  bool ordinary_source;
  if (iid == -1)
  {
    uint16_t backup_iid = source->backup_internal_card_id;
    source_type = get_type_with_iid(src_player, src_card, backup_iid);
    source_state = source->state;
    image_data = (uint16_t)cards_data[backup_iid].id |
                 0;
    ordinary_source = true;
  }
  else if (iid == activation_card)
  {
    uint32_t original_iid = source->original_internal_card_id;
    keyword_source = get_card_instance(source->parent_controller, source->parent_card);
    source_type = get_type_with_iid(source->parent_controller, source->parent_card, original_iid);
    damage->display_pic_csv_id = source->display_pic_csv_id;
    damage->display_pic_num = source->display_pic_num;
    damage->damage_source_player = (int8_t)source->parent_controller;
    damage->damage_source_card = source->parent_card;
    ordinary_source = false;
  }
  else
  {
    uint16_t id = cards_data[iid].id;
    ordinary_source = (uint16_t)(id - 0x386) > 1 && id != 0x38b;
    if (ordinary_source)
    {
      source_type = get_type_with_iid(src_player, src_card, iid);
      source_state = source->state;
      image_data =
          (uint16_t)cards_data[iid].id | 0;
    }
    else
    {
      uint32_t original_iid = source->original_internal_card_id;
      keyword_source = get_card_instance(source->damage_source_player, source->damage_source_card);
      source_type =
          get_type_with_iid(source->damage_source_player, source->damage_source_card, original_iid);
      damage->display_pic_csv_id = source->display_pic_csv_id;
      damage->display_pic_num = source->display_pic_num;
    }
  }
  if (ordinary_source)
  {
    source_state &= STATE_ATTACKING;
    damage->display_pic_csv_id = (uint16_t)image_data;
    damage->display_pic_num =
        (uint16_t)get_card_image_number(image_data & 0xffff, src_player, src_card);
  }
  unsigned char color = (unsigned char)get_color(src_player, src_card);
  if (ordinary_source)
    keyword_source = source;
  if (source_type & TYPE_ARTIFACT)
    color |= 0x40;
  damage->color = color;
  damage->initial_color = color;
  damage->eot_toughness = damage_slot;
  if (ordinary_source && source_state)
  {
    unsigned int status = damage->token_status;
    if (current_phase == PHASE_FIRST_STRIKE_DAMAGE)
      damage->token_status = status | STATUS_FIRST_STRIKE_DAMAGE;
    else if (current_phase == PHASE_NORMAL_COMBAT_DAMAGE)
    {
      status |= STATUS_COMBAT_DAMAGE;
      damage->token_status = status;
    }
    if ((current_phase == PHASE_FIRST_STRIKE_DAMAGE ||
         (status & (STATUS_FIRST_STRIKE_DAMAGE | STATUS_COMBAT_DAMAGE))) &&
        check_special_flags(src_player, src_card, SF_ATTACKING_PWALKER))
    {
      damage->targets[4].player = 1 - src_player;
      damage->targets[4].card = planeswalker_its_attacking(src_player, src_card);
    }
  }
  type_t target_type = TYPE_NONE;
  if (damage->damage_target_card < 0)
  {
    damage->targets[3].card = 0;
    damage->targets[3].player = source_type;
  }
  else
  {
    card_instance_t *target =
        get_card_instance(damage->damage_target_player, damage->damage_target_card);
    damage->targets[3].player = source_type;
    damage->targets[3].card = 0;
    if (target)
      target_type = get_type(damage->damage_target_player, damage->damage_target_card);
  }
  damage->targets[5].card = target_type;
  damage->regen_status = keyword_source->regen_status & 0x60ffffff;
  damage->ext->sp_keywords = keyword_source->ext->sp_keywords;
  if (!(source_type & TYPE_PERMANENT) && !is_planeswalker(src_player, src_card))
    for (int c = 0; c < active_cards_count[src_player]; ++c)
      if (in_play(src_player, c) && !is_humiliated(src_player, c, AL_STATIC))
      {
        bool grants_lifelink = get_id(src_player, c) == CARD_ID_SOULFIRE_GRAND_MASTER ||
                               get_id(src_player, c) == CARD_ID_RADIANT_SCROLLWIELDER;
        if (!grants_lifelink && get_id(src_player, c) == CARD_ID_FIRESONG_AND_SUNSPEAKER)
        {
          unsigned int source_color = get_color(src_player, src_card);
          unsigned int sleighted_red = get_sleighted_color_test(src_player, c, COLOR_TEST_RED);
          grants_lifelink = (source_color & sleighted_red) != 0;
        }
        if (grants_lifelink)
        {
          damage->ext->sp_keywords |= SP_KEYWORD_LIFELINK;
          break;
        }
      }
  damage->targets[7].player = 0;
  if (tgt_card >= 0)
  {
    unsigned int count = source->number_of_targets;
    if (count != 9 && source->number_of_targets > 8)
      count = 9;
    target_t *target = source->targets + count;
    // The native scan also visits the record prefix before targets[0].
    do
    {
      if (target->player == tgt_player && target->card == tgt_card)
      {
        damage->targets[7].player = 1;
        break;
      }
      --target;
    } while (target != (target_t *)&source->internal_card_id);
  }
  if (ordinary_source && is_being_cast(src_player, src_card))
    damage->targets[7].player |= 2;
  return damage_slot;
}

// Convenient frontend to damage_creature().  Deals dmg damage to targets[0].player/card.  Promotes to parent if an activation card.  Does not validate.
int damage_target0(int player, int card, int dmg)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (instance->internal_card_id == activation_card)
	{
	  player = instance->parent_controller;
	  card = instance->parent_card;
	}
  return damage_creature(instance->targets[0].player, instance->targets[0].card, dmg, player, card);
}

static int remove_indestructible_until_eot(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t* instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_ABILITIES && affect_me(p, c) ){
			remove_special_ability(p, c, SP_KEYWORD_INDESTRUCTIBLE);
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int effect_damage(int player, int card, event_t event)
{
  uint16_t source_csvid;
  uint16_t previous_damage;
  short source_id;
  uint16_t current_csvid;
  int32_t redirected_player;
  card_instance_t *damage;
  unsigned int flags;
  int target_player;
  int result;
  int source_player;
  int amount;
  int damage_player;
  unsigned int data_bits;
  int damage_flag;
  int32_t value;
  card_instance_t *target_instance;

  damage_player = affected_card_controller;
  if ((land_can_be_played & 4U) != 0)
  {
    if (card != affected_card)
    {
      return 0;
    }
    if (player != affected_card_controller)
    {
      return 0;
    }
    damage = get_card_instance(player, card);
    if (damage != NULL)
    {
      result = damage->targets[4].player;
      bool use_original_target = result < 0 && damage->targets[4].card < 0;
      if (!use_original_target && damage->damage_target_player >= 0 &&
          damage->damage_target_card >= 0)
      {
        if (!is_planeswalker(damage->damage_target_player, damage->damage_target_card))
          use_original_target = true;
        else
          result = damage->targets[4].player;
      }
      if (use_original_target)
      {
        result = damage->damage_target_player;
        value = damage->damage_target_card;
      }
      else
        value = damage->targets[4].card;
      if (((((value != -1) && (flags = get_abilities(result, value, EVENT_ABILITIES, -1),
                               (flags & 0x35f800) != 0)) &&
            ((damage->state & 0x100000) == 0)) &&
           ((((((unsigned char)damage->initial_color & 0x3e & (int)flags >> 10) != 0 ||
               ((data_bits = damage->targets[3].player,
                 (flags & 0x200000) != 0 && ((data_bits & 2) != 0)))) ||
              (((flags & 0x10000) != 0 && ((data_bits & 0x40) != 0)))) ||
             ((((flags & 0x100000) != 0 && ((data_bits & 8) != 0)) ||
               (((flags & 0x40000) != 0 && ((data_bits & 0x30) != 0)))))))) ||
          ((result =
                has_protection_from_impl(result, value, damage_player, card, IPF_DAMAGE, KEYWORD_0),
            result != 0 && ((damage->state & 0x100000) == 0))))
      {
        damage->info_slot = 0;
        kill_card(damage_player, card, KILL_BURY);
      }
    }
  }
  if (event != EVENT_DEAL_DAMAGE)
  {
    return 0;
  }
  if (card != affected_card)
  {
    return 0;
  }
  if (player != affected_card_controller)
  {
    return 0;
  }
  damage = get_card_instance(player, card);
  if (damage == NULL)
  {
    return 0;
  }
  if ((damage->state & 0x10) != 0)
  {
    return 0;
  }
  amount = damage->info_slot;
  damage->state = damage->state | 0x10;
  damage->token_status = damage->token_status | 0x10000;
  if (amount < 1)
  {
    return 0;
  }
  int selected_player = damage->targets[4].player;
  int original_player = damage->damage_target_player;
  int selected_card = 0;
  bool use_original_target = selected_player < 0 || (selected_card = damage->targets[4].card) < 0;
  if (!use_original_target && damage->damage_target_player >= 0 && damage->damage_target_card >= 0)
  {
    if (!is_planeswalker(original_player, damage->damage_target_card))
    {
      original_player = damage->damage_target_player;
      use_original_target = true;
    }
    else
    {
      selected_card = damage->targets[4].card;
      selected_player = damage->targets[4].player;
    }
  }
  if (use_original_target)
  {
    selected_card = damage->damage_target_card;
    damage->targets[4].player = original_player;
    damage->targets[4].card = selected_card;
    target_instance = NULL;
    if (selected_card != -1)
    {
      target_instance = in_play(original_player, selected_card);
      if (!target_instance)
        return 0;
      damage->damage_target_player = (int8_t)damage->targets[4].player;
      damage->damage_target_card = damage->targets[4].card;
    }
  }
  else
  {
    target_instance = in_play(selected_player, selected_card);
    if (!target_instance || !is_planeswalker(damage->targets[4].player, damage->targets[4].card))
      return 0;
  }
  result = get_trap_condition(player, 0x205);
  if ((result < amount) || (result = get_trap_condition(1 - player, 0x205), result < amount))
  {
    set_trap_condition(player, 0x205, amount);
    set_trap_condition(1 - player, 0x205, amount);
  }
  flags = damage->ext->sp_keywords;
  if ((flags & 2) != 0)
  {
    gain_life_from_lifelink((int)damage->damage_source_player, amount);
    flags = damage->ext->sp_keywords;
  }
  if ((flags & 0x20000000) != 0)
  {
    generate_tokens_by_id((int)damage->damage_source_player, -1, CARD_ID_SQUIRREL, amount);
  }
  result = damage->targets[4].card;
  target_player = damage->targets[4].player;
  if (result != -1)
  {
    result = is_what(target_player, result, 2);
    if (result == 0)
    {
      if (is_planeswalker(damage->targets[4].player, damage->targets[4].card) != 0)
      {
        result = check_for_special_ability(player, card, 0x40000);
        value = 0;
        if (result != 0)
        {
          result =
              count_counters(damage->targets[4].player, damage->targets[4].card, COUNTER_LOYALTY);
          value = amount - result;
        }
        remove_counters(player, card, damage->targets[4].player, damage->targets[4].card,
                        COUNTER_LOYALTY, amount);
        if ((damage->targets[3].card & 0x80) != 0)
        {
          set_special_flags(damage->targets[4].player, damage->targets[4].card, 0x200);
          exile_if_would_be_put_into_graveyard(
              (int)damage->damage_source_player, damage->damage_source_card,
              damage->targets[4].player, damage->targets[4].card, 1);
        }
        if (check_for_special_ability(player, card, 0x40000) != 0)
        {
          damage->state = damage->state & 0xffffffef;
          damage->token_status = damage->token_status & 0xfffeffff;
          land_can_be_played = land_can_be_played | 2;
          redirected_player = damage->targets[4].player;
          damage->damage_target_card = -1;
          damage->damage_target_player = (int8_t)redirected_player;
          damage->targets[4].card = -1;
          damage->targets[4].player = -1;
          damage->targets[5].player = 0;
          damage->info_slot = value;
        }
        call_card_function(damage->targets[4].player, damage->targets[4].card,
                           EVENT_STATIC_EFFECTS);
      }
    }
    else
    {
      flags = damage->targets[3].card;
      if ((flags & 0x100) != 0)
      {
        pump_ability_until_eot((int)damage->damage_source_player, damage->damage_source_card,
                               damage->targets[4].player, damage->targets[4].card, 0, 0, 0, 0x400);
        flags = damage->targets[3].card;
      }
      if ((flags & 0x40) != 0)
      {
        tap_card(damage->targets[4].player, damage->targets[4].card);
        flags = damage->targets[3].card;
      }
      if ((flags & 0x80) != 0)
      {
        set_special_flags(damage->targets[4].player, damage->targets[4].card, 0x200);
        exile_if_would_be_put_into_graveyard((int)damage->damage_source_player,
                                             damage->damage_source_card, damage->targets[4].player,
                                             damage->targets[4].card, 1);
        flags = damage->targets[3].card;
      }
      if ((flags & 0x20) != 0)
      {
        cannot_regenerate_until_eot((int)damage->damage_source_player, damage->damage_source_card,
                                    damage->targets[4].player, damage->targets[4].card);
      }
      flags = damage->ext->sp_keywords;
      if ((flags & 0x200) != 0)
      {
        source_csvid = damage->display_pic_csv_id;
        if ((source_csvid == 0x1916) || (result = 1, source_csvid == 0x1b04))
        {
          result = (((damage->token_status & 0x140000) == 0) ? 1U : 8U);
        }
        set_special_flags(damage->targets[4].player, damage->targets[4].card, result);
        flags = damage->ext->sp_keywords;
      }
      if ((flags & 0x100000) != 0)
      {
        set_special_flags(damage->targets[4].player, damage->targets[4].card, 0x10);
      }
      value = damage->targets[10].card;
      if (value == 0x3c28)
      {
        if (get_color(damage->targets[4].player, damage->targets[4].card) == 0)
        {
          tap_card(damage->targets[4].player, damage->targets[4].card);
        }
        value = damage->targets[10].card;
      }
      if (value == 0x3d8f)
      {
        negate_ability_and_special_ability_until_eot(
            (int)damage->damage_source_player, damage->damage_source_card,
            damage->targets[4].player, damage->targets[4].card, KEYWORD_0, 0x800);
        value = damage->targets[10].card;
      }
      if (value == 0x441d)
      {
        pump_ability_until_eot_no_repeat((int)damage->damage_source_player,
                                         damage->damage_source_card, damage->targets[4].player,
                                         damage->targets[4].card, 0, 0x20);
        value = damage->targets[10].card;
      }
      if (value == 0x50eb)
      {
        flags = get_color(damage->targets[4].player, damage->targets[4].card);
        data_bits = get_sleighted_color_test((int)damage->damage_source_player,
                                             damage->damage_source_card, 0x10);
        if ((flags & data_bits) == 0)
        {
          test_definition_t test;
          new_default_test_definition(&test, 1, "Select a land to sacrifice.");
          new_sacrifice((int)damage->damage_source_player, damage->damage_source_card,
                        (int)damage->damage_source_player, SAC_NO_CANCEL, &test);
        }
      }
      if (((damage->ext->sp_keywords & 0x4000) == 0) && ((damage->regen_status & 0x20000000) == 0))
      {
        previous_damage = target_instance->damage_on_card;
        target_instance->damage_on_card = previous_damage + (short)amount;
      }
      else
      {
        hack_silent_counters = hack_silent_counters + 1;
        add_counters(player, card, damage->targets[4].player, damage->targets[4].card,
                     COUNTER_M1_M1, amount);
        hack_silent_counters = hack_silent_counters + -1;
      }
    }
    if (ai_is_speculating == 1)
    {
      return 0;
    }
    play_sound_effect(WAV_DAMAGE);
    request_duel_display_refresh();
    return 0;
  }
  source_player = (int)damage->damage_source_player;
  result = damage->damage_source_card;
  increase_trap_condition(target_player, 0, amount);
  flags = damage->targets[3].player;
  if ((flags & 0x40) != 0)
  {
    increase_trap_condition(target_player, 3, amount);
    flags = damage->targets[3].player;
  }
  if (flags & TYPE_PERMANENT)
  {
    if (target_player == 0 || target_player == 1)
    {
      damage_flag = target_player == 0 ? 0x4000000 : 0x8000000;
      set_special_flags2(source_player, result, damage_flag);
      flags = damage->targets[3].player;
    }
    if (((flags & 2) != 0) && ((damage->token_status & 0x140000) != 0))
    {
      increase_trap_condition(target_player, 0x605, 1);
      if (has_subtype(source_player, result, SUBTYPE_ROGUE) != 0)
      {
        increase_trap_condition(source_player, 0x601, 1);
      }
      if (has_subtype(source_player, result, SUBTYPE_GOBLIN) != 0)
      {
        increase_trap_condition(source_player, 0x602, 1);
      }
      if (has_subtype(source_player, result, SUBTYPE_FAERIE) != 0)
      {
        increase_trap_condition(source_player, 0x609, 1);
      }
      if (has_subtype(source_player, result, SUBTYPE_PIRATE) != 0)
      {
        increase_trap_condition(source_player, 0x40a, 1);
      }
    }
  }
  if (((source_player != target_player) && ((damage->initial_color & 0x10U) != 0)) &&
      ((damage->targets[3].player & 0x138U) != 0))
  {
    increase_trap_condition(source_player, 0x603, 1);
  }
  shandalar_player_damage_totals()[target_player] += amount;
  if ((damage->regen_status & 0x20000000) == 0)
  {
    result = life[target_player];
    amount = result - amount;
    if (amount < 7)
    {
      amount =
          dispatch_event_with_initial_event_result(player, card, EVENT_DAMAGE_REDUCTION, amount);
      result = life[target_player];
    }
    if (amount < result)
    {
      if ((card_flags[target_player] & CFLAGS_ARCHON_OF_CORONATION) == 0)
      {
        lose_life(target_player, result - amount);
      }
    }
    else if (result < amount)
    {
      gain_life(target_player, amount - result);
    }
  }
  else
  {
    poison(target_player, amount);
  }
  source_id = damage->display_pic_csv_id;
  if (source_id == 0x3af9)
  {
    token_generation_t token;
    if (damage->targets[10].card != 0x3af9)
    {
      return 0;
    }
    default_token_definition(player, card, CARD_ID_CHANDRA_ROARING_FLAME_EMBLEM, &token);
    token.t_player = target_player;
    generate_token(&token);
    current_csvid = damage->display_pic_csv_id;
    source_id = current_csvid;
  }
  if ((source_id == 0x3257) && (damage->targets[10].card == 0x3257))
  {
    amount = generate_reserved_token_by_id(player, CARD_ID_SPECIAL_EFFECT);
    damage = get_card_instance(player, amount);
    damage->targets[3].player = target_player;
    damage->targets[2].player = 0x80;
    damage->targets[2].card = 0x3257;
    create_card_name_legacy(player, amount, CARD_ID_AURELIAS_FURY);
  }
  return 0;
}

void after_damage(void)
{
  for (int player = 0; player < 2; ++player)
    for (int card = 0; card < active_cards_count[player]; ++card)
    {
      card_instance_t *instance = in_play(player, card);
      if (!instance || (!is_what(player, card, TYPE_CREATURE) && !is_planeswalker(player, card)))
        continue;
      unsigned int flags = check_special_flags(player, card, 0x219);
      if (flags)
        remove_special_flags(player, card, 0x219);
      kill_t kill_mode = 0;
      if (flags & 0x10)
        kill_mode = KILL_EXILE;
      else if (flags & 0x200)
      {
        instance = get_card_instance(player, card);
        unsigned short damage = instance->damage_on_card;
        if (get_toughness(player, card) <= (int)damage)
          kill_mode = KILL_EXILE;
      }
      if (!kill_mode)
      {
        if (flags & 8)
          kill_mode = KILL_BURY;
        else if (flags & 1)
          kill_mode = KILL_DESTROY;
        else
        {
          if (is_what(player, card, TYPE_CREATURE))
          {
            instance = get_card_instance(player, card);
            unsigned short damage = instance->damage_on_card;
            if (get_toughness(player, card) <= (int)damage)
              kill_mode = KILL_DESTROY;
          }
          if (!kill_mode && check_special_flags3(player, card, SF3_ARCHANGEL_OF_THUNE_COUNTER) &&
              is_what(player, card, TYPE_CREATURE))
          {
            instance = get_card_instance(player, card);
            uint32_t count = instance->eot_toughness;
            if (count)
            {
              instance = get_card_instance(player, card);
              instance->eot_toughness = 0;
              add_counters(player, card, player, card, COUNTER_P1_P1, count);
            }
          }
        }
      }
      if (kill_mode)
        kill_card(player, card, kill_mode);
    }
  for (int player = 0; player < 2; ++player)
    for (int card = 0; card < active_cards_count[player]; ++card)
      if (in_play(player, card))
        dispatch_event_with_attacker_to_one_card(player, card, EVENT_AFTER_DAMAGE, 1 - player, -1);
}

// Returns a damage card during EVENT_PREVENT_DAMAGE if it's combat damage and doing more than 0.
card_instance_t* combat_damage_being_prevented(event_t event)
{
  card_instance_t* damage;
  if (event == EVENT_PREVENT_DAMAGE
	  && (damage = get_card_instance(affected_card_controller, affected_card))
	  && damage->internal_card_id == damage_card
	  && (damage->token_status & (STATUS_COMBAT_DAMAGE|STATUS_FIRST_STRIKE_DAMAGE))
	  && damage->info_slot > 0)
	return damage;

  return NULL;
}

// Returns a damage card during EVENT_PREVENT_DAMAGE if it's noncombat damage and doing more than 0.
card_instance_t* noncombat_damage_being_prevented(event_t event)
{
  card_instance_t* damage;
  if (event == EVENT_PREVENT_DAMAGE
	  && (damage = get_card_instance(affected_card_controller, affected_card))
	  && damage->internal_card_id == damage_card
	  && !(damage->token_status & (STATUS_COMBAT_DAMAGE|STATUS_FIRST_STRIKE_DAMAGE))
	  && damage->info_slot > 0)
	return damage;

  return NULL;
}

// Returns a damage card during EVENT_PREVENT_DAMAGE if it's doing more than 0.
card_instance_t* damage_being_prevented(event_t event)
{
  card_instance_t* damage;
  if (event == EVENT_PREVENT_DAMAGE
	  && (damage = get_card_instance(affected_card_controller, affected_card))
	  && damage->internal_card_id == damage_card
	  && damage->info_slot > 0)
	return damage;

  return NULL;
}

/* Returns a damage card during EVENT_DEAL_DAMAGE if it's combat damage and doing more than 0.  damage->targets[16].player will hold the damage dealt if it's
 * determinable, even in the case of wither/infect damage; this is no more robust than the previous workarounds were. */
card_instance_t* combat_damage_being_dealt(event_t event)
{
  card_instance_t* damage;
  if (event == EVENT_DEAL_DAMAGE
	  && (damage = get_card_instance(affected_card_controller, affected_card))
	  && damage->internal_card_id == damage_card
	  && (damage->token_status & (STATUS_COMBAT_DAMAGE|STATUS_FIRST_STRIKE_DAMAGE)))
	{
	  if (damage->info_slot > 0)
		{
		  damage->targets[16].player = damage->info_slot;
		  return damage;
		}

	  card_instance_t* source = get_card_instance(damage->damage_source_player, damage->damage_source_card);
	  if (source->targets[16].player > 0)
		{
		  damage->targets[16].player = source->targets[16].player;
		  return damage;
		}
	}

  return NULL;
}

/* Returns a damage card during EVENT_DEAL_DAMAGE if it's noncombat damage and doing more than 0.  damage->targets[16].player will hold the damage dealt if it's
 * determinable, even in the case of wither/infect damage; this is no more robust than the previous workarounds were. */
card_instance_t* noncombat_damage_being_dealt(event_t event)
{
  card_instance_t* damage;
  if (event == EVENT_DEAL_DAMAGE
	  && (damage = get_card_instance(affected_card_controller, affected_card))
	  && damage->internal_card_id == damage_card
	  && !(damage->token_status & (STATUS_COMBAT_DAMAGE|STATUS_FIRST_STRIKE_DAMAGE)))
	{
	  if (damage->info_slot > 0)
		{
		  damage->targets[16].player = damage->info_slot;
		  return damage;
		}

	  card_instance_t* source = get_card_instance(damage->damage_source_player, damage->damage_source_card);
	  if (source->targets[16].player > 0)
		{
		  damage->targets[16].player = source->targets[16].player;
		  return damage;
		}
	}

  return NULL;
}

/* Returns a damage card during EVENT_DEAL_DAMAGE if it's doing more than 0.  damage->targets[16].player will hold the damage dealt if it's determinable, even
 * in the case of wither/infect damage; this is no more robust than the previous workarounds were. */
card_instance_t* damage_being_dealt(event_t event)
{
  card_instance_t* damage;
  if (event == EVENT_DEAL_DAMAGE
	  && (damage = get_card_instance(affected_card_controller, affected_card))
	  && damage->internal_card_id == damage_card)
	{
	  if (damage->info_slot > 0)
		{
		  damage->targets[16].player = damage->info_slot;
		  return damage;
		}

	  card_instance_t* source = get_card_instance(damage->damage_source_player, damage->damage_source_card);
	  if (source->targets[16].player > 0)
		{
		  damage->targets[16].player = source->targets[16].player;
		  return damage;
		}
	}

  return NULL;
}

// Returns nonzero if this damage card is being dealt to a planeswalker.
int damage_is_to_planeswalker(card_instance_t* damage)
{
  return (damage->damage_target_card == -1
		  ? ((damage->token_status & (STATUS_FIRST_STRIKE_DAMAGE|STATUS_COMBAT_DAMAGE))
			 && check_special_flags(damage->damage_source_player, damage->damage_source_card, SF_ATTACKING_PWALKER))
		  : is_planeswalker(damage->damage_target_player, damage->damage_target_card));
}

static int if_attached_creature_dies_do_something(int player, int card, event_t event)
{

  card_instance_t *instance = get_card_instance(player, card);
  int p = instance->damage_target_player;
  int c = instance->damage_target_card;

  int mode = instance->targets[1].card;
  // modes
  // 1<<0 --> Controller of source gains life equal to attached creature toughness [Abattoir Ghoul]
  // 1<<1 --> Controller of source may draw a card [Rot Wolf]

  if (p > -1)
  {
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {
      if (affect_me(p, c) && in_play(p, c) && get_card_instance(p, c)->kill_code > 0 &&
          get_card_instance(p, c)->kill_code < KILL_EXILE &&
          !check_special_flags2(p, c, SF2_WILL_BE_EXILED_IF_PUTTED_IN_GRAVE) &&
          !check_for_death_trigger_removal(p, c))
      {
        if (mode && (mode & 1))
        {
          int toughness = get_toughness(p, c);
          instance->targets[1].player = (card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1) * toughness;
        }
        instance->targets[11].player = 1; // Otherwise "resolve_gfp_ability" won't trigger
        instance->damage_target_player = instance->damage_target_card = -1;
      }
    }
  }

  // This check is mandatory or the legacy card will be removed before EVENT_GRAVEYARD_ABILITY if
  // source is killed in combat
  if (instance->targets[0].player > -1 && current_phase != PHASE_NORMAL_COMBAT_DAMAGE &&
      current_phase != PHASE_FIRST_STRIKE_DAMAGE)
  {
    other_leaves_play(player, card, instance->targets[0].player, instance->targets[0].card, event,
                      0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      kill_card(player, card, KILL_EXILE);
    }
  }

  if (resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    if (mode && (mode & 1) && instance->targets[1].player > 0)
    {
      gain_life(instance->targets[0].player, instance->targets[1].player);
    }
    if (mode && (mode & 2))
    {
      int amount = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
      do
      {
        if (do_dialog(instance->targets[0].player, player, card, -1, -1, " Draw a card\n Pass",
                      0) == 0)
          draw_cards(instance->targets[0].player, 1);
      } while (--amount != 0);
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (current_phase == PHASE_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

void if_a_creature_damaged_by_me_dies_do_something(int player, int card, event_t event, int mode){
	// modes
	// 1<<0 --> Controller of source gains life equal to attached creature toughness [Abattoir Ghoul]
	// 1<<1 --> Controller of source may draw a card [Rot Wolf]

	card_instance_t *instance = get_card_instance(player, card);

	if( ! in_play(player, card) || is_humiliated(player, card, AL_TRIG) ){
		return;
	}

	if( event == EVENT_DEAL_DAMAGE ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card != -1 && damage->damage_source_player == player &&
				damage->damage_source_card == card && damage->info_slot > 0
			  ){
				if( instance->info_slot < 0 ){
					instance->info_slot = 0;
				}
				if( instance->info_slot < 10 ){
					instance->targets[instance->info_slot].player = damage->damage_target_player;
					instance->targets[instance->info_slot].card = damage->damage_target_card;
					instance->info_slot++;
				}
			}
		}
	}

	if( instance->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player ){
		int i;
		for(i=0; i<instance->info_slot; i++){
			int legacy = create_targetted_legacy_effect(player, card, &if_attached_creature_dies_do_something,
														instance->targets[i].player, instance->targets[i].card);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[0].player = player;
			leg->targets[0].card = card;
			leg->number_of_targets = 1;
			leg->targets[1].card = mode;
		}
		instance->info_slot = 0;
	}
}

