#include "manalink.h"

typedef int (*test_selection_predicate_t)(int, intptr_t, int, int);

/* Canonical recovered helper declarations. */
int new_get_test_score(test_definition_t *this_test);
void new_default_test_definition(test_definition_t *this_test, int type, const char *msg);
void default_test_definition(test_definition_t *this_test, int type);
int new_make_test_in_play(int player, int to_examine, int test_type,
                               test_definition_t *this_test);
int new_make_test(int player, int to_examine, int test_type, test_definition_t *this_test);
int make_test_in_play(int player, int to_examine, int test_type, int type, int flag1,
                           int subtype, int flag2, int color, int flag3, int id, int flag4, int cc,
                           int flag5);
int make_test(int player, int to_examine, int test_type, int type, int flag1, int subtype,
                   int flag2, int color, int flag3, int id, int flag4, int cc, int flag5);
int check_battlefield_for_special_card(int player, int card, int t_player, int mode,
                                            test_definition_t *this_test);
int get_test_score(int type, int flag1, int subtype, int flag2, int color, int flag3, int id,
                        int flag4, int cc, int flag5);

int get_test_score(int type, int flag1, int subtype, int flag2, int color, int flag3, int id,
                   int flag4, int cc, int flag5)
{
  unsigned int score = 0;

  if (type != 0)
  {
    switch (flag1)
    {
    case MATCH: score |= 0x1u; break;
    case DOESNT_MATCH: score |= 0x20u; break;
    case F1_NO_CREATURE: score |= 0x1000u; break;
    case F1_NO_TOKEN: score |= 0x2000u; break;
    case F1_NO_PWALKER: score |= 0x10000u; break;
    case 5: score |= 0x40000u; break;
    case F1_IS_TOKEN: score |= 0x80000u; break;
    }
  }
  if (subtype != 0)
  {
    if (flag2 == MATCH)
      score |= 0x2u;
    else if (flag2 > 0)
      score |= 0x40u;
  }
  if (color != 0)
  {
    if (flag3 == MATCH)
      score |= 0x4u;
    else if (flag3 == DOESNT_MATCH)
      score |= 0x80u;
  }
  else if (flag3 == F3_MULTICOLORED)
    score |= 0x20000u;

  if (id != 0)
  {
    switch (flag4)
    {
    case MATCH: score |= 0x8u; break;
    case DOESNT_MATCH: score |= 0x100u; break;
    case 2: score |= 0x4000u; break;
    case F4_TOKEN_W_SPECIAL_INFOS: score |= 0x8000u; break;
    }
  }
  if (cc >= 0)
  {
    switch (flag5)
    {
    case MATCH: score |= 0x10u; break;
    case DOESNT_MATCH: score |= 0x200u; break;
    case F5_CMC_GREATER_THAN_VALUE: score |= 0x400u; break;
    case F5_CMC_LESSER_THAN_VALUE: score |= 0x800u; break;
    }
  }
  return score;
}

int new_get_test_score(test_definition_t *this_test)
{
  unsigned int score = 0;

  if (this_test->type != 0)
  {
    switch (this_test->type_flag)
    {
    case MATCH: score |= 0x1u; break;
    case DOESNT_MATCH: score |= 0x20u; break;
    case F1_NO_CREATURE: score |= 0x1000u; break;
    case F1_NO_TOKEN: score |= 0x2000u; break;
    case F1_NO_PWALKER: score |= 0x10000u; break;
    case F1_IS_TOKEN: score |= 0x80000u; break;
    }
  }
  if (this_test->subtype != 0)
  {
    if (this_test->subtype_flag == MATCH)
      score |= 0x2u;
    else if (this_test->subtype_flag == DOESNT_MATCH)
      score |= 0x40u;
  }
  if (this_test->color != 0)
  {
    if (this_test->color_flag == MATCH)
      score |= 0x4u;
    else if (this_test->color_flag == DOESNT_MATCH)
      score |= 0x80u;
  }
  else if (this_test->color_flag == F3_MULTICOLORED)
    score |= 0x20000u;
  else if (this_test->color_flag == F3_MONOCOLORED)
    score |= 0x40000u;

  if (this_test->id > 0)
  {
    switch (this_test->id_flag)
    {
    case MATCH: score |= 0x8u; break;
    case DOESNT_MATCH: score |= 0x100u; break;
    case 2: score |= 0x4000u; break;
    case F4_TOKEN_W_SPECIAL_INFOS: score |= 0x8000u; break;
    }
  }
  if (this_test->cmc >= 0)
  {
    switch (this_test->cmc_flag)
    {
    case MATCH: score |= 0x10u; break;
    case DOESNT_MATCH: score |= 0x200u; break;
    case F5_CMC_GREATER_THAN_VALUE: score |= 0x400u; break;
    case F5_CMC_LESSER_THAN_VALUE: score |= 0x800u; break;
    }
  }
  if (this_test->keyword != 0 || this_test->keyword_flag != 0)
    score |= 0x100000u;

  if (this_test->power >= 0)
  {
    switch (this_test->power_flag)
    {
    case MATCH: score |= 0x400000u; break;
    case DOESNT_MATCH: score |= 0x800000u; break;
    case F5_POWER_GREATER_THAN_VALUE: score |= 0x10000000u; break;
    case F5_POWER_LESSER_THAN_VALUE: score |= 0x20000000u; break;
    }
  }
  if (this_test->toughness >= 0)
  {
    switch (this_test->toughness_flag)
    {
    case MATCH: score |= 0x1000000u; break;
    case DOESNT_MATCH: score |= 0x2000000u; break;
    case F5_TOUGHNESS_GREATER_THAN_VALUE: score |= 0x40000000u; break;
    case F5_TOUGHNESS_LESSER_THAN_VALUE: score |= 0x80000000u; break;
    }
  }
  if (this_test->state != 0)
  {
    if (this_test->state_flag == MATCH)
      score |= 0x4000000u;
    else if (this_test->state_flag == DOESNT_MATCH)
      score |= 0x8000000u;
  }
  // Native ownership filtering shares the high bit with the upper toughness mode.
  if (this_test->owner != ANYBODY)
    score |= 0x80000000u;
  return score;
}

static int check_multisubtype(int player, int card, int sub1, int sub2, int sub3, int sub4, int sub5){
	if( sub1 > 0 && has_subtype(player, card, sub1) ){
		return 1;
	}
	if( sub2 > 0 && has_subtype(player, card, sub2) ){
		return 1;
	}
	if( sub3 > 0 && has_subtype(player, card, sub3) ){
		return 1;
	}
	if( sub4 > 0 && has_subtype(player, card, sub4) ){
		return 1;
	}
	if( sub5 > 0 && has_subtype(player, card, sub5) ){
		return 1;
	}
	return 0;
}

static int check_multisubtype_all(int player, int card, int sub1, int sub2, int sub3, int sub4, int sub5){
	if( sub1 > 0 && !has_subtype(player, card, sub1) ){
		return 0;
	}
	if( sub2 > 0 && !has_subtype(player, card, sub2) ){
		return 0;
	}
	if( sub3 > 0 && !has_subtype(player, card, sub3) ){
		return 0;
	}
	if( sub4 > 0 && !has_subtype(player, card, sub4) ){
		return 0;
	}
	if( sub5 > 0 && !has_subtype(player, card, sub5) ){
		return 0;
	}
	return 1;
}

