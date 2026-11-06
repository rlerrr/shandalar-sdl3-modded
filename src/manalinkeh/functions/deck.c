// -*- c-basic-offset:2 -*-
// Creation and destruction of cards; (mostly non-interactive) manipulation of libraries/graveyards/exile; reanimation.

#include "manalink.h"

/* Canonical recovered helper declarations. */
card_data_t *get_card_data(int player, int card);
int counterspell_resolve_unless_pay_x(int player, int card, target_definition_t *td,
                                           int target_num, int x);
int seek_grave_for_id_to_reanimate(int player, int card, int t_player, int id,
                                        reanimate_mode_t mode);
int new_select_target_from_grave(int player, int card, int t_player, select_grave_t mode,
                                      int ai_mode, test_definition_t *this_test,
                                      int ret_location);
int select_target_from_grave_source(int player, int card, int t_player, select_grave_t mode,
                                         int ai_mode, test_definition_t *this_test,
                                         int ret_location);
int select_target_from_either_grave(int player, int card, select_grave_t mode,
                                         int ai_mode_own_graveyard, int ai_mode_opponent_graveyard,
                                         test_definition_t *test, int ret_location_graveyard,
                                         int ret_location_card);

/* October 2022 extension storage; fixed banks and original template. */
card_ext_t card_ext_buffer1;
card_ext_t card_ext[2][151];
card_ext_t card_ext_buffer2;
card_ext_t disp_card_ext[2][151];
card_ext_t card_ext_buffer3;
static const card_ext_t card_ext_template = {
    .sp_keywords = 0,
    .sp_keywords2 = 0,
    .subtype = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    .inst_info_data = {{.id = 0, .data2 = 0, .data = 0},
                       {.id = 0, .data2 = 0, .data = 0},
                       {.id = 0, .data2 = 0, .data = 0},
                       {.id = 0, .data2 = 0, .data = 0},
                       {.id = 0, .data2 = 0, .data = 0},
                       {.id = 0, .data2 = 0, .data = 0}},
    .mana_paid_to_cast = {0, 0, 0, 0, 0, 0, 0, 0},
    .phyrexian_mana_spent_to_cast = 0,
    .snow_mana_spent_to_cast = 0,
    .any_snow_mana_of_color_spent_to_cast_raw = 0,
    .treasure_mana_spent_to_cast = 0,
    .inst_stored_iids = {{.generating_id = -1, .stored_iid = -1},
                         {.generating_id = -1, .stored_iid = -1},
                         {.generating_id = -1, .stored_iid = -1},
                         {.generating_id = -1, .stored_iid = -1},
                         {.generating_id = -1, .stored_iid = -1},
                         {.generating_id = -1, .stored_iid = -1},
                         {.generating_id = -1, .stored_iid = -1},
                         {.generating_id = -1, .stored_iid = -1},
                         {.generating_id = -1, .stored_iid = -1},
                         {.generating_id = -1, .stored_iid = -1},
                         {.generating_id = -1, .stored_iid = -1},
                         {.generating_id = -1, .stored_iid = -1}},
    .cardfromwhere = 0,
    .turn_etb = 0,
    .became_player = -1,
    .became_card = 255,
    .base_power = 0,
    .base_toughness = 0,
    .pow_mod_unblocked = 0,
    .pow_before_attack = 0,
    .raw_power = 0,
    .raw_toughness = 0,
    .soulbond = 255,
    .times_regenerated = 0,
    .times_conspire_paid = 0,
    .times_replicate_paid = 0,
    .invis_counters = {0, 0, 0, 0, 0, 0, 0, 0},
    .invis_counter_types = {0, 0, 0, 0, 0, 0, 0, 0},
    .copy_iid_ = -1,
    .change_name_csvid_ = -1,
    .special_flags = 0,
    .special_flags2 = 0,
    .special_flags3 = 0};

/* Each element {p,c} in graveyard_source[p][n] corresponds to the card at get_grave(p)[n] (itself storing the card's original_internal_card_id).
 * As in deck_ptr[][], rfg_ptr[][], and get_grave()[], an element of -1 means there's no card in that position.  Otherwise, it's a 32-bit int, containing:
 *
 * Bit 0-7:  The card index, if the card went into the graveyard directly from the battlefield, a hand, or the stack (i.e, it had a meaningful {player,card}).
 *           Otherwise, will be 255.  Most easily accessible as BYTE0(graveyard_source[p][n]).
 * Bit 8:    The card's controller as it went into the graveyard, as above.  Always 0 if there was no in-play/hand/stack {player,card}.
 *           Most easily accessible as BYTE1(graveyard_source[p][n]) & 1.
 * Bit 9-23: A unique id generated as each card is put into the graveyard.  15 bits is wide enough to store 32768 ids, which should be plenty for even the most
 *           pathological recursion deck.  (Id 1 is the first card put into a graveyard that game, id 2 the second, etc, but this shouldn't be relied on.)
 * Bit 24-31:turn_count when the card was put in the graveyard.  The first turn of the game is turn 1, and that's incremented at the start of each player's
 *           turn.  It doesn't distinguish between *whose* turn it is, and it'll probably count skipped ones, too; but they shouldn't repeat.  On the other
 *           hand, we only store 8 bits of it here, so in a game lasting 256 turns or we won't be able to tell the difference from graveyard_source.  Oh well.
 *           Most easily accessible as BYTE3(graveyard_source[p][n]).
 *
 * So if player 0 went first, it's player 1's second turn, player 1 has a Forest stolen from player 0 in play at {1,14}, player 0 has two cards in his
 * graveyard, and the Forest is destroyed, then:
 * get_grave(0)[2] will be 2 (Forest's internal_card_id),
 * graveyard_source[0][2] & 0x100 will be 1 (since it was at {1,14}),
 * graveyard_source[0][2] & 0xFF will be 14 (again, since it was at {1,14}),
 * graveyard_source[0][2] & 0x00FFFE00 will be its unique id (3, unless more cards than those first two have been put in a graveyard this game), and
 * graveyard_source[0][2] & 0xFF000000 will be 4 (player 0's first turn was 1, player 1's first was 2, player 0's second was 3, and player 1's second is 4).
 * So it'll be 0x0400070E: The 4 is the turn number; the 7 is (id<<1) | (player), and the E is hex 14.
 *
 * But generally the only thing you're interested in is that once a card goes into a graveyard, then the generated element is unique and guaranteed not to
 * change until the card leaves the graveyard.  Its position in the graveyard_source[][] array *will* change if a card below it is removed from the graveyard,
 * just as its position in get_grave()[] does; but it can still be found by searching for its graveyard_source value.
 *
 * So if player 0 in the above scenario casts Animate Dead on the bottom card of his graveyard (which is at get_grave(0)[0]), the Forest will have moved to
 * get_grave(0)[1] and graveyard_source[0][1]; but graveyard_source[0][1] will still be 0x4000070E, and anything that's looking for that specific Forest will be
 * able to find it by recording the graveyard_source[0][2] at targetting and then searching graveyard_source[][] for that value at resolution. */
int graveyard_source[2][500] = {{0}};
int next_graveyard_source_id = 1;
#define GRAVEYARD_SOURCE_VALUE(turn, id, player, card)	((((turn) & 0xFF) << 24) | (((id) << 9) & 0x00FFFE00)	\
														 | (((player) & 1) << 8) | ((card) & 0xFF))
#define GENERATE_GRAVEYARD_SOURCE_VALUE(player, card)	GRAVEYARD_SOURCE_VALUE((turn_count), next_graveyard_source_id++, (player), (card))

int calc_initial_attack_rating_by_iid(int player, int iid)
{
  // As per calc_initial_attack_rating, but takes an iid instead of a {player,card}.

  /* card_instance_t::attack_rating seems to get recalculated in various AI, but they inline this function with various changes instead of calling it, so no
   * point modernizing it.  The calculations here match the ones in ai_opinion_of_gamestate() at 0x499160, so this can be called to figure out the ai_modifier
   * equivalent of a creature not in play (there's further calculation added in ai_opinion_of_gamestate_continued() which is difficult to localize to individual
   * cards, plus the part omitted from STATE_IN_PLAY in calc_initial_attack_rating() below). */

  if (iid < 0)
	return 0;

  keyword_t abils = cards_data[iid].static_ability;

  int pow = get_base_power_iid(player, iid);
  if ((abils & KEYWORD_DEFENDER) || pow < 0)
	pow = 0;

  int tgh = get_base_toughness_iid(player, iid);
  int rating = (tgh + 4) * (2 * pow + 3) / 2;

  if (abils & KEYWORD_TRAMPLE)
	rating = 3 * rating / 2;

  if (abils & KEYWORD_FIRST_STRIKE)
	rating = 3 * rating / 2;

  if (cards_data[iid].extra_ability & (EA_ACT_INTERRUPT|EA_ACT_ABILITY))
	rating = 3 * rating / 2;

  if (abils & KEYWORD_BANDING)
	rating = rating * (tgh + 1) / 2;

  if (abils & KEYWORD_REGENERATION)
	rating = 3 * rating / 2;

  if (cards_data[iid].id == CARD_ID_BALL_LIGHTNING)	// original card_ball_lightning()
	rating = 1;

  rating *= g_deck_rating_multipliers[player];	// Seems to always be 12

  return rating / 8;
}

static int calc_initial_attack_rating(int player, int card)
{
  //0x479880 - called only from create_card_instance()

  card_instance_t* instance = get_card_instance(player, card);
  int iid = instance->internal_card_id;
  if (iid < 0 || (instance->state & STATE_OUBLIETTED))
	return 0;

  keyword_t abils = cards_data[iid].static_ability;

  int pow = get_base_power_iid(player, iid);
  if ((abils & KEYWORD_DEFENDER) || pow < 0)
	pow = 0;

  int tgh = get_base_toughness_iid(player, iid);
  int rating = (tgh + 4) * (2 * pow + 3) / 2;

  if (instance->state & STATE_TAPPED && current_turn == player)	// Seems impossible.
	--rating;

  if (abils & KEYWORD_TRAMPLE)
	rating = 3 * rating / 2;

  if (abils & KEYWORD_FIRST_STRIKE)
	rating = 3 * rating / 2;

  if (cards_data[iid].extra_ability & (EA_ACT_INTERRUPT|EA_ACT_ABILITY))
	rating = 3 * rating / 2;

  if (abils & KEYWORD_BANDING)
	rating = rating * (tgh + 1) / 2;

  if (abils & KEYWORD_REGENERATION)
	rating = 3 * rating / 2;

  if (instance->state & STATE_IN_PLAY)
	{
#if 0
	  if (g_duel_state_0093f4b0 && player != AI && current_turn == AI)
		{
		  // compare against human's creatures - omit for now
		}
#endif
	  rating *= 3;
	}
  else if (cards_data[iid].id == CARD_ID_BALL_LIGHTNING)	// original card_ball_lightning()
	rating = 1;

  rating *= g_deck_rating_multipliers[player];	// Seems to always be 12

  return rating / 8;
}

static const card_instance_t card_instance_template =
{
  .damage_target_card = -1,
  .damage_source_player = -1,
  .blocking = 255,
  .regen_status = KEYWORD_RECALC_ABILITIES,
  .original_internal_card_id = -1,
  .backup_internal_card_id = (uint16_t)-1,
  .damage_source_card = -1,
  .damage_target_player = -1,
  .special_counter_type = 0xff,
  .unk52 = 0xff,
  .unk53 = 0xff,
  .timestamp = (uint32_t)-1,
  .internal_card_id = -1,
  .targets = {{-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1},
              {-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1},
              {-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1},
              {-1,-1},{-1,-1},{-1,-1},{-1,-1}},
  .parent_controller = -1,
  .parent_card = -1
};

void initialize_card_extensions(void)
{
  for (int player = 0; player < 2; ++player)
    for (int card = 0; card < 151; ++card) {
      card_ext[player][card] = card_ext_template;
      disp_card_ext[player][card] = card_ext_template;
      raw_card_instances[player][card].ext = &card_ext[player][card];
      raw_displayed_card_instances[player][card].ext = &disp_card_ext[player][card];
    }
}

void create_card_instance(int player, unsigned int iid, int card)
{
  // Original at 0x4796A0

  ASSERT(iid < 0x8000);	// Is there a better constraint for the upper bound?

  card_instance_t* instance = get_card_instance(player, card);
  memcpy(instance, &card_instance_template, sizeof(card_instance_t));
  /* Recovered 2022 extension fragment: create_card_instance @ 0x26b2b50. */
  instance->ext = &card_ext[player][card];
  memcpy(instance->ext, &card_ext_template, sizeof(*instance->ext));
  /* End recovered 2022 extension fragment: create_card_instance. */

  instance->backup_internal_card_id = instance->original_internal_card_id = instance->internal_card_id = iid;

  if (player)
	instance->state = STATE_OWNED_BY_OPPONENT;

  card_data_t* cd = cards_at_7c7000[iid];
  instance->power = cd->power;
  instance->toughness = cd->toughness;
  instance->mana_color = instance->card_color = instance->color = cd->color;
  instance->attack_rating = calc_initial_attack_rating(player, card);

  if (cd->extra_ability & EA_MANA_SOURCE)
	{
	  if (cd->type == TYPE_LAND || cd->type == TYPE_ARTIFACT)
		instance->color = COLOR_TEST_COLORLESS;

	  // Begin additions
	  int csvid = cd->id;
	  int mc = get_color_of_mana_produced_by_id(csvid, -1, player);
	  if (mc != -1)
		instance->mana_color = mc;
	  else
		instance->mana_color = cards_ptr[csvid]->mana_source_colors;
	  // End additions

#if 0
	  switch (cd->id)
		{
		  case CARD_ID_APPRENTICE_WIZARD:
		  case CARD_ID_ENERGY_TAP:
			instance->mana_color = COLOR_TEST_COLORLESS;
			break;

		  case CARD_ID_BIRDS_OF_PARADISE:
		  case CARD_ID_CITY_OF_BRASS:
			instance->mana_color = COLOR_TEST_ANY_COLORED;
			break;

		  case CARD_ID_ELVES_OF_DEEP_SHADOW:
			instance->mana_color = COLOR_TEST_BLACK;
			break;

		  case CARD_ID_COAL_GOLEM:
			instance->mana_color = COLOR_TEST_RED;
			break;

		  case CARD_ID_FIRE_SPRITES:
			instance->mana_color = COLOR_TEST_RED;
			break;

		  default:
			break;
		}
#endif
	}

  TENTATIVE_set_timestamps(player, card);	// TENTATIVE_set_timestamps()

#define TRIGGER_DEPTH	g_nested_trigger_depth
  if (TRIGGER_DEPTH > 0)
	instance->state |= STATE_PROCESSING;
}

int add_card_to_hand_exe(int player, int iid);
int add_card_to_hand(int player, int iid)
{
  ++hand_count[player];
  int card_added = add_card_to_hand_exe(player, iid);
  if (card_added >= 0)
    reset_subtypes_from_instance(get_card_instance(player, card_added), iid);
  return card_added;
}

void show_backtrace(const char*, const char*);
SetVoidptr already_displayed = NULL;

extern card_instance_t raw_card_instances[2][151];
card_instance_t* get_card_instance(int player, int card)
{
  // 0x401a80 (not replaced)
  if ((card < 0 || card >= 150 || player < 0 || player > 1)
	  && SetVoidptr_insert(&already_displayed, __builtin_return_address(0)))
	{
	  char buf[100];
	  sprintf(buf, "get_card_instance(%d, %d)", player, card);
	  show_backtrace("bad parameters", buf);
	}

  return &raw_card_instances[player][card];
}

extern card_instance_t raw_displayed_card_instances[2][151];
card_instance_t* get_displayed_card_instance(int player, int card)
{
  // 0x401ab0 (not replaced)
  if ((card < 0 || card >= 150 || player < 0 || player > 1)
	  && SetVoidptr_insert(&already_displayed, __builtin_return_address(0)))
	{
	  char buf[100];
	  sprintf(buf, "get_displayed_card_instance(%d, %d)", player, card);
	  show_backtrace("bad parameters", buf);
	}

  return &raw_displayed_card_instances[player][card];
}

