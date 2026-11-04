#include "manalink.h"

// ---- GLOBAL FUNCTIONS

// ---- CARDS

int card_decompose(int player, int card, event_t event);

int card_academy_rector(int player, int card, event_t event)
{
/* CARD_ID_ACADEMY_RECTOR	4091
Academy Rector	|3|W
Creature - Human Cleric 1/2
When ~ dies, you may exile it. If you do, search your library for an enchantment card, put that card onto the battlefield, then shuffle your library. */

  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_AI(player))
	  && exile_from_owners_graveyard(player, card))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_ENCHANTMENT, "Select an enchantment card.");
	  new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
	}

  return 0;
}

int card_aether_sting(int player, int card, event_t event){
/* CARD_ID_AETHER_STING	4096
Aether Sting	|3|R
Enchantment
Whenever an opponent casts a creature spell, ~ deals 1 damage to that player. */

	sp_ability_trigger(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);
	if (event == EVENT_SPA){
		damage_player(1-player, 1, player, card);
	}
	return global_enchantment(player, card, event);
}

int card_apprentice_necromancer(int player, int card, event_t event){
/* CARD_ID_APPRENTICE_NECROMANCER	4092
Apprentice Necromancer	|1|B
Creature - Zombie Wizard 1/1
|B, |T, Sacrifice ~: Return target creature card from your graveyard to the battlefield. That creature gains haste.
At the beginning of the next end step, sacrifice it. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
			create_targetted_legacy_effect(player, card, &haste_and_sacrifice_eot, player, selected);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_B(1), 0, NULL, NULL,
															0, &this_test, GS_GRAVE_RECYCLER);
}

int card_attrition(int player, int card, event_t event){
/* CARD_ID_ATTRITION	4094
Attrition	|1|B|B
Enchantment
|B, Sacrifice a creature: Destroy target non|Sblack creature. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_B(1), 0,
									&td, get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK));
}

int card_aura_thief(int player, int card, event_t event){
/* CARD_ID_AURA_THIEF	4095
Aura Thief	|3|U
Creature - Illusion 2/2
Flying
When ~ dies, you gain control of all enchantments. (You don't get to move Auras.) */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT);
		new_manipulate_all(player, card, 1-player, &this_test, ACT_GAIN_CONTROL);
	}

	return 0;
}

int card_bloodshot_cyclops(int player, int card, event_t event){
	/* Bloodshot Cyclops	|5|R	0x200a372
	 * Creature - Cyclops Giant 4/4
	 * |T, Sacrifice a creature: ~ deals damage equal to the sacrificed creature's power to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0,
															&td, "TARGET_ANY", 0, &test, GFPC_STORE_POWER);
}

int card_body_snatcher(int player, int card, event_t event){

	/* Body Snatcher	|2|B|B
	 * Creature - Minion 2/2
	 * When ~ enters the battlefield, exile it unless you discard a creature card.
	 * When ~ dies, exile ~ and return target creature card from your graveyard to the battlefield. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
		this_test.zone = TARGET_ZONE_HAND;
		if( player == AI && ! check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			ai_modifier -= 1000;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
		this_test.zone = TARGET_ZONE_HAND;
		int result = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
		if( result != -1 ){
			discard_card(player, result);
		}
		else{
			kill_card(player, card, KILL_EXILE);
		}
	}

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
		exile_from_owners_graveyard(player, card);
		if( new_special_count_grave(player, &this_test) && !graveyard_has_shroud(player)){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_CMC, &this_test);
		}
	}

	return 0;
}

int card_brine_seer(int player, int card, event_t event){
	/* Brine Seer	|3|U
	 * Creature - Human Wizard 1/1
	 * |2|U, |T: Reveal any number of |Sblue cards in your hand. Counter target spell unless its controller pays |1 for each card revealed this way. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_BLUE));
	this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
	this_test.zone = TARGET_ZONE_HAND;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int amount = reveal_cards_from_your_hand(player, card, &this_test);
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, amount);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SPELL_ON_STACK, MANACOST_XU(2, 1), 0, NULL, NULL);
}

static int bubbling_muck_effect(int player, int card, event_t event){

	produce_mana_when_subtype_is_tapped(ANYBODY, event, TYPE_PERMANENT, SUBTYPE_SWAMP, COLOR_BLACK, 1);

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_bubbling_muck(int player, int card, event_t event){
/* CARD_ID_BUBBLING_MUCK	4104
Bubbling Muck	|B
Sorcery
Until end of turn, whenever a player taps |Ha Swamp for mana, that player adds |B to his or her mana pool (in addition to the mana the land produces). */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.subtype = SUBTYPE_SWAMP;
		test.state = STATE_TAPPED;
		test.state_flag = DOESNT_MATCH;
		ai_modifier += (check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test) * 8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &bubbling_muck_effect);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_caltrops(int player, int card, event_t event)
{
/* CARD_ID_CALTROPS	4105
Caltrops	|3
Artifact
Whenever a creature attacks, ~ deals 1 damage to it. */

	if (event == EVENT_DECLARE_ATTACKERS){
		store_attackers(player, card, event, DAT_TRACK, ANYBODY, -1, NULL);
	}
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY){
		card_instance_t* instance = get_card_instance(player, card);
		unsigned char* attackers = (unsigned char*)&instance->targets[2].player;
		for (int i = instance->info_slot - 1; i >= 0; --i){
			if (in_play(current_turn, attackers[i])){
				damage_creature(current_turn, attackers[i], 1, player, card);
			}
		}
	}else if (event == EVENT_CHECK_PUMP && current_phase < PHASE_BEFORE_BLOCKING){
		pumpable_toughness[current_turn] -= 1;
	}
	return 0;
}

