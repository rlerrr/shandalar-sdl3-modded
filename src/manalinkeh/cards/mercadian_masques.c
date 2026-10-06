#include "manalink.h"

// Functions
int generic_subtype_searcher(int player, int card, event_t event, int subtype, int val, int mode){
	// mode = 0 --> CMC < val
	// mode = 1 --> CMC <= val
	// mode = 2 --> any CMC

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );
		card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "");
		this_test.subtype = subtype;
		if( mode != 2 ){
			this_test.cmc = val+mode;
			this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
			if (ai_is_speculating != 1){
				scnprintf(this_test.message, 100, "Select %s permanent card with CMC %d or less.", get_subtype_text("%a", subtype), val + mode - 1);
			}
		} else if (ai_is_speculating != 1){
			strcpy(this_test.message, get_subtype_text("Select %a permanent card.", subtype));
		}
		parent->targets[2].card = 2+new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DECK_SEARCHER, MANACOST_X(val), 0, NULL, NULL);
}

static int count_type_in_hand(int player, int type){
	// 0x100 = TYPE_PLANESWALKER
	// 0x200 = TYPE_TRIBAL

	int count = 0;
	int result = 0;
	while( count < active_cards_count[player] ){
			if( in_hand(player, count) ){
				if( type & 0x100 ){
					if( is_planeswalker(player, count) ){
						result++;
					}
				}
				else if( type & 0x200 ){
						if( ! is_what(player, count, TYPE_CREATURE) && is_tribal(player, count) ){
							result++;
						}
				}
				else{
					if( is_what(player, count, type)  ){
						result++;
					}
				}

			}
			count++;
	}
	return result;
}

static int part_of_ramos(int player, int card, event_t event, int manacolor){

	if( ! IS_GAA_EVENT(event) && !(event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( ! is_tapped(player, card) && ! is_animated_and_sick(player, card) ){
			declare_mana_available(player, manacolor, 2);
		}
		else{
			declare_mana_available(player, manacolor, 1);
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return can_produce_mana(player, card);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		if( ! is_tapped(player, card) && ! is_animated_and_sick(player, card) ){
			if( player != AI ){
				choice = do_dialog(player, player, card, -1, -1, " Tap to add mana\n Sac to add mana\n Cancel", 0);
			}
		}
		else{
			choice = 1;
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		else if( choice == 1 ){
				ai_modifier -= 36;

				produce_mana(player, manacolor, 1);
				kill_card(player, card, KILL_SACRIFICE);
		}
		else{
			spell_fizzled = 1;
		}
	}

	return 0;
}

static int depletion_land(int player, int card, event_t event, int manacolor)
{
  /* Land
   * ~ enters the battlefield tapped with two depletion counters on it.
   * |T, Remove a depletion counter from ~: Add |C|C to your mana pool. If there are no depletion counters on ~, sacrifice it. */

	comes_into_play_tapped(player, card, event);
	enters_the_battlefield_with_counters(player, card, event, COUNTER_DEPLETION, 2);

	if (event == EVENT_RESOLVE_SPELL)
		play_land_sound_effect(player, card);

	if (event == EVENT_CAN_ACTIVATE)
		return CAN_TAP_FOR_MANA(player, card) && count_counters(player, card, COUNTER_DEPLETION);

	if (event == EVENT_ACTIVATE)
	{
		if( count_counters(player, card, COUNTER_DEPLETION) == 1 )
			ai_modifier -= 36;
		remove_counter(player, card, COUNTER_DEPLETION);
		produce_mana_tapped(player, card, manacolor, 2);
		if (count_counters(player, card, COUNTER_DEPLETION) == 0)
			kill_card(player, card, KILL_SACRIFICE);
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && CAN_TAP_FOR_MANA(player, card) &&
		count_counters(player, card, COUNTER_DEPLETION) )
		declare_mana_available(player, manacolor, 2);

	return 0;
}

static int rishadan_delinquents(int player, int card, event_t event, int mana_req){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int sac = 1;
		if( has_mana(1-player, COLOR_COLORLESS, mana_req) ){
			charge_mana(1-player, COLOR_COLORLESS, mana_req);
			if( spell_fizzled != 1 ){
				sac = 0;
			}
		}
		if( sac == 1 ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent to sacrifice.");
			new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &test);
		}
	}

	return 0;
}


// Cards

int card_alley_grifters(int player, int card, event_t event ){
/* CARD_ID_ALLEY_GRIFTERS	4382
Alley Grifters	|1|B|B
Creature - Human Mercenary 2/2
Whenever ~ becomes blocked, defending player discards a card. */

	if( event == EVENT_DECLARE_BLOCKERS && is_attacking(player, card) && !is_unblocked(player, card) && ! is_humiliated(player, card) ){
		discard(1-player, 0, player);
	}
	return 0;
}

int card_ancestral_mask(int player, int card, event_t event ){
/* CARD_ID_ANCESTRAL_MASK	4383
Ancestral Mask	|2|G
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2 for each other enchantment on the battlefield. */

	if( in_play(player, card) && get_card_instance( player, card )->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance( player, card );
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			int plus = 2*(count_subtype(2, TYPE_ENCHANTMENT, -1)-1);
			if( plus < 0 ){
				plus = 0;
			}
			event_result+=plus;
		}
	}

	return vanilla_aura(player, card, event, player);
}

int card_arms_dealer(int player, int card, event_t event ){
/*
Arms Dealer |2|R
Creature — Goblin Rogue 1/1
{1}{R}, Sacrifice a Goblin: Arms Dealer deals 4 damage to target creature.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Goblin to sacrifice.");
	test.subtype = SUBTYPE_GOBLIN;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0,
															&td, "TARGET_CREATURE", 0, &test, 0);
}

int card_arrest(int player, int card, event_t event){
/* CARD_ID_ARREST	4386
Arrest	|2|W
Enchantment - Aura
Enchant creature
Enchanted creature can't attack or block, and its activated abilities can't be activated. */

	// Used externally
	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance( player, card )->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance( player, card );
		cannot_attack(instance->damage_target_player, instance->damage_target_card, event);
		cannot_block(instance->damage_target_player, instance->damage_target_card, event);
		if( leaves_play(player, card, event) ){
			disable_all_activated_abilities(instance->damage_target_player, instance->damage_target_card, 0);
			return 0;
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	return disabling_targeted_aura(player, card, event, &td, "Select target creature.", 2);
}

int card_bargaining_table(int player, int card, event_t event ){
/* CARD_ID_BARGAINING_TABLE	4391
Bargaining Table	|5
Artifact
|X, |T: Draw a card. X is the number of cards in an opponent's hand. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(hand_count[1-player]), 0, NULL, NULL);
}

int card_bifurcate(int player, int card, event_t event){
/* CARD_ID_BIFURCATE	4394
Bifurcate	|3|G
Sorcery
Search your library for a permanent card with the same name as target nontoken creature and put that card onto the battlefield. Then shuffle your library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_type = TARGET_TYPE_TOKEN;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nontoken creature", 1, NULL);
		if( spell_fizzled != 1 ){
			if( get_card_name(instance->targets[0].player, instance->targets[0].card) == -1 ){
				ai_modifier -= 128;
			}
		}
		return 0;
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			tutor_card_with_the_same_name(instance->targets[0].player, instance->targets[0].card, player, player, TUTOR_FROM_DECK,
											TUTOR_PLAY, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nontoken creature", 1, NULL);
}

int card_black_market(int player, int card, event_t event ){

	/* Black Market	|3|B|B
	 * Enchantment
	 * Whenever a creature dies, put a charge counter on ~.
	 * At the beginning of your precombat main phase, add |B to your mana pool for each charge counter on ~. */

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, 0);
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( result ){
		add_counters(player, card, COUNTER_CHARGE, BYTE0(result));
	}

	if( beginning_of_phase(player, card, event, player, card, PHASE_MAIN1) ){
		produce_mana(player, COLOR_BLACK, count_counters(player, card, COUNTER_CHARGE));
	}

	return global_enchantment(player, card, event);
}

int card_blood_oath(int player, int card, event_t event){
/* CARD_ID_BLOOD_OATH	4399
Blood Oath	|3|R
Instant
Choose a card type. Target opponent reveals his or her hand. ~ deals 3 damage to that player for each card of the chosen type revealed this way.
(Artifact, creature, enchantment, instant, land, planeswalker, sorcery, and tribal are card types.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int types[8] = {TYPE_LAND, TYPE_CREATURE, TYPE_ENCHANTMENT, TYPE_SORCERY, TYPE_INSTANT, TYPE_ARTIFACT, 0x100, 0x200};
			int choice = do_dialog(player, player, card, -1, -1, " Land\n Creature\n Enchantment\n Sorcery\n Instant\n Artifact\n Planeswalker\n Tribal", internal_rand(8));
			int sel_type = types[choice];

			int amount = count_type_in_hand(instance->targets[0].player, sel_type);

			reveal_target_player_hand(instance->targets[0].player);

			if( amount > 0 ){
				damage_player(instance->targets[0].player, 3*amount, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_bog_witch(int player, int card, event_t event ){
/* CARD_ID_BOG_WITCH	4402
Bog Witch	|2|B
Creature - Human Spellshaper 1/1
|B, |T, Discard a card: Add |B|B|B to your mana pool. */

	if( ! IS_GAA_EVENT(event) && !(event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		return 0;
	}

	int active = can_use_activated_abilities(player, card) && has_mana(player, COLOR_BLACK, 1) && ! is_tapped(player, card) &&
				! is_sick(player, card);

	if( event == EVENT_CAN_ACTIVATE && active && hand_count[player] > 0 ){
		return 1;
	}

	if( event == EVENT_ACTIVATE ){
		card_instance_t* instance = get_card_instance(player, card);
		instance->state |= STATE_TAPPED;
		charge_mana(player, COLOR_BLACK, 1);
		if( spell_fizzled == 1 ){
			instance->state &= ~STATE_TAPPED;
		}
		else{
			discard(player, 0, player);
			produce_mana_tapped(player, card, COLOR_BLACK, 3);
		}
	}

	if( event == EVENT_COUNT_MANA ){
		if( affect_me(player, card) && active && hand_count[player] > 0 ){
			declare_mana_available(player, COLOR_BLACK, 3);
		}
	}

	return 0;
}

int card_briar_patch(int player, int card, event_t event)
{
/* CARD_ID_BRIAR_PATCH	4404
Briar Patch	|1|G|G
Enchantment
Whenever a creature attacks you, it gets -1/-0 until end of turn. */

  int amt;
  if ((amt = declare_attackers_trigger(player, card, event, DAT_TRACK | DAT_ATTACKS_PLAYER, 1-player, -1)))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
	  for (--amt; amt >= 0; --amt)
		if (in_play(current_turn, attackers[amt]) )
		  pump_until_eot(player, card, current_turn, attackers[amt], -1,0);
	}

  return global_enchantment(player, card, event);
}

int card_bribery(int player, int card, event_t event){
/* CARD_ID_BRIBERY	985
Bribery	|3|U|U
Sorcery
Search target opponent's library for a creature card and put that card onto the battlefield under your control. Then that player shuffles his or her library. */

	// original code : 0040F730
	if( IS_GS_EVENT(player, card, event) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card");
		return steal_permanent_from_target_opponent_deck(player, card, event, &this_test);
	}
	return 0;
}

int card_cackling_witch(int player, int card, event_t event){
/* CARD_ID_CACKLING_WITCH	4406
Cackling Witch	|1|B
Creature - Human Spellshaper 1/1
|X|B, |T, Discard a card: Target creature gets +X/+0 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card,
							instance->targets[0].player, instance->targets[0].card, instance->info_slot, 0);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DISCARD, MANACOST_XB(-1, 1), 0,
									&td, "TARGET_CREATURE");
}

int card_cateran_brute(int player, int card, event_t event ){
/* CARD_ID_CATERAN_BRUTE	4408
Cateran Brute	|2|B
Creature - Horror Mercenary 2/2
|2, |T: Search your library for a Mercenary permanent card with converted mana cost 2 or less and put it onto the battlefield. Then shuffle your library. */

	return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 2, 1);
}

int card_cateran_enforcer(int player, int card, event_t event ){
/*CARD_ID_CATERAN_ENFORCER	4409
Cateran Enforcer	|3|B|B
Creature - Horror Mercenary 4/3
Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
|4, |T: Search your library for a Mercenary permanent card with converted mana cost 4 or less and put it onto the battlefield. Then shuffle your library. */

	fear(player, card, event);

	return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 4, 1);
}

int card_cateran_kidnapper(int player, int card, event_t event ){
/* CARD_ID_CATERAN_KIDNAPPERS	4410
Cateran Kidnappers	|2|B|B
Creature - Human Mercenary 4/2
|3, |T: Search your library for a Mercenary permanent card with converted mana cost 3 or less and put it onto the battlefield. Then shuffle your library. */

	return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 3, 1);
}

