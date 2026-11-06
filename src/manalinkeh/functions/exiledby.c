#include <ctype.h>
#include <limits.h>
#include "manalink.h"

// Functions to remember which cards are exiled by which.

// empty effect, used only for data storage and comparison purposes

static int exiledby_legacy(int player, int card, event_t event)
{
  /* Targets:
   * [0] through [17]: each of .player and .card are an iid of an exiled card.  0x80000000 is set if the card is owned by player 1.
   * [18].player: unused
   * [18].card: stores identifying id if this is detached - see exiledby_detach() */
  return 0;
}

static int exiledby_legacymatches(card_instance_t* instance, int player, int card)
{
  if (instance->internal_card_id != LEGACY_EFFECT_CUSTOM)
	return 0;

  if (instance->info_slot != (intptr_t)exiledby_legacy)
	return 0;

  if ((instance->state & (STATE_OUBLIETTED | STATE_INVISIBLE | STATE_IN_PLAY)) != STATE_IN_PLAY)	// not just in_play(player, card), since player may be -1 or -2
	return 0;

  if (player < 0)
	return instance->targets[18].card == card;
  else
	return instance->damage_target_player == player && instance->damage_target_card == card;
}

/* player/card: The card that exiled the cards to search for.
 * csvid: Csvid to display in the dialog.  Typically the same as player/card's; but must be provided since player/card may have been obliterated.
 * mode, testarg: Action and constraints per exiledby_choose_mode_t, documented in manalink.h.
 * cardtype_txt: Descriptor of cards exiled (e.g. "creature"), or NULL if arbitrary cards.
 * faceup: Whether the cards are exiled face-up (and thus should exist in rfg_ptr[][]) or face-down (and thus should not yet). */
