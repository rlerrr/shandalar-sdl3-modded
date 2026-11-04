#include "manalink.h"

static int general_in_rfg_comm_zone(int player, int card, int commander_code);
static int prismatic_piper_check(int player, int card, int card_color, int to_compare);

/* Canonical recovered helper declarations. */
int general_has_partner(int csvid);

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

int vanguard_card(int player, int card, event_t event, int cards, int starting_life, int special)
{
  // set the max hand size
  if (event == EVENT_MAX_HAND_SIZE && current_turn == player)
  {
    event_result = cards;
  }

  indestructible(player, card, event);

  // perform the first turn mulligans
  if (mulligans_complete[player] != 1 && trigger_condition == TRIGGER_UPKEEP &&
      affect_me(player, card) && reason_for_trigger_controller == player && ai_is_speculating != 1)
  {
    if (event == EVENT_TRIGGER)
    {
      event_result |= RESOLVE_TRIGGER_MANDATORY;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      // set the deck from challenge mode, if applicable
      if (get_setting(SETTING_CHALLENGE_MODE))
      {
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
      int second_general_csvid = 0;
      if (special == 6)
      {
        general_csvid = cards_data[instance->targets[2].player].id;
        second_general_csvid = cards_data[instance->targets[2].card].id;
      }
      int counter = 0;
      for (i = 0; i < 500; i++)
      {
        if (deck[i] != -1)
        {
          if (cards_data[deck[i]].cc[2] == 3 ||
              (general_csvid > 0 && cards_data[deck[i]].id == general_csvid) ||
              (second_general_csvid > 0 && cards_data[deck[i]].id == second_general_csvid))
          {
            if (counter++ > 1 && cards_data[deck[i]].cc[2] == 3 && general_csvid == 0)
            {
              life[player] = counter;
            }
            remove_card_from_deck(player, i);
            i--;
          }
        }
      }

      if (general_csvid == CARD_ID_OLORO_AGELESS_ASCETIC && player == current_turn)
        gain_life(player, 2 * count_upkeeps(player));

      // internal_randomize the deck if that's our thing
      int skip_mulligans = 0;
      if (special == 4)
      {
        create_random_test_deck(player, card, color_to_color_test(internal_rand(5) + 1), 0,
                                available_slots - 1, 60);
      }
      else if (special == 5)
      {
        create_spat_deck(player);
        skip_mulligans = 1;
      }
      else if (special == 9)
      {
        int clrs = 0;
        int choice2 = internal_rand(3);
        if (player != AI)
        {
          choice2 = do_dialog(player, player, card, -1, -1,
                              " Monocolored deck\n Two-colors deck\n Three-colors deck", 0);
        }
        choice2++;
        int k;
        for (k = 0; k < choice2; k++)
        {
          if (player != AI)
          {
            clrs |= select_a_color(player);
          }
          else
          {
            clrs |= color_to_color_test(internal_rand(5) + 1);
          }
        }
        choice2 = 0;
        int min = 0;
        int max = available_slots;
        if (player == AI)
        {
          choice2 = 22 + internal_rand(2);
        }
        else
        {
          while (1)
          {
            choice2 =
                do_dialog(player, player, card, -1, -1,
                          " Everything\n Unlimited\n DK+AN+AQ\n Legends\n 8th Edition\n FE + "
                          "Homelands\n Ice Age Block\n Mirage block\n Tempest block\n Urza block\n "
                          "Zendikar block\n Scars block\n Innistrad block\n More sets",
                          0);
            if (choice2 != 13)
            {
              break;
            }
            else
            {
              choice2 = do_dialog(player, player, card, -1, -1,
                                  " Previous expansions\n Masques Block\n Mirrodin Block\n "
                                  "Kamigawa Block\n M13\n Ravnica block\n Lorwyn block\n "
                                  "Shadowmoor block\n Shards block\n M10\n Return to Ravnica block",
                                  0);
              if (choice2 != 0)
              {
                choice2 += 13;
                break;
              }
            }
          }
        }
        if (choice2 == 1)
        {
          min = CARD_ID_AIR_ELEMENTAL;
          max = CARD_ID_ZOMBIE_MASTER;
        }
        if (choice2 == 2)
        {
          min = CARD_ID_AMNESIA;
          max = CARD_ID_YOTIAN_SOLDIER;
        }
        if (choice2 == 3)
        {
          min = CARD_ID_ABOMINATION;
          max = CARD_ID_ZEPHYR_FALCON;
        }
        if (choice2 == 4)
        {
          min = CARD_ID_ANGEL_OF_MERCY;
          max = CARD_ID_YAVIMAYA_ENCHANTRESS;
        }
        if (choice2 == 5)
        {
          min = CARD_ID_AEOLIPILE;
          max = CARD_ID_WIZARDS_SCHOOL;
        }
        if (choice2 == 6)
        {
          min = CARD_ID_ADARKAR_SENTINEL;
          max = CARD_ID_ZUR_THE_ENCHANTER;
        }
        if (choice2 == 7)
        {
          min = CARD_ID_ABYSSAL_HUNTER;
          max = CARD_ID_ZOMBIE_SCAVENGERS;
        }
        if (choice2 == 8)
        {
          min = CARD_ID_ABANDON_HOPE;
          max = CARD_ID_ZEALOTS_EN_DAL;
        }
        if (choice2 == 9)
        {
          min = CARD_ID_ABSOLUTE_GRACE;
          max = CARD_ID_YAWGMOTHS_BARGAIN;
        }
        if (choice2 == 10)
        {
          min = CARD_ID_ADVENTURING_GEAR;
          max = CARD_ID_ZULAPORT_ENFORCER;
        }
        if (choice2 == 11)
        {
          min = CARD_ID_ABUNA_ACOLYTE;
          max = CARD_ID_XENOGRAFT;
        }
        if (choice2 == 12)
        {
          min = CARD_ID_ABATTOIR_GHOUL;
          max = CARD_ID_ZEALOUS_STRIKE;
        }
        if (choice2 == 14)
        {
          min = CARD_ID_AERIAL_CARAVAN;
          max = CARD_ID_ZERAPA_MINOTAUR;
        }
        if (choice2 == 15)
        {
          min = CARD_ID_ALTAR_OF_SHADOWS;
          max = CARD_ID_VIRIDIAN_SCOUT;
        }
        if (choice2 == 16)
        {
          min = CARD_ID_AKKI_AVALANCHERS;
          max = CARD_ID_YUKI_ONNA;
        }
        if (choice2 == 17)
        {
          min = CARD_ID_AJANI_CALLER_OF_THE_PRIDE;
          max = CARD_ID_YEVAS_FORCEMAGE;
        }
        if (choice2 == 18)
        {
          min = CARD_ID_AGRUS_KOS_WOJEK_VETERAN;
          max = CARD_ID_WRIT_OF_PASSAGE;
        }
        if (choice2 == 19)
        {
          min = CARD_ID_ADDER_STAFF_BOGGART;
          max = CARD_ID_WOLF_SKULL_SHAMAN;
        }
        if (choice2 == 20)
        {
          min = CARD_ID_ADVICE_FROM_THE_FAE;
          max = CARD_ID_WORM_HARVEST;
        }
        if (choice2 == 21)
        {
          min = CARD_ID_AD_NAUSEAM;
          max = CARD_ID_ZEALOUS_PERSECUTION;
        }
        if (choice2 == 22)
        {
          min = CARD_ID_ACIDIC_SLIME;
          max = CARD_ID_ZOMBIE_GOLIATH;
        }
        if (choice2 == 23)
        {
          min = CARD_ID_ABRUPT_DECAY;
          max = CARD_ID_ZHUR_TAA_DRUID;
        }
        create_random_test_deck(player, card, clrs, min, max, 60);
      }
      else if (special == 10)
      { // Face the horde
        int choice =
            do_dialog(HUMAN, player, card, -1, -1,
                      " Level: Standard\n Level: Legacy\n Legal: Vintage\n Level: Illegal deck", 0);
        get_card_instance(player, card)->targets[0].player = 1; // Zombie horde
        get_card_instance(player, card)->targets[0].card = 2 + choice;
      }
      else if (special == 11)
      { // Hidden agenda
        call_card_function(player, card, EVENT_HIDDEN_AGENDA);
      }

      // if we are playing momir basic, give both players a deck of
      // basic lands
      if (get_momir())
      {
        create_internal_random_basic_deck(player);
        skip_mulligans = 1;
      }
      else if (get_setting(SETTING_CHALLENGE_MODE) && get_challenge_round() == 13)
      {
        // gain_life(AI, 17);
        create_internal_random_basic_deck(player);
        skip_mulligans = 1;
      }
      if (general_csvid > 1)
      {
        shuffle_vanguard(player, 1);
        draw_cards(player, cards);
        if (player == HUMAN)
        {
          test_definition_t this_test;
          new_default_test_definition(&this_test, 0, "Select a card to exile.");

          int mulliganed_cards[200]; // More than safe, since deck should've been flagged illegal if
                                     // it didn't contain exactly 100 cards
          memset(mulliganed_cards, -1, sizeof(mulliganed_cards));
          int *mull = &mulliganed_cards[0];
          while (hand_count[player] > 0 &&
                 do_dialog(player, player, card, -1, -1, " Keep\n EDH Mulligan", 0) == 1)
          {
            int num_exiled = 0, selected;
            while (hand_count[player] > 0 &&
                   (selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1,
                                                 &this_test)) != -1)
            {
              *mull++ = get_card_instance(player, selected)->internal_card_id;
              obliterate_card(
                  player, selected); // not kill_card(...KILL_EXILE), since it's exiled face-down
              play_sound_effect(WAV_DESTROY); // the exile sound effect, despite its name
              num_exiled++;
            }
            if (num_exiled > 1)
            {
              draw_cards(player, num_exiled - 1);
            }
          }
          if (mulliganed_cards[0] != -1)
          {
            int bottom = count_deck(player);
            for (mull = &mulliganed_cards[0]; *mull != -1; ++mull)
            {
              deck_ptr[player][bottom++] = *mull;
            }
            shuffle(player);
          }
        }
      }
      else
      {
        int max_hand = cards;
        // perform any mulliganing
        while (max_hand > 0)
        {
          // draw the appropriate number of cards
          shuffle_vanguard(player, 1);
          draw_cards(player, max_hand);

          // allow the choice to mulligan
          int choice = 0;
          if (player == HUMAN && !skip_mulligans)
          {
            choice = do_dialog(player, player, card, -1, -1, " Keep\n Mulligan", 0);
            if (choice == 1)
            {
              max_hand--;
              raw_put_hand_in_deck(player);
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

      // elvish champion
      if (special == 1)
      {
        token_generation_t token;
        default_token_definition(player, card, CARD_ID_ELF, &token);
        token.action = TOKEN_ACTION_HASTE;
        token.special_infos = 66;
        generate_token(&token);
      }
      // royal assassin
      else if (special == 2 && current_turn == player)
      {
        lose_life(player, 1);
        draw_a_card(player);
      }
      // prodigal_sorcerer
      else if (special == 3 && current_turn == player)
      {
        effect_prodigal_sorcerer(player, card);
      }
      // chaos orb - give the AI a Sol Ring
      else if (special == 4 && player == AI && cards > 7)
      {
        add_card_to_hand(player, get_internal_card_id_from_csv_id(CARD_ID_SOL_RING));
      }
      // Planechase - get the first Plane
      else if (special == 8)
      {
        if (player == HUMAN)
        {
          planeswalk_to(player, get_new_plane(-1), 1);
        }
      }
      else if (special == 9)
      {
        obliterate_card(player, card);
      }
    }
  }

  return 0;
}

static int legacy_avatar(int player, int card, event_t event, int (*func_ptr)(int, int, event_t))
{
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
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
  /* Momir Vig, Simic Visionary Avatar	""
   * Vanguard
   * |X, Discard a card: Create a token that's a copy of a creature card with mana value X chosen at random. Activate only as a sorcery and only once each turn.
   * Starting hand size modifier: +0.
   * Starting life total modifier: +4. */

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
  /* Oni of Wild Places Avatar	""
   * Vanguard
   * Creatures you control have haste.
   * At the beginning of your upkeep, return a creature you control to its owner's hand.
   * Starting hand size modifier: +0.
   * Starting life total modifier: -2. */

	return legacy_avatar(player, card, event, &effect_avatar_oni_of_wild_places );
}

int card_avatar_elvish_champion(int player, int card, event_t event){
  /* Elvish Champion Avatar	""
   * Vanguard
   * You begin the game with a 1/1 green Elf creature token on the battlefield. It has "|T: Add |G."
   * Starting hand size modifier: +0.
   * Starting life total modifier: -5. */

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
  /* Akroma, Angel of Wrath Avatar	""
   * Vanguard
   * Whenever a creature you control enters, it gains two abilities chosen at random from among flying, first strike, trample, haste, protection from black, protection from red, and vigilance.
   * Starting hand size modifier: +1.
   * Starting life total modifier: +7. */

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
  /* Goblin Warchief Avatar	""
   * Vanguard
   * Attacking creatures you control get +1/+0.
   * Starting hand size modifier: +1.
   * Starting life total modifier: +2. */

	return card_face_the_horde(player, card, event);
	//	return legacy_avatar(player, card, event, &effect_avatar_goblin_warchief );
}

int card_avatar_heartwood_storyteller(int player, int card, event_t event){
  /* Heartwood Storyteller Avatar	""
   * Vanguard
   * The first creature spell you cast each turn costs |1 less to cast.
   * The first noncreature spell each opponent casts each turn costs |1 more to cast.
   * Starting hand size modifier: -1.
   * Starting life total modifier: +4. */

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

int card_avatar_mirri_the_cursed(int player, int card, event_t event)
{
  /* Mirri the Cursed Avatar	""
   * Vanguard
   * Creatures you control have "|T: Another target creature gets -1/-1 until end of turn. Put a +1/+1 counter on this creature."
   * Starting hand size modifier: -2.
   * Starting life total modifier: -3. */

  card_instance_t *parent;
  int target_player;
  card_instance_t *instance;
  card_instance_t *damaged_creature;
  int result;
  target_definition_t td;
  target_definition_t td1;

  vanguard_card(player, card, event, 5, 0x11, 0);
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.illegal_state = TARGET_STATE_TAPPED;
  td.illegal_abilities = 0;
  default_target_definition(player, card, &td1, 2);
  td1.allow_cancel = 0;
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = target_available(player, card, &td);
    if (result != 0)
    {
      result = target_available(player, card, &td1);
      return (unsigned int)(1 < result);
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    do
    {
      result = select_target(player, card, &td, "Choose a creature to activate", NULL);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      result = is_sick(instance->targets[0].player, instance->targets[0].card);
    } while (result != 0);
    if (spell_fizzled != 1)
    {
      result = instance->targets[0].card;
      target_player = instance->targets[0].player;
      instance->targets[1].card = result;
      instance->targets[1].player = target_player;
      damaged_creature = get_card_instance(target_player, result);
      instance->card_color = damaged_creature->card_color;
      damaged_creature->state = damaged_creature->state | 0x100000;
      result = target_available(player, card, &td1);
      if (result == 0)
      {
        spell_fizzled = 1;
      }
      else
      {
        tap_card(instance->targets[0].player, instance->targets[0].card);
        select_target(player, card, &td1, "Choose a creature to shrink", NULL);
      }
      damaged_creature->state = damaged_creature->state & 0xffefffff;
      return 0;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    parent = get_card_instance(instance->parent_controller, instance->parent_card);
    result = validate_target(player, card, &td1, 0);
    if (result != 0)
    {
      pump_until_eot(player, card, parent->targets[0].player, parent->targets[0].card, -1, -1);
      add_counters(player, card, parent->targets[1].player, parent->targets[1].card, COUNTER_P1_P1,
                   1);
    }
    instance->card_color = '\0';
  }
  return 0;
}

int card_avatar_nekrataal(int player, int card, event_t event){
  /* Nekrataal Avatar	""
   * Vanguard
   * Creature spells you cast cost |B less to cast. This effect reduces only the amount of colored mana you pay.
   * Starting hand size modifier: -1.
   * Starting life total modifier: -4. */

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
  /* Royal Assassin Avatar	""
   * Vanguard
   * At the beginning of your upkeep, you draw a card and you lose 1 life.
   * Starting hand size modifier: -2.
   * Starting life total modifier: +0. */

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
  /* Serra Angel Avatar	""
   * Vanguard
   * Whenever you cast a spell, you gain 2 life.
   * Starting hand size modifier: +0.
   * Starting life total modifier: -1. */

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
  /* Reaper King Avatar	""
   * Vanguard
   * Each creature you control gets +1/+1 for each of its colors.
   * Starting hand size modifier: -2.
   * Starting life total modifier: -5. */

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
  /* Prodigal Sorcerer Avatar	""
   * Vanguard
   * At the beginning of your upkeep, look at the top card of your library. You may put that card into your graveyard.
   * Starting hand size modifier: +0.
   * Starting life total modifier: +5. */

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
					   int card_id = choose_a_card_iid("Choose a card.", -1, -1);
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

int card_spat(int player, int card, event_t event)
{
  card_instance_t *instance;
  int player_or_result;
  int color_or_spell_count;

  vanguard_card(player, card, event, 7, 0x14, 5);
  for (player_or_result = 0; player_or_result < 2; ++player_or_result)
    for (color_or_spell_count = 0; color_or_spell_count < 6; ++color_or_spell_count)
      if (!has_mana(player_or_result, color_or_spell_count, 10))
        produce_mana(player_or_result, color_or_spell_count, 10);
  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    player_or_result = is_what(affected_card_controller, affected_card, 0x81);
    if ((player_or_result == 0) && (0 < instance->targets[affected_card_controller].player))
    {
      infinite_casting_cost();
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((((card != affected_card) || (affected_card_controller != player)) &&
         (player_or_result = is_what(affected_card_controller, affected_card, 0x81),
          player_or_result == 0)) &&
        (spell_fizzled != 1))
    {
      player_or_result = 1;
      color_or_spell_count = instance->targets[affected_card_controller].player;
      if (-1 < color_or_spell_count)
      {
        player_or_result = color_or_spell_count + 1;
      }
      instance->targets[affected_card_controller].player = player_or_result;
      add_counters(player, card, player, card, COUNTER_VORTEX, 1);
    }
  }
  else if (event == EVENT_CLEANUP)
  {
    instance->targets[0].player = 0;
    instance->targets[1].player = 0;
    remove_all_counters(player, card, player, card, COUNTER_VORTEX);
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

static int edh_special_case(int internal_card_id)
{
  int result;
  if (cards_data[internal_card_id].cc[2] != '\x03')
  {
    result = is_what(-1, internal_card_id, 1);
    if ((result == 0) || (cards_data[internal_card_id].id == CARD_ID_DRYAD_ARBOR))
    {
      result = is_what(-1, internal_card_id, 0x40);
      if (result == 0)
      {
        return 0;
      }
      return cards_data[internal_card_id].extra_ability >> 0xc & 1;
    }
  }
  return 1;
}

static int is_legal_general(int iid)
{
  switch (cards_data[iid].id)
  {
  case CARD_ID_DUNE_BROOD_NEPHILIM:
  case CARD_ID_GLINT_EYE_NEPHILIM:
  case CARD_ID_INK_TREADER_NEPHILIM:
  case CARD_ID_WITCH_MAW_NEPHILIM:
  case CARD_ID_YORE_TILLER_NEPHILIM:
  case CARD_ID_DARETTI_SCRAP_SAVANT:
  case CARD_ID_FREYALISE_LLANOWARS_FURY:
  case CARD_ID_NAHIRI_THE_LITHOMANCER:
  case CARD_ID_OB_NIXILIS_OF_THE_BLACK_OATH:
  case CARD_ID_TEFERI_TEMPORAL_ARCHMAGE:
  case CARD_ID_ROWAN_KENRITH:
  case CARD_ID_WILL_KENRITH:
  case CARD_ID_AMINATOU_THE_FATESHIFTER:
  case CARD_ID_ESTRID_THE_MASKED:
  case CARD_ID_LORD_WINDGRACE:
  case CARD_ID_SAHEELI_THE_GIFTED:
  case CARD_ID_JESKA_THRICE_REBORN:
  case CARD_ID_TEVESH_SZAT_DOOM_OF_FOOLS:
    return 1;
  default:
    if (!is_legendary(-1, iid))
      return 0;
    if (is_what(-1, iid, TYPE_CREATURE))
      return 1;
    return has_subtype_by_id(-1, cards_data[iid].id, SUBTYPE_GOD) != 0;
  }
}

int general_has_partner(int csvid)
{
  switch (csvid)
  {
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
  case CARD_ID_AKROMA_VISION_OF_IXIDOR:
  case CARD_ID_ALENA_KESSIG_TRAPPER:
  case CARD_ID_ALHARU_SOLEMN_RITUALIST:
  case CARD_ID_ANARA_WOLVID_FAMILIAR:
  case CARD_ID_ARDENN_INTREPID_ARCHAEOLOGIST:
  case CARD_ID_ARMIX_FILIGREE_THRASHER:
  case CARD_ID_BREECHES_BRAZEN_PLUNDERER:
  case CARD_ID_BRINELIN_THE_MOON_KRAKEN:
  case CARD_ID_DARGO_THE_SHIPWRECKER:
  case CARD_ID_ELIGETH_CROSSROADS_AUGUR:
  case CARD_ID_ESIOR_WARDWING_FAMILIAR:
  case CARD_ID_FALTHIS_SHADOWCAT_FAMILIAR:
  case CARD_ID_GHOST_OF_RAMIREZ_DEPIETRO:
  case CARD_ID_GILANRA_CALLER_OF_WIREWOOD:
  case CARD_ID_GLACIAN_POWERSTONE_ENGINEER:
  case CARD_ID_HALANA_KESSIG_RANGER:
  case CARD_ID_ICH_TEKIK_SALVAGE_SPLICER:
  case CARD_ID_JESKA_THRICE_REBORN:
  case CARD_ID_KAMAHL_HEART_OF_KROSA:
  case CARD_ID_KEDISS_EMBERCLAW_FAMILIAR:
  case CARD_ID_KELETH_SUNMANE_FAMILIAR:
  case CARD_ID_KESKIT_THE_FLESH_SCULPTOR:
  case CARD_ID_KODAMA_OF_THE_EAST_TREE:
  case CARD_ID_KRARK_THE_THUMBLESS:
  case CARD_ID_LIVIO_OATHSWORN_SENTINEL:
  case CARD_ID_MALCOLM_KEEN_EYED_NAVIGATOR:
  case CARD_ID_MIARA_THORN_OF_THE_GLADE:
  case CARD_ID_NADIER_AGENT_OF_THE_DUSKENEL:
  case CARD_ID_NUMA_JORAGA_CHIEFTAIN:
  case CARD_ID_PRAVA_OF_THE_STEEL_LEGION:
  case CARD_ID_RADIANT_SERRA_ARCHANGEL:
  case CARD_ID_REBBEC_ARCHITECT_OF_ASCENSION:
  case CARD_ID_ROGRAKH_SON_OF_ROHGAHH:
  case CARD_ID_SAKASHIMA_OF_A_THOUSAND_FACES:
  case CARD_ID_SENGIR_THE_DARK_BARON:
  case CARD_ID_SIANI_EYE_OF_THE_STORM:
  case CARD_ID_SLURRK_ALL_INGESTING:
  case CARD_ID_TEVESH_SZAT_DOOM_OF_FOOLS:
  case CARD_ID_THE_PRISMATIC_PIPER:
  case CARD_ID_TOGGO_GOBLIN_WEAPONSMITH:
  case CARD_ID_TORMOD_THE_DESECRATOR:
    return 1;
  default:
    return 0;
  }
}

enum{
		COMM_IN_COMMAND_ZONE 	= 1,
		COMM_IN_PLAY 			= 2,
		COMM_MISSING_IN_ACTION 	= 4
};

static int legacy_edh_general(int player, int card, event_t event)
{
  int i;
  int owner;
  int iid;
  int t_card;
  int t_player;
  int grave_owner;
  int *grave;
  uint8_t command_zone_state;
  card_instance_t *instance;
  card_instance_t *edh_record;
  int grave_size;
  card_instance_t *commander;
  int commander_slot_or_iid;

  instance = get_card_instance(player, card);
  edh_record = get_card_instance(instance->targets[1].player, instance->targets[1].card);
  t_player = (int)instance->damage_target_player;
  if (-1 < instance->damage_target_player)
  {
    t_card = instance->damage_target_card;
    player_bits[player] = player_bits[player] | 0x80000;
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {
      if ((((t_card == affected_card) && (t_player == affected_card_controller)) &&
           (commander = in_play(t_player, t_card), commander != NULL)) &&
          (commander = get_card_instance(t_player, t_card), commander->kill_code != '\0'))
      {
        owner = get_owner(t_player, t_card);
        if (do_dialog(owner, t_player, t_card, -1, -1,
                      " Move the Commander in the Command Zone\n Pass", 0) == 0)
        {
          dispatch_event(t_player, t_card, 0x1c00);
          instance->targets[0].player = owner;
          iid = get_original_internal_card_id(t_player, t_card);
          instance->targets[0].card = iid;
          instance->damage_target_card = -1;
          instance->damage_target_player = -1;
          instance->number_of_targets = '\0';
          if (commander->kill_code == '\x04')
          {
            instance->targets[0].card = -1;
          }
          instance->targets[0xb].player = 0x42;
        }
        else
        {
          commander_slot_or_iid = instance->targets[2].player;
          if (commander_slot_or_iid == 1)
          {
            *(uint8_t *)&edh_record->targets[3].player = 4;
            commander_slot_or_iid = instance->targets[2].player;
          }
          if (commander_slot_or_iid == 2)
          {
            SET_BYTE1(edh_record->targets[3].player) = 4;
          }
        }
      }
    }
    else if (event == EVENT_STATIC_EFFECTS)
    {
      event_flags = event_flags | 0x10000000;
    }
    else if (((event == EVENT_CARDCONTROLLED) && (t_card == affected_card)) &&
             (t_player == affected_card_controller))
    {
      commander_slot_or_iid = instance->targets[2].player;
      if (commander_slot_or_iid == 1)
      {
        edh_record->targets[4].player = t_card;
      }
      else if (commander_slot_or_iid == 2)
      {
        edh_record->targets[4].card = t_card;
      }
    }
  }
  if (resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) != 0)
  {
    commander_slot_or_iid = instance->targets[0].card;
    if (commander_slot_or_iid == -1)
    {
      commander_slot_or_iid = instance->targets[2].player;
      command_zone_state = 1;
    }
    else
    {
      command_zone_state = 4;
      grave_owner = instance->targets[0].player;
      grave = graveyard_ptr_mutable[grave_owner];
      grave_size = count_graveyard(grave_owner);
      i = grave_size + -1;
      if (-1 < i)
      {
        do
        {
          if (commander_slot_or_iid == grave[i])
          {
            rfg_card_from_grave(grave_owner, i);
            commander_slot_or_iid = instance->targets[2].player;
            command_zone_state = 1;
            break;
          }
          i = i + -1;
        } while (i != -1);
      }
      commander_slot_or_iid = instance->targets[2].player;
    }
    if (commander_slot_or_iid == 1)
    {
      *(uint8_t *)&edh_record->targets[3].player = command_zone_state;
      commander_slot_or_iid = instance->targets[2].player;
    }
    if (commander_slot_or_iid == 2)
    {
      SET_BYTE1(edh_record->targets[3].player) = command_zone_state;
    }
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

int commander_enters_the_battlefield(int edh_player, int edh_card, int commander_player,
                                     int commander_card, int commander_code)
{
  int card;
  card_instance_t *instance;

  card = create_targetted_legacy_effect(edh_player, edh_card, legacy_edh_general, commander_player,
                                        commander_card);
  instance = get_card_instance(edh_player, card);
  instance->targets[1].player = edh_player;
  instance->targets[1].card = edh_card;
  instance->targets[2].player = commander_code;
  instance = get_card_instance(edh_player, edh_card);
  if (commander_code == 1)
  {
    *(uint8_t *)&instance->targets[3].player = 2;
    instance->targets[4].player = commander_card;
  }
  else if (commander_code == 2)
  {
    SET_BYTE1(instance->targets[3].player) = 2;
    instance->targets[4].card = commander_card;
  }
  set_special_flags2(commander_player, commander_card, 0x1000000);
  return card;
}

/* ManalinkEh 2022: 0x22c3610; original cards/vanguard.c:1662. */
static int general_in_rfg_comm_zone(int player, int card, int commander_code)
{
  int result;
  card_instance_t *instance;
  if (commander_code == 1)
  {
    result = count_counters(player, card, COUNTER_VORTEX);
    if (result < 1)
    {
      return 1;
    }
    instance = get_card_instance(player, card);
    result = instance->targets[3].player;
    if (result != -1)
    {
      return (unsigned int)((char)result == '\x01');
    }
  }
  else
  {
    result = count_counters(player, card, COUNTER_VELOCITY);
    if (result < 1)
    {
      return 1;
    }
    instance = get_card_instance(player, card);
    if (instance->targets[3].card != -1)
    {
      return (unsigned int)((char)SET_BYTE1(instance->targets[3].player) == '\x01');
    }
  }
  return 0;
}

// instance->targets[2].player / card : internal_card_id of both commanders
// instance->targets[3].player : keeps track of whether general is in play (2), in the Command Zone (1), or if we've lost track of it (4) (BYTE0 and BYTE1)
// instance->targets[4].player / card : if the Commander is in play, its "card" value is stored here.
static int can_play_commander(int player, int card, event_t event, int commander_code)
{
  card_ptr_t *tasigur_cost;
  uint16_t commander_csvid;
  card_instance_t *instance;
  int available_or_temporary_card;
  int available_or_grave_size;
  int32_t iid;
  unsigned int generic_cost;
  target_definition_t td;

  instance = get_card_instance(player, card);
  if (commander_code == 2)
  {
    iid = instance->targets[2].card;
  }
  else
  {
    iid = instance->targets[2].player;
  }
  if (iid == -1)
  {
    return 0;
  }
  do
  {
    commander_csvid = cards_data[iid].id;
    if (commander_csvid == 0x352e)
    {
      available_or_temporary_card = has_mana_multi(player, 1, 0, 1, 1, 0, 1);
      if ((available_or_temporary_card != 0) &&
          ((available_or_temporary_card = count_counters(player, card, COUNTER_VORTEX),
            available_or_temporary_card < 1 ||
                ((instance->targets[3].player == 1 &&
                  (available_or_temporary_card = check_rfg(player, CARD_ID_DEREVI_EMPYRIAL_TACTICIAN),
                   available_or_temporary_card != 0))))))
      {
        return 2;
      }
      break;
    }
    if (commander_csvid == 0x392e)
    {
      available_or_grave_size = general_in_rfg_comm_zone(player, card, commander_code);
      tasigur_cost = cards_ptr[CARD_ID_TASIGUR_THE_GOLDEN_FANG];
      if (available_or_grave_size == 0)
      {
        return 0;
      }
      instance->targets[5].card = iid;
      generic_cost = true_get_updated_casting_cost(player, -1, iid, event, -1);
      instance->targets[5].card = -1;
      if (0 < (int)generic_cost)
      {
        available_or_grave_size = count_graveyard(player);
        generic_cost = generic_cost - available_or_grave_size;
      }
      available_or_temporary_card = has_mana_multi(
          player, (int)(generic_cost ^ 0xffffffff) >> 0x1f & generic_cost,
          (unsigned int)tasigur_cost->req.req_black, (unsigned int)tasigur_cost->req.req_blue,
          (unsigned int)tasigur_cost->req.req_green, (unsigned int)tasigur_cost->req.req_red,
          (unsigned int)tasigur_cost->req.req_white);
    }
    else
    {
      if (commander_csvid == 0x4ea3)
      {
        if (general_in_rfg_comm_zone(player, card, commander_code) == 0)
        {
          return 0;
        }
        instance->targets[5].card = iid;
        available_or_temporary_card = add_card_to_hand(player, iid);
        hand_count[player] = hand_count[player] + -1;
        add_state(player, available_or_temporary_card, 0x20);
        available_or_grave_size = can_cast_hogaak(player, available_or_temporary_card, event, 0);
        instance->targets[5].card = -1;
        obliterate_card(player, available_or_temporary_card);
        return available_or_grave_size;
      }
      if (commander_csvid == 0x4abe)
      {
        available_or_temporary_card = general_in_rfg_comm_zone(player, card, commander_code);
        if (available_or_temporary_card == 0)
        {
          return 0;
        }
        base_target_definition(player, card, &td, 2);
        td.required_state = TARGET_STATE_ATTACKING;
        td.illegal_state = TARGET_STATE_ISBLOCKED;
        td.allowed_controller = player;
        td.preferred_controller = player;
        if (((current_phase == 0x18) &&
             (available_or_temporary_card = has_mana_multi(player, 0, 1, 1, 0, 0, 0),
              available_or_temporary_card != 0)) &&
            (available_or_temporary_card = can_target(&td), available_or_temporary_card != 0))
        {
          return 4;
        }
        break;
      }
      if ((commander_csvid != 0x5690) ||
          (available_or_temporary_card = general_in_rfg_comm_zone(player, card, commander_code),
           available_or_temporary_card == 0))
        break;
      instance->targets[5].card = iid;
      if (has_mana_to_cast_iid(player, event, iid) == 0)
        break;
      available_or_temporary_card = can_pay_life_as_cost_for_spells_or_activated_abilities(
          player, count_counters(player, card, COUNTER_VORTEX));
    }
    if (available_or_temporary_card != 0)
    {
      return 1;
    }
  } while (false);
  available_or_temporary_card = general_in_rfg_comm_zone(player, card, commander_code);
  if ((available_or_temporary_card != 0) &&
      ((available_or_temporary_card = can_sorcery_be_played(player, event),
        available_or_temporary_card != 0 || ((cards_data[iid].type & 0x30) != 0))))
  {
    instance->targets[5].card = iid;
    available_or_temporary_card = has_mana_to_cast_iid(player, event, iid);
    instance->targets[5].card = -1;
    if (available_or_temporary_card != 0)
    {
      return can_legally_play_iid(player, iid);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x22c64f0; original cards/vanguard.c:1803. */
int locate_edh_card(int player)
{
  for (int i = 0; i < active_cards_count[player]; ++i)
    if (in_play(player, i) && get_id(player, i) == CARD_ID_ELDER_DRAGON_HIGHLANDER)
      return i;
  return -1;
}

/* ManalinkEh 2022: 0x22c6940; original cards/vanguard.c:1813. */
int get_commander_storm_count(int player)
{
  int edh_card = locate_edh_card(player);
  if (edh_card == -1)
    return 0;
  return count_counters(player, edh_card, COUNTER_VORTEX) / 2;
}

/* ManalinkEh 2022: 0x22c6590; original cards/vanguard.c:1830. */
static int put_commander_from_command_zone_to_impl(int player, int where, int which_com, int *rv)
{
  int packed_state_or_iid;
  unsigned int selected_slot;
  int id;
  int edh_or_added_card;
  card_instance_t *instance;
  int first_cmc;
  int second_cmc;
  int first_priority_cmc;
  int second_priority_cmc;
  char partner_state;
  unsigned int commander_slot;
  unsigned int choice_or_priority;
  int cid[3];
  char buffer1[100];
  char buffer2[100];

  edh_or_added_card = locate_edh_card(player);
  if (edh_or_added_card != -1)
  {

    instance = get_card_instance(player, edh_or_added_card);
    packed_state_or_iid = instance->targets[3].player;
    if (packed_state_or_iid < 0)
    {
      return -1;
    }
    cid[0] = -1;
    cid[1] = (char)packed_state_or_iid == 1 ? instance->targets[2].player : -1;
    partner_state = (char)((unsigned int)packed_state_or_iid >> 8);
    cid[2] = partner_state == 1 ? instance->targets[2].card : -1;
    if ((cid[1] & cid[2]) == -1)
      return -1;
    id = cid[2];
    packed_state_or_iid = cid[1];
    scnprintf(buffer1, 100, " %s", cards_ptr[cards_data[cid[1]].id]->name);
    scnprintf(buffer2, 100, " %s", cards_ptr[cards_data[id].id]->name);
    choice_or_priority = which_com;
    if (which_com == 0)
    {
      first_cmc = get_cmc_by_internal_id(packed_state_or_iid);
      second_cmc = get_cmc_by_internal_id(id);
      first_priority_cmc = get_cmc_by_internal_id(packed_state_or_iid);
      second_priority_cmc = get_cmc_by_internal_id(id);
      choice_or_priority = 0;
      if (second_priority_cmc < first_priority_cmc)
      {
        choice_or_priority = 10;
      }
      choice_or_priority = dialog_impl(
          player, edh_or_added_card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL, DLG_AUTOCHOOSE_IF_1,
          DLG_NO_CANCEL, buffer1, (packed_state_or_iid ^ 0xffffffffU) >> 0x1f, choice_or_priority,
          buffer2, (id ^ 0xffffffffU) >> 0x1f, ((second_cmc <= first_cmc) ? 0 : 10), 0, 0, 0, 0);
    }

    commander_slot = 1;
    edh_or_added_card = -1;
    selected_slot = choice_or_priority & 1;
    do
    {
      if ((selected_slot != 0) && (-1 < packed_state_or_iid))
      {
        remove_card_from_rfg(player, (unsigned int)cards_data[packed_state_or_iid].id);
        if (commander_slot == 1)
        {
          *(uint8_t *)&instance->targets[3].player = 4;
        }
        else if (commander_slot == 2)
        {
          SET_BYTE1(instance->targets[3].player) = 4;
        }
        edh_or_added_card = add_card_to_hand(player, packed_state_or_iid);
        if (rv != NULL)
        {
          rv[commander_slot] = edh_or_added_card;
        }
        set_special_flags2(player, edh_or_added_card, 0x1000000);
        if (where == 5)
        {
          add_state(player, edh_or_added_card, 0x10);
        }
        if (where == 4 || where == 5)
          put_into_play(player, edh_or_added_card);
      }
      commander_slot = commander_slot + 1;
      if (commander_slot == 3)
      {
        return edh_or_added_card;
      }
      selected_slot = choice_or_priority & commander_slot;
      packed_state_or_iid = cid[commander_slot];
    } while (true);
  }
  return -1;
}

/* ManalinkEh 2022: 0x22c6990; original cards/vanguard.c:1877. */
int put_all_commanders_from_command_zone_to(int player, int where, int *rv)
{
  int result;

  result = put_commander_from_command_zone_to_impl(player, where, 3, rv);
  return result;
}

/* ManalinkEh 2022: 0x22c69d0; original cards/vanguard.c:1886. */
int get_commander_id(int player, int commander_code)
{
  int32_t edh_card;
  card_instance_t *instance;

  edh_card = locate_edh_card(player);
  if (edh_card != -1)
  {
    if (commander_code == 2)
    {
      instance = get_card_instance(player, edh_card);
      return instance->targets[2].card;
    }
    instance = get_card_instance(player, edh_card);
    edh_card = instance->targets[2].player;
  }
  return edh_card;
}

/* ManalinkEh 2022: 0x22c6a20; original cards/vanguard.c:1895. */
void put_commander_in_the_command_zone_from_play(int player, int card)
{
  uint16_t commander_csvid;
  uint16_t partner_csvid;
  int player_00;
  int iid;
  int card_00;
  int result;
  card_instance_t *instance;

  player_00 = get_owner(player, card);
  iid = get_original_internal_card_id(player, card);
  commander_csvid = cards_data[iid].id;
  card_00 = locate_edh_card(player_00);
  if (card_00 == -1)
  {
    return;
  }

  result = get_commander_id(player_00, 1);
  partner_csvid = cards_data[result].id;
  result = get_commander_id(player_00, 2);
  if (commander_csvid == cards_data[result].id)
  {
    dispatch_event(player, card, 0x1c00);
    get_card_instance(player_00, card_00);
  }
  else
  {
    dispatch_event(player, card, 0x1c00);
    instance = get_card_instance(player_00, card_00);
    if (commander_csvid != partner_csvid)
    {
      return;
    }
    *(uint16_t *)&instance->targets[3].player = 0x101;
  }
  add_card_to_rfg(player_00, iid);
  obliterate_card(player, card);
  return;
}

/* ManalinkEh 2022: 0x22c3b20; original cards/vanguard.c:1919. */
static int prismatic_piper_check(int player, int card, int card_color, int to_compare)
{
  card_instance_t *instance;
  unsigned int color_count_or_mask;
  unsigned int identity_color_count;

  instance = get_card_instance(player, card);
  if ((player == 1) && ((cards_data[instance->targets[2].player].id == CARD_ID_THE_PRISMATIC_PIPER ||
                         (cards_data[instance->targets[2].card].id == CARD_ID_THE_PRISMATIC_PIPER))))
  {
    color_count_or_mask = num_bits_set(card_color);
    identity_color_count = num_bits_set(to_compare);
    if (color_count_or_mask - 1 <= identity_color_count)
    {
      color_count_or_mask = card_color & (to_compare ^ 0xffffffffU);
      instance = get_card_instance(1, card);
      instance->targets[6].card = color_count_or_mask;
      return color_count_or_mask;
    }
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

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_edh(int player, int card, event_t event)
{
  enum
  {
    CHOICE_PLAY_COM1 = 1,
    CHOICE_PLAY_COM2 = 2,
    CHOICE_DEREVI_SPECIAL_ABILITY = 3,
    CHOICE_YURIKO_SPECIAL_ABILITY = 4
  };

  card_instance_t *parent;
  int i;
  int choice;
  int count;

  int *next_deck_entry;
  unsigned char card_colors;
  unsigned short selected_commander_csvid;
  card_ptr_t *card_description;
  bool yuriko_available;
  card_instance_t *instance;
  card_instance_t *scanned_or_damage_instance;
  int opponent;
  int first_commander_or_result;
  card_instance_t *other_edh_record;
  int choice_iid_or_result;
  int iid_count_or_result;
  const char *commander_name;
  int second_commander_or_result;
  counter_t type;
  unsigned int csvid_or_color_mask;
  int32_t total_mana_paid;
  int *next_or_scanned_deck_entry;
  unsigned int csvid;
  unsigned int csvid_colors_or_available;
  const char *deck_owner_text;
  unsigned int csvid_00;
  int *deck_or_mana_cursor;
  bool invalid_or_special_case;

  int32_t second_commander_iid;
  int32_t first_commander_iid;
  int deck_card_count;
  int message_length;
  unsigned int color_identity;
  unsigned int error_count_or_derevi_available;
  const char *second_commander_prompt;
  const char *first_commander_prompt;
  int temporary_commander_card;

  vanguard_card(player, card, event, 7, 0x28, 6);

  instance = get_card_instance(player, card);

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    int options[available_slots];

    for (i = 0; i < available_slots; ++i)
      options[i] = 0;
    deck_or_mana_cursor = deck_ptr[player];
    choice_iid_or_result = *deck_or_mana_cursor;
    next_or_scanned_deck_entry = deck_or_mana_cursor;
    while (choice_iid_or_result != -1)
    {
      choice_iid_or_result = is_legal_general(choice_iid_or_result);
      if (choice_iid_or_result != 0)
      {
        options[cards_data[*next_or_scanned_deck_entry].id] =
            options[cards_data[*next_or_scanned_deck_entry].id] + 1;
      }
      next_deck_entry = next_or_scanned_deck_entry + 1;
      next_or_scanned_deck_entry = next_or_scanned_deck_entry + 1;
      choice_iid_or_result = *next_deck_entry;
    }
    if (0 < active_cards_count[player])
    {
      i = 0;
      do
      {
        scanned_or_damage_instance = in_hand(player, i);
        if ((scanned_or_damage_instance != NULL) &&
            (iid_count_or_result = is_legal_general(scanned_or_damage_instance->internal_card_id),
             iid_count_or_result != 0))
        {
          options[cards_data[scanned_or_damage_instance->internal_card_id].id] =
              options[cards_data[scanned_or_damage_instance->internal_card_id].id] + 1;
        }
        i = i + 1;
      } while (i < active_cards_count[player]);
    }
    csvid_00 = 0xffffffff;
    csvid = 0xffffffff;
    for (csvid_or_color_mask = 0; csvid_or_color_mask < (unsigned int)available_slots;
         ++csvid_or_color_mask)
    {
      if (options[csvid_or_color_mask] > 1)
      {
        if (csvid == 0xffffffff)
          csvid = csvid_or_color_mask;
        else if (csvid_00 == 0xffffffff)
          csvid_00 = csvid_or_color_mask;
      }
    }
    if ((csvid & csvid_00) == 0xffffffff)
    {
      char buffer[600];
      if (player == 1)
      {
        deck_owner_text = "AI's";
      }
      else
      {
        deck_owner_text = "your";
      }
      snprintf(buffer, 600,
               "No general found in %s deck.\nPlace two copies of a legendary creature in the deck "
               "to identify him.",
               deck_owner_text);
      do_dialog(0, player, card, -1, -1, buffer, 0);
      lose_the_game(0);
      return 0;
    }
    if ((int)csvid >= 0 && (int)csvid_00 >= 0)
    {
      choice_iid_or_result = general_has_partner(csvid);
      if (((choice_iid_or_result == 0) ||
           (choice_iid_or_result = general_has_partner(csvid_00), choice_iid_or_result == 0)) &&
          ((csvid_00 != (unsigned int)get_partner_with_id(csvid))))
      {
        char buffer[600];
        snprintf(buffer, 600, "Two Commanders are allowed only if both have Partner.");
        do_dialog(0, player, card, -1, -1, buffer, 0);
        lose_the_game(0);
        return 0;
      }
    }
    first_commander_iid = get_internal_card_id_from_csv_id(csvid);
    card_colors = cards_data[first_commander_iid].color;
    color_identity = get_color_from_remainder_text(csvid);
    color_identity = card_colors | color_identity;
    if ((int)csvid_00 >= 0)
    {
      second_commander_iid = get_internal_card_id_from_csv_id(csvid_00);
      if (-1 < second_commander_iid)
      {
        card_colors = cards_data[second_commander_iid].color;
        csvid_colors_or_available = get_color_from_remainder_text(csvid_00);
        color_identity = card_colors | color_identity | csvid_colors_or_available;
      }
    }
    else
      second_commander_iid = -1;
    if (player == 0)
    {
      if ((csvid == CARD_ID_THE_PRISMATIC_PIPER) || (csvid_00 == 0x56c6))
      {
        csvid_colors_or_available = color_to_color_test(choose_a_color(0, 1));
        color_identity = color_identity | csvid_colors_or_available;
        instance->targets[6].card = csvid_colors_or_available;
      }
      deck_owner_text = "your";
      commander_name = cards_ptr[csvid]->name;
    }
    else
    {
      deck_owner_text = "AI's";
      commander_name = cards_ptr[csvid]->name;
      if (player != 1)
      {
        deck_owner_text = "your";
      }
    }
    char buffer[600];
    choice_iid_or_result =
        snprintf(buffer, 600, "%s general is %s\n", deck_owner_text, commander_name);
    if (-1 < (int)csvid_00)
    {
      snprintf(buffer + choice_iid_or_result, 600 - choice_iid_or_result, "%s general is %s\n",
               deck_owner_text, cards_ptr[csvid_00]->name);
    }
    do_dialog(0, player, card, -1, -1, buffer, 0);
    for (i = 0; i < available_slots; ++i)
      options[i] = 0;
    instance->targets[2].player = first_commander_iid;
    instance->targets[2].card = second_commander_iid;
    message_length = snprintf(buffer, 600, " Checking the deck...\n");
    invalid_or_special_case = false;
    deck_card_count = 0;
    error_count_or_derevi_available = 0;
    choice_iid_or_result = *deck_or_mana_cursor;
    while (choice_iid_or_result != -1)
    {
      csvid_colors_or_available = (unsigned int)cards_data[choice_iid_or_result].id;
      if ((csvid != csvid_colors_or_available && csvid_00 != csvid_colors_or_available) &&
          (csvid_colors_or_available != 0x3953))
      {
        deck_card_count = deck_card_count + 1;
        choice_iid_or_result = limit_allowed_in_deck(csvid_colors_or_available);
        iid_count_or_result = options[csvid_colors_or_available] + 1;
        options[csvid_colors_or_available] = iid_count_or_result;
        card_description = cards_ptr[csvid_colors_or_available];
        if ((1 < iid_count_or_result) &&
            ((choice_iid_or_result == 4 || (choice_iid_or_result < iid_count_or_result))))
        {
          choice_iid_or_result = snprintf(buffer + message_length, 600 - message_length,
                                          " Duplicate card: %s\n", card_description->name);
          message_length = message_length + choice_iid_or_result;
          error_count_or_derevi_available = error_count_or_derevi_available + 1;
        }
        card_colors = cards_data[*deck_or_mana_cursor].color;
        choice_iid_or_result = edh_special_case(*deck_or_mana_cursor);
        bool skip_remaining_checks = false;
        if ((choice_iid_or_result == 0) &&
            (csvid_or_color_mask = card_colors & 0x3e,
             (color_identity & csvid_or_color_mask) != csvid_or_color_mask))
        {
          if ((invalid_or_special_case) || (csvid_or_color_mask = prismatic_piper_check(
                                                player, card, csvid_or_color_mask, color_identity),
                                            csvid_or_color_mask == 0))
          {
            choice_iid_or_result = snprintf(buffer + message_length, 600 - message_length,
                                            " Illegal color: %s\n", card_description->name);
            message_length = message_length + choice_iid_or_result;
            error_count_or_derevi_available = error_count_or_derevi_available + 1;
          }
          else
          {
            invalid_or_special_case = true;
            color_identity |= csvid_or_color_mask;
            skip_remaining_checks = true;
          }
        }
        if (!skip_remaining_checks)
        {
          csvid_or_color_mask = get_color_from_remainder_text(csvid_colors_or_available);
          if ((color_identity & csvid_or_color_mask) != csvid_or_color_mask)
          {
            if ((!invalid_or_special_case) &&
                (csvid_or_color_mask =
                     prismatic_piper_check(player, card, csvid_or_color_mask, color_identity),
                 csvid_or_color_mask != 0))
            {
              invalid_or_special_case = true;
              color_identity |= csvid_or_color_mask;
              skip_remaining_checks = true;
            }
            else
            {
              choice_iid_or_result =
                  snprintf(buffer + message_length, 600 - message_length,
                           " Illegal mana symbols in text:\n %s\n", card_description->name);
              message_length = message_length + choice_iid_or_result;
              error_count_or_derevi_available = error_count_or_derevi_available + 1;
            }
          }
          if (!skip_remaining_checks)
          {
            if (((cards_ptr[csvid_colors_or_available]->flags & CPF_BANNED_COMMANDER) != CPF_0) ||
                (choice_iid_or_result =
                     has_subtype_by_id(player, csvid_colors_or_available, SUBTYPE_CONSPIRACY),
                 choice_iid_or_result != 0))
            {
              choice_iid_or_result = snprintf(buffer + message_length, 600 - message_length,
                                              " Banned card: %s\n", card_description->name);
              message_length = message_length + choice_iid_or_result;
              error_count_or_derevi_available = error_count_or_derevi_available + 1;
            }
          }
        }
      }
      next_or_scanned_deck_entry = deck_or_mana_cursor + 1;
      deck_or_mana_cursor = deck_or_mana_cursor + 1;
      choice_iid_or_result = *next_or_scanned_deck_entry;
    }
    if (0 < active_cards_count[player])
    {
      i = 0;
      do
      {
        scanned_or_damage_instance = in_hand(player, i);
        if (scanned_or_damage_instance != NULL)
        {
          scanned_or_damage_instance = get_card_instance(player, i);
          csvid_colors_or_available =
              (unsigned int)cards_data[scanned_or_damage_instance->internal_card_id].id;
          if ((csvid != csvid_colors_or_available && csvid_00 != csvid_colors_or_available) &&
              (csvid_colors_or_available != 0x3953))
          {
            deck_card_count = deck_card_count + 1;
            iid_count_or_result = limit_allowed_in_deck(csvid_colors_or_available);
            card_description = cards_ptr[csvid_colors_or_available];
            first_commander_or_result = options[csvid_colors_or_available] + 1;
            options[csvid_colors_or_available] = first_commander_or_result;
            if ((1 < first_commander_or_result) &&
                ((iid_count_or_result < first_commander_or_result || (iid_count_or_result == 4))))
            {
              iid_count_or_result = snprintf(buffer + message_length, 600 - message_length,
                                             " Duplicate card: %s\n", card_description->name);
              message_length = message_length + iid_count_or_result;
              error_count_or_derevi_available = error_count_or_derevi_available + 1;
            }
            iid_count_or_result = get_original_internal_card_id(player, i);
            card_colors = cards_data[iid_count_or_result].color;
            iid_count_or_result = edh_special_case(scanned_or_damage_instance->internal_card_id);
            bool skip_remaining_checks = false;
            if ((iid_count_or_result == 0) &&
                (csvid_or_color_mask = card_colors & 0x3e,
                 (color_identity & csvid_or_color_mask) != csvid_or_color_mask))
            {
              if ((invalid_or_special_case) ||
                  (csvid_or_color_mask =
                       prismatic_piper_check(player, card, csvid_or_color_mask, color_identity),
                   csvid_or_color_mask == 0))
              {
                iid_count_or_result = snprintf(buffer + message_length, 600 - message_length,
                                               " Illegal color: %s\n", card_description->name);
                message_length = message_length + iid_count_or_result;
                error_count_or_derevi_available = error_count_or_derevi_available + 1;
              }
              else
              {
                invalid_or_special_case = true;
                color_identity |= csvid_or_color_mask;
                skip_remaining_checks = true;
              }
            }
            if (!skip_remaining_checks)
            {
              csvid_or_color_mask = get_color_from_remainder_text(csvid_colors_or_available);
              if ((color_identity & csvid_or_color_mask) != csvid_or_color_mask)
              {
                if ((!invalid_or_special_case) &&
                    (csvid_or_color_mask =
                         prismatic_piper_check(player, card, csvid_or_color_mask, color_identity),
                     csvid_or_color_mask != 0))
                {
                  invalid_or_special_case = true;
                  color_identity |= csvid_or_color_mask;
                  skip_remaining_checks = true;
                }
                else
                {
                  iid_count_or_result =
                      snprintf(buffer + message_length, 600 - message_length,
                               " Illegal mana symbols in text:\n %s\n", card_description->name);
                  message_length = message_length + iid_count_or_result;
                  error_count_or_derevi_available = error_count_or_derevi_available + 1;
                }
              }
              if (!skip_remaining_checks)
              {
                if ((cards_ptr[csvid_colors_or_available]->flags & CPF_BANNED_COMMANDER) != CPF_0)
                {
                  iid_count_or_result = snprintf(buffer + message_length, 600 - message_length,
                                                 " Banned card: %s\n", card_description->name);
                  message_length = message_length + iid_count_or_result;
                  error_count_or_derevi_available = error_count_or_derevi_available + 1;
                }
              }
            }
          }
        }
        i = i + 1;
      } while (i < active_cards_count[player]);
    }
    if (((csvid_00 == 0xffffffff) && (deck_card_count != 99)) ||
        ((deck_card_count != 0x62 && ((int)(csvid_00 ^ 0xffffffff) < 0))))
    {
      snprintf(buffer + message_length, 600 - message_length, " Illegal deck size: %d\n",
               deck_card_count + 1);
      error_count_or_derevi_available = error_count_or_derevi_available + 1;
    }
    if (error_count_or_derevi_available != 0)
    {
      do_dialog(0, player, card, -1, -1, buffer, 0);
      lose_the_game(0);
      return 0;
    }
    instance->targets[2].player = first_commander_iid;
    instance->targets[2].card = second_commander_iid;
    create_card_name_legacy(player, card, csvid);
    if ((int)csvid_00 < 0)
    {
      instance->targets[3].player = 1;
    }
    else
    {
      create_card_name_legacy(player, card, csvid_00);
      instance->targets[3].player = 0x101;
    }
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {

    choice_iid_or_result = can_play_commander(player, card, EVENT_CAN_ACTIVATE, 1);

    iid_count_or_result = can_play_commander(player, card, EVENT_CAN_ACTIVATE, 2);
    if ((choice_iid_or_result == 99) || (iid_count_or_result == 99))
    {
      return 99;
    }
    if (choice_iid_or_result != 0 || iid_count_or_result != 0)
    {
      return 1;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    char msg[100];
    char msg2[100];

    first_commander_or_result = can_play_commander(player, card, EVENT_ACTIVATE, 1);

    second_commander_or_result = can_play_commander(player, card, EVENT_ACTIVATE, 2);
    choice_iid_or_result = instance->targets[2].player;
    iid_count_or_result = instance->targets[2].card;
    if (iid_count_or_result < 0)
    {
      invalid_or_special_case = first_commander_or_result == 2 || second_commander_or_result == 2;
      yuriko_available = first_commander_or_result == 4 || second_commander_or_result == 4;

      scnprintf(msg, 100, "Play %s", cards_ptr[cards_data[choice_iid_or_result].id]->name);
      csvid_colors_or_available = 0;
    }
    else
    {
      csvid_colors_or_available =
          (unsigned int)(second_commander_or_result == 1 || second_commander_or_result == 99);
      invalid_or_special_case = first_commander_or_result == 2 || second_commander_or_result == 2;
      yuriko_available = first_commander_or_result == 4 || second_commander_or_result == 4;

      scnprintf(msg, 100, "Play %s", cards_ptr[cards_data[choice_iid_or_result].id]->name);

      scnprintf(msg2, 100, "Play %s", cards_ptr[cards_data[iid_count_or_result].id]->name);
    }
    error_count_or_derevi_available = (unsigned int)invalid_or_special_case;
    first_commander_prompt = msg;
    second_commander_prompt = msg2;

    choice = dialog_impl(
        player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_OMIT_ILLEGAL, DLG_AUTOCHOOSE_IF_1, first_commander_prompt,
        (unsigned int)(first_commander_or_result == 99 || first_commander_or_result == 1), 1,
        second_commander_prompt, csvid_colors_or_available, 1, "Put Derevi into play",
        error_count_or_derevi_available, 5, "Put Yuriko into play", (unsigned int)yuriko_available,
        5, 0, 0, 0, 0);
    if (choice == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (choice == CHOICE_DEREVI_SPECIAL_ABILITY)
    {

      charge_mana_multi(player, 1, 0, 1, 1, 0, 1);
      if (spell_fizzled != 1)
      {

        remove_card_from_rfg(player, 0x352e);
        return 0;
      }
      return 0;
    }
    if (choice == CHOICE_YURIKO_SPECIAL_ABILITY)
    {

      target_definition_t td;
      base_target_definition(player, card, &td, 2);
      td.allowed_controller = player;
      td.preferred_controller = player;
      td.required_state = TARGET_STATE_ATTACKING;
      td.illegal_state = TARGET_STATE_ISBLOCKED;
      td.illegal_abilities = 0;

      choice_iid_or_result = can_target(&td);
      if (choice_iid_or_result == 0)
      {
        return 0;
      }

      choice_iid_or_result = charge_mana_multi(player, 0, 1, 1, 0, 0, 0);
      if (choice_iid_or_result == 0)
      {
        return 0;
      }

      choice_iid_or_result = new_pick_target(&td, "NINJUTSU", 0, 1);
      if (choice_iid_or_result == 0)
      {
        return 0;
      }

      bounce_permanent(instance->targets[0].player, instance->targets[0].card);

      remove_card_from_rfg(player, 0x4abe);
      return 1;
    }
    if (choice == CHOICE_PLAY_COM2)
    {
      selected_commander_csvid = cards_data[iid_count_or_result].id;
      instance->targets[3].card = 0;
      first_commander_or_result = iid_count_or_result;
    }
    else
    {
      selected_commander_csvid = cards_data[choice_iid_or_result].id;
      instance->targets[3].card = 0;
      first_commander_or_result = choice_iid_or_result;
    }
    csvid_colors_or_available = (unsigned int)selected_commander_csvid;
    instance->targets[5].card = first_commander_or_result;
    if (csvid_colors_or_available == 0x392e || csvid_colors_or_available == 0x4ea3)
    {
      temporary_commander_card = add_card_to_hand(player, first_commander_or_result);
      hand_count[player] -= 1;
      add_state(player, temporary_commander_card, 0x20);
      if (csvid_colors_or_available == 0x392e)
        first_commander_or_result = cast_spell_with_delve(player, temporary_commander_card);
      else
        first_commander_or_result =
            cast_hogaak(player, temporary_commander_card, EVENT_ACTIVATE, 0);
      obliterate_card(player, temporary_commander_card);
      instance->targets[5].card = -1;
      if (first_commander_or_result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
    else
    {
      if (csvid_colors_or_available == 0x5690)
      {
        instance->targets[5].card = -1;

        charge_mana_from_id(player, -1, EVENT_ACTIVATE, 0x5690);
        if (spell_fizzled != 1)
        {
          count = count_counters(player, card, COUNTER_VORTEX);
          lose_life(player, count);
        }
      }
      else
      {

        charge_mana_from_id(player, -1, EVENT_ACTIVATE, csvid_colors_or_available);
        instance->targets[5].card = -1;
      }
    }
    if (spell_fizzled != 1)
    {

      remove_card_from_rfg(player, csvid_colors_or_available);
      total_mana_paid = instance->targets[3].card;
      first_commander_or_result = 0;
      do
      {
        deck_or_mana_cursor = mana_paid + first_commander_or_result;
        first_commander_or_result = first_commander_or_result + 1;
        total_mana_paid = total_mana_paid + *deck_or_mana_cursor;
      } while (first_commander_or_result != 7);
      instance->targets[3].card = total_mana_paid;
      if (cards_data[choice_iid_or_result].id == csvid_colors_or_available)
      {
        type = COUNTER_VORTEX;
      }
      else
      {
        if (cards_data[iid_count_or_result].id != csvid_colors_or_available)
        {

          backtrace_and_abort("cards/vanguard.c:2262\ngeneral_csvid == cards_data[general_iid].id "
                              "|| general_csvid == cards_data[general_iid2].id");
        }
        type = COUNTER_VELOCITY;
      }

      add_counters(player, card, player, card, type, 2);
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {

    parent = get_card_instance(instance->parent_controller, instance->parent_card);
    choice_iid_or_result = instance->info_slot;
    if (choice_iid_or_result == CHOICE_DEREVI_SPECIAL_ABILITY)
    {
      *(uint8_t *)&parent->targets[3].player = 4;

      choice_iid_or_result = add_card_to_hand(player, get_internal_card_id_from_csv_id(CARD_ID_DEREVI_EMPYRIAL_TACTICIAN));

      set_special_flags2(player, choice_iid_or_result, 0x1000000);

      put_into_play(player, choice_iid_or_result);
    }
    else if (choice_iid_or_result == CHOICE_YURIKO_SPECIAL_ABILITY)
    {
      *(uint8_t *)&parent->targets[3].player = 4;

      choice_iid_or_result = add_card_to_hand(player, get_internal_card_id_from_csv_id(CARD_ID_YURIKO_THE_TIGERS_SHADOW));

      set_special_flags2(player, choice_iid_or_result, 0x1000000);

      scanned_or_damage_instance = get_card_instance(player, choice_iid_or_result);
      scanned_or_damage_instance->state = scanned_or_damage_instance->state | 0x14;

      put_into_play(player, choice_iid_or_result);
    }
    else
    {
      if (choice_iid_or_result == CHOICE_PLAY_COM2)
      {
        selected_commander_csvid = cards_data[instance->targets[2].card].id;
        SET_BYTE1(parent->targets[3].player) = 4;
      }
      else
      {
        selected_commander_csvid = cards_data[instance->targets[2].player].id;
        *(uint8_t *)&parent->targets[3].player = 4;
      }

      play_card_in_exile_for_free_commander(player, player, (unsigned int)selected_commander_csvid,
                                            instance->targets[3].card);
    }
  }
  else if (((event == EVENT_CAST_SPELL) && (affected_card_controller == player)) &&
           (choice_iid_or_result = instance->targets[3].player, -1 < choice_iid_or_result))
  {
    iid_count_or_result = instance->targets[2].card;
    if ((char)choice_iid_or_result == '\x04')
    {
      choice_iid_or_result = instance->targets[2].player;

      first_commander_or_result = is_token(player, affected_card);
      if (first_commander_or_result == 0)
      {

        first_commander_or_result =
            get_original_internal_card_id(affected_card_controller, affected_card);
        if (first_commander_or_result == choice_iid_or_result)
        {

          commander_enters_the_battlefield(player, card, affected_card_controller, affected_card,
                                           1);
        }
      }
    }
    if ((-1 < iid_count_or_result) && ((char)SET_BYTE1(instance->targets[3].player) == '\x04'))
    {

      choice_iid_or_result = is_token(affected_card_controller, affected_card);
      if (choice_iid_or_result == 0)
      {

        choice_iid_or_result =
            get_original_internal_card_id(affected_card_controller, affected_card);
        if (choice_iid_or_result == iid_count_or_result)
        {

          commander_enters_the_battlefield(player, card, affected_card_controller, affected_card,
                                           2);
        }
      }
    }
  }

  scanned_or_damage_instance = combat_damage_being_dealt(event);
  if (scanned_or_damage_instance != NULL)
  {
    choice_iid_or_result = instance->targets[4].player;
    iid_count_or_result = instance->targets[4].card;
    opponent = 1 - player;

    other_edh_record = get_card_instance(opponent, locate_edh_card(opponent));
    first_commander_or_result = other_edh_record->targets[4].player;
    second_commander_or_result = other_edh_record->targets[4].card;
    if (((-1 < choice_iid_or_result) &&
         (scanned_or_damage_instance->damage_source_card == choice_iid_or_result)) &&
        ((scanned_or_damage_instance->damage_source_player == player &&
          ((scanned_or_damage_instance->damage_target_card == -1 &&
            (opponent == scanned_or_damage_instance->damage_target_player))))))
    {

      choice_iid_or_result = damage_is_to_planeswalker(scanned_or_damage_instance);
      if (choice_iid_or_result == 0)
      {

        add_counters(player, card, player, card, COUNTER_DEATH,
                     scanned_or_damage_instance->info_slot);

        choice_iid_or_result = count_counters(player, card, COUNTER_DEATH);
        if (0x14 < choice_iid_or_result)
        {

          lose_the_game(opponent);
        }
      }
    }
    if ((((-1 < iid_count_or_result) &&
          (scanned_or_damage_instance->damage_source_card == iid_count_or_result)) &&
         (scanned_or_damage_instance->damage_source_player == player)) &&
        ((scanned_or_damage_instance->damage_target_card == -1 &&
          (opponent == scanned_or_damage_instance->damage_target_player))))
    {

      choice_iid_or_result = damage_is_to_planeswalker(scanned_or_damage_instance);
      if (choice_iid_or_result == 0)
      {

        add_counters(player, card, player, card, COUNTER_CARRION,
                     scanned_or_damage_instance->info_slot);

        choice_iid_or_result = count_counters(player, card, COUNTER_CARRION);
        if (0x14 < choice_iid_or_result)
        {

          lose_the_game(opponent);
        }
      }
    }
    if (((-1 < first_commander_or_result) &&
         (scanned_or_damage_instance->damage_source_card == first_commander_or_result)) &&
        ((scanned_or_damage_instance->damage_source_player == player &&
          ((scanned_or_damage_instance->damage_target_card == -1 &&
            (opponent == scanned_or_damage_instance->damage_target_player))))))
    {

      choice_iid_or_result = damage_is_to_planeswalker(scanned_or_damage_instance);
      if (choice_iid_or_result == 0)
      {

        add_counters(player, card, player, card, COUNTER_DOOM,
                     scanned_or_damage_instance->info_slot);

        choice_iid_or_result = count_counters(player, card, COUNTER_DOOM);
        if (0x14 < choice_iid_or_result)
        {

          lose_the_game(opponent);
        }
      }
    }
    if ((((-1 < second_commander_or_result) &&
          (scanned_or_damage_instance->damage_source_card == second_commander_or_result)) &&
         (scanned_or_damage_instance->damage_source_player == player)) &&
        ((scanned_or_damage_instance->damage_target_card == -1 &&
          (opponent == scanned_or_damage_instance->damage_target_player))))
    {

      choice_iid_or_result = damage_is_to_planeswalker(scanned_or_damage_instance);
      if (choice_iid_or_result == 0)
      {

        add_counters(player, card, player, card, COUNTER_PLAGUE,
                     scanned_or_damage_instance->info_slot);

        choice_iid_or_result = count_counters(player, card, COUNTER_PLAGUE);
        if (0x14 < choice_iid_or_result)
        {

          lose_the_game(opponent);
        }
      }
    }
  }
  if (trigger_condition == TRIGGER_UPKEEP && card == affected_card && player == affected_card_controller &&
      reason_for_trigger_controller == player)
  {
    int second_iid = instance->targets[2].card;
    unsigned int first_id = cards_data[instance->targets[2].player].id;
    unsigned int second_id = second_iid < 0 ? 0xffffffffU : cards_data[second_iid].id;
    if ((first_id == CARD_ID_OLORO_AGELESS_ASCETIC || second_id == CARD_ID_OLORO_AGELESS_ASCETIC) &&
        current_turn == player)
    {
      int commander_tax = count_counters(player, card, COUNTER_VORTEX);
      bool eminence_available = commander_tax == 0;
      commander_tax = count_counters(player, card, COUNTER_VORTEX);
      if (commander_tax > 0)
      {
        int in_exile = check_rfg(player, CARD_ID_OLORO_AGELESS_ASCETIC);
        if (in_exile != 0)
        {
          int commander_positions = instance->targets[3].player;
          if (commander_positions >= 0 &&
              ((first_id == CARD_ID_OLORO_AGELESS_ASCETIC && (char)commander_positions == 1) ||
               (second_id == CARD_ID_OLORO_AGELESS_ASCETIC &&
                (char)((unsigned int)commander_positions >> 8) == 1)))
            eminence_available = true;
        }
      }
      if (eminence_available)
      {
        choice_iid_or_result = count_upkeeps(player);
        if (choice_iid_or_result != 0)
        {
          if (event == EVENT_TRIGGER)
            event_result |= 2;
          else if (event == EVENT_RESOLVE_TRIGGER)
            gain_life(player, choice_iid_or_result * 2);
        }
      }
    }
  }
  if (event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player)
  {
    int second_iid = instance->targets[2].card;
    unsigned int first_id = cards_data[instance->targets[2].player].id;
    unsigned int second_id = second_iid < 0 ? 0xffffffffU : cards_data[second_iid].id;
    if (first_id == CARD_ID_THE_UR_DRAGON || second_id == CARD_ID_THE_UR_DRAGON)
    {
      int commander_tax = count_counters(player, card, COUNTER_VORTEX);
      bool eminence_available = commander_tax == 0;
      commander_tax = count_counters(player, card, COUNTER_VORTEX);
      if (commander_tax > 0)
      {
        int in_exile = check_rfg(player, CARD_ID_THE_UR_DRAGON);
        if (in_exile != 0)
        {
          int commander_positions = instance->targets[3].player;
          if (commander_positions >= 0 &&
              ((first_id == CARD_ID_THE_UR_DRAGON && (char)commander_positions == 1) ||
               (second_id == CARD_ID_THE_UR_DRAGON &&
                (char)((unsigned int)commander_positions >> 8) == 1)))
            eminence_available = true;
        }
      }
      if (eminence_available)
      {
        choice_iid_or_result = is_what(affected_card_controller, affected_card, 2);
        if (choice_iid_or_result != 0)
        {
          choice_iid_or_result =
              has_subtype(affected_card_controller, affected_card, SUBTYPE_DRAGON);
          if (choice_iid_or_result != 0)
            COST_COLORLESS -= 1;
        }
      }
    }
  }
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && card == affected_card && player == affected_card_controller &&
      reason_for_trigger_controller == player)
  {
    int second_iid = instance->targets[2].card;
    unsigned int first_id = cards_data[instance->targets[2].player].id;
    unsigned int second_id = second_iid < 0 ? 0xffffffffU : cards_data[second_iid].id;
    if (first_id == CARD_ID_INALLA_ARCHMAGE_RITUALIST ||
        second_id == CARD_ID_INALLA_ARCHMAGE_RITUALIST)
    {
      int commander_tax = count_counters(player, card, COUNTER_VORTEX);
      bool eminence_available = commander_tax == 0;
      commander_tax = count_counters(player, card, COUNTER_VORTEX);
      if (commander_tax > 0)
      {
        int in_exile = check_rfg(player, CARD_ID_INALLA_ARCHMAGE_RITUALIST);
        if (in_exile != 0)
        {
          int commander_positions = instance->targets[3].player;
          if (commander_positions >= 0 &&
              ((first_id == CARD_ID_INALLA_ARCHMAGE_RITUALIST && (char)commander_positions == 1) ||
               (second_id == CARD_ID_INALLA_ARCHMAGE_RITUALIST &&
                (char)((unsigned int)commander_positions >> 8) == 1)))
            eminence_available = true;
        }
      }
      if (eminence_available)
      {
        test_definition_t test;
        default_test_definition(&test, 2);
        test.subtype = 0xd6;
        test.not_me = 1;
        test.type_flag = 3;
        choice_iid_or_result = duh_mode(player);
        etb_ability_global(player, event, (choice_iid_or_result != 0) + 1, TYPE_NONE, &test, player,
                           card);
      }
    }
  }
  if (event == EVENT_ETB)
  {
    choice_iid_or_result = has_mana_multi(player, 1, 0, 0, 0, 0, 0);
    if (choice_iid_or_result != 0)
    {
      choice_iid_or_result = charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER,
                                                               player, 1, 0, 0, 0, 0, 0);
      if (choice_iid_or_result != 0)
      {
        token_generation_t token;
        copy_token_definition(player, card, &token, trigger_cause_controller, trigger_cause);
        token.special_code_on_generation = haste_and_remove_token_at_end_of_next_turn;
        generate_token(&token);
      }
    }
  }
  if (trigger_condition == TRIGGER_SPELL_CAST && card == affected_card && player == affected_card_controller &&
      reason_for_trigger_controller == player)
  {
    int second_iid = instance->targets[2].card;
    unsigned int first_id = cards_data[instance->targets[2].player].id;
    unsigned int second_id = second_iid < 0 ? 0xffffffffU : cards_data[second_iid].id;
    if (first_id == CARD_ID_EDGAR_MARKOV || second_id == CARD_ID_EDGAR_MARKOV)
    {
      int commander_tax = count_counters(player, card, COUNTER_VORTEX);
      bool eminence_available = commander_tax == 0;
      commander_tax = count_counters(player, card, COUNTER_VORTEX);
      if (commander_tax > 0)
      {
        int in_exile = check_rfg(player, CARD_ID_EDGAR_MARKOV);
        if (in_exile != 0)
        {
          int commander_positions = instance->targets[3].player;
          if (commander_positions >= 0 &&
              ((first_id == CARD_ID_EDGAR_MARKOV && (char)commander_positions == 1) ||
               (second_id == CARD_ID_EDGAR_MARKOV &&
                (char)((unsigned int)commander_positions >> 8) == 1)))
            eminence_available = true;
        }
      }
      if (eminence_available)
      {
        test_definition_t test;
        default_test_definition(&test, 2);
        test.subtype = 0xcd;
        test.not_me = 1;
        choice_iid_or_result = new_specific_spell_played(player, card, event, player, 2, &test);
        if (choice_iid_or_result != 0)
        {
          token_generation_t token;
          default_token_definition(player, card, CARD_ID_VAMPIRE, &token);
          token.tou = 1;
          token.pow = 1;
          generate_token(&token);
        }
      }
    }
  }
  choice_iid_or_result = beginning_of_combat(player, card, event, player);
  if (choice_iid_or_result != 0)
  {
    int second_iid = instance->targets[2].card;
    unsigned int first_id = cards_data[instance->targets[2].player].id;
    unsigned int second_id = second_iid < 0 ? 0xffffffffU : cards_data[second_iid].id;
    if (first_id == CARD_ID_ARAHBO_ROAR_OF_THE_WORLD ||
        second_id == CARD_ID_ARAHBO_ROAR_OF_THE_WORLD)
    {
      int commander_tax = count_counters(player, card, COUNTER_VORTEX);
      bool eminence_available = commander_tax == 0;
      commander_tax = count_counters(player, card, COUNTER_VORTEX);
      if (commander_tax > 0)
      {
        int in_exile = check_rfg(player, CARD_ID_ARAHBO_ROAR_OF_THE_WORLD);
        if (in_exile != 0)
        {
          int commander_positions = instance->targets[3].player;
          if (commander_positions >= 0 &&
              ((first_id == CARD_ID_ARAHBO_ROAR_OF_THE_WORLD && (char)commander_positions == 1) ||
               (second_id == CARD_ID_ARAHBO_ROAR_OF_THE_WORLD &&
                (char)((unsigned int)commander_positions >> 8) == 1)))
            eminence_available = true;
        }
      }
      if (eminence_available)
      {
        target_definition_t td;
        default_target_definition(player, card, &td, 2);
        td.required_subtype = 0x1f;
        td.allowed_controller = player;
        td.preferred_controller = player;
        td.special = TARGET_SPECIAL_NOT_ME;
        instance->number_of_targets = 0;
        choice_iid_or_result = can_target(&td);
        if (choice_iid_or_result != 0)
        {
          choice_iid_or_result = pick_target(&td, "Select target Cat you control");
          if (choice_iid_or_result != 0)
            pump_ability_until_eot(player, card, instance->targets[0].player,
                                   instance->targets[0].card, 3, 3, 0, 0);
        }
      }
    }
  }
  if ((event == EVENT_CLEANUP) &&
      (choice_iid_or_result = instance->targets[3].player, -1 < choice_iid_or_result))
  {
    if (((char)choice_iid_or_result == '\x02') &&
        (iid_count_or_result = instance->targets[4].player, -1 < iid_count_or_result))
    {

      scanned_or_damage_instance = in_play(player, iid_count_or_result);
      if (scanned_or_damage_instance == NULL)
      {
        *(uint8_t *)&instance->targets[3].player = 4;
        instance->targets[4].player = -1;
        choice_iid_or_result = instance->targets[3].player;
      }
      else
      {
        choice_iid_or_result = instance->targets[3].player;
      }
    }
    if (((char)((unsigned int)choice_iid_or_result >> 8) == '\x02') &&
        (choice_iid_or_result = instance->targets[4].card, -1 < choice_iid_or_result))
    {

      scanned_or_damage_instance = in_play(player, choice_iid_or_result);
      if (scanned_or_damage_instance == NULL)
      {
        instance->targets[4].card = -1;
        SET_BYTE1(instance->targets[3].player) = 4;
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

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int get_commander_storm_count(int player);
int put_all_commanders_from_command_zone_to(int player, int where, int *rv);
int get_commander_id(int player, int commander_code);
static int put_commander_from_command_zone_to_impl(int player, int where, int which_com, int *rv);
void put_commander_in_the_command_zone_from_play(int player, int card);

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

#ifndef SHANDALAR
/* Recovered October 2022 flag and combat-query consumers. */
int general_has_partner(int csvid);
int get_partner_with_id(int csvid);

/* ManalinkEh 2022: 0x22c63c0; original cards/vanguard.c:1466. */

/* ManalinkEh 2022: 0x22c6410; original cards/vanguard.c:1531. */
int get_partner_with_id(int csvid)
{
  switch (csvid)
  {
  case CARD_ID_GORM_THE_GREAT:
    return CARD_ID_VIRTUS_THE_VEILED;
  case CARD_ID_KHORVATH_BRIGHTFLAME:
    return CARD_ID_SYLVIA_BRIGHTSPEAR;
  case CARD_ID_KRAV_THE_UNREDEEMED:
    return CARD_ID_REGNA_THE_REDEEMER;
  case CARD_ID_OKAUN_EYE_OF_CHAOS:
    return CARD_ID_ZNDRSPLT_EYE_OF_WISDOM;
  case CARD_ID_PIR_IMAGINATIVE_RASCAL:
    return CARD_ID_TOOTHY_IMAGINARY_FRIEND;
  case CARD_ID_ROWAN_KENRITH:
    return CARD_ID_WILL_KENRITH;
  case CARD_ID_SYLVIA_BRIGHTSPEAR:
    return CARD_ID_KHORVATH_BRIGHTFLAME;
  case CARD_ID_TOOTHY_IMAGINARY_FRIEND:
    return CARD_ID_PIR_IMAGINATIVE_RASCAL;
  case CARD_ID_VIRTUS_THE_VEILED:
    return CARD_ID_GORM_THE_GREAT;
  case CARD_ID_WILL_KENRITH:
    return CARD_ID_ROWAN_KENRITH;
  case CARD_ID_ZNDRSPLT_EYE_OF_WISDOM:
    return CARD_ID_OKAUN_EYE_OF_CHAOS;
  default:
    return -1;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22c6540; original cards/vanguard.c:1686. */
int is_commander_in_command_zone(int player)
{
  int edh_card = locate_edh_card(player);
  if (edh_card != -1)
  {
    int32_t state = get_card_instance(player, edh_card)->targets[3].player;
    if (state >= 0)
      return ((state >> 8) | state) & 1;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22c69b0; original cards/vanguard.c:1881. */
int put_commander_from_command_zone_to(int player, int where)
{
  int result;
  result = put_commander_from_command_zone_to_impl(player, where, 0, NULL);
  return result;
}

#endif
