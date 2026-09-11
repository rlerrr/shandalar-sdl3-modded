#include "manalink.h"

// global functions

int has_convoked_mana_extended_generic(int player, int card, event_t event, int cless, int black, int blue, int green, int red, int white, int cmc_modifier){
	int up_cless = (cless == 40 || cless == -1) ? 0 : cless; // cless == 40 or -1 meaning 'X in casting cost'.
	if( cless != 40 && cless != -1 && cmc_modifier ){
		up_cless = get_updated_casting_cost(player, card, -1, event, cless);
	}
	int tapped[active_cards_count[player]];
	int tc = 0;
	int tapped_for_color[active_cards_count[player]][5];
	int i,k;
	for(i=0; i<active_cards_count[player]; i++){
		for(k=0; k<5; k++){
			tapped_for_color[i][k] = 0;
		}
	}
	int rval = 0;
	int attempt = 0;
	while( ! rval && attempt < 50 ){
		int clrs[6] = {up_cless, black, blue, green, red, white};
		for(i=0; i<active_cards_count[player]; i++){
			if( in_play(player, i) && ! is_tapped(player, i) && is_what(player, i, TYPE_CREATURE) ){
				int clr = get_color(player, i);
				int col_found = 0;
				for(k=1; k<6; k++){
					if( (clr & (1<<k)) && clrs[k] > 0 && tapped_for_color[i][k-1] != 1){
						clrs[k]--;
						col_found = 1;
						tapped_for_color[i][k-1] = 1;
						add_state(player, i, STATE_TAPPED);
						tapped[tc] = i;
						tc++;
						break;
					}
				}
				if( ! col_found && clrs[0]){
					clrs[0]--;
					add_state(player, i, STATE_TAPPED);
					tapped[tc] = i;
					tc++;
				}
			}
			if( clrs[0]+clrs[1]+clrs[2]+clrs[3]+clrs[4]+clrs[5] == 0 ){
				break;
			}
		}
		rval = has_mana_multi(player, clrs[0], clrs[1], clrs[2], clrs[3], clrs[4], clrs[5]);
		for(i=0; i<tc; i++){
			remove_state(player, tapped[i], STATE_TAPPED);
		}
		if( rval && tc > 0 ){
			rval = 2;
		}
		tc = 0;
		attempt++;
	}
	return rval;
}

int has_convoked_mana_extended(int player, int card, event_t event, int cless, int black, int blue, int green, int red, int white){
	return has_convoked_mana_extended_generic(player, card, event, cless, black, blue, green, red, white, 1);
}

int has_convoked_mana(int player, int card, int cless, int green, int white ){

	return has_convoked_mana_extended(player, card, EVENT_CAN_CAST, cless, 0, 0, green, 0, white);
}

static int will_tap_permanent_for_convoke(int player, int card, int *clrs){
	int clr = get_color(player, card);
	if( clr == COLOR_COLORLESS ){
		if( clrs[0] ){
			clrs[0]--;
			return 1;
		}
	}
	else if( real_count_colors(clr) == 1 ){
			int k;
			int col_found = 0;
			for(k=1; k<6; k++){
				if( (clr & (1<<k)) && clrs[k] > 0 ){
					clrs[k]--;
					return 1;
				}
			}
			if( ! col_found ){
				if( clrs[0] ){
					clrs[0]--;
					return 1;
				}
			}
	}
	else{
		int choice = DIALOG(player, card, EVENT_CAST_SPELL,
							DLG_NO_STORAGE,
							"Tap for Colorless", clrs[COLOR_COLORLESS], 0,
							"Tap for Black", (clr & COLOR_TEST_COLORLESS) && clrs[COLOR_BLACK], 0,
							"Tap for Blue", (clr & COLOR_TEST_BLUE) && clrs[COLOR_BLUE], 0,
							"Tap for Green", (clr & COLOR_TEST_GREEN) && clrs[COLOR_GREEN], 0,
							"Tap for Red", (clr & COLOR_TEST_RED) && clrs[COLOR_RED], 0,
							"Tap for White", (clr & COLOR_TEST_WHITE) && clrs[COLOR_WHITE], 0);
		if( choice ){
			clrs[choice-1]--;
			return 1;
		}
	}
	return 0;
}

int charge_convoked_mana_extended_generic(int player, int card, event_t event, int cless, int black, int blue, int green, int red, int white, int check_for_cc_modifier){
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;
	td.illegal_abilities = 0;

	int up_cless = (cless == 40 || cless == -1) ? 0 : cless; // cless == 40 or -1 meaning 'X in casting cost'.
	if( check_for_cc_modifier ){
		up_cless = get_updated_casting_cost(player, card, -1, event, cless);
	}
	int x_flag = (cless == 40 || cless == -1) ? 1 : 0;

	int tapped[active_cards_count[player]];
	int tc = 0;

	card_instance_t *instance = get_card_instance( player, card );

	int plus = 0;
	int mode = 0;
	if( can_target(&td) ){
		if( has_mana_to_cast_id(player, event, get_id(player, card)) ){
			int ai_choice = is_what(player, card, TYPE_INSTANT | TYPE_INTERRUPT) && current_turn != player ? 0 : 1;
			mode = do_dialog(player, player, card, -1, -1, " Use Convoke\n Play normally\n Cancel", ai_choice);
		}
	}
	else{
		mode = 1;
	}

	int creatures_tapped_for_convoke = 0;
	if( mode == 0 ){
		if( player == HUMAN ){
			int clrs[6] = {up_cless, black, blue, green, red, white};
			while( can_target(&td) ){
					if( select_target(player, card, &td, "Select a creature to tap for Convoke.", NULL) ){
						instance->number_of_targets = 0;
						if( will_tap_permanent_for_convoke(instance->targets[0].player, instance->targets[0].card, clrs) ){
							add_state(instance->targets[0].player, instance->targets[0].card, STATE_TAPPED);
							tapped[tc] = instance->targets[0].card;
							tc++;
							creatures_tapped_for_convoke++;
						}
						else if( x_flag ){
								plus++;
								add_state(instance->targets[0].player, instance->targets[0].card, STATE_TAPPED);
								tapped[tc] = instance->targets[0].card;
								tc++;
								creatures_tapped_for_convoke++;
						}
						if( clrs[0]+clrs[1]+clrs[2]+clrs[3]+clrs[4]+clrs[5] == 0 && ! x_flag ){
							break;
						}
					}
					else{
						break;
					}
			}
			charge_mana_multi(player, clrs[0], clrs[1], clrs[2], clrs[3], clrs[4], clrs[5]);
			if( x_flag && has_mana(player, COLOR_COLORLESS, 1) ){
				charge_mana(player, COLOR_COLORLESS, -1);
				if( spell_fizzled != 1 ){
					plus+=x_value;
				}
			}
			if( spell_fizzled == 1 ){
				int i;
				for(i=0; i<tc; i++){
					remove_state(player, tapped[i], STATE_TAPPED);
				}
			}
		}
		else{
			int rval = 0;
			int tapped_for_color[active_cards_count[player]][5];
			int i,k;
			for(i=0; i<active_cards_count[player]; i++){
				for(k=0; k<5; k++){
					tapped_for_color[i][k] = 0;
				}
			}
			int attempt = 0;
			while( attempt < 50 ){
					int clrs[6] = {up_cless, black, blue, green, red, white};
					for(i=0; i<active_cards_count[player]; i++){
						if( in_play(player, i) && ! is_tapped(player, i) && is_what(player, i, TYPE_CREATURE) ){
							int clr = get_color(player, i);
							int col_found = 0;
							for(k=1; k<6; k++){
								if( (clr & (1<<k)) && clrs[k] > 0 && tapped_for_color[i][k-1] != 1){
									clrs[k]--;
									col_found = 1;
									tapped_for_color[i][k-1] = 1;
									add_state(player, i, STATE_TAPPED);
									tapped[tc] = i;
									tc++;
									creatures_tapped_for_convoke++;
									break;
								}
							}
							if( ! col_found ){
								if( clrs[0] ){
									add_state(player, i, STATE_TAPPED);
									tapped[tc] = i;
									tc++;
									clrs[0]--;
									creatures_tapped_for_convoke++;
								}
								else if( x_flag ){
										add_state(player, i, STATE_TAPPED);
										tapped[tc] = i;
										tc++;
										plus++;
										creatures_tapped_for_convoke++;
								}
							}
						}
						if( clrs[0]+clrs[1]+clrs[2]+clrs[3]+clrs[4]+clrs[5] == 0 && ! x_flag ){
							break;
						}
					}
					rval = has_mana_multi(player, clrs[0], clrs[1], clrs[2], clrs[3], clrs[4], clrs[5]);
					if( rval ){
						charge_mana_multi(player, clrs[0], clrs[1], clrs[2], clrs[3], clrs[4], clrs[5]);
						if( x_flag && has_mana(player, COLOR_COLORLESS, 1) ){
							charge_mana(player, COLOR_COLORLESS, -1);
							if( spell_fizzled != 1 ){
								plus+=x_value;
							}
						}
						if( spell_fizzled == 1 ){
							rval = 0;
						}
						else{
							break;
						}
					}
					plus = 0;
					for(i=0; i<tc; i++){
						remove_state(player, tapped[i], STATE_TAPPED);
					}
					creatures_tapped_for_convoke = 0;
					tc = 0;
					attempt++;
			}
			if( ! rval ){
				spell_fizzled = 1;
			}
		}
		int result = spell_fizzled != 1 ? (plus ? plus : 1) : 0;
		if( result ){
			result |= (creatures_tapped_for_convoke ? 1<<31 : 0);
		}
		return result;
	}
	if( mode == 1 ){
		if( check_for_cc_modifier ){
			charge_mana_from_id(player, -1, event, get_id(player, card));
		}
		else{
			charge_mana_multi(player, cless, black, blue, green, red, white);
		}
		if( spell_fizzled != 1 ){
			if (cless == -1){
				return x_value;	// And no way to tell the difference between 0 and cancel from the return value - but you should just check for spell_fizzled == 1 anyway
			} else {
				return 1;
			}
		}
	}
	if( mode == 2 ){
		spell_fizzled = 1;
	}
	return 0;
}

int charge_convoked_mana_extended(int player, int card, event_t event, int cless, int black, int blue, int green, int red, int white){
	return charge_convoked_mana_extended_generic(player, card, event, cless, black, blue, green, red, white, 1);
}

int charge_convoked_mana(int player, int card, int cless, int green, int white ){

	return charge_convoked_mana_extended(player, card, EVENT_CAST_SPELL, cless, 0, 0, green, 0, white);
}

int ravnica_manachanger(int player, int card, event_t event, int plus_black, int plus_blue, int plus_green, int plus_red, int plus_white){

	if( event == EVENT_MODIFY_COST && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		card_ptr_t* c = cards_ptr[ get_id(player, card) ];
		int cless = get_updated_casting_cost(player, card, -1, event, -1);
		if( has_mana_multi(player, cless-1, c->req_black+plus_black, c->req_blue+plus_blue,
							c->req_green+plus_green, c->req_red+plus_red, c->req_white+plus_white)
		  ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}
	return 0;
}

int charge_changed_mana(int player, int card, int plus_black, int plus_blue, int plus_green, int plus_red, int plus_white){
	int special_color = COLOR_BLACK;
	if( plus_blue ){
		special_color = COLOR_BLUE;
	}
	if( plus_green ){
		special_color = COLOR_GREEN;
	}
	if( plus_red ){
		special_color = COLOR_RED;
	}
	if( plus_white ){
		special_color = COLOR_WHITE;
	}
	card_ptr_t* c = cards_ptr[ get_id(player, card) ];
	int cless = get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, c->req_colorless);
	if( cless < 1 ){
		cless = 0;
	}
	int cless2 = cless-1;
	if( cless2 < 1 ){
		cless2 = 0;
	}
	int choice = 0;
	if( has_mana_multi(player, cless-1, c->req_black+plus_black, c->req_blue+plus_blue, c->req_green+plus_green, c->req_red+plus_red, c->req_white+plus_white)){
		if( player == AI ){
			choice = 1;
		}
		else{
			int manacost[5] = { c->req_black, c->req_blue, c->req_green, c->req_red, c->req_white };
			int manacost_mod[5] = { c->req_black+plus_black,  c->req_blue+plus_blue, c->req_green+plus_green, c->req_red+plus_red, c->req_white+plus_white };
			char buffer[500];
			int pos = scnprintf(buffer, 500, " Pay ");
			if( cless == -1 ){
				pos += scnprintf(buffer + pos, 500-pos, "X" );
			}
			else if( cless > 0 ){
					pos += scnprintf(buffer + pos, 500-pos, "%d", cless );
			}

			int q;
			for(q=0; q<5; q++){
				if( manacost[q] > 0 ){
					int w;
					for(w=0;w<manacost[q]; w++){
						if( q == 0 ){
							pos += scnprintf(buffer + pos, 500-pos, "B" );
						}
						if( q == 1 ){
							pos += scnprintf(buffer + pos, 500-pos, "U" );
						}
						if( q == 2 ){
							pos += scnprintf(buffer + pos, 500-pos, "G" );
						}
						if( q == 3 ){
							pos += scnprintf(buffer + pos, 500-pos, "R" );
						}
						if( q == 4 ){
							pos += scnprintf(buffer + pos, 500-pos, "W" );
						}
					}
				}
			}
			pos += scnprintf(buffer + pos, 500-pos, "\n Pay ");
			if( cless2 == -1 ){
				pos += scnprintf(buffer + pos, 500-pos, "X" );
			}
			else if( cless2 > 0 ){
					pos += scnprintf(buffer + pos, 500-pos, "%d", cless2 );
			}

			q=0;
			for(q=0; q<5; q++){
				if( manacost_mod[q] > 0 ){
					int w;
					for(w=0;w<manacost_mod[q]; w++){
						if( q == 0 ){
							pos += scnprintf(buffer + pos, 500-pos, "B" );
						}
						if( q == 1 ){
							pos += scnprintf(buffer + pos, 500-pos, "U" );
						}
						if( q == 2 ){
							pos += scnprintf(buffer + pos, 500-pos, "G" );
						}
						if( q == 3 ){
							pos += scnprintf(buffer + pos, 500-pos, "R" );
						}
						if( q == 4 ){
							pos += scnprintf(buffer + pos, 500-pos, "W" );
						}
					}
				}
			}
			pos += scnprintf(buffer + pos, 500-pos, "\n Cancel");
			choice = do_dialog(player, player, card, -1, -1, buffer , 0);
		}
	}
	if( choice == 0 ){
		charge_mana_multi(player, cless, c->req_black, c->req_blue, c->req_green, c->req_red, c->req_white);
		if( spell_fizzled != 1 ){
			int result = 1 + (mana_paid[special_color] > 0);
			return result;
		}
		else{
			return 0;
		}
	}
	else if( choice == 1 ){
			charge_mana_multi(player, cless2, c->req_black+plus_black, c->req_blue+plus_blue, c->req_green+plus_green, c->req_red+plus_red, c->req_white+plus_white);
			if( spell_fizzled != 1 ){
				return 2;
			}
			else{
				return 0;
			}
	}
	else{
		spell_fizzled = 1;
		return 0;
	}
	return 0;
}

static void radiant_pump(int player, int card, int t_player, int t_card, int power, int toughness, int ability, int sp_ability, int effect){
	int clr = get_color(t_player, t_card);
	if( clr != 0 ){
		APNAP(p,{
					int c;
					for(c = active_cards_count[p]-1; c > -1; c--){
						if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
							if( has_my_colors(t_player, t_card, p, c) ){
								pump_ability_until_eot(player, card, p, c, power, toughness, ability, sp_ability );
								if( effect == ACT_UNTAP ){
									untap_card(p, c);
								}
							}
						}
					}
				}
		);
	}
	else pump_ability_until_eot(player, card, t_player, t_card, power, toughness, ability, sp_ability );
}

static void radiant_damage(int player, int card, int t_player, int t_card, int amount_damage, int additional_flags){
	int clr = get_color(t_player, t_card);
	if( clr != 0 ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.color = clr;
		new_damage_all(player, card, ANYBODY, amount_damage, additional_flags, &test);
	}
	else damage_creature(t_player, t_card, amount_damage, player, card);
}

static void radiant_action(int player, int card, int t_player, int t_card, int action){
	int clr = get_color(t_player, t_card);
	if( clr != 0 ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ENCHANTMENT);
		test.color = clr;
		APNAP(p, { new_manipulate_all(player, card, p, &test, KILL_DESTROY); };);
	}
	else kill_card(t_player, t_card, KILL_DESTROY);
}

int transmute(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white, int my_cmc){
	if( event == EVENT_CAN_ACTIVATE_FROM_HAND && has_mana_multi(player, colorless, black, blue, green, red, white) ){
		if( can_sorcery_be_played(player, event) ){
			return 1;
		}
	}
	if( event == EVENT_ACTIVATE_FROM_HAND ){
		charge_mana_multi(player, colorless, black, blue, green, red, white);
		if( spell_fizzled != 1 ){
			discard_card(player, card);
			return 1;
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		test_definition_t test;
		new_default_test_definition(&test, 0, "");
		test.cmc = my_cmc;
		test.cmc_flag = MATCH;
		scnprintf(test.message, 100, "Select a card with CMC %d.", my_cmc);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
	}
	return 0;
}

int card_generic_shockland(int player, int card, event_t event){

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  card_instance_t* land = get_card_instance(player, card);

	  int played_normally = !played_for_free(player, card) && !(land->state & STATE_IN_PLAY);	// The latter will be true if real_put_into_play() was called

	  int cpl = can_pay_life(player, 2);

	  int ai_pay_life_priority = -1;

	  if (player == AI
		  && cpl
		  && life[AI] >= 8	// This is very blunt.  Ideally we'd want the AI to speculate on both paths and see whether a newly-castable spell is worth paying 2 life.
		  && (land->state & (STATE_INVISIBLE | STATE_OUBLIETTED | STATE_IN_PLAY | STATE_TAPPED)) == STATE_INVISIBLE	// not already tapped by something else, and being put into play normally (some cards, like Rampant Growth, call put_into_play() before setting STATE_TAPPED)
		  && !check_battlefield_for_id(player, CARD_ID_AMULET_OF_VIGOR))	// it'll untap itself anyway
		{
		  // see if this land will help the AI cast a spell
		  land->state &= ~STATE_INVISIBLE;	// temporarily off stack
		  land->state |= STATE_IN_PLAY;		// temporarily in play
		  count_mana();
		  int c;
		  for (c = 0; c < active_cards_count[player]; ++c)
			if (in_hand(player, c))
			  {
				int iid = get_card_instance(player, c)->internal_card_id;
				if (iid > -1
					&& has_mana_to_cast_iid(player, event, iid))	// card is castable with land untapped
				  {
					// tap the land and see if card becomes uncastable
					land->state |= STATE_TAPPED;
					count_mana();
					int can_cast = has_mana_to_cast_iid(player, event, iid);
					land->state &= ~STATE_TAPPED;
					count_mana();
					if (!can_cast)
					  {
						ai_pay_life_priority = 10;
						break;
					  }
				  }
			  }
		  land->state &= ~STATE_IN_PLAY;	// back out of play
		  land->state |= STATE_INVISIBLE;	// back onto the stack
		  count_mana();
		}

	  int choice = DIALOG(player, card, event,
						  DLG_NO_STORAGE, DLG_NO_CANCEL, /* Added manually, so we can grey it out if not played normally */
						  "Enter the battlefield tapped", 1, 1,
						  "Pay 2 life", cpl, ai_pay_life_priority,
						  "Cancel", played_normally, -1);

	  if (choice == 1)
		land->state |= STATE_TAPPED;	// avoid sending event
	  else if (choice == 2)
		lose_life(player, 2);
	  else
		cancel = 1;	// must set manually due to DLG_NO_STORAGE flag, and because it's not autogenerated due to DLG_NO_CANCEL flag
	}

  return mana_producer(player, card, event);
}

static int signet2(int player, int card, event_t event, int c1, int c2){

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_produce_mana(player, card) && ! is_tapped(player, card) && ! is_animated_and_sick(player, card) &&
		instance->targets[1].player != 66 && has_mana(player, COLOR_COLORLESS, 2)
	  ){
		if( can_produce_mana(player, card) ){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->targets[1].player = 66;
		charge_mana(player, COLOR_COLORLESS, 1);
		instance->targets[1].player = 0;
		if( spell_fizzled != 1 ){
			produce_mana_tapped2(player, card, c1, 1, c2, 1);
		}
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( ! is_tapped(player, card) && !IS_ACTIVATING(event) && instance->targets[1].player != 66 && !is_animated_and_sick(player, card) ){
		  /* Best we can do - this says that it can produce one mana of either color1 or color2.  There may be a way to manipulate the has_mana() backend data
		   * to account for color-fixing, but it's not understood well enough yet.
		   *
		   * Investigating card_celestial_prism(), and in particular dword_738B48, dword_7A2FE4, sub_498F20(), and sub_499050(), might help, but I doubt it;
		   * celestial prism (and the former asm version of signet) doesn't handle EVENT_COUNT_MANA at all. */

		  declare_mana_available_hex(player, (1<<c1) | (1<<c2), 1);
		}
	}

	return 0;
}

// cards
int card_agrus_kos_wojek_veteran(int player, int card, event_t event)
{
/* CARD_ID_AGRUS_KOS_WOJEK_VETERAN	7765
Agrus Kos, Wojek Veteran	|3|R|W
Legendary Creature - Human Soldier 3/3
Whenever ~ attacks, attacking |Sred creatures get +2/+0 and attacking |Swhite creatures get +0/+2 until end of turn. */

  check_legend_rule(player, card, event);

  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.state = STATE_ATTACKING;
	  test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
	  pump_creatures_until_eot(player, card, current_turn, 0, 2,0, 0,0, &test);

	  test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
	  pump_creatures_until_eot(player, card, current_turn, 0, 0,2, 0,0, &test);
	}

  return 0;
}

