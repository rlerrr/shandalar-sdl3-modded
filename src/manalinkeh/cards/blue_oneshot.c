#include "manalink.h"

int card_compulsive_research(int player, int card, event_t event){
  /* CARD_ID_COMPULSIVE_RESEARCH	1549
   * Compulsive Research	|2|U
   * Sorcery
   * Target player draws three cards. Then that player discards two cards unless he or she discards a land card. */

    /* Original Code: 004E81DF */
    target_definition_t td;
    card_instance_t *instance = get_card_instance( player, card);
    if( event == EVENT_CAN_CAST ){
        return 1;
    }
    else if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
        default_target_definition(player, card, &td, TYPE_LAND);
        td.zone = TARGET_ZONE_PLAYERS;
        pick_target(&td, "TARGET_PLAYER");
    }
    else if( event == EVENT_RESOLVE_SPELL ){
        int who = instance->targets[0].player;
        draw_cards(who, 3);
        default_target_definition(player, card, &td, TYPE_LAND);
        td.allowed_controller = who;
        td.preferred_controller = who;
        td.who_chooses = who;
        td.zone = TARGET_ZONE_HAND;
        td.illegal_abilities = 0;
        td.allow_cancel=0;
        int choice = 1;
        if( can_target(&td) ){
            choice = do_dialog(who, player, card, -1, -1, " Discard a Land\nDiscard 2 Cards\n", 0);
        }
        if( choice == 0 ){
            select_target(player, card, &td, "Choose a land to discard", NULL);
            discard_card(who, instance->targets[0].card);
        }
        else{
            discard(who, 0, 0);
            discard(who, 0, 0);
        }
        kill_card(player, card, KILL_SACRIFICE);
    }
    return 0;
}

int card_thirst_for_knowledge(int player, int card, event_t event){
  /* CARD_ID_THIRST_FOR_KNOWLEDGE	1545
   * Thirst for Knowledge	|2|U
   * Instant
   * Draw three cards. Then discard two cards unless you discard an artifact card. */

    /* Original Code: 004E76FB */
    card_instance_t *instance = get_card_instance( player, card);
    if( event == EVENT_CAN_CAST ){
        return 1;
    }
    else if( event == EVENT_RESOLVE_SPELL ){
        int who = player;
        draw_cards(who, 3);
        target_definition_t td;
        default_target_definition(player, card, &td, TYPE_ARTIFACT);
        td.allowed_controller = who;
        td.preferred_controller = who;
        td.who_chooses = who;
        td.zone = TARGET_ZONE_HAND;
        td.illegal_abilities = 0;
        td.allow_cancel=0;
        int choice = 1;
        if( can_target(&td) ){
            choice = do_dialog(who, player, card, -1, -1, " Discard an Artifact\nDiscard 2 Cards\n", 0);
        }
        if( choice == 0 ){
            select_target(player, card, &td, "Choose an artifact to discard", NULL);
            discard_card(who, instance->targets[0].card);
        }
        else{
            discard(who, 0, 0);
            discard(who, 0, 0);
        }
        kill_card(player, card, KILL_SACRIFICE);
    }
    return 0;
}

int card_mind_bend(int player, int card, event_t event){
  /* CARD_ID_MIND_BEND	1086
   * Mind Bend	|U
   * Instant
   * Change the text of target permanent by replacing all instances of one color word with another or one basic land type with another.
   * (For example, you may change "nonblack creature" to "nongreen creature" or "forestwalk" to "islandwalk." This effect lasts indefinitely.) */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_PERMANENT);
    card_instance_t *instance = get_card_instance( player, card);
    if( event == EVENT_CAN_CAST && can_target(&td) ){
        return 1;
    }
    else if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int choice = do_dialog(player, player, card, -1, -1, " Change color word\n Change land word", 0);
		instance->targets[3].player = choice;

		if( instance->targets[3].player == 1 ){
			return card_magical_hack(player, card, event);
		}
		else if( instance->targets[3].player == 0 ){
			return card_sleight_of_mind(player, card, event);
		}
    }
	else if( event == EVENT_RESOLVE_SPELL ){
			if( instance->targets[3].player == 1 ){
				return card_magical_hack(player, card, event);
			}
			else if( instance->targets[3].player == 0 ){
				return card_sleight_of_mind(player, card, event);
			}
    }

    return 0;
}

