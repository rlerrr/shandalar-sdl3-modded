#include "manalink.h"

// Functions

static int is_basic_land_by_internal_id(int iid, int unused, int unused1, int unused2){
	return is_basic_land_by_id(cards_data[iid].id);
}

static int panorama(int player, int card, event_t event, int sub1, int sub2, int sub3){

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_TUTOR_LAND
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL) )
		{
			choice = CHOICE_TUTOR_LAND;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) && choice == CHOICE_TUTOR_LAND ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Produce mana", 1, 1,
							"Sac and tutor land", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_TUTOR_LAND ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_TUTOR_LAND ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND,
										get_hacked_land_text(player, card, "Select a basic %s, %s, or %s card.", sub1, sub2, sub3));
			this_test.subtype = get_hacked_subtype(player, card, sub1);
			this_test.sub2 = get_hacked_subtype(player, card, sub2);
			this_test.sub3 = get_hacked_subtype(player, card, sub3);
			this_test.subtype_flag = F2_MULTISUBTYPE;
			this_test.special_selection_function = &is_basic_land_by_internal_id;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY_TAPPED, 0, AI_FIRST_FOUND, &this_test);
		}
	}

	return 0;
}

int unearth(int player, event_t event, int colorless, int black, int blue, int green, int red, int white){
	/* 702.83a Unearth is an activated ability that functions while the card with unearth is in a graveyard. "Unearth
	 * [cost]" means "[Cost]: Return this card from your graveyard to the battlefield. It gains haste. Exile it at the
	 * beginning of the next end step. If it would leave the battlefield, exile it instead of putting it anywhere
	 * else. Activate this ability only any time you could cast a sorcery." */
	if(event == EVENT_GRAVEYARD_ABILITY){
		if( (has_mana_multi( player, colorless, black, blue, green, red, white )) && ( can_sorcery_be_played(player, event)) ){
			return GA_UNEARTH;
		}
	}
	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana_multi( player, colorless, black, blue, green, red, white );
		if( spell_fizzled != 1 ){
			return GAPAID_REMOVE;
		}
	}
	return 0;
}

int devouring(int player, int card){
	target_definition_t td;
	base_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	int devoured[151];
	int dc = 0;
	int to_devour = IS_AI(player) ? count_subtype(player, TYPE_CREATURE, -1) / 3 : -1;
	instance->targets[1].player = 0;
	instance->targets[1].card = 0;
	int count_goblins = get_id(player, card) == CARD_ID_VORACIOUS_DRAGON;
	while( can_target(&td) ){
			instance->number_of_targets = 0;
			int result = pick_creature_for_sacrifice(player, card, 0);
			if( result != -1 ){
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
				instance->targets[1].card++;
				if( count_goblins && has_subtype(player, result, SUBTYPE_GOBLIN) ){
					instance->targets[1].player++;
				}
				devoured[dc] = instance->targets[0].card;
				dc++;
				if( dc == to_devour ){
					break;
				}
			}
			else{
				break;
			}
	}
	int count = dc-1;
	while( count > -1 ){
			kill_card(player, devoured[count], KILL_SACRIFICE);
			count--;
	}
	return dc;
}


void devour(int player, int card, event_t event, int number){
	/* 702.81. Devour
	 *
	 * 702.81a Devour is a static ability. "Devour N" means "As this object enters the battlefield, you may sacrifice any number of creatures. This permanent
	 * enters the battlefield with N +1/+1 counters on it for each creature sacrificed this way."
	 *
	 * 702.81b Some objects have abilities that refer to the number of creatures the permanent devoured. "It devoured" means "sacrificed as a result of its
	 * devour ability as it entered the battlefield." */

	if( event == EVENT_RESOLVE_SPELL && can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE) ){
		card_instance_t* instance = get_card_instance(player, card);
		instance->state |= STATE_OUBLIETTED;
		int result = devouring(player, card);
		instance->state &= ~STATE_OUBLIETTED;
		add_1_1_counters(player, card, number * result);
	}
}

// Cards

int card_ad_nauseam(int player, int card, event_t event){
/* CARD_ID_AD_NAUSEAM	9908
Ad Nauseam	|3|B|B
Instant
Reveal the top card of your library and put that card into your hand. You lose life equal to its converted mana cost.
You may repeat this process any number of times. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( life[player] > 6 ){
			ai_modifier += 96;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int flag = can_pay_life(player, 0);
		int cd = count_deck(player);
		int *deck = deck_ptr[player];
		while( cd > 0 ){
			int cmc = get_cmc_by_id(cards_data[deck[0]].id);
			int card_added = add_card_to_hand(player, deck[0] );
			remove_card_from_deck( player, 0 );
			if( flag ){
				lose_life(player, cmc);
			}
			cd--;
			if( cd > 0 ){
				if( player == AI ){
					reveal_card(player, card, player, card_added);
					if( life[AI] < 6 && ! flag ){
						break;
					}
				}
				else{
					int choice = do_dialog(player, player, card, player, card_added, " Reveal another card\n Stop", 1);
					if( choice == 1 ){
						break;
					}
				}
			}
		}
		kill_card( player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_agony_warp(int player, int card, event_t event){
/* CARD_ID_AGONY_WARP	9909
Agony Warp	|U|B
Instant
Target creature gets -3/-0 until end of turn.
Target creature gets -0/-3 until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature for -3/-0", 1, NULL);
		if( spell_fizzled != 1 ){
			new_pick_target(&td, "Select target creature for -0/-3", 1, 1 | GS_LITERAL_PROMPT);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -3, 0);
		}
		if( validate_target(player, card, &td, 1) ){
			pump_until_eot(player, card, instance->targets[1].player, instance->targets[1].card, 0, -3);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_ajani_vengeant(int player, int card, event_t event)
{
	/* Ajani Vengeant	|2|R|W	0x2001885
	 * Legendary Planeswalker - Ajani (3)
	 * +1: Target permanent doesn't untap during its controller's next untap step.
	 * -2: ~ deals 3 damage to any target and you gain 3 life.
	 * -7: Destroy all lands target player controls. */
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  target_definition_t td_permanent;
	  default_target_definition(player, card, &td_permanent, TYPE_PERMANENT);

	  target_definition_t td_any;
	  default_target_definition(player, card, &td_any, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	  td_any.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	  target_definition_t td_player;
	  default_target_definition(player, card, &td_player, 0);
	  td_player.zone = TARGET_ZONE_PLAYERS;

	  int ai_priority_helix = 1;
	  if (player == AI || ai_is_speculating == 1)
		{
		  target_definition_t td_opponent;
		  default_target_definition(player, card, &td_opponent, 0);
		  td_opponent.allowed_controller = 1-player;
		  td_opponent.zone = TARGET_ZONE_PLAYERS;

		  target_definition_t td_weak_creature;
		  default_target_definition(player, card, &td_weak_creature, TYPE_CREATURE);
		  td_weak_creature.allowed_controller = 1-player;
		  td_weak_creature.toughness_requirement = 3 | TARGET_PT_LESSER_OR_EQUAL | TARGET_PT_INCLUDE_DAMAGE;

		  if (life[1-player] <= 6 && can_target(&td_opponent))
			ai_priority_helix = life[1-player] <= 3 ? 15 : 5;
		  else if (can_target(&td_weak_creature))
			ai_priority_helix = 5;
		}

	  enum {
		CHOICE_LOCKDOWN = 1,
		CHOICE_HELIX,
		CHOICE_LANDS
	  } choice = DIALOG(player, card, event,
						DLG_PLANESWALKER, DLG_RANDOM,
						"Lock down card", can_target(&td_permanent), 1, 1,
						"Lightning Helix", can_target(&td_any), ai_priority_helix, -2,
						"Nuke lands", can_target(&td_player), 10, -7);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;	// else fall through to planeswalker()
		}
	  else if (event == EVENT_ACTIVATE)
		{
		  instance->number_of_targets = 0;
		  switch (choice)
			{
			  case CHOICE_LOCKDOWN:
				if (player == AI || ai_is_speculating == 1)
				  {
					target_definition_t td_tapped_permanent;
					default_target_definition(player, card, &td_tapped_permanent, TYPE_PERMANENT);
					td_tapped_permanent.required_state = TARGET_STATE_TAPPED;

					if (can_target(&td_tapped_permanent))
					  pick_target(&td_tapped_permanent, "TARGET_PERMANENT");
					else
					  pick_target(&td_permanent, "TARGET_PERMANENT");
				  }
				else
				  pick_target(&td_permanent, "TARGET_PERMANENT");
				break;

			  case CHOICE_HELIX:
				pick_target(&td_any, "TARGET_ANY");
				break;

			  case CHOICE_LANDS:
				pick_target(&td_player, "TARGET_PLAYER");
				break;
			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		switch (choice)
		  {
			case CHOICE_LOCKDOWN:
			  if (valid_target(&td_permanent))
				does_not_untap_effect(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 0, 1);
			  break;

			case CHOICE_HELIX:
			  if (valid_target(&td_any))
				{
				  damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, card);
				  gain_life(player, 3);
				}
			  break;

			case CHOICE_LANDS:
			  if (valid_target(&td_player))
				{
				  test_definition_t this_test;
				  default_test_definition(&this_test, TYPE_LAND);
				  new_manipulate_all(player, instance->parent_card, instance->targets[0].player, &this_test, KILL_DESTROY);
				}
			  break;
		  }
	}

  return planeswalker(player, card, event, 3);
}

int card_akrasan_squire( int player, int card, event_t event){
/* CARD_ID_AKRASAN_SQUIRE	9911
Akrasan Squire	|W
Creature - Human Soldier 1/1
Exalted (Whenever a creature you control attacks alone, that creature gets +1/+1 until end of turn.) */

	exalted(player, card, event, 0, 0);
	return 0;
}

int card_angelic_benediction( int player, int card, event_t event){
/* CARD_ID_ANGELIC_BENEDICTION	9913
Angelic Benediction	|3|W
Enchantment
Exalted (Whenever a creature you control attacks alone, that creature gets +1/+1 until end of turn.)
Whenever a creature you control attacks alone, you may tap target creature. */

	int attacker = exalted(player, card, event, 0, 0);
	if (attacker != -1){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		card_instance_t* instance = get_card_instance(player, card);

		if (select_target(player, card, &td, "Select target creature to tap.", NULL)){
			tap_card( instance->targets[0].player, instance->targets[0].card );
		}

		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);;
}

int card_angelsong(int player, int card, event_t event){
/* CARD_ID_ANGELSONG	9914
Angelsong	|1|W
Instant
Prevent all combat damage that would be dealt this turn.
Cycling |2 (|2, Discard this card: Draw a card.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return cycling(player, card, event, MANACOST_X(2));
	}

	return card_fog(player, card, event);
}

int card_bant_battlemage(int player, int card, event_t event){
/* CARD_ID_BANT_BATTLEMAGE	9917
Bant Battlemage	|2|W
Creature - Human Wizard 2/2
|G, |T: Target creature gains trample until end of turn.
|U, |T: Target creature gains flying until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_TRAMPLE = 1,
		CHOICE_FLYING
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_DO_NOT_RECALCULATE_ACTIVATION_COST | GAA_CAN_TARGET,
			MANACOST0, 0, &td, NULL) )
		{
			if( has_mana_for_activated_ability(player, card, MANACOST_U(1)) ||
				has_mana_for_activated_ability(player, card, MANACOST_G(1)) )
			{
				return 1;
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		int priority_trample = current_phase == PHASE_AFTER_BLOCKING ? 10 : 1;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Give Trample", has_mana_for_activated_ability(player, card, MANACOST_G(1)), priority_trample,
							"Give Flying", has_mana_for_activated_ability(player, card, MANACOST_G(1)), 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_TRAMPLE ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_G(1), 0, &td, "TARGET_CREATURE");
		}
		if( choice == CHOICE_FLYING ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(1), 0, &td, "TARGET_CREATURE");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int key = instance->info_slot == CHOICE_FLYING ? KEYWORD_FLYING : KEYWORD_TRAMPLE;
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, key, 0);
		}
	}

	return 0;
}

int card_bant_charm(int player, int card, event_t event){
/* CARD_ID_BANT_CHARM	9918
Bant Charm	|G|W|U
Instant
Choose one -
* Destroy target artifact.
* Put target creature on the bottom of its owner's library.
* Counter target instant spell. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_ARTIFACT );

	target_definition_t td3;
	counterspell_target_definition(player, card, &td3, TYPE_INSTANT | TYPE_INTERRUPT );

	enum{
		CHOICE_KILL_ARTIFACT = 1,
		CHOICE_REMOVE_CREATURE,
		CHOICE_COUNTER
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_COUNTERSPELL, &td3, NULL, 1, NULL) ){
			return 99;
		}
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return 1;
		}
		return generic_spell(player, card, event, GS_CAN_TARGET, &td2, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {0, can_target(&td2), can_target(&td), counterspell(player, card, EVENT_CAN_CAST, &td3, 0)};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Destroy target artifact", abils[1], 5,
								"Put creature on bottom", abils[2], 10,
								"Counter an instant", abils[3], 20);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( instance->info_slot == CHOICE_KILL_ARTIFACT ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td2, "TARGET_ARTIFACT", 1, NULL);
		}
		if( instance->info_slot == CHOICE_REMOVE_CREATURE ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
		if( instance->info_slot == CHOICE_COUNTER ){
			return generic_spell(player, card, event, GS_COUNTERSPELL, &td3, NULL, 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_KILL_ARTIFACT && valid_target(&td2) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( instance->info_slot == CHOICE_REMOVE_CREATURE && valid_target(&td) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
			put_top_card_of_deck_to_bottom(instance->targets[0].player);
		}
		if( instance->info_slot == CHOICE_COUNTER ){
			return counterspell(player, card, event, &td3, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_bant_panorama(int player, int card, event_t event){
/* Bant Panorama	""
* Land
* |T: Add |C to your mana pool.
* |1, |T, Sacrifice ~: Search your library for a basic |H2Forest, |H2Plains, or |H2Island card and put it onto the battlefield tapped. Then shuffle your library. */

	return panorama(player, card, event, SUBTYPE_FOREST, SUBTYPE_ISLAND, SUBTYPE_PLAINS);
}

int card_battlegrace_angel( int player, int card, event_t event){
/* CARD_ID_BATTLEGRACE_ANGEL	9920
Battlegrace Angel	|3|W|W
Creature - Angel 4/4
Flying
Exalted (Whenever a creature you control attacks alone, that creature gets +1/+1 until end of turn.)
Whenever a creature you control attacks alone, it gains lifelink until end of turn. */

	exalted(player, card, event, 0, SP_KEYWORD_LIFELINK);

	return 0;
}

int card_blightning(int player, int card, event_t event)
{
	/* Blightning	|1|B|R	0x200be8e
	 * Sorcery
	 * ~ deals 3 damage to target player or planeswalker. That player or that planeswalker's controller discards two cards. */
  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  damage_target0(player, card, 3);
		  new_multidiscard(get_card_instance(player, card)->targets[0].player, 2, 0, player);
		}

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
}

