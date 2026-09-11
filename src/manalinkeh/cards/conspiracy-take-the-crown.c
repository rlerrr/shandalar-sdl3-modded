#include "manalink.h"

// FUNCTIONS

static int who_is_the_monarch(void){
	int result = -1;
	CYCLE_ALL_CARDS(p, c, 	{
								if( in_play(p, c) && get_id(p, c) == CARD_ID_THE_MONARCH ){
									result = p;
								}
							};
	);
	return result;
}


static void you_become_the_monarch(int player){
	int monarch_player = -1;
	int monarch_card = -1;
	CYCLE_ALL_CARDS(p, c, 	{
								if( in_play(p, c) && get_id(p, c) == CARD_ID_THE_MONARCH ){
									monarch_player = p;
									monarch_card = c;
								}
							};
	);
	if( monarch_player == -1 ){
		monarch_player = player;
		monarch_card = generate_reserved_token_by_id(player, CARD_ID_THE_MONARCH);
	}
	else{
		monarch_player = player;
		monarch_card = gain_control_permanently(player, monarch_player, monarch_card);
	}

	// Knights of the Black Rose effect;
	/* Whenever an opponent becomes the monarch, if you were the monarch as the turn began, that player loses 2 life and you gain 2 life.*/
	int c;
	if( get_card_instance(monarch_player, monarch_card)->targets[0].player == 1-player ){
		for(c=0; c<active_cards_count[1-player]; c++){
			if( in_play(1-player, c) && ! is_humiliated(1-player, c) ){
				if( get_id(1-player, c) == CARD_ID_KNIGHTS_OF_THE_BLACK_ROSE ){
					lose_life(player, 2);
					gain_life(1-player, 2);
				}
			}
		}
	}

	// Custodi Lich effect
	/* Whenever you become the monarch, target player sacrifices a creature. */
	int marked[2][100] = {{-1}};
	for(c=0; c<active_cards_count[player]; c++){
		if( in_play(player, c) && ! is_humiliated(player, c) ){
			if( get_id(player, c) == CARD_ID_CUSTODI_LICH ){
				target_definition_t td;
				default_target_definition(player, c, &td, 0);
				td.zone = TARGET_ZONE_PLAYERS;
				td.allow_cancel = 0;
				if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
					card_instance_t *instance = get_card_instance(player, c);
					test_definition_t test;
					new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
					int sac = new_sacrifice(player, c, instance->targets[0].player,
											SAC_CAUSED | SAC_NO_CANCEL | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
					instance->number_of_targets = 0;
					if( sac ){
						int k;
						for(k=0; k<100; k++){
							if( marked[BYTE2(sac)][k] == -1 ){
								marked[BYTE2(sac)][k] = BYTE3(sac);
							}
							break;
						}
					}
				}
			}
		}
	}
	int p;
	c = 0;
	for(p=0; p<2; p++){
		while( marked[p][c] != -1 ){
				kill_card(p, marked[p][c], KILL_SACRIFICE);
				c++;
		}
	}
}

// EMBLEMS
int card_the_monarch(int player, int card, event_t event){
/* The Monarch	0x200f0f7
 * Emblem
 * At the beginning of your end step, draw a card.
 * Whenever a creature deals combat damage to you, its controller becomes the monarch. */

	if( event == EVENT_BEGIN_TURN ){
		get_card_instance(player, card)->targets[0].player = player;
	}

	card_instance_t* damage = damage_being_dealt(event);
	if( damage && damage->damage_target_card == -1 && damage->damage_target_player == player)
	{
		if( !(damage->token_status & (STATUS_COMBAT_DAMAGE|STATUS_FIRST_STRIKE_DAMAGE)) ){
			return 0;
		}

		if( ! is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE) ){
			return 0;
		}

		if( check_special_flags(damage->damage_source_player, damage->damage_source_card, SF_ATTACKING_PWALKER) ){
			return 0;
		}

		get_card_instance(player, card)->targets[8].player = 66;
	}

	if( resolve_damage_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		get_card_instance(player, card)->targets[8].player = 0;
		give_control_of_self(player, card);
	}

	if( current_turn == player && eot_trigger(player, card, event) ){
		draw_cards(player, 1);
	}

	return 0;
}


// CONSPIRACY

int card_conspiracy_adrianas_valor(int player, int card, event_t event){
/* Adriana's Valor	0x200f025
 * Conspiracy
 * Hidden agenda (Start the game with this conspiracy face down in the command zone and secretly name a card. You may turn this conspiracy face up any time and reveal the chosen name.)
 * Whenever a creature you control with the chosen name attacks, you may pay |W.
 * If you do, that creature gains indestructible until end of turn. (Damage and effects that say "destroy" don't destroy it.) */

	if (event == EVENT_DECLARE_ATTACKERS && !is_humiliated(player, card))
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.id = get_conspiracys_hidden_name(player, card);
		store_attackers(player, card, event, DAT_STORE_IN_INFO_SLOT | DAT_TRACK, player, -1, &test);
	}

	int amt = declare_attackers_trigger(player, card, event, DAT_STORE_IN_INFO_SLOT | DAT_TRACK, player, -1);
	if( amt && has_mana(player, COLOR_WHITE, 1) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
		for (--amt; amt >= 0; --amt){
			if (in_play(current_turn, attackers[amt])){
				charge_mana_while_resolving(player, card, event, player, COLOR_WHITE, 1);
				if( spell_fizzled != 1 ){
					pump_ability_until_eot(player, card, current_turn, attackers[amt], 0, 0, 0, SP_KEYWORD_INDESTRUCTIBLE);
				}
			}
		}
	}

	return 0;
}

int card_conspiracy_assemble_the_rank_and_vile(int player, int card, event_t event){
 /* Assemble the Rank and Vile	0x200f02a
 * Conspiracy
 * Hidden agenda (Start the game with this conspiracy face down in the command zone and secretly name a card. You may turn this conspiracy face up any time and reveal the chosen name.)
 * Creatures you control with the chosen name have:
 * "When this creature dies, you may pay |B. If you do, create a tapped 2/2 |Sblack Zombie creature token." */

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.id = get_conspiracys_hidden_name(player, card);
		count_for_gfp_ability(player, card, event, player, 0, &test);
	}

	int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( amount && has_mana(player, COLOR_BLACK, 1) ){
		while( amount ){
				charge_mana_while_resolving(player, card, event, player, COLOR_BLACK, 1);
				if( spell_fizzled != 1 ){
					token_generation_t token;
					default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
					token.pow = token.tou = 2;
					token.color_forced = COLOR_TEST_BLUE;
					token.action = TOKEN_ACTION_TAPPED;
					generate_token(&token);
				}
				amount--;
		}
	}

 	return 0;
}

int card_conspiracy_echoing_boon(int player, int card, event_t event){
/* Echoing Boon	0x200f02f
 * Conspiracy
 * Hidden agenda (Start the game with this conspiracy face down in the command zone and secretly name a card. You may turn this conspiracy face up any time and reveal the chosen name.)
 * Whenever you cast an instant or sorcery spell, if it targets a creature you control with the chosen name,
 * you may copy that spell and may choose new targets for the copy. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && reason_for_trigger_controller == player ){
		int trig = 0;
		if( trigger_cause_controller == player && is_what(trigger_cause_controller, trigger_cause, TYPE_SPELL) ){
			card_instance_t *spell = get_card_instance( trigger_cause_controller, trigger_cause );
			int i;
			for(i=0; i<spell->number_of_targets; i++){
				if( spell->targets[i].card != -1 &&
					get_id(spell->targets[i].player, spell->targets[i].card) == get_conspiracys_hidden_name(player, card) ){
					trig = 1;
					break;
				}
			}
		}

		if( trig && new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), NULL) ){
			copy_spell(trigger_cause_controller, trigger_cause);
		}
	}

 	return 0;
}

int card_conspiracy_emissarys_ploy(int player, int card, event_t event){
/* Emissary's Ploy	0x200f034
 * Conspiracy
 * (Start the game with this conspiracy face up in the command zone.)
 * Before drawing your opening hand, choose 1, 2, or 3.
 * You may spend mana as though it were mana of any color to cast creature spells with converted mana cost equal to the chosen number. */

//	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_HIDDEN_AGENDA ){
		int priorities[4] = {0};
		int i;
		for(i=0; i<count_deck(player); i++){
			int iid = deck_ptr[player][i];
			if( is_what(-1, iid, TYPE_CREATURE) ){
				int cmc = get_cmc_by_internal_id(iid);
				if( cmc < 3 ){
					priorities[cmc] += 2;
				}
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
							"CMC 1", 1, priorities[1],
							"CMC 2", 1, priorities[2],
							"CMC 3", 1, priorities[3]);

		store_conspiracys_hidden_name(player, card, choice);
	}

	if( ! IS_GAA_EVENT(event) ){
		return vanguard_card(player, card, event, 7, 20, 11);
	}

	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);
	test.cmc = get_conspiracys_hidden_name(player, card);

	return you_may_spend_mana_as_though_it_were_mana_of_any_color_to_play_spell(player, card, event, 0, &test);
}

