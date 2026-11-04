#include <sys/stat.h>
#include <errno.h>

#include "manalink.h"
#include "platform/file_io.h"
#include "card_id.h"

#define ALIGN_COLUMNS	9	//align Manalink.csv Rearity columns with rarity bits in rarity.dat, check header *1
#define CUSTOM_SET	16	//should be 7, but =16 check header *1
#define ALIGN_IDS	7	//-7 to align Manalink.csv expansion set columns with expansions.h IDs

static int packs[24][15];
static int picks[8][100];
static unsigned int colors[8][2];
static int sets[3];
#ifdef ROTISSERIE
static int in_set[2000];
static int rotisserie[500];
#endif
typedef struct {
	int csvid;
	int rarity;
} in_set_t;
static in_set_t in_set1[3][1000];	//[set][card_id]
static int set_rs[3][3];	//set rarities [C][U] = rarity
static const char *set_name[3];

static uint32_t expansions;
static uint32_t expansion_size;
static uint8_t* rarities = NULL;
static int num_valid_expansions;

static void read_rarities(int player, int card){
	if (rarities){
		return;
	}

	SDL_IOStream *f;
	int total_cards = 0;

	if( !( f = shandalar_open_file_io("rarity.dat", "rb") ) ){
		do_dialog(HUMAN, player, card, -1, -1, "Rarity.dat missing", 0);
		lose_the_game(HUMAN);
	}
	shandalar_io_read(&total_cards, 4, 1, f);
	if( total_cards != available_slots) {
		do_dialog(HUMAN, player, card, -1, -1, "Mismatch between Rarity.dat\nand cards.dat", 0);
		lose_the_game(HUMAN);
	}
	shandalar_io_read(&expansions, 4, 1, f);
	if(!expansions) {
		do_dialog(HUMAN, player, card, -1, -1, "Invalid Rarity.dat", 0);
		lose_the_game(HUMAN);
	}
	expansion_size = (expansions * 3 + 7) / 8;
	if( !(rarities = malloc(expansion_size * total_cards + 1)) ){
		do_dialog(HUMAN, player, card, -1, -1, "Out of memory\nwhile reading Rarity.dat", 0);
		lose_the_game(HUMAN);
	}
	rarities[expansion_size * total_cards] = 0;
	shandalar_io_read(rarities, expansion_size, total_cards, f);
	shandalar_io_close(f);
}

static int check_rarity(int card_id, unsigned int expansion_id){
	if ( (card_id >= available_slots) || (expansion_id >= expansions) ){
		return 0;
	}
	int bit_pos  = (expansion_id * 3);
	int bit_mask = 7 << (bit_pos & 7);
	int offset   = (card_id * expansion_size) + (bit_pos / 8);
	return ((((uint16_t)(rarities[offset + 1]) << 8) | rarities[offset]) & bit_mask) >> (bit_pos & 0x07);
}

