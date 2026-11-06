#include "manalink.h"

// Cards originally printed as promos; Astral cards

/*
Nalathni Dragon |2|R|R --> Dragon Whelp
Creature - Dragon 1/1
Flying; banding
{R}: Nalathni Dragon gets +1/+0 until end of turn.
	If this ability has been activated four or more times this turn, sacrifice Nalathni Dragon at the beginning of the next end step.
*/

/*
Windseeker Centaur |1|R|R --> Serra Angel
Creature - Centaur 2/2
Vigilance
*/

static int ai_choice_for_arena(int player, int card, target_t* ret_tgt){
	card_instance_t* instance;
	int c, best_rating = -1;
	uint32_t prots = get_protections_from(player, card);

	ret_tgt->player = ret_tgt->card = -1;
	for (c = 0; c < active_cards_count[AI]; ++c){
		if (in_play(AI, c)
			&& cards_data[(instance = get_card_instance(AI, c))->internal_card_id].type & TYPE_CREATURE
			&& instance->kill_code != KILL_SACRIFICE	// How would it even get here?
			&& !(instance->state & STATE_CANNOT_TARGET)){
			/* Straight out of the exe.  There's definitely room for improvement here.
			 * For starters, if the human player chose first, the chosen creature should be taken into account.  This will always pick a
			 *    vanilla 4/4 creature over either a 6/1 or a 1/6, even when fighting a known 5/5.
			 * Most abilities, as well as EA_MANA_SOURCE and EA_ACT_ABILITY, should make the AI less likely to pick a given creature.
			 * Protections are an exception.  Especially if the human player's picked and the candidate has protection from that creature.
			 * Base card value should also be looked at. */
			uint32_t abils = get_abilities(AI, c, EVENT_ABILITIES, -1);
			if (abils & prots){
				continue;
			}
			int rating = (get_abilities(AI, c, EVENT_POWER, -1)
						  + get_abilities(AI, c, EVENT_TOUGHNESS, -1)
						  + num_bits_set(abils)
						  + num_bits_set(abils & (KEYWORD_FLYING | KEYWORD_FIRST_STRIKE))
						  + num_bits_set(cards_data[instance->internal_card_id].extra_ability & (EA_MANA_SOURCE | EA_ACT_ABILITY)));

			if (best_rating < rating){
				best_rating = rating;
				ret_tgt->player = AI;
				ret_tgt->card = c;
			}
		}
	}

  return best_rating >= 0;
}

int card_arena(int player, int card, event_t event){
  /* Arena	""
   * Land
   * |3, |T: Tap target creature you control and target creature of an opponent's choice he or she controls. Those creatures fight each other. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ABILITY = 2
  };


	// Used externally
	if (event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI){
		ai_modifier += 48;
		return 0;
	}

	if (event == EVENT_RESOLVE_SPELL){
		play_land_sound_effect(player, card);
		return 0;
	}

	if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP){
		return mana_colors_added[player] ? mana_producer(player, card, event) : 0;
	}

	card_instance_t* instance = get_card_instance(player, card);

	target_definition_t td_me, td_opp;

	default_target_definition(player, card, &td_me, TYPE_CREATURE);
	td_me.allowed_controller = td_me.preferred_controller = player;

	default_target_definition(player, card, &td_opp, TYPE_CREATURE);
	td_opp.allowed_controller = td_opp.preferred_controller = td_opp.who_chooses = 1 - player;
	td_opp.allow_cancel = 0;

	if (event == EVENT_CAN_ACTIVATE){
		if( mana_colors_added[player] ){
			return mana_producer(player, card, event);
		}
		return (!is_tapped(player, card) && !is_animated_and_sick(player, card) && !paying_mana()
				&& can_use_activated_abilities(player, card) && has_mana_for_activated_ability(player, card, MANACOST_X(3))
				&& can_target(&td_me) && can_target(&td_opp));
	}

	if (event == EVENT_ACTIVATE){
		int choice = CHOICE_ABILITY;
		if( mana_colors_added[player] ){
			if( paying_mana() || is_tapped(player, card) || is_animated_and_sick(player, card) || paying_mana()
				|| !can_use_activated_abilities(player, card) || !has_mana_for_activated_ability(player, card, MANACOST_X(4))
				|| !can_target(&td_me) || !can_target(&td_opp) ){
				choice = CHOICE_MANA;
			}else if( !IS_AI(player) ){
				choice = DIALOG(player, card, event, DLG_RANDOM,
					"Get mana", 1, 1,
					"Use Ability", 1, 1);
				if( !choice ){
					spell_fizzled = 1;
					return 0;
				}
			}
		}
		instance->info_slot = choice;
		if( choice == CHOICE_MANA ){
			mana_producer(player, card, event);
			return 0;
		}
		if (paying_mana()){
			cancel = 1;
			return 0;
		}

		charge_mana_for_activated_ability(player, card, MANACOST_X(3));
		tapped_for_mana_color = -1;

		if (cancel != 1){
			instance->number_of_targets = 0;

			if (player == HUMAN){
				if (!pick_target(&td_me, "ARENA")){
					cancel = 1;
					return 0;
				}

				instance->state |= STATE_TAPPED;
				if (trace_mode & 2){
					send_arena_network_choice(player, 0);
					new_pick_target(&td_opp, "ARENA", 1, 0);
				} else {
					if (!ai_choice_for_arena(player, card, &instance->targets[1])){
						// shouldn't be possible currently, but in case validate_target()'s definition drifts
						new_pick_target(&td_opp, "ARENA", 1, 0);
					}
					instance->number_of_targets = 2;
				}
				do_dialog(player, player, card, instance->targets[1].player, instance->targets[1].card, "Selected for Arena", 0);
			} else {	// player == AI
				if (trace_mode & 2){
					cancel = pick_target(&td_me, "ARENA") ? 0 : 1;
				} else {
					if (ai_choice_for_arena(player, card, &instance->targets[0])){
						instance->number_of_targets = 1;
						cancel = 0;
					} else {
						// shouldn't be possible currently, but in case validate_target()'s definition drifts
						cancel = pick_target(&td_opp, "ARENA") ? 0 : 1;
					}
				}

				if (cancel != 1){
					instance->state |= STATE_TAPPED;

					do_dialog(player, player, card, instance->targets[0].player, instance->targets[0].card, "Selected for Arena", 0);
					new_pick_target(&td_opp, "ARENA", 1, 0);

					if (trace_mode & 2){
						send_arena_network_choice(1 - player, 0);
					}
				}
			}

			if (cancel == 1){
				instance->number_of_targets = 0;
			}
		}

		return 0;
	}

	if (event == EVENT_RESOLVE_ACTIVATION && instance->info_slot == CHOICE_ABILITY){
		int target_0_valid = validate_target(player, card, &td_me, 0);
		int target_1_valid = validate_target(player, card, &td_opp, 1);

		if (target_0_valid && target_1_valid){
			fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player, instance->targets[1].card);
			if (in_play(instance->targets[0].player, instance->targets[0].card)){
				tap_card(instance->targets[0].player, instance->targets[0].card);
			}
			if (in_play(instance->targets[1].player, instance->targets[1].card)){
				tap_card(instance->targets[1].player, instance->targets[1].card);
			}
		}
		/* Otherwise, if one target one was valid, the exe version still tapped it and did damage to it.  This contradicts ruling 9/25/2006: "If either target
		 * is illegal at the time the ability resolves (whether because it has left the battlefield, because it has gained Shroud or Protection, or for any
		 * other reason), the ability taps the remaining target, but no damage is dealt. If both targets are illegal, the ability is countered." */
		else if (target_0_valid){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		} else if (target_1_valid){
			tap_card(instance->targets[1].player, instance->targets[1].card);
		} else {
			spell_fizzled = 1;
		}

		get_card_instance(instance->parent_controller, instance->parent_card)->number_of_targets = 0;
		return 0;
	}

	return 0;
}