void new_default_test_definition(test_definition_t *this_test, int type, const char *msg)
{
  if (ai_is_speculating == 1)
  {
    this_test->message[0] = '\0';
  }
  else
  {
    strcpy(this_test->message, msg);
  }
  this_test->subtype = 0;
  this_test->type = type;
  this_test->type_flag = 0;
  this_test->subtype_flag = 0;
  this_test->color = 0;
  this_test->id = 0;
  this_test->id2 = 0;
  this_test->color_flag = 0;
  this_test->id_flag = 0;
  this_test->cmc = -1;
  this_test->keyword = 0;
  this_test->cmc_flag = 0;
  this_test->keyword_flag = 0;
  this_test->sp_keyword = 0;
  this_test->sp_keyword_flag = 0;
  this_test->power = -1;
  this_test->toughness = -1;
  this_test->power_flag = 0;
  this_test->toughness_flag = 0;
  this_test->state = 0;
  this_test->zone = 0x200;
  this_test->state_flag = 0;
  this_test->not_me = 0;
  this_test->ai_selection_mode = 1;
  this_test->qty = 1;
  this_test->sub2 = 0;
  this_test->sub3 = 0;
  this_test->sub4 = 0;
  this_test->sub5 = 0;
  this_test->no_shuffle = 0;
  this_test->create_minideck = 0;
  this_test->special_selection_function = (test_selection_predicate_t)0x0;
  this_test->value_for_special_selection_function = -1;
  this_test->painters_servant_hack[0] = -1;
  this_test->painters_servant_hack[1] = -1;
  this_test->has_mana_to_pay_cmc = '\0';
  this_test->can_legally_play = '\0';
  this_test->owner = '\x02';
  this_test->storage = -1;
  return;
}

void default_test_definition(test_definition_t *this_test, int type)
{
  new_default_test_definition(this_test, type, "Select a card.");
  return;
}