int card_conspiracy_hired_heist(int player, int card, event_t event){
/* Hired Heist	0x200f039
 * Conspiracy
 * Hidden agenda (Start the game with this conspiracy face down in the command zone and secretly name a card. You may turn this conspiracy face up any time and reveal the chosen name.)
 * Whenever a creature you control with the chosen name deals combat damage to a player, you may pay |U. If you do, draw a card. */

	if( event == EVENT_DEAL_DAMAGE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.id = get_conspiracys_hidden_name(player, card);

		check_damage_test(player, -1, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE, player, card, 0, &test);
	}

	if( trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->targets[8].player > 0 && has_mana(player, COLOR_BLUE, 1) ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_AI(player);
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					if (--instance->targets[8].player > 0){
						instance->state &= ~STATE_PROCESSING;	// More optional triggers left.  Must be the first thing done during resolve trigger.
					}
					charge_mana_while_resolving(player, card, event, player, COLOR_BLUE, 1);
					if( spell_fizzled != 1 ){
						draw_cards(player, 1);
					}
			}
			else if (event == EVENT_END_TRIGGER){
					instance->targets[8].player = 0;
			}
		}
	}

	return 0;
}

static int hold_the_perimeter_legacy_goblin(int player, int card, event_t event){

	if( current_turn != player && upkeep_trigger(player, card, event) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_GOBLIN, &token);
		token.t_player = 1-player;
		token.pow = token.tou = 1;
		token.s_key_plus = SP_KEYWORD_CANNOT_BLOCK;
		token.color_forced = COLOR_TEST_RED;
		generate_token(&token);

		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

static int hold_the_perimeter_legacy_soldier(int player, int card, event_t event){

	if( current_turn == player && upkeep_trigger(player, card, event) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SOLDIER, &token);
		token.pow = 1;
		token.tou = 2;
		token.key_plus = KEYWORD_DEFENDER;
		token.color_forced = COLOR_TEST_WHITE;
		generate_token(&token);

		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_conspiracy_hold_the_perimeter(int player, int card, event_t event){
/* Hold the Perimeter	0x200f03e
 * Conspiracy
 * (Start the game with this conspiracy face up in the command zone.)
 * At the beginning of your first upkeep, create a 1/2 |Swhite Soldier creature token with defender.
 * At the beginning of each other player's first upkeep, that player creates a 1/1 |Sred Goblin creature token with
 * "This creature can't block." */

	if( event == EVENT_HIDDEN_AGENDA ){
		if( current_turn == player ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SOLDIER, &token);
			token.pow = 1;
			token.tou = 2;
			token.key_plus = KEYWORD_DEFENDER;
			token.color_forced = COLOR_TEST_WHITE;
			generate_token(&token);

			create_legacy_effect(player, card, &hold_the_perimeter_legacy_goblin);
		}
		else{
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_GOBLIN, &token);
			token.t_player = 1-player;
			token.pow = token.tou = 1;
			token.s_key_plus = SP_KEYWORD_CANNOT_BLOCK;
			token.color_forced = COLOR_TEST_RED;
			generate_token(&token);

			create_legacy_effect(player, card, &hold_the_perimeter_legacy_soldier);
		}
	}

	return vanguard_card(player, card, event, 7, 20, 11);
}

int card_conspiracy_hymn_of_the_wilds(int player, int card, event_t event){
/* Hymn of the Wilds	0x200f043
 * Conspiracy
 * (Start the game with this conspiracy face up in the command zone.)
 * The first creature spell you cast each turn costs |1 less to cast.
 * You can't cast instant or sorcery spells. */

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player ){
		if( is_what(affected_card_controller, affected_card, TYPE_SPELL) ){
			infinite_casting_cost();
		}
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) && get_stormcreature_count(player) == 0 ){
			COST_COLORLESS--;
		}
	}

	return vanguard_card(player, card, event, 7, 20, 11);
}

static const char* can_activate_this(int who_chooses, int player, int card)
{
	if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, 0, NULL, NULL) ){
		return NULL;
	}
	return "can't activate this";
}

int card_conspiracy_incendiary_dissent(int player, int card, event_t event){
/* Incendiary Dissent	0x200f048
 * Conspiracy
 * Hidden agenda (Start the game with this conspiracy face down in the command zone and secretly name a card. You may turn this conspiracy face up any time and reveal the chosen name.)
 * Creatures you control with the chosen name have "|R: This creature gets +1/+0 until end of turn." */

	if( event == EVENT_CAN_ACTIVATE ){
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_play(player, i) && is_what(player, i, TYPE_CREATURE) ){
				if( generic_activated_ability(player, i, event, 0, MANACOST_R(1), 0, NULL, NULL) ){
					return 1;
				}
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		target_definition_t td;
		counterspell_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.extra = (int32_t)can_activate_this;
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;

		card_instance_t *instance = get_card_instance(player, card);

		instance->number_of_targets = 0;

		if( new_pick_target(&td, "Select a creature you control to activate.", 0, 1 | GS_LITERAL_PROMPT) ){
			charge_mana_for_activated_ability(instance->targets[0].player, instance->targets[0].card, MANACOST_R(1));
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		target_definition_t td;
		counterspell_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( valid_target(&td) ){
			pump_until_eot_merge_previous(instance->targets[0].player, instance->targets[0].card,
											instance->targets[0].player, instance->targets[0].card, 1, 0);
		}
	}

	return 0;
}

int card_conspiracy_natural_unity(int player, int card, event_t event){
/* Natural Unity	0x200f04d
 * Conspiracy
 * Hidden agenda (Start the game with this conspiracy face down in the command zone and secretly name a card. You may turn this conspiracy face up any time and reveal the chosen name.)
 * Creatures you control with the chosen name have:
 * "At the beginning of combat on your turn, you may pay |G. If you do, put a +1/+1 counter on this creature." */

	if( beginning_of_combat(player, card, event, player, -1) ){
		int i;
		for(i=0; i<active_cards_count[player] && has_mana(player, COLOR_GREEN, 1); i++){
			if( in_play(player, i) && is_what(player, i, TYPE_CREATURE) && ! is_humiliated(player, i) ){
				int choice = DIALOG(player, i, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_NO_CANCEL,
									"Add +1/+1 counter", 1, 5,
									"Decline",	1, 1);
				if( choice == 1 ){
					charge_mana(player, COLOR_GREEN, 1);
					if( spell_fizzled != 1 ){
						add_1_1_counter(player, i);
					}
				}
			}
		}
	}

	return 0;
}

/* Sovereign's Realm	0x000000
 * Conspiracy
 * (Start the game with this conspiracy face up in the command zone.)
 * Your deck can't have basic land cards and your starting hand size is five.
 * Exile a card from your hand: This turn, you may play basic land cards from outside the game.
 * Basic lands you control have "|T: Add one mana of any color to your mana pool." */

int card_conspiracy_summoners_bond(int player, int card, event_t event){
/* Summoner's Bond	0x200f052
 * Conspiracy
 * Double agenda (Start the game with this conspiracy face down in the command zone and secretly name two different cards.
					You may turn this conspiracy face up any time and reveal the chosen names.)
 * Whenever you cast a creature spell with one of the chosen names, you may search your library for a
 * creature card with the other chosen name, reveal it, put it into your hand, then shuffle your library. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && reason_for_trigger_controller == player ){
		int do_it = 0;
		if( get_id(trigger_cause_controller, trigger_cause) == get_conspiracys_hidden_name(player, card) ){
			do_it = 1;
		}
		if( get_id(trigger_cause_controller, trigger_cause) == get_conspiracys_hidden_name2(player, card) ){
			do_it = 2;
		}
		if( do_it ){
			if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), NULL) ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				if( do_it == 1 ){
					this_test.id = get_conspiracys_hidden_name(player, card);
				}
				if( do_it == 1 ){
					this_test.id = get_conspiracys_hidden_name2(player, card);
				}
				scnprintf(this_test.message, 100, "Select a creature card named %s.", cards_ptr[this_test.id]->name);
				new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);
			}
		}
	}
	return 0;
}

/* Weight Advantage	0x401000 (vanilla, the real effect is in "get_attack_power")
 * Conspiracy
 * (Start the game with this conspiracy face up in the command zone.)
 * Each creature you control assigns combat damage equal to its toughness rather than its power. */

// WHITE

/* Ballot Broker	|2|W	0x401000 (vanilla, the real effect is in "will_of_the_council_impl")
 * Creature - Human Advisor 2/3
 * While voting, you may vote an additional time. (The votes can be for different choices or for the same choice.) */

/* Custodi Peacekeeper	|2|W	0x0000000
 * Creature - Human Cleric 2/3
 * Reveal ~ as you draft it and note how many cards you've drafted this draft round, including ~.
 * |W, |T: Tap target creature with power less than or equal to the highest number you noted for cards named ~. */

/* Custodi Soulcaller	|1|W|W	0x0000000
 * Creature - Human Cleric 1/2
 * Melee (Whenever this creature attacks, it gets +1/+1 until end of turn for each opponent you attacked with a creature this combat.)
 * Whenever ~ attacks, return target creature card with converted mana cost X or less from your graveyard to the battlefield,
 * where X is the number of players you attacked with a creature this combat. */

int card_liutenants_of_the_guard(int player, int card, event_t event){
/* Lieutenants of the Guard	|4|W	0x200f057
 * Creature - Human Soldier 2/2
 * Council's dilemma - When ~ enters the battlefield, starting with you, each player votes for strength or numbers.
 * Put a +1/+1 counter on ~ for each strength vote and create a 1/1 |Swhite Soldier creature token for each numbers vote. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		char pos1[100] = "Strenght (+1/+1 counters on this)";
		char pos2[100] = "Number (1/1 soldier tokens)";
		int ai_priorities[2] = {10, 5};
		int result = will_of_the_council(player, card, pos1, pos2, ai_priorities);
		add_1_1_counters(player, card, BYTE0(result));

		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SOLDIER, &token);
		token.qty = BYTE1(result);
		token.pow = token.tou = 1;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		generate_token(&token);
	}

	return 0;
}

/* Noble Banneret	|2|W|W	0x0000000
 * Creature - Human Knight 3/3
 * Draft ~ face up.
 * As you draft a creature card, you may reveal it, note its name, then turn ~ face down.
 * As long as you control one or more creatures with a name you noted for cards named ~, ~ and those creatures get +1/+1 and have lifelink. */

int card_palace_jailer(int player, int card, event_t event){
/* Palace Jailer	|2|W|W	0x200f05c
 * Creature - Human Soldier 2/2
 * When ~ enters the battlefield, you become the monarch.
 * When ~ enters the battlefield, exile target creature an opponent controls until an opponent becomes the monarch.
 * (That creature returns under its owner's control.) */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) && ! is_humiliated(player, card) ){
		if( get_card_instance(player, card)->info_slot == 66 && who_is_the_monarch() != player ){
			true_return_from_oblivion(player, card);
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		you_become_the_monarch(player);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target creature an opponent controls.", 0, GS_LITERAL_PROMPT) ){
			obliviation(player, card, instance->targets[0].player, instance->targets[0].card);
			instance->number_of_targets = 0;
			instance->info_slot = 66;
		}
	}
	return 0;
}

int card_palace_sentinels(int player, int card, event_t event){
	/* Canal Courier	|5|U
	 * Creature - Human Rogue 3/5
	 * When ~ enters the battlefield, you become the monarch.
	 * Whenever ~ and another creature attack different players, ~ can't be blocked this combat. */
	/* Palace Sentinels	|3|W	0x200f061
	 * Creature - Human Soldier 2/4
	 * When ~ enters the battlefield, you become the monarch. */
	// Used externally
	// Canal Courier's last ability can't trigger in two-player.

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		you_become_the_monarch(player);
	}

	return 0;
}

