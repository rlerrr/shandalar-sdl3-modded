#include "manalink.h"

/* Canonical recovered helper declarations. */
static int new_can_sacrifice_as_cost_impl(int player, int card, int amount,
                                               test_definition_t *test);
int new_can_sacrifice_as_cost(int player, int card, test_definition_t *test);
int max_can_sacrifice_as_cost(int player, int card, test_definition_t *test);
int new_can_sacrifice(int player, int card, int t_player, test_definition_t *test);
int new_get_special_permanent_for_sacrifice(int player, int card, test_definition_t *test);
int can_sacrifice_as_cost(int player, int amount, int type, int flag1, int subtype, int flag2,
                               int color, int flag3, int id, int flag4, int cc, int flag5);
int can_sacrifice_type_as_cost(int player, int amount, type_t type);
int can_sacrifice(int player, int t_player, int amount, type_t type, subtype_t subtype);
int impose_sacrifice(int player, int card, int t_player, int amount, int type, int flag1,
                          int subtype, int flag2, int color, int flag3, int id, int flag4, int cc,
                          int flag5);
int can_activate_altar_basic(int player, int card, int act_mode, int type);
static const char *can_sacrifice_this(int who_chooses, int player, int card, int unused_source_player, int unused_source_card);
int can_sacrifice_this_as_cost(int player, int card);
int can_cause_sacrifice(int player, int t_player);
int is_nice_creature_to_sacrifice(int player, int card);
int get_best_permanent_for_sacrifice(int player, int type);
int pick_creature_for_sacrifice(int player, int card, int cannot_cancel);
int pick_permanent_for_sacrifice(int player, int card, int type, int cannot_cancel);
int pick_special_permanent_for_sacrifice(int player, int card, int cannot_cancel, int type,
                                              int flag1, int subtype, int flag2, int color,
                                              int flag3, int id, int flag4, int cc, int flag5);
int sacrifice(int player, int card, int t_player, int cannot_cancel, int type, int flag1,
                   int subtype, int flag2, int color, int flag3, int id, int flag4, int cc,
                   int flag5);
void controller_sacrifices_a_creature(int player, int card);
void player_sacrifices_a_creature(int s_player, int s_card, int t_player);
int prevent_damage_sacrificing_a_creature(int player, int card, event_t event, int mode);
int check_battlefield_for_sacrifice(int player, int type, int flag1, int subtype, int flag2,
                                         int color, int flag3, int id, int flag4, int cc, int flag5);
int altar_basic_activation(int player, int card, int act_mode, int type);
int altar_basic(int player, int card, event_t event, int act_mode, int type);
int altar_extended(int player, int card, event_t event, int act_mode, int type, int flag1,
                        int subtype, int flag2, int color, int flag3, int id, int flag4, int cmc,
                        int flag5);
int sacrifice_and_report_value(int player, int card, int t_player, int flags,
                                    test_definition_t *test);
static void get_sacrifice_prompt_text(char *prompt, int maxlen, int type, int invert);
static int new_sacrifice_impl(int player, int card, int t_player, sacrifice_t options,
                                   test_definition_t *test, char *marked_for_sacrifice);
int new_sacrifice(int player, int card, int t_player, sacrifice_t options,
                       test_definition_t *test);
int mark_sacrifice(int player, int card, int t_player, sacrifice_t options,
                        test_definition_t *test, char *marked_for_sacrifice);
int controller_sacrifices_a_permanent(int player, int card, int req_type,
                                           sacrifice_t options);
int player_sacrifices_a_permanent(int src_player, int src_card, int t_player, int req_type,
                                       sacrifice_t options);
int player_sacrifices_a_hacked_land(int src_player, int src_card, int t_player,
                                         subtype_t land_subtype, sacrifice_t options);

// Returns nonzero if there's a non-humiliated Angel of Jubilation in play.  (check_battlefield_for_special_card returns true for humiliated ones.)

// Verify that "player" could sacrifice amount permanents matching test for paying a cost.

// Verify that a spell or an ability from "player" could make "t_player" sacrifice a specific type of permanent

/* ManalinkEh 2022: 0x26e36e0; original functions/sacrifices.c:4. */
static type_t types_cannot_sac(void)
{
  type_t rv = TYPE_NONE;
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < active_cards_count[p]; ++c)
    {
      card_instance_t *inst = in_play(p, c);
      if (inst && cards_data[inst->internal_card_id].id == CARD_ID_ANGEL_OF_JUBILATION &&
          !is_humiliated(p, c, AL_STATIC))
        rv |= TYPE_CREATURE;
      inst = in_play(p, c);
      if (inst && cards_data[inst->internal_card_id].id == CARD_ID_YASHARN_IMPLACABLE_EARTH &&
          !is_humiliated(p, c, AL_STATIC))
        rv = TARGET_TYPE_PLANESWALKER | TYPE_ARTIFACT | TYPE_ENCHANTMENT | TYPE_CREATURE;
    }
  return rv;
}

// only usable with basic land subtypes; use player_sacrifices_a_permanent if you just want a land

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

static int get_special_permanent_for_sacrifice(int player, int type, int flag1, int subtype, int flag2,
											   int color, int flag3, int id, int flag4, int cc, int flag5){
	test_definition_t test;
	new_default_test_definition(&test, type, "");
	test.type_flag = flag1;
	test.subtype = subtype;
	test.subtype_flag = flag2;
	test.color = color;
	test.color_flag = flag3;
	test.id = id;
	test.id_flag = flag4;
	test.cmc = cc;
	test.cmc_flag = flag5;
	return new_get_special_permanent_for_sacrifice(player, -1, &test);
}

#ifndef SHANDALAR
/* Recovered October 2022 sacrifice family. */