intptr_t exiledby_choose(int player, int card, int csvid, exiledby_choose_mode_t mode, int testarg,
                    const char *cardtype_txt, int faceup)
{
  int iids[1000];
  int *locs[1000];
  int num_found = 0;

  int tests = mode & ~EXBY_MODE_MASK;
  mode &= EXBY_MODE_MASK;

  if (mode != EXBY_MAX_VALUE && mode != EXBY_FIRST_FOUND)
  {
    memset(iids, -1, 1000 * sizeof(int));
    memset(locs, 0, 1000 * sizeof(int *));
  }

  int p = player;
  if (player < 0)
    player += 2;

  card_instance_t *instance;
  int i, leg, field, ai_choice = -1, highest_value = INT_MIN, can_cast = 0;
  /* We look at all cards each call, so if a card's no longer exiled, we remove it immediately.  Not
   * quite as good as we'd like - if you exile two Atogs, then Pull From Reality one, it'll still be
   * listed twice. */
  for (leg = 0; leg < active_cards_count[player]; ++leg)
  {
    instance = get_card_instance(player, leg);
    if (exiledby_legacymatches(instance, p, card))
      for (i = 0; i < 18; ++i)
        for (field = 0; field < 2; ++field)
        {
          int *loc = field == 0 ? &instance->targets[i].player : &instance->targets[i].card;

          if (*loc != -1)
          {
            int plr = (*loc & 0x80000000) ? 1 : 0;
            int iid = *loc & ~0x80000000;
            if (faceup > 0 && !check_rfg(plr, cards_data[iid].id))
              *loc = -1;
            else
            {
              // > 0: pass test.  == 0: pass test, but just barely.  < 0: fail.
              int test_cmc =
                  (tests & EXBY_TEST_CMC_LE) ? testarg - get_cmc_by_id(cards_data[iid].id) : 99;
              if (test_cmc < 0)
                continue;

              int test_iid =
                  ((tests & EXBY_TEST_IID) && !(iid == testarg || cards_data[iid].id == csvid))
                      ? -1
                      : 99;
              if (test_iid < 0)
                continue;

              int test_can_cast = 99;
              if (tests & EXBY_TEST_CAN_CAST)
              {
                if (testarg)
                  test_can_cast = can_legally_play_iid_now(player, iid, testarg);
                else
                  test_can_cast = can_legally_play_iid(player, iid);
                if (!test_can_cast)
                  continue;
                else if (test_can_cast != 99)
                  test_can_cast = 1;
              }

              int test_has_mana_to_cast = ((tests & EXBY_TEST_HAS_MANA_TO_CAST) &&
                                           !has_mana_to_cast_iid(player, testarg, iid))
                                              ? -1
                                              : 99;
              if (tests & EXBY_TEST_HAS_GENERIC_MANA_TO_PAY_CMC)
              {
                int cmc = get_cmc_by_internal_id(iid);
                test_has_mana_to_cast =
                    has_mana_for_activated_ability(player, card, MANACOST_X(cmc)) ? 99 : -1;
              }
              if (test_has_mana_to_cast < 0)
                continue;

              int pass = MIN(test_cmc, test_iid);
              pass = MIN(pass, test_can_cast);

              if (pass < 0)
                continue;

              if (mode == EXBY_FIRST_FOUND)
                return (intptr_t)loc;

              if (mode == EXBY_CAN_CAST)
              {
                if (test_can_cast == 99)
                  return 99;
                can_cast = 1;
              }

              if (mode == EXBY_MAX_VALUE || mode == EXBY_CHOOSE)
              {
                int val = my_base_value_by_id(iid);
                if (pass == 0)
                  val /= 2;
                if (val > highest_value)
                {
                  highest_value = val;
                  ai_choice = num_found;
                }
              }

              if (mode == EXBY_CHOOSE)
              {
                iids[num_found] = iid;
                locs[num_found] = loc;
                ++num_found;
              }
            }
          }
        }
  }

  if (mode == EXBY_MAX_VALUE)
    return highest_value;

  if (mode == EXBY_FIRST_FOUND)
    return 0;

  if (mode == EXBY_CAN_CAST)
    return can_cast;

  if (IS_AI(player))
  {
    if (ai_choice == -1)
      return 0;
    else
      return (intptr_t)(locs[ai_choice]);
  }

  char buf[200];
  if (num_found == 0)
  {
    sprintf(buf, "No %s%scards have been exiled by %s.", cardtype_txt ? cardtype_txt : "",
            cardtype_txt ? " " : "", cards_ptr[csvid]->name);
    DIALOG(player, card, EVENT_ACTIVATE, DLG_FULLCARD_CSVID(csvid), DLG_MSG(buf));
    return 0;
  }

  sprintf(buf, "%s%scards exiled by %s", cardtype_txt ? cardtype_txt : "", cardtype_txt ? " " : "",
          cards_ptr[csvid]->name);
  buf[0] = toupper(buf[0]);
  int selected = show_deck(player, iids, num_found, buf, 0, gs_done_008b40e0);

  if (selected == -1)
  {
    return 0;
  }
  else
  {
    return (intptr_t)(locs[selected]);
  }
}

// Find the first card exiled by player/card starting at ret_leg/ret_idx.

int* exiledby_find_any(int player, int card, int* ret_leg, int* ret_idx)
{
  int leg = ret_leg ? *ret_leg : 0, idx = ret_idx ? *ret_idx : 0, *loc = NULL;
  card_instance_t* instance;

  int p = player;
  if (player < 0)
	player += 2;

  for (; leg < active_cards_count[player]; ++leg)
	{
	  instance = get_card_instance(player, leg);
	  if (exiledby_legacymatches(instance, p, card))
		for (; idx < 18; ++idx)
		  {
			if (instance->targets[idx].player != -1)
			  {
				loc = &instance->targets[idx].player;
				goto break_outer;
			  }
			if (instance->targets[idx].card != -1)
			  {
				loc = &instance->targets[idx].card;
				goto break_outer;
			  }
		  }
	  idx = 0;
	}
 break_outer:

  if (!loc)	// i.e., not found
	leg = idx = 0;

  // Remember last-searched position, so we start here again in subsequent searches this activation
  if (ret_leg)
	*ret_leg = leg;

  if (ret_idx)
	*ret_idx = idx;

  return loc;
}