/* Paliano Vanguard	|1|W	0x0000000
 * Creature - Human Soldier 2/2
 * Draft ~ face up.
 * As you draft a creature card, you may reveal it, note its creature types, then turn ~ face down.
 * Other creatures you control of a type you noted for cards named ~ get +1/+1. */

int card_protector_of_the_crown(int player, int card, event_t event){
/* Protector of the Crown	|5|W	0x200f066
 * Creature - Giant Soldier 2/5
 * When ~ enters the battlefield, you become the monarch.
 * All damage that would be dealt to you is dealt to ~ instead. */

	if( event == EVENT_PREVENT_DAMAGE && in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->damage_target_player == player &&
			damage->damage_target_card == -1
		  ){
			if( damage->targets[4].card == -1 ){ // AKA we aren't dealing damage to a Planeswalker, see 'damage_creature'
				damage->damage_target_player = player;
				damage->damage_target_card = card;
			}
		}
	}

	return card_palace_sentinels(player, card, event);
}

int card_recruiter_of_the_guard(int player, int card, event_t event){
/* Recruiter of the Guard	|2|W	0x200f06b
 * Creature - Human Soldier 1/1
 * When ~ enters the battlefield, you may search your library for a creature card with toughness 2 or less, reveal it,
 * put it into your hand, then shuffle your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		char msg[100] = "Select a creature with Toughness 2 or less";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, msg);
		this_test.toughness = 3;
		this_test.toughness_flag = F5_TOUGHNESS_LESSER_THAN_VALUE;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return 0;
}

int card_sanctum_prelate(int player, int card, event_t event){
/* Sanctum Prelate	|1|W|W	0x200f070
 * Creature - Human Cleric 2/2
 * As ~ enters the battlefield, choose a number.
 * Noncreature spells with converted mana cost equal to the chosen number can't be cast. */

	if( event == EVENT_RESOLVE_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE ){
		get_card_instance(player, card)->info_slot = choose_a_number(player, "Select a number.", internal_rand(6)+1);
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && in_play(player, card) && ! is_humiliated(player, card) ){
		if( ! is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			if( get_cmc(affected_card_controller, affected_card) == get_card_instance(player, card)->info_slot ){
				infinite_casting_cost();
			}
		}
	}

	return 0;
}

int card_spectral_grasp(int player, int card, event_t event){
/* Spectral Grasp	|1|W	0x200f075
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature can't attack you or a planeswalker you control.
 * Enchanted creature can't block creatures you control. */

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( p != player ){
			cannot_attack(p, c, event);
			cannot_block(p, c, event);
		}
	}

	return disabling_aura(player, card, event);
}

int card_throne_warden(int player, int card, event_t event){
/* Throne Warden	|1|W	0x200f07a
 * Creature - Human Soldier 2/2
 * At the beginning of your end step, if you're the monarch, put a +1/+1 counter on ~. */
	if( current_turn == player && trigger_condition == TRIGGER_EOT && affect_me(player, card) ){
		if( who_is_the_monarch() == player && eot_trigger(player, card, event) ){
			add_1_1_counter(player, card);
		}
	}
	return 0;
}

/* Wings of the Guard	|1|W	0x0000000
 * Creature - Bird 1/1
 * Flying
 * Melee (Whenever this creature attacks, it gets +1/+1 until end of turn for each opponent you attacked with a creature this combat.) */

// BLUE

/* Arcane Savant	|3|U|U	0x0000000
 * Creature - Human Wizard 3/3
 * Before you shuffle your deck to start the game, you may reveal this card from your deck and exile an instant or sorcery card you drafted that isn't in your deck.
 * When ~ enters the battlefield, copy a card you exiled with cards named ~. You may cast the copy without paying its mana cost. */

/* Canal Courier	|5|U	=>card_palace_sentinels 0x200f061
 * Creature - Human Rogue 3/5
 * When ~ enters the battlefield, you become the monarch.
 * Whenever ~ and another creature attack different players, ~ can't be blocked this combat. */

/* Coveted Peacock	|3|U|U	0x0000000
 * Creature - Bird 3/4
 * Flying
 * Whenever ~ attacks, you may goad target creature defending player controls. (Until your next turn, that creature attacks each combat if able and attacks a player other than you if able.) */

