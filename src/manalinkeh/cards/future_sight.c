#include "manalink.h"

// Functions

static const char* exchangeable_aura(int who_chooses, int player, int card)
{
	if( has_subtype(player, card, SUBTYPE_AURA_CREATURE) || has_subtype(player, card, SUBTYPE_AURA_PERMANENT) ){
		return NULL;
	}
	return "this aura cannot enchant a creature.";
}

static int aura_swap(int player, int card, event_t event, int cless, int black, int blue, int green, int red, int white,
					 int p_pump, int t_pump, int key_pump, int s_key_pump ){

	/* Aura swap |2|U (|2|U: Exchange this Aura with an Aura card in your hand.) */

	card_instance_t *instance = get_card_instance(player, card);

	if( in_play(player, card) && instance->damage_target_player > -1 ){

		if( event == EVENT_CAN_ACTIVATE ){
			if( generic_activated_ability(player, card, event, 0, cless, black, blue, green, red, white, 0, NULL, NULL) ){
				target_definition_t td;
				base_target_definition(player, card, &td, TYPE_ENCHANTMENT);
				td.zone = TARGET_ZONE_HAND;
				td.extra = (int32_t)exchangeable_aura;
				td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
				td.illegal_abilities = 0;

				return can_target(&td);
			}
		}

		if(event == EVENT_ACTIVATE ){
			charge_mana_for_activated_ability(player, card, cless, black, blue, green, red, white);
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			target_definition_t td;
			base_target_definition(player, card, &td, TYPE_ENCHANTMENT);
			td.zone = TARGET_ZONE_HAND;
			td.extra = (int32_t)exchangeable_aura;
			td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
			td.illegal_abilities = 0;

			if( can_target(&td) && new_pick_target(&td, "Select an Aura that can enchant this creature.", 0, GS_LITERAL_PROMPT) ){
				int selected = instance->targets[0].card;
				put_into_play_aura_attached_to_target(player, selected, instance->damage_target_player, instance->damage_target_card);
				bounce_permanent(instance->parent_controller, instance->parent_card);
			}
		}
	}

	return generic_aura(player, card, event, player, p_pump, t_pump, key_pump, s_key_pump, 0, 0, 0);
}

static int discard_id_from_hand(int player, int id){

	if( hand_count[player] > 0 ){
		int count = active_cards_count[player]-1;
		while( count > -1  ){
				if( in_hand(player, count) && get_id(player, count) == id ){
					discard_card(player, count);
					return count;
				}
				count--;
		}
	}

	return 0;
}

static int grandeur(int player, int card, event_t event ){

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			return is_id_in_hand(player, get_id(player, card));
		}
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			discard_id_from_hand(player, get_id(player, card));
		}
	}

	return 0;
}

void modify_cost_for_delve(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	if( event == EVENT_MODIFY_COST ){
		const int *grave = get_grave(player);
		if( grave[0] != -1 ){
			card_ptr_t* c = cards_ptr[ get_id(player, card) ];
			int cless = get_updated_casting_cost(player, card, -1, event, -1);
			cless-=count_graveyard(player);
			if( cless < 0 ){
				cless = 0;
			}
			if( has_mana_multi(player, cless, c->req_black, c->req_blue, c->req_green, c->req_red, c->req_white) ){
				instance->info_slot = 1;
				null_casting_cost(player, card);
			}
		}
		else{
			instance->info_slot = 0;
		}
	}
}

int cast_spell_with_delve(int player, int card){
	card_instance_t *instance = get_card_instance(player, card);
	int choice = 0;
	if( has_mana_to_cast_iid(player, EVENT_CAST_SPELL, get_card_instance(player, card)->internal_card_id) ){
		choice = do_dialog(player, player, card, -1, -1, " Use Delve\n Play normally\n Cancel", 1);
	}
	if( choice == 2 ){
		spell_fizzled = 1;
		return 0;
	}
	int i;
	int id = get_id(player, card);
	card_ptr_t* c = cards_ptr[ id ];
	int cless = get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, -1);
	if( choice == 0 ){
		int max = MIN(cless, count_graveyard(player));
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile for Delve.");
		int trc = select_multiple_cards_from_graveyard(player, player, 0, AI_MIN_VALUE, &this_test, max, &instance->targets[0]);
		charge_mana_multi(player, cless-trc, c->req_black, c->req_blue, c->req_green, c->req_red, c->req_white);
		if( spell_fizzled != 1 ){
			int result = get_id(player, card) == CARD_ID_SOULFLAYER ? 0 : 2;
			const int *grave = get_grave(player);
			for(i=0; i<trc; i++){
				if( get_id(player, card) == CARD_ID_SOULFLAYER ){
					int ability_mask = KEYWORD_FIRST_STRIKE | KEYWORD_DOUBLE_STRIKE | KEYWORD_TRAMPLE | KEYWORD_FLYING | KEYWORD_REACH;
					result |= (cards_data[grave[instance->targets[i].player]].static_ability & ability_mask);
				}
				rfg_card_from_grave(player, instance->targets[i].player);
			}
			return result;
		}
		else{
			return 0;
		}
	}

	if( choice == 1 ){
		charge_mana_multi(player, cless, c->req_black, c->req_blue, c->req_green, c->req_red, c->req_white);
		if( spell_fizzled != 1 ){
			return 1;
		}
	}
	return 0;
}

int delve(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	modify_cost_for_delve(player, card, event);
	if( event == EVENT_CAST_SPELL && affect_me(player, card)  ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1){
			if( ! cast_spell_with_delve(player, card) ){
				spell_fizzled = 1;
			}
		}
	}
	return 0;
}

int slivercycling(int player, int card, event_t event){

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_X(3));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a Sliver card.");
		this_test.subtype = SUBTYPE_SLIVER;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return 0;
}

static int generic_pact_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);

  if (current_turn == instance->targets[0].player && upkeep_trigger(player, card, event))
	{
	  if (!charge_mana_multi_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, instance->targets[0].player,
											 instance->targets[1].player, instance->targets[2].player, instance->targets[3].player,
											 instance->targets[4].player, instance->targets[5].player, instance->targets[6].player))
		lose_the_game(instance->targets[0].player);

	  kill_card(player, card, KILL_EXILE);
	}

  return 0;
}

static int create_pact_legacy(int player, int card, int t_player, int cless, int black, int blue, int green, int red, int white){
	int legacy = create_legacy_effect(player, card, &generic_pact_legacy);
	card_instance_t *instance = get_card_instance(player, legacy);
	instance->targets[0].player = t_player;
	instance->targets[1].player = cless;
	instance->targets[2].player = black;
	instance->targets[3].player = blue;
	instance->targets[4].player = green;
	instance->targets[5].player = red;
	instance->targets[6].player = white;
	return legacy;
}

static int has_virtual_mana_multi(int player, int cless, int black, int blue, int green, int red, int white){
	int total[] = { cless, black, blue, green, red, white };
	int i, card;
	for (card = 0; card < active_cards_count[player]; ++card){
		if (in_play(player, card) && is_what(player, card, TYPE_PERMANENT)){
			card_data_t* card_d = get_card_data(player, card);
			if (card_d->extra_ability & EA_MANA_SOURCE){
				card_instance_t *instance = get_card_instance(player, card);
				int clr = get_color_of_mana_produced_by_id(card_d->id, instance->info_slot, player);
				if (clr == -1){
					clr = card_d->color;
				}
				for (i = COLOR_COLORLESS; i <= COLOR_WHITE; ++i){
					if (clr & (1 << i)){
						if (total[i] > 0){	// apply it to the appropriate color if needed
							--total[i];
						} else {
							--total[COLOR_COLORLESS];	// otherwise, apply it to colorless
						}
					}
				}
			}
		}
	}
	for (i = COLOR_COLORLESS; i <= COLOR_WHITE; ++i){
		if (total[i] > 0){
			return 0;
		}
	}
	return 1;
}

static void fateseal(int player, int number_of_cards){
	return scrylike_effect(player, 1-player, number_of_cards);
}


// Cards

int card_akromas_memorial(int player, int card, event_t event)
{
/* CARD_ID_AKROMAS_MEMORIAL	1607
Akroma's Memorial	|7
Legendary Artifact
Creatures you control have flying, first strike, vigilance, trample, haste, and protection from |Sblack and from |Sred. */

	check_legend_rule(player, card, event);

	if( in_play(player, card) && !is_humiliated(player, card) ){
		if( event == EVENT_ABILITIES && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
		{
			int key = KEYWORD_FLYING | KEYWORD_FIRST_STRIKE | KEYWORD_TRAMPLE;
			event_result |= (key | get_sleighted_protection(player, card, KEYWORD_PROT_BLACK | KEYWORD_PROT_RED));
			vigilance(affected_card_controller, affected_card, event);
			haste(affected_card_controller, affected_card, event);
		}
	}

	return 0;
}

int card_angel_of_salvation(int player, int card, event_t event)
{
	/* Angel of Salvation	|6|W|W	0x200bb64
	 * Creature - Angel 5/5
	 * Flash; convoke
	 * Flying
	 * When ~ enters the battlefield, prevent the next 5 damage that would be dealt this turn to any number of targets, divided as you choose. */
	if (event == EVENT_MODIFY_COST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return generic_spell_with_convoke(player, card, event);
	}

	if (event == EVENT_CAN_CAST){
		return (land_can_be_played & LCBP_DAMAGE_PREVENTION) ? 99 : 1;
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	  td.preferred_controller = player;
	  td.allow_cancel = 0;

	  card_instance_t* instance = get_card_instance(player, card);

	  instance->number_of_targets = 0;
	  int i, j;
	  for (i = 0; i < 5; ++i)
		{
		  char prompt[200];
		  if (ai_is_speculating == 1)
			prompt[0] = 0;
		  else
			{
			  load_text(0, "TARGET_ANY");
			  scnprintf(prompt, 200, "%s (%d of 5)", text_lines[0], i + 1);
			}

		  if (can_target(&td) && pick_next_target_noload(&td, prompt)
			  && instance->targets[i].card != -1)
			get_card_instance(instance->targets[i].player, instance->targets[i].card)->state |= STATE_TARGETTED;
		}

	  for (i = 0; i < 5; ++i)
		if (instance->targets[i].player != -1)
		  {
			if (instance->targets[i].card != -1)
			  get_card_instance(instance->targets[i].player, instance->targets[i].card)->state &= ~STATE_TARGETTED;

			// consolidate targets chosen more than once
			int prev = 1;
			for (j = i + 1; j < 5; ++j)
			  if (instance->targets[j].player == instance->targets[i].player
				  && instance->targets[j].card == instance->targets[i].card)
				{
				  ++prev;
				  instance->targets[j].player = -1;
				}

			prevent_the_next_n_damage(player, card, instance->targets[i].player, instance->targets[i].card, prev, 0, -1, -1);
		  }
	}

	return flash(player, card, event);
}

int card_arcanum_wings(int player, int card, event_t event){
/* CARD_ID_ARCANUM_WINGS	8885
Arcanum Wings	|1|U
Enchantment - Aura
Enchant creature
Enchanted creature has flying.
Aura swap |2|U (|2|U: Exchange this Aura with an Aura card in your hand.) */

	return aura_swap(player, card, event, MANACOST_XU(2, 1), 0, 0, KEYWORD_FLYING, 0);
}

int card_augur_of_skulls(int player, int card, event_t event){
/* CARD_ID_AUGUR_OF_SKULLS	8887
Augur of Skulls	|1|B
Creature - Skeleton Wizard 1/1
|1|B: Regenerate ~.
Sacrifice ~: Target player discards two cards. Activate this ability only during your upkeep. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_REGENERATION, MANACOST_XB(1, 1), 0, NULL, NULL) == 99){
			return 99;
		}
		return generic_activated_ability(player, card, event, GAA_ONLY_ON_UPKEEP | GAA_IN_YOUR_TURN | GAA_SACRIFICE_ME | GAA_CAN_TARGET,
										MANACOST0, 0, &td, "TARGET_PLAYER");
	}

	if(event == EVENT_ACTIVATE ){
		if( regeneration(player, card, EVENT_CAN_ACTIVATE, MANACOST_XB(1, 1)) ){
			generic_activated_ability(player, card, event, GAA_REGENERATION, MANACOST_XB(1, 1), 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				instance->info_slot = 66;
			}
		}
		else{
			generic_activated_ability(player, card, event, GAA_ONLY_ON_UPKEEP | GAA_IN_YOUR_TURN | GAA_SACRIFICE_ME | GAA_CAN_TARGET,
										MANACOST0, 0, &td, "TARGET_PLAYER");
			if( spell_fizzled != 1 ){
				instance->info_slot = 67;
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 && can_regenerate(instance->parent_controller, instance->parent_card) ){
			regenerate_target(instance->parent_controller, instance->parent_card);
		}
		if( instance->info_slot == 67 && valid_target(&td) ){
			new_multidiscard(instance->targets[0].player, 2, 0, player);
		}
	}

	return 0;
}

int card_aven_mindcensor(int player, int card, event_t event){
/* CARD_ID_AVEN_MINDCENSOR	8889
Aven Mindcensor	|2|W
Creature - Bird Wizard 2/1
Flash
Flying
If an opponent would search a library, that player searches the top four cards of that library instead. */

	return flash(player, card, event);
}

int card_barren_glory(int player, int card, event_t event){
/* CARD_ID_BARREN_GLORY	8890
Barren Glory	|4|W|W
Enchantment
At the beginning of your upkeep, if you control no permanents other than ~ and have no cards in hand, you win the game. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( (event == EVENT_SHOULD_AI_PLAY || (current_turn == player && upkeep_trigger(player, card, event))) &&
			hand_count[player] < 1 && count_subtype(player, TYPE_PERMANENT, -1) == 1 )
		{
			lose_the_game(1-player);
		}
	}
	return global_enchantment(player, card, event);
}

int card_baru_fist_of_krosa(int player, int card, event_t event){
/* CARD_ID_BARU_FIST_OF_KROSA	8891
Baru, Fist of Krosa	|3|G|G
Legendary Creature - Human Druid 4/4
Whenever |Ha Forest enters the battlefield, |Sgreen creatures you control get +1/+1 and gain trample until end of turn.
Grandeur - Discard another card named ~: Create an X/X |Sgreen Wurm creature token, where X is the number of lands you control. */

	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &this_test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		pump_creatures_until_eot(player, card, player, 0, 1, 1, KEYWORD_TRAMPLE, 0, &this_test);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_WURM, &token);
		token.pow = count_subtype(player, TYPE_LAND, -1);
		token.tou = count_subtype(player, TYPE_LAND, -1);
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		generate_token(&token);
	}

	return grandeur(player, card, event);
}