int card_auratouched_mage(int player, int card, event_t event){

	/* Auratouched Mage	|5|W
	 * Creature - Human Wizard 3/3
	 * When ~ enters the battlefield, search your library for an Aura card that could enchant it. If ~ is still on the battlefield, put that Aura card onto the
	 * battlefield attached to it. Otherwise, reveal the Aura card and put it into your hand. Then shuffle your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ENCHANTMENT, "Select an Aura card with enchant creature.");
		this_test.subtype = SUBTYPE_AURA_CREATURE;

		int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_CMC, &this_test);
		if( result > -1 && in_play(player, card) ){
			put_into_play_aura_attached_to_target(player, result, player, card);
		}
	}

	return 0;
}

int card_autochthon_wurm(int player, int card, event_t event){
/* CARD_ID_AUTOCHTHON_WURM	7767
Autochthon Wurm	|10|G|G|G|W|W
Creature - Wurm 9/14
Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
Trample */

	return generic_spell_with_convoke(player, card, event);
}

int card_azorius_signet(int player, int card, event_t event){
/* CARD_ID_AZORIUS_SIGNET	1722
Azorius Signet	|2
Artifact
|1, |T: Add |W|U to your mana pool. */

	// First printed in Dissension, but here to keep local to the other signets.
	return signet2(player, card, event, COLOR_BLUE, COLOR_WHITE);
}

int card_bathe_in_light(int player, int card, event_t event){
/* CARD_ID_BATHE_IN_LIGHT	7769
Bathe in Light	|1|W
Instant
Radiance - Choose a color. Target creature and each other creature that shares a color with it gain protection from the chosen color until end of turn.*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int kw = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
			radiant_pump(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, kw, 0, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_blazing_archon(int player, int card, event_t event){
/* CARD_ID_BLAZING_ARCHON	7772
Blazing Archon	|6|W|W|W
Creature - Archon 5/6
Flying
Creatures can't attack you. */

	if( current_turn != player && in_play(player, card) && ! is_humiliated(player, card) ){
		nobody_can_attack(player, card, event, 1-player);
	}

	return 0;
}

