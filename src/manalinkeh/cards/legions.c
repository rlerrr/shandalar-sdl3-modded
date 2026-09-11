#include "manalink.h"

// Functions

static int provoke_impl(int src_player, int src_card, int player, int card, event_t event)
{
  // Uses src_player/src_card as the source of the legacy card, but deliberately uses player/card to store targetting
  if (!is_humiliated(player, card))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = 1-player;
	  td.preferred_controller = 1-player;

	  card_instance_t* instance = get_card_instance(player, card);

	  char prompt[200];
	  if (ai_is_speculating == 1)
		prompt[0] = 0;
	  else
		{
		  load_text(0, "TARGET_CREATURE_OPPONENT_CONTROLS");
		  scnprintf(prompt, 200, "%s: Provoke: %s", get_card_name_by_instance(instance), text_lines[0]);
		}

	  instance->number_of_targets = 0;
	  if (pick_next_target_noload(&td, prompt))
		{
		  int leg = target_must_block_me(player, card, instance->targets[0].player, instance->targets[0].card, 1);
		  if (player != src_player || card != src_card)
			{
			  // Reseat the legacy's text and image to the source card's
			  card_instance_t* legacy = get_card_instance(player, leg);
			  legacy->display_pic_csv_id = get_id(src_player, src_card);
			  legacy->display_pic_num = get_card_image_number(legacy->display_pic_csv_id, src_player, src_card);
			}

		  untap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

  return 0;
}

static int provoke(int player, int card, event_t event)
{
  // When this attacks, you may have target creature defending player controls untap and block it if able.
  if (declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), player, card))
	provoke_impl(player, card, player, card, event);

  return 0;
}

static void amplify(int player, int card, event_t event, int subtype, int amount){

	if( event == EVENT_RESOLVE_SPELL ){
		add_state(player, card, STATE_OUBLIETTED);
		test_definition_t this_test;
		new_default_test_definition(&this_test, 0, get_subtype_text("Select %a card.", subtype));
		this_test.subtype = subtype;
		add_1_1_counters(player, card, amount*reveal_cards_from_your_hand(player, card, &this_test));
		remove_state(player, card, STATE_OUBLIETTED);
	}
}

// Cards

int card_akroma_angel_of_wrath(int player, int card, event_t event)
{
  /* Akroma, Angel of Wrath	|5|W|W|W
   * Legendary Creature - Angel 6/6
   * Flying, first strike, vigilance, trample, haste, protection from |Sblack and from |Sred */

  check_legend_rule(player, card, event);

  vigilance(player, card, event);
  haste(player, card, event);

  return 0;
}

int card_akromas_devoted(int player, int card, event_t event){
/* CARD_ID_AKROMAS_DEVOTED	6333
Akroma's Devoted	|3|W
Creature - Human Cleric 2/4
Cleric creatures have vigilance. */

	if( event == EVENT_ABILITIES && has_creature_type(affected_card_controller, affected_card, SUBTYPE_CLERIC) && in_play(affected_card_controller, card) ){
		vigilance(affected_card_controller, affected_card, event);
	}

	return 0;
}

int card_aphetto_exterminator(int player, int card, event_t event){
/* CARD_ID_APHETTO_EXTERMINATOR	6334
Aphetto Exterminator	|2|B
Creature - Human Wizard 3/1
Morph |3|B (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, target creature gets -3/-3 until end of turn. */

	if( event == EVENT_TURNED_FACE_UP ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -3, -3);
		}
	}
	return morph(player, card, event, MANACOST_XB(3, 1));
}

int card_bane_of_the_living(int player, int card, event_t event)
{
/* CARD_ID_BANE_OF_THE_LIVING	6338
Bane of the Living	|2|B|B
Creature - Insect 4/3
Morph |X|B|B (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, all creatures get -X/-X until end of turn. */

	if (event == EVENT_UNMORPH)
	{
		if( charge_mana_multi(player, MANACOST_XB(-1, 2)) )
			get_card_instance(player, card)->info_slot = x_value;

		return 0;
	}

	if (event == EVENT_TURNED_FACE_UP)
	{
		card_instance_t* instance = get_card_instance(player, card);
		pump_subtype_until_eot(player, card, 2, -1, -instance->info_slot, -instance->info_slot, 0, 0);
	}

	return morph(player, card, event, MANACOST_XB(-1,2));
}

int card_beacon_of_destiny(int player, int card, event_t event){
/* CARD_ID_BEACON_OF_DESTINY	6339
Beacon of Destiny	|1|W
Creature - Human Cleric 1/3
|T: The next time a source of your choice would deal damage to you this turn, that damage is dealt to ~ instead. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;
	td.required_type = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			target->damage_target_player = instance->parent_controller;
			target->damage_target_card = instance->parent_card;
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION | GAA_LITERAL_PROMPT,
									MANACOST0, 0, &td, "Select target damage card that targets you.");
}

int card_blade_sliver(int player, int card, event_t event){
/* CARD_ID_BLADE_SLIVER	1380
Blade Sliver	|2|R
Creature - Sliver 2/2
All Sliver creatures get +1/+0. */

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 1, 0, 0, BCT_INCLUDE_SELF);
	return slivercycling(player, card, event);
}

int card_brontotherium(int player, int card, event_t event){
/* CARD_ID_BRONTOTHERIUM	6344
Brontotherium	|4|G|G
Creature - Beast 5/3
Trample
Provoke (Whenever this creature attacks, you may have target creature defending player controls untap and block it if able.) */

	return provoke(player, card, event);
}

int card_brood_sliver(int player, int card, event_t event){
/* Brood Sliver	|4|G
 * Creature - Sliver 3/3
 * Whenever a Sliver deals combat damage to a player, its controller may put a 1/1 colorless Sliver creature token onto the battlefield. */

	card_instance_t *damage = combat_damage_being_dealt(event);
	if( damage ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_SLIVER;
		if( new_make_test_in_play(damage->damage_source_player, damage->damage_source_card, -1, &test) ){
			if( damage->damage_target_card == -1 && ! damage_is_to_planeswalker(damage) ){
				card_instance_t *instance = get_card_instance(player, card);
				if( instance->targets[8].player < 0 ){
					instance->targets[8].player = 0;
				}
				instance->targets[8].player++;
				if( instance->targets[1].player < 0 ){
					instance->targets[1].player = 0;
				}
				if( damage->damage_source_player == 0 ){
					SET_BYTE0(instance->targets[1].player)++;
				}
				if( damage->damage_source_player == 1 ){
					SET_BYTE1(instance->targets[1].player)++;
				}
			}
		}
	}

	if( resolve_damage_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int slivers[2] = {BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player)};
		instance->targets[1].player = 0;

		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SLIVER, &token);
		token.t_player = 0;
		token.qty = slivers[0];
		generate_token(&token);

		token.t_player = 1;
		token.qty = slivers[1];
		generate_token(&token);
	}

	return slivercycling(player, card, event);
}