int card_echoing_truth(int player, int card, event_t event){
  /* CARD_ID_ECHOING_TRUTH	6887
   * Echoing Truth	|1|U
   * Instant
   * Return target nonland permanent and all other permanents with the same name as that permanent to their owners' hands. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_NONLAND);
    td.illegal_type = TYPE_LAND;
    if( event == EVENT_CAN_CAST ){
        if(  target_available(player, card, &td) ){
            return 1;
        }
    }
    else if( event == EVENT_CAST_SPELL ){
        if( ! affect_me(player, card) ){ return 0; }
        if( ! select_target(player, card, &td, "Select target non-land permanent", NULL) ){
            spell_fizzled = 1;
        }
    }
    else if( event == EVENT_RESOLVE_SPELL ){
        if(validate_target(player, card, &td, 0)){
            card_instance_t *instance = get_card_instance(player, card);
            int id = get_id( instance->targets[0].player, instance->targets[0].card );
            int p;
            for( p = 0; p < 2; p++){
                int count = 0;
                while(count < active_cards_count[p]){
                    if( in_play(p, count) && get_id(p, count) == id ){
                        bounce_permanent( p, count );
                    }
                    count++;
                }
            }
        }
        kill_card(player, card, KILL_DESTROY);
    }
    return 0;
}

int card_gifts_ungiven(int player, int card, event_t event){
  /* Gifts Ungiven	|3|U
   * Instant
   * Search your library for up to four cards with different names and reveal them. Target opponent chooses two of those cards. Put the chosen cards into your graveyard and the rest into your hand. Then shuffle. */

    return 0;
}

int card_gifts_ungiven2(int player, int card, event_t event){
  /* Gifts Ungiven	|3|U
   * Instant
   * Search your library for up to four cards with different names and reveal them. Target opponent chooses two of those cards. Put the chosen cards into your graveyard and the rest into your hand. Then shuffle. */

//    card_instance_t *instance = get_card_instance(player, card);

    if( event == EVENT_CAN_CAST ){
        return 1;
    }
    else if( event == EVENT_RESOLVE_SPELL ){
            int cards = 4;
            if( count_deck(player) < cards ){
                cards = count_deck(player);
            }
            if( cards > 0 ){
                int i=0;
                int chosen[4] = {-1, -1, -1, -1};
                int *deck = deck_ptr[player];
                while( i < cards ){
						if( player == HUMAN ){
							int selected = pick_card_from_deck(player);
							if( i < 1 ){
							   chosen[i] = deck[selected];
							   remove_card_from_deck(player, selected);
							   i++;
							}
							else{
								int k;
								int valid = 1;
								for(k=0; k<i;k++){
									if( cards_data[deck[selected]].id == cards_data[chosen[k]].id ){
										valid = 0;
									}
								}
								if( valid == 1 ){
									chosen[i] = deck[selected];
									remove_card_from_deck(player, selected);
									i++;
								}
							}
						}
						else{
							int c1 = 0;
							int max = 0;
							int selected = -1;
							while( deck[c1] != -1 ){
									if( get_base_value(-1, deck[c1]) > max ){
										if( i == 0 ){
											max = get_base_value(-1, deck[c1]);
											selected = c1;
										}
										else{
											int good = 1;
											int k;
											for(k=0; k<i; k++){
												if( deck[c1] == chosen[k] ){
													good = 0; 
													break;
												}
											}
											if( good == 1 ){
												max = get_base_value(-1, deck[c1]);
												selected = c1;
											}
										}
									}
									c1++;
							}
							chosen[i] = deck[selected];
							remove_card_from_deck(player, selected);
							i++;
						}
                }
                int cloned_chosen[4];
                 i=0;
                 for(i=0;i<cards;i++){
					cloned_chosen[i] = chosen[i];
                 }
                 i=0;
                 for(i=0;i<2;i++){
                     int selected = -1;
                     if( 1-player != AI ){
                         while( selected == -1 ){
                                selected = show_deck(1-player, chosen, cards, "Choose a card", 0, gs_done_008b40e0 );
								if( cards_data[chosen[selected]].id == CARD_ID_RULES_ENGINE ){
									selected = -1;
								}
                         }
                     }
                     else{
                          int x;
                          int max_value = 1000;
                          for(x=0; x<cards; x++){
                              if( chosen[x] != -1 && cards_data[chosen[x]].id != CARD_ID_RULES_ENGINE ){
                                  card_ptr_t* c = cards_ptr[ cards_data[chosen[x]].id ];
                                  if( c->ai_base_value < max_value ){
                                      max_value = c->ai_base_value;
                                      selected = x;
                                  }
                              }
                          }
                     }
                     chosen[selected] = get_internal_card_id_from_csv_id(CARD_ID_RULES_ENGINE);
					 if( cards < 2 ){
						break;
					 }
                 }

                 i=0;
                 for(i=0;i<cards;i++){
					 int card_added = add_card_to_hand(player, cloned_chosen[i]);
					 if( chosen[i] != cloned_chosen[i] ){
						kill_card(player, card_added, KILL_DESTROY);
					 }
				}
              }
              gain_life(player, 0);
              shuffle(player);
              kill_card(player, card, KILL_DESTROY);
    }

    return 0;
}

