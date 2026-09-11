#include "manalink.h"

/* Cards that were not coded since they have clones already:
1) Arc Runner -> Ball Lightning
2) Canyon Minotaur -> Hill Giant
3) Fiery Hellhound -> Shivan Dragon
4) Goblin Piker -> Hill Giant
5) Maniac Vandal -> Viridian Shaman
6) Reverberate -> Fork (too lazy for to code the miniscule difference)
7) Vulshok Berserker -> Raging Goblin
8) Stone Golem -> Stonework Puma
9) Autumn's Veil -> skipped (impossible)
10) Back to Nature -> Tranquility
11) Cudgel Troll -> Wall of Brambles
12) Duskdale Wurm -> Craw Wurm
13) Garruk's Companion -> Craw Wurm
14) Obstinate Baloth -> Staunch Defenders
15) Plummet -> Wing Snare
16) Runeclaw Bear -> Grizzly Bear
17) Sylvan Ranger -> Civic Wayfinder
18) Wall of Vines -> Wall of Wood
19) Yavimaya Wurm -> Craw Wurm
20) Aether Adept -> Man O war
21) Armored Cancrix -> Aie Elemental
22) Cloud Elemental -> Welkin Tern
23) Jaces Ingenuity -> Concentrate
24) Leyline of Anticipation -> skipped (impossible)
25) Maritime Guard -> Air Elemental
26) Mind Control -> Control Magic
27) Redirect -> skipped (impossible)
28) Scroll Thief -> Thieving Magpie
*/


/* WHITE CARDS */

int card_ajanis_mantra(int player, int card, event_t event){
	/* Ajani's Mantra	|1|W
	 * Enchantment
	 * At the beginning of your upkeep, you may gain 1 life. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_DUH);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		gain_life( player, 1 );
	}

	return global_enchantment(player, card, event);
}

int card_ajanis_pridemate(int player, int card, event_t event)
{
/* CARD_ID_AJANIS_PRIDEMATE	11087
Ajani's Pridemate	|1|W
Creature - Cat Soldier 2/2
Whenever you gain life, put a +1/+1 counter on ~. */

  if (trigger_gain_life(player, card, event, player, RESOLVE_TRIGGER_MANDATORY))
	add_1_1_counter(player, card);

  return 0;
}

int card_angelic_arbiter(int player, int card, event_t event){
/*
Angelic Arbiter |5|W|W
Creature — Angel 5/6
Flying
Each opponent who cast a spell this turn can't attack with creatures.
Each opponent who attacked with a creature this turn can't cast spells.
*/
	if( current_turn != player && in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);

		enum{
			OPP_CANT_ATTACK = 1,
			OPP_CANT_CAST_SPELLS
		};

		//CHECK IF OPPONENT IS ATTACKING
		if (event == EVENT_DECLARE_ATTACKERS && current_turn == 1-player && count_attackers(current_turn) > 0){
			if( instance->targets[1].card < 0 ){
				instance->targets[1].card = 0;
			}
			instance->targets[1].card |= OPP_CANT_CAST_SPELLS;
		}
		//INCREASING THE COST IF OPPONENT ATTACKED THIS TURN
		if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == 1-player && instance->targets[1].card > -1 ){
			if( ! is_what(affected_card_controller, affected_card, TYPE_LAND) && (instance->targets[1].card & OPP_CANT_CAST_SPELLS) ){
				infinite_casting_cost();
			}
		}

		//CHECK IF OPPONENT IS CASTING A SPELL
		if( event == EVENT_CAST_SPELL && affected_card_controller == 1-player &&
			! is_what(affected_card_controller, affected_card, TYPE_LAND) )
		{
			if( instance->targets[1].card < 0 ){
				instance->targets[1].card = 0;
			}
			instance->targets[1].card |= OPP_CANT_ATTACK;
		}

		//MAKING IT IMPOSSIBLE TO ATTACK IF SPELL WAS PLAYED THIS TURN
		if( instance->targets[1].card > -1 && (instance->targets[1].card & OPP_CANT_ATTACK) ){
			nobody_can_attack(player, card, event, 1-player);
		}

		if( event == EVENT_CLEANUP ){
			instance->targets[1].card = 0;
		}
	}


	return 0;
}

int card_armored_ascension(int player, int card, event_t event)
{
  /* Armored Ascension	|3|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+1 for each |H2Plains you control and has flying. */

  card_instance_t* instance;
  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES)
	  && (instance = in_play(player, card))
	  && ! is_humiliated(player, card)
	  && affect_me(instance->damage_target_player, instance->damage_target_card))
	{
	  if (event == EVENT_ABILITIES)
		event_result |= KEYWORD_FLYING;
	  else
		event_result += count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_PLAINS));
	}

  return vanilla_aura(player, card, event, player);
}

int card_celestial_purge(int player, int card, event_t event){
/* CARD_ID_CELESTIAL_PURGE	10120
Celestial Purge	|1|W
Instant
Exile target |Sblack or |Sred permanent. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK) | get_sleighted_color_test(player, card, COLOR_TEST_RED);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE );
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text2(player, card, "Select a %s or %s creature card.", COLOR_BLACK, COLOR_RED), 1, NULL);
}

int card_excommunicate(int player, int card, event_t event){
/* CARD_ID_EXCOMMUNICATE	9964
Excommunicate	|2|W
Sorcery
Put target creature on top of its owner's library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_goldenglow_moth(int player, int card, event_t event){
/* CARD_ID_GOLDENGLOW_MOTH	9552
Goldenglow Moth	|W
Creature - Insect 0/1
Flying
Whenever ~ blocks, you may gain 4 life. */

	if( event == EVENT_DECLARE_BLOCKERS && get_card_instance(player, card)->blocking < 255 && ! is_humiliated(player, card) ){
		if( do_dialog(player, player, card, -1, -1, " Gain life\n Pass", 0) == 0 ){
			gain_life(player, 4 );
		}
	}

	return 0;
}

