#include "manalink.h"

// Functions
static int graft_trigger(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me( player, card ) && reason_for_trigger_controller == player &&
		count_1_1_counters(player, card) > 0
	  ){
		int trig_mode = player == AI ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;
		if( player == AI ){
			if( (is_what(player, card, TYPE_CREATURE) && get_toughness(player, card) <= 1) || trigger_cause_controller != player ){
				trig_mode = 0;
			}
		}
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;

		etb_ability_global(ANYBODY, event, trig_mode, 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		move_counters(trigger_cause_controller, trigger_cause, player, card, COUNTER_P1_P1, 1);
	}

	return 0;
}

int graft(int player, int card, event_t event, int amount){

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, amount);

	graft_trigger(player, card, event);

	return 0;
}

void protection_from_multicolored(int player, int card, event_t event){

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_BLOCK_LEGALITY ){
			if( player == attacking_card_controller && card == attacking_card ){
				if( count_colors(affected_card_controller, affected_card) > 1 ){
					event_result =  1;
				}
			}
		}
		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card && damage->damage_target_player == player &&
				damage->damage_target_card == card && damage->info_slot > 0 &&
				count_colors(damage->damage_source_player, damage->damage_source_card) > 1 )
			{
				int amount = damage->info_slot;
				prevent_damage(affected_card_controller, affected_card, amount);
			}
		}
	}
}

const char* is_multicolored(int who_chooses, int player, int card){
	if( count_colors(player, card) > 1 ){
		return NULL;
	}
	return gs_target_error_color;	// ",color"
}


