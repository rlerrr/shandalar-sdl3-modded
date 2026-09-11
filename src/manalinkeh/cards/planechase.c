#include "manalink.h"

// Also finds Phenomenon cards (by design)
int is_plane_card(int csvid)
{
  // Subtype Plane/Phenomenon is guaranteed to be first.
  return csvid >= 0 && (cards_ptr[csvid]->types[0] == SUBTYPE_PLANE || cards_ptr[csvid]->types[0] == SUBTYPE_PHENOMENON);
}

static int get_actual_plane(int player){
	int pcc = locate_id(player, CARD_ID_PLANECHASE);
	if( pcc > -1 ){
		card_instance_t *instance = get_card_instance(player, pcc);
		return instance->targets[1].card;
	}
	else{
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_play(player, i) && is_plane_card(get_id(player, i)) ){
				return get_id(player, i);
			}
		}
	}
	return -1;
}

int get_new_plane(int current_plane)
{
  static int* plane_csvids = NULL;
  static int num_plane_csvids;

  if (!plane_csvids)
	{
	  // Find all valid plane csvids.  Since we only do this once and it's (relatively) quick, do it in two passes.

	  // First pass: find number.
	  int iid, csvid, num_plane_csvids_first_pass = 0;
	  for (iid = 0; (csvid = cards_data[iid].id) != CARD_ID_RULES_ENGINE; ++iid)
		if (is_plane_card(csvid) && (card_coded[csvid / 8] & (1 << (csvid % 8))))
		  ++num_plane_csvids_first_pass;

	  // Allocate the array
	  ASSERT(num_plane_csvids_first_pass > 0);
	  plane_csvids = (int*)malloc(sizeof(int) * num_plane_csvids_first_pass);
	  ASSERT(plane_csvids != NULL);

	  // Second pass: Populate array
	  num_plane_csvids = 0;
	  for (iid = 0; (csvid = cards_data[iid].id) != CARD_ID_RULES_ENGINE; ++iid)
		if (is_plane_card(csvid) && (card_coded[csvid / 8] & (1 << (csvid % 8))))
		  plane_csvids[num_plane_csvids++] = csvid;
	  ASSERT(num_plane_csvids == num_plane_csvids_first_pass);
	}

  return plane_csvids[internal_rand(num_plane_csvids)];
}

void planeswalk_to(int player, int target_plane, int override){
	int i;
	int actual_plane = -1;
	for(i=0;i<2; i++){
		int count = active_cards_count[i]-1;
		while( count > -1 ){
				if( in_play(i, count) ){
					int id = get_id(i, count);
					if( id == CARD_ID_PLANECHASE ){
						card_instance_t *instance = get_card_instance(i, count);
						if( actual_plane == -1 ){
							actual_plane = instance->targets[1].card;
						}
						instance->targets[1].card = target_plane;
					}
					if( is_plane_card(id) ){
						if( player == i && override != 1 ){
							card_instance_t *plane = get_card_instance(i, count);
							CardFunction ptFunction = cards_data[plane->internal_card_id].code_pointer;
							ptFunction(i, count, EVENT_PLANESWALK_OUT);
						}
						kill_card(i, count, KILL_EXILE);
					}
				}
				count--;
		}
		int card_added = add_card_to_hand(i, get_internal_card_id_from_csv_id(target_plane));
		convert_to_token(i, card_added);
		put_into_play(i, card_added);
		card_instance_t *instance = get_card_instance(player, card_added);
		instance->targets[1].player = 66;
		set_special_infos(player, card_added, 88);
	}
	if( override != 1 ){
		int fake = add_card_to_hand(player, get_internal_card_id_from_csv_id(target_plane));
		CardFunction ptFunction = cards_data[get_internal_card_id_from_csv_id(target_plane)].code_pointer;
		ptFunction(player, fake, EVENT_PLANESWALK_IN);
		obliterate_card(player, fake);
	}
}

static void casting_the_planar_dice( int player, int card, int result){
	int cp = get_actual_plane(player);
	if( cp > -1 ){
		if( player == AI  ){
			do_dialog(HUMAN, player, card, -1, -1, "Opponent is casting the Planar Dice.", 0);
		}
		if( (get_id(player, card) == CARD_ID_PLANECHASE || get_id(player, card) == CARD_ID_FRACTURED_POWERSTONE) && result == 1 ){
			int np = get_new_plane(cp);
			card_ptr_t* c = cards_ptr[ cp  ];
			card_ptr_t* c1 = cards_ptr[ np  ];
			char buffer[100];
			scnprintf(buffer, 100, "Planeswalking away from %s\nto %s", c->name, c1->name);
			do_dialog(HUMAN, player, card, -1, -1, buffer, 0);
			int se_card = check_for_special_effect_card(player, CARD_ID_ELOREN_WILDS);
			if( result > -1 ){
				kill_card(player, se_card, KILL_EXILE);
			}
			planeswalk_to(player, np, 0);
		}
		else if( result == 6 || (get_id(player, card) != CARD_ID_PLANECHASE && result == 5) ){
				card_ptr_t* c = cards_ptr[ cp  ];
				char buffer[100];
				scnprintf(buffer, 100, "The power of %s\nis unleashed!", c->name);
				do_dialog(HUMAN, player, card, -1, -1, buffer, 0);
				int plane = locate_id(player, cp);
				if( plane > -1 ){
					card_instance_t *instance = get_card_instance(player, plane);
					CardFunction ptFunction = cards_data[instance->internal_card_id].code_pointer;
					ptFunction(player, plane, EVENT_CHAOS);
				}
		}
		else{
			 do_dialog(HUMAN, player, card, -1, -1, "Nothing happened.", 0);
		}
	}
}

static int generic_plane(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( get_special_infos(player, card) == 88 ){
			return 0;
		}
		int result = 0;
		if( can_sorcery_be_played(player, event) && has_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX)) ){
			result = 1;
		}
		return result;
	}

	if( event == EVENT_ACTIVATE ){
		if( current_phase == PHASE_MAIN2 ){
			ai_modifier+=50;
		}
		int amount = count_counters(player, card, COUNTER_VORTEX);
		charge_mana(player, COLOR_COLORLESS, amount);
		if( spell_fizzled != 1 ){
			add_counter(player, card, COUNTER_VORTEX);
			int cc = internal_rand(6)+1;
			instance->info_slot = cc;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if (instance->info_slot > 0){
			casting_the_planar_dice(instance->parent_controller, instance->parent_card, instance->info_slot);
		}
	}

	if( event == EVENT_CLEANUP ){
		remove_all_counters(player, card, COUNTER_VORTEX);
	}

	return 0;
}


int card_planechase(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 8);

	return generic_plane(player, card, event);
}


// Planechase 2012

// white
int card_felidar_umbra(int player, int card, event_t event){
/* CARD_ID_FELIDAR_UMBRA	12523
Felidar Umbra	|1|W
Enchantment - Aura
Enchant creature
Enchanted creature has lifelink.
|1|W: Attach ~ to target creature you control.
Totem armor (If enchanted creature would be destroyed, instead remove all damage from it and destroy this Aura.) */

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XW(1, 1), 0, &td, "TARGET_CREATURE");
	}


	if( event == EVENT_RESOLVE_ACTIVATION ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( valid_target(&td) ){
			attach_aura_to_target(instance->parent_controller, instance->parent_card, event,
									instance->targets[0].player, instance->targets[0].card);
		}
	}

	return totem_armor(player, card, event, 0, 0, 0, SP_KEYWORD_LIFELINK);
}

int card_illusory_angel(int player, int card, event_t event){
/* CARD_ID_ILLUSORY_ANGEL	12525
Illusory Angel	|2|U
Creature - Angel Illusion 4/4
Flying
Cast ~ only if you've cast another spell this turn. */

	if( event == EVENT_MODIFY_COST && get_specific_storm_count(player) < 1 ){
		infinite_casting_cost();
	}
	return 0;
}

// blue
int card_sakashimas_student(int player, int card, event_t event)
{
  /* Sakashima's Student	|2|U|U
   * Creature - Human Ninja 0/0
   * Ninjutsu |1|U
   * You may have ~ enter the battlefield as a copy of any creature on the battlefield, except it's still a Ninja in addition to its other creature types. */

  if (enters_the_battlefield_as_copy_of_any_creature(player, card, event))
	add_a_subtype(player, card, SUBTYPE_NINJA);

  return ninjutsu(player, card, event, MANACOST_XU(1,1), CARD_ID_SAKASHIMAS_STUDENT);
}

// red
int card_beetleback_chief(int player, int card, event_t event ){
	/* Beetleback Chief	|2|R|R
	 * Creature - Goblin Warrior 2/2
	 * When ~ enters the battlefield, put two 1/1 |Sred Goblin creature tokens onto the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_tokens_by_id(player, card, CARD_ID_GOBLIN, 2);
	}

	return 0;
}

int card_mass_mutiny(int player, int card, event_t event)
{
  /* Mass Mutiny	|3|R|R
   * Sorcery
   * For each opponent, gain control of up to one target creature that player controls until end of turn. Untap those creatures. They gain haste until end of
   * turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = 1-player;

	if (event == EVENT_RESOLVE_SPELL)
	{
		card_instance_t* instance = get_card_instance(player, card);
		if (valid_target(&td))
			effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE_OPPONENT_CONTROLS", 1, NULL);
}

int card_preyseizer_dragon(int player, int card, event_t event)
{
	/* Preyseizer Dragon	|4|R|R	0x20065ce
	 * Creature - Dragon 4/4
	 * Flying
	 * Devour 2
	 * Whenever ~ attacks, it deals damage to any target equal to the number of +1/+1 counters on ~. */
	if (declare_attackers_trigger(player, card, event, 0, player, card)){
		int amt = count_1_1_counters(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);
		instance->number_of_targets = 0;
		if (can_target(&td) && pick_target(&td, "TARGET_ANY") && amt > 0){	// Still needs to trigger and target even if no counters
			damage_creature(instance->targets[0].player, instance->targets[0].card, amt, player, card);
		}
	}

	devour(player, card, event, 2);
	return 0;
}