int new_make_test_in_play(int player, int to_examine, int test_type, test_definition_t *this_test)
{
  int cmc;
  uint16_t initial_type_mode;
  uint16_t initial_color_mode;
  short state_match_mode;
  uint16_t color_mode_after_query;
  uint16_t cmc_match_mode;
  uint16_t type_mode_after_match;
  uint16_t type_mode_after_exclusion;
  uint16_t type_mode_after_noncreature;
  uint16_t power_match_mode;
  uint16_t type_mode_after_nontoken;
  uint16_t type_mode_after_nonplaneswalker;
  uint16_t type_mode_after_token;
  short type_or_special_keyword_mode;
  uint16_t state_mode_after_query;
  short color_match_mode;
  subtype_t first_or_fifth_subtype;
  test_selection_predicate_t special_selection;
  subtype_t subtype;
  subtype_t fifth_or_third_subtype;
  subtype_t second_or_fourth_subtype;
  subtype_t fourth_or_second_subtype;
  int id_stat_or_predicate_result;
  card_instance_t *instance;
  unsigned int requested_colors_or_owner;
  unsigned int card_colors;
  type_t card_types;
  int probe_id_or_result;
  unsigned short subtype_id_keyword_or_toughness_mode;

  id_stat_or_predicate_result = get_id(player, to_examine);
  if (this_test->can_legally_play != '\0')
  {
    instance = get_card_instance(player, to_examine);
    probe_id_or_result = can_legally_play_iid(player, instance->internal_card_id);
    if (probe_id_or_result == 0)
    {
      return 0;
    }
  }
  if (this_test->type != 0)
  {
    initial_type_mode = this_test->type_flag;
    type_or_special_keyword_mode = initial_type_mode;
    if (initial_type_mode == 0)
    {
      probe_id_or_result = is_what(player, to_examine, this_test->type);
      if (probe_id_or_result == 0)
      {
        return 0;
      }
      type_mode_after_match = this_test->type_flag;
      type_or_special_keyword_mode = type_mode_after_match;
    }
    if (type_or_special_keyword_mode == 1)
    {
      probe_id_or_result = is_what(player, to_examine, this_test->type);
      if (probe_id_or_result != 0)
      {
        return 0;
      }
      type_mode_after_exclusion = this_test->type_flag;
      type_or_special_keyword_mode = type_mode_after_exclusion;
    }
    if (type_or_special_keyword_mode == 2)
    {
      if (is_what(player, to_examine, this_test->type) == 0)
      {
        return 0;
      }
      probe_id_or_result = is_what(player, to_examine, 2);
      if (probe_id_or_result != 0)
      {
        return 0;
      }
      type_mode_after_noncreature = this_test->type_flag;
      type_or_special_keyword_mode = type_mode_after_noncreature;
    }
    if (type_or_special_keyword_mode == 3)
    {
      if (is_what(player, to_examine, this_test->type) == 0)
      {
        return 0;
      }
      probe_id_or_result = is_token(player, to_examine);
      if (probe_id_or_result != 0)
      {
        return 0;
      }
      type_mode_after_nontoken = this_test->type_flag;
      type_or_special_keyword_mode = type_mode_after_nontoken;
    }
    if (type_or_special_keyword_mode == 4)
    {
      if (is_what(player, to_examine, this_test->type) == 0)
      {
        return 0;
      }
      probe_id_or_result = is_planeswalker(player, to_examine);
      if (probe_id_or_result != 0)
      {
        return 0;
      }
      type_mode_after_nonplaneswalker = this_test->type_flag;
      type_or_special_keyword_mode = type_mode_after_nonplaneswalker;
    }
    if (type_or_special_keyword_mode == 6)
    {
      if (is_what(player, to_examine, this_test->type) == 0)
      {
        return 0;
      }
      probe_id_or_result = is_token(player, to_examine);
      if (probe_id_or_result == 0)
      {
        return 0;
      }
      type_mode_after_token = this_test->type_flag;
      type_or_special_keyword_mode = type_mode_after_token;
    }
    if (type_or_special_keyword_mode == 9)
    {
      if (is_what(player, to_examine, 2) == 0)
      {
        return 0;
      }
      probe_id_or_result = is_what(player, to_examine, 0x40);
      if (probe_id_or_result != 0)
      {
        return 0;
      }
      type_or_special_keyword_mode = this_test->type_flag;
    }
    if ((type_or_special_keyword_mode == 0xb) &&
        (card_types = get_type(player, to_examine),
         this_test->type != (card_types & this_test->type)))
    {
      return 0;
    }
  }
  first_or_fifth_subtype = this_test->subtype;
  if (first_or_fifth_subtype != SUBTYPE_INVALID)
  {
    subtype_id_keyword_or_toughness_mode = this_test->subtype_flag;
    if (subtype_id_keyword_or_toughness_mode == 3)
    {
      fifth_or_third_subtype = this_test->sub5;
      second_or_fourth_subtype = this_test->sub2;
      fourth_or_second_subtype = this_test->sub4;
      subtype = this_test->sub3;
      if ((((((int)first_or_fifth_subtype < 1) ||
             (probe_id_or_result = has_subtype(player, to_examine, first_or_fifth_subtype),
              probe_id_or_result == 0)) &&
            (((int)second_or_fourth_subtype < 1 ||
              (probe_id_or_result = has_subtype(player, to_examine, second_or_fourth_subtype),
               probe_id_or_result == 0)))) &&
           (((int)subtype < 1 || (probe_id_or_result = has_subtype(player, to_examine, subtype),
                                  probe_id_or_result == 0)))) &&
          (((int)fourth_or_second_subtype < 1 ||
            (probe_id_or_result = has_subtype(player, to_examine, fourth_or_second_subtype),
             probe_id_or_result == 0))))
      {
        if ((int)fifth_or_third_subtype < 1)
        {
          return 0;
        }
        probe_id_or_result = has_subtype(player, to_examine, fifth_or_third_subtype);
        if (probe_id_or_result == 0)
        {
          return 0;
        }
      }
      subtype_id_keyword_or_toughness_mode = this_test->subtype_flag;
    }
    if (subtype_id_keyword_or_toughness_mode == 4)
    {
      first_or_fifth_subtype = this_test->sub5;
      fifth_or_third_subtype = this_test->sub3;
      second_or_fourth_subtype = this_test->sub4;
      fourth_or_second_subtype = this_test->sub2;
      if ((0 < (int)this_test->subtype) &&
          (probe_id_or_result = has_subtype(player, to_examine, this_test->subtype),
           probe_id_or_result == 0))
      {
        return 0;
      }
      if ((0 < (int)fourth_or_second_subtype) &&
          (probe_id_or_result = has_subtype(player, to_examine, fourth_or_second_subtype),
           probe_id_or_result == 0))
      {
        return 0;
      }
      if ((0 < (int)fifth_or_third_subtype) &&
          (probe_id_or_result = has_subtype(player, to_examine, fifth_or_third_subtype),
           probe_id_or_result == 0))
      {
        return 0;
      }
      if ((0 < (int)second_or_fourth_subtype) &&
          (probe_id_or_result = has_subtype(player, to_examine, second_or_fourth_subtype),
           probe_id_or_result == 0))
      {
        return 0;
      }
      if ((0 < (int)first_or_fifth_subtype) &&
          (probe_id_or_result = has_subtype(player, to_examine, first_or_fifth_subtype),
           probe_id_or_result == 0))
      {
        return 0;
      }
      subtype_id_keyword_or_toughness_mode = this_test->subtype_flag;
    }
    if (subtype_id_keyword_or_toughness_mode == 0)
    {
      probe_id_or_result = has_subtype(player, to_examine, this_test->subtype);
      if (probe_id_or_result == 0)
      {
        return 0;
      }
      subtype_id_keyword_or_toughness_mode = this_test->subtype_flag;
    }
    if ((subtype_id_keyword_or_toughness_mode == 1) &&
        (probe_id_or_result = has_subtype(player, to_examine, this_test->subtype),
         probe_id_or_result != 0))
    {
      return 0;
    }
  }
  if (this_test->color == 0)
  {
    initial_color_mode = this_test->color_flag;
    color_match_mode = initial_color_mode;
    if (color_match_mode == 2)
    {
      probe_id_or_result = count_colors(player, to_examine);
      if (probe_id_or_result < 2)
      {
        return 0;
      }
      color_match_mode = this_test->color_flag;
    }
    if ((color_match_mode == 3) &&
        (probe_id_or_result = count_colors(player, to_examine), probe_id_or_result != 1))
    {
      return 0;
    }
  }
  else
  {
    card_colors = get_color(player, to_examine);
    requested_colors_or_owner = this_test->color;
    color_mode_after_query = this_test->color_flag;
    if (requested_colors_or_owner == 1)
    {
      if ((color_mode_after_query == 0 && card_colors != 0) ||
          (color_mode_after_query == 1 && card_colors == 0))
        return 0;
    }
    else if (color_mode_after_query == 0)
    {
      if ((card_colors & requested_colors_or_owner) == 0)
      {
        return 0;
      }
    }
    else if (color_mode_after_query == 1)
    {
      if ((card_colors & requested_colors_or_owner) != 0)
      {
        return 0;
      }
    }
    else
    {
      color_match_mode = color_mode_after_query;
      if (requested_colors_or_owner == 0)
      {
        if (color_match_mode == 2)
        {
          probe_id_or_result = count_colors(player, to_examine);
          if (probe_id_or_result < 2)
          {
            return 0;
          }
          color_match_mode = this_test->color_flag;
        }
        if ((color_match_mode == 3) &&
            (probe_id_or_result = count_colors(player, to_examine), probe_id_or_result != 1))
        {
          return 0;
        }
      }
    }
  }
  if (this_test->id == -1)
  {
    return 0;
  }
  if (this_test->id != 0)
  {
    subtype_id_keyword_or_toughness_mode = this_test->id_flag;
    if (subtype_id_keyword_or_toughness_mode == 3)
    {
      if (is_token(player, to_examine) == 0)
      {
        return 0;
      }
      probe_id_or_result = get_special_infos(player, to_examine);
      if (probe_id_or_result != this_test->id)
      {
        return 0;
      }
      subtype_id_keyword_or_toughness_mode = this_test->id_flag;
    }
    if (subtype_id_keyword_or_toughness_mode == 4)
    {
      if (this_test->id != id_stat_or_predicate_result)
      {
        probe_id_or_result = this_test->id2;
        if (probe_id_or_result != id_stat_or_predicate_result)
        {
          return 0;
        }
      }
    }
    else
    {
      if (subtype_id_keyword_or_toughness_mode == 0)
      {
        probe_id_or_result = this_test->id;
        if (probe_id_or_result != id_stat_or_predicate_result)
          return 0;
      }
      if ((subtype_id_keyword_or_toughness_mode == 1) &&
          (this_test->id == id_stat_or_predicate_result))
      {
        return 0;
      }
    }
  }
  if (-1 < this_test->cmc)
  {
    id_stat_or_predicate_result = get_cmc(player, to_examine);
    cmc_match_mode = this_test->cmc_flag;
    if (cmc_match_mode == 0)
    {
      if (this_test->cmc != id_stat_or_predicate_result)
      {
        return 0;
      }
    }
    else if (cmc_match_mode == 1)
    {
      if (this_test->cmc == id_stat_or_predicate_result)
      {
        return 0;
      }
    }
    else if (cmc_match_mode == 2)
    {
      if (id_stat_or_predicate_result <= this_test->cmc)
      {
        return 0;
      }
    }
    else if ((cmc_match_mode == 3) && (this_test->cmc <= id_stat_or_predicate_result))
    {
      return 0;
    }
  }
  if (this_test->keyword != 0)
  {
    subtype_id_keyword_or_toughness_mode = this_test->keyword_flag;
    if (subtype_id_keyword_or_toughness_mode == 0)
    {
      id_stat_or_predicate_result = check_for_ability(player, to_examine, this_test->keyword);
      if (id_stat_or_predicate_result == 0)
      {
        return 0;
      }
      subtype_id_keyword_or_toughness_mode = this_test->keyword_flag;
    }
    if (subtype_id_keyword_or_toughness_mode == 1)
    {
      id_stat_or_predicate_result = check_for_ability(player, to_examine, this_test->keyword);
      if (id_stat_or_predicate_result != 0)
      {
        return 0;
      }
      subtype_id_keyword_or_toughness_mode = this_test->keyword_flag;
    }
    if (subtype_id_keyword_or_toughness_mode == 2)
    {
      id_stat_or_predicate_result =
          check_for_special_ability(player, to_examine, this_test->keyword);
      if (id_stat_or_predicate_result == 0)
      {
        return 0;
      }
      subtype_id_keyword_or_toughness_mode = this_test->keyword_flag;
    }
    if ((subtype_id_keyword_or_toughness_mode == 3) &&
        (id_stat_or_predicate_result =
             check_for_special_ability(player, to_examine, this_test->keyword),
         id_stat_or_predicate_result != 0))
    {
      return 0;
    }
  }
  if (this_test->sp_keyword != 0)
  {
    type_or_special_keyword_mode = (int16_t)this_test->sp_keyword_flag;
    if (type_or_special_keyword_mode == 0)
    {
      id_stat_or_predicate_result =
          check_for_special_ability(player, to_examine, this_test->sp_keyword);
      if (id_stat_or_predicate_result == 0)
      {
        return 0;
      }
      type_or_special_keyword_mode = (int16_t)this_test->sp_keyword_flag;
    }
    if ((type_or_special_keyword_mode == 1) &&
        (id_stat_or_predicate_result =
             check_for_special_ability(player, to_examine, this_test->sp_keyword),
         id_stat_or_predicate_result != 0))
    {
      return 0;
    }
  }
  if (-1 < this_test->power)
  {
    id_stat_or_predicate_result = get_power(player, to_examine);
    power_match_mode = this_test->power_flag;
    if (power_match_mode == 0)
    {
      if (this_test->power != id_stat_or_predicate_result)
      {
        return 0;
      }
    }
    else if (power_match_mode == 1)
    {
      if (this_test->power == id_stat_or_predicate_result)
      {
        return 0;
      }
    }
    else if (power_match_mode == 3)
    {
      if (this_test->power <= id_stat_or_predicate_result)
        return 0;
    }
    else if (power_match_mode == 2 || power_match_mode == 4)
    {
      if (power_match_mode == 4)
        id_stat_or_predicate_result = count_counters(player, to_examine, COUNTER_P1_P1);
      if (id_stat_or_predicate_result <= this_test->power)
        return 0;
    }
  }
  if (-1 < this_test->toughness)
  {
    id_stat_or_predicate_result = get_toughness(player, to_examine);
    subtype_id_keyword_or_toughness_mode = this_test->toughness_flag;
    if (subtype_id_keyword_or_toughness_mode == 0)
    {
      if (this_test->toughness != id_stat_or_predicate_result)
      {
        return 0;
      }
    }
    else if (subtype_id_keyword_or_toughness_mode == 1)
    {
      if (this_test->toughness == id_stat_or_predicate_result)
      {
        return 0;
      }
    }
    else if (subtype_id_keyword_or_toughness_mode == 2)
    {
      if (id_stat_or_predicate_result <= this_test->toughness)
      {
        return 0;
      }
    }
    else if ((subtype_id_keyword_or_toughness_mode == 3) &&
             (this_test->toughness <= id_stat_or_predicate_result))
    {
      return 0;
    }
  }
  if (this_test->state != 0)
  {
    state_match_mode = this_test->state_flag;
    if (state_match_mode == 0)
    {
      id_stat_or_predicate_result = check_state(player, to_examine, this_test->state);
      if (id_stat_or_predicate_result == 0)
      {
        return 0;
      }
      state_mode_after_query = this_test->state_flag;
      state_match_mode = state_mode_after_query;
    }
    if ((state_match_mode == 1) &&
        (id_stat_or_predicate_result = check_state(player, to_examine, this_test->state),
         id_stat_or_predicate_result != 0))
    {
      return 0;
    }
  }
  special_selection = this_test->special_selection_function;
  if (special_selection != (test_selection_predicate_t)0x0)
  {
    instance = get_card_instance(player, to_examine);
    id_stat_or_predicate_result = (*special_selection)(
        instance->internal_card_id, this_test->value_for_special_selection_function, player, to_examine);
    if (id_stat_or_predicate_result == 0)
    {
      return 0;
    }
  }
  if (this_test->has_mana_to_pay_cmc == '\x01')
  {
    cmc = get_cmc(player, to_examine);
    if (has_mana(player, 0, cmc) == 0)
    {
      return 0;
    }
  }
  if (this_test->owner == '\x02')
  {
    return 1;
  }
  requested_colors_or_owner = get_owner(player, to_examine);
  return (unsigned int)(this_test->owner == requested_colors_or_owner);
}