// Cards
#if 1
int card_aethermages_touch(int player, int card, event_t event){return 0;}
#else
static int aethermages_touch_legacy(int player, int card, event_t event ){
	card_instance_t *instance = get_card_instance(player, card);
	int p = instance->damage_target_player;
	int c = instance->damage_target_card;
	if( eot_trigger(player, card, event) ){
		if( p > -1 && ! check_state(p, c, STATE_OUBLIETTED) ){
			bounce_permanent(p, c);
		}
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_aethermages_touch(int player, int card, event_t event){
/* CARD_ID_AETHERMAGES_TOUCH	8167
Aethermage's Touch	|2|W|U
Instant
Reveal the top four cards of your library. You may put a creature card from among them onto the battlefield.
It gains "At the beginning of your end step, return this creature to its owner's hand."
Then put the rest of the cards revealed this way on the bottom of your library in any order. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card");
		int result = reveal_top_cards_of_library_and_choose(player, card, player, 4, 0, TUTOR_PLAY, 1, TUTOR_BOTTOM_OF_DECK, 0, &this_test);
		if( result > -1 ){
			create_targetted_legacy_effect(player, card, &aethermages_touch_legacy, player, result);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}
#endif

int card_anthem_of_rakdos(int player, int card, event_t event){
	/* Anthem of Rakdos	|2|B|R|R	0x2006cd1
	 * Enchantment
	 * Whenever a creature you control attacks, it gets +2/+0 until end of turn and ~ deals 1 damage to you.
	 * Hellbent - As long as you have no cards in hand, if a source you control would deal damage to a permanent or player, it deals double that damage to that permanent or player instead. */
	if( ! is_humiliated(player, card) ){
	  // Whenever a creature you control attacks, it gets +2/+0 until end of turn and ~ deals 1 damage to you.
	  int amt;
	  if ((amt = declare_attackers_trigger(player, card, event, DAT_TRACK, player, -1)))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
		  for (--amt; amt >= 0; --amt)
			{
			  if (in_play(current_turn, attackers[amt]))
				pump_until_eot(player, card, current_turn, attackers[amt], 2, 0);

			  damage_player(player, 1, player, card);
			}
		}

	  /* Hellbent - As long as you have no cards in hand, if a source you control would deal damage to a creature or player, it deals double that damage to that
	   * creature or player instead. */
	  card_instance_t* damage = damage_being_dealt(event);
	  if (damage
		  && damage->damage_source_player == player
		  && hand_count[player] <= 0)
		damage->info_slot *= 2;
	}

	return global_enchantment(player, card, event);
}

int card_aquastrand_spider(int player, int card, event_t event){

	/* Aquastrand Spider	|1|G
	 * Creature - Spider Mutant 0/0
	 * Graft 2
	 * |G: Target creature with a +1/+1 counter on it gains reach until end of turn. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);

	graft_trigger(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_P1_P1;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_REACH, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_G(1), 0,
									&td, "Select target creature with a +1/+1 counter.");
}

int card_avatar_of_discord(int player, int card, event_t event){
/* CARD_ID_AVATAR_OF_DISCORD	8169
Avatar of Discord	|BR|BR|BR
Creature - Avatar 5/3
(|BR can be paid with either |B or |R.)
Flying
When ~ enters the battlefield, sacrifice it unless you discard two cards. */

	hybrid(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int kill_me = 1;
		if( hand_count[player] > 1 ){
			int p_hand[2][hand_count[player]];
			int pc = 0;
			int discarded[2];
			int dc = 0;
			int count = 0;
			while( count < active_cards_count[player] ){
					if( in_hand(player, count) ){
						p_hand[0][pc] = get_original_internal_card_id(player, count);
						p_hand[1][pc] = count;
						pc++;
					}
					count++;
			}
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to discard.");
			while( dc < 2 ){
					int selected = select_card_from_zone(player, player, p_hand[0], pc, 0, AI_MIN_VALUE, -1, &this_test);
					if( selected != -1 ){
						discarded[dc] = p_hand[1][selected];
						dc++;
						for(count = selected; count < pc; count++){
							p_hand[0][count] = p_hand[0][count+1];
							p_hand[1][count] = p_hand[1][count+1];
							pc--;
						}
					}
					else{
						break;
					}
			}
			if( dc == 2 ){
				discard_card(player, discarded[0]);
				discard_card(player, discarded[1]);
				kill_me = 0;
			}
		}
		if( kill_me ){
			kill_card(player, card, KILL_SACRIFICE );
		}
	}

	return 0;
}

int card_azorius_guildmage(int player, int card, event_t event){
/* CARD_ID_AZORIUS_GUILDMAGE	8172
Azorius Guildmage	|WU|WU
Creature - Vedalken Wizard 2/2
|2|W: Tap target creature.
|2|U: Counter target activated ability. (Mana abilities can't be targeted.) */

	hybrid(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t* instance = get_card_instance( player, card );

	int can_counter =  has_mana_for_activated_ability(player, card, MANACOST_XU(2,1)) && can_counter_activated_ability(player, card, event, NULL);

	enum
	{
	  CHOICE_TAP = 1,
	  CHOICE_COUNTER = 2
	} choice = DIALOG(player, card, event,
					  "Tap target creature", has_mana_for_activated_ability(player, card, MANACOST_XW(2,1)) && can_target(&td), 1,
					  "Counter activated ability", can_counter, 2);

	if (event == EVENT_CAN_ACTIVATE)
		return choice ? (can_counter ? 99 : 1) : 0;
	else if (event == EVENT_ACTIVATE)
	{
		instance->number_of_targets = 0;
		switch (choice)
		  {
			case CHOICE_TAP:
			  if (charge_mana_for_activated_ability(player, card, MANACOST_XW(2,1)))
				pick_target(&td, "TARGET_CREATURE");
			  break;
			case CHOICE_COUNTER:
			  if (charge_mana_for_activated_ability(player, card, MANACOST_XU(2,1)))
				cast_counter_activated_ability(player, card, 0);
			  break;
		  }
	}
	else{	// event == EVENT_RESOLVE_ACTIVATION
	  switch (choice)
		{
		  case CHOICE_TAP:
			if (valid_target(&td))
			  tap_card(instance->targets[0].player, instance->targets[0].card);
			break;
		  case CHOICE_COUNTER:
			resolve_counter_activated_ability(player, card, NULL, 0);
			break;
		}
	}
	return 0;
}

int card_azorius_herald(int player, int card, event_t event){
/* CARD_ID_AZORIUS_HERALD	8173
Azorius Herald	|2|W
Creature - Spirit 2/1
~ can't be blocked.
When ~ enters the battlefield, you gain 4 life.
When ~ enters the battlefield, sacrifice it unless |U was spent to cast it. */

	ravnica_manachanger(player, card, event, 0, 1, 0, 0, 0);

	unblockable(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && get_card_instance( player, card )->info_slot == 1){
			get_card_instance( player, card )->targets[1].player = charge_changed_mana(player, card, 0, 1, 0, 0, 0);
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 4);
		if( get_card_instance( player, card )->targets[1].player != 2 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_azorius_ploy(int player, int card, event_t event){
/* CARD_ID_AZORIUS_PLOY	8174
Azorius Ploy	|1|W|W|U
Instant
Prevent all combat damage target creature would deal this turn.
Prevent all combat damage that would be dealt to target creature this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			maze_of_ith_effect(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_beacon_hawk(int player, int card, event_t event){
/* CARD_ID_BEACON_HAWK	8175
Beacon Hawk	|1|W
Creature - Bird 1/1
Flying
Whenever ~ deals combat damage to a player, you may untap target creature.
|W: ~ gets +0/+1 until end of turn. */

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE+DDBM_MUST_DAMAGE_OPPONENT) ){
		card_instance_t *instance = get_card_instance( player, card );

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return generic_shade(player, card, event, 0, MANACOST_W(1), 0, 1, 0, 0);
}

int card_biomantic_mastery(int player, int card, event_t event){
/*
Biomantic Mastery 4{G/U}{G/U}{G/U}
Sorcery
Draw a card for each creature target player controls, then draw a card for each creature another target player controls.
*/
	hybrid(player, card, event);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( target_available(player, card, &td) > 1 ){
			return 1;
		}
	}
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_PLAYER") ){
			if( instance->targets[0].player == player ){
				instance->targets[1].player = 1-player;
				instance->targets[1].card = -1;
				instance->number_of_targets = 2;
			}
			else{
				instance->targets[1].player = player;
				instance->targets[1].card = -1;
				instance->number_of_targets = 2;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			draw_cards(player, count_subtype(instance->targets[0].player, TYPE_CREATURE, -1));
		}
		if( validate_target(player, card, &td, 1) ){
			draw_cards(player, count_subtype(instance->targets[1].player, TYPE_CREATURE, -1));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_blessing_of_the_nephilim(int player, int card, event_t event){
/* CARD_ID_BLESSING_OF_THE_NEPHILIM	8177
Blessing of the Nephilim	|W
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+1 for each of its colors. */

	card_instance_t *instance= get_card_instance(player, card);
	int amount = 0;
	if( in_play(player, card) && instance->damage_target_player != -1 ){
		amount = count_colors(instance->damage_target_player, instance->damage_target_card);
	}
	return generic_aura(player, card, event, player, amount, amount, 0, 0, 0, 0, 0);
}

int card_bond_of_agony(int player, int card, event_t event){
/* CARD_ID_BOND_OF_AGONY	8178
Bond of Agony	|X|B
Sorcery
As an additional cost to cast ~, pay X life.
Each other player loses X life. */

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL) ){
			return can_pay_life_as_cost_for_spells_or_activated_abilities(player, 1);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance= get_card_instance(player, card);
		instance->info_slot = 0;
		set_x_for_x_spells(player, card, event, x_value);
		int amount = instance->info_slot;
		if( can_pay_life(player, amount) ){
			lose_life(player, amount);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		lose_life(1-player, get_card_instance(player, card)->info_slot);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int determined_effect(int player, int card, event_t event){
	type_uncounterable(player, card, event, player, 0, NULL);
	if( eot_trigger(player, card, event) ){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_bound_determined(int player, int card, event_t event){
	/*
	  Bound |3|B|G
	  Instant
	  Sacrifice a creature. Return up to X cards from your graveyard to your hand, where X is the number of colors that creature was. Exile this card.

	  Determined |U|G
	  Instant
	  Other spells you control can't be countered by spells or abilities this turn.
	  Draw a card.
	*/

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_MODIFY_COST ){
		int can_play_bound = can_sacrifice_as_cost(player, 1, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0);
		int can_play_determined = card_on_stack_controller != -1 ? (player == HUMAN ? 99 : 1) : (card_on_stack_controller == player ? 99 : 0);

		generic_split_card(player, card, event, can_play_bound, 1, MANACOST_UG(1, 1), can_play_determined, 1, 0, NULL, NULL);
	}

	if( event == EVENT_PLAY_FIRST_HALF ){
		if( ! sacrifice(player, card, player, 0, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0) ){
			spell_fizzled = 1;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	int can_play_bound = can_sacrifice_as_cost(player, 1, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0);
	int can_play_determined = card_on_stack_controller != -1 ? (player == HUMAN ? 99 : (card_on_stack_controller == player ? 99 : 0)) : 1;

	if(event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 1) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card");
			int cd = count_graveyard(player);
			int amount = instance->targets[1].player;
			int i;
			for(i=0; i<amount; i++){
				if( amount > 1 ){
					scnprintf(test.message, 100, "Select a card (%d of %d)", i+1, amount);
				}
				if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &test) ){
					cd--;
				}
			}
		}
		if( (instance->info_slot & 2) ){
			int i;
			for(i=0; i<active_cards_count[player]; i++){
				if( ! in_play(player, i) && check_state(player, i, STATE_INVISIBLE) ){
					add_state(player, i, STATE_CANNOT_TARGET);
				}
			}
			create_legacy_effect(player, card, &determined_effect);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, can_play_bound, 10, MANACOST_UG(1, 1), can_play_determined, 5, 0, "Bound", "Determined");
}

int card_brain_pry(int player, int card, event_t event){
/*
Brain Pry |1|B
Sorcery
Name a nonland card. Target player reveals his or her hand. That player discards a card with that name. If he or she can't, you draw a card.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	if(event == EVENT_RESOLVE_SPELL ){
		int csvid = -1;
		if( player == HUMAN ){
			csvid = card_from_list(player, 3, TYPE_LAND, DOESNT_MATCH, 0, 0, 0, 0, 0, 0, -1, 0);
		}
		else{
			int rnd = internal_rand(count_deck(instance->targets[0].player));
			int rounds = 0;
			while( rounds < 2 ){
					int count = rnd;
					while( deck_ptr[instance->targets[0].player][count] != -1 ){
							if( ! is_what(-1, deck_ptr[instance->targets[0].player][count], TYPE_LAND) ){
								csvid = cards_data[deck_ptr[instance->targets[0].player][count]].id;
								rounds = 2;
								break;
							}
					}
					rnd = 0;
					rounds++;
			}
			if( csvid == -1 ){
				csvid = CARD_ID_LIGHTNING_BOLT;
			}
		}
		reveal_target_player_hand(instance->targets[0].player);
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[instance->targets[0].player]; c++){
			if( in_hand(instance->targets[0].player, c) && get_id(instance->targets[0].player, c) == csvid ){
				found = 1;
				new_discard_card(instance->targets[0].player, c, 0, player);
			}
		}
		if( ! found ){
			draw_cards(player, 1);
		}

		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_bronze_bombshell(int player, int card, event_t event)
{
  /* Bronze Bombshell	|4
   * Artifact Creature - Construct 4/1
   * When a player other than ~'s owner controls it, that player sacrifices it. If the player does, ~ deals 7 damage to him or her. */

	if (event == EVENT_STATIC_EFFECTS && is_stolen(player, card) && ! is_humiliated(player, card) )
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->info_slot == CARD_ID_BRONZE_BOMBSHELL)	// Prevent reentrance
		return 0;
	  instance->info_slot = CARD_ID_BRONZE_BOMBSHELL;
	  damage_player(player, 7, player, card);
	  kill_card(player, card, KILL_SACRIFICE);
	}

	return 0;
}

int card_celestial_ancient(int player, int card, event_t event){
	/* Celestial Ancient	|3|W|W
	 * Creature - Elemental 3/3
	 * Flying
	 * Whenever you cast an enchantment spell, put a +1/+1 counter on each creature you control. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ENCHANTMENT);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			manipulate_type(player, card, player, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));
		}
	}
	return 0;
}

int card_coiling_oracle(int player, int card, event_t event){
/*
Coiling Oracle |U|G
Creature — Snake Elf Druid 1/1
When Coiling Oracle enters the battlefield, reveal the top card of your library.
If it's a land card, put it onto the battlefield. Otherwise, put that card into your hand.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( count_deck(player) > 0 ){
			int *deck = deck_ptr[player];
			int card_added = add_card_to_hand(player, deck[0] );
			reveal_card(player, card, player, card_added);
			card_data_t* card_d = get_card_data(player, card_added);
			if ( card_d->type & TYPE_LAND ){
				put_into_play(player, card_added);
			}
			remove_card_from_deck( player, 0 );
		}
	}
	return 0;
}

int card_condemn(int player, int card, event_t event){
/* CARD_ID_CONDEMN	8187
Condemn	|W
Instant
Put target attacking creature on the bottom of its owner's library. Its controller gains life equal to its toughness. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			gain_life( instance->targets[0].player, get_toughness(instance->targets[0].player, instance->targets[0].card) );
			put_on_bottom_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_court_hussar(int player, int card, event_t event){
/* CARD_ID_COURT_HUSSAR	8188
Court Hussar	|2|U
Creature - Vedalken Knight 1/3
Vigilance
When ~ enters the battlefield, look at the top three cards of your library, then put one of them into your hand and the rest on the bottom of your library in any order.
When ~ enters the battlefield, sacrifice it unless |W was spent to cast it. */

	ravnica_manachanger(player, card, event, 0, 0, 0, 0, 1);

	vigilance(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && get_card_instance( player, card )->info_slot == 1){
			get_card_instance( player, card )->targets[1].player = charge_changed_mana(player, card, 0, 0, 0, 0, 1);
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		select_one_and_put_the_rest_on_bottom(player, player, 3);
		if( get_card_instance( player, card )->targets[1].player != 2 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_crime_punishment(int player, int card, event_t event){
	/*
	  Crime |3|W|B
	  Sorcery
	  Put target creature or enchantment card from an opponent's graveyard onto the battlefield under your control.

	  Punishment |X|B|G
	  Sorcery
	  Destroy each artifact, creature, and enchantment with converted mana cost X.
	*/
	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_MODIFY_COST ){
		int can_play_crime = count_graveyard_by_type(1-player, TYPE_CREATURE | TYPE_ENCHANTMENT) && ! graveyard_has_shroud(1-player);
		generic_split_card(player, card, event, can_play_crime, 1, MANACOST_BG(1, 1), 1, 1, 0, NULL, NULL);
	}

	if( event == EVENT_PLAY_FIRST_HALF ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE | TYPE_ENCHANTMENT, "Select a creature or enchantment card");
		if( select_target_from_grave_source(player, card, player, 0, AI_MAX_CMC, &this_test, 0) == -1 ){
			spell_fizzled = 1;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	int can_play_crime = count_graveyard_by_type(1-player, TYPE_CREATURE | TYPE_ENCHANTMENT) && ! graveyard_has_shroud(1-player);
	int priority_crime = 10;
	int priority_punishment = 5;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( (instance->info_slot & (1<<8)) && player == AI ){
			int p_cmc = get_average_cmc(player);
			int opp_cmc = get_average_cmc(1-player);
			if( p_cmc != opp_cmc && has_mana_multi(player, MANACOST_XBG(opp_cmc, 1, 1)) ){
				priority_punishment = 15;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( 1 ){
			int selected = validate_target_from_grave_source(player, card, 1-player, 0);
			if( selected != -1 ){
				reanimate_permanent(player, card, 1-player, selected, REANIMATE_DEFAULT);
			}
		}
		if( instance->info_slot & 2 ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_ENCHANTMENT);
			this_test.cmc = instance->targets[1].card;
			new_manipulate_all(player, card, ANYBODY, &this_test, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, can_play_crime, priority_crime, MANACOST_XBG(-1, 1, 1), 1, priority_punishment, 0, "Crime", "Punishment");
}

int card_crypt_champion(int player, int card, event_t event){
/* CARD_ID_CRYPT_CHAMPION	8190
Crypt Champion	|3|B
Creature - Zombie 2/2
Double strike
When ~ enters the battlefield, each player puts a creature card with converted mana cost 3 or less from his or her graveyard onto the battlefield.
When ~ enters the battlefield, sacrifice it unless |R was spent to cast it. */

	ravnica_manachanger(player, card, event, 0, 0, 0, 1, 0);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && get_card_instance( player, card )->info_slot == 1){
			get_card_instance( player, card )->targets[1].player = charge_changed_mana(player, card, 0, 0, 0, 1, 0);
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card with CMC 3 or less.");
		this_test.cmc = 4;
		this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		APNAP(p,{
					if( new_special_count_grave(p, &this_test) > 0 ){
						new_global_tutor(p, p, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_VALUE, &this_test);
					}
				}
		);

		if( get_card_instance( player, card )->targets[1].player != 2 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_cytoplast_manipulator(int player, int card, event_t event){

	/* Cytoplast Manipulator	|2|U|U
	 * Creature - Human Wizard Mutant 0/0
	 * Graft 2
	 * |U, |T: Gain control of target creature with a +1/+1 counter on it for as long as ~ remains on the battlefield. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);

	graft_trigger(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_P1_P1;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			gain_control_until_source_is_in_play_and_tapped(instance->parent_controller, instance->parent_card,
															instance->targets[0].player, instance->targets[0].card, GCUS_CONTROLLED);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_U(1), 0,
									&td, "Select target creature with a +1/+1 counter.");
}


int card_cytoplast_root_kin(int player, int card, event_t event)
{
  /* Cytoplast Root-Kin	|2|G|G
   * Creature - Elemental Mutant 0/0
   * Graft 4
   * When ~ enters the battlefield, put a +1/+1 counter on each other creature you control that has a +1/+1 counter on it.
   * |2: Move a +1/+1 counter from target creature you control onto ~. */

	graft(player, card, event, 4);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int c;
		for (c = 0; c < active_cards_count[player]; ++c){
			if (c != card && in_play(player, c) && is_what(player, c, TYPE_CREATURE) && count_1_1_counters(player, c) > 0){
				add_1_1_counter(player, c);
			}
		}
	}

	if( !IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	base_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	// It's legal to target itself, or a creature without a +1/+1 counter, but the effect does nothing in that case.
	if (IS_AI(player))
	{
		td.special = TARGET_SPECIAL_REQUIRES_COUNTER | TARGET_SPECIAL_NOT_ME;
		td.extra = COUNTER_P1_P1;
	}

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int pp = instance->parent_controller, pc = instance->parent_card;
	  if (in_play(pp, pc))
		move_counters(pp, pc, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(2), 0,
									&td, "Select target creature you control.");
}

int card_cytoshape(int player, int card, event_t event){

	/* Cytoshape	|1|G|U
	 * Instant
	 * Choose a nonlegendary creature on the battlefield. Target creature becomes a copy of that creature until end of turn. */

	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	base_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_LEGEND;
	td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td) && can_target(&td1);
	}
	else if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			instance->number_of_targets = 0;
			if( pick_target(&td, "SELECT_A_NONLEGENDARY_CREATURE") ){
				/* Stash in damage_source_player/card, which will still get updated if the creature changes control before resolution, but won't be considered
				 * by things that with becomes-targeted triggers.  On the minus side, you can't see what was chosen while this is on the stack. */
				instance->damage_source_player = instance->targets[0].player;
				instance->damage_source_card = instance->targets[0].card;

				instance->number_of_targets = 0;
				if (!pick_target(&td1, "TARGET_CREATURE")){
					instance->damage_source_player = instance->damage_source_card = -1;
				}
			}
	}
	else if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td1) ){
				shapeshift_target(player, card,
								  instance->targets[0].player, instance->targets[0].card,
								  instance->damage_source_player, instance->damage_source_card,
								  SHAPESHIFT_UNTIL_EOT);
			}
			kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_cytospawn_shambler(int player, int card, event_t event){

	/* Cytospawn Shambler	|6|G
	 * Creature - Elemental Mutant 0/0
	 * Graft 6
	 * |G: Target creature with a +1/+1 counter on it gains trample until end of turn. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 6);

	graft_trigger(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_P1_P1;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_TRAMPLE, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_G(1), 0,
									&td, "Select target creature with a +1/+1 counter.");
}

int card_delirium_skeins(int player, int card, event_t event){
/* CARD_ID_DELIRIUM_SKEINS	8195
Delirium Skeins	|2|B
Sorcery
Each player discards three cards. */

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p, {new_multidiscard(p, 3, 0, player);};);
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_demand_supply(int player, int card, event_t event){
	/*
	  Supply |X|W|G
	  Sorcery
	  Put X 1/1 green Saproling creature tokens onto the battlefield.

	  Demand |1|W|U
	  Sorcery
	  Search your library for a multicolored card, reveal it, and put it into your hand. Then shuffle your library.
	*/
	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_MODIFY_COST ){
		return generic_split_card(player, card, event, 1, 0, MANACOST_XUW(1, 1, 1), 1, 0, 0, NULL, NULL);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot & 1 ){
			generate_tokens_by_id(player, card, CARD_ID_SAPROLING, instance->targets[1].card);
		}
		if( instance->info_slot & 2 ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			this_test.color_flag = F3_MULTICOLORED;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, 1, 10, MANACOST_XUW(1, 1, 1), 8, 0, 0, "Supply", "Demand");
}

int card_demonfire(int player, int card, event_t event){
	/* Demonfire	|X|R	0x2006d26
	 * Sorcery
	 * ~ deals X damage to any target. If a creature dealt damage this way would die this turn, exile it instead.
	 * Hellbent - If you have no cards in hand, this spell can't be countered and the damage can't be prevented. */
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance( player, card );


	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 ){
			if( hand_count[player] < 1 ){
				state_untargettable(player, card, 1);
				instance->targets[1].player = 66;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		state_untargettable(player, card, 0);
		if( valid_target(&td) ){
			if( instance->targets[1].player == 66 ){
				int legacy = create_legacy_effect(player, card, &my_damage_cannot_be_prevented);
				get_card_instance(player, legacy)->targets[0].player = player;
				get_card_instance(player, legacy)->targets[0].card = card;
			}
			int dc = damage_target0(player, card, instance->info_slot);
			if( instance->targets[0].card != -1 && is_what(instance->targets[0].player, instance->targets[0].card, TYPE_CREATURE) ){
				get_card_instance(player, dc)->targets[3].card |= DMG_EXILE_IF_LETHALLY_DAMAGED_THIS_WAY;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_development_research(int player, int card, event_t event){
	/*
	  Development |3|U|R
	  Instant
	  Put a 3/1 red Elemental creature token onto the battlefield unless any opponent has you draw a card. Repeat this process two more times.

	  Research |U|G
	  Instant
	  Choose up to four cards you own from outside the game and shuffle them into your library.
	*/

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_MODIFY_COST ){
		return generic_split_card(player, card, event, 1, 0, MANACOST_UG(1, 1), 1, 0, 0, NULL, NULL);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot & 1 ){
			int i;
			for(i=0; i<3; i++){
				int ai_choice = 1;
				if( hand_count[1-player] < hand_count[player] ){
					ai_choice = 0;
				}
				int choice = do_dialog(1-player, player, card, -1, -1, " Opponent gets an Elemental\n Opponent draws", ai_choice);
				if( choice == 0 ){
					token_generation_t token;
					default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
					token.pow = 3;
					token.tou = 1;
					token.color_forced = COLOR_TEST_RED;
					generate_token(&token);
				}
				else{
					draw_cards(player, 1);
				}
			}
		}
		if( instance->info_slot & 2 ){
			int i;
			int cd = count_deck(player);
			int *deck = deck_ptr[player];
			for(i=0; i<4; i++){
				int crd = -1;
				if( player != AI ){
					crd = card_from_list(player, 4, TYPE_ANY, 0, 0, 0, 0, 0, 0, 0, -1, 0);
				}
				else{
					if( i == 0 ){
						crd = get_internal_card_id_from_csv_id(CARD_ID_SIMIC_SKY_SWALLOWER);
					}
					if( i == 1 ){
						crd = get_internal_card_id_from_csv_id(CARD_ID_REMAND);
					}
					if( i == 2 ){
						crd = get_internal_card_id_from_csv_id(CARD_ID_CHAR);
					}
					if( i == 3 ){
						crd = get_internal_card_id_from_csv_id(CARD_ID_ELECTROLYZE);
					}
				}
				if( crd != -1 ){
					update_rules_engine(check_card_for_rules_engine(get_internal_card_id_from_csv_id(crd)));
					deck[cd] = crd;
					cd++;
				}
			}
			shuffle(player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, 1, 10, MANACOST_UG(1, 1), 1, 8, 0, "Development", "Research");
}


int card_dovescape(int player, int card, event_t event){
	/* Dovescape	|3|WU|WU|WU
	 * Enchantment
	 * Whenever a player casts a noncreature spell, counter that spell. That player puts X 1/1 |Swhite and |Sblue Bird creature tokens with flying onto the battlefield, where X is the spell's converted mana cost. */

	hybrid(player, card, event);

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.type_flag = DOESNT_MATCH;
		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &test) ){
			int amount = get_cmc(trigger_cause_controller, trigger_cause);

			token_generation_t token;
			default_token_definition(player, card, CARD_ID_BIRD, &token);
			token.t_player = trigger_cause_controller;
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE) |
								get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
			token.qty = amount;
			generate_token(&token);

			real_counter_a_spell(player, card, trigger_cause_controller, trigger_cause);
		}
	}

	return global_enchantment(player, card, event);
}

int card_dread_slag(int player, int card, event_t event){
/* CARD_ID_DREAD_SLAG	8199
Dread Slag	|3|B|R
Creature - Horror 9/9
Trample
~ gets -4/-4 for each card in your hand. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card) ){
		event_result -= (4*hand_count[player]);
	}

	return 0;
}

int card_drekavac(int player, int card, event_t event){
/* CARD_ID_DREKAVAC	8200
Drekavac	|1|B
Creature - Beast 3/3
When ~ enters the battlefield, sacrifice it unless you discard a noncreature card. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.type_flag = DOESNT_MATCH;
		this_test.zone = TARGET_ZONE_HAND;
		if( player == AI && ! check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			ai_modifier-=100;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.type_flag = DOESNT_MATCH;
		this_test.zone = TARGET_ZONE_HAND;

		ec_definition_t this_definition;
		default_ec_definition(player, player, &this_definition);
		this_definition.ai_selection_mode = AI_MIN_VALUE;
		int result = new_effect_coercion(&this_definition, &this_test);
		if( result == -1 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_elemental_resonance(int player, int card, event_t event){
/* CARD_ID_ELEMENTAL_RESONANCE	8201
Elemental Resonance	|2|G|G
Enchantment - Aura
Enchant permanent
At the beginning of your precombat main phase, add mana equal to enchanted permanent's mana cost to your mana pool. (Mana cost includes color.
If a mana symbol has multiple colors, choose one.) */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		if( beginning_of_phase(player, card, event, player, card, PHASE_MAIN1) ){
			card_ptr_t* c = cards_ptr[ get_id(instance->damage_target_player, instance->damage_target_card) ];
			produce_mana_multi(player, c->req_colorless, c->req_black, c->req_blue, c->req_green, c->req_red, c->req_white);
		}
	}
		if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	return targeted_aura(player, card, event, &td, "TARGET_PERMANENT");
}

int card_odds_ends(int player, int card, event_t event){
	/*
	  Odds |U|R
	  Instant
	  Flip a coin. If it comes up heads, counter target instant or sorcery spell. If it comes up tails, copy that spell and you may choose new targets for the copy.

	  Ends |3|W|R
	  Instant
	  Target player sacrifices two attacking creatures.
	*/
	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		counterspell_target_definition(player, card, &td, TYPE_SPELL);
		td.illegal_type = TYPE_CREATURE;

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		return generic_split_card(player, card, event, counterspell(player, card, EVENT_CAN_CAST, &td, 0), 0, MANACOST_XRW(3, 1, 1), can_target(&td2), 0, 0, NULL, NULL);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_PLAY_FIRST_HALF ){
		target_definition_t td;
		counterspell_target_definition(player, card, &td, TYPE_SPELL);
		td.illegal_type = TYPE_CREATURE;
		counterspell(player, card, EVENT_CAST_SPELL, &td, 0);
	}

	if( event == EVENT_PLAY_SECOND_HALF ){
		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		pick_target(&td2, "TARGET_PLAYER");
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_SPELL);
	td.illegal_type = TYPE_CREATURE;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 1) && counterspell_validate(player, card, &td, 0) ){
			int result = flip_a_coin(player, card);
			if( ! result ){
				real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			}
			else{
				copy_spell_from_stack(player, instance->targets[0].player,  instance->targets[0].card);
			}
		}
		if( (instance->info_slot & 2) && valid_target(&td2) ){
			if( can_sacrifice(player, instance->targets[0].player, 1, TYPE_CREATURE, 0) ){
				target_definition_t td1;
				default_target_definition(player, card, &td1, TYPE_CREATURE );
				td1.allowed_controller = instance->targets[0].player;
				td1.preferred_controller = instance->targets[0].player;
				td1.who_chooses = instance->targets[0].player;
				td1.illegal_abilities = 0;
				td1.required_state = TARGET_STATE_ATTACKING;
				td1.allow_cancel = 0;
				int i;
				 for(i=0; i<2; i++){
					if( can_target(&td1) ){
						pick_target(&td1, "LORD_OF_THE_PIT");
						instance->number_of_targets = 1;
						kill_card( instance->targets[0].player,  instance->targets[0].card, KILL_SACRIFICE);
					}
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, counterspell(player, card, EVENT_CAN_CAST, &td, 0), 10, MANACOST_XRW(3, 1, 1), can_target(&td2), 8, 0, "Odds", "Ends");
}

int card_enemy_of_the_guildpact(int player, int card, event_t event){
/* CARD_ID_ENEMY_OF_THE_GUILDPACT	8202
Enemy of the Guildpact	|4|B
Creature - Spirit 4/2
Protection from multicolored */

	protection_from_multicolored(player, card, event);
	return 0;
}

int card_error_trial(int player, int card, event_t event){
	/*
	  Trial |W|U
	  Instant
	  Return all creatures blocking or blocked by target creature to their owner's hand.

	  Error |U|B
	  Instant
	  Counter target multicolored spell.
	*/
	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		target_definition_t td2;
		counterspell_target_definition(player, card, &td2, TYPE_SPELL);
		td2.extra = (int32_t)is_multicolored;
		td2.special = TARGET_SPECIAL_EXTRA_FUNCTION;

		return generic_split_card(player, card, event, can_target(&td), 0, MANACOST_UB(1, 1), counterspell(player, card, EVENT_CAN_CAST, &td2, 0), 0, 0, NULL, NULL);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_PLAY_FIRST_HALF ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		pick_target(&td, "TARGET_CREATURE");
	}

	if( event == EVENT_PLAY_SECOND_HALF ){
		target_definition_t td2;
		counterspell_target_definition(player, card, &td2, TYPE_SPELL);
		td2.extra = (int32_t)is_multicolored;
		td2.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		counterspell(player, card, EVENT_CAST_SPELL, &td2, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	target_definition_t td2;
	counterspell_target_definition(player, card, &td2, TYPE_SPELL);
	td2.extra = (int32_t)is_multicolored;
	td2.special = TARGET_SPECIAL_EXTRA_FUNCTION;

	if(event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 1) && valid_target(&td) ){
			if( current_turn != instance->targets[0].player && get_card_instance(instance->targets[0].player, instance->targets[0].card)->blocking != 255 ){
				bounce_permanent(1-instance->targets[0].player, get_card_instance(instance->targets[0].player, instance->targets[0].card)->blocking);
			}
			if( current_turn == instance->targets[0].player && is_attacking(instance->targets[0].player, instance->targets[0].card) ){
				int i;
				for(i=0; i<active_cards_count[1-instance->targets[0].player]; i++){
					if( in_play(1-instance->targets[0].player, i) ){
						if( get_card_instance(1-instance->targets[0].player, i)->blocking == instance->targets[0].card ){
							bounce_permanent(1-instance->targets[0].player, i);
						}
					}
				}
			}
		}
		if( (instance->info_slot & 2) && counterspell_validate(player, card, &td2, 0) ){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, can_target(&td), 8, MANACOST_UB(1, 1), counterspell(player, card, EVENT_CAN_CAST, &td2, 0), 10, 0, NULL, NULL);
}

static int evolution_vat_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_RESOLVE_ACTIVATION ){
			int amount = count_1_1_counters(p, c);
			add_1_1_counters(p, c, amount);
		}

		return granted_generic_activated_ability(player, card, p, c, event, 0, MANACOST_XUG(2, 1, 1), 0, NULL, NULL);
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_evolution_vat(int player, int card, event_t event){
/* CARD_ID_EVOLUTION_VAT	8205
Evolution Vat	|3
Artifact
|3, |T: Tap target creature and put a +1/+1 counter on it. Until end of turn, that creature gains "|2|G|U: Double the number of +1/+1 counters on this creature."*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
			if( instance->targets[0].player == player ){
				create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &evolution_vat_legacy,
												instance->targets[0].player, instance->targets[0].card);
			}
			else{
				int fake = add_card_to_hand(1-player, get_card_instance(instance->parent_controller, instance->parent_card)->internal_card_id);
				create_targetted_legacy_effect(1-player, fake, &evolution_vat_legacy, instance->targets[0].player, instance->targets[0].card);
				obliterate_card(1-player, fake);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_CREATURE");
}

int card_experiment_kraj(int player, int card, event_t event ){
/* CARD_ID_EXPERIMENT_KRAJ	8206
Experiment Kraj	|2|G|G|U|U
Legendary Creature - Ooze Mutant 4/6
~ has all activated abilities of each other creature with a +1/+1 counter on it.
|T: Put a +1/+1 counter on target creature. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			int i, result = 0;
			for(i=0; i < 2; i++){
				int count = active_cards_count[i]-1;
				while( count > -1 ){
						if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) && count_1_1_counters(i, count) > 0 &&
							get_id(i, count) != CARD_ID_EXPERIMENT_KRAJ
						  ){
							card_data_t* card_d = get_card_data(i, count);
							CardFunction ptFunction = card_d->code_pointer;
							int this_result = ptFunction(player, card, EVENT_CAN_ACTIVATE );
							if( this_result ){
								if( this_result == 99 ){
									return 99;
								}
								result = this_result;
							}
						}
						count--;
				}
			}
			return result;
		}
	}
	if( event == EVENT_ACTIVATE){
		int stolen_abilities[12] = {-1, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}; //For simplicity sake, let's consider 10 to be the maximum amount of the "stolen" abilities. Position '0' in this array is 'cancel' and position '1' is the main Experiment Kraj ability.
		char stolen_abilities_name[12][100];
		int i, sac = 2;
		for(i=0; i < 2; i++){
			int count = active_cards_count[i]-1;
			while( count > -1 ){
					if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) && count_1_1_counters(i, count) > 0 &&
						get_id(i, count) != CARD_ID_EXPERIMENT_KRAJ
					  ){
						card_data_t* card_d = get_card_data(i, count);
						CardFunction ptFunction = card_d->code_pointer;
						if( ptFunction(player, card, EVENT_CAN_ACTIVATE ) ){
							stolen_abilities[sac] = get_card_instance(i, count)->internal_card_id;
							strcpy(stolen_abilities_name[sac], cards_ptr[get_id(i, count)]->name);
							sac++;
						}
					}
					if( sac == 11 ){
						break;
					}
					count--;
			}
			if( sac == 11 ){
				break;
			}
		}
		int ek_ability = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL,
						"Give a +1/+1 counter", ek_ability, 5,
						stolen_abilities_name[2], stolen_abilities[2] > -1, 1,
						stolen_abilities_name[3], stolen_abilities[3] > -1, 1,
						stolen_abilities_name[4], stolen_abilities[4] > -1, 1,
						stolen_abilities_name[5], stolen_abilities[5] > -1, 1,
						stolen_abilities_name[6], stolen_abilities[6] > -1, 1,
						stolen_abilities_name[7], stolen_abilities[7] > -1, 1,
						stolen_abilities_name[8], stolen_abilities[8] > -1, 1,
						stolen_abilities_name[9], stolen_abilities[9] > -1, 1,
						stolen_abilities_name[10], stolen_abilities[10] > -1, 1,
						stolen_abilities_name[11], stolen_abilities[11] > -1, 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == 1 ){
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
			if( spell_fizzled != 1 ){
				instance->info_slot = 0;
			}
		}
		else{
			CardFunction ptFunction = cards_data[stolen_abilities[choice]].code_pointer;
			ptFunction(player, card, EVENT_ACTIVATE );
			if( spell_fizzled != 1 ){
				instance->info_slot = stolen_abilities[choice];
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 0 ){
			if( valid_target(&td) ){
				add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
			}
		}
		if( instance->info_slot > 0 ){
			CardFunction ptFunction = cards_data[instance->info_slot].code_pointer;
			ptFunction(instance->parent_controller, instance->parent_card, EVENT_RESOLVE_ACTIVATION);
		}
	}

	return 0;
}

int card_fall_rise(int player, int card, event_t event){
	/*
	  Rise |U|B
	  Sorcery
	  Return target creature card from a graveyard and target creature on the battlefield to their owners' hands.

	  Fall |B|R
	  Sorcery
	  Target player reveals two cards at random from his or her hand, then discards each nonland card revealed this way.
	*/

	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.zone = TARGET_ZONE_PLAYERS;

		int can_play_rise = can_target(&td) && count_graveyard_by_type(2, TYPE_CREATURE) && ! graveyard_has_shroud(2);

		return generic_split_card(player, card, event, can_play_rise, 0, MANACOST_BR(1, 1), can_target(&td2), 0, 0, NULL, NULL);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_PLAY_FIRST_HALF ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		if( pick_target(&td, "TARGET_CREATURE") ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card");
			select_target_from_either_grave(player, card, 0, AI_MAX_VALUE, AI_MIN_VALUE, &this_test, 1, 2);
		}
	}


	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.zone = TARGET_ZONE_PLAYERS;

	int can_play_rise = can_target(&td) && count_graveyard_by_type(2, TYPE_CREATURE) && ! graveyard_has_shroud(2);

	if(event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 1) && valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			this_test.type_flag = DOESNT_MATCH;

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			this_definition.cards_to_reveal = 2;
			this_definition.effect = EC_ALL_WHICH_MATCH_CRITERIA | EC_DISCARD;
			new_effect_coercion(&this_definition, &this_test);
		}
		if( instance->info_slot & 2 ){
			if( validate_target(player, card, &td2, 0) ){
				bounce_permanent(instance->targets[0].player,  instance->targets[0].card);
			}
			int selected = validate_target_from_grave_source(player, card, instance->targets[1].player, 2);
			if( selected != -1 ){
				from_grave_to_hand(instance->targets[1].player, selected, TUTOR_HAND);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, can_play_rise, 0, MANACOST_BR(1, 1), can_target(&td2), 0, 0, "Rise", "Fall");
}

int card_flame_kin_war_scout(int player, int card, event_t event){
/* CARD_ID_FLAME_KIN_WAR_SCOUT	8208
Flame-Kin War Scout	|3|R
Creature - Elemental Scout 2/4
When another creature enters the battlefield, sacrifice ~. If you do, ~ deals 4 damage to that creature. */

	if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) )
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.not_me = 1;

        if( new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) )
		{
			damage_creature(trigger_cause_controller, trigger_cause, 4, player, card);
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_flaring_flame_kin(int player, int card, event_t event){
/* CARD_ID_FLARING_FLAME_KIN	8209
Flaring Flame-Kin	|2|R
Creature - Elemental Warrior 2/2
As long as ~ is enchanted, it gets +2/+2, has trample, and has "|R: ~ gets +1/+0 until end of turn." */

	if( in_play(player, card) && ! is_humiliated(player, card) && is_enchanted(player, card) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			event_result += 2;
		}
		if( event == EVENT_ABILITIES && affect_me(player, card) ){
			event_result |= KEYWORD_TRAMPLE;
		}
		return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1, 0);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1, 0);
	}

	return 0;
}