// green
int card_brindle_shoat(int player, int card, event_t event ){
	/* Brindle Shoat	|1|G
	 * Creature - Boar 1/1
	 * When ~ dies, put a 3/3 |Sgreen Boar creature token onto the battlefield. */

	if( (event == EVENT_ATTACK_RATING || event == EVENT_BLOCK_RATING) && affect_me(player, card)){
		if (event == EVENT_ATTACK_RATING){
			ai_defensive_modifier -= 24;
		}
		else {	// EVENT_BLOCK_RATING
			ai_defensive_modifier += 24;
		}
	}

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_BOAR, &token);
		token.pow = token.tou = 3;
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		generate_token(&token);
	}

	return 0;
}

int card_dreampod_druid(int player, int card, event_t event )
{
  /* Dreampod Druid	|1|G
   * Creature - Human Druid 2/2
   * At the beginning of each upkeep, if ~ is enchanted, put a 1/1 |Sgreen Saproling creature token onto the battlefield. */

  if (trigger_condition == TRIGGER_UPKEEP && is_enchanted(player, card))
	upkeep_trigger_ability(player, card, event, ANYBODY);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	generate_token_by_id(player, card, CARD_ID_SAPROLING);

  return 0;
}

// gold

int card_baleful_strix(int player, int card, event_t event ){
/* CARD_ID_BALEFUL_STRIX	12516
Baleful Strix	|U|B
Artifact Creature - Bird 1/1
Flying, deathtouch
When ~ enters the battlefield, draw a card. */

	deathtouch(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_dragonlair_spider(int player, int card, event_t event ){
	/* Dragonlair Spider	|2|R|R|G|G
	 * Creature - Spider 5/6
	 * Reach
	 * Whenever an opponent casts a spell, put a 1/1 |Sgreen Insect creature token onto the battlefield. */

	if( new_specific_spell_played(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		generate_token_by_id(player, card, CARD_ID_INSECT);
	}

	return 0;
}

// elderwood scion --> skipped (impossible)

int card_etherium_horn_sorcerer(int player, int card, event_t event ){
/* CARD_ID_ETHERIUM_HORN_SORCERER	12522
Etherium-Horn Sorcerer	|4|U|R
Artifact Creature - Minotaur Wizard 3/6
|1|U|R: Return ~ to its owner's hand.
Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less.
You may cast it without paying its mana cost. Put the exiled cards on the bottom of your library in a random order.) */

	cascade(player, card, event, -1);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		bounce_permanent(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XUR(1, 1, 1), 0, NULL, NULL);
}

int card_indrik_umbra(int player, int card, event_t event){
/* CARD_ID_INDRIK_UMBRA	12526
Indrik Umbra	|4|G|W
Enchantment - Aura
Enchant creature
Enchanted creature gets +4/+4 and has first strike, and all creatures able to block it do so.
Totem armor (If enchanted creature would be destroyed, instead remove all damage from it and destroy this Aura.) */

	return totem_armor(player, card, event, 4, 4, KEYWORD_FIRST_STRIKE, SP_KEYWORD_LURE);
}

int card_krond_the_dawn_clad(int player, int card, event_t event)
{
/* CARD_ID_KROND_THE_DAWN_CLAD	12527
Krond the Dawn-Clad	|G|G|G|W|W|W
Legendary Creature - Archon 6/6
Flying, vigilance
Whenever ~ attacks, if it's enchanted, exile target permanent. */

  check_legend_rule(player, card, event);

  vigilance(player, card, event);

  // Whenever ~ attacks, if it's enchanted, exile target permanent.
  if ((xtrigger_condition() == XTRIGGER_ATTACKING || event == EVENT_DECLARE_ATTACKERS)
	  && is_enchanted(player, card)
	  && declare_attackers_trigger(player, card, event, 0, player, card))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_PERMANENT );
	  td.allow_cancel = 0;

	  card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_PERMANENT")){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		instance->number_of_targets = 0;
	}

  return 0;
}

int card_maeltrom_wanderer(int player, int card, event_t event){
/* CARD_ID_MAELSTROM_WANDERER	12528
Maelstrom Wanderer	|5|U|R|G
Legendary Creature - Elemental 7/5
Creatures you control have haste.
Cascade, cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less.
You may cast it without paying its mana cost. Put the exiled cards on the bottom of your library in a random order. Then do it again.) */

	check_legend_rule(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && ! is_token(player, card) && ! check_special_flags(player, card, SF_NOT_CAST) ){
		do_cascade(player, card, -1);
		do_cascade(player, card, -1);
	}
	if( event == EVENT_ABILITIES && is_what(affected_card_controller, affected_card, TYPE_CREATURE) && ! is_humiliated(player, card) ){
		if( affected_card_controller == player && in_play(affected_card_controller, affected_card) ){
			haste(player, affected_card, event);
		}
	}
	return 0;
}

static void silent_blade_oni_ability(int player, int card, int t_player){
	if( hand_count[t_player] == 0 ){
		return;
	}
	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a nonland card to cast for free.");
	int selected = -1;
	if( IS_AI(player) ){
		int playable[2][hand_count[t_player]];
		int pc = 0;
		CYCLE_ALL_CARDS(p, c,	{
									if( p == t_player && in_hand(p, c) && ! is_what(p, c, TYPE_LAND) ){
										int iid = get_original_internal_card_id(p, c);
										if( can_legally_play_iid(player, iid) ){
											playable[0][pc] = iid;
											playable[1][pc] = c;
											pc++;
										}
									}
								}
		);
		if( pc ){
			int s2 = select_card_from_zone(player, player, playable[0], pc, 0, AI_MAX_CMC, -1, &test);
			if( s2 > -1 ){
				selected = playable[1][s2];
			}
		}
	}
	else{
		int s2 = new_select_a_card(player, t_player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &test);
		if( s2 > -1 && can_legally_play_iid(player, get_original_internal_card_id(t_player, s2)) ){
			selected = s2;
		}
	}
	if( selected > -1 ){
		if( t_player == player ){
			play_card_in_hand_for_free(t_player, selected);
		}
		else{
			opponent_plays_card_in_your_hand_for_free(t_player, selected);
		}
	}
}

int card_silent_blade_oni(int player, int card, event_t event ){

	/* Silent-Blade Oni	|3|U|U|B|B
	 * Creature - Demon Ninja 6/5
	 * Ninjutsu |4|U|B
	 * Whenever ~ deals combat damage to a player, look at that player's hand.
	 You may cast a nonland card in it without paying that card's mana cost. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					if( times_damaged[p] ){
						silent_blade_oni_ability(player, card, p);
						times_damaged[p]--;
					}
				}
		);
	}

	return ninjutsu(player, card, event, MANACOST_XUB(4, 1, 1), CARD_ID_SILENT_BLADE_ONI);
}

int card_thromok_the_insatiable(int player, int card, event_t event ){
	/* Thromok the Insatiable	|3|R|G
	 * Legendary Creature - Hellion 0/0
	 * Devour X, where X is the number of creatures devoured this way */

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_SPELL && can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE) ){
		add_state(player, card, STATE_OUBLIETTED);
		int result = devouring(player, card);
		remove_state(player, card, STATE_OUBLIETTED);
		add_1_1_counters(player, card, result * result);
	}

	return 0;
}

int card_vela_the_night_clad(int player, int card, event_t event ){
/* CARD_ID_VELA_THE_NIGHT_CLAD	12536
Vela the Night-Clad	|4|U|B
Legendary Creature - Human Wizard 4/4
Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)
Other creatures you control have intimidate.
Whenever ~ or another creature you control leaves the battlefield, each opponent loses 1 life. */

	check_legend_rule(player, card, event);

	if (event == EVENT_ABILITIES
		&& affected_card_controller == player
		&& is_what(affected_card_controller, affected_card, TYPE_CREATURE)
		&& !is_humiliated(player, card)){
		intimidate(affected_card_controller, affected_card, event);
	}

	if( trigger_condition == TRIGGER_LEAVE_PLAY && ! is_humiliated(player, card) ){
		if( affect_me(player, card) && trigger_cause_controller == player && reason_for_trigger_controller == player &&
			in_play(trigger_cause_controller, trigger_cause) && is_what(trigger_cause_controller, trigger_cause, TYPE_CREATURE)
		  ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					lose_life(1-player, 1);
			}
		}
	}

	return 0;
}

// artifacts
int card_fractured_powerstone(int player, int card, event_t event){
	/* Fractured Powerstone	|2
	 * Artifact
	 * |T: Add |C to your mana pool.
	 * |T: Roll the planar die. Activate this ability only any time you could cast a sorcery. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_PLANAR_DICE
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, 0, NULL, NULL) ){
			choice = CHOICE_PLANAR_DICE;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_PLANAR_DICE && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Produce mana", 1, 1,
							"Cast the planar dice", 1, 5);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_PLANAR_DICE ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PLANAR_DICE ){
			casting_the_planar_dice(instance->parent_controller, instance->parent_card, internal_rand(6)+1);
		}
	}

	return 0;
}

int card_sai_of_the_shinobi(int player, int card, event_t event){
/* CARD_ID_SAI_OF_THE_SHINOBI	12531
Sai of the Shinobi	|1
Artifact - Equipment
Equipped creature gets +1/+1.
Whenever a creature enters the battlefield under your control, you may attach ~ to it.
Equip |2 */

	etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), TYPE_CREATURE, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		equip_target_creature(player, card, trigger_cause_controller, trigger_cause);
	}

	return vanilla_equipment(player, card, event, 2, 1, 1, 0, 0);
}

// Plane cards

