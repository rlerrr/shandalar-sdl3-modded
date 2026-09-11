#include "manalink.h"

int mulligans_complete[2];

// shuffle the deck
void shuffle_vanguard(int player, int cheat)
{
  shuffle(player);

  // if the top 5 cards of the opponent's deck don't include at least 2 lands, reshuffle the deck.  Up to 10 tries.
  if (player == AI && cheat == 1)
	{
	  int tries;
	  int* deck = deck_ptr[player];
	  for (tries = 0; tries < 10; ++tries)
		{
		  int i, land_count = 0;
		  for (i = 0; i < 5; ++i)
			if (deck[i] == -1 || (cards_data[deck[i]].type & TYPE_LAND))
			  ++land_count;

		  if (land_count >= 2)
			break;
		  else if (ai_is_speculating == 1)
			shuffle(player);
		  else
			{
			  // Forcibly avoid the animation
			  int old_ai_is_speculating = ai_is_speculating;
			  ai_is_speculating = 1;
			  shuffle(player);
			  ai_is_speculating = old_ai_is_speculating;
			}
		}
	}
}

static int create_spat_deck(int player){
	// put AI cards back on deck
	// put every non-avatar card back into the deck
	int count = 0;
	while( count < active_cards_count[AI]){
		if(! in_play(AI, count) ){
			put_on_top_of_deck(AI, count);
		}
		count++;
	}

	// rfg opponent's deck
	int *my_deck = deck_ptr[HUMAN];
	int *deck = deck_ptr[AI];
	int i;
	for(i = 0; i < 500; i++){
		deck[i] = -1;
	}

	// deal out the new deck
	int deck_size = count_deck(player);
	for( i=0;i<deck_size/2;i++){
		deck[i] = my_deck[0];
		remove_card_from_deck(player, 0 );
	}

	// give the AI 7 new cards
	draw_cards(AI, 7);

	return 0;
}

static int get_random_card_special(int min, int max, int colors, int no_mana_producer_land, int max_cmc, int req_type){

	int diff = (max-min)/10;
	int result = min;
	int k;
	int rounds = internal_rand(diff);
	for(k=0;k<rounds;k++){
		result+=internal_rand(10);
	}
	if( result > max ){
		result = min;
	}
	while( 1 ){
		if( is_valid_card(result) ){
			int crd = get_internal_card_id_from_csv_id(result);
			int test = 0;
			int card_test = 0;
			if( colors > 0 ){
				test+=1;
			}
			if( max_cmc > -1 ){
				test+=2;
			}
			if( req_type > 0 ){
				test+=4;
			}
			if( colors > 0 ){
				if( evaluate_colors(colors, cards_data[crd].color) ){
					card_test+=1;
				}
			}
			if( max_cmc > -1 ){
				if( get_cmc_by_id(result) <= max_cmc ){
					card_test+=2;
				}
			}
			if( req_type > 0 ){
				if( is_what(-1, crd, req_type) ){
					card_test+=4;
				}
			}
			/*
			else{
				if( is_what(-1, crd, TYPE_LAND) ){
					test+=8;
					if( is_mana_producer_land(-1, crd) && ! no_mana_producer_land ){
						if( evaluate_colors(colors, cards_data[crd].color) ){
							card_test+=8;
						}
					}
				}
				if( is_what(-1, crd, TYPE_ARTIFACT) && ! is_what(-1, crd, TYPE_CREATURE) ){
					if( cards_data[crd].extra_ability & EA_MANA_SOURCE ){
						test+=16;
						if( evaluate_colors(colors, cards_data[crd].color) ){
							card_test+=16;
						}
					}
				}
			}
			*/
			if( test == card_test ){
				break;
			}
		}
		result++;
		if( result > max ){
			result = min;
		}
	}
	return result;
}


static int create_random_singleton_deck(int player, int colors, int min, int max, int decksize){

	// The standard formula for a deck is 60% cards / 40% lands

	int cards = decksize * 0.6;
	int lands = decksize-cards;
	int ids[available_slots];
	int i;
	for(i=0;i<available_slots;i++){
		ids[i] = 0;
	}
	int clrs[5] = {0, 0, 0, 0, 0};
	int total_colors = 0;
	int b_lands[5] = {CARD_ID_SWAMP, CARD_ID_ISLAND, CARD_ID_FOREST, CARD_ID_MOUNTAIN, CARD_ID_PLAINS};
	if( colors == -1 ){
		colors = 2;
		int rnd = internal_rand(5);
		if( rnd == 1 ){
			colors = 4;
		}
		if( rnd == 2 ){
			colors = 8;
		}
		if( rnd == 3 ){
			colors = 16;
		}
		if( rnd > 3 ){
			colors = 32;
		}
	}

	if( colors & 2 ){
		clrs[0] = 2;
		total_colors++;
	}
	if( colors & 4 ){
		clrs[1] = 4;
		total_colors++;
	}
	if( colors & 8 ){
		clrs[2] = 8;
		total_colors++;
	}
	if( colors & 16 ){
		clrs[3] = 16;
		total_colors++;
	}
	if( colors & 32 ){
		clrs[4] = 32;
		total_colors++;
	}
	i=0;
	int *deck = deck_ptr[player];
	while(i<decksize){
			if( i<cards ){
				int rand_card;
				int req_type = 0;
				if( i <= 19 ){
					req_type = TYPE_CREATURE;
				}
				if( i <= 9 ){
					rand_card = get_random_card_special(min, max, colors, 0, 1, req_type);
				}
				else if( i >= 10 && i <= 16 ){
						rand_card = get_random_card_special(min, max, colors, 0, 2, req_type);
				}
				else if( i >= 17 && i <= 23 ){
						rand_card = get_random_card_special(min, max, colors, 0, 3, req_type);
				}
				else{
					rand_card = get_random_card_special(min, max, colors, 0, -1, req_type);
				}
				if( ids[rand_card] != 1 ){
					ids[rand_card] = 1;
					int crd = get_internal_card_id_from_csv_id(rand_card);
					if( is_mana_producer_land(-1, crd) ){
						lands--;
						cards++;
					}
					deck[i] = crd;
					i++;
				}
			}
			else{
				int k;
				for(k=0;k<5;k++){
					if( clrs[k] > 0 ){
						deck[i] = get_internal_card_id_from_csv_id(b_lands[k]);
						i++;
					}
				}
			}
	}

	// shuffle the deck
	shuffle_vanguard( player, 1 );

	recalculate_rules_engine_and_deadbox();

	return 0;
}

static int get_random_card_special2(int player, int card, int min, int max, int colors, int no_mana_producer_land, int max_cmc, int req_type){

	int result = min+1;

	if( result > max ){
		result = min;
	}
	while( 1 ){
		if( is_valid_card(result) ){
			int crd = get_internal_card_id_from_csv_id(result);
			int test = 0;
			int card_test = 0;
			if( colors > 0 ){
				test+=1;
			}
			if( max_cmc > -1 ){
				test+=2;
			}
			if( req_type > 0 ){
				test+=4;
			}
			if( colors > 0 ){
				if( evaluate_colors(colors, cards_data[crd].color) ){
					card_test+=1;
				}
			}
			if( max_cmc > -1 ){
				if( get_cmc_by_id(result) <= max_cmc ){
					card_test+=2;
				}
			}
			if( req_type > 0 ){
				if( is_what(-1, crd, req_type) ){
					card_test+=4;
				}
			}
			/*
			else{
				if( is_what(-1, crd, TYPE_LAND) ){
					test+=8;
					if( is_mana_producer_land(-1, crd) && ! no_mana_producer_land ){
						if( evaluate_colors(colors, cards_data[crd].color) ){
							card_test+=8;
						}
					}
				}
				if( is_what(-1, crd, TYPE_ARTIFACT) && ! is_what(-1, crd, TYPE_CREATURE) ){
					if( cards_data[crd].extra_ability & EA_MANA_SOURCE ){
						test+=16;
						if( evaluate_colors(colors, cards_data[crd].color) ){
							card_test+=16;
						}
					}
				}
			}
			*/
			if( test == card_test ){
				break;
			}
		}
		result++;
		if( result > max ){
			return -1;
		}
	}
	return result;
}

int create_random_test_deck(int player, int card, int colors, int min, int max, int decksize){


	// The standard formula for a deck is 60% cards / 40% lands

	int cards = decksize * 0.6;
	int lands = decksize-cards;
	int ids[available_slots];
	int i;
	for(i=0;i<available_slots;i++){
		ids[i] = 0;
	}
	int b_lands[5] = {CARD_ID_SWAMP, CARD_ID_ISLAND, CARD_ID_FOREST, CARD_ID_MOUNTAIN, CARD_ID_PLAINS};
	if( colors == -1 ){
		colors = 2;
		int rnd = internal_rand(5);
		if( rnd == 1 ){
			colors = 4;
		}
		if( rnd == 2 ){
			colors = 8;
		}
		if( rnd == 3 ){
			colors = 16;
		}
		if( rnd > 3 ){
			colors = 32;
		}
	}

	i=0;
	int z;
	int orig_min = min;
	for(z=0; z<internal_rand(50)+1; z++){
		min+=internal_rand(50)+1;
	}
	int *deck = deck_ptr[player];
	while(i<decksize){
			if( i<cards ){
				int rand_card = -1;
				int req_type = 0;
				if( i <= 19 ){
					req_type = TYPE_CREATURE;
				}
				rand_card = get_random_card_special2(player, card, min, max, colors, 0, -1, req_type);
				if( rand_card != -1 ){
					min = rand_card;
					if( ids[rand_card] < 4 ){
						ids[rand_card]++;
						int crd = get_internal_card_id_from_csv_id(rand_card);
						if( is_mana_producer_land(-1, crd) ){
							lands--;
							cards++;
						}
						deck[i] = crd;
						i++;
					}
				}
				else{
					min = orig_min;
				}
			}
			else{
				int k;
				for(k=1;k<6;k++){
					if( colors & (1<<k) ){
						deck[i] = get_internal_card_id_from_csv_id(b_lands[k-1]);
						i++;
					}
				}
			}
	}

	// shuffle the deck
	shuffle_vanguard( player, 1 );

	recalculate_rules_engine_and_deadbox();

	return 0;
}

static int create_internal_random_basic_deck(int player){
	int b_lands[5] = {CARD_ID_SWAMP, CARD_ID_ISLAND, CARD_ID_FOREST, CARD_ID_MOUNTAIN, CARD_ID_PLAINS};

	// add basic land
	int *deck = deck_ptr[player];
	int i;
	for(i=0;i<60;i++){
		deck[i] = get_internal_card_id_from_csv_id( b_lands[internal_rand(5)] );
	}

	// shuffle the deck
	shuffle_vanguard( player, 1 );

	return 0;
}

static int effect_prodigal_sorcerer(int player, int card ){
	int *deck = deck_ptr[player];
	int card_added = add_card_to_hand(player, deck[0] );
	card_instance_t *target = get_card_instance(player, card_added);
	target->state |= STATE_INVISIBLE;
	--hand_count[player];
	// The Ai will always mill
	int choice = 1;
	if( player == HUMAN ){
		choice = do_dialog(player, player, card_added, -1, -1, " Keep card\n Put card in graveyard", 0);
	}
	if( choice == 1 ){
		remove_card_from_deck( player, 0 );
		kill_card(player, card_added, KILL_DESTROY );
	}
	else{
		obliterate_card(player, card_added);
	}
	return 0;
}

