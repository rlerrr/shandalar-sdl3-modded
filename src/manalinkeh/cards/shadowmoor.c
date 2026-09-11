#include "manalink.h"

// Functions
int get_color_for_monocolored_hybrid(int player, int card){
	int card_color = player != -1 ? cards_data[ get_card_instance(player, card)->internal_card_id ].color : cards_data[ card ].color;
	int i;
	for(i=1; i<6; i++){
		if( card_color & (1<<i) ){
			return i;
		}
	}
	return 0;
}

int get_number_of_monocolored_hybrid_mana(int player, int card, int exc_cless){
	int id = player != -1 ? get_id(player, card) : cards_data[card].id;
	card_ptr_t* c = cards_ptr[ id ];
	return c->req_colorless-exc_cless;
}

int has_mana_for_monocolor_hybrid(int player, int hybrid_cless, int clr, int cless){

	int i,x;
	int mana[6];

	for( x = 0 ;x < 6;x++){
		mana[x] = 0;
	}

	for( i = hybrid_cless ;i>=0;i-=2){
		mana[clr] = (hybrid_cless-i)/2;
		if( has_mana_multi( player, i+cless, mana[1], mana[2], mana[3], mana[4], mana[5]) ){
			return 1;
		}
	}
	return 0;
}

int charge_mana_for_monocolor_hybrid(int player, int card, int hybrid_cless, int clr, int cless){

	int i,x;
	int mana[6];
	int mana2[10];

	for( x = 0 ;x < 6;x++){
		mana[x] = 0;
	}

	char buffer[500];
	int pos = scnprintf(buffer, 500, " Cancel\n");
	int m = 1;
	test_definition_t this_test;
	for( i = hybrid_cless ;i>=0;i-=2){
		mana[clr] = (hybrid_cless-i)/2;
		if( has_mana_multi( player, i+cless, mana[1], mana[2], mana[3], mana[4], mana[5]) ){
			mana2[m] = 0;
			mana_into_string(i+cless, mana[1], mana[2], mana[3], mana[4], mana[5], &this_test);
			pos +=scnprintf(buffer+pos, 500-pos, " Pay %s\n", this_test.message);
			int q;
			for(q=0; q<(i+cless); q++){
				mana2[m] |= (1<<q);
			}
			for(q=0; q<mana[clr]; q++){
				mana2[m] |= (1<<(q+16));
			}
			m++;
		}
	}
	int choice = 1;
	if( m > 2 && player == HUMAN){
		choice = do_dialog(player, player, card, -1, -1, buffer, 1);
	}
	if( choice == 0 ){
		spell_fizzled = 1;
		return 0;
	}
	for( x = 0 ;x < 6;x++){
		mana[x] = 0;
	}
	int q;
	for(q=0; q<16; q++){
		if( mana2[choice] & (1<<q) ){
			mana[0]++;
		}
	}
	for(q=16; q<32; q++){
		if( mana2[choice] & (1<<q) ){
			mana[clr]++;
		}
	}
	charge_mana_multi(  player, mana[0], mana[1], mana[2], mana[3], mana[4], mana[5] );
	if( spell_fizzled != 1 ){
		return 1;
	}
	return 0;
}

int get_number_of_hybrid_mana(int player, int card, int ex_c1, int ex_c2){
	int result = 0;
	int id = player != -1 ? get_id(player, card) : cards_data[card].id;
	card_ptr_t* c = cards_ptr[ id ];
	if( ex_c1 != COLOR_BLACK && ex_c2 != COLOR_BLACK){
		result+=c->req_black;
	}
	if( ex_c1 != COLOR_BLUE && ex_c2 != COLOR_BLUE){
		result+=c->req_blue;
	}
	if( ex_c1 != COLOR_GREEN && ex_c2 != COLOR_GREEN){
		result+=c->req_green;
	}
	if( ex_c1 != COLOR_RED && ex_c2 != COLOR_RED){
		result+=c->req_red;
	}
	if( ex_c1 != COLOR_WHITE && ex_c2 != COLOR_WHITE){
		result+=c->req_white;
	}
	return result;
}

int get_colors_for_hybrid(int player, int card, int ex_c1, int ex_c2){
	int card_color = player != -1 ? cards_data[ get_card_instance(player, card)->internal_card_id ].color : cards_data[ card ].color;
	int i;
	for(i=1; i<7; i++){
		if( (1<<ex_c1) != (1<<i) && (1<<ex_c2) != (1<<i) && (card_color & (1<<i))  ){
			return i;
		}
	}
	return 0;
}

int has_mana_hybrid(int player, int number, int first_color, int second_color, int colorless){

	int i,x;
	int mana[6];

	for( x = 0 ;x < 6;x++){
		mana[x] = 0;
	}

	for ( i = number ;i>=0;i--){
		 mana[first_color] = i;
		 mana[second_color] = number - i;
		if( has_mana_multi( player,  colorless, mana[1], mana[2], mana[3], mana[4], mana[5]) ){
			return 1;
		}
	}
	return 0;
}

int charge_mana_hybrid(int player, int card, int number, int first_color, int second_color, int colorless){
	int i,x;
	int mana[6];
	int mana2[10];

	for( x = 0 ;x < 6;x++){
		mana[x] = 0;
	}

	char buffer[500];
	int pos = scnprintf(buffer, 500, " Cancel\n");
	int m = 1;
	test_definition_t this_test;
	for( i = number ;i>=0;i--){
		mana[first_color] = i;
		mana[second_color] = number - i;
		if( has_mana_multi( player, colorless, mana[1], mana[2], mana[3], mana[4], mana[5]) ){
			mana2[m] = 0;
			mana_into_string(colorless, mana[1], mana[2], mana[3], mana[4], mana[5], &this_test);
			pos +=scnprintf(buffer+pos, 500-pos, " Pay %s\n", this_test.message);
			int q;
			for(q=1; q<6; q++){
				int k;
				for(k=0; k<mana[q]; k++){
					mana2[m] |= (1<<(((q-1)*6)+k));
				}
			}
			m++;
		}
	}
	int choice = 1;
	if( m > 2 && player == HUMAN){
		choice = do_dialog(player, player, card, -1, -1, buffer, 1);
	}
	if( choice == 0 ){
		spell_fizzled = 1;
		return 0;
	}
	for( x = 0 ;x < 6;x++){
		mana[x] = 0;
	}
	int q;
	for(q=0; q<5; q++){
		int k;
		for(k=0; k<6; k++){
			if( mana2[choice] & (1<<(((q*6)+k))) ){
				mana[q+1]++;
			}
		}
	}
	charge_mana_multi(  player, colorless, mana[1], mana[2], mana[3], mana[4], mana[5] );
	if( spell_fizzled != 1 ){
		return 1;
	}
	return 0;
}

int hybrid_casting(int player, int card, int mcolor_hybrid){
	card_instance_t *instance = get_card_instance(player, card);
	if( played_for_free(player, card) || is_token(player, card) || instance->info_slot == 0){
		return 1;
	}
	else{
		if( ! mcolor_hybrid){
			int c1 = get_colors_for_hybrid(player, card, 0, 0);
			int c2 = get_colors_for_hybrid(player, card, c1, 0);
			int cless = get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, -1);
			if( has_mana_hybrid(player, get_number_of_hybrid_mana(player, card, 0, 0), c1, c2, cless) ){
				charge_mana_hybrid(player, card, get_number_of_hybrid_mana(player, card, 0, 0), c1, c2, cless);
				if( spell_fizzled != 1 ){
					return 1;
				}
			}
		}
		else{
			int c1 = get_color_for_monocolored_hybrid(player, card);
			if( has_mana_for_monocolor_hybrid(player, get_number_of_monocolored_hybrid_mana(player, card, 0), c1,  0) ){
				charge_mana_for_monocolor_hybrid(player, card, get_number_of_monocolored_hybrid_mana(player, card, 0), c1,  0);
				if( spell_fizzled != 1 ){
					return 1;
				}
			}
		}
	}
	return 0;
}

int modify_cost_for_hybrid_spells(int player, int card, event_t event, int mcolor_hybrid ){

	if( event == EVENT_MODIFY_COST ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! mcolor_hybrid ){
			int c1 = get_colors_for_hybrid(player, card, 0, 0);
			int c2 = get_colors_for_hybrid(player, card, c1, 0);
			int cless = get_updated_casting_cost(player, card, -1, event, -1);
			if( has_mana_hybrid(player, get_number_of_hybrid_mana(player, card, 0, 0), c1, c2, cless) ){
				null_casting_cost(player, card);
				instance->info_slot = 1;
			}
			else{
				instance->info_slot = 0;
			}
		}
		else{
			int c1 = get_color_for_monocolored_hybrid(player, card);
			if( has_mana_for_monocolor_hybrid(player, get_number_of_monocolored_hybrid_mana(player, card, 0), c1, 0) ){
				null_casting_cost(player, card);
				instance->info_slot = 1;
			}
			else{
				instance->info_slot = 0;
			}
		}
	}
	return 0;
}

static int monocolor_hybrid(int player, int card, event_t event){
	modify_cost_for_hybrid_spells(player, card, event, 1);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! hybrid_casting(player, card, 1) ){
			spell_fizzled = 1;
		}
	}
	return 0;
}

int hybrid(int player, int card, event_t event){
	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! hybrid_casting(player, card, 0) ){
			spell_fizzled = 1;
		}
	}
	return 0;
}


int conspire(int player, int card, event_t event ){

	card_instance_t *instance = get_card_instance( player, card );

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller &&
		trigger_cause_controller == player && ! check_special_flags(trigger_cause_controller, trigger_cause, SF_NOT_CAST)
	  ){
		card_instance_t *spell = get_card_instance( trigger_cause_controller, trigger_cause);
		if( cards_data[spell->internal_card_id].cc[2] == 19 ){
			int clr = get_color(trigger_cause_controller, trigger_cause);
			int clrs[5] = {0, 0, 0, 0, 0};
			int i;
			for(i=1; i<7; i++){
				if( clr & (1<<i) ){
					clrs[i-1] = 1<<i;
				}
			}
			target_definition_t td1;
			default_target_definition(player, card, &td1, TYPE_CREATURE);
			td1.allowed_controller = player;
			td1.preferred_controller = player;
			td1.illegal_abilities = 0;
			td1.illegal_state = TARGET_STATE_TAPPED;
			td1.required_color = clrs[0] | clrs[1] | clrs[2] | clrs[3] | clrs[4];
			if( target_available(player, card, &td1) > 1 ){
				if(event == EVENT_TRIGGER){
					event_result |= 1+player;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						int total = 0;
						while( total < 2 && can_target(&td1) ){
								if( select_target(player, card, &td1, "Select a creature which shares a color with the spell.", &(instance->targets[total])) ){
									state_untargettable(instance->targets[total].player, instance->targets[total].card, 1);
									total++;
								}
								else{
									break;
								}
						}
						for(i=0; i<total; i++){
							state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
							if( total == 2 ){
								tap_card(instance->targets[i].player, instance->targets[i].card);
							}
						}
						if( total == 2 ){
							copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
						}
				}
			}
		}
	}
	return 0;
}

int liege(int player, int card, event_t event, int color1, int color2){
	boost_creature_by_color(player, card, event, color1, 1, 1, 0, BCT_CONTROLLER_ONLY);
	boost_creature_by_color(player, card, event, color2, 1, 1, 0, BCT_CONTROLLER_ONLY);
	return hybrid(player, card, event);
}


int card_filter_land(int player, int card, event_t event, int color1, int color2, int color_test1, int color_test2, const char * str){

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);

		instance->info_slot = COLOR_TEST_COLORLESS;
	}

	if( has_mana(player, color1, 1) || has_mana(player, color2, 1) ){
		instance->info_slot = COLOR_TEST_COLORLESS | color_test1 | color_test2;
	}
	else{
		instance->info_slot = COLOR_TEST_COLORLESS;
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && instance->targets[1].player != 66){
		if( ! is_tapped(player, card) && ! is_animated_and_sick(player, card) ){
			if( check_special_flags2(player, card, SF2_CONTAMINATION) ){
				declare_mana_available(player, COLOR_BLACK, 1);
			}
			else{
				if( has_mana(player, color1, 1) || has_mana(player, color2, 1) ){
					declare_mana_available_hex(player, (1<<color1) | (1<<color2), 1);
				}
				else{
					declare_mana_available(player, COLOR_COLORLESS, 1);
				}
			}
		}
	}

	if( event == EVENT_CAN_ACTIVATE && instance->targets[1].player != 66 ){
		if( ! is_tapped(player, card) && ! is_animated_and_sick(player, card) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( player == AI || ai_is_speculating == 1 ){
			int consume_color = -1;
			if( has_mana(player, color1, 2) ){
				consume_color = color1;
			}
			else if( has_mana(player, color2, 2) ){
				consume_color = color2;
			}

			if( consume_color == -1 ){
				produce_mana_tapped(player, card, COLOR_COLORLESS, 1);
			}
			else{
				instance->targets[1].player = 66;
				charge_mana(player, consume_color, 1);
				if( spell_fizzled != 1 ){
					produce_mana_tapped_any_combination_of_colors(player, card, (1<<color1) | (1<<color2), 2, NULL);
				}
				instance->targets[1].player = 0;
			}
		}
		else{
			if(event == EVENT_ACTIVATE ){
				int choice = 0;
				if( has_mana(player, color1, 1) || has_mana(player, color2, 1) ){
					choice = do_dialog(player, player, card, -1, -1, str, 0);
				}
				if( choice == 7){
					spell_fizzled = 1;
				}
				else{
					int consume_color = color2;
					int produce_color1 = color2;
					int produce_color2 = color2;
					if( choice == 0 ){
						produce_mana_tapped(player, card, COLOR_COLORLESS, 1);
					}
					else{
						consume_color = color2;
						if( choice < 4 ){
							consume_color = color1;
						}
						if( choice == 1 || choice == 2 || choice == 4 || choice == 5 ){
							produce_color1 = color1;
						}
						if( choice == 1 || choice == 4 ){
							produce_color2 = color1;
						}
						instance->targets[1].player = 66;
						charge_mana(player, consume_color, 1);
						if( spell_fizzled != 1 ){
							/* produce_mana_tapped_any_combination_of_colors() would pick the colors more intelligently while autotapping.  However, when not
							 * autotapping, or when the choice was ambiguous, there'd be three dialogs - 1. pay nothing, color1, or color2 to activate (done
							 * here), then 2. pick first color and 3. pick second color (both done by produce_mana_tapped_any_combination_of_colors()). */
							produce_mana_tapped2(player, card, produce_color1, 1, produce_color2, 1);
						}
						instance->targets[1].player = 0;
					}
				}
			}
		}
	}
	return 0;
}

void aura_ability_for_color(int player, int card, event_t event, int c1, int p, int t, int k, int sk){
	card_instance_t *instance = get_card_instance(player, card);
	if( in_play(player, card) && instance->damage_target_player > -1 && ! is_humiliated(player, card) ){
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;
		if( get_color(t_player, t_card) & get_sleighted_color_test(player, card, c1) ){
			modify_pt_and_abilities(t_player, t_card, event, p, t, k);
			special_abilities(t_player, t_card, event, sk, player, card);
		}
	}
}