int card_academy_at_tolaria_west(int player, int card, event_t event){
/* CARD_ID_ACADEMY_AT_TOLARIA_WEST	14714
*** Academy at Tolaria West	""
Plane - Dominaria
At the beginning of your end step, if you have no cards in hand, draw seven cards.
Whenever you roll |A, discard your hand. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( current_turn == player && eot_trigger(player, card, event) && hand_count[player] < 1 ){
		draw_cards(player, 7);
	}

	if( event == EVENT_CHAOS ){
		discard_all(player);
	}

	return generic_plane(player, card, event);
}

static int agyrem_legacy(int player, int card, event_t event){

	if( eot_trigger(player, card, event) ){
		card_instance_t *instance = get_card_instance(player, card);
		int owner = BYTE0(instance->targets[0].player);
		int csvid = cards_data[instance->targets[0].card].id;
		reanimate_mode_t mode = REANIMATEXTRA_RETURN_TO_HAND2;
		if( BYTE1(instance->targets[0].player) ){
			mode = REANIMATE_DEFAULT;
		}
		seek_grave_for_id_to_reanimate(owner, -1, owner, csvid, mode);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

static int agyrem_gfp_ability(int player, int card, event_t event){

	if( event == EVENT_GRAVEYARD_FROM_PLAY && in_play(affected_card_controller, affected_card) ){
		if( is_this_dying(affected_card_controller, affected_card) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			card_instance_t *instance = get_card_instance(player, card);
			if( instance->targets[11].player < 0 ){
				instance->targets[11].player = 0;
			}
			int pos = instance->targets[11].player;
			if( pos < 10 ){
				instance->targets[pos].player = 0;
				SET_BYTE0(instance->targets[pos].player) = get_owner(affected_card_controller, affected_card);
				instance->targets[pos].card = get_original_internal_card_id(affected_card_controller, affected_card);
				if( get_color(affected_card_controller, affected_card) & COLOR_TEST_WHITE ){
					SET_BYTE1(instance->targets[pos].player) = 1;
				}
				instance->targets[11].player++;
			}
		}
	}

	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && reason_for_trigger_controller == player ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( affect_me(p, c) && trigger_cause_controller == p && trigger_cause == c && instance->targets[11].player > 0 ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					int i;
					for(i=0; i<10; i++){
						if( instance->targets[i].player > -1 ){
							int legacy = create_legacy_effect(player, card, &agyrem_legacy);
							card_instance_t *leg = get_card_instance(player, legacy);
							leg->targets[0].player = instance->targets[i].player;
							leg->targets[0].card = instance->targets[i].card;
							instance->targets[i].player = instance->targets[i].card = -1;
						}
					}
			}
		}
	}

	return 0;
}

int card_agyrem(int player, int card, event_t event){
/* CARD_ID_AGYREM	14715
*** Agyrem	""
Plane - Ravnica
Whenever a |Swhite creature dies, return it to the battlefield under its owner's control at the beginning of the next end step.
Whenever a non|Swhite creature dies, return it to its owner's hand at the beginning of the next end step.
Whenever you roll |A, creatures can't attack you until a player planeswalks. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		nobody_can_attack_until_eot(player, card, 1-player, -1, -1);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int legacy = create_targetted_legacy_effect(player, card, &agyrem_gfp_ability, player, card);
		add_status(player, legacy, STATUS_INVISIBLE_FX);
	}

	return generic_plane(player, card, event);
}

static int effect_indestructible_as_long_as_has_divinity_counter(int player, int card, event_t event)
{
  card_instance_t* instance = get_card_instance(player, card);
  indestructible(instance->damage_target_player, instance->damage_target_card, event);

  if (event == EVENT_STATIC_EFFECTS && !count_counters(instance->damage_target_player, instance->damage_target_card, COUNTER_DIVINITY))
	{
	  kill_card(player, card, KILL_EXILE);
	  recalculate_all_cards_in_play();	// since this is an effect (kill_card_guts() calls this for permanents)
	}

  return 0;
}
int card_bant(int player, int card, event_t event){
/* CARD_ID_BANT	14716
*** Bant	""
Plane - Alara
All creatures have exalted. (Whenever a creature attacks alone, it gets +1/+1 until end of turn for each instance of exalted among permanents its controller controls.)
Whenever you roll |A, put a divinity counter on target |Sgreen, |Swhite, or |Sblue creature.
That creature has indestructible for as long as it has a divinity counter on it. */

	vanguard_card(player, card, event, 7, 20, 0);

	// All creatures have exalted.

	/* Planes use targets[1] *and* info_slot.  Fortunately, targets[3] for alternate main storage and targets[2] for tracking are free.  DAT_TRACK is normally
	 * incompatible with DAT_STORE_IN_TARGETS_3, but we get away with it since DAT_ATTACKS_ALONE means we'll never store more than 1 attacking index. */
	if (declare_attackers_trigger(player, card, event, DAT_ATTACKS_ALONE|DAT_STORE_IN_TARGETS_3|DAT_TRACK, 2, -1)){
		card_instance_t* instance = get_card_instance(player, card);
		int attacker = BYTE0(instance->targets[2].player);
		if (in_play(current_turn, attacker)){
			int c, amount = 0;
			for (c = 0; c < active_cards_count[current_turn]; ++c){
				if (in_play(current_turn, c) && is_what(current_turn, c, TYPE_CREATURE) && !is_humiliated(current_turn, c)){
					++amount;
				}
			}
			if (amount > 0){
				pump_until_eot(player, card, player, attacker, amount, amount);
			}
		}
	}

	/* Whenever you roll |C, put a divinity counter on target |Sgreen, |Swhite, or |Sblue creature. That creature is indestructible as long as it has a divinity
	 * counter on it. */
	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.required_color = COLOR_TEST_BLUE | COLOR_TEST_GREEN | COLOR_TEST_WHITE;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "Select target green, white or blue creature.", 0, GS_LITERAL_PROMPT) ){
			add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_DIVINITY);
			create_targetted_legacy_effect(player, card, effect_indestructible_as_long_as_has_divinity_counter,
										   instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return generic_plane(player, card, event);
}

int card_cliffside_market(int player, int card, event_t event){
/* CARD_ID_CLIFFSIDE_MARKET	14717
*** Cliffside Market	""
Plane - Mercadia
When you planeswalk to Cliffside Market or at the beginning of your upkeep, you may exchange life totals with target player.
Whenever you roll |A, exchange control of two target permanents that share a card type. */

	vanguard_card(player, card, event, 7, 20, 0);

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_PLANESWALK_IN || event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance( player, card );

		instance->targets[0].player = 1-player;
		instance->targets[0].card = -1;
		instance->number_of_targets = 1;

		if( would_valid_target(&td) ){
			int ai_choice = 0;
			if( life[1-player] < life[player] ){
				ai_choice = 1;
			}
			int choice = do_dialog(player, player, card, -1, -1, " Exchange life totals with opponent\n Pass", ai_choice);
			if( choice == 0 ){
				int amount = life[player];
				set_life_total(player, life[1-player]);
				set_life_total(1-player, amount);
			}
		}
	}

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allowed_controller = player;
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
			instance->number_of_targets = 1;
			target_definition_t td1;
			default_target_definition(player, card, &td1, get_type(instance->targets[0].player, instance->targets[0].card));
			td1.allowed_controller = 1-player;
			td1.preferred_controller = 1-player;
			if( new_pick_target(&td1, "TARGET_PERMANENT", 1, 0) ){
				exchange_control_of_target_permanents(player, card, instance->targets[0].player, instance->targets[0].card,
														instance->targets[1].player, instance->targets[1].card);
			}
		}
		instance->number_of_targets = 0;
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return generic_plane(player, card, event);
}

int card_eloren_wilds(int player, int card, event_t event){
/* CARD_ID_ELOREN_WILDS	14718
*** Eloren Wilds	""
Plane - Shandalar
Whenever a player taps a permanent for mana, that player adds one mana to his or her mana pool of any type that permanent produced.
Whenever you roll |A, target player can't cast spells until a player planeswalks. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			target_player_cant_cast_type(player, card, instance->targets[0].player, TYPE_CREATURE|TYPE_ENCHANTMENT|TYPE_ARTIFACT|TARGET_TYPE_PLANESWALKER|
			                             TYPE_SPELL|TYPE_EFFECT);
		}
		instance->number_of_targets = 0;
	}

	if( event == EVENT_TAP_CARD || event == EVENT_COUNT_MANA ){
		card_mana_flare(player, card, event);
	}

	return generic_plane(player, card, event);
}

int card_feeding_grounds(int player, int card, event_t event){
/* CARD_ID_FEEDING_GROUNDS	14719
*** Feeding Grounds	""
Plane - Muraganda
|SRed spells cost |1 less to cast.
|SGreen spells cost |1 less to cast.
Whenever you roll |A, put X +1/+1 counters on target creature, where X is that creature's converted mana cost. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			add_1_1_counters(instance->targets[0].player, instance->targets[0].card,
							get_cmc(instance->targets[0].player, instance->targets[0].card));
		}
		instance->number_of_targets = 0;
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player ){
		if( get_color(affected_card_controller, affected_card) & COLOR_TEST_GREEN ){
			COST_COLORLESS--;
		}
		if( get_color(affected_card_controller, affected_card) & COLOR_TEST_RED ){
			COST_COLORLESS--;
		}
	}

	return generic_plane(player, card, event);
}

int card_fields_of_summer(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		gain_life(player, 10);
	}

	if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), NULL) ){
		gain_life(player, 2);
	}

	return generic_plane(player, card, event);
}

// glimmervoid basing --> skipped

int card_goldmeadow2(int player, int card, event_t event){
	/* *** Goldmeadow	""
	 * Plane - Lorwyn
	 * Whenever a land enters the battlefield, that land's controller puts three 0/1 |Swhite Goat creature tokens onto the battlefield.
	 * Whenever you roll |C, put a 0/1 |Swhite Goat creature token onto the battlefield. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		generate_token_by_id(player, card, CARD_ID_GOAT);
	}

	etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_LAND, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		generate_tokens_by_id(player, card, CARD_ID_GOAT, 3);
	}

	return generic_plane(player, card, event);
}

int card_grixis(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");
		reanimate_test_from_any_graveyard(player, card, &test, 1, REANIMATE_DEFAULT);
	}

	if( ! IS_GAA_EVENT(event) ){
		return generic_plane(player, card, event);
	}

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_sorcery_be_played(player, event) ){
			if( has_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX)) ){
				return 1;
			}
			return can_cast_creature_from_grave(player, 5, event);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0,
						has_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX)) ? 1 : 0,
						can_cast_creature_from_grave(player, 5, event)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Cast the Planar Dice", abils[1], current_phase == PHASE_MAIN2 ? 5 : 1,
							"Use Unearth", abils[2], current_phase < PHASE_DECLARE_ATTACKERS ? 5 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == 1 ){
			return generic_plane(player, card, event);
		}
		if( choice == 2 ){
			int selected = -1;
			if( ! IS_AI(player) ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_CREATURE, "Select a black, blue or red creature card.");
				test.color = COLOR_TEST_BLACK | COLOR_TEST_BLUE | COLOR_TEST_RED;
				selected = select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &test, 0);
			}
			else{
				selected = can_cast_creature_from_grave(player, 6, event);
			}
			if( selected > -1 ){
				int id = cards_data[get_grave(player)[selected]].id;
				charge_mana_from_id(player, -1, event, id);
				if( spell_fizzled != 1 ){
					instance->info_slot = 10;
				}
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 10 ){
			int selected = validate_target_from_grave_source(player, card, player, 0);
			if( selected > -1 ){
				reanimate_permanent(player, -1, player, selected, REANIMATE_UNEARTH);
			}
		}
		else{
			return generic_plane(player, card, event);
		}
	}

	return 0;
}

int card_immersturm(int player, int card, event_t event){
	/* *** Immersturm	""	0x200a809
	 * Plane - Valla
	 * Whenever a creature enters the battlefield, that creature's controller may have it deal damage equal to its power to any target of their choice.
	 * Whenever you roll |A, exile target creature, then return it to the battlefield under its owner's control. */
	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			blink_effect(instance->targets[0].player, instance->targets[0].card, 0);
		}
		instance->number_of_targets = 0;
	}

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		pandemonium_effect(trigger_cause_controller, trigger_cause);
	}

	return generic_plane(player, card, event);
}