int card_expropriate(int player, int card, event_t event){
/* Expropriate	|7|U|U	0x200f07f
 * Sorcery
 * Council's dilemma - Starting with you, each player votes for time or money.
 * For each time vote, take an extra turn after this one.
 * For each money vote, choose a permanent owned by the voter and gain control of it. Exile ~. */

	if( event == EVENT_RESOLVE_SPELL ){
		char pos1[100] = "Time (Extra turn)";
		char pos2[100] = "Money (Control a permanent owner by voter)";
		int ai_priorities[2] = {5, 10};
		int result = will_of_the_council(player, card, pos1, pos2, ai_priorities);

		int i;
		for(i=0; i<BYTE0(result); i++){
			int l = time_walk_effect(player, card);
			if (l >= 0){
				get_card_instance(player, l)->eot_toughness = 1;
			}
		}

		int gain_control_of_permanent_owned_by[2] = {BYTE1(result), BYTE2(result)};
		card_instance_t *instance = get_card_instance(player, card);

		APNAP(p,{
					for(i=0; i<gain_control_of_permanent_owned_by[p]; i++){
						target_definition_t td;
						default_target_definition(player, card, &td, TYPE_PERMANENT);
						td.allowed_controller = td.preferred_controller = p;
						td.illegal_abilities = 0;
						td.allow_cancel = 0;

						if( can_target(&td) ){
							if( p == player ){
								new_pick_target(&td, "Select a permanent you control.", 0, GS_LITERAL_PROMPT);
							}
							else{
								new_pick_target(&td, "Select a permanent controlled by opponent.", 0, GS_LITERAL_PROMPT);
							}
							if( instance->number_of_targets ){
								gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
							}
							instance->number_of_targets = 0;
						}
					}
				};
		);
		kill_card(player, card, KILL_EXILE);
	}

	return basic_spell(player, card, event);
}

static int illusion_of_choice_legacy(int player, int card, event_t event){

 // Main effect is in "will_of_the_council_impl"

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_illusion_of_choice(int player, int card, event_t event){
/* Illusion of Choice	|U	0x200f084
 * Instant
 * You choose how each player votes this turn.
 * Draw a card. */

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &illusion_of_choice_legacy);
		draw_cards(player, 1);
		kill_card(player, card, KILL_EXILE);
	}

	return basic_spell(player, card, event);
}

/* Illusionary Informant	|1|U	0x0000000
 * Creature - Bird Illusion 1/3
 * Draft ~ face up.
 * During the draft, you may turn ~ face down. If you do, look at the next card drafted by a player of your choice.
 * Flying */

/* Jeering Homunculus	|1|U	0x0000000
 * Creature - Homunculus 0/4
 * When ~ enters the battlefield, you may goad target creature. (Until your next turn, that creature attacks each combat if able and attacks a player other than you if able.) */

int card_keeper_of_keys(int player, int card, event_t event){
/* Keeper of Keys	|3|U|U	0x200f089
 * Creature - Human Rogue Mutant 4/4
 * When ~ enters the battlefield, you become the monarch.
 * At the beginning of your upkeep, if you're the monarch, creatures you control can't be blocked this turn. */

	if( current_turn == player && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card)
	      && reason_for_trigger_controller == player ){
		if( who_is_the_monarch() == player && upkeep_trigger(player, card, event) ){
			get_card_instance(player, card)->info_slot = 66;
		}
	}

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_BLOCK_LEGALITY && affected_card_controller == 1-player && get_card_instance(player, card)->info_slot == 66 ){
			event_result = 1;
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->info_slot = 0;
	}

	return card_palace_sentinels(player, card, event);
}

int card_messenger_jays(int player, int card, event_t event){
/* Messenger Jays	|4|U	0x200f08e
 * Creature - Bird 2/1
 * Flying
 * Council's dilemma - When ~ enters the battlefield, starting with you, each player votes for feather or quill.
 * Put a +1/+1 counter on ~ for each feather vote and draw a card for each quill vote.
 * For each card drawn this way, discard a card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		char pos1[100] = "Feather (+1/+1 counters on this)";
		char pos2[100] = "Quill (Draw & discard)";
		int ai_priorities[2] = {10, 5};
		int result = will_of_the_council(player, card, pos1, pos2, ai_priorities);
		add_1_1_counters(player, card, BYTE0(result));

		draw_cards(player, BYTE1(result));

		new_multidiscard(player, BYTE1(result), 0, player);
	}

	return 0;
}

int card_skittering_crustacean(int player, int card, event_t event){
/* Skittering Crustacean	|2|U	0x200f093
 * Creature - Crab 2/3
 * |6|U: Monstrosity 4. (If this creature isn't monstrous, put four +1/+1 counters on it and it becomes monstrous.)
 * As long as ~ is monstrous, it has hexproof. (It can't be the target of spells or abilities your opponents control.) */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_ABILITIES && affect_me(player, card) && is_monstrous(player, card) ){
			hexproof(player, card, event);
		}
	}

	return monstrosity(player, card, event, MANACOST_XU(6, 1), 4);
}

/* Spire Phantasm	|2|U|U	0x0000000
 * Creature - Gargoyle Illusion 3/2
 * Reveal ~ as you draft it. The next time a player drafts a card from this booster pack, guess that card's name. Then that player reveals the drafted card.
 * Flying
 * When ~ enters the battlefield, if you guessed correctly for a card named ~, draw a card. */

/* Stunt Double	|3|U	0x2003e23 (Clone)
 * Creature - Shapeshifter 0/0
 * Flash
 * You may have ~ enter the battlefield as a copy of any creature on the battlefield. */

// BLACK

/* Archdemon of Paliano	|2|B|B	0x0000000
 * Creature - Demon 5/4
 * Draft ~ face up.
 * As long as ~ is face up during the draft, you can't look at booster packs and must draft cards at random. After you draft three cards this way, turn ~ face down. (You may look at cards as you draft them.)
 * Flying */

int card_capital_punishment(int player, int card, event_t event){
/* Capital Punishment	|4|B|B	0x200f098
 * Sorcery
 * Council's dilemma - Starting with you, each player votes for death or taxes.
 * Each opponent sacrifices a creature for each death vote and discards a card for each taxes vote. */

	if( event == EVENT_RESOLVE_SPELL ){
		char pos1[100] = "Death (Sac a creature)";
		char pos2[100] = "Taxes (Discard a card)";
		int ai_priorities[2] = {2*count_subtype(player, TYPE_CREATURE, -1), 3*hand_count[player]};
		int result = will_of_the_council(player, card, pos1, pos2, ai_priorities);

		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		test.qty = BYTE0(result);
		new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &test);

		new_multidiscard(1-player, BYTE1(result), 0, player);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}


/* Custodi Lich	|3|B|B		0x200f061 (Palace Sentinels, the rest of the effect is in "you_become_the_monarch")
 * Creature - Zombie Cleric 4/2
 * When ~ enters the battlefield, you become the monarch.
 * Whenever you become the monarch, target player sacrifices a creature. */