static void raw_put_hand_in_deck(int player)
{
  // n, not n^2, and only force redisplay at the end instead of after every card
  int bottom = count_deck(player);
  card_instance_t* inst;
  int c = 0;
  for (c = 0; c < active_cards_count[player]; ++c)
	if ((inst = in_hand(player, c)))
	  {
		deck_ptr[player][bottom++] = inst->original_internal_card_id;
		inst->state |= STATE_INVISIBLE;	// So obliterate_card doesn't put it on top of the deck then remove it from there, which is what we're trying to avoid in the first place
		--hand_count[player];
		obliterate_card(player, c);
	  }

  if (ai_is_speculating != 1)
	request_duel_display_refresh();	// force redisplay
}

int vanguard_card(int player, int card, event_t event, int cards, int starting_life, int special){
	// set the max hand size
	if(event == EVENT_MAX_HAND_SIZE && current_turn == player){
		event_result = cards;
	}

	indestructible(player, card, event);

	// perform the first turn mulligans
	if( mulligans_complete[player] != 1 && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card)
	    && reason_for_trigger_controller == player && ai_is_speculating != 1 ){
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
			// set the deck from challenge mode, if applicable
			if( get_setting(SETTING_CHALLENGE_MODE) ){
				challenge_mode_upkeep(1);
			}

			// set the life total
			set_starting_life_total(player, starting_life);
			mulligans_complete[player] = 1;

			// put every non-avatar card back into the deck
			raw_put_hand_in_deck(player);

			// remove any remaining avatars (and generals) from the deck
			int *deck = deck_ptr[player];
			int i;
			card_instance_t *instance = get_card_instance(player, card);
			int general_csvid = 0;
			if( special == 6 ){
				general_csvid = cards_data[instance->info_slot].id;
			}
			int counter=0;
			for(i = 0; i < 500; i++){
				if(deck[i] != -1 ){
					if ( cards_data[  deck[i] ].cc[2] == 3 || ( general_csvid > 0 && cards_data[  deck[i] ].id == general_csvid ) ){
						if( counter++ > 1 && cards_data[  deck[i] ].cc[2] == 3 && general_csvid == 0 ){ life[player] = counter; }
						remove_card_from_deck( player, i );
						i--;
					}
				}
			}

			// internal_randomize the deck if that's our thing
			int skip_mulligans = 0;
			if( special == 4 ){
				create_random_test_deck(player, card, color_to_color_test(internal_rand(5)+1), 0, available_slots-1, 60);
			}
			else if( special == 5 ){
				create_spat_deck(player);
				skip_mulligans = 1;
			}
			else if( special == 9 ){
					 int clrs = 0;
					 int choice2 = internal_rand(3);
					 if( player != AI ){
						 choice2 = do_dialog(player, player, card, -1, -1, " Monocolored deck\n Two-colors deck\n Three-colors deck", 0);
					 }
					 choice2++;
					 int k;
					 for(k=0; k<choice2; k++){
						 if( player != AI ){
							 clrs |= select_a_color(player);
						 }
						 else{
							  clrs |= color_to_color_test(internal_rand(5)+1);
						 }
					 }
					choice2 = 0;
					int min = 0;
					int max = available_slots;
					if( player == AI ){
						choice2 = 22+internal_rand(2);
					}
					else{
						while( 1 ){
								choice2 = do_dialog(player, player, card, -1, -1, " Everything\n Unlimited\n DK+AN+AQ\n Legends\n 8th Edition\n FE + Homelands\n Ice Age Block\n Mirage block\n Tempest block\n Urza block\n Zendikar block\n Scars block\n Innistrad block\n More sets", 0);
								if( choice2 != 13 ){
									break;
								}
								else{
									choice2 = do_dialog(player, player, card, -1, -1, " Previous expansions\n Masques Block\n Mirrodin Block\n Kamigawa Block\n M13\n Ravnica block\n Lorwyn block\n Shadowmoor block\n Shards block\n M10\n Return to Ravnica block", 0);
									if( choice2 != 0 ){
										choice2+=13;
										break;
									}
								}
						}
					}
					if( choice2 == 1 ){
						min = CARD_ID_AIR_ELEMENTAL;
						max = CARD_ID_ZOMBIE_MASTER;
					}
					if( choice2 == 2 ){
						min = CARD_ID_AMNESIA;
						max = CARD_ID_YOTIAN_SOLDIER;
					}
					if( choice2 == 3 ){
						min = CARD_ID_ABOMINATION;
						max = CARD_ID_ZEPHYR_FALCON;
					}
					if( choice2 == 4 ){
						min = CARD_ID_ANGEL_OF_MERCY;
						max = CARD_ID_YAVIMAYA_ENCHANTRESS;
					}
					if( choice2 == 5 ){
						min = CARD_ID_AEOLIPILE;
						max = CARD_ID_WIZARDS_SCHOOL;
					}
					if( choice2 == 6 ){
						min = CARD_ID_ADARKAR_SENTINEL ;
						max = CARD_ID_ZUR_THE_ENCHANTER;
					}
					if( choice2 == 7 ){
						min = CARD_ID_ABYSSAL_HUNTER ;
						max = CARD_ID_ZOMBIE_SCAVENGERS;
					}
					if( choice2 == 8 ){
						min = CARD_ID_ABANDON_HOPE ;
						max = CARD_ID_ZEALOTS_EN_DAL;
					}
					if( choice2 == 9 ){
						min = CARD_ID_ABSOLUTE_GRACE  ;
						max = CARD_ID_YAWGMOTHS_BARGAIN;
					}
					if( choice2 == 10 ){
						min = CARD_ID_ADVENTURING_GEAR;
						max = CARD_ID_ZULAPORT_ENFORCER ;
					}
					if( choice2 == 11 ){
						min = CARD_ID_ABUNA_ACOLYTE;
						max = CARD_ID_XENOGRAFT ;
					}
					if( choice2 == 12 ){
						min = CARD_ID_ABATTOIR_GHOUL;
						max = CARD_ID_ZEALOUS_STRIKE ;
					}
					if( choice2 == 14 ){
						min = CARD_ID_AERIAL_CARAVAN;
						max = CARD_ID_ZERAPA_MINOTAUR;
					}
					if( choice2 == 15 ){
						min = CARD_ID_ALTAR_OF_SHADOWS;
						max = CARD_ID_VIRIDIAN_SCOUT;
					}
					if( choice2 == 16 ){
						min = CARD_ID_AKKI_AVALANCHERS;
						max = CARD_ID_YUKI_ONNA;
					}
					if( choice2 == 17 ){
						min = CARD_ID_AJANI_CALLER_OF_THE_PRIDE;
						max = CARD_ID_YEVAS_FORCEMAGE;
					}
					if( choice2 == 18 ){
						min = CARD_ID_AGRUS_KOS_WOJEK_VETERAN ;
						max = CARD_ID_WRIT_OF_PASSAGE;
					}
					if( choice2 == 19 ){
						min = CARD_ID_ADDER_STAFF_BOGGART;
						max = CARD_ID_WOLF_SKULL_SHAMAN;
					}
					if( choice2 == 20 ){
						min = CARD_ID_ADVICE_FROM_THE_FAE;
						max = CARD_ID_WORM_HARVEST;
					}
					if( choice2 == 21 ){
						min = CARD_ID_AD_NAUSEAM;
						max = CARD_ID_ZEALOUS_PERSECUTION;
					}
					if( choice2 == 22 ){
						min = CARD_ID_ACIDIC_SLIME;
						max = CARD_ID_ZOMBIE_GOLIATH;
					}
					if( choice2 == 23 ){
						min = CARD_ID_ABRUPT_DECAY;
						max = CARD_ID_ZHUR_TAA_DRUID;
					}
					create_random_test_deck(player, card, clrs, min, max, 60);
			}
			else if( special == 10 ){ // Face the horde
					int choice = do_dialog(HUMAN, player, card, -1, -1, " Level: Standard\n Level: Legacy\n Legal: Vintage\n Level: Illegal deck", 0);
					get_card_instance(player, card)->targets[0].player = 1; // Zombie horde
					get_card_instance(player, card)->targets[0].card = 2+choice;
			}
			else if( special == 11 ){ // Hidden agenda
					call_card_function(player, card, EVENT_HIDDEN_AGENDA);
			}

			// if we are playing momir basic, give both players a deck of
			// basic lands
			if( get_momir()){
				create_internal_random_basic_deck(player);
				skip_mulligans = 1;
			}
			else if( get_setting(SETTING_CHALLENGE_MODE) && get_challenge_round() == 13 ){
				//gain_life(AI, 17);
				create_internal_random_basic_deck(player);
				skip_mulligans = 1;
			}
			if( general_csvid > 1 ){
				shuffle_vanguard(player, 1);
				draw_cards(player, cards);
				if( player == HUMAN ){
					test_definition_t this_test;
					new_default_test_definition(&this_test, 0, "Select a card to exile.");

					int mulliganed_cards[200];	// More than safe, since deck should've been flagged illegal if it didn't contain exactly 100 cards
					memset(mulliganed_cards, -1, sizeof(mulliganed_cards));
					int* mull = &mulliganed_cards[0];
					while (hand_count[player] > 0 && do_dialog(player, player, card, -1, -1, " Keep\n EDH Mulligan", 0) == 1){
						int num_exiled = 0, selected;
						while (hand_count[player] > 0 && (selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test)) != -1){
							*mull++ = get_card_instance(player, selected)->internal_card_id;
							obliterate_card(player, selected);	// not kill_card(...KILL_EXILE), since it's exiled face-down
							play_sound_effect(WAV_DESTROY);	// the exile sound effect, despite its name
							num_exiled++;
						}
						if (num_exiled > 1){
							draw_cards(player, num_exiled - 1);
						}
					}
					if (mulliganed_cards[0] != -1){
						int bottom = count_deck(player);
						for (mull = &mulliganed_cards[0]; *mull != -1; ++mull){
							deck_ptr[player][bottom++] = *mull;
						}
						shuffle(player);
					}
				}
			}
			else{
				int max_hand = cards;
				// perform any mulliganing
				while(max_hand > 0 ){
					// draw the appropriate number of cards
					shuffle_vanguard(player, 1);
					draw_cards(player, max_hand);

					// allow the choice to mulligan
					int choice = 0;
					if( player == HUMAN && ! skip_mulligans ){
						choice = do_dialog(player, player, card, -1, -1, " Keep\n Mulligan", 0);
						if( choice == 1 ){
							max_hand--;
							raw_put_hand_in_deck(player);
						}
						else{
							break;
						}
					} else {
						break;
					}
				}
			}

			// elvish champion
			if( special == 1 ){
				token_generation_t token;
				default_token_definition(player, card, CARD_ID_ELF, &token);
				token.action = TOKEN_ACTION_HASTE;
				token.special_infos = 66;
				generate_token(&token);
			}
			// royal assassin
			else if( special == 2 && current_turn == player ){
					lose_life(player, 1);
					draw_a_card(player);
			}
			// prodigal_sorcerer
			else if( special == 3 && current_turn == player ){
				effect_prodigal_sorcerer(player, card);
			}
			// chaos orb - give the AI a Sol Ring
			else if( special == 4 && player == AI && cards > 7){
					add_card_to_hand( player, get_internal_card_id_from_csv_id(CARD_ID_SOL_RING));
			}
			// Planechase - get the first Plane
			else if( special == 8 ){
					if( player == HUMAN ){
						planeswalk_to(player, get_new_plane(-1), 1);
					}
			}
			else if( special == 9 ){
					obliterate_card(player, card);
			}
		}
	}

	return 0;
}