static test_definition_t *sacrifice_test;

/* ManalinkEh 2022: 0x26e3af0; original functions/sacrifices.c:19. */
int can_sacrifice_this_as_cost(int player, int card)
{
  int result;
  type_t test_type;
  result = check_special_flags2(player, card, 0x400000);
  if (result != 0)
  {
    return 0;
  }
  test_type = types_cannot_sac();
  result = is_what(player, card, test_type);
  return (unsigned int)(result == 0);
}

/* ManalinkEh 2022: 0x26e37b0; original functions/sacrifices.c:31. */
static int new_can_sacrifice_as_cost_impl(int player, int card, int amount, test_definition_t *test)
{
  type_t test_type;
  card_instance_t *instance;
  int result;
  int card_00;
  unsigned int sacrificable_count;

  test_type = types_cannot_sac();
  sacrificable_count = 0;
  if (0 < active_cards_count[player])
  {
    card_00 = 0;
    do
    {
      instance = in_play(player, card_00);
      if ((((instance != NULL) && (result = is_what(player, card_00, 0x47), result != 0)) &&
           ((card != card_00 || (test->not_me != 1)))) &&
          ((result = new_make_test_in_play(player, card_00, -1, test),
            result != 0 &&
                ((((test_type == TYPE_NONE ||
                    (result = is_what(player, card_00, test_type), result == 0)) &&
                   (result = check_special_flags2(player, card_00, 0x400000), result == 0)) &&
                  ((sacrificable_count = sacrificable_count + 1,
                    (char)((unsigned char)((unsigned int)amount >> 0x18) ^ 0xff) < '\0' &&
                        (amount <= (int)sacrificable_count))))))))
      {
        return 1;
      }
      card_00 = card_00 + 1;
    } while (card_00 < active_cards_count[player]);
  }
  return sacrificable_count & amount >> 0x1f;
}

/* ManalinkEh 2022: 0x26e3b50; original functions/sacrifices.c:53. */
int new_can_sacrifice_as_cost(int player, int card, test_definition_t *test)
{
  int result;
  result = new_can_sacrifice_as_cost_impl(player, card, (unsigned int)test->qty, test);
  return result;
}

/* ManalinkEh 2022: 0x26e3b70; original functions/sacrifices.c:58. */
int max_can_sacrifice_as_cost(int player, int card, test_definition_t *test)
{
  int result;
  result = new_can_sacrifice_as_cost_impl(player, card, -1, test);
  return result;
}

/* ManalinkEh 2022: 0x26e3b90; original functions/sacrifices.c:63. */
int can_sacrifice_as_cost(int player, int amount, int type, int flag1, int subtype, int flag2,
                               int color, int flag3, int id, int flag4, int cc, int flag5)
{
  int result;
  test_definition_t test;
  new_default_test_definition(&test, type, "");
  test.type_flag = (unsigned short)flag1;
  test.subtype = subtype;
  test.subtype_flag = (unsigned short)flag2;
  test.color = color;
  test.color_flag = (unsigned short)flag3;
  test.id = id;
  test.id_flag = (unsigned short)flag4;
  test.cmc = cc;
  test.cmc_flag = (unsigned short)flag5;
  test.qty = (unsigned short)amount;
  result = new_can_sacrifice_as_cost_impl(player, -1, amount & 0xffff, &test);
  return result;
}

/* ManalinkEh 2022: 0x26e3c20; original functions/sacrifices.c:79. */
int can_sacrifice_type_as_cost(int player, int amount, type_t type)
{
  int result;
  test_definition_t test;
  new_default_test_definition(&test, type, "");
  test.qty = (unsigned short)amount;
  result = new_can_sacrifice_as_cost_impl(player, -1, amount & 0xffff, &test);
  return result;
}

/* ManalinkEh 2022: 0x26e3990; original functions/sacrifices.c:87. */
static int can_cause_sacrifice_part(int player, int t_player)
{
  for (int c = 0; c < active_cards_count[t_player]; ++c)
  {
    if (in_play(t_player, c))
    {
      int id = get_id(t_player, c);
      if ((id == 0x30b1 || id == 0x4e30 || id == 0x2b35) &&
          !is_humiliated(t_player, c, AL_STATIC))
        return 0;
    }
  }
  return 1;
}

/* ManalinkEh 2022: 0x26e3c70; original functions/sacrifices.c:87. */
int can_cause_sacrifice(int player, int t_player)
{
  if (player == t_player)
    return 1;
  return can_cause_sacrifice_part(player, t_player);
}

/* ManalinkEh 2022: 0x26e3c90; original functions/sacrifices.c:105. */
int new_can_sacrifice(int player, int card, int t_player, test_definition_t *test)
{
  int c;
  int can_cause_sacrifice;
  card_instance_t *instance;
  int candidate_matches;
  unsigned int required_count;
  int matching_count;
  bool exclude_source = false;

  if (test == NULL)
  {
    if ((player != t_player) &&
        (can_cause_sacrifice = can_cause_sacrifice_part(player, t_player), can_cause_sacrifice == 0))
    {
      return 0;
    }
    required_count = 1;
  }
  else
  {
    required_count = (unsigned int)test->qty;
    if (required_count == 0)
    {
      return 1;
    }
    if (player == t_player)
    {
      if (test->not_me == 1)
        exclude_source = true;
    }
    else
    {
      if (can_cause_sacrifice_part(player, t_player) == 0)
      {
        return 0;
      }
    }
  }
  if (!exclude_source)
    card = -1;
  if (0 < active_cards_count[t_player])
  {
    matching_count = 0;
    c = 0;
    do
    {
      instance = in_play(t_player, c);
      if ((card != c) && (instance != NULL))
      {
        if (test == NULL)
        {
          candidate_matches = is_what(t_player, c, 0x47);
        }
        else
        {
          candidate_matches = new_make_test_in_play(t_player, c, -1, test);
        }
        if (((candidate_matches != 0) &&
             (candidate_matches = check_special_flags2(t_player, c, 0x400000),
              candidate_matches == 0)) &&
            (matching_count = matching_count + 1, (int)required_count <= matching_count))
        {
          return 1;
        }
      }
      c = c + 1;
    } while (c < active_cards_count[t_player]);
  }
  return 0;
}