int card_bitter_ordeal(int player, int card, event_t event){
/* CARD_ID_BITTER_ORDEAL	8892
Bitter Ordeal	|2|B
Sorcery
Search target player's library for a card and exile it. Then that player shuffles his or her library.
Gravestorm (When you cast this spell, copy it for each permanent put into a graveyard this turn. You may choose new targets for the copies.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( pick_target(&td, "TARGET_PLAYER")  ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");

			int amount = get_dead_count(2, TYPE_PERMANENT);
			int i = 0;
			int cd = count_deck(instance->targets[0].player);
			while( i < amount && cd > 0 ){
					if( new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_RFG, 0, 1, &this_test) != -1 ){
						cd--;
					}
					pick_target(&td, "TARGET_PLAYER");
					i++;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) && deck_ptr[instance->targets[0].player][0] != -1){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_RFG, 0, 1, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_bonded_fetch(int player, int card, event_t event){
/* CARD_ID_BONDED_FETCH	8898
Bonded Fetch	|2|U
Creature - Homunculus 0/2
Defender, haste
|T: Draw a card, then discard a card. */

	haste(player, card, event);
	return card_merfolk_looter(player, card, event);
}

int card_bridge_from_below(int player, int card, event_t event){
	/* Bridge from Below	|B|B|B
	 * Enchantment
	 * Whenever a nontoken creature is put into your graveyard from the battlefield, if ~ is in your graveyard, put a 2/2 |Sblack Zombie creature token onto the battlefield.
	 * When a creature is put into an opponent's graveyard from the battlefield, if ~ is in your graveyard, exile ~. */

	// the code for this card is below
	if( event == EVENT_CAN_CAST ){
		if( player != AI ){
			return 1;
		}
	}
	return 0;
}

int bridge_from_below(int player, int card, event_t event){
	/* Bridge from Below	|B|B|B
	 * Enchantment
	 * Whenever a nontoken creature is put into your graveyard from the battlefield, if ~ is in your graveyard, put a 2/2 |Sblack Zombie creature token onto the battlefield.
	 * When a creature is put into an opponent's graveyard from the battlefield, if ~ is in your graveyard, exile ~. */

	if (event == EVENT_GRAVEYARD_ABILITY){
		return 0;
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance= get_card_instance(player, card);
		if(! in_play(affected_card_controller, affected_card) || count_graveyard_by_id(player, CARD_ID_BRIDGE_FROM_BELOW) < 1){
			return 0;
		}
			if( is_this_dying(affected_card_controller, affected_card) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
				if( instance->targets[11].player < 0 ){
					instance->targets[11].player = 0;
				}
				int owner = get_owner(affected_card_controller, affected_card);
				if( owner == player && ! is_token(affected_card_controller, affected_card)){
					SET_BYTE0(instance->targets[11].player)++;
				}
				else{
					SET_BYTE1(instance->targets[11].player)++;
				}
			}
	}

	int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( amount ){
		int zombies = BYTE0(amount);
		int kill_bridges = BYTE1(amount) ? 1 : 0;

		generate_tokens_by_id(player, card, CARD_ID_ZOMBIE, zombies);

		if( kill_bridges ){
			int count = count_graveyard(player)-1;
			while( count > -1 ){
					if( cards_data[get_grave(player)[count]].id == CARD_ID_BRIDGE_FROM_BELOW ){
						rfg_card_from_grave(player, count);
					}
					count--;
			}
		}
	}

	return 0;
}

// bound in silence --> pacifism

int card_centaur_omenreader(int player, int card, event_t event){
/* CARD_ID_CENTAUR_OMENREADER	8901
Centaur Omenreader	|3|G
Snow Creature - Centaur Shaman 3/3
As long as ~ is tapped, creature spells you cast cost |2 less to cast. */

	if( in_play(player, card) && is_tapped(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player ){
			if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
				COST_COLORLESS-=2;
			}
		}
	}

	return 0;
}

int card_chronomantic_escape(int player, int card, event_t event){

	/* Chronomantic Escape	|4|W|W
	 * Sorcery
	 * Until your next turn, creatures can't attack you. Exile ~ with three time counters on it.
	 * Suspend 3-|2|W */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		nobody_can_attack_until_your_next_turn(player, card, 1-player);
		suspend_a_card(player, card, player, card, 3);
	}

	return suspend(player, card, event, 3, MANACOST_XW(2, 1), NULL, NULL);
}

int card_cloud_key(int player, int card, event_t event){
/* CARD_ID_CLOUD_KEY	8904
Cloud Key	|3
Artifact
As ~ enters the battlefield, choose artifact, creature, enchantment, instant, or sorcery.
Spells you cast of the chosen type cost |1 less to cast. */

	if( event == EVENT_RESOLVE_SPELL ){
		int types[5] = {TYPE_CREATURE, TYPE_ENCHANTMENT, TYPE_SORCERY, (TYPE_INSTANT | TYPE_INTERRUPT), TYPE_ARTIFACT};
		int choice = do_dialog(player, player, card, -1, -1, " Creature\n Enchantment\n Sorcery\n Instant\n Artifact", internal_rand(5));
		get_card_instance(player, card)->targets[1].card = types[choice];
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card) ){
		int sel_type = get_card_instance(player, card)->targets[1].card;
		if( sel_type > -1 ){
			if( is_what(affected_card_controller, affected_card, sel_type) ){
				COST_COLORLESS--;
			}
		}
	}

	return 0;
}

int card_cloudseeder(int player, int card, event_t event){
	/* Cloudseeder	|1|U
	 * Creature - Faerie Spellshaper 1/1
	 * Flying
	 * |U, |T, Discard a card: Put a 1/1 |Sblue Faerie creature token named Cloud Sprite onto the battlefield.
	 * 		It has flying and "Cloud Sprite can block only creatures with flying." */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_CLOUD_SPRITE);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD, MANACOST_U(1), 0, NULL, NULL);
}

int card_coalition_relic(int player, int card, event_t event){
/* Coalition Relic	|3
 * Artifact
 * |T: Add one mana of any color to your mana pool.
 * |T: Put a charge counter on ~.
 * At the beginning of your precombat main phase, remove all charge counters from ~.
 * 		Add one mana of any color to your mana pool for each charge counter removed this way. */

	if( event == EVENT_PHASE_CHANGED && current_phase == PHASE_MAIN1 && get_card_instance(player, card)->targets[1].player != 1
		&& current_turn == player && in_play(player, card) && ! is_humiliated(player, card) )
	{
		get_card_instance(player, card)->targets[1].player = 1;	// prevent being asked multiple times
		int amount = count_counters(player, card, COUNTER_CHARGE);
		remove_counters(player, card, COUNTER_CHARGE, amount);
		produce_mana_any_combination_of_colors(player, COLOR_TEST_ANY_COLORED, amount, NULL);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_COUNTER
	};

	if( event == EVENT_ACTIVATE ){
		int can_add_counter = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		int choice = CHOICE_MANA;
		instance->info_slot = choice;
		if( ! paying_mana() && can_add_counter ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get Mana", 			1, 					5,
							"Add Charge counter", 	can_add_counter, 	10);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_COUNTER ){
			ai_modifier += (current_turn == 1-player && current_phase >= PHASE_MAIN2 ? 96 : -24);
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_COUNTER ){
			add_counter(instance->parent_controller, instance->parent_card, COUNTER_CHARGE);
		}
	}

	return 0;
}

int card_cryptic_anellid(int player, int card, event_t event){
/* CARD_ID_CRYPTIC_ANNELID	8907
Cryptic Annelid	|3|U
Creature - Worm Beast 1/4
When ~ enters the battlefield, scry 1, then scry 2, then scry 3.
(To scry X, look at the top X cards of your library, then put any number of them on the bottom of your library and the rest on top in any order.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int i;
		for(i = 1; i <= 3; i++){
			scry(player, i);
		}
	}

	return 0;
}

int card_dakmor_salvage(int player, int card, event_t event)
{
/* CARD_ID_DAKMOR_SALVAGE	8910
Dakmor Salvage	""
Land
~ enters the battlefield tapped.
|T: Add |B to your mana pool.
Dredge 2 (If you would draw a card, instead you may put exactly two cards from the top of your library into your graveyard.
If you do, return this card from your graveyard to your hand. Otherwise, draw a card.) */

	comes_into_play_tapped(player, card, event);

	if( event == EVENT_DREDGE ){
		return 2;
	}

	return mana_producer(player, card, event);
}

static const char* target_is_already_enchanted(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  return is_enchanted(player, card) ? NULL : "another Aura attached";
}
int card_daybreak_coronet(int player, int card, event_t event)
{
/* CARD_ID_DAYBREAK_CORONET	8912
Daybreak Coronet	|W|W
Enchantment - Aura
Enchant creature with another Aura attached to it
Enchanted creature gets +3/+3 and has first strike, vigilance, and lifelink. (Damage dealt by the creature also causes its controller to gain that much life.) */

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (int32_t)target_is_already_enchanted;

  card_instance_t* instance;
  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS)
	  && (instance = in_play(player, card))
	  && affect_me(instance->damage_target_player, instance->damage_target_card))
	event_result += 3;

  if (event == EVENT_ABILITIES
	  && (instance = in_play(player, card))
	  && affect_me(instance->damage_target_player, instance->damage_target_card))
	{
	  event_result |= KEYWORD_FIRST_STRIKE;
	  vigilance(instance->damage_target_player, instance->damage_target_card, event);
	  lifelink(instance->damage_target_player, instance->damage_target_card, event);
	}

  // Until aura target legality is handled centrally, though in this case it'll probably end up forwarding here anyway.
  if (event == EVENT_STATIC_EFFECTS
	  && (instance = in_play(player, card))
	  && instance->damage_target_player >= 0
	  && (instance->damage_target_card == -1
		  || count_auras_enchanting_me(instance->damage_target_player, instance->damage_target_card) < 2
		  || !is_what(instance->damage_target_player, instance->damage_target_card, TYPE_CREATURE)))
	kill_card(player, card, KILL_STATE_BASED_ACTION);

  return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