int card_mental_note(int player, int card, event_t event){
  /* CARD_ID_MENTAL_NOTE	5974
   * Mental Note	|U
   * Instant
   * Put the top two cards of your library into your graveyard.
   * Draw a card. */

    if( event == EVENT_CAN_CAST ){
        return 1;
    }
    else if(event == EVENT_RESOLVE_SPELL ){
        mill( player,  2 );
        draw_a_card( player );
        kill_card(player, card, KILL_DESTROY);
    }
    return 0;
}

int card_prosperity(int player, int card, event_t event){
  /* CARD_ID_PROSPERITY	1497
   * Prosperity	|X|U
   * Sorcery
   * Each player draws X cards. */

    card_instance_t *instance = get_card_instance( player, card);
    if(event == EVENT_CAN_CAST ){
        return ! check_battlefield_for_id(2, CARD_ID_GADDOCK_TEEG);
    }
    else if(event == EVENT_CAST_SPELL ){
        if( ! affect_me(player, card) ){ return 0; }
        instance->info_slot = x_value;
    }
    else if(event == EVENT_RESOLVE_SPELL){
        draw_cards(HUMAN,  instance->info_slot );
        draw_cards(AI,  instance->info_slot );
        kill_card(player, card, KILL_DESTROY);
    }
    return 0;
}

int card_gush(int player, int card, event_t event){
  /* CARD_ID_GUSH	4482
   * Gush	|4|U
   * Instant
   * You may return two |H1Islands you control to their owner's hand rather than pay ~'s mana cost.
   * Draw two cards. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_LAND);
    td.required_subtype = SUBTYPE_ISLAND;
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.illegal_abilities = 0;
    td.allow_cancel = 0;

    if( event == EVENT_MODIFY_COST ){
        if( target_available(player, card, &td) > 1 ){
            COST_COLORLESS-=4;
            COST_BLUE--;
        }
    }
    else if(event == EVENT_CAST_SPELL  && affect_me(player, card) ){
			if( ! played_for_free(player, card) ){
				int pitch = 0;
				int choice = 1;
				if( target_available(player, card, &td) > 1 ){
					pitch = 1;
				}
				if( pitch == 1 && has_mana_to_cast_id(player, event, get_id(player, card)) ){
					char buffer[50];
					snprintf(buffer, 50, " Return 2 Islands\n Play normally" );
					choice = do_dialog(player, player, card, -1, -1, buffer, 1);
				}
				else if(pitch == 1){
						choice = 0;
				}

				if( choice == 1 && pitch == 1){
					charge_mana_from_id(player, card, event, get_id(player, card));
				}
				else if(choice == 0){
						card_instance_t *instance = get_card_instance(player, card);
						int i;
						for(i=0;i<2;i++){
							select_target(player, card, &td, "Choose an Island", NULL);
							int target = instance->targets[0].card;
							bounce_permanent( player, target );
						}
				}
			}
    }
    else if( event == EVENT_RESOLVE_SPELL ){
        draw_cards(player, 2);
        kill_card(player, card, KILL_DESTROY);
    }
    else if ( event == EVENT_CAN_CAST ){
        return 1;
    }
    return 0;
}

int card_disrupt(int player, int card, event_t event){
  /* Disrupt	|U
   * Instant
   * Counter target instant or sorcery spell unless its controller pays |1.
   * Draw a card. */

    if(event == EVENT_CAN_CAST ){
        int result =  card_force_spike(player, card, event);
        if( result > 0 ){
            if( ! is_what(card_on_stack_controller, card_on_stack, TYPE_PERMANENT) ){
                return result;
            }
            else{
                 return 0;
            }
        }
        else{
             return 0;
        }
    }
    else if(event == EVENT_RESOLVE_SPELL ){
            if( spell_fizzled != 1 ){
                draw_a_card(player);
            }
            return card_force_spike(player, card, event);
    }
    else{
         return card_force_spike(player, card, event);
    }
    return 0;
}