static int legacy_avatar(int player, int card, event_t event, int (*func_ptr)(int, int, event_t) ){
	if( comes_into_play(player, card, event) > 0 ){
		create_legacy_effect(player, card, func_ptr);
		kill_card(player, card, KILL_EXILE);
		purge_rfg(player);
	}
	return 0;
}


static int effect_avatar_oni_of_wild_places(int player, int card, event_t event){
	vanguard_card(player, card, event, 7, 18, 0);

	boost_subtype(player, card, event, -1, 0,0, 0,SP_KEYWORD_HASTE, BCT_CONTROLLER_ONLY|BCT_INCLUDE_SELF);

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && current_turn == player
	       && reason_for_trigger_controller == player){
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.allowed_controller = player;
			td.preferred_controller = player;
			td.illegal_abilities = 0;
			td.allow_cancel = 0;
			if( target_available(player, card, &td) ){
				select_target(player, card, &td, "Choose a creature to bounce", NULL);
				card_instance_t *instance = get_card_instance(player, card);
				bounce_permanent( instance->targets[0].player, instance->targets[0].card );
			}
		}
	}
	return 0;
}

static int effect_avatar_elvish_champion(int player, int card, event_t event){
	vanguard_card(player, card, event, 7, 15, 1);
	return 0;
}

static int get_random_creature(int cost){
	int q;
	int crd = internal_rand(available_slots/100)+1;
	for(q=0; q<100; q++){
		crd += internal_rand(available_slots/100);
	}
	if( crd > (available_slots-1) ){
		crd = 0;
	}

	while(1){
			if( is_valid_card(crd) && get_cmc_by_id(crd) == cost ){
				if( crd != CARD_ID_SHIVAN_WURM && crd != CARD_ID_MAN_O_WAR && crd != CARD_ID_AETHER_ADEPT &&
					crd != CARD_ID_COUNTRYSIDE_CRUSHER
				  ){
					int fake = get_internal_card_id_from_csv_id(crd);
					if( is_what(-1, fake, TYPE_CREATURE) ){
						break;
					}
				}
			}
			crd++;
			if( crd > (available_slots-1) ){
				crd = 0;
			}
	}
	return crd;
}

int card_momir(int player, int card, event_t event){
	vanguard_card(player, card, event, 7, 24, 0);
	int AI_max = 10;
	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_ACTIVATE){
		//Can only active at sorcery speed
		if( ! can_sorcery_be_played(player, event) ){
			return 0;
		}

		// A card in hand is required
		if( hand_count[player] < 1 ){
			return 0;
		}

		//Can only be played once per turn
		if( instance->targets[3].card > -1 ){
			return 0;
		}

		// AI requires at least 2 mana
		if( player == AI ){
			// Don't let the AI activate if they
			// have 2 or more cards in hand, but have not played a land yet
			// (unless they are already at max mana)
			if( lands_played < 1 && hand_count[player] > 1 && ! has_mana( player, COLOR_ANY, AI_max) ){
				return 0;
			}

			if( has_mana( player, COLOR_ANY, 2 )){
				return 1;
			}
		}

		// Human can active for any number of mana
		else{
			return 1;
		}
	}
	else if(event == EVENT_ACTIVATE){
		// For the AI, use the most mana we have, from 2-max
		int choice = -1;
		if( player == AI ){
			int i = AI_max;
			while(i > 1){
				if( has_mana( player, COLOR_ANY, i) ){
					charge_mana( player, COLOR_COLORLESS, i);
					choice = i;
					break;
				}
				i--;
			}
		}
		else{
			charge_mana( player, COLOR_COLORLESS, -1);
			choice = x_value;
		}
		if( spell_fizzled != 1){
			instance->targets[3].card = choice;
			discard(player, 0, 0);
		}
	}
	else if(event == EVENT_RESOLVE_ACTIVATION ){
		// if would be nice if we could reveal the creature here
		int ran = get_random_creature( instance->targets[3].card );
		if( ran > -1 ){
			int card_added = add_card_to_hand( player, get_internal_card_id_from_csv_id( ran ) );
			convert_to_token(player, card_added);
			put_into_play(player, card_added);
			card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
			if( parent->targets[4].card != -1 ){
				kill_card(player, parent->targets[4].card, KILL_EXILE);
			}
			parent->targets[4].card = create_card_name_legacy(player, instance->parent_card, ran);
		}
	}
	else if( event == EVENT_CLEANUP && current_turn == player ){
			instance->targets[3].card = -1;
	}
	return 0;
}

int card_avatar_oni_of_wild_places(int player, int card, event_t event){
	return legacy_avatar(player, card, event, &effect_avatar_oni_of_wild_places );
}


int card_avatar_elvish_champion(int player, int card, event_t event){
	return legacy_avatar(player, card, event, &effect_avatar_elvish_champion );
}

static int avatar_akroma_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	if( event == EVENT_ABILITIES && affect_me(instance->targets[0].player, instance->targets[0].card) ){
		int ability1 = instance->targets[1].player;
		int ability2 = instance->targets[1].card;
		if( ability1 == 0 || ability2 == 0 ){
			haste(instance->targets[0].player, instance->targets[0].card, event);
		}
		if( ability1 == 1 || ability2 == 1 ){
			vigilance(instance->targets[0].player, instance->targets[0].card, event);
		}
		if( ability1 == 2 || ability2 == 2 ){
			event_result |= KEYWORD_PROT_BLACK;
		}
		if( ability1 == 3 || ability2 == 3 ){
			event_result |= KEYWORD_PROT_RED;
		}
		if( ability1 == 4 || ability2 == 4 ){
			event_result |= KEYWORD_FLYING;
		}
		if( ability1 == 5 || ability2 == 5 ){
			event_result |= KEYWORD_TRAMPLE;
		}
		if( ability1 == 6 || ability2 == 6 ){
			event_result |= KEYWORD_FIRST_STRIKE;
		}
	}
	return 0;
}

static int effect_avatar_akroma(int player, int card, event_t event){
	vanguard_card(player, card, event, 8, 27, 0);
	if(trigger_cause_controller == player && trigger_condition == TRIGGER_COMES_INTO_PLAY &&
		affect_me( trigger_cause_controller, trigger_cause ) &&
		reason_for_trigger_controller == affected_card_controller
	  ){
		int trig = 0;
		if( is_what(trigger_cause_controller, trigger_cause, TYPE_CREATURE) ){
			trig = 1;
		}

		if( trig == 1 && (check_battlefield_for_id(ANYBODY, CARD_ID_HUSHWING_GRYFF)
						  || check_battlefield_for_id(ANYBODY, CARD_ID_TOCATLI_HONOR_GUARD)
						  || check_battlefield_for_id(ANYBODY, CARD_ID_TORPOR_ORB)
						  ) ){
			trig = 0;
		}

		if( trig == 1 ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
				int internal_rand1 = internal_rand(7);
				int internal_rand2 = internal_rand(7);
				while( internal_rand2 == internal_rand1 ){
					internal_rand2 = internal_rand(7);
				}
				int legacy_card = create_targetted_legacy_effect(player, card, &avatar_akroma_legacy, trigger_cause_controller, trigger_cause);
				card_instance_t *legacy_instance = get_card_instance(player, legacy_card);
				legacy_instance->targets[1].player = internal_rand1;
				legacy_instance->targets[1].card = internal_rand2;
				legacy_instance->targets[0].player = player;
				legacy_instance->targets[0].card = trigger_cause;
			}
		}
	}

	return 0;
}

int card_avatar_akroma(int player, int card, event_t event){
	return legacy_avatar(player, card, event, &effect_avatar_akroma );
}

static int effect_avatar_erhnam_djinn(int player, int card, event_t event){
	vanguard_card(player, card, event, 7, 23, 0);
	if(trigger_condition == TRIGGER_SPELL_CAST && player == affected_card_controller
	   && player == reason_for_trigger_controller && player == trigger_cause_controller && card == affected_card )
	{
		card_instance_t *instance = get_card_instance(trigger_cause_controller, trigger_cause);
		if( cards_data[ instance->internal_card_id].type & TYPE_CREATURE ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
				generate_token_by_id(player, card, CARD_ID_SAPROLING);
			}
		}
	}
	return 0;
}

int card_avatar_erhnam_djinn(int player, int card, event_t event){
	/* ** AVATAR - Erhnam Djinn	""
	 * Vanguard Avatar
	 * Life: +3, Hand: +0
	 * Whenever you play a creature spell, put a 1/1 |Sgreen Saproling creature token into play. */

	return legacy_avatar(player, card, event, &effect_avatar_erhnam_djinn );
}

int effect_avatar_goblin_warchief(int player, int card, event_t event){
	vanguard_card(player, card, event, 8, 22, 0);
	return card_orcish_oriflamme(player, card, event);
}

static void get_horde_cards(int player, int card, int horde_type, int amount){
	while( amount ){
			int rnd = internal_rand(100);
			if( horde_type == 1 ){ // Zombies
				if( rnd <= 60 ){
					token_generation_t token;
					default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
					token.pow = 2;
					token.tou = 2;
					generate_token(&token);
				}
				if( rnd > 60 && rnd <70 ){
					token_generation_t token;
					default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
					token.pow = 5;
					token.tou = 5;
					generate_token(&token);
				}
				if( rnd > 70 && rnd < 86){
					static int zombie_horde_special_cards[30] =
					{
					CARD_ID_BAD_MOON,
					CARD_ID_UNDEAD_WARCHIEF,
					CARD_ID_PHYREXIAN_GARGANTUA,
					CARD_ID_CEMETERY_REAPER,
					CARD_ID_GRAVEBORN_MUSE,
					CARD_ID_SKINRENDER,
					CARD_ID_GRAVE_TITAN,
					CARD_ID_COAT_OF_ARMS,
					CARD_ID_FERVOR,
					CARD_ID_HOWLING_MINE,
					CARD_ID_SHEOLDRED_WHISPERING_ONE,
					CARD_ID_FERVENT_CHARGE,
					CARD_ID_ENDLESS_RANKS_OF_THE_DEAD,
					CARD_ID_CALL_TO_THE_GRAVE,
					CARD_ID_NOXIOUS_GHOUL,
					CARD_ID_SHEPHERD_OF_ROT,
					CARD_ID_VENGEFUL_DEAD,
					CARD_ID_PLAGUE_WIND,
					CARD_ID_DECREE_OF_PAIN,
					CARD_ID_OVERWHELMING_FORCES,
					CARD_ID_OVERWHELMING_STAMPEDE,
					CARD_ID_ANCESTRAL_RECALL,
					CARD_ID_SIGN_IN_BLOOD,
					CARD_ID_READ_THE_BONES,
					CARD_ID_AMBITIONS_COST,
					CARD_ID_FATAL_LORE,
					CARD_ID_TIDINGS,
					CARD_ID_TIME_STRETCH,
					CARD_ID_TIME_WALK,
					CARD_ID_PARALLEL_EVOLUTION,
					};
					copy_spell(player, zombie_horde_special_cards[rnd-70]);
				}
			}
			amount--;
	}
}

int card_face_the_horde(int player, int card, event_t event){
	// Used externally
	if (trigger_condition == TRIGGER_REPLACE_CARD_DRAW
		&& reason_for_trigger_controller == player
		&& affect_me(player, card)
		&& !suppress_draw
	   ){
		if (event == EVENT_TRIGGER){
			event_result = RESOLVE_TRIGGER_MANDATORY;
		}
		else if (event == EVENT_RESOLVE_TRIGGER){
				card_instance_t *instance = get_card_instance(player, card);
				get_horde_cards(player, card, instance->targets[0].player, instance->targets[0].card);
				suppress_draw = 1;
		}
	}
	return vanguard_card(player, card, event, 0, 20, 10);
}