int deadly_designs_legacy(int player, int card, event_t event){

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->targets[0].player;
		int c = instance->targets[0].card;

		if( affect_me(p, c) ){
			if( get_card_instance(p, c)->kill_code == KILL_SACRIFICE ){

				target_definition_t td;
				default_target_definition(player, card, &td, TYPE_CREATURE);
				td.allow_cancel = 2; // "Done" button
				td.illegal_abilities = instance->targets[1].player > -1 ?
										instance->targets[1].player : KEYWORD_PROT_BLACK;

				int i;
				for(i=0; i<2; i++){
					new_pick_target(&td, "TARGET_CREATURE", i, 0);
				}
				for(i=0; i<instance->number_of_targets; i++){
					if( validate_target(player, card, &td, i) ){
						kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
					}
				}
			}
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int deadly_designs_effect(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_LEAVE_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->targets[0].player;
		int c = instance->targets[0].card;

		if( affect_me(p, c) && reason_for_trigger_controller == player ){
			if( event == EVENT_TRIGGER ){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}

			if( event == EVENT_RESOLVE_TRIGGER ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}


	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	int p = instance->targets[0].player;
	int c = instance->targets[0].card;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		add_counter(p, c, COUNTER_PLOT);
	}

	return granted_generic_activated_ability(player, card, p, c, event, 0, MANACOST_X(2), 0, NULL, NULL);
}

int card_deadly_designs(int player, int card, event_t event){
/* Deadly Designs	|1|B	0x200f09d
 * Enchantment
 * |2: Put a plot counter on ~. Any player may activate this ability.
 * When there are five or more plot counters on ~, sacrifice it. If you do, destroy up to two target creatures. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		if( ! is_humiliated(player, card) && count_counters(player, card, COUNTER_PLOT) >= 5 ){
			if( ! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) ){
				int legacy = create_legacy_effect(player, card, &deadly_designs_legacy);
				card_instance_t *leg = get_card_instance(player, legacy);
				leg->targets[0].player = player;
				leg->targets[0].card = card;
				leg->number_of_targets = 0;
				leg->targets[1].player = get_protections_from(player, card);

				kill_card(player, card, KILL_SACRIFICE);
			}
		}

		int marked[2] = {-1, -1};
		CYCLE_ALL_CARDS(p, c, 	{
									if( in_play(p, c) && is_what(p, c, TYPE_EFFECT) ){
										card_instance_t *inst = get_card_instance(p, c);
										if( inst->info_slot == (int)deadly_designs_effect ){
											if( inst->targets[0].player == player && inst->targets[0].card == card )
											{
												marked[p] = c;
											}
										}
									}
								}
		);

		if( marked[player] > -1 ){
			kill_card(player, marked[player], KILL_EXILE);
		}

		if( marked[1-player] == -1 ){
			int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
			int legacy = create_legacy_activate(1-player, fake, &deadly_designs_effect);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			obliterate_card(1-player, fake);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		add_counter(instance->parent_controller, instance->parent_card, COUNTER_PLOT);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
}

int card_garrulous_sycophant(int player, int card, event_t event){
/* Garrulous Sycophant	|2|B	0x200f0a2
 * Creature - Human Advisor 1/4
 * At the beginning of your end step, if you're the monarch, each opponent loses 1 life and you gain 1 life. */

	if( current_turn == player && trigger_condition == TRIGGER_EOT && affect_me(player, card) ){
		if( who_is_the_monarch() == player && eot_trigger(player, card, event) ){
			lose_life(1-player, 1);
			gain_life(player, 1);
		}
	}

	return 0;
}

int card_marchesas_decreee(int player, int card, event_t event){
/* Marchesa's Decree	|3|B	0x200f0a7
 * Enchantment
 * When ~ enters the battlefield, you become the monarch.
 * Whenever a creature attacks you or a planeswalker you control, that creature's controller loses 1 life. */

	if( event == EVENT_CAN_CAST || event == EVENT_SHOULD_AI_PLAY || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return global_enchantment(player, card, event);
	}

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( declare_attackers_trigger(player, card, event, DAT_SEPARATE_TRIGGERS, 1-player, -1) ){
			lose_life(1-player, 1);
		}
	}

	return card_palace_sentinels(player, card, event);
}

/* Regicide	|B	0x0000000
 * Instant
 * Reveal ~ as you draft it. The player to your right chooses a color, you choose another color, then the player to your left chooses a third color.
 * Destroy target creature that's one or more of the colors chosen as you drafted cards named ~. */

int card_sinuous_vermin(int player, int card, event_t event){
/* Sinuous Vermin	|1|B	0x200f0ac
 * Creature - Rat Horror 2/2
 * |3|B|B: Monstrosity 3. (If this creature isn't monstrous, put three +1/+1 counters on it and it becomes monstrous.)
 * As long as ~ is monstrous, it has menace. (It can't be blocked except by two or more creatures.) */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_ABILITIES && affect_me(player, card) && is_monstrous(player, card) ){
			menace(player, card, event);
		}
	}

	return monstrosity(player, card, event, MANACOST_XB(3, 2), 3);
}

/* Smuggler Captain	|3|B	0x0000000
 * Creature - Human Pirate 2/2
 * Draft ~ face up.
 * As you draft a card, you may reveal it, note its name, then turn ~ face down.
 * When ~ enters the battlefield, you may search your library for a card with a name you noted for cards named ~, reveal it, put it into your hand, then shuffle your library. */

int card_thorn_of_the_black_rose(int player, int card, event_t event){
/* Thorn of the Black Rose	|3|B	0x200f0b1
 * Creature - Human Assassin 1/3
 * Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.)
 * When ~ enters the battlefield, you become the monarch. */
	deathtouch(player, card, event);
	return card_palace_sentinels(player, card, event);
}

// RED

/* Besmirch	|1|R|R	0x0000000
 * Sorcery
 * Until end of turn, gain control of target creature and it gains haste. Untap and goad that creature. (Until your next turn, that creature attacks each combat if able and attacks a player other than you if able.) */

int card_crow_hunter_hireling(int player, int card, event_t event){
/* Crown-Hunter Hireling	|4|R	0x200f0b6
 * Creature - Ogre Mercenary 4/4
 * When ~ enters the battlefield, you become the monarch.
 * ~ can't attack unless defending player is the monarch. */

	if( current_turn == player && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && reason_for_trigger_controller == player ){
		if( who_is_the_monarch() == player && upkeep_trigger(player, card, event) ){
			get_card_instance(player, card)->info_slot = 66;
		}
	}

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_ATTACK_LEGALITY && affect_me(player, card) ){
			if( who_is_the_monarch() != 1-player ){
				cannot_attack(player, card, event);
			}
		}
	}

	return card_palace_sentinels(player, card, event);
}

/* Deputized Protester	|2|R	0x0000000
 * Creature - Human Warrior 2/1
 * Menace (This creature can't be blocked except by two or more creatures.)
 * Melee (Whenever this creature attacks, it gets +1/+1 until end of turn for each opponent you attacked with a creature this combat.) */

/* Garbage Fire	|2|R	0x0000000
 * Instant
 * Reveal ~ as you draft it and note how many cards you've drafted this draft round, including ~.
 * ~ deals damage to target creature equal to the highest number you noted for cards named ~. */

/* Goblin Racketeer	|3|R	0x0000000
 * Creature - Goblin Rogue 4/2
 * Whenever ~ attacks, you may goad target creature defending player controls. (Until your next turn, that creature attacks each combat if able and attacks a player other than you if able.) */

int card_grenzo_havoc_raiser(int player, int card, event_t event){
/* Grenzo, Havoc Raiser	|R|R	0x200f0bb
 * Legendary Creature - Goblin Rogue 2/2
 * Whenever a creature you control deals combat damage to a player, choose one -
 * * Goad target creature that player controls.
 * * Exile the top card of that player's library. Until end of turn, you may cast that card and you may spend mana as though
 * * it were mana of any color to cast it. */

 // The "Goad" ability is worthless for 1 vs 1, but the second ability is relevant

	check_legend_rule(player, card, event);

	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);
	check_damage_test(player, -1, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_TRACE_DAMAGED_PLAYERS,
						player, card, 0, &test);

	if( trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->targets[8].player > 0 && ! is_humiliated(player, card) ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					if (--instance->targets[8].player > 0){
						instance->state &= ~STATE_PROCESSING;	// More optional triggers left.  Must be the first thing done during resolve trigger.
					}
					int damaged_players = instance->targets[1].player;
					int i;
					for(i=0; i<BYTE0(damaged_players);i++){
						int iid = deck_ptr[0][0];
						if( iid > -1 ){
							rfg_top_card_of_deck(0);
							create_may_play_card_from_exile_effect(player, card, 0, cards_data[iid].id, MPCFE_FOR_CMC);
						}
					}
					for(i=0; i<BYTE1(damaged_players);i++){
						int iid = deck_ptr[1][0];
						if( iid > -1 ){
							rfg_top_card_of_deck(1);
							create_may_play_card_from_exile_effect(player, card, 1, cards_data[iid].id, MPCFE_FOR_CMC);
						}
					}
			}
			else if (event == EVENT_END_TRIGGER){
					instance->targets[8].player = 0;
			}
		}
	}

	return 0;
}

/* Grenzo's Ruffians	|2|R|R	0x0000000
 * Creature - Goblin 2/2
 * Melee (Whenever this creature attacks, it gets +1/+1 until end of turn for each opponent you attacked with a creature this combat.)
 * Whenever ~ deals combat damage to an opponent, it deals that much damage to each other opponent. */

/* Pyretic Hunter	|4|R	0x0000000
 * Creature - Elemental Cat 0/0
 * Reveal ~ as you draft it and note how many cards you've drafted this draft round, including ~.
 * Menace (This creature can't be blocked except by two or more creatures.)
 * ~ enters the battlefield with X +1/+1 counters on it, where X is the highest number you noted for cards named ~. */

int card_skyline_despot(int player, int card, event_t event){
/* Skyline Despot	|5|R|R	0x200f0c0
 * Creature - Dragon 5/5
 * Flying
 * When ~ enters the battlefield, you become the monarch.
 * At the beginning of your upkeep, if you're the monarch, create a 5/5 |Sred Dragon creature token with flying. */

	if( current_turn == player && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		if( who_is_the_monarch() == player ){
			upkeep_trigger_ability(player, card, event, player);
		}
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_DRAGON, &token);
		token.pow = token.tou = 5;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		token.key_plus = KEYWORD_FLYING;
		generate_token(&token);
	}

	return card_palace_sentinels(player, card, event);
}


