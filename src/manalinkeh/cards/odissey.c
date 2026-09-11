#include "manalink.h"

// Functions
static void goyf(int player, int card, event_t event, type_t type){

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && player != -1 && affect_me(player, card) && ! is_humiliated(player, card) ){
		int amount = count_graveyard_by_type(0, type) + count_graveyard_by_type(1, type);
		event_result+=amount;
	}
}

int can_pay_flashback(int player, int iid, event_t event, int colorless, int black, int blue, int green, int red, int white){
	int cless = get_updated_casting_cost(player, -1, iid, event, colorless);
	cless-=(2*count_cards_by_id(player, CARD_ID_CATALYST_STONE));
	cless+=(2*count_cards_by_id(1-player, CARD_ID_CATALYST_STONE));
	if( cless < 0 ){
		cless = 0;
	}
	if( has_mana_multi(player, cless, black, blue, green, red, white) ){
		return 1;
	}
	return 0;
}

int pay_flashback(int player, int iid, event_t event, int colorless, int black, int blue, int green, int red, int white){
	int orig_colorless = colorless;
	int cless = get_updated_casting_cost(player, -1, iid, event, colorless);
	cless-=(2*count_cards_by_id(player, CARD_ID_CATALYST_STONE));
	cless+=(2*count_cards_by_id(1-player, CARD_ID_CATALYST_STONE));
	if( cless < 0 ){
		cless = 0;
	}
	charge_mana_multi(player, cless, black, blue, green, red, white);
	if( orig_colorless == -1 ){
		charge_mana(player, COLOR_COLORLESS, -1);
	}
	if( spell_fizzled != 1 ){
		return GAPAID_EXILE;
	}
	return 0;
}

int do_flashback(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white){
	card_instance_t *instance = get_card_instance(player, card);
	if( event == EVENT_GRAVEYARD_ABILITY &&
		can_pay_flashback(player, instance->internal_card_id, event, colorless, black, blue, green, red, white) )
	{
		if( can_legally_play_iid_now(player, get_card_instance(player, card)->internal_card_id, event) ){
			return GA_PLAYABLE_FROM_GRAVE;
		}
	}
	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		return pay_flashback(player, instance->internal_card_id, event, colorless, black, blue, green, red, white);
	}
	return 0;
}

static int od_land(int player, int card, event_t event, int color1, int color2){

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( event == EVENT_CAN_ACTIVATE && ! is_tapped(player, card) && has_mana(player, COLOR_COLORLESS, 1) &&
		get_card_instance(player, card)->targets[1].player != 66
	  ){
		return can_produce_mana(player, card);
	}

	if(event == EVENT_ACTIVATE ){
		get_card_instance(player, card)->targets[1].player = 66;
		charge_mana(player, COLOR_COLORLESS, 1);
		if( spell_fizzled != 1 ){
			produce_mana_tapped2(player, card, color1, 1, color2, 1);
		}
		get_card_instance(player, card)->targets[1].player = 0;
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( ! is_tapped(player, card) && has_mana(player, COLOR_COLORLESS, 1) && get_card_instance(player, card)->targets[1].player != 66 &&
			can_produce_mana(player, card) )
		{
			declare_mana_available(player, color1, 1);
			declare_mana_available(player, color2, 1);
		}
	}

	return 0;
}

static int od_egg(int player, int card, event_t event, int color1, int color2){

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(4), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		ai_modifier += 48;
		add_state(player, card, STATE_TAPPED);
		generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
		if( spell_fizzled == 1 ){
			remove_state(player, card, STATE_TAPPED);
			return 0;
		}
		produce_mana_tapped2(player, card, color1, 1, color2, 1);
		kill_card(player, card, KILL_SACRIFICE);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return 0;
}

static int od_shrine(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		int csvid = get_card_name(trigger_cause_controller, trigger_cause);
		int amount = count_graveyard_by_id(ANYBODY, csvid);
		if( amount && new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, NULL) ){
			return amount;
		}
	}

	return 0;
}

static void threshold_mod_for_pt_and_abilitie(int player, int card, event_t event, int pow, int tou, int key, int s_key){
	if( in_play(player, card) && ! is_humiliated(player, card) && has_threshold(player)  ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES) && affect_me(player, card) ){
			switch( event ){
					case EVENT_POWER:
						event_result += pow;
						break;
					case EVENT_TOUGHNESS:
						event_result += tou;
						break;
					case EVENT_ABILITIES:
					{
						special_abilities(player, card, event, s_key, player, card);
						event_result |= key;
						break;
					}
					default:
						break;
			}
		}
	}
}

static int discard_multiple_cards_at_once(int player, int card, int ai_max){
	int hand_copy[2][hand_count[player]];
	int hcc = 0;

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select a card to discard.");
	this_test.id = cards_data[iid_draw_a_card].id;
	this_test.id_flag = DOESNT_MATCH;
	int amount = 0;
	while( amount < hcc ){
			int selected = select_card_from_zone(player, player, hand_copy[0], hcc, 0, AI_MIN_VALUE, -1, &this_test);
			if( selected != -1  ){
				hand_copy[0][selected] = iid_draw_a_card;
				amount++;
				if( IS_AI(player) && amount == ai_max ){
					break;
				}
			}
			else{
				break;
			}
	}
	int i;
	for(i=0; i<hcc; i++){
		if( hand_copy[0][i] == iid_draw_a_card ){
			new_discard_card(player, hand_copy[1][i], 0, player);
		}
	}
	return amount;
}

// Cards

int card_aboshan_cephalid_emperor(int player, int card, event_t event){
/*
Aboshan, Cephalid Emperor |4|U|U
Legendary Creature — Cephalid 3/3
Tap an untapped Cephalid you control: Tap target permanent.
{U}{U}{U}: Tap all creatures without flying.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);
	td1.allowed_controller = player;
	td1.preferred_controller = player;
	td1.required_subtype = SUBTYPE_CEPHALID;
	td1.illegal_abilities = 0;
	td1.illegal_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_TAP_PERMANENT = 1,
		CHOICE_TAP_ALL_NONFLYERS
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			return 1;
		}
		if( can_target(&td1) ){
			if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_U(1), 0, &td, NULL) ){
				return 1;
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_U(3), 0, NULL, NULL),
						can_target(&td1) &&
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ? 1 : 0
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Tap target permanent", abils[1], current_turn == player ? 10 : 1,
							"Tap all nonflying creatures", abils[2], current_turn != player ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_TAP_PERMANENT ){
			if( new_pick_target(&td1, "Select an untapped Cephalid you control.", 0, 1 | GS_LITERAL_PROMPT) ){
				int ceph = instance->targets[0].card;
				generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PERMANENT");
				if( spell_fizzled != 1 ){
					tap_card(player, ceph);
				}
			}
		}
		if( choice == CHOICE_TAP_ALL_NONFLYERS ){
			return generic_activated_ability(player, card, event, 0, MANACOST_U(3), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_TAP_PERMANENT && valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
		if( instance->info_slot == CHOICE_TAP_ALL_NONFLYERS ){
			test_definition_t this_test2;
			default_test_definition(&this_test2, TYPE_CREATURE);
			this_test2.keyword = KEYWORD_FLYING;
			this_test2.keyword_flag = 1;
			new_manipulate_all(player, card, 2, &this_test2, ACT_TAP);
		}
	}

	return 0;
}

static const char* instant_or_sorcery_is_damaging_me(int who_chooses, int player, int card)
{
	card_instance_t *instance = get_card_instance(player, card);
	if(	instance->internal_card_id == damage_card ){
		if( instance->targets[3].player & TYPE_SPELL ){
			if( instance->damage_target_player == who_chooses && instance->damage_target_card == -1 &&
				! damage_is_to_planeswalker(instance) )
			{
				return NULL;
			}
		}
	}
	return "must be damage from Instant or Sorcey dealt to you";
}

int card_aegis_of_honor(int player, int card, event_t event){
/*
Aegis of Honor |W
Enchantment
{1}: The next time an instant or sorcery spell would deal damage to you this turn, that spell deals that damage to its controller instead.
*/
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = (int32_t)instant_or_sorcery_is_damaging_me;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if (valid_target(&td)){
			card_instance_t *dmg = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			dmg->damage_target_player = dmg->damage_source_player;
		}
	}

	return generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_DAMAGE");
}

int card_aether_burst(int player, int card, event_t event){
	/* AEther Burst	|1|U
	 * Instant
	 * Return up to X target creatures to their owners' hands, where X is one plus the number of cards named ~ in all graveyards as you cast ~. */

	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	int max_targ = 1 + count_graveyard_by_id(ANYBODY, get_id(player, card));

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0;i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				bounce_permanent(instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", max_targ , NULL);
}

int card_ancestral_tribute(int player, int card, event_t event){
/* CARD_ID_ANCESTRAL_TRIBUTE	5467
Ancestral Tribute	|5|W|W
Sorcery
You gain 2 life for each card in your graveyard.
Flashback |9|W|W|W (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, count_graveyard(player)*2);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XW(9, 3));
}

int card_animal_boneyard(int player, int card, event_t event){
/* CARD_ID_ANIMAL_BONEYARD	5468
Animal Boneyard	|2|W
Enchantment - Aura
Enchant land
Enchanted land has "|T, Sacrifice a creature: You gain life equal to the sacrificed creature's toughness." */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		if( event == EVENT_CAN_ACTIVATE ){
			return generic_activated_ability(t_player, t_card, event, GAA_UNTAPPED | GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL);
		}

		if( event == EVENT_ACTIVATE ){
			if( life[player] < 6 ){
				ai_modifier += 48;
			}
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			if (!sac){
				cancel = 1;
				return 0;
			}
			generic_activated_ability(t_player, t_card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				instance->info_slot = get_toughness(BYTE2(sac), BYTE3(sac));
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
			else{
				state_untargettable(BYTE2(sac), BYTE3(sac), 0);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, get_card_instance(player, card)->info_slot);
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;
	td.allowed_controller = player;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target land you control.");
}

int card_atogatog(int player, int card, event_t event){
	/* Atogatog	|W|U|B|R|G
	 * Legendary Creature - Atog 5/5
	 * Sacrifice an Atog creature: ~ gets +X/+X until end of turn, where X is the sacrificed creature's power. */

	check_legend_rule(player, card, event);

	if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST){
		/* Greatly simplifying.  Assumes that the other atogs can't pump themselves first, and that if Atogatog's activated ability's cost has increased from 0,
		 * it only needs to be paid once. */

		if (!CAN_ACTIVATE0(player, card)){
			return 0;
		}

		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "");
		test.subtype = SUBTYPE_ATOG;
		test.subtype_flag = MATCH;
		test.power = 0;
		test.power_flag = F5_POWER_GREATER_THAN_VALUE;
		test.not_me = 1;

		if (!new_can_sacrifice_as_cost(player, 1, &test)){
			return 0;
		}

		return check_battlefield_for_special_card(player, card, player, CBFSC_GET_TOTAL_POW, &test);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select an Atog creature to sacrifice.");
	test.subtype = SUBTYPE_ATOG;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t* instance = get_card_instance(player, card);
		return generic_shade(player, card, event, 0, MANACOST0, instance->info_slot, instance->info_slot, 0, 0);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, SARV_REPORT_POWER);
}

int card_auramancer(int player, int card, event_t event){
/* CARD_ID_AURAMANCER	5473
Auramancer	|2|W
Creature - Human Wizard 2/2
When ~ enters the battlefield, you may return target enchantment card from your graveyard to your hand. */

	// Used externally
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ENCHANTMENT, "Select target enchantment card.");
		if( count_graveyard_by_type(player, TYPE_ENCHANTMENT) && ! graveyard_has_shroud(player) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 1, AI_MAX_VALUE, &test);
		}
	}

	return 0;
}

int card_aven_shrine(int player, int card, event_t event){
/* CARD_ID_AVEN_SHRINE	5475
Aven Shrine	|1|W|W
Enchantment
Whenever a player casts a spell, that player gains X life, where X is the number of cards in all graveyards with the same name as that spell.*/

	int result = od_shrine(player, card, event);
	if( result ){
		gain_life(trigger_cause_controller, result);
	}

	return global_enchantment(player, card, event);
}

int card_aven_fisher(int player, int card, event_t event){
	/*
	  Aven Fisher |3|U
	  Creature - Bird Soldier 2/2
	  Flying (This creature can't be blocked except by creatures with flying or reach.)
	  When Aven Fisher dies, you may draw a card.
	*/
	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_DUH) ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_aven_flock(int player, int card, event_t event){//code for Honor Guard, Capashen Templar, Kjeldoran Outrider, Moonwing Moth, Wall of Faith.
	/*
	  Aven Flock |4|W
	  Creature - Bird Soldier 2/3
	  Flying (This creature can't be blocked except by creatures with flying or reach.)
	  {W}: Aven Flock gets +0/+1 until end of turn.
	*/
	return generic_shade_merge_pt(player, card, event, 0, MANACOST_W(1), 0, 1);
}

int card_balancing_act(int player, int card, event_t event){
/* CARD_ID_BALANCING_ACT	5478
Balancing Act	|2|W|W
Sorcery
Each player chooses a number of permanents he or she controls equal to the number of permanents controlled by the player who controls the fewest,
then sacrifices the rest. Each player discards cards the same way. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int my_p = count_subtype(player, TYPE_PERMANENT, -1);
		int his_p = count_subtype(1-player, TYPE_PERMANENT, -1);
		if( my_p != his_p ){
			test_definition_t test;
			default_test_definition(&test, TYPE_PERMANENT);
			char msg[100];
			int diff = my_p > his_p ? my_p - his_p : his_p - my_p;
			test.qty = diff;
			if( diff > 1 ){
				scnprintf(msg, 100, "Select %d permanents to sacrifice.", diff);
			}
			else{
				scnprintf(msg, 100, "Select a permanent to sacrifice.");
			}
			int who_will_sacrifice = my_p > his_p ? player : 1-player;
			new_sacrifice(player, card, who_will_sacrifice, SAC_CAUSED | SAC_NO_CANCEL, &test);
		}
		my_p = hand_count[player];
		his_p = hand_count[1-player];
		if( my_p != his_p ){
			int diff = my_p > his_p ? my_p - his_p : his_p - my_p;
			int who_will_discard = my_p > his_p ? player : 1-player;
			new_multidiscard(who_will_discard, diff, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_barbarian_ring(int player, int card, event_t event){
	/* Barbarian Ring	""	0x2001443
	 * Land
	 * |T: Add |R. ~ deals 1 damage to you.
	 * Threshold - |R, |T, Sacrifice ~: It deals 2 damage to any target. Activate this ability only if seven or more cards are in your graveyard. */
	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAC_DAMAGE
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() && has_threshold(player) &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_R(2), 0, &td, NULL) )
		{
			choice = CHOICE_SAC_DAMAGE;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_SAC_DAMAGE && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Sac & damage", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA ){
			mana_producer(player, card, event);
			damage_player(player, 1, player, card);
		}
		if( choice == CHOICE_SAC_DAMAGE ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_R(1), 0,
										&td, "TARGET_ANY");
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAC_DAMAGE && valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return 0;
}