card_instance_t* in_play(int player, int card)
{
  // 0x401a40 (not replaced)
  if ((card < 0 || card >= 150 || player < 0 || player > 1)
	  && SetVoidptr_insert(&already_displayed, __builtin_return_address(0)))
	{
	  char buf[100];
	  sprintf(buf, "in_play(%d, %d)", player, card);
	  show_backtrace("bad parameters", buf);
	}

  card_instance_t* inst = &raw_card_instances[player][card];
  if (inst->internal_card_id >= 0
	  && (inst->state & (STATE_OUBLIETTED|STATE_INVISIBLE|STATE_IN_PLAY)) == STATE_IN_PLAY)
	return inst;
  else
	return NULL;
}

card_instance_t* in_hand(int player, int card){
	card_instance_t* instance = get_card_instance(player, card);
	if( ! ( instance->state & STATE_INVISIBLE ) && ! ( instance->state & STATE_IN_PLAY )  ){
		int id = instance->internal_card_id;
		if( id > -1 ){
			return instance;
		}
	}
	return NULL;
}

static int* csvid_to_iid = NULL;
static const int max_csvid = 32768;

static int get_internal_card_id_from_csv_id_impl(int csvid)
{
  if (csvid < 0 || csvid >= max_csvid - 1)
	return -1;

  if (!csvid_to_iid)
	{
	  csvid_to_iid = (int32_t*)malloc(sizeof(int32_t) * max_csvid);
	  memset(csvid_to_iid, -1, sizeof(int32_t) * max_csvid);

	  int i;
	  for (i = 0; i < g_card_count + 16; ++i)
		{
		  int id = cards_data[i].id ;
		  if (id == -1 || id == 65535)break;
		  if(id >= 0 && id < max_csvid && csvid_to_iid[id] ==-1)
		csvid_to_iid[id] = i;
	}}

  if (csvid_to_iid[csvid] == -1)
	{
	  // Search for it, like the original exe version at 0x479df0 did every call.  Slow!
	  int end = g_card_count + 16;
	  ASSERT(end < max_csvid);
	  int i;
	  for (i = 0; i < end; ++i)
		if (cards_data[i].id == csvid)
		  {
			csvid_to_iid[csvid] = i;
			break;
		  }

	  if (csvid_to_iid[csvid] == -1)
		csvid_to_iid[csvid] = -2;	// So we don't search again
	}

  if (csvid_to_iid[csvid] == -2)
	return -1;
  else
	return csvid_to_iid[csvid];
}

int get_internal_card_id_from_csv_id(int csvid)
{
  if (csvid == -1)
	return -1;
  else if (csvid_to_iid && csvid < max_csvid && csvid_to_iid[csvid] >= 0)
	return csvid_to_iid[csvid];
  else
	return get_internal_card_id_from_csv_id_impl(csvid);
}

card_data_t *get_card_data(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  int iid = instance->internal_card_id;
  if (iid == -1)
    iid = instance->backup_internal_card_id;
  return &cards_data[iid];
}

int get_id(int player, int card){
	card_instance_t *instance= get_card_instance(player, card);
	if( cards_data[instance->original_internal_card_id].id == CARD_ID_SAKASHIMA_THE_IMPOSTOR ){
		return CARD_ID_SAKASHIMA_THE_IMPOSTOR;
	}
	return cards_data[ instance->internal_card_id ].id;
}

// Probably unsafe to just fall back to backup_internal_card_id in get_id(); something might rely on it being -1 for cards not in play.
int get_backup_id(int player, int card)
{
  card_instance_t* instance = get_card_instance(player, card);
  int iid = instance->internal_card_id;
  if (iid == -1)
	iid = instance->backup_internal_card_id;
  return cards_data[iid].id;
}

int get_original_id(int player, int card){
	card_instance_t *instance= get_card_instance(player, card);
	return cards_data[instance->original_internal_card_id].id;
}

int get_original_internal_card_id(int player, int card){
	card_instance_t *instance= get_card_instance(player, card);
	return instance->original_internal_card_id;
}

/***************
* Obliteration *
***************/

void obliterate_card(int player, int card)
{
  card_instance_t* instance = get_card_instance(player, card);
  int owner = get_owner(player, card);
  int inserted = !(instance->token_status & (STATUS_TOKEN | STATUS_OBLITERATED));
  if (in_play(player, card) || in_hand(player, card))
    {
      int melded = get_id(player, card) == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES
                   || get_id(player, card) == CARD_ID_CHITTERING_HOST
                   || get_id(player, card) == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP;
      put_on_top_of_deck(player, card);
      if (melded)
        remove_card_from_deck(owner, 0);
      if (inserted) remove_card_from_deck(owner, 0);
    }
  else
    {
      instance->internal_card_id = -1;
      instance->counter_toughness = 0;
      instance->damage_on_card = 0;
      instance->counter_power = 0;
      instance->blocking = 255;
    }
}

void obliterate_card_and_recycle(int player, int card)
{
  card_instance_t* instance = get_card_instance(player, card);
  int owner = get_owner(player, card);
  int inserted = !(instance->token_status & (STATUS_TOKEN | STATUS_OBLITERATED));
  if (in_play(player, card) || in_hand(player, card))
    {
      int melded = get_id(player, card) == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES
                   || get_id(player, card) == CARD_ID_CHITTERING_HOST
                   || get_id(player, card) == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP;
      put_on_top_of_deck(player, card);
      if (melded)
        remove_card_from_deck(owner, 0);
      if (inserted) remove_card_from_deck(owner, 0);
    }
  else
    {
      instance->internal_card_id = -1;
      instance->original_internal_card_id = -1;
      instance->counter_toughness = 0;
      instance->damage_on_card = 0;
      instance->counter_power = 0;
      instance->blocking = 255;
    }
}

// Remove the card at position pos in deck without putting it anywhere else
static void obliterate_card_from_deck_impl(int* deck, int pos){
	if (deck[pos] == -1){
		return;
	}

	int i;
	for (i = pos; i < 499 && deck[i] != -1; ++i){
		deck[i] = deck[i + 1];
	}
	deck[499] = -1;
}

// Remove the top card of player's library, without putting it anywhere else
int obliterate_top_card_of_deck(int player){
	 remove_card_from_deck(player, 0);
}

// Remove the top number cards of player's library, without putting them anywhere else
void obliterate_top_n_cards_of_deck(int player, int number){
	if (number > 0){
		int* deck = deck_ptr[player];
		int i, end = 500 - number;
		for (i = 0; i < end && deck[i] != -1; ++i){
			deck[i] = deck[i + number];
		}
		for (i = end; i < 500; ++i){
			deck[i] = -1;
		}
	}
}

void obliterate_top_card_of_grave(int player)
{
  int top = count_graveyard(player) - 1;
  if (top >= 0)
    {
      int type = cards_data[graveyard_ptr_mutable[player][top]].type;
      dispatch_event_with_initial_event_result(player, graveyard_source[player][top], EVENT_REMOVED_FROM_GRAVEYARD,
                                               ((type & TYPE_CREATURE) << 16) | ((type & TYPE_SPELL) << 8) | 1);
      graveyard_ptr_mutable[player][top] = -1;
      graveyard_source[player][top] = -1;
    }
}

// The primary function the exe uses to remove a card from a player's graveyard.  Does not put the card anywhere else.
void remove_card_from_grave(int player, int position)
{
  int type = cards_data[graveyard_ptr_mutable[player][position]].type;
  dispatch_event_with_initial_event_result(player, graveyard_source[player][position], EVENT_REMOVED_FROM_GRAVEYARD,
                                           ((type & TYPE_CREATURE) << 16) | ((type & TYPE_SPELL) << 8) | 1);
  obliterate_card_from_deck_impl(graveyard_ptr_mutable[player], position);
  obliterate_card_from_deck_impl(graveyard_source[player], position);
}

/**********
* To hand *
**********/

int is_hydra_head(int player, int card){
	return 0;
}

void bounce_permanent(int player, int card)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (instance->internal_card_id == -1)
    return;
  int was_in_play = in_play(player, card) != NULL;
  if (was_in_play)
    dispatch_trigger2(current_turn, TRIGGER_LEAVE_PLAY, "Card leaving play", 0, player, card);
  instance->internal_card_id = -1;
  destroy_attached_auras_and_obliterate_card(player, card);
  int owner = get_owner(player, card);
  int csvid = cards_data[instance->original_internal_card_id].id;
  int card_added = -1;
  if (!(instance->token_status & STATUS_TOKEN))
    {
      if (csvid == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES || csvid == CARD_ID_CHITTERING_HOST
          || csvid == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP)
        {
          int csvida, csvidb;
          unmeld(csvid, &csvida, &csvidb);
          card_added = add_card_to_hand(owner, get_internal_card_id_from_csv_id(csvida));
          add_card_to_hand(owner, get_internal_card_id_from_csv_id(csvidb));
        }
      else
        card_added = add_card_to_hand(owner, instance->original_internal_card_id);
    }
  call_sub_437E20_unless_ai_is_speculating();
  if (was_in_play)
    dispatch_trigger2(current_turn, TRIGGER_BOUNCE_PERMANENT, "Permanent to hand", 0, owner, card_added);
  if (card_added != -1)
    {
      if (csvid == CARD_ID_WARPED_DEVOTION)
        discard(owner, 0, owner);
      else if (csvid == CARD_ID_STORMFRONT_RIDERS)
        {
          token_generation_t token;
          default_token_definition(player, card, CARD_ID_SOLDIER, &token);
          token.tou = token.pow = 1;
          token.color_forced = COLOR_TEST_WHITE;
          token.no_sleight = 1;
          generate_token(&token);
        }
    }
}

int can_draw_cards_as_cost(int player, int amount)
{
  if (check_battlefield_for_id(ANYBODY, CARD_ID_MARALEN_OF_THE_MORNSONG)
	  || check_battlefield_for_id(ANYBODY, CARD_ID_OMEN_MACHINE))
	return 0;

  if (check_battlefield_for_id(ANYBODY, CARD_ID_SPIRIT_OF_THE_LABYRINTH)
	  && amount + cards_drawn_this_turn[player] > 1)
	return 0;

  if (amount > 0
	  && (amount > 500 || deck_ptr[player][amount-1] == -1))
	return 0;

  return 1;
}

int draw_a_card(int player)
{
  // 0x433190

  suppress_draw = 0;
  dispatch_trigger(player, TRIGGER_REPLACE_CARD_DRAW, gs_prompt_specialfephase_5/*PROMPT_SPECIALFEPHASE[5]*/, 1);
  if (suppress_draw)
	return -1;	// Changed from exe, which returned 0.  Though nothing looks at the return value except card_sindband().

  int card_added;

  if (player == HUMAN && g_selected_wizard_color != -1)
	{
	  card_added = add_card_to_hand(player, DrawRandomCardFromInitialLibrary(g_selected_wizard_color));	// Fetches from network?
	  // Begin additions
	  --hand_count[player];	// Since our add_card_to_hand() frontend increases it, and it's too soon
	  // End additions
	}
  else if (player == AI && g_opponent_initial_library_index == -1)	// Never seems to be set to anything but 1.  Just as well.
	{
	  int type;
	  switch (internal_rand(3))
		{
		  case 0:	type = TYPE_LAND;		break;
		  case 1:	type = TYPE_CREATURE;	break;
		  default:	type = TYPE_INSTANT|TYPE_INTERRUPT|TYPE_SORCERY|TYPE_ENCHANTMENT;	break;
		  // Interesting that it never cheats itself into drawing artifacts.
		}

	  int csvid, ok;
	  do
		{
		  csvid = internal_rand(g_card_count);
		  ok = is_card_color_compatible_with_mask(cards_data[csvid].color, g_current_encounter_color, g_encounter_color_match_mode);
		  if (ok && GetCardRarity(csvid) > g_current_encounter_strength)
			ok = 0;
		}
	  while (!ok
			 || !(cards_data[csvid].type & type)
			 || cards_data[csvid].extra_ability & EA_UNK40);

	  card_added = add_card_to_hand(player, csvid);
	  // Begin additions
	  --hand_count[player];	// Since our add_card_to_hand() frontend increases it, and it's too soon
	  // End additions
	}
  else
	{
	  card_added = deck_ptr[player][0];
	  if (card_added != -1)
		{
		  remove_card_from_deck(player, 0);
		  card_added = add_card_to_hand(player, card_added);
		  // Begin additions
		  --hand_count[player];	// Since our add_card_to_hand() frontend increases it, and it's too soon
		  // End additions
		}
	}

  card_instance_t* instance;

  if (card_added == -1
	  || ((instance = get_card_instance(player, card_added))
		  && instance->internal_card_id == -1))
	{
	  if (ai_is_speculating != 1)
		{
		  load_text(0, "PROMPT_DRAWACARD");

		  static char buf[260];
		  strcpy(buf, text_lines[player + 1]);	// Unclear that this is necessary.
		  set_centerwindow_txt(buf);

		  Sleep(2500);	// Sleep(2500)

		  *buf = 0;
		  set_centerwindow_txt(buf);

		  real_lose_the_game(player);	// We won't get here in any cases where lose_the_game() would prevent an actual game loss.
		}
	  else
		{
		  life[player] = -99;
		  life[1-player] = 20;
		}

	  // Begin additions
	  return -1;
	  // End additions
	}

  TENTATIVE_reassess_all_cards();	// TENTATIVE_reassess_all_cards()
  if (player == HUMAN && ai_is_speculating != 1)
	{
	  // This seems to manipulate the FullCard window.
	  load_text(0, "PROMPT_DRAWACARD");
	  if (g_duel_active)
		show_duel_card_preview(instance->internal_card_id, player, card_added);
	  else
		show_card_preview_if_human(instance->internal_card_id);
	}

  ++hand_count[player];
  if (cards_drawn_this_turn[player] != 0xFFFF)
	++cards_drawn_this_turn[player];

  play_sound_effect(WAV_DRAW);
  instance->state |= STATE_DRAWN_THIS_TURN;
  dispatch_trigger2(player, TRIGGER_CARD_DRAWN, gs_prompt_specialfephase_5/*PROMPT_SPECIALFEPHASE[5]*/, 0, player, card_added);

  return card_added;
}

int draw_cards(int player, int amount){
	return draw_cards_exe(player, amount);
}

int draw_some_cards_if_you_want(int player, int card, int t_player, int howmany)
{
  int mode = (1 << 0) | (1 << 2);
  int ai_choice = 0;
  char buffer[100];
  int pos = scnprintf(buffer, 100, " Draw %d card", howmany);
  if (howmany == 1)
    pos = scnprintf(buffer, 100, " Draw a card");
  else
    {
      if (howmany < 1)
        return 0;
      pos += scnprintf(buffer + pos, 100 - pos, "s");
      pos += scnprintf(buffer + pos, 100 - pos, "\n Choose the number of cards to draw");
      mode |= 1 << 1;
      if (t_player == AI)
        {
          int amount = count_deck(t_player);
          if (amount < 10)
            ai_choice = 2;
          else
            {
              int b_howmany = howmany;
              while (amount - b_howmany < 10)
                --b_howmany;
              if (b_howmany > 0)
                {
                  howmany = b_howmany;
                  ai_choice = 1;
                }
              else
                ai_choice = 2;
            }
        }
    }
  scnprintf(buffer + pos, 100 - pos, "\n Do not draw");
  int choice = do_dialog(t_player, player, card, -1, -1, buffer, ai_choice);
  while (!((1 << choice) & mode))
    ++choice;
  if (choice == 0 || (choice == 1 && player == AI))
    {
      draw_cards(t_player, howmany);
      return howmany;
    }
  if (choice == 1)
    {
      int ctd = choose_a_number(t_player, "How many cards you'll draw ?", howmany);
      if (ctd >= 0 && ctd <= howmany)
        {
          draw_cards(t_player, ctd);
          return howmany;
        }
    }
  return 0;
}