int card_infantry_veteran(int player, int card, event_t event){
/* CARD_ID_INFANTRY_VETERAN	2723
Infantry Veteran	|W
Creature - Human Soldier 1/1
|T: Target attacking creature gets +1/+1 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target( &td ) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target attacking creature.");
}


int card_inspired_charge(int player, int card, event_t event){
/* CARD_ID_INSPIRED_CHARGE	11135
Inspired Charge	|2|W|W
Instant
Creatures you control get +2/+1 until end of turn. */

	if(event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 2, 1, 0, 0);
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_knight_exemplar(int player, int card, event_t event)
{
  /* Knight Exemplar	|1|W|W
   * Creature - Human Knight 2/2
   * First strike
   * Other Knight creatures you control get +1/+1 and are indestructible. */

  boost_subtype(player, card, event, SUBTYPE_KNIGHT, 1,1, 0,SP_KEYWORD_INDESTRUCTIBLE, BCT_CONTROLLER_ONLY);
  return 0;
}

int card_leyline_of_sanctity(int player, int card, event_t event){
/* CARD_ID_LEYLINE_OF_SANCTITY	11141
Leyline of Sanctity	|2|W|W
Enchantment
If ~ is in your opening hand, you may begin the game with it on the battlefield.
You have hexproof. (You can't be the target of spells or abilities your opponents control.) */
    if( in_play(player, card) && !is_humiliated(player, card) ){
	     give_hexproof_to_player(player, card, event);
	}
	return global_enchantment(player, card, event);
}

int card_mighty_leap(int player, int card, event_t event){
/* CARD_ID_MIGHTY_LEAP	11149
Mighty Leap	|1|W
Instant
Target creature gets +2/+2 and gains flying until end of turn. */

	return vanilla_instant_pump(player, card, event, player, player, 2, 2, KEYWORD_FLYING, 0);
}

int card_roc_egg(int player, int card, event_t event){
	/* Roc Egg	|2|W
	 * Creature - Bird 0/3
	 * Defender
	 * When ~ dies, put a 3/3 |Swhite Bird creature token with flying onto the battlefield. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		generate_token_by_id(player, card, CARD_ID_ROC);
	}

	return 0;
}


//Safe Passage
static int effect_safe_passage(int player, int card, event_t event){

	card_instance_t *damage = damage_being_prevented(event);
	if( damage ){
		if( ! damage_is_to_planeswalker(damage) && damage->damage_target_player == player ){
			prevent_damage(affected_card_controller, affected_card, -1);
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_SACRIFICE );
	}

	return 0;
}

int card_safe_passage(int player, int card, event_t event){
/* CARD_ID_SAFE_PASSAGE	10462
Safe Passage	|2|W
Instant
Prevent all damage that would be dealt to you and creatures you control this turn. */

	if( event == EVENT_CAN_CAST ){
		if( IS_AI(player) && (land_can_be_played && LCBP_DAMAGE_PREVENTION) ){
			int total_dmg = 0;
			CYCLE_ALL_CARDS(p, c, 	{
										if( in_play(p, c) && is_what(p, c, TYPE_EFFECT) ){
											card_instance_t *instance = get_card_instance(p, c);
											if( instance->internal_card_id == damage_card && instance->damage_target_player == player ){
												total_dmg += instance->info_slot;
											}
										}
									}
			);
			if( total_dmg ){
				return 99;
			}
		}
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( land_can_be_played && LCBP_DAMAGE_PREVENTION ){
			int total_dmg = 0;
			CYCLE_ALL_CARDS(p, c, 	{
										if( in_play(p, c) && is_what(p, c, TYPE_EFFECT) ){
											card_instance_t *instance = get_card_instance(p, c);
											if( instance->internal_card_id == damage_card && instance->damage_target_player == player ){
												total_dmg += (instance->info_slot * 4);
											}
										}
									}
			);
			ai_modifier += total_dmg;
		}
		else{
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			test.state = STATE_ATTACKING;
			int total_pow = check_battlefield_for_special_card(player, card, player, CBFSC_GET_TOTAL_POW, &test);
			ai_modifier += (total_pow * 4);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &effect_safe_passage);
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_serra_ascendant(int player, int card, event_t event){
/* CARD_ID_SERRA_ASCENDANT	11172
Serra Ascendant	|W
Creature - Human Monk 1/1
Lifelink (Damage dealt by this creature also causes you to gain that much life.)
As long as you have 30 or more life, ~ gets +5/+5 and has flying. */

	if ((event == EVENT_ABILITIES || event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) &&
		!is_humiliated(player, card) && in_play(player, card) )
	{
		lifelink(player, card, event);
		if (life[player] >= 30){
			modify_pt_and_abilities(player, card, event, 5, 5, KEYWORD_FLYING);
		}
	}
	return 0;
}

int card_silence(int player, int card, event_t event ){
/*
Silence |W
Instant
Your opponents can't cast spells this turn. (Spells cast before this resolves are unaffected.)
*/
	if( event == EVENT_RESOLVE_SPELL ){
		target_player_cant_cast_type(player, card, 1-player, TYPE_CREATURE|TYPE_ENCHANTMENT|TYPE_ARTIFACT|TARGET_TYPE_PLANESWALKER|TYPE_SPELL|TYPE_EFFECT);
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_solemn_offering(int player, int card, event_t event){
/* CARD_ID_SOLEMN_OFFERING	10471
Solemn Offering	|2|W
Sorcery
Destroy target artifact or enchantment. You gain 4 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if(validate_target(player, card, &td, 0)){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			gain_life( player, 4 );
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
}

int card_squadron_hawk(int player, int card, event_t event){
/* CARD_ID_SQUADRON_HAWK	11174
Squadron Hawk	|1|W
Creature - Bird 1/1
Flying
When ~ enters the battlefield, you may search your library for up to three cards named ~, reveal them, put them into your hand, then shuffle your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		char buffer[100];
		card_ptr_t* c = cards_ptr[ get_id(player, card) ];
		scnprintf(buffer, 100, "Select a card named %s.", c->name);

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, buffer);
		this_test.id = get_id(player, card);
		this_test.qty = MIN( count_deck(player), 3);

		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);

		shuffle(player);
	}

	return 0;
}

int card_sun_titan(int player, int card, event_t event){
/* CARD_ID_SUN_TITAN	11179
Sun Titan	|4|W|W
Creature - Giant 6/6
Vigilance
Whenever ~ enters the battlefield or attacks, you may return target permanent card with converted mana cost 3 or less from your graveyard to the battlefield. */

	vigilance(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), player, card) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a permanent with CMC 3 or less.");
		this_test.cmc = 4;
		this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;

		int can_tutor = new_special_count_grave(player, &this_test) > 0 && ! graveyard_has_shroud(player);

		if( can_tutor ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

int card_tireless_missionaries(int player, int card, event_t event){
/* CARD_ID_TIRELESS_MISSIONARIES	11185
Tireless Missionaries	|4|W
Creature - Human Cleric 2/3
When ~ enters the battlefield, you gain 3 life. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life( player, 3 );
	}

	return 0;
}

int card_vengeful_archon(int player, int card, event_t event){
	/* Vengeful Archon	|4|W|W|W	0x20027a3
	 * Creature - Archon 7/7
	 * Flying
	 * |X: Prevent the next X damage that would be dealt to you this turn. If damage is prevented this way, ~ deals that much damage to target player or planeswalker. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;
	td.illegal_abilities = 0;
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, NULL) ){
			if( can_target(&td1) ){
				return 99;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = instance->info_slot = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			if( pick_target(&td, "TARGET_DAMAGE") ){
				card_instance_t *dmg = get_card_instance(instance->targets[0].player, instance->targets[0].card);
				int amount = dmg->info_slot;
				if( ! IS_AI(player) ){
					charge_mana(player, COLOR_COLORLESS, -1);
					if( spell_fizzled !=1 ){
						instance->info_slot = x_value;
					}
				}
				else{
					while( ! has_mana(player, COLOR_COLORLESS, amount) ){
							amount--;
					}
					charge_mana(player, COLOR_COLORLESS, amount);
					if( spell_fizzled !=1 ){
						instance->info_slot = amount;
					}
				}
				if( spell_fizzled != 1 ){
					new_pick_target(&td1, "TARGET_PLAYER_OR_PLANESWALKER", 1, 1);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int prevented = prevent_damage(instance->targets[0].player, instance->targets[0].card, instance->info_slot);
			if( prevented && validate_target(player, card, &td1, 1) ){
				damage_creature(instance->targets[1].player, instance->targets[1].card, prevented, instance->parent_controller, instance->parent_card);
			}
		}
	}

	return 0;
}

int card_war_priest_of_thune(int player, int card, event_t event){
/* CARD_ID_WAR_PRIEST_OF_THUNE	11190
War Priest of Thune	|1|W
Creature - Human Cleric 2/2
When ~ enters the battlefield, you may destroy target enchantment. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_ENCHANTMENT", 0, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

/* BLACK CARDS */

int card_blood_tithe(int player, int card, event_t event ){
/* CARD_ID_BLOOD_TITHE	11098
Blood Tithe	|3|B
Sorcery
Each opponent loses 3 life. You gain life equal to the life lost this way. */

	if( event == EVENT_RESOLVE_SPELL ){
		int result = lose_life(1-player, 3);
		gain_life( player, result );
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_captivating_vampire(int player, int card, event_t event){
/* Captivating Vampire	|1|B|B
 * Creature - Vampire 2/2
 * Other Vampire creatures you control get +1/+1.
 * Tap five untapped Vampires you control: Gain control of target creature. It becomes a Vampire in addition to its other types. */

	if (!IS_GAA_EVENT(event)){
		return boost_creature_type(player, card, event, SUBTYPE_VAMPIRE, 1, 1, 0, BCT_CONTROLLER_ONLY);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_abilities = 0;
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_VAMPIRE;
	td.illegal_state = TARGET_STATE_TAPPED;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td.allow_cancel = 0;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( target_available(player, card, &td) >= 5 ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
		}
	}

	if( event == EVENT_ACTIVATE){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			if( tapsubtype_ability(player, card, 5, &td) ){
				pick_target(&td1, "TARGET_CREATURE");
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			if( ! has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_VAMPIRE) ){
				add_a_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_VAMPIRE);
			}
			gain_control(player, card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_deathmark(int player, int card, event_t event){
/* CARD_ID_DEATHMARK	8357
Deathmark	|B
Sorcery
Destroy target |Sgreen or |Swhite creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN) | get_sleighted_color_test(player, card, COLOR_TEST_WHITE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY );
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text2(player, card, "Select a %s or %s creature.", COLOR_GREEN, COLOR_WHITE), 1, NULL);
}

int card_demon_of_deaths_gate(int player, int card, event_t event){
/* CARD_ID_DEMON_OF_DEATHS_GATE	11113
Demon of Death's Gate	|6|B|B|B
Creature - Demon 9/9
You may pay 6 life and sacrifice three |Sblack creatures rather than pay ~'s mana cost.
Flying, trample */

	if( event == EVENT_MODIFY_COST ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		test.qty = 3;

		if( new_can_sacrifice_as_cost(player, card, &test) && can_pay_life(player, 6) ){
			get_card_instance( player, card )->info_slot = 1;
			null_casting_cost(player, card);
		}
		else{
			get_card_instance( player, card )->info_slot = 0;
		}

	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1){
			int choice = 0;
			int ai_choice = 0;
			if( player == AI && life[player]-6 > 5 ){
				ai_choice = 1;
			}
			if( has_mana_to_cast_iid(player, event, instance->internal_card_id) ){
				choice = do_dialog(player, player, card, -1, -1, " Sac 3 black guys & lose 6 life\n Play normally\n Cancel", ai_choice);
			}

			if( choice == 0 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_CREATURE, "Select a black creature to sacrifice.");
				test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

				int sacced[3];
				int sc = 0;
				while( sc < 3 ){
						int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
						if (!sac){
							cancel = 1;
							break;
						}
						sacced[sc] = sac;
						sc++;
				}
				int i;
				for(i=0; i<sc; i++){
					state_untargettable(BYTE2(sacced[i]), BYTE3(sacced[i]), 0);
				    if( sc == 3 ){
						kill_card(BYTE2(sacced[i]), BYTE3(sacced[i]), KILL_SACRIFICE);
					}
				}
                if ( KILL_SACRIFICE == sc ){
			     lose_life(player, 6);
			   }
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, -1, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	return 0;
}

int card_doom_blade(int player, int card, event_t event){
	/* Doom Blade	|1|B
	 * Instant
	 * Destroy target non|Sblack creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY );
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, NULL);
}

int card_grave_titan(int player, int card, event_t event){
	/* Grave Titan	|4|B|B
	 * Creature - Giant 6/6
	 * Deathtouch
	 * Whenever ~ enters the battlefield or attacks, put two 2/2 |Sblack Zombie creature tokens onto the battlefield. */

	deathtouch(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card)){
		generate_tokens_by_id(player, card, CARD_ID_ZOMBIE, 2);
	}

	return 0;
}

int card_howling_banshee(int player, int card, event_t event){
/* CARD_ID_HOWLING_BANSHEE	10431
Howling Banshee	|2|B|B
Creature - Spirit 3/3
Flying
When ~ enters the battlefield, each player loses 3 life. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		APNAP(p, {lose_life(p, 3);};);
	}

	return 0;
}

int card_lilianas_specter(int player, int card, event_t event){
	/*
	  Liliana's Specter |1|B|B
	  Creature - Specter 2/1
	  Flying
	  When Liliana's Specter enters the battlefield, each opponent discards a card.
	*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		discard(1-player, 0, player);
	}

	return 0;
}

static int necrotic_plague_legacy(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		const int *grave = get_grave(instance->targets[1].player);
		int count = count_graveyard(instance->targets[1].player)-1;
		while( count > -1 ){
				if( grave[count] == instance->targets[1].card ){
					int card_added = add_card_to_hand(instance->targets[1].player, grave[count]);
					set_special_flags3(instance->targets[1].player, card_added, SF3_REANIMATED);
					card_instance_t *this = get_card_instance(instance->targets[1].player, card_added);
					this->targets[0] = instance->targets[0];
					set_special_flags(instance->targets[1].player, card_added, SF_TARGETS_ALREADY_SET);
					put_into_play(instance->targets[1].player, card_added);
					break;
				}
				count--;
		}
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_necrotic_plague(int player, int card, event_t event){
	/*
	  Necrotic Plague |2|B|B
	  Enchantment - Aura
	  Enchant creature
	  Enchanted creature has "At the beginning of your upkeep, sacrifice this creature."
	  When enchanted creature dies, its controller chooses target creature one of his or her opponents controls.
	  Return Necrotic Plague from its owner's graveyard to the battlefield attached to that creature.
	*/

	if( get_card_instance(player, card)->damage_target_player > -1 && in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(t_player, t_card) ){
			if( is_this_dying(t_player, t_card) ){
				target_definition_t td;
				default_target_definition(t_player, t_card, &td, TYPE_CREATURE );
				td.who_chooses = t_player;
				td.allowed_controller = 1-t_player;
				td.preferred_controller = 1 - t_player;
				if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
					int legacy = create_legacy_effect(player, card, &necrotic_plague_legacy);
					card_instance_t *leg = get_card_instance(player, legacy);
					leg->targets[0] = instance->targets[0];
					leg->targets[1].player = get_owner(player, card);
					leg->targets[1].card = get_original_internal_card_id(player, card);
					leg->targets[11].player = 66;
				}
			}
		}

		if( current_turn == t_player && upkeep_trigger(player, card, event) ){
			if( can_cause_sacrifice(player, t_player) && ! check_special_flags2(t_player, t_card, SF2_CANNOT_BE_SACRIFICED) ){
				kill_card(t_player, t_card, KILL_SACRIFICE);
			}
		}
	}

	return disabling_aura(player, card, event);
}

int card_nightwing_shade(int player, int card, event_t event){
/* CARD_ID_NIGHTWING_SHADE	11154
Nightwing Shade	|4|B
Creature - Shade 2/2
Flying
|1|B: ~ gets +1/+1 until end of turn. */

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_XB(1, 1), 1, 1);
}

int card_phylactery_lich(int player, int card, event_t event)
{
  /* Phylactery Lich	|B|B|B
   * Creature - Zombie 5/5
   * As ~ enters the battlefield, put a phylactery counter on an artifact you control.
   * ~ is indestructible.
   * When you control no permanents with phylactery counters on them, sacrifice ~. */

  indestructible(player, card, event);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  target_definition_t td;
	  base_target_definition(player, card, &td, TYPE_ARTIFACT);
	  td.allowed_controller = player;
	  td.preferred_controller = player;
	  td.allow_cancel = 0;

	  card_instance_t* instance = get_card_instance(player, card);
	  if (can_target(&td) && pick_target(&td, "TARGET_ARTIFACT"))
		add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_PHYLACTERY);

	  instance->number_of_targets = 0;
	}

  if (event == EVENT_STATIC_EFFECTS && ! is_humiliated(player, card) && !has_any_counters(player, COUNTER_PHYLACTERY, TYPE_PERMANENT))
	kill_card(player, card, KILL_SACRIFICE);

  return 0;
}

int card_quag_sickness(int player, int card, event_t event)
{
  /* Quag Sickness	|2|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets -1/-1 for each |H2Swamp you control. */

  card_instance_t* instance;
  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS)
	  && (instance = in_play(player, card))
	  && affect_me(instance->damage_target_player, instance->damage_target_card))
	event_result -= count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_SWAMP));

  return disabling_aura(player, card, event);
}

int card_reassembling_skeleton(int player, int card, event_t event){
	/* Reassembling Skeleton	|1|B
	 * Creature - Skeleton Warrior 1/1
	 * |1|B: Return ~ from your graveyard to the battlefield tapped. */

	if(event == EVENT_GRAVEYARD_ABILITY){
		if( has_mana_multi( player, MANACOST_XB(1, 1) ) ){
			return GA_RETURN_TO_PLAY_MODIFIED;
		}
	}
	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana_multi( player, MANACOST_XB(1, 1) );
		if( spell_fizzled != 1){
			return GAPAID_REMOVE;
		}
	}
	if( event == EVENT_RETURN_TO_PLAY_FROM_GRAVE_MODIFIED ){
		add_state(player, card, STATE_TAPPED);
	}
	return 0;
}

int card_relentless_rats(int player, int card, event_t event){
/*
Relentless Rats |1|B|B
Creature — Rat 2/2
Relentless Rats gets +1/+1 for each other creature on the battlefield named Relentless Rats.
A deck can have any number of cards named Relentless Rats.
*/
	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			int pump = 0;
			CYCLE_ALL_CARDS(p, c, 	{
										if( in_play(p, c) && !(p == player && c == card) && has_same_name(player, card, p, c) ){
											pump++;
										}
									}
			);
			event_result += pump;
		}
	}

	return 0;
}