// Find the first card exiled by player/card starting at ret_leg/ret_idx with stored value needle.

int* exiledby_find(int player, int card, int needle, int* ret_leg, int* ret_idx)
{
  int leg = ret_leg ? *ret_leg : 0, idx = ret_idx ? *ret_idx : 0, *loc = NULL;
  card_instance_t* instance;

  int p = player;
  if (player < 0)
	player += 2;

  for (; leg < active_cards_count[player]; ++leg)
	{
	  instance = get_card_instance(player, leg);
	  if (exiledby_legacymatches(instance, p, card))
		for (; idx < 18; ++idx)
		  {
			if (instance->targets[idx].player == needle)
			  {
				loc = &instance->targets[idx].player;
				goto break_outer;
			  }
			if (instance->targets[idx].card == needle)
			  {
				loc = &instance->targets[idx].card;
				goto break_outer;
			  }
		  }
	  idx = 0;
	}
 break_outer:

  if (!loc)	// i.e., not found
	leg = idx = 0;

  // Remember last-searched position, so we start here again in subsequent searches this activation
  if (ret_leg)
	*ret_leg = leg;

  if (ret_idx)
	*ret_idx = idx;

  return loc;
}

// Returns total number of cards exiled by player/card owned by owned_by.

int exiledby_count(int player, int card, int owned_by)
{
  int leg = 0;
  int idx = 0;
  int count = 0;
  int* loc;

  int p = player;
  if (player < 0)
	player += 2;

  while ((loc = exiledby_find_any(p, card, &leg, &idx)))
	{
	  card_instance_t* instance = get_card_instance(player, leg);
	  if (owned_by == 0 ? !(*loc & 0x80000000)
		  : owned_by == 1 ? (*loc & 0x80000000)
		  : 1)
		++count;

	  if (loc == &instance->targets[idx].player
		  && instance->targets[idx].card != -1)
		{
		  loc = &instance->targets[idx].card;
		  if (owned_by == 0 ? !(*loc & 0x80000000)
			  : owned_by == 1 ? (*loc & 0x80000000)
			  : 1)
			++count;
		}

	  ++idx;
	}
  return count;
}

/* ManalinkEh 2022: 0x26d6d50; original functions/exiledby.c:309. */
int exiledby_count_cmcs(int player, int card, int owned_by)
{
  target_t *current_exile_record;
  int *cmc_cursor_or_exile_entry;
  card_instance_t *instance;
  unsigned char owner_bits_or_match;
  int result;
  int legacy_player;
  int leg;
  int idx;
  int cmcs[21];

  leg = 0;
  memset(cmcs, 0, sizeof(cmcs));
  result = 0;
  idx = 0;
  legacy_player = player;
  if (player < 0)
  {
    legacy_player = player + 2;
  }
  do
  {
    cmc_cursor_or_exile_entry = exiledby_find_any(player, card, &leg, &idx);
    if (cmc_cursor_or_exile_entry == NULL)
    {
      cmc_cursor_or_exile_entry = cmcs;
      result = 0;
      do
      {
        if (*cmc_cursor_or_exile_entry == 1)
        {
          result = result + 1;
        }
        cmc_cursor_or_exile_entry = cmc_cursor_or_exile_entry + 1;
      } while (cmcs + 0x14 != cmc_cursor_or_exile_entry);
      return result;
    }
    instance = get_card_instance(legacy_player, leg);
    owner_bits_or_match = (unsigned char)((unsigned int)*cmc_cursor_or_exile_entry >> 0x18);
    bool count_first_entry = true;
    if (owned_by == 0)
    {
      owner_bits_or_match = owner_bits_or_match ^ 0xff;
      if ((char)owner_bits_or_match >= '\0')
        count_first_entry = false;
    }
    else if (owned_by == 1 && (char)owner_bits_or_match >= '\0')
      count_first_entry = false;
    if (count_first_entry)
    {
      result = get_cmc_by_id((unsigned int)cards_data[*cmc_cursor_or_exile_entry].id);
      cmcs[result] = 1;
    }
    current_exile_record = instance->targets + idx;
    if (((target_t *)cmc_cursor_or_exile_entry == current_exile_record) &&
        (result = instance->targets[idx].card, result != -1))
    {
      owner_bits_or_match = (unsigned char)((unsigned int)result >> 0x18);
      if (owned_by == 0)
      {
        owner_bits_or_match = (owner_bits_or_match ^ 0xff) >> 7;
      }
      else
      {
        owner_bits_or_match = owned_by != 1 | owner_bits_or_match >> 7;
      }
      if (owner_bits_or_match != 0)
      {
        result = get_cmc_by_id((unsigned int)cards_data[result].id);
        cmcs[result] = 1;
      }
    }
    idx = idx + 1;
  } while (true);
}