int persist_granted(int player, int card, event_t event){
 	card_instance_t *instance = get_card_instance(player, card);
	if( instance->damage_target_player > -1 ){
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(p, c) && in_play(p, c) && ! is_token(p, c) && count_counters(p, c, COUNTER_M1_M1) < 1 ){
			card_instance_t *dead = get_card_instance(p, c);
			if( dead->kill_code > 0 && dead->kill_code < KILL_EXILE ){
				instance->targets[11].card = get_original_internal_card_id(p, c);
				instance->targets[11].player = get_owner(p, c);
				instance->damage_target_player = instance->damage_target_card = -1;
			}
		}
	}

	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && player == reason_for_trigger_controller && affect_me(player, card)){
		if( instance->targets[11].player != -1 ){
			if(event == EVENT_TRIGGER){
				//Make all trigges mandatoy for now
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					seek_grave_for_id_to_reanimate(instance->targets[11].player, -1, instance->targets[11].player, cards_data[instance->targets[11].card].id,
													REANIMATE_MINUS1_MINUS1_COUNTER);
					kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

// Cards

int card_advice_from_the_fae(int player, int card, event_t event){
	/*
	  Advice from the Fae {2/U}{2/U}{2/U}
	  Sorcery
	  ({2/U} can be paid with any two mana or with {U}. This card's converted mana cost is 6.)
	  Look at the top five cards of your library. If you control more creatures than each other player, put two of those cards into your hand.
	  Otherwise, put one of them into your hand. Then put the rest on the bottom of your library in any order.
	*/
	monocolor_hybrid(player, card, event);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1) > count_subtype(1-player, TYPE_CREATURE, -1) ? 48 : 24);
	}


	if( event == EVENT_RESOLVE_SPELL ){
		int to_look = MIN(count_deck(player), 5);
		if( to_look ){
			int amount = 1 + (count_subtype(player, TYPE_CREATURE, -1) > count_subtype(1-player, TYPE_CREATURE, -1));
			amount = MIN(amount, to_look);

			while( amount && to_look ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, "Select a card to add to your hand");
				this_test.create_minideck = to_look;
				this_test.no_shuffle = 1;
				new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 1, AI_MAX_VALUE, &this_test);
				amount--;
				to_look--;
			}
			if( to_look-amount > 0 ){
				put_top_x_on_bottom(player, player, to_look-amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_aethertow(int player, int card, event_t event){
/* CARD_ID_AETHERTOW	9472
Aethertow	|3|WU
Instant
Put target attacking or blocking creature on top of its owner's library.
Conspire (As you cast this spell, you may tap two untapped creatures you control that share a color with it.
When you do, copy it and you may choose a new target for the copy.) */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_IN_COMBAT;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( hybrid_casting(player, card, 0) ){
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
								"Select target attacking or blocking creature.", 1, NULL);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

// apothecary initiate --> ivory cup

int card_ashenmoor_gouger(int player, int card, event_t event){
/* CARD_ID_ASHENMOOR_GOUGER	1415
Ashenmoor Gouger	|BR|BR|BR
Creature - Elemental Warrior 4/4
~ can't block. */

	cannot_block(player, card, event);
	return hybrid(player, card, event);
}

int card_ashenmoor_liege(int player, int card, event_t event)
{
/* CARD_ID_ASHENMOOR_LIEGE	9471
Ashenmoor Liege	|1|BR|BR|BR
Creature - Elemental Knight 4/1
Other |Sblack creatures you control get +1/+1.
Other |Sred creatures you control get +1/+1.
Whenever ~ becomes the target of a spell or ability an opponent controls, that player loses 4 life. */

  if (in_play(player, card)
	  && !is_humiliated(player, card)
	  && becomes_target_of_spell_or_effect(player, card, event, player, card, 1-player))
	lose_life(1-player, 4);

  return liege(player, card, event, COLOR_TEST_BLACK, COLOR_TEST_RED);
}

int card_augury_adept(int player, int card, event_t event){
/* CARD_ID_AUGURY_ADEPT	9473
Augury Adept	|1|WU|WU
Creature - Kithkin Wizard 2/2
Whenever ~ deals combat damage to a player, reveal the top card of your library and put that card into your hand. You gain life equal to its converted mana cost.*/

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		while( amount && deck_ptr[player][0] != -1 ){
				int iid = deck_ptr[player][0];
				remove_card_from_deck(player, 0);
				reveal_card_iid(player, card, iid);
				add_card_to_hand(player, iid);
				gain_life(player, get_cmc_by_internal_id(iid));
				amount--;
		}
	}

	return hybrid(player, card, event);
}

int card_barkshell_blessing(int player, int card, event_t event){
/* CARD_ID_BARKSHELL_BLESSING	9475
Barkshell Blessing	|GW
Instant
Target creature gets +2/+2 until end of turn.
Conspire (As you cast this spell, you may tap two untapped creatures you control that share a color with it.
When you do, copy it and you may choose a new target for the copy.) */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);

	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( hybrid_casting(player, card, 0) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_beseech_the_queen(int player, int card, event_t event){
/* CARD_ID_BESEECH_THE_QUEEN	9478
Beseech the Queen	|2B|2B|2B
Sorcery
(|2B can be paid with any two mana or with |B. This card's converted mana cost is 6.)
Search your library for a card with converted mana cost less than or equal to the number of lands you control, reveal it, and put it into your hand.
Then shuffle your library. */

	monocolor_hybrid(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_LAND, -1) * 12);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int amount = count_subtype(player, TYPE_LAND, -1);
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		scnprintf(test.message, 100, "Select a card with CMC %d or less.", amount);
		test.cmc = amount+1;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_blight_sickle(int player, int card, event_t event){
/* CARD_ID_BLIGHT_SICKLE	9481
Blight Sickle	|2
Artifact - Equipment
Equipped creature gets +1/+0 and has wither. (It deals damage to creatures in the form of -1/-1 counters.)
Equip |2 */

	return vanilla_equipment(player, card, event, 2, 1, 0, 0, SP_KEYWORD_WITHER);
}

int card_bloodmark_mentor(int player, int card, event_t event){
/* CARD_ID_BLOODMARK_MENTOR	9483
Bloodmark Mentor	|1|R
Creature - Goblin Warrior 1/1
|SRed creatures you control have first strike. */

	boost_creature_by_color(player, card, event, COLOR_TEST_RED, 0, 0, KEYWORD_FIRST_STRIKE, BCT_INCLUDE_SELF+BCT_CONTROLLER_ONLY);
	return 0;
}

static int blowfly_infestation_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_BLOWFLY_INFESTATION ){
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
				if( count_counters(affected_card_controller, affected_card, COUNTER_M1_M1) ){
					count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
				}
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.allow_cancel = 0;
			card_instance_t *instance = get_card_instance(player, card);
			while( can_target(&td) && amount ){
					if( new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
						add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 1);
						instance->number_of_targets = 0;
						amount--;
					}
			}
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

int card_blowfly_infestation(int player, int card, event_t event){
/*
Blowfly Infestation |2|B
Enchantment
Whenever a creature dies, if it had a -1/-1 counter on it, put a -1/-1 counter on target creature.
*/
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)blowfly_infestation_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &blowfly_infestation_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( count_counters(affected_card_controller, affected_card, COUNTER_M1_M1) ){
			count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.allow_cancel = 0;
			card_instance_t *instance = get_card_instance(player, card);
			while( can_target(&td) && amount ){
					if( new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
						add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 1);
						instance->number_of_targets = 0;
						amount--;
					}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_boartusk_liege(int player, int card, event_t event){
/* CARD_ID_BOARTUSK_LIEGE	9486
Boartusk Liege	|1|RG|RG|RG
Creature - Goblin Knight 3/4
Trample
Other |Sred creatures you control get +1/+1.
Other |Sgreen creatures you control get +1/+1. */

	return liege(player, card, event, COLOR_TEST_GREEN, COLOR_TEST_RED);
}

int card_boggart_ramgang(int player, int card, event_t event){
/* CARD_ID_BOGGART_RAM_GANG	9488
Boggart Ram-Gang	|RG|RG|RG
Creature - Goblin Warrior 3/3
Haste
Wither (This deals damage to creatures in the form of -1/-1 counters.) */

	hybrid(player, card, event);
	haste(player, card, event);
	wither(player, card, event);
	return 0;
}

// burn trail --> lighting bolt

int card_cauldron_of_souls(int player, int card, event_t event){
/* CARD_ID_CAULDRON_OF_SOULS	9492
Cauldron of Souls	|5
Artifact
|T: Choose any number of target creatures. Each of those creatures gains persist until end of turn.
(When it dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int total = 0;
			while( can_target(&td) ){
					if( select_target(player, card, &td, "Select target Creature.", &(instance->targets[total])) ){
						state_untargettable(instance->targets[total].player, instance->targets[total].card, 1);
						total++;
					}
					else{
						break;
					}
			}
			int i;
			for(i=0; i<total; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			}
			if( total > 0 ){
				tap_card(player, card);
				instance->info_slot = total;
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int i;
		for(i=0; i<instance->info_slot; i++){
			if( validate_target(player, card, &td, i) ){
				create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &persist_granted,
												instance->targets[i].player, instance->targets[i].card);
			}
		}
	}

	return 0;
}

static int effect_cemetery_puca(int player, int card, event_t event);
static void cemetery_puca_impl(int player, int card, event_t event, int is_effect)
{
  if (event != EVENT_GRAVEYARD_FROM_PLAY && trigger_condition != TRIGGER_GRAVEYARD_FROM_PLAY)
	return;

  card_instance_t* instance = get_card_instance(player, card);
  int p, c;
  if (is_effect)
	{
	  p = instance->damage_target_player;
	  c = instance->damage_target_card;
	  if (!in_play(p, c))
		return;
	}
  else
	{
	  p = player;
	  c = card;
	}

  if (trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && !has_mana(p, COLOR_ANY, 1) && event != EVENT_END_TRIGGER)
	return;

  test_definition_t test;
  new_default_test_definition(&test, TYPE_CREATURE, "");

  int num = store_and_trigger_graveyard_from_play(p, c, event, ANYBODY, RESOLVE_TRIGGER_AI(player), &test, player, card);
  if (num)
	{
	  if (num > 11)
		num = 11;
	  char names[11][200];
	  int i;
	  for (i = 0; i < 11; ++i)
		if (i < num)
		  scnprintf(names[i], 200, cards_ptr[get_backup_id(instance->targets[i].player, instance->targets[i].card)]->full_name);
		else
		  names[i][0] = 0;

#define NAME(x)	names[x], (x) < num, 1
	  int choice = DIALOG(player, card, EVENT_ACTIVATE,
						  DLG_RANDOM, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL, DLG_NO_CANCEL, // cancel option added manually so the AI considers it
						  DLG_FULLCARD_CSVID(CARD_ID_CEMETERY_PUCA), DLG_SMALLCARD(p, c),
						  NAME(0), NAME(1), NAME(2), NAME(3), NAME(4), NAME(5), NAME(6), NAME(7), NAME(8), NAME(9), NAME(10),
						  "Cancel", 1, 1);
#undef NAME

	  --choice;
	  if (choice >= 0 && choice < num)
		{
		  // Store first because charge_mana() could overwrite, e.g. if sacrificing an Eldrazi Spawn token for mana
		  int t_player = instance->targets[choice].player;
		  int t_card = instance->targets[choice].card;
		  if (charge_mana_while_resolving_csvid(CARD_ID_CEMETERY_PUCA, EVENT_RESOLVE_TRIGGER, player, COLOR_COLORLESS, 1))
			{
			  if (!is_effect)
				create_targetted_legacy_effect(p, c, effect_cemetery_puca, player, card);

			  cloning_and_verify_legend(p, c, t_player, t_card);
			}
		}
	}
}
static int effect_cemetery_puca(int player, int card, event_t event)
{
  if (effect_follows_control_of_attachment(player, card, event))
	return 0;

  cemetery_puca_impl(player, card, event, 1);

  return 0;
}
int card_cemetery_puca(int player, int card, event_t event)
{
	/* Cemetery Puca	|1|UB|UB	0x2007c67
	 * Creature - Shapeshifter 1/2
	 * Whenever a creature dies, you may pay |1. If you do, ~ becomes a copy of that creature, except it has this ability. */
	cloning_card(player, card, event);

	cemetery_puca_impl(player, card, event, 0);

	return hybrid(player, card, event);
}

int card_chainbreaker(int player, int card, event_t event){

	/* Chainbreaker	|2
	 * Artifact Creature - Scarecrow 3/3
	 * ~ enters the battlefield with two -1/-1 counters on it.
	 * |3, |T: Remove a -1/-1 counter from target creature. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_M1_M1, 2);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_M1_M1;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		if( current_turn == 1-player && current_phase >= PHASE_AFTER_BLOCKING ){
			ai_modifier += 48;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			remove_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_M1_M1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_CREATURE");
}

int card_cinderhaze_wretch(int player, int card, event_t event){
/*
Cinderhaze Wretch |4|B
Creature â€” Elemental Shaman 3/2
{T}: Target player discards a card. Activate this ability only during your turn.
Put a -1/-1 counter on Cinderhaze Wretch: Untap Cinderhaze Wretch.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_DISCARD = 1,
		CHOICE_UNTAP
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_IN_YOUR_TURN | GAA_UNTAPPED, MANACOST0, 0, &td, NULL) )
		{
			return 1;
		}
		return generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_IN_YOUR_TURN | GAA_UNTAPPED,
													MANACOST0, 0, &td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, 0, NULL, NULL)
		};
		int priority_untap = is_tapped(player, card) && get_toughness(player, card) > 1 ? 10 : 1;
		int choice = DIALOG(player, card, event, DLG_NO_STORAGE,
							"Target player discard", abils[1], 5,
							"Untap Cinderhaze Wretch", abils[2], priority_untap);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_DISCARD ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_IN_YOUR_TURN | GAA_UNTAPPED, MANACOST0, 0,
											&td, "TARGET_PLAYER");
		}
		if( choice == CHOICE_UNTAP ){
			generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				add_counter(player, card, COUNTER_M1_M1);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DISCARD && valid_target(&td) ){
			discard(instance->targets[0].player, 0, player);
		}
		if( instance->info_slot == CHOICE_UNTAP ){
			untap_card(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_corrosive_mentor(int player, int card, event_t event)
{
/* CARD_ID_CORROSIVE_MENTOR	9499
Corrosive Mentor	|2|B
Creature - Elemental Rogue 1/3
|SBlack creatures you control have wither. (They deal damage to creatures in the form of -1/-1 counters.) */

  if (event == EVENT_ABILITIES
	  && affected_card_controller == player
	  && in_play(player, card)
	  && !is_humiliated(player, card)
	  && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
	  && (get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_BLACK)))
	wither(affected_card_controller, affected_card, event);

  return 0;
}

int card_counterbore(int player, int card, event_t event){
/* Counterbore	|3|U|U
 * Instant
 * Counter target spell. Search its controller's graveyard, hand, and library for all cards with the same name as that spell and exile them.
 * Then that player shuffles his or her library. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			int csvid = get_id(instance->targets[0].player, instance->targets[0].card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			lobotomy_effect(player, instance->targets[0].player, csvid, 1);
		}
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return counterspell(player, card, event, NULL, 0);
}

int card_cragganwick_cremator(int player, int card, event_t event){
	/* Cragganwick Cremator	|2|R|R	0x2007c85
	 * Creature - Giant Shaman 5/4
	 * When ~ enters the battlefield, discard a card at random. If you discard a creature card this way, ~ deals damage equal to that card's power to target player or planeswalker. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( hand_count[player] && can_target(&td) ){
			int cih[100];
			int cih_index = 0;
			int i;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_hand(player, i) ){
					cih[cih_index] = i;
					cih_index++;
				}
			}
			int discarded = cih_index-1;
			if( cih_index > 1 ){
				discarded = internal_rand(cih_index);
			}
			int pow = get_power(player, cih[discarded]);
			discard_card(player, cih[discarded]);
			if( pow > 0 && new_pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER", 0, 0) ){
				damage_target0(player, card, pow);
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_crowd_of_cinders(int player, int card, event_t event){
/* CARD_ID_CROWD_OF_CINDERS	9504
Crowd of Cinders	|3|B
Creature - Elemental 100/100
Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
~'s power and toughness are each equal to the number of |Sblack permanents you control. */

	fear(player, card, event);

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && player != -1 && ! is_humiliated(player, card) ){
		event_result += count_permanents_by_color(player, TYPE_PERMANENT, get_sleighted_color_test(player, card, COLOR_TEST_BLACK));
	}

	return 0;
}

int card_cursecatcher(int player, int card, event_t event){
	/* Cursecatcher	|U
	 * Creature - Merfolk Wizard 1/1
	 * Sacrifice ~: Counter target instant or sorcery spell unless its controller pays |1. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_SPELL);

	if( event == EVENT_ACTIVATE ){
		if( has_mana(card_on_stack_controller, COLOR_COLORLESS, 1) ){
			ai_modifier-=20;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		counterspell_resolve_unless_pay_x(player, card, &td, 0, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_SPELL_ON_STACK, MANACOST0, 0, &td, NULL);
}

int card_dawnglove_infusion(int player, int card, event_t event){
/* CARD_ID_DAWNGLOW_INFUSION	9508
Dawnglow Infusion	|X|GW
Sorcery
You gain X life if |G was spent to cast ~ and X life if |W was spent to cast it. (Do both if |G|W was spent.) */

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_MODIFY_COST ){
		if( has_mana_hybrid(player, 1, COLOR_GREEN, COLOR_WHITE, 0) ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) && ! played_for_free(player, card) ){
			int paid_mana_white = 0;
			int paid_mana_green	= 0;
			if( ! played_for_free(player, card) && instance->info_slot == 1 ){
				int cless = get_updated_casting_cost(player, card, -1, event, 0);
				charge_mana_hybrid(player, card, 1, COLOR_GREEN, COLOR_WHITE, cless);
				paid_mana_white = mana_paid[COLOR_WHITE];
				paid_mana_green	= mana_paid[COLOR_GREEN];
			}
			if( spell_fizzled != 1 ){
				charge_mana(player, COLOR_COLORLESS, -1);
				if( spell_fizzled != 1 ){
					set_x_for_x_spells(player, card, event, x_value);
					if( (mana_paid[COLOR_GREEN] + paid_mana_green) > 0 && (mana_paid[COLOR_WHITE] + paid_mana_white) > 0){
						instance->info_slot *= 2;
					}
					if( life[player] < 6 ){
						ai_modifier += (instance->info_slot * 12);
					}
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, instance->info_slot);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_deep_slumber_titan(int player, int card, event_t event){
/* CARD_ID_DEEP_SLUMBER_TITAN	9509
Deep-Slumber Titan	|2|R|R
Creature - Giant Warrior 7/7
~ enters the battlefield tapped.
~ doesn't untap during your untap step.
Whenever ~ is dealt damage, untap it. */

	comes_into_play_tapped(player, card, event);

	does_not_untap(player, card, event);

	if( damage_dealt_to_me_arbitrary(player, card, event, 0, player, card) ){
		untap_card(player, card);
	}

	return 0;
}

int card_demigod_of_revenge(int player, int card, event_t event){
/* CARD_ID_DEMIGOD_OF_REVENGE	9511
Demigod of Revenge	|BR|BR|BR|BR|BR
Creature - Spirit Avatar 5/4
Flying, haste
When you cast ~, return all cards named ~ from your graveyard to the battlefield. */

	modify_cost_for_hybrid_spells(player, card, event, 0);
	haste(player, card, event);
	if( event == EVENT_CAST_SPELL && affect_me(player, card) && ! check_special_flags(player, card, SF_NOT_CAST) &&
		! is_token(player, card) )
	{
		if( hybrid_casting(player, card, 0) ){
			const int *graveyard = get_grave(player);
			int count = count_graveyard(player)-1;
			while( count > -1 ){
				   if( cards_data[ graveyard[count]].id == get_id(player, card)  ){
						reanimate_permanent(player, card, player, count, REANIMATE_DEFAULT);
					}
					count--;
			}
		}
	}

	return 0;
}

int card_deus_of_calamity(int player, int card, event_t event){
/* CARD_ID_DEUS_OF_CALAMITY	9512
Deus of Calamity	|RG|RG|RG|RG|RG
Creature - Spirit Avatar 6/6
Trample
Whenever ~ deals 6 or more damage to an opponent, destroy target land that player controls. */

	if( event == EVENT_DEAL_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_source_player == player && damage->damage_source_card == card &&
				damage->damage_target_card == -1 && damage->damage_target_player == 1-player &&
				! damage_is_to_planeswalker(damage) )
			{
				if( damage->info_slot >= 6 ){
					if( instance->targets[8].player < 0 ){
						instance->targets[8].player = 0;
					}
					instance->targets[8].player++;
				}
			}
		}
	}

	int result = resolve_damage_trigger(player, card, event, 0, player, card);
	if( result ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		while( result && can_target(&td) ){
				if( new_pick_target(&td, "Select target land your opponent controls.", 0, 0) ){
					kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				}
				instance->number_of_targets = 0;
				result--;
		}
	}

	return hybrid(player, card, event);
}

int card_devoted_druid(int player, int card, event_t event)
{
  /* Devoted Druid	|1|G
   * Creature - Elf Druid 0/2
   * |T: Add |G to your mana pool.
   * Put a -1/-1 counter on ~: Untap ~. */

  if (IS_ACTIVATING(event))
	{
	  enum
	  {
		CHOICE_MANA = 1,
		CHOICE_UNTAP
	  } choice = DIALOG(player, card, event,
						DLG_AUTOCHOOSE_IF_1,
						"Add mana", CAN_TAP_FOR_MANA(player, card) && (paying_mana() || !IS_AI(player)), 1,
						"Untap", !paying_mana() && CAN_ACTIVATE0(player, card) && !check_battlefield_for_id(player, CARD_ID_MELIRA_SYLVOK_OUTCAST), 2);

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (event == EVENT_ACTIVATE)
		switch (choice)
		  {
			case CHOICE_MANA:
			  produce_mana_tapped(player, card, COLOR_GREEN, 1);
			  break;

			case CHOICE_UNTAP:
			  if (charge_mana_for_activated_ability(player, card, MANACOST0))
				add_counter(player, card, COUNTER_M1_M1);
			  if (!is_tapped(player, card))
				ai_modifier += player == AI ? -32 : 32;
			  break;
		  }
	  else	// EVENT_RESOLVE_ACTIVATION
		if (choice == CHOICE_UNTAP)
		  {
			card_instance_t* instance = get_card_instance(player, card);
			if (in_play(instance->parent_controller, instance->parent_card))
			  untap_card(instance->parent_controller, instance->parent_card);
		  }

	  return 0;
	}
  else
	return mana_producing_creature(player, card, event, 24, COLOR_GREEN, 1);	// EVENT_COUNT_MANA, EVENT_ATTACK_RATING, EVENT_BLOCK_RATING
}

int card_din_of_the_fireherd(int player, int card, event_t event){
/* Din of the Fireherd	|5|BR|BR|BR
 * Sorcery
 * Put a 5/5 |Sblack and |Sred Elemental creature token onto the battlefield. Target opponent sacrifices a creature for each
 *		|Sblack creature you control, then sacrifices a land for each |Sred creature you control. */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( hybrid_casting(player, card, 0) ){
			return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
			token.pow = 5;
			token.tou = 5;
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_RED | COLOR_TEST_BLACK);
			generate_token(&token);

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.color = COLOR_TEST_BLACK;
			int result = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test);
			if( result > 0 ){
				test_definition_t test2;
				new_default_test_definition(&test2, TYPE_CREATURE, "Select a creature to sacrifice.");
				test2.qty = result;
				new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test2);
			}
			this_test.color = COLOR_TEST_RED;
			result = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test);
			if( result > 0 ){
				test_definition_t test2;
				new_default_test_definition(&test2, TYPE_LAND, "Select a land to sacrifice.");
				test2.qty = result;
				new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test2);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_dire_undercurrents(int player, int card, event_t event){
/*
Dire Undercurrents |3{U/B}{U/B}
Enchantment
Whenever a blue creature enters the battlefield under your control, you may have target player draw a card.
Whenever a black creature enters the battlefield under your control, you may have target player discard a card.
*/

	hybrid(player, card, event);

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK | COLOR_TEST_BLUE);

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		int c1 = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		int c2 = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		int clr = get_color(trigger_cause_controller, trigger_cause);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance( player, card );

		if( clr & c1 ){
			if( new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
				discard(instance->targets[0].player, 0, player);
			}
		}
		instance->number_of_targets = 0;

		if( clr & c2 ){
			td.preferred_controller = player;
			if( new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
				draw_a_card(instance->targets[0].player);
			}
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_disturbing_plot(int player, int card, event_t event){
/*CARD_ID_DISTURBING_PLOT	9516
Disturbing Plot	|1|B
Sorcery
Return target creature card from a graveyard to its owner's hand.
Conspire (As you cast this spell, you may tap two untapped creatures you control that share a color with it.
When you do, copy it and you may choose a new target for the copy.)*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	char msg[100] = "Select a creature card.";
	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, msg);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		select_target_from_either_grave(player, card, 0, AI_MAX_VALUE, AI_MIN_VALUE, &this_test, 0, 1);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			from_grave_to_hand(instance->targets[0].player, selected, TUTOR_HAND);
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return 0;
}

int card_dramatic_entrance(int player, int card, event_t event){
/* CARD_ID_DRAMATIC_ENTRANCE	9517
Dramatic Entrance	|3|G|G
Instant
You may put a |Sgreen creature card from your hand onto the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		scnprintf(this_test.message, 100, "%s", get_sleighted_color_text(player, card, "Select a %s creature card", COLOR_GREEN));
		new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		kill_card(player, card, KILL_SACRIFICE);
	}

	return basic_spell(player, card, event);
}

int card_dream_salvage(int player, int card, event_t event){
/* CARD_ID_DREAM_SALVAGE	9518
Dream Salvage	|UB
Instant
Draw cards equal to the number of cards target opponent discarded this turn. */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( hybrid_casting(player, card, 0) ){
			return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(player, get_trap_condition(instance->targets[0].player, TRAP_DISCARDED_CARDS));
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return 0;
}

int card_drove_of_elves(int player, int card, event_t event){
  /* Drove of Elves	|3|G
   * Creature - Elf 100/100
   * Hexproof
   * ~'s power and toughness are each equal to the number of |Sgreen permanents you control. */

  hexproof(player, card, event);

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && player != -1 && ! is_humiliated(player, card) )
	event_result += count_permanents_by_color(player, TYPE_PERMANENT, get_sleighted_color_test(player, card, COLOR_TEST_GREEN));

  return 0;
}

int card_drowner_initiate(int player, int card, event_t event){
/* CARD_ID_DROWNER_INITIATE	9520
Drowner Initiate	|U
Creature - Merfolk Wizard 1/1
Whenever a player casts a |Sblue spell, you may pay |1. If you do, target player puts the top two cards of his or her library into his or her graveyard.*/

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_AI(player), &test) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE );
			td.zone = TARGET_ZONE_PLAYERS;

			card_instance_t *instance = get_card_instance( player, card );

			if( can_target(&td) && has_mana(player, COLOR_ANY, 1) &&
				charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1) )
			{
				if( new_pick_target(&td, "TARGET_PLAYER", 0, 1) ){
					mill(instance->targets[0].player, 2);
				}
			}
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_dusk_urchins(int player, int card, event_t event)
{
/* CARD_ID_DUSK_URCHINS	9521
Dusk Urchins	|2|B
Creature - Ouphe 4/3
Whenever ~ attacks or blocks, put a -1/-1 counter on it.
When ~ dies, draw a card for each -1/-1 counter on it. */

  if (declare_attackers_trigger(player, card, event, 0, player, card)
	  || (blocking(player, card, event) && !is_humiliated(player, card)))
	add_minus1_minus1_counters(player, card, 1);

  // When ~ dies, draw a card for each -1/-1 counter on it.
  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	draw_cards(player, count_minus1_minus1_counters(player, card));

  return 0;
}