int card_rise_from_the_grave(int player, int card, event_t event){
/* CARD_ID_RISE_FROM_THE_GRAVE	10459
Rise from the Grave	|4|B
Sorcery
Put target creature card from a graveyard onto the battlefield under your control. That creature is a |Sblack Zombie in addition to its other colors and types.*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL){
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			reanimate_permanent(player, card, instance->targets[0].player, selected, REANIMATE_ADD_BLACK_ZOMBIE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
}

int card_rotting_legion(int player, int card, event_t event){
/* CARD_ID_ROTTING_LEGION	11169
Rotting Legion	|4|B
Creature - Zombie 4/5
~ enters the battlefield tapped. */

	comes_into_play_tapped(player, card, event);
	return 0;
}


int card_sign_in_blood(int player, int card, event_t event){
/* CARD_ID_SIGN_IN_BLOOD	10465
Sign in Blood	|B|B
Sorcery
Target player draws two cards and loses 2 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = life[player]-2 < 6 ? 1-player : player;

	card_instance_t *this_instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(this_instance->targets[0].player, 2);
			lose_life(this_instance->targets[0].player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_stabbing_pain(int player, int card, event_t event){
/* CARD_ID_STABBING_PAIN	11175
Stabbing Pain	|B
Instant
Target creature gets -1/-1 until end of turn. Tap that creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -1, -1);
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_viscera_seer(int player, int card, event_t event){
/* CARD_ID_VISCERA_SEER	11187
Viscera Seer	|B
Creature - Vampire Wizard 1/1
Sacrifice a creature: Scry 1. (Look at the top card of your library. You may put that card on the bottom of your library.) */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		scry(player, 1);
	}
	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL);
}

/* LANDS */

int m10_lands(int player, int card, event_t event, int sub1, int sub2){

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_state(player, card, STATE_TAPPED) ){
			int tap = 1;
			int i;
			for(i=0; i<active_cards_count[player]; i++){
				if( in_play(player, i) && (has_subtype(player, i, sub1) || has_subtype(player, i, sub2)) ){
					tap = 0;
					break;
				}
			}
			if( tap == 1 ){
				get_card_instance(player, card)->state |= STATE_TAPPED;	// avoid sending event
			}
		}
	}

	return mana_producer(player, card, event);
}


int card_dragonskull_summit(int player, int card, event_t event){
/*CARD_ID_DRAGONSKULL_SUMMIT	10409
Dragonskull Summit	""
Land
~ enters the battlefield tapped unless you control |Ha Swamp or |Ha Mountain.
|T: Add |B or |R to your mana pool. */

	return m10_lands(player, card, event, SUBTYPE_SWAMP, SUBTYPE_MOUNTAIN);
}

int card_drowned_catacomb(int player, int card, event_t event){
/* CARD_ID_DROWNED_CATACOMB	10411
Drowned Catacomb	""
Land
~ enters the battlefield tapped unless you control |Han Island or |Ha Swamp.
|T: Add |U or |B to your mana pool. */

	return m10_lands(player, card, event, SUBTYPE_SWAMP, SUBTYPE_ISLAND);
}

int card_glacial_fortress(int player, int card, event_t event){
/* CARD_ID_GLACIAL_FORTRESS	10419
Glacial Fortress	""
Land
~ enters the battlefield tapped unless you control |Ha Plains or |Han Island.
|T: Add |W or |U to your mana pool. */

	return m10_lands(player, card, event, SUBTYPE_PLAINS, SUBTYPE_ISLAND);
}

int card_rootbound_crag(int player, int card, event_t event){
/* CARD_ID_ROOTBOUND_CRAG	10460
Rootbound Crag	""
Land
~ enters the battlefield tapped unless you control |Ha Mountain or |Ha Forest.
|T: Add |R or |G to your mana pool. */

	return m10_lands(player, card, event, SUBTYPE_FOREST, SUBTYPE_MOUNTAIN);
}