int make_test_in_play(int player, int to_examine, int test_type, int type, int flag1,
                           int subtype, int flag2, int color, int flag3, int id, int flag4, int cc,
                           int flag5)
{
  int result;
  test_definition_t this_test;
  new_default_test_definition(&this_test, type, "Select a card.");
  this_test.type_flag = (unsigned short)flag1;
  this_test.subtype = subtype;
  this_test.subtype_flag = (unsigned short)flag2;
  this_test.color = color;
  this_test.color_flag = (unsigned short)flag3;
  this_test.id = id;
  this_test.id_flag = (unsigned short)flag4;
  this_test.cmc = cc;
  this_test.cmc_flag = (unsigned short)flag5;
  this_test.type = type;
  result = new_make_test_in_play(player, to_examine, test_type, &this_test);
  return result;
}

int new_make_test(int player, int to_examine, int test_type, test_definition_t *this_test)
{
  uint16_t initial_type_mode;
  uint16_t initial_color_mode;
  uint16_t color_count_mode;
  uint16_t color_mode_after_query;
  uint16_t cmc_match_mode;
  uint16_t type_mode_after_match;
  uint16_t type_mode_after_exclusion;
  uint16_t type_mode_after_noncreature;
  uint16_t power_match_mode;
  uint16_t type_mode_after_nonplaneswalker;
  short type_or_special_keyword_mode;
  short color_match_mode;
  subtype_t first_or_fifth_subtype;
  unsigned int requested_colors_or_id;
  subtype_t subtype;
  subtype_t fifth_or_third_subtype;
  subtype_t third_or_fourth_subtype;
  subtype_t fourth_or_second_subtype;
  unsigned int csvid_or_keyword_result;
  unsigned int clr;
  int result;
  type_t card_types;
  unsigned short subtype_id_keyword_or_toughness_mode;

  csvid_or_keyword_result =
      (uint32_t)cards_data[to_examine].id | 0;
  if ((this_test->can_legally_play != '\0') &&
      (result = can_legally_play_iid(player, to_examine), result == 0))
  {
    return 0;
  }
  if (this_test->type != 0)
  {
    initial_type_mode = this_test->type_flag;
    type_or_special_keyword_mode = initial_type_mode;
    if (initial_type_mode == 0)
    {
      result = is_what(-1, to_examine, this_test->type);
      if (result == 0)
      {
        return 0;
      }
      type_mode_after_match = this_test->type_flag;
      type_or_special_keyword_mode = type_mode_after_match;
    }
    if (type_or_special_keyword_mode == 1)
    {
      result = is_what(-1, to_examine, this_test->type);
      if (result != 0)
      {
        return 0;
      }
      type_mode_after_exclusion = this_test->type_flag;
      type_or_special_keyword_mode = type_mode_after_exclusion;
    }
    if (type_or_special_keyword_mode == 2)
    {
      result = is_what(-1, to_examine, this_test->type);
      if (result == 0)
      {
        return 0;
      }
      result = is_what(-1, to_examine, 2);
      if (result != 0)
      {
        return 0;
      }
      type_mode_after_noncreature = this_test->type_flag;
      type_or_special_keyword_mode = type_mode_after_noncreature;
    }
    if (type_or_special_keyword_mode == 4)
    {
      result = is_what(-1, to_examine, this_test->type);
      if (result == 0)
      {
        return 0;
      }
      result = is_planeswalker(-1, to_examine);
      if (result != 0)
      {
        return 0;
      }
      type_mode_after_nonplaneswalker = this_test->type_flag;
      type_or_special_keyword_mode = type_mode_after_nonplaneswalker;
    }
    if (type_or_special_keyword_mode == 9)
    {
      result = is_what(-1, to_examine, 2);
      if (result == 0)
      {
        return 0;
      }
      result = is_what(-1, to_examine, 0x40);
      if (result != 0)
      {
        return 0;
      }
      type_or_special_keyword_mode = this_test->type_flag;
    }
    if ((type_or_special_keyword_mode == 0xb) &&
        (card_types = get_type(-1, to_examine), this_test->type != (card_types & this_test->type)))
    {
      return 0;
    }
  }
  csvid_or_keyword_result = csvid_or_keyword_result & 0xffff;
  first_or_fifth_subtype = this_test->subtype;
  if (first_or_fifth_subtype != SUBTYPE_INVALID)
  {
    subtype_id_keyword_or_toughness_mode = this_test->subtype_flag;
    if (subtype_id_keyword_or_toughness_mode == 3)
    {
      fifth_or_third_subtype = this_test->sub5;
      third_or_fourth_subtype = this_test->sub3;
      fourth_or_second_subtype = this_test->sub4;
      subtype = this_test->sub2;
      if ((((((int)first_or_fifth_subtype < 1) ||
             (result = has_subtype_by_id(-1, csvid_or_keyword_result, first_or_fifth_subtype),
              result == 0)) &&
            (((int)subtype < 1 ||
              (result = has_subtype_by_id(-1, csvid_or_keyword_result, subtype), result == 0)))) &&
           (((int)third_or_fourth_subtype < 1 ||
             (result = has_subtype_by_id(-1, csvid_or_keyword_result, third_or_fourth_subtype),
              result == 0)))) &&
          (((int)fourth_or_second_subtype < 1 ||
            (result = has_subtype_by_id(-1, csvid_or_keyword_result, fourth_or_second_subtype),
             result == 0))))
      {
        if ((int)fifth_or_third_subtype < 1)
        {
          return 0;
        }
        result = has_subtype_by_id(-1, csvid_or_keyword_result, fifth_or_third_subtype);
        if (result == 0)
        {
          return 0;
        }
      }
      subtype_id_keyword_or_toughness_mode = this_test->subtype_flag;
    }
    if (subtype_id_keyword_or_toughness_mode == 4)
    {
      first_or_fifth_subtype = this_test->sub5;
      fifth_or_third_subtype = this_test->sub3;
      third_or_fourth_subtype = this_test->sub4;
      fourth_or_second_subtype = this_test->sub2;
      if ((0 < (int)this_test->subtype) &&
          (result = has_subtype_by_id(-1, csvid_or_keyword_result, this_test->subtype),
           result == 0))
      {
        return 0;
      }
      if ((0 < (int)fourth_or_second_subtype) &&
          (result = has_subtype_by_id(-1, csvid_or_keyword_result, fourth_or_second_subtype),
           result == 0))
      {
        return 0;
      }
      if ((0 < (int)fifth_or_third_subtype) &&
          (result = has_subtype_by_id(-1, csvid_or_keyword_result, fifth_or_third_subtype),
           result == 0))
      {
        return 0;
      }
      if ((0 < (int)third_or_fourth_subtype) &&
          (result = has_subtype_by_id(-1, csvid_or_keyword_result, third_or_fourth_subtype),
           result == 0))
      {
        return 0;
      }
      if ((0 < (int)first_or_fifth_subtype) &&
          (result = has_subtype_by_id(-1, csvid_or_keyword_result, first_or_fifth_subtype),
           result == 0))
      {
        return 0;
      }
      subtype_id_keyword_or_toughness_mode = this_test->subtype_flag;
    }
    if (subtype_id_keyword_or_toughness_mode == 0)
    {
      result = has_subtype_by_id(player, csvid_or_keyword_result, this_test->subtype);
      if (result == 0)
      {
        return 0;
      }
      subtype_id_keyword_or_toughness_mode = this_test->subtype_flag;
    }
    if ((subtype_id_keyword_or_toughness_mode == 1) &&
        (result = has_subtype_by_id(player, csvid_or_keyword_result, this_test->subtype),
         result != 0))
    {
      return 0;
    }
  }
  if ((this_test->color == 0) &&
      (color_count_mode = this_test->color_flag, 1 < (unsigned short)(color_count_mode - 2)))
  {
    clr = 0;
    color_match_mode = color_count_mode;
    if (color_match_mode == 2)
    {
      result = real_count_colors(clr);
      if (result < 2)
      {
        return 0;
      }
      color_match_mode = this_test->color_flag;
    }
    if ((color_match_mode == 3) && (result = real_count_colors(clr), result != 1))
    {
      return 0;
    }
  }
  else
  {
    if (this_test->painters_servant_hack[player] == -1)
    {
      result = get_global_color_hack(player);
      this_test->painters_servant_hack[player] = (int16_t)result;
    }
    clr = get_color_by_internal_id(player, to_examine);
    requested_colors_or_id = this_test->color;
    if (requested_colors_or_id == 0)
    {
      initial_color_mode = this_test->color_flag;
      color_match_mode = initial_color_mode;
      if (color_match_mode == 2)
      {
        result = real_count_colors(clr);
        if (result < 2)
        {
          return 0;
        }
        color_match_mode = this_test->color_flag;
      }
      if ((color_match_mode == 3) && (result = real_count_colors(clr), result != 1))
      {
        return 0;
      }
    }
    else
    {
      color_mode_after_query = this_test->color_flag;
      if (color_mode_after_query == 0)
      {
        if ((clr & requested_colors_or_id) == 0)
        {
          return 0;
        }
      }
      else if ((color_mode_after_query == 1) && ((clr & requested_colors_or_id) != 0))
      {
        return 0;
      }
    }
  }
  requested_colors_or_id = this_test->id;
  if (requested_colors_or_id == 0xffffffff)
  {
    return 0;
  }
  if (requested_colors_or_id != 0)
  {
    subtype_id_keyword_or_toughness_mode = this_test->id_flag;
    if ((subtype_id_keyword_or_toughness_mode != 4) ||
        (requested_colors_or_id == csvid_or_keyword_result))
    {
      if ((subtype_id_keyword_or_toughness_mode == 0) &&
          (requested_colors_or_id != csvid_or_keyword_result))
      {
        return 0;
      }
      if ((requested_colors_or_id == csvid_or_keyword_result) &&
          (subtype_id_keyword_or_toughness_mode == 1))
      {
        return 0;
      }
    }
    else if (this_test->id2 != csvid_or_keyword_result)
    {
      return 0;
    }
  }
  if (-1 < this_test->cmc)
  {
    result = get_cmc_by_id((unsigned int)cards_data[to_examine].id);
    cmc_match_mode = this_test->cmc_flag;
    if (cmc_match_mode == 0)
    {
      if (this_test->cmc != result)
      {
        return 0;
      }
    }
    else if (cmc_match_mode == 1)
    {
      if (this_test->cmc == result)
      {
        return 0;
      }
    }
    else if (cmc_match_mode == 2)
    {
      if (result <= this_test->cmc)
      {
        return 0;
      }
    }
    else if ((cmc_match_mode == 3) && (this_test->cmc <= result))
    {
      return 0;
    }
  }
  csvid_or_keyword_result = this_test->keyword;
  if (csvid_or_keyword_result != 0)
  {
    subtype_id_keyword_or_toughness_mode = this_test->keyword_flag;
    if (subtype_id_keyword_or_toughness_mode == 0)
    {
      if ((cards_data[to_examine].static_ability & csvid_or_keyword_result) == 0)
        return 0;
    }
    else if (subtype_id_keyword_or_toughness_mode == 1)
    {
      csvid_or_keyword_result = cards_data[to_examine].static_ability & csvid_or_keyword_result;
      if (csvid_or_keyword_result != 0)
        return 0;
    }
    else
    {
      if (subtype_id_keyword_or_toughness_mode == 2)
      {
        result = check_for_special_ability(player, to_examine, csvid_or_keyword_result);
        if (result == 0)
          return 0;
        subtype_id_keyword_or_toughness_mode = this_test->keyword_flag;
      }
      if (subtype_id_keyword_or_toughness_mode == 3)
      {
        csvid_or_keyword_result = check_for_special_ability(player, to_examine, this_test->keyword);
        if (csvid_or_keyword_result != 0)
          return 0;
      }
    }
  }
  if (this_test->sp_keyword != 0)
  {
    type_or_special_keyword_mode = (int16_t)this_test->sp_keyword_flag;
    if (type_or_special_keyword_mode == 0)
    {
      result = check_for_special_ability(-1, to_examine, this_test->sp_keyword);
      if (result == 0)
      {
        return 0;
      }
      type_or_special_keyword_mode = (int16_t)this_test->sp_keyword_flag;
    }
    if ((type_or_special_keyword_mode == 1) &&
        (result = check_for_special_ability(-1, to_examine, this_test->sp_keyword), result != 0))
    {
      return 0;
    }
  }
  if (-1 < this_test->power)
  {
    result = get_base_power_iid(player, to_examine);
    power_match_mode = this_test->power_flag;
    if (power_match_mode == 0)
    {
      if (this_test->power != result)
      {
        return 0;
      }
    }
    else if (power_match_mode == 1)
    {
      if (this_test->power == result)
      {
        return 0;
      }
    }
    else if (power_match_mode == 2)
    {
      if (result <= this_test->power)
      {
        return 0;
      }
    }
    else if (power_match_mode == 3)
    {
      if (this_test->power <= result)
      {
        return 0;
      }
    }
    else
    {
      if (power_match_mode == 4)
      {
        return 0;
      }
    }
  }
  if (-1 < this_test->toughness)
  {
    result = get_base_toughness_iid(player, to_examine);
    subtype_id_keyword_or_toughness_mode = this_test->toughness_flag;
    if (subtype_id_keyword_or_toughness_mode == 0)
    {
      if (this_test->toughness != result)
      {
        return 0;
      }
    }
    else if (subtype_id_keyword_or_toughness_mode == 1)
    {
      if (this_test->toughness == result)
      {
        return 0;
      }
    }
    else if (subtype_id_keyword_or_toughness_mode == 2)
    {
      if (result <= this_test->toughness)
      {
        return 0;
      }
    }
    else if ((subtype_id_keyword_or_toughness_mode == 3) && (this_test->toughness <= result))
    {
      return 0;
    }
  }
  if ((this_test->special_selection_function != (test_selection_predicate_t)0x0) &&
      (result = (*this_test->special_selection_function)(
           to_examine, this_test->value_for_special_selection_function, player, -1),
       result == 0))
  {
    return 0;
  }
  if (this_test->has_mana_to_pay_cmc == '\x01')
  {
    result = get_cmc_by_id((unsigned int)cards_data[to_examine].id);
    result = has_mana(player, 0, result);
    if (result == 0)
    {
      return 0;
    }
  }
  if (this_test->owner == 2)
  {
    return 1;
  }
  return (unsigned int)((unsigned int)this_test->owner == player);
}