int card_isle_of_vesuva(int player, int card, event_t event){
	/* *** Isle of Vesuva	""
	 * Plane - Dominaria
	 * Whenever a nontoken creature enters the battlefield, its controller puts a token onto the battlefield that's a copy of that creature.
	 * Whenever you roll |C, destroy target creature and all other creatures with the same name as that creature. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.id = get_id(instance->targets[0].player, instance->targets[0].card);
			new_manipulate_all(player, card, 2, &this_test, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		copy_token_definition(player, card, &token, trigger_cause_controller, trigger_cause);
		generate_token(&token);
	}

	return generic_plane(player, card, event);
}

static int izzet_steam_maze_legacy(int player, int card, event_t event){

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_SPELL);
		if( new_make_test_in_play(affected_card_controller, affected_card, -1, &this_test) ){
			COST_COLORLESS-=3;
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_izzet_steam_maze(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		if( ! check_special_flags(trigger_cause_controller, trigger_cause, SF_NOT_CAST) ){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &test) ){
				copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
			}
		}
	}

	if( event == EVENT_CHAOS ){
		create_legacy_effect(player, card, &izzet_steam_maze_legacy);
	}

	return generic_plane(player, card, event);
}

int card_krosa2(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		produce_mana_multi(player, 0, 1, 1, 1, 1, 1);
	}

	boost_creature_type(player, card, event, -1, 2, 2, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);

	return generic_plane(player, card, event);
}

int card_lethe_lake(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			mill(instance->targets[0].player, 10);
		}
		instance->number_of_targets = 0;
	}

	if( get_card_instance( player, card )->targets[1].player != 66 ){
		upkeep_trigger_ability(player, card, event, player);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		mill(player, 10);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return generic_plane(player, card, event);
}

int card_llanowar2(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_ACTIVATE ){
		get_card_instance(player, card)->info_slot = 0;	// generic_plane() sets this

		int choice = 0;
		int can_tap_creature = permanents_you_control_can_tap_for_mana(player, card, EVENT_CAN_ACTIVATE, TYPE_CREATURE, -1, COLOR_GREEN, 2);
		if( can_tap_creature ){
			if( generic_plane(player, card, EVENT_CAN_ACTIVATE)){
				choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Cast the Planar Dice\n Cancel", 1);
			}
		}
		else{
			choice = 1;
		}

		if( choice == 0 ){
			return permanents_you_control_can_tap_for_mana(player, card, event, TYPE_CREATURE, -1, COLOR_GREEN, 2);
		}
		else if( choice == 1 ){
			return generic_plane(player, card, event);
		}
		else{
			spell_fizzled = 1;
		}
		return 0;
	}

	int result = permanents_you_control_can_tap_for_mana(player, card, event, TYPE_CREATURE, -1, COLOR_GREEN, 2);
	if( event == EVENT_CAN_ACTIVATE && result ){
		return 1;
	}

	if( event == EVENT_CHAOS ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
	}

	return generic_plane(player, card, event);
}

int card_minamo2(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a blue card.");
		this_test.color = COLOR_TEST_BLUE;

		APNAP(p,{
					if( new_special_count_grave(p, &this_test) ){
						new_global_tutor(p, p, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
					}
				}
		);
	}

	if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, NULL) ){
		draw_some_cards_if_you_want(player, card, trigger_cause_controller, 1);
	}

	return generic_plane(player, card, event);
}

// murasa --> skipped

int card_naar_isle(int player, int card, event_t event){
	/* *** Naar Isle	""	0x200a827
	 * Plane - Wildfire
	 * At the beginning of your upkeep, put a flame counter on Naar Isle, then Naar Isle deals damage to you equal to the number of flame counters on it.
	 * Whenever you roll |A, Naar Isle deals 3 damage to target player or planeswalker. */
	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER", 0, 0) ){
			damage_target0(player, card, 3);
		}
		instance->number_of_targets = 0;
	}

	if( get_card_instance( player, card )->targets[1].player != 66 ){
		upkeep_trigger_ability(player, card, event, player);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counter(player, card, COUNTER_FLAME);
		damage_player(player, count_counters(player, card, COUNTER_FLAME), player, card);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return generic_plane(player, card, event);
}

int card_naya2(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.required_color = COLOR_TEST_RED | COLOR_TEST_GREEN | COLOR_TEST_WHITE;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			int amount = count_subtype(player, TYPE_LAND, -1);
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, amount, amount);
		}
		instance->number_of_targets = 0;
	}

	if( current_turn == player ){
		if(current_phase == PHASE_MAIN1 || current_phase == PHASE_MAIN2 ){
			land_can_be_played &= ~LCBP_LAND_HAS_BEEN_PLAYED;
		}
	}

	return generic_plane(player, card, event);
}

int card_otaria(int player, int card, event_t event){

	/* *** Otaria	""
	 * Plane - Dominaria
	 * Instant and sorcery cards in graveyards have flashback. The flashback cost is equal to the card's mana cost.
	 * Whenever you roll |C, take an extra turn after this one. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS){
		time_walk_effect(player, card);
	}

	if( ! IS_GAA_EVENT(event) ){
		return generic_plane(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( has_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX)) && can_sorcery_be_played(player, event) ){
			return 1;
		}
		return can_cast_spell_from_grave(player, 1);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		if( has_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX)) && can_sorcery_be_played(player, event) ){
			if( can_cast_spell_from_grave(player, 1) ){
				choice = do_dialog(player, player, card, -1, -1, " Cast the Planar Dice\n Play a spell from grave\n Cancel", 1);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 0 ){
			charge_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX));
			if( spell_fizzled != 1 ){
				instance->info_slot = internal_rand(6)+1;
				add_counter(player, card, COUNTER_VORTEX);
			}
		}
		else if( choice == 1 ){
				const int *grave = get_grave(player);
				char msg[100] = "Select a spell to Flashback.";
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_SPELL, msg);
				this_test.type_flag = F1_NO_CREATURE;
				int selected = new_select_a_card(player, player, TUTOR_FROM_GRAVE, 0, AI_MAX_VALUE, -1, &this_test);
				if( selected != -1 ){
					if( can_legally_play_iid(player, grave[selected]) && charge_mana_from_id(player, -1, event, cards_data[grave[selected]].id) ){
						play_card_in_grave_for_free_and_exile_it(player, player, selected);
						cant_be_responded_to = 1;
						instance->info_slot = 10;
					}
					else{
						 spell_fizzled = 1;
					}
				}
				else{
					spell_fizzled = 1;
				}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot != 10 ){
			casting_the_planar_dice(instance->parent_controller, instance->parent_card, instance->info_slot);
		}
	}

	return 0;
}

int card_panopticon(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_PLANESWALK_IN || event == EVENT_CHAOS){
		draw_cards(player, 1);
	}

	if( event == EVENT_DRAW_PHASE && current_turn == player ){
		event_result++;
	}

	return generic_plane(player, card, event);
}

int card_pools_of_becoming(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS){
		int i;
		for(i=0; i<3; i++){
			int id = get_new_plane(CARD_ID_POOLS_OF_BECOMING);
			card_ptr_t* c = cards_ptr[id];
			char buffer[100];
			scnprintf(buffer, 100, "%s", c->name);
			do_dialog(HUMAN, player, card, -1, -1, buffer, 0);

			int iid = get_internal_card_id_from_csv_id(id);
			int card_added = add_card_to_hand(player, iid);
			card_instance_t* plane = get_card_instance(player, card_added);
			plane->state |= STATE_INVISIBLE;	--hand_count[player];
			call_card_function_i(plane, player, card_added, EVENT_CHAOS);
			obliterate_card(player, card_added);
		}
	}

	if( current_turn == player && eot_trigger(player, card, event) ){
		char msg[100] = "Select a card to put on bottom.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, msg);
		int amount = 0;
		while( hand_count[player] > 0 ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &this_test);
				put_on_bottom_of_deck(player, selected);
				amount++;
		}
		draw_cards(player, amount);
	}

	return generic_plane(player, card, event);
}

int card_ravens_run(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( target_available(player, card, &td) > 2 ){
			instance->number_of_targets = 0;
			int trgs = 0;
			while( trgs < 3 ){
					if( new_pick_target(&td, "TARGET_CREATURE", trgs, 0) ){
						state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
						trgs++;
					}
			}
			while( trgs > 0 ){
					state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 0);
					add_minus1_minus1_counters(instance->targets[trgs].player, instance->targets[trgs].card, trgs);
					trgs--;
			}
		}
		instance->number_of_targets = 0;
	}

	if (event == EVENT_ABILITIES && is_what(affected_card_controller, affected_card, TYPE_CREATURE)){
		wither(affected_card_controller, affected_card, event);
	}

	return generic_plane(player, card, event);
}

int card_sanctum_of_serra(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_PLANESWALK_OUT){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.type_flag = 1;
		new_manipulate_all(player, card, 2, &this_test, KILL_DESTROY);
	}

	if( event == EVENT_CHAOS ){
		int ai_choice = 0;
		if( life[player] > 20 ){
			ai_choice = 1;
		}
		int choice = do_dialog(player, player, card, -1, -1, " Set your life points to 20\n Pass", ai_choice);
		if( choice == 0 ){
			set_life_total(player, 20);
		}
	}

	return generic_plane(player, card, event);
}

int card_sea_of_sand(int player, int card, event_t event){

	/* *** Sea of Sand	""
	 * Plane - Rabiah
	 * Players reveal each card they draw.
	 * Whenever a player draws a land card, that player gains 3 life.
	 * Whenever a player draws a nonland card, that player loses 3 life.
	 * Whenever you roll |C, put target permanent on top of its owner's library. */

	vanguard_card(player, card, event, 7, 20, 0);

	if (card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY)){
		int iid = get_original_internal_card_id(trigger_cause_controller, trigger_cause);	// Original, since that'll stay in place even if another trigger makes it be discarded
		if( iid != -1 ){
			reveal_card_iid(player, card, iid);
			if( is_what(-1, iid, TYPE_LAND) ){
				gain_life(player, 3);
			}
			else{
				lose_life(player, 3);
			}
		}
	}

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_PERMANENT", 0, 0) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return generic_plane(player, card, event);
}