int card_elemental_mastery(int player, int card, event_t event){
/* Elemental Mastery	|3|R
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature has "|T: Put X 1/1 |Sred Elemental creature tokens with haste onto the battlefield, where X is this creature's
 *							power. Exile them at the beginning of the next end step." */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		if( event == EVENT_CAN_ACTIVATE ){
			return granted_generic_activated_ability(player, card, t_player, t_card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
		if( event == EVENT_ACTIVATE ){
			granted_generic_activated_ability(player, card, t_player, t_card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				instance->info_slot = get_power(t_player, t_card);
			}
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int pow = instance->info_slot;
		if( in_play(instance->parent_controller, instance->parent_card) ){
			card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
			if( in_play(parent->damage_target_player, parent->damage_target_card) ){
				pow = get_power(parent->damage_target_player, parent->damage_target_card);
			}
		}
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
		token.qty = pow;
		token.pow = token.tou = 1;
		token.s_key_plus = SP_KEYWORD_HASTE;
		token.special_infos = 66;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		generate_token(&token);
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target creature you control.");
}

int card_elvish_hexhunter(int player, int card, event_t event){
/* CARD_ID_ELVISH_HEXHUNTER	9524
Elvish Hexhunter	|GW
Creature - Elf Shaman 1/1
|GW, |T, Sacrifice ~: Destroy target enchantment. */

	if( ! IS_GAA_EVENT(event) ){
		return hybrid(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, 0, 0, 0, 1, 0, 0);
		if( has_mana_hybrid(player, 1, COLOR_GREEN, COLOR_WHITE, c1) ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED | GAA_SACRIFICE_ME |
											GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, &td, "TARGET_ENCHANTMENT");
		}
	}

	if(event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, 0, 0, 0, 1, 0, 0);
		charge_mana_hybrid(player, card, 1, COLOR_GREEN, COLOR_WHITE, c1);
		if( spell_fizzled != 1 ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED | GAA_SACRIFICE_ME |
											GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, &td, "TARGET_ENCHANTMENT");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_enchanted_evening(int player, int card, event_t event)
{
  /* Enchanted Evening	|3|WU|WU
   * Enchantment
   * All permanents are enchantments in addition to their other types. */

	hybrid(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card) && in_play(affected_card_controller, affected_card) &&
		event == EVENT_CHANGE_TYPE && is_what(affected_card_controller, affected_card, TYPE_PERMANENT) )
	{
		set_special_flags2(affected_card_controller, affected_card, SF2_ENCHANTED_EVENING);
	}

	return global_enchantment(player, card, event);
}

int card_everlasting_torment(int player, int card, event_t event){
/* CARD_ID_EVERLASTING_TORMENT	9528
Everlasting Torment	|2|BR
Enchantment
Players can't gain life.
Damage can't be prevented.
All damage is dealt as though its source had wither. (A source with wither deals damage to creatures in the form of -1/-1 counters.)*/

	hybrid(player, card, event);

	if( ! is_humiliated(player, card) ){

		card_instance_t* damage = damage_being_dealt(event);
		if (damage){
			damage->targets[16].card |= SP_KEYWORD_WITHER;
		}

		// tentative for 'damage_cannot_be_prevented'
		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *source = get_card_instance(affected_card_controller, affected_card);
			if( damage_card == source->internal_card_id ){
				state_untargettable(affected_card_controller, affected_card, 1);
			}
		}

		if( in_play(player, card) && event == EVENT_STATIC_EFFECTS && affect_me(player, card) )
		{
			player_bits[0] |= PB_CANT_GAIN_LIFE;
			player_bits[1] |= PB_CANT_GAIN_LIFE;
		}
	}

	return global_enchantment(player, card, event);
}

int card_farie_macabre(int player, int card, event_t event){
	/* Faerie Macabre	|1|B|B
	 * Creature - Faerie Rogue 2/2
	 * Flying
	 * Discard ~: Exile up to two target cards from graveyards. */

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND  ){
		if( (count_graveyard(player) > 0 && ! graveyard_has_shroud(player)) ||
			(count_graveyard(1-player) > 0 && ! graveyard_has_shroud(1-player)) )
		{
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE_FROM_HAND ){
		discard_card(player, card);
		return 1;
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		card_instance_t *instance = get_card_instance(player, card);
		instance->targets[0].player = 1-player;
		if( count_graveyard(1-player) > 0 && ! graveyard_has_shroud(1-player) ){
			if( count_graveyard(player) > 0 && ! graveyard_has_shroud(player) ){
				target_definition_t td;
				default_target_definition(player, card, &td, TYPE_CREATURE);
				td.zone = TARGET_ZONE_PLAYERS;
				td.illegal_abilities = 0;
				td.allow_cancel = 0;

				new_pick_target(&td, "Select a graveyard owner.", 0, 0);
			}
			else{
				instance->targets[0].player = player;
			}
			int count = count_graveyard(instance->targets[0].player);
			int amount = 0;
			while( count > 0 && amount < 2 ){
					char msg[100] = "Select a card to Exile.";
					test_definition_t this_test;
					new_default_test_definition(&this_test, TYPE_ANY, msg);
					if( new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MAX_VALUE, &this_test) != -1 ){
						amount++;
						count--;
					}
					else{
						break;
					}
			}
		}
	}

	return 0;
}

int card_faerie_swarm(int player, int card, event_t event)
{
  /* Faerie Swarm	|3|U
   * Creature - Faerie 100/100
   * Flying
   * ~'s power and toughness are each equal to the number of |Sblue permanents you control. */

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && player != -1)
	event_result += count_permanents_by_color(player, TYPE_PERMANENT, get_sleighted_color_test(player, card, COLOR_TEST_BLUE));

  return 0;
}

int card_farhaven_elf(int player, int card, event_t event){
/* Farhaven Elf	|2|G
 * Creature - Elf Druid 1/1
 * When ~ enters the battlefield, you may search your library for a basic land card and put it onto the battlefield tapped.
 * If you do, shuffle your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 1);
	}

	return 0;
}

int card_fate_transfer(int player, int card, event_t event)
{
  /* Fate Transfer	|1|UB
   * Instant
   * Move all counters from target creature onto another target creature. */

  hybrid(player, card, event);

  if (!IS_CASTING(player, card, event))
	return 0;

  target_definition_t td_src;
  default_target_definition(player, card, &td_src, TYPE_CREATURE);
  td_src.allowed_controller = ANYBODY;
  td_src.special = TARGET_SPECIAL_REQUIRES_COUNTER;
  td_src.extra = 0;
  SET_BYTE0(td_src.extra) = -1;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = ANYBODY;

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CAN_CAST)
	{
	  if (IS_AI(player) && !can_target(&td_src))
		return 0;

	  return target_available(player, card, &td) >= 2;
	}

  if (event == EVENT_CAST_SPELL && affect_me(player, card) && cancel != 1)
	{
	  target_definition_t* source = IS_AI(player) ? &td_src : &td;
	  instance->number_of_targets = 0;

	  if( new_pick_target(source, "Select target creature with counters to move.", 0, 1 | GS_LITERAL_PROMPT))
		{
		  state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
		  new_pick_target(&td, "Select target creature which will receive the counters.", 1, 1 | GS_LITERAL_PROMPT);
		  state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		  if (cancel == 1)
			instance->number_of_targets = 0;
		}
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td) && validate_target(player, card, &td, 1))
		move_counters(instance->targets[1].player, instance->targets[1].card, instance->targets[0].player, instance->targets[0].card, -1, -1);
	  else if (!valid_target(&td) && !validate_target(player, card, &td, 1))	// Nothing happens if only one target is invalid; spell countered if both are
		spell_fizzled = 1;

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_fire_lit_thicket(int player, int card, event_t event){
/* CARD_ID_FIRE_LIT_THICKET	9532
Fire-Lit Thicket	""
Land
|T: Add |C to your mana pool.
|RG, |T: Add |R|R, |R|G, or |G|G to your mana pool. */

	return card_filter_land(player, card, event, COLOR_GREEN, COLOR_RED, COLOR_TEST_GREEN, COLOR_TEST_RED, " Colorless\n G -> GG\n G -> GR\n G -> RR\n R -> GG\n R -> GR\n R -> RR\n Cancel");
}

int card_firespout(int player, int card, event_t event){
/* CARD_ID_FIRESPOUT	9533
Firespout	|2|RG
Sorcery
~ deals 3 damage to each creature without flying if |R was spent to cast ~ and 3 damage to each creature with flying if |G was spent to cast it.
(Do both if |R|G was spent.) */

	if( event == EVENT_MODIFY_COST ){
		return hybrid(player, card, event);
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		hybrid(player, card, event);
		if( spell_fizzled != 1 && ! is_token(player, card) ){
			card_instance_t *instance = get_card_instance( player, card );
			if( mana_paid[COLOR_GREEN] ){
				instance->info_slot |= (1<<1);
			}
			if( mana_paid[COLOR_RED] ){
				instance->info_slot |= (1<<2);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card );
		if( instance->info_slot > 1 ){
			int color_mask = (1<<1) | (1<<2);
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			if( instance->info_slot && instance->info_slot != color_mask ){
				if( instance->info_slot & (1<<1) ){
					this_test.keyword = KEYWORD_FLYING;
				}
				if( instance->info_slot & (1<<2) ){
					this_test.keyword = KEYWORD_FLYING;
					this_test.keyword_flag = DOESNT_MATCH;
				}
			}
			new_damage_all(player, card, ANYBODY, 3, 0, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_fists_of_the_demigod(int player, int card, event_t event){
/* CARD_ID_FISTS_OF_THE_DEMIGOD	9534
Fists of the Demigod	|1|BR
Enchantment - Aura
Enchant creature
As long as enchanted creature is |Sblack, it gets +1/+1 and has wither. (It deals damage to creatures in the form of -1/-1 counters.)
As long as enchanted creature is |Sred, it gets +1/+1 and has first strike. */

	hybrid(player, card, event);
	aura_ability_for_color(player, card, event, COLOR_TEST_BLACK, 1, 1, 0, SP_KEYWORD_WITHER);
	aura_ability_for_color(player, card, event, COLOR_TEST_RED, 1, 1, KEYWORD_FIRST_STRIKE, 0);
	return vanilla_aura(player, card, event, player);
}

int card_flame_javelin(int player, int card, event_t event){
	/* Flame Javelin	|2R|2R|2R	0x2001574
	 * Instant
	 * ~ deals 4 damage to any target. */
	modify_cost_for_hybrid_spells(player, card, event, 1);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( hybrid_casting(player, card, 1) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_creature_or_player(player, card, event, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

// Flourishing defenses --> Doubling Season
int card_flourishing_defenses(int player, int card, event_t event){
	return 0;
}

int card_fracturing_gust(int player, int card, event_t event){
/* CARD_ID_FRACTURING_GUST	9538
Fracturing Gust	|2|GW|GW|GW
Instant
Destroy all artifacts and enchantments. You gain 2 life for each permanent destroyed this way. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int result = 0;
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT | TYPE_ARTIFACT);
		APNAP(p, { result += new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);
		gain_life(player, result*2);
		kill_card(player, card, KILL_DESTROY);
	}

	return hybrid(player, card, event);
}

int card_fulminator_mage(int player, int card, event_t event){
/* CARD_ID_FULMINATOR_MAGE	9539
Fulminator Mage	|1|BR|BR
Creature - Elemental Shaman 2/2
Sacrifice ~: Destroy target nonbasic land. */

	if( ! IS_GAA_EVENT(event) ){
		return hybrid(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = SUBTYPE_BASIC;
	td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target non-basic land.");
}
static int tap_and_damage(int player, int card, event_t event){

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
		damage_target0(player, card, 2);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ANY");
}

static int furystoke_giant_effect(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_CLEANUP ){
			kill_card(player, card, KILL_EXILE);
		}

		return tap_and_damage(p, c, event);
	}
	return 0;
}

int card_furystoke_giant(int player, int card, event_t event){
	/* Furystoke Giant	|3|R|R	0x2007cfd
	 * Creature - Giant Warrior 3/3
	 * When ~ enters the battlefield, other creatures you control gain "|T: This creature deals 2 damage to any target" until end of turn.
	 * Persist */
	if( comes_into_play(player, card, event) ){
		int c;
		for(c = active_cards_count[player]-1; c > -1; c--){
			if( in_play(player, c) && c != card && is_what(player, c, TYPE_CREATURE) ){
				create_targetted_legacy_activate(player, card, &furystoke_giant_effect, player, c);
			}
		}
	}

	persist(player, card, event);

	return 0;
}

int card_ghastlord_of_fugue(int player, int card, event_t event){
/* CARD_ID_GHASTLORD_OF_FUGUE	9541
Ghastlord of Fugue	|UB|UB|UB|UB|UB
Creature - Spirit Avatar 4/4
~ can't be blocked.
Whenever ~ deals combat damage to a player, that player reveals his or her hand. You choose a card from it. That player exiles that card.*/

	unblockable(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					while( times_damaged[p] && hand_count[p] ){
							new_global_tutor(player, p, TUTOR_FROM_HAND, TUTOR_RFG, 1, AI_MAX_VALUE, &this_test);
							times_damaged[p]--;
					}
				}
		);
	}


	return hybrid(player, card, event);
}

int card_ghastly_discovery(int player, int card, event_t event){
/* CARD_ID_GHASTLY_DISCOVERY	9542
Ghastly Discovery	|2|U
Sorcery
Draw two cards, then discard a card.
Conspire (As you cast this spell, you may tap two untapped creatures you control that share a color with it. When you do, copy it.)*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 24;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		discard(player, 0, player);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// Giant warrior token --> rhino token.

int card_giantbaiting(int player, int card, event_t event){
	/* Giantbaiting	|2|RG
	 * Sorcery
	 * Put a 4/4 |Sred and |Sgreen Giant Warrior creature token with haste onto the battlefield. Exile it at the beginning of the next end step.
	 * Conspire */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_GIANT_WARRIOR, &token);
		token.pow = 4;
		token.tou = 4;
		token.color_forced = COLOR_TEST_GREEN | COLOR_TEST_RED;
		token.special_code_on_generation = &haste_and_remove_token_at_end_of_next_turn;
		generate_token(&token);
		kill_card(player, card, KILL_DESTROY);
	}

	return hybrid(player, card, event);
}

// gleeful sabotage --> disenchant

int card_glen_elendra_liege(int player, int card, event_t event){
/* CARD_ID_GLEN_ELENDRA_LIEGE	9546
Glen Elendra Liege	|1|UB|UB|UB
Creature - Faerie Knight 2/3
Flying
Other |Sblue creatures you control get +1/+1.
Other |Sblack creatures you control get +1/+1. */

	return liege(player, card, event, COLOR_TEST_BLACK, COLOR_TEST_BLUE);
}

int card_gnarled_effigy(int player, int card, event_t event){
/* CARD_ID_GNARLED_EFFIGY	9550
Gnarled Effigy	|4
Artifact
|4, |T: Put a -1/-1 counter on target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(4), 0, &td, "TARGET_CREATURE");
}

int card_godhead_of_awe(int player, int card, event_t event){
/* CARD_ID_GODHEAD_OF_AWE	9551
Godhead of Awe	|WU|WU|WU|WU|WU
Creature - Spirit Avatar 4/4
Flying
Other creatures have base power and toughness 1/1. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_POWER && ! affect_me(player, card) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			event_result += (1 - get_base_power(affected_card_controller, affected_card));
		}
		if( event == EVENT_TOUGHNESS && ! affect_me(player, card) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			event_result += (1 - get_base_toughness(affected_card_controller, affected_card));
		}
	}

	return hybrid(player, card, event);
}

int card_graven_cairns(int player, int card, event_t event){
/* CARD_ID_GRAVEN_CAIRNS	8938
Graven Cairns	""
Land
|T: Add |C to your mana pool.
|BR, |T: Add |B|B, |B|R, or |R|R to your mana pool. */

	return card_filter_land(player, card, event, COLOR_BLACK, COLOR_RED, COLOR_TEST_BLACK, COLOR_TEST_RED, " Colorless\n B -> BB\n B -> BR\n B -> RR\n R -> BB\n R -> BR\n R -> RR\n Cancel");
}

int card_greater_auramancy(int player, int card, event_t event){
/* CARD_ID_GREATER_AURAMANCY	9555
Greater Auramancy	|1|W
Enchantment
Other enchantments you control have shroud. (A permanent with shroud can't be the target of spells or abilities.)
Enchanted creatures you control have shroud. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_ABILITIES && ! affect_me(player, card) && affected_card_controller == player ){
			if( is_what(affected_card_controller, affected_card, TYPE_ENCHANTMENT) ){
				event_result |= KEYWORD_SHROUD;
			}
			if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) && is_enchanted(affected_card_controller, affected_card) ){
				event_result |= KEYWORD_SHROUD;
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_grief_tyrant(int player, int card, event_t event){
/* CARD_ID_GRIEF_TYRANT	9556
Grief Tyrant	|5|BR
Creature - Horror 8/8
~ enters the battlefield with four -1/-1 counters on it.
When ~ dies, put a -1/-1 counter on target creature for each -1/-1 counter on ~. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_M1_M1, 4);

	if( this_dies_trigger(player, card, event, 2) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, count_minus1_minus1_counters(player, card));
		}

	}

	return hybrid(player, card, event);
}

int card_grim_poppet(int player, int card, event_t event){
/* CARD_ID_GRIM_POPPET	9557
Grim Poppet	|7
Artifact Creature - Scarecrow 4/4
~ enters the battlefield with three -1/-1 counters on it.
Remove a -1/-1 counter from ~: Put a -1/-1 counter on another target creature. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_M1_M1, 3);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, GVC_COUNTER(COUNTER_M1_M1), &td, "TARGET_CREATURE");
}

int card_heap_doll(int player, int card, event_t event){
/* CARD_ID_HEAP_DOLL	9559
Heap Doll	|1
Artifact Creature - Scarecrow 1/1
Sacrifice ~: Exile target card from a graveyard. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select target card to exile.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int result = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( result > -1 ){
			rfg_card_from_grave(instance->targets[0].player, result);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER_BOTH_GRAVES);
}

int card_heartmender(int player, int card, event_t event)
{
  /* Heartmender	|2|GW|GW
   * Creature - Elemental 2/2
   * At the beginning of your upkeep, remove a -1/-1 counter from each creature you control.
   * Persist */

  persist(player, card, event);

  upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	manipulate_type(player, card, player, TYPE_CREATURE, ACT_REMOVE_COUNTERS(COUNTER_M1_M1, 1));

  return hybrid(player, card, event);
}

int card_helm_of_the_ghastlord(int player, int card, event_t event)
{
  /* Helm of the Ghastlord	|3|UB
   * Enchantment - Aura
   * Enchant creature
   * As long as enchanted creature is |Sblue, it gets +1/+1 and has "Whenever this creature deals damage to an opponent, draw a card."
   * As long as enchanted creature is |Sblack, it gets +1/+1 and has "Whenever this creature deals damage to an opponent, that player discards a card." */

  hybrid(player, card, event);

  aura_ability_for_color(player, card, event, COLOR_TEST_BLACK, 1, 1, 0, 0);
  aura_ability_for_color(player, card, event, COLOR_TEST_BLUE, 1, 1, 0, 0);

  if (in_play(player, card) && get_card_instance(player, card)->damage_target_card > -1 && ! is_humiliated(player, card) && IS_DDBM_EVENT(event))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->damage_target_card >= 0 && !is_humiliated(instance->damage_target_player, instance->damage_target_card))
		{
		  int clr = get_color(instance->damage_target_player, instance->damage_target_card);
		  int blue = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		  int black = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		  int packets;
		  if ((clr & (blue|black))
			  && (packets = attached_creature_deals_damage(player, card, event, DDBM_MUST_DAMAGE_OPPONENT)))
			for (; packets > 0; --packets)
			  {
				if (clr & blue)
				  draw_a_card(player);

				if (clr & black)
				  discard(1-player, 0, player);
			  }
		}
	}

  return vanilla_aura(player, card, event, player);
}

int card_hollowborn_barghest(int player, int card, event_t event){
/*
Hollowborn Barghest |5|B|B
Creature — Demon Hound 7/6
At the beginning of your upkeep, if you have no cards in hand, each opponent loses 2 life.
At the beginning of each opponent's upkeep, if that player has no cards in hand, he or she loses 2 life.
*/
	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( current_turn == player && ! hand_count[player] ){
			lose_life(1-player, 2);
		}
		if( current_turn == 1-player && ! hand_count[1-player] ){
			lose_life(1-player, 2);
		}
	}

	return 0;
}

int card_hollowsage(int player, int card, event_t event)
{
/* CARD_ID_HOLLOWSAGE	9563
Hollowsage	|3|B
Creature - Merfolk Wizard 2/2
Whenever ~ becomes untapped, you may have target player discard a card. */

  if (event == EVENT_UNTAP_CARD && affect_me(player, card) && !is_humiliated(player, card))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, 0);
	  td.zone = TARGET_ZONE_PLAYERS;

	  card_instance_t* instance = get_card_instance(player, card);

	  instance->number_of_targets = 0;
	  if (can_target(&td) && pick_target(&td, "TARGET_PLAYER"))
		discard(instance->targets[0].player, 0, player);
	}

  return 0;
}

int card_horde_of_boggarts(int player, int card, event_t event)
{
  /* Horde of Boggarts	|3|R
   * Creature - Goblin 100/100
   * Menace
   * ~'s power and toughness are each equal to the number of |Sred permanents you control. */

  menace(player, card, event);

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && player != -1 && ! is_humiliated(player, card) )
	event_result += count_permanents_by_color(player, TYPE_PERMANENT, get_sleighted_color_test(player, card, COLOR_TEST_RED));

  return 0;
}