int card_sunpetal_grove(int player, int card, event_t event){
/* CARD_ID_SUNPETAL_GROVE	10476
Sunpetal Grove	""
Land
~ enters the battlefield tapped unless you control |Ha Forest or |Ha Plains.
|T: Add |G or |W to your mana pool. */

	return m10_lands(player, card, event, SUBTYPE_FOREST, SUBTYPE_PLAINS);
}

int card_mystifying_maze(int player, int card, event_t event){
/* CARD_ID_MYSTIFYING_MAZE	11151
Mystifying Maze	""
Land
|T: Add |C to your mana pool.
|4, |T: Exile target attacking creature an opponent controls. At the beginning of the next end step, return it to the battlefield tapped under its owner's control.*/

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;
	td.allowed_controller = 1-player;
	td.preferred_controller = 1-player;

	if(event == EVENT_ACTIVATE ){
		int choice = instance->number_of_targets = instance->info_slot = 0;
		if( !paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_X(5), 0, &td, NULL) )
		{
			choice = do_dialog(player, player, card, -1, -1, " Get mana\n Exile an Attacker\n Cancel", 0);
		}
		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		else if( choice == 1 ){
				add_state(player, card, STATE_TAPPED);
				if( charge_mana_for_activated_ability(player, card, MANACOST_X(4)) &&
					new_pick_target(&td, "Select target creature your opponent controls", 0, 1 | GS_LITERAL_PROMPT) )
				{
					ai_modifier += (is_token(instance->targets[0].player, instance->targets[0].card) ? 24 : -12);
					instance->info_slot = 1;
				}
				else{
					remove_state(player, card, STATE_TAPPED);
					spell_fizzled = 1;
				}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 && can_target(&td) ){
			remove_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_terramorphic_expanse(int player, int card, event_t event){
/* CARD_ID_TERRAMORPHIC_EXPANSE	1468
Terramorphic Expanse	""
Land
|T, Sacrifice ~: Search your library for a basic land card and put it onto the battlefield tapped. Then shuffle your library. */

	// Also code for Evolving Wilds

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( event == EVENT_ACTIVATE ){
		ai_modifier += 100;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a basic land card.");
		this_test.subtype = SUBTYPE_BASIC;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY_TAPPED, 0, AI_FIRST_FOUND, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

/* RED CARDS */
int card_ancient_hellkite(int player, int card, event_t event){
/* CARD_ID_ANCIENT_HELLKITE	11088
Ancient Hellkite	|4|R|R|R
Creature - Dragon 6/6
Flying
|R: ~ deals 1 damage to target creature defending player controls. Activate this ability only if ~ is attacking. */

	if( ! IS_GAA_EVENT(event) || ! is_attacking(player, card) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.allowed_controller = opp;
	td.preferred_controller = opp;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 1, instance->parent_controller, instance->parent_card );
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_R(1), 0,
									&td, "Select target creature your opponent controls.");
}

int card_bloodcrazed_goblin(int player, int card, event_t event){
/* CARD_ID_BLOODCRAZED_GOBLIN	11099
Bloodcrazed Goblin	|R
Creature - Goblin Berserker 2/2
~ can't attack unless an opponent has been dealt damage this turn. */

	if( event == EVENT_ATTACK_LEGALITY && affect_me(player, card) && ! is_humiliated(player, card) &&
		get_trap_condition(opp, TRAP_DAMAGE_TAKEN) <= 0 )
	{
		event_result = 1;
	}
	return 0;
}

int card_chandras_outrage(int player, int card, event_t event){
	/* Chandra's Outrage	|2|R|R
	 * Instant
	 * ~ deals 4 damage to target creature and 2 damage to that creature's controller. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
			damage_player(instance->targets[0].player, 2, player, card);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_chandras_spitfire(int player, int card, event_t event){
/* CARD_ID_CHANDRAS_SPITFIRE	11105
Chandra's Spitfire	|2|R
Creature - Elemental 1/3
Flying
Whenever an opponent is dealt noncombat damage, ~ gets +3/+0 until end of turn. */

  card_instance_t* damage = noncombat_damage_being_dealt(event);
  if (damage
	  && damage->damage_target_card == -1 && !check_special_flags(damage->damage_source_player, damage->damage_source_card, SF_ATTACKING_PWALKER)
	  && damage->damage_target_player != player)
	++get_card_instance(player, card)->info_slot;

  if (trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->info_slot <= 0 || is_humiliated(player, card) )
		return 0;

	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_MANDATORY;

	  if (event == EVENT_RESOLVE_TRIGGER)
		{
		  pump_until_eot(player, card, player, card, 3 * instance->info_slot, 0);
		  instance->info_slot = 0;
		}
	}

  return 0;
}

int card_combust(int player, int card, event_t event){
/* CARD_ID_COMBUST	11107
Combust	|1|R
Instant
~ can't be countered by spells or abilities.
~ deals 5 damage to target |Swhite or |Sblue creature. The damage can't be prevented. */

	cannot_be_countered(player, card, event);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE) | get_sleighted_color_test(player, card, COLOR_TEST_WHITE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		state_untargettable(player, card, 0);
		if( valid_target(&td) ){
			int legacy = create_legacy_effect(player, card, &my_damage_cannot_be_prevented);
			get_card_instance(player, legacy)->targets[0].player = player;
			get_card_instance(player, legacy)->targets[0].card = card;
			damage_creature(instance->targets[0].player, instance->targets[0].card, 5, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text2(player, card, "Select a %s or %s creature.", COLOR_BLUE, COLOR_WHITE), 1, NULL);
}

int card_cyclops_gladiator( int player, int card, event_t event){
/* CARD_ID_CYCLOPS_GLADIATOR	11111
Cyclops Gladiator	|1|R|R|R
Creature - Cyclops Warrior 4/4
Whenever ~ attacks, you may have it deal damage equal to its power to target creature defending player controls.
If you do, that creature deals damage equal to its power to ~. */

	if (declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), player, card)){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.preferred_controller = 1-player;

		card_instance_t* instance = get_card_instance(player, card);
		instance->number_of_targets = 0;
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS") ){
			int my_power = get_power(player, card);
			int their_power = get_power(instance->targets[0].player, instance->targets[0].card);
			damage_creature(instance->targets[0].player, instance->targets[0].card, my_power, player, card);
			damage_creature(player, card, their_power, instance->targets[0].player, instance->targets[0].card);
		}
	}
	return 0;
}

int card_destructive_force(int player, int card, event_t event){
/*
Destructive Force |5|R|R
Sorcery
Each player sacrifices five lands. Destructive Force deals 5 damage to each creature.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		int marked[2][151] = {{0}};
		APNAP(p,{
					if( can_cause_sacrifice(player, p) ){
						//Select 5 lands to sacrifice
						target_definition_t td;
						default_target_definition(player, card, &td, TYPE_LAND);
						td.illegal_abilities = 0;
						td.allowed_controller = p;
						td.preferred_controller = p;
						td.who_chooses = p;
						td.allow_cancel = 0;

						int count = 0;
						int sacced[5];
						while( count < 5 && can_target(&td) ){
								if( new_pick_target(&td, "Select a land you control.", 0, GS_LITERAL_PROMPT) ){
									marked[p][instance->targets[0].card] = 1;
									sacced[count] = instance->targets[0].card;
									state_untargettable(p, instance->targets[0].card, 1);
									count++;
								}
								instance->number_of_targets = 0;
						}
						while( count ){
								state_untargettable(p, sacced[count], 0);
								count--;
						}
					}
				};
		);

		APNAP(p,{
					int c;
					for(c=150; c > -1; c--){
						if( marked[p][c] ){
							kill_card(p, marked[p][c], KILL_SACRIFICE);
						}
					}
				};
		);

		APNAP(p, {new_damage_all(player, card, p, 5, NDA_ALL_CREATURES, NULL);};);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_earth_servant(int player, int card, event_t event){
/* CARD_ID_EARTH_SERVANT	11117
Earth Servant	|5|R
Creature - Elemental 4/4
~ gets +0/+1 for each |H2Mountain you control. */

	if( event == EVENT_TOUGHNESS && affect_me(player, card) && ! is_humiliated(player, card) ){
		event_result += count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN));
	}

	return 0;
}

int card_ember_hauler(int player, int card, event_t event){
	/* Ember Hauler	|R|R	0x200263b
	 * Creature - Goblin 2/2
	 * |1, Sacrifice ~: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION  ){
		if( valid_target(&td1) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_X(1), 0,
									&td1, "TARGET_ANY");
}

int card_fire_servant(int player, int card, event_t event){
/* CARD_ID_FIRE_SERVANT	11121
Fire Servant	|3|R|R
Creature - Elemental 4/3
If a |Sred instant or sorcery spell you control would deal damage, it deals double that damage instead. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t* damage = damage_being_dealt(event);
		if (damage && damage->damage_source_player == player &&
			(damage->initial_color & get_sleighted_color_test(player, card, COLOR_TEST_RED)) &&
			(damage->targets[3].player & TYPE_SPELL))
		{
			damage->info_slot *= 2;
		}
	}

	return 0;
}

int card_fling(int player, int card, event_t event){
	/* Fling	|1|R	0x2002654
	 * Instant
	 * As an additional cost to cast this spell, sacrifice a creature.
	 * ~ deals damage equal to the sacrificed creature's power to any target. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_SAC_CREATURE_AS_COST, &td1, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		if( pick_target(&td1, "TARGET_ANY") ){
			instance->info_slot = get_power(BYTE2(sac), BYTE3(sac));
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
		else{
			state_untargettable(BYTE2(sac), BYTE3(sac), 1);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			damage_target0(player, card, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_goblin_chieftain(int player, int card, event_t event){
/* CARD_ID_GOBLIN_CHIEFTAIN	10422
Goblin Chieftain	|1|R|R
Creature - Goblin 2/2
Haste (This creature can attack and |T as soon as it comes under your control.)
Other Goblin creatures you control get +1/+1 and have haste. */

  haste(player, card, event);

  // Other Goblin creatures you control get +1/+1 and have haste.
  boost_subtype(player, card, event, SUBTYPE_GOBLIN, 1,1, 0,SP_KEYWORD_HASTE, BCT_CONTROLLER_ONLY);

  return 0;
}