/* ManalinkEh 2022: 0x26e3de0; original functions/sacrifices.c:128. */
int can_sacrifice(int player, int t_player, int amount, type_t type, subtype_t subtype)
{
  int result;
  test_definition_t test;
  new_default_test_definition(&test, type, "");
  test.subtype = subtype;
  test.qty = (unsigned short)amount;
  result = new_can_sacrifice(player, -1, t_player, &test);
  return result;
}

/* ManalinkEh 2022: 0x26e3e40; original functions/sacrifices.c:138. */
int is_nice_creature_to_sacrifice(int player, int card)
{
  uint8_t ai_category;
  card_data_t *creature_data;
  card_instance_t *instance;
  int result;

  creature_data = get_card_data(player, card);
  ai_category = creature_data->cc[2];
  if (ai_category == '\t')
  {
    result = is_what(player, card, 4);
    if (result == 0)
    {
      return 1;
    }
    ai_category = creature_data->cc[2];
  }
  if ((ai_category != '\x02') && (ai_category != '\x05'))
  {
    instance = get_card_instance(player, card);
    return instance->ext->sp_keywords2 >> 3 & 1;
  }
  return 1;
}

/* ManalinkEh 2022: 0x26e3ec0; original functions/sacrifices.c:156. */
int get_best_permanent_for_sacrifice(int player, int type)
{
  for (int card = 0; card < active_cards_count[player]; ++card)
    if (in_play(player, card) && is_what(player, card, type) &&
        !check_special_flags2(player, card, 0x400000) &&
        is_nice_creature_to_sacrifice(player, card))
    {
      card_instance_t *instance = get_card_instance(player, card);
      if (instance->kill_code < 2)
        return card;
    }
  return -1;
}

/* ManalinkEh 2022: 0x26e3f70; original functions/sacrifices.c:171. */
int new_get_special_permanent_for_sacrifice(int player, int card, test_definition_t *test)
{
  card_instance_t *instance;
  int scan_or_result;
  int candidate_value_or_result;
  int selected_card;
  int lowest_value;

  selected_card = 0;
  if (0 < active_cards_count[player])
  {
    do
    {
      instance = in_play(player, selected_card);
      if (((((instance != NULL) &&
             (scan_or_result = new_make_test_in_play(player, selected_card, -1, test),
              scan_or_result != 0)) &&
            (scan_or_result = is_nice_creature_to_sacrifice(player, selected_card),
             scan_or_result != 0)) &&
           (((scan_or_result = check_special_flags2(player, selected_card, 0x400000),
              scan_or_result == 0 && (instance = get_card_instance(player, selected_card),
                                      (instance->state & 0x100000) == 0)) &&
             ((test->not_me != 1 || (card != selected_card)))))) &&
          (instance->kill_code < 2))
      {
        return selected_card;
      }
      selected_card = selected_card + 1;
    } while (selected_card < active_cards_count[player]);
    if (0 < active_cards_count[player])
    {
      lowest_value = 1000;
      selected_card = -1;
      scan_or_result = 0;
      do
      {
        instance = in_play(player, scan_or_result);
        if ((((instance != NULL) &&
              (candidate_value_or_result = new_make_test_in_play(player, scan_or_result, -1, test),
               candidate_value_or_result != 0)) &&
             (((candidate_value_or_result = check_special_flags2(player, scan_or_result, 0x400000),
                candidate_value_or_result == 0 &&
                    (instance = get_card_instance(player, scan_or_result),
                     (instance->state & 0x100000) == 0)) &&
               (((test->not_me != 1 || (card != scan_or_result)) &&
                 (instance->kill_code < 2)))))) &&
            (candidate_value_or_result = get_base_value(player, scan_or_result),
             candidate_value_or_result < lowest_value))
        {
          lowest_value = get_base_value(player, scan_or_result);
          selected_card = scan_or_result;
        }
        scan_or_result = scan_or_result + 1;
      } while (scan_or_result < active_cards_count[player]);
      return selected_card;
    }
  }
  return -1;
}

/* ManalinkEh 2022: 0x26e3a30; original functions/sacrifices.c:219. */
static void get_sacrifice_prompt_text(char *prompt, int maxlen, int type, int invert)
{
  const char *type_description;
  char typetext[100];
  if (ai_is_speculating == 1)
  {
    *prompt = '\0';
    return;
  }
  if (invert != 0)
  {
    type_description = type_text(typetext, 100, type & (TARGET_TYPE_PLANESWALKER | TYPE_PERMANENT),
                                 TYPETEXT_INVERT);
    scnprintf(prompt, maxlen, "Select %s permanent to sacrifice.", type_description);
    return;
  }
  type_description =
      type_text(typetext, 100, type & (TARGET_TYPE_PLANESWALKER | TYPE_PERMANENT), TYPETEXT_0);
  scnprintf(prompt, maxlen, "Select %s to sacrifice.", type_description);
  return;
}

/* ManalinkEh 2022: 0x26e3940; original functions/sacrifices.c:234. */
static const char *can_sacrifice_this(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
  int result;
  card_instance_t *instance;
  result = check_special_flags2(player, card, 0x400000);
  if ((result == 0) && (instance = get_card_instance(player, card), instance->kill_code == '\0'))
  {
    return NULL;
  }
  return "can\'t sacririce this";
}