int card_blister_beetle( int player, int card, event_t event){
/* CARD_ID_BLISTER_BEETLE	9922
Blister Beetle	|1|B
Creature - Insect 1/1
When ~ enters the battlefield, target creature gets -1/-1 until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE );
		td1.allow_cancel = 0;
		card_instance_t *instance = get_card_instance( player, card);
		if (can_target(&td1) && new_pick_target(&td1, "TARGET_CREATURE", 0, 0)){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -1, -1);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_blood_cultist(int player, int card, event_t event){
/* CARD_ID_BLOOD_CULTIST	9923
Blood Cultist	|1|B|R
Creature - Human Wizard 1/1
|T: ~ deals 1 damage to target creature.
Whenever a creature dealt damage by ~ this turn dies, put a +1/+1 counter on ~. */

	if( sengir_vampire_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		add_1_1_counters(player, card, get_card_instance( player, card)->targets[11].card);
		get_card_instance( player, card)->targets[11].card = 0;
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

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_bloodthorn_taunter(int player, int card, event_t event){
/* CARD_ID_BLOODTHORN_TAUNTER	9925
Bloodthorn Taunter	|1|R
Creature - Human Scout 1/1
Haste
|T: Target creature with power 5 or greater gains haste until end of turn. */

	haste(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = 5 | TARGET_PT_GREATER_OR_EQUAL;
	td.required_state = TARGET_STATE_SUMMONING_SICK;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target creature with POW 5 or higher.");
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

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0, &td, NULL);
}

int card_bone_splinters(int player, int card, event_t event){
/* CARD_ID_BONE_SPLINTERS	9926
Bone Splinters	|B
Sorcery
As an additional cost to cast ~, sacrifice a creature.
Destroy target creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_SAC_CREATURE_AS_COST, &td, "TARGET_CREATURE", 1, NULL);
}

int card_branching_bolt(int player, int card, event_t event){
/* CARD_ID_BRANCHING_BOLT	9927
Branching Bolt	|1|R|G
Instant
Choose one or both -
* ~ deals 3 damage to target creature with flying.
* ~ deals 3 damage to target creature without flying. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_abilities = KEYWORD_FLYING;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE );
	td2.illegal_abilities = KEYWORD_FLYING;

	target_definition_t td3;
	default_target_definition(player, card, &td3, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card);

	enum{
		CHOICE_DAMAGE_FLYER = 1,
		CHOICE_DAMAGE_NONFLYER,
		CHOICE_BOTH
	};

	if( event == EVENT_CAN_CAST  ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td3, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {0, can_target(&td), can_target(&td2), can_target(&td) && can_target(&td2) ? 1 : 0};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Damage creature with flying", abils[1], 5,
								"Damage creatuew without flying", abils[2], 10,
								"Do both", abils[3], 20);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		int base_target = 0;
		if( instance->info_slot & CHOICE_DAMAGE_FLYER ){
			new_pick_target(&td, "Select target creature with flying.", 0, 1 | GS_LITERAL_PROMPT);
		}
		if( spell_fizzled == 1 ){
			return 0;
		}
		if( instance->info_slot == CHOICE_BOTH ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			base_target++;
		}
		if( instance->info_slot & CHOICE_DAMAGE_NONFLYER ){
			new_pick_target(&td2, "Select target creature without flying.", base_target, 1 | GS_LITERAL_PROMPT);
		}
		if( instance->info_slot == CHOICE_BOTH ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
		if( spell_fizzled != 1 ){
			return basic_spell(player, card, event);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_BOTH ){
			if( validate_target(player, card, &td, 0) ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, card);
			}
			if( validate_target(player, card, &td2, 1) ){
				damage_creature(instance->targets[1].player, instance->targets[1].card, 3, player, card);
			}
		}
		else{
			if( instance->info_slot == CHOICE_DAMAGE_FLYER ){
				if( validate_target(player, card, &td, 0) ){
					damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, card);
				}
			}
			if( instance->info_slot == CHOICE_DAMAGE_NONFLYER ){
				if( validate_target(player, card, &td2, 0) ){
					damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, card);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_brilliant_ultimatum(int player, int card, event_t event){
/* CARD_ID_BRILLIANT_ULTIMATUM	9928
Brilliant Ultimatum	|W|W|U|U|U|B|B
Sorcery
Exile the top five cards of your library. An opponent separates those cards into two piles.
You may play any number of cards from one of those piles without paying their mana costs. */

	if( event == EVENT_CAN_CAST  ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 96;
	}

	if( event == EVENT_RESOLVE_SPELL  ){
		int number = count_deck(player);
		number = MIN(5, number);
		if( number > 0 ){
			int piles[2][number];

			separate_into_two_piles(1-player, deck_ptr[player], number, piles);

			int chosen = choose_between_two_piles(player, number, piles, AI_MAX_VALUE, "Play these");

			rfg_top_n_cards_of_deck(player, number);

			while (1){
				int playable[number];
				int i, num_playable = 0;
				for (i = 0; i < number; ++i){
					if (piles[chosen][i] != iid_draw_a_card && piles[chosen][i] != -1){
						/* Since we're playing these immediately as they're chosen instead of putting them on the stack, some of them
						 * might have been removed from exile already */
						if (!check_rfg(player, cards_data[piles[chosen][i]].id)){
							piles[chosen][i] = -1;
						} else if (can_legally_play_iid(player, piles[chosen][i])){
							playable[num_playable] = piles[chosen][i];
							++num_playable;
						}
					}
				}
				if (num_playable == 0){
					break;
				}

				int selected;
				if (player == AI){
					selected = 0;
				} else {
					selected = show_deck(player, playable, num_playable, "Play a card", 0, 0x7375B0);
				}
				if (selected == -1){
					break;
				}

				play_card_in_exile_for_free(player, player, cards_data[playable[selected]].id);

				for (i = 0; i < number; ++i){
					if (piles[chosen][i] == playable[selected]){
						piles[chosen][i] = -1;
						break;
					}
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_broodmate_dragon(int player, int card, event_t event){
	/* Broodmate Dragon	|3|B|R|G
	 * Creature - Dragon 4/4
	 * Flying
	 * When ~ enters the battlefield, put a 4/4 |Sred Dragon creature token with flying onto the battlefield. */

	// original code : 004DEB1D

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_DRAGON, &token);
		token.pow = token.tou = 4;
		generate_token(&token);
	}

	return 0;
}

int card_bull_cerodon(int player, int card, event_t event){
	/*
	  Bull Cerodon |4|W|R
	  Creature - Beast 5/5
	  Vigilance, haste
	*/
	vigilance(player, card, event);
	haste(player, card, event);
	return 0;
}

int card_caldera_hellion(int player, int card, event_t event){
/* CARD_ID_CALDERA_HELLION	9929  => card_wildfire_emissary 200bb0a
Caldera Hellion	|3|R|R
Creature - Hellion 3/3
Devour 1 (As this enters the battlefield, you may sacrifice any number of creatures. This creature enters the battlefield with that many +1/+1 counters on it.)
When ~ enters the battlefield, it deals 3 damage to each creature. */

	devour(player, card, event, 1);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		new_damage_all(player, card, ANYBODY, 3, NDA_ALL_CREATURES, NULL);
	}

	return 0;
}

int card_call_to_heel(int player, int card, event_t event){
/* CARD_ID_CALL_TO_HEEL	9930
Call to Heel	|1|U
Instant
Return target creature to its owner's hand. Its controller draws a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			draw_cards(instance->targets[0].player, 1);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "Select target creature you control.", 1, NULL);
}

/* CARD_ID_CAVERN_THOCTAR	9933  => card_wildfire_emissary 200bb0a
Cavern Thoctar	|5|G
Creature - Beast 5/5
|1|R: ~ gets +1/+0 until end of turn. */

static int check_clarion_ultimatum_targets(int iid, int card, int player, int unused){
	card_instance_t* instance = get_card_instance(player, card);
	int i;
	for(i=0; i<5; i++){
		if( instance->targets[i].card != -1 && cards_data[iid].id == instance->targets[i].card ){
			instance->targets[i].card = -1;
			return 1;
		}
	}
	return 0;
}

int card_clarion_ultimatum(int player, int card, event_t event){
	/*
	  Clarion Ultimatum |W|W|W|U|U|G|G
	  Sorcery
	  Choose five permanents you control.
	  For each of those permanents, you may search your library for a card with the same name as that permanent.
	  Put those cards onto the battlefield tapped, then shuffle your library.
	*/

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 96;
	}

	if(event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT );
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;

		card_instance_t *instance = get_card_instance( player, card);

		int selected = 0;

		while( selected < 5 && can_target(&td) ){
				if( select_target(player, card, &td, "Select a permanent you control", &(instance->targets[selected])) ){
					state_untargettable(instance->targets[selected].player, instance->targets[selected].card, 1);
					selected++;
				}
		}
		instance->number_of_targets = 0;
		int i;
		for(i=0; i<selected; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			int name = get_card_name(instance->targets[i].player, instance->targets[i].card);
			instance->targets[i].player = -1;
			instance->targets[i].card = name;
		}

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a permanent that was targetted by Clarion Ultimatum");
		this_test.special_selection_function = &check_clarion_ultimatum_targets;
		this_test.qty = selected;
		this_test.value_for_special_selection_function = card;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);

		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_corpse_conoisseur(int player, int card, event_t event){
/* CARD_ID_CORPSE_CONNOISSEUR	9937
Corpse Connoisseur	|4|B
Creature - Zombie Wizard 3/3
When ~ enters the battlefield, you may search your library for a creature card and put that card into your graveyard. If you do, shuffle your library.
Unearth |3|B (|3|B: Return this card from your graveyard to the battlefield. It gains haste.
Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_GRAVE, 0, AI_MAX_CMC, &this_test);
	}

	return unearth(player, event, MANACOST_XB(3, 1));
}

int card_couriers_capsule( int player, int card, event_t event){
/* CARD_ID_COURIERS_CAPSULE	9938
Courier's Capsule	|1|U
Artifact
|1|U, |T, Sacrifice ~: Draw two cards. */

	if( event == EVENT_ACTIVATE ){
		int total = get_total_available_lands(player);
		if( total <= 3 || total >= 6 ){
			ai_modifier += 48;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		draw_cards(player, 2);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_XU(1, 1), 0, NULL, NULL);
}

// court archers --> akrasan squire

int card_cradle_of_vitality(int player, int card, event_t event){
/* CARD_ID_CRADLE_OF_VITALITY	9941
Cradle of Vitality	|3|W
Enchantment
Whenever you gain life, you may pay |1|W. If you do, put a +1/+1 counter on target creature for each 1 life you gained. */

	return global_enchantment(player, card, event);
}

int card_crucible_of_fire(int player, int card, event_t event){
/* CARD_ID_CRUCIBLE_OF_FIRE	9942
Crucible of Fire	|3|R
Enchantment
Dragon creatures you control get +3/+3. */

	boost_creature_type(player, card, event, SUBTYPE_DRAGON, 3, 3, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
	return global_enchantment(player, card, event);
}

int card_cruel_ultimatum(int player, int card, event_t event){
/* CARD_ID_CRUEL_ULTIMATUM	1503
Cruel Ultimatum	|U|U|B|B|B|R|R
Sorcery
Target opponent sacrifices a creature, discards three cards, then loses 5 life. You return a creature card from your graveyard to your hand,
draw three cards, then gain 5 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 96;
		return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			new_sacrifice(player, card, instance->targets[0].player, SAC_CAUSED | SAC_NO_CANCEL, &test);
			new_multidiscard(instance->targets[0].player, 3, 0, player);
			lose_life(instance->targets[0].player, 5);

			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
			if( count_graveyard_by_type(player, TYPE_CREATURE) > 0 ){
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 1, AI_MAX_VALUE, &test);
			}
			draw_cards(player, 3);
			gain_life(player, 5);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_cunning_lethemancer(int player, int card, event_t event){
/* CARD_ID_CUNNING_LETHEMANCER	9943
Cunning Lethemancer	|2|B
Creature - Human Wizard 2/2
At the beginning of your upkeep, each player discards a card. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		APNAP(p, {discard(p, 0, player);};);
	}

	return 0;
}

int card_death_baron(int player, int card, event_t event)
{
	/* Death Baron	|1|B|B	0x2008135
	 * Creature - Zombie Wizard 2/2
	 * Skeletons you control and other Zombies you control get +1/+1 and have deathtouch. */
  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES)
	  && affected_card_controller == player
	  && (has_subtype(affected_card_controller, affected_card, SUBTYPE_SKELETON)
		  || (affected_card != card && has_subtype(affected_card_controller, affected_card, SUBTYPE_ZOMBIE)))
	  && !is_humiliated(player, card))
	{
	  if (event == EVENT_ABILITIES)
		deathtouch(affected_card_controller, affected_card, event);
	  else
		++event_result;
	}

	return 0;
}

static int deathgreeter_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_DEATHGREETER ){
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

int card_deathgreeter(int player, int card, event_t event){
/*
Deathgreeter |B	0x200f57a
Creature — Human Shaman 1/1
Whenever another creature dies, you may gain 1 life.
*/

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)deathgreeter_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &deathgreeter_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY && ! affect_me(player, card) ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			gain_life(player,  amount);
		}
	}

	return 0;
}

// deft duelist --> vanilla

int card_dispeller_capsule( int player, int card, event_t event){
/* CARD_ID_DISPELLERS_CAPSULE	9949
Dispeller's Capsule	|W
Artifact
|2|W, |T, Sacrifice ~: Destroy target artifact or enchantment. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT );

	card_instance_t *instance = get_card_instance( player, card);

	if(event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_XW(2, 1), 0,
									&td, "DISENCHANT");
}

int card_dragon_fodder(int player, int card, event_t event){
	/* Dragon Fodder	|1|R
	 * Sorcery
	 * Put two 1/1 |Sred Goblin creature tokens onto the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_GOBLIN, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_dregscape_zombie(int player, int card, event_t event){
/* CARD_ID_DREGSCAPE_ZOMBIE	9952
Dregscape Zombie	|1|B
Creature - Zombie 2/1
Unearth |B (|B: Return this card from your graveyard to the battlefield. It gains haste.
Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

	return unearth(player, event, MANACOST_B(1));
}

int card_drumhunter(int player, int card, event_t event){
/* CARD_ID_DRUMHUNTER	9954
Drumhunter	|3|G
Creature - Human Druid Warrior 2/2
At the beginning of your end step, if you control a creature with power 5 or greater, you may draw a card.
|T: Add |C to your mana pool. */

	if( eot_trigger_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player)) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.power_requirement = 5 | TARGET_PT_GREATER_OR_EQUAL;
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;

		if( can_target(&td) ){
			draw_some_cards_if_you_want(player, card, player, 1);
		}
	}

	return mana_producing_creature(player, card, event, 0, COLOR_COLORLESS, 1);
}