int card_bash_to_bits(int player, int card, event_t event){
	/* Bash to Bits	|3|R
	 * Instant
	 * Destroy target artifact.
	 * Flashback |4|R|R */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XR(4, 2));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE );
		}
		else{
			kill_card(player, card, KILL_DESTROY );
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
}

int card_battle_of_wits(int player, int card, event_t event)
{
  /* Battle of Wits	|3|U|U
   * Enchantment
   * At the beginning of your upkeep, if you have 200 or more cards in your library, you win the game. */

  if (trigger_condition == TRIGGER_UPKEEP && player == current_turn && affect_me(player, card)
	  && count_deck(player) >= 200 && upkeep_trigger(player, card, event))
	lose_the_game(1-player);

  if (event == EVENT_SHOULD_AI_PLAY && count_deck(player) >= 200)
	lose_the_game(1-player);

  return global_enchantment(player, card, event);
}

int card_battle_strain(int player, int card, event_t event){
/* CARD_ID_BATTLE_STRAIN	5485
Battle Strain	|1|R
Enchantment
Whenever a creature blocks, ~ deals 1 damage to that creature's controller. */

	if( event == EVENT_DECLARE_BLOCKERS && ! is_humiliated(player, card) ){
		int i;
		for(i = active_cards_count[1-current_turn]-1; i > -1; i--){
			if( in_play(1-current_turn, i) && is_what(1-current_turn, i, TYPE_CREATURE) && blocking(1-current_turn, i, event) ){
				damage_player(1-current_turn, 1, player, card);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_bearscape(int player, int card, event_t event){
	/* Bearscape	|1|G|G
	 * Enchantment
	 * |1|G, Exile two cards from your graveyard: Put a 2/2 |Sgreen Bear creature token onto the battlefield. */

	if( event == EVENT_CAN_ACTIVATE && count_graveyard(player) > 1 ){
		return generic_activated_ability(player, card, event, 0, MANACOST_XG(1, 1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, 0, MANACOST_XG(1, 1), 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			char msg[100] = "Select a card to exile.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, msg);
			if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test) != -1 ){
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 1, AI_MIN_VALUE, &this_test);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_BEAR, &token);
		token.pow = token.tou = 2;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		generate_token(&token);
	}

	return global_enchantment(player, card, event);
}

int card_beast_attack(int player, int card, event_t event){
/* CARD_ID_BEAST_ATTACK	5488
Beast Attack	|2|G|G|G
Instant
Create a 4/4 |Sgreen Beast creature token.
Flashback |2|G|G|G (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_BEAST, &token);
		token.pow = token.tou = 4;
		generate_token(&token);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XG(2, 3));
}

int card_beloved_chaplain(int player, int card, event_t event){
/* CARD_ID_BELOVED_CHAPLAIN	5489
Beloved Chaplain	|1|W
Creature - Human Cleric 1/1
Protection from creatures */

	protection_from_creatures(player, card, event);
	return 0;
}

int card_bomb_squad(int player, int card, event_t event)
{
  /* Bomb Squad	|3|R
   * Creature - Dwarf 1/1
   * |T: Put a fuse counter on target creature.
   * At the beginning of your upkeep, put a fuse counter on each creature with a fuse counter on it.
   * Whenever a creature has four or more fuse counters on it, remove all fuse counters from it and destroy it. That creature deals 4 damage to its
   * controller. */

  int p, c, counters;

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	for (p = 0; p <= 1; ++p)
	  for (c = 0; c < active_cards_count[p]; ++c)
		if (in_play(p, c) && is_what(p, c, TYPE_CREATURE) && count_counters(p, c, COUNTER_FUSE))
		  add_counter(p, c, COUNTER_FUSE);

  if (event == EVENT_STATIC_EFFECTS || event == EVENT_SHOULD_AI_PLAY)
	for (p = 0; p <= 1; ++p)
	  for (c = 0; c < active_cards_count[p]; ++c)
		if (in_play(p, c) && is_what(p, c, TYPE_CREATURE) && (counters = count_counters(p, c, COUNTER_FUSE)))
		  {
			if (counters >= 4)
			  {
				remove_all_counters(p, c, COUNTER_FUSE);
				damage_player(p, 4, p, c);
				kill_card(p, c, KILL_DESTROY);
			  }
			else if (event == EVENT_SHOULD_AI_PLAY)
			  ai_modifier += (p == AI ? -16 : 16) * counters;
		  }

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  if( event == EVENT_ACTIVATE )
	  ai_modifier += (current_turn == 1-player ? 48 : 24);

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_FUSE);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_braids_cabal_minion(int player, int card, event_t event){
/* CARD_ID_BRAIDS_CABAL_MINION	5495
Braids, Cabal Minion	|2|B|B
Legendary Creature - Human Minion 2/2
At the beginning of each player's upkeep, that player sacrifices an artifact, creature, or land. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, 2);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select an artifact, creature, or land to sacrifice.");
		new_sacrifice(player, card, current_turn, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	return 0;
}

static int burning_sands_effect(int player, int card, event_t event){

	//In case something is copying Pawn of Ulamog and then reverts to its original form.
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_BURNING_SANDS ){
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
				count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, TYPE_CREATURE, NULL, GFPC_TRACK_CONTROLLER, 0);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int creatures_dead_by_owner[2] = {BYTE0(result), BYTE1(result)};
			test_definition_t test;
			default_test_definition(&test, TYPE_LAND);
			APNAP(p,{
						if( creatures_dead_by_owner[p] ){
							test.qty = creatures_dead_by_owner[p];
							new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test);
						}
					}
			);
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

int card_burning_sands(int player, int card, event_t event)
{
  /* Burning Sands	|3|R|R	0x200d734
   * Enchantment
   * Whenever a creature dies, that creature's controller sacrifices a land. */
	// Used externally
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)burning_sands_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &burning_sands_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, TYPE_CREATURE, NULL, GFPC_TRACK_CONTROLLER, 0);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int creatures_dead_by_owner[2] = {BYTE0(result), BYTE1(result)};
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
			APNAP(p,{
						if( creatures_dead_by_owner[p] ){
							test.qty = creatures_dead_by_owner[p];
							new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test);
						}
					}
			);
		}
	}

	return global_enchantment(player, card, event);
}

int card_cabal_patriarch(int player, int card, event_t event){
/*
Cabal Patriarch |3|B|B|B
Legendary Creature — Human Wizard 5/5
{2}{B}, Sacrifice a creature: Target creature gets -2/-2 until end of turn.
{2}{B}, Exile a creature card from your graveyard: Target creature gets -2/-2 until end of turn.
*/
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XB(2, 1), 0, &td, NULL) ){
			if( can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE) || count_graveyard_by_type(player, TYPE_CREATURE) ){
				return 1;
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Sac a creature", can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE), 1,
							"Exile a creature card", count_graveyard_by_type(player, TYPE_CREATURE), 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		int sac = -1;
		if( choice == 1 ){
			scnprintf(test.message, 100, "Select a creature to sacrifice.");
			sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			if( ! sac ){
				spell_fizzled = 1;
				return 0;
			}
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
		}
		if( choice == 2 ){
			scnprintf(test.message, 100, "Select a creature card to exile.");
			sac = new_select_a_card(player, player, TUTOR_FROM_GRAVE, 0, AI_MIN_VALUE, -1, &test);
			if( ! sac ){
				spell_fizzled = 1;
				return 0;
			}
		}
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XB(2, 1), 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			if( choice == 1 ){
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
			if( choice == 2 ){
				rfg_card_from_grave(player, sac);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, -2, -2);
		}
	}

	return 0;
}

int card_cabal_pit(int player, int card, event_t event){
	/* Cabal Pit	""
	 * Land
	 * |T: Add |B to your mana pool. ~ deals 1 damage to you.
	 * Threshold - |B, |T, Sacrifice ~: Target creature gets -2/-2 until end of turn. Activate this ability only if seven or more cards are in your graveyard. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAC_WEAKEN
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() && has_threshold(player) &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_B(2), 0, &td, NULL) )
		{
			choice = CHOICE_SAC_WEAKEN;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_SAC_WEAKEN && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Sac & weaken creature", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA ){
			mana_producer(player, card, event);
			damage_player(player, 1, player, card);
		}
		if( choice == CHOICE_SAC_WEAKEN ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_B(1), 0,
										&td, "TARGET_CREATURE");
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAC_WEAKEN && valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -2, -2);
		}
	}

	return 0;
}

int card_cabal_shrine(int player, int card, event_t event){
/* CARD_ID_CABAL_SHRINE	5500
Cabal Shrine	|1|B|B
Enchantment
Whenever a player casts a spell, that player discards X cards, where X is the number of cards in all graveyards with the same name as that spell.*/

	int result = od_shrine(player, card, event);
	if( result ){
		new_multidiscard(trigger_cause_controller, result, 0, player);
	}

	return global_enchantment(player, card, event);
}

int card_call_of_the_herd(int player, int card, event_t event){
	/* Call of the Herd	|2|G
	 * Sorcery
	 * Put a 3/3 |Sgreen Elephant creature token onto the battlefield.
	 * Flashback |3|G */

	 if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		generate_token_by_id(player, card, CARD_ID_ELEPHANT);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XG(3, 1));
}

int card_cantivore(int player, int card, event_t event){
	/* Cantivore	|1|W|W
	 * Creature - Lhurgoyf 100/100
	 * Vigilance
	 * ~'s power and toughness are each equal to the number of enchantment cards in all graveyards. */

	vigilance(player, card, event);
	goyf(player, card, event, TYPE_ENCHANTMENT);
	return 0;
}

// catalyst stone --> vanilla

int card_careful_study(int player, int card, event_t event){
/*
Careful Study |U
Sorcery
Draw two cards, then discard two cards.
*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 48;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		multidiscard(player, 2, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_caustic_tar(int player, int card, event_t event){
/*
Caustic Tar |4|B|B
Enchantment — Aura
Enchant land
Enchanted land has "{T}: Target player loses 3 life."
*/
	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		target_definition_t td1;
		default_target_definition(t_player, t_card, &td1, 0);
		td1.zone = TARGET_ZONE_PLAYERS;

		if( event == EVENT_CAN_ACTIVATE ){
			return generic_activated_ability(t_player, t_card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td1, NULL);
		}

		if( event == EVENT_ACTIVATE ){
			ai_modifier += (current_turn == 1-player ? 48 : -48);
			instance->number_of_targets = 0;
			generic_activated_ability(t_player, t_card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				if( select_target(player, card, &td1, "Select target player.", &(instance->targets[0])) ){
					instance->info_slot = get_protections_from(t_player, t_card);
					instance->number_of_targets = 1;
				}
				else{
					spell_fizzled = 1;
					remove_state(t_player, t_card, STATE_TAPPED);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td1;
		base_target_definition(player, card, &td1, 0);
		td1.zone = TARGET_ZONE_PLAYERS;
		td1.allowed_controller = ANYBODY;
		td1.preferred_controller = 1-player;
		td1.illegal_abilities = instance->info_slot;
		if( validate_target(player, card, &td1, 0) ){
			lose_life(instance->targets[0].player, 3);
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;
	td.allowed_controller = player;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target land you control.");
}

int card_centaur_garden(int player, int card, event_t event){
	/* Centaur Garden	""
	 * Land
	 * |T: Add |G to your mana pool. ~ deals 1 damage to you.
	 * Threshold - |G, |T, Sacrifice ~: Target creature gets +3/+3 until end of turn. Activate this ability only if seven or more cards are in your graveyard. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAC_PUMP
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() && has_threshold(player) &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_G(2), 0, &td, NULL) )
		{
			choice = CHOICE_SAC_PUMP;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_SAC_PUMP && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Sac & pump creature", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA ){
			mana_producer(player, card, event);
			damage_player(player, 1, player, card);
		}
		if( choice == CHOICE_SAC_PUMP ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_G(1), 0,
										&td, "TARGET_CREATURE");
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAC_PUMP && valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3, 3);
		}
	}

	return 0;
}

int card_cephalid_broker(int player, int card, event_t event){
/* CARD_ID_CEPHALID_BROKER	5507
Cephalid Broker	|3|U
Creature - Cephalid 2/2
|T: Target player draws two cards, then discards two cards. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
		if( spell_fizzled != 1 ){
			ai_modifier_for_targeted_card_drawing(player, card, 0, 2);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			draw_cards(instance->targets[0].player, 2);
			new_multidiscard(instance->targets[0].player, 2, 0, player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_cephalid_coliseum(int player, int card, event_t event){
/* CARD_ID_CEPHALID_COLISEUM	5508
Cephalid Coliseum	""
Land
|T: Add |U to your mana pool. ~ deals 1 damage to you.
Threshold - |U, |T, Sacrifice ~: Target player draws three cards, then discards three cards. Activate this ability only if seven or more cards are in your graveyard.*/

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAC_DRAW
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() && has_threshold(player) &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_U(2), 0, &td, NULL) )
		{
			choice = CHOICE_SAC_DRAW;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_SAC_DRAW && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Sac & draw", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA ){
			mana_producer(player, card, event);
			damage_player(player, 1, player, card);
		}
		if( choice == CHOICE_SAC_DRAW ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_U(1), 0,
										&td, "TARGET_CREATURE_OR_PLAYER");
			ai_modifier_for_targeted_card_drawing(player, card, 0, 3);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAC_DRAW ){
			if( valid_target(&td) ){
				draw_cards(instance->targets[0].player, 3);
				new_multidiscard(instance->targets[0].player, 3, 0, player);
			}
		}
	}

	return 0;
}

int card_cephalid_looter(int player, int card, event_t event){
/* CARD_ID_CEPHALID_LOOTER	5509
Cephalid Looter	|2|U
Creature - Cephalid Rogue 2/1
|T: Target player draws a card, then discards a card. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
		if( spell_fizzled != 1 ){
			ai_modifier_for_targeted_card_drawing(player, card, 0, 1);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			draw_cards(instance->targets[0].player, 1);
			new_multidiscard(instance->targets[0].player, 1, 0, player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_cephalid_retainer(int player, int card, event_t event){
/* CARD_ID_CEPHALID_RETAINER	5510
Cephalid Retainer	|2|U|U
Creature - Cephalid 2/3
|U|U: Tap target creature without flying. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_abilities = KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_U(2), 0, &td, "TARGET_CREATURE");
}

int card_cephalid_shrine(int player, int card, event_t event){
/* CARD_ID_CEPHALID_SHRINE	5512
Cephalid Shrine	|1|U|U
Enchantment
Whenever a player casts a spell, counter that spell unless that player pays |X, where X is the number of cards in all graveyards with the same name as the spell.*/

	int result = od_shrine(player, card, event);
	if( result ){
		int kill = 1;
		if( has_mana(trigger_cause_controller, COLOR_ANY, result) &&
			charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, trigger_cause_controller, COLOR_ANY, result) )
		{
			kill = 0;
		}
		if( kill ){
			real_counter_a_spell(player, card, trigger_cause_controller, trigger_cause);
		}
	}

	return global_enchantment(player, card, event);
}