int card_shiv2(int player, int card, event_t event){

	/* *** Shiv	""
	 * Plane - Dominaria
	 * All creatures have "|R: This creature gets +1/+0 until end of turn."
	 * Whenever you roll |C, put a 5/5 |Sred Dragon creature token with flying onto the battlefield. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_DRAGON, &token);
		token.color_forced = COLOR_TEST_RED;
		token.pow = 5;
		token.tou = 5;
		generate_token(&token);
	}

	if( ! IS_GAA_EVENT(event) ){
		return generic_plane(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( has_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX)) && can_sorcery_be_played(player, event) ){
			return 1;
		}
		if( has_mana(player, COLOR_RED, 1) ){
			int count = 0;
			while( count < active_cards_count[player] ){
					if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) && can_use_activated_abilities(player, count) ){
						return 1;
					}
					count++;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int choice = 0;
		int pump = 0;
		if( has_mana(player, COLOR_RED, 1) ){
			int count = 0;
			while( count < active_cards_count[player] ){
					if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) && can_use_activated_abilities(player, count) ){
						pump = 1;
						break;
					}
					count++;
			}
		}
		if( has_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX))  && can_sorcery_be_played(player, event) ){
			if( pump == 1 ){
				choice = do_dialog(player, player, card, -1, -1, " Cast the Planar Dice\n Pump a creature\n Cancel", 1);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 0 ){
			charge_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX));
			if( spell_fizzled != 1 ){
				instance->info_slot = internal_rand(6)+1;
				add_counter(player, card, COUNTER_VORTEX);
			}
		}
		else if( choice == 1 ){
				charge_mana(player, COLOR_RED, 1);
				if( spell_fizzled != 1 && pick_target(&td, "TARGET_CREATURE") ){
					instance->number_of_targets = 1;
					if( !can_use_activated_abilities(player, instance->targets[0].card) ){
						spell_fizzled = 1;
						instance->info_slot = 10;
					}
				}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 10 ){
			if( valid_target(&td)  ){
				pump_until_eot(player, instance->parent_card, player, instance->targets[0].card, 1, 0);
			}
		}
		else{
			casting_the_planar_dice(player, instance->parent_card, instance->info_slot);
		}
	}

	return 0;
}

int card_skybreen(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	reveal_top_card(player, card, event);

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player ){
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			if (is_what(affected_card_controller, affected_card, get_type(-1, deck[0])) && !is_what(affected_card_controller, affected_card, TYPE_LAND)){
				infinite_casting_cost();
			}
		}
	}

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			lose_life(instance->targets[0].player, hand_count[instance->targets[0].player]);
			instance->number_of_targets = 1;
		}
	}

	return generic_plane(player, card, event);
}

int card_sokenzan(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	boost_subtype(player, card, event, -1, 1,1, 0,SP_KEYWORD_HASTE, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);

	if( event == EVENT_CHAOS ){
		int count;
		card_instance_t* inst;
		for (count = 0; count < active_cards_count[current_turn]; ++count){
			if( (inst = in_play(current_turn, count)) && is_what(current_turn, count, TYPE_CREATURE) && (inst->state & STATE_ATTACKED) ){
				untap_card(current_turn, count);
			}
		}
	}

	return generic_plane(player, card, event);
}

int card_stronghold_furnace(int player, int card, event_t event){
	/* *** Stronghold Furnace	""	0x200a859
	 * Plane - Rath
	 * If a source would deal damage to a permanent or player, it deals double that damage to that permanent or player instead.
	 * Whenever you roll |A, Stronghold Furnace deals 1 damage to any target. */
	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0) ){
			damage_creature_or_player(player, card, event, 1);
		}
		instance->number_of_targets = 0;
	}

	card_instance_t* damage = damage_being_dealt(event);
	if (damage)
		damage->info_slot *= 2;

	return generic_plane(player, card, event);
}

int card_tazeem(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_BLOCK_LEGALITY ){
		event_result = 1;
	}

	if( event == EVENT_CHAOS ){
		draw_cards(player, count_subtype(player, TYPE_LAND, -1));
	}

	return generic_plane(player, card, event);
}

static int aether_flues_effect(int player, int card){
	int card_added = -1;
	if( count_subtype(player, TYPE_CREATURE, -1) > 0 ){
		int selected = pick_creature_for_sacrifice(player, card, 0);
		if( selected != -1 ){
			card_added = metamorphosis(player, selected, TYPE_CREATURE, KILL_SACRIFICE);
		}
	}
	return card_added;
}

int card_the_aether_flues(int player, int card, event_t event){
//The Aether Flues
	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_PLANESWALK_IN ){
		aether_flues_effect(player, card);
	}

	if( get_card_instance( player, card )->targets[1].player != 66 ){
		upkeep_trigger_ability(player, card, event, player);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		aether_flues_effect(player, card);
	}

	if( event == EVENT_CHAOS ){
		char msg[100] = "Select a creature card to put into play.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, msg);
		this_test.zone = TARGET_ZONE_HAND;
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return generic_plane(player, card, event);
}

// the dark barony --> skipped

int card_the_eon_fog(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	// Skip-untap effect handled in engine.c:untap_phase()

	if( event == EVENT_CHAOS ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
	}

	return generic_plane(player, card, event);
}