int hoarding_dragon_legacy(int player, int card, event_t event){

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
							"Return exiled artifacts", 1, 5,
							"Decline", 1, 1);
		if( choice == 1 ){
			card_instance_t *instance = get_card_instance(player, card);
			int leg = 0;
			int idx = 0;
			int* loc;
			while( (loc = exiledby_find_any(player-2, instance->targets[0].card, &leg, &idx)) != NULL ){
					int owner = (*loc & 0x80000000) ? 1 : 0;
					int iid = *loc & ~0x80000000;
					*loc = -1;
					add_card_to_hand(owner, iid);
			}
		}
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_hoarding_dragon(int player, int card, event_t event){
/* CARD_ID_HOARDING_DRAGON	11130
Hoarding Dragon	|3|R|R
Creature - Dragon 4/4
Flying
When ~ enters the battlefield, you may search your library for an artifact card, exile it, then shuffle your library.
When ~ dies, you may put the exiled card into its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an artifact card.");
		int selected = new_select_a_card(player, player, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_MAX_VALUE, &this_test);
		if( selected > -1 ){
			int leg, idx;
			exiledby_remember(player, card, player, deck_ptr[player][selected], &leg, &idx);
			rfg_card_in_deck(player, selected);
			get_card_instance(player, card)->info_slot++;
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(player, card) ){
		if( is_this_dying(player, card) && get_card_instance(player, card)->info_slot ){
			int legacy = create_legacy_effect(player, card, &hoarding_dragon_legacy);
			card_instance_t *instance = get_card_instance(player, legacy);
			instance->targets[0].card = exiledby_detach(player, card);
			instance->targets[11].player = 1;
		}
	}

	return 0;
}

static int effect_incite(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( event == EVENT_SET_COLOR && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			event_result |= COLOR_TEST_RED;
		}
		attack_if_able(instance->damage_target_player, instance->damage_target_card, event);
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_incite(int player, int card, event_t event){
	/* Incite	|R
	 * Instant
	 * Target creature becomes |Sred until end of turn and attacks this turn if able. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn != player ? 48 : -48);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &effect_incite, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}


int card_inferno_titan(int player, int card, event_t event){
	/* Inferno Titan	|4|R|R	0x20026a4
	 * Creature - Giant 6/6
	 * |R: ~ gets +1/+0 until end of turn.
	 * Whenever ~ enters the battlefield or attacks, it deals 3 damage divided as you choose among one, two, or three targets. */
	store_attackers(player, card, event, DAT_STORE_IN_INFO_SLOT, player, card, NULL);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY  || resolve_declare_attackers_trigger(player, card, event, DAT_STORE_IN_INFO_SLOT) ){
		target_and_divide_damage(player, card, NULL, NULL, 3);
	}

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1, 0);
}

int card_leyline_of_punishment(int player, int card, event_t event){
/* CARD_ID_LEYLINE_OF_PUNISHMENT	11140
Leyline of Punishment	|2|R|R
Enchantment
If ~ is in your opening hand, you may begin the game with it on the battlefield.
Players can't gain life.
Damage can't be prevented. */

	// tentative for 'damage_cannot_be_prevented'
	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *source = get_card_instance(affected_card_controller, affected_card);
		if( damage_card == source->internal_card_id ){
			state_untargettable(affected_card_controller, affected_card, 1);
		}
	}

	if( in_play(player, card) && event == EVENT_STATIC_EFFECTS && affect_me(player, card) && ! is_humiliated(player, card) )
	{
		player_bits[0] |= PB_CANT_GAIN_LIFE;
		player_bits[1] |= PB_CANT_GAIN_LIFE;
	}

	return global_enchantment(player, card, event);
}

int card_magma_phoenix(int player, int card, event_t event){
	/* Magma Phoenix	|3|R|R
	 * Creature - Phoenix 3/3
	 * Flying
	 * When ~ dies, it deals 3 damage to each creature and each player.
	 * |3|R|R: Return ~ from your graveyard to your hand. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		APNAP(p, {new_damage_all(player, card, p, 3, NDA_ALL_CREATURES | NDA_PLAYER_TOO, &this_test);};);
	}
	if( event == EVENT_GRAVEYARD_ABILITY && has_mana_multi(player, MANACOST_XR(3, 2)) ){
		return GA_RETURN_TO_HAND;
	}
	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana_multi(player, MANACOST_XR(3, 2));
		if( spell_fizzled != 1 ){
			return GAPAID_REMOVE;
		}
	}
	return 0;
}

int card_pyretic_ritual(int player, int card, event_t event){
/* CARD_ID_PYRETIC_RITUAL	11163
Pyretic Ritual	|1|R
Instant
Add |R|R|R to your mana pool. */

	if( event == EVENT_RESOLVE_SPELL ){
		produce_mana(player, COLOR_RED, 3);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_thunder_strike(int player, int card, event_t event){
/* CARD_ID_THUNDER_STRIKE	11183
Thunder Strike	|1|R
Instant
Target creature gets +2/+0 and gains first strike until end of turn. (It deals combat damage before creatures without first strike.) */

	return vanilla_instant_pump(player, card, event, player, player, 2, 0, KEYWORD_FIRST_STRIKE, 0);
}

int card_volcanic_strength(int player, int card, event_t event){
/* CARD_ID_VOLCANIC_STRENGTH	11188
Volcanic Strength	|1|R
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2 and has |H2mountainwalk. (It can't be blocked as long as defending player controls |Ha Mountain.) */

	return generic_aura(player, card, event, player, 2, 2, get_hacked_walk(player, card, KEYWORD_MOUNTAINWALK), 0, 0, 0, 0);
}

int card_wild_evocation(int player, int card, event_t event){
/* CARD_ID_WILD_EVOCATION	11193
Wild Evocation	|5|R
Enchantment
At the beginning of each player's upkeep, that player reveals a card at random from his or her hand.
If it's a land card, the player puts it onto the battlefield. Otherwise, the player casts it without paying its mana cost if able. */

	upkeep_trigger_ability(player, card, event, current_turn);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int p = current_turn;
		int rnd = get_random_card_in_hand(p);
			if( rnd != -1 ){
						reveal_card(player, card, p, rnd);
						if( is_what(p, rnd, TYPE_LAND) ){
							put_into_play(p, rnd);
						}
						else{
							play_card_in_hand_for_free(p, rnd);
						}
		    }
	}

	return global_enchantment(player, card, event);
}

/* ARTIFACTS */
int card_brittle_effigy(int player, int card, event_t event){
/* CARD_ID_BRITTLE_EFFIGY	11101
Brittle Effigy	|1
Artifact
|4, |T, Exile ~: Exile target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_RFG_ME, MANACOST_X(4), 0, &td, "TARGET_CREATURE");
}

static void ai_modifier_for_scy(int player, int card){

	int total_lands = count_subtype(player, TYPE_LAND, -1);

	test_definition_t test;
	default_test_definition(&test, TYPE_LAND);
	test.zone = TARGET_ZONE_HAND;
	total_lands += check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test);

	if( total_lands <= 3 || total_lands >= 6 ){
		ai_modifier += 48;
	}
}

int card_crystal_ball(int player, int card, event_t event){
/* CARD_ID_CRYSTAL_BALL	11109
Crystal Ball	|3
Artifact
|1, |T: Scry 2. (Look at the top two cards of your library, then put any number of them on the bottom of your library and the rest on top in any order.)*/

	if( event == EVENT_ACTIVATE ){
		ai_modifier_for_scy(player, card);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		scry(player, 2);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL);
}

int card_elixir_of_immortality(int player, int card, event_t event)
{
  /* Elixir of Immortality	|1
   * Artifact
   * |2, |T: You gain 5 life. Shuffle ~ and your graveyard into their owner's library. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  gain_life(player, 5);

	  if (get_owner(instance->parent_controller, instance->parent_card) != player)
		shuffle_into_library(instance->parent_controller, instance->parent_card);
	  else
		put_on_top_of_deck(instance->parent_controller, instance->parent_card);

	  reshuffle_grave_into_deck(player, 0);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL);
}

int effect_lose_defender_gain_flying_until_eot(int player, int card, event_t event)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (event == EVENT_ABILITIES && affect_me(instance->damage_target_player, instance->damage_target_card))
	{
	  event_result |= KEYWORD_FLYING;
	  event_result &= ~KEYWORD_DEFENDER;
	}

  if ((event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY) && affect_me(player, card))
	kill_card(player, card, KILL_EXILE);

  return 0;
}

int card_gargoyle_sentinel(int player, int card, event_t event)
{
/* CARD_ID_GARGOYLE_SENTINEL	11124
Gargoyle Sentinel	|3
Artifact Creature - Gargoyle 3/3
Defender (This creature can't attack.)
|3: Until end of turn, ~ loses defender and gains flying. */

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS && ! is_sick(player, card) ? 48 : -48);
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t *instance = get_card_instance(player, card);
	  create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, effect_lose_defender_gain_flying_until_eot,
									 instance->parent_controller, instance->parent_card);
	}

  return generic_activated_ability(player, card, event, 0, MANACOST_X(3), 0, NULL, NULL);
}