int card_chamber_of_manipulation(int player, int card, event_t event){

	/* Chamber of Manipulation	|2|U|U
	 * Enchantment - Aura
	 * Enchant land
	 * Enchanted land has "|T, Discard a card: Gain control of target creature until end of turn." */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		target_definition_t td1;
		default_target_definition(t_player, t_card, &td1, TYPE_CREATURE);

		if( event == EVENT_CAN_ACTIVATE ){
			return generic_activated_ability(t_player, t_card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DISCARD, MANACOST0, 0, &td1, NULL);
		}

		if( event == EVENT_ACTIVATE ){
			instance->number_of_targets = 0;
			generic_activated_ability(t_player, t_card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				if( select_target(player, card, &td1, "Select target creature.", &(instance->targets[0])) ){
					instance->info_slot = get_protections_from(t_player, t_card);
					instance->number_of_targets = 1;
					discard(player, 0, player);
				}
				else{
					spell_fizzled = 1;
					remove_state(t_player, t_card, STATE_TAPPED);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td1;
		default_target_definition(player, card, &td1, 0);
		td1.illegal_abilities = instance->info_slot;

		if( validate_target(player, card, &td1, 0) ){
			gain_control_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card);
		}
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;
	td.allowed_controller = player;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target land you control.");
}

int card_chance_encounter(int player, int card, event_t event){
	/* Chance Encounter	|2|R|R
	 * Enchantment
	 * Whenever you win a coin flip, put a luck counter on ~.
	 * At the beginning of your upkeep, if ~ has ten or more luck counters on it, you win the game. */

	// Coin-flip trigger is in flip_a_coin()

	if( current_turn == player && upkeep_trigger(player, card, event) ){
		if( count_counters(player, card, COUNTER_LUCK) >= 10 ){
			lose_the_game(1-player);
		}
	}

	if (event == EVENT_SHOULD_AI_PLAY){
		int counters = count_counters(player, card, COUNTER_LUCK);
		if (player == AI){
			ai_modifier += counters * counters * counters;
		} else if (counters >= 7){
			ai_modifier -= counters * counters * counters;
		}
		if (counters >= 10){
			lose_the_game(1-player);
		}
	}

	return global_enchantment(player, card, event);
}

int card_charmed_pendant(int player, int card, event_t event){
/* CARD_ID_CHARMED_PENDANT	5516
Charmed Pendant	|4
Artifact
|T, Put the top card of your library into your graveyard: For each colored mana symbol in that card's mana cost, add one mana of that color to your mana pool.
Activate this ability only any time you could cast an instant. (For example, if the card's mana cost is |3|U|U|B, you add |U|U|B to your mana pool.)*/

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_use_activated_abilities(player, card) && ! is_tapped(player, card) && ! is_animated_and_sick(player, card) ){
			if( deck_ptr[player][0] > -1 ){
				return 1;
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		ai_modifier += 48;
		card_ptr_t* c = cards_ptr[ cards_data[deck_ptr[player][0]].id ];
		// Unimplemented: Ruling 11/1/2005: If you flip over a hybrid card, you get one of either color mana, not both, for each mana symbol.
		produce_mana_tapped_multi(player, card, 0, c->req_black, c->req_blue, c->req_green, c->req_red, c->req_white);
		mill(player, 1);
	}

	return 0;
}

int card_chatter_of_squirrel(int player, int card, event_t event){
	/* Chatter of the Squirrel	|G
	 * Sorcery
	 * Put a 1/1 |Sgreen Squirrel creature token onto the battlefield.
	 * Flashback |1|G */

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		generate_token_by_id(player, card, CARD_ID_SQUIRREL);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XG(1, 1));
}

int card_childhood_horror(int player, int card, event_t event){
/* CARD_ID_CHILDHOOD_HORROR	5518
Childhood Horror	|3|B
Creature - Horror 2/2
Flying
Threshold - As long as seven or more cards are in your graveyard, ~ gets +2/+2 and can't block. */

	threshold_mod_for_pt_and_abilitie(player, card, event, 2, 2, 0, SP_KEYWORD_CANNOT_BLOCK);

	return 0;
}

int card_chlorophant(int player, int card, event_t event){
/* CARD_ID_CHLOROPHANT	5519
Chlorophant	|G|G|G
Creature - Elemental 1/1
At the beginning of your upkeep, you may put a +1/+1 counter on ~.
Threshold - As long as seven or more cards are in your graveyard, ~ has "At the beginning of your upkeep, you may put another +1/+1 counter on ~."*/

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_DUH);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int cnters = 1;
		if( has_threshold(player) ){
			cnters++;
		}
		add_1_1_counters(player, card, cnters);
	}

	return 0;
}

int card_coffin_purge(int player, int card, event_t event){
/* CARD_ID_COFFIN_PURGE	5520
Coffin Purge	|B
Instant
Exile target card from a graveyard.
Flashback |B (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_B(1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select target card to exile.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			rfg_card_from_grave(instance->targets[0].player, selected);
		}
		kill_card(player, card, get_flashback(player, card) ? KILL_EXILE : KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &test);
}

int card_cognivore(int player, int card, event_t event){
/* CARD_ID_COGNIVORE	5521
Cognivore	|6|U|U
Creature - Lhurgoyf 100/100
Flying
~'s power and toughness are each equal to the number of instant cards in all graveyards. */

	goyf(player, card, event, TYPE_INSTANT | TYPE_INTERRUPT);
	return 0;
}

int card_concentrate(int player, int card, event_t event){
	/*
	  Concentrate |2|U|U
	  Sorcery
	  Draw three cards.
	*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_LAND, -1)*8);
	}

	if(event == EVENT_RESOLVE_SPELL){
		draw_cards(player, 3);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_confessor(int player, int card, event_t event){
/* CARD_ID_CONFESSOR	5522
Confessor	|W
Creature - Human Cleric 1/1
Whenever a player discards a card, you may gain 1 life. */

	if( discard_trigger(player, card, event, 2, RESOLVE_TRIGGER_DUH, 0) ){
		gain_life(player, 1);
	}

	return 0;
}

int card_crystal_quarry(int player, int card, event_t event){
/* CARD_ID_CRYSTAL_QUARRY	5525
Crystal Quarry	""
Land
|T: Add |C to your mana pool.
|5, |T: Add |W|U|B|R|G to your mana pool. */

	if( event == EVENT_ACTIVATE ){
		int choice = 1;
		if( ! paying_mana() ){
			choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1,
							"Get 1", 1, 5,
							"Get WUBRG", has_mana(player, COLOR_ANY, 6), 10);
		}
		if( choice == 1 ){
			return mana_producer(player, card, event);
		}

		if( choice == 2 ){
			add_state(player, card, STATE_TAPPED);
			charge_mana(player, COLOR_ANY, 5);
			if( spell_fizzled != 1 ){
				produce_mana_tapped_multi(player, card, 0, 1, 1, 1, 1, 1);
			}
			else{
				remove_state(player, card, STATE_TAPPED);
			}
		}
		return 0;
	}

	return mana_producer(player, card, event);
}

int card_cultural_exchanges(int player, int card, event_t event){
/*
Cultural Exchange |4|U|U
Sorcery
Choose any number of creatures target player controls. Choose the same number of creatures another target player controls.
Those players exchange control of those creatures. (This effect lasts indefinitely.)
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;
	td.allow_cancel = 2; //"Done" button.

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.allowed_controller = 1-player;
	td2.preferred_controller = 1-player;
	td2.illegal_abilities = 0;
	td2.allow_cancel = 0;

	target_definition_t td3;
	default_target_definition(player, card, &td3, 0);
	td3.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( would_validate_arbitrary_target(&td3, player, -1) && would_validate_arbitrary_target(&td3, 1-player, -1) ){
			return basic_spell(player, card, event);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->targets[0].player = player;
		instance->targets[0].card = -1;
		instance->targets[1].player = 1-player;
		instance->targets[1].card = -1;
		instance->number_of_targets = 2;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td3, 0) && validate_target(player, card, &td3, 1) ){
			int marked[2][151] = {{0}};
			int max = 0;
			while( max < target_available(player, card, &td2) ){
					if( new_pick_target(&td, "Select a creature you control.", 0, GS_LITERAL_PROMPT) ){
						marked[player][instance->targets[0].card] = 1;
						add_state(player, instance->targets[0].card, STATE_TARGETTED);
						max++;
					}
					else{
						break;
					}
			}
			while( max ){
					if( new_pick_target(&td2, "Select a creature you opponent controls.", 0, GS_LITERAL_PROMPT) ){
						marked[1-player][instance->targets[0].card] = 1;
						add_state(1-player, instance->targets[0].card, STATE_TARGETTED);
						max--;
					}
			}
			APNAP(p,{
						int c;
						for( c=active_cards_count[p]-1; c > -1; c++ ){
							if( in_play(p, c) && marked[p][c] ){
								remove_state(p, c, STATE_TARGETTED);
								gain_control_permanently(1-p, p, c);
							}
						}
					}
			);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_darkwater_catacombs(int player, int card, event_t event){
/* CARD_ID_DARKWATER_CATACOMBS	5528
Darkwater Catacombs	""
Land
|1, |T: Add |U|B to your mana pool. */

	return od_land(player, card, event, COLOR_BLACK, COLOR_BLUE);
}

int card_darkwater_egg(int player, int card, event_t event){
/* CARD_ID_DARKWATER_EGG	5529
Darkwater Egg	|1
Artifact
|2, |T, Sacrifice ~: Add |U|B to your mana pool. Draw a card. */

	return od_egg(player, card, event, COLOR_BLACK, COLOR_BLUE);
}