// Remembers that a card with iid owned by t_player was exiled.  This doesn't actually exiled the card; if the exiled card is to be face-up, the caller should do so.

void exiledby_remember(int player, int card, int t_player, int iid, int *ret_leg, int *ret_idx)
{
  if (iid == -1)
    return;

  ASSERT(player >= 0 && "exiledby_remember() cannot be called on detached effects");

  int csvid = cards_data[iid].id;
  int csvids[2];
  int meld = csvid == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES || csvid == CARD_ID_CHITTERING_HOST ||
             csvid == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP;
  if (meld)
  {
    unmeld(csvid, &csvids[0], &csvids[1]);
    iid = get_internal_card_id_from_csv_id(csvids[0]);
  }
  for (;;)
  {
    int *loc = exiledby_find(player, card, -1, ret_leg, ret_idx);

    if (!loc)
    {
      int leg = create_targetted_legacy_effect(player, card, exiledby_legacy, player, card);
      card_instance_t *instance = get_card_instance(player, leg);
      instance->targets[0].player = instance->targets[0].card = -1;
      instance->token_status |= STATUS_INVISIBLE_FX;
      if (ret_leg)
        *ret_leg = leg;
      if (ret_idx)
        *ret_idx = 0;
      loc = &instance->targets[0].player;
    }

    ASSERT(!(iid & 0x80000000));
    if (t_player == 1)
    {
      iid |= 0x80000000;
      ASSERT(iid != -1);
    }
    *loc = iid;

    if (!meld)
      break;
    meld = 0;
    iid = get_internal_card_id_from_csv_id(csvids[1]);
  }
}

/* Normally, these legacies are attached to the card that exiled them.  This is problematic when that permanent leaves the battlefield: all the attached effects
 * will be obliterated.  This function turns the effect cards into freestanding legacies, determines a unique id, and stores that id in their targets[18].card
 * so they can be distinguished from each other.  Returns the id.  Other functions in this package - except for exiledby_remember() - can then be called with
 * player=(player - 2), card=id to examine the legacies.  Caller will have to call exiledby_destroy_detached() later with that id, or else the legacies stay in place
 * permanently. */
int exiledby_detach(int player, int card)
{
  card_instance_t* instance;
  int leg, detach_id = 0;

  // First, find an unused detach_id.
  for (leg = 0; leg < active_cards_count[player]; ++leg)
	{
	  instance = get_card_instance(player, leg);
	  if (instance->internal_card_id == LEGACY_EFFECT_CUSTOM
		  && instance->info_slot == (intptr_t)exiledby_legacy
		  && instance->targets[18].card > detach_id)
		detach_id = instance->targets[18].card;
	}

  ++detach_id;

  // Now detach all exiledby legacies attached to player/card and set their detach_id.
  for (leg = 0; leg < active_cards_count[player]; ++leg)
	{
	  instance = get_card_instance(player, leg);
	  if (exiledby_legacymatches(instance, player, card))
		{
		  instance->damage_target_player = instance->damage_target_card = -1;
		  instance->targets[18].card = detach_id;
		}
	}

  return detach_id;
}

// Destroys all detached exiledby legacies set to detach_id.