int card_jinxed_idol(int player, int card, event_t event){
/* CARD_ID_JINXED_IDOL	3181
Jinxed Idol	|2
Artifact
At the beginning of your upkeep, ~ deals 2 damage to you.
Sacrifice a creature: Target opponent gains control of ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY || event == EVENT_SHOULD_AI_PLAY ){
		damage_player(player, 2, player, card);
	}

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1) * 12);
	}

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) && player == instance->parent_controller){
		give_control_of_self(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE | GAA_CAN_ONLY_TARGET_OPPONENT, MANACOST0, 0,
									&td, "TARGET_PLAYER");
}

int card_platinum_angel(int player, int card, event_t event){
	// 004C48A0
/* CARD_ID_PLATINUM_ANGEL	1308
Platinum Angel	|7
Artifact Creature - Angel 4/4
Flying
You can't lose the game and your opponents can't win the game. */

	if( (event == EVENT_ATTACK_RATING || event == EVENT_BLOCK_RATING) && affect_me(player, card)){
		if (event == EVENT_ATTACK_RATING){
			ai_defensive_modifier += 128 / (landsofcolor_controlled[player][COLOR_ANY] + 2);
		}
		else {	// EVENT_BLOCK_RATING
			ai_defensive_modifier -= 128 / (landsofcolor_controlled[player][COLOR_ANY] + 2);
		}
	}

	cannot_lose_the_game(player, card, event, player);

	return 0;
}