int card_decimate(int player, int card, event_t event){
/* CARD_ID_DECIMATE	5531
Decimate	|2|R|G
Sorcery
Destroy target artifact, target creature, target enchantment, and target land. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td_artifact;
	default_target_definition(player, card, &td_artifact, TYPE_ARTIFACT );

	target_definition_t td_creature;
	default_target_definition(player, card, &td_creature, TYPE_CREATURE );

	target_definition_t td_enchantment;
	default_target_definition(player, card, &td_enchantment, TYPE_ENCHANTMENT );

	target_definition_t td_land;
	default_target_definition(player, card, &td_land, TYPE_LAND );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td_artifact) && can_target(&td_creature) && can_target(&td_enchantment) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td_land, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( pick_target(&td_artifact, "TARGET_ARTIFACT")
			&& new_pick_target(&td_creature, "TARGET_CREATURE", 1, 1)
			&& new_pick_target(&td_enchantment, "TARGET_ENCHANTMENT", 2, 1) )
		{
			new_pick_target(&td_land, "TARGET_LAND", 3, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td_artifact, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( validate_target(player, card, &td_creature, 1) ){
			kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
		}
		if( validate_target(player, card, &td_enchantment, 2) ){
			kill_card(instance->targets[2].player, instance->targets[2].card, KILL_DESTROY);
		}
		if( validate_target(player, card, &td_land, 3) ){
			kill_card(instance->targets[3].player, instance->targets[3].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int ability_decompose(int player, int card, event_t event)
{
  target_definition_t td;
  base_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CAN_ACTIVATE)
	return can_target(&td);

  if (event == EVENT_ACTIVATE
	  && pick_target(&td, "TARGET_GRAVEYARD"))
	{
	  instance->number_of_targets = 0;
	  select_multiple_cards_from_graveyard(player, instance->targets[0].player, 0, player == instance->targets[0].player ? AI_MIN_VALUE : AI_MAX_VALUE,
										   NULL, 3, &instance->targets[1]);
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  int i, any_exiled = 0, num_targets = 0;
	  for (i = 1; i <= 3; ++i)
		if (instance->targets[i].player != -1)
		  {
			++num_targets;
			int selected = validate_target_from_grave(player, card, instance->targets[0].player, i);
			if (selected != -1)
			  {
				rfg_card_from_grave(instance->targets[0].player, selected);
				any_exiled = 1;
			  }
		  }

	  if (num_targets > 0 && !any_exiled)
		spell_fizzled = 1;
	}

  return 0;
}

int card_decompose(int player, int card, event_t event)
{
  /* Decompose	|1|B
   * Sorcery
   * Exile up to three target cards from a single graveyard. */
  // Used externally
  if (!IS_CASTING(player, card, event))
	return 0;

  if (event == EVENT_CAN_CAST)
	return ability_decompose(player, card, EVENT_CAN_ACTIVATE);
  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	return ability_decompose(player, card, EVENT_ACTIVATE);
  if (event == EVENT_RESOLVE_SPELL)
	{
	  ability_decompose(player, card, EVENT_RESOLVE_ACTIVATION);
	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_dedicated_martyr(int player, int card, event_t event){
/* CARD_ID_DEDICATED_MARTYR	5533
Dedicated Martyr	|W
Creature - Human Cleric 1/1
|W, Sacrifice ~: You gain 3 life. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 3);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_W(1), 0, NULL, NULL);
}

int card_delaying_shield(int player, int card, event_t event){
/* Delaying Shield	|3|W
 * Enchantment
 * If damage would be dealt to you, put that many delay counters on ~ instead.
 * At the beginning of your upkeep, remove all delay counters from ~. For each delay counter removed this way, you lose 1 life unless
 *		you pay |1|W. */

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *source = get_card_instance(affected_card_controller, affected_card);
		if( damage_card == source->internal_card_id && source->info_slot > 0 ){
			if( source->damage_target_player == player && source->damage_target_card == -1 ){
				add_counters(player, card, COUNTER_DELAY, source->info_slot);
				source->info_slot = 0;
			}
		}
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int count = count_counters(player, card, COUNTER_DELAY);
		int paid = 0;
		while( count > 0 ){
				if( has_mana_multi(player, MANACOST_XW(1, 1)) ){
					charge_mana_multi(player, MANACOST_XW(1, 1));
					if( spell_fizzled != 1 ){
						paid++;
					}
				}
				else{
					break;
				}
				count--;
		}
		int ltl = count_counters(player, card, COUNTER_DELAY) - paid;
		lose_life(player, ltl);
		remove_all_counters(player, card, COUNTER_DELAY);
	}

	return global_enchantment(player, card, event);
}

int card_dematerialize(int player, int card, event_t event){
/* CARD_ID_DEMATERIALIZE	5537
Dematerialize	|3|U
Sorcery
Return target permanent to its owner's hand.
Flashback |5|U|U (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XU(5, 2));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
}

int card_demolish(int player, int card, event_t event ){
	/*
	  Demolish |3|R
	  Sorcery
	  Destroy target artifact or land.
	*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_LAND);

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target artifact or land.", 1, NULL);
}

int card_demoralize(int player, int card, event_t event){
	/* Demoralize	|2|R
	 * Instant
	 * All creatures gain menace until end of turn.
	 * Threshold - If seven or more cards are in your graveyard, creatures can't block this turn. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) )
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1) * (has_threshold(player) ? 8 : 4));

	if( event == EVENT_RESOLVE_SPELL ){
		int s_key = has_threshold(player) ? SP_KEYWORD_CANNOT_BLOCK : SP_KEYWORD_MENACE;
		pump_subtype_until_eot(player, card, ANYBODY, -1, 0, 0, 0, s_key);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_deserted_temple(int player, int card, event_t event){
/* CARD_ID_DESERTED_TEMPLE	5539
Deserted Temple	""
Land
|T: Add |C to your mana pool.
|1, |T: Untap target land. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_UNTAP
	};

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, NULL))
		{
			choice = CHOICE_UNTAP;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_UNTAP && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Untap land", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_UNTAP ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_LAND");
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_UNTAP ){
			if( valid_target(&td) ){
				untap_card( instance->targets[0].player, instance->targets[0].card );
			}
		}
	}

	return 0;
}

int card_devoted_caretaker(int player, int card, event_t event){
/* CARD_ID_DEVOTED_CARETAKER	5540
Devoted Caretaker	|W
Creature - Human Cleric 1/2
|W, |T: Target permanent you control gains protection from instant spells and from sorcery spells until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0,
									KEYWORD_PROT_INSTANTS + KEYWORD_PROT_SORCERIES, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(1), 0, &td, "TARGET_CREATURE");
}

int card_diligent_farmhand(int player, int card, event_t event){

	/* Diligent Farmhand	|G
	 * Creature - Human Druid 1/1
	 * |1|G, Sacrifice ~: Search your library for a basic land card and put that card onto the battlefield tapped. Then shuffle your library.
	 * If ~ is in a graveyard, effects from spells named Muscle Burst count it as a card named Muscle Burst. */

	if( event == EVENT_ACTIVATE ){
		if( current_turn == 1-player && count_subtype(player, TYPE_LAND, -1) <= 3 ){
			ai_modifier += (count_subtype(player, TYPE_CREATURE, -1) * 8);
		}
		card_instance_t *instance = get_card_instance(player, card);
		if( check_state(player, card, STATE_BLOCKING) && instance->blocking != 255 ){
			if( get_power(1-player, instance->blocking) >= get_toughness(player, card) &&
				get_toughness(1-player, instance->blocking) > get_power(player, card) )
			{
				ai_modifier += 96;
			}

		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XG(1, 1), 0, NULL, NULL);
}

int card_dirty_wererat(int player, int card, event_t event){
/* CARD_ID_DIRTY_WERERAT	5542
Dirty Wererat	|3|B
Creature - Human Rat Minion 2/3
|B, Discard a card: Regenerate ~.
Threshold - As long as seven or more cards are in your graveyard, ~ gets +2/+2 and can't block. */

	threshold_mod_for_pt_and_abilitie(player, card, event, 2, 2, 0, SP_KEYWORD_CANNOT_BLOCK);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );
		if( can_regenerate(instance->parent_controller, instance->parent_card) ){
			regenerate_target(instance->parent_controller, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD | GAA_REGENERATION, MANACOST_B(1), 0, NULL, NULL);
}

int card_divine_sacrament(int player, int card, event_t event){
/* CARD_ID_DIVINE_SACRAMENT	5544
Divine Sacrament	|1|W|W
Enchantment
|SWhite creatures get +1/+1.
Threshold - |SWhite creatures get an additional +1/+1 as long as seven or more cards are in your graveyard. */

	if( event == EVENT_POWER || event == EVENT_TOUGHNESS ){
		int amt = has_threshold(player) ? 2 : 1;
		boost_creature_by_color(player, card, event, COLOR_TEST_WHITE, amt, amt, 0, BCT_INCLUDE_SELF);
	}

	return global_enchantment(player, card, event);
}

// druid lyrist --> elvish lirist

// dwarvern grunt --> vanilla

int card_dwarven_recruiter(int player, int card, event_t event){
/* CARD_ID_DWARVEN_RECRUITER	5550
Dwarven Recruiter	|2|R
Creature - Dwarf 2/2
When ~ enters the battlefield, search your library for any number of Dwarf cards and reveal those cards.
Shuffle your library, then put them on top of it in any order. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, 0, "Select a Dwarf card.");
		this_test.subtype = SUBTYPE_DWARF;
		this_test.qty = 100;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND_AND_MARK, 0, AI_MAX_VALUE, &this_test);
		shuffle(player);

		int tutored = 0;
		int i;
		for(i=active_cards_count[player]-1; i > -1; i--){
			if( in_hand(player, i) && check_state(player, i, STATE_TARGETTED) ){
				put_on_top_of_deck(player, i);
				tutored++;
			}
		}
		if( tutored > 1 ){
			rearrange_top_x(player, player, tutored);
		}
	}

	return 0;
}

int card_dwarven_shrine(int player, int card, event_t event){
/* CARD_ID_DWARVEN_SHRINE	5551
Dwarven Shrine	|1|R|R
Enchantment
Whenever a player casts a spell, ~ deals X damage to that player, where X is twice the number of cards in all graveyards with the same name as that spell.*/

	int amount = od_shrine(player, card, event);
	if( amount ){
		damage_player(trigger_cause_controller,  2*amount, player, card);
	}

	return global_enchantment(player, card, event);
}

int card_earnest_fellowship(int player, int card, event_t event){
/* CARD_ID_EARNEST_FELLOWSHIP	5553
Earnest Fellowship	|1|W
Enchantment
Each creature has protection from its colors. */

	if( event == EVENT_ABILITIES && in_play(player, card) && ! is_humiliated(player, card) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			int clr = get_color(affected_card_controller, affected_card);
			int i;
			for(i=1; i<6; i++){
				if( clr & (1<<i) ){
					event_result |= (1<<(10+i));
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_earth_rift(int player, int card, event_t event){
/* CARD_ID_EARTH_RIFT	5554
Earth Rift	|3|R
Sorcery
Destroy target land.
Flashback |5|R|R (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XR(5, 2));
	}


	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);

}

int card_elephants_ambush(int player, int card, event_t event){
	/* Elephant Ambush	|2|G|G
	 * Instant
	 * Put a 3/3 |Sgreen Elephant creature token onto the battlefield.
	 * Flashback |6|G|G */

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}
	if( event == EVENT_RESOLVE_SPELL ){
		generate_token_by_id(player, card, CARD_ID_ELEPHANT);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return do_flashback(player, card, event, MANACOST_XG(6, 2));
}

static int engulfing_flames_legacy(int player, int card, event_t event){

	if( event == EVENT_CLEANUP ){
		card_instance_t *instance = get_card_instance(player, card);
		remove_status(instance->damage_target_player, instance->damage_target_card, STATUS_CANNOT_REGENERATE);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_engulfing_flames(int player, int card, event_t event){
/* CARD_ID_ENGULFING_FLAMES	5558
Engulfing Flames	|R
Instant
~ deals 1 damage to target creature. It can't be regenerated this turn.
Flashback |3|R (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XR(3, 1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			add_status(instance->targets[0].player, instance->targets[0].card, STATUS_CANNOT_REGENERATE);
			create_targetted_legacy_effect(player, card, &engulfing_flames_legacy, instance->targets[0].player, instance->targets[0].card);
			damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_entomb2(int player, int card, event_t event){
/* CARD_ID_ENTOMB	1526
Entomb	|B
Instant
Search your library for a card and put that card into your graveyard. Then shuffle your library. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 96;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card to put into your graveyard.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_GRAVE, 0, AI_GOOD_TO_PUT_IN_GRAVE, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_epicenter(int player, int card, event_t event){
/* CARD_ID_EPICENTER	5559
Epicenter	|4|R
Sorcery
Target player sacrifices a land.
Threshold - Each player sacrifices all lands he or she controls instead if seven or more cards are in your graveyard. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( ! has_threshold(player) ){
			if( valid_target(&td) ){
				player_sacrifices_a_permanent(player, card, instance->targets[0].player, TYPE_LAND, SAC_CAUSED | SAC_NO_CANCEL);
			}
		}
		else{
			APNAP(p,{
						if( can_cause_sacrifice(player, p) ){
							int c;
							for(c=active_cards_count[p]-1; c > -1; c--){
								if( in_play(p, c) && is_what(p, c, TYPE_LAND) && ! check_special_flags2(p, c, SF2_CANNOT_BE_SACRIFICED) ){
									kill_card(p, c, KILL_SACRIFICE);
								}
							}
						}
					}
			);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_escape_artist(int player, int card, event_t event){
/* CARD_ID_ESCAPE_ARTIST	5560
Escape Artist	|1|U
Creature - Human Wizard 1/1
~ can't be blocked.
|U, Discard a card: Return ~ to its owner's hand. */

	unblockable(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		bounce_permanent(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST_U(1), 0, NULL, NULL);
}

int card_execute(int player, int card, event_t event){
	/*
	  Execute |2|B
	  Instant
	  Destroy target white creature. It can't be regenerated.
	  Draw a card.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target %s creature.", COLOR_WHITE), 1, NULL);
}

int card_extract2(int player, int card, event_t event){
/* CARD_ID_EXTRACT	5561
Extract	|U
Sorcery
Search target player's library for a card and exile it. Then that player shuffles his or her library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");
			new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_MAX_VALUE, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_firebolt(int player, int card, event_t event){
	/* Firebolt	|R	0x2008d2e
	 * Sorcery
	 * ~ deals 2 damage to any target.
	 * Flashback |4|R */
	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XR(4, 1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			damage_target0(player, card, 2);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

// flame burst --> kindle

int card_frightcrawler(int player, int card, event_t event){
/* CARD_ID_FRIGHTCRAWLER	5570
Frightcrawler	|1|B
Creature - Horror 1/1
Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
Threshold - As long as seven or more cards are in your graveyard, ~ gets +2/+2 and can't block. */

	fear(player, card, event);

	threshold_mod_for_pt_and_abilitie(player, card, event, 2, 2, 0, SP_KEYWORD_CANNOT_BLOCK);

	return 0;
}

int card_ghastly_demise(int player, int card, event_t event){
/*
Ghastly Demise |B
Instant
Destroy target nonblack creature if its toughness is less than or equal to the number of cards in your graveyard.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td,
						get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, NULL);
		if( spell_fizzled != 1 ){
			if( get_toughness(instance->targets[0].player, instance->targets[0].card) > count_graveyard(player) ){
				ai_modifier -= 96;
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			if( get_toughness(instance->targets[0].player, instance->targets[0].card) <= count_graveyard(player) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td,
						get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, NULL);
}

int card_gorilla_titan(int player, int card, event_t event){
/* CARD_ID_GORILLA_TITAN	1706
Gorilla Titan	|3|G|G
Creature - Ape 4/4
Trample
~ gets +4/+4 as long as there are no cards in your graveyard. */

	if(event == EVENT_POWER || event == EVENT_TOUGHNESS){
		if( affect_me(player, card) && count_graveyard(player) == 0 && ! is_humiliated(player, card) ){
			event_result += 4;
		}
	}
	return 0;
}

int card_gravestorm(int player, int card, event_t event){
/* CARD_ID_GRAVESTORM	5573
Gravestorm	|B|B|B
Enchantment
At the beginning of your upkeep, target opponent may exile a card from his or her graveyard. If that player doesn't, you may draw a card. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int will_draw = 1;
		if( count_graveyard(1-player) > 0 ){
			char msg[100] = "Select a card to exile.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, msg);
			if( new_global_tutor(1-player, 1-player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test) != -1 ){
				will_draw = 0;
			}
		}
		if( will_draw == 1 ){
			draw_cards(player, 1);
		}
	}

	return global_enchantment(player, card, event);
}

int card_ground_seal(int player, int card, event_t event){
/* CARD_ID_GROUND_SEAL	5574
Ground Seal	|1|G
Enchantment
When ~ enters the battlefield, draw a card.
Cards in graveyards can't be the targets of spells or abilities. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	return global_enchantment(player, card, event);
}

int card_hallowed_healer(int player, int card, event_t event){
	/* Hallowed Healer	|2|W	0x2008d42
	 * Creature - Human Cleric 1/1
	 * |T: Prevent the next 2 damage that would be dealt to any target this turn.
	 * Threshold - |T: Prevent the next 4 damage that would be dealt to any target this turn. Activate this ability only if seven or more cards are in your graveyard. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Prevent 2 damage", 1, 1,
							"Prevent 4 damage", has_threshold(player), 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 2*instance->info_slot);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST0, 0,
									&td, "TARGET_DAMAGE");
}

int card_haunting_echoes(int player, int card, event_t event){
/*
Haunting Echoes |3|B|B
Sorcery
Exile all cards from target player's graveyard other than basic land cards. For each card exiled this way, search that player's library
for all cards with the same name as that card and exile them. Then that player shuffles his or her library.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_graveyard(1-player)*4);
	}


	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int exiled[count_graveyard(instance->targets[0].player)];
			int ec = 0;
			int count = count_graveyard(instance->targets[0].player)-1;
			while( count > -1 ){
					int iid = get_grave(instance->targets[0].player)[count];
					if( ! is_basic_land_by_id( cards_data[ iid ].id ) ){
						rfg_card_from_grave(instance->targets[0].player, count);
						if( ec == 0 ){
							exiled[ec] = iid;
							ec++;
						}
						else{
							int i;
							int found = 0;
							for(i=0; i<ec; i++){
								if( exiled[i] == iid ){
									found = 1;
									break;
								}
							}
							if( ! found ){
								exiled[ec] = iid;
								ec++;
							}
						}
					}
					count--;
			}
			int choice = 0;
			if( player == HUMAN ){
				choice = do_dialog(player, player, card, -1, -1, " Remove All Matches\n Manually Pick Cards", 0);
			}
			if( choice == 0 ){
				count = count_deck(instance->targets[0].player);
				while( count > -1 ){
						int i;
						int found = 0;
						for(i=0; i<ec; i++){
							if( deck_ptr[instance->targets[0].player][count] == exiled[i] ){
								found = 1;
								break;
							}
						}
						if( found ){
							rfg_card_in_deck(instance->targets[0].player, count);
						}
						count--;
				}
				show_deck( player, deck_ptr[instance->targets[0].player], count_deck(instance->targets[0].player),
						"Cards Remaining in Deck", 0, 0x7375B0 );
			}
			else{
				int i;
				for(i=0; i<ec; i++){
					int csvid = cards_data[exiled[i]].id;
					test_definition_t test;
					default_test_definition(&test, TYPE_ANY);
					scnprintf(test.message, 100, "Select a card named %s to exile.", cards_ptr[csvid]->name);
					test.qty = count_deck(instance->targets[0].player);
					new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_FIRST_FOUND, &test);
				}
			}
		}
		kill_card( player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_holistic_wisdom(int player, int card, event_t event){
/*
Holistic Wisdom |1|G|G
Enchantment
{2}, Exile a card from your hand: Return target card from your graveyard to your hand if it shares a card type with the card exiled this way.
	(Artifact, creature, enchantment, instant, land, planeswalker, sorcery, and tribal are card types.)
*/
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");

	test_definition_t this_test2;
	new_default_test_definition(&this_test2, TYPE_ANY, "Select target card.");

	if( event == EVENT_CAN_ACTIVATE ){
		if( hand_count[player] ){
			return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL,
																	0, &this_test2, GS_GRAVE_RECYCLER);
		}
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
			if( selected == -1 ){
				spell_fizzled = 1;
				return 0;
			}
			if( IS_AI(player) ){
				this_test2.type = get_type(player, selected);
			}
			int sel2 = select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &this_test, 0);
			if( sel2 == -1 ){
				spell_fizzled = 1;
				return 0;
			}
			int hand_value = get_base_value(player, selected);
			int grave_value = get_base_value(-1, get_grave(player)[sel2]);
			ai_modifier += ((grave_value - hand_value) * 2);
			rfg_card_in_hand(player, selected);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1  ){
			add_card_to_hand(player, get_grave(player)[selected]);
			remove_card_from_grave(player, selected);
		}
	}

	return 0;
}

int card_infected_vermin(int player, int card, event_t event){
/*
Infected Vermin |2|B
Creature — Rat 1/1
{2}{B}: Infected Vermin deals 1 damage to each creature and each player.
Threshold — {3}{B}: Infected Vermin deals 3 damage to each creature and each player.
	Activate this ability only if seven or more cards are in your graveyard.
*/
	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_XB(2, 1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int ab2 = has_threshold(player) && has_mana_for_activated_ability(player, card, MANACOST_XB(3, 1)) ? 1 : 0;
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"1 Damage to all", 1, 1,
							"3 Damages to all", ab2, 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == 1 ){
			return generic_activated_ability(player, card, event, 0, MANACOST_XB(2, 1), 0, NULL, NULL);
		}
		if( choice == 2 ){
			return generic_activated_ability(player, card, event, 0, MANACOST_XB(3, 1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int dmg = (instance->info_slot * 2)-1;
		new_damage_all(instance->parent_controller, instance->parent_card, ANYBODY, dmg, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
	}

	return 0;
}

int card_innocent_blood(int player, int card, event_t event){
/* CARD_ID_INNOCENT_BLOOD	5584
Innocent Blood	|B
Sorcery
Each player sacrifices a creature. */

	if( event == EVENT_CAN_CAST ){
		return 1;
	}
	else if(event == EVENT_RESOLVE_SPELL ){
			impose_sacrifice(player, card, player, 1, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0);
			impose_sacrifice(player, card, 1-player, 1, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0);
			kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_ivy_elemental(int player, int card, event_t event){
	/* Ivy Elemental	|X|G
	 * Creature - Elemental 0/0
	 * ~ enters the battlefield with X +1/+1 counters on it. */
	// Used externally
	if( event == EVENT_CAST_SPELL && affect_me(player, card) && ! is_token(player, card) ){
		generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, get_card_instance(player, card)->info_slot);
	}

	return 0;
}

int card_kamahls_desire(int player, int card, event_t event){
/* CARD_ID_KAMAHLS_DESIRE	5587
Kamahl's Desire	|1|R
Enchantment - Aura
Enchant creature
Enchanted creature has first strike.
Threshold - Enchanted creature gets +3/+0 as long as seven or more cards are in your graveyard. */

	int pump = 0;
	if( has_threshold(player) ){
		pump = 3;
	}
	return generic_aura(player, card, event, player, pump, 0, KEYWORD_FIRST_STRIKE, 0, 0, 0, 0);
}

int card_kamahl_pit_fighter(int player, int card, event_t event){
	/* Kamahl, Pit Fighter	|4|R|R	0x2008d56
	 * Legendary Creature - Human Barbarian 6/1
	 * Haste
	 * |T: ~ deals 3 damage to any target. */
	check_legend_rule(player, card, event);

	haste(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 3);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ANY");
}

int card_kirtars_desire(int player, int card, event_t event){
/* CARD_ID_KIRTARS_DESIRE	5590
Kirtar's Desire	|W
Enchantment - Aura
Enchant creature
Enchanted creature can't attack.
Threshold - Enchanted creature can't block as long as seven or more cards are in your graveyard. */

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		effect_cannot_attack(player, card, event);

		if( event == EVENT_ABILITIES && affect_me(player, card) && has_threshold(player) ){
			cannot_block(p, c, event);
		}
	}

	return disabling_aura(player, card, event);
}

int card_kirtars_wrath(int player, int card, event_t event){
/* Kirtar's Wrath	|4|W|W
 * Sorcery
 * Destroy all creatures. They can't be regenerated.
 * Threshold - If seven or more cards are in your graveyard, instead destroy all creatures, then put two 1/1 |Swhite Spirit creature
 *		tokens with flying onto the battlefield. Creatures destroyed this way can't be regenerated. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		APNAP(p,{ new_manipulate_all(player, card, p, &this_test, KILL_BURY); };);

		if( has_threshold(player) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SPIRIT, &token);
			token.qty = 2;
			token.key_plus = KEYWORD_FLYING;
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
			generate_token(&token);
		}

		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_krosan_avenger(int player, int card, event_t event){
/* CARD_ID_KROSAN_AVENGER	5593
Krosan Avenger	|2|G
Creature - Human Druid 3/1
Trample
Threshold - |1|G: Regenerate ~. Activate this ability only if seven or more cards are in your graveyard. */

	if( has_threshold(player) ){
		return regeneration(player, card, event, MANACOST_XG(1, 1));
	}
	return 0;
}

int card_krosan_beast(int player, int card, event_t event){
/* CARD_ID_KROSAN_BEAST	5594
Krosan Beast	|3|G
Creature - Squirrel Beast 1/1
Threshold - ~ gets +7/+7 as long as seven or more cards are in your graveyard. */

	threshold_mod_for_pt_and_abilitie(player, card, event, 7, 7, 0, 0);
	return 0;
}

int card_laquatus_creativity(int player, int card, event_t event){
/* CARD_ID_LAQUATUSS_CREATIVITY	5595
Laquatus's Creativity	|4|U
Sorcery
Target player draws cards equal to the number of cards in his or her hand, then discards that many cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier_for_targeted_card_drawing(player, card, 0, hand_count[instance->targets[0].player]);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = hand_count[instance->targets[0].player];
			draw_cards(instance->targets[0].player, amount);
			new_multidiscard(instance->targets[0].player, amount, 0, player);
		}
		kill_card( player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_last_rites(int player, int card, event_t event){

	/* Last Rites	|2|B
	 * Sorcery
	 * Discard any number of cards. Target player reveals his or her hand, then you choose a nonland card from it for each card discarded this way. That player
	 * discards those cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = discard_multiple_cards_at_once(player, card, 2);
			while( hand_count[instance->targets[0].player] > 0 && amount > 0 ){
					test_definition_t this_test;
					new_default_test_definition(&this_test, TYPE_LAND, "Select a nonland card.");
					this_test.type_flag = 1;

					ec_definition_t this_definition;
					default_ec_definition(instance->targets[0].player, player, &this_definition);
					this_definition.effect = EC_DISCARD;

					new_effect_coercion(&this_definition, &this_test);
					amount--;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_liutenant_kirtar(int player, int card, event_t event){
/* CARD_ID_LIEUTENANT_KIRTAR	5599
Lieutenant Kirtar	|1|W|W
Legendary Creature - Bird Soldier 2/2
Flying
|1|W, Sacrifice ~: Exile target attacking creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XW(1, 1), 0,
									&td, "Select target attacking creature.");
}

int card_life_burst(int player, int card, event_t event){
	/* Life Burst	|1|W
	 * Instant
	 * Target player gains 4 life, then gains 4 life for each card named ~ in each graveyard. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			gain_life(instance->targets[0].player, 4);
			int amount = 4 * count_graveyard_by_id(2, get_id(player, card));
			gain_life(instance->targets[0].player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);

}

int card_lithatog(int player, int card, event_t event){
	/* Lithatog	|1|R|G
	 * Creature - Atog 1/2
	 * Sacrifice an artifact: ~ gets +1/+1 until end of turn.
	 * Sacrifice a land: ~ gets +1/+1 until end of turn. */
	return generic_husk(player, card, event, TYPE_LAND | TYPE_ARTIFACT, 1, 1, 0, 0);
}

int card_luminous_guardian(int player, int card, event_t event)
{
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION)
	{
	  int choice = DIALOG(player, card, event,
						  DLG_AUTOCHOOSE_IF_1,
						  "+0/+1", has_mana_for_activated_ability(player, card, MANACOST_W(1)), 1,
						  "Block an additional creature", has_mana_for_activated_ability(player, card, MANACOST_X(2)), 0);	// Never activate for the AI, since it doesn't ever multiblock unless forced to by Blaze of Glory :(

	  if (event == EVENT_CAN_ACTIVATE)
		return choice && can_use_activated_abilities(player, card);

	  if (event == EVENT_ACTIVATE)
		{
		  if (choice == 1)
			charge_mana_for_activated_ability(player, card, MANACOST_W(1));
		  else if (choice == 2)
			charge_mana_for_activated_ability(player, card, MANACOST_X(2));
		}

	  if (event == EVENT_RESOLVE_ACTIVATION)
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  if (choice == 1)
			alternate_legacy_text(1, instance->parent_controller, pump_ability_until_eot(player, card, instance->parent_controller, instance->parent_card, 0, 1, 0, 0));
		  else if (choice == 2)
			alternate_legacy_text(2, instance->parent_controller, can_block_additional_until_eot(player, card, instance->parent_controller, instance->parent_card, 1));
		}
	}

  return 0;
}

int card_magma_vein(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_sacrifice_type_as_cost(player, 1, TYPE_LAND) ){
			return generic_activated_ability(player, card, event, 0, MANACOST_R(1), 0, NULL, NULL);
		}
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, 0, MANACOST_R(1), 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
			if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
				cancel = 1;
				return 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		this_test.keyword_flag = 1;
		new_damage_all(instance->parent_controller, instance->parent_card, ANYBODY, 1, 0, &this_test);
	}

	return 0;
}

int card_magnivore(int player, int card, event_t event){
	/* Magnivore	|2|R|R
	 * Creature - Lhurgoyf 100/100
	 * Haste
	 * ~'s power and toughness are each equal to the number of sorcery cards in all graveyards. */

	haste(player, card, event);
	goyf(player, card, event, TYPE_SORCERY);
	return 0;
}

int card_malevolent_awakening(int player, int card, event_t event){
	/*
	  Malevolent Awakening |1|B|B
	  Enchantment
	  {1}{B}{B}, Sacrifice a creature: Return target creature card from your graveyard to your hand.
	*/
	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST_XB(1, 2), 0, NULL, NULL);
		if( result && count_graveyard_by_type(player, TYPE_CREATURE) && ! graveyard_has_shroud(2) ){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		charge_mana_for_activated_ability(player, card, MANACOST_XB(1, 2));
		if( spell_fizzled != 1 ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			if (!sac){
				spell_fizzled = 1;
				return 0;
			}
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
			if( select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &test, 0) != -1 ){
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
			else{
				state_untargettable(BYTE2(sac), BYTE3(sac), 0);
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}

	return global_enchantment(player, card, event);
}

int card_master_apothecary(int player, int card, event_t event){
	/* Master Apothecary	|W|W|W	0x2008d92
	 * Creature - Human Cleric 2/2
	 * Tap an untapped Cleric you control: Prevent the next 2 damage that would be dealt to any target this turn. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller = player;
	td1.preferred_controller = player;
	td1.illegal_abilities = 0;
	td1.illegal_state = TARGET_STATE_TAPPED;
	td1.required_subtype = SUBTYPE_CLERIC;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_target(&td1) ){
		return generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
	}
	if( event == EVENT_ACTIVATE ){
		if( new_pick_target(&td1, "Select an untapped Cleric you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			int ctt = instance->targets[0].card;
			generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_DAMAGE");
			if( spell_fizzled != 1 ){
				tap_card(player, ctt);
			}
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 2);
		}
	}

	return 0;
}

int card_millikin(int player, int card, event_t event){

	if( event == EVENT_CAN_ACTIVATE || (event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		if( deck_ptr[player][0] == -1 ){
			return 0;
		}
	}

	if( event == EVENT_ACTIVATE ){
		mana_producer(player, card, event);
		if( spell_fizzled != 1 ){
			mill(player, 1);
		}
		return 0;
	}

	return mana_producer(player, card, event);
}

int card_mind_burst(int player, int card, event_t event){
	/* Mind Burst	|1|B
	 * Sorcery
	 * Target player discards X cards, where X is one plus the number of cards named ~ in all graveyards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = 1 + count_graveyard_by_id(2, get_id(player, card));
			new_multidiscard(instance->targets[0].player, amount, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_mindslicer(int player, int card, event_t event){
	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		APNAP(p, {new_discard_all(p, player);};);
	}

	return 0;
}

int card_minotaur_explorer(int player, int card, event_t event){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int choice = 1;
		if( hand_count[player] > 0 ){
			choice = do_dialog(player, player, card, -1, -1, " Discard\n Sacrifice", 0);
		}
		if (choice == 0){
			discard(player, DISC_RANDOM, player);
		}
		else{
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_mirari2(int player, int card, event_t event){
/*
Mirari |5
Legendary Artifact
Whenever you cast an instant or sorcery spell, you may pay {3}. If you do, copy that spell. You may choose new targets for the copy.
*/
	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_SPELL);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) ){
			if( has_mana(player, COLOR_ANY, 3) && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 3) ){
				copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
			}
		}
	}

	return 0;
}

int card_molten_influence(int player, int card, event_t event){

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_SPELL);

	if (event == EVENT_RESOLVE_SPELL){
		if (counterspell_validate(player, card, &td, 0)){
			card_instance_t* instance = get_card_instance(player, card);
			int ai_choice = 0;
			if (life[instance->targets[0].player]-4 < 6){
				ai_choice = 1;
			}
			int choice = do_dialog(instance->targets[0].player, player, card, -1, -1, " Take 4 damage\n Let Molten Influence counter the spell", ai_choice);
			if (choice == 0){
				damage_player(instance->targets[0].player, 4, player, card);
			}
			else{
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}

int card_moments_peace(int player, int card, event_t event){
	/* Moment's Peace	|1|G
	 * Instant
	 * Prevent all combat damage that would be dealt this turn.
	 * Flashback |2|G */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XG(2, 1));
	}

	return card_fog(player, card, event);
}

int card_mortivore(int player, int card, event_t event){
	/* Mortivore	|2|B|B
	 * Creature - Lhurgoyf 100/100
	 * ~'s power and toughness are each equal to the number of creature cards in all graveyards.
	 * |B: Regenerate ~. */

	goyf(player, card, event, TYPE_CREATURE);

	return regeneration(player, card, event, MANACOST_B(1));
}

int card_mossfire_egg(int player, int card, event_t event){
	return od_egg(player, card, event, COLOR_GREEN, COLOR_RED);
}

int card_mossfire_valley(int player, int card, event_t event){
	return od_land(player, card, event, COLOR_GREEN, COLOR_RED);
}

int card_muscle_burst(int player, int card, event_t event){
	/* Muscle Burst	|1|G
	 * Instant
	 * Target creature gets +X/+X until end of turn, where X is 3 plus the number of cards named ~ in all graveyards. */

	if( event == EVENT_CHECK_PUMP ){
		int amount = 3 + count_graveyard_by_id(ANYBODY, get_id(player, card)) + count_graveyard_by_id(ANYBODY, CARD_ID_DILIGENT_FARMHAND);
		return vanilla_instant_pump(player, card, event, ANYBODY, player, amount, amount, 0, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = 3 + count_graveyard_by_id(ANYBODY, get_id(player, card)) +
						count_graveyard_by_id(ANYBODY, CARD_ID_DILIGENT_FARMHAND);
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, amount, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_mystic_crusader(int player, int card, event_t event){
	threshold_mod_for_pt_and_abilitie(player, card, event, 1, 1, KEYWORD_FLYING, 0);
	return 0;
}

int card_mystic_enforcer(int player, int card, event_t event){
	if( ! is_unlocked(player, card, event, 1) ){ return 0; }
	threshold_mod_for_pt_and_abilitie(player, card, event, 3, 3, KEYWORD_FLYING, 0);
	return 0;
}

int card_mystic_penitent(int player, int card, event_t event){
	vigilance(player, card, event);
	threshold_mod_for_pt_and_abilitie(player, card, event, 1, 1, KEYWORD_FLYING, 0);
	return 0;
}

int card_mystic_visionary(int player, int card, event_t event){
	threshold_mod_for_pt_and_abilitie(player, card, event, 0, 0, KEYWORD_FLYING, 0);
	return 0;
}

// mystic visionary, mystic zealot --> mystic crusader

int card_nantuko_disciple(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_G(1), 0, &td, "TARGET_CREATURE");
}

int card_nantuko_elder(int player, int card, event_t event){
	return mana_producing_creature_multi(player, card, event, 12, MANACOST_XG(1, 1));
}

int card_nantuko_mentor(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = get_power(instance->targets[0].player, instance->targets[0].card);
			pump_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, amount, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XG(2, 1), 0, &td, "TARGET_CREATURE");
}

int card_nantuko_shrine(int player, int card, event_t event){
	/* Nantuko Shrine	|1|G|G
	 * Enchantment
	 * Whenever a player casts a spell, that player puts X 1/1 |Sgreen Squirrel creature tokens onto the battlefield, where X is the number of cards in all graveyards with the same name as that spell. */

	int amount = od_shrine(player, card, event);
	if( amount ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SQUIRREL, &token);
		token.t_player = trigger_cause_controller;
		token.qty = amount;
		generate_token(&token);
	}

	return global_enchantment(player, card, event);
}

int card_need_for_speed(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_sacrifice_type_as_cost(player, 1, TYPE_LAND) ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, NULL);
		}
	}

	if(event == EVENT_ACTIVATE ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			if( check_state(instance->targets[0].player, instance->targets[0].card, STATE_SUMMON_SICK) ){
				ai_modifier += 24;
				if( current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS ){
					ai_modifier += 24;
				}
			}
			else{
				ai_modifier -= 48;
			}
		}
		else{
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td)  ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_HASTE);
		}
	}

	return 0;
}