int card_freewind_equenaut(int player, int card, event_t event){
/* CARD_ID_FREEWIND_EQUENAUT	8211
Freewind Equenaut	|2|W
Creature - Human Archer 2/2
Flying
As long as ~ is enchanted, it has "|T: ~ deals 2 damage to target attacking or blocking creature." */

	if( in_play(player, card) && ! is_humiliated(player, card) && is_enchanted(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_state = TARGET_STATE_IN_COMBAT;

		card_instance_t *instance = get_card_instance( player, card );

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, 2, player, instance->parent_card);
			}
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0,
										&td, "Select target attacking or blocking creature.");
	}

	return 0;
}

int card_gobhobbler_rats(int player, int card, event_t event){
/* CARD_ID_GOBHOBBLER_RATS	8214
Gobhobbler Rats	|B|R
Creature - Rat 2/2
Hellbent - As long as you have no cards in hand, ~ gets +1/+0 and has "|B: Regenerate ~." */

	if( in_play(player, card) && ! is_humiliated(player, card) && hand_count[player] < 1 ){
		modify_pt_and_abilities(player, card, event, 1, 0, KEYWORD_REGENERATION);
		return regeneration(player, card, event, MANACOST_B(1));
	}
	return 0;
}

int card_grand_arbiter_augustin_iv(int player, int card, event_t event){
/* CARD_ID_GRAND_ARBITER_AUGUSTIN_IV	8216
Grand Arbiter Augustin IV	|2|W|U
Legendary Creature - Human Advisor 2/3
|SWhite spells you cast cost |1 less to cast.
|SBlue spells you cast cost |1 less to cast.
Spells your opponents cast cost |1 more to cast. */

	check_legend_rule(player, card, event);
	if(event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card) ){
		if( affected_card_controller == player ){
			int clr = get_color_by_internal_id(player, get_card_instance(affected_card_controller, affected_card)->internal_card_id);
			if( clr & COLOR_TEST_BLUE ){
				COST_COLORLESS--;
			}
			if( clr & COLOR_TEST_WHITE ){
				COST_COLORLESS--;
			}
		}
		else{
			card_data_t* card_d = get_card_data(affected_card_controller, affected_card);
			if( ! ( card_d->type & TYPE_LAND)  ){
				COST_COLORLESS++;
			}
		}
	}
	return 0;
}