int card_elspeths_emblem(int player, int card, event_t event)
{
  /* Elspeth's Emblem	""
   * Emblem
   * Artifacts, creatures, enchantments, and lands you control are indestructible. */

  if (event == EVENT_ABILITIES && affected_card_controller == player
	  /* Can't pass all four types to is_what() at once, or it'll interpret it as "all permanents" including planeswalkers.  And can't just check
	   * !is_planeswalker(), because sometimes they're also creatures (e.g. Gideon Jura) or artifacts (e.g. Memnarch) or enchantments (Enchanted Evening). */
	  && (is_what(affected_card_controller, affected_card, TYPE_ARTIFACT | TYPE_CREATURE | TYPE_LAND)
		  || is_what(affected_card_controller, affected_card, TYPE_ENCHANTMENT)))
	indestructible(affected_card_controller, affected_card, event);

  return 0;
}

int card_elspeth_knight_errant(int player, int card, event_t event){

	/* Elspeth, Knight-Errant	|2|W|W
	 * Planeswalker - Elspeth (4)
	 * +1: Put a 1/1 |Swhite Soldier creature token onto the battlefield.
	 * +1: Target creature gets +3/+3 and gains flying until end of turn.
	 * -8: You get an emblem with "Artifacts, creatures, enchantments, and lands you control are indestructible." */

	if (IS_ACTIVATING(event)){

		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		int priority_emblem = 0;
		if( event == EVENT_ACTIVATE && ! check_battlefield_for_id(player, CARD_ID_ELSPETHS_EMBLEM) ){
			priority_emblem = (count_counters(player, card, COUNTER_LOYALTY)*3)-21;
			priority_emblem += count_subtype(player, TYPE_PERMANENT, -1)*2;
		}

		enum{
			CHOICE_SOLDIER = 1,
			CHOICE_PUMP,
			CHOICE_EMBLEM
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Generate a Soldier", 1, 10, 1,
						"Pump a creature", can_target(&td), 12, 1,
						"Emblem", 1, check_battlefield_for_id(player, CARD_ID_ELSPETHS_EMBLEM) ? -1 : 20, -8);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_ACTIVATE)
		{
		  instance->number_of_targets = 0;
		  switch (choice)
			{
				case CHOICE_PUMP:
					pick_target(&td, "TARGET_CREATURE");
					break;
				default:
					break;
			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		switch (choice)
		{
			case CHOICE_SOLDIER:
				generate_token_by_id(player, card, CARD_ID_SOLDIER);
				break;

			case CHOICE_PUMP:
				if( valid_target(&td) ){
					pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
											3, 3, KEYWORD_FLYING, 0);
				}
				break;

			case CHOICE_EMBLEM:
				generate_token_by_id(player, card, CARD_ID_ELSPETHS_EMBLEM);
				break;
		}
	}

	return planeswalker(player, card, event, 4);
}

int card_empyrial_archangel(int player, int card, event_t event ){
/*CARD_ID_EMPYRIAL_ARCHANGEL	9957
Empyrial Archangel	|4|G|W|W|U
Creature - Angel 5/8
Flying
Shroud (This creature can't be the target of spells or abilities.)
All damage that would be dealt to you is dealt to ~ instead. */

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *source = get_card_instance(affected_card_controller, affected_card);
		if( damage_card == source->internal_card_id && source->info_slot > 0 ){
			if( source->damage_target_player == player && source->damage_target_card == -1 ){
				source->damage_target_card = card;
			}
		}
	}

	return 0;
}

int card_esper_battlemage(int player, int card, event_t event){
/*
Esper Battlemage |2|U
Artifact Creature — Human Wizard 2/2
{W}, {T}: Prevent the next 2 damage that would be dealt to you this turn.
{B}, {T}: Target creature gets -1/-1 until end of turn.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_PREVENT_DAMAGE = 1,
		CHOICE_WEAKEN
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_W(1), 0,
									&td, NULL) )
		{
			return 99;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1), 0, &td2, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION,
													MANACOST_W(1), 0, &td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1), 0,
													&td2, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Prevent 2 damage to you", abils[1], 10,
							"Give -1/-1", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_PREVENT_DAMAGE ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_W(1), 0,
											&td, "TARGET_DAMAGE");
		}
		if( choice == CHOICE_WEAKEN ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1), 0, &td2, "TARGET_CREATURE");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PREVENT_DAMAGE && valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 2);
		}
		if( instance->info_slot == CHOICE_WEAKEN && valid_target(&td2) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							-1, -1);
		}
	}

	return 0;
}

int card_esper_charm(int player, int card, event_t event){
	/* Esper Charm	|W|U|B
	 * Instant
	 * Choose one -
	 * * Destroy target enchantment.
	 * * Draw two cards.
	 * * Target player discards two cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_KILL_ENCH = 1,
		CHOICE_DRAW,
		CHOICE_DISCARD
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return 1;
		}
		return generic_spell(player, card, event, GS_CAN_TARGET, &td2, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {0, can_target(&td), 1, can_target(&td2)};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Destroy target enchantment", abils[1], 15,
								"Draw 2 cards", abils[2], 5,
								"Target discars 2", abils[3], 10);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( instance->info_slot == CHOICE_KILL_ENCH ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
		}
		if( instance->info_slot == CHOICE_DRAW ){
			return basic_spell(player, card, event);
		}
		if( instance->info_slot == CHOICE_DISCARD ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td2, "TARGET_PLAYER", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_KILL_ENCH && valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( instance->info_slot == CHOICE_DRAW ){
			draw_cards(player, 2);
		}
		if( instance->info_slot == CHOICE_DISCARD && valid_target(&td2) ){
			new_multidiscard(instance->targets[0].player, 2, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_esper_panorama(int player, int card, event_t event){
/* Esper Panorama	""
* Land
* |T: Add |C to your mana pool.
* |1, |T, Sacrifice ~: Search your library for a basic |H2Plains, |H2Island, or |H2Swamp card and put it onto the battlefield tapped. Then shuffle your library. */

	return panorama(player, card, event, SUBTYPE_SWAMP, SUBTYPE_ISLAND, SUBTYPE_PLAINS);
}

int card_etherium_sculptor(int player, int card, event_t event){
/* CARD_ID_ETHERIUM_SCULPTOR	9962
Etherium Sculptor	|1|U
Artifact Creature - Vedalken Artificer 1/2
Artifact spells you cast cost |1 less to cast. */

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card) ){
		if( is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) ){
			COST_COLORLESS--;
		}
	}

	return 0;
}

int card_ethersworn_canonist(int player, int card, event_t event){
/* CARD_ID_ETHERSWORN_CANONIST	9963
Ethersworn Canonist	|1|W
Artifact Creature - Human Cleric 2/2
Each player who has cast a nonartifact spell this turn can't cast additional nonartifact spells. */

	if( event == EVENT_CAST_SPELL && ! is_what(affected_card_controller, affected_card, TYPE_LAND | TYPE_ARTIFACT) ){
		card_instance_t *instance = get_card_instance( player, card );
		if( instance->targets[1].player < 0 ){
			instance->targets[1].player = 0;
		}
		instance->targets[1].player |= (1+affected_card_controller);
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		if( ! is_what(affected_card_controller, affected_card, TYPE_LAND | TYPE_ARTIFACT) && instance->targets[1].player > -1 ){
			if( instance->targets[1].player & (1+affected_card_controller) ){
				infinite_casting_cost();
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return 0;
}

int card_executioners_capsule( int player, int card, event_t event){
/* CARD_ID_EXECUTIONERS_CAPSULE	9965
Executioner's Capsule	|B
Artifact
|1|B, |T, Sacrifice ~: Destroy target non|Sblack creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance( player, card);

	if(event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_XB(1, 1), 0,
									&td, get_sleighted_color_text(player, card, "Select target non%s creature", COLOR_BLACK));
}

int card_exuberant_firestoker(int player, int card, event_t event){
	/* Exuberant Firestoker	|2|R	0x2008162
	 * Creature - Human Druid Shaman 1/1
	 * At the beginning of your end step, if you control a creature with power 5 or greater, you may have ~ deal 2 damage to target player or planeswalker.
	 * |T: Add |C. */
	if( eot_trigger_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player)) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.power_requirement = 5 | TARGET_PT_GREATER_OR_EQUAL;
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;

		target_definition_t td1;
		default_target_definition(player, card, &td1, TARGET_TYPE_PLANESWALKER);
		td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		card_instance_t *instance = get_card_instance( player, card);

		if( can_target(&td) && can_target(&td1) && pick_target(&td1, "TARGET_PLAYER_OR_PLANESWALKER") ){
			damage_target0(player, card, 2);
		}
		instance->number_of_targets = 0;
	}

	return mana_producing_creature(player, card, event, 0, COLOR_COLORLESS, 1);
}

int card_fatestitcher(int player, int card, event_t event){
/* CARD_ID_FATESTITCHER	9967
Fatestitcher	|3|U
Creature - Zombie Wizard 1/2
|T: You may tap or untap another target permanent.
Unearth |U (|U: Return this card from your graveyard to the battlefield. It gains haste.
Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

	if( ! IS_GAA_EVENT(event) ){
		return unearth(player, event, MANACOST_U(1));
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.preferred_controller = ANYBODY;

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PERMANENT");
		if( spell_fizzled != 1 ){
			ai_modifier_twiddle(player, card, 0);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			twiddle(player, card, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PERMANENT");
}

int card_filigree_sages(int player, int card, event_t event){
/* CARD_ID_FILIGREE_SAGES	9969
Filigree Sages	|3|U
Artifact Creature - Vedalken Wizard 2/3
|2|U: Untap target artifact. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XU(2, 1), 0, &td, "TARGET_ARTIFACT");
		if( spell_fizzled != 1 ){
			ai_mod_for_untap_effects(player, card, 0, 3);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XU(2, 1), 0, &td, "TARGET_ARTIFACT");
}

int card_fire_field_ogre(int player, int card, event_t event){
/* CARD_ID_FIRE_FIELD_OGRE	9970
Fire-Field Ogre	|1|U|B|R
Creature - Ogre Mutant 4/2
First strike
Unearth |U|B|R (|U|B|R: Return this card from your graveyard to the battlefield. It gains haste.
Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

	return unearth(player, event, MANACOST_XBR(1, 1, 1));
}

int card_flameblast_dragon(int player, int card, event_t event)
{
	/* Flameblast Dragon	|4|R|R	0x200408f
	 * Creature - Dragon 5/5
	 * Flying
	 * Whenever ~ attacks, you may pay |X|R. If you do, ~ deals X damage to any target. */
  if ((xtrigger_condition() == XTRIGGER_ATTACKING || event == EVENT_DECLARE_ATTACKERS)
	  && has_mana(player, COLOR_RED, 1))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	  // AI only activates if it can target opponent or opponent's creature, and X can be at least 1.
	  int trig_mode;
	  if (player == HUMAN && ai_is_speculating != 1)
		trig_mode = RESOLVE_TRIGGER_OPTIONAL;
	  else
		{
		  td.allowed_controller = 1-player;
		  if (has_mana_multi(player, MANACOST_XR(1,1)) && can_target(&td))
			trig_mode = RESOLVE_TRIGGER_MANDATORY;
		  td.allowed_controller = ANYBODY;
		}

	  card_instance_t* instance = get_card_instance(player, card);
	  instance->number_of_targets = 0;
	  if (can_target(&td)
		  && declare_attackers_trigger(player, card, event, trig_mode, player, card)
		  && charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, MANACOST_XR(-1,1))
		  && pick_target(&td, "TARGET_ANY"))
		damage_creature(instance->targets[0].player, instance->targets[0].card, x_value, player, card);
	}

	return 0;
}