int draw_up_to_n_cards(int player, int maximum)
{
  int how_many;
  if (player == AI || ai_is_speculating == 1)
	how_many = MIN(count_deck(player) - 20, maximum);
  else
	how_many = choose_a_number(player, "Draw how many cards?", maximum);

  if (how_many > 0)
	draw_cards(player, how_many);

  return how_many;
}

/*************
* To library *
*************/

int from_graveyard_to_deck(int player, int selected, int action){
  // action = 1 -> Put on top
  // action = 2 -> Put on bottom
  // action = 3 -> Shuffle into deck

  const int *grave = get_grave(player);

  if( action == 1){
	 int card_added = add_card_to_hand(player, grave[selected]);
	 remove_card_from_grave(player, selected);
	 put_on_top_of_deck(player, card_added);
  }

  if( action == 2){
	 int *deck = deck_ptr[player];
	 deck[ count_deck(player) ] = grave[selected];
	 remove_card_from_grave(player, selected);
  }

  if( action == 3){
	 int *deck = deck_ptr[player];
	 deck[ count_deck(player) ] = grave[selected];
	 remove_card_from_grave(player, selected);
	 shuffle(player);
  }

  return action;
}

void real_put_on_top_of_deck(int player, int card);
void put_on_top_of_deck(int player, int card)
{
  if (player >= 0 && card >= 0 && in_hand(player, card))
    --hand_count[player];
  if (get_id(player, card) == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES
      || get_id(player, card) == CARD_ID_CHITTERING_HOST
      || get_id(player, card) == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP)
    {
      int csvida, csvidb;
      unmeld(get_id(player, card), &csvida, &csvidb);
      real_put_on_top_of_deck(player, card);
      remove_card_from_deck(player, 0);
      int carda = add_card_to_hand(player, get_internal_card_id_from_csv_id(csvida));
      int cardb = add_card_to_hand(player, get_internal_card_id_from_csv_id(csvidb));
      hand_count[player] -= 2;
      real_put_on_top_of_deck(player, carda);
      card = cardb;
    }
  real_put_on_top_of_deck(player, card);
}

void put_on_bottom_of_deck(int player, int card)
{
  if (player >= 0 && card >= 0 && in_hand(player, card))
    --hand_count[player];
  int melded = get_id(player, card) == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES
               || get_id(player, card) == CARD_ID_CHITTERING_HOST
               || get_id(player, card) == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP;
  if (melded)
    {
      int csvida, csvidb;
      unmeld(get_id(player, card), &csvida, &csvidb);
      real_put_on_top_of_deck(player, card);
      remove_card_from_deck(player, 0);
      int carda = add_card_to_hand(player, get_internal_card_id_from_csv_id(csvida));
      int cardb = add_card_to_hand(player, get_internal_card_id_from_csv_id(csvidb));
      hand_count[player] -= 2;
      real_put_on_top_of_deck(player, carda);
      real_put_on_top_of_deck(player, cardb);
    }
  else
    real_put_on_top_of_deck(player, card);
  if (!(get_card_instance(player, card)->token_status & (STATUS_TOKEN | STATUS_OBLITERATED)))
    {
      int owner = is_stolen(player, card) ? 1 - player : player;
      if (melded)
        put_card_in_deck_to_bottom(owner, 0);
      put_card_in_deck_to_bottom(owner, 0);
    }
}

/* Moves the card on top of player's library to the bottom, and returns its internal_card_id (or -1 if library is empty).  Equivalent to
 * put_card_in_deck_to_bottom(player, 0). */
int put_top_card_of_deck_to_bottom(int player)
{
  return put_card_in_deck_to_bottom(player, 0);
}

/* Moves the card at deck_ptr[player][position] to the bottom of his library, and returns its internal_card_id (or -1 if position is higher than the number of
 * cards in the library). */
int put_card_in_deck_to_bottom(int player, int position)
{
  int* deck = deck_ptr[player];
  int count = count_deck(player) - 1;
  if (count < position)
	return -1;

  int i, iid = deck[position];
  for (i = position; i < count; ++i)
	deck[i] = deck[i+1];

  deck[count] = iid;
  return iid;
}

void put_iid_under_the_first_x_cards_of_library(int player, int iid, int pos)
{
	int* deck = deck_ptr[player];

	int count = count_deck(player);
	pos = MIN(pos, count);

	for (; count > pos; --count)
		deck[count] = deck[count - 1];

	deck[pos] = iid;
}

void put_permanent_under_the_first_x_card_of_its_owners_library(int player, int card, int pos)
{
  if (get_card_instance(player, card)->token_status & STATUS_TOKEN)
    {
      obliterate_card(player, card);
      return;
    }
  int owner = get_owner(player, card);
  int iid = get_card_instance(player, card)->original_internal_card_id;
  int csvid = cards_data[iid].id;
  if (csvid == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES || csvid == CARD_ID_CHITTERING_HOST
      || csvid == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP)
    {
      int csvida, csvidb;
      obliterate_card(player, card);
      unmeld(csvid, &csvida, &csvidb);
      put_iid_under_the_first_x_cards_of_library(owner, get_internal_card_id_from_csv_id(csvida), pos);
      put_iid_under_the_first_x_cards_of_library(owner, get_internal_card_id_from_csv_id(csvidb), pos + 1);
    }
  else
    {
      obliterate_card(player, card);
      put_iid_under_the_first_x_cards_of_library(owner, iid, pos);
    }
}

void shuffle_into_library(int player, int card)
{
  put_on_top_of_deck(player, card);
  shuffle(is_stolen(player, card) ? 1-player : player);
}

int reshuffle_grave_into_deck(int player, int avoid_shuffle){

	int *deck = deck_ptr[player];
	const int *grave = get_grave(player);
	int d_count = count_deck(player);
	int count = count_graveyard(player)-1;
	int amount = 0;
	while( count > -1 ){
			deck[d_count] = grave[count];
			remove_card_from_grave(player, count);
			d_count++;
			count--;
			amount++;
	}
	if( avoid_shuffle != 1 ){
		shuffle(player);
	}
	return amount;
}

int reshuffle_hand_into_deck(int player, int avoid_shuffle){

	int count = active_cards_count[player]-1;
	int amount = 0;
	while( count > -1 ){
		   if( in_hand(player, count) ){
			   put_on_top_of_deck(player, count);
				amount++;
			}
			count--;
	 }
	if( avoid_shuffle != 1 ){
		shuffle(player);
	}
	return amount;
}

void reshuffle_all_into_deck(int player, int mode)
{
  // 0 --> normal mode
  // 1 --> "Restart the game" mode (Karn Liberated)
  // 2 --> put everything back to deck but don't reshuffle.
  int edh_general_iid = -1;
  int commander_found = 0;
  int count = active_cards_count[player] - 1;
  int planes[active_cards_count[player]];
  int pc = 0;
  while (count > -1)
  {
    if (in_play(player, count))
    {
      if (is_what(player, count, TYPE_PERMANENT))
      {
        if (check_special_flags2(player, count, SF2_COMMANDER))
        {
          if (player >= 0)
            kill_card(player, count, KILL_EXILE);
          commander_found = 1;
        }
        else
        {
          put_on_top_of_deck(player, count);
        }
      }
      else
      {
        if ((mode & 1))
        {
          if (get_id(player, count) == CARD_ID_ELDER_DRAGON_HIGHLANDER)
          {
            remove_counters(player, count, player, count, COUNTER_VORTEX,
                                        count_counters(player, count, COUNTER_VORTEX));
            if (get_card_instance(player, count)->targets[3].player != 0)
            {
              get_card_instance(player, count)->targets[3].player = 0;
              if (!commander_found)
              {
                edh_general_iid = get_card_instance(player, count)->info_slot;
              }
            }
          }
          if (is_plane_card(get_id(player, count)))
          {
            remove_counters(player, count, player, count, COUNTER_VORTEX,
                                        count_counters(player, count, COUNTER_VORTEX));
            planes[pc] = count;
            pc++;
          }
          if (get_id(player, count) == CARD_ID_MOUNT_KERALIA)
          {
            remove_counters(player, count, player, count, COUNTER_PRESSURE,
                                        count_counters(player, count, COUNTER_PRESSURE));
          }
          if (get_id(player, count) == CARD_ID_NAAR_ISLE)
          {
            remove_counters(player, count, player, count, COUNTER_FLAME,
                                        count_counters(player, count, COUNTER_FLAME));
          }
          if (get_id(player, count) == CARD_ID_ARETOPOLIS)
          {
            remove_counters(player, count, player, count, COUNTER_SCROLL,
                                        count_counters(player, count, COUNTER_SCROLL));
          }
          if (get_id(player, count) == CARD_ID_KILNSPIRE_DISTRICT)
          {
            remove_counters(player, count, player, count, COUNTER_CHARGE,
                                        count_counters(player, count, COUNTER_CHARGE));
          }
          int kill_effect = 1;
          if (cards_data[get_original_internal_card_id(player, count)].cc[2] != 0)
          {
            kill_effect = 0;
          }
          if (get_id(player, count) == CARD_ID_RULES_ENGINE)
          {
            kill_effect = 0;
          }
          card_instance_t *instance = get_card_instance(player, count);
          if (instance->damage_target_player > -1)
          {
            if (cards_data[get_original_internal_card_id(instance->damage_target_player,
                                                         instance->damage_target_card)]
                    .cc[2] != 0)
            {
              kill_effect = 0;
            }
            if (get_id(instance->damage_target_player, instance->damage_target_card) ==
                CARD_ID_RULES_ENGINE)
            {
              kill_effect = 0;
            }
          }
          if (kill_effect)
          {
            if (player >= 0)
              kill_card(player, count, KILL_EXILE);
          }
        }
      }
    }
    else if (in_hand(player, count))
    {
      put_on_top_of_deck(player, count);
    }
    count--;
  }

  const int *grave = get_grave(player);
  int *deck = deck_ptr[player];

  count = count_graveyard(player) - 1;
  while (count > -1)
  {
    int put_into_deck = 1;
    if (!commander_found && edh_general_iid != -1 && grave[count] == edh_general_iid)
    {
      if (!check_rfg(player, cards_data[edh_general_iid].id))
      {
        rfg_card_from_grave(player, count);
        put_into_deck = 0;
        commander_found = 1;
      }
    }
    if (put_into_deck)
    {
      deck[count_deck(player)] = grave[count];
      remove_card_from_grave(player, count);
    }
    count--;
  }
  if (!commander_found && edh_general_iid != -1)
  {
    count = 0;
    while (deck_ptr[player][count] != -1)
    {
      if (deck_ptr[player][count] == edh_general_iid)
      {
        rfg_card_in_deck(player, count);
        break;
      }
      count++;
    }
  }
  if (!(mode & 2))
  {
    shuffle(player);
  }
  count = 0;
  while (count < pc)
  {
    call_card_function(player, planes[count], EVENT_PLANESWALK_IN);
    count++;
  }
}

/***************
* To graveyard *
***************/

static void kill_card_impl(int player, int card, kill_t kill_mode, int flags)
{
  if (player < 0 || card < 0)
    return;

  enum { FROM_EXE = 1, SPELL_IS_BEING_COUNTERED = 2 };
  card_instance_t* instance = get_card_instance(player, card);
  if (instance->internal_card_id == -1 || (instance->token_status & STATUS_DYING))
    {
      instance->token_status |= STATUS_DYING;
      return;
    }
  if ((kill_mode == KILL_DESTROY || kill_mode == KILL_BURY)
      && (instance->state & (STATE_OUBLIETTED | STATE_INVISIBLE | STATE_IN_PLAY)) == STATE_IN_PLAY
      && (cards_data[instance->internal_card_id].type & TYPE_PERMANENT)
      && check_for_special_ability(player, card, SP_KEYWORD_INDESTRUCTIBLE))
    return;
  if (kill_mode == KILL_STATE_BASED_ACTION)
    {
      kill_mode = KILL_SACRIFICE;
      if (cards_data[instance->internal_card_id].type & TYPE_PERMANENT)
        set_special_flags(player, card, SF_KILL_STATE_BASED_ACTION);
    }
  if (kill_mode != KILL_EXILE && is_what(player, card, TYPE_SPELL)
      && check_special_flags(player, card, SF_EXILE_ON_RESOLUTION))
    kill_mode = KILL_EXILE;
  if (ai_is_speculating != 1 && kill_mode == KILL_SACRIFICE
      && !(flags & FROM_EXE) && in_play(player, card))
    {
      type_t typ = get_card_data(player, card)->type;
      if ((typ & TYPE_PERMANENT) && !(typ & TYPE_EFFECT))
        play_sound_effect(WAV_SACRFICE);
    }
  if (kill_mode != KILL_EXILE && is_what(player, card, TYPE_SPELL)
      && check_special_flags2(player, card, SF2_SOULFIRE_GRAND_MASTER)
      && !(flags & SPELL_IS_BEING_COUNTERED))
    {
      bounce_permanent(player, card);
      return;
    }

  int exile_event = 0;
  if (kill_mode == KILL_EXILE && !(flags & FROM_EXE) && !is_what(player, card, TYPE_EFFECT))
    {
      if (!(instance->state & STATE_IN_PLAY))
        {
          if (instance->state & (STATE_INVISIBLE | STATE_OUBLIETTED))
            instance->state |= STATE_IN_PLAY;
          else
            {
              rfg_card_in_hand(player, card);
              return;
            }
        }
      if (!(cards_data[instance->original_internal_card_id].type & TYPE_CREATURE))
        play_sound_effect(WAV_DESTROY);
      exile_event = (instance->state & (STATE_INVISIBLE | STATE_OUBLIETTED)) == 0;
    }
  if (!(flags & FROM_EXE) && in_hand(player, card))
    --hand_count[player];
  instance->token_status |= STATUS_DYING;
  int original_iid = instance->original_internal_card_id;
  int csvid = cards_data[original_iid].id;
  int melded = csvid == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES || csvid == CARD_ID_CHITTERING_HOST
               || csvid == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP;
  if (!(instance->state & STATE_IN_PLAY))
    kill_mode = KILL_SACRIFICE;
  instance->kill_code = kill_mode;
  if ((instance->state & STATE_IN_PLAY) && !(instance->token_status & STATUS_OBLITERATED)
      && kill_mode != KILL_SACRIFICE && kill_mode != KILL_EXILE
      && (cards_data[instance->internal_card_id].type & (TYPE_ARTIFACT | TYPE_CREATURE | TYPE_LAND)))
    {
      if (melded)
        {
          int csvida, csvidb;
          unmeld(csvid, &csvida, &csvidb);
          obliterate_card(player, card);
          int carda = add_card_to_hand(player, get_internal_card_id_from_csv_id(csvida));
          int cardb = add_card_to_hand(player, get_internal_card_id_from_csv_id(csvidb));
          hand_count[player] -= 2;
          card_instance_t* insta = get_card_instance(player, carda);
          card_instance_t* instb = get_card_instance(player, cardb);
          insta->kill_code = instb->kill_code = kill_mode;
          insta->state |= STATE_IN_PLAY;
          instb->state |= STATE_IN_PLAY;
          insta->unknown0x14 = instb->unknown0x14 = TRIGGER_GRAVEYARD_ORDER;
        }
      else
        {
          instance->unknown0x14 = TRIGGER_GRAVEYARD_ORDER;
          instance->state |= STATE_IN_PLAY;
        }
      pending_killed_card_handler = process_killed_card;
      return;
    }
  if (melded)
    {
      int csvida, csvidb;
      unmeld(csvid, &csvida, &csvidb);
      instance->kill_code = 0;
      obliterate_card(player, card);
      int carda = add_card_to_hand(player, get_internal_card_id_from_csv_id(csvida));
      int cardb = add_card_to_hand(player, get_internal_card_id_from_csv_id(csvidb));
      hand_count[player] -= 2;
      get_card_instance(player, carda)->kill_code = kill_mode;
      get_card_instance(player, cardb)->kill_code = kill_mode;
      process_killed_card(player, carda);
      process_killed_card(player, cardb);
    }
  else
    process_killed_card(player, card);
  if (exile_event)
    dispatch_event(player, original_iid, EVENT_CARD_TO_EXILE);
}