int card_cateran_overlord(int player, int card, event_t event ){
/*
Cateran Overlord |4|B|B|B
Creature — Horror Mercenary 7/5
Sacrifice a creature: Regenerate Cateran Overlord.
{6}, {T}: Search your library for a Mercenary permanent card with converted mana cost 6 or less and put it onto the battlefield.
		Then shuffle your library.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_REGENERATE = 1,
		CHOICE_TUTOR
	};

	if( event == EVENT_CAN_ACTIVATE ){
		int result = regeneration(player, card, event, MANACOST0);
		if( result && new_can_sacrifice_as_cost(player, card, &test) ){
			if( ! IS_AI(player) ){
				return 99;
			}
			else{
				CYCLE_ALL_CARDS(p, c,	{
											if( p == player && in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
												if( c != card && ! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) ){
													return 99;
												}
											}
										}
				);
			}
		}
		return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 6, 0);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, 0, 0};
		abils[1] = regeneration(player, card, EVENT_CAN_ACTIVATE, MANACOST0) && new_can_sacrifice_as_cost(player, card, &test) ? 1 : 0;
		if( IS_AI(player) && abils[1] ){
			abils[1] = 0;
			CYCLE_ALL_CARDS(p, c,	{
										if( p == player && in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
											if( c != card && ! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) ){
												abils[1] = 1;
											}
										}
									}
			);
		}
		abils[2] = generic_subtype_searcher(player, card, EVENT_CAN_ACTIVATE, SUBTYPE_MERCENARY, 6, 0);
		int choice = CHOICE_TUTOR;
		if( abils[1] ){
			choice = CHOICE_REGENERATE;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Regenerate", 		abils[1], 1,
							"Tutor Mercenary", 	abils[2], 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_REGENERATE ){
			if( IS_AI(player) ){
				state_untargettable(player, card, 1);
			}
			generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL);
			if( IS_AI(player) ){
				state_untargettable(player, card, 0);
			}
		}
		if( choice == CHOICE_TUTOR ){
			return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 6, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_REGENERATE && can_regenerate(instance->parent_controller, instance->parent_card) ){
			regenerate_target(instance->parent_controller, instance->parent_card);
		}
		if( instance->info_slot == CHOICE_TUTOR ){
			return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 6, 0);
		}
	}

	return 0;
}

int card_cateran_persuader(int player, int card, event_t event ){
/* CARD_ID_CATERAN_PERSUADER	4412
Cateran Persuader	|B|B
Creature - Human Mercenary 2/1
|1, |T: Search your library for a Mercenary permanent card with converted mana cost 1 or less and put it onto the battlefield. Then shuffle your library.*/

	return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 1, 1);
}

int card_cateran_slavers(int player, int card, event_t event ){
/* CARD_ID_CATERAN_SLAVER	4413
Cateran Slaver	|4|B|B
Creature - Horror Mercenary 5/5
|H2Swampwalk (This creature can't be blocked as long as defending player controls |Ha Swamp.)
|5, |T: Search your library for a Mercenary permanent card with converted mana cost 5 or less and put it onto the battlefield. Then shuffle your library. */

	return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 5, 1);
}

int card_cateran_summons(int player, int card, event_t event ){
/* CARD_ID_CATERAN_SUMMONS	4414
Cateran Summons	|B
Sorcery
Search your library for a Mercenary card, reveal that card, and put it into your hand. Then shuffle your library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (56-(hand_count[player]*8));
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a Mercenary card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_cave_in(int player, int card, event_t event){
/* CARD_ID_CAVE_IN	4417
Cave-In	|3|R|R
Sorcery
You may exile a |Sred card from your hand rather than pay ~'s mana cost.
~ deals 2 damage to each creature and each player. */

	al_pitchspell(player, card, event, COLOR_RED, 0);

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! casting_al_pitchspell(player, card, event, COLOR_RED, 0) ){
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		new_damage_all(player, card, ANYBODY, 2, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_chameleon_spirit(int player, int card, event_t event){
/* CARD_ID_CHAMELEON_SPIRIT	4421
Chameleon Spirit	|3|U
Creature - Illusion Spirit 100/100
As ~ enters the battlefield, choose a color.
~'s power and toughness are each equal to the number of permanents of the chosen color your opponents control. */

	if (card == -1){
		return 0;	// no power/toughness except on battlefield
	}

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->targets[1].player = 1<<choose_a_color_and_show_legacy(player, card, 1-player, -1);
	}

	if( get_card_instance(player, card)->targets[1].player > 0 && ! is_humiliated(player, card) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			test_definition_t test;
			default_test_definition(&test, TYPE_PERMANENT);
			test.color = get_card_instance(player, card)->targets[1].player;

			event_result += check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &test);
		}
	}

	return 0;
}

int card_charisma(int player, int card, event_t event){
/* Charisma	|U|U|U
 * Enchantment - Aura
 * Enchant creature
 * Whenever enchanted creature deals damage to a creature, gain control of the other creature for as long as ~ remains on the battlefield. */

	 if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;
		if( event == EVENT_DEAL_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_target_card != -1 && damage->damage_source_player == t_player &&
					damage->damage_source_card == t_card && damage->info_slot > 0 &&
					is_what(damage->damage_target_player, damage->damage_target_card, TYPE_CREATURE) )
				{
					int pos = instance->info_slot;
					instance->targets[pos].player = damage->damage_target_player;
					instance->targets[pos].card = damage->damage_target_card;
					instance->info_slot++;
				}
			}
		}
		if( instance->info_slot > 1 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
			reason_for_trigger_controller == player )
		{
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					int i;
					for(i=0;i<instance->info_slot;i++){
						gain_control_until_source_is_in_play_and_tapped(player, card, instance->targets[i].player, instance->targets[i].card, GCUS_SOURCE_IN_PLAY);
					}
					instance->info_slot = 0;
			}
		}
	}

	return vanilla_aura(player, card, event, player);
}

int card_cho_arrim_alchemist(int player, int card, event_t event){
/* CARD_ID_CHO_ARRIM_ALCHEMIST	4425
Cho-Arrim Alchemist	|W
Creature - Human Spellshaper 1/1
|1|W|W, |T, Discard a card: The next time a source of your choice would deal damage to you this turn, prevent that damage.
You gain life equal to the damage prevented this way. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ANY);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			gain_life(player, target->info_slot);
			prevent_damage(instance->targets[0].player, instance->targets[0].card, -1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DAMAGE_PREVENTION|GAA_DISCARD,
									MANACOST_XW(1, 2), 0, &td, "TARGET_DAMAGE");
}

int card_cho_arrim_bruiser(int player, int card, event_t event)
{
/* CARD_ID_CHO_ARRIM_BRUISER	4426
Cho-Arrim Bruiser	|5|W
Creature - Ogre Rebel 3/4
Whenever ~ attacks, you may tap up to two target creatures. */

	if (declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), player, card))
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 3;

		card_instance_t* instance = get_card_instance(player, card);

		pick_up_to_n_targets(&td, "TARGET_CREATURE", 2);
		int i;
		for (i = 0; i < instance->number_of_targets; ++i)
			tap_card(instance->targets[i].player, instance->targets[i].card);
		instance->number_of_targets = 0;
	}

  return 0;
}

int card_cho_manno_revolutionary(int player, int card, event_t event ){
/* CARD_ID_CHO_MANNO_REVOLUTIONARY	4429
Cho-Manno, Revolutionary	|2|W|W
Legendary Creature - Human Rebel 2/2
Prevent all damage that would be dealt to ~. */

	check_legend_rule(player, card, event);

	prevent_all_damage(player, card, event);

	return 0;
}

int card_cinder_elemental(int player, int card, event_t event ){
	/* Cinder Elemental	|3|R	0x2009922
	 * Creature - Elemental 2/2
	 * |X|R, |T, Sacrifice ~: ~ deals X damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_SACRIFICE_ME|GAA_CAN_TARGET, MANACOST_XR(-1, 1), 0,
									&td, "TARGET_ANY");
}

int card_collective_unconscious(int player, int card, event_t event){
	/*
	  Collective Unconscious |4|G|G
	  Sorcery
	  Draw a card for each creature you control.
	*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1)*8);
		ai_modifier -= (hand_count[player]*8);
	}

	if(event == EVENT_RESOLVE_SPELL){
		draw_cards(player, count_subtype(player, TYPE_CREATURE, -1));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_coastal_piracy(int player, int card, event_t event){
	// original code : 00405650
	/*
	  Coastal Piracy |2|U|U
	  Enchantment
	  Whenever a creature you control deals combat damage to an opponent, you may draw a card.
	*/
	// Used externally
	if( event == EVENT_DEAL_DAMAGE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		check_damage_test(player, -1, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE,
							player, card, 0, &test);
	}

	if( resolve_damage_trigger(player, -1, event, RESOLVE_TRIGGER_AI(player) | DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS |
								DDBM_MUST_BE_COMBAT_DAMAGE, player, card) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		APNAP(p,{
					if( times_damaged[p] ){
						draw_some_cards_if_you_want(player, card, player, times_damaged[p]);

					}
				};
		);
	}

	return global_enchantment(player, card, event);
}

int card_common_cause(int player, int card, event_t event ){
/*
Common Cause |2|W
Enchantment
Nonartifact creatures get +2/+2 as long as they all share a color.
*/
	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && in_play(player, card) && ! is_humiliated(player, card) ){
		if( !is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) ){
			int color_mask = 0;
			CYCLE_ALL_CARDS(p, c, 	{
										if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) && ! is_what(p, c, TYPE_ARTIFACT) ){
											int this_color = get_color(p, c);
											if( this_color == 0 ){
												return 0;
											}
											if( color_mask == 0 ){
												color_mask = this_color;
											}
											if( !( this_color & color_mask) ){
												return 0;
											}
										}
									};
			);
			event_result+=2;
		}
	}

	return global_enchantment(player, card, event);
}

int card_crackdown(int player, int card, event_t event ){
/* CARD_ID_CRACKDOWN	4438
Crackdown	|2|W
Enchantment
Non|Swhite creatures with power 3 or greater don't untap during their controllers' untap steps. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
		{
			if( !(get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_WHITE)) &&
				get_power(affected_card_controller, affected_card) > 2 )
			{
				does_not_untap(affected_card_controller, affected_card, event);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_crooked_scales(int player, int card, event_t event)
{
/* Crooked Scales	|4
 * Artifact
 * |4, |T: Flip a coin. If you win the flip, destroy target creature an opponent controls. If you lose the flip, destroy target creature
 * 		you control unless you pay |3 and repeat this process. */

  if (!IS_ACTIVATING(event))
	return 0;

  target_definition_t td_opp;
  default_target_definition(player, card, &td_opp, TYPE_CREATURE);
  td_opp.allowed_controller = td_opp.preferred_controller = 1-player;

  target_definition_t td_own;
  default_target_definition(player, card, &td_own, TYPE_CREATURE);
  td_own.allowed_controller = td_own.preferred_controller = player;

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CAN_ACTIVATE)
	return CAN_TAP(player, card) && CAN_ACTIVATE(player, card, MANACOST_X(4)) && can_target(&td_opp) && can_target(&td_own);

  if (event == EVENT_ACTIVATE)
	{
	  instance->number_of_targets = 0;
	  if (charge_mana_for_activated_ability(player, card, MANACOST_X(4))
		  && pick_target(&td_opp, "TARGET_CREATURE_OPPONENT_CONTROLS")
		  && new_pick_target(&td_own, "ASHNODS_BATTLEGEAR", 1, 1))	// "Select target creature you control."
		tap_card(player, card);
	  else
		instance->number_of_targets = 0;	// in case it was cancelled during second target choice
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  int valid_opp = valid_target(&td_opp), valid_own = validate_target(player, card, &td_own, 1);
	  if (!valid_opp && !valid_own)
		{
		  spell_fizzled = 1;
		  return 0;
		}

	  while (1)
		{
		  if (flip_a_coin(player, card))
			{
			  if (valid_opp)
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			  break;
			}
		  else if (has_mana(player, COLOR_ANY, 3)
				   && do_dialog(player, player, card, valid_own ? instance->targets[1].player : -1, valid_own ? instance->targets[1].card : -1,
								" Pay 3\n Destroy creature you control", 0) == 0
				   && charge_mana_while_resolving(player, card, EVENT_RESOLVE_ACTIVATION, player, COLOR_COLORLESS, 3))
			continue;
		  else
			{
			  if (valid_own)
				kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
			  break;
			}
		}
	}

  return 0;
}

