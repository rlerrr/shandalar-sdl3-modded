#include "manalink.h"

int card_zombie_master2(int player, int card, event_t event){
  /* Zombie Master	|1|B|B
   * Creature - Zombie 2/3
   * Other Zombie creatures have swampwalk. (They can't be blocked as long as defending player controls a Swamp.)
   * Other Zombies have "|B: Regenerate this permanent." */

    // do the swampwalk bit
    if( event == EVENT_ABILITIES ){
        if( has_creature_type(affected_card_controller, affected_card, SUBTYPE_ZOMBIE) && ! affect_me(player, card) ){
            event_result |= get_hacked_walk(player, card, KEYWORD_SWAMPWALK);
        }
    }

    // do the regeneration bit
    card_instance_t *instance = get_card_instance(player, card);
    if( instance->info_slot == 1 ){
        return 0;
    }
    instance->info_slot = 1;
    if( ! get_forbid_activation_flag(player, card) && is_creature_dead() && ( land_can_be_played & 0x200) ){
        int p=0;
        for(p=0;p<2;p++){
            int c=0;
            for(c=0;c< active_cards_count[p];c++){
                card_data_t* card_d = get_card_data(p, c);
                if( !(p==player && c==card) && (card_d->type & TYPE_CREATURE) && in_play(p, c) && has_creature_type(p, c, SUBTYPE_ZOMBIE) ){
                    card_instance_t *creature = get_card_instance(p, c);
                    if( creature->kill_code == KILL_DESTROY && has_mana(p, COLOR_BLACK, 1) ){
                        int choice = do_dialog(p, player, card, p, c, " Regenerate for B\n Do not regenerate", 0);
                        if( choice == 0 ){
                            charge_mana(p, COLOR_BLACK, 1);
                            if( spell_fizzled != 1 ){
                                regenerate_target(p, c);
                            }
                        }
                    }
                }
            }
        }
    }
    instance->info_slot = 0;
    return 0;
}

int card_spinneret_sliver(int player, int card, event_t event){
  /* CARD_ID_SPINNERET_SLIVER	1387
   * Spinneret Sliver	|1|G
   * Creature - Sliver 2/2
   * All Sliver creatures have reach. (They can block creatures with flying.) */

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 0, 0, KEYWORD_REACH, BCT_INCLUDE_SELF);
	return slivercycling(player, card, event);
}

int card_triskelion2(int player, int card, event_t event){
  /* Triskelion	|6
   * Artifact Creature - Construct 1/1
   * This creature enters with three +1/+1 counters on it.
   * Remove a +1/+1 counter from this creature: It deals 1 damage to any target. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		add_1_1_counters(player, card, player, card, 3);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_creature_or_player(player, card, event, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_1_1_COUNTER+GAA_CAN_TARGET, 0, 0, 0, 0, 0, 0, 0, &td, "TARGET_CREATURE_OR_PLAYER");
}

int card_lord_of_atlantis2(int player, int card, event_t event){
  /* Lord of Atlantis	|U|U
   * Creature - Merfolk 2/2
   * Other Merfolk get +1/+1 and have islandwalk. (They can't be blocked as long as defending player controls an Island.) */

  if( event == EVENT_ABILITIES && in_play(affected_card_controller, affected_card) ){
     if( affect_me(player, card) ){ return 0; }
     if( has_subtype(affected_card_controller, affected_card, SUBTYPE_MERFOLK) ){
        event_result |= get_hacked_walk(player, card, KEYWORD_ISLANDWALK);
     }
  }

  if( event == EVENT_POWER || event == EVENT_TOUGHNESS ){
     if( affect_me(player, card) ){ return 0; }
     if( has_subtype(affected_card_controller, affected_card, SUBTYPE_MERFOLK) ){
        event_result++;
     }
  }

  return 0;
}

int card_bribery(int player, int card, event_t event){
  /* CARD_ID_BRIBERY	985
   * Bribery	|3|U|U
   * Sorcery
   * Search target opponent's library for a creature card and put that card onto the battlefield under your control. Then that player shuffles his or her library. */

// original code : 0040F730
	return steal_permanent_from_target_opponent_deck(player, card, event, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0);
}

int card_kid_ape2(int player, int card, event_t event){
  /* Kird Ape	|R
   * Creature - Ape 1/1
   * This creature gets +1/+2 as long as you control a Forest. */

// original code : 00453E50

  if( event == EVENT_POWER && affect_me(player, card) && check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_FOREST) ){
      event_result++;
  }

  if( event == EVENT_TOUGHNESS && affect_me(player, card) &&
      check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_FOREST)
    ){
      event_result+=2;
  }

  return 0;
}

int card_tetravus2(int player, int card, event_t event){
  /* Tetravus	|6
   * Artifact Creature - Construct 1/1
   * Flying
   * This creature enters with three +1/+1 counters on it.
   * At the beginning of your upkeep, you may remove any number of +1/+1 counters from this creature. If you do, create that many 1/1 colorless Tetravite artifact creature tokens. They each have flying and "This token can't be enchanted."
   * At the beginning of your upkeep, you may exile any number of tokens created with this creature. If you do, put that many +1/+1 counters on this creature. */

//  original code : 00421590

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		add_1_1_counters(player, card, player, card, 3);
		int ids = internal_rand(100);
		instance->targets[8].player = ids;
	}

	upkeep_trigger_ability(player, card, event, player);	

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int tetravites = 0;
		int i;
		int choice;

		if( count_1_1_counters(player, card) > 0 ){
			choice = do_dialog(player, player, card, -1, -1,
                             " Release some Tetravites (Auto)\n Release some Tetravites (Manual)\n Pass", 0);
			int number = 0;

			if( choice == 0 ){
				number = count_1_1_counters(player, card);
			}

			if( choice == 1 ){
				number = choose_a_number(player, "Remove how many counters?", count_1_1_counters(player, card));
				if( number > count_1_1_counters(player, card) ){
					number = count_1_1_counters(player, card);
				}
			}

			if( number > 0 ){
				for(i=0; i<number;i++){
					remove_1_1_counter(player, card);
				}
				token_generation_t token;
				default_token_definition(player, card, CARD_ID_TETRAVITE, &token);
				token.qty = number;
				token.special_infos = instance->targets[8].player;
				generate_token(&token);
			}
		}

		for(i =0; i<2; i++){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && get_id(i, count) == CARD_ID_TETRAVITE ){
						if( instance->targets[8].player == get_special_infos(i, count) ){
							tetravites++;
						}
					}
					count++;
			}
		}

		if( tetravites > 0 ){
			int ai_choice = 2;
			if( check_battlefield_for_id(player, CARD_ID_DOUBLING_SEASON) ||
				check_battlefield_for_id(player, CARD_ID_PARALLEL_LIVES) ){
				ai_choice = 0;
			}

			choice = do_dialog(player, player, card, -1, -1, " Dock some Tetravites (Auto)\n Dock some Tetravites (Manual)\n Pass", ai_choice);
			if( choice == 0 ){
				i=0;
				for(i =0; i<2; i++){
					int count = 0;
					while( count < active_cards_count[i] ){
							if( in_play(i, count) && get_id(i, count) == CARD_ID_TETRAVITE ){
								if( instance->targets[8].player == get_special_infos(i, count) ){
									kill_card(i, count, KILL_SACRIFICE);
									add_1_1_counter(player, card, player, card);
								}
							}
							count++;
					}
				}
			}

			if( choice == 1 ){
				while( tetravites > 0 ){
						if( pick_target(&td, "TETRAVITE") ){
							if( get_id(instance->targets[0].player, instance->targets[0].card) == get_id_by_name("Tetravite") ){
								if( instance->targets[8].player == get_special_infos(instance->targets[0].player,instance->targets[0].card) ){
									kill_card(instance->targets[0].player,instance->targets[0].card, KILL_SACRIFICE);
									add_1_1_counter(player, card, player, card);
									tetravites--;
								}	

							}
						}
						else{
							break;
						}
				}
			}

		}	
	}

	return 0;
}

int card_slith_predator(int player, int card, event_t event){
  /* CARD_ID_SLITH_PREDATOR	1475
   * Slith Predator	|G|G
   * Creature - Slith 1/1
   * Trample
   * Whenever ~ deals combat damage to a player, put a +1/+1 counter on it. */

    if( has_combat_damage_been_inflicted_to_a_player(player, card, event) ){
        add_1_1_counter(player, card, player, card);
    }

    return 0;
}

int card_slith_firewalker(int player, int card, event_t event){
  /* CARD_ID_SLITH_FIREWALKER	1476
   * Slith Firewalker	|R|R
   * Creature - Slith 1/1
   * Haste
   * Whenever ~ deals combat damage to a player, put a +1/+1 counter on it. */

    haste(player, card);

    return card_slith_predator(player, card, event);
}

int card_slith_bloodletter(int player, int card, event_t event){
  /* CARD_ID_SLITH_BLOODLETTER	1477
   * Slith Bloodletter	|B|B
   * Creature - Slith 1/1
   * Whenever ~ deals combat damage to a player, put a +1/+1 counter on it.
   * |1|B: Regenerate ~. */

    if( has_combat_damage_been_inflicted_to_a_player(player, card, event) ){
        add_1_1_counter(player, card, player, card);
    }

    return regeneration(player, card, event, 1, 1, 0, 0, 0, 0);
}

int card_juggernaut(int player, int card, event_t event){
  /* CARD_ID_JUGGERNAUT	132
   * Juggernaut	|4
   * Artifact Creature - Juggernaut 5/3
   * ~ attacks each turn if able.
   * ~ can't be blocked by Walls. */

// original code : 00453C40

    attack_if_able(player, card, event);

	if(event == EVENT_BLOCK_LEGALITY ){
		if( player == attacking_card_controller && card == attacking_card ){
			if( has_subtype(affected_card_controller, affected_card, SUBTYPE_WALL) ){
				event_result = 1;
			}    
		}      
	}

   return 0;
}

int card_carven_caryatid(int player, int card, event_t event){
  /* CARD_ID_CARVEN_CARYATID	1251
   * Carven Caryatid	|1|G|G
   * Creature - Spirit 2/5
   * Defender (This creature can't attack.)
   * When ~ enters the battlefield, draw a card. */

// original code : 0040E990

   if( event == EVENT_RESOLVE_SPELL ){
       draw_cards(player, 1);
   }

   cannot_attack(player, card, event);

   return 0;
}

int card_swords_to_plowshares2(int player, int card, event_t event){
  /* Swords to Plowshares	|W
   * Instant
   * Exile target creature. Its controller gains life equal to its power. */

	// original code : 004A3750
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);

    card_instance_t *instance = get_card_instance( player, card);

    if(event == EVENT_CAN_CAST ){
        return can_target(&td);
    }
    else if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
            pick_target(&td, "TARGET_CREATURE");
    }
    else if( event == EVENT_RESOLVE_SPELL ){
             if( valid_target(&td) ){
				gain_life(instance->targets[0].player, get_power(instance->targets[0].player, instance->targets[0].card));
				rfg_target_permanent(instance->targets[0].player, instance->targets[0].card);
             }
             kill_card(player, card, KILL_DESTROY);
    }
    return 0;
}

int card_fog2(int player, int card, event_t event){
  /* Fog	|G
   * Instant
   * Prevent all combat damage that would be dealt this turn. */

// original code : 004A3700
// also code for Holy Day and Darkness

    if( event == EVENT_RESOLVE_SPELL ){
			 fog_effect(player, card);
			 kill_card(player, card, KILL_DESTROY);
    }
	else{
		 return card_fog(player, card, event);
	}

    return 0;
}

int card_lady_evangela(int player, int card, event_t event){
  /* Lady Evangela	|W|U|B
   * Legendary Creature - Human Cleric 1/2
   * |W|B, |T: Prevent all combat damage that would be dealt by target creature this turn. */

// original code : 0040CD80

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = 1-player;
	td.preferred_controller = 1-player;
	
	card_instance_t *instance = get_card_instance(player, card);

	check_legend_rule(player, card, event);
	
    if( event == EVENT_CAN_ACTIVATE && ! get_forbid_activation_flag(player, card) && has_mana_for_activated_ability(player, card, 0, 1, 0, 0, 0, 1) ){
		if( ! is_tapped(player, card) && ! is_sick(player, card) ){
			return can_target(&td);
		}
    }

    else if(event == EVENT_ACTIVATE ){
            charge_mana_for_activated_ability(player, card, 0, 1, 0, 0, 0, 1);
			if( spell_fizzled != 1 && pick_target(&td, "TARGET_CREATURE") ){
				tap_card(player, card);
			}
	}

    else if( event == EVENT_RESOLVE_ACTIVATION ){
			 negate_combat_damage_this_turn(player, instance->parent_card, 
											instance->targets[0].player, instance->targets[0].card, 0);
    }

    return 0;
}

int card_angus_mackenzie(int player, int card, event_t event){
  /* Angus Mackenzie	|G|W|U
   * Legendary Creature - Human Cleric 2/2
   * |G|W|U, |T: Prevent all combat damage that would be dealt this turn. Activate this ability only before the combat damage step. */

// original code : 00406250

	check_legend_rule(player, card, event);
	
	card_instance_t *instance = get_card_instance(player, card);

    if( event == EVENT_CAN_ACTIVATE && ! get_forbid_activation_flag(player, card) && has_mana_for_activated_ability(player, card, 0, 0, 1, 1, 0, 1) ){
		if( ! is_tapped(player, card) && ! is_sick(player, card) && current_phase < PHASE_DECLARE_ATTACKERS ){
			return 1;
		}
    }

    else if(event == EVENT_ACTIVATE ){
            charge_mana_for_activated_ability(player, card, 0, 0, 1, 1, 0, 1);
			if( spell_fizzled != 1 ){
				tap_card(player, card);
			}
	}

    else if( event == EVENT_RESOLVE_ACTIVATION ){
			 fog_effect(player, instance->parent_card);
    }

    return 0;
}