int card_bloodbond_march(int player, int card, event_t event){
/*
Bloodbond March |2|B|G
Enchantment
Whenever a player casts a creature spell, each player returns all cards with the same name as that spell from his or her graveyard to
	the battlefield.
*/
	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &test) ){
			int csvid = get_card_name(trigger_cause_controller, trigger_cause);
			int i;
			for(i=0; i<2; i++){
				const int *grave = get_grave(i);
				int count = count_graveyard(i)-1;
				while( count > -1 ){
						if( cards_data[grave[count]].id == csvid ){
							reanimate_permanent(i, -1, i, count, REANIMATE_DEFAULT);
						}
						count--;
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_bloodletter_quill(int player, int card, event_t event)
{
  /* Bloodletter Quill	|3
   * Artifact
   * |2, |T, Put a blood counter on ~: Draw a card, then lose 1 life for each blood counter on ~.
   * |U|B: Remove a blood counter from ~. */

  if (IS_ACTIVATING(event))
	{
	  if (event == EVENT_CAN_ACTIVATE && !can_use_activated_abilities(player, card))
		return 0;

	  enum
	  {
		CHOICE_DRAW = 1,
		CHOICE_REMOVE
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM,
						"Draw a card", CAN_TAP(player, card), 1, DLG_MANA(MANACOST_X(2)),
						"Remove a blood counter", 1, count_counters(player, card, COUNTER_BLOOD), DLG_MANA(MANACOST_UB(1,1)));

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (event == EVENT_ACTIVATE)
		switch (choice)
		  {
			case CHOICE_DRAW:
			  add_counter(player, card, COUNTER_BLOOD);
			  tap_card(player, card);
			  if (player == AI)
				ai_modifier += 48;	// so it'll even consider activating
			  break;
			case CHOICE_REMOVE:
			  break;
		  }
	  else	// EVENT_RESOLVE_ACTIVATION
		switch (choice)
		  {
			case CHOICE_DRAW:
			  draw_a_card(player);
			  lose_life(player, count_counters(player, card, COUNTER_BLOOD));
			  break;
			case CHOICE_REMOVE:
			  remove_counter(player, card, COUNTER_BLOOD);
			  break;
		  }
	}

  if (event == EVENT_SHOULD_AI_PLAY)
	lose_life(player, count_counters(player, card, COUNTER_BLOOD));

  return 0;
}

int card_boros_fury_shield(int player, int card, event_t event){
/* CARD_ID_BOROS_FURY_SHIELD	7777
Boros Fury-Shield	|2|W
Instant
Prevent all combat damage that would be dealt by target attacking or blocking creature this turn. If |R was spent to cast ~,
it deals damage to that creature's controller equal to the creature's power. */

	if( event == EVENT_MODIFY_COST ){
		return ravnica_manachanger(player, card, event, 0, 0, 0, 1, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->targets[1].player = 1;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1){
			instance->targets[1].player = charge_changed_mana(player, card, 0, 0, 0, 1, 0);
		}
		if( spell_fizzled != 1 ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			negate_combat_damage_this_turn(player, card, instance->targets[0].player, instance->targets[0].card, 0);
			if( instance->targets[1].player  == 2 ){
				damage_player(instance->targets[0].player, get_power(instance->targets[0].player, instance->targets[0].card), player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_boros_guildmage(int player, int card, event_t event){
/*
Boros Guildmage {R/W}{R/W}
Creature — Human Wizard 2/2
{1}{R}: Target creature gains haste until end of turn.
{1}{W}: Target creature gains first strike until end of turn.
*/

	if( ! IS_GAA_EVENT(event) ){
		return hybrid(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_FIRST_STRIKE = 1,
		CHOICE_HASTE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			if( has_mana_for_activated_ability(player, card, MANACOST_XW(1, 1)) ||
				has_mana_for_activated_ability(player, card, MANACOST_XR(1, 1)) )
			{
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						has_mana_for_activated_ability(player, card, MANACOST_XW(1, 1)),
						has_mana_for_activated_ability(player, card, MANACOST_XR(1, 1))
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Give First Strike", abils[1], current_phase == PHASE_AFTER_BLOCKING ? 10 : 1,
							"Give Haste", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_FIRST_STRIKE ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XW(1, 1), 0, &td, "TARGET_CREATURE");
		}
		if( choice == CHOICE_HASTE ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, "TARGET_CREATURE");
			if( spell_fizzled != 1 ){
				ai_modifier_for_haste_granting_effects(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			if( instance->info_slot == CHOICE_FIRST_STRIKE ){
				pump_ability_until_eot(instance->parent_controller, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FIRST_STRIKE, 0);
			}
			if( instance->info_slot == CHOICE_HASTE ){
				pump_ability_until_eot(instance->parent_controller, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_HASTE);
			}
		}
	}
	return 0;
}

int card_boros_recruit(int player, int card, event_t event){
/* CARD_ID_BOROS_RECRUIT	7780
Boros Recruit	|RW
Creature - Goblin Soldier 1/1
(|RW can be paid with either |R or |W.)
First strike */

	hybrid(player, card, event);

	return 0;
}

int card_boros_signet(int player, int card, event_t event){
/* CARD_ID_BOROS_SIGNET	1718
Boros Signet	|2
Artifact
|1, |T: Add |R|W to your mana pool. */

	return signet2(player, card, event, COLOR_WHITE, COLOR_RED);
}

// boros swiftblade --> vanilla

int card_brainspoil(int player, int card, event_t event){
/* CARD_ID_BRAINSPOIL	7783
Brainspoil	|3|B|B
Sorcery
Destroy target creature that isn't enchanted. It can't be regenerated.
Transmute |1|B|B (|1|B|B, Discard this card: Search your library for a card with the same converted mana cost as this card,
reveal it, and put it into your hand. Then shuffle your library. Transmute only as a sorcery.) */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return transmute(player, card, event, MANACOST_XB(1, 2), 5);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_state = TARGET_STATE_ENCHANTED;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target non-enchanted creature.", 1, NULL);
}

int card_bramble_elemental(int player, int card, event_t event){

	/* Bramble Elemental	|3|G|G
	 * Creature - Elemental 4/4
	 * Whenever an Aura becomes attached to ~ or enters the battlefield attached to ~, put two 1/1 |Sgreen Saproling creature tokens onto the battlefield. */

	if( aura_attached_to_me(player, card, event, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, 2);
	}

	return 0;
}

int brightflame_legacy(int player, int card, event_t event){

	if( event == EVENT_DEAL_DAMAGE ){
		card_instance_t *instance = get_card_instance( player, card );
		card_instance_t *dmg = get_card_instance( affected_card_controller, affected_card );
		if( dmg->internal_card_id == damage_card && dmg->info_slot > 0 ){
			if( dmg->damage_source_player == instance->targets[0].player && dmg->damage_source_card == instance->targets[0].card ){
				instance->targets[1].player = instance->targets[1].player < 0 ? 0 : instance->targets[1].player;
				instance->targets[1].player+=dmg->info_slot;
			}
		}
	}

	if( get_card_instance( player, card )->targets[1].player > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
		reason_for_trigger_controller == player )
	{
		card_instance_t *instance = get_card_instance( player, card );
		gain_life(player, instance->targets[1].player);
		instance->targets[1].player = 0;
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_brightflame(int player, int card, event_t event){
/* CARD_ID_BRIGHTFLAME	7786
Brightflame	|X|R|R|W|W
Sorcery
Radiance - ~ deals X damage to target creature and each other creature that shares a color with it. You gain life equal to the damage dealt this way.*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance( player, card );
			int legacy = create_legacy_effect(player, card, &brightflame_legacy);
			get_card_instance(player, legacy)->targets[0].player = player;
			get_card_instance(player, legacy)->targets[0].card = card;

			radiant_damage(player, card, instance->targets[0].player, instance->targets[0].card, instance->info_slot, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_carven_caryatid(int player, int card, event_t event){
/* CARD_ID_CARVEN_CARYATID	1251
Carven Caryatid	|1|G|G
Creature - Spirit 2/5
Defender (This creature can't attack.)
When ~ enters the battlefield, draw a card. */

	// original code : 0040E990

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_centaur_safeguard(int player, int card, event_t event){
/* CARD_ID_CENTAUR_SAFEGUARD	7789
Centaur Safeguard	|2|GW
Creature - Centaur Warrior 3/1
(|GW can be paid with either |G or |W.)
When ~ dies, you may gain 3 life. */

	hybrid(player, card, event);
	if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_DUH)){
		gain_life(player, 3);
	}
	return 0;
}

static int covg_legacy(int player, int card, event_t event){
	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->info_slot > 0 ){
			if( is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE) ){
				gain_life(player, damage->info_slot);
				damage->info_slot = 0;
			}
		}
	}

	if( eot_trigger(player, card, event) ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_chant_of_vitu_ghazi(int player, int card, event_t event){
/*
Chant of Vitu-Ghazi |6|W|W
Instant
Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for {1} or one mana of that creature's color.)
Prevent all damage that would be dealt by creatures this turn. You gain life equal to the damage prevented this way.
*/
	if( event == EVENT_MODIFY_COST ){
		return generic_spell_with_convoke(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( current_turn != player || life[player] < 6 ){
			ai_modifier += (count_attackers(current_turn) * 12);
		}
		return generic_spell_with_convoke(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &covg_legacy);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}


int card_chord_of_calling(int player, int card, event_t event){
/* CARD_ID_CHORD_OF_CALLING	7792
Chord of Calling	|X|G|G|G
Instant
Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
Search your library for a creature card with converted mana cost X or less and put it onto the battlefield. Then shuffle your library. */

	if( event == EVENT_MODIFY_COST){
		card_ptr_t* c = cards_ptr[ get_id(player, card)  ];
		if( has_convoked_mana_extended(player, card, event, c->req_colorless, c->req_black, c->req_blue, c->req_green, c->req_red, c->req_white) == 2 ){
			null_casting_cost(player, card);
			get_card_instance( player, card )->targets[1].player = 1;
		}
		else{
			get_card_instance( player, card )->targets[1].player = 0;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			return 0;
		}
		if( played_for_free(player, card) || is_token(player, card) ){
			instance->info_slot = 0;
			return 0;
		}
		if( instance->targets[1].player == 1 ){
			if( charge_convoked_mana_extended(player, card, event, MANACOST_G(3)) ){
				int result = charge_convoked_mana_extended_generic(player, card, event, MANACOST_X(-1), 0);
				if( result & (1<<31) ){
					result &= ~(1<<31);
				}
				instance->info_slot = result;
				x_value = instance->info_slot;
			}
		}
		else{
			instance->info_slot = x_value;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		x_value = 0;
		char buffer[100];
		scnprintf(buffer, 100, "Select a creature card with CMC %d or less", instance->info_slot);
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, buffer);
		test.cmc = instance->info_slot+1;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_chorus_of_the_conclave(int player, int card, event_t event){
/*
Chorus of the Conclave |4|W|W|G|G
Legendary Creature — Dryad 3/8
Forestwalk (This creature can't be blocked as long as defending player controls a Forest.)
As an additional cost to cast creature spells, you may pay any amount of mana. If you do, that creature enters the battlefield with
	that many additional +1/+1 counters on it.
*/
	check_legend_rule(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_CAST_SPELL && ! affect_me(player, card) && affected_card_controller == player &&
		    is_what(affected_card_controller, affected_card, TYPE_CREATURE)){

			if( has_mana(player, COLOR_ANY, 1) ){
				int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
									"Activate Chorus of the Conclave", 1, 5,
									"Decline", 1, 1);
				if( choice == 1 ){
					charge_mana(player, COLOR_COLORLESS, -1);
					if( spell_fizzled != 1 ){
						hack_silent_counters = 1;
						add_1_1_counters(affected_card_controller, affected_card, x_value);
						hack_silent_counters = 0;
					}
					spell_fizzled = 0;
				}
			}
		}
	}

	return 0;
}

int card_circu_dimir_lobotomist(int player, int card, event_t event){
/*
Circu, Dimir Lobotomist |2|U|B
Legendary Creature — Human Wizard 2/3)
Whenever you cast a blue spell, exile the top card of target library.
Whenever you cast a black spell, exile the top card of target library.
Your opponents can't cast nonland cards with the same name as a card exiled with Circu, Dimir Lobotomist.
*/
	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK | COLOR_TEST_BLUE);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.zone = TARGET_ZONE_PLAYERS;
			td.illegal_abilities = 0;
			td.allow_cancel = 0;

			card_instance_t *instance = get_card_instance( player, card );

			if( new_pick_target(&td, "Select a library owner.", 0, GS_LITERAL_PROMPT) ){
				int iid = deck_ptr[instance->targets[0].player][0];
				if( iid > -1 ){
					int i;
					for(i=1; i<10; i++){
						if( instance->targets[i].player == -1 ){
							instance->targets[i].player = iid;
							break;
						}
						if( instance->targets[i].card == -1 ){
							instance->targets[i].card = iid;
							break;
						}
					}
					create_card_name_legacy(player, card, cards_data[iid].id);
				}
			}
		}
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == 1-player && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		int i;
		for(i=1; i<10; i++){
			int iid = instance->targets[i].player;
			if( iid == -1 ){
				break;
			}
			if( get_id(affected_card_controller, affected_card) == cards_data[iid].id ){
				infinite_casting_cost();
				break;
			}
			iid = instance->targets[i].card;
			if( iid == -1 ){
				break;
			}
			if( get_id(affected_card_controller, affected_card) == cards_data[iid].id ){
				infinite_casting_cost();
				break;
			}
		}
	}


	return 0;
}

int card_cleansing_beam(int player, int card, event_t event){
/* CARD_ID_CLEANSING_BEAM	7795
Cleansing Beam	|4|R
Instant
Radiance - ~ deals 2 damage to target creature and each other creature that shares a color with it. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			radiant_damage(player, card, instance->targets[0].player, instance->targets[0].card, 2, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_cloudstone_curio(int player, int card, event_t event){
/*
Cloudstone Curio |3
Artifact
Whenever a nonartifact permanent enters the battlefield under your control, you may return another permanent you control that
	shares a permanent type with it to its owner's hand.
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		test.type_flag = DOESNT_MATCH;

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *curio = get_card_instance(player, card);

		int orig_trigger_cause_controller = trigger_cause_controller;
		int orig_trigger_cause = trigger_cause;
		state_untargettable(orig_trigger_cause_controller, orig_trigger_cause, 1);

		target_definition_t td;
		default_target_definition(player, card, &td, get_type(trigger_cause_controller, trigger_cause));
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;

		if( can_target(&td) && new_pick_target(&td, "CURIO", 0, 0) ){
			bounce_permanent(curio->targets[0].player, curio->targets[0].card);
		}
		state_untargettable(orig_trigger_cause_controller, orig_trigger_cause, 0);
		curio->number_of_targets = 0;
	}

	return 0;
}

int card_clutch_of_the_undercity(int player, int card, event_t event){
/* CARD_ID_CLUTCH_OF_THE_UNDERCITY	7798
Clutch of the Undercity	|1|U|U|B
Instant
Return target permanent to its owner's hand. Its controller loses 3 life.
Transmute |1|U|B (|1|U|B, Discard this card: Search your library for a card with the same converted mana cost as this card, reveal it, and put it into your hand.
Then shuffle your library. Transmute only as a sorcery.) */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return transmute(player, card, event, MANACOST_XUB(1, 1, 1), 4);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			lose_life(instance->targets[0].player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
}

int card_compulsive_research(int player, int card, event_t event){
/* CARD_ID_COMPULSIVE_RESEARCH	1549
Compulsive Research	|2|U
Sorcery
Target player draws three cards. Then that player discards two cards unless he or she discards a land card. */

	/* Original Code: 004E81DF */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 48;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance( player, card);
			draw_cards(instance->targets[0].player, 3);
			int discarded = 0;
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card to discard");
			test.zone = TARGET_ZONE_HAND;

			test_definition_t test2;
			default_test_definition(&test2, TYPE_LAND);
			test2.zone = TARGET_ZONE_HAND;
			while( hand_count[instance->targets[0].player] && discarded < 2 ){
					int selected = -1;
					if( player == HUMAN ){
						selected = new_select_a_card(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &test);
					}
					else{
						selected = new_select_a_card(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &test2);
						if( selected == -1 ){
							selected = new_select_a_card(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &test);
						}
					}
					int is_land = is_what(player, selected, TYPE_LAND);
					discard_card(player, selected);
					if( is_land ){
						break;
					}
					discarded++;
			}
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_concerted_effort(int player, int card, event_t event){
/* CARD_ID_CONCERTED_EFFORT	7800
Concerted Effort	|2|W|W
Enchantment
At the beginning of each upkeep, creatures you control gain flying until end of turn if a creature you control has flying.
The same is true for fear, first strike, double strike, landwalk, protection, trample, and vigilance. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int count = 0;
		int key = 0;
		int s_key  = 0;
		while( count < active_cards_count[player] ){
				if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) ){
					// Leaves out weird protections and landwalks.  Oh well.
					key |= (get_abilities(player, count, EVENT_ABILITIES, -1)
							& (KEYWORD_FLYING | KEYWORD_FIRST_STRIKE | KEYWORD_DOUBLE_STRIKE | KEYWORD_BASIC_LANDWALK
							   | KEYWORD_PROT_COLORED | KEYWORD_PROT_ARTIFACTS | KEYWORD_TRAMPLE));
					if( !(s_key & SP_KEYWORD_VIGILANCE) && check_for_special_ability(player, count, SP_KEYWORD_VIGILANCE) ){
						s_key |= SP_KEYWORD_VIGILANCE;
					}
					if( !(s_key & SP_KEYWORD_FEAR) && check_for_special_ability(player, count, SP_KEYWORD_FEAR) ){
						s_key |= SP_KEYWORD_FEAR;
					}
				}
				count++;
		}
		if (key || s_key){
			pump_subtype_until_eot(player, card, player, -1, 0, 0, key | KEYWORD_RECALC_SET_COLOR, s_key);
		}
	}

	return global_enchantment(player, card, event);
}

int card_conclave_phalanx(int player, int card, event_t event){
/* CARD_ID_CONCLAVE_PHALANX	7802
Conclave Phalanx	|4|W
Creature - Human Soldier 2/4
Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
When ~ enters the battlefield, you gain 1 life for each creature you control. */

	if( event == EVENT_MODIFY_COST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return generic_spell_with_convoke(player, card, event);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, count_subtype(player, TYPE_CREATURE, -1));
	}

	return 0;
}

int card_congregation_at_dawn(int player, int card, event_t event){
/*
Congregation at Dawn |W|G|G	0x200f548
Instant
Search your library for up to three creature cards and reveal them. Shuffle your library, then put those cards on top of it in any order.
*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == 1-player ? 48 : 12);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
		test.qty = 3;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_DECK, 0, AI_MAX_VALUE, &test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_consult_the_necrosages(int player, int card, event_t event){
/* CARD_ID_CONSULT_THE_NECROSAGES	7805
Consult the Necrosages	|1|U|B
Sorcery
Choose one -
* Target player draws two cards.
* Target player discards two cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_DRAW = 1,
		CHOICE_DISCARD
	};

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Target player draws 2", 1, 5,
								"Target player discard 2", 1, hand_count[1-player] > 0 ? 5 : 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			if( instance->info_slot == CHOICE_DRAW ){
				ai_modifier_for_targeted_card_drawing(player, card, 0, 2);
			}
			if( instance->info_slot == CHOICE_DISCARD ){
				ai_modifier += (instance->targets[0].player == 1-player ? 48 : -48);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			if( instance->info_slot == CHOICE_DRAW ){
				draw_cards(instance->targets[0].player, 2);
			}
			if( instance->info_slot == CHOICE_DISCARD ){
				new_multidiscard(instance->targets[0].player, 2, 0, player);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_convolute(int player, int card, event_t event){
	/* Convolute	|2|U
	 * Instant
	 * Counter target spell unless its controller pays |4. */

	if( event == EVENT_RESOLVE_SPELL){
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, 4);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

int card_copy_enchantment(int player, int card, event_t event)
{
  /* Copy Enchantment	|2|U
   * Enchantment
   * You may have ~ enter the battlefield as a copy of any enchantment on the battlefield. */

  target_definition_t td;
  if (event == EVENT_RESOLVE_SPELL)
	base_target_definition(player, card, &td, TYPE_ENCHANTMENT);

  enters_the_battlefield_as_copy_of_any(player, card, event, &td, "SELECT_AN_ENCHANTMENT");

  return global_enchantment(player, card, event);
}

int card_crown_of_convergence(int player, int card, event_t event ){
/* CARD_ID_CROWN_OF_CONVERGENCE	7809
Crown of Convergence	|2
Artifact
Play with the top card of your library revealed.
As long as the top card of your library is a creature card, creatures you control that share a color with that card get +1/+1.
|G|W: Put the top card of your library on the bottom of your library. */

	reveal_top_card(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		put_top_card_of_deck_to_bottom(player);
	}

	if( affected_card_controller == player && (event == EVENT_POWER || event == EVENT_TOUGHNESS) && ! is_humiliated(player, card) ){
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			if( is_what(-1, deck[0], TYPE_CREATURE) && has_my_colors(-1, deck[0], affected_card_controller, affected_card) ){
				event_result++;
			}
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_WG(1, 1), 0, NULL, NULL);
}

int card_darkblast(int player, int card, event_t event){
/* CARD_ID_DARKBLAST	7812
Darkblast	|B
Instant
Target creature gets -1/-1 until end of turn.
Dredge 3 (If you would draw a card, instead you may put exactly three cards from the top of your library into your graveyard.
If you do, return this card from your graveyard to your hand. Otherwise, draw a card.) */

	if( event == EVENT_DREDGE ){
		return 3;
	}

	return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -1, -1, 0, 0);
}

void dark_confidant_effect(int player, int card, int t_player){
	int *deck = deck_ptr[t_player];
	if( deck[0] != -1 ){
		int card_added = add_card_to_hand(t_player, deck[0] );
		remove_card_from_deck( t_player, 0 );
		int cmc = get_cmc(t_player, card_added);
		lose_life(t_player, cmc);
		reveal_card(player, card, t_player, card_added);
	}
}

int card_dark_confidant(int player, int card, event_t event){
/* CARD_ID_DARK_CONFIDANT	7811
Dark Confidant	|1|B
Creature - Human Wizard 2/1
At the beginning of your upkeep, reveal the top card of your library and put that card into your hand. You lose life equal to its converted mana cost. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		dark_confidant_effect(player, card, player);
	}

	return 0;
}

int card_devouring_light(int player, int card, event_t event){
/* CARD_ID_DEVOURING_LIGHT	7813
Devouring Light	|1|W|W
Instant
Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
Exile target attacking or blocking creature. */

	if( event == EVENT_MODIFY_COST){
		if( has_convoked_mana_extended(player, card, EVENT_CAN_CAST, MANACOST_XW(1, 2)) ){
			null_casting_cost(player, card);
			get_card_instance( player, card )->info_slot = 1;
		}
		else{
			get_card_instance( player, card )->info_slot = 0;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_state = TARGET_STATE_IN_COMBAT;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int result = charge_convoked_mana_extended(player, card, event, MANACOST_XW(1, 2));
			if( result & (1<<31) ){
				td.allow_cancel = 0;
			}
		}
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
							"Select target attacking or blocking creature.", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_dimir_cutpurse(int player, int card, event_t event){
/*
Dimir Cutpurse |1|U|B
Creature — Spirit 2/2
Whenever Dimir Cutpurse deals combat damage to a player, that player discards a card and you draw a card.
*/
	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result )
	{
		int times_damaged[2] = { BYTE0(result), BYTE1(result) };
		APNAP(p,{
					new_multidiscard(p, times_damaged[p], 0, player);
					draw_cards(player, times_damaged[p]);
				}
		);
	}

	return 0;
}

static int effect_dimir_doppelganger(int player, int card, event_t event);
static int dimir_doppelganger_impl(int player, int card, event_t event, int is_effect)
{
  if (!IS_ACTIVATING(event))
	return 0;

  card_instance_t* instance = get_card_instance(player, card), *parent;
  int p, c;
  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  p = instance->parent_controller;
	  c = instance->parent_card;
	  parent = get_card_instance(p, c);
	}
  else
	{
	  p = player;
	  c = card;
	  parent = instance;
	}

  if (is_effect)
	{
	  p = parent->damage_target_player;
	  c = parent->damage_target_card;
	}

  test_definition_t test;
  new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");

  if (event == EVENT_CAN_ACTIVATE)
	return CAN_ACTIVATE(p, c, MANACOST_XUB(1,1,1)) && !graveyard_has_shroud(ANYBODY) && has_dead_creature(ANYBODY);

  if (event == EVENT_ACTIVATE
	  && charge_mana_for_activated_ability(p, c, MANACOST_XUB(1,1,1))
	  && select_target_from_either_grave(player, card, 0, AI_MAX_VALUE, AI_MAX_VALUE, &test, 1, 2) == -1)
	cancel = 1;

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  int selected = validate_target_from_grave_source(player, card, instance->targets[1].player, 2);
	  if (selected != -1)
		{
		  int iid = get_grave(instance->targets[1].player)[selected];
		  rfg_card_from_grave(instance->targets[1].player, selected);

		  if (in_play(p, c))
			{
			  if (!is_effect)
				create_targetted_legacy_activate(p, c, effect_dimir_doppelganger, p, c);

			  cloning_and_verify_legend(p, c, -1, iid);
			}
		}
	}

  return 0;
}
static int effect_dimir_doppelganger(int player, int card, event_t event)
{
  return dimir_doppelganger_impl(player, card, event, 1);
}
int card_dimir_doppelganger(int player, int card, event_t event)
{
	/* Dimir Doppelganger	|1|U|B	0x20066c3
	 * Creature - Shapeshifter 0/2
	 * |1|U|B: Exile target creature card from a graveyard. ~ becomes a copy of that card, except it has this ability. */
  cloning_card(player, card, event);

  return dimir_doppelganger_impl(player, card, event, 0);
}

int card_dimir_guildmage(int player, int card, event_t event){
/*
Dimir Guildmage {U/B}{U/B}
Creature — Human Wizard 2/2
({U/B} can be paid with either {U} or {B}.)
{3}{U}: Target player draws a card. Activate this ability only any time you could cast a sorcery.
{3}{B}: Target player discards a card. Activate this ability only any time you could cast a sorcery.
*/
	if( ! IS_GAA_EVENT(event) ){
		return hybrid(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_DRAW = 1,
		CHOICE_DISCARD
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_CAN_SORCERY_BE_PLAYED, MANACOST0, 0, &td, NULL) ){
			if( has_mana_for_activated_ability(player, card, MANACOST_XU(3, 1)) ||
				has_mana_for_activated_ability(player, card, MANACOST_XB(3, 1)) )
			{
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						has_mana_for_activated_ability(player, card, MANACOST_XU(3, 1)),
						has_mana_for_activated_ability(player, card, MANACOST_XB(3, 1))
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Target player draws", abils[1], 5,
							"Target player discard", abils[2], hand_count[1-player] > 0 ? 5 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_DRAW ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XU(3, 1), 0, &td, "TARGET_PLAYER");
			if( spell_fizzled != 1 ){
				ai_modifier_for_targeted_card_drawing(player, card, 0, 1);
			}
		}
		if( choice == CHOICE_DISCARD ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, "TARGET_CREATURE");
			if( spell_fizzled != 1 ){
				ai_modifier += (instance->targets[0].player == 1-player ? 48 : -48);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			if( instance->info_slot == CHOICE_DRAW ){
				draw_cards(instance->targets[0].player, 1);
			}
			if( instance->info_slot == CHOICE_DISCARD ){
				discard(instance->targets[0].player, 0, player);
			}
		}
	}
	return 0;
}

int card_dimir_house_guard(int player, int card, event_t event)
{
  /* Dimir House Guard	|3|B
   * Creature - Skeleton 2/3
   * Fear
   * Sacrifice a creature: Regenerate ~.
   * Transmute |1|B|B */

  fear(player, card, event);

  if (IS_ACTIVATING(event) && (land_can_be_played & LCBP_REGENERATION))
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  if (IS_AI(player))
		test.not_me = 1;

	  if (event == EVENT_CAN_ACTIVATE && CAN_ACTIVATE0(player, card) && new_can_sacrifice_as_cost(player, card, &test))
		return can_regenerate(player, card);

	  if (event == EVENT_ACTIVATE && charge_mana_for_activated_ability(player, card, MANACOST0))
		new_sacrifice(player, card, player, SAC_AS_COST, &test);

	  if (event == EVENT_RESOLVE_ACTIVATION && can_regenerate(instance->parent_controller, instance->parent_card))
		regenerate_target(instance->parent_controller, instance->parent_card);
	}

  return transmute(player, card, event, MANACOST_XB(1,2), 4);
}

int card_dimir_infiltrator(int player, int card, event_t event){
/* CARD_ID_DIMIR_INFILTRATOR	7818
Dimir Infiltrator	|U|B
Creature - Spirit 1/3
~ can't be blocked.
Transmute |1|U|B (|1|U|B, Discard this card: Search your library for a card with the same converted mana cost as this card,
reveal it, and put it into your hand. Then shuffle your library. Transmute only as a sorcery.) */

	unblockable(player, card, event);
	return transmute(player, card, event, MANACOST_XUB(1, 1, 1), 2);
}

/* static void look_at_the_top_x_exile_one_and_rearrange_the_rest(int player, int card, int t_player, int amount){
	int qty = MIN(amount, count_deck(t_player));
	if( qty ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a card to exile.");
		test.qty = qty;
		test.no_shuffle = 1;
		if(	IS_AI(player) ){
			if( t_player == player ){
				int my_lands = get_total_available_lands(player);
				if( my_lands <= 3 ){
					default_test_definition(&test, TYPE_LAND);
					test.type_flag = DOESNT_MATCH;
					test.qty = qty;
					test.no_shuffle = 1;
				}
				if( my_lands >= 6 ){
					default_test_definition(&test, TYPE_LAND);
					test.type_flag = MATCH;
					test.qty = qty;
					test.no_shuffle = 1;
				}
			}
			else{
				int opp_lands = count_subtype(1-player, TYPE_LAND, -1);
				if( opp_lands <= 3 ){
					default_test_definition(&test, TYPE_LAND);
					test.type_flag = MATCH;
					test.qty = qty;
					test.no_shuffle = 1;
				}
				if( opp_lands >= 6 ){
					default_test_definition(&test, TYPE_LAND);
					test.type_flag = DOESNT_MATCH;
					test.qty = qty;
					test.no_shuffle = 1;
				}
			}
		}
		int selected = new_global_tutor(player, t_player, TUTOR_FROM_DECK, TUTOR_RFG, 1, AI_MAX_VALUE, &test);
		if( selected == -1 ){
			new_default_test_definition(&test, TYPE_ANY, "Select a card to exile.");
			test.qty = qty;
			test.no_shuffle = 1;
			int mode = t_player == player ? AI_MIN_VALUE : AI_MAX_VALUE;
			new_global_tutor(player, t_player, TUTOR_FROM_DECK, TUTOR_RFG, 1, mode, &test);
		}
		qty--;
		if( qty ){
			rearrange_top_x(player, t_player, qty);
		}
	}
}*/

int card_dimir_machinations(int player, int card, event_t event){
/*
Dimir Machinations |2|B
Sorcery
Look at the top three cards of target player's library. Exile any number of those cards, then put the rest back in any order.
Transmute {1}{B}{B} ({1}{B}{B}, Discard this card: Search your library for a card with the same converted mana cost as this card,
	reveal it, and put it into your hand. Then shuffle your library. Transmute only as a sorcery.)
*/
	if( IS_ACTIVATING_FROM_HAND(event) ){
		return transmute(player, card, event, MANACOST_XB(1, 2), 3);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			int amount = get_total_available_lands(player);
			if( amount <= 3 || amount >= 6 ){
				ai_modifier += (instance->targets[0].player == player ? 48 : -48);
			}
			else{
				ai_modifier += (instance->targets[0].player == 1-player ? 48 : -48);
			}
		}
		return 0;
	}
	int *deck = deck_ptr[player];
    int total = MIN(3, count_deck(player));
	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			while( total ){
					int selected = show_deck( player, deck, total, "Choose a card to remove", 0, 0x7375B0);
					if( selected != -1 ){
						rfg_card_in_deck(player, selected);
						total--;
					}
					else{
						break;
					}
			}
		rearrange_top_x(player, player, total);
	}

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_dimir_signet(int player, int card, event_t event){
/* CARD_ID_DIMIR_SIGNET	1719
Dimir Signet	|2
Artifact
|1, |T: Add |U|B to your mana pool. */

	return signet2(player, card, event, COLOR_BLACK, COLOR_BLUE);
}

int card_dogpile(int player, int card, event_t event){
/*
Dogpile |3|R
Instant
Dogpile deals damage to target creature or player equal to the number of attacking creatures you control.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, count_attackers(player));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE_OR_PLAYER", 1, NULL);
}

int card_doubling_season(int player, int card, event_t event){
/* CARD_ID_DOUBLING_SEASON	7824
Doubling Season	|4|G
Enchantment
If an effect would create one or more tokens under your control, it creates twice that many of those tokens instead.
If an effect would put one or more counters on a permanent you control, it puts twice that many of those counters on that permanent instead. */

  return global_enchantment(player, card, event);
}

int card_dream_leash(int player, int card, event_t event){
/*
Dream Leash |3|U|U
Enchantment — Aura
Enchant permanent
You can't choose an untapped permanent as Dream Leash's target as you cast Dream Leash.
You control enchanted permanent.
*/
	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );

	if( event == EVENT_CAN_CAST ){
		td.required_state = TARGET_STATE_TAPPED;
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		td.required_state = TARGET_STATE_TAPPED;
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target tapped permanent.", 1, NULL);
		if( spell_fizzled != 1 ){
			card_instance_t *instance = get_card_instance(player, card);
			ai_modifier += (instance->targets[0].player == 1-player ? 48 : -48);
		}
		return 0;
	}

	return generic_stealing_aura(player, card, event, &td, "TARGET_PERMANENT");
}

int card_drift_of_phantasms(int player, int card, event_t event){
/* CARD_ID_DRIFT_OF_PHANTASMS	7828
Drift of Phantasms	|2|U
Creature - Spirit 0/5
Defender (This creature can't attack.)
Flying
Transmute |1|U|U (|1|U|U, Discard this card: Search your library for a card with the same converted mana cost as this card,
reveal it, and put it into your hand. Then shuffle your library. Transmute only as a sorcery.) */

	return transmute(player, card, event, MANACOST_XU(1, 2), 3);
}

int card_drooling_groodion(int player, int card, event_t event){
/*
Drooling Groodion |3|G|B|B
Creature — Beast 4/3
{2}{B}{G}, Sacrifice a creature: Target creature gets +2/+2 until end of turn. Another target creature gets -2/-2 until end of turn.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = ANYBODY;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	if( event == EVENT_CAN_ACTIVATE && target_available(player, card, &td) >= 2 ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE, MANACOST_XBG(2, 1, 1), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		state_untargettable(BYTE2(sac), BYTE3(sac), 0);
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE | GAA_LITERAL_PROMPT,
									MANACOST_XBG(2, 1, 1), 0, &td, "Select target creature (+2/+2).");
		if( spell_fizzled != 1 ){
			ai_modifier += (instance->targets[0].player == player ? 48 : -48);
			if( new_pick_target(&td, "Select target creature (-2/-2).", 1, 1 | GS_LITERAL_PROMPT) ){
				ai_modifier += (instance->targets[0].player == 1-player ? 48 : -48);
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td, 0) ){
			pump_until_eot(instance->parent_controller, instance->parent_card,
							instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
		if( validate_target(player, card, &td, 1) ){
			pump_until_eot(instance->parent_controller, instance->parent_card,
							instance->targets[1].player, instance->targets[1].card, -2, -2);
		}
	}

	return 0;
}

int card_dryads_caress(int player, int card, event_t event){
/* CARD_ID_DRYADS_CARESS	7831
Dryad's Caress	|4|G|G
Instant
You gain 1 life for each creature on the battlefield. If |W was spent to cast ~, untap all creatures you control. */

	ravnica_manachanger(player, card, event, 0, 0, 0, 1, 0);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance( player, card )->targets[1].player = 1;
		if( ! played_for_free(player, card) && ! is_token(player, card) && get_card_instance( player, card )->info_slot == 1){
			get_card_instance( player, card )->targets[1].player = charge_changed_mana(player, card, 0, 0, 0, 0, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, count_permanents_by_type(player, TYPE_CREATURE));
		if( get_card_instance( player, card )->targets[1].player == 2 ){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			new_manipulate_all(player, card, player, &test, ACT_UNTAP);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_duskmantle_house_of_shadows(int player, int card, event_t event){
	/* Duskmantle, House of Shadow	""
	 * Land
	 * |T: Add |C to your mana pool.
	 * |U|B, |T: Target player puts the top card of his or her library into his or her graveyard. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_MILL
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_UB(1, 1), 0, &td, NULL) )
		{
			choice = CHOICE_MILL;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_MILL && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Mill target player", 1, 5);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_MILL ){
			if( count_deck(1-player) <= 10 ){
				ai_modifier += 48;
			}
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_UB(1, 1), 0, &td, "TARGET_PLAYER");
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_MILL ){
			if( valid_target(&td) ){
				mill(instance->targets[0].player, 1);
			}
		}
	}

	return 0;
}

int card_elvish_skysweeper(int player, int card, event_t event){
/* CARD_ID_ELVISH_SKYSWEEPER	7833
Elvish Skysweeper	|G
Creature - Elf Warrior 1/1
|4|G, Sacrifice a creature: Destroy target creature with flying. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_abilities = KEYWORD_FLYING;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE | GAA_LITERAL_PROMPT,
									MANACOST_XG(4, 1), 0, &td, "Select target creature with flying.");
}

int card_empty_the_catacombs(int player, int card, event_t event){

	/* Empty the Catacombs	|3|B
	 * Sorcery
	 * Each player returns all creature cards from his or her graveyard to his or her hand. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "");

		int num_returned[2];
		num_returned[current_turn] = from_grave_to_hand_multiple(current_turn, &test);
		num_returned[1-current_turn] = from_grave_to_hand_multiple(1-current_turn, &test);

		if (player == AI && num_returned[player] == 0){
			ai_modifier -= 96;
		}

		ai_modifier += 12 * (num_returned[AI] - num_returned[HUMAN]);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static void store_spell_on_eye_of_the_storm(int player, int card, int owner, int iid){
/* "Whenever a player casts an instant or sorcery card, exile it." and record it on the card, from "instance->targets[0]" to
 * "instance->targets[9]", for a total of 18 available slots.
 */
	card_instance_t *instance = get_card_instance(player, card);
	int i;
	for(i=1; i<10; i++){
		if( instance->targets[i].player == -1 ){
			instance->targets[i].player = iid;
			instance->targets[i].player |= (owner == 1 ? (1<<31) : 0);
			break;
		}
		if( instance->targets[i].card == -1 ){
			instance->targets[i].card = iid;
			instance->targets[i].card |= (owner == 1 ? (1<<31) : 0);
			break;
		}
	}
}

static void eye_of_the_storm_trigger(int player, int card, int triggering_player){
/* "[...] Then that player copies each instant or sorcery card exiled with Eye of the Storm. For each copy, the player may cast the
 * copy without paying its mana cost."
 * Also check if a card was removed from owner's exile zone and update storage.
 */

	card_instance_t *instance = get_card_instance(player, card);
	int exiled[2][18];
	int ec[2] = {0};
	int i;
	for(i=1; i<10; i++){
		int result = instance->targets[i].player;
		if( result == -1 ){
			break;
		}
		instance->targets[i].player = -1;
		int owner = (result & (1<<31)) ? 1 : 0;
		int iid = result &= ~(1<<31);
		if( check_rfg(owner, cards_data[iid].id) ){
			exiled[0][ec[0]] = result;
			ec[0]++;
		}
		if( can_legally_play_iid(triggering_player, iid) ){
			exiled[1][ec[1]] = iid;
			ec[1]++;
		}
		result = instance->targets[i].card;
		if( result == -1 ){
			break;
		}
		instance->targets[i].card = -1;
		owner = (result & (1<<31)) ? 1 : 0;
		iid = result &= ~(1<<31);
		if( check_rfg(owner, cards_data[iid].id) ){
			exiled[0][ec[0]] = result;
			ec[0]++;
		}
		if( can_legally_play_iid(triggering_player, iid) ){
			exiled[1][ec[1]] = iid;
			ec[1]++;
		}
	}
	if( ec[1] ){
		int opts[18] = {0};
		int priorities[18] = {0};
		char msg[18][100];
		for(i=0; i<ec[1]; i++){
			scnprintf(msg[i], 100, "Play %s", cards_ptr[cards_data[exiled[1][i]].id]->name);
			opts[i] = 1;
			priorities[i] = cards_ptr[cards_data[exiled[1][i]].id]->ai_base_value;
		}
		while( ec[1] ){
				int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL,
									DLG_WHO_CHOOSES(triggering_player),
									msg[0], opts[0], priorities[0],
									msg[1], opts[1], priorities[1],
									msg[2], opts[2], priorities[2],
									msg[3], opts[3], priorities[3],
									msg[4], opts[4], priorities[4],
									msg[5], opts[5], priorities[5],
									msg[6], opts[6], priorities[6],
									msg[7], opts[7], priorities[7],
									msg[8], opts[8], priorities[8],
									msg[9], opts[9], priorities[9],
									msg[10], opts[10], priorities[10],
									msg[11], opts[11], priorities[11],
									msg[12], opts[12], priorities[12],
									msg[13], opts[13], priorities[13],
									msg[14], opts[14], priorities[14],
									msg[15], opts[15], priorities[15],
									msg[16], opts[16], priorities[16],
									msg[17], opts[17], priorities[17]);
				if( ! choice ){
					break;
				}
				opts[choice-1] = 0;
				ec[1]--;
				copy_spell(triggering_player, cards_data[exiled[1][choice]].id);
		}
	}
	for(i=0; i<ec[0]; i++){
		int k;
		for(k = 1; k<10; k++){
			if( instance->targets[k].player == -1 ){
				instance->targets[k].player = exiled[0][i];
				break;
			}
			if( instance->targets[k].card == -1 ){
				instance->targets[k].card = exiled[0][i];
				break;
			}
		}
	}
}

int card_eye_of_the_storm(int player, int card, event_t event){
/*
Eye of the Storm |5|U|U	0x200f54d
Enchantment
Whenever a player casts an instant or sorcery card, exile it. Then that player copies each instant or sorcery card exiled with
	Eye of the Storm. For each copy, the player may cast the copy without paying its mana cost.
*/
	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		if( ! is_token(trigger_cause_controller, trigger_cause) ){
			test_definition_t test;
			default_test_definition(&test, TYPE_SPELL);
			if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &test) ){
				int iid = get_original_internal_card_id(trigger_cause_controller, trigger_cause);
				int owner = get_owner(trigger_cause_controller, trigger_cause);
				store_spell_on_eye_of_the_storm(player, card, owner, iid);
				put_on_top_of_deck(trigger_cause_controller, trigger_cause);
				rfg_top_card_of_deck(trigger_cause_controller);
				create_legacy_name_with_rfg_check(player, card, owner, iid);
				eye_of_the_storm_trigger(player, card, trigger_cause_controller);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_excruciator(int player, int card, event_t event){
/* CARD_ID_EXCRUCIATOR	7836
Excruciator	|6|R|R
Creature - Avatar 7/7
Damage that would be dealt by ~ can't be prevented. */

	my_damage_cannot_be_prevented(player, card, event);
	return 0;
}

int card_faiths_fetters(int player, int card, event_t event){
/* CARD_ID_FAITHS_FETTERS	7838
Faith's Fetters	|3|W
Enchantment - Aura
Enchant permanent
When ~ enters the battlefield, you gain 4 life.
Enchanted permanent's activated abilities can't be activated unless they're mana abilities. If enchanted permanent is a creature, it can't attack or block. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 4);
	}

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance( player, card )->damage_target_player > -1 ){
			card_instance_t *instance = get_card_instance(player, card);
			cannot_attack(instance->damage_target_player, instance->damage_target_card, event);
		    cannot_block(instance->damage_target_player, instance->damage_target_card, event);
			if( leaves_play(player, card, event) ){
			     disable_nonmana_activated_abilities(instance->damage_target_player, instance->damage_target_card, 0);
			     return 0;
		    }
			int iid = get_card_instance(instance->targets[0].player, instance->targets[0].card)->internal_card_id;
			if( cards_data[iid].extra_ability & EA_ACT_ABILITY ){
				ai_modifier += (get_cmc(instance->targets[0].player, instance->targets[0].card) * 12);
			}
			else{
				ai_modifier -= 48;
			}

	}
	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	return disabling_targeted_aura(player, card, event, &td, "Select target permanent.", 1);
}

int card_farseek(int player, int card, event_t event){
/* CARD_ID_FARSEEK	1507
Farseek	|1|G
Sorcery
Search your library for |Ha Plains, |H2Island, |H2Swamp, or |H2Mountain card and put it onto the battlefield tapped. Then shuffle your library.*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int lands = get_total_available_lands(player);
		if( lands <= 3 || lands >= 6 ){
			ai_modifier += 48;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, get_hacked_land_text(player, card, "Select %a, %s, %s, or %s card.",
																				SUBTYPE_PLAINS, SUBTYPE_ISLAND, SUBTYPE_SWAMP, SUBTYPE_MOUNTAIN));
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_SWAMP);
		this_test.sub2 = get_hacked_subtype(player, card, SUBTYPE_ISLAND);
		this_test.sub3 = get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN);
		this_test.sub4 = get_hacked_subtype(player, card, SUBTYPE_PLAINS);
		this_test.subtype_flag = F2_MULTISUBTYPE;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY_TAPPED, 0, AI_MAX_VALUE, &this_test);
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_festival_of_the_guildpact(int player, int card, event_t event){
/* CARD_ID_FESTIVAL_OF_THE_GUILDPACT	7839
Festival of the Guildpact	|X|W
Instant
Prevent the next X damage that would be dealt to you this turn.
Draw a card. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( current_turn != player ){
			ai_modifier += (count_attackers(1-player) * 12);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card );
		if( instance->info_slot > 0 ){
			prevent_the_next_n_damage(player, card, player, -1, instance->info_slot, 0, 0, 0);
		}
		draw_cards(player, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_flamekin_zealot(int player, int card, event_t event){
/* CARD_ID_FLAME_KIN_ZEALOT	7844
Flame-Kin Zealot	|1|R|R|W
Creature - Elemental Berserker 2/2
When ~ enters the battlefield, creatures you control get +1/+1 and gain haste until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		pump_subtype_until_eot(player, card, player, -1, 1, 1, 0, SP_KEYWORD_HASTE);
	}

	return 0;
}

int card_fiery_conclusion(int player, int card, event_t event)
{
  /* Fiery Conclusion	|1|R	0x200dd38
   * Instant
   * As an additional cost to cast ~, sacrifice a creature.
   * ~ deals 5 damage to target creature. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		damage_target0(player, card, 5);
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET|GS_SAC_CREATURE_AS_COST, &td, "TARGET_CREATURE", 1, NULL);
}

int card_firemane_angel(int player, int card, event_t event){
/*
Firemane Angel |3|W|W|R
Creature — Angel 4/3
Flying, first strike
At the beginning of your upkeep, if Firemane Angel is in your graveyard or on the battlefield, you may gain 1 life.
{6}{R}{R}{W}{W}: Return Firemane Angel from your graveyard to the battlefield. Activate this ability only during your upkeep.
*/
	if( current_turn == player && event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		int abils[3] = {0, 1, has_mana_multi(player, MANACOST_XWR(6, 2, 2))};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1,
							"Gain 1 life", abils[1], 1,
							"Return Firemane Angel to your hand", abils[2], 1,
							"Do both", abils[1] && abils[2], 20);
		if( ! choice ){
			return -2;
		}
		if( choice & 1 ){
			gain_life(player, 1);
		}
		if( choice & 2 ){
			charge_mana_multi(player, MANACOST_XWR(6, 2, 2));
			if( spell_fizzled != 1 ){
				remove_state(player, card, STATE_INVISIBLE);
				put_into_play(player, card);
				return -1;
			}
			else{
				return -2;
			}
		}
	}

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		gain_life(player, 1);
	}

	return 0;
}

int card_fists_of_ironwood(int player, int card, event_t event){
	/* Fists of Ironwood	|1|G
	 * Enchantment - Aura
	 * Enchant creature
	 * When ~ enters the battlefield, put two 1/1 |Sgreen Saproling creature tokens onto the battlefield.
	 * Enchanted creature has trample. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, 2);
	}

	return generic_aura(player, card, event, player, 0, 0, KEYWORD_TRAMPLE, 0, 0, 0, 0);
}

int effect_flame_fusillade(int player, int card, event_t event)
{
  // Until end of turn, permanents you control gain "|T: This permanent deals 1 damage to any target."

  // Local data usage:
  // targets[0]: Player or creature targeted to be dealt damaged.
  // targets[1]: Permanent being tapped to deal damage.
  // targets[2].player: Types of permanents that can be activated.  Must be set when effect is created.

  if (eot_trigger(player, card, event))
	kill_card(player, card, KILL_EXILE);

  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION)
	return 0;

  card_instance_t* instance = get_card_instance(player, card);

  target_definition_t td_tap;
  default_target_definition(player, card, &td_tap, instance->targets[2].player);
  td_tap.allowed_controller = player;
  td_tap.preferred_controller = player;
  td_tap.illegal_abilities = 0;
  td_tap.illegal_state = TARGET_STATE_TAPPED | TARGET_STATE_SUMMONING_SICK;

#define DECL_TGT(p, c, td_tgt)	\
  target_definition_t td_tgt;	\
  default_target_definition(p, c, &td_tgt, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);	\
  td_tgt.zone = TARGET_ZONE_CREATURE_OR_PLAYER

#define ABILITY(p, c, event, td_tgt)	\
  generic_activated_ability(p, c, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(0), 0, &td_tgt, "TARGET_ANY")

  if (event == EVENT_CAN_ACTIVATE && !is_humiliated(player, card))
	{
	  int p = player, c;
	  for (c = 0; c < active_cards_count[p]; ++c)
		if (in_play(p, c) && is_what(p, c, instance->targets[2].player))
		  {
			DECL_TGT(p, c, td_tgt);
			if (ABILITY(p, c, event, td_tgt))
			  return 1;
		  }
	}

  if (event == EVENT_ACTIVATE)
	{
	  instance->number_of_targets = 0;

	  char typetext[100], prompt[200];
	  if (ai_is_speculating == 1)
		prompt[0] = 0;
	  else
		scnprintf(prompt, 200, "Select %s you control.", type_text(typetext, 100, instance->targets[2].player, 0));

	  if (pick_next_target_noload(&td_tap, prompt))
		{
		  int p = instance->targets[0].player;
		  int c = instance->targets[0].card;

		  if (can_use_activated_abilities(p, c))
			{
			  DECL_TGT(p, c, td_tgt);
			  if (can_target(&td_tgt))
				{
				  instance->targets[1] = instance->targets[0];	// struct copy

				  card_instance_t* activating = get_card_instance(p, c);
				  int old_number_of_targets = activating->number_of_targets;	// since select_targets() overwrites it
				  load_text(0, "TARGET_ANY");
				  if (select_target(p, c, &td_tgt, text_lines[0], &instance->targets[0]))
					{
					  activating->number_of_targets = old_number_of_targets;
					  instance->number_of_targets = 1;
					  tap_card(p, c);
					  return 0;
					}
				  activating->number_of_targets = old_number_of_targets;
				}
			}
		}
	  instance->number_of_targets = 0;
	  spell_fizzled = 1;
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  int p = instance->targets[1].player;
	  int c = instance->targets[1].card;
	  DECL_TGT(p, c, td_tgt);

	  if (validate_arbitrary_target(&td_tgt, instance->targets[0].player, instance->targets[0].card))
		damage_creature(instance->targets[0].player, instance->targets[0].card, 1, p, c);
	}

  return 0;
#undef DECL_TGT
#undef ABILITY
}

int card_flame_fusillade(int player, int card, event_t event)
{
	/* Flame Fusillade	|3|R	0x200691b
	 * Sorcery
	 * Until end of turn, permanents you control gain "|T: This permanent deals 1 damage to any target." */
	if (event == EVENT_CAN_CAST)
		return basic_spell(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int total = 0;
		int c;
		for(c = 0; c < active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_CREATURE) ){
				target_definition_t td;
				default_target_definition(player, c, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
				td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
				if( generic_activated_ability(player, c, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
					total++;
				}
			}
		}
		if( life[1-player] <= total ){
			ai_modifier += 256;
		}
		else{
			ai_modifier += (total * 8);
		}
	}


	if (event == EVENT_RESOLVE_SPELL)
	{
	  int leg = create_legacy_activate(player, card, &effect_flame_fusillade);
	  if (leg >= 0)
		get_card_instance(player, leg)->targets[2].player = TYPE_PERMANENT;

	  kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int flash_conscript_damage_effect(int player, int card, event_t event){
	card_instance_t *instance= get_card_instance(player, card);

	if( instance->damage_target_player > -1 ){
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_DEAL_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_source_card == c && damage->damage_source_player == p ){
					if( damage->token_status & (STATUS_COMBAT_DAMAGE|STATUS_FIRST_STRIKE_DAMAGE) ){
						int good = damage->info_slot;
						if( good < 1){
							card_instance_t *trg = get_card_instance(p, c);
							if( trg->targets[16].player > 0 ){
								good = trg->targets[16].player;
							}
						}

						if( good > 0 ){
							if( instance->targets[1].player < 0 ){
								instance->targets[1].player = 0;
							}
							instance->targets[1].player+=good;
						}
					}
				}
			}
		}

		if( instance->targets[1].player > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
			reason_for_trigger_controller == player ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					gain_life(player, instance->targets[1].player);
					instance->targets[1].player = 0;
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_flash_conscript(int player, int card, event_t event){
/* CARD_ID_FLASH_CONSCRIPTION	7845
Flash Conscription	|5|R
Instant
Untap target creature and gain control of it until end of turn. That creature gains haste until end of turn.
If |W was spent to cast ~, the creature gains "Whenever this creature deals combat damage, you gain that much life" until end of turn. */

	ravnica_manachanger(player, card, event, 0, 0, 0, 0, 1);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td1, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = instance->targets[1].player = 0;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1){
			charge_changed_mana(player, card, 0, 0, 0, 0, 1);
		}
		if( spell_fizzled != 1 && pick_target(&td1, "TARGET_CREATURE") ){
			ai_modifier_for_act_of_treason(player, card);
			instance->targets[1].player = mana_paid[COLOR_WHITE] > 0 ? 2 : 0;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			if( instance->targets[1].player == 2 ){
				int l1 = create_targetted_legacy_effect(player, card, &flash_conscript_damage_effect,
														instance->targets[0].player, instance->targets[0].card);
				add_status(player, l1, STATUS_INVISIBLE_FX);
			}
			if( instance->targets[0].player == player ){
				untap_card(instance->targets[0].player, instance->targets[0].card);
				pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_HASTE);
			}
			else{
				effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_flight_of_fancy(int player, int card, event_t event){
/* CARD_ID_FLIGHT_OF_FANCY	7847
Flight of Fancy	|3|U
Enchantment - Aura
Enchant creature
When ~ enters the battlefield, draw two cards.
Enchanted creature has flying. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 2);
	}

	return generic_aura(player, card, event, player, 0, 0, KEYWORD_FLYING, 0, 0, 0, 0);
}

int card_flow_of_ideas(int player, int card, event_t event){
/* CARD_ID_FLOW_OF_IDEAS	7848
Flow of Ideas	|5|U
Sorcery
Draw a card for each |H2Island you control. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_ISLAND)) * 8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_ISLAND)));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_followed_footsteps(int player, int card, event_t event){
	/* Followed Footsteps	|3|U|U
	 * Enchantment - Aura
	 * Enchant creature
	 * At the beginning of your upkeep, put a token that's a copy of enchanted creature onto the battlefield. */

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance( player, card )->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance( player, card );

		upkeep_trigger_ability(player, card, event, player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			token_generation_t token;
			copy_token_definition(player, card, &token, instance->damage_target_player, instance->damage_target_card);
			generate_token(&token);
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier += (get_cmc(instance->targets[0].player, instance->targets[0].card) * 8);
		}
		return 0;
	}

	return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

int card_frenzied_goblin(int player, int card, event_t event)
{
/* CARD_ID_FRENZIED_GOBLIN	7850
Frenzied Goblin	|R
Creature - Goblin Berserker 1/1
Whenever ~ attacks, you may pay |R. If you do, target creature can't block this turn. */

  if ((xtrigger_condition() == XTRIGGER_ATTACKING || event == EVENT_DECLARE_ATTACKERS)
	  && has_mana(player, COLOR_RED, 1))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);

	  if (can_target(&td)
		  && declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), player, card)
		  && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_RED, 1))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  instance->number_of_targets = 0;
		  if (pick_target(&td, "TARGET_CREATURE"))
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
		}
	}

  return 0;
}

int card_flickerform(int player, int card, event_t event){
/* CARD_ID_FLICKERFORM	7846
Flickerform	|1|W
Enchantment - Aura
Enchant creature
|2|W|W: Exile enchanted creature and all Auras attached to it. At the beginning of the next end step, return that card to the battlefield under its owner's control.
If you do, return the other cards exiled this way to the battlefield under their owners' control attached to that creature. */

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return generic_activated_ability(player, card, event, 0, MANACOST_XW(2, 2), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t* instance = get_card_instance(player, card);
		exile_permanent_and_auras_attached(instance->parent_controller, instance->parent_card,
											instance->damage_target_player, instance->damage_target_card, EPAAC_STORE_AURAS_RETURN_EOT);
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier += (get_cmc(instance->targets[0].player, instance->targets[0].card) * 8);
		}
		return 0;
	}

	return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

int card_gather_courage(int player, int card, event_t event){
/* CARD_ID_GATHER_COURAGE	7853
Gather Courage	|G
Instant
Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
Target creature gets +2/+2 until end of turn. */

	if( event == EVENT_CHECK_PUMP ){
		return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 2, 0, 0);
	}

	if( event == EVENT_MODIFY_COST ){
		return generic_spell_with_convoke(player, card, event);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int result = cast_spell_with_convoke(player, card, event);
		if( ! result ){
			spell_fizzled = 1;
			return 0;
		}
		td.allow_cancel = result & (1<<31) ? 0 : 1;
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2, 0, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_ghosts_of_the_innocent(int player, int card, event_t event){
	/* Ghosts of the Innocent	|5|W|W	0x200693e
	 * Creature - Spirit 4/5
	 * If a source would deal damage to a permanent or player, it deals half that damage, rounded down, to that permanent or player instead. */
	card_instance_t* damage = damage_being_dealt(event);
	if( damage && in_play(player, card) && ! is_humiliated(player, card) ){
		damage->info_slot /= 2;
	}
	return 0;
}

int card_glare_of_subdual(int player, int card, event_t event){
/* CARD_ID_GLARE_OF_SUBDUAL	7856
Glare of Subdual	|2|G|W
Enchantment
Tap an untapped creature you control: Tap target artifact or creature. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;
	td.illegal_state = TARGET_STATE_TAPPED;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE | TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_target(&td1) ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
								&td, "Select an untapped creature you control.");
		if( spell_fizzled != 1 ){
			int to_tap = instance->targets[0].card;
			instance->number_of_targets = 0;
			if( new_pick_target(&td, "Select target artifact or creature.", 0, 1 | GS_LITERAL_PROMPT) ){
				tap_card(player, to_tap);
				ai_modifier += (current_turn == player ? 48 : 24);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td1, 0) ){
			tap_card( instance->targets[0].player, instance->targets[0].card );
		}
	}

	return global_enchantment(player, card, event);
}

int card_gleancrawler(int player, int card, event_t event){
/* CARD_ID_GLEANCRAWLER	7858
Gleancrawler	|3|BG|BG|BG
Creature - Insect Horror 6/6
(|BG can be paid with either |B or |G.)
Trample
At the beginning of your end step, return to your hand all creature cards in your graveyard that were put there from the battlefield this turn.*/

	hybrid(player, card, event);
	if( current_turn == player && eot_trigger(player, card, event) ){
		return_all_dead_this_turn_to_hand(player, TYPE_CREATURE);
	}
	return 0;
}

int card_glimpse_the_unthinkable(int player, int card, event_t event){
/* CARD_ID_GLIMPSE_THE_UNTHINKABLE	1566
Glimpse the Unthinkable	|U|B
Sorcery
Target player puts the top ten cards of his or her library into his or her graveyard. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_CAST_SPELL ){
		ai_modifier += (count_deck(1-player) <= 10 ? 96 : 48);
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			mill(instance->targets[0].player, 10);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_golgari_brownscale(int player, int card, event_t event){
/* CARD_ID_GOLGARI_BROWNSCALE	7860
Golgari Brownscale	|1|G|G
Creature - Lizard 2/3
When ~ is put into your hand from your graveyard, you gain 2 life.
Dredge 2 (If you would draw a card, instead you may put exactly two cards from the top of your library into your graveyard.
If you do, return this card from your graveyard to your hand. Otherwise, draw a card.) */

	if( event == EVENT_DREDGE ){
		return 2;
	}

	return 0;
}

static int golgari_germination_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_GOLGARI_GERMINATION ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(affected_card_controller, affected_card) ){
			int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
			if( kill_code ){
				if( instance->damage_target_player > -1 ){
					int p = instance->damage_target_player;
					int c = instance->damage_target_card;
					if( affect_me(p, c) ){
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
						if( instance->targets[11].player < 0 ){
							instance->targets[11].player = 0;
						}
						SET_BYTE1(instance->targets[11].player) = 1;
					}
				}
				test_definition_t test;
				default_test_definition(&test, TYPE_CREATURE);
				test.type_flag = F1_NO_TOKEN;
				count_for_gfp_ability(player, card, event, player, 0, &test);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			generate_tokens_by_id(player, card, CARD_ID_SAPROLING, amount);
			kill_card(player, card, KILL_EXILE);
		}
	}

	//Reset counts if the legacy is still invisible
	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && event == EVENT_END_TRIGGER ){
		get_card_instance(player, card)->targets[11].player = 0;
	}

	if( event == EVENT_CLEANUP && ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_golgari_germination(int player, int card, event_t event){
	/* Golgari Germination	|1|B|G
	 * Enchantment
	 * Whenever a nontoken creature you control dies, put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)golgari_germination_effect ){
					if( (inst->damage_target_player == player && inst->damage_target_card == card) ||
						(inst->targets[0].player == player && inst->targets[0].card == card) )
					{
						found = 1;
						break;
					}
				}
			}
		}
		if( ! found ){
			int legacy = create_targetted_legacy_effect(player, card, &golgari_germination_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}


	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.type_flag = F1_NO_TOKEN;

		count_for_gfp_ability(player, card, event, player, 0, &test);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			generate_tokens_by_id(player, card, CARD_ID_SAPROLING, amount);
		}
	}

	return global_enchantment(player, card, event);
}

int card_golgari_grave_troll(int player, int card, event_t event){
/*
Golgari Grave-Troll |4|G
Creature — Troll Skeleton 0/0, 4G (5)
Golgari Grave-Troll enters the battlefield with a +1/+1 counter on it for each creature card in your graveyard.
{1}, Remove a +1/+1 counter from Golgari Grave-Troll: Regenerate Golgari Grave-Troll.
Dredge 6 (If you would draw a card, instead you may put exactly six cards from the top of your library into your graveyard.
	If you do, return this card from your graveyard to your hand. Otherwise, draw a card.)
*/
	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, count_graveyard_by_type(player, TYPE_CREATURE));

	if( event == EVENT_DREDGE ){
		return 6;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return regeneration(player, card, event, MANACOST_X(1));
	}

	return generic_activated_ability(player, card, event, GAA_REGENERATION, MANACOST_X(1), GVC_COUNTER(COUNTER_P1_P1), NULL, NULL);
}

int card_golgari_guildmage(int player, int card, event_t event){
/*
Golgari Guildmage {B/G}{B/G}
Creature — Elf Shaman 2/2
{4}{B}, Sacrifice a creature: Return target creature card from your graveyard to your hand.
{4}{G}: Put a +1/+1 counter on target creature.
*/

	hybrid(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_RAISE_DEAD = 1,
		CHOICE_P1_P1
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability_with_target_in_grave(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST_XB(4, 1), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER) )
		{
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XG(4, 1), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0,
						generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_CREATURE,
																		MANACOST_XB(4, 1), 0, NULL, NULL, 0, &test, GS_GRAVE_RECYCLER),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_XG(4, 1), 0, &td, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Sac and Raise Dead", abils[1], 5,
							"Add +1/+1 counter", abils[2], current_phase == PHASE_AFTER_BLOCKING ? 10 : 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_RAISE_DEAD ){
			return generic_activated_ability_with_target_in_grave(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST_XB(4, 1), 0,
																	NULL, NULL, 0, &test, GS_GRAVE_RECYCLER);
		}
		if( choice == CHOICE_P1_P1 ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XG(4, 1), 0, &td, "TARGET_CREATURE");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_RAISE_DEAD ){
			int selected = validate_target_from_grave_source(player, card, player, 0);
			if( selected != -1 ){
				from_grave_to_hand(player, selected, TUTOR_HAND);
			}
		}
		if( instance->info_slot == CHOICE_P1_P1 && valid_target(&td) ){
				add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
		}
	}
	return 0;
}