int card_carnival_of_souls(int player, int card, event_t event){
/* CARD_ID_CARNIVAL_OF_SOULS	4109
Carnival of Souls	|1|B
Enchantment
Whenever a creature enters the battlefield, you lose 1 life and add |B to your mana pool. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);

		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		produce_mana(player, COLOR_BLACK, 1);
		lose_life(player, 1);
	}

	return global_enchantment(player, card, event);
}

int card_cinder_seer(int player, int card, event_t event){
	/* Cinder Seer	|3|R	0x2003b21
	 * Creature - Human Wizard 1/1
	 * |2|R, |T: Reveal any number of |Sred cards in your hand. ~ deals X damage to any target, where X is the number of cards revealed this way. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_RED));
	this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
	this_test.zone = TARGET_ZONE_HAND;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = reveal_cards_from_your_hand(player, card, &this_test);
			damage_target0(player, card, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XR(2, 1), 0, &td, "TARGET_ANY");
}

int card_covetous_dragon( int player, int card, event_t event){
/* CARD_ID_COVETOUS_DRAGON	4114
Covetous Dragon	|4|R
Creature - Dragon 6/5
Flying
When you control no artifacts, sacrifice ~. */

	if( event == EVENT_STATIC_EFFECTS && ! is_humiliated(player, card, AL_STATIC) ){
		if( count_subtype(player, TYPE_ARTIFACT, -1) < 1 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_donate(int player, int card, event_t event){
/* CARD_ID_DONATE	4117
Donate	|2|U
Sorcery
Target player gains control of target permanent you control */

	if( ! is_unlocked(player, card, event, 22) ){ return 0; }

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_PERMANENT);
	td2.allowed_controller = td2.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return can_target(&td2);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			new_pick_target(&td2, "Select target permanent you control.", 1, 1 | GS_LITERAL_PROMPT);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td2, 1) ){
			gain_control_permanently(instance->targets[0].player, instance->targets[1].player, instance->targets[1].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_elvish_piper(int player, int card, event_t event){
/* CARD_ID_ELVISH_PIPER	1027
Elvish Piper	|3|G
Creature - Elf Shaman 1/1
|G, |T: You may put a creature card from your hand onto the battlefield. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		char msg[100] = "Select a creature card.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, msg);

		new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_G(1), 0, NULL, NULL);
}

int card_emperor_crocodile(int player, int card, event_t event){
  /* Emperor Crocodile |3|G
   * 	  Creature - Crocodile 5/5
   * 	  When you control no other creatures, sacrifice Emperor Crocodile. */

	if( event == EVENT_STATIC_EFFECTS && ! is_humiliated(player, card, AL_STATIC) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		if( ! check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_eradicate(int player, int card, event_t event){

	/* Eradicate	|2|B|B
	 * Sorcery
	 * Exile target non|Sblack creature. Search its controller's graveyard, hand, and library for all cards with the same name as that creature and exile
	 * them. Then that player shuffles his or her library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int id = get_id(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			lobotomy_effect(player, instance->targets[0].player, id, -1, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, NULL);
}

int card_extruder(int player, int card, event_t event){
  /* Extruder |4
   * Artifact Creature - Juggernaut 4/3
   * Echo {4} (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep,
   * 		sacrifice it unless you pay its echo cost.)
   * Sacrifice an artifact: Put a +1/+1 counter on target creature. */

	echo(player, card, event, MANACOST_X(4));

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE",
															0, &test, 0);
}

int card_false_prophet(int player, int card, event_t event)
{
  /* CARD_ID_FALSE_PROPHET	4123
False Prophet	|2|W|W
Creature - Human Cleric 2/2
When ~ dies, exile all creatures. */

  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_CREATURE);
    new_manipulate_all(player, card, ANYBODY, &test, ACT_KILL_EXILE);
  }
  return 0;
}

int card_fend_off(int player, int card, event_t event){
/* CARD_ID_FEND_OFF	4125
Fend Off	|1|W
Instant
Prevent all combat damage that would be dealt by target creature this turn.
Cycling |2 (|2, Discard this card: Draw a card.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return us_cycling(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			negate_combat_damage_this_turn(player, card, instance->targets[0].player, instance->targets[0].card, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_field_surgeon(int player, int card, event_t event){
/* CARD_ID_FIELD_SURGEON	4127
Field Surgeon	|1|W
Creature - Human Cleric 1/1
Tap an untapped creature you control: Prevent the next 1 damage that would be dealt to target creature this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_abilities = 0;
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.extra = damage_card;
	td1.illegal_abilities = 0;
	td1.special = TARGET_SPECIAL_DAMAGE_CREATURE;
	td1.allow_cancel = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_target(&td) ){
			return generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0, &td1, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int sac = -1;
		new_pick_target(&td, "Select target creature to tap.", 0, 1 | GS_LITERAL_PROMPT);
		if( spell_fizzled == 1 ){
			return 0;
		}
		sac = instance->targets[0].card;
		instance->number_of_targets = 0;
		add_state(player, sac, STATE_TARGETTED);
		generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST0, 0, &td1, "TARGET_DAMAGE");
		remove_state(player, sac, STATE_TARGETTED);
		if( spell_fizzled != 1 ){
			tap_card(player, sac);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td1, 0) ){
			prevent_damage(instance->targets[0].player,  instance->targets[0].card, 1);
		}
	}

	return 0;
}

int card_flame_jet(int player, int card, event_t event){
	/* Flame Jet	|1|R	0x200a37c
	 * Sorcery
	 * ~ deals 3 damage to target player or planeswalker.
	 * Cycling |2 */
	if( ! IS_GS_EVENT(player, card, event) ){
		return us_cycling(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
}

int card_fledgling_osprey(int player, int card, event_t event){
/* CARD_ID_FLEDGLING_OSPREY	4129
Fledgling Osprey	|U
Creature - Bird 1/1
~ has flying as long as it's enchanted. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && is_enchanted(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		event_result |= KEYWORD_FLYING;
	}

	return 0;
}

static const char* target_is_not_token(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  // As there's no way to require a given target to have more than one of the types in required_type, we can't just use TARGET_TYPE_TOKEN.
  return ! is_token(player, card) ? NULL : "can't target a token.";
}

int card_flicker2(int player, int card, event_t event){
	/* Flicker	|1|W
	 * Sorcery
	 * Exile target nontoken permanent, then return it to the battlefield under its owner's control. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	base_target_definition(player, card, &td, TYPE_PERMANENT);
	td.preferred_controller = player;
	td.allow_cancel = 0;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(target_is_not_token);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nontoken permanent.", 1, NULL);

	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			blink_effect(instance->targets[0].player, instance->targets[0].card, 0);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_fodder_cannon(int player, int card, event_t event){
  /* Fodder Cannon |4
   * 	  Artifact
   * 	  {4}, {T}, Sacrifice a creature: Fodder Cannon deals 4 damage to target creature. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allow_cancel = 0;;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
		damage_target0(player, card, 4);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE, MANACOST_X(4), 0,
									&td, "TARGET_CREATURE");
}

int card_gamekeeper(int player, int card, event_t event)
{
/* CARD_ID_GAMEKEEPER	4131
Gamekeeper	|3|G
Creature - Elf 2/2
When ~ dies, you may exile it. If you do, reveal cards from the top of your library until you reveal a creature card.
Put that card onto the battlefield and put all other cards revealed this way into your graveyard. */

  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_AI(player))
	  && exile_from_owners_graveyard(player, card))
	{
	  int* deck = deck_ptr[player];
	  int z;
	  for (z = 0; deck[z] != -1 && z < 500; z++)
		if (cards_data[deck[z]].type & TYPE_CREATURE)
		  {
			int iid = deck[z];
			remove_card_from_deck(player, z);
			if (z > 0)
			  mill(player, z);

			put_into_play(player, add_card_to_hand(player, iid));
			return 0;
		  }

	  // Not found - mill them all!
	  mill(player, count_deck(player));
	}

  return 0;
}

int card_goblin_marshall(int player, int card, event_t event){
	/* Goblin Marshal	|4|R|R
	 * Creature - Goblin Warrior 3/3
	 * Echo |4|R|R
	 * When ~ enters the battlefield or dies, put two 1/1 |Sred Goblin creature tokens onto the battlefield. */

	echo(player, card, event, MANACOST_XR(4, 2));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_tokens_by_id(player, card, CARD_ID_GOBLIN, 2);
	}

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		generate_tokens_by_id(player, card, CARD_ID_GOBLIN, 2);
	}

	return 0;
}

int card_heart_warden(int player, int card, event_t event){
/* CARD_ID_HEART_WARDEN	4138
Heart Warden	|1|G
Creature - Elf Druid 1/1
|T: Add |G to your mana pool.
|2, Sacrifice ~: Draw a card. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producing_creature(player, card, event, 24, COLOR_GREEN, 1);
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_DRAW
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME, MANACOST_X(3), 0, NULL, NULL) )
		{
			choice = CHOICE_DRAW;
		}
		if( choice == CHOICE_DRAW && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Sac & draw a card", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_DRAW ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, EVENT_ACTIVATE, GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DRAW ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_hunting_moa(int player, int card, event_t event){
/* CARD_ID_HUNTING_MOA	4140
Hunting Moa	|2|G
Creature - Bird Beast 3/2
Echo |2|G (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep,
sacrifice it unless you pay its echo cost.)
When ~ enters the battlefield or dies, put a +1/+1 counter on target creature. */

	echo(player, card, event, MANACOST_XG(2, 1));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_impatience(int player, int card, event_t event){
/* CARD_ID_IMPATIENCE	4142
Impatience	|2|R
Enchantment
At the beginning of each player's end step, if that player didn't cast a spell this turn, ~ deals 2 damage to him or her. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		instance->targets[current_turn+1].player = get_specific_storm_count(current_turn);
	}

	if( event == EVENT_CAST_SPELL && ! affect_me(player, card) && ! is_what(affected_card_controller, affected_card, TYPE_LAND) &&
		affected_card_controller == current_turn )
	{
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->targets[current_turn+1].player < 0 ){
			instance->targets[current_turn+1].player = 0;
		}
		instance->targets[current_turn+1].player++;
	}

	if( trigger_condition == TRIGGER_EOT &&  affect_me(player, card ) && reason_for_trigger_controller == player &&
		! is_humiliated(player, card, AL_TRIG) )
	{
		card_instance_t *instance = get_card_instance(player, card);
		int trigs = instance->targets[current_turn+1].player < 1 ? 1 : 0;
		instance->targets[current_turn+1].player = 0;
		if( eot_trigger_mode(player, card, event, ANYBODY, (trigs ? RESOLVE_TRIGGER_MANDATORY : 0)) ){
			damage_player(current_turn, 2, player, card);
		}
	}

	return global_enchantment(player, card, event);
}

int card_iridescent_drake(int player, int card, event_t event){

	/* Iridescent Drake	|3|U
	 * Creature - Drake 2/2
	 * Flying
	 * When ~ enters the battlefield, put target Aura card from a graveyard onto the battlefield under your control attached to ~. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if (event == EVENT_ETB_ABILITY){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ENCHANTMENT, "Select target aura card with enchant creature or permanent");
		test.subtype = SUBTYPE_AURA_CREATURE;
		test.sub2 = SUBTYPE_AURA_PERMANENT;
		test.subtype_flag = F2_MULTISUBTYPE;
		if (select_target_from_either_grave(player, card, SFG_CANNOT_CANCEL, AI_MAX_CMC, AI_MAX_CMC, &test, 0, 1) != -1){
			card_instance_t* instance = get_card_instance(player, card);
			int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
			if (selected != -1){
				reanimate_permanent(player, card, instance->targets[0].player, selected, REANIMATE_ATTACH_AURA_TO_REANIMATOR);
			}
		}
	}
	return 0;
}

int card_ivy_seer(int player, int card, event_t event){
/* CARD_ID_IVY_SEER	4145
Ivy Seer	|3|G
Creature - Elf Wizard 1/1
|2|G, |T: Reveal any number of |Sgreen cards in your hand. Target creature gets +X/+X until end of turn, where X is the number of cards revealed this way. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_GREEN));
	this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = reveal_cards_from_your_hand(player, card, &this_test);
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, amount, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XG(2, 1), 0, &td, "TARGET_CREATURE");
}

int card_jasmine_seer(int player, int card, event_t event){
/* CARD_ID_JASMINE_SEER	4146
Jasmine Seer	|3|W
Creature - Human Wizard 1/1
|2|W, |T: Reveal any number of |Swhite cards in your hand. You gain 2 life for each card revealed this way. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_WHITE));
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		this_test.zone = TARGET_ZONE_HAND;
		int amount = reveal_cards_from_your_hand(player, card, &this_test);
		gain_life(player, 2*amount);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XW(2, 1), 0, NULL, NULL);
}

int card_junk_diver(int player, int card, event_t event)
{
/* CARD_ID_JUNK_DIVER	4147
Junk Diver	|3
Artifact Creature - Bird 1/1
Flying
When ~ dies, return another target artifact card from your graveyard to your hand. */

  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact card.");
	  tutor_from_grave_to_hand_except_for_dying_card(player, card, 1, AI_MAX_CMC, &test);
	}

  return 0;
}

int card_keldon_champion(int player, int card, event_t event){
	/* Keldon Champion	|2|R|R	0x2003b62
	 * Creature - Human Barbarian 3/2
	 * Haste
	 * Echo |2|R|R
	 * When ~ enters the battlefield, it deals 3 damage to target player or planeswalker. */
	echo(player, card, event, MANACOST_XR(2, 2));

	haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER")){
			damage_target0(player, card, 3);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_keldon_vandals(int player, int card, event_t event){
/* CARD_ID_KELDON_VANDALS	4149
Keldon Vandals	|2|R
Creature - Human Rogue 4/1
Echo |2|R (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)
When ~ enters the battlefield, destroy target artifact. */

	echo(player, card, event, MANACOST_XR(2, 1));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_ARTIFACT);
		td1.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td1) && pick_target(&td1, "TARGET_ARTIFACT") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_marker_beetle(int player, int card, event_t event){
/* CARD_ID_MARKER_BEETLES	4156
Marker Beetles	|1|G|G
Creature - Insect 2/3
When ~ dies, target creature gets +1/+1 until end of turn.
|2, Sacrifice ~: Draw a card. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.preferred_controller = player;
		td1.allow_cancel = 0;

		card_instance_t *instance= get_card_instance(player, card);
		if( can_target(&td1) && pick_target(&td1, "TARGET_CREATURE") ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL);
}

int card_master_healer(int player, int card, event_t event){
	/* Master Healer	|4|W	0x200a390
	 * Creature - Human Cleric 1/4
	 * |T: Prevent the next 4 damage that would be dealt to any target this turn. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 4);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST0, 0,
									&td, "TARGET_DAMAGE");
}

int card_masticore(int player, int card, event_t event){
/* CARD_ID_MASTICORE	1230
Masticore	|4
Artifact Creature - Masticore 4/4
At the beginning of your upkeep, sacrifice ~ unless you discard a card.
|2: ~ deals 1 damage to target creature.
|2: Regenerate ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int kill = 1;
		if( hand_count[player] > 0 ){
			int ai_choice = 0;
			if( (!can_use_activated_abilities(player, card) && !can_attack(player, card)) ||
				hand_count[player] < count_upkeeps(player)	// In second and subsequent upkeeps, this compares handcount after discard from prior upkeeps against total number of upkeeps, not number remaining
			 ){
				ai_choice = 1;
			}
			int choice = do_dialog(player, player, card, -1, -1, " Pay upkeep\n Pass", ai_choice);
			if( choice == 0 ){
				discard(player, 0, player);
				kill--;
			}
		}
		if( kill == 1 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_REGENERATION, MANACOST_X(2), 0, NULL, NULL) ){
			return 99;
		}
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(2), 0, &td, NULL) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			int choice = 67;
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_REGENERATION, MANACOST_X(2), 0, NULL, NULL) ){
				choice = 66;
			}
			else{
				pick_target(&td, "TARGET_CREATURE");
			}
			instance->info_slot = choice;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 && can_regenerate(instance->parent_controller, instance->parent_card) > 0 ){
			regenerate_target(instance->parent_controller, instance->parent_card);
		}
		if( instance->info_slot == 67 && valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}
	return 0;
}

int card_mental_discipline(int player, int card, event_t event){
/* CARD_ID_MENTAL_DISCIPLINE	4158
Mental Discipline	|1|U|U
Enchantment
|1|U, Discard a card: Draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST_XU(1, 1), 0, NULL, NULL);
}

int card_metalworker(int player, int card, event_t event){
/* CARD_ID_METALWORKER	4159
Metalworker	|3
Artifact Creature - Construct 1/2
|T: Reveal any number of artifact cards in your hand. Add |C|C to your mana pool for each card revealed this way. */

	if( IS_GAA_EVENT(event) || (event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an artifact card.");
		this_test.zone = TARGET_ZONE_HAND;

		if( event == EVENT_COUNT_MANA && affect_me(player, card) && mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
			declare_mana_available(player, COLOR_COLORLESS, 2 * check_battlefield_for_special_card(player, card, player, 0, &this_test));
		}

		if(event == EVENT_CAN_ACTIVATE ){
			return mana_producer(player, card, event);
		}

		if( event == EVENT_ACTIVATE ){
			int result = reveal_cards_from_your_hand(player, card, &this_test);
			produce_mana_tapped(player, card, COLOR_COLORLESS, 2*result);
		}
	}

	return 0;
}

int card_metathran_elite(int player, int card, event_t event){
/* CARD_ID_METATHRAN_ELITE	4160
Metathran Elite	|1|U|U
Creature - Metathran Soldier 2/3
~ can't be blocked as long as it's enchanted. */

	if(event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card, AL_STATIC) ){
		if( attacking_card_controller == player && attacking_card == card ){
			if( is_enchanted(player, card) ){
				event_result = 1;
			}
		}
	}

	return 0;
}

int card_multanis_decree(int player, int card, event_t event){
/* CARD_ID_MULTANIS_DECREE	4163
Multani's Decree	|3|G
Sorcery
Destroy all enchantments. You gain 2 life for each enchantment destroyed this way. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT);

		int result = new_manipulate_all(player, card, ANYBODY, &this_test, KILL_DESTROY);
		gain_life(player, result*2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_nightshade_seer(int player, int card, event_t event){
/* CARD_ID_NIGHTSHADE_SEER	4164
Nightshade Seer	|3|B
Creature - Human Wizard 1/1
|2|B, |T: Reveal any number of |Sblack cards in your hand. Target creature gets -X/-X until end of turn, where X is the number of cards revealed this way. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_BLACK));
	this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = reveal_cards_from_your_hand(player, card, &this_test);
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -amount, -amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XB(2, 1), 0, &td, "TARGET_CREATURE");
}

int card_opalescence(int player, int card, event_t event){

	/* Opalescence	|2|W|W
	 * Enchantment
	 * Each other non-Aura enchantment is a creature with power and toughness each equal to its converted mana cost. It's still an enchantment. */

	if( event == EVENT_CHANGE_TYPE && in_play(player, card) && is_what(affected_card_controller, affected_card, TYPE_ENCHANTMENT) &&
		! has_subtype(affected_card_controller, affected_card, SUBTYPE_AURA) && ! is_humiliated(player, card, AL_STATIC) &&
		! affect_me(player, card) )
	{
		card_instance_t *instance = get_card_instance( player, card );
		card_instance_t *lnd = get_card_instance(affected_card_controller, affected_card);
		int found = 0;
		int i;
		for(i=0; i<instance->info_slot; i++){
			if( lnd->internal_card_id == instance->targets[i].player ){
				event_result = instance->targets[i].card;
				found = 1;
			}
		}
		if( ! found ){
			int pos = instance->info_slot;
			if( pos < 10 ){
				int newtype = create_a_card_type(lnd->internal_card_id);
				cards_at_7c7000[newtype]->type |= (cards_data[lnd->internal_card_id].type | TYPE_CREATURE);
				cards_at_7c7000[newtype]->power = get_cmc(affected_card_controller, affected_card);
				cards_at_7c7000[newtype]->toughness = get_cmc(affected_card_controller, affected_card);
				instance->targets[pos].player = lnd->internal_card_id;
				instance->targets[pos].card = newtype;
				instance->info_slot++;
				event_result = newtype;
			}
		}
	}

	if( event == EVENT_CLEANUP && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		int i;
		for(i=0; i<10; i++){
			instance->targets[i].player = instance->targets[i].card = -1;
		}
		instance->info_slot = 0;
	}

	return global_enchantment(player, card, event);
}

int card_opposition(int player, int card, event_t event){
/* CARD_ID_OPPOSITION	4166
Opposition	|2|U|U
Enchantment
Tap an untapped creature you control: Tap target artifact, creature, or land. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;
	td.illegal_state = TARGET_STATE_TAPPED;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE | TYPE_LAND | TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td2) ){
			tap_card( instance->targets[0].player, instance->targets[0].card );
		}
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td2, "ICY_MANIPULATOR",
															&td, 1);
}

int card_pattern_of_rebirth(int player, int card, event_t event){
	/* Pattern of Rebirth	|3|G
	 * Enchantment - Aura
	 * Enchant creature
	 * When enchanted creature dies, that creature's controller may search his or her library for a creature card and put that card onto the battlefield. If that player does, he or she shuffles his or her library. */

	if (attached_creature_dies_trigger_for_controller(player, card, event, RESOLVE_TRIGGER_MANDATORY)){
		int controller = get_card_instance(player, card)->damage_target_player;
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
		new_global_tutor(controller, controller, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
	}
	return aura(player, card, event, NULL, NULL);
}

void true_phyrexian_negator(int player, int card, event_t event)
{
  int32_t damage_amount;
  int attacker_power;
  card_instance_t *instance;
  test_definition_t test;
  if (event == EVENT_CHANGE_TYPE)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      if (is_humiliated(player, card, AL_TRIG) == 0)
      {
        instance = get_card_instance(player, card);
        instance->destroys_if_blocked = instance->destroys_if_blocked | 1;
      }
    }
  }
  else if (((event == EVENT_CHECK_DESTROY_IF_BLOCKED) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    if (is_humiliated(player, card, AL_TRIG) == 0)
    {
      attacker_power = get_power(attacking_card_controller, attacking_card);
      if (0 < attacker_power)
      {
        event_result = event_result | 2;
      }
    }
  }
  if (is_humiliated(player, card, AL_TRIG) == 0)
  {
    if (damage_dealt_to_me_arbitrary(player, card, event, DDBM_0, player, card) != 0)
    {
      instance = get_card_instance(player, card);
      damage_amount = instance->targets[7].card;
      instance = get_card_instance(player, card);
      instance->targets[7].card = 0;
      new_default_test_definition(&test, 0x47, "Select a permanent to sacrifice.");
      test.qty = (unsigned short)damage_amount;
      new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test);
      return;
    }
  }
  return;
}