int card_caller_of_the_claw(int player, int card, event_t event){
/* CARD_ID_CALLER_OF_THE_CLAW	6346
Caller of the Claw	|2|G
Creature - Elf 2/2
Flash
When ~ enters the battlefield, create a 2/2 |Sgreen Bear creature token for each nontoken creature put into your graveyard from the battlefield this turn.*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int amount = get_dead_count(player, TYPE_CREATURE | GDC_NONTOKEN);
		if( ! amount ){
			ai_modifier -= 128;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_BEAR, &token);
		token.qty = get_dead_count(player, TYPE_CREATURE | GDC_NONTOKEN);
		generate_token(&token);
	}

	return flash(player, card, event);
}

int card_canopy_crawler(int player, int card, event_t event){
/* CARD_ID_CANOPY_CRAWLER	6347
Canopy Crawler	|3|G
Creature - Beast 2/2
Amplify 1 (As this creature enters the battlefield, put a +1/+1 counter on it for each Beast card you reveal in your hand.)
|T: Target creature gets +1/+1 until end of turn for each +1/+1 counter on ~. */

	amplify(player, card, event, SUBTYPE_BEAST, 1);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = count_1_1_counters(instance->parent_controller, instance->parent_card);
			pump_until_eot(instance->parent_controller, instance->parent_card,
							instance->targets[0].player, instance->targets[0].card, amount, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_chromeshell_crab(int player, int card, event_t event){
/* CARD_ID_CHROMESHELL_CRAB	6350
Chromeshell Crab	|4|U
Creature - Crab Beast 3/3
Morph |4|U (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, you may exchange control of target creature you control and target creature an opponent controls. */

	if( event == EVENT_TURNED_FACE_UP ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.allowed_controller = 1-player;
		td1.preferred_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && can_target(&td1) ){
			if( select_target(player, card, &td, "Select a creature your control", &(instance->targets[0])) ){
				if( select_target(player, card, &td1, "Select a creature opponent controls", &(instance->targets[1])) ){
					exchange_control_of_target_permanents(player, card, instance->targets[0].player, instance->targets[0].card,
															instance->targets[1].player, instance->targets[1].card);
				}
			}
		}
	}

	return morph(player, card, event, MANACOST_XU(4, 1));
}

int card_clickslither(int player, int card, event_t event){
/* CARD_ID_CLICKSLITHER	6351
Clickslither	|1|R|R|R
Creature - Insect 3/3
Haste
Sacrifice a Goblin: ~ gets +2/+2 and gains trample until end of turn. */

	if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a Goblin to sacrifice.");
		test.subtype = SUBTYPE_GOBLIN;
		int num_can_sac = max_can_sacrifice_as_cost(player, card, &test);
		if (num_can_sac <= 0)
			return 0;

		return generic_shade_amt_can_pump(player, card, 2, 0, MANACOST0, num_can_sac);
	}

	haste(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Goblin to sacrifice.");
	test.subtype = SUBTYPE_GOBLIN;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								2, 2, KEYWORD_TRAMPLE, 0);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, 0, &test, 0);
}

int card_corpse_harvester(int player, int card, event_t event){
/* CARD_ID_CORPSE_HARVESTER	6353
Corpse Harvester	|3|B|B
Creature - Zombie Wizard 3/3
|1|B, |T, Sacrifice a creature: Search your library for a Zombie card and |Ha Swamp card, reveal them, and put them into your hand.
Then shuffle your library. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a Zombie card.");
		this_test.subtype = SUBTYPE_ZOMBIE;
		this_test.no_shuffle = 1;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);

		new_default_test_definition(&this_test, TYPE_LAND, get_hacked_land_text(player, card, "Select %a card.", SUBTYPE_SWAMP));
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_SWAMP);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_CREATURE, MANACOST_XB(1, 1), 0, NULL, NULL);
}

int crypt_sliver_shared_ability(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[1].player > -1 ){
		int p = instance->targets[1].player;
		int c = instance->targets[1].card;

		if( leaves_play(p, c, event) ){
			add_status(player, card, STATUS_INVISIBLE_FX);
		}

		if( ! in_play(p, c) ){
			if( event == EVENT_CAN_ACTIVATE ){
				return 0;
			}

			if( event == EVENT_CLEANUP ){
				kill_card(player, card, KILL_EXILE);
			}
		}

		if( is_humiliated(p, c) ){
			return 0;
		}

		if( ! IS_GAA_EVENT(event) ){
			return 0;
		}

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.preferred_controller = player;
		td2.required_subtype = SUBTYPE_SLIVER;
		td2.required_state = TARGET_STATE_DESTROYED;
		if( player == AI ){
			td2.special = TARGET_SPECIAL_REGENERATION;
		}


		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( would_validate_arbitrary_target(&td2, instance->targets[2].player, instance->targets[2].card) ){
				if( can_be_regenerated(instance->targets[2].player, instance->targets[2].card) ){
					regenerate_target(instance->targets[2].player, instance->targets[2].card);
				}
			}
		}

		return shared_sliver_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT | GAA_REGENERATION,
												MANACOST0, 0, &td2, "Select target Sliver to regenerate.");
	}

	return 0;
}

int card_crypt_sliver(int player, int card, event_t event){
	/*
	  Crypt Sliver |1|B
	  Creature - Sliver 1/1
	  All Slivers have "{T}: Regenerate target Sliver."
	*/

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &crypt_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &crypt_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	if( ! is_humiliated(player, card) && event == EVENT_ABILITIES ){
		if( in_play(affected_card_controller, affected_card) && is_what(affected_card_controller, affected_card, TYPE_PERMANENT) &&
			has_subtype(affected_card_controller, affected_card, SUBTYPE_SLIVER) )
		{
			event_result |= KEYWORD_REGENERATION;
		}
	}

	return slivercycling(player, card, event);
}