int card_deepcavern_imp(int player, int card, event_t event){
/* CARD_ID_DEEPCAVERN_IMP	8914
Deepcavern Imp	|2|B
Creature - Imp Rebel 2/2
Flying, haste
Echo-Discard a card. */

	haste(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		set_special_flags(player, card, SF_ECHO_TO_PAY);
	}

	if( check_special_flags(player, card, SF_ECHO_TO_PAY) && ! check_special_flags3(player, card, SF3_ECHO_PAID) && current_turn == player
	                                 && upkeep_trigger(player, card, event) ){
		set_special_flags3(player, card, SF3_ECHO_PAID);
	   if( count_upkeeps(player) > 0 ){
		int kill = 1;
		  if( hand_count[player] > 0 ){
			   int choice = do_dialog(player, player, card, -1, -1, " Pay Echo\n Pass", 0);
			    if( choice == 0 ){
				   if( ! check_battlefield_for_id(player, CARD_ID_THICK_SKINNED_GOBLIN) ){
				       discard(player, 0, player);
				   }
				    if( spell_fizzled != 1 ){
				        kill = 0;
			        }
			     }
		   }
		    if( kill == 1 ){
		        kill_card(player, card, KILL_SACRIFICE);
		    }
	     }
	}
	return 0;
}

int card_death_rattle(int player, int card, event_t event){
/* CARD_ID_DEATH_RATTLE	8913
Death Rattle	|5|B
Instant
Delve (Each card you exile from your graveyard while casting this spell pays for |1.)
Destroy target non|Sgreen creature. It can't be regenerated. */

	modify_cost_for_delve(player, card, event);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card)  ){
		int result = 1;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1){
			result = cast_spell_with_delve(player, card);
            if( ! result ){
				spell_fizzled = 1;
				return 0;
			}
		}
        td.allow_cancel = (result == 2 ? 0 : 1);
		instance->number_of_targets = 0;
        generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
								get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_GREEN), 1, NULL);
		}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_delay(int player, int card, event_t event){
/* CARD_ID_DELAY	8915
Delay	|1|U
Instant
Counter target spell. If the spell is countered this way, exile it with three time counters on it instead of putting it into its owner's graveyard.
If it doesn't have suspend, it gains suspend. (At the beginning of its owner's upkeep, remove a time counter from that card.
When the last is removed, the player plays it without paying its mana cost. If it's a creature, it has haste.) */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return counterspell(player, card, event, NULL, 0);
	}
	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			int result = manage_counterspell_linked_hacks(player, card, instance->targets[0].player, instance->targets[0].card);
			if( result != KILL_EXILE ){
				suspend_a_card(player, card, instance->targets[0].player, instance->targets[0].card, 3);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_dryad_arbor(int player, int card, event_t event){
/* CARD_ID_DRYAD_ARBOR	8916
Dryad Arbor	""
Land Creature - |H2Forest Dryad 1/1
(~ isn't a spell, it's affected by summoning sickness, and it has "|T: Add |H|G to your mana pool.") */

	return mana_producer_fixed(player, card, event, COLOR_GREEN);
}

int card_edge_of_autumn(int player, int card, event_t event){
/* Edge of Autumn	|1|G
 * Sorcery
 * If you control four or fewer lands, search your library for a basic land card, put it onto the battlefield tapped,
 *		then shuffle your library.
 * Cycling-Sacrifice a land. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( count_permanents_by_type(player, TYPE_LAND) >= 5 ){
			ai_modifier-=500;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( count_permanents_by_type(player, TYPE_LAND) < 5 ){
			tutor_basic_land(player, 1, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return cycling_special_cost(player, card, event, 1);
}

int card_emberwilde_augur(int player, int card, event_t event){
	/* Emberwilde Augur	|1|R	0x2007320
	 * Creature - Goblin Shaman 2/1
	 * Sacrifice ~: It deals 3 damage to target player or planeswalker. Activate this ability only during your upkeep. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 3);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_ONLY_ON_UPKEEP | GAA_IN_YOUR_TURN | GAA_CAN_TARGET,
									MANACOST0, 0, &td, "TARGET_PLAYER_OR_PLANESWALKER");
}

int card_emblem_of_the_warmind(int player, int card, event_t event)
{
/* CARD_ID_EMBLEM_OF_THE_WARMIND	8920
Emblem of the Warmind	|1|R
Enchantment - Aura
Enchant creature you control
Creatures you control have haste. */

     boost_subtype(player, card, event, -1, 0, 0, 0, SP_KEYWORD_HASTE, BCT_CONTROLLER_ONLY);

     return generic_aura(player, card, event, player, 0, 0, 0, 0, 0, 0, 0);// generic_aura() contains a bletcherous hack for "Enchant creature you control" ability and text
}

int card_epochrasite(int player, int card, event_t event){

	/* Epochrasite	|2
	 * Artifact Creature - Construct 1/1
	 * ~ enters the battlefield with three +1/+1 counters on it if you didn't cast it from your hand.
	 * When ~ dies, exile it with three time counters on it and it gains suspend. */

	if (not_played_from_hand(player, card)){
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( affect_me(player, card) && is_this_dying(player, card) ){
			suspend_a_card(player, card, player, card, 3);
			get_card_instance(player, card)->kill_code = KILL_EXILE;
		}
	}

	if (event == EVENT_TOU_BOOST){
		return 99;
	}

	return 0;
}

int card_festering_march(int player, int card, event_t event){

	/* Festering March	|3|B|B
	 * Sorcery
	 * Creatures your opponents control get -1/-1 until end of turn. Exile ~ with three time counters on it.
	 * Suspend 3-|2|B */

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, 1-player, -1, -1, -1, 0, 0);
		suspend_a_card(player, card, player, card, 3);
	}

	return suspend(player, card, event, 3, MANACOST_XB(2, 1), NULL, NULL);
}

int card_fleshwrither(int player, int card, event_t event){
/* CARD_ID_FLESHWRITHER	8925
Fleshwrither	|2|B|B
Creature - Horror 3/3
Transfigure |1|B|B (|1|B|B, Sacrifice this creature: Search your library for a creature card with the same converted mana cost
as this creature and put that card onto the battlefield. Then shuffle your library. Transfigure only as a sorcery.) */

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XB(1, 2), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		get_card_instance(player, card)->info_slot = get_cmc(player, card);
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XB(1, 2), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int cmc = get_card_instance(player, card)->info_slot;

		char buffer[100];
		scnprintf(buffer, 100, "Select a creature card with CMC %d", cmc);
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, buffer);
		this_test.cmc = cmc;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
	}

	return 0;
}

int card_foresee(int player, int card, event_t event){
/* CARD_ID_FORESEE	8929
Foresee	|3|U
Sorcery
Scry 4, then draw two cards. (To scry 4, look at the top four cards of your library, then put any number of them on the bottom of your library
and the rest on top in any order.) */

	if( event == EVENT_RESOLVE_SPELL ){
		scry(player, 4);
		draw_cards (player, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_frenzy_sliver(int player, int card, event_t event){
/* CARD_ID_FRENZY_SLIVER	8930
Frenzy Sliver	|1|B
Creature - Sliver 1/1
All Sliver creatures have frenzy 1. (Whenever a Sliver attacks and isn't blocked, it gets +1/+0 until end of turn.) */

	if( event == EVENT_DECLARE_BLOCKERS && ! is_humiliated(player, card) ){
		int count = active_cards_count[current_turn]-1;
		while( count > -1 ){
			if( in_play(current_turn, count) && has_subtype(current_turn, count, SUBTYPE_SLIVER) && is_attacking(current_turn, count) &&
				is_unblocked(current_turn, count)
			  ){
				pump_until_eot(player, card, current_turn, count, 1, 0);
			}
			count--;
		}
	}
	return slivercycling(player, card, event);
}

int card_gathan_raiders(int player, int card, event_t event){
/* CARD_ID_GATHAN_RAIDERS	8931
Gathan Raiders	|3|R|R
Creature - Human Warrior 3/3
Hellbent - ~ gets +2/+2 as long as you have no cards in hand.
Morph-Discard a card. (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! hand_count[player] ){
			event_result += 2;
		}
	}

	if( event == EVENT_CAN_UNMORPH ){
		if( hand_count[player] > 0 ){
			return 1;
		}
	}

	if( event == EVENT_UNMORPH ){
		discard(player, 0, player);
	}

	return morph(player, card, event, MANACOST0);
}

int card_gibbering_descend(int player, int card, event_t event){
/* CARD_ID_GIBBERING_DESCENT	8932
Gibbering Descent	|4|B|B
Enchantment
At the beginning of each player's upkeep, that player loses 1 life and discards a card.
Hellbent - Skip your upkeep step if you have no cards in hand.
Madness |2|B|B (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.) */

	if( event == EVENT_CAN_CAST || event == EVENT_SHOULD_AI_PLAY ){
		return global_enchantment(player, card, event);
	}

	upkeep_trigger_ability(player, card, event, 2);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		lose_life(current_turn, 1);
		discard(current_turn, 0, player);
	}

	return madness(player, card, event, MANACOST_XB(2, 2));
}

int card_glittering_wish(int player, int card, event_t event){
/* CARD_ID_GLITTERING_WISH	8934
Glittering Wish	|G|W
Sorcery
You may choose a multicolored card you own from outside the game, reveal that card, and put it into your hand. Exile ~. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( player == AI || ai_is_speculating == 1){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			this_test.color_flag = F3_MULTICOLORED;
			int id = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MAX_VALUE, -1, &this_test);
			if( id > -1 ){
				int iid = deck_ptr[player][id];
				update_rules_engine(check_card_for_rules_engine(iid));
				int card_added = add_card_to_hand(player, iid);
				reveal_card(player, card, player, card_added);
			}
		}
		else{
			int crd = card_from_list(player, 4, TYPE_ANY, 0, 0, 0, 0, F3_MULTICOLORED, 0, 0, -1, 0);
			if( crd != -1 ){
				update_rules_engine(check_card_for_rules_engine(crd));
				add_card_to_hand(player, crd);
			}
		}
		kill_card(player, card, KILL_EXILE);
	}

	return basic_spell(player, card, event);
}

int card_goldmeadow_lookout(int player, int card, event_t event){
	/* Goldmeadow Lookout	|3|W
	 * Creature - Kithkin Spellshaper 2/2
	 * |W, |T, Discard a card: Put a 1/1 |Swhite Kithkin Soldier creature token named Goldmeadow Harrier onto the battlefield.
	 *		It has "|W, |T: Tap target creature." */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_GOLDMEADOW_HARRIER);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD, MANACOST_W(1), 0, NULL, NULL);
}