int card_brainstorm(int player, int card, event_t event){
  /* Brainstorm	|U	0x200133F
   * Instant
   * Draw three cards, then put two cards from your hand on top of your library in any order. */

    if( event == EVENT_CAN_CAST ){
        return 1;
    }
    else if(event == EVENT_RESOLVE_SPELL ){
        draw_cards(player, 3);
        int i;
        target_definition_t td;
        default_target_definition(player, card, &td, TYPE_ANY);
        td.zone = TARGET_ZONE_HAND;
        td.illegal_abilities = 0;
        td.allow_cancel = 0;
        td.allowed_controller = player;
        td.preferred_controller = player;
        for(i=0;i<2;i++){
            select_target(player, card, &td, "Choose a card to put on deck", NULL);
            card_instance_t *instance = get_card_instance( player, card);
            put_on_top_of_deck(player, instance->targets[0].card);
        }
        kill_card(player, card, KILL_DESTROY);
    }
    return 0;
}

int card_repulse(int player, int card, event_t event)
{
  /* CARD_ID_REPULSE	1352
   * Repulse	|2|U
   * Instant
   * Return target creature to its owner's hand.
   * Draw a card. */

  int result =  card_unsummon(player, card, event);
  if(event == EVENT_RESOLVE_SPELL){
    if( spell_fizzled != 1 ){
        draw_a_card(player);
    }
  }
  return result;
}

int card_dissipate(int player, int card, event_t event){
  /* CARD_ID_DISSIPATE	2452
   * Dissipate	|1|U|U
   * Instant
   * Counter target spell. If that spell is countered this way, exile it instead of putting it into its owner's graveyard. */

    int result =  card_memory_lapse(player, card, event);
    if(event == EVENT_RESOLVE_SPELL && spell_fizzled != 1 ){
        card_instance_t *instance = get_card_instance(player, card);
        rfg_top_card_of_deck(instance->targets[0].player);
    }
    return result;
}

int default_tutor_1(int card){
    return 1;
}

int card_mystical_tutor(int player, int card, event_t event){
  /* CARD_ID_MYSTICAL_TUTOR	1527
   * Mystical Tutor	|U
   * Instant
   * Search your library for an instant or sorcery card and reveal that card. Shuffle your library, then put the card on top of it. */

     //return type_tutor(player, card, event, TYPE_INSTANT | TYPE_INTERRUPT, TYPE_SORCERY, 1, 0);
    if( event == EVENT_CAN_CAST ){
        return 1;
    }
    else if( event == EVENT_RESOLVE_SPELL ){
			int my_type = TYPE_INSTANT | TYPE_SORCERY | TYPE_INTERRUPT;
			global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_DECK, 0, 1, my_type, 2, 0, 0, 0, 0, 0, 0, -1, 0);		
			kill_card(player, card, KILL_DESTROY);
    }
    return 0;
}

int effect_mana_drain(int player, int card, event_t event){
    card_instance_t *instance = get_card_instance(player, card);
    if( current_turn == player && current_phase == instance->targets[0].player ){
		if( instance->targets[1].card == CARD_ID_MANA_DRAIN ){
			produce_mana(player, COLOR_COLORLESS, instance->targets[0].card);
		}
		else if( instance->targets[1].card == CARD_ID_CHANCELLOR_OF_THE_TANGLE ){
				produce_mana(player, COLOR_GREEN, 1);
		}
        instance->targets[0].player = 0;
        kill_card(player, card, KILL_REMOVE);
    }
    return 0;
}

int card_mana_drain(int player, int card, event_t event){
  /* Mana Drain	|U|U
   * Instant
   * Counter target spell. At the beginning of your next main phase, add an amount of |C equal to that spell's mana value. */

    card_instance_t *instance = get_card_instance(player, card);
	
	if( event == EVENT_CAN_CAST && card_on_stack_controller != -1 && card_on_stack != -1){
		if( ! check_state(card_on_stack_controller, card_on_stack, STATE_CANNOT_TARGET) ){
			return 0x63;
		}
    }
	
    if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( spell_fizzled != 1 ){
			instance->info_slot = get_cmc(card_on_stack_controller, card_on_stack);
			if( is_x_spell(card_on_stack_controller, card_on_stack) ){
				card_instance_t *spell = get_card_instance(card_on_stack_controller, card_on_stack);
				instance->info_slot+=spell->info_slot;
			}
			instance->targets[0].player = card_on_stack_controller;
			instance->targets[0].card = card_on_stack;
		}
	}

    if( event == EVENT_RESOLVE_SPELL ){

		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);

        int legacy_card = create_legacy_effect(player, card, &effect_mana_drain );
        card_instance_t *legacy = get_card_instance(player, legacy_card);
        legacy->targets[0].card = instance->info_slot;
        if( current_turn == player && current_phase < PHASE_MAIN2 ){
            legacy->targets[0].player = PHASE_MAIN2;
        }
        else{
            legacy->targets[0].player = PHASE_MAIN1;
        }
		legacy->targets[1].card = get_id(player, card);

		kill_card(player, card, KILL_DESTROY);
    }
	return 0;
}