void kill_card(int player, int card, kill_t kill_mode)
{
	kill_card_impl(player, card, kill_mode, 0);
}

void kill_card_exe(int player, int card, kill_t kill_mode)
{
  kill_card_impl(player, card, kill_mode, 1);
}

int effect_this_dies(int player, int card, event_t event);
void make_smallcard_visible(int player, int card);

int destroy_attached_auras_and_obliterate_card(int player, int card)
{
  // Original at 0x477D90
  card_instance_t* instance;
  target_t auras_to_destroy[300];

  int p, c, count = 0;
  for (p = 0; p < 2; ++p)
	  for (c = 0; active_cards_count[p] > c; ++c)
		if (!(p == player && c == card)
			&& in_play(p, c))
		  {
			instance = get_card_instance(p, c);
			if (instance->damage_target_player == player && instance->damage_target_card == card)
			  {
				if ((cards_data[instance->internal_card_id].type & (TYPE_LAND | TYPE_CREATURE | TYPE_ARTIFACT))
					// Begin additions
					|| instance->internal_card_id == activation_card
					//|| is_planeswalker(p, c)	// add if we ever make planewalkers attachable to a card for some reason
					// End additions
					)
				  {
					instance->damage_target_player = -1;
					instance->damage_target_card = -1;
					make_smallcard_visible(p, c);	/* e.g. if it was attached to an attacking creature, this makes it visible in the territory window - see
													 * http://www.slightlymagic.net/forum/tracker.php?t=987 */
				  }
				else if (!(instance->internal_card_id == LEGACY_EFFECT_CUSTOM
						   && instance->info_slot == (intptr_t)effect_this_dies))
				  {
					auras_to_destroy[count].player = p;
					auras_to_destroy[count].card = c;
					++count;
				  }
				else
				  make_smallcard_visible(p, c);	// as above
			  }
		  }

  for (--count; count >= 0; --count)
	if (auras_to_destroy[count].player >= 0 && auras_to_destroy[count].card >= 0)
	  kill_card(auras_to_destroy[count].player, auras_to_destroy[count].card, KILL_STATE_BASED_ACTION);

  instance = get_card_instance(player, card);
  if (ai_is_speculating != 1)
    {
      instance->counter_toughness = 0;
      instance->damage_on_card = 0;
      instance->counter_power = 0;
      instance->blocking = 255;
    }

  return -1;	// Not directly used by the original exe, but sometimes seen by asm-coded cards (like Allay) via through bounce_permanent() (which also is void in the original exe)
}

void real_counter_a_spell(int player, int card, int to_counter_player, int to_counter_card)
{
  if (pre_counterspell(player, card, to_counter_player, to_counter_card)
      && to_counter_player >= 0 && to_counter_card >= 0)
    kill_card_impl(to_counter_player, to_counter_card, KILL_STATE_BASED_ACTION, 2);
}

// Returns 0 if didn't validate, 1 if validated but didn't pay (so spell was countered), 2 if validated and paid (so spell wasn't countered).
int counterspell_resolve_unless_pay_x(int player, int card, target_definition_t *td, int target_num,
                                      int x)
{
  int card_00;
  int result;
  card_instance_t *instance;
  kill_t kill_mode;
  int countered;
  char options[40];

  kill_mode = KILL_STATE_BASED_ACTION;
  if (target_num < 0)
  {
    kill_mode = KILL_EXILE;
    target_num = 0;
  }
  result = counterspell_validate(player, card, td, target_num);
  countered = 0;
  if (result != 0)
  {
    if (x < 1)
    {
      return 2;
    }
    sprintf(options, " Pay %d\n Decline", x);
    instance = get_card_instance(player, card);
    result = has_mana(instance->targets[target_num].player, 7, x);
    if (((result != 0) && (result = instance->targets[target_num].player,
                           result = do_dialog(result, player, card, result,
                                              instance->targets[target_num].card, options, 0),
                           result == 0)) &&
        (result =
             charge_mana_while_resolving(player, card, EVENT_RESOLVE_SPELL,
                                         instance->targets[target_num].player, COLOR_COLORLESS, x),
         result != 0))
    {
      return 2;
    }
    result = pre_counterspell(player, card, instance->targets[target_num].player,
                              instance->targets[target_num].card);
    countered = 0;
    if (result != 0)
    {
      result = instance->targets[target_num].player;
      card_00 = instance->targets[target_num].card;
      if ((-1 < result) && (-1 < card_00))
      {
        kill_card_impl(result, card_00, kill_mode, 2);
      }
      result = is_what(player, card, 0x20);
      countered = 1;
      if (result == 0)
      {
        countered = 1;
        play_sound_effect(WAV_INTERUPT);
      }
    }
  }
  return countered;
}

#define CASES_GY_FROM_ANYWHERE_REPLACE_SHUFFLE	\
	   CARD_ID_BLIGHTSTEEL_COLOSSUS:			\
  case CARD_ID_DARKSTEEL_COLOSSUS:				\
  case CARD_ID_LEGACY_WEAPON:					\
  case CARD_ID_PROGENITUS: \
  case CARD_ID_NEXUS_OF_FATE

#define CASES_GY_FROM_ANYWHERE_TRIGGER_SHUFFLE	\
	   CARD_ID_DREAD:							\
  case CARD_ID_GUILE:							\
  case CARD_ID_HOSTILITY:						\
  case CARD_ID_PURITY:							\
  case CARD_ID_SERRA_AVATAR:					\
  case CARD_ID_VIGOR:							\
  case CARD_ID_WORLDSPINE_WURM

#define CASES_GY_FROM_ANYWHERE_TRIGGER_SHUFFLE_WHOLE_GY	\
	   CARD_ID_EMRAKUL_THE_AEONS_TORN:					\
  case CARD_ID_KOZILEK_BUTCHER_OF_TRUTH:				\
  case CARD_ID_ULAMOG_THE_INFINITE_GYRE

// Put a newly-created card with internal_card_id iid on top of player's graveyard and return its position.  Caller's responsibility to generate source_val.
static int raw_put_iid_on_top_of_graveyard_impl(int player, int iid, int position_hint, int32_t source_val)
{
  types_of_cards_in_graveyard[player] |= cards_data[iid].type;
  increase_trap_condition(player, TRAP_CARDS_TO_GY_FROM_ANYWHERE, 1);

  int* grave = graveyard_ptr_mutable[player];

  int i;
  if (position_hint > 0 && grave[position_hint - 1] != -1)
	i = position_hint;
  else
	i = 0;

  for (; i < 500; ++i)
	if (grave[i] == -1)
	  {
		grave[i] = iid;
		graveyard_source[player][i] = source_val;
		return i;
	  }

  return -1;	// And probably crash, but we deserve to at this point anyway
}

/* Put a newly-created card with internal_card_id iid on top of player's graveyard and return its position.  Should only be used to replace cards temporarily
 * removed as an implementation detail - doesn't run any "whenever a card is put into a graveyard" triggers. */
int raw_put_iid_on_top_of_graveyard(int player, int iid)
{
  return raw_put_iid_on_top_of_graveyard_impl(player, iid, 0, GENERATE_GRAVEYARD_SOURCE_VALUE(0, 255));
}

static int effect_self_mill(int player, int card, event_t event);
static int effect_replace_self_mill(int player, int card, event_t event);

int gy_from_anywhere_pos = -1;
int gy_from_anywhere_source = -1;
static void raw_put_iid_on_top_of_graveyard_with_triggers(int player, int iid, int player_while_instantiated, int card_while_instantiated)
{
  switch (cards_data[iid].id)
	{
	  case CASES_GY_FROM_ANYWHERE_REPLACE_SHUFFLE:
		enable_xtrigger_flags |= ENABLE_XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY;
		create_legacy_effect_from_iid(player, iid, effect_replace_self_mill, -1, -1);
		break;
	}

  if (enable_xtrigger_flags & ENABLE_XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY)
	{
	  int old_replace_milled = replace_milled;
	  replace_milled = 0;

	  char prompt[100];
	  if (ai_is_speculating == 1)
		prompt[0] = 0;
	  else
		scnprintf(prompt, 100, "%s to graveyard", cards_ptr[cards_data[iid].id]->full_name);

	  dispatch_xtrigger2(player, XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY, prompt, 0, player, iid);

	  int replaced = replace_milled;
	  replace_milled = old_replace_milled;

	  if (replaced)
		return;
	}

  int gy_pos = raw_put_iid_on_top_of_graveyard_impl(player, iid, 0, GENERATE_GRAVEYARD_SOURCE_VALUE(player_while_instantiated, card_while_instantiated));

  switch (cards_data[iid].id)
	{
	  case CASES_GY_FROM_ANYWHERE_TRIGGER_SHUFFLE:
	  case CASES_GY_FROM_ANYWHERE_TRIGGER_SHUFFLE_WHOLE_GY:
		enable_xtrigger_flags |= ENABLE_XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY;
		int leg = create_legacy_effect_from_iid(player, iid, effect_self_mill, -1, -1);
		card_instance_t* legacy = get_card_instance(player, leg);
		legacy->targets[0].player = gy_pos;
		legacy->targets[0].card = graveyard_source[player][gy_pos];
		legacy->eot_toughness = 2;	// Always show the second alternate text if the text has alternates
		break;
	}

  if (enable_xtrigger_flags & ENABLE_XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY)
	{
	  int old_gy_from_anywhere_pos = gy_from_anywhere_pos;
	  int old_gy_from_anywhere_source = gy_from_anywhere_source;
	  gy_from_anywhere_pos = gy_pos;
	  gy_from_anywhere_source = graveyard_source[player][gy_pos];

	  char prompt[100];
	  if (ai_is_speculating == 1)
		prompt[0] = 0;
	  else
		scnprintf(prompt, 100, "%s in graveyard", cards_ptr[cards_data[iid].id]->full_name);

	  dispatch_xtrigger2(current_turn, XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY, prompt, 0, player, iid);

	  gy_from_anywhere_pos = old_gy_from_anywhere_pos;
	  gy_from_anywhere_source = old_gy_from_anywhere_source;
	}
}

// The primary function the exe uses to put a card in a player's graveyard.  Should not be called directly.
void raw_put_card_in_graveyard(int player, int card)
{
  // 0x477ec0
  card_instance_t* instance = get_card_instance(player, card);

  int owner, oid = instance->original_internal_card_id;

  if (instance->state & STATE_OWNED_BY_OPPONENT)
	owner = 1;
  else
	owner = 0;

  raw_put_iid_on_top_of_graveyard_with_triggers(owner, oid, player, card);
}

// Moves a card from a player's exile zone to his graveyard.
void from_exile_to_graveyard(int player, int position_in_exile)
{
  ASSERT(position_in_exile >= 0 && position_in_exile < 500 && player >= 0 && player <= 1);
  int iid = rfg_ptr[player][position_in_exile];
  ASSERT(iid != -1);
  remove_card_from_rfg_by_position(player, position_in_exile);

  raw_put_iid_on_top_of_graveyard_with_triggers(player, iid, 0, 255);
}

/* Replaces the card in player's graveyard at position position to the card back image, and returns the internal_card_id that was there.  It's the caller's
 * responsibility to turn it back with turn_card_in_grave_face_up(), using the same iid that was returned. */
int turn_card_in_grave_face_down(int player, int position)
{
  ASSERT(position >= 0 && position < 500 && player >= 0 && player <= 1);
  int previous = graveyard_ptr_mutable[player][position];
  ASSERT(previous != -1 && previous != iid_draw_a_card);
  graveyard_ptr_mutable[player][position] = iid_draw_a_card;	// Doesn't change graveyard_source[][]
  return previous;
}

/* Changes the card in player's graveyard at position position to the internal_card_id that was there.  Aborts if the current card there isn't face-down.
 * Please don't abuse this function to assign arbitrary values within a player's graveyard. */
void turn_card_in_grave_face_up(int player, int position, int iid)
{
  ASSERT(iid != -1 && iid != iid_draw_a_card);
  ASSERT(graveyard_ptr_mutable[player][position] == iid_draw_a_card);
  types_of_cards_in_graveyard[player] |= cards_data[iid].type;
  graveyard_ptr_mutable[player][position] = iid;	// Doesn't change graveyard_source[][]
}

/**********
* To ante *
**********/

int can_ante_top_card_of_library(int player)
{
  if (deck_ptr[player][0] == -1)
	return 0;

  int pos;
  for (pos = 0; pos < 16; ++pos)
	if (ante_cards[player][pos] == -1)
	  return 1;

  return 0;
}

int ante_top_card_of_library(int player)
{
  // 0x458070.  Exe version draws the card (allowing triggers and replacements) then antes it from hand, instead of anteing directly from library.

  if (deck_ptr[player][0] == -1)
	return 0;	// no cards in library

  int pos;
  for (pos = 0; pos < 16; ++pos)
	if (ante_cards[player][pos] == -1)
	  {
		int iid = deck_ptr[player][0];
		obliterate_top_card_of_deck(player);
		ante_cards[player][pos] = iid;

		load_text(0, "ANTE_A_CARD");
		DIALOG(player, 0, EVENT_ACTIVATE, DLG_MSG(text_lines[player]), DLG_FULLCARD_ID(iid));

		return 1;
	  }

  return 0;	// can't ante any more cards
}

/***********
* To exile *
***********/

// Exiles the card at position pos in deck to player's exile zone.
static int rfg_card_from_deck_impl(int player, int* deck, int pos)
{
  int iid = deck[pos];
  if (iid != -1)
    {
      int* rfg = rfg_ptr[player];
      int rfg_pos = count_rfg(player);
      iid = deck[pos];
      rfg[rfg_pos] = iid;
      if (iid != -1)
        obliterate_card_from_deck_impl(deck, pos);
      play_sound_effect(WAV_DESTROY);
      dispatch_event(player, iid, EVENT_CARD_TO_EXILE);
    }
  return iid;
}

// Exile the card at position grave_id in player's graveyard.
int rfg_card_from_grave(int player, int grave_id)
{
  if ((unsigned int)grave_id >= 500)
    return -1;
  int iid = graveyard_ptr_mutable[player][grave_id];
  int num_spells = cards_data[iid].type & 0x38;
  int num_creatures = cards_data[iid].type & 2;
  int num_cards = 1;
  dispatch_event_with_initial_event_result(player, iid, EVENT_REMOVED_FROM_GRAVEYARD,
      (num_spells << 8) | (num_creatures << 16) | num_cards);
  if (graveyard_source[player][grave_id] != -1)
    obliterate_card_from_deck_impl(graveyard_source[player], grave_id);
  iid = rfg_card_from_deck_impl(player, graveyard_ptr_mutable[player], grave_id);
  for (int c = 0; c < active_cards_count[player]; ++c)
  {
    if (in_play(player, c))
    {
      card_instance_t *instance = get_card_instance(player, c);
      if (cards_data[instance->original_internal_card_id].id != CARD_ID_SAKASHIMA_THE_IMPOSTOR &&
          cards_data[instance->internal_card_id].id == CARD_ID_RAKSHASA_VIZIER &&
          !is_humiliated(player, c, AL_TRIG))
        add_counters(player, c, player, c, COUNTER_P1_P1, 1);
    }
  }
  if (is_what(-1, iid, TYPE_CREATURE))
    ereboss_titan_effect(1 - player);
  return iid;
}

// Exile an arbitrary player/card.  Works for cards on the stack, in play, or oublietted, not just in hand, despite the name.
int rfg_card_in_hand(int player, int card){
	put_on_top_of_deck(player, card);
	return rfg_top_card_of_deck(player);
}

// Exile the top card of player's library.
int rfg_top_card_of_deck(int player){
	return rfg_card_from_deck_impl(player, deck_ptr[player], 0);
}

// Exile the card at position pos in player's library.
int rfg_card_in_deck(int player, int pos){
	return rfg_card_from_deck_impl(player, deck_ptr[player], pos);
}