int make_test(int player, int to_examine, int test_type, int type, int flag1, int subtype,
                   int flag2, int color, int flag3, int id, int flag4, int cc, int flag5)
{
  int result;
  test_definition_t this_test;
  new_default_test_definition(&this_test, type, "Select a card.");
  this_test.type_flag = (unsigned short)flag1;
  this_test.subtype = subtype;
  this_test.subtype_flag = (unsigned short)flag2;
  this_test.color = color;
  this_test.color_flag = (unsigned short)flag3;
  this_test.id = id;
  this_test.id_flag = (unsigned short)flag4;
  this_test.cmc = cc;
  this_test.cmc_flag = (unsigned short)flag5;
  result = new_make_test(player, to_examine, test_type, &this_test);
  return result;
}

int my_base_value_by_id( int csvid ){
	card_ptr_t* c = cards_ptr[ csvid ];
	int result = c->ai_base_value;

	if( result < 1 ){
		result = 0;
		int iid = get_internal_card_id_from_csv_id(csvid);
		if( is_what(-1, iid, TYPE_CREATURE) ){
			result += get_base_power_iid(-1, iid);
			result += get_base_toughness_iid(-1, iid);
			result -= get_cmc_by_id(csvid)*2;
			int i;
			for(i=0; i<32; i++){
				if( cards_data[iid].static_ability & (1<<i) ){
					result+=4;
				}
				if( cards_data[iid].extra_ability & (1<<i) ){
					result+=8;
				}
			}
			// Flash
			if(cards_data[iid].type & TYPE_INSTANT){
				result+=10;
			}
		}
		if( is_what(-1, iid, TYPE_ARTIFACT)){
			int i;
			for(i=0; i<32; i++){
				if( cards_data[iid].extra_ability & (1<<i) ){
					result+=4;
				}
			}
			if( cards_data[iid].extra_ability & EA_MANA_SOURCE ){
				result+=real_count_colors(c->mana_source_colors)*2;
			}
			result-=get_cmc_by_id(csvid);
		}
		if( is_what(-1, iid, TYPE_ENCHANTMENT) || is_planeswalker(-1, iid) ){
			int i;
			for(i=0; i<32; i++){
				if( cards_data[iid].extra_ability & (1<<i) ){
					result+=4;
				}
			}
			if( cards_data[iid].extra_ability & EA_MANA_SOURCE ){
				result+=real_count_colors(c->mana_source_colors)*2;
			}
			result-=get_cmc_by_id(csvid);
			if( is_planeswalker(-1, iid) ){
				result+=20;
			}
		}
		if( is_what(-1, iid, TYPE_LAND)){
			int i;
			for(i=0; i<32; i++){
				if( cards_data[iid].extra_ability & (1<<i) ){
					result+=2;
				}
			}
			if( cards_data[iid].extra_ability & EA_MANA_SOURCE ){
				result+=real_count_colors(c->mana_source_colors)*2;
				result+=10;
			}
		}
		if( is_what(-1, iid, TYPE_SPELL) && ! is_what(-1, iid, TYPE_CREATURE) ){
			int i;
			for(i=0; i<32; i++){
				if( cards_data[iid].extra_ability & (1<<i) ){
					result+=4;
				}
			}
			result-=get_cmc_by_id(csvid);
		}
		result*=5;
		if( result < 0 ){
			result = 0;
		}
	}
	return result;
}