void exiledby_destroy_detached(int player, int detach_id)
{
  int leg;
  for (leg = 0; leg < active_cards_count[player]; ++leg)
	if (exiledby_legacymatches(get_card_instance(player, leg), player - 2, detach_id))
	  kill_card(player, leg, KILL_EXILE);
}

void exile_card_and_remember_it_on_exiledby(int player, int card, int t_player, int t_card){
	int owner = get_owner(t_player, t_card);
	int iid = get_card_instance(t_player, t_card)->original_internal_card_id;
	kill_card(t_player, t_card, KILL_EXILE);
	exiledby_remember(player, card, owner, iid, NULL, NULL);
}

// empty effect, used only for data storage and comparison purposes

static int store_counters_legacy(int player, int card, event_t event)
{
  /* Targets:
   * [17].card: stores a "card" value used to identify a specific legacy of this type */
  return 0;
}

static void return_auras_and_counters_to_play(int player, int card)
{
  int target_player;
  unsigned int return_mode;
  card_instance_t *instance;
  int result;
  card_instance_t *aura_or_counter_legacy;
  target_t *aura_record;
  target_t *next_aura_record;
  int32_t returned_card;
  int8_t attachment_player;

  instance = get_card_instance(player, card);
  target_player = instance->targets[0].player;
  return_mode = instance->targets[0x12].player;
  result = check_rfg(target_player, (unsigned int)cards_data[instance->targets[0].card].id);
  returned_card = -1;
  if (result != 0)
  {
    returned_card = add_card_to_hand(target_player, instance->targets[0].card);
    remove_card_from_rfg(target_player, (unsigned int)cards_data[instance->targets[0].card].id);
    if ((int)return_mode < 1)
    {
      put_into_play(target_player, returned_card);
      return;
    }
    if ((return_mode & 0x10) != 0)
    {
      add_state(target_player, returned_card, 0x10);
    }
    put_into_play(target_player, returned_card);
    if (((-1 < returned_card) && ((return_mode & 0x2a) != 0)) && (0 < active_cards_count[player]))
    {
      result = 0;
      do
      {
        aura_or_counter_legacy = in_play(player, result);
        if (((aura_or_counter_legacy != NULL) &&
             (aura_or_counter_legacy->internal_card_id == LEGACY_EFFECT_CUSTOM)) &&
            (((CardFunction)aura_or_counter_legacy->info_slot == store_counters_legacy &&
              (aura_or_counter_legacy->targets[0x11].card == instance->targets[0x12].card))))
        {
          copy_counters(player, card, target_player, returned_card, player, result, 1);
          kill_card(player, result, KILL_EXILE);
          break;
        }
        result = result + 1;
      } while (result < active_cards_count[player]);
    }
  }
  if ((return_mode & 0x25) != 0)
  {
    aura_record = instance->targets + 1;
    do
    {
      while (
          (aura_record->player != -1 &&
           (result = check_rfg(aura_record->player, (unsigned int)cards_data[aura_record->card].id),
            result != 0)))
      {
        next_aura_record = aura_record + 1;
        result = add_card_to_hand(aura_record->player, aura_record->card);
        remove_card_from_rfg(aura_record->player, (unsigned int)cards_data[aura_record->card].id);
        aura_or_counter_legacy = get_card_instance(aura_record->player, result);
        attachment_player = (int8_t)target_player;
        aura_or_counter_legacy->damage_target_player = attachment_player;
        aura_or_counter_legacy->damage_target_card = returned_card;
        set_special_flags(aura_record->player, result, 0x20000000);
        put_into_play(aura_record->player, result);
        aura_record = next_aura_record;
        if (next_aura_record == instance->targets + 0x12)
        {
          return;
        }
      }
      aura_record = aura_record + 1;
    } while (aura_record != instance->targets + 0x12);
  }
  return;
}