int card_guardian_of_the_guildpact(int player, int card, event_t event){
/* CARD_ID_GUARDIAN_OF_THE_GUILDPACT	8217
Guardian of the Guildpact	|3|W
Creature - Spirit 2/3
Protection from monocolored */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card) ){
			if( player == attacking_card_controller && card == attacking_card ){
				if( count_colors(affected_card_controller, affected_card) == 1 ){
					event_result =  1;
				}
			}
		}
		if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card) ){
			card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card && damage->damage_target_player == player &&
				damage->damage_target_card == card && damage->info_slot > 0 &&
				count_colors(damage->damage_source_player, damage->damage_source_card) == 1 )
			{
				int amount = damage->info_slot;
				prevent_damage(affected_card_controller, affected_card, amount);
			}
		}
	}
	return 0;
}

int card_haazda_shield_mate(int player, int card, event_t event){
	/* Haazda Shield Mate	|2|W
	 * Creature - Human Soldier 1/1
	 * At the beginning of your upkeep, sacrifice ~ unless you pay |W|W.
	 * |W: The next time a source of your choice would deal damage to you this turn, prevent that damage. */

	basic_upkeep(player, card, event, MANACOST_W(2));

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_EFFECT);
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;
	td.extra = damage_card;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			int amount = target->info_slot;
			prevent_damage(instance->targets[0].player, instance->targets[0].card, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION | GAA_LITERAL_PROMPT, MANACOST_W(1), 0,
									&td, "Select target damage card attached to you.");
}

int card_helium_squirter(int player, int card, event_t event){

	/* Helium Squirter	|4|U
	 * Creature - Beast Mutant 0/0
	 * Graft 3
	 * |1: Target creature with a +1/+1 counter on it gains flying until end of turn. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);

	graft_trigger(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_P1_P1;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FLYING, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(1), 0,
									&td, "Select target creature with a +1/+1 counter.");
}

int card_hellhole_rats(int player, int card, event_t event){
/* CARD_ID_HELLHOLE_RATS	8221
Hellhole Rats	|2|B|R
Creature - Rat 2/2
Haste
When ~ enters the battlefield, target player discards a card. ~ deals damage to that player equal to that card's converted mana cost. */
	haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, instance->targets[0].player, &this_definition);
			this_definition.ai_selection_mode = AI_MIN_CMC;
			int result = new_effect_coercion(&this_definition, &this_test);
			if( result > -1 ){
				damage_player(instance->targets[0].player, get_cmc_by_internal_id(result), player, card);
			}
		}
	}

	return 0;
}

int card_hide_seek(int player, int card, event_t event){
	/*
	  Hide |W|R
	  Instant
	  Put target artifact or enchantment on the bottom of its owner's library.

	  Seek |W|B
	  Instant
	  Search target opponent's library for a card and exile it. You gain life equal to its converted mana cost. Then that player shuffles his or her library.
	*/
	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT );

		target_definition_t td2;
		default_target_definition(player, card, &td2, 0 );
		td2.zone = TARGET_ZONE_PLAYERS;

		int can_play_seek = would_validate_arbitrary_target(&td2, 1-player, -1);
		return generic_split_card(player, card, event, can_target(&td), 0, MANACOST_WB(1, 1), can_play_seek, 0, 0, NULL, NULL);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_PLAY_FIRST_HALF ){
		instance->number_of_targets = 0;

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT );
		pick_target(&td, "DISENCHANT");
	}

	if( event == EVENT_PLAY_SECOND_HALF ){
		instance->targets[0].player = 1-player;
		instance->targets[0].card = -1;
		instance->number_of_targets = 1;
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT );

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE );
	td2.zone = TARGET_ZONE_PLAYERS;

	int can_play_seek = would_validate_arbitrary_target(&td2, 1-player, -1);
	int priority_hide = 10;
	int priority_seek = life[player] < 6 ? 15 : 5;

	if( event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 1) && valid_target(&td) ){
			put_on_bottom_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		if( (instance->info_slot & 2) && valid_target(&td2) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			int selected = new_select_a_card(player, instance->targets[0].player, TUTOR_FROM_DECK, 0, AI_MAX_CMC, -1, &this_test);
			if( selected != -1 ){
				int amount = get_cmc_by_id(cards_data[deck_ptr[1-player][selected]].id);
				rfg_card_in_deck(1-player, selected);
				gain_life(player, amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, can_target(&td), priority_hide, MANACOST_WB(1, 1), can_play_seek, priority_seek, 0, "Hide", "Seek");
}

int card_hit_run(int player, int card, event_t event){
	/*
	  Hit |1|B|R
	  Instant
	  Target player sacrifices an artifact or creature. Hit deals damage to that player equal to that permanent's converted mana cost.

	  Run |3|R|G
	  Instant
	  Attacking creatures you control get +1/+0 until end of turn for each other attacking creature.
	*/

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.zone = TARGET_ZONE_PLAYERS;

		return generic_split_card(player, card, event, can_target(&td), 0, MANACOST_XGR(3, 1, 1), 1, 0, 0, NULL, NULL);
	}

	if( event == EVENT_PLAY_FIRST_HALF ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.zone = TARGET_ZONE_PLAYERS;
		pick_target(&td, "TARGET_PLAYER");
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	int priority_hit = 10;
	int priority_run = 5;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI && current_turn == player ){
		priority_run = 4*count_attackers(player);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 1) && valid_target(&td) ){
			instance->targets[1].card = -1;
			impose_sacrifice(player, card, instance->targets[0].player, 1, TYPE_CREATURE | TYPE_ARTIFACT, 0, 0, 0, 0, 0, 0, 0, -1, 0);
			if( instance->targets[1].card > -1 ){
				damage_player(instance->targets[0].player, get_cmc_by_id(instance->targets[1].card), player, card);
			}
		}
		if( (instance->info_slot & 2) ){
			int amount = count_attackers(player);
			if( amount > 0 ){
				pump_subtype_until_eot(player, card, player, -1, amount, 0, 0, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, can_target(&td), priority_hit, MANACOST_XGR(3, 1, 1), 1, priority_run, 0, "Hit", "Run");
}

int card_indrik_stomphowler(int player, int card, event_t event){
	/*
	  Indrik Stomphowler |4|G
	  Creature - Beast 4/4
	  When Indrik Stomphowler enters the battlefield, destroy target artifact or enchantment.
	*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ENCHANTMENT | TYPE_ARTIFACT);
		td.allow_cancel = 0;

		if( can_target(&td) && pick_target(&td, "DISENCHANT") ){
			action_on_target(player, card, 0, KILL_DESTROY);
		}
		get_card_instance(player, card)->number_of_targets = 0;
	}

	return 0;
}


int card_infernal_tutor(int player, int card, event_t event){
/* CARD_ID_INFERNAL_TUTOR	8225
Infernal Tutor	|1|B
Sorcery
Reveal a card from your hand. Search your library for a card with the same name as that card, reveal it, put it into your hand, then shuffle your library.
Hellbent - If you have no cards in hand, instead search your library for a card, put it into your hand, then shuffle your library. */

	if(event == EVENT_RESOLVE_SPELL ){
		if( hand_count[player] < 1 ){
			return card_demonic_tutor(player, card, event);
		}
		else{
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card");
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MAX_VALUE, -1, &test);
			int csvid = get_id(player, selected);
			reveal_card(player, card, player, selected);

			char buffer[100];
			scnprintf(buffer, 100, "Select a card named %s", cards_ptr[csvid]->name);
			test_definition_t test2;
			new_default_test_definition(&test2, TYPE_ANY, buffer);
			test2.id = csvid;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &test2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_isperia_the_inscrutable(int player, int card, event_t event){
/*
Isperia the Inscrutable |1|W|W|U|U
Legendary Creature — Sphinx 3/6
Flying
Whenever Isperia the Inscrutable deals combat damage to a player, name a card. That player reveals his or her hand.
If he or she reveals the named card, search your library for a creature card with flying, reveal it, put it into your hand,
	then shuffle your library.
*/
	check_legend_rule(player, card, event);

	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE))
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = {BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player)};
		instance->targets[1].player = 0;
		APNAP(p,{
					if( times_damaged[p] ){
						int csvid = -1;
						if( player == HUMAN ){
							csvid = card_from_list(player, 3, TYPE_ANY, 0, 0, 0, 0, 0, 0, 0, -1, 0);
						}
						else{
							if( p != player ){
								csvid = ai_name_a_random_csvid(player, card, p, TYPE_ANY, NULL);
							}
							else{
								test_definition_t test;
								default_test_definition(&test, TYPE_ANY);
								int crd = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_VALUE, -1, &test);
								if( crd ){
									csvid = get_id(player, crd);
								}
								else{
									csvid = ai_name_a_random_csvid(player, card, p, TYPE_ANY, NULL);
								}
							}
						}
						reveal_target_player_hand(p);
						if( is_id_in_hand(p, csvid) ){
							test_definition_t this_test;
							new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card with flying.");
							this_test.keyword = KEYWORD_FLYING;
							new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
						}
					}
				}
		);
	}

	return 0;
}

int card_jagged_poppet(int player, int card, event_t event){
/*
Jagged Poppet |1|B|R
Creature — Ogre Warrior 3/4
Whenever Jagged Poppet is dealt damage, discard that many cards.
Hellbent — Whenever Jagged Poppet deals combat damage to a player, if you have no cards in hand, that player discards cards
			equal to the damage.
*/
	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t* damage = damage_being_prevented(event);
		if( damage ){
			card_instance_t *instance = get_card_instance(player, card);
			if( damage->damage_target_card == card && damage->damage_target_player == player ){
				SET_BYTE0(instance->info_slot) += damage->info_slot;
			}
			if( hand_count[player] == 0 && damage->damage_source_player == player && damage->damage_source_card == card &&
				damage->damage_target_card == -1 && ! damage_is_to_planeswalker(damage) )
			{
				SET_BYTE1(instance->info_slot) += damage->info_slot;
				if( damage->damage_target_player == 0 ){
					SET_BYTE0(instance->targets[1].player) = 1;
				}
				if( damage->damage_target_player == 1 ){
					SET_BYTE1(instance->targets[1].player) = 1;
				}
			}
		}

		if( get_card_instance(player, card)->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
			reason_for_trigger_controller == player )
		{
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if( event == EVENT_RESOLVE_TRIGGER ){
					card_instance_t *instance = get_card_instance(player, card);
					int controller_discards = BYTE0(instance->info_slot);
					int damaged_players_discards = BYTE1(instance->info_slot);
					if( controller_discards ){
						new_multidiscard(player, controller_discards, 0, player);
					}
					if( damaged_players_discards ){
						if( BYTE0(instance->targets[1].player) ){
							new_multidiscard(0, damaged_players_discards, 0, player);
						}
						if( BYTE1(instance->targets[1].player) ){
							new_multidiscard(1, damaged_players_discards, 0, player);
						}
						instance->targets[1].player = 0;
					}
					instance->info_slot = 0;
			}
		}
	}

	return 0;
}

int card_loaming_shaman(int player, int card, event_t event){
/*
Loaming Shaman |2|G
Creature — Centaur Shaman 3/2
When Loaming Shaman enters the battlefield, target player shuffles any number of target cards from his or her graveyard
	into his or her library.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.zone = TARGET_ZONE_PLAYERS;
		td1.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td1) && pick_target(&td1, "TARGET_PLAYER") ){
			int cg = count_graveyard(instance->targets[0].player)-1;
			if( player == AI ){
				while( cg > -1 ){
						from_graveyard_to_deck(instance->targets[0].player, cg, 1);
						cg--;
				}
				shuffle(instance->targets[0].player);
			}
			else{
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_ANY);
				this_test.no_shuffle = 1;
				while( cg > -1 && new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_GRAVE, TUTOR_DECK, 0, AI_MAX_VALUE, &this_test) != -1 ){
						cg--;
				}
				shuffle(instance->targets[0].player);
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_lyzolda_the_blood_witch(int player, int card, event_t event){
	/* Lyzolda, the Blood Witch	|1|B|R	0x2006da3
	 * Legendary Creature - Human Cleric 3/1
	 * |2, Sacrifice a creature: ~ deals 2 damage to any target if the sacrificed creature was |Sred. Draw a card if the sacrificed creature was |Sblack. */
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST_X(2), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
			if( ! sac ){
				spell_fizzled = 1;
				return 0;
			}
			int clr = get_color(BYTE2(sac), BYTE3(sac));
			if( clr & COLOR_TEST_RED ){
				pick_target(&td, "TARGET_ANY");
			}
			if( spell_fizzled != 1 ){
				instance->info_slot = clr;
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( (instance->info_slot & COLOR_TEST_RED) && valid_target(&td) ){
			damage_target0(player, card, 2);
		}
		if( instance->info_slot & COLOR_TEST_BLACK ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_macabre_waltz(int player, int card, event_t event)
{
  /* Macabre Waltz	|1|B
   * Sorcery
   * Return up to two target creature cards from your graveyard to your hand, then discard a card. */

  if (!IS_CASTING(player, card, event))
	return 0;
  test_definition_t test;
  new_default_test_definition(&test, TYPE_CREATURE, "Select up to two target creature cards.");
  int rval = spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(player, card, event, 2, &test, 1);
  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (spell_fizzled != 1)
		discard(player, 0, player);
	  kill_card(player, card, KILL_DESTROY);
	}
  return rval;
}

int card_might_of_the_nephilim(int player, int card, event_t event){
/* CARD_ID_MIGHT_OF_THE_NEPHILIM	8235
Might of the Nephilim	|1|G
Instant
Target creature gets +2/+2 until end of turn for each of its colors. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = 2*count_colors(instance->targets[0].player, instance->targets[0].card);
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, amount, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_momir_vig(int player, int card, event_t event){
/*
Momir Vig, Simic Visionary |3|U|G
Legendary Creature — Elf Wizard 2/2
Whenever you cast a green creature spell, you may search your library for a creature card and reveal it.
		If you do, shuffle your library and put that card on top of it.
Whenever you cast a blue creature spell, reveal the top card of your library. If it's a creature card, put that card into your hand.
*/
	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		int clr1 = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		int clr2 = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		int spell_color = get_color(trigger_cause_controller, trigger_cause);
		if( is_what(trigger_cause_controller, trigger_cause, TYPE_CREATURE) && (spell_color & (clr1 | clr2)) ){
			int trig = (spell_color & clr2) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_AI(player);
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			test.color = clr1 | clr2;
			if( new_specific_spell_played(player, card, event, player, trig, &test) ){
				if( spell_color & clr1 ){
					int choice = 0;
					if( player != AI ){
						choice = do_dialog(player, player, card, -1, -1, " Tutor creature\n Pass", 0);
					}
					if( choice == 0 ){
						test_definition_t test2;
						new_default_test_definition(&test2, TYPE_CREATURE, "Select a creature card");
						new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_DECK, 0, AI_MAX_VALUE, &test2);
					}
				}
				if( spell_color & clr2 ){
					int *deck = deck_ptr[player];
					if( deck[0] != -1 ){
						int iid = deck[0];
						reveal_card_iid(player, card, iid);
						if( is_what(-1, iid, TYPE_CREATURE) ){
							add_card_to_hand(player, iid);
							remove_card_from_deck(player, 0);
						}
					}
				}
			}
		}
	}

	return 0;
}