int card_fleshbag_marauder(int player, int card, event_t event){
/* CARD_ID_FLESHBAG_MARAUDER	9972
Fleshbag Marauder	|2|B
Creature - Zombie Warrior 3/1
When ~ enters the battlefield, each player sacrifices a creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		APNAP(p, { new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test); };);
	}

	return 0;
}

static int gather_specimens_legacy(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance( player, card );

	if( specific_spell_played(player, card, event, 1-player, 2, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0) ){
		if( ! is_token(instance->targets[1].player, instance->targets[1].card) ){
			int iid = get_original_internal_card_id(instance->targets[1].player, instance->targets[1].card);
			obliterate_card(instance->targets[1].player, instance->targets[1].card);
			int card_added = add_card_to_hand(player, iid);
			if( 1-player == AI ){
				add_state(player, card_added, STATE_OWNED_BY_OPPONENT);
			}
			else{
				remove_state(player, card_added, STATE_OWNED_BY_OPPONENT);
			}
			put_into_play(player, card_added);
		}
	}

	if( specific_cip(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0) ){
		gain_control(player, card, instance->targets[1].player, instance->targets[1].card);
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}


int card_gather_specimens(int player, int card, event_t event){
/* CARD_ID_GATHER_SPECIMENS	9973
Gather Specimens	|3|U|U|U
Instant
If a creature would enter the battlefield under an opponent's control this turn, it enters the battlefield under your control instead. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( counterspell(player, card, event, NULL, 0) ){
			return 99;
		}
		return 1;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( card_on_stack_controller > -1 && card_on_stack > -1 ){
			instance->targets[0].player = card_on_stack_controller;
			instance->targets[0].card = card_on_stack;
			instance->number_of_targets = 1;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( instance->number_of_targets == 1 && is_what(instance->targets[0].player, instance->targets[0].card, TYPE_CREATURE) ){
			int iid = get_original_internal_card_id(instance->targets[0].player, instance->targets[0].card);
			obliterate_card(instance->targets[0].player, instance->targets[0].card);
			int card_added = add_card_to_hand(player, iid);
			if( 1-player == AI ){
				add_state(player, card_added, STATE_OWNED_BY_OPPONENT);
			}
			else{
				remove_state(player, card_added, STATE_OWNED_BY_OPPONENT);
			}
			real_put_into_play(player, card_added);
		}
		create_legacy_effect(player, card, &gather_specimens_legacy);
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_gift_of_the_gargantuan(int player, int card, event_t event)
{
  /* Gift of the Gargantuan	|2|G
   * Sorcery
   * Look at the top four cards of your library. You may reveal a creature card and/or a land card from among them and put the revealed cards into your hand.
   Put the rest on the bottom of your library in any order. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 48;
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
	  if (deck_ptr[player][0] != -1)
		{
		  int sz = MIN(4, count_deck(player));

		  test_definition_t test;
		  new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
		  test.no_shuffle = 1;
		  test.create_minideck = sz;

		  if (new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test) != -1)
			--sz;

		  if (deck_ptr[player][0] != -1)
			{
			  new_default_test_definition(&test, TYPE_LAND, "Select a land card.");
			  test.no_shuffle = 1;
			  test.create_minideck = sz;

			  if (new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test) != -1)
				--sz;
			}

		  if (sz > 0)
			put_top_x_on_bottom(player, player, sz);
		}

	  kill_card(player, card, KILL_DESTROY);
	}

  return basic_spell(player, card, event);
}

int card_glaze_fiend(int player, int card, event_t event){
/* CARD_ID_GLAZE_FIEND	9975
Glaze Fiend	|1|B
Artifact Creature - Illusion 0/1
Flying
Whenever another artifact enters the battlefield under your control, ~ gets +2/+2 until end of turn. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT);
		this_test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		pump_until_eot_merge_previous(player, card, player, card, 2, 2);
	}

	return 0;
}

int card_goblin_assault(int player, int card, event_t event){
/* CARD_ID_GOBLIN_ASSAULT	9976
Goblin Assault	|2|R
Enchantment
At the beginning of your upkeep, create a 1/1 |Sred Goblin creature token with haste.
Goblin creatures attack each turn if able. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_GOBLIN, &token);
		token.special_infos = 67;
		generate_token(&token);
	}

	all_must_attack_if_able(player, event, SUBTYPE_GOBLIN);

	return global_enchantment(player, card, event);
}

// goblin deathriders --> vanilla

int card_godsire(int player, int card, event_t event){
	/* Godsire	|4|R|G|G|W
	 * Creature - Beast 8/8
	 * Vigilance
	 * |T: Put an 8/8 Beast creature token that's |Sred, |Sgreen, and |Swhite onto the battlefield. */

	// original code : 004E33F7

	vigilance(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_BEAST, &token);
		token.pow = token.tou = 8;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE | COLOR_TEST_GREEN | COLOR_TEST_RED);
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_grixis_battlemage(int player, int card, event_t event){
/* CARD_ID_GRIXIS_BATTLEMAGE	9980
Grixis Battlemage	|2|B
Creature - Human Wizard 2/2
|U, |T: Draw a card, then discard a card.
|R, |T: Target creature can't block this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_DRAW = 1,
		CHOICE_CANNOT_BLOCK
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td, NULL) )
		{
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_U(1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0,
												&td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_U(1), 0, NULL, NULL)
		};
		int priority_cannot_block = current_turn == player && current_phase < PHASE_DECLARE_BLOCKERS ? 10 : 1;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Draw & discard", abils[1], 5,
							"Target creature cannot block", abils[2], priority_cannot_block);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_DRAW ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_U(1), 0, NULL, NULL);
		}
		if( choice == CHOICE_CANNOT_BLOCK ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_CREATURE");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DRAW ){
			draw_a_card(player);
			discard(player, 0, player);
		}
		if( instance->info_slot == CHOICE_CANNOT_BLOCK && valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
		}
	}

	return 0;
}

int card_grixis_charm(int player, int card, event_t event){
	/* Grixis Charm	|U|B|R
	 * Instant
	 * Choose one -
	 * * Return target permanent to its owner's hand.
	 * * Target creature gets -4/-4 until end of turn.
	 * * Creatures you control get +2/+0 until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_BOUNCE = 1,
		CHOICE_M4_M4,
		CHOICE_P2_P0
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return 1;
		}
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td2, NULL, 1, NULL) ){
			return 1;
		}
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {0, can_target(&td), can_target(&td2), 2};
			int priorities[4] = {
								5,
								current_turn != player ? 10 : 1,
								current_turn == player && current_phase == PHASE_AFTER_BLOCKING ? 10 : 1
			};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Bounce a permanent", 	abils[1], priorities[1],
								"Give -4/-4", 			abils[2], priorities[2],
								"+2/+0 for your crits", abils[3], priorities[3]);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( instance->info_slot == CHOICE_BOUNCE ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
		}
		if( instance->info_slot == CHOICE_M4_M4 ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td2, "TARGET_CREATURE", 1, NULL);
		}
		if( instance->info_slot == CHOICE_P2_P0 ){
			return basic_spell(player, card, event);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_BOUNCE && valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		if( instance->info_slot == CHOICE_M4_M4 && valid_target(&td2) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -4, -4);
		}
		if( instance->info_slot == CHOICE_P2_P0  ){
			pump_subtype_until_eot(player, card, player, -1, 2, 0, 0, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_grixis_panorama(int player, int card, event_t event){
/* Grixis Panorama	""
* Land
* |T: Add |C to your mana pool.
* |1, |T, Sacrifice ~: Search your library for a basic |H2Island, |H2Swamp, or |H2Mountain card and put it onto the battlefield tapped. Then shuffle your library. */

	return panorama(player, card, event, SUBTYPE_SWAMP, SUBTYPE_ISLAND, SUBTYPE_MOUNTAIN);
}

int card_hell_thunder(int player, int card, event_t event){
/* CARD_ID_HELLS_THUNDER	9985
Hell's Thunder	|1|R|R
Creature - Elemental 4/4
Flying, haste
At the beginning of the end step, sacrifice ~.
Unearth |4|R (|4|R: Return this card from your graveyard to the battlefield. It gains haste.
Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

	card_ball_lightning(player, card, event);
	return unearth(player, event, MANACOST_XR(4, 1));
}

int card_hellkite_overlord(int player, int card, event_t event){
/* CARD_ID_HELLKITE_OVERLORD	1538
Hellkite Overlord	|4|B|R|R|G
Creature - Dragon 8/8
Flying, trample, haste
|R: ~ gets +1/+0 until end of turn.
|B|G: Regenerate ~. */

	haste(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_PUMP = 1,
		CHOICE_REGENERATE
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( regeneration(player, card, event, MANACOST_GB(1, 1)) ){
			return 99;
		}
		return generic_shade(player, card, event, 0, MANACOST_R(1), 1, 0, 0, 0);
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = CHOICE_PUMP;
		if( regeneration(player, card, EVENT_CAN_ACTIVATE, MANACOST_GB(1, 1)) ){
			instance->info_slot = CHOICE_REGENERATE;
			return regeneration(player, card, event, MANACOST_GB(1, 1));
		}
		return generic_shade(player, card, event, 0, MANACOST_R(1), 1, 0, 0, 0);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PUMP ){
			return generic_shade(player, card, event, 0, MANACOST_R(1), 1, 0, 0, 0);
		}
		if( instance->info_slot == CHOICE_REGENERATE ){
			return regeneration(player, card, event, MANACOST_GB(1, 1));
		}
	}
	return 0;
}

int card_hindering_light(int player, int card, event_t event){
/* CARD_ID_HINDERING_LIGHT	9986
Hindering Light	|W|U
Instant
Counter target spell that targets you or a permanent you control.
Draw a card. */

	if( event == EVENT_CAN_CAST ){
		int result = generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
		if( result ){
			card_instance_t *spell = get_card_instance(card_on_stack_controller, card_on_stack);
			int i;
			for(i=0; i<spell->number_of_targets; i++){
				if( spell->targets[i].player == player ){
					return 0x63;
				}
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int hissing_iguanar_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_HISSING_IGUANAR ){
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
				count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);

			target_definition_t td;
			default_target_definition(player, card, &td, 0);
			td.zone = TARGET_ZONE_PLAYERS;
			td.allow_cancel = 0;

			card_instance_t *instance = get_card_instance(player, card);

			if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 1, 0) ){
				damage_player(instance->targets[1].player, amount, player, card);
			}
			instance->number_of_targets = 1;
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

int card_hissing_iguanar(int player, int card, event_t event){
	/* Hissing Iguanar	|2|R	0x2008199
	 * Creature - Lizard 3/1
	 * Whenever another creature dies, you may have ~ deal 1 damage to target player or planeswalker. */
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)hissing_iguanar_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &hissing_iguanar_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY && ! affect_me(player, card) ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);

			target_definition_t td;
			default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
			td.allow_cancel = 0;

			card_instance_t *instance = get_card_instance(player, card);

			if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER", 0, 0) ){
				damage_target0(player, card, amount);
			}
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_immortal_coil(int player, int card, event_t event){

	/* Immortal Coil	|2|B|B
	 * Artifact
	 * |T, Exile two cards from your graveyard: Draw a card.
	 * If damage would be dealt to you, prevent that damage. Exile a card from your graveyard for each 1 damage prevented this way.
	 * When there are no cards in your graveyard, you lose the game. */

	if( event == EVENT_STATIC_EFFECTS && in_play(player, card) && ! is_humiliated(player, card) ){
		if( get_grave(player)[0] ){
			lose_the_game(player);
		}
	}

	if (event == EVENT_SHOULD_AI_PLAY){
		ai_modifier += (player == AI ? 12 : -12) * count_graveyard(player);
	}

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *source = get_card_instance(affected_card_controller, affected_card);
		if( damage_card == source->internal_card_id && source->info_slot > 0 ){
			if( source->damage_target_player == player && source->damage_target_card == -1 ){
				int count = count_graveyard(player);
				while( count > 0 && source->info_slot > 0 ){
						char msg[100] = "Select a card to exile.";
						test_definition_t this_test;
						new_default_test_definition(&this_test, TYPE_ANY, msg);
						int result = new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 1, AI_MIN_VALUE, &this_test);
						if( result != -1 ){
							count--;
							prevent_damage(affected_card_controller, affected_card, 1);
						}
				}
			}
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( count_graveyard(player) >= 2 ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
	}

	if(event == EVENT_ACTIVATE ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");
		int result = new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test);
		if( result != -1 ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 1, AI_MIN_VALUE, &this_test);
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_invincible_hymn(int player, int card, event_t event){
/* CARD_ID_INVINCIBLE_HYMN	9990
Invincible Hymn	|6|W|W
Sorcery
Count the number of cards in your library. Your life total becomes that number. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int diff = count_deck(player) - life[player];
		ai_modifier += (diff * 2);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = count_deck(player);
		set_life_total(player, amount);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_jund_battlemage(int player, int card, event_t event){
	/* Jund Battlemage	|2|R
	 * Creature - Human Shaman 2/2
	 * |B, |T: Target player loses 1 life.
	 * |G, |T: Put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_LOSE_LIFE = 1,
		CHOICE_SAPROLING
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1), 0, &td, NULL) )
		{
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_G(1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1), 0,
												&td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_G(1), 0, NULL, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Target player loses 1 life", abils[1], life[1-player] < 6 ? 10 : 1,
							"Create Saproling", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_LOSE_LIFE ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(1), 0, &td, "TARGET_PLAYER");
		}
		if( choice == CHOICE_SAPROLING ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_G(1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_LOSE_LIFE && valid_target(&td) ){
			lose_life(instance->targets[0].player, 1);
		}
		if( instance->info_slot == CHOICE_SAPROLING ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
		}
	}

	return 0;
}

int card_jund_charm(int player, int card, event_t event){
	/* Jund Charm	|B|R|G
	 * Instant
	 * Choose one -
	 * * Exile all cards from target player's graveyard.
	 * * ~ deals 2 damage to each creature.
	 * * Put two +1/+1 counters on target creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_EXILE_GRAVE = 1,
		CHOICE_PYROCLASM,
		CHOICE_P1_P1_COUNTERS
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return 1;
		}
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td2, NULL, 1, NULL) ){
			return 1;
		}
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {0, can_target(&td), 1, can_target(&td2)};
			int priorities[4] = {
								would_validate_arbitrary_target(&td, 1-player, -1) ? 1 : 0,
								5,
								10,
			};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Exile a graveyard", 	abils[1], priorities[1],
								"Pyroclasm", 			abils[2], priorities[2],
								"2 +1/+1 counters", 	abils[3], priorities[3]);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( instance->info_slot == CHOICE_EXILE_GRAVE ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		}
		if( instance->info_slot == CHOICE_PYROCLASM ){
			return basic_spell(player, card, event);
		}
		if( instance->info_slot == CHOICE_P1_P1_COUNTERS ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td2, "TARGET_CREATURE", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_EXILE_GRAVE && valid_target(&td) ){
			rfg_whole_graveyard(instance->targets[0].player);
		}
		if( instance->info_slot == CHOICE_PYROCLASM ){
			new_damage_all(player, card, ANYBODY, 2, NDA_ALL_CREATURES, NULL);
		}
		if( instance->info_slot == CHOICE_P1_P1_COUNTERS && valid_target(&td2) ){
			add_1_1_counters(instance->targets[0].player, instance->targets[0].card, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_jund_panorama(int player, int card, event_t event){
	/* Jund Panorama	""
	 * Land
	 * |T: Add |C to your mana pool.
	 * |1, |T, Sacrifice ~: Search your library for a basic |H2Swamp, |H2Mountain, or |H2Forest card and put it onto the battlefield tapped. Then shuffle your library. */

	return panorama(player, card, event, SUBTYPE_SWAMP, SUBTYPE_FOREST, SUBTYPE_MOUNTAIN);
}

int card_kederekt_creeper(int player, int card, event_t event){
	/* Kederekt Creeper	|U|B|R
	 * Creature - Horror 2/3
	 * Menace
	 * Deathtouch */

	menace(player, card, event);
	deathtouch(player, card, event);
	return 0;
}

int card_kederekt_leviathan(int player, int card, event_t event){
/* CARD_ID_KEDEREKT_LEVIATHAN	9998
Kederekt Leviathan	|6|U|U
Creature - Leviathan 5/5
When ~ enters the battlefield, return all other nonland permanents to their owners' hands.
Unearth |6|U (|6|U: Return this card from your graveyard to the battlefield. It gains haste.
Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.type_flag = DOESNT_MATCH;
		this_test.not_me = 1;
		APNAP(p, { new_manipulate_all(player, card, p, &this_test, ACT_BOUNCE); };);
	}

	return unearth(player, event, 6, 0, 1, 0, 0, 0);
}

int card_keeper_of_progenitus(int player, int card, event_t event){
/* CARD_ID_KEEPER_OF_PROGENITUS	9999
Keeper of Progenitus	|3|G
Creature - Elf Druid 1/3
Whenever a player taps |Ha Mountain, |H2Forest, or |H2Plains for mana, that player adds one mana to his or her mana pool of any type that land produced. */

	if ((event == EVENT_COUNT_MANA || event == EVENT_TAP_CARD) && is_what(affected_card_controller, affected_card, TYPE_LAND)
		&& (has_subtype(affected_card_controller, affected_card, SUBTYPE_FOREST)
			|| has_subtype(affected_card_controller, affected_card, SUBTYPE_MOUNTAIN)
			|| has_subtype(affected_card_controller, affected_card, SUBTYPE_PLAINS))){
		// See comments in card_mana_flare().

		if (!in_play(player, card) || is_humiliated(player, card) ){
			return 0;
		}

		if (event == EVENT_COUNT_MANA){
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
		} else {	// event == EVENT_TAP_CARD
			produce_mana_of_any_type_tapped_for(player, card, 1);
		}
	}

	return 0;
}

int card_kiss_of_the_amesha(int player, int card, event_t event){
/* CARD_ID_KISS_OF_THE_AMESHA	1324
Kiss of the Amesha	|4|W|U
Sorcery
Target player gains 7 life and draws two cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( life[player] < 6 ){
			ai_modifier += 24;
		}
		if( hand_count[player] < 3 ){
			ai_modifier += 24;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			gain_life(instance->targets[0].player, 7);
			draw_cards(instance->targets[0].player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_knight_of_the_white_orchid(int player, int card, event_t event)
{
  /* Knight of the White Orchid	|W|W
   * Creature - Human Knight 2/2
   * First strike
   * When ~ enters the battlefield, if an opponent controls more lands than you, you may search your library for |Ha Plains card, put it onto the battlefield, then shuffle your library. */

	if( landsofcolor_controlled[1-player][COLOR_ANY] > landsofcolor_controlled[player][COLOR_ANY] ){
		etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
	}

	if( event == EVENT_ETB_ABILITY ){
		if( landsofcolor_controlled[1-player][COLOR_ANY] > landsofcolor_controlled[player][COLOR_ANY] ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, get_hacked_land_text(player, card, "Select %a card.", SUBTYPE_PLAINS));
			test.subtype = get_hacked_subtype(player, card, SUBTYPE_PLAINS);
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
		}
	}

	return 0;
}