int card_aswan_jaguar(int player, int card, event_t event)
{
  /* Aswan Jaguar	|1|G|G
   * Creature - Cat 2/2
   * When ~ comes into play, choose a random creature subtype from those in target opponent's deck.
   * |G|G, |T: Destroy target creature of the chosen subtype. It can't be regenerated. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && target_opponent(player, card) ){
	  // Equal chance for each creature type present in opponent's library, no matter how many times it appears, or whether it's on a creature or a Tribal.

	  char creature_type_is_present[SUBTYPE_MAX_USED_CREATURE_SUBTYPE + 1] = {0};

	  int* deck = deck_ptr[1-player];
	  int i, j;
	  for (i = 0; i < 500 && deck[i] != -1; ++i)
		{
		  int16_t* types = cards_ptr[cards_data[deck[i]].id]->types;
		  for (j = 0; j <= 5; ++j)	// ignore types[6], per has_new_types()
			if (types[j] >= SUBTYPE_MIN_CREATURE_SUBTYPE && types[j] <= SUBTYPE_MAX_USED_CREATURE_SUBTYPE)
			  creature_type_is_present[types[j]] = 1;
		}

	  subtype_t creature_types_present[SUBTYPE_MAX_USED_CREATURE_SUBTYPE + 1];
	  int num_creature_types_present = 0;
	  for (i = 0; i <= SUBTYPE_MAX_USED_CREATURE_SUBTYPE; ++i)
		if (creature_type_is_present[i])
		  creature_types_present[num_creature_types_present++] = i;

	  card_instance_t* instance = get_card_instance(player, card);

	  if (num_creature_types_present == 0)
		instance->info_slot = 0;
	  else if (num_creature_types_present == 1)
		instance->info_slot = creature_types_present[0];
	  else if (player == 1 && (trace_mode & 2))
		{
		  TENTATIVE_wait_for_network_result(1, 26);	// receive_network_packet(1, 26)
		  instance->info_slot = *shandalar_network_result();
		}
	  else
		{
		  instance->info_slot = creature_types_present[internal_rand(num_creature_types_present)];
		  if (trace_mode & 2)
			{
			  *shandalar_network_result() = instance->info_slot;
			  *shandalar_network_packet_type() = 26;
			  TENTATIVE_send_network_result(0, 26);	// send_network_packet(0, 26)
			}
		}

	  if (instance->info_slot > 0)
		create_subtype_name_legacy(player, card, instance->info_slot);
	}

  if (event == EVENT_GET_SELECTED_CARD)
	load_recorded_action_target(0);

  if (!IS_GAA_EVENT(event))
	return 0;

  card_instance_t* instance = get_card_instance(player, card);

  if (instance->info_slot <= 0)
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.required_subtype = instance->info_slot;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  play_sound_effect(WAV_ASWANJAG);
	  kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST_G(2), 0, &td,
								   get_subtype_text("Select target %s creature.", instance->info_slot));
}

int card_call_from_the_grave(int player, int card, event_t event){
	/*
	  Call from the Grave	|2|B
	  Sorcery
	  Put a random creature from a random graveyard into play under your control.
	  Call from the Grave deals to you an amount of damage equal to that creature's converted mana cost.
	*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.cmc = 3;
		test.cmc_flag = F5_CMC_GREATER_THAN_VALUE;
		if( new_special_count_grave(player, &test) != -1 || new_special_count_grave(1-player, &test) != -1 ){
			ai_modifier += 48;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if (count_graveyard_by_type(ANYBODY, TYPE_CREATURE) < 1){ kill_card(player, card, KILL_DESTROY); return 0; }
		int corpses[500];
		int cc = 0;
		int i;
		for(i=0; i<2; i++){
			const int *grave = get_grave(i);
			int count = 0;
			while( grave[count] != -1 ){
					if( is_what(-1, grave[count], TYPE_CREATURE) ){
						corpses[cc] = count;
						if( i != player ){
							corpses[cc] |= (1<<31);
						}
						cc++;
					}
					count++;
			}
		}
		int rnd = internal_rand(cc);
		int t_player = corpses[rnd] & (1<<31) ? 1-player : player;
		int pos = corpses[rnd];
		if( t_player != player ){
			pos &= ~(1<<31);
		}
		int card_added = reanimate_permanent(player, card, t_player, pos, REANIMATE_DEFAULT);
		if (card_added < 1){ spell_fizzled = 1; return 0; }
		damage_player(player, get_cmc(player, card_added), player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_faerie_dragon(int player, int card, event_t event)
{
  /* Faerie Dragon	|2|G|G
   * Creature - Faerie Dragon 1/3
   * Flying
   * |1|G|G: Play a random effect. */

  // 0x4735C0



  target_t tgts[300]; // [sp+Ch] [bp-960h]@14

  if (event == EVENT_CAN_ACTIVATE)
	return can_use_activated_abilities(player, card) && has_mana_for_activated_ability(player, card, MANACOST_XG(1,2));

  if (event == EVENT_GET_SELECTED_CARD)
	load_recorded_action_target(0);

  if (event == EVENT_ACTIVATE && charge_mana_for_activated_ability(player, card, MANACOST_XG(1,2)))
	{
	  int abil = network_rand(player, 20);	// 0x44E070 either picks a random number and sends it over the network, or awaits a number of the network

	  card_instance_t* instance = get_card_instance(player, card);

	  instance->info_slot = abil;

	  int numtgts = choose_orcish_catapult_targets(player, card, tgts);	// picks random targets, sends/receives over network

	  int chosen = network_rand(player, numtgts);

	  instance->targets[0].player = tgts[chosen].player;
	  instance->targets[0].card = tgts[chosen].card;
	  instance->number_of_targets = 1;
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (!instance->number_of_targets)
		return 0;

	  // 0x473720

	  int abil = instance->info_slot;

	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.preferred_controller = ANYBODY;

	  if (!valid_target(&td))
		{
		  cancel = 1;
		  get_card_instance(instance->parent_controller, instance->parent_card)->number_of_targets = 0;
		  return 0;
		}

	  int tgt_player = instance->targets[0].player;
	  int tgt_card = instance->targets[0].card;

	  card_instance_t* tgt_inst = get_card_instance(tgt_player, tgt_card);

	  int effect_card = -1;

	  char buf[301];

	  if (abil >= 0 && abil < 20 && abil != 13 && abil != 1)
		{
		  load_text(0, "FAERIEDRAGON_MESSAGES");
		  scnprintf(buf, 300, "\n%s", text_lines[abil]);
		  do_dialog(player, player, card, tgt_player, tgt_card, buf, 0);
		  play_sound_effect(WAV_FAERDRAG);
		}

	  switch (abil)
		{
		  case 0:
			effect_card = create_legacy_effect_exe(player, card, LEGACY_EFFECT_GENERIC, tgt_player, tgt_card);
			if (effect_card != -1)
			  {
				card_instance_t* legacy = get_card_instance(player, effect_card);
				legacy->info_slot = KEYWORD_TRAMPLE;
				legacy->counter_power = tgt_inst->power;
				legacy->token_status |= STATUS_BERSERK;
			  }
			break;

		  case 1:
			if (tgt_inst->power >= 3)
			  {
				cancel = 1;
				load_text(0, "FAERIEDRAGON_TAWNOSWAND");
				sprintf(buf, "\n%s", text_lines[1]);
				do_dialog(player, player, card, tgt_player, tgt_card, buf, 0);
			  }
			else
			  {
				load_text(0, "FAERIEDRAGON_TAWNOSWAND");
				sprintf(buf, "\n%s", text_lines[0]);
				do_dialog(player, player, card, tgt_player, tgt_card, buf, 0);
				effect_card = pump_ability_until_eot(player, card, tgt_player, tgt_card, 0,0, 0,SP_KEYWORD_UNBLOCKABLE);
				play_sound_effect(WAV_FAERDRAG);
			  }
			break;

		  case 2:
			;int tgh = get_toughness(tgt_player, tgt_card);

			if (tgh >= 5)
			  tgh = 4;
			else
			  tgh = tgh - 1;

			effect_card = pump_until_eot(player, card, tgt_player, tgt_card, 4, -tgh);
			break;

		  case 3:
			// change_color_and_kill_other_change_color_effects
			change_color(player, card,tgt_player, tgt_card, get_sleighted_color_test(player, card, COLOR_TEST_GREEN), CHANGE_COLOR_SET | CHANGE_COLOR_NO_SLEIGHT);
			break;

		  case 4:
			// change_color_and_kill_other_change_color_effects
			change_color(player, card,tgt_player, tgt_card, get_sleighted_color_test(player, card, COLOR_TEST_WHITE), CHANGE_COLOR_SET | CHANGE_COLOR_NO_SLEIGHT);
			break;

		  case 5:
			// change_color_and_kill_other_change_color_effects
			change_color(player, card,tgt_player, tgt_card, get_sleighted_color_test(player, card, COLOR_TEST_RED), CHANGE_COLOR_SET | CHANGE_COLOR_NO_SLEIGHT);
			break;

		  case 6:
			damage_creature(tgt_player, tgt_card, 3, instance->parent_controller, instance->parent_card);
			break;

		  case 7:
			effect_card = pump_ability_until_eot(player, card, tgt_player, tgt_card, 0,0, KEYWORD_FLYING,0);
			break;

		  case 8:
			effect_card = pump_until_eot(player, card, tgt_player, tgt_card, 3,3);
			break;

		  case 9:
			effect_card = pump_ability_until_eot(player, card, tgt_player, tgt_card, 0,0, KEYWORD_BANDING,0);
			break;

		  case 10:
			// change_color_and_kill_other_change_color_effects
			change_color(player, card,tgt_player, tgt_card, get_sleighted_color_test(player, card, COLOR_TEST_BLACK), CHANGE_COLOR_SET | CHANGE_COLOR_NO_SLEIGHT);
			break;

		  case 11:
			// change_color_and_kill_other_change_color_effects
			change_color(player, card,tgt_player, tgt_card, get_sleighted_color_test(player, card, COLOR_TEST_BLUE), CHANGE_COLOR_SET | CHANGE_COLOR_NO_SLEIGHT);
			break;

		  case 12:
			effect_card = cannot_regenerate_until_eot(player, card, tgt_player, tgt_card);
			break;

		  case 13:
			load_text(0, "FAERIEDRAGON_TWIDDLE");
			scnprintf(buf, 300, "\n%s\n %s\n %s", text_lines, text_lines[1], text_lines[2]);
			if (do_dialog(player, player, card, tgt_player, tgt_card, buf, (tgt_inst->state & STATE_TAPPED) ? 0 : 1))
			  untap_card(tgt_player, tgt_card);
			else
			  tap_card(tgt_player, tgt_card);

			play_sound_effect(WAV_FAERDRAG);
			break;

		  case 14:
			effect_card = pump_until_eot(player, card, tgt_player, tgt_card, -2,0);
			break;

		  case 15:
			bounce_permanent(tgt_player, tgt_card);
			break;

		  case 16:
			damage_creature(tgt_player, tgt_card, 1, instance->parent_controller, instance->parent_card);
			break;

		  case 17:
			set_pt_and_abilities_until_eot(instance->parent_controller, instance->parent_card, tgt_player, tgt_card, 0, 2, 0, 0, 0);
			break;

		  case 18:
			gain_life(tgt_player, get_power(tgt_player, tgt_card));
			kill_card(tgt_player, tgt_card, KILL_EXILE);
			break;

		  case 19:
			if (ai_is_speculating != 1)
			  {
				play_sound_effect(WAV_ORCART);
				Sleep(3000);	// Sleep(3000);
			  }
			++hack_silent_counters;
			add_counters(player, card, tgt_player, tgt_card, COUNTER_M0_M1, 1);
			--hack_silent_counters;
			play_sound_effect(WAV_CATATAP);
			break;

		  default:
			load_text(0, "FAERIEDRAGON_ERROR");
			scnprintf(buf, 300, "\n%s", text_lines[0]);
			do_dialog(player, player, card, tgt_player, tgt_card, buf, 0);
			break;
		}

	  if (effect_card != -1)
		{
		  card_instance_t* legacy = get_card_instance(player, effect_card);
		  legacy->display_pic_csv_id = g_mimicked_effect_csv_ids[abil];	// The csvids of the mimicked effects
		  legacy->display_pic_num = get_card_image_number(legacy->display_pic_csv_id, player, card);
		}

	  get_card_instance(instance->parent_controller, instance->parent_card)->number_of_targets = 0;
	}

  return 0;
}