static void make_deck(int player){
	// go through our colors, and put in all cards that match
	int deck[45];
	int deck_index = 0;
	int i;
	for(i=0;i<100;i++){
		int card = picks[player][i];
		if( card == - 1 ){ break; }
		card_data_t* card_d = &cards_data[ get_internal_card_id_from_csv_id( card ) ];
		card_ptr_t* c = cards_ptr[ card ];
		if( c->ai_base_value > 0 ){
			if( card_d->type & TYPE_ARTIFACT && ( card_d->color == 1 || card_d->color == 0x3E )){
				deck[deck_index++]=card;
			}
			// if it's a land, only take it if it's both our colors
			else if( (card_d->type & TYPE_LAND ) && ( card_d->color == colors[player][0] + colors[player][1] ) ){
				deck[deck_index++]=card;
			}
			// only take cards in our color
			// 1 color match
			else if( card_d->color == colors[player][0] || card_d->color == colors[player][1] ){
				deck[deck_index++]=card;
			}
			// 2 color match
			else if( card_d->color == colors[player][0] + colors[player][1] ){
				deck[deck_index++]=card;
			}
		}
	}

	// if we're at more than 23 cards, trim the worst
	while( deck_index > 23 ){
		int worst_index = -1;
		int worst_score = 200;
		for(i=0;i<deck_index;i++){
			card_ptr_t* c = cards_ptr[ deck[i] ];
			if( c->ai_base_value < worst_score ){
				worst_score = c->ai_base_value;
				worst_index = i;
			}
		}
		for(i=worst_index;i<deck_index;i++){
			deck[i] = deck[i+1];
		}
		deck_index--;
	}

	// add lands (be lazy, just give half and half)
	int lands_needed = 40 - deck_index;
	int ind = 0;
	for(i=0;i<lands_needed;i++){
		if( i > lands_needed / 2 ){
			ind = 1;
		}
		switch(colors[player][ind]){
			case COLOR_TEST_BLACK :
				deck[deck_index++] = CARD_ID_SWAMP;
				break;
			case COLOR_TEST_BLUE :
				deck[deck_index++] = CARD_ID_ISLAND;
				break;
			case COLOR_TEST_GREEN :
				deck[deck_index++] = CARD_ID_FOREST;
				break;
			case COLOR_TEST_RED :
				deck[deck_index++] = CARD_ID_MOUNTAIN;
				break;
			case COLOR_TEST_WHITE :
				deck[deck_index++] = CARD_ID_PLAINS;
				break;
			default:
				{
				SDL_IOStream *file = shandalar_open_file_io("draft_errors.txt", "a+");
				shandalar_io_printf(file,"Can't add land to deck.  Player = %d, ind = %d, color= %d\n", player, ind, colors[player][ind]  );
				shandalar_io_close(file);
				break;
				}
		}

	}

	// finally put our deck into out picks slots
	for(i=0;i<40;i++){
		picks[player][i] = deck[i];
	}
	for(i=40;i<45;i++){
		picks[player][i] = -1;
	}
}

static const char* get_set_name(int setnum)
{
  static const char** all_set_names = NULL;
  if (!all_set_names)
	{
	  SDL_IOStream* f = shandalar_open_file_io("Menus.txt", "r");
	  ASSERT(f && "Could not open Menus.txt");

	  char buf[1000], *p;
	  do
		{
		  p = shandalar_io_gets(buf, 1000, f);
		  ASSERT(p && "Could not find @EXPANSIONNAMES in Menus.txt");
		} while (strcmp(buf, "@EXPANSIONNAMES\n"));

	  p = shandalar_io_gets(buf, 1000, f);
	  ASSERT(p && "Could not read number of expansions from Menus.txt");

	  num_valid_expansions = atoi(buf);
	  ASSERT(num_valid_expansions > 0 && "Could not parse number of expansions from Menus.txt");

	  all_set_names = (const char**)malloc(num_valid_expansions * sizeof(const char*));
	  ASSERT(all_set_names && "Out of memory");

	  int i;
	  for (i = 0; i < num_valid_expansions; ++i)
		{
		  p = shandalar_io_gets(buf, 1000, f);
		  if (!p)
			{
			  popup("Fatal", "Could not read expansion name #%d from Menus.txt", i);
			  abort();
			}

		  int l = strlen(buf);
		  if (l <= 1)
			{
			  popup("Fatal", "Could not read expansion name #%d from Menus.txt", i);
			  abort();
			}
		  buf[l - 1] = 0;	// strip the \n

		  all_set_names[i] = strdup(buf);
		}
	}
  ASSERT(setnum < num_valid_expansions);
  return all_set_names[setnum];
}