// Exiles every card in the 500-card array deck to player's exile zone.
static int rfg_whole_deck_impl(int player, int* deck)
{
  int* rfg = rfg_ptr[player];
  int deck_pos = 0;
  int rfg_pos = 0;
  for (; deck_pos < 500 && deck[deck_pos] != -1; ++deck_pos)
    {
      int iid = deck[deck_pos];
      while (rfg[rfg_pos] != -1)
        ++rfg_pos;
      if (rfg_pos >= 500)
        break;
      rfg[rfg_pos] = iid;
      deck[deck_pos] = -1;
      dispatch_event(player, iid, EVENT_CARD_TO_EXILE);
    }
  if (deck_pos > 0)
    play_sound_effect(WAV_DESTROY);
  return deck_pos;
}

// Exile player's entire graveyard.
int rfg_whole_graveyard(int player)
{
  int c;
  int num_cards;
  int *deck;
  int exiled_count;
  int instant_sorcery_count;
  unsigned int graveyard_count;
  card_instance_t *instance;
  int source_player;
  int source_index;

  num_cards = count_graveyard_by_type(player, 2);
  instant_sorcery_count = count_graveyard_by_type(player, 0x38);
  graveyard_count = count_graveyard(player);
  dispatch_event_with_initial_event_result(
      player, graveyard_source[player][0], EVENT_REMOVED_FROM_GRAVEYARD,
      instant_sorcery_count << 8 | num_cards << 0x10 | graveyard_count);
  deck = graveyard_ptr_mutable[player];
  for (source_player = 0; source_player < 2; ++source_player)
    for (source_index = 0; source_index < 500; ++source_index)
      graveyard_source[source_player][source_index] = -1;
  exiled_count = rfg_whole_deck_impl(player, deck);
  if (0 < active_cards_count[player])
  {
    c = 0;
    do
    {
      instance = in_play(player, c);
      if (instance != NULL)
      {
        instance = get_card_instance(player, c);
        if ((cards_data[instance->original_internal_card_id].id != CARD_ID_SAKASHIMA_THE_IMPOSTOR) &&
            (cards_data[instance->internal_card_id].id == CARD_ID_RAKSHASA_VIZIER))
        {
          if (is_humiliated(player, c, AL_TRIG) == 0)
          {
            add_counters(player, c, player, c, COUNTER_P1_P1, exiled_count);
          }
        }
      }
      c = c + 1;
    } while (c < active_cards_count[player]);
  }
  return exiled_count;
}

// Exile player's entire library.
int rfg_whole_library(int player)
{
  return rfg_whole_deck_impl(player, deck_ptr[player]);
}

// Exile the top number cards of player's library.
void rfg_top_n_cards_of_deck(int player, int number)
{
  if (number < 1)
    return;
  int* rfg = rfg_ptr[player];
  int* deck = deck_ptr[player];
  int rfg_pos = 0;
  int any_exiled = 0;
  for (int deck_pos = 0; deck_pos < number && deck[deck_pos] != -1; ++deck_pos)
    {
      int iid = deck[deck_pos];
      while (rfg[rfg_pos] != -1)
        ++rfg_pos;
      if (rfg_pos >= 500)
        break;
      rfg[rfg_pos] = iid;
      remove_card_from_deck(player, 0);
      dispatch_event(player, iid, EVENT_CARD_TO_EXILE);
      any_exiled = 1;
    }
  if (any_exiled)
    play_sound_effect(WAV_DESTROY);
}

// Add a card with internal_card_id iid to player's exile zone.  Does not remove it from anywhere else first.  Returns its position in rfg_ptr[player].
int add_card_to_rfg(int player, int iid)
{
  int* rfg = rfg_ptr[player];
  int pos = count_rfg(player);
  rfg[pos] = iid;
  dispatch_event(player, iid, EVENT_CARD_TO_EXILE);
  return pos;
}

// As add_card_to_rfg(), but converts the csvid to an iid first.
int add_csvid_to_rfg(int player, int csvid)
{
  return add_card_to_rfg(player, get_internal_card_id_from_csv_id(csvid));
}

// Return the highest position in player's exile zone with internal_card_id iid, or -1 if not found.
int find_iid_in_rfg(int player, int iid){
	int* rfg = rfg_ptr[player];
	int i;
	for (i = count_rfg(player); i >= 0; --i){
		if (rfg[i] == iid){
			return i;
		}
	}
	return -1;
}

// Obliterates the card at position count in player's exile zone.
void remove_card_from_rfg_by_position(int player, int count){
	obliterate_card_from_deck_impl(rfg_ptr[player], count);
}

// Obliterates the highest-positioned card in player's exile zone with csvid.  Returns 1 if not found, else 0.
int remove_card_from_rfg(int player, int csvid){
	int i = find_iid_in_rfg(player, get_internal_card_id_from_csv_id(csvid));
	if (i == -1){
		return 0;
	}
	remove_card_from_rfg_by_position(player, i);
	return 1;
}

// Returns 1 if a card with csvid is in player's exile zone.
int check_rfg(int player, int csvid){
	return find_iid_in_rfg(player, get_internal_card_id_from_csv_id(csvid)) == -1 ? 0 : 1;
}

int rfg_target_permanent(int player, int card){

	if( ! is_what(player, card, TYPE_PERMANENT) || ! in_play(player, card) ){
		return -1;
	}
	if( is_token(player, card) ){
		kill_card(player, card, KILL_EXILE);
		return -1;
	}

	card_instance_t* instance = get_card_instance(player, card);

	int orig = cards_data[instance->original_internal_card_id].id;

	int owner = (instance->state & STATE_OWNED_BY_OPPONENT) ? 1 : 0;

	if (owner != player)
		orig += available_slots;

	kill_card(player, card, KILL_EXILE);

	return orig;
}

/****************************
* Higher-level manipulation *
****************************/

int put_into_play_a_card_from_deck(int player, int t_player, int deck_position)
{
  int* deck = deck_ptr[t_player];
  if (is_what(-1, deck[deck_position], TYPE_CREATURE)
      && grafdiggers_cages(player, TYPE_CREATURE, TUTOR_FROM_DECK))
    return -1;
  int card_added = add_card_to_hand(player, deck[deck_position]);
  remove_card_from_deck(t_player, deck_position);
  if (t_player != player)
    get_card_instance(player, card_added)->state ^= STATE_OWNED_BY_OPPONENT;
  set_special_flags(player, card_added, SF_PLAYED_FROM_DECK);
  put_into_play(player, card_added);
  return card_added;
}

static int effect_replace_self_mill(int player, int card, event_t event)
{
  // If ~ would be put into your graveyard from your library/anywhere, ...

  // display_pic_csv_id: csvid

  enable_xtrigger_flags |= ENABLE_XTRIGGER_REPLACE_MILL | ENABLE_XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY;
  if ((xtrigger_condition() == XTRIGGER_REPLACE_MILL || xtrigger_condition() == XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY)
	  && affect_me(player, card) && reason_for_trigger_controller == player && !replace_milled
	  && trigger_cause_controller == player)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int iid, csvid = instance->display_pic_csv_id;

	  if (xtrigger_condition() == XTRIGGER_REPLACE_MILL)
		iid = deck_ptr[player][trigger_cause];
	  else
		iid = trigger_cause;

	  if (cards_data[iid].id != csvid)	// ...shouldn't have gotten here, but ok
		return 0;

	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_MANDATORY;

	  if (event == EVENT_END_TRIGGER)
		kill_card(player, card, KILL_EXILE);

	  if (event == EVENT_RESOLVE_TRIGGER)
		switch (csvid)
		  {
			case CASES_GY_FROM_ANYWHERE_REPLACE_SHUFFLE:
			  // If ~ would be put into a graveyard from anywhere, reveal ~ and shuffle it into its owner's library instead.
			  kill_card(player, card, KILL_EXILE);

			  if (ai_is_speculating != 1)
				{
				  // Yuck, can't use standard reveal_card_iid() because card is an ugly orange effect.
				  load_text(0, "REVEALS");
				  DIALOG(player, card, EVENT_ACTIVATE,
						 DLG_FULLCARD_ID(iid),
						 DLG_MSG(text_lines[0]));
				}

			  if (xtrigger_condition() == XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY)
				deck_ptr[player][count_deck(player)] = iid;
			  // otherwise, card is still in deck anyway; just don't remove it, and prevent special_mill() from doing so

			  shuffle(player);
			  replace_milled = 1;
			  break;

			default:
			  abort();
		  }
	}

  return 0;
}

static int effect_self_mill(int player, int card, event_t event)
{
  // When ~ is put into your graveyard from your library/anywhere, ...

  /* targets[0].player: tentative position in graveyard
   * targets[0].card: graveyard_source[][] value
   * display_pic_csv_id: csvid */

  if ((player_bits[player] & PB_GY_ABILITIES_DISABLED) && event != EVENT_END_TRIGGER)
    return 0;

  enable_xtrigger_flags |= ENABLE_XTRIGGER_MILLED | ENABLE_XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY;
  if ((xtrigger_condition() == XTRIGGER_MILLED || xtrigger_condition() == XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY)
	  && affect_me(player, card) && reason_for_trigger_controller == player
	  && trigger_cause_controller == player)
	{
	  if (event == EVENT_END_TRIGGER)
		{
		  kill_card(player, card, KILL_EXILE);
		  return 0;
		}

	  card_instance_t* instance = get_card_instance(player, card);
	  int csvid = instance->display_pic_csv_id;
	  int pos;

	  switch (csvid)
		{
		  case CARD_ID_NARCOMOEBA:
			// When ~ is put into your graveyard from your library, you may put it onto the battlefield.
			if (xtrigger_condition() == XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY)
			  return 0;
			pos = find_in_graveyard_by_source(player, instance->targets[0].card, instance->targets[0].player);
			if (pos == -1)	// not in graveyard anymore
			  kill_card(player, card, KILL_EXILE);
			else if (event == EVENT_TRIGGER)
			  event_result |= duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;
			else if (event == EVENT_RESOLVE_TRIGGER)
			  {
				kill_card(player, card, KILL_EXILE);
				reanimate_permanent(player, -1, player, pos, REANIMATE_DEFAULT);
			  }
			break;

      case CARD_ID_CREEPING_CHILL:
        if (xtrigger_condition() == XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY)
          return 0;
        pos = find_in_graveyard_by_source(player, instance->targets[0].card, instance->targets[0].player);
        if (pos == -1)
          kill_card(player, card, KILL_EXILE);
        else if (event == EVENT_TRIGGER)
          event_result |= duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;
        else if (event == EVENT_RESOLVE_TRIGGER)
        {
          damage_player(1-player, 3, player, card);
          gain_life(player, 3);
          rfg_card_from_grave(player, pos);
          kill_card(player, card, KILL_EXILE);
        }
        break;

		  case CASES_GY_FROM_ANYWHERE_TRIGGER_SHUFFLE:
			// When ~ is put into a graveyard from anywhere, shuffle it into its owner's library.
			pos = find_in_graveyard_by_source(player, instance->targets[0].card, instance->targets[0].player);
			if (pos == -1)	// not in graveyard anymore
			  kill_card(player, card, KILL_EXILE);
			else if (event == EVENT_TRIGGER)
			  event_result |= RESOLVE_TRIGGER_MANDATORY;
			else if (event == EVENT_RESOLVE_TRIGGER)
			  {
				int iid = get_internal_card_id_from_csv_id(csvid);
				kill_card(player, card, KILL_EXILE);
				if (ai_is_speculating != 1)
				  {
					// Strictly speaking, shouldn't explicitly reveal, but do so because this is confusing (especially when coming from a library)
					// Yuck, can't use standard reveal_card_iid() because card is an ugly orange effect.
					load_text(0, "REVEALS");
					DIALOG(player, card, EVENT_ACTIVATE,
						   DLG_FULLCARD_ID(iid),
						   DLG_MSG(text_lines[0]));
				  }
				from_graveyard_to_deck(player, pos, 3);
			  }
			break;

		  case CARD_ID_GAEAS_BLESSING:
			// When ~ is put into your graveyard from your library, shuffle your graveyard into your library.
			if (xtrigger_condition() == XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY)
			  return 0;
			// else fall through
		  case CASES_GY_FROM_ANYWHERE_TRIGGER_SHUFFLE_WHOLE_GY:
			// When ~ is put into a graveyard from anywhere, its owner shuffles his or her graveyard into his or her library.
			// (Unlike the others, it doesn't matter if these are in the graveyard anymore)
			if (event == EVENT_TRIGGER)
			  event_result |= RESOLVE_TRIGGER_MANDATORY;
			else if (event == EVENT_RESOLVE_TRIGGER)
			  {
				int iid = get_internal_card_id_from_csv_id(csvid);
				kill_card(player, card, KILL_EXILE);
				if (ai_is_speculating != 1)
				  {
					// Strictly speaking, shouldn't explicitly reveal, but do so because this is confusing (especially when coming from a library)
					// Yuck, can't use standard reveal_card_iid() because card is an ugly orange effect.
					load_text(0, "REVEALS");
					DIALOG(player, card, EVENT_ACTIVATE,
						   DLG_FULLCARD_ID(iid),
						   DLG_MSG(text_lines[0]));
				  }
				reshuffle_grave_into_deck(player, 0);
			  }
			break;

		  default:
			abort();
		}
	}

  return 0;
}