int card_subterranean_tremors(int player, int card, event_t event){
/* Subterranean Tremors	|X|R	0x200f0c5
 * Sorcery
 * ~ deals X damage to each creature without flying. If X is 4 or more, destroy all artifacts.
 * If X is 8 or more, create an 8/8 |Sred Lizard creature token. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t* instance = get_card_instance(player, card);
		if( instance->info_slot > 0 ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.keyword = KEYWORD_FLYING;
			this_test.keyword_flag = 1;

			APNAP(p, {new_damage_all(player, card, p, instance->info_slot, 0, &this_test);};);

			if( instance->info_slot >= 4 ){
				default_test_definition(&this_test, TYPE_ARTIFACT);

				APNAP(p, {new_manipulate_all(player, card, p, &this_test, KILL_DESTROY);};);
			}

			if( instance->info_slot >= 8 ){
				token_generation_t token;
				default_token_definition(player, card, CARD_ID_LIZARD, &token);
				token.pow = token.tou = 8;
				token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_RED);
				generate_token(&token);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 1, NULL);
}


/* Volatile Chimera	|2|R	0x0000000
 * Creature - Elemental Chimera 3/2
 * Before you shuffle your deck to start the game, you may reveal this card from your deck and exile three or more creature cards you drafted that aren't in your deck.
 * |1|R: Choose a card at random you exiled with cards named ~. ~ becomes a copy of that card and gains this ability. */

/* Animus of Predation	|4|G	0x0000000
 * Creature - Avatar 4/4
 * Draft ~ face up.
 * As you draft a card, you may remove it from the draft face up. (It isn't in your card pool.)
 * If you removed a creature card with flying from the draft with cards named ~, ~ has flying. The same is true for first strike, double strike, deathtouch, haste, hexproof, indestructible, lifelink, menace, reach, and vigilance. */

// GREEN

int card_borderland_explorer(int player, int card, event_t event){
/* Borderland Explorer	|1|G	0x200f0ca
 * Creature - Elf Scout 3/1
 * When ~ enters the battlefield, each player may discard a card. Each player who discarded a card this way may search
 * his or her library for a basic land card, reveal it, put it into his or her hand, then shuffle his or her library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		APNAP(p,{
					if( hand_count[p] > 0 ){
						int ai_mode = 1;
						test_definition_t test;
						default_test_definition(&test, TYPE_LAND);
						test.zone = TARGET_ZONE_HAND;
						if( count_subtype(player, TYPE_LAND, -1) < 3 &&
							! check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test) )
						{
							ai_mode = 0;
						}

						if( player == HUMAN || ai_mode ){
							new_default_test_definition(&test, TYPE_ANY, "Select a card to discard or hit CANCEL");
							int selected = new_select_a_card(p, p, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &test);
							if( selected != -1 ){
								new_discard_card(p, selected, 0, player);
								tutor_basic_land(player, 0, 0);
							}
						}
					}
				};
		);
	}

	return 0;
}


/* Caller of the Untamed	|3|G	0x0000000
 * Creature - Elf Shaman 2/4
 * Before you shuffle your deck to start the game, you may reveal this card from your deck and exile a creature card you drafted that isn't in your deck.
 * |X, |T: Create a token that's a copy of a card you exiled with cards named ~. X is the converted mana cost of that card. */

int card_domesticated_hydra(int player, int card, event_t event){
/* Domesticated Hydra	|2|G|G	0x200f0cf
 * Creature - Hydra 3/3
 * |X|G|G|G: Monstrosity X. (If this creature isn't monstrous, put X +1/+1 counters on it and it becomes monstrous.)
 * As long as ~ is monstrous, it has trample. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_ABILITIES && affect_me(player, card) && is_monstrous(player, card) ){
			event_result |= KEYWORD_TRAMPLE;
		}
	}

	return monstrosity(player, card, event, MANACOST_XG(-2, 3), get_card_instance(player, card)->info_slot);
}

/* Entourage of Trest	|4|G	0x0000000
 * Creature - Elf Soldier 4/4
 * When ~ enters the battlefield, you become the monarch.
 * ~ can block an additional creature each combat as long as you're the monarch. */

/* Fang of the Pack	|5|G	0x0000000
 * Creature - Wolf 5/3
 * Melee (Whenever this creature attacks, it gets +1/+1 until end of turn for each opponent you attacked with a creature this combat.)
 * At the beginning of combat on your turn, another target creature you control gains melee until end of turn. (If a creature has multiple instances of melee, each triggers separately.) */

/* Leovold's Operative	|2|G	0x0000000
 * Creature - Elf Rogue 3/2
 * Draft ~ face up.
 * As you draft a card, you may draft an additional card from that booster pack. If you do, turn ~ face down, then pass the next booster pack without drafting a card from it. (You may look at that booster pack.) */

/* Menagerie Liberator	|3|G	0x0000000
 * Creature - Human Warrior 3/2
 * Trample
 * Melee (Whenever this creature attacks, it gets +1/+1 until end of turn for each opponent you attacked with a creature this combat.) */

int card_orchard_elemental(int player, int card, event_t event){
/* Orchard Elemental	|5|G	0x200f0d4
 * Creature - Elemental 2/2
 * Council's dilemma - When ~ enters the battlefield, starting with you, each player votes for sprout or harvest.
 * Put two +1/+1 counters on ~ for each sprout vote.
 * You gain 3 life for each harvest vote. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		char pos1[100] = "Sprout (2 +1/+1 counters on this)";
		char pos2[100] = "Harvest (3 life)";
		int ai_priorities[2] = {life[player]*3, life[1-player]*3};
		int result = will_of_the_council(player, card, pos1, pos2, ai_priorities);

		add_1_1_counters(player, card, 2*BYTE0(result));

		gain_life(player, 3*BYTE1(result));
	}

	return 0;
}

int card_regal_behemoth(int player, int card, event_t event){
/* Regal Behemoth	|4|G|G	0x200f0d9
 * Creature - Lizard 5/5
 * Trample
 * When ~ enters the battlefield, you become the monarch.
 * Whenever you tap a land for mana while you're the monarch,
 * add one mana of any color to your mana pool (in addition to the mana the land produces). */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		you_become_the_monarch(player);
	}

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		switch (event){
			case EVENT_TAP_CARD:
				if (!is_what(affected_card_controller, affected_card, TYPE_LAND)){
					return 0;
				}

				if (!in_play(player, card)){	// Seems unnecessary, but it's explicitly checked in the exe version, and is harmless
					return 0;
				}

				produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1);

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

				declare_mana_available_hex(affected_card_controller, COLOR_TEST_ANY_COLORED, 1);
				return 0;

			default:
				return 0;
		}
	}

	return 0;
}

int card_selvala_heart_of_the_wilds(int player, int card, event_t event){
/* Selvala, Heart of the Wilds	|1|G|G	0x200f0de
 * Legendary Creature - Elf Scout 2/3
 * Whenever another creature enters the battlefield, its controller may draw a card if its power is greater than each other creature's power.
 * |G, |T: Add X mana in any combination of colors to your mana pool, where X is the greatest power among creatures you control. */

	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		int max_pow = check_battlefield_for_special_card(player, card, ANYBODY, CBFSC_GET_MAX_POW, &test);
		test.power = max_pow;
		test.power_flag = F5_POWER_GREATER_THAN_VALUE;

		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_WHO_CHOOSES(trigger_cause_controller),
							"Draw a card", 	1, count_deck(trigger_cause_controller) >= 20 ? 10 : 1,
							"Decline", 		1, 5);

		if( choice == 1 ){
			draw_cards(trigger_cause_controller, 1);
		}
	}

	if( ! IS_GAA_EVENT(event) && event != EVENT_COUNT_MANA ){
		return 0;
	}

	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( mana_producer(player, card, event) && has_mana_for_activated_ability(player, card, MANACOST_G(1)) ){
			int max_pow = check_battlefield_for_special_card(player, card, ANYBODY, CBFSC_GET_MAX_POW, &test);
			declare_mana_available_hex(player, COLOR_TEST_ANY_COLORED, max_pow-1);
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( mana_producer(player, card, event) && has_mana_for_activated_ability(player, card, MANACOST_G(1)) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_G(1)) ){
			int max_pow = check_battlefield_for_special_card(player, card, ANYBODY, CBFSC_GET_MAX_POW, &test);
			produce_mana_tapped_any_combination_of_colors(player, card, COLOR_TEST_ANY_COLORED, max_pow, NULL);
		}
	}

	return 0;
}