int card_phyrexian_negator(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_NEGATOR	4169
Phyrexian Negator	|2|B
Creature - Horror 5/5
Trample
Whenever ~ is dealt damage, sacrifice that many permanents. */

	if( in_play(player, card) && ! is_unlocked(player, card, event, 32) ){
		kill_card(player, card, KILL_SACRIFICE);
		return 0;
	}
	true_phyrexian_negator(player, card, event);
	return 0;
}

int card_plow_under(int player, int card, event_t event){
/* CARD_ID_PLOW_UNDER	1117
Plow Under	|3|G|G
Sorcery
Put two target lands on top of their owners' libraries. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				put_on_top_of_deck(instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 2, NULL);
}

int card_powder_keg(int player, int card, event_t event)
{
  /* Powder Keg	|2
   * Artifact
   * At the beginning of your upkeep, you may put a fuse counter on ~.
   * |T, Sacrifice ~: Destroy each artifact and creature with converted mana cost equal to the number of fuse counters on ~. */

  if (trigger_condition == TRIGGER_UPKEEP && affect_me(player, card))
	{
	  int mode;
	  if (!IS_AI(player))
		mode = RESOLVE_TRIGGER_OPTIONAL;
	  else if (count_counters(player, card, COUNTER_FUSE) >= get_highest_cmc(1-player, TYPE_CREATURE|TYPE_ARTIFACT))
		mode = 0;
	  else
		mode = RESOLVE_TRIGGER_MANDATORY;

	  upkeep_trigger_ability_mode(player, card, event, player, mode);
	}

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	add_counters(player, card, player, card, COUNTER_FUSE, 1);

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE | TYPE_ARTIFACT, "");
	  test.cmc = count_counters(player, card, COUNTER_FUSE);
	  new_manipulate_all(player, card, ANYBODY, &test, KILL_DESTROY);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_SACRIFICE_ME, MANACOST_X(0), 0, NULL, NULL);
}