int card_golgari_rotwurm(int player, int card, event_t event){
/* CARD_ID_GOLGARI_ROTWURM	7864
Golgari Rotwurm	|3|B|G
Creature - Zombie Wurm 5/4
|B, Sacrifice a creature: Target player loses 1 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (life[1-player] < 6 ? 48 : 0);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE, MANACOST_B(1), 0, &td, "TARGET_PLAYER");
}

int card_golgari_signet(int player, int card, event_t event){
/* CARD_ID_GOLGARI_SIGNET	1717
Golgari Signet	|2
Artifact
|1, |T: Add |B|G to your mana pool. */

	return signet2(player, card, event, COLOR_BLACK, COLOR_GREEN);
}

int card_golgari_thug(int player, int card, event_t event){
/* CARD_ID_GOLGARI_THUG	7865
Golgari Thug	|1|B
Creature - Human Warrior 1/1
When ~ dies, put target creature card from your graveyard on top of your library.
Dredge 4 (If you would draw a card, instead you may put exactly four cards from the top of your library into your graveyard.
If you do, return this card from your graveyard to your hand. Otherwise, draw a card.) */

	if( event == EVENT_DREDGE ){
		return 4;
	}

	if( this_dies_trigger(player, card, event, 2) ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) && ! graveyard_has_shroud(player) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_DECK, 1, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

int card_graveshell_scarab(int player, int card, event_t event){
/* CARD_ID_GRAVE_SHELL_SCARAB	7867
Grave-Shell Scarab	|2|B|G|G
Creature - Insect 4/4
|1, Sacrifice ~: Draw a card.
Dredge 1 (If you would draw a card, instead you may put exactly one card from the top of your library into your graveyard.
If you do, return this card from your graveyard to your hand. Otherwise, draw a card.)*/

	if( event == EVENT_DREDGE ){
		return 1;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST_X(1), 0, NULL, NULL);
}

int card_greater_mossdog(int player, int card, event_t event){
/* CARD_ID_GREATER_MOSSDOG	7870
Greater Mossdog	|3|G
Creature - Plant Hound 3/3
Dredge 3 (If you would draw a card, instead you may put exactly three cards from the top of your library into your graveyard.
If you do, return this card from your graveyard to your hand. Otherwise, draw a card.) */

	if( event == EVENT_DREDGE ){
		return 3;
	}
	return 0;
}

int card_grozoth(int player, int card, event_t event){
/*
Grozoth |6|U|U|U
Creature — Leviathan 9/9
Defender (This creature can't attack.)
When Grozoth enters the battlefield, you may search your library for any number of cards that have converted mana cost 9,
	reveal them, and put them into your hand. If you do, shuffle your library.
{4}: Grozoth loses defender until end of turn.
Transmute {1}{U}{U} ({1}{U}{U}, Discard this card: Search your library for a card with the same converted mana cost as this card, reveal it, and put it into your hand. Then shuffle your library. Transmute only as a sorcery.)
*/
	if( IS_ACTIVATING_FROM_HAND(event) ){
		return transmute(player, card, event, MANACOST_XU(1, 2), 9);
	}

	if( event == EVENT_ACTIVATE ){
		ai_modifier += ((get_card_instance(player, card)->regen_status & KEYWORD_DEFENDER) ? 48 : -48);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int leg = pump_ability_until_eot(instance->parent_controller, instance->parent_card,
										instance->parent_controller, instance->parent_card, 0, 0, KEYWORD_DEFENDER, 0);
		if (leg != -1){
			get_card_instance(instance->parent_controller, leg)->targets[4].player = 0;	// remove keyword
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a card with CMC 9.");
		while( new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test) != -1 ){
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_X(4), 0, NULL, NULL);
}

int card_gruul_signet(int player, int card, event_t event){
/* CARD_ID_GRUUL_SIGNET	1720
Gruul Signet	|2
Artifact
|1, |T: Add |R|G to your mana pool. */

	// First printed in Guildpact, but here to keep local to the other signets.
	return signet2(player, card, event, COLOR_RED, COLOR_GREEN);
}

int card_guardian_of_vitu_ghazi(int player, int card, event_t event){
/* CARD_ID_GUARDIAN_OF_VITU_GHAZI	7873
Guardian of Vitu-Ghazi	|6|G|W
Creature - Elemental 4/7
Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
Vigilance */

	vigilance(player, card, event);

	return generic_spell_with_convoke(player, card, event);
}

int card_hammerfist_giant(int player, int card, event_t event){
/* CARD_ID_HAMMERFIST_GIANT	7875
Hammerfist Giant	|4|R|R
Creature - Giant Warrior 5/4
|T: ~ deals 4 damage to each creature without flying and each player. */

	if( event == EVENT_ACTIVATE ){
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_play(p, c) && ! is_humiliated(p, c) ){
										int csvid = get_id(p, c);
										switch( csvid ){
												case CARD_ID_INNER_SANCTUM:
												case CARD_ID_LIGHT_OF_SANCTION:
												case CARD_ID_RUNE_TAILS_ESSENCE:
												{
													if( life[player] >= 10 ){
														ai_modifier += (current_turn != player ? 48 : 24);
													}
													break;
												}
												default:
													break;
										}
									}
								}
		);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		this_test.keyword_flag = 1;
		new_damage_all(instance->parent_controller, instance->parent_card, ANYBODY, 4, NDA_PLAYER_TOO, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_helldozer( int player, int card, event_t event){
/* CARD_ID_HELLDOZER	7876
Helldozer	|3|B|B|B
Creature - Zombie Giant 6/5
|B|B|B, |T: Destroy target land. If that land was nonbasic, untap ~. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(3), 0, &td, "TARGET_LAND");
		if( spell_fizzled != 1 ){
			if( ! is_basic_land(instance->targets[0].player, instance->targets[0].card) ){
				ai_modifier += 48;
			}
			else{
				int mod = count_subtype(1-player, TYPE_LAND, -1) <= 3 ? 48 : 24;
				ai_modifier += ((current_turn != player && current_phase >= PHASE_MAIN2) ? mod : (mod-12));
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			if( ! is_basic_land(instance->targets[0].player, instance->targets[0].card) ){
				untap_card(instance->parent_controller, instance->parent_card);
			}
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(3), 0, &td, "TARGET_LAND");
}

int card_hex(int player, int card, event_t event){
/* CARD_ID_HEX	7877
Hex	|4|B|B
Sorcery
Destroy six target creatures. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		int i;
		for(i=0; i<6; i++){
			if( validate_target(player, card, &td, i) ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 6, NULL);
}

int card_hour_of_reckoning(int player, int card, event_t event){
/* CARD_ID_HOUR_OF_RECKONING	7878
Hour of Reckoning	|4|W|W|W
Sorcery
Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
Destroy all nontoken creatures. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.type_flag = F1_NO_TOKEN;
		APNAP(p, {new_manipulate_all(player, card, p, &this_test, KILL_DESTROY);};);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell_with_convoke(player, card, event);
}

int card_hunted_dragon(int player, int card, event_t event){
	/* Hunted Dragon	|3|R|R
	 * Creature - Dragon 6/6
	 * Flying, haste
	 * When ~ enters the battlefield, target opponent puts three 2/2 |Swhite Knight creature tokens with first strike onto the battlefield. */

	haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_KNIGHT, &token);
		token.pow = token.tou = 2;
		token.t_player = 1-player;
		token.qty = 3;
		token.key_plus = KEYWORD_FIRST_STRIKE;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		generate_token(&token);
	}

	return 0;
}