int card_nefarious_lich(int player, int card, event_t event ){
/*
Nefarious Lich |B|B|B|B
Enchantment
If damage would be dealt to you, exile that many cards from your graveyard instead. If you can't, you lose the game.
If you would gain life, draw that many cards instead.
When Nefarious Lich leaves the battlefield, you lose the game.
*/

	if( event == EVENT_PREVENT_DAMAGE && in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == -1 && damage->damage_target_player == player && ! damage_is_to_planeswalker(damage) ){
				test_definition_t test;
				default_test_definition(&test, TYPE_ANY);
				while( damage->info_slot && get_grave(player)[0] != -1 ){
						scnprintf(test.message, 100, "Select a card to exile (%d damage to prevent).", damage->info_slot);
						new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 1, AI_MIN_VALUE, &test);
						damage->info_slot--;
				}
				if( damage->info_slot ){
					lose_the_game(player);
				}
			}
		}
	}

	if( leaves_play(player, card, event) ){
		lose_the_game(player);
	}

	return global_enchantment(player, card, event);
}

int card_new_frontiers(int player, int card, event_t event){
/* New Frontiers	|X|G
 * Sorcery
 * Each player may search his or her library for up to X basic land cards and put them onto the battlefield tapped.
 * Then each player who searched his or her library this way shuffles it. */

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = get_card_instance(player, card)->info_slot;
		if( amount > 0 ){
			char options[100];
			scnprintf(options, 100, "Tutor up to %d basic lands\n Decline", amount);
			APNAP(p,{
						if (do_dialog(p, player, card, -1, -1, options, 0) == 0){
							tutor_basic_lands(p, TUTOR_PLAY_TAPPED, amount);
						}
					}
			);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_nimble_mongoose(int player, int card, event_t event){
	threshold_mod_for_pt_and_abilitie(player, card, event, 2, 2, 0, 0);
	return 0;
}

int card_nomad_decoy(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(1), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int ab2 = has_threshold(player) && target_available(player, card, &td) >= 2 &&
					has_mana_for_activated_ability(player, card, MANACOST_W(2)) ? 1 : 0;
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Tap 1 creature", 1, 1,
							"Tap 2 creatures", ab2, 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(choice), 0, &td, NULL);
		if( spell_fizzled != 1 ){
			if( choice == 2 ){
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
				if( ! new_pick_target(&td, "TARGET_CREATURE", 1, 1) ){
					remove_state(player, card, STATE_TAPPED);
				}
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				tap_card(instance->targets[i].player, instance->targets[i].card);
			}
		}
	}

	return 0;
}