int card_dark_supplicant(int player, int card, event_t event){
/* CARD_ID_DARK_SUPPLICANT	6358
Dark Supplicant	|B
Creature - Human Cleric 1/1
|T, Sacrifice three Clerics: Search your graveyard, hand, and/or library for a card named Scion of Darkness and put it onto the battlefield.
If you search your library this way, shuffle it. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Cleric to sacrifice.");
	test.subtype = SUBTYPE_CLERIC;

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			test.qty = 3;
			if( new_can_sacrifice_as_cost(player, card, &test) ){
				return 1;
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int sacced = 0;
			int sacs[3];
			while( sacced < 3 ){
					int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
					if (!sac){
						cancel = 1;
						break;
					}
					sacs[sacced] = BYTE3(sac);
					sacced++;
			}
			if( sacced == 3 ){
				tap_card(player, card);
			}
			int i;
			for(i=0; i<sacced; i++){
				state_untargettable(player, sacs[i], 0);
				if( sacced == 3 ){
					kill_card(player, sacs[i], KILL_SACRIFICE);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a Scion of Darkness card.");
		this_test.id = CARD_ID_SCION_OF_DARKNESS;
		this_test.zone = TARGET_ZONE_HAND;

		int result = -1;
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			result = new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);
		}
		if( result == -1 && new_special_count_grave(player, &this_test) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);
		}
		if( result == -1 ){
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);
		}
	}

	return 0;
}

int card_daru_sanctifier(int player, int card, event_t event){
/* CARD_ID_DARU_SANCTIFIER	6360
Daru Sanctifier	|3|W
Creature - Human Cleric 1/4
Morph |1|W (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, destroy target enchantment. */

	if( event == EVENT_TURNED_FACE_UP ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ENCHANTMENT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		instance->number_of_targets = 0;

		if( can_target(&td) && pick_target(&td, "TARGET_ENCHANTMENT") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return morph(player, card, event, MANACOST_XW(1, 1));
}

int card_defender_of_the_order(int player, int card, event_t event){
/* CARD_ID_DEFENDER_OF_THE_ORDER	6363
Defender of the Order	|3|W
Creature - Human Cleric 2/4
Morph |W|W (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, creatures you control get +0/+2 until end of turn. */

	if( event == EVENT_TURNED_FACE_UP ){
		pump_subtype_until_eot(player, card, player, -1, 0, 2, 0, 0);
	}

	return morph(player, card, event, MANACOST_W(2));
}

int card_deftblade_elite(int player, int card, event_t event){
/* CARD_ID_DEFTBLADE_ELITE	6365
Deftblade Elite	|W
Creature - Human Soldier 1/1
Provoke (Whenever this creature attacks, you may have target creature defending player controls untap and block it if able.)
|1|W: Prevent all combat damage that would be dealt to and dealt by ~ this turn. */

	provoke(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		maze_of_ith_effect(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XW(1, 1), 0, NULL, NULL);
}

int card_dreamborn_muse(int player, int card, event_t event){
/* CARD_ID_DREAMBORN_MUSE	6367
Dreamborn Muse	|2|U|U
Creature - Spirit 2/2
At the beginning of each player's upkeep, that player puts the top X cards of his or her library into his or her graveyard,
where X is the number of cards in his or her hand. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		mill(current_turn, hand_count[current_turn]);
	}

	return 0;
}

int card_drinker_of_sorrow(int player, int card, event_t event){
/*
Drinker of Sorrow |2|B
Creature — Horror 5/3
Drinker of Sorrow can't block.
Whenever Drinker of Sorrow deals combat damage, sacrifice a permanent.
*/
	cannot_block(player, card, event);

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent to sacrifice.");
		new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	return 0;
}

int card_echo_tracer(int player, int card, event_t event){
/* CARD_ID_ECHO_TRACER	6371
Echo Tracer	|2|U
Creature - Human Wizard 2/2
Morph |2|U (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, return target creature to its owner's hand. */

	if( event == EVENT_TURNED_FACE_UP ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		instance->number_of_targets = 0;

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return morph(player, card, event, MANACOST_XU(2, 1));
}

int card_embalmed_brawler(int player, int card, event_t event){
/* CARD_ID_EMBALMED_BRAWLER	6373
Embalmed Brawler	|2|B
Creature - Zombie 2/2
Amplify 1 (As this creature enters the battlefield, put a +1/+1 counter on it for each Zombie card you reveal in your hand.)
Whenever ~ attacks or blocks, you lose 1 life for each +1/+1 counter on it. */

	amplify(player, card, event, SUBTYPE_ZOMBIE, 1);

	// Whenever ~ attacks or blocks, you lose 1 life for each +1/+1 counter on it.
	if( declare_attackers_trigger(player, card, event, 0, player, card) ||
		(blocking(player, card, event) && !is_humiliated(player, card)) )
	{
		lose_life(player, count_1_1_counters(player, card));
	}

	return 0;
}

int card_essence_sliver(int player, int card, event_t event){
/*
Essence Sliver |3|W
Creature — Sliver 3/3
Whenever a Sliver deals damage, its controller gains that much life.
*/
	card_instance_t *damage = combat_damage_being_dealt(event);
	if( damage ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_SLIVER;
		if( new_make_test_in_play(damage->damage_source_player, damage->damage_source_card, -1, &test) ){
			card_instance_t *instance = get_card_instance(player, card);
			if( instance->targets[8].player < 0 ){
				instance->targets[8].player = 0;
			}
			instance->targets[8].player++;
			if( instance->targets[1].player < 0 ){
				instance->targets[1].player = 0;
			}
			if( damage->damage_source_player == 0 ){
				SET_BYTE0(instance->targets[1].player) += damage->info_slot;
			}
			if( damage->damage_source_player == 1 ){
				SET_BYTE1(instance->targets[1].player) += damage->info_slot;
			}
		}
	}

	if( resolve_damage_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int slivers[2] = {BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player)};
		instance->targets[1].player = 0;
		gain_life(0, slivers[0]);
		gain_life(1, slivers[1]);
	}

	return 0;
}

int card_feral_throwback(int player, int card, event_t event){
/* CARD_ID_FERAL_THROWBACK	6376
Feral Throwback	|4|G|G
Creature - Beast 3/3
Amplify 2 (As this creature enters the battlefield, put two +1/+1 counters on it for each Beast card you reveal in your hand.)
Provoke (Whenever this creature attacks, you may have target creature defending player controls untap and block it if able.) */

	amplify(player, card, event, SUBTYPE_BEAST, 2);

	provoke(player, card, event);

	return 0;
}

int card_frenetic_raptor(int player, int card, event_t event){
/* CARD_ID_FRENETIC_RAPTOR	6378
Frenetic Raptor	|5|R
Creature - Lizard Beast 6/6
Beasts can't block. */

	if( event == EVENT_BLOCK_LEGALITY && has_subtype(affected_card_controller, affected_card, SUBTYPE_BEAST) &&
		! is_humiliated(player, card) )
	{
		event_result = 1;
	}

	return 0;
}

int card_gempalm_avenger(int player, int card, event_t event){
/* CARD_ID_GEMPALM_AVENGER	6379
Gempalm Avenger	|5|W
Creature - Human Soldier 3/5
Cycling |2|W (|2|W, Discard this card: Draw a card.)
When you cycle ~, Soldier creatures get +1/+1 and gain first strike until end of turn. */

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND ){
		pump_subtype_until_eot(player, card, 2, SUBTYPE_SOLDIER, 1, 1, KEYWORD_FIRST_STRIKE, 0);
	}

	return cycling(player, card, event, MANACOST_XW(2, 1));
}

int card_gempalm_incinerator(int player, int card, event_t event){
/* CARD_ID_GEMPALM_INCINERATOR	6380
Gempalm Incinerator	|2|R
Creature - Goblin 2/1
Cycling |1|R (|1|R, Discard this card: Draw a card.)
When you cycle ~, you may have it deal X damage to target creature, where X is the number of Goblins on the battlefield. */

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		int amount = count_subtype(2, TYPE_PERMANENT, SUBTYPE_GOBLIN);
		if( amount > 0 && can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, amount, player, card);
		}
	}

	return cycling(player, card, event, MANACOST_XR(1, 1));
}

int card_gempalm_polluter(int player, int card, event_t event){
/* CARD_ID_GEMPALM_POLLUTER	6381
Gempalm Polluter	|5|B
Creature - Zombie 4/3
Cycling |B|B (|B|B, Discard this card: Draw a card.)
When you cycle ~, you may have target player lose life equal to the number of Zombies on the battlefield. */

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		int amount = count_subtype(2, TYPE_PERMANENT, SUBTYPE_ZOMBIE);
		if( amount > 0 && can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			lose_life(instance->targets[0].player, amount);
		}
	}

	return cycling(player, card, event, MANACOST_B(2));
}