static int grave_peril_legacy(int player, int card, event_t event){


	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance( player, card);

		int p = instance->targets[0].player;
		int c = instance->targets[0].card;

		if( affect_me(p, c) && get_card_instance(p, c)->kill_code == KILL_SACRIFICE ){
			instance->targets[11].player = 66;
		}
	}

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance( player, card);
		if( in_play(instance->targets[1].player, instance->targets[1].card) ){
			kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_grave_peril(int player, int card, event_t event){
/*
Grave Peril |1|B
Enchantment
When a nonblack creature enters the battlefield, sacrifice Grave Peril. If you do, destroy that creature.
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		test.color = DOESNT_MATCH;
		if( new_specific_cip(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &test) ){
			if( ! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) ){
				int legacy = create_legacy_effect(player, card, &grave_peril_legacy);
				card_instance_t *instance = get_card_instance( player, legacy );
				instance->targets[0].player = player;
				instance->targets[0].card = card;
				instance->targets[1].player = trigger_cause_controller;
				instance->targets[1].card = trigger_cause;
				instance->number_of_targets = 1;
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_grinning_ignus(int player, int card, event_t event){
/* CARD_ID_GRINNING_IGNUS	8939
Grinning Ignus	|2|R
Creature - Elemental 2/2
|R, Return ~ to its owner's hand: Add |C|C|R to your mana pool. Activate this ability only any time you could cast a sorcery. */

	if( event == EVENT_COUNT_MANA ){
		if( affect_me(player, card) && can_sorcery_be_played(player, event) && can_produce_mana(player, card) ){
			if( has_mana_for_activated_ability(player, card, MANACOST_R(1)) ){
				declare_mana_available(player, COLOR_COLORLESS, 2);
			}
		}
	}

	if( event == EVENT_CAN_ACTIVATE && has_mana_for_activated_ability(player, card, MANACOST_R(1)) &&
		can_sorcery_be_played(player, event) && can_produce_mana(player, card) )
	{
		return 1;
	}

	if( event == EVENT_ACTIVATE ){
		charge_mana_for_activated_ability(player, card, MANACOST_R(1));
		if( spell_fizzled != 1 ){
			produce_mana_multi(player, MANACOST_XR(2, 1));
			bounce_permanent(player, card);
		}
	}

	return 0;
}

int card_grove_of_the_burnwillows(int player, int card, event_t event){
/* CARD_ID_GROVE_OF_THE_BURNWILLOWS	8940
Grove of the Burnwillows	""
Land
|T: Add |C to your mana pool.
|T: Add |R or |G to your mana pool. Each opponent gains 1 life. */

	if( event == EVENT_ACTIVATE ){
		int red_mana_before = mana_pool[player][COLOR_RED];
		int green_mana_before = mana_pool[player][COLOR_GREEN];
		mana_producer(player, card, event);
		if( red_mana_before < mana_pool[player][COLOR_RED] || green_mana_before < mana_pool[player][COLOR_GREEN] ){
			gain_life(1-player, 1);
		}
	}
	else{
		return mana_producer(player, card, event);
	}
	return 0;
}

int card_haze_of_rage(int player, int card, event_t event){
/* CARD_ID_HAZE_OF_RAGE	8941
Haze of Rage	|1|R
Sorcery
Buyback |2 (You may pay an additional |2 as you cast this spell. If you do, put this card into your hand as it resolves.)
Creatures you control get +1/+0 until end of turn.
Storm (When you cast this spell, copy it for each spell cast before it this turn.) */

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		instance->info_slot = buyback(player, card, MANACOST_X(2));
		if( ! check_special_flags(player, card, SF_NOT_CAST) ){
			int i;
			for(i=0; i < get_storm_count(); i++){
				pump_subtype_until_eot(player, card, player, -1, 1, 0, 0, 0);
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card );
		pump_subtype_until_eot(player, card, player, -1, 1, 0, 0, 0);
		if( instance->info_slot == 1){
			  bounce_permanent(player, card);
		}
		else{
			 kill_card(player, card, KILL_DESTROY);
		}
	}

	return basic_spell(player, card, event);
}

int card_heartwood_storyteller(int player, int card, event_t event){
/* CARD_ID_HEARTWOOD_STORYTELLER	8942
Heartwood Storyteller	|1|G|G
Creature - Treefolk 2/3
Whenever a player casts a noncreature spell, each of that player's opponents may draw a card. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.type_flag = DOESNT_MATCH;
		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_CHECK_DECK_COUNT(1-player, 1), &test) ){
			draw_cards(1-trigger_cause_controller, 1);
		}
	}

	return 0;
}

#pragma message "Probably needs its own PLAYER_BITS"
int card_homing_sliver(int player, int card, event_t event){
/* CARD_ID_HOMING_SLIVER	8944
Homing Sliver	|2|R
Creature - Sliver 2/2
Each Sliver card in each player's hand has slivercycling |3.
Slivercycling |3 (|3, Discard this card: Search your library for a Sliver card, reveal it, and put it into your hand. Then shuffle your library.) */

	if( in_play(player, card) && event == EVENT_CHANGE_TYPE && affect_me(player, card) && ! is_humiliated(player, card) ){
		set_trap_condition(player, TRAP_HOMING_SLIVER, 1);
	}
	if( leaves_play(player, card, event) ){
		set_trap_condition(player, TRAP_HOMING_SLIVER, 0);
	}
	return slivercycling(player, card, event);
}

int card_horizon_canopy(int player, int card, event_t event){
/* CARD_ID_HORIZON_CANOPY	8945
Horizon Canopy	""
Land
|T, Pay 1 life: Add |G or |W to your mana pool.
|1, |T, Sacrifice ~: Draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( mana_producer(player, card, event) && can_pay_life(player, 1) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL);
	}
	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		if( mana_producer(player, card, EVENT_CAN_ACTIVATE) && can_pay_life(player, 1) ){
			if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(2), 0, NULL, NULL) ){
				choice = do_dialog(player, player, card, -1, -1, " Add G or W\n Sac and draw a card\n Cancel", 1);
			}
		}
		else{
			choice = 1;
		}

		if( choice == 0){
			mana_producer(player, card, event);
			if( spell_fizzled != 1 ){
				lose_life(player, 1);
			}
		}
		else if( choice == 1 ){
				add_state(player, card, STATE_TAPPED);
				if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
					instance->info_slot = 1;
					kill_card(player, card, KILL_SACRIFICE);
				}
				else{
					remove_state(player, card, STATE_TAPPED);
				}
		}
		else if( choice == 2 ){
				spell_fizzled = 1;
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION && instance->info_slot == 1 ){
		draw_a_card(player);
	}

	return 0;
}

int card_ichor_slick(int player, int card, event_t event){
	/* Ichor Slick	|2|B
	 * Sorcery
	 * Target creature gets -3/-3 until end of turn.
	 * Cycling |2
	 * Madness |3|B */

	if( event == EVENT_CAN_PAY_MADNESS_COST || event == EVENT_PAY_MADNESS_COST ){
		return madness(player, card, event, MANACOST_XB(3, 1));
	}

	if (event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND || event == EVENT_RESOLVE_ACTIVATION_FROM_HAND ){
		return cycling(player, card, event, MANACOST_X(2));
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance( player, card );
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -3, -3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_imperial_mask(int player, int card, event_t event){
/* CARD_ID_IMPERIAL_MASK	8947
Imperial Mask	|4|W
Enchantment
When ~ enters the battlefield, if it's not a token, each of your teammates creates a token that's a copy of ~.
You have hexproof. (You can't be the target of spells or abilities your opponents control.) */
   if( in_play(player, card) && !is_humiliated(player, card) ){
	   give_hexproof_to_player(player, card, event);
   }

	return global_enchantment(player, card, event);
}

int card_intervention_pact(int player, int card, event_t event){
/* CARD_ID_INTERVENTION_PACT	8950
Intervention Pact	|0
Instant
The next time a source of your choice would deal damage to you this turn, prevent that damage. You gain life equal to the damage prevented this way.
At the beginning of your next upkeep, pay |1|W|W. If you don't, you lose the game. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.extra = damage_card;
	td1.required_type = 0;
	td1.special = TARGET_SPECIAL_DAMAGE_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player == AI && ! has_virtual_mana_multi(player, MANACOST_XW(1, 2)) ){
			ai_modifier -= 1500;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			int amount = target->info_slot;
			target->info_slot = 0;
			gain_life(player, amount);
			create_pact_legacy(player, card, player, MANACOST_XW(1, 2));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td1, "TARGET_DAMAGE", 1, NULL);
}

int card_jhoira_of_the_ghitu(int player, int card, event_t event){
/* Jhoira of the Ghitu	|1|U|R
 * Legendary Creature - Human Wizard 2/2
 * |2, Exile a nonland card from your hand: Put four time counters on the exiled card. If it doesn't have suspend, it gains suspend. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_LAND, "Select a nonland card to Suspend with four time counters.");
	this_test.type_flag = DOESNT_MATCH;
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL) ){
			return check_battlefield_for_special_card(player, card, player, 0, &this_test);
		}
	}

	if( event == EVENT_ACTIVATE){
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
			if( selected != -1 ){
				instance->info_slot = get_original_internal_card_id(player, selected);
				ai_modifier += (5*get_cmc(player, selected));
				rfg_card_in_hand(player, selected);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		suspend_a_card(instance->parent_controller, instance->parent_card, -1, instance->info_slot, 4);
	}

	return 0;
}

int card_keldon_megaliths(int player, int card, event_t event){
	/* Keldon Megaliths	""	0x2007361
	 * Land
	 * ~ enters the battlefield tapped.
	 * |T: Add |R.
	 * Hellbent - |1|R, |T: ~ deals 1 damage to any target. Activate this ability only if you have no cards in hand. */
	comes_into_play_tapped(player, card, event);

	if( event == EVENT_CAN_ACTIVATE || ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance( player, card );

	comes_into_play_tapped(player, card, event);

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int choice = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XR(2, 1), 0, &td, NULL) &&
			hand_count[player] < 1
		  ){
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Damage creature or player\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		else if( choice == 1 ){
				instance->number_of_targets = 0;
				add_state(player, card, STATE_TAPPED);
				generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, "TARGET_ANY");
				if( spell_fizzled != 1 ){
					instance->info_slot = 1;
				}
				else{
					remove_state(player, card, STATE_TAPPED);
				}
		}

		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
			if( valid_target(&td) ){
				damage_target0(player, card, 1);
			}
		}
	}

	return 0;
}

int card_knight_of_sursi(int player, int card, event_t event){
	/* Knight of Sursi	|3|W
	 * Creature - Human Knight 2/2
	 * Flying; flanking
	 * Suspend 3-|W */
	flanking(player, card, event);
	return suspend(player, card, event, 3, MANACOST_W(1), NULL, NULL);
}

int card_korlash_heir_to_blackblade(int player, int card, event_t event){
	/* Korlash, Heir to Blackblade	|2|B|B
	 * Legendary Creature - Zombie Warrior 100/100
	 * ~'s power and toughness are each equal to the number of |H1Swamps you control.
	 * |1|B: Regenerate Korlash.
	 * Grandeur - Discard another card named ~: Search your library for up to two |H2Swamp cards, put them onto the battlefield tapped, then shuffle your library. */

	check_legend_rule(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		if( regeneration(player, card, event, MANACOST_XB(1, 1)) ){
			return 99;
		}
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) && is_id_in_hand(player, get_id(player, card)) ){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		get_card_instance(player, card)->info_slot = 0;
		if( land_can_be_played & LCBP_REGENERATION ){
			return regeneration(player, card, event, MANACOST_XB(1, 1));
		}
		else{
			if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
				discard_id_from_hand(player, get_id(player, card));
				get_card_instance(player, card)->info_slot = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t* instance = get_card_instance(player, card);
		if( instance->info_slot > 0 ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, get_hacked_land_text(player, card, "Select %a card.", SUBTYPE_SWAMP));
			this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_SWAMP);
			this_test.qty = 2;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY_TAPPED, 0, AI_MAX_VALUE, &this_test);
		}
		else{
			if( can_regenerate(player, instance->parent_card) ){
				regenerate_target(player, instance->parent_card);
			}
		}
	}

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && !is_humiliated(player, card) && player != -1){
		event_result += count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_SWAMP));
	}
	return 0;
}

static int target_for_lynessa(int player, int card, int mode){
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance= get_card_instance(player, card);

	int i;
	int trg = -1;
	int par = -1;
	for(i=0; i<2; i++){
		if( i == 1-player || player == HUMAN ){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) ){
						if( has_mana_for_activated_ability(player, card, MANACOST_XU(get_cmc(i, count), 2)) ){
							instance->targets[0].player = i;
							instance->targets[0].card = count;
							instance->number_of_targets = 1;
							if( would_validate_target(player, card, &td, 0) ){
								if( mode == 0 || player == HUMAN ){
									return 1;
								}
								if( mode == 1 && get_base_value(i, count) > par ){
									par = get_base_value(i, count);
									trg = count;
								}
							}
						}
					}
					count++;
			}
		}
	}
	if( mode == 1 ){
		return trg;
	}
	return 0;
}