int card_the_fourth_sphere(int player, int card, event_t event){
	/* *** The Fourth Sphere	""
	 * Plane - Phyrexia
	 * At the beginning of your upkeep, sacrifice a non|Sblack creature.
	 * Whenever you roll |C, put a 2/2 |Sblack Zombie creature token onto the battlefield. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( get_card_instance( player, card )->targets[1].player != 66 ){
		upkeep_trigger_ability(player, card, event, player);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a nonblack creature to sacrifice.");
		test.color = COLOR_TEST_BLACK;
		test.color = DOESNT_MATCH;
		new_sacrifice(player, card, player, SAC_NO_CANCEL | SAC_CAUSED, &test);
	}

	if( event == EVENT_CHAOS ){
		generate_token_by_id(player, card, CARD_ID_ZOMBIE);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return generic_plane(player, card, event);
}

int card_the_great_forest(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 2, KEYWORD_TRAMPLE, 0);
		}
		instance->number_of_targets = 0;
	}

	return generic_plane(player, card, event);
}

int card_the_hippodrome(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	boost_creature_type(player, card, event, -1, -5, 0, 0, BCT_INCLUDE_SELF);

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.power_requirement = 0;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return generic_plane(player, card, event);
}

static void the_maelstrom_effect(int player){
	int *deck = deck_ptr[player];
	if( deck[0] != -1 ){
		show_deck(player, deck, 1, "Here's the first card of deck", 0, 0x7375B0 );
		if( cards_data[deck[0]].type & TYPE_PERMANENT ){
			put_into_play_a_card_from_deck(player, player, 0);
		}
		else{
			 deck[count_deck(player)] = deck[0];
			 remove_card_from_deck(player, 0);
		}
	}
}


int card_the_maelstrom(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_PLANESWALK_IN ){
		the_maelstrom_effect(player);
	}

	if( get_card_instance( player, card )->targets[1].player != 66 ){
		upkeep_trigger_ability(player, card, event, player);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		the_maelstrom_effect(player);
	}

	if( event == EVENT_CHAOS ){
		char msg[100] = "Select a permanent card to put into play.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, msg);
		if( new_special_count_grave(player, &this_test) > 0 ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return generic_plane(player, card, event);
}

int card_turri_island(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			COST_COLORLESS-=2;
		}
	}

	if( event == EVENT_CHAOS ){
		int *deck = deck_ptr[player];
		int amount = 3;
		if( amount > count_deck(player) ){
			amount = count_deck(player);
		}
		if( amount > 0 ){
			show_deck( HUMAN, deck, amount, "Cards revealed by Turri Island.", 0, 0x7375B0 );
			while( amount > 0 ){
					if( is_what(-1, deck[0], TYPE_CREATURE) ){
						add_card_to_hand(player, deck[0]);
						remove_card_from_deck(player, 0);
						amount--;
					}
					else{
						mill(player, 1);
					}
					amount--;
			}
		}
	}

	return generic_plane(player, card, event);
}

int card_undercity_reaches(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	card_instance_t* damage = combat_damage_being_dealt(event);
	if (damage &&
		damage->damage_target_card == -1 && !check_special_flags(damage->damage_source_player, damage->damage_source_card, SF_ATTACKING_PWALKER) &&
		(damage->targets[3].player & TYPE_CREATURE)	// probably redundant to status check
	   ){
		card_instance_t *instance = get_card_instance(player, card);
		if (instance->targets[damage->damage_source_player + 4].card < 0){
			instance->targets[damage->damage_source_player + 4].card = 0;
		}
		instance->targets[damage->damage_source_player + 4].card++;
	}

	if( trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if (instance->targets[reason_for_trigger_controller + 4].card <= 0){
			return 0;
		}
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_AI(reason_for_trigger_controller);
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
			if (--instance->targets[reason_for_trigger_controller + 4].card > 0){
				instance->state &= ~STATE_PROCESSING;	// More optional triggers left.  Must be the first thing done during resolve trigger.
			}
			draw_cards(player, 1);
		}
		else if (event == EVENT_END_TRIGGER){
			instance->targets[reason_for_trigger_controller + 4].card = 0;
		}
	}

	if( event == EVENT_CHAOS ){
		create_legacy_effect(player, card, &card_spellbook);
	}

	return generic_plane(player, card, event);
}

int card_velis_vel(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
		int pump = 0;
		int i;
		for(i=0; i<2; i++){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) &&
						shares_creature_subtype(affected_card_controller, affected_card, i, count) )
					{
						pump++;
					}
					count++;
			}
		}
		if( pump > 0 ){
			pump--;
		}
		event_result+=pump;
	}

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_PROT_INTERRUPTS, 0);
		}
		instance->number_of_targets = 0;
	}

	return generic_plane(player, card, event);
}

// Planes - PC 2012

// akoum --> skipped

int card_aretopolis(int player, int card, event_t event){

	/* *** Aretopolis	""
	 * Plane - Kephalai
	 * When you planeswalk to Aretopolis or at the beginning of your upkeep, put a scroll counter on Aretopolis, then you gain life equal to the number of
	 * scroll counters on it.
	 * When Aretopolis has ten or more scroll counters on it, planeswalk.
	 * Whenever you roll |C, put a scroll counter on Aretopolis, then draw cards equal to the number of scroll counters on it. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_PLANESWALK_IN ){
		add_counter(player, card, COUNTER_SCROLL);
		gain_life(player, count_counters(player, card, COUNTER_SCROLL));
	}

	if( get_card_instance( player, card )->targets[1].player != 66 ){
		upkeep_trigger_ability(player, card, event, player);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counter(player, card, COUNTER_SCROLL);
		gain_life(player, count_counters(player, card, COUNTER_SCROLL));
		if( count_counters(player, card, COUNTER_SCROLL) >= 10 ){
			planeswalk_to(player, get_new_plane(get_actual_plane(player)), 0);
		}
	}

	if( event == EVENT_CHAOS ){
		add_counter(player, card, COUNTER_SCROLL);
		draw_cards(player, count_counters(player, card, COUNTER_SCROLL));
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return generic_plane(player, card, event);
}

// astral arena --> skipped

int card_bloodhill_bastion(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.illegal_type = TARGET_TYPE_TOKEN;
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target nontoken creature.", 0, GS_LITERAL_PROMPT) ){
			blink_effect(instance->targets[0].player, instance->targets[0].card, 0);
		}
		instance->number_of_targets = 0;
	}

	etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		pump_ability_until_eot(player, card, trigger_cause_controller, trigger_cause, 0, 0, KEYWORD_DOUBLE_STRIKE, SP_KEYWORD_HASTE);
	}

	return generic_plane(player, card, event);
}

int card_edge_of_malacol(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && affected_card_controller == player &&
		is_what(affected_card_controller, affected_card, TYPE_CREATURE)
	  ){
		card_instance_t *instance= get_card_instance(affected_card_controller, affected_card);
		instance->untap_status &= ~3;
		add_1_1_counters(affected_card_controller, affected_card, 2);
	}

	if( event == EVENT_CHAOS ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
	}

	return generic_plane(player, card, event);
}

int card_furnace_layer(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_PLANESWALK_IN ){
		int result = get_random_card_in_hand(player);
		if( result > -1 ){
			if( is_what(player, result, TYPE_LAND) ){
				lose_life(player, 3);
			}
			discard_card(player, result);
		}
	}

	if( get_card_instance( player, card )->targets[1].player != 66 ){
		upkeep_trigger_ability(player, card, event, player);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int result = get_random_card_in_hand(player);
		if( result > -1 ){
			if( is_what(player, result, TYPE_LAND) ){
				lose_life(player, 3);
			}
			discard_card(player, result);
		}
	}

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_LAND;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "Select target nonland permanent.", 0, GS_LITERAL_PROMPT) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return generic_plane(player, card, event);
}

int card_gavony(int player, int card, event_t event){

	/* *** Gavony	""
	 * Plane - Innistrad
	 * All creatures have vigilance.
	 * Whenever you roll |C, creatures you control gain indestructible until end of turn. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_ABILITIES && affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
		vigilance(affected_card_controller, affected_card, event);
	}

	if( event == EVENT_CHAOS ){
		pump_creatures_until_eot(player, card, player, 0, 0,0, 0,SP_KEYWORD_INDESTRUCTIBLE, NULL);
	}

	return generic_plane(player, card, event);
}

// glen elendra -> skipped

int card_grand_ossuary(int player, int card, event_t event){
/* *** Grand Ossuary	""
 * Plane - Ravnica
 * Whenever a creature dies, its controller distributes a number of +1/+1 counters equal to its power among any number of target creatures
 *		he or she controls.
 * Whenever you roll |C, each player exiles all creatures he or she controls and puts X 1/1 |Sgreen Saproling creature tokens onto
 *		the battlefield, where X is the total power of the creatures he or she exiled this way. Then planeswalk. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( in_play(affected_card_controller, affected_card) && is_this_dying(affected_card_controller, affected_card) ){
			if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
				card_instance_t *instance = get_card_instance( player, card );
				if( instance->targets[11].player < 0 ){
					instance->targets[11].player = 0;
				}
				int pos = BYTE0(instance->targets[11].player);
				if( pos < 10 ){
					instance->targets[pos].player = affected_card_controller;
					instance->targets[pos].card = get_power(affected_card_controller, affected_card);
					SET_BYTE0(instance->targets[11].player)++;
				}
			}
		}
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( result ){
		card_instance_t *instance = get_card_instance( player, card );
		int amount = BYTE0(result);
		int i;
		for(i=0; i<amount; i++){
			int t_player = instance->targets[i].player;
			int qty = instance->targets[i].card;

			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.allowed_controller = td.preferred_controller = t_player;
			td.who_chooses = t_player;
			td.allow_cancel = 0;

			while( qty && can_target(&td) ){
					char msg[100];
					scnprintf(msg, 100, "Select target creature you control (%d counter%s left to distribute).", qty, qty > 1 ? "s" : "");
					if( new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
						add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
						instance->number_of_targets = 0;
						qty--;
					}
			}
		}
	}

	if( event == EVENT_CHAOS ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SAPROLING, &token);
		APNAP(p,{
					test_definition_t this_test;
					default_test_definition(&this_test, TYPE_CREATURE);
					int sapro = check_battlefield_for_special_card(player, card, p, CBFSC_GET_TOTAL_POW, &this_test);
					new_manipulate_all(player, card, p, &this_test, KILL_EXILE);
					token.t_player = p;
					token.qty = sapro;
					generate_token(&token);
				}
		);
		planeswalk_to(player, get_new_plane(get_actual_plane(player)), 0);
	}

	return generic_plane(player, card, event);
}

static void gotd_effect(int player){
	test_definition_t this_test;
	default_test_definition(&this_test, TYPE_CREATURE);
	reveal_card_until_you_reveal_test(player, player, &this_test, RCUYRT_PUT_TEST_INTO_PLAY | RCUYRT_PUT_REST_ON_BOTTOM_RANDOM);
}

int card_grove_of_the_dreampods(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_PLANESWALK_IN ){
		gotd_effect(player);
	}

	if( get_card_instance( player, card )->targets[1].player != 66 ){
		upkeep_trigger_ability(player, card, event, player);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		gotd_effect(player);
	}

	if( event == EVENT_CHAOS ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) && ! graveyard_has_shroud(player) ){
			char msg[100] = "Select a creature card.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, msg);
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return generic_plane(player, card, event);
}

int card_hedron_fields_of_agadeem(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if ((event == EVENT_ATTACK_LEGALITY || event == EVENT_BLOCK_LEGALITY)
		&& get_power(affected_card_controller, affected_card) >= 7){
		event_result = 1;
	}

	if( event == EVENT_CHAOS ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ELDRAZI, &token);
		token.pow = 7;
		token.tou = 7;
		generate_token(&token);
	}
	return generic_plane(player, card, event);
}

int card_jund2(int player, int card, event_t event){
	/* *** Jund	""
	 * Plane - Alara
	 * Whenever a player casts a |Sblack, |Sred, or |Sgreen creature spell, it gains devour 5.
	 * Whenever you roll |C, put two 1/1 |Sred Goblin creature tokens onto the battlefield. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		generate_tokens_by_id(player, card, CARD_ID_GOBLIN, 2);
	}

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && reason_for_trigger_controller == player ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.color = COLOR_TEST_BLACK | COLOR_TEST_GREEN | COLOR_TEST_RED;
		if( new_make_test_in_play(trigger_cause_controller, trigger_cause, -1, &test) ){
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					add_state(trigger_cause_controller, trigger_cause, STATE_OUBLIETTED);
					int result = devouring(trigger_cause_controller, trigger_cause);
					remove_state(trigger_cause_controller, trigger_cause, STATE_OUBLIETTED);
					add_1_1_counters(trigger_cause_controller, trigger_cause, 5*result);
			}
		}
	}

	return generic_plane(player, card, event);
}

int card_kessig2(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_ability_t pump;
			default_pump_ability_definition(player, card, &pump, 2, 2, KEYWORD_TRAMPLE, 0);
			pump.paue_flags = PAUE_END_AT_EOT | PAUE_RESET_FORCED_SUBTYPES_AT_EOT;
			pump_ability(player, card, instance->targets[0].player, instance->targets[0].card, &pump);
		}
		instance->number_of_targets = 0;
	}

	if( event == EVENT_PREVENT_DAMAGE && affected_card_controller == player ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->info_slot > 0 ){
			if( is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE) &&
				! has_subtype(damage->damage_source_player, damage->damage_source_card, SUBTYPE_WEREWOLF) )
			{
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
	}

	return generic_plane(player, card, event);
}

int card_kharasha_foothills(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int pow = 0;
		int sacced[151] = {0};
		while( new_can_sacrifice_as_cost(player, card, &test) ){
				int sac = new_sacrifice(player, card, player, SAC_AS_COST | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
				if( sac ){
					sacced[BYTE3(sac)] = 1;
				}
				else{
					break;
				}
		}
		int c;
		for(c = active_cards_count[player]-1; c > -1; c--){
			if( sacced[c] ){
				pow += get_power(player, c);
				kill_card(player, c, KILL_SACRIFICE);
			}
		}

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) ){
			if( new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, pow, player, card);
			}
		}
		instance->number_of_targets = 0;
	}
	return generic_plane(player, card, event);
}

int card_kilnspire_district(int player, int card, event_t event){
	/* *** Kilnspire District	""	0x200a8d6
	 * Plane - Ravnica
	 * When you planeswalk to Kilnspire District or at the beginning of your precombat main phase, put a charge counter on Kilnspire District, then add |R for each charge counter on it.
	 * Whenever you roll |A, you may pay |X. If you do, Kilnspire District deals X damage to any target. */
	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_PLANESWALK_IN ){
		add_counter(player, card, COUNTER_CHARGE);
		produce_mana(player, COLOR_RED, count_counters(player, card, COUNTER_CHARGE));
	}

	if( beginning_of_phase(player, card, event, player, -1, PHASE_MAIN1) ){
		add_counter(player, card, COUNTER_CHARGE);
		produce_mana(player, COLOR_RED, count_counters(player, card, COUNTER_CHARGE));
	}

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) ){
			charge_mana(player, COLOR_COLORLESS, -1);
			if( spell_fizzled != 1 && pick_target(&td, "TARGET_ANY") ){
				damage_creature_or_player(player, card, event, x_value);
			}
		}
		instance->number_of_targets = 0;
	}

	return generic_plane(player, card, event);
}