int card_gem_bazaar(int player, int card, event_t event){
  /* Gem Bazaar	""
   * Land
   * When ~ comes into play, choose a random color.
   * |T: Add to your mana pool one mana of the color last chosen. Then choose a random color. */

	/*
	  Gem Bazaar
	  When Gem Bazaar comes into play, choose a random color.
	  |T: Add to your mana pool one mana of the color last chosen. Then choose a random color.
	*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if (event == EVENT_ETB_ABILITY){
		play_sound_effect(WAV_GEMBAZAR);
		if (!is_humiliated(player, card, AL_TRIG)){
			get_card_instance(player, card)->info_slot = 1<<(1+internal_rand(5));
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		play_sound_effect(WAV_GEMBAZAR);
		card_instance_t *instance = get_card_instance(player, card);
		get_card_instance(instance->parent_controller, instance->parent_card)->info_slot = 1<<(1+internal_rand(5));
	}

	return plain_mana_producer(player, card, event);
}

int card_giant_badger(int player, int card, event_t event){
  /* Giant Badger |1|G|G
   * 	  Creature - Badger 2/2
   * 	  Whenever Giant Badger blocks, it gets +2/+2 until end of turn. */

	if( ! is_humiliated(player, card, AL_TRIG) && blocking(player, card, event) ){
		int amount = 2*count_creatures_this_is_blocking(player, card);
		pump_until_eot(player, card, player, card, amount, amount);
	}
	return 0;
}