int card_lynessa_zephyr_mage(int player, int card, event_t event){
	/*
	  Linessa, Zephyr Mage |3|U

	  Legendary Creature - Human Wizard 3/3

	  {X}{U}{U}, {T}: Return target creature with converted mana cost X to its owner's hand.

	  Grandeur - Discard another card named Linessa, Zephyr Mage: Target player returns a creature he or she controls to its owner's hand, then repeats this process for an artifact, an enchantment, and a land.
	*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE );
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	check_legend_rule(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) && target_for_lynessa(player, card, 0) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td1, NULL) && is_id_in_hand(player, get_id(player, card))){
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		int abilities[2] = {	generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) && target_for_lynessa(player, card, 0),
								generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST0, 0, &td1, NULL) && is_id_in_hand(player, get_id(player, card))
		};

		int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
							"Bounce a creature", abilities[0], 5,
							"Grandeur's Hyperbounce", abilities[1], 10);
		if( ! choice  ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == 1 ){
			instance->number_of_targets = 0;
			if( player == HUMAN ){
				pick_target(&td, "TARGET_CREATURE");
			}
			else{
				int result = target_for_lynessa(player, card, 1);
				if( result != -1 ){
					instance->targets[0].player = player;
					instance->targets[0].card = result;
					instance->number_of_targets = 1;
				}
			}
			if( spell_fizzled != 1 ){
				if( charge_mana_for_activated_ability(player, card, MANACOST_XU(get_cmc(instance->targets[0].player, instance->targets[0].card), 2)) ){
					instance->info_slot = 1;
					tap_card(player, card);
				}
			}
		}

		if( choice == 2 ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td1, "TARGET_PLAYER");
			if( spell_fizzled != 1 ){
				discard_id_from_hand(player, get_id(player, card));
				instance->info_slot = 2;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 && valid_target(&td) ){
			bounce_permanent( instance->targets[0].player, instance->targets[0].card );
		}
		if( instance->info_slot == 2 && valid_target(&td1) ){
			int i;
			for(i=0; i<4; i++){
				int type = 0;
				if( i < 3 ){
					type = 1<<i;
				}
				else{
					type = TYPE_ARTIFACT;
				}
				target_definition_t td2;
				default_target_definition(player, card, &td2, type );
				td2.allowed_controller = instance->targets[0].player;
				td2.preferred_controller = instance->targets[0].player;
				td2.illegal_abilities = 0;
				td2.allow_cancel = 0;
				td2.who_chooses = instance->targets[0].player;
				char buffer[500];
				int pos = 0;
				pos += scnprintf(buffer + pos, 500-pos, " Select a", buffer);
				if( i == 0 ){
					pos += scnprintf(buffer + pos, 500-pos, " Land ", buffer);
				}
				if( i == 1 ){
					pos += scnprintf(buffer + pos, 500-pos, " Creature ", buffer);
				}
				if( i == 2 ){
					pos += scnprintf(buffer + pos, 500-pos, "n Enchantment ", buffer);
				}
				if( i == 3 ){
					pos += scnprintf(buffer + pos, 500-pos, "n Artifact ", buffer);
				}
				pos += scnprintf(buffer + pos, 500-pos, "to bounce.", buffer);
				if( can_target(&td2) && select_target(player, card, &td2, buffer , &(instance->targets[1])) ){
					instance->number_of_targets = 1;
					bounce_permanent( instance->targets[1].player, instance->targets[1].card );
				}
			}
		}
	}

	return 0;
}

int card_llanowar_empath(int player, int card, event_t event){
/* CARD_ID_LLANOWAR_EMPATH	8960
Llanowar Empath	|3|G
Creature - Elf Shaman 2/2
When ~ enters the battlefield, scry 2, then reveal the top card of your library. If it's a creature card, put it into your hand.
(To scry 2, look at the top two cards of your library, then put any number of them on the bottom of your library and the rest on top in any order.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( deck_ptr[player][0] != -1 ){
			scry(player, 2);
			int iid = deck_ptr[player][0];
			reveal_card_iid(player, card, iid);
			if( is_what(-1, iid, TYPE_CREATURE)  ){
				add_card_to_hand(player, iid);
				remove_card_from_deck(player, 0);
			}
		}
	}
	return 0;
}

int card_llanowar_mentor(int player, int card, event_t event){
	/* Llanowar Mentor	|G
	 * Creature - Elf Spellshaper 1/1
	 * |G, |T, Discard a card: Put a 1/1 |Sgreen Elf Druid creature token named Llanowar Elves onto the battlefield. It has "|T: Add |G to your mana pool." */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_LLANOWAR_ELVES);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD, MANACOST_G(1), 0, NULL, NULL);
}

int card_llanowar_reborn(int player, int card, event_t event){
/* CARD_ID_LLANOWAR_REBORN	8962
Llanowar Reborn	""
Land
~ enters the battlefield tapped.
|T: Add |G to your mana pool.
Graft 1 (This land enters the battlefield with a +1/+1 counter on it.
Whenever a creature enters the battlefield, you may move a +1/+1 counter from this land onto it.) */

	comes_into_play_tapped(player, card, event);

	graft(player, card, event, 1);

	return mana_producer(player, card, event);
}

int card_logic_knot(int player, int card, event_t event){
	/* Logic Knot	|X|U|U
	 * Instant
	 * Delve
	 * Counter target spell unless its controller pays |X. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_COUNTERSPELL | GS_X_SPELL, NULL, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		counterspell(player, card, event, NULL, 0);
		charge_mana(player, COLOR_COLORLESS, -1);
		if( spell_fizzled != 1 ){
			int amount = x_value;
			if( do_dialog(player, player, card, -1, -1, " Use Delve\n Pass", 0) == 0 ){
				int cg = count_graveyard(player);
				int max = 0;
				test_definition_t this_test2;
				new_default_test_definition(&this_test2, 0, "Select a card to exile.");
				while( max < cg ){
						if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &this_test2) != -1 ){
							max++;
						}
						else{
							break;
						}
				}
				amount+=max;
			}
			set_x_for_x_spells(player, card, event, amount);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, instance->info_slot);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_lost_auramancers(int player, int card, event_t event){

	/* Lost Auramancers	|2|W|W
	 * Creature - Human Wizard 3/3
	 * Vanishing 3
	 * When ~ dies, if it had no time counters on it, you may search your library for an enchantment card and put it onto the battlefield. If you do, shuffle
	 * your library. */

	vanishing(player, card, event, 3);

	if( count_counters(player, card, COUNTER_TIME) == 0 && this_dies_trigger(player, card, event, RESOLVE_TRIGGER_OPTIONAL) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ENCHANTMENT, "Select an enchantment card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
	}

	return 0;
}

int card_lumithread_field(int player, int card, event_t event){
/* CARD_ID_LUMITHREAD_FIELD	8967
Lumithread Field	|1|W
Enchantment
Creatures you control get +0/+1.
Morph |1|W (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.) */

	boost_creature_type(player, card, event, -1, 0, 1, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);

	if( event == EVENT_SHOULD_AI_PLAY ){
		return should_ai_play(player, card);
	}

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	return morph(player, card, event, MANACOST_XW(1, 1));
}

int card_lymph_sliver(int player, int card, event_t event){
/* CARD_ID_LYMPH_SLIVER	8968
Lymph Sliver	|4|W
Creature - Sliver 3/3
All Sliver creatures have absorb 1. (If a source would deal damage to a Sliver, prevent 1 of that damage.) */

	if( event == EVENT_PREVENT_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card );
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card != -1 && has_subtype(damage->damage_target_player, damage->damage_target_card, SUBTYPE_SLIVER) &&
				damage->info_slot > 0 )
			{
				prevent_damage(affected_card_controller, affected_card, 1);
			}
		}
	}
	return slivercycling(player, card, event);
}

int card_maelstrom_djinn(int player, int card, event_t event){

	/* Maelstrom Djinn	|7|U
	 * Creature - Djinn 5/6
	 * Flying
	 * Morph |2|U
	 * When ~ is turned face up, put two time counters on it and it gains vanishing. */

	card_instance_t *instance = get_card_instance(player, card);
	if( instance->targets[1].player == 66 ){
		vanishing(player, card, event, 0);
	}
	if( event == EVENT_TURNED_FACE_UP ){
		instance->targets[1].player = 66;

		// Still face-down until the next EVENT_CHANGE_TYPE, so temporarily turn it face up long enough to get time counters in the normal slot
		int current_iid = instance->internal_card_id;
		instance->internal_card_id = get_internal_card_id_from_csv_id(CARD_ID_MAELSTROM_DJINN);
		add_counters(player, card, COUNTER_TIME, 2);
		instance->internal_card_id = current_iid;

		instance->targets[9].card = 66;
	}
	return morph(player, card, event, MANACOST_XU(2, 1));
}

int card_magus_of_the_abyss(int player, int card, event_t event)
{
  /* Magus of the Abyss	|3|B
   * Creature - Human Wizard 4/3
   * At the beginning of each player's upkeep, destroy target nonartifact creature that player controls of his or her choice. It can't be regenerated. */

  abyss_trigger(player, card, event);
  return 0;
}

// magus of the future --> future sight

int card_magus_of_the_vineyard(int player, int card, event_t event){
/* CARD_ID_MAGUS_OF_THE_VINEYARD	8971
Magus of the Vineyard	|G
Creature - Human Wizard 1/1
At the beginning of each player's precombat main phase, add |G|G to that player's mana pool. */

  vineyard_effect(player, card, event);

  return 0;
}

int card_marshaling_cry(int player, int card, event_t event){
	/* Marshaling Cry	|1|W|W
	 * Sorcery
	 * Creatures you control get +1/+1 and gain vigilance until end of turn.
	 * Cycling |2
	 * Flashback |3|W */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 1, 1, 0, SP_KEYWORD_VIGILANCE);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND || event == EVENT_RESOLVE_ACTIVATION_FROM_HAND ){
		return cycling(player, card, event, MANACOST_X(2));
	}

	return do_flashback(player, card, event, MANACOST_XW(3, 1));
}

int card_mesmeric_sliver(int player, int card, event_t event)
{
/*
Mesmeric Sliver |3|U
Creature — Sliver 2/2, 3U (4)
All Slivers have "When this permanent enters the battlefield, you may fateseal 1."
(To fateseal 1, its controller looks at the top card of an opponent's library, then he or she may put that card on the bottom
of that library.)
*/
	if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) )
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_SLIVER;

		// Mandatory, since it's harmless; and otherwise, it would be the Mesmeric Sliver's controller who chose whether to activate
		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		fateseal(trigger_cause_controller, 1);
	}

	return slivercycling(player, card, event);
}