static int return_auras_counters_on_eot(int player, int card, event_t event){
	if( eot_trigger(player, card, event) ){
		return_auras_and_counters_to_play(player, card);
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

static int return_auras_counters_if_source_leaves_play(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	if (event == EVENT_SET_LEGACY_EFFECT_NAME && instance->targets[0].card > -1 ){
		scnprintf(set_legacy_effect_name_addr, 51, "%s", cards_ptr[cards_data[instance->targets[0].card].id]->name);
	}
	if( trigger_condition == TRIGGER_LEAVE_PLAY ){
		if( affect_me( player, card) && trigger_cause == instance->damage_target_card && trigger_cause_controller == instance->damage_target_player
			&& reason_for_trigger_controller == player )
		{
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					return_auras_and_counters_to_play(player, card);
					kill_card(player, card, KILL_EXILE);
			}
		}
	}
	return 0;
}

static int tawnoss_coffin_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_SET_LEGACY_EFFECT_NAME && instance->targets[0].card > -1)
  {
    scnprintf(set_legacy_effect_name_addr, 51, "%s",
              cards_ptr[cards_data[instance->targets[0].card].id]->name);
  }
  if (instance->damage_target_player > -1)
  {
    player_bits[player] |= PB_SEND_EVENT_UNTAP_CARD_TO_ALL;
    leaves_play(instance->damage_target_player, instance->damage_target_card, event, 0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY ||
        (event == EVENT_UNTAP_CARD &&
         affect_me(instance->damage_target_player, instance->damage_target_card)))
    {
      return_auras_and_counters_to_play(player, card);
      kill_card(player, card, KILL_EXILE);
    }
  }
  return 0;
}

static void store_permanent_on_legacy(card_instance_t* leg, int to_copy_player, int to_copy_card, int targ_min, int targ_max){
	targ_max = targ_max > 19 ? 19 : targ_max;
	int k;
	for(k=targ_min; k<targ_max; k++){
		if( leg->targets[k].player == -1 ){
			leg->targets[k].player = get_owner(to_copy_player, to_copy_card);
			leg->targets[k].card = get_original_internal_card_id(to_copy_player, to_copy_card);
			break;
		}
	}
}