int card_crumbling_sanctuary(int player, int card, event_t event ){
/* CARD_ID_CRUMBLING_SANCTUARY	4444
Crumbling Sanctuary	|5
Artifact
If damage would be dealt to a player, that player exiles that many cards from the top of his or her library instead. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_target_card == -1 && damage->info_slot > 0 && !damage_is_to_planeswalker(damage) ){
					int amount = MIN(count_deck(damage->damage_target_player), damage->info_slot);
					int result = prevent_damage(affected_card_controller, affected_card, amount);
					int i;
					for(i=0; i<result; i++){
						rfg_top_card_of_deck(damage->damage_target_player);
					}
				}
			}
		}
	}

	return 0;
}

int card_custom_depot(int player, int card, event_t event ){
/* CARD_ID_CUSTOMS_DEPOT	4445
Customs Depot	|1|U
Enchantment
Whenever you cast a creature spell, you may pay |1. If you do, draw a card, then discard a card. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) ){
			if( charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1) ){
				draw_cards(player, 1);
				discard(player, 0, player);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_dawnstrider(int player, int card, event_t event ){
/* CARD_ID_DAWNSTRIDER	4447
Dawnstrider	|1|G
Creature - Dryad Spellshaper 1/1
|G, |T, Discard a card: Prevent all combat damage that would be dealt this turn. */

	if( event == EVENT_ACTIVATE ){
		if( current_turn == 1-player ){
			ai_modifier += (count_attackers(current_turn)*8);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );
		fog_effect(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD, MANACOST_G(1), 0, NULL, NULL);
}

static void destroy_nonblack_at_end_of_combat(int player, int card, int t_player, int t_card)
{
  if (!(get_color(t_player, t_card) & get_sleighted_color_test(player, card, COLOR_TEST_BLACK)))
	create_legacy_effect_exe(player, card, LEGACY_EFFECT_STONING, t_player, t_card);
}

int card_deathgazer(int player, int card, event_t event)
{
/* CARD_ID_DEATHGAZER	1010
Deathgazer	|3|B
Creature - Lizard 2/2
Whenever ~ blocks or becomes blocked by a non|Sblack creature, destroy that creature at end of combat. */

  // Used externally
  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card))
	instance->destroys_if_blocked |= DIFB_ASK_CARD;

  if (event == EVENT_CHECK_DESTROY_IF_BLOCKED && affect_me(player, card) && !is_humiliated(player, card)
	  && !(get_color(attacking_card_controller, attacking_card) & get_sleighted_color_test(player, card, COLOR_TEST_BLACK)))
	event_result |= 1;

  if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card))
	{
	  if (player == current_turn && (instance->state & STATE_ATTACKING))
		for_each_creature_blocking_me(player, card, destroy_nonblack_at_end_of_combat, player, card);

	  if (player == 1-current_turn && (instance->state & STATE_BLOCKING))
		for_each_creature_blocked_by_me(player, card, destroy_nonblack_at_end_of_combat, player, card);
	}

  return 0;
}

int card_deepwood_drummer(int player, int card, event_t event){
/* CARD_ID_DEEPWOOD_DRUMMER	4448
Deepwood Drummer	|1|G
Creature - Human Spellshaper 1/1
|G, |T, Discard a card: Target creature gets +2/+2 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD|GAA_CAN_TARGET, MANACOST_G(1), 0, &td, "TARGET_CREATURE");
}

int card_deepwood_ghoul(int player, int card, event_t event){
	/*
	  Deepwood Ghoul |2|B
	  Creature - Zombie 2/1
	  Pay 2 life: Regenerate Deepwood Ghoul.
	*/
	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( can_regenerate(instance->parent_controller, instance->parent_card) ){
			regenerate_target(instance->parent_controller, instance->parent_card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_REGENERATION, MANACOST0, 2, NULL, NULL);
}

int card_dehydration(int player, int card, event_t event)
{
/* CARD_ID_DEHYDRATION	1014
Dehydration	|3|U
Enchantment - Aura
Enchant creature (Target a creature as you cast this. This card enters the battlefield attached to that creature.)
Enchanted creature doesn't untap during its controller's untap step. */

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t* instance = get_card_instance(player, card);
		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);
	}

	return disabling_aura(player, card, event);
}

int card_delraich(int player, int card, event_t event){
	/*
	  Delraich |6|B
	  Creature - Horror 6/6
	  Trample
	  You may sacrifice three black creatures rather than pay Delraich's mana cost.
	*/

	if( event == EVENT_MODIFY_COST ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		test.qty = 3;

		if( new_can_sacrifice_as_cost(player, card, &test) ){
			get_card_instance( player, card )->info_slot = 1;
			null_casting_cost(player, card);
		}
		else{
			get_card_instance( player, card )->info_slot = 0;
		}

	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_iid(player, event, instance->internal_card_id) ){
				choice = do_dialog(player, player, card, -1, -1, " Sac 3 black creatures\n Pay mana cost\n Cancel", 1);
			}

			if( choice == 0 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_CREATURE, "Select a black creature to sacrifice.");
				test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
				int ts = 0;
				int sacced[3];
				while( ts < 3 ){
						int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
						if (!sac){
							cancel = 1;
							break;
						}
						sacced[ts] = sac;
						ts++;
				}
				int i;
				for(i=0; i<ts; i++){
					state_untargettable(BYTE2(sacced[i]), BYTE3(sacced[i]), 0);
					if( ts == 3 ){
						kill_card(BYTE2(sacced[i]), BYTE3(sacced[i]), KILL_SACRIFICE);
					}
				}
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
	          else{
				   spell_fizzled = 1;
			      }
		}

	}

	return 0;
}

int card_devout_witness(int player, int card, event_t event ){
/* CARD_ID_DEVOUT_WITNESS	4454
Devout Witness	|2|W
Creature - Human Spellshaper 2/2
|1|W, |T, Discard a card: Destroy target artifact or enchantment. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT | TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD|GAA_CAN_TARGET, MANACOST_XW(1, 1), 0, &td, "DISENCHANT");
}

int card_diplomatic_immunity(int player, int card, event_t event ){
	/*
	  Diplomatic Immunity |1|U
	  Enchantment - Aura
	  Enchant creature
	  Shroud (A permanent with shroud can't be the target of spells or abilities.)
	  Enchanted creature has shroud.
	*/

	return generic_aura(player, card, event, player, 0, 0, KEYWORD_SHROUD, 0, 0, 0, 0);
}