int card_nomad_stadium(int player, int card, event_t event){
	/* Nomad Stadium	""
	 * Land
	 * |T: Add |W to your mana pool. ~ deals 1 damage to you.
	 * Threshold - |W, |T, Sacrifice ~: You gain 4 life. Activate this ability only if seven or more cards are in your graveyard. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAC_DAMAGE
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() && has_threshold(player) &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME, MANACOST_W(2), 0, NULL, NULL) )
		{
			choice = CHOICE_SAC_DAMAGE;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_SAC_DAMAGE && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Sac & gain life", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA ){
			mana_producer(player, card, event);
			damage_player(player, 1, player, card);
		}
		if( choice == CHOICE_SAC_DAMAGE ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_W(1), 0, NULL, NULL);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAC_DAMAGE ){
			gain_life(player, 4);
		}
	}

	return 0;
}

int card_nut_collector(int player, int card, event_t event){
	/* Nut Collector	|5|G
	 * Creature - Human Druid 1/1
	 * At the beginning of your upkeep, you may put a 1/1 |Sgreen Squirrel creature token onto the battlefield.
	 * Threshold - Squirrel creatures get +2/+2 as long as seven or more cards are in your graveyard. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		generate_token_by_id(player, card, CARD_ID_SQUIRREL);
	}

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && in_play(player, card) && ! is_humiliated(player, card) &&
		has_threshold(player) )
	{
		boost_creature_type(player, card, event, SUBTYPE_SQUIRREL, 2, 2, 0, BCT_INCLUDE_SELF);
	}

	return 0;
}

int card_overeager_apprentice(int player, int card, event_t event, int color){

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_produce_mana(player, card) && hand_count[player] > 0 ){
			return can_sacrifice_this_as_cost(player, card);
		}
	}

	if(event == EVENT_ACTIVATE ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card to discard");
		int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
		if( selected != -1 ){
			discard_card(player, selected);
			produce_mana(player, COLOR_BLACK, 3);
			kill_card(player, card, KILL_SACRIFICE);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( can_produce_mana(player, card) && hand_count[player] > 0 && can_sacrifice_this_as_cost(player, card) ){
			declare_mana_available(player, COLOR_BLACK, 3);
		}
	}

	return 0;
}

int card_patrol_hound(int player, int card, event_t event){

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade(player, card, event, 0, MANACOST0, 0, 0, KEYWORD_FIRST_STRIKE, 0);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
}

int card_patron_wizard(int player, int card, event_t event){
	/* Patron Wizard	|U|U|U
	 * Creature - Human Wizard 2/2
	 * Tap an untapped Wizard you control: Counter target spell unless its controller pays |1. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.allowed_controller = player;
	td2.preferred_controller = player;
	td2.illegal_abilities = 0;
	td2.illegal_state = TARGET_STATE_TAPPED;
	td2.required_subtype = SUBTYPE_WIZARD;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_target(&td2) ){
		return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( new_pick_target(&td2, "Select an untapped Wizard you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			int wtt = instance->targets[0].card;
			generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST0, 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				tap_card(player, wtt);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, 1);
	}

	return 0;
}

int card_peek(int player, int card, event_t event){
	/* Peek	|U
	 * Instant
	 * Look at target player's hand.
	 * Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 96;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			reveal_target_player_hand(instance->targets[0].player);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_petrified_field(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) || event == EVENT_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land card.");

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_RETURN_LAND
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER) )
		{
			choice = CHOICE_RETURN_LAND;
		}
		if( ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Return a land from grave", 1, 5);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_MANA ){
			return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, 0,
																	NULL, NULL, 0, &test, GS_GRAVE_RECYCLER);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_RETURN_LAND ){
			int selected = validate_target_from_grave_source(player, card, player, 0);
			if( selected != -1 ){
				from_grave_to_hand(player, selected, TUTOR_HAND);
			}
		}
	}

	return 0;
}

int card_phantatog(int player, int card, event_t event){

	/* Phantatog	|1|W|U
	 * Creature - Atog 1/2
	 * Sacrifice an enchantment: ~ gets +1/+1 until end of turn.
	 * Discard a card: ~ gets +1/+1 until end of turn. */

	if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ENCHANTMENT, "");

		int num_can_sac = max_can_sacrifice_as_cost(player, card, &test);
		int num_can_discard = hand_count[player];
		int num_times_can_activate = MAX(0, num_can_sac) + MAX(0, num_can_discard);
		if (num_times_can_activate <= 0){
			return 0;
		}

		return generic_shade_amt_can_pump(player, card, 1, 0, MANACOST0, num_times_can_activate);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			if( hand_count[player] > 0 || can_sacrifice_type_as_cost(player, 1, TYPE_ENCHANTMENT) ){
				return 1;
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			int choice = 0;
			if( hand_count[player] > 0 ){
				if( can_sacrifice_type_as_cost(player, 1, TYPE_ENCHANTMENT) ){
					choice = do_dialog(player, player, card, -1, -1, " Discard\n Sac an enchantment\n Cancel", 1);
				}
			}
			if( choice == 0 ){
				discard(player, 0, player);
			}
			else if( choice == 1 ){
				controller_sacrifices_a_permanent(player, card, TYPE_ENCHANTMENT, SAC_AS_COST);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->parent_controller, c = instance->parent_card;
		if (in_play(p, c)){
			pump_until_eot_merge_previous(p, c, p, c, 1, 1);
		}
	}

	return 0;
}

int card_pianna_nomad_captain(int player, int card, event_t event)
{
  check_legend_rule(player, card, event);

  // Whenever ~ attacks, attacking creatures get +1/+1 until end of turn.
  if (declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.state = STATE_ATTACKING;
	  pump_creatures_until_eot(player, card, current_turn, 0, 1, 1, 0, 0, &test);
	}

  return 0;
}

int card_primal_frenzy(int player, int card, event_t event){
	/*
	  Primal Frenzy |G
	  Enchantment - Aura
	  Enchant creature
	  Enchanted creature has trample.
	*/
	return generic_aura(player, card, event, player, 0, 0, KEYWORD_TRAMPLE, 0, 0, 0, 0);
}

int card_psychatog(int player, int card, event_t event){

	/* Psychatog	|1|U|B
	 * Creature - Atog 1/2
	 * Discard a card: ~ gets +1/+1 until end of turn.
	 * Exile two cards from your graveyard: ~ gets +1/+1 until end of turn. */

	if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST){
		int num_can_discard = hand_count[player];
		int num_times_can_exile = count_graveyard(player) / 2;
		int num_times_can_activate = MAX(0, num_can_discard) + MAX(0, num_times_can_exile);
		if (num_times_can_activate <= 0){
			return 0;
		}

		return generic_shade_amt_can_pump(player, card, 1, 0, MANACOST0, num_times_can_activate);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			if( hand_count[player] > 0 || count_graveyard(player) > 1 ){
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1,
								"Exile from graveyard", count_graveyard(player) > 1, 5,
								"Discard", hand_count[player] > 0, 1);

			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}

			if( choice == 1 ){
				card_instance_t *instance = get_card_instance(player, card);
				if( select_multiple_cards_from_graveyard(player, player, 1, AI_MIN_VALUE, NULL, 2, &instance->targets[0]) ){
					rfg_card_from_grave(player, instance->targets[0].player);
					rfg_card_from_grave(player, instance->targets[1].player);
				}
				else{
					spell_fizzled = 1;
				}
			}

			if( choice == 2 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ANY, "Select a card to discard.");
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &test);
				if( selected > -1 ){
					discard_card(player, selected);
					ai_modifier += (player == AI) ? -48 : 48;
				}
				else{
					spell_fizzled = 1;
				}
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->parent_controller, c = instance->parent_card;
		if (in_play(p, c)){
			pump_until_eot_merge_previous(p, c, p, c, 1, 1);
		}
	}

	return 0;
}

int card_puppeteer(int player, int card, event_t event)
{
  // 0x4110d0

  // |U, |T: You may tap or untap target creature.
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
		twiddle(player, card, 0);

	int rval = generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_U(1), 0, &td, "TARGET_CREATURE");

	if (event == EVENT_ACTIVATE && player == AI && cancel != 1)
		ai_modifier_twiddle(player, card, 0);

	return rval;
}

int card_ray_of_distortion(int player, int card, event_t event){

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XW(4, 2));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
}

int card_reckless_charge(int player, int card, event_t event){
	/* Reckless Charge	|R
	 * Sorcery
	 * Target creature gets +3/+0 and gains haste until end of turn.
	 * Flashback |2|R */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XR(2, 1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			if( check_state(instance->targets[0].player, instance->targets[0].card, STATE_SUMMON_SICK) ){
				ai_modifier += 24;
			}
			if( current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS ){
				ai_modifier += 24;
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3, 0, 0, SP_KEYWORD_HASTE);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_recoup(int player, int card, event_t event ){

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XR(3, 1));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_SPELL, "Select target Instant or Sorcery card.");

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_LAND, -1)*8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		create_spell_has_flashback_legacy(player, card, selected, 0);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &test);
}