int my_base_value(int player, int card ){
	int result = 0;
	if( player == -1 ){
		result = my_base_value_by_id(cards_data[card].id);
	}
	else{
		int cmc = in_play(player, card) ? -get_cmc(player, card) : get_cmc(player, card);

		card_instance_t *instance = get_card_instance( player, card );
		int iid = instance->internal_card_id;
		int csvid = cards_data[iid].id;
		card_ptr_t* cp = cards_ptr[csvid];

		result = 0;
		if( is_what(player, card, TYPE_CREATURE) ){
			result+=get_power(player, card);
			result+=get_toughness(player, card);
			result-=cmc*2;
			int i;
			for(i=0; i<32; i++){
				if( cards_data[iid].static_ability & (1U << i) ){
					result+=4;
				}
				if( cards_data[iid].extra_ability & (1U << i) ){
					result+=8;
				}
			}
			// Flash
			if( (get_card_data(player, card)->type & TYPE_INSTANT) && !in_play(player, card) ){
				result+=10;
			}
		}
		if( is_what(player, card, TYPE_ARTIFACT)){
			int i;
			for(i=0; i<32; i++){
				if( cards_data[iid].extra_ability & (1U << i) ){
					result+=4;
				}
			}
			if( cards_data[iid].extra_ability & EA_MANA_SOURCE ){
				result+=real_count_colors(cp->mana_source_colors)*2;
			}
			result-=cmc;
		}
		if( is_what(player, card, TYPE_ENCHANTMENT) || is_planeswalker(player, card) ){
			int i;
			for(i=0; i<32; i++){
				if( cards_data[iid].extra_ability & (1U << i) ){
					result+=4;
				}
			}
			if( cards_data[iid].extra_ability & EA_MANA_SOURCE ){
				result+=real_count_colors(cp->mana_source_colors)*2;
			}
			result-=cmc;
			if( is_planeswalker(player, card) ){
				result+=20;
			}
		}
		if( is_what(player, card, TYPE_LAND)){
			int i;
			for(i=0; i<32; i++){
				if( cards_data[iid].extra_ability & (1U << i) ){
					result+=2;
				}
			}
			if( cards_data[iid].extra_ability & EA_MANA_SOURCE ){
				result+=real_count_colors(cp->mana_source_colors)*2;
				result+=10;
			}
		}
		if( is_what(player, card, TYPE_SPELL) && ! is_what(player, card, TYPE_CREATURE) ){
			int i;
			for(i=0; i<32; i++){
				if( cards_data[iid].extra_ability & (1U << i) ){
					result+=4;
				}
			}
			result-=cmc;
		}
		result*=5;
		if( result < 0 ){
			result = 0;
		}
	}
	return result;
}