int card_distorting_lens(int player, int card, event_t event){
	/*
	  Distorting Lens English
	  Artifact, 2
	  {T}: Target permanent becomes the color of your choice until end of turn.
	*/
	if (!IS_GAA_EVENT(event)  ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		card_instance_t *instance = get_card_instance(player, card);
		change_color(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
					1<<(choose_a_color(player, get_deck_color(player, 1-player))), 2 | 4);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PERMANENT");
}

int card_dust_bowl(int player, int card, event_t event){
/* Dust Bowl	""
 * Land
 * |T: Add |1 to your mana pool.
 * |3, |T, Sacrifice a land: Destroy target nonbasic land. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t* instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = SUBTYPE_NONBASIC;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

	enum{
		CHOICE_MANA = 1,
		CHOICE_KILL_LAND,
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() ){
			if( generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_UNTAPPED,
																MANACOST_X(4), 0, &td, NULL, 0, &test, 0) )
			{
				choice = CHOICE_KILL_LAND;
			}
		}
		instance->info_slot = choice;
		if( choice == CHOICE_KILL_LAND ){
			if( count_subtype(player, TYPE_LAND, -1) >= 6 || count_subtype(1-player, TYPE_LAND, -1) <= 2 ){
				ai_modifier += 48;
			}
		}
		if( ! IS_AI(player) && choice != CHOICE_MANA ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Destroy a nonbasic land ", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_KILL_LAND ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT,
															MANACOST_X(3), 0, &td, "Select target nonbasic land.", 0, &test, 0);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_KILL_LAND ){
			if( valid_target(&td) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
		}
	}

	return 0;
}

int card_embargo(int player, int card, event_t event ){
/* CARD_ID_EMBARGO	4458
Embargo	|3|U
Enchantment
Nonland permanents don't untap during their controllers' untap steps.
At the beginning of your upkeep, you lose 2 life. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && ! is_what(affected_card_controller, affected_card, TYPE_LAND) ){
			does_not_untap(affected_card_controller, affected_card, event);
		}
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		lose_life(player, 2);
	}

	return global_enchantment(player, card, event);
}

int card_enslaved_horror(int player, int card, event_t event ){
/* CARD_ID_ENSLAVED_HORROR	4459
Enslaved Horror	|3|B
Creature - Horror 4/4
When ~ enters the battlefield, each other player may return a creature card from his or her graveyard to the battlefield. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int count = 0;
		int max_cmc = 0;
		while( get_grave(1-player)[count] != -1 ){
				int iid = get_grave(1-player)[count];
				if( is_what(-1, iid, TYPE_CREATURE) ){
					int this_cmc = get_cmc_by_internal_id(iid);
					if( this_cmc > max_cmc ){
						max_cmc = this_cmc;
					}
				}
				count++;
		}
		ai_modifier += (max_cmc > 3 ? -128 : 24);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
		if( count_graveyard_by_type(1-player, TYPE_CREATURE) ){
			new_global_tutor(1-player, 1-player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
		}
	}

	return 0;
}

int card_eye_of_ramos(int player, int card, event_t event ){
/* CARD_ID_EYE_OF_RAMOS	4463
Eye of Ramos	|3
Artifact
|T: Add |U to your mana pool.
Sacrifice ~: Add |U to your mana pool. */

	return part_of_ramos(player, card, event, COLOR_BLUE);
}

int card_forced_march(int player, int card, event_t event){
/* CARD_ID_FORCED_MARCH	4470
Forced March	|X|B|B|B
Sorcery
Destroy all creatures with converted mana cost X or less. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card );

		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.cmc = instance->info_slot+1;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		APNAP(p, { new_manipulate_all(player, card, p, &test, KILL_DESTROY); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

static int foster_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_FOSTER ){
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
				count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			int amount = BYTE0(result);
			while( amount && has_mana(player, COLOR_ANY, 1) ){
					int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
										"Activate Foster", 1, count_deck(player) >= 20 ? 10 : 1,
										"Decline", 1, 5);
					if( choice == 1 ){
						if( charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1) ){
							test_definition_t this_test;
							default_test_definition(&this_test, TYPE_CREATURE);
							reveal_card_until_you_reveal_test(player, player, &this_test, RCUYRT_ADD_TEST_TO_HAND | RCUYRT_MILL_REST);
						}
					}
					else{
						break;
					}
					amount--;
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

int card_foster(int player, int card, event_t event){
/*
Foster |2|G|G
Enchantment
Whenever a creature you control dies, you may pay {1}. If you do, reveal cards from the top of your library until you reveal a
	creature card. Put that card into your hand and the rest into your graveyard.
*/

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)foster_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &foster_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY){
		count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			int amount = BYTE0(result);
			while( amount && has_mana(player, COLOR_ANY, 1) ){
					int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
										"Activate Foster", 1, count_deck(player) >= 20 ? 10 : 1,
										"Decline", 1, 5);
					if( choice == 1 ){
						if( charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1) ){
							test_definition_t this_test;
							default_test_definition(&this_test, TYPE_CREATURE);
							reveal_card_until_you_reveal_test(player, player, &this_test, RCUYRT_ADD_TEST_TO_HAND | RCUYRT_MILL_REST);
						}
					}
					else{
						break;
					}
					amount--;
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_furious_assault(int player, int card, event_t event){
/* CARD_ID_FURIOUS_ASSAULT	4475
Furious Assault	|2|R
Enchantment
Whenever you cast a creature spell, ~ deals 1 damage to target player. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);

		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			target_definition_t td1;
			default_target_definition(player, card, &td1, TYPE_CREATURE);
			td1.zone = TARGET_ZONE_PLAYERS;
			td1.allow_cancel = 0;

			card_instance_t *instance= get_card_instance(player, card);

			if( can_target(&td1) && new_pick_target(&td1, "TARGET_PLAYER", 0, 0) ){
				damage_player(instance->targets[0].player, 1, player, card);
			}
			instance->number_of_targets = 0;
		}
	}

	return global_enchantment(player, card, event);
}

int card_game_preserve(int player, int card, event_t event){
/* CARD_ID_GAME_PRESERVE	4476
Game Preserve	|2|G
Enchantment
At the beginning of your upkeep, each player reveals the top card of his or her library.
If all cards revealed this way are creature cards, put those cards onto the battlefield under their owners' control. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int id1 = deck_ptr[player][0];
		if( id1 != -1 ){
			reveal_card_iid(player, card, id1);
		}
		int id2 = deck_ptr[1-player][0];
		if( id1 != -1 ){
			reveal_card_iid(player, card, id2);
		}
		if( is_what(-1, id1, TYPE_CREATURE) && is_what(-1, id2, TYPE_CREATURE) ){
			put_into_play_a_card_from_deck(player, player, 0);
			put_into_play_a_card_from_deck(1-player, 1-player, 0);
		}
	}

	return global_enchantment(player, card, event);
}

int card_generals_regalia(int player, int card, event_t event){
/*
General's Regalia |3
Artifact
{3}: The next time a source of your choice would deal damage to you this turn, that damage is dealt to target creature you control instead.
*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ANY);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.allowed_controller = player;
	td2.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_target(&td) ){
			return generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, NULL);
		}
	}
	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(3)) ){
			if( pick_target(&td, "TARGET_DAMAGE") ){
				new_pick_target(&td2, "Select target creature you control.", 1, 1 | GS_LITERAL_PROMPT);
				if( spell_fizzled != 1 ){
					int mod = -24;
					if( check_for_special_ability(instance->targets[1].player, instance->targets[1].card, SP_KEYWORD_INDESTRUCTIBLE) ){
						mod = 128;
					}
					if( get_id(instance->targets[1].player, instance->targets[1].card) == CARD_ID_CHO_MANNO_REVOLUTIONARY ){
						mod = 128;
					}
					int actual_toughness = get_toughness(instance->targets[1].player, instance->targets[1].card);
					actual_toughness -= get_card_instance(instance->targets[1].player, instance->targets[1].card)->damage_on_card;
					if( actual_toughness >  get_card_instance(instance->targets[0].player, instance->targets[0].card)->info_slot ){
						ai_modifier = 48;
					}
					ai_modifier += mod;
				}
			}
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && validate_target(player, card, &td2, 1) ){
			card_instance_t *dmg = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			dmg->damage_target_card = instance->targets[1].card;
			if( get_id(instance->targets[1].player, instance->targets[1].card) == CARD_ID_CHO_MANNO_REVOLUTIONARY ){
				dmg->info_slot = 0;
			}
		}
	}
	return 0;
}

int card_gush(int player, int card, event_t event){
/* CARD_ID_GUSH	4482
Gush	|4|U
Instant
You may return two |H1Islands you control to their owner's hand rather than pay ~'s mana cost.
Draw two cards. */
	// Used externally

	if( event == EVENT_MODIFY_COST ){
		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.required_subtype = SUBTYPE_ISLAND;
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		if( target_available(player, card, &td) > 1 ){
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

	if(event == EVENT_CAST_SPELL  && affect_me(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_LAND);
			td.required_subtype = SUBTYPE_ISLAND;
			td.allowed_controller = player;
			td.preferred_controller = player;
			td.illegal_abilities = 0;

			int choice = 0;
			if( has_mana_to_cast_iid(player, event, instance->internal_card_id) ){
				choice = do_dialog(player, player, card, -1, -1, " Return 2 Islands\n Play normally\n Cancel", 0);
			}

			if( choice == 0 ){
				int trgs = 0;
				while( trgs < 2 ){
						if( new_pick_target(&td, "Select an Island to bounce.", trgs, GS_LITERAL_PROMPT) ){
							state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
							trgs++;
						}
						else{
							break;
						}
				}
				int i;
				for(i=0; i<trgs; i++){
					if( trgs == 2 ){
						bounce_permanent(instance->targets[i].player, instance->targets[i].card);
					}
					else{
						state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
					}
				}
				instance->number_of_targets = 0;
				if( trgs < 2 ){
					spell_fizzled = 1;
					return 0;
				}
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_hammer_mage(int player, int card, event_t event){
/* CARD_ID_HAMMER_MAGE	4483
Hammer Mage	|1|R
Creature - Human Spellshaper 1/1
|X|R, |T, Discard a card: Destroy all artifacts with converted mana cost X or less. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		test.cmc = instance->info_slot + 1;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		APNAP(p, { new_manipulate_all(instance->parent_controller, instance->parent_card, p, &test, KILL_DESTROY); };);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD, MANACOST_XR(-1, 1), 0, NULL, NULL);
}

int card_haunted_crossroads(int player, int card, event_t event){
/* CARD_ID_HAUNTED_CROSSROADS	4484
Haunted Crossroads	|2|B
Enchantment
|B: Put target creature card from your graveyard on top of your library. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1) < 3 ? 48 : -48);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_graveyard_to_deck(player, selected, 1);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL,
															0, &this_test, GS_GRAVE_RECYCLER);
}

int card_heart_of_ramos(int player, int card, event_t event ){
/* CARD_ID_HEART_OF_RAMOS	4485
Heart of Ramos	|3
Artifact
|T: Add |R to your mana pool.
Sacrifice ~: Add |R to your mana pool. */

	return part_of_ramos(player, card, event, COLOR_RED);
}

int card_hickory_woodlot(int player, int card, event_t event ){
	/* Hickory Woodlot	""
	 * Land
	 * ~ enters the battlefield tapped with two depletion counters on it.
	 * |T, Remove a depletion counter from ~: Add |G|G to your mana pool. If there are no depletion counters on ~, sacrifice it. */
	return depletion_land(player, card, event, COLOR_GREEN);
}

int card_high_market(int player, int card, event_t event){

	/* High Market	""
	 * Land
	 * |T: Add |1 to your mana pool.
	 * |T, Sacrifice a creature: You gain 1 life. */

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAC_AND_GAIN
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL) )
		{
			choice = CHOICE_SAC_AND_GAIN;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) && choice == CHOICE_SAC_AND_GAIN ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 5,
							"Sac & gain 1 life", 1, life[player] < 6 ? 10 : 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		instance->info_slot = choice;

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_SAC_AND_GAIN ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAC_AND_GAIN ){
			gain_life(player, 1);
		}
	}

	return 0;
}

int card_hired_giant(int player, int card, event_t event ){
/* CARD_ID_HIRED_GIANT	4492
Hired Giant	|3|R
Creature - Giant 4/4
When ~ enters the battlefield, each other player may search his or her library for a land card and put that card onto the battlefield.
Then each player who searched his or her library this way shuffles it. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( do_dialog(1-player, player, card, -1, -1, " Tutor a land\n Pass", (1-player == AI ? 0 : 1)) == 0 ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, "Select a land card.");
			new_global_tutor(1-player, 1-player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, 1, &this_test);
		}
	}

	return 0;
}

int card_honor_the_fallen(int player, int card, event_t event){
/* CARD_ID_HONOR_THE_FALLEN	4493
Honor the Fallen	|1|W
Instant
Exile all creature cards from all graveyards. You gain 1 life for each card exiled this way. */

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		int amount = 0;
		for(i=0; i<2; i++){
			const int *grave = get_grave(i);
			int count = count_graveyard(i)-1;
			while( count > -1 ){
					if( is_what(-1, grave[count], TYPE_CREATURE) ){
						rfg_card_from_grave(i, count);
						amount++;
					}
					count--;
			}
		}
		gain_life(player, amount);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static int horn_of_plenty_legacy(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player ){
		if( trigger_cause_controller == player && trigger_cause == card ){
			card_instance_t *instance = get_card_instance( player, card );
			if( instance->targets[0].card > 0 ){
				instance->targets[0].card--;
			}
			else{
				if( eot_trigger(player, card, event) ){
					draw_cards(instance->targets[0].player, 1);
					kill_card(player, card, KILL_EXILE);
				}
			}
		}
	}

	return 0;
}

int card_horn_of_plenty(int player, int card, event_t event){
/*
Horn of Plenty |6
Artifact
Whenever a player casts a spell, he or she may pay {1}. If that player does, he or she draws a card at the beginning of the next end step.
*/
	if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		int choice = 2;
		if( has_mana( trigger_cause_controller, COLOR_ANY, 1) ){
			choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
							"Activate Horn of Plenty", 1, 14-(hand_count[player]*2),
							"Decline", 1, 5);
		}
		if( choice == 1 && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, trigger_cause_controller, COLOR_ANY, 1) ){
			int legacy = create_legacy_effect(player, card, &horn_of_plenty_legacy);
			card_instance_t *instance = get_card_instance( player, legacy );
			instance->targets[0].player = trigger_cause_controller;
			if( current_phase == PHASE_DISCARD ){
				instance->targets[0].card = 1;
			}
		}
	}
	return 0;
}

int card_horn_of_ramos(int player, int card, event_t event ){
/* CARD_ID_HORN_OF_RAMOS	4496
Horn of Ramos	|3
Artifact
|T: Add |G to your mana pool.
Sacrifice ~: Add |G to your mana pool. */

	return part_of_ramos(player, card, event, COLOR_GREEN);
}

int card_howling_wolf(int player, int card, event_t event ){
/* Howling Wolf	|2|G|G
Creature - Wolf 2/2
When ~ enters the battlefield, you may search your library for up to three cards named ~, reveal them, and put them into your hand.
If you do, shuffle your library.*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		char buffer[100];
		card_ptr_t* c = cards_ptr[ get_id(player, card) ];
		scnprintf(buffer, 100, "Select a card named %s.", c->name);;

		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, buffer);
		test.id = get_id(player, card);
		test.qty = MIN(count_deck(player), 3);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &test);
	}

	return 0;
}

int card_hunted_wumpus(int player, int card, event_t event ){
	/*
	  Hunted Wumpus |3|G
	  Creature - Beast 6/6
	  When Hunted Wumpus enters the battlefield, each other player may put a creature card from his or her hand onto the battlefield.
	*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card to put into play.");
		this_test.zone = TARGET_ZONE_HAND;
		if( check_battlefield_for_special_card(player, card, 1-player, 0, &this_test) ){
			new_global_tutor(1-player, 1-player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}

	return 0;
}

int card_intimidation(int player, int card, event_t event)
{
/* CARD_ID_INTIMIDATION	4502
Intimidation	|2|B|B|B
Enchantment
Creatures you control have fear. (They can't be blocked except by artifact creatures and/or black creatures.) */
	// Used externally
	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if (event == EVENT_ABILITIES && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
		{
			fear(affected_card_controller, affected_card, event);
		}
	}

	return global_enchantment(player, card, event);
}

int card_invigorate(int player, int card, event_t event){
	/*
	  Invigorate |2|G
	  Instant
	  If you control a Forest, rather than pay Invigorate's mana cost, you may have an opponent gain 3 life.
	  Target creature gets +4/+4 until end of turn.
	*/

	if( event == EVENT_MODIFY_COST ){
		card_instance_t *instance = get_card_instance( player, card );
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_FOREST) ){
			instance->info_slot = 1;
			null_casting_cost(player, card);
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CHECK_PUMP ){
		if( has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id) ||
			check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_FOREST)
		  ){
			pumpable_power[player] += 4;
			pumpable_toughness[player] += 4;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_iid(player, get_card_instance(player, card)->internal_card_id, event) ){
				choice = do_dialog(player, player, card, -1, -1, " Make opponent gain life\n Play normally\n Cancel", 0);
			}
			if( choice == 0 ){
				gain_life(1-player, 3);
				td.allow_cancel = 0;
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		if( spell_fizzled != 1 ){
			pick_target(&td, "TARGET_CREATURE");
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 4, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_ivory_mask(int player, int card, event_t event){
/* CARD_ID_IVORY_MASK	1064
Ivory Mask	|2|W|W
Enchantment
You have shroud. (You can't be the target of spells or abilities.) */

	give_shroud_to_player(player, card, event);
	return global_enchantment(player, card, event);
}

int card_karns_touch(int player, int card, event_t event){
/* CARD_ID_KARNS_TOUCH	4509
Karn's Touch	|U|U
Instant
Target noncreature artifact becomes an artifact creature with power and toughness each equal to its converted mana cost until end of turn.
(It retains its abilities.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.illegal_type = TYPE_CREATURE;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			artifact_animation(player, card, instance->targets[0].player, instance->targets[0].card, 1, -1, -1, 0, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select targer noncreature artifact", 1, NULL);
}

int card_kris_mage(int player, int card, event_t event){
	/* Kris Mage	|R	0x2004f94
	 * Creature - Human Spellshaper 1/1
	 * |R, |T, Discard a card: ~ deals 1 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DISCARD, MANACOST_R(1), 0,
									&td, "TARGET_ANY");
}

int card_kyren_negotiations(int player, int card, event_t event){
	/* Kyren Negotiations	|2|R|R	0x2004f99
	 * Enchantment
	 * Tap an untapped creature you control: ~ deals 1 damage to target player or planeswalker. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller = player;
	td1.preferred_controller = player;
	td1.illegal_abilities = 0;
	td1.illegal_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_target(&td1) ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			if( new_pick_target(&td1, "Select an untapped creature you control.", 0, 1 | GS_LITERAL_PROMPT) ){
				instance->number_of_targets = 0;
				int to_tap = instance->targets[0].card;
				if( pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER") ){
					tap_card(player, to_tap);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}

	return 0;
}

int card_kyren_toy(int player, int card, event_t event){
/* CARD_ID_KYREN_TOY	4516
Kyren Toy	|3
Artifact
|1, |T: Put a charge counter on ~.
|T, Remove X charge counters from ~: Add an amount of |C to your mana pool equal to X plus one. */

	return mana_battery_generic(player, card, event, COLOR_COLORLESS, 1);
}

int card_land_grant(int player, int card, event_t event){
/* CARD_ID_LAND_GRANT	4517
Land Grant	|1|G
Sorcery
If you have no land cards in hand, you may reveal your hand rather than pay ~'s mana cost.
Search your library for |Ha Forest card, reveal that card, and put it into your hand. Then shuffle your library. */

	if( event == EVENT_MODIFY_COST ){
		card_instance_t* instance = get_card_instance(player, card);
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.zone = TARGET_ZONE_HAND;
		if( ! check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
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

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_iid(player, event, instance->internal_card_id) ){
				choice = do_dialog(player, player, card, -1, -1, " Reveal your hand\n Play normally\n Cancel", 0);
			}
			if( choice == 0 ){
				reveal_target_player_hand(player);
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, get_hacked_land_text(player, card, "Select %a card.", SUBTYPE_FOREST));
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_larceny(int player, int card, event_t event){
/*
Larceny |3|B|B
Enchantment
Whenever a creature you control deals combat damage to a player, that player discards a card.
*/
	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);
	check_damage_test(player, -1, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE,
						player, card, 0, &test);

	if( resolve_damage_trigger(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE,
								player, card) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		int num[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		new_multidiscard(0, num[0], 0, player);
		new_multidiscard(1, num[1], 0, player);
	}

	return global_enchantment(player, card, event);
}

static int liability_effect(int player, int card, event_t event ){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_LIABILITY ){
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
						SET_BYTE2(instance->targets[11].player) = 1;
					}
				}
				count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, TYPE_PERMANENT, NULL, GFPC_TRACK_OWNER, 0);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			lose_life(0, BYTE0(result));
			lose_life(1, BYTE1(result));
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

int card_liability(int player, int card, event_t event ){
/*
Liability |1|B|B
Enchantment
Whenever a nontoken permanent is put into a player's graveyard from the battlefield, that player loses 1 life.
*/
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)liability_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &liability_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( ! is_token(affected_card_controller, affected_card) ){
			count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, TYPE_PERMANENT, NULL, GFPC_TRACK_OWNER, 0);
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			lose_life(0, BYTE0(result));
			lose_life(1, BYTE1(result));
		}
	}

	return global_enchantment(player, card, event);
}