int card_nihilistic_glee(int player, int card, event_t event){
	/* Nihilistic Glee	|2|B|B
	 * Enchantment
	 * |2|B, Discard a card: Target opponent loses 1 life and you gain 1 life.
	 * Hellbent - |1, Pay 2 life: Draw a card. Activate this ability only if you have no cards in hand. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_DISCARD | GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST_XB(2, 1), 0, &td, NULL) ){
			return 1;
		}
		return hand_count[player] < 1 && generic_activated_ability(player, card, event, 0, MANACOST_X(1), 2, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_DISCARD | GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST_XB(2, 1), 0, &td, NULL) ){
			if( hand_count[player] < 1 && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(1), 2, NULL, NULL) ){
				choice = do_dialog(player, player, card, -1, -1, " Suck life\n Draw a card\n Cancel", life[1-player] < 6 && life[player] < 6 ? 0 : 1);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 0 ){
			generic_activated_ability(player, card, event, GAA_DISCARD | GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST_XB(2, 1), 0, &td, "TARGET_PLAYER");
			if( spell_fizzled != 1 ){
				instance->info_slot = 66;
			}
		}
		else if( choice == 1 ){
				generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(1), 2, NULL, NULL);
				if( spell_fizzled != 1 ){
					instance->info_slot = 67;
				}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 && valid_target(&td) ){
			lose_life(1-player, 1);
			gain_life(player, 1);
		}
		if( instance->info_slot == 67 ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_novijen_sages(int player, int card, event_t event){

	/* Novijen Sages	|4|U|U
	 * Creature - Human Advisor Mutant 0/0
	 * Graft 4
	 * |1, Remove two +1/+1 counters from among creatures you control: Draw a card. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 4);

	graft_trigger(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	base_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_P1_P1;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL) ){
			int result = 0;
			int count = 0;
			while( count < active_cards_count[player] ){
					if( is_what(player, count, TYPE_CREATURE) && in_play(player, count) && count_1_1_counters(player, count) > 0 ){
						result+=count_1_1_counters(player, count);
					}
					if( result > 1 ){
						return 1;
					}
					count++;
			}
		}
	}

	if( event == EVENT_ACTIVATE){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			int targetted[2];
			int i;
			for(i=0; i<2; i++){
				if( new_pick_target(&td, "Select a creature you control with a +1/+1 counter.", 0, GS_LITERAL_PROMPT) ){
					targetted[i] = instance->targets[0].card;
					if( i == 1 && targetted[0] == targetted[1] && count_counters(player, targetted[0], COUNTER_P1_P1) < 2 ){
						spell_fizzled = 1;
						return 0;
					}
				}
				else{
					spell_fizzled = 1;
					return 0;
				}
			}
			instance->number_of_targets = 0;
			for(i=0; i<2; i++){
				remove_1_1_counter(player, targetted[i]);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return 0;
}

void activate_novijen_heart_of_progress(int player, int card, int color){
	int p, c;
	for(p=0; p<2; p++){
		for(c=active_cards_count[p]-1; c>-1; c--){
			if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) && check_special_flags(p, c, SF_JUST_CAME_INTO_PLAY) ){
				add_1_1_counter(p, c);
				}
			}
		}
}

int card_novijen_heart_of_progress(int player, int card, event_t event){
/* CARD_ID_NOVIJEN_HEART_OF_PROGRESS	8243
Novijen, Heart of Progress	""
Land
|T: Add |C to your mana pool.
|G|U, |T: Put a +1/+1 counter on each creature that entered the battlefield this turn. */

	if( event == EVENT_CLEANUP ){
		remove_special_flags(2, -1, SF_JUST_CAME_INTO_PLAY);
	}

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance= get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_P1_P1
	};

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_UG(1, 1), 0, NULL, NULL) )
		{
			int priority_p1_p1 = 1;
			int i;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_play(player, i) && is_what(player, i, TYPE_CREATURE) && check_special_flags(player, i, SF_JUST_CAME_INTO_PLAY) ){
					priority_p1_p1 = 10;
					break;
				}
			}
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 		1, 5,
							"Pump Creatures", 	1, priority_p1_p1);

			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_P1_P1 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_UG(1, 1)) ){
				add_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_P1_P1 ){
			activate_novijen_heart_of_progress(player, card, 0);
		}
	}

	return 0;
}

int card_omnibian(int player, int card, event_t event){
/* CARD_ID_OMNIBIAN	8247
Omnibian	|1|G|G|U
Creature - Frog 3/3
|T: Target creature becomes a Frog with base power and toughness 3/3 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			force_a_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_FROG);
			set_pt_and_abilities_until_eot( instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card,
											3, 3, 0, 0, SET_PT_END_AT_EOT | SET_PT_RESET_SUBTYPES_AT_END);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_overrule(int player, int card, event_t event){
	/* Overrule	|X|W|U
	 * Instant
	 * Counter target spell unless its controller pays |X. You gain X life. */

	if( event == EVENT_RESOLVE_SPELL){
		card_instance_t *instance = get_card_instance( player, card );
		if( counterspell_resolve_unless_pay_x(player, card, NULL, 0, instance->info_slot) ){
			gain_life(player, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL | GS_X_SPELL, NULL, NULL, 1, NULL);
}

int card_pain_magnification(int player, int card, event_t event){
/*
Pain Magnification |1|B|R
Enchantment
Whenever an opponent is dealt 3 or more damage by a single source, that player discards a card.
*/

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t* damage = damage_being_dealt(event);
		if( damage ){
			if( damage->damage_target_card == -1 && damage->damage_target_player == 1-player &&
				damage_is_to_planeswalker(damage) && damage->info_slot >= 3 )
			{
				card_instance_t *instance = get_card_instance(player, card);
				if( instance->targets[8].player < 0 ){
					instance->targets[8].player = 0;
				}
				instance->targets[8].player++;
			}
		}

		int result = resolve_damage_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card);
		if( result ){
			new_multidiscard(1-player, result, 0, player);
		}
	}

	return global_enchantment(player, card, event);
}

int card_paladin_of_prahv(int player, int card, event_t event){
	/* Paladin of Prahv	|4|W|W
	 * Creature - Human Knight 3/4
	 * Whenever ~ deals damage, you gain that much life.
	 * Forecast - |1|W, Reveal ~ from your hand: Whenever target creature deals damage this turn, you gain that much life. */

	spirit_link_effect(player, card, event, player);

	if( ! IS_ACTIVATING_FROM_HAND(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);



	if( event == EVENT_CAN_ACTIVATE_FROM_HAND ){
		if( has_mana_multi(player, MANACOST_XW(1, 1)) && can_target(&td) && instance->info_slot != 1 && current_phase == PHASE_UPKEEP){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE_FROM_HAND ){
		instance->number_of_targets = 0;
		charge_mana_multi(player, MANACOST_XW(1, 1));
		if( spell_fizzled != 1 && pick_target(&td, "TARGET_CREATURE") ){
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		if( valid_target(&td) ){
			int legacy = pump_ability_until_eot(player, get_card_instance(player, card)->targets[1].card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, 0);
			card_instance_t* leg = get_card_instance(player, legacy);
			if (leg->targets[4].card == -1){
				leg->targets[4].card = 0;
			}
			leg->targets[4].card |= (player == 0 ? 1 : 2);
		}
	}

	return 0;
}

int card_patagia_viper(int player, int card, event_t event){
	/* Patagia Viper	|3|G
	 * Creature - Snake 2/1
	 * Flying
	 * When ~ enters the battlefield, put two 1/1 |Sgreen and |Sblue Snake creature tokens onto the battlefield.
	 * When ~ enters the battlefield, sacrifice it unless |U was spent to cast it. */

	ravnica_manachanger(player, card, event, 0, 1, 0, 0, 0);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && get_card_instance( player, card )->info_slot == 1){
			get_card_instance( player, card )->targets[1].player = charge_changed_mana(player, card, 0, 1, 0, 0, 0);
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SNAKE, &token);
		token.color_forced = 	get_sleighted_color_test(player, card, COLOR_TEST_BLUE) |
								get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		token.qty = 2;
		generate_token(&token);
		if( get_card_instance( player, card )->targets[1].player != 2 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_plaxcaster_frogling(int player, int card, event_t event){

	/* Plaxcaster Frogling	|1|G|U
	 * Creature - Frog Mutant 0/0
	 * Graft 3
	 * |2: Target creature with a +1/+1 counter on it gains shroud until end of turn. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);

	graft_trigger(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_P1_P1;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot( instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_SHROUD, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(2), 0,
									&td, "Select target creature with a +1/+1 counter.");
}

int card_plaxmanta(int player, int card, event_t event){
/* CARD_ID_PLAXMANTA	8255
Plaxmanta	|1|U
Creature - Beast 2/2
Flash
When ~ enters the battlefield, creatures you control gain shroud until end of turn. (They can't be the targets of spells or abilities.)
When ~ enters the battlefield, sacrifice it unless |G was spent to cast it. */

	ravnica_manachanger(player, card, event, 0, 0, 1, 0, 0);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && get_card_instance( player, card )->info_slot == 1){
			get_card_instance( player, card )->targets[1].player = charge_changed_mana(player, card, 0, 0, 1, 0, 0);
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		pump_subtype_until_eot(player, card, player, -1, 0, 0, KEYWORD_SHROUD, 0);
		if( get_card_instance( player, card )->targets[1].player != 2 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return flash(player, card, event);
}

int card_plumes_of_peace(int player, int card, event_t event){
	/* Plumes of Peace	|1|W|U
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature doesn't untap during its controller's untap step.
	 * Forecast - |W|U, Reveal ~ from your hand: Tap target creature. */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_ACTIVATE_FROM_HAND ){
			if( has_mana_multi(player, MANACOST_UW(1, 1)) && can_target(&td) && instance->info_slot != 1 && current_phase == PHASE_UPKEEP){
				return 1;
			}
		}

		if( event == EVENT_ACTIVATE_FROM_HAND ){
			charge_mana_multi(player, MANACOST_UW(1, 1));
			if( spell_fizzled != 1 && pick_target(&td, "TARGET_CREATURE") ){
				instance->number_of_targets = 1;
				return 1;
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
			if( valid_target(&td) ){
				tap_card(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);
	}

	return disabling_aura(player, card, event);
}

int card_prahv_spires_of_order(int player, int card, event_t event){
/*
Prahv, Spires of Order
Land
{T}: Add {C} to your mana pool.
{4}{W}{U}, {T}: Prevent all damage a source of your choice would deal this turn.
*/
	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.extra = damage_card;
	td.required_type = 0;

	card_instance_t *instance= get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_PREVENT_DAMAGE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_XUW(5, 1, 1),
									0, &td, NULL) )
		{
			return 99;
		}
		else{
			return mana_producer(player, card, event);
		}
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int choice = CHOICE_MANA;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET |
														GAA_DAMAGE_PREVENTION, MANACOST_XUW(5, 1, 1), 0, &td, NULL))
		{
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Generate mana", 	1, 1,
							"Prevent damage", 	1, 5);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_PREVENT_DAMAGE ){
			add_state(player, card, STATE_TAPPED);
			if( charge_mana_for_activated_ability(player, card, MANACOST_XUW(4, 1, 1)) ){
				if( ! pick_target(&td, "TARGET_DAMAGE") ){
					untap_card_no_event(player, card);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PREVENT_DAMAGE ){
			if( valid_target(&td) ){
				card_instance_t *dmg = get_card_instance(instance->targets[0].player, instance->targets[0].card);
				int amount = dmg->info_slot;
				prevent_damage(instance->targets[0].player, instance->targets[0].card, amount);
				prevent_damage_until_eot(instance->parent_controller, instance->parent_card,
										dmg->damage_source_player, dmg->damage_source_card, -1);
			}
		}
	}

	return 0;
}

int card_pride_of_clouds(int player, int card, event_t event){
	/* Pride of the Clouds	|W|U
	 * Creature - Elemental Cat 1/1
	 * Flying
	 * ~ gets +1/+1 for each other creature with flying on the battlefield.
	 * Forecast - |2|W|U, Reveal ~ from your hand: Put a 1/1 |Swhite and |Sblue Bird creature token with flying onto the battlefield. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		this_test.keyword = KEYWORD_FLYING;
		event_result += check_battlefield_for_special_card(player, card, ANYBODY, CBFSC_GET_COUNT, &this_test);
	}

	if( ! IS_ACTIVATING_FROM_HAND(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND ){
		if( has_mana_multi(player, MANACOST_XUW(2, 1, 1)) && instance->info_slot != 1 && current_phase == PHASE_UPKEEP){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE_FROM_HAND ){
		charge_mana_multi(player, MANACOST_XUW(2, 1, 1));
		if( spell_fizzled != 1 ){
			instance->number_of_targets = 1;
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_BIRD, &token);
		token.color_forced = COLOR_TEST_WHITE | COLOR_TEST_BLUE;
		generate_token(&token);
	}

	return 0;
}

int card_proclamation_of_rebirth(int player, int card, event_t event){
/* Proclamation of Rebirth	|2|W
 * Sorcery
 * Return up to three target creature cards with converted mana cost 1 or less from your graveyard to the battlefield.
 * Forecast - |5|W, Reveal ~ from your hand: Return target creature card with converted mana cost 1 or less from your graveyard
		to the battlefield. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.cmc = 2;
		this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		if( has_mana_multi(player, MANACOST_XW(5, 1)) && get_card_instance(player, card)->info_slot != 1 &&
			current_phase == PHASE_UPKEEP && new_special_count_grave(player, &this_test) > 0 && ! graveyard_has_shroud(player) )
		{
			return ! graveyard_has_shroud(2);
		}
	}

	if( event == EVENT_ACTIVATE_FROM_HAND ){
		charge_mana_multi(player, MANACOST_XW(5, 1));
		if( spell_fizzled != 1 ){
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select up to 3 target creature cards with CMC 1 or less.");
		test.cmc = 2;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, 1, &test);
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select up to 3 target creature cards with CMC 1 or less.");
		test.cmc = 2;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;

		select_multiple_cards_from_graveyard(player, player, 0, AI_MAX_VALUE, &test, 3, &instance->targets[1]);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		// Move out of graveyard first, then animate one at a time, to prevent strange interactions with Gravedigger, Angel of Glory's Rise, etc.
		int i, num_validated = 0, num_targeted = 0, selected, dead[3];
		for (i = 0; i < 3; ++i){
			if (instance->targets[i + 1].card == -1){
				dead[i] = -1;
			} else {
				++num_targeted;
				if ((selected = validate_target_from_grave(player, card, player, i + 1)) == -1){
					dead[i] = -1;
				} else {
					dead[i] = get_grave(player)[selected];
					obliterate_card_in_grave(player, selected);
					++num_validated;
				}
			}
		}
		for (i = 0; i < 3; ++i){
			if (dead[i] != -1){
				int pos = raw_put_iid_on_top_of_graveyard(player, dead[i]);
				increase_trap_condition(player, TRAP_CARDS_TO_GY_FROM_ANYWHERE, -1);	// Since not really putting in graveyard
				reanimate_permanent(player, card, player, pos, REANIMATE_DEFAULT);
			}
		}

		if (num_validated == 0 && num_targeted != 0){
			spell_fizzled = 1;
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int proper_burial_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_PROPER_BURIAL ){
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
				count_for_gfp_ability_and_store_values(player, card, event, player, TYPE_CREATURE, NULL, GFPC_STORE_TOUGHNESS, 1);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		card_instance_t *instance = get_card_instance( player, card );
		if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			int amount = instance->targets[1].player;
			instance->targets[1].player = 0;
			gain_life(player, amount);
			kill_card(player, card, KILL_EXILE);
		}
	}

	//Reset counts if the legacy is still invisible
	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && event == EVENT_END_TRIGGER ){
		get_card_instance(player, card)->targets[11].player = 0;
		get_card_instance(player, card)->targets[1].player = 0;
	}

	if( event == EVENT_CLEANUP && ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_proper_burial(int player, int card, event_t event){
/* CARD_ID_PROPER_BURIAL	8260
Proper Burial	|3|W
Enchantment
Whenever a creature you control dies, you gain life equal to that creature's toughness. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)proper_burial_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &proper_burial_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	count_for_gfp_ability_and_store_values(player, card, event, player, TYPE_CREATURE, NULL, GFPC_STORE_TOUGHNESS, 0);

	if( get_card_instance(player, card)->kill_code <= 0 ){
		card_instance_t *instance = get_card_instance( player, card );
		if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			int amount = instance->targets[0].player;
			instance->targets[0].player = 0;
			gain_life(player, amount);
		}
	}

	return global_enchantment(player, card, event);
}

int card_protean_hulk(int player, int card, event_t event){
/* CARD_ID_PROTEAN_HULK	8261
Protean Hulk	|5|G|G
Creature - Beast 6/6
When ~ dies, search your library for any number of creature cards with total converted mana cost 6 or less and put them onto the battlefield.
Then shuffle your library. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		int max_cmc = 6;
		while( max_cmc > 0 ){
				char msg[100];
				scnprintf(msg, 100, "Select a creature card with CMC %d or less.", max_cmc);
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_CREATURE, msg);
				this_test.cmc = max_cmc+1;
				this_test.cmc_flag = 3;
				int crd = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
				if( crd != -1 ){
					max_cmc -= get_cmc(player, crd);
				}
				else{
					break;
				}
		}
	}

	return 0;
}

int card_psychic_possession(int player, int card, event_t event){

	/* Psychic Possession	|2|U|U
	 * Enchantment - Aura
	 * Enchant opponent
	 * Skip your draw step.
	 * Whenever enchanted opponent draws a card, you may draw a card. */

	skip_your_draw_step(player, event);

	if( event == EVENT_CAN_CAST ){
		return opponent_is_valid_target(player, card);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_opponent(player, card);
	}

	if( event == EVENT_RESOLVE_SPELL  ){
		if( ! opponent_is_valid_target(player, card) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( card_drawn_trigger(player, card, event, get_card_instance(player, card)->targets[0].player, RESOLVE_TRIGGER_AI(player)) ){
		draw_a_card(player);
	}

	return global_enchantment(player, card, event);
}

int card_psychotic_fury(int player, int card, event_t event){
/* CARD_ID_PSYCHOTIC_FURY	8263
Psychotic Fury	|1|R
Instant
Target multicolored creature gains double strike until end of turn.
Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int)is_multicolored;


	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_DOUBLE_STRIKE, 0);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target multicolored creature.", 1, NULL);
}

int card_pure_simple(int player, int card, event_t event){

	/* Pure // Simple
	 * |1|R|G|/|1|G|W
	 * Sorcery // Sorcery
	 * Destroy target multicolored permanent.
	 * //
	 * Destroy all Auras and Equipment. */

	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT );
		td.extra = (int32_t)is_multicolored;
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		return generic_split_card(player, card, event, can_target(&td), 0, MANACOST_XGW(1, 1, 1), 1, 0, 0, NULL, NULL);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_PLAY_FIRST_HALF ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT );
		td.extra = (int32_t)is_multicolored;
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		new_pick_target(&td, "Select target multicolored permanent.", 0, 1 | GS_LITERAL_PROMPT);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );
	td.extra = (int32_t)is_multicolored;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;

	int priority_simple = 5;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_AURA;
		this_test.sub2 = SUBTYPE_EQUIPMENT;
		this_test.subtype_flag = F2_MULTISUBTYPE;
		int my_p = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test);
		int opp_p = check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &this_test);
		priority_simple+=3*(opp_p-my_p);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 1) && valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( instance->info_slot & 2 ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_PERMANENT);
			this_test.subtype = SUBTYPE_AURA;
			this_test.sub2 = SUBTYPE_EQUIPMENT;
			this_test.subtype_flag = F2_MULTISUBTYPE;
			new_manipulate_all(player, card, ANYBODY, &this_test, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_split_card(player, card, event, can_target(&td), 10, MANACOST_XGW(1, 1, 1), 1, priority_simple, 0, NULL, NULL);
}