int card_minions_murmurs(int player, int card, event_t event){
/* CARD_ID_MINIONS_MURMURS	8975
Minions' Murmurs	|2|B|B
Sorcery
You draw X cards and you lose X life, where X is the number of creatures you control. */

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = count_subtype(player, TYPE_CREATURE, -1);
		draw_cards(player, amount);
		lose_life(player, amount);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_molten_disaster(int player, int card, event_t event){
/* CARD_ID_MOLTEN_DISASTER	8977
Molten Disaster	|X|R|R
Sorcery
Kicker |R (You may pay an additional |R as you cast this spell.)
If ~ was kicked, it has split second. (As long as this spell is on the stack, players can't cast spells or activate abilities that aren't mana abilities.)
~ deals X damage to each creature without flying and each player. */

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
		if( ! is_token(player, card) ){
			do_kicker(player, card, MANACOST_R(1));
		}
		if( kicked(player, card) ){
			state_untargettable(player, card, 1);
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.keyword = KEYWORD_FLYING;
			this_test.keyword_flag = 1;
			new_damage_all(player, card, ANYBODY, get_card_instance(player, card)->info_slot, NDA_PLAYER_TOO, &this_test);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( !kicked(player, card) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.keyword = KEYWORD_FLYING;
			this_test.keyword_flag = 1;
			new_damage_all(player, card, ANYBODY, get_card_instance(player, card)->info_slot, NDA_PLAYER_TOO, &this_test);
		}
		state_untargettable(player, card, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_mystic_speculation(int player, int card, event_t event){
/* CARD_ID_MYSTIC_SPECULATION	8979
Mystic Speculation	|U
Sorcery
Buyback |2 (You may pay an additional |2 as you cast this spell. If you do, put this card into your hand as it resolves.)
Scry 3. (Look at the top three cards of your library, then put any number of them on the bottom of your library and the rest on top in any order.)*/

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->info_slot = buyback(player, card, MANACOST_X(2));
	}

	if( event == EVENT_RESOLVE_SPELL ){
		scry(player, 3);
		if( get_card_instance(player, card)->info_slot == 1 ){
			bounce_permanent(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_new_benalia(int player, int card, event_t event)
{
  /* New Benalia	""
   * Land
   * ~ enters the battlefield tapped.
   * When ~ enters the battlefield, scry 1.
   * |T: Add |W to your mana pool. */

	comes_into_play_tapped(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
		scry(player, 1);

	return mana_producer(player, card, event);
}

int card_nihilith(int player, int card, event_t event){
	/* Nihilith	|4|B|B
	 * Creature - Horror 4/4
	 * Fear
	 * Suspend 7-|1|B
	 * Whenever a card is put into an opponent's graveyard from anywhere, if ~ is suspended, you may remove a time counter from ~. */
	fear(player, card, event);
	return suspend(player, card, event, 7, MANACOST_XB(1, 1), NULL, NULL);
}

int card_nimbus_maze(int player, int card, event_t event)
{
/* CARD_ID_NIMBUS_MAZE	8984
Nimbus Maze	""
Land
|T: Add |C to your mana pool.
|T: Add |W to your mana pool. Activate this ability only if you control |Han Island.
|T: Add |U to your mana pool. Activate this ability only if you control |Ha Plains. */


	if ((event == EVENT_CHANGE_TYPE && affect_me(player, card) && in_play(player, card))
	  || event == EVENT_RESOLVE_SPELL)	// mostly so the right land sound plays
	{
		card_instance_t* instance = get_card_instance(player, card);
	  instance->info_slot = COLOR_TEST_COLORLESS;

	  int plains = get_hacked_subtype(player, card, SUBTYPE_PLAINS);
	  int island = get_hacked_subtype(player, card, SUBTYPE_ISLAND);

	  int c;
	  for (c = 0; c < active_cards_count[player]; ++c)
		if (in_play(player, c) && is_what(player, c, TYPE_LAND))
		  {
			if (has_subtype(player, c, plains))
			  instance->info_slot |= COLOR_TEST_BLUE;

			if (has_subtype(player, c, island))
			  instance->info_slot |= COLOR_TEST_WHITE;

			if (instance->info_slot == (COLOR_TEST_COLORLESS | COLOR_TEST_BLUE | COLOR_TEST_WHITE))
			  break;
		  }
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
	  play_land_sound_effect_force_color(player, card, get_card_instance(player, card)->info_slot);
	  return 0;	// so mana_producer() doesn't play the colorless sound
	}

	return mana_producer(player, card, event);
}

int card_oriss_samite_guardian(int player, int card, event_t event){
	/*
	  Oriss, Samite Guardian |1|W|W
	  Legendary Creature - Human Cleric 1/3
	  {T}: Prevent all damage that would be dealt to target creature this turn.
	  Grandeur - Discard another card named Oriss, Samite Guardian: Target player can't cast spells this turn,
	  and creatures that player controls can't attack this turn.
	*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0 );
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	check_legend_rule(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td1, NULL) && is_id_in_hand(player, get_id(player, card))){
			return 1 ;
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int abilities[2] = {	generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL),
								generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST0, 0, &td1, NULL) && is_id_in_hand(player, get_id(player, card))
		};

		int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
							"Prevent all damage to a creature", abilities[0], 10,
							"Grandeur's Chant", abilities[1], 5);
		if( ! choice  ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == 1 ){
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
			if( spell_fizzled != 1 ){
				instance->info_slot = 1;
			}
		}

		if( choice == 2 ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td1, "TARGET_PLAYER");
			if( spell_fizzled != 1 ){
				discard_id_from_hand(player, get_id(player, card));
				instance->info_slot = 2;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 && valid_target(&td) ){
			prevent_the_next_n_damage(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
									  0, PREVENT_INFINITE, 0, 0);
		}

		if( instance->info_slot == 2 && valid_target(&td1) ){
			int result = target_player_cant_cast_type(instance->parent_controller, instance->parent_card,
													  instance->targets[0].player, TYPE_CREATURE|TYPE_ENCHANTMENT|TYPE_ARTIFACT|TARGET_TYPE_PLANESWALKER|
													  TYPE_SPELL|TYPE_EFFECT);
			add_flag_to_special_effect(instance->parent_controller, result, SE_CREATURES_CANT_ATTACK | SE_END_AT_EOT);
		}
	}

	return 0;
}

int card_pact_of_negation(int player, int card, event_t event){
/* CARD_ID_PACT_OF_NEGATION	8988
Pact of Negation	|0
Instant
Counter target spell.
At the beginning of your next upkeep, pay |3|U|U. If you don't, you lose the game. */

	if( event == EVENT_CAN_CAST ){
		return counterspell(player, card, event, NULL, 0);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player == AI && ! has_virtual_mana_multi(player, MANACOST_XU(3, 2)) ){
			ai_modifier -= 1500;
		}
		return counterspell(player, card, event, NULL, 0);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			create_pact_legacy(player, card, player, MANACOST_XU(3, 2));
			return counterspell(player, card, event, NULL, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

// giant token --> vanilla

int card_pact_of_the_titan(int player, int card, event_t event){
	/* Pact of the Titan	|0
	 * Instant
	 * Put a 4/4 |Sred Giant creature token onto the battlefield.
	 * At the beginning of your next upkeep, pay |4|R. If you don't, you lose the game. */

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player == AI && ! has_virtual_mana_multi(player, MANACOST_XR(4, 1)) ){
			ai_modifier -= 1500;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		generate_token_by_id(player, card, CARD_ID_GIANT);
		create_pact_legacy(player, card, player, MANACOST_XR(4, 1));
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_patricians_scorn(int player, int card, event_t event){
/* CARD_ID_PATRICIANS_SCORN	8990
Patrician's Scorn	|3|W
Instant
If you've cast another |Swhite spell this turn, you may cast ~ without paying its mana cost.
Destroy all enchantments. */

	if( event == EVENT_MODIFY_COST ){
		if( get_stormcolor_count(player, COLOR_WHITE) > 1){
			null_casting_cost(player, card);
		}
	}

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT);
		APNAP(p, { new_manipulate_all(player, card, p, &this_test, KILL_DESTROY); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_pyromancers_swath(int player, int card, event_t event){
	/* Pyromancer's Swath	|2|R	0x20073cf
	 * Enchantment
	 * If an instant or sorcery source you control would deal damage to a permanent or player, it deals that much damage plus 2 to that permanent or player instead.
	 * At the beginning of each end step, discard your hand. */
	card_instance_t* damage = damage_being_dealt(event);
	if (damage && (damage->targets[3].player & TYPE_SPELL)){
		damage->info_slot += 2;
	}

	if (eot_trigger(player, card, event)){
		discard_all(player);
	}
	return global_enchantment(player, card, event);
}

int card_quagnoth(int player, int card, event_t event){//UNUSEDCARD
/* CARD_ID_QUAGNOTH	8996
Quagnoth	|5|G
Creature - Beast 4/5
Split second (As long as this spell is on the stack, players can't cast spells or activate abilities that aren't mana abilities.)
Shroud (This creature can't be the target of spells or abilities.)
When a spell or ability an opponent controls causes you to discard ~, return it to your hand. */

	cannot_be_countered(player, card, event);
	return 0;
}

int card_ramosia_revivalist(int player, int card, event_t event){
/* CARD_ID_RAMOSIAN_REVIVALIST	8998
Ramosian Revivalist	|3|W
Creature - Human Rebel Cleric 2/2
|6, |T: Return target Rebel permanent card with converted mana cost 5 or less from your graveyard to the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	char msg[100] = "Select a Rebel permanent with CMC 5 or less.";
	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_PERMANENT, msg);
	this_test.subtype = SUBTYPE_REBEL;
	this_test.cmc = 6;
	this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, -1, player, selected, REANIMATE_DEFAULT);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST_X(6), 0, NULL, NULL,
															0, &this_test, GS_GRAVE_RECYCLER);
}

int card_ravaging_riftwurm(int player, int card, event_t event){
	/* Ravaging Riftwurm	|1|G|G
	 * Creature - Wurm 6/6
	 * Kicker |4
	 * Vanishing 2
	 * If ~ was kicked, it enters the battlefield with three additional time counters on it. */

	vanishing(player, card, event, 2);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) ){
			if( do_kicker(player, card, MANACOST_X(4)) ){
				add_counters(player, card, COUNTER_TIME, 3);
			}
		}
	}
	return 0;
}

int card_reality_strobe(int player, int card, event_t event)
{
  /* Reality Strobe	|4|U|U
   * Sorcery
   * Return target permanent to its owner's hand. Exile ~ with three time counters on it.
   * Suspend 3-|2|U */

	if( event != EVENT_MODIFY_COST && ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	if (event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td)){
			card_instance_t *instance = get_card_instance(player, card);
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			suspend_a_card(player, card, player, card, 3);
		}
		else
			kill_card(player, card, KILL_DESTROY);
	}

	return suspend(player, card, event, 3, MANACOST_XU(2,1), &td, "TARGET_PERMANENT");
}

int card_riddle_of_lightning(int player, int card, event_t event){
	/* Riddle of Lightning	|3|R|R	0x20073e8
	 * Instant
	 * Choose any target. Scry 3, then reveal the top card of your library. ~ deals damage equal to that card's converted mana cost to that permanent or player. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if( deck_ptr[player][0] != -1 ){
				scry(player, 3);
				int iid = deck_ptr[player][0];
				reveal_card_iid(player, card, iid);
				damage_creature_or_player(player, card, event, get_cmc_by_internal_id(iid));
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

static const char* target_has_a_time_counter(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  return count_counters(player, card, COUNTER_TIME) > 0 ? NULL : "time counters";
}

int card_rift_elemental(int player, int card, event_t event)
{
  /* Rift Elemental	|R
   * Creature - Elemental 1/1
   * |1|R, Remove a time counter from a permanent you control or suspended card you own: ~ gets +2/+0 until end of turn. */

  if (event == EVENT_POW_BOOST)
	return 2 * count_counters_by_counter_and_card_type(player, COUNTER_TIME, TYPE_ANY);

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  base_target_definition(player, card, &td, TYPE_PERMANENT | TYPE_EFFECT);
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (int)target_has_a_time_counter;

  card_instance_t *instance = get_card_instance(player, card);

  int rval = generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1,1), 0, &td, "TARGET_CARD");
  if (event == EVENT_ACTIVATE && cancel != 1)
	{
	  instance->number_of_targets = 0;
	  remove_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_TIME);

	  if (is_what(instance->targets[0].player, instance->targets[0].card, TYPE_EFFECT))
		ai_modifier += 64;	// Remove time counters from suspended cards - good!
	  else
		ai_modifier -= 64;	// Remove time counters from cards with vanishing, or Infinite Hourglass, or such - bad.
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	pump_until_eot_merge_previous(player, card, instance->parent_controller, instance->parent_card, 2, 0);

  return rval;
}

int card_riftsweeper(int player, int card, event_t event){
/* CARD_ID_RIFTSWEEPER	9003
Riftsweeper	|1|G
Creature - Elf Shaman 2/2
When ~ enters the battlefield, choose target face-up exiled card. Its owner shuffles it into his or her library. */


	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		instance->targets[0].player = player;
		if( count_rfg(player) > 0 ){
			if( count_rfg(1-player) > 0 ){
				pick_target(&td, "TARGET_PLAYER");
			}
		}
		else{
			instance->targets[0].player = 1-player;
		}
		instance->number_of_targets = 0;

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		int result = new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_RFG, TUTOR_DECK, 1, AI_MAX_VALUE, &this_test);
		if( result != -1 ){
			shuffle(instance->targets[0].player);
		}

	}

	return 0;
}

int card_river_of_tears(int player, int card, event_t event){
/* CARD_ID_RIVER_OF_TEARS	9005
River of Tears	""
Land
|T: Add |U to your mana pool. If you played a land this turn, add |B to your mana pool instead. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = COLOR_TEST_BLACK;
	}

	if( get_card_instance(player, card)->info_slot == COLOR_TEST_BLUE &&
		whenever_a_player_plays_a_land(player, card, event, player, RESOLVE_TRIGGER_MANDATORY) )
	{
		get_card_instance(player, card)->info_slot = COLOR_TEST_BLACK;
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->info_slot = COLOR_TEST_BLUE;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		// This event also handled above
		play_land_sound_effect_force_color(player, card, COLOR_TEST_BLUE|COLOR_TEST_BLACK);
		return 0;	// so mana_producer() doesn't play the black sound
	}
	return mana_producer(player, card, event);
}

int card_saltskitter(int player, int card, event_t event){
/* CARD_ID_SALTSKITTER	9006
Saltskitter	|3|W
Creature - Wurm 3/4
Whenever another creature enters the battlefield, exile ~. Return ~ to the battlefield under its owner's control at the beginning of the next end step.*/

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && reason_for_trigger_controller == player ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		if( new_specific_cip(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &this_test) ){
			remove_until_eot(player, card, player, card);
		}
	}

	return 0;
}

int card_scourge_of_kher_ridges(int player, int card, event_t event){
/* CARD_ID_SCOURGE_OF_KHER_RIDGES	9009
Scourge of Kher Ridges	|6|R|R
Creature - Dragon 6/6
Flying
|1|R: ~ deals 2 damage to each creature without flying.
|5|R: ~ deals 6 damage to each other creature with flying. */

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_XR(1, 1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		card_instance_t *instance = get_card_instance(player, card);

		int abilities[2] = {	has_mana_for_activated_ability(player, card, MANACOST_XR(1, 1)),
								has_mana_for_activated_ability(player, card, MANACOST_XR(5, 1))
		};
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.illegal_abilities = KEYWORD_FLYING;
		td.toughness_requirement = 2 | TARGET_PT_LESSER_OR_EQUAL;
		td.illegal_abilities = 0;

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.required_abilities = KEYWORD_FLYING;
		td1.toughness_requirement = 6 | TARGET_PT_LESSER_OR_EQUAL;
		td1.illegal_abilities = 0;

		int priorities[2] = {0, 0};
		td.allowed_controller = player;
		priorities[0] -= 2*target_available(player, card, &td);
		priorities[1] -= 2*target_available(player, card, &td1);
		td.allowed_controller = 1-player;
		priorities[0] += 2*target_available(player, card, &td);
		priorities[1] += 2*target_available(player, card, &td1);
		td.allowed_controller = 2;

		int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
							"2 damage to nonflying creatures", abilities[0], priorities[0],
							"6 damage to flying creatures", abilities[1], priorities[1]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_XR((choice == 1 ? 1 : 5), 1)) ){
			instance->info_slot = choice;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		if( instance->info_slot == 1){
			this_test.keyword_flag = 1;
			APNAP(p, {new_damage_all(instance->parent_controller, instance->parent_card, p, 2, 0, &this_test);};);
		}
		if( instance->info_slot == 2){
			this_test.not_me = 1;
			APNAP(p, {new_damage_all(instance->parent_controller, instance->parent_card, p, 6, 0, &this_test);};);
		}
	}

	return 0;
}

int card_sehts_tiger(int player, int card, event_t event){
/*
Seht's Tiger |2|W|W
Creature — Cat 3/3
Flash (You may cast this spell any time you could cast an instant.)
When Seht's Tiger enters the battlefield, you gain protection from the color of your choice until end of turn.
(You can't be targeted, dealt damage, or enchanted by anything of the chosen color.)
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		get_card_instance(player, card)->info_slot = 1<<choose_a_color(player, get_deck_color(player, 1-player));
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->info_slot = 0;
	}

	return flash(player, card, event);
}

int card_shah_of_naar_isle(int player, int card, event_t event){
/* CARD_ID_SHAH_OF_NAAR_ISLE	9013
Shah of Naar Isle	|3|R
Creature - Efreet 6/6
Trample
Echo |0 (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)
When ~'s echo cost is paid, each opponent may draw up to three cards.*/

	if( event == EVENT_RESOLVE_SPELL ){
		set_special_flags(player, card, SF_ECHO_TO_PAY);
	}

	if( check_special_flags(player, card, SF_ECHO_TO_PAY) && current_turn == player && upkeep_trigger(player, card, event) ){
		remove_special_flags(player, card, SF_ECHO_TO_PAY);
		int kill = 1;
		int choice = do_dialog(player, player, card, -1, -1, " Pay Echo\n Pass", 0);
		if( choice == 0 ){
			draw_some_cards_if_you_want(player, card, 1-player, 3);
			kill = 0;
		}

		if( kill == 1 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_shapeshifters_marrow(int player, int card, event_t event){
/* Shapeshifter's Marrow	|2|U|U
 * Enchantment
 * At the beginning of each opponent's upkeep, that player reveals the top card of his or her library.
 * If it's a creature card, the player puts the card into his or her graveyard and ~ becomes a copy of that card. */

	cloning_card(player, card, event);

	upkeep_trigger_ability(player, card, event, 1-player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int iid = deck_ptr[1-player][0];
		if( iid != -1 ){
			reveal_card_iid(player, card, iid);
			if( is_what(-1, iid, TYPE_CREATURE) ){
				mill(1-player, 1);
				cloning_and_verify_legend(player, card, -1, iid);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_shimian_specter(int player, int card, event_t event){
/* Shimian Specter	|2|B|B
 * Creature - Specter 2/2
 * Flying
 * Whenever ~ deals combat damage to a player, that player reveals his or her hand. You choose a nonland card from it.
 * Search that player's graveyard, hand, and library for all cards with the same name as that card and exile them.
 * Then that player shuffles his or her library. */

	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE))
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = {0};
		times_damaged[0] = BYTE0(instance->targets[1].player);
		times_damaged[1] = BYTE1(instance->targets[1].player);
		instance->targets[1].player = 0;
		APNAP(p,{
					if( times_damaged[p] && hand_count[p] ){
						test_definition_t this_test;
						new_default_test_definition(&this_test, TYPE_LAND, "Select a non-land card.");
						this_test.type_flag = 1;
						this_test.zone = TARGET_ZONE_HAND;
						int cc = check_battlefield_for_special_card(player, card, 1-player, 0, &this_test);
						int selected = new_select_a_card(player, p, TUTOR_FROM_HAND, cc, AI_MAX_VALUE, -1, &this_test);
						if( selected != -1 ){
							int id = get_id(1-player, selected);
							lobotomy_effect(player, p, id, 1);
						}
					}
				}
		);
	}
	return 0;
}

int card_skirk_ridge_exhumer(int player, int card, event_t event){
	/* Skirk Ridge Exhumer	|1|B
	 * Creature - Zombie Spellshaper 1/1
	 * |B, |T, Discard a card: Put a 1/1 |Sblack Zombie Goblin creature token named Festering Goblin onto the battlefield. It has "When Festering Goblin dies, target creature gets -1/-1 until end of turn." */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_FESTERING_GOBLIN);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD, MANACOST_B(1), 0, NULL, NULL);
}

int card_slaughter_pact(int player, int card, event_t event){
/* CARD_ID_SLAUGHTER_PACT	9019
Slaughter Pact	|0
Instant
Destroy target non|Sblack creature.
At the beginning of your next upkeep, pay |2|B. If you don't, you lose the game. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.illegal_color = COLOR_TEST_BLACK;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player == AI && ! has_virtual_mana_multi(player, MANACOST_XB(2, 1)) ){
			ai_modifier -= 1500;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			create_pact_legacy(player, card, player, MANACOST_XB(2, 1));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_CREATURE", 1, NULL);
}

int card_sliver_legion( int player, int card, event_t event){
	/* Sliver Legion	|W|U|B|R|G
	 * Legendary Creature - Sliver 7/7
	 * All Sliver creatures get +1/+1 for each other Sliver on the battlefield. */

	check_legend_rule(player, card, event);

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && in_play(player, card) && ! is_humiliated(player, card) ){
		if( has_subtype(affected_card_controller, affected_card, SUBTYPE_SLIVER) && in_play(affected_card_controller, affected_card) ){
			/* "each other Sliver" means other than the Sliver creature in question, not other than Sliver Legion, so this
			* doesn't need to check if it's still a Sliver itself */
			int total = count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_SLIVER) - 1;
			if (total > 0){
				event_result += total;
			}
		}
	}

	return slivercycling(player, card, event);
}

int card_sliversmith(int player, int card, event_t event){
	/* Sliversmith	|2
	 * Artifact Creature - Spellshaper 1/1
	 * |1, |T, Discard a card: Put a 1/1 colorless Sliver artifact creature token named Metallic Sliver onto the battlefield. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_METALLIC_SLIVER);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD, MANACOST_X(1), 0, NULL, NULL);
}

int card_soultether_golem(int player, int card, event_t event){
	/* Soultether Golem	|2
	 * Artifact Creature - Golem 3/3
	 * Vanishing 1
	 * Whenever another creature enters the battlefield under your control, put a time counter on ~. */

	vanishing(player, card, event, 1);

    test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);
	test.not_me = 1;

	etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);

	if( event == EVENT_ETB_ABILITY ){
		add_counter(player, card, COUNTER_TIME);
	}

	return 0;
}

int card_sparkspitter(int player, int card, event_t event){
	/* Sparkspitter	|2|R
	 * Creature - Elemental Spellshaper 1/3
	 * |R, |T, Discard a card: Put a 3/1 |Sred Elemental creature token named Spark Elemental onto the battlefield. It has trample, haste, and "At the beginning of the end step, sacrifice Spark Elemental." */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_SPARK_ELEMENTAL);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD, MANACOST_R(1), 0, NULL, NULL);
}