int card_howl_of_the_night_pack(int player, int card, event_t event){
	/* Howl of the Night Pack	|6|G
	 * Sorcery
	 * Put a 2/2 |Sgreen Wolf creature token onto the battlefield for each |H2Forest you control. */

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_FOREST));
		generate_tokens_by_id(player, card, CARD_ID_WOLF, amount);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_hungry_spriggan(int player, int card, event_t event)
{
	/* Hungry Spriggan	|2|G	0x200ec7e
	 * Creature - Goblin Warrior 1/1
	 * Trample
	 * Whenever ~ attacks, it gets +3/+3 until end of turn. */

	return when_attacks_pump_self(player, card, event, 3,3);
}

int card_improptu_raid(int player, int card, event_t event){
/* CARD_ID_IMPROMPTU_RAID	9568
Impromptu Raid	|3|RG
Enchantment
|2|RG: Reveal the top card of your library. If it isn't a creature card, put it into your graveyard. Otherwise, put that card onto the battlefield.
That creature gains haste. Sacrifice it at the beginning of the next end step. */

	hybrid(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_XR(2, 1));
		if( has_mana_hybrid(player, 1, COLOR_GREEN, COLOR_RED, c1) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_XR(2, 1));
		if( charge_mana_hybrid(player, card, 1, COLOR_GREEN, COLOR_RED, c1) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			if( is_what(-1, deck[0], TYPE_CREATURE)  ){
				int card_added = put_into_play_a_card_from_deck(player, player, 0);
				if( card_added > -1 ){
					haste_and_remove_at_beginning_of_next_end_step(player, card, player, card_added, KILL_SACRIFICE);
				}
			}
			else{
				mill(player, 1);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_incremental_blight(int player, int card, event_t event){
/* CARD_ID_INCREMENTAL_BLIGHT	9569
Incremental Blight	|3|B|B
Sorcery
Put a -1/-1 counter on target creature, two -1/-1 counters on another target creature, and three -1/-1 counters on a third target creature.*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 3, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		char buffer[100];
		int i;
		for (i = 0; i < 3; ++i){
			scnprintf(buffer, 100, "Select target creature (%d -1/-1 counters)", i+1 );
			if ( select_target(player, card, &td, buffer, &instance->targets[i])){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
			}
			else{
				break;
			}
		}
		for(i=0; i<instance->number_of_targets; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
		}
		if( instance->number_of_targets < 3 ){
			spell_fizzled = 1;
		}
	}

	if(event == EVENT_RESOLVE_SPELL){
		int i;
		for(i=0; i<3; i++){
			if( validate_target(player, card, &td, i) ){
				add_minus1_minus1_counters(instance->targets[i].player, instance->targets[i].card, i+1);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_inkfathom_infiltrator(int player, int card, event_t event){
/* CARD_ID_INKFATHOM_INFILTRATOR	9571
Inkfathom Infiltrator	|UB|UB
Creature - Merfolk Rogue 2/1
~ can't block and can't be blocked. */

	cannot_block(player, card, event);
	unblockable(player, card, event);
	return hybrid(player, card, event);
}

int card_inkfathom_witch(int player, int card, event_t event){
/* CARD_ID_INKFATHOM_WITCH	9572
Inkfathom Witch	|1|UB
Creature - Merfolk Wizard 1/1
Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
|2|U|B: Each unblocked creature has base power and toughness 4/1 until end of turn. */

	fear(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return hybrid(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int c;
		for(c=active_cards_count[current_turn]-1; c > -1; c--){
			if( in_play(current_turn, c) && is_what(current_turn, c, TYPE_CREATURE) && is_attacking(current_turn, c) &&
				is_unblocked(current_turn, c) )
			{
				set_pt_and_abilities_until_eot(instance->parent_controller, instance->parent_card, current_turn, c, 4, 1, 0, 0, 1);
			}
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XUB(2, 1, 1), 0, NULL, NULL);
}

int card_intimidator_initiate(int player, int card, event_t event){
/* CARD_ID_INTIMIDATOR_INITIATE	9574
Intimidator Initiate	|R
Creature - Goblin Shaman 1/1
Whenever a player casts a |Sred spell, you may pay |1. If you do, target creature can't block this turn. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);

		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_AI(player), &test) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE );

			card_instance_t *instance = get_card_instance( player, card );

			if( can_target(&td) && has_mana(player, COLOR_ANY, 1) &&
				charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1) )
			{
				if( pick_target(&td, "TARGET_CREATURE") ){
					pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
											0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
				}
			}
			instance->number_of_targets = 0;
		}

	}

	return 0;
}

int card_isleback_spawn(int player, int card, event_t event){
/* CARD_ID_ISLEBACK_SPAWN	9575
Isleback Spawn	|5|U|U
Creature - Kraken 4/8
Shroud (This creature can't be the target of spells or abilities.)
~ gets +4/+8 as long as a library has twenty or fewer cards in it. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card) ){
		if( deck_ptr[player][20] == -1 || deck_ptr[1-player][20] == -1 ){
			if( event == EVENT_POWER ){
				event_result += 4;
			}
			if( event == EVENT_TOUGHNESS ){
				event_result += 8;
			}
		}
	}

	return 0;
}

int card_juvenile_gloomwidow(int player, int card, event_t event){
/* CARD_ID_JUVENILE_GLOOMWIDOW	9577
Juvenile Gloomwidow	|G|G
Creature - Spider 1/3
Reach (This creature can block creatures with flying.)
Wither (This deals damage to creatures in the form of -1/-1 counters.) */

	wither(player, card, event);
	return 0;
}

int card_kitchen_finks(int player, int card, event_t event){
/* CARD_ID_KITCHEN_FINKS	9579
Kitchen Finks	|1|GW|GW
Creature - Ouphe 3/2
When ~ enters the battlefield, you gain 2 life.
Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.)*/

	hybrid(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 2);
	}

	persist(player, card, event);

	return 0;
}

int card_kithkin_rabble(int player, int card, event_t event){
  /* Kithkin Rabble	|3|W
   * Creature - Kithkin 100/100
   * Vigilance
   * ~'s power and toughness are each equal to the number of |Swhite permanents you control. */

  vigilance(player, card, event);

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && player != -1 && ! is_humiliated(player, card) )
	event_result += count_permanents_by_color(player, TYPE_PERMANENT, get_sleighted_color_test(player, card, COLOR_TEST_WHITE));

  return 0;
}

int card_knacksaw_clique(int player, int card, event_t event)
{
  /* Knacksaw Clique	|3|U
   * Creature - Faerie Rogue 1/4
   * Flying
   * |1|U, |Q: Target opponent exiles the top card of his or her library. Until end of turn, you may play that card. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		int iid = deck_ptr[1-player][0];
		if (iid != -1){
			int csvid = cards_data[iid].id;
			rfg_top_card_of_deck(1-player);
			create_may_play_card_from_exile_effect(player, card, 1-player, csvid, MPCFE_UNTIL_EOT);
		}
	}

	return generic_activated_ability(player, card, event, GAA_TAPPED|GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST_XU(1,1), 0, &td, "TARGET_OPPONENT");
}

int card_knollspine_dragon(int player, int card, event_t event){
/*
Knollspine Dragon |5|R|R
Creature — Dragon 7/5
Flying
When Knollspine Dragon enters the battlefield, you may discard your hand and draw cards equal to the damage dealt to target opponent
	this turn.
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		int ai_mode = would_validate_arbitrary_target(&td, 1-player, -1) &&
						get_trap_condition(1-player, TRAP_DAMAGE_TAKEN) > hand_count[player] ?
						RESOLVE_TRIGGER_MANDATORY : 0;

		int trigger_resolution = would_validate_arbitrary_target(&td, 1-player, -1) ?
								(player == HUMAN ? RESOLVE_TRIGGER_OPTIONAL : ai_mode) : 0;

		etb_ability(player, card, event, trigger_resolution);
	}

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		if( would_validate_arbitrary_target(&td, 1-player, -1) ){
			int to_draw = get_trap_condition(1-player, TRAP_DAMAGE_TAKEN);
			discard_all(player);
			draw_cards(player, to_draw);
		}
	}

	return 0;
}

int card_knollspine_invocation(int player, int card, event_t event){
	/* Knollspine Invocation	|1|R|R	0x2007d84
	 * Enchantment
	 * |X, Discard a card with converted mana cost X: ~ deals X damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_hand(p, c) ){
										int cmc = get_cmc(p, c);
										if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(cmc), 0, &td, NULL) )
										{
											return 1;
										}
									}
								}
		);
	}

	if( event == EVENT_ACTIVATE ){
		int hand_copy[2][hand_count[player]];
		int hcc = 0;
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_hand(p, c) ){
										int cmc = get_cmc(p, c);
										if( has_mana_for_activated_ability(player, card, MANACOST_X(cmc)) ){
											hand_copy[0][hcc] = get_original_internal_card_id(p, c);
											hand_copy[1][hcc] = c;
											hcc++;
										}
									}
								}
		);
		if( ! hcc ){
			spell_fizzled = 1;
			return 0;
		}

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card to discard");
		int selected = select_card_from_zone(player, player, hand_copy[0], hcc, 0, AI_MAX_CMC, -1, &this_test);
		if( selected == -1 ){
			spell_fizzled = 1;
			return 0;
		}
		int cmc = get_cmc(player, hand_copy[1][selected]);
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(cmc), 0, &td, "TARGET_ANY");
		if( spell_fizzled != 1 ){
			instance->info_slot = cmc;
			discard_card(player, hand_copy[1][selected]);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
	}

	return 0;
}

int card_kulrath_knight(int player, int card, event_t event)
{
  /* Kulrath Knight	|3|BR|BR
   * Creature - Elemental Knight 3/3
   * Flying
   * Wither
   * Creatures your opponents control with counters on them can't attack or block. */

  wither(player, card, event);

  if ((event == EVENT_ATTACK_LEGALITY || event == EVENT_BLOCK_LEGALITY) && affected_card_controller != player
	  && count_counters(affected_card_controller, affected_card, -1))
	event_result = 1;

  return hybrid(player, card, event);
}

int card_leech_bonder(int player, int card, event_t event)
{
  /* Leech Bonder	|2|U
   * Creature - Merfolk Soldier 3/3
   * ~ enters the battlefield with two -1/-1 counters on it.
   * |U, |Q: Move a counter from target creature onto another target creature. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_M1_M1, 2);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td_src;
	default_target_definition(player, card, &td_src, TYPE_CREATURE);
	td_src.allowed_controller = ANYBODY;
	td_src.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td_src.extra = 0;
	SET_BYTE0(td_src.extra) = -1;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_CAN_ACTIVATE)
	{
		int result = generic_activated_ability(player, card, event, GAA_TAPPED, MANACOST_U(1), 0, NULL, NULL);
		if( result ){
			if( can_target(&td_src) && can_target(&td) ){
				return 1;
			}
			if( target_available(player, card, &td) >= 2 ){
				return player == AI ? 0 : 1;
			}
		}
	}

	if (event == EVENT_ACTIVATE )
	{
		target_definition_t* source = IS_AI(player) ? &td_src : &td;
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_U(1), 0, source, "TARGET_CREATURE");
		if ( spell_fizzled != 1 )
		{
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			new_pick_target(&td, "TARGET_ANOTHER_CREATURE", 1, 1);
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
			if (cancel == 1)
				instance->number_of_targets = 0;
			else
				untap_card(player, card);
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION)
	{
		if (validate_target(player, card, &td, 0) && validate_target(player, card, &td, 1))
		{
			counter_t type = choose_existing_counter_type(player, instance->parent_controller, instance->parent_card,
															instance->targets[0].player, instance->targets[0].card, CECT_MOVE, player, card);
			if (type == COUNTER_invalid)	// were no counters to move - this is actually legal, and doesn't cause the effect to fizzle
			{
				spell_fizzled = 0;	// since choose_existing_counter_type() sets it
			}
			else
			{
				move_counters(instance->targets[1].player, instance->targets[1].card,
								instance->targets[0].player, instance->targets[0].card, type, 1);
			}
		}
		else if (!validate_target(player, card, &td, 0) && !validate_target(player, card, &td, 1))
				// Nothing happens if only one target is invalid; spell countered if both are
				spell_fizzled = 1;
	}

  return 0;
}

int card_leechridden_swamp(int player, int card, event_t event){

	/* Leechridden Swamp	""
	 * Land - |HSwamp
	 * ~ enters the battlefield tapped.
	 * |B, |T: Each opponent loses 1 life. Activate this ability only if you control two or more |Sblack permanents. */

	comes_into_play_tapped(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_LOSE_LIFE
	};

	if( event == EVENT_ACTIVATE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		int can_use_ability = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test) >= 2 &&
								generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_B(2), 0, NULL, NULL) ?
								1 : 0;
		int choice = CHOICE_MANA;
		if( ! paying_mana() && can_use_ability ){
			choice = CHOICE_LOSE_LIFE;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) && choice == CHOICE_LOSE_LIFE ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Opponent loses 1 life", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_LOSE_LIFE ){
			add_state(player, card, STATE_TAPPED);
			charge_mana_for_activated_ability(player, card, MANACOST_B(1));
			if (spell_fizzled == 1){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_LOSE_LIFE ){
			lose_life(1-player, 1);
		}
	}

	return 0;
}

int card_lockjaw_snapper(int player, int card, event_t event){
/* CARD_ID_LOCKJAW_SNAPPER	9590
Lockjaw Snapper	|4
Artifact Creature - Scarecrow 2/2
Wither (This deals damage to creatures in the form of -1/-1 counters.)
When ~ dies, put a -1/-1 counter on each creature with a -1/-1 counter on it. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		int i;
		for(i=0; i<2; i++){
			int count = active_cards_count[i]-1;
			while( count > -1 ){
					if( in_play(i, count) && is_what(i, count, TYPE_PERMANENT) && count_minus1_minus1_counters(i, count) > 0 ){
						add_minus1_minus1_counters(i, count, 1);
					}
					count--;
			}
		}
	}

	wither(player, card, event);

	return 0;
}

int card_lurebound_scarecrow(int player, int card, event_t event){

	/* Lurebound Scarecrow	|3
	 * Artifact Creature - Scarecrow 4/4
	 * As ~ enters the battlefield, choose a color.
	 * When you control no permanents of the chosen color, sacrifice ~. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->targets[1].player = 1 << choose_a_color_and_show_legacy(player, card, player, -1);
	}

	if( event == EVENT_STATIC_EFFECTS ){
		if (get_card_instance(player, card)->targets[1].player <= 0){
			// no color chosen, so never sacrificed per ruling
			return 0;
		}
		target_definition_t td;
		base_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.required_color = get_card_instance(player, card)->targets[1].player;

		if( !can_target(&td) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_madblind_mountain(int player, int card, event_t event){
	/* Madblind Mountain	""
	 * Land - |H2Mountain
	 * ~ enters the battlefield tapped.
	 * |R, |T: Shuffle your library. Activate this ability only if you control two or more |Sred permanents. */

	comes_into_play_tapped(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_SHUFFLE
	};

	if( event == EVENT_ACTIVATE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		int can_use_ability = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test) >= 2 &&
								generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_R(2), 0, NULL, NULL) ?
								1 : 0;
		int choice = CHOICE_MANA;
		if( ! paying_mana() && can_use_ability ){
			choice = CHOICE_SHUFFLE;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) && choice == CHOICE_SHUFFLE ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Shuffle", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_SHUFFLE ){
			add_state(player, card, STATE_TAPPED);
            charge_mana_for_activated_ability(player, card, MANACOST_R(1));
			if (spell_fizzled == 1){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SHUFFLE ){
			shuffle(player);
		}
	}

	return 0;
}

int card_mana_reflection(int player, int card, event_t event){
/* CARD_ID_MANA_REFLECTION	9593
Mana Reflection	|4|G|G
Enchantment
If you tap a permanent for mana, it produces twice as much of that mana instead. */

	if( in_play(player, card) && is_humiliated(player, card) ){
		return 0;
	}

	if (event == EVENT_TAP_CARD && affected_card_controller == player){
		color_t c;
		switch (tapped_for_mana_color){
			case 0x100:
				for (c = COLOR_COLORLESS; c <= COLOR_ARTIFACT; ++c){
					if (tapped_for_mana[c] > 0){
						produce_mana(affected_card_controller, c, tapped_for_mana[c]);
						tapped_for_mana[c] *= 2;
					}
				}
				break;

			case COLOR_COLORLESS ... COLOR_ARTIFACT:
				produce_mana(affected_card_controller, tapped_for_mana_color, 1);
				for (c = COLOR_COLORLESS; c <= COLOR_ARTIFACT; ++c){
					tapped_for_mana[c] = 0;
				}
				tapped_for_mana[tapped_for_mana_color] = 2;
				tapped_for_mana_color = 0x100;
				break;
		}
		return 0;
	}

	if (event == EVENT_COUNT_MANA && affected_card_controller == player && !is_tapped(affected_card_controller, affected_card)
		&& !is_animated_and_sick(affected_card_controller, affected_card) && can_produce_mana(affected_card_controller, affected_card)){
		/* Count an extra time.  This is an approximation; it'll give somewhat incorrect results for sources that can produce different colors (e.g. Badlands -
		 * it'll say BR is produceable, when only BB or RR is), and very incorrect results for multiple Mana Reflections (two copies should quadruple mana; this
		 * will say it's tripled).  This will also claim to increase mana from permanents flagged as mana sources that produce mana without tapping, like
		 * Cadaverous Bloom, and that don't trigger tapped-for-mana effects despite tapping and producing mana, like Heritage Druid and Seton, Krosan Protector.
		 * The actual mana produced is correct in all cases.
		 *
		 * Tappedness, summoning sickness, and can_produce_mana() will all be checked by the called card's function, too, for cases where Mana Reflection
		 * eventually will produce mana; the extra checks above help weed out a couple of the cases where it won't. */
		card_instance_t* instance = get_card_instance(affected_card_controller, affected_card);
		card_data_t* cd = cards_at_7c7000[instance->internal_card_id];
		if (cd->extra_ability & EA_MANA_SOURCE){
			call_card_function(affected_card_controller, affected_card, EVENT_COUNT_MANA);
		}
	}

	return global_enchantment(player, card, event);
}

int card_manaforge_cinder(int player, int card, event_t event){
/* CARD_ID_MANAFORGE_CINDER	9594
Manaforge Cinder	|BR
Creature - Elemental Shaman 1/1
|1: Add |B or |R to your mana pool. Activate this ability no more than three times each turn. */
    hybrid(player, card, event);

	if( event == EVENT_CAN_ACTIVATE && has_mana(player, COLOR_COLORLESS, 1) && get_card_instance(player, card)->targets[1].player < 3 ){
		return can_produce_mana(player, card);
	}

	if(event == EVENT_ACTIVATE ){
		charge_mana(player, COLOR_COLORLESS, 1);
		if( spell_fizzled != 1 ){
			get_card_instance(player, card)->targets[1].player++;
			return mana_producer(player, card, event);
		}
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && has_mana(player, COLOR_COLORLESS, 1) &&
		get_card_instance(player, card)->targets[1].player < 3 )
	{
		return mana_producer(player, card, event);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	return 0;
}

int card_manamorphose(int player, int card, event_t event){
/* CARD_ID_MANAMORPHOSE	9595
Manamorphose	|1|RG
Instant
Add two mana in any combination of colors to your mana pool.
Draw a card. */

	hybrid(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 48;
	}

	if(event == EVENT_RESOLVE_SPELL ){
		FORCE(produce_mana_any_combination_of_colors(player, COLOR_TEST_ANY_COLORED, 2, NULL));
		draw_a_card(player);
		kill_card( player, card, KILL_SACRIFICE);
	}

	return basic_spell(player, card, event);
}

int card_mass_calcify(int player, int card, event_t event){
/* CARD_ID_MASS_CALCIFY	9596
Mass Calcify	|5|W|W
Sorcery
Destroy all non|Swhite creatures. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		this_test.color_flag = DOESNT_MATCH;
		new_manipulate_all(player, card, 2, &this_test, KILL_DESTROY);
		kill_card( player, card, KILL_SACRIFICE);
	}

	return basic_spell(player, card, event);
}

static const char* target_must_have_counters(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  return count_counters(player, card, -1) ? NULL : "must have counters";
}
int card_medicine_runner(int player, int card, event_t event)
{
  /* Medicine Runner	|1|GW
   * Creature - Elf Cleric 2/1
   * When ~ enters the battlefield, you may remove a counter from target permanent. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY )
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.preferred_controller = ANYBODY;
		if (IS_AI(player))
		{
		/* Prevents the AI from considering removal of counters from permanents without them (thus speculating more on which type of
		 * counter to remove from permanents that *do* have them).  However, it also prevents it from using this ability to kill a Skulking
		 * Ghost or activate another whenever-this-becomes-targeted trigger. I think this is an acceptable tradeoff. */
			td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
			td.extra = (int)target_must_have_counters;
		}

		counter_t typ;
		card_instance_t *instance = get_card_instance(player, card);
		instance->number_of_targets = 0;
		if (can_target(&td) && pick_target(&td, "TARGET_PERMANENT")
			&& ((typ = choose_existing_counter_type(player, player, card, instance->targets[0].player, instance->targets[0].card,
													CECT_HUMAN_CAN_CANCEL | CECT_REMOVE, -1, -1)) != COUNTER_invalid))
			remove_counter(instance->targets[0].player, instance->targets[0].card, typ);
		else
			instance->number_of_targets = 0;
	}

	return hybrid(player, card, event);
}

int card_memory_plunder(int player, int card, event_t event){

	/* Memory Plunder	|UB|UB|UB|UB
	 * Instant
	 * You may cast target instant or sorcery card from an opponent's graveyard without paying its mana cost. */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_SORCERY | TYPE_INSTANT, "Select target instant or sorcery card.");

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, 1-player, 1);
		if( selected != -1 ){
			play_card_in_grave_for_free(player, 1-player, selected);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_OPP_GRAVE, NULL, NULL, 1, &test);
}