/* ManalinkEh 2022: 0x26e4a50; original functions/sacrifices.c:242. */
int pick_creature_for_sacrifice(int player, int card, int cannot_cancel)
{
  card_instance_t *instance;
  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  td.allow_cancel = (unsigned int)(cannot_cancel == 0) * 2;
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.illegal_abilities = 0;
  td.who_chooses = player;
  td.extra = target_predicate_extra(can_sacrifice_this);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    if ((player == 1) && (result = get_best_permanent_for_sacrifice(1, 2), result != -1))
    {
      return result;
    }
    result = new_pick_target(&td, "LORD_OF_THE_PIT", 0, 0);
    if (result != 0)
    {
      instance->number_of_targets = '\x01';
      return instance->targets[0].card;
    }
  }
  return -1;
}

/* ManalinkEh 2022: 0x26e4b30; original functions/sacrifices.c:279. */
int pick_permanent_for_sacrifice(int player, int card, int type, int cannot_cancel)
{
  card_instance_t *instance;
  int result;
  target_definition_t td;
  char prompt[200];

  default_target_definition(player, card, &td, type);
  td.allow_cancel = 1 - cannot_cancel;
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.illegal_abilities = 0;
  td.who_chooses = player;
  td.extra = target_predicate_extra(can_sacrifice_this);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    if (player == 1)
    {
      result = get_best_permanent_for_sacrifice(1, type);
      if (result != -1)
      {
        return result;
      }
    }
    get_sacrifice_prompt_text(prompt, 200, type, 0);
    select_target(player, card, &td, prompt, instance->targets);
    if (spell_fizzled != 1)
    {
      instance->number_of_targets = '\x01';
      return instance->targets[0].card;
    }
  }
  return -1;
}

/* ManalinkEh 2022: 0x26e4c50; original functions/sacrifices.c:311. */
int pick_special_permanent_for_sacrifice(int player, int card, int cannot_cancel, int type,
                                              int flag1, int subtype, int flag2, int color,
                                              int flag3, int id, int flag4, int cc, int flag5)
{
  card_instance_t *instance;
  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, type);
  td.allow_cancel = 1 - cannot_cancel;
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.illegal_abilities = 0;
  td.who_chooses = player;
  td.extra = target_predicate_extra(can_sacrifice_this);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    if (player == 1)
    {
      test_definition_t test;
      new_default_test_definition(&test, type, "");
      test.type_flag = (uint16_t)flag1;
      test.subtype = subtype;
      test.subtype_flag = (uint16_t)flag2;
      test.color = color;
      test.color_flag = (uint16_t)flag3;
      test.id = id;
      test.id_flag = (uint16_t)flag4;
      test.cmc = cc;
      test.cmc_flag = (uint16_t)flag5;
      result = new_get_special_permanent_for_sacrifice(1, -1, &test);
      return result;
    }
    char prompt[200];
    get_sacrifice_prompt_text(prompt, 200, type, (unsigned int)(flag1 == 1));
    select_target(player, card, &td, prompt, instance->targets);
    if (spell_fizzled != 1)
    {
      result =
          make_test_in_play(instance->targets[0].player, instance->targets[0].card, -1, type,
                                 flag1, subtype, flag2, color, flag3, id, flag4, cc, flag5);
      if (result != 0)
      {
        instance->number_of_targets = '\x01';
        return instance->targets[0].card;
      }
    }
  }
  return -1;
}

/* ManalinkEh 2022: 0x26e38d0; original functions/sacrifices.c:348. */
static const char *passes_test_definition_and_not_dying(int who_chooses, int player, int card,
                                                             int targeting_player,
                                                             int targeting_card)
{
  int result;
  card_instance_t *instance;
  result = new_make_test_in_play(player, card, -1, sacrifice_test);
  if (((result != 0) &&
       (result = check_special_flags2(player, card, 0x400000), result == 0)) &&
      (instance = get_card_instance(player, card), instance->kill_code == '\0'))
  {
    return NULL;
  }
  return "Illegal sacrifice";
}