int exile_permanent_and_auras_attached(int player, int card, int t_player, int t_card,
                                       unsigned int mode)
{
  unsigned int legacy_status;
  int scan_player_or_check;
  int aura_check_or_next_card;
  card_instance_t *aura_or_legacy;
  int aura_owner_or_iid;
  int aura_card_or_legacy;
  CardFunction func_ptr;
  card_instance_t *return_legacy;
  int return_legacy_card;

  scan_player_or_check = is_token(t_player, t_card);
  if (scan_player_or_check == 0)
  {
    scan_player_or_check = get_original_internal_card_id(t_player, t_card);
    scan_player_or_check = is_what(-1, scan_player_or_check, 0x47);
    if (scan_player_or_check != 0)
    {
      scan_player_or_check = 0;
      if ((mode & 1) != 0)
      {
        aura_card_or_legacy = create_legacy_effect(player, card, return_auras_counters_on_eot);
        aura_check_or_next_card = get_id(t_player, t_card);
        return_legacy_card = set_legacy_image(player, aura_check_or_next_card, aura_card_or_legacy);
      }
      else if ((mode & (4 | 0x20)) != 0)
      {
        func_ptr =
            (mode & 4) != 0 ? return_auras_counters_if_source_leaves_play : tawnoss_coffin_legacy;
        return_legacy_card = create_targetted_legacy_effect(player, card, func_ptr, player, card);
        aura_or_legacy = get_card_instance(player, return_legacy_card);
        aura_or_legacy->eot_toughness = 0x100;
      }
      else
        return_legacy_card = -1;
      return_legacy = NULL;
      if (return_legacy_card != -1)
      {
        return_legacy = get_card_instance(player, return_legacy_card);
        return_legacy->targets[0x12].player = mode;
        return_legacy->targets[0x12].card = return_legacy_card;
      }
      do
      {
        aura_card_or_legacy = active_cards_count[scan_player_or_check] + -1;
        if (-1 < active_cards_count[scan_player_or_check] + -1)
        {
          do
          {
            aura_or_legacy = in_play(scan_player_or_check, aura_card_or_legacy);
            if (aura_or_legacy != NULL &&
                (aura_check_or_next_card = is_what(scan_player_or_check, aura_card_or_legacy, 4),
                 aura_check_or_next_card != 0) &&
                (aura_check_or_next_card =
                     has_subtype(scan_player_or_check, aura_card_or_legacy, SUBTYPE_AURA),
                 aura_check_or_next_card != 0) &&
                aura_or_legacy->damage_target_player == t_player &&
                aura_or_legacy->damage_target_card == t_card)
            {
              if ((return_legacy_card != -1) &&
                  (aura_check_or_next_card = is_token(scan_player_or_check, aura_card_or_legacy),
                   aura_check_or_next_card == 0))
              {
                aura_check_or_next_card = 1;
                do
                {
                  if (return_legacy->targets[aura_check_or_next_card].player == -1)
                  {
                    aura_owner_or_iid = get_owner(scan_player_or_check, aura_card_or_legacy);
                    return_legacy->targets[aura_check_or_next_card].player = aura_owner_or_iid;
                    aura_owner_or_iid =
                        get_original_internal_card_id(scan_player_or_check, aura_card_or_legacy);
                    return_legacy->targets[aura_check_or_next_card].card = aura_owner_or_iid;
                    break;
                  }
                  aura_check_or_next_card = aura_check_or_next_card + 1;
                } while (aura_check_or_next_card != 0x12);
              }
              aura_check_or_next_card = aura_card_or_legacy + -1;
              kill_card(scan_player_or_check, aura_card_or_legacy, KILL_EXILE);
              aura_card_or_legacy = aura_check_or_next_card;
            }
            else
              aura_card_or_legacy = aura_card_or_legacy - 1;
          } while (aura_card_or_legacy != -1);
        }
        scan_player_or_check = scan_player_or_check + 1;
      } while (scan_player_or_check != 2);
      if ((mode & 0x2a) != 0)
      {
        scan_player_or_check =
            create_legacy_effect(player, card, (CardFunction)&store_counters_legacy);
        aura_or_legacy = get_card_instance(player, scan_player_or_check);
        legacy_status = aura_or_legacy->token_status;
        aura_or_legacy->targets[0x11].card = return_legacy_card;
        aura_or_legacy->token_status = legacy_status | 0x10000;
        copy_counters(player, card, player, scan_player_or_check, t_player, t_card, 1);
      }
      if (return_legacy == NULL)
      {
        return_legacy_card = -1;
      }
      else
      {
        scan_player_or_check = get_owner(t_player, t_card);
        return_legacy->targets[0].player = scan_player_or_check;
        aura_or_legacy = get_card_instance(t_player, t_card);
        return_legacy->targets[0].card = aura_or_legacy->original_internal_card_id;
      }
      kill_card(t_player, t_card, KILL_EXILE);
      return return_legacy_card;
    }
  }
  manipulate_auras_enchanting_target(player, card, t_player, t_card, NULL, ACT_KILL_REMOVE);
  return_legacy_card = -1;
  kill_card(t_player, t_card, KILL_EXILE);
  return return_legacy_card;
}

void get_back_cards_stored_in_exiledby(int player, int exby_player, int exby_card, get_back_cards_stored_in_exiledby_mode flags){
	int leg;
	int idx;
	int* loc;
	while( (loc = exiledby_find_any(exby_player, exby_card, &leg, &idx)) != NULL ){
			int owner = (*loc & 0x80000000) ? 1 : 0;
			int iid = *loc & ~0x80000000;
			*loc = -1;
			int who_gets_the_card = (flags & GBC_PLAYER_GET_ALL_CARDS) ? player : owner;
			int card_added = add_card_to_hand(who_gets_the_card, iid);
			if( owner != player ){
				if( player == AI ){
					remove_state(who_gets_the_card, card_added, STATE_OWNED_BY_OPPONENT);
				}
				else{
					add_state(who_gets_the_card, card_added, STATE_OWNED_BY_OPPONENT);
				}
			}
			if( flags & GBC_PUT_PERMANENTS_INTO_BF ){
				put_into_play(who_gets_the_card, card_added);
			}
	}
}