int card_mercy_killing(int player, int card, event_t event){
/* Mercy Killing	|2|GW
 * Instant
 * Target creature's controller sacrifices it, then puts X 1/1 |Sgreen and |Swhite Elf Warrior creature tokens onto the battlefield,
 *		where X is that creature's power. */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( hybrid_casting(player, card, 0) ){
			generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
			if( spell_fizzled !=1 ){
				int mod = 0;
				if( instance->targets[0].player == player ){
					mod = -48;
					if( current_turn == 1-player && check_state(instance->targets[0].player, instance->targets[0].card, STATE_BLOCKING) ){
						mod = 48;
					}
				}
				if( instance->targets[0].player == 1-player ){
					mod = 48;
					if( current_turn == player && check_state(instance->targets[0].player, instance->targets[0].card, STATE_BLOCKING) ){
						mod = -48;
					}
				}
				ai_modifier += mod;
			}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int elves = get_power(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ELF_WARRIOR, &token);
			token.t_player = instance->targets[0].player;
			token.qty = elves;
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_GREEN | COLOR_TEST_WHITE);
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_midnight_banshee(int player, int card, event_t event){

	/* Midnight Banshee	|3|B|B|B
	 * Creature - Spirit 5/5
	 * Wither
	 * At the beginning of your upkeep, put a -1/-1 counter on each non|Sblack creature. */

	wither(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "");
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		test.color_flag = DOESNT_MATCH;
		new_manipulate_all(player, card, ANYBODY, &test, ACT_ADD_COUNTERS(COUNTER_M1_M1, 1));
	}

	return 0;
}

int card_mine_excavation(int player, int card, event_t event){
/* CARD_ID_MINE_EXCAVATION	9604
Mine Excavation	|1|W
Sorcery
Return target artifact or enchantment card from a graveyard to its owner's hand.
Conspire (As you cast this spell, you may tap two untapped creatures you control that share a color with it.
When you do, copy it and you may choose a new target for the copy.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ARTIFACT | TYPE_ENCHANTMENT, "Select an Artifact or Enchantment card.");

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card );
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			const int *grave = get_grave(instance->targets[0].player);
			add_card_to_hand(instance->targets[0].player, grave[selected]);
			remove_card_from_grave(instance->targets[0].player, selected);
		}
		kill_card(player, card, KILL_SACRIFICE);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
}


static int test_exclude_this_creature(int iid, int val, int player, int card)
{
  return !(card == LOWORD(val) && player == HIWORD(val));
}
int card_mirrorweave(int player, int card, event_t event)
{
  /* Mirrorweave	|2|WU|WU
   * Instant
   * Each other creature becomes a copy of target nonlegendary creature until end of turn. */

  // Shapeshifts all cards matching test and controlled by t_player (or ANYBODY).
  // void shapeshift_all(int src_player, int src_card, int t_player, test_definition_t* test, int turn_into_player, int turn_into_card, int mode)

  hybrid(player, card, event);

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;
  td.required_subtype = SUBTYPE_LEGEND;
  td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* instance = get_card_instance(player, card);

		  test_definition_t test;
		  new_default_test_definition(&test, TYPE_CREATURE, "");
		  test.special_selection_function = &test_exclude_this_creature;
		  test.value_for_special_selection_function = (instance->targets[0].card & 0xFFFF) | ((instance->targets[0].player & 0xFFFF) << 16);

		  shapeshift_all(player, card, ANYBODY, &test, instance->targets[0].player, instance->targets[0].card, SHAPESHIFT_UNTIL_EOT);
		}

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_NONLEGENDARY_CREATURE", 1, NULL);
}

int card_mistmeadow_witch(int player, int card, event_t event){
/* CARD_ID_MISTMEADOW_WITCH	9606
Mistmeadow Witch	|1|WU
Creature - Kithkin Wizard 1/1
|2|W|U: Exile target creature. Return that card to the battlefield under its owner's control at the beginning of the next end step. */

	if( ! IS_GAA_EVENT(event) ){
		return hybrid(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			remove_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XUW(2, 1, 1), 0, &td, "TARGET_CREATURE");
}

int card_mistveil_plains(int player, int card, event_t event){
/* Mistveil Plains	""
 * Land - |HPlains
 * ~ enters the battlefield tapped.
 * |W, |T: Put target card from your graveyard on the bottom of your library. Activate this ability only if you control two or more
 *		|Swhite permanents. */

	comes_into_play_tapped(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_RECYCLE
	};

	if( event == EVENT_ACTIVATE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);

		test_definition_t test2;
		new_default_test_definition(&test2, TYPE_ANY, "Select target card.");

		int can_use_ability = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test) >= 2 &&
								generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED,
																				MANACOST_W(2), 0, NULL, NULL,
																				0, &test2, GS_GRAVE_RECYCLER) ?
																				1 : 0;
		int choice = CHOICE_MANA;
		if( ! paying_mana() && can_use_ability ){
			choice = CHOICE_RECYCLE;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) && choice == CHOICE_RECYCLE ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Recycle grave", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_RECYCLE ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST_W(1), 0, NULL, NULL,
	                                                             0, &test2, GS_GRAVE_RECYCLER);
			if (spell_fizzled == 1){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_RECYCLE ){
			int selected = validate_target_from_grave_source(player, card, player, 0);
			if( selected > -1 ){
				deck_ptr[player][count_deck(player)] = get_grave(player)[selected];
				remove_card_from_grave(player, selected);
			}
		}
	}

	return 0;
}

int card_moonring_island(int player, int card, event_t event){
	/* Moonring Island	""
	 * Land - |H2Island
	 * ~ enters the battlefield tapped.
	 * |U, |T: Look at the top card of target player's library. Activate this ability only if you control two or more |Sblue permanents. */

	comes_into_play_tapped(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_LOOK_TOP_CARD
	};

	if( event == EVENT_ACTIVATE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);

		int can_use_ability = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test) >= 2 &&
								generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(2), 0,
															&td, NULL);
		int choice = CHOICE_MANA;
		if( ! paying_mana() && can_use_ability ){
			choice = CHOICE_LOOK_TOP_CARD;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) && choice == CHOICE_LOOK_TOP_CARD ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Look at top card", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_LOOK_TOP_CARD ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(1), 0, &td, "TARGET_PLAYER");
			if (spell_fizzled == 1){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_LOOK_TOP_CARD ){
			if( deck_ptr[instance->targets[0].player][0] != -1 ){
				show_deck( player, deck_ptr[instance->targets[0].player], 1, "Here's the first card of deck.", 0, 0x7375B0 );
			}
		}
	}

	return 0;
}

int card_morselhoarder(int player, int card, event_t event){

	/* Morselhoarder	|4|RG|RG
	 * Creature - Elemental 6/4
	 * ~ enters the battlefield with two -1/-1 counters on it.
	 * Remove a -1/-1 counter from ~: Add one mana of any color to your mana pool. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_M1_M1, 2);

	if (event == EVENT_COUNT_MANA && affect_me(player, card) && can_produce_mana(player, card)){
		declare_mana_available_any_combination_of_colors(player, COLOR_TEST_ANY_COLORED, count_counters(player, card, COUNTER_M1_M1));
	}

	if (event == EVENT_CAN_ACTIVATE && can_produce_mana(player, card) && count_counters(player, card, COUNTER_M1_M1) >= 1){
		return can_produce_mana(player, card);
	}

	if (event == EVENT_ACTIVATE){
		if (produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1)){
			remove_counter(player, card, COUNTER_M1_M1);
		}
	}

	return hybrid(player, card, event);
}

int card_mossbridge_troll(int player, int card, event_t event){
/* CARD_ID_MOSSBRIDGE_TROLL	9610
Mossbridge Troll	|5|G|G
Creature - Troll 5/5
If ~ would be destroyed, regenerate it.
Tap any number of untapped creatures you control other than ~ with total power 10 or greater: ~ gets +20/+20 until end of turn. */

	card_instance_t *instance = get_card_instance(player, card);

	// Autoregeneration
	if( instance->info_slot == 1 ){
		return 0;
	}
	instance->info_slot = 1;
	if( ( land_can_be_played & LCBP_REGENERATION) && can_regenerate(player, card) ){
		regenerate_target(player, card);
	}
	instance->info_slot = 0;

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;
	td.special = TARGET_SPECIAL_NOT_ME;
	td.illegal_abilities = 0;

	if( event == EVENT_CAN_ACTIVATE && generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
		int c;
		int pow_count = 0;
		for(c=0; c<active_cards_count[player]; c++){
			if( c != card && in_play(player, c) && ! is_tapped(player, c) && is_what(player, c, TYPE_CREATURE) ){
				pow_count += get_power(player, c);
				if( pow_count >= 10 ){
					return 1;
				}
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int pow_sum = 0;
			int total = 0;
			while( pow_sum < 10 && can_target(&td) ){
					if( select_target(player, card, &td, "Select another creature to tap.", &(instance->targets[total])) ){
						pow_sum+=get_power(instance->targets[total].player, instance->targets[total].card);
						state_untargettable(instance->targets[total].player, instance->targets[total].card, 1);
						total++;
					}
					else{
						break;
					}
			}
			instance->number_of_targets = 0;
			int i;
			for(i=0; i<total; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
				if( pow_sum >=10 ){
					tap_card(instance->targets[i].player, instance->targets[i].card);
				}
			}
			if( pow_sum < 10 ){
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 20, 20);
	}

	return 0;
}

int card_murderous_redcap(int player, int card, event_t event){
	/* Murderous Redcap	|2|BR|BR	0x200235c
	 * Creature - Goblin Assassin 2/2
	 * When ~ enters the battlefield, it deals damage equal to its power to any target.
	 * Persist */
	persist(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		if( can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0) ){
			damage_target0(player, card, get_power(player, card));
		}
		get_card_instance(player, card)->number_of_targets = 0;
	}

	return hybrid(player, card, event);
}

int card_mystic_gate(int player, int card, event_t event){
/* CARD_ID_MYSTIC_GATE	9614
Mystic Gate	""
Land
|T: Add |C to your mana pool.
|WU, |T: Add |W|W, |W|U, or |U|U to your mana pool. */

	return card_filter_land(player, card, event, COLOR_BLUE, COLOR_WHITE, COLOR_TEST_BLUE, COLOR_TEST_WHITE, " Colorless\n U -> UU\n U -> UW\n U -> WW\n W -> UU\n W -> UW\n W -> WW\n Cancel");
}

int card_nurturer_initiate(int player, int card, event_t event){
/* CARD_ID_NURTURER_INITIATE	9616
Nurturer Initiate	|G
Creature - Elf Shaman 1/1
Whenever a player casts a |Sgreen spell, you may pay |1. If you do, target creature gets +1/+1 until end of turn. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_AI(player), &test) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE );
			td.preferred_controller = player;

			card_instance_t *instance = get_card_instance( player, card );

			if( can_target(&td) && has_mana(player, COLOR_ANY, 1) &&
				charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1) )
			{
				if( pick_target(&td, "TARGET_CREATURE") ){
					pump_until_eot_merge_previous(player, card, instance->targets[0].player, instance->targets[0].card, 1, 1);
				}
			}
			instance->number_of_targets = 0;
		}

	}

	return 0;
}

int card_oona_queen_of_the_fae(int player, int card, event_t event){
	/*
	  Oona, Queen of the Fae 3{U/B}{U/B}{U/B}
	  Legendary Creature - Faerie Wizard 5/5
	  Flying
	  {X}{U/B}: Choose a color. Target opponent exiles the top X cards of his or her library.
	  For each card of the chosen color exiled this way, put a 1/1 blue and black Faerie Rogue creature token with flying onto the battlefield.
	*/
	check_legend_rule(player, card, event);

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance( player, card );

		if( event == EVENT_CAN_ACTIVATE ){
			if( generic_activated_ability(player, card, event, GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST0, 0, &td, NULL) ){
				int c1 = get_cost_mod_for_activated_abilities(player, card, 1, 0, 1, 0, 0, 0);
				return has_mana_hybrid(player, 1, COLOR_BLACK, COLOR_BLUE, c1);
			}
		}

		if( event == EVENT_ACTIVATE ){
			int c1 = get_cost_mod_for_activated_abilities(player, card, 0, 0, 1, 0, 0, 0);
			charge_mana_hybrid(player, card, 1, COLOR_BLACK, COLOR_BLUE, c1);
			if( spell_fizzled != 1  ){
				charge_mana(player, COLOR_COLORLESS, -1);
				if( spell_fizzled != 1  ){
					instance->targets[0].player = 1-player;
					instance->targets[0].card = -1;
					instance->number_of_targets = 1;
					instance->targets[1].player = x_value;
				}
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				int amount = MIN(count_deck(instance->targets[0].player), instance->targets[1].player);
				if( amount > 0 ){
					int clr = 1<<choose_a_color(player, get_deck_color(player, 1-player));
					if( player == AI ){
						char buffer[500];
						int pos = scnprintf(buffer, 500, "Opponent selected ", buffer);
						int i;
						for(i=0; i<5; i++){
							if( clr & (1<<i) ){
								card_ptr_t* c = cards_ptr[ CARD_ID_BLACK+i ];
								pos += scnprintf(buffer + pos, 100-pos, "%s", c->name);
							}
						}
						do_dialog(1-player, instance->parent_controller, instance->parent_card, -1, -1, buffer, 0);
					}
					int *deck = deck_ptr[instance->targets[0].player];
					show_deck(player, deck, amount, "Oona revealed...", 0, 0x7375B0 );
					show_deck(1-player, deck, amount, "Oona revealed...", 0, 0x7375B0 );
					int i, num_tokens = 0;
					for(i=0; i<amount; i++){
						if( !(cards_data[deck[0]].type & TYPE_LAND) &&
							(cards_data[deck[0]].color & clr)
						  ){
							++num_tokens;
						}
						rfg_card_in_deck(instance->targets[0].player, 0);
					}
					if (num_tokens > 0){
						token_generation_t token;
						default_token_definition(player, card, CARD_ID_FAERIE_ROGUE, &token);
						token.color_forced = COLOR_TEST_BLACK | COLOR_TEST_BLUE;
						token.qty = num_tokens;
						generate_token(&token);
					}
				}
			}
		}
	}

	return hybrid(player, card, event);
}

int card_oracle_of_nectars(int player, int card, event_t event){
/* CARD_ID_ORACLE_OF_NECTARS	9620
Oracle of Nectars	|2|GW
Creature - Elf Cleric 2/2
|X, |T: You gain X life. */

	if( ! IS_GAA_EVENT(event) ){
		return hybrid(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(-1), 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			if( life[player] < 6 ){
				ai_modifier += (instance->info_slot * 12);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, instance->info_slot);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_order_of_whiteclay(int player, int card, event_t event){

	/* Order of Whiteclay	|1|W|W
	 * Creature - Kithkin Cleric 1/4
	 * |1|W|W, |Q: Return target creature card with converted mana cost 3 or less from your graveyard to the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card with CMC 3 or less.");
	test.cmc = 4;
	test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_TAPPED, MANACOST_XW(1, 2), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER);
}

int card_oversoul_of_dusk(int player, int card, event_t event){
/* CARD_ID_OVERSOUL_OF_DUSK	1417
Oversoul of Dusk	|GW|GW|GW|GW|GW
Creature - Spirit Avatar 5/5
Protection from |Sblue, from |Sblack, and from |Sred */

	return hybrid(player, card, event);
}

int card_painters_servant(int player, int card, event_t event)
{
/* CARD_ID_PAINTERS_SERVANT	9622
Painter's Servant	|2
Artifact Creature - Scarecrow 1/3
As ~ enters the battlefield, choose a color. */

  if (event == EVENT_RESOLVE_SPELL)
	get_card_instance(player, card)->targets[1].card = 1<<choose_a_color_and_show_legacy(player, card, player, -1);

  // All cards that aren't on the battlefield, spells, and permanents are the chosen color in addition to their other colors.
  // For exile/graveyards/libraries, get_global_color_hack().
  if (event == EVENT_SET_COLOR)
	{
	  int col = get_card_instance(player, card)->targets[1].card;
	  if (col > 0 && !is_what(affected_card_controller, affected_card, TYPE_EFFECT))
		event_result |= col;
	}

  if (event == EVENT_CAST_SPELL)
	{
	  // Make sure spells cast onto the stack are colored correctly.
	  int col = get_card_instance(player, card)->targets[1].card;
	  if (col > 0)
		get_card_instance(affected_card_controller, affected_card)->regen_status |= KEYWORD_RECALC_SET_COLOR;
	}

  if ((event == EVENT_STATIC_EFFECTS && ai_is_speculating != 1) || leaves_play(player, card, event))
	{
	  // Eye candy only.  Primarily aimed at cards in hand, but will also hit phased/oublietted ones.
	  card_instance_t* instance = get_card_instance(player, card);
	  int col = instance->targets[1].card;
	  if (event != EVENT_STATIC_EFFECTS)
		col = instance->targets[1].card = 0;
	  else if (col <= 0)
		return 0;

	  card_instance_t* inst;
	  int p, c;
	  for (p = 0; p <= 1; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if (!(get_color(p, c) & col) && !in_play(p, c) && (inst = get_card_instance(p, c)) && inst->internal_card_id != -1)
			{
			  inst->regen_status |= KEYWORD_RECALC_SET_COLOR;
			  get_color(p, c);
			}
	}

  return 0;
}

int card_pili_pala(int player, int card, event_t event){
/* CARD_ID_PILI_PALA	9625
Pili-Pala	|2
Artifact Creature - Scarecrow 1/1
Flying
|2, |Q: Add one mana of any color to your mana pool. (|Q is the untap symbol.) */

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && can_produce_mana(player, card) && is_tapped(player, card) && ! is_sick(player, card) &&
		has_mana(player, COLOR_COLORLESS, 2)
	  ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) && is_tapped(player, card) && ! is_sick(player, card) &&
		has_mana(player, COLOR_COLORLESS, 2)
	  ){
		return can_produce_mana(player, card);
	}

	if(event == EVENT_ACTIVATE ){
		charge_mana(player, COLOR_COLORLESS, 2);
		if( spell_fizzled != 1 ){
			mana_producer(player, card, event);
			untap_card(player, card);
		}
	}

	return 0;
}

int card_plague_of_vermin(int player, int card, event_t event){
	/* Plague of Vermin	|6|B
	 * Sorcery
	 * Starting with you, each player may pay any amount of life. Repeat this process until no one pays life. Each player puts a 1/1 |Sblack Rat creature token onto the battlefield for each 1 life he or she paid this way. */

	if( event == EVENT_RESOLVE_SPELL ){
		int life_bid[2] = {0, 0};
		int total_life_bid[2] = {0, 0};
		int bid_turn = player;
		int bid_turn_count = 0;
		while( 1 ){
				if( can_pay_life(bid_turn, 1) ){
					if( bid_turn != AI ){
						life_bid[bid_turn] = choose_a_number(bid_turn, "Pay how much life?", life[bid_turn]);
						if( life_bid[bid_turn] < 1 || ! can_pay_life(bid_turn, life_bid[bid_turn]) ){
							life_bid[bid_turn] = 0;
						}
					}
					else{
						if( life_bid[1-bid_turn] == 0 ){
							life_bid[bid_turn] = count_permanents_by_type(1-bid_turn, TYPE_CREATURE);
							if( life_bid[bid_turn] < 1 ){
								life_bid[bid_turn] = 1;
							}
						}
						else{
							life_bid[bid_turn] = life[1-bid_turn];
							while( life[bid_turn]-life_bid[bid_turn] < 6 ){
									life_bid[bid_turn]--;
							}
						}
					}
				}
				else{
					life_bid[bid_turn] = 0;
				}

				if( bid_turn != HUMAN ){
					char buffer[100];
					if( life_bid[bid_turn] > 0 ){
						snprintf(buffer, 100, "Opponent pays %d life.", life_bid[bid_turn] );
					}
					else{
						snprintf(buffer, 100, "Opponent passes.");
					}
					do_dialog(HUMAN, player, card, -1, -1, buffer, 0);
				}
				lose_life(bid_turn, life_bid[bid_turn]);
				total_life_bid[bid_turn]+=life_bid[bid_turn];
				bid_turn_count++;
				if( bid_turn_count > 1 && (life_bid[bid_turn] == 0 || life_bid[1-bid_turn] == 0) ){
					break;
				}
				if( bid_turn_count > 1 ){
					bid_turn_count = 0;
				}
				bid_turn = 1-bid_turn;
		}
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_RAT, &token);
		int i;
		for(i=0; i<2; i++){
			if( total_life_bid[i] > 0 ){
				token.t_player = i;
				token.qty = total_life_bid[i];
				generate_token(&token);
			}
		}
		kill_card(player, card, KILL_DESTROY);

	}

	return basic_spell(player, card, event);
}

int card_polluted_bonds(int player, int card, event_t event){
/* CARD_ID_POLLUTED_BONDS	9629
Polluted Bonds	|3|B|B
Enchantment
Whenever a land enters the battlefield under an opponent's control, that player loses 2 life and you gain 2 life. */

	etb_ability_global(1-player, event, RESOLVE_TRIGGER_OPTIONAL, TYPE_LAND, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		lose_life(1-player, 2);
		gain_life(player, 2);
	}

	return global_enchantment(player, card, event);
}

int card_prismatic_omen(int player, int card, event_t event){
/* CARD_ID_PRISMATIC_OMEN	9632
Prismatic Omen	|1|G
Enchantment
Lands you control are every basic land type in addition to their other types. */

	int result = permanents_you_control_can_tap_for_mana_all_one_color(player, card, event, TYPE_LAND, -1, COLOR_TEST_ANY_COLORED, 1);
	if (event == EVENT_CAN_ACTIVATE){
		return result;
	}

	return global_enchantment(player, card, event);
}

int card_prison_term(int player, int card, event_t event){
/* CARD_ID_PRISON_TERM	9634
Prison Term	|1|W|W
Enchantment - Aura
Enchant creature
Enchanted creature can't attack or block, and its activated abilities can't be activated.
Whenever a creature enters the battlefield under an opponent's control, you may attach ~ to that creature. */

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if(trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me( player, card ) &&
			reason_for_trigger_controller == player && trigger_cause_controller == 1-player
		  ){
			int ai_mode = my_base_value(trigger_cause_controller, trigger_cause) > my_base_value(instance->damage_target_player, instance->damage_target_card) ?
							RESOLVE_TRIGGER_MANDATORY : 0;

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			if( new_specific_cip(player, card, event, 1-player, player == AI ? ai_mode : RESOLVE_TRIGGER_OPTIONAL, &this_test) ){
				disable_all_activated_abilities(instance->damage_target_player, instance->damage_target_card, 0);
				disable_all_activated_abilities(trigger_cause_controller, trigger_cause, 1);
				attach_aura_to_target(player, card, event, trigger_cause_controller, trigger_cause);
			}
		}
	}

	return card_arrest(player, card, event);
}