int card_selvalas_stampede(int player, int card, event_t event){
/* Selvala's Stampede	|4|G|G	0x200f0e3
 * Sorcery
 * Council's dilemma - Starting with you, each player votes for wild or free.
 * Reveal cards from the top of your library until you reveal a creature card for each wild vote.
 * Put those creature cards onto the battlefield, then shuffle the rest into your library.
 * You may put a permanent card from your hand onto the battlefield for each free vote. */

	if( event == EVENT_RESOLVE_SPELL ){
		char pos1[100] = "Wild (Creature in play from deck)";
		char pos2[100] = "Free (Permanent in play from hand)";
		int ai_priorities[2] = {count_deck(player), hand_count[player]*20};
		int result = will_of_the_council(player, card, pos1, pos2, ai_priorities);

		int creatures_count =  BYTE0(result);
		if( creatures_count ){
			int deck_copy[count_deck(player)];
			int dcc = 0;

			while( deck_ptr[player][0] != -1 && creatures_count ){
					if( is_what(-1, deck_ptr[player][0], TYPE_CREATURE) ){
						creatures_count--;
					}
					deck_copy[dcc] = deck_ptr[player][0];
					remove_card_from_deck(player, 0);
					dcc++;
			}
			show_deck( player, deck_copy, dcc, "Cards revealed by Selvala's Stampede", 0, 0x7375B0 );
			show_deck( 1-player, deck_copy, dcc, "Cards revealed by Selvala's Stampede", 0, 0x7375B0 );

			if( dcc ){
				int added[dcc];
				int ac = 0;
				int i;
				for(i=dcc-1; i>-1; i--){
					if( is_what(-1, deck_copy[i], TYPE_CREATURE) ){
						added[ac] = add_card_to_hand(player, deck_copy[i]);
					}
					else{
						raw_put_iid_on_top_of_deck(player, deck_copy[i]);
					}
				}
				/* We should really shuffle after putting the creatures in play, but since the ETB abilities will trigger immediately,
				 * this could be exploited so better do things in this way */
				shuffle(player);
				for(i=0; i<ac; i++){
					put_into_play(player, added[i]);
				}
			}
		}

		int permanents_count =  BYTE1(result);
		if( permanents_count ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent card.");
			test.qty = permanents_count;
			tutor_multiple_card_from_hand(player, player, player, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}


int card_splitting_slime(int player, int card, event_t event){
/* Splitting Slime	|3|G|G	0x200f0e8
 * Creature - Ooze 3/3
 * |4|G|G: Monstrosity 3. (If this creature isn't monstrous, put three +1/+1 counters on it and it becomes monstrous.)
 * When ~ becomes monstrous, create a token that's a copy of ~. (The token has no counters and isn't monstrous.) */

	if( event == EVENT_BECAME_MONSTROUS ){
		token_generation_t token;
		default_token_definition(player, card, get_id(player, card), &token);
		generate_token(&token);
	}

	return monstrosity(player, card, event, MANACOST_XG(4, 2), 3);
}


// GOLD

/* Adriana, Captain of the Guard	|3|R|W	0x0000000
 * Legendary Creature - Human Knight 4/4
 * Melee (Whenever this creature attacks, it gets +1/+1 until end of turn for each opponent you attacked with a creature this combat.)
 * Other creatures you control have melee. (If a creature has multiple instances of melee, each triggers separately.) */

int card_daretti_ingenious_iconoclast(int player, int card, event_t event)
{
/* Daretti, Ingenious Iconoclast	|1|B|R
 * Planeswalker - Daretti (3)
 * +1: Create a 1/1 colorless Construct artifact creature token with defender.
 * -1: You may sacrifice an artifact. If you do, destroy target artifact or creature.
 * -6: Choose target artifact card in a graveyard or artifact on the battlefield. Create three tokens that are copies of it. */

	if (IS_ACTIVATING(event))
	{
		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_ARTIFACT);

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_ARTIFACT);
		td2.preferred_controller = player;
		td2.allow_cancel = 2; // "Done" button

		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);

		enum
		{
			CHOICE_TOKEN = 1,
			CHOICE_SAC_DESTROY,
			CHOICE_3_TOKENS
		};

		enum
		{
			AI_TARGET_ZONE_PLAY = 1<<2,
		};

		if( event == EVENT_ACTIVATE ){
			instance->number_of_targets = instance->info_slot = 0;

			int can_use_sac_destroy = can_target(&td);
			if( player == AI ){
				if( ! can_sacrifice(player, player, 1, TYPE_ARTIFACT, -1) ){
					can_use_sac_destroy = 0;
				}
			}

			int ai_targeting_zones_for_3_tokens = can_target(&td2) ? AI_TARGET_ZONE_PLAY : 0;
			if( new_special_count_grave(player, &test) && ! graveyard_has_shroud(player) ){
				ai_targeting_zones_for_3_tokens |= (1<<player);
			}
			if( new_special_count_grave(1-player, &test) && ! graveyard_has_shroud(1-player) ){
				ai_targeting_zones_for_3_tokens |= (1<<(1-player));
			}

			int priority_sac_destroy = count_subtype(player, TYPE_ARTIFACT, -1)*2;
			int priority_3_tokens = 0;
			int best_target_zone_for_3_tokens = 0;
			if( player == AI ){
				int max_cmc = -1;
				if( ai_targeting_zones_for_3_tokens & AI_TARGET_ZONE_PLAY ){
					CYCLE_ALL_CARDS(p, c, 	{
												if( in_play(p, c) && is_what(p, c, TYPE_ARTIFACT) && get_cmc(p, c) > max_cmc ){
													if( would_validate_arbitrary_target(&td2, p, c) ){
														max_cmc = get_cmc(p, c);
														best_target_zone_for_3_tokens = AI_TARGET_ZONE_PLAY;
													}
												}
											};
					);
				}
				int i;
				for(i=0; i<2; i++){
					if( ai_targeting_zones_for_3_tokens & (1<<i) ){
						int selected = new_select_a_card(player, i, TUTOR_FROM_GRAVE, 0, AI_MAX_CMC, -1, &test);
						if( selected != -1 && get_cmc_by_internal_id(get_grave(i)[selected]) > max_cmc ){
							max_cmc = get_cmc_by_internal_id(get_grave(i)[selected]);
							best_target_zone_for_3_tokens = i;
						}
					}
				}
			}

			int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
								"Create a 1/1 Construct", 1, 5, 1,
								"Sac artifact & destroy", can_use_sac_destroy, priority_sac_destroy, -1,
								"3 copies of an artifact", ai_targeting_zones_for_3_tokens ? 1 : 0, priority_3_tokens, -6);
			if( ! choice ){
				cancel = 1;
				return 0;
			}

			if( choice == CHOICE_SAC_DESTROY ){
				if( ! new_pick_target(&td, "Select target artifact or creature.", 0, 1 | GS_LITERAL_PROMPT) ){
					return 0;
				}
			}

			if( choice == CHOICE_3_TOKENS ){
				int done = 0;
				if( can_target(&td2) && (player == HUMAN || (player == AI && best_target_zone_for_3_tokens == AI_TARGET_ZONE_PLAY)) ){
					if( new_pick_target(&td2, "Select target artifact.", 0, GS_LITERAL_PROMPT) ){
						done = 1;
						instance->targets[1].player = AI_TARGET_ZONE_PLAY;
						goto end;
					}
				}
				if( (ai_targeting_zones_for_3_tokens & (1<<player)) &&
					(player == HUMAN || (player == AI && best_target_zone_for_3_tokens == player)) )
				{
					scnprintf(test.message, 100, "Select target artifact card from your graveyard.");
					if( select_target_from_grave_source(player, card, player, 0, AI_MAX_CMC, &test, 0) > -1 ){
						done = 1;
						instance->targets[1].player = player;
						goto end;
					}
					if( cancel == 1 ){
						cancel = 0;
					}
				}
				if( (ai_targeting_zones_for_3_tokens & (1<<(1-player))) &&
					(player == HUMAN || (player == AI && best_target_zone_for_3_tokens == 1-player)) )
				{
					scnprintf(test.message, 100, "Select target artifact card from oppoenent's graveyard.");
					if( select_target_from_grave_source(player, card, 1-player, 0, AI_MAX_CMC, &test, 0) > -1 ){
						done = 1;
						instance->targets[1].player = 1-player;
					}
				}
				end:
				if( ! done ){
					cancel = 1;
					return 0;
				}
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			int choice = instance->info_slot;
			switch (choice)
			{
				case CHOICE_TOKEN:
				{
					token_generation_t token;
					default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_CONSTRUCT, &token);
					token.pow = token.tou = 1;
					token.key_plus = KEYWORD_DEFENDER;
					generate_token(&token);
					break;
				}

				case CHOICE_SAC_DESTROY:
				{
					if( valid_target(&td) ){
						scnprintf(test.message, 100, "Select an artifact to sacrifice.");
						if( new_sacrifice(player, card, player, 0, &test) ){
							kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
						}
					}
					break;
				}

				case CHOICE_3_TOKENS:
				{
					int csvid = -1;
					if( instance->targets[1].player == AI_TARGET_ZONE_PLAY && valid_target(&td2) ){
						csvid = get_id(instance->targets[0].player, instance->targets[0].card);
					}
					else if( instance->targets[1].player == player ){
							int selected = validate_target_from_grave_source(player, card, player, 0);
							if( selected != -1 ){
								csvid = cards_data[get_grave(player)[selected]].id;
							}
					}
					else if( instance->targets[1].player == 1-player ){
							int selected = validate_target_from_grave_source(player, card, 1-player, 0);
							if( selected != -1 ){
								csvid = cards_data[get_grave(1-player)[selected]].id;
							}
					}
					if( csvid > -1 ){
						token_generation_t token;
						default_token_definition(instance->parent_controller, instance->parent_card, csvid, &token);
						token.qty = 3;
						generate_token(&token);
					}
					break;
				}
			}
		}
	}

	return planeswalker(player, card, event, 3);
}