#ifdef ROTISSERIE
static void load_custom_set(int num_set, int set){
		int i;

		char buffer[500];
		char buffer2[5];
		SDL_IOStream *file = shandalar_open_file_io("DraftSets//sets.txt", "r");
		int line=0;

	   for(line=0; line<set-CUSTOM_SET; line++){	//find the name of the set to load, check header *1
			shandalar_io_scanf(file, "%[^\n]", buffer);
			shandalar_io_scanf(file, "%[\n]", buffer2);
		}

		shandalar_io_close(file);
		set_name[num_set] = strncpy(buffer, buffer, strlen(buffer) );

		for(i=0;i<2000;i++){
			in_set[ i ] = 0;
		}
		// load this set from the file
		char file_buffer[500];
		snprintf(file_buffer, 500, "DraftSets//%s.dck", buffer);
		SDL_IOStream *file2 = shandalar_open_file_io(file_buffer, "r");
		for(i=0;i<8;i++){
			shandalar_io_scanf(file2, "%[^\n]", buffer);
			shandalar_io_scanf(file2, "%[\n]", buffer);
		}
		// load  cards of this set
		i=0;
		while( shandalar_io_scanf(file2, "%[.]", buffer) == 1 ){
			shandalar_io_scanf(file2, "%[0-9]", buffer);
			int card_id = atoi(buffer);
			if ( is_valid_card(card_id)>0 ){	//should not be needed if set is made from the deckbuilder, but we are in transition phase
			in_set1[num_set][i].csvid = card_id;
			in_set1[num_set][i].rarity = 0;		//check_rarity1(card_id, set);
			set_rs[num_set][0]++;	//for custom sets [0] = total_cards
			i++;
			}
			shandalar_io_scanf(file2, "%[^\n]", buffer);
			shandalar_io_scanf(file2, "%[\n]", buffer);
		}

		shandalar_io_close(file2);
}
#endif

static void load_set(int player, int card, int num_set, int set){
	int set_cards =0;
	unsigned char rarity =0;
	int card_id =0;
	read_rarities(player, card);

	for (card_id=0; card_id < available_slots; card_id++) {
		if ( is_valid_card(card_id)>0 ){
			rarity = check_rarity(card_id, set);
			switch(rarity){	//lands in expansions are usually used as commons, uncommons or rares
				case COMMON:
					set_rs[num_set][0]++;
					in_set1[num_set][set_cards].csvid  = card_id;
					in_set1[num_set][set_cards].rarity  = rarity;
					set_cards++;
					break;
				case UNCOMMON:
					set_rs[num_set][1]++;
					in_set1[num_set][set_cards].csvid  = card_id;
					in_set1[num_set][set_cards].rarity  = rarity;
					set_cards++;
					break;
				case RARE:
				case MYTHIC:	//counted as rares
					set_rs[num_set][2]++;
					in_set1[num_set][set_cards].csvid  = card_id;
					in_set1[num_set][set_cards].rarity  = rarity;
					set_cards++;
					break;
				case LAND:	//ordinary lands are not included in booster packs?
				/*
					in_set1[num_set][set_cards].csvid  = card_id;
					in_set1[num_set][set_cards].rarity  = rarity;
					set_cards++;
					break;
				*/
				case NONE :
				case SPECIAL :
					break;
				default:	//UNUSED :
					{
						SDL_IOStream *file = shandalar_open_file_io("draft_errors.txt", "a+");
						shandalar_io_printf(file,"Can't add card to set=%d, CARD_ID = %d, Rarity= %d\n", set, card_id, rarity );
						shandalar_io_close(file);
						break;
					}
			}
		}
	}
}