int card_goblin_polka_band(int player, int card, event_t event)
{
  /* Goblin Polka Band	|R|R
   * Summon Goblin 1/1
   * |2, |T, Pay |R for each target: Tap any number of random target creatures. Goblins tapped in this way do not untap during their controllers' next untap phases. */

  card_instance_t* instance = get_card_instance(player, card);

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = 2;

  int ai = player == AI || ai_is_speculating == 1;

  if (event == EVENT_CAN_ACTIVATE)
	{
	  if (!(can_use_activated_abilities(player, card)
			&& has_mana_for_activated_ability(player, card, MANACOST_XR(2, ai ? 1 : 0))
			&& !is_tapped(player, card)
			&& !is_sick(player, card)))
		return 0;

	  if (ai)
		{
		  // Only activate if opponent controls more potential targets than player.
		  td.allowed_controller = td.preferred_controller = 1-player;
		  int num_opp = target_available(player, card, &td);
		  td.allowed_controller = td.preferred_controller = player;
		  int num_self = target_available(player, card, &td);

		  if (num_opp == 0 || num_self >= num_opp)
			return 0;
		}

	  return 1;
	}

  if (event == EVENT_GET_SELECTED_CARD)
	{
	  load_recorded_action_target(1);
	  load_recorded_action_code(0);
	}

  if (event == EVENT_ACTIVATE)
	{
	  if (charge_mana_for_activated_ability(player, card, MANACOST_X(2)))
		{
		  /* I... *think*... that 9 is the highest target number that hasn't been co-opted for local storage.  Which is pretty crazy, actually; we need to
		   * create parallel storage.  Shouldn't be too difficult - 1) clear it in create_card_instance(); 2) track down everywhere in the exe that memcpy()s a
		   * card_instance_t; some notable examples are start and end of ai speculation, display, and change of control; 3) rewrite saving and loading, which is
		   * overdue anyway. */
		  int old_max_x_value = max_x_value;
		  max_x_value = 10;

		  if (ai)
			{
			  max_x_value = target_available(player, card, &td);
			  if (max_x_value > 10)
				max_x_value = 10;
			}

		  charge_mana(player, COLOR_RED, -1);
		  max_x_value = old_max_x_value;
		  if (cancel != 1)
			{
			  tapped_for_mana_color = -1;
			  instance->state |= STATE_TAPPED;

			  if (x_value <= 0)
				return 0;

			  // Choose the targets.  This is done during resolution in the original version.

			  target_t targets[500];
			  int p, c, end = 0, num = x_value;
			  for (p = 0; p < 2; ++p)
				{
				  instance->targets[0].player = p;
				  for (c = 0; c < active_cards_count[player]; ++c)
					{
					  instance->targets[0].card = c;
					  if (would_valid_target(&td))
						targets[end++] = instance->targets[0];	// struct copy
					}
				}

			  num = MIN(num, end);
			  if (num <= 0)
				return 0;

			  instance->number_of_targets = 0;
			  for (c = 0; c < num; ++c)
				{
				  int choice = internal_rand(end);
				  instance->targets[c] = targets[choice];	// struct copy
				  // Don't choose this target again
				  targets[choice] = targets[end - 1];	// struct copy
				  --end;
				  ++instance->number_of_targets;
				}
			}
		}
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  play_sound_effect(WAV_POLKAMIX);	// EVERYBODY POLKA!  Ha ha ha!  Ho ho ho!

	  int i;
	  for (i = 0; i < instance->number_of_targets; ++i)
		if (validate_target(player, card, &td, i))
		  {
			if (has_subtype(instance->targets[i].player, instance->targets[i].card, SUBTYPE_GOBLIN))
			  does_not_untap_effect(player, instance->parent_card, instance->targets[i].player, instance->targets[i].card, EDNT_TAP_TARGET, 1);
			else
			  tap_card(instance->targets[i].player, instance->targets[i].card);
		  }
	}

  return 0;
}

int card_mana_crypt(int player, int card, event_t event){
  /* Mana Crypt	|0
   * Artifact
   * At the beginning of your upkeep, flip a coin. If you lose the flip, this artifact deals 3 damage to you.
   * |T: Add |C|C. */

	/*
	  Mana Crypt 0
	  Artifact
	  At the beginning of your upkeep, flip a coin. If you lose the flip, Mana Crypt deals 3 damage to you.
	  {T}: Add {2} to your mana pool.
	*/
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( ! flip_a_coin(player, card) ){
			damage_player(player, 3, player, card);
		}
	}

	return card_sol_ring(player, card, event);
}

// Spawn of Azar token --> rhino token

int card_necropolis_of_azar(int player, int card, event_t event)
{
  /* Necropolis of Azar	|2|B|B
   * Enchantment
   * Whenever a non|Sblack creature is put into any graveyard from play, put a husk counter on ~.
   * |5, Remove a husk counter from ~: Put a Spawn of Azar token into play. Treat this token as a |Sblack creature with a random power and toughness, each no
   * less than 1 and no greater than 3, that has |H2swampwalk. */

  int parent_controller;
  unsigned int death_count;
  int activation_or_counter_count;
  card_instance_t *instance;
  int random_power;
  int random_toughness;

  if (event == EVENT_CAN_CAST)
  {
    return 1;
  }
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    test_definition_t test;
    new_default_test_definition(&test, 2, "");
    test.color = get_sleighted_color_test(player, card, 2);
    test.color_flag = 1;
    count_for_gfp_ability(player, card, 0x77, 2, 0, &test);
  }
  death_count = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (death_count != 0)
  {
    for (death_count = death_count & 0xff; death_count != 0; death_count = death_count - 1)
      add_counters(player, card, player, card, COUNTER_HUSK, 1);
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    activation_or_counter_count = count_counters(player, card, COUNTER_HUSK);
    if (activation_or_counter_count < 1)
      return 0;
    return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 5, 0, 0, 0, 0, 0, 0, NULL,
                                     NULL);
  }
  activation_or_counter_count =
      generic_activated_ability(player, card, event, 0, 5, 0, 0, 0, 0, 0, 0, NULL, NULL);
  if (event == EVENT_ACTIVATE)
  {
    if (spell_fizzled != 1)
    {
      remove_counters(player, card, player, card, COUNTER_HUSK, 1);
      return activation_or_counter_count;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    token_generation_t token;
    play_sound_effect(WAV_NECRAZAR);
    instance = get_card_instance(player, card);
    parent_controller = instance->parent_controller;
    default_token_definition(player, card, CARD_ID_SPAWN_OF_AZAR, &token);
    random_power = network_rand(parent_controller, 3);
    token.pow = (int32_t)(random_power + 1);
    random_toughness = network_rand(parent_controller, 3);
    token.tou = (int32_t)(random_toughness + 1);
    generate_token(&token);
    return activation_or_counter_count;
  }
  return activation_or_counter_count;
}