int card_lumbering_satyr(int player, int card, event_t event ){
/* CARD_ID_LUMBERING_SATYR	4524
Lumbering Satyr	|2|G|G
Creature - Satyr Beast 5/4
All creatures have |H2forestwalk. (They can't be blocked as long as defending player controls |Ha Forest.) */

	return boost_creature_type(player, card, event, -1, 0, 0, get_hacked_walk(player, card, KEYWORD_FORESTWALK), BCT_INCLUDE_SELF);
}

int card_lunge(int player, int card, event_t event){
	/* Lunge	|2|R	0x2004fb2
	 * Instant
	 * ~ deals 2 damage to target creature and 2 damage to target player or planeswalker. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST && can_target(&td1) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		instance->targets[1].player = instance->targets[0].player;
		instance->targets[1].card = instance->targets[0].card;
		if( spell_fizzled != 1 ){
			pick_target(&td1, "TARGET_PLAYER_OR_PLANESWALKER");
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 1) ){
			damage_creature(instance->targets[1].player, instance->targets[1].card, 2, player, card);
		}
		if( validate_target(player, card, &td1, 0) ){
			damage_target0(player, card, 2);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_magistrates_scepter(int player, int card, event_t event){
/* Magistrate's Scepter	|3
 * Artifact
 * |4, |T: Put a charge counter on ~.
 * |T, Remove three charge counters from ~: Take an extra turn after this one. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_COUNTER = 1,
		CHOICE_TIME_WALK
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, GVC_COUNTERS(COUNTER_CHARGE, 3), NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( count_counters(player, card, COUNTER_CHARGE) < 3 ){
			ai_modifier += (current_turn == 1-player ? 48 : 24);
		}
		else{
			ai_modifier += (current_turn == player ? 128 : -128);
		}
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, GVC_COUNTERS(COUNTER_CHARGE, 3),
												NULL, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Add a counter", 	abils[1], 5,
							"Time Walk", 		abils[2], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		else{
			choice = 1;
		}
		if( choice == CHOICE_COUNTER ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL);
		}
		if( choice == CHOICE_TIME_WALK ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, GVC_COUNTERS(COUNTER_CHARGE, 3), NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_COUNTER ){
			add_counter(player, card, COUNTER_CHARGE);
		}
		if( instance->info_slot == CHOICE_TIME_WALK ){
			time_walk_effect(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_megatherium(int player, int card, event_t event ){
/* CARD_ID_MEGATHERIUM	4529
Megatherium	|2|G
Creature - Beast 4/4
Trample
When ~ enters the battlefield, sacrifice it unless you pay |1 for each card in your hand. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! has_mana(player, COLOR_ANY, hand_count[player]) ){
			ai_modifier-=1000;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int kill_me = 1;
		if( has_mana(player, COLOR_COLORLESS, hand_count[player]) ){
			charge_mana(player, COLOR_COLORLESS, hand_count[player]);
			if( spell_fizzled != 1 ){
				kill_me = 0;
			}
		}
		if( kill_me == 1 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_mercadian_atlas(int player, int card, event_t event ){
/* CARD_ID_MERCADIAN_ATLAS	4531
Mercadian Atlas	|5
Artifact
At the beginning of your end step, if you didn't play a land this turn, you may draw a card. */

	if( current_turn == player && ! lands_played && ! is_humiliated(player, card) &&
		eot_trigger_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player)) )
	{
		draw_cards(player, 1);
	}

	return 0;
}

int card_mercadian_lift(int player, int card, event_t event){
/* Mercadian Lift	|2
 * Artifact
 * |1, |T: Put a winch counter on ~.
 * |T, Remove X winch counters from ~: You may put a creature card with converted mana cost X from your hand onto the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_COUNTER = 1,
		CHOICE_PUT_CREATURE_ON_BF
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			return 2;
	    }
	}
	if( event == EVENT_ACTIVATE ){
		instance->targets[0].player = 0;
		int amount = count_counters(player, card, COUNTER_WINCH);
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.cmc = amount;
		test.zone = TARGET_ZONE_HAND;
		int found = check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test);
		if( found ){
			ai_modifier += (current_turn == player ? (amount * 8) : (amount * 12));
		}
		else{
			ai_modifier += (current_turn == player ? 24 : 48);
		}
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Add a counter", 			abils[1], 5,
							"Put a creature on BF", 	abils[2], found ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_COUNTER ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL);
		}
		if( choice == CHOICE_PUT_CREATURE_ON_BF ){
			if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
				int max_value = 0;
				int to_remove = 0;
				int c;
				for(c=0; c<active_cards_count[player]; c++){
					if( in_hand(player, c) ){
						int this_cmc = get_cmc(player, c);
						if( this_cmc <= amount ){
							int this_value = get_base_value(player, c);
							if( this_value > max_value){
								to_remove = this_cmc;
								max_value = this_value;
							}
						}
					}
				}
				if( ! IS_AI(player) ){
					to_remove = choose_a_number(player, "How many counters you'll remove?", to_remove);
					if( to_remove < 0 || to_remove > amount ){
						spell_fizzled = 1;
						return 0;
					}
				}
				remove_counters(player, card, COUNTER_WINCH, to_remove);
				add_state(player, card, STATE_TAPPED);
				instance->targets[0].player = to_remove;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_COUNTER ){
			add_counter(player, card, COUNTER_CHARGE);
		}
		if( instance->info_slot == CHOICE_PUT_CREATURE_ON_BF ){
			char msg[100];
			scnprintf(msg, 100, "Select creature card with CMC %d.", instance->targets[0].player);
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, msg);
			test.cmc = instance->targets[0].player;
			new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
		}
	}

	return 0;
}

int card_midnight_ritual(int player, int card, event_t event){
/* CARD_ID_MIDNIGHT_RITUAL	4534
Midnight Ritual	|X|2|B
Sorcery
Exile X target creature cards from your graveyard. For each creature card exiled this way, create a 2/2 |Sblack Zombie creature token. */

	if (event == EVENT_CAN_CAST){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		card_instance_t *instance = get_card_instance(player, card);
		int max_cards = count_graveyard_by_type(player, TYPE_CREATURE);
		int generic_mana_available = has_mana(player, COLOR_ANY, 1);	// A useful trick from card_fireball().
		max_cards = MIN(max_cards, generic_mana_available);
		max_cards = MIN(max_cards, 19);

		if (max_cards <= 0 || graveyard_has_shroud(player)){
			ai_modifier -= 48;
			instance->info_slot = 0;
			return 0;
		}

		char buf[100];
		if (ai_is_speculating == 1){
			*buf = 0;
		} else if (max_cards == 1){
			strcpy(buf, "Select a creature card.");
		} else {
			sprintf(buf, "Select up to %d creature cards.", max_cards);
		}
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, buf);
		instance->info_slot = select_multiple_cards_from_graveyard(player, player, 0, AI_MIN_VALUE, &this_test, max_cards, &instance->targets[0]);

		if( instance->info_slot > 0 ){
			int diff = true_get_updated_casting_cost(player, card, -1, event, -1);
			int amount = instance->info_slot += diff;
			charge_mana(player, COLOR_COLORLESS, amount);
		}
	}

	if (event == EVENT_RESOLVE_SPELL){
		card_instance_t *instance = get_card_instance(player, card);
		int i, num_validated = 0;
		for (i = 0; i < instance->info_slot; ++i){
			int selected = validate_target_from_grave(player, card, player, i);
			if (selected != -1){
				rfg_card_from_grave(player, selected);
				++num_validated;
			}
		}
		if (num_validated > 0){
			generate_tokens_by_id(player, card, CARD_ID_ZOMBIE, num_validated);
		} else if (instance->info_slot > 0) {
			spell_fizzled = 1;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_misstep(int player, int card, event_t event){
/* CARD_ID_MISSTEP	4537
Misstep	|1|U
Sorcery
Creatures target player controls don't untap during that player's next untap step. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int c;
		for(c = 0; c < active_cards_count[1-player]; c++){
			if( in_play(1-player, c) && is_what(1-player, c, TYPE_CREATURE) && is_tapped(1-player, c) ){
				ai_modifier += (2*get_power(1-player, c));
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int p = instance->targets[0].player;
			int count = active_cards_count[p]-1;
			while( count > -1 ){
					if( in_play(p, count) && is_what(p, count, TYPE_CREATURE) ){
						does_not_untap_effect(player, card, p, count, 0, 1);
					}
					count--;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_moment_of_silence(int player, int card, event_t event)
{
/* CARD_ID_MOMENT_OF_SILENCE	4539
Moment of Silence	|W
Instant
Target player skips his or her next combat phase this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td_player;
	default_target_definition(player, card, &td_player, 0);
	td_player.zone = TARGET_ZONE_PLAYERS;

	if (event == EVENT_CAN_CAST)
		return can_target(&td_player);

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
		pick_target(&td_player, "TARGET_PLAYER");


	if (event == EVENT_RESOLVE_SPELL)
	{
		card_instance_t* instance = get_card_instance(player, card);
		if (valid_target(&td_player))
			target_player_skips_his_next_attack_step(player, card, instance->targets[0].player, 1);

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_monkey_cage(int player, int card, event_t event){
/* Monkey Cage	|5
 * Artifact
 * When a creature enters the battlefield, sacrifice ~ and put X 2/2 |Sgreen Ape creature tokens onto the battlefield, where X
 *		is that creature's converted mana cost. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && ! is_humiliated(player, card) ){
		if( new_specific_cip_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, 0, player, card) ){		
			int cmc = get_cmc(trigger_cause_controller, trigger_cause);
			generate_tokens_by_id(player, card, CARD_ID_MONKEY, cmc);
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

static int moonlit_wake_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_MOONLIT_WAKE ){
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
						//Will trigger also if this is not a creature when it goes into the graveyard;
						if( ! is_what(p, c, TYPE_CREATURE) ){
							count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
						}
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
						if( instance->targets[11].player < 0 ){
							instance->targets[11].player = 0;
						}
						SET_BYTE1(instance->targets[11].player) = 1;
					}
				}
				count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			gain_life(player, amount);
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

int card_monlit_wake(int player, int card, event_t event){
/*
Moonlit Wake |2|W
Enchantment
Whenever a creature dies, you gain 1 life.
*/
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)moonlit_wake_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &moonlit_wake_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, 0);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			gain_life(player, amount);
		}
	}

	return global_enchantment(player, card, event);
}