int replace_milled = 0;
milled_t* cards_milled = NULL;
int num_cards_milled = 0;
void special_mill(int player, int card, int source_csvid, int orig_t_player, int amount)
{
  if (amount <= 0)
    return;

  int mill_from_bottom =
      (source_csvid == CARD_ID_CELLAR_DOOR || source_csvid == CARD_ID_GRENZO_DUNGEON_WARDEN) ? 1
                                                                                             : 0;
  int number[2] = {0, 0};
  if (orig_t_player == ANYBODY)
    number[0] = number[1] = amount;
  else if (orig_t_player == 0 || orig_t_player == 1)
    number[orig_t_player] = amount;
  else
    return;
  if (enable_xtrigger_flags & ENABLE_XTRIGGER_REPLACE_MILL)
  {
    int old_replace_milled = replace_milled;
    for (int apnap = 0; apnap < 2; ++apnap)
    {
      int p = current_turn ^ apnap;
      if (number[p] > 0)
      {
        replace_milled = number[p];
        char prompt[100];
        if (ai_is_speculating == 1)
          prompt[0] = 0;
        else
          scnprintf(prompt, 100, "%s milling %d cards", p == 0 ? "You" : g_duel_message_text,
                    number[p]);
        dispatch_xtrigger2(p, XTRIGGER_REPLACE_MILL_CARDS, prompt, 0, p, 0);
        number[p] = MAX(0, replace_milled);
      }
    }
    replace_milled = old_replace_milled;
  }
  int gyruda_iids[5][8];
  int gyruda_slots_filled = 0;
  for (int apnap = 0; apnap < 2; ++apnap)
  {
    int k = current_turn ^ apnap;
    if (orig_t_player == k || orig_t_player == ANYBODY)
    {
      int t_player = k;
      int *deck = deck_ptr[t_player];

      milled_t local_milled[number[k] + 1];

      int gy_position_hint = count_graveyard(t_player) - 1;
      int i, local_num_cards_milled = 0;
      int replaced = 0;
      if (number[k] <= 0)
        return;
      for (i = 0; i < number[k] && deck[0] != -1; ++i)
      {
        int deck_pos = mill_from_bottom ? count_deck(t_player) - 1 : 0;
        int iid = deck[deck_pos];
        int csvid = cards_data[iid].id;

        // Cards that have "When this card would be put into your graveyard from your library"
        // replacement effects
        switch (csvid)
        {
        case CASES_GY_FROM_ANYWHERE_REPLACE_SHUFFLE:
          enable_xtrigger_flags |= ENABLE_XTRIGGER_REPLACE_MILL;
          create_legacy_effect_from_iid(t_player, iid, effect_replace_self_mill, -1, -1);
          break;
        }
        if (enable_xtrigger_flags & ENABLE_XTRIGGER_REPLACE_MILL)
        {
          int old_replace_milled = replace_milled;
          replace_milled = 0;

          char prompt[100];
          if (ai_is_speculating == 1)
            prompt[0] = 0;
          else
            scnprintf(prompt, 100, "Milling %s", cards_ptr[cards_data[iid].id]->full_name);

          dispatch_xtrigger2(t_player, XTRIGGER_REPLACE_MILL, prompt, 0, t_player, deck_pos);

          replaced = replace_milled;
          replace_milled = old_replace_milled;
        }

        /* Part of the milling effect, not a separate trigger.  Unlike Helm of Obedience,
         * it happens even if the library-to-graveyard effect is replaced, so long
         * as the card is moved to a public zone. */
        if (source_csvid == CARD_ID_CELLAR_DOOR && (replaced != 2) &&
            is_what(-1, iid, TYPE_CREATURE))
          generate_token_by_id(player, card, CARD_ID_ZOMBIE);

        if (source_csvid == CARD_ID_DREAD_SUMMONS && (replaced != 2) &&
            is_what(-1, iid, TYPE_CREATURE))
        {
          token_generation_t token;
          default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
          token.action = TOKEN_ACTION_TAPPED;
          generate_token(&token);
        }

        if (replaced)
        {
          if (source_csvid == CARD_ID_HELM_OF_OBEDIENCE ||
              source_csvid == CARD_ID_GRENZO_DUNGEON_WARDEN)
            i--; // Here, i is counting number of cards put into the graveyard this way
          // Native Gyruda leaves this player's milling loop at the first replacement.
          if (source_csvid == CARD_ID_GYRUDA_DOOM_OF_DEPTHS)
          {
            if (local_num_cards_milled > 0)
              goto collect_gyruda;
            goto dispatch_milled;
          }
          continue;
        }

        gy_position_hint = raw_put_iid_on_top_of_graveyard_impl(
            t_player, iid, gy_position_hint, GENERATE_GRAVEYARD_SOURCE_VALUE(0, 255));
        remove_card_from_deck(t_player, deck_pos);

        local_milled[local_num_cards_milled].internal_card_id = iid;
        local_milled[local_num_cards_milled].position = gy_position_hint;
        local_milled[local_num_cards_milled].source = graveyard_source[t_player][gy_position_hint];

        ++local_num_cards_milled;

        // Cards that have "When this card is put into your graveyard from your library" triggers
        switch (csvid)
        {
        case CASES_GY_FROM_ANYWHERE_TRIGGER_SHUFFLE:
        case CASES_GY_FROM_ANYWHERE_TRIGGER_SHUFFLE_WHOLE_GY:
        case CARD_ID_GAEAS_BLESSING:
        case CARD_ID_NARCOMOEBA:
        case CARD_ID_CREEPING_CHILL:
          enable_xtrigger_flags |= ENABLE_XTRIGGER_MILLED;
          int leg = create_legacy_effect_from_iid(t_player, iid, effect_self_mill, -1, -1);
          card_instance_t *legacy = get_card_instance(t_player, leg);
          legacy->targets[0].player = gy_position_hint;
          legacy->targets[0].card = graveyard_source[t_player][gy_position_hint];
          legacy->eot_toughness =
              2; // Always show the second alternate text if the text has alternates
          break;
        }

        if (source_csvid == CARD_ID_HELM_OF_OBEDIENCE && (cards_data[iid].type & TYPE_CREATURE))
        {
          reanimate_permanent(player, card, t_player, gy_position_hint, REANIMATE_DEFAULT);
          kill_card(player, card, KILL_SACRIFICE);
          break;
        }

        if (source_csvid == CARD_ID_GRENZO_DUNGEON_WARDEN &&
            (cards_data[iid].type & TYPE_CREATURE) &&
            get_base_power_iid(t_player, iid) <= get_power(player, card))
        {
          reanimate_permanent(player, card, t_player, gy_position_hint, REANIMATE_DEFAULT);
        }
      }

      if (local_num_cards_milled <= 0)
        return;

      play_sound_effect(WAV_DISCARD); // Same as exe Millstone and other exe milling effects do,
                                      // though I'd prefer it was different from the discard sound

      if (source_csvid ==
          CARD_ID_JACES_MINDSEEKER) // Part of resolution, so happens before normal mill triggers
      {
        int iids[local_num_cards_milled], indices[local_num_cards_milled], num = 0;
        for (i = 0; i < local_num_cards_milled; ++i)
        {
          if (local_milled[i].position !=
              -1) // update positions, just in case a replcaement trigger (or, more likely,
                  // something misbehaving) somehow caused other cards in graveyard to shift
            local_milled[i].position = find_in_graveyard_by_source(
                1 - player, local_milled[i].source, local_milled[i].position);
          if (local_milled[i].position != -1) // still in graveyard?
          {
            iids[num] = local_milled[i].internal_card_id;
            indices[num] = i;
            ++num;
          }
        }

        if (num > 0)
        {
          test_definition_t test;
          new_default_test_definition(&test, TYPE_SPELL,
                                      "Select an instant or sorcery card to play.");

          int chosen =
              select_card_from_zone(player, 1 - player, iids, num, 0, AI_MAX_CMC, -1, &test);
          if (chosen != -1)
            play_card_in_grave_for_free(player, 1 - player, local_milled[indices[chosen]].position);
        }
      }

      if (source_csvid ==
          CARD_ID_STITCHER_GERALF) // Part of resolution, so happens before normal mill triggers
      {
        card_instance_t *instance = get_card_instance(player, card);
        if (instance->targets[1].player < 2)
        {
          while (instance->targets[1].player < 2)
          {
            int iids[local_num_cards_milled], indices[local_num_cards_milled], num = 0;
            for (i = 0; i < local_num_cards_milled; ++i)
            {
              if (local_milled[i].position !=
                  -1) // update positions, just in case a replacement trigger (or, more likely,
                      // something misbehaving) somehow caused other cards in graveyard to shift
                local_milled[i].position = find_in_graveyard_by_source(
                    t_player, local_milled[i].source, local_milled[i].position);
              if (local_milled[i].position != -1)
              { // still in graveyard?
                iids[num] = local_milled[i].internal_card_id;
                indices[num] = i;
                ++num;
              }
            }

            if (num > 0)
            {
              test_definition_t test;
              new_default_test_definition(&test, TYPE_CREATURE, "Select creature card to exile.");
              int chosen =
                  select_card_from_zone(player, t_player, iids, num, 0, AI_MAX_CMC, -1, &test);
              if (chosen != -1)
              {
                instance->info_slot += get_base_power_iid(t_player, iids[chosen]);
                rfg_card_from_grave(t_player, local_milled[indices[chosen]].position);
                instance->targets[1].player++;
              }
              else
              {
                break;
              }
            }
            else
            {
              break;
            }
          }
        }
      }

      if (source_csvid ==
          CARD_ID_HERETICS_PUNISHMENT) // Part of resolution, so happens before normal mill triggers
      {
        card_instance_t *instance = get_card_instance(player, card);
        int iids[local_num_cards_milled], num = 0;
        for (i = 0; i < local_num_cards_milled; ++i)
        {
          if (local_milled[i].position !=
              -1) // update positions, just in case a replacement trigger (or, more likely,
                  // something misbehaving) somehow caused other cards in graveyard to shift
            local_milled[i].position = find_in_graveyard_by_source(player, local_milled[i].source,
                                                                   local_milled[i].position);
          if (local_milled[i].position != -1) // still in graveyard?
          {
            iids[num] = local_milled[i].internal_card_id;
            ++num;
          }
        }

        if (num > 0)
        {
          int q;
          int max_cmc = 0;
          for (q = 0; q < num; q++)
          {
            if (get_cmc_by_internal_id(iids[q]) > max_cmc)
            {
              max_cmc = get_cmc_by_internal_id(iids[q]);
            }
          }
          instance->info_slot = max_cmc;
        }
      }

      if (source_csvid == CARD_ID_COMBUSTIBLE_GEARHULK) // Part of resolution, so happens before
                                                        // normal mill triggers
      {
        card_instance_t *instance = get_card_instance(player, card);
        int iids[local_num_cards_milled], num = 0;
        for (i = 0; i < local_num_cards_milled; ++i)
        {
          if (local_milled[i].position !=
              -1) // update positions, just in case a replacement trigger (or, more likely,
                  // something misbehaving) somehow caused other cards in graveyard to shift
            local_milled[i].position = find_in_graveyard_by_source(player, local_milled[i].source,
                                                                   local_milled[i].position);
          if (local_milled[i].position != -1) // still in graveyard?
          {
            iids[num] = local_milled[i].internal_card_id;
            ++num;
          }
        }

        if (num > 0)
        {
          int q;
          for (q = 0; q < num; q++)
          {
            instance->info_slot += get_cmc_by_internal_id(iids[q]);
          }
        }
      }

      if (source_csvid == CARD_ID_GORGING_VULTURE || source_csvid == CARD_ID_LOREHOLD_EXCAVATION ||
          source_csvid == CARD_ID_GRIST_THE_HUNGER_TIDE)
      {
        int iids[local_num_cards_milled], num = 0;
        for (i = 0; i < local_num_cards_milled; ++i)
        {
          if (local_milled[i].position != -1)
            local_milled[i].position = find_in_graveyard_by_source(player, local_milled[i].source,
                                                                   local_milled[i].position);
          if (local_milled[i].position != -1)
            iids[num++] = local_milled[i].internal_card_id;
        }
        if (num > 0)
        {
          if (source_csvid == CARD_ID_GORGING_VULTURE)
          {
            int lifegain = 0;
            for (i = 0; i < num; ++i)
              if (cards_data[iids[i]].type & TYPE_CREATURE)
                ++lifegain;
            gain_life(player, lifegain);
          }
          else if (source_csvid == CARD_ID_LOREHOLD_EXCAVATION)
          {
            int lifegain = 0, damage = 0;
            for (i = 0; i < num; ++i)
            {
              if (cards_data[iids[i]].type & TYPE_LAND)
                ++lifegain;
              else
                ++damage;
            }
            if (lifegain)
              gain_life(player, lifegain);
            if (damage)
              damage_player(1 - player, 1, player, card);
          }
          else
          {
            card_instance_t *inst = get_card_instance(player, card);
            clear_info(inst, CARD_ID_GRIST_THE_HUNGER_TIDE);
            for (i = 0; i < num; ++i)
              if (has_subtype_by_id(player, cards_data[iids[i]].id, SUBTYPE_INSECT))
                set_inst_info(inst, CARD_ID_GRIST_THE_HUNGER_TIDE, 0, 66);
          }
        }
      }
      if (source_csvid == CARD_ID_ANOWON_THE_RUIN_THIEF)
      {
        for (i = 0; i < local_num_cards_milled; ++i)
        {
          if (local_milled[i].position != -1)
            local_milled[i].position = find_in_graveyard_by_source(t_player, local_milled[i].source,
                                                                   local_milled[i].position);
          if (local_milled[i].position != -1 &&
              is_what(-1, local_milled[i].internal_card_id, TYPE_CREATURE))
          {
            draw_a_card(player);
            break;
          }
        }
      }
      if (source_csvid == CARD_ID_GYRUDA_DOOM_OF_DEPTHS)
      {
      collect_gyruda:
        for (i = 0; i < local_num_cards_milled; ++i)
        {
          if ((cards_data[local_milled[i].internal_card_id].type & TYPE_CREATURE) &&
              !(get_cmc_by_internal_id(local_milled[i].internal_card_id) & 1))
          {
            if (local_milled[i].position != -1)
            {
              local_milled[i].position = find_in_graveyard_by_source(
                  t_player, local_milled[i].source, local_milled[i].position);
              if (local_milled[i].position != -1)
              {
                gyruda_iids[0][gyruda_slots_filled] = local_milled[i].internal_card_id;
                gyruda_iids[1][gyruda_slots_filled] = 1;
                gyruda_iids[2][gyruda_slots_filled] = t_player;
                gyruda_iids[3][gyruda_slots_filled] = local_milled[i].position;
                gyruda_iids[4][gyruda_slots_filled++] = local_milled[i].source;
                continue;
              }
            }
            if (replaced)
            {
              local_milled[i].position =
                  find_iid_in_rfg(t_player, local_milled[i].internal_card_id);
              if (local_milled[i].position != -1)
              {
                gyruda_iids[0][gyruda_slots_filled] = local_milled[i].internal_card_id;
                gyruda_iids[1][gyruda_slots_filled] = 2;
                gyruda_iids[2][gyruda_slots_filled] = t_player;
                gyruda_iids[3][gyruda_slots_filled] = local_milled[i].position;
                gyruda_iids[4][gyruda_slots_filled++] = -1;
              }
            }
          }
        }
      }
    dispatch_milled:;

      if (enable_xtrigger_flags & ENABLE_XTRIGGER_MILLED)
      {
        milled_t *old_cards_milled = cards_milled;
        int old_num_cards_milled = num_cards_milled;
        cards_milled = local_milled;
        num_cards_milled = local_num_cards_milled;

        char prompt[100];
        if (ai_is_speculating == 1)
          prompt[0] = 0;
        else if (num_cards_milled == 1)
          scnprintf(prompt, 100, "Milled %s",
                    cards_ptr[cards_data[local_milled[0].internal_card_id].id]->full_name);
        else
          scnprintf(prompt, 100, "Milled %d cards", num_cards_milled);

        dispatch_xtrigger2(current_turn, XTRIGGER_MILLED, prompt, 0, t_player, 0);

        cards_milled = old_cards_milled;
        num_cards_milled = old_num_cards_milled;
      }
    }
  }
  if (gyruda_slots_filled)
  {
    test_definition_t test;
    new_default_test_definition(&test, TYPE_CREATURE, "Select a card to put onto the battlefield.");
    int selected = select_card_from_zone(player, player, gyruda_iids[0], gyruda_slots_filled, 1,
                                         AI_MAX_CMC, -1, &test);
    if (selected != -1)
    {
      if (gyruda_iids[1][selected] == 1)
      {
        int p = gyruda_iids[2][selected];
        int pos =
            find_in_graveyard_by_source(p, gyruda_iids[4][selected], gyruda_iids[3][selected]);
        if (pos >= 0)
          reanimate_permanent(player, card, p, pos, REANIMATE_NO_CONTROL_LEGACY);
      }
      else if (gyruda_iids[1][selected] == 2 &&
               find_iid_in_rfg(gyruda_iids[2][selected], gyruda_iids[0][selected]) >= 0)
      {
        int added = add_card_to_hand(player, gyruda_iids[0][selected]);
        if (gyruda_iids[2][selected] != player)
          add_state(player, added, STATE_OWNED_BY_OPPONENT);
        put_into_play(player, added);
      }
    }
  }
}

void mill(int t_player, int amount)
{
  special_mill(-1, -1, -1, t_player, amount);
}

int whenever_type_is_put_into_graveyard_from_anywhere(int player, int card, event_t event, int t_player, int type,
	test_definition_t *test )
{
	// from library
	enable_xtrigger_flags |= ENABLE_XTRIGGER_MILLED;
	if (xtrigger_condition() == XTRIGGER_MILLED && affect_me(player, card) && reason_for_trigger_controller == player
		&& trigger_cause_controller == t_player )
	{
		if( (event != EVENT_TRIGGER && event != EVENT_RESOLVE_TRIGGER) )
			return 0;

		int i, num_milled = 0;
		for (i = 0; i < num_cards_milled; ++i){
			if( (type && is_what(-1, cards_milled[i].internal_card_id, TYPE_LAND)) ||
				(test != NULL && new_make_test(player, cards_milled[i].internal_card_id, -1, test)) )
			{
				if (event == EVENT_TRIGGER)
				{
					event_result |= RESOLVE_TRIGGER_MANDATORY;
					return 0;
				}
				if (event == EVENT_RESOLVE_TRIGGER)
				++num_milled;
			}
		}
		return num_milled;
	}

	enable_xtrigger_flags |= ENABLE_XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY;
	if (xtrigger_condition() == XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY )
	{
		if( test == NULL ){
			test_definition_t test2;
			default_test_definition(&test2, type);
			if (when_a_card_is_put_into_graveyard_from_anywhere_but_library_do_something(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test2))
			{
				return 1;
			}
		}
		else{
			if (when_a_card_is_put_into_graveyard_from_anywhere_but_library_do_something(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, test))
			{
				return 1;
			}
		}
	}

	return 0;
}

