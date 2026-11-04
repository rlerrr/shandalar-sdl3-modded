// Interactive searching and manipulation of libraries/graveyards/exile.

#include "manalink.h"
#include "magic/src/duel_host.h"

/* Canonical recovered helper declarations. */
int select_multiple_cards_from_graveyard_to_array(int player, int targ_player,
                                                       int must_select_all, int ai_selection_mode,
                                                       test_definition_t *this_test,
                                                       int max_targets, target_t *ret_location);
int new_select_a_card(int player, int targ_player, int search_location, int must_select,
                           int ai_selection_mode, int test_type, test_definition_t *this_test);
int select_a_card(int player, int targ_player, int search_location, int must_select,
                       int ai_selection_mode, int test_type, int type, int flag1, int subtype,
                       int flag2, int color, int flag3, int id, int flag4, int cc, int flag5);
int show_deck(int player, const int *deck, int num_cards, const char *prompt,
              int suppress_done_label, const char *done_label);
int get_tutoring_denial(int player);
int select_multiple_cards_from_graveyard(int player, int targ_player, int must_select_all,
                                              int ai_selection_mode,
                                              test_definition_t *this_test, int max_targets,
                                              target_t *ret_location);
int new_global_tutor(int player, int targ_player, int search_location, int destination,
                          int must_select, int ai_selection_mode, test_definition_t *this_test);
int global_tutor(int player, int targ_player, int search_location, int destination,
                      int must_select, int ai_selection_mode, int type, int flag1, int subtype,
                      int flag2, int color, int flag3, int id, int flag4, int cc, int flag5);
int reveal_top_cards_of_library(int player, int num_cards);
int get_average_value_from_zone(int targ_player, const int *s_location, int zone_count,
                                     test_definition_t *this_test, int my_test, int mode);
int select_card_from_zone(int player, int targ_player, const int *s_location, int zone_count,
                               int must_select, int ai_selection_mode, int test_type,
                               test_definition_t *this_test);
int reveal_top_cards_of_library_and_choose(int src_player, int src_card, int t_player,
                                                int num_cards, int must_select,
                                                tutor_t destination_chosen,
                                                int reveal_chosen,
                                                tutor_t destination_rest,
                                                int reveal_rest, test_definition_t *test);
int reveal_top_cards_of_library_and_choose_type(int src_player, int src_card, int t_player,
                                                     int num_cards, int must_select,
                                                     tutor_t destination_chosen,
                                                     int reveal_chosen,
                                                     tutor_t destination_rest,
                                                     int reveal_rest, type_t legal_types);

int show_deck_exe(int player, const int* deck, int num_cards, const char* prompt, int suppress_done_label, /* const char* */int done_label);

const char* hack_override_show_deck_done_button_label = NULL;

/* ManalinkEh 2022: 0x2650b50; original functions/tutors.c:313. */
static int effect_leonin_arbiter(int player, int card, event_t event)
{
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

// [player] reveals the top [num_cards] of his library.

/* [reveal_rest or Look at] the top [num_cards] of your library.  Choose a [test] card from among them, [reveal_chosen], and move it to [destination_chosen].
 * Move the rest to [destination_rest]. */

// Identical to reveal_top_cards_of_library_and_choose(), but specify a type bitfield instead of a full test_definition_t.  Constructs an appropriate prompt.

int tutor_from_grave_to_hand_except_for_dying_card(int player, int card, int must_select, int ai_selection_mode, test_definition_t* test)
{
  /* Find card in graveyard and make it unselectable without disturbing its placement.
   * If not there - maybe it was stolen or a token or has already been removed by something else - this won't do anything.
   *
   * It's conceivable that there's some sort of effect somewhere that can trigger during the new_global_tutor() call to shuffle graveyard into library or do
   * some other shenanigans that will move the iid_draw_a_card elsewhere ("Whenever a player has more than 4 cards in his hand, he shuffles his graveyard into
   * his library"?  "While Yixlid Jailer Variant is in a graveyard, all other cards in all graveyards lose all abilities" + an Eldrazi?), but I doubt it.
   *
   * It would be much more of a concern if the chosen card was put on the battlefield or played for free; either of those could very easily cause a reshuffling.
   * (Consider reanimating Barishi with a Pandemonium in play, or Timetwister.)  If such a case were needed, it would need support within new_global_tutor to
   * change the iid_draw_a_card's back into their original iids before removing the selected card. */

  if (graveyard_has_shroud(player))
	return 0;

  int owner, position, iid = -1;
  if (find_in_owners_graveyard(player, card, &owner, &position)
	  && owner == player)	// If it's in the other player's graveyard, then don't even bother - we aren't searching that one
	iid = turn_card_in_grave_face_down(player, position);

  int rval = -1;
  if (new_special_count_grave(player, test))
	rval = new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, must_select, ai_selection_mode, test);

  // Restore the unselectable card, if indeed there was one.  Have to search, since its position may have changed.
  if (iid != -1)
	for (position = 0; get_grave(player)[position] != -1; ++position)
	  if (get_grave(player)[position] == iid_draw_a_card)
		{
		  turn_card_in_grave_face_up(player, position, iid);
		  break;
		}

  return rval;
}

int select_one_and_mill_the_rest(int player, int t_player, int number, int type){

	int card_added = -1;
	int *deck = deck_ptr[t_player];
	if( number > count_deck(t_player) ){
		 number = count_deck(t_player);
	}
	if( number > 0 ){
		int selected = -1;
		while( selected == -1 ){
				selected = show_deck( player, deck, number, "Pick a card", 0, gs_done_008b40e0 );
		}
		if( is_what(-1, deck[selected], type) ){
			card_added = add_card_to_hand(t_player, deck[selected]);
			remove_card_from_deck(t_player, selected);
			number--;
		}
		mill(player, number);
	}

	return card_added;
}

int select_one_and_put_the_rest_on_bottom(int player, int t_player, int number){

	int selected = -1;
	int *deck = deck_ptr[t_player];
	while( selected == -1 ){
			selected = show_deck( player, deck, number, "Pick a card", 0, gs_done_008b40e0 );
	}
	int card_added = add_card_to_hand(t_player, deck[selected]);
	remove_card_from_deck(t_player, selected);
	put_top_x_on_bottom(player, t_player, number-1);

	return card_added;
}

int reveal_x_and_choose_a_card_type(int player, int card, int to_reveal, int selected_type)
{

  if (count_deck(player) < to_reveal)
  {
    to_reveal = count_deck(player);
  }

  test_definition_t this_test;
  char buffer[100];
  int pos = scnprintf(buffer, 100, " Select a");
  if (selected_type == TYPE_CREATURE)
  {
    pos += scnprintf(buffer + pos, 100 - pos, " creature");
  }
  if (selected_type == TYPE_LAND)
  {
    pos += scnprintf(buffer + pos, 100 - pos, " land");
  }
  if (selected_type == TYPE_ARTIFACT)
  {
    pos += scnprintf(buffer + pos, 100 - pos, "n artifact");
  }
  if (selected_type == TYPE_ENCHANTMENT)
  {
    pos += scnprintf(buffer + pos, 100 - pos, " enchantment");
  }
  pos += scnprintf(buffer + pos, 100 - pos, " card.");
  new_default_test_definition(&this_test, selected_type, buffer);
  this_test.create_minideck = to_reveal;
  this_test.no_shuffle = 1;

  int card_added = -1;

  if (to_reveal > 0)
  {
    card_added =
        new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
    if (card_added >= 0)
    {
      to_reveal--;
    }
    if (to_reveal > 0)
    {
      put_top_x_on_bottom(player, player, to_reveal);
    }
  }

  return card_added;
}

void tutor_lands(int player, tutor_t destination, int num){
	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Choose a land card.");
	test.qty = num;
	new_global_tutor(player, player, TUTOR_FROM_DECK, destination, 0, AI_MAX_VALUE, &test);
}

void tutor_basic_land(int player, int put_in_play, int tap_it){

	int mode = TUTOR_HAND;
	if( put_in_play == 1 ){
		mode = TUTOR_PLAY;
	}
	if( tap_it == 1 ){
		mode = TUTOR_PLAY_TAPPED;
	}
	tutor_basic_lands(player, mode, 1);
}

// Tutor two basic lands, one onto bf tapped, one into hand, then shuffle
void tutor_basic_lands(int player, tutor_t destination, int num){
	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Choose a basic land card.");
	test.subtype = SUBTYPE_BASIC;
	test.subtype_flag = MATCH;
	test.qty = num;
	test.no_shuffle = 1;
	new_global_tutor(player, player, TUTOR_FROM_DECK, destination, 0, AI_MAX_VALUE, &test);
	shuffle(player);
}

// Simple n^2 insertion sort on array[].player, descending.  Poorly suited if number is greater than about 30.
void cultivate(int player)
{
  test_definition_t test;
  new_default_test_definition(&test, TYPE_LAND,
                              "Choose a basic land card to put on the battlefield tapped.");
  test.subtype = SUBTYPE_BASIC;
  test.subtype_flag = MATCH;
  test.no_shuffle = 1;

  if (new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY_TAPPED, 0, AI_MAX_VALUE,
                       &test) >= 0)
  {
    strcpy(test.message, "Choose a basic land card to put in your hand.");
    new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
  }

  shuffle(player);
}

// test only needs to be defined during EVENT_CAST_SPELL.
/* ManalinkEh 2022: 0x2650b10; original functions/tutors.c:1154. */
static int qsort_compare_int_descending(const void *lhs, const void *rhs)
{
  return *(const int *)rhs - *(const int *)lhs;
}