int card_raggamuffyn(int player, int card, event_t event){
/* CARD_ID_RAGAMUFFYN	8265
Ragamuffyn	|2|B
Creature - Zombie Cleric 2/2
Hellbent - |T, Sacrifice a creature or land: Draw a card. Activate this ability only if you have no cards in hand. */

	if( ! IS_GAA_EVENT(event) || hand_count[player] ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE | TYPE_LAND, "Select a creature or land to sacrifice");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL,
															0, &test, 0);
}

int card_rakdos_augermage(int player, int card, event_t event){
/* CARD_ID_RAKDOS_AUGERMAGE	8267
Rakdos Augermage	|B|B|R
Creature - Human Wizard 3/2
First strike
|T: Reveal your hand and discard a card of target opponent's choice. Then that player reveals his or her hand and discards a card of your choice.
Activate this ability only any time you could cast a sorcery. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);

			ec_definition_t this_definition;
			default_ec_definition(player, instance->targets[0].player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);

			default_ec_definition(instance->targets[0].player, player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_ONLY_TARGET_OPPONENT|GAA_CAN_SORCERY_BE_PLAYED,
									MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_rakdos_guildmage(int player, int card, event_t event){
/*
Rakdos Guildmage  {B/R}{B/R}
Creature — Zombie Shaman 2/2
{3}{B}, Discard a card: Target creature gets -2/-2 until end of turn.
{3}{R}: Put a 2/1 red Goblin creature token with haste onto the battlefield. Exile it at the beginning of the next end step.
*/
	hybrid(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_M2_M2 = 1,
		CHOICE_GOBLIN
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DISCARD, MANACOST_XB(3, 1), 0, &td, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, 0, MANACOST_XR(3, 1), 0, NULL, NULL) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_DISCARD, MANACOST_XB(3, 1), 0,
												&td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XR(3, 1), 0, NULL, NULL)
		};
		int priorities[3] = {
							0,
							current_phase >= PHASE_AFTER_BLOCKING ? 5 : 1,
							current_phase <= PHASE_DECLARE_BLOCKERS ? 5 : 1,
		};
		int choice = DIALOG(player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_NO_CANCEL,
							"Weaken a creature", abils[1], priorities[1],
							"Generate a Goblin", abils[2], priorities[2]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_M2_M2 ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DISCARD, MANACOST_XB(3, 1), 0, &td, "TARGET_CREATURE");
		}
		if( choice == CHOICE_GOBLIN ){
			return generic_activated_ability(player, card, event, 0, MANACOST_XR(3, 1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_M2_M2 && valid_target(&td)){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, -2, -2);
		}
		if( instance->info_slot == CHOICE_GOBLIN ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_GOBLIN, &token);
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_RED);
			token.pow = 2;
			token.tou = 1;
			token.special_code_for_legacy = &haste_and_remove_eot;
			generate_token(&token);
		}
	}

	return 0;
}

// goblin token --> 0x2006e16

int card_rakdos_ickspitter(int player, int card, event_t event){
/* CARD_ID_RAKDOS_ICKSPITTER	8269
Rakdos Ickspitter	|1|B|R
Creature - Thrull 1/1
|T: ~ deals 1 damage to target creature and that creature's controller loses 1 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
			lose_life(instance->targets[0].player, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_NONSICK, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_rakdos_pit_dragon(int player, int card, event_t event){
/*
Rakdos Pit Dragon |2|R|R
Creature — Dragon 3/3
{R}{R}: Rakdos Pit Dragon gains flying until end of turn.
{R}: Rakdos Pit Dragon gets +1/+0 until end of turn.
Hellbent — Rakdos Pit Dragon has double strike as long as you have no cards in hand.
*/
	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card) ){
		if( hand_count[player] < 1){
			event_result |= KEYWORD_DOUBLE_STRIKE;
		}
	}

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( get_abilities(player, card, EVENT_ABILITIES, -1) & KEYWORD_FLYING ){
			return generic_shade(player, card, event, 0, MANACOST_R(1), 1, 0, 0, 0);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_FLYING = 1,
		CHOICE_P1_P0
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_R(1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int prev_activation = instance->targets[1].player > -1 ? instance->targets[1].player : 0;
		prev_activation |= get_abilities(player, card, EVENT_ABILITIES, -1);
		int priorities[3] = {
							0,
							!(prev_activation & KEYWORD_FLYING) && current_phase <= PHASE_DECLARE_BLOCKERS ? 10 : 1,
							current_phase == PHASE_AFTER_BLOCKING ? 10 : 1
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Flying until EOT", has_mana_for_activated_ability(player, card, MANACOST_R(2)), priorities[1],
							"+1/+0 until EOT", 1, priorities[2]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_R(choice == CHOICE_P1_P0 ? 1 : 2)) ){
			if( choice == CHOICE_FLYING ){
				instance->targets[1].player = KEYWORD_FLYING;
			}
		}
	}

    if( event == EVENT_RESOLVE_ACTIVATION ){
		int paco = instance->parent_controller;
		int paca = instance->parent_card;
		if( instance->info_slot == CHOICE_FLYING ){
			alternate_legacy_text(1, player, pump_ability_until_eot(paco, paca, paco, paca, 0, 0, KEYWORD_FLYING, 0));
		}
		if( instance->info_slot == CHOICE_P1_P0 ){
			return alternate_legacy_text(2, player, pump_until_eot_merge_previous(paco, paca, paco, paca, 1, 0));
		}
	}

	return 0;
}

int card_rakdos_riteknife(int player, int card, event_t event){

	/* Rakdos Riteknife	|2
	 * Artifact - Equipment
	 * Equipped creature gets +1/+0 for each blood counter on ~ and has "|T, Sacrifice a creature: Put a blood counter on ~."
	 * |B|R, Sacrifice ~: Target player sacrifices a permanent for each blood counter on ~.
	 * Equip |2 */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if (is_equipping(player, card) && ! is_humiliated(player, card) ){
		modify_pt_and_abilities(instance->targets[8].player, instance->targets[8].card, event, count_counters(player, card, COUNTER_BLOOD), 0, 0);
	}

	enum{
		CHOICE_EQUIP = 1,
		CHOICE_SAC_CREATURE,
		CHOICE_SAC_KNIFE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( is_equipping(player, card) && generic_activated_ability(instance->targets[8].player, instance->targets[8].card, event, GAA_UNTAPPED | GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_BR(1, 1), 0, &td1, NULL) ){
			return 1;
		}
		return can_activate_basic_equipment(player, card, event, 2);
	}

	if( event == EVENT_ACTIVATE ){
		int can_sacrifice_creature = is_equipping(player, card) &&
										generic_activated_ability(instance->targets[8].player, instance->targets[8].card, EVENT_CAN_ACTIVATE,
																	GAA_UNTAPPED | GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL);
		int can_sacrifice_knife = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_BR(1, 1), 0, &td1, NULL);

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Equip", can_activate_basic_equipment(player, card, event, 2), ! is_equipping(player, card) ? 15 : 0,
							"Sac a creature", can_sacrifice_creature, 3*count_subtype(player, TYPE_CREATURE, -1),
							"Sac the knife", can_sacrifice_knife, 4*count_counters(player, card, COUNTER_BLOOD));
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_EQUIP ){
			activate_basic_equipment(player, card, 2);
			if( spell_fizzled != 1 ){
				instance->info_slot = choice;
			}
		}
		if( choice == CHOICE_SAC_CREATURE ){
			if( sacrifice(instance->targets[8].player, instance->targets[8].card, player, 0, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0) ){
				tap_card(instance->targets[8].player, instance->targets[8].card);
				instance->info_slot = choice;
			}
			else{
				spell_fizzled = 1;
			}
		}
		if( choice == CHOICE_SAC_KNIFE ){
			if( pick_target(&td1, "TARGET_PLAYER") ){
				instance->info_slot = choice+count_counters(player, card, COUNTER_BLOOD);
				kill_card(player, card, KILL_SACRIFICE);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_EQUIP ){
			resolve_activation_basic_equipment(player, card);
		}
		if( instance->info_slot == CHOICE_SAC_CREATURE ){
			add_counter(instance->parent_controller, instance->parent_card, COUNTER_BLOOD);
		}
		if( instance->info_slot == CHOICE_SAC_KNIFE && valid_target(&td1) ){
			int amount = instance->info_slot-CHOICE_SAC_KNIFE;
			impose_sacrifice(player, card, instance->targets[0].player, amount, TYPE_PERMANENT, 0, 0, 0, 0, 0, 0, 0, -1, 0);
		}
	}

	return 0;
}