int card_avatar_goblin_warchief(int player, int card, event_t event){
	return card_face_the_horde(player, card, event);
	//	return legacy_avatar(player, card, event, &effect_avatar_goblin_warchief );
}

int card_avatar_heartwood_storyteller(int player, int card, event_t event){
	vanguard_card(player, card, event, 6, 24, 0);
	card_instance_t *instance = get_card_instance(player, card);
	if(event == EVENT_MODIFY_COST_GLOBAL ){
		card_data_t* card_d = get_card_data(affected_card_controller, affected_card);

		// my spells cost less
		if( instance->targets[0].player != 1 && affected_card_controller == player ){
			if(! (card_d->type & TYPE_LAND) && (card_d->type & TYPE_CREATURE)){
				COST_COLORLESS--;
			}
		}

		// your spells cost more
		if( instance->targets[0].card != 1 && affected_card_controller == 1 - player){
			if(! (card_d->type & TYPE_LAND) && ! (card_d->type & TYPE_CREATURE)){
				COST_COLORLESS++;
			}
		}
	}

	// keep track of spells played this turn
	if(trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)
	   && player == reason_for_trigger_controller )
	{
		card_instance_t *played= get_card_instance(trigger_cause_controller, trigger_cause);
		if( ! ( cards_data[ played->internal_card_id].type & TYPE_LAND )  ){
			if( instance->targets[0].card != 1 && trigger_cause_controller == 1-player && ! ( cards_data[ played->internal_card_id].type & TYPE_CREATURE) ){
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
					instance->targets[0].card = 1;
				}
			}
			else if( instance->targets[0].player != 1 &&  trigger_cause_controller == player &&  cards_data[ played->internal_card_id].type & TYPE_CREATURE ){
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
					instance->targets[0].player = 1;
				}
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		instance->targets[0].card = 0;
		instance->targets[0].player = 0;
	}

	return 0;
}

int card_avatar_mirri_the_cursed(int player, int card, event_t event){
	vanguard_card(player, card, event, 5, 17, 0);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;
	td.illegal_abilities = 0;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allow_cancel = 0;

	card_instance_t *instance = get_card_instance(player, card);
	if(event == EVENT_CAN_ACTIVATE){
		if( target_available(player, card, &td) && target_available(player, card, &td1) > 1  ){
			return 1;
		}
	}
	else if(event == EVENT_ACTIVATE ){
		while( 1 ){
			if( ! select_target(player, card, &td, "Choose a creature to activate", NULL) ){
				spell_fizzled = 1;
				break;
			}
			if( ! is_sick(instance->targets[0].player, instance->targets[0].card) ){
				break;
			}
		}
		if( spell_fizzled != 1 ){
			instance->targets[1].player = instance->targets[0].player;
			instance->targets[1].card = instance->targets[0].card;
			card_instance_t *target = get_card_instance(instance->targets[1].player, instance->targets[1].card );
			instance->card_color = target->card_color;
			target->state |= STATE_CANNOT_TARGET;
			if( target_available(player, card, &td1) ){
				tap_card( instance->targets[0].player, instance->targets[0].card );
				select_target(player, card, &td1, "Choose a creature to shrink", NULL);
			}
			else{
				spell_fizzled = 1;
			}
			target->state &= ~STATE_CANNOT_TARGET;
		}
	}
	else if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *parent = get_card_instance( instance->parent_controller, instance->parent_card  );
		if(validate_target(player, card, &td1, 0)){
			pump_until_eot(player, card, parent->targets[0].player, parent->targets[0].card, -1, -1);
			add_1_1_counter(parent->targets[1].player, parent->targets[1].card);
		}
		instance->card_color = 0;
	}
	return 0;
}

int card_avatar_nekrataal(int player, int card, event_t event){
	vanguard_card(player, card, event, 6, 16, 0);
	if(event == EVENT_MODIFY_COST_GLOBAL ){
		if( affected_card_controller == player ){
			card_data_t* card_d = get_card_data(affected_card_controller, affected_card);
			if( card_d->type & TYPE_CREATURE ){
				COST_BLACK--;
			}
		}
	}
	return 0;
}

static int effect_avatar_royal_assassin(int player, int card, event_t event){
	vanguard_card(player, card, event, 5, 20, 2);
	return card_phyrexian_arena(player, card, event);
}

int card_avatar_royal_assassin(int player, int card, event_t event){
	return legacy_avatar(player, card, event, &effect_avatar_royal_assassin );
}

static int effect_avatar_serra_angel(int player, int card, event_t event){
	vanguard_card(player, card, event, 7, 19, 0);

	// if you have 2 tops and a helm in play, gain 1000 life
	if( count_cards_by_id( player, CARD_ID_SENSEIS_DIVINING_TOP ) > 1 && count_cards_by_id( player, CARD_ID_HELM_OF_AWAKENING ) > 0 && life[player] < 200 ){
		gain_life( player, 1000 );
	}


	if(trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)
		&& player == reason_for_trigger_controller && trigger_cause_controller == player )
	{
		card_instance_t *played= get_card_instance(trigger_cause_controller, trigger_cause);
		if( ! ( cards_data[ played->internal_card_id].type & TYPE_LAND )  ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
				gain_life(player, 2);
			}
		}
	}
	return 0;
}

int card_avatar_serra_angel(int player, int card, event_t event){
	return legacy_avatar(player, card, event, &effect_avatar_serra_angel );
}

static int effect_avatar_reaper_king(int player, int card, event_t event){
	vanguard_card(player, card, event, 5, 15, 0);
	if(affected_card_controller == player && ( event == EVENT_POWER || event == EVENT_TOUGHNESS ) ){
		card_data_t* card_d = get_card_data(affected_card_controller, affected_card);
		if(in_play(affected_card_controller, affected_card) && card_d->type & TYPE_CREATURE ){
			if( card_d->color & COLOR_TEST_BLACK ){ event_result++; }
			if( card_d->color & COLOR_TEST_BLUE ){ event_result++; }
			if( card_d->color & COLOR_TEST_GREEN ){ event_result++; }
			if( card_d->color & COLOR_TEST_RED ){ event_result++; }
			if( card_d->color & COLOR_TEST_WHITE ){ event_result++; }
		}
	}
	return 0;
}

int card_avatar_reaper_king(int player, int card, event_t event){
	return legacy_avatar(player, card, event, &effect_avatar_reaper_king );
}

static int effect_avatar_prodigal_sorcerer(int player, int card, event_t event){
	vanguard_card(player, card, event, 7, 25, 3);
	if( player == current_turn && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && reason_for_trigger_controller == player){
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
			effect_prodigal_sorcerer(player, card);
		}
	}
	return 0;
}

int card_avatar_prodigal_sorcerer(int player, int card, event_t event){
	return legacy_avatar(player, card, event, &effect_avatar_prodigal_sorcerer );
}

int card_avatar_chaos_orb(int player, int card, event_t event){
	vanguard_card(player, card, event, 10, 50, 4);
	card_instance_t *instance = get_card_instance(player, card);
	if( event == EVENT_CAN_ACTIVATE && instance->targets[0].player != 1 && player == HUMAN ){
		return 1;
	}
	else if(event == EVENT_ACTIVATE){
		instance->targets[0].player = 1;
	}
	else if(event == EVENT_RESOLVE_ACTIVATION ){
		if( ai_is_speculating != 1 ){
				int stop = 0;
				while( stop == 0 ){
					   int card_id = choose_a_card("Choose a card", -1, -1);
					   if( is_valid_card(cards_data[card_id].id) ){
							add_card_to_hand(player, card_id);
							stop = 1;
					   }
				}
		}
	}
	return 0;
}

static int effect_avatar_chaos_orb_lite(int player, int card, event_t event){
	vanguard_card(player, card, event, 7, 20, 4);
	return 0;
}

int card_avatar_chaos_orb_lite(int player, int card, event_t event){
	return legacy_avatar(player, card, event, &effect_avatar_chaos_orb_lite );
}

int card_spat(int player, int card, event_t event){
	// do the deck thing
	vanguard_card(player, card, event, 7, 20, 5);

	// give each play unlimited mana
	int p, c;
	for(p=0;p<2;p++){
		for(c=0;c<6;c++){
			if( ! has_mana(p, c, 10) ) {
				produce_mana(p, c, 10);
			}
		}
	}

	// can't play a spell if I already did
	card_instance_t *instance = get_card_instance(player, card);
	if(event == EVENT_MODIFY_COST_GLOBAL  && ! is_what(affected_card_controller, affected_card, TYPE_EFFECT | TYPE_LAND) ){
		if( instance->targets[affected_card_controller].player > 0 ){
			infinite_casting_cost();
		}
	}

	// keep track of spells played this turn
	if( event == EVENT_CAST_SPELL && ! affect_me(player, card) && ! is_what(affected_card_controller, affected_card, TYPE_EFFECT | TYPE_LAND) ){
		if( spell_fizzled != 1 ){
			if( instance->targets[affected_card_controller].player < 0 ){
				instance->targets[affected_card_controller].player = 0;
			}
			instance->targets[affected_card_controller].player++;
			add_counter(player, card, COUNTER_VORTEX);
		}
	}

	if( event == EVENT_CLEANUP ){
		instance->targets[0].player = 0;
		instance->targets[1].player = 0;
		remove_all_counters(player, card, COUNTER_VORTEX);
	}

	return 0;
}

static int is_banned_from_edh( int id ){
//Last revision: 31/01/2017

	switch (id){
		// Ante cards
		case CARD_ID_AMULET_OF_QUOZ:
		case CARD_ID_BRONZE_TABLET:
		case CARD_ID_CONTRACT_FROM_BELOW:
		case CARD_ID_DARKPACT:
		case CARD_ID_DEMONIC_ATTORNEY:
		case CARD_ID_JEWELED_BIRD:
		case CARD_ID_REBIRTH:
		case CARD_ID_TEMPEST_EFREET:
		case CARD_ID_TIMMERIAN_FIENDS:

		// Unset cards - only these three are implemented in Manalink
		case CARD_ID_DUH:
		case CARD_ID_GOBLIN_TUTOR:
		case CARD_ID_JACK_IN_THE_MOX:

		// Main banlist
		case CARD_ID_ANCESTRAL_RECALL:
		case CARD_ID_BALANCE:
		case CARD_ID_BIORHYTHM:
		case CARD_ID_BLACK_LOTUS:
		case CARD_ID_BRAIDS_CABAL_MINION:
		case CARD_ID_CHAOS_ORB:
		case CARD_ID_COALITION_VICTORY:
		case CARD_ID_CHANNEL:
		case CARD_ID_EMRAKUL_THE_AEONS_TORN:
		case CARD_ID_ERAYO_SORATAMI_ASCENDANT:
		case CARD_ID_FALLING_STAR:
		case CARD_ID_FASTBOND:
		case CARD_ID_GIFTS_UNGIVEN:
		case CARD_ID_GRISELBRAND:
		case CARD_ID_IONA_SHIELD_OF_EMERIA:
		case CARD_ID_KARAKAS:
		case CARD_ID_LEOVOLD_EMISSARY_OF_TREST:
		case CARD_ID_LIBRARY_OF_ALEXANDRIA:
		case CARD_ID_LIMITED_RESOURCES:
		case CARD_ID_MOX_EMERALD:
		case CARD_ID_MOX_JET:
		case CARD_ID_MOX_PEARL:
		case CARD_ID_MOX_RUBY:
		case CARD_ID_MOX_SAPPHIRE:
		case CARD_ID_PANOPTIC_MIRROR:
		case CARD_ID_PARADOX_ENGINE:
		case CARD_ID_PRIMEVAL_TITAN:
		case CARD_ID_PROPHET_OF_KRUPHIX:
		case CARD_ID_RECURRING_NIGHTMARE:
		case CARD_ID_ROFELLOS_LLANOWAR_EMISSARY:
		case CARD_ID_SHAHRAZAD:
		case CARD_ID_SUNDERING_TITAN:
		case CARD_ID_SWAY_OF_THE_STARS:
		case CARD_ID_SYLVAN_PRIMORDIAL:
		case CARD_ID_TIME_VAULT:
		case CARD_ID_TIME_WALK:
		case CARD_ID_TINKER:
		case CARD_ID_TOLARIAN_ACADEMY:
		case CARD_ID_TRADE_SECRETS:
		case CARD_ID_UPHEAVAL:
		case CARD_ID_WORLDFIRE:
		case CARD_ID_YAWGMOTHS_BARGAIN:
			return 1;

		default:
			return 0;
	}
}