static int spellweaver_volute_name_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	if( instance->damage_target_player > -1 ){
		card_instance_t *sw_card = get_card_instance(instance->damage_target_player, instance->damage_target_card);
		if (event == EVENT_SET_LEGACY_EFFECT_NAME && sw_card->targets[2].card > -1 ){
			scnprintf(set_legacy_effect_name_addr, 51, "%s", cards_ptr[cards_data[sw_card->targets[2].card].id]->name);
		}
	}
	return 0;
}

int card_spellweaver_volute(int player, int card, event_t event){
/*
Spellweaver Volute |3|U|U
Enchantment — Aura
Enchant instant card in a graveyard
Whenever you cast a sorcery spell, copy the enchanted instant card. You may cast the copy without paying its mana cost.
If you do, exile the enchanted card and attach Spellweaver Volute to another instant card in a graveyard.
*/
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) && get_card_instance(player, card)->info_slot == 66 ){
		card_instance_t *instance = get_card_instance(player, card);
		int pos = find_in_graveyard_by_source(instance->targets[0].player, instance->targets[1].card,
												instance->targets[1].player);
		if( pos == -1 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( get_card_instance(player, card)->info_slot == 66 && trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_SORCERY);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			card_instance_t *instance = get_card_instance(player, card);

			int pos = instance->targets[1].player;
			int iid = get_grave(instance->targets[0].player)[pos];
			int new_attaching = 0;
			if( can_legally_play_iid(player, iid) ){
				copy_spell(player, cards_data[iid].id);
				new_attaching = 1;
			}
			if( ! new_attaching ){
				return 0;
			}
			instance->info_slot = 0;
			rfg_card_from_grave(instance->targets[0].player, pos);

			test_definition_t test2;
			new_default_test_definition(&test2, TYPE_INSTANT | TYPE_INTERRUPT, "Select target instant card.");
			int targettable_grave_for_spellweaver_volute[2] = {0};
			APNAP(p,{
						if( ! graveyard_has_shroud(p) && new_special_count_grave(p, &test2) ){
							targettable_grave_for_spellweaver_volute[p] = 1;
						}
					}
			);
			if( targettable_grave_for_spellweaver_volute[player] + targettable_grave_for_spellweaver_volute[1-player] == 0 ){
				kill_card(player, card, KILL_SACRIFICE);
				return 0;
			}
			int t_player = player;
			if( targettable_grave_for_spellweaver_volute[player] ){
				if( targettable_grave_for_spellweaver_volute[1-player] ){
					target_definition_t td;
					default_target_definition(player, card, &td, 0);
					td.zone = TARGET_ZONE_PLAYERS;
					td.allow_cancel = 0;
					td.illegal_abilities = 0;

					new_pick_target(&td, "Whose graveyard?", 0, GS_LITERAL_PROMPT);
					t_player = instance->targets[0].player;
					instance->number_of_targets = 0;
				}
			}
			else{
				t_player = 1-player;
			}
			if( select_target_from_grave_source(player, card, t_player, 1, AI_MAX_CMC, &test2, 1) ){
				instance->info_slot = 66;
			}
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_INSTANT | TYPE_INTERRUPT, "Select target instant card.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &test);
		if( spell_fizzled != 1 ){
			ai_modifier += 25;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			instance->info_slot = 66;
			int legacy = create_targetted_legacy_effect(player, card, &spellweaver_volute_name_legacy, player, card);
			get_card_instance(player, legacy)->eot_toughness = EVENT_SET_LEGACY_EFFECT_NAME;
		}
		else{
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_spin_into_myth(int player, int card, event_t event){
/* CARD_ID_SPIN_INTO_MYTH	9027
Spin into Myth	|4|U
Instant
Put target creature on top of its owner's library, then fateseal 2. (To fateseal 2, look at the top two cards of an opponent's library,
then put any number of them on the bottom of that player's library and the rest on top in any order.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
			fateseal(player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_spirit_en_dal(int player, int card, event_t event){
	/* Spirit en-Dal	|2|W
	 * Creature - Spirit 2/1
	 * Shadow
	 * Forecast - |1|W, Reveal ~ from your hand: Target creature gains shadow until end of turn. */

	shadow(player, card, event);

	if( !(event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND || event == EVENT_RESOLVE_ACTIVATION_FROM_HAND) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND ){
		if( has_mana_multi(player, MANACOST_XW(1, 1)) && can_target(&td) && instance->info_slot != 1 && current_phase == PHASE_UPKEEP &&
			count_upkeeps(player) > 0
		  ){
			return 1;
		}
	}
	if( event == EVENT_ACTIVATE_FROM_HAND ){
		instance->number_of_targets = 0;
		charge_mana_multi(player, MANACOST_XW(1, 1));
		if( spell_fizzled != 1 && pick_target(&td, "TARGET_CREATURE") ){
			instance->info_slot = 1;
			return 1;
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_SHADOW);
		}
	}
	return 0;
}

static const char* target_has_2_spore_counters_and_can_activate(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  if (count_counters(player, card, COUNTER_SPORE) < 2)
	return "must have counters";
  if (!CAN_ACTIVATE0(player, card))
	return "cannot activate";
  return NULL;
}
int card_sporoloth_ancient(int player, int card, event_t event)
{
  /* Sporoloth Ancient	|3|G|G
   * Creature - Fungus 4/4
   * At the beginning of your upkeep, put a spore counter on ~.
   * Creatures you control have "Remove two spore counters from this creature: Put a 1/1 |Sgreen Saproling creature token onto the battlefield." */

  add_spore_counters(player, card, event);

  if (!IS_ACTIVATING(event))
	return 0;

  target_definition_t td;
  base_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = td.preferred_controller = player;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (int)target_has_2_spore_counters_and_can_activate;

  if (event == EVENT_CAN_ACTIVATE)
	return !is_humiliated(player, card) && can_target(&td);

  card_instance_t* instance = get_card_instance(player, card);
  if (event == EVENT_ACTIVATE && pick_next_target_noload(&td, "Select a creature you control with two spore counters."))
	{
	  instance->number_of_targets = 0;
	  if (charge_mana_for_activated_ability(instance->targets[0].player, instance->targets[0].card, MANACOST0))
		remove_counters(instance->targets[0].player, instance->targets[0].card, COUNTER_SPORE, 2);
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	generate_token_by_id(player, card, CARD_ID_SAPROLING);

  return 0;
}

int card_sprout_swarm(int player, int card, event_t event){
	/* Sprout Swarm	|1|G
	 * Instant
	 * Convoke
	 * Buyback |3
	 * Put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_MODIFY_COST){
		if( has_convoked_mana_extended(player, card, event, MANACOST_XG(1, 1)) == 2){
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

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1){
			if( ! charge_convoked_mana_extended(player, card, event, MANACOST_XG(1, 1)) ){
				spell_fizzled = 1;
			}
		}
		if( ! is_token(player, card) ){
			int amount = 3-(2*count_cards_by_id(player, CARD_ID_MEMORY_CRYSTAL));
			if( has_convoked_mana_extended(player, card, event, MANACOST_X(amount)) ){
				int choice = do_dialog(player, player, card, -1, -1, " Play normally\n Buyback", 1);
				if( choice == 1 ){
					if( charge_convoked_mana_extended_generic(player, card, event, MANACOST_X(amount), 0) ){
						instance->info_slot |= 2;
					}
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		generate_token_by_id(player, card, CARD_ID_SAPROLING);
		if( instance->info_slot & 2){
			bounce_permanent(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_storm_entity(int player, int card, event_t event){
/* CARD_ID_STORM_ENTITY	9032
Storm Entity	|1|R
Creature - Elemental 1/1
Haste
~ enters the battlefield with a +1/+1 counter on it for each other spell cast this turn. */

	haste(player, card, event);

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, get_storm_count());

	return 0;
}

int card_street_wraith(int player, int card, event_t event){

	/* Street Wraith	|3|B|B
	 * Creature - Wraith 3/4
	 * |H2Swampwalk
	 * Cycling-Pay 2 life. */

	if( ! is_unlocked(player, card, event, 14) ){ return 0; }

	return cycling_special_cost(player, card, event, 2);
}

int card_stronghold_rats(int player, int card, event_t event){
/*
Stronghold Rats |2|B
Creature — Rat 2/1, 2B (3)
Shadow (This creature can block or be blocked by only creatures with shadow.)
Whenever Stronghold Rats deals combat damage to a player, each player discards a card.
*/
	shadow(player, card, event);

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		APNAP(p, { new_multidiscard(p, amount, 0, player); };);
	}

	return 0;
}

int card_summoners_pact(int player, int card, event_t event){
/* CARD_ID_SUMMONERS_PACT	9035
Summoner's Pact	|0
Instant
Search your library for a |Sgreen creature card, reveal it, and put it into your hand. Then shuffle your library.
At the beginning of your next upkeep, pay |2|G|G. If you don't, you lose the game. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player == AI && ! has_virtual_mana_multi(player, MANACOST_XG(2, 2)) ){
			ai_modifier -= 1500;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, get_sleighted_color_text(player, card, "Select a %s creature card.", COLOR_GREEN));
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, 1, &this_test);
		create_pact_legacy(player, card, player, MANACOST_XG(2, 2));
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_sword_of_the_meek(int player, int card, event_t event)
{
	/* Sword of the Meek	|2
	 * Artifact - Equipment
	 * Equipped creature gets +1/+2.
	 * Equip |2
	 * Whenever a 1/1 creature enters the battlefield under your control, you may return ~ from your graveyard to the battlefield, then attach it to that
	 * creature. */

	return vanilla_equipment(player, card, event, 2, 1, 2, 0, 0);
}

int card_take_possession(int player, int card, event_t event){
/* CARD_ID_TAKE_POSSESSION	9037
Take Possession	|5|U|U
Enchantment - Aura
Split second (As long as this spell is on the stack, players can't cast spells or activate abilities that aren't mana abilities.)
Enchant permanent
You control enchanted permanent. */

	// Approximantion

	cannot_be_countered(player, card, event);

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	return generic_stealing_aura(player, card, event, &td, "TARGET_PERMANENT");
}

int card_tarmogoyf(int player, int card, event_t event){
/* Tarmogoyf	|1|G
 * Creature - Lhurgoyf 100/101
 * ~'s power is equal to the number of card types among cards in all graveyards and its toughness is equal to that number plus 1. */

	if( event == EVENT_POWER || event == EVENT_TOUGHNESS){
		if( affect_me(player, card ) && ! is_humiliated(player, card) && player != -1 ){
			event_result += count_types_in_grave(ANYBODY);
		}
	}
	return 0;
}

int card_tarox_bladewing(int player, int card, event_t event){
/*
Tarox Bladewing |2|R|R|R
Legendary Creature — Dragon 4/3
Flying, haste
Grandeur — Discard another card named Tarox Bladewing: Tarox Bladewing gets +X/+X until end of turn, where X is its power.
*/
	check_legend_rule(player, card, event);

	haste(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return card_yew_spirit(player, card, event);
	}

	return grandeur(player, card, event);
}

int card_tolaria_west(int player, int card, event_t event)
{
  /* Tolaria West	""
   * Land
   * ~ enters the battlefield tapped.
   * |T: Add |U to your mana pool.
   * Transmute |1|U|U */

  if (IS_ACTIVATING_FROM_HAND(event))
	return transmute(player, card, event, MANACOST_XU(1,2), 0);

  comes_into_play_tapped(player, card, event);

  if (in_play(player, card))
	return mana_producer(player, card, event);

  return 0;
}

int card_tombstalker(int player, int card, event_t event){
/* CARD_ID_TOMBSTALKER	9043
Tombstalker	|6|B|B
Creature - Demon 5/5
Flying
Delve (Each card you exile from your graveyard while casting this spell pays for |1.) */

	return delve(player, card, event);
}

int card_unblinking_bleb(int player, int card, event_t event){
/* CARD_ID_UNBLINKING_BLEB	9044
Unblinking Bleb	|3|U
Creature - Illusion 1/3
Morph |2|U (You may cast this card face down as a 2/2 creature for |3. Turn it face up any time for its morph cost.)
Whenever ~ or another permanent is turned face up, you may scry 2. (Look at the top two cards of your library,
then put any number of them on the bottom of your library and the rest on top in any order.) */

	if( event == EVENT_TURNED_FACE_UP ){
		scry(player, 2);
	}

	return morph(player, card, event, MANACOST_XU(2, 1));
}

int card_utopia_mycon(int player, int card, event_t event)
{
  /* Utopia Mycon	|G
   * Creature - Fungus 0/2
   * At the beginning of your upkeep, put a spore counter on ~.
   * Remove three spore counters from ~: Put a 1/1 |Sgreen Saproling creature token onto the battlefield.
   * Sacrifice a Saproling: Add one mana of any color to your mana pool. */

	add_spore_counters(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Saproling to sacrifice.");
	test.subtype = SUBTYPE_SAPROLING;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAPROLING
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3), NULL, NULL) ){
			return 1;
		}
		return can_produce_mana(player, card) && new_can_sacrifice_as_cost(player, card, &test) ? 1 : 0;
	}

	if( event == EVENT_ACTIVATE ){
		int abilities[3] = {
							0,
							can_produce_mana(player, card) && new_can_sacrifice_as_cost(player, card, &test) ? 1 : 0,
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3),
														NULL, NULL)
		};
		int choice = CHOICE_MANA;
		instance->info_slot = choice;
		if( ! paying_mana() ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Sac Saproling and get mana", abilities[1], 1,
							"Create a Saproling", abilities[2], 5);

			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			if (new_sacrifice(player, card, player, SAC_AS_COST, &test))
			{
				instance->number_of_targets = 0;
				FORCE(produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1));
				tapped_for_mana_color = -2;
			}
			else{
				spell_fizzled = 1;
			}
		}
		if( choice == CHOICE_SAPROLING ){
			return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_SPORE, 3), NULL, NULL) ;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAPROLING ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_SAPROLING);
		}
	}

	return 0;
}