int card_orcish_catapult(int player, int card, event_t event)
{
  /* Orcish Catapult	|X|R|R
   * Instant
   * Randomly distribute X -0/-1 counters among a random number of random target creatures. */

  /*
    Randomly distribute |X -0/-1 counters among a random number of random target creatures.
  */
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
  }

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
  {
    instance->number_of_targets = 0;
    instance->info_slot = x_value;
    int max_trgs = 0;
    int trgs[2][18];
    int p;
    for (p = 0; p < 2; p++)
    {
      int c;
      for (c = 0; c < active_cards_count[p]; c++)
      {
        if (in_play(p, c) && is_what(p, c, TYPE_CREATURE))
        {
          if (would_validate_arbitrary_target(&td, p, c))
          {
            trgs[0][max_trgs] = p;
            trgs[1][max_trgs] = c;
            max_trgs++;
          }
          if (max_trgs == 18)
          {
            break;
          }
        }
      }
      if (max_trgs == 18)
      {
        break;
      }
    }
    if (max_trgs)
    {
      for (p = 0; p < max_trgs; p++)
      {
        if (internal_rand(10) + 1 >= 5)
        {
          int pos = instance->number_of_targets;
          instance->targets[pos].player = trgs[0][p];
          instance->targets[pos].card = trgs[1][p];
          instance->number_of_targets++;
        }
      }
    }
  }

  if (event == EVENT_RESOLVE_SPELL)
  {
    int i, amount = instance->info_slot, sound_played = 0;
    for (i = 0; i < instance->number_of_targets; i++)
    {
      if (validate_target(player, card, &td, i))
      {
        int cnters = 1;
        if (amount > 1)
        {
          cnters = internal_rand(amount);
        }
        amount -= cnters;

        if (ai_is_speculating != 1 && !sound_played)
        {
          play_sound_effect(WAV_ORCART);
          Sleep(3000); // Sleep(3000);
        }
        ++hack_silent_counters;
        add_counters(player, card, instance->targets[i].player,
                                 instance->targets[i].card, COUNTER_M0_M1, cnters);
        --hack_silent_counters;
        if (ai_is_speculating != 1 && !sound_played)
        {
          sound_played = 1;
          play_sound_effect(WAV_CATATAP);
        }
      }
      if (amount < 1)
      {
        break;
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return 0;
}

int card_pandoras_box(int player, int card, event_t event){
  /*
	  Pandora's Box	|5
	  |3, |T: Choose a random non-artifact creature card from all players' libraries.
	  For each player, flip a coin. If the flip ends up heads, put a token creature into play
	  and treat it as though an exact copy of the chosen creature card were just played.
	*/
	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card);
		play_sound_effect(WAV_PANDORA);
		int corpses[500];
		int cc = 0;
		int i;
		for(i=0; i<2; i++){
			int *deck = deck_ptr[i];
			int count = 0;
			while( deck[count] != -1 ){
					if( is_what(-1, deck[count], TYPE_CREATURE) && ! is_what(-1, deck[count], TYPE_ARTIFACT) ){
						corpses[cc] = deck[count];
						cc++;
					}
					count++;
			}
		}
		int revealed_by_pandoras_box[1] = {corpses[internal_rand(cc)]};
		show_deck( player, revealed_by_pandoras_box, 1, "Pandora's Box chosen...", 0, gs_done_008b40e0 );

		token_generation_t token;
		default_token_definition(player, card, cards_data[revealed_by_pandoras_box[0]].id, &token);
		token.no_sleight = 1;

		APNAP(p, {
					if( player_flips_a_coin(instance->parent_controller, instance->parent_card, p) ){
						token.t_player = p;
						generate_token(&token);
					}
				};
		);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
}

int card_power_struggle(int player, int card, event_t event){
  /*
	  Power Struggle	|2|U|U|U
	  At the beginning of each player's upkeep, that player exchanges control of random target artifact, creature or land he or she controls,
	  for control of random target permanent of the same type that a random opponent controls.
	*/
	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_ARTIFACT | TYPE_LAND);
		int players_permanents[active_cards_count[player]];
		int ppc = 0;
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_play(player, i) && is_what(player, i, TYPE_CREATURE | TYPE_ARTIFACT | TYPE_LAND) ){
				if( would_validate_arbitrary_target(&td, player, i) ){
					players_permanents[ppc] = i;
					ppc++;
				}
			}
		}
		if( ppc ){
			int my_permanent_to_give = players_permanents[ppc];
			if( ppc > 1 ){
				my_permanent_to_give = players_permanents[internal_rand(ppc)];
			}
			int type = get_type(player, my_permanent_to_give);
			int opp_permanents[active_cards_count[1-player]];
			int opc = 0;
			for(i=0; i<active_cards_count[1-player]; i++){
				if( in_play(1-player, i) && is_what(1-player, i, type) ){
					if( would_validate_arbitrary_target(&td, 1-player, i) ){
						opp_permanents[opc] = i;
						opc++;
					}
				}
			}
			if( opc ){
				exchange_control_of_target_permanents(player, card, player, my_permanent_to_give, 1-player, opp_permanents[internal_rand(opc)]);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_prismatic_dragon(int player, int card, event_t event){
	/*
	  Prismatic Dragon	|2|W|W
	  Creature - Dragon
	  Flying
	  At the beginning of your upkeep, Prismatic Dragon becomes a random color permanently.
	  |2: Prismatic Dragon becomes a random color permanently.
	*/
	if( ! is_humiliated(player, card, AL_TRIG) ){
		if( current_turn == player && upkeep_trigger(player, card, event) ){
			get_card_instance(player, card)->info_slot = 1<<(1+internal_rand(5));
		}

		if( event == EVENT_SET_COLOR && affect_me(player, card) && get_card_instance(player, card)->info_slot > 0){
			event_result = get_card_instance(player, card)->info_slot;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		play_sound_effect(WAV_PRSMDRAG);
		card_instance_t *instance = get_card_instance(player, card);
		get_card_instance(instance->parent_controller, instance->parent_card)->info_slot = 1<<(1+internal_rand(5));
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
}

int card_rainbow_knights(int player, int card, event_t event){
  /* Rainbow Knights	|W|W
   * Summon Knights 2/1
   * When ~ comes into play, it gains protection from a random color permanently.
   * |1: First strike until end of turn.
   * |W|W: +0/+0, +1/+0 or +2/+0 until end of turn chosen at random. */

	/*
	  Rainbow Knights	|W|W
	  When Rainbow Knights comes into play, it gains protection from a random color permanently.
	  |1: First strike until end of turn.
	  |W|W: +0/+0, +1/+0, or +2/+0 until end of turn, chosen at random.
	*/
	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
        instance->targets[1].card = 0;
    }
    etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (event == EVENT_ETB_ABILITY){
        instance->targets[1].card |= 1 << (11 + internal_rand(5));
    }
    if (event == EVENT_ABILITIES && affect_me(player, card) && (instance->targets[1].card & 0xf800) &&
        !is_humiliated(player, card, AL_STATIC)){
        event_result |= instance->targets[1].card & 0xf800;
    }

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_W(2), 0, NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = DIALOG(player, card, event, DLG_RANDOM,
						"First strike", has_mana_for_activated_ability(player, card, MANACOST_X(1)), 15*(!check_for_ability(player, card, KEYWORD_FIRST_STRIKE)),
						"Random pump", has_mana_for_activated_ability(player, card, MANACOST_W(2)), 10);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_XW((choice == 1 ? 1 : 0), 2*(choice == 2))) ){
			instance->info_slot = choice;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									0, 0, KEYWORD_FIRST_STRIKE, 0);
		}
		if( instance->info_slot == 2 ){
			play_sound_effect(WAV_RAINBOWK);
			int rnd = internal_rand(3)+1;
			if( rnd < 3 ){
				pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, rnd, 0);
			}
		}
	}

	return 0;
}