extern int hack_illusionary_mask_event;	// must set to event when setting test_definition_t::has_mana_to_pay_cmc to 2
int check_battlefield_for_special_card(int player, int card, int t_player, int mode,
                                       test_definition_t *this_test)
{
  card_instance_t *candidate;
  int result;
  unsigned char mana_requirement_mode;
  unsigned int card_00;
  int player_00;
  bool candidate_valid;
  card_instance_t *targeting_instance;
  unsigned int best_value;
  unsigned int count_value_or_card;
  target_definition_t td;

  targeting_instance = NULL;
  if ((mode & 8U) != 0)
  {
    default_target_definition(player, card, &td, this_test->type);
    td.allowed_controller = t_player;
    td.preferred_controller = t_player;
    if (this_test->type_flag == 1)
    {
      td.required_type = ((this_test->zone != 0x200) ? 127U : 71U);
      td.illegal_type = this_test->type | 0x80;
    }
    targeting_instance = get_card_instance(player, card);
  }
  best_value = mode & 0x402;
  if (best_value != 0)
  {
    best_value = 1000;
  }
  count_value_or_card = -(unsigned int)((mode & 3U) != 0);
  player_00 = 0;
  do
  {
    if (((t_player == player_00) || (t_player == 2)) &&
        (card_00 = active_cards_count[player_00] - 1, -1 < (int)card_00))
    {
      do
      {
        candidate = in_play(player_00, card_00);
        if ((((((candidate != NULL) && (this_test->zone == 0x200)) &&
               (result = check_state(player_00, card_00, 0x100000), result == 0)) ||
              ((candidate = in_hand(player_00, card_00),
                candidate != NULL && (this_test->zone == 0x100)))) &&
             (result = new_make_test_in_play(player_00, card_00, -1, this_test), result != 0)) &&
            ((this_test->not_me == 0 ||
              ((this_test->not_me == 1 && ((player != player_00 || (card != card_00))))))))
        {
          candidate_valid = true;
          if ((mode & 8U) == 0)
          {
            mana_requirement_mode = this_test->has_mana_to_pay_cmc;
          }
          else
          {
            targeting_instance->targets[0].player = player_00;
            targeting_instance->targets[0].card = card_00;
            targeting_instance->number_of_targets = '\x01';
            result = valid_target(&td);
            mana_requirement_mode = this_test->has_mana_to_pay_cmc;
            candidate_valid = result != 0;
          }
          if (mana_requirement_mode == '\x01')
          {
            result = get_cmc(player_00, card_00);
            result = has_mana(player, 0, result);
            mana_requirement_mode = this_test->has_mana_to_pay_cmc;
            if (result == 0)
            {
              candidate_valid = false;
            }
          }
          if (mana_requirement_mode == '\x02')
          {
            result = get_original_internal_card_id(player_00, card_00);
            result = has_mana_to_cast_iid(player, hack_illusionary_mask_event, result);
            if (result == 0)
              candidate_valid = false;
          }
          if (((player != player_00 || card != card_00) || ((mode & 0x20U) == 0)) &&
              (candidate_valid))
          {
            if (mode == 0)
            {
              return 1;
            }
            count_value_or_card = (count_value_or_card + 1) - (unsigned int)((mode & 4U) == 0);
            if (((mode & 0x10U) != 0) && (result = is_what(player_00, card_00, 2), result != 0))
            {
              result = get_toughness(player_00, card_00);
              count_value_or_card = count_value_or_card + result;
            }
            if (((mode & 0x40U) != 0) && (result = is_what(player_00, card_00, 2), result != 0))
            {
              result = get_power(player_00, card_00);
              count_value_or_card = count_value_or_card + result;
            }
            if ((mode & 0x800U) != 0)
            {
              result = get_cmc(player_00, card_00);
              count_value_or_card = count_value_or_card + result;
            }
            if ((((mode & 0x100U) != 0) &&
                 (result = is_what(player_00, card_00, 2), result != 0)) &&
                (result = get_toughness(player_00, card_00), (int)best_value < result))
            {
              best_value = get_toughness(player_00, card_00);
              count_value_or_card = best_value;
            }
            if ((((mode & 0x80U) != 0) && (result = is_what(player_00, card_00, 2), result != 0)) &&
                (result = get_power(player_00, card_00), (int)best_value < result))
            {
              best_value = get_power(player_00, card_00);
              count_value_or_card = best_value;
            }
            if (((mode & 0x200U) != 0) &&
                (result = get_cmc(player_00, card_00), (int)best_value < result))
            {
              best_value = get_cmc(player_00, card_00);
              count_value_or_card = best_value;
            }
            if (((mode & 0x400U) != 0) &&
                (result = get_cmc(player_00, card_00), result < (int)best_value))
            {
              best_value = get_cmc(player_00, card_00);
              count_value_or_card = best_value;
            }
            if ((mode & 1U) != 0)
            {
              result = is_token(player_00, card_00);
              if (result == 0)
              {
                result = my_base_value(player_00, card_00);
                if ((int)best_value < result)
                {
                  best_value = my_base_value(player_00, card_00);
                  count_value_or_card = card_00;
                }
              }
              else
              {
                result = get_power(player_00, card_00);
                if ((int)best_value < result)
                {
                  best_value = get_power(player_00, card_00);
                  count_value_or_card = card_00;
                }
              }
            }
            if ((mode & 2U) != 0)
            {
              result = is_token(player_00, card_00);
              if (result == 0)
              {
                result = my_base_value(player_00, card_00);
                if (result < (int)best_value)
                {
                  best_value = my_base_value(player_00, card_00);
                  count_value_or_card = card_00;
                }
              }
              else
              {
                result = get_power(player_00, card_00);
                if (result < (int)best_value)
                {
                  best_value = get_power(player_00, card_00);
                  count_value_or_card = card_00;
                }
              }
            }
          }
        }
        card_00 = card_00 - 1;
      } while (card_00 != 0xffffffff);
    }
    player_00 = player_00 + 1;
    if (player_00 == 2)
    {
      return count_value_or_card;
    }
  } while (true);
}