int card_horn_of_deafening(int player, int card, event_t event){
  /* CARD_ID_HORN_OF_DEAFENING	673
   * Horn of Deafening	|4
   * Artifact
   * |2, |T: Prevent all combat damage that would be dealt by target creature this turn. */

// original code : 00405F70

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
	
	card_instance_t *instance = get_card_instance(player, card);

    if( event == EVENT_RESOLVE_ACTIVATION ){
		negate_combat_damage_this_turn(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 0);
    }

	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK+GAA_CAN_TARGET, 2, 0, 0, 0, 0, 0, 0, &td, "TARGET_CREATURE");
}

int maze_of_ith_legacy(int player, int card, event_t event){
    card_instance_t *instance = get_card_instance( player, card );

    if( event == EVENT_PREVENT_DAMAGE && 
        (current_phase == PHASE_FIRST_STRIKE_DAMAGE || current_phase == PHASE_NORMAL_COMBAT_DAMAGE) 
	  ){
        card_instance_t *source = get_card_instance(affected_card_controller, affected_card);

        if( damage_card != source->internal_card_id || source->info_slot <= 0 ){
            return 0;
        }

		if( source->damage_source_player == instance->targets[0].player && 
            source->damage_source_card == instance->targets[0].card
		  ){
             source->info_slot = 0;
        }

		if( source->damage_target_player == instance->targets[0].player && 
            source->damage_target_card == instance->targets[0].card
		  ){
             source->info_slot = 0;
        }

	}

    if( eot_trigger(player, card, event) ){
		card_instance_t *trg = get_card_instance( instance->targets[0].player, instance->targets[0].card );
		if( trg->targets[14].card == 66  ){
			trg->targets[14].card = -1;
		}
        kill_card(player, card, KILL_REMOVE);
    }

    return 0;
}

int maze_of_ith_effect(int player, int card, int t_player, int t_card){
	card_instance_t *trg = get_card_instance(t_player, t_card);
	trg->targets[14].card = 66;
	int legacy = create_targetted_legacy_effect( player, card, &maze_of_ith_legacy, t_player, t_card);
	return legacy;
}

int card_maze_of_ith2(int player, int card, event_t event){
  /* Maze of Ith	""
   * Land
   * |T: Untap target attacking creature. Prevent all combat damage that would be dealt to and dealt by that creature this turn. */

// original code : 00406100

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;
	
	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

    if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier+=100;
    }

    if( event == EVENT_RESOLVE_ACTIVATION ){
		if(valid_target(&td) ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
			maze_of_ith_effect(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
    }

	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK+GAA_CAN_TARGET, 0, 0, 0, 0, 0, 0, 0, &td, "TARGET_CREATURE");
}

int card_nuisance_engine(int player, int card, event_t event){
  /* Nuisance Engine	|3
   * Artifact
   * |2, |T: Put a 0/1 colorless Pest artifact creature token onto the battlefield. */

// original code : 004DAC3A

    if( event == EVENT_RESOLVE_ACTIVATION ){
        generate_token_by_id(player, CARD_ID_PEST);
    }

	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK, 2, 0, 0, 0, 0, 0, 0, 0, 0);
}

int clone_legacy(int player, int card, event_t event){

    card_instance_t *instance = get_card_instance( player, card );

	if(	trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me( player, card ) && reason_for_trigger_controller == affected_card_controller ){
		if( trigger_cause_controller == instance->targets[0].player && trigger_cause == instance->targets[0].card ){
			if(event == EVENT_TRIGGER){
				event_result |= 2;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					card_instance_t *orig = get_card_instance( instance->targets[0].player, instance->targets[0].card );
					orig->targets[10].player = 66;
					if( instance->targets[1].card == CARD_ID_EVIL_TWIN ){
						int effect = generate_token_by_id(player, CARD_ID_EVIL_TWIN_EFFECT);
						card_instance_t *twin = get_card_instance(player, effect);
						twin->targets[9].player = player;
						twin->targets[9].card = card;
						twin->targets[8].card = get_id(instance->targets[0].player, instance->targets[0].card);
					}
			}
		}
    }
	
	return 0;
}

int card_clone_fixed(int player, int card, event_t event ){
  /* Clone	|3|U
   * Creature - Shapeshifter 0/0
   * You may have this creature enter as a copy of any creature on the battlefield. */

// original code :  020027DF 

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.illegal_abilities = 0;
	
    card_instance_t *instance = get_card_instance(player, card);

    if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
        if( pick_target(&td, "TARGET_CREATURE") ){
			cloning(player, card, instance->targets[0].player, instance->targets[0].card);
			int legacy = create_targetted_legacy_effect(player, card, &clone_legacy, player, card);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[1].card = get_id(player, card);
		}
    }
	
	cloning_card(player, card, event);

    return 0;
}

int card_fire_elemental(int player, int card, event_t event ){
  /* Fire Elemental	|3|R|R
   * Creature - Elemental 5/4 */

// original code : 00401000

	if( get_special_infos(player, card) > 0 ){
		int amount = get_special_infos(player, card);
		if( event == EVENT_POWER && affect_me(player, card)  ){
			event_result+=(amount-5);
		}
		if( event == EVENT_TOUGHNESS && affect_me(player, card)  ){
			event_result+=(amount-4);
		}
	}

    return 0;
}

int card_fork_fixed(int player, int card, event_t event){
  /* Fork	|R|R
   * Instant
   * Copy target instant or sorcery spell, except that the copy is red. You may choose new targets for the copy. */

// original code : 

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[2].player != -1 && instance->targets[2].card != -1 ){
		int p = instance->targets[2].player;
		int c = instance->targets[2].card;
		if( event == EVENT_SET_COLOR && affect_me(p, c) ){
			event_result = COLOR_TEST_RED;
		}
	}

    if( event == EVENT_CAN_CAST ){
		int result = card_counterspell(player, card, event);
		if( result > 0 ){
			if( ! is_what(card_on_stack_controller, card_on_stack, TYPE_PERMANENT) ){
				instance->targets[1].player = card_on_stack_controller;
				instance->targets[1].card = card_on_stack;
				return result;
			}
		}
		else{
			 return 0;
		}
    }

    else if( event == EVENT_RESOLVE_SPELL ){
			 int id = get_id(instance->targets[1].player, instance->targets[1].card);
			 instance->targets[2].player = player;
			 instance->targets[2].card = copy_spell(player, id);
			 kill_card(player, card, KILL_DESTROY);
    }
    else{
		 return card_counterspell(player, card, event);
    }

    return 0;
}

int card_rock_hydra(int player, int card, event_t event){
  /* Rock Hydra	|X|R|R
   * Creature - Hydra 0/0
   * This creature enters with X +1/+1 counters on it.
   * For each 1 damage that would be dealt to this creature, if it has a +1/+1 counter on it, remove a +1/+1 counter from it and prevent that 1 damage.
   * |R: Prevent the next 1 damage that would be dealt to this creature this turn.
   * |R|R|R: Put a +1/+1 counter on this creature. Activate only during your upkeep. */

// original code : 00454ED0

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->info_slot = x_value;
	}
	else if( event == EVENT_RESOLVE_SPELL ){
			 add_1_1_counters(player, card, player, card, instance->info_slot);
 			 instance->info_slot = 0;
	}
	else{
		return card_rock_hydra_exe(player, card, event);
	}
	return 0;
}

int card_coastal_piracy(int player, int card, event_t event){
  /* Coastal Piracy	|2|U|U
   * Enchantment
   * Whenever a creature you control deals combat damage to an opponent, you may draw a card. */

// original code : 00405650
    card_instance_t *instance = get_card_instance(player, card);

    if( current_turn == player && event == EVENT_DEAL_DAMAGE &&
		( current_phase == PHASE_FIRST_STRIKE_DAMAGE || current_phase == PHASE_NORMAL_COMBAT_DAMAGE ) 
	  ){
        card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
        if( damage->internal_card_id == damage_card ){
            if( damage->damage_source_player == player && damage->damage_target_card == -1 ){
				if( is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE) ){
					card_instance_t *target = get_card_instance(damage->damage_source_player, damage->damage_source_card);
					if( damage->info_slot > 0 || target->targets[16].player > 0 ){
						instance->info_slot++;
					}
				}
            }
        }
    }
    
    if(	trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && 
		reason_for_trigger_controller == player && instance->info_slot > 0 ){
        if(event == EVENT_TRIGGER){
            event_result |= 2;
        }
        else if(event == EVENT_RESOLVE_TRIGGER){
				int i;
				for(i=0; i<instance->info_slot; i++){
					draw_some_cards_if_you_want(player, card, player, 1);
				}
				instance->info_slot = 0;
        }
    }

    return global_enchantment(player, card, event);
}

int card_craw_wurm(int player, int card, event_t event ){
  /* Craw Wurm	|4|G|G
   * Creature - Wurm 6/4 */

// original code : 00401000

	if( get_special_infos(player, card) == 66 ){
		modify_pt_and_abilities(player, card, event, 0, 2, 0);
	}

    return 0;
}

int card_pentavite(int player, int card, event_t event ){
  /* Pentavite	""
   * Token Artifact Creature - Pentavite 1/1
   * Flying */

// original code as Tetravite : 00421B70;

	if( get_special_infos(player, card) > 0 ){
		if(	trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && 
			player == reason_for_trigger_controller 
		  ){
			int trig = 0;

			if( is_what(trigger_cause_controller, trigger_cause, TYPE_ENCHANTMENT) &&
				is_aura(trigger_cause_controller, trigger_cause, 1) 
			  ){
				card_instance_t *instance = get_card_instance(player, card);
				if( instance->targets[0].player == player && instance->targets[0].card == card ){
					trig = 1;
				}
			}

			if( trig > 0 ){
				if(event == EVENT_TRIGGER){
					event_result |= 2;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						kill_card(trigger_cause_controller, trigger_cause, KILL_SACRIFICE);
				}
			}
		}
	}

    return 0;
}

int card_giant_spider(int player, int card, event_t event ){
  /* Giant Spider	|3|G
   * Creature - Spider 2/4
   * Reach (This creature can block creatures with flying.) */

// original code : 004C7D10

	if( get_special_infos(player, card) == 66 ){
		modify_pt_and_abilities(player, card, event, -1, -2, 0);
	}

    return card_giant_spider_exe(player, card, event);
}