int card_rakdos_the_defiler(int player, int card, event_t event){
/*
Rakdos the Defiler |2|B|B|R|R
Legendary Creature — Demon 7/6
Flying, trample
Whenever Rakdos the Defiler attacks, sacrifice half the non-Demon permanents you control, rounded up.
Whenever Rakdos deals combat damage to a player, that player sacrifices half the non-Demon permanents he or she controls, rounded up.
*/
	check_legend_rule(player, card, event);

	if( declare_attackers_trigger(player, card, event, 0, player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a non-Demon permanent to sacrifice.");
		test.subtype = SUBTYPE_DEMON;
		test.subtype_flag = DOESNT_MATCH;
		int amount = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test);
		amount = (amount + 1) / 2;
		if( amount > 0 ){
			test.qty = amount;
			new_sacrifice(player, card, player, SAC_NO_CANCEL | SAC_CAUSED, &test);
		}
	}

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_OPPONENT) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		APNAP(p,{
					if( times_damaged[p] ){
						test_definition_t test;
						new_default_test_definition(&test, TYPE_PERMANENT, "Select a non-Demon permanent to sacrifice.");
						test.subtype = SUBTYPE_DEMON;
						test.subtype_flag = DOESNT_MATCH;
						int amount = check_battlefield_for_special_card(player, card, p, CBFSC_GET_COUNT, &test);
						amount = (amount + 1) / 2;
						if( amount > 0 ){
							test.qty = amount;
							new_sacrifice(player, card, p, SAC_NO_CANCEL | SAC_CAUSED, &test);
						}
					}
				}
		);
	}

	return 0;
}

int card_ratcatcher(int player, int card, event_t event){
/* CARD_ID_RATCATCHER	8273
Ratcatcher	|4|B|B
Creature - Ogre Rogue 4/4
Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
At the beginning of your upkeep, you may search your library for a Rat card, reveal it, and put it into your hand. If you do, shuffle your library. */

	fear(player, card, event);

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a Rat card.");
		this_test.subtype = SUBTYPE_RAT;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return 0;
}

int card_riot_spikes(int player, int card, event_t event){
/* CARD_ID_RIOT_SPIKES	8275
Riot Spikes	|BR
Enchantment - Aura
(|BR can be paid with either |B or |R.)
Enchant creature
Enchanted creature gets +2/-1. */

	hybrid(player, card, event);
	return generic_aura(player, card, event, player, 2, -1, 0, 0, 0, 0, 0);
}

int card_rix_maadi_dungeon_palace(int player, int card, event_t event){
/* CARD_ID_RIX_MAADI_DUNGEON_PALACE	8277
Rix Maadi, Dungeon Palace	""
Land
|T: Add |C to your mana pool.
|1|B|R, |T: Each player discards a card. Activate this ability only any time you could cast a sorcery. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int choice = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_XBR(2, 1, 1), 0, NULL, NULL) && can_sorcery_be_played(player, event) ){
			choice = do_dialog(player, player, card, -1, -1, " Generate mana\n Both playes discard\n Cancel", 1);
		}

		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		else if( choice == 1 ){
				add_state(player, card, STATE_TAPPED);
				charge_mana_for_activated_ability(player, card, MANACOST_XBR(1, 1, 1));
				if( spell_fizzled != 1 ){
					instance->info_slot = 1;
					tap_card(player, card);
				}
				else{
					untap_card_no_event(player, card);
				}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			APNAP(p, {discard(p, 0, player);};);
		}
	}

	return 0;
}

static int simic_basilisk_legacy(int player, int card, event_t event){
// Whenever this creature deals combat damage to a creature, destroy that creature at end of combat.
	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		card_instance_t* damage = damage_being_dealt(event);

		if( damage ){
			if( damage->damage_source_player == p && damage->damage_source_card == c &&
				is_what(damage->damage_target_player, damage->damage_target_card, TYPE_CREATURE) )
			{
				if( instance->targets[0].player < 1 ){
					instance->targets[0].player = 1;
				}
				int pos = instance->targets[0].player;
				if( pos < 19 ){
					instance->targets[pos].player = damage->damage_target_player;
					instance->targets[pos].card = damage->damage_target_card;
					instance->targets[0].player++;
				}
			}
		}

		if( trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(p, c) && reason_for_trigger_controller == p )
		{
			int pos = instance->targets[0].player;
			if( pos > 1 ){
				if( event == EVENT_TRIGGER ){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						int i;
						for(i=1; i<pos; i++){
							if( in_play(instance->targets[i].player, instance->targets[i].card) ){
								create_targetted_legacy_effect( p, c, &die_at_end_of_combat,
																instance->targets[i].player, instance->targets[i].card);
							}
						}
						instance->targets[0].player = 1;
				}
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_simic_basilisk(int player, int card, event_t event){
/* Simic Basilisk	|4|G|G
 * Creature - Basilisk Mutant 0/0
 * Graft 3
 * |1|G: Until end of turn, target creature with a +1/+1 counter on it gains
		"Whenever this creature deals combat damage to a creature, destroy that creature at end of combat." */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);

	graft_trigger(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_P1_P1;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int legacy = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &simic_basilisk_legacy,
														instance->targets[0].player, instance->targets[0].card);
			card_instance_t *leg = get_card_instance( player, legacy );
			leg->targets[0].player = leg->targets[0].card = -1;
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XG(1, 1), 0,
									&td, "Select target creature with a +1/+1 counter.");
}

static const char* can_move_aura_same_controller(int who_chooses, int player, int card){
	int rvalue = 0;
	set_special_flags(player, card, SF_MOVE_AURA_SAME_CONTROLLER);
	rvalue = call_card_function(player, card, EVENT_CAN_MOVE_AURA);
	remove_special_flags(player, card, SF_MOVE_AURA_SAME_CONTROLLER);
	return rvalue ? NULL : "cannot move this Aura.";
}

int card_simic_guildmage(int player, int card, event_t event){

	/* Simic Guildmage	|GU|GU
	 * Creature - Elf Wizard 2/2
	 * |1|G: Move a +1/+1 counter from target creature onto another target creature with the same controller.
	 * |1|U: Attach target Aura enchanting a permanent to another permanent with the same controller. */

	hybrid(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_P1_P1;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.preferred_controller = player;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_ENCHANTMENT);
	td1.required_subtype = SUBTYPE_AURA_CREATURE;
	td1.preferred_controller = player;
	td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td1.extra = (int)can_move_aura_same_controller;

	card_instance_t *instance = get_card_instance( player, card );


	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XG(1, 1), 0, &td, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XU(1, 1), 0, &td1, NULL) ){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		int choice = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_XG(1, 1), 0, &td, NULL) ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_XU(1, 1), 0, &td1, NULL) ){
				choice = do_dialog(player, player, card, -1, -1, " Move a +1/+1 counter\n Move an Enchant Creature\n Cancel", 1);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 2 ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_XUG(1, choice == 1, choice == 0)) ){
			instance->number_of_targets = 0;
			if( choice == 0 ){
				if( new_pick_target(&td, "Select target creature with a +1/+1 counter.", 0, 1 | GS_LITERAL_PROMPT) ){
					state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);

					td2.allowed_controller = td2.preferred_controller = instance->targets[0].player;

					if( can_target(&td2) && select_target(player, card, &td2, "Select target creature with the same controller", &(instance->targets[1])) ){
						instance->info_slot = 66+choice;
					}
					else{
						spell_fizzled = 1;
					}
					state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
				}
			}
			if( choice == 1 ){
				if( new_pick_target(&td1, "Select target Enchant Creature to move.", 0, 1 | GS_LITERAL_PROMPT) ){
					set_special_flags(instance->targets[0].player, instance->targets[0].card, SF_MOVE_AURA_SAME_CONTROLLER);
					card_instance_t *targ = get_card_instance( instance->targets[0].player, instance->targets[0].card );
					CardFunction ptFunction = cards_data[targ->internal_card_id].code_pointer;
					if( ptFunction(instance->targets[0].player, instance->targets[0].card, EVENT_MOVE_AURA) ){
						instance->info_slot = 66+choice;
					}
					else{
						remove_special_flags(instance->targets[0].player, instance->targets[0].card, SF_MOVE_AURA_SAME_CONTROLLER);
						spell_fizzled = 1;
					}
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 ){
			td2.allowed_controller = instance->targets[0].player;
			if( valid_target(&td) && validate_target(player, card, &td2, 1) ){
				move_counters(instance->targets[1].player, instance->targets[1].card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
			}
		}
		if( instance->info_slot == 67 ){
			if( valid_target(&td1) ){
				card_instance_t *targ = get_card_instance( instance->targets[0].player, instance->targets[0].card );
				CardFunction ptFunction = cards_data[targ->internal_card_id].code_pointer;
				ptFunction(instance->targets[0].player, instance->targets[0].card, EVENT_RESOLVE_MOVING_AURA);
				remove_special_flags(instance->targets[0].player, instance->targets[0].card, SF_MOVE_AURA_SAME_CONTROLLER);
			}
			else{
				if( in_play(instance->targets[0].player, instance->targets[0].card) ){
					remove_special_flags(instance->targets[0].player, instance->targets[0].card, SF_MOVE_AURA_SAME_CONTROLLER);
				}
			}
		}
	}

	return 0;
}

int card_simic_initiate(int player, int card, event_t event){
/* CARD_ID_SIMIC_INITIATE	8283
Simic Initiate	|G
Creature - Human Mutant 0/0
Graft 1 (This creature enters the battlefield with a +1/+1 counter on it. Whenever another creature enters the battlefield,
you may move a +1/+1 counter from this creature onto it.) */

	return graft(player, card, event, 1);
}