#ifdef ROTISSERIE
static void make_rotisserie_pack(void){
	int set = sets[0];
	//FILE *file = fopen("packs.txt", "a+");
	int card;
	for(card=0;card<500;card++){
		rotisserie[card] = -1;
	}

	if( set > 7 ){
		load_custom_set(0,set);
	}

	// If this is a built-in set, go through each card and see if it is in
	// this set
	int count = 0;
	for(card=0;card<2000;card++){
		int match = 0;
		card_ptr_t* c = cards_ptr[ card ];
		if( set == 1 && c->expansion != 8 ){
			match = 1;
		}
		else if( set == 2 && c->expansion != 2048 ){
			match = 1;
		}
		else if( set == 3 && c->expansion != 4 ){
			match = 1;
		}
		else if( set == 4 && c->expansion != 2 ){
			match = 1;
		}
		else if( set == 5 && c->expansion != 256 ){
			match = 1;
		}
		else if( set == 6 && c->expansion != 32 ){
			match = 1;
		}
		else if( set == 7 && c->expansion != 64 ){
			match = 1;
		}
		else if( set > 7 && ! in_set[ card ] ){
			match = 1;
		}

		if( match == 0 && is_valid_card(card) ){
			// insert sort the cards by color then name
			//rotisserie[count++] = card;
			int idx = count;
			int i;
			for(i=0;i<count;i++){
				card_ptr_t* c1 = cards_ptr[ rotisserie[i] ];
				if( c->color < c1->color || (c->color == c1->color && strcmp( c1->name, c->name  ) > 0  ) ){
					idx = i;
					break;
				}
			}

			for(i=499;i>=idx;i--){
				rotisserie[i] = rotisserie[i-1];
			}
			rotisserie[idx] = card;
			count++;
			//fprintf(file,"Card=%s Pack %d, position: %d\n", c->name, count, idx );
		}
	}

	// print out the deck list
	int i;
	for(i=0;i<500;i++){
		//fprintf(file,"Deck %d: %d\n", i, rotisserie[i] );
	}

	//fclose(file);
}
#endif

static int get_best_card_in_pack(int player, int cards_left, int* pack){
		SDL_IOStream *file = shandalar_open_file_io("picks.txt", "w");
		// pick the card in the pack with the highest value, as long as it is
		// in our colors
		int i;
		int max_score = -1;
		int best_card = -1;
		int max_score_backup = -1;
		int best_card_backup = 0;
		for(i=0;i<cards_left;i++){

			int card = pack[i];
			shandalar_io_printf(file,"Looking at %d\n", i );
			card_ptr_t* c = cards_ptr[ card ];
			card_data_t* card_d = &cards_data[ get_internal_card_id_from_csv_id( card ) ];
			//card_data_t* card_d = &cards_data[ card ];
			//fprintf(file,"it is %d\n", card );
			// keep track of our backup pick
			int score = c->ai_base_value - internal_rand(10);
			if( score > max_score_backup ){
				max_score_backup = score;
				best_card_backup = i;
			}

			// if the card doesn't have a better AI Base value, forget it
			if( score > max_score ){
				// if this card is multicolored and we do not have both colors
				// set, skip it
				if( c->color == 4 && ( colors[player][0] < 1 || colors[player][1] < 1 ) ){
					// do nothing
				}
				// if it's an artifact, we'll take it
				else if( card_d->type & TYPE_ARTIFACT && ( card_d->color == 1 || card_d->color == 0x3E ) ){
					best_card = i;
				}
				// if it's a land, only take it if it's both our colors
				else if( card_d->type & TYPE_LAND && colors[player][0] > 0 && colors[player][1] > 0 ){
					if( card_d->color == colors[player][0] + colors[player][1] ){
						best_card = i;
					}
				}
				// if both colors are set, only take cards in our color
				else if( colors[player][0] > 0 && colors[player][1] > 0 ){
					// 1 color match
					if( card_d->color == colors[player][0] || card_d->color == colors[player][1] ){
						best_card = i;
					}
					// 2 color match
					if( card_d->color == colors[player][0] + colors[player][1] ){
						best_card = i;
					}
				}
				// if we have just 1 color, take this card
				else if( colors[player][0] > 0 ){
					best_card = i;
					if( card_d->color != colors[player][0] && (card_d->color == 2 || card_d->color == 4 || card_d->color == 8 || card_d->color == 16 || card_d->color == 32) ){
						colors[player][1] = card_d->color;
					}
				}
				else{
					best_card = i;
					if( card_d->color == 2 || card_d->color == 4 || card_d->color == 8 || card_d->color == 16 || card_d->color == 32){
						colors[player][0] = card_d->color;
					}
				}
			}
			if( best_card == i ){
				max_score = score;
			}
		}

		// If we found a card, great.  Otherwise take the best remaining card.
		if( best_card == -1 ){
			best_card = best_card_backup;
		}
		shandalar_io_printf(file,"returning %d\n", best_card );

		shandalar_io_close(file);
		return best_card;
}