int card_time_walk(int player, int card, event_t event ){
  /* CARD_ID_TIME_WALK	249
   * Time Walk	|1|U
   * Sorcery
   * Take an extra turn after this one. */

// original code : 0041B710

	if( event == EVENT_RESOLVE_SPELL ){
		if( ! check_battlefield_for_id(1-player, CARD_ID_STRANGLEHOLD) ){
			return card_time_walk_exe(player, card, event);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	else{
		return card_time_walk_exe(player, card, event);
	}
	return 0;
}

int card_demonic_tutor(int player, int card, event_t event ){
  /* CARD_ID_DEMONIC_TUTOR	62
   * Demonic Tutor	|1|B
   * Sorcery
   * Search your library for a card and put that card into your hand. Then shuffle your library. */

// original code : 0041D7C0

	if( event == EVENT_RESOLVE_SPELL ){
		if( ! check_battlefield_for_id(1-player, CARD_ID_STRANGLEHOLD) ){
			return card_demonic_tutor_exe(player, card, event);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	else{
		return card_demonic_tutor_exe(player, card, event);
	}
	return 0;
}

int card_grey_ogre(int player, int card, event_t event ){
  /* Gray Ogre	|2|R
   * Creature - Ogre 2/2 */

// original code : 00401000

	if( get_special_infos(player, card) == 66 ){
		modify_pt_and_abilities(player, card, event, 1, 1, 0);
	}

    return 0;
}

int card_solemn_simulacrum(int player, int card, event_t event){
  /* CARD_ID_SOLEMN_SIMULACRUM	1207
   * Solemn Simulacrum	|4
   * Artifact Creature - Golem 2/2
   * When ~ enters the battlefield, you may search your library for a basic land card, put that card onto the battlefield tapped, then shuffle your library.
   * When ~ dies, you may draw a card. */

//    card_instance_t *instance = get_card_instance(player, card);

    if( event == EVENT_RESOLVE_SPELL ){
        int legacy = create_legacy_effect(player, card, &solemn_simulacrum_legacy);
        card_instance_t *leg = get_card_instance(player, legacy);
        leg->targets[0].card = card;
        leg->targets[0].player = player;
		if(	check_for_cip_effects_removal(player, card) > 0 ){
			tutor_basic_land(player, 1, 1);
		}
    }

    return 0;
}

int card_raise_the_alarm(int player, int card, event_t event){
  /* CARD_ID_RAISE_THE_ALARM	1450
   * Raise the Alarm	|1|W
   * Instant
   * Create two 1/1 |Swhite Soldier creature tokens. */

// original code : 0x4DED1D

	if( event == EVENT_CAN_CAST ){
		return 1;
	}
	
	else if( event == EVENT_RESOLVE_SPELL ){
			generate_tokens_by_id(player, CARD_ID_SOLDIER, 2);
			kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_battering_ram(int player, int card, event_t event){
  /* CARD_ID_BATTERING_RAM	478
   * Battering Ram	|2
   * Artifact Creature - Construct 1/1
   * At the beginning of combat on your turn, ~ gains banding until end of combat. (Any creatures with banding, and up to one without, can attack in a band. Bands are blocked as a group. If any creatures with banding you control are being blocked by a creature, you divide that creature's combat damage, not its controller, among any of the creatures it's blocking.)
   * Whenever ~ becomes blocked by a Wall, destroy that Wall at end of combat. */

// original code : 0x422CC0

	if( event == EVENT_ABILITIES && affect_me(player, card) ){
		if( current_turn == player && current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2 ){
			event_result |= KEYWORD_BANDING;
		}
	}

	return card_battering_ram_exe(player, card, event);
}

int card_voice_of_all(int player, int card, event_t event){
  /* CARD_ID_VOICE_OF_ALL	1620
   * Voice of All	|2|W|W
   * Creature - Angel 2/2
   * Flying
   * As ~ enters the battlefield, choose a color.
   * ~ has protection from the chosen color. */

// original code :  

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[8].player != -1 ){
		modify_pt_and_abilities(player, card, event, 0, 0, instance->targets[8].player);
	}

	if( comes_into_play(player, card, event) ){
		instance->targets[8].player = select_a_protection(player);
	}

	return 0;
}

int card_larceny(int player, int card, event_t event){
  /* Larceny	|3|B|B
   * Enchantment
   * Whenever a creature you control deals combat damage to a player, that player discards a card. */

// original code : 0x40DC50
    card_instance_t *instance = get_card_instance(player, card);

    if( current_turn == player && event == EVENT_DEAL_DAMAGE &&
		( current_phase == PHASE_FIRST_STRIKE_DAMAGE || current_phase == PHASE_NORMAL_COMBAT_DAMAGE ) 
	  ){
        card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
        if( damage->internal_card_id == damage_card ){
            if( damage->damage_source_player == player && damage->damage_target_card == -1 ){
				if( is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE) ){
					card_instance_t *target = get_card_instance(damage->damage_source_player, damage->damage_source_card);
					if( damage->info_slot > 0 || target->targets[16].player > 0 ){
						instance->info_slot++;
					}
				}
            }
        }
    }
    
    if(	trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && 
		reason_for_trigger_controller == player && instance->info_slot > 0 ){
        if(event == EVENT_TRIGGER){
            event_result |= 2;
        }
        else if(event == EVENT_RESOLVE_TRIGGER){
				int i;
				for(i=0; i<instance->info_slot; i++){
					discard(1-player, 0, 0);
				}
				instance->info_slot = 0;
        }
    }

    return global_enchantment(player, card, event);
}

int card_noble_purpose(int player, int card, event_t event){
  /* Noble Purpose	|3|W|W
   * Enchantment
   * Whenever a creature you control deals combat damage, you gain that much life. */

// original code : 0x403D20

	if (event == EVENT_DEAL_DAMAGE){
		card_instance_t* source = get_card_instance(affected_card_controller, affected_card);

		if (source->internal_card_id == damage_card && source->info_slot > 0
			&& source->damage_source_player == player && source->damage_source_card >= 0
			&& (source->token_status & (STATUS_FIRST_STRIKE_DAMAGE | STATUS_COMBAT_DAMAGE))
			&& is_what(source->damage_source_player, source->damage_source_card, TYPE_CREATURE)){
			gain_life(player, source->info_slot);
		}
	}

    return global_enchantment(player, card, event);
}

int card_ashnods_trasmogrant(int player, int card, event_t event){
  /* Ashnod's Transmogrant	|1
   * Artifact
   * |T, Sacrifice this artifact: Put a +1/+1 counter on target nonartifact creature. That creature becomes an artifact in addition to its other types. */

// original code : 0x044E240

    card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_ashnods_trasmogrant_exe(player, card, event);
		card_instance_t *target = get_card_instance( instance->targets[0].player, instance->targets[0].card );
		target->counters_byte--;
		target->counter_power--;
		target->counter_toughness--;
		add_1_1_counter(instance->targets[0].player, instance->targets[0].card, instance->targets[0].player, instance->targets[0].card);
	}
	else{
		return card_ashnods_trasmogrant_exe(player, card, event);
	}
	return 0;
}

int card_powerleech(int player, int card, event_t event){
  /* CARD_ID_POWERLEECH	518
   * Powerleech	|G|G
   * Enchantment
   * Whenever an artifact an opponent controls becomes tapped or an opponent activates an artifact's ability without |T in its activation cost,
   * you gain 1 life. */

// original code : 0x4327D0

//    card_instance_t *instance = get_card_instance( player, card );
	
	if( event == EVENT_PLAY_ABILITY && affected_card_controller == 1-player &&
		is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) && 
		! is_tapped(affected_card_controller, affected_card)
	  ){
		gain_life(player, 1);
	}
	if( event == EVENT_TAP_CARD && affected_card_controller == 1-player &&
		is_what(affected_card_controller, affected_card, TYPE_ARTIFACT)
	  ){
		gain_life(player, 1);
	}

	return global_enchantment(player, card, event);
}

int card_phyrexian_gremlins(int player, int card, event_t event){
  /* Phyrexian Gremlins	|2|B
   * Creature - Phyrexian Gremlin 1/1
   * You may choose not to untap this creature during your untap step.
   * |T: Tap target artifact. It doesn't untap during its controller's untap step for as long as this creature remains tapped. */

// original code : 0x4549D0

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

    card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK+GAA_CAN_TARGET, 0, 0, 0, 0, 0, 0, 0, &td, "TARGET_ARTIFACT");
    }

    else if(event == EVENT_ACTIVATE ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK+GAA_CAN_TARGET, 0, 0, 0, 0, 0, 0, 0, &td, "TARGET_ARTIFACT");
    }

    else if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				does_not_untap_until_im_tapped(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
			}
    }

    else{
		return card_tawnoss_weaponry(player, card, event);
    }

	return 0;
}

int card_karakas(int player, int card, event_t event){
  /* Karakas	""
   * Legendary Land
   * |T: Add |W to your mana pool.
   * |T: Return target legendary creature to its owner's hand. */

// original code : 0x40D220

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_LEGEND;

    card_instance_t *instance = get_card_instance(player, card);

	check_legend_rule(player, card, event);

    if( event == EVENT_ACTIVATE ){
        int choice = 0;
        instance->info_slot = 0;
        if( ! paying_mana() && ! get_forbid_activation_flag(player, card) &&  can_target(&td) && has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0)
		  ){
            choice = do_dialog(player, player, card, -1, -1, " Produce W\n Bounce a Legend\n Do nothing", 1);
        }
        if( choice == 0 ){
			return mana_producer(player, card, event);
        }
        if( choice == 1 ){
            if( charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) && pick_target(&td, "TARGET_CREATURE") ){
				if( is_legendary(instance->targets[0].player, instance->targets[0].card) ){
					tap_card(player, card);
					instance->info_slot = 1;
				}
				else{
					spell_fizzled = 1;
				}
			}
        }
        else{
            spell_fizzled = 1;
        }
    }
    else if( event == EVENT_RESOLVE_ACTIVATION ){
             if( instance->info_slot == 1 ){
				card_instance_t *parent = get_card_instance(player, instance->parent_card);
				if( valid_target(&td)  ){
					bounce_permanent(instance->targets[0].player, instance->targets[0].card);
				}
				parent->info_slot = 0;
			}
    }

    else{
		return mana_producer(player, card, event);
    }

	return 0;
}

int card_martyrs_of_korlis(int player, int card, event_t event ){
  /* CARD_ID_MARTYRS_OF_KORLIS	505
   * Martyrs of Korlis	|3|W|W
   * Creature - Human 1/6
   * As long as ~ is untapped, all damage that would be dealt to you by artifacts is dealt to ~ instead. */

// original code : 0x453EB0

	if( event == EVENT_PREVENT_DAMAGE && ! is_tapped(player, card) ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == -1 && damage->damage_target_player == player &&  
                damage->info_slot > 0 
              ){
				if( in_play(damage->damage_source_player, damage->damage_source_card) &&
					is_what(damage->damage_source_player, damage->damage_source_card, TYPE_ARTIFACT)
				  ){
					damage->damage_target_card = card;
				}
            }
        }
    }
	
	return 0;
}

int card_willow_satyr(int player, int card, event_t event){
  /* Willow Satyr	|2|G|G
   * Creature - Satyr 1/1
   * You may choose not to untap ~ during your untap step.
   * |T: Gain control of target legendary creature for as long as you control ~ and ~ remains tapped. */

// original code : 

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = 1-player;
	td.required_subtype = SUBTYPE_LEGEND;

    card_instance_t *instance = get_card_instance(player, card);

	choose_to_untap(player, card, event);

    if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && in_play(instance->parent_controller, instance->parent_card) && is_tapped(instance->parent_controller, instance->parent_card)){
			gain_control_until_source_is_in_play_and_tapped(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK+GAA_CAN_TARGET, 0, 0, 0, 0, 0, 0, 0, &td, "TARGET_CREATURE");
}

int card_ageless_sentinels(int player, int card, event_t event){
  /* Ageless Sentinels	|3|W
   * Creature - Wall 4/4
   * Defender (This creature can't attack.)
   * Flying
   * When this creature blocks, it becomes a Bird Giant, and it loses defender. (It's no longer a Wall. This effect lasts indefinitely.) */

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[5].player != 66 ){
		cannot_attack(player, card, event);
    }

    if(current_turn == 1-player && instance->targets[5].player != 66 && 
		instance->blocking < 255
	  ){
		instance->targets[5].player = 66;
    }
    return 0;    
}

int card_consume_spirit(int player, int card, event_t event){
  /* Consume Spirit	|X|1|B	0x2005642
   * Sorcery
   * Spend only |Sblack mana on X.
   * ~ deals X damage to any target and you gain X life. */

	return generic_x_spell(player, card, event, TARGET_ZONE_CREATURE_OR_PLAYER, COLOR_BLACK, 9);	
}

int card_drain_life(int player, int card, event_t event){
  /* Drain Life	|X|1|B	0x2005647
   * Sorcery
   * Spend only |Sblack mana on X.
   * ~ deals X damage to any target. You gain life equal to the damage dealt, but not more life than the player's life total before the damage was dealt, the planeswalker's loyalty before the damage was dealt, or the creature's toughness. */

// Original code : 0x41E9B0

	return generic_x_spell(player, card, event, TARGET_ZONE_CREATURE_OR_PLAYER, COLOR_BLACK, 17);	
}

int card_blaze(int player, int card, event_t event){
  /* Blaze	|X|R	0x200564c
   * Sorcery
   * ~ deals X damage to any target. */

// Original code : 0x4049B0

	return generic_x_spell(player, card, event, TARGET_ZONE_CREATURE_OR_PLAYER, 0, 1);	
}

int card_stream_of_life(int player, int card, event_t event){
  /* CARD_ID_STREAM_OF_LIFE	237
   * Stream of Life	|X|G
   * Sorcery
   * Target player gains X life. */

// Original code : 

	return generic_x_spell(player, card, event, 0, 0, 2);	
}

int card_braingeyser(int player, int card, event_t event){
  /* CARD_ID_BRAINGEYSER	25
   * Braingeyser	|X|U|U
   * Sorcery
   * Target player draws X cards. */

// Original code : 

	return generic_x_spell(player, card, event, TARGET_ZONE_PLAYERS, 0, 64);	
}

int card_mind_twist(int player, int card, event_t event){
  /* CARD_ID_MIND_TWIST	162
   * Mind Twist	|X|B
   * Sorcery
   * Target player discards X cards at random. */

// Original code : 

	return generic_x_spell(player, card, event, TARGET_ZONE_PLAYERS, 0, 512);	
}

int card_howl_from_beyond(int player, int card, event_t event){
  /* Howl from Beyond	|X|B	=>unlimited.c:card_howl_from_beyond
   * Instant
   * Target creature gets +X/+0 until end of turn. */

// Original code : 

	return generic_x_spell(player, card, event, TARGET_ZONE_IN_PLAY, 0, 2048);	
}

int generic_damage_all_with_x_spell(int player, int card, event_t event, test_definition_t *this_test){

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return ! check_battlefield_for_id(2, CARD_ID_GADDOCK_TEEG);
	}
        
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->info_slot = x_value;
	}

    else if(event == EVENT_RESOLVE_SPELL ){
			new_damage_all(player, card, 2, instance->info_slot, NDA_PLAYER_TOO, this_test);
			kill_card(player, card, KILL_DESTROY);
    }
	return 0;	
}

int card_earthquake(int player, int card, event_t event){
  /* CARD_ID_EARTHQUAKE	75
   * Earthquake	|X|R
   * Sorcery
   * ~ deals X damage to each creature without flying and each player. */

// Original code : 0x41C7E0
	test_definition_t this_test;
	default_test_definition(&this_test, TYPE_CREATURE);
	this_test.keyword = KEYWORD_FLYING;
	this_test.keyword_flag = 1;
	return generic_damage_all_with_x_spell(player, card, event, &this_test);	
}

int card_hurricane(int player, int card, event_t event){
  /* Hurricane	|X|G	=>unlimited.c:card_hurricane
   * Sorcery
   * ~ deals X damage to each creature with flying and each player. */

	test_definition_t this_test;
	default_test_definition(&this_test, TYPE_CREATURE);
	this_test.keyword = KEYWORD_FLYING;
	return generic_damage_all_with_x_spell(player, card, event, &this_test);	
}

int generic_x_spell_recoded(int player, int card, event_t event, int result){

	if( event == EVENT_CAN_CAST ){
		if( result > 0 && ! check_battlefield_for_id(2, CARD_ID_GADDOCK_TEEG) ){
			return result; 
		}
	}
	else{
		return result;
	}
	return 0;		
}