int card_muzzle(int player, int card, event_t event){
/* CARD_ID_MUZZLE	4542
Muzzle	|1|W
Enchantment - Aura
Enchant creature
Prevent all damage that would be dealt by enchanted creature. */

	if( in_play(player, card) && get_card_instance( player, card )->damage_target_player != -1 && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;
		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card && damage->info_slot > 0){
				if( damage->damage_source_player == t_player && damage->damage_source_card == t_card ){
					prevent_damage(affected_card_controller, affected_card, -1);
				}
			}
		}
	}

	return disabling_aura(player, card, event);
}

int card_natural_affinity(int player, int card, event_t event){
/* CARD_ID_NATURAL_AFFINITY	1095
Natural Affinity	|2|G
Instant
All lands become 2/2 creatures until end of turn. They're still lands. */

	if(event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test2;
		default_test_definition(&this_test2, TYPE_LAND);
		land_animation2(player, card, 2, -1, 1, 2, 2, 0, 0, 0, &this_test2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_nether_spirit(int player, int card, event_t event){
/* CARD_ID_NETHER_SPIRIT	4543
Nether Spirit	|1|B|B
Creature - Spirit 2/2
At the beginning of your upkeep, if ~ is the only creature card in your graveyard, you may return ~ to the battlefield. */

	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) == 1 ){
			int choice = 0;
			if( ! duh_mode(player) ){
				choice = do_dialog(player, player, card, -1, -1," Return Nether Spirit\n Pass", 0);
			}
			if( choice == 0 ){
				put_into_play(player, card);
				return -1;
			}
		}
		return -2;
	}

	return 0;
}

int card_noble_purpose(int player, int card, event_t event){
/*
Noble Purpose |3|W|W
Enchantment
Whenever a creature you control deals combat damage, you gain that much life.
*/
    test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);
	check_damage_test(player, -1, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_REPORT_DAMAGE_DEALT, player, card, 0, &test);

	if( resolve_damage_trigger(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_REPORT_DAMAGE_DEALT, player, card) ){
		int amount = get_card_instance(player, card)->targets[16].player;
		get_card_instance(player, card)->targets[16].player = 0;
		gain_life(player, amount);
	}

	return global_enchantment(player, card, event);
}