int card_pucas_mischief(int player, int card, event_t event){
/* CARD_ID_PUCAS_MISCHIEF	9635
Puca's Mischief	|3|U
Enchantment
At the beginning of your upkeep, you may exchange control of target nonland permanent you control and
target nonland permanent an opponent controls with an equal or lesser converted mana cost. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_LAND;
		td.allowed_controller = player;
		td.preferred_controller = player;

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_PERMANENT);
		td1.illegal_type = TYPE_LAND;
		td1.allowed_controller = 1-player;
		td1.preferred_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && can_target(&td1) ){
			if( new_pick_target(&td, "Select target permanent you control.", 0, GS_LITERAL_PROMPT) ){
				td1.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;
				td1.extra = get_cmc(instance->targets[0].player, instance->targets[0].card);

				if( can_target(&td1) ){
					char buffer[100];
					scnprintf(buffer, 100, "Select target permanent an opponent controls with CMC %d or less.",
							get_cmc(instance->targets[0].player, instance->targets[0].card));
					if( new_pick_target(&td1, buffer, 1, GS_LITERAL_PROMPT) ){
						exchange_control_of_target_permanents(player, card, instance->targets[0].player, instance->targets[0].card,
															instance->targets[1].player, instance->targets[1].card);
					}
				}
			}
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}


int card_puncture_bolt(int player, int card, event_t event){
/* CARD_ID_PUNCTURE_BOLT	9636
Puncture Bolt	|1|R
Instant
~ deals 1 damage to target creature. Put a -1/-1 counter on that creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			add_minus1_minus1_counters(instance->targets[0].player, instance->targets[0].card, 1);
			damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

static int puppeteer_clique_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		haste(p, c, event);

		if( current_turn == player && trigger_condition == TRIGGER_EOT &&  affect_me(player, card ) &&
			reason_for_trigger_controller == player )
		{
			if( instance->targets[1].player > 0 ){
				instance->targets[1].player--;
			}
			else{
				if( eot_trigger(player, card, event) ){
					kill_card(p, c, KILL_EXILE);
				}
			}
		}
	}

	return 0;
}

int card_puppeteer_clique(int player, int card, event_t event){
/* CARD_ID_PUPPETEER_CLIQUE	9637
Puppeteer Clique	|3|B|B
Creature - Faerie Wizard 3/2
Flying
When ~ enters the battlefield, put target creature card from an opponent's graveyard onto the battlefield under your control. It gains haste.
At the beginning of your next end step, exile it.
Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.) */

	persist(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( count_graveyard_by_type(1-player, TYPE_CREATURE) > 0 && ! graveyard_has_shroud(1-player) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
			int result = new_global_tutor(player, 1-player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_CMC, &this_test);
			if( result > -1 ){
				int legacy = create_targetted_legacy_effect(player, card, &puppeteer_clique_legacy, player, result);
				if( current_turn == player && current_phase == PHASE_DISCARD ){
					get_card_instance(player, legacy)->targets[1].player = 1;
				}
			}
		}
	}

	return 0;
}

int card_put_away(int player, int card, event_t event){
/* CARD_ID_PUT_AWAY	9639
Put Away	|2|U|U
Instant
Counter target spell. You may shuffle up to one target card from your graveyard into your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);

			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card.");
			int result = new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_DECK, 0, AI_MAX_VALUE, &this_test);
			if( result > -1 ){
				shuffle(player);
			}
		}
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return card_counterspell(player, card, event);
}

// pyre charger --> shivan dragon

int card_pyre_charger(int player, int card, event_t event){
/* CARD_ID_PYRE_CHARGER	9640
Pyre Charger	|R|R
Creature - Elemental Warrior 1/1
Haste
|R: ~ gets +1/+0 until end of turn. */

	haste(player, card, event);
	return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1,0);
}

int card_rage_reflection(int player, int card, event_t event){
/* CARD_ID_RAGE_REFLECTION	9641
Rage Reflection	|4|R|R
Enchantment
Creatures you control have double strike. */

	boost_creature_type(player, card, event, -1, 0, 0, KEYWORD_DOUBLE_STRIKE, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
	return global_enchantment(player, card, event);
}

int card_raking_canopy(int player, int card, event_t event)
{
/* CARD_ID_RAKING_CANOPY	9642
Raking Canopy	|1|G|G
Enchantment
Whenever a creature with flying attacks you, ~ deals 4 damage to it. */

  if (xtrigger_condition() == XTRIGGER_ATTACKING || event == EVENT_DECLARE_ATTACKERS)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.keyword = KEYWORD_FLYING;

	  int amt;
	  if ((amt = declare_attackers_trigger_test(player, card, event, DAT_TRACK|DAT_ATTACKS_PLAYER, 1-player, -1, &test)))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
		  for (--amt; amt >= 0; --amt)
			if (in_play(current_turn, attackers[amt]))
			  damage_creature(current_turn, attackers[amt], 4, player, card);
		}
	}

  return global_enchantment(player, card, event);
}

static int get_mana_image(int cless, int black, int blue, int green, int red, int white){
	int mana[6] = {cless, black, blue, green, red, white};
	int image = 0;
	int q;
	for(q=0; q<mana[0]; q++){
		image |= (1<<q);
	}
	for(q=1; q<6; q++){
		if( mana[q] == 1 ){
			image |= (1<<(q+15));
		}
	}
	return image;
}

int can_cast_reaper_king(int player, int card, event_t event){
	int cless = 0;
	if( card == -1 ){
		cless = get_updated_casting_cost(player, -1, get_internal_card_id_from_csv_id(CARD_ID_REAPER_KING), event, -1);
	}
	else{
		cless = get_updated_casting_cost(player, card, -1, event, -1);
	}
	int i;
	int costs[6] = {0, 0, 0, 0, 0, 0};
	for(i=1; i<6; i++ ){
		if( has_mana(player, i, 1) ){
			cless -= 2;
			costs[i]++;
		}
	}
	if( has_mana_multi_a(player, cless, costs[1], costs[2], costs[3], costs[4], costs[5]) ){
		return 1;
	}
	return 0;
}

int casting_reaper_king(int player, int card, event_t event){
			int x;
			int mana[6];
			int mana2[100];

			for( x = 0 ;x < 6;x++){
				mana[x] = 0;
			}
			for( x = 0 ;x < 100;x++){
				mana2[x] = 0;
			}

			int cless = get_updated_casting_cost(player, card, -1, event, -1);
			int hybrid_cless = 10;
			cless-=hybrid_cless;
			int m = 1;
			if( has_mana(player, COLOR_ARTIFACT, 10+cless) ){
				mana2[m] = get_mana_image(10+cless, 0, 0, 0, 0, 0);
				m++;
			}
			int a,b,c,d,e;
			for(a=1; a<6; a++){
				mana[a] = 1;
				if( has_mana_multi_a(player, (hybrid_cless-2)+cless, mana[1], mana[2], mana[3], mana[4], mana[5]) ){
					mana2[m] = get_mana_image((hybrid_cless-2)+cless, mana[1], mana[2], mana[3], mana[4], mana[5]);
					m++;
					for(b=a+1; b<6; b++){
						mana[b] = 1;
						if( has_mana_multi_a(player, (hybrid_cless-4)+cless, mana[1], mana[2], mana[3], mana[4], mana[5]) ){
							mana2[m] = get_mana_image((hybrid_cless-4)+cless, mana[1], mana[2], mana[3], mana[4], mana[5]);
							m++;
							for(c=b+1; c<6; c++){
								mana[c] = 1;
								if( has_mana_multi_a(player, (hybrid_cless-6)+cless, mana[1], mana[2], mana[3], mana[4], mana[5]) ){
									mana2[m] = get_mana_image((hybrid_cless-6)+cless, mana[1], mana[2], mana[3], mana[4], mana[5]);
									m++;
									for(d=c+1; d<6; d++){
										mana[d] = 1;
										if( has_mana_multi_a(player, (hybrid_cless-8)+cless, mana[1], mana[2], mana[3], mana[4], mana[5]) ){
											mana2[m] = get_mana_image((hybrid_cless-8)+cless, mana[1], mana[2], mana[3], mana[4], mana[5]);
											m++;
											for(e=d+1; e<6; e++){
												mana[e] = 1;
												if( has_mana_multi_a(player, (hybrid_cless-10)+cless, mana[1], mana[2], mana[3], mana[4], mana[5]) ){
													mana2[m] = get_mana_image((hybrid_cless-10)+cless, mana[1], mana[2], mana[3], mana[4], mana[5]);
													m++;
												}
												mana[e] = 0;
											}
										}
										mana[d] = 0;
									}
								}
								mana[c] = 0;
							}
						}
						mana[b] = 0;
					}
				}
				mana[a] = 0;
			}
			char buffer[500];
			int pos = scnprintf(buffer, 500, " Cancel\n");
			test_definition_t this_test;
			for(a=1; a<m; a++){
				for( x = 0 ;x < 6;x++){
					mana[x] = 0;
				}
				int q;
				for(q=0; q<16; q++){
					if( mana2[a] & (1<<q) ){
						mana[0]++;
					}
				}
				for(q=16; q<21; q++){
					if( mana2[a] & (1<<q) ){
						mana[q-15]++;
					}
				}
				mana_into_string(mana[0], mana[1], mana[2], mana[3], mana[4], mana[5], &this_test);
				pos +=scnprintf(buffer+pos, 500-pos, " Pay %s\n", this_test.message);
			}
			int choice = 1;
			if( m > 2 && player == HUMAN){
				choice = do_dialog(player, player, card, -1, -1, buffer, 1);
			}
			if( choice == 0 ){
				spell_fizzled = 1;
				return 0;
			}
			for( x = 0 ;x < 6;x++){
				mana[x] = 0;
			}
			int q;
			for(q=0; q<16; q++){
				if( mana2[choice] & (1<<q) ){
					mana[0]++;
				}
			}
			for(q=16; q<21; q++){
				if( mana2[choice] & (1<<q) ){
					mana[q-15]++;
				}
			}
			charge_mana_multi_a(player, 0, mana[1], mana[2], mana[3], mana[4], mana[5], mana[0]+cless);
			if( spell_fizzled != 1 ){
				return 1;
			}
	return 0;
}