int card_quash(int player, int card, event_t event){

	/* Quash	|2|U|U
	 * Instant
	 * Counter target instant or sorcery spell. Search its controller's graveyard, hand, and library for all cards with the same name as that spell and exile
	 * them. Then that player shuffles his or her library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_SPELL);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int id = get_id(instance->targets[0].player, instance->targets[0].card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			lobotomy_effect(player, instance->targets[0].player, id, -1, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}

int card_rapid_decay(int player, int card, event_t event)
{
  /* Rapid Decay	|1|B
   * Instant
   * Exile up to three target cards from a single graveyard.
   * Cycling |2 */

  if (card_decompose(player, card, event))
	return 1;

  return cycling(player, card, event, MANACOST_X(2));
}

int card_ravenous_rats(int player, int card, event_t event){
  /* Ravenous Rats |1|B
   * 	  Creature - Rat 1/1
   * 	  When Ravenous Rats enters the battlefield, target opponent discards a card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;

		if( would_validate_arbitrary_target(&td, 1-player, -1) && hand_count[1-player] ){
			discard(1-player, 0, player);
		}
		get_card_instance(player, card)->number_of_targets = 0;
	}

	return 0;
}

int card_rayne_academy_chancellor(int player, int card, event_t event)
{
/* CARD_ID_RAYNE_ACADEMY_CHANCELLOR	4176
Rayne, Academy Chancellor	|2|U
Legendary Creature - Human Wizard 1/1
Whenever you or a permanent you control becomes the target of a spell or ability an opponent controls, you may draw a card.
You may draw an additional card if ~ is enchanted. */

  const target_t* targets = any_becomes_target(player, card, event, player, 1, TYPE_PERMANENT, -1, TYPE_EFFECT|TYPE_NONEFFECT, 1-player, RESOLVE_TRIGGER_AI(player));
  if (targets)
	{
	  int num = 0;
	  for (; targets->player != -1; ++targets)
		++num;

	  if (is_enchanted(player, card))
		num *= 2;

	  if (num == 1)
		draw_cards(player, 1);
	  else
		draw_up_to_n_cards(player, num);
	}

  return 0;
}