int card_guardian_angel(int player, int card, event_t event){
  /* Guardian Angel	|X|W	0x2005665
   * Instant
   * Prevent the next X damage that would be dealt to any target this turn. Until end of turn, you may pay |1 any time you could cast an instant. If you do, prevent the next 1 damage that would be dealt to that permanent or player this turn. */

	return generic_x_spell_recoded(player, card, event, card_guardian_angel_exe(player, card, event));
}

int card_disintegrate(int player, int card, event_t event){
  /* Disintegrate	|X|R	0x200566a
   * Sorcery
   * ~ deals X damage to any target. If it's a creature, it can't be regenerated this turn, and if it would die this turn, exile it instead. */

	return generic_x_spell_recoded(player, card, event, card_disintegrate_exe(player, card, event));
}

int card_fireball(int player, int card, event_t event){
  /* Fireball	|X|R	0x200566f
   * Sorcery
   * This spell costs |1 more to cast for each target beyond the first.
   * ~ deals X damage divided evenly, rounded down, among any number of targets. */

	return generic_x_spell_recoded(player, card, event, card_fireball_exe(player, card, event));
}

int card_spell_blast(int player, int card, event_t event){
  /* Spell Blast	|X|U
   * Instant
   * Counter target spell with converted mana cost X. */

	return generic_x_spell_recoded(player, card, event, card_spell_blast_exe(player, card, event));
}

int card_power_sink(int player, int card, event_t event){
  /* Power Sink	|X|U	=>unlimited.c:card_power_sink
   * Instant
   * Counter target spell unless its controller pays |X. If he or she doesn't, that player taps all lands with mana abilities he or she controls and empties his or her mana pool. */

	if (event == EVENT_RESOLVE_SPELL){
		ldoubleclicked = 0;
	}
	return generic_x_spell_recoded(player, card, event, card_power_sink_exe(player, card, event));
}

int card_detonate(int player, int card, event_t event){
  /* CARD_ID_DETONATE	490
   * Detonate	|X|R
   * Sorcery
   * Destroy target artifact with converted mana cost X. It can't be regenerated. ~ deals X damage to that artifact's controller. */

	return generic_x_spell_recoded(player, card, event, card_detonate_exe(player, card, event));
}

int card_word_of_binding(int player, int card, event_t event){
  /* CARD_ID_WORD_OF_BINDING	388
   * Word of Binding	|X|B|B
   * Sorcery
   * Tap X target creatures. */

	return generic_x_spell_recoded(player, card, event, card_word_of_binding_exe(player, card, event));
}

int card_winter_blast(int player, int card, event_t event){
  /* CARD_ID_WINTER_BLAST	855
   * Winter Blast	|X|G
   * Sorcery
   * Tap X target creatures. ~ deals 2 damage to each of those creatures with flying. */

	return generic_x_spell_recoded(player, card, event, card_winter_blast_exe(player, card, event));
}

int card_whimsy(int player, int card, event_t event){
  /* Whimsy	|X|U|U
   * Sorcery
   * Play X random fast effects. */

	return generic_x_spell_recoded(player, card, event, card_whimsy_exe(player, card, event));
}

int card_alabaster_potion(int player, int card, event_t event){
  /* Alabaster Potion	|X|W|W	0x200569c
   * Instant
   * Choose one -
   * * Target player gains X life.
   * * Prevent the next X damage that would be dealt to any target this turn. */

    target_definition_t td1;
    default_target_definition(player, card, &td1, TYPE_CREATURE);
    td1.extra = damage_card;
    td1.required_type = 0;

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && ! check_battlefield_for_id(2, CARD_ID_GADDOCK_TEEG) ){
		if( can_target(&td1) ){
			return card_guardian_angel_exe(player, card, event);
		}
		else{
			return 1;
		}
	}
    else if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
			int choice = 0;
			if( can_target(&td1) ){
				choice = do_dialog(player, player, card, -1, -1, " Gain X life\n Prevent X damage\n Cancel", 1);
			}

			if( choice == 0 ){
				instance->targets[1].player = x_value;
				instance->info_slot = 66;
			}
			else if( choice == 1 ){
					if( pick_target(&td1, "TARGET_DAMAGE") ){
						instance->targets[1].player = x_value;
						instance->info_slot = 67;
					}
			}
			else{
				spell_fizzled = 1;
			}
    }

	else if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot == 66 ){
				gain_life(player, instance->targets[1].player);
			}
			if( instance->info_slot == 67 ){
				if( valid_target(&td1) ){
					card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
					if( target->info_slot <= instance->targets[1].player ){
						target->info_slot = 0;
					}
					else{
						target->info_slot-=instance->targets[1].player;
					}
				}
			}
			kill_card(player, card, KILL_DESTROY);
	}

    return 0;
}

void add_oc_counters(int player, int card, int howmany){

    card_instance_t *instance = get_card_instance(player, card);

	if( howmany < 1 ){
		return ;
	}

    int i;
    for( i = 0; i < howmany; i++){
           instance->counters_byte++;
           instance->counter_toughness--;
    }
	play_sound_effect(WAV_COUNTER);
}

int add_my_legacy(int player, int card, int t_player, int t_card){
	int legacy = create_targetted_legacy_effect(player, card, &empty, t_player, t_card);
	card_instance_t *instance = get_card_instance(player, legacy);
	instance->targets[2].card = get_id(player, card);
	return legacy;
}

int card_orcish_catapult(int player, int card, event_t event){
  /* Orcish Catapult	|X|R|R
   * Instant
   * Randomly distribute X -0/-1 counters among a random number of random target creatures. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
	
    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td) ){
		return ! check_battlefield_for_id(2, CARD_ID_GADDOCK_TEEG);
	}
        
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->info_slot = x_value;
		instance->targets[0].player = 0;
		int trgs[99];
		int n_target = 0;
		int i;
		for(i=0; i<2; i++){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) && 
						! is_protected_from_me(player, card, i, count)
					  ){
						trgs[n_target] = (8192 * i) + count;
						n_target++;
					}
					count++;
			}
		}
		int r_targets = internal_rand(n_target);
		int count = 0;
		while( count < r_targets){
				int rnd = internal_rand(n_target);
				if( trgs[rnd] >= 8192 ){
					instance->targets[i+1].player = AI;
					instance->targets[i+1].card = trgs[rnd]-8192;
				}
				else{
					instance->targets[i+1].player = HUMAN;
					instance->targets[i+1].card = trgs[rnd];
				}
				add_my_legacy(player, card, instance->targets[i+1].player, instance->targets[i+1].card);
				int k;
				for(k=rnd; k<n_target; k++){
					trgs[k] = trgs[k+1];
				}
				n_target--;
				count++;
		}
		instance->targets[0].player = r_targets;
	}

    else if(event == EVENT_RESOLVE_SPELL ){
			int i;
			int amount = instance->info_slot;
			for(i=0; i<instance->targets[0].player; i++){
				if( validate_target(player, card, &td, i+1) ){
					int cnters = 1;
					if( amount > 1 ){
						cnters = internal_rand(amount);
					}
					int cters = get_updated_counters_number(instance->targets[i+1].player, cnters);
					add_oc_counters(instance->targets[i+1].player, instance->targets[i+1].card, cters);
					amount-=cnters;
				}
				if( amount < 1 ){
					break;
				}
			}
			kill_my_legacy(player, card);
			kill_card(player, card, KILL_DESTROY);
    }
	return 0;	
}

int card_volcanic_eruption(int player, int card, event_t event){
  /* CARD_ID_VOLCANIC_ERUPTION	265
   * Volcanic Eruption	|X|U|U|U
   * Sorcery
   * Destroy X target |H1Mountains. ~ deals damage to each creature and each player equal to the number of |H1Mountains put into a graveyard this way. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_LAND);
	
    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && has_mana_multi(player, 1, 0, 3, 0, 0, 0) ){
		if( player == AI ){
			if( check_battlefield_for_targettable_subtype(player, card, 1-player, TYPE_LAND, SUBTYPE_MOUNTAIN) &&
				has_mana(player, COLOR_COLORLESS, 1)
			  ){
				return ! check_battlefield_for_id(2, CARD_ID_GADDOCK_TEEG);
			}
		}
		else{
			if( check_battlefield_for_targettable_subtype(player, card, 2, TYPE_LAND, SUBTYPE_MOUNTAIN) ){
				return ! check_battlefield_for_id(2, CARD_ID_GADDOCK_TEEG);
			}
		}
	}
        
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int count = count_subtype(2, TYPE_LAND, SUBTYPE_MOUNTAIN);
		if( player != AI ){
			count = count_subtype(1-player, TYPE_LAND, SUBTYPE_MOUNTAIN);
		}
		int amount = 0;
		if( player != AI ){
			while( count > 0 && can_target(&td) ){
					if( amount > 0 ){
						td.allow_cancel = 0;
					}
					pick_target(&td, "TARGET_LAND");
					if( has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_MOUNTAIN) ){
						instance->targets[amount+1] = instance->targets[0];
						amount++;
						state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
						count--;
						if( count > 0 && can_target(&td) ){
							int choice = do_dialog(player, player, card, player, card, " Continue\n Stop", 0);
							if( choice == 0 ){
								break;
							}
						}
					}
			}
			if( amount > 0 ){
				int i;
				for(i=0; i<amount; i++){
					state_untargettable(instance->targets[i+1].player, instance->targets[i+1].card, 0);
				}
			}

		}
		else{
			int c2 = 0;
			while( c2 < active_cards_count[1-player] && amount < count ){
					if( in_play(1-player, count) && is_what(1-player, count, TYPE_LAND) &&
						has_subtype(1-player, count, SUBTYPE_MOUNTAIN) && 
						! is_protected_from_me(player, card, 1-player, count)
					  ){
						add_my_legacy(player, card, 1-player, count);
						instance->targets[amount+1].player = 1-player;
						instance->targets[amount+1].card = count;
						amount++;
					}
					if( amount < count ){
						if( ! has_mana(player, COLOR_COLORLESS, amount+1) ){
							break;
						}
					}
					c2++;
			}
		}
		if( amount > 0 ){
			charge_mana(player, COLOR_COLORLESS, amount);
			if( spell_fizzled != 1 	){
				instance->info_slot = amount;
			}
		}
		else{
			spell_fizzled = 1;
		}
	}

    else if(event == EVENT_RESOLVE_SPELL ){
			int i;
			int dmg = 0;
			for(i=0; i<instance->info_slot; i++){
				if( validate_target(player, card, &td, i+1) ){
					kill_card(instance->targets[i+1].player, instance->targets[i+1].card, KILL_DESTROY);
					dmg++;
				}
			}
			if( dmg > 0 ){
				damage_all(player, card, player, dmg, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0);
				damage_all(player, card, 1-player, dmg, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0);
			}
			kill_my_legacy(player, card);
			kill_card(player, card, KILL_DESTROY);
    }
	return 0;	
}

int card_crystal_rod(int player, int card, event_t event){
  /* CARD_ID_CRYSTAL_ROD	52
   * Crystal Rod	|1
   * Artifact
   * Whenever a player casts a |Sblue spell, you may pay |1. If you do, you gain 1 life. */

    return lifegaining_charm(player, card, event, 2, COLOR_TEST_BLUE, 1+player, 1);
}

int card_iron_star(int player, int card, event_t event){
  /* CARD_ID_IRON_STAR	123
   * Iron Star	|1
   * Artifact
   * Whenever a player casts a |Sred spell, you may pay |1. If you do, you gain 1 life. */

    return lifegaining_charm(player, card, event, 2, COLOR_TEST_RED, 1+player, 1);
}

int card_ivory_cup(int player, int card, event_t event){
  /* CARD_ID_IVORY_CUP	128
   * Ivory Cup	|1
   * Artifact
   * Whenever a player casts a |Swhite spell, you may pay |1. If you do, you gain 1 life. */

    return lifegaining_charm(player, card, event, 2, COLOR_TEST_WHITE, 1+player, 1);
}

int card_throne_of_bone(int player, int card, event_t event){
  /* CARD_ID_THRONE_OF_BONE	246
   * Throne of Bone	|1
   * Artifact
   * Whenever a player casts a |Sblack spell, you may pay |1. If you do, you gain 1 life. */

    return lifegaining_charm(player, card, event, 2, COLOR_TEST_BLACK, 1+player, 1);
}

int card_wooden_sphere(int player, int card, event_t event){
  /* CARD_ID_WOODEN_SPHERE	288
   * Wooden Sphere	|1
   * Artifact
   * Whenever a player casts a |Sgreen spell, you may pay |1. If you do, you gain 1 life. */

    return lifegaining_charm(player, card, event, 2, COLOR_TEST_GREEN, 1+player, 1);
}

int card_lone_missionary(int player, int card, event_t event){
  /* CARD_ID_LONE_MISSIONARY	10984
   * Lone Missionary	|1|W
   * Creature - Kor Monk 2/1
   * When ~ enters the battlefield, you gain 4 life. */

// Also code for Teroh's Faithful
    return cip_lifegain(player, card, event, 4);
}

int card_angel_of_mercy(int player, int card, event_t event){
  /* Angel of Mercy	|4|W
   * Creature - Angel 3/3
   * Flying
   * When this creature enters, you gain 3 life. */

    return cip_lifegain(player, card, event, 3);
}