int card_repentant_vampire(int player, int card, event_t event){

	/* Repentant Vampire	|3|B|B
	 * Creature - Vampire 3/3
	 * Flying
	 * Whenever a creature dealt damage by ~ this turn dies, put a +1/+1 counter on ~.
	 * Threshold - As long as seven or more cards are in your graveyard, ~ is |Swhite and has "|T: Destroy target |Sblack creature." */

	if( sengir_vampire_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t* instance = get_card_instance(player, card);
		int amount = instance->targets[11].card;
		instance->targets[11].card = 0;
		add_1_1_counters(player, card, amount);
	}

	if( event == EVENT_SET_COLOR && affect_me(player, card) && in_play(player, card) && ! is_humiliated(player, card) ){
		if( has_threshold(player) ){
			event_result = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		}
	}

	if (!IS_GAA_EVENT(event) && event != EVENT_SET_COLOR){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	if( has_threshold(player) ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
	}
	return 0;
}

int card_rites_of_initiation(int player, int card, event_t event){

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1)*8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = discard_multiple_cards_at_once(player, card, hand_count[player]/2);
		pump_subtype_until_eot(player, card, player, -1, amount, 0, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_rites_of_refusal(int player, int card, event_t event){
	/* Rites of Refusal	|1|U
	 * Instant
	 * Discard any number of cards. Counter target spell unless its controller pays |3 for each card discarded this way. */

	if(event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			int amount = discard_multiple_cards_at_once(player, card, 2);
			counterspell_resolve_unless_pay_x(player, card, NULL, 0, 3*amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

int card_rites_of_spring(int player, int card, event_t event){
/* Rites of Spring	|1|G
 * Sorcery
 * Discard any number of cards. Search your library for that many basic land cards, reveal those cards, and put them into your hand.
 * Then shuffle your library. */

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += MIN(hand_count[player]*8, (56-(get_total_available_lands(player)*8)));
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int amount = discard_multiple_cards_at_once(player, card, (4-get_total_available_lands(player)));
		if( amount == 0 ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Your deck.");
			new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_FIRST_FOUND, -1, &test);
			shuffle(player);
		}
		else{
			tutor_basic_lands(player, TUTOR_HAND, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_roar_of_the_wurm(int player, int card, event_t event){
	/* Roar of the Wurm	|6|G
	 * Sorcery
	 * Put a 6/6 |Sgreen Wurm creature token onto the battlefield.
	 * Flashback |3|G */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL){
		generate_token_by_id(player, card, CARD_ID_WURM);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XG(3, 1));
}

int card_rotting_giant(int player, int card, event_t event){
/* CARD_ID_ROTTING_GIANT	5679
Rotting Giant	|1|B
Creature - Zombie Giant 3/3
Whenever ~ attacks or blocks, sacrifice it unless you exile a card from your graveyard. */

	if( in_play(player, card) && ! is_humiliated(player, card)  && ! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) ){
		if( declare_attackers_trigger(player, card, event, 0, player, card) || blocking(player, card, event) )
		{
			test_definition_t this_test;
			new_default_test_definition(&this_test, 0, "Select a card to exile.");
			if( get_grave(player)[0] == -1 ||
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test) == -1)
				kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_sacred_rites(int player, int card, event_t event){
/* CARD_ID_SACRED_RITES	5680
Sacred Rites	|W
Instant
Discard any number of cards. Creatures you control get +0/+1 until end of turn for each card discarded this way. */

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int amount = discard_multiple_cards_at_once(player, card, 3);
		pump_subtype_until_eot(player, card, player, -1, 0, amount, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_sadistic_hypnotist(int player, int card, event_t event){
/* CARD_ID_SADISTIC_HYPNOTIST	5681
Sadistic Hypnotist	|3|B|B
Creature - Human Minion 2/2
Sacrifice a creature: Target player discards two cards. Activate this ability only any time you could cast a sorcery. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			new_multidiscard(instance->targets[0].player, 2, 0, player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE | GAA_CAN_TARGET | GAA_CAN_SORCERY_BE_PLAYED,
									MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_sarcatog(int player, int card, event_t event){

	/* Sarcatog	|1|B|R
	 * Creature - Atog 1/2
	 * Exile two cards from your graveyard: ~ gets +1/+1 until end of turn.
	 * Sacrifice an artifact: ~ gets +1/+1 until end of turn. */

	if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ARTIFACT, "");
		test.not_me = 1;

		int num_can_sac = max_can_sacrifice_as_cost(player, card, &test);
		int num_times_can_exile = count_graveyard(player) / 2;
		int num_times_can_activate = MAX(0, num_can_sac) + MAX(0, num_times_can_exile);
		if (num_times_can_activate <= 0){
			return 0;
		}

		return generic_shade_amt_can_pump(player, card, 1, 0, MANACOST0, num_times_can_activate);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			if( count_graveyard(player) > 1 ){
				return 1;
			}
			return can_sacrifice_type_as_cost(player, 1, TYPE_ARTIFACT);
		}
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Exile from graveyard", count_graveyard(player) > 1, 5,
								"Sac an artifact", can_sacrifice_type_as_cost(player, 1, TYPE_ARTIFACT), 1);

			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			if( choice == 1 ){
				char msg[100] = "Select a card to exile.";
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, msg);
				if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test) != -1 ){
					new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 1, AI_MIN_VALUE, &this_test);
				}
				else{
					spell_fizzled = 1;
				}
			}
			if( choice == 2 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");
				if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
					cancel = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t* instance = get_card_instance(player, card);
		int p = instance->parent_controller, c = instance->parent_card;
		if (in_play(p, c)){
			pump_until_eot_merge_previous(p, c, p, c, 1, 1);
		}
	}

	return 0;
}

int card_setons_desire(int player, int card, event_t event){
/* CARD_ID_SETONS_DESIRE	5690
Seton's Desire	|2|G
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2.
Threshold - As long as seven or more cards are in your graveyard, all creatures able to block enchanted creature do so. */

	int pump = 0;
	if( has_threshold(player) ){
		pump = SP_KEYWORD_LURE;
	}

	return generic_aura(player, card, event, player, 2, 2, 0, pump, 0, 0, 0);
}

int card_seton_krosan_protector(int player, int card, event_t event){
/* CARD_ID_SETON_KROSAN_PROTECTOR	5691
Seton, Krosan Protector	|G|G|G
Legendary Creature - Centaur Druid 2/2
Tap an untapped Druid you control: Add |G to your mana pool. */

	check_legend_rule(player, card, event);
	return tap_a_permanent_you_control_for_mana(player, card, event, TYPE_PERMANENT, SUBTYPE_DRUID, COLOR_GREEN, 1);
}

int card_shadowblood_ridge(int player, int card, event_t event){
/* CARD_ID_SHADOWBLOOD_RIDGE	5693
Shadowblood Ridge	""
Land
|1, |T: Add |B|R to your mana pool. */

	return od_land(player, card, event, COLOR_BLACK, COLOR_RED);
}

int card_shadowblood_egg(int player, int card, event_t event){
/* CARD_ID_SHADOWBLOOD_EGG	5692
Shadowblood Egg	|1
Artifact
|2, |T, Sacrifice ~: Add |B|R to your mana pool. Draw a card. */

	return od_egg(player, card, event, COLOR_BLACK, COLOR_RED);
}

int card_shadowmage_infiltrator(int player, int card, event_t event){
	/* Shadowmage Infiltrator	|1|U|B
	 * Creature - Human Wizard 1/3
	 * Fear
	 * Whenever ~ deals combat damage to a player, you may draw a card. */

	fear(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_TRIGGER_OPTIONAL);
	if( result ){
		int amount = BYTE0(result)+BYTE1(result);
		draw_cards(player, amount);
	}

	return 0;
}

int card_shelter2(int player, int card, event_t event){
/* CARD_ID_SHELTER	5694
Shelter	|1|W
Instant
Target creature you control gains protection from the color of your choice until end of turn.
Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
    td.allowed_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			int key = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, key, 0);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE_YOU_CONTROL", 1, NULL);
}

static int shifty_doppelganger_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		haste(p, c, event);

		if( trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player ){
			if( instance->targets[2].player > 0 ){
				instance->targets[2].player--;
			}
			else{
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						if( ! check_state(p, c, STATE_OUBLIETTED) ){
							state_untargettable(p, c, 1);
							int owner = instance->targets[1].player;
							int iid = instance->targets[1].card;
							if( check_rfg(owner, cards_data[iid].id) ){
								int card_added = add_card_to_hand(owner, iid);
								remove_card_from_rfg(owner, cards_data[iid].id);
								put_into_play(owner, card_added);
							}
							kill_card(p, c, KILL_SACRIFICE);
						}
						else{
							kill_card(player, card, KILL_EXILE);
						}
				}
			}
		}
	}

	return 0;
}

int card_shifty_doppelganger(int player, int card, event_t event){
/*
Shifty Doppelganger |2|U
Creature - Shapeshifter 1/1
{3}{U}, Exile Shifty Doppelganger: You may put a creature card from your hand onto the battlefield.
If you do, that creature gains haste until end of turn. At the beginning of the next end step, sacrifice that creature.
If you do, return Shifty Doppelganger to the battlefield.
*/
	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_XU(3, 1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, 0, MANACOST_XU(3, 1), 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			if( IS_AI(player) ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_CREATURE);
				int result = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
				if( get_cmc(player, result) > 4 ){
					if( (current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS) ||
						(current_turn == 1-player && current_phase == PHASE_DISCARD) )
					{
						ai_modifier += 96;
					}
				}
			}
			card_instance_t *instance = get_card_instance(player, card);
			instance->targets[1].player = get_owner(player, card);
			instance->targets[1].card = get_original_internal_card_id(player, card);
			kill_card(player, card, KILL_EXILE);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature to put into play.");
		int result = new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		if( result > -1 ){
			int legacy = create_targetted_legacy_effect(player, card, &shifty_doppelganger_legacy, player, result);
			card_instance_t *leg = get_card_instance(player, legacy);
			if( current_phase == PHASE_DISCARD ){
				leg->targets[2].player = 1;
			}
			card_instance_t *instance = get_card_instance(player, card);
			leg->targets[1] = instance->targets[1];
		}
	}

	return 0;
}

int card_shower_of_coals(int player, int card, event_t event){
	/* Shower of Coals	|3|R|R	0x2008e64
	 * Sorcery
	 * ~ deals 2 damage to each of up to three targets.
	 * Threshold - ~ deals 4 damage to each of those permanents and/or players instead if seven or more cards are in your graveyard. */
	if (!IS_CASTING(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		int dmg = 2;
		if( has_threshold(player) ){
			dmg = 4;
		}
		for(i=0;i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				damage_creature(instance->targets[i].player, instance->targets[i].card, dmg, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_ANY", 3, NULL);
}

int card_skeletal_scrying(int player, int card, event_t event){
/*
Skeletal Scrying |X|B
Instant
As an additional cost to cast Skeletal Scrying, exile X cards from your graveyard.
You draw X cards and you lose X life.
*/

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			card_instance_t *instance = get_card_instance(player, card);
			instance->info_slot = 0;
			set_special_flags2(player, card, SF2_X_SPELL);
			int diff = true_get_updated_casting_cost(player, card, -1, event, -1);
			int max_mana = has_mana(player, COLOR_ANY, 1) - diff;
			int max = MIN(count_graveyard(player), max_mana);
			if( IS_AI(player) ){
				while( life[player] - max < 6 ){
						max--;
				}
			}
			if( max ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");
				int trc = select_multiple_cards_from_graveyard(player, player, 0, AI_MIN_VALUE, &this_test, max, &instance->targets[0]);
				charge_mana(player, COLOR_COLORLESS, MAX(0, trc + diff));
				if( spell_fizzled != 1 ){
					int i;
					for(i=0; i<trc; i++){
						rfg_card_from_grave(player, instance->targets[i].player);
					}
					instance->info_slot = trc;
				}
			}
			if( instance->info_slot < 2 ){
				ai_modifier -= 96;
			}
			else{
				ai_modifier += (instance->info_slot * 8);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		draw_cards(player, instance->info_slot);
		lose_life(player, instance->info_slot);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_skycloud_expanse(int player, int card, event_t event){
/* CARD_ID_SKYCLOUD_EXPANSE	5701
Skycloud Expanse	""
Land
|1, |T: Add |W|U to your mana pool. */

	return od_land(player, card, event, COLOR_BLUE, COLOR_WHITE);
}

int card_skycloud_eGG(int player, int card, event_t event){
/* CARD_ID_SKYCLOUD_EGG	5700
Skycloud Egg	|1
Artifact
|2, |T, Sacrifice ~: Add |W|U to your mana pool. Draw a card. */

	return od_egg(player, card, event, COLOR_BLUE, COLOR_WHITE);
}

int card_soulcatcher(int player, int card, event_t event){
/* CARD_ID_SOULCATCHER	5703
Soulcatcher	|1|W
Creature - Bird Soldier 1/1
Flying
Whenever a creature with flying dies, put a +1/+1 counter on ~. */

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( result ){
		int amount = BYTE0(result);
		add_1_1_counters(player, card, amount);
	}

	return 0;
}

int card_spark_mage(int player, int card, event_t event){
/* CARD_ID_SPARK_MAGE	5704
Spark Mage	|R
Creature - Dwarf Wizard 1/1
Whenever ~ deals combat damage to a player, you may have ~ deal 1 damage to target creature that player controls. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_TRIGGER_OPTIONAL | DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damage[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					if( times_damage[p] ){
						target_definition_t td;
						default_target_definition(player, card, &td, TYPE_CREATURE );
						td.allowed_controller = td.preferred_controller = p;
						card_instance_t *instance = get_card_instance(player, card);
						if( p == player ){
							if( new_pick_target(&td, "Select target creature you control.", 0, 0) ){
								damage_target0(player, card, 1);
							}
						}
						else{
							if( new_pick_target(&td, "Select target creature you opponent controls.", 0, 0) ){
								damage_target0(player, card, 1);
							}
						}
						instance->number_of_targets = 0;
					}
				}
		);
	}

	return 0;
}

int card_squirrel_mob(int player, int card, event_t event){
/* CARD_ID_SQUIRREL_MOB	5713
Squirrel Mob	|1|G|G
Creature - Squirrel 2/2
~ gets +1/+1 for each other Squirrel on the battlefield. */

	if( (event == EVENT_POWER ||  event == EVENT_TOUGHNESS) && affect_me(player, card) && in_play(player, card) &&
		! is_humiliated(player, card) )
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_SQUIRREL;
		test.not_me = 1;
		event_result += check_battlefield_for_special_card(player, card, ANYBODY, CBFSC_GET_COUNT, &test);
	}

	return 0;
}

int card_squirrel_nest(int player, int card, event_t event){
	/* Squirrel Nest	|1|G|G
	 * Enchantment - Aura
	 * Enchant land
	 * Enchanted land has "|T: Put a 1/1 |Sgreen Squirrel creature token onto the battlefield." */

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return generic_activated_ability(p, c, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SQUIRREL, &token);
		generate_token(&token);
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );
	td.preferred_controller = player;
	td.allowed_controller = player;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target land you control.");
}

int card_stalking_bloodsucker(int player, int card, event_t event){
/* CARD_ID_STALKING_BLOODSUCKER	5715
Stalking Bloodsucker	|4|B|B
Creature - Vampire 4/4
Flying
|1|B, Discard a card: ~ gets +2/+2 until end of turn. */

	if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
	{
		return generic_shade_amt_can_pump(player, card, 2, 0, MANACOST_XB(1, 1), hand_count[player]);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade(player, card, event, 0,  MANACOST_XB(1, 1), 2, 2, 0, 0);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST_XB(1, 1), 0, NULL, NULL);
}

int card_standstill(int player, int card, event_t event){
/* CARD_ID_STANDSTILL	5716
Standstill	|1|U
Enchantment
When a player casts a spell, sacrifice ~. If you do, each of that player's opponents draws three cards. */

	if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, NULL) ){
	    kill_card(player, card, KILL_SACRIFICE);
		draw_cards(1-trigger_cause_controller, 3);
	}

	return global_enchantment(player, card, event);
}

int card_stone_tongue_basilisk(int player, int card, event_t event){
/* CARD_ID_STONE_TONGUE_BASILISK	5720
Stone-Tongue Basilisk	|4|G|G|G
Creature - Basilisk 4/5
Whenever ~ deals combat damage to a creature, destroy that creature at end of combat.
Threshold - As long as seven or more cards are in your graveyard, all creatures able to block ~ do so. */

	if( ! is_humiliated(player, card) ){
		if (trigger_condition == TRIGGER_MUST_BLOCK && has_threshold(player)){
			everybody_must_block_me(player, card, event);
		}

		if (event == EVENT_CHANGE_TYPE && affect_me(player, card) ){
			card_instance_t* instance = get_card_instance(player, card);
			instance->destroys_if_blocked |= DIFB_DESTROYS_UNPROTECTED;
		}

		for_each_creature_damaged_by_me(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE, &lowland_basilisk_effect, player, card);
	}

	return 0;
}

int card_sungrass_prairie(int player, int card, event_t event){
/* CARD_ID_SUNGRASS_PRAIRIE	5722
Sungrass Prairie	""
Land
|1, |T: Add |G|W to your mana pool. */

	return od_land(player, card, event, COLOR_GREEN, COLOR_WHITE);
}

int card_sungrass_egg(int player, int card, event_t event){
/* CARD_ID_SUNGRASS_EGG	5721
Sungrass Egg	|1
Artifact
|2, |T, Sacrifice ~: Add |G|W to your mana pool. Draw a card. */

	return od_egg(player, card, event, COLOR_GREEN, COLOR_WHITE);
}

int card_sylvan_might(int player, int card, event_t event){
	/* Sylvan Might	|1|G
	 * Instant
	 * Target creature gets +2/+2 and gains trample until end of turn.
	 * Flashback |2|G|G */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XG(2, 2));
	}

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2, KEYWORD_TRAMPLE, 0);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
		return 0;
	}

	return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 2, KEYWORD_TRAMPLE, 0);
}