int card_notorious_assassin(int player, int card, event_t event){
/* CARD_ID_NOTORIOUS_ASSASSIN	4545
Notorious Assassin	|3|B
Creature - Human Spellshaper Assassin 2/2
|2|B, |T, Discard a card: Destroy target non|Sblack creature. It can't be regenerated. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DISCARD|GAA_LITERAL_PROMPT, MANACOST_XB(2, 1), 0,
									&td, get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK));
}

int card_orims_cure(int player, int card, event_t event){
	/* Orim's Cure	|1|W	0x20050b1
	 * Instant
	 * If you control |Ha Plains, you may tap an untapped creature you control rather than pay this spell's mana cost.
	 * Prevent the next 4 damage that would be dealt to any target this turn. */
	if( event == EVENT_MODIFY_COST ){
		card_instance_t *instance = get_card_instance( player, card );

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.allowed_controller = player;
		td2.preferred_controller = player;
		td2.illegal_abilities = 0;
		td2.illegal_state = TARGET_STATE_TAPPED;

		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_PLAINS) && can_target(&td2) ){
			instance->info_slot = 1;
			null_casting_cost(player, card);
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, "TARGET_DAMAGE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_iid(player, get_card_instance(player, card)->internal_card_id, event) ){
				choice = do_dialog(player, player, card, -1, -1, " Tap a creature you control\n Play normally\n Cancel", 0);
			}
			if( choice == 0 ){
				target_definition_t td2;
				default_target_definition(player, card, &td2, TYPE_CREATURE);
				td2.allowed_controller = player;
				td2.preferred_controller = player;
				td2.illegal_abilities = 0;
				td2.illegal_state = TARGET_STATE_TAPPED;
				if( new_pick_target(&td2, "Select a creature you control to tap.", 0, 1 | GS_LITERAL_PROMPT) ){
					action_on_target(player, card, 0, ACT_TAP);
					td.allow_cancel = 0;
					instance->number_of_targets = 0;
				}
				else{
					spell_fizzled = 1;
					return 0;
				}
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		if( spell_fizzled != 1 ){
			pick_target(&td, "TARGET_DAMAGE");
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			if( target->info_slot <= 4 ){
				target->info_slot = 0;
			}
			else{
				target->info_slot-=4;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	if( event == EVENT_CAN_CHANGE_TARGET || event == EVENT_CHANGE_TARGET ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, "TARGET_DAMAGE", 1, NULL);
	}

	return 0;
}

int card_overtaker(int player, int card, event_t event){
/* CARD_ID_OVERTAKER	4547
Overtaker	|1|U
Creature - Merfolk Spellshaper 1/1
|3|U, |T, Discard a card: Untap target creature and gain control of it until end of turn. That creature gains haste until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			effect_act_of_treason(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DISCARD, MANACOST_XU(3, 1), 0,
									&td, "TARGET_CREATURE");
}

int card_pangosaur(int player, int card, event_t event){
/* CARD_ID_PANGOSAUR	4549
Pangosaur	|2|G|G
Creature - Lizard 6/6
Whenever a player plays a land, return ~ to its owner's hand. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( whenever_a_player_plays_a_land(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY) ){
			bounce_permanent(player, card);
		}
	}

	return 0;
}

int card_peat_bog(int player, int card, event_t event){
	/* Peat Bog	""
	 * Land
	 * ~ enters the battlefield tapped with two depletion counters on it.
	 * |T, Remove a depletion counter from ~: Add |B|B to your mana pool. If there are no depletion counters on ~, sacrifice it. */
	return depletion_land(player, card, event, COLOR_BLACK);
}

int card_power_matrix(int player, int card, event_t event){
/* CARD_ID_POWER_MATRIX	4553
Power Matrix	|4
Artifact
|T: Target creature gets +1/+1 and gains flying, first strike, and trample until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	return vanilla_creature_pumper(player, card, event, MANACOST0, GAA_UNTAPPED, 1, 1, KEYWORD_FIRST_STRIKE | KEYWORD_TRAMPLE |
									KEYWORD_FLYING, 0, &td);
}

static int puffer_extract_effect(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		int boost = instance->targets[1].player;

		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) && boost > 0 ){
			event_result += boost;
		}
	}

	if( event == EVENT_CLEANUP ){
		card_instance_t *instance = get_card_instance(player, card);
		kill_card(instance->damage_target_player, instance->damage_target_card, KILL_DESTROY);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_puffer_extract(int player, int card, event_t event){
/* CARD_ID_PUFFER_EXTRACT	4555
Puffer Extract	|5
Artifact
|X, |T: Target creature you control gets +X/+X until end of turn. Destroy it at the beginning of the next end step. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int legacy = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &puffer_extract_effect,
														instance->targets[0].player, instance->targets[0].card);
			card_instance_t *leg = get_card_instance(instance->parent_controller, legacy);
			leg->targets[1].player = instance->info_slot;
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(-1), 0, &td, "TARGET_CREATURE");
}

int card_pulverize(int player, int card, event_t event){
	/*
	  Pulverize |4|R|R
	  Sorcery
	  You may sacrifice two Mountains rather than pay Pulverize's mana cost.
	  Destroy all artifacts.
	*/

	if( event == EVENT_MODIFY_COST ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a Mountain to sacrifice.");
		test.subtype = SUBTYPE_MOUNTAIN;
		test.qty = 2;

		card_instance_t *instance = get_card_instance( player, card );

		if( new_can_sacrifice_as_cost(player, card, &test) ){
			instance->info_slot = 1;
			null_casting_cost(player, card);
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_iid(player, get_card_instance(player, card)->internal_card_id, event) ){
				choice = do_dialog(player, player, card, -1, -1, " Sacrifice 2 Mountains\n Play normally\n Cancel", 0);
			}
			if( choice == 0 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_PERMANENT, "Select a Mountain to sacrifice.");
				test.subtype = SUBTYPE_MOUNTAIN;
				int sacced[2];
				int sc = 0;
				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_RETURN_CHOICE | SAC_AS_COST, &test);
				if( sac ){
					sacced[sc] = sac;
					sc++;
					sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_RETURN_CHOICE | SAC_AS_COST, &test);
					if( sac ){
						sacced[sc] = sac;
						int i;
						for(i=0; i<2; i++){
							state_untargettable(BYTE2(sacced[i]), BYTE3(sacced[i]), 0);
							kill_card(BYTE2(sacced[i]), BYTE3(sacced[i]), KILL_SACRIFICE);
						}
					}
					else{
						state_untargettable(BYTE2(sacced[0]), BYTE3(sacced[0]), 0);
						spell_fizzled = 1;
						return 0;
					}
				}
				else{
					spell_fizzled = 1;
					return 0;
				}
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT);
		APNAP(p, { new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_puppets_verdict(int player, int card, event_t event)
{
	// Flip a coin. If you win the flip, destroy all creatures with power 2 or less.
	// If you lose the flip, destroy all creatures with power 3 or greater.

	if (event == EVENT_RESOLVE_SPELL)
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "");

		if (flip_a_coin(player, card))
		{
			test.power = 3;
			test.power_flag = F5_POWER_LESSER_THAN_VALUE;
		}
		else
		{
			test.power = 2;
			test.power_flag = F5_POWER_GREATER_THAN_VALUE;
		}
		APNAP(p, { new_manipulate_all(player, card, p, &test, KILL_DESTROY); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_ramosian_sergeant(int player, int card, event_t event){
/* CARD_ID_RAMOSIAN_SERGEANT	4564
Ramosian Sergeant	|W
Creature - Human Rebel 1/1
|3, |T: Search your library for a Rebel permanent card with converted mana cost 2 or less and put it onto the battlefield. Then shuffle your library.*/

  return generic_subtype_searcher(player, card, event, SUBTYPE_REBEL, 3, 0);
}

int card_ramosian_captain(int player, int card, event_t event){
/* CARD_ID_RAMOSIAN_CAPTAIN	4560
Ramosian Captain	|1|W|W
Creature - Human Rebel 2/2
First strike
|5, |T: Search your library for a Rebel permanent card with converted mana cost 4 or less and put it onto the battlefield. Then shuffle your library.*/

  return generic_subtype_searcher(player, card, event, SUBTYPE_REBEL, 5, 0);
}

int card_ramosian_commander(int player, int card, event_t event ){
/* CARD_ID_RAMOSIAN_COMMANDER	4561
Ramosian Commander	|2|W|W
Creature - Human Rebel 2/4
|6, |T: Search your library for a Rebel permanent card with converted mana cost 5 or less and put it onto the battlefield. Then shuffle your library.*/

	return generic_subtype_searcher(player, card, event, SUBTYPE_REBEL, 6, 0);
}

int card_ramosian_liutenant(int player, int card, event_t event ){
/* CARD_ID_RAMOSIAN_LIEUTENANT	4562
Ramosian Lieutenant	|1|W
Creature - Human Rebel 1/2
|4, |T: Search your library for a Rebel permanent card with converted mana cost 3 or less and put it onto the battlefield. Then shuffle your library.*/

	return generic_subtype_searcher(player, card, event, SUBTYPE_REBEL, 4, 0);
}

int card_ramosian_sky_marshall(int player, int card, event_t event ){
/* CARD_ID_RAMOSIAN_SKY_MARSHAL	4565
Ramosian Sky Marshal	|3|W|W
Creature - Human Rebel 3/3
Flying
|7, |T: Search your library for a Rebel permanent card with converted mana cost 6 or less and put it onto the battlefield. Then shuffle your library.*/

	return generic_subtype_searcher(player, card, event, SUBTYPE_REBEL, 7, 0);
}

int card_reverent_mantra(int player, int card, event_t event){
/* CARD_ID_REVERENT_MANTRA	4571
Reverent Mantra	|3|W
Instant
You may exile a |Swhite card from your hand rather than pay ~'s mana cost.
Choose a color. All creatures gain protection from the chosen color until end of turn. */

	al_pitchspell(player, card, event, COLOR_WHITE, 0);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! casting_al_pitchspell(player, card, event, COLOR_WHITE, 0) ){
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int keyword = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
		APNAP(p, { pump_subtype_until_eot(player, card, p, -1, 0, 0, keyword, 0); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_remote_farm(int player, int card, event_t event){
	/* Remote Farm	""
	 * Land
	 * ~ enters the battlefield tapped with two depletion counters on it.
	 * |T, Remove a depletion counter from ~: Add |W|W to your mana pool. If there are no depletion counters on ~, sacrifice it. */
	return depletion_land(player, card, event, COLOR_WHITE);
}

int card_renounce(int player, int card, event_t event)
{
  /* Renounce	|1|W
   * Instant
   * Sacrifice any number of permanents. You gain 2 life for each permanent sacrificed this way. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent to sacrifice");
		if( life[player] < 6 && new_can_sacrifice(player, card, player, &test) ){
			ai_modifier += 48;
		}
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent to sacrifice");
		if (IS_AI(player))
		{
			int c, max_can_sac = 0;
			for (c = 0; c < active_cards_count[player]; ++c)
				if (in_play(player, c) && is_what(player, c, TYPE_PERMANENT))
					++max_can_sac;

			test.qty = recorded_rand(player, max_can_sac + 1);
		}
		else{
			test.qty = 151;
		}

		if (test.qty > 0){
			gain_life(player, 2 * new_sacrifice(player, card, player, SAC_NO_CANCEL|SAC_DONE, &test));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_revive(int player, int card, event_t event){
/* CARD_ID_REVIVE	1130
Revive	|1|G
Sorcery
Return target |Sgreen card from your graveyard to your hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_GREEN));
	this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
}

// Rishadan Airship --> Skywinder Drake

int card_rishadan_brigand(int player, int card, event_t event){
/* CARD_ID_RISHADAN_BRIGAND	4574
Rishadan Brigand	|4|U
Creature - Human Pirate 3/2
Flying
When ~ enters the battlefield, each opponent sacrifices a permanent unless he or she pays |3.
~ can block only creatures with flying. */

	return rishadan_delinquents(player, card, event, 3);
}

int card_rishadan_cutpurse(int player, int card, event_t event){
/* CARD_ID_RISHADAN_CUTPURSE	4575
Rishadan Cutpurse	|2|U
Creature - Human Pirate 1/1
When ~ enters the battlefield, each opponent sacrifices a permanent unless he or she pays |1. */

	return rishadan_delinquents(player, card, event, 1);
}

int card_rishadan_footpad(int player, int card, event_t event){
/* CARD_ID_RISHADAN_FOOTPAD	4576
Rishadan Footpad	|3|U
Creature - Human Pirate 2/2
When ~ enters the battlefield, each opponent sacrifices a permanent unless he or she pays |2. */

	return rishadan_delinquents(player, card, event, 2);
}

int card_rishadan_pawnshop(int player, int card, event_t event){

	/* Rishadan Pawnshop	|2
	 * Artifact
	 * |2, |T: Shuffle target nontoken permanent you control into its owner's library. */

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_type = TARGET_TYPE_TOKEN;
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( IS_AI(player) ){
			td.required_state = TARGET_STATE_DESTROYED;
			return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_REGENERATION, MANACOST_X(2), 0, &td, NULL);
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_PERMANENT");
	}

	if( event == EVENT_ACTIVATE ){
		if( IS_AI(player) ){
			td.required_state = TARGET_STATE_DESTROYED;
			ai_modifier += 24;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_PERMANENT");
	}

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		shuffle_into_library(instance->targets[0].player, instance->targets[0].card);
	}

	return 0;
}

int card_rishadan_port(int player, int card, event_t event){
	/* Rishadan Port	""
	 * Land
	 * |T: Add |C to your mana pool.
	 * |1, |T: Tap target land. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_TAP_LAND
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_X(2), 0, &td, NULL) )
		{
			choice = CHOICE_TAP_LAND;
			ai_modifier += (current_turn == player ? -24 : 24);
			ai_modifier += (count_subtype(1-player, TYPE_LAND, -1) <= 3 ? -24 : 24);
		}
		instance->info_slot = choice;
		if( choice == CHOICE_TAP_LAND && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 5,
							"Tap a land", 1, 10);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_TAP_LAND ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_LAND");
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_TAP_LAND ){
			if( valid_target(&td) ){
				tap_card(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_robber_fly(int player, int card, event_t event){
/*
	Robber Fly |2|R
	Creature — Insect 1/1
	Flying
	Whenever Robber Fly becomes blocked, defending player discards all the cards in his or her hand, then draws that many cards.
*/

	if( event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card) ){
		if( check_state(player, card, STATE_ATTACKING) && ! is_unblocked(player, card) ){
			int amount = hand_count[1-player];
			new_discard_all(1-player, player);
			draw_cards(1-player, amount);
		}
	}

	return 0;
}

int card_rushwood_elemental(int player, int card, event_t event){
/* CARD_ID_RUSHWOOD_ELEMENTAL	4582
Rushwood Elemental	|G|G|G|G|G
Creature - Elemental 4/4
Trample
At the beginning of your upkeep, you may put a +1/+1 counter on ~. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_1_1_counter(player, card);
	}

	return 0;
}

int card_rushwood_herbalist(int player, int card, event_t event){
/* CARD_ID_RUSHWOOD_HERBALIST	4584
Rushwood Herbalist	|2|G
Creature - Human Spellshaper 2/2
|G, |T, Discard a card: Regenerate target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_state = TARGET_STATE_DESTROYED;
	td.preferred_controller = player;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target( instance->targets[0].player, instance->targets[0].card );
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_REGENERATION|GAA_DISCARD, MANACOST_G(1), 0,
									&td, "TARGET_CREATURE");
}

int card_saber_ants(int player, int card, event_t event ){
	/* Saber Ants	|3|G
	 * Creature - Insect 2/3
	 * Whenever ~ is dealt damage, you may put that many 1/1 |Sgreen Insect creature tokens onto the battlefield. */
	// Used externally -> card_broodhatch_nantuko
	if( ! is_humiliated(player, card) && damage_dealt_to_me_arbitrary(player, card, event, DDBM_TRIGGER_OPTIONAL, player, card) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_INSECT, &token);
		token.qty = get_card_instance(player, card)->targets[7].card;
		generate_token(&token);
		get_card_instance(player, card)->targets[7].card = 0;
    }

	return 0;
}

int card_sand_squid(int player, int card, event_t event){
/* CARD_ID_SAND_SQUID	4589
Sand Squid	|3|U
Creature - Squid Beast 2/2
|H2Islandwalk (This creature can't be blocked as long as defending player controls |Han Island.)
You may choose not to untap ~ during your untap step.
|T: Tap target creature. That creature doesn't untap during its controller's untap step for as long as ~ remains tapped. */

	choose_to_untap(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
			does_not_untap_until_im_tapped(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
			card_instance_t *parent = get_card_instance( player, instance->parent_card );
			parent->targets[1] = instance->targets[0];
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_sandstone_needle(int player, int card, event_t event){
	/* Sandstone Needle	""
	 * Land
	 * ~ enters the battlefield tapped with two depletion counters on it.
	 * |T, Remove a depletion counter from ~: Add |R|R to your mana pool. If there are no depletion counters on ~, sacrifice it. */
	return depletion_land(player, card, event, COLOR_RED);
}

int card_saprazzan_heir(int player, int card, event_t event){
/*
Saprazzan Heir |1|U
Creature — Merfolk 1/1
Whenever Saprazzan Heir becomes blocked, you may draw three cards.
*/
	if( event == EVENT_CAN_ACTIVATE && current_phase == PHASE_AFTER_BLOCKING && is_attacking(player, card) &&
		! is_unblocked(player, card) && get_card_instance( player, card )->targets[1].player != 66 &&
		! is_humiliated(player, card) )
	{
		return 1;
	}

	if( event == EVENT_ACTIVATE ){
		get_card_instance( player, card )->targets[1].player = 66;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 3);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return 0;
}

int card_saprazzan_skerry(int player, int card, event_t event){
	/* Saprazzan Skerry	""
	 * Land
	 * ~ enters the battlefield tapped with two depletion counters on it.
	 * |T, Remove a depletion counter from ~: Add |U|U to your mana pool. If there are no depletion counters on ~, sacrifice it. */
	return depletion_land(player, card, event, COLOR_BLUE);
}

int card_seismic_mage(int player, int card, event_t event){
/* CARD_ID_SEISMIC_ASSAULT	1142
Seismic Assault	|R|R|R
Enchantment
Discard a land card: ~ deals 2 damage to target creature or player. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DISCARD, MANACOST_XR(2, 1), 0, &td, "TARGET_LAND");
}

int card_sever_soul(int player, int card, event_t event){
/* CARD_ID_SEVER_SOUL	1144
Sever Soul	|3|B|B
Sorcery
Destroy target non|Sblack creature. It can't be regenerated. You gain life equal to its toughness. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = get_toughness(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			gain_life(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, NULL);
}

int card_silverglade_elemental(int player, int card, event_t event){
/*
Silverglade Elemental |4|G
Creature — Elemental 4/4
When Silverglade Elemental enters the battlefield, you may search your library for a Forest card and put that card onto the battlefield.
	If you do, shuffle your library.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT,
									get_hacked_land_text(player, card, "Select a %s card.", SUBTYPE_FOREST));
		test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
	}

	return 0;
}

int card_silverglade_pathfinder(int player, int card, event_t event){
/* Silverglade Pathfinder	|1|G
 * Creature - Dryad Spellshaper 1/1
 * |1|G, |T, Discard a card: Search your library for a basic land card and put that card onto the battlefield tapped.
 *		Then shuffle your library. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD, MANACOST_XG(1, 1), 0, NULL, NULL);
}

int card_sizzle(int player, int card, event_t event){
/* CARD_ID_SIZZLE	1149
Sizzle	|2|R
Sorcery
~ deals 3 damage to each opponent. */

	if( event == EVENT_RESOLVE_SPELL ){
		damage_player(1-player, 3, player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// Skulking fugitive --> Skulking ghost

int card_skull_of_ramos(int player, int card, event_t event){
/* CARD_ID_SKULL_OF_RAMOS	4607
Skull of Ramos	|3
Artifact
|T: Add |B to your mana pool.
Sacrifice ~: Add |B to your mana pool. */

	return part_of_ramos(player, card, event, COLOR_BLACK);
}

int card_snuff_out(int player, int card, event_t event){
	/*
	  Snuff Out |3|B
	  Instant
	  If you control a Swamp, you may pay 4 life rather than pay Snuff Out's mana cost.
	  Destroy target nonblack creature. It can't be regenerated.
	*/

	if( event == EVENT_MODIFY_COST ){
		card_instance_t *instance = get_card_instance( player, card );
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_SWAMP) && can_pay_life(player, 4) ){
			instance->info_slot = 1;
			null_casting_cost(player, card);
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_iid(player, get_card_instance(player, card)->internal_card_id, event) ){
				choice = do_dialog(player, player, card, -1, -1, " Pay 4 life\n Play normally\n Cancel", life[player] >= 10 ? 0 : 1);
			}
			if( choice == 0 ){
				lose_life(player, 4);
				td.allow_cancel = 0;
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
							get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	if( event == EVENT_CAN_CHANGE_TARGET || event == EVENT_CHANGE_TARGET ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, NULL, 1, NULL);
	}

	return 0;
}

int card_soothsaying(int player, int card, event_t event){
/*
Soothsaying |U
Enchantment
{3}{U}{U}: Shuffle your library.
{X}: Look at the top X cards of your library, then put them back in any order.
*/
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	enum{
		CHOICE_SHUFFLE = 1,
		CHOICE_REARRANGE
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_X(-1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int lands = count_subtype(player, TYPE_LAND, -1);
		if( lands <= 3 || lands > 6 ){
			ai_modifier += 48;
		}
		instance->targets[0].player = 0;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Shuffle", has_mana_for_activated_ability(player, card, MANACOST_XU(3, 2)), 5,
							"Rearrange", 1, 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_SHUFFLE ){
			return generic_activated_ability(player, card, event, 0, MANACOST_XU(3, 2), 0, NULL, NULL);
		}
		if( choice == CHOICE_REARRANGE ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_X(-1)) ){
			     instance->targets[0].player = x_value;

			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SHUFFLE ){
			shuffle(player);
		}
		if( instance->info_slot == CHOICE_REARRANGE ){
			rearrange_top_x(player, player, instance->targets[0].player);
		}
	}

	return 0;
}

int card_spidersilk_armor(int player, int card, event_t event){
/* CARD_ID_SPIDERSILK_ARMOR	1333
Spidersilk Armor	|2|G
Enchantment
Creatures you control get +0/+1 and have reach. (They can block creatures with flying.) */

	boost_creature_type(player, card, event, -1, 0, 1, KEYWORD_REACH, BCT_CONTROLLER_ONLY);

	return global_enchantment(player, card, event);
}

int card_spiritual_focus(int player, int card, event_t event){
/* CARD_ID_SPIRITUAL_FOCUS	4615
Spiritual Focus	|1|W
Enchantment
Whenever a spell or ability an opponent controls causes you to discard a card, you gain 2 life and you may draw a card. */

	if (discard_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, DISCARD_NO_NORMAL)){
		gain_life(player, 2);
		draw_some_cards_if_you_want(player, card, player, 1);
	}

	return global_enchantment(player, card, event);
}

int card_spontaneous_generation(int player, int card, event_t event){
	/* Spontaneous Generation	|3|G
	 * Sorcery
	 * Put a 1/1 |Sgreen Saproling creature token onto the battlefield for each card in your hand. */

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, hand_count[player]);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_squee_goblin_nabob(int player, int card, event_t event){
/* CARD_ID_SQUEE_GOBLIN_NABOB	4619
Squee, Goblin Nabob	|2|R
Legendary Creature - Goblin 1/1
At the beginning of your upkeep, you may return ~ from your graveyard to your hand. */

	check_legend_rule(player, card, event);

	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		int choice = 0;
		if( ! duh_mode(player) ){
			choice = do_dialog(player, player, card, -1, -1," Return Squee to hand\n Leave Squee be\n", 0);
		}
		if( choice == 0 ){
			card_instance_t *instance= get_card_instance(player, card);
			instance->state &= ~STATE_INVISIBLE;
			hand_count[player]++;
			return -1;
		}
		else{
			return -2;
		}
	}

	return 0;
}

int card_squeeze(int player, int card, event_t event){
/* CARD_ID_SQUEEZE	4620
Squeeze	|3|U
Enchantment
Sorcery spells cost |3 more to cast. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_MODIFY_COST_GLOBAL && is_what(affected_card_controller, affected_card, TYPE_SORCERY) ){
			COST_COLORLESS+=3;
		}
	}

	return global_enchantment(player, card, event);
}

int card_statecraft(int player, int card, event_t event ){
/*
Statecraft |3|U
Enchantment
Prevent all combat damage that would be dealt to and dealt by creatures you control.
*/
	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *damage= get_card_instance(affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card &&
				(damage->token_status & (STATUS_COMBAT_DAMAGE|STATUS_FIRST_STRIKE_DAMAGE)) )
			{
				if( damage->damage_target_card != -1 && damage->damage_target_player == player &&
					damage->info_slot > 0 && is_what(damage->damage_target_player, damage->damage_target_card, TYPE_CREATURE) )
				{
					prevent_damage(affected_card_controller, affected_card, -1);
				}
				if( damage->damage_source_card != -1 && damage->damage_source_player == player &&
					damage->info_slot > 0 && is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE) )
				{
					prevent_damage(affected_card_controller, affected_card, -1);
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

static const char* colored_damage_source_that_damages_me(int who_chooses, int player, int card, int targeting_player, int targeting_card){
	card_instance_t* dmg = get_card_instance(player, card);
	if( dmg->internal_card_id == damage_card && dmg->damage_target_player == who_chooses && dmg->damage_target_card == -1 ){
		int clr = dmg->initial_color;
		if( in_play(dmg->damage_source_player, dmg->damage_source_card) ){
			if( check_special_flags(dmg->damage_source_player, dmg->damage_source_card, SF_ATTACKING_PWALKER) ){
				return "damaging a Planeswalker.";
			}
			else{
				clr = get_color(dmg->damage_source_player, dmg->damage_source_card);
			}
		}
		if( clr & get_card_instance(targeting_player, targeting_card)->info_slot ){
			return NULL;
		}
	}

	return gs_target_error_color;//",color"
}

int card_story_circle(int player, int card, event_t event){
/* CARD_ID_STORY_CIRCLE	1163
Story Circle	|1|W|W
Enchantment
As ~ enters the battlefield, choose a color.
|W: The next time a source of your choice of the chosen color would deal damage to you this turn, prevent that damage. */


	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = 1<<choose_a_color_and_show_legacy(player, card, 1-player, -1);
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_EFFECT);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(colored_damage_source_that_damages_me);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, -1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_W(1), 0, &td, "TARGET_DAMAGE");
}

int card_task_force(int player, int card, event_t event){
/* CARD_ID_TASK_FORCE	4628
Task Force	|2|W
Creature - Human Rebel 1/3
Whenever ~ becomes the target of a spell or ability, it gets +0/+3 until end of turn. */

  if (in_play(player, card)
	  && !is_humiliated(player, card)
	  && becomes_target_of_spell_or_effect(player, card, event, player, card, ANYBODY))
	pump_until_eot(player, card, player, card, 0, 3);

  return 0;
}

int card_tectonic_break(int player, int card, event_t event){
/* CARD_ID_TECTONIC_BREAK	4629
Tectonic Break	|X|R|R
Sorcery
Each player sacrifices X lands. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		test.qty = instance->info_slot;
		APNAP(p, { new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_thrashing_wumpus(int player, int card, event_t event){
/* CARD_ID_THRASHING_WUMPUS	1259
Thrashing Wumpus	|3|B|B
Creature - Beast 3/3
|B: ~ deals 1 damage to each creature and each player. */

  return pestilence_impl(player, card, event, 0, COLOR_BLACK);
}

int card_thwart(int player, int card, event_t event){
	/* Thwart	|2|U|U
	 * Instant
	 * You may return three |H1Islands you control to their owner's hand rather than pay ~'s mana cost.
	 * Counter target spell. */

	if( event == EVENT_MODIFY_COST ){
		card_instance_t *instance = get_card_instance(player, card);
		if( count_subtype(player, TYPE_LAND, SUBTYPE_ISLAND) > 2 ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return counterspell(player, card, event, NULL, 0);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_id(player, event, get_id(player, card)) ){
				choice = do_dialog(player, player, card, -1, -1, " Bounce 3 Islands\n Cast normally\n Cancel", 0);
			}
			if( choice == 0 ){
				target_definition_t td;
				default_target_definition(player, card, &td, TYPE_LAND);
				td.allowed_controller = player;
				td.preferred_controller = player;
				td.required_subtype = SUBTYPE_ISLAND;
				td.illegal_abilities = 0;

				instance->number_of_targets = 0;

				int trgs = 0;
				while( trgs < 3 ){
						if( new_pick_target(&td, "Select an Island to return to your hand.", trgs, GS_LITERAL_PROMPT) ){
							state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
							trgs++;
						}
						else{
							break;
						}
				}
				int i;
				for(i=0; i<trgs; i++){
					state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
					if( trgs == 3 ){
						bounce_permanent(instance->targets[i].player, instance->targets[i].card);
					}
				}
				if( trgs < 3 ){
					spell_fizzled = 1;
					return 0;
				}
				instance->number_of_targets = 0;
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
			}
		}
		if( spell_fizzled != 1 ){
			return counterspell(player, card, event, NULL, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		return counterspell(player, card, event, NULL, 0);
	}

	return 0;
}

int card_tooth_of_ramos(int player, int card, event_t event){
/* CARD_ID_TOOTH_OF_RAMOS	4637
Tooth of Ramos	|3
Artifact
|T: Add |W to your mana pool.
Sacrifice ~: Add |W to your mana pool. */

	return part_of_ramos(player, card, event, COLOR_WHITE);
}

int card_toymaker(int player, int card, event_t event){//UNUSEDCARD
/* CARD_ID_TOYMAKER	4639
Toymaker	|2
Artifact Creature - Spellshaper 1/1
|1, |T, Discard a card: Target noncreature artifact becomes an artifact creature with power and toughness each equal to its converted mana cost until end of turn.
(It retains its abilities.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.illegal_type = TYPE_CREATURE;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			artifact_animation(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, -1, -1, 0, 0);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_ARTIFACT");
}

int card_trade_routes(int player, int card, event_t event){
	/*
	  Trade Routes |1|U
	  Enchantment
	  {1}: Return target land you control to its owner's hand.
	  {1}, Discard a land card: Draw a card.
	*/

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;
	td.allowed_controller = player;
	td.illegal_abilities = 0;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land card to discard.");
	test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL) ){
		if( can_target(&td) ){
			return 1;
		}
		return check_battlefield_for_special_card(player, card, player, 0, &test);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Return a target land to your hand", can_target(&td), 5,
							"Discard a land card and draw", check_battlefield_for_special_card(player, card, player, 0, &test), 5 + (7-hand_count[player]));
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			if( choice == 1 ){
				new_pick_target(&td, "Select target land you control.", 0, 1 | GS_LITERAL_PROMPT);
			}
			if( choice == 2 ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &test);
				if( selected != -1 ){
					discard_card(player, selected);
				}
				else{
					spell_fizzled = 1;
				}
			}
			if( spell_fizzled != 1 ){
				instance->info_slot = choice;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 && valid_target(&td) ){
			action_on_target(player, card, 0, ACT_BOUNCE);
		}
		if( instance->info_slot == 2 ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_two_headed_dragon(int player, int card, event_t event){
	/* Two-Headed Dragon	|4|R|R
	 * Creature - Dragon 4/4
	 * Flying
	 * Menace
	 * ~ can block an additional creature each combat.
	 * |1|R: ~ gets +2/+0 until end of turn. */

	menace(player, card, event);

	creature_can_block_additional(player, card, event, 1);

	return generic_shade(player, card, event, 0, MANACOST_XR(1, 1), 2, 0, 0, 0);
}

int card_undertaker(int player, int card, event_t event){
/* CARD_ID_UNDERTAKER	4641
Undertaker	|1|B
Creature - Human Spellshaper 1/1
|B, |T, Discard a card: Return target creature card from your graveyard to your hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED | GAA_DISCARD, MANACOST_B(1), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER);
}

int card_unmask(int player, int card, event_t event){
/* CARD_ID_UNMASK	4642
Unmask	|3|B
Sorcery
You may exile a |Sblack card from your hand rather than pay ~'s mana cost.
Target player reveals his or her hand. You choose a nonland card from it. That player discards that card. */

	al_pitchspell(player, card, event, COLOR_BLACK, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_PLAYER", 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
			if( casting_al_pitchspell(player, card, event, COLOR_BLACK, 0) ){
				return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_PLAYER", 1, NULL);
			}
			else{
			      spell_fizzled = 1;
		    }

	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			this_test.type_flag = DOESNT_MATCH;

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_uphill_battle(int player, int card, event_t event){

	/* Uphill Battle	|2|R
	 * Enchantment
	 * Creatures played by your opponents enter the battlefield tapped. */

	permanents_enters_battlefield_tapped(player, card, event, 1-player, TYPE_CREATURE, NULL);

	return global_enchantment(player, card, event);
}

int card_vendetta(int player, int card, event_t event){
/* CARD_ID_VENDETTA	4645
Vendetta	|B
Instant
Destroy target non|Sblack creature. It can't be regenerated. You lose life equal to that creature's toughness. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			int amount = get_toughness(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			lose_life(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, NULL);
}

int card_vine_dryad(int player, int card, event_t event){
/* CARD_ID_VINE_DRYAD	4648
Vine Dryad	|3|G
Creature - Dryad 1/3
Flash
|H2Forestwalk (This creature can't be blocked as long as defending player controls |Ha Forest.)
You may exile a |Sgreen card from your hand rather than pay ~'s mana cost. */

	al_pitchspell(player, card, event, COLOR_GREEN, 0);

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! casting_al_pitchspell(player, card, event, COLOR_GREEN, 0) ){
			spell_fizzled = 1;
		}
	}

	return flash(player, card, event);
}

static int war_tax_legacy(int player, int card, event_t event){
	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_war_tax(int player, int card, event_t event){
/* CARD_ID_WAR_TAX	4652
War Tax	|2|U
Enchantment
|X|U: This turn, creatures can't attack unless their controller pays |X for each attacking creature he or she controls. */

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->targets[1].player > 0 ){
		if( ! is_humiliated(player, card) ){
			tax_attack(player, card, event);
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_XU(1, 1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( current_turn != player && current_phase < PHASE_DECLARE_ATTACKERS ){
			ai_modifier+=15;
		}
		else{
			ai_modifier-=50;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_U(1)) ){
			charge_mana(player, COLOR_COLORLESS, -1);
			if( spell_fizzled != 1 ){
				instance->info_slot = x_value;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
		parent->targets[1].player = instance->info_slot;
		create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &war_tax_legacy,
										instance->parent_controller, instance->parent_card);
	}

	return global_enchantment(player, card, event);
}

int card_waterfront_bouncer(int player, int card, event_t event){
/* CARD_ID_WATERFRONT_BOUNCER	4655
Waterfront Bouncer	|1|U
Creature - Merfolk Spellshaper 1/1
|U, |T, Discard a card: Return target creature to its owner's hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DISCARD, MANACOST_U(1), 0, &td, "TARGET_CREATURE");
}

int card_wave_of_reckoning(int player, int card, event_t event){
/*
Wave of Reckoning |4|W
Sorcery
Each creature deals damage to itself equal to its power.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p,{
					int c;
					for(c = active_cards_count[p]-1; c>-1; c--){
						if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
							damage_creature(p, c, get_power(p, c), p, c);
						}
					}
				}
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}