/**************
* Reanimation *
**************/

int graveyard_has_shroud(int player)
{
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < active_cards_count[p]; ++c)
      if (in_play(p, c))
        {
          int id = get_id(p, c);
          if (id == CARD_ID_UNDERWORLD_CERBERUS || id == CARD_ID_GROUND_SEAL || id == CARD_ID_SILENT_GRAVESTONE)
            return 1;
        }
  return 0;
}

int good_to_put_in_grave(int player, int card){
	int int_card_id = player == -1 ? card : get_original_internal_card_id(player, card);
	int value = check_card_for_rules_engine(int_card_id);
	if( (value & REF_DREDGE) || (value & REF_UPKEEP_ABILITY_IN_GRAVE) || (value & REF_CONT_AND_TRIG_ABILITY_IN_GRAVE) ||
		(value & REF_BRIDGE_FROM_BELOW) || (value & REF_KROVIKAN_HORROR) || (value & REF_DISCARD_TRIGGER_MADNESS) ||
		(value & REF_VENGEFUL_PHARAOH)
	  ){
		return 1;
	}
	if( value & REF_ACTIVATED_ABILITIES_HAND_GRAVE ){
		if( cards_data[int_card_id].cc[2] != 4 ){
			return 1;
		}
	}
	return 0;
}

int seek_grave_for_id_to_reanimate(int player, int card, int t_player, int id,
                                   reanimate_mode_t mode)
{
  int count;
  int *graveyard;
  int grave_index;
  int hand_card;

  graveyard = graveyard_ptr_mutable[t_player];
  count = count_graveyard(t_player);
  grave_index = count + -1;
  if (grave_index < 0)
  {
    grave_index = -1;
  }
  else
  {
    do
    {
      if ((unsigned int)cards_data[graveyard[grave_index]].id == id)
      {
        if (REANIMATEXTRA_RETURN_TO_HAND2 < mode)
        {
          return reanimate_permanent(player, card, t_player, grave_index, mode);
        }
        if (mode == REANIMATEXTRA_RETURN_TO_HAND2)
        {
          hand_card = add_card_to_hand(t_player, graveyard[grave_index]);
          remove_card_from_grave(t_player, grave_index);
          return hand_card;
        }
        if (mode == REANIMATEXTRA_LEAVE_IN_GRAVEYARD)
        {
          return grave_index;
        }
        if (mode == REANIMATEXTRA_EXILE_FROM_GRAVEYARD)
        {
          rfg_card_from_grave(t_player, grave_index);
          return -1;
        }
        return -1;
      }
      grave_index = grave_index + -1;
    } while (grave_index != -1);
  }
  return grave_index;
}

/* Chooses a card in t_player's graveyard that matches this_test.  Sets {player,card}->targets[ret_location].player to its location in the graveyard and
 * {player,card}->targets[ret_location].card to its internal_card_id.  Returns the location, or -1 if not chosen.
 * If anything else removes a lower card in the graveyard before the selected card is acted upon, this will fail.
 * Prefer select_target_from_grave_source()/select_target_from_either_grave() and validate_target_from_grave_source() in new code. */
int new_select_target_from_grave(int player, int card, int t_player, select_grave_t mode,
                                 int ai_mode, test_definition_t *this_test, int ret_location)
{
  int *graveyard;
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  result = new_select_a_card(player, t_player,
                             (((mode & SFG_NOTARGET) == SFG_0) ? 2U : 5U),
                             mode & SFG_CANNOT_CANCEL, ai_mode, -1, this_test);
  if (result != -1)
  {
    graveyard = graveyard_ptr_mutable[t_player];
    instance->targets[ret_location].player = result;
    instance->targets[ret_location].card = graveyard[result];
  }
  return result;
}

/* Similar to new_select_target_from_grave(), but puts the graveyard_source[][] value in targets[ret_location].card instead of the internal_card_id, so the card
 * can be found even if it moves within the graveyard by the time it's validated.
 * Returns the internal_card_id, or -1 and sets cancel if not chosen. */
int select_target_from_grave_source(int player, int card, int t_player, select_grave_t mode,
                                         int ai_mode, test_definition_t *this_test,
                                         int ret_location)
{
  int result;
  card_instance_t *instance;

  result = new_select_a_card(
      player, t_player, (((mode & SFG_NOTARGET) == SFG_0) ? 2U : 5U),
      mode & SFG_CANNOT_CANCEL, ai_mode, -1, this_test);
  if (result == -1)
  {
    if ((mode & SFG_NO_SPELL_FIZZLED) == SFG_0)
    {
      spell_fizzled = 1;
    }
    return -1;
  }
  instance = get_card_instance(player, card);
  instance->targets[ret_location].player = result;
  instance->targets[ret_location].card = graveyard_source[t_player][result];
  return graveyard_ptr_mutable[t_player][result];
}

extern const char* hack_override_show_deck_done_button_label;
/* ManalinkEh 2022: 0x26b2a10; original functions/deck.c:147. */
void memcpy_inst_and_ext(card_instance_t *dest, const card_instance_t *src)
{
  card_ext_t *ext = dest->ext;
  memcpy(ext, src->ext, sizeof(*ext));
  memcpy(dest, src, sizeof(*dest));
  dest->ext = ext;
}

/* ManalinkEh 2022: 0x26b2e90; original functions/deck.c:345. */
int add_card_to_hand_invisible(int player, int iid)
{
  unsigned int saved_state;
  int card;
  card_instance_t *instance;

  card = add_card_to_hand(player, iid);
  if (-1 < card)
  {

    instance = get_card_instance(player, card);
    saved_state = instance->state;
    hand_count[player] = hand_count[player] + -1;
    instance->state = saved_state | 0x20;
  }
  return card;
}

/* ManalinkEh 2022: 0x26b33d0; original functions/deck.c:417. */
card_instance_t *on_stack(int player, int card)
{
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  if (((instance->state & (STATE_OUBLIETTED | STATE_INVISIBLE | STATE_IN_PLAY)) == 0x20) &&
      (-1 < instance->internal_card_id))
  {
    return instance;
  }
  return NULL;
}

/* ManalinkEh 2022: 0x26b3680; original functions/deck.c:516. */
void unmeld(int csvid, int *csvida, int *csvidb)
{
  switch (csvid)
  {
    case CARD_ID_CHITTERING_HOST:
      *csvida = CARD_ID_GRAF_RATS;
      *csvidb = CARD_ID_MIDNIGHT_SCAVENGERS;
      return;
    case CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP:
      *csvida = CARD_ID_HANWEIR_BATTLEMENTS;
      *csvidb = CARD_ID_HANWEIR_GARRISON;
      return;
    case CARD_ID_BRISELA_VOICE_OF_NIGHTMARES:
      *csvida = CARD_ID_GISELA_THE_BROKEN_BLADE;
      *csvidb = CARD_ID_BRUNA_THE_FADING_LIGHT;
      return;
    default:
      *csvida = csvid;
      *csvidb = -1;
  }
}

/* ManalinkEh 2022: 0x26b36e0; original functions/deck.c:542. */
int set_inst_info(card_instance_t *inst, int cid, uint32_t data, uint16_t data2)
{
  inst_info_t *entries = inst->ext->inst_info_data;
  int slot;
  for (slot = 0; slot < 6; ++slot)
    if ((uint32_t)entries[slot].id == (uint32_t)cid)
      break;
  if (slot == 6)
  {
    for (slot = 0; slot < 6; ++slot)
      if (entries[slot].id == 0)
        break;
    if (slot == 6)
      return 0;
  }
  entries[slot].id = (uint16_t)cid;
  entries[slot].data2 = data2;
  entries[slot].data = data;
  return 1;
}

// Returns all cards matching test in player's graveyard to his hand.  Returns the number of matching cards.
/* ManalinkEh 2022: 0x26b3740; original functions/deck.c:584. */
inst_info_t inst_info(card_instance_t *inst, int cid)
{
  inst_info_t *entries = inst->ext->inst_info_data;
  int slot;
  for (slot = 0; slot < 6; ++slot)
  {
    if ((uint32_t)entries[slot].id == (uint32_t)cid)
      return entries[slot];
    if (entries[slot].id == 0)
      break;
  }
  if (slot == 6)
    slot = 5;
  entries[slot].id = (uint16_t)cid;
  entries[slot].data2 = 0;
  entries[slot].data = 0;
  return entries[slot];
}

/* ManalinkEh 2022: 0x26b37f0; original functions/deck.c:603. */
int increment_data(card_instance_t *inst, int cid)
{
  inst_info_t initial = inst_info(inst, cid);
  inst_info_t second = inst_info(inst, cid);
  return set_inst_info(inst, cid, initial.data + 1u, second.data2);
}

/* ManalinkEh 2022: 0x26b3870; original functions/deck.c:608. */
int increment_data2(card_instance_t *inst, int cid)
{
  inst_info_t initial = inst_info(inst, cid);
  inst_info_t second = inst_info(inst, cid);
  return set_inst_info(inst, cid, initial.data, (uint16_t)(second.data2 + 1));
}

/* ManalinkEh 2022: 0x26b38f0; original functions/deck.c:615. */
void clear_info(card_instance_t *inst, int cid)
{
  inst_info_t *entries = inst->ext->inst_info_data;
  for (int slot = 0; slot < 6; ++slot)
  {
    if (entries[slot].id == 0)
      return;
    while ((uint32_t)entries[slot].id == (uint32_t)cid)
    {
      if (slot < 5)
        memmove(&entries[slot], &entries[slot + 1], (5 - slot) * sizeof(*entries));
      entries[5].id = 0;
    }
  }
}

/* ManalinkEh 2022: 0x26b3990; original functions/deck.c:625. */
int store_iid(card_instance_t *inst, int16_t generating_id, int16_t stored_iid)
{
  inst_stored_iid_t *entries = inst->ext->inst_stored_iids;
  for (int slot = 0; slot < 12; ++slot)
    if (entries[slot].generating_id == -1)
    {
      entries[slot].generating_id = generating_id;
      entries[slot].stored_iid = stored_iid;
      return slot;
    }
  return -1;
}