int card_diamond_valley(int player, int card, event_t event){
  /* CARD_ID_DIAMOND_VALLEY	410
   * Diamond Valley	""
   * Land
   * |T, Sacrifice a creature: You gain life equal to the sacrificed creature's toughness. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, instance->targets[1].card);
	}
	return altar_basic(player, card, event, 4, TYPE_CREATURE);
}

int card_life_chisel(int player, int card, event_t event){
  /* Life Chisel	|4
   * Artifact
   * Sacrifice a creature: You gain life equal to the sacrificed creature's toughness. Activate only during your upkeep. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, instance->targets[1].card);
	}
	return altar_basic(player, card, event, 24, TYPE_CREATURE);
}

int card_zuran_orb(int player, int card, event_t event){
  /* Zuran Orb	|0	0x20057b4
   * Artifact
   * Sacrifice a land: You gain 2 life. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 2);
	}
	return altar_basic(player, card, event, 0, TYPE_LAND);
}

int card_dark_heart_of_the_woods(int player, int card, event_t event){
  /* Dark Heart of the Wood	|B|G
   * Enchantment
   * Sacrifice a Forest: You gain 3 life. */

    if( event == EVENT_SHOULD_AI_PLAY ){
        return should_ai_play(player, card);
    } 

    else if( event == EVENT_CAN_CAST ){
			return 1;
    }

	else if( event == EVENT_RESOLVE_ACTIVATION ){
			gain_life(player, 3);
	}
	else{
		return altar_extended(player, card, event, 0, TYPE_LAND, 0, SUBTYPE_FOREST, 0, 0, 0, 0, 0, -1, 0);
	}

	return 0;
}

int card_farmstead(int player, int card, event_t event){
  /* Farmstead	|W|W|W
   * Enchantment - Aura
   * Enchant land
   * Enchanted land has "At the beginning of your upkeep, you may pay |W|W. If you do, you gain 1 life." */

	if( current_turn == player && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) &&
		reason_for_trigger_controller == player
	  ){
		int trig = 0;
		if( has_mana(player, COLOR_WHITE, 3) ){
			trig = 1;
        }
		if( trig == 1 ){
			if(event == EVENT_TRIGGER){
				event_result |= 1+player;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					charge_mana(player, COLOR_WHITE, 3);
					if( spell_fizzled != 1 ){
						gain_life(player, 1);
					}
			}
		}
    }
	else{
		return card_farmstead_exe(player, card, event); 
    }

    return 0;
}

int card_lifeblood(int player, int card, event_t event){
  /* Lifeblood	|2|W|W
   * Enchantment
   * Whenever |Ha Mountain an opponent controls becomes tapped, you gain 1 life. */

// Also code for Sanctimony
	if( event == EVENT_TAP_CARD && affected_card_controller == 1-player ){
//		if( has_subtype(affected_card_controller, affected_card, get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN)) ){
		if( has_subtype(affected_card_controller, affected_card, SUBTYPE_MOUNTAIN) ){
			gain_life(player, 1);
		}
	}

    return global_enchantment(player, card, event);
}

int card_spiritual_sanctury(int player, int card, event_t event){
  /* Spiritual Sanctuary	|2|W|W
   * Enchantment
   * At the beginning of each player's upkeep, if that player controls a Plains, they gain 1 life. */

    if(	get_forbid_activation_flag(player, card) != 4 && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		int count = count_upkeeps(player);
        if(event == EVENT_TRIGGER && count > 0 && check_battlefield_for_subtype(current_turn, TYPE_LAND, SUBTYPE_PLAINS) ){
            event_result |= 2;
        }
        else if(event == EVENT_RESOLVE_TRIGGER){
				
				card_instance_t *instance= get_card_instance(player, card);
				card_data_t* card_d = &cards_data[ instance->internal_card_id ]; 
				int (*ptFunction)(int, int, event_t) = (void*)card_d->code_pointer;
				while( count > 0 ){
						ptFunction(player, card, EVENT_UPKEEP_TRIGGER_ABILITY );
						count--;
				}
        }
    }

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		gain_life(current_turn, 1);
	}

    return global_enchantment(player, card, event);
}

int card_chastise(int player, int card, event_t event){
  /* CARD_ID_CHASTISE	991
   * Chastise	|3|W
   * Instant
   * Destroy target attacking creature. You gain life equal to its power. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}
        
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		pick_target(&td, "TARGET_CREATURE");
	}

    else if(event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				int amount = get_power(instance->targets[0].player, instance->targets[0].card);
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				gain_life(player, amount);
			}
			kill_card(player, card, KILL_DESTROY);
    }
	return 0;	
}

int card_divine_offering(int player, int card, event_t event){
  /* CARD_ID_DIVINE_OFFERING	615
   * Divine Offering	|1|W
   * Instant
   * Destroy target artifact. You gain life equal to its converted mana cost. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_ARTIFACT);

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}
        
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		pick_target(&td, "TARGET_ARTIFACT");
	}

    else if(event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				int amount = get_cmc(instance->targets[0].player, instance->targets[0].card);
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				gain_life(player, amount);
			}
			kill_card(player, card, KILL_DESTROY);
    }
	return 0;	
}

int card_healing_salve(int player, int card, event_t event){
  /* Healing Salve	|W	0x200571e
   * Instant
   * Choose one -
   * * Target player gains 3 life.
   * * Prevent the next 3 damage that would be dealt to any target this turn. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

    target_definition_t td1;
    default_target_definition(player, card, &td1, TYPE_CREATURE);
    td1.extra = damage_card;
    td1.required_type = 0;

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( can_target(&td1) ){
			return card_guardian_angel_exe(player, card, event);
		}
		else{
			return can_target(&td);
		}
	}
    else if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
			int choice = 0;
			if( can_target(&td) ){
				if( can_target(&td1) ){
					choice = do_dialog(player, player, card, -1, -1, " Gain 3 life\n Prevent 3 damage\n Cancel", 1);
				}
			}
			else{
				choice = 1;
			}

			if( choice == 0 ){
				if( pick_target(&td, "TARGET_PLAYER") ){
					instance->info_slot = 66;
				}
			}
			else if( choice == 1 ){
					if( pick_target(&td1, "TARGET_DAMAGE") ){
						instance->info_slot = 67;
					}
			}
			else{
				spell_fizzled = 1;
			}
    }

	else if( event == EVENT_RESOLVE_SPELL ){
			if( instance->info_slot == 66 && valid_target(&td) ){
				gain_life(instance->targets[0].player, 3);
			}
			if( instance->info_slot == 67 ){
				if( valid_target(&td1) ){
					card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
					if( target->info_slot <= 3 ){
						target->info_slot = 0;
					}
					else{
						target->info_slot-=3;
					}
				}
			}
			kill_card(player, card, KILL_DESTROY);
	}

    return 0;
}

int card_punish_ignorance(int player, int card, event_t event){
  /* Punish Ignorance	|W|U|U|B
   * Instant
   * Counter target spell. Its controller loses 3 life and you gain 3 life. */

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		int result = card_counterspell(player, card, event);
		if( result > 0 ){
			instance->targets[1].player = card_on_stack_controller;
			return result;
		}
	}

	else if( event == EVENT_RESOLVE_SPELL ){
			gain_life(player, 3);
			lose_life(instance->targets[1].player, 3);
			return card_counterspell(player, card, event);
	}
	else{
		return card_counterspell(player, card, event);
	}

    return 0;
}

int card_reverse_damage(int player, int card, event_t event){
  /* CARD_ID_REVERSE_DAMAGE	204
   * Reverse Damage	|1|W|W
   * Instant
   * The next time a source of your choice would deal damage to you this turn, prevent that damage. You gain life equal to the damage prevented this way. */

    target_definition_t td1;
    default_target_definition(player, card, &td1, TYPE_ANY);
    td1.extra = damage_card;
	td1.special = 32; // Only damage to players

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( (land_can_be_played & 4) && can_target(&td1) ){
			return 99;
		}
	}
    else if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
			if( pick_target(&td1, "TARGET_DAMAGE") ){
				card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
				if( target->damage_target_card != -1 ){
					spell_fizzled = 1;
				}
			}
    }

	else if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td1) ){
				card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
				gain_life(player, target->info_slot);
				target->info_slot = 0;
			}
			kill_card(player, card, KILL_DESTROY);
	}

    return 0;
}

int card_kiss_of_the_amesha(int player, int card, event_t event){
  /* CARD_ID_KISS_OF_THE_AMESHA	1324
   * Kiss of the Amesha	|4|W|U
   * Sorcery
   * Target player gains 7 life and draws two cards. */

    target_definition_t td1;
    default_target_definition(player, card, &td1, TYPE_CREATURE);
    td1.zone = TARGET_ZONE_PLAYERS;
	td1.preferred_controller = player;

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td1);
	}
    else if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
			pick_target(&td1, "TARGET_PLAYER");
    }

	else if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td1) ){
				gain_life(instance->targets[0].player, 7);
				draw_cards(instance->targets[0].player, 2);
			}
			kill_card(player, card, KILL_DESTROY);
	}

    return 0;
}

int card_peach_garden_oath(int player, int card, event_t event){
  /* Peach Garden Oath	|W
   * Sorcery
   * You gain 2 life for each creature you control. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.allowed_controller = player;
	td.illegal_abilities = 0;
//    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}
        
    else if(event == EVENT_RESOLVE_SPELL ){
			int amount = target_available(player, card, &td) * 2;
			gain_life(player, amount);
			kill_card(player, card, KILL_DESTROY);
    }
	return 0;	
}

int card_sacred_nectar(int player, int card, event_t event){
  /* Sacred Nectar	|1|W
   * Sorcery
   * You gain 4 life. */

	if( event == EVENT_CAN_CAST ){
		return 1;
	}
        
    else if(event == EVENT_RESOLVE_SPELL ){
			gain_life(player, 4);
			kill_card(player, card, KILL_DESTROY);
    }
	return 0;	
}

int card_living_artifact(int player, int card, event_t event){
  /* Living Artifact	|G
   * Enchantment - Aura
   * Enchant artifact
   * Whenever you're dealt damage, put that many vitality counters on ~.
   * At the beginning of your upkeep, you may remove a vitality counter from ~. If you do, you gain 1 life. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.preferred_controller = player;

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}
    else if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
			ai_modifier+=100;
			pick_target(&td, "TARGET_ARTIFACT");
    }

	else if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				instance->damage_target_player = instance->targets[0].player;
				instance->damage_target_card = instance->targets[0].card;
			}
			else{
				kill_card(player, card, KILL_SACRIFICE);
			}
	}

	if( in_play(player, card) && instance->targets[0].player != -1 ){
		damage_effects(player, card, event);
		
		if( current_turn == player && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) &&
			reason_for_trigger_controller == player
		  ){
			int trig = 0;
			if( count_counters(player, card) > 0 ){
				trig = 1;
			}
			if( trig == 1 ){
				if(event == EVENT_TRIGGER){
					event_result |= 1+player;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						remove_counter(player, card);
						gain_life(player, 1);
				}
			}
		}
	}

    return 0;
}

int card_crumble(int player, int card, event_t event){
  /* CARD_ID_CRUMBLE	487
   * Crumble	|G
   * Instant
   * Destroy target artifact. It can't be regenerated. That artifact's controller gains life equal to its converted mana cost. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_ARTIFACT);

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}
        
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		pick_target(&td, "TARGET_ARTIFACT");
	}

    else if(event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				int amount = get_cmc(instance->targets[0].player, instance->targets[0].card);
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
				gain_life(instance->targets[0].player, amount);
			}
			kill_card(player, card, KILL_DESTROY);
    }
	return 0;	
}

int card_biorythm(int player, int card, event_t event){
  /* Biorhythm	|6|G|G
   * Sorcery
   * Each player's life total becomes the number of creatures they control. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.allowed_controller = player;
	td.illegal_abilities = 0;

	if( event == EVENT_CAN_CAST ){
		if( player == AI ){
			return can_target(&td);
		}
		else{
			return 1;
		}
	}
        
    else if(event == EVENT_RESOLVE_SPELL ){
			set_life_total(player, count_permanents_by_type(player, card, TYPE_CREATURE));
			set_life_total(1-player, count_permanents_by_type(1-player, card, TYPE_CREATURE));
			kill_card(player, card, KILL_DESTROY);
    }
	return 0;	
}

int card_sol_kanar_the_swamp_king(int player, int card, event_t event){
  /* Sol'kanar the Swamp King	|2|U|B|R
   * Legendary Creature - Demon 5/5
   * Swampwalk (This creature can't be blocked as long as defending player controls a Swamp.)
   * Whenever a player casts a black spell, you gain 1 life. */

	check_legend_rule(player, card, event);
	
	if( specific_spell_played(player, card, event, 2, 2, TYPE_ANY, 0, 0, 0, COLOR_TEST_BLACK, 0, 0, 0, -1, 0) ){
		gain_life(player, 1);
	}

	return 0;	
}

int card_merchant_ship(int player, int card, event_t event){
  /* CARD_ID_MERCHANT_SHIP	440
   * Merchant Ship	|U
   * Creature - Human 0/2
   * ~ can't attack unless defending player controls |Han Island.
   * Whenever ~ attacks and isn't blocked, you gain 2 life.
   * When you control no |H1Islands, sacrifice ~. */

//	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ATTACK_LEGALITY && affect_me(player, card) ){
		if( ! check_battlefield_for_subtype(1-player, TYPE_LAND, SUBTYPE_ISLAND) ){
			event_result = 1;
		}     
	}     

	if( current_turn == player && is_attacking(player, card) && event == EVENT_DECLARE_BLOCKERS &&
		affect_me(player, card) && is_unblocked(player, card)
	  ){
		gain_life(player, 2);
	}     
	
	if( ! check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_ISLAND) ){
		kill_card(player, card, KILL_SACRIFICE);
	}     

	return 0;
}