int card_hunted_horror(int player, int card, event_t event){
/* CARD_ID_HUNTED_HORROR	7880
Hunted Horror	|B|B
Creature - Horror 7/7
Trample
When ~ enters the battlefield, target opponent creates two 3/3 |Sgreen Centaur creature tokens with protection from |Sblack. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_CENTAUR, &token);
		token.t_player = 1-player;
		token.qty = 2;
		token.key_plus = get_sleighted_protection(player, card, KEYWORD_PROT_BLACK);
		generate_token(&token);
	}

	return 0;
}

int card_hunted_lammasu(int player, int card, event_t event){
	/* Hunted Lammasu	|2|W|W
	 * Creature - Lammasu 5/5
	 * Flying
	 * When ~ enters the battlefield, target opponent puts a 4/4 |Sblack Horror creature token onto the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_HORROR, &token);
		token.t_player = 1-player;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		token.pow = 4;
		token.tou = 4;
		generate_token(&token);
	}
	return 0;
}

int card_hunted_phantasm(int player, int card, event_t event){
	/* Hunted Phantasm	|1|U|U
	 * Creature - Spirit 4/6
	 * ~ can't be blocked.
	 * When ~ enters the battlefield, target opponent puts five 1/1 |Sred Goblin creature tokens onto the battlefield. */

	unblockable(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_GOBLIN, &token);
		token.t_player = 1-player;
		token.qty = 5;
		generate_token(&token);
	}

	return 0;
}