int get_color_from_remainder_text(int id){
	int c1 = 0;
	const char* p = cards_ptr[ id ]->rules_text;
	while (*p){
			if( *p == '(' ){
				while( *p && *p != ')')
						++p;
			}
			else if( *p == '|' ){
					++p;
					if (strchr("BUGRWP2", *p)){
						if( *p == 'B' || *(p + 1) == 'B' ){
							c1 |= COLOR_TEST_BLACK;
						}
						if( *p == 'U' || *(p + 1) == 'U' ){
							c1 |= COLOR_TEST_BLUE;
						}
						if( *p == 'G' || *(p + 1) == 'G' ){
							c1 |= COLOR_TEST_GREEN;
						}
						if( *p == 'R' || *(p + 1) == 'R' ){
							c1 |= COLOR_TEST_RED;
						}
						if( *p == 'W' || *(p + 1) == 'W' ){
							c1 |= COLOR_TEST_WHITE;
						}
						if( c1 == COLOR_TEST_BLACK+COLOR_TEST_BLUE+COLOR_TEST_GREEN+COLOR_TEST_RED+COLOR_TEST_WHITE){
							break;
						}
					}
			} else {
				++p;
			}
	}
	return c1;
}

static int edh_special_case(int internal_card_id){
	if( cards_data[internal_card_id].cc[2] == 3 )	// vanguard cards; EDH in particular
		return 1;
	if( is_what(-1, internal_card_id, TYPE_LAND) && cards_data[internal_card_id].id != CARD_ID_DRYAD_ARBOR )
		return 1;
	if( is_what(-1, internal_card_id, TYPE_ARTIFACT) && (cards_data[internal_card_id].extra_ability & EA_MANA_SOURCE))
		return 1;

	return 0;
}

static int is_legal_general(int iid)
{
	if( cards_data[iid].id == CARD_ID_TEFERI_TEMPORAL_ARCHMAGE ||
		cards_data[iid].id == CARD_ID_NAHIRI_THE_LITHOMANCER ||
		cards_data[iid].id == CARD_ID_DARETTI_SCRAP_SAVANT ||
		cards_data[iid].id == CARD_ID_FREYALISE_LLANOWARS_FURY ||
		cards_data[iid].id == CARD_ID_OB_NIXILIS_OF_THE_BLACK_OATH
	)
		return 1;

	if (!is_legendary(-1, iid))
		return 0;

	if (is_what(-1, iid, TYPE_CREATURE))
		return 1;

	// Theros-block god cards still have their non-creature versions as the base card
	if (has_subtype_by_id(cards_data[iid].id, SUBTYPE_GOD))
		return 1;

  return 0;
}

static int general_has_partner(int csvid ){
	switch( csvid ){
			case CARD_ID_AKIRI_LINE_SLINGER:
			case CARD_ID_BRUSE_TARL_BOORISH_HERDER:
			case CARD_ID_IKRA_SHIDIQI_THE_USURPER:
			case CARD_ID_ISHAI_OJUTAI_DRAGONSPEAKER:
			case CARD_ID_KRAUM_LUDEVICS_OPUS:
			case CARD_ID_KYDELE_CHOSEN_OF_KRUPHIX:
			case CARD_ID_LUDEVIC_NECRO_ALCHEMIST:
			case CARD_ID_RAVOS_SOULTENDER:
			case CARD_ID_REYHAN_LAST_OF_THE_ABZAN:
			case CARD_ID_SIDAR_KONDO_OF_JAMURAA:
			case CARD_ID_SILAS_RENN_SEEKER_ADEPT:
			case CARD_ID_TANA_THE_BLOODSOWER:
			case CARD_ID_THRASIOS_TRITON_HERO:
			case CARD_ID_TYMNA_THE_WEAVER:
			case CARD_ID_VIAL_SMASHER_THE_FIERCE:
				return 1;
			default:
				break;
	}
	return 0;
}

enum{
		COMM_IN_COMMAND_ZONE 	= 1,
		COMM_IN_PLAY 			= 2,
		COMM_MISSING_IN_ACTION 	= 4
};

static int legacy_edh_general(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);
	card_instance_t *edh = get_card_instance(instance->targets[1].player, instance->targets[1].card);

	if( instance->damage_target_player > -1 ){
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;
		player_bits[player] |= PB_COMMANDER_IN_PLAY;
		if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(t_player, t_card) && in_play(t_player, t_card) ){
			card_instance_t *general = get_card_instance(t_player, t_card);
			if( general->kill_code > 0 ){
				int owner = get_owner(t_player, t_card);
				int choice = do_dialog(owner, t_player, t_card, -1, -1, " Move the Commander in the Command Zone\n Pass", 0);
				if( choice == 0 ){
					instance->targets[0].player = owner;
					instance->targets[0].card = get_original_internal_card_id(t_player, t_card);
					instance->damage_target_player = instance->damage_target_card = -1;
					instance->number_of_targets = 0;
					if( general->kill_code == KILL_EXILE ){
						instance->targets[0].card = -1;
					}
					instance->targets[11].player = 66;
				}
				else{
					if( instance->targets[2].player == 1 )
						SET_BYTE0(edh->targets[3].player) = COMM_MISSING_IN_ACTION;
					if( instance->targets[2].player == 2 )
						SET_BYTE1(edh->targets[3].player) = COMM_MISSING_IN_ACTION;
				}
			}
		}
	}

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		int found = instance->targets[0].card == -1 ? 1 : 0;
		if( ! found ){
			int owner = instance->targets[0].player;
			int iid = instance->targets[0].card;
			const int *grave = get_grave(owner);
			int i;
			for(i=count_graveyard(owner)-1; i>-1; i--){
				if( grave[i] == iid ){
					rfg_card_from_grave(owner, i);
					found = 1;
					break;
				}
			}
		}
		int result = found ? COMM_IN_COMMAND_ZONE : COMM_MISSING_IN_ACTION;
		if( instance->targets[2].player == 1 )
			SET_BYTE0(edh->targets[3].player) = result;
		if( instance->targets[2].player == 2 )
			SET_BYTE1(edh->targets[3].player) = result;
		kill_card(player, card, KILL_EXILE);
	}

	return 0;

}

int commander_enters_the_battlefield(int edh_player, int edh_card, int commander_player, int commander_card, int commander_code){
	int legacy = create_targetted_legacy_effect(edh_player, edh_card, &legacy_edh_general, commander_player, commander_card);
	card_instance_t *leg = get_card_instance(edh_player, legacy);
	leg->targets[1].player = edh_player;
	leg->targets[1].card = edh_card;
	leg->targets[2].player = commander_code;

	card_instance_t *parent = get_card_instance(edh_player, edh_card);
	if( commander_code == 1 ){
		parent->targets[4].player = commander_card;
		SET_BYTE0(parent->targets[3].player) = COMM_IN_PLAY;
	}
	if( commander_code == 2 ){
		parent->targets[4].card = commander_card;
		SET_BYTE1(parent->targets[3].player) = COMM_IN_PLAY;
	}
	return legacy;
}

int can_play_commander(int player, int card, event_t event, int commander_code){

	card_instance_t *instance = get_card_instance(player, card);

	int general_iid = commander_code == 2 ? instance->targets[2].card : instance->targets[2].player;
	if( general_iid == -1 )
		return 0;

	int general_csvid = cards_data[general_iid].id;

	// Derevi special ability
	if( general_csvid == CARD_ID_DEREVI_EMPYRIAL_TACTICIAN && has_mana_multi(player, 1, 0, 1, 1, 0, 1) ){
		if( count_counters(player, card, COUNTER_VORTEX) < 1 ||
			(instance->targets[3].player == COMM_IN_COMMAND_ZONE && check_rfg(player, general_csvid)) )
		{
			return 2;
		}
	}

	// Tasigur, the Golden Fang: a special case since he has Delve :(
	if( general_csvid == CARD_ID_TASIGUR_THE_GOLDEN_FANG ){
		card_ptr_t* c = cards_ptr[ general_csvid ];
		instance->targets[5].card = general_iid;
		int t_cless = true_get_updated_casting_cost(player, -1, general_iid, event, -1);
		instance->targets[5].card = -1;
		if( t_cless > 0 ){
			t_cless -= count_graveyard(player);
		}
		int cless = MAX(0, t_cless);
		if( has_mana_multi(player, cless, c->req_black, c->req_blue, c->req_green, c->req_red, c->req_white) ){
			return 1;
		}
	}

	// Check if the general is in the RFG / Command zone (unaccurate, will be fixed later)
	if( count_counters(player, card, COUNTER_VORTEX) > 0 ){
		if( instance->targets[3].player == -1 )
			return 0;
		int result = commander_code == 2 ? BYTE1(instance->targets[3].player) : BYTE0(instance->targets[3].player);
		if( result != COMM_IN_COMMAND_ZONE )
			return 0;
	}

	// If the general doesn't have flash, could be played only at sorcery speed
	if( ! can_sorcery_be_played(player, event) ){
		if( !(cards_data[general_iid].type & (TYPE_INSTANT | TYPE_INTERRUPT)) )
			return 0;
	}

	// Checking if the player could afford the modified casting cost by the EDH card.
	instance->targets[5].card = general_iid;
	int has_mana_to_cast_general = has_mana_to_cast_iid(player, event, general_iid);
	instance->targets[5].card = -1;
	if( has_mana_to_cast_general ){
		int rval = can_legally_play_iid(player, general_iid);
		if( rval == 99 && (cards_data[general_iid].type & (TYPE_INSTANT | TYPE_INTERRUPT)) ){
			return 99;
		}
		if( rval )
			return rval;
	}
	return 0;
}