int card_lair_of_the_ashen_idol(int player, int card, event_t event){
	/* *** Lair of the Ashen Idol	""
	 * Plane - Azgol
	 * At the beginning of your upkeep, sacrifice a creature. If you can't, planeswalk.
	 * Whenever you roll |C, any number of target players each put a 2/2 |Sblack Zombie creature token onto the battlefield. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( get_card_instance( player, card )->targets[1].player != 66 ){
		upkeep_trigger_ability(player, card, event, player);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		if( ! new_sacrifice(player, card, player, SAC_NO_CANCEL, &test) ){
			planeswalk_to(player, get_new_plane(get_actual_plane(player)), 0);
		}
	}

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance( player, card );

		instance->targets[0].player = player;
		instance->targets[0].card = -1;
		int can_target_self = would_valid_target(&td);

		instance->targets[0].player = 1-player;
		instance->targets[0].card = -1;
		int can_target_opp = would_valid_target(&td);

		int choice = DIALOG(player, card, EVENT_ACTIVATE,
							DLG_NO_STORAGE, DLG_NO_CANCEL,
							"You get a Zombie", can_target_self, 2,
							"Opponent gets a Zombie", can_target_opp, 0,
							"Both players get Zombies", can_target_self && can_target_opp, 0,
							"Nobody gets a Zombie", 1, 1);

		if( choice == 1 || choice == 3 ){
			generate_token_by_id(player, card, CARD_ID_ZOMBIE);
		}
		if( choice == 2 || choice == 3 ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
			token.t_player = 1-player;
			generate_token(&token);
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return generic_plane(player, card, event);
}

int card_mount_keralia(int player, int card, event_t event){
/* Mount Keralia	""
 * Plane - Regatha
 * At the beginning of your end step, put a pressure counter on Mount Keralia.
 * When you planeswalk away from Mount Keralia, it deals damage equal to the number of pressure counters on it to each creature and
 *		each planeswalker.
 * Whenever you roll |C, prevent all damage that planes named Mount Keralia would deal this game to permanents you control. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		create_id_legacy(player, card, -1, -1, 0);
	}

	if( event == EVENT_PLANESWALK_OUT ){
		int amount = count_counters(player, card, COUNTER_PRESSURE);
		APNAP(p,{
					if( ! check_for_id_legacy(p, get_id(player, card)) ){
						int count = active_cards_count[p]-1;
						while( count > -1 ){
								if( in_play(p, count) ){
									if( is_what(p, count, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER) ){
										damage_creature(p, count, amount, player, card);
									}
								}
								count--;
						}
					}
				}
		);
	}

	if( eot_trigger(player, card, event) ){
		add_counter(player, card, COUNTER_PRESSURE);
	}

	return generic_plane(player, card, event);
}

int card_nephalia2(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		char msg[100] = "Select a card to return to hand.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, msg);

		if( ! graveyard_has_shroud(player) && count_graveyard(player) > 0 ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 1, AI_MAX_VALUE, &this_test);
		}
	}

	if( eot_trigger(player, card, event) ){
		const int *grave = get_grave(player);
		mill(player, 7);
		if( grave[0] != -1 ){
			int count = count_graveyard(player);
			int rnd = 0;
			if( count > 1 ){
				rnd = internal_rand(count);
			}
			add_card_to_hand(player, grave[rnd]);
			remove_card_from_grave(player, rnd);
		}
	}

	return generic_plane(player, card, event);
}

int card_norns_dominion(int player, int card, event_t event){

	/* Norn's Dominion	""
	 * Plane - New Phyrexia
	 * When you planeswalk away from Norn's Dominion, destroy each nonland permanent without a fate counter on it, then remove all fate counters from all
	 * permanents.
	 * Whenever you roll |C, you may put a fate counter on target permanent. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_PLANESWALK_OUT ){
		oblivion_stone_effect();
	}

	if(  event == EVENT_CHAOS  ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_LAND;
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "Select target nonland permanent.", 0, GS_LITERAL_PROMPT) ){
			add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_FATE);
		}
		instance->number_of_targets = 0;
	}

	return generic_plane(player, card, event);
}

int card_onakke_catacomb(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_SET_COLOR && affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
		event_result = COLOR_TEST_BLACK;
	}

	if (event == EVENT_ABILITIES && is_what(affected_card_controller, affected_card, TYPE_CREATURE)){
		deathtouch(affected_card_controller, affected_card, event);
	}

	if( event == EVENT_CHAOS ){
		pump_subtype_until_eot(player, card, player, -1, 1, 0, KEYWORD_FIRST_STRIKE, 0);
	}

	return generic_plane(player, card, event);
}

int card_orochi_colony(int player, int card, event_t event){

	/* Orochi Colony	""
	 * Plane - Kamigawa
	 * Whenever a creature you control deals combat damage to a player, you may search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library.
	 * Whenever you roll |C, target creature is unblockable this turn. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( subtype_deals_damage(player, card, event, player, -1, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_TRIGGER_OPTIONAL) ){
		tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 1);
	}

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
		}
		instance->number_of_targets = 0;
	}

	return generic_plane(player, card, event);
}

int card_orzhova2(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if (event == EVENT_CHAOS){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card to exile.");
		if( new_special_count_grave(1-player, &this_test) && ! graveyard_has_shroud(1-player) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MAX_VALUE, &this_test);
		}
	}

	if( event == EVENT_PLANESWALK_OUT ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		APNAP(p,{ new_reanimate_all(p, -1, p, &test, REANIMATE_DEFAULT); };);
	}

	return generic_plane(player, card, event);
}

int card_prahv(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( get_card_instance( player, card )->targets[2].player < 0 ){
		get_card_instance( player, card )->targets[2].player = 0;
	}

	if( event == EVENT_DECLARE_ATTACKERS && current_turn == player && count_attackers(player) > 0 ){
		get_card_instance( player, card )->targets[2].player |= 1;
	}

	if( event == EVENT_CAST_SPELL && ! affect_me(player, card) ){
		if( ! is_what(affected_card_controller, affected_card, TYPE_LAND) ){
			get_card_instance( player, card )->targets[2].player |= 2;
		}
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player &&
		(get_card_instance( player, card )->targets[2].player & 1) )
	{
		infinite_casting_cost();
	}

	if( (get_card_instance( player, card )->targets[2].player & 2) ){
		nobody_can_attack(player, card, event, player);
	}

	if( event == EVENT_CHAOS ){
		gain_life(player, hand_count[player]);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[2].player = 0;
	}

	return generic_plane(player, card, event);
}

int card_quicksilver_sea(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_PLANESWALK_IN ){
		scry(player, 4);
	}

	if( get_card_instance( player, card )->targets[1].player != 66 ){
		upkeep_trigger_ability(player, card, event, player);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		scry(player, 4);
	}

	if( event == EVENT_CHAOS ){
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			if( show_deck( player, deck, 1, "Click on the card if you wish to play it", 0, 0x7375B0 ) != -1 &&
				can_legally_play_iid(player, deck[0]) )
			{
				play_card_in_deck_for_free(player, player, 0);
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return generic_plane(player, card, event);
}

int card_selesnya_loft_gardens(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if (get_card_instance( player, card )->targets[2].player == 66 && (event == EVENT_TAP_CARD || event == EVENT_COUNT_MANA) &&
		affected_card_controller == player)
	{
		card_mana_flare(player, card, event);
	}

	if( event == EVENT_CHAOS ){
		get_card_instance( player, card )->targets[2].player = 66;
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[2].player = 0;
	}

	return generic_plane(player, card, event);
}

static int stensia_legacy(int player, int card, event_t event){
// Used mainly for comparison purposes

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

static int check_for_stensia_legacy(int player, int card){
	int c;
	for(c=0; c<active_cards_count[player]; c++){
		if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
			card_instance_t *inst = get_card_instance(player, c);
			if( inst->info_slot == (int)stensia_legacy ){
				if( inst->damage_target_player == player && inst->damage_target_card == card ){
					return 1;
				}
			}
		}
	}
	return 0;
}

static int stensia_chaos_legacy(int player, int card, event_t event){

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return card_hermetic_study(player, card, event);
}

int card_stensia(int player, int card, event_t event){
	/* *** Stensia	""	0x200a90d
	 * Plane - Innistrad
	 * Whenever a creature deals damage to one or more players for the first time each turn, put a +1/+1 counter on it.
	 * Whenever you roll |A, each creature you control gains "|T: This creature deals 1 damage to target player or planeswalker" until end of turn. */
	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_DEAL_DAMAGE ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->info_slot > 0 ){
			if( damage->damage_target_card == -1 && damage->damage_source_player == player && ! damage_is_to_planeswalker(damage) ){
				if( is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE) &&
					! check_for_stensia_legacy(damage->damage_source_player, damage->damage_source_card) )
				{
					card_instance_t *instance = get_card_instance( player, card );
					if( instance->targets[16].player < 0 ){
						instance->targets[16].player = 0;
					}
					int good = 1;
					int pos = instance->targets[16].player;
					if( pos > 0 ){
						int i;
						for(i=0; i<pos; i++){
							if( instance->targets[i].player == damage->damage_source_player &&
								instance->targets[i].card == damage->damage_source_card )
							{
								good = 0;
								break;
							}
						}
					}
					if( good && pos < 10 ){
						instance->targets[pos].player = damage->damage_source_player;
						instance->targets[pos].card = damage->damage_source_card;
						instance->targets[16].player++;
					}
				}
			}
		}
	}

	if( get_card_instance( player, card )->targets[16].player > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
		reason_for_trigger_controller == player ){
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				card_instance_t *instance = get_card_instance( player, card );
				int amount = instance->targets[16].player;
				instance->targets[16].player = 0;
				int i;
				for(i=0;i<amount;i++){
					add_1_1_counter(instance->targets[i].player, instance->targets[i].card);
					int legacy = create_targetted_legacy_effect(player, card, &stensia_legacy,
																instance->targets[i].player, instance->targets[i].card);
					add_status(player, legacy, STATUS_INVISIBLE_FX);
				}
		}
	}

	if( event == EVENT_CHAOS ){
		int c;
		for(c=active_cards_count[player]-1; c > -1; c--){
			if( in_play(player, c) && is_what(player, c, TYPE_CREATURE) ){
				create_targetted_legacy_activate(player, card, &stensia_chaos_legacy, player, c);
			}
		}
	}

	return generic_plane(player, card, event);
}