static void sewers_of_estark_prevent_damage_effect(intptr_t player, intptr_t card, int t_player, int t_card){
	prevent_all_damage_dealt_by_target(player, card, t_player, t_card, DDBM_MUST_BE_COMBAT_DAMAGE);
}

int card_sewers_of_estark(int player, int card, event_t event){
  /* Sewers of Estark	|2|B|B
   * Instant
   * Choose target creature. If it's attacking, it can't be blocked this turn. If it's blocking, prevent all combat damage that would be dealt this combat by it and each creature it's blocking. */

	/*
	  Sewers of Estark
	  Instant
	  Choose target creature.
	  If it's attacking, it can't be blocked this turn.
	  If it's blocking, prevent all combat damage that would be dealt this combat by it and each creature it's blocking.
	*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_state = player == AI ? (current_turn == player ? STATE_ATTACKING : STATE_BLOCKING) : 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if( check_state(instance->targets[0].player, instance->targets[0].card, STATE_ATTACKING) ){
				pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
			}
			if( check_state(instance->targets[0].player, instance->targets[0].card, STATE_BLOCKING) ){
				prevent_all_damage_dealt_by_target(player, card, instance->targets[0].player, instance->targets[0].card, DDBM_MUST_BE_COMBAT_DAMAGE);
				for_each_creature_blocked_by_me(player, card, sewers_of_estark_prevent_damage_effect, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_whimsy(int player, int card, event_t event){
  /* Whimsy	|X|U|U
   * Sorcery
   * Play X random fast effects. */

	return card_whimsy_exe(player, card, event);
}

/* Chandra, Gremlin Wrangler	|2|R|R
 * Legendary Planeswalker - Chandra (3)
 * +1: Create a 2/2 |Sred Gremlin creature token.
 * -2: ~ deals X damage to target creature or player, where X is the number of Gremlins you control. */

/* Dungeon Master	|2|W|U
 * Legendary Planeswalker - Dungeon Master (1d4+1)
 * +1: Target opponent creates a 1/1 |Sblack Skeleton creature token with "When this creature dies, each opponent gains 2 life."
 * +1: Roll a d20. If you roll a 1, skip your next turn. If you roll a 12 or higher, draw a card.
 * -6: You get an adventuring party. (Your party is a 3/3 |Sred Fighter with first strike, a 1/1 |Swhite Cleric with lifelink, a 2/2 |Sblack Rogue with hexproof, and a 1/1 |Sblue Wizard with flying.) */

/* Nira, Hellkite Duelist	|W|U|B|R|G
 * Legendary Creature - Dragon 6/6
 * Flash
 * Flying, trample, haste
 * When ~ enters the battlefield, the next time you would lose the game this turn, instead draw three cards and your life total becomes 5. */

/* Grimlock, Dinobot Leader	|1|R|G|W
 * Legendary Artifact Creature - Autobot 4/4
 * Dinosaurs, Vehicles, and other Transformers(R) creatures you control get +2/+0.
 * |2, Convert a Transformers toy you own to its other mode: ~ becomes ~, Ferocious King.
 * (~, Ferocious King - 8/8) Trample
 * |2, Convert a Transformers toy you own to its other mode: ~, Ferocious King becomes ~. */

/* Grimlock, Ferocious King	""
 * Legendary Artifact Creature - Dinosaur 8/8
 * Trample
 * |2, Convert a Transformers toy you own to its other mode: ~ becomes ~, Dinobot Leader. */

/* Nerf War	|3|U|R
 * Sorcery
 * Fire a Nerf(R) blaster until empty at target library from at least two meters away. For each card knocked off that library, put it into its owner's graveyard and ~ deals 1/2 damage to that player. (Foam darts only.) */

/* Angelic Reward	|3|W|W
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +3/+3 and has flying. */

/* Blinding Radiance	|2|W
 * Sorcery
 * Tap all creatures your opponents control with toughness 2 or less. */

/* Confront the Assault	|4|W
 * Instant
 * Cast this spell only if a creature is attacking you.
 * Create three 1/1 |Swhite Spirit creature tokens with flying. */

/* Cruel Cut	|1|B
 * Instant
 * Destroy target creature with power 2 or less. */

/* Feral Roar	|1|G
 * Sorcery
 * Target creature gets +4/+4 until end of turn. */

/* Goblin Bruiser	|1|R|R
 * Creature - Goblin Warrior 3/3 */

/* Goblin Gang Leader	|2|R|R
 * Creature - Goblin Warrior 2/2
 * When ~ enters the battlefield, create two 1/1 |Sred Goblin creature tokens. */

/* Inspiring Commander	|4|W|W
 * Creature - Human Soldier 1/4
 * Whenever another creature with power 2 or less enters the battlefield under your control, you gain 1 life and draw a card. */

/* Nimble Pilferer	|1|B
 * Creature - Human Rogue 2/1
 * Flash */

/* Ogre Painbringer	|3|R|R
 * Creature - Ogre 7/3
 * When ~ enters the battlefield, it deals 3 damage to each player. */

/* River's Favor	|U
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +1/+1. */

/* Shorecomber Crab	|U
 * Creature - Crab 0/4 */

/* Shrine Keeper	|1|W
 * Creature - Human Cleric 2/2 */

/* Soulhunter Rakshasa	|3|B|B
 * Creature - Cat Demon 5/5
 * ~ can't block.
 * When ~ enters the battlefield, it deals 5 damage to target opponent. */

/* Tactical Advantage	|W
 * Instant
 * Target blocking or blocked creature you control gets +2/+2 until end of turn. */

/* Titanic Pelagosaur	|3|U|U
 * Creature - Dinosaur 4/6 */

/* Treetop Warden	|1|G
 * Creature - Elf Warrior 2/2 */

/* Zephyr Gull	|U
 * Creature - Bird 1/1
 * Flying */

/* Militant Angel	|3|W|W
 * Creature - Angel 3/4
 * Flying, lifelink
 * When ~ enters the battlefield, create a number of 2/2 |Swhite Knight creature tokens with vigilance equal to the number of opponents you attacked this turn. */