int card_knight_captain_of_eos(int player, int card, event_t event){
	/* Knight-Captain of Eos	|4|W
	 * Creature - Human Knight 2/2
	 * When ~ enters the battlefield, put two 1/1 |Swhite Soldier creature tokens onto the battlefield.
	 * |W, Sacrifice a Soldier: Prevent all combat damage that would be dealt this turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_tokens_by_id(player, card, CARD_ID_SOLDIER, 2);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Soldier to sacrifice.");
	test.subtype = SUBTYPE_SOLDIER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		fog_effect(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_W(1), 0, NULL, NULL, 0, &test, 0);
}

int card_kresh_the_bloodbraided(int player, int card, event_t event){
/* CARD_ID_KRESH_THE_BLOODBRAIDED	10003
Kresh the Bloodbraided	|2|B|R|G
Legendary Creature - Human Warrior 3/3
Whenever another creature dies, you may put X +1/+1 counters on ~, where X is that creature's power. */

	check_legend_rule(player, card, event);

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, 0, &this_test, GFPC_STORE_POWER, 1);
	}

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		card_instance_t *instance = get_card_instance(player, card);
		add_1_1_counters(player, card, instance->targets[1].player);
		instance->targets[1].player = 0;
	}
	return 0;
}

int card_magma_spray(int player, int card, event_t event){
/* CARD_ID_MAGMA_SPRAY	10007
Magma Spray	|R
Instant
~ deals 2 damage to target creature. If that creature would die this turn, exile it instead. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);
			exile_if_would_be_put_into_graveyard(player, card, instance->targets[0].player, instance->targets[0].card, 1);
			damage_target0(player, card, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}


int card_manaplasm(int player, int card, event_t event){
/* CARD_ID_MANAPLASM	10008
Manaplasm	|2|G
Creature - Ooze 1/1
Whenever you cast a spell, ~ gets +X/+X until end of turn, where X is that spell's converted mana cost. */

	if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		int amount = get_cmc(trigger_cause_controller, trigger_cause);
		pump_until_eot(player, card, player, card, amount, amount);
	}

	return 0;
}

int card_marble_chalice(int player, int card, event_t event){
/* CARD_ID_MARBLE_CHALICE	10009
Marble Chalice	|2|W
Artifact
|T: You gain 1 life. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_master_of_etherium(int player, int card, event_t event)
{
  /* Master of Etherium	|2|U
   * Artifact Creature - Vedalken Wizard 100/100
   * ~'s power and toughness are each equal to the number of artifacts you control.
   * Other artifact creatures you control get +1/+1. */

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == player && !is_humiliated(player, card) &&
		player != -1)
	{
		if (affected_card == card)
			event_result += count_permanents_by_type(player, TYPE_ARTIFACT);
		else if (is_what(affected_card_controller, affected_card, TYPE_ARTIFACT))
			event_result += 1;
	}

	return 0;
}

int card_mayael_the_anima(int player, int card, event_t event){
/* CARD_ID_MAYAEL_THE_ANIMA	10011
Mayael the Anima	|R|G|W
Legendary Creature - Elf Shaman 2/3
|3|R|G|W, |T: Look at the top five cards of your library. You may put a creature card with power 5 or greater from among them onto the battlefield.
Put the rest on the bottom of your library in any order. */

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int amount = MIN(count_deck(player), 5);
		if( amount ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature with power 5 or higher.");
			this_test.power = 4;
			this_test.power_flag = F5_POWER_GREATER_THAN_VALUE;
			this_test.create_minideck = amount;
			this_test.no_shuffle = 1;
			if( new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test) != -1 ){
				amount--;
			}
			if( amount > 0 ){
				put_top_x_on_bottom(player, player, amount);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, 3, 0, 0, 1, 1, 1, 0, NULL, NULL);
}

int card_memory_erosion(int player, int card, event_t event){
/* CARD_ID_MEMORY_EROSION	10012
Memory Erosion	|1|U|U
Enchantment
Whenever an opponent casts a spell, that player puts the top two cards of his or her library into his or her graveyard. */

	if( new_specific_spell_played(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		mill(1-player, 2);
	}

	return global_enchantment(player, card, event);
}

int card_mighty_emergence(int player, int card, event_t event){
/* CARD_ID_MIGHTY_EMERGENCE	10014
Mighty Emergence	|2|G
Enchantment
Whenever a creature with power 5 or greater enters the battlefield under your control, you may put two +1/+1 counters on it. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.power = 4;
		this_test.power_flag = F5_POWER_GREATER_THAN_VALUE;

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		add_1_1_counters(trigger_cause_controller, trigger_cause, 2);
	}

	return global_enchantment(player, card, event);
}

int card_mindlock_orb(int player, int card, event_t event){
/* CARD_ID_MINDLOCK_ORB	10015
Mindlock Orb	|3|U
Artifact
Players can't search libraries. */

	// the true code is in "tutors.c"
	return 0;
}

int card_minion_reflector(int player, int card, event_t event){
/* CARD_ID_MINION_REFLECTOR	10016
Minion Reflector	|5
Artifact
Whenever a nontoken creature enters the battlefield under your control, you may pay |2. If you do, create a token that's a copy of that creature.
That token has haste and "At the beginning of the end step, sacrifice this permanent." */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		if( has_mana(player, COLOR_ANY, 2) && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 2) ){
			token_generation_t token;
			copy_token_definition(player, card, &token, trigger_cause_controller, trigger_cause);
			token.legacy = 1;
			token.special_code_for_legacy = &haste_and_sacrifice_eot;
			generate_token(&token);
		}
	}

	return 0;
}

int card_mosstodon(int player, int card, event_t event){
/* CARD_ID_MOSSTODON	10017
Mosstodon	|4|G
Creature - Plant Elephant 5/3
|1: Target creature with power 5 or greater gains trample until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = 5 | TARGET_PT_GREATER_OR_EQUAL;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_TRAMPLE, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(1), 0,
									&td, "Select target creature with POW 5 or higher.");
}

int card_mycoloth(int player, int card, event_t event){

	/* Mycoloth	|3|G|G
	 * Creature - Fungus 4/4
	 * Devour 2
	 * At the beginning of your upkeep, put a 1/1 |Sgreen Saproling creature token onto the battlefield for each +1/+1 counter on ~. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, count_1_1_counters(player, card) );
	}

	devour(player, card, event, 2);

	return 0;
}

int card_naya_battlemage(int player, int card, event_t event){
/* CARD_ID_NAYA_BATTLEMAGE	10019
Naya Battlemage	|2|G
Creature - Human Shaman 2/2
|R, |T: Target creature gets +2/+0 until end of turn.
|W, |T: Tap target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_P2_P0 = 1,
		CHOICE_TAP
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td, NULL) )
		{
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(1), 0, &td2, NULL) )
		{
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0,
												&td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(1), 0,
												&td2, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Give +2/+0", abils[1], current_phase == PHASE_AFTER_BLOCKING ? 10 : 1,
							"Tap creature", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_P2_P0 ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_CREATURE");
		}
		if( choice == CHOICE_TAP ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(1), 0, &td2, "TARGET_CREATURE");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_P2_P0 && valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 2 , 0);
		}
		if( instance->info_slot == CHOICE_TAP && valid_target(&td2) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_naya_charm(int player, int card, event_t event){
/* Naya Charm	|R|G|W
 * Instant
 * Choose one -
 * ~ deals 3 damage to target creature;
 * or return target card from a graveyard to its owner's hand;
 * or tap all creatures target player controls. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	test_definition_t test2;
	new_default_test_definition(&test2, TYPE_ANY, "Select target card.");

	target_definition_t td3;
	default_target_definition(player, card, &td3, 0);
	td3.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_DAMAGE_CRIT = 1,
		CHOICE_REGROWTH,
		CHOICE_TAP_CRITS
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return 1;
		}
		if( generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &test2) ){
			return 1;
		}
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td3, NULL, 1, NULL) ){
			return 1;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {0,
							can_target(&td),
							generic_spell(player, card, EVENT_CAN_CAST, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &test2),
							can_target(&td3)
			};
			int priorities[4] = {
								5,
								count_graveyard(player) * 2,
								current_turn != player ? 10 : 1,
			};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Damage target creature", 	abils[1], priorities[1],
								"Regrowth", 				abils[2], priorities[2],
								"Tap creatures", 			abils[3], priorities[3]);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( instance->info_slot == CHOICE_DAMAGE_CRIT ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
		if( instance->info_slot == CHOICE_REGROWTH ){
			return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &test2);
		}
		if( instance->info_slot == CHOICE_TAP_CRITS ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td3, "TARGET_PLAYER", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_DAMAGE_CRIT && valid_target(&td) ){
			damage_target0(player, card, 3);
		}
		if( instance->info_slot == CHOICE_REGROWTH ){
			int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
			if( selected > -1 ){
				from_grave_to_hand(instance->targets[0].player, selected, TUTOR_HAND);
			}
		}
		if( instance->info_slot == CHOICE_TAP_CRITS && valid_target(&td3) ){
			default_test_definition(&test2, TYPE_CREATURE);
			new_manipulate_all(player, card, instance->targets[0].player, &test2, ACT_TAP);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_naya_panorama(int player, int card, event_t event){
	/* Naya Panorama	""
	 * Land
	 * |T: Add |C to your mana pool.
	 * |1, |T, Sacrifice ~: Search your library for a basic |H2Mountain, |H2Forest, or |H2Plains card and put it onto the battlefield tapped. Then shuffle your library. */

	return panorama(player, card, event, SUBTYPE_FOREST, SUBTYPE_PLAINS, SUBTYPE_MOUNTAIN);
}

int card_necrogenesis(int player, int card, event_t event){
	/* Necrogenesis	|B|G
	 * Enchantment
	 * |2: Exile target creature card from a graveyard. Put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card to exile.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			rfg_card_from_grave(instance->targets[0].player, selected);
			generate_token_by_id(player, card, CARD_ID_SAPROLING);
		}
	}

	return 	generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER_BOTH_GRAVES);
}

int card_obelisk_of_bant(int player, int card, event_t event){
/* CARD_ID_OBELISK_OF_NAYA	10027
Obelisk of Naya	|3
Artifact
|T: Add |R, |G, or |W to your mana pool. */

	// also code for all the others obelisks.
	return mana_producer(player, card, event);
}

int card_onyx_goblet(int player, int card, event_t event){
/* CARD_ID_ONYX_GOBLET	10028
Onyx Goblet	|2|B
Artifact
|T: Target player loses 1 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_ooze_garden(int player, int card, event_t event){
/*
Ooze Garden |1|G
Enchantment
{1}{G}, Sacrifice a non-Ooze creature: Put an X/X green Ooze creature token onto the battlefield, where X is the sacrificed
		creature's power. Activate this ability only any time you could cast a sorcery.
*/

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a non-Ooze creature.");
	test.subtype = SUBTYPE_OOZE;
	test.subtype_flag = DOESNT_MATCH;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_OOZE, &token);
		token.pow = token.tou = get_card_instance(player, card)->info_slot;
		generate_token(&token);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XG(1, 1), 0, NULL, NULL,
															0, &test, SARV_REPORT_POWER);
}

int card_predator_dragon(int player, int card, event_t event){
/* CARD_ID_PREDATOR_DRAGON	10031
Predator Dragon	|3|R|R|R
Creature - Dragon 4/4
Flying, haste
Devour 2 (As this enters the battlefield, you may sacrifice any number of creatures.
This creature enters the battlefield with twice that many +1/+1 counters on it.) */

	haste(player, card, event);
	devour(player, card, event, 2);
	return 0;
}

static int prince_of_thralls_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_PRINCE_OF_THRALLS ){
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
				count_for_gfp_ability_and_store_values(player, card, event, 1-player, TYPE_PERMANENT, NULL, GFPC_TRACK_DEAD_CREATURES, 1);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			card_instance_t *instance = get_card_instance(player, card);
			int amount = BYTE0(result);
			int stored[2][amount];
			int sc = 0;
			int i;
			for(i=1; i<amount; i++){
				stored[0][sc] = instance->targets[i].player;
				stored[1][sc] = instance->targets[i].card;
				sc++;
				instance->targets[i].player = instance->targets[i].card = -1;
			}
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a permenent to keep.");
			test.id = cards_data[iid_draw_a_card].id;
			test.id_flag = DOESNT_MATCH;
			while( can_pay_life(1-player, 3) ){
					int selected = select_card_from_zone(1-player, 1-player, stored[1], sc, 0, AI_MAX_VALUE, -1, &test);
					if( selected == -1 ){
						break;
					}
					stored[1][selected] = iid_draw_a_card;
					lose_life(1-player, 3);
			}
			for(i=0; i<sc; i++){
				int owner = instance->targets[i].player;
				int iid = instance->targets[i].card;
				if( iid != iid_draw_a_card ){
					seek_grave_for_id_to_reanimate(player, card, owner, cards_data[iid].id, REANIMATE_DEFAULT);
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

int card_prince_of_thralls(int player, int card, event_t event){
/*
Prince of Thralls |4|U|B|B|R
Creature — Demon 7/7
Whenever a permanent an opponent controls is put into a graveyard, put that card onto the battlefield under your control unless
	that opponent pays 3 life.
*/
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)prince_of_thralls_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &prince_of_thralls_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability_and_store_values(player, card, event, 1-player, TYPE_PERMANENT, NULL, GFPC_TRACK_DEAD_CREATURES, 0);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			card_instance_t *instance = get_card_instance(player, card);
			int amount = BYTE0(result);
			int stored[2][amount];
			int i;
			for(i=0; i<amount; i++){
				stored[0][i] = instance->targets[i].player;
				stored[1][i] = instance->targets[i].card;
				instance->targets[i].player = instance->targets[i].card = -1;
			}
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a permenent to keep.");
			test.id = cards_data[iid_draw_a_card].id;
			test.id_flag = DOESNT_MATCH;
			while( can_pay_life(1-player, 3) ){
					int selected = select_card_from_zone(1-player, 1-player, stored[1], amount, 0, AI_MAX_VALUE, -1, &test);
					if( selected == -1 ){
						break;
					}
					stored[1][selected] = iid_draw_a_card;
					lose_life(1-player, 3);
			}
			for(i=0; i<amount; i++){
				int owner = instance->targets[i].player;
				int iid = instance->targets[i].card;
				if( iid != iid_draw_a_card ){
					seek_grave_for_id_to_reanimate(player, card, owner, cards_data[iid].id, REANIMATE_DEFAULT);
				}
			}
		}
	}

	return 0;
}