/* ManalinkEh 2022: 0x26e4110; original functions/sacrifices.c:355. */
static int new_sacrifice_impl(int player, int card, int t_player, sacrifice_t options,
                              test_definition_t *test, char *marked_for_sacrifice)
{
  int c;
  int csvid;
  uint8_t saved_target_count;
  uint16_t requested_type_mode;
  int32_t saved_target_player;
  int32_t saved_target_card;
  int saved_fizzle_or_scan;
  card_instance_t *instance;
  int available_card_or_result;
  card_instance_t *selected_instance;
  type_t prohibited_types;
  unsigned int required_types_or_marking_state;
  int scan_limit_card_or_result;
  unsigned int requested_types;
  int last_selected_player;
  const char *prompt;
  int last_selected_card;
  unsigned int selection_limit;
  int selected_count_or_packed_choice;
  target_definition_t td;
  char marked[151];
  char default_prompt[200];

  base_target_definition(player, card, &td, 0x47);
  td.allow_cancel = options & (SAC_DONE | SAC_NO_CANCEL) ^ SAC_NO_CANCEL;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = target_predicate_extra(passes_test_definition_and_not_dying);
  if (test->not_me == 1)
  {
    td.special = TARGET_SPECIAL_NOT_ME | TARGET_SPECIAL_EXTRA_FUNCTION;
  }
  requested_types = test->type & 0x147;
  required_types_or_marking_state = td.required_type;
  if (((requested_types != 0) &&
       (requested_type_mode = test->type_flag, required_types_or_marking_state = requested_types,
        requested_type_mode != 0)) &&
      (required_types_or_marking_state = td.required_type, requested_type_mode == 1))
  {
    td.illegal_type = requested_types;
  }
  td.required_type = required_types_or_marking_state;
  td.who_chooses = t_player;
  td.allowed_controller = t_player;
  td.preferred_controller = t_player;
  if (((options & SAC_AS_COST) != 0) &&
      (prohibited_types = types_cannot_sac(), prohibited_types != TYPE_NONE))
  {
    td.illegal_type = td.illegal_type | prohibited_types;
  }
  if ((((options & SAC_CAUSED) != 0) && (player != t_player)) &&
      (saved_fizzle_or_scan = can_cause_sacrifice_part(player, t_player), saved_fizzle_or_scan == 0))
  {
    spell_fizzled = 1;
    return 0;
  }
  instance = get_card_instance(player, card);
  saved_fizzle_or_scan = spell_fizzled;
  sacrifice_test = test;
  if ((options & SAC_ALL_OR_NONE) == 0)
  {
    if (can_target(&td) == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  else
  {
    available_card_or_result = target_available(player, card, &td);
    if (available_card_or_result < (int)(unsigned int)test->qty)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  selection_limit = (unsigned int)test->qty;
  available_card_or_result = target_available(player, card, &td);
  if (available_card_or_result < (int)selection_limit)
  {
    selection_limit = target_available(player, card, &td);
  }
  if (test->message[0] == '\0')
  {
    get_sacrifice_prompt_text(default_prompt, 200, requested_types,
                              (unsigned int)(test->type_flag == 1));
    prompt = default_prompt;
  }
  else
  {
    prompt = test->message;
  }
  saved_target_count = instance->number_of_targets;
  saved_target_player = instance->targets[0].player;
  saved_target_card = instance->targets[0].card;
  memset(marked, 0, sizeof(marked));
  last_selected_player = -1;
  last_selected_card = -1;
  selected_count_or_packed_choice = 0;
  bool accepted;
  if ((int)selection_limit < 1)
    accepted = spell_fizzled != 1 && (options & SAC_OPTIONAL) != 0;
  else
  {
    available_card_or_result = -1;
    if (player == t_player)
      available_card_or_result = card;
    bool ended_early = false;
    bool cancelled = false;
    do
    {
      target_t tgt;
      spell_fizzled = 0;
      tgt.player = -1;
      tgt.card = -1;
      bool use_dialog = true;
      if (ai_is_speculating == 1 || (t_player == 1 && (trace_mode & 2U) == 0))
      {
        scan_limit_card_or_result =
            new_get_special_permanent_for_sacrifice(t_player, available_card_or_result, test);
        if (scan_limit_card_or_result != -1)
        {
          tgt.player = t_player;
          tgt.card = scan_limit_card_or_result;
          use_dialog = false;
        }
      }
      if (use_dialog)
      {
        sacrifice_test = test;
        instance->number_of_targets = 0;
        select_target(player, card, &td, prompt, &tgt);
      }
      if (tgt.card >= 0)
      {
        if (marked[tgt.card] == '\0')
        {
          selected_instance = get_card_instance(tgt.player, tgt.card);
          if (ai_is_speculating == 1)
          {
            required_types_or_marking_state = 0x300000;
          }
          else
          {
            required_types_or_marking_state = 0x200000;
            if (t_player == 1)
            {
              required_types_or_marking_state =
                  (((trace_mode & 2U) == 0) ? 0x300000U : 0x200000U);
            }
          }
          selected_instance->state = selected_instance->state | required_types_or_marking_state;
          selected_count_or_packed_choice = selected_count_or_packed_choice + 1;
          last_selected_card = tgt.card;
          marked[tgt.card] = '\x01';
          last_selected_player = tgt.player;
        }
        else
        {
          selected_instance = get_card_instance(tgt.player, tgt.card);
          selected_count_or_packed_choice = selected_count_or_packed_choice + -1;
          selected_instance->state = selected_instance->state & 0xffdfffff;
          marked[tgt.card] = '\0';
        }
      }
      else if ((options & (SAC_DONE | SAC_NO_CANCEL)) != SAC_NO_CANCEL)
      {
        ended_early = true;
        cancelled = tgt.card == -1 ||
                    (selected_count_or_packed_choice == 0 && (options & SAC_OPTIONAL) == 0);
        break;
      }
    } while (selected_count_or_packed_choice < (int)selection_limit);
    instance->number_of_targets = saved_target_count;
    instance->targets[0].player = saved_target_player;
    instance->targets[0].card = saved_target_card;
    accepted = !cancelled && (ended_early || spell_fizzled != 1) &&
               ((int)selection_limit <= selected_count_or_packed_choice ||
                (options & SAC_ALL_OR_NONE) == 0);
  }
  if (accepted)
  {
    spell_fizzled = saved_fizzle_or_scan;
    if ((options & SAC_SET_TARGETS) != 0)
    {
      instance->number_of_targets = '\x01';
      instance->targets[0].player = last_selected_player;
      instance->targets[0].card = last_selected_card;
      available_card_or_result = get_color(last_selected_player, last_selected_card);
      instance->targets[1].player = available_card_or_result;
      csvid = get_id(last_selected_player, last_selected_card);
      instance->targets[1].card = csvid;
      available_card_or_result = get_power(last_selected_player, last_selected_card);
      instance->targets[2].player = available_card_or_result;
      available_card_or_result = get_toughness(last_selected_player, last_selected_card);
      instance->targets[2].card = available_card_or_result;
      selected_instance = get_card_instance(last_selected_player, last_selected_card);
      instance->targets[3].card = selected_instance->internal_card_id;
      available_card_or_result = get_id(player, card);
      if (available_card_or_result == 0x2f91)
      {
        available_card_or_result =
            has_subtype(last_selected_player, last_selected_card, SUBTYPE_HUMAN);
        instance->targets[3].player = available_card_or_result;
      }
      else if ((available_card_or_result == 0x24f3) &&
               (available_card_or_result =
                    has_subtype(last_selected_player, last_selected_card, SUBTYPE_GOBLIN),
                available_card_or_result != 0))
      {
        token_generation_t token;
        default_token_definition(player, card, CARD_ID_GOBLIN_ROGUE, &token);
        token.t_player = last_selected_player;
        token.qty = '\x02';
        token.action = TOKEN_ACTION_HASTE;
        generate_token(&token);
      }
    }
    c = 0;
    if (0 < active_cards_count[t_player])
    {
      do
      {
        if (marked[c] != '\0')
        {
          instance = get_card_instance(t_player, c);
          c = c + 1;
          instance->state = instance->state & 0xffdfffff | 0x100000;
        }
        else
          c = c + 1;
        scan_limit_card_or_result = active_cards_count[t_player];
      } while (c < scan_limit_card_or_result);
      if ((options & SAC_JUST_MARK) == 0 && scan_limit_card_or_result > 0)
      {
        c = 0;
        do
        {
          if (marked[c] != '\0')
          {
            instance = get_card_instance(t_player, c);
            instance->state = instance->state & 0xffefffff;
            instance = in_play(t_player, c);
            if (instance != NULL)
            {
              scan_limit_card_or_result = c + 1;
              kill_card(t_player, c, KILL_SACRIFICE);
              c = scan_limit_card_or_result;
              if (active_cards_count[t_player] <= scan_limit_card_or_result)
                break;
              continue;
            }
          }
          c = c + 1;
        } while (c < active_cards_count[t_player]);
      }
    }
    if ((options & SAC_RETURN_CHOICE) != 0)
    {
      selected_count_or_packed_choice = (int32_t)((uint16_t)selected_count_or_packed_choice |
                                                  ((uint32_t)(uint8_t)last_selected_player << 16) |
                                                  ((uint32_t)(uint8_t)last_selected_card << 24));
    }
    if (marked_for_sacrifice == NULL)
    {
      spell_fizzled = saved_fizzle_or_scan;
      return selected_count_or_packed_choice;
    }
    memcpy(marked_for_sacrifice, marked, sizeof(marked));
    spell_fizzled = saved_fizzle_or_scan;
    return selected_count_or_packed_choice;
  }
  saved_fizzle_or_scan = 0;
  spell_fizzled = 1;
  if (0 < active_cards_count[t_player])
  {
    do
    {
      while (marked[saved_fizzle_or_scan] == '\0')
      {
        saved_fizzle_or_scan = saved_fizzle_or_scan + 1;
        if (active_cards_count[t_player] <= saved_fizzle_or_scan)
        {
          return 0;
        }
      }
      instance = get_card_instance(t_player, saved_fizzle_or_scan);
      saved_fizzle_or_scan = saved_fizzle_or_scan + 1;
      available_card_or_result = active_cards_count[t_player];
      instance->state = instance->state & 0xffcfffff;
    } while (saved_fizzle_or_scan < available_card_or_result);
  }
  return 0;
}

/* ManalinkEh 2022: 0x26e4e30; original functions/sacrifices.c:536. */
int new_sacrifice(int player, int card, int t_player, sacrifice_t options,
                       test_definition_t *test)
{
  int result;
  result = new_sacrifice_impl(player, card, t_player, options, test, NULL);
  return result;
}

/* ManalinkEh 2022: 0x26e4e60; original functions/sacrifices.c:541. */
int mark_sacrifice(int player, int card, int t_player, sacrifice_t options,
                        test_definition_t *test, char *marked_for_sacrifice)
{
  int result;
  result = new_sacrifice_impl(player, card, t_player, options, test, marked_for_sacrifice);
  return result;
}

/* ManalinkEh 2022: 0x26e4e90; original functions/sacrifices.c:546. */
int sacrifice(int player, int card, int t_player, int cannot_cancel, int type, int flag1,
                   int subtype, int flag2, int color, int flag3, int id, int flag4, int cc,
                   int flag5)
{
  int result;
  sacrifice_t options;
  test_definition_t test;

  new_default_test_definition(&test, type, "");
  test.type_flag = (unsigned short)flag1;
  test.subtype = subtype;
  test.subtype_flag = (unsigned short)flag2;
  test.color = color;
  test.color_flag = (unsigned short)flag3;
  test.id = id;
  test.id_flag = (unsigned short)flag4;
  test.cmc = cc;
  test.cmc_flag = (unsigned short)flag5;
  if (cannot_cancel == 1)
  {
    options = SAC_SET_TARGETS | SAC_NO_CANCEL;
  }
  else if (cannot_cancel == -1)
  {
    options = SAC_SET_TARGETS | SAC_DONE;
  }
  else
  {
    options = cannot_cancel | SAC_SET_TARGETS;
  }
  result = new_sacrifice_impl(player, card, t_player, options, &test, NULL);
  return result;
}

/* ManalinkEh 2022: 0x26e4f40; original functions/sacrifices.c:565. */
int controller_sacrifices_a_permanent(int player, int card, int req_type,
                                           sacrifice_t options)
{
  int result;
  test_definition_t test;
  new_default_test_definition(&test, req_type, "");
  result = new_sacrifice_impl(player, card, player, options, &test, NULL);
  return result;
}

/* ManalinkEh 2022: 0x26e4f90; original functions/sacrifices.c:572. */
int player_sacrifices_a_permanent(int src_player, int src_card, int t_player, int req_type,
                                       sacrifice_t options)
{
  int result;
  test_definition_t test;
  new_default_test_definition(&test, req_type, "");
  result = new_sacrifice_impl(src_player, src_card, t_player, options, &test, NULL);
  return result;
}

/* ManalinkEh 2022: 0x26e4fe0; original functions/sacrifices.c:580. */
int player_sacrifices_a_hacked_land(int src_player, int src_card, int t_player,
                                         subtype_t land_subtype, sacrifice_t options)
{
  int result;
  const char *msg;
  test_definition_t test;

  if ((src_player != t_player) && (result = can_cause_sacrifice_part(src_player, t_player), result == 0))
  {
    return 0;
  }
  msg = get_hacked_land_text(src_player, src_card, "Select %a to sacrifice.", land_subtype);
  new_default_test_definition(&test, 1, msg);
  test.subtype = get_hacked_subtype(src_player, src_card, land_subtype);
  result = new_sacrifice_impl(src_player, src_card, t_player, options, &test, NULL);
  return result;
}

/* ManalinkEh 2022: 0x26e5080; original functions/sacrifices.c:593. */
int impose_sacrifice(int player, int card, int t_player, int amount, int type, int flag1,
                     int subtype, int flag2, int color, int flag3, int id, int flag4, int cc,
                     int flag5)
{
  int i;
  int can_cause_sacrifice;

  if (amount < 1)
  {
    return 0;
  }
  if ((player != t_player) &&
      (can_cause_sacrifice = can_cause_sacrifice_part(player, t_player), can_cause_sacrifice == 0))
  {
    return 0;
  }
  i = 0;
  do
  {
    if (sacrifice(player, card, t_player, 1, type, flag1, subtype, flag2, color, flag3, id, flag4,
                  cc, flag5) == 0)
    {
      return i;
    }
    i = i + 1;
  } while (amount != i);
  return i;
}

/* ManalinkEh 2022: 0x26e5140; original functions/sacrifices.c:618. */
void controller_sacrifices_a_creature(int player, int card)
{
  sacrifice(player, card, player, 1, 2, 0, 0, 0, 0, 0, 0, 0, -1, 0);
  return;
}

/* ManalinkEh 2022: 0x26e51c0; original functions/sacrifices.c:622. */
void player_sacrifices_a_creature(int s_player, int s_card, int t_player)
{
  sacrifice(s_player, s_card, t_player, 1, 2, 0, 0, 0, 0, 0, 0, 0, -1, 0);
  return;
}

/* ManalinkEh 2022: 0x26e5240; original functions/sacrifices.c:626. */
int prevent_damage_sacrificing_a_creature(int player, int card, event_t event, int mode)
{
  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.extra = damage_card;
  td.allow_cancel = 0;
  target_definition_t td1;
  default_target_definition(player, card, &td1, TYPE_CREATURE);
  td1.allowed_controller = player;
  td1.preferred_controller = player;
  td1.illegal_abilities = 0;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (can_sacrifice_as_cost(player, 1, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0) &&
        can_target(&td))
    {
      if ((mode == 1 && has_mana(player, COLOR_COLORLESS, 1)) || mode == 2)
        return 99;
      return 0;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    bool resolved = mode == 2;
    if (mode == 1)
    {
      charge_mana(player, COLOR_COLORLESS, 1);
      resolved = spell_fizzled != 1;
    }
    if (resolved && controller_sacrifices_a_permanent(player, card, TYPE_CREATURE, 0))
      pick_target(&td, "TARGET_DAMAGE");
    else
      spell_fizzled = 1;
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
    if (target->info_slot > 0)
      target->info_slot = 0;
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x26e5460; original functions/sacrifices.c:684. */
int check_battlefield_for_sacrifice(int player, int type, int flag1, int subtype, int flag2,
                                    int color, int flag3, int id, int flag4, int cc, int flag5)
{
  card_instance_t *instance;
  int i;
  int scan_or_matches;
  if (player < 2)
  {
    if (0 < active_cards_count[player])
    {
      i = 0;
      do
      {
        instance = in_play(player, i);
        if ((instance != NULL) &&
            (scan_or_matches = make_test_in_play(player, i, -1, type, flag1, subtype, flag2, color,
                                                 flag3, id, flag4, cc, flag5),
             scan_or_matches != 0))
        {
          return 1;
        }
        i = i + 1;
      } while (i < active_cards_count[player]);
    }
  }
  else if (player == 2)
  {
    i = 0;
    do
    {
      scan_or_matches = 0;
      if (0 < active_cards_count[i])
      {
        do
        {
          instance = in_play(i, scan_or_matches);
          if ((instance != NULL) &&
              (make_test_in_play(i, scan_or_matches, -1, type, flag1, subtype, flag2, color, flag3,
                                 id, flag4, cc, flag5) != 0))
          {
            return 1;
          }
          scan_or_matches = scan_or_matches + 1;
        } while (scan_or_matches < active_cards_count[i]);
      }
      i = i + 1;
    } while (i != 2);
  }
  return 0;
}

/* ManalinkEh 2022: 0x26e55d0; original functions/sacrifices.c:717. */
int can_activate_altar_basic(int player, int card, int act_mode, int type)
{
  int result;
  unsigned int available_modes;

  available_modes = 0;
  result = is_sick(player, card);
  if (result == 0)
  {
    available_modes = act_mode & 1;
  }
  result = is_animated_and_sick(player, card);
  if ((result == 0) && ((act_mode & 2U) != 0))
  {
    available_modes = available_modes + 2;
  }
  result = is_tapped(player, card);
  if ((result == 0) && ((act_mode & 4U) != 0))
  {
    available_modes = available_modes + 4;
  }
  if ((current_phase == 4) && ((act_mode & 8U) != 0))
  {
    available_modes = available_modes + 8;
  }
  if ((current_turn == player) && ((act_mode & 0x10U) != 0))
  {
    available_modes = available_modes + 0x10;
  }
  if (available_modes != act_mode)
  {
    return 0;
  }
  return can_sacrifice_as_cost(player, 1, type, 0, 0, 0, 0, 0, 0, 0, -1, 0);
}

/* ManalinkEh 2022: 0x26e56f0; original functions/sacrifices.c:741. */
int altar_basic_activation(int player, int card, int act_mode, int type)
{
  card_instance_t *instance;
  int sacrificed_card_or_paid;
  int sacrifice_stat;

  instance = get_card_instance(player, card);
  sacrificed_card_or_paid = charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0);
  if ((sacrificed_card_or_paid != 0) &&
      (sacrificed_card_or_paid = pick_permanent_for_sacrifice(player, card, type, 0),
       sacrificed_card_or_paid != -1))
  {
    instance->number_of_targets = '\x01';
    sacrifice_stat = get_power(player, sacrificed_card_or_paid);
    instance->targets[1].player = sacrifice_stat;
    sacrifice_stat = get_toughness(player, sacrificed_card_or_paid);
    instance->targets[1].card = sacrifice_stat;
    sacrifice_stat = get_color(player, sacrificed_card_or_paid);
    instance->targets[2].player = sacrifice_stat;
    sacrifice_stat = get_cmc(player, sacrificed_card_or_paid);
    instance->targets[2].card = sacrifice_stat;
    kill_card(player, sacrificed_card_or_paid, KILL_SACRIFICE);
    if ((act_mode & 4U) == 0)
    {
      return 1;
    }
    tap_card(player, card);
    return 1;
  }
  return 0;
}

/* ManalinkEh 2022: 0x26e5810; original functions/sacrifices.c:763. */
int altar_basic(int player, int card, event_t event, int act_mode, int type)
{
  int result;
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    if (result != 0)
    {
      result = can_activate_altar_basic(player, card, act_mode, type);
      return result;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    result = altar_basic_activation(player, card, act_mode, type);
    if (result == 0)
    {
      spell_fizzled = 1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x26e5900; original functions/sacrifices.c:782. */
int altar_extended(int player, int card, event_t event, int act_mode, int type, int flag1,
                   int subtype, int flag2, int color, int flag3, int id, int flag4, int cmc,
                   int flag5)
{
  card_instance_t *instance;
  int sacrificed_card;
  int sacrifice_stat;
  unsigned int available_modes;

  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (can_use_activated_abilities(player, card) != 0)
    {
      int is_summoning_sick = is_sick(player, card);
      available_modes = 0;
      if (!is_summoning_sick)
      {
        available_modes = act_mode & 1;
      }
      if (!is_animated_and_sick(player, card) && (act_mode & 2U))
      {
        available_modes = available_modes + 2;
      }
      if (!is_tapped(player, card) && (act_mode & 4U))
      {
        available_modes = available_modes + 4;
      }
      if ((current_phase == 4) && ((act_mode & 8U) != 0))
      {
        available_modes = available_modes + 8;
      }
      if ((current_turn == player) && ((act_mode & 0x10U) != 0))
      {
        available_modes = available_modes + 0x10;
      }
      if (available_modes == act_mode)
      {
        return can_sacrifice_as_cost(player, 1, type, flag1, subtype, flag2, color, flag3, id,
                                     flag4, cmc, flag5);
      }
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    sacrificed_card = pick_special_permanent_for_sacrifice(
        player, card, 0, type, flag1, subtype, flag2, color, flag3, id, flag4, cmc, flag5);
    if (sacrificed_card == -1)
    {
      spell_fizzled = 1;
    }
    else
    {
      sacrifice_stat = get_power(player, sacrificed_card);
      instance->targets[1].player = sacrifice_stat;
      sacrifice_stat = get_toughness(player, sacrificed_card);
      instance->targets[1].card = sacrifice_stat;
      sacrifice_stat = get_color(player, sacrificed_card);
      instance->targets[2].player = sacrifice_stat;
      sacrifice_stat = get_cmc(player, sacrificed_card);
      instance->targets[2].card = sacrifice_stat;
      kill_card(player, sacrificed_card, KILL_SACRIFICE);
      if ((act_mode & 4U) != 0)
      {
        tap_card(player, card);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x26e5b80; original functions/sacrifices.c:830. */
int sacrifice_and_report_value(int player, int card, int t_player, int flags,
                               test_definition_t *test)
{
  unsigned int packed_choice_or_player;
  card_instance_t *instance;
  int32_t result;
  unsigned int card_00;

  packed_choice_or_player =
      new_sacrifice_impl(player, card, t_player,
                         (flags >> 0x1f & 0x21U) + (SAC_RETURN_CHOICE | SAC_JUST_MARK), test, NULL);
  if ((int)packed_choice_or_player < 0)
  {
    result = -1;
  }
  else
  {
    card_00 = packed_choice_or_player >> 0x18;
    packed_choice_or_player = (int)packed_choice_or_player >> 0x10 & 0xff;
    instance = get_card_instance(packed_choice_or_player, card_00);
    if ((flags & 1U) == 0)
    {
      result = instance->internal_card_id;
    }
    else
    {
      result = get_power(packed_choice_or_player, card_00);
    }
    if ((flags & 2U) != 0)
    {
      result = get_toughness(packed_choice_or_player, card_00);
    }
    if ((flags & 4U) != 0)
    {
      result = get_color(packed_choice_or_player, card_00);
    }
    if ((flags & 0x10U) != 0)
    {
      result = get_cmc(packed_choice_or_player, card_00);
    }
    kill_card(packed_choice_or_player, card_00, KILL_SACRIFICE);
  }
  return result;
}

#endif