int card_reaper_king(int player, int card, event_t event){
/* CARD_ID_REAPER_KING	9645
Reaper King	|2W|2U|2B|2R|2G
Legendary Artifact Creature - Scarecrow 6/6
(|2W can be paid with any two mana or with |W. This card's converted mana cost is 10.)
Other Scarecrow creatures you control get +1/+1.
Whenever another Scarecrow enters the battlefield under your control, destroy target permanent. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_MODIFY_COST ){
		if( can_cast_reaper_king(player, card, event) ){
			null_casting_cost(player, card);
		}
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			casting_reaper_king(player, card, event);
		}
	}

	if( event == EVENT_SET_COLOR && affect_me(player, card)  ){
		event_result |= cards_data[instance->internal_card_id].color;
	}

	boost_creature_type(player, card, event, SUBTYPE_SCARECROW, 1, 1, 0, BCT_CONTROLLER_ONLY);

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_SCARECROW;
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}
	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;

		if( can_target(&td) && new_pick_target(&td, "TARGET_PERMANENT", 0, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_reknit(int player, int card, event_t event){
/* CARD_ID_REKNIT	9646
Reknit	|1|GW
Instant
Regenerate target permanent. */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.required_state = TARGET_STATE_DESTROYED;
	td.preferred_controller = player;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_REGENERATION, &td, "TARGET_PERMANENT", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( hybrid_casting(player, card, 0) ){
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_REGENERATION, &td, "TARGET_PERMANENT", 1, NULL);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_repel_intruders(int player, int card, event_t event){
	/* Repel Intruders	|3|WU
	 * Instant
	 * Put two 1/1 |Swhite Kithkin Soldier creature tokens onto the battlefield if |W was spent to cast ~. Counter up to one target creature spell if |U was spent to cast ~. */

	if( event == EVENT_MODIFY_COST ){
		if( card_on_stack_controller != -1 && card_on_stack != -1){
			if( ! check_state(card_on_stack_controller, card_on_stack, STATE_CANNOT_TARGET) &&
				is_what(card_on_stack_controller, card_on_stack, TYPE_CREATURE)
			  ){
				if( has_mana_multi(player, 3, 0, 1, 0, 0, 0) ){
					null_casting_cost(player, card);
					get_card_instance( player, card )->targets[1].card = COLOR_WHITE;
				}
			}
		}
		else{
			get_card_instance( player, card )->targets[1].card = -1;
		}
	}

	if( event == EVENT_CAN_CAST ){
		if( counterspell(player, card, event, NULL, 0) ){
			return 0x63;
		}
		return 1;
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			card_instance_t *instance = get_card_instance( player, card );
			if( instance->targets[1].card == COLOR_WHITE ){
				int cless = get_updated_casting_cost(player, card, -1, event, -1);
				char buffer[500];
				int mode = (1<<3);
				int pos = 0;
				int ai_choice = 1;
				if( has_mana_multi(player, cless, 0, 1, 0, 0, 0) ){
					pos +=scnprintf(buffer+pos, 500-pos, " Pay %dU\n", cless);
					mode |=(1<<0);
				}
				if( has_mana_multi(player, cless, 0, 0, 0, 0, 1) ){
					pos +=scnprintf(buffer+pos, 500-pos, " Pay %dW\n", cless);
					mode |=(1<<1);
				}
				if( has_mana_multi(player, cless-1, 0, 1, 0, 0, 1) ){
					pos +=scnprintf(buffer+pos, 500-pos, " Pay %dUW\n", cless-1);
					mode |=(1<<2);
					ai_choice = 2;
				}
				pos +=scnprintf(buffer+pos, 500-pos, " Cancel");

				int choice = do_dialog(player, player, card, -1, -1, buffer, ai_choice);
				while( !((1<<choice) & mode) ){
						choice++;
				}
				int white = 1;
				int blue = 0;
				if( choice == 3 ){
					spell_fizzled = 1;
					return 0;
				}
				if( choice == 0 ){
					blue = 1;
					white = 0;
				}
				if( choice == 2 ){
					blue = 1;
					cless--;
				}
				charge_mana_multi(player, cless, 0, blue, 0, 0, white);
				if( spell_fizzled != 1 ){
					if( choice == 0 ){
						instance->targets[0].player = card_on_stack_controller;
						instance->targets[0].card = card_on_stack;
					}
					if( choice != 2 ){
						instance->info_slot = 1<<choice;
					}
					else{
						instance->info_slot = (1<<0)+(1<<1);
					}
				}
			}
			else{
				instance->info_slot = (1<<1);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card );
		if( instance->info_slot & (1<<0) ){
			if( counterspell_validate(player, card, NULL, 0) ){
				real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			}
		}
		if( instance->info_slot & (1<<1) ){
			generate_tokens_by_id(player, card, CARD_ID_KITHKIN_SOLDIER, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static const char* can_be_activated(int who_chooses, int player, int card)
{
	if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
		return NULL;
	}
	return "you can't activate this creature.";
}

int card_resplendant_mentor(int player, int card, event_t event){
/* CARD_ID_RESPLENDENT_MENTOR	9648
Resplendent Mentor	|4|W
Creature - Kithkin Cleric 2/2
|SWhite creatures you control have "|T: You gain 1 life." */

	if( ! IS_GAA_EVENT(event) || is_humiliated(player, card) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = player;
	td.extra = (int32_t)can_be_activated;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.illegal_abilities = 0;
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return can_target(&td);
	}

	if( event == EVENT_ACTIVATE ){
		char msg[100];
		scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select an untapped, nonsick %s creature you control.", COLOR_WHITE));
		if( new_pick_target(&td, msg, 0, 1 | GS_LITERAL_PROMPT) ){
			instance->number_of_targets = 0;
			granted_generic_activated_ability(player, card, instance->targets[0].player, instance->targets[0].card, event, GAA_UNTAPPED,
										MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 1);
	}

	return 0;
}

int card_rhys_the_redeemed(int player, int card, event_t event){
	/* Rhys the Redeemed	|GW
	 * Legendary Creature - Elf Warrior 1/1
	 * |2|GW, |T: Put a 1/1 |Sgreen and |Swhite Elf Warrior creature token onto the battlefield.
	 * |4|GW|GW, |T: For each creature token you control, put a token that's a copy of that creature onto the battlefield. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return hybrid(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_ELF_WARRIOR = 1,
		CHOICE_TOKEN_DUPLICATION
	};

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_XG(2, 1));
		if( has_mana_hybrid(player, 1, COLOR_GREEN, COLOR_WHITE, c1) ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0,
											NULL, NULL);
		}
	}

	if(event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_XG(4, 1));
		int can_dupe_tokens = has_mana_hybrid(player, 1, COLOR_GREEN, COLOR_WHITE, c1);
		int total_tok_pow = 0;
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_play(p, c) && is_what(p, c, TYPE_CREATURE) && is_token(p, c) ){
										total_tok_pow += get_power(p, c);
									}
								}
		);
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Create an Elf Warrior", 1, 5,
							"Duplicate your tokens", can_dupe_tokens, total_tok_pow >= 6 ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_XG(choice * 2, choice));
		if( charge_mana_hybrid(player, card, choice, COLOR_GREEN, COLOR_WHITE, c1) ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0,
												NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_ELF_WARRIOR ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ELF_WARRIOR, &token);
			token.color_forced = COLOR_TEST_GREEN | COLOR_TEST_WHITE;
			generate_token(&token);
		}
		if( instance->info_slot == CHOICE_TOKEN_DUPLICATION ){
			copy_all_tokens(instance->parent_controller, instance->parent_card, TYPE_CREATURE, NULL);
		}
	}

	return 0;
}

int card_rite_of_consumption(int player, int card, event_t event){
	/* Rite of Consumption	|1|B	0x2007e33
	 * Sorcery
	 * As an additional cost to cast this spell, sacrifice a creature.
	 * ~ deals damage equal to the sacrificed creature's power to target player or planeswalker. You gain life equal to the damage dealt this way. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_SAC_CREATURE_AS_COST, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		instance->info_slot = get_power(BYTE2(sac), BYTE3(sac));
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
		if( spell_fizzled != 1 ){
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
		else{
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
		}
	}


	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			damage_target0(player, card, instance->info_slot);
			gain_life(player, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_river_kelpie(int player, int card, event_t event){
	/*
	  River Kelpie |3|U|U
	  Creature - Beast 3/3
	  Whenever River Kelpie or another permanent enters the battlefield from a graveyard, draw a card.
	  Whenever a player casts a spell from a graveyard, draw a card.
	  Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.)
	*/
	persist(player, card, event);

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		if( check_special_flags3(trigger_cause_controller, trigger_cause, SF3_REANIMATED) ){
			test_definition_t test;
			default_test_definition(&test, TYPE_PERMANENT);
			etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
		}
	}

	if( event == EVENT_ETB_ABILITY ){
		draw_a_card(player);
	}

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		if( check_special_flags(trigger_cause_controller, trigger_cause, SF_PLAYED_FROM_GRAVE) ){
			if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, NULL) ){
				draw_a_card(player);
			}
		}
	}

	return 0;
}

int card_rivers_grasp(int player, int card, event_t event){
/* CARD_ID_RIVERS_GRASP	9653
River's Grasp	|3|UB
Sorcery
If |U was spent to cast ~, return up to one target creature to its owner's hand. If |B was spent to cast ~, target player reveals his or her hand,
you choose a nonland card from it, then that player discards that card. (Do both if |U|B was spent.) */

	if( event == EVENT_MODIFY_COST ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.zone = TARGET_ZONE_PLAYERS;
		card_instance_t *instance = get_card_instance( player, card );
		if( has_mana_multi(player, 3, 1, 0, 0, 0, 0) && can_target(&td1) ){
			null_casting_cost(player, card);
			instance->targets[1].card = COLOR_BLUE;
		}
		else{
			instance->targets[1].card = -1;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			if( instance->targets[1].card == COLOR_BLUE ){
				int cless = get_updated_casting_cost(player, card, -1, event, -1);
				char buffer[500];
				int mode = (1<<3);
				int pos = 0;
				int ai_choice = 1;
				if( has_mana_multi(player, cless, 0, 1, 0, 0, 0) ){
					pos +=scnprintf(buffer+pos, 500-pos, " Pay %dU\n", cless);
					mode |=(1<<0);
				}
				if( has_mana_multi(player, cless, 1, 0, 0, 0, 0) ){
					pos +=scnprintf(buffer+pos, 500-pos, " Pay %dB\n", cless);
					mode |=(1<<1);
				}
				if( has_mana_multi(player, cless-1, 1, 1, 0, 0, 0) ){
					pos +=scnprintf(buffer+pos, 500-pos, " Pay %dBU\n", cless-1);
					mode |=(1<<2);
					ai_choice = 2;
				}
				pos +=scnprintf(buffer+pos, 500-pos, " Cancel");

				int choice = do_dialog(player, player, card, -1, -1, buffer, ai_choice);
				while( !((1<<choice) & mode) ){
						choice++;
				}
				int black = 0;
				int blue = 1;
				if( choice == 3 ){
					spell_fizzled = 1;
					return 0;
				}
				if( choice == 1 ){
					blue = 0;
					black = 1;
				}
				if( choice == 2 ){
					black = 1;
					cless--;
				}
				charge_mana_multi(player, cless, black, blue, 0, 0, 0);
				if( spell_fizzled != 1 ){
					choice++;
					instance->info_slot = 0;
					if( choice & 1 ){
						if( pick_target(&td, "TARGET_CREATURE") ){
							instance->info_slot |= 1;
						}
					}
					if( choice & 2 ){
						if( new_pick_target(&td1, "TARGET_PLAYER", 1, 1) ){
							instance->info_slot |= 2;
						}
					}
				}
			}
			else{
				if( pick_target(&td, "TARGET_CREATURE") ){
					instance->info_slot = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 1) && validate_target(player, card, &td, 0) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		if( (instance->info_slot & 2) && validate_target(player, card, &td1, 1) ){
			char msg[100] = "Select a nonland card.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, msg);
			this_test.type_flag = 1;
			ec_definition_t this_definition;
			default_ec_definition(instance->targets[1].player, player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_roughshod_mentor(int player, int card, event_t event){
/* CARD_ID_ROUGHSHOD_MENTOR	9655
Roughshod Mentor	|5|G
Creature - Giant Warrior 5/4
|SGreen creatures you control have trample. */

	boost_creature_by_color(player, card, event, COLOR_TEST_GREEN, 0, 0, KEYWORD_TRAMPLE, BCT_INCLUDE_SELF+BCT_CONTROLLER_ONLY);
	return 0;
}

static int ai_helper_for_runed_halo(int player){
	int *deck = deck_ptr[1-player];
	int csvid = -1;
	if( deck[0] != -1 ){
		int start_from_top = internal_rand(50) > 25 ? 1 : 0;
		int count = start_from_top ? 0 : count_deck(1-player)-1;
		while( count > -1 && deck[count] != -1 ){
				if( is_what(-1, deck[count], TYPE_CREATURE) ){
					csvid = cards_data[deck[count]].id;
					break;
				}
				if( start_from_top ){
					count++;
				}
				else{
					count--;
				}
		}
		if( csvid == -1 ){
			count = start_from_top ? 0 : count_deck(1-player)-1;
			while( count > -1 && deck[count] != -1 ){
					if( is_what(-1, deck[count], TYPE_PERMANENT) ){
						csvid = cards_data[deck[count]].id;
						break;
					}
					if( start_from_top ){
						count++;
					}
					else{
						count--;
					}
			}
		}
		if( csvid == -1 ){
			count = start_from_top ? 0 : count_deck(1-player)-1;
			while( count > -1 && deck[count] != -1 ){
					if( ! is_what(-1, deck[count], TYPE_LAND) ){
						csvid = cards_data[deck[count]].id;
						break;
					}
					if( start_from_top ){
						count++;
					}
					else{
						count--;
					}
			}
		}
	}
	if( csvid == -1 ){
		csvid = CARD_ID_LIGHTNING_BOLT;
	}
	return csvid;
}

int card_runed_halo(int player, int card, event_t event){
/* CARD_ID_RUNED_HALO	9657
Runed Halo	|W|W
Enchantment
As ~ enters the battlefield, choose a card name.
You have protection from the chosen name. (You can't be targeted, dealt damage, or enchanted by anything with that name.) */

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT );
		td.illegal_abilities = 0;

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE );
		td2.power_requirement = 1 | TARGET_PT_GREATER_OR_EQUAL;

		card_instance_t *instance = get_card_instance(player, card);

		int id = -1;
		int choice = 1;
		while( id == -1 ){
				if( (player == HUMAN && can_target(&td)) || (player == AI && can_target(&td2)) ){
					choice = do_dialog(player, player, card, -1, -1, " Select a card in play\n Choose a card from a list", 0);
				}
				if( choice == 0 ){
					if( (player == HUMAN && pick_target(&td, "TARGET_PERMANENT")) || (player == AI && pick_target(&td2, "TARGET_CREATURE")) ){
						id = get_id(instance->targets[0].player, instance->targets[0].card);
						instance->number_of_targets = 0;
					}
				}
				else{
					if( player == HUMAN ){
						id = card_from_list(player, 3, TYPE_ANY, 0, 0, 0, 0, 0, 0, 0, -1, 0);
					}
					else{
						id = ai_helper_for_runed_halo(player);
					}
				}
		}
		instance->targets[9].card = id;
		create_card_name_legacy(player, card, id);
	}

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == -1 && damage->damage_target_player == player && damage->info_slot > 0 &&
				damage->display_pic_csv_id == instance->targets[9].card )
			{
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_runes_of_the_deus(int player, int card, event_t event){
/* CARD_ID_RUNES_OF_THE_DEUS	9658
Runes of the Deus	|4|RG
Enchantment - Aura
Enchant creature
As long as enchanted creature is |Sred, it gets +1/+1 and has double strike. (It deals both first-strike and regular combat damage.)
As long as enchanted creature is |Sgreen, it gets +1/+1 and has trample. */

	hybrid(player, card, event);
	aura_ability_for_color(player, card, event, COLOR_TEST_GREEN, 1, 1, KEYWORD_TRAMPLE, 0);
	aura_ability_for_color(player, card, event, COLOR_TEST_RED, 1, 1, KEYWORD_DOUBLE_STRIKE, 0);
	return generic_aura(player, card, event, player, 0, 0, 0, 0, 0, 0, 0);
}

// safehold elite --> scuzzback marauders

int card_safewright_quest(int player, int card, event_t event){

	/* Safewright Quest	|GW
	 * Sorcery
	 * Search your library for |Ha Forest or |H2Plains card, reveal it, and put it into your hand. Then shuffle your library. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int total = get_total_available_lands(player);
		ai_modifier += (total <= 3 || total >= 6 ? 48 : 12);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, get_hacked_land_text(player, card, "Select %a or %s card.", SUBTYPE_FOREST, SUBTYPE_PLAINS));
		test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
		test.sub2 = get_hacked_subtype(player, card, SUBTYPE_PLAINS);
		test.subtype_flag = F2_MULTISUBTYPE;

		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);

		kill_card(player, card, KILL_DESTROY);
	}

	return hybrid(player, card, event);
}

int card_sapseep_forest(int player, int card, event_t event){
	/* Sapseep Forest	""
	 * Land - |H2Forest
	 * ~ enters the battlefield tapped.
	 * |G, |T: You gain 1 life. Activate this ability only if you control two or more |Sgreen permanents. */

	comes_into_play_tapped(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_GAIN_LIFE
	};

	if( event == EVENT_ACTIVATE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

		int can_use_ability = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test) >= 2 &&
								generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_G(2), 0, NULL, NULL);
		int choice = CHOICE_MANA;
		if( ! paying_mana() && can_use_ability ){
			choice = CHOICE_GAIN_LIFE;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) && choice == CHOICE_GAIN_LIFE ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Gain 1 life", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_GAIN_LIFE ){
			add_state(player, card, STATE_TAPPED);
			charge_mana_for_activated_ability(player, card, MANACOST_G(1));
			if (spell_fizzled == 1){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_GAIN_LIFE ){
			gain_life(player, 1);
		}
	}

	return 0;
}

int card_savor_the_moment(int player, int card, event_t event){
/* CARD_ID_SAVOR_THE_MOMENT	9665
Savor the Moment	|1|U|U
Sorcery
Take an extra turn after this one. Skip the untap step of that turn. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int total = get_total_available_lands(player);
		ai_modifier += (total >= 6 ? 96 : 48);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		target_player_skips_next_untap(player, card, player);
		return card_time_walk(player, card, event);
	}

	return basic_spell(player, card, event);
}

int card_scarscale_ritual(int player, int card, event_t event){
/* CARD_ID_SCARSCALE_RITUAL	9667
Scarscale Ritual	|1|UB
Sorcery
As an additional cost to cast ~, put a -1/-1 counter on a creature you control.
Draw two cards. */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( hybrid_casting(player, card, 0) ){
			generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select a creature you control.", 1, NULL);
			if( spell_fizzled != 1 ){
				int csvid = get_id(instance->targets[0].player, instance->targets[0].card);
				if( csvid == CARD_ID_DUSK_URCHINS ){
					ai_modifier += 48;
				}
				else{
					if( get_toughness(instance->targets[0].player, instance->targets[0].card) > 1 ){
						switch( csvid ){
								case CARD_ID_CARNIFEX_DEMON:
								case CARD_ID_DEITY_OF_SCARS:
								case CARD_ID_GRIM_POPPET:
								case CARD_ID_MORSELHOARDER:
								case CARD_ID_WICKERBOUGH_ELDER:
									ai_modifier += 48;
									break;
								default:
									ai_modifier += 24;
									break;
						}
					}
				}
			}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_scuzzback_marauders(int player, int card, event_t event){
/* CARD_ID_SCUZZBACK_MARAUDERS	9670
Scuzzback Marauders	|4|RG
Creature - Goblin Warrior 5/2
Trample
Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.)*/

	persist(player, card, event);

	return hybrid(player, card, event);
}

int card_scuzzback_scrapper(int player, int card, event_t event){
/* CARD_ID_SCUZZBACK_SCRAPPER	9671
Scuzzback Scrapper	|RG
Creature - Goblin Warrior 1/1
Wither (This deals damage to creatures in the form of -1/-1 counters.) */

	wither(player, card, event);
	return hybrid(player, card, event);
}

int card_seedcradle_witch(int player, int card, event_t event){

	/* Seedcradle Witch	|GW
	 * Creature - Elf Shaman 1/1
	 * |2|G|W: Target creature gets +3/+3 until end of turn. Untap that creature. */

	if (event == EVENT_CHECK_PUMP){
		int amt = generic_shade_amt_can_pump(player, card, 3, 0, MANACOST_XGW(2,1,1), -1);
		pumpable_power[player] += amt;
		pumpable_toughness[player] += amt;
	}

	if( ! IS_GAA_EVENT(event) ){
		return hybrid(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 3, 3);
			untap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XGW(2, 1, 1), 0, &td, "TARGET_CREATURE");
}

int card_shield_of_the_oversoul(int player, int card, event_t event){
/* CARD_ID_SHIELD_OF_THE_OVERSOUL	9673
Shield of the Oversoul	|2|GW
Enchantment - Aura
Enchant creature
As long as enchanted creature is |Sgreen, it gets +1/+1 and has indestructible. (Damage and effects that say "destroy" don't destroy it.
If its toughness is 0 or less, it's still put into its owner's graveyard.)
As long as enchanted creature is |Swhite, it gets +1/+1 and has flying. */

	hybrid(player, card, event);
	aura_ability_for_color(player, card, event, COLOR_TEST_GREEN, 1, 1, 0, SP_KEYWORD_INDESTRUCTIBLE);
	aura_ability_for_color(player, card, event, COLOR_TEST_WHITE, 1, 1, KEYWORD_FLYING, 0);
	return vanilla_aura(player, card, event, player);
}

int card_smash_to_smithereens(int player, int card, event_t event){

	/* Smash to Smithereens	|1|R
	 * Instant
	 * Destroy target artifact. ~ deals 3 damage to that artifact's controller. */

	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	if( event == EVENT_RESOLVE_SPELL ){
		if(valid_target(&td)){
			card_instance_t* instance = get_card_instance(player, card);
			kill_card( instance->targets[0].player, instance->targets[0].card, KILL_DESTROY );
			damage_player(instance->targets[0].player, 3, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
}

int card_smolder_initiate(int player, int card, event_t event){
/* CARD_ID_SMOLDER_INITIATE	9678
Smolder Initiate	|B
Creature - Elemental Shaman 1/1
Whenever a player casts a |Sblack spell, you may pay |1. If you do, target player loses 1 life. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_AI(player), &test) ){
			target_definition_t td;
			default_target_definition(player, card, &td, 0);
			td.zone = TARGET_ZONE_PLAYERS;

			card_instance_t *instance = get_card_instance( player, card );

			if( can_target(&td) && has_mana(player, COLOR_ANY, 1) &&
				charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1) )
			{
				if( pick_target(&td, "TARGET_PLAYER") ){
					lose_life(instance->targets[0].player, 1);
				}
			}
			instance->number_of_targets = 0;
		}

	}

	return 0;
}

int card_somnomancer(int player, int card, event_t event){
/* CARD_ID_SOMNOMANCER	9679
Somnomancer	|1|WU
Creature - Kithkin Wizard 2/1
When ~ enters the battlefield, you may tap target creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return hybrid(player, card, event);
}

int card_sootstoke_kindler(int player, int card, event_t event){
/* CARD_ID_SOOTSTOKE_KINDLER	9680
Sootstoke Kindler	|1|BR
Creature - Elemental Shaman 1/1
Haste
|T: Target |Sblack or |Sred creature gains haste until end of turn. */

	haste(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		hybrid(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_RED | COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		char msg[100];
		snprintf(msg, 100, "%s", get_sleighted_color_text2(player, card, "Select target %s or %s creature.", COLOR_BLACK, COLOR_RED));
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0, &td, msg);
		if( spell_fizzled != 1 ){
			ai_modifier_for_haste_granting_effects(player, card);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_HASTE);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
}

int card_spawnwrithe(int player, int card, event_t event){
	/* Spawnwrithe	|2|G
	 * Creature - Elemental 2/2
	 * Trample
	 * Whenever ~ deals combat damage to a player, put a token that's a copy of ~ onto the battlefield. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		token_generation_t token;
		copy_token_definition(player, card, &token, player, card);
		token.qty = amount;
		generate_token(&token);
	}

	return 0;
}

int card_spectral_procession(int player, int card, event_t event){
	/* Spectral Procession	|2W|2W|2W
	 * Sorcery
	 * Put three 1/1 |Swhite Spirit creature tokens with flying onto the battlefield. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SPIRIT, &token);
		token.qty = 3;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		token.key_plus = KEYWORD_FLYING;
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return monocolor_hybrid(player, card, event);
}

int card_spiteflame_witch(int player, int card, event_t event){
/* CARD_ID_SPITEFLAME_WITCH	9683
Spiteflame Witch	|1|BR
Creature - Elemental Shaman 2/1
|B|R: Each player loses 1 life. */

	hybrid(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		lose_life(player, 1);
		lose_life(1-player, 1);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_BR(1, 1), 0, NULL, NULL);
}

int card_spiteful_visions(int player, int card, event_t event){

	/* Spiteful Visions	|2|BR|BR
	 * Enchantment
	 * At the beginning of each player's draw step, that player draws an additional card.
	 * Whenever a player draws a card, ~ deals 1 damage to that player. */

	hybrid(player, card, event);

	if( event == EVENT_DRAW_PHASE ){
		event_result++;
	}

	if( card_drawn_trigger(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY) ){
		damage_player(trigger_cause_controller, 1, player, card);
	}

	return global_enchantment(player, card, event);
}

int card_steel_of_the_godhead(int player, int card, event_t event){
/* CARD_ID_STEEL_OF_THE_GODHEAD	9686
Steel of the Godhead	|2|WU
Enchantment - Aura
Enchant creature
As long as enchanted creature is |Swhite, it gets +1/+1 and has lifelink. (Damage dealt by the creature also causes its controller to gain that much life.)
As long as enchanted creature is |Sblue, it gets +1/+1 and can't be blocked. */

	hybrid(player, card, event);
	aura_ability_for_color(player, card, event, COLOR_TEST_BLUE, 1, 1, 0, SP_KEYWORD_UNBLOCKABLE);
	aura_ability_for_color(player, card, event, COLOR_TEST_WHITE, 1, 1, 0, SP_KEYWORD_LIFELINK);
	return vanilla_aura(player, card, event, player);
}

int card_sunken_ruins(int player, int card, event_t event){
/* CARD_ID_SUNKEN_RUINS	9688
Sunken Ruins	""
Land
|T: Add |C to your mana pool.
|UB, |T: Add |U|U, |U|B, or |B|B to your mana pool. */

	return card_filter_land(player, card, event, COLOR_BLUE, COLOR_BLACK, COLOR_TEST_BLUE, COLOR_TEST_BLACK, " Colorless\n U -> UU\n U -> UB\n U -> BB\n B -> UU\n B -> UB\n B -> BB\n Cancel");
}

int card_swans_of_bryn_argoll(int player, int card, event_t event){
/* CARD_ID_SWANS_OF_BRYN_ARGOLL	9689
Swans of Bryn Argoll	|2|WU|WU
Creature - Bird Spirit 4/3
Flying
If a source would deal damage to ~, prevent that damage. The source's controller draws cards equal to the damage prevented this way. */

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == card && damage->damage_target_player == player && damage->info_slot > 0 ){
				int amount = damage->info_slot;
				prevent_damage(affected_card_controller, affected_card, -1);
				draw_cards(damage->damage_source_player, amount);
			}
		}
	}

	return hybrid(player, card, event);
}

int card_sygg_river_cutthroat(int player, int card, event_t event){
/* CARD_ID_SYGG_RIVER_CUTTHROAT	9690
Sygg, River Cutthroat	|UB|UB
Legendary Creature - Merfolk Rogue 1/3
At the beginning of each end step, if an opponent lost 3 or more life this turn, you may draw a card. (Damage causes loss of life.) */

	check_legend_rule(player, card, event);

	if( eot_trigger(player, card, event) ){
		if( get_trap_condition(1-player, TRAP_DAMAGE_TAKEN) > 2 ){
			draw_cards(player, 1);
		}
	}

	return hybrid(player, card, event);
}

int card_tattermunge_maniac(int player, int card, event_t event){
/* CARD_ID_TATTERMUNGE_MANIAC	9693
Tattermunge Maniac	|RG
Creature - Goblin Warrior 2/1
~ attacks each turn if able. */

	hybrid(player, card, event);
	return attack_if_able(player, card, event);
}

int card_tattermunge_witch(int player, int card, event_t event){
/* CARD_ID_TATTERMUNGE_WITCH	9694
Tattermunge Witch	|1|RG
Creature - Goblin Shaman 2/1
|R|G: Each blocked creature gets +1/+0 and gains trample until end of turn. */

	hybrid(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int count = active_cards_count[player]-1;
		while( count > -1 ){
				if( in_play(player, count) && is_attacking(player, count) && ! is_unblocked(player, count) ){
					pump_ability_until_eot(instance->parent_controller, instance->parent_card, player, count, 1, 0, KEYWORD_TRAMPLE, 0);
				}
				count--;
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_RG(1, 1), 0, NULL, NULL);
}

int card_thistledown_liege(int player, int card, event_t event){
/* CARD_ID_THISTLEDOWN_LIEGE	9696
Thistledown Liege	|1|WU|WU|WU
Creature - Kithkin Knight 1/3
Flash
Other |Swhite creatures you control get +1/+1.
Other |Sblue creatures you control get +1/+1. */

	liege(player, card, event, COLOR_TEST_WHITE, COLOR_TEST_BLUE);
	return flash(player, card, event);
}

int card_thought_reflection(int player, int card, event_t event){

	/* Thought Reflection	|4|U|U|U
	 * Enchantment
	 * If you would draw a card, draw two cards instead. */

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && ! is_humiliated(player, card) &&
		reason_for_trigger_controller == player && !suppress_draw )
	{
		card_instance_t* instance = get_card_instance(player, card);
		if (instance->info_slot == 1){
			return 0;
		}
		if( event == EVENT_TRIGGER){
			event_result |= 2u;
		}
		else if( event == EVENT_RESOLVE_TRIGGER ){
			instance->info_slot = 1;	// Prevent from triggering on its own draws
			draw_cards(player, 2);
			instance->info_slot = 0;
			suppress_draw = 1;
		}
	}

	return global_enchantment(player, card, event);
}

int card_thoughtweft_gambit(int player, int card, event_t event){
/* CARD_ID_THOUGHTWEFT_GAMBIT	9699
Thoughtweft Gambit	|4|WU|WU
Instant
Tap all creatures your opponents control and untap all creatures you control. */

	if(event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(player, card, 1-player, &this_test, ACT_TAP);
		new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
		kill_card(player, card, KILL_DESTROY);
	}

	return hybrid(player, card, event);
}

int card_torrent_of_souls(int player, int card, event_t event){
/* CARD_ID_TORRENT_OF_SOULS	9702
Torrent of Souls	|4|BR
Sorcery
Return up to one target creature card from your graveyard to the battlefield if |B was spent to cast ~.
Creatures target player controls get +2/+0 and gain haste until end of turn if |R was spent to cast ~. (Do both if |B|R was spent.) */

	if( event == EVENT_MODIFY_COST ){
		if( has_mana_multi(player, 4, 0, 0, 0, 1, 0) ){
			null_casting_cost(player, card);
			get_card_instance( player, card )->targets[1].card = COLOR_BLACK;
		}
		else{
			get_card_instance( player, card )->targets[1].card = -1;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			card_instance_t *instance = get_card_instance( player, card );
			if( instance->targets[1].card == COLOR_BLACK ){
				int cless = get_updated_casting_cost(player, card, -1, event, -1);
				char buffer[500];
				int mode = (1<<3);
				int pos = 0;
				int ai_choice = 1;
				if( has_mana_multi(player, cless, 1, 0, 0, 0, 0) ){
					pos +=scnprintf(buffer+pos, 500-pos, " Pay %dB\n", cless);
					mode |=(1<<0);
				}
				if( has_mana_multi(player, cless, 0, 0, 0, 1, 0) ){
					pos +=scnprintf(buffer+pos, 500-pos, " Pay %dR\n", cless);
					mode |=(1<<1);
				}
				if( has_mana_multi(player, cless-1, 1, 0, 0, 1, 0) ){
					pos +=scnprintf(buffer+pos, 500-pos, " Pay %dBR\n", cless-1);
					mode |=(1<<2);
					ai_choice = 2;
				}
				pos +=scnprintf(buffer+pos, 500-pos, " Cancel");

				int choice = do_dialog(player, player, card, -1, -1, buffer, ai_choice);
				while( !((1<<choice) & mode) ){
						choice++;
				}
				int black = 1;
				int red = 0;
				if( choice == 3 ){
					spell_fizzled = 1;
					return 0;
				}
				if( choice == 1 ){
					red = 1;
					black = 0;
				}
				if( choice == 2 ){
					red = 1;
					cless--;
				}
				charge_mana_multi(player, cless, black, 0, 0, red, 0);
				if( spell_fizzled != 1 ){
					choice++;
					instance->info_slot = 0;
					if( choice & 1 ){
						if( ! graveyard_has_shroud(2) ){
							char msg[100] = "Select a creature card.";
							test_definition_t this_test;
							new_default_test_definition(&this_test, TYPE_CREATURE, msg);
							if( new_select_target_from_grave(player, card, player, 0, AI_MAX_CMC, &this_test, 0) != -1 ){
								instance->info_slot |= 1;
							}
						}
					}
					if( choice & 2 ){
						instance->info_slot |= 2;
					}
				}
			}
			else{
				if( ! graveyard_has_shroud(2) ){
					char msg[100] = "Select a creature card.";
					test_definition_t this_test;
					new_default_test_definition(&this_test, TYPE_CREATURE, msg);
					if( new_select_target_from_grave(player, card, player, 0, AI_MAX_CMC, &this_test, 0) != -1 ){
						instance->info_slot |= 1;
					}
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card );
		if( (instance->info_slot & 1) ){
			int selected = validate_target_from_grave(player, card, player, 0);
			if( selected != -1 ){
				reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
			}
		}
		if( (instance->info_slot & 2) ){
			pump_subtype_until_eot(player, card, player, -1, 2, 0, 0, SP_KEYWORD_HASTE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int tower_above_legacy(int player, int card, event_t event)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (instance->damage_target_player < 0 || instance->damage_target_card < 0)
	return 0;

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(instance->damage_target_player, instance->damage_target_card))
	event_result += 4;

  if (event == EVENT_ABILITIES && affect_me(instance->damage_target_player, instance->damage_target_card))
	{
	  event_result |= KEYWORD_TRAMPLE;
	  wither(player, card, event);
	}

  if (!is_humiliated(instance->damage_target_player, instance->damage_target_card)
	  && declare_attackers_trigger(player, card, event, 0, instance->damage_target_player, instance->damage_target_card))
	{
	  target_definition_t td;
	  default_target_definition(instance->damage_target_player, instance->damage_target_card, &td, TYPE_CREATURE);
	  td.allow_cancel = 0;

	  instance->number_of_targets = 0;
	  if (can_target(&td) && new_pick_target(&td, "Select target creature your opponent controls.", 1, GS_LITERAL_PROMPT))
		target_must_block_me(player, card, instance->targets[1].player, instance->targets[1].card, 1);
	}

	if (eot_trigger(player, card, event))
	  kill_card(player, card, KILL_EXILE);

	return 0;
}

int card_tower_above(int player, int card, event_t event){
/* CARD_ID_TOWER_ABOVE	9703
Tower Above	|2G|2G|2G
Sorcery
(|2G can be paid with any two mana or with |G. This card's converted mana cost is 6.)
Until end of turn, target creature gets +4/+4 and gains trample, wither, and "When this creature attacks, target creature blocks it this turn if able."
(It deals damage to creatures in the form of -1/-1 counters.) */

	modify_cost_for_hybrid_spells(player, card, event, 1);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( hybrid_casting(player, card, 1) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &tower_above_legacy, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_trip_noose(int player, int card, event_t event){
/* CARD_ID_TRIP_NOOSE	9705
Trip Noose	|2
Artifact
|2, |T: Tap target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_CREATURE");
}

int card_turn_to_mist(int player, int card, event_t event){
/* CARD_ID_TURN_TO_MIST	9706
Turn to Mist	|1|WU
Instant
Exile target creature. Return that card to the battlefield under its owner's control at the beginning of the next end step. */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( hybrid_casting(player, card, 0) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			remove_until_eot(player, card,instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_twilight_shepherd(int player, int card, event_t event){
/* CARD_ID_TWILIGHT_SHEPHERD	9707
Twilight Shepherd	|3|W|W|W
Creature - Angel 5/5
Flying, vigilance
When ~ enters the battlefield, return to your hand all cards in your graveyard that were put there from the battlefield this turn.
Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.)*/

	vigilance(player, card, event);

	persist(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		return_all_dead_this_turn_to_hand(player, TYPE_ANY);
	}

	return 0;
}

int card_tyrannize(int player, int card, event_t event){
/* CARD_ID_TYRANNIZE	9708
Tyrannize	|3|BR|BR
Sorcery
Target player discards his or her hand unless he or she pays 7 life. */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( hybrid_casting(player, card, 0) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int choice = 1;
			if( can_pay_life(instance->targets[0].player, 7) ){
				int ai_choice = 1;
				if( life[instance->targets[0].player]-7 > 6 ){
					ai_choice = 0;
				}
				choice = do_dialog(instance->targets[0].player, player, card, -1, -1, " Pay 7 life\n Discard all", ai_choice);
			}
			if( choice == 0 ){
				lose_life(instance->targets[0].player, 7);
			}
			if( choice == 1 ){
				new_discard_all(instance->targets[0].player, player);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_umbral_mantle(int player, int card, event_t event){
/* CARD_ID_UMBRAL_MANTLE	9709
Umbral Mantle	|3
Artifact - Equipment
Equipped creature has "|3, |Q: This creature gets +2/+2 until end of turn." (|Q is the untap symbol.)
Equip |0 */

	card_instance_t *instance = get_card_instance(player, card);

#define CAN_PUMP	(!is_humiliated(player, card)																				\
					 && is_equipping(player, card)																				\
					 && player == instance->targets[8].player	/* engine limitation - other player won't be able to click */	\
					 && can_use_activated_abilities(instance->targets[8].player, instance->targets[8].card)						\
					 && has_mana_for_activated_ability(instance->targets[8].player, instance->targets[8].card, MANACOST_X(3))	\
					 && is_tapped(instance->targets[8].player, instance->targets[8].card)										\
					 && !is_sick(instance->targets[8].player, instance->targets[8].card))
#define CAN_EQUIP	(can_use_activated_abilities(player, card) && can_activate_basic_equipment(player, card, event, 0))

	if (event == EVENT_CAN_ACTIVATE){
		return CAN_EQUIP || CAN_PUMP;
	}
	else if( event == EVENT_ACTIVATE ){
			int choice = 0;
			if (CAN_EQUIP){
				if (CAN_PUMP){
					choice = do_dialog(player, player, card, -1, -1, " Change equipped creature\n Untap & Pump\n Cancel", 1);
				}
			}
			else{
				choice = 1;
			}

			instance->info_slot = 66+choice;
			if (choice == 0){
				activate_basic_equipment(player, card, 0);
			}
			else if (choice == 1){
				if (charge_mana_for_activated_ability(instance->targets[8].player, instance->targets[8].card, MANACOST_X(3))){
					untap_card(instance->targets[8].player, instance->targets[8].card);
				}
			}
			else{
				spell_fizzled = 1;
			}
	}
	else if(event == EVENT_RESOLVE_ACTIVATION ){
			if( instance->info_slot == 66 ){
				resolve_activation_basic_equipment(player, card);
			}
			if( instance->info_slot == 67 ){
				pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[8].player, instance->targets[8].card, 2, 2);
			}
	}

	return 0;
#undef CAN_EQUIP
#undef CAN_PUMP
}

int card_valleymaker(int player, int card, event_t event){
	/*
	  Valleymaker 5{R/G}
	  Creature - Giant Shaman 5/5
	  {T}, Sacrifice a Mountain: Valleymaker deals 3 damage to target creature.
	  {T}, Sacrifice a Forest: Choose a player. That player adds {G}{G}{G} to his or her mana pool.
	*/
	if(event == EVENT_COUNT_MANA && affect_me(player, card) ){
		test_definition_t test2;
		default_test_definition(&test2, TYPE_LAND);
		test2.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);

		target_definition_t td2;
		default_target_definition(player, card, &td2, 0);
		td2.zone = TARGET_ZONE_PLAYERS;
		td2.preferred_controller = player;
		td2.illegal_abilities = 0;

		if( generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_UNTAPPED,
																MANACOST0, 0, &td2, NULL, 0, &test2, 0) )
		{
			declare_mana_available(player, COLOR_GREEN, 3);
		}
	}

	if( IS_GAA_EVENT(event) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, get_hacked_land_text(player, card, "Select a %s to sacrifice.", SUBTYPE_MOUNTAIN));
		test.subtype = get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		test_definition_t test2;
		new_default_test_definition(&test2, TYPE_LAND, get_hacked_land_text(player, card, "Select a %s to sacrifice.", SUBTYPE_FOREST));
		test2.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);

		target_definition_t td2;
		default_target_definition(player, card, &td2, 0);
		td2.zone = TARGET_ZONE_PLAYERS;
		td2.preferred_controller = player;
		td2.illegal_abilities = 0;

		card_instance_t *instance = get_card_instance(player, card);

		enum{
			CHOICE_DAMAGE = 1,
			CHOICE_MANA
		};

		if( event == EVENT_CAN_ACTIVATE ){
			if( generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST0, 0,
																&td, NULL, 0, &test, 0) )
			{
				return 1;
			}
			if( generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST0, 0,
																&td2, NULL, 0, &test2, 0) )
			{
				return 1;
			}
		}

		if( event == EVENT_ACTIVATE ){
			int abils[3] = {
								0,
								generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE,
																					GAA_CAN_TARGET | GAA_UNTAPPED,
																					MANACOST0, 0, &td, NULL, 0, &test, 0),
								generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE,
																					GAA_CAN_TARGET | GAA_UNTAPPED,
																					MANACOST0, 0, &td2, NULL, 0, &test2, 0)
			};
			int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
								"3 damage to target creature", abils[1] && ! paying_mana(), 5,
								"Get GGG", abils[2], 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}

			if( choice == CHOICE_DAMAGE ){
				return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST0, 0,
																		&td, "TARGET_CREATURE", 0, &test, 0);
			}

			if( choice == CHOICE_MANA ){
				generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST0, 0,
																&td2, "TARGET_PLAYER", 0, &test2, 0);
				if( spell_fizzled != 1 ){
					produce_mana(instance->targets[0].player, COLOR_GREEN, 3);
				}
			}
		}

		if(event == EVENT_RESOLVE_ACTIVATION ){
			if( instance->info_slot == CHOICE_DAMAGE && valid_target(&td) ){
				damage_target0(player, card, 3);
			}
		}
	}

	return hybrid(player, card, event);
}

static int remove_untargettable(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me( player, card ) &&
		reason_for_trigger_controller == affected_card_controller
	  ){
		if( trigger_cause_controller == instance->targets[0].player && trigger_cause == instance->targets[0].card ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					state_untargettable(trigger_cause_controller, trigger_cause, 0);
					kill_card(player, card, KILL_EXILE);
			}
		}
	}
	return 0;
}

int card_vexing_shusher(int player, int card, event_t event){
/* CARD_ID_VEXING_SHUSHER	9711
Vexing Shusher	|RG|RG
Creature - Goblin Shaman 2/2
~ can't be countered.
|RG: Target spell can't be countered by spells or abilities. */

	cannot_be_countered(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return hybrid(player, card, event);
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_R(1));
		if( has_mana_hybrid(player, 1, COLOR_GREEN, COLOR_RED, c1) ){
			return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK | GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0,
											&td, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( card_on_stack_controller != player ){
			ai_modifier-=50;
		}
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_R(1));
		if( charge_mana_hybrid(player, card, 1, COLOR_GREEN, COLOR_RED, c1) ){
			return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK | GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0,
											&td, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( counterspell_validate(player, card, NULL, 0) ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			if( is_what(instance->targets[0].player, instance->targets[0].card, TYPE_PERMANENT) ){
				int legacy = create_legacy_effect(player, card, &remove_untargettable);
				get_card_instance(player, legacy)->targets[0] = instance->targets[0];	// struct copy
			}
		}
	}

	return 0;
}

int card_wheel_of_sun_and_moon(int player, int card, event_t event)
{
  /* Wheel of Sun and Moon	|GW|GW
   * Enchantment - Aura
   * Enchant player
   * If a card would be put into enchanted player's graveyard from anywhere, instead that card is revealed and put on the bottom of that player's library. */

  hybrid(player, card, event);

  int ench = get_card_instance(player, card)->targets[0].player;

  if (if_a_card_would_be_put_into_graveyard_from_library_do_something_instead(player, card, event, ench, RESOLVE_TRIGGER_MANDATORY, NULL))
	{
	  reveal_card_iid(player, card, deck_ptr[trigger_cause_controller][trigger_cause]);
	  put_card_in_deck_to_bottom(trigger_cause_controller, trigger_cause);
	  replace_milled = 1;
	  return 0;
	}

  if (if_a_card_would_be_put_into_graveyard_from_anywhere_but_library_do_something_instead(player, card, event, ench, RESOLVE_TRIGGER_MANDATORY, NULL))
	{
	  reveal_card_iid(player, card, trigger_cause);
	  deck_ptr[trigger_cause_controller][count_deck(trigger_cause_controller)] = trigger_cause;
	  replace_milled = 1;
	  return 0;
	}
  return curse(player, card, event);
}

int card_wilt_leaf_cavaliers(int player, int card, event_t event){
	vigilance( player, card, event );
	return hybrid(player, card, event);
}

int card_wilt_leaf_liege(int player, int card, event_t event){
/* CARD_ID_WILT_LEAF_LIEGE	9720
Wilt-Leaf Liege	|1|GW|GW|GW
Creature - Elf Knight 4/4
Other |Sgreen creatures you control get +1/+1.
Other |Swhite creatures you control get +1/+1.
If a spell or ability an opponent controls causes you to discard ~, put it onto the battlefield instead of putting it into your graveyard.*/

	return liege(player, card, event, COLOR_TEST_GREEN, COLOR_TEST_WHITE);
}

int card_windbrisk_raptor(int player, int card, event_t event)
{
/* CARD_ID_WINDBRISK_RAPTOR	9721
Windbrisk Raptor	|5|W|W
Creature - Bird 5/7
Flying
Attacking creatures you control have lifelink. */

  if (event == EVENT_ABILITIES
	  && affected_card_controller == player
	  && is_attacking(affected_card_controller, affected_card)
	  && in_play(player, card) && !is_humiliated(player, card))
	lifelink(affected_card_controller, affected_card, event);

  return 0;
}

int card_witherscale_wurm(int player, int card, event_t event){
	/* Witherscale Wurm	|4|G|G
	 * Creature - Wurm 9/9
	 * Whenever ~ blocks or becomes blocked by a creature, that creature gains wither until end of turn.
	 * Whenever ~ deals damage to an opponent, remove all -1/-1 counters from it. */

	if( event == EVENT_DECLARE_BLOCKERS && ! is_humiliated(player, card) ){
		if( current_turn == player ){
			int count = active_cards_count[1-player]-1;
			while( count > -1 ){
					if( in_play(1-player, count) && is_what(1-player, count, TYPE_CREATURE) ){
						card_instance_t *instance = get_card_instance( 1-player, count );
						if( instance->blocking == card ){
							pump_ability_until_eot(player, card, 1-player, count, 0, 0, 0, SP_KEYWORD_WITHER);
						}
					}
					count--;
			}
		}
		else{
			card_instance_t *instance = get_card_instance( player, card );
			if( instance->blocking < 255 ){
				pump_ability_until_eot(player, card, 1-player, instance->blocking, 0, 0, 0, SP_KEYWORD_WITHER);
			}
		}
	}

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_OPPONENT) ){
		remove_all_counters(player, card, COUNTER_M1_M1);
	}

	return 0;
}

int card_wooded_bastion(int player, int card, event_t event){
/* CARD_ID_WOODED_BASTION	9725
Wooded Bastion	""
Land
|T: Add |C to your mana pool.
|GW, |T: Add |G|G, |G|W, or |W|W to your mana pool. */

	return card_filter_land(player, card, event, COLOR_GREEN, COLOR_WHITE, COLOR_TEST_GREEN, COLOR_TEST_WHITE, " Colorless\n G -> GG\n G -> GW\n G -> WW\n W -> GG\n W -> GW\n W -> WW\n Cancel");
}

int card_woodfall_primus(int player, int card, event_t event){
/* CARD_ID_WOODFALL_PRIMUS	9726
Woodfall Primus	|5|G|G|G
Creature - Treefolk Shaman 6/6
Trample
When ~ enters the battlefield, destroy target noncreature permanent.
Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.)*/

	persist(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT );
		td.illegal_type = TYPE_CREATURE;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target non-land permanent.", 0, GS_LITERAL_PROMPT) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_worldpurge(int player, int card, event_t event){
/* Worldpurge	|4|WU|WU|WU|WU
 * Sorcery
 * Return all permanents to their owners' hands. Each player chooses up to seven cards in his or her hand, then shuffles the rest
 * 		into his or her library. Empty all mana pools. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return hybrid(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "");
		this_test.subtype = SUBTYPE_AURA;
		new_manipulate_all(player, card, 2, &this_test, ACT_BOUNCE);

		this_test.subtype = 0;
		new_manipulate_all(player, card, 2, &this_test, ACT_BOUNCE);

		new_default_test_definition(&this_test, 0, "Select a card to keep.");

		APNAP(p, {
					int orig_hand[2][hand_count[p]];
					int ohc = 0;
					int c;
					for(c=0; c<active_cards_count[p]; c++){
						if( in_hand(p, c) ){
							orig_hand[0][ohc] = get_original_internal_card_id(p, c);
							orig_hand[1][ohc] = c;
							ohc++;
						}
					}
					int max = 0;
					while( max < 7 && ohc ){
							int selected = select_card_from_zone(p, p, orig_hand[0], ohc, 0, AI_MIN_VALUE, -1, &this_test);
							if( selected != -1 ){
								state_untargettable(p, orig_hand[1][selected], 1);
								max++;
								int k;
								for(k=selected; k<ohc; k++){
									orig_hand[0][k] = orig_hand[0][k+1];
									orig_hand[1][k] = orig_hand[1][k+1];
								}
								ohc--;
							}
							else{
								break;
							}
					}
					for(c=active_cards_count[p]-1; c > -1; c--){
						if( in_hand(p, c) ){
							if( check_state(p, c, STATE_CANNOT_TARGET) ){
								remove_state(p, c, STATE_CANNOT_TARGET);
							}
							else{
								put_on_top_of_deck(p, c);
							}
						}
					}
					shuffle(p);
					int clr;
					for (clr = COLOR_COLORLESS; clr <= COLOR_ANY; ++clr){
						mana_pool[p][clr] = 0;
					}
				};
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_worth_the_raidmother(int player, int card, event_t event){
	/* Wort, the Raidmother	|4|RG|RG
	 * Legendary Creature - Goblin Shaman 3/3
	 * When ~ enters the battlefield, put two 1/1 |Sred and |Sgreen Goblin Warrior creature tokens onto the battlefield.
	 * Each |Sred or |Sgreen instant or sorcery spell you cast has conspire. */

	card_instance_t *instance = get_card_instance(player, card);

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_tokens_by_id(player, card, CARD_ID_GOBLIN_WARRIOR, 2);
	}

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller &&
		trigger_cause_controller == player && ! check_special_flags(trigger_cause_controller, trigger_cause, SF_NOT_CAST) )
	{
		int c1 = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		int c2 = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		int clr = get_color(trigger_cause_controller, trigger_cause);
		if( is_what(trigger_cause_controller, trigger_cause, TYPE_SPELL) && (clr && (c1 | c2)) )
		{
			target_definition_t td1;
			default_target_definition(player, card, &td1, TYPE_CREATURE);
			td1.allowed_controller = player;
			td1.preferred_controller = player;
			td1.illegal_abilities = 0;
			td1.illegal_state = TARGET_STATE_TAPPED;
			td1.required_color = clr;
			if( target_available(player, card, &td1) > 1 ){
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_AI(player);
				}
				else if( event == EVENT_RESOLVE_TRIGGER ){
						int total = 0;
						while( total < 2 && can_target(&td1) ){
								if( select_target(player, card, &td1, "Select a creature which shares a color with the spell.", &(instance->targets[total])) ){
									state_untargettable(instance->targets[total].player, instance->targets[total].card, 1);
									total++;
								}
								else{
									break;
								}
						}
						int i;
						for(i=0; i<total; i++){
							state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
							if( total == 2 ){
								tap_card(instance->targets[i].player, instance->targets[i].card);
							}
						}
						if( total == 2 ){
							copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
						}
				}
			}
		}
	}

	return hybrid(player, card, event);
}

int card_wound_reflection(int player, int card, event_t event){
/* CARD_ID_WOUND_REFLECTION	9729
Wound Reflection	|5|B
Enchantment
At the beginning of each end step, each opponent loses life equal to the life he or she lost this turn. (Damage causes loss of life.) */

	if( eot_trigger(player, card, event) ){
		lose_life(1-player, get_trap_condition(1-player, TRAP_LIFE_LOST));
	}
	return global_enchantment(player, card, event);
}