static void make_AI_picks(int pack, int pick){
	// Remove a card out of each OTHER pack
	SDL_IOStream *file = shandalar_open_file_io("packs.txt", "a+");
	int player;
	for (player=1;player<8;player++){
		int which_pack = ( player + pick ) % 8;
		if( pack == 1 ){
			which_pack = pack * 8 - which_pack + 7;
		}
		else{
			which_pack = pack * 8 + which_pack;
		}
		//fprintf(file,"player %d (pick %d)is picking out of pack %d:\n", player, 15*pack+pick, which_pack );

		int best_card = get_best_card_in_pack(player, 15-pick, packs[which_pack]);

		//fprintf(file,"pick made\n" );

		int card = packs[which_pack][best_card];
		picks[player][15*pack+pick] = card;

		// remove the card from the pack
		int i;
		for (i=best_card;i<14;i++){
			packs[which_pack][i] = packs[which_pack][i+1];
		}
		packs[which_pack][14] = -1;
	}
	shandalar_io_close(file);
}

#ifdef ROTISSERIE
static int make_AI_picks_rotisserie(int player){
	int i=0;
	while( rotisserie[i] > -1 ){ i++; }
	int cards_count = i;

	int best_card = get_best_card_in_pack(player, cards_count, rotisserie );
	int card = rotisserie[best_card];

	i=0;
	while( picks[player][i] > -1 ){ i++; }
	picks[player][i] = card;

	// remove the card from the pack
	for (i=best_card;i<cards_count;i++){
		rotisserie[i] = rotisserie[i+1];
	}
	rotisserie[499] = -1;

	return card;

}
#endif

static void make_human_pick(int pack, int pick){
	int orig_pick = pick;
	pick = pick % 8;
	int which_pack = 8*pack+pick;
	if( pack == 1 ){
		which_pack = 7+8*pack-pick;
	}


	// put the cards into your deck
	int *deck = deck_ptr[HUMAN];
	int i;
	for(i=0;i<500;i++){
		if( i < 15 && packs[which_pack][i] > -1 ){
			deck[i] = get_internal_card_id_from_csv_id( packs[which_pack][i] );
		}
		else{
			deck[i] = -1;
		}
	}

	// tutor a card from your deck
	int selected = -1;
	while(selected == -1 ){
		selected = pick_card_from_deck(HUMAN);
	}

	picks[0][ 15*pack+orig_pick ] = cards_data[ deck[selected] ].id;
	for (i=selected;i<14;i++){
		packs[which_pack][i] = packs[which_pack][i+1];
	}
	packs[which_pack][14] = -1;
}

#ifdef ROTISSERIE
static void make_human_pick_rotisserie(void){
	// put the cards into your deck
	int *deck = deck_ptr[HUMAN];
	int i;
	for(i=0;i<500;i++){
		if( rotisserie[i] != -1 ){
			deck[i] = get_internal_card_id_from_csv_id( rotisserie[i] );
		}
		else{
			deck[i] = -1;
		}
	}

	// tutor a card from your deck
	int selected = -1;
	while(selected == -1 ){
		selected = pick_card_from_deck(HUMAN);
	}

	picks[0][0] = cards_data[ deck[selected] ].id;
	for (i=selected;i<500;i++){
		rotisserie[i] = rotisserie[i+1];
	}
	rotisserie[499] = -1;
}
#endif

static void review_picks(int pack){
	// put the cards into your deck
	int *deck = deck_ptr[HUMAN];
	int i;
	for(i=0;i<15*(pack+1);i++){
		deck[i] = get_internal_card_id_from_csv_id( picks[0][i] );
	}
	show_deck( HUMAN, deck_ptr[HUMAN], 500, "Review your picks", 0, gs_done_008b40e0 );
}