static int kaya_ghost_assassin_legacy(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && reason_for_trigger_controller == player ){
		card_instance_t* instance = get_card_instance(player, card);
		if( count_upkeeps(player) ){
			if( instance->targets[1].player > 0 ){
				instance->targets[1].player--;
			}
			else{
				if( upkeep_trigger(player, card, event) ){
					int owner = instance->targets[0].player;
					int iid = instance->targets[0].card;
					if( check_rfg(owner, cards_data[iid].id) ){
						remove_card_from_rfg(owner, cards_data[iid].id);
						int card_added = add_card_to_hand(owner, iid);
						put_into_play(owner, card_added);
					}
					kill_card(player, card, KILL_EXILE);
				}
			}
		}
	}

	return 0;
}

int card_kaya_ghost_assassin(int player, int card, event_t event)
{
/* Kaya, Ghost Assassin	|2|W|B	0x200f0f2
 * Planeswalker - Kaya (5)
 * 0: Exile ~ or up to one target creature. Return that card to the battlefield under its owner's control at
 * 	  the beginning of your next upkeep. You lose 2 life.
 * -1: Each opponent loses 2 life and you gain 2 life.
 * -2: Each opponent discards a card and you draw a card. */

	if (IS_ACTIVATING(event))
	{
		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		enum
		{
			CHOICE_EXILE = 1,
			CHOICE_SUCK_LIFE,
			CHOICE_DISCARD_DRAW
		};

		if( event == EVENT_ACTIVATE ){
			instance->number_of_targets = instance->info_slot = 0;

			int priority_exile = count_counters(player, card, COUNTER_LOYALTY) < 2 ? 35 : 5;
			if( priority_exile == 35 && life[player] >= 8 ){
				ai_modifier += 50;
			}

			int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
								"Exile this or target creature", 1, priority_exile, 0,
								"Opp. lose 2 life and you gain 2 life", 1, life[1-player]*3, -1,
								"Opp. discard and you draw", 1, hand_count[1-player]*2, -2);
			if( ! choice ){
				cancel = 1;
				return 0;
			}

			if( choice == CHOICE_EXILE ){
				if( priority_exile == 35 && life[player] >= 8 && player == AI ){
					instance->targets[0].player = player;
					instance->targets[0].card = card;
				}
				else{
					new_pick_target(&td, "TARGET_CREATURE", 0, 0);
				}
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			int choice = instance->info_slot;
			switch (choice)
			{
				case CHOICE_EXILE:
				{
					int valid = 1;
					if( instance->targets[0].player != player && instance->targets[0].card != card && valid_target(&td) ){
						valid = 0;
					}
					if( valid ){
						int iid = get_original_internal_card_id(instance->targets[0].player, instance->targets[0].card);
						int owner = get_owner(instance->targets[0].player, instance->targets[0].card);
						int legacy = create_legacy_effect(instance->parent_controller, instance->parent_card, kaya_ghost_assassin_legacy);
						card_instance_t* leg = get_card_instance(instance->parent_controller, legacy);
						leg->targets[0].player = owner;
						leg->targets[0].card = iid;
						if( current_turn == player && current_phase == PHASE_UPKEEP ){
							leg->targets[1].player = 1;
						}
						kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
						lose_life(player, 2);
					}
					break;
				}

				case CHOICE_SUCK_LIFE:
				{
					lose_life(1-player, 2);
					gain_life(player, 2);
					break;
				}

				case CHOICE_DISCARD_DRAW:
				{
					discard(1-player, 0, player);
					draw_cards(player, 1);
					break;
				}
			}
		}
	}

	return planeswalker(player, card, event, 5);
}

/* Knights of the Black Rose	|3|W|B	0x200f061 (Palace Sentinels, the rest of the effect is in "you_become_the_monarch")
 * Creature - Human Knight 4/4
 * When ~ enters the battlefield, you become the monarch.
 * Whenever an opponent becomes the monarch, if you were the monarch as the turn began, that player loses 2 life and you gain 2 life. */

int card_leovold_emissary_of_trest(int player, int card, event_t event)
{
/* Leovold, Emissary of Trest	|B|G|U	0x200f0fc
 * Legendary Creature - Elf Advisor 3/3
 * Each opponent can't draw more than one card each turn.
 * Whenever you or a permanent you control becomes the target of a spell or ability an opponent controls, you may draw a card. */

	check_legend_rule(player, card, event);

	if( ! in_play(player, card) || is_humiliated(player, card) ){
		return 0;
	}

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && event == EVENT_TRIGGER &&
		reason_for_trigger_controller == 1-player && !suppress_draw && in_play(player, card) &&
		cards_drawn_this_turn[reason_for_trigger_controller] > 0 )
	{
		suppress_draw = 1;
		// Deliberately do this during EVENT_TRIGGER, not EVENT_RESOLVE_TRIGGER, so nothing else can replace the (nonexistent) draw
	}

	if( any_becomes_target(player, card, event, player, -1, TYPE_PERMANENT, -1, TYPE_ANY, 1-player, RESOLVE_TRIGGER_AI(player)) ){
		draw_cards(player, 1);
	}

	if( becomes_target_of_spell_or_effect(player, card, event, player, -1, 1-player) ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_queen_marchesa(int player, int card, event_t event){
/* Queen Marchesa	|1|R|W|B	0x200f101
 * Legendary Creature - Human Assassin 3/3
 * Deathtouch, haste
 * When ~ enters the battlefield, you become the monarch.
 * At the beginning of your upkeep, if an opponent is the monarch, create a 1/1 |Sblack Assassin creature token with deathtouch and haste. */

	check_legend_rule(player, card, event);
	deathtouch(player, card, event);
	haste(player, card, event);

	if( current_turn == player && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		if( who_is_the_monarch() == player && upkeep_trigger(player, card, event) ){
			upkeep_trigger_ability(player, card, event, player);
		}
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ASSASSIN, &token);
		token.pow = token.tou = 1;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		token.s_key_plus = SP_KEYWORD_HASTE | SP_KEYWORD_DEATHTOUCH;
		generate_token(&token);
	}

	return card_palace_sentinels(player, card, event);
}

// ARTIFACT

/* Spy Kit	|2	0x0000000
 * Artifact - Equipment
 * Equipped creature gets +1/+1 and has all names of nonlegendary creature cards in addition to its name.
 * Equip |2 */

int card_throne_of_the_high_city(int player, int card, event_t event){
/* Throne of the High City	0x200f106
 * Land
 * |T: Add |C to your mana pool.
 * |4, |T, Sacrifice ~: You become the monarch. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_MONARCH
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(5), 0, NULL, NULL) )
		{
			choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Produce mana", 1, 5,
							"You become the monarch", 1,  who_is_the_monarch() != player ? 10 : 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_MONARCH ){
			instance->state |= STATE_TAPPED;
			if( charge_mana_for_activated_ability(player, card, MANACOST_X(4)) ){
				instance->info_slot = 1;
				tap_card(player, card); // Now get the proper event sent
				kill_card(player, card, KILL_SACRIFICE);
			}
			else{
				untap_card_no_event(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_MONARCH ){
			you_become_the_monarch(player);
		}
	}

	return 0;
}