int card_hunted_troll(int player, int card, event_t event){
	/* Hunted Troll	|2|G|G
	 * Creature - Troll Warrior 8/4
	 * When ~ enters the battlefield, target opponent puts four 1/1 |Sblue Faerie creature tokens with flying onto the battlefield.
	 * |G: Regenerate ~. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_FAERIE, &token);
		token.t_player = 1-player;
		token.pow = token.tou = 1;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		token.key_plus = KEYWORD_FLYING;
		token.qty = 4;
		generate_token(&token);
	}

	return regeneration(player, card, event, 0, 0, 0, 1, 0, 0);
}

int card_induce_paranoia(int player, int card, event_t event){
/* CARD_ID_INDUCE_PARANOIA	7886
Induce Paranoia	|2|U|U
Instant
Counter target spell. If |B was spent to cast ~, that spell's controller puts the top X cards of his or her library into his or her graveyard,
where X is the spell's converted mana cost. */

	ravnica_manachanger(player, card, event, 1, 0, 0, 0, 0);

	if( event == EVENT_CAN_CAST ){
		return card_counterspell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		instance->targets[1].player = 1;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1){
			instance->targets[1].player = charge_changed_mana(player, card, 1, 0, 0, 0, 0);
		}
		if( spell_fizzled != 1 ){
			return card_counterspell(player, card, event);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance( player, card );
			int cmc = get_cmc(instance->targets[0].player, instance->targets[0].card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			if( instance->targets[1].player == 2 ){
				mill(instance->targets[0].player, cmc);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_ivy_dancer(int player, int card, event_t event){
/* CARD_ID_IVY_DANCER	7889
Ivy Dancer	|2|G
Creature - Dryad Shaman 1/2
|T: Target creature gains |H2forestwalk until end of turn. (It can't be blocked as long as defending player controls |Ha Forest.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		if( current_turn == player && current_phase < PHASE_DECLARE_BLOCKERS && can_target(&td) ){
			ai_modifier = (count_subtype(1-player, TYPE_PERMANENT, get_hacked_subtype(player, card, SUBTYPE_FOREST)) ? 48 : -48);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card,
									0, 0, get_hacked_walk(player, card, KEYWORD_FORESTWALK), 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_izzet_signet(int player, int card, event_t event){
/* CARD_ID_IZZET_SIGNET	1550
Izzet Signet	|2
Artifact
|1, |T: Add |U|R to your mana pool. */

	// First printed in Guildpact, but here to keep local to the other signets.
	return signet2(player, card, event, COLOR_BLUE, COLOR_RED);
}

int card_keening_banshee(int player, int card, event_t event){
/* CARD_ID_KEENING_BANSHEE	7891
Keening Banshee	|2|B|B
Creature - Spirit 2/2
Flying
When ~ enters the battlefield, target creature gets -2/-2 until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td1) && new_pick_target(&td1, "TARGET_CREATURE", 0, 0)){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -2, -2);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_last_gasp(int player, int card, event_t event){
	/*
	  Last Gasp |1|B
	  Instant
	  Target creature gets -3/-3 until end of turn.
	*/
	return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -3, -3, 0, 0);
}

int card_leave_no_trace(int player, int card, event_t event){
/* CARD_ID_LEAVE_NO_TRACE	7893
Leave No Trace	|1|W
Instant
Radiance - Destroy target enchantment and each other enchantment that shares a color with it. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			radiant_action(player, card, instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target enchantment to destroy.", 1, NULL);
}

int card_life_from_the_loam(int player, int card, event_t event)
{
  /* Life from the Loam	|1|G
   * Sorcery
   * Return up to three target land cards from your graveyard to your hand.
   * Dredge 3 */

  if (event == EVENT_DREDGE)
	return 3;

  if (!IS_CASTING(player, card, event))
	return 0;
  test_definition_t test;
  new_default_test_definition(&test, TYPE_LAND, "Select up to three target land cards.");
  return spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(player, card, event, 3, &test, 0);
}

int card_light_of_sanction(int player, int card, event_t event){
/* CARD_ID_LIGHT_OF_SANCTION	7895
Light of Sanction	|1|W|W
Enchantment
Prevent all damage that would be dealt to creatures you control by sources you control. */

	if( event == EVENT_PREVENT_DAMAGE && in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_source_player == player && damage->damage_target_player == player && damage->damage_target_card != -1 ){
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_lightning_helix(int player, int card, event_t event){
	/* Lightning Helix	|R|W	0x2001155
	 * Instant
	 * ~ deals 3 damage to any target and you gain 3 life. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (life[player] < 6 ? 48 : 24);
	}

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			damage_creature_or_player(player, card, event, 3);
			gain_life(player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_lore_broker(int player, int card, event_t event){
/* CARD_ID_LORE_BROKER	7896
Lore Broker	|1|U
Creature - Human Rogue 1/2
|T: Each player draws a card, then discards a card. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		APNAP(p,{
				draw_cards(p, 1);
				discard(p, 0, player);
				};
		);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_NONSICK, MANACOST0, 0, NULL, NULL);
}

int card_lurking_informant(int player, int card, event_t event){
/* CARD_ID_LURKING_INFORMANT	7897
Lurking Informant	|1|UB
Creature - Human Rogue 1/2
(|UB can be paid with either |U or |B.)
|2, |T: Look at the top card of target player's library. You may put that card into that player's graveyard. */

	hybrid(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_PLAYER");
		if( spell_fizzled != 1 ){
			if( current_turn != player && current_phase >= PHASE_MAIN2 ){
				int total = get_total_available_lands(player);
				if( total <= 3 || total >= 6 ){
					ai_modifier += (instance->targets[0].player == player ? 48 : -48);
				}
				else{
					ai_modifier += (instance->targets[0].player == 1-player ? 12 : -48);
				}
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int iid = deck_ptr[instance->targets[0].player][0];
			if( iid != -1 ){
				int total = instance->targets[0].player == player ?
							get_total_available_lands(player) : count_subtype(1-player, TYPE_LAND, -1);
				int is_land = is_what(-1, iid, TYPE_LAND);
				int mill_first = 1;
				if( instance->targets[0].player == player ){
					if( is_land && total <= 3 ){
						mill_first = 0;
					}
					if( ! is_land && total >= 6 ){
						mill_first = 0;
					}
				}
				else{
					if( ! is_land && total <= 3 ){
						mill_first = 0;
					}
					if( is_land && total >= 6 ){
						mill_first = 0;
					}
				}
				if( ! IS_AI(player) ){
					mill_first = 0;
					if( show_deck(player, deck_ptr[instance->targets[0].player], 1, "Select a card to mill", 0, 0x7375B0 ) != -1 ){
						mill_first = 1;
					}
				}
				if( mill_first ){
					mill(instance->targets[0].player, 1);
				}
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_PLAYER");
}

int card_loxodon_hierarch(int player, int card, event_t event){
/* CARD_ID_LOXODON_HIERARCH	1368
Loxodon Hierarch	|2|G|W
Creature - Elephant Cleric 4/4
When ~ enters the battlefield, you gain 4 life.
|G|W, Sacrifice ~: Regenerate each creature you control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 4);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if(land_can_be_played && LCBP_REGENERATION){
		      if( generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_GW(1, 1), 0, NULL, NULL) ){
			    int c;
			    for(c=0; c<active_cards_count[player]; c++)
				    if( c != card && in_play(player, c) && is_what(player, c, TYPE_CREATURE) &&
					    get_card_instance(player, c)->kill_code == KILL_DESTROY )
					    return 99;
		       }
		}
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_GW(1, 1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int mod = 0;
		int c = active_cards_count[player]-1;
		while( c > -1 ){
				if( c!= card && in_play(player, c) && is_what(player, c, TYPE_CREATURE) ){
					if( get_card_instance(player, c)->kill_code == KILL_DESTROY ){
						mod += (get_cmc(player, c) * 4);
					}
				}
				c--;
		}
		ai_modifier += mod;
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_GW(1, 1), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int c = active_cards_count[player]-1;
		while( c > -1 ){
				if( c!= instance->parent_card && in_play(player, c) && is_what(player, c, TYPE_CREATURE) ){
						regenerate_or_shield(player, card, player, c);
				}
				c--;
		}
	}

	return 0;
}

int card_mark_of_eviction(int player, int card, event_t event){

	/* Mark of Eviction	|U
	 * Enchantment - Aura
	 * Enchant creature
	 * At the beginning of your upkeep, return enchanted creature and all Auras attached to that creature to their owners' hands. */

	if( ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		if( current_turn == player && upkeep_trigger(player, card, event) ){
			card_instance_t *instance = get_card_instance(player, card);
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			int i;
			for(i=0; i<2; i++){
				int count = active_cards_count[i]-1;
				while( count > -1 ){
						if( in_play(i, count) && is_what(i, count, TYPE_ENCHANTMENT) && has_subtype(i, count, SUBTYPE_AURA) ){
							card_instance_t *this = get_card_instance(i, count);
							if( this->damage_target_player == p && this->damage_target_card == c ){
								bounce_permanent(i, count);
							}
						}
						count--;
				}
			}
			bounce_permanent(p, c);
		}
	}

	return disabling_aura(player, card, event);
}

int card_mindleech_mass(int player, int card, event_t event){
	/* Mindleech Mass	|5|U|B|B
	 * Creature - Horror 6/6
	 * Trample
	 * Whenever ~ deals combat damage to a player, you may look at that player's hand. If you do, you may cast a nonland card in it without paying that card's mana cost. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					while( times_damaged[p] ){
							if( hand_count[p] ){
								test_definition_t this_test;
								new_default_test_definition(&this_test, TYPE_LAND, "Select a nonland card to cast for free.");
								this_test.type_flag = DOESNT_MATCH;
								this_test.can_legally_play = 1;
								if( p != player ){
									play_card_in_opponent_hand_by_test(player, card, &this_test);
								}
								else{
									int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
									if( selected > -1 ){
										play_card_in_hand_for_free(player, selected);
									}
								}
							}
							times_damaged[p]--;
					}
				}
		);
	}

	return 0;
}

int card_mindmoil(int player, int card, event_t event){
/* CARD_ID_MINDMOIL	7902
Mindmoil	|4|R
Enchantment
Whenever you cast a spell, put the cards in your hand on the bottom of your library in any order, then draw that many cards. */

	if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		teferis_puzzle_box_effect(player, card, player);
	}

	return global_enchantment(player, card, event);
}

int card_mnemonic_nexus(int player, int card, event_t event){
/* CARD_ID_MNEMONIC_NEXUS	7903
Mnemonic Nexus	|3|U
Instant
Each player shuffles his or her graveyard into his or her library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 24;
	}

	if( event == EVENT_RESOLVE_SPELL){
		APNAP(p, { reshuffle_grave_into_deck(p, 0); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_moldervine_cloak(int player, int card, event_t event){
	/* Moldervine Cloak	|2|G
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature gets +3/+3.
	 * Dredge 2 */

	if( event == EVENT_DREDGE ){
		return 2;
	}

	return generic_aura(player, card, event, player, 3, 3, 0, 0, 0, 0, 0);
}

int card_moonlight_bargain(int player, int card, event_t event){
/* CARD_ID_MOONLIGHT_BARGAIN	7906
Moonlight Bargain	|3|B|B
Instant
Look at the top five cards of your library. For each card, put that card into your graveyard unless you pay 2 life.
Then put the rest into your hand. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( life[player] > 6 && count_deck(player) >= 10 ){
			ai_modifier += (life[player]*4);
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		int amount = MIN(5, count_deck(player));
		int life_to_pay = 0;
		while( amount ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ANY, "Pick a card to add to your hand (you'll lose 2 life).");
				test.create_minideck = amount;
				test.no_shuffle = 1;
				int selected = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
				if( selected > -1 ){
					amount--;
					life_to_pay+=2;
					if( life[player] - life_to_pay <= 6 ){
						break;
					}
				}
				else{
					break;
				}
		}
		lose_life(player, life_to_pay);
		mill(player, amount);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_moroii(int player, int card, event_t event){
/* CARD_ID_MOROII	1311
Moroii	|2|U|B
Creature - Vampire 4/4
Flying
At the beginning of your upkeep, you lose 1 life. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		lose_life(player, 1);
	}

	return 0;
}

int card_mortipede(int player, int card, event_t event){
/* CARD_ID_MORTIPEDE	7907
Mortipede	|3|B
Creature - Insect 4/1
|2|G: All creatures able to block ~ this turn do so. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_ability_until_eot(player, instance->parent_card, player, instance->parent_card, 0, 0, 0, SP_KEYWORD_LURE);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XG(2, 1), 0, NULL, NULL);
}

int card_muddle_the_mixture(int player, int card, event_t event)
{
  /* Muddle the Mixture	|U|U
   * Instant
   * Counter target instant or sorcery spell.
   * Transmute |1|U|U */

	if (IS_ACTIVATING_FROM_HAND(event))
		return transmute(player, card, event, MANACOST_XU(1,2), 2);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_INSTANT | TYPE_INTERRUPT | TYPE_SORCERY);

	return counterspell(player, card, event, &td, 0);
}

int card_necroplasm(int player, int card, event_t event){
/* CARD_ID_NECROPLASM	7910
Necroplasm	|1|B|B
Creature - Ooze 1/1
At the beginning of your upkeep, put a +1/+1 counter on ~.
At the beginning of your end step, destroy each creature with converted mana cost equal to the number of +1/+1 counters on ~.
Dredge 2 (If you would draw a card, instead you may put exactly two cards from the top of your library into your graveyard.
If you do, return this card from your graveyard to your hand. Otherwise, draw a card.) */

	if( event == EVENT_DREDGE ){
		return 2;
	}

	if( current_turn == player ){
		upkeep_trigger_ability(player, card, event, player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			add_1_1_counter(player, card);
		}

		if( eot_trigger(player, card, event) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.cmc = count_1_1_counters(player, card);
			APNAP(p, { new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);
		}
	}

	return 0;
}

// nightguard phalanx --> serra angel

int card_nightmare_void(int player, int card, event_t event){

	/* Nightmare Void	|3|B
	 * Sorcery
	 * Target player reveals his or her hand. You choose a card from it. That player discards that card.
	 * Dredge 2 */

	if( event == EVENT_DREDGE ){
		return 2;
	}

	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 48;
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			ec_definition_t ec;
			default_ec_definition(instance->targets[0].player, player, &ec);

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			new_effect_coercion(&ec, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_nullmage_shepherd(int player, int card, event_t event){
/*
Nullmage Shepherd |3|G
Creature — Elf Shaman 2/4
Tap four untapped creatures you control: Destroy target artifact or enchantment.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_state = TARGET_STATE_TAPPED;
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td2) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td2, "DISENCHANT",
															&td, 4);
}

int card_nullstone_gargoyle(int player, int card, event_t event){
/* CARD_ID_NULLSTONE_GARGOYLE	7915
Nullstone Gargoyle	|9
Artifact Creature - Gargoyle 4/5
Flying
Whenever the first noncreature spell of a turn is cast, counter that spell. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = get_storm_count()-(get_stormcreature_count(player)+get_stormcreature_count(1-player));
	}

	if( event == EVENT_CAST_SPELL && ! affect_me(player, card) &&
		! is_what(affected_card_controller, affected_card, TYPE_CREATURE | TYPE_LAND) )
	{
		get_card_instance(player, card)->info_slot++;
	}

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.type_flag = DOESNT_MATCH;
		if( get_card_instance(player, card)->info_slot == 1 ){
			if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &test) ){
				real_counter_a_spell(player, card, trigger_cause_controller, trigger_cause);
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->info_slot = 0;
	}

	return 0;
}

int card_oathsworn_giant(int player, int card, event_t event){
/* CARD_ID_OATHSWORN_GIANT	7916
Oathsworn Giant	|4|W|W
Creature - Giant Soldier 3/4
Vigilance
Other creatures you control get +0/+2 and have vigilance. */

	if (event == EVENT_ABILITIES){
		if (affect_me(player, card)){
			// Separate check, in case this (somehow) becomes a noncreature, and it (somehow) becomes relevant whether that noncreature has vigilance.
			vigilance(player, card, event);
		} else if (affected_card_controller == player
				   && affected_card != card
				   && in_play(player, card)
				   && in_play(affected_card_controller, affected_card)
				   && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
				   && !is_humiliated(player, card)){
			vigilance(affected_card_controller, affected_card, event);
		}
	}

	return boost_creature_type(player, card, event, -1, 0, 2, 0, BCT_CONTROLLER_ONLY);
}

int card_orzhov_signet(int player, int card, event_t event){
/* CARD_ID_ORZHOV_SIGNET	1721
Orzhov Signet	|2
Artifact
|1, |T: Add |W|B to your mana pool. */

	// First printed in Guildpact, but here to keep local to the other signets.
	return signet2(player, card, event, COLOR_BLACK, COLOR_WHITE);
}

int card_overwhelm2(int player, int card, event_t event){
/* CARD_ID_OVERWHELM	7918
Overwhelm	|5|G|G
Sorcery
Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
Creatures you control get +3/+3 until end of turn. */

	if( event == EVENT_MODIFY_COST ){
		return generic_spell_with_convoke(player, card, event);
	}
	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int result = cast_spell_with_convoke(player, card, event);
		if( ! result ){
			spell_fizzled = 1;
			return 0;
		}
		if( current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS ){
			ai_modifier += (count_subtype(player, TYPE_CREATURE, -1) * 8);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 3, 3, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_pariahs_shield(int player, int card, event_t event){

	/* Pariah's Shield	|5
	 * Artifact - Equipment
	 * All damage that would be dealt to you is dealt to equipped creature instead.
	 * Equip |3 */

	if( is_equipping(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		pariah_effect(player, -1, event, instance->damage_target_player, instance->damage_target_card);
	}

	return basic_equipment(player, card, event, 3);
}

int card_putrefy(int player, int card, event_t event){
	/*
	  Putrefy |1|B|G
	  Instant
	  Destroy target artifact or creature. It can't be regenerated.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target target creature or artifact.", 1, NULL);
}

int card_phytohydra(int player, int card, event_t event){
/* CARD_ID_PHYTOHYDRA	7924
Phytohydra	|2|G|W|W
Creature - Plant Hydra 1/1
If damage would be dealt to ~, put that many +1/+1 counters on it instead. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *damage = damage_being_prevented(event);
		if( damage ){
			if( damage->damage_target_player == player && damage->damage_target_card == card && damage->info_slot > 0 ){
				if( check_for_ability(damage->damage_source_player, damage->damage_source_card, KEYWORD_TRAMPLE) ){
					// This is a bit hackish but otherwise it won't work as it should.
					int total = damage->info_slot;
					int amount = get_toughness(player, card);
					total -= amount;
					prevent_damage(affected_card_controller, affected_card, amount);
					add_1_1_counters(player, card, amount);
					damage->info_slot = 0;
					damage_player(damage->damage_target_player, total, damage->damage_source_player, damage->damage_source_card);
				}
				else{
					int amount = damage->info_slot;
					prevent_damage(affected_card_controller, affected_card, amount);
					add_1_1_counters(player, card, amount);
				}
			}
		}
	}

 return 0;
}

int card_plague_boiler(int player, int card, event_t event){

	/* Plague Boiler	|3
	 * Artifact
	 * At the beginning of your upkeep, put a plague counter on ~.
	 * |1|B|G: Put a plague counter on ~ or remove a plague counter from it.
	 * When ~ has three or more plague counters on it, sacrifice it. If you do, destroy all nonland permanents. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counter(player, card, COUNTER_PLAGUE);
	}

	if( event == EVENT_STATIC_EFFECTS && ! is_humiliated(player, card) && count_counters(player, card, COUNTER_PLAGUE) >= 3 ){
		kill_card(player, card, KILL_SACRIFICE);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.type_flag = DOESNT_MATCH;
		APNAP(p, { new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = 0;
		if( count_counters(player, card, COUNTER_PLAGUE) > 0 ){
			choice = do_dialog(player, player, card, -1, -1, " Add a counter\n Remove a counter", 0);
		}
		if( choice == 0 ){
			add_counter(player, card, COUNTER_PLAGUE);
		}
		else{
			remove_counter(player, card, COUNTER_PLAGUE);
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XBG(1,1,1), 0, NULL, NULL);
}

int card_pollenbright_wings(int player, int card, event_t event)
{
  /* Pollenbright Wings	|4|G|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has flying.
   * Whenever enchanted creature deals combat damage to a player, put that many 1/1 |Sgreen Saproling creature tokens onto the battlefield. */

  if (attached_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_REPORT_DAMAGE_DEALT))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int saprolings = instance->targets[16].player;
	  instance->targets[16].player = 0;
	  generate_tokens_by_id(player, card, CARD_ID_SAPROLING, saprolings);
	}

  return generic_aura(player, card, event, player, 0, 0, KEYWORD_FLYING, 0, 0, 0, 0);
}

int card_primordial_sage(int player, int card, event_t event){
/* CARD_ID_PRIMORDIAL_SAGE	7927
Primordial Sage	|4|G|G
Creature - Spirit 4/5
Whenever you cast a creature spell, you may draw a card. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_privileged_position(int player, int card, event_t event){
/* CARD_ID_PRIVILEGED_POSITION	7928
Privileged Position	|2|GW|GW|GW
Enchantment
(|GW can be paid with either |G or |W.)
Other permanents you control have hexproof. (They can't be the targets of spells or abilities your opponents control.) */

	hybrid(player, card, event);

	if (event == EVENT_ABILITIES
		&& affected_card_controller == player
		&& affected_card != card
		&& in_play(player, card)
		&& ! is_humiliated(player, card)
		&& in_play(affected_card_controller, affected_card)
		&& is_what(affected_card_controller, affected_card, TYPE_PERMANENT) )
	{
		hexproof(affected_card_controller, affected_card, event);
	}

	return global_enchantment(player, card, event);
}

int card_psychic_drain(int player, int card, event_t event){
/* CARD_ID_PSYCHIC_DRAIN	7929
Psychic Drain	|X|U|B
Sorcery
Target player puts the top X cards of his or her library into his or her graveyard and you gain X life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			int mod = life[player] < 6 ? 8 : 2;
			ai_modifier += (instance->info_slot * mod);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			mill(instance->targets[0].player, instance->info_slot);
			gain_life(player, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}


	return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_rain_of_embers(int player, int card, event_t event){
/* CARD_ID_RAIN_OF_EMBERS	7931
Rain of Embers	|1|R
Sorcery
~ deals 1 damage to each creature and each player. */

	if( event == EVENT_RESOLVE_SPELL ){
		new_damage_all(player, card, ANYBODY, 1, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_rakdos_signet(int player, int card, event_t event){
/* CARD_ID_RAKDOS_SIGNET	1723
Rakdos Signet	|2
Artifact
|1, |T: Add |B|R to your mana pool. */

	// First printed in Dissension, but here to keep local to the other signets.
	return signet2(player, card, event, COLOR_BLACK, COLOR_RED);
}

int card_rally_the_righteous(int player, int card, event_t event){
/* CARD_ID_RALLY_THE_RIGHTEOUS	7932
Rally the Righteous	|1|R|W
Instant
Radiance - Untap target creature and each other creature that shares a color with it. Those creatures get +2/+0 until end of turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		card_instance_t *instance = get_card_instance( player, card );
		if( valid_target(&td) ){
			radiant_pump(player, card, instance->targets[0].player, instance->targets[0].card, 2, 0, 0, 0, ACT_UNTAP);
		}
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 0, 0, 0);
}

int card_razias_purification(int player, int card, event_t event){
/* CARD_ID_RAZIAS_PURIFICATION	7933
Razia's Purification	|4|R|W
Sorcery
Each player chooses three permanents he or she controls, then sacrifices the rest.*/

	if( event == EVENT_RESOLVE_SPELL ){
		int marked[2][151] = {{0}};
		APNAP(p,{
					if( can_cause_sacrifice(player, p) ){
						target_definition_t td;
						default_target_definition(player, card, &td, TYPE_PERMANENT);
						td.allowed_controller = td.preferred_controller = p;
						td.illegal_abilities = 0;
						td.who_chooses = p;
						td.allow_cancel = 0;

						card_instance_t *instance = get_card_instance( player, card );

						int selected = 0;
						while( selected < 3 && can_target(&td) ){
								char msg[100];
								scnprintf(msg, 100, "Select a permanent you control to keep (%d of 3).", selected + 1);
								if( new_pick_target(&td, msg, selected, GS_LITERAL_PROMPT) ){
									state_untargettable(instance->targets[selected].player, instance->targets[selected].card, 1);
									marked[p][instance->targets[selected].card] = 1;
									selected++;
								}
						}
						instance->number_of_targets = 0;
					}
				}
		);
		APNAP(p,{
					if( can_cause_sacrifice(player, p) ){
						int c;
						for(c = active_cards_count[p]-1; c > -1; c--){
							if( in_play(p, c) && is_what(p, c, TYPE_PERMANENT) ){
								if( marked[p][c] ){
									state_untargettable(p, c, 0);
								}
								else{
									kill_card(p, c, KILL_SACRIFICE);
								}
							}
						}
					}
				}
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static const char* damage_dealt_to_a_creature_you_control(int who_chooses, int player, int card)
{
	card_instance_t* instance = get_card_instance(player, card);
	if( instance->internal_card_id == damage_card && instance->damage_target_player == who_chooses &&
		instance->damage_target_card > -1 && is_what(instance->damage_target_player, instance->damage_target_card, TYPE_CREATURE) )
	{
		return NULL;
	}
	return "must be damage dealt to a creature you control.";
}

int card_razia_boros_archangel(int player, int card, event_t event){
/*
Razia, Boros Archangel |4|W|W|R|R
Legendary Creature — Angel 6/3
Flying, vigilance, haste
{T}: The next 3 damage that would be dealt to target creature you control this turn is dealt to another target creature instead.
*/
	check_legend_rule(player, card, event);

	vigilance(player, card, event);

	haste(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int)damage_dealt_to_a_creature_you_control;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_target(&td2) ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_LITERAL_PROMPT | GAA_CAN_TARGET, MANACOST0, 0,
								&td, "Select target damage dealt to a creature you control.");
		if( spell_fizzled != 1 ){
			card_instance_t *dmg = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			state_untargettable(dmg->damage_target_player, dmg->damage_target_card, 1);
			if( can_target(&td2) && new_pick_target(&td2, "TARGET_CREATURE", 1, 0) ){
				add_state(player, card, STATE_TAPPED);
				int amount = MIN(3, dmg->info_slot);
				if( instance->targets[1].player == player ){
					if( get_toughness(instance->targets[1].player, instance->targets[1].card) < amount ){
						ai_modifier += 24;
					}
					else{
						ai_modifier -= 48;
					}
				}
				if( instance->targets[1].player == 1-player ){
					if( get_toughness(instance->targets[1].player, instance->targets[1].card) <= amount ){
						ai_modifier += 48;
					}
					else{
						ai_modifier += 24;
					}
				}
			}
			else{
				spell_fizzled = 1;
			}
			state_untargettable(dmg->damage_target_player, dmg->damage_target_card, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td2, 1) ){
			card_instance_t *dmg = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			prevent_the_next_n_damage(instance->parent_controller, instance->parent_card,
									  dmg->damage_target_player, dmg->damage_target_card,
									  3, PREVENT_REDIRECT, instance->targets[1].player, instance->targets[1].card);
		}
	}

	return 0;
}

int card_remand(int player, int card, event_t event){
	/*
	  Remand |1|U
	  Instant
	  Counter target spell. If that spell is countered this way, put it into its owner's hand instead of into that player's graveyard.
	  Draw a card.
	*/

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			if( manage_counterspell_linked_hacks(player, card, instance->targets[0].player, instance->targets[0].card) != KILL_EXILE ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
			else{
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			}
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

int card_ribbons_of_night(int player, int card, event_t event){
/* CARD_ID_RIBBONS_OF_NIGHT	7936
Ribbons of Night	|4|B
Sorcery
~ deals 4 damage to target creature and you gain 4 life. If |U was spent to cast ~, draw a card. */

	ravnica_manachanger(player, card, event, 0, 1, 0, 0, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->targets[1].player = 1;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1){
			instance->targets[1].player = charge_changed_mana(player, card, 0, 1, 0, 0, 0);
		}
		if( spell_fizzled != 1 ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
			gain_life(player, 4);
			if( instance->targets[1].player == 2 ){
				draw_cards(player, 1);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_rolling_spoil(int player, int card, event_t event){
/* CARD_ID_ROLLING_SPOIL	7937
Rolling Spoil	|2|G|G
Sorcery
Destroy target land. If |B was spent to cast ~, all creatures get -1/-1 until end of turn. */

	ravnica_manachanger(player, card, event, 1, 0, 0, 0, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->targets[1].player = 1;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1){
			instance->targets[1].player = charge_changed_mana(player, card, 1, 0, 0, 0, 0);
		}
		if( spell_fizzled != 1 ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if( instance->targets[1].player  == 2 ){
				pump_subtype_until_eot(player, card, 2, -1, -1, -1, 0, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_sandsower(int player, int card, event_t event){
	/* Sandsower	|3|W
	 * Creature - Spirit 1/3
	 * Tap three untapped creatures you control: Tap target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_abilities = 0;
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (current_turn == 1-player && current_phase >= PHASE_MAIN2 ? 48 : -24);
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td2) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td2, "TARGET_CREATURE",
															&td, 3);
}

static void savra_trigger(int player, int card){
	card_instance_t *instance = get_card_instance( player, card );
	int cpl = can_pay_life(player, 2);
	/* Store previous triggers and clear them, in case the sacrifice this imposes causes another sacrifice of a black/green creature -
	 * consider Abyssal Gatekeeper */
	int i;
	int colors[20];
	int cc = 0;
	for(i=0; i<10; i++){
		int result = instance->targets[i].player;
		if( result == -1 ){
			break;
		}
		colors[cc] = result;
		cc++;
		instance->targets[i].player = -1;
		result = instance->targets[i].card;
		if( result == -1 ){
			break;
		}
		colors[cc] = result;
		cc++;
		instance->targets[i].card = -1;
	}
	for(i=0; i<cc; i++){
		int abils[5] = {0,
						((colors[i] & 1) && cpl) ? 1 : 0,
						(colors[i] & 2),
						((colors[i] & 1) && cpl) && (colors[i] & 2) ? 1 : 0,
						1
		};
		int priorities[5] = {
							0,
							(life[player]-2 > 6 || (colors[i] & 2)) && can_cause_sacrifice(player, 1-player) ? 10 : 1,
							10,
							priorities[1] == 10 ? 25 : 1,
							1
		};
		int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
							"Pay 2 life and make opp. sacrifice", abils[1], priorities[1],
							"Gain 2 life", abils[2], priorities[2],
							"Do both", abils[3], priorities[3],
							"Do nothing", abils[4], priorities[4]);
		if( choice & 1 ){
			lose_life(player, 2);
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &test);
		}
		if( choice & 2 ){
			gain_life(player, 2);
		}
	}
}

static void count_for_savra(int player, int card, int sl_color1, int sl_color2){
	card_instance_t *instance = get_card_instance( player, card );
	card_instance_t* affected = in_play(affected_card_controller, affected_card);
	if( affected && affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
		affected->kill_code == KILL_SACRIFICE &&
		!check_special_flags(affected_card_controller, affected_card, SF_KILL_STATE_BASED_ACTION) )
	{
		int colors = get_color(affected_card_controller, affected_card);
		int black = sl_color1;
		int green = sl_color2;
		if (!(colors & (black | green))){
			return;
		}
		if( instance->targets[11].player < 0 ){
			instance->targets[11].player = 0;
		}
		int pos = BYTE0(instance->targets[11].player);
		int result = (colors & black) ? 1 : 0;;
		result |= (colors & green) ? 2 : 0;
		while( pos < 10 ){
				if( instance->targets[pos].player == -1 ){
					instance->targets[pos].player = result;
					break;
				}
				if( instance->targets[pos].card == -1 ){
					instance->targets[pos].card = result;
					break;
				}
				pos++;
		}
		SET_BYTE0(instance->targets[11].player) = pos;
	}
}

static int savra_queen_of_the_golgari_effect(int player, int card, event_t event){

	//In case something is copying Pawn of Ulamog and then reverts to its original form.
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_SAVRA_QUEEN_OF_THE_GOLGARI ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(affected_card_controller, affected_card) ){
			int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
			if( kill_code ){
				int c1 = COLOR_TEST_BLACK;
				int c2 = COLOR_TEST_GREEN;
				int p = instance->damage_target_player;
				int c = instance->damage_target_card;
				if( p > -1 ){
					c1  = get_sleighted_color_test(p, c, COLOR_TEST_BLACK);
					c2  = get_sleighted_color_test(p, c, COLOR_TEST_GREEN);
					if( affect_me(p, c) ){
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
						if( instance->targets[11].player < 0 ){
							instance->targets[11].player = 0;
						}
						SET_BYTE1(instance->targets[11].player) = 1;
					}
				}
				count_for_savra(player, card, c1, c2);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			savra_trigger(player, card);
			kill_card(player, card, KILL_EXILE);
		}
	}

	//Reset counts if the legacy is still invisible
	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && event == EVENT_END_TRIGGER ){
		get_card_instance(player, card)->targets[11].player = 0;
	}

	if( event == EVENT_CLEANUP && ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_savra_queen_of_the_golgari(int player, int card, event_t event){
/*
Savra, Queen of the Golgari |2|B|G
Legendary Creature — Elf Shaman 2/2
Whenever you sacrifice a black creature, you may pay 2 life. If you do, each other player sacrifices a creature.
Whenever you sacrifice a green creature, you may gain 2 life.
*/
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)savra_queen_of_the_golgari_effect ){
					if( (inst->damage_target_player == player && inst->damage_target_card == card) ||
						(inst->targets[0].player == player && inst->targets[0].card == card) )
					{
						found = 1;
						break;
					}
				}
			}
		}
		if( ! found ){
			int legacy = create_targetted_legacy_effect(player, card, &savra_queen_of_the_golgari_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	check_legend_rule(player, card, event);

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_savra(player, card, get_sleighted_color_test(player, card, COLOR_TEST_BLACK),
						get_sleighted_color_test(player, card, COLOR_TEST_GREEN));
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
			savra_trigger(player, card);
		}
	}

	return 0;
}

int card_scatter_the_seeds(int player, int card, event_t event){

	/* Scatter the Seeds	|3|G|G
	 * Instant
	 * Convoke
	 * Put three 1/1 |Sgreen Saproling creature tokens onto the battlefield. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, 3);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell_with_convoke(player, card, event);
}

int card_scion_of_the_wild(int player, int card, event_t event)
{
  /* Crusader of Odric	|2|W
   * Creature - Human Soldier 100/100
   * ~'s power and toughness are each equal to the number of creatures you control. */
  /* Scion of the Wild	|1|G|G
   * Creature - Avatar 100/100
   * ~'s power and toughness are each equal to the number of creatures you control. */
  /* Battle Squadron */

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && !is_humiliated(player, card) && player != -1)
	event_result += count_subtype(player, TYPE_CREATURE, -1);

  return 0;
}

// searing meditation is a placeholder, the actual one is in "gain_life" in "functions.c"

int card_seed_spark(int player, int card, event_t event){
	/* Seed Spark	|3|W
	 * Instant
	 * Destroy target artifact or enchantment. If |G was spent to cast ~, put two 1/1 |Sgreen Saproling creature tokens onto the battlefield. */

	ravnica_manachanger(player, card, event, 0, 0, 1, 0, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->targets[1].player = 1;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1){
			instance->targets[1].player = charge_changed_mana(player, card, 0, 0, 1, 0, 0);
		}
		if( spell_fizzled != 1 ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if( instance->targets[1].player == 2 ){
				generate_tokens_by_id(player, card, CARD_ID_SAPROLING, 2);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_seeds_of_strength(int player, int card, event_t event){
/* CARD_ID_SEEDS_OF_STRENGTH	7949
Seeds of Strength	|G|W
Instant
Target creature gets +1/+1 until end of turn.
Target creature gets +1/+1 until end of turn.
Target creature gets +1/+1 until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int i;
		for(i=0; i<3; i++){
			if( ! new_pick_target(&td, "TARGET_CREATURE", i, 1) ){
				add_state(instance->targets[i].player, instance->targets[i].card, STATE_TARGETTED);
			}
			else{
				break;
			}
		}
		for(i=0; i<instance->number_of_targets; i++){
			remove_state(instance->targets[i].player, instance->targets[i].card, STATE_TARGETTED);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<3; i++){
			if( validate_target(player, card, &td, i) ){
				pump_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 1, 1);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_seismic_spike(int player, int card, event_t event){
/* CARD_ID_SEISMIC_SPIKE	7950
Seismic Spike	|2|R|R
Sorcery
Destroy target land. Add |R|R to your mana pool. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			produce_mana(player, COLOR_RED, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
}

int card_selesnya_evangel(int player, int card, event_t event){
	/* Selesnya Evangel	|G|W	0x200f552
	 * Creature - Elf Shaman 1/2
	 * |1, |T, Tap an untapped creature you control: Put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller = player;
	td1.preferred_controller = player;
	td1.illegal_state = TARGET_STATE_TAPPED;
	td1.illegal_abilities = 0;
	td1.special = TARGET_SPECIAL_NOT_ME;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (current_turn != player ? 48 : 24);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL, &td1, 1);
}

int card_selesnya_guildmage(int player, int card, event_t event){
	/* Selesnya Guildmage	|GW|GW
	 * Creature - Elf Wizard 2/2
	 * |3|G: Put a 1/1 |Sgreen Saproling creature token onto the battlefield.
	 * |3|W: Creatures you control get +1/+1 until end of turn. */

	hybrid(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_SAPROLING = 1,
		CHOICE_PUMP
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_XG(3, 1), 0, NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_XW(3, 1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XG(3, 1), 0, NULL, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XW(3, 1), 0, NULL, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Create a Saproling", 	abils[1], current_phase < PHASE_DECLARE_BLOCKERS ? 10 : 1,
							"Pump your guys", 		abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_SAPROLING ){
			ai_modifier += (current_turn == 1-player ? 48 : 24);
			return generic_activated_ability(player, card, event, 0, MANACOST_XG(3, 1), 0, NULL, NULL);
		}
		if( choice == CHOICE_PUMP ){
			return generic_activated_ability(player, card, event, 0, MANACOST_XW(3, 1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAPROLING ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
		}
		if( instance->info_slot == CHOICE_PUMP ){
			pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1, 1, 1, 0, 0);
		}
	}

	return 0;
}

int card_selesnya_signet(int player, int card, event_t event){
/* CARD_ID_SELESNYA_SIGNET	1716
Selesnya Signet	|2
Artifact
|1, |T: Add |G|W to your mana pool. */

	return signet2(player, card, event, COLOR_WHITE, COLOR_GREEN);
}

int card_shadow_of_doubt(int player, int card, event_t event){
/* CARD_ID_SHADOW_OF_DOUBT	7956
Shadow of Doubt	|UB|UB
Instant
(|UB can be paid with either |U or |B.)
Players can't search libraries this turn.
Draw a card. */

	hybrid(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 1);
		create_id_legacy(player, card, -1, -1, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_shambling_shell(int player, int card, event_t event){
	/* Shambling Shell	|1|B|G
	 * Creature - Plant Zombie 3/1
	 * Sacrifice ~: Put a +1/+1 counter on target creature.
	 * Dredge 3 */

	if( event == EVENT_DREDGE ){
		return 3;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_SACRIFICE_ME, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_shred_memory(int player, int card, event_t event){
/* CARD_ID_SHRED_MEMORY	7958
Shred Memory	|1|B
Instant
Exile up to four target cards from a single graveyard.
Transmute |1|B|B (|1|B|B, Discard this card: Search your library for a card with the same converted mana cost as this card,
reveal it, and put it into your hand. Then shuffle your library. Transmute only as a sorcery.) */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		return transmute(player, card, event, MANACOST_XB(1, 2), 2);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select target card to exile.");

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int t_player = 1-player;
		if( count_graveyard(1-player) && ! graveyard_has_shroud(1-player) ){
			if( count_graveyard(player) && ! graveyard_has_shroud(player) ){
				target_definition_t td;
				default_target_definition(player, card, &td, 0);
				td.zone = TARGET_ZONE_PLAYERS;
				new_pick_target(&td, "Select a graveayrd owner.", 0, 1 | GS_LITERAL_PROMPT);
				if( spell_fizzled == 1 ){
					return 0;
				}
				t_player = instance->targets[0].player;
				instance->number_of_targets = 0;
			}
		}
		else{
			t_player = player;
		}
		select_multiple_cards_from_graveyard(player, t_player, 0, player == t_player ? AI_MIN_VALUE : AI_MAX_VALUE, NULL, 4,
											&instance->targets[1]);
		return 0;
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int i;
		for (i = 1; i <= 4; ++i){
			int selected = validate_target_from_grave(player, card, instance->targets[0].player, i);
			if (selected != -1){
				rfg_card_from_grave(instance->targets[0].player, selected);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &test);
}

int card_simic_signet(int player, int card, event_t event){
/* CARD_ID_SIMIC_SIGNET	1724
Simic Signet	|2
Artifact
|1, |T: Add |G|U to your mana pool. */

	// First printed in Dissension, but here to keep local to the other signets.
	return signet2(player, card, event, COLOR_BLUE, COLOR_GREEN);
}

int card_sins_of_the_past(int player, int card, event_t event){
/* CARD_ID_SINS_OF_THE_PAST	7960
Sins of the Past	|4|B|B
Sorcery
Until end of turn, you may cast target instant or sorcery card from your graveyard without paying its mana cost.
If that card would be put into your graveyard this turn, exile it instead. Exile ~. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_SPELL, "Select target sorcery or instant card.");
	this_test.ai_selection_mode = AI_MAX_CMC;

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave(player, card, player, 0);
		if( selected != -1 ){
			create_spell_has_flashback_legacy(player, card, selected, FBL_NO_MANA_COST);
		}
		kill_card(player, card, KILL_EXILE);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_OPP_GRAVE, NULL, NULL, 1, &this_test);
}

static const char* is_blocking_ssd(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  if (is_blocking(card, targeting_card))
	return NULL;
  else
	return "must be blocking Sisters of the Stone Death";
}

static const char* ssd_is_blocking_him(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  if (is_blocking(targeting_card, card))
	return NULL;
  else
	return "must be blocked by Sisters of the Stone Death";
}

int card_sisters_of_stone_death(int player, int card, event_t event){
/* CARD_ID_SISTERS_OF_STONE_DEATH	7961
Sisters of Stone Death	|4|B|B|G|G
Legendary Creature - Gorgon 7/5
|G: Target creature blocks ~ this turn if able.
|B|G: Exile target creature blocking or blocked by ~.
|2|B: Put a creature card exiled with ~ onto the battlefield under your control. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = 1-player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.allowed_controller =  1-player;
	td2.preferred_controller = 1-player;
	td2.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td2.extra = (int32_t)is_blocking_ssd;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller =  1-player;
	td1.preferred_controller = 1-player;
	td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td1.extra = (int32_t)ssd_is_blocking_him;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( current_phase < PHASE_DECLARE_BLOCKERS && generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, "TARGET_CREATURE") ){
			return 1;
		}
		if( current_turn == player ){
			if( current_phase == PHASE_AFTER_BLOCKING && generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_BG(1, 1), 0, &td2, "TARGET_CREATURE") ){
				return 1;
			}
		}
		if( current_turn != player ){
			if( current_phase == PHASE_AFTER_BLOCKING && generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_BG(1, 1), 0, &td1, "TARGET_CREATURE") ){
				return 1;
			}
		}
		if( generic_activated_ability(player, card, event, 0, MANACOST_XB(2, 1), 0, NULL, NULL) &&
			exiledby_choose(player, card, CARD_ID_SISTERS_OF_STONE_DEATH, EXBY_MAX_VALUE, 0, "creature", 1)
		  ){
			return 1;
		}
	}

	if (event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION){

		int can_force_block = current_phase < PHASE_DECLARE_BLOCKERS && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, "TARGET_CREATURE");
		int can_exile_blocked_blocking_creature = 0;
		if( current_turn == player ){
			can_exile_blocked_blocking_creature = (current_phase == PHASE_AFTER_BLOCKING && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_BG(1, 1), 0, &td2, "TARGET_CREATURE"));
		}
		else{
			can_exile_blocked_blocking_creature = (current_phase == PHASE_AFTER_BLOCKING && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_BG(1, 1), 0, &td1, "TARGET_CREATURE"));
		}
		int can_return_exiled_creature = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XB(2, 1), 0, NULL, NULL) && exiledby_choose(player, card, CARD_ID_SISTERS_OF_STONE_DEATH, EXBY_MAX_VALUE, 0, "creature", 1);

		int priority_force_block = current_phase < PHASE_DECLARE_BLOCKERS && can_force_block ? 10 : 0;
		int priority_exile_blocked_blocking_creature = current_phase == PHASE_AFTER_BLOCKING && can_exile_blocked_blocking_creature ? 15 : 0;
		int priority_return_exiled_creature = current_phase < PHASE_DECLARE_BLOCKERS && can_return_exiled_creature ? 20 : 0;
		enum{
			CHOICE_FBLOCK = 1,
			CHOICE_EXILE_BLOCK = 2,
			CHOICE_RETURN_C = 3,
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM,
						"Force a creature to block", can_force_block, priority_force_block, DLG_MANA(MANACOST_G(1)),
						"Exile a creature blocking / blocked by me", can_exile_blocked_blocking_creature, priority_exile_blocked_blocking_creature, DLG_MANA(MANACOST_BG(1, 1)),
						"Return an exiled creature", can_return_exiled_creature, priority_return_exiled_creature, DLG_MANA(MANACOST_XB(2, 1))
						);

		if (event == EVENT_ACTIVATE){
				switch (choice){
					case CHOICE_FBLOCK:
					{
						if( ! select_target(player, card, &td, "Select a creature that will be forced to block this", &(instance->targets[0])) ){
							spell_fizzled = 1;
							return 0;
						}
						break;

					}
					case CHOICE_EXILE_BLOCK:
					{
						if( current_turn == player ){
							if( ! select_target(player, card, &td2, "Select a creature that is blocking Sisters of the Stone Death", &(instance->targets[0])) ){
								spell_fizzled = 1;
								return 0;
							}
						}
						else{
							if( ! select_target(player, card, &td1, "Select a creature that is blocked by Sisters of the Stone Death", &(instance->targets[0])) ){
								spell_fizzled = 1;
								return 0;
							}
						}
						break;
					}
					case CHOICE_RETURN_C:
					{
						int rval = exiledby_choose(player, card, CARD_ID_SISTERS_OF_STONE_DEATH, EXBY_CHOOSE, 0, "creature", 1);
						int* loc = (int*)rval;
						if (!loc)
						  spell_fizzled = 1;
						else
						  {
							instance->targets[1].player = -1;
							instance->targets[1].card = *loc;
						  }
							break;
					}

				}
		}
		else	// event == EVENT_RESOLVE_ACTIVATION
			{
			  switch (choice)
				{
				  case CHOICE_FBLOCK:
					{
						if( valid_target(&td) ){
							target_must_block_me(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1);
						}
						break;
					}

				  case CHOICE_EXILE_BLOCK:
					{
						if( (current_turn == player && valid_target(&td2)) || (current_turn != player && valid_target(&td1)) ){
							exile_card_and_remember_it_on_exiledby(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
						}
						break;
					}
				  case CHOICE_RETURN_C:
					{
					  int* loc = exiledby_find(player, instance->parent_card, instance->targets[1].card, NULL, NULL);
					  if (!loc)
						spell_fizzled = 1;
					  else
						{
						  *loc = -1;
						  int owner = (instance->targets[1].card & 0x80000000) ? 1 : 0;
						  int iid = instance->targets[1].card & ~0x80000000;
						  if (remove_card_from_rfg(owner, cards_data[iid].id))
							{
							  int nightmare = add_card_to_hand(player, iid);
							  if (player != owner)
								get_card_instance(player, nightmare)->state ^= STATE_OWNED_BY_OPPONENT;
							  create_targetted_legacy_effect(player, instance->parent_card, empty, player, nightmare);
							  put_into_play(player, nightmare);
							}
						  else
							spell_fizzled = 1;
						}
						break;
					}

				}
		}
	}
	return 0;
}

int card_smash2(int player, int card, event_t event){
/* CARD_ID_SMASH	5433
Smash	|2|R
Instant
Destroy target artifact.
Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			draw_cards(player, 1);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
}

int card_sparkmage_apprentice(int player, int card, event_t event){
	/* Akoum Boulderfoot	|4|R|R	0x2006a79
	 * Creature - Giant Warrior 4/5
	 * When ~ enters the battlefield, it deals 1 damage to any target. */
	/* Sparkmage Apprentice	|1|R	0x2006a79
	 * Creature - Human Wizard 1/1
	 * When ~ enters the battlefield, it deals 1 damage to any target. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0) ){
			damage_target0(player, card, 1);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_spawnbroker(int player, int card, event_t event){
/*
Spawnbroker |2|U
Creature — Human Wizard 1/1, 2U (3)
When Spawnbroker enters the battlefield, you may exchange control of target creature you control and target creature with power
	less than or equal to that creature's power an opponent controls.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			target_definition_t td1;
			default_target_definition(player, card, &td1, TYPE_CREATURE);
			td1.allowed_controller = 1-player;
			td1.preferred_controller = 1-player;
			td1.power_requirement = get_power(instance->targets[0].player, instance->targets[0].card) | TARGET_PT_LESSER_OR_EQUAL;
			if( can_target(&td1) && new_pick_target(&td1, "TARGET_CREATURE", 1, 0) ){
				exchange_control_of_target_permanents(player, card, instance->targets[0].player, instance->targets[0].card,
														instance->targets[1].player, instance->targets[1].card);
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_stinkweed_imp(int player, int card, event_t event){
/* CARD_ID_STINKWEED_IMP	7967
Stinkweed Imp	|2|B
Creature - Imp 1/2
Flying
Whenever ~ deals combat damage to a creature, destroy that creature.
Dredge 5 (If you would draw a card, instead you may put exactly five cards from the top of your library into your graveyard.
If you do, return this card from your graveyard to your hand. Otherwise, draw a card.) */

	if( event == EVENT_DREDGE ){
		return 5;
	}
	deathtouch(player, card, event);
	return 0;
}

int card_stoneseeder_hierophant(int player, int card, event_t event){
/*
Stone-Seeder Hierophant |2|G|G
Creature — Human Druid 1/1
Whenever a land enters the battlefield under your control, untap Stone-Seeder Hierophant.
{T}: Untap target land.
*/
	etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_LAND, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		untap_card(player, card);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_LAND");
		if( spell_fizzled != 1 ){
			ai_modifier_for_land_untappers(player, card, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_LAND");
}

int card_stoneshaker_shaman(int player, int card, event_t event){
/*
Stoneshaker Shaman |2|R
Creature — Human Shaman 1/1
At the beginning of each player's end step, that player sacrifices an untapped land.
*/
	if( eot_trigger(player, card, event) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select an untapped land to sacrifice.");
		test.state = STATE_TAPPED;
		test.state_flag = DOESNT_MATCH;

		new_sacrifice(player, card, current_turn, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	return 0;
}

int card_strands_of_undeath(int player, int card, event_t event)
{
/* CARD_ID_STRANDS_OF_UNDEATH	7970
Strands of Undeath	|3|B
Enchantment - Aura
Enchant creature
When ~ enters the battlefield, target player discards two cards.
|B: Regenerate enchanted creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
	{
		target_definition_t td1;
		default_target_definition(player, card, &td1, 0);
		td1.zone = TARGET_ZONE_PLAYERS;
		td1.allow_cancel = 0;

		if (can_target(&td1) && new_pick_target(&td1, "TARGET_PLAYER", 1, 1))
			new_multidiscard(get_card_instance(player, card)->targets[1].player, 2, 0, player);
	}

	// |B: Regenerate enchanted creature.
	if( get_card_instance(player, card)->damage_target_player > -1 && ! is_humiliated(player, card) && IS_GAA_EVENT(event) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( can_be_regenerated(p, c) ){
				regenerate_target(p, c);
			}
		}
		return granted_generic_activated_ability(player, card, p, c, event, GAA_REGENERATION, MANACOST_B(1), 0, NULL, NULL);
	}

	return vanilla_aura(player, card, event, player);
}

int card_sundering_vitae(int player, int card, event_t event){
/* CARD_ID_SUNDERING_VITAE	7971
Sundering Vitae	|2|G
Instant
Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for |1 or one mana of that creature's color.)
Destroy target artifact or enchantment. */

	if( event == EVENT_MODIFY_COST){
		return generic_spell_with_convoke(player, card, event);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 0, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int result = cast_spell_with_convoke(player, card, event);
		if( ! result ){
			spell_fizzled = 1;
			return 0;
		}
		td.allow_cancel = result & (1<<31) ? 0 : 1;
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 0, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(player, 1);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_sunforger(int player, int card, event_t event){
/* Sunforger	|3
 * Artifact - Equipment
 * Equipped creature gets +4/+0.
 * |R|W, Unattach ~: Search your library for a |Sred or |Swhite instant card with converted mana cost 4 or less and cast that card without
 * 		paying its mana cost. Then shuffle your library.
 * Equip |3 */

	if( is_equipping(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_POWER && affect_me(p, c) ){
			event_result += 4;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_EQUIP = 1,
		CHOICE_FREE_INSTANT
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( is_equipping(player, card) && generic_activated_ability(player, card, event, 0, MANACOST_RW(1, 1), 0, NULL, NULL) ){
			return 1;
		}
		return basic_equipment(player, card, event, 3);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						can_activate_basic_equipment(player, card, event, 3),
						is_equipping(player, card) &&
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_RW(1, 1), 0, NULL, NULL) ? 1 : 0
		};
		int priority_free_spell = 5;
		if( is_equipping(player, card) ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			if( current_turn == 1-player && ((is_tapped(p, c) && ! is_attacking(player, card)) || current_phase >= PHASE_MAIN2) ){
				priority_free_spell = 10;
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Equip", abils[1], 5,
							"Play a free instant", abils[2], priority_free_spell);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 3);
		}
		if( choice == CHOICE_FREE_INSTANT ){
			generic_activated_ability(player, card, event, 0, MANACOST_RW(1, 1), 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				ai_modifier += (current_turn == 1-player ? 48 : 12);
				unattach(player, card);
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 3);
		}
		if( instance->info_slot == CHOICE_FREE_INSTANT ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_INSTANT);
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED | COLOR_TEST_WHITE);
			this_test.cmc = 5;
			this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
			int result = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MAX_VALUE, -1, &this_test);
			if (result == -1){
				spell_fizzled = 1;
			}
			else{
				play_card_in_deck_for_free(player, player, result);
			}
		}
	}

	return 0;
}

int card_sunhome_enforcer(int player, int card, event_t event){
/* CARD_ID_SUNHOME_ENFORCER	7973
Sunhome Enforcer	|2|R|W
Creature - Giant Soldier 2/4
Whenever ~ deals combat damage, you gain that much life.
|1|R: ~ gets +1/+0 until end of turn. */

    check_damage_test(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_REPORT_DAMAGE_DEALT|DDBM_MUST_DAMAGE_PLANESWALKER, player, card, 0, NULL);

	if( resolve_damage_trigger(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_REPORT_DAMAGE_DEALT|DDBM_MUST_DAMAGE_PLANESWALKER, player, card) ){
		int amount = get_card_instance(player, card)->targets[16].player;
		get_card_instance(player, card)->targets[16].player = 0;

		gain_life(player, amount);
	}
	return generic_shade_merge_pt(player, card, event, 0, MANACOST_XR(1, 1), 1, 0);
}

int card_sunhome_fortress_of_the_legion(int player, int card, event_t event){
	/* Sunhome, Fortress of the Legion	""
	 * Land
	 * |T: Add |C to your mana pool.
	 * |2|R|W, |T: Target creature gains double strike until end of turn. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_DOUBLE_STRIKE,
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XRW(3, 1, 1), 0, &td, NULL) )
		{
			choice = CHOICE_DOUBLE_STRIKE;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) && choice == CHOICE_DOUBLE_STRIKE ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Give double strike", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_DOUBLE_STRIKE ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XRW(2, 1, 1), 0, &td, "TARGET_CREATURE");
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
				return 0;
			}
			if( current_turn == player ){
				if( is_attacking(instance->targets[0].player, instance->targets[0].card) ){
					ai_modifier += (is_unblocked(instance->targets[0].player, instance->targets[0].card) ? 48 : 24);
				}
			}
			else{
				if( is_blocking(instance->targets[0].player, instance->targets[0].card) ){
					ai_modifier += 48;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DOUBLE_STRIKE ){
			if( valid_target(&td) ){
				pump_ability_until_eot(instance->parent_controller, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_DOUBLE_STRIKE, 0);
			}
		}
	}

	return 0;
}

int card_suppression_field(int player, int card, event_t event){
/* CARD_ID_SUPPRESSION_FIELD	7975
Suppression Field	|1|W
Enchantment
Activated abilities cost |2 more to activate unless they're mana abilities. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		set_cost_mod_for_activated_abilities(2, -1, 3, &this_test);
	}

	if( event == EVENT_CAST_SPELL ){
		if( is_what(affected_card_controller, affected_card, TYPE_PERMANENT) ){
			set_cost_mod_for_activated_abilities(affected_card_controller, affected_card, 3, 0);
		}
	}


	if( leaves_play(player, card, event) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		remove_cost_mod_for_activated_abilities(2, -1, 3, &this_test);
	}

	return global_enchantment(player, card, event);
}

int card_surge_of_zeal(int player, int card, event_t event){
/* CARD_ID_SURGE_OF_ZEAL	7976
Surge of Zeal	|R
Instant
Radiance - Target creature and each other creature that shares a color with it gain haste until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier_for_haste_granting_effects(player, card);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			radiant_pump(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_HASTE, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_svoghtos_the_restless_tomb(int player, int card, event_t event){
/* CARD_ID_SVOGTHOS_THE_RESTLESS_TOMB	7978
Svogthos, the Restless Tomb	""
Land
|T: Add |C to your mana pool.
|3|B|G: Until end of turn, ~ becomes a |Sblack and |Sgreen Plant Zombie creature with
"This creature's power and toughness are each equal to the number of creature cards in your graveyard." It's still a land. */

	return manland_normal(player, card, event, MANACOST_XGB(3, 1, 1));
}

int card_svoghtos_animated(int player, int card, event_t event){
	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && player != -1){
		event_result += count_graveyard_by_type(player, TYPE_CREATURE);
	}
	if (card == -1){	// Not on bf.  Though I can't imagine how the animated version would be called in that case.
		return 0;
	}
	return manland_animated(player, card, event, MANACOST_XGB(3, 1, 1));
}

int card_szadeck_lord_of_secrets(int player, int card, event_t event){
/* CARD_ID_SZADEK_LORD_OF_SECRETS	7980
Szadek, Lord of Secrets	|3|U|U|B|B
Legendary Creature - Vampire 5/5
Flying
If ~ would deal combat damage to a player, instead put that many +1/+1 counters on ~ and that player
puts that many cards from the top of his or her library into his or her graveyard. */

	check_legend_rule(player, card, event);
	damage_effects(player, card, event);
	return 0;
}

int card_telling_time(int player, int card, event_t event){
/* CARD_ID_TELLING_TIME	7982
Telling Time	|1|U
Instant
Look at the top three cards of your library. Put one of those cards into your hand, one on top of your library,
and one on the bottom of your library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int total = get_total_available_lands(player);
		int mod = (total <= 3 || total >= 6 ? 24 : 12);
		mod += (current_turn == 1-player ? 24 : 12);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int amount = MIN(3, count_deck(player));
		if( amount > 0 ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to put on your hand.");
			this_test.create_minideck = amount;
			this_test.no_shuffle = 1;

			if( IS_AI(player) ){
				int total = get_total_available_lands(player);
				if( total <= 3 ){
					default_test_definition(&this_test, TYPE_LAND);
				}
				if( total >= 6 ){
					default_test_definition(&this_test, TYPE_LAND);
					this_test.type = DOESNT_MATCH;
				}
				this_test.create_minideck = amount;
				this_test.no_shuffle = 1;
			}
			int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 1, AI_MAX_VALUE, &this_test);
			if( result == -1 ){
				default_test_definition(&this_test, TYPE_ANY);
				this_test.create_minideck = amount;
				this_test.no_shuffle = 1;
				new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 1, AI_MAX_VALUE, &this_test);
			}
			amount--;
			if( amount ){
				put_top_x_on_bottom(player, player, 1);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_thoughtpicker_witch(int player, int card, event_t event){
/*
Thoughtpicker Witch |B
Creature — Human Wizard 1/1
{1}, Sacrifice a creature: Look at the top two cards of target opponent's library, then exile one of them.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (count_subtype(player, TYPE_LAND, -1) * 8);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && count_deck(instance->targets[0].player) > 0 ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card to exile.");
			if( IS_AI(player) ){
				int amount = count_subtype(1-player, TYPE_LAND, -1);
				if( amount <= 3 ){
					default_test_definition(&test, TYPE_LAND);
				}
				else{
					default_test_definition(&test, TYPE_LAND);
					test.type_flag = DOESNT_MATCH;
				}
			}
			test.create_minideck = MIN(2, count_deck(instance->targets[0].player));
			test.no_shuffle = 1;
			int selected = new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_RFG, 1, AI_MAX_VALUE, &test);
			if( selected == -1 ){
				test.type = TYPE_ANY;
				test.type_flag = MATCH;
				new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_RFG, 1, AI_MAX_VALUE, &test);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_PLAYER");
}

static int test_not_in_array3(int iid, int arrptr, int player, int card)
{
  int* arr = (int*)arrptr;
  return iid != arr[0] && iid != arr[1] && iid != arr[2];
}

int card_three_dreams(int player, int card, event_t event){
/* Three Dreams	|4|W
 * Sorcery
 * Search your library for up to three Aura cards with different names, reveal them, and put them into your hand. Then shuffle your library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 48;
	}

	if (event == EVENT_RESOLVE_SPELL ){
		int chosen[3] = {-1, -1, -1};
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select up to three Aura cards with different names.");
		test.subtype = SUBTYPE_AURA;
		test.value_for_special_selection_function = (int)chosen;
		test.special_selection_function = test_not_in_array3;
		test.qty = 3;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_thundersong_trumpeter(int player, int card, event_t event){
/* CARD_ID_THUNDERSONG_TRUMPETER	7987
Thundersong Trumpeter	|R|W
Creature - Human Soldier 2/1
|T: Target creature can't attack or block this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		if( (current_turn == player && count_subtype(player, TYPE_CREATURE, -1) > 1) || current_turn != player ){
			ai_modifier += 48;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int leg = pump_ability_until_eot(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
			if (leg >= 0){
				get_card_instance(instance->parent_controller, leg)->targets[3].player = PAUE_CANT_ATTACK;
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

static int find_voja(int player){
	int c;
	for(c=0; c<active_cards_count[player]; c++){
		if( in_play(player, c) && ! is_humiliated(player, c) && get_card_name(player, c) == CARD_ID_VOJA ){
			if( ! check_for_ability(player, c, KEYWORD_DEFENDER) && ! check_for_special_ability(player, c, SP_KEYWORD_CANNOT_BLOCK) ){
				return 1;
			}
		}
	}
	return 0;
}

int card_tolsimir_wolfblood(int player, int card, event_t event){
	/* Tolsimir Wolfblood	|4|G|W
	 * Legendary Creature - Elf Warrior 3/4
	 * Other |Sgreen creatures you control get +1/+1.
	 * Other |Swhite creatures you control get +1/+1.
	 * |T: Put a legendary 2/2 |Sgreen and |Swhite Wolf creature token named Voja onto the battlefield. */

	// original code : 004E0980


	check_legend_rule(player, card, event);

	boost_creature_by_color(player, card, event, COLOR_TEST_GREEN, 1, 1, 0, BCT_CONTROLLER_ONLY);

	boost_creature_by_color(player, card, event, COLOR_TEST_WHITE, 1, 1, 0, BCT_CONTROLLER_ONLY);

	if( event == EVENT_ACTIVATE ){
		if( ! find_voja(player) ){
			ai_modifier += (current_turn == 1-player ? 48 : 24);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_VOJA);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

static int transluminant_legacy(int player, int card, event_t event){

	if( eot_trigger(player, card, event) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SPIRIT, &token);
		token.key_plus = KEYWORD_FLYING;
		token.color_forced = COLOR_TEST_WHITE;
		generate_token(&token);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_transluminant(int player, int card, event_t event){
	/* Transluminant	|1|G
	 * Creature - Dryad Shaman 2/2
	 * |W, Sacrifice ~: Put a 1/1 |Swhite Spirit creature token with flying onto the battlefield at the beginning of the next end step. */

	if( (event == EVENT_ATTACK_RATING || event == EVENT_BLOCK_RATING) &&
		generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME, MANACOST_W(1), 0, NULL, NULL) )
	{
		if (event == EVENT_ATTACK_RATING){
			ai_defensive_modifier -= 24;
		}
		else {	// EVENT_BLOCK_RATING
			ai_defensive_modifier += 24;
		}
	}

	if( event == EVENT_ACTIVATE ){
		card_instance_t *instance = get_card_instance(player, card);
		if( check_state(player, card, STATE_BLOCKING) && get_attack_power(1-player, instance->blocking) >= get_toughness(player, card) ){
			ai_modifier += 96;
		}
		if( check_state(player, card, STATE_ATTACKING) && current_phase == PHASE_AFTER_BLOCKING ){
			int total_pow = 0;
			int c;
			for(c=0; c<active_cards_count[1-player]; c++){
				if( in_play(1-player, c) && is_what(1-player, c, TYPE_CREATURE) && get_card_instance(1-player, c)->blocking == card ){
					total_pow += get_attack_power(1-player, c);
				}
			}
			if( total_pow >= get_toughness(player, card) ){
				ai_modifier += 96;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		create_legacy_effect(player, card, &transluminant_legacy);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_W(1), 0, NULL, NULL);
}

int card_trophy_hunter(int player, int card, event_t event){
/* CARD_ID_TROPHY_HUNTER	7991
Trophy Hunter	|2|G
Creature - Human Archer 2/3
|1|G: ~ deals 1 damage to target creature with flying.
Whenever a creature with flying dealt damage by ~ this turn dies, put a +1/+1 counter on ~. */

	if( event == EVENT_DEAL_DAMAGE ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->info_slot > 0 ){
			if( damage->damage_source_card == card && damage->damage_source_player == player ){
				card_instance_t *instance = get_card_instance(player, card);
				int pos = MAX(1, instance->info_slot);
				if( pos < 10 ){
					instance->targets[pos].player = damage->damage_target_player;
					instance->targets[pos].card = damage->damage_target_card;
					instance->info_slot = pos + 1;
				}
			}
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! in_play(affected_card_controller, affected_card) ){ return 0; }
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
			check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING) &&
			is_this_dying(affected_card_controller, affected_card) &&
			instance->info_slot > 1 )
		{
			int i;
			for(i=1; i<instance->info_slot; i++){
				if( instance->targets[i].player == affected_card_controller &&
					instance->targets[i].card == affected_card )
				{
					if( instance->targets[11].player < 0 ){
						instance->targets[11].player = 0;
					}
					SET_BYTE0(instance->targets[11].player)++;
				}
			}
		}
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( result ){
		int amount = BYTE0(result);
		add_1_1_counters(player, card, amount);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_abilities = KEYWORD_FLYING;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XG(1, 1), 0,
									&td, "Select target creature with flying.");
}

int card_tunnel_vision(int player, int card, event_t event){
/*
Tunnel Vision |5|U
Sorcery
Name a card. Target player reveals cards from the top of his or her library until the named card is revealed.
If it is, that player puts the rest of the revealed cards into his or her graveyard and puts the named card on top of his or her library.
Otherwise, the player shuffles his or her library.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if( deck_ptr[instance->targets[0].player][0] == -1 ){
				kill_card(player, card, KILL_DESTROY);
				return 0;
			}
			int csvid = -1;
			if( IS_AI(player) ){
				csvid = ai_name_a_random_csvid(player, card, instance->targets[0].player, TYPE_ANY, NULL);
			}
			else{
				csvid = card_from_list(player, 3, TYPE_ANY, 0, 0, 0, 0, 0, 0, 0, -1, 0);
			}
			int count = 0;
			int milled = 0;
			while( deck_ptr[instance->targets[0].player][count] != -1 ){
					if( cards_data[deck_ptr[instance->targets[0].player][count]].id == csvid ){
						milled = 1;
						break;
					}
					count++;
			}
			APNAP(p,{show_deck(p, deck_ptr[instance->targets[0].player], count+1, "Target player revealed these cards", 0, 0x7375B0 );};);
			if( milled == 1 && count > 0 ){
				mill(instance->targets[0].player, count);
			}
			else{
				shuffle(instance->targets[0].player);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_twilight_drover(int player, int card, event_t event){

	/* Twilight Drover	|2|W
	 * Creature - Spirit 1/1
	 * Whenever a creature token leaves the battlefield, put a +1/+1 counter on ~.
	 * |2|W, Remove a +1/+1 counter from ~: Put two 1/1 |Swhite Spirit creature tokens with flying onto the battlefield. */

	if( trigger_condition == TRIGGER_LEAVE_PLAY && affect_me( player, card) && reason_for_trigger_controller == player ){
		if( is_what(trigger_cause_controller, trigger_cause, TYPE_CREATURE) && is_token(trigger_cause_controller, trigger_cause) &&
			! is_humiliated(player, card) )
		{
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					add_1_1_counter(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SPIRIT, &token);
		token.qty = 2;
		token.key_plus = KEYWORD_FLYING;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XW(2, 1), GVC_COUNTER(COUNTER_P1_P1), NULL, NULL);
}

int card_twisted_justice(int player, int card, event_t event){
/* CARD_ID_TWISTED_JUSTICE	7994
Twisted Justice	|4|U|B
Sorcery
Target player sacrifices a creature. You draw cards equal to that creature's power. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			int sac = new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED|SAC_NO_CANCEL|SAC_JUST_MARK|SAC_RETURN_CHOICE,
									&test);
			if( sac ){
				int amount = get_power(BYTE2(sac), BYTE3(sac));
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				draw_cards(player, amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_ursapine(int player, int card, event_t event){
/* CARD_ID_URSAPINE	7996
Ursapine	|3|G|G
Creature - Beast 3/3
|G: Target creature gets +1/+1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_G(1), 0, &td, "TARGET_CREATURE");
}

int card_vedalken_dismisser(int player, int card, event_t event){
/*
Vedalken Dismisser |5|U
Creature — Vedalken Wizard 2/2
When Vedalken Dismisser enters the battlefield, put target creature on top of its owner's library.
*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(1-player, TYPE_CREATURE, -1) < 1 ? -256 : 0);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_veteran_armorer(int player, int card, event_t event){
	/*
	  Veteran Armorer |1|W
	  Creature - Human Soldier 2/2
	  Other creatures you control get +0/+1.
	*/
	boost_creature_type(player, card, event, -1, 0, 1, 0, BCT_CONTROLLER_ONLY);
	return 0;
}

// viashino fangtail --> prodigal sorcerer

int card_vigor_mortis(int player, int card, event_t event){
/* CARD_ID_VIGOR_MORTIS	8001
Vigor Mortis	|2|B|B
Sorcery
Return target creature card from your graveyard to the battlefield.
If |G was spent to cast ~, that creature enters the battlefield with an additional +1/+1 counter on it. */

	ravnica_manachanger(player, card, event, 0, 0, 1, 0, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			instance->targets[1].player = charge_changed_mana(player, card, 0, 0, 1, 0, 0);
		}
		if( spell_fizzled != 1 ){
			return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected > -1 ){
			int zombo = reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
			if( instance->targets[1].player == 2 ){
				add_1_1_counter(player, zombo);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_vinelasher_kudzu(int player, int card, event_t event){
/* CARD_ID_VINELASHER_KUDZU	8003
Vinelasher Kudzu	|1|G
Creature - Plant 1/1
Whenever a land enters the battlefield under your control, put a +1/+1 counter on ~. */

	etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_LAND, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		add_1_1_counter(player, card);
	}

	return 0;
}

int card_vitu_ghazi_the_city_tree(int player, int card, event_t event){
	/* Vitu-Ghazi, the City-Tree	""
	 * Land
	 * |T: Add |1 to your mana pool.
	 * |2|G|W, |T: Put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAPROLING
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_XGW(3, 1, 1), 0, NULL, NULL) )
		{
			choice = CHOICE_SAPROLING;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Create a Saproling", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_SAPROLING ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, 0, MANACOST_XGW(3, 1, 1), 0, NULL, NULL);
			if( spell_fizzled == 1){
				ai_modifier += (current_turn != player && current_phase < PHASE_DECLARE_BLOCKERS ? 48 : 12);
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAPROLING ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
		}
	}

	return 0;
}

int card_voyager_staff(int player, int card, event_t event){
/* CARD_ID_VOYAGER_STAFF	8006
Voyager Staff	|1
Artifact
|2, Sacrifice ~: Exile target creature. Return the exiled card to the battlefield under its owner's control at the beginning of the next end step.*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, NULL);
		if( result ){
			if( IS_AI(player) && (land_can_be_played & LCBP_REGENERATION) ){
				return 99;
			}
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, NULL);
		if( spell_fizzled != 1 ){
			int kill_code = get_card_instance(instance->targets[0].player, instance->targets[0].card)->kill_code;
			ai_modifier += (kill_code == KILL_DESTROY || kill_code == KILL_BURY ? 48 : -48);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			remove_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_vulturous_zombie(int player, int card, event_t event)
{
  /* Vulturous Zombie	|3|B|G
   * Creature - Plant Zombie 3/3
   * Flying
   * Whenever a card is put into an opponent's graveyard from anywhere, put a +1/+1 counter on ~. */

  if (when_a_card_is_put_into_graveyard_from_anywhere_but_library_do_something(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, NULL))
	add_1_1_counter(player, card);

  enable_xtrigger_flags |= ENABLE_XTRIGGER_MILLED;
  if (xtrigger_condition() == XTRIGGER_MILLED && affect_me(player, card) && reason_for_trigger_controller == player
	  && trigger_cause_controller == 1-player && !is_humiliated(player, card))
	{
	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_MANDATORY;

	  if (event == EVENT_RESOLVE_TRIGGER)
		add_1_1_counters(player, card, num_cards_milled);
	}

  return 0;
}

int card_warp_world(int player, int card, event_t event){
/*
Warp World |5|R|R|R
Sorcery
Each player shuffles all permanents he or she owns into his or her library, then reveals that many cards from the top of his or her library.
Each player puts all artifact, creature, and land cards revealed this way onto the battlefield, then does the same for enchantment cards,
	then puts all cards revealed this way that weren't put onto the battlefield on the bottom of his or her library.
*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int diff = count_subtype(player, TYPE_PERMANENT, -1) - count_subtype(1-player, TYPE_PERMANENT, -1);
		ai_modifier += (diff * 8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int revealed[2][151] = {{-1}};
		int rc[2] = {0};
		APNAP(p,{
					test_definition_t test;
					default_test_definition(&test, TYPE_ENCHANTMENT);
					rc[p] = new_manipulate_all(player, card, p, &test, ACT_PUT_ON_TOP);
					default_test_definition(&test, TYPE_PERMANENT);
					rc[p] += new_manipulate_all(player, card, p, &test, ACT_PUT_ON_TOP);
					rc[p] = MIN(rc[p], count_deck(p));
					shuffle(p);
					if( rc[p] ){
						show_deck(0, deck_ptr[p], rc[p], "Cards revealed by Warp World", 0, 0x7375B0 );
						show_deck(1, deck_ptr[p], rc[p], "Cards revealed by Warp World", 0, 0x7375B0 );
						int i;
						for(i=0; i<rc[p]; i++){
							revealed[p][i] = deck_ptr[p][0];
							remove_card_from_deck(p, 0);
						}
					}
				}
		);

		APNAP(p,{
					int i;
					for(i=0; i<rc[p]; i++){
						if( is_what(-1, revealed[p][i], TYPE_CREATURE | TYPE_LAND | TYPE_ARTIFACT) ){
							int card_added = add_card_to_hand(p, revealed[p][i]);
							revealed[p][i] = -1;
							put_into_play(p, card_added);
						}
					}
					for(i=0; i<rc[p]; i++){
						if( revealed[p][i] > -1 && is_what(-1, revealed[p][i], TYPE_ENCHANTMENT) ){
							int card_added = add_card_to_hand(p, revealed[p][i]);
							revealed[p][i] = -1;
							put_into_play(p, card_added);
						}
					}
					int putted_back = 0;
					for(i=0; i<rc[p]; i++){
						if( revealed[p][i] > -1 ){
							raw_put_iid_on_top_of_deck(p, revealed[p][i]);
							putted_back++;
						}
					}
					put_top_x_on_bottom(p, p, putted_back);
				}
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_woebringer_demon(int player, int card, event_t event){
/* CARD_ID_WOEBRINGER_DEMON	8011
Woebringer Demon	|3|B|B
Creature - Demon 4/4
Flying
At the beginning of each player's upkeep, that player sacrifices a creature. If the player can't, sacrifice ~. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int sac = new_sacrifice(player, card, current_turn, SAC_CAUSED | SAC_NO_CANCEL | SAC_RETURN_CHOICE, &test);
		if( !sac){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_wojek_siren(int player, int card, event_t event){
/* CARD_ID_WOJEK_SIREN	8014
Wojek Siren	|W
Instant
Radiance - Target creature and each other creature that shares a color with it get +1/+1 until end of turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		if( valid_target(&td) ){
			radiant_pump(player, card, instance->targets[0].player, instance->targets[0].card, 1, 1, 0, 0, 0);
		}
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return vanilla_instant_pump(player, card, event, ANYBODY, player, 1, 1, 0, 0);
}

int card_selesnya_sanctuary(int player, int card, event_t event){
/* CARD_ID_SELESNYA_SANCTUARY	1725
Selesnya Sanctuary	""
Land
~ enters the battlefield tapped.
When ~ enters the battlefield, return a land you control to its owner's hand.
|T: Add |G|W to your mana pool. */

	return karoo( player, card, event, COLOR_GREEN, COLOR_WHITE, CARD_ID_SAVANNAH );
}

int card_golgari_rot_farm(int player, int card, event_t event){
/* CARD_ID_GOLGARI_ROT_FARM	1726
Golgari Rot Farm	""
Land
~ enters the battlefield tapped.
When ~ enters the battlefield, return a land you control to its owner's hand.
|T: Add |B|G to your mana pool. */

	return karoo( player, card, event, COLOR_GREEN, COLOR_BLACK, CARD_ID_BAYOU );
}

int card_boros_garrison(int player, int card, event_t event){
/* CARD_ID_BOROS_GARRISON	7778
Boros Garrison	""
Land
~ enters the battlefield tapped.
When ~ enters the battlefield, return a land you control to its owner's hand.
|T: Add |R|W to your mana pool. */

	return karoo( player, card, event, COLOR_RED, COLOR_WHITE, CARD_ID_PLATEAU );
}

int card_dimir_aqueduct(int player, int card, event_t event){
/* CARD_ID_DIMIR_AQUEDUCT	1727
Dimir Aqueduct	""
Land
~ enters the battlefield tapped.
When ~ enters the battlefield, return a land you control to its owner's hand.
|T: Add |U|B to your mana pool. */

	return karoo( player, card, event, COLOR_BLACK, COLOR_BLUE, CARD_ID_UNDERGROUND_SEA );
}