int card_gempalm_sorcerer(int player, int card, event_t event){
/* CARD_ID_GEMPALM_SORCERER	6382
Gempalm Sorcerer	|2|U
Creature - Human Wizard 2/2
Cycling |2|U (|2|U, Discard this card: Draw a card.)
When you cycle ~, Wizard creatures gain flying until end of turn. */

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND ){
		pump_subtype_until_eot(player, card, 2, SUBTYPE_WIZARD, 0, 0, KEYWORD_FLYING, 0);
	}

	return cycling(player, card, event, MANACOST_XU(2, 1));
}

int card_gempalm_strider(int player, int card, event_t event){
/* CARD_ID_GEMPALM_STRIDER	6383
Gempalm Strider	|1|G
Creature - Elf 2/2
Cycling |2|G|G (|2|G|G, Discard this card: Draw a card.)
When you cycle ~, Elf creatures get +2/+2 until end of turn. */

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND ){
		pump_subtype_until_eot(player, card, 2, SUBTYPE_ELF, 2, 2, 0, 0);
	}

	return cycling(player, card, event, MANACOST_XG(2, 2));
}

int card_ghastly_remains(int player, int card, event_t event){
/* CARD_ID_GHASTLY_REMAINS	6384
Ghastly Remains	|B|B|B
Creature - Zombie 0/0
Amplify 1 (As this creature enters the battlefield, put a +1/+1 counter on it for each Zombie card you reveal in your hand.)
At the beginning of your upkeep, if ~ is in your graveyard, you may pay |B|B|B. If you do, return ~ to your hand. */

	amplify(player, card, event, SUBTYPE_ZOMBIE, 1);

	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP && has_mana(player, COLOR_BLACK, 3) ){
		card_instance_t *instance = get_card_instance( player, card );
		int choice = do_dialog(player, player, card, -1, -1," Return Ghastly Remains to hand\n Pass\n", 0);
		if( choice == 0 ){
			charge_mana(player, COLOR_BLACK, 3);
			if( spell_fizzled != 1 ){
				instance->state &= ~STATE_INVISIBLE;
				hand_count[player]++;
				return -1;
			}
		}
		return -2;
	}

	return 0;
}

// glowrider --> thorn_of_amethyst

int card_goblin_clearcutter(int player, int card, event_t event){
/* CARD_ID_GOBLIN_CLEARCUTTER	6389
Goblin Clearcutter	|3|R
Creature - Goblin 3/3
|T, Sacrifice |Ha Forest: Add three mana in any combination of |R and/or |G to your mana pool. */


	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, get_hacked_land_text(player, card, "Select a %s to sacrifice.", SUBTYPE_FOREST));
	test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);

	if( event == EVENT_CAN_ACTIVATE && mana_producer(player, card, event) ){
		return new_can_sacrifice_as_cost(player, card, &test);
	}

	if(event == EVENT_ACTIVATE ){
		ai_modifier -= 36;
		if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
			add_state(player, card, STATE_TAPPED);
			produce_mana_tapped_any_combination_of_colors(player, card, COLOR_TEST_RED|COLOR_TEST_GREEN, 3, NULL);
		}
		else{
			spell_fizzled = 1;
		}
	}

	return 0;
}

int card_goblin_goon(int player, int card, event_t event){
/* CARD_ID_GOBLIN_GOON	6392
Goblin Goon	|3|R
Creature - Goblin Mutant 6/6
~ can't attack unless you control more creatures than defending player.
~ can't block unless you control more creatures than attacking player. */

	if( ! is_humiliated(player, card) ){
		if( event == EVENT_ATTACK_LEGALITY && affect_me(player, card) ){
			if( count_subtype(player, TYPE_CREATURE, -1) <= count_subtype(1-player, TYPE_CREATURE, -1) ){
				event_result = 1;
			}
		}

		if( event == EVENT_BLOCK_LEGALITY && affect_me(player, card) ){
			if( count_subtype(player, TYPE_CREATURE, -1) <= count_subtype(1-player, TYPE_CREATURE, -1) ){
				event_result = 1;
			}
		}
	}

	return 0;
}

// goblin grappler -> brontotherium

int card_goblin_lookout(int player, int card, event_t event){
/* CARD_ID_GOBLIN_LOOKOUT	6394
Goblin Lookout	|1|R
Creature - Goblin 1/2
|T, Sacrifice a Goblin: Goblin creatures get +2/+0 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Goblin to sacrifice.");
	test.subtype = SUBTYPE_GOBLIN;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, SUBTYPE_GOBLIN, 2, 0, 0, 0);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL, 0, &test, 0);
}

int card_graveborn_muse(int player, int card, event_t event){
/* CARD_ID_GRAVEBORN_MUSE	6396
Graveborn Muse	|2|B|B
Creature - Zombie Spirit 3/3
At the beginning of your upkeep, you draw X cards and you lose X life, where X is the number of Zombies you control. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int count = count_subtype(player, TYPE_PERMANENT, SUBTYPE_ZOMBIE);
		lose_life(player, count);
		draw_cards(player, count);
	}

	return 0;
}

int card_havok_demon(int player, int card, event_t event){
/* CARD_ID_HAVOC_DEMON	6397
Havoc Demon	|5|B|B
Creature - Demon 5/5
Flying
When ~ dies, all creatures get -5/-5 until end of turn. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		pump_subtype_until_eot(player, card, ANYBODY, -1, -5, -5, 0, 0);
	}

	return 0;
}

static void hollow_specter_ability(int player, int card, int t_player){
	charge_mana(player, COLOR_COLORLESS, -1);
	if( spell_fizzled != 1 && x_value > 0){
		ec_definition_t ec;
		default_ec_definition(t_player, player, &ec);
		ec.cards_to_reveal = x_value;

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		new_effect_coercion(&ec, &this_test);
	}
}

int card_hollow_specter(int player, int card, event_t event){
/*
Hollow Specter |1|B|B
Creature — Specter 2/2
Flying
Whenever Hollow Specter deals combat damage to a player, you may pay {X}. If you do, that player reveals X cards from his or her hand and
	you choose one of them. That player discards that card.
*/
	int result = whenever_i_deal_damage_to_a_player(player, card, event, RESOLVE_TRIGGER_AI(player) | DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					if( times_damaged[p] ){
						while( times_damaged[p] ){
							hollow_specter_ability(player, card, p);
							times_damaged[p]--;
						}
					}
				};
		);
	}

	return 0;
}

int card_hunter_sliver(int player, int card, event_t event, int trigger_mode)
{
/* CARD_ID_HUNTER_SLIVER	6400
Hunter Sliver	|1|R
Creature - Sliver 1/1
All Sliver creatures have provoke. (Whenever a Sliver attacks, its controller may have target creature defending player controls untap and block it if able.)*/

  if (xtrigger_condition() == XTRIGGER_ATTACKING || event == EVENT_DECLARE_ATTACKERS)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.subtype = SUBTYPE_SLIVER;

	  int amt;
	  if ((amt = declare_attackers_trigger_test(player, card, event, RESOLVE_TRIGGER_AI(player) | DAT_TRACK, 2, -1, &test)))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
		  for (--amt; amt >= 0; --amt)
			provoke_impl(player, card, current_turn, attackers[amt], event);
		}
	}

  return slivercycling(player, card, event);
}