/* ManalinkEh 2022: 0x26b39d0; original functions/deck.c:638. */
int retrieve_stored_iids(card_instance_t *inst, int16_t generating_id, int16_t *iids)
{
  int count = 0;
  inst_stored_iid_t *entries = inst->ext->inst_stored_iids;
  for (int slot = 0; slot < 12; ++slot)
    if (entries[slot].generating_id == generating_id)
      iids[count++] = entries[slot].stored_iid;
  return count;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
void unmeld(int csvid, int *csvida, int *csvidb);

/* ManalinkEh 2022: 0x26b3a10; original functions/deck.c:650. */
void clear_stored_iids(card_instance_t *inst, int16_t generating_id)
{
  inst_stored_iid_t *entries = inst->ext->inst_stored_iids;
  for (int slot = 0; slot < 12; ++slot)
    if (entries[slot].generating_id == generating_id)
    {
      entries[slot].generating_id = -1;
      entries[slot].stored_iid = -1;
    }
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
void kill_target0(int player, int card, kill_t kill_mode);

/* ManalinkEh 2022: 0x26b5a30; original functions/deck.c:1544. */
void kill_target0(int player, int card, kill_t kill_mode)
{
  int target_card;
  int target_player;
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  target_card = instance->targets[0].card;
  target_player = instance->targets[0].player;

  if ((-1 < target_player) && (-1 < target_card))
  {
    kill_card_impl(target_player, target_card, kill_mode, 0);
    return;
  }
  return;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int count_iid_in_rfg(int player, int iid);

/* ManalinkEh 2022: 0x26b5e90; original functions/deck.c:1615. */
int pre_counterspell(int player, int card, int to_counter_player, int to_counter_card)
{
  int owner;
  unsigned char uncounterable_colors;
  card_instance_t *spell;
  card_instance_t *live_spell_or_commander;
  int spell_iid_or_creature;
  unsigned int counterspell_color;
  int card_00;
  int choice_or_result;

  spell = get_card_instance(to_counter_player, to_counter_card);
  spell_iid_or_creature = spell->internal_card_id;
  if ((((spell_iid_or_creature == -1) ||
        (live_spell_or_commander = get_card_instance(to_counter_player, to_counter_card),
         (live_spell_or_commander->state & (STATE_OUBLIETTED | STATE_INVISIBLE | STATE_IN_PLAY)) !=
             0x20)) ||
       (live_spell_or_commander->internal_card_id < 0)) ||
      ((spell->token_status & 0x200) != 0))
  {
    return 0;
  }
  uncounterable_colors = your_spells_cant_be_countered_by_color[to_counter_player];
  if (uncounterable_colors != 0)
  {
    counterspell_color = get_color(player, card);
    if (counterspell_color == 0)
    {
      counterspell_color = 1;
    }
    if (((counterspell_color & uncounterable_colors) != 0) &&
        (choice_or_result = is_being_cast(to_counter_player, to_counter_card),
         choice_or_result != 0))
    {
      return 0;
    }
  }
  if (((((uint8_t)other_game_mode & OGM_EDH) != OGM_NONE) &&
       (choice_or_result = check_special_flags2(to_counter_player, to_counter_card, 0x1000000),
        choice_or_result != 0)) &&
      (choice_or_result = do_dialog(to_counter_player, to_counter_player, to_counter_card, -1, -1,
                                    " Return Commander to commander\'s zone\n Decline", 0),
       choice_or_result == 0))
  {
    owner = get_owner(to_counter_player, to_counter_card);
    card_00 = locate_edh_card(owner);
    if (-1 < card_00)
    {
      live_spell_or_commander = get_card_instance(owner, card_00);
      if (live_spell_or_commander->targets[2].player == spell->original_internal_card_id)
      {
        *(uint8_t *)&live_spell_or_commander->targets[3].player = 1;
      }
      else if (live_spell_or_commander->targets[2].card == spell->original_internal_card_id)
      {
        SET_BYTE1(live_spell_or_commander->targets[3].player) = 1;
      }
      if ((-1 < to_counter_player) && (-1 < to_counter_card))
        kill_card_impl(to_counter_player, to_counter_card, KILL_EXILE, 0);
      return -1;
    }
  }
  if (((card_flags[player] & CFLAGS_GUILE) != 0) && ((cards_data[spell_iid_or_creature].type & 1) == 0))
  {
    if ((-1 < to_counter_player) && (-1 < to_counter_card))
    {
      kill_card_impl(to_counter_player, to_counter_card, KILL_EXILE, 0);
    }
    if (can_legally_play_iid(player, spell_iid_or_creature) == 0)
    {
      return -1;
    }
    choice_or_result =
        dialog_impl(player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_RANDOM, DLG_NO_CANCEL, DLG_FULLCARD_ID(
                    spell_iid_or_creature), "Play this card", 1, 10, "Decline", 1, 1, 0, 0, 0, 0);
    if (choice_or_result != 1)
    {
      return -1;
    }
    play_card_in_exile_for_free(player, to_counter_player,
                                (unsigned int)cards_data[spell_iid_or_creature].id);
    return -1;
  }
  dispatch_event_with_attacker(to_counter_player, to_counter_card, EVENT_COUNTERSPELL, player,
                               card);
  if (is_what(player, card, 0x20) == 0)
  {
    play_sound_effect(WAV_INTERUPT);
  }
  spell_iid_or_creature = is_what(to_counter_player, to_counter_card, 2);
  if ((to_counter_player != player) && (spell_iid_or_creature != 0))
  {
    increase_trap_condition(to_counter_player, 0x608, 1);
  }
  if (check_special_flags(to_counter_player, to_counter_card, 0x100000) == 0)
  {
    return 1;
  }
  if ((-1 < to_counter_player) && (-1 < to_counter_card))
  {
    kill_card_impl(to_counter_player, to_counter_card, KILL_EXILE, 0);
  }
  return -1;
}

/* ManalinkEh 2022: 0x26b7650; original functions/deck.c:2108. */
int count_iid_in_rfg(int player, int iid)
{
  int *exile;
  int exile_index;
  int matches;

  exile = rfg_ptr[player];
  exile_index = count_rfg(player);
  matches = 0;
  if (-1 < exile_index)
  {
    do
    {
      if (exile[exile_index] == iid)
      {
        matches = matches + 1;
      }
      exile_index = exile_index + -1;
    } while (exile_index != -1);
  }
  return matches;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
card_instance_t *on_stack(int player, int card);

/* Prompts for a graveyard (if both have a targetable card), then prompts for a card.  Returns -1 and sets cancel if cancelled or there's no targets.
 * Otherwise identical to select_target_from_grave_source(). */
int select_target_from_either_grave(int player, int card, select_grave_t mode,
                                    int ai_mode_own_graveyard, int ai_mode_opponent_graveyard,
                                    test_definition_t *test, int ret_location_graveyard,
                                    int ret_location_card)
{
  int human_grave_count;
  int ai_grave_count;
  int selected_card_or_ai_mode;
  card_instance_t *instance;
  bool human_grave_available;
  bool ai_grave_available;
  select_grave_t card_selection_mode;
  target_t tgt_gy;
  target_definition_t td;

  human_grave_count = new_special_count_grave(0, test);
  ai_grave_count = new_special_count_grave(1, test);
  human_grave_available = human_grave_count != 0;
  ai_grave_available = ai_grave_count != 0;
  card_selection_mode = mode;
  if (human_grave_available && ai_grave_available)
  {
    card_selection_mode = mode & ~SFG_CANNOT_CANCEL;
  }
  while (true)
  {
    if (human_grave_available && ai_grave_available)
    {
      base_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      td.allow_cancel = (mode ^ SFG_CANNOT_CANCEL) & SFG_CANNOT_CANCEL;
      load_text(NULL, "TARGET_GRAVEYARD");
      if (select_target(player, card + -1000, &td, text_lines[0], &tgt_gy) == 0)
      {
        spell_fizzled = 1;
        return -1;
      }
      hack_override_show_deck_done_button_label = "Rechoose GY";
    }
    else
    {
      if (human_grave_count == 0 && ai_grave_count == 0)
      {
        spell_fizzled = 1;
        return -1;
      }
      tgt_gy.player = (int32_t)(human_grave_count == 0);
    }
    selected_card_or_ai_mode = ai_mode_opponent_graveyard;
    if (player == tgt_gy.player)
    {
      selected_card_or_ai_mode = ai_mode_own_graveyard;
    }
    selected_card_or_ai_mode =
        select_target_from_grave_source(player, card, tgt_gy.player, card_selection_mode,
                                        selected_card_or_ai_mode, test, ret_location_card);
    hack_override_show_deck_done_button_label = NULL;
    if (selected_card_or_ai_mode != -1)
    {
      instance = get_card_instance(player, card);
      if (-1 < ret_location_graveyard)
      {
        instance->targets[ret_location_graveyard].player = tgt_gy.player;
        instance->targets[ret_location_graveyard].card = -1;
        return selected_card_or_ai_mode;
      }
      return selected_card_or_ai_mode;
    }
    if (!human_grave_available || !ai_grave_available)
    {
      spell_fizzled = 1;
      hack_override_show_deck_done_button_label = NULL;
      return -1;
    }
    if (ai_is_speculating == 1)
      break;
    if ((player == 1) && (((unsigned char)trace_mode & 2) == 0))
    {
      spell_fizzled = 1;
      hack_override_show_deck_done_button_label = NULL;
      return -1;
    }
    spell_fizzled = 0;
  }
  spell_fizzled = 1;
  hack_override_show_deck_done_button_label = NULL;
  return -1;
}

/* Checks to see if get_grave(whose_graveyard)[{player,card}->targets[target_location].player] is still {player,card}->targets[target_location].card.
 * This will fail if anything else removes a lower card in the graveyard in response, even if the selected card is still there.
 * Prefer select_target_from_grave_source()/validate_target_from_grave_source() in new code. */
int validate_target_from_grave(int player, int card, int t_player, int targ_location){
	if (graveyard_has_shroud(t_player)){
		return -1;
	}
	card_instance_t *instance = get_card_instance(player, card);
	const int *grave = get_grave(t_player);
	int selected = instance->targets[targ_location].player;
	if (selected >= 0 && grave[selected] == instance->targets[targ_location].card){
		return selected;
	}
	return -1;
}

/* Similar to validate_target_from_grave(), but uses the values set in select_target_from_grave_source()/select_target_from_either_grave() to find the selected
 * card even if it's moved within its graveyard.  Returns the position within whose_graveyard's graveyard if the card is still there, else -1. */
int validate_target_from_grave_source(int player, int card, int whose_graveyard,
                                      int target_location)
{
  if (graveyard_has_shroud(whose_graveyard))
    return -1;

  card_instance_t *instance = get_card_instance(player, card);

  if (instance->targets[target_location].card == -1)
    return -1;

  int pos = find_in_graveyard_by_source(whose_graveyard, instance->targets[target_location].card,
                                        (uint16_t)instance->targets[target_location].player);

  if (pos == -1)
    return -1;

  instance->targets[target_location].player =
      (whose_graveyard << 16) | pos; // update position in targets, just in case something looks for
                                     // it there instead of checking return value

  return pos;
}

/* Runs triggers specifically looking for a card moving from graveyard to hand, like Golgari Brownscale.  Avoid calling directly; use from_grave_to_hand(),
 * from_grave_to_hand_multiple(), or new_global_tutor() instead. */
void from_grave_to_hand_triggers(int player, int card_added)
{
  // Anything in here should also be inlined in from_grave_to_hand() and from_grave_to_hand_multiple() below.
  switch (get_id(player, card_added))
	{
	  case CARD_ID_GOLGARI_BROWNSCALE:
		gain_life(player, 2);
		break;
	}
}

void from_grave_to_hand(int player, int grave_position, int unused)
{
  int iid = get_grave(player)[grave_position];

  add_card_to_hand(player, iid);
  remove_card_from_grave(player, grave_position);

  switch (cards_data[iid].id)
	{
	  case CARD_ID_GOLGARI_BROWNSCALE:
		gain_life(player, 2);
		break;
	}
}

int from_grave_to_hand_multiple(int player, test_definition_t* test)
{
  /* First, remove all matching cards from graveyard, then put them in hand one by one and return them to hand, in order to avoid any horrid chains of triggers
   * that interfere with the graveyard (e.g. Golgari Brownscale returns to hand from graveyard => gain 2 life => Searing Meditation => damage Moldgraf
   * Monstrosity => returns creature cards from graveyard to battlefield).
   *
   * Since I'm only aware of the one card that triggers when moving from graveyard to hand, and it doesn't use a real engine-level trigger anyway, we could just
   * put the card in hand as we loop through the graveyard and store the triggers to all run at the end, but moving it to two steps (remove all from gy, then
   * add all to hand) will be more maintainable.  We do store up the triggers to run at the end anyway, though. */

  int iids[500];
  int i, j, num_iids = 0;

  int* grave = graveyard_ptr_mutable[player];
  int* source = graveyard_source[player];
  for (i = j = 0; i < 500 && grave[i] != -1; ++i)
	if (new_make_test(player, grave[i], -1, test))
	  iids[num_iids++] = grave[i];
	else
	  {
		grave[j] = grave[i];
		source[j] = source[i];
		++j;
	  }

  for (; j < 500; ++j)
	grave[j] = source[j] = -1;

  // Now, one by one, put each card in player's hand, storing up triggers to run.
  int num_golgari_brownscale = 0;
  for (i = 0; i < num_iids; ++i)
	{
	  add_card_to_hand(player, iids[i]);

	  switch (cards_data[iids[i]].id)
		{
		  case CARD_ID_GOLGARI_BROWNSCALE:
			++num_golgari_brownscale;
			break;
		}
	}

  for (i = 0; i < num_golgari_brownscale; ++i)
	gain_life(player, 2);

  return num_iids;
}

/* Given a (player,card) pair of a card that has left the battlefield, searches its owner's graveyard for the underlying card.
 * If found, return nonzero and set *owner and *position. */
int find_in_owners_graveyard(int player, int card, int* owner, int* position)
{
  *owner = get_owner(player, card);

  card_instance_t* instance = get_card_instance(player, card);
  if (instance->internal_card_id == activation_card)
	{
	  player = instance->parent_controller;
	  card = instance->parent_card;
	}
  else if (instance->internal_card_id == LEGACY_EFFECT_CUSTOM
		   || instance->internal_card_id == LEGACY_EFFECT_ACTIVATED)
	{
	  player = instance->damage_source_player;
	  card = instance->damage_source_card;
	}

  if (is_token(player, card))
	{
	  *position = -1;
	  return 0;
	}

  int source = ((*owner & 1) << 8) | (card & 0xFF);
  int i;
  /* Unused provenance slots can contain zero or stale source values.  Only
   * live graveyard entries identify cards; source zero is itself valid. */
  for (i = count_graveyard(*owner) - 1; i >= 0; --i)
	if ((graveyard_source[*owner][i] & 0x1FF) == source)
	  {
		*position = i;
		return 1;
	  }

  *position = -1;
  return 0;
}

/* Given a (player,card) pair of a card that has left the battlefield, searches its owner's graveyard for the underlying card.
 * If found, exile it and return nonzero. */
int exile_from_owners_graveyard(int player, int card)
{
  int owner, position;
  if (find_in_owners_graveyard(player, card, &owner, &position))
	{
	  rfg_card_from_grave(owner, position);
	  return 1;
	}
  else
	return 0;
}

/* Finds the card in player's graveyard whose graveyard_source[][] value is source, and returns its position or -1 if not found.  If position_hint is given,
 * check there first. */
int find_in_graveyard_by_source(int player, int source, int position_hint)
{
  if (position_hint >= 0)
	{
	  // Check at position_hint
	  if (graveyard_source[player][position_hint] == source)
		return position_hint;

	  // Check position immediately before, in case a single card lower in graveyard has been removed
	  if (position_hint >= 1 && graveyard_source[player][position_hint - 1] == source)
		return position_hint - 1;

	  // Otherwise, search whole graveyard
	}

  int i;
  for (i = 0; i < 500; ++i)
	if (graveyard_source[player][i] == source)
	  return i;
	else if (graveyard_source[player][i] == -1)
	  break;

  return -1;
}

void whenever_a_card_is_put_into_a_graveyard_from_anywhere_exile_that_card(int player, int card, event_t event, int t_player, int type, test_definition_t *this_test){

	// From library
	enable_xtrigger_flags |= ENABLE_XTRIGGER_MILLED;
	if (xtrigger_condition() == XTRIGGER_MILLED && affect_me(player, card) && reason_for_trigger_controller == player
	  && !is_humiliated(player, card, AL_TRIG) && (trigger_cause_controller == t_player || t_player == ANYBODY)
	  ){
		if (event == EVENT_TRIGGER)
			event_result |= RESOLVE_TRIGGER_MANDATORY;

		if (event == EVENT_RESOLVE_TRIGGER){
			int i, p = trigger_cause_controller;
			for (i = 0; i < num_cards_milled; ++i)
				if (event == EVENT_RESOLVE_TRIGGER && cards_milled[i].position != -1){
					if( cards_milled[i].source != -1 &&
						((type > 0 && is_what(-1, cards_milled[i].internal_card_id, type)) ||
						(this_test != NULL && new_make_test(p, cards_milled[i].internal_card_id, -1, this_test)))
					  ){
						int pos = find_in_graveyard_by_source(p, cards_milled[i].source, cards_milled[i].position);
						if (pos != -1){
							rfg_card_from_grave(p, pos);
							cards_milled[i].position = -1;	// No longer in graveyard, so keep any other triggers from looking
						}
					}
				}
			}
	}

	if (when_a_card_is_put_into_graveyard_from_anywhere_but_library_do_something(player, card, event, t_player, RESOLVE_TRIGGER_MANDATORY, NULL)
	  && gy_from_anywhere_pos != -1
	 ){
		int p = trigger_cause_controller;
		if( gy_from_anywhere_source != -1 &&
			((type > 0 && is_what(-1, gy_from_anywhere_source, type)) ||
			(this_test != NULL && new_make_test(p, gy_from_anywhere_source, -1, this_test)))
		  ){
			int pos = find_in_graveyard_by_source(p, gy_from_anywhere_source, gy_from_anywhere_pos);
			if (pos != -1){	// already removed, but hadn't been recorded
				rfg_card_from_grave(p, pos);
				gy_from_anywhere_pos = -1;	// No longer in graveyard, so keep any other triggers from looking
			}
		}
	}
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int pre_counterspell(int player, int card, int to_counter_player, int to_counter_card);

int count_types_in_grave(int player)
{
  int *next_grave_card;
  unsigned int type_count;
  type_t card_types;
  int result;
  type_t v;
  int *grave_cursor;
  int grave_owner;

  grave_owner = 0;
  type_count = 0;
  v = TYPE_NONE;
  do
  {
    if ((player == grave_owner) || (player == 2))
    {
      grave_cursor = graveyard_ptr_mutable[grave_owner];
      result = *grave_cursor;
      while (result != -1)
      {
        card_types = get_type(-1, result);
        v = card_types & (TARGET_TYPE_PLANESWALKER | TYPE_ARTIFACT | TYPE_INSTANT | TYPE_SORCERY |
                          TYPE_ENCHANTMENT | TYPE_CREATURE | TYPE_LAND) |
            v;
        result =
            has_subtype_by_id(player, (unsigned int)cards_data[*grave_cursor].id, SUBTYPE_TRIBAL);
        if (result != 0)
        {
          v = v | TARGET_TYPE_TOKEN;
        }
        next_grave_card = grave_cursor + 1;
        grave_cursor = grave_cursor + 1;
        result = *next_grave_card;
      }
      type_count = num_bits_set(v);
      if (7 < (int)type_count)
      {
        return type_count;
      }
    }
    grave_owner = grave_owner + 1;
    if (grave_owner == 2)
    {
      return type_count;
    }
  } while (true);
}

#ifndef SHANDALAR
/* Recovered October 2022 typed consumers and compiler parts. */

/* ManalinkEh 2022: 0x26b9850; original functions/deck.c:2949. */

/* ManalinkEh 2022: 0x26b98e0; original functions/deck.c:2966. */

/* ManalinkEh 2022: 0x26b99a0; original functions/deck.c:2991. */

/* ManalinkEh 2022: 0x26b6500; original functions/deck.c:1688. */

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26b9770; original functions/deck.c:2918. */

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26b3560; original functions/deck.c:479. */

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline mechanics; native callers retain their expansions. */
static inst_info_t info_outofline(card_instance_t *inst, int cid)
{
  inst_info_t *entries = inst->ext->inst_info_data;
  int slot;
  for (slot = 0; slot < 6; ++slot)
  {
    if ((uint32_t)entries[slot].id == (uint32_t)cid)
      return entries[slot];
    if (entries[slot].id == 0)
      break;
  }
  if (slot == 6)
    slot = 5;
  entries[slot].id = (uint16_t)cid;
  entries[slot].data2 = 0;
  entries[slot].data = 0;
  return entries[slot];
}
#endif

/* SDL host helper retained during the 2022 migration. */
void real_put_on_top_of_deck(int player, int card)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (instance->internal_card_id < 0)
    return;

  int owner = get_owner(player, card);
  int iid = instance->original_internal_card_id;
  int insert = !(instance->token_status & (STATUS_TOKEN | STATUS_OBLITERATED));
  if (in_play(player, card))
    dispatch_trigger2(current_turn, TRIGGER_LEAVE_PLAY, "Card leaving play", 0, player, card);

  instance->internal_card_id = -1;
  destroy_attached_auras_and_obliterate_card(player, card);
  if (insert)
    raw_put_iid_on_top_of_deck(owner, iid);
  call_sub_437E20_unless_ai_is_speculating();
}