int card_lifetap(int player, int card, event_t event){
  /* CARD_ID_LIFETAP	144
   * Lifetap	|U|U
   * Enchantment
   * Whenever |Ha Forest an opponent controls becomes tapped, you gain 1 life. */

	if( event == EVENT_TAP_CARD && affected_card_controller == 1-player ){
		if( has_subtype(affected_card_controller, affected_card, get_hacked_subtype(player, card, SUBTYPE_FOREST)) ){
			gain_life(player, 1);
		}
	}

    return global_enchantment(player, card, event);
}

int card_relic_bind2(int player, int card, event_t event){
  /* Relic Bind	|2|U
   * Enchantment - Aura
   * Enchant artifact an opponent controls
   * Whenever enchanted artifact becomes tapped, choose one -
   * * This Aura deals 1 damage to target player or planeswalker.
   * * Target player gains 1 life. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.preferred_controller = 1-player;
	td.allowed_controller = 1-player;

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}
    else if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
			ai_modifier+=100;
			pick_target(&td, "TARGET_ARTIFACT");
    }

	else if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				instance->damage_target_player = instance->targets[0].player;
				instance->damage_target_card = instance->targets[0].card;
			}
			else{
				kill_card(player, card, KILL_SACRIFICE);
			}
	}

	if( in_play(player, card) && instance->targets[0].player != -1 ){
		if( event == EVENT_TAP_CARD && affect_me(instance->targets[0].player, instance->targets[0].card) ){
			int choice = do_dialog(player, player, card, player, card, " Deal 1 damge\n Gain 1 life", 0);
			if( choice == 0 ){
				damage_player(instance->targets[0].player, 1, player, card);
			}
			if( choice == 1 ){
				gain_life(player, 1);
			}
		}
	}

    return 0;
}

void exchange_life_totals(int player, int t_player){
	if( player == t_player ){
		return;
	}
	int my_life = life[player];
	if( check_battlefield_for_id(player, CARD_ID_LICH) ){
		my_life = 0;
	}
	int his_life = life[t_player];
	if( check_battlefield_for_id(t_player, CARD_ID_LICH) ){
		his_life = 0;
	}
	set_life_total(player, his_life);
	set_life_total(t_player, my_life);
}

int card_mirror_universe(int player, int card, event_t event){
  /* CARD_ID_MIRROR_UNIVERSE	725
   * Mirror Universe	|6
   * Artifact
   * |T, Sacrifice ~: Exchange life totals with target opponent. Activate this ability only during your upkeep. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

    if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td)  ){
			exchange_life_totals(player, 1-player);
		}
    }

	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK+GAA_CAN_ONLY_TARGET_OPPONENT+GAA_SACRIFICE_ME+GAA_ONLY_ON_UPKEEP+GAA_IN_YOUR_TURN, 
									0, 0, 0, 0, 0, 0, 0, &td, "TARGET_PLAYER");
}

int card_magus_of_the_mirror(int player, int card, event_t event){
  /* Magus of the Mirror	|4|B|B
   * Creature - Human Wizard 4/2
   * |T, Sacrifice this creature: Exchange life totals with target opponent. Activate only during your upkeep. */

	return card_mirror_universe(player, card, event);
}

int card_vicious_hunger(int player, int card, event_t event){
  /* CARD_ID_VICIOUS_HUNGER	1185
   * Vicious Hunger	|B|B
   * Sorcery
   * ~ deals 2 damage to target creature and you gain 2 life. */

// Also code for Douse in Gloom and Sorin't Thirst

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}
    else if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
			pick_target(&td, "TARGET_CREATURE");
    }

	else if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, 2, player, card);
				gain_life(player, 2);
			}
			kill_card(player, card, KILL_DESTROY);
	}

    return 0;
}

int card_simulacrum(int player, int card, event_t event){
  /* CARD_ID_SIMULACRUM	225
   * Simulacrum	|1|B
   * Instant
   * You gain life equal to the damage dealt to you this turn. ~ deals damage to target creature you control equal to the damage dealt to you this turn. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( player == AI ){
			if( get_trap_condition(player, TRAP_DAMAGE_TAKEN) > 0 ){
				return can_target(&td);
			}
		}
		else{
			return can_target(&td);
		}
	}
    else if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
			pick_target(&td, "TARGET_CREATURE");
    }

	else if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				int amount = get_trap_condition(player, TRAP_DAMAGE_TAKEN);
				damage_creature(instance->targets[0].player, instance->targets[0].card, amount, player, card);
				gain_life(player, amount);
			}
			kill_card(player, card, KILL_DESTROY);
	}

    return 0;
}

int card_sever_soul(int player, int card, event_t event){
  /* CARD_ID_SEVER_SOUL	1144
   * Sever Soul	|3|B|B
   * Sorcery
   * Destroy target non|Sblack creature. It can't be regenerated. You gain life equal to its toughness. */

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = COLOR_TEST_BLACK;

    card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}
    else if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
			pick_target(&td, "TARGET_CREATURE");
    }

	else if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				int amount = get_toughness(instance->targets[0].player, instance->targets[0].card);
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
				gain_life(player, amount);
			}
			kill_card(player, card, KILL_DESTROY);
	}

    return 0;
}

int card_syphon_soul(int player, int card, event_t event){
  /* CARD_ID_SYPHON_SOUL	805
   * Syphon Soul	|2|B
   * Sorcery
   * ~ deals 2 damage to each other player. You gain life equal to the damage dealt this way. */

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	else if( event == EVENT_RESOLVE_SPELL ){
			int amount = lose_life(1-player, 2);
			gain_life(player, amount);
			kill_card(player, card, KILL_DESTROY);
	}

    return 0;
}

int card_fountain_of_youth(int player, int card, event_t event){
  /* CARD_ID_FOUNTAIN_OF_YOUTH	324
   * Fountain of Youth	|0
   * Artifact
   * |2, |T: You gain 1 life. */

    if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 1);
    }

	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK, 2, 0, 0, 0, 0, 0, 0, 0, 0);
}

int card_ivory_tower(int player, int card, event_t event){
  /* CARD_ID_IVORY_TOWER	503
   * Ivory Tower	|1
   * Artifact
   * At the beginning of your upkeep, you gain X life, where X is the number of cards in your hand minus 4. */

    if(	get_forbid_activation_flag(player, card) != 4 && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) &&
		current_turn == player
	  ){
		int count = count_upkeeps(player);
        if(event == EVENT_TRIGGER && count > 0 && hand_count[player] > 4 ){
            event_result |= 2;
        }
        else if(event == EVENT_RESOLVE_TRIGGER){
				card_instance_t *instance= get_card_instance(player, card);
				card_data_t* card_d = &cards_data[ instance->internal_card_id ]; 
				int (*ptFunction)(int, int, event_t) = (void*)card_d->code_pointer;
				while( count > 0 ){
						ptFunction(player, card, EVENT_UPKEEP_TRIGGER_ABILITY );
						count--;
				}
        }
    }

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int amount = hand_count[player]-4;
		gain_life(player, amount);
	}

    return 0;
}

int card_onulet(int player, int card, event_t event){
  /* CARD_ID_ONULET	512
   * Onulet	|3
   * Artifact Creature - Construct 2/2
   * When ~ dies, you gain 2 life. */

	if( graveyard_from_play(player, card, event) ){
		gain_life(player, 2);
	}

    return 0;
}

int card_soul_net(int player, int card, event_t event){
  /* CARD_ID_SOUL_NET	231
   * Soul Net	|1
   * Artifact
   * Whenever a creature dies, you may pay |1. If you do, you gain 1 life. */

    card_instance_t *instance = get_card_instance(player, card);

	if( has_mana(player, COLOR_COLORLESS, 1) && 
		count_graveyard_from_play(player, card, event, 10, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0)
	  ){
		int i;
		for(i=0; i<instance->targets[11].card; i++){
			if( ! has_mana(player, COLOR_COLORLESS, 1) ){
				break;
			}
			charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, COLOR_COLORLESS, 1);
			if( spell_fizzled == 1 ){
				break;
			} else {
				gain_life(player, 1);
			}
		}
		instance->targets[11].card = 0;
	}

    return 0;
}

int card_tablet_of_epityr(int player, int card, event_t event){
  /* Tablet of Epityr	|1
   * Artifact
   * Whenever an artifact you control is put into a graveyard from the battlefield, you may pay |1. If you do, you gain 1 life. */

    card_instance_t *instance = get_card_instance(player, card);

	if( has_mana(player, COLOR_COLORLESS, 1) && 
		count_graveyard_from_play(player, card, event, player, TYPE_ARTIFACT, 0, 0, 0, 0, 0, 0, 0, -1, 0)
	  ){
		int i;
		for(i=0; i<instance->targets[11].card; i++){
			if( ! has_mana(player, COLOR_COLORLESS, 1) ){
				break;
			}
			charge_mana(player, COLOR_COLORLESS, 1);
			if( spell_fizzled != 1 ){
				gain_life(player, 1);
			}
		}
		instance->targets[11].card = 0;
	}

    return 0;
}

int card_urzas_chalice(int player, int card, event_t event){
  /* CARD_ID_URZAS_CHALICE	540
   * Urza's Chalice	|1
   * Artifact
   * Whenever a player casts an artifact spell, you may pay |1. If you do, you gain 1 life. */

	if( has_mana(player, COLOR_COLORLESS, 1) && 
		specific_spell_played(player, card, event, 2, 1+player, TYPE_ARTIFACT, 0, 0, 0, 0, 0, 0, 0, -1, 0)
	  ){
		charge_mana(player, COLOR_COLORLESS, 1);
		if( spell_fizzled != 1 ){
			gain_life(player, 1);
		}
	}

    return 0;
}

int card_enlightened_tutor(int player, int card, event_t event){
  /* CARD_ID_ENLIGHTENED_TUTOR	1525
   * Enlightened Tutor	|W
   * Instant
   * Search your library for an artifact or enchantment card and reveal that card. Shuffle your library, then put the card on top of it. */

	if( event == EVENT_CAN_CAST ){
		return 1;
    }
	
    else if(event == EVENT_RESOLVE_SPELL ){
			global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_DECK, 0, 1, TYPE_ARTIFACT | TYPE_ENCHANTMENT, 0, 0, 0, 0, 0, 0, 0, -1, 0);
			kill_card(player, card, KILL_DESTROY);
    }
	
    return 0;    
}

int card_moroii(int player, int card, event_t event){
  /* CARD_ID_MOROII	1311
   * Moroii	|2|U|B
   * Creature - Vampire 4/4
   * Flying
   * At the beginning of your upkeep, you lose 1 life. */

	upkeep_trigger_ability(player, card, event, player);	

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		lose_life(player, 1);
	}

    return 0;    
}

int card_nevinyrrals_disk(int player, int card, event_t event){
  /* CARD_ID_NEVINYRRALS_DISK	173
   * Nevinyrral's Disk	|4
   * Artifact
   * ~ enters the battlefield tapped.
   * |1, |T: Destroy all artifacts, creatures, and enchantments. */

// 0x425A40
	comes_into_play_tapped(player, card, event);
	
	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_NONLAND);
		this_test.type_flag = F1_NO_PWALKER;
		new_manipulate_all(player, card, 2, &this_test, KILL_DESTROY);
	}
	
	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK, 1, 0, 0, 0, 0, 0, 0, 0, 0);
}

int card_goblin_shrine(int player, int card, event_t event){
  /* CARD_ID_GOBLIN_SHRINE	938
   * Goblin Shrine	|1|R|R
   * Enchantment - Aura
   * Enchant land
   * As long as enchanted land is a basic |H2Mountain, Goblin creatures get +1/+0.
   * When ~ leaves the battlefield, it deals 1 damage to each Goblin creature. */

    card_instance_t *instance = get_card_instance(player, card);
	
	if( in_play(player, card) && instance->targets[0].player != -1 ){
		if( get_id(instance->targets[0].player, instance->targets[0].card) == CARD_ID_MOUNTAIN ){
			boost_creature_type(player, card, event, SUBTYPE_GOBLIN, 1, 0, 0, BCT_INCLUDE_SELF);
		}
		if( leaves_play(player, card, event, RESOLVE_TRIGGER_MANDATORY, 1) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.subtype = SUBTYPE_GOBLIN;
			new_damage_all(player, card, 2, 1, 0, &this_test);
		}
	}
	else{
		return card_goblin_shrine_exe(player, card, event);
	}
	return 0;		
}

int card_goblin_caves(int player, int card, event_t event){
  /* CARD_ID_GOBLIN_CAVES	329
   * Goblin Caves	|1|R|R
   * Enchantment - Aura
   * Enchant land
   * As long as enchanted land is a basic |H2Mountain, Goblin creatures get +0/+2. */

    card_instance_t *instance = get_card_instance(player, card);
	
	if( in_play(player, card) && instance->targets[0].player != -1 ){
		if( get_id(instance->targets[0].player, instance->targets[0].card) == CARD_ID_MOUNTAIN ){
			boost_creature_type(player, card, event, SUBTYPE_GOBLIN, 0, 2, 0, BCT_INCLUDE_SELF);
		}
	}
	else{
		return card_goblin_shrine_exe(player, card, event);
	}
	return 0;		
}