// instance->targets[2].player / card : internal_card_id of both commanders
// instance->targets[3].player : keeps track of whether general is in play (2), in the Command Zone (1), or if we've lost track of it (4) (BYTE0 and BYTE1)
// instance->targets[4].player / card : if the Commander is in play, its "card" value is stored here.
int card_edh(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 40, 6);

	card_instance_t *instance = get_card_instance(player, card);

	// identify our general
	if( comes_into_play(player, card, event) ){
		// Step 1: go through the deck and identify the general
		int color = 0;
		int general_csvid = -1;
		int general_csvid2 = -1;
		int options[available_slots];
		int i;
		for(i=0; i<available_slots; i++){
			options[i] = 0;
		}
		int *deck = deck_ptr[player];
		for (i = 0; deck[i] != -1; ++i ){
			if (is_legal_general(deck[i])){
				options[cards_data[deck[i]].id]++;
			}
		}
		for (i = 0; i < active_cards_count[player]; ++i){
			card_instance_t* inst = in_hand(player, i);
			if (inst && is_legal_general(inst->internal_card_id)){
				options[cards_data[inst->internal_card_id].id]++;
			}
		}
		for(i=0; i<available_slots; i++){
			if( options[i] > 1 ){
				if( general_csvid == -1 ){
					general_csvid = i;
				}
				else if( general_csvid2 == -1 ){
						general_csvid2 = i;
				}
			}
		}

		if( general_csvid == -1 && general_csvid2 == -1 ){
			char buffer[600];
			snprintf(buffer, 600, "No general found in %s deck.\nPlace two copies of a legendary creature in the deck to identify him.",
					 player == AI ? "AI's" : "your");
			do_dialog(HUMAN, player, card, -1, -1, buffer, 0);
			lose_the_game(HUMAN);
			return 0;
		}
		if( general_csvid > -1 && general_csvid2 > -1 ){
			char buffer[600];
			if( ! general_has_partner(general_csvid) || ! general_has_partner(general_csvid2) ){
				snprintf(buffer, 600, "Two Commanders are allowed only if both have Partner.");
				do_dialog(HUMAN, player, card, -1, -1, buffer, 0);
				lose_the_game(HUMAN);
				return 0;
			}
		}

		int general_iid = get_internal_card_id_from_csv_id(general_csvid);
		color = cards_data[general_iid].color;
		color |= get_color_from_remainder_text(general_csvid);

		int general_iid2 = general_csvid2 > -1 ? get_internal_card_id_from_csv_id(general_csvid2) : -1;
		if( general_iid2 > -1 ){
			color |= cards_data[general_iid2].color;
			color |= get_color_from_remainder_text(general_csvid2);
		}

		card_ptr_t* c = cards_ptr[ general_csvid ];
		int invalid = 0;
		char buffer[600];
		int pos = snprintf(buffer, 600, "%s general is %s\n", player == AI ? "AI's" : "your", c->name );
		if( general_csvid2 > -1 ){
			c = cards_ptr[ general_csvid2 ];
			pos += snprintf(buffer + pos, 600 - pos, "%s general is %s\n", player == AI ? "AI's" : "your", c->name );
		}
		do_dialog(HUMAN, player, card, -1, -1, buffer, 0);
		if( invalid > 0 ){
			lose_the_game(HUMAN);
			return 0;
		}
		i = 0;
		for(i=0; i<available_slots; i++){
			options[i] = 0;
		}

		// Step 2: check legality of deck
		int cards = 0;
		invalid = 0;
		pos = snprintf(buffer, 600, " Checking the deck...\n");
		for (i = 0; deck[i] != -1; ++i){
				int csvid = cards_data[deck[i]].id;
				if (csvid == general_csvid || csvid == general_csvid2 || csvid == CARD_ID_ELDER_DRAGON_HIGHLANDER){
					continue;
				}
				++cards;

				options[csvid]++;
				card_ptr_t* c1 = cards_ptr[ csvid ];
				if( options[csvid] > 1 && !unlimited_allowed_in_deck(csvid) ){
					pos+=snprintf(buffer + pos, 600-pos, " Duplicate card: %s\n", c1->name);
					invalid++;
				}
				int c2 = cards_data[deck[i]].color & COLOR_TEST_ANY_COLORED;
				if( ! edh_special_case(deck[i]) && (c2 & color) != c2 ){
					pos+=snprintf(buffer + pos , 600-pos, " Illegal color: %s\n", c1->name);
					invalid++;
				}
				c2 = get_color_from_remainder_text(csvid);
				if( (c2 & color) != c2 ){
					pos+=snprintf(buffer + pos , 600-pos, " Illegal mana symbols in text:\n %s\n", c1->name);
					invalid++;
				}

				if( is_banned_from_edh(csvid) || has_subtype_by_id(csvid, SUBTYPE_CONSPIRACY) ){
					pos+=snprintf(buffer + pos , 600-pos, " Banned card: %s\n", c1->name);
					invalid++;
				}
		}

		for (i = 0; i < active_cards_count[player]; ++i){
			if( in_hand(player, i) ){
				card_instance_t* inst = get_card_instance(player, i);
				int csvid = cards_data[inst->internal_card_id].id;
				if (csvid == general_csvid || csvid == general_csvid2 || csvid == CARD_ID_ELDER_DRAGON_HIGHLANDER){
					continue;
				}
				++cards;

				options[csvid]++;
				card_ptr_t* c1 = cards_ptr[ csvid ];
				if( options[csvid] > 1 && !unlimited_allowed_in_deck(csvid) ){
					pos+=snprintf(buffer + pos, 600-pos, " Duplicate card: %s\n", c1->name);
					invalid++;
				}
				int c2 = cards_data[get_original_internal_card_id(player, i)].color & COLOR_TEST_ANY_COLORED;
				if( ! edh_special_case(inst->internal_card_id) && (c2 & color) != c2 ){
					pos+=snprintf(buffer + pos , 600-pos, " Illegal color: %s\n", c1->name);
					invalid++;
				}
				c2 = get_color_from_remainder_text(csvid);
				if( (c2 & color) != c2 ){
					pos+=snprintf(buffer + pos , 600-pos, " Illegal mana symbols in text:\n %s\n", c1->name);
					invalid++;
				}

				if( is_banned_from_edh(csvid) ){
					pos+=snprintf(buffer + pos , 600-pos, " Banned card: %s\n", c1->name);
					invalid++;
				}
			}
		}

		if( (general_csvid2 == -1 && cards != 99) || (general_csvid2 > -1 && cards != 98) ){
			pos+=snprintf(buffer + pos , 600-pos, " Illegal deck size: %d\n", cards + 1);
			invalid++;
		}

		if( invalid ){
			do_dialog(HUMAN, player, card, -1, -1, buffer, 0);
			lose_the_game(HUMAN);
			return 0;
		}
		instance->targets[2].player = general_iid;
		instance->targets[2].card = general_iid2;
		create_card_name_legacy(player, card, general_csvid);
		if( general_csvid2 > -1 ){
			create_card_name_legacy(player, card, general_csvid2);
		}
		instance->targets[3].player = 0;
		SET_BYTE0(instance->targets[3].player) = COMM_IN_COMMAND_ZONE; //Our Commander starts the game in the "Command Zone"
		if( general_csvid2 > -1 ){
			SET_BYTE1(instance->targets[3].player) = COMM_IN_COMMAND_ZONE;
		}
	}

	enum{
		CHOICE_PLAY_COM1 = 1,
		CHOICE_PLAY_COM2,
		CHOICE_DEREVI_SPECIAL_ABILITY
	};

	if( event == EVENT_CAN_ACTIVATE ){
		int result = can_play_commander(player, card, event, 1);
		int result2 = can_play_commander(player, card, event, 2);
		if( result == 99 || result2 == 99 )
			return 99;
		if( result || result2 )
			return 1;
	}

	if( event == EVENT_ACTIVATE ){
		int abils[4] = {0};
		int general_iid = instance->targets[2].player;
		abils[1] = can_play_commander(player, card, event, 1);
		int general_iid2 = instance->targets[2].card;
		if( general_iid2 > -1 )
			abils[2] = can_play_commander(player, card, event, 2);
		if( abils[1] == 2 || abils[2] == 2 )
			abils[3] = 1;
		char msg[100];
		scnprintf(msg, 100, "Play %s", cards_ptr[cards_data[general_iid].id]->name);
		char msg2[100];
		if( general_iid2 > -1 )
			scnprintf(msg, 100, "Play %s", cards_ptr[cards_data[general_iid2].id]->name);
 		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_OMIT_ILLEGAL, DLG_AUTOCHOOSE_IF_1,
							msg, abils[1], 1,
							msg2, abils[2], 1,
							"Put Derevi into play", abils[3], 5);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_DEREVI_SPECIAL_ABILITY ){ // Putting Derevi directly into play
			charge_mana_multi(player, 1, 0, 1, 1, 0, 1);
			if( spell_fizzled != 1 ){
				remove_card_from_rfg(player, CARD_ID_DEREVI_EMPYRIAL_TACTICIAN);
			}
			return 0;
		}

		int general_csvid = choice == CHOICE_PLAY_COM2 ? cards_data[general_iid2].id : cards_data[general_iid].id;
		instance->targets[3].card = 0;
		instance->targets[5].card = choice == CHOICE_PLAY_COM2 ? general_iid2 : general_iid;
		if( general_csvid == CARD_ID_TASIGUR_THE_GOLDEN_FANG ){
			int card_added = add_card_to_hand(player, instance->targets[5].card);
			hand_count[player]--;
			add_state(player, card_added, STATE_INVISIBLE);
			int result = cast_spell_with_delve(player, card_added);
			obliterate_card(player, card_added);
			instance->targets[5].card = -1;
			if( ! result ){
				spell_fizzled = 1;
				return 0;
			}
		}
		else{
			charge_mana_from_id(player, -1, event, general_csvid);
			instance->targets[5].card = -1;
		}
		if (cancel != 1){
			remove_card_from_rfg(player, general_csvid);
			int i;
			for(i=0; i<7; i++){
				instance->targets[3].card += mana_paid[i];
			}
			add_counters(player, card, COUNTER_VORTEX, 2);
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
		int choice = instance->info_slot;
		if( choice == CHOICE_DEREVI_SPECIAL_ABILITY ){
			SET_BYTE0(parent->targets[3].player) = COMM_MISSING_IN_ACTION; //Needed for generating the "commander" legacy
			int card_added = add_card_to_hand(player, get_internal_card_id_from_csv_id(CARD_ID_DEREVI_EMPYRIAL_TACTICIAN));
			set_special_flags2(player, card_added, SF2_COMMANDER);
			put_into_play(player, card_added);
		}
		else{
			int general_iid = instance->targets[2].player;
			int general_iid2 = instance->targets[2].card;
			int general_csvid = choice == CHOICE_PLAY_COM2 ? cards_data[general_iid2].id : cards_data[general_iid].id;
			if( choice == CHOICE_PLAY_COM2 ){
				SET_BYTE1(parent->targets[3].player) = COMM_MISSING_IN_ACTION;
			}
			else{
				SET_BYTE0(parent->targets[3].player) = COMM_MISSING_IN_ACTION;
			}
			add_csvid_to_rfg(player, general_csvid);
			play_card_in_exile_for_free_commander(player, player, general_csvid, instance->targets[3].card);
		}
	}

	// "Wait mode" : our Commander is not in play and not in the Command Zone.
	if( event == EVENT_CAST_SPELL && affected_card_controller == player && instance->targets[3].player > -1 ){
		int general_iid = instance->targets[2].player;
		int general_iid2 = instance->targets[2].card;
		if( BYTE0(instance->targets[3].player) == COMM_MISSING_IN_ACTION )
		{
			if( ! is_token(affected_card_controller, affected_card) &&
				get_original_internal_card_id(affected_card_controller, affected_card) == general_iid )
			{
				commander_enters_the_battlefield(player, card, affected_card_controller, affected_card, 1);
			}
		}
		if( general_iid2 > -1 && BYTE1(instance->targets[3].player) == COMM_MISSING_IN_ACTION )
		{
			if( ! is_token(affected_card_controller, affected_card) &&
				get_original_internal_card_id(affected_card_controller, affected_card) == general_iid2 )
			{
				commander_enters_the_battlefield(player, card, affected_card_controller, affected_card, 2);
			}
		}
	}

	// if our general deals damage to opponent, add counters
	int general_card = instance->targets[4].player;
	int general_card2 = instance->targets[4].card;
	card_instance_t* damage = combat_damage_being_dealt(event);
	if( damage && general_card > -1 &&
		damage->damage_source_card == general_card && damage->damage_source_player == player &&
		damage->damage_target_card == -1 && damage->damage_target_player == 1-player && !damage_is_to_planeswalker(damage) )
	{
		add_counters(player, card, COUNTER_DEATH, damage->info_slot);
		if( count_counters(player, card, COUNTER_DEATH) >= 21 ){
			lose_the_game(1-player);
		}
	}

	if( damage && general_card2 > -1 &&
		damage->damage_source_card == general_card && damage->damage_source_player == player &&
		damage->damage_target_card == -1 && damage->damage_target_player == 1-player && !damage_is_to_planeswalker(damage) )
	{
		add_counters(player, card, COUNTER_CARRION, damage->info_slot);
		if( count_counters(player, card, COUNTER_CARRION) >= 21 ){
			lose_the_game(1-player);
		}
	}

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		int general_iid = instance->targets[2].player;
		int general_iid2 = instance->targets[2].card;
		int general_csvid = cards_data[general_iid].id;
		int general_csvid2 = general_iid2 > -1 ? cards_data[general_iid2].id : -1;

		if( (general_csvid == CARD_ID_OLORO_AGELESS_ASCETIC || general_csvid2 == CARD_ID_OLORO_AGELESS_ASCETIC) &&
			current_turn == player )
		{
			int good = 0;
			if( count_counters(player, card, COUNTER_VORTEX) == 0 )
				good = 1;
			if( count_counters(player, card, COUNTER_VORTEX) > 0 && check_rfg(player, CARD_ID_OLORO_AGELESS_ASCETIC) &&
				instance->targets[3].player > -1 )
			{
				if( general_csvid == CARD_ID_OLORO_AGELESS_ASCETIC && BYTE0(instance->targets[3].player) == COMM_IN_COMMAND_ZONE )
					good = 1;
				if( general_csvid2 == CARD_ID_OLORO_AGELESS_ASCETIC && BYTE1(instance->targets[3].player) == COMM_IN_COMMAND_ZONE )
					good = 1;
			}
			if( good ){
				int count = count_upkeeps(player);
				if( count ){
					if( event == EVENT_TRIGGER ){
						event_result |= RESOLVE_TRIGGER_MANDATORY;
					}
					else if( event == EVENT_RESOLVE_TRIGGER ){
							gain_life(player, count*2);
					}
				}
			}
		}
	}

	//If we somehow loose track of our Commander, set the EDH card to "wait mode" at EOT
	if( event == EVENT_CLEANUP ){
		if( instance->targets[3].player > -1 ){
			if( BYTE0(instance->targets[3].player) == COMM_IN_PLAY && instance->targets[4].player > -1){
				if( ! in_play(player, instance->targets[4].player) ){
					instance->targets[4].player = -1;
					SET_BYTE0(instance->targets[3].player) = COMM_MISSING_IN_ACTION;
				}
			}
			if( BYTE1(instance->targets[3].player) == COMM_IN_PLAY && instance->targets[4].card > -1){
				if( ! in_play(player, instance->targets[4].card) ){
					instance->targets[4].card = -1;
					SET_BYTE1(instance->targets[3].player) = COMM_MISSING_IN_ACTION;
				}
			}
		}
	}

	return 0;
}