int card_imperial_hellkite(int player, int card, event_t event){
/* CARD_ID_IMPERIAL_HELLKITE	6401
Imperial Hellkite	|5|R|R
Creature - Dragon 6/6
Flying
Morph |6|R|R (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, you may search your library for a Dragon card, reveal it, and put it into your hand. If you do, shuffle your library.*/

	if( event == EVENT_TURNED_FACE_UP ){
		if( player == AI || do_dialog(player, player, card, -1, -1, " Tutor a Dragon\n Pass", 0) == 0 ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, 0, "Select a Dragon card.");
			this_test.subtype = SUBTYPE_DRAGON;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		}
	}
	return morph(player, card, event, MANACOST_XR(6, 2));
}

int card_infernal_caretaker(int player, int card, event_t event){
	/* Infernal Caretaker	|3|B
	 * Creature - Human Cleric 2/2
	 * Morph |3|B
	 * When ~ is turned face up, return all Zombie cards from all graveyards to their owners' hands. */

	if( event == EVENT_TURNED_FACE_UP ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "");
		test.subtype = SUBTYPE_ZOMBIE;

		int i;
		for(i=0; i<2; i++){
			from_grave_to_hand_multiple(i ? current_turn : 1-current_turn, &test);
		}
	}
	return morph(player, card, event, MANACOST_XB(3, 1));
}

int card_keeper_of_the_nine_gales(int player, int card, event_t event){
/* CARD_ID_KEEPER_OF_THE_NINE_GALES	6404
Keeper of the Nine Gales	|2|U
Creature - Bird Wizard 1/2
Flying
|T, Tap two untapped Birds you control: Return target permanent to its owner's hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.required_subtype = SUBTYPE_BIRD;
	td.illegal_state = TARGET_STATE_TAPPED;
	td.preferred_controller = player;
	td.allowed_controller = player;
	td.illegal_abilities = 0;
	td.special = TARGET_SPECIAL_NOT_ME;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td2, NULL) ){
			return target_available(player, card, &td) > 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int total = 0;
		int bt[2];
		while( total < 2 ){
				instance->number_of_targets = 0;
				if( new_pick_target(&td, "Select another Bird to tap.", 0, GS_LITERAL_PROMPT) ){
					bt[total] = instance->targets[0].card;
					state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
					total++;
				}
				else{
					break;
				}
		}
		int i;
		for(i=0; i<total; i++){
			state_untargettable(player, bt[i], 0);
		}
		if( total == 2 ){
			instance->number_of_targets = 0;
			if( pick_target(&td2, "TARGET_PERMANENT") ){
				for(i=0; i<total; i++){
					tap_card(player, bt[i]);
				}
				tap_card(player, card);
			}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td2) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_kilnmouth_dragon(int player, int card, event_t event){
	/* Kilnmouth Dragon	|5|R|R	0x20093f5
	 * Creature - Dragon 5/5
	 * Amplify 3
	 * Flying
	 * |T: ~ deals damage equal to the number of +1/+1 counters on it to any target. */
	amplify(player, card, event, SUBTYPE_DRAGON, 3);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = count_1_1_counters(instance->parent_controller, instance->parent_card);
			damage_target0(player, card, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ANY");
}

int card_krosan_cloudscrapper(int player, int card, event_t event){
/* CARD_ID_KROSAN_CLOUDSCRAPER	6406
Krosan Cloudscraper	|7|G|G|G
Creature - Beast Mutant 13/13
At the beginning of your upkeep, sacrifice ~ unless you pay |G|G.
Morph |7|G|G (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	basic_upkeep(player, card, event, MANACOST_G(2));
	return morph(player, card, event, MANACOST_XG(7, 2));
}

int card_lavaborn_muse(int player, int card, event_t event){
/* CARD_ID_LAVABORN_MUSE	6408
Lavaborn Muse	|3|R
Creature - Spirit 3/3
At the beginning of each opponent's upkeep, if that player has two or fewer cards in hand, ~ deals 3 damage to him or her. */

	if (hand_count[1-player] <= 2){
		upkeep_trigger_ability(player, card, event, 1 - player);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if (hand_count[1-player] <= 2){	// has to be checked each time, stupidly enough, due to things like Farsight Mask
			damage_player(current_turn, 3, player, card);
		}
	}

	return 0;
}

int magma_sliver_shared_ability(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[1].player > -1 ){
		int p = instance->targets[1].player;
		int c = instance->targets[1].card;

		if( leaves_play(p, c, event) ){
			add_status(player, card, STATUS_INVISIBLE_FX);
		}

		if( ! in_play(p, c) ){
			if( event == EVENT_CAN_ACTIVATE ){
				return 0;
			}

			if( event == EVENT_CLEANUP ){
				kill_card(player, card, KILL_EXILE);
			}
		}

		if( is_humiliated(p, c) ){
			return 0;
		}

		if( ! IS_GAA_EVENT(event) ){
			return 0;
		}

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.preferred_controller = player;
		td2.required_subtype = SUBTYPE_SLIVER;


		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( would_validate_arbitrary_target(&td2, instance->targets[2].player, instance->targets[2].card) ){
				int amount = count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_SLIVER);
				pump_until_eot(instance->targets[0].player, instance->targets[0].card, instance->targets[2].player, instance->targets[2].card, amount, 0);
			}
		}

		return shared_sliver_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
												&td2, "Select target Sliver to pump.");
	}

	return 0;
}

int card_magma_sliver(int player, int card, event_t event){
	/*
	  Magma Sliver |3|R
	  Creature - Sliver 3/3
	  All Slivers have "{T}: Target Sliver creature gets +X/+0 until end of turn, where X is the number of Slivers on the battlefield."
	*/

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_legacy_activate(player, card, &magma_sliver_shared_ability);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		int fake = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
		legacy = create_legacy_activate(1-player, fake, &magma_sliver_shared_ability);
		instance = get_card_instance(1-player, legacy);
		instance->targets[1].player = player;
		instance->targets[1].card = card;
		instance->number_of_targets = 1;
		obliterate_card(1-player, fake);
	}

	return slivercycling(player, card, event);
}

int card_mistform_ultimus(int player, int card, event_t event){
/* CARD_ID_MISTFORM_ULTIMUS	6416
Mistform Ultimus	|3|U
Legendary Creature - Illusion 3/3
~ is every creature type (even if this card isn't on the battlefield). */

	check_legend_rule(player, card, event);
	changeling_switcher(player, card, event);
	return 0;
}

int card_nantuko_vigilante(int player, int card, event_t event){
/* CARD_ID_NANTUKO_VIGILANTE	6418
Nantuko Vigilante	|3|G
Creature - Insect Druid Mutant 3/2
Morph |1|G (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, destroy target artifact or enchantment. */

	if( event == EVENT_TURNED_FACE_UP ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ENCHANTMENT | TYPE_ARTIFACT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "DISENCHANT") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return morph(player, card, event, MANACOST_XG(1, 1));
}