int card_punish_ignorance(int player, int card, event_t event)
{
  /* Punish Ignorance	|W|U|U|B
   * Instant
   * Counter target spell. Its controller loses 3 life and you gain 3 life. */

	if (event == EVENT_RESOLVE_SPELL)
	{
		card_instance_t* inst = get_card_instance(player, card);
		if (counterspell_validate(player, card, NULL, 0))
		{
			real_counter_a_spell(player, card, inst->targets[0].player, inst->targets[0].card);
			gain_life(player, 3);
			lose_life(inst->targets[0].player, 3);
		}
		inst->number_of_targets = 0;
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}
	return counterspell(player, card, event, NULL, 0);
}

int card_qasali_ambusher(int player, int card, event_t event){
/* CARD_ID_QASALI_AMBUSHER	10035
Qasali Ambusher	|1|G|W
Creature - Cat Warrior 2/3
Reach
If a creature is attacking you and you control |Ha Forest and |Ha Plains, you may cast ~ without paying its mana cost and as though it had flash. */

	if( event == EVENT_MODIFY_COST ){
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_FOREST) &&
			check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_PLAINS) && count_attackers(1-player) > 0
		  ){
			null_casting_cost(player, card);
		}
		else if (!can_sorcery_be_played(player, event)){
				infinite_casting_cost();
		}
	}

	return flash(player, card, event);
}

int card_quietus_spike(int player, int card, event_t event)
{
  /* Quietus Spike	|3
   * Artifact - Equipment
   * Equipped creature has deathtouch.
   * Whenever equipped creature deals combat damage to a player, that player loses half his or her life, rounded up.
   * Equip |3 */

  if (equipped_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_TRACE_DAMAGED_PLAYERS))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
	  instance->targets[1].player = 0;
	  int p;
	  for (p = 0; p <= 1; ++p)
		for (; times_damaged[p] > 0; --times_damaged[p])
		  lose_life(p, (life[p] + 1) / 2);
	}

  return vanilla_equipment(player, card, event, 3, 0, 0, 0, SP_KEYWORD_DEATHTOUCH);
}

int card_rafiq_of_the_many( int player, int card, event_t event){
/* CARD_ID_RAFIQ_OF_THE_MANY	10037
Rafiq of the Many	|1|G|W|U
Legendary Creature - Human Knight 3/3
Exalted (Whenever a creature you control attacks alone, that creature gets +1/+1 until end of turn.)
Whenever a creature you control attacks alone, it gains double strike until end of turn. */

	check_legend_rule(player, card, event);

	exalted(player, card, event, KEYWORD_DOUBLE_STRIKE, 0);

	return 0;
}

int card_rakeclaw_gargantuan(int player, int card, event_t event){
/*
Rakeclaw Gargantuan |2|W|R|G	0x200f57f
Creature — Beast 5/3
{1}: Target creature with power 5 or greater gains first strike until end of turn.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = 5 | TARGET_PT_GREATER_OR_EQUAL;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FIRST_STRIKE, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(1), 0,
									&td, "Select target creature with POW 5 or higher.");
}

int card_ranger_of_eos(int player, int card, event_t event){
/* CARD_ID_RANGER_OF_EOS	10039
Ranger of Eos	|3|W
Creature - Human Soldier 3/2
When ~ enters the battlefield, you may search your library for up to two creature cards with converted mana cost 1 or less, reveal them,
and put them into your hand. If you do, shuffle your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card with CMC 1 or less.");
		this_test.cmc = 2;
		this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		this_test.qty = 2;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return 0;
}

static int realm_razer_legacy(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_ACTIVATE && player != AI ){
		int p;
		for(p=0; p<2; p++){
			int c;
			for(c=active_cards_count[p]-1; c>-1; c--){
				if( is_what(p, c, TYPE_EFFECT) && !(p == player && c == card) ){
					card_instance_t *lnd = get_card_instance(p, c);
					if( lnd->targets[1].card == instance->targets[1].card ){
						if( lnd->targets[0].player == instance->targets[0].player && lnd->targets[0].card == instance->targets[0].card ){
							int k;
							for(k=2; k<19; k++){
								if( lnd->targets[k].player != -1 ){
									if( check_rfg(lnd->targets[k].player, cards_data[lnd->targets[k].card].id) ){
										return 1;
									}
								}
								else{
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int lands[2][500];
		int lc[2] = {0, 0};
		int p;
		for(p=0; p<2; p++){
			int c;
			for(c=active_cards_count[p]-1; c>-1; c--){
				if( is_what(p, c, TYPE_EFFECT) && !(p == player && c == card) ){
					card_instance_t *lnd = get_card_instance(p, c);
					if( lnd->targets[1].card == instance->targets[1].card ){
						if( lnd->targets[0].player == instance->targets[0].player && lnd->targets[0].card == instance->targets[0].card ){
							int k;
							for(k=2; k<19; k++){
								if( lnd->targets[k].player != -1 ){
									if( check_rfg(lnd->targets[k].player, cards_data[lnd->targets[k].card].id) ){
										lands[lnd->targets[k].player][lc[lnd->targets[k].player]] = lnd->targets[k].card;
										lc[lnd->targets[k].player]++;
									}
								}
								else{
									break;
								}
							}
						}
					}
				}
			}
		}
		show_deck( player, lands[player], lc[player], "Realm Razer exiled these lands you own.", 0, 0x7375B0 );
		show_deck( player, lands[1-player], lc[1-player], "Realm Razer exiled these lands onwed by your opponent.", 0, 0x7375B0 );
		spell_fizzled = 1;
	}

	if( leaves_play(instance->targets[0].player, instance->targets[0].card, event) ){
		int lands[2][500];
		int lc[2] = {0, 0};
		int p;
		for(p=0; p<2; p++){
			int c;
			for(c=active_cards_count[p]-1; c>-1; c--){
				if( is_what(p, c, TYPE_EFFECT) && !(p == player && c == card) ){
					card_instance_t *lnd = get_card_instance(p, c);
					if( lnd->targets[1].card == instance->targets[1].card ){
						if( lnd->targets[0].player == instance->targets[0].player && lnd->targets[0].card == instance->targets[0].card ){
							int k;
							for(k=2; k<19; k++){
								if( lnd->targets[k].player != -1 ){
									if( check_rfg(lnd->targets[k].player, cards_data[lnd->targets[k].card].id) ){
										lands[lnd->targets[k].player][lc[lnd->targets[k].player]] = lnd->targets[k].card;
										lc[lnd->targets[k].player]++;
										remove_card_from_rfg(lnd->targets[k].player, cards_data[lnd->targets[k].card].id);
									}
								}
								else{
									break;
								}
							}
							kill_card(player, card, KILL_EXILE);
						}
					}
				}
			}
		}
		for(p=0; p<2; p++){
			int c;
			for(c=0; c<lc[p]; c++){
				int card_added = add_card_to_hand(p, lands[p][c]);
				put_into_play(p, card_added);
			}
		}
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_realm_razer(int player, int card, event_t event){
/* CARD_ID_REALM_RAZER	10040
Realm Razer	|3|R|G|W
Creature - Beast 4/2
When ~ enters the battlefield, exile all lands.
When ~ leaves the battlefield, return the exiled cards to the battlefield tapped under their owners' control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int effect_legacy = create_legacy_activate(player, card, &realm_razer_legacy);
		card_instance_t *instance = get_card_instance( player, effect_legacy);
		instance->targets[0].player = player;
		instance->targets[0].card = card;
		instance->number_of_targets = 1;
		instance->targets[1].card = get_id(player, card);
		int storage_legacy = create_legacy_effect(player, card, &empty);
		add_status(player, storage_legacy, STATUS_INVISIBLE_FX);
		instance = get_card_instance( player, storage_legacy);
		instance->targets[0].player = player;
		instance->targets[0].card = card;
		instance->number_of_targets = 1;
		instance->targets[1].card = get_id(player, card);
		int i;
		for(i=0; i<2; i++){
			int count = active_cards_count[i]-1;
			while( count > -1 ){
					if( in_play(i, count) && is_what(i, count, TYPE_LAND) ){
						if( ! is_token(i, count) ){
							int iid = get_original_internal_card_id(i, count);
							int owner = get_owner(i, count);
							int found = 0;
							while( found == 0 ){
									int k;
									for(k=2; k<19; k++){
										if( instance->targets[k].player == -1 ){
											instance->targets[k].player = owner;
											instance->targets[k].card = iid;
											found = 1;
											break;
										}
									}
									if( ! found ){
										storage_legacy = create_legacy_effect(player, card, &empty);
										add_status(player, storage_legacy, STATUS_INVISIBLE_FX);
										instance = get_card_instance( player, storage_legacy);
										instance->targets[0].player = player;
										instance->targets[0].card = card;
										instance->number_of_targets = 1;
										instance->targets[1].card = get_id(player, card);
									}
							}
							kill_card(i, count, KILL_EXILE);
						}
					}
					count--;
			}
		}
	}

	return 0;
}

int card_relic_of_progenitus(int player, int card, event_t event){
/*
Relic of Progenitus |1
Artifact
{T}: Target player exiles a card from his or her graveyard.
{1}, Exile Relic of Progenitus: Exile all cards from all graveyards. Draw a card.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_TARGET_EXILES = 1,
		CHOICE_EXILE_ALL
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) )
		{
			return 1;
		}
		if( generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL) )
		{
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(1), 0, NULL, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Target player exiles a card.", 	abils[1], 5,
							"Exile all graveyards", 			abils[2], count_graveyard(1-player));
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_TARGET_EXILES ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
		}
		if( choice == CHOICE_EXILE_ALL ){
			generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_TARGET_EXILES && valid_target(&td) ){
			if( count_graveyard(instance->targets[0].player) ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ANY, "Select a card to exile.");
				new_global_tutor(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_GRAVE, TUTOR_RFG, 1, AI_MIN_VALUE,
								&test);
			}
		}
		if( instance->info_slot == CHOICE_EXILE_ALL ){
			APNAP(p, { rfg_whole_graveyard(p); };);
			draw_a_card(player);
		}
	}

	return 0;
}

int card_resounding_roar(int player, int card, event_t event){
	/* Resounding Roar	|1|G
	 * Instant
	 * Target creature gets +3/+3 until end of turn.
	 * Cycling |5|R|G|W
	 * When you cycle ~, target creature gets +6/+6 until end of turn. */

	if( IS_ACTIVATING_FROM_HAND(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && can_target(&td) ){
			draw_a_card(player);
			td.allow_cancel = 0;
			if( pick_target(&td, "TARGET_CREATURE") ){
				pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 6, 6);
			}
		}
		return cycling(player, card, event, MANACOST_XRGW(5,1,1,1));
	}

	return vanilla_instant_pump(player, card, event, ANYBODY, player, 3, 3, 0, 0);
}

int card_resounding_scream(int player, int card, event_t event){
	/* Resounding Scream	|2|B
	 * Sorcery
	 * Target player discards a card at random.
	 * Cycling |5|U|B|R
	 * When you cycle ~, target player discards two cards at random. */

	if (IS_CASTING(player, card, event) || event == EVENT_RESOLVE_ACTIVATION_FROM_HAND){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		}
		if(event == EVENT_RESOLVE_SPELL){
			if( valid_target(&td) ){
				discard(instance->targets[0].player, DISC_RANDOM, player);
			}
			kill_card(player, card, KILL_DESTROY );
		}
		if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && can_target(&td) ){
			td.allow_cancel = 0;
			if( pick_target(&td, "TARGET_PLAYER") ){
				new_multidiscard(instance->targets[0].player, 2, DISC_RANDOM, player);
			}
		}
	}
	return cycling(player, card, event, MANACOST_XUBR(5,1,1,1));
}

int card_resounding_silence(int player, int card, event_t event){
	/* Resounding Silence	|3|W
	 * Instant
	 * Exile target attacking creature.
	 * Cycling |5|G|W|U
	 * When you cycle ~, exile up to two target attacking creatures. */

	if (IS_CASTING(player, card, event) || event == EVENT_RESOLVE_ACTIVATION_FROM_HAND){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_state = TARGET_STATE_ATTACKING;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target attacking creature.", 1, NULL);
		}
		if(event == EVENT_RESOLVE_SPELL){
			if( valid_target(&td) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			}
			kill_card(player, card, KILL_DESTROY );
		}
		if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && can_target(&td) ){
			int rfged = 0;
			while( rfged < 2 && can_target(&td) ){
				if( pick_target(&td, "TARGET_ATTACKING_CREATURE") ){
					instance->number_of_targets = 1;
					kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
					rfged++;
				}
				else{
					break;
				}
			}
		}
	}
	return cycling(player, card, event, MANACOST_XGWU(5,1,1,1));
}

int card_resounding_thunder(int player, int card, event_t event){
	/* Resounding Thunder	|2|R	0x2008243
	 * Instant
	 * ~ deals 3 damage to any target.
	 * Cycling |5|B|R|G
	 * When you cycle ~, it deals 6 damage to any target. */
	if (IS_CASTING(player, card, event) || event == EVENT_RESOLVE_ACTIVATION_FROM_HAND){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		}
		if(event == EVENT_RESOLVE_SPELL){
			if( valid_target(&td) ){
				damage_target0(player, card, 3);
			}
			kill_card(player, card, KILL_DESTROY );
		}
		if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && can_target(&td) ){
			td.allow_cancel = 0;
			if( pick_target(&td, "TARGET_ANY") ){
				damage_target0(player, card, 6);
			}
		}
	}
	return cycling(player, card, event, MANACOST_XBRG(5,1,1,1));
}

int card_resounding_wave(int player, int card, event_t event){
	/* Resounding Wave	|2|U
	 * Instant
	 * Return target permanent to its owner's hand.
	 * Cycling |5|W|U|B
	 * When you cycle ~, return two target permanents to their owners' hands. */

	if (IS_CASTING(player, card, event) || event == EVENT_RESOLVE_ACTIVATION_FROM_HAND){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
		}
		if(event == EVENT_RESOLVE_SPELL){
			if( valid_target(&td) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
			kill_card(player, card, KILL_DESTROY );
		}
		if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && can_target(&td) ){
			td.allow_cancel = 0;
			int rfged = 0;
			while( rfged < 2 && can_target(&td) ){
				if( pick_target(&td, "TARGET_PERMANENT") ){
					instance->number_of_targets = 1;
					bounce_permanent(instance->targets[0].player, instance->targets[0].card);
					rfged++;
				}
				else{
					break;
				}
			}
		}
	}
	return cycling(player, card, event, MANACOST_XWUB(5,1,1,1));
}