void descending_insertion_sort_on_player(int number, target_t* array){
	target_t value;
	int i, j;
	for (i = 1; i < number; ++i){
		value = array[i];	// struct copy
		for (j = i; j > 0 && value.player > array[j - 1].player; --j){
			array[j] = array[j - 1];	// struct copy
		}
		array[j] = value;	// struct copy
	}
}

// Separates the first number cards in source_deck[] into two piles.  AI attempts to divide evenly by base_value.
// Preconditions: piles is an array of int[2][number].  Elements of source_deck[] are internal_card_ids (such as from deck_ptr[], get_grave(), or rfg_ptr[]).
// Postcondition: For each i in [0..number), one of piles[0][i] and piles[1][i] will be set to source_deck[i], and the other iid_draw_a_card.
/* Legacy 208-byte test compatibility for the to-array selector.
 * Native 2022 entry and private part are recovered below. */

/* ManalinkEh 2022: 0x2652ab0; original functions/tutors.c:1158. */
void descending_qsort(int number, int *array)
{
  qsort(array, number, 4, qsort_compare_int_descending);
  return;
}

// Chooses between two piles of cards each with number cards.
// Preconditions: piles is an array of int[2][number].  Elements of piles[][] are internal_card_ids.  ai_selection_mode is AI_MAX_VALUE or AI_MIN_VALUE.  choice_txt is 11 chars or less.
// Returns chosen pile, 0 or 1.
/* ManalinkEh 2022: 0x2650b30; original functions/tutors.c:1162. */
static int qsort_compare_target_player_descending(const void *lhs, const void *rhs)
{
  return *(const int *)rhs - *(const int *)lhs;
}

// 1-who_chooses separates the top number cards of player's deck into two piles.  who_chooses picks a pile to put in player's hand.
// The rest are put in put_rest_where, which must currently be TUTOR_GRAVE or TUTOR_BOTTOM_OF_DECK.
/* ManalinkEh 2022: 0x2652ae0; original functions/tutors.c:1166. */
void descending_qsort_on_player(int number, target_t *array)
{
  qsort(array, number, 8, qsort_compare_target_player_descending);
  return;
}

int spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(int player, int card, event_t event, int max_cards, test_definition_t* test, int dont_kill_self)
{
  if (event == EVENT_CAN_CAST)
	return 1;

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int num_chosen = select_multiple_cards_from_graveyard(player, player, 0, AI_MAX_VALUE, test, max_cards, &instance->targets[0]);
	  if (num_chosen < max_cards)
		ai_modifier += (player == AI ? -24 : 24) * (max_cards - num_chosen);
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int i, num_validated = 0, num_targeted = 0, selected;
	  for (i = 0; i < max_cards; ++i)
		if (instance->targets[i].card != -1)
		  {
			++num_targeted;
			if ((selected = validate_target_from_grave(player, card, player, i)) != -1)
			  {
				from_grave_to_hand(player, selected, TUTOR_HAND);
				++num_validated;
			  }
		  }

	  if (num_validated == 0 && num_targeted > 0)
		spell_fizzled = 1;

	  if (!dont_kill_self)
		kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

/* Choose one or both - Return target [type1] card from your graveyard to your hand; and/or return target [type2] card from your graveyard to your hand.
 * Not really appropriate if type1 == type2, for which you want spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(). */
int spell_return_one_or_two_cards_from_gy_to_hand(int player, int card, event_t event, type_t type1, type_t type2)
{
  if (event == EVENT_CAN_CAST)
	{
	  if (((type1 | type2) & (TYPE_PERMANENT | TARGET_TYPE_PLANESWALKER)) == TYPE_PERMANENT)
		{
		  // have to check separately, or else a planeswalker in the graveyard will be a false positive
		  if (!any_in_graveyard_by_type(player, type1) && !any_in_graveyard_by_type(player, type2))
			return 0;
		}
	  else if (!any_in_graveyard_by_type(player, type1 | type2))	// Otherwise, can just iterate through graveyard once
		return 0;

	  return !graveyard_has_shroud(player);
	}

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  instance->targets[0].player = instance->targets[0].card = -1;
	  instance->targets[1].player = instance->targets[1].card = -1;

	  int iid = -1, i;
	  type_t types[2] = { type1, type2 };
	  test_definition_t test;
	  for (i = 0; i <= 1; ++i)
		if (any_in_graveyard_by_type(player, types[i]))
		  {
			new_default_test_definition(&test, types[i], "");
			char type_txt[100];
			type_text(type_txt, 100, types[i], 0);
			scnprintf(test.message, 100, "Select target %s card.", type_txt);

			if (select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &test, i) == -1)
			  cancel = 0;	// Might still choose the other type; or might have done so already.
			else if (i == 0)
			  iid = turn_card_in_grave_face_down(player, instance->targets[i].player);
		  }

	  if (iid != -1)
		turn_card_in_grave_face_up(player, instance->targets[0].player, iid);

	  if (instance->targets[0].player == -1 && instance->targets[1].player == -1)	// chose neither
		cancel = 1;
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  int i, num_validated = 0;;
	  for (i = 0; i <= 1; ++i)
		if (validate_target_from_grave_source(player, card, player, i) >= 0)
		  {
			from_grave_to_hand(player, instance->targets[i].player, TUTOR_HAND);
			++num_validated;
		  }

	  if (num_validated == 0)
		spell_fizzled = 1;

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

void separate_into_two_piles(int who_separates, const int *source_deck, int number, void *piles_)
{
  int (*piles)[number] = piles_;
  int i;
  for (i = 0; i < number; ++i)
  {
    piles[0][i] = source_deck[i];
    piles[1][i] = iid_draw_a_card;
  }

  if (number <= 1)
    return;

  if (who_separates == AI || ai_is_speculating == 1)
  {
    /* Evenly separating a pile of things with various scores into two piles of roughly equal total
     * scores is a variant of the Knapsack Problem, and is NP complete.  On the other hand, we could
     * solve it exactly, since we know number is always very small (5 or under).  But, it's not
     * worth even that much effort.  What we're going to do instead is sort on score, then for each
     * card, put it into the pile with the lowest total score so far. */
    int scores[2] = {0, 0};

    target_t vals[number]; // vals[].player = score, vals[].card = index into source deck
    for (i = 0; i < number; ++i)
    {
      vals[i].player = my_base_value_by_id(source_deck[i]);
      vals[i].player = MAX(1, vals[i].player);
      vals[i].card = i;
    }

    descending_qsort_on_player(number, vals);

    for (i = 0; i < number; ++i)
      if (scores[0] <= scores[1])
        // Leave in pile 0
        scores[0] += vals[i].player;
      else
      {
        // Move to pile 1
        scores[1] += vals[i].player;
        SWAP(piles[0][vals[i].card], piles[1][vals[i].card]);
      }
  }
  else
    while (1)
    {
      int selected = show_deck(who_separates, piles[0], number, "Choose pile 1.", 0, gs_done_008b40e0);
      if (selected == -1)
        break;

      SWAP(piles[0][selected], piles[1][selected]);
    }
}

int choose_between_two_piles(int chooser, int number, void *piles_, int ai_selection_mode,
                             const char *choice_txt)
{
  ASSERT(ai_selection_mode == AI_MIN_VALUE || ai_selection_mode == AI_MAX_VALUE);

  int (*piles)[number] = piles_;

  if (chooser == AI || ai_is_speculating == 1)
  {
    int scores[2] = {0, 0};

    int i, j;
    for (i = 0; i < 2; ++i)
      for (j = 0; j < number; ++j)
        if (piles[i][j] != -1 && piles[i][j] != iid_draw_a_card)
        {
          int val = my_base_value_by_id(piles[i][j]);
          val = MAX(1, val);
          scores[i] += val;
        }

    int chosen;
    if (scores[0] > scores[1])
      chosen = ai_selection_mode == AI_MAX_VALUE ? 0 : 1;
    else
      chosen = ai_selection_mode == AI_MAX_VALUE ? 1 : 0;

    show_deck(HUMAN, piles[chosen], number, "Opponent chose this pile.", 0, gs_ok_00924800);

    return chosen;
  }
  else
  {
    int selected, chosen = 1;
    do
    {
      chosen = 1 - chosen;
      selected = show_deck(chooser, piles[chosen], number, "Click a card to see the other pile.", 0,
                           choice_txt);
    } while (selected != -1);

    return chosen;
  }
}

void effect_fof(int player, int who_chooses, int number, int put_rest_where)
{
  int num_cards_in_deck = count_deck(player);
  number = MIN(number, num_cards_in_deck);

  ASSERT((put_rest_where == TUTOR_GRAVE || put_rest_where == TUTOR_BOTTOM_OF_DECK) && "others unimplemented");

  if (number > 0)
	{
	  int piles[2][number];

	  separate_into_two_piles(1 - who_chooses, deck_ptr[player], number, piles);

	  int chosen = choose_between_two_piles(who_chooses, number, piles, who_chooses == player ? AI_MAX_VALUE : AI_MIN_VALUE, "Put in hand");

	  // Obliterate the existing cards on top of the deck
	  obliterate_top_n_cards_of_deck(player, number);

	  // Put the ones from the chosen pile into hand one at a time
	  int i;
	  for (i = 0; i < number; ++i)
		if (piles[chosen][i] != iid_draw_a_card && piles[chosen][i] != -1)
		  add_card_to_hand(player, piles[chosen][i]);

	  // Put the ones from the unchosen pile back on top of the library
	  int num_unchosen = 0;
	  for (i = number - 1; i >= 0; --i)
		if (piles[1 - chosen][i] != iid_draw_a_card && piles[1 - chosen][i] != -1)
		  {
			raw_put_iid_on_top_of_deck(player, piles[1 - chosen][i]);
			++num_unchosen;
		  }

	  if (num_unchosen > 0)
		switch (put_rest_where)
		  {
			case TUTOR_GRAVE:
			  mill(player, num_unchosen);
			  break;

			case TUTOR_BOTTOM_OF_DECK:
			  for (i = 0; i < num_unchosen; ++i)
				put_top_card_of_deck_to_bottom(player);
			  if (num_unchosen > 1)
				rearrange_bottom_x(player, player, num_unchosen);
			  break;
		  }
	}
}

void alphabetize_deck(int player, int* deck, int num_cards){
	int count = count_deck(player);
	if( num_cards < count || deck != deck_ptr[player] ){ return; }
	//set_trap_condition(player, TRAP_DECK_WAS_SEARCHED, 1);
	if( count < 10 || ! get_setting(SETTING_ALPHABETIZE_DECK) ){ return; }
	if( player == AI ){ return; }

	//FILE *file = fopen("sort.txt", "w");
	//int *deck = deck_ptr[player];

	int i;
	int deck2[500];
	for(i=0;i<500;i++){
		deck2[i] = -1;
	}
	for(i=0;i<count;i++){
		int card = deck[i];
		card_ptr_t* c = cards_ptr[ cards_data[ deck[i] ].id ];
		//fprintf(file,"%d) Card=%s\n", i, c->name );

		int j;
		int index = i;
		for(j=0;j<i;j++){
			card_ptr_t* c1 = cards_ptr[ cards_data[ deck2[j] ].id ];
			if( strcmp( c1->name, c->name  ) > 0  ){
				index = j;
				//fprintf(file," --Fitting before %s\n", c1->name );
				break;
			}
			else{
				//fprintf(file," Skipping %s\n", c1->name );
			}
		}
		for(j=i+1;j>=index;j--){
			deck2[j] = deck2[j-1];
		}
		deck2[index] = card;

		// print out the deck
		//for(j=0;j<=i;j++){
		//    card_ptr_t* c1 = cards_ptr[ cards_data[ deck2[j] ].id ];
		//    fprintf(file,"   %d) %s\n", j, c1->name );
		//}

	}
	for(i=0;i<500;i++){
		deck[i] = deck2[i];
	}
	//fclose(file);
}

int get_total_available_lands(int player){

	int total_lands = 0;

	CYCLE_ALL_CARDS(p, c, 	{
								if( p == player ){
									if( (in_hand(p, c) || in_play(p, c)) && is_what(p, c, TYPE_LAND) ){
										total_lands++;
									}
								}
							}
	);

	return total_lands;
}

void scrylike_effect(int player, int t_player, int remaining)
{
  int *deck = deck_ptr[t_player];
  if (remaining > count_deck(t_player))
    remaining = count_deck(t_player);
  if (remaining <= 0)
    return;
  if (IS_AI(player))
  {
    int lands = get_total_available_lands(t_player);
    if (lands < 4 || lands >= 6)
    {
      test_definition_t test;
      default_test_definition(&test, TYPE_LAND);
      if ((player == t_player && lands < 4) || (player != t_player && lands >= 6))
        test.type_flag = DOESNT_MATCH;
      test.no_shuffle = 1;
      test.qty = remaining;
      test.create_minideck = remaining;
      int result = new_global_tutor(player, t_player, TUTOR_FROM_DECK, TUTOR_BOTTOM_OF_DECK, 0,
                                    AI_MAX_VALUE, &test);
      if (test.qty < 2)
        remaining -= result >= 0 ? 1 : 0;
      else
        remaining -= result;
    }
  }
  else
  {
    while (remaining > 0)
    {
      int selected =
          show_deck(player, deck, remaining, "Choose a card to put on the bottom", 0, gs_done_008b40e0);
      if (selected == -1)
        break;
      int deck_size = count_deck(t_player);
      deck[deck_size] = deck[selected];
      remove_card_from_deck(t_player, selected);
      --remaining;
    }
  }
  if (remaining > 1)
    rearrange_top_x(t_player, player, remaining);
}

void scry(int player, int number_of_cards)
{
  if (check_battlefield_for_id(player, CARD_ID_ELIGETH_CROSSROADS_AUGUR)) /* Eligeth, Crossroads Augur */
    draw_cards(player, number_of_cards);
  else
  {
    scrylike_effect(player, player, number_of_cards);
    dispatch_xtrigger2(player, XTRIGGER_SCRY, "Scrying", 0, player, number_of_cards);
  }
}

void rearrange_top_x(int t_player, int who_chooses, int number)
{
  // t_player = target player
  // who_chooses = player who rearranges

  int *deck = deck_ptr[t_player];
  number = MIN(number, count_deck(t_player));
  if (number <= 0)
    return;
  if (number == 1)
  {
    show_deck(who_chooses, deck, 1, "Top card of library", 0, gs_done_008b40e0);
  }
  else
  {
    int backup_deck[number];
    int i;
    for (i = 0; i < number; i++)
    {
      backup_deck[i] = deck_ptr[t_player][0];
      remove_card_from_deck(t_player, 0);
    }
    if (!IS_AI(who_chooses))
    {
      test_definition_t test;
      new_default_test_definition(&test, TYPE_ANY, "Pick a card to put on top.");
      while (1)
      {
        int selected = select_card_from_zone(who_chooses, who_chooses, backup_deck, number, 0,
                                             AI_MAX_VALUE, -1, &test);
        if (selected != -1)
        {
          int transit = backup_deck[selected];
          for (int j = selected; j > 0; --j)
            backup_deck[j] = backup_deck[j - 1];
          backup_deck[0] = transit;
        }
        else
        {
          break;
        }
      }
      for (i = number - 1; i > -1; i--)
      {
        raw_put_iid_on_top_of_deck(t_player, backup_deck[i]);
      }
    }
    else
    {
      int lands = get_total_available_lands(t_player);
      int remaining = number;
      if (who_chooses != t_player)
      {
        if (lands <= 3)
        {
          for (i = 0; i < number; i++)
          {
            if (is_what(-1, backup_deck[i], TYPE_LAND))
            {
              raw_put_iid_on_top_of_deck(t_player, backup_deck[i]);
              backup_deck[i] = -1;
              remaining--;
            }
          }
          while (remaining)
          {
            int max_cmc = -1;
            int selected = -1;
            for (i = 0; i < number; i++)
            {
              if (backup_deck[i] > -1)
              {
                int this_cmc = get_cmc_by_internal_id(backup_deck[i]);
                if (this_cmc > max_cmc)
                {
                  max_cmc = this_cmc;
                  selected = i;
                }
              }
            }
            raw_put_iid_on_top_of_deck(t_player, backup_deck[selected]);
            backup_deck[selected] = -1;
            remaining--;
          }
        }
        if (lands >= 6)
        {
          for (i = 0; i < number; i++)
          {
            if (!is_what(-1, backup_deck[i], TYPE_LAND))
            {
              raw_put_iid_on_top_of_deck(t_player, backup_deck[i]);
              backup_deck[i] = -1;
              remaining--;
            }
          }
          while (remaining)
          {
            int min_cmc = 1000;
            int selected = -1;
            for (i = 0; i < number; i++)
            {
              if (backup_deck[i] > -1)
              {
                int this_cmc = get_cmc_by_internal_id(backup_deck[i]);
                if (this_cmc < min_cmc)
                {
                  min_cmc = this_cmc;
                  selected = i;
                }
              }
            }
            raw_put_iid_on_top_of_deck(t_player, backup_deck[selected]);
            backup_deck[selected] = -1;
            remaining--;
          }
        }
      }
      else
      {
        if (lands <= 3)
        {
          for (i = 0; i < number; i++)
          {
            if (!is_what(-1, backup_deck[i], TYPE_LAND))
            {
              raw_put_iid_on_top_of_deck(t_player, backup_deck[i]);
              backup_deck[i] = -1;
              remaining--;
            }
          }
          while (remaining)
          {
            int min_cmc = 1000;
            int selected = -1;
            for (i = 0; i < number; i++)
            {
              if (backup_deck[i] > -1)
              {
                int this_cmc = get_cmc_by_internal_id(backup_deck[i]);
                if (this_cmc < min_cmc)
                {
                  min_cmc = this_cmc;
                  selected = i;
                }
              }
            }
            raw_put_iid_on_top_of_deck(t_player, backup_deck[selected]);
            backup_deck[selected] = -1;
            remaining--;
          }
        }
        if (lands >= 6)
        {
          for (i = 0; i < number; i++)
          {
            if (is_what(-1, backup_deck[i], TYPE_LAND))
            {
              raw_put_iid_on_top_of_deck(t_player, backup_deck[i]);
              backup_deck[i] = -1;
              remaining--;
            }
          }
          while (remaining)
          {
            int max_cmc = -1;
            int selected = -1;
            for (i = 0; i < number; i++)
            {
              if (backup_deck[i] > -1)
              {
                int this_cmc = get_cmc_by_internal_id(backup_deck[i]);
                if (this_cmc > max_cmc)
                {
                  max_cmc = this_cmc;
                  selected = i;
                }
              }
            }
            raw_put_iid_on_top_of_deck(t_player, backup_deck[selected]);
            backup_deck[selected] = -1;
            remaining--;
          }
        }
      }
    }
  }
}

int show_bottom_of_deck(int player, int* deck, int num_cards, const char* prompt, int count_in_deck){
	// This is a moderate hack - instead of passing deck[] to show_deck, we find the exact position so that the bottom num_cards cards are shown
	return show_deck(player, &deck[count_in_deck - num_cards], num_cards, prompt, 0, gs_done_008b40e0);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

void rearrange_bottom_x(int t_player, int who_chooses, int number){
	// t_player = target player
	// who_chooses = player who rearranges

	int* deck = deck_ptr[t_player];
	int count = count_deck(t_player);
	if (count < number){
		number = count;
	}

	if (number == 1){
		show_bottom_of_deck(who_chooses, deck, 1, "Bottom card of library", count);
	} else {
		for (; number > 1; --number){
			int selected;
			do {
				selected = show_bottom_of_deck(who_chooses, deck, number, "Pick a card to put on bottom", count);
			} while (selected == -1);

			SWAP(deck[count - number], deck[count - number + selected]);
		}
	}
}

int select_best_card_from_top_x(int player, int t_player, int amount){

	int result = -1;
	int *deck = deck_ptr[t_player];
	int count = 0;
	int par = 0;
	if( player != t_player ){
		par = 1000;
	}

	while( count < amount ){
			card_ptr_t* c = cards_ptr[ cards_data[deck[count]].id ];
			if( player == t_player ){
				if( c->ai_base_value > par ){
					par = c->ai_base_value;
					result = count;
				}
			}
			else{
				if( c->ai_base_value < par ){
					par = c->ai_base_value;
					result = count;
				}
			}
			count++;
	}
	return result;
}

void put_top_x_on_bottom(int player, int t_player, int remaining ){

	int *deck = deck_ptr[t_player];

	while( remaining > 0 ){
			if( remaining == 1 ){
				deck[count_deck(t_player)] = deck[0];
				remove_card_from_deck(t_player, 0);
				remaining--;
			}
			else{
				char msg[100] = "Select a card to put on bottom.";
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, msg);
				this_test.create_minideck = remaining;
				this_test.no_shuffle = 1;
				int mode = AI_MAX_VALUE;
				if( player != t_player ){
					mode = AI_MIN_VALUE;
				}
				int selected = new_select_a_card(player, t_player, TUTOR_FROM_DECK, 1, mode, -1, &this_test);
				if( selected != -1 ){
					deck[count_deck(t_player)] = deck[selected];
					remove_card_from_deck(t_player, selected);
					remaining--;
				}
			}
	}
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int select_multiple_cards_from_graveyard_to_array(int player, int targ_player, int must_select_all,
                                                  int ai_selection_mode,
                                                  test_definition_t *this_test, int max_targets,
                                                  target_t *ret_location);

void put_top_x_on_bottom_in_random_order(int player, int amount)
{
  if (amount <= 0)
	return;

  int* deck = deck_ptr[player];

  // Store top amount cards of deck
  int i, cards[amount];
  for (i = 0; i < amount; ++i)
	cards[i] = deck[i];

  // Remove them from deck
  obliterate_top_n_cards_of_deck(player, amount);

  int pos = count_deck(player);

  for (; amount > 0; --amount, ++pos)
	{
	  // Put a random card from array on bottom of deck
	  int selected = internal_rand(amount);
	  ASSERT(deck[pos] == -1);
	  ASSERT(pos == 0 || deck[pos - 1] != -1);
	  deck[pos] = cards[selected];

	  // Replace the randomly chosen card from array with the last one
	  cards[selected] = cards[amount - 1];
	}
}

void tutor_multiple_card_from_hand(int who_chooses, int t_player, int who_gets_the_card, int destination, int must_select, int ai_selection_mode, test_definition_t *this_test){
	int hand_copy[2][hand_count[t_player]];
	int hcc = 0;
	int count = 0;
	while( count < active_cards_count[t_player] ){
			if( in_hand(t_player, count) ){
				hand_copy[0][hcc] = get_original_internal_card_id(t_player, count);
				hand_copy[1][hcc] = count;
				hcc++;
			}
			count++;
	}
	this_test->zone = TARGET_ZONE_HAND;
	int amount = MIN(this_test->qty, check_battlefield_for_special_card(t_player, -1, t_player, CBFSC_GET_COUNT, this_test));
	if( amount ){
		int i;
		int tutored[amount];
		int tc = 0;
		while( amount ){
				int selected = select_card_from_zone(who_chooses, who_chooses, hand_copy[0], hcc, 0, ai_selection_mode, -1, this_test);
				if( selected != -1 ){
					tutored[tc] = hand_copy[0][selected];
					obliterate_card(t_player, hand_copy[1][selected]);
					tc++;
					for(i=selected; i<hcc; i++){
						hand_copy[0][i] = hand_copy[0][i+1];
						hand_copy[1][i] = hand_copy[1][i+1];
					}
					hcc--;
					amount--;
				}
				else{
					break;
				}
		}
		for(i=0; i<tc; i++){
			if( destination == TUTOR_PLAY || destination == TUTOR_PLAY_TAPPED ){
				int card_added = add_card_to_hand(who_gets_the_card, tutored[i]);
				if( destination == TUTOR_PLAY_TAPPED ){
					add_state(who_gets_the_card, card_added, STATE_TAPPED);
				}
				if( t_player != who_gets_the_card ){
					if( t_player == AI && who_gets_the_card == HUMAN){
						add_state(who_gets_the_card, card_added, STATE_OWNED_BY_OPPONENT);
					}
					if( t_player == HUMAN && who_gets_the_card == HUMAN){
						remove_state(who_gets_the_card, card_added, STATE_OWNED_BY_OPPONENT);
					}
				}
				put_into_play(who_gets_the_card, card_added);
			}
			if( destination == TUTOR_RFG ){
				rfg_card_in_hand(t_player, tutored[i]);
			}
		}
	}
}

#ifndef SHANDALAR
/* Recovered October 2022 typed consumers and compiler parts. */

/* ManalinkEh 2022: 0x2650c40; original functions/tutors.c:10. */
int show_deck(int player, const int *deck, int num_cards, const char *prompt,
              int suppress_done_label, const char *done_label)
{
  if (duel_host_is_headless()) {
    int choice = duel_host_choose(DUEL_CHOICE_ZONE, player, prompt, num_cards, deck, NULL);
    if (choice < -1 || choice >= num_cards) duel_host_fail("illegal deck display choice");
    return choice;
  }

  int result;
  call_sub_437E20_unless_ai_is_speculating();
  hack_show_deck_force_expansion = 0;
  result = show_deck_exe(player, deck, num_cards, prompt, suppress_done_label, (int32_t)(intptr_t)done_label);
  return result;
}

/* ManalinkEh 2022: 0x2650ca0; original functions/tutors.c:18. */
int show_deck_force_expansion(int player, const int *deck, int num_cards, const char *prompt,
                              int suppress_done_label, const char *done_label, EXPANSION_t expansion)
{
  call_sub_437E20_unless_ai_is_speculating();
  hack_show_deck_force_expansion = expansion;
  int result = show_deck_exe(player, deck, num_cards, prompt, suppress_done_label,
                             (int32_t)(intptr_t)done_label);
  hack_show_deck_force_expansion = 0;
  return result;
}

/* ManalinkEh 2022: 0x2650cf0; original functions/tutors.c:28. */
int get_average_value_from_zone(int targ_player, const int *s_location, int zone_count,
                                test_definition_t *this_test, int my_test, int mode)
{
  int total = 0, found = 0;
  for (int i = 0; i < zone_count; ++i)
    if (new_make_test(targ_player, s_location[i], my_test, this_test))
    {
      ++found;
      if (mode == AI_MAX_CMC || mode == AI_MIN_CMC || mode == AI_GOOD_TO_PUT_IN_GRAVE)
        total += get_cmc_by_internal_id(s_location[i]);
      if (mode == AI_MAX_VALUE || mode == AI_MIN_VALUE)
        total += get_base_value(-1, s_location[i]);
    }
  return found ? total / found : 0;
}

/* ManalinkEh 2022: 0x2650de0; original functions/tutors.c:62. */
int select_card_from_zone(int player, int targ_player, const int *s_location, int zone_count,
                          int must_select, int ai_selection_mode, int test_type,
                          test_definition_t *this_test)
{
  if (duel_host_scripted_input()) {
    int selected = duel_host_choose_zone(player, this_test->message, zone_count, s_location, must_select);
    if (selected == -1 && !must_select) return -1;
    if (selected < 0 || selected >= zone_count ||
        !new_make_test(targ_player, s_location[selected], test_type, this_test))
      duel_host_fail("illegal scripted zone selection");
    return selected;
  }

  if (player != AI && !duel_host_is_headless())
    for (;;)
    {
      const char *label = hack_override_show_deck_done_button_label;
      if (!label)
        label = (const char *)ENGINE_DONE_LABEL;
      call_sub_437E20_unless_ai_is_speculating();
      hack_show_deck_force_expansion = 0;
      int selected =
          show_deck_exe(player, s_location, zone_count, this_test->message, 0, (int)label);
      if (selected != -1 && new_make_test(targ_player, s_location[selected], test_type, this_test))
        return selected;
      if (!must_select)
        return -1;
    }

  int average = get_average_value_from_zone(targ_player, s_location, zone_count, this_test,
                                            test_type, ai_selection_mode);
  int candidates[zone_count];
  int candidate_count = 0;
  for (int i = 0; i < zone_count; ++i)
    if (new_make_test(targ_player, s_location[i], test_type, this_test))
    {
      if (ai_selection_mode == AI_FIRST_FOUND)
        return i;
      if (ai_selection_mode == AI_RANDOM)
      {
        if (internal_rand(100) >= 50)
          return i;
        continue;
      }
      int graveyard_fallback = ai_selection_mode == AI_GOOD_TO_PUT_IN_GRAVE;
      if (graveyard_fallback && good_to_put_in_grave(-1, s_location[i]))
        return i;

      int amount = -1;
      if (ai_selection_mode == AI_MAX_VALUE || ai_selection_mode == AI_MIN_VALUE)
      {
        if (this_test->has_mana_to_pay_cmc == 0 ||
            (this_test->has_mana_to_pay_cmc == 1 &&
             has_mana(AI, COLOR_COLORLESS, get_cmc_by_internal_id(s_location[i]))))
          amount = get_base_value(-1, s_location[i]);
      }
      if (ai_selection_mode == AI_MAX_CMC || ai_selection_mode == AI_MIN_CMC || graveyard_fallback)
      {
        if (this_test->has_mana_to_pay_cmc == 0 ||
            (this_test->has_mana_to_pay_cmc == 1 &&
             has_mana(AI, COLOR_COLORLESS, get_cmc_by_internal_id(s_location[i]))))
          amount = get_cmc_by_internal_id(s_location[i]);
      }
      if ((ai_selection_mode == AI_MAX_VALUE || ai_selection_mode == AI_MAX_CMC ||
           graveyard_fallback) &&
          amount >= average)
        candidates[candidate_count++] = i;
      if ((ai_selection_mode == AI_MIN_VALUE || ai_selection_mode == AI_MIN_CMC) &&
          amount <= average + 1)
        candidates[candidate_count++] = i;
    }
  return candidate_count ? candidates[internal_rand(candidate_count)] : -1;
}

/* ManalinkEh 2022: 0x2650b80; original functions/tutors.c:152. */
static void side_effects_to_tutor_deck_from_permanents(int player, int targ_player)
{
  int amount;
  int card;
  int opponent;

  opponent = 1 - player;
  amount = count_cards_by_id(opponent, CARD_ID_OB_NIXILIS_UNSHACKLED);
  if (amount == 0)
  {
    return;
  }
  lose_life(player, amount * 10);
  card = locate_id(opponent, 0x3741);
  impose_sacrifice(opponent, card, player, amount, 2, 0, 0, 0, 0, 0, 0, 0, -1, 0);
  return;
}

/* ManalinkEh 2022: 0x2651140; original functions/tutors.c:323. */
int get_tutoring_denial(int player)
{
  card_instance_t *instance;
  int result;
  int card;
  int player_00;

  player_00 = 0;
  do
  {
    if (0 < active_cards_count[player_00])
    {
      card = 0;
      do
      {
        instance = in_play(player_00, card);
        if (instance != NULL)
        {
          result = is_what(player_00, card, 0x47);
          if (result != 0)
          {
            result = get_id(player_00, card);
            if (result == 0x271f)
            {
              return 1;
            }
            result = get_id(player_00, card);
            if (((result == 0x2ddf) || (result = get_id(player_00, card), result == 0x4d62)) &&
                (player != player_00))
            {
              return 1;
            }
            result = get_id(player_00, card);
            if ((result == 0x2c1e) &&
                (result = find_legacy_effect(player_00, card, effect_leonin_arbiter), result == -1))
            {
              result = has_mana(player, 0, 2);
              if (result == 0)
              {
                return 1;
              }
              result =
                  do_dialog(player, player_00, card, -1, -1, " Deactivate until EOT\n Pass", 0);
              if (result == 1)
              {
                return 1;
              }
              charge_mana(player, COLOR_COLORLESS, 2);
              if (spell_fizzled == 1)
              {
                return 1;
              }
              create_legacy_effect(player_00, card, effect_leonin_arbiter);
            }
          }
          result = is_what(player_00, card, 0x80);
          if ((result != 0) &&
              (instance = get_card_instance(player_00, card), instance->targets[2].card == 0x1f14))
          {
            return 1;
          }
        }
        card = card + 1;
      } while (card < active_cards_count[player_00]);
    }
    player_00 = player_00 + 1;
    if (player_00 == 2)
    {
      return 0;
    }
  } while (true);
}

/* ManalinkEh 2022: 0x2651300; original functions/tutors.c:178. */
int new_select_a_card(int player, int targ_player, int search_location, int must_select,
                      int ai_selection_mode, int test_type, test_definition_t *this_test)
{
  if (search_location < 1)
    return -1;

  unsigned int zone_count = this_test->create_minideck;
  const int *location;
  int hand_index[150];
  if (search_location == TUTOR_FROM_DECK)
  {
    if (!zone_count)
    {
      if (get_tutoring_denial(targ_player))
        return -1;
      zone_count = this_test->create_minideck;
    }
    location = deck_ptr[targ_player];
    if (check_battlefield_for_id(1 - targ_player, CARD_ID_AVEN_MINDCENSOR))
    {
      zone_count = 4;
      if (count_deck(targ_player) < 4)
        zone_count = count_deck(targ_player);
    }
  }
  else
  {
    if (search_location == TUTOR_FROM_GRAVE && graveyard_has_shroud(targ_player))
      return -1;
    location = search_location == TUTOR_FROM_GRAVE || search_location == TUTOR_FROM_GRAVE_NOTARGET
                   ? graveyard_ptr_mutable[targ_player]
                   : deck_ptr[targ_player];
    if (search_location == TUTOR_FROM_RFG)
      location = rfg_ptr[targ_player];
    else if (search_location == TUTOR_FROM_HAND)
    {
      for (int i = 0; i < 150; ++i)
        hand_index[i] = -1;
      int hand_count_local = 0;
      for (int i = 0; i < active_cards_count[targ_player]; ++i)
        if (in_hand(targ_player, i) && !check_state(targ_player, i, STATE_CANNOT_TARGET))
        {
          card_instance_t *instance = get_card_instance(targ_player, i);
          hand_index[hand_count_local++] = instance->internal_card_id;
          ++zone_count;
        }
      location = hand_index;
    }
  }
  if (*location == -1)
    return -1;
  if ((int)zone_count < 1)
    while (location[zone_count] != -1)
      ++zone_count;

  int selected = select_card_from_zone(player, targ_player, location, zone_count, must_select,
                                       ai_selection_mode, -1, this_test);
  if (targ_player == player && search_location == TUTOR_FROM_DECK)
  {
    side_effects_to_tutor_deck_from_permanents(player, player);
    return selected;
  }
  if (selected != -1 && search_location == TUTOR_FROM_HAND)
  {
    uint16_t selected_id = cards_data[location[selected]].id;
    for (int i = 0; i < active_cards_count[targ_player]; ++i)
      if (in_hand(targ_player, i) && get_id(targ_player, i) == selected_id)
        return i;
  }
  return selected;
}

/* ManalinkEh 2022: 0x26515c0; original functions/tutors.c:272. */
int select_a_card(int player, int targ_player, int search_location, int must_select,
                       int ai_selection_mode, int test_type, int type, int flag1, int subtype,
                       int flag2, int color, int flag3, int id, int flag4, int cc, int flag5)
{
  int result;
  test_definition_t this_test;
  default_test_definition(&this_test, type);
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
  result = new_select_a_card(player, targ_player, search_location, must_select,
                                  ai_selection_mode, test_type, &this_test);
  return result;
}

static int select_a_card_from_deck_automatic(int player, int targ_player, int test_type,
                                              test_definition_t *this_test)
{
  if (get_tutoring_denial(player))
    return -1;
  int pos = 0;
  if (check_battlefield_for_id(1 - targ_player, CARD_ID_AVEN_MINDCENSOR) && count_deck(targ_player) > 3)
    pos = count_deck(targ_player) - 4;
  for (; pos < count_deck(player); ++pos)
    if (new_make_test(targ_player, deck_ptr[targ_player][pos], test_type, this_test))
      return pos;
  return -1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2651990; original functions/tutors.c:389. */
int new_global_tutor(int player, int targ_player, int search_location, int destination,
                          int must_select, int ai_selection_mode, test_definition_t *this_test)
{
  int avoid_shuffle = this_test->no_shuffle;
  int test_type = new_get_test_score(this_test);
  int capacity = this_test->qty;
  int tutored[capacity];
  int selector = 1 - player;
  int opposition_agent = check_battlefield_for_id(selector, CARD_ID_OPPOSITION_AGENT);
  int self_search = player == targ_player;
  int shuffle_after = avoid_shuffle == 0 && search_location == TUTOR_FROM_DECK;
  int shuffle_before;
  int zone;
  if (!opposition_agent || player != targ_player || search_location != TUTOR_FROM_DECK)
  {
    shuffle_before = destination == TUTOR_DECK && shuffle_after;
    shuffle_after = shuffle_after && destination != TUTOR_DECK;
    selector = player;
    zone =
        search_location == TUTOR_FROM_GRAVE_NOTARGET ? TUTOR_FROM_GRAVE : search_location;
  }
  else
  {
    shuffle_before = 0;
    self_search = selector == targ_player;
    destination = TUTOR_OPPOSITION_AGENT;
    zone = TUTOR_FROM_DECK;
  }

  int tc = 0;
  int cancelled = 0;
  int card_added = -1;
  int really_tutored = 0;
  if (this_test->qty != 0)
  {
    unsigned int q = 0;
    do
    {
      int selected = -1;
      if (zone == TUTOR_FROM_DECK_AUTO)
      {
        if (!get_tutoring_denial(selector))
        {
          unsigned int starting_card = 0;
          if (check_battlefield_for_id(1 - targ_player, 0x22b9) && count_deck(targ_player) > 3)
            starting_card = count_deck(targ_player) - 4;
          while ((int)starting_card < count_deck(selector))
          {
            if (new_make_test(targ_player, deck_ptr[targ_player][starting_card], test_type,
                                   this_test))
            {
              selected = starting_card;
              break;
            }
            ++starting_card;
          }
        }
        zone = TUTOR_FROM_DECK;
      }
      else
        selected = new_select_a_card(selector, targ_player, search_location, must_select,
                                          ai_selection_mode, test_type, this_test);

      if (selected == -1)
      {
        if (this_test->qty > 1)
          break;
        cancelled = -2;
      }
      else
      {
        if (zone == TUTOR_FROM_GRAVE || zone == TUTOR_FROM_DECK)
        {
          int *s_location = zone == TUTOR_FROM_GRAVE ? graveyard_ptr_mutable[targ_player]
                                                         : deck_ptr[targ_player];
          if (zone == TUTOR_FROM_DECK && selector == targ_player)
            set_trap_condition(targ_player, 0x600, 1);
          int from = zone == TUTOR_FROM_GRAVE ? 2 : 1;
          if (!grafdiggers_cages(targ_player, (type_t)cards_data[s_location[selected]].type,
                                 from) ||
              ((unsigned int)(destination - TUTOR_PLAY) > 1 &&
               destination != TUTOR_PLAY_ATTACKING))
          {
            tutored[tc++] = s_location[selected];
            if (zone == TUTOR_FROM_GRAVE)
              remove_card_from_grave(targ_player, selected);
            else
              remove_card_from_deck(targ_player, selected);
          }
          else
            tutored[tc] = -1;
        }
        else if (zone == TUTOR_FROM_RFG)
        {
          int iid = rfg_ptr[targ_player][selected];
          tutored[tc++] = iid;
          remove_card_from_rfg(targ_player, cards_data[iid].id);
        }
        else if (zone == TUTOR_FROM_HAND)
        {
          if (destination == TUTOR_GET_ID)
            selected = get_id(targ_player, selected);
          tutored[tc++] = selected;
          get_card_instance(targ_player, selected)->state |= 0x20;
        }
        if (this_test->create_minideck > 1)
          --this_test->create_minideck;
      }
      ++q;
    } while (q < this_test->qty);
  }

  if (shuffle_before)
    shuffle(targ_player);
  else if (search_location == TUTOR_FROM_HAND)
    for (int q = 0; q < tc; ++q)
      get_card_instance(targ_player, tutored[q])->state &= ~0x20U;

  int storage_loc = 0;
  for (int q = 0; q < tc; ++q)
  {
    int iid = tutored[q];
    if (iid == -1)
      continue;
    ++really_tutored;
    switch (destination)
    {
    case TUTOR_HAND:
    case TUTOR_TPLAYER_HAND:
    case TUTOR_HAND_AND_MARK:
    {
      int dest_player = destination == TUTOR_TPLAYER_HAND ? targ_player : selector;
      card_added = add_card_to_hand(dest_player, iid);
      int reveal = selector == (destination == TUTOR_HAND) && avoid_shuffle != 2;
      if (reveal && ((unsigned int)(zone - TUTOR_FROM_GRAVE) < 2 ||
                     (test_type != 0 && (test_type != 1 || this_test->type != 0xff))))
        reveal_card(selector, card_added, -1, -1);
      if (destination == TUTOR_HAND_AND_MARK)
        add_state(dest_player, card_added, 0x200000);
      if (zone == TUTOR_FROM_GRAVE)
        from_grave_to_hand_triggers(dest_player, card_added);
      break;
    }
    case TUTOR_DECK:
      if (zone == TUTOR_FROM_HAND)
        card_added = iid;
      if ((test_type == 0 || (test_type == 1 && this_test->type == 0xff)) || selector != 1)
      {
        if (zone != TUTOR_FROM_HAND)
        {
          put_iid_under_the_first_x_cards_of_library(targ_player, iid, 0);
          /* Report a successful move, as the reveal/hand path does. Callers
             such as Drafna's Restoration continue selecting until cancelled. */
          card_added = iid;
          break;
        }
      }
      else if (zone == TUTOR_FROM_HAND)
        reveal_card(1, card_added, -1, -1);
      else
      {
        card_added = add_card_to_hand_invisible(targ_player, iid);
        reveal_card(1, card_added, -1, -1);
      }
      put_on_top_of_deck(targ_player, card_added);
      break;
    case TUTOR_GRAVE:
      if (zone == TUTOR_FROM_HAND)
      {
        new_discard_card(targ_player, iid, selector, 0);
        card_added = iid;
      }
      else
      {
        card_added = add_card_to_hand(targ_player, iid);
        if (zone == TUTOR_FROM_DECK)
        {
          put_on_top_of_deck(targ_player, card_added);
          mill(targ_player, 1);
        }
        else
          kill_card(targ_player, card_added, KILL_DESTROY);
      }
      break;
    case TUTOR_PLAY:
    case TUTOR_PLAY_TAPPED:
    case TUTOR_PLAY_ATTACKING:
    {
      card_added = zone == TUTOR_FROM_HAND ? iid : add_card_to_hand(targ_player, iid);
      card_instance_t *instance = get_card_instance(targ_player, card_added);
      if (targ_player != selector)
      {
        if (selector == 0)
          add_state(targ_player, card_added, 0x1000);
        else
          remove_state(targ_player, card_added, 0x1000);
      }
      if (destination == TUTOR_PLAY_TAPPED || destination == TUTOR_PLAY_ATTACKING)
        instance->state |= 0x10;
      if (zone == TUTOR_FROM_GRAVE)
        set_special_flags3(targ_player, card_added, 1);
      else if (zone == TUTOR_FROM_DECK)
        set_special_flags(targ_player, card_added, 0x10000);
      put_into_play(targ_player, card_added);
      if (targ_player != selector)
      {
        card_added = gain_control_impl(targ_player, card_added);
        instance = get_card_instance(selector, card_added);
      }
      if (destination == TUTOR_PLAY_ATTACKING)
      {
        instance->state |= 4;
        choose_who_attack(selector, card_added);
      }
      break;
    }
    case TUTOR_RFG:
      if (zone == TUTOR_FROM_HAND)
      {
        card_added = get_id(targ_player, iid);
        rfg_card_in_hand(targ_player, iid);
      }
      else
      {
        card_added = cards_data[iid].id;
        add_card_to_rfg(targ_player, iid);
      }
      play_sound_effect((wav_t)0x17);
      break;
    case TUTOR_BOTTOM_OF_DECK:
    case TUTOR_BOTTOM_OF_DECK_RANDOM:
      if (zone == TUTOR_FROM_HAND)
      {
        int position = count_deck(targ_player);
        deck_ptr[targ_player][position] = get_original_internal_card_id(targ_player, iid);
        obliterate_card(targ_player, iid);
      }
      else
      {
        card_added = cards_data[iid].id;
        deck_ptr[targ_player][count_deck(targ_player)] = iid;
      }
      break;
    case TUTOR_OPPOSITION_AGENT:
    {
      card_added = add_card_to_hand(selector, iid);
      card_instance_t *instance = get_card_instance(selector, card_added);
      instance->state |= 0x20;
      --hand_count[selector];
      reveal_card(selector, card_added, -1, -1);
      add_card_to_rfg(player, iid);
      create_may_play_card_from_exile_effect(selector, card_added, player, cards_data[iid].id, MPCFE_FOR_CMC);
      obliterate_card(selector, card_added);
      break;
    }
    default:
      break;
    }
    if (this_test->storage >= 0)
      get_card_instance(selector, this_test->storage)->targets[storage_loc++].card = iid;
  }
  if (search_location == TUTOR_FROM_DECK_AUTO && self_search)
    side_effects_to_tutor_deck_from_permanents(selector, targ_player);
  if (shuffle_after)
    shuffle(targ_player);
  if (cancelled == -2)
    return -2;
  return this_test->qty < 2 ? card_added : really_tutored;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 specialized activation costs. */

/* ManalinkEh 2022: 0x26524d0; original functions/tutors.c:689. */
int global_tutor(int player, int targ_player, int search_location, int destination,
                      int must_select, int ai_selection_mode, int type, int flag1, int subtype,
                      int flag2, int color, int flag3, int id, int flag4, int cc, int flag5)
{
  int result;
  test_definition_t this_test;
  default_test_definition(&this_test, type);
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
  result = new_global_tutor(player, targ_player, search_location, destination, must_select,
                                 ai_selection_mode, &this_test);
  return result;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 global tutoring and control transfer. */

/* ManalinkEh 2022: 0x26526c0; original functions/tutors.c:757.
 * Native behavior: the 50-byte local is passed with a formatting bound of 100. */
int reveal_top_cards_of_library(int player, int num_cards)
{
  int count = count_deck(player);
  if (num_cards < count)
    count = num_cards;
  if (count <= 0)
    return 0;
  if (ai_is_speculating != 1)
  {
    char buffer[50];
    if (player == 0)
      scnprintf(buffer, 100, " You reveal");
    else
      scnprintf(buffer, 100, " %s reveals", opponent_name);
    int *deck = deck_ptr[player];
    call_sub_437E20_unless_ai_is_speculating();
    hack_show_deck_force_expansion = 0;
    show_deck_exe(0, deck, count, buffer, 0, 0x78f0a8);
  }
  return count;
}

/* ManalinkEh 2022: 0x2654d60; original functions/tutors.c:796. */
int reveal_top_cards_of_library_and_choose_fn_who_chooses(
    int src_player, int src_card, int t_player, int who_chooses, int num_cards, int must_select,
    tutor_t destination_chosen, int reveal_chosen,
    tutor_t destination_rest, int reveal_rest, test_definition_t *test,
    library_reveal_callback_t fn)
{
  if (reveal_rest)
    num_cards = reveal_top_cards_of_library(t_player, num_cards);
  else
  {
    int count = count_deck(t_player);
    if (num_cards > count)
      num_cards = count;
  }
  if (num_cards <= 0)
    return -1;

  int num_to_pick = test->qty;
  if (num_to_pick > num_cards)
    num_to_pick = num_cards;
  if (num_to_pick <= 0)
  {
    char buffer[100];
    scnprintf(buffer, 100,
              " %s reveals:", cards_ptr[get_id(src_player, src_card)]->name);
    call_sub_437E20_unless_ai_is_speculating();
    hack_show_deck_force_expansion = 0;
    show_deck_exe(who_chooses, deck_ptr[t_player], num_cards, buffer, 0, 0x7375b0);
    return -1;
  }

  test->qty = 1;
  test->create_minideck = (uint16_t)num_cards;
  int chosen_iids[num_to_pick];
  int num_chosen = 0;
  int selected = -1;
  int cage = 0;
  if (destination_chosen == TUTOR_PLAY || destination_chosen == TUTOR_PLAY_TAPPED ||
      destination_chosen == TUTOR_PLAY_ATTACKING)
    cage = grafdiggers_cages(t_player, TYPE_CREATURE, 1) != 0;

  for (int i = 0; i < num_to_pick; ++i)
  {
    selected = select_card_from_zone(who_chooses, t_player, deck_ptr[t_player], num_cards,
                                          must_select, test->ai_selection_mode, -1, test);
    if (selected == -1)
      break;
    --num_cards;
    int iid = deck_ptr[t_player][selected];
    if (reveal_chosen)
      reveal_card_iid(src_player, src_card, iid);

    switch (destination_chosen)
    {
    case TUTOR_HAND:
    case TUTOR_HAND_AND_MARK:
      remove_card_from_deck(t_player, selected);
      selected = add_card_to_hand(src_player, iid);
      if (destination_chosen == TUTOR_HAND_AND_MARK && selected >= 0)
        add_state(src_player, selected, STATE_TARGETTED);
      break;
    case TUTOR_GRAVE:
      remove_card_from_deck(t_player, selected);
      chosen_iids[num_chosen++] = iid;
      break;
    case TUTOR_PLAY:
    case TUTOR_PLAY_TAPPED:
    case TUTOR_PLAY_ATTACKING:
      if (!(cage && is_what(-1, deck_ptr[t_player][selected], TYPE_CREATURE)))
      {
        remove_card_from_deck(t_player, selected);
        chosen_iids[num_chosen++] = iid;
        break;
      }
    case TUTOR_DECK:
      for (int n = selected; n < num_cards; ++n)
      {
        int temporary = deck_ptr[t_player][n];
        deck_ptr[t_player][n] = deck_ptr[t_player][n + 1];
        deck_ptr[t_player][n + 1] = temporary;
      }
      break;
    case TUTOR_EXILE:
      rfg_card_in_deck(t_player, selected);
      break;
    case TUTOR_GET_ID:
      selected = (uint16_t)cards_data[iid].id;
    case TUTOR_GET_POSITION:
      if (num_to_pick > 1)
      {
        char buf[200];
        sprintf(buf, "%s:%d\012illegal destination_chosen: %d with qty=%d",
                "functions/tutors.c", 884, (int)destination_chosen, num_to_pick);
        backtrace_and_abort(buf);
      }
      break;
    case TUTOR_BOTTOM_OF_DECK:
      put_card_in_deck_to_bottom(t_player, selected);
      break;
    case TUTOR_TPLAYER_HAND:
      remove_card_from_deck(t_player, selected);
      selected = add_card_to_hand(t_player, iid);
      break;
    case TUTOR_BOTTOM_OF_DECK_RANDOM:
    {
      char buf[200];
      sprintf(buf, "%s:%d\012illegal destination_chosen: %d", "functions/tutors.c", 902,
              (int)destination_chosen);
      backtrace_and_abort(buf);
      break;
    }
    }
  }

  if (num_cards > 0)
    switch (destination_rest)
    {
    case TUTOR_HAND:
    case TUTOR_HAND_AND_MARK:
      for (int i = 0; i < num_cards; ++i)
      {
        int card_added = add_card_to_hand(t_player, deck_ptr[t_player][i]);
        if (card_added >= 0 && destination_rest == TUTOR_HAND_AND_MARK)
          add_state(t_player, card_added, STATE_TARGETTED);
      }
      obliterate_top_n_cards_of_deck(t_player, num_cards);
      break;
    case TUTOR_DECK:
      rearrange_top_x(t_player, t_player, num_cards);
      break;
    case TUTOR_GRAVE:
      mill(t_player, num_cards);
      break;
    case TUTOR_EXILE:
      rfg_top_n_cards_of_deck(t_player, num_cards);
      break;
    case TUTOR_BOTTOM_OF_DECK:
      put_top_x_on_bottom(t_player, t_player, num_cards);
      break;
    case TUTOR_TPLAYER_HAND:
      for (int i = 0; i < num_cards; ++i)
        add_card_to_hand(1 - t_player, deck_ptr[t_player][i]);
      obliterate_top_n_cards_of_deck(t_player, num_cards);
      break;
    case TUTOR_BOTTOM_OF_DECK_RANDOM:
      put_top_x_on_bottom_in_random_order(t_player, num_cards);
      break;
    case TUTOR_PLAY:
    case TUTOR_PLAY_TAPPED:
    case TUTOR_GET_ID:
    case TUTOR_PLAY_ATTACKING:
    case TUTOR_GET_POSITION:
    {
      char buf[200];
      sprintf(buf, "%s:%d\012illegal destination_rest: %d", "functions/tutors.c", 945,
              (int)destination_rest);
      backtrace_and_abort(buf);
      break;
    }
    }

  if (num_chosen <= 0)
    return selected;
  if (destination_chosen == TUTOR_GRAVE)
  {
    for (int i = num_chosen - 1; i >= 0; --i)
      raw_put_iid_on_top_of_deck(t_player, chosen_iids[i]);
    mill(t_player, num_chosen);
    return num_chosen;
  }
  if (destination_chosen == TUTOR_PLAY || destination_chosen == TUTOR_PLAY_TAPPED ||
      destination_chosen == TUTOR_PLAY_ATTACKING)
  {
    int result = num_chosen;
    for (int i = 0; i < num_chosen; ++i)
    {
      int card_added = add_card_to_hand(src_player, chosen_iids[i]);
      set_special_flags(src_player, card_added, SF_PLAYED_FROM_DECK);
      card_instance_t *instance = get_card_instance(src_player, card_added);
      if (t_player != src_player)
        instance->state ^= STATE_OWNED_BY_OPPONENT;
      if (fn)
        fn(src_player, card_added);
      if (destination_chosen == TUTOR_PLAY_ATTACKING)
      {
        instance->state |= STATE_TAPPED | STATE_ATTACKING;
        put_into_play(src_player, card_added);
        choose_who_attack(src_player, card_added);
      }
      else
      {
        if (destination_chosen == TUTOR_PLAY_TAPPED)
          instance->state |= STATE_TAPPED;
        put_into_play(src_player, card_added);
        if (num_chosen == 1)
          result = card_added;
      }
    }
    return result;
  }
  {
  char buf[200];
  sprintf(buf, "%s:%d\012illegal destination_chosen: %d with num_chosen=%d",
          "functions/tutors.c", 999, (int)destination_chosen, num_chosen);
  backtrace_and_abort(buf);
  return num_chosen;
  }
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 library reveal and selection logic. */
/* ManalinkEh 2022: 0x2655580; original functions/tutors.c:779. */
int reveal_top_cards_of_library_and_choose(int src_player, int src_card, int t_player,
                                                int num_cards, int must_select,
                                                tutor_t destination_chosen,
                                                int reveal_chosen,
                                                tutor_t destination_rest,
                                                int reveal_rest, test_definition_t *test)
{
  int result;
  result = reveal_top_cards_of_library_and_choose_fn_who_chooses(
      src_player, src_card, t_player, src_player, num_cards, must_select, destination_chosen,
      reveal_chosen, destination_rest, reveal_rest, test, (library_reveal_callback_t)0x0);
  return result;
}

/* ManalinkEh 2022: 0x2655740; original functions/tutors.c:784.
 * Native behavior: this frontend ignores fn and passes NULL to the worker. */
int reveal_top_cards_of_library_and_choose_fn(int src_player, int src_card, int t_player,
                                                   int num_cards, int must_select,
                                                   tutor_t destination_chosen,
                                                   int reveal_chosen,
                                                   tutor_t destination_rest,
                                                   int reveal_rest, test_definition_t *test,
                                                   library_reveal_callback_t fn)
{
  int result;
  result = reveal_top_cards_of_library_and_choose_fn_who_chooses(
      src_player, src_card, t_player, src_player, num_cards, must_select, destination_chosen,
      reveal_chosen, destination_rest, reveal_rest, test, (library_reveal_callback_t)0x0);
  return result;
}

/* ManalinkEh 2022: 0x26557a0; original functions/tutors.c:790. */
int reveal_top_cards_of_library_and_choose_who_chooses(
    int src_player, int src_card, int t_player, int who_chooses, int num_cards, int must_select,
    tutor_t destination_chosen, int reveal_chosen,
    tutor_t destination_rest, int reveal_rest, test_definition_t *test)
{
  int result;
  result = reveal_top_cards_of_library_and_choose_fn_who_chooses(
      src_player, src_card, t_player, who_chooses, num_cards, must_select, destination_chosen,
      reveal_chosen, destination_rest, reveal_rest, test, (library_reveal_callback_t)0x0);
  return result;
}

/* ManalinkEh 2022: 0x26555e0; original functions/tutors.c:1009. */
int reveal_top_cards_of_library_and_choose_type(int src_player, int src_card, int t_player,
                                                     int num_cards, int must_select,
                                                     tutor_t destination_chosen,
                                                     int reveal_chosen,
                                                     tutor_t destination_rest,
                                                     int reveal_rest, type_t legal_types)
{
  test_definition_t test;
  if ((legal_types & ~TYPE_EFFECT) == TYPE_NONEFFECT || legal_types == TYPE_NONE)
    default_test_definition(&test, TYPE_ANY);
  else
  {
    new_default_test_definition(&test, legal_types, "Select a ");
    if (ai_is_speculating != 1)
    {
      int length = strlen(test.message);
      type_text(test.message + length, 100 - length, legal_types, TYPETEXT_0);
      length = strlen(test.message);
      scnprintf(test.message + length, 100 - length, " card.");
    }
  }
  return reveal_top_cards_of_library_and_choose_fn_who_chooses(
      src_player, src_card, t_player, src_player, num_cards, must_select, destination_chosen,
      reveal_chosen, destination_rest, reveal_rest, &test, NULL);
}

/* ManalinkEh 2022: 0x2651660; original functions/tutors.c:1180. */
static int select_multiple_cards_from_graveyard_to_array_part(
    int player, int targ_player, int must_select_all, int ai_selection_mode,
    test_definition_t *this_test, int max_targets, target_t *ret_location)
{
  test_definition_t default_test;
  if (!this_test)
  {
    this_test = &default_test;
    new_default_test_definition(this_test, 0, "");
    if (ai_is_speculating != 1)
    {
      if (max_targets == 1)
        strncpy(default_test.message, "Select a card.", 15);
      else if (!must_select_all)
        sprintf(this_test->message, "Select up to %d cards.", max_targets);
      else
        sprintf(this_test->message, "Select %d cards.", max_targets);
    }
    default_test.id = 904;
    default_test.id_flag = DOESNT_MATCH;
  }
  else if (!this_test->id)
  {
    this_test->id = 904;
    this_test->id_flag = DOESNT_MATCH;
  }
  else if (this_test->id != 904 || this_test->id_flag != DOESNT_MATCH)
    backtrace_and_abort(
        "functions/tutors.c:1188\nthis_test->id == 904 && this_test->id_flag == DOESNT_MATCH");

  for (int i = 0; i < max_targets; ++i)
    ret_location[i].player = ret_location[i].card = -1;
  if (graveyard_has_shroud(targ_player))
  {
    if (must_select_all)
      spell_fizzled = 1;
    return 0;
  }
  int grave_count = count_graveyard(targ_player);
  if (!grave_count || (max_targets > grave_count && must_select_all))
  {
    if (must_select_all)
      spell_fizzled = 1;
    return 0;
  }
  if (max_targets > grave_count)
    max_targets = grave_count;
  if (max_targets < 1)
  {
    qsort(ret_location, max_targets, sizeof(*ret_location), qsort_compare_target_player_descending);
    return max_targets;
  }
  int selected = 0;
  while (selected < max_targets)
  {
    int position = new_select_a_card(player, targ_player, TUTOR_FROM_GRAVE, must_select_all > 0,
                                     ai_selection_mode, -1, this_test);
    if (position == -1)
      break;
    ret_location[selected].player = position;
    ret_location[selected].card = turn_card_in_grave_face_down(targ_player, position);
    ++selected;
  }
  for (int i = 0; i < selected; ++i)
    turn_card_in_grave_face_up(targ_player, ret_location[i].player, ret_location[i].card);
  qsort(ret_location, selected, sizeof(*ret_location), qsort_compare_target_player_descending);
  if (selected < max_targets && must_select_all)
    spell_fizzled = 1;
  return selected;
}

/* ManalinkEh 2022: 0x2652b10; original functions/tutors.c:1170. */
int select_multiple_cards_from_graveyard(int player, int targ_player, int must_select_all,
                                              int ai_selection_mode,
                                              test_definition_t *this_test, int max_targets,
                                              target_t *ret_location)
{
  int result;
  if (max_targets == 0)
  {
    return 0;
  }
  if (max_targets - 1U < 0x13)
  {
    result = select_multiple_cards_from_graveyard_to_array_part(
        player, targ_player, must_select_all, ai_selection_mode, this_test, max_targets,
        ret_location);
    return result;
  }
  backtrace_and_abort("functions/tutors.c:1175\nmax_targets > 0 && max_targets <= 19");
  __builtin_unreachable();
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2652b90; original functions/tutors.c:1180. */
int select_multiple_cards_from_graveyard_to_array(int player, int targ_player,
                                                       int must_select_all, int ai_selection_mode,
                                                       test_definition_t *this_test,
                                                       int max_targets, target_t *ret_location)
{
  int result;

  if (max_targets != 0)
  {
    result = select_multiple_cards_from_graveyard_to_array_part(
        player, targ_player, must_select_all, ai_selection_mode, this_test, max_targets,
        ret_location);
    return result;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline mechanics; native callers retain their expansions. */
/* ManalinkEh 2022: 0x2652be0; original functions/tutors.c:1266. */
int select_multiple_cards_from_either_graveyard(int player, int card, select_grave_t mode,
                                                int ai_mode_own_graveyard,
                                                int ai_mode_opponent_graveyard,
                                                test_definition_t *this_test, int max_targets,
                                                target_t *ret_location)
{
  if (max_targets == 0)
    return 0;
  if ((unsigned int)max_targets - 1 > 18)
    backtrace_and_abort("functions/tutors.c:1270\nmax_targets > 0 && max_targets <= 19");

  int cannot_cancel = mode & SFG_CANNOT_CANCEL;
  test_definition_t default_test;
  if (!this_test)
  {
    this_test = &default_test;
    new_default_test_definition(this_test, 0, "");
    if (ai_is_speculating != 1)
    {
      if (max_targets == 1)
        strncpy(default_test.message, "Select a card.", 15);
      else if (!cannot_cancel)
        sprintf(this_test->message, "Select up to %d cards.", max_targets);
      else
        sprintf(this_test->message, "Select %d cards.", max_targets);
    }
    default_test.id = 904;
    default_test.id_flag = DOESNT_MATCH;
  }
  else if (!this_test->id)
  {
    this_test->id = 904;
    this_test->id_flag = DOESNT_MATCH;
  }
  else if (this_test->id != 904 || this_test->id_flag != DOESNT_MATCH)
    backtrace_and_abort(
        "functions/tutors.c:1277\nthis_test->id == 904 && this_test->id_flag == DOESNT_MATCH");

  int count0 = new_special_count_grave(0, this_test);
  int count1 = new_special_count_grave(1, this_test);
  int minimum = cannot_cancel ? max_targets : 0;
  int can_choose0 = count0 > minimum && (!graveyard_has_shroud(0) || (mode & SFG_NOTARGET));
  int can_choose1 = count1 > minimum && (!graveyard_has_shroud(1) || (mode & SFG_NOTARGET));
  int legal_count = (can_choose0 ? count0 : 0) + (can_choose1 ? count1 : 0);
  if (!legal_count)
  {
    if (cannot_cancel)
      spell_fizzled = 1;
    return 0;
  }
  if (legal_count < max_targets)
  {
    max_targets = legal_count;
    if (cannot_cancel)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  int face_down_cards[max_targets];
  for (int i = 0; i < max_targets; ++i)
    ret_location[i].player = ret_location[i].card = -1;

  int both_graveyards = can_choose0 && can_choose1;
  int selection_required = both_graveyards ? 0 : cannot_cancel;
  int retries = 3, selected = 0;
  target_t graveyard;
  graveyard.player = can_choose0 ^ 1;
  for (;;)
  {
    if (both_graveyards)
    {
      target_definition_t td;
      base_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      td.allow_cancel = (mode ^ SFG_CANNOT_CANCEL) & SFG_CANNOT_CANCEL;
      if (mode & SFG_NOTARGET)
        td.illegal_abilities = 0;
      load_text(NULL, "TARGET_GRAVEYARD");
      if (!select_target(player, card - 1000, &td, text_lines[0], &graveyard))
      {
        spell_fizzled = 1;
        for (int i = 0; i < selected; ++i)
          turn_card_in_grave_face_up(ret_location[i].player >> 16, ret_location[i].player & 0xffff,
                                     face_down_cards[i]);
        return -1;
      }
    }
    while (selected < max_targets)
    {
      if (both_graveyards)
        hack_override_show_deck_done_button_label = "Rechoose GY";
      int ai_mode = graveyard.player == player ? ai_mode_own_graveyard : ai_mode_opponent_graveyard;
      int position =
          new_select_a_card(player, graveyard.player,
                            (mode & SFG_NOTARGET) ? TUTOR_FROM_GRAVE_NOTARGET : TUTOR_FROM_GRAVE,
                            selection_required, ai_mode, -1, this_test);
      hack_override_show_deck_done_button_label = NULL;
      if (position == -1)
        break;
      ret_location[selected].player = (graveyard.player << 16) | position;
      ret_location[selected].card = graveyard_source[graveyard.player][position];
      face_down_cards[selected] = turn_card_in_grave_face_down(graveyard.player, position);
      ++selected;
    }
    if (selected >= max_targets || !both_graveyards || ai_is_speculating == 1 ||
        (player == AI && !(trace_mode & 2)) || --retries == 0)
      break;
    spell_fizzled = 0;
  }
  for (int i = 0; i < selected; ++i)
    turn_card_in_grave_face_up(ret_location[i].player >> 16, ret_location[i].player & 0xffff,
                               face_down_cards[i]);
  if (cannot_cancel && selected < max_targets)
    spell_fizzled = 1;
  return selected;
}
#endif