extern int hack_animate_dead_player, hack_animate_dead_card;
int card_animate_dead(int player, int card, event_t event){
  /* CARD_ID_ANIMATE_DEAD	3
   * Animate Dead	|1|B
   * Enchantment - Aura
   * Enchant creature card in a graveyard
   * When ~ enters the battlefield, if it's on the battlefield, it loses "enchant creature card in a graveyard" and gains "enchant creature put onto the battlefield with ~."
   * Return enchanted creature card to the battlefield under your control and attach ~ to it. When ~ leaves the battlefield, that creature's controller sacrifices it.
   * Enchanted creature gets -1/-0. */

// 0x4B5780
	// See also card_dance_of_the_dead() in ice_age.c and card_necromancy() in visions.c

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;
	td.illegal_abilities = 0;

	char msg[100] = "Select a creature card.";
	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, msg);

	card_instance_t *instance = get_card_instance( player, card );

	if( in_play(player, card) && instance->damage_target_player != -1 ){
		modify_pt_and_abilities(instance->damage_target_player, instance->damage_target_card, event, -1, 0, 0);

		if( leaves_play(player, card, event, RESOLVE_TRIGGER_MANDATORY, 1) ){
			kill_card(instance->damage_target_player, instance->damage_target_card, KILL_SACRIFICE);
		}
		
		if( leaves_play(instance->damage_target_player, instance->damage_target_card, event, RESOLVE_TRIGGER_MANDATORY, 1) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}
	if( event == EVENT_CAN_CAST && (count_graveyard_by_type(player, TYPE_CREATURE) > 0 || count_graveyard_by_type(1-player, TYPE_CREATURE)) ){
		return ! graveyard_has_shroud(2);
	}
	
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->targets[0].player = 1-player;
		if( count_graveyard_by_type(1-player, TYPE_CREATURE) > 0 ){
			if( count_graveyard_by_type(player, TYPE_CREATURE) > 0 ){
				pick_target(&td, "TARGET_PLAYER");
			}
		}
		else{
			instance->targets[0].player = player;
		}
		if( new_select_target_from_grave(player, card, instance->targets[0].player, 0, AI_MAX_CMC, &this_test, 1) == -1 ){
			spell_fizzled = 1;
		}
	}
	if( event == EVENT_RESOLVE_SPELL){
		/* real_put_into_play removes STATE_INVISIBLE just after sending EVENT_RESOLVE_SPELL instead of just before; remove it now, so in_play() works right in
		   any come-into-play effects.  (This might be a more reliable way of determining whether a card was played from hand.) */
		instance->state &= ~STATE_INVISIBLE;

		int selected = validate_target_from_grave(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			hack_animate_dead_player = player;
			hack_animate_dead_card = card;
			reanimate_permanent(player, card, instance->targets[0].player, selected, 9, 0);
		}
		else{
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_whirlwind_derwish(int player, int card, event_t event){
  /* Whirling Dervish	|G|G
   * Creature - Human Monk 1/1
   * Protection from black
   * At the beginning of each end step, if this creature dealt damage to an opponent this turn, put a +1/+1 counter on it. */

// whirling dervish
	card_instance_t *instance = get_card_instance( player, card );

    if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_OPPONENT) ){
		instance->targets[1].player = 66;
    }
	if( instance->targets[1].player == 66 && eot_trigger(player, card, event) ){
		add_1_1_counter(player, card, player, card);
		instance->targets[1].player = 0;
    }

    return 0;
}

int card_raise_dead(int player, int card, event_t event){
  /* CARD_ID_RAISE_DEAD	198
   * Raise Dead	|B
   * Sorcery
   * Return target creature card from your graveyard to your hand. */

	char msg[100] = "Select a creature card.";
	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, msg);

    if( event == EVENT_CAN_CAST && count_graveyard_by_type(player, TYPE_CREATURE) > 0 ){
        return ! graveyard_has_shroud(2);
    }

    if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( new_select_target_from_grave(player, card, player, 0, AI_MAX_VALUE, &this_test, 0) == -1 ){
			spell_fizzled = 1;
		}
    }

    if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
		kill_card(player, card, KILL_DESTROY);
    }

    return 0;
}

int card_clockwork_beast(int player, int card, event_t event){
  /* CARD_ID_CLOCKWORK_BEAST	38
   * Clockwork Beast	|6
   * Artifact Creature - Beast 0/4
   * ~ enters the battlefield with seven +1/+0 counters on it.
   * At end of combat, if ~ attacked or blocked this combat, remove a +1/+0 counter from it.
   * |X, |T: Put up to X +1/+0 counters on ~. This ability can't cause the total number of +1/+0 counters on ~ to be greater than seven.
   * Activate this ability only during your upkeep. */

	return hom_clockwork(player, card, event, 7);
}

int card_drop_of_honey(int player, int card, event_t event){
  /* CARD_ID_DROP_OF_HONEY	411
   * Drop of Honey	|G
   * Enchantment
   * At the beginning of your upkeep, destroy the creature with the least power. It can't be regenerated.
   * If two or more creatures are tied for least power, you choose one of them. When there are no creatures on the battlefield, sacrifice ~. */

// 0x430FA0

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.illegal_abilities = 0;
    td.allow_cancel = 0;

    card_instance_t *instance = get_card_instance(player, card);
 
	upkeep_trigger_ability(player, card, event, player);	

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( can_target(&td) ){
			int i;
			int t_player = -1;
			int t_card = -1;
			int doubles_count = 1;
			int min_t = 100;
			int count;
			for(i=0; i<2; i++){
				count = active_cards_count[i]-1;
				while( count > -1 ){
					   if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) ){
						   if( get_power(i, count) < min_t ){
							   t_player = i;
							   t_card = count;
							   min_t = get_power(i, count);
							   doubles_count = 1;
							   instance->targets[doubles_count].player = t_player;
							   instance->targets[doubles_count].card = t_card;
						   }
						   else if( get_power(i, count) == min_t ){
									doubles_count++;
									instance->targets[doubles_count].player = i;
									instance->targets[doubles_count].card = count;
						   }


					   }
					   count--;
				}
			}

			if( doubles_count > 1 ){
				i=0;
				for(i=0; i<2; i++){
					count = 0;
					while( count < active_cards_count[i] ){
							if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) ){
								card_instance_t *this = get_card_instance(i, count);
								this->state |= STATE_CANNOT_TARGET;
							}     
							count++;
					}     
				}     
				int k;
				for(k=0; k<doubles_count; k++){
					card_instance_t *this = get_card_instance(instance->targets[k+1].player, instance->targets[k+1].card);
					this->state  &= ~STATE_CANNOT_TARGET;
				}
				if( pick_target(&td, "TARGET_CREATURE") ){
					instance->number_of_targets = 1;
					t_player = instance->targets[0].player;
					t_card = instance->targets[0].card;
					for(i=0; i<2; i++){
						count = 0;
						while( count < active_cards_count[i] ){
								if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) ){
									card_instance_t *this = get_card_instance(i, count);
									if( this->state & STATE_CANNOT_TARGET ){
										this->state  &= ~STATE_CANNOT_TARGET;
									}     
								}     
								count++;
						}     
					}     
				}
			}

			if( t_player != -1 && t_card != -1 ){
				kill_card(t_player, t_card, KILL_BURY);
			}
		}
	}

	if( ! can_target(&td) && in_play(player, card) ){
		kill_card(player, card, KILL_SACRIFICE);
    }

    return global_enchantment(player, card, event);
}

int card_tranquillity(int player, int card, event_t event){
  /* Tranquility	|2|G
   * Sorcery
   * Destroy all enchantments. */

    if( event == EVENT_CAN_CAST ){
		return 1;
    }

    if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		if( ! check_battlefield_for_id(2, CARD_ID_ENCHANTED_EVENING) ){
			default_test_definition(&this_test, TYPE_ENCHANTMENT);
		}
		else{
			default_test_definition(&this_test, TYPE_PERMANENT);
		}
		new_manipulate_all(player, card, 2, &this_test, KILL_DESTROY);
		kill_card(player, card, KILL_DESTROY);
    }

    return 0;
}

int card_desert2(int player, int card, event_t event){
  /* Desert	""
   * Land - Desert
   * |T: Add |C.
   * |T: This land deals 1 damage to target attacking creature. Activate only during the end of combat step. */

// original code : 0x4A8970

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller = current_turn;
	td1.preferred_controller = current_turn;
	td1.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

    if( trigger_condition == TRIGGER_END_COMBAT && affect_me(player, card) && reason_for_trigger_controller == player ){
		int trig = 0;
		int count = 0;
		while( count < active_cards_count[current_turn] ){
				if( in_play(current_turn, count) && is_what(current_turn, count, TYPE_CREATURE) && is_attacking(current_turn, count) ){
					instance->targets[0].player = current_turn;
					instance->targets[0].card = count;
					instance->number_of_targets = 1;
					if( valid_target(&td1) ){
						if( player == HUMAN ){
							trig = 1;
							break;
						}
						else{
							if( get_toughness(current_turn, count) < 2 ){
								trig = 1;
								break;
							}
						}
					}
				}
				count++;
        }
		if( ! is_tapped(player, card) && trig == 1 && (( player == AI && current_turn != player) || player != AI) ){
			if( event == EVENT_TRIGGER){
				event_result |= 2;
			}
			else if( event == EVENT_RESOLVE_TRIGGER ){
					int choice = do_dialog(player, player, card, -1, -1, " Activate Desert\n Pass", 0);
					if( choice == 0 && pick_target(&td1, "TARGET_CREATURE") ){
						instance->number_of_targets = 1;
						tap_card(player, card);
						damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
					}
			}
		}
	}
	
    return mana_producer(player, card, event);
}

int card_assembly_worker(int player, int card, event_t event){
  /* CARD_ID_ASSEMBLY_WORKER	910
   * Assembly-Worker	""(token)
   * Artifact Creature Land - Assembly-Worker 2/2
   * |T: Add |C to your mana pool.
   * |T: Target ~ creature gets +1/+1 until end of turn.
   * |1: Until end of turn, Mishra's Factory becomes a 2/2 ~ artifact creature. It's still a land. */

	if( get_special_infos(player, card) != 66 ){
		return card_assembly_worker_exe(player, card, event);
	}
	return 0;
}

int card_fellwar_stone(int player, int card, event_t event){
  /* CARD_ID_FELLWAR_STONE	318
   * Fellwar Stone	|2
   * Artifact
   * |T: Add to your mana pool one mana of any color that a land an opponent controls could produce. */

// original code : 0x422360.  Also Exotic Orchard.

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_CHANGE_TYPE && affect_me(player, card)){
		instance->mana_color = instance->card_color = get_color_of_mana_produced_by_id(get_id(player, card), COLOR_TEST_ANY_COLORED, player);
	}

	if (instance->mana_color == 0){	// Can't produce mana, but can still tap for 0
		if (event == EVENT_CAN_ACTIVATE && player == AI){
			return 0;
		}
		if (event == EVENT_ACTIVATE){
			if (can_produce_mana(player, card)){
				produce_mana_tapped(player, card, COLOR_COLORLESS, 0);
				return 0;
			}
		}
		if (event == EVENT_COUNT_MANA){
			return 0;
		}
	}

	return mana_producer(player, card, event);
}

int is_monolith(int id){
	if( id == CARD_ID_MANA_VAULT ||
		id == CARD_ID_GRIM_MONOLITH ||
		id == CARD_ID_BASALT_MONOLITH
	  ){
		return 1;
	}
	return 0;
}

int card_power_artifact(int player, int card, event_t event){
  /* CARD_ID_POWER_ARTIFACT	517
   * Power Artifact	|U|U
   * Enchantment - Aura
   * Enchant artifact
   * Enchanted artifact's activated abilities cost |2 less to activate.
   * This effect can't reduce the amount of mana an ability costs to activate to less than one mana. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( in_play(player, card) && instance->targets[0].player != -1 ){
		if( leaves_play(player, card, event, RESOLVE_TRIGGER_MANDATORY, 1) ){
			remove_cost_mod_for_activated_abilities(instance->targets[0].player, instance->targets[0].card, 2, 0);
		}
	}
	if( event == EVENT_CAN_CAST ){
		if( player != AI ){
			return can_target(&td);
		}
		else{
			int count = 0;
			while( count < active_cards_count[player] ){
					if( in_play(player, count) && is_monolith(get_id(player, count)) ){
						instance->targets[0].player = player;
						instance->targets[0].card = count;
						instance->number_of_targets = 1;
						if( valid_target(&td) ){
							return 1;
						}
					}
					count++;
			}
		}
	}
	
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player != AI ){
			pick_target(&td, "TARGET_ARTIFACT");
		}
		else{
			instance->targets[0].player = -1;
			instance->targets[0].card = -1;
			instance->number_of_targets = 0;
			int count = 0;
			while( count < active_cards_count[player] ){
					if( in_play(player, count) && is_monolith(get_id(player, count)) ){
						instance->targets[0].player = player;
						instance->targets[0].card = count;
						instance->number_of_targets = 1;
						if( valid_target(&td) ){
							ai_modifier+=100;
							break;
						}
					}
					count++;
			}
			if( instance->targets[0].player == -1 ){
				spell_fizzled = 1;
			}
		}
	}
	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			instance->damage_target_player = instance->targets[0].player;
			instance->damage_target_card = instance->targets[0].card;
			set_cost_mod_for_activated_abilities(instance->targets[0].player, instance->targets[0].card, 2, 0);
		}
		else{
			kill_card(player, card, KILL_SACRIFICE);
		}
	}
	

	return 0;
}

int card_black_lotus(int player, int card, event_t event){
  /* CARD_ID_BLACK_LOTUS	17
   * Black Lotus	|0
   * Artifact
   * |T, Sacrifice ~: Add three mana of any one color to your mana pool. */

	//0x420020
	return artifact_mana_all_one_color(player, card, event, 3, 1);
}