int card_sacellum_godspeaker(int player, int card, event_t event){
	/* Sacellum Godspeaker	|2|G
	 * Creature - Elf Druid 2/2
	 * |T: Reveal any number of creature cards with power 5 or greater from your hand. Add |G to your mana pool for each card revealed this way. */

	if( ! IS_GAA_EVENT(event) && !(event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card with power 5 or more.");
	this_test.power = 4;
	this_test.power_flag = 2;
	this_test.zone = TARGET_ZONE_HAND;

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
		declare_mana_available(player, COLOR_GREEN, check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test));
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int result = reveal_cards_from_your_hand(player, card, &this_test);
		produce_mana_tapped(player, card, COLOR_GREEN, result);
	}

	return 0;
}

int card_salvage_titan(int player, int card, event_t event){

	/* Salvage Titan	|4|B|B
	 * Artifact Creature - Golem 6/4
	 * You may sacrifice three artifacts rather than pay ~'s mana cost.
	 * Exile three artifact cards from your graveyard: Return ~ from your graveyard to your hand. */

	if( event == EVENT_MODIFY_COST ){

		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
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
		card_instance_t *instance = get_card_instance(player, card);
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 1;
			if( has_mana_to_cast_iid(player, event, instance->internal_card_id) ){
				choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
									"Sac 3 artifacts", 1, 1,
									"Play normally", 1, 5);
			}
			if( choice == 1 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");

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
			else if( choice == 2 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
	          else{
				   spell_fizzled = 1;
			      }
		}
	}

	if(event == EVENT_GRAVEYARD_ABILITY){
		if( count_graveyard_by_type(player, TYPE_ARTIFACT) >= 3 ){
			return GA_RETURN_TO_HAND;
		}
	}

  if (event == EVENT_PAY_FLASHBACK_COSTS)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_ARTIFACT, "Select three artifact cards.");

	  card_instance_t *instance = get_card_instance(player, card);

	  if (!select_multiple_cards_from_graveyard(player, player, -1, AI_MIN_VALUE, &test, 3, &instance->targets[0]))
		{
		  spell_fizzled = 1;
		  return 0;
		}

	  int i;
	  for (i = 0; i < 3; ++i)
		rfg_card_from_grave(player, instance->targets[i].player);

	  // If no Salvage Titans left in graveyard, then fizzle; otherwise, obliterate the first one found (it'll be added to player's hand at resolution)
	  int pos = seek_grave_for_id_to_reanimate(player, card, player, CARD_ID_SALVAGE_TITAN, REANIMATEXTRA_LEAVE_IN_GRAVEYARD);
	  if (pos == -1)
		spell_fizzled = 1;
	  else
		{
		  /* Done here instead of activate_cards_in_hand() because it might end up being a different Salvage Titan being raised - if there are two or more in
		   * his graveyard, it's not reasonable to make the user guess which one will make the effect fizzle */
		  remove_card_from_grave(player, pos);
		  return GAPAID_REMAIN_IN_GRAVE;
		}
	}

  return 0;
}

int card_sanctum_gargoyle( int player, int card, event_t event){
/* CARD_ID_SANCTUM_GARGOYLE	1616
Sanctum Gargoyle	|3|W
Artifact Creature - Gargoyle 2/3
Flying
When ~ enters the battlefield, you may return target artifact card from your graveyard to your hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		if( count_graveyard_by_type(player, TYPE_ARTIFACT) && ! graveyard_has_shroud(player) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select target artifact card.");
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

int card_sarkhan_vol(int player, int card, event_t event){

	/* Sarkhan Vol	|2|R|G
	 * Planeswalker - Sarkhan (4)
	 * +1: Creatures you control get +1/+1 and gain haste until end of turn.
	 * -2: Gain control of target creature until end of turn. Untap that creature. It gains haste until end of turn.
	 * -6: Put five 4/4 |Sred Dragon creature tokens with flying onto the battlefield. */

	if (IS_ACTIVATING(event)){

		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		int priority_steal = 0;
		if( event == EVENT_ACTIVATE ){
			priority_steal = ((count_counters(player, card, COUNTER_LOYALTY)*2)-6)+15;
		}

		enum{
			CHOICE_PUMP_CREATURES = 1,
			CHOICE_STEAL,
			CHOICE_DRAGONS
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Pump your creatures", 1, 10, 1,
						"Act of Treason", can_target(&td), priority_steal, -2,
						"5 Dragons", 1, 20, -6);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_ACTIVATE)
		{
			instance->number_of_targets = 0;
			switch (choice)
			{
				case CHOICE_STEAL:
					pick_target(&td, "TARGET_CREATURE");
					break;

				default:
					break;
			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		switch (choice)
		{
			case CHOICE_PUMP_CREATURES:
				pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1, 1, 1, 0, SP_KEYWORD_HASTE);
				break;

			case CHOICE_STEAL:
				if( valid_target(&td) ){
					if( instance->targets[0].player == player ){
						untap_card(instance->targets[0].player, instance->targets[0].card);
						pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
												0, 0, 0, SP_KEYWORD_HASTE);
					}
					else{
						effect_act_of_treason(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
					}
				}
				break;

			case CHOICE_DRAGONS:
			{
				token_generation_t token;
				default_token_definition(player, card, CARD_ID_DRAGON, &token);
				token.pow = 4;
				token.tou = 4;
				token.qty = 5;
				generate_token(&token);
			}
			break;
		}
	}

	return planeswalker(player, card, event, 4);
}

int card_scavenger_drake(int player, int card, event_t event){
/* CARD_ID_SCAVENGER_DRAKE	1373
Scavenger Drake	|3|B
Creature - Drake 1/1
Flying
Whenever another creature dies, you may put a +1/+1 counter on ~. */

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
	if( result ){
		add_1_1_counters(player, card, BYTE0(result));
	}

	return 0;
}

int card_scourglass(int player, int card, event_t event){
/* CARD_ID_SCOURGLASS	10056
Scourglass	|3|W|W
Artifact
|T, Sacrifice ~: Destroy all permanents except for artifacts and lands. Activate this ability only during your upkeep. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT | TYPE_LAND);
		this_test.type_flag = 1;
		APNAP(p, { new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_SACRIFICE_ME|GAA_ONLY_ON_UPKEEP|GAA_IN_YOUR_TURN,
									MANACOST_X(0), 0, NULL, NULL);
}

int card_sedraxis_specter(int player, int card, event_t event)
{
  /* Sedraxis Specter	|U|B|R
   * Creature - Specter 3/2
   * Flying
   * Whenever ~ deals combat damage to a player, that player discards a card.
   * Unearth |1|B */

  whenever_i_deal_damage_to_a_player_he_discards_a_card(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE, 0);

  return unearth(player, event, MANACOST_XB(1,1));
}

int card_sedris_the_traitor_king(int player, int card, event_t event){
/* CARD_ID_SEDRIS_THE_TRAITOR_KING	10058
Sedris, the Traitor King	|3|U|B|R
Legendary Creature - Zombie Warrior 5/5
Each creature card in your graveyard has unearth |2|B. (|2|B: Return the card to the battlefield. The creature gains haste.
Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

	check_legend_rule(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		if( has_mana_multi(player, MANACOST_XB(2, 1)) && count_graveyard_by_type(player, TYPE_CREATURE) > 0 &&
			can_sorcery_be_played(player, event)
		  ){
		   return 1;
		}
	}

	if( event == EVENT_ACTIVATE){
		charge_mana_multi(player, MANACOST_XB(2, 1));
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature to unearth.");
		int selected = new_select_a_card(player, player, TUTOR_FROM_GRAVE, 1, AI_MAX_CMC, -1, &this_test);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_UNEARTH);
		}
	}

	return 0;
}

int card_sharding_sphinx(int player, int card, event_t event){
/* Sharding Sphinx	|4|U|U
 * Artifact Creature - Sphinx 4/4
 * Flying
 * Whenever an artifact creature you control deals combat damage to a player, you may put a 1/1 |Sblue Thopter artifact creature token
 *		with flying onto the battlefield. */

	if( is_humiliated(player, card) )
		return 0;

	card_instance_t* damage = combat_damage_being_dealt(event);
	if( damage &&
		damage->damage_source_player == player &&
		(damage->targets[3].player & TYPE_CREATURE) &&	// probably redundant to status check
		(damage->targets[3].player & TYPE_ARTIFACT) &&
		damage->damage_target_card == -1 &&
		! damage_is_to_planeswalker(damage) )
	{
		get_card_instance(player, card)->info_slot++;
	}

	if( trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
		reason_for_trigger_controller == player && get_card_instance(player, card)->info_slot > 0 ){
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_AI(player);
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
			card_instance_t *instance = get_card_instance(player, card);
			if (--instance->info_slot > 0){
				instance->state &= ~STATE_PROCESSING;	// More optional triggers left.  Must be the first thing done during resolve trigger.
			}
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_THOPTER, &token);
			token.color_forced = COLOR_TEST_BLUE;
			generate_token(&token);
		}
		else if (event == EVENT_END_TRIGGER){
				get_card_instance(player, card)->info_slot = 0;
		}
	}

	return 0;
}

int card_sharuum_the_hegemon(int player, int card, event_t event){
/* CARD_ID_SHARUUM_THE_HEGEMON	10061
Sharuum the Hegemon	|3|W|U|B
Legendary Artifact Creature - Sphinx 5/5
Flying
When ~ enters the battlefield, you may return target artifact card from your graveyard to the battlefield. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		if( count_graveyard_by_type(player, TYPE_ARTIFACT) && ! graveyard_has_shroud(player) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select target artifact card.");
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}

	return 0;
}

int card_sigil_blessing(int player, int card, event_t event){
	/* Sigil Blessing	|G|W
	 * Instant
	 * Until end of turn, target creature you control gets +3/+3 and other creatures you control get +1/+1. */

	if( event == EVENT_RESOLVE_SPELL){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allowed_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3, 3);

			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			test.not_me = 1;
			pump_creatures_until_eot(player, card, player, 0, 1, 1, 0, 0, &test);
		}
		kill_card(player, card, KILL_DESTROY );
		return 0;
	}

	return vanilla_instant_pump(player, card, event, player, player, 3, 3, 0, 0);
}