int card_reckless_abandon(int player, int card, event_t event){
	/* Reckless Abandon	|R	0x2003ba8
	 * Sorcery
	 * As an additional cost to cast this spell, sacrifice a creature.
	 * ~ deals 4 damage to any target. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_SAC_CREATURE_AS_COST, &td, "TARGET_ANY", 1, NULL);
}

int card_repercussion(int player, int card, event_t event){
/* CARD_ID_REPERCUSSION	4179
Repercussion	|1|R|R
Enchantment
Whenever a creature is dealt damage, ~ deals that much damage to that creature's controller. */

	if( ! is_humiliated(player, card, AL_TRIG) ){
		if( event == EVENT_DEAL_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card && damage->damage_target_card != -1){
				card_instance_t *instance = get_card_instance(player, card);
				int dmg = damage->info_slot;
				if( dmg > 0 ){
					int p = damage->damage_target_player;
					if( instance->targets[p].player < 0 ){
						instance->targets[p].player = 0;
					}
					instance->targets[p].player += dmg;
					instance->info_slot++;
				}
			}
		}

		if( get_card_instance(player, card)->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
			reason_for_trigger_controller == affected_card_controller )
		{
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if( event == EVENT_RESOLVE_TRIGGER ){
					card_instance_t *instance = get_card_instance(player, card);
					int dmg[2] = {
									MIN(instance->targets[0].player, 0),
									MIN(instance->targets[1].player, 0)
					};
					instance->targets[0].player = instance->targets[1].player = 0;
					instance->info_slot = 0;
					APNAP(p, { damage_player(p, dmg[p], player, card); };);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_reliquary_monk(int player, int card, event_t event){
/* CARD_ID_RELIQUARY_MONK	4178
Reliquary Monk	|2|W
Creature - Human Monk Cleric 2/2
When ~ dies, destroy target artifact or enchantment. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && pick_target(&td, "TARGET_ARTIFACT_ENCHANTMENT")){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_replenish(int player, int card, event_t event){
/* CARD_ID_REPLENISH	4180
Replenish	|3|W
Sorcery
Return all enchantment cards from your graveyard to the battlefield. (Auras with nothing to enchant remain in your graveyard.) */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ENCHANTMENT);
		new_reanimate_all(player, card, player, &test, REANIMATE_DEFAULT);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_rofellos_gift(int player, int card, event_t event){
/* CARD_ID_ROFELLOSS_GIFT	4182
Rofellos's Gift	|G
Sorcery
Reveal any number of |Sgreen cards in your hand. Return an enchantment card from your graveyard to your hand for each card revealed this way. */

	if( event == EVENT_RESOLVE_SPELL ){
		char msg[100] = "Select an enchantment card";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ENCHANTMENT, msg);

		char msg2[100] = "Select an enchantment card";
		test_definition_t this_test2;
		new_default_test_definition(&this_test2, TYPE_ANY, msg2);
		this_test2.color = COLOR_TEST_GREEN;

		int amount = reveal_cards_from_your_hand(player, card, &this_test2);
		while( amount > 0 ){
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
				amount--;
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_rofellos_llanowar_emissary(int player, int card, event_t event){
/* CARD_ID_ROFELLOS_LLANOWAR_EMISSARY	1533
Rofellos, Llanowar Emissary	|G|G
Legendary Creature - Elf Druid 2/1
|T: Add |G to your mana pool for each |H2Forest you control. */

	check_legend_rule(player, card, event);

	int count = 0;
	if (event == EVENT_COUNT_MANA || event == EVENT_ACTIVATE || (event == EVENT_CAN_ACTIVATE && player == AI)){	// Only count if it'll actually be needed
		count = count_subtype(player, TYPE_LAND, SUBTYPE_FOREST);
	}

	return mana_producing_creature(player, card, event, 48, COLOR_GREEN, count);
}

int card_scour(int player, int card, event_t event){

	/* Scour	|2|W|W
	 * Instant
	 * Exile target enchantment. Search its controller's graveyard, hand, and library for all cards with the same name as that enchantment and exile them. Then
	 * that player shuffles his or her library. */

	if( ! IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int id = get_id(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			lobotomy_effect(player, instance->targets[0].player, id, -1, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT", 1, NULL);
}

int card_scent_of_brine(int player, int card, event_t event){
	/* Scent of Brine	|1|U
	 * Instant
	 * Reveal any number of |Sblue cards in your hand. Counter target spell unless its controller pays |1 for each card revealed this way. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_BLUE));
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		this_test.zone = TARGET_ZONE_HAND;
		int amount = reveal_cards_from_your_hand(player, card, &this_test);

		counterspell_resolve_unless_pay_x(player, card, NULL, 0, amount);

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

int card_scent_of_cinder(int player, int card, event_t event){
	/* Scent of Cinder	|1|R	0x200a3bd
	 * Sorcery
	 * Reveal any number of |Sred cards in your hand. ~ deals X damage to any target, where X is the number of cards revealed this way. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_RED));
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
			this_test.zone = TARGET_ZONE_HAND;
			int amount = reveal_cards_from_your_hand(player, card, &this_test);
			damage_target0(player, card, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY" , 1, NULL);
}

int card_scent_of_ivy(int player, int card, event_t event){
/* CARD_ID_SCENT_OF_IVY	4185
Scent of Ivy	|G
Instant
Reveal any number of |Sgreen cards in your hand. Target creature gets +X/+X until end of turn, where X is the number of cards revealed this way. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_GREEN));
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
			this_test.zone = TARGET_ZONE_HAND;
			int amount = reveal_cards_from_your_hand(player, card, &this_test);
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, amount, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE" , 1, NULL);
}

int card_scent_of_jasmine(int player, int card, event_t event){
/* CARD_ID_SCENT_OF_JASMINE	4186
Scent of Jasmine	|W
Instant
Reveal any number of |Swhite cards in your hand. You gain 2 life for each card revealed this way. */

	if( event == EVENT_RESOLVE_SPELL ){
		char buffer[100];
		scnprintf(buffer, 100, "Select a white card");
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_WHITE));
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		this_test.zone = TARGET_ZONE_HAND;
		int amount = reveal_cards_from_your_hand(player, card, &this_test);
		gain_life(player, amount*2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_scent_of_nightshade(int player, int card, event_t event){
/* CARD_ID_SCENT_OF_NIGHTSHADE	4187
Scent of Nightshade	|1|B
Instant
Reveal any number of |Sblack cards in your hand. Target creature gets -X/-X until end of turn, where X is the number of cards revealed this way. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_BLACK));
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
			this_test.zone = TARGET_ZONE_HAND;
			int amount = reveal_cards_from_your_hand(player, card, &this_test);
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -amount, -amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_sigil_of_sleep(int player, int card, event_t event){
/* CARD_ID_SIGIL_OF_SLEEP	4190
Sigil of Sleep	|U
Enchantment - Aura
Enchant creature
Whenever enchanted creature deals damage to a player, return target creature that player controls to its owner's hand. */

	if( ! is_humiliated(player, card, AL_TRIG) && get_card_instance(player, card)->damage_target_player > -1 ){
		if( attached_creature_deals_damage(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS) ){
			card_instance_t *instance = get_card_instance(player, card);
			int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
			instance->targets[1].player = 0;
			APNAP(p,{
						target_definition_t td;
						default_target_definition(player, card, &td, TYPE_CREATURE);
						td.allowed_controller = p;
						td.preferred_controller = p;
						td.allow_cancel = 0;
						char msg[100];
						if( p == player ){
							scnprintf(msg, 100, "Select target creature you control.");
						}
						else{
							scnprintf(msg, 100, "Select target creature your opponent controls.");
						}
						while( times_damaged[p] && can_target(&td) ){
								if( pick_target(&td, "TARGET_CREATURE") ){
									bounce_permanent(instance->targets[0].player, instance->targets[0].card);
									instance->number_of_targets = 0;
								}
								times_damaged[p]--;
						}
					}
			);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, player);
}

// Skittering horror code is the same for Skittering Skirge in urza_saga.c

int card_solidarity2(int player, int card, event_t event){
  /* Solidarity |3|W
   * 	  Instant
   * 	  Creatures you control get +0/+5 until end of turn. */

	if(event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 0, 5, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_soul_feast(int player, int card, event_t event){
/* CARD_ID_SOUL_FEAST	1153
Soul Feast	|3|B|B
Sorcery
Target player loses 4 life and you gain 4 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 4);
			gain_life(player, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_sowing_salt(int player, int card, event_t event){

	/* Sowing Salt	|2|R|R
	 * Sorcery
	 * Exile target nonbasic land. Search its controller's graveyard, hand, and library for all cards with the same name as that land and exile them. Then that player shuffles his or her library. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = SUBTYPE_NONBASIC;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int id = get_id(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			lobotomy_effect(player, instance->targets[0].player, id, -1, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nonbasic land.", 1, NULL);
}

int card_splinter2(int player, int card, event_t event){

	/* Splinter	|2|G|G
	 * Sorcery
	 * Exile target artifact. Search its controller's graveyard, hand, and library for all cards with the same name as that artifact and exile them. Then that
	 * player shuffles his or her library. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int id = get_id(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			lobotomy_effect(player, instance->targets[0].player, id, -1, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
}

int card_taunting_elf(int player, int card, event_t event){
/* CARD_ID_TAUNTING_ELF	4197
Taunting Elf	|G
Creature - Elf 0/1
All creatures able to block ~ do so. */

	return everybody_must_block_me(player, card, event);
}

int card_temporal_adept(int player, int card, event_t event){
/* CARD_ID_TEMPORAL_ADEPT	1170
Temporal Adept	|1|U|U
Creature - Human Wizard 1/1
|U|U|U, |T: Return target permanent to its owner's hand. */

	if (event == EVENT_ATTACK_RATING && affect_me(player, card)){
		ai_defensive_modifier += 60;
	}

	if (event == EVENT_BLOCK_RATING && affect_me(player, card)){
		ai_defensive_modifier -= 60;
	}

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);


	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(3), 0, &td, "TARGET_PERMANENT");
}

int card_tethered_griffin(int player, int card, event_t event){
/* CARD_ID_TETHERED_GRIFFIN	4199
Tethered Griffin	|W
Creature - Griffin 2/3
Flying
When you control no enchantments, sacrifice ~. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		if( count_subtype(player, TYPE_ENCHANTMENT, -1) < 1 ){
			ai_modifier-=1000;
		}
	}

	if( event == EVENT_STATIC_EFFECTS && ! is_humiliated(player, card, AL_STATIC) && count_subtype(player, TYPE_ENCHANTMENT, -1) < 1 ){
		kill_card(player, card, KILL_SACRIFICE);
	}

	return 0;
}

int card_thieving_magpie(int player, int card, event_t event){
  /* Thieving Magpie |2|U|U
   * Creature — Bird 1/3
   * Flying (This creature can't be blocked except by creatures with flying or reach.)
   * Whenever Thieving Magpie deals damage to an opponent, draw a card. */

	int result = damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_OPPONENT);
	if( result ){
		draw_cards(player, result);
	}

	return 0;
}

int card_thorn_elemental(int player, int card, event_t event)
{
  /* Thorn Elemental	|5|G|G
   * Creature - Elemental 7/7
   * You may have ~ assign its combat damage as though it weren't blocked. */
  // Used externally
  card_instance_t* damage = combat_damage_being_prevented(event);
  if (damage
	  && damage->damage_source_card == card && damage->damage_source_player == player
	  && damage->damage_target_card != -1
	  && get_card_instance(player, card)->targets[1].player == 1)
	{
	  damage->damage_target_player = 1-player;
	  damage->damage_target_card = -1;
	}

  if (event == EVENT_DECLARE_BLOCKERS)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  instance->targets[1].player = 0;
	  if ((instance->state & STATE_ATTACKING) && !is_unblocked(player, card))
		instance->targets[1].player = do_dialog(player, player, card, -1, -1, " Deal damage normally\n Ignore blocker", 1);
	}

  return 0;
}

int card_thran_dynamo(int player, int card, event_t event){
/* CARD_ID_THRAN_DYNAMO	4200
Thran Dynamo	|4
Artifact
|T: Add |C|C|C to your mana pool. */

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
			declare_mana_available(player, COLOR_COLORLESS, 3);
		}
	}

	if(event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	if(event == EVENT_ACTIVATE){
		produce_mana_tapped(player, card, COLOR_COLORLESS, 3);
	}

	return 0;
}

int card_thran_foundry(int player, int card, event_t event){
/* CARD_ID_THRAN_FOUNDRY	4201
Thran Foundry	|1
Artifact
|1, |T, Exile ~: Target player shuffles his or her graveyard into his or her library. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			int *deck = deck_ptr[instance->targets[0].player];
			int count_d = count_deck(instance->targets[0].player);
			int count = count_graveyard(instance->targets[0].player)-1;
			while( count > -1 ){
					deck[count_d] = get_grave(instance->targets[0].player)[count];
					remove_card_from_grave(instance->targets[0].player, count);
					count_d++;
					count--;
			}
			shuffle(instance->targets[0].player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_RFG_ME | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_PLAYER");
}

int card_thran_golem(int player, int card, event_t event){
/* CARD_ID_THRAN_GOLEM	4202
Thran Golem	|5
Artifact Creature - Golem 3/3
As long as ~ is enchanted, it gets +2/+2 and has flying, first strike, and trample. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_ABILITIES && affect_me(player, card) ){
			if( is_enchanted(player, card) ){
				event_result |= KEYWORD_FIRST_STRIKE;
				event_result |= KEYWORD_TRAMPLE;
				event_result |= KEYWORD_FLYING;
			}
		}
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			if( is_enchanted(player, card) ){
				event_result +=2;
			}
		}
	}

	return 0;
}

int card_treachery(int player, int card, event_t event){
/* CARD_ID_TREACHERY	1341
Treachery	|3|U|U
Enchantment - Aura
Enchant creature
When ~ enters the battlefield, untap up to five lands.
You control enchanted creature. */
	// Original code :0x4DB760

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		untap_lands(player, card, 5);
	}

	return card_control_magic(player, card, event);
}

int card_trumpet_blast(int player, int card, event_t event){
/* CARD_ID_TRUMPET_BLAST	4204
Trumpet Blast	|2|R
Instant
Attacking creatures get +2/+0 until end of turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.state = STATE_ATTACKING;
		pump_creatures_until_eot(player, card, current_turn, 0, 2, 0, 0, 0, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_twisted_experiment(int player, int card, event_t event){
/* CARD_ID_TWISTED_EXPERIMENT	4205
Twisted Experiment	|1|B
Enchantment - Aura
Enchant creature
Enchanted creature gets +3/-1. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = ANYBODY;

		card_instance_t *instance = get_card_instance(player, card);

		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			int tou = get_toughness(instance->targets[0].player, instance->targets[0].card);
			if( instance->targets[0].player == player ){
				if( tou == 1 ){
					ai_modifier -= 48;
				}
				else{
					ai_modifier += 12;
					int k_mask = KEYWORD_FIRST_STRIKE | KEYWORD_DOUBLE_STRIKE | KEYWORD_FLYING;
					if( check_for_ability(instance->targets[0].player, instance->targets[0].card, k_mask) ){
						ai_modifier += 24;
					}
					int sk_mask = SP_KEYWORD_UNBLOCKABLE | SP_KEYWORD_SHADOW;
					if( check_for_special_ability(instance->targets[0].player, instance->targets[0].card, sk_mask) ){
						ai_modifier += 24;
					}
				}
			}
			if( instance->targets[0].player == 1-player ){
				if( tou == 1 ){
					ai_modifier += 48;
				}
				else{
					ai_modifier -= 12;
					int k_mask = KEYWORD_FIRST_STRIKE | KEYWORD_DOUBLE_STRIKE | KEYWORD_FLYING;
					if( check_for_ability(instance->targets[0].player, instance->targets[0].card, k_mask) ){
						ai_modifier -= 24;
					}
					int sk_mask = SP_KEYWORD_UNBLOCKABLE | SP_KEYWORD_SHADOW;
					if( check_for_special_ability(instance->targets[0].player, instance->targets[0].card, sk_mask) ){
						ai_modifier -= 24;
					}
				}
			}
		}
		return 0;
	}

	return generic_aura(player, card, event, player, 3, -1, 0, 0, 0, 0, 0);
}

int card_urzas_incubator(int player, int card, event_t event){
/* CARD_ID_URZAS_INCUBATOR	4206
Urza's Incubator	|3
Artifact
As ~ enters the battlefield, choose a creature type.
Creature spells of the chosen type cost |2 less to cast. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = select_a_subtype(player, card);
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card, AL_STATIC) ){
		int subt = get_card_instance(player, card)->info_slot;
		if( has_subtype(affected_card_controller, affected_card, subt) ){
			COST_COLORLESS-=2;
		}
	}

	return 0;
}

int card_wake_of_destruction(int player, int card, event_t event){
/* CARD_ID_WAKE_OF_DESTRUCTION	4207
Wake of Destruction	|3|R|R|R
Sorcery
Destroy target land and all other lands with the same name as that land. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	return echoing_spell(player, card, event, &td, "TARGET_LAND", KILL_DESTROY);
}

int card_yavimaya_elder(int player, int card, event_t event){
/* CARD_ID_YAVIMAYA_ELDER	4210
Yavimaya Elder	|1|G|G
Creature - Human Druid 2/1
When ~ dies, you may search your library for up to two basic land cards, reveal them, and put them into your hand. If you do, shuffle your library.
|2, Sacrifice ~: Draw a card. */

	if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_AI(player))){
		tutor_basic_lands(player, TUTOR_HAND, 2);
	}

	if (event == EVENT_RESOLVE_ACTIVATION){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL);
}

int card_yavimaya_enchantress(int player, int card, event_t event){
/* CARD_ID_YAVIMAYA_ENCHANTRESS	1194
Yavimaya Enchantress	|2|G
Creature - Human Druid 2/2
~ gets +1/+1 for each enchantment on the battlefield. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		event_result +=count_subtype(ANYBODY, TYPE_ENCHANTMENT, -1);
	}

	return 0;
}

int card_yavimaya_hollow(int player, int card, event_t event){
/* CARD_ID_YAVIMAYA_HOLLOW	4211
Yavimaya Hollow	""
Legendary Land
|T: Add |C to your mana pool.
|G, |T: Regenerate target creature. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_state = TARGET_STATE_DESTROYED;
	td.preferred_controller = player;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_REGENERATE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( land_can_be_played & LCBP_REGENERATION ){
			if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_G(1), 0, &td, NULL) ){
				return 99;
			}
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int can_regenerate = 0;
		if( (land_can_be_played & LCBP_REGENERATION) && ! paying_mana() ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_G(1), 0, &td, NULL) ){
				can_regenerate = 1;
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Get mana", 1, 1,
							"Regenerate target creature", can_regenerate, 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_REGENERATE ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_G(1), 0,
											&td, "Select target creature to regenerate");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_REGENERATE ){
			if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
				regenerate_target( instance->targets[0].player, instance->targets[0].card );
			}
		}
	}

	return 0;
}

int card_yawgmoths_bargain(int player, int card, event_t event){
/* CARD_ID_YAWGMOTHS_BARGAIN	4212
Yawgmoth's Bargain	|4|B|B
Enchantment
Skip your draw step.
Pay 1 life: Draw a card. */

	skip_your_draw_step(player, event);

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if(event == EVENT_RESOLVE_ACTIVATION){
		draw_a_card(player);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, 1, NULL, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2107da0; original cards/urza_destiny.c:2397. */
int card_archery_training(int player, int card, event_t event)
{
  /* Archery Training	|W
   * Enchantment - Aura
   * Enchant creature
   * At the beginning of your upkeep, you may put an arrow counter on this Aura.
   * Enchanted creature has "|T: This creature deals X damage to target attacking or blocking creature, where X is the number of arrow counters on ~." */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  result = duh_mode(player);
  upkeep_trigger_ability_mode(player, card, event, player,
                              (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    add_counters(player, card, player, card, COUNTER_ARROW, 1);
  }
  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = get_card_instance(player, card);
    if (-1 < instance->damage_target_player)
    {

      instance = get_card_instance(player, card);
      if (event == EVENT_ACTIVATE)
      {
        default_target_definition(player, card, &td, 2);
        td.required_state = TARGET_STATE_IN_COMBAT;
      }
      else
      {
        if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
        {
          default_target_definition(player, card, &td, 2);
          td.preferred_controller = player;
          return aura(player, card, event, &td, NULL);
        }
        default_target_definition(player, card, &td, 2);
        td.required_state = TARGET_STATE_IN_COMBAT;
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          result = valid_target(&td);
          if (result != 0)
          {
            result = count_counters(player, card, COUNTER_ARROW);
            damage_target0(player, card, result);
          }
        }
      }
      return granted_generic_activated_ability(player, card, (int)instance->damage_target_player,
                                               instance->damage_target_card, event, 5, 0, 0, 0, 0,
                                               0, 0, 0, &td, "TARGET_ATTACKING_BLOCKING_CREATURE");
    }
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  return aura(player, card, event, &td, NULL);
}
#endif

/* ManalinkEh 2022: 0x2107ff0; original cards/urza_destiny.c:2433. */
int card_blizzard_elemental(int player, int card, event_t event)
{
  /* Blizzard Elemental	|5|U|U
   * Creature - Elemental 5/5
   * Flying
   * |3|U: Untap this creature. */

  card_instance_t *instance;
  int result;
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    untap_card(instance->parent_controller, instance->parent_card);
  }
  result = generic_activated_ability(player, card, event, 0, 3, 0, 1, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x21080b0; original cards/urza_destiny.c:2451. */
int card_braidwood_sextant(int player, int card, event_t event)
{
  /* Braidwood Sextant	|1
   * Artifact
   * |2, |T, Sacrifice this artifact: Search your library for a basic land card, reveal that card, put it into your hand, then shuffle. */

  int result;
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    tutor_basic_land(player, 0, 0);
  }
  result = generic_activated_ability(player, card, event, 9, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2108160; original cards/urza_destiny.c:2467. */
int card_bubbling_beebles(int player, int card, event_t event)
{
  /* Bubbling Beebles	|4|U
   * Creature - Beeble 3/3
   * This creature can't be blocked as long as defending player controls an enchantment. */

  card_instance_t *instance;
  int result;

  if (event == EVENT_BLOCK_LEGALITY)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (((result == 0) && (attacking_card_controller == player)) && (attacking_card == card))
      {
        result = count_permanents_by_type(1 - current_turn, TYPE_ENCHANTMENT);
        if (0 < result)
        {
          event_result = 1;
        }
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21081f0; original cards/urza_destiny.c:2482. */
int card_capashen_standard(int player, int card, event_t event)
{
  /* Capashen Standard	|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+1.
   * |2, Sacrifice this Aura: Draw a card. */

  int result;

  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      attached_gets_pt(player, card, event, 1, 1);
      result = aura(player, card, event, NULL, NULL);
      return result;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      draw_cards(player, 1);
    }
  }
  return generic_activated_ability(player, card, event, 8, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21082e0; original cards/urza_destiny.c:2501. */
int card_chime_of_night(int player, int card, event_t event)
{
  /* Chime of Night	|1|B
   * Enchantment - Aura
   * Enchant creature
   * When this Aura is put into a graveyard from the battlefield, destroy target nonblack creature. */

  int result;
  card_instance_t *instance;
  const char *prompt;
  target_definition_t td;

  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {

    default_target_definition(player, card, &td, 2);
    td.illegal_color = get_sleighted_color_test(player, card, 2);
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      prompt = get_sleighted_color_text(player, card, "Select target non%s creature.", 1);
      result = new_pick_target(&td, prompt, 0, 0x80);
      if (result != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
      }
    }
  }
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x21083f0; original cards/urza_destiny.c:2522. */
int card_compost(int player, int card, event_t event)
{
  /* Compost	|1|G
   * Enchantment
   * Whenever a black card is put into an opponent's graveyard from anywhere, you may draw a card. */

  int i;
  xtrigger_t current_trigger;
  int result_or_ai_player;
  unsigned int compost_color;
  int deck_count_or_ai_player;
  unsigned int milled_color_or_draw_choice;
  test_definition_t test2;

  enable_xtrigger_flags = enable_xtrigger_flags | ENABLE_XTRIGGER_MILLED;
  current_trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    current_trigger = xtrigger_impl_value_dont_use_directly;
  }
  if (((((current_trigger == XTRIGGER_MILLED) && (card == affected_card)) &&
        (player == affected_card_controller)) &&
       ((reason_for_trigger_controller == player && (trigger_cause_controller == 1 - player)))) &&
      (result_or_ai_player = is_humiliated(player, card, AL_TRIG), result_or_ai_player == 0))
  {

    if (1 < (uint32_t)(event - EVENT_TRIGGER))
    {
      return 0;
    }
    if (0 < num_cards_milled)
    {
      i = 0;
      do
      {
        milled_color_or_draw_choice =
            get_color(trigger_cause_controller, cards_milled[i].internal_card_id);
        compost_color = get_sleighted_color_test(player, card, 2);
        if ((milled_color_or_draw_choice & compost_color) != 0)
        {
          if (event == EVENT_TRIGGER)
          {
            event_result = event_result | 2;
            return 0;
          }
          if (cards_milled[i].position != -1)
          {

            milled_color_or_draw_choice = 0;
            if ((ai_is_speculating == 1 && player == 1) ||
                (ai_is_speculating != 1 &&
                 (player == 0 || (player == 1 && ((unsigned char)trace_mode & 2) == 0))))
            {
              deck_count_or_ai_player = count_deck(player);
              milled_color_or_draw_choice = (unsigned int)(deck_count_or_ai_player < 6);
            }
            if (do_dialog(player, player, card, -1, -1, " Draw a card\n Pass",
                          milled_color_or_draw_choice) == 0)
            {
              draw_a_card(player);
            }
          }
        }
        i = i + 1;
      } while (i < num_cards_milled);
    }
  }
  default_test_definition(&test2, 0xff);
  test2.color = get_sleighted_color_test(player, card, 2);
  if (when_a_card_is_put_into_graveyard_from_anywhere_but_library_do_something(
          player, card, event, 1 - player, RESOLVE_TRIGGER_MANDATORY, &test2) == 0)
    return global_enchantment(player, card, event);

  milled_color_or_draw_choice = 0;
  if ((ai_is_speculating == 1 && player == 1) ||
      (ai_is_speculating != 1 &&
       (player == 0 || (player == 1 && ((unsigned char)trace_mode & 2) == 0))))
  {
    result_or_ai_player = count_deck(player);
    milled_color_or_draw_choice = (unsigned int)(result_or_ai_player < 6);
  }
  if (do_dialog(player, player, card, -1, -1, " Draw a card\n Pass", milled_color_or_draw_choice) ==
      0)
  {
    draw_a_card(player);
  }
  return global_enchantment(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21086d0; original cards/urza_destiny.c:2578. */
int card_disappear(int player, int card, event_t event)
{
  /* Disappear	|2|U|U
   * Enchantment - Aura
   * Enchant creature
   * |U: Return enchanted creature and this Aura to their owners' hands. */

  card_instance_t *instance;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = get_card_instance(player, card);
    if (-1 < instance->damage_target_player)
    {

      instance = get_card_instance(player, card);
      if (event != EVENT_ACTIVATE)
      {
        if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
          return aura(player, card, event, NULL, NULL);
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          bounce_permanent(instance->parent_controller, instance->parent_card);
          bounce_permanent((int)instance->damage_target_player, instance->damage_target_card);
        }
      }
      return generic_activated_ability(player, card, event, 0, 0, 0, 1, 0, 0, 0, 0, NULL, NULL);
    }
  }
  return aura(player, card, event, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x21087f0; original cards/urza_destiny.c:2599. */
int card_disease_carriers(int player, int card, event_t event)
{
  /* Disease Carriers	|2|B|B
   * Creature - Rat 2/2
   * When this creature dies, target creature gets -2/-2 until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    default_target_definition(player, card, &td, 2);
    td.special = TARGET_SPECIAL_NOT_ME;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      pick_target(&td, "TARGET_CREATURE");
      pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -2, -2);
      return 0;
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21088c0; original cards/urza_destiny.c:2622. */
int card_dying_wail(int player, int card, event_t event)
{
  /* Dying Wail	|1|B
   * Enchantment - Aura
   * Enchant creature
   * When enchanted creature dies, target player discards two cards. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = get_card_instance(player, card);
    if (-1 < instance->damage_target_player)
    {
      result = is_humiliated(player, card, AL_TRIG);
      if (result == 0)
      {
        result = attached_creature_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
        if (result != 0)
        {

          default_target_definition(player, card, &td, 0);
          td.zone = TARGET_ZONE_PLAYERS;
          td.allow_cancel = 0;
          instance = get_card_instance(player, card);
          result = can_target(&td);
          if (result != 0)
          {
            result = pick_target(&td, "TARGET_PLAYER");
            if (result != 0)
            {
              new_multidiscard(instance->targets[0].player, 2, 0, player);
            }
          }
        }
      }
    }
  }
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x21089e0; original cards/urza_destiny.c:2646. */
int card_encroach(int player, int card, event_t event)
{
  /* Encroach	|B
   * Sorcery
   * Target player reveals their hand. You choose a nonbasic land card from it. That player discards that card. */

  card_instance_t *instance;
  ec_definition_t this_definition;
  target_definition_t td;
  test_definition_t this_test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      new_default_test_definition(&this_test, 1, "Choose a nonbasic land card.");
      this_test.subtype = 0x2000;
      this_test.subtype_flag = 1;
      default_ec_definition(instance->targets[0].player, player, &this_definition);
      new_effect_coercion(&this_definition, &this_test);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2108bb0; original cards/urza_destiny.c:2684. */
int card_festering_wound(int player, int card, event_t event)
{
  /* Festering Wound	|1|B
   * Enchantment - Aura
   * Enchant creature
   * At the beginning of your upkeep, you may put an infection counter on this Aura.
   * At the beginning of the upkeep of enchanted creature's controller, this Aura deals X damage to that player, where X is the number of infection counters on this Aura. */

  card_instance_t *instance;
  int result;

  upkeep_trigger_ability(player, card, event, 2);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    if (current_turn == player)
    {

      result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_FULLCARD(player, card), DLG_SMALLCARD(
                           player, card), DLG_WHO_CHOOSES(player), DLG_RANDOM, DLG_NO_STORAGE,
                           "Put infection counter", 1, 10, 0, 0, 0, 0);
      if (result == 0)
      {
        instance = get_card_instance(player, card);
        if (instance->damage_target_player == current_turn)
        {
          result = count_counters(player, card, COUNTER_INFECTION);
          damage_enchanted_permanent_controller_at_upkeep(player, card,
                                                          EVENT_UPKEEP_TRIGGER_ABILITY, result);
        }
        spell_fizzled = 0;
        return 0;
      }
      add_counters(player, card, player, card, COUNTER_INFECTION, 1);
    }
    instance = get_card_instance(player, card);
    if (instance->damage_target_player == current_turn)
    {
      result = count_counters(player, card, COUNTER_INFECTION);
      damage_enchanted_permanent_controller_at_upkeep(player, card, EVENT_UPKEEP_TRIGGER_ABILITY,
                                                      result);
    }
  }
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x2108d90; original cards/urza_destiny.c:2719. */
int card_goblin_festival(int player, int card, event_t event)
{
  /* Goblin Festival	|1|R
   * Enchantment
   * |2: This enchantment deals 1 damage to any target. Flip a coin. If you lose the flip, choose one of your opponents. That player gains control of this enchantment. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      result = global_enchantment(player, card, event);
      return result;
    }
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        damage_target0(player, card, 1);
        result = flip_a_coin(player, card);
        if (result == 0)
        {
          give_control_of_self(instance->parent_controller, instance->parent_card);
        }
      }
    }
  }
  result =
      generic_activated_ability(player, card, event, 4, 2, 0, 0, 0, 0, 0, 0, &td, "TARGET_ANY");
  return result;
}

/* ManalinkEh 2022: 0x2108f30; original cards/urza_destiny.c:2746. */
int card_goblin_masons(int player, int card, event_t event)
{
  /* Goblin Masons	|1|R
   * Creature - Goblin 2/1
   * When this creature dies, destroy target Wall. */

  int result;
  target_definition_t td;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    default_target_definition(player, card, &td, 0x47);
    td.required_subtype = 0xd1;
    td.allow_cancel = 0;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "Select target wall.", 0, 0x80);
      if (result != 0)
      {
        action_on_target(player, card, 0, ACT_KILL_DESTROY);
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2108ff0; original cards/urza_destiny.c:2766. */
int card_illuminated_wings(int player, int card, event_t event)
{
  /* Illuminated Wings	|1|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has flying.
   * |2, Sacrifice this Aura: Draw a card. */

  int result;

  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      attached_gets_kw(player, card, event, KEYWORD_FLYING, SP_KEYWORD_0);
      result = aura(player, card, event, NULL, NULL);
      return result;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      draw_cards(player, 1);
    }
  }
  return generic_activated_ability(player, card, event, 8, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21090e0; original cards/urza_destiny.c:2784. */
int card_incendiary(int player, int card, event_t event)
{
  /* Incendiary	|R
   * Enchantment - Aura
   * Enchant creature
   * At the beginning of your upkeep, you may put a fuse counter on this Aura.
   * When enchanted creature dies, this Aura deals X damage to any target, where X is the number of fuse counters on this Aura. */

  int result;
  card_instance_t *instance;
  int damage_triggers_remaining;
  target_definition_t td;

  result = duh_mode(player);
  upkeep_trigger_ability_mode(player, card, event, player,
                              (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    add_counters(player, card, player, card, COUNTER_FUSE, 1);
  }
  instance = get_card_instance(player, card);

  if (((((-1 < instance->damage_target_player) &&
         (result = is_humiliated(player, card, AL_TRIG), result == 0)) &&
        (instance = get_card_instance(player, card), event == EVENT_GRAVEYARD_FROM_PLAY)) &&
       ((instance->damage_target_card == affected_card &&
         (instance->damage_target_player == affected_card_controller)))) &&
      ((result = is_this_dying((int)instance->damage_target_player, instance->damage_target_card),
        result != 0 &&
            (result = check_for_death_trigger_removal(affected_card_controller, affected_card),
             result == 0))))
  {
    instance->targets[0xb].player = 0x42;
  }
  result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {

    damage_triggers_remaining = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
    do
    {
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      result = can_target(&td);
      if (result != 0 && (result = pick_target(&td, "TARGET_ANY"), result != 0))
      {
        result = count_counters(player, card, COUNTER_FUSE);
        damage_target0(player, card, result);
      }
      damage_triggers_remaining = damage_triggers_remaining - 1;
    } while (damage_triggers_remaining != 0);
    kill_card(player, card, KILL_STATE_BASED_ACTION);
  }
  return aura(player, card, event, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x2109300; original cards/urza_destiny.c:2824. */
int card_landslide(int player, int card, event_t event)
{
  /* Landslide	|R
   * Sorcery
   * Sacrifice any number of Mountains. ~ deals that much damage to target player or planeswalker. */

  int result;
  card_instance_t *instance;
  const char *msg;
  target_definition_t td;
  test_definition_t test;

  default_target_definition(player, card, &td, 0x100);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      msg = get_hacked_land_text(player, card, "Select a %s.", 0x5006);
      new_default_test_definition(&test, 1, msg);
      test.subtype = get_hacked_subtype(player, card, 0x5006);
      test.qty = 500;
      result = new_sacrifice(player, card, player, SAC_DONE | SAC_NO_CANCEL, &test);
      damage_creature(instance->targets[0].player, instance->targets[0].card, result, player, card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  result = generic_spell(player, card, event, 1, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2109460; original cards/urza_destiny.c:2853. */
int card_lurking_jackals(int player, int card, event_t event)
{
  /* Lurking Jackals	|B
   * Enchantment
   * When an opponent has 10 or less life, if this permanent is an enchantment, it becomes a 3/2 Jackal creature. */

  int result;

  if ((((event == EVENT_STATIC_EFFECTS) && (life[1 - player] < 0xb)) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_what(player, card, 4);
    if (result != 0)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        animate_self_remove_type(player, card, 3, 2, TYPE_ENCHANTMENT, KEYWORD_0, SP_KEYWORD_0,
                                 COLOR_TEST_0, 1);
      }
    }
  }
  has_subtype_if_animated_self(player, card, event, SUBTYPE_JACKAL);
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2109550; original cards/urza_destiny.c:2869. */
int card_magnify(int player, int card, event_t event)
{
  /* Magnify	|G
   * Instant
   * All creatures get +1/+1 until end of turn. */

  int result;
  if (event == EVENT_RESOLVE_SPELL)
  {
    pump_creatures_until_eot(player, card, 2, 0, 1, 1, 0, 0, NULL);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21095e0; original cards/urza_destiny.c:2883. */
int card_mantis_engine(int player, int card, event_t event)
{
  /* Mantis Engine	|5
   * Artifact Creature - Insect 3/3
   * |2: This creature gains flying until end of turn. (It can't be blocked except by creatures with flying or reach.)
   * |2: This creature gains first strike until end of turn. (It deals combat damage before creatures without first strike.) */

  enum
  {
    CHOICE_FLYING = 1,
    CHOICE_FIRST_STRIKE = 2
  };

  int choice;
  uint32_t first_strike_priority;
  card_instance_t *instance;
  int source_card_or_result;
  int ability_choice;
  int source_player_or_priority;
  event_t event_00;
  if (event == EVENT_CHECK_ABILITIES)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (source_player_or_priority = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0,
                                                               2, 0, 0, 0, 0, 0, 0, NULL, NULL),
         source_player_or_priority != 0))
    {
      check_abilities_keywords = check_abilities_keywords | 0x100;
    }
  }
  else
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      event_00 = EVENT_CAN_ACTIVATE;
      return generic_activated_ability(player, card, event_00, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      if (((current_turn != player) || (source_player_or_priority = 10, 0x14 < current_phase)) &&
          (source_player_or_priority = 1, current_turn == 1 - player))
      {
        source_player_or_priority = (unsigned int)(current_phase < 0x17) * 9 + 1;
      }
      if (((current_phase == 0x18) &&
           (source_card_or_result = is_unblocked(player, card), source_card_or_result == 0)) &&
          (source_card_or_result = check_for_ability(player, card, 0x100),
           source_card_or_result == 0))
      {
        first_strike_priority = 10;
      }
      else
      {
        first_strike_priority = 1;
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Gains flying", 1,
                           source_player_or_priority, "Gains first strike", 1,
                           first_strike_priority, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
      }
      else if (choice - 1U < 2)
      {
        event_00 = EVENT_ACTIVATE;
        return generic_activated_ability(player, card, event_00, 0, 2, 0, 0, 0, 0, 0, 0, NULL,
                                         NULL);
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      source_player_or_priority = instance->parent_controller;
      source_card_or_result = instance->parent_card;
      ability_choice = instance->info_slot;
      if (ability_choice == CHOICE_FLYING)
      {
        alternate_legacy_text(1, player,
                              pump_ability_until_eot(
                                  source_player_or_priority, source_card_or_result,
                                  source_player_or_priority, source_card_or_result, 0, 0, 0x20, 0));
        ability_choice = instance->info_slot;
      }
      if (ability_choice == CHOICE_FIRST_STRIKE)
      {
        alternate_legacy_text(2, player,
                              pump_ability_until_eot(source_player_or_priority,
                                                     source_card_or_result,
                                                     source_player_or_priority,
                                                     source_card_or_result, 0, 0, 0x100, 0));
      }
    }
  }
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21099f0; original cards/urza_destiny.c:2939. */
int card_mark_of_fury(int player, int card, event_t event)
{
  /* Mark of Fury	|R
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has haste.
   * At the beginning of the end step, return this Aura to its owner's hand. */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      result = eot_trigger(player, card, event);
      if (result != 0)
      {
        bounce_permanent(player, card);
      }
    }
  }
  attached_gets_kw(player, card, event, KEYWORD_0, SP_KEYWORD_HASTE);
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2109aa0; original cards/urza_destiny.c:2956. */
int card_mask_of_law_and_grace(int player, int card, event_t event)
{
  /* Mask of Law and Grace	|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has protection from black and from red. */

  int result;

  attached_gets_kw(player, card, event, KEYWORD_PROT_RED | KEYWORD_PROT_BLACK, SP_KEYWORD_0);
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2109b00; original cards/urza_destiny.c:2967. */
int card_momentum(int player, int card, event_t event)
{
  /* Momentum	|2|G
   * Enchantment - Aura
   * Enchant creature
   * At the beginning of your upkeep, you may put a growth counter on this Aura.
   * Enchanted creature gets +1/+1 for each growth counter on this Aura. */

  int result;

  result = duh_mode(player);
  upkeep_trigger_ability_mode(player, card, event, player,
                              (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    add_counters(player, card, player, card, COUNTER_GROWTH, 1);
  }
  result = count_counters(player, card, COUNTER_GROWTH);
  attached_gets_pt(player, card, event, result, result);
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x2109bd0; original cards/urza_destiny.c:2985. */
int card_plague_dogs(int player, int card, event_t event)
{
  /* Plague Dogs	|4|B
   * Creature - Phyrexian Zombie Dog 3/3
   * When this creature dies, all creatures get -1/-1 until end of turn.
   * |2, Sacrifice this creature: Draw a card. */

  int result;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    pump_subtype_until_eot(player, card, 2, -1, -1, -1, 0, 0);
  }
  if (!IS_GAA_EVENT(event))
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    draw_cards(player, 1);
  }
  result = generic_activated_ability(player, card, event, 8, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2109ce0; original cards/urza_destiny.c:3005. */
int card_private_research(int player, int card, event_t event)
{
  /* Private Research	|U
   * Enchantment - Aura
   * Enchant creature
   * At the beginning of your upkeep, you may put a page counter on this Aura.
   * When enchanted creature dies, draw a card for each page counter on this Aura. */

  int result;
  card_instance_t *instance;

  result = duh_mode(player);
  upkeep_trigger_ability_mode(player, card, event, player,
                              (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    add_counters(player, card, player, card, COUNTER_PAGE, 1);
  }
  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = get_card_instance(player, card);
    if (-1 < instance->damage_target_player)
    {
      result = is_humiliated(player, card, AL_TRIG);
      if (result == 0)
      {
        result = attached_creature_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
        if (result != 0)
        {
          result = count_counters(player, card, COUNTER_PAGE);
          draw_cards(player, result);
        }
      }
    }
  }
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x2109e10; original cards/urza_destiny.c:3026. */
int card_rescue(int player, int card, event_t event)
{
  /* Rescue	|U
   * Instant
   * Return target permanent you control to its owner's hand. */

  card_instance_t *instance;
  target_definition_t td;
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
    }
    else
    {
      if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
      {
        default_target_definition(player, card, &td, 0x47);
        td.preferred_controller = player;
        td.allowed_controller = player;
        if (event == EVENT_RESOLVE_SPELL)
        {
          if (valid_target(&td))
          {
            instance = get_card_instance(player, card);
            bounce_permanent(instance->targets[0].player, instance->targets[0].card);
          }
          kill_card(player, card, KILL_DESTROY);
        }
        return generic_spell(player, card, event, 1, &td, "TARGET_PERMANENT_YOU_CONTROL", 1, NULL);
      }
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
    }
  }
  default_target_definition(player, card, &td, 0x47);
  td.allowed_controller = player;
  td.preferred_controller = player;
  return generic_spell(player, card, event, 1, &td, "TARGET_PERMANENT_YOU_CONTROL", 1, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2109f60; original cards/urza_destiny.c:3050. */
int card_scrying_glass(int player, int card, event_t event)
{
  /* Scrying Glass	|2
   * Artifact
   * |3, |T: Choose a number greater than 0 and a color. Target opponent reveals their hand. If that opponent reveals exactly the chosen number of cards of the chosen color, you draw a card. */

  int p;
  card_instance_t *instance;
  int target_valid;
  int unmatched_guess;
  int color_or_card_index;
  unsigned char chosen_color;
  unsigned int chosen_color_test;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    get_card_instance(player, card);
    return generic_activated_ability(player, card, event,
                                     (GAA_UNTAPPED | GAA_CAN_ONLY_TARGET_OPPONENT), 3, 0, 0, 0, 0,
                                     0, 0, &td, NULL);
  }
  if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  instance = get_card_instance(player, card);
  if ((event != EVENT_RESOLVE_ACTIVATION) || (target_valid = valid_target(&td), target_valid == 0))
    return generic_activated_ability(player, card, event,
                                     (GAA_UNTAPPED | GAA_CAN_ONLY_TARGET_OPPONENT), 3, 0, 0, 0, 0,
                                     0, 0, &td, NULL);
  p = instance->targets[0].player;
  if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
  {
    unmatched_guess = internal_rand(hand_count[p]);
    unmatched_guess = unmatched_guess + 1;
    color_or_card_index = choose_a_color(player, get_deck_color(player, p));
    chosen_color = (unsigned char)color_or_card_index;
    reveal_target_player_hand(p);
  }
  else
  {
    do
    {
      unmatched_guess = choose_a_number(player, "Choose a number greater than 0.", hand_count[p]);
    } while (unmatched_guess < 1);
    color_or_card_index = choose_a_color(player, get_deck_color(player, p));
    chosen_color = (unsigned char)color_or_card_index;
    reveal_target_player_hand(p);
    if (active_cards_count[p] < 1)
      return generic_activated_ability(player, card, event,
                                       (GAA_UNTAPPED | GAA_CAN_ONLY_TARGET_OPPONENT), 3, 0, 0, 0, 0,
                                       0, 0, &td, NULL);
  }
  if (0 < active_cards_count[p])
  {
    chosen_color_test = 1 << (chosen_color & 0x1f);
    color_or_card_index = 0;
    do
    {
      instance = in_hand(p, color_or_card_index);
      if (instance != NULL)
      {
        unmatched_guess =
            unmatched_guess + -1 + (unsigned int)(((int)instance->color & chosen_color_test) == 0);
      }
      color_or_card_index = color_or_card_index + 1;
    } while (color_or_card_index < active_cards_count[p]);
  }
  if (unmatched_guess == 0)
  {
    draw_cards(player, 1);
  }
  return generic_activated_ability(player, card, event,
                                   (GAA_UNTAPPED | GAA_CAN_ONLY_TARGET_OPPONENT), 3, 0, 0, 0, 0, 0,
                                   0, &td, NULL);
}
#endif

/* ManalinkEh 2022: 0x210a1d0; original cards/urza_destiny.c:3097. */
int card_storage_matrix(int player, int card, event_t event) {
  /* Storage Matrix	|3
   * Artifact
   * As long as this artifact is untapped, each player chooses artifact, creature, or land during their untap step. That player can untap only permanents of the chosen type this step. */

 return 0; }