int card_noxious_ghoul(int player, int card, event_t event){
/* CARD_ID_NOXIOUS_GHOUL	6420
Noxious Ghoul	|3|B|B
Creature - Zombie 3/3
Whenever ~ or another Zombie enters the battlefield, all non-Zombie creatures get -1/-1 until end of turn. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_ZOMBIE;

		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_ZOMBIE;
		test.subtype_flag = DOESNT_MATCH;
		APNAP(p, { pump_creatures_until_eot(player, card, p, 0, -1, -1, 0, 0, &test);};);
	}

	return 0;
}

int card_patron_of_the_wild(int player, int card, event_t event){
/* CARD_ID_PATRON_OF_THE_WILD	6421
Patron of the Wild	|G
Creature - Elf 1/1
Morph |2|G (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, target creature gets +3/+3 until end of turn. */

	if( event == EVENT_TURNED_FACE_UP ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3, 3);
		}
		instance->number_of_targets = 0;
	}

	return morph(player, card, event, MANACOST_XG(2, 1));
}

int card_phage_the_untouchable(int player, int card, event_t event){
/*
Phage the Untouchable |3|B|B|B|B
Legendary Creature — Avatar Minion 4/4
When Phage the Untouchable enters the battlefield, if you didn't cast it from your hand, you lose the game.
Whenever Phage deals combat damage to a creature, destroy that creature. It can't be regenerated.
Whenever Phage deals combat damage to a player, that player loses the game.
*/
	check_legend_rule(player, card, event);

	deathtouch(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->info_slot = not_played_from_hand(player, card);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && get_card_instance(player, card)->info_slot == 1 ){
		lose_the_game(player);
	}

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		if( times_damaged[0] && times_damaged[1] ){
			lose_the_game(ANYBODY);
		}
		else{
			if( times_damaged[0] ){
				lose_the_game(0);
			}
			if( times_damaged[1] ){
				lose_the_game(1);
			}
		}
	}

	return 0;
}

static const char* is_destroyed_or_buried(int who_chooses, int player, int card)
{
	int kill_code = is_token(player, card) ? 0 : get_card_instance(player, card)->kill_code;
	if( kill_code == KILL_DESTROY || kill_code == KILL_BURY ){
		return NULL;
	}
	return "AI function";
}

int card_planar_guide(int player, int card, event_t event){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int32_t)is_destroyed_or_buried;
	td.illegal_abilities = 0;
	td.preferred_controller = player;

	if( event == EVENT_CAN_ACTIVATE ){
		if( player == HUMAN ){
			return generic_activated_ability(player, card, event, GAA_RFG_ME, MANACOST_XW(3, 1), 0, NULL, NULL);
		}
		else{
			if( land_can_be_played & LCBP_REGENERATION ){
				if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_RFG_ME, MANACOST_XW(3, 1), 0, &td, NULL) ){
					return 99;
				}
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( player == HUMAN ){
			return generic_activated_ability(player, card, event, GAA_RFG_ME, MANACOST_XW(3, 1), 0, NULL, NULL);
		}
		else{
			generic_activated_ability(player, card, event, GAA_RFG_ME, MANACOST_XW(3, 1), 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				CYCLE_ALL_CARDS(p, c, 	{
											if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) && !(p == player && c == card) &&
												! is_token(p, c) )
											{
												int kill_code = get_card_instance(player, card)->kill_code;
												if( kill_code == KILL_DESTROY || kill_code == KILL_BURY ){
													int pow = get_power(p, c);
													ai_modifier += (p == player ? (pow*8) : -(pow*8));
												}
											}
										};
				);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		APNAP(p,{ new_manipulate_all(instance->parent_controller, instance->parent_card, p, &this_test, ACT_RFG_UNTIL_EOT); };);
	}

	return generic_activated_ability(player, card, event, GAA_RFG_ME, MANACOST_XW(3, 1), 0, NULL, NULL);
}

int card_plated_sliver(int player, int card, event_t event){
/* CARD_ID_PLATED_SLIVER	1385
Plated Sliver	|W
Creature - Sliver 1/1
All Sliver creatures get +0/+1. */

	boost_creature_type(player, card, event, SUBTYPE_SLIVER, 0, 1, 0, BCT_INCLUDE_SELF);
	return slivercycling(player, card, event);
}

int card_riptide_director(int player, int card, event_t event){
/* CARD_ID_RIPTIDE_DIRECTOR	6428
Riptide Director	|2|U|U
Creature - Human Wizard 2/3
|2|U|U, |T: Draw a card for each Wizard you control. */

	if( event == EVENT_ACTIVATE ){
		if( count_deck(player) > 10 ){
			ai_modifier +=  (count_subtype(player, TYPE_LAND, -1) * count_subtype(player, TYPE_PERMANENT, SUBTYPE_WIZARD));
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, count_subtype(player, TYPE_PERMANENT, SUBTYPE_WIZARD));
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XU(2, 2), 0, NULL, NULL);
}

int card_riptide_mangler(int player, int card, event_t event){
/* CARD_ID_RIPTIDE_MANGLER	6429
Riptide Mangler	|1|U
Creature - Beast 0/3
|1|U: Change ~'s base power to target creature's power. (This effect lasts indefinitely.) */

	if( event == EVENT_POW_BOOST ){
		int actual_power = get_power(player, card);
		int boost = 0;
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = ANYBODY;
		CYCLE_ALL_CARDS(p, c, 	{
									if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
										int this_power = get_power(p, c);
										if( (this_power - actual_power) > boost && would_validate_arbitrary_target(&td, p, c) ){
											boost = this_power - actual_power;
										}
									}
								};
		);
		return boost;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XU(1, 1), 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			ai_modifier += ((get_power(instance->targets[0].player, instance->targets[0].card)-get_power(player, card))*8);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int pow = get_power(instance->targets[0].player, instance->targets[0].card);
			set_pt_and_abilities_until_eot(instance->parent_controller, instance->parent_card,
											instance->parent_controller, instance->parent_card, pow, 0, 0, 0, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XU(1, 1), 0, &td, "TARGET_CREATURE");
}

int card_root_sliver(int player, int card, event_t event){
/* CARD_ID_ROOT_SLIVER	6431
Root Sliver	|3|G
Creature - Sliver 2/2
~ can't be countered.
Sliver spells can't be countered by spells or abilities. */

	cannot_be_countered(player, card, event);

	if( event == EVENT_CAST_SPELL ){
		if( ! affect_me(player, card) ){
			test_definition_t this_test;
			default_test_definition(&this_test, 0);
			this_test.subtype = SUBTYPE_SLIVER;

			type_uncounterable(player, card, event, player, 0, &this_test);
		}
	}

	type_uncounterable(player, card, event, player, 0, NULL);

	return 0;
}

int card_scion_of_darkness(int player, int card, event_t event){
/*
Scion of Darkness |5|B|B|B
Creature — Avatar 6/6
Trample
Whenever Scion of Darkness deals combat damage to a player, you may put target creature card from that player's graveyard onto
	the battlefield under your control.
Cycling {3} ({3}, Discard this card: Draw a card.)
*/

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					if( times_damaged[p] ){
						while( times_damaged[p] ){
								if( count_graveyard_by_type(p, TYPE_CREATURE) && ! graveyard_has_shroud(p) ){
									char msg[100] = "Select target creature card.";
									test_definition_t this_test;
									new_default_test_definition(&this_test, TYPE_CREATURE, msg);
									new_global_tutor(player, p, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
								}
								times_damaged[p]--;
						}
					}
				};
		);
	}

	return cycling(player, card, event, MANACOST_X(3));
}