int card_3_starting_hand(int player, int card, event_t event){
	vanguard_card(player, card, event, 3, 20, 99);
	return 0;
}

int card_4_starting_hand(int player, int card, event_t event){
	vanguard_card(player, card, event, 4, 20, 99);
	return 0;
}

int card_5_starting_hand(int player, int card, event_t event){
	vanguard_card(player, card, event, 5, 20, 99);
	return 0;
}

int card_6_starting_hand(int player, int card, event_t event){
	vanguard_card(player, card, event, 6, 20, 99);
	return 0;
}

int card_7_starting_hand(int player, int card, event_t event){
	vanguard_card(player, card, event, 7, 20, 99);
	return 0;
}

int card_8_starting_hand(int player, int card, event_t event){
	vanguard_card(player, card, event, 8, 20, 99);
	return 0;
}

int card_random_singleton_deck(int player, int card, event_t event){
	vanguard_card(player, card, event, 7, 20, 9);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && instance->targets[1].card != 66 ){
		return 1;
	}
	if( event == EVENT_ACTIVATE ){
		instance->targets[1].card = 66;
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		reshuffle_all_into_deck(player, 0);
		create_random_singleton_deck(player, -1, 292, 737, 60);
		draw_cards(player, 7);
	}

	return 0;
}

/*
** AVATAR - Arcanis, the Omnipotent	""
Vanguard (+1/-3)
|X, Return a creature you control with converted mana cost X to its owner's hand: Draw a number of cards chosen at random between 0 and X. X can't be 0.
*/

/*
** AVATAR - Arcbound Overseer	""
Vanguard (+0/+3)
At the beginning of your upkeep, you may put a +1/+1 counter on target creature you control.
At the beginning of your upkeep, you may put a charge counter on target permanent you control.
*/

/*
** AVATAR - Ashling the Pilgrim	""
Vanguard (-1/+6)
|2: Ashling the Pilgrim deals 1 damage to each creature and each player.
*/

/*
** AVATAR - Ashling, the Extinguisher	""
Vanguard (+1/-3)
|5: Destroy all nonland permanents. Activate this ability only once and only during your turn.
*/

/*
** AVATAR - Ashnod	""
Vanguard (+1/-8)
Whenever a creature deals damage to you, destroy it.
*/

/*
** AVATAR - Barrin	""
Vanguard (+0/+6)
Sacrifice a permanent: Return target creature to its owner's hand.
*/

/*
** AVATAR - Birds of Paradise	""
Vanguard (+0/-3)
Lands you control have "|T: Add one mana of any color."
*/

/*
** AVATAR - Bosh, Iron Golem	""
Vanguard (+0/-2)
|X, Sacrifice an artifact with converted mana cost X: Bosh, Iron Golem deals X damage to any target.
*/

/*
** AVATAR - Braids, Conjurer Adept	""
Vanguard (+0/+3)
|2: Each player may put a land card from their hand onto the battlefield tapped.
|3: Each player may put a noncreature artifact card from their hand onto the battlefield.
|4: Each player may put a creature card from their hand onto the battlefield. Activate this ability only any time you could cast a sorcery.
*/

/*
** AVATAR - Chronatog	""
Vanguard (-1/+1)
You have no maximum hand size.
|0: Draw three cards. You skip your next turn. Activate this ability only once each turn.
*/

/*
** AVATAR - Crovax	""
Vanguard (+2/+0)
Whenever a creature you control deals damage to a creature or player, you gain 1 life.
*/

/*
** AVATAR - Dakkon Blackblade	""
Vanguard (+1/+0)
You may play any colored card from your hand as a copy of a basic land card chosen at random that can produce mana of one of the card's colors.
*/

/*
** AVATAR - Dauntless Escort	""
Vanguard (+1/+1)
Creatures you control have exalted.
*/

/*
** AVATAR - Diamond Faerie	""
Vanguard (+0/+5)
|I: Target creature you control gets +1/+1 until end of turn.
*/

/*
** AVATAR - Eight-and-a-Half-Tails	""
Vanguard (+2/-3)
|1: Until end of turn, target permanent you control gains protection from a color chosen at random from colors it doesn't have protection from.
*/

/*
** AVATAR - Eladamri, Lord of Leaves	""
Vanguard (-1/+2)
At the beginning of each player's precombat main phase, that player adds |G|G.
*/

/*
** AVATAR - Eladamri	""
Vanguard (-1/+15)
|0: The next 1 damage that would be dealt to target creature you control is dealt to you instead.
*/

/*
** AVATAR - Enigma Sphinx	""
Vanguard (+0/+5)
Whenever you cast a colored artifact spell for the first time each turn, search your library for a colored artifact card chosen at random whose converted mana cost is less than that spell's converted mana cost. You may play that card without paying its mana cost. If you don't, put that card on the bottom of your library.
*/

/*
** AVATAR - Ertai	""
Vanguard (-1/+4)
Creatures you control have hexproof.
*/

/*
** AVATAR - Etched Oracle	""
Vanguard (+1/-4)
You may pay |W|U|B|R|G rather than pay the mana cost for spells that you cast.
*/

/*
** AVATAR - Fallen Angel	""
Vanguard (+0/-5)
Whenever a creature you control dies, target opponent loses 1 life and you gain 1 life.
*/

/*
** AVATAR - Figure of Destiny	""
Vanguard (+0/+3)
|X: Put a +1/+1 counter on target creature with fewer than X +1/+1 counters on it.
*/

/*
** AVATAR - Flametongue Kavu	""
Vanguard (+0/-6)
Whenever a nontoken creature enters the battlefield under your control, that creature deals X damage to target creature, where X is a number chosen at random from 0 to 4.
*/

/*
** AVATAR - Frenetic Efreet	""
Vanguard (-1/-3)
Each permanent you control has phasing.
At the beginning of your end step, flip a coin. If you win the flip, take an extra turn after this one.
*/

/*
** AVATAR - Gerrard	""
Vanguard (-4/+0)
At the beginning of your draw step, draw an additional card.
*/

/*
** AVATAR - Gix	""
Vanguard (-2/+18)
|3: Return target creature card from your graveyard to your hand.
*/

/*
** AVATAR - Greven il-Vec	""
Vanguard (-1/+2)
Whenever a creature you control deals damage to a creature, destroy the other creature. It can't be regenerated.
*/

/*
** AVATAR - Grinning Demon	""
Vanguard (-1/-2)
Whenever a nontoken creature you control dies, target opponent discards a card.
*/

/*
** AVATAR - Haakon, Stromgald Scourge	""
Vanguard (+0/-3)
Pay 1 life: You may play target creature card in your graveyard this turn.
Whenever you play a creature card from your graveyard, it becomes a |Sblack Zombie Knight.
If a Zombie Knight would be put into your graveyard from the battlefield, exile it instead.
*/

/*
** AVATAR - Hanna	""
Vanguard (+1/-5)
Spells you cast cost |1 less to cast.
*/

/*
** AVATAR - Hell's Caretaker	""
Vanguard (-1/-4)
|3, Sacrifice a creature: Return target creature card from your graveyard to the battlefield.
*/

/*
** AVATAR - Hermit Druid	""
Vanguard (-2/-2)
At the beginning of your upkeep, put a land card from your library chosen at random onto the battlefield.
*/

/*
** AVATAR - Higure, the Still Wind	""
Vanguard (-1/+3)
Whenever a nontoken creature you control deals combat damage to an opponent, choose a creature card at random from your library, reveal that card, and put it into your hand. Then shuffle your library.
*/

/*
** AVATAR - Ink-Eyes, Servant of Oni	""
Vanguard (+0/-3)
At the beginning of the game, look at target opponent's hand and choose a nonland card from it. That player discards that card.
|X, Pay X life: Put target creature card with converted mana cost X from an opponent's graveyard onto the battlefield under your control.
*/