int card_takenuma2(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( trigger_condition == TRIGGER_LEAVE_PLAY ){
		if( affect_me( player, card) && reason_for_trigger_controller == player && trigger_cause_controller == player ){
			if( is_what(trigger_cause_controller, trigger_cause, TYPE_CREATURE) ){
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						draw_cards(player, 1);
				}
			}
		}
	}

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return generic_plane(player, card, event);
}

int card_talon_gates(int player, int card, event_t event){

/* Talon Gates	""
 * Plane - Dominaria
 * Any time you could cast a sorcery, you may exile a nonland card from your hand with X time counters on it, where X is its converted
 *		mana cost. If the exiled card doesn't have suspend, it gains suspend.
 * Whenever you roll |C, remove two time counters from each suspended card you own. */

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS){
		card_instance_t* inst;
		int c;
		for (c = 0; c < active_cards_count[player]; ++c){
			if ((inst = in_play(player, c))
				&& ((inst->internal_card_id == LEGACY_EFFECT_CUSTOM && inst->info_slot == (int)generic_suspend_legacy)
					|| cards_data[inst->internal_card_id].id == CARD_ID_GREATER_GARGADON_SUSPENDED))
				remove_counters(player, c, COUNTER_TIME, 2);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return generic_plane(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a nonland card to Suspend.");
	test.type_flag = DOESNT_MATCH;
	test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_PLANAR_DICE = 1,
		CHOICE_SUSPEND
	};

	if( event == EVENT_CAN_ACTIVATE && can_sorcery_be_played(player, event) ){
		if( has_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX)) ){
			return 1;
		}
		return check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test);
	}

	if( event == EVENT_ACTIVATE){
		int abils[3] = {
						0,
						has_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX)),
						check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Cast the Planar Dice", abils[1], 1,
							"Suspend a card", abils[2], 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		instance->targets[1].player = choice;
		if( choice == CHOICE_PLANAR_DICE ){
			charge_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX));
			if( spell_fizzled != 1 ){
				instance->info_slot = internal_rand(6)+1;
				add_counter(player, card, COUNTER_VORTEX);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( instance->targets[1].player == CHOICE_SUSPEND ){
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_VALUE, -1, &test);
			if( selected > -1 ){
				suspend_a_card(instance->parent_controller, instance->parent_card, player, selected, get_cmc(player, selected));
			}
		}
		if( instance->targets[1].player == CHOICE_PLANAR_DICE ){
			casting_the_planar_dice(instance->parent_controller, instance->parent_card, instance->info_slot);
		}
	}

	return 0;
}

int card_the_zephyr_maze(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_POWER && affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
		if( check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING) ){
			event_result+=2;
		}
		else{
			event_result-=2;
		}
	}

	if( event == EVENT_CHAOS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FLYING, 0);
		}
		instance->number_of_targets = 0;
	}

	return generic_plane(player, card, event);
}

// trails of the mage-ring --> skipped

int card_truga_jungle(int player, int card, event_t event){

	vanguard_card(player, card, event, 7, 20, 0);

	if(event == EVENT_ACTIVATE ){
		get_card_instance(player, card)->info_slot = 0;	// generic_plane() sets this

		int choice = 1;
		if( generic_plane(player, card, EVENT_CAN_ACTIVATE) ){
			int can_tap_land = permanents_you_control_can_tap_for_mana_all_one_color(player, card, EVENT_CAN_ACTIVATE, TYPE_LAND, -1, COLOR_TEST_ANY_COLORED, 1);
			if( can_tap_land){
				choice = do_dialog(player, player, card, -1, -1, " Get mana\n Cast the Planar Dice\n Cancel", 1);
			}
		} else {
			choice = 0;
		}

		if( choice == 0 ){
			return permanents_you_control_can_tap_for_mana_all_one_color(player, card, event, TYPE_LAND, -1, COLOR_TEST_ANY_COLORED, 1);
		}
		else if( choice == 1 ){
			return generic_plane(player, card, event);
		}
		else{
			spell_fizzled = 1;
		}
		return 0;
	}

	int result = permanents_you_control_can_tap_for_mana_all_one_color(player, card, event, TYPE_LAND, -1, COLOR_TEST_ANY_COLORED, 1);
	if( event == EVENT_CAN_ACTIVATE && result ){
		return 1;
	}

	if( event == EVENT_CHAOS ){
		int *deck = deck_ptr[player];
		int max = 3;
		if( max > count_deck(player) ){
			max = count_deck(player);
		}
		if( max > 0 ){
			show_deck( HUMAN, deck, 3, "Cards revealed by Truga Jungle", 0, 0x7375B0 );
			int i;
			int rev = max;
			for(i=0; i<rev; i++){
				if( is_what(-1, deck[i], TYPE_LAND) ){
					add_card_to_hand(player, deck[i]);
					remove_card_from_deck(player, i);
					max--;
				}
			}
			if( max > 0 ){
				put_top_x_on_bottom(player, player, max);
			}
		}
	}

	return generic_plane(player, card, event);
}

int reveal_top_card_both_player(int player, int card, event_t event);
int card_windriddle_palaces(int player, int card, event_t event){

	/* Windriddle Palaces	""
	 * Plane - Belenon
	 * Players play with the top card of their libraries revealed.
	 * You may play the top card of any player's library.
	 * Whenever you roll |C, each player puts the top card of his or her library into his or her graveyard. */

	reveal_top_card_both_player(player, card, event);
	vanguard_card(player, card, event, 7, 20, 0);

	if( event == EVENT_CHAOS ){
		mill(player, 1);
		mill(1-player, 1);
	}

	if( ! IS_GAA_EVENT(event) ){
		return generic_plane(player, card, event);
	}

	int *deck_player = deck_ptr[player];
	int *deck_opp = deck_ptr[1-player];

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( has_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX)) && can_sorcery_be_played(player, event) ){
			return 1;
		}
		if( deck_player[0] != -1 ){
			if( can_legally_play_iid_now(player, deck_player[0], event) && has_mana_to_cast_id(player, event, cards_data[deck_player[0]].id) ){
				return 1;
			}
		}
		if( deck_opp[0] != -1 ){
			if( can_legally_play_iid_now(player, deck_opp[0], event) && has_mana_to_cast_id(player, event, cards_data[deck_opp[0]].id) ){
				return 1;
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		int abils[4] = {0};
		if( has_mana(player, COLOR_COLORLESS, count_counters(player, card, COUNTER_VORTEX)) && can_sorcery_be_played(player, event) ){
			abils[1] = 1;
		}
		if( deck_player[0] != -1 ){
			if( can_legally_play_iid_now(player, deck_player[0], event) && has_mana_to_cast_id(player, event, cards_data[deck_player[0]].id) ){
				abils[2] = 1;
			}
		}
		if( deck_opp[0] != -1 ){
			if( can_legally_play_iid_now(player, deck_opp[0], event) && has_mana_to_cast_id(player, event, cards_data[deck_opp[0]].id) ){
				abils[3] = 1;
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Cast the Planar Dice", abils[1], 1,
							"Play the first card of your deck", abils[2], 1,
							"Play the first card of opp. deck", abils[3], 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		instance->targets[1].player = choice;
		if( choice == 1 ){
			return generic_plane(player, card, event);
		}
		if( choice == 2 ){
			if (charge_mana_from_id(player, -1, event, cards_data[deck_player[0]].id)){
				play_card_in_deck_for_free(player, player, 0);
				cant_be_responded_to = 1;
			}
		}
		if( choice == 3 ){
			charge_mana_from_id(player, -1, event, cards_data[deck_opp[0]].id);
			if( spell_fizzled != 1 ){
				play_card_in_deck_for_free(player, 1-player, 0);
				cant_be_responded_to = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->targets[1].player == 1){
			casting_the_planar_dice(player, instance->parent_card, instance->info_slot);
		}
	}

	return 0;
}

/*
*** Celestine Reef	""
Plane - Luvion
Creatures without flying or |H2islandwalk can't attack.
Whenever you roll |A, until a player planeswalks, you can't lose the game and your opponents can't win the game.
*/

/*
*** Horizon Boughs	""
Plane - Pyrulea
All permanents untap during each player's untap step.
Whenever you roll |A, you may search your library for up to three basic land cards, put them onto the battlefield tapped, then shuffle your library.
*/

/*
*** Mirrored Depths	""
Plane - Karsus
Whenever a player casts a spell, that player flips a coin. If he or she loses the flip, counter that spell.
Whenever you roll |A, target player reveals the top card of his or her library. If it's a nonland card, you may cast it without paying its mana cost.
*/

/*
*** Tember City	""
Plane - Kinshala
Whenever a player taps a land for mana, Tember City deals 1 damage to that player.
Whenever you roll |A, each other player sacrifices a nonland permanent.
*/