int card_sorcerers_strongbox(int player, int card, event_t event){
/* CARD_ID_SORCERERS_STRONGBOX	11173
Sorcerer's Strongbox	|4
Artifact
|2, |T: Flip a coin. If you win the flip, sacrifice ~ and draw three cards. */

	if( event == EVENT_ACTIVATE ){
		ai_modifier += ((count_subtype(player, TYPE_LAND, -1) * 8)-40);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		int flip = flip_a_coin(instance->parent_controller, instance->parent_card);
		if( flip == 1 ){
			kill_card(instance->parent_controller, instance->parent_card, KILL_SACRIFICE);
			draw_cards(player, 3);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL);
}

int card_steel_overseer(int player, int card, event_t event){
/* CARD_ID_STEEL_OVERSEER	11176
Steel Overseer	|2
Artifact Creature - Construct 1/1
|T: Put a +1/+1 counter on each artifact creature you control. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int count = active_cards_count[player]-1;
		while( count > -1 ){
				if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) && is_what(player, count, TYPE_ARTIFACT) ){
					add_1_1_counter(player, count);
				}
				count--;
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_sword_of_vengeance(int player, int card, event_t event){
/* CARD_ID_SWORD_OF_VENGEANCE	11180
Sword of Vengeance	|3
Artifact - Equipment
Equipped creature gets +2/+0 and has first strike, vigilance, trample, and haste.
Equip |3 */

	return vanilla_equipment(player, card, event, 3, 2, 0,  KEYWORD_TRAMPLE | KEYWORD_FIRST_STRIKE, SP_KEYWORD_VIGILANCE | SP_KEYWORD_HASTE);
}

int card_temple_bell(int player, int card, event_t event){
/* CARD_ID_TEMPLE_BELL	11182
Temple Bell	|3
Artifact
|T: Each player draws a card. */

	if( event == EVENT_ACTIVATE ){
		ai_modifier += ((hand_count[1-player]-hand_count[player])*8);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_a_card(player);
		draw_a_card(1-player);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_warlords_axe(int player, int card, event_t event){
/* CARD_ID_WARLORDS_AXE	11191
Warlord's Axe	|3
Artifact - Equipment
Equipped creature gets +3/+1.
Equip |4 (|4: Attach to target creature you control. Equip only as a sorcery.) */

	return vanilla_equipment(player, card, event, 4, 3, 1, 0, 0);
}

int card_whispersilk_cloak(int player, int card, event_t event){
/* CARD_ID_WHISPERSILK_CLOAK	6996
Whispersilk Cloak	|3
Artifact - Equipment
Equipped creature can't be blocked and has shroud. (It can't be the target of spells or abilities.)
Equip |2 */

	return vanilla_equipment(player, card, event, 2, 0, 0, KEYWORD_SHROUD, SP_KEYWORD_UNBLOCKABLE);
}

/* GREEN CARDS */
int card_acidic_slime(int player, int card, event_t event){
/* CARD_ID_ACIDIC_SLIME	10379
Acidic Slime	|3|G|G
Creature - Ooze 2/2
Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.)
When ~ enters the battlefield, destroy target artifact, enchantment, or land. */

	deathtouch(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.required_type = TYPE_ARTIFACT | TYPE_ENCHANTMENT | TYPE_LAND;
		td.allowed_controller = 2;
		td.preferred_controller = 1-player;
		td.allow_cancel = 0;

		if( can_target(&td) && new_pick_target(&td, "Select target artifact, enchantment or land.", 0, GS_LITERAL_PROMPT) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_brindle_boar(int player, int card, event_t event){
/* CARD_ID_BRINDLE_BOAR	11100
Brindle Boar	|2|G
Creature - Boar 2/2
Sacrifice ~: You gain 4 life. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 4);
	}
	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_cultivate(int player, int card, event_t event){
/* CARD_ID_CULTIVATE	11110
Cultivate	|2|G
Sorcery
Search your library for up to two basic land cards, reveal those cards, and put one onto the battlefield tapped and the other into your hand.
Then shuffle your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		cultivate(player);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_dryads_favor(int player, int card, event_t event){
/* CARD_ID_DRYADS_FAVOR	11116
Dryad's Favor	|G
Enchantment - Aura
Enchant creature
Enchanted creature has |H2forestwalk. (It can't be blocked as long as defending player controls |Ha Forest.) */

	return generic_aura(player, card, event, player, 2, 2, get_hacked_walk(player, card, KEYWORD_FORESTWALK), 0, 0, 0, 0);
}

int card_elvish_archdruid(int player, int card, event_t event){

	/* Elvish Archdruid	|1|G|G
	 * Creature - Elf Druid 2/2
	 * Other Elf creatures you control get +1/+1.
	 * |T: Add |G to your mana pool for each Elf you control. */

	boost_subtype(player, card, event, SUBTYPE_ELF, 1,1, 0,0, BCT_CONTROLLER_ONLY);

	int count = 0;
	if (event == EVENT_COUNT_MANA || event == EVENT_ACTIVATE || (event == EVENT_CAN_ACTIVATE && player == AI)){	// Only count if it'll actually be needed
		count = count_subtype(player, TYPE_PERMANENT, SUBTYPE_ELF);
	}
	return mana_producing_creature(player, card, event, 36, COLOR_GREEN, count);
}

int card_fauna_shaman(int player, int card, event_t event){

	/* Fauna Shaman	|1|G
	 * Creature - Elf Shaman 2/2
	 * |G, |T, Discard a creature card: Search your library for a creature card, reveal it, and put it into your hand. Then shuffle your library. */

	if (!IS_ACTIVATING(event)){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
	this_test.zone = TARGET_ZONE_HAND;

	if(event == EVENT_CAN_ACTIVATE && generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_G(1), 0, NULL, NULL) &&
		check_battlefield_for_special_card(player, card, player, 0, &this_test)
	   ){
		if( player == AI ){
			if( new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_GOOD_TO_PUT_IN_GRAVE, -1, &this_test) > -1 ){
				return 1;
			}
		}
		else{
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE){
		charge_mana_for_activated_ability(player, card, MANACOST_G(1));
		if( spell_fizzled != 1 ){
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_GOOD_TO_PUT_IN_GRAVE, -1, &this_test);
			if( selected != -1 ){
				tap_card(player, card);
				discard_card(player, selected);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION){
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return 0;
}

int card_gaeas_revenge(int player, int card, event_t event){
/* CARD_ID_GAEAS_REVENGE	11123
Gaea's Revenge	|5|G|G
Creature - Elemental 8/5
~ can't be countered.
Haste
~ can't be the target of non|Sgreen spells or abilities from non|Sgreen sources. */

	cannot_be_countered(player, card, event);
	haste(player, card, event);
	
	if( specific_spell_played(player, card, event, ANYBODY, 2, TYPE_SPELL, F1_NO_CREATURE, 0, 0, COLOR_TEST_BLACK | COLOR_TEST_BLUE |
	                          COLOR_TEST_RED | COLOR_TEST_WHITE, 0, 0, 0, -1, 0) ){
		card_instance_t* instance = get_card_instance(player, card);
		if (target_me(player, card, instance->targets[1].player, instance->targets[1].card)){
			kill_card(instance->targets[1].player, instance->targets[1].card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_garruks_packleader(int player, int card, event_t event){
/* CARD_ID_GARRUKS_PACKLEADER	11126
Garruk's Packleader	|4|G
Creature - Beast 4/4
Whenever another creature with power 3 or greater enters the battlefield under your control, you may draw a card. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me( player, card ) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.power = 2;
		this_test.power_flag = F5_POWER_GREATER_THAN_VALUE;
		this_test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &this_test, player, card);
	}


	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}
	return 0;
}

int card_hornet_sting(int player, int card, event_t event){
	/* Hornet Sting	|G	0x2002686
	 * Instant
	 * ~ deals 1 damage to any target. */
	/* Scorching Spear	|R	0x2002686
	 * Sorcery
	 * ~ deals 1 damage to any target. */
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
		kill_card(player, card, KILL_DESTROY );
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_hunters_feast(int player, int card, event_t event)
{
  /* Hunters' Feast	|3|G
   * Sorcery
   * Any number of target players each gain 6 life. */

	if (event == EVENT_CAN_CAST)
		return 1;

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.preferred_controller = player;
  td.allow_cancel = 3;	// Both Done and Cancel buttons

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  instance->number_of_targets = 0;
	  while (can_target(&td))
		{
		  if (!new_pick_target(&td, "TARGET_PLAYER", -1, 0))
			{
			  if (instance->targets[instance->number_of_targets].card == -1)	// picked cancel, not done
				cancel = 1;
			  return 0;
			}

		  if (td.allowed_controller == ANYBODY)	// was first choice
			{
			  if (IS_AI(player) && instance->targets[0].player == player)
				break;	// AI never targets opponent
			  else
				td.allowed_controller = td.preferred_controller = 1 - instance->targets[0].player;
			}
		  else
			break;	// was second choice
		}
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  int i;
	  for (i = 0; i < instance->number_of_targets; ++i)
		if (validate_target(player, card, &td, i))
		  gain_life(instance->targets[i].player, 6);

	  kill_card(player, card, KILL_DESTROY );
	}

  return 0;
}

int card_leyline_of_vitality(int player, int card, event_t event){
/* CARD_ID_LEYLINE_OF_VITALITY	11142
Leyline of Vitality	|2|G|G
Enchantment
If ~ is in your opening hand, you may begin the game with it on the battlefield.
Creatures you control get +0/+1.
Whenever a creature enters the battlefield under your control, you may gain 1 life. */

	if( ! is_humiliated(player, card) ){
		if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me( player, card ) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &this_test, player, card);
		}


		if( event == EVENT_ETB_ABILITY ){
			gain_life(player, 1);
		}

		if( event == EVENT_TOUGHNESS && affected_card_controller == player ){
			event_result++;
		}
	}
	return global_enchantment(player, card, event);
}

// This is the version for Limited
int card_mitotic_slime(int player, int card, event_t event){
/* CARD_ID_MITOTIC_SLIME	11150
Mitotic Slime	|4|G
Creature - Ooze 4/4
When ~ dies, create two 2/2 |Sgreen Ooze creature tokens. They have "When this creature dies, create two 1/1 |Sgreen Ooze creature tokens." */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_OOZE, &token);
		token.qty = 2;
		token.pow = token.tou = 2;
		token.special_infos = 67;
		generate_token(&token);
	}

	return 0;
}

int card_natures_spiral(int player, int card, event_t event){
/* CARD_ID_NATURES_SPIRAL	10450
Nature's Spiral	|1|G
Sorcery
Return target permanent card from your graveyard to your hand. (A permanent card is an artifact, creature, enchantment, land, or planeswalker card.)*/

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a card to return to your hand.");

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
}

int card_overwhelming_stampede(int player, int card, event_t event){
/* CARD_ID_OVERWHELMING_STAMPEDE	11156
Overwhelming Stampede	|3|G|G
Sorcery
Until end of turn, creatures you control gain trample and get +X/+X, where X is the greatest power among creatures you control. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		int max = check_battlefield_for_special_card(player, card, player, CBFSC_GET_MAX_POW, &test);
		pump_subtype_until_eot(player, card, player, -1, max, max, KEYWORD_TRAMPLE, 0);

		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_primal_cocoon(int player, int card, event_t event){
/* CARD_ID_PRIMAL_COCOON	11161
Primal Cocoon	|G
Enchantment - Aura
Enchant creature
At the beginning of your upkeep, put a +1/+1 counter on enchanted creature.
When enchanted creature attacks or blocks, sacrifice ~. */

	card_instance_t* instance = in_play(player, card);
	if (instance && instance->damage_target_player >= 0){
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		upkeep_trigger_ability(player, card, event, player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			add_1_1_counters(t_player, t_card, 1);
		}

		if (declare_attackers_trigger(player, card, event, 0, t_player, t_card) || blocking(t_player, t_card, event)){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	// Enchant creature
	return vanilla_aura(player, card, event, player);
}

int card_primeval_titan(int player, int card, event_t event)
{
/* CARD_ID_PRIMEVAL_TITAN	11162
Primeval Titan	|4|G|G
Creature - Giant 6/6
Trample
Whenever ~ enters the battlefield or attacks, you may search your library for up to two land cards, put them onto the battlefield tapped, then shuffle your library.*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY || declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to put into play.");
		test.qty = 2;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
	}

	return 0;
}

int card_protean_hydra(int player, int card, event_t event){

	/* Protean Hydra	|X|G
	 * Creature - Hydra 0/0
	 * ~ enters the battlefield with X +1/+1 counters on it.
	 * If damage would be dealt to ~, prevent that damage and remove that many +1/+1 counters from it.
	 * Whenever a +1/+1 counter is removed from ~, put two +1/+1 counters on it at the beginning of the next end step. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, get_card_instance(player, card)->info_slot);

	if( event == EVENT_DEAL_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *instance= get_card_instance(player, card);
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == card && damage->damage_target_player == player && damage->info_slot > 0 ){
				int counters = count_counters(player, card, COUNTER_P1_P1);
				int prevented = prevent_damage(affected_card_controller, affected_card, counters);
				instance->eot_toughness += prevented;
				remove_1_1_counters(player, card, prevented);
			}
		}
	}

	if( get_card_instance(player, card)->eot_toughness > 0 && eot_trigger(player, card, event) ){
		card_instance_t *instance= get_card_instance(player, card);
		add_1_1_counters(player, card, 2 * instance->eot_toughness);
		instance->eot_toughness = 0;
	}

	return 0;
}

// BLUE CARDS

int card_air_servant(int player, int card, event_t event){
/* CARD_ID_AIR_SERVANT	11085
Air Servant	|4|U
Creature - Elemental 4/3
Flying
|2|U: Tap target creature with flying. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XU(2, 1), 0,
									&td, "Select target creature with flying.");
}

int card_augury_owl(int player, int card, event_t event){
/* CARD_ID_AUGURY_OWL	11094
Augury Owl	|1|U
Creature - Bird 1/1
Flying
When ~ enters the battlefield, scry 3.
(Look at the top three cards of your library, then put any number of them on the bottom of your library and the rest on top in any order.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		scry(player, 3);
	}
	return 0;
}

int card_call_to_mind(int player, int card, event_t event){
/* CARD_ID_CALL_TO_MIND	11102
Call to Mind	|2|U
Sorcery
Return target instant or sorcery card from your graveyard to your hand. */

	// Used externally
	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_SPELL, "Select a instant or sorcery spell.");

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
}

int card_conundrum_sphinx(int player, int card, event_t event){
/* CARD_ID_CONUNDRUM_SPHINX	11108
Conundrum Sphinx	|2|U|U
Creature - Sphinx 4/4
Flying
Whenever ~ attacks, each player chooses a card name. Then each player reveals the top card of his or her library.
If the card a player revealed has the name he or she chose, that player puts it into his or her hand.
If it doesn't, that player puts it on the bottom of his or her library. */

	if( ! is_unlocked(player, card, event, 8) ){ return 0; }

	/* Whenever ~ attacks, each player names a card. Then each player reveals the top card of his or her library. If the card a player revealed is the card he
	 * or she named, that player puts it into his or her hand. If it's not, that player puts it on the bottom of his or her library. */
	if (declare_attackers_trigger(player, card, event, 0, player, card)){
		// each player names a card
		int i;
		for(i=0;i<2;i++){
			int p = player;
			int card_id = -1;
			if(i == 1 ){ p = opp; };
			int *deck = deck_ptr[p];
			if( p == HUMAN ){
				if( ai_is_speculating != 1 ){
					card_id = choose_a_card("Conundrum Sphinx: Name a card", -1, -1);
				}
			}
			else{
				int r = life[AI]-5;
				if( life[AI] > 20 ){
					r = 15;
				}
				else if( life[AI] < 6 ){
					r = 1;
				}
				card_id = deck[ internal_rand(r) ];
			}

			// show what was revealed
			int top_card = add_card_to_hand(p, deck[0]);
			char buffer[100];
			if( p == HUMAN ){
				snprintf(buffer, 100, "You reveal..." );
			}
			else{
				card_data_t* card_d = &cards_data[ card_id ];
				card_ptr_t* c1 = cards_ptr[ card_d->id ];
				snprintf(buffer, 100, "AI guesses %s and reveals...", c1->full_name );
			}

			do_dialog(HUMAN, player, card, p, top_card, buffer, 0);

			if( deck[0] == card_id ){
				obliterate_top_card_of_deck(p);
			}
			else{
				obliterate_card(p, top_card);
				put_top_card_of_deck_to_bottom(p);
			}
		}
	}
	return 0;
}

int card_diminish(int player, int card, event_t event){
/* CARD_ID_DIMINISH	11115
Diminish	|U
Instant
Target creature has base power and toughness 1/1 until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if(event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance= get_card_instance(player, card);
		set_pt_and_abilities_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1, 1, 0, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_flashfreeze(int player, int card, event_t event){
/* CARD_ID_FLASHFREEZE	8364
Flashfreeze	|1|U
Instant
Counter target |Sred or |Sgreen spell. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_RED) | get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

	return counterspell(player, card, event, &td, 0);
}

int effect_frost_titan(int player, int card, int t_player, int t_card){
	if( in_play(t_player, t_card) ){
		return does_not_untap_effect(player, card, t_player, t_card, EDNT_TAP_TARGET, 1);
	} else {
		return -1;
	}
}

int card_frost_titan(int player, int card, event_t event)
{
  /* Frost Titan	|4|U|U
   * Creature - Giant 6/6
   * Whenever ~ becomes the target of a spell or ability an opponent controls, counter that spell or ability unless its controller pays |2.
   * Whenever ~ enters the battlefield or attacks, tap target permanent. It doesn't untap during its controller's next untap step. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || declare_attackers_trigger(player, card, event, 0, player, card) )
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
			effect_frost_titan(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	const target_t* targeter;
	if (!is_humiliated(player, card)
		&& (targeter = becomes_target_of_spell(player, card, event, player, card, 1-player, RESOLVE_TRIGGER_MANDATORY)))
	{
		int p = targeter->player, c = targeter->card;
		if (has_mana(p, COLOR_ANY, 2)
			&& do_dialog(p, player, card, p, c, " Pay 2\n Decline", 0) == 0)
		{
			ldoubleclicked = 0;
			charge_mana(p, COLOR_ANY, 2);
			if (spell_fizzled != 1)
				return 0;
		}
		real_counter_a_spell(player, card, p, c);
	}

	return 0;
}

int card_harbor_serpent(int player, int card, event_t event){
/* CARD_ID_HARBOR_SERPENT	11129
Harbor Serpent	|4|U|U
Creature - Serpent 5/5
|H2Islandwalk (This creature can't be blocked as long as defending player controls |Han Island.)
~ can't attack unless there are five or more |H1Islands on the battlefield. */

  if (event == EVENT_ATTACK_LEGALITY && affect_me(player, card) && ! is_humiliated(player, card) )
	{
	  int island = get_hacked_color(player, card, COLOR_BLUE);
	  if (basiclandtypes_controlled[0][island] + basiclandtypes_controlled[1][island] < 5)
		event_result = 1;
	}

  return 0;
}

int card_ice_cage(int player, int card, event_t event){
/* CARD_ID_ICE_CAGE	10432
Ice Cage	|1|U
Enchantment - Aura
Enchant creature
Enchanted creature can't attack or block, and its activated abilities can't be activated.
When enchanted creature becomes the target of a spell or ability, destroy ~. */

  if (kill_attachment_when_creature_is_targeted(player, card, event, KILL_DESTROY))
	return 0;

  return card_arrest(player, card, event);
}

int card_jaces_erasure(int player, int card, event_t event){
	/* Jace's Erasure	|1|U
	 * Enchantment
	 * Whenever you draw a card, you may have target player put the top card of his or her library into his or her graveyard. */

	if( card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_DUH) ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			instance->number_of_targets = 1;
			mill(instance->targets[0].player, 1);
		}
		instance->number_of_targets = 0;
	}
	return global_enchantment(player, card, event);
}

int card_mass_polymorph(int player, int card, event_t event){
/*
Mass Polymorph |5|U
Sorcery
Exile all creatures you control, then reveal cards from the top of your library until you reveal that many creature cards.
Put all creature cards revealed this way onto the battlefield, then shuffle the rest of the revealed cards into your library.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		int result = new_manipulate_all(player, card, player, &test, KILL_EXILE);

		test.qty = result;
		reveal_card_until_you_reveal_test(player, player, &test, RCUYRT_PUT_TEST_INTO_PLAY | RCUYRT_SHUFFLE_REST);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_merfolk_sovereign(int player, int card, event_t event){
/* CARD_ID_MERFOLK_SOVEREIGN	10445
Merfolk Sovereign	|1|U|U
Creature - Merfolk 2/2
Other Merfolk creatures you control get +1/+1.
|T: Target Merfolk creature can't be blocked this turn. */

	boost_creature_type(player, card, event, SUBTYPE_MERFOLK, 1, 1, 0, BCT_CONTROLLER_ONLY);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_MERFOLK;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (target_available(player, card, &td) > 1 && current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS ?
						48 : -48);
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target Merfolk creature.");
}

static void merfolk_spy_ability(int player, int card, int t_player){
	if( hand_count[t_player]==0 ){
		return;
	}
	int r =  internal_rand( hand_count[t_player] );
	int cards_array[ 1 ];
	int i=0;
	int hand_index = 0;
	for(i=0;i<active_cards_count[t_player]; i++){
		card_instance_t *instance = get_card_instance(t_player, i);
		if( ! ( instance->state & STATE_INVISIBLE ) && ! ( instance->state & STATE_IN_PLAY )  ){
			int id = instance->internal_card_id;
			if( id > -1 ){
				if( hand_index == r ){
					cards_array[0] = id;
				}
				hand_index++;
			}
		}
	}
	show_deck(player, cards_array, 1, "Card Revealed at Random", 0, 0x7375B0 );
}

int card_merfolk_spy(int player, int card, event_t event){
/*
Merfolk Spy |U
Creature — Merfolk Rogue 1/1
Islandwalk (This creature can't be blocked as long as defending player controls an Island.)
Whenever Merfolk Spy deals combat damage to a player, that player reveals a card at random from his or her hand.
*/
	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					while( times_damaged[p] ){
							merfolk_spy_ability(player, card, p);
							times_damaged[p]--;
					}
				};
		);
	}

	return 0;

}

int card_phantom_beast(int player, int card, event_t event){
/* CARD_ID_PHANTOM_BEAST	11157
Phantom Beast	|3|U
Creature - Illusion Beast 4/5
When ~ becomes the target of a spell or ability, sacrifice it. */

	return card_skulking_ghost(player, card, event);
}

int card_preordain(int player, int card, event_t event){
/* CARD_ID_PREORDAIN	11160
Preordain	|U
Sorcery
Scry 2, then draw a card.
(To scry 2, look at the top two cards of your library, then put any number of them on the bottom of your library and the rest on top in any order.) */

	if( event == EVENT_RESOLVE_SPELL ){
		scry(player, 2);
		draw_a_card(player);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_sleep(int player, int card, event_t event){
/* CARD_ID_SLEEP	10470
Sleep	|2|U|U
Sorcery
Tap all creatures target player controls. Those creatures don't untap during that player's next untap step. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1)*12);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int p = instance->targets[0].player;
			int count = active_cards_count[p] -1;
			while(count >= 0 ){
					if( in_play(p, count) && is_what(p, count, TYPE_CREATURE) ){
						effect_frost_titan(player, card, p, count);
					}
					count--;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_stormtide_leviathan(int player, int card, event_t event){
/* CARD_ID_STORMTIDE_LEVIATHAN	11178
Stormtide Leviathan	|5|U|U|U
Creature - Leviathan 8/8
|H2Islandwalk (This creature can't be blocked as long as defending player controls |Han Island.)
All lands are |H1Islands in addition to their other types.
Creatures without flying or |H2islandwalk can't attack. */

	if( event == EVENT_ATTACK_LEGALITY && ! is_humiliated(player, card) ){
		int abilities = get_abilities(affected_card_controller, affected_card, EVENT_ABILITIES, -1);
		if (!(abilities & KEYWORD_FLYING) && !(abilities & get_hacked_walk(player, card, KEYWORD_ISLANDWALK))){
			event_result = 1;
		}
	}

	return all_lands_are_basiclandtype(player, card, event, 2, COLOR_BLUE, SUBTYPE_ISLAND);
}

int card_tome_scour(int player, int card, event_t event){
/* CARD_ID_TOME_SCOUR	10477
Tome Scour	|U
Sorcery
Target player puts the top five cards of his or her library into his or her graveyard. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			mill(instance->targets[0].player, 5);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_time_reversal(int player, int card, event_t event)
{
  /* Time Reversal	|3|U|U
   * Sorcery
   * Each player shuffles his or her hand and graveyard into his or her library, then draws seven cards. Exile ~. */

  int rval = resolve_timetwister(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
	kill_card(player, card, KILL_EXILE);

  return rval;
}

int card_wall_of_frost(int player, int card, event_t event){
/* CARD_ID_WALL_OF_FROST	10485
Wall of Frost	|1|U|U
Creature - Wall 0/7
Defender
Whenever ~ blocks a creature, that creature doesn't untap during its controller's next untap step. */

	if( event == EVENT_DECLARE_BLOCKERS && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->blocking < 255 ){
			does_not_untap_effect(player, card, opp, instance->blocking, 0, 1);
		}
	}
	return 0;
}

int card_water_servant(int player, int card, event_t event){
/* CARD_ID_WATER_SERVANT	11192
Water Servant	|2|U|U
Creature - Elemental 3/4
|U: ~ gets +1/-1 until end of turn.
|U: ~ gets -1/+1 until end of turn. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( current_turn == player ){
			return generic_shade_merge_pt(player, card, event, 0, MANACOST_U(1), 1, -1);
		}
		else{
			return generic_shade_merge_pt(player, card, event, 0, MANACOST_U(1), -1, 1);
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_U(1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		card_instance_t *instance = get_card_instance(player, card);
		instance->info_slot = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_U(1)) ){
			int choice = do_dialog(player, player, card, -1, -1, " +1/-1\n -1/+1\n Cancel", current_turn == player ? 0 : 1);
			if( choice == 2 ){
				spell_fizzled = 1;
			}
			else{
				instance->info_slot = choice;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->info_slot == 0 ){
			return generic_shade_merge_pt(player, card, event, 0, MANACOST_U(1), 1, -1);
		}
		else{
			return generic_shade_merge_pt(player, card, event, 0, MANACOST_U(1), -1, 1);
		}
	}
	return 0;
}

int card_dark_tutelage(int player, int card, event_t event){
/* CARD_ID_DARK_TUTELAGE	11112
Dark Tutelage	|2|B
Enchantment
At the beginning of your upkeep, reveal the top card of your library and put that card into your hand. You lose life equal to its converted mana cost. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int *deck = deck_ptr[player];
		int card_added = add_card_to_hand(player, deck[0] );
		remove_card_from_deck( player, 0 );
		int cmc = get_cmc(player, card_added);
		lose_life(player, cmc);
		reveal_card(player, card, player, card_added);
	}

	return global_enchantment(player, card, event);
}