int card_seedborn_muse(int player, int card, event_t event){
/* CARD_ID_SEEDBORN_MUSE	6433
Seedborn Muse	|3|G|G
Creature - Spirit 2/4
Untap all permanents you control during each other player's untap step. */

	untap_permanents_during_opponents_untap(player, card, TYPE_PERMANENT, &get_card_instance(player, card)->info_slot);
	return 0;
}

int card_shifting_sliver(int player, int card, event_t event){
/* CARD_ID_SHIFTING_SLIVER	6435
Shifting Sliver	|3|U
Creature - Sliver 2/2
Slivers can't be blocked except by Slivers. */

	if( event == EVENT_BLOCK_LEGALITY && has_subtype(attacking_card_controller, attacking_card, SUBTYPE_SLIVER) &&
		! is_humiliated(player, card) )
	{
		if( ! has_subtype(affected_card_controller, affected_card, SUBTYPE_SLIVER) ){
			event_result = 1;
		}
	}

	return 0;
}

int card_skinthinner(int player, int card, event_t event){
/* CARD_ID_SKINTHINNER	6436
Skinthinner	|1|B
Creature - Zombie 2/1
Morph |3|B|B (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, destroy target non|Sblack creature. It can't be regenerated. */

	if( event == EVENT_TURNED_FACE_UP ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.illegal_color = COLOR_TEST_BLACK;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) &&
			new_pick_target(&td, get_sleighted_color_text(player, card, "Select target non%s creature", COLOR_BLACK), 0, GS_LITERAL_PROMPT))
		{
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		instance->number_of_targets = 0;
	}

	return morph(player, card, event, MANACOST_XB(3, 2));
}

static int sacrifice_at_eot(int player, int card, event_t event)
{
	if( trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player ){
		card_instance_t* instance = get_card_instance(player, card);
		if( instance->targets[1].player > 0 ){
			instance->targets[1].player--;
		}
		else{
			if( eot_trigger(player, card, event) ){
				if( ! check_state(instance->damage_target_player, instance->damage_target_card, STATE_OUBLIETTED) ){
					kill_card(instance->damage_target_player, instance->damage_target_card, KILL_SACRIFICE);
				}
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	return 0;
}

int card_skirk_alarmist(int player, int card, event_t event)
{
  /* Skirk Alarmist	|1|R
   * Creature - Human Wizard 1/2
   * Haste
   * |T: Turn target face-down creature you control face up. At the beginning of the next end step, sacrifice it. */

	haste(player, card, event);

	if( !IS_GAA_EVENT(event) )
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.extra = get_internal_card_id_from_csv_id(CARD_ID_FACE_DOWN_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target face-down creature you control.");
		if( spell_fizzled != 1 ){
			card_instance_t *aff = get_card_instance(player, card);
			int iid = get_internal_card_id_from_csv_id( aff->targets[12].player );
			ai_modifier += (get_base_power_iid(instance->targets[0].player, iid)*8);
		}
		return 0;
	}

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
		int legacy = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &sacrifice_at_eot,
										instance->targets[0].player, instance->targets[0].card);
		if( current_phase == PHASE_DISCARD ){
			get_card_instance(instance->parent_controller, legacy)->targets[1].player = 1;
		}
		flip_card(instance->targets[0].player, instance->targets[0].card);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target face-down creature you control.");
}

int card_skirk_marauder(int player, int card, event_t event){
	/* Skirk Marauder	|1|R	0x2009445
	 * Creature - Goblin 2/1
	 * Morph |2|R
	 * When ~ is turned face up, it deals 2 damage to any target. */
	if( event == EVENT_TURNED_FACE_UP ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
			damage_target0(player, card, 2);
		}
		get_card_instance(player, card)->number_of_targets = 0;
	}

	return morph(player, card, event, MANACOST_XR(2, 1));
}

int card_skirk_outrider(int player, int card, event_t event){
/* CARD_ID_SKIRK_OUTRIDER	6440
Skirk Outrider	|3|R
Creature - Goblin 2/2
As long as you control a Beast, ~ gets +2/+2 and has trample. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES) && affect_me(player, card) &&
		! is_humiliated(player, card) )
	{
		if( check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_BEAST) ){
			switch( event ){
					case EVENT_POWER:
					case EVENT_TOUGHNESS:
						event_result+=2;
						break;
					case EVENT_ABILITIES:
						event_result |= KEYWORD_TRAMPLE;
						break;
					default:
						break;
			}
		}
	}

	return 0;
}

int card_spectral_sliver(int player, int card, event_t event){
	/*
	  Spectral Sliver |2|B
	  Creature - Sliver Spirit 2/2
	  All Sliver creatures have "{2}: This creature gets +1/+1 until end of turn."
	*/
	return sliver_with_shared_shade_ability(player, card, event, 1, 1, 0, 0);
}

int card_synapse_sliver(int player, int card, event_t event){
/*
Synapse Sliver |4|U
Creature — Sliver 3/3
Whenever a Sliver deals combat damage to a player, its controller may draw a card.
*/
	card_instance_t *damage = combat_damage_being_dealt(event);
	if( damage ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_SLIVER;
		if( new_make_test_in_play(damage->damage_source_player, damage->damage_source_card, -1, &test) ){
			if( damage->damage_target_card == -1 && ! damage_is_to_planeswalker(damage) ){
				card_instance_t *instance = get_card_instance(player, card);
				if( instance->targets[8].player < 0 ){
					instance->targets[8].player = 0;
				}
				instance->targets[8].player++;
				if( instance->targets[1].player < 0 ){
					instance->targets[1].player = 0;
				}
				if( damage->damage_source_player == 0 ){
					SET_BYTE0(instance->targets[1].player)++;
				}
				if( damage->damage_source_player == 1 ){
					SET_BYTE1(instance->targets[1].player)++;
				}
			}
		}
	}

	if( resolve_damage_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int slivers[2] = {BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player)};
		instance->targets[1].player = 0;
		draw_some_cards_if_you_want(player, card, 0, slivers[0]);
		draw_some_cards_if_you_want(player, card, 1, slivers[1]);
	}

	return slivercycling(player, card, event);
}

int card_timberwatch_elf(int player, int card, event_t event){
/* CARD_ID_TIMBERWATCH_ELF	6450
Timberwatch Elf	|2|G
Creature - Elf 1/2
|T: Target creature gets +X/+X until end of turn, where X is the number of Elves on the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_ELF);
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							amount, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_totem_speaker(int player, int card, event_t event){
/* CARD_ID_TOTEM_SPEAKER	6451
Totem Speaker	|4|G
Creature - Elf Druid 3/3
Whenever a Beast enters the battlefield, you may gain 3 life. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_BEAST;

		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 3);
	}

	return 0;
}

int card_toxin_sliver(int player, int card, event_t event){
/* CARD_ID_TOXIN_SLIVER	6452
Toxin Sliver	|3|B
Creature - Sliver 3/3
Whenever a Sliver deals combat damage to a creature, destroy that creature. It can't be regenerated. */

	card_instance_t* damage = combat_damage_being_dealt(event);
	if (damage && damage->damage_target_card != -1 &&
		has_subtype(damage->damage_source_player, damage->damage_source_card, SUBTYPE_SLIVER) )
	{
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->info_slot < 40 ){
			unsigned char* creatures = (unsigned char*)(&instance->targets[0].player);
			creatures[2 * instance->info_slot] = damage->damage_target_player;
			creatures[2 * instance->info_slot + 1] = damage->damage_target_card;
			instance->info_slot++;
		}
	}

	if(trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player &&
		get_card_instance(player, card)->info_slot > 0 )
	{
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
			card_instance_t *instance = get_card_instance(player, card);
			unsigned char* creatures = (unsigned char*)(&instance->targets[0].player);
			int i;
			for (i = 0; i < instance->info_slot; ++i){
				if( in_play(creatures[2 * i], creatures[2 * i + 1]) ){
					kill_card(creatures[2 * i], creatures[2 * i + 1], KILL_BURY);
				}
			}
			instance->info_slot = 0;
		}
	}
	return slivercycling(player, card, event);
}