int select_card_to_play_from_zone(int player, int card, event_t event, const int *zone, test_definition_t *test, int mode){
	int ng[count_zone(player, zone)];
	int ngc = 0;
	int count = 0;
	int result = -1;
	while( zone[count] != -1 ){
			if( new_make_test(player, zone[count], -1, test)  ){
				int good = 0;
				if( ! mode && has_mana_to_cast_iid(player, event, zone[count]) ){
					good = 1;
				}
				if( mode == 1 ){
					int cless = get_updated_casting_cost(player, -1, event, zone[count], -1);
					if( has_mana(player, COLOR_COLORLESS, cless) ){
						good = 1;
					}
				}
				if( good ){
					ng[ngc] = zone[count];
					ngc++;
				}
			}
			count++;
	}
	int selected = select_card_from_zone(player, player, ng, ngc, 0, AI_MAX_CMC, -1, test);
	if( selected != -1 ){
		count = 0;
		while( zone[count] != -1 ){
				if( zone[count] == ng[selected] ){
					result = count;
					break;
				}
				count++;
		}
	}
	return result;
}

int check_battlefield_for_name(int player, int type, int name){
	APNAP(p,{ 
				if( player == ANYBODY || p == player ){
					int c;
					for(c=0; c<active_cards_count[p]; c++){
						if( in_play(p, c) && is_what(p, c, type) && get_card_name(p, c) == name ){
							return 1;
						}
					}
				}
			};
	);
	return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x26c1000; original functions/global_checking.c:1212. */
int test_not_in_array(int iid, intptr_t arrptr, int player, int card)
{
  int result;

  result = 0;
  do
  {
    if (*(int *)arrptr == iid)
    {
      return 0;
    }
    result = result + 1;
    arrptr = arrptr + 4;
  } while ((result < 4) && (result != -1));
  return 1;
}

#ifndef SHANDALAR

/* ManalinkEh 2022: 0x26be510; original functions/global_checking.c:3. */

/* ManalinkEh 2022: 0x26be6e0; original functions/global_checking.c:90. */

/* ManalinkEh 2022: 0x26bea60; original functions/global_checking.c:374. */

/* ManalinkEh 2022: 0x26bebb0; original functions/global_checking.c:421. */

/* ManalinkEh 2022: 0x26bebe0; original functions/global_checking.c:425. */

/* ManalinkEh 2022: 0x26bf6c0; original functions/global_checking.c:638. */

/* ManalinkEh 2022: 0x26bf750; original functions/global_checking.c:654. */

/* ManalinkEh 2022: 0x26c0130; original functions/global_checking.c:848. */

#endif

#ifndef SHANDALAR
/* Recovered October 2022 split card helpers. */

/* ManalinkEh 2022: 0x26c0860; original functions/global_checking.c:1033. */

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline card mechanics; original native callers contain the expansions. */
static int check_multisubtype_by_csvid(int csvid, int sub1, int sub2, int sub3, int sub4, int sub5)
{
  return (sub1 > 0 && has_subtype_by_id(-1, csvid, sub1)) ||
         (sub2 > 0 && has_subtype_by_id(-1, csvid, sub2)) ||
         (sub3 > 0 && has_subtype_by_id(-1, csvid, sub3)) ||
         (sub4 > 0 && has_subtype_by_id(-1, csvid, sub4)) ||
         (sub5 > 0 && has_subtype_by_id(-1, csvid, sub5));
}

static int check_multisubtype_all_by_csvid(int csvid, int sub1, int sub2, int sub3, int sub4, int sub5)
{
  return (sub1 <= 0 || has_subtype_by_id(-1, csvid, sub1)) &&
         (sub2 <= 0 || has_subtype_by_id(-1, csvid, sub2)) &&
         (sub3 <= 0 || has_subtype_by_id(-1, csvid, sub3)) &&
         (sub4 <= 0 || has_subtype_by_id(-1, csvid, sub4)) &&
         (sub5 <= 0 || has_subtype_by_id(-1, csvid, sub5));
}
#endif