/*
** AVATAR - Jaya Ballard	""
Vanguard (+0/+1)
|X: Jaya Ballard deals an amount of damage chosen at random from 0 to X to any target. Activate this ability only once each turn.
*/

/*
** AVATAR - Jhoira of the Ghitu	""
Vanguard (+1/+0)
|3, Discard a card: Copy three instant cards chosen at random. You may cast one of the copies without paying its mana cost.
|3, Discard a card: Copy three sorcery cards chosen at random. You may cast one of the copies without paying its mana cost. Activate this ability only any time you could cast a sorcery.
*/

/*
** AVATAR - Karn	""
Vanguard (+1/+6)
Each noncreature artifact you control is an artifact creature with power and toughness each equal to its converted mana cost.
*/

/*
** AVATAR - Karona, False God	""
Vanguard (-1/+8)
At the beginning of your upkeep, exchange control of a permanent you control chosen at random and a permanent target opponent controls chosen at random.
*/

/*
** AVATAR - Kresh the Bloodbraided	""
Vanguard (+1/-3)
Whenever a creature you control is devoured, create an X/X |Sgreen Ooze creature token, where X is the devoured creature's power.
*/

/*
** AVATAR - Loxodon Hierarch	""
Vanguard (+0/+12)
Sacrifice a permanent: Regenerate target creature you control.
*/

/*
** AVATAR - Lyna	""
Vanguard (+2/-4)
Creatures you control have shadow.
*/

/*
** AVATAR - Lyzolda, the Blood Witch	""
Vanguard (-1/-1)
Hellbent - As long as you have no cards in hand, if a source you control would deal damage to a permanent or player, it deals double that damage to that permanent or player instead.
Hellbent - At the beginning of your end step, if you have no cards in hand, each of your opponents discards a card.
*/

/*
** AVATAR - Maelstrom Archangel	""
Vanguard (+0/+5)
At end of combat, for each creature you controlled at the time it dealt combat damage to a player this turn, copy a random card with the same mana cost as that creature. You may pay |3. If you do, choose one of those copies. If a copy of a permanent card is chosen, you may create a token that's a copy of that card. If a copy of an instant or sorcery card is chosen, you may cast the copy without paying its mana cost.
*/

/*
** AVATAR - Malfegor	""
Vanguard (+2/-2)
Whenever a creature enters the battlefield under your control, if it was unearthed, it gets +3/+0.
Whenever a creature you control is exiled, if it was unearthed, shuffle that card into its owner's library.
*/

/*
** AVATAR - Maralen of the Mornsong	""
Vanguard (+0/-3)
At the beginning of the game, you may pay any amount of life.
You can't draw cards.
At the beginning of your draw step, look at the top X cards of your library, where X is the amount of life paid with Maralen of the Mornsong. Put one of them into your hand, then shuffle your library.
*/

/*
** AVATAR - Maraxus	""
Vanguard (+1/+2)
Creatures you control get +1/+0.
*/

/*
** AVATAR - Maro	""
Vanguard (+2/-7)
Tap an untapped creature you control, Discard a card: Target creature you control gets +X/+X until end of turn, where X is the number of cards in your hand.
*/

/*
** AVATAR - Master of the Wild Hunt	""
Vanguard (+1/+3)
|2|G: Create a |Sgreen creature token that's a 2/2 Wolf, a 2/3 Antelope with |H2forestwalk, a 3/2 Cat with shroud, or a 4/4 Rhino with trample, chosen at random.
*/

/*
** AVATAR - Mayael the Anima	""
Vanguard (+1/+5)
At the beginning of your upkeep, reveal the top card of your library. If it's a creature card with power 5 or greater, put it into your hand. Otherwise, you may put it on the bottom of your library.
*/

/*
** AVATAR - Mirri	""
Vanguard (+0/+5)
If a basic land you control is tapped for mana, it produces mana of a color of your choice instead of any other type.
*/

/*
** AVATAR - Mirror Entity	""
Vanguard (+1/-2)
|X: Choose a creature type. Until end of turn, creatures you control of the chosen type have base power and toughness X/X and gain all creature types.
*/

/*
** AVATAR - Mishra	""
Vanguard (+0/-3)
If a creature you control would deal damage, it deals double that damage instead.
*/

/*
** AVATAR - Morinfen	""
Vanguard (+0/+30)
At the beginning of your upkeep, you lose 1 life for each permanent you control.
*/

/*
** AVATAR - Multani	""
Vanguard (-3/-2)
Creatures you control get +X/+0, where X is the number of cards in your hand.
*/

/*
** AVATAR - Murderous Redcap	""
Vanguard (+0/-2)
Whenever a creature enters the battlefield under your control with a counter on it, you may have it deal damage equal to its power to any target.
*/

/*
** AVATAR - Necropotence	""
Vanguard (+0/+4)
Skip your draw step.
At the beginning of your end step, if it's not the first turn of the game, put a death counter on Necropotence. You draw X cards and you lose X life, where X is the number of death counters on Necropotence.
*/

/*
** AVATAR - Oracle	""
Vanguard (+1/+9)
|0: Untap target attacking creature you control and remove it from combat.
*/

/*
** AVATAR - Orcish Squatters	""
Vanguard (-1/-1)
At the beginning of your precombat main phase, add |C for each land target opponent controls.
*/

/*
** AVATAR - Orim	""
Vanguard (+0/+12)
Creatures you control have reach.
*/

/*
** AVATAR - Peacekeeper	""
Vanguard (+0/+9)
|3: For each opponent who controls a creature, create a token that's a copy of a card named Arrest and attach it to a creature that player controls chosen at random.
*/

/*
** AVATAR - Phage the Untouchable	""
Vanguard (+0/-5)
Pay 1 life: Until end of turn, whenever a creature deals combat damage to you, destroy that creature.
Pay 1 life: Until end of turn, whenever target creature you control deals combat damage to a creature, destroy the damaged creature.
*/

/*
** AVATAR - Platinum Angel	""
Vanguard (-1/-2)
If you control an artifact, a creature, an enchantment, and a land, you can't lose the game and your opponents can't win the game.
*/

/*
** AVATAR - Raksha Golden Cub	""
Vanguard (+1/+9)
Creatures you control get +0/+1.
Equipped creatures you control get +1/+0 and have first strike.
*/

/*
** AVATAR - Rith, the Awakener	""
Vanguard (+0/+6)
Whenever a creature you control deals combat damage to a player, you may pay |5. If you do, create a 5/5 |Sred Dragon creature token with flying.
*/

/*
** AVATAR - Rofellos	""
Vanguard (-2/+4)
Whenever a creature you control dies, draw a card.
*/

/*
** AVATAR - Rumbling Slum	""
Vanguard (+1/+1)
At the beginning of your upkeep, Rumbling Slum deals 1 damage to each opponent.
*/

/*
** AVATAR - Sakashima the Impostor	""
Vanguard (+0/+1)
|2: Choose a creature you control. It becomes a copy of target creature except for its name.
*/

/*
** AVATAR - Selenia	""
Vanguard (+1/+7)
Creatures you control have vigilance.
*/

/*
** AVATAR - Serra	""
Vanguard (+1/+1)
Creatures you control get +0/+2.
*/

/*
** AVATAR - Seshiro the Anointed	""
Vanguard (+0/-1)
At the beginning of the game, choose a creature type. Creatures you control, creature spells you control, and creature cards you own in any zone other than the battlefield or the stack have the chosen type in addition to their other types.
*/

/*
** AVATAR - Sidar Kondo	""
Vanguard (-1/+12)
|3: Target creature gets +3/+3 until end of turn.
*/

/*
** AVATAR - Sisay	""
Vanguard (-2/-3)
Whenever you tap a land for mana, add one mana of any type that land produced.
*/

/*
** AVATAR - Sisters of Stone Death	""
Vanguard (+1/-5)
Each creature you control must be blocked if able.
|4: Exile target creature that's blocking a creature you control.
*/

/*
** AVATAR - Sliver Queen, Brood Mother	""
Vanguard (+0/+8)
|3: Create a 1/1 colorless Sliver creature token.
*/

/*
** AVATAR - Sliver Queen	""
Vanguard (+0/+7)
Whenever you cast a non-Sliver creature spell, exile that spell. If you do, create a token that's a copy of a random non-Shapeshifter Sliver creature card with the same converted mana cost as that spell.
*/

/*
** AVATAR - Squee, Goblin Nabob	""
Vanguard (+3/-4)
|1: Prevent the next 1 damage that would be dealt to target creature you control this turn.
*/

/*
** AVATAR - Squee	""
Vanguard (+3/-4)
Your opponents play with their hands revealed.
*/

/*
** AVATAR - Stalking Tiger	""
Vanguard (+0/+2)
Whenever you cast a creature spell, you may pay |1. If you do, draw a card.
*/

/*
** AVATAR - Starke	""
Vanguard (+0/-2)
At the beginning of your draw step, you may draw an additional card. If you do, put a card from your hand on the bottom of your library.
*/

/*
** AVATAR - Stonehewer Giant	""
Vanguard (+1/-5)
Whenever a creature enters the battlefield under your control, create a token that's a copy of a random Equipment card with converted mana cost less than that creature's converted mana cost. Attach that Equipment to that creature.
*/

/*
** AVATAR - Stuffy Doll	""
Vanguard (+0/-6)
|0: If damage would be dealt to you this turn by a source of your choice, prevent that damage. Stuffy Doll deals damage to you and target opponent or planeswalker equal to half the damage prevented this way, rounded up. Activate this ability only once each turn.
*/

/*
** AVATAR - Tahngarth	""
Vanguard (-1/+7)
Creatures you control have haste.
*/

/*
** AVATAR - Takara	""
Vanguard (+3/-8)
Sacrifice a creature: Takara deals 1 damage to any target.
*/

/*
** AVATAR - Tawnos	""
Vanguard (+3/-4)
You may cast artifact, creature, and enchantment spells as though they had flash.
*/

/*
** AVATAR - Teysa, Orzhov Scion	""
Vanguard (+0/-2)
Whenever a nontoken creature dies, create a 1/1 |Swhite Spirit creature token with flying.
*/

/*
** AVATAR - Titania	""
Vanguard (+2/-5)
You may play an additional land on each of your turns.
*/

/*
** AVATAR - Tradewind Rider	""
Vanguard (+1/+4)
|3: Each player returns a permanent they control to its owner's hand unless they pay 2 life.
*/

/*
** AVATAR - Two-Headed Giant of Foriys	""
Vanguard (+1/-4)
Creatures you control have menace.
Each creature you control can block an additional creature each combat.
*/

/*
** AVATAR - Urza	""
Vanguard (-1/+10)
|3: Urza deals 1 damage to any target.
*/

/*
** AVATAR - Vampire Nocturnus	""
Vanguard (-1/-2)
Play with the top card of your library revealed.
As long as the top card of your library is |Sblack, |Sblack creatures you control get +2/+1.
*/

/*
** AVATAR - Viridian Zealot	""
Vanguard (+0/+2)
|2, Sacrifice a creature: Destroy target artifact or enchantment. Search your library for a card with the same name as the sacrificed creature, reveal that card, and put it into your hand. Then shuffle your library.
*/

/*
** AVATAR - Volrath	""
Vanguard (+2/-3)
Whenever a creature you control is put into your graveyard from the battlefield, you may put it on top of your library.
*/

/*
** AVATAR - Xantcha	""
Vanguard (+1/+3)
Sacrifice a permanent: Regenerate target creature.
*/