int card_vedalken_aethermage(int player, int card, event_t event){
	/* Vedalken AEthermage	|1|U
	 * Creature - Vedalken Wizard 1/2
	 * Flash
	 * When ~ enters the battlefield, return target Sliver to its owner's hand.
	 * Wizardcycling |3 */

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND ){
		return cycling(player, card, event, MANACOST_X(3));
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_WIZARD;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, 1, &this_test);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.required_subtype = SUBTYPE_SLIVER;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target Sliver.", 0, GS_LITERAL_PROMPT) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}
	return flash(player, card, event);
}

int card_veilstone_amulet(int player, int card, event_t event){
/*
Veilstone Amulet |3
Artifact
Whenever you cast a spell, creatures you control can't be the targets of spells or abilities your opponents control this turn.
*/
	if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL)  ){
		pump_subtype_until_eot(player, card, player, -1, 0, 0, 0, SP_KEYWORD_HEXPROOF);
	}

	return 0;
}

int card_venser_shaper_savant(int player, int card, event_t event){
/* CARD_ID_VENSER_SHAPER_SAVANT	9049
Venser, Shaper Savant	|2|U|U
Legendary Creature - Human Wizard 2/2
Flash
When ~ enters the battlefield, return target spell or permanent to its owner's hand. */

	check_legend_rule(player, card, event);

	if( event == EVENT_CAN_CAST ){
		if( counterspell(player, card, event, NULL, 0) ){
			get_card_instance( player, card )->info_slot = 1;
			return 99;
		}
		else{
			get_card_instance( player, card )->info_slot = 0;
			return 1;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( get_card_instance( player, card )->info_slot == 1 ){
			counterspell(player, card, event, NULL, 0);
		}
	}

	if( comes_into_play(player, card, event) ){
		card_instance_t *instance = get_card_instance( player, card );
		if( instance->info_slot == 1 ){
			if( counterspell_validate(player, card, NULL, 0) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
		else{
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_PERMANENT );
			td.allow_cancel = 0;

			pick_target(&td, "TARGET_PERMANENT");
			if( valid_target(&td) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
			instance->number_of_targets = 0;
		}
	}
	return 0;
}

int card_virulent_sliver(int player, int card, event_t event){
/* CARD_ID_VIRULENT_SLIVER	9050
Virulent Sliver	|G
Creature - Sliver 1/1
All Sliver creatures have poisonous 1. (Whenever a Sliver deals combat damage to a player, that player gets a poison counter.
A player with ten or more poison counters loses the game.) */

	if( ! is_humiliated(player, card) ){
		if( event == EVENT_DEAL_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card && damage->info_slot > 0){
				if( damage->damage_target_card == -1 && ! damage_is_to_planeswalker(damage) &&
					has_subtype(damage->damage_source_player, damage->damage_source_card, SUBTYPE_SLIVER) )
				{
					card_instance_t *instance = get_card_instance(player, card);
					int damaged_player = instance->damage_target_player;
					if( instance->targets[damaged_player].player < 0 ){
						instance->targets[damaged_player].player = 0;
					}
					instance->targets[damaged_player].player += damage->info_slot;
				}
			}
		}

		if( trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player ){
			card_instance_t *instance = get_card_instance(player, card);
			if( instance->targets[0].player > 0 || instance->targets[1].player > 0 ){
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						APNAP(p,{
									if( instance->targets[p].player ){
										poison(p, instance->targets[p].player);
									}
								}
						);
						instance->targets[0].player = instance->targets[1].player = 0;
				}
			}
		}
	}

	return 0;
}

int card_wrap_in_vigor(int player, int card, event_t event){
/* CARD_ID_WRAP_IN_VIGOR	9054
Wrap in Vigor	|1|G
Instant
Regenerate each creature you control. */

	if( event == EVENT_RESOLVE_SPELL ){
		int i = active_cards_count[player]-1;
		while( i > -1 ){
				if( in_play(player, i) && is_what(player, i, TYPE_CREATURE) ){
						regenerate_or_shield(player, card, player, i);
				}
				i--;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