int card_syncopate(int player, int card, event_t event){
	/* Syncopate	|X|U
	 * Instant
	 * Counter target spell unless its controller pays |X. If that spell is countered this way, exile it instead of putting it into its owner's graveyard. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		// That - sign is there for a reason.
		counterspell_resolve_unless_pay_x(player, card, NULL, -KILL_EXILE, instance->info_slot);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL | GS_X_SPELL, NULL, NULL, 1, NULL);
}


int card_tainted_pact(int player, int card, event_t event){
/*
Tainted Pact |1|B
Instant
Exile the top card of your library. You may put that card into your hand unless it has the same name as another card exiled this way.
Repeat this process until you put a card into your hand or you exile two cards with the same name, whichever comes first.
*/
	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( deck_ptr[player][0] != -1 ){
			int exiled[count_deck(player)];
			int ec = 0;
			int stop = -1;
			while( deck_ptr[player][0] != -1 ){
					exiled[ec] = deck_ptr[player][0];
					rfg_top_card_of_deck(player);
					ec++;
					if( ec > 1 ){
						int iid = exiled[ec-1];
						int i;
						for(i=1; i<ec; i++){
							if( exiled[i] == iid ){
								goto end;
							}
						}
					}
					if( IS_AI(player) && (has_subtype_by_id(cards_data[exiled[ec-1]].id, SUBTYPE_BASIC) || ec >= internal_rand(10)+1) )
					{
						stop = 1;
						break;
					}
					stop = show_deck( player, exiled, ec, "Click on a card to stop", 0, 0x7375B0 );
					if( stop > -1 ){
						break;
					}
			}
			end:
				show_deck(1-player, exiled, ec, "Cards exile by Tainted Pact.", 0, 0x7375B0 );
				if( stop > -1 ){
					add_card_to_hand(player, exiled[ec-1]);
					remove_card_from_rfg(player, cards_data[exiled[ec-1]].id);
				}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_terravore(int player, int card, event_t event){
	/* Terravore	|1|G|G
	 * Creature - Lhurgoyf 100/100
	 * Trample
	 * ~'s power and toughness are each equal to the number of land cards in all graveyards. */
	goyf(player, card, event, TYPE_LAND);
	return 0;
}

int card_thaumatog(int player, int card, event_t event){
	/* Thaumatog	|1|G|W
	 * Creature - Atog 1/2
	 * Sacrifice a land: ~ gets +1/+1 until end of turn.
	 * Sacrifice an enchantment: ~ gets +1/+1 until end of turn. */
	return generic_husk(player, card, event, TYPE_LAND | TYPE_ENCHANTMENT, 1, 1, 0, 0);
}

int card_think_tank(int player, int card, event_t event){
/* CARD_ID_THINK_TANK	5732
Think Tank	|2|U
Enchantment
At the beginning of your upkeep, look at the top card of your library. You may put that card into your graveyard. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			if( show_deck( player, deck, 1, "Put this card into your graveyard ?", 0, 0x7375B0 ) != -1 ){
				mill(player, 1);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_thought_devourer(int player, int card, event_t event){
/* CARD_ID_THOUGHT_DEVOURER	5733
Thought Devourer	|2|U|U
Creature - Beast 4/4
Flying
Your maximum hand size is reduced by four. */

	if( event == EVENT_MAX_HAND_SIZE && current_turn == player && in_play(player, card) && ! is_humiliated(player, card) ){
		event_result-=4;
	}
	return 0;
}

int card_thought_eater(int player, int card, event_t event){
/* CARD_ID_THOUGHT_EATER	5734
Thought Eater	|1|U
Creature - Beast 2/2
Flying
Your maximum hand size is reduced by three. */

	if( event == EVENT_MAX_HAND_SIZE && current_turn == player && in_play(player, card) && ! is_humiliated(player, card)  ){
		event_result-=3;
	}
	return 0;
}

int card_thought_nibbler(int player, int card, event_t event){
/* CARD_ID_THOUGHT_NIBBLER	5735
Thought Nibbler	|U
Creature - Beast 1/1
Flying
Your maximum hand size is reduced by two. */

	if( event == EVENT_MAX_HAND_SIZE && current_turn == player && in_play(player, card) && ! is_humiliated(player, card) ){
		event_result-=2;
	}
	return 0;
}

int card_time_stretch(int player, int card, event_t event)
{
  /* Time Stretch	|8|U|U
   * Sorcery
   * Target player takes two extra turns after this one. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	if (event == EVENT_CAN_CAST)
		return can_target(&td);

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
		pick_target(&td, "TARGET_PLAYER");

	if (event == EVENT_RESOLVE_SPELL)
	{
		card_instance_t* instance = get_card_instance(player, card);
		if (valid_target(&td))
		{
			if( instance->targets[0].player == player ){
				time_walk_effect(player, card);
				time_walk_effect(player, card);
			}
			else{
				int fake = add_card_to_hand(instance->targets[0].player, get_original_internal_card_id(player, card));
				time_walk_effect(instance->targets[0].player, fake);
				time_walk_effect(instance->targets[0].player, fake);
				obliterate_card(instance->targets[0].player, fake);
			}
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_tireless_tribe(int player, int card, event_t event){
/* CARD_ID_TIRELESS_TRIBE	5737
Tireless Tribe	|W
Creature - Human Nomad 1/1
Discard a card: ~ gets +0/+4 until end of turn. */

	if (event == EVENT_TOU_BOOST)
	{
		return generic_shade_amt_can_pump(player, card, 4, 0, MANACOST0, hand_count[player]);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
	  card_instance_t* inst = get_card_instance(player, card);
	  if (in_play(inst->parent_controller, inst->parent_card))
		pump_until_eot_merge_previous(player, card, inst->parent_controller, inst->parent_card, 0, 4);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
}

int card_traumatize(int player, int card, event_t event){
/* CARD_ID_TRAUMATIZE	5740
Traumatize	|3|U|U
Sorcery
Target player puts the top half of his or her library, rounded down, into his or her graveyard. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 96;
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			mill( instance->targets[0].player, count_deck(instance->targets[0].player) / 2 );
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_unifying_theory(int player, int card, event_t event){

	/* Unifying Theory	|1|U
	 * Enchantment
	 * Whenever a player casts a spell, that player may pay |2. If the player does, he or she draws a card. */

	if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		if( has_mana(trigger_cause_controller, COLOR_ANY, 2) &&
			charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, trigger_cause_controller, COLOR_ANY, 2) )
		{
			draw_a_card(trigger_cause_controller);
		}
	}

	return global_enchantment(player, card, event);
}

int card_upheaval(int player, int card, event_t event){
/* CARD_ID_UPHEAVAL	1532
Upheaval	|4|U|U
Sorcery
Return all permanents to their owners' hands. */

	if( event == EVENT_RESOLVE_SPELL ){
		// do auras first, so that auras get bounced
		test_definition_t test;
		default_test_definition(&test, TYPE_ENCHANTMENT);
		test.subtype = SUBTYPE_AURA;
		APNAP(p, { new_manipulate_all(player, card, p, &test, ACT_BOUNCE); };);

		default_test_definition(&test, TYPE_PERMANENT);
		APNAP(p, { new_manipulate_all(player, card, p, &test, ACT_BOUNCE); };);

		kill_card(player, card, KILL_SACRIFICE);
	}

	return basic_spell(player, card, event);
}

int card_vampiric_dragon(int player, int card, event_t event){
/* CARD_ID_VAMPIRIC_DRAGON	5746
Vampiric Dragon	|6|B|R
Creature - Vampire Dragon 5/5
Flying
Whenever a creature dealt damage by ~ this turn dies, put a +1/+1 counter on ~.
|1|R: ~ deals 1 damage to target creature. */

	if( sengir_vampire_trigger(player, card, event, 2) ){
		card_instance_t *instance = get_card_instance(player, card);
		int amount = instance->targets[11].card;
		instance->targets[11].card = 0;
		add_1_1_counters(player, card, amount);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, "TARGET_CREATURE");
}

int card_verdant_succession(int player, int card, event_t event){
/* CARD_ID_VERDANT_SUCCESSION	5747
Verdant Succession	|4|G
Enchantment
Whenever a |Sgreen nontoken creature dies, that creature's controller may search his or her library for a card with the same name as that creature
and put it onto the battlefield. If that player does, he or she shuffles his or her library. */

	if( ! is_humiliated(player, card) ){
		if( event == EVENT_GRAVEYARD_FROM_PLAY ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
			count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, 0, &this_test, GFPC_TRACK_DEAD_CREATURES, 0);
		}

		if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			card_instance_t *instance= get_card_instance(player, card);

			int added[10][2];
			int ac = 0;
			int k;
			for(k=0; k<10; k++){
				if( instance->targets[k].player != -1 ){
					added[ac][0] = instance->targets[k].player;
					added[ac][1] = get_card_name(-1, instance->targets[k].card);
					instance->targets[k].player = instance->targets[k].card = -1;
					ac++;
				}
			}
			if( ac ){
				int i;
				for(i=0;i<ac;i++){
					int id = added[i][1];
					char msg[100];
					if( id == -1 ){ // A card with no name, probably a face-down card.
						scnprintf(msg, 100, "You won't be able to tutor any card as you're searching for a card with no name.");
					}
					else{
						scnprintf(msg, 100, "Select a card named %s", cards_ptr[ id ]->name);
					}
					test_definition_t this_test;
					new_default_test_definition(&this_test, TYPE_ANY, msg);
					this_test.id = id;
					new_global_tutor(added[i][0], added[i][0], TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);
				}
			}
			instance->targets[11].player = 0;
		}
	}

	return global_enchantment(player, card, event);
}

int card_volley_of_boulders(int player, int card, event_t event){
	/* Volley of Boulders	|8|R	0x2008ed7
	 * Sorcery
	 * ~ deals 6 damage divided as you choose among any number of targets.
	 * Flashback |R|R|R|R|R|R */
	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_R(6));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int i;
		for(i=0; i<6; i++){
			if( new_pick_target(&td, "TARGET_ANY", i, 1) ){
				if( instance->targets[i].card != -1 ){
					add_state(instance->targets[i].player, instance->targets[i].card, STATE_TARGETTED);
				}
			}
			else{
				break;
			}
		}
		for(i=0; i<instance->number_of_targets; i++){
			if( instance->targets[i].card != -1 ){
				add_state(instance->targets[i].player, instance->targets[i].card, STATE_TARGETTED);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		divide_damage(player, card, &td);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_wayward_angel(int player, int card, event_t event){
/* CARD_ID_WAYWARD_ANGEL	5751
Wayward Angel	|4|W|W
Creature - Angel Horror 4/4
Flying, vigilance
Threshold - As long as seven or more cards are in your graveyard, ~ gets +3/+3, is |Sblack, has trample, and has "At the beginning of your upkeep, sacrifice a creature." */

	threshold_mod_for_pt_and_abilitie(player, card, event, 3, 3, KEYWORD_TRAMPLE, 0);

	if( in_play(player, card) && ! is_humiliated(player, card) && has_threshold(player) ){
		if( event == EVENT_SET_COLOR && affect_me(player, card) ){
			event_result = COLOR_TEST_BLACK;
		}

		upkeep_trigger_ability(player, card, event, player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test);
		}
	}

	return 0;
}

int card_werebear(int player, int card, event_t event){
/* CARD_ID_WEREBEAR	5752
Werebear	|1|G
Creature - Human Bear Druid 1/1
|T: Add |G to your mana pool.
Threshold - ~ gets +3/+3 as long as seven or more cards are in your graveyard. */

	threshold_mod_for_pt_and_abilitie(player, card, event, 3, 3, 0, 0);
	return mana_producing_creature(player, card, event, has_threshold(player) ? 0 : 24, COLOR_GREEN, 1);
}

int card_wild_mongrel(int player, int card, event_t event){
/* CARD_ID_WILD_MONGREL	5755
Wild Mongrel	|1|G
Creature - Hound 2/2
Discard a card: ~ gets +1/+1 and becomes the color of your choice until end of turn. */

	if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
	{
		return generic_shade_amt_can_pump(player, card, 1, 0, MANACOST0, hand_count[player]);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		change_color(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
					1<<choose_a_color(player, get_deck_color(player, 1-player)),
					CHANGE_COLOR_SET|CHANGE_COLOR_END_AT_EOT|CHANGE_COLOR_NO_SLEIGHT);
		return generic_shade(player, card, event, 0,  MANACOST0, 1, 1, 0, 0);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
}

int card_words_of_wisdom(int player, int card, event_t event){
/* CARD_ID_WORDS_OF_WISDOM	5757
Words of Wisdom	|1|U
Instant
You draw two cards, then each other player draws a card. */

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		draw_cards(1-player, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_zombie_infestation(int player, int card, event_t event){
	/* Zombie Infestation	|1|B
	 * Enchantment
	 * Discard two cards: Put a 2/2 |Sblack Zombie creature token onto the battlefield. */

	if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_ZOMBIE);
	}

	return global_enchantment(player, card, event);
}

int card_zoologist(int player, int card, event_t event){
/* CARD_ID_ZOOLOGIST	5761
Zoologist	|3|G
Creature - Human Druid 1/2
|3|G, |T: Reveal the top card of your library. If it's a creature card, put it onto the battlefield. Otherwise, put it into your graveyard. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			show_deck( player, deck, 1, "Here's the first card of deck", 0, 0x7375B0 );
			if( is_what(-1, deck[0], TYPE_CREATURE) ){
				put_into_play_a_card_from_deck(player, player, 0);
			}
			else{
				mill(player, 1);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XG(3, 1), 0, NULL, NULL);
}