/* Inspired Sphinx	|5|U|U
 * Creature - Sphinx 5/5
 * Flying
 * When ~ enters the battlefield, draw cards equal to the number of opponents you have.
 * |3|U: Create a 1/1 colorless Thopter creature token with flying. */

/* Rot Hulk	|5|B|B
 * Creature - Zombie 5/5
 * Menace
 * When ~ enters the battlefield, return up to X target Zombie cards from your graveyard to the battlefield, where X is the number of opponents you have. */

/* Goblin Goliath	|4|R|R
 * Creature - Goblin Mutant 5/4
 * When ~ enters the battlefield, create a number of 1/1 |Sred Goblin creature tokens equal to the number of opponents you have.
 * |3|R, |T: If a source you control would deal damage to an opponent this turn, it deals double that damage to that player instead. */

/* Avatar of Growth	|4|G|G
 * Creature - Elemental Avatar 4/4
 * This spell costs |1 less to cast for each opponent you have.
 * Trample
 * When ~ enters the battlefield, each player searches their library for up to two basic land cards, puts them onto the battlefield, then shuffles their library. */

/* ManalinkEh 2022: 0x2410730; original cards/promo.c:1314. */
int card_fruitcake_elemental(int player, int card, event_t event)
{
  /* Fruitcake Elemental	|1|G|G
   * Creature - Elemental 7/7
   * ~ is indestructible.
   * At the end of your turn, ~ deals 7 damage to you.
   * |3: Target player gains control of ~. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  indestructible(player, card, event);
  if ((current_turn == player) && (result = eot_trigger(player, card, event), result != 0))
  {
    damage_player(player, 7, player, card);
  }
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;

    if (((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0)) &&
        (instance = get_card_instance(player, card), instance->targets[0].player == 1 - player))
    {
      give_control_of_self(instance->parent_controller, instance->parent_card);
    }
  }
  result =
      generic_activated_ability(player, card, event, 4, 3, 0, 0, 0, 0, 0, 0, &td, "TARGET_PLAYER");
  return result;
}

/* ManalinkEh 2022: 0x24108e0; original cards/promo.c:1353. */
int card_seasons_beatings(int player, int card, event_t event)
{
  /* Season's Beatings	|R|R|R|R
   * Sorcery
   * Family gathering - Each creature target player controls deals damage equal to its power to another random creature that player controls. */

  int c;
  int card_00;
  int p;
  card_instance_t *instance;
  int random_target_or_creature;
  int amt;
  int i;
  int damage_source_index;
  target_definition_t td;
  int creatures[151];

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td) != 0)
    {

      instance = get_card_instance(player, card);

      p = instance->targets[0].player;
      if (0 < active_cards_count[p])
      {
        i = 0;
        c = 0;
        do
        {
          instance = in_play(p, c);
          if ((instance != NULL) &&
              (random_target_or_creature = is_what(p, c, 2), random_target_or_creature != 0))
          {
            creatures[i] = c;
            i = i + 1;
          }
          c = c + 1;
        } while (c < active_cards_count[p]);
        if (1 < i)
        {

          damage_source_index = 0;
          do
          {

            random_target_or_creature = network_rand(player, i + -1);
            random_target_or_creature = random_target_or_creature + 1;
            if (random_target_or_creature == damage_source_index)
            {
              random_target_or_creature = 0;
            }
            card_00 = creatures[damage_source_index];
            amt = get_power(p, card_00);
            damage_source_index = damage_source_index + 1;
            damage_creature(p, creatures[random_target_or_creature], amt, p, card_00);
          } while (i != damage_source_index);
        }
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}

/**********************************
 * Heroes of the Realm 2016 cards *
 **********************************/

/***********************************************************************************
 * Hascon 2017 cards (except Swords of Dungeons & Dragons, which is in unstable.c) *
 ***********************************************************************************/

/* Rulings from Maro, for what they're worth, amount to:
 * "Transformers creatures" includes creatures that transform, and creatures that are the backs of double-faced-cards
 * whose front could transform, and melded creatures; but not flip-walkers, the fronts of meld cards, or anything else
 * that reads something like "exile this card, then return it to the battlefield transformed". */

/************************************************
 * MTG:Arena "New Player Experience"-only cards *
 ************************************************/

/****************************
 * Game Night box set, 2018 *
 ****************************/

/* ManalinkEh 2022: 0x240d300; original cards/promo.c:1398. */
static int has_globe_counter(int unused, intptr_t unused2, int player, int card)
{
  int result;
  result = count_counters(player, card, COUNTER_GLOBE);
  return (unsigned int)(result != 0);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2410b40; original cards/promo.c:1401. */
int card_snow_mercy(int player, int card, event_t event)
{
  /* Snow Mercy	|2|W|W
   * Snow Enchantment
   * Whenever a creature deals damage to you, put a globe counter on it.
   * |T, |Q, |T, |Q, |T: Tap all creatures with globe counters on them. */

  unsigned char stored_source_count;
  card_instance_t *damage_or_enchantment;
  int result;
  wav_t soundnum;
  card_instance_t *source_or_enchantment;
  unsigned int tap_step_or_ability_flags;
  test_definition_t test;

  damage_or_enchantment = damage_being_dealt(event);
  if (((((damage_or_enchantment != NULL) && (damage_or_enchantment->damage_target_card == -1)) &&
        (damage_or_enchantment->damage_target_player == player)) &&
       ((result = damage_is_to_planeswalker(damage_or_enchantment),
         result == 0 && ((damage_or_enchantment->targets[3].player & 2) != 0)))) &&
      ((source_or_enchantment = in_play((int)damage_or_enchantment->damage_source_player,
                                        damage_or_enchantment->damage_source_card),
        source_or_enchantment != NULL &&
            (result = is_humiliated(player, card, AL_TRIG), result == 0))))
  {
    source_or_enchantment = get_card_instance(player, card);
    stored_source_count = source_or_enchantment->number_of_targets;
    if (stored_source_count < 10)
    {
      source_or_enchantment->targets[stored_source_count].player =
          (int)damage_or_enchantment->damage_source_player;
      source_or_enchantment->targets[stored_source_count].card =
          damage_or_enchantment->damage_source_card;
      source_or_enchantment->number_of_targets = stored_source_count + 1;
    }
    else
    {
      add_counters(player, card, (int)damage_or_enchantment->damage_source_player,
                   damage_or_enchantment->damage_source_card, COUNTER_GLOBE, 1);
    }
  }
  if (((trigger_condition == TRIGGER_DEAL_DAMAGE) && (card == affected_card)) &&
      ((player == affected_card_controller &&
        ((reason_for_trigger_controller == player &&
          (result = is_humiliated(player, card, AL_TRIG), result == 0))))))
  {
    damage_or_enchantment = get_card_instance(player, card);
    if (event == EVENT_TRIGGER)
    {
      if (damage_or_enchantment->number_of_targets != '\0')
        event_result = event_result | 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {

      if (damage_or_enchantment->number_of_targets != '\0')
      {
        result = 0;
        do
        {
          add_counters(player, card, damage_or_enchantment->targets[result].player,
                       damage_or_enchantment->targets[result].card, COUNTER_GLOBE, 1);
          result = result + 1;
        } while (result < (int)(unsigned int)damage_or_enchantment->number_of_targets);
      }
      damage_or_enchantment->number_of_targets = '\0';
      return global_enchantment(player, card, event);
    }
  }
  if (event == EVENT_ACTIVATE)
  {
    damage_or_enchantment = get_card_instance(player, card);
    damage_or_enchantment->state = damage_or_enchantment->state | 0x10;
    result =
        generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
    damage_or_enchantment = get_card_instance(player, card);
    damage_or_enchantment->state = damage_or_enchantment->state & 0xffffffef;
    if (result == 0)
    {
      return 0;
    }
    if (spell_fizzled == 1)
    {
      return result;
    }
    tap_step_or_ability_flags = 0;
    do
    {
      if ((tap_step_or_ability_flags & 1) == 0)
        tap_card(player, card);
      else
        untap_card(player, card);
      if (ai_is_speculating != 1)
      {
        TENTATIVE_redisplay_all(0, 0xff);
        soundnum = (tap_step_or_ability_flags & 1) == 0 ? WAV_FASTFX : WAV_UNTAP;
        play_sound_effect(soundnum);
        sleepms(0x15e);
      }
      tap_step_or_ability_flags = tap_step_or_ability_flags + 1;
    } while (tap_step_or_ability_flags != 4);
    damage_or_enchantment = get_card_instance(player, card);
    damage_or_enchantment->state = damage_or_enchantment->state | 0x10;
    return result;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
    return global_enchantment(player, card, event);
  tap_step_or_ability_flags = 1;
  result = generic_activated_ability(player, card, event, tap_step_or_ability_flags, 0, 0, 0, 0, 0,
                                     0, 0, NULL, NULL);
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return result;
  }
  default_test_definition(&test, 2);
  test.special_selection_function = has_globe_counter;
  new_manipulate_all(player, card, 2, &test, ACT_TAP);
  return result;
}
#endif

/* ManalinkEh 2022: 0x2410fc0; original cards/promo.c:1482. */
int card_yule_ooze(int player, int card, event_t event)
{
  /* Yule Ooze	|2|R|G
   * Creature - Ooze 1/1
   * At the beginning of your upkeep, destroy another nonland permanent chosen at random, then put a number of +1/+1 counters on ~ equal to that permanent's converted mana cost.
   * |R|G, Eat some food: Regenerate ~. */

  int p;
  int c;
  int scan_player_or_regeneration;
  card_instance_t *instance;
  int cmc;
  int chosen_card_or_index;
  int nonland_count;
  target_t nlperms[151];

  upkeep_trigger_ability(player, card, event, player);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    scan_player_or_regeneration = regeneration(player, card, event, 0, 0, 0, 1, 1, 0);
    if ((event == EVENT_ACTIVATE) && (spell_fizzled != 1))
    {
      play_sound_effect(WAV_DEVOUR);
      return scan_player_or_regeneration;
    }
    return scan_player_or_regeneration;
  }

  nonland_count = 0;
  scan_player_or_regeneration = 0;
  do
  {

    if (0 < active_cards_count[scan_player_or_regeneration])
    {
      c = 0;
      do
      {
        if ((card != c) || (player != scan_player_or_regeneration))
        {
          instance = in_play(scan_player_or_regeneration, c);
          if (instance != NULL)
          {
            if (is_what(scan_player_or_regeneration, c, 1) == 0)
            {
              if (is_what(scan_player_or_regeneration, c, 0x47) != 0)
              {
                nlperms[nonland_count].card = c;
                nlperms[nonland_count].player = scan_player_or_regeneration;
                c = c + 1;
                nonland_count = nonland_count + 1;
                continue;
              }
            }
          }
        }
        c = c + 1;
      } while (c < active_cards_count[scan_player_or_regeneration]);
    }
    scan_player_or_regeneration = scan_player_or_regeneration + 1;
    if (scan_player_or_regeneration == 2)
    {
      if (0 < nonland_count)
      {

        chosen_card_or_index = network_rand(player, nonland_count);
        p = nlperms[chosen_card_or_index].player;
        chosen_card_or_index = nlperms[chosen_card_or_index].card;
        cmc = get_cmc(p, chosen_card_or_index);
        kill_card(p, chosen_card_or_index, KILL_DESTROY);
        add_counters(player, card, player, card, COUNTER_P1_P1, cmc);
      }
      return regeneration(player, card, EVENT_UPKEEP_TRIGGER_ABILITY, 0, 0, 0, 1, 1, 0);
    }
  } while (true);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2411220; original cards/promo.c:1526. */
int card_stocking_tiger(int player, int card, event_t event)
{
  /* Stocking Tiger	|5
   * Artifact Creature - Cat Construct 3/3
   * ~ enters with a sealed Magic booster pack under it.
   * When ~ deals combat damage to a player, unwrap the booster pack and put it into your hand. */

  if (event == EVENT_RESOLVE_SPELL && !is_humiliated(player, card, AL_STATIC))
  {
    EXPANSION_t expansion = (EXPANSION_t)-1;
    if (ai_is_speculating == 1 || (player == 1 && (trace_mode & 2) == 0))
    {
      expansion = EXPANSION_THE_DARK;
      announce_expansion_choice(player, card, expansion);
    }
    else if ((trace_mode & 2) != 0 && player != 0)
    {
      TENTATIVE_wait_for_network_result(1, 26);
      expansion = dblword_628C10;
      announce_expansion_choice(player, card, expansion);
    }
    else
    {
      choose_packs(1, PACK_ALLOW_BOOSTER, &expansion, NULL);
      if ((trace_mode & 2) != 0)
      {
        dblword_628C10 = expansion;
        byte_628C0C = 26;
        TENTATIVE_send_network_result(0, 26);
      }
    }
    if (is_valid_pack(expansion, 1))
      get_card_instance(player, card)->targets[3].card =
          ((uint32_t)expansion & 0xffff) | 0x3f7c0000;
  }
  if (has_combat_damage_been_inflicted_to_a_player(player, card, event))
  {
    card_instance_t *instance = get_card_instance(player, card);
    uint32_t stored = instance->targets[3].card;
    if ((uint16_t)(stored >> 16) == 0x3f7c)
    {
      EXPANSION_t expansion = stored & 0xffff;
      if (is_valid_pack(expansion, 1))
      {
        instance->targets[3].card = -1;
        int capacity = pack_size(expansion, 1);
        int csvids[capacity];
        int num_csvids = make_pack(expansion, 1, csvids);
        for (int i = 0; i < num_csvids; ++i)
          add_card_to_hand(player, get_internal_card_id_from_csv_id(csvids[i]));
      }
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x2411480; original cards/promo.c:1596. */
int card_bog_humbugs(int player, int card, event_t event)
{
  /* Bog Humbugs	|1|B
   * Creature - Insect 1/1
   * Flying
   * Whenever ~ deals combat damage, hum that many notes of a festive song. If you can, put a +1/+1 counter on ~. */

  if (!damage_dealt_by_me(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE))
  {
    return 0;
  }
  play_sound_effect(WAV_POLKAMIX);
  hack_silent_counters = hack_silent_counters + 1;
  add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  hack_silent_counters = hack_silent_counters + -1;
  return 0;
}