int card_sol_ring(int player, int card, event_t event){
  /* Sol Ring	|1
   * Artifact
   * |T: Add |2 to your mana pool. */

	//0x424b30
	if (event == EVENT_CAST_SPELL){
		if (affect_me(player, card)){
			ai_modifier += 192 / (landsofcolor_controlled[1][COLOR_ANY] + 1);
		}
	} else if (event == EVENT_CAN_ACTIVATE){
		return !is_tapped(player, card) && !is_animated_and_sick(player, card) && can_produce_mana(player, card);
	} else if (event == EVENT_ACTIVATE){
		ai_modifier -= 12;
		produce_mana_tapped(player, card, COLOR_COLORLESS, 2);
    } else if (event == EVENT_COUNT_MANA){
		if (affect_me(player, card) && !is_tapped(player, card) && !is_animated_and_sick(player, card) && can_produce_mana(player, card)){
			declare_mana_available(player, COLOR_COLORLESS, 2);
		}
	}
	return 0;
}

int card_mana_flare(int player, int card, event_t event){
  /* CARD_ID_MANA_FLARE	155
   * Mana Flare	|2|R
   * Enchantment
   * Whenever a player taps a land for mana, that player adds one mana to his or her mana pool of any type that land produced. */

	//0x4B8940; also code for Heartbeat of Spring, Zhur-Taa Ancient

	switch (event){
		case EVENT_TAP_CARD:
			if (!is_what(affected_card_controller, affected_card, TYPE_LAND)){
				return 0;
			}

			if (!in_play(player, card)){	// Seems unnecessary, but it's explicitly checked in the exe version, and is harmless
				return 0;
			}

			produce_mana_of_any_type_tapped_for(player, card, 1);

			return 0;

		case EVENT_COUNT_MANA:
			if (!is_what(affected_card_controller, affected_card, TYPE_LAND)){
				return 0;
			}

			if (!in_play(player, card)){	// Seems unnecessary, but it's explicitly checked in the exe version, and is harmless
				return 0;
			}

			// A check for EA_MANA_SOURCE is conspicuous by its absence in the exe version.
			if (is_tapped(affected_card_controller, affected_card) || is_animated_and_sick(affected_card_controller, affected_card)
				|| !can_produce_mana(affected_card_controller, affected_card) ){
				return 0;
			}

			int num_colors = 0;
			color_t col;
			card_instance_t* aff_instance = get_card_instance(affected_card_controller, affected_card);
			for (col = COLOR_COLORLESS; col <= COLOR_ARTIFACT; ++col){
				if (aff_instance->card_color & (1 << col)){
					++num_colors;
				}
			}

			if (num_colors > 0){
				declare_mana_available_hex(affected_card_controller, aff_instance->card_color, 1);
			} else {
				declare_mana_available(affected_card_controller, single_color_test_bit_to_color(aff_instance->card_color), 1);
			}

			return 0;

		case EVENT_CAN_CAST:
			return 1;

		case EVENT_CAST_SPELL:
			if (affect_me(player, card)){
				ai_modifier += 48;
			}
			return 0;

		default:
			return 0;
	}
}

int card_font_of_mythos(int player, int card, event_t event)
{
  /* CARD_ID_FONT_OF_MYTHOS	1599
   * Font of Mythos	|4
   * Artifact
   * At the beginning of each player's draw step, that player draws two additional cards. */

  if (event == EVENT_DRAW_PHASE )
    event_result+=2;
  return 0;
}

int card_dragon_engine(int player, int card, event_t event){
  /* CARD_ID_DRAGON_ENGINE	492
   * Dragon Engine	|3
   * Artifact Creature - Construct 1/3
   * |2: ~ gets +1/+0 until end of turn. */

	return generic_shade(player, card, event, 0, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0);
}

int card_city_of_brass(int player, int card, event_t event){
  /* CARD_ID_CITY_OF_BRASS	403
   * City of Brass	""
   * Land
   * Whenever ~ becomes tapped, it deals 1 damage to you.
   * |T: Add one mana of any color to your mana pool. */

	if (event == EVENT_TAP_CARD && affect_me(player, card)){
		damage_player(player, 1, player, card);
		return 0;
	} else {
		return mana_producer(player, card, event);
	}
}

int card_generic_noncombat_1_mana_producing_creature(int player, int card, event_t event){
	// Birds of Paradise, Copper Myr, Gold Myr, Iron Myr, Leaden Myr, Silver Myr
	if (event == EVENT_CAN_CAST){
		return 1;	// so it can be assigned to a creature with flash
	}

	int colors = get_card_instance(player, card)->mana_color & COLOR_TEST_ANY;
	if (!colors){
		return 0;
	} else if ((colors & (colors - 1)) == 0){	// exactly one bit set
		return mana_producing_creature(player, card, event, 24, single_color_test_bit_to_color(colors), 1);
	} else {
		return mana_producing_creature_all_one_color(player, card, event, 24, colors, 1);
	}
}

// Just very thin wrappers for now
int card_bazaar_of_baghdad(int player, int card, event_t event){
  /* CARD_ID_BAZAAR_OF_BAGHDAD	398
   * Bazaar of Baghdad	""
   * Land
   * |T: Draw two cards, then discard three cards. */

	if (event == EVENT_RESOLVE_SPELL){
		play_land_sound_effect(player, card);
	}
	return ((int (*)(int, int, event_t))(0x4A8520))(player, card, event);
}

int card_elephant_graveyard(int player, int card, event_t event){
  /* CARD_ID_ELEPHANT_GRAVEYARD	414
   * Elephant Graveyard	""
   * Land
   * |T: Add |C to your mana pool.
   * |T: Regenerate target Elephant. */

	if (event == EVENT_RESOLVE_SPELL){
		play_land_sound_effect(player, card);
	}
	return ((int (*)(int, int, event_t))(0x4A8D40))(player, card, event);
}

int card_island_of_wak_wak(int player, int card, event_t event){
  /* CARD_ID_ISLAND_OF_WAK_WAK	428
   * Island of Wak-Wak	""
   * Land
   * |T: Target creature with flying has base power 0 until end of turn. */

	if (event == EVENT_RESOLVE_SPELL){
		play_land_sound_effect(player, card);
	}
	return ((int (*)(int, int, event_t))(0x4A8F70))(player, card, event);
}

int card_oasis(int player, int card, event_t event){
  /* CARD_ID_OASIS	445
   * Oasis	""
   * Land
   * |T: Prevent the next 1 damage that would be dealt to target creature this turn. */

	if (event == EVENT_RESOLVE_SPELL){
		play_land_sound_effect(player, card);
	}
	return ((int (*)(int, int, event_t))(0x47A010))(player, card, event);
}

static int ai_choice_for_arena(int player, int card, target_t* ret_tgt){
	card_instance_t* instance;
	int c, best_rating = -1;
	uint32_t prots = get_protections_from(player, card);

	ret_tgt->player = ret_tgt->card = -1;
	for (c = 0; c < active_cards_count[AI]; ++c){
		if (in_play(AI, c)
			&& cards_data[(instance = get_card_instance(AI, c))->internal_card_id].type & TYPE_CREATURE
			&& instance->kill_code != KILL_SACRIFICE	// How would it even get here?
			&& !(instance->state & STATE_CANNOT_TARGET)){
			/* Straight out of the exe.  There's definitely room for improvement here.
			 * For starters, if the human player chose first, the chosen creature should be taken into account.  This will always pick a
			 *    vanilla 4/4 creature over either a 6/1 or a 1/6, even when fighting a known 5/5.
			 * Most abilities, as well as EA_MANA_SOURCE and EA_ACT_ABILITY, should make the AI less likely to pick a given creature.
			 * Protections are an exception.  Especially if the human player's picked and the candidate has protection from that creature.
			 * Base card value should also be looked at. */
			uint32_t abils = get_abilities(AI, c, EVENT_ABILITIES, -1);
			if (abils & prots){
				continue;
			}
			int rating = (get_abilities(AI, c, EVENT_POWER, -1)
						  + get_abilities(AI, c, EVENT_TOUGHNESS, -1)
						  + num_bits_set(abils)
						  + num_bits_set(abils & (KEYWORD_FLYING | KEYWORD_FIRST_STRIKE))
						  + num_bits_set(cards_data[instance->internal_card_id].extra_ability & (EA_MANA_SOURCE | EA_ACT_ABILITY)));

			if (best_rating < rating){
				best_rating = rating;
				ret_tgt->player = AI;
				ret_tgt->card = c;
			}
		}
	}

  return best_rating >= 0;
}

int card_arena(int player, int card, event_t event){
  /* Arena	""
   * Land
   * |3, |T: Tap target creature you control and target creature of an opponent's choice he or she controls. Those creatures fight each other. */

	if (event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI){
		ai_modifier += 48;
		return 0;
	}

	if (event == EVENT_RESOLVE_SPELL){
		play_land_sound_effect(player, card);
		return 0;
	}

	if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION){
		return 0;
	}

	card_instance_t* instance = get_card_instance(player, card);

	target_definition_t td_me, td_opp;

	default_target_definition(player, card, &td_me, TYPE_CREATURE);
	td_me.allowed_controller = td_me.preferred_controller = player;

	default_target_definition(player, card, &td_opp, TYPE_CREATURE);
	td_opp.allowed_controller = td_opp.preferred_controller = td_opp.who_chooses = 1 - player;
	td_opp.allow_cancel = 0;

	if (event == EVENT_CAN_ACTIVATE){
		return (!is_tapped(player, card) && !is_animated_and_sick(player, card) && !paying_mana()
				&& !get_forbid_activation_flag(player, card) && has_mana_for_activated_ability(player, card, MANACOST_X(3))
				&& can_target(&td_me) && can_target(&td_opp));
	}

	if (event == EVENT_ACTIVATE){
		if (paying_mana()){
			cancel = 1;
			return 0;
		}

		charge_mana_for_activated_ability(player, card, MANACOST_X(3));

		if (cancel != 1){
			instance->number_of_targets = 0;

			if (player == HUMAN){
				if (!pick_target(&td_me, "ARENA")){
					cancel = 1;
					return 0;
				}

				instance->state |= STATE_TAPPED;
				if (trace_mode & 2){
					((int (*)(int, int))(0x4720E0))(player, 0);	// networking
					new_pick_target(&td_opp, "ARENA", 1, 0);
				} else {
					if (!ai_choice_for_arena(player, card, &instance->targets[1])){
						// shouldn't be possible currently, but in case validate_target()'s definition drifts
						new_pick_target(&td_opp, "ARENA", 1, 0);
					}
					instance->number_of_targets = 2;
				}
				do_dialog(player, player, card, instance->targets[1].player, instance->targets[1].card, text_lines[1] /*"Selected for Arena"*/, 0);
			} else {	// player == AI
				if (trace_mode & 2){
					cancel = pick_target(&td_me, "ARENA") ? 0 : 1;
				} else {
					if (ai_choice_for_arena(player, card, &instance->targets[0])){
						instance->number_of_targets = 1;
						cancel = 0;
					} else {
						// shouldn't be possible currently, but in case validate_target()'s definition drifts
						cancel = pick_target(&td_opp, "ARENA") ? 0 : 1;
					}
				}

				if (cancel != 1){
					instance->state |= STATE_TAPPED;

					do_dialog(player, player, card, instance->targets[0].player, instance->targets[0].card, text_lines[1] /*"Selected for Arena"*/, 0);
					new_pick_target(&td_opp, "ARENA", 1, 0);

					if (trace_mode & 2){
						((int (*)(int, int))(0x4720E0))(1 - player, 0);	// networking
					}
				}
			}

			if (cancel == 1){
				instance->number_of_targets = 0;
			}
		}

		return 0;
	}

	if (event == EVENT_RESOLVE_ACTIVATION){
		int target_0_valid = validate_target(player, card, &td_me, 0);
		int target_1_valid = validate_target(player, card, &td_opp, 1);

		if (target_0_valid && target_1_valid){
			fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player, instance->targets[1].card);
			if (in_play(instance->targets[0].player, instance->targets[0].card)){
				tap_card(instance->targets[0].player, instance->targets[0].card);
			}
			if (in_play(instance->targets[1].player, instance->targets[1].card)){
				tap_card(instance->targets[1].player, instance->targets[1].card);
			}
		}
		/* Otherwise, if one target one was valid, the exe version still tapped it and did damage to it.  This contradicts ruling 9/25/2006: "If either target
		 * is illegal at the time the ability resolves (whether because it has left the battlefield, because it has gained Shroud or Protection, or for any
		 * other reason), the ability taps the remaining target, but no damage is dealt. If both targets are illegal, the ability is countered." */
		else if (target_0_valid){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		} else if (target_1_valid){
			tap_card(instance->targets[1].player, instance->targets[1].card);
		} else {
			spell_fizzled = 1;
		}

		get_card_instance(instance->parent_controller, instance->parent_card)->number_of_targets = 0;
		return 0;
	}

	return 0;
}

int card_el_hajjaj(int player, int card, event_t event){
  /* CARD_ID_EL_HAJJAJ	413
   * El-Hajjaj	|1|B|B
   * Creature - Human Wizard 1/1
   * Whenever ~ deals damage, you gain that much life. */

	// 4C8070
	spirit_link_effect(player, card, event, player);
	return 0;
}

int card_two_headed_giant_of_foriys(int player, int card, event_t event)
{
  /* CARD_ID_TWO_HEADED_GIANT_OF_FORIYS	257
   * Two-Headed Giant of Foriys	|4|R
   * Creature - Giant 4/4
   * Trample
   * ~ can block an additional creature each combat. */

	// 4559F0
	creature_can_block_additional(player, card, event, 1);
	return 0;
}