int card_sigil_of_distinction(int player, int card, event_t event){

	/* Sigil of Distinction	|X
	 * Artifact - Equipment
	 * ~ enters the battlefield with X charge counters on it.
	 * Equipped creature gets +1/+1 for each charge counter on ~.
	 * Equip-Remove a charge counter from ~. */

	if( event == EVENT_MODIFY_COST ){
		if( check_battlefield_for_id(2, CARD_ID_GADDOCK_TEEG) ){
			infinite_casting_cost();
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( IS_AI(player) && ! has_mana(player, COLOR_ARTIFACT, 2) ){
			ai_modifier-=1000;
		}
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			charge_mana(player, COLOR_ARTIFACT, -1);
			if( spell_fizzled != 1  ){
				set_x_for_x_spells(player, card, event, x_value);
			}
		}
		set_special_flags2(player, card, SF2_X_SPELL);
	}

	enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, get_card_instance(player, card)->info_slot);

	if( is_equipping(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			event_result += count_counters(player, card, COUNTER_CHARGE);
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( is_equipping(player, card) ){
			card_instance_t *instance = get_card_instance(player, card);

			int p = instance->damage_target_player;
			int c = instance->damage_target_card;

			if( !(check_for_ability(p, c, KEYWORD_DEFENDER) && check_for_special_ability(p, c, SP_KEYWORD_CANNOT_BLOCK)) ){
				ai_modifier-=500;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return basic_equipment(player, card, event, 0);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	return generic_activated_ability(player, card, event, GAA_EQUIPMENT|GAA_LITERAL_PROMPT, MANACOST0, GVC_COUNTER(COUNTER_CHARGE),
	                                  &td, "Select target creature you control.");
}

int card_skill_borrower(int player, int card, event_t event){
/*
Skill Borrower |2|U
Artifact Creature — Human Wizard 1/3
Play with the top card of your library revealed.
As long as the top card of your library is an artifact or creature card, Skill Borrower has all activated abilities of that card.
	(If any of the abilities use that card's name, use this creature's name instead.)
*/
	if( ! in_play(player, card) || is_humiliated(player, card) )
		return 0;

	reveal_top_card(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	int iid = deck_ptr[player][0];

	if( iid == -1 )
		return 0;

	if( ! is_what(-1, iid, TYPE_CREATURE | TYPE_ARTIFACT) )
		return 0;

	if( event == EVENT_CAN_ACTIVATE ){
		CardFunction ptFunction = cards_data[iid].code_pointer;
		return ptFunction(player, card, EVENT_CAN_ACTIVATE);
	}

	if( event == EVENT_ACTIVATE ){
		CardFunction ptFunction = cards_data[iid].code_pointer;
		return ptFunction(player, card, EVENT_ACTIVATE);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		CardFunction ptFunction = cards_data[iid].code_pointer;
		return ptFunction(player, card, EVENT_RESOLVE_ACTIVATION);
	}

	return 0;
}

int card_skullmulcher(int player, int card, event_t event){
	/* Skullmulcher	|4|G
	 * Creature - Elemental 3/3
	 * Devour 1
	 * When ~ enters the battlefield, draw a card for each creature it devoured. */

	devour(player, card, event, 1);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, get_card_instance(player, card)->targets[1].card);
	}

	return 0;
}

int card_souls_fire(int player, int card, event_t event){
	/* Soul's Fire	|2|R	0x2008275
	 * Instant
	 * Target creature you control deals damage equal to its power to any target. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.allowed_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td2) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if(event == EVENT_CAST_SPELL ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature you control.", 1, NULL);
		if( spell_fizzled != 1 ){
			new_pick_target(&td2, "TARGET_ANY", 1, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td2, 1) ){
			int dmg = get_power(instance->targets[0].player, instance->targets[0].card);
			damage_creature(instance->targets[1].player, instance->targets[1].card, dmg,
							instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_spearbreaker_behemoth(int player, int card, event_t event){
/* CARD_ID_SPEARBREAKER_BEHEMOTH	10074
Spearbreaker Behemoth	|5|G|G
Creature - Beast 5/5
Indestructible
|1: Target creature with power 5 or greater gains indestructible until end of turn. */

	indestructible(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = 5 | TARGET_PT_GREATER_OR_EQUAL;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_INDESTRUCTIBLE);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(1), 0,
									&td, "Select target creature with POW 5 or higher.");
}

int card_sphinx_sovereign(int player, int card, event_t event){
/* CARD_ID_SPHINX_SOVEREIGN	10076
Sphinx Sovereign	|4|W|U|U|B
Artifact Creature - Sphinx 6/6
Flying
At the beginning of your end step, you gain 3 life if ~ is untapped. Otherwise, each opponent loses 3 life. */

	if( current_turn == player && eot_trigger(player, card, event) ){
		if( is_tapped(player, card) ){
			lose_life(1-player, 3);
		}
		else{
			gain_life(player, 3);
		}
	}
	return 0;
}

int card_sprouting_thrinax(int player, int card, event_t event){
	/* Sprouting Thrinax	|B|R|G
	 * Creature - Lizard 3/3
	 * When ~ dies, put three 1/1 |Sgreen Saproling creature tokens onto the battlefield. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, 3);
	}

	return 0;
}

int card_steward_of_valeron(int player, int card, event_t event){
/* CARD_ID_STEWARD_OF_VALERON	10080
Steward of Valeron	|G|W
Creature - Human Druid Knight 2/2
Vigilance
|T: Add |G to your mana pool. */

	vigilance(player, card, event);
	if (event == EVENT_ATTACK_RATING && affect_me(player, card)){
		return 0;	// Don't make extra-reluctant to attack, due to vigilance
	}
	return mana_producing_creature(player, card, event, 24, COLOR_GREEN, 1);
}

int card_stoic_angel(int player, int card, event_t event){
	/*
	  Stoic Angel English |1|W|U|G
	  Creature - Angel 3/4, 1WUG (4)
	  Flying, vigilance
	  Players can't untap more than one creature during their untap steps.
	*/
	vigilance(player, card, event);
	return card_smoke(player, card, event);
}

int card_tar_fiend(int player, int card, event_t event){
	/* Tar Fiend	|5|B
	 * Creature - Elemental 4/4
	 * Devour 2
	 * When ~ enters the battlefield, target player discards a card for each creature it devoured. */

	devour(player, card, event, 2);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t* instance = get_card_instance(player, card);

		int devoured = instance->targets[1].card;

		if( devoured ){
			target_definition_t td;
			default_target_definition(player, card, &td, 0);
			td.zone = TARGET_ZONE_PLAYERS;
			td.allow_cancel = 0;
			if (can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
				new_multidiscard(instance->targets[0].player, devoured, 0, player);
			}
		}
	}

	return 0;
}
static int tezzeret_the_seeker_legacy(int player, int card, event_t event){
	if( event == EVENT_CHANGE_TYPE ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT);
		this_test.type_flag = F1_NO_CREATURE;
		global_type_change(player, card, event, player, TYPE_CREATURE, &this_test, 5, 5, 0, 0, 0);
	}
	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_tezzeret_the_seeker(int player, int card, event_t event){
	if( ! is_unlocked(player, card, event, 23) ){ return 0; }

	/* Tezzeret the Seeker	|3|U|U
	 * Planeswalker - Tezzeret (4)
	 * +1: Untap up to two target artifacts.
	 * -X: Search your library for an artifact card with converted mana cost X or less and put it onto the battlefield. Then shuffle your library.
	 * -5: Artifacts you control become 5/5 artifact creatures until end of turn. */

	if (IS_ACTIVATING(event)){

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);
		int priority_tutor = 0;
		int priority_animate = 0;
		if( event == EVENT_ACTIVATE ){
			int max_t = count_counters(player, card, COUNTER_LOYALTY)-1;
			int i = 0;
			while( deck_ptr[player][i] != -1 ){
					if( is_what(-1, deck_ptr[player][i], TYPE_ARTIFACT) && get_cmc_by_internal_id(deck_ptr[player][i]) <= max_t ){
						int new_p = (get_cmc_by_internal_id(deck_ptr[player][i]) * 4)+4;
						priority_tutor = new_p > priority_tutor ? new_p : priority_tutor;
						if( priority_tutor == (max_t * 4)+4 ){
							break;
						}
					}
					i++;
			}

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ARTIFACT);
			this_test.type_flag = F1_NO_CREATURE;
			priority_animate += check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test) * 4;
			if( priority_animate ){
				priority_animate += ((count_counters(player, card, COUNTER_LOYALTY)*5)-25);
			}
		}

		enum{
			CHOICE_UNTAP = 1,
			CHOICE_TUTOR,
			CHOICE_ANIMATE
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Untap up to 2 artifacts", 1, 10, 1,
						"Tutor an artifact", 1, priority_tutor, 0,
						"Global artifact animation", 1, priority_animate, -5);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_ACTIVATE)
		{
		  instance->number_of_targets = 0;
		  switch (choice)
			{
				case CHOICE_UNTAP:
				{
					if( new_pick_target(&td, "TARGET_ARTIFACT", 0, 0) ){
						state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
						if( can_target(&td) ){
							new_pick_target(&td, "TARGET_ARTIFACT", 1, 0);
						}
						state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
					}
				}
				break;

				case CHOICE_TUTOR:
				{
					if( player == AI ){
						remove_counters(player, card, COUNTER_LOYALTY, priority_tutor/4);
						instance->targets[1].player = priority_tutor/4;
					}
					else{
						int ctr = choose_a_number(player, "How many Loyalty you'll pay ?", count_counters(player, card, COUNTER_LOYALTY));
						if( ctr > -1 || ctr <= count_counters(player, card, COUNTER_LOYALTY) ){
							remove_counters(player, card, COUNTER_LOYALTY, ctr);
							instance->targets[1].player = ctr;
						}
						else{
							spell_fizzled = 1;
						}
					}
				}
				break;

			  case CHOICE_ANIMATE:
					break;
			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		switch (choice)
		{
			case CHOICE_UNTAP:
			{
				int i;
				for(i=0;i<instance->number_of_targets;i++){
					if( validate_target(player, card, &td, i) ){
						untap_card( instance->targets[i].player, instance->targets[i].card );
					}
				}
			}
			break;

			case CHOICE_TUTOR:
			{
				char buffer[500];
				scnprintf(buffer, 500, " Select an artifact with CMC %d or less.", instance->targets[1].player);
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ARTIFACT, buffer);
				this_test.cmc = instance->targets[1].player+1;
				this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
				new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
			}
			break;

			case CHOICE_ANIMATE:
				create_legacy_effect(instance->parent_controller, instance->parent_card, &tezzeret_the_seeker_legacy);
				break;
		}
	}

	return planeswalker(player, card, event, 4);
}

int card_thorn_thrash_viashino(int player, int card, event_t event){
/* CARD_ID_THORN_THRASH_VIASHINO	10085
Thorn-Thrash Viashino	|3|R
Creature - Viashino Warrior 2/2
Devour 2 (As this enters the battlefield, you may sacrifice any number of creatures. This creature enters the battlefield with twice that many +1/+1 counters on it.)
|G: ~ gains trample until end of turn. */

	devour(player, card, event, 2);

	return generic_shade(player, card, event, 0, MANACOST_G(1), 0, 0, KEYWORD_TRAMPLE, 0);
}

int card_thoughtcutter_agent(int player, int card, event_t event){
/* CARD_ID_THOUGHTCUTTER_AGENT	10086
Thoughtcutter Agent	|U|B
Artifact Creature - Human Rogue 1/1
|U|B, |T: Target player loses 1 life and reveals his or her hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 1);
			if( ! IS_AI(player)	){
				reveal_target_player_hand(instance->targets[0].player);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_UB(1, 1), 0, &td, "TARGET_PLAYER");
}

int card_thunder_thrash_elder(int player, int card, event_t event){
/* CARD_ID_THUNDER_THRASH_ELDER	10087
Thunder-Thrash Elder	|2|R
Creature - Viashino Warrior 1/1
Devour 3 (As this enters the battlefield, you may sacrifice any number of creatures.
This creature enters the battlefield with three times that many +1/+1 counters on it.) */

	devour(player, card, event, 3);
	return 0;
}

int card_tidehollow_sculler(int player, int card, event_t event){
/*
Tidehollow Sculler |W|B
Artifact Creature — Zombie 2/2
When Tidehollow Sculler enters the battlefield, target opponent reveals his or her hand and you choose a nonland card from it.
	Exile that card.
When Tidehollow Sculler leaves the battlefield, return the exiled card to its owner's hand.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;

		if( would_validate_arbitrary_target(&td, 1-player, -1) && hand_count[1-player] ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Select a nonland card.");
			test.type_flag = DOESNT_MATCH;
			int selected = new_select_a_card(player, 1-player, TUTOR_FROM_HAND, 0, AI_MAX_VALUE, -1, &test);
			if( selected != -1 ){
				create_card_name_legacy(player, card, get_id(1-player, selected));
				exile_card_and_remember_it_on_exiledby(player, card, 1-player, selected);
			}
		}
	}

	if( leaves_play(player, card, event) ){
		int leg = 0;
		int idx = 0;
		int* loc;

		while( (loc = exiledby_find_any(player, card, &leg, &idx)) != NULL ){
				int owner = (*loc & 0x80000000) ? 1 : 0;
				int iid = *loc & ~0x80000000;
				*loc = -1;
				if( check_rfg(owner, cards_data[iid].id) ){
					add_card_to_hand(owner, iid);
					remove_card_from_rfg(owner, cards_data[iid].id);
				}
		}
	}

	return 0;
}

int card_titanic_ultimatum(int player, int card, event_t event){
/* CARD_ID_TITANIC_ULTIMATUM	10089
Titanic Ultimatum	|R|R|G|G|G|W|W
Sorcery
Until end of turn, creatures you control get +5/+5 and gain first strike, lifelink, and trample. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS ){
			ai_modifier += (count_subtype(player, TYPE_CREATURE, -1) * 12);
		}
		else{
			ai_modifier -= 96;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 5, 5, KEYWORD_TRAMPLE | KEYWORD_FIRST_STRIKE, SP_KEYWORD_LIFELINK);
		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

/* CARD_ID_TOPAN_ASCETIC	1366 =>   CARD_ID_BRAMBLESNAP
Topan Ascetic	|2|G
Creature - Human Monk 2/2
Tap an untapped creature you control: ~ gets +1/+1 until end of turn. */

int card_vein_drinker(int player, int card, event_t event){
/* CARD_ID_VEIN_DRINKER	10093
Vein Drinker	|4|B|B
Creature - Vampire 4/4
Flying
|R, |T: ~ deals damage equal to its power to target creature. That creature deals damage equal to its power to ~.
Whenever a creature dealt damage by ~ this turn dies, put a +1/+1 counter on ~. */

	if( sengir_vampire_trigger(player, card, event, 2) ){
		add_1_1_counters(player, card, get_card_instance(player, card)->targets[11].card);
		get_card_instance(player, card)->targets[11].card = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		int vd_damage = get_power(instance->parent_controller, instance->parent_card);
		int t_damage = get_power(instance->targets[0].player, instance->targets[0].card);
		damage_creature(instance->targets[0].player, instance->targets[0].card, vd_damage,
						instance->parent_controller, instance->parent_card);
		damage_creature(instance->parent_controller, instance->parent_card, t_damage,
						instance->targets[0].player, instance->targets[0].card );
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td, "TARGET_CREATURE");
}

static int vicious_shadows_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_VICIOUS_SHADOWS ){
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
				count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			target_definition_t td;
			default_target_definition(player, card, &td, 0);
			td.zone = TARGET_ZONE_PLAYERS;
			card_instance_t *instance = get_card_instance(player, card);
			if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 1, 0) ){
				damage_target0(player, card, BYTE0(result) * hand_count[instance->targets[0].player]);
			}
			instance->number_of_targets = 1;
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

int card_vicious_shadows(int player, int card, event_t event){
/*
Vicious Shadows |6|R
Enchantment
Whenever a creature dies, you may have Vicious Shadows deal damage to target player equal to the number of cards in that player's hand.
*/
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)vicious_shadows_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &vicious_shadows_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			target_definition_t td;
			default_target_definition(player, card, &td, 0);
			td.zone = TARGET_ZONE_PLAYERS;
			card_instance_t *instance = get_card_instance(player, card);
			if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
				damage_target0(player, card, BYTE0(result) * hand_count[instance->targets[0].player]);
			}
			instance->number_of_targets = 0;
		}
	}

	return global_enchantment(player, card, event);
}

int card_violent_ultimatum(int player, int card, event_t event){
/* CARD_ID_VIOLENT_ULTIMATUM	10096
Violent Ultimatum	|B|B|R|R|R|G|G
Sorcery
Destroy three target permanents. */

	if( ! is_unlocked(player, card, event, 30) ){ return 0; }

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0;i<3;i++){
			if( validate_target(player, card, &td, i) ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GAA_CAN_TARGET, &td, "TARGET_PERMANENT", 3, NULL);
}


int card_viscera_dragger(int player, int card, event_t event){
/* CARD_ID_VISCERA_DRAGGER	10097
Viscera Dragger	|3|B
Creature - Zombie Ogre Warrior 3/3
Cycling |2 (|2, Discard this card: Draw a card.)
Unearth |1|B (|1|B: Return this card from your graveyard to the battlefield. It gains haste.
Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

 	if( IS_ACTIVATING_FROM_HAND(event) ){
		return cycling(player, card, event, MANACOST_X(2));
	}
	return unearth(player, event, MANACOST_XB(1, 1));
}

int card_where_ancients_tread(int player, int card, event_t event){
	/* Where Ancients Tread	|4|R	0x200829d
	 * Enchantment
	 * Whenever a creature with power 5 or greater enters the battlefield under your control, you may have ~ deal 5 damage to any target. */
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.power = 4;
		this_test.power_flag = F5_POWER_GREATER_THAN_VALUE;

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td1) && new_pick_target(&td1, "TARGET_ANY", 0, 0) ){
			damage_target0(player, card, 5);
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_wild_nacatal(int player, int card, event_t event){
/* CARD_ID_WILD_NACATL	1326
Wild Nacatl	|G
Creature - Cat Warrior 1/1
~ gets +1/+1 as long as you control |Ha Mountain.
~ gets +1/+1 as long as you control |Ha Plains. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card) ){
		int count, mountain = 0, plains = 0;
		for( count = 0; count < active_cards_count[player]; ++count ){
			if( in_play(player, count) ){
				if( !mountain && has_subtype(player, count, SUBTYPE_MOUNTAIN) ){
					mountain = 1;
					if (plains)
						break;
				}
				if( !plains && has_subtype(player, count, SUBTYPE_PLAINS) ){
					plains = 1;
					if (mountain)
						break;
				}
			}
		}
		event_result += mountain + plains;
	}

	return 0;
}

int card_worldheart_phoenix(int player, int card, event_t event){
	/* Worldheart Phoenix	|3|R
	 * Creature - Phoenix 2/2
	 * Flying
	 * You may cast ~ from your graveyard by paying |W|U|B|R|G rather than paying its mana cost. If you do, it enters the battlefield with two +1/+1 counters on it. */

	if(event == EVENT_GRAVEYARD_ABILITY){
		if( has_mana_multi( player, 0, 1, 1, 1, 1, 1) && can_sorcery_be_played(player, event) ){
			return GA_RETURN_TO_PLAY_MODIFIED;
		}
	}
	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana_multi( player, 0, 1, 1, 1, 1, 1);
		if( spell_fizzled != 1){
			return GAPAID_REMOVE;
		}
	}
	if( event == EVENT_RETURN_TO_PLAY_FROM_GRAVE_MODIFIED ){
		add_1_1_counters(player, card, 2);
	}
	return 0;
}