static void build_deck(const char *deck_name, int player){

	if(player == 0 ){
		SDL_IOStream *file2 = shandalar_open_file_io("packs.txt", "w");
		shandalar_io_printf(file2,"Building deck for player %d:\n", player );
		shandalar_io_close(file2);
	}

	SDL_IOStream *file2 = shandalar_open_file_io("packs.txt", "a+");
	shandalar_io_printf(file2,"Building deck for player %d:\n", player );

	// score the deck
	int score = 0;
	if( player > 0 ){
		int i;
		for(i=0;i<40;i++){
			shandalar_io_printf(file2, "CARD: %d, pick=%d ", i, picks[player][i] );
			card_ptr_t* c = cards_ptr[ picks[player][i] ];
			if( picks[player][i]  < 1 ){
				shandalar_io_printf(file2,"Invalid pick found: %d, pick=%d\n", i, picks[player][i] );
			}
			score += c->ai_base_value;
			shandalar_io_printf(file2,"CARD SCORE: %d\n", c->ai_base_value );
		}
	}
	shandalar_io_printf(file2,"Score: %d\n", score );
	shandalar_io_close(file2);

	char buffer[50];
	snprintf(buffer, 50, "Playdeck\\%s (%d).dck ", deck_name, score );
	SDL_IOStream *file = shandalar_open_file_io(buffer, "w");
	shandalar_io_printf(file, ";%s (%d)\n;\n;Underdogs\n;no.spam@plea.se\n;11/09/2008\n;1\n;4th Edition\n;\n\n", deck_name, score);

	snprintf(buffer, 50, "%s.dck ", deck_name );
	SDL_IOStream *file3 = shandalar_open_file_io(buffer, "w");
	shandalar_io_printf(file3, ";%s\n;\n;Underdogs\n;no.spam@plea.se\n;11/09/2008\n;1\n;4th Edition\n;\n\n", deck_name);

	int i;
	for(i=0;i<45;i++){
		if( picks[player][i] > -1){
			shandalar_io_printf(file, ".%d\t1\t%s\n", picks[player][i], cards_ptr[ picks[player][i] ]->full_name  );
			shandalar_io_printf(file3, ".%d\t1\t%s\n", picks[player][i], cards_ptr[ picks[player][i] ]->full_name  );
		}
	}
	shandalar_io_close(file);
	shandalar_io_close(file3);
}

#define SETS_PER_MENU 12

static int choose_set_submenu(int player, int card, int setnum, int start_from)
{
  if (start_from + 1 == num_valid_expansions)	// just one in the menu
	return start_from;

  char submenu[600];
  char* p = submenu;
  p += sprintf(p, "Pick a set for pack %d", setnum);
  int i;
  for (i = 0; i < SETS_PER_MENU && i + start_from < num_valid_expansions; ++i)
	p += sprintf(p, "\n %s", get_set_name(start_from + i));
  strcpy(p, "\n Initial menu");

  int set = do_dialog(HUMAN, player, card, -1, -1, submenu, 0);
  if (set >= SETS_PER_MENU)
	set = -1;
  else
	{
	  set += start_from;
	  if (set >= num_valid_expansions)	// in last submenu
		set = -1;
	}

  return set;
}

static int choose_set(int player, int card, int setnum)
{
  ++setnum;

  get_set_name(0);	// ensure Menus.txt has been read and so num_valid_expansions is valid

  // add on custom sets
  char initial_menu[600];
  char* p = initial_menu;
  p += sprintf(initial_menu, "Pick a set for pack %d", setnum);

  int base, hi;
  for (base = ALIGN_COLUMNS; base < num_valid_expansions; base += SETS_PER_MENU)
	{
	  hi = base + SETS_PER_MENU - 1;
	  if (hi >= num_valid_expansions)
		hi = num_valid_expansions - 1;

	  if (hi == base)
		p += sprintf(p, "\n %s", get_set_name(base));
	  else
		p += sprintf(p, "\n %s - %s", get_set_name(base), get_set_name(hi));
	}

  int set = -1;
  while (set == -1)
	{
	  int submenu = do_dialog(HUMAN, player, card, -1, -1, initial_menu, 0);
	  set = choose_set_submenu(player, card, setnum, submenu * SETS_PER_MENU + ALIGN_COLUMNS);
	}

  return set;
}