int card_tribal_forcemage(int player, int card, event_t event){
/* CARD_ID_TRIBAL_FORCEMAGE	6453
Tribal Forcemage	|1|G
Creature - Elf Wizard 1/1
Morph |1|G (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, creatures of the creature type of your choice get +2/+2 and gain trample until end of turn. */

	if( event == EVENT_TURNED_FACE_UP ){
		int subt = select_a_subtype(player, card);
		pump_subtype_until_eot(player, card, 2, subt, 2, 2, KEYWORD_TRAMPLE, 0);
	}
	return morph(player, card, event, MANACOST_XG(1, 1));
}

int card_voidmage_apprentice(int player, int card, event_t event)
{
/* CARD_ID_VOIDMAGE_APPRENTICE	6457
Voidmage Apprentice	|1|U
Creature - Human Wizard 1/1
Morph |2|U|U (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
When ~ is turned face up, counter target spell. */

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CAN_UNMORPH)
	{
	  if (!has_mana_multi(player, MANACOST_XU(2,2)))
		return 0;

	  int cs = instance->targets[9].card = counterspell(player, card, EVENT_CAN_CAST, NULL, -1);
	  return cs ? cs : 1;
	}

  int mf = morph(player, card, event, MANACOST_XU(2,2));

  if (event == EVENT_UNMORPH)
	{
	  if (cancel != 1 && card_on_stack_controller != -1 && instance->targets[9].card == 99)
		{
		  counterspell(player, card, EVENT_CAST_SPELL, NULL, 0);
		  if (cancel == 1)
			{
			  cancel = 0;
			  instance->targets[9].card = 0;
			  ai_modifier -= 96;
			}
		}
	  else
		ai_modifier -= 96;
	}

	if (event == EVENT_TURNED_FACE_UP){
		if (instance->targets[9].card == 99 && counterspell_validate(player, card, NULL, 0)){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		cancel = 0;
	}

	return mf;
}

int card_wall_of_deceit(int player, int card, event_t event){
/* CARD_ID_WALL_OF_DECEIT	6458
Wall of Deceit	|1|U
Creature - Wall 0/5
Defender (This creature can't attack.)
|3: Turn ~ face down.
Morph |U (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	if( IS_GAA_EVENT(event) ){
		if( event == EVENT_ACTIVATE ){
			ai_modifier += 12;
		}
		if( event == EVENT_RESOLVE_ACTIVATION ){
			card_instance_t *instance = get_card_instance(player, card);
			turn_face_down(instance->parent_controller, instance->parent_card);
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_X(3), 0, NULL, NULL);
	}

	return morph(player, card, event, MANACOST_U(1));
}

int card_wall_of_hope(int player, int card, event_t event){
/*
	Wall of Hope |W
	Creature — Wall 0/3
	Defender (This creature can't attack.)
	Whenever Wall of Hope is dealt damage, you gain that much life.
*/
	if( ! is_humiliated(player, card) ){
		if( damage_dealt_to_me_arbitrary(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
			card_instance_t *instance = get_card_instance(player, card);
			gain_life(player, instance->targets[7].card);
			instance->targets[7].card = 0;
		}
	}

	return 0;
}

int card_warbreak_trumpeter(int player, int card, event_t event)
{
  /* Warbreak Trumpeter	|R
   * Creature - Goblin 1/1
   * Morph |X|X|R
   * When ~ is turned face up, put X 1/1 |Sred Goblin creature tokens onto the battlefield. */

  if (event == EVENT_UNMORPH)
	{
	  charge_mana(player, COLOR_RED, 1);
	  if (cancel != 1)
		{
		  int result = charge_mana_for_double_x(player, COLOR_COLORLESS);
		  if (cancel != 1)
			get_card_instance(player, card)->info_slot = result / 2;
		}

	  return 0;
	}

  if (event == EVENT_TURNED_FACE_UP)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  generate_tokens_by_id(player, card, CARD_ID_GOBLIN, instance->info_slot);
	  instance->info_slot = 0;
	}

  return morph(player, card, event, MANACOST_R(1));
}

int card_ward_sliver(int player, int card, event_t event){
/* CARD_ID_WARD_SLIVER	6461
Ward Sliver	|4|W
Creature - Sliver 2/2
As ~ enters the battlefield, choose a color.
All Slivers have protection from the chosen color. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = select_a_protection(player);
	}

	if( get_card_instance(player, card)->info_slot > 0 && in_play(player, card) && ! is_humiliated(player, card) ){
		boost_creature_type(player, card, event, SUBTYPE_SLIVER, 0, 0, get_card_instance(player, card)->info_slot, BCT_INCLUDE_SELF);
	}

	return slivercycling(player, card, event);
}

// windborn muse --> propaganda

int card_wirewood_channeler(int player, int card, event_t event){
/* CARD_ID_WIREWOOD_CHANNELER	6468
Wirewood Channeler	|3|G
Creature - Elf Druid 2/2
|T: Add X mana of any one color to your mana pool, where X is the number of Elves on the battlefield. */

	int count = 0;
	if (event == EVENT_COUNT_MANA || event == EVENT_ACTIVATE || (event == EVENT_CAN_ACTIVATE && player == AI)){	// Only count if it'll actually be needed
		count = count_subtype(2, TYPE_PERMANENT, SUBTYPE_ELF);
	}
	return mana_producing_creature_all_one_color(player, card, event, 24, COLOR_TEST_ANY_COLORED, count);
}

int card_wirewood_hivemaster(int player, int card, event_t event){
	/* Wirewood Hivemaster	|1|G
	 * Creature - Elf 1/1
	 * Whenever another nontoken Elf enters the battlefield, you may put a 1/1 |Sgreen Insect creature token onto the battlefield. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.type_flag = F1_NO_TOKEN;
		this_test.subtype = SUBTYPE_ELF;
		this_test.not_me = 1;

		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_AI(player), 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		generate_token_by_id(player, card, CARD_ID_INSECT);
	}

	return 0;
}

int card_withered_wretch(int player, int card, event_t event){
/* CARD_ID_WITHERED_WRETCH	6470
Withered Wretch	|B|B
Creature - Zombie Cleric 2/2
|1: Exile target card from a graveyard. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select target card to exile.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			rfg_card_from_grave(instance->targets[0].player, selected);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER_BOTH_GRAVES);
}