int card_sky_hussar(int player, int card, event_t event){
	/* Sky Hussar	|3|W|U
	 * Creature - Human Knight 4/3
	 * Flying
	 * When ~ enters the battlefield, untap all creatures you control.
	 * Forecast - Tap two untapped |Swhite and/or |Sblue creatures you control, Reveal ~ from your hand: Draw a card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
	}

	if( ! IS_ACTIVATING_FROM_HAND(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_color = COLOR_TEST_WHITE | COLOR_TEST_BLUE;
	td.illegal_abilities = 0;

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND ){
		if( can_target(&td) && instance->info_slot != 1 && current_phase == PHASE_UPKEEP){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE_FROM_HAND ){
		if( pick_target(&td, "TARGET_CREATURE") ){
			instance->number_of_targets = 1;
			tap_card(instance->targets[0].player, instance->targets[0].card);
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_skyscribing(int player, int card, event_t event){
	/* Skyscribing	|X|U|U
	 * Sorcery
	 * Each player draws X cards.
	 * Forecast - |2|U, Reveal ~ from your hand: Each player draws a card. */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_ACTIVATE_FROM_HAND ){
			if( has_mana_multi(player, MANACOST_XU(2, 1)) && instance->info_slot != 1 && current_phase == PHASE_UPKEEP ){
				return 1;
			}
		}

		if( event == EVENT_ACTIVATE_FROM_HAND ){
			charge_mana_multi(player, MANACOST_XU(2, 1));
			if( spell_fizzled != 1 ){
				return 1;
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
			draw_cards(player, 1);
			draw_cards(1-player, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, get_card_instance(player, card)->info_slot);
		draw_cards(1-player, get_card_instance(player, card)->info_slot);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_slithering_shade(int player, int card, event_t event){
/* CARD_ID_SLITHERING_SHADE	8289
Slithering Shade	|B
Creature - Shade 0/1
Defender (This creature can't attack.)
|B: ~ gets +1/+1 until end of turn.
Hellbent - ~ can attack as though it didn't have defender as long as you have no cards in hand. */

	if (event == EVENT_ABILITIES && affect_me(player, card) && hand_count[player] <= 0 && ! is_humiliated(player, card) ){
		get_card_instance(player, card)->token_status |= STATUS_WALL_CAN_ATTACK;
	}

	return generic_shade(player, card, event, 0, MANACOST_B(1), 1, 1, 0, 0);
}

int card_soulsworn_jury(int player, int card, event_t event){
/* CARD_ID_SOULSWORN_JURY	8290
Soulsworn Jury	|2|W
Creature - Spirit 1/4
Defender (This creature can't attack.)
|1|U, Sacrifice ~: Counter target creature spell. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		real_counter_a_spell(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_SPELL_ON_STACK, MANACOST_XU(1, 1), 0, &td, NULL);
}

static const char* cmc_is_2(int who_chooses, int player, int card){
	if( get_cmc(player, card) == 2){
		return NULL;
	}
	return "CMC must be 2.";
}

int card_spell_snare(int player, int card, event_t event){
/* CARD_ID_SPELL_SNARE	8291
Spell Snare	|U
Instant
Counter target spell with converted mana cost 2. */

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int)cmc_is_2;

	return counterspell(player, card, event, &td, 0);
}

int card_sporeback_troll(int player, int card, event_t event){

	/* Sporeback Troll	|3|G
	 * Creature - Troll Mutant 0/0
	 * Graft 2
	 * |1|G: Regenerate target creature with a +1/+1 counter on it. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);

	graft_trigger(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_DESTROYED;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER | (player == AI ? TARGET_SPECIAL_REGENERATION : 0);
	td.extra = COUNTER_P1_P1;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT | GAA_REGENERATION, MANACOST_XG(1, 1), 0,
									&td, "Select target creature with a +1/+1 counter to regenerate.");
}

int card_sprouting_phytohydra(int player, int card, event_t event){
	/* Sprouting Phytohydra	|4|G
	 * Creature - Plant Hydra 0/2
	 * Defender
	 * Whenever ~ is dealt damage, you may put a token that's a copy of ~ onto the battlefield. */

	if( ! is_humiliated(player, card) && damage_dealt_to_me_arbitrary(player, card, event, DDBM_TRIGGER_OPTIONAL, player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		token_generation_t token;
		copy_token_definition(player, card, &token, player, card);
		token.qty = instance->targets[7].player;
		instance->targets[7].player = 0;
		generate_token(&token);
	}

	return 0;
}

int card_squealing_devil(int player, int card, event_t event){
/* CARD_ID_SQUEALING_DEVIL	8294
Squealing Devil	|1|R
Creature - Devil 2/1
Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
When ~ enters the battlefield, you may pay |X. If you do, target creature gets +X/+0 until end of turn.
When ~ enters the battlefield, sacrifice it unless |B was spent to cast it. */

	ravnica_manachanger(player, card, event, 1, 0, 0, 0, 0);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && get_card_instance( player, card )->info_slot == 1){
			get_card_instance( player, card )->targets[1].player = charge_changed_mana(player, card, 1, 0, 0, 0, 0);
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		if( has_mana(player, COLOR_COLORLESS, 1) && can_target(&td) ){
			charge_mana(player, COLOR_COLORLESS, -1);
			if( spell_fizzled != 1 ){
				int amount = x_value;
				if( pick_target(&td, "TARGET_CREATURE") ){
					pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, amount, 0);
				}
			}
		}
		if( instance->targets[1].player != 2 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

static int stalking_vengeance_effect(int player, int card, event_t event){

	//In case something is copying Pawn of Ulamog and then reverts to its original form.
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_STALKING_VENGEANCE ){
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
						return 0;
					}
				}
				count_for_gfp_ability_and_store_values(player, card, event, player, TYPE_CREATURE, NULL, GFPC_STORE_POWER, 1);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.zone = TARGET_ZONE_PLAYERS;
			td.allow_cancel = 0;

			card_instance_t *instance = get_card_instance( player, card );
			int amount = instance->targets[1].player;
			instance->targets[1].player = 0;

			if( resolve_gfp_ability(player, card, event, can_target(&td) ? RESOLVE_TRIGGER_MANDATORY : 0) ){
				if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 1, 0) ){
					damage_player(instance->targets[1].player, amount, player, card);
				}
			}
		}
	}

	//Reset counts if the legacy is still invisible
	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && event == EVENT_END_TRIGGER ){
		get_card_instance(player, card)->targets[11].player = 0;
		get_card_instance(player, card)->targets[1].player = 0;
	}

	if( event == EVENT_CLEANUP && ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_stalking_vengeance(int player, int card, event_t event){
	/* Stalking Vengeance	|5|R|R	0x2006e70
	 * Creature - Avatar 5/5
	 * Haste
	 * Whenever another creature you control dies, it deals damage equal to its power to target player or planeswalker. */
	haste(player, card, event);

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)stalking_vengeance_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &stalking_vengeance_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability_and_store_values(player, card, event, player, TYPE_CREATURE, NULL, GFPC_STORE_POWER, 1);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			td.allow_cancel = 0;

			card_instance_t *instance = get_card_instance( player, card );
			int amount = instance->targets[1].player;
			instance->targets[1].player = 0;

			if( resolve_gfp_ability(player, card, event, can_target(&td) ? RESOLVE_TRIGGER_MANDATORY : 0) ){
				if( can_target(&td) && pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER") ){
					damage_target0(player, card, amount);
				}
			}

			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_stomp_and_howl(int player, int card, event_t event){
/* CARD_ID_STOMP_AND_HOWL	8298
Stomp and Howl	|2|G
Sorcery
Destroy target artifact and target enchantment. */

	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_ENCHANTMENT);

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td) ){
		return can_target(&td1);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_ARTIFACT") ){
			new_pick_target(&td1, "TARGET_ENCHANTMENT", 1, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( validate_target(player, card, &td1, 1) ){
			kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_stormscale_anarch(int player, int card, event_t event){
	/* Stormscale Anarch	|2|R|R	0x2006e7a
	 * Creature - Viashino Shaman 2/2
	 * |2|R, Discard a card at random: ~ deals 2 damage to any target. If the discarded card was multicolored, ~ deals 4 damage to that permanent or player instead. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DISCARD, MANACOST_XR(2, 1), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_XR(2, 1)) && pick_target(&td, "TARGET_ANY") ){
			instance->number_of_targets = 1;
			instance->info_slot = 2;
			int hand_index = 0;
			int count = 0;
			int my_hand[100];
			while( count < active_cards_count[player] ){
					if( in_hand(player, count) ){
						my_hand[hand_index] = count;
						hand_index++;
					}
					count++;
			}
			int to_discard = internal_rand(hand_index);
			if( count_colors(player, my_hand[to_discard]) > 1 ){
				instance->info_slot+=2;
			}
			discard_card(player, my_hand[to_discard]);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
	}

	return 0;
}

int card_taste_for_mayhem(int player, int card, event_t event){
/* CARD_ID_TASTE_FOR_MAYHEM	8303
Taste for Mayhem	|R
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+0.
Hellbent - Enchanted creature gets an additional +2/+0 as long as you have no cards in hand. */

	int amount = 2;
	if( hand_count[player] < 1 ){
		amount = 4;
	}
	return generic_aura(player, card, event, player, amount, 0, 0, 0, 0, 0, 0);
}

int card_tidespout_tyrant(int player, int card, event_t event){
/* CARD_ID_TIDESPOUT_TYRANT	8304
Tidespout Tyrant	|5|U|U|U
Creature - Djinn 5/5
Flying
Whenever you cast a spell, return target permanent to its owner's hand. */

	if( trigger_condition == TRIGGER_SPELL_CAST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;

		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
			card_instance_t *instance = get_card_instance(player, card);
			if( pick_target(&td, "TARGET_PERMANENT") ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_trygon_predator(int player, int card, event_t event){
/*
Trygon Predator |1|U|G
Creature — Beast 2/3
Flying
Whenever Trygon Predator deals combat damage to a player, you may destroy target artifact or enchantment that player controls.
*/

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		APNAP(p,{
					if( times_damaged[p] ){
						target_definition_t td;
						default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);
						td.allowed_controller = td.preferred_controller = p;
						if( can_target(&td) ){
							if( p == player ){
								if( new_pick_target(&td, "Select target artifact or enchantment you control.", 0, GS_LITERAL_PROMPT) ){
									action_on_target(player, card, 0, KILL_DESTROY);
								}
							}
							if( p != player ){
								if( new_pick_target(&td, "Select target artifact or enchantment controlled by opponent.", 0,
													GS_LITERAL_PROMPT))
								{
									action_on_target(player, card, 0, KILL_DESTROY);
								}
							}
						}
					}
					instance->number_of_targets = 0;
				}
		);
	}

	return 0;
}

int card_twinstrike(int player, int card, event_t event){
/* CARD_ID_TWINSTRIKE	8308
Twinstrike	|3|B|R
Instant
~ deals 2 damage to each of two target creatures.
Hellbent - Destroy those creatures instead if you have no cards in hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				if( hand_count[player] < 1 ){
					kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
				}
				else{
					damage_creature(instance->targets[i].player, instance->targets[i].card, 2, player, card);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

int card_unliving_psychopath(int player, int card, event_t event){
	/* Unliving Psychopath	|2|B|B
	 * Creature - Zombie Assassin 0/4
	 * |B: ~ gets +1/-1 until end of turn.
	 * |B, |T: Destroy target creature with power less than ~'s power. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	int pwr = get_power(player, card);
	if (pwr > 0){
		td.power_requirement = (pwr - 1) | TARGET_PT_LESSER_OR_EQUAL;
	} else {
		td.power_requirement = TARGET_PT_LESSER_OR_EQUAL | TARGET_PT_GREATER_OR_EQUAL | TARGET_PT_MASK;	// always impossible
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int choice = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1), 0, &td, "") ){
			choice = do_dialog(player, player, card, -1, -1, " Pump Unliving Psychopath\n Destroy a creature\n Cancel", 1);
		}
		if( choice == 2 ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_B(1)) ){
			instance->number_of_targets = 0;
			if( choice == 1 ){
				new_pick_target(&td, "Select target creature with power less than Unliving Psychopath's power.", 0, 1 | GS_LITERAL_PROMPT);
				if (cancel != 1){
					instance->state |= STATE_TAPPED;	// no, not tap_card(), never tap_card() during something's own EVENT_ACTIVATE
				}
			}
			instance->info_slot = 66+choice;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 ){
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 1, -1);
		}
		if( instance->info_slot == 67 && valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_utopia_sprawl(int player, int card, event_t event){
/* CARD_ID_UTOPIA_SPRAWL	8310
Utopia Sprawl	|G
Enchantment - Aura
Enchant |H2Forest
As ~ enters the battlefield, choose a color.
Whenever enchanted |H2Forest is tapped for mana, its controller adds one mana of the chosen color to his or her mana pool (in addition to the mana the land produces). */

	card_instance_t* instance = get_card_instance(player, card);
	int result = wild_growth_aura(player, card, event, get_hacked_subtype(player, card, SUBTYPE_FOREST), instance->targets[2].player, instance->targets[2].card);
	if (comes_into_play(player, card, event)){
		instance->targets[2].player = choose_a_color(player, get_deck_color(player, player));
		instance->targets[2].card = 1;
		instance->info_slot = 1 << instance->targets[2].player;
	}
	return result;
}

int card_valor_made_real(int player, int card, event_t event){
/* CARD_ID_VALOR_MADE_REAL	8312
Valor Made Real	|W
Instant
Target creature can block any number of creatures this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	if (event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td)){
			card_instance_t* instance = get_card_instance(player, card);
			can_block_additional_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 255);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_vigean_graftmage(int player, int card, event_t event){

	/* Vigean Graftmage	|2|U
	 * Creature - Vedalken Wizard Mutant 0/0
	 * Graft 2
	 * |1|U: Untap target creature with a +1/+1 counter on it. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);

	graft_trigger(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_P1_P1;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XU(1, 1), 0,
									&td, "Select target creature with a +1/+1 counter.");
}

int card_vigean_hydropon(int player, int card, event_t event){
/* CARD_ID_VIGEAN_HYDROPON	8316
Vigean Hydropon	|1|G|U
Creature - Plant Mutant 0/0
Graft 5 (This creature enters the battlefield with five +1/+1 counters on it.
Whenever another creature enters the battlefield, you may move a +1/+1 counter from this creature onto it.)
~ can't attack or block. */

	cannot_attack(player, card, event);

	cannot_block(player, card, event);

	return graft(player, card, event, 5);
}

int card_vision_skeins(int player, int card, event_t event){
/* CARD_ID_VISION_SKEINS	8318
Vision Skeins	|1|U
Instant
Each player draws two cards. */

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		draw_cards(1-player, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_voidslime(int player, int card, event_t event){
/* CARD_ID_VOIDSLIME	8319
Voidslime	|G|U|U
Instant
Counter target spell, activated ability, or triggered ability. (Mana abilities can't be targeted.) */

  card_instance_t* instance = get_card_instance(player, card);

  target_definition_t td_spell;
  counterspell_target_definition(player, card, &td_spell, 0);

  if (event == EVENT_CAN_CAST)
	{
	  if (counterspell(player, card, event, &td_spell, 0))
		{
		  instance->info_slot = 1;
		  return 99;
		}
	  if (can_counter_activated_ability(player, card, event, NULL))
		{
		  instance->info_slot = 2;
		  return 99;
		}
	  return 0;
	}
  else if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  instance->number_of_targets = 0;
	  if (instance->info_slot == 1)
		counterspell(player, card, event, &td_spell, 0);
	  else if (instance->info_slot == 2)
		cast_counter_activated_ability(player, card, 0);
	}
  else if (event == EVENT_RESOLVE_SPELL)
	{
	  if (instance->info_slot == 1)
		counterspell(player, card, event, &td_spell, 0);	// destroys card
	  else if (instance->info_slot == 2 && resolve_counter_activated_ability(player, card, NULL, 0))
		kill_card(player, card, KILL_DESTROY);
	}
  else
	return counterspell(player, card, event, &td_spell, 0);

  return 0;
}

static int effect_wakestone_gargoyle(int player, int card, event_t event){
  if (event == EVENT_ABILITIES
	  && affected_card_controller == player
	  && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
	  && in_play(affected_card_controller, affected_card))
	add_status(affected_card_controller, affected_card, STATUS_WALL_CAN_ATTACK);

  if (event == EVENT_CLEANUP && affect_me(player, card))
	kill_card(player, card, KILL_EXILE);

  return 0;
}

int card_wakestone_gargoyle(int player, int card, event_t event){
/* CARD_ID_WAKESTONE_GARGOYLE	8320
Wakestone Gargoyle	|3|W
Creature - Gargoyle 3/4
Defender, flying
|1|W: Creatures you control with defender can attack this turn as though they didn't have defender. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  create_legacy_effect(instance->parent_controller, instance->parent_card, effect_wakestone_gargoyle);
	}

  return generic_activated_ability(player, card, event, 0, MANACOST_XW(1, 1), 0, 0, 0);
}

int card_windreaver(int player, int card, event_t event){
/* CARD_ID_WINDREAVER	8325
Windreaver	|3|W|U
Creature - Elemental 1/3
Flying
|W: ~ gains vigilance until end of turn.
|W: ~ gets +0/+1 until end of turn.
|U: Switch ~'s power and toughness until end of turn.
|U: Return ~ to its owner's hand. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_U(1), 0, NULL, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, 0, MANACOST_W(1), 0, NULL, NULL) ){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		int priorities[4] = {	current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS && ! check_for_special_ability(player, card, SP_KEYWORD_VIGILANCE) ? 15 : 0,
								8,
								is_attacking(player, card) && is_unblocked(player, card) ? 10 : 0,
								5
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
						"Give Vigilance", has_mana_for_activated_ability(player, card, MANACOST_W(1)), priorities[0],
						"Pump toughness", has_mana_for_activated_ability(player, card, MANACOST_W(1)), priorities[1],
						"Switch P/T", has_mana_for_activated_ability(player, card, MANACOST_U(1)), priorities[2],
						"Return to hand", has_mana_for_activated_ability(player, card, MANACOST_U(1)), priorities[3]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_UW((choice == 2 || choice == 3), (choice == 0 || choice == 1))) ){
			instance->info_slot = 66+choice;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 ){
			pump_ability_until_eot(player, instance->parent_card, player, instance->parent_card, 0, 0, 0, SP_KEYWORD_VIGILANCE);
		}
		if( instance->info_slot == 67 ){
			pump_until_eot(player, instance->parent_card, player, instance->parent_card, 0, 1);
		}
		if( instance->info_slot == 68 ){
			switch_power_and_toughness_until_eot(player, card, instance->parent_controller, instance->parent_card);
		}
		if( instance->info_slot == 69 ){
			bounce_permanent(player, instance->parent_card);
		}
	}

	return 0;
}

int card_wrecking_ball(int player, int card, event_t event){
/* CARD_ID_WRECKING_BALL	1278
Wrecking Ball	|2|B|R
Instant
Destroy target creature or land. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_LAND);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature or land.", 1, NULL);
}

int card_rakdos_carnarium(int player, int card, event_t event){
/* CARD_ID_RAKDOS_CARNARIUM	1731
Rakdos Carnarium	""
Land
~ enters the battlefield tapped.
When ~ enters the battlefield, return a land you control to its owner's hand.
|T: Add |B|R to your mana pool. */

	return karoo( player, card, event, COLOR_RED, COLOR_BLACK, CARD_ID_BADLANDS );
}

int card_simic_growth_chamber(int player, int card, event_t event){
/* CARD_ID_SIMIC_GROWTH_CHAMBER	1732
Simic Growth Chamber	""
Land
~ enters the battlefield tapped.
When ~ enters the battlefield, return a land you control to its owner's hand.
|T: Add |G|U to your mana pool. */

	return karoo( player, card, event, COLOR_GREEN, COLOR_BLUE, CARD_ID_TROPICAL_ISLAND );
}

int card_orzhov_basilica(int player, int card, event_t event){
/* CARD_ID_ORZHOV_BASILICA	1551
Orzhov Basilica	""
Land
~ enters the battlefield tapped.
When ~ enters the battlefield, return a land you control to its owner's hand.
|T: Add |W|B to your mana pool. */

	return karoo( player, card, event, COLOR_WHITE, COLOR_BLACK, CARD_ID_SCRUBLAND );
}

